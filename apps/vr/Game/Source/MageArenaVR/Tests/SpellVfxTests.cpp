#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Kernel/ArenaKernel.h"
#include "Kernel/Enemies.h"
#include "Kernel/KernelData.h"
#include "Kernel/SimTypes.h"
#include "Engine/Texture2D.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionTextureSample.h"
#include "Misc/PackageName.h"
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

// T72. Fire separates from the orange sand by value: every fire flame, flame star and spark streak has a dark rim at its
// silhouette, a deep ember tone of the fire colour (T67's outline was a hair-wide band at 0.47-0.59). The flames and the
// star keep it wider: the band 4-6 px in is dark too (T67: 0.52-0.69). The spark streak's bound is looser (0.40, T67 0.57):
// a darker streak made the perfect ring of them read as a rope in the blind critic. The rim stays a tone of the fire
// colour, not black, so it never reads as the unblockable's black core.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaSpellVfxFireDarkRim, "MageArena.SpellVfx.FireCellsHaveDarkRimAtSilhouette",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaSpellVfxFireDarkRim::RunTest(const FString& Parameters)
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
	constexpr float EdgeMost = 0.34f;
	constexpr float StreakEdgeMost = 0.40f;
	constexpr float InnerMost = 0.45f;
	constexpr float EdgeLeast = 0.15f;
	bool bPass = true;
	for (const SpellVfx::ECell Which : {SpellVfx::ECell::FireFlameA, SpellVfx::ECell::FireFlameB, SpellVfx::ECell::FireCrown, SpellVfx::ECell::SparkStreak})
	{
		const int32 Index = static_cast<int32>(Which);
		const FFireBands Bands = FireEdgeBands(Mip, Atlas->Source.GetSizeX(), Which);
		const float Most = Which == SpellVfx::ECell::SparkStreak ? StreakEdgeMost : EdgeMost;
		bPass &= TestTrue(*FString::Printf(TEXT("fire cell %d: the 1-3 px edge band is dark (mean shade %.3f, at most %.2f)"), Index, Bands.Edge, Most),
		    Bands.EdgeCount > 0 && Bands.Edge <= Most);
		bPass &= TestTrue(*FString::Printf(TEXT("fire cell %d: the edge band is an ember tone, not black (mean shade %.3f, at least %.2f)"), Index, Bands.Edge, EdgeLeast),
		    Bands.Edge >= EdgeLeast);
		if (Which != SpellVfx::ECell::SparkStreak)
		{
			bPass &= TestTrue(*FString::Printf(TEXT("fire cell %d: the dark band is wide, 4-6 px in (mean shade %.3f, at most %.2f)"), Index, Bands.Inner, InnerMost),
			    Bands.InnerCount > 0 && Bands.Inner <= InnerMost);
		}
	}
	return bPass;
#else
	AddInfo(TEXT("no editor-only data: the atlas source is not readable here"));
	return true;
#endif
}

// T72. The dark rim adds no other colour and no white core: every fire cell is a grey mask (R = G = B, so M_SpellGlow draws
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

#endif
