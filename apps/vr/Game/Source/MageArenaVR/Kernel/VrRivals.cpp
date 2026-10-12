#include "Kernel/VrRules.h"

#include "Kernel/Air.h"
#include "Kernel/ArenaKernel.h"
#include "Kernel/Fire.h"
#include "Kernel/KernelData.h"
#include "Kernel/SimConstants.h"
#include "MageArenaVR.h"

// DECISIONS 2026-10-07 (DF-003 answered): named rivals fight in HP-gated phases. VR overlay rule until CR-005.
// Every piece of state here lives in FVrRuleset::RivalRuntime, which the pinned state hash never reads.

int32 FVrRuleset::FindRival(const FString& TierId, int32 WaveN, const FString& WaveKind, const FString& School) const
{
	if (!bActive)
	{
		return INDEX_NONE;
	}
	for (int32 Index = 0; Index < Rivals.Num(); ++Index)
	{
		const FVrRival& Rival = Rivals[Index];
		if (Rival.TierId == TierId && Rival.WaveN == WaveN && Rival.WaveKind == WaveKind && Rival.School == School)
		{
			return Index;
		}
	}
	return INDEX_NONE;
}

const FVrRival* FVrRuleset::BoundRival() const
{
	if (!bActive || RivalRuntime.ActorId < 0 || !Rivals.IsValidIndex(RivalRuntime.RivalIndex))
	{
		return nullptr;
	}
	return &Rivals[RivalRuntime.RivalIndex];
}

bool FVrRuleset::IsRival(int32 ActorId) const
{
	return BoundRival() != nullptr && RivalRuntime.ActorId == ActorId;
}

namespace
{
// One surge step per tier, each a normal unlock: the tier, the unlock tick and the event. The minimum gap between
// unlocks is not checked here (the surge is the point of the break); the normal clock counts its gap from this tick.
void ApplySurge(FArenaState& State, FActor& Actor, int32 Tiers)
{
	for (int32 Step = 0; Step < Tiers && Actor.Tier < TierCap; ++Step)
	{
		Actor.Tier++;
		Actor.LastUnlockTick = State.Tick;
		Actor.UnlockTicks.Add(State.Tick);
		Emit(State, TEXT("unlock"), Actor, Actor.Tier);
	}
}
}

void FVrRuleset::TickRivalPhases(FArenaState& State)
{
	const FVrRival* Rival = BoundRival();
	if (!Rival)
	{
		return;
	}
	FActor* Actor = SimFindActor(State, RivalRuntime.ActorId);
	// A downed rival ends the duel. Its last hit is the victory, not a phase.
	if (!Actor || Actor->bDown)
	{
		return;
	}
	bool bBroke = false;
	while (RivalRuntime.PhasesBroken < Rival->Phases.Num())
	{
		const int32 Index = RivalRuntime.PhasesBroken;
		const FVrRivalPhase& Phase = Rival->Phases[Index];
		if (!(Actor->Hp <= Phase.AtHpFraction * Actor->MaxHp))
		{
			break;
		}
		RivalRuntime.PhasesBroken++;
		RivalRuntime.BreakTicks.Add(State.Tick);
		bBroke = true;
		// Value is the phase now entered (2 of 3, then 3 of 3). TargetId is the index of the threshold that broke.
		const int32 Entered = Index + 2;
		Emit(State, TEXT("phase"), *Actor, Entered, Index);
		ApplySurge(State, *Actor, Rival->SurgeTiers);
		for (FActor& Other : State.Actors)
		{
			if (Other.Id == Actor->Id || Other.Team == Actor->Team || Other.bDown || Other.Enemy.IsSet() || Other.bDummy)
			{
				continue;
			}
			ApplySurge(State, Other, Rival->SurgeTiers);
		}
		if (!Phase.OpensWith.IsEmpty() && !RivalRuntime.bGrantUsed && !RivalRuntime.bGrantPending)
		{
			RivalRuntime.bGrantPending = true;
			RivalRuntime.GrantSpell = Phase.OpensWith;
		}
		UE_LOG(LogMageArena, Verbose, TEXT("rival phase %d/%d actor=%d tick=%d hp=%.3f tier=%d"),
			Entered, Rival->Phases.Num() + 1, Actor->Id, State.Tick, Actor->Hp, Actor->Tier);
	}
	if (bBroke && RivalRuntime.bGrantPending)
	{
		// The break tick itself, when the rival is free. Otherwise the actor loop starts it once the current action ends.
		TryGrantedCast(State, *Actor, Actor->LastInput.Aim);
	}
}

bool FVrRuleset::TryGrantedCast(FArenaState& State, FActor& Actor, const FSimVec& Aim)
{
	if (!RivalRuntime.bGrantPending || !IsRival(Actor.Id) || Actor.bDown)
	{
		return false;
	}
	if (Actor.Pending.IsSet() || FireChannelBusy(Actor) || AirChannelBusy(Actor) || Actor.bAbsorb || State.Tick < Actor.RollUntil
		|| State.Tick < Actor.RecoveryUntil || State.Tick < Actor.Water.EncasedUntil)
	{
		return false;
	}
	// T23: the signature is cast in the rival's own school. FireMageDamage is the fire mage's knob only.
	if (Actor.Air.bSchool)
	{
		if (!StartGrantedAirCast(State, Actor, RivalRuntime.GrantSpell, Aim, this))
		{
			return false;
		}
	}
	else
	{
		if (!StartGrantedFireCast(State, Actor, RivalRuntime.GrantSpell, Aim, this))
		{
			return false;
		}
		if (Actor.MageAI.IsSet() && Pressure.FireMageDamage != 1.0 && Actor.Pending.IsSet())
		{
			Actor.Pending->DamageMult = Pressure.FireMageDamage;
		}
	}
	RivalRuntime.bGrantPending = false;
	RivalRuntime.bGrantUsed = true;
	RivalRuntime.GrantTick = State.Tick;
	return true;
}
