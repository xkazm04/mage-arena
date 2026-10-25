#include "Greybox/BrazierFire.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Greybox/ArenaLayout.h"
#include "Greybox/GreyboxUtil.h"
#include "MageArenaVR.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Session/ElementColour.h"

namespace
{
// The three waves of the flicker, slow and incommensurate, so the lick never repeats in a beat the eye can catch.
struct FWaves
{
	double A = 0.0;
	double B = 0.0;
	double C = 0.0;
};

FWaves Waves(int32 Seed, double TimeS)
{
	const double Phase = 1.37 * Seed;
	FWaves Out;
	Out.A = FMath::Sin(2.0 * PI * 0.55 * TimeS + Phase);
	Out.B = FMath::Sin(2.0 * PI * 0.93 * TimeS + 1.7 * Phase + 0.6);
	Out.C = FMath::Sin(2.0 * PI * BrazierFire::FastestFlickerHz * TimeS + 2.3 * Phase + 1.9);
	return Out;
}

// A puff centred at Base + Up x Height + Sway, its tall axis leaning along the sway (a lick bends, it does not slide).
void Lick(TArray<SpellVfx::FPuff>& Out, const FVector& BaseCm, double HeightCm, const FVector& SwayCm, const FVector& SizeCm)
{
	SpellVfx::FPuff& Puff = Out.AddDefaulted_GetRef();
	Puff.CentreCm = BaseCm + FVector(SwayCm.X, SwayCm.Y, HeightCm);
	Puff.SizeCm = SizeCm.ComponentMax(FVector(0.5));
	const FVector Axis = FVector(SwayCm.X, SwayCm.Y, FMath::Max(HeightCm, 1.0)).GetSafeNormal();
	Puff.Rotation = FRotationMatrix::MakeFromZ(Axis).ToQuat();
}

FLinearColor AtLuminance(const FLinearColor& Chroma, float Luminance)
{
	const float Own = Chroma.GetLuminance();
	// The chroma's brightest channel is 1, so its luminance times MinBrightest is the floor.
	Luminance = FMath::Max(Luminance, BrazierFire::MinBrightest * Own);
	const float Scale = Own > 1.0e-4f ? Luminance / Own : 0.f;
	return FLinearColor(Chroma.R * Scale, Chroma.G * Scale, Chroma.B * Scale, 1.f);
}

void Pose(UInstancedStaticMeshComponent* Layer, const TArray<SpellVfx::FPuff>& Puffs, bool bAdd)
{
	if (!Layer)
	{
		return;
	}
	TArray<FTransform> Transforms;
	Transforms.Reserve(Puffs.Num());
	for (const SpellVfx::FPuff& Puff : Puffs)
	{
		// The engine sphere is 100 cm across.
		Transforms.Emplace(Puff.Rotation, Puff.CentreCm, Puff.SizeCm / 100.0);
	}
	if (bAdd)
	{
		Layer->AddInstances(Transforms, false, false);
	}
	else if (Layer->GetInstanceCount() == Transforms.Num())
	{
		Layer->BatchUpdateInstancesTransforms(0, Transforms, false, true, false);
	}
}

void AllPuffs(const FArenaLayout& Layout, double TimeS, TArray<SpellVfx::FPuff>& Body, TArray<SpellVfx::FPuff>& Heart)
{
	for (int32 Index = 0; Index < Layout.Braziers.Num(); ++Index)
	{
		BrazierFire::FlamePuffs(Layout.Braziers[Index], Index + 1, TimeS, Body, Heart);
	}
}
}

float BrazierFire::DimmestGlowLuminance(const FElementPalette& Palette)
{
	float Least = Palette.UnblockableRim.GetLuminance();
	for (const EElement Element : {EElement::Water, EElement::Fire, EElement::Air, EElement::Earth})
	{
		Least = FMath::Min(Least, Palette.ElementColour(Element).GetLuminance());
	}
	return Least;
}

FLinearColor BrazierFire::FlameColour(const FElementPalette& Palette, ELayer Layer)
{
	// An environment colour, never a threat's: never projectiles.fire (the fire bolt's absorb colour) and never
	// arena.flame (style 25 samples it as the leave red). Only its brightness follows the palette, so it stays dimmer
	// than whatever a threat glows in.
	const float Heart = HeartShare * DimmestGlowLuminance(Palette);
	return Layer == ELayer::Heart ? AtLuminance(HeartChroma, Heart) : AtLuminance(BodyChroma, BodyShare * Heart);
}

void BrazierFire::BowlPieces(const FArenaProp& Brazier, TArray<FPiece>& Out)
{
	const double X = Brazier.PositionM.X;
	const double Y = Brazier.PositionM.Y;
	const double R = Brazier.BowlRadiusM;
	const double H = Brazier.BowlHeightM;
	const double Stand = Brazier.StandHeightM;
	auto Ring = [&Out, X, Y](double Radius, double Bottom, double Top)
	{
		FPiece& Piece = Out.AddDefaulted_GetRef();
		Piece.CentreM = FVector(X, Y, (Bottom + Top) * 0.5);
		Piece.SizeM = FVector(Radius * 2.0, Radius * 2.0, Top - Bottom);
	};
	const double Foot = FMath::Min(0.05, Stand * 0.2);
	// A broad foot, a slim stem, then the bowl: five equal rings on a rounded profile (an ellipse quarter, sampled at
	// each ring's middle), narrow at the base and flaring to the lip, so the silhouette is a bowl, not a drum.
	Ring(R * 0.55, 0.0, Foot);
	Ring(R * 0.22, Foot, Stand);
	constexpr double Profile[] = {0.44, 0.71, 0.87, 0.96, 1.0};
	constexpr int32 Rings = UE_ARRAY_COUNT(Profile);
	for (int32 Index = 0; Index < Rings; ++Index)
	{
		Ring(R * Profile[Index], Stand + H * Index / Rings, Stand + H * (Index + 1) / Rings);
	}
}

void BrazierFire::FlamePuffs(const FArenaProp& Brazier, int32 Seed, double TimeS, TArray<SpellVfx::FPuff>& OutBody, TArray<SpellVfx::FPuff>& OutHeart)
{
	const double R = Brazier.FlameRadiusM * 100.0;
	const FVector Base(Brazier.PositionM.X * 100.0, Brazier.PositionM.Y * 100.0, (Brazier.StandHeightM + Brazier.BowlHeightM) * 100.0);
	const FWaves W = Waves(Seed, TimeS);
	// One tapered flame: a bed in the bowl's mouth, a tall tongue and a side tongue, a low heart and a heart tip. Each
	// breathes and leans on its own wave; nothing rises, so the flame licks instead of throwing sparks. The whole flame
	// stays inside the old cone's silhouette (radius R at the lip, a point MaxFlameTopRadii above it): a wider, forked
	// flame crossed the unblockable's flight line from the seat and broke its red ring (BRAZIERS-T60, critic rounds 1-2).
	const FVector Low(W.A, W.C, 0.0);
	const FVector High(W.C, W.A, 0.0);
	const FVector Side(0.0, R * 0.15, 0.0);
	Lick(OutBody, Base, 0.0, Low * (R * 0.05), FVector(R * 1.8, R * 1.8, R * 1.0 * (1.0 + 0.06 * W.B)));
	Lick(OutBody, Base, R * (0.95 + 0.10 * W.A), High * (R * 0.10), FVector(R * 0.8, R * 0.8, R * 2.2 * (1.0 + 0.12 * W.B)));
	Lick(OutBody, Base, R * (0.80 + 0.10 * W.C), Side + Low * (R * 0.08), FVector(R * 0.7, R * 0.7, R * 1.6 * (1.0 + 0.12 * W.A)));
	Lick(OutHeart, Base, R * 0.50, Low * (R * 0.08), FVector(R * 1.2, R * 1.2, R * 1.4 * (1.0 + 0.10 * W.A)));
	Lick(OutHeart, Base, R * (1.30 + 0.10 * W.C), High * (R * 0.12), FVector(R * 0.5, R * 0.5, R * 1.2 * (1.0 + 0.12 * W.B)));
}

bool BrazierFire::IsSwitchedOff()
{
	return FParse::Param(FCommandLine::Get(), TEXT("MageArenaNoBrazierFire"));
}

BrazierFire::FFlameLayers BrazierFire::Build(AActor* Owner, const FArenaLayout& Layout, const FElementPalette& Palette, bool bSwitchedOff)
{
	FFlameLayers Layers;
	if (bSwitchedOff)
	{
		UE_LOG(LogMageArena, Log, TEXT("MAGEVR_ARENA_BRAZIER_FIRE off (-MageArenaNoBrazierFire)"));
		return Layers;
	}
	if (!Owner || Layout.Braziers.IsEmpty())
	{
		return Layers;
	}
	UMaterialInterface* Glow = SpellVfx::GlowMaterial();
	UStaticMesh* Sphere = Greybox::LoadShape(TEXT("Sphere"));
	for (const ELayer Layer : {ELayer::Body, ELayer::Heart})
	{
		UMaterialInstanceDynamic* Tint = Greybox::Tint(Glow, Owner, FlameColour(Palette, Layer));
		if (Tint)
		{
			// Denser than a spell glow: a tongue of flame has a body, a bolt's halo only a falloff.
			Tint->SetScalarParameterValue(TEXT("GlowGain"), FlameGlowGain);
		}
		(Layer == ELayer::Body ? Layers.Body : Layers.Heart) = Greybox::MakeInstances(Owner, Sphere, Tint);
	}
	TArray<SpellVfx::FPuff> Body;
	TArray<SpellVfx::FPuff> Heart;
	AllPuffs(Layout, 0.0, Body, Heart);
	Pose(Layers.Body, Body, true);
	Pose(Layers.Heart, Heart, true);
	UE_LOG(LogMageArena, Log, TEXT("MAGEVR_ARENA_BRAZIER_FIRE braziers=%d puffs=%d components=%d"), Layout.Braziers.Num(), Body.Num() + Heart.Num(),
	    (Layers.Body ? 1 : 0) + (Layers.Heart ? 1 : 0));
	return Layers;
}

void BrazierFire::Animate(const FFlameLayers& Layers, const FArenaLayout& Layout, double TimeS)
{
	TArray<SpellVfx::FPuff> Body;
	TArray<SpellVfx::FPuff> Heart;
	AllPuffs(Layout, TimeS, Body, Heart);
	Pose(Layers.Body, Body, false);
	Pose(Layers.Heart, Heart, false);
}
