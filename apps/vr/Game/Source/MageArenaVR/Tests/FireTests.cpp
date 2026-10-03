#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Kernel/ArenaKernel.h"
#include "Kernel/Enemies.h"
#include "Kernel/Fire.h"
#include "Kernel/Games.h"
#include "Kernel/Geometry.h"
#include "Kernel/MageAI.h"
#include "MageArenaVR.h"

#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"

#include <cmath>

// Hand literals from spells-fire.csv, schools.json combatIdentity, combat.json, stats.csv and runtime.json.
// The kernel is asserted against these numbers. Loaded fields are checked against the same literals first.

namespace
{
constexpr double BoltDamage = 8.0;
constexpr double BoltSpeed = 18.0;
constexpr double BoltMana = 4.0;
constexpr double BoltCooldownS = 0.30;
constexpr double BoltHeat = 2.0;
constexpr double FlickDamage = 20.0;
constexpr double FlickArcDeg = 60.0;
constexpr double FlickRangeM = 4.0;
constexpr double FlickHeat = 6.0;
constexpr double KindleCastS = 0.40;
constexpr double KindleHeat = 25.0;
constexpr double LanceDamage = 32.0;
constexpr double LanceCastS = 0.60;
constexpr double RingDamage = 18.0;
constexpr double RingRadiusM = 3.0;
constexpr double RingKnockM = 2.0;
constexpr double RingCastS = 0.30;
constexpr double RingHeat = 5.0;
constexpr double PyreDamage = 48.0;
constexpr double PyreRadiusM = 3.0;
constexpr double PyreCastS = 0.50;
constexpr double PyreTelegraphS = 0.80;
constexpr double PyreHeat = 8.0;
constexpr double CinderDamage = 6.0;
constexpr double CinderTickS = 0.50;
constexpr double CinderDurationS = 2.0;
constexpr double CinderDashM = 5.0;
constexpr double CinderHeat = 3.0;
constexpr double FurnaceCastS = 0.50;
constexpr double FurnaceLock = 100.0;
constexpr double FurnaceLockS = 6.0;
constexpr double FurnaceDrainMult = 2.0;
constexpr double SunfallDamage = 95.0;
constexpr double SunfallCastS = 0.80;
constexpr double SunfallShadowS = 1.20;
constexpr double SunfallRadiusM = 3.5;
constexpr double SunfallMana = 70.0;
constexpr double SunfallRangeM = 12.0;
constexpr double SunfallHeat = 10.0;
constexpr double WrathDamage = 14.0;
constexpr double WrathCastS = 0.20;
constexpr double WrathTickS = 0.25;
constexpr double WrathDurationS = 2.0;
constexpr double WrathHeat = 2.0;
constexpr double WrathMana = 60.0;
constexpr double HeatPerBolt = 2.0;
constexpr double HeatPerFlick = 6.0;
constexpr double HeatPerHitTaken = 12.0;
constexpr double HeatPerSecondNear = 3.0;
constexpr double HeatNearM = 3.0;
constexpr double HeatDecayPerSecond = 6.0;
constexpr double HeatIdleS = 2.0;
constexpr double HeatMax = 100.0;
constexpr double KindledAt = 50.0;
constexpr double KindledMult = 1.15;
constexpr double BlazingAt = 80.0;
constexpr double BlazingDamageMult = 1.3;
constexpr double BlazingDrainMult = 1.5;
constexpr double BlazingStaminaMult = 0.7;
constexpr double ManaRegenPerSecond = 7.5;
constexpr double MaxManaRank1 = 95.0;
constexpr double MaxHpRank1 = 95.0;
constexpr double StaffRangeM = 1.6;
constexpr double StaffWindupS = 0.25;
constexpr double StaffStamina = 12.0;
constexpr double StaffDamage = 6.0;
constexpr double WalkMps = 4.5;
constexpr double StaminaPerSecond = 20.0;
constexpr double StaminaDelayS = 0.8;
constexpr double AbsorbWindowS = 0.15;
constexpr double AbsorbReRaiseS = 0.12;
constexpr double AbsorbMagicReduction = 0.85;
constexpr double AbsorbUnblockableReduction = 0.0;
constexpr double DrainBase = 30.0;
constexpr double DrainPerRank = 3.0;
constexpr double ShieldHp = 70.0;
constexpr double ShieldFrontMult = 0.0;
constexpr int32 Samples = 120;

bool Ready(FAutomationTestBase& Test)
{
	if (KernelData().bReady)
	{
		return true;
	}
	Test.AddError(KernelData().Error.IsEmpty() ? TEXT("kernel data failed to load") : KernelData().Error);
	return false;
}

bool Near(FAutomationTestBase& Test, const TCHAR* What, double Actual, double Expected)
{
	const double Diff = std::abs(Actual - Expected);
	const double Scale = std::max(std::abs(Actual), std::abs(Expected));
	const bool bOk = Actual == Expected || Diff <= 1.0e-9 || Diff <= 1.0e-6 * Scale;
	if (!bOk)
	{
		Test.AddError(FString::Printf(TEXT("%s: expected %.17g got %.17g"), What, Expected, Actual));
	}
	return bOk;
}

void Advance(FArenaState& State, int32 Count)
{
	for (int32 Index = 0; Index < Count; ++Index)
	{
		StepArena(State);
	}
}

void StepOne(FArenaState& State, int32 ActorId, const FInputFrame& Input)
{
	TMap<int32, FInputFrame> Inputs;
	Inputs.Add(ActorId, Input);
	StepArena(State, Inputs);
}

void StepBoth(FArenaState& State, int32 First, const FInputFrame& FirstInput, int32 Second, const FInputFrame& SecondInput)
{
	TMap<int32, FInputFrame> Inputs;
	Inputs.Add(First, FirstInput);
	Inputs.Add(Second, SecondInput);
	StepArena(State, Inputs);
}

FInputFrame CastInput(const FSimVec& Aim, int32 Slot)
{
	FInputFrame Input = SimIdleInput(Aim);
	Input.Slot = Slot;
	Input.bCast = true;
	return Input;
}

FInputFrame AimHold(const FSimVec& Aim, bool bAbsorb)
{
	FInputFrame Input = SimIdleInput(Aim);
	Input.bAbsorb = bAbsorb;
	return Input;
}

void SetPool(FActor& Actor, double Hp, double Mana)
{
	Actor.MaxHp = Hp;
	Actor.Hp = Hp;
	Actor.MaxMana = Mana;
	Actor.Mana = Mana;
}

int32 SlotOf(const TCHAR* Id)
{
	const TArray<FFireSpell>& Spells = KernelData().FireSpells;
	for (int32 Index = 0; Index < Spells.Num(); ++Index)
	{
		if (Spells[Index].Id == Id)
		{
			return Index;
		}
	}
	return INDEX_NONE;
}

FActor& Live(FArenaState& State, int32 Id)
{
	return *SimFindActor(State, Id);
}

int32 CountKind(const FArenaState& State, const TCHAR* Kind, int32 ActorId)
{
	int32 Count = 0;
	for (const FArenaEvent& Event : State.Events)
	{
		if (Event.Kind == Kind && Event.ActorId == ActorId)
		{
			++Count;
		}
	}
	return Count;
}

bool DrewPurpose(const FArenaState& State, const FString& Purpose)
{
	for (const FRandomDraw& Draw : State.RandomLog)
	{
		if (Draw.Purpose == Purpose)
		{
			return true;
		}
	}
	return false;
}

bool ProjectileConnects(const FArenaState& State, int32 TargetId)
{
	const FActor* Target = SimFindActor(State, TargetId);
	if (!Target)
	{
		return false;
	}
	for (const FProjectile& Projectile : State.Projectiles)
	{
		const double Travel = std::min(SimLength(Projectile.Velocity) * SimDt(), Projectile.RemainingM);
		const FSimVec End = SimAdd(Projectile.Pos, SimScale(SimUnit(Projectile.Velocity), Travel));
		if (SimSegmentHit(Projectile.Pos, End, Target->Pos, Target->Radius + Projectile.Radius).IsSet())
		{
			return true;
		}
	}
	return false;
}

const FEnemySpec* SpecById(const TCHAR* Id)
{
	for (const FEnemySpec& Spec : KernelData().Enemies)
	{
		if (Spec.Id == Id)
		{
			return &Spec;
		}
	}
	return nullptr;
}

struct FFirePair
{
	FArenaState State;
	int32 Actor = 0;
	int32 Target = 0;
};

FFirePair MakeFire(double RangeM)
{
	FFirePair Pair;
	Pair.State = CreateArena(11);
	Pair.Actor = AddMage(Pair.State, 0, FSimVec{10.0, 10.0}, TEXT("Ember")).Id;
	Pair.Target = AddMage(Pair.State, 1, FSimVec{10.0 + RangeM, 10.0}, TEXT("Target")).Id;
	FActor& Actor = Live(Pair.State, Pair.Actor);
	Actor.Fire.bSchool = true;
	Actor.Tier = 4;
	Actor.LastUnlockTick = NeverTick;
	SetPool(Actor, 1000.0, 500.0);
	SetPool(Live(Pair.State, Pair.Target), 1000.0, 500.0);
	return Pair;
}

bool Literals(FAutomationTestBase& Test)
{
	const FKernelData& Data = KernelData();
	const TCHAR* Order[] = {
		TEXT("fire_bolt"), TEXT("fire_flick"), TEXT("fire_cinderstep"), TEXT("fire_kindle"), TEXT("fire_lance"),
		TEXT("fire_ring"), TEXT("fire_pyre"), TEXT("fire_furnace"), TEXT("fire_sunfall"), TEXT("fire_wrath")
	};
	bool bPass = Test.TestEqual(TEXT("water catalog stays 24"), Data.Spells.Num(), 24);
	bPass &= Test.TestEqual(TEXT("ten fire rows"), Data.FireSpells.Num(), 10);
	if (Data.FireSpells.Num() == 10)
	{
		for (int32 Index = 0; Index < 10; ++Index)
		{
			bPass &= Test.TestEqual(FString::Printf(TEXT("row %d"), Index), Data.FireSpells[Index].Id, FString(Order[Index]));
		}
	}
	const FFireSpell* Bolt = FindFireSpell(TEXT("fire_bolt"));
	const FFireSpell* Flick = FindFireSpell(TEXT("fire_flick"));
	const FFireSpell* Cinder = FindFireSpell(TEXT("fire_cinderstep"));
	const FFireSpell* Kindle = FindFireSpell(TEXT("fire_kindle"));
	const FFireSpell* Lance = FindFireSpell(TEXT("fire_lance"));
	const FFireSpell* Ring = FindFireSpell(TEXT("fire_ring"));
	const FFireSpell* Pyre = FindFireSpell(TEXT("fire_pyre"));
	const FFireSpell* Furnace = FindFireSpell(TEXT("fire_furnace"));
	const FFireSpell* Sun = FindFireSpell(TEXT("fire_sunfall"));
	const FFireSpell* Wrath = FindFireSpell(TEXT("fire_wrath"));
	bPass &= Test.TestNotNull(TEXT("bolt"), Bolt) && Test.TestNotNull(TEXT("flick"), Flick) && Test.TestNotNull(TEXT("cinder"), Cinder);
	bPass &= Test.TestNotNull(TEXT("kindle"), Kindle) && Test.TestNotNull(TEXT("lance"), Lance) && Test.TestNotNull(TEXT("ring"), Ring);
	bPass &= Test.TestNotNull(TEXT("pyre"), Pyre) && Test.TestNotNull(TEXT("furnace"), Furnace) && Test.TestNotNull(TEXT("sun"), Sun);
	bPass &= Test.TestNotNull(TEXT("wrath"), Wrath);
	if (!bPass)
	{
		return false;
	}
	bPass &= Near(Test, TEXT("bolt speed"), Bolt->SpeedMps, BoltSpeed);
	bPass &= Near(Test, TEXT("bolt damage"), Bolt->Damage, BoltDamage);
	bPass &= Near(Test, TEXT("bolt mana"), Bolt->Mana, BoltMana);
	bPass &= Near(Test, TEXT("bolt cooldown"), Bolt->CooldownS, BoltCooldownS);
	bPass &= Near(Test, TEXT("bolt heat"), Bolt->HeatAmount, BoltHeat);
	bPass &= Test.TestEqual(TEXT("bolt family"), Bolt->Family, FString(TEXT("magic")));
	bPass &= Near(Test, TEXT("flick arc"), Flick->ArcDeg, FlickArcDeg);
	bPass &= Near(Test, TEXT("flick range"), Flick->RangeM, FlickRangeM);
	bPass &= Near(Test, TEXT("flick damage"), Flick->Damage, FlickDamage);
	bPass &= Near(Test, TEXT("flick heat"), Flick->HeatAmount, FlickHeat);
	bPass &= Near(Test, TEXT("kindle cast"), Kindle->CastS, KindleCastS);
	bPass &= Near(Test, TEXT("kindle heat"), Kindle->InstantHeat, KindleHeat);
	bPass &= Test.TestTrue(TEXT("kindle rooted"), Kindle->bRootDuringCast);
	bPass &= Near(Test, TEXT("lance damage"), Lance->Damage, LanceDamage);
	bPass &= Near(Test, TEXT("lance cast"), Lance->CastS, LanceCastS);
	bPass &= Test.TestTrue(TEXT("lance pierces"), Lance->bPierceShields);
	bPass &= Near(Test, TEXT("ring radius"), Ring->RadiusM, RingRadiusM);
	bPass &= Near(Test, TEXT("ring knock"), Ring->KnockbackM, RingKnockM);
	bPass &= Near(Test, TEXT("ring damage"), Ring->Damage, RingDamage);
	bPass &= Near(Test, TEXT("pyre radius"), Pyre->RadiusM, PyreRadiusM);
	bPass &= Near(Test, TEXT("pyre cast"), Pyre->CastS, PyreCastS);
	bPass &= Near(Test, TEXT("pyre telegraph"), Pyre->TelegraphS, PyreTelegraphS);
	bPass &= Test.TestFalse(TEXT("pyre is not sequential"), Pyre->bSequentialTelegraph);
	bPass &= Near(Test, TEXT("cinder dash"), Cinder->DashM, CinderDashM);
	bPass &= Near(Test, TEXT("cinder tick"), Cinder->TickS, CinderTickS);
	bPass &= Near(Test, TEXT("cinder duration"), Cinder->DurationS, CinderDurationS);
	bPass &= Near(Test, TEXT("cinder damage"), Cinder->Damage, CinderDamage);
	bPass &= Near(Test, TEXT("furnace lock"), Furnace->HeatLockValue, FurnaceLock);
	bPass &= Near(Test, TEXT("furnace seconds"), Furnace->HeatLockS, FurnaceLockS);
	bPass &= Near(Test, TEXT("furnace drain"), Furnace->AbsorbDrainMult, FurnaceDrainMult);
	bPass &= Near(Test, TEXT("sunfall damage"), Sun->Damage, SunfallDamage);
	bPass &= Near(Test, TEXT("sunfall cast"), Sun->CastS, SunfallCastS);
	bPass &= Near(Test, TEXT("sunfall shadow"), Sun->TelegraphS, SunfallShadowS);
	bPass &= Near(Test, TEXT("sunfall radius"), Sun->RadiusM, SunfallRadiusM);
	bPass &= Near(Test, TEXT("sunfall mana"), Sun->Mana, SunfallMana);
	bPass &= Near(Test, TEXT("sunfall range"), Sun->RangeM, SunfallRangeM);
	bPass &= Near(Test, TEXT("sunfall heat"), Sun->HeatAmount, SunfallHeat);
	bPass &= Test.TestTrue(TEXT("sunfall sequential"), Sun->bSequentialTelegraph);
	bPass &= Test.TestEqual(TEXT("sunfall family"), Sun->Family, FString(TEXT("unblockable")));
	bPass &= Near(Test, TEXT("wrath damage"), Wrath->Damage, WrathDamage);
	bPass &= Near(Test, TEXT("wrath cast"), Wrath->CastS, WrathCastS);
	bPass &= Near(Test, TEXT("wrath tick"), Wrath->TickS, WrathTickS);
	bPass &= Near(Test, TEXT("wrath duration"), Wrath->DurationS, WrathDurationS);
	bPass &= Near(Test, TEXT("wrath heat"), Wrath->HeatAmount, WrathHeat);
	bPass &= Near(Test, TEXT("wrath mana"), Wrath->Mana, WrathMana);
	bPass &= Test.TestTrue(TEXT("wrath first tick only"), Wrath->bPerfectOnlyFirstTick);
	bPass &= Near(Test, TEXT("per bolt"), Data.Heat.PerBoltHit, HeatPerBolt);
	bPass &= Near(Test, TEXT("per flick"), Data.Heat.PerFlickTarget, HeatPerFlick);
	bPass &= Near(Test, TEXT("per hit taken"), Data.Heat.PerHitTakenUnabsorbed, HeatPerHitTaken);
	bPass &= Near(Test, TEXT("per second near"), Data.Heat.PerSecondPerEnemyWithin, HeatPerSecondNear);
	bPass &= Near(Test, TEXT("near metres"), Data.Heat.EnemyWithinM, HeatNearM);
	bPass &= Near(Test, TEXT("decay rate"), Data.Heat.DecayPerSecond, HeatDecayPerSecond);
	bPass &= Near(Test, TEXT("decay delay"), Data.Heat.DecayAfterNoGainS, HeatIdleS);
	bPass &= Near(Test, TEXT("heat max"), Data.Heat.Max, HeatMax);
	bPass &= Test.TestEqual(TEXT("two thresholds"), Data.Heat.Thresholds.Num(), 2);
	if (Data.Heat.Thresholds.Num() == 2)
	{
		bPass &= Near(Test, TEXT("kindled at"), Data.Heat.Thresholds[0].At, KindledAt);
		bPass &= Near(Test, TEXT("kindled mult"), Data.Heat.Thresholds[0].SpellDamageMult, KindledMult);
		bPass &= Test.TestFalse(TEXT("kindled has no drain"), Data.Heat.Thresholds[0].bAbsorbDrain);
		bPass &= Near(Test, TEXT("blazing at"), Data.Heat.Thresholds[1].At, BlazingAt);
		bPass &= Near(Test, TEXT("blazing damage"), Data.Heat.Thresholds[1].SpellDamageMult, BlazingDamageMult);
		bPass &= Near(Test, TEXT("blazing drain"), Data.Heat.Thresholds[1].AbsorbDrainMult, BlazingDrainMult);
		bPass &= Near(Test, TEXT("blazing stamina"), Data.Heat.Thresholds[1].StaminaRegenMult, BlazingStaminaMult);
	}
	bPass &= Near(Test, TEXT("mana regen"), Data.ManaRegenBase + Data.ManaRegenPerRank, ManaRegenPerSecond);
	bPass &= Near(Test, TEXT("max mana"), Data.ManaBase + Data.ManaPerRank, MaxManaRank1);
	bPass &= Near(Test, TEXT("max hp"), Data.HpBase + Data.HpPerRank, MaxHpRank1);
	bPass &= Near(Test, TEXT("staff range"), Data.StaffRangeM, StaffRangeM);
	bPass &= Near(Test, TEXT("staff windup"), Data.StaffWindupS, StaffWindupS);
	bPass &= Near(Test, TEXT("staff stamina"), Data.StaffStaminaCost, StaffStamina);
	bPass &= Near(Test, TEXT("staff damage"), Data.StaffDamage, StaffDamage);
	bPass &= Near(Test, TEXT("walk"), Data.WalkMps, WalkMps);
	bPass &= Near(Test, TEXT("stamina regen"), Data.StaminaRegenPerSecond, StaminaPerSecond);
	bPass &= Near(Test, TEXT("stamina delay"), Data.StaminaRegenDelayS, StaminaDelayS);
	bPass &= Near(Test, TEXT("absorb window"), Data.Absorb.WindowS, AbsorbWindowS);
	bPass &= Near(Test, TEXT("re-raise"), Data.Absorb.MinReleaseS, AbsorbReRaiseS);
	bPass &= Near(Test, TEXT("magic reduction"), Data.Absorb.ReductionMagic, AbsorbMagicReduction);
	bPass &= Near(Test, TEXT("unblockable reduction"), Data.Absorb.ReductionUnblockable, AbsorbUnblockableReduction);
	bPass &= Near(Test, TEXT("drain base"), Data.Absorb.DrainBase, DrainBase);
	bPass &= Near(Test, TEXT("drain per rank"), Data.Absorb.DrainSubtractPerRank, DrainPerRank);
	bPass &= Test.TestEqual(TEXT("bolt cooldown ticks"), SimTicks(BoltCooldownS), 18);
	bPass &= Test.TestEqual(TEXT("idle ticks"), SimTicks(HeatIdleS), 120);
	bPass &= Test.TestEqual(TEXT("kindle ticks"), SimTicks(KindleCastS), 24);
	bPass &= Test.TestEqual(TEXT("furnace cast ticks"), SimTicks(FurnaceCastS), 30);
	bPass &= Test.TestEqual(TEXT("furnace lock ticks"), SimTicks(FurnaceLockS), 360);
	bPass &= Test.TestEqual(TEXT("sunfall cast ticks"), SimTicks(SunfallCastS), 48);
	bPass &= Test.TestEqual(TEXT("sunfall shadow ticks"), SimTicks(SunfallShadowS), 72);
	bPass &= Test.TestEqual(TEXT("wrath windup ticks"), SimTicks(WrathCastS), 12);
	bPass &= Test.TestEqual(TEXT("wrath interval ticks"), SimTicks(WrathTickS), 15);
	bPass &= Test.TestEqual(TEXT("wrath duration ticks"), SimTicks(WrathDurationS), 120);
	bPass &= Test.TestEqual(TEXT("cinder interval ticks"), SimTicks(CinderTickS), 30);
	bPass &= Test.TestEqual(TEXT("window ticks"), SimTicks(AbsorbWindowS), 9);
	bPass &= Test.TestEqual(TEXT("reraise ticks"), SimTicks(AbsorbReRaiseS), 8);
	bPass &= Test.TestEqual(TEXT("staff windup ticks"), SimTicks(StaffWindupS), 15);
	bPass &= Test.TestEqual(TEXT("stamina delay ticks"), SimTicks(StaminaDelayS), 48);
	bPass &= Test.TestEqual(TEXT("sim hz"), static_cast<int32>(Data.SimStepHz), 60);
	return bPass;
}

struct FDuelResult
{
	FString Phase;
	FString Outcome;
	int32 Ticks = 0;
	double DurationS = 0.0;
	FString Hash;
	double Hp = 0.0;
	double Mana = 0.0;
	int32 SunfallCast = 0;
	int32 SunfallRelease = 0;
	bool bSunfall = false;
	int32 WavesCleared = 0;
	FString Kind;
};

FString MapOutcome(const FString& Phase)
{
	if (Phase == TEXT("lost"))
	{
		return TEXT("loss");
	}
	if (Phase == TEXT("intermission") || Phase == TEXT("complete"))
	{
		return TEXT("win");
	}
	return TEXT("timeout");
}

bool RunDuel(FAutomationTestBase& Test, uint32 Seed, int32 Wave, FDuelResult& Out)
{
	const FKernelData& Data = KernelData();
	const FComposition* Preset = nullptr;
	for (const FComposition& Candidate : Data.Presets)
	{
		if (Candidate.Name == Data.ReferencePreset)
		{
			Preset = &Candidate;
			break;
		}
	}
	if (!Test.TestNotNull(TEXT("reference preset"), Preset))
	{
		return false;
	}
	FGames Games;
	if (!Test.TestTrue(TEXT("fire duel creates"), TryCreateGames(Games, Seed, Preset, Wave, true, true)))
	{
		return false;
	}
	if (!Test.TestTrue(TEXT("ember spawn"), Games.SpawnLog.Num() > 0 && Games.SpawnLog[0].Kind.StartsWith(TEXT("Ember mage"))))
	{
		return false;
	}
	const int32 Limit = SimTicks(Data.FightTimeoutS);
	while (Games.Phase == TEXT("active") && Games.State.Tick < Limit)
	{
		StepGames(Games);
	}
	const FActor* Player = SimFindActor(Games.State, Games.PlayerId);
	if (!Test.TestNotNull(TEXT("player"), Player))
	{
		return false;
	}
	Out.Phase = Games.Phase;
	Out.Outcome = MapOutcome(Games.Phase);
	Out.Ticks = Games.State.Tick;
	Out.DurationS = SimSeconds(Games.State.Tick);
	Out.Hash = StateHash(Games.State);
	Out.Hp = Player->Hp;
	Out.Mana = Player->Mana;
	Out.WavesCleared = Games.WavesCleared;
	Out.Kind = Games.SpawnLog[0].Kind;
	for (const FActor& Actor : Games.State.Actors)
	{
		if (Actor.Fire.bSchool)
		{
			Out.bSunfall = Actor.Fire.bSunfallLoosed;
			Out.SunfallCast = Actor.Fire.SunfallCastTick;
			Out.SunfallRelease = Actor.Fire.SunfallTick;
		}
	}
	return true;
}

FString ScenarioPath(const TCHAR* Name)
{
	return FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT("../data/scenarios-vr"), Name));
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaFireHeat, "MageArena.Fire.Heat",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaFireHeat::RunTest(const FString& Parameters)
{
	if (!Ready(*this) || !Literals(*this))
	{
		return false;
	}
	bool bPass = true;

	{
		FFirePair Pair = MakeFire(6.0);
		const int32 Slot = SlotOf(TEXT("fire_bolt"));
		const FSimVec Aim = Live(Pair.State, Pair.Target).Pos;
		int32 Hits = 0;
		int32 Guard = 0;
		while (Hits < 5 && Guard++ < 400)
		{
			const FActor& Actor = Live(Pair.State, Pair.Actor);
			const bool bFlying = Pair.State.Projectiles.Num() > 0;
			const bool bReady = !bFlying && Pair.State.Tick + 1 >= FireCooldownUntil(Actor, TEXT("fire_bolt"));
			if (bReady)
			{
				StepOne(Pair.State, Pair.Actor, CastInput(Aim, Slot));
			}
			else
			{
				Advance(Pair.State, 1);
			}
			Hits = CountKind(Pair.State, TEXT("hit"), Pair.Target);
		}
		const FActor& Actor = Live(Pair.State, Pair.Actor);
		bPass &= TestEqual(TEXT("five bolt hits"), Hits, 5);
		bPass &= TestEqual(TEXT("no bolt left in the air"), Pair.State.Projectiles.Num(), 0);
		bPass &= Near(*this, TEXT("five bolts are 10 heat, not 4 and not 20"), Actor.Fire.Heat, 5.0 * HeatPerBolt);
		const int32 Gain = Actor.Fire.LastGainTick;
		while (Pair.State.Tick < Gain + SimTicks(HeatIdleS) - 1)
		{
			Advance(Pair.State, 1);
		}
		bPass &= Near(*this, TEXT("idle below 2 s does not decay"), Live(Pair.State, Pair.Actor).Fire.Heat, 10.0);
		Advance(Pair.State, 60);
		bPass &= Near(*this, TEXT("60 decay ticks of 6 per second leave 4"), Live(Pair.State, Pair.Actor).Fire.Heat, 4.0);
	}

	{
		FArenaState State = CreateArena(2);
		const int32 ActorId = AddMage(State, 0, FSimVec{10.0, 10.0}, TEXT("Ember")).Id;
		FActor& Actor = Live(State, ActorId);
		Actor.Fire.bSchool = true;
		Actor.Tier = 4;
		const int32 Slot = SlotOf(TEXT("fire_kindle"));
		StepOne(State, ActorId, CastInput(FSimVec{11.0, 10.0}, Slot));
		bPass &= TestEqual(TEXT("kindle releases on tick 25"), Live(State, ActorId).Pending.IsSet() ? Live(State, ActorId).Pending->ReleaseTick : 0, 1 + SimTicks(KindleCastS));
		Advance(State, SimTicks(KindleCastS));
		bPass &= TestFalse(TEXT("kindle finished"), Live(State, ActorId).Pending.IsSet());
		bPass &= Near(*this, TEXT("kindle pays 25"), Live(State, ActorId).Fire.Heat, KindleHeat);
		const int32 Gain = Live(State, ActorId).Fire.LastGainTick;
		bPass &= TestEqual(TEXT("kindle gain tick"), Gain, 1 + SimTicks(KindleCastS));
		while (State.Tick < Gain + SimTicks(HeatIdleS) - 1)
		{
			Advance(State, 1);
		}
		bPass &= Near(*this, TEXT("kindle holds through the idle"), Live(State, ActorId).Fire.Heat, KindleHeat);
		Advance(State, 60);
		bPass &= Near(*this, TEXT("kindle decays 6"), Live(State, ActorId).Fire.Heat, KindleHeat - HeatDecayPerSecond);
		Live(State, ActorId).Fire.Heat = 90.0;
		Live(State, ActorId).Fire.LastGainTick = State.Tick;
		Live(State, ActorId).Fire.Cooldowns.Reset();
		StepOne(State, ActorId, CastInput(FSimVec{11.0, 10.0}, Slot));
		Advance(State, SimTicks(KindleCastS));
		bPass &= Near(*this, TEXT("heat clamps at 100"), Live(State, ActorId).Fire.Heat, HeatMax);
	}

	{
		FArenaState State = CreateArena(3);
		const int32 ActorId = AddMage(State, 0, FSimVec{10.0, 10.0}, TEXT("Ember")).Id;
		const int32 NearId = AddMage(State, 1, FSimVec{12.0, 10.0}, TEXT("Near")).Id;
		const int32 OtherId = AddMage(State, 1, FSimVec{10.0, 12.0}, TEXT("Other")).Id;
		Live(State, ActorId).Fire.bSchool = true;
		Advance(State, 60);
		bPass &= Near(*this, TEXT("two enemies inside 3 m add 6 in one second"), Live(State, ActorId).Fire.Heat, 2.0 * HeatPerSecondNear);
		Live(State, NearId).Pos = FSimVec{30.0, 10.0};
		Live(State, OtherId).Pos = FSimVec{30.0, 12.0};
		const double Held = Live(State, ActorId).Fire.Heat;
		const int32 Stamp = State.Tick;
		while (State.Tick < Stamp + SimTicks(HeatIdleS) - 1)
		{
			Advance(State, 1);
		}
		bPass &= Near(*this, TEXT("leaving range starts the idle, not an instant drop"), Live(State, ActorId).Fire.Heat, Held);
	}

	{
		FArenaState State = CreateArena(4);
		const int32 ActorId = AddMage(State, 0, FSimVec{10.0, 10.0}, TEXT("Ember")).Id;
		AddMage(State, 1, FSimVec{12.0, 10.0}, TEXT("Near"));
		Live(State, ActorId).Fire.bSchool = true;
		Advance(State, 60);
		bPass &= Near(*this, TEXT("one enemy inside 3 m adds 3, so the rule is per enemy"), Live(State, ActorId).Fire.Heat, HeatPerSecondNear);
	}

	{
		FArenaState State = CreateArena(5);
		const int32 ActorId = AddMage(State, 0, FSimVec{10.0, 10.0}, TEXT("Ember")).Id;
		const int32 TargetId = AddMage(State, 1, FSimVec{20.0, 10.0}, TEXT("Target")).Id;
		FActor& Actor = Live(State, ActorId);
		Actor.Fire.bSchool = true;
		Actor.Tier = 4;
		SetPool(Actor, 1000.0, 500.0);
		StepOne(State, ActorId, CastInput(FSimVec{11.0, 10.0}, SlotOf(TEXT("fire_furnace"))));
		Advance(State, SimTicks(FurnaceCastS));
		bPass &= Near(*this, TEXT("furnace locks at 100"), Live(State, ActorId).Fire.Heat, FurnaceLock);
		bPass &= TestEqual(TEXT("lock runs through tick 390"), Live(State, ActorId).Fire.LockUntil, 31 + SimTicks(FurnaceLockS));
		FHit Gift;
		Gift.OwnerId = ActorId;
		Gift.ActivationId = 1;
		Gift.Damage = 0.0;
		Gift.Family = TEXT("magic");
		Gift.HeatOnHit = 50.0;
		Gift.Source = FSimVec{20.0, 10.0};
		ResolveHit(State, Live(State, TargetId), Gift);
		bPass &= Near(*this, TEXT("gains during the lock are discarded"), Live(State, ActorId).Fire.Heat, FurnaceLock);
		while (State.Tick < 390)
		{
			Advance(State, 1);
		}
		bPass &= Near(*this, TEXT("tick 390 is still locked"), Live(State, ActorId).Fire.Heat, FurnaceLock);
		Advance(State, 1);
		bPass &= TestEqual(TEXT("lock ends on 391"), State.Tick, 391);
		bPass &= Near(*this, TEXT("the idle already elapsed, so decay starts the same tick"), Live(State, ActorId).Fire.Heat, FurnaceLock - HeatDecayPerSecond / 60.0);
	}

	{
		FFirePair Pair = MakeFire(6.0);
		FActor& Target = Live(Pair.State, Pair.Target);
		Target.Fire.bSchool = true;
		FHit Blow;
		Blow.OwnerId = Pair.Actor;
		Blow.ActivationId = 7;
		Blow.Damage = 10.0;
		Blow.Family = TEXT("magic");
		Blow.Source = Live(Pair.State, Pair.Actor).Pos;
		ResolveHit(Pair.State, Target, Blow);
		bPass &= Near(*this, TEXT("unabsorbed hit taken pays 12"), Target.Fire.Heat, HeatPerHitTaken);
		Target.bAbsorb = true;
		Target.AbsorbFreshTick = Pair.State.Tick;
		Target.Facing = FSimVec{-1.0, 0.0};
		const double Before = Target.Fire.Heat;
		FHit Covered = Blow;
		Covered.ActivationId = 8;
		ResolveHit(Pair.State, Target, Covered);
		bPass &= Near(*this, TEXT("a perfect absorb pays no extra heat"), Target.Fire.Heat, Before);
		bPass &= Near(*this, TEXT("perfect still negates the blow"), Target.Hp, 1000.0 - 10.0);
	}
	return bPass;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaFireThresholds, "MageArena.Fire.Thresholds",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaFireThresholds::RunTest(const FString& Parameters)
{
	if (!Ready(*this) || !Literals(*this))
	{
		return false;
	}
	bool bPass = true;
	const int32 Slot = SlotOf(TEXT("fire_bolt"));
	auto Snapshot = [&](double Heat, const TCHAR* What, double Expected)
	{
		FFirePair Pair = MakeFire(6.0);
		FActor& Actor = Live(Pair.State, Pair.Actor);
		Actor.Fire.Heat = Heat;
		StepOne(Pair.State, Pair.Actor, CastInput(Live(Pair.State, Pair.Target).Pos, Slot));
		const bool bShot = TestTrue(What, Pair.State.Projectiles.Num() == 1);
		if (bShot)
		{
			return Near(*this, What, Pair.State.Projectiles[0].Damage, Expected);
		}
		return false;
	};
	bPass &= Snapshot(49.0, TEXT("below kindled the bolt is 8"), BoltDamage);
	bPass &= Snapshot(KindledAt, TEXT("kindled bolt is 8 * 1.15"), BoltDamage * KindledMult);
	bPass &= Snapshot(BlazingAt, TEXT("blazing bolt is 8 * 1.3"), BoltDamage * BlazingDamageMult);
	bPass &= Snapshot(HeatMax, TEXT("blazing replaces kindled, it does not stack"), BoltDamage * BlazingDamageMult);

	{
		FArenaState State = CreateArena(6);
		const int32 ActorId = AddMage(State, 0, FSimVec{10.0, 10.0}, TEXT("Ember")).Id;
		FActor& Actor = Live(State, ActorId);
		Actor.Fire.bSchool = true;
		Actor.Fire.Heat = BlazingAt;
		Actor.Stamina = 0.0;
		Actor.StaminaUsedTick = 0;
		Advance(State, SimTicks(StaminaDelayS));
		bPass &= Near(*this, TEXT("one blazing stamina tick is 20/60 * 0.7"), Actor.Stamina, StaminaPerSecond / 60.0 * BlazingStaminaMult);
	}
	{
		FArenaState State = CreateArena(7);
		const int32 ActorId = AddMage(State, 0, FSimVec{10.0, 10.0}, TEXT("Ember")).Id;
		FActor& Actor = Live(State, ActorId);
		Actor.Fire.bSchool = true;
		Actor.Stamina = 0.0;
		Actor.StaminaUsedTick = 0;
		Advance(State, SimTicks(StaminaDelayS));
		bPass &= Near(*this, TEXT("cold stamina tick is 20/60"), Actor.Stamina, StaminaPerSecond / 60.0);
	}
	{
		FArenaState State = CreateArena(8);
		const int32 ActorId = AddMage(State, 0, FSimVec{10.0, 10.0}, TEXT("Ember")).Id;
		FActor& Actor = Live(State, ActorId);
		Actor.Fire.bSchool = true;
		Actor.Fire.Heat = BlazingAt;
		SetPool(Actor, 1000.0, 500.0);
		const FSimVec Aim{11.0, 10.0};
		for (int32 Index = 0; Index < 60; ++Index)
		{
			StepOne(State, ActorId, AimHold(Aim, true));
		}
		const double Expected = 60.0 * (DrainBase - DrainPerRank) * BlazingDrainMult / 60.0;
		bPass &= Near(*this, TEXT("blazing absorb drain over 1 s is 27 * 1.5"), Actor.Metrics.ManaDrained, Expected);
	}
	{
		FArenaState State = CreateArena(9);
		const int32 ActorId = AddMage(State, 0, FSimVec{10.0, 10.0}, TEXT("Ember")).Id;
		FActor& Actor = Live(State, ActorId);
		Actor.Fire.bSchool = true;
		Actor.Tier = 4;
		SetPool(Actor, 1000.0, 500.0);
		StepOne(State, ActorId, CastInput(FSimVec{11.0, 10.0}, SlotOf(TEXT("fire_furnace"))));
		Advance(State, SimTicks(FurnaceCastS));
		const double Drained = Live(State, ActorId).Metrics.ManaDrained;
		for (int32 Index = 0; Index < 60; ++Index)
		{
			StepOne(State, ActorId, AimHold(FSimVec{11.0, 10.0}, true));
		}
		const double Expected = 60.0 * (DrainBase - DrainPerRank) * BlazingDrainMult * FurnaceDrainMult / 60.0;
		bPass &= Near(*this, TEXT("furnace doubles the blazing drain, 27 * 3"), Live(State, ActorId).Metrics.ManaDrained - Drained, Expected);
		bPass &= TestTrue(TEXT("still inside the lock"), State.Tick < Live(State, ActorId).Fire.LockUntil);
	}
	return bPass;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaFireGating, "MageArena.Fire.Gating",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaFireGating::RunTest(const FString& Parameters)
{
	if (!Ready(*this) || !Literals(*this))
	{
		return false;
	}
	bool bPass = true;
	{
		FFirePair Pair = MakeFire(6.0);
		FActor& Actor = Live(Pair.State, Pair.Actor);
		Actor.Mana = 3.0;
		const double Before = Actor.Mana;
		StepOne(Pair.State, Pair.Actor, CastInput(Live(Pair.State, Pair.Target).Pos, SlotOf(TEXT("fire_bolt"))));
		bPass &= TestFalse(TEXT("bolt below 4 mana does not cast"), Live(Pair.State, Pair.Actor).Pending.IsSet());
		bPass &= TestEqual(TEXT("no cast metric"), Live(Pair.State, Pair.Actor).Metrics.Casts, 0);
		bPass &= Near(*this, TEXT("refused bolt only regens"), Live(Pair.State, Pair.Actor).Mana, Before + ManaRegenPerSecond / 60.0);
	}
	{
		FFirePair Pair = MakeFire(6.0);
		FActor& Actor = Live(Pair.State, Pair.Actor);
		Actor.Mana = 50.0;
		const FSimVec Aim = Live(Pair.State, Pair.Target).Pos;
		const int32 Slot = SlotOf(TEXT("fire_bolt"));
		StepOne(Pair.State, Pair.Actor, CastInput(Aim, Slot));
		bPass &= TestEqual(TEXT("cooldown is 18 ticks out"), FireCooldownUntil(Live(Pair.State, Pair.Actor), TEXT("fire_bolt")), 1 + SimTicks(BoltCooldownS));
		StepOne(Pair.State, Pair.Actor, CastInput(Aim, Slot));
		bPass &= TestEqual(TEXT("second press inside the cooldown does not cast"), Live(Pair.State, Pair.Actor).Metrics.Casts, 1);
		bPass &= Near(*this, TEXT("mana paid the bolt once"), Live(Pair.State, Pair.Actor).Mana,
			50.0 + 2.0 * ManaRegenPerSecond / 60.0 - BoltMana);
	}
	{
		FFirePair Pair = MakeFire(1.0);
		FActor& Actor = Live(Pair.State, Pair.Actor);
		const double Stamina = Actor.Stamina;
		Actor.Mana = 400.0;
		StepOne(Pair.State, Pair.Actor, CastInput(Live(Pair.State, Pair.Target).Pos, 0));
		bPass &= TestTrue(TEXT("bolt inside 1.6 m is a staff strike"), Live(Pair.State, Pair.Actor).Pending.IsSet()
			&& Live(Pair.State, Pair.Actor).Pending->Kind == TEXT("staff"));
		bPass &= Near(*this, TEXT("staff spends stamina 12"), Live(Pair.State, Pair.Actor).Stamina, Stamina - StaffStamina);
		bPass &= Near(*this, TEXT("staff does not spend bolt mana"), Live(Pair.State, Pair.Actor).Mana, 400.0 + ManaRegenPerSecond / 60.0);
		bPass &= Near(*this, TEXT("the close bolt does not pay bolt heat"), Live(Pair.State, Pair.Actor).Fire.Heat, HeatPerSecondNear / 60.0);
	}
	{
		FFirePair Pair = MakeFire(6.0);
		const FSimVec Aim = Live(Pair.State, Pair.Target).Pos;
		FActor& Target = Live(Pair.State, Pair.Target);
		Target.Facing = FSimVec{-1.0, 0.0};
		for (int32 Index = 0; Index < SimTicks(AbsorbWindowS) + 2; ++Index)
		{
			StepOne(Pair.State, Pair.Target, AimHold(Live(Pair.State, Pair.Actor).Pos, true));
		}
		bPass &= TestTrue(TEXT("ward is up and stale"), Live(Pair.State, Pair.Target).bAbsorb
			&& Pair.State.Tick - Live(Pair.State, Pair.Target).AbsorbFreshTick > SimTicks(AbsorbWindowS));
		bool bCast = false;
		int32 Guard = 0;
		while (Guard++ < 80 && (Pair.State.Projectiles.Num() > 0 || !bCast))
		{
			FInputFrame Ward = AimHold(Live(Pair.State, Pair.Actor).Pos, true);
			if (!bCast)
			{
				StepBoth(Pair.State, Pair.Actor, CastInput(Aim, SlotOf(TEXT("fire_bolt"))), Pair.Target, Ward);
				bCast = true;
			}
			else
			{
				StepOne(Pair.State, Pair.Target, Ward);
			}
		}
		bPass &= Near(*this, TEXT("a held ward chips a bolt to 8 * 0.15"), 1000.0 - Live(Pair.State, Pair.Target).Hp, BoltDamage * (1.0 - AbsorbMagicReduction));
	}
	{
		FFirePair Pair = MakeFire(6.0);
		const FSimVec Aim = Live(Pair.State, Pair.Target).Pos;
		Live(Pair.State, Pair.Target).Facing = FSimVec{-1.0, 0.0};
		StepOne(Pair.State, Pair.Actor, CastInput(Aim, SlotOf(TEXT("fire_bolt"))));
		int32 Guard = 0;
		while (Guard++ < 80 && Live(Pair.State, Pair.Target).Hp == 1000.0)
		{
			if (ProjectileConnects(Pair.State, Pair.Target))
			{
				StepOne(Pair.State, Pair.Target, AimHold(Live(Pair.State, Pair.Actor).Pos, true));
				break;
			}
			Advance(Pair.State, 1);
		}
		bPass &= TestEqual(TEXT("fresh ward perfects the bolt"), CountKind(Pair.State, TEXT("perfect"), Pair.Target), 1);
		bPass &= Near(*this, TEXT("perfected bolt deals 0"), Live(Pair.State, Pair.Target).Hp, 1000.0);
		bPass &= Near(*this, TEXT("a fully absorbed bolt still pays 2 heat"), Live(Pair.State, Pair.Actor).Fire.Heat, BoltHeat);
	}
	return bPass;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaFireSunfall, "MageArena.Fire.Sunfall",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaFireSunfall::RunTest(const FString& Parameters)
{
	if (!Ready(*this) || !Literals(*this))
	{
		return false;
	}
	bool bPass = true;
	const int32 Slot = SlotOf(TEXT("fire_sunfall"));
	const int32 CastTicks = SimTicks(SunfallCastS);
	const int32 ShadowTicks = SimTicks(SunfallShadowS);
	const FSimVec Meteor{16.0, 10.0};
	{
		// Target stands 2 m off the meteor centre so a ward facing that centre is actually guarding.
		FFirePair Pair = MakeFire(8.0);
		const FSimVec Aim = Meteor;
		FActor& Target = Live(Pair.State, Pair.Target);
		Target.Facing = FSimVec{-1.0, 0.0};
		StepOne(Pair.State, Pair.Actor, CastInput(Aim, Slot));
		bPass &= TestTrue(TEXT("sunfall is pending"), Live(Pair.State, Pair.Actor).Pending.IsSet());
		bPass &= TestEqual(TEXT("release is cast only, 48 ticks"), Live(Pair.State, Pair.Actor).Pending->ReleaseTick, 1 + CastTicks);
		const FSimVec Rooted = Live(Pair.State, Pair.Actor).Pos;
		FInputFrame Move = CastInput(Aim, Slot);
		Move.bCast = false;
		Move.Move = FSimVec{1.0, 0.0};
		for (int32 Index = 0; Index < CastTicks; ++Index)
		{
			StepOne(Pair.State, Pair.Actor, Move);
		}
		bPass &= Near(*this, TEXT("rooted through the cast"), Live(Pair.State, Pair.Actor).Pos.X, Rooted.X);
		bPass &= TestFalse(TEXT("cast released"), Live(Pair.State, Pair.Actor).Pending.IsSet());
		bPass &= TestEqual(TEXT("shadow is committed"), Pair.State.Telegraphs.Num(), 1);
		if (Pair.State.Telegraphs.Num() == 1)
		{
			bPass &= TestTrue(TEXT("committed"), Pair.State.Telegraphs[0].bCommitted);
			bPass &= TestTrue(TEXT("survives the owner"), Pair.State.Telegraphs[0].bSurvivesOwner);
			bPass &= TestEqual(TEXT("lands 72 ticks later"), Pair.State.Telegraphs[0].ResolveTick, Pair.State.Tick + ShadowTicks);
			bPass &= Near(*this, TEXT("radius is the width"), Pair.State.Telegraphs[0].WidthM, SunfallRadiusM);
			bPass &= Near(*this, TEXT("damage snapshotted at 95"), Pair.State.Telegraphs[0].Damage, SunfallDamage);
		}
		Move.Move = FSimVec{1.0, 0.0};
		StepOne(Pair.State, Pair.Actor, Move);
		bPass &= Near(*this, TEXT("the shadow does not keep the root"), Live(Pair.State, Pair.Actor).Pos.X, Rooted.X + WalkMps / 60.0);
		while (Pair.State.Tick < 120)
		{
			StepOne(Pair.State, Pair.Target, AimHold(Live(Pair.State, Pair.Actor).Pos, true));
		}
		bPass &= TestTrue(TEXT("ward is stale before the land"), Pair.State.Tick - Live(Pair.State, Pair.Target).AbsorbFreshTick > SimTicks(AbsorbWindowS));
		StepOne(Pair.State, Pair.Target, AimHold(Live(Pair.State, Pair.Actor).Pos, true));
		bPass &= TestEqual(TEXT("lands on 121"), Pair.State.Tick, 1 + CastTicks + ShadowTicks);
		bPass &= Near(*this, TEXT("a held ward does not reduce Sunfall"), 1000.0 - Live(Pair.State, Pair.Target).Hp, SunfallDamage);
		bPass &= TestEqual(TEXT("no perfect on an unblockable"), CountKind(Pair.State, TEXT("perfect"), Pair.Target), 0);
		bPass &= Near(*this, TEXT("sunfall pays 10 heat"), Live(Pair.State, Pair.Actor).Fire.Heat, SunfallHeat);
	}
	{
		FFirePair Pair = MakeFire(8.0);
		const FSimVec Aim = Meteor;
		Live(Pair.State, Pair.Target).Facing = FSimVec{-1.0, 0.0};
		StepOne(Pair.State, Pair.Actor, CastInput(Aim, Slot));
		while (Pair.State.Tick < CastTicks + ShadowTicks)
		{
			Advance(Pair.State, 1);
		}
		StepOne(Pair.State, Pair.Target, AimHold(Live(Pair.State, Pair.Actor).Pos, true));
		bPass &= Near(*this, TEXT("a fresh ward still takes 95"), 1000.0 - Live(Pair.State, Pair.Target).Hp, SunfallDamage);
		bPass &= TestEqual(TEXT("fresh ward is not a perfect"), CountKind(Pair.State, TEXT("perfect"), Pair.Target), 0);
	}
	{
		FArenaState State = CreateArena(12);
		const int32 Caster = AddMage(State, 0, FSimVec{10.0, 10.0}, TEXT("Ember")).Id;
		const int32 Striker = AddMage(State, 1, FSimVec{9.0, 10.0}, TEXT("Striker")).Id;
		FActor& Actor = Live(State, Caster);
		Actor.Fire.bSchool = true;
		Actor.Tier = 4;
		SetPool(Actor, 1000.0, 500.0);
		SetPool(Live(State, Striker), 1000.0, 500.0);
		StepBoth(State, Caster, CastInput(Live(State, Striker).Pos, Slot), Striker, CastInput(Actor.Pos, 0));
		bPass &= TestTrue(TEXT("sunfall started"), Live(State, Caster).Pending.IsSet() && Live(State, Caster).Pending->Kind == TEXT("fire"));
		Advance(State, SimTicks(StaffWindupS));
		bPass &= TestFalse(TEXT("staff during the cast cancels Sunfall"), Live(State, Caster).Pending.IsSet());
		bPass &= TestFalse(TEXT("interrupt does not loose it"), Live(State, Caster).Fire.bSunfallLoosed);
		bPass &= TestEqual(TEXT("no shadow yet"), State.Telegraphs.Num(), 0);
		bPass &= Near(*this, TEXT("staff connected"), 1000.0 - Live(State, Caster).Hp, StaffDamage);
	}
	{
		FFirePair Pair = MakeFire(6.0);
		const FSimVec Aim = Live(Pair.State, Pair.Target).Pos;
		StepOne(Pair.State, Pair.Actor, CastInput(Aim, Slot));
		Advance(Pair.State, SimTicks(SunfallCastS));
		bPass &= TestEqual(TEXT("shadow exists"), Pair.State.Telegraphs.Num(), 1);
		Interrupt(Pair.State, Live(Pair.State, Pair.Actor));
		bPass &= TestEqual(TEXT("interrupt keeps the committed shadow"), Pair.State.Telegraphs.Num(), 1);
		Live(Pair.State, Pair.Actor).bDown = true;
		while (Pair.State.Tick < 1 + SimTicks(SunfallCastS) + SimTicks(SunfallShadowS))
		{
			Advance(Pair.State, 1);
		}
		bPass &= Near(*this, TEXT("the shadow lands after the owner is down"), 1000.0 - Live(Pair.State, Pair.Target).Hp, SunfallDamage);
	}
	return bPass;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaFireShapes, "MageArena.Fire.Shapes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaFireShapes::RunTest(const FString& Parameters)
{
	if (!Ready(*this) || !Literals(*this))
	{
		return false;
	}
	bool bPass = true;
	{
		FArenaState State = CreateArena(13);
		const int32 ActorId = AddMage(State, 0, FSimVec{10.0, 10.0}, TEXT("Ember")).Id;
		const int32 HitId = AddMage(State, 1, FSimVec{14.0, 10.0}, TEXT("Edge")).Id;
		const int32 FarId = AddMage(State, 1, FSimVec{14.05, 10.0}, TEXT("Past")).Id;
		const int32 BehindId = AddMage(State, 1, FSimVec{10.0, 14.0}, TEXT("Behind")).Id;
		FActor& Actor = Live(State, ActorId);
		Actor.Fire.bSchool = true;
		Actor.Tier = 4;
		SetPool(Actor, 1000.0, 500.0);
		SetPool(Live(State, HitId), 1000.0, 500.0);
		SetPool(Live(State, FarId), 1000.0, 500.0);
		SetPool(Live(State, BehindId), 1000.0, 500.0);
		StepOne(State, ActorId, CastInput(FSimVec{14.0, 10.0}, SlotOf(TEXT("fire_flick"))));
		Advance(State, SimTicks(0.15));
		bPass &= Near(*this, TEXT("flick hits the body on the 4 m edge"), 1000.0 - Live(State, HitId).Hp, FlickDamage);
		bPass &= Near(*this, TEXT("flick excludes past the reach"), Live(State, FarId).Hp, 1000.0);
		bPass &= Near(*this, TEXT("flick excludes outside 60 deg"), Live(State, BehindId).Hp, 1000.0);
		bPass &= Near(*this, TEXT("flick heat is 6, once"), Live(State, ActorId).Fire.Heat, FlickHeat);
	}
	{
		FFirePair Pair = MakeFire(2.0);
		const int32 Windup = SimTicks(RingCastS);
		StepOne(Pair.State, Pair.Actor, CastInput(Live(Pair.State, Pair.Target).Pos, SlotOf(TEXT("fire_ring"))));
		Advance(Pair.State, Windup);
		bPass &= Near(*this, TEXT("ring damage 18"), 1000.0 - Live(Pair.State, Pair.Target).Hp, RingDamage);
		bPass &= Near(*this, TEXT("ring knockback 2 m"), Live(Pair.State, Pair.Target).Pos.X, 14.0);
		bPass &= Near(*this, TEXT("ring heat 5 plus the 3 m proximity during the windup"), Live(Pair.State, Pair.Actor).Fire.Heat,
			RingHeat + static_cast<double>(1 + Windup) * HeatPerSecondNear / 60.0);
	}
	{
		FArenaState State = CreateArena(14);
		const int32 ActorId = AddMage(State, 0, FSimVec{10.0, 10.0}, TEXT("Ember")).Id;
		const int32 ShieldId = AddEnemy(State, TEXT("shieldman"), FSimVec{20.0, 10.0}).Id;
		const FEnemySpec* Shield = SpecById(TEXT("shieldman"));
		bPass &= TestNotNull(TEXT("shieldman"), Shield);
		if (Shield)
		{
			bPass &= Near(*this, TEXT("shield hp literal"), Shield->Hp, ShieldHp);
			bPass &= TestTrue(TEXT("shield front mult"), Shield->FrontBlockMult.IsSet());
			if (Shield->FrontBlockMult.IsSet())
			{
				bPass &= Near(*this, TEXT("shield front is 0"), Shield->FrontBlockMult.GetValue(), ShieldFrontMult);
			}
		}
		FActor& Actor = Live(State, ActorId);
		Actor.Fire.bSchool = true;
		Actor.Tier = 4;
		SetPool(Actor, 1000.0, 500.0);
		FActor& Bearer = Live(State, ShieldId);
		Bearer.Facing = FSimVec{-1.0, 0.0};
		StepOne(State, ActorId, CastInput(Bearer.Pos, SlotOf(TEXT("fire_lance"))));
		Advance(State, SimTicks(LanceCastS));
		bPass &= Near(*this, TEXT("lance ignores the scutum and deals 32"), ShieldHp - Live(State, ShieldId).Hp, LanceDamage);
		bPass &= Near(*this, TEXT("lance heat 4"), Live(State, ActorId).Fire.Heat, 4.0);
	}
	{
		FFirePair Pair = MakeFire(6.0);
		StepOne(Pair.State, Pair.Actor, CastInput(Live(Pair.State, Pair.Target).Pos, SlotOf(TEXT("fire_pyre"))));
		bPass &= TestEqual(TEXT("pyre windup is the 0.8 s warning"), Live(Pair.State, Pair.Actor).Pending->ReleaseTick, 1 + SimTicks(PyreTelegraphS));
		Advance(Pair.State, SimTicks(PyreTelegraphS));
		bPass &= Near(*this, TEXT("pyre detonates for 48"), 1000.0 - Live(Pair.State, Pair.Target).Hp, PyreDamage);
		bPass &= TestEqual(TEXT("pyre leaves no telegraph"), Pair.State.Telegraphs.Num(), 0);
		bPass &= Near(*this, TEXT("pyre heat 8"), Live(Pair.State, Pair.Actor).Fire.Heat, PyreHeat);
		const double Hp = Live(Pair.State, Pair.Target).Hp;
		Advance(Pair.State, 60);
		bPass &= Near(*this, TEXT("pyre does not tick again"), Live(Pair.State, Pair.Target).Hp, Hp);
	}
	{
		FArenaState State = CreateArena(15);
		const int32 ActorId = AddMage(State, 0, FSimVec{10.0, 10.0}, TEXT("Ember")).Id;
		const int32 TargetId = AddMage(State, 1, FSimVec{30.0, 10.0}, TEXT("Off the trail")).Id;
		FActor& Actor = Live(State, ActorId);
		Actor.Fire.bSchool = true;
		Actor.Tier = 4;
		SetPool(Actor, 1000.0, 500.0);
		SetPool(Live(State, TargetId), 1000.0, 500.0);
		const double Stamina = Actor.Stamina;
		StepOne(State, ActorId, CastInput(FSimVec{20.0, 10.0}, SlotOf(TEXT("fire_cinderstep"))));
		bPass &= Near(*this, TEXT("cinder dash is 5 m"), Live(State, ActorId).Pos.X, 15.0);
		bPass &= Near(*this, TEXT("cinder spends no stamina"), Live(State, ActorId).Stamina, Stamina - 0.0);
		Live(State, TargetId).Pos = FSimVec{11.0, 10.0};
		const int32 Until = 1 + SimTicks(CinderDurationS);
		while (State.Tick < Until)
		{
			Advance(State, 1);
		}
		bPass &= Near(*this, TEXT("four trail ticks of 6"), 1000.0 - Live(State, TargetId).Hp, 4.0 * CinderDamage);
		bPass &= Near(*this, TEXT("four trail heats of 3"), Live(State, ActorId).Fire.Heat, 4.0 * CinderHeat);
	}
	{
		FArenaState State = CreateArena(16);
		const int32 ActorId = AddMage(State, 0, FSimVec{10.0, 10.0}, TEXT("Ember")).Id;
		const int32 TargetId = AddMage(State, 1, FSimVec{30.0, 10.0}, TEXT("Off the trail")).Id;
		FActor& Actor = Live(State, ActorId);
		Actor.Fire.bSchool = true;
		Actor.Tier = 4;
		SetPool(Actor, 1000.0, 500.0);
		SetPool(Live(State, TargetId), 1000.0, 500.0);
		StepOne(State, ActorId, CastInput(FSimVec{20.0, 10.0}, SlotOf(TEXT("fire_cinderstep"))));
		bPass &= TestTrue(TEXT("one trail"), Live(State, ActorId).Fire.Trails.Num() == 1);
		if (Live(State, ActorId).Fire.Trails.Num() == 1)
		{
			const FFireTrail Copy = Live(State, ActorId).Fire.Trails[0];
			Live(State, ActorId).Fire.Trails.Add(Copy);
		}
		Live(State, TargetId).Pos = FSimVec{11.0, 10.0};
		while (State.Tick < 1 + SimTicks(CinderDurationS))
		{
			Advance(State, 1);
		}
		bPass &= Near(*this, TEXT("overlapping trails dedup to four hits"), 1000.0 - Live(State, TargetId).Hp, 4.0 * CinderDamage);
		bPass &= Near(*this, TEXT("dedup pays heat once a tick"), Live(State, ActorId).Fire.Heat, 4.0 * CinderHeat);
	}
	{
		FFirePair Pair = MakeFire(6.0);
		const FSimVec Aim = Live(Pair.State, Pair.Target).Pos;
		StepOne(Pair.State, Pair.Actor, CastInput(Aim, SlotOf(TEXT("fire_wrath"))));
		while (Pair.State.Tick < 1 + SimTicks(WrathCastS) + SimTicks(WrathDurationS))
		{
			StepOne(Pair.State, Pair.Actor, AimHold(Aim, false));
		}
		bPass &= TestEqual(TEXT("eight wrath ticks"), CountKind(Pair.State, TEXT("hit"), Pair.Target), 8);
		bPass &= Near(*this, TEXT("wrath deals 8 * 14"), 1000.0 - Live(Pair.State, Pair.Target).Hp, 8.0 * WrathDamage);
		bPass &= Near(*this, TEXT("wrath heat stays under kindled"), Live(Pair.State, Pair.Actor).Fire.Heat, 8.0 * WrathHeat);
	}
	return bPass;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaFireWrathWard, "MageArena.Fire.WrathWard",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaFireWrathWard::RunTest(const FString& Parameters)
{
	if (!Ready(*this) || !Literals(*this))
	{
		return false;
	}
	FFirePair Pair = MakeFire(6.0);
	const FSimVec Aim = Live(Pair.State, Pair.Target).Pos;
	Live(Pair.State, Pair.Target).Facing = FSimVec{-1.0, 0.0};
	const int32 FirstTick = 1 + SimTicks(WrathCastS) + SimTicks(WrathTickS);
	const int32 SecondTick = FirstTick + SimTicks(WrathTickS);
	const int32 RaiseAgain = FirstTick + SimTicks(AbsorbReRaiseS);
	StepOne(Pair.State, Pair.Actor, CastInput(Aim, SlotOf(TEXT("fire_wrath"))));
	while (Pair.State.Tick < SecondTick)
	{
		const int32 Upcoming = Pair.State.Tick + 1;
		// The beam resolves in the caster's step, before this tick's ward input. The ward has to already be up.
		const bool bAbsorb = Upcoming == FirstTick - 1 || Upcoming == FirstTick || (Upcoming >= RaiseAgain && Upcoming <= SecondTick);
		FInputFrame Caster = AimHold(Aim, false);
		FInputFrame Ward = AimHold(Live(Pair.State, Pair.Actor).Pos, bAbsorb);
		StepBoth(Pair.State, Pair.Actor, Caster, Pair.Target, Ward);
	}
	bool bPass = TestEqual(TEXT("first wrath tick is 28"), FirstTick, 28);
	bPass &= TestEqual(TEXT("second wrath tick is 43"), SecondTick, 43);
	bPass &= TestEqual(TEXT("re-raise gap is 8"), SimTicks(AbsorbReRaiseS), 8);
	bPass &= TestEqual(TEXT("one perfect"), CountKind(Pair.State, TEXT("perfect"), Pair.Target), 1);
	bPass &= Near(*this, TEXT("the second tick chips 14 * 0.15"), 1000.0 - Live(Pair.State, Pair.Target).Hp, WrathDamage * (1.0 - AbsorbMagicReduction));
	bPass &= Near(*this, TEXT("both ticks pay heat"), Live(Pair.State, Pair.Actor).Fire.Heat, 2.0 * WrathHeat);
	return bPass;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaFireSunfallBeat, "MageArena.Fire.SunfallBeat",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaFireSunfallBeat::RunTest(const FString& Parameters)
{
	if (!Ready(*this) || !Literals(*this))
	{
		return false;
	}
	bool bPass = TestEqual(TEXT("sunfall is slot 8"), SlotOf(TEXT("fire_sunfall")), 8);
	{
		FFirePair Pair = MakeFire(6.0);
		FActor& Actor = Live(Pair.State, Pair.Actor);
		Actor.Mana = MaxManaRank1;
		Actor.MaxMana = MaxManaRank1;
		AttachMageAI(Actor, 1.0, Pair.State.Tick);
		const FInputFrame Input = MageInput(Pair.State, Actor);
		bPass &= TestTrue(TEXT("tier 4 opens on Sunfall"), Input.bCast && Input.Slot == 8);
		bPass &= TestFalse(TEXT("Sunfall does not draw the spell random"), DrewPurpose(Pair.State, TEXT("mage 1 spell")));
		StepOne(Pair.State, Pair.Actor, Input);
		bPass &= TestTrue(TEXT("the decision casts Sunfall"), Live(Pair.State, Pair.Actor).Pending.IsSet()
			&& Live(Pair.State, Pair.Actor).Pending->SpellId.Get(FString()) == TEXT("fire_sunfall"));
	}
	{
		FFirePair Pair = MakeFire(6.0);
		FActor& Actor = Live(Pair.State, Pair.Actor);
		Actor.Mana = SunfallMana - 1.0;
		Actor.MaxMana = MaxManaRank1;
		AttachMageAI(Actor, 1.0, Pair.State.Tick);
		const FInputFrame Input = MageInput(Pair.State, Actor);
		bPass &= TestFalse(TEXT("short of 70 mana, offense waits"), Input.bCast);
		bPass &= TestFalse(TEXT("the wait does not draw the spell random"), DrewPurpose(Pair.State, TEXT("mage 1 spell")));
	}
	{
		FFirePair Pair = MakeFire(6.0);
		FActor& Actor = Live(Pair.State, Pair.Actor);
		// 74 is 70 plus one bolt. It already pays Sunfall, so the opening cast is Sunfall.
		// The cheapest pinned spell is 4, so a pool that can pay a spell and still hold 70 already holds 70.
		Actor.Mana = SunfallMana + BoltMana;
		Actor.MaxMana = MaxManaRank1;
		AttachMageAI(Actor, 1.0, Pair.State.Tick);
		const FInputFrame Input = MageInput(Pair.State, Actor);
		bPass &= TestTrue(TEXT("74 mana still opens on Sunfall"), Input.bCast && Input.Slot == SlotOf(TEXT("fire_sunfall")));
		bPass &= TestFalse(TEXT("74 mana does not draw the spell random"), DrewPurpose(Pair.State, TEXT("mage 1 spell")));
	}
	{
		FGames Water;
		const FComposition* Preset = &KernelData().Presets[0];
		bPass &= TestTrue(TEXT("water proxy still creates"), TryCreateGames(Water, 40000, Preset, 2, false, false));
		bPass &= TestTrue(TEXT("default label is the water proxy"), Water.SpawnLog.Num() > 0 && Water.SpawnLog[0].Kind.StartsWith(TEXT("Water proxy")));
		if (Water.SpawnLog.Num() > 0)
		{
			const FActor* Duelist = SimFindActor(Water.State, Water.SpawnLog[0].Id);
			bPass &= TestTrue(TEXT("default duelist is not a fire school"), Duelist && !Duelist->Fire.bSchool);
		}
	}
	return bPass;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaFireGolden, "MageArena.Fire.Golden",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaFireGolden::RunTest(const FString& Parameters)
{
	if (!Ready(*this))
	{
		return false;
	}
	bool bPass = true;
	struct FCase
	{
		uint32 Seed;
		int32 Wave;
		const TCHAR* File;
	};
	const FCase Cases[] = {
		{40000, 2, TEXT("fire-duel-competence-1.json")},
		{40001, 3, TEXT("fire-duel-competence-1-5.json")}
	};
	for (const FCase& Case : Cases)
	{
		FDuelResult First;
		FDuelResult Second;
		if (!RunDuel(*this, Case.Seed, Case.Wave, First) || !RunDuel(*this, Case.Seed, Case.Wave, Second))
		{
			return false;
		}
		bPass &= TestEqual(FString::Printf(TEXT("replay %u"), Case.Seed), First.Hash, Second.Hash);
		bPass &= TestEqual(TEXT("replay ticks"), First.Ticks, Second.Ticks);
		const FString Line = FString::Printf(
			TEXT("FIRE_DUEL seed=%u wave=%d outcome=%s phase=%s ticks=%d durationS=%.6f hash=%s sunfallCast=%d sunfallRelease=%d sunfall=%d hp=%.6f mana=%.6f wavesCleared=%d kind=%s"),
			Case.Seed, Case.Wave, *First.Outcome, *First.Phase, First.Ticks, First.DurationS, *First.Hash,
			First.SunfallCast, First.SunfallRelease, First.bSunfall ? 1 : 0, First.Hp, First.Mana, First.WavesCleared, *First.Kind);
		UE_LOG(LogMageArena, Display, TEXT("%s"), *Line);
		AddInfo(Line);
		const FString Path = ScenarioPath(Case.File);
		FString Text;
		if (!FFileHelper::LoadFileToString(Text, *Path))
		{
			AddError(TEXT("Missing scenario ") + Path + TEXT(" :: ") + Line);
			bPass = false;
			continue;
		}
		TSharedPtr<FJsonObject> Root;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
		if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
		{
			AddError(TEXT("Invalid scenario ") + Path);
			bPass = false;
			continue;
		}
		double Seed = 0.0;
		double Wave = 0.0;
		double Ticks = 0.0;
		double SunCast = 0.0;
		double SunRelease = 0.0;
		double Waves = 0.0;
		double SunFlag = 0.0;
		FString Outcome;
		FString Phase;
		FString Hash;
		bPass &= TestTrue(TEXT("seed field"), Root->TryGetNumberField(TEXT("seed"), Seed) && static_cast<uint32>(Seed) == Case.Seed);
		bPass &= TestTrue(TEXT("wave field"), Root->TryGetNumberField(TEXT("wave"), Wave) && static_cast<int32>(Wave) == Case.Wave);
		bPass &= TestTrue(TEXT("outcome field"), Root->TryGetStringField(TEXT("outcome"), Outcome) && Outcome == First.Outcome);
		bPass &= TestTrue(TEXT("phase field"), Root->TryGetStringField(TEXT("phase"), Phase) && Phase == First.Phase);
		bPass &= TestTrue(TEXT("ticks field"), Root->TryGetNumberField(TEXT("ticks"), Ticks) && static_cast<int32>(Ticks) == First.Ticks);
		bPass &= TestTrue(TEXT("hash field"), Root->TryGetStringField(TEXT("hash"), Hash) && Hash == First.Hash);
		bPass &= TestTrue(TEXT("sunfall cast field"), Root->TryGetNumberField(TEXT("sunfallCast"), SunCast) && static_cast<int32>(SunCast) == First.SunfallCast);
		bPass &= TestTrue(TEXT("sunfall release field"), Root->TryGetNumberField(TEXT("sunfallRelease"), SunRelease) && static_cast<int32>(SunRelease) == First.SunfallRelease);
		bPass &= TestTrue(TEXT("sunfall flag"), Root->TryGetNumberField(TEXT("sunfall"), SunFlag) && (SunFlag != 0.0) == First.bSunfall);
		bPass &= TestTrue(TEXT("waves field"), Root->TryGetNumberField(TEXT("wavesCleared"), Waves) && static_cast<int32>(Waves) == First.WavesCleared);
		double Hp = 0.0;
		double Mana = 0.0;
		double Duration = 0.0;
		bPass &= TestTrue(TEXT("hp field"), Root->TryGetNumberField(TEXT("hp"), Hp));
		bPass &= TestTrue(TEXT("mana field"), Root->TryGetNumberField(TEXT("mana"), Mana));
		bPass &= TestTrue(TEXT("duration field"), Root->TryGetNumberField(TEXT("durationS"), Duration));
		if (bPass)
		{
			bPass &= Near(*this, TEXT("hp"), First.Hp, Hp);
			bPass &= Near(*this, TEXT("mana"), First.Mana, Mana);
			bPass &= Near(*this, TEXT("duration"), First.DurationS, Duration);
		}
	}
	return bPass;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaFireStepCost, "MageArena.Fire.StepCost",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaFireStepCost::RunTest(const FString& Parameters)
{
	if (!Ready(*this))
	{
		return false;
	}
	const FKernelData& Data = KernelData();
	const FComposition* Preset = nullptr;
	for (const FComposition& Candidate : Data.Presets)
	{
		if (Candidate.Name == Data.ReferencePreset)
		{
			Preset = &Candidate;
			break;
		}
	}
	if (!TestNotNull(TEXT("reference preset"), Preset))
	{
		return false;
	}
	FGames Games;
	if (!TestTrue(TEXT("duel"), TryCreateGames(Games, 40000, Preset, 2, true, true)))
	{
		return false;
	}
	for (FActor& Actor : Games.State.Actors)
	{
		Actor.MaxHp = 100000.0;
		Actor.Hp = 100000.0;
	}
	for (int32 Index = 0; Index < 20; ++Index)
	{
		StepGames(Games);
	}
	double SumUs = 0.0;
	int32 Measured = 0;
	for (int32 Index = 0; Index < Samples && Games.Phase == TEXT("active"); ++Index)
	{
		const double Start = FPlatformTime::Seconds();
		StepGames(Games);
		SumUs += (FPlatformTime::Seconds() - Start) * 1.0e6;
		++Measured;
	}
	if (!TestTrue(TEXT("samples"), Measured == Samples))
	{
		return false;
	}
	const double MeanUs = SumUs / static_cast<double>(Measured);
	UE_LOG(LogMageArena, Display, TEXT("FireStepUs=%.3f"), MeanUs);
	AddInfo(FString::Printf(TEXT("FireStepUs=%.3f"), MeanUs));
	return TestTrue(TEXT("mean fire step is within 1 ms"), MeanUs <= 1000.0);
}

#endif
