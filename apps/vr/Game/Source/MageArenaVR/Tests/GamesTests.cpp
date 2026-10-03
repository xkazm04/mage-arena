#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Kernel/ArenaKernel.h"
#include "Kernel/Catalog.h"
#include "Kernel/Enemies.h"
#include "Kernel/Games.h"
#include "Kernel/Geometry.h"
#include "Kernel/MageAI.h"
#include "MageArenaVR.h"

#include <cmath>

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

const FEnemySpec* FindSpec(const FString& Id)
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

int32 CountEnemies(const FArenaState& State, const TCHAR* Id)
{
	int32 Count = 0;
	for (const FActor& Actor : State.Actors)
	{
		if (Actor.Enemy.IsSet() && Actor.Enemy->Id == Id)
		{
			++Count;
		}
	}
	return Count;
}

void SimulateEnemies(FArenaState& State, int32 Count)
{
	for (int32 Index = 0; Index < Count; ++Index)
	{
		StepArena(State, EnemyInputs(State));
		QueueDeathEffects(State);
	}
}

void StepInput(FArenaState& State, int32 ActorId, const FInputFrame& Input)
{
	TMap<int32, FInputFrame> Inputs;
	Inputs.Add(ActorId, Input);
	StepArena(State, Inputs);
}

FHit FixtureHit(int32 Owner, double Damage, const TCHAR* Family, const FSimVec& Source, const TCHAR* Delivery)
{
	FHit Hit;
	Hit.OwnerId = Owner;
	Hit.ActivationId = 100;
	Hit.Damage = Damage;
	Hit.Family = Family;
	Hit.Tier = 1;
	Hit.Source = Source;
	Hit.Delivery = Delivery;
	return Hit;
}

bool SameReport(const FFightReport& Left, const FFightReport& Right)
{
	return Left.Seed == Right.Seed && Left.Wave == Right.Wave && Left.Outcome == Right.Outcome
		&& Left.DurationS == Right.DurationS && Left.Hp == Right.Hp && Left.Mana == Right.Mana && Left.Stamina == Right.Stamina
		&& Left.Perfects == Right.Perfects && Left.IncomingHits == Right.IncomingHits
		&& Left.DamageDealt == Right.DamageDealt && Left.DamageTaken == Right.DamageTaken
		&& Left.DeadAirBuckets == Right.DeadAirBuckets && Left.Buckets == Right.Buckets
		&& Left.Hash == Right.Hash && Left.bFinite == Right.bFinite;
}

const TCHAR* RosterIds[] = {
	TEXT("conscript"), TEXT("shieldman"), TEXT("slinger"), TEXT("netter"),
	TEXT("cinder_hound"), TEXT("mire_maw"), TEXT("thornback"), TEXT("hush_moth"),
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaKernelRoster, "MageArena.Kernel.Roster",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaKernelRoster::RunTest(const FString& Parameters)
{
	if (!Ready(*this))
	{
		return false;
	}
	const TArray<FEnemySpec>& Roster = KernelData().Enemies;
	if (!TestEqual(TEXT("roster size"), Roster.Num(), 8))
	{
		return false;
	}
	bool bPass = true;
	for (int32 Index = 0; Index < 8; ++Index)
	{
		bPass &= TestEqual(FString::Printf(TEXT("roster %d"), Index), Roster[Index].Id, FString(RosterIds[Index]));
	}
	FArenaState State = CreateArena();
	for (const FEnemySpec& Spec : Roster)
	{
		const int32 Id = AddEnemy(State, Spec.Id, FSimVec{20.0, 10.0}).Id;
		const FActor& Enemy = *SimFindActor(State, Id);
		bPass &= TestEqual(TEXT("enemy hp"), Enemy.MaxHp, Spec.Hp);
		bPass &= TestTrue(TEXT("enemy speed"), Enemy.SpeedMps.IsSet() && Enemy.SpeedMps.GetValue() == Spec.SpeedMps);
	}
	bPass &= TestNull(TEXT("unknown enemy"), FindSpec(TEXT("invented")));
	return bPass;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaKernelEnemyBouts, "MageArena.Kernel.EnemyBouts",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaKernelEnemyBouts::RunTest(const FString& Parameters)
{
	if (!Ready(*this))
	{
		return false;
	}
	bool bPass = true;
	const FKernelData& Data = KernelData();

	{
		FArenaState State = CreateArena();
		const int32 PlayerId = AddMage(State, 0, FSimVec{10.0, 10.0}).Id;
		const int32 EnemyId = AddEnemy(State, TEXT("conscript"), FSimVec{12.0, 10.0}).Id;
		const FEnemySpec* Spec = FindSpec(TEXT("conscript"));
		bPass &= TestNotNull(TEXT("conscript"), Spec);
		SimulateEnemies(State, SimTicks(0.6));
		bPass &= TestEqual(TEXT("spear still winding"), SimFindActor(State, PlayerId)->Hp, SimFindActor(State, PlayerId)->MaxHp);
		SimulateEnemies(State, 1);
		const FActor& Player = *SimFindActor(State, PlayerId);
		bPass &= TestEqual(TEXT("spear damage"), Player.Hp, Player.MaxHp - Spec->Attacks[0].Damage);
		bPass &= TestTrue(TEXT("spear backoff"), SimFindActor(State, EnemyId)->Enemy->BackoffUntil > State.Tick);
	}

	{
		FArenaState State = CreateArena();
		const int32 PlayerId = AddMage(State, 0, FSimVec{10.0, 10.0}).Id;
		const int32 EnemyId = AddEnemy(State, TEXT("conscript"), FSimVec{12.0, 10.0}).Id;
		SimulateEnemies(State, 1);
		const FSimVec Aim = State.Telegraphs[0].Target;
		SimFindActor(State, PlayerId)->Pos.Y += 3.0;
		const TMap<int32, FInputFrame> Inputs = EnemyInputs(State);
		const FInputFrame* Held = Inputs.Find(EnemyId);
		bPass &= TestNotNull(TEXT("committed aim"), Held);
		if (Held)
		{
			bPass &= TestEqual(TEXT("aim x"), Held->Aim.X, Aim.X);
			bPass &= TestEqual(TEXT("aim y"), Held->Aim.Y, Aim.Y);
		}
		SimulateEnemies(State, SimTicks(0.6));
		const FSimVec Pos = SimFindActor(State, EnemyId)->Pos;
		SimulateEnemies(State, SimTicks(0.4));
		const FActor& Enemy = *SimFindActor(State, EnemyId);
		bPass &= TestEqual(TEXT("recovery x"), Enemy.Pos.X, Pos.X);
		bPass &= TestEqual(TEXT("recovery y"), Enemy.Pos.Y, Pos.Y);
		bPass &= TestEqual(TEXT("backoff schedule"), Enemy.Enemy->BackoffUntil - SimTicks(1.0), 1 + SimTicks(0.6 + 0.5));
	}

	{
		constexpr double Fixture = 10.0;
		FArenaState State = CreateArena();
		const int32 ShieldId = AddEnemy(State, TEXT("shieldman"), FSimVec{10.0, 10.0}).Id;
		const FEnemySpec* Shield = FindSpec(TEXT("shieldman"));
		FActor& ShieldActor = *SimFindActor(State, ShieldId);
		const FSimHitResult Front = ResolveHit(State, ShieldActor, FixtureHit(100, Fixture, TEXT("magic"), FSimVec{12.0, 10.0}, TEXT("projectile")));
		bPass &= TestEqual(TEXT("shield front"), Front.Damage, Fixture * Shield->FrontBlockMult.GetValue());
		const FSimHitResult Flank = ResolveHit(State, ShieldActor, FixtureHit(100, Fixture, TEXT("magic"), FSimVec{8.0, 10.0}, TEXT("projectile")));
		bPass &= TestEqual(TEXT("shield flank"), Flank.Damage, Fixture);
		const FSimHitResult Area = ResolveHit(State, ShieldActor, FixtureHit(100, Fixture, TEXT("magic"), FSimVec{12.0, 10.0}, TEXT("area")));
		bPass &= TestEqual(TEXT("shield area"), Area.Damage, Fixture);
		const int32 ThornId = AddEnemy(State, TEXT("thornback"), FSimVec{20.0, 10.0}).Id;
		const FEnemySpec* Thorn = FindSpec(TEXT("thornback"));
		FHit Bolt = FixtureHit(100, Fixture, TEXT("magic"), FSimVec{22.0, 10.0}, TEXT("projectile"));
		Bolt.bBolt = true;
		const FSimHitResult Armoured = ResolveHit(State, *SimFindActor(State, ThornId), Bolt);
		bPass &= TestEqual(TEXT("thorn bolt armour"), Armoured.Damage, Fixture * Thorn->ArmourMultVsBolt.GetValue());
	}

	{
		FArenaState State = CreateArena();
		const int32 PlayerId = AddMage(State, 0, FSimVec{10.0, 10.0}).Id;
		AddEnemy(State, TEXT("netter"), FSimVec{18.0, 10.0});
		SimulateEnemies(State, 1);
		bPass &= TestTrue(TEXT("net telegraph"), State.Telegraphs.Num() > 0 && State.Telegraphs[0].Kind == TEXT("area"));
		bPass &= TestEqual(TEXT("net not yet"), SimFindActor(State, PlayerId)->Water.RootUntil, 0);
		SimFindActor(State, PlayerId)->ImmuneUntil = SimTicks(2.0);
		SimulateEnemies(State, SimTicks(1.0));
		bPass &= TestEqual(TEXT("net during roll"), SimFindActor(State, PlayerId)->Water.RootUntil, 0);
		SimFindActor(State, PlayerId)->ImmuneUntil = 0;
		SimulateEnemies(State, SimTicks(6.0));
		const FActor& Player = *SimFindActor(State, PlayerId);
		bPass &= TestTrue(TEXT("net roots"), Player.Water.RootUntil > 0);
		bPass &= TestEqual(TEXT("net deals no hp"), Player.Hp, Player.MaxHp);
	}

	{
		FArenaState State = CreateArena();
		const int32 PlayerId = AddMage(State, 0, FSimVec{10.0, 10.0}).Id;
		const int32 First = AddEnemy(State, TEXT("cinder_hound"), FSimVec{11.0, 10.0}).Id;
		const int32 Second = AddEnemy(State, TEXT("cinder_hound"), FSimVec{10.0, 11.0}).Id;
		const int32 Third = AddEnemy(State, TEXT("cinder_hound"), FSimVec{9.0, 10.0}).Id;
		EnemyInputs(State);
		int32 Melee = 0;
		for (const FTelegraph& Telegraph : State.Telegraphs)
		{
			if (Telegraph.Kind == TEXT("melee"))
			{
				++Melee;
			}
		}
		bPass &= TestEqual(TEXT("hound slots"), Melee, 2);
		FActor& Lead = *SimFindActor(State, First);
		const FSimVec BurstPos = Lead.Pos;
		Lead.bDown = true;
		Lead.Hp = 0.0;
		State.Telegraphs.Reset();
		QueueDeathEffects(State);
		bPass &= TestEqual(TEXT("one burst"), State.Telegraphs.Num(), 1);
		bPass &= TestTrue(TEXT("burst survives"), State.Telegraphs[0].bSurvivesOwner);
		for (const int32 Id : TArray<int32>{Second, Third})
		{
			FActor& Other = *SimFindActor(State, Id);
			Other.bDown = true;
			Other.Enemy->bDeathQueued = true;
		}
		for (int32 Index = 0; Index < SimTicks(0.6); ++Index)
		{
			FInputFrame Input = SimIdleInput(BurstPos);
			Input.bAbsorb = Index >= SimTicks(0.5);
			StepInput(State, PlayerId, Input);
		}
		const FActor& Player = *SimFindActor(State, PlayerId);
		bPass &= TestEqual(TEXT("burst perfect"), Player.Metrics.Perfects, 1);
		bPass &= TestEqual(TEXT("burst absorbed"), Player.Hp, Player.MaxHp);
	}

	{
		FArenaState State = CreateArena();
		const int32 PlayerId = AddMage(State, 0, FSimVec{4.0, 10.0}).Id;
		const int32 MawId = AddEnemy(State, TEXT("mire_maw"), FSimVec{10.0, 10.0}).Id;
		EnemyInputs(State);
		bPass &= TestTrue(TEXT("glob"), State.Telegraphs.Num() > 0 && State.Telegraphs[0].Family == TEXT("magic"));
		State.Telegraphs.Reset();
		SimFindActor(State, MawId)->Enemy->ReadyTick = 0;
		EnemyInputs(State);
		bPass &= TestTrue(TEXT("pull"), State.Telegraphs.Num() > 0 && State.Telegraphs[0].PullM == 4.0);
		for (int32 Index = 0; Index < SimTicks(1.0) + 1; ++Index)
		{
			StepArena(State);
		}
		bPass &= Near(*this, TEXT("pull distance"), SimFindActor(State, PlayerId)->Pos.X, 8.0);

		FArenaState Charge = CreateArena();
		const double Edge = Data.ArenaCentre.X + Data.ArenaWidthM * 0.5;
		const double Y = Data.ArenaCentre.Y;
		const int32 ChargePlayer = AddMage(Charge, 0, FSimVec{Edge - 1.0, Y}).Id;
		const int32 ThornId = AddEnemy(Charge, TEXT("thornback"), FSimVec{Edge - 7.0, Y}).Id;
		SimulateEnemies(Charge, SimTicks(1.0) + 1);
		const FActor& Victim = *SimFindActor(Charge, ChargePlayer);
		const FEnemySpec* Thorn = FindSpec(TEXT("thornback"));
		bPass &= TestEqual(TEXT("charge damage"), Victim.Hp, Victim.MaxHp - Thorn->Attacks[0].Damage);
		const FActor& Charger = *SimFindActor(Charge, ThornId);
		bPass &= TestTrue(TEXT("wall stun"), Charger.Enemy->StunnedUntil > Charge.Tick);
		const FSimVec Expected = ConstrainToArena(FSimVec{Edge + 10.0, Y}, Charger.Radius);
		bPass &= TestEqual(TEXT("charge x"), Charger.Pos.X, Expected.X);
		bPass &= TestEqual(TEXT("charge y"), Charger.Pos.Y, Expected.Y);
	}

	{
		FArenaState State = CreateArena();
		const int32 PlayerId = AddMage(State, 0, FSimVec{10.0, 10.0}).Id;
		for (int32 Index = 0; Index < 6; ++Index)
		{
			AddEnemy(State, TEXT("hush_moth"), FSimVec{10.5, 10.0});
		}
		const FActor& Before = *SimFindActor(State, PlayerId);
		const double Regen = (Data.ManaRegenBase + Data.ManaRegenPerRank * static_cast<double>(Before.Ranks.Focus));
		const FEnemySpec* Moth = FindSpec(TEXT("hush_moth"));
		double Drain = 0.0;
		const FString& Effect = Moth->Attacks[0].Effect;
		const int32 DrainAt = Effect.Find(TEXT("drain "));
		if (DrainAt != INDEX_NONE)
		{
			Drain = FCString::Atod(*Effect.Mid(DrainAt + 6));
		}
		const double ExpectedMana = Before.MaxMana - 6.0 * Drain + Regen;
		SimulateEnemies(State, SimTicks(1.0));
		const FActor& Player = *SimFindActor(State, PlayerId);
		bPass &= Near(*this, TEXT("moth drain"), Player.Mana, ExpectedMana);
		bPass &= TestEqual(TEXT("moth hp"), Player.Hp, Player.MaxHp);

		FArenaState Fog = CreateArena();
		const int32 FogPlayer = AddMage(Fog, 0, FSimVec{10.0, 10.0}).Id;
		const int32 SlingerId = AddEnemy(Fog, TEXT("slinger"), FSimVec{20.0, 10.0}).Id;
		FZone Zone;
		Zone.Id = 100;
		Zone.OwnerId = FogPlayer;
		Zone.Kind = TEXT("fog");
		Zone.Pos = SimFindActor(Fog, FogPlayer)->Pos;
		Zone.RadiusM = 3.0;
		Zone.Until = 300;
		Zone.SlowMult = 1.0;
		Fog.Zones.Add(Zone);
		EnemyInputs(Fog);
		bPass &= TestEqual(TEXT("fog blocks slinger"), Fog.Telegraphs.Num(), 0);
		bPass &= TestEqual(TEXT("slinger holds attack"), SimFindActor(Fog, SlingerId)->Enemy->AttackIndex, 0);
	}
	return bPass;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaKernelCompetence, "MageArena.Kernel.Competence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaKernelCompetence::RunTest(const FString& Parameters)
{
	if (!Ready(*this))
	{
		return false;
	}
	bool bPass = true;
	const FKernelData& Data = KernelData();

	{
		FArenaState State = CreateArena();
		const int32 ActorId = AddMage(State, 0, FSimVec{10.0, 10.0}).Id;
		const int32 TargetId = AddMage(State, 1, FSimVec{12.0, 10.0}).Id;
		FActor& Actor = *SimFindActor(State, ActorId);
		Actor.Water.EncasedUntil = SimTicks(2.0);
		Actor.Mana = 10.0;
		Actor.Stamina = 10.0;
		const double ManaGain = (Data.ManaRegenBase + Data.ManaRegenPerRank * static_cast<double>(Actor.Ranks.Focus));
		const double StaminaGain = Data.StaminaRegenPerSecond;
		for (int32 Index = 0; Index < SimTicks(1.0); ++Index)
		{
			FInputFrame Input = SimIdleInput(SimFindActor(State, TargetId)->Pos);
			Input.bCast = true;
			Input.Move = FSimVec{1.0, 0.0};
			StepInput(State, ActorId, Input);
		}
		const FActor& After = *SimFindActor(State, ActorId);
		bPass &= Near(*this, TEXT("encased mana"), After.Mana, 10.0 + ManaGain);
		bPass &= Near(*this, TEXT("encased stamina"), After.Stamina, 10.0 + StaminaGain);
		bPass &= TestEqual(TEXT("encased casts"), After.Metrics.Casts, 0);

		FActor& Victim = *SimFindActor(State, TargetId);
		Victim.Hp = 1.0;
		FHit Hit;
		Hit.OwnerId = ActorId;
		Hit.ActivationId = 100;
		Hit.Family = TEXT("magic");
		Hit.Tier = 4;
		Hit.Damage = 100.0;
		Hit.Source = After.Pos;
		ResolveHit(State, Victim, Hit);
		bPass &= TestEqual(TEXT("overkill taken"), Victim.Metrics.DamageTaken, 1.0);
		bPass &= TestEqual(TEXT("overkill dealt"), SimFindActor(State, ActorId)->Metrics.DamageDealt, 1.0);
		bool bHit = false;
		for (const FArenaEvent& Event : State.Events)
		{
			if (Event.Kind == TEXT("hit"))
			{
				bHit = true;
				bPass &= TestEqual(TEXT("hit value"), Event.Value, 1.0);
			}
		}
		bPass &= TestTrue(TEXT("hit event"), bHit);
	}

	{
		bPass &= TestTrue(TEXT("competence rows"), Data.MageCompetence.Num() >= 2);
		const FMageCompetenceRow& Lo = Data.MageCompetence[0];
		const FMageCompetenceRow& Hi = Data.MageCompetence[1];
		const FMageProfile Mid = MageCompetence(1.5);
		bPass &= Near(*this, TEXT("mid delay"), Mid.ReactionDelayS, std::max(Data.MageCaps.ReactionDelayMinS, (Lo.ReactionDelayS + Hi.ReactionDelayS) * 0.5));
		bPass &= Near(*this, TEXT("mid absorb"), Mid.AbsorbChance, (Lo.AbsorbChance + Hi.AbsorbChance) * 0.5);
		bPass &= Near(*this, TEXT("mid perfect"), Mid.PerfectChance, std::min(Data.MageCaps.PerfectAbsorbChanceMax, (Lo.PerfectChance + Hi.PerfectChance) * 0.5));
		bPass &= Near(*this, TEXT("mid aim"), Mid.AimErrorDeg, (Lo.AimErrorDeg + Hi.AimErrorDeg) * 0.5);
		bPass &= Near(*this, TEXT("mid cadence"), Mid.DecisionCadenceS, (Lo.DecisionCadenceS + Hi.DecisionCadenceS) * 0.5);
		const FMageProfile Capped = MageCompetence(100.0);
		bPass &= TestTrue(TEXT("delay cap"), Capped.ReactionDelayS >= Data.MageCaps.ReactionDelayMinS);
		bPass &= TestTrue(TEXT("perfect cap"), Capped.PerfectChance <= Data.MageCaps.PerfectAbsorbChanceMax);
	}

	for (const double Level : {1.0, 1.5, 2.0, 3.0, 4.0})
	{
		FArenaState State = CreateArena(100);
		const int32 ActorId = AddMage(State, 0, FSimVec{10.0, 10.0}).Id;
		const int32 FoeId = AddMage(State, 1, FSimVec{20.0, 10.0}).Id;
		FActor& Actor = *SimFindActor(State, ActorId);
		const double MaxHp = Actor.MaxHp;
		const double MaxMana = Actor.MaxMana;
		const double MaxStamina = Actor.MaxStamina;
		const FSimRanks Ranks = Actor.Ranks;
		AttachMageAI(Actor, Level);
		bPass &= TestEqual(TEXT("ai hp"), Actor.MaxHp, MaxHp);
		bPass &= TestEqual(TEXT("ai mana"), Actor.MaxMana, MaxMana);
		bPass &= TestEqual(TEXT("ai stamina"), Actor.MaxStamina, MaxStamina);
		bPass &= TestEqual(TEXT("ai vigor"), Actor.Ranks.Vigor, Ranks.Vigor);
		bPass &= TestEqual(TEXT("ai focus"), Actor.Ranks.Focus, Ranks.Focus);
		bPass &= TestEqual(TEXT("ai nerve"), Actor.Ranks.Nerve, Ranks.Nerve);
		const FActor& Foe = *SimFindActor(State, FoeId);
		FHit Shot;
		Shot.OwnerId = FoeId;
		Shot.ActivationId = 99;
		Shot.Damage = 14.0;
		Shot.Family = TEXT("magic");
		Shot.Tier = 1;
		Shot.Source = Foe.Pos;
		SpawnProjectile(State, Shot, Foe.Pos, FSimVec{-1.0, 0.0}, 1.0, 20.0);
		const FMageProfile Profile = MageCompetence(Level);
		const int32 Delay = SimTicks(Profile.ReactionDelayS);
		for (int32 Index = 0; Index < Delay; ++Index)
		{
			MageInput(State, *SimFindActor(State, ActorId));
			State.Tick++;
		}
		bPass &= TestEqual(TEXT("no early reaction"), SimFindActor(State, ActorId)->MageAI->ReactionAges.Num(), 0);
		MageInput(State, *SimFindActor(State, ActorId));
		const FMageBrain& Brain = SimFindActor(State, ActorId)->MageAI.GetValue();
		bPass &= TestTrue(TEXT("reaction age"), Brain.ReactionAges.Num() > 0 && Brain.ReactionAges[0] >= Delay);
		const int32 Cadence = SimTicks(Profile.DecisionCadenceS);
		for (int32 Index = 1; Index < Brain.DecisionTicks.Num(); ++Index)
		{
			bPass &= TestTrue(TEXT("decision cadence"), Brain.DecisionTicks[Index] - Brain.DecisionTicks[Index - 1] >= Cadence);
		}
	}

	{
		FArenaState State = CreateArena(1);
		const int32 ActorId = AddMage(State, 0, FSimVec{10.0, 10.0}).Id;
		const int32 TargetId = AddMage(State, 1, FSimVec{17.0, 10.0}).Id;
		FActor& Actor = *SimFindActor(State, ActorId);
		AttachMageAI(Actor, 4.0);
		SimFindActor(State, TargetId)->Water.Decoy = FDecoy{FSimVec{17.0, 16.0}, 300};
		const FInputFrame Input = MageInput(State, Actor);
		bPass &= TestTrue(TEXT("decoy aim"), Input.Aim.Y > 14.0);
		const double Mana = Actor.Mana;
		StepInput(State, ActorId, Input);
		const FActor& After = *SimFindActor(State, ActorId);
		bPass &= TestEqual(TEXT("decoy cast"), After.Metrics.Casts, 1);
		bPass &= TestTrue(TEXT("decoy spent mana"), After.Mana < Mana);
	}
	return bPass;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaKernelGames, "MageArena.Kernel.Games",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaKernelGames::RunTest(const FString& Parameters)
{
	if (!Ready(*this))
	{
		return false;
	}
	bool bPass = true;
	const FKernelData& Data = KernelData();
	const FArenaTier& Tiro = Data.ArenaTiers[0];
	bPass &= TestEqual(TEXT("tiro id"), Tiro.Id, FString(TEXT("tiro")));
	FGames Rejected;
	bPass &= TestFalse(TEXT("invalid wave"), TryCreateGames(Rejected, 3, nullptr, 4, false));
	bPass &= TestFalse(TEXT("negative wave"), TryCreateGames(Rejected, 3, nullptr, -1, false));

	{
		FGames Games;
		bPass &= TestTrue(TEXT("create soldiers"), TryCreateGames(Games, 3));
		bPass &= TestEqual(TEXT("conscripts"), CountEnemies(Games.State, TEXT("conscript")), 4);
		bPass &= TestEqual(TEXT("slingers"), CountEnemies(Games.State, TEXT("slinger")), 2);
		bPass &= TestEqual(TEXT("spawn log wave"), Games.SpawnLog.Num() > 0 ? Games.SpawnLog[0].Wave : 0, 1);
		FGames Final;
		bPass &= TestTrue(TEXT("create final"), TryCreateGames(Final, 3, &Data.Presets[0], 3, false));
		bPass &= TestTrue(TEXT("final brain"), Final.State.Actors.Num() > 1 && Final.State.Actors[1].MageAI.IsSet());
		if (Final.State.Actors.Num() > 1 && Final.State.Actors[1].MageAI.IsSet())
		{
			bPass &= TestEqual(TEXT("final competence"), Final.State.Actors[1].MageAI->Competence, 1.5);
			bPass &= TestEqual(TEXT("proxy label"), Final.State.Actors[1].Label, FString(TEXT("Tiro entrant \u00B7 Water proxy")));
			bPass &= TestEqual(TEXT("mirror preset"), Final.State.Actors[1].Water.Composition.Name, Data.OpponentPresets.Last());
		}
	}

	{
		FGames Games;
		bPass &= TestTrue(TEXT("create advance"), TryCreateGames(Games, 3));
		bPass &= TestFalse(TEXT("advance early"), TryAdvanceGames(Games));
		FActor* Player = SimFindActor(Games.State, Games.PlayerId);
		Player->Hp = 45.0;
		Player->Mana = 2.0;
		for (FActor& Actor : Games.State.Actors)
		{
			if (Actor.Id == Games.PlayerId)
			{
				continue;
			}
			Actor.bDown = true;
			Actor.Hp = 0.0;
		}
		const FInputFrame Idle = SimIdleInput();
		StepGames(Games, &Idle);
		bPass &= TestEqual(TEXT("intermission"), Games.Phase, FString(TEXT("intermission")));
		const double Hp = SimFindActor(Games.State, Games.PlayerId)->Hp;
		bPass &= TestTrue(TEXT("advance"), TryAdvanceGames(Games));
		const FActor& Healed = *SimFindActor(Games.State, Games.PlayerId);
		bPass &= Near(*this, TEXT("heal once"), Healed.Hp, Hp + (Healed.MaxHp - Hp) * Data.HealFraction);
		bPass &= TestEqual(TEXT("mana refill"), Healed.Mana, Healed.MaxMana);
		bPass &= TestEqual(TEXT("tier reset"), Healed.Tier, 1);
		bPass &= TestEqual(TEXT("hounds"), CountEnemies(Games.State, TEXT("cinder_hound")), 3);
		bPass &= TestEqual(TEXT("maw"), CountEnemies(Games.State, TEXT("mire_maw")), 1);
		bPass &= TestFalse(TEXT("second advance"), TryAdvanceGames(Games));
	}

	{
		FGames Games;
		bPass &= TestTrue(TEXT("create loss"), TryCreateGames(Games, 3));
		Games.WavesCleared = 2;
		for (FActor& Actor : Games.State.Actors)
		{
			Actor.bDown = true;
			Actor.Hp = 0.0;
		}
		StepGames(Games);
		bPass &= TestEqual(TEXT("lost"), Games.Phase, FString(TEXT("lost")));
		bPass &= TestFalse(TEXT("no advance after loss"), TryAdvanceGames(Games));
		const FGamesResult* First = GamesResult(Games);
		const FGamesResult* Second = GamesResult(Games);
		bPass &= TestTrue(TEXT("result settled"), First && Second && First == Second);
		if (First)
		{
			bPass &= TestEqual(TEXT("loss gold"), First->Gold, Tiro.PayoutGold[1]);
			bPass &= TestEqual(TEXT("loss renown"), First->Renown, Tiro.Renown[1]);
		}
	}

	{
		FGames Games;
		bPass &= TestTrue(TEXT("create ladder"), TryCreateGames(Games, 3));
		for (int32 Wave = 0; Wave < 4; ++Wave)
		{
			for (FActor& Actor : Games.State.Actors)
			{
				if (Actor.Id == Games.PlayerId)
				{
					continue;
				}
				Actor.bDown = true;
				Actor.Hp = 0.0;
				if (Actor.Enemy.IsSet())
				{
					Actor.Enemy->bDeathQueued = true;
				}
			}
			Games.State.Projectiles.Reset();
			Games.State.Telegraphs.Reset();
			const FInputFrame Idle = SimIdleInput();
			StepGames(Games, &Idle);
			if (Wave < 3)
			{
				bPass &= TestTrue(TEXT("ladder advance"), TryAdvanceGames(Games));
			}
		}
		bPass &= TestEqual(TEXT("complete"), Games.Phase, FString(TEXT("complete")));
		const FGamesResult* Result = GamesResult(Games);
		bPass &= TestNotNull(TEXT("champion"), Result);
		if (Result)
		{
			bPass &= TestEqual(TEXT("kind"), Result->Kind, FString(TEXT("champion")));
			bPass &= TestEqual(TEXT("waves"), Result->WavesCleared, 4);
			bPass &= TestEqual(TEXT("gold"), Result->Gold, Tiro.PayoutGold.Last());
			bPass &= TestEqual(TEXT("renown"), Result->Renown, Tiro.Renown.Last());
			bPass &= TestTrue(TEXT("final reached"), Result->bFinalReached);
			bPass &= TestTrue(TEXT("final won"), Result->bFinalWon);
			int32 Sum = 0;
			for (const int32 Gold : Tiro.PayoutGold)
			{
				Sum += Gold;
			}
			bPass &= TestTrue(TEXT("not a sum"), Result->Gold != Sum);
		}
	}

	{
		const double Timeout = Data.FightTimeoutS;
		const FFightReport First = RunFight(40000, 2, Timeout);
		const FFightReport Second = RunFight(40000, 2, Timeout);
		bPass &= TestTrue(TEXT("fight replays"), SameReport(First, Second));
		bPass &= TestTrue(TEXT("fight finite"), First.bFinite);
		const FFightReport Other = RunFight(40001, 2, Timeout);
		bPass &= TestTrue(TEXT("seed changes hash"), Other.Hash != First.Hash);
	}
	return bPass;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaKernelFireCatalog, "MageArena.Kernel.FireCatalog",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaKernelFireCatalog::RunTest(const FString& Parameters)
{
	if (!Ready(*this))
	{
		return false;
	}
	const TCHAR* Ids[] = {TEXT("fire_bolt"), TEXT("fire_flick"), TEXT("fire_lance"), TEXT("fire_ring"), TEXT("fire_pyre"), TEXT("fire_sunfall")};
	bool bPass = TestEqual(TEXT("fire rows"), KernelData().FireSpells.Num(), 10);
	for (const TCHAR* Id : Ids)
	{
		bPass &= TestNotNull(FString::Printf(TEXT("fire %s"), Id), FindFireSpell(Id));
	}
	const FFireSpell* Bolt = FindFireSpell(TEXT("fire_bolt"));
	const FFireSpell* Sun = FindFireSpell(TEXT("fire_sunfall"));
	const FFireSpell* Kindle = FindFireSpell(TEXT("fire_kindle"));
	if (Bolt)
	{
		bPass &= TestEqual(TEXT("bolt family"), Bolt->Family, FString(TEXT("magic")));
		bPass &= TestTrue(TEXT("bolt notes kept"), Bolt->Notes.Contains(TEXT("Bolt button")));
	}
	if (Sun)
	{
		bPass &= TestEqual(TEXT("sunfall family"), Sun->Family, FString(TEXT("unblockable")));
	}
	if (Kindle)
	{
		bPass &= TestEqual(TEXT("kindle family"), Kindle->Family, FString(TEXT("n/a")));
	}
	return bPass;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaKernelMageStepCost, "MageArena.Kernel.MageStepCost",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaKernelMageStepCost::RunTest(const FString& Parameters)
{
	if (!Ready(*this))
	{
		return false;
	}
	const FComposition* Preset = nullptr;
	for (const FComposition& Candidate : KernelData().Presets)
	{
		if (Candidate.Name == KernelData().ReferencePreset)
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
	if (!TestTrue(TEXT("duel"), TryCreateGames(Games, 40000, Preset, 2, true)))
	{
		return false;
	}
	for (int32 Index = 0; Index < 20; ++Index)
	{
		StepGames(Games);
	}
	constexpr int32 Samples = 120;
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
	UE_LOG(LogMageArena, Display, TEXT("MageStepUs=%.3f"), MeanUs);
	AddInfo(FString::Printf(TEXT("MageStepUs=%.3f"), MeanUs));
	return TestTrue(TEXT("mean AI step is within 1 ms"), MeanUs <= 1000.0);
}

#endif
