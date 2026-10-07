#include "Kernel/ArenaKernel.h"
#include "Kernel/ArenaThreats.h"

#include "MageArenaVR.h"
#include "Kernel/Catalog.h"
#include "Kernel/Enemies.h"
#include "Kernel/Fire.h"
#include "Kernel/Geometry.h"
#include "Kernel/VrRules.h"
#include "Kernel/Water.h"

#include <cmath>
#include <limits>

namespace
{
double Maximum(double Base, double PerRank, double Rank)
{
	return Base + PerRank * Rank;
}

void UpdateWard(FArenaState& State, FActor& Actor, const FInputFrame& Input, const FVrRuleset* Rules)
{
	const FKernelData& Data = KernelData();
	if (!Input.bAbsorb)
	{
		if (Actor.LastInput.bAbsorb || Actor.bAbsorb)
		{
			Actor.ReleaseTick = State.Tick;
		}
		Actor.bAbsorb = false;
		Actor.bAbsorbExhausted = false;
	}
	else if (!Actor.bAbsorb && !Actor.bAbsorbExhausted && State.Tick >= Actor.RollUntil && !Actor.Pending.IsSet()
		&& State.Tick - Actor.ReleaseTick >= SimTicks(Data.Absorb.MinReleaseS))
	{
		if (Actor.Mana >= Data.Absorb.RaiseCostMana)
		{
			Actor.Mana -= Data.Absorb.RaiseCostMana;
			Actor.Metrics.ManaRaised += Data.Absorb.RaiseCostMana;
			Actor.bAbsorb = true;
			Actor.AbsorbFreshTick = State.Tick;
		}
		else
		{
			Actor.bAbsorbExhausted = true;
		}
	}
	if (Actor.bAbsorb)
	{
		double Cost = Data.Absorb.DrainPerSecond(Actor.Ranks.Nerve) * WardDrainMult(State, Actor) * SimDt();
		if (Actor.Fire.bSchool)
		{
			Cost *= FireAbsorbDrainMult(State, Actor);
		}
		if (Rules && Rules->bActive && Rules->IsSplitCasting(Actor.Id) && Rules->Defence.WardDrainSplit != 1.0)
		{
			Cost *= Rules->Defence.WardDrainSplit;
		}
		const double Paid = std::min(Actor.Mana, Cost);
		Actor.Mana -= Paid;
		Actor.Metrics.ManaDrained += Paid;
		if (Paid < Cost || Actor.Mana <= ManaExhaustedEpsilon)
		{
			Actor.Mana = std::max(0.0, Actor.Mana);
			Actor.bAbsorb = false;
			Actor.bAbsorbExhausted = true;
		}
	}
}

void MoveActor(FArenaState& State, FActor& Actor, const FInputFrame& Input, FVrRuleset* Rules)
{
	const FKernelData& Data = KernelData();
	Actor.PreviousPos = Actor.Pos;
	const bool bRooted = State.Tick < Actor.Water.RootUntil
		|| (Actor.Pending.IsSet() && Actor.Pending->SpellId.IsSet() && Actor.Pending->SpellId.GetValue() == TEXT("mend:4:base"))
		|| FireCastRoots(Actor);
	const bool bBlinkEdge = Input.bRoll && !Actor.LastInput.bRoll;
	const bool bAnchored = Rules && Rules->IsPlanted(Actor.Id) && bBlinkEdge;
	if (bAnchored)
	{
		Rules->Lift(TEXT("blink"));
	}
	else if (!bRooted && bBlinkEdge && State.Tick >= Actor.RollUntil && State.Tick >= Actor.RecoveryUntil
		&& Actor.Stamina >= Data.RollStaminaCost)
	{
		Actor.RollDirection = SimUnit(Input.Move, Actor.Facing);
		Actor.RollUntil = State.Tick + SimTicks(Data.RollDurationS);
		Actor.ImmuneUntil = State.Tick + SimTicks(Data.RollIFramesS);
		Actor.RecoveryUntil = Actor.RollUntil + SimTicks(Data.RollRecoveryS);
		Actor.Stamina -= Data.RollStaminaCost;
		Actor.StaminaUsedTick = State.Tick;
		Actor.Metrics.Rolls++;
		Actor.bAbsorb = false;
		if (Input.bAbsorb)
		{
			Actor.bAbsorbExhausted = true;
		}
		Interrupt(State, Actor);
		Emit(State, TEXT("roll"), Actor);
	}
	FSimVec Velocity{0.0, 0.0};
	if (State.Tick < Actor.RollUntil)
	{
		const double Speed = Data.RollDistanceM / (static_cast<double>(SimTicks(Data.RollDurationS)) * SimDt());
		Velocity = SimScale(Actor.RollDirection, Speed);
	}
	else if (!bRooted && SimLength(Input.Move) > 0.0)
	{
		const FSimVec Move = SimUnit(Input.Move);
		double Speed = Actor.SpeedMps.Get(Data.WalkMps);
		const double SprintCost = Data.SprintStaminaPerSecond * SimDt();
		if (Input.bSprint && !Actor.bAbsorb && !Actor.Pending.IsSet() && Actor.Stamina >= SprintCost)
		{
			Speed = Data.SprintMps;
			Actor.Stamina -= SprintCost;
			Actor.StaminaUsedTick = State.Tick;
		}
		if (Actor.bAbsorb)
		{
			Speed *= Data.Absorb.MoveSpeedWhileHeldMult;
		}
		if (State.Tick < Actor.Water.SlowUntil)
		{
			Speed *= Actor.Water.SlowMult;
		}
		Velocity = SimScale(Move, Speed);
	}
	Actor.Pos = ConstrainToArena(SimAdd(Actor.Pos, SimScale(Velocity, SimDt())), Actor.Radius);
}

void ReleaseCast(FArenaState& State, FActor& Actor, FVrRuleset* Rules)
{
	if (!Actor.Pending.IsSet() || State.Tick < Actor.Pending->ReleaseTick)
	{
		return;
	}
	if (Actor.Pending->Kind == TEXT("spell"))
	{
		ReleaseSpell(State, Actor);
		return;
	}
	if (Actor.Pending->Kind == TEXT("fire"))
	{
		ReleaseFire(State, Actor, Rules);
		return;
	}
	const FKernelData& Data = KernelData();
	const FPendingCast Pending = Actor.Pending.GetValue();
	Actor.Pending.Reset();
	const FSimVec Direction = SimUnit(SimSub(Pending.Aim, Actor.Pos), Actor.Facing);
	// kernel.ts:128. Live bolts go through trySpellCast. This branch remains because the TS still has it.
	if (Pending.Kind == TEXT("bolt"))
	{
		FHit Hit;
		Hit.OwnerId = Actor.Id;
		Hit.ActivationId = Pending.ActivationId;
		Hit.Damage = Data.BoltDamage;
		if (Rules && Rules->bActive && Actor.Team == 0 && Rules->Power.BoltDamage != 1.0)
		{
			Hit.Damage *= Rules->Power.BoltDamage;
		}
		Hit.Family = TEXT("magic");
		Hit.Tier = 0;
		Hit.Source = Actor.Pos;
		Hit.bBolt = true;
		SpawnProjectile(State, Hit, Actor.Pos, Direction, Data.BoltSpeedMps, Data.BoltRangeM);
	}
	else
	{
		for (FActor& Target : State.Actors)
		{
			if (Target.Team == Actor.Team || Target.bDown || SimDistance(Actor.Pos, Target.Pos) > Data.StaffRangeM
				|| !SimInArc(Direction, SimSub(Target.Pos, Actor.Pos), Data.StaffArcDeg))
			{
				continue;
			}
			FHit Hit;
			Hit.OwnerId = Actor.Id;
			Hit.ActivationId = Pending.ActivationId;
			Hit.Damage = Data.StaffDamage;
			Hit.Family = TEXT("physical");
			Hit.Tier = 0;
			Hit.Source = Actor.Pos;
			Hit.Delivery = TEXT("melee");
			const FSimHitResult Result = ResolveHit(State, Target, Hit, Rules);
			if (Result.Damage > 0.0 && Data.bStaffInterrupts)
			{
				Interrupt(State, Target);
			}
		}
		Actor.RecoveryUntil = State.Tick + SimTicks(Data.StaffRecoveryS);
	}
}

void StartCast(FArenaState& State, FActor& Actor, const FInputFrame& Input, FVrRuleset* Rules)
{
	const bool bPlanted = Rules && Rules->IsPlanted(Actor.Id);
	const bool bSplit = Rules && Rules->Split.bEnabled && Actor.bAbsorb && !bPlanted;
	const bool bHandsLocked = Actor.bAbsorb && !bSplit && !bPlanted;
	if (!Input.bCast || Actor.Pending.IsSet() || FireChannelBusy(Actor) || bHandsLocked || State.Tick < Actor.RollUntil || State.Tick < Actor.RecoveryUntil)
	{
		return;
	}
	const FKernelData& Data = KernelData();
	bool bNearby = false;
	if (Input.Slot == 0)
	{
		for (const FActor& Target : State.Actors)
		{
			if (Target.Team != Actor.Team && !Target.bDown && SimDistance(Actor.Pos, Target.Pos) <= Data.StaffRangeM
				&& SimInArc(Actor.Facing, SimSub(Target.Pos, Actor.Pos), Data.StaffArcDeg))
			{
				bNearby = true;
				break;
			}
		}
	}
	if (bNearby)
	{
		if (Actor.Stamina < Data.StaffStaminaCost || State.Tick < Actor.CooldownUntil)
		{
			return;
		}
		Actor.Stamina -= Data.StaffStaminaCost;
		Actor.StaminaUsedTick = State.Tick;
		FPendingCast Pending;
		Pending.Kind = TEXT("staff");
		Pending.StartTick = State.Tick;
		Pending.ReleaseTick = State.Tick + SimTicks(Data.StaffWindupS);
		Pending.Aim = Input.Aim;
		Pending.ActivationId = State.NextId++;
		Actor.Pending = Pending;
		Actor.CooldownUntil = Pending.ReleaseTick + SimTicks(Data.StaffRecoveryS);
	}
	else if (Actor.Fire.bSchool)
	{
		if (TryFireCast(State, Actor, Input, Rules))
		{
			if (Rules && Rules->bActive && Actor.MageAI.IsSet() && Rules->Pressure.FireMageDamage != 1.0 && Actor.Pending.IsSet())
			{
				Actor.Pending->DamageMult = Rules->Pressure.FireMageDamage;
			}
			ReleaseCast(State, Actor, Rules);
		}
		return;
	}
	else
	{
		if (bSplit)
		{
			const FSpell* Spell = SpellFor(Actor, Input.Slot);
			if (!Spell || Spell->Kind == TEXT("passive"))
			{
				return;
			}
			if (const TCHAR* Refusal = Rules->SplitRefusal(Actor, *Spell))
			{
				Rules->Refuse(Actor, Refusal);
				return;
			}
			FSpellCastMod Mod;
			Mod.bSuppressFlow = true;
			Mod.PowerMult = Rules->Split.OneHandPower;
			if (Actor.Team == 0 && Spell->Kind == TEXT("line") && Rules->Power.LineDamage != 1.0)
			{
				Mod.PowerMult *= Rules->Power.LineDamage;
			}
			else if (Actor.Team == 0 && (Spell->Kind == TEXT("projectile") || Spell->Line == TEXT("bolt")) && Rules->Power.BoltDamage != 1.0)
			{
				Mod.PowerMult *= Rules->Power.BoltDamage;
			}
			if (TrySpellCast(State, Actor, Input, &Mod))
			{
				Rules->SetSplitCasting(Actor.Id, true);
				ReleaseCast(State, Actor, Rules);
			}
			return;
		}
		const FSpell* PowerSpell = SpellFor(Actor, Input.Slot);
		const bool bLine = PowerSpell && PowerSpell->Kind == TEXT("line");
		const bool bBolt = PowerSpell && (PowerSpell->Kind == TEXT("projectile") || PowerSpell->Line == TEXT("bolt"));
		const double Power = (Rules && Rules->bActive && Actor.Team == 0)
			? (bLine ? Rules->Power.LineDamage : (bBolt ? Rules->Power.BoltDamage : 1.0))
			: 1.0;
		if (Power != 1.0)
		{
			FSpellCastMod Mod;
			Mod.PowerMult = Power;
			if (TrySpellCast(State, Actor, Input, &Mod))
			{
				ReleaseCast(State, Actor, Rules);
			}
			return;
		}
		if (TrySpellCast(State, Actor, Input))
		{
			ReleaseCast(State, Actor, Rules);
		}
		return;
	}
	if (bSplit && Rules)
	{
		Rules->SetSplitCasting(Actor.Id, true);
	}
	Actor.Metrics.Casts++;
	Emit(State, TEXT("cast"), Actor);
	ReleaseCast(State, Actor, Rules);
}
}

int32 SimTicks(double Seconds)
{
	return static_cast<int32>(std::ceil(Seconds * KernelData().SimStepHz - TickCeilBias));
}

double SimSeconds(int32 TickCount)
{
	return static_cast<double>(TickCount) / KernelData().SimStepHz;
}

double SimDt()
{
	return 1.0 / KernelData().SimStepHz;
}

double DrainPerSecond(double NerveRank)
{
	return KernelData().Absorb.DrainPerSecond(NerveRank);
}

double PerfectReturn(int32 Tier, double NerveRank)
{
	return KernelData().Absorb.ManaForPerfect(Tier, NerveRank);
}

FArenaState CreateArena(uint32 Seed)
{
	FArenaState State;
	State.Tick = 0;
	State.Seed = Seed;
	State.Rng = Seed;
	State.NextId = 1;
	State.Actors.Reserve(8);
	return State;
}

double ArenaRandom(FArenaState& State, const FString& Purpose)
{
	uint32 Rng = State.Rng + MulberryAdd;
	State.Rng = Rng;
	const uint32 First = (Rng ^ (Rng >> 15)) * (Rng | 1u);
	const uint32 Second = (First ^ (First >> 7)) * (First | 61u);
	const int64 Sum = static_cast<int64>(static_cast<int32>(First)) + static_cast<int64>(static_cast<int32>(Second));
	const uint32 Mixed = First ^ static_cast<uint32>(Sum);
	const uint32 Bits = Mixed ^ (Mixed >> 14);
	const double Value = static_cast<double>(Bits) / 4294967296.0;
	State.RandomLog.Add({State.Tick, Purpose, Value});
	return Value;
}

FActor& AddMage(FArenaState& State, int32 Team, const FSimVec& Pos, const FString& Label, const FSimRanks* Ranks)
{
	const FKernelData& Data = KernelData();
	const FSimRanks Used = Ranks ? *Ranks : Data.DefaultRanks;
	checkf(Used.Vigor >= RankMin && Used.Vigor <= RankMax && Used.Focus >= RankMin && Used.Focus <= RankMax && Used.Nerve >= RankMin && Used.Nerve <= RankMax,
		TEXT("Arena ranks must be integers from 1 to 5"));
	FActor Actor;
	Actor.Id = State.NextId++;
	Actor.Team = Team;
	Actor.Label = Label;
	Actor.Pos = Pos;
	Actor.PreviousPos = Pos;
	Actor.Radius = Data.MageRadiusM;
	Actor.Ranks = Used;
	Actor.MaxHp = Maximum(Data.HpBase, Data.HpPerRank, Used.Vigor);
	Actor.Hp = Actor.MaxHp;
	Actor.MaxMana = Maximum(Data.ManaBase, Data.ManaPerRank, Used.Focus);
	Actor.Mana = Actor.MaxMana;
	Actor.MaxStamina = Maximum(Data.StaminaBase, Data.StaminaPerRank, Used.Vigor);
	Actor.Stamina = Actor.MaxStamina;
	Actor.Water = NewWaterState();
	Actor.LastInput = SimIdleInput();
	Actor.WaveStartTick = State.Tick;
	Actor.Tier = 1;
	Actor.LastUnlockTick = State.Tick;
	Actor.UnlockTicks.Add(State.Tick);
	State.Actors.Add(MoveTemp(Actor));
	return State.Actors.Last();
}

void Emit(FArenaState& State, const TCHAR* Kind, const FActor& Actor, double Value, const TOptional<int32>& TargetId)
{
	FArenaEvent Event;
	Event.Tick = State.Tick;
	Event.Kind = Kind;
	Event.ActorId = Actor.Id;
	Event.Value = Value;
	Event.TargetId = TargetId;
	State.Events.Add(Event);
}

void Interrupt(FArenaState& State, FActor& Actor)
{
	bool bOwned = false;
	for (const FTelegraph& Telegraph : State.Telegraphs)
	{
		if (Telegraph.OwnerId == Actor.Id)
		{
			bOwned = true;
			break;
		}
	}
	const bool bHadCast = Actor.Pending.IsSet() || bOwned;
	Actor.Pending.Reset();
	CancelFireChannel(Actor);
	State.Telegraphs.RemoveAll([&Actor](const FTelegraph& Telegraph) { return Telegraph.OwnerId == Actor.Id && !Telegraph.bCommitted; });
	if (bHadCast)
	{
		Emit(State, TEXT("interrupt"), Actor);
	}
}

void UpdateClock(FArenaState& State, FActor& Actor)
{
	const int32 Next = Actor.Tier + 1;
	if (Next > TierCap)
	{
		return;
	}
	const FKernelData& Data = KernelData();
	if (State.Tick - Actor.WaveStartTick + Actor.ClockAdvanceTicks >= SimTicks(Data.UnlockAtSeconds[Next])
		&& State.Tick - Actor.LastUnlockTick >= SimTicks(Data.MinimumSecondsBetweenUnlocks))
	{
		Actor.Tier = Next;
		Actor.LastUnlockTick = State.Tick;
		Actor.UnlockTicks.Add(State.Tick);
		Emit(State, TEXT("unlock"), Actor, Next);
	}
}

void StepArena(FArenaState& State, const TMap<int32, FInputFrame>& Inputs, FVrRuleset* Rules)
{
	const FKernelData& Data = KernelData();
	State.Tick++;
	if (Rules)
	{
		Rules->ExpireWalls(State);
	}
	for (FActor& Actor : State.Actors)
	{
		if (Actor.bDown)
		{
			continue;
		}
		UpdateWater(State, Actor);
		UpdateFireResource(State, Actor);
		double ManaRegen = Maximum(Data.ManaRegenBase, Data.ManaRegenPerRank, Actor.Ranks.Focus) * SimDt();
		if (Rules && Rules->bActive && Actor.Team == 0 && Rules->Power.ManaRegen != 1.0)
		{
			ManaRegen *= Rules->Power.ManaRegen;
		}
		Actor.Mana = std::min(Actor.MaxMana, Actor.Mana + ManaRegen);
		if (State.Tick - Actor.StaminaUsedTick >= SimTicks(Data.StaminaRegenDelayS))
		{
			double Regen = Data.StaminaRegenPerSecond * SimDt();
			if (Actor.Fire.bSchool)
			{
				Regen *= FireStaminaRegenMult(Actor);
			}
			Actor.Stamina = std::min(Actor.MaxStamina, Actor.Stamina + Regen);
		}
		if (State.Tick < Actor.Water.EncasedUntil)
		{
			Actor.PreviousPos = Actor.Pos;
			UpdateFireOngoing(State, Actor);
			continue;
		}
		const FInputFrame* Found = Inputs.Find(Actor.Id);
		const FInputFrame Input = Found ? *Found : SimIdleInput(FSimVec{Actor.Pos.X + Actor.Facing.X, Actor.Pos.Y + Actor.Facing.Y});
		Actor.Facing = SimUnit(SimSub(Input.Aim, Actor.Pos), Actor.Facing);
		UpdateWard(State, Actor, Input, Rules);
		if (Rules && !Actor.bAbsorb)
		{
			Rules->SetSplitCasting(Actor.Id, false);
		}
		if (Rules)
		{
			Rules->TickSplitLatch(Actor);
		}
		if (Rules)
		{
			Rules->TickStaff(State, Actor, Input);
			Rules->TickWalls(State, Actor, Input);
		}
		MoveActor(State, Actor, Input, Rules);
		ReleaseCast(State, Actor, Rules);
		StartCast(State, Actor, Input, Rules);
		Actor.LastInput = Input;
		UpdateFireOngoing(State, Actor);
	}
	if (Rules)
	{
		Rules->BurnCrossers(State);
	}
	UpdateTelegraphs(State, Rules);
	UpdateProjectiles(State, Rules);
	State.Zones.RemoveAll([&State](const FZone& Zone) { return !(Zone.Until > State.Tick); });
	for (FActor& Actor : State.Actors)
	{
		if (!Actor.bDown)
		{
			UpdateClock(State, Actor);
		}
	}
}

void StepArena(FArenaState& State, const TMap<int32, FInputFrame>& Inputs)
{
	StepArena(State, Inputs, nullptr);
}

void StepArena(FArenaState& State)
{
	StepArena(State, TMap<int32, FInputFrame>(), nullptr);
}

void ResetWave(FArenaState& State, FActor& Actor)
{
	const FKernelData& Data = KernelData();
	Actor.Hp += (Actor.MaxHp - Actor.Hp) * Data.HealFraction;
	Actor.Mana = Actor.MaxMana * Data.ManaRefill;
	Actor.Stamina = Actor.MaxStamina * Data.StaminaRefill;
	Actor.Tier = 1;
	Actor.WaveStartTick = State.Tick;
	Actor.LastUnlockTick = State.Tick;
	Actor.ClockAdvanceTicks = 0;
	Actor.UnlockTicks.Reset();
	Actor.UnlockTicks.Add(State.Tick);
	Actor.bAbsorb = false;
	Actor.bAbsorbExhausted = false;
	Actor.AbsorbFreshTick = NeverTick;
	Actor.ReleaseTick = NeverTick;
	Actor.Pending.Reset();
	Actor.CooldownUntil = State.Tick;
	Actor.RollUntil = State.Tick;
	Actor.ImmuneUntil = State.Tick;
	Actor.RecoveryUntil = State.Tick;
	Actor.LastInput = SimIdleInput();
	State.Projectiles.Reset();
	State.Telegraphs.Reset();
	State.Zones.Reset();
	ResetWater(Actor);
	ResetFire(Actor);
}

double FFixedStepper::Advance(double ElapsedS, const TFunctionRef<void()>& Update)
{
	Accumulated += std::max(0.0, std::min(ElapsedS, KernelData().MaxFrameDeltaS));
	while (Accumulated + StepperEpsilon >= SimDt())
	{
		Update();
		Accumulated -= SimDt();
	}
	return std::max(0.0, Accumulated / SimDt());
}
