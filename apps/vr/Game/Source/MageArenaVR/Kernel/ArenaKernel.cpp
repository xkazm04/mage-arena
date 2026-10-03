#include "Kernel/ArenaKernel.h"

#include "Kernel/Catalog.h"
#include "Kernel/Enemies.h"
#include "Kernel/Fire.h"
#include "Kernel/Geometry.h"
#include "Kernel/Water.h"

#include <cmath>
#include <limits>

namespace
{
double Maximum(double Base, double PerRank, double Rank)
{
	return Base + PerRank * Rank;
}

bool HitListContains(const TArray<int32>& Ids, int32 Id)
{
	return Ids.Contains(Id);
}

void MixByte(uint32& Hash, uint8 Byte)
{
	Hash ^= Byte;
	Hash *= FnvPrime;
}

void MixString(uint32& Hash, const FString& Text)
{
	MixByte(Hash, static_cast<uint8>(Text.Len() & 0xff));
	for (TCHAR Char : Text)
	{
		const uint32 Code = static_cast<uint32>(Char);
		MixByte(Hash, static_cast<uint8>(Code));
		MixByte(Hash, static_cast<uint8>(Code >> 8));
	}
}

void MixInt(uint32& Hash, int32 Value)
{
	const uint32 Bits = static_cast<uint32>(Value);
	MixByte(Hash, static_cast<uint8>(Bits));
	MixByte(Hash, static_cast<uint8>(Bits >> 8));
	MixByte(Hash, static_cast<uint8>(Bits >> 16));
	MixByte(Hash, static_cast<uint8>(Bits >> 24));
}

void MixUInt(uint32& Hash, uint32 Value)
{
	MixInt(Hash, static_cast<int32>(Value));
}

void MixDouble(uint32& Hash, double Value)
{
	uint64 Bits = 0;
	static_assert(sizeof(Bits) == sizeof(Value), "double width");
	FMemory::Memcpy(&Bits, &Value, sizeof(Bits));
	for (int32 Shift = 0; Shift < 64; Shift += 8)
	{
		MixByte(Hash, static_cast<uint8>(Bits >> Shift));
	}
}

void MixBool(uint32& Hash, bool Value)
{
	MixByte(Hash, Value ? 1 : 0);
}

void MixVec(uint32& Hash, const FSimVec& Value)
{
	MixDouble(Hash, Value.X);
	MixDouble(Hash, Value.Y);
}

EAbsorbHitKind KindFromFamily(const FString& Family)
{
	if (Family == TEXT("physical"))
	{
		return EAbsorbHitKind::Physical;
	}
	if (Family == TEXT("unblockable"))
	{
		return EAbsorbHitKind::Unblockable;
	}
	return EAbsorbHitKind::Magic;
}

void UpdateWard(FArenaState& State, FActor& Actor, const FInputFrame& Input)
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

void MoveActor(FArenaState& State, FActor& Actor, const FInputFrame& Input)
{
	const FKernelData& Data = KernelData();
	Actor.PreviousPos = Actor.Pos;
	const bool bRooted = State.Tick < Actor.Water.RootUntil
		|| (Actor.Pending.IsSet() && Actor.Pending->SpellId.IsSet() && Actor.Pending->SpellId.GetValue() == TEXT("mend:4:base"))
		|| FireCastRoots(Actor);
	if (!bRooted && Input.bRoll && !Actor.LastInput.bRoll && State.Tick >= Actor.RollUntil && State.Tick >= Actor.RecoveryUntil
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

void ReleaseCast(FArenaState& State, FActor& Actor)
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
		ReleaseFire(State, Actor);
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
			const FSimHitResult Result = ResolveHit(State, Target, Hit);
			if (Result.Damage > 0.0 && Data.bStaffInterrupts)
			{
				Interrupt(State, Target);
			}
		}
		Actor.RecoveryUntil = State.Tick + SimTicks(Data.StaffRecoveryS);
	}
}

void StartCast(FArenaState& State, FActor& Actor, const FInputFrame& Input)
{
	if (!Input.bCast || Actor.Pending.IsSet() || FireChannelBusy(Actor) || Actor.bAbsorb || State.Tick < Actor.RollUntil || State.Tick < Actor.RecoveryUntil)
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
		if (TryFireCast(State, Actor, Input))
		{
			ReleaseCast(State, Actor);
		}
		return;
	}
	else
	{
		if (TrySpellCast(State, Actor, Input))
		{
			ReleaseCast(State, Actor);
		}
		return;
	}
	Actor.Metrics.Casts++;
	Emit(State, TEXT("cast"), Actor);
	ReleaseCast(State, Actor);
}

void UpdateTelegraphs(FArenaState& State)
{
	TArray<FTelegraph> Due;
	TArray<FTelegraph> Keep;
	for (const FTelegraph& Telegraph : State.Telegraphs)
	{
		if (Telegraph.ResolveTick <= State.Tick)
		{
			Due.Add(Telegraph);
		}
		else
		{
			Keep.Add(Telegraph);
		}
	}
	State.Telegraphs = MoveTemp(Keep);
	for (const FTelegraph& Telegraph : Due)
	{
		FActor* Owner = SimFindActor(State, Telegraph.OwnerId);
		if (!Owner || (Owner->bDown && !Telegraph.bSurvivesOwner) || (!Telegraph.bSurvivesOwner && State.Tick < Owner->Water.EncasedUntil))
		{
			continue;
		}
		const FSimVec Direction = SimUnit(SimSub(Telegraph.Target, Telegraph.Origin));
		if (Telegraph.Kind == TEXT("projectile"))
		{
			FHit Hit;
			Hit.OwnerId = Telegraph.OwnerId;
			Hit.ActivationId = Telegraph.ActivationId;
			Hit.Damage = Telegraph.Damage;
			Hit.Family = Telegraph.Family;
			Hit.Tier = Telegraph.Tier;
			Hit.Source = Telegraph.Source;
			Hit.bBolt = Telegraph.bBolt;
			Hit.HeatOnHit = Telegraph.HeatOnHit;
			Hit.bSuppressPerfect = Telegraph.bSuppressPerfect;
			Hit.bPierceShields = Telegraph.bPierceShields;
			SpawnProjectile(State, Hit, Telegraph.Origin, Direction, Telegraph.SpeedMps, Telegraph.RangeM);
		}
		else
		{
			const FSimVec End = SimAdd(Telegraph.Origin, SimScale(Direction, Telegraph.RangeM));
			for (FActor& Target : State.Actors)
			{
				if (Target.Team == Owner->Team || Target.bDown)
				{
					continue;
				}
				bool bInShape = false;
				if (Telegraph.Kind == TEXT("area"))
				{
					bInShape = SimDistance(Telegraph.Target, Target.Pos) <= Telegraph.WidthM + Target.Radius;
				}
				else if (Telegraph.Kind == TEXT("melee"))
				{
					bInShape = SimDistance(Telegraph.Origin, Target.Pos) <= Telegraph.RangeM + Target.Radius
						&& SimInArc(Direction, SimSub(Target.Pos, Telegraph.Origin), Telegraph.WidthM);
				}
				else
				{
					bInShape = SimSegmentHit(Telegraph.Origin, End, Target.Pos, Telegraph.WidthM * 0.5 + Target.Radius).IsSet();
				}
				if (!bInShape)
				{
					continue;
				}
				const bool bImmune = State.Tick < Target.ImmuneUntil || State.Tick < Target.Water.EncasedUntil;
				FHit Hit;
				Hit.OwnerId = Telegraph.OwnerId;
				Hit.ActivationId = Telegraph.ActivationId;
				Hit.Damage = Telegraph.Damage;
				Hit.Family = Telegraph.Family;
				Hit.Tier = Telegraph.Tier;
				Hit.Source = Telegraph.Kind == TEXT("area") ? Telegraph.Target : Telegraph.Source;
				Hit.bBolt = Telegraph.bBolt;
				Hit.HeatOnHit = Telegraph.HeatOnHit;
				Hit.bSuppressPerfect = Telegraph.bSuppressPerfect;
				Hit.bPierceShields = Telegraph.bPierceShields;
				Hit.Delivery = Telegraph.Kind == TEXT("melee") ? TEXT("melee") : TEXT("area");
				const FSimHitResult Result = ResolveHit(State, Target, Hit);
				if (!bImmune && !Result.bPerfect && !Target.bDown)
				{
					if (Telegraph.RootS > 0.0)
					{
						Target.Water.RootUntil = std::max(Target.Water.RootUntil, State.Tick + SimTicks(Telegraph.RootS));
					}
					if (Telegraph.PullM > 0.0)
					{
						const FSimVec PullDirection = SimUnit(SimSub(Owner->Pos, Target.Pos));
						const double Amount = std::min(Telegraph.PullM, std::max(0.0, SimDistance(Owner->Pos, Target.Pos) - Owner->Radius - Target.Radius));
						Target.Pos = ConstrainToArena(SimAdd(Target.Pos, SimScale(PullDirection, Amount)), Target.Radius);
					}
				}
			}
			if (Telegraph.Kind == TEXT("charge"))
			{
				const FSimVec Constrained = ConstrainToArena(End, Owner->Radius);
				Owner->Pos = Constrained;
				if ((Constrained.X != End.X || Constrained.Y != End.Y) && Owner->Enemy.IsSet())
				{
					Owner->Enemy->StunnedUntil = State.Tick + SimTicks(Telegraph.WallStunS);
				}
			}
		}
	}
}

void UpdateProjectiles(FArenaState& State)
{
	const int32 Count = State.Projectiles.Num();
	State.Projectiles.Reserve(Count * 2 + 8);
	for (int32 Index = 0; Index < Count; ++Index)
	{
		FProjectile& Projectile = State.Projectiles[Index];
		const double Travel = std::min(SimLength(Projectile.Velocity) * SimDt(), Projectile.RemainingM);
		const FSimVec Direction = SimUnit(Projectile.Velocity);
		Projectile.PreviousPos = Projectile.Pos;
		const FSimVec End = SimAdd(Projectile.Pos, SimScale(Direction, Travel));
		const FActor* Owner = SimFindActor(State, Projectile.OwnerId);
		FActor* First = nullptr;
		double FirstT = std::numeric_limits<double>::infinity();
		for (FActor& Actor : State.Actors)
		{
			if (Actor.bDown || (Owner && Actor.Team == Owner->Team) || Actor.Id == Projectile.OwnerId || HitListContains(Projectile.HitIds, Actor.Id))
			{
				continue;
			}
			const TOptional<double> T = SimSegmentHit(Projectile.Pos, End, Actor.Pos, Actor.Radius + Projectile.Radius);
			if (T.IsSet() && T.GetValue() < FirstT)
			{
				FirstT = T.GetValue();
				First = &Actor;
			}
		}
		if (First)
		{
			Projectile.Source = Projectile.Pos;
			FHit Hit;
			Hit.OwnerId = Projectile.OwnerId;
			Hit.ActivationId = Projectile.ActivationId;
			Hit.Damage = Projectile.Damage;
			Hit.Family = Projectile.Family;
			Hit.Tier = Projectile.Tier;
			Hit.Source = Projectile.Source;
			Hit.bBolt = Projectile.bBolt;
			Hit.bReaction = Projectile.bReaction;
			Hit.HeatOnHit = Projectile.HeatOnHit;
			Hit.bSuppressPerfect = Projectile.bSuppressPerfect;
			Hit.bPierceShields = Projectile.bPierceShields;
			Hit.Delivery = (Projectile.BurstRadiusM > 0.0 || Projectile.bPiercing) ? TEXT("area") : TEXT("projectile");
			const FSimHitResult Result = ResolveHit(State, *First, Hit);
			Projectile.HitIds.Add(First->Id);
			if (Result.bPerfect)
			{
				ReflectProjectile(State, *First, Projectile);
			}
			if (Projectile.BurstRadiusM > 0.0 && !Result.bPerfect)
			{
				for (FActor& Target : State.Actors)
				{
					if (!Target.bDown && (!Owner || Target.Team != Owner->Team) && !HitListContains(Projectile.HitIds, Target.Id)
						&& SimDistance(Target.Pos, First->Pos) <= Projectile.BurstRadiusM)
					{
						FHit Burst = Hit;
						Burst.Source = First->Pos;
						Burst.Delivery = TEXT("area");
						ResolveHit(State, Target, Burst);
						Projectile.HitIds.Add(Target.Id);
					}
				}
			}
			if (!Projectile.bPiercing || Result.bPerfect)
			{
				Projectile.RemainingM = 0.0;
			}
			else
			{
				Projectile.Pos = End;
				Projectile.RemainingM -= Travel;
			}
		}
		else
		{
			Projectile.Pos = End;
			Projectile.RemainingM -= Travel;
		}
	}
	State.Projectiles.RemoveAll([](const FProjectile& Projectile) { return !(Projectile.RemainingM > ProjectileRemainEpsilon); });
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

FSimHitResult ResolveHit(FArenaState& State, FActor& Target, const FHit& Hit)
{
	FSimHitResult Result;
	if (Target.bDown || State.Tick < Target.ImmuneUntil || State.Tick < Target.Water.EncasedUntil)
	{
		return Result;
	}
	const FKernelData& Data = KernelData();
	const FEnemySpec* Spec = Target.Enemy.IsSet() ? EnemySpecFor(Target) : nullptr;
	const bool bShielded = Spec && Spec->FrontBlockDeg.IsSet() && Spec->FrontBlockDeg.GetValue() != 0.0
		&& Hit.Delivery != TEXT("area") && Hit.Family != TEXT("unblockable") && !Hit.bPierceShields
		&& SimInArc(Target.Facing, SimSub(Hit.Source, Target.Pos), Spec->FrontBlockDeg.GetValue());
	const double Armour = Hit.bBolt ? (Spec && Spec->ArmourMultVsBolt.IsSet() ? Spec->ArmourMultVsBolt.GetValue() : 1.0) : 1.0;
	const double Block = bShielded ? (Spec->FrontBlockMult.IsSet() ? Spec->FrontBlockMult.GetValue() : 1.0) : 1.0;
	const double Incoming = Hit.Damage * Armour * Block;
	const bool bGuarded = Target.bAbsorb && SimInArc(Target.Facing, SimSub(Hit.Source, Target.Pos), Data.Absorb.ArcDeg);
	const bool bFreshWindow = !Hit.bSuppressPerfect && State.Tick - Target.AbsorbFreshTick <= SimTicks(Data.Absorb.WindowS);
	const FAbsorbVerdict Verdict = EvaluateAbsorb(Data.Absorb, bGuarded, bFreshWindow, KindFromFamily(Hit.Family), Hit.Tier, static_cast<double>(Target.Ranks.Nerve));
	Result.Damage = Incoming * (1.0 - Verdict.Reduction);
	Result.bPerfect = Verdict.bPerfect;
	if (Verdict.bPerfect)
	{
		Target.Metrics.Perfects++;
		const double Refund = std::min(Target.MaxMana - Target.Mana, Verdict.ManaReturned * RefundMult(State, Target));
		Target.Mana += Refund;
		Target.Metrics.ManaReturned += Refund;
		Target.ClockAdvanceTicks += SimTicks(Verdict.TierClockAdvanceS);
		Emit(State, TEXT("perfect"), Target, Refund, Hit.OwnerId);
	}
	else if (Verdict.Reduction > 0.0)
	{
		Target.Metrics.Blocks++;
	}
	if (Verdict.Reduction > 0.0)
	{
		WaterAbsorbed(State, Target, Hit, Incoming - Result.Damage, Verdict.bPerfect);
	}
	const double Removed = std::min(Target.Hp, Result.Damage);
	Target.Hp = std::max(0.0, Target.Hp - Result.Damage);
	Target.Metrics.DamageTaken += Removed;
	if (FActor* Owner = SimFindActor(State, Hit.OwnerId))
	{
		Owner->Metrics.DamageDealt += Removed;
	}
	FireOnHitResolved(State, Target, Hit, Incoming, Verdict.Reduction);
	Target.Metrics.Hits++;
	Emit(State, TEXT("hit"), Target, Removed, Hit.OwnerId);
	if (Target.Hp == 0.0)
	{
		Target.bDown = true;
		Target.bAbsorb = false;
		Interrupt(State, Target);
		Emit(State, TEXT("down"), Target);
	}
	return Result;
}

FProjectile& SpawnProjectile(FArenaState& State, const FHit& Hit, const FSimVec& Origin, const FSimVec& Direction, double SpeedMps, double RangeM, TOptional<double> Radius)
{
	const FSimVec UnitDirection = SimUnit(Direction);
	FProjectile Projectile;
	Projectile.OwnerId = Hit.OwnerId;
	Projectile.ActivationId = Hit.ActivationId;
	Projectile.Damage = Hit.Damage;
	Projectile.Family = Hit.Family;
	Projectile.Tier = Hit.Tier;
	Projectile.Source = Origin;
	Projectile.bBolt = Hit.bBolt;
	Projectile.bReaction = Hit.bReaction;
	Projectile.HeatOnHit = Hit.HeatOnHit;
	Projectile.bSuppressPerfect = Hit.bSuppressPerfect;
	Projectile.bPierceShields = Hit.bPierceShields;
	Projectile.Delivery = TEXT("projectile");
	Projectile.Id = State.NextId++;
	Projectile.Pos = Origin;
	Projectile.PreviousPos = Origin;
	Projectile.Velocity = SimScale(UnitDirection, SpeedMps);
	Projectile.Radius = Radius.Get(KernelData().ProjectileRadiusM);
	Projectile.RemainingM = RangeM;
	State.Projectiles.Add(MoveTemp(Projectile));
	return State.Projectiles.Last();
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

void StepArena(FArenaState& State, const TMap<int32, FInputFrame>& Inputs)
{
	const FKernelData& Data = KernelData();
	State.Tick++;
	for (FActor& Actor : State.Actors)
	{
		if (Actor.bDown)
		{
			continue;
		}
		UpdateWater(State, Actor);
		UpdateFireResource(State, Actor);
		Actor.Mana = std::min(Actor.MaxMana, Actor.Mana + Maximum(Data.ManaRegenBase, Data.ManaRegenPerRank, Actor.Ranks.Focus) * SimDt());
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
		UpdateWard(State, Actor, Input);
		MoveActor(State, Actor, Input);
		ReleaseCast(State, Actor);
		StartCast(State, Actor, Input);
		Actor.LastInput = Input;
		UpdateFireOngoing(State, Actor);
	}
	UpdateTelegraphs(State);
	UpdateProjectiles(State);
	State.Zones.RemoveAll([&State](const FZone& Zone) { return !(Zone.Until > State.Tick); });
	for (FActor& Actor : State.Actors)
	{
		if (!Actor.bDown)
		{
			UpdateClock(State, Actor);
		}
	}
}

void StepArena(FArenaState& State)
{
	StepArena(State, TMap<int32, FInputFrame>());
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

FString StateHash(const FArenaState& State)
{
	uint32 Hash = FnvOffset;
	MixInt(Hash, State.Tick);
	MixUInt(Hash, State.Seed);
	MixUInt(Hash, State.Rng);
	MixInt(Hash, State.NextId);
	for (const FActor& Actor : State.Actors)
	{
		MixInt(Hash, Actor.Id);
		MixInt(Hash, Actor.Team);
		MixString(Hash, Actor.Label);
		MixVec(Hash, Actor.Pos);
		MixVec(Hash, Actor.PreviousPos);
		MixVec(Hash, Actor.Facing);
		MixDouble(Hash, Actor.Hp);
		MixDouble(Hash, Actor.Mana);
		MixDouble(Hash, Actor.Stamina);
		MixBool(Hash, Actor.bDown);
		MixBool(Hash, Actor.bAbsorb);
		MixInt(Hash, Actor.Tier);
		MixInt(Hash, Actor.ClockAdvanceTicks);
		MixInt(Hash, Actor.Water.Flow);
		MixDouble(Hash, Actor.Water.Stored);
		MixBool(Hash, Actor.Fire.bSchool);
		MixDouble(Hash, Actor.Fire.Heat);
		MixInt(Hash, Actor.Fire.LockUntil);
		MixInt(Hash, Actor.Fire.SunfallTick);
		MixInt(Hash, Actor.Pending.IsSet() ? 1 : 0);
	}
	for (const FProjectile& Projectile : State.Projectiles)
	{
		MixInt(Hash, Projectile.Id);
		MixVec(Hash, Projectile.Pos);
		MixVec(Hash, Projectile.Velocity);
		MixDouble(Hash, Projectile.RemainingM);
	}
	for (const FTelegraph& Telegraph : State.Telegraphs)
	{
		MixInt(Hash, Telegraph.Id);
		MixInt(Hash, Telegraph.ResolveTick);
	}
	for (const FZone& Zone : State.Zones)
	{
		MixInt(Hash, Zone.Id);
		MixInt(Hash, Zone.Until);
	}
	for (const FArenaEvent& Event : State.Events)
	{
		MixInt(Hash, Event.Tick);
		MixString(Hash, Event.Kind);
		MixInt(Hash, Event.ActorId);
		MixDouble(Hash, Event.Value);
	}
	for (const FRandomDraw& Draw : State.RandomLog)
	{
		MixInt(Hash, Draw.Tick);
		MixString(Hash, Draw.Purpose);
		MixDouble(Hash, Draw.Value);
	}
	return FString::Printf(TEXT("%08x"), Hash);
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
