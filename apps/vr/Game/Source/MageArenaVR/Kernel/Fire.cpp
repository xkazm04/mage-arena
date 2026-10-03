#include "Kernel/Fire.h"

#include "Kernel/ArenaKernel.h"
#include "Kernel/Geometry.h"

#include "Containers/Set.h"

#include <cmath>

void GainHeat(FArenaState& State, FActor& Actor, double Amount)
{
	if (!Actor.Fire.bSchool || !(Amount > 0.0) || State.Tick < Actor.Fire.LockUntil)
	{
		return;
	}
	const FHeatRules& Rules = KernelData().Heat;
	Actor.Fire.Heat = SimClamp(Actor.Fire.Heat + Amount, Rules.Min, Rules.Max);
	Actor.Fire.MaxHeat = std::max(Actor.Fire.MaxHeat, Actor.Fire.Heat);
	Actor.Fire.LastGainTick = State.Tick;
}

namespace
{
void SetFireCooldown(FActor& Actor, const FString& SpellId, int32 Until)
{
	for (FFireCooldown& Entry : Actor.Fire.Cooldowns)
	{
		if (Entry.SpellId == SpellId)
		{
			Entry.Until = Until;
			return;
		}
	}
	Actor.Fire.Cooldowns.Add({SpellId, Until});
}

FString HitFamily(const FFireSpell& Spell)
{
	return Spell.Family == TEXT("n/a") ? TEXT("magic") : Spell.Family;
}

double HeatPaid(const FFireSpell& Spell)
{
	if (Spell.HeatMode == EFireHeatMode::Hit || Spell.HeatMode == EFireHeatMode::Target || Spell.HeatMode == EFireHeatMode::Tick)
	{
		return Spell.HeatAmount;
	}
	return 0.0;
}

FHit MakeFireHit(FArenaState& State, const FActor& Actor, const FFireSpell& Spell, int32 ActivationId, double Damage)
{
	FHit Hit;
	Hit.OwnerId = Actor.Id;
	Hit.ActivationId = ActivationId;
	Hit.Damage = Damage;
	Hit.Family = HitFamily(Spell);
	Hit.Tier = Spell.Tier;
	Hit.Source = Actor.Pos;
	Hit.bBolt = Spell.bBolt;
	Hit.bPierceShields = Spell.bPierceShields;
	Hit.HeatOnHit = HeatPaid(Spell);
	return Hit;
}

FSimVec AimDirection(const FActor& Actor, const FSimVec& Aim)
{
	return SimUnit(SimSub(Aim, Actor.Pos), Actor.Facing);
}

FSimVec PlacedPoint(const FActor& Actor, const FSimVec& Aim, double RangeM)
{
	const FSimVec Direction = AimDirection(Actor, Aim);
	const double Distance = std::min(SimDistance(Actor.Pos, Aim), RangeM);
	return {Actor.Pos.X + Direction.X * Distance, Actor.Pos.Y + Direction.Y * Distance};
}

bool Hostile(const FActor& Actor, const FActor& Target)
{
	return !Target.bDown && Target.Team != Actor.Team && Target.Id != Actor.Id;
}

void ApplyAreaHit(FArenaState& State, FActor& Actor, FActor& Target, FHit& Hit, double KnockbackM)
{
	const FSimHitResult Result = ResolveHit(State, Target, Hit);
	if (KnockbackM > 0.0 && !Target.bDown && !Result.bPerfect && Result.Damage > 0.0)
	{
		const FSimVec Away = SimUnit(SimSub(Target.Pos, Actor.Pos));
		Target.Pos = ConstrainToArena(SimAdd(Target.Pos, SimScale(Away, KnockbackM)), Target.Radius);
	}
}

void HitAlongSegment(FArenaState& State, FActor& Actor, const FFireSpell& Spell, const FSimVec& From, const FSimVec& To, int32 ActivationId, double Damage, bool bSuppressPerfect)
{
	FHit Hit = MakeFireHit(State, Actor, Spell, ActivationId, Damage);
	Hit.bSuppressPerfect = bSuppressPerfect;
	Hit.Delivery = TEXT("line");
	Hit.Source = From;
	for (FActor& Target : State.Actors)
	{
		if (!Hostile(Actor, Target) || !SimSegmentHit(From, To, Target.Pos, Target.Radius).IsSet())
		{
			continue;
		}
		ResolveHit(State, Target, Hit);
	}
}

int32 EnemiesWithin(const FArenaState& State, const FActor& Actor, double RadiusM)
{
	int32 Count = 0;
	for (const FActor& Other : State.Actors)
	{
		if (Other.Id != Actor.Id && Other.Team != Actor.Team && !Other.bDown && SimDistance(Actor.Pos, Other.Pos) <= RadiusM)
		{
			++Count;
		}
	}
	return Count;
}

double ThresholdValue(const FActor& Actor, bool FHeatThreshold::* Flag, double FHeatThreshold::* Value, double Fallback)
{
	if (!Actor.Fire.bSchool)
	{
		return Fallback;
	}
	double Result = Fallback;
	for (const FHeatThreshold& Threshold : KernelData().Heat.Thresholds)
	{
		if (Actor.Fire.Heat < Threshold.At)
		{
			continue;
		}
		if (Threshold.*Flag)
		{
			Result = Threshold.*Value;
		}
	}
	return Result;
}
}

const FFireSpell* FireSpellFor(const FActor& Actor, int32 Slot)
{
	const TArray<FFireSpell>& Spells = KernelData().FireSpells;
	if (!Actor.Fire.bSchool || !Spells.IsValidIndex(Slot))
	{
		return nullptr;
	}
	const FFireSpell& Spell = Spells[Slot];
	if (Spell.Tier > Actor.Tier)
	{
		return nullptr;
	}
	return &Spell;
}

int32 FireCooldownUntil(const FActor& Actor, const FString& SpellId)
{
	for (const FFireCooldown& Entry : Actor.Fire.Cooldowns)
	{
		if (Entry.SpellId == SpellId)
		{
			return Entry.Until;
		}
	}
	return 0;
}

bool TryFireCast(FArenaState& State, FActor& Actor, const FInputFrame& Input)
{
	if (Actor.Pending.IsSet() || FireChannelBusy(Actor))
	{
		return false;
	}
	const FFireSpell* Spell = FireSpellFor(Actor, Input.Slot);
	if (!Spell || State.Tick < FireCooldownUntil(Actor, Spell->Id) || Actor.Mana < Spell->Mana)
	{
		return false;
	}
	Actor.Mana -= Spell->Mana;
	SetFireCooldown(Actor, Spell->Id, State.Tick + SimTicks(Spell->CooldownS));
	// Sunfall's cast and its shadow are two phases, so the windup is only cast_s. Every other row uses the longer of the two columns.
	const double Windup = Spell->bSequentialTelegraph ? Spell->CastS : std::max(Spell->CastS, Spell->TelegraphS);
	FPendingCast Pending;
	Pending.Kind = TEXT("fire");
	Pending.StartTick = State.Tick;
	Pending.ReleaseTick = State.Tick + SimTicks(Windup);
	Pending.Aim = (Spell->Kind == TEXT("zone") || Spell->Kind == TEXT("meteor")) ? PlacedPoint(Actor, Input.Aim, Spell->RangeM) : Input.Aim;
	Pending.ActivationId = State.NextId++;
	Pending.SpellId = Spell->Id;
	Actor.Pending = Pending;
	Actor.Metrics.Casts++;
	if (Spell->Id == TEXT("fire_sunfall"))
	{
		Actor.Fire.SunfallCastTick = State.Tick;
	}
	Emit(State, TEXT("cast"), Actor, Spell->Tier);
	return true;
}

void ReleaseFire(FArenaState& State, FActor& Actor)
{
	if (!Actor.Pending.IsSet() || Actor.Pending->Kind != TEXT("fire"))
	{
		return;
	}
	const FPendingCast Pending = Actor.Pending.GetValue();
	Actor.Pending.Reset();
	const FFireSpell* Spell = Pending.SpellId.IsSet() ? FindFireSpell(Pending.SpellId.GetValue()) : nullptr;
	if (!Spell)
	{
		return;
	}
	const double Mult = FireSpellDamageMult(Actor);
	double Damage = Spell->Damage * Mult;
	if (Pending.DamageMult.IsSet())
	{
		Damage *= Pending.DamageMult.GetValue();
	}
	const FSimVec Direction = AimDirection(Actor, Pending.Aim);
	if (Spell->Kind == TEXT("projectile"))
	{
		FHit Hit = MakeFireHit(State, Actor, *Spell, Pending.ActivationId, Damage);
		const double Speed = Spell->SpeedMps;
		SpawnProjectile(State, Hit, Actor.Pos, Direction, Speed, Spell->RangeM);
	}
	else if (Spell->Kind == TEXT("cone") || Spell->Kind == TEXT("ring"))
	{
		const double Reach = Spell->Kind == TEXT("ring") && Spell->RadiusM > 0.0 ? Spell->RadiusM : Spell->RangeM;
		for (FActor& Target : State.Actors)
		{
			if (!Hostile(Actor, Target) || SimDistance(Actor.Pos, Target.Pos) > Reach || !SimInArc(Direction, SimSub(Target.Pos, Actor.Pos), Spell->ArcDeg))
			{
				continue;
			}
			FHit Hit = MakeFireHit(State, Actor, *Spell, Pending.ActivationId, Damage);
			Hit.Delivery = TEXT("area");
			ApplyAreaHit(State, Actor, Target, Hit, Spell->KnockbackM);
		}
	}
	else if (Spell->Kind == TEXT("line"))
	{
		const FSimVec End = SimAdd(Actor.Pos, SimScale(Direction, Spell->RangeM));
		HitAlongSegment(State, Actor, *Spell, Actor.Pos, End, Pending.ActivationId, Damage, false);
	}
	else if (Spell->Kind == TEXT("zone"))
	{
		for (FActor& Target : State.Actors)
		{
			if (!Hostile(Actor, Target) || SimDistance(Pending.Aim, Target.Pos) > Spell->RadiusM + Target.Radius)
			{
				continue;
			}
			FHit Hit = MakeFireHit(State, Actor, *Spell, Pending.ActivationId, Damage);
			Hit.Delivery = TEXT("area");
			Hit.Source = Pending.Aim;
			ResolveHit(State, Target, Hit);
		}
	}
	else if (Spell->Kind == TEXT("meteor"))
	{
		FTelegraph Telegraph;
		Telegraph.Id = State.NextId++;
		Telegraph.ActivationId = Pending.ActivationId;
		Telegraph.OwnerId = Actor.Id;
		Telegraph.Family = HitFamily(*Spell);
		Telegraph.Tier = Spell->Tier;
		Telegraph.Damage = Damage;
		Telegraph.Source = Actor.Pos;
		Telegraph.Kind = TEXT("area");
		Telegraph.Origin = Actor.Pos;
		Telegraph.Target = Pending.Aim;
		Telegraph.ResolveTick = State.Tick + SimTicks(Spell->TelegraphS);
		Telegraph.StartTick = State.Tick;
		Telegraph.WidthM = Spell->RadiusM;
		Telegraph.HeatOnHit = HeatPaid(*Spell);
		Telegraph.bSurvivesOwner = true;
		Telegraph.bCommitted = true;
		State.Telegraphs.Add(Telegraph);
		Actor.Fire.SunfallTick = State.Tick;
		Actor.Fire.bSunfallLoosed = true;
	}
	else if (Spell->Kind == TEXT("dash"))
	{
		const FSimVec Start = Actor.Pos;
		Actor.Pos = ConstrainToArena(SimAdd(Start, SimScale(Direction, Spell->DashM)), Actor.Radius);
		FFireTrail Trail;
		Trail.From = Start;
		Trail.To = Actor.Pos;
		Trail.Until = State.Tick + SimTicks(Spell->DurationS);
		Trail.IntervalTicks = std::max(1, SimTicks(Spell->TickS));
		Trail.NextTick = State.Tick + Trail.IntervalTicks;
		Trail.Damage = Spell->Damage;
		if (Pending.DamageMult.IsSet())
		{
			Trail.Damage *= Pending.DamageMult.GetValue();
		}
		Trail.HeatOnHit = HeatPaid(*Spell);
		Trail.Tier = Spell->Tier;
		Trail.Family = HitFamily(*Spell);
		Trail.SpellId = Spell->Id;
		Actor.Fire.Trails.Add(Trail);
	}
	else if (Spell->Kind == TEXT("beam"))
	{
		FFireChannel Channel;
		Channel.SpellId = Spell->Id;
		Channel.Until = State.Tick + SimTicks(Spell->DurationS);
		Channel.IntervalTicks = std::max(1, SimTicks(Spell->TickS));
		Channel.NextTick = State.Tick + Channel.IntervalTicks;
		Channel.Damage = Spell->Damage;
		if (Pending.DamageMult.IsSet())
		{
			Channel.Damage *= Pending.DamageMult.GetValue();
		}
		Channel.HeatOnHit = HeatPaid(*Spell);
		Channel.RangeM = Spell->RangeM;
		Channel.Tier = Spell->Tier;
		Channel.Family = HitFamily(*Spell);
		Channel.bPierceShields = Spell->bPierceShields;
		Channel.bPerfectOnlyFirstTick = Spell->bPerfectOnlyFirstTick;
		Actor.Fire.Channel = Channel;
	}
	else if (Spell->HeatMode == EFireHeatMode::Instant)
	{
		GainHeat(State, Actor, Spell->InstantHeat);
	}
	else if (Spell->HeatMode == EFireHeatMode::Lock)
	{
		const FHeatRules& Rules = KernelData().Heat;
		Actor.Fire.Heat = SimClamp(Spell->HeatLockValue, Rules.Min, Rules.Max);
		Actor.Fire.MaxHeat = std::max(Actor.Fire.MaxHeat, Actor.Fire.Heat);
		Actor.Fire.LockValue = Actor.Fire.Heat;
		Actor.Fire.LockUntil = State.Tick + SimTicks(Spell->HeatLockS);
		Actor.Fire.LastGainTick = State.Tick;
		Actor.Fire.AbsorbDrainSpellMult = Spell->AbsorbDrainMult;
		Actor.Fire.AbsorbDrainSpellUntil = Actor.Fire.LockUntil;
	}
}

void UpdateFireResource(FArenaState& State, FActor& Actor)
{
	if (!Actor.Fire.bSchool)
	{
		return;
	}
	const FHeatRules& Rules = KernelData().Heat;
	if (State.Tick < Actor.Fire.LockUntil)
	{
		Actor.Fire.Heat = Actor.Fire.LockValue;
		Actor.Fire.MaxHeat = std::max(Actor.Fire.MaxHeat, Actor.Fire.Heat);
		return;
	}
	const int32 Near = EnemiesWithin(State, Actor, Rules.EnemyWithinM);
	if (Near > 0)
	{
		GainHeat(State, Actor, Rules.PerSecondPerEnemyWithin * static_cast<double>(Near) * SimDt());
	}
	if (Actor.Fire.Heat > 0.0 && Actor.Fire.LastGainTick != NeverTick
		&& State.Tick - Actor.Fire.LastGainTick >= SimTicks(Rules.DecayAfterNoGainS))
	{
		Actor.Fire.Heat = SimClamp(Actor.Fire.Heat - Rules.DecayPerSecond * SimDt(), Rules.Min, Rules.Max);
	}
}

void UpdateFireOngoing(FArenaState& State, FActor& Actor)
{
	if (!Actor.Fire.bSchool)
	{
		return;
	}
	TSet<int32> TrailClaimed;
	for (FFireTrail& Trail : Actor.Fire.Trails)
	{
		if (Trail.IntervalTicks <= 0 || State.Tick < Trail.NextTick || State.Tick > Trail.Until)
		{
			continue;
		}
		const FFireSpell* Spell = FindFireSpell(Trail.SpellId);
		const double Damage = Trail.Damage * FireSpellDamageMult(Actor);
		const int32 ActivationId = State.NextId++;
		for (FActor& Target : State.Actors)
		{
			if (!Hostile(Actor, Target) || TrailClaimed.Contains(Target.Id) || !SimSegmentHit(Trail.From, Trail.To, Target.Pos, Target.Radius).IsSet())
			{
				continue;
			}
			TrailClaimed.Add(Target.Id);
			FHit Hit;
			Hit.OwnerId = Actor.Id;
			Hit.ActivationId = ActivationId;
			Hit.Damage = Damage;
			Hit.Family = Trail.Family;
			Hit.Tier = Trail.Tier;
			Hit.Source = Trail.From;
			Hit.Delivery = TEXT("area");
			Hit.HeatOnHit = Trail.HeatOnHit;
			if (Spell)
			{
				Hit.bPierceShields = Spell->bPierceShields;
			}
			ResolveHit(State, Target, Hit);
		}
		Trail.NextTick += Trail.IntervalTicks;
	}
	Actor.Fire.Trails.RemoveAll([](const FFireTrail& Trail)
	{
		return Trail.NextTick > Trail.Until;
	});

	if (!Actor.Fire.Channel.IsSet())
	{
		return;
	}
	FFireChannel& Channel = Actor.Fire.Channel.GetValue();
	if (Channel.IntervalTicks > 0 && State.Tick >= Channel.NextTick && State.Tick <= Channel.Until)
	{
		const FFireSpell* Spell = FindFireSpell(Channel.SpellId);
		const double Damage = Channel.Damage * FireSpellDamageMult(Actor);
		const bool bSuppress = Channel.bPerfectOnlyFirstTick && Channel.TicksDone > 0;
		const FSimVec Direction = Actor.Facing;
		const FSimVec End = SimAdd(Actor.Pos, SimScale(Direction, Channel.RangeM));
		if (Spell)
		{
			HitAlongSegment(State, Actor, *Spell, Actor.Pos, End, State.NextId++, Damage, bSuppress);
		}
		Channel.TicksDone++;
		Channel.NextTick += Channel.IntervalTicks;
	}
	if (Channel.NextTick > Channel.Until)
	{
		Actor.Fire.Channel.Reset();
	}
}

double FireSpellDamageMult(const FActor& Actor)
{
	return ThresholdValue(Actor, &FHeatThreshold::bSpellDamage, &FHeatThreshold::SpellDamageMult, 1.0);
}

double FireAbsorbDrainMult(const FArenaState& State, const FActor& Actor)
{
	double Mult = ThresholdValue(Actor, &FHeatThreshold::bAbsorbDrain, &FHeatThreshold::AbsorbDrainMult, 1.0);
	if (Actor.Fire.bSchool && State.Tick < Actor.Fire.AbsorbDrainSpellUntil)
	{
		Mult *= Actor.Fire.AbsorbDrainSpellMult;
	}
	return Mult;
}

double FireStaminaRegenMult(const FActor& Actor)
{
	return ThresholdValue(Actor, &FHeatThreshold::bStaminaRegen, &FHeatThreshold::StaminaRegenMult, 1.0);
}

bool FireCastRoots(const FActor& Actor)
{
	if (!Actor.Fire.bSchool || !Actor.Pending.IsSet() || Actor.Pending->Kind != TEXT("fire") || !Actor.Pending->SpellId.IsSet())
	{
		return false;
	}
	const FFireSpell* Spell = FindFireSpell(Actor.Pending->SpellId.GetValue());
	return Spell && Spell->bRootDuringCast;
}

bool FireChannelBusy(const FActor& Actor)
{
	return Actor.Fire.bSchool && Actor.Fire.Channel.IsSet();
}

void CancelFireChannel(FActor& Actor)
{
	Actor.Fire.Channel.Reset();
}

void ResetFire(FActor& Actor)
{
	const bool bSchool = Actor.Fire.bSchool;
	Actor.Fire = FFireState();
	Actor.Fire.bSchool = bSchool;
}

void FireOnHitResolved(FArenaState& State, FActor& Target, const FHit& Hit, double Incoming, double Reduction)
{
	if (Hit.HeatOnHit > 0.0)
	{
		if (FActor* Owner = SimFindActor(State, Hit.OwnerId))
		{
			GainHeat(State, *Owner, Hit.HeatOnHit);
		}
	}
	if (Target.Fire.bSchool && Reduction <= 0.0 && Incoming > 0.0)
	{
		GainHeat(State, Target, KernelData().Heat.PerHitTakenUnabsorbed);
	}
}
