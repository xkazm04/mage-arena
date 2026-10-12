#include "Kernel/ArenaKernel.h"

namespace
{
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

// T23. Mixed only for an air actor, so every water and fire state hashes as it did before Air existed (the fire
// scenario hashes in scenarios-vr and the T19 digest stay byte-identical).
void MixAir(uint32& Hash, const FAirState& Air)
{
	if (!Air.bSchool)
	{
		return;
	}
	MixBool(Hash, Air.bSchool);
	MixDouble(Hash, Air.Momentum);
	MixDouble(Hash, Air.MaxMomentum);
	MixInt(Hash, Air.LockUntil);
	MixDouble(Hash, Air.LockValue);
	MixDouble(Hash, Air.AbsorbDrainSpellMult);
	MixInt(Hash, Air.AbsorbDrainSpellUntil);
	MixInt(Hash, Air.FormUntil);
	MixDouble(Hash, Air.FormPhysical);
	MixDouble(Hash, Air.FormMagic);
	MixInt(Hash, Air.Forms);
	MixInt(Hash, Air.Evades);
	MixInt(Hash, Air.Deflects);
	MixInt(Hash, Air.TempestCastTick);
	MixInt(Hash, Air.Cooldowns.Num());
	for (const FFireCooldown& Entry : Air.Cooldowns)
	{
		MixString(Hash, Entry.SpellId);
		MixInt(Hash, Entry.Until);
	}
	MixBool(Hash, Air.Volley.IsSet());
	if (Air.Volley.IsSet())
	{
		const FAirVolley& Volley = Air.Volley.GetValue();
		MixString(Hash, Volley.SpellId);
		MixVec(Hash, Volley.Aim);
		MixInt(Hash, Volley.Left);
		MixInt(Hash, Volley.NextTick);
		MixInt(Hash, Volley.IntervalTicks);
		MixDouble(Hash, Volley.Damage);
		MixDouble(Hash, Volley.RangeMult);
		MixInt(Hash, Volley.PierceLeft);
	}
	MixBool(Hash, Air.Channel.IsSet());
	if (Air.Channel.IsSet())
	{
		const FFireChannel& Channel = Air.Channel.GetValue();
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
	MixAir(Hash, Actor.Air);
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
	// T23: an air projectile's flight fields, only when it is one (see MixAir).
	if (Projectile.bAir || Projectile.bCurved)
	{
		MixBool(Hash, Projectile.bAir);
		MixDouble(Hash, Projectile.MomentumOnHit);
		MixInt(Hash, Projectile.PierceLeft);
		MixBool(Hash, Projectile.bCurved);
		MixVec(Hash, Projectile.ArcCentre);
		MixDouble(Hash, Projectile.ArcRadiusM);
		MixDouble(Hash, Projectile.ArcAngle);
		MixDouble(Hash, Projectile.ArcSign);
		MixDouble(Hash, Projectile.ArcLeftM);
	}
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
