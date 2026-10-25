#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Kernel/ArenaKernel.h"
#include "Kernel/Enemies.h"
#include "Kernel/KernelData.h"
#include "Kernel/SimTypes.h"
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

#endif
