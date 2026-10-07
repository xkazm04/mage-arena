#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Kernel/ArenaKernel.h"
#include "MageArenaVR.h"

// StateHash is what the determinism tests compare (KernelTests, SettingsTests, PauseTests, ReplayTests). A field it does not
// mix in is a field those tests cannot see drift in. This file perturbs every field of FArenaState, FActor, FProjectile,
// FTelegraph and FZone one at a time and requires the hash to move. It is written from the struct definitions in
// SimTypes.h, not from the hash, so a field missing from either side is caught by the other.

namespace
{
struct FCase
{
	const TCHAR* Name;
	TFunction<void(FArenaState&)> Apply;
};

#define CASE(Label, ...) Cases.Add({TEXT(Label), [](FArenaState& S) { __VA_ARGS__; }})
#define ACT S.Actors[0]
#define WAT S.Actors[0].Water
#define FIR S.Actors[0].Fire
#define MAG S.Actors[0].MageAI.GetValue()
#define PRJ S.Projectiles[0]
#define TEL S.Telegraphs[0]
#define ZON S.Zones[0]

// Every optional and array is populated, so a perturbation of its contents is a perturbation of something that exists.
FArenaState MakeBase()
{
	FArenaState S;
	S.Tick = 7;
	S.Seed = 3;
	S.Rng = 5;
	S.NextId = 11;
	FActor A;
	A.Id = 1;
	A.Label = TEXT("a");
	A.Hp = 50.0;
	A.MaxHp = 60.0;
	A.Mana = 20.0;
	A.MaxMana = 30.0;
	A.Stamina = 10.0;
	A.MaxStamina = 12.0;
	A.Water.Composition.Name = TEXT("n");
	A.Water.Composition.Lines = {TEXT("lash"), TEXT("mend"), TEXT("mire")};
	A.Water.Composition.Branches = {TEXT("A"), TEXT("B"), TEXT("A")};
	A.Water.LastLine = TEXT("lash");
	A.Water.Cooldowns.Add({TEXT("lash"), 3});
	A.Water.Decoy = FDecoy{FSimVec{1.0, 1.0}, 9};
	A.Fire.Cooldowns.Add({TEXT("fire_bolt"), 4});
	FFireTrail Trail;
	Trail.Family = TEXT("magic");
	Trail.SpellId = TEXT("fire_step");
	A.Fire.Trails.Add(Trail);
	FFireChannel Channel;
	Channel.SpellId = TEXT("fire_beam");
	Channel.Family = TEXT("magic");
	A.Fire.Channel = Channel;
	FEnemyBrain Brain;
	Brain.Id = TEXT("conscript");
	A.Enemy = Brain;
	FMageBrain Mage;
	Mage.Observed.Add(FMageObservation{4, 5, false});
	Mage.DecisionTicks.Add(2);
	Mage.ReactionAges.Add(3);
	A.MageAI = Mage;
	A.SpeedMps = 1.5;
	FPendingCast Pending;
	Pending.Kind = TEXT("spell");
	Pending.SpellId = TEXT("lash:1:base");
	Pending.DamageMult = 1.0;
	Pending.TargetId = 2;
	A.Pending = Pending;
	A.UnlockTicks.Add(0);
	S.Actors.Add(A);
	FProjectile P;
	P.Id = 2;
	P.Family = TEXT("magic");
	P.Delivery = TEXT("projectile");
	P.HitIds.Add(1);
	S.Projectiles.Add(P);
	FTelegraph T;
	T.Id = 3;
	T.Family = TEXT("magic");
	T.Kind = TEXT("area");
	S.Telegraphs.Add(T);
	FZone Z;
	Z.Id = 4;
	Z.Kind = TEXT("slow");
	S.Zones.Add(Z);
	S.Events.Add(FArenaEvent{1, TEXT("cast"), 1, TOptional<int32>(), 0.0});
	S.RandomLog.Add(FRandomDraw{1, TEXT("p"), 0.5});
	return S;
}

void BuildSimCases(TArray<FCase>& Cases)
{
	CASE("State.Tick", S.Tick += 1);
	CASE("State.Seed", S.Seed += 1);
	CASE("State.Rng", S.Rng += 1);
	CASE("State.NextId", S.NextId += 1);
	CASE("State.Events add", S.Events.Add(FArenaEvent{}));
	CASE("State.RandomLog add", S.RandomLog.Add(FRandomDraw{}));

	CASE("Actor.Id", ACT.Id += 1);
	CASE("Actor.Team", ACT.Team += 1);
	CASE("Actor.Label", ACT.Label += TEXT("x"));
	CASE("Actor.Pos.X", ACT.Pos.X += 1.0);
	CASE("Actor.Pos.Y", ACT.Pos.Y += 1.0);
	CASE("Actor.PreviousPos.X", ACT.PreviousPos.X += 1.0);
	CASE("Actor.PreviousPos.Y", ACT.PreviousPos.Y += 1.0);
	CASE("Actor.Facing.X", ACT.Facing.X += 1.0);
	CASE("Actor.Facing.Y", ACT.Facing.Y += 1.0);
	CASE("Actor.Radius", ACT.Radius += 1.0);
	CASE("Actor.Ranks.Vigor", ACT.Ranks.Vigor += 1);
	CASE("Actor.Ranks.Focus", ACT.Ranks.Focus += 1);
	CASE("Actor.Ranks.Nerve", ACT.Ranks.Nerve += 1);
	CASE("Actor.Hp", ACT.Hp += 1.0);
	CASE("Actor.MaxHp", ACT.MaxHp += 1.0);
	CASE("Actor.Mana", ACT.Mana += 1.0);
	CASE("Actor.MaxMana", ACT.MaxMana += 1.0);
	CASE("Actor.Stamina", ACT.Stamina += 1.0);
	CASE("Actor.MaxStamina", ACT.MaxStamina += 1.0);
	CASE("Actor.bDown", ACT.bDown = !ACT.bDown);
	CASE("Actor.bDummy", ACT.bDummy = !ACT.bDummy);
	CASE("Actor.bAbsorb", ACT.bAbsorb = !ACT.bAbsorb);
	CASE("Actor.AbsorbFreshTick", ACT.AbsorbFreshTick += 1);
	CASE("Actor.ReleaseTick", ACT.ReleaseTick += 1);
	CASE("Actor.bAbsorbExhausted", ACT.bAbsorbExhausted = !ACT.bAbsorbExhausted);
	CASE("Actor.SpeedMps reset", ACT.SpeedMps.Reset());
	CASE("Actor.SpeedMps value", ACT.SpeedMps = 2.5);
	CASE("Actor.RollUntil", ACT.RollUntil += 1);
	CASE("Actor.ImmuneUntil", ACT.ImmuneUntil += 1);
	CASE("Actor.RecoveryUntil", ACT.RecoveryUntil += 1);
	CASE("Actor.RollDirection.X", ACT.RollDirection.X += 1.0);
	CASE("Actor.StaminaUsedTick", ACT.StaminaUsedTick += 1);
	CASE("Actor.CooldownUntil", ACT.CooldownUntil += 1);
	CASE("Actor.WaveStartTick", ACT.WaveStartTick += 1);
	CASE("Actor.Tier", ACT.Tier += 1);
	CASE("Actor.LastUnlockTick", ACT.LastUnlockTick += 1);
	CASE("Actor.ClockAdvanceTicks", ACT.ClockAdvanceTicks += 1);
	CASE("Actor.UnlockTicks[0]", ACT.UnlockTicks[0] += 1);
	CASE("Actor.UnlockTicks add", ACT.UnlockTicks.Add(5));

	CASE("Actor.LastInput.Move.X", ACT.LastInput.Move.X += 1.0);
	CASE("Actor.LastInput.Aim.X", ACT.LastInput.Aim.X += 1.0);
	CASE("Actor.LastInput.Slot", ACT.LastInput.Slot += 1);
	CASE("Actor.LastInput.bCast", ACT.LastInput.bCast = !ACT.LastInput.bCast);
	CASE("Actor.LastInput.bAbsorb", ACT.LastInput.bAbsorb = !ACT.LastInput.bAbsorb);
	CASE("Actor.LastInput.bRoll", ACT.LastInput.bRoll = !ACT.LastInput.bRoll);
	CASE("Actor.LastInput.bSprint", ACT.LastInput.bSprint = !ACT.LastInput.bSprint);
	CASE("Actor.LastInput.bPlantStaff", ACT.LastInput.bPlantStaff = !ACT.LastInput.bPlantStaff);
	CASE("Actor.LastInput.bLiftStaff", ACT.LastInput.bLiftStaff = !ACT.LastInput.bLiftStaff);
	CASE("Actor.LastInput.bRaiseFireWall", ACT.LastInput.bRaiseFireWall = !ACT.LastInput.bRaiseFireWall);

	CASE("Actor.Metrics.Perfects", ACT.Metrics.Perfects += 1);
	CASE("Actor.Metrics.Blocks", ACT.Metrics.Blocks += 1);
	CASE("Actor.Metrics.Hits", ACT.Metrics.Hits += 1);
	CASE("Actor.Metrics.DamageDealt", ACT.Metrics.DamageDealt += 1.0);
	CASE("Actor.Metrics.DamageTaken", ACT.Metrics.DamageTaken += 1.0);
	CASE("Actor.Metrics.ManaDrained", ACT.Metrics.ManaDrained += 1.0);
	CASE("Actor.Metrics.ManaRaised", ACT.Metrics.ManaRaised += 1.0);
	CASE("Actor.Metrics.ManaReturned", ACT.Metrics.ManaReturned += 1.0);
	CASE("Actor.Metrics.Casts", ACT.Metrics.Casts += 1);
	CASE("Actor.Metrics.Rolls", ACT.Metrics.Rolls += 1);

	CASE("Water.Composition.Name", WAT.Composition.Name += TEXT("x"));
	CASE("Water.Composition.Lines[0]", WAT.Composition.Lines[0] += TEXT("x"));
	CASE("Water.Composition.Lines add", WAT.Composition.Lines.Add(TEXT("x")));
	CASE("Water.Composition.Branches.Lash", WAT.Composition.Branches.Lash += TEXT("x"));
	CASE("Water.Composition.Branches.Mirror", WAT.Composition.Branches.Mirror += TEXT("x"));
	CASE("Water.Composition.Branches.TideOrb", WAT.Composition.Branches.TideOrb += TEXT("x"));
	CASE("Water.Flow", WAT.Flow += 1);
	CASE("Water.LastLine", WAT.LastLine += TEXT("x"));
	CASE("Water.LastCastTick", WAT.LastCastTick += 1);
	CASE("Water.LastActivityTick", WAT.LastActivityTick += 1);
	CASE("Water.Cooldowns[0].Line", WAT.Cooldowns[0].Line += TEXT("x"));
	CASE("Water.Cooldowns[0].Until", WAT.Cooldowns[0].Until += 1);
	CASE("Water.Cooldowns add", WAT.Cooldowns.Add(FCooldownEntry{TEXT("x"), 1}));
	CASE("Water.Stored", WAT.Stored += 1.0);
	CASE("Water.RootUntil", WAT.RootUntil += 1);
	CASE("Water.EncasedUntil", WAT.EncasedUntil += 1);
	CASE("Water.SlowUntil", WAT.SlowUntil += 1);
	CASE("Water.SlowMult", WAT.SlowMult += 0.5);
	CASE("Water.WardUntil", WAT.WardUntil += 1);
	CASE("Water.SheenUntil", WAT.SheenUntil += 1);
	CASE("Water.HotUntil", WAT.HotUntil += 1);
	CASE("Water.HotPerTick", WAT.HotPerTick += 1.0);
	CASE("Water.Crests", WAT.Crests += 1);
	CASE("Water.Healing", WAT.Healing += 1.0);
	CASE("Water.ControlTicks", WAT.ControlTicks += 1);
	CASE("Water.Decoy reset", WAT.Decoy.Reset());
	CASE("Water.Decoy.Pos.X", WAT.Decoy->Pos.X += 1.0);
	CASE("Water.Decoy.Until", WAT.Decoy->Until += 1);

	CASE("Fire.bSchool", FIR.bSchool = !FIR.bSchool);
	CASE("Fire.Heat", FIR.Heat += 1.0);
	CASE("Fire.MaxHeat", FIR.MaxHeat += 1.0);
	CASE("Fire.LastGainTick", FIR.LastGainTick += 1);
	CASE("Fire.LockUntil", FIR.LockUntil += 1);
	CASE("Fire.LockValue", FIR.LockValue += 1.0);
	CASE("Fire.AbsorbDrainSpellMult", FIR.AbsorbDrainSpellMult += 1.0);
	CASE("Fire.AbsorbDrainSpellUntil", FIR.AbsorbDrainSpellUntil += 1);
	CASE("Fire.SunfallCastTick", FIR.SunfallCastTick += 1);
	CASE("Fire.SunfallTick", FIR.SunfallTick += 1);
	CASE("Fire.bSunfallLoosed", FIR.bSunfallLoosed = !FIR.bSunfallLoosed);
	CASE("Fire.Cooldowns[0].SpellId", FIR.Cooldowns[0].SpellId += TEXT("x"));
	CASE("Fire.Cooldowns[0].Until", FIR.Cooldowns[0].Until += 1);
	CASE("Fire.Cooldowns add", FIR.Cooldowns.Add(FFireCooldown{TEXT("x"), 1}));
	CASE("Fire.Trails[0].From.X", FIR.Trails[0].From.X += 1.0);
	CASE("Fire.Trails[0].To.X", FIR.Trails[0].To.X += 1.0);
	CASE("Fire.Trails[0].Until", FIR.Trails[0].Until += 1);
	CASE("Fire.Trails[0].NextTick", FIR.Trails[0].NextTick += 1);
	CASE("Fire.Trails[0].IntervalTicks", FIR.Trails[0].IntervalTicks += 1);
	CASE("Fire.Trails[0].Damage", FIR.Trails[0].Damage += 1.0);
	CASE("Fire.Trails[0].HeatOnHit", FIR.Trails[0].HeatOnHit += 1.0);
	CASE("Fire.Trails[0].Tier", FIR.Trails[0].Tier += 1);
	CASE("Fire.Trails[0].Family", FIR.Trails[0].Family += TEXT("x"));
	CASE("Fire.Trails[0].SpellId", FIR.Trails[0].SpellId += TEXT("x"));
	CASE("Fire.Trails add", FIR.Trails.Add(FFireTrail{}));
	CASE("Fire.Channel reset", FIR.Channel.Reset());
	CASE("Fire.Channel.SpellId", FIR.Channel->SpellId += TEXT("x"));
	CASE("Fire.Channel.Until", FIR.Channel->Until += 1);
	CASE("Fire.Channel.NextTick", FIR.Channel->NextTick += 1);
	CASE("Fire.Channel.IntervalTicks", FIR.Channel->IntervalTicks += 1);
	CASE("Fire.Channel.TicksDone", FIR.Channel->TicksDone += 1);
	CASE("Fire.Channel.Damage", FIR.Channel->Damage += 1.0);
	CASE("Fire.Channel.HeatOnHit", FIR.Channel->HeatOnHit += 1.0);
	CASE("Fire.Channel.RangeM", FIR.Channel->RangeM += 1.0);
	CASE("Fire.Channel.Tier", FIR.Channel->Tier += 1);
	CASE("Fire.Channel.Family", FIR.Channel->Family += TEXT("x"));
	CASE("Fire.Channel.bPierceShields", FIR.Channel->bPierceShields = !FIR.Channel->bPierceShields);
	CASE("Fire.Channel.bPerfectOnlyFirstTick", FIR.Channel->bPerfectOnlyFirstTick = !FIR.Channel->bPerfectOnlyFirstTick);

	CASE("Enemy reset", ACT.Enemy.Reset());
	CASE("Enemy.Id", ACT.Enemy->Id += TEXT("x"));
	CASE("Enemy.ReadyTick", ACT.Enemy->ReadyTick += 1);
	CASE("Enemy.BackoffUntil", ACT.Enemy->BackoffUntil += 1);
	CASE("Enemy.StunnedUntil", ACT.Enemy->StunnedUntil += 1);
	CASE("Enemy.AttackIndex", ACT.Enemy->AttackIndex += 1);
	CASE("Enemy.bDeathQueued", ACT.Enemy->bDeathQueued = !ACT.Enemy->bDeathQueued);

	CASE("MageAI reset", ACT.MageAI.Reset());
	CASE("MageAI.Competence", MAG.Competence += 1.0);
	CASE("MageAI.NextDecisionTick", MAG.NextDecisionTick += 1);
	CASE("MageAI.Input.Move.X", MAG.Input.Move.X += 1.0);
	CASE("MageAI.Input.Aim.X", MAG.Input.Aim.X += 1.0);
	CASE("MageAI.Input.Slot", MAG.Input.Slot += 1);
	CASE("MageAI.Input.bCast", MAG.Input.bCast = !MAG.Input.bCast);
	CASE("MageAI.Input.bAbsorb", MAG.Input.bAbsorb = !MAG.Input.bAbsorb);
	CASE("MageAI.Input.bRoll", MAG.Input.bRoll = !MAG.Input.bRoll);
	CASE("MageAI.Input.bSprint", MAG.Input.bSprint = !MAG.Input.bSprint);
	CASE("MageAI.Input.bPlantStaff", MAG.Input.bPlantStaff = !MAG.Input.bPlantStaff);
	CASE("MageAI.Input.bLiftStaff", MAG.Input.bLiftStaff = !MAG.Input.bLiftStaff);
	CASE("MageAI.Input.bRaiseFireWall", MAG.Input.bRaiseFireWall = !MAG.Input.bRaiseFireWall);
	CASE("MageAI.Observed[0].Id", MAG.Observed[0].Id += 1);
	CASE("MageAI.Observed[0].FirstSeenTick", MAG.Observed[0].FirstSeenTick += 1);
	CASE("MageAI.Observed[0].bReacted", MAG.Observed[0].bReacted = !MAG.Observed[0].bReacted);
	CASE("MageAI.Observed add", MAG.Observed.Add(FMageObservation{}));
	CASE("MageAI.DefendUntil", MAG.DefendUntil += 1);
	CASE("MageAI.PlannedRaiseTick", MAG.PlannedRaiseTick += 1);
	CASE("MageAI.PlannedReleaseTick", MAG.PlannedReleaseTick += 1);
	CASE("MageAI.TargetPoint.X", MAG.TargetPoint.X += 1.0);
	CASE("MageAI.DecisionTicks[0]", MAG.DecisionTicks[0] += 1);
	CASE("MageAI.DecisionTicks add", MAG.DecisionTicks.Add(9));
	CASE("MageAI.ReactionAges[0]", MAG.ReactionAges[0] += 1);
	CASE("MageAI.ReactionAges add", MAG.ReactionAges.Add(9));
	CASE("MageAI.WallSeenTick", MAG.WallSeenTick += 1);
	CASE("MageAI.WallLastSeenTick", MAG.WallLastSeenTick += 1);

	CASE("Pending reset", ACT.Pending.Reset());
	CASE("Pending.Kind", ACT.Pending->Kind += TEXT("x"));
	CASE("Pending.ReleaseTick", ACT.Pending->ReleaseTick += 1);
	CASE("Pending.StartTick", ACT.Pending->StartTick += 1);
	CASE("Pending.Aim.X", ACT.Pending->Aim.X += 1.0);
	CASE("Pending.ActivationId", ACT.Pending->ActivationId += 1);
	CASE("Pending.SpellId reset", ACT.Pending->SpellId.Reset());
	CASE("Pending.SpellId value", ACT.Pending->SpellId = FString(TEXT("other")));
	CASE("Pending.DamageMult reset", ACT.Pending->DamageMult.Reset());
	CASE("Pending.DamageMult value", ACT.Pending->DamageMult = 2.0);
	CASE("Pending.TargetId reset", ACT.Pending->TargetId.Reset());
	CASE("Pending.TargetId value", ACT.Pending->TargetId = 99);

	CASE("Projectile.Id", PRJ.Id += 1);
	CASE("Projectile.OwnerId", PRJ.OwnerId += 1);
	CASE("Projectile.ActivationId", PRJ.ActivationId += 1);
	CASE("Projectile.Damage", PRJ.Damage += 1.0);
	CASE("Projectile.Family", PRJ.Family += TEXT("x"));
	CASE("Projectile.Tier", PRJ.Tier += 1);
	CASE("Projectile.Source.X", PRJ.Source.X += 1.0);
	CASE("Projectile.bBolt", PRJ.bBolt = !PRJ.bBolt);
	CASE("Projectile.bReaction", PRJ.bReaction = !PRJ.bReaction);
	CASE("Projectile.Delivery", PRJ.Delivery += TEXT("x"));
	CASE("Projectile.Pos.X", PRJ.Pos.X += 1.0);
	CASE("Projectile.PreviousPos.X", PRJ.PreviousPos.X += 1.0);
	CASE("Projectile.Velocity.X", PRJ.Velocity.X += 1.0);
	CASE("Projectile.Radius", PRJ.Radius += 1.0);
	CASE("Projectile.RemainingM", PRJ.RemainingM += 1.0);
	CASE("Projectile.HitIds[0]", PRJ.HitIds[0] += 1);
	CASE("Projectile.HitIds add", PRJ.HitIds.Add(7));
	CASE("Projectile.BurstRadiusM", PRJ.BurstRadiusM += 1.0);
	CASE("Projectile.bPiercing", PRJ.bPiercing = !PRJ.bPiercing);
	CASE("Projectile.bReflected", PRJ.bReflected = !PRJ.bReflected);
	CASE("Projectile.HeatOnHit", PRJ.HeatOnHit += 1.0);
	CASE("Projectile.bSuppressPerfect", PRJ.bSuppressPerfect = !PRJ.bSuppressPerfect);
	CASE("Projectile.bPierceShields", PRJ.bPierceShields = !PRJ.bPierceShields);

	CASE("Telegraph.Id", TEL.Id += 1);
	CASE("Telegraph.ActivationId", TEL.ActivationId += 1);
	CASE("Telegraph.OwnerId", TEL.OwnerId += 1);
	CASE("Telegraph.Family", TEL.Family += TEXT("x"));
	CASE("Telegraph.Tier", TEL.Tier += 1);
	CASE("Telegraph.Damage", TEL.Damage += 1.0);
	CASE("Telegraph.Source.X", TEL.Source.X += 1.0);
	CASE("Telegraph.bBolt", TEL.bBolt = !TEL.bBolt);
	CASE("Telegraph.Kind", TEL.Kind += TEXT("x"));
	CASE("Telegraph.Origin.X", TEL.Origin.X += 1.0);
	CASE("Telegraph.Target.X", TEL.Target.X += 1.0);
	CASE("Telegraph.ResolveTick", TEL.ResolveTick += 1);
	CASE("Telegraph.StartTick", TEL.StartTick += 1);
	CASE("Telegraph.SpeedMps", TEL.SpeedMps += 1.0);
	CASE("Telegraph.RangeM", TEL.RangeM += 1.0);
	CASE("Telegraph.WidthM", TEL.WidthM += 1.0);
	CASE("Telegraph.RootS", TEL.RootS += 1.0);
	CASE("Telegraph.PullM", TEL.PullM += 1.0);
	CASE("Telegraph.bSurvivesOwner", TEL.bSurvivesOwner = !TEL.bSurvivesOwner);
	CASE("Telegraph.WallStunS", TEL.WallStunS += 1.0);
	CASE("Telegraph.HeatOnHit", TEL.HeatOnHit += 1.0);
	CASE("Telegraph.bSuppressPerfect", TEL.bSuppressPerfect = !TEL.bSuppressPerfect);
	CASE("Telegraph.bPierceShields", TEL.bPierceShields = !TEL.bPierceShields);
	CASE("Telegraph.bCommitted", TEL.bCommitted = !TEL.bCommitted);

	CASE("Zone.Id", ZON.Id += 1);
	CASE("Zone.OwnerId", ZON.OwnerId += 1);
	CASE("Zone.Pos.X", ZON.Pos.X += 1.0);
	CASE("Zone.RadiusM", ZON.RadiusM += 1.0);
	CASE("Zone.Until", ZON.Until += 1);
	CASE("Zone.Kind", ZON.Kind += TEXT("x"));
	CASE("Zone.SlowMult", ZON.SlowMult += 1.0);
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaKernelStateHashCoverage, "MageArena.Kernel.StateHashCoverage",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaKernelStateHashCoverage::RunTest(const FString& Parameters)
{
	const FArenaState Base = MakeBase();
	const FString BaseHash = StateHash(Base);
	TArray<FCase> Cases;
	BuildSimCases(Cases);

	TArray<FString> Missed;
	for (const FCase& Case : Cases)
	{
		FArenaState Copy = Base;
		Case.Apply(Copy);
		if (StateHash(Copy) == BaseHash)
		{
			Missed.Add(Case.Name);
		}
	}
	for (const FString& Name : Missed)
	{
		AddError(FString::Printf(TEXT("%s does not reach StateHash"), *Name));
	}
	UE_LOG(LogMageArena, Display, TEXT("StateHashCoverage perturbed=%d missed=%d"), Cases.Num(), Missed.Num());
	AddInfo(FString::Printf(TEXT("StateHashCoverage perturbed=%d missed=%d"), Cases.Num(), Missed.Num()));

	// SimTypes.h marks these as presentation only: "The sim does not read these". They stay out of the hash on purpose.
	struct FExcluded
	{
		const TCHAR* Name;
		TFunction<void(FArenaState&)> Apply;
	};
	const FExcluded Excluded[] = {
		{TEXT("Projectile.OriginPos"), [](FArenaState& S) { PRJ.OriginPos.X += 1.0; }},
		{TEXT("Projectile.AimedAt"), [](FArenaState& S) { PRJ.AimedAt.X += 1.0; }},
		{TEXT("Projectile.bHasAim"), [](FArenaState& S) { PRJ.bHasAim = !PRJ.bHasAim; }},
	};
	for (const FExcluded& Item : Excluded)
	{
		FArenaState Copy = Base;
		Item.Apply(Copy);
		TestEqual(*FString::Printf(TEXT("%s is presentation only and stays out of the hash"), Item.Name), StateHash(Copy), BaseHash);
	}
	return Missed.Num() == 0;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMageArenaKernelStateHashFieldGuard, "MageArena.Kernel.StateHashFieldGuard",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMageArenaKernelStateHashFieldGuard::RunTest(const FString& Parameters)
{
	// The perturbation list above is written by hand, so a field added to one of these structs is in neither it nor the hash
	// until someone adds it. These sizes change when a field is added or removed. When one fails: add the field to the
	// matching Mix function in StateHash.cpp and to the case list in this file (or to the presentation-only list in both),
	// then update the number. Win64 editor sizes, which is where this suite runs.
	TestEqual(TEXT("sizeof(FActor): a field was added or removed, see the comment"), static_cast<int32>(sizeof(FActor)), 1096);
	TestEqual(TEXT("sizeof(FProjectile): a field was added or removed, see the comment"), static_cast<int32>(sizeof(FProjectile)), 240);
	TestEqual(TEXT("sizeof(FTelegraph): a field was added or removed, see the comment"), static_cast<int32>(sizeof(FTelegraph)), 200);
	TestEqual(TEXT("sizeof(FZone): a field was added or removed, see the comment"), static_cast<int32>(sizeof(FZone)), 64);
	return true;
}

#endif
