#include "Session/SpellVfx.h"

#include "Greybox/GreyboxUtil.h"
#include "MageArenaVR.h"
#include "Materials/MaterialInterface.h"
#include "Misc/PackageName.h"

namespace
{
// A cheap, stable 0..1 hash: the same bolt jitters its trail the same way every frame.
double Hash01(int32 Seed, int32 Index)
{
	uint32 X = static_cast<uint32>(Seed) * 0x9E3779B1u + static_cast<uint32>(Index) * 0x85EBCA77u;
	X ^= X >> 15;
	X *= 0x2C1B3C6Du;
	X ^= X >> 12;
	return static_cast<double>(X & 0xFFFFu) / 65535.0;
}

// Two unit vectors across Dir.
void Across(const FVector& Dir, FVector& Side, FVector& Lift)
{
	const FVector Up = FMath::Abs(Dir.Z) > 0.95 ? FVector::ForwardVector : FVector::UpVector;
	Side = FVector::CrossProduct(Up, Dir).GetSafeNormal();
	Lift = FVector::CrossProduct(Dir, Side).GetSafeNormal();
}

void Add(TArray<SpellVfx::FPuff>& Out, const FVector& At, const FVector& Size, const FQuat& Rotation = FQuat::Identity)
{
	SpellVfx::FPuff& Puff = Out.AddDefaulted_GetRef();
	Puff.CentreCm = At;
	Puff.SizeCm = Size.ComponentMax(FVector(0.5));
	Puff.Rotation = Rotation;
}

double EaseOut(double T)
{
	const double C = FMath::Clamp(T, 0.0, 1.0);
	return 1.0 - (1.0 - C) * (1.0 - C);
}
}

TOptional<FLinearColor> SpellVfx::GlowColour(const FThreatLook& Look, const FString& Family)
{
	if (Look.bRim)
	{
		return Look.Rim;
	}
	// Physical draws steel (ThreatLook): a dodge cue never glows. Magic, and any cast the player owns, draws its element.
	if (Family == TEXT("physical"))
	{
		return {};
	}
	return Look.Body;
}

SpellVfx::ETrail SpellVfx::TrailFor(EElement Element)
{
	switch (Element)
	{
	case EElement::Fire:
		return ETrail::Embers;
	case EElement::Air:
		return ETrail::Wisps;
	case EElement::Earth:
		return ETrail::Grit;
	case EElement::Water:
	default:
		return ETrail::Droplets;
	}
}

void SpellVfx::BoltPuffs(ETrail Trail, const FVector& AtCm, const FVector& FlightDir, double CoreDiameterCm, int32 Seed, double TimeS, TArray<FPuff>& Out)
{
	const double D = FMath::Max(CoreDiameterCm, 8.0);
	// The shell: the soft falloff around the opaque core.
	Add(Out, AtCm, FVector(D * ShellScale));
	const FVector Dir = FlightDir.IsNearlyZero() ? FVector::ForwardVector : FlightDir.GetSafeNormal();
	FVector Side;
	FVector Lift;
	Across(Dir, Side, Lift);
	const FQuat Along = FRotationMatrix::MakeFromX(Dir).ToQuat();
	switch (Trail)
	{
	case ETrail::Droplets:
		// Six drops falling behind, sagging and shrinking.
		for (int32 I = 1; I <= 6; ++I)
		{
			const double Back = D * (0.6 + 0.8 * I);
			const double Jitter = (Hash01(Seed, I) - 0.5) * D * 0.35;
			const FVector At = AtCm - Dir * Back + Side * Jitter - FVector::UpVector * (0.9 * I * I);
			const double Size = D * (0.70 - 0.09 * I);
			Add(Out, At, FVector(Size * 1.35, Size, Size), Along);
		}
		break;
	case ETrail::Embers:
		// Eight small embers scattered behind, rising and flickering.
		for (int32 I = 1; I <= 8; ++I)
		{
			const double Back = D * (0.5 + 0.6 * I);
			const double Spread = D * (0.15 + 0.06 * I);
			const FVector At = AtCm - Dir * Back + Side * ((Hash01(Seed, I) - 0.5) * 2.0 * Spread) + Lift * ((Hash01(Seed, I + 31) - 0.5) * 2.0 * Spread) + FVector::UpVector * (1.6 * I);
			const double Flicker = 0.7 + 0.3 * FMath::Sin(TimeS * 23.0 + I * 1.7 + Seed);
			Add(Out, At, FVector(D * (0.36 - 0.03 * I) * Flicker), Along);
		}
		break;
	case ETrail::Wisps:
		// Four long streaks winding behind.
		for (int32 I = 1; I <= 4; ++I)
		{
			const double Back = D * (0.6 + 1.0 * I);
			const double Turn = TimeS * 9.0 + I * 1.4;
			const FVector At = AtCm - Dir * Back + (Side * FMath::Cos(Turn) + Lift * FMath::Sin(Turn)) * (D * 0.22);
			const double Cross = D * (0.42 - 0.07 * I);
			Add(Out, At, FVector(D * 1.7, Cross, Cross), Along);
		}
		break;
	case ETrail::Grit:
		for (int32 I = 1; I <= 5; ++I)
		{
			const double Back = D * (0.4 + 0.5 * I);
			const FVector At = AtCm - Dir * Back + Side * ((Hash01(Seed, I) - 0.5) * D * 0.4) - FVector::UpVector * (1.4 * I * I);
			Add(Out, At, FVector(D * (0.4 - 0.05 * I)), Along);
		}
		break;
	case ETrail::None:
	default:
		break;
	}
}

void SpellVfx::SplashPuffs(ETrail Trail, const FVector& AtCm, const FVector& TowardEye, double Age01, int32 Seed, TArray<FPuff>& Out)
{
	const double Age = FMath::Clamp(Age01, 0.0, 1.0);
	if (Age >= 1.0)
	{
		return;
	}
	const FVector Facing = TowardEye.IsNearlyZero() ? FVector::BackwardVector : TowardEye.GetSafeNormal();
	FVector Side;
	FVector Lift;
	Across(Facing, Side, Lift);
	// The burst: a flat disc facing the eye that widens and thins. Its centre is dim and its edge bright, a splash crown.
	const double Radius = FMath::Lerp(5.0, 18.0, EaseOut(Age));
	const FQuat Flat = FRotationMatrix::MakeFromZ(Facing).ToQuat();
	Add(Out, AtCm, FVector(Radius * 2.0, Radius * 2.0, FMath::Lerp(8.0, 1.5, Age)), Flat);
	// Ten droplets (or embers) thrown out across the ward.
	for (int32 I = 0; I < 10; ++I)
	{
		const double Angle = 2.0 * PI * (I + Hash01(Seed, I) * 0.6) / 10.0;
		const FVector Radial = Side * FMath::Cos(Angle) + Lift * FMath::Sin(Angle);
		const double Reach = FMath::Lerp(5.0, 26.0 + 8.0 * Hash01(Seed, I + 11), EaseOut(Age));
		double Fall = 0.0;
		if (Trail == ETrail::Droplets || Trail == ETrail::Grit)
		{
			Fall = -30.0 * Age * Age;
		}
		else if (Trail == ETrail::Embers)
		{
			Fall = 18.0 * Age;
		}
		const FVector At = AtCm + Radial * Reach + Facing * (6.0 * Age) + FVector::UpVector * Fall;
		const double Size = FMath::Lerp(5.0, 1.0, Age);
		Add(Out, At, FVector(Size * 1.6, Size, Size), FRotationMatrix::MakeFromX(Radial).ToQuat());
	}
}

void SpellVfx::PerfectRingPuffs(const FVector& AtCm, const FVector& TowardEye, double Age01, TArray<FPuff>& Out)
{
	const double Age = FMath::Clamp(Age01, 0.0, 1.0);
	if (Age >= 1.0)
	{
		return;
	}
	const FVector Facing = TowardEye.IsNearlyZero() ? FVector::BackwardVector : TowardEye.GetSafeNormal();
	FVector Side;
	FVector Lift;
	Across(Facing, Side, Lift);
	const double Radius = FMath::Lerp(8.0, 26.0, EaseOut(Age));
	const double Bead = FMath::Lerp(6.0, 1.8, Age);
	constexpr int32 Beads = 20;
	for (int32 I = 0; I < Beads; ++I)
	{
		const double Angle = 2.0 * PI * I / Beads;
		const FVector Radial = Side * FMath::Cos(Angle) + Lift * FMath::Sin(Angle);
		const FVector Tangent = FVector::CrossProduct(Facing, Radial).GetSafeNormal();
		Add(Out, AtCm + Radial * Radius, FVector(Bead * 2.4, Bead, Bead), FRotationMatrix::MakeFromX(Tangent).ToQuat());
	}
	if (Age < 0.45)
	{
		Add(Out, AtCm, FVector(FMath::Lerp(26.0, 4.0, Age / 0.45)));
	}
}

float SpellVfx::RimPulse(double TimeS)
{
	const double Wave = 0.5 + 0.5 * FMath::Sin(TimeS * 2.0 * PI * RimPulseHz);
	return Greybox::QuantiseFade(static_cast<float>(Wave), RimPulseSteps);
}

FLinearColor SpellVfx::RimPulseColour(const FLinearColor& Rim, float Pulse)
{
	const float Gain = FMath::Lerp(1.f, RimPulseGain, FMath::Clamp(Pulse, 0.f, 1.f));
	return FLinearColor(Rim.R * Gain, Rim.G * Gain, Rim.B * Gain, Rim.A);
}

UMaterialInterface* SpellVfx::GlowMaterial()
{
	static bool bWarned = false;
	const FString ObjectPath = FString::Printf(TEXT("%s.%s"), GlowMaterialPackage, *FPackageName::GetShortName(GlowMaterialPackage));
	if (UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, *ObjectPath, nullptr, LOAD_NoWarn | LOAD_Quiet))
	{
		return Material;
	}
	if (!bWarned)
	{
		bWarned = true;
		UE_LOG(LogMageArena, Warning, TEXT("spell vfx: %s missing (run create-budget-fx.ps1 -SpellVfx); flat translucent glow"), *ObjectPath);
	}
	return Greybox::TranslucentMaterial();
}
