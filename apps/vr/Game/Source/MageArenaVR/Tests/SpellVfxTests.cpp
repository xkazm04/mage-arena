#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Kernel/ArenaKernel.h"
#include "Kernel/Enemies.h"
#include "Kernel/KernelData.h"
#include "Kernel/SimTypes.h"
#include "Dom/JsonObject.h"
#include "Engine/Texture2D.h"
#include "Greybox/BrazierFire.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionLinearInterpolate.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionTextureSample.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Session/ElementColour.h"
#include "Session/SpellVfx.h"

// T58. Every spell VFX colour (bolt shell, trail, absorb splash, perfect ring) is GlowColour of the look the presentation
// already drew the threat with, and the unblockable rim's pulse is RimPulseColour of its rim, so these tests hold the VFX
// to the threat colour language: magic glows in exactly its element's colour (absorb), the unblockable glows and pulses
// only in its rim red with its core still black (leave), and steel never glows (dodge).

namespace
{
enum class EThreatClass : uint8
{
	Absorb,
	Dodge,
	Leave,
	None,
};

// Chromaticity: a colour divided by its brightest channel, so an additive gain never changes the answer.
FLinearColor Chroma(const FLinearColor& Colour)
{
	const float Most = FMath::Max3(Colour.R, Colour.G, Colour.B);
	return Most > 1.0e-4f ? FLinearColor(Colour.R / Most, Colour.G / Most, Colour.B / Most, 1.f) : FLinearColor::Black;
}

// The class a VFX colour reads as against Palette, and for Absorb which element (the nearest chroma, within 0.02).
EThreatClass Classify(const FElementPalette& Palette, const FLinearColor& Colour, TOptional<EElement>& OutElement)
{
	OutElement.Reset();
	const FLinearColor C = Chroma(Colour);
	if (C.Equals(Chroma(Palette.UnblockableRim), 0.02f))
	{
		return EThreatClass::Leave;
	}
	if (C.Equals(Chroma(Palette.Steel), 0.02f))
	{
		return EThreatClass::Dodge;
	}
	for (const EElement Element : {EElement::Water, EElement::Fire, EElement::Air, EElement::Earth})
	{
		if (C.Equals(Chroma(Palette.ElementColour(Element)), 0.02f))
		{
			OutElement = Element;
			return EThreatClass::Absorb;
		}
	}
	return EThreatClass::None;
}

const TCHAR* ClassName(EThreatClass Class)
{
	switch (Class)
	{
	case EThreatClass::Absorb:
		return TEXT("absorb");
	case EThreatClass::Dodge:
		return TEXT("dodge");
	case EThreatClass::Leave:
		return TEXT("leave");
	default:
		return TEXT("none");
	}
}

// The style palettes a run may apply over the greybox one: the greybox itself, and a palette whose every colour moved.
TArray<FElementPalette> Palettes()
{
	FElementPalette Moved = FElementPalette::Greybox();
	Moved.Water = FLinearColor(0.02f, 0.60f, 0.80f);
	Moved.Fire = FLinearColor(1.0f, 0.35f, 0.04f);
	Moved.Air = FLinearColor(0.30f, 0.90f, 0.35f);
	Moved.Steel = FLinearColor(0.75f, 0.75f, 0.78f);
	Moved.UnblockableRim = FLinearColor(0.90f, 0.02f, 0.02f);
	return {FElementPalette::Greybox(), Moved};
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaSpellVfxThreatClass, "MageArena.SpellVfx.ColoursKeepThreatClass",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaSpellVfxThreatClass::RunTest(const FString& Parameters)
{
	bool bPass = true;
	int32 PaletteIndex = 0;
	for (const FElementPalette& Palette : Palettes())
	{
		for (const EElement Element : {EElement::Water, EElement::Fire, EElement::Air, EElement::Earth})
		{
			const FString School = FString::Printf(TEXT("palette %d school %d"), PaletteIndex, static_cast<int32>(Element));
			// Magic: the VFX glows in the element colour, nothing else.
			const FThreatLook Magic = ThreatLook(Palette, Element, TEXT("magic"));
			const TOptional<FLinearColor> MagicGlow = SpellVfx::GlowColour(Magic, TEXT("magic"));
			if (!TestTrue(*FString::Printf(TEXT("%s magic glows"), *School), MagicGlow.IsSet()))
			{
				bPass = false;
				continue;
			}
			TOptional<EElement> Read;
			const EThreatClass MagicClass = Classify(Palette, MagicGlow.GetValue(), Read);
			bPass &= TestEqual(*FString::Printf(TEXT("%s magic glow is absorb (read %s)"), *School, ClassName(MagicClass)),
			    static_cast<int32>(MagicClass), static_cast<int32>(EThreatClass::Absorb));
			bPass &= TestTrue(*FString::Printf(TEXT("%s magic glow is its own element"), *School), Read.IsSet() && Read.GetValue() == Element);
			bPass &= TestTrue(*FString::Printf(TEXT("%s magic glow equals the threat body"), *School), MagicGlow.GetValue().Equals(Magic.Body, 1.0e-6f));

			// Unblockable: only the rim red glows; the core keeps its black.
			const FThreatLook Leave = ThreatLook(Palette, Element, TEXT("unblockable"));
			const TOptional<FLinearColor> LeaveGlow = SpellVfx::GlowColour(Leave, TEXT("unblockable"));
			if (TestTrue(*FString::Printf(TEXT("%s unblockable glows"), *School), LeaveGlow.IsSet()))
			{
				const EThreatClass LeaveClass = Classify(Palette, LeaveGlow.GetValue(), Read);
				bPass &= TestEqual(*FString::Printf(TEXT("%s unblockable glow is leave (read %s)"), *School, ClassName(LeaveClass)),
				    static_cast<int32>(LeaveClass), static_cast<int32>(EThreatClass::Leave));
				bPass &= TestFalse(*FString::Printf(TEXT("%s unblockable glow is not the core"), *School), LeaveGlow.GetValue().Equals(Leave.Body, 0.05f));
			}
			else
			{
				bPass = false;
			}
			bPass &= TestTrue(*FString::Printf(TEXT("%s unblockable core stays black"), *School), Leave.Body.R < 0.08f && Leave.Body.G < 0.08f && Leave.Body.B < 0.08f);

			// The unblockable rim's pulse brightens the rim red and never moves it off the leave class.
			for (const float Pulse : {0.f, 0.25f, 0.5f, 0.75f, 1.f})
			{
				const FLinearColor Pulsed = SpellVfx::RimPulseColour(Leave.Rim, Pulse);
				const EThreatClass PulseClass = Classify(Palette, Pulsed, Read);
				bPass &= TestEqual(*FString::Printf(TEXT("%s rim at pulse %.2f is leave (read %s)"), *School, Pulse, ClassName(PulseClass)),
				    static_cast<int32>(PulseClass), static_cast<int32>(EThreatClass::Leave));
				bPass &= TestTrue(*FString::Printf(TEXT("%s rim at pulse %.2f is no dimmer"), *School, Pulse), Pulsed.R >= Leave.Rim.R - 1.0e-6f);
			}

			// Physical: steel never glows.
			const FThreatLook Dodge = ThreatLook(Palette, Element, TEXT("physical"));
			bPass &= TestFalse(*FString::Printf(TEXT("%s steel has no glow"), *School), SpellVfx::GlowColour(Dodge, TEXT("physical")).IsSet());
		}
		++PaletteIndex;
	}
	return bPass;
}

// The colours the presentation passes for every owner of the T26 cast: a projectile's look comes from ElementLook, so
// the glow of a Fire mage's bolt is ember orange, the player's own bolt Water, an unblockable rim red, a conscript none.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaSpellVfxOwners, "MageArena.SpellVfx.OwnersGlowTheirSchool",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaSpellVfxOwners::RunTest(const FString& Parameters)
{
	const FElementPalette Palette = FElementPalette::Greybox();
	FArenaState State = CreateArena(58);
	const FSimVec Centre = KernelData().ArenaCentre;
	const int32 Player = AddMage(State, 0, Centre, TEXT("Player")).Id;
	FActor& FireMage = AddMage(State, 1, FSimVec{Centre.X + 8.0, Centre.Y}, TEXT("Brennic"));
	FireMage.Fire.bSchool = true;
	const int32 Fire = FireMage.Id;
	FActor& AirMage = AddMage(State, 1, FSimVec{Centre.X + 8.0, Centre.Y + 2.0}, TEXT("Lio"));
	AirMage.Air.bSchool = true;
	const int32 Air = AirMage.Id;
	const int32 Hound = AddEnemy(State, TEXT("cinder_hound"), FSimVec{Centre.X + 5.0, Centre.Y + 4.0}).Id;
	const int32 Conscript = AddEnemy(State, TEXT("conscript"), FSimVec{Centre.X + 6.0, Centre.Y}).Id;
	struct FRow
	{
		const TCHAR* Who;
		int32 Owner;
		const TCHAR* Family;
		TOptional<FLinearColor> Expected;
	};
	const FRow Rows[] = {
	    {TEXT("Fire mage bolt"), Fire, TEXT("magic"), Palette.Fire},
	    {TEXT("air mage bolt"), Air, TEXT("magic"), Palette.Air},
	    {TEXT("hound ember"), Hound, TEXT("magic"), Palette.Fire},
	    {TEXT("player bolt"), Player, TEXT("magic"), Palette.Water},
	    {TEXT("player's own orb"), Player, TEXT("unblockable"), Palette.Water},
	    {TEXT("Fire mage unblockable"), Fire, TEXT("unblockable"), Palette.UnblockableRim},
	    {TEXT("conscript spear"), Conscript, TEXT("physical"), {}},
	    {TEXT("Fire mage stone"), Fire, TEXT("physical"), {}},
	};
	bool bPass = true;
	for (const FRow& Row : Rows)
	{
		const FThreatLook Look = ElementLook(Palette, State, Player, Row.Owner, Row.Family);
		const TOptional<FLinearColor> Glow = SpellVfx::GlowColour(Look, Row.Family);
		bPass &= TestEqual(*FString::Printf(TEXT("%s glows"), Row.Who), Glow.IsSet(), Row.Expected.IsSet());
		if (Glow.IsSet() && Row.Expected.IsSet())
		{
			bPass &= TestTrue(*FString::Printf(TEXT("%s glow colour %s"), Row.Who, *Glow.GetValue().ToString()), Glow.GetValue().Equals(Row.Expected.GetValue(), 1.0e-6f));
		}
	}
	return bPass;
}

// The puffs stay inside the budget: a bolt adds at most ten, the trail is behind the bolt, the rim pulse takes a few
// shared tints, and the bursts end on time.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaSpellVfxPuffs, "MageArena.SpellVfx.PuffsStayInBudget",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaSpellVfxPuffs::RunTest(const FString& Parameters)
{
	bool bPass = true;
	for (const SpellVfx::ETrail Trail : {SpellVfx::ETrail::Droplets, SpellVfx::ETrail::Embers, SpellVfx::ETrail::Wisps, SpellVfx::ETrail::Grit, SpellVfx::ETrail::None})
	{
		TArray<SpellVfx::FPuff> Puffs;
		SpellVfx::BoltPuffs(Trail, FVector(100.0, 0.0, 120.0), FVector(-1.0, 0.0, 0.0), 22.0, 7, 1.25, Puffs);
		bPass &= TestTrue(*FString::Printf(TEXT("trail %d: 1 to 10 puffs (got %d)"), static_cast<int32>(Trail), Puffs.Num()), Puffs.Num() >= 1 && Puffs.Num() <= 10);
		bPass &= TestTrue(*FString::Printf(TEXT("trail %d: the shell is around the core"), static_cast<int32>(Trail)),
		    Puffs.Num() > 0 && Puffs[0].CentreCm.Equals(FVector(100.0, 0.0, 120.0)) && Puffs[0].SizeCm.X > 22.0);
		for (int32 Index = 1; Index < Puffs.Num(); ++Index)
		{
			// Behind the bolt: the flight is -X, so the trail is at larger X.
			bPass &= TestTrue(*FString::Printf(TEXT("trail %d puff %d is behind the bolt"), static_cast<int32>(Trail), Index), Puffs[Index].CentreCm.X > 100.0);
		}
	}
	// The unblockable rim's pulse: a handful of steps, never dimmer than the rim, and always the rim's own chroma.
	TSet<float> Steps;
	for (double Time = 0.0; Time < 2.0; Time += 0.01)
	{
		Steps.Add(SpellVfx::RimPulse(Time));
	}
	bPass &= TestTrue(*FString::Printf(TEXT("the rim pulse takes at most %d steps (took %d)"), static_cast<int32>(SpellVfx::RimPulseSteps) + 1, Steps.Num()),
	    Steps.Num() >= 2 && Steps.Num() <= static_cast<int32>(SpellVfx::RimPulseSteps) + 1);
	TArray<SpellVfx::FPuff> Done;
	SpellVfx::SplashPuffs(SpellVfx::ETrail::Droplets, FVector::ZeroVector, FVector::BackwardVector, 1.0, 3, true, Done);
	SpellVfx::PerfectRingPuffs(FVector::ZeroVector, FVector::BackwardVector, 1.0, Done);
	bPass &= TestEqual(TEXT("a finished splash and ring draw nothing"), Done.Num(), 0);
	TArray<SpellVfx::FPuff> Live;
	SpellVfx::SplashPuffs(SpellVfx::ETrail::Embers, FVector::ZeroVector, FVector::BackwardVector, 0.2, 3, true, Live);
	SpellVfx::PerfectRingPuffs(FVector::ZeroVector, FVector::BackwardVector, 0.2, Live);
	bPass &= TestTrue(TEXT("a live splash and ring draw puffs"), Live.Num() > 20);
	// 1,468 live Niagara particles at the D-G4 max (DRAWCALL-BUDGET section 4) plus the cap stays at or under 2,000.
	bPass &= TestTrue(TEXT("the puff cap keeps the particle budget"), 1468 + SpellVfx::MaxPuffs <= 2000);
	return bPass;
}

// L2. A fire wall flame is one Fire flame card standing on the sand, narrower than the 1.3 m between neighbours on the
// 4 m, 150 degree wall and lower than the seated eye (1.2 m), so the rival shows between and above the flames.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaSpellVfxWallFlames, "MageArena.SpellVfx.WallFlamesLeaveTheRivalVisible",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaSpellVfxWallFlames::RunTest(const FString& Parameters)
{
	bool bPass = true;
	for (int32 Index = 0; Index < 8; ++Index)
	{
		TArray<SpellVfx::FPuff> Puffs;
		SpellVfx::WallFlamePuffs(FVector(400.0, 0.0, 0.0), Index, 0.7 * Index, Puffs);
		bPass &= TestEqual(TEXT("one puff per flame"), Puffs.Num(), 1);
		if (Puffs.Num() == 1)
		{
			const SpellVfx::FPuff& Puff = Puffs[0];
			bPass &= TestTrue(TEXT("a Fire flame cell"), Puff.Cell == SpellVfx::ECell::FireFlameA || Puff.Cell == SpellVfx::ECell::FireFlameB);
			bPass &= TestTrue(TEXT("stands on the sand"), FMath::IsNearlyEqual(Puff.CentreCm.Z - Puff.SizeCm.Y * 0.5, 0.0, 0.01));
		}
	}
	const double SpacingCm = 400.0 * (150.0 * PI / 180.0) / 8.0;
	bPass &= TestTrue(TEXT("flames leave gaps"), SpellVfx::WallFlameWidthCm < SpacingCm * 0.6);
	bPass &= TestTrue(TEXT("flames stay below the seated eye"), SpellVfx::WallFlameHeightCm < 120.0);
	return bPass;
}

// T63. The flipbook keeps T58's draw count: every glow colour is one layer (one instanced draw), whatever cells its
// puffs show, because the cell rides in the per-instance custom data, not in the material or the layer key.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaSpellVfxFlipbookLayers, "MageArena.SpellVfx.FlipbookKeepsOneLayerPerColour",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaSpellVfxFlipbookLayers::RunTest(const FString& Parameters)
{
	bool bPass = true;
	const FElementPalette Palette = FElementPalette::Greybox();
	const FVector Eye(0.0, 0.0, 120.0);
	const FVector Ward(110.0, 0.0, 120.0);
	SpellVfx::FGlowFrame Frame;
	Frame.Reset(Eye);
	// Each school's bolt, plain splash and perfect ring in its own colour, as the presentation queues them in a frame.
	struct FSchool
	{
		SpellVfx::ETrail Trail;
		FLinearColor Colour;
	};
	const FSchool Schools[] = {
	    {SpellVfx::ETrail::Droplets, Palette.Water},
	    {SpellVfx::ETrail::Embers, Palette.Fire},
	    {SpellVfx::ETrail::Wisps, Palette.Air},
	};
	for (const FSchool& School : Schools)
	{
		for (int32 Bolt = 0; Bolt < 3; ++Bolt)
		{
			TArray<SpellVfx::FPuff> Puffs;
			SpellVfx::BoltPuffs(School.Trail, FVector(350.0, -120.0 + 80.0 * Bolt, 130.0), FVector(-1.0, 0.0, 0.0), 18.0, 11 + Bolt, 4.0 + 0.13 * Bolt, Puffs);
			SpellVfx::SplashPuffs(School.Trail, Ward, Eye - Ward, 0.3, 5 + Bolt, true, Puffs);
			SpellVfx::SplashPuffs(School.Trail, Ward, Eye - Ward, 0.3, 9 + Bolt, false, Puffs);
			SpellVfx::PerfectRingPuffs(Ward, Eye - Ward, 0.3, Puffs, School.Trail);
			Frame.Add(School.Colour, Puffs);
		}
	}
	// The unblockable's shell: rim red, no trail, the soft glow only.
	TArray<SpellVfx::FPuff> Leave;
	SpellVfx::BoltPuffs(SpellVfx::ETrail::None, FVector(500.0, 0.0, 10.0), FVector(-1.0, 0.0, 0.0), 30.0, 3, 4.0, Leave);
	Frame.Add(Palette.UnblockableRim, Leave);

	bPass &= TestEqual(TEXT("four colours, four layers"), Frame.Layers.Num(), 4);
	int32 Queued = 0;
	for (const TPair<FLinearColor, SpellVfx::FGlowFrame::FLayer>& Layer : Frame.Layers)
	{
		const FString Name = Layer.Key.ToString();
		const int32 Cards = Layer.Value.Transforms.Num();
		Queued += Cards;
		bPass &= TestEqual(*FString::Printf(TEXT("%s: three custom floats a card"), *Name), Layer.Value.CustomData.Num(), Cards * SpellVfx::CardCustomFloats);
		TSet<SpellVfx::ECell> Cells;
		for (int32 Card = 0; Card < Cards; ++Card)
		{
			float Data[SpellVfx::CardCustomFloats];
			for (int32 Index = 0; Index < SpellVfx::CardCustomFloats; ++Index)
			{
				Data[Index] = Layer.Value.CustomData[Card * SpellVfx::CardCustomFloats + Index];
			}
			bool bMirror = false;
			Cells.Add(SpellVfx::CellOfCustomData(Data, bMirror));
			// The cell stays inside the atlas: U and V of the card's corners in 0..1.
			const float U0 = Data[1];
			const float U1 = Data[1] + Data[0];
			bPass &= TestTrue(*FString::Printf(TEXT("%s card %d reads inside the atlas"), *Name, Card),
			    FMath::Min(U0, U1) >= -1.0e-4f && FMath::Max(U0, U1) <= 1.0001f && Data[2] >= 0.f && Data[2] + 1.f / SpellVfx::FlipbookGrid <= 1.0001f);
		}
		if (Layer.Key.Equals(Palette.UnblockableRim))
		{
			bPass &= TestTrue(TEXT("the unblockable shows only the soft glow"), Cells.Num() == 1 && Cells.Contains(SpellVfx::ECell::SoftGlow));
		}
		else
		{
			// Shell, trail, crown and streak cells all on the one layer of the colour.
			bPass &= TestTrue(*FString::Printf(TEXT("%s: one layer carries shell, trail and burst cells (got %d)"), *Name, Cells.Num()), Cells.Num() >= 3);
			bPass &= TestFalse(*FString::Printf(TEXT("%s: a school never shows the unblockable's soft glow"), *Name), Cells.Contains(SpellVfx::ECell::SoftGlow));
		}
	}
	bPass &= TestEqual(TEXT("every queued puff is counted"), Queued, Frame.Drawn);

	// Reset keeps the colours (so an emptied layer is hidden, not leaked) and empties them.
	Frame.Reset(Eye);
	bPass &= TestEqual(TEXT("reset keeps the layers"), Frame.Layers.Num(), 4);
	for (const TPair<FLinearColor, SpellVfx::FGlowFrame::FLayer>& Layer : Frame.Layers)
	{
		bPass &= TestTrue(TEXT("reset empties a layer"), Layer.Value.Transforms.IsEmpty() && Layer.Value.CustomData.IsEmpty());
	}
	// The cap still holds with the flipbook on.
	TArray<SpellVfx::FPuff> Many;
	for (int32 Bolt = 0; Bolt < 80; ++Bolt)
	{
		SpellVfx::BoltPuffs(SpellVfx::ETrail::Embers, FVector(400.0, Bolt * 10.0, 120.0), FVector(-1.0, 0.0, 0.0), 18.0, Bolt, 2.0, Many);
	}
	Frame.Add(Palette.Fire, Many);
	bPass &= TestEqual(TEXT("the puff cap holds"), Frame.Drawn, SpellVfx::MaxPuffs);
	bPass &= TestEqual(TEXT("the capped layer holds the cap"), Frame.Layers.FindChecked(Palette.Fire).Transforms.Num(), SpellVfx::MaxPuffs);
	return bPass;
}

// T63. Cards face the eye, read top up (a flame licks upward), and a shell flickers through its cells over time.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaSpellVfxFlipbookCards, "MageArena.SpellVfx.FlipbookCardsFaceTheEye",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaSpellVfxFlipbookCards::RunTest(const FString& Parameters)
{
	bool bPass = true;
	const FVector Eye(0.0, 0.0, 120.0);
	TArray<SpellVfx::FPuff> Puffs;
	SpellVfx::BoltPuffs(SpellVfx::ETrail::Embers, FVector(350.0, 60.0, 130.0), FVector(-1.0, 0.0, 0.0), 18.0, 7, 3.0, Puffs);
	SpellVfx::BoltPuffs(SpellVfx::ETrail::Wisps, FVector(300.0, 0.0, 125.0), FVector(0.0, 1.0, 0.0), 18.0, 8, 3.0, Puffs);
	for (int32 Index = 0; Index < Puffs.Num(); ++Index)
	{
		const SpellVfx::FPuff& Puff = Puffs[Index];
		const FTransform Card = SpellVfx::CardTransform(Puff, Eye);
		const FVector Normal = Card.GetRotation().GetAxisZ();
		bPass &= TestTrue(*FString::Printf(TEXT("puff %d faces the eye"), Index), FVector::DotProduct(Normal, (Eye - Puff.CentreCm).GetSafeNormal()) > 0.999);
		bPass &= TestTrue(*FString::Printf(TEXT("puff %d sits at its centre"), Index), Card.GetLocation().Equals(Puff.CentreCm));
		bPass &= TestTrue(*FString::Printf(TEXT("puff %d is no smaller than its puff"), Index), Card.GetScale3D().Y * 100.0 >= Puff.SizeCm.Y - 1.0e-6);
	}
	// The Fire shell stands upright: the atlas's up (the card's -Y, V running down) points up the world.
	const FTransform Flame = SpellVfx::CardTransform(Puffs[0], Eye);
	bPass &= TestTrue(TEXT("the flame shell is a flame cell"), Puffs[0].Cell == SpellVfx::ECell::FireFlameA || Puffs[0].Cell == SpellVfx::ECell::FireFlameB);
	bPass &= TestTrue(TEXT("the flame licks upward"), -Flame.GetRotation().GetAxisY().Z > 0.99);
	// A wisp flying across the view lies along its flight; seen head-on, it shortens to a round card.
	const SpellVfx::FPuff* Wisp = Puffs.FindByPredicate([](const SpellVfx::FPuff& Puff)
	{ return Puff.Cell == SpellVfx::ECell::AirWisp; });
	if (TestNotNull(TEXT("the air trail shows wisps"), Wisp))
	{
		const FTransform Across = SpellVfx::CardTransform(*Wisp, Eye);
		bPass &= TestTrue(TEXT("a crossing wisp lies along its flight"), FMath::Abs(Across.GetRotation().GetAxisX().Y) > 0.9);
		bPass &= TestTrue(TEXT("a crossing wisp keeps its length"), Across.GetScale3D().X > 2.0 * Across.GetScale3D().Y);
		SpellVfx::FPuff HeadOn = *Wisp;
		HeadOn.Rotation = FRotationMatrix::MakeFromX((Eye - HeadOn.CentreCm).GetSafeNormal()).ToQuat();
		const FTransform Toward = SpellVfx::CardTransform(HeadOn, Eye);
		bPass &= TestTrue(TEXT("a wisp seen head-on is round"), FMath::IsNearlyEqual(Toward.GetScale3D().X, Toward.GetScale3D().Y, 0.02));
	}
	// A shell flickers: over a second it shows both of its school's cells, mirrored and not.
	TSet<int32> Looks;
	for (double Time = 0.0; Time < 1.0; Time += 0.05)
	{
		TArray<SpellVfx::FPuff> Shell;
		SpellVfx::BoltPuffs(SpellVfx::ETrail::Droplets, FVector(300.0, 0.0, 120.0), FVector(-1.0, 0.0, 0.0), 18.0, 4, Time, Shell);
		Looks.Add(static_cast<int32>(Shell[0].Cell) * 2 + (Shell[0].bMirror ? 1 : 0));
		bPass &= TestTrue(TEXT("a water shell shows a water shell cell"), Shell[0].Cell == SpellVfx::ECell::WaterShellA || Shell[0].Cell == SpellVfx::ECell::WaterShellB);
	}
	bPass &= TestEqual(TEXT("a shell takes four looks"), Looks.Num(), 4);
	// The custom data round-trips every cell, mirrored and not.
	for (int32 Index = 0; Index < SpellVfx::FlipbookGrid * SpellVfx::FlipbookGrid; ++Index)
	{
		for (const bool bMirror : {false, true})
		{
			SpellVfx::FPuff Puff;
			Puff.Cell = static_cast<SpellVfx::ECell>(Index);
			Puff.bMirror = bMirror;
			float Data[SpellVfx::CardCustomFloats];
			SpellVfx::CardCustomData(Puff, Data);
			bool bReadMirror = false;
			const SpellVfx::ECell Read = SpellVfx::CellOfCustomData(Data, bReadMirror);
			bPass &= TestTrue(*FString::Printf(TEXT("cell %d mirror %d round-trips"), Index, bMirror ? 1 : 0), Read == Puff.Cell && bReadMirror == bMirror);
		}
	}
	return bPass;
}

// T63. The atlas and the material as saved: Quest-sane (512 or less, mips on, linear), every cell's border clear (a
// card never shows its square), and M_SpellGlow samples it. The shade channel is a multiplier of at most 1, so a glow
// is never brighter than its own threat colour.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaSpellVfxFlipbookAsset, "MageArena.SpellVfx.FlipbookAtlasIsQuestSane",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaSpellVfxFlipbookAsset::RunTest(const FString& Parameters)
{
#if WITH_EDITORONLY_DATA
	const FString TexturePath = FString::Printf(TEXT("%s.%s"), SpellVfx::FlipbookTexturePackage, *FPackageName::GetShortName(SpellVfx::FlipbookTexturePackage));
	UTexture2D* Atlas = LoadObject<UTexture2D>(nullptr, *TexturePath);
	if (!TestNotNull(TEXT("T_SpellFlipbook loads (create-budget-fx.ps1 -SpellVfx)"), Atlas))
	{
		return false;
	}
	bool bPass = true;
	const int32 Size = Atlas->Source.GetSizeX();
	bPass &= TestTrue(*FString::Printf(TEXT("the atlas is square, a power of two, at most 512 (is %dx%d)"), Size, Atlas->Source.GetSizeY()),
	    Size == Atlas->Source.GetSizeY() && FMath::IsPowerOfTwo(Size) && Size <= 512);
	bPass &= TestFalse(TEXT("the atlas is a linear mask, not sRGB"), Atlas->SRGB);
	bPass &= TestNotEqual(TEXT("the atlas has mips"), static_cast<int32>(Atlas->MipGenSettings), static_cast<int32>(TMGS_NoMipmaps));
	TArray64<uint8> Mip;
	if (TestEqual(TEXT("the atlas source is BGRA8"), static_cast<int32>(Atlas->Source.GetFormat()), static_cast<int32>(TSF_BGRA8)) && Atlas->Source.GetMipData(Mip, 0))
	{
		const int32 Cell = Size / SpellVfx::FlipbookGrid;
		// The generator keeps content inside 0.9 of a cell: the outer 5% each side is clear.
		const int32 Band = FMath::FloorToInt32(Cell * 0.05f) - 1;
		int32 Worst = 0;
		for (int32 Y = 0; Y < Size; ++Y)
		{
			for (int32 X = 0; X < Size; ++X)
			{
				const int32 Cx = X % Cell;
				const int32 Cy = Y % Cell;
				if (Cx >= Band && Cx < Cell - Band && Cy >= Band && Cy < Cell - Band)
				{
					continue;
				}
				Worst = FMath::Max(Worst, static_cast<int32>(Mip[(static_cast<int64>(Y) * Size + X) * 4 + 3]));
			}
		}
		bPass &= TestEqual(TEXT("every cell's border is clear"), Worst, 0);
	}
	const FString MaterialPath = FString::Printf(TEXT("%s.%s"), SpellVfx::GlowMaterialPackage, *FPackageName::GetShortName(SpellVfx::GlowMaterialPackage));
	UMaterial* Material = LoadObject<UMaterial>(nullptr, *MaterialPath);
	if (TestNotNull(TEXT("M_SpellGlow loads"), Material))
	{
		bPass &= TestEqual(TEXT("M_SpellGlow is translucent"), static_cast<int32>(Material->BlendMode), static_cast<int32>(BLEND_Translucent));
		bPass &= TestTrue(TEXT("M_SpellGlow is unlit"), Material->GetShadingModels().HasOnlyShadingModel(MSM_Unlit));
		bPass &= TestTrue(TEXT("M_SpellGlow draws on instanced meshes"), Material->bUsedWithInstancedStaticMeshes);
		bool bSamples = false;
		for (const UMaterialExpression* Expression : Material->GetExpressions())
		{
			const UMaterialExpressionTextureSample* Sample = Cast<UMaterialExpressionTextureSample>(Expression);
			bSamples |= Sample && Sample->Texture == Atlas;
		}
		bPass &= TestTrue(TEXT("M_SpellGlow samples the atlas"), bSamples);
	}
	return bPass;
#else
	AddInfo(TEXT("no editor-only data: the atlas source is not readable here"));
	return true;
#endif
}

// T67. Fire reads with volume: the fire shell is a taller card than it is wide (flames lick upward), Water's and Air's stay
// as T63 drew them, and no bolt adds a puff. The shell stays centred on the core and nothing here is a colour.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaSpellVfxFireVolume, "MageArena.SpellVfx.FireShellHasVolume",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaSpellVfxFireVolume::RunTest(const FString& Parameters)
{
	bool bPass = true;
	constexpr double Core = 22.0;
	TArray<SpellVfx::FPuff> Fire;
	TArray<SpellVfx::FPuff> Water;
	TArray<SpellVfx::FPuff> Air;
	SpellVfx::BoltPuffs(SpellVfx::ETrail::Embers, FVector(100.0, 0.0, 120.0), FVector(-1.0, 0.0, 0.0), Core, 7, 1.25, Fire);
	SpellVfx::BoltPuffs(SpellVfx::ETrail::Droplets, FVector(100.0, 0.0, 120.0), FVector(-1.0, 0.0, 0.0), Core, 7, 1.25, Water);
	SpellVfx::BoltPuffs(SpellVfx::ETrail::Wisps, FVector(100.0, 0.0, 120.0), FVector(-1.0, 0.0, 0.0), Core, 7, 1.25, Air);
	if (!TestTrue(TEXT("each bolt has a shell"), Fire.Num() > 0 && Water.Num() > 0 && Air.Num() > 0))
	{
		return false;
	}
	bPass &= TestTrue(*FString::Printf(TEXT("the fire shell is taller than wide (%.1f x %.1f cm)"), Fire[0].SizeCm.X, Fire[0].SizeCm.Y), Fire[0].SizeCm.Y > Fire[0].SizeCm.X * 1.2);
	bPass &= TestTrue(TEXT("the fire shell covers more than the water shell"), Fire[0].SizeCm.X * Fire[0].SizeCm.Y > Water[0].SizeCm.X * Water[0].SizeCm.Y);
	bPass &= TestEqual(TEXT("the water shell stays square"), Water[0].SizeCm.X, Water[0].SizeCm.Y);
	bPass &= TestEqual(TEXT("the air shell stays square"), Air[0].SizeCm.X, Air[0].SizeCm.Y);
	bPass &= TestTrue(TEXT("the fire shell stays centred on the core"), Fire[0].CentreCm.Equals(FVector(100.0, 0.0, 120.0)));
	bPass &= TestEqual(TEXT("the fire bolt keeps T63's nine puffs (a shell and eight embers)"), Fire.Num(), 9);
	TArray<SpellVfx::FPuff> FireRing;
	TArray<SpellVfx::FPuff> WaterRing;
	SpellVfx::PerfectRingPuffs(FVector::ZeroVector, FVector::BackwardVector, 0.3, FireRing, SpellVfx::ETrail::Embers);
	SpellVfx::PerfectRingPuffs(FVector::ZeroVector, FVector::BackwardVector, 0.3, WaterRing, SpellVfx::ETrail::Droplets);
	if (TestTrue(TEXT("both rings have beads"), FireRing.Num() > 0 && FireRing.Num() == WaterRing.Num()))
	{
		bPass &= TestTrue(TEXT("a fire ring bead is a larger card than a water one"), FireRing[0].SizeCm.X > WaterRing[0].SizeCm.X * 1.2);
	}
	return bPass;
}

// T67. The fire cells have a dark body and a hot heart: the outline and the outer flame are darker tones of the fire
// colour (contrast against the orange sand) and the heart is the full element colour (shade 1, never brighter).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaSpellVfxFireContrast, "MageArena.SpellVfx.FireCellsHaveHotCoreAndDarkBody",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaSpellVfxFireContrast::RunTest(const FString& Parameters)
{
#if WITH_EDITORONLY_DATA
	const FString TexturePath = FString::Printf(TEXT("%s.%s"), SpellVfx::FlipbookTexturePackage, *FPackageName::GetShortName(SpellVfx::FlipbookTexturePackage));
	UTexture2D* Atlas = LoadObject<UTexture2D>(nullptr, *TexturePath);
	if (!TestNotNull(TEXT("T_SpellFlipbook loads (create-budget-fx.ps1 -SpellVfx)"), Atlas))
	{
		return false;
	}
	TArray64<uint8> Mip;
	if (!TestTrue(TEXT("the atlas source is readable"), Atlas->Source.GetFormat() == TSF_BGRA8 && Atlas->Source.GetMipData(Mip, 0)))
	{
		return false;
	}
	const int32 Size = Atlas->Source.GetSizeX();
	const int32 Cell = Size / SpellVfx::FlipbookGrid;
	bool bPass = true;
	for (const SpellVfx::ECell Which : {SpellVfx::ECell::FireFlameA, SpellVfx::ECell::FireFlameB, SpellVfx::ECell::FireCrown})
	{
		const int32 Index = static_cast<int32>(Which);
		int32 Opaque = 0;
		int32 Dark = 0;
		int32 Hot = 0;
		for (int32 Y = 0; Y < Cell; ++Y)
		{
			for (int32 X = 0; X < Cell; ++X)
			{
				const int64 At = (static_cast<int64>((Index / SpellVfx::FlipbookGrid) * Cell + Y) * Size + (Index % SpellVfx::FlipbookGrid) * Cell + X) * 4;
				if (Mip[At + 3] < 200)
				{
					continue;
				}
				++Opaque;
				Dark += Mip[At] < 179 ? 1 : 0;
				Hot += Mip[At] > 235 ? 1 : 0;
			}
		}
		const float DarkShare = Opaque > 0 ? static_cast<float>(Dark) / Opaque : 0.f;
		const float HotShare = Opaque > 0 ? static_cast<float>(Hot) / Opaque : 0.f;
		bPass &= TestTrue(*FString::Printf(TEXT("fire cell %d: at least half the flame is a darker tone (%.2f)"), Index, DarkShare), DarkShare >= 0.5f);
		bPass &= TestTrue(*FString::Printf(TEXT("fire cell %d: a hot heart of full colour (%.2f)"), Index, HotShare), HotShare >= 0.08f);
	}
	return bPass;
#else
	AddInfo(TEXT("no editor-only data: the atlas source is not readable here"));
	return true;
#endif
}

// T72. The edge bands of a fire cell (an atlas cell's opaque pixels, A >= 200, by their distance in pixels to the nearest
// clear one, A < 128, the cell's outside counting as clear): the mean shade of the band 1-3 px in and of the band 4-6 px in.
namespace
{
struct FFireBands
{
	float Edge = 0.f;
	float Inner = 0.f;
	int32 EdgeCount = 0;
	int32 InnerCount = 0;
};

FFireBands FireEdgeBands(const TArray64<uint8>& Mip, int32 Size, SpellVfx::ECell Which)
{
	const int32 Cell = Size / SpellVfx::FlipbookGrid;
	const int32 Index = static_cast<int32>(Which);
	const int32 X0 = (Index % SpellVfx::FlipbookGrid) * Cell;
	const int32 Y0 = (Index / SpellVfx::FlipbookGrid) * Cell;
	auto Alpha = [&](int32 X, int32 Y) -> int32
	{
		if (X < 0 || Y < 0 || X >= Cell || Y >= Cell)
		{
			return 0;
		}
		return Mip[(static_cast<int64>(Y0 + Y) * Size + X0 + X) * 4 + 3];
	};
	double EdgeSum = 0.0;
	double InnerSum = 0.0;
	FFireBands Bands;
	for (int32 Y = 0; Y < Cell; ++Y)
	{
		for (int32 X = 0; X < Cell; ++X)
		{
			if (Alpha(X, Y) < 200)
			{
				continue;
			}
			// Chebyshev distance to the nearest clear pixel, up to 7.
			int32 Distance = 7;
			for (int32 Ring = 1; Ring < 7 && Distance == 7; ++Ring)
			{
				for (int32 J = -Ring; J <= Ring && Distance == 7; ++J)
				{
					for (int32 I = -Ring; I <= Ring; ++I)
					{
						if ((FMath::Abs(I) == Ring || FMath::Abs(J) == Ring) && Alpha(X + I, Y + J) < 128)
						{
							Distance = Ring;
							break;
						}
					}
				}
			}
			const double Shade = Mip[(static_cast<int64>(Y0 + Y) * Size + X0 + X) * 4 + 2] / 255.0;
			if (Distance <= 3)
			{
				EdgeSum += Shade;
				++Bands.EdgeCount;
			}
			else if (Distance <= 6)
			{
				InnerSum += Shade;
				++Bands.InnerCount;
			}
		}
	}
	Bands.Edge = Bands.EdgeCount > 0 ? static_cast<float>(EdgeSum / Bands.EdgeCount) : 1.f;
	Bands.Inner = Bands.InnerCount > 0 ? static_cast<float>(InnerSum / Bands.InnerCount) : 1.f;
	return Bands;
}
}

// T72b. Each flame lick keeps a crisp tip: no dark backing caps the licks as a filled halo. Measured per column of flame A
// and B as the run from the topmost opaque pixel (A >= 200) down to the first lit one (shade >= 0.40): the dark cap over
// the licks. T67 has none (0 px), T72's even 0.1 growth capped them by 11.3 / 8.6 px; the bound is about half of that.
// T72b's narrow backing (3.1 / 3.2 px) did not pass Athena's critic bar, and the Fire cells went back to T67's.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaSpellVfxFireTips, "MageArena.SpellVfx.FireLickTipsStayCrisp",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaSpellVfxFireTips::RunTest(const FString& Parameters)
{
#if WITH_EDITORONLY_DATA
	const FString TexturePath = FString::Printf(TEXT("%s.%s"), SpellVfx::FlipbookTexturePackage, *FPackageName::GetShortName(SpellVfx::FlipbookTexturePackage));
	UTexture2D* Atlas = LoadObject<UTexture2D>(nullptr, *TexturePath);
	if (!TestNotNull(TEXT("T_SpellFlipbook loads (create-budget-fx.ps1 -SpellVfx)"), Atlas))
	{
		return false;
	}
	TArray64<uint8> Mip;
	if (!TestTrue(TEXT("the atlas source is readable"), Atlas->Source.GetFormat() == TSF_BGRA8 && Atlas->Source.GetMipData(Mip, 0)))
	{
		return false;
	}
	constexpr float CapMost = 4.5f;
	constexpr uint8 Lit = 102;
	const int32 Size = Atlas->Source.GetSizeX();
	const int32 Cell = Size / SpellVfx::FlipbookGrid;
	bool bPass = true;
	for (const SpellVfx::ECell Which : {SpellVfx::ECell::FireFlameA, SpellVfx::ECell::FireFlameB})
	{
		const int32 Index = static_cast<int32>(Which);
		const int32 X0 = (Index % SpellVfx::FlipbookGrid) * Cell;
		const int32 Y0 = (Index / SpellVfx::FlipbookGrid) * Cell;
		int32 CapSum = 0;
		int32 Columns = 0;
		for (int32 X = 0; X < Cell; ++X)
		{
			int32 Top = -1;
			for (int32 Y = 0; Y < Cell; ++Y)
			{
				const int64 At = (static_cast<int64>(Y0 + Y) * Size + X0 + X) * 4;
				if (Mip[At + 3] < 200)
				{
					continue;
				}
				Top = Top < 0 ? Y : Top;
				if (Mip[At + 2] >= Lit)
				{
					CapSum += Y - Top;
					++Columns;
					break;
				}
			}
		}
		const float Cap = Columns > 0 ? static_cast<float>(CapSum) / Columns : 0.f;
		bPass &= TestTrue(*FString::Printf(TEXT("fire cell %d: the dark cap over the lick tips is thin (mean %.2f px over %d columns, at most %.1f)"), Index, Cap, Columns, CapMost),
		    Columns > 0 && Cap <= CapMost);
	}
	return bPass;
#else
	AddInfo(TEXT("no editor-only data: the atlas source is not readable here"));
	return true;
#endif
}

// T72b. The spark streak (the perfect ring's bead) is T67's cell exactly (Athena, 2026-10-26: T72's darker tail and spark
// rim read as rope). FNV-1a 32 over each pixel's shade and alpha, row-major; the cell is a grey mask, so the source's BGRA
// order reads the same bytes as the PNG. 0xd64783f6 is T67's cell (spell-flipbook.mjs at 6cbe7d9); T72's was 0x6e768181.
// Its 1-3 px edge band stays T67's light outline (0.567), not a dark rim.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaSpellVfxStreakT67, "MageArena.SpellVfx.SparkStreakIsT67sCell",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaSpellVfxStreakT67::RunTest(const FString& Parameters)
{
#if WITH_EDITORONLY_DATA
	const FString TexturePath = FString::Printf(TEXT("%s.%s"), SpellVfx::FlipbookTexturePackage, *FPackageName::GetShortName(SpellVfx::FlipbookTexturePackage));
	UTexture2D* Atlas = LoadObject<UTexture2D>(nullptr, *TexturePath);
	if (!TestNotNull(TEXT("T_SpellFlipbook loads (create-budget-fx.ps1 -SpellVfx)"), Atlas))
	{
		return false;
	}
	TArray64<uint8> Mip;
	if (!TestTrue(TEXT("the atlas source is readable"), Atlas->Source.GetFormat() == TSF_BGRA8 && Atlas->Source.GetMipData(Mip, 0)))
	{
		return false;
	}
	constexpr uint32 T67Streak = 0xd64783f6u;
	const int32 Size = Atlas->Source.GetSizeX();
	const int32 Cell = Size / SpellVfx::FlipbookGrid;
	const int32 Index = static_cast<int32>(SpellVfx::ECell::SparkStreak);
	const int32 X0 = (Index % SpellVfx::FlipbookGrid) * Cell;
	const int32 Y0 = (Index / SpellVfx::FlipbookGrid) * Cell;
	uint32 Hash = 0x811c9dc5u;
	for (int32 Y = 0; Y < Cell; ++Y)
	{
		for (int32 X = 0; X < Cell; ++X)
		{
			const int64 At = (static_cast<int64>(Y0 + Y) * Size + X0 + X) * 4;
			for (const uint8 Byte : {Mip[At + 2], Mip[At + 3]})
			{
				Hash = (Hash ^ Byte) * 0x01000193u;
			}
		}
	}
	bool bPass = TestTrue(*FString::Printf(TEXT("the spark streak is T67's cell (FNV-1a 0x%08x, T67 0x%08x)"), Hash, T67Streak), Hash == T67Streak);
	const FFireBands Bands = FireEdgeBands(Mip, Size, SpellVfx::ECell::SparkStreak);
	bPass &= TestTrue(*FString::Printf(TEXT("the spark streak keeps T67's light outline (1-3 px band %.3f, at least 0.50)"), Bands.Edge), Bands.EdgeCount > 0 && Bands.Edge >= 0.5f);
	return bPass;
#else
	AddInfo(TEXT("no editor-only data: the atlas source is not readable here"));
	return true;
#endif
}

// T72. The fire cells add no other colour and no white core: every fire cell is a grey mask (R = G = B, so M_SpellGlow draws
// the element colour times a shade of at most 1, never a whiter or yellower hue) and its heart is the full element colour.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaSpellVfxFireNoWhite, "MageArena.SpellVfx.FireShadeNeverAboveOne",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaSpellVfxFireNoWhite::RunTest(const FString& Parameters)
{
#if WITH_EDITORONLY_DATA
	const FString TexturePath = FString::Printf(TEXT("%s.%s"), SpellVfx::FlipbookTexturePackage, *FPackageName::GetShortName(SpellVfx::FlipbookTexturePackage));
	UTexture2D* Atlas = LoadObject<UTexture2D>(nullptr, *TexturePath);
	if (!TestNotNull(TEXT("T_SpellFlipbook loads (create-budget-fx.ps1 -SpellVfx)"), Atlas))
	{
		return false;
	}
	TArray64<uint8> Mip;
	if (!TestTrue(TEXT("the atlas source is readable"), Atlas->Source.GetFormat() == TSF_BGRA8 && Atlas->Source.GetMipData(Mip, 0)))
	{
		return false;
	}
	const int32 Size = Atlas->Source.GetSizeX();
	const int32 Cell = Size / SpellVfx::FlipbookGrid;
	bool bPass = true;
	for (const SpellVfx::ECell Which : {SpellVfx::ECell::FireFlameA, SpellVfx::ECell::FireFlameB, SpellVfx::ECell::FireEmbers, SpellVfx::ECell::FireCrown, SpellVfx::ECell::SparkStreak})
	{
		const int32 Index = static_cast<int32>(Which);
		int32 Tinted = 0;
		int32 Heart = 0;
		for (int32 Y = 0; Y < Cell; ++Y)
		{
			for (int32 X = 0; X < Cell; ++X)
			{
				const int64 At = (static_cast<int64>((Index / SpellVfx::FlipbookGrid) * Cell + Y) * Size + (Index % SpellVfx::FlipbookGrid) * Cell + X) * 4;
				Tinted += (Mip[At] != Mip[At + 1] || Mip[At] != Mip[At + 2]) ? 1 : 0;
				if (Mip[At + 3] >= 200)
				{
					Heart = FMath::Max(Heart, static_cast<int32>(Mip[At + 2]));
				}
			}
		}
		bPass &= TestEqual(*FString::Printf(TEXT("fire cell %d: a grey mask, R = G = B (pixels off: %d)"), Index, Tinted), Tinted, 0);
		bPass &= TestTrue(*FString::Printf(TEXT("fire cell %d: the heart is the full element colour (shade %d of 255)"), Index, Heart), Heart >= 250);
	}
	return bPass;
#else
	AddInfo(TEXT("no editor-only data: the atlas source is not readable here"));
	return true;
#endif
}

namespace
{
// T73: a style palette as a run applies it (apps/vr/data/vr/styles/style-<id>.json over the greybox one).
TOptional<FElementPalette> T73StylePalette(const TCHAR* Id)
{
	FString Path = FPaths::Combine(FPaths::ProjectDir(), FString::Printf(TEXT("../data/vr/styles/style-%s.json"), Id));
	FPaths::CollapseRelativeDirectories(Path);
	FString Json;
	TSharedPtr<FJsonObject> Style;
	if (!FFileHelper::LoadFileToString(Json, *Path) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Style) || !Style.IsValid())
	{
		return {};
	}
	FElementPalette Palette = FElementPalette::Greybox();
	ApplyStylePalette(Palette, *Style);
	return Palette;
}

float T73ChromaDistance(const FLinearColor& A, const FLinearColor& B)
{
	const FLinearColor CA = Chroma(A);
	const FLinearColor CB = Chroma(B);
	return FMath::Sqrt(FMath::Square(CA.R - CB.R) + FMath::Square(CA.G - CB.G) + FMath::Square(CA.B - CB.B));
}

// The hue the eye sees, in degrees: of the sRGB-encoded colour, as a picker reads it.
float T73SrgbHue(const FLinearColor& Colour)
{
	const FColor C = Chroma(Colour).ToFColorSRGB();
	const float R = C.R / 255.f;
	const float G = C.G / 255.f;
	const float B = C.B / 255.f;
	const float Most = FMath::Max3(R, G, B);
	const float Span = Most - FMath::Min3(R, G, B);
	if (Span < 1.0e-4f)
	{
		return 0.f;
	}
	const float Hue = Most == R ? FMath::Fmod((G - B) / Span + 6.f, 6.f) : (Most == G ? (B - R) / Span + 2.f : (R - G) / Span + 4.f);
	return Hue * 60.f;
}

float T73Luminance(const FLinearColor& Colour)
{
	return 0.2126f * Colour.R + 0.7152f * Colour.G + 0.0722f * Colour.B;
}

TArray<TPair<FString, FElementPalette>> T73Palettes(FAutomationTestBase& Test)
{
	TArray<TPair<FString, FElementPalette>> Out;
	Out.Emplace(TEXT("greybox"), FElementPalette::Greybox());
	for (const TCHAR* Id : {TEXT("13"), TEXT("25"), TEXT("30")})
	{
		const TOptional<FElementPalette> Palette = T73StylePalette(Id);
		if (Test.TestTrue(FString::Printf(TEXT("style %s loads"), Id), Palette.IsSet()))
		{
			Out.Emplace(FString::Printf(TEXT("style %s"), Id), Palette.GetValue());
		}
	}
	return Out;
}
}

// T73 (owner ruling 3, 2026-10-27). Fire's second rim hue is a material rim term, not a 4th custom float: M_SpellGlow carries
// RimColor and RimMix (0 by default, so every tint the presentation does not set draws as before), the shade band, and the
// lerp from Color to RimColor that feeds the emissive. A card keeps three custom floats.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaSpellVfxRimTerm, "MageArena.SpellVfx.GlowMaterialHasRimTerm",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaSpellVfxRimTerm::RunTest(const FString& Parameters)
{
	const FString MaterialPath = FString::Printf(TEXT("%s.%s"), SpellVfx::GlowMaterialPackage, *FPackageName::GetShortName(SpellVfx::GlowMaterialPackage));
	UMaterial* Material = LoadObject<UMaterial>(nullptr, *MaterialPath);
	if (!TestNotNull(TEXT("M_SpellGlow loads (create-budget-fx.ps1 -SpellVfx)"), Material))
	{
		return false;
	}
	bool bPass = TestEqual(TEXT("a card still carries three custom floats"), SpellVfx::CardCustomFloats, 3);
#if WITH_EDITORONLY_DATA
	const UMaterialExpressionVectorParameter* Colour = nullptr;
	const UMaterialExpressionVectorParameter* Rim = nullptr;
	TMap<FName, float> Scalars;
	for (const UMaterialExpression* Expression : Material->GetExpressions())
	{
		if (const UMaterialExpressionVectorParameter* Vector = Cast<UMaterialExpressionVectorParameter>(Expression))
		{
			if (Vector->ParameterName == TEXT("Color"))
			{
				Colour = Vector;
			}
			else if (Vector->ParameterName == TEXT("RimColor"))
			{
				Rim = Vector;
			}
		}
		if (const UMaterialExpressionScalarParameter* Scalar = Cast<UMaterialExpressionScalarParameter>(Expression))
		{
			Scalars.Add(Scalar->ParameterName, Scalar->DefaultValue);
		}
	}
	bPass &= TestNotNull(TEXT("M_SpellGlow has the RimColor parameter"), Rim);
	const float* Mix = Scalars.Find(TEXT("RimMix"));
	bPass &= TestTrue(*FString::Printf(TEXT("M_SpellGlow has RimMix, 0 by default (%s)"), Mix ? *FString::SanitizeFloat(*Mix) : TEXT("missing")), Mix && *Mix == 0.f);
	const float* Top = Scalars.Find(TEXT("RimShadeTop"));
	const float* Bottom = Scalars.Find(TEXT("RimShadeBottom"));
	bPass &= TestTrue(TEXT("M_SpellGlow has the rim's shade band, FireRimShadeTop and FireRimShadeBottom by default"),
	    Top && Bottom && *Top == SpellVfx::FireRimShadeTop && *Bottom == SpellVfx::FireRimShadeBottom);
	bool bLerp = false;
	for (const UMaterialExpression* Expression : Material->GetExpressions())
	{
		const UMaterialExpressionLinearInterpolate* Lerp = Cast<UMaterialExpressionLinearInterpolate>(Expression);
		bLerp |= Lerp && Colour && Rim && Lerp->A.Expression == Colour && Lerp->B.Expression == Rim;
	}
	bPass &= TestTrue(TEXT("the emissive lerps from Color to RimColor"), bLerp);
#endif
	return bPass;
}

// T73. The rim hue sits in its band in every palette a run may draw with: a second hue (not Fire's own chroma), deeper toward
// red but inside Fire's red-orange family (at most half way to the leave red), never within BrazierFire::ThreatChromaMargin of
// the unblockable's rim red or of another threat colour, never brighter than Fire in any channel, and darker in luminance at
// the rim's shade, which is the edge contrast it buys. The body and the heart keep Fire's colour.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaSpellVfxFireRimBand, "MageArena.SpellVfx.FireRimHueInItsBand",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaSpellVfxFireRimBand::RunTest(const FString& Parameters)
{
	bool bPass = true;
	for (const TPair<FString, FElementPalette>& Case : T73Palettes(*this))
	{
		const FElementPalette& Palette = Case.Value;
		const FString& Name = Case.Key;
		const FLinearColor Fire = Palette.Fire;
		const FLinearColor Rim = SpellVfx::FireRimColour(Palette);
		const float FromFire = T73ChromaDistance(Rim, Fire);
		const float FromLeave = T73ChromaDistance(Rim, Palette.UnblockableRim);
		const float FireToLeave = T73ChromaDistance(Fire, Palette.UnblockableRim);
		bPass &= TestTrue(*FString::Printf(TEXT("%s: the rim is a second hue, not Fire's chroma (%.3f from it, at least 0.08)"), *Name, FromFire), FromFire >= 0.08f);
		bPass &= TestTrue(*FString::Printf(TEXT("%s: the rim stays in Fire's family, at most half way to the leave red (%.3f of %.3f)"), *Name, FromFire, FireToLeave),
		    FromFire <= FireToLeave * SpellVfx::FireRimMostShift + 1.0e-4f);
		bPass &= TestTrue(*FString::Printf(TEXT("%s: the rim stands off the unblockable's rim red by chroma (%.3f)"), *Name, FromLeave), FromLeave >= BrazierFire::ThreatChromaMargin);
		const float RimHue = T73SrgbHue(Rim);
		const float FireHue = T73SrgbHue(Fire);
		const float LeaveHue = T73SrgbHue(Palette.UnblockableRim);
		bPass &= TestTrue(*FString::Printf(TEXT("%s: the rim hue (%.1f deg) is a deeper red-orange than Fire (%.1f) and well off the leave red (%.1f)"), *Name, RimHue, FireHue, LeaveHue),
		    RimHue < FireHue - 3.f && RimHue > LeaveHue + 10.f);
		const TPair<const TCHAR*, FLinearColor> Others[] = {
		    {TEXT("water"), Palette.Water},
		    {TEXT("air"), Palette.Air},
		    {TEXT("earth"), Palette.Earth},
		    {TEXT("steel"), Palette.Steel},
		};
		for (const TPair<const TCHAR*, FLinearColor>& Other : Others)
		{
			const float Distance = T73ChromaDistance(Rim, Other.Value);
			bPass &= TestTrue(*FString::Printf(TEXT("%s: the rim stands off %s (%.3f)"), *Name, Other.Key, Distance), Distance >= BrazierFire::ThreatChromaMargin);
		}
		bPass &= TestTrue(*FString::Printf(TEXT("%s: the rim is never brighter than Fire in any channel (%s vs %s)"), *Name, *Rim.ToString(), *Fire.ToString()),
		    Rim.R <= Fire.R + 1.0e-6f && Rim.G <= Fire.G + 1.0e-6f && Rim.B <= Fire.B + 1.0e-6f && FMath::IsNearlyEqual(Rim.GetMax(), Fire.GetMax(), 1.0e-5f));
		const FLinearColor Edge = SpellVfx::GlowEmissive(Fire, Rim, 1.f, SpellVfx::FireRimShadeBottom);
		bPass &= TestTrue(*FString::Printf(TEXT("%s: at the rim's shade the edge is the rim colour"), *Name), Edge.Equals(Rim * SpellVfx::FireRimShadeBottom, 1.0e-5f));
		const float EdgeLuminance = T73Luminance(Edge);
		const float T67Luminance = T73Luminance(Fire * SpellVfx::FireRimShadeBottom);
		bPass &= TestTrue(*FString::Printf(TEXT("%s: the edge is darker by hue at T67's shade (luminance %.4f vs T67 %.4f, at most 0.92 of it)"), *Name, EdgeLuminance, T67Luminance),
		    EdgeLuminance <= T67Luminance * 0.92f);
		for (const float Shade : {SpellVfx::FireRimShadeTop, 0.68f, 1.f})
		{
			const FLinearColor Body = SpellVfx::GlowEmissive(Fire, Rim, 1.f, Shade);
			bPass &= TestTrue(*FString::Printf(TEXT("%s: at shade %.2f the body keeps Fire's colour"), *Name, Shade), Body.Equals(Fire * Shade, 1.0e-5f));
		}
		const TOptional<FLinearColor> Drawn = SpellVfx::GlowRimFor(Palette, Fire);
		bPass &= TestTrue(*FString::Printf(TEXT("%s: Fire's layer draws the rim"), *Name), Drawn.IsSet() && Drawn.GetValue().Equals(Rim));
	}
	return bPass;
}

// T73. Every other layer draws exactly as before: no colour but Fire's gets a rim (Water, Air, Earth, steel, the unblockable's
// red and black, its pulse, the braziers' flame), and with RimMix 0 the material's term is Color x shade to the bit, at every
// shade the 8-bit atlas holds, whatever RimColor is.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaSpellVfxRimOthers, "MageArena.SpellVfx.GlowRimLeavesOtherLayersUnchanged",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaSpellVfxRimOthers::RunTest(const FString& Parameters)
{
	bool bPass = true;
	TArray<TPair<FString, FElementPalette>> Cases = T73Palettes(*this);
	for (const FElementPalette& Moved : Palettes())
	{
		Cases.Emplace(TEXT("moved"), Moved);
	}
	for (const TPair<FString, FElementPalette>& Case : Cases)
	{
		const FElementPalette& Palette = Case.Value;
		const FLinearColor Rim = SpellVfx::FireRimColour(Palette);
		const TPair<const TCHAR*, FLinearColor> Others[] = {
		    {TEXT("water"), Palette.Water},
		    {TEXT("air"), Palette.Air},
		    {TEXT("earth"), Palette.Earth},
		    {TEXT("steel"), Palette.Steel},
		    {TEXT("unblockable rim"), Palette.UnblockableRim},
		    {TEXT("unblockable body"), Palette.UnblockableBody},
		    {TEXT("rim pulse"), SpellVfx::RimPulseColour(Palette.UnblockableRim, 1.f)},
		    {TEXT("brazier body"), BrazierFire::FlameColour(Palette, BrazierFire::ELayer::Body)},
		    {TEXT("brazier heart"), BrazierFire::FlameColour(Palette, BrazierFire::ELayer::Heart)},
		};
		for (const TPair<const TCHAR*, FLinearColor>& Other : Others)
		{
			bPass &= TestFalse(*FString::Printf(TEXT("%s: the %s layer draws no rim"), *Case.Key, Other.Key), SpellVfx::GlowRimFor(Palette, Other.Value).IsSet());
			int32 Off = 0;
			for (int32 Level = 0; Level <= 255; ++Level)
			{
				const float Shade = Level / 255.f;
				const FLinearColor Drawn = SpellVfx::GlowEmissive(Other.Value, Rim, 0.f, Shade);
				Off += (Drawn.R != Other.Value.R * Shade || Drawn.G != Other.Value.G * Shade || Drawn.B != Other.Value.B * Shade) ? 1 : 0;
			}
			bPass &= TestEqual(*FString::Printf(TEXT("%s: with RimMix 0 the %s layer is Color x shade to the bit (levels off)"), *Case.Key, Other.Key), Off, 0);
		}
	}
	return bPass;
}

#endif
