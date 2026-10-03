#include "Kernel/MageAI.h"

#include "Kernel/ArenaKernel.h"
#include "Kernel/Catalog.h"
#include "Kernel/Fire.h"
#include "Kernel/Geometry.h"
#include "Kernel/VrRules.h"
#include "Kernel/Water.h"

#include <algorithm>
#include <cmath>

namespace
{
struct FThreat
{
	int32 Id = 0;
	FString Family;
	FSimVec Origin;
	int32 ImpactTick = 0;
	bool bAimed = false;
};

struct FChoice
{
	int32 Slot = 0;
	const FSpell* Spell = nullptr;
};

const FActor* ActorById(const FArenaState& State, int32 Id)
{
	return SimFindActor(State, Id);
}

bool SameTeam(const FArenaState& State, int32 OwnerId, int32 Team)
{
	const FActor* Owner = ActorById(State, OwnerId);
	return Owner && Owner->Team == Team;
}

TArray<FThreat> CollectThreats(const FArenaState& State, const FActor& Self)
{
	const FKernelData& Data = KernelData();
	TArray<FThreat> Result;
	for (const FProjectile& Projectile : State.Projectiles)
	{
		if (SameTeam(State, Projectile.OwnerId, Self.Team) || !HasLineOfSight(State, Self.Pos, Projectile.Pos))
		{
			continue;
		}
		const double Speed = SimLength(Projectile.Velocity);
		const FSimVec Direction = SimUnit(Projectile.Velocity);
		const FSimVec End = SimAdd(Projectile.Pos, SimScale(Direction, Projectile.RemainingM));
		int32 Travel = 1;
		if (Speed > 0.0)
		{
			const double Seconds = (SimDistance(Projectile.Pos, Self.Pos) - Projectile.Radius - Self.Radius) / Speed;
			Travel = std::max(1, SimTicks(Seconds));
		}
		FThreat Threat;
		Threat.Id = Projectile.ActivationId;
		Threat.Family = Projectile.Family;
		Threat.Origin = Projectile.Pos;
		Threat.ImpactTick = State.Tick + Travel;
		Threat.bAimed = SimSegmentHit(Projectile.Pos, End, Self.Pos, Self.Radius + Projectile.Radius).IsSet();
		Result.Add(Threat);
	}
	for (const FTelegraph& Telegraph : State.Telegraphs)
	{
		if (SameTeam(State, Telegraph.OwnerId, Self.Team) || !HasLineOfSight(State, Self.Pos, Telegraph.Origin))
		{
			continue;
		}
		const FSimVec Toward = SimSub(Telegraph.Target, Telegraph.Origin);
		const FSimVec Direction = SimUnit(Toward);
		const FSimVec End = Telegraph.Kind == TEXT("area") ? Telegraph.Target : SimAdd(Telegraph.Origin, SimScale(Direction, Telegraph.RangeM));
		bool bAimed = false;
		if (Telegraph.Kind == TEXT("area"))
		{
			bAimed = SimDistance(Telegraph.Target, Self.Pos) < Telegraph.WidthM + Self.Radius;
		}
		else if (Telegraph.Kind == TEXT("melee"))
		{
			bAimed = SimDistance(Telegraph.Origin, Self.Pos) < Telegraph.RangeM + Self.Radius
				&& SimInArc(Toward, SimSub(Self.Pos, Telegraph.Origin), Telegraph.WidthM);
		}
		else
		{
			const double Radius = Self.Radius + (Telegraph.Kind == TEXT("projectile") ? Data.ProjectileRadiusM : Telegraph.WidthM * 0.5);
			bAimed = SimSegmentHit(Telegraph.Origin, End, Self.Pos, Radius).IsSet();
		}
		int32 Impact = Telegraph.ResolveTick;
		if (Telegraph.Kind == TEXT("projectile") && Telegraph.SpeedMps > 0.0)
		{
			Impact += SimTicks(SimDistance(Telegraph.Origin, Self.Pos) / Telegraph.SpeedMps);
		}
		FThreat Threat;
		Threat.Id = Telegraph.ActivationId;
		Threat.Family = Telegraph.Family;
		Threat.Origin = Telegraph.Origin;
		Threat.ImpactTick = Impact;
		Threat.bAimed = bAimed;
		Result.Add(Threat);
	}
	for (const FActor& Enemy : State.Actors)
	{
		if (Enemy.Team == Self.Team || Enemy.bDown || !Enemy.Pending.IsSet() || !HasLineOfSight(State, Self.Pos, Enemy.Pos))
		{
			continue;
		}
		const FPendingCast& Pending = Enemy.Pending.GetValue();
		const FSpell* Spell = Pending.SpellId.IsSet() ? FindSpellById(Pending.SpellId.GetValue()) : nullptr;
		// A dash is mobility. It is not a blow the defender has to answer.
		if (Spell && (Spell->Kind == TEXT("self") || Spell->Kind == TEXT("passive") || Spell->Kind == TEXT("dash")))
		{
			continue;
		}
		const bool bPlaced = Spell && (Spell->Kind == TEXT("zone") || Spell->Kind == TEXT("target"));
		const FSimVec Centre = bPlaced ? Pending.Aim : Enemy.Pos;
		bool bAimed = true;
		if (Spell && Spell->Kind == TEXT("zone"))
		{
			bAimed = SimDistance(Centre, Self.Pos) <= Spell->RadiusM + Self.Radius;
		}
		else if (Spell && Spell->Kind == TEXT("target"))
		{
			bAimed = Pending.TargetId.IsSet() && Pending.TargetId.GetValue() == Self.Id;
		}
		else if (Spell && (Spell->Kind == TEXT("cone") || Spell->Kind == TEXT("ring")))
		{
			bAimed = SimDistance(Centre, Self.Pos) <= Spell->RangeM + Self.Radius
				&& SimInArc(SimSub(Pending.Aim, Enemy.Pos), SimSub(Self.Pos, Centre), Spell->ArcDeg);
		}
		else if (Spell && (Spell->Kind == TEXT("line") || Spell->Kind == TEXT("beam")))
		{
			const FSimVec Beam = SimUnit(SimSub(Pending.Aim, Enemy.Pos));
			const FSimVec End = SimAdd(Enemy.Pos, SimScale(Beam, Spell->RangeM));
			bAimed = SimSegmentHit(Enemy.Pos, End, Self.Pos, Self.Radius).IsSet();
		}
		int32 Impact = Pending.ReleaseTick;
		if (Spell && Spell->Kind == TEXT("projectile") && Spell->SpeedMps > 0.0)
		{
			Impact += SimTicks(SimDistance(Enemy.Pos, Self.Pos) / Spell->SpeedMps);
		}
		FThreat Threat;
		Threat.Id = Pending.ActivationId;
		Threat.Family = Pending.Kind == TEXT("staff") ? TEXT("physical") : (Spell ? Spell->Family : TEXT("magic"));
		Threat.Origin = Centre;
		Threat.ImpactTick = Impact;
		Threat.bAimed = bAimed;
		Result.Add(Threat);
	}
	for (const FActor& Enemy : State.Actors)
	{
		if (Enemy.Team == Self.Team || Enemy.bDown || !Enemy.Fire.Channel.IsSet() || !HasLineOfSight(State, Self.Pos, Enemy.Pos))
		{
			continue;
		}
		const FFireChannel& Channel = Enemy.Fire.Channel.GetValue();
		const FSimVec End = SimAdd(Enemy.Pos, SimScale(Enemy.Facing, Channel.RangeM));
		FThreat Threat;
		Threat.Id = -Enemy.Id;
		Threat.Family = Channel.Family;
		Threat.Origin = Enemy.Pos;
		Threat.ImpactTick = Channel.NextTick;
		Threat.bAimed = SimSegmentHit(Enemy.Pos, End, Self.Pos, Self.Radius).IsSet();
		Result.Add(Threat);
	}
	return Result;
}

int32 NearestTargetId(const FArenaState& State, const FActor& Self)
{
	TArray<int32> Ids;
	for (const FActor& Other : State.Actors)
	{
		if (Other.Team != Self.Team && !Other.bDown && HasLineOfSight(State, Self.Pos, Other.Pos))
		{
			Ids.Add(Other.Id);
		}
	}
	if (Ids.Num() == 0)
	{
		return INDEX_NONE;
	}
	std::stable_sort(Ids.GetData(), Ids.GetData() + Ids.Num(), [&](int32 Left, int32 Right)
	{
		const double LeftDistance = SimDistance(Self.Pos, ActorById(State, Left)->Pos);
		const double RightDistance = SimDistance(Self.Pos, ActorById(State, Right)->Pos);
		if (LeftDistance < RightDistance)
		{
			return true;
		}
		if (LeftDistance > RightDistance)
		{
			return false;
		}
		return Left < Right;
	});
	return Ids[0];
}

bool Useful(const FSpell& Spell, const FActor& Self, const TArray<FThreat>& Seen)
{
	if (Spell.Damage > 0.0)
	{
		return true;
	}
	if ((Spell.Effect == TEXT("heal") || Spell.Effect == TEXT("hot")) && Self.Hp < Self.MaxHp)
	{
		return true;
	}
	if (Spell.Effect == TEXT("font") && Self.Mana < Self.MaxMana)
	{
		return true;
	}
	if (Spell.Effect == TEXT("ward"))
	{
		for (const FThreat& Threat : Seen)
		{
			if (Threat.Family == TEXT("magic"))
			{
				return true;
			}
		}
	}
	return Spell.Kind == TEXT("zone") || Spell.Effect == TEXT("decoy") || Spell.Effect == TEXT("sheen") || Spell.Effect == TEXT("encase");
}

struct FFireChoice
{
	int32 Slot = 0;
	const FFireSpell* Spell = nullptr;
};

bool FireOfferable(const FArenaState& State, const FActor& Self, const FFireSpell& Spell, double Distance)
{
	if (Spell.Tier > Self.Tier)
	{
		return false;
	}
	if (FireCooldownUntil(Self, Spell.Id) > State.Tick + 1)
	{
		return false;
	}
	if (Spell.Mana > Self.Mana)
	{
		return false;
	}
	// Cinder Step is a dash, not a ranged attack, so distance does not bar it. Self spells have no reach.
	if (Spell.Kind != TEXT("dash") && Spell.Kind != TEXT("self") && Spell.RangeM > 0.0 && Distance > Spell.RangeM)
	{
		return false;
	}
	return true;
}

bool FireUseful(const FFireSpell& Spell)
{
	return Spell.Damage > 0.0 || Spell.Kind == TEXT("zone");
}

void ChooseFireSpell(FArenaState& State, FActor& Self, FMageBrain& Brain, const FMageProfile& Profile, const FActor& Target, double Distance)
{
	const TArray<FFireSpell>& Spells = KernelData().FireSpells;
	int32 SunSlot = INDEX_NONE;
	for (int32 Slot = 0; Slot < Spells.Num(); ++Slot)
	{
		if (Spells[Slot].Id == TEXT("fire_sunfall"))
		{
			SunSlot = Slot;
		}
	}
	const bool bOpening = Self.Tier >= 4 && !Self.Fire.bSunfallLoosed && Spells.IsValidIndex(SunSlot);
	const FFireSpell* Sun = bOpening ? &Spells[SunSlot] : nullptr;
	// Session beat, on top of the competence curve: the first Sunfall is the cast and does not draw the spell random.
	// Marked for the owner in docs/schools/FIRE.md. An interrupt leaves the flag clear, so the next decision tries again.
	if (Sun && FireOfferable(State, Self, *Sun, Distance))
	{
		Brain.Input.Slot = SunSlot;
		Brain.Input.bCast = true;
		return;
	}
	// Short of Sunfall's mana, only a spell that leaves that cost behind may be cast. Otherwise withhold and regen.
	const bool bBank = Sun && Self.Mana < Sun->Mana;
	TArray<FFireChoice> Available;
	for (int32 Slot = 0; Slot < Spells.Num(); ++Slot)
	{
		const FFireSpell& Spell = Spells[Slot];
		if (!FireOfferable(State, Self, Spell, Distance))
		{
			continue;
		}
		if (bBank && Self.Mana - Spell.Mana < Sun->Mana)
		{
			continue;
		}
		Available.Add({Slot, &Spell});
	}
	if (Available.Num() == 0)
	{
		return;
	}
	TArray<FFireChoice> UsefulChoices;
	for (const FFireChoice& Choice : Available)
	{
		if (FireUseful(*Choice.Spell))
		{
			UsefulChoices.Add(Choice);
		}
	}
	const TArray<FFireChoice>& Choices = UsefulChoices.Num() > 0 ? UsefulChoices : Available;
	const FFireChoice* Picked = &Choices[0];
	if (Profile.Level < 2.0)
	{
		const double Roll = ArenaRandom(State, FString::Printf(TEXT("mage %d spell"), Self.Id));
		const int32 Index = static_cast<int32>(std::floor(Roll * static_cast<double>(Choices.Num())));
		Picked = &Choices[Index];
	}
	else
	{
		TArray<FFireChoice> Ordered = Choices;
		std::stable_sort(Ordered.GetData(), Ordered.GetData() + Ordered.Num(), [&](const FFireChoice& Left, const FFireChoice& Right)
		{
			const bool bCounterLeft = Profile.Level >= 3.0 && Target.bAbsorb && Left.Spell->Family == TEXT("unblockable");
			const bool bCounterRight = Profile.Level >= 3.0 && Target.bAbsorb && Right.Spell->Family == TEXT("unblockable");
			if (bCounterLeft != bCounterRight)
			{
				return bCounterLeft;
			}
			const double LeftValue = Left.Spell->Damage;
			const double RightValue = Right.Spell->Damage;
			if (LeftValue != RightValue)
			{
				return LeftValue > RightValue;
			}
			return Left.Slot < Right.Slot;
		});
		Picked = &Ordered[0];
		Brain.Input.Slot = Picked->Slot;
		Brain.Input.bCast = true;
		return;
	}
	Brain.Input.Slot = Picked->Slot;
	Brain.Input.bCast = true;
}

void ChooseSpell(FArenaState& State, FActor& Self, FMageBrain& Brain, const FMageProfile& Profile, const FActor& Target, double Distance, const TArray<FThreat>& Seen)
{
	if (Self.Fire.bSchool)
	{
		ChooseFireSpell(State, Self, Brain, Profile, Target, Distance);
		return;
	}
	const FKernelData& Data = KernelData();
	TArray<FChoice> Available;
	for (int32 Slot = 0; Slot <= Data.LineSlots; ++Slot)
	{
		const FSpell* Spell = SpellFor(Self, Slot);
		if (!Spell || Spell->Kind == TEXT("passive"))
		{
			continue;
		}
		if (SimCooldownUntil(Self.Water, Spell->Line) > State.Tick + 1)
		{
			continue;
		}
		if (!(Self.Water.Flow >= Data.FlowMax || Spell->Mana <= Self.Mana))
		{
			continue;
		}
		if (Spell->RangeM > 0.0 && Distance > Spell->RangeM)
		{
			continue;
		}
		Available.Add({Slot, Spell});
	}
	TArray<FChoice> UsefulChoices;
	for (const FChoice& Choice : Available)
	{
		if (Useful(*Choice.Spell, Self, Seen))
		{
			UsefulChoices.Add(Choice);
		}
	}
	const TArray<FChoice>& Choices = UsefulChoices.Num() > 0 ? UsefulChoices : Available;
	if (Choices.Num() == 0)
	{
		return;
	}
	const FChoice* Picked = &Choices[0];
	if (Profile.Level < 2.0)
	{
		const double Roll = ArenaRandom(State, FString::Printf(TEXT("mage %d spell"), Self.Id));
		const int32 Index = static_cast<int32>(std::floor(Roll * static_cast<double>(Choices.Num())));
		Picked = &Choices[Index];
	}
	else
	{
		TArray<FChoice> Ordered = Choices;
		std::stable_sort(Ordered.GetData(), Ordered.GetData() + Ordered.Num(), [&](const FChoice& Left, const FChoice& Right)
		{
			const bool bCounterLeft = Profile.Level >= 3.0 && Target.bAbsorb && Left.Spell->Family == TEXT("unblockable");
			const bool bCounterRight = Profile.Level >= 3.0 && Target.bAbsorb && Right.Spell->Family == TEXT("unblockable");
			if (bCounterLeft != bCounterRight)
			{
				return bCounterLeft;
			}
			const bool bSustainLeft = (Left.Spell->Effect == TEXT("heal") || Left.Spell->Effect == TEXT("hot")) && Self.Hp < Self.MaxHp * 0.5;
			const bool bSustainRight = (Right.Spell->Effect == TEXT("heal") || Right.Spell->Effect == TEXT("hot")) && Self.Hp < Self.MaxHp * 0.5;
			if (bSustainLeft != bSustainRight)
			{
				return bSustainLeft;
			}
			const bool bFreshLeft = Left.Spell->Line != Self.Water.LastLine;
			const bool bFreshRight = Right.Spell->Line != Self.Water.LastLine;
			if (bFreshLeft != bFreshRight)
			{
				return bFreshLeft;
			}
			const double LeftValue = Left.Spell->Damage * static_cast<double>(Left.Spell->Count);
			const double RightValue = Right.Spell->Damage * static_cast<double>(Right.Spell->Count);
			if (LeftValue != RightValue)
			{
				return LeftValue > RightValue;
			}
			return Left.Slot < Right.Slot;
		});
		Picked = &Ordered[0];
		Brain.Input.Slot = Picked->Slot;
		Brain.Input.bCast = true;
		return;
	}
	Brain.Input.Slot = Picked->Slot;
	Brain.Input.bCast = true;
}

void React(FArenaState& State, FActor& Self, FMageBrain& Brain, const FMageProfile& Profile, const TArray<FThreat>& Seen)
{
	const FKernelData& Data = KernelData();
	for (const FThreat& Threat : Seen)
	{
		FMageObservation* Memory = nullptr;
		for (FMageObservation& Row : Brain.Observed)
		{
			if (Row.Id == Threat.Id)
			{
				Memory = &Row;
				break;
			}
		}
		if (!Memory)
		{
			FMageObservation Created;
			Created.Id = Threat.Id;
			Created.FirstSeenTick = State.Tick;
			Brain.Observed.Add(Created);
			Memory = &Brain.Observed.Last();
		}
		if (Memory->bReacted || !Threat.bAimed || State.Tick - Memory->FirstSeenTick < SimTicks(Profile.ReactionDelayS))
		{
			continue;
		}
		Memory->bReacted = true;
		Brain.ReactionAges.Add(State.Tick - Memory->FirstSeenTick);
		if (Threat.Family == TEXT("magic"))
		{
			if (ArenaRandom(State, FString::Printf(TEXT("mage %d absorb %d"), Self.Id, Threat.Id)) < Profile.AbsorbChance)
			{
				Brain.TargetPoint = Threat.Origin;
				const bool bPerfect = ArenaRandom(State, FString::Printf(TEXT("mage %d perfect %d"), Self.Id, Threat.Id)) < Profile.PerfectChance;
				const int32 HalfWindow = SimTicks(Data.Absorb.WindowS) / 2;
				Brain.PlannedRaiseTick = bPerfect ? std::max(State.Tick + 1, Threat.ImpactTick - HalfWindow) : State.Tick + 1;
				Brain.PlannedReleaseTick = Threat.ImpactTick + 1;
				Brain.DefendUntil = Brain.PlannedReleaseTick;
			}
		}
		else if (Self.Stamina >= Data.RollStaminaCost)
		{
			Brain.Input.bRoll = true;
			const FSimVec Away = SimUnit(SimSub(Self.Pos, Threat.Origin));
			Brain.Input.Move = FSimVec{-Away.Y, Away.X};
			Brain.DefendUntil = State.Tick + SimTicks(Data.MageDefenceHoldS);
		}
	}
	TArray<FMageObservation> Kept;
	for (const FMageObservation& Row : Brain.Observed)
	{
		for (const FThreat& Threat : Seen)
		{
			if (Threat.Id == Row.Id)
			{
				Kept.Add(Row);
				break;
			}
		}
	}
	Brain.Observed = MoveTemp(Kept);
}

bool HostileProjectileCrosses(const FArenaState& State, const FActor& Self, const FSimVec& Facing, const FVrFireWall& Spec)
{
	FVrWall Ghost;
	Ghost.Centre = Self.Pos;
	Ghost.Facing = Facing;
	Ghost.DistanceM = Spec.DistanceM;
	Ghost.ArcDeg = Spec.ArcDeg;
	for (const FProjectile& Projectile : State.Projectiles)
	{
		if (Projectile.OwnerId == Self.Id || Projectile.Family == TEXT("unblockable"))
		{
			continue;
		}
		if (Projectile.Family != TEXT("magic") && Projectile.Family != TEXT("physical"))
		{
			continue;
		}
		const FActor* Owner = ActorById(State, Projectile.OwnerId);
		if (Owner && Owner->Team == Self.Team)
		{
			continue;
		}
		if (!(Projectile.RemainingM > 0.0) || !(SimLength(Projectile.Velocity) > 0.0))
		{
			continue;
		}
		const FSimVec Direction = SimUnit(Projectile.Velocity);
		const FSimVec End = SimAdd(Projectile.Pos, SimScale(Direction, Projectile.RemainingM));
		double CrossT = 0.0;
		if (VrWallSegmentCrosses(Ghost, Projectile.Pos, End, CrossT))
		{
			return true;
		}
	}
	return false;
}

void ConsiderFireWall(FArenaState& State, FActor& Self, FMageBrain& Brain, const FMageProfile& Profile, FInputFrame& Result, const FVrRuleset* Rules)
{
	if (!Rules || !Rules->bActive || !Rules->FireWall.bEnabled || !Self.Fire.bSchool)
	{
		return;
	}
	if (Rules->FindWall(Self.Id) != nullptr)
	{
		// The curtain is already up. The next volley starts a fresh reaction.
		Brain.WallSeenTick = NeverTick;
		Brain.WallLastSeenTick = NeverTick;
		return;
	}
	const FSimVec Facing = SimUnit(SimSub(Result.Aim, Self.Pos), Self.Facing);
	if (HostileProjectileCrosses(State, Self, Facing, Rules->FireWall))
	{
		Brain.WallLastSeenTick = State.Tick;
		if (Brain.WallSeenTick == NeverTick)
		{
			Brain.WallSeenTick = State.Tick;
		}
	}
	else if (Brain.WallSeenTick == NeverTick)
	{
		return;
	}
	else
	{
		// A water bolt from duel range lands inside the reaction window, so the tick
		// it dies would otherwise wipe the timer. Hold the first sight across that gap.
		// Forget only after a quiet longer than the reaction plus a short grace.
		const int32 QuietFor = State.Tick - Brain.WallLastSeenTick;
		const int32 ForgetAfter = SimTicks(Profile.ReactionDelayS) + SimTicks(0.4);
		if (QuietFor > ForgetAfter)
		{
			Brain.WallSeenTick = NeverTick;
			Brain.WallLastSeenTick = NeverTick;
			return;
		}
	}
	if (State.Tick - Brain.WallSeenTick < SimTicks(Profile.ReactionDelayS))
	{
		return;
	}
	if (Self.Mana + 1.0e-9 < Rules->FireWall.ManaCost)
	{
		return;
	}
	if (Self.Tier >= 4 && !Self.Fire.bSunfallLoosed)
	{
		if (const FFireSpell* Sun = FindFireSpell(TEXT("fire_sunfall")))
		{
			if (Self.Mana - Rules->FireWall.ManaCost < Sun->Mana)
			{
				return;
			}
		}
	}
	if (Result.bCast)
	{
		const FFireSpell* Spell = FireSpellFor(Self, Result.Slot);
		if (Spell && Spell->Id == TEXT("fire_sunfall"))
		{
			return;
		}
	}
	// The curtain deletes the bolt. Spending the same tick on the palm would keep the wall down.
	Result.bRaiseFireWall = true;
	Result.bCast = false;
	Result.bAbsorb = false;
}
}

FMageProfile MageCompetence(double Level)
{
	const FKernelData& Data = KernelData();
	checkf(Data.MageCompetence.Num() > 0, TEXT("mage competence table is empty"));
	const TArray<FMageCompetenceRow>& Rows = Data.MageCompetence;
	const double Bounded = SimClamp(Level, Rows[0].Level, Rows.Last().Level);
	const int32 LoIndex = static_cast<int32>(std::floor(Bounded)) - 1;
	const int32 HiIndex = static_cast<int32>(std::ceil(Bounded)) - 1;
	checkf(Rows.IsValidIndex(LoIndex) && Rows.IsValidIndex(HiIndex), TEXT("competence level %g is outside the table"), Level);
	const FMageCompetenceRow& Lo = Rows[LoIndex];
	const FMageCompetenceRow& Hi = Rows[HiIndex];
	const double Fraction = Bounded - std::floor(Bounded);
	auto Mix = [&](double Low, double High)
	{
		return Low + (High - Low) * Fraction;
	};
	FMageProfile Profile;
	Profile.Level = Bounded;
	Profile.ReactionDelayS = std::max(Data.MageCaps.ReactionDelayMinS, Mix(Lo.ReactionDelayS, Hi.ReactionDelayS));
	Profile.AbsorbChance = Mix(Lo.AbsorbChance, Hi.AbsorbChance);
	Profile.PerfectChance = std::min(Data.MageCaps.PerfectAbsorbChanceMax, Mix(Lo.PerfectChance, Hi.PerfectChance));
	Profile.AimErrorDeg = Mix(Lo.AimErrorDeg, Hi.AimErrorDeg);
	Profile.DecisionCadenceS = Mix(Lo.DecisionCadenceS, Hi.DecisionCadenceS);
	return Profile;
}

void AttachMageAI(FActor& Actor, double Level, int32 Tick)
{
	FMageBrain Brain;
	Brain.Competence = Level;
	Brain.NextDecisionTick = Tick;
	Brain.Input = SimIdleInput();
	Brain.DefendUntil = 0;
	Brain.PlannedRaiseTick = MagePlanSentinelTick;
	Brain.PlannedReleaseTick = 0;
	Brain.TargetPoint = Actor.Pos;
	Actor.MageAI = Brain;
}

FInputFrame MageInput(FArenaState& State, FActor& Self, const FVrRuleset* Rules)
{
	if (!Self.MageAI.IsSet() || Self.bDown)
	{
		return SimIdleInput();
	}
	FMageBrain& Brain = Self.MageAI.GetValue();
	const FKernelData& Data = KernelData();
	const FMageProfile Profile = MageCompetence(Brain.Competence);
	const int32 TargetId = NearestTargetId(State, Self);
	const FActor* Target = TargetId == INDEX_NONE ? nullptr : ActorById(State, TargetId);
	const TArray<FThreat> Seen = CollectThreats(State, Self);
	React(State, Self, Brain, Profile, Seen);
	if (State.Tick >= Brain.NextDecisionTick)
	{
		// Cadence still advances while defending. The body waits until defendUntil.
		Brain.NextDecisionTick = State.Tick + SimTicks(Profile.DecisionCadenceS);
		Brain.DecisionTicks.Add(State.Tick);
		// mage-ai.ts:78 — a decision past defendUntil stores idleInput(targetPoint) before the
		// foe check. With nobody left, cast and move are cleared below but aim stays targetPoint.
		if (State.Tick >= Brain.DefendUntil)
		{
			Brain.Input = SimIdleInput(Brain.TargetPoint);
			if (Target)
			{
				const double Distance = SimDistance(Self.Pos, Target->Pos);
				const FSimVec Direction = SimUnit(SimSub(Target->Pos, Self.Pos));
				int32 Strafe = -1;
				const int32 Period = SimTicks(Data.MageStrafePeriodS);
				if (Period > 0)
				{
					Strafe = (State.Tick / Period) % 2 ? 1 : -1;
				}
				if (Distance > Data.MagePreferredMaxM)
				{
					Brain.Input.Move = Direction;
				}
				else if (Distance < Data.MagePreferredMinM)
				{
					Brain.Input.Move = FSimVec{-Direction.X, -Direction.Y};
				}
				else
				{
					Brain.Input.Move = FSimVec{-Direction.Y * static_cast<double>(Strafe), Direction.X * static_cast<double>(Strafe)};
				}
				if (!ArenaContains(Self.Pos, Data.SpawnMarginM))
				{
					Brain.Input.Move = SimUnit(SimSub(Data.ArenaCentre, Self.Pos));
				}
				const FSimVec AimPoint = Target->Water.Decoy.IsSet() ? Target->Water.Decoy->Pos : Target->Pos;
				const FSimVec Velocity = SimSub(Target->Pos, Target->PreviousPos);
				double BoltSpeed = Data.BoltSpeedMps;
				if (Self.Fire.bSchool)
				{
					if (const FFireSpell* Bolt = FindFireSpell(TEXT("fire_bolt")))
					{
						BoltSpeed = Bolt->SpeedMps;
					}
				}
				const double Lead = Distance / BoltSpeed * Data.SimStepHz * Data.MageAimLeadFraction;
				const FSimVec Prediction = Target->Water.Decoy.IsSet() ? AimPoint : SimAdd(AimPoint, SimScale(Velocity, Lead));
				const double Error = (ArenaRandom(State, FString::Printf(TEXT("mage %d aim"), Self.Id)) * 2.0 - 1.0) * Profile.AimErrorDeg * SimPi / 180.0;
				const FSimVec AimDirection = SimRotate(SimSub(Prediction, Self.Pos), Error);
				Brain.Input.Aim = SimAdd(Self.Pos, AimDirection);
				ChooseSpell(State, Self, Brain, Profile, *Target, Distance, Seen);
			}
		}
	}
	FInputFrame Result = Brain.Input;
	Result.bAbsorb = State.Tick + 1 >= Brain.PlannedRaiseTick && State.Tick + 1 <= Brain.PlannedReleaseTick;
	if (Result.bAbsorb)
	{
		Result.bCast = false;
		Result.Aim = Brain.TargetPoint;
	}
	if (!Target)
	{
		Result.bCast = false;
		Result.Move = FSimVec{0.0, 0.0};
	}
	ConsiderFireWall(State, Self, Brain, Profile, Result, Rules);
	Brain.Input.bRoll = false;
	return Result;
}
