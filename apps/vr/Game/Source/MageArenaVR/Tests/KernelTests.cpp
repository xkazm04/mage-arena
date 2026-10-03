#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Kernel/ArenaKernel.h"
#include "Kernel/Catalog.h"
#include "Kernel/Training.h"
#include "MageArenaVR.h"

#include <cmath>
#include <limits>

namespace
{
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

FHit MakeHit(int32 Owner, int32 Activation, double Damage, const TCHAR* Family, int32 Tier, const FSimVec& Source)
{
	FHit Hit;
	Hit.OwnerId = Owner;
	Hit.ActivationId = Activation;
	Hit.Damage = Damage;
	Hit.Family = Family;
	Hit.Tier = Tier;
	Hit.Source = Source;
	return Hit;
}

struct FHitCase
{
	FArenaState State;
	int32 ActorId = 0;
	FSimHitResult Result;
};

// kernel.test.ts:7. Damage 20 is the fixture in that test.
FHitCase HitCase(int32 Age, double Degrees, const TCHAR* Family = TEXT("magic"), int32 Tier = 1, int32 Nerve = 1)
{
	FHitCase Out;
	Out.State = CreateArena(17);
	FSimRanks Ranks;
	Ranks.Vigor = 1;
	Ranks.Focus = 1;
	Ranks.Nerve = Nerve;
	Out.ActorId = AddMage(Out.State, 0, FSimVec{10.0, 10.0}, TEXT("test"), &Ranks).Id;
	FActor& Actor = *SimFindActor(Out.State, Out.ActorId);
	Actor.Mana = 20.0;
	Actor.bAbsorb = true;
	Actor.AbsorbFreshTick = 0;
	Out.State.Tick = Age;
	const FSimVec Direction = SimRotate(FSimVec{1.0, 0.0}, Degrees * SimPi / 180.0);
	const FHit Hit = MakeHit(99, 100, 20.0, Family, Tier, FSimVec{Actor.Pos.X + Direction.X, Actor.Pos.Y + Direction.Y});
	Out.Result = ResolveHit(Out.State, Actor, Hit);
	return Out;
}

void StepOne(FArenaState& State, int32 ActorId, const FInputFrame& Input)
{
	TMap<int32, FInputFrame> Inputs;
	Inputs.Add(ActorId, Input);
	StepArena(State, Inputs);
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaKernelRng, "MageArena.Kernel.Rng",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaKernelRng::RunTest(const FString& Parameters)
{
	if (!Ready(*this))
	{
		return false;
	}
	// First five mulberry32 draws from seed 1, checked against kernel.ts:22-26 in JavaScript.
	const uint32 RngAfter[] = {1831565814u, 3663131627u, 1199730144u, 3031295957u, 567894474u};
	const uint32 Bits[] = {2693262067u, 11749833u, 2265367787u, 4213581821u, 4159151403u};
	FArenaState State = CreateArena(1);
	bool bPass = true;
	for (int32 Index = 0; Index < 5; ++Index)
	{
		const double Value = ArenaRandom(State, TEXT("check"));
		const uint32 Drawn = static_cast<uint32>(std::llround(Value * 4294967296.0));
		bPass &= TestEqual(*FString::Printf(TEXT("rng after draw %d"), Index), State.Rng, RngAfter[Index]);
		bPass &= TestEqual(*FString::Printf(TEXT("bits %d"), Index), Drawn, Bits[Index]);
	}
	uint32 Hash = 2166136261u;
	FArenaState Long = CreateArena(1);
	for (int32 Index = 0; Index < 10000; ++Index)
	{
		const double Value = ArenaRandom(Long, TEXT("check"));
		const uint32 Drawn = static_cast<uint32>(std::llround(Value * 4294967296.0));
		Hash ^= Drawn;
		Hash *= 16777619u;
	}
	bPass &= TestEqual(TEXT("10000-draw checksum"), Hash, 2108861193u);
	return bPass;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaKernelAbsorb, "MageArena.Kernel.Absorb",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaKernelAbsorb::RunTest(const FString& Parameters)
{
	if (!Ready(*this))
	{
		return false;
	}
	const FAbsorbRules& Rules = KernelData().Absorb;
	bool bPass = true;
	const double Blocked = 20.0 * (1.0 - Rules.ReductionMagic);
	for (const double Angle : {-70.0, -69.0, 69.0, 70.0})
	{
		bPass &= Near(*this, TEXT("arc boundary damage"), HitCase(12, Angle).Result.Damage, Blocked);
	}
	for (const double Angle : {-71.0, 71.0, 180.0})
	{
		const FHitCase Outside = HitCase(0, Angle);
		bPass &= TestEqual(TEXT("outside arc is not perfect"), Outside.Result.bPerfect, false);
		bPass &= Near(*this, TEXT("outside arc damage"), Outside.Result.Damage, 20.0);
	}
	// math.ts:9. Zero-length toward, and a full circle, are inside. The 3D ward keeps zero and vertical hits outside.
	bPass &= TestTrue(TEXT("full circle contains a back attack"), SimInArc(FSimVec{1.0, 0.0}, FSimVec{-1.0, 0.0}, 360.0));
	bPass &= TestTrue(TEXT("zero toward is inside the 2D arc"), SimInArc(FSimVec{1.0, 0.0}, FSimVec{0.0, 0.0}, Rules.ArcDeg));

	const int32 Window = SimTicks(Rules.WindowS);
	bPass &= TestTrue(TEXT("window edge is perfect"), HitCase(Window, 0.0).Result.bPerfect);
	bPass &= TestFalse(TEXT("one tick past the window is not perfect"), HitCase(Window + 1, 0.0).Result.bPerfect);

	const FHitCase Steel = HitCase(0, 0.0, TEXT("physical"));
	bPass &= TestFalse(TEXT("steel is not perfect"), Steel.Result.bPerfect);
	bPass &= Near(*this, TEXT("steel damage"), Steel.Result.Damage, 20.0 * (1.0 - Rules.ReductionPhysical));
	const FHitCase Open = HitCase(0, 0.0, TEXT("unblockable"));
	bPass &= TestFalse(TEXT("unblockable is not perfect"), Open.Result.bPerfect);
	bPass &= Near(*this, TEXT("unblockable damage"), Open.Result.Damage, 20.0);

	for (int32 Nerve = 1; Nerve <= 5; ++Nerve)
	{
		bPass &= Near(*this, TEXT("drain"), DrainPerSecond(Nerve), Rules.DrainBase - Rules.DrainSubtractPerRank * static_cast<double>(Nerve));
		for (int32 Tier = 0; Tier <= 4; ++Tier)
		{
			const FHitCase Refund = HitCase(0, 0.0, TEXT("magic"), Tier, Nerve);
			const FActor& Actor = *SimFindActor(Refund.State, Refund.ActorId);
			bPass &= Near(*this, TEXT("perfect return"), PerfectReturn(Tier, Nerve), Rules.ManaForPerfect(Tier, Nerve));
			bPass &= Near(*this, TEXT("refunded mana"), Actor.Mana, 20.0 + Rules.ManaForPerfect(Tier, Nerve));
		}
	}
	FHitCase Capped = HitCase(0, 0.0);
	FActor& CappedActor = *SimFindActor(Capped.State, Capped.ActorId);
	CappedActor.Mana = CappedActor.MaxMana - 1.0;
	ResolveHit(Capped.State, CappedActor, MakeHit(99, 1, 2.0, TEXT("magic"), 4, FSimVec{11.0, 10.0}));
	bPass &= Near(*this, TEXT("refund cap"), CappedActor.Mana, CappedActor.MaxMana);
	return bPass;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaKernelWard, "MageArena.Kernel.Ward",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaKernelWard::RunTest(const FString& Parameters)
{
	if (!Ready(*this))
	{
		return false;
	}
	const FKernelData& Data = KernelData();
	FArenaState State = CreateArena();
	const int32 ActorId = AddMage(State, 0, FSimVec{10.0, 10.0}).Id;
	FInputFrame Held = SimIdleInput();
	Held.bAbsorb = true;
	StepOne(State, ActorId, Held);
	const FActor& Raised = *SimFindActor(State, ActorId);
	bool bPass = Near(*this, TEXT("raise and first drain"), Raised.Mana,
		Raised.MaxMana - Data.Absorb.RaiseCostMana - DrainPerSecond(Raised.Ranks.Nerve) * SimDt());
	StepOne(State, ActorId, SimIdleInput());
	const int32 Released = State.Tick;
	const int32 Gap = SimTicks(Data.Absorb.MinReleaseS);
	for (int32 Index = 0; Index < Gap - 1; ++Index)
	{
		StepOne(State, ActorId, Held);
	}
	bPass &= TestFalse(TEXT("gap holds the ward down"), SimFindActor(State, ActorId)->bAbsorb);
	StepOne(State, ActorId, Held);
	bPass &= TestEqual(TEXT("fresh tick follows the release gap"), SimFindActor(State, ActorId)->AbsorbFreshTick - Released, Gap);
	SimFindActor(State, ActorId)->Mana = 0.0;
	StepOne(State, ActorId, Held);
	bPass &= TestTrue(TEXT("empty mana latches exhaustion"), SimFindActor(State, ActorId)->bAbsorbExhausted);
	for (int32 Index = 0; Index < SimTicks(3.0); ++Index)
	{
		StepOne(State, ActorId, Held);
	}
	const FActor& Spent = *SimFindActor(State, ActorId);
	bPass &= TestFalse(TEXT("exhaustion stays down"), Spent.bAbsorb);
	bPass &= TestTrue(TEXT("regen passes the raise cost"), Spent.Mana > Data.Absorb.RaiseCostMana);

	FHitCase Stream = HitCase(0, 0.0);
	for (int32 Index = 1; Index < Data.StreamCount; ++Index)
	{
		Stream.State.Tick = Index * SimTicks(Data.StreamIntervalS);
		FActor& Actor = *SimFindActor(Stream.State, Stream.ActorId);
		ResolveHit(Stream.State, Actor, MakeHit(99, Index, 20.0, TEXT("magic"), 1, FSimVec{11.0, 10.0}));
	}
	const FActor& StreamActor = *SimFindActor(Stream.State, Stream.ActorId);
	bPass &= TestEqual(TEXT("stream perfects once"), StreamActor.Metrics.Perfects, 1);
	bPass &= TestEqual(TEXT("later stream hits block"), StreamActor.Metrics.Blocks, Data.StreamCount - 1);
	return bPass;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaKernelMovement, "MageArena.Kernel.Movement",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaKernelMovement::RunTest(const FString& Parameters)
{
	if (!Ready(*this))
	{
		return false;
	}
	const FKernelData& Data = KernelData();
	FArenaState State = CreateArena();
	const int32 ActorId = AddMage(State, 0, FSimVec{10.0, 10.0}).Id;
	FInputFrame Diagonal = SimIdleInput();
	Diagonal.Move = FSimVec{1.0, 1.0};
	for (int32 Index = 0; Index < SimTicks(1.0); ++Index)
	{
		StepOne(State, ActorId, Diagonal);
	}
	const FActor& Walked = *SimFindActor(State, ActorId);
	bool bPass = Near(*this, TEXT("diagonal walk"), SimDistance(Walked.Pos, FSimVec{10.0, 10.0}), Data.WalkMps);

	SimFindActor(State, ActorId)->Pos = FSimVec{10.0, 10.0};
	FInputFrame Sprint = SimIdleInput();
	Sprint.Move = FSimVec{1.0, 0.0};
	Sprint.bSprint = true;
	for (int32 Index = 0; Index < SimTicks(1.0); ++Index)
	{
		StepOne(State, ActorId, Sprint);
	}
	const FActor& Sprinted = *SimFindActor(State, ActorId);
	bPass &= Near(*this, TEXT("sprint distance"), Sprinted.Pos.X, 10.0 + Data.SprintMps);
	bPass &= Near(*this, TEXT("sprint stamina"), Sprinted.Stamina, Sprinted.MaxStamina - Data.SprintStaminaPerSecond);

	SimFindActor(State, ActorId)->Pos = FSimVec{10.0, 10.0};
	const double Before = SimFindActor(State, ActorId)->Stamina;
	FInputFrame Roll = SimIdleInput();
	Roll.bRoll = true;
	for (int32 Index = 0; Index < SimTicks(Data.RollDurationS); ++Index)
	{
		StepOne(State, ActorId, Roll);
	}
	const FActor& Rolled = *SimFindActor(State, ActorId);
	bPass &= Near(*this, TEXT("roll distance"), Rolled.Pos.X, 10.0 + Data.RollDistanceM);
	bPass &= Near(*this, TEXT("roll stamina"), Rolled.Stamina, Before - Data.RollStaminaCost);
	bPass &= TestEqual(TEXT("one roll per press"), Rolled.Metrics.Rolls, 1);

	FArenaState Immune = CreateArena();
	const int32 ImmuneId = AddMage(Immune, 0, FSimVec{10.0, 10.0}).Id;
	StepOne(Immune, ImmuneId, Roll);
	FActor& Dodging = *SimFindActor(Immune, ImmuneId);
	const FHit Blow = MakeHit(99, 1, 10.0, TEXT("unblockable"), 4, FSimVec{20.0, 10.0});
	bPass &= Near(*this, TEXT("i-frames absorb the hit"), ResolveHit(Immune, Dodging, Blow).Damage, 0.0);
	Immune.Tick = Dodging.ImmuneUntil;
	bPass &= Near(*this, TEXT("i-frames end"), ResolveHit(Immune, Dodging, Blow).Damage, 10.0);
	bPass &= TestTrue(TEXT("movement recovery outlasts i-frames"), Immune.Tick < Dodging.RollUntil);
	return bPass;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaKernelProjectiles, "MageArena.Kernel.Projectiles",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaKernelProjectiles::RunTest(const FString& Parameters)
{
	if (!Ready(*this))
	{
		return false;
	}
	FArenaState State = CreateArena();
	const int32 Caster = AddMage(State, 0, FSimVec{5.0, 10.0}).Id;
	const int32 First = AddMage(State, 1, FSimVec{10.0, 10.0}).Id;
	const int32 Second = AddMage(State, 1, FSimVec{11.0, 10.0}).Id;
	const FActor& Owner = *SimFindActor(State, Caster);
	// kernel.test.ts:85. Speed 600 and damage 6 are the sweep fixture.
	SpawnProjectile(State, MakeHit(Owner.Id, 1, 6.0, TEXT("magic"), 0, Owner.Pos), Owner.Pos, FSimVec{1.0, 0.0}, 600.0, 12.0);
	StepArena(State);
	bool bPass = Near(*this, TEXT("first body"), SimFindActor(State, First)->Hp, SimFindActor(State, First)->MaxHp - 6.0);
	bPass &= Near(*this, TEXT("second body untouched"), SimFindActor(State, Second)->Hp, SimFindActor(State, Second)->MaxHp);
	bPass &= TestEqual(TEXT("projectile is consumed"), State.Projectiles.Num(), 0);
	return bPass;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaKernelStaff, "MageArena.Kernel.Staff",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaKernelStaff::RunTest(const FString& Parameters)
{
	if (!Ready(*this))
	{
		return false;
	}
	FTraining Training = CreateTraining(TEXT("charge"));
	FActor& Dummy = *SimFindActor(Training.State, Training.DummyId);
	const FActor& Player = *SimFindActor(Training.State, Training.PlayerId);
	Dummy.Pos = FSimVec{Player.Pos.X + 1.0, Player.Pos.Y};
	Training.NextAttack = 1;
	for (int32 Index = 0; Index < SimTicks(0.9); ++Index)
	{
		FInputFrame Input = SimIdleInput(SimFindActor(Training.State, Training.DummyId)->Pos);
		Input.bCast = Index == 0;
		StepTraining(Training, Input);
	}
	bool bInterrupted = false;
	for (const FArenaEvent& Event : Training.State.Events)
	{
		if (Event.Kind == TEXT("interrupt") && Event.ActorId == Training.DummyId)
		{
			bInterrupted = true;
		}
	}
	bool bPass = TestTrue(TEXT("staff interrupts the charge"), bInterrupted);
	bPass &= Near(*this, TEXT("player unhurt"), SimFindActor(Training.State, Training.PlayerId)->Hp, SimFindActor(Training.State, Training.PlayerId)->MaxHp);
	bPass &= TestEqual(TEXT("charge cleared"), Training.State.Telegraphs.Num(), 0);
	return bPass;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaKernelClock, "MageArena.Kernel.Clock",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaKernelClock::RunTest(const FString& Parameters)
{
	if (!Ready(*this))
	{
		return false;
	}
	FArenaState State = CreateArena();
	const int32 ActorId = AddMage(State, 0, FSimVec{10.0, 10.0}).Id;
	FActor& Actor = *SimFindActor(State, ActorId);
	Actor.ClockAdvanceTicks = SimTicks(100.0);
	for (int32 Index = 0; Index < SimTicks(18.0); ++Index)
	{
		StepArena(State);
	}
	const int32 Expected[] = {0, SimTicks(6.0), SimTicks(12.0), SimTicks(18.0)};
	bool bPass = TestEqual(TEXT("unlock count"), Actor.UnlockTicks.Num(), 4);
	for (int32 Index = 0; Index < Actor.UnlockTicks.Num() && Index < 4; ++Index)
	{
		bPass &= TestEqual(*FString::Printf(TEXT("unlock %d"), Index), Actor.UnlockTicks[Index], Expected[Index]);
	}
	Actor.Hp = 45.0;
	const double Healed = 45.0 + (Actor.MaxHp - 45.0) * KernelData().HealFraction;
	ResetWave(State, Actor);
	bPass &= TestEqual(TEXT("wave tier"), Actor.Tier, 1);
	bPass &= Near(*this, TEXT("between-wave heal"), Actor.Hp, Healed);
	bPass &= TestEqual(TEXT("clock cleared"), Actor.ClockAdvanceTicks, 0);
	return bPass;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaKernelDeterminism, "MageArena.Kernel.Determinism",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaKernelDeterminism::RunTest(const FString& Parameters)
{
	if (!Ready(*this))
	{
		return false;
	}
	auto Replay = [](uint32 Seed)
	{
		FTraining Training = CreateTraining(TEXT("flanker"), Seed);
		TArray<FString> Hashes;
		const int32 Steps = SimTicks(120.0);
		for (int32 Index = 0; Index < Steps; ++Index)
		{
			const FActor& Dummy = *SimFindActor(Training.State, Training.DummyId);
			FInputFrame Input = SimIdleInput(Dummy.Pos);
			Input.Move = FSimVec{0.0, std::sin(static_cast<double>(Index) / 60.0)};
			Input.bCast = Index % 5 == 0;
			Input.bAbsorb = Index % 80 < 12;
			Input.bRoll = Index % 90 == 0;
			StepTraining(Training, Input);
			if (Index % 60 == 0)
			{
				Hashes.Add(StateHash(Training.State));
			}
		}
		return Hashes;
	};
	const TArray<FString> Left = Replay(17);
	const TArray<FString> Again = Replay(17);
	const TArray<FString> Other = Replay(18);
	bool bPass = TestEqual(TEXT("hash count"), Left.Num(), Again.Num());
	bool bSame = Left.Num() == Again.Num();
	for (int32 Index = 0; bSame && Index < Left.Num(); ++Index)
	{
		bSame &= Left[Index] == Again[Index];
	}
	bPass &= TestTrue(TEXT("seed 17 matches itself"), bSame);
	bool bDifferent = Left.Num() != Other.Num();
	for (int32 Index = 0; !bDifferent && Index < Left.Num(); ++Index)
	{
		bDifferent |= Left[Index] != Other[Index];
	}
	bPass &= TestTrue(TEXT("seed 18 diverges"), bDifferent);
	return bPass;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaKernelStepper, "MageArena.Kernel.Stepper",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaKernelStepper::RunTest(const FString& Parameters)
{
	if (!Ready(*this))
	{
		return false;
	}
	auto Run = [](int32 Fps)
	{
		FArenaState State = CreateArena();
		const int32 ActorId = AddMage(State, 0, FSimVec{10.0, 10.0}).Id;
		FFixedStepper Clock;
		FInputFrame Input = SimIdleInput();
		Input.Move = FSimVec{1.0, 0.0};
		Input.bAbsorb = true;
		for (int32 Index = 0; Index < Fps; ++Index)
		{
			Clock.Advance(1.0 / static_cast<double>(Fps), [&State, ActorId, Input]()
			{
				StepOne(State, ActorId, Input);
			});
		}
		const FActor& Actor = *SimFindActor(State, ActorId);
		return TTuple<double, double, double, int32>(Actor.Pos.X, Actor.Pos.Y, Actor.Mana, State.Tick);
	};
	const auto Slow = Run(30);
	const auto Fast = Run(60);
	bool bPass = TestTrue(TEXT("x matches"), Slow.Get<0>() == Fast.Get<0>());
	bPass &= TestTrue(TEXT("y matches"), Slow.Get<1>() == Fast.Get<1>());
	bPass &= TestTrue(TEXT("mana matches"), Slow.Get<2>() == Fast.Get<2>());
	bPass &= TestEqual(TEXT("tick matches"), Slow.Get<3>(), Fast.Get<3>());
	return bPass;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaKernelTiming, "MageArena.Kernel.Timing",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaKernelTiming::RunTest(const FString& Parameters)
{
	if (!Ready(*this))
	{
		return false;
	}
	const FTimingReport Perfect = RunTimingBot(TEXT("perfect"), TOptional<double>(30.0));
	const FTimingReport Late = RunTimingBot(TEXT("late"), TOptional<double>(30.0));
	const FTimingReport Holder = RunTimingBot(TEXT("holder"), TOptional<double>(30.0));
	const FTimingReport Never = RunTimingBot(TEXT("never"), TOptional<double>(30.0));
	bool bPass = TestTrue(TEXT("perfect rate"), Perfect.PerfectRate == 1.0);
	bPass &= TestEqual(TEXT("late perfects"), Late.Perfects, 0);
	bPass &= TestEqual(TEXT("holder perfects"), Holder.Perfects, 0);
	bPass &= TestEqual(TEXT("never perfects"), Never.Perfects, 0);
	bPass &= TestTrue(TEXT("perfect returns mana"), Perfect.ManaReturned > 0.0);
	bPass &= TestTrue(TEXT("holding drains more than perfecting"), Holder.ManaDrained > Perfect.ManaDrained);
	return bPass;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaKernelPerformance, "MageArena.Kernel.Performance",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaKernelPerformance::RunTest(const FString& Parameters)
{
	if (!Ready(*this))
	{
		return false;
	}
	const int32 Field = KernelData().PerformanceProjectiles;
	FTraining Training = CreateTraining(TEXT("performance"), 7);
	int32 MaxId = 0;
	bool bPass = true;
	for (int32 Index = 0; Index < SimTicks(9.0); ++Index)
	{
		const FSimVec Aim = SimFindActor(Training.State, Training.DummyId)->Pos;
		StepTraining(Training, SimIdleInput(Aim));
		bPass &= TestEqual(TEXT("field size"), Training.State.Projectiles.Num(), Field);
		for (const FProjectile& Projectile : Training.State.Projectiles)
		{
			bPass &= TestTrue(TEXT("field moves +x"), Projectile.Velocity.X > 0.0);
			bPass &= TestTrue(TEXT("field damage stays zero"), Projectile.Damage == 0.0);
			MaxId = std::max(MaxId, Projectile.Id);
		}
	}
	const FActor& Player = *SimFindActor(Training.State, Training.PlayerId);
	bPass &= TestTrue(TEXT("ids turn over"), MaxId > Field * 3);
	bPass &= TestTrue(TEXT("player unhurt"), Player.Hp == Player.MaxHp);
	return bPass;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaKernelStepCost, "MageArena.Kernel.StepCost",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaKernelStepCost::RunTest(const FString& Parameters)
{
	if (!Ready(*this))
	{
		return false;
	}
	FTraining Training = CreateTraining(TEXT("performance"), 7);
	const FSimVec Aim = SimFindActor(Training.State, Training.DummyId)->Pos;
	for (int32 Index = 0; Index < 20; ++Index)
	{
		StepTraining(Training, SimIdleInput(Aim));
	}
	constexpr int32 Samples = 120;
	double SumUs = 0.0;
	for (int32 Index = 0; Index < Samples; ++Index)
	{
		const double Start = FPlatformTime::Seconds();
		StepTraining(Training, SimIdleInput(Aim));
		SumUs += (FPlatformTime::Seconds() - Start) * 1.0e6;
	}
	const double MeanUs = SumUs / static_cast<double>(Samples);
	UE_LOG(LogMageArena, Display, TEXT("KernelStepUs=%.3f"), MeanUs);
	AddInfo(FString::Printf(TEXT("KernelStepUs=%.3f"), MeanUs));
	return TestTrue(TEXT("mean step is within 1 ms"), MeanUs <= 1000.0);
}

#endif
