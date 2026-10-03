#include "Kernel/Enemies.h"

#include "Kernel/ArenaKernel.h"
#include "Kernel/Water.h"

#include "Algo/Sort.h"
#include "Internationalization/Regex.h"

#include <cmath>

namespace
{
double Extract(const FString& Text, const FString& Pattern)
{
	const FRegexPattern Compiled(Pattern);
	FRegexMatcher Matcher(Compiled, Text);
	checkf(Matcher.FindNext(), TEXT("Missing enemy magnitude: %s / %s"), *Text, *Pattern);
	return FCString::Atod(*Matcher.GetCaptureGroup(1));
}

void ScheduleAttack(FArenaState& State, FActor& Actor, const FActor& Target, const FEnemySpec& Spec, const FAttackSpec& Attack)
{
	const FKernelData& Data = KernelData();
	const int32 Id = State.NextId++;
	FTelegraph Telegraph;
	Telegraph.Id = Id;
	Telegraph.ActivationId = Id;
	Telegraph.OwnerId = Actor.Id;
	Telegraph.Family = Attack.Family;
	Telegraph.Tier = Attack.Tier.Get(0);
	Telegraph.Damage = Attack.Damage;
	Telegraph.Source = Actor.Pos;
	Telegraph.Kind = Attack.ProjectileMps.IsSet() ? TEXT("projectile") : TEXT("melee");
	Telegraph.Origin = Actor.Pos;
	Telegraph.Target = Target.Pos;
	Telegraph.StartTick = State.Tick + 1;
	Telegraph.ResolveTick = State.Tick + 1 + SimTicks(Attack.WindupS);
	Telegraph.SpeedMps = Attack.ProjectileMps.Get(0.0);
	Telegraph.RangeM = Attack.RangeM.Get(Attack.ProjectileMps.IsSet() ? Data.RangedRangeM : Data.DefaultMeleeRangeM);
	Telegraph.WidthM = Data.MeleeArcDeg;
	if (Attack.Id == TEXT("net_cast"))
	{
		Telegraph.Kind = TEXT("area");
		Telegraph.RangeM = Data.NetRangeM;
		Telegraph.WidthM = Extract(Attack.Telegraph, TEXT("([\\d.]+) m"));
		Telegraph.RootS = Extract(Attack.Effect, TEXT("root ([\\d.]+) s"));
	}
	if (Attack.Id == TEXT("tongue_pull"))
	{
		Telegraph.Kind = TEXT("lane");
		Telegraph.RangeM = Data.TongueRangeM;
		Telegraph.WidthM = Data.TongueWidthM;
		Telegraph.PullM = Extract(Attack.Effect, TEXT("pull ([\\d.]+) m"));
	}
	if (Attack.Id == TEXT("thorn_charge"))
	{
		Telegraph.Kind = TEXT("charge");
		Telegraph.WidthM = Extract(Attack.Telegraph, TEXT("lane ([\\d.]+) m"));
		Telegraph.RangeM = Extract(Attack.Telegraph, TEXT("x ([\\d.]+) m"));
		Telegraph.WallStunS = Extract(Attack.OnWallHit, TEXT("self-stun ([\\d.]+) s"));
	}
	State.Telegraphs.Add(Telegraph);
	FEnemyBrain& Brain = Actor.Enemy.GetValue();
	Brain.AttackIndex++;
	double Cooldown = Attack.WindupS + Attack.RecoveryS.Get(Data.DefaultRecoveryS);
	if (Spec.Id == TEXT("mire_maw"))
	{
		Cooldown = Data.MawArtilleryCooldownS;
	}
	else if (Spec.Id == TEXT("thornback"))
	{
		Cooldown = Data.ChargeCooldownS;
	}
	if (Attack.CooldownS.IsSet())
	{
		Cooldown = Attack.CooldownS.GetValue();
	}
	Brain.ReadyTick = State.Tick + 1 + SimTicks(Cooldown);
	if (Spec.Id == TEXT("conscript"))
	{
		Brain.BackoffUntil = Brain.ReadyTick + SimTicks(Extract(Spec.Behaviour, TEXT("back off ([\\d.]+) s")));
		Brain.ReadyTick = Brain.BackoffUntil;
	}
}
}

const FEnemySpec* EnemySpecFor(const FActor& Actor)
{
	if (!Actor.Enemy.IsSet())
	{
		return nullptr;
	}
	for (const FEnemySpec& Spec : KernelData().Enemies)
	{
		if (Spec.Id == Actor.Enemy->Id)
		{
			return &Spec;
		}
	}
	return nullptr;
}

FActor& AddEnemy(FArenaState& State, const FString& Id, const FSimVec& Pos)
{
	const FEnemySpec* Spec = nullptr;
	for (const FEnemySpec& Candidate : KernelData().Enemies)
	{
		if (Candidate.Id == Id)
		{
			Spec = &Candidate;
			break;
		}
	}
	checkf(Spec, TEXT("Unknown enemy %s"), *Id);
	FActor& Actor = AddMage(State, 1, Pos, Spec->Name);
	Actor.Hp = Spec->Hp;
	Actor.MaxHp = Spec->Hp;
	Actor.SpeedMps = Spec->SpeedMps;
	FEnemyBrain Brain;
	Brain.Id = Id;
	Brain.ReadyTick = State.Tick;
	Actor.Enemy = Brain;
	return Actor;
}

TMap<int32, FInputFrame> EnemyInputs(FArenaState& State)
{
	const FKernelData& Data = KernelData();
	TMap<int32, FInputFrame> Inputs;
	TArray<FActor*> Enemies;
	TArray<FActor*> Hounds;
	for (FActor& Actor : State.Actors)
	{
		if (Actor.Enemy.IsSet() && !Actor.bDown)
		{
			Enemies.Add(&Actor);
			if (Actor.Enemy->Id == TEXT("cinder_hound"))
			{
				Hounds.Add(&Actor);
			}
		}
	}
	for (FActor* ActorPtr : Enemies)
	{
		FActor& Actor = *ActorPtr;
		const FEnemySpec* Spec = EnemySpecFor(Actor);
		check(Spec);
		FEnemyBrain& Brain = Actor.Enemy.GetValue();
		TArray<FActor*> Targets;
		for (FActor& Candidate : State.Actors)
		{
			if (Candidate.Team != Actor.Team && !Candidate.bDown
				&& (SimDistance(Actor.Pos, Candidate.Pos) <= Data.ContactRangeM || HasLineOfSight(State, Actor.Pos, Candidate.Pos)))
			{
				Targets.Add(&Candidate);
			}
		}
		const bool bHush = Spec->Id == TEXT("hush_moth");
		Algo::Sort(Targets, [&Actor, bHush](const FActor* Left, const FActor* Right)
		{
			if (bHush && Left->bAbsorb != Right->bAbsorb)
			{
				return Left->bAbsorb;
			}
			const double Delta = SimDistance(Actor.Pos, Left->Pos) - SimDistance(Actor.Pos, Right->Pos);
			if (Delta < 0.0)
			{
				return true;
			}
			if (Delta > 0.0)
			{
				return false;
			}
			return Left->Id < Right->Id;
		});
		if (Targets.Num() == 0)
		{
			continue;
		}
		FActor& Target = *Targets[0];
		FInputFrame Input = SimIdleInput(Target.Pos);
		Inputs.FindOrAdd(Actor.Id) = Input;
		if (State.Tick < Brain.StunnedUntil || State.Tick < Actor.Water.EncasedUntil)
		{
			continue;
		}
		const FTelegraph* Windup = nullptr;
		for (const FTelegraph& Telegraph : State.Telegraphs)
		{
			if (Telegraph.OwnerId == Actor.Id && !Telegraph.bSurvivesOwner)
			{
				Windup = &Telegraph;
				break;
			}
		}
		if (Windup)
		{
			Input.Aim = Windup->Target;
			Inputs.FindOrAdd(Actor.Id) = Input;
			continue;
		}
		if (Spec->Id == TEXT("conscript") && State.Tick < Brain.BackoffUntil - SimTicks(Extract(Spec->Behaviour, TEXT("back off ([\\d.]+) s"))))
		{
			continue;
		}
		const FSimVec Delta = SimSub(Target.Pos, Actor.Pos);
		const double Distance = SimDistance(Target.Pos, Actor.Pos);
		const FSimVec Toward = SimUnit(Delta);
		FSimVec Desired = Toward;
		const FAttackSpec& Attack = Spec->Attacks[Brain.AttackIndex % Spec->Attacks.Num()];
		double AttackRange = Attack.RangeM.Get(Data.DefaultMeleeRangeM);
		if (Attack.ProjectileMps.IsSet())
		{
			AttackRange = Data.RangedRangeM;
		}
		if (Attack.Id == TEXT("net_cast"))
		{
			AttackRange = Data.NetRangeM;
		}
		if (Attack.Id == TEXT("tongue_pull"))
		{
			AttackRange = Data.TongueRangeM;
		}
		if (Attack.Id == TEXT("thorn_charge"))
		{
			AttackRange = Extract(Attack.Telegraph, TEXT("x ([\\d.]+) m"));
		}
		if (Spec->KeepMinM.IsSet())
		{
			if (Distance < Spec->KeepMinM.GetValue())
			{
				Desired = FSimVec{-Toward.X, -Toward.Y};
			}
			else if (Distance > Spec->KeepMaxM.GetValue())
			{
				Desired = Toward;
			}
			else
			{
				Desired = FSimVec{-Toward.Y, Toward.X};
			}
		}
		else if (Spec->Id == TEXT("mire_maw"))
		{
			Desired = Distance <= AttackRange ? FSimVec{0.0, 0.0} : Toward;
		}
		else if (Spec->Id == TEXT("netter"))
		{
			if (Distance < Data.NetRangeM * 0.5)
			{
				Desired = FSimVec{-Toward.X, -Toward.Y};
			}
			else if (Distance > Data.NetRangeM)
			{
				Desired = Toward;
			}
			else
			{
				Desired = FSimVec{-Toward.Y, Toward.X};
			}
		}
		else if (Spec->Id == TEXT("conscript") && State.Tick < Brain.BackoffUntil)
		{
			Desired = Distance < Data.ConscriptBackoffM ? FSimVec{-Toward.X, -Toward.Y} : FSimVec{0.0, 0.0};
		}
		else if (Distance < AttackRange * 0.8)
		{
			Desired = FSimVec{0.0, 0.0};
		}
		int32 HoundIndex = INDEX_NONE;
		for (int32 Index = 0; Index < Hounds.Num(); ++Index)
		{
			if (Hounds[Index]->Id == Actor.Id)
			{
				HoundIndex = Index;
				break;
			}
		}
		const bool bEngaged = Spec->Id != TEXT("cinder_hound") || HoundIndex < static_cast<int32>(std::llround(Extract(Spec->Behaviour, TEXT("uses (\\d+) engagement slots"))));
		if (!bEngaged)
		{
			Desired = Distance > Data.HoundOrbitM ? Toward : FSimVec{-Toward.Y, Toward.X};
		}
		if (Spec->Id == TEXT("hush_moth"))
		{
			Desired = Distance > Data.ContactRangeM ? Toward : FSimVec{0.0, 0.0};
			if (Distance <= Data.ContactRangeM && State.Tick + 1 >= Target.ImmuneUntil && State.Tick + 1 >= Target.Water.EncasedUntil)
			{
				const double Drain = Extract(Attack.Effect, TEXT("drain ([\\d.]+) mana")) / Data.SimStepHz;
				Target.Mana = std::max(0.0, Target.Mana - Drain);
				if (Target.Mana == 0.0)
				{
					Target.bAbsorb = false;
					Target.bAbsorbExhausted = true;
				}
			}
		}
		else if (bEngaged && Distance <= AttackRange && State.Tick + 1 >= Brain.ReadyTick)
		{
			ScheduleAttack(State, Actor, Target, *Spec, Attack);
			Desired = FSimVec{0.0, 0.0};
		}
		if (Desired.X != 0.0 || Desired.Y != 0.0)
		{
			for (FActor* Other : Enemies)
			{
				if (Other->Id == Actor.Id)
				{
					continue;
				}
				const double Gap = SimDistance(Actor.Pos, Other->Pos);
				if (Gap > 0.0 && Gap < Data.SeparationM)
				{
					const FSimVec Away = SimUnit(SimSub(Actor.Pos, Other->Pos));
					Desired.X += Away.X * Data.SeparationWeight;
					Desired.Y += Away.Y * Data.SeparationWeight;
				}
			}
			Input.Move = SimUnit(Desired);
		}
		Inputs.FindOrAdd(Actor.Id) = Input;
	}
	return Inputs;
}

void QueueDeathEffects(FArenaState& State)
{
	for (FActor& Actor : State.Actors)
	{
		if (!Actor.bDown || !Actor.Enemy.IsSet() || Actor.Enemy->bDeathQueued)
		{
			continue;
		}
		Actor.Enemy->bDeathQueued = true;
		const FEnemySpec* Spec = EnemySpecFor(Actor);
		check(Spec);
		if (!Spec->OnDeath.IsSet())
		{
			continue;
		}
		const FDeathSpec& Death = Spec->OnDeath.GetValue();
		const int32 Id = State.NextId++;
		FTelegraph Telegraph;
		Telegraph.Id = Id;
		Telegraph.ActivationId = Id;
		Telegraph.OwnerId = Actor.Id;
		Telegraph.Family = Death.Family;
		Telegraph.Tier = Death.Tier;
		Telegraph.Damage = Death.Damage;
		Telegraph.Source = Actor.Pos;
		Telegraph.Kind = TEXT("area");
		Telegraph.Origin = Actor.Pos;
		Telegraph.Target = Actor.Pos;
		Telegraph.StartTick = State.Tick;
		Telegraph.ResolveTick = State.Tick + SimTicks(Death.DelayS);
		Telegraph.SpeedMps = 0.0;
		Telegraph.RangeM = Death.RadiusM;
		Telegraph.WidthM = Death.RadiusM;
		Telegraph.bSurvivesOwner = true;
		State.Telegraphs.Add(Telegraph);
	}
}
