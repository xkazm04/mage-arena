#include "Kernel/Water.h"

#include "Kernel/ArenaKernel.h"
#include "Kernel/Catalog.h"
#include "Kernel/Geometry.h"

#include "Algo/Sort.h"

#include <cmath>

namespace
{
struct FFlowCast
{
	bool bCrest = false;
	double DamageMult = 1.0;
};

FFlowCast FlowCast(FActor& Actor, const FString& Line, int32 Tick)
{
	const FKernelData& Data = KernelData();
	FWaterState& Water = Actor.Water;
	const bool bCrest = Water.Flow >= Data.FlowMax;
	if (bCrest)
	{
		Water.Flow = 0;
		Water.Crests++;
	}
	else if (Line == Water.LastLine && Data.bFlowResetSameLine)
	{
		Water.Flow = 0;
	}
	else if (!Water.LastLine.IsEmpty() && Tick - Water.LastCastTick <= SimTicks(Data.FlowWithinS))
	{
		Water.Flow = static_cast<int32>(std::min(Data.FlowMax, static_cast<double>(Water.Flow) + 1.0));
	}
	Water.LastLine = Line;
	Water.LastCastTick = Tick;
	Water.LastActivityTick = Tick;
	FFlowCast Result;
	Result.bCrest = bCrest;
	Result.DamageMult = bCrest ? Data.CrestDamageMult : 1.0 + Water.Flow * Data.FlowPerStack;
	return Result;
}

void Heal(FActor& Actor, double Amount)
{
	const double Actual = std::min(Actor.MaxHp - Actor.Hp, Amount);
	Actor.Hp += Actual;
	Actor.Water.Healing += Actual;
}

void ApplyControl(FArenaState& State, FActor& Actor, FActor& Target, const FSpell& Spell)
{
	if (Target.bDown || State.Tick < Target.ImmuneUntil || State.Tick < Target.Water.EncasedUntil)
	{
		return;
	}
	if (Spell.Effect == TEXT("pull") || Spell.Effect == TEXT("push"))
	{
		const FSimVec Direction = SimUnit(SimSub(Target.Pos, Actor.Pos));
		const double Amount = Spell.Effect == TEXT("pull")
			? -std::min(Spell.Amount, std::max(0.0, SimDistance(Actor.Pos, Target.Pos) - Actor.Radius - Target.Radius))
			: Spell.Amount;
		Target.Pos = ConstrainToArena(SimAdd(Target.Pos, SimScale(Direction, Amount)), Target.Radius);
		Actor.Water.ControlTicks++;
	}
	if (Spell.Effect == TEXT("root"))
	{
		Target.Water.RootUntil = std::max(Target.Water.RootUntil, State.Tick + SimTicks(Spell.DurationS));
		Actor.Water.ControlTicks += SimTicks(Spell.DurationS);
	}
}
}

bool TrySpellCast(FArenaState& State, FActor& Actor, const FInputFrame& Input, const FSpellCastMod* Mod)
{
	const FSpell* Spell = SpellFor(Actor, Input.Slot);
	if (!Spell || Spell->Kind == TEXT("passive") || State.Tick < SimCooldownUntil(Actor.Water, Spell->Line))
	{
		return false;
	}
	const FKernelData& Data = KernelData();
	const bool bSuppressFlow = Mod && Mod->bSuppressFlow;
	const double Cost = (!bSuppressFlow && Actor.Water.Flow >= Data.FlowMax) ? Data.CrestManaCost : Spell->Mana;
	if (Actor.Mana < Cost)
	{
		return false;
	}
	TOptional<int32> TargetId;
	if (Spell->Kind == TEXT("target"))
	{
		TArray<int32> Candidates;
		for (const FActor& Target : State.Actors)
		{
			if (!Target.bDown && Target.Team != Actor.Team && SimDistance(Actor.Pos, Target.Pos) <= Spell->RangeM
				&& SimDistance(Input.Aim, Target.Pos) <= Data.TombCursorRadiusM)
			{
				Candidates.Add(Target.Id);
			}
		}
		Algo::Sort(Candidates, [&State, &Input](int32 Left, int32 Right)
		{
			const FActor* A = SimFindActor(State, Left);
			const FActor* B = SimFindActor(State, Right);
			const double Delta = SimDistance(A->Pos, Input.Aim) - SimDistance(B->Pos, Input.Aim);
			if (Delta < 0.0)
			{
				return true;
			}
			if (Delta > 0.0)
			{
				return false;
			}
			return Left < Right;
		});
		if (Candidates.Num() == 0)
		{
			return false;
		}
		TargetId = Candidates[0];
	}
	double DamageMult = 1.0;
	if (bSuppressFlow)
	{
		DamageMult = Mod->PowerMult;
	}
	else
	{
		const FFlowCast Flow = FlowCast(Actor, Spell->Line, State.Tick);
		DamageMult = Flow.DamageMult;
		if (Mod && Mod->PowerMult != 1.0)
		{
			DamageMult *= Mod->PowerMult;
		}
	}
	Actor.Mana -= Cost;
	SimSetCooldown(Actor.Water, Spell->Line, State.Tick + SimTicks(Spell->CooldownS));
	const FSimVec Direction = SimUnit(SimSub(Input.Aim, Actor.Pos), Actor.Facing);
	const double Range = std::min(SimDistance(Actor.Pos, Input.Aim), Spell->RangeM);
	FPendingCast Pending;
	Pending.Kind = TEXT("spell");
	Pending.StartTick = State.Tick;
	Pending.ReleaseTick = State.Tick + SimTicks(std::max(Spell->CastS, Spell->TelegraphS));
	Pending.Aim = (Spell->Kind == TEXT("zone") || Spell->Effect == TEXT("decoy"))
		? FSimVec{Actor.Pos.X + Direction.X * Range, Actor.Pos.Y + Direction.Y * Range}
		: Input.Aim;
	Pending.ActivationId = State.NextId++;
	Pending.SpellId = Spell->Id;
	Pending.DamageMult = DamageMult;
	Pending.TargetId = TargetId;
	Actor.Pending = Pending;
	Actor.Metrics.Casts++;
	Emit(State, TEXT("cast"), Actor, Spell->Tier);
	return true;
}

void ReleaseSpell(FArenaState& State, FActor& Actor)
{
	if (!Actor.Pending.IsSet() || Actor.Pending->Kind != TEXT("spell"))
	{
		return;
	}
	const FPendingCast Pending = Actor.Pending.GetValue();
	const FSpell* Spell = Pending.SpellId.IsSet() ? FindSpellById(Pending.SpellId.GetValue()) : nullptr;
	check(Spell);
	Actor.Pending.Reset();
	const FKernelData& Data = KernelData();
	const FSimVec Direction = SimUnit(SimSub(Pending.Aim, Actor.Pos), Actor.Facing);
	const double Damage = Spell->Damage * Pending.DamageMult.Get(1.0);
	FHit Hit;
	Hit.OwnerId = Actor.Id;
	Hit.ActivationId = Pending.ActivationId;
	Hit.Damage = Damage;
	Hit.Family = Spell->Family;
	Hit.Tier = Spell->Tier;
	Hit.Source = Actor.Pos;
	Hit.bBolt = Spell->Line == TEXT("bolt");
	if (Spell->Kind == TEXT("projectile"))
	{
		for (int32 Index = 0; Index < Spell->Count; ++Index)
		{
			const double Angle = Spell->Count > 1
				? (-Spell->SpreadDeg / 2.0 + Index * Spell->SpreadDeg / static_cast<double>(Spell->Count - 1)) * SimPi / 180.0
				: 0.0;
			const double Radius = Spell->RadiusM != 0.0 ? Spell->RadiusM : Data.ProjectileRadiusM;
			FProjectile& Projectile = SpawnProjectile(State, Hit, Actor.Pos, SimRotate(Direction, Angle), Spell->SpeedMps, Spell->RangeM, Radius);
			if (Spell->BurstRadiusM != 0.0)
			{
				Projectile.BurstRadiusM = Spell->BurstRadiusM;
			}
		}
	}
	else if (Spell->Kind == TEXT("cone") || Spell->Kind == TEXT("ring") || (Spell->Kind == TEXT("zone") && Spell->Effect == TEXT("root")))
	{
		const FSimVec Centre = Spell->Kind == TEXT("zone") ? Pending.Aim : Actor.Pos;
		const double Range = Spell->Kind == TEXT("zone") ? Spell->RadiusM : Spell->RangeM;
		for (FActor& Target : State.Actors)
		{
			if (Target.bDown || Target.Team == Actor.Team || SimDistance(Centre, Target.Pos) > Range
				|| !SimInArc(Direction, SimSub(Target.Pos, Centre), Spell->ArcDeg))
			{
				continue;
			}
			FHit Area = Hit;
			Area.Source = Centre;
			Area.Delivery = TEXT("area");
			const FSimHitResult Result = ResolveHit(State, Target, Area);
			if (!Result.bPerfect && Result.Damage > 0.0)
			{
				ApplyControl(State, Actor, Target, *Spell);
			}
		}
	}
	else if (Spell->Kind == TEXT("zone"))
	{
		FZone Zone;
		Zone.Id = Pending.ActivationId;
		Zone.OwnerId = Actor.Id;
		Zone.Pos = Pending.Aim;
		Zone.RadiusM = Spell->RadiusM;
		Zone.Until = State.Tick + SimTicks(Spell->DurationS);
		Zone.Kind = Spell->Effect;
		Zone.SlowMult = 1.0 - Spell->Amount;
		State.Zones.Add(Zone);
	}
	else if (Spell->Kind == TEXT("target"))
	{
		FActor* Target = Pending.TargetId.IsSet() ? SimFindActor(State, Pending.TargetId.GetValue()) : nullptr;
		if (Target && !Target->bDown && SimDistance(Actor.Pos, Target->Pos) <= Spell->RangeM && State.Tick >= Target->ImmuneUntil)
		{
			Target->Water.EncasedUntil = State.Tick + SimTicks(Spell->DurationS);
			Target->Water.Flow = 0;
			Target->Water.Stored = 0.0;
			Target->bAbsorb = false;
			Interrupt(State, *Target);
			Actor.Water.ControlTicks += SimTicks(Spell->DurationS);
		}
	}
	else if (Spell->Kind == TEXT("wave"))
	{
		const double Stored = std::min(Actor.Water.Stored, Spell->Amount);
		Actor.Water.Stored = 0.0;
		FHit Wave = Hit;
		Wave.Damage = Stored * Pending.DamageMult.Get(1.0);
		FProjectile& Projectile = SpawnProjectile(State, Wave, Actor.Pos, Direction, Data.ReturnWaveSpeedMps, Spell->RangeM, Data.ReturnWaveRadiusM);
		Projectile.bPiercing = true;
	}
	else
	{
		if (Spell->Effect == TEXT("heal"))
		{
			Heal(Actor, Spell->Amount);
		}
		if (Spell->Effect == TEXT("hot"))
		{
			Actor.Water.HotUntil = State.Tick + SimTicks(Spell->DurationS);
			Actor.Water.HotPerTick = Spell->Amount / static_cast<double>(SimTicks(Spell->DurationS));
		}
		if (Spell->Effect == TEXT("ward"))
		{
			Actor.Water.WardUntil = State.Tick + SimTicks(Spell->DurationS);
		}
		if (Spell->Effect == TEXT("font"))
		{
			Actor.Mana = std::min(Actor.MaxMana, Actor.Mana + Spell->Amount);
		}
		if (Spell->Effect == TEXT("sheen"))
		{
			Actor.Water.SheenUntil = State.Tick + SimTicks(Spell->DurationS);
		}
		if (Spell->Effect == TEXT("decoy"))
		{
			Actor.Water.Decoy = FDecoy{Pending.Aim, State.Tick + SimTicks(Spell->DurationS)};
		}
	}
}

void UpdateWater(FArenaState& State, FActor& Actor)
{
	const FKernelData& Data = KernelData();
	FWaterState& Water = Actor.Water;
	if (State.Tick - Water.LastActivityTick >= SimTicks(Data.FlowIdleS))
	{
		Water.Flow = 0;
	}
	if (State.Tick <= Water.HotUntil)
	{
		Heal(Actor, Water.HotPerTick);
	}
	if (Water.Decoy.IsSet() && Water.Decoy->Until <= State.Tick)
	{
		Water.Decoy.Reset();
	}
	for (const FZone& Zone : State.Zones)
	{
		const FActor* Owner = SimFindActor(State, Zone.OwnerId);
		if (Zone.Kind != TEXT("slow") || (Owner && Owner->Team == Actor.Team) || Zone.Until <= State.Tick || SimDistance(Zone.Pos, Actor.Pos) > Zone.RadiusM)
		{
			continue;
		}
		Water.SlowUntil = State.Tick + 1;
		Water.SlowMult = Zone.SlowMult;
		if (Owner)
		{
			// Owner is const from the find on a non-const state. Control ticks live on the live actor.
			if (FActor* Live = SimFindActor(State, Owner->Id))
			{
				Live->Water.ControlTicks++;
			}
		}
	}
}

double WardDrainMult(const FArenaState& State, const FActor& Actor)
{
	const FSpell* Ward = FindSpellByEffect(TEXT("ward"));
	check(Ward);
	return State.Tick < Actor.Water.WardUntil ? 1.0 - Ward->Amount : 1.0;
}

double RefundMult(const FArenaState& State, const FActor& Actor)
{
	const FSpell* Sheen = FindSpellByEffect(TEXT("sheen"));
	check(Sheen);
	return State.Tick < Actor.Water.SheenUntil ? 1.0 + Sheen->Amount : 1.0;
}

void WaterAbsorbed(FArenaState& State, FActor& Actor, const FHit& Hit, double Prevented, bool bPerfect)
{
	const FKernelData& Data = KernelData();
	if (SimHasLine(Actor.Water.Composition, TEXT("mirror")))
	{
		const FSpell* Source = FindSpellByEffect(TEXT("return"));
		check(Source);
		Actor.Water.Stored = std::min(Source->Amount, Actor.Water.Stored + Prevented * Data.StoredFraction);
	}
	if (!bPerfect)
	{
		return;
	}
	Actor.Water.Flow = static_cast<int32>(std::min(Data.FlowMax, static_cast<double>(Actor.Water.Flow) + Data.PerfectAbsorbGives));
	Actor.Water.LastActivityTick = State.Tick;
	if (Hit.bReaction || !SimHasLine(Actor.Water.Composition, TEXT("mirror")) || Actor.Tier < 2 || Actor.Water.Composition.Branches.Mirror != TEXT("B"))
	{
		return;
	}
	const FSpell* Ripple = FindSpellByEffect(TEXT("ripple"));
	check(Ripple);
	const int32 ActivationId = State.NextId++;
	for (FActor& Target : State.Actors)
	{
		if (!Target.bDown && Target.Team != Actor.Team && SimDistance(Actor.Pos, Target.Pos) <= Ripple->RangeM)
		{
			FHit RippleHit;
			RippleHit.OwnerId = Actor.Id;
			RippleHit.ActivationId = ActivationId;
			RippleHit.Damage = Ripple->Damage;
			RippleHit.Family = TEXT("magic");
			RippleHit.Tier = Ripple->Tier;
			RippleHit.Source = Actor.Pos;
			RippleHit.bReaction = true;
			RippleHit.Delivery = TEXT("area");
			ResolveHit(State, Target, RippleHit);
		}
	}
}

bool ReflectProjectile(FArenaState& State, FActor& Target, const FProjectile& Projectile)
{
	if (Projectile.bReflected || Projectile.Family != TEXT("magic") || Projectile.Tier > Target.Tier || Target.Tier < 2
		|| !SimHasLine(Target.Water.Composition, TEXT("mirror")) || Target.Water.Composition.Branches.Mirror != TEXT("A"))
	{
		return false;
	}
	const FActor* Owner = SimFindActor(State, Projectile.OwnerId);
	const FSimVec Origin = Owner ? Owner->Pos : Projectile.Source;
	const double Burst = Projectile.BurstRadiusM;
	const bool bPiercing = Projectile.bPiercing;
	const double Speed = SimLength(Projectile.Velocity);
	const double Range = std::max(Projectile.RemainingM, SimDistance(Target.Pos, Origin) + Target.Radius);
	const FSimVec Direction = SimSub(Origin, Target.Pos);
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
	FProjectile& Reflected = SpawnProjectile(State, Hit, Target.Pos, Direction, Speed, Range);
	Reflected.bReflected = true;
	Reflected.BurstRadiusM = Burst;
	Reflected.bPiercing = bPiercing;
	return true;
}

bool HasLineOfSight(const FArenaState& State, const FSimVec& A, const FSimVec& B)
{
	for (const FZone& Zone : State.Zones)
	{
		if (Zone.Kind == TEXT("fog") && Zone.Until > State.Tick && SimSegmentHit(A, B, Zone.Pos, Zone.RadiusM).IsSet())
		{
			return false;
		}
	}
	return true;
}

void ResetWater(FActor& Actor)
{
	const FComposition Kept = Actor.Water.Composition;
	Actor.Water = NewWaterState(&Kept);
}
