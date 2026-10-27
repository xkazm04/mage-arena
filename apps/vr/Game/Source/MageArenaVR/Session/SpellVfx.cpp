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

SpellVfx::FPuff& Add(TArray<SpellVfx::FPuff>& Out, const FVector& At, const FVector& Size, const FQuat& Rotation = FQuat::Identity)
{
	SpellVfx::FPuff& Puff = Out.AddDefaulted_GetRef();
	Puff.CentreCm = At;
	Puff.SizeCm = Size.ComponentMax(FVector(0.5));
	Puff.Rotation = Rotation;
	return Puff;
}

// T63: the card a puff shows. An upright card turns by Roll; a streak lies along the puff's rotation.
void Card(SpellVfx::FPuff& Puff, SpellVfx::ECell Cell, bool bStreak, float Roll = 0.f, bool bMirror = false)
{
	Puff.Cell = Cell;
	Puff.bStreak = bStreak;
	Puff.Roll = Roll;
	Puff.bMirror = bMirror;
}

double EaseOut(double T)
{
	const double C = FMath::Clamp(T, 0.0, 1.0);
	return 1.0 - (1.0 - C) * (1.0 - C);
}

// T63: a shell's flicker frame at TimeS, 0 to 3. Bit 0 picks the school's second shell cell, bit 1 mirrors it.
int32 ShellFrame(int32 Seed, double TimeS)
{
	const int64 Tick = static_cast<int64>(FMath::FloorToDouble(TimeS * SpellVfx::FlipbookFps)) + Seed;
	return static_cast<int32>(((Tick % 4) + 4) % 4);
}

SpellVfx::ECell ShellCell(SpellVfx::ETrail Trail, int32 Frame)
{
	const bool bSecond = (Frame & 1) != 0;
	switch (Trail)
	{
	case SpellVfx::ETrail::Droplets:
		return bSecond ? SpellVfx::ECell::WaterShellB : SpellVfx::ECell::WaterShellA;
	case SpellVfx::ETrail::Embers:
		return bSecond ? SpellVfx::ECell::FireFlameB : SpellVfx::ECell::FireFlameA;
	case SpellVfx::ETrail::Wisps:
		return bSecond ? SpellVfx::ECell::AirSwirlB : SpellVfx::ECell::AirSwirlA;
	case SpellVfx::ETrail::Grit:
	case SpellVfx::ETrail::None:
	default:
		return SpellVfx::ECell::SoftGlow;
	}
}

SpellVfx::ECell CrownCell(SpellVfx::ETrail Trail)
{
	switch (Trail)
	{
	case SpellVfx::ETrail::Embers:
		return SpellVfx::ECell::FireCrown;
	case SpellVfx::ETrail::Wisps:
		return SpellVfx::ECell::AirCrown;
	case SpellVfx::ETrail::Grit:
		return SpellVfx::ECell::Grit;
	case SpellVfx::ETrail::Droplets:
	default:
		return SpellVfx::ECell::WaterCrown;
	}
}

// T67: Fire's flame star is a larger card than the other crowns, so its layered licks fill more of the ward.
double CrownScaleFor(SpellVfx::ETrail Trail)
{
	return Trail == SpellVfx::ETrail::Embers ? SpellVfx::FireCrownScale : 1.0;
}

// T67: Fire's flung sparks and ring beads are larger cards, so each reads as a small flame.
double StreakScaleFor(SpellVfx::ETrail Trail)
{
	return Trail == SpellVfx::ETrail::Embers ? SpellVfx::FireStreakScale : 1.0;
}

// The streak a school's splash and perfect ring throw.
SpellVfx::ECell StreakCell(SpellVfx::ETrail Trail)
{
	switch (Trail)
	{
	case SpellVfx::ETrail::Embers:
		return SpellVfx::ECell::SparkStreak;
	case SpellVfx::ETrail::Wisps:
		return SpellVfx::ECell::AirWisp;
	case SpellVfx::ETrail::Droplets:
	case SpellVfx::ETrail::Grit:
	default:
		return SpellVfx::ECell::DropletStreak;
	}
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
	// The shell: the soft falloff around the opaque core. T63: the school's shell cell, flickering between two cells and
	// their mirrors. Water's spray and Air's wisps turn about the core; Fire's flame stays upright and sways.
	const int32 Frame = ShellFrame(Seed, TimeS);
	double Roll = 0.0;
	if (Trail == ETrail::Droplets)
	{
		Roll = FMath::Fmod(TimeS * 1.3 + Seed * 0.71, 2.0 * PI);
	}
	else if (Trail == ETrail::Wisps)
	{
		Roll = FMath::Fmod(-TimeS * 3.2 + Seed * 0.71, 2.0 * PI);
	}
	else if (Trail == ETrail::Embers)
	{
		Roll = 0.07 * FMath::Sin(TimeS * 5.0 + Seed);
	}
	// T67: a flame is taller than it is wide, so Fire's shell card is a taller card (the flames lick up) and a little wider
	// than the other schools', for volume at the same one puff.
	const FVector ShellSize = Trail == ETrail::Embers ? FVector(D * ShellScale * FireShellWidth, D * ShellScale * FireShellHeight, D * ShellScale) : FVector(D * ShellScale);
	Card(Add(Out, AtCm, ShellSize), ShellCell(Trail, Frame), false, static_cast<float>(Roll), (Frame & 2) != 0);
	const FVector Dir = FlightDir.IsNearlyZero() ? FVector::ForwardVector : FlightDir.GetSafeNormal();
	FVector Side;
	FVector Lift;
	Across(Dir, Side, Lift);
	const FQuat Along = FRotationMatrix::MakeFromX(Dir).ToQuat();
	switch (Trail)
	{
	case ETrail::Droplets:
		// Six drops falling behind, sagging and shrinking. T63: each a run of fine drops (the spray cell), so the trail
		// is a dense spray without more puffs.
		for (int32 I = 1; I <= 6; ++I)
		{
			const double Back = D * (0.6 + 0.8 * I);
			const double Jitter = (Hash01(Seed, I) - 0.5) * D * 0.35;
			const FVector At = AtCm - Dir * Back + Side * Jitter - FVector::UpVector * (0.9 * I * I);
			const double Size = D * (0.70 - 0.09 * I) * ClusterCardScale;
			Card(Add(Out, At, FVector(Size * 1.35, Size, Size), Along), ECell::WaterSpray, true);
		}
		break;
	case ETrail::Embers:
		// Eight small embers scattered behind, rising and flickering. T63: each a cluster of sparks with dark rims.
		for (int32 I = 1; I <= 8; ++I)
		{
			const double Back = D * (0.5 + 0.6 * I);
			const double Spread = D * (0.15 + 0.06 * I);
			const FVector At = AtCm - Dir * Back + Side * ((Hash01(Seed, I) - 0.5) * 2.0 * Spread) + Lift * ((Hash01(Seed, I + 31) - 0.5) * 2.0 * Spread) + FVector::UpVector * (1.6 * I);
			const double Flicker = 0.7 + 0.3 * FMath::Sin(TimeS * 23.0 + I * 1.7 + Seed);
			const double EmberRoll = 2.0 * PI * Hash01(Seed, I + 57) + TimeS * 2.0;
			Card(Add(Out, At, FVector(D * (0.36 - 0.03 * I) * Flicker * ClusterCardScale * FireEmberScale), Along), ECell::FireEmbers, false, static_cast<float>(EmberRoll),
			    Hash01(Seed, I + 73) > 0.5);
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
			Card(Add(Out, At, FVector(D * 1.7, Cross, Cross), Along), ECell::AirWisp, true);
		}
		break;
	case ETrail::Grit:
		for (int32 I = 1; I <= 5; ++I)
		{
			const double Back = D * (0.4 + 0.5 * I);
			const FVector At = AtCm - Dir * Back + Side * ((Hash01(Seed, I) - 0.5) * D * 0.4) - FVector::UpVector * (1.4 * I * I);
			Card(Add(Out, At, FVector(D * (0.4 - 0.05 * I) * ClusterCardScale), Along), ECell::Grit, false, static_cast<float>(2.0 * PI * Hash01(Seed, I + 57)));
		}
		break;
	case ETrail::None:
	default:
		break;
	}
}

void SpellVfx::WallFlamePuffs(const FVector& BaseCm, int32 Index, double TimeS, TArray<FPuff>& Out)
{
	// Neighbouring flames are out of step, so the wall sways rather than blinks.
	const int32 Frame = ShellFrame(Index * 3, TimeS);
	const double Sway = 0.06 * FMath::Sin(TimeS * 4.0 + Index * 1.3);
	const FVector Centre = BaseCm + FVector::UpVector * (WallFlameHeightCm * 0.5);
	Card(Add(Out, Centre, FVector(WallFlameWidthCm, WallFlameHeightCm, WallFlameWidthCm)), ShellCell(ETrail::Embers, Frame), false, static_cast<float>(Sway), (Frame & 2) != 0);
}

void SpellVfx::SplashPuffs(ETrail Trail, const FVector& AtCm, const FVector& TowardEye, double Age01, int32 Seed, bool bCrown, TArray<FPuff>& Out)
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
	if (bCrown)
	{
		const FQuat Flat = FRotationMatrix::MakeFromZ(Facing).ToQuat();
		// T63: the school's crown cell (crests and drops, flame licks and sparks, curling wisps), each burst turned its own way.
		Card(Add(Out, AtCm, FVector(Radius * 2.0 * CrownCardScale * CrownScaleFor(Trail), Radius * 2.0 * CrownCardScale * CrownScaleFor(Trail), FMath::Lerp(8.0, 1.5, Age)), Flat), CrownCell(Trail), false,
		    static_cast<float>(2.0 * PI * Hash01(Seed, 97)));
	}
	// Ten droplets (or embers) thrown out across the ward, unevenly, each its own size.
	for (int32 I = 0; I < 10; ++I)
	{
		const double Angle = 2.0 * PI * (I + (Hash01(Seed, I) - 0.5) * 1.2) / 10.0;
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
		const double Size = FMath::Lerp(5.0, 1.0, Age) * (0.6 + 0.8 * Hash01(Seed, I + 23));
		// T63: a flung drop, spark or wisp with its tail, head outward.
		Card(Add(Out, At, FVector(Size * 2.2, Size * 1.1, Size) * StreakScaleFor(Trail), FRotationMatrix::MakeFromX(Radial).ToQuat()), StreakCell(Trail), true);
	}
}

void SpellVfx::PerfectRingPuffs(const FVector& AtCm, const FVector& TowardEye, double Age01, TArray<FPuff>& Out, ETrail Trail)
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
		// T63: each bead a streak of the school, head along the ring, so the ring reads as spinning out.
		Card(Add(Out, AtCm + Radial * Radius, FVector(Bead * 2.4 * RingCardScale, Bead * RingCardScale, Bead) * StreakScaleFor(Trail), FRotationMatrix::MakeFromX(Tangent).ToQuat()), StreakCell(Trail), true);
	}
}

void SpellVfx::CardCustomData(const FPuff& Puff, float (&Out)[CardCustomFloats])
{
	const int32 Index = static_cast<int32>(Puff.Cell);
	const float Step = 1.f / FlipbookGrid;
	const float Column = static_cast<float>(Index % FlipbookGrid);
	const float Row = static_cast<float>(Index / FlipbookGrid);
	// U = u x Out[0] + Out[1] and V = v / FlipbookGrid + Out[2]: a mirrored card reads its cell from right to left.
	Out[0] = Puff.bMirror ? -Step : Step;
	Out[1] = (Column + (Puff.bMirror ? 1.f : 0.f)) * Step;
	Out[2] = Row * Step;
}

SpellVfx::ECell SpellVfx::CellOfCustomData(const float (&Data)[CardCustomFloats], bool& bOutMirror)
{
	const float Step = 1.f / FlipbookGrid;
	bOutMirror = Data[0] < 0.f;
	const int32 Column = FMath::RoundToInt32(Data[1] / Step) - (bOutMirror ? 1 : 0);
	const int32 Row = FMath::RoundToInt32(Data[2] / Step);
	return static_cast<ECell>(FMath::Clamp(Row * FlipbookGrid + Column, 0, FlipbookGrid * FlipbookGrid - 1));
}

FTransform SpellVfx::CardTransform(const FPuff& Puff, const FVector& EyeCm)
{
	FVector Normal = (EyeCm - Puff.CentreCm).GetSafeNormal();
	if (Normal.IsNearlyZero())
	{
		Normal = FVector::BackwardVector;
	}
	// Upright: U to the viewer's right, so V (Normal x U) runs down the screen.
	FVector Right = FVector::CrossProduct(Normal, FVector::UpVector);
	if (Right.SizeSquared() < 1.0e-6)
	{
		Right = FVector::CrossProduct(Normal, FVector::ForwardVector);
	}
	Right.Normalize();
	double Width = Puff.SizeCm.X;
	const double Height = Puff.SizeCm.Y;
	FVector U = Right;
	if (Puff.bStreak)
	{
		const FVector Along = Puff.Rotation.GetForwardVector();
		const FVector Seen = Along - Normal * FVector::DotProduct(Along, Normal);
		const double Length = Seen.Size();
		if (Length > 1.0e-3)
		{
			U = Seen / Length;
		}
		// Head-on a streak is as long as it is wide, as the sphere it replaced was.
		Width = FMath::Lerp(Height, Width, FMath::Min(Length, 1.0));
	}
	else if (Puff.Roll != 0.f)
	{
		U = FQuat(Normal, Puff.Roll).RotateVector(Right);
	}
	const FVector V = FVector::CrossProduct(Normal, U);
	const FQuat Rotation = FRotationMatrix::MakeFromXY(U, V).ToQuat();
	return FTransform(Rotation, Puff.CentreCm, FVector(Width * CardOversize / 100.0, Height * CardOversize / 100.0, 1.0));
}

void SpellVfx::FGlowFrame::Reset(const FVector& InEyeCm)
{
	EyeCm = InEyeCm;
	Drawn = 0;
	for (TPair<FLinearColor, FLayer>& Layer : Layers)
	{
		Layer.Value.Transforms.Reset();
		Layer.Value.CustomData.Reset();
	}
}

void SpellVfx::FGlowFrame::Add(const FLinearColor& Colour, TConstArrayView<FPuff> Puffs)
{
	FLayer& Layer = Layers.FindOrAdd(Colour);
	for (const FPuff& Puff : Puffs)
	{
		if (Drawn >= MaxPuffs)
		{
			return;
		}
		Layer.Transforms.Add(CardTransform(Puff, EyeCm));
		float Data[CardCustomFloats];
		CardCustomData(Puff, Data);
		Layer.CustomData.Append(Data, CardCustomFloats);
		++Drawn;
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

FLinearColor SpellVfx::GlowEmissive(const FLinearColor& Colour, const FLinearColor& Rim, float RimMix, float Shade)
{
	return FLinearColor(Colour.R * Shade, Colour.G * Shade, Colour.B * Shade, Colour.A);
}

FLinearColor SpellVfx::FireRimColour(const FElementPalette& Palette)
{
	return Palette.Fire;
}

TOptional<FLinearColor> SpellVfx::GlowRimFor(const FElementPalette& Palette, const FLinearColor& Colour)
{
	return {};
}

bool SpellVfx::FireRimEnabled()
{
	return false;
}

void SpellVfx::ApplyGlowRim(UMaterialInstanceDynamic* Tint, const TOptional<FLinearColor>& Rim)
{
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
