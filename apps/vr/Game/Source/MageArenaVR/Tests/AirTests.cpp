#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Kernel/Air.h"
#include "Kernel/ArenaKernel.h"
#include "Kernel/Catalog.h"
#include "Kernel/Fire.h"
#include "Kernel/Games.h"
#include "Kernel/KernelData.h"
#include "Kernel/MageAI.h"
#include "Kernel/VrRules.h"
#include "MageArenaVR.h"

#include "Misc/FileHelper.h"

#include <cmath>

// T23, the Air school. Every expected number below is a hand literal with its source:
// - apps/vr/data/vr/schools/spells-air.csv (the VR proposal sheet, docs/schools/AIR.md): one row per FRow below.
// - pinned schools.json air.combatIdentity: Momentum 0-100, +20/s moving above 4 m/s, -25/s still, at 40 spellsPierce 1
//   and rollStaminaMult 0.6, at 80 spellRangeMult 1.3, absorbDrainMult 1.2, perfect absorb deflects tier <= own tier.
// - pinned combat.json: simStepHz 60, walkMps 4.5, roll staminaCost 25. stats.csv rank 1: HP 95, mana 95.
// - pinned runtime.json geometry: mageRadiusM 0.38, projectileRadiusM 0.12.
// Ticks are ceil(seconds x 60): 0.12 s -> 8, 0.25 s -> 15, 0.30 s -> 18, 0.50 s -> 30, 0.70 s -> 42, 1.20 s -> 72.

namespace
{
constexpr double MomentumMax = 100.0;
constexpr double GainPerSecond = 20.0;
constexpr double MovingAboveMps = 4.0;
constexpr double DecayPerSecond = 25.0;
constexpr double PierceAt = 40.0;
constexpr double RollMult = 0.6;
constexpr double RangeAt = 80.0;
constexpr double RangeMult = 1.3;
constexpr double AirDrainMult = 1.2;
constexpr double EyeDrainMult = 1.5;
constexpr double WalkMps = 4.5;
constexpr double RollStamina = 25.0;
constexpr double Hz = 60.0;

struct FRow
{
	const TCHAR* Id;
	const TCHAR* Kind;
	int32 Tier;
	double CastS;
	double CooldownS;
	double Mana;
	double Damage;
	const TCHAR* Family;
	double TelegraphS;
	double RangeM;
	EAirMomentumMode Mode;
	double Momentum;
	// Hand-counted windup and cooldown ticks at 60 Hz.
	int32 WindupTicks;
	int32 CooldownTicks;
};

const FRow Rows[] = {
	{TEXT("air_bolt"), TEXT("projectile"), 0, 0.00, 0.30, 4, 7, TEXT("magic"), 0.00, 14, EAirMomentumMode::Hit, 2, 0, 18},
	{TEXT("air_shear"), TEXT("twin"), 1, 0.12, 3.0, 12, 9, TEXT("magic"), 0.12, 12, EAirMomentumMode::Hit, 4, 8, 180},
	{TEXT("air_slipstream"), TEXT("dash"), 1, 0.00, 5.0, 12, 0, TEXT("n/a"), 0.00, 6, EAirMomentumMode::Instant, 30, 0, 300},
	{TEXT("air_veer"), TEXT("veer"), 2, 0.45, 7.0, 22, 24, TEXT("magic"), 0.50, 14, EAirMomentumMode::Hit, 6, 30, 420},
	{TEXT("air_downdraft"), TEXT("zone"), 2, 0.40, 9.0, 20, 26, TEXT("magic"), 0.70, 12, EAirMomentumMode::Target, 5, 42, 540},
	{TEXT("air_form"), TEXT("form"), 2, 0.15, 14.0, 10, 0, TEXT("n/a"), 0.00, 0, EAirMomentumMode::Spend, 40, 9, 840},
	{TEXT("air_squall"), TEXT("squall"), 3, 0.30, 11.0, 34, 11, TEXT("magic"), 0.30, 12, EAirMomentumMode::Hit, 3, 18, 660},
	{TEXT("air_eye"), TEXT("self"), 3, 0.40, 16.0, 28, 0, TEXT("n/a"), 0.00, 0, EAirMomentumMode::Lock, 0, 24, 960},
	{TEXT("air_tempest"), TEXT("line"), 4, 0.90, 25.0, 70, 85, TEXT("unblockable"), 1.20, 16, EAirMomentumMode::Target, 10, 72, 1500},
	{TEXT("air_cyclone"), TEXT("beam"), 4, 0.20, 20.0, 60, 12, TEXT("magic"), 0.20, 10, EAirMomentumMode::Tick, 2, 12, 1200},
};

bool Ready(FAutomationTestBase& Test)
{
	if (KernelData().bReady)
	{
		return true;
	}
	Test.AddError(KernelData().Error.IsEmpty() ? TEXT("kernel data failed to load") : KernelData().Error);
	return false;
}

bool Near(FAutomationTestBase& Test, const FString& What, double Actual, double Expected, double Tolerance = 1.0e-9)
{
	const bool bOk = std::abs(Actual - Expected) <= Tolerance;
	if (!bOk)
	{
		Test.AddError(FString::Printf(TEXT("%s: expected %.17g got %.17g"), *What, Expected, Actual));
	}
	return bOk;
}

int32 SlotOf(const TCHAR* Id)
{
	return KernelData().AirSpells.IndexOfByPredicate([Id](const FAirSpell& Spell) { return Spell.Id == Id; });
}

FActor& Live(FArenaState& State, int32 Id)
{
	return *SimFindActor(State, Id);
}

void SetPool(FActor& Actor, double Hp, double Mana)
{
	Actor.MaxHp = Hp;
	Actor.Hp = Hp;
	Actor.MaxMana = Mana;
	Actor.Mana = Mana;
}

struct FAirPair
{
	FArenaState State;
	int32 Actor = 0;
	int32 Target = 0;
};

// An air caster at the arena centre and a still target RangeM along +X. Tier 4, deep pools, Momentum 0.
FAirPair MakeAir(double RangeM, uint32 Seed = 23)
{
	FAirPair Pair;
	Pair.State = CreateArena(Seed);
	const FSimVec Centre = KernelData().ArenaCentre;
	Pair.Actor = AddMage(Pair.State, 0, Centre, TEXT("Gale")).Id;
	Pair.Target = AddMage(Pair.State, 1, FSimVec{Centre.X + RangeM, Centre.Y}, TEXT("Target")).Id;
	FActor& Actor = Live(Pair.State, Pair.Actor);
	Actor.Air.bSchool = true;
	Actor.Tier = 4;
	SetPool(Actor, 1000.0, 500.0);
	SetPool(Live(Pair.State, Pair.Target), 1000.0, 500.0);
	return Pair;
}

void Step(FArenaState& State, int32 ActorId, const FInputFrame& Input)
{
	TMap<int32, FInputFrame> Inputs;
	Inputs.Add(ActorId, Input);
	StepArena(State, Inputs);
}

FInputFrame CastAt(const FSimVec& Aim, int32 Slot)
{
	FInputFrame Input = SimIdleInput(Aim);
	Input.Slot = Slot;
	Input.bCast = true;
	return Input;
}

int32 CountEvents(const FArenaState& State, const TCHAR* Kind, int32 ActorId)
{
	int32 Count = 0;
	for (const FArenaEvent& Event : State.Events)
	{
		Count += (Event.Kind == Kind && Event.ActorId == ActorId) ? 1 : 0;
	}
	return Count;
}

// The mulberry32 step of kernel.ts / ArenaRandom, written out again so the expected evasions are counted without the kernel.
double Mulberry(uint32& Rng)
{
	Rng += 0x6d2b79f5u;
	const uint32 First = (Rng ^ (Rng >> 15)) * (Rng | 1u);
	const uint32 Second = (First ^ (First >> 7)) * (First | 61u);
	const int64 Sum = static_cast<int64>(static_cast<int32>(First)) + static_cast<int64>(static_cast<int32>(Second));
	const uint32 Mixed = First ^ static_cast<uint32>(Sum);
	return static_cast<double>(Mixed ^ (Mixed >> 14)) / 4294967296.0;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaAirLiterals, "MageArena.Air.Literals",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaAirLiterals::RunTest(const FString& Parameters)
{
	if (!Ready(*this))
	{
		return false;
	}
	const FKernelData& Data = KernelData();
	bool bPass = TestEqual(TEXT("water catalog stays 24"), Data.Spells.Num(), 24);
	bPass &= TestEqual(TEXT("fire rows stay 10"), Data.FireSpells.Num(), 10);
	bPass &= TestEqual(TEXT("ten air rows"), Data.AirSpells.Num(), 10);
	bPass &= TestEqual(TEXT("ten air catalog views"), Data.AirCatalog.Num(), 10);
	if (Data.AirSpells.Num() != 10)
	{
		return false;
	}
	for (int32 Index = 0; Index < 10; ++Index)
	{
		const FRow& Row = Rows[Index];
		const FAirSpell& Spell = Data.AirSpells[Index];
		const FString At = FString::Printf(TEXT("%s"), Row.Id);
		bPass &= TestEqual(*(At + TEXT(" row order")), Spell.Id, FString(Row.Id));
		bPass &= TestEqual(*(At + TEXT(" kind")), Spell.Kind, FString(Row.Kind));
		bPass &= TestEqual(*(At + TEXT(" tier")), Spell.Tier, Row.Tier);
		bPass &= Near(*this, At + TEXT(" cast_s"), Spell.CastS, Row.CastS);
		bPass &= Near(*this, At + TEXT(" cooldown_s"), Spell.CooldownS, Row.CooldownS);
		bPass &= Near(*this, At + TEXT(" mana"), Spell.Mana, Row.Mana);
		bPass &= Near(*this, At + TEXT(" damage"), Spell.Damage, Row.Damage);
		bPass &= TestEqual(*(At + TEXT(" family")), Spell.Family, FString(Row.Family));
		bPass &= Near(*this, At + TEXT(" telegraph_s"), Spell.TelegraphS, Row.TelegraphS);
		bPass &= Near(*this, At + TEXT(" range_m"), Spell.RangeM, Row.RangeM);
		bPass &= TestTrue(*(At + TEXT(" momentum mode")), Spell.MomentumMode == Row.Mode);
		if (Row.Mode != EAirMomentumMode::Lock)
		{
			bPass &= Near(*this, At + TEXT(" momentum"), Spell.MomentumAmount, Row.Momentum);
		}
		bPass &= TestEqual(*(At + TEXT(" windup ticks")), SimTicks(std::max(Spell.CastS, Spell.TelegraphS)), Row.WindupTicks);
		bPass &= TestEqual(*(At + TEXT(" cooldown ticks")), SimTicks(Spell.CooldownS), Row.CooldownTicks);
		bPass &= TestNotNull(*(At + TEXT(" resolves by id (threat scan)")), FindSpellById(Spell.Id));
	}
	const FAirSpell* Bolt = FindAirSpell(TEXT("air_bolt"));
	const FAirSpell* Shear = FindAirSpell(TEXT("air_shear"));
	const FAirSpell* Slip = FindAirSpell(TEXT("air_slipstream"));
	const FAirSpell* Veer = FindAirSpell(TEXT("air_veer"));
	const FAirSpell* Down = FindAirSpell(TEXT("air_downdraft"));
	const FAirSpell* Form = FindAirSpell(TEXT("air_form"));
	const FAirSpell* Squall = FindAirSpell(TEXT("air_squall"));
	const FAirSpell* Eye = FindAirSpell(TEXT("air_eye"));
	const FAirSpell* Tempest = FindAirSpell(TEXT("air_tempest"));
	const FAirSpell* Cyclone = FindAirSpell(TEXT("air_cyclone"));
	if (!Bolt || !Shear || !Slip || !Veer || !Down || !Form || !Squall || !Eye || !Tempest || !Cyclone)
	{
		AddError(TEXT("an air row is missing"));
		return false;
	}
	bPass &= Near(*this, TEXT("Gale Dart 24 m/s"), Bolt->SpeedMps, 24.0);
	bPass &= TestTrue(TEXT("Gale Dart is the bolt"), Bolt->bBolt);
	bPass &= Near(*this, TEXT("Shear 22 m/s"), Shear->SpeedMps, 22.0);
	bPass &= TestEqual(TEXT("Shear two blades"), Shear->Count, 2);
	bPass &= Near(*this, TEXT("Shear +-12 deg is a 24 deg fan"), Shear->SpreadDeg, 24.0);
	bPass &= Near(*this, TEXT("Slipstream 6 m"), Slip->DashM, 6.0);
	bPass &= Near(*this, TEXT("Veering Bolt 16 m/s"), Veer->SpeedMps, 16.0);
	bPass &= Near(*this, TEXT("Veering Bolt enters 60 deg off"), Veer->EntryDeg, 60.0);
	bPass &= Near(*this, TEXT("Downdraft r 2.5"), Down->RadiusM, 2.5);
	bPass &= Near(*this, TEXT("Air Form 3 s"), Form->DurationS, 3.0);
	bPass &= Near(*this, TEXT("Air Form evades 60 % physical"), Form->EvadePhysical, 0.60);
	bPass &= Near(*this, TEXT("Air Form evades 30 % magic"), Form->EvadeMagic, 0.30);
	bPass &= Near(*this, TEXT("Air Form needs 40"), Form->NeedMomentum, 40.0);
	bPass &= TestEqual(TEXT("Squall four orbs"), Squall->Count, 4);
	bPass &= Near(*this, TEXT("Squall 14 m/s"), Squall->SpeedMps, 14.0);
	bPass &= Near(*this, TEXT("Squall 0.25 s apart"), Squall->IntervalS, 0.25);
	bPass &= Near(*this, TEXT("Eye locks 100"), Eye->MomentumLockValue, 100.0);
	bPass &= Near(*this, TEXT("Eye for 5 s"), Eye->MomentumLockS, 5.0);
	bPass &= Near(*this, TEXT("Eye drain x1.5"), Eye->AbsorbDrainMult, EyeDrainMult);
	bPass &= TestTrue(TEXT("Tempest roots its caster"), Tempest->bRootDuringCast);
	bPass &= Near(*this, TEXT("Cyclone 2 s"), Cyclone->DurationS, 2.0);
	bPass &= Near(*this, TEXT("Cyclone 0.25 s tick"), Cyclone->TickS, 0.25);

	const FMomentumRules& Momentum = Data.Momentum;
	bPass &= Near(*this, TEXT("momentum min"), Momentum.Min, 0.0);
	bPass &= Near(*this, TEXT("momentum max"), Momentum.Max, MomentumMax);
	bPass &= Near(*this, TEXT("gain per second moving"), Momentum.GainPerSecondMoving, GainPerSecond);
	bPass &= Near(*this, TEXT("moving above"), Momentum.MovingAboveMps, MovingAboveMps);
	bPass &= Near(*this, TEXT("decay per second still"), Momentum.DecayPerSecondStill, DecayPerSecond);
	bPass &= Near(*this, TEXT("absorb drain"), Momentum.AbsorbDrainMult, AirDrainMult);
	bPass &= TestEqual(TEXT("two bands"), Momentum.Thresholds.Num(), 2);
	if (Momentum.Thresholds.Num() == 2)
	{
		bPass &= Near(*this, TEXT("band 40"), Momentum.Thresholds[0].At, PierceAt);
		bPass &= TestTrue(TEXT("band 40 pierces 1"), Momentum.Thresholds[0].bPierce && Momentum.Thresholds[0].SpellsPierce == 1);
		bPass &= Near(*this, TEXT("band 40 roll"), Momentum.Thresholds[0].RollStaminaMult, RollMult);
		bPass &= Near(*this, TEXT("band 80"), Momentum.Thresholds[1].At, RangeAt);
		bPass &= Near(*this, TEXT("band 80 range"), Momentum.Thresholds[1].SpellRangeMult, RangeMult);
	}
	bPass &= Near(*this, TEXT("walk is above the gain speed"), Data.WalkMps, WalkMps);
	bPass &= Near(*this, TEXT("roll stamina"), Data.RollStaminaCost, RollStamina);
	bPass &= Near(*this, TEXT("60 Hz"), Data.SimStepHz, Hz);
	return bPass;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaAirStrictLoad, "MageArena.Air.StrictLoad",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaAirStrictLoad::RunTest(const FString& Parameters)
{
	if (!Ready(*this))
	{
		return false;
	}
	FString Text;
	if (!TestTrue(TEXT("air csv readable"), FFileHelper::LoadFileToString(Text, *KernelDataAirSpellsPath())))
	{
		return false;
	}
	TArray<FAirSpell> Spells;
	TArray<FSpell> Catalog;
	FString Error;
	bool bPass = TestTrue(TEXT("the sheet as committed parses"), ParseAirSpellsCsv(Text, Spells, Catalog, Error));
	bPass &= TestEqual(TEXT("ten rows"), Spells.Num(), 10);
	// Notes stay whole even when they hold commas (the Fire reading of an unquoted last column).
	bPass &= TestTrue(TEXT("unquoted notes keep their commas"), ParseAirSpellsCsv(Text.Replace(TEXT("The signature."), TEXT("The signature, drawn.")), Spells, Catalog, Error));
	struct FBad
	{
		const TCHAR* Name;
		const TCHAR* From;
		const TCHAR* To;
	};
	const FBad Bad[] = {
		{TEXT("cooldown cell with a unit"), TEXT("air_bolt,Gale Dart,0,projectile 24 m/s,0.00,0.30,"), TEXT("air_bolt,Gale Dart,0,projectile 24 m/s,0.00,0.30s,")},
		{TEXT("empty mana cell"), TEXT("0.12,3.0,12,9 each"), TEXT("0.12,3.0,,9 each")},
		{TEXT("tier 5"), TEXT("air_tempest,Tempest Lance,4,"), TEXT("air_tempest,Tempest Lance,5,")},
		{TEXT("unknown shape"), TEXT("ground circle r 2.5 m at a point"), TEXT("ground square 2.5 m at a point")},
		{TEXT("dash reach disagrees with range"), TEXT("dash 6 m,0.00,5.0,12,0,n/a,0.00,6,"), TEXT("dash 7 m,0.00,5.0,12,0,n/a,0.00,6,")},
		{TEXT("unknown momentum cell"), TEXT(",3 per hit,"), TEXT(",3 per orb,")},
		{TEXT("unknown blockable"), TEXT("85,UNBLOCKABLE,"), TEXT("85,unblockable-ish,")},
		{TEXT("beam without a tick"), TEXT("12 per 0.25 s tick"), TEXT("12")},
		{TEXT("form without its Momentum need"), TEXT("Needs Momentum 40 or more"), TEXT("Needs a breeze")},
		{TEXT("header renamed"), TEXT("momentum_gain"), TEXT("heat_gain")},
	};
	for (const FBad& Case : Bad)
	{
		const FString Mutated = Text.Replace(Case.From, Case.To);
		if (!TestNotEqual(FString::Printf(TEXT("%s: the mutation applies"), Case.Name), Mutated, Text))
		{
			bPass = false;
			continue;
		}
		TArray<FAirSpell> Out;
		TArray<FSpell> OutCatalog;
		FString Why;
		const bool bLoaded = ParseAirSpellsCsv(Mutated, Out, OutCatalog, Why);
		bPass &= TestFalse(FString::Printf(TEXT("%s is a load failure"), Case.Name), bLoaded);
		bPass &= TestTrue(FString::Printf(TEXT("%s leaves nothing behind"), Case.Name), Out.Num() == 0 && OutCatalog.Num() == 0);
		AddInfo(FString::Printf(TEXT("%s -> %s"), Case.Name, *Why));
	}
	// The kernel load itself refuses a bad cell: KernelData is not ready with that file.
	TMap<FString, FString> Overrides;
	Overrides.Add(KernelDataAirSpellsPath(), Text.Replace(TEXT("0.00,0.30,4,7,"), TEXT("0.00,0.30,4,seven,")));
	const FKernelData Broken = LoadKernelDataWithOverrides(Overrides);
	bPass &= TestFalse(TEXT("kernel data with a bad air cell is not ready"), Broken.bReady);
	bPass &= TestTrue(TEXT("and says which file"), Broken.Error.Contains(TEXT("spells-air.csv")));
	const FKernelData Clean = LoadKernelDataWithOverrides(TMap<FString, FString>());
	bPass &= TestTrue(TEXT("the committed sheet loads"), Clean.bReady);
	bPass &= TestEqual(TEXT("the air file is not in the pin hash"), Clean.PinHash, KernelData().PinHash);
	return bPass;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaAirMomentum, "MageArena.Air.Momentum",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaAirMomentum::RunTest(const FString& Parameters)
{
	if (!Ready(*this))
	{
		return false;
	}
	FAirPair Pair = MakeAir(10.0);
	const int32 Water = Pair.Target;
	bool bPass = true;
	// 30 ticks of walking (4.5 m/s > 4) at +20/s: 30 x 20 / 60 = 10. The water actor walks too and stays at 0.
	FInputFrame Walk = SimIdleInput(FSimVec{0.0, 0.0});
	for (int32 Tick = 0; Tick < 30; ++Tick)
	{
		FInputFrame Air = Walk;
		Air.Move = FSimVec{0.0, 1.0};
		Air.Aim = SimAdd(Live(Pair.State, Pair.Actor).Pos, FSimVec{1.0, 0.0});
		FInputFrame Other = Air;
		Other.Aim = SimAdd(Live(Pair.State, Water).Pos, FSimVec{-1.0, 0.0});
		TMap<int32, FInputFrame> Inputs;
		Inputs.Add(Pair.Actor, Air);
		Inputs.Add(Water, Other);
		StepArena(Pair.State, Inputs);
	}
	bPass &= Near(*this, TEXT("30 walking ticks"), Live(Pair.State, Pair.Actor).Air.Momentum, 10.0);
	bPass &= Near(*this, TEXT("a water actor gains none"), Live(Pair.State, Water).Air.Momentum, 0.0);
	// 12 still ticks at -25/s: 10 - 12 x 25 / 60 = 5.
	for (int32 Tick = 0; Tick < 12; ++Tick)
	{
		Step(Pair.State, Pair.Actor, SimIdleInput(SimAdd(Live(Pair.State, Pair.Actor).Pos, FSimVec{1.0, 0.0})));
	}
	bPass &= Near(*this, TEXT("12 still ticks"), Live(Pair.State, Pair.Actor).Air.Momentum, 5.0);
	// 12 more still ticks: 5 - 5 = 0, and 6 after that stay clamped at 0.
	for (int32 Tick = 0; Tick < 18; ++Tick)
	{
		Step(Pair.State, Pair.Actor, SimIdleInput(SimAdd(Live(Pair.State, Pair.Actor).Pos, FSimVec{1.0, 0.0})));
	}
	bPass &= Near(*this, TEXT("decay stops at 0"), Live(Pair.State, Pair.Actor).Air.Momentum, 0.0);
	// Walking while the ward is held (move x0.5 = 2.25 m/s): neither above 4 nor still, so Momentum holds.
	Live(Pair.State, Pair.Actor).Air.Momentum = 30.0;
	const double HeldMult = KernelData().Absorb.MoveSpeedWhileHeldMult;
	bPass &= TestTrue(TEXT("held walk is under 4 m/s"), WalkMps * HeldMult < MovingAboveMps && WalkMps * HeldMult > 0.0);
	for (int32 Tick = 0; Tick < 10; ++Tick)
	{
		FInputFrame Held = SimIdleInput(SimAdd(Live(Pair.State, Pair.Actor).Pos, FSimVec{1.0, 0.0}));
		Held.Move = FSimVec{0.0, 1.0};
		Held.bAbsorb = true;
		Step(Pair.State, Pair.Actor, Held);
	}
	bPass &= Near(*this, TEXT("slow walking neither gains nor decays"), Live(Pair.State, Pair.Actor).Air.Momentum, 30.0);
	// Gains clamp at 100.
	Live(Pair.State, Pair.Actor).Air.Momentum = 99.9;
	FInputFrame Fast = SimIdleInput(SimAdd(Live(Pair.State, Pair.Actor).Pos, FSimVec{1.0, 0.0}));
	Fast.Move = FSimVec{0.0, -1.0};
	Step(Pair.State, Pair.Actor, Fast);
	bPass &= Near(*this, TEXT("clamped at 100"), Live(Pair.State, Pair.Actor).Air.Momentum, MomentumMax);
	return bPass;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaAirThresholds, "MageArena.Air.Thresholds",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaAirThresholds::RunTest(const FString& Parameters)
{
	if (!Ready(*this))
	{
		return false;
	}
	bool bPass = true;
	FActor Probe;
	Probe.Air.bSchool = true;
	const double Levels[] = {0.0, 39.999, 40.0, 79.999, 80.0, 100.0};
	for (const double Level : Levels)
	{
		Probe.Air.Momentum = Level;
		const bool bPierce = Level >= PierceAt;
		const bool bRange = Level >= RangeAt;
		bPass &= TestEqual(*FString::Printf(TEXT("pierce at %g"), Level), AirSpellsPierce(Probe), bPierce ? 1 : 0);
		bPass &= Near(*this, FString::Printf(TEXT("roll mult at %g"), Level), AirRollStaminaMult(Probe), bPierce ? RollMult : 1.0);
		bPass &= Near(*this, FString::Printf(TEXT("range mult at %g"), Level), AirSpellRangeMult(Probe), bRange ? RangeMult : 1.0);
	}
	FActor Plain;
	Plain.Air.Momentum = 100.0;
	bPass &= TestEqual(TEXT("a non-air actor never pierces"), AirSpellsPierce(Plain), 0);
	bPass &= Near(*this, TEXT("nor rolls cheaper"), AirRollStaminaMult(Plain), 1.0);

	// Roll at Momentum 50 (locked so the tick cannot move it): 25 x 0.6 = 15 stamina. At 0: 25.
	for (const double Level : {50.0, 0.0})
	{
		FAirPair Pair = MakeAir(10.0);
		FActor& Actor = Live(Pair.State, Pair.Actor);
		Actor.Air.Momentum = Level;
		Actor.Air.LockValue = Level;
		Actor.Air.LockUntil = 1000;
		const double Before = Actor.Stamina;
		FInputFrame Roll = SimIdleInput(SimAdd(Actor.Pos, FSimVec{1.0, 0.0}));
		Roll.Move = FSimVec{0.0, 1.0};
		Roll.bRoll = true;
		Step(Pair.State, Pair.Actor, Roll);
		bPass &= Near(*this, FString::Printf(TEXT("roll stamina at Momentum %g"), Level), Before - Live(Pair.State, Pair.Actor).Stamina,
			Level >= PierceAt ? RollStamina * RollMult : RollStamina);
	}
	// Range: a Gale Dart released at Momentum 80 flies 14 x 1.3 = 18.2 m; at 79 it flies 14.
	for (const double Level : {80.0, 79.0})
	{
		FAirPair Pair = MakeAir(30.0);
		FActor& Actor = Live(Pair.State, Pair.Actor);
		Actor.Air.Momentum = Level;
		Actor.Air.LockValue = Level;
		Actor.Air.LockUntil = 1000;
		Step(Pair.State, Pair.Actor, CastAt(SimAdd(Actor.Pos, FSimVec{1.0, 0.0}), SlotOf(TEXT("air_bolt"))));
		const FProjectile* Shot = Pair.State.Projectiles.FindByPredicate([&](const FProjectile& P) { return P.OwnerId == Pair.Actor; });
		// The release tick's flight already ran: 24 / 60 = 0.4 m.
		bPass &= TestNotNull(*FString::Printf(TEXT("dart at %g"), Level), Shot);
		if (Shot)
		{
			bPass &= Near(*this, FString::Printf(TEXT("dart range at Momentum %g"), Level), Shot->RemainingM + 24.0 / Hz, Level >= RangeAt ? 14.0 * RangeMult : 14.0);
		}
	}
	// Pierce: two targets in a row 4 m and 6 m out. At Momentum 40 the dart hits both, at 39 only the first.
	for (const double Level : {40.0, 39.0})
	{
		FAirPair Pair = MakeAir(4.0);
		FActor& Actor = Live(Pair.State, Pair.Actor);
		Actor.Air.Momentum = Level;
		Actor.Air.LockValue = Level;
		Actor.Air.LockUntil = 1000;
		FActor& Second = AddMage(Pair.State, 1, FSimVec{Actor.Pos.X + 6.0, Actor.Pos.Y}, TEXT("Second"));
		SetPool(Second, 1000.0, 500.0);
		const int32 SecondId = Second.Id;
		Step(Pair.State, Pair.Actor, CastAt(SimAdd(Live(Pair.State, Pair.Actor).Pos, FSimVec{1.0, 0.0}), SlotOf(TEXT("air_bolt"))));
		for (int32 Tick = 0; Tick < 30; ++Tick)
		{
			Step(Pair.State, Pair.Actor, SimIdleInput(SimAdd(Live(Pair.State, Pair.Actor).Pos, FSimVec{1.0, 0.0})));
		}
		bPass &= Near(*this, FString::Printf(TEXT("first target at %g"), Level), 1000.0 - Live(Pair.State, Pair.Target).Hp, 7.0);
		bPass &= Near(*this, FString::Printf(TEXT("second target at %g"), Level), 1000.0 - Live(Pair.State, SecondId).Hp, Level >= PierceAt ? 7.0 : 0.0);
	}
	// Absorb drain: x1.2 for an air actor, x1.2 x 1.5 = 1.8 while Eye of the Storm lasts.
	{
		FAirPair Pair = MakeAir(10.0);
		FActor& Actor = Live(Pair.State, Pair.Actor);
		bPass &= Near(*this, TEXT("air drain"), AirAbsorbDrainMult(Pair.State, Actor), AirDrainMult);
		Actor.Air.AbsorbDrainSpellMult = EyeDrainMult;
		Actor.Air.AbsorbDrainSpellUntil = Pair.State.Tick + 10;
		bPass &= Near(*this, TEXT("air drain under the Eye"), AirAbsorbDrainMult(Pair.State, Actor), AirDrainMult * EyeDrainMult);
		bPass &= Near(*this, TEXT("water drain"), AirAbsorbDrainMult(Pair.State, Live(Pair.State, Pair.Target)), 1.0);
	}
	return bPass;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaAirRows, "MageArena.Air.Rows",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaAirRows::RunTest(const FString& Parameters)
{
	if (!Ready(*this))
	{
		return false;
	}
	bool bPass = true;
	// Mana at the press, the cooldown, and the windup, for every row. Momentum 50 covers Air Form's need of 40.
	for (const FRow& Row : Rows)
	{
		FAirPair Pair = MakeAir(5.0);
		FActor& Actor = Live(Pair.State, Pair.Actor);
		Actor.Air.Momentum = 50.0;
		const int32 Slot = SlotOf(Row.Id);
		Step(Pair.State, Pair.Actor, CastAt(Live(Pair.State, Pair.Target).Pos, Slot));
		const FActor& After = Live(Pair.State, Pair.Actor);
		const int32 Tick = Pair.State.Tick;
		bPass &= Near(*this, FString::Printf(TEXT("%s mana"), Row.Id), 500.0 - After.Mana, Row.Mana);
		bPass &= TestEqual(*FString::Printf(TEXT("%s cooldown"), Row.Id), AirCooldownUntil(After, Row.Id), Tick + Row.CooldownTicks);
		bPass &= TestEqual(*FString::Printf(TEXT("%s one cast event"), Row.Id), CountEvents(Pair.State, TEXT("cast"), Pair.Actor), 1);
		if (Row.WindupTicks > 0)
		{
			bPass &= TestTrue(*FString::Printf(TEXT("%s windup %d ticks"), Row.Id, Row.WindupTicks),
				After.Pending.IsSet() && After.Pending->ReleaseTick - After.Pending->StartTick == Row.WindupTicks);
		}
		else
		{
			bPass &= TestFalse(*FString::Printf(TEXT("%s releases on the press"), Row.Id), After.Pending.IsSet());
		}
		// A second press inside the cooldown does nothing.
		for (int32 Index = 0; Index < Row.WindupTicks + 2; ++Index)
		{
			Step(Pair.State, Pair.Actor, SimIdleInput(Live(Pair.State, Pair.Target).Pos));
		}
		const double ManaBefore = Live(Pair.State, Pair.Actor).Mana;
		Step(Pair.State, Pair.Actor, CastAt(Live(Pair.State, Pair.Target).Pos, Slot));
		bPass &= TestEqual(*FString::Printf(TEXT("%s on cooldown"), Row.Id), CountEvents(Pair.State, TEXT("cast"), Pair.Actor), 1);
		bPass &= TestTrue(*FString::Printf(TEXT("%s no mana on a refused press"), Row.Id), Live(Pair.State, Pair.Actor).Mana >= ManaBefore);
	}
	// A row above the actor's tier is not offered.
	{
		FAirPair Pair = MakeAir(5.0);
		Live(Pair.State, Pair.Actor).Tier = 3;
		Step(Pair.State, Pair.Actor, CastAt(Live(Pair.State, Pair.Target).Pos, SlotOf(TEXT("air_tempest"))));
		bPass &= TestEqual(TEXT("tier 3 cannot cast Tempest Lance"), CountEvents(Pair.State, TEXT("cast"), Pair.Actor), 0);
	}

	// Damage and the Momentum each row pays, against a still target with no ward (Momentum starts at 0 and the still
	// caster's decay clamps at 0, so the first connecting tick shows the payment exactly).
	struct FHitCase
	{
		const TCHAR* Id;
		double Distance;
		double Damage;
		double Momentum;
		int32 MaxTicks;
	};
	const FHitCase Hits[] = {
		{TEXT("air_bolt"), 5.0, 7.0, 2.0, 40},
		// Twin Tides fan at +-12 deg: 2 m out each blade passes 2 tan 12 = 0.43 m off the line, inside 0.38 + 0.12.
		{TEXT("air_shear"), 2.0, 18.0, 8.0, 40},
		{TEXT("air_veer"), 10.0, 24.0, 6.0, 120},
		{TEXT("air_downdraft"), 6.0, 26.0, 5.0, 60},
		{TEXT("air_squall"), 6.0, 11.0, 3.0, 60},
		{TEXT("air_tempest"), 8.0, 85.0, 10.0, 90},
		{TEXT("air_cyclone"), 6.0, 12.0, 2.0, 60},
	};
	for (const FHitCase& Case : Hits)
	{
		FAirPair Pair = MakeAir(Case.Distance);
		const int32 Slot = SlotOf(Case.Id);
		Step(Pair.State, Pair.Actor, CastAt(Live(Pair.State, Pair.Target).Pos, Slot));
		int32 Ticks = 0;
		while (Live(Pair.State, Pair.Target).Hp >= 1000.0 && Ticks < Case.MaxTicks)
		{
			Step(Pair.State, Pair.Actor, SimIdleInput(Live(Pair.State, Pair.Target).Pos));
			++Ticks;
		}
		bPass &= Near(*this, FString::Printf(TEXT("%s first connecting damage"), Case.Id), 1000.0 - Live(Pair.State, Pair.Target).Hp, Case.Damage);
		bPass &= Near(*this, FString::Printf(TEXT("%s momentum paid"), Case.Id), Live(Pair.State, Pair.Actor).Air.Momentum, Case.Momentum);
	}
	// Squall total: four orbs of 11. Cyclone total: eight 0.25 s ticks in 2 s, 8 x 12 = 96.
	for (const TCHAR* Id : {TEXT("air_squall"), TEXT("air_cyclone")})
	{
		FAirPair Pair = MakeAir(6.0);
		Step(Pair.State, Pair.Actor, CastAt(Live(Pair.State, Pair.Target).Pos, SlotOf(Id)));
		for (int32 Tick = 0; Tick < 200; ++Tick)
		{
			Step(Pair.State, Pair.Actor, SimIdleInput(Live(Pair.State, Pair.Target).Pos));
		}
		const bool bSquall = FString(Id) == TEXT("air_squall");
		bPass &= Near(*this, FString::Printf(TEXT("%s total"), Id), 1000.0 - Live(Pair.State, Pair.Target).Hp, bSquall ? 44.0 : 96.0);
	}
	// Tempest Lance through a held ward: unblockable, so the full 85.
	{
		FAirPair Pair = MakeAir(8.0);
		const FSimVec CasterPos = Live(Pair.State, Pair.Actor).Pos;
		TMap<int32, FInputFrame> Inputs;
		FInputFrame Ward = SimIdleInput(CasterPos);
		Ward.bAbsorb = true;
		Inputs.Add(Pair.Actor, CastAt(Live(Pair.State, Pair.Target).Pos, SlotOf(TEXT("air_tempest"))));
		Inputs.Add(Pair.Target, Ward);
		StepArena(Pair.State, Inputs);
		Inputs.Add(Pair.Actor, SimIdleInput(Live(Pair.State, Pair.Target).Pos));
		for (int32 Tick = 0; Tick < 80; ++Tick)
		{
			StepArena(Pair.State, Inputs);
		}
		bPass &= TestTrue(TEXT("the ward was up"), Live(Pair.State, Pair.Target).bAbsorb || Live(Pair.State, Pair.Target).Metrics.ManaRaised > 0.0);
		bPass &= Near(*this, TEXT("Tempest Lance through the ward"), 1000.0 - Live(Pair.State, Pair.Target).Hp, 85.0);
		// Rooted for the cast: the caster did not move while it walked against the windup.
	}
	{
		FAirPair Pair = MakeAir(8.0);
		const FSimVec Start = Live(Pair.State, Pair.Actor).Pos;
		Step(Pair.State, Pair.Actor, CastAt(Live(Pair.State, Pair.Target).Pos, SlotOf(TEXT("air_tempest"))));
		FInputFrame Walk = SimIdleInput(Live(Pair.State, Pair.Target).Pos);
		Walk.Move = FSimVec{0.0, 1.0};
		for (int32 Tick = 0; Tick < 30; ++Tick)
		{
			Step(Pair.State, Pair.Actor, Walk);
		}
		bPass &= Near(*this, TEXT("Tempest Lance roots its caster"), SimDistance(Start, Live(Pair.State, Pair.Actor).Pos), 0.0);
	}
	// Slipstream: 6 m along the aim at once and +30 Momentum.
	{
		FAirPair Pair = MakeAir(10.0);
		const FSimVec Start = Live(Pair.State, Pair.Actor).Pos;
		Step(Pair.State, Pair.Actor, CastAt(FSimVec{Start.X, Start.Y + 20.0}, SlotOf(TEXT("air_slipstream"))));
		bPass &= Near(*this, TEXT("Slipstream 6 m"), SimDistance(Start, Live(Pair.State, Pair.Actor).Pos), 6.0);
		bPass &= Near(*this, TEXT("Slipstream +30"), Live(Pair.State, Pair.Actor).Air.Momentum, 30.0);
	}
	// Air Form: refused at Momentum 39 (pre-tick decay 25 / 60 leaves 38.58), cast at 50: 50 - 25/60 - 40 at the press.
	{
		FAirPair Pair = MakeAir(10.0);
		Live(Pair.State, Pair.Actor).Air.Momentum = 39.0;
		Step(Pair.State, Pair.Actor, CastAt(Live(Pair.State, Pair.Target).Pos, SlotOf(TEXT("air_form"))));
		bPass &= TestEqual(TEXT("Air Form needs 40"), CountEvents(Pair.State, TEXT("cast"), Pair.Actor), 0);
		Live(Pair.State, Pair.Actor).Air.Momentum = 50.0;
		Step(Pair.State, Pair.Actor, CastAt(Live(Pair.State, Pair.Target).Pos, SlotOf(TEXT("air_form"))));
		bPass &= Near(*this, TEXT("Air Form spends 40"), Live(Pair.State, Pair.Actor).Air.Momentum, 50.0 - DecayPerSecond / Hz - 40.0);
		const int32 Release = Live(Pair.State, Pair.Actor).Pending.IsSet() ? Live(Pair.State, Pair.Actor).Pending->ReleaseTick : -1;
		while (Pair.State.Tick < Release)
		{
			Step(Pair.State, Pair.Actor, SimIdleInput(Live(Pair.State, Pair.Target).Pos));
		}
		bPass &= TestEqual(TEXT("Air Form lasts 3 s = 180 ticks"), Live(Pair.State, Pair.Actor).Air.FormUntil, Release + 180);
		bPass &= TestEqual(TEXT("one airform event"), CountEvents(Pair.State, TEXT("airform"), Pair.Actor), 1);
	}
	// Eye of the Storm: Momentum locked at 100 for 5 s = 300 ticks, then decay resumes while still.
	{
		FAirPair Pair = MakeAir(10.0);
		Step(Pair.State, Pair.Actor, CastAt(Live(Pair.State, Pair.Target).Pos, SlotOf(TEXT("air_eye"))));
		const int32 Release = Live(Pair.State, Pair.Actor).Pending->ReleaseTick;
		while (Pair.State.Tick < Release)
		{
			Step(Pair.State, Pair.Actor, SimIdleInput(Live(Pair.State, Pair.Target).Pos));
		}
		bPass &= Near(*this, TEXT("Eye locks at 100"), Live(Pair.State, Pair.Actor).Air.Momentum, 100.0);
		bPass &= TestEqual(TEXT("for 300 ticks"), Live(Pair.State, Pair.Actor).Air.LockUntil, Release + 300);
		for (int32 Tick = 0; Tick < 299; ++Tick)
		{
			Step(Pair.State, Pair.Actor, SimIdleInput(Live(Pair.State, Pair.Target).Pos));
		}
		bPass &= Near(*this, TEXT("still 100 on the last locked tick"), Live(Pair.State, Pair.Actor).Air.Momentum, 100.0);
		Step(Pair.State, Pair.Actor, SimIdleInput(Live(Pair.State, Pair.Target).Pos));
		bPass &= Near(*this, TEXT("decay resumes"), Live(Pair.State, Pair.Actor).Air.Momentum, 100.0 - DecayPerSecond / Hz);
	}
	return bPass;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaAirVeerArc, "MageArena.Air.VeerArc",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaAirVeerArc::RunTest(const FString& Parameters)
{
	if (!Ready(*this))
	{
		return false;
	}
	bool bPass = true;
	// L = 10, E = 60 deg: R = 10 / (2 sin 60) = 5.773502691896258, arc = 2 E R = 12.091995761561452.
	const FSimVec From{0.0, 0.0};
	const FSimVec To{10.0, 0.0};
	for (const double Side : {1.0, -1.0})
	{
		const FAirArc Arc = AirVeerArc(From, To, 60.0, Side);
		bPass &= Near(*this, TEXT("radius"), Arc.RadiusM, 5.773502691896258);
		bPass &= Near(*this, TEXT("arc length"), Arc.LengthM, 12.091995761561452);
		const FSimVec End = AirArcPoint(Arc, Arc.LengthM);
		bPass &= Near(*this, FString::Printf(TEXT("endpoint x (side %g)"), Side), End.X, 10.0);
		bPass &= Near(*this, FString::Printf(TEXT("endpoint y (side %g)"), Side), End.Y, 0.0);
		// Launch 60 deg off the line of sight, to the given side.
		bPass &= Near(*this, TEXT("launch x"), Arc.LaunchDir.X, 0.5);
		bPass &= Near(*this, TEXT("launch y"), Arc.LaunchDir.Y, Side * 0.8660254037844386);
		// Arrival tangent: 60 deg off the chord on the other side, so it comes in from the flank.
		const double S = Arc.LengthM - 1.0e-6;
		const FSimVec Before = AirArcPoint(Arc, S);
		const FSimVec Tangent = SimUnit(SimSub(End, Before));
		bPass &= Near(*this, TEXT("arrival x"), Tangent.X, 0.5, 1.0e-5);
		bPass &= Near(*this, TEXT("arrival y"), Tangent.Y, -Side * 0.8660254037844386, 1.0e-5);
		// Halfway the bolt is R (1 - cos 60) = 2.886751345948129 off the line.
		const FSimVec Mid = AirArcPoint(Arc, Arc.LengthM * 0.5);
		bPass &= Near(*this, TEXT("mid x"), Mid.X, 5.0);
		bPass &= Near(*this, TEXT("mid y"), Mid.Y, Side * 2.886751345948129);
	}
	bPass &= Near(*this, TEXT("even activation launches left"), AirVeerSide(8), 1.0);
	bPass &= Near(*this, TEXT("odd activation launches right"), AirVeerSide(9), -1.0);

	// The kernel flies exactly that arc: 16 m/s, so after k ticks the bolt is at arc distance 16 k / 60 (k >= 1, the
	// release tick's own flight included), until it reaches the target standing on the aim point.
	auto Fly = [&](uint32 Seed, FString& Hash) -> bool
	{
		FAirPair Pair = MakeAir(10.0, Seed);
		const FSimVec Caster = Live(Pair.State, Pair.Actor).Pos;
		const FSimVec Aim = Live(Pair.State, Pair.Target).Pos;
		Step(Pair.State, Pair.Actor, CastAt(Aim, SlotOf(TEXT("air_veer"))));
		const int32 Activation = Live(Pair.State, Pair.Actor).Pending->ActivationId;
		const int32 Release = Live(Pair.State, Pair.Actor).Pending->ReleaseTick;
		const FAirArc Arc = AirVeerArc(Caster, Aim, 60.0, AirVeerSide(Activation));
		bool bOk = true;
		int32 HitTick = -1;
		for (int32 Guard = 0; Guard < 200 && HitTick < 0; ++Guard)
		{
			Step(Pair.State, Pair.Actor, SimIdleInput(Aim));
			if (Pair.State.Tick < Release)
			{
				continue;
			}
			const FProjectile* Bolt = Pair.State.Projectiles.FindByPredicate([&](const FProjectile& P) { return P.ActivationId == Activation; });
			if (Live(Pair.State, Pair.Target).Hp < 1000.0)
			{
				HitTick = Pair.State.Tick;
				break;
			}
			if (!Bolt)
			{
				AddError(TEXT("the veering bolt vanished before it hit"));
				return false;
			}
			const int32 K = Pair.State.Tick - Release + 1;
			const FSimVec Expected = AirArcPoint(Arc, 16.0 * K / Hz);
			bOk &= Near(*this, FString::Printf(TEXT("arc x after %d ticks"), K), Bolt->Pos.X, Expected.X, 1.0e-9);
			bOk &= Near(*this, FString::Printf(TEXT("arc y after %d ticks"), K), Bolt->Pos.Y, Expected.Y, 1.0e-9);
		}
		// The bolt meets the body when it is within 0.38 + 0.12 = 0.5 m of the aim point: arc distance >= 11.59 m,
		// which it covers in its 44th tick of flight (16 x 44 / 60 = 11.73; 43 ticks is 11.47).
		bOk &= TestEqual(TEXT("hits on the 44th flight tick"), HitTick - Release + 1, 44);
		bOk &= Near(*this, TEXT("veer damage"), 1000.0 - Live(Pair.State, Pair.Target).Hp, 24.0);
		Hash = StateHash(Pair.State);
		return bOk;
	};
	FString HashA;
	FString HashB;
	bPass &= Fly(31, HashA);
	bPass &= Fly(31, HashB);
	bPass &= TestEqual(TEXT("the same inputs fly the same arc"), HashA, HashB);
	return bPass;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaAirSquallTicks, "MageArena.Air.SquallTicks",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaAirSquallTicks::RunTest(const FString& Parameters)
{
	if (!Ready(*this))
	{
		return false;
	}
	FAirPair Pair = MakeAir(12.0);
	const FSimVec Aim = Live(Pair.State, Pair.Target).Pos;
	Step(Pair.State, Pair.Actor, CastAt(Aim, SlotOf(TEXT("air_squall"))));
	const int32 Press = Pair.State.Tick;
	// Windup max(0.30, 0.30) = 18 ticks; then orbs on R, R + 15, R + 30, R + 45 (0.25 s = 15 ticks).
	TArray<int32> ReleaseTicks;
	TSet<int32> Activations;
	TSet<int32> Seen;
	for (int32 Tick = 0; Tick < 140; ++Tick)
	{
		for (const FProjectile& Shot : Pair.State.Projectiles)
		{
			if (Shot.OwnerId == Pair.Actor && !Seen.Contains(Shot.Id))
			{
				Seen.Add(Shot.Id);
				ReleaseTicks.Add(Pair.State.Tick);
				Activations.Add(Shot.ActivationId);
			}
		}
		Step(Pair.State, Pair.Actor, SimIdleInput(Aim));
	}
	const int32 R = Press + 18;
	bool bPass = TestEqual(TEXT("four orbs"), ReleaseTicks.Num(), 4);
	if (ReleaseTicks.Num() == 4)
	{
		bPass &= TestEqual(TEXT("orb 1"), ReleaseTicks[0], R);
		bPass &= TestEqual(TEXT("orb 2"), ReleaseTicks[1], R + 15);
		bPass &= TestEqual(TEXT("orb 3"), ReleaseTicks[2], R + 30);
		bPass &= TestEqual(TEXT("orb 4"), ReleaseTicks[3], R + 45);
	}
	bPass &= TestEqual(TEXT("each orb its own activation id"), Activations.Num(), 4);
	bPass &= Near(*this, TEXT("four orbs of 11 land"), 1000.0 - Live(Pair.State, Pair.Target).Hp, 44.0);
	bPass &= TestFalse(TEXT("the volley is done"), Live(Pair.State, Pair.Actor).Air.Volley.IsSet());
	return bPass;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaAirFormEvasion, "MageArena.Air.FormEvasion",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaAirFormEvasion::RunTest(const FString& Parameters)
{
	if (!Ready(*this))
	{
		return false;
	}
	const FAirSpell* Form = FindAirSpell(TEXT("air_form"));
	if (!TestNotNull(TEXT("air_form"), Form))
	{
		return false;
	}
	// Seed 23023, no other draw: the hits below take draws 1-100 (magic) and 101-200 (physical). Counted with the
	// mulberry32 step written out above, and by hand from a Python port of kernel.ts (runs/T23): 28 magic and 63
	// physical evasions, so 72 and 37 hits land. The first draw is 0.015370714943856001.
	constexpr uint32 Seed = 23023;
	constexpr int32 MagicLand = 72;
	constexpr int32 PhysicalLand = 37;
	uint32 Rng = Seed;
	int32 CountedMagic = 0;
	int32 CountedPhysical = 0;
	const double FirstDraw = Mulberry(Rng);
	CountedMagic += FirstDraw < Form->EvadeMagic ? 0 : 1;
	for (int32 Index = 1; Index < 100; ++Index)
	{
		CountedMagic += Mulberry(Rng) < Form->EvadeMagic ? 0 : 1;
	}
	for (int32 Index = 0; Index < 100; ++Index)
	{
		CountedPhysical += Mulberry(Rng) < Form->EvadePhysical ? 0 : 1;
	}
	bool bPass = Near(*this, TEXT("first draw"), FirstDraw, 0.015370714943856001, 1.0e-15);
	bPass &= TestEqual(TEXT("written-out RNG agrees on magic"), CountedMagic, MagicLand);
	bPass &= TestEqual(TEXT("written-out RNG agrees on physical"), CountedPhysical, PhysicalLand);

	FArenaState State = CreateArena(Seed);
	const int32 Player = AddMage(State, 0, KernelData().PlayerSpawn, TEXT("Cassia")).Id;
	FActor& Gale = AddMage(State, 1, KernelData().ArenaCentre, TEXT("Gale"));
	Gale.Air.bSchool = true;
	SetPool(Gale, 1.0e6, 95.0);
	const int32 GaleId = Gale.Id;
	Gale.Air.FormUntil = State.Tick + SimTicks(Form->DurationS);
	Gale.Air.FormPhysical = Form->EvadePhysical;
	Gale.Air.FormMagic = Form->EvadeMagic;
	auto Throw = [&](const TCHAR* Family, int32 Count) -> int32
	{
		int32 Landed = 0;
		for (int32 Index = 0; Index < Count; ++Index)
		{
			FHit Hit;
			Hit.OwnerId = Player;
			Hit.ActivationId = State.NextId++;
			Hit.Damage = 1.0;
			Hit.Family = Family;
			Hit.Source = KernelData().PlayerSpawn;
			const FSimHitResult Result = ResolveHit(State, Live(State, GaleId), Hit);
			Landed += Result.bEvaded ? 0 : 1;
			bPass &= TestTrue(TEXT("an evaded hit does no damage"), !Result.bEvaded || Result.Damage == 0.0);
		}
		return Landed;
	};
	const int32 Magic = Throw(TEXT("magic"), 100);
	const int32 Physical = Throw(TEXT("physical"), 100);
	const int32 DrawsBefore = State.RandomLog.Num();
	const int32 Unblockable = Throw(TEXT("unblockable"), 100);
	bPass &= TestEqual(TEXT("magic hits that land"), Magic, MagicLand);
	bPass &= TestEqual(TEXT("physical hits that land"), Physical, PhysicalLand);
	bPass &= TestEqual(TEXT("unblockable is never evaded"), Unblockable, 100);
	bPass &= TestEqual(TEXT("and draws nothing"), State.RandomLog.Num(), DrawsBefore);
	bPass &= TestEqual(TEXT("evade events"), CountEvents(State, TEXT("evade"), GaleId), 200 - MagicLand - PhysicalLand);
	bPass &= Near(*this, TEXT("hp lost = hits landed"), 1.0e6 - Live(State, GaleId).Hp, static_cast<double>(MagicLand + PhysicalLand + 100));
	bPass &= TestEqual(TEXT("metrics count landed hits only"), Live(State, GaleId).Metrics.Hits, MagicLand + PhysicalLand + 100);
	// Outside the window nothing is drawn and everything lands.
	Live(State, GaleId).Air.FormUntil = State.Tick;
	const int32 Draws = State.RandomLog.Num();
	bPass &= TestEqual(TEXT("after the form all land"), Throw(TEXT("magic"), 10), 10);
	bPass &= TestEqual(TEXT("no draw after the form"), State.RandomLog.Num(), Draws);
	// A water actor never evades, whatever its air fields say.
	FActor& Water = Live(State, Player);
	Water.Air.FormUntil = State.Tick + 100;
	Water.Air.FormMagic = 1.0;
	FHit Hit;
	Hit.OwnerId = GaleId;
	Hit.ActivationId = State.NextId++;
	Hit.Damage = 1.0;
	Hit.Family = TEXT("magic");
	Hit.Source = Live(State, GaleId).Pos;
	bPass &= TestFalse(TEXT("a non-air actor does not evade"), ResolveHit(State, Water, Hit).bEvaded);
	return bPass;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaAirDeflect, "MageArena.Air.Deflect",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaAirDeflect::RunTest(const FString& Parameters)
{
	if (!Ready(*this))
	{
		return false;
	}
	bool bPass = true;
	// A water bolt (tier 0, then tier 3) flies at a tier-2 air mage who raises a fresh ward 3 ticks before impact
	// (inside the 0.15 s window), facing 45 deg off the bolt's line. The perfect redirects the bolt along that facing.
	for (const int32 BoltTier : {0, 3})
	{
		FArenaState State = CreateArena(5);
		const FSimVec Centre = KernelData().ArenaCentre;
		const int32 Player = AddMage(State, 0, FSimVec{Centre.X - 8.0, Centre.Y}, TEXT("Cassia")).Id;
		FActor& Gale = AddMage(State, 1, Centre, TEXT("Gale"));
		Gale.Air.bSchool = true;
		Gale.Tier = 2;
		const int32 GaleId = Gale.Id;
		FHit Hit;
		Hit.OwnerId = Player;
		Hit.ActivationId = State.NextId++;
		Hit.Damage = 10.0;
		Hit.Family = TEXT("magic");
		Hit.Tier = BoltTier;
		Hit.Source = FSimVec{Centre.X - 8.0, Centre.Y};
		// 12 m/s from 3 m out: 0.2 m a tick, contact at 3 - 0.5 = 2.5 m, on the 13th world step.
		SpawnProjectile(State, Hit, FSimVec{Centre.X - 3.0, Centre.Y}, FSimVec{1.0, 0.0}, 12.0, 10.0);
		const FSimVec Facing = SimUnit(FSimVec{-1.0, 1.0});
		const FSimVec Aim = SimAdd(Centre, Facing);
		int32 Deflected = 0;
		for (int32 Tick = 1; Tick <= 20; ++Tick)
		{
			FInputFrame Input = SimIdleInput(Aim);
			Input.bAbsorb = Tick >= 11;
			TMap<int32, FInputFrame> Inputs;
			Inputs.Add(GaleId, Input);
			StepArena(State, Inputs);
			Deflected = CountEvents(State, TEXT("deflect"), GaleId);
			if (CountEvents(State, TEXT("perfect"), GaleId) > 0)
			{
				break;
			}
		}
		const FActor& After = Live(State, GaleId);
		bPass &= TestEqual(*FString::Printf(TEXT("tier %d: perfect"), BoltTier), After.Metrics.Perfects, 1);
		const FProjectile* Back = State.Projectiles.FindByPredicate([&](const FProjectile& P) { return P.OwnerId == GaleId; });
		if (BoltTier <= 2)
		{
			bPass &= TestEqual(TEXT("one deflect"), Deflected, 1);
			bPass &= TestNotNull(TEXT("the deflected bolt flies"), Back);
			if (Back)
			{
				const FSimVec Dir = SimUnit(Back->Velocity);
				bPass &= Near(*this, TEXT("along the aim x"), Dir.X, Facing.X);
				bPass &= Near(*this, TEXT("along the aim y"), Dir.Y, Facing.Y);
				bPass &= Near(*this, TEXT("same speed"), SimLength(Back->Velocity), 12.0);
				bPass &= Near(*this, TEXT("same damage"), Back->Damage, 10.0);
				bPass &= TestTrue(TEXT("cannot be deflected or reflected again"), Back->bReflected);
				bPass &= TestEqual(TEXT("same tier"), Back->Tier, BoltTier);
			}
			bPass &= TestEqual(TEXT("deflect counted"), After.Air.Deflects, 1);
		}
		else
		{
			bPass &= TestEqual(TEXT("tier 3 above own tier 2: no deflect"), Deflected, 0);
			bPass &= TestNull(TEXT("nothing flies back"), Back);
		}
	}
	return bPass;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaAirTempestSignature, "MageArena.Air.TempestSignature",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaAirTempestSignature::RunTest(const FString& Parameters)
{
	if (!Ready(*this))
	{
		return false;
	}
	FVrRuleset Rules;
	FString Error;
	if (!TestTrue(TEXT("overlay loads"), LoadVrRuleset(Rules, Error, false)))
	{
		AddError(Error);
		return false;
	}
	const int32 Index = Rules.FindRival(TEXT("tiro"), 4, TEXT("final"), TEXT("air"));
	if (!TestTrue(TEXT("Lio is the air rival of the Tiro final"), Index != INDEX_NONE))
	{
		return false;
	}
	const FVrRival& Lio = Rules.Rivals[Index];
	bool bPass = TestEqual(TEXT("id lio"), Lio.Id, FString(TEXT("lio")));
	bPass &= TestEqual(TEXT("surge 1"), Lio.SurgeTiers, 1);
	bPass &= TestTrue(TEXT("phases 0.66 then 0.33 with Tempest Lance"), Lio.Phases.Num() == 2 && Lio.Phases[0].AtHpFraction == 0.66
		&& Lio.Phases[0].OpensWith.IsEmpty() && Lio.Phases[1].AtHpFraction == 0.33 && Lio.Phases[1].OpensWith == TEXT("air_tempest"));

	// The session's games: school mages on and the air final. Wave index 3 spawns Lio's air mage, bound to the rival.
	const FComposition* Preset = KernelData().Presets.FindByPredicate([](const FComposition& C) { return C.Name == KernelData().ReferencePreset; });
	FGames Games;
	if (!TestTrue(TEXT("final creates"), TryCreateGames(Games, 7, Preset, 3, false, true, &Rules, true)))
	{
		return false;
	}
	FVrRuleset& Live = Games.VrRules.GetValue();
	FActor* Rival = SimFindActor(Games.State, Live.RivalRuntime.ActorId);
	if (!TestNotNull(TEXT("Lio bound"), Rival))
	{
		return false;
	}
	bPass &= TestTrue(TEXT("Lio is an air mage"), Rival->Air.bSchool && !Rival->Fire.bSchool);
	bPass &= TestTrue(TEXT("labelled Gale"), Rival->Label.Contains(TEXT("Gale")));
	bPass &= TestEqual(TEXT("at competence 1.5"), Rival->MageAI.IsSet() ? Rival->MageAI->Competence : -1.0, 1.5);
	const int32 RivalId = Rival->Id;
	// Last break with mana 0 and tier 1: the grant ignores the tier gate and the mana cost.
	Rival->Hp = 0.30 * Rival->MaxHp;
	Rival->Mana = 0.0;
	Rival->Tier = 1;
	Rival->Pending.Reset();
	const FInputFrame Idle = SimIdleInput(Games.State.Actors[0].Pos);
	TMap<int32, FInputFrame> Inputs;
	Inputs.Add(RivalId, Idle);
	StepArena(Games.State, Inputs, &Live);
	Rival = SimFindActor(Games.State, RivalId);
	bPass &= TestEqual(TEXT("both thresholds break on one hit"), Live.RivalRuntime.PhasesBroken, 2);
	bPass &= TestTrue(TEXT("Tempest Lance starts on the break"), Rival->Pending.IsSet() && Rival->Pending->SpellId.IsSet()
		&& Rival->Pending->SpellId.GetValue() == TEXT("air_tempest") && Rival->Pending->Kind == TEXT("air"));
	bPass &= TestEqual(TEXT("grant tick"), Live.RivalRuntime.GrantTick, Games.State.Tick);
	bPass &= TestTrue(TEXT("no mana spent"), Rival->Mana >= 0.0 && Rival->Mana < 1.0);
	bPass &= TestEqual(TEXT("72-tick windup (1.20 s telegraph)"), Rival->Pending.IsSet() ? Rival->Pending->ReleaseTick - Rival->Pending->StartTick : -1, 72);
	bPass &= TestEqual(TEXT("tier cast event 4"), Games.State.Events.Last().Kind == TEXT("cast") ? static_cast<int32>(Games.State.Events.Last().Value) : -1, 4);
	return bPass;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaAirNothingElseMoves, "MageArena.Air.NothingElseMoves",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaAirNothingElseMoves::RunTest(const FString& Parameters)
{
	if (!Ready(*this))
	{
		return false;
	}
	bool bPass = true;
	FVrRuleset Rules;
	FString Error;
	if (!LoadVrRuleset(Rules, Error, false))
	{
		AddError(Error);
		return false;
	}
	const FComposition* Preset = KernelData().Presets.FindByPredicate([](const FComposition& C) { return C.Name == KernelData().ReferencePreset; });
	// The semifinal stays Brennic (fire) with the air final on; water proxies stay water; the final is fire without it.
	struct FSchoolCase
	{
		int32 Wave;
		bool bFire;
		bool bAirFinal;
		const TCHAR* Expected;
	};
	const FSchoolCase Cases[] = {
		{2, true, true, TEXT("fire")},
		{3, true, true, TEXT("air")},
		{3, true, false, TEXT("fire")},
		{3, false, true, TEXT("water")},
		{2, false, true, TEXT("water")},
	};
	for (const FSchoolCase& Case : Cases)
	{
		FGames Games;
		if (!TryCreateGames(Games, 3, Preset, Case.Wave, true, Case.bFire, &Rules, Case.bAirFinal))
		{
			AddError(TEXT("games did not create"));
			return false;
		}
		bPass &= TestEqual(*FString::Printf(TEXT("wave %d fire=%d air=%d"), Case.Wave, Case.bFire ? 1 : 0, Case.bAirFinal ? 1 : 0),
			GamesMageSchool(Games, Case.Wave), FString(Case.Expected));
		for (const FActor& Actor : Games.State.Actors)
		{
			if (Actor.Team != 0 && Actor.MageAI.IsSet())
			{
				const FString School = Actor.Air.bSchool ? TEXT("air") : (Actor.Fire.bSchool ? TEXT("fire") : TEXT("water"));
				bPass &= TestEqual(TEXT("spawned school"), School, FString(Case.Expected));
			}
		}
	}
	// A non-air actor's air fields never reach the hash: the fire and water digests (T19, scenarios-vr) stay as recorded.
	FGames Fire;
	TryCreateGames(Fire, 1, Preset, 2, true, true, &Rules);
	for (int32 Tick = 0; Tick < 120; ++Tick)
	{
		StepGames(Fire);
	}
	const FString Before = StateHash(Fire.State);
	for (FActor& Actor : Fire.State.Actors)
	{
		Actor.Air.Momentum = 42.0;
		Actor.Air.FormUntil = 99;
		Actor.Air.Cooldowns.Add({TEXT("air_bolt"), 5});
	}
	bPass &= TestEqual(TEXT("air fields on fire and water actors are outside the hash"), StateHash(Fire.State), Before);
	return bPass;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaAirMind, "MageArena.Air.Mind",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaAirMind::RunTest(const FString& Parameters)
{
	if (!Ready(*this))
	{
		return false;
	}
	bool bPass = true;
	auto Decide = [](FAirPair& Pair, double Level) -> FInputFrame
	{
		AttachMageAI(Live(Pair.State, Pair.Actor), Level, Pair.State.Tick);
		return MageInput(Pair.State, Live(Pair.State, Pair.Actor));
	};
	// Tier IV, Tempest legal, the target not rolling: Tempest Lance, without the spell draw.
	{
		FAirPair Pair = MakeAir(8.0);
		Live(Pair.State, Pair.Actor).Air.Momentum = 60.0;
		const int32 Draws = Pair.State.RandomLog.Num();
		const FInputFrame Input = Decide(Pair, 1.5);
		bPass &= TestTrue(TEXT("Tempest Lance at tier IV"), Input.bCast && Input.Slot == SlotOf(TEXT("air_tempest")));
		bool bSpellDraw = false;
		for (int32 Index = Draws; Index < Pair.State.RandomLog.Num(); ++Index)
		{
			bSpellDraw |= Pair.State.RandomLog[Index].Purpose.EndsWith(TEXT(" spell"));
		}
		bPass &= TestFalse(TEXT("no spell draw for the signature"), bSpellDraw);
	}
	// The same, with the target mid-roll: not Tempest.
	{
		FAirPair Pair = MakeAir(8.0);
		Live(Pair.State, Pair.Target).RollUntil = Pair.State.Tick + 10;
		const FInputFrame Input = Decide(Pair, 1.5);
		bPass &= TestTrue(TEXT("no Tempest at a blinking target"), !Input.bCast || Input.Slot != SlotOf(TEXT("air_tempest")));
	}
	// A bolt in flight at it, Momentum 50, tier 2: Air Form.
	{
		FAirPair Pair = MakeAir(8.0);
		FActor& Gale = Live(Pair.State, Pair.Actor);
		Gale.Tier = 2;
		Gale.Air.Momentum = 50.0;
		FHit Hit;
		Hit.OwnerId = Pair.Target;
		Hit.ActivationId = Pair.State.NextId++;
		Hit.Damage = 8.0;
		Hit.Family = TEXT("magic");
		SpawnProjectile(Pair.State, Hit, Live(Pair.State, Pair.Target).Pos, FSimVec{-1.0, 0.0}, 12.0, 12.0);
		const FInputFrame Input = Decide(Pair, 1.0);
		bPass &= TestTrue(TEXT("Air Form against a cast in flight"), Input.bCast && Input.Slot == SlotOf(TEXT("air_form")));
	}
	// Momentum 10, tier 2, nothing incoming: Slipstream, dashed along the move and not at the target.
	{
		FAirPair Pair = MakeAir(8.0);
		FActor& Gale = Live(Pair.State, Pair.Actor);
		Gale.Tier = 2;
		Gale.Air.Momentum = 10.0;
		const FInputFrame Input = Decide(Pair, 1.0);
		bPass &= TestTrue(TEXT("Slipstream below Air Form's 40"), Input.bCast && Input.Slot == SlotOf(TEXT("air_slipstream")));
		const FSimVec Dash = SimUnit(SimSub(Input.Aim, Live(Pair.State, Pair.Actor).Pos));
		const FSimVec AtTarget = SimUnit(SimSub(Live(Pair.State, Pair.Target).Pos, Live(Pair.State, Pair.Actor).Pos));
		bPass &= TestTrue(TEXT("the dash is not at the target"), SimDot(Dash, AtTarget) < 0.99);
	}
	// Level 2, tier 3, Momentum 60: Veering Bolt or Squall sorts first (Squall 11 x 4 = 44 over Veering 24).
	{
		FAirPair Pair = MakeAir(8.0);
		FActor& Gale = Live(Pair.State, Pair.Actor);
		Gale.Tier = 3;
		Gale.Air.Momentum = 60.0;
		const FInputFrame Input = Decide(Pair, 2.0);
		bPass &= TestTrue(TEXT("level 2 prefers Squall"), Input.bCast && Input.Slot == SlotOf(TEXT("air_squall")));
	}
	return bPass;
}

#endif
