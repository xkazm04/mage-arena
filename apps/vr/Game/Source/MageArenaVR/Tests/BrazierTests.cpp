#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Components/InstancedStaticMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Greybox/ArenaGreyboxActor.h"
#include "Greybox/ArenaLayout.h"
#include "Greybox/BrazierFire.h"
#include "Materials/Material.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Session/ElementColour.h"
#include "Session/SpellVfx.h"

// T60: the braziers as fire props (Greybox/BrazierFire.h). The flame is scenery: these tests hold its colours out of
// every threat class by chromaticity, as MageArena.SpellVfx.ColoursKeepThreatClass holds the spell glows in theirs, and
// out of the classes MageArena.Greybox.ThreatColours and ThreatPairReserved check, under the greybox palette and every
// shipped style.

namespace
{
// As SpellVfxTests: a colour divided by its brightest channel, so a gain never changes the answer.
FLinearColor Chroma(const FLinearColor& Colour)
{
	const float Most = FMath::Max3(Colour.R, Colour.G, Colour.B);
	return Most > 1.0e-4f ? FLinearColor(Colour.R / Most, Colour.G / Most, Colour.B / Most, 1.f) : FLinearColor::Black;
}

float ChromaDistance(const FLinearColor& A, const FLinearColor& B)
{
	const FLinearColor CA = Chroma(A);
	const FLinearColor CB = Chroma(B);
	return FMath::Sqrt(FMath::Square(CA.R - CB.R) + FMath::Square(CA.G - CB.G) + FMath::Square(CA.B - CB.B));
}

// As GreyboxTests: the layout's threat colour classes.
bool IsEmberOrange(const FLinearColor& Colour)
{
	return Colour.R > 0.7f && Colour.G > 0.15f && Colour.G < 0.45f && Colour.B < 0.25f;
}

bool IsBlackCore(const FLinearColor& Colour)
{
	return Colour.R < 0.08f && Colour.G < 0.08f && Colour.B < 0.08f;
}

bool IsRedRim(const FLinearColor& Colour)
{
	return Colour.R > 0.65f && Colour.G < 0.2f && Colour.B < 0.2f;
}

FLinearColor Rgb(const TSharedPtr<FJsonObject>& Style, const TCHAR* Group, const TCHAR* Field)
{
	const TSharedPtr<FJsonObject>* Object = nullptr;
	const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
	if (!Style->TryGetObjectField(Group, Object) || !(*Object)->TryGetArrayField(Field, Values) || Values->Num() < 3)
	{
		return FLinearColor::Transparent;
	}
	return FLinearColor(static_cast<float>((*Values)[0]->AsNumber()), static_cast<float>((*Values)[1]->AsNumber()), static_cast<float>((*Values)[2]->AsNumber()));
}

TSharedPtr<FJsonObject> LoadStyle(const TCHAR* Id)
{
	FString Path = FPaths::Combine(FPaths::ProjectDir(), FString::Printf(TEXT("../data/vr/styles/style-%s.json"), Id));
	FPaths::CollapseRelativeDirectories(Path);
	FString Json;
	TSharedPtr<FJsonObject> Style;
	if (!FFileHelper::LoadFileToString(Json, *Path) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Style))
	{
		return nullptr;
	}
	return Style;
}

bool LoadLayout(FAutomationTestBase& Test, FArenaLayout& Layout)
{
	FString Error;
	const bool bLoaded = Layout.LoadFromFile(FArenaLayout::DefaultFilePath(), Error);
	return Test.TestTrue(FString::Printf(TEXT("the arena layout loads (%s)"), *Error), bLoaded) && Test.TestEqual(TEXT("four braziers"), Layout.Braziers.Num(), 4);
}

struct FBrazierWorld
{
	UWorld* World = nullptr;
	AArenaGreybox* Arena = nullptr;

	bool Open(FAutomationTestBase& Test)
	{
		World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("BrazierTests"));
		if (!Test.TestNotNull(TEXT("a world is made"), World))
		{
			return false;
		}
		FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
		Context.SetCurrentWorld(World);
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		// Not built: the test places only the flames on the actor.
		Arena = World->SpawnActor<AArenaGreybox>(AArenaGreybox::StaticClass(), FTransform::Identity, Params);
		return Test.TestNotNull(TEXT("the arena actor spawns"), Arena);
	}

	void Close()
	{
		if (World)
		{
			GEngine->DestroyWorldContext(World);
			World->DestroyWorld(false);
			World = nullptr;
		}
	}
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaGreyboxBrazierFlameNotThreat, "MageArena.Greybox.BrazierFlameNotThreat",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaGreyboxBrazierFlameNotThreat::RunTest(const FString& Parameters)
{
	struct FCase
	{
		FString Name;
		FElementPalette Palette;
		// A style's own fire and flame samples, which the flame must never take.
		TArray<FLinearColor> Never;
	};
	TArray<FCase> Cases;
	Cases.Add({TEXT("greybox"), FElementPalette::Greybox(), {}});
	for (const TCHAR* Id : {TEXT("13"), TEXT("25"), TEXT("30")})
	{
		const TSharedPtr<FJsonObject> Style = LoadStyle(Id);
		if (!TestTrue(FString::Printf(TEXT("style %s loads"), Id), Style.IsValid()))
		{
			continue;
		}
		FCase Case{FString::Printf(TEXT("style %s"), Id), FElementPalette::Greybox(), {Rgb(Style, TEXT("projectiles"), TEXT("fire")), Rgb(Style, TEXT("arena"), TEXT("flame"))}};
		TestEqual(FString::Printf(TEXT("style %s sets all seven threat colours"), Id), ApplyStylePalette(Case.Palette, *Style), 7);
		Cases.Add(Case);
	}

	bool bPass = true;
	for (const FCase& Case : Cases)
	{
		const FElementPalette& P = Case.Palette;
		struct FThreat
		{
			const TCHAR* Name;
			FLinearColor Colour;
		};
		const FThreat Threats[] = {
		    {TEXT("water"), P.Water}, {TEXT("fire"), P.Fire}, {TEXT("air"), P.Air}, {TEXT("earth"), P.Earth},
		    {TEXT("steel"), P.Steel}, {TEXT("unblockable rim"), P.UnblockableRim}, {TEXT("unblockable core"), P.UnblockableBody}};
		// T61 (option B, DECISIONS 2026-10-25 on deaa15b): brighter than the rim, dimmer than the dimmer of Fire and Earth.
		const float RimLuminance = P.UnblockableRim.GetLuminance();
		const float FireEarthLuminance = FMath::Min(P.Fire.GetLuminance(), P.Earth.GetLuminance());
		const float HeartLuminance = BrazierFire::FlameColour(P, BrazierFire::ELayer::Heart).GetLuminance();
		for (const BrazierFire::ELayer Layer : {BrazierFire::ELayer::Body, BrazierFire::ELayer::Heart})
		{
			const FLinearColor Flame = BrazierFire::FlameColour(P, Layer);
			const FString What = FString::Printf(TEXT("%s %s flame %s"), *Case.Name, Layer == BrazierFire::ELayer::Body ? TEXT("body") : TEXT("heart"), *Flame.ToString());
			// Out of every threat class by chromaticity, with a margin seven times the classifier's 0.02.
			for (const FThreat& Threat : Threats)
			{
				const float Distance = ChromaDistance(Flame, Threat.Colour);
				bPass &= TestTrue(FString::Printf(TEXT("%s stands off %s by chroma (%.3f)"), *What, Threat.Name, Distance), Distance >= BrazierFire::ThreatChromaMargin);
			}
			for (const FLinearColor& Sample : Case.Never)
			{
				const float Distance = ChromaDistance(Flame, Sample);
				bPass &= TestTrue(FString::Printf(TEXT("%s is not the style's fire or flame sample %s (%.3f)"), *What, *Sample.ToString(), Distance), Distance >= BrazierFire::ThreatChromaMargin);
			}
			// A pale flame bright enough to read as fire against the lit wall, still dimmer than the two orange glows: hue
			// and shape keep it off the fire bolt, not darkness. The body never outshines the heart.
			const float Luminance = Flame.GetLuminance();
			bPass &= TestTrue(FString::Printf(TEXT("%s luminance %.3f is over the rim's %.3f"), *What, Luminance, RimLuminance), Luminance > RimLuminance);
			bPass &= TestTrue(FString::Printf(TEXT("%s luminance %.3f is under min(Fire, Earth) %.3f"), *What, Luminance, FireEarthLuminance), Luminance < FireEarthLuminance);
			bPass &= TestTrue(FString::Printf(TEXT("%s luminance %.3f is at or under the heart's %.3f"), *What, Luminance, HeartLuminance), Luminance <= HeartLuminance + 1.0e-6f);
			// Not one of the layout's threat classes, and not either half of the leave pair.
			bPass &= TestFalse(FString::Printf(TEXT("%s is not ember orange"), *What), IsEmberOrange(Flame));
			bPass &= TestFalse(FString::Printf(TEXT("%s is not the red rim"), *What), IsRedRim(Flame));
			bPass &= TestFalse(FString::Printf(TEXT("%s is not a black core"), *What), IsBlackCore(Flame));
			bPass &= TestTrue(FString::Printf(TEXT("%s stays off the black core line"), *What), FMath::Max3(Flame.R, Flame.G, Flame.B) >= BrazierFire::MinBrightest - 1.0e-6f);
			// Warm: red over green over blue.
			bPass &= TestTrue(FString::Printf(TEXT("%s is warm"), *What), Flame.R > Flame.G && Flame.G > Flame.B);
			if (Layer == BrazierFire::ELayer::Heart)
			{
				// Pale: its weakest channel stands higher against its brightest than Fire's or Earth's, so hue tells it from
				// the orange glows (min over max, the inverse of saturation).
				const auto Paleness = [](const FLinearColor& C) { return FMath::Min3(C.R, C.G, C.B) / FMath::Max(FMath::Max3(C.R, C.G, C.B), 1.0e-4f); };
				bPass &= TestTrue(FString::Printf(TEXT("%s is paler than Fire and Earth (%.2f)"), *What, Paleness(Flame)), Paleness(Flame) > FMath::Max(Paleness(P.Fire), Paleness(P.Earth)) + 0.25f);
			}
		}
	}

	// The layout itself carries no flame colour a style could repaint: ApplyStyle reads no projectiles.fire.
	FArenaLayout Layout;
	FString Error;
	if (TestTrue(TEXT("layout loads"), Layout.LoadFromFile(FArenaLayout::DefaultFilePath(), Error)))
	{
		const TSharedPtr<FJsonObject> Style = LoadStyle(TEXT("25"));
		if (Style.IsValid())
		{
			FJsonObject FireOnly;
			FireOnly.SetObjectField(TEXT("projectiles"), Style->GetObjectField(TEXT("projectiles")));
			bPass &= TestEqual(TEXT("a style's projectiles paint no arena colour"), Layout.ApplyStyle(FireOnly), 0);
		}
	}
	return bPass;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaGreyboxBrazierBowl, "MageArena.Greybox.Brazier.Bowl",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaGreyboxBrazierBowl::RunTest(const FString& Parameters)
{
	FArenaLayout Layout;
	if (!LoadLayout(*this, Layout))
	{
		return false;
	}
	for (const FArenaProp& Brazier : Layout.Braziers)
	{
		TArray<BrazierFire::FPiece> Pieces;
		BrazierFire::BowlPieces(Brazier, Pieces);
		if (!TestEqual(TEXT("seven pieces"), Pieces.Num(), BrazierFire::BowlPiecesPerBrazier))
		{
			continue;
		}
		const double Lip = Brazier.StandHeightM + Brazier.BowlHeightM;
		TestTrue(TEXT("the foot stands on the sand"), FMath::IsNearlyZero(Pieces[0].CentreM.Z - Pieces[0].SizeM.Z * 0.5, 1.0e-6));
		TestTrue(TEXT("the lip tops the bowl"), FMath::IsNearlyEqual(Pieces.Last().CentreM.Z + Pieces.Last().SizeM.Z * 0.5, Lip, 1.0e-6));
		TestTrue(TEXT("the lip is the layout's bowl radius"), FMath::IsNearlyEqual(Pieces.Last().SizeM.X * 0.5, Brazier.BowlRadiusM, 1.0e-6));
		TestTrue(TEXT("the stem is slimmer than the foot"), Pieces[1].SizeM.X < Pieces[0].SizeM.X);
		for (int32 Index = 0; Index < Pieces.Num(); ++Index)
		{
			const BrazierFire::FPiece& Piece = Pieces[Index];
			TestTrue(TEXT("a piece sits on the brazier's spot"), FMath::IsNearlyEqual(Piece.CentreM.X, Brazier.PositionM.X) && FMath::IsNearlyEqual(Piece.CentreM.Y, Brazier.PositionM.Y));
			TestTrue(TEXT("a piece is round"), FMath::IsNearlyEqual(Piece.SizeM.X, Piece.SizeM.Y));
			if (Index > 0)
			{
				const BrazierFire::FPiece& Below = Pieces[Index - 1];
				// Stacked without a gap or an overlap: each piece starts where the one below ends.
				TestTrue(TEXT("pieces stack"), FMath::IsNearlyEqual(Below.CentreM.Z + Below.SizeM.Z * 0.5, Piece.CentreM.Z - Piece.SizeM.Z * 0.5, 1.0e-6));
			}
			if (Index > 2)
			{
				// The bowl flares: every ring above the stem is wider than the one below.
				TestTrue(TEXT("the bowl widens upward"), Piece.SizeM.X > Pieces[Index - 1].SizeM.X);
			}
		}
		TestTrue(TEXT("the bowl's base is narrower than half its lip"), Pieces[2].SizeM.X < Pieces.Last().SizeM.X * 0.5);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaGreyboxBrazierFlicker, "MageArena.Greybox.Brazier.Flicker",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaGreyboxBrazierFlicker::RunTest(const FString& Parameters)
{
	FArenaLayout Layout;
	if (!LoadLayout(*this, Layout))
	{
		return false;
	}
	const int32 PerBrazier = BrazierFire::BodyPuffsPerBrazier + BrazierFire::HeartPuffsPerBrazier;
	// Counted against the 2,000-particle budget with the spell glow: T58's D-G4 drew 1,468 Niagara particles and 400 puffs.
	TestTrue(TEXT("the flames fit beside the spell puffs under 2,000"), 1468 + SpellVfx::MaxPuffs + PerBrazier * Layout.Braziers.Num() <= 2000);
	TestTrue(TEXT("the flicker is slow (under 1.5 Hz)"), BrazierFire::FastestFlickerHz < 1.5);

	const FArenaProp& Brazier = Layout.Braziers[0];
	const double R = Brazier.FlameRadiusM * 100.0;
	const FVector Spot(Brazier.PositionM.X * 100.0, Brazier.PositionM.Y * 100.0, (Brazier.StandHeightM + Brazier.BowlHeightM) * 100.0);
	TArray<double> TopZ;
	TArray<FVector> Previous;
	constexpr double StepS = 1.0 / 30.0;
	for (int32 Frame = 0; Frame <= 240; ++Frame)
	{
		TArray<SpellVfx::FPuff> Body;
		TArray<SpellVfx::FPuff> Heart;
		BrazierFire::FlamePuffs(Brazier, 1, Frame * StepS, Body, Heart);
		if (!TestEqual(TEXT("three body puffs"), Body.Num(), BrazierFire::BodyPuffsPerBrazier) || !TestEqual(TEXT("two heart puffs"), Heart.Num(), BrazierFire::HeartPuffsPerBrazier))
		{
			return false;
		}
		TArray<FVector> Centres;
		for (const TArray<SpellVfx::FPuff>* Layer : {&Body, &Heart})
		{
			for (const SpellVfx::FPuff& Puff : *Layer)
			{
				const FVector Off = Puff.CentreCm - Spot;
				// Every puff stays inside the old cone's silhouette (radius R at the lip, a point MaxFlameTopRadii above it): a
				// lick, not a spark thrown off, and nothing that covers more of a threat's flight line than the cone did.
				const double Top = Off.Z + Puff.SizeCm.Z * 0.5;
				const double Reach = FVector2D(Off.X, Off.Y).Size() + FMath::Max(Puff.SizeCm.X, Puff.SizeCm.Y) * 0.5;
				const double ConeHalfWidth = R * (1.0 - Off.Z / (R * BrazierFire::MaxFlameTopRadii));
				if (!TestTrue(FString::Printf(TEXT("frame %d puff stays inside the cone (%s, reach %.1f of %.1f cm, top %.1f cm)"), Frame, *Off.ToString(), Reach, ConeHalfWidth, Top),
				        Off.Z >= 0.0 && Top <= R * BrazierFire::MaxFlameTopRadii && Reach <= ConeHalfWidth + R * 0.05))
				{
					return false;
				}
				Centres.Add(Puff.CentreCm);
			}
		}
		if (Previous.Num() == Centres.Num())
		{
			for (int32 Index = 0; Index < Centres.Num(); ++Index)
			{
				// Slow: no puff moves more than a fifth of the flame radius in a 30 Hz frame (an ember at 23 rad/s would).
				const double Step = FVector::Dist(Centres[Index], Previous[Index]);
				if (!TestTrue(FString::Printf(TEXT("frame %d puff %d moves slowly (%.2f cm)"), Frame, Index, Step), Step <= R * 0.2))
				{
					return false;
				}
			}
		}
		Previous = Centres;
		TopZ.Add(Body.Last().CentreCm.Z);
	}
	// The lick rises and falls about its place; it never climbs away as sparks do.
	int32 Rises = 0;
	int32 Falls = 0;
	for (int32 Index = 1; Index < TopZ.Num(); ++Index)
	{
		Rises += TopZ[Index] > TopZ[Index - 1] ? 1 : 0;
		Falls += TopZ[Index] < TopZ[Index - 1] ? 1 : 0;
	}
	TestTrue(FString::Printf(TEXT("the lick both rises and falls over 8 s (%d up, %d down)"), Rises, Falls), Rises > 30 && Falls > 30);

	// Irregular: no two braziers lick together.
	for (int32 A = 0; A < Layout.Braziers.Num(); ++A)
	{
		for (int32 B = A + 1; B < Layout.Braziers.Num(); ++B)
		{
			TArray<SpellVfx::FPuff> BodyA;
			TArray<SpellVfx::FPuff> HeartA;
			TArray<SpellVfx::FPuff> BodyB;
			TArray<SpellVfx::FPuff> HeartB;
			BrazierFire::FlamePuffs(Layout.Braziers[A], A + 1, 2.0, BodyA, HeartA);
			BrazierFire::FlamePuffs(Layout.Braziers[B], B + 1, 2.0, BodyB, HeartB);
			const double ZA = BodyA.Last().CentreCm.Z - (Layout.Braziers[A].StandHeightM + Layout.Braziers[A].BowlHeightM) * 100.0;
			const double ZB = BodyB.Last().CentreCm.Z - (Layout.Braziers[B].StandHeightM + Layout.Braziers[B].BowlHeightM) * 100.0;
			TestFalse(FString::Printf(TEXT("braziers %d and %d are out of phase"), A, B), FMath::IsNearlyEqual(ZA, ZB, 0.01));
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaGreyboxBrazierFireBatch, "MageArena.Greybox.Brazier.FireBatch",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaGreyboxBrazierFireBatch::RunTest(const FString& Parameters)
{
	FArenaLayout Layout;
	if (!LoadLayout(*this, Layout))
	{
		return false;
	}
	FBrazierWorld Rig;
	if (!Rig.Open(*this))
	{
		Rig.Close();
		return false;
	}
	const FElementPalette Palette = FElementPalette::Greybox();
	TArray<UActorComponent*> Before;
	Rig.Arena->GetComponents(Before);
	// -MageArenaNoBrazierFire: nothing is added, so the bowls stand cold.
	const BrazierFire::FFlameLayers Off = BrazierFire::Build(Rig.Arena, Layout, Palette, true);
	TestTrue(TEXT("no flame with the switch set"), !Off.Body && !Off.Heart);
	TArray<UActorComponent*> AfterOff;
	Rig.Arena->GetComponents(AfterOff);
	TestEqual(TEXT("the switch adds no component"), AfterOff.Num(), Before.Num());

	const BrazierFire::FFlameLayers On = BrazierFire::Build(Rig.Arena, Layout, Palette, false);
	TArray<UActorComponent*> After;
	Rig.Arena->GetComponents(After);
	// Two instanced components for all four braziers: two draw calls, whatever the number of braziers.
	TestEqual(TEXT("the flames add exactly two components"), After.Num() - Before.Num(), 2);
	struct FLayer
	{
		UInstancedStaticMeshComponent* Mesh;
		int32 PerBrazier;
	};
	const FLayer Layers[] = {{On.Body, BrazierFire::BodyPuffsPerBrazier}, {On.Heart, BrazierFire::HeartPuffsPerBrazier}};
	for (const FLayer& Layer : Layers)
	{
		if (!TestNotNull(TEXT("a flame layer"), Layer.Mesh))
		{
			continue;
		}
		TestEqual(TEXT("its puffs for every brazier"), Layer.Mesh->GetInstanceCount(), Layer.PerBrazier * Layout.Braziers.Num());
		TestFalse(TEXT("a flame casts no shadow"), Layer.Mesh->CastShadow);
		TestEqual(TEXT("one material slot"), Layer.Mesh->GetNumMaterials(), 1);
		const UMaterial* Material = Layer.Mesh->GetMaterial(0) ? Layer.Mesh->GetMaterial(0)->GetMaterial() : nullptr;
		if (TestNotNull(TEXT("the flame material"), Material))
		{
			TestTrue(TEXT("the flame is T58's glow"), Material->GetPathName().StartsWith(SpellVfx::GlowMaterialPackage));
			TestEqual(TEXT("translucent"), static_cast<int32>(Material->GetBlendMode()), static_cast<int32>(BLEND_Translucent));
		}
	}
	if (!On.Body)
	{
		Rig.Close();
		return false;
	}
	// Animate moves the puffs and adds none.
	FTransform First;
	On.Body->GetInstanceTransform(2, First);
	BrazierFire::Animate(On, Layout, 1.3);
	FTransform Moved;
	On.Body->GetInstanceTransform(2, Moved);
	TestEqual(TEXT("animating keeps the body puffs"), On.Body->GetInstanceCount(), BrazierFire::BodyPuffsPerBrazier * Layout.Braziers.Num());
	TestFalse(TEXT("animating moves the lick"), First.Equals(Moved, 0.01));
	Rig.Close();
	return true;
}

#endif
