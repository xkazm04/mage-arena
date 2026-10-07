#include "Kernel/Air.h"

#include "Kernel/ArenaKernel.h"
#include "Kernel/Geometry.h"
#include "Kernel/VrRules.h"

#include <algorithm>
#include <cmath>

// Air school, T23. The design and every reading that is not the literal cell are in docs/schools/AIR.md ("Kernel
// readings"). Fire (Kernel/Fire.cpp) is the template: Momentum is Heat's twin, the cast path is the fire cast path.

void GainMomentum(FArenaState& State, FActor& Actor, double Amount)
{
	if (!Actor.Air.bSchool || !(Amount > 0.0) || State.Tick < Actor.Air.LockUntil)
	{
		return;
	}
	const FMomentumRules& Rules = KernelData().Momentum;
	Actor.Air.Momentum = SimClamp(Actor.Air.Momentum + Amount, Rules.Min, Rules.Max);
	Actor.Air.MaxMomentum = std::max(Actor.Air.MaxMomentum, Actor.Air.Momentum);
}

namespace
{
void SetAirCooldown(FActor& Actor, const FString& SpellId, int32 Until)
{
	for (FFireCooldown& Entry : Actor.Air.Cooldowns)
	{
		if (Entry.SpellId == SpellId)
		{
			Entry.Until = Until;
			return;
		}
	}
	Actor.Air.Cooldowns.Add({SpellId, Until});
}

FString HitFamily(const FAirSpell& Spell)
{
	return Spell.Family == TEXT("n/a") ? TEXT("magic") : Spell.Family;
}

double MomentumPaid(const FAirSpell& Spell)
{
	if (Spell.MomentumMode == EAirMomentumMode::Hit || Spell.MomentumMode == EAirMomentumMode::Target || Spell.MomentumMode == EAirMomentumMode::Tick)
	{
		return Spell.MomentumAmount;
	}
	return 0.0;
}

FHit MakeAirHit(const FActor& Actor, const FAirSpell& Spell, int32 ActivationId, double Damage)
{
	FHit Hit;
	Hit.OwnerId = Actor.Id;
	Hit.ActivationId = ActivationId;
	Hit.Damage = Damage;
	Hit.Family = HitFamily(Spell);
	Hit.Tier = Spell.Tier;
	Hit.Source = Actor.Pos;
	Hit.bBolt = Spell.bBolt;
	Hit.MomentumOnHit = MomentumPaid(Spell);
	return Hit;
}

FSimVec AimDirection(const FActor& Actor, const FSimVec& Aim)
{
	return SimUnit(SimSub(Aim, Actor.Pos), Actor.Facing);
}

bool Hostile(const FActor& Actor, const FActor& Target)
{
	return !Target.bDown && Target.Team != Actor.Team && Target.Id != Actor.Id;
}

// Highest band at or below the current Momentum that names the effect. A non-air actor gets the fallback.
template <typename TValue>
TValue Band(const FActor& Actor, bool FMomentumThreshold::* Flag, TValue FMomentumThreshold::* Value, TValue Fallback)
{
	if (!Actor.Air.bSchool)
	{
		return Fallback;
	}
	TValue Result = Fallback;
	for (const FMomentumThreshold& Threshold : KernelData().Momentum.Thresholds)
	{
		if (Actor.Air.Momentum >= Threshold.At && Threshold.*Flag)
		{
			Result = Threshold.*Value;
		}
	}
	return Result;
}

FProjectile& SpawnAirProjectile(FArenaState& State, const FActor& Actor, const FAirSpell& Spell, const FHit& Hit, const FSimVec& Direction, double RangeM)
{
	FProjectile& Projectile = SpawnProjectile(State, Hit, Actor.Pos, Direction, Spell.SpeedMps, RangeM);
	Projectile.bAir = true;
	Projectile.MomentumOnHit = Hit.MomentumOnHit;
	Projectile.PierceLeft = AirSpellsPierce(Actor);
	return Projectile;
}

// Unlike Fire's area and line rows, the air hits pass the ruleset to ResolveHit, so the planted-staff dome and the split
// latch apply to them as they apply to projectiles (Kernel readings, AIR.md).
void HitAlongSegment(FArenaState& State, FActor& Actor, const FAirSpell& Spell, const FSimVec& From, const FSimVec& To, int32 ActivationId, double Damage, const FVrRuleset* Rules)
{
	FHit Hit = MakeAirHit(Actor, Spell, ActivationId, Damage);
	Hit.Delivery = TEXT("line");
	Hit.Source = From;
	for (FActor& Target : State.Actors)
	{
		if (!Hostile(Actor, Target) || !SimSegmentHit(From, To, Target.Pos, Target.Radius).IsSet())
		{
			continue;
		}
		ResolveHit(State, Target, Hit, Rules);
	}
}

// Placed points and the dash end obey the VR geometry the spawn obeys (Games.cpp SpawnWave): narrow view compresses to
// the arc (as Fire's placed points do). bBody: the point is where the caster will stand, so it also stays out of the
// dais hold (DECISIONS 2026-10-03, CR-003). A placed blow (Downdraft) may land on the dais: that is its target.
FSimVec KeepOpponentPoint(const FVrRuleset* Rules, const FActor& Actor, FSimVec Point, bool bBody)
{
	if (!Rules || Actor.Team == 0)
	{
		return Point;
	}
	if (Rules->bNarrow)
	{
		Point = Rules->CompressToArc(Point);
	}
	if (bBody && Rules->bActive && Rules->InsideHold(Point))
	{
		Point = Rules->PushOutsideHold(Point);
		if (Rules->bNarrow)
		{
			Point = Rules->CompressToArc(Point);
		}
	}
	return Point;
}

void BeginAirCast(FArenaState& State, FActor& Actor, const FAirSpell& Spell, const FSimVec& Aim, const FVrRuleset* Rules, bool bPay)
{
	if (bPay)
	{
		Actor.Mana -= Spell.Mana;
		// Air Form's "spends 40" is paid at the press, with the mana. The lock of Eye of the Storm still wins next tick.
		if (Spell.MomentumMode == EAirMomentumMode::Spend)
		{
			const FMomentumRules& Momentum = KernelData().Momentum;
			Actor.Air.Momentum = SimClamp(Actor.Air.Momentum - Spell.MomentumAmount, Momentum.Min, Momentum.Max);
		}
	}
	SetAirCooldown(Actor, Spell.Id, State.Tick + SimTicks(Spell.CooldownS));
	// One windup, the longer of cast_s and telegraph_s, as every fire row but Sunfall.
	const double Windup = VrOpponentTelegraph(Rules, Actor.Team, std::max(Spell.CastS, Spell.TelegraphS));
	FPendingCast Pending;
	Pending.Kind = TEXT("air");
	Pending.StartTick = State.Tick;
	Pending.ReleaseTick = State.Tick + SimTicks(Windup);
	Pending.Aim = Aim;
	if (Spell.Kind == TEXT("zone"))
	{
		const double Range = Spell.RangeM * AirSpellRangeMult(Actor);
		const FSimVec Direction = AimDirection(Actor, Aim);
		const double Distance = std::min(SimDistance(Actor.Pos, Aim), Range);
		Pending.Aim = KeepOpponentPoint(Rules, Actor, FSimVec{Actor.Pos.X + Direction.X * Distance, Actor.Pos.Y + Direction.Y * Distance}, false);
	}
	Pending.ActivationId = State.NextId++;
	Pending.SpellId = Spell.Id;
	Actor.Pending = Pending;
	Actor.Metrics.Casts++;
	if (Spell.Kind == TEXT("line") && Spell.Family == TEXT("unblockable"))
	{
		Actor.Air.TempestCastTick = State.Tick;
	}
	Emit(State, TEXT("cast"), Actor, Spell.Tier);
}
}

double AirVeerSide(int32 ActivationId)
{
	return (ActivationId % 2 == 0) ? 1.0 : -1.0;
}

// The Veering Bolt arc. The bolt leaves the caster C at the entry angle E off the line of sight C->T and arrives at T
// at the same angle E on the other side of that line: a circular arc through C and T whose tangent makes E with the
// chord at both ends. With L = |T - C|:
//   radius        R = L / (2 sin E)
//   central angle 2E, turned in one direction only (fixed curvature 1/R for the whole flight)
//   arc length    S = 2 E R = E L / sin E          (E = 60 deg: S = 1.2092 L)
//   launch        v0 = rotate(u, Side * E), u = (T - C) / L
//   centre        C + R * rotate(v0, -Side * 90 deg), on the perpendicular bisector of C T
// The bolt turns the opposite way to its launch side (TurnSign = -Side). Point(s) = centre + R (cos a, sin a) with
// a = StartAngle + TurnSign * s / R, so Point(S) is T exactly. Pure double maths on the inputs, so replays match.
FAirArc AirVeerArc(const FSimVec& From, const FSimVec& To, double EntryDeg, double Side)
{
	FAirArc Arc;
	Arc.From = From;
	Arc.To = To;
	const double Chord = SimDistance(From, To);
	const FSimVec Along = SimUnit(SimSub(To, From));
	if (!(Chord > 1.0e-6) || !(EntryDeg > 0.0))
	{
		Arc.bStraight = true;
		Arc.LaunchDir = Along;
		Arc.LengthM = Chord;
		return Arc;
	}
	const double Entry = EntryDeg * SimPi / 180.0;
	Arc.RadiusM = Chord / (2.0 * std::sin(Entry));
	Arc.LengthM = 2.0 * Entry * Arc.RadiusM;
	Arc.LaunchDir = SimRotate(Along, Side * Entry);
	const FSimVec ToCentre = SimRotate(Arc.LaunchDir, -Side * SimPi / 2.0);
	Arc.Centre = SimAdd(From, SimScale(ToCentre, Arc.RadiusM));
	Arc.StartAngle = std::atan2(From.Y - Arc.Centre.Y, From.X - Arc.Centre.X);
	Arc.TurnSign = -Side;
	return Arc;
}

FSimVec AirArcPoint(const FAirArc& Arc, double DistanceM)
{
	if (Arc.bStraight)
	{
		return SimAdd(Arc.From, SimScale(Arc.LaunchDir, DistanceM));
	}
	const double S = SimClamp(DistanceM, 0.0, Arc.LengthM);
	const double Angle = Arc.StartAngle + Arc.TurnSign * S / Arc.RadiusM;
	FSimVec Point{Arc.Centre.X + Arc.RadiusM * std::cos(Angle), Arc.Centre.Y + Arc.RadiusM * std::sin(Angle)};
	if (DistanceM > Arc.LengthM)
	{
		// Past T the bolt flies straight along its arrival tangent.
		const FSimVec Tangent{-Arc.TurnSign * std::sin(Angle), Arc.TurnSign * std::cos(Angle)};
		Point = SimAdd(Point, SimScale(Tangent, DistanceM - Arc.LengthM));
	}
	return Point;
}

FSimVec AirCurvedStep(const FProjectile& Projectile, double Travel, double& OutAngle, FSimVec& OutVelocity, double& OutArcLeft)
{
	const double Speed = SimLength(Projectile.Velocity);
	OutAngle = Projectile.ArcAngle;
	OutVelocity = Projectile.Velocity;
	OutArcLeft = Projectile.ArcLeftM;
	if (!Projectile.bCurved || !(Projectile.ArcLeftM > 0.0) || !(Projectile.ArcRadiusM > 0.0))
	{
		return SimAdd(Projectile.Pos, SimScale(SimUnit(Projectile.Velocity), Travel));
	}
	const double OnArc = std::min(Travel, Projectile.ArcLeftM);
	OutAngle = Projectile.ArcAngle + Projectile.ArcSign * OnArc / Projectile.ArcRadiusM;
	FSimVec End{Projectile.ArcCentre.X + Projectile.ArcRadiusM * std::cos(OutAngle), Projectile.ArcCentre.Y + Projectile.ArcRadiusM * std::sin(OutAngle)};
	const FSimVec Tangent{-Projectile.ArcSign * std::sin(OutAngle), Projectile.ArcSign * std::cos(OutAngle)};
	OutVelocity = SimScale(Tangent, Speed);
	OutArcLeft = Projectile.ArcLeftM - OnArc;
	if (Travel > OnArc)
	{
		End = SimAdd(End, SimScale(Tangent, Travel - OnArc));
	}
	return End;
}

const FAirSpell* AirSpellFor(const FActor& Actor, int32 Slot)
{
	const TArray<FAirSpell>& Spells = KernelData().AirSpells;
	if (!Actor.Air.bSchool || !Spells.IsValidIndex(Slot))
	{
		return nullptr;
	}
	const FAirSpell& Spell = Spells[Slot];
	return Spell.Tier > Actor.Tier ? nullptr : &Spell;
}

int32 AirCooldownUntil(const FActor& Actor, const FString& SpellId)
{
	for (const FFireCooldown& Entry : Actor.Air.Cooldowns)
	{
		if (Entry.SpellId == SpellId)
		{
			return Entry.Until;
		}
	}
	return 0;
}

bool AirCastLegal(const FArenaState& State, const FActor& Actor, const FAirSpell& Spell)
{
	if (!Actor.Air.bSchool || Spell.Tier > Actor.Tier || State.Tick < AirCooldownUntil(Actor, Spell.Id) || Actor.Mana < Spell.Mana)
	{
		return false;
	}
	return !(Spell.NeedMomentum > 0.0) || Actor.Air.Momentum >= Spell.NeedMomentum;
}

bool TryAirCast(FArenaState& State, FActor& Actor, const FInputFrame& Input, const FVrRuleset* Rules)
{
	if (Actor.Pending.IsSet() || AirChannelBusy(Actor))
	{
		return false;
	}
	const FAirSpell* Spell = AirSpellFor(Actor, Input.Slot);
	if (!Spell || !AirCastLegal(State, Actor, *Spell))
	{
		return false;
	}
	BeginAirCast(State, Actor, *Spell, Input.Aim, Rules, true);
	return true;
}

bool StartGrantedAirCast(FArenaState& State, FActor& Actor, const FString& SpellId, const FSimVec& Aim, const FVrRuleset* Rules)
{
	if (!Actor.Air.bSchool || Actor.Pending.IsSet() || AirChannelBusy(Actor))
	{
		return false;
	}
	const FAirSpell* Spell = FindAirSpell(SpellId);
	if (!Spell)
	{
		return false;
	}
	BeginAirCast(State, Actor, *Spell, Aim, Rules, false);
	return true;
}

void ReleaseAir(FArenaState& State, FActor& Actor, const FVrRuleset* Rules)
{
	if (!Actor.Pending.IsSet() || Actor.Pending->Kind != TEXT("air"))
	{
		return;
	}
	const FPendingCast Pending = Actor.Pending.GetValue();
	Actor.Pending.Reset();
	const FAirSpell* Spell = Pending.SpellId.IsSet() ? FindAirSpell(Pending.SpellId.GetValue()) : nullptr;
	if (!Spell)
	{
		return;
	}
	const double Damage = Spell->Damage * Pending.DamageMult.Get(1.0);
	const double RangeMult = AirSpellRangeMult(Actor);
	const double Range = Spell->RangeM * RangeMult;
	const FSimVec Direction = AimDirection(Actor, Pending.Aim);
	if (Spell->Kind == TEXT("projectile"))
	{
		SpawnAirProjectile(State, Actor, *Spell, MakeAirHit(Actor, *Spell, Pending.ActivationId, Damage), Direction, Range);
	}
	else if (Spell->Kind == TEXT("twin"))
	{
		// The Twin Tides fan (Water.cpp ReleaseSpell): Count bolts spread evenly over SpreadDeg, one activation.
		const FHit Hit = MakeAirHit(Actor, *Spell, Pending.ActivationId, Damage);
		for (int32 Index = 0; Index < Spell->Count; ++Index)
		{
			const double Angle = (-Spell->SpreadDeg / 2.0 + Index * Spell->SpreadDeg / static_cast<double>(Spell->Count - 1)) * SimPi / 180.0;
			SpawnAirProjectile(State, Actor, *Spell, Hit, SimRotate(Direction, Angle), Range);
		}
	}
	else if (Spell->Kind == TEXT("veer"))
	{
		// The aim point is the arc's end, clamped to the range. Past it the bolt flies on straight for the unused range.
		const double Chord = std::min(SimDistance(Actor.Pos, Pending.Aim), Range);
		const FSimVec To = SimAdd(Actor.Pos, SimScale(Direction, Chord));
		const FAirArc Arc = AirVeerArc(Actor.Pos, To, Spell->EntryDeg, AirVeerSide(Pending.ActivationId));
		const FHit Hit = MakeAirHit(Actor, *Spell, Pending.ActivationId, Damage);
		FProjectile& Projectile = SpawnAirProjectile(State, Actor, *Spell, Hit, Arc.LaunchDir, Arc.LengthM + std::max(0.0, Range - Chord));
		Projectile.bHasAim = true;
		Projectile.AimedAt = To;
		if (!Arc.bStraight)
		{
			Projectile.bCurved = true;
			Projectile.ArcCentre = Arc.Centre;
			Projectile.ArcRadiusM = Arc.RadiusM;
			Projectile.ArcAngle = Arc.StartAngle;
			Projectile.ArcSign = Arc.TurnSign;
			Projectile.ArcLeftM = Arc.LengthM;
		}
	}
	else if (Spell->Kind == TEXT("squall"))
	{
		// The first orb leaves on the release tick with the cast's activation; the rest follow IntervalS apart, each with
		// its own activation id, from where the caster stands then, toward the same aim point.
		SpawnAirProjectile(State, Actor, *Spell, MakeAirHit(Actor, *Spell, Pending.ActivationId, Damage), Direction, Range);
		FAirVolley Volley;
		Volley.SpellId = Spell->Id;
		Volley.Aim = Pending.Aim;
		Volley.Left = Spell->Count - 1;
		Volley.IntervalTicks = std::max(1, SimTicks(Spell->IntervalS));
		Volley.NextTick = State.Tick + Volley.IntervalTicks;
		Volley.Damage = Damage;
		Volley.RangeMult = RangeMult;
		Volley.PierceLeft = AirSpellsPierce(Actor);
		if (Volley.Left > 0)
		{
			Actor.Air.Volley = Volley;
		}
	}
	else if (Spell->Kind == TEXT("zone"))
	{
		for (FActor& Target : State.Actors)
		{
			if (!Hostile(Actor, Target) || SimDistance(Pending.Aim, Target.Pos) > Spell->RadiusM + Target.Radius)
			{
				continue;
			}
			FHit Hit = MakeAirHit(Actor, *Spell, Pending.ActivationId, Damage);
			Hit.Delivery = TEXT("area");
			Hit.Source = Pending.Aim;
			ResolveHit(State, Target, Hit, Rules);
		}
	}
	else if (Spell->Kind == TEXT("line"))
	{
		const FSimVec End = SimAdd(Actor.Pos, SimScale(Direction, Range));
		HitAlongSegment(State, Actor, *Spell, Actor.Pos, End, Pending.ActivationId, Damage, Rules);
	}
	else if (Spell->Kind == TEXT("beam"))
	{
		FFireChannel Channel;
		Channel.SpellId = Spell->Id;
		Channel.Until = State.Tick + SimTicks(Spell->DurationS);
		Channel.IntervalTicks = std::max(1, SimTicks(Spell->TickS));
		Channel.NextTick = State.Tick + Channel.IntervalTicks;
		Channel.Damage = Damage;
		Channel.RangeM = Range;
		Channel.Tier = Spell->Tier;
		Channel.Family = HitFamily(*Spell);
		Actor.Air.Channel = Channel;
	}
	else if (Spell->Kind == TEXT("dash"))
	{
		// Cinder Step's dash without the trail: an instant step along the aim, then the arena clamp and the VR geometry.
		const FSimVec End = ConstrainToArena(SimAdd(Actor.Pos, SimScale(Direction, Spell->DashM)), Actor.Radius);
		Actor.Pos = ConstrainToArena(KeepOpponentPoint(Rules, Actor, End, true), Actor.Radius);
	}
	else if (Spell->Kind == TEXT("form"))
	{
		Actor.Air.FormUntil = State.Tick + SimTicks(Spell->DurationS);
		Actor.Air.FormPhysical = Spell->EvadePhysical;
		Actor.Air.FormMagic = Spell->EvadeMagic;
		Actor.Air.Forms++;
		Emit(State, TEXT("airform"), Actor, Spell->DurationS);
	}
	if (Spell->MomentumMode == EAirMomentumMode::Instant)
	{
		GainMomentum(State, Actor, Spell->MomentumAmount);
	}
	else if (Spell->MomentumMode == EAirMomentumMode::Lock)
	{
		const FMomentumRules& Momentum = KernelData().Momentum;
		Actor.Air.Momentum = SimClamp(Spell->MomentumLockValue, Momentum.Min, Momentum.Max);
		Actor.Air.MaxMomentum = std::max(Actor.Air.MaxMomentum, Actor.Air.Momentum);
		Actor.Air.LockValue = Actor.Air.Momentum;
		Actor.Air.LockUntil = State.Tick + SimTicks(Spell->MomentumLockS);
		Actor.Air.AbsorbDrainSpellMult = Spell->AbsorbDrainMult;
		Actor.Air.AbsorbDrainSpellUntil = Actor.Air.LockUntil;
	}
}

// Called right after MoveActor and before the same tick's release: the speed is this tick's locomotion (walk, sprint,
// roll), Pos - PreviousPos over one step. A Slipstream dash lands later in the tick and pays its own "+30 instant".
// Above MovingAboveMps it gains; at zero displacement ("still") it decays; in between neither runs.
void UpdateAirResource(FArenaState& State, FActor& Actor)
{
	if (!Actor.Air.bSchool)
	{
		return;
	}
	const FMomentumRules& Rules = KernelData().Momentum;
	if (State.Tick < Actor.Air.LockUntil)
	{
		Actor.Air.Momentum = Actor.Air.LockValue;
		Actor.Air.MaxMomentum = std::max(Actor.Air.MaxMomentum, Actor.Air.Momentum);
		return;
	}
	const double Speed = SimDistance(Actor.Pos, Actor.PreviousPos) / SimDt();
	if (Speed > Rules.MovingAboveMps)
	{
		GainMomentum(State, Actor, Rules.GainPerSecondMoving * SimDt());
	}
	else if (!(Speed > 1.0e-9))
	{
		Actor.Air.Momentum = SimClamp(Actor.Air.Momentum - Rules.DecayPerSecondStill * SimDt(), Rules.Min, Rules.Max);
	}
}

void UpdateAirOngoing(FArenaState& State, FActor& Actor, const FVrRuleset* Rules)
{
	if (!Actor.Air.bSchool)
	{
		return;
	}
	if (Actor.Air.Volley.IsSet())
	{
		FAirVolley& Volley = Actor.Air.Volley.GetValue();
		const FAirSpell* Spell = FindAirSpell(Volley.SpellId);
		if (Spell && State.Tick >= Volley.NextTick && Volley.Left > 0)
		{
			const FHit Hit = MakeAirHit(Actor, *Spell, State.NextId++, Volley.Damage);
			FProjectile& Projectile = SpawnProjectile(State, Hit, Actor.Pos, AimDirection(Actor, Volley.Aim), Spell->SpeedMps, Spell->RangeM * Volley.RangeMult);
			Projectile.bAir = true;
			Projectile.MomentumOnHit = Hit.MomentumOnHit;
			Projectile.PierceLeft = Volley.PierceLeft;
			Volley.Left--;
			Volley.NextTick += Volley.IntervalTicks;
		}
		if (!Spell || Volley.Left <= 0)
		{
			Actor.Air.Volley.Reset();
		}
	}
	if (!Actor.Air.Channel.IsSet())
	{
		return;
	}
	FFireChannel& Channel = Actor.Air.Channel.GetValue();
	if (Channel.IntervalTicks > 0 && State.Tick >= Channel.NextTick && State.Tick <= Channel.Until)
	{
		// Each tick is its own activation and the beam follows the current facing (the Brand Wrath channel).
		if (const FAirSpell* Spell = FindAirSpell(Channel.SpellId))
		{
			const FSimVec End = SimAdd(Actor.Pos, SimScale(Actor.Facing, Channel.RangeM));
			HitAlongSegment(State, Actor, *Spell, Actor.Pos, End, State.NextId++, Channel.Damage, Rules);
		}
		Channel.TicksDone++;
		Channel.NextTick += Channel.IntervalTicks;
	}
	if (Channel.NextTick > Channel.Until)
	{
		Actor.Air.Channel.Reset();
	}
}

double AirAbsorbDrainMult(const FArenaState& State, const FActor& Actor)
{
	if (!Actor.Air.bSchool)
	{
		return 1.0;
	}
	double Mult = KernelData().Momentum.AbsorbDrainMult;
	if (State.Tick < Actor.Air.AbsorbDrainSpellUntil)
	{
		Mult *= Actor.Air.AbsorbDrainSpellMult;
	}
	return Mult;
}

double AirRollStaminaMult(const FActor& Actor)
{
	return Band<double>(Actor, &FMomentumThreshold::bRollStamina, &FMomentumThreshold::RollStaminaMult, 1.0);
}

double AirSpellRangeMult(const FActor& Actor)
{
	return Band<double>(Actor, &FMomentumThreshold::bRange, &FMomentumThreshold::SpellRangeMult, 1.0);
}

int32 AirSpellsPierce(const FActor& Actor)
{
	return Band<int32>(Actor, &FMomentumThreshold::bPierce, &FMomentumThreshold::SpellsPierce, 0);
}

bool AirCastRoots(const FActor& Actor)
{
	if (!Actor.Air.bSchool || !Actor.Pending.IsSet() || Actor.Pending->Kind != TEXT("air") || !Actor.Pending->SpellId.IsSet())
	{
		return false;
	}
	const FAirSpell* Spell = FindAirSpell(Actor.Pending->SpellId.GetValue());
	return Spell && Spell->bRootDuringCast;
}

bool AirChannelBusy(const FActor& Actor)
{
	return Actor.Air.bSchool && (Actor.Air.Channel.IsSet() || Actor.Air.Volley.IsSet());
}

void CancelAirChannel(FActor& Actor)
{
	Actor.Air.Channel.Reset();
	Actor.Air.Volley.Reset();
}

void ResetAir(FActor& Actor)
{
	const bool bSchool = Actor.Air.bSchool;
	Actor.Air = FAirState();
	Actor.Air.bSchool = bSchool;
}

bool AirTryEvade(FArenaState& State, FActor& Target, const FHit& Hit)
{
	if (!Target.Air.bSchool || State.Tick >= Target.Air.FormUntil || Hit.Family == TEXT("unblockable"))
	{
		return false;
	}
	const double Chance = Hit.Family == TEXT("physical") ? Target.Air.FormPhysical : Target.Air.FormMagic;
	// Drawn from the kernel RNG, one draw per hit that reaches the form, so a replay of the same inputs evades the same hits.
	const double Roll = ArenaRandom(State, FString::Printf(TEXT("air %d evade %d"), Target.Id, Hit.ActivationId));
	if (!(Roll < Chance))
	{
		return false;
	}
	Target.Air.Evades++;
	Emit(State, TEXT("evade"), Target, Hit.Damage, Hit.OwnerId);
	return true;
}

void AirOnHitResolved(FArenaState& State, const FHit& Hit)
{
	if (Hit.MomentumOnHit > 0.0)
	{
		if (FActor* Owner = SimFindActor(State, Hit.OwnerId))
		{
			GainMomentum(State, *Owner, Hit.MomentumOnHit);
		}
	}
}

bool AirTryDeflect(FArenaState& State, FActor& Target, const FProjectile& Projectile)
{
	if (!Target.Air.bSchool || Projectile.bReflected || Projectile.Tier > Target.Tier)
	{
		return false;
	}
	const FActor* Owner = SimFindActor(State, Projectile.OwnerId);
	const FSimVec Origin = Owner ? Owner->Pos : Projectile.Source;
	const double Speed = SimLength(Projectile.Velocity);
	const double Range = std::max(Projectile.RemainingM, SimDistance(Target.Pos, Origin) + Target.Radius);
	FHit Hit;
	Hit.OwnerId = Target.Id;
	Hit.ActivationId = State.NextId++;
	Hit.Damage = Projectile.Damage;
	Hit.Family = Projectile.Family;
	Hit.Tier = Projectile.Tier;
	Hit.Source = Target.Pos;
	Hit.bBolt = Projectile.bBolt;
	Hit.HeatOnHit = Projectile.HeatOnHit;
	Hit.bSuppressPerfect = Projectile.bSuppressPerfect;
	Hit.bPierceShields = Projectile.bPierceShields;
	// "Redirected along the aim direction": the deflector's facing, which the kernel sets from its aim every step.
	FProjectile& Deflected = SpawnProjectile(State, Hit, Target.Pos, Target.Facing, Speed, Range);
	Deflected.bReflected = true;
	Target.Air.Deflects++;
	return true;
}
