#include "Kernel/ArenaThreats.h"

#include "MageArenaVR.h"
#include "Kernel/Air.h"
#include "Kernel/Enemies.h"
#include "Kernel/Fire.h"
#include "Kernel/Geometry.h"
#include "Kernel/VrRules.h"
#include "Kernel/Water.h"

#include <limits>

namespace
{
bool HitListContains(const TArray<int32>& Ids, int32 Id)
{
	return Ids.Contains(Id);
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

// T22 collar events. They exist on the ruleset path only: conformance runs without a ruleset and its pinned event logs
// were recorded without them, so the null path stays byte-identical. Team 0 is the player's side (the slice seats one
// actor there).
bool IsUnblockableThreatTo(const FActor& Target, const FActor* Owner, const FString& Family)
{
	return Family == TEXT("unblockable") && Target.Team == 0 && !Target.bDown && (!Owner || Owner->Team != 0);
}

// A team-0 projectile that has stopped (RemainingM at or below the epsilon) ends its activation when no other
// projectile of that activation is still flying or stops later in the list. The activation is a miss when none of its
// hits damaged anyone. An opposing unblockable that stops without touching a team-0 actor was left: a dodge.
void CollarProjectileDone(FArenaState& State, FVrRuleset& Rules, int32 DoneIndex)
{
	const FProjectile Done = State.Projectiles[DoneIndex];
	const FActor* Owner = SimFindActor(State, Done.OwnerId);
	if (Done.bReflected)
	{
		Rules.ReflectedCasters.Remove(Done.Id);
	}
	else if (Owner && Owner->Team == 0)
	{
		bool bLast = true;
		for (int32 Index = 0; Index < State.Projectiles.Num(); ++Index)
		{
			const FProjectile& Other = State.Projectiles[Index];
			if (Index != DoneIndex && Other.ActivationId == Done.ActivationId && Other.OwnerId == Done.OwnerId
				&& (Other.RemainingM > ProjectileRemainEpsilon || Index > DoneIndex))
			{
				bLast = false;
				break;
			}
		}
		if (bLast)
		{
			if (!Rules.CollarDamaged.Contains(Done.ActivationId))
			{
				Emit(State, TEXT("miss"), *Owner, static_cast<double>(Done.Tier));
			}
			Rules.CollarDamaged.Remove(Done.ActivationId);
		}
	}
	if (Done.Family == TEXT("unblockable") && (!Owner || Owner->Team != 0))
	{
		for (const FActor& Actor : State.Actors)
		{
			if (IsUnblockableThreatTo(Actor, Owner, Done.Family) && !Done.HitIds.Contains(Actor.Id))
			{
				Emit(State, TEXT("dodge"), Actor, static_cast<double>(Done.Tier), Done.OwnerId);
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
			auto InShape = [&Telegraph, &Direction, &End](const FSimVec& Pos, double Radius)
			{
				if (Telegraph.Kind == TEXT("area"))
				{
					return SimDistance(Telegraph.Target, Pos) <= Telegraph.WidthM + Radius;
				}
				if (Telegraph.Kind == TEXT("melee"))
				{
					return SimDistance(Telegraph.Origin, Pos) <= Telegraph.RangeM + Radius
						&& SimInArc(Direction, SimSub(Pos, Telegraph.Origin), Telegraph.WidthM);
				}
				return SimSegmentHit(Telegraph.Origin, End, Pos, Telegraph.WidthM * 0.5 + Radius).IsSet();
			};
			bool bDamagedAny = false;
			for (FActor& Target : State.Actors)
			{
				if (Target.Team == Owner->Team || Target.bDown)
				{
					continue;
				}
				const bool bInShape = InShape(Target.Pos, Target.Radius);
				// T22 dodge: an unblockable that resolves on a blinking target, or off a target it was aimed at (its
				// aim point is inside its own shape, so staying put would have been a hit).
				if (Rules && IsUnblockableThreatTo(Target, Owner, Telegraph.Family)
					&& (bInShape ? State.Tick < Target.ImmuneUntil : InShape(Telegraph.Target, Target.Radius)))
				{
					Emit(State, TEXT("dodge"), Target, static_cast<double>(Telegraph.Tier), Telegraph.OwnerId);
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
				bDamagedAny |= Result.Damage > 0.0;
				if (!bImmune && !Result.bPerfect && !Result.bEvaded && !Target.bDown)
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
			// T22 miss: a team-0 area, lane or melee that could damage and damaged nobody.
			if (Rules && Owner->Team == 0 && Telegraph.Damage > 0.0 && !bDamagedAny)
			{
				Emit(State, TEXT("miss"), *Owner, static_cast<double>(Telegraph.Tier));
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
		FSimVec End = SimAdd(Projectile.Pos, SimScale(Direction, Travel));
		// T23 Veering Bolt: a curved flight's end is on its arc. The commit below applies the new angle and velocity
		// wherever the projectile moves on; every other projectile keeps the straight step above.
		double ArcAngle = Projectile.ArcAngle;
		FSimVec ArcVelocity = Projectile.Velocity;
		double ArcLeft = Projectile.ArcLeftM;
		if (Projectile.bCurved)
		{
			End = AirCurvedStep(Projectile, Travel, ArcAngle, ArcVelocity, ArcLeft);
		}
		auto MoveOn = [&](FProjectile& Moving)
		{
			Moving.Pos = End;
			Moving.RemainingM -= Travel;
			if (Moving.bCurved)
			{
				Moving.ArcAngle = ArcAngle;
				Moving.Velocity = ArcVelocity;
				Moving.ArcLeftM = ArcLeft;
			}
		};
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
			Hit.MomentumOnHit = Projectile.MomentumOnHit;
			// T22 dodge: an opposing unblockable reaching a team-0 actor in blink i-frames.
			const bool bBlinkedThrough = Rules && IsUnblockableThreatTo(*First, Owner, Projectile.Family) && State.Tick < First->ImmuneUntil;
			const FSimHitResult Result = ResolveHit(State, *First, Hit, Rules);
			Projectile.HitIds.Add(First->Id);
			if (bBlinkedThrough)
			{
				Emit(State, TEXT("dodge"), *First, static_cast<double>(Projectile.Tier), Projectile.OwnerId);
			}
			if (Rules && Result.Damage > 0.0)
			{
				if (Owner && Owner->Team == 0 && !Projectile.bReflected)
				{
					Rules->CollarDamaged.Add(Projectile.ActivationId);
				}
				// T22: a reflected projectile that damages the caster it came from.
				const int32* From = Projectile.bReflected ? Rules->ReflectedCasters.Find(Projectile.Id) : nullptr;
				if (From && *From == First->Id && Owner)
				{
					Emit(State, TEXT("reflectHit"), *Owner, Result.Damage, First->Id);
				}
			}
			if (Result.bEvaded)
			{
				// Air Form: the bolt passes through the evading body and flies on. It cannot hit that body again.
				MoveOn(Projectile);
				continue;
			}
			if (Result.bPerfect)
			{
				// Read before the spawn: the reserve above keeps Projectile valid, but the owner id is what the event names.
				const int32 Caster = Projectile.OwnerId;
				const int32 Tier = Projectile.Tier;
				// The reflect event is a VR overlay event (T20): conformance runs without a ruleset and its pinned event
				// logs (mirror-2a) were recorded without it, so the null path stays byte-identical.
				if (First->Air.bSchool)
				{
					// T23: an air actor's perfect is the school's deflect (schools.json air perfectAbsorbExtra), not the
					// water mirror its attached water preset might name. No conformance vector has an air actor, so the
					// event needs no ruleset gate.
					if (AirTryDeflect(State, *First, Projectile))
					{
						Emit(State, TEXT("deflect"), *First, static_cast<double>(Tier), Caster);
					}
				}
				else if (ReflectProjectile(State, *First, Projectile) && Rules)
				{
					Emit(State, TEXT("reflect"), *First, static_cast<double>(Tier), Caster);
					Rules->ReflectedCasters.Add(State.Projectiles.Last().Id, Caster);
				}
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
						const FSimHitResult BurstResult = ResolveHit(State, Target, Burst, Rules);
						Projectile.HitIds.Add(Target.Id);
						if (Rules && BurstResult.Damage > 0.0 && Owner && Owner->Team == 0 && !Projectile.bReflected)
						{
							Rules->CollarDamaged.Add(Projectile.ActivationId);
						}
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
			else if (!Projectile.bPiercing && !Result.bPerfect && Projectile.PierceLeft > 0)
			{
				// T23 spellsPierce, read as "pierces one extra target": the bolt that connected flies on for one more body.
				Projectile.PierceLeft--;
				MoveOn(Projectile);
			}
			else if (!Projectile.bPiercing || Result.bPerfect)
			{
				Projectile.RemainingM = 0.0;
			}
			else
			{
				MoveOn(Projectile);
			}
		}
		else
		{
			MoveOn(Projectile);
		}
	}
	if (Rules)
	{
		for (int32 Index = 0; Index < State.Projectiles.Num(); ++Index)
		{
			if (!(State.Projectiles[Index].RemainingM > ProjectileRemainEpsilon))
			{
				CollarProjectileDone(State, *Rules, Index);
			}
		}
	}
	State.Projectiles.RemoveAll([](const FProjectile& Projectile) { return !(Projectile.RemainingM > ProjectileRemainEpsilon); });
}

FSimHitResult ResolveHit(FArenaState& State, FActor& Target, const FHit& Hit, const FVrRuleset* Rules)
{
	FSimHitResult Result;
	if (Target.bDown || State.Tick < Target.ImmuneUntil || State.Tick < Target.Water.EncasedUntil)
	{
		return Result;
	}
	// T23 Air Form. Before the ward: an evaded hit does not touch the ward, the HP or the metrics.
	if (AirTryEvade(State, Target, Hit))
	{
		Result.bEvaded = true;
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
	AirOnHitResolved(State, Hit);
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
