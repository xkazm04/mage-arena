#include "Kernel/ArenaKernel.h"

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

void MixOptInt(uint32& Hash, const TOptional<int32>& Value)
{
	MixBool(Hash, Value.IsSet());
	if (Value.IsSet())
	{
		MixInt(Hash, Value.GetValue());
	}
}

void MixOptDouble(uint32& Hash, const TOptional<double>& Value)
{
	MixBool(Hash, Value.IsSet());
	if (Value.IsSet())
	{
		MixDouble(Hash, Value.GetValue());
	}
}

void MixOptString(uint32& Hash, const TOptional<FString>& Value)
{
	MixBool(Hash, Value.IsSet());
	if (Value.IsSet())
	{
		MixString(Hash, Value.GetValue());
	}
}

void MixInts(uint32& Hash, const TArray<int32>& Values)
{
	MixInt(Hash, Values.Num());
	for (const int32 Value : Values)
	{
		MixInt(Hash, Value);
	}
}

void MixInput(uint32& Hash, const FInputFrame& Input)
{
	MixVec(Hash, Input.Move);
	MixVec(Hash, Input.Aim);
	MixInt(Hash, Input.Slot);
	MixBool(Hash, Input.bCast);
	MixBool(Hash, Input.bAbsorb);
	MixBool(Hash, Input.bRoll);
	MixBool(Hash, Input.bSprint);
	MixBool(Hash, Input.bPlantStaff);
	MixBool(Hash, Input.bLiftStaff);
	MixBool(Hash, Input.bRaiseFireWall);
}

void MixWater(uint32& Hash, const FWaterState& Water)
{
	MixString(Hash, Water.Composition.Name);
	MixInt(Hash, Water.Composition.Lines.Num());
	for (const FString& Line : Water.Composition.Lines)
	{
		MixString(Hash, Line);
	}
	MixString(Hash, Water.Composition.Branches.Lash);
	MixString(Hash, Water.Composition.Branches.Mirror);
	MixString(Hash, Water.Composition.Branches.TideOrb);
	MixInt(Hash, Water.Flow);
	MixString(Hash, Water.LastLine);
	MixInt(Hash, Water.LastCastTick);
	MixInt(Hash, Water.LastActivityTick);
	MixInt(Hash, Water.Cooldowns.Num());
	for (const FCooldownEntry& Entry : Water.Cooldowns)
	{
		MixString(Hash, Entry.Line);
		MixInt(Hash, Entry.Until);
	}
	MixDouble(Hash, Water.Stored);
	MixInt(Hash, Water.RootUntil);
	MixInt(Hash, Water.EncasedUntil);
	MixInt(Hash, Water.SlowUntil);
	MixDouble(Hash, Water.SlowMult);
	MixInt(Hash, Water.WardUntil);
	MixInt(Hash, Water.SheenUntil);
	MixInt(Hash, Water.HotUntil);
	MixDouble(Hash, Water.HotPerTick);
	MixInt(Hash, Water.Crests);
	MixDouble(Hash, Water.Healing);
	MixInt(Hash, Water.ControlTicks);
	MixBool(Hash, Water.Decoy.IsSet());
	if (Water.Decoy.IsSet())
	{
		MixVec(Hash, Water.Decoy->Pos);
		MixInt(Hash, Water.Decoy->Until);
	}
}

void MixFire(uint32& Hash, const FFireState& Fire)
{
	MixBool(Hash, Fire.bSchool);
	MixDouble(Hash, Fire.Heat);
	MixDouble(Hash, Fire.MaxHeat);
	MixInt(Hash, Fire.LastGainTick);
	MixInt(Hash, Fire.LockUntil);
	MixDouble(Hash, Fire.LockValue);
	MixDouble(Hash, Fire.AbsorbDrainSpellMult);
	MixInt(Hash, Fire.AbsorbDrainSpellUntil);
	MixInt(Hash, Fire.SunfallCastTick);
	MixInt(Hash, Fire.SunfallTick);
	MixBool(Hash, Fire.bSunfallLoosed);
	MixInt(Hash, Fire.Cooldowns.Num());
	for (const FFireCooldown& Entry : Fire.Cooldowns)
	{
		MixString(Hash, Entry.SpellId);
		MixInt(Hash, Entry.Until);
	}
	MixInt(Hash, Fire.Trails.Num());
	for (const FFireTrail& Trail : Fire.Trails)
	{
		MixVec(Hash, Trail.From);
		MixVec(Hash, Trail.To);
		MixInt(Hash, Trail.Until);
		MixInt(Hash, Trail.NextTick);
		MixInt(Hash, Trail.IntervalTicks);
		MixDouble(Hash, Trail.Damage);
		MixDouble(Hash, Trail.HeatOnHit);
		MixInt(Hash, Trail.Tier);
		MixString(Hash, Trail.Family);
		MixString(Hash, Trail.SpellId);
	}
	MixBool(Hash, Fire.Channel.IsSet());
	if (Fire.Channel.IsSet())
	{
		const FFireChannel& Channel = Fire.Channel.GetValue();
		MixString(Hash, Channel.SpellId);
		MixInt(Hash, Channel.Until);
		MixInt(Hash, Channel.NextTick);
		MixInt(Hash, Channel.IntervalTicks);
		MixInt(Hash, Channel.TicksDone);
		MixDouble(Hash, Channel.Damage);
		MixDouble(Hash, Channel.HeatOnHit);
		MixDouble(Hash, Channel.RangeM);
		MixInt(Hash, Channel.Tier);
		MixString(Hash, Channel.Family);
		MixBool(Hash, Channel.bPierceShields);
		MixBool(Hash, Channel.bPerfectOnlyFirstTick);
	}
}

void MixEnemy(uint32& Hash, const TOptional<FEnemyBrain>& Enemy)
{
	MixBool(Hash, Enemy.IsSet());
	if (!Enemy.IsSet())
	{
		return;
	}
	MixString(Hash, Enemy->Id);
	MixInt(Hash, Enemy->ReadyTick);
	MixInt(Hash, Enemy->BackoffUntil);
	MixInt(Hash, Enemy->StunnedUntil);
	MixInt(Hash, Enemy->AttackIndex);
	MixBool(Hash, Enemy->bDeathQueued);
}

void MixMage(uint32& Hash, const TOptional<FMageBrain>& Mage)
{
	MixBool(Hash, Mage.IsSet());
	if (!Mage.IsSet())
	{
		return;
	}
	MixDouble(Hash, Mage->Competence);
	MixInt(Hash, Mage->NextDecisionTick);
	MixInput(Hash, Mage->Input);
	MixInt(Hash, Mage->Observed.Num());
	for (const FMageObservation& Row : Mage->Observed)
	{
		MixInt(Hash, Row.Id);
		MixInt(Hash, Row.FirstSeenTick);
		MixBool(Hash, Row.bReacted);
	}
	MixInt(Hash, Mage->DefendUntil);
	MixInt(Hash, Mage->PlannedRaiseTick);
	MixInt(Hash, Mage->PlannedReleaseTick);
	MixVec(Hash, Mage->TargetPoint);
	MixInts(Hash, Mage->DecisionTicks);
	MixInts(Hash, Mage->ReactionAges);
	MixInt(Hash, Mage->WallSeenTick);
	MixInt(Hash, Mage->WallLastSeenTick);
}

void MixPending(uint32& Hash, const TOptional<FPendingCast>& Pending)
{
	MixBool(Hash, Pending.IsSet());
	if (!Pending.IsSet())
	{
		return;
	}
	MixString(Hash, Pending->Kind);
	MixInt(Hash, Pending->ReleaseTick);
	MixInt(Hash, Pending->StartTick);
	MixVec(Hash, Pending->Aim);
	MixInt(Hash, Pending->ActivationId);
	MixOptString(Hash, Pending->SpellId);
	MixOptDouble(Hash, Pending->DamageMult);
	MixOptInt(Hash, Pending->TargetId);
}

void MixActor(uint32& Hash, const FActor& Actor)
{
	MixInt(Hash, Actor.Id);
	MixInt(Hash, Actor.Team);
	MixString(Hash, Actor.Label);
	MixVec(Hash, Actor.Pos);
	MixVec(Hash, Actor.PreviousPos);
	MixVec(Hash, Actor.Facing);
	MixDouble(Hash, Actor.Radius);
	MixInt(Hash, Actor.Ranks.Vigor);
	MixInt(Hash, Actor.Ranks.Focus);
	MixInt(Hash, Actor.Ranks.Nerve);
	MixDouble(Hash, Actor.Hp);
	MixDouble(Hash, Actor.MaxHp);
	MixDouble(Hash, Actor.Mana);
	MixDouble(Hash, Actor.MaxMana);
	MixDouble(Hash, Actor.Stamina);
	MixDouble(Hash, Actor.MaxStamina);
	MixBool(Hash, Actor.bDown);
	MixBool(Hash, Actor.bDummy);
	MixBool(Hash, Actor.bAbsorb);
	MixInt(Hash, Actor.AbsorbFreshTick);
	MixInt(Hash, Actor.ReleaseTick);
	MixBool(Hash, Actor.bAbsorbExhausted);
	MixWater(Hash, Actor.Water);
	MixFire(Hash, Actor.Fire);
	MixEnemy(Hash, Actor.Enemy);
	MixMage(Hash, Actor.MageAI);
	MixOptDouble(Hash, Actor.SpeedMps);
	MixInput(Hash, Actor.LastInput);
	MixInt(Hash, Actor.RollUntil);
	MixInt(Hash, Actor.ImmuneUntil);
	MixInt(Hash, Actor.RecoveryUntil);
	MixVec(Hash, Actor.RollDirection);
	MixInt(Hash, Actor.StaminaUsedTick);
	MixInt(Hash, Actor.CooldownUntil);
	MixPending(Hash, Actor.Pending);
	MixInt(Hash, Actor.WaveStartTick);
	MixInt(Hash, Actor.Tier);
	MixInt(Hash, Actor.LastUnlockTick);
	MixInt(Hash, Actor.ClockAdvanceTicks);
	MixInts(Hash, Actor.UnlockTicks);
	MixInt(Hash, Actor.Metrics.Perfects);
	MixInt(Hash, Actor.Metrics.Blocks);
	MixInt(Hash, Actor.Metrics.Hits);
	MixDouble(Hash, Actor.Metrics.DamageDealt);
	MixDouble(Hash, Actor.Metrics.DamageTaken);
	MixDouble(Hash, Actor.Metrics.ManaDrained);
	MixDouble(Hash, Actor.Metrics.ManaRaised);
	MixDouble(Hash, Actor.Metrics.ManaReturned);
	MixInt(Hash, Actor.Metrics.Casts);
	MixInt(Hash, Actor.Metrics.Rolls);
}

// OriginPos, AimedAt and bHasAim are presentation only (SimTypes.h: "The sim does not read these"), so they stay out.
void MixProjectile(uint32& Hash, const FProjectile& Projectile)
{
	MixInt(Hash, Projectile.Id);
	MixInt(Hash, Projectile.OwnerId);
	MixInt(Hash, Projectile.ActivationId);
	MixDouble(Hash, Projectile.Damage);
	MixString(Hash, Projectile.Family);
	MixInt(Hash, Projectile.Tier);
	MixVec(Hash, Projectile.Source);
	MixBool(Hash, Projectile.bBolt);
	MixBool(Hash, Projectile.bReaction);
	MixString(Hash, Projectile.Delivery);
	MixVec(Hash, Projectile.Pos);
	MixVec(Hash, Projectile.PreviousPos);
	MixVec(Hash, Projectile.Velocity);
	MixDouble(Hash, Projectile.Radius);
	MixDouble(Hash, Projectile.RemainingM);
	MixInts(Hash, Projectile.HitIds);
	MixDouble(Hash, Projectile.BurstRadiusM);
	MixBool(Hash, Projectile.bPiercing);
	MixBool(Hash, Projectile.bReflected);
	MixDouble(Hash, Projectile.HeatOnHit);
	MixBool(Hash, Projectile.bSuppressPerfect);
	MixBool(Hash, Projectile.bPierceShields);
}

void MixTelegraph(uint32& Hash, const FTelegraph& Telegraph)
{
	MixInt(Hash, Telegraph.Id);
	MixInt(Hash, Telegraph.ActivationId);
	MixInt(Hash, Telegraph.OwnerId);
	MixString(Hash, Telegraph.Family);
	MixInt(Hash, Telegraph.Tier);
	MixDouble(Hash, Telegraph.Damage);
	MixVec(Hash, Telegraph.Source);
	MixBool(Hash, Telegraph.bBolt);
	MixString(Hash, Telegraph.Kind);
	MixVec(Hash, Telegraph.Origin);
	MixVec(Hash, Telegraph.Target);
	MixInt(Hash, Telegraph.ResolveTick);
	MixInt(Hash, Telegraph.StartTick);
	MixDouble(Hash, Telegraph.SpeedMps);
	MixDouble(Hash, Telegraph.RangeM);
	MixDouble(Hash, Telegraph.WidthM);
	MixDouble(Hash, Telegraph.RootS);
	MixDouble(Hash, Telegraph.PullM);
	MixBool(Hash, Telegraph.bSurvivesOwner);
	MixDouble(Hash, Telegraph.WallStunS);
	MixDouble(Hash, Telegraph.HeatOnHit);
	MixBool(Hash, Telegraph.bSuppressPerfect);
	MixBool(Hash, Telegraph.bPierceShields);
	MixBool(Hash, Telegraph.bCommitted);
}

void MixZone(uint32& Hash, const FZone& Zone)
{
	MixInt(Hash, Zone.Id);
	MixInt(Hash, Zone.OwnerId);
	MixVec(Hash, Zone.Pos);
	MixDouble(Hash, Zone.RadiusM);
	MixInt(Hash, Zone.Until);
	MixString(Hash, Zone.Kind);
	MixDouble(Hash, Zone.SlowMult);
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
			if (Spell->Tier > Rules->Split.MaxTier)
			{
				Rules->Refuse(Actor, TEXT("tier"));
				return;
			}
			if (Actor.Water.Flow >= Data.FlowMax)
			{
				Rules->Refuse(Actor, TEXT("crest"));
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

void UpdateTelegraphs(FArenaState& State, FVrRuleset* Rules)
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
			SpawnProjectile(State, Hit, Telegraph.Origin, Direction, Telegraph.SpeedMps, Telegraph.RangeM, {}, Telegraph.Target);
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
				const FSimHitResult Result = ResolveHit(State, Target, Hit, Rules);
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

bool WallClaimsProjectile(const FArenaState& State, const FVrWall& Wall, const FProjectile& Projectile, const FActor* ProjectileOwner)
{
	if (Projectile.OwnerId == Wall.OwnerId)
	{
		return false;
	}
	if (const FActor* WallOwner = SimFindActor(State, Wall.OwnerId))
	{
		if (ProjectileOwner && ProjectileOwner->Team == WallOwner->Team)
		{
			return false;
		}
	}
	return true;
}

void UpdateProjectiles(FArenaState& State, FVrRuleset* Rules)
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
		double WallT = std::numeric_limits<double>::infinity();
		const FVrWall* Stopping = nullptr;
		if (Rules && Rules->bActive && Rules->FireWall.bEnabled && Projectile.Family != TEXT("unblockable")
			&& (Projectile.Family == TEXT("magic") || Projectile.Family == TEXT("physical")))
		{
			for (const FVrWall& Wall : Rules->Walls)
			{
				if (!WallClaimsProjectile(State, Wall, Projectile, Owner))
				{
					continue;
				}
				double CrossT = 0.0;
				if (VrWallSegmentCrosses(Wall, Projectile.Pos, End, CrossT) && CrossT < WallT)
				{
					WallT = CrossT;
					Stopping = &Wall;
				}
			}
		}
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
		if (Stopping && WallClaimsProjectile(State, *Stopping, Projectile, Owner) && (!First || WallT <= FirstT))
		{
			if (FActor* Caster = SimFindActor(State, Stopping->OwnerId))
			{
				GainHeat(State, *Caster, Rules->FireWall.HeatPerStop);
			}
			UE_LOG(LogMageArena, Log, TEXT("defence firewall stop wall=%d projectile=%d owner=%d"), Stopping->Id, Projectile.Id, Stopping->OwnerId);
			Projectile.RemainingM = 0.0;
			continue;
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
			const FSimHitResult Result = ResolveHit(State, *First, Hit, Rules);
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
						ResolveHit(State, Target, Burst, Rules);
						Projectile.HitIds.Add(Target.Id);
					}
				}
			}
			if (Stopping && WallClaimsProjectile(State, *Stopping, Projectile, Owner) && Projectile.bPiercing && !Result.bPerfect)
			{
				if (FActor* Caster = SimFindActor(State, Stopping->OwnerId))
				{
					GainHeat(State, *Caster, Rules->FireWall.HeatPerStop);
				}
				UE_LOG(LogMageArena, Log, TEXT("defence firewall stop wall=%d projectile=%d owner=%d"), Stopping->Id, Projectile.Id, Stopping->OwnerId);
				Projectile.RemainingM = 0.0;
			}
			else if (!Projectile.bPiercing || Result.bPerfect)
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

FSimHitResult ResolveHit(FArenaState& State, FActor& Target, const FHit& Hit, const FVrRuleset* Rules)
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
	bool bFreshWindow = !Hit.bSuppressPerfect && State.Tick - Target.AbsorbFreshTick <= SimTicks(Data.Absorb.WindowS);
	if (Rules && Target.bAbsorb && Rules->IsSplitCasting(Target.Id))
	{
		bFreshWindow = false;
	}
	FAbsorbVerdict Verdict = EvaluateAbsorb(Data.Absorb, bGuarded, bFreshWindow, KindFromFamily(Hit.Family), Hit.Tier, static_cast<double>(Target.Ranks.Nerve));
	if (Rules && Rules->IsPlanted(Target.Id) && !Verdict.bPerfect)
	{
		const double Dome = Rules->DomeReduction(Hit.Family);
		if (Dome > Verdict.Reduction)
		{
			Verdict.Reduction = Dome;
			Verdict.bPerfect = false;
		}
	}
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

FProjectile& SpawnProjectile(FArenaState& State, const FHit& Hit, const FSimVec& Origin, const FSimVec& Direction, double SpeedMps, double RangeM, TOptional<double> Radius, TOptional<FSimVec> AimedAt)
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
	Projectile.OriginPos = Origin;
	Projectile.Velocity = SimScale(UnitDirection, SpeedMps);
	Projectile.Radius = Radius.Get(KernelData().ProjectileRadiusM);
	Projectile.RemainingM = RangeM;
	if (AimedAt.IsSet())
	{
		Projectile.bHasAim = true;
		Projectile.AimedAt = AimedAt.GetValue();
	}
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

FString StateHash(const FArenaState& State)
{
	uint32 Hash = FnvOffset;
	MixInt(Hash, State.Tick);
	MixUInt(Hash, State.Seed);
	MixUInt(Hash, State.Rng);
	MixInt(Hash, State.NextId);
	// Each list is counted first, so dropping or adding an element moves the hash even when the survivors are equal.
	MixInt(Hash, State.Actors.Num());
	for (const FActor& Actor : State.Actors)
	{
		MixActor(Hash, Actor);
	}
	MixInt(Hash, State.Projectiles.Num());
	for (const FProjectile& Projectile : State.Projectiles)
	{
		MixProjectile(Hash, Projectile);
	}
	MixInt(Hash, State.Telegraphs.Num());
	for (const FTelegraph& Telegraph : State.Telegraphs)
	{
		MixTelegraph(Hash, Telegraph);
	}
	MixInt(Hash, State.Zones.Num());
	for (const FZone& Zone : State.Zones)
	{
		MixZone(Hash, Zone);
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
