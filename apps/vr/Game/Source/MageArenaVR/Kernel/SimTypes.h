#pragma once

#include "Kernel/SimConstants.h"
#include "Kernel/SimMath.h"

#include "CoreMinimal.h"

// Plain simulation records. No UObject. Field set follows types.ts.

struct FSimRanks
{
	int32 Vigor = 1;
	int32 Focus = 1;
	int32 Nerve = 1;
};

struct FSpellBranches
{
	FString Lash;
	FString Mirror;
	FString TideOrb;
};

struct FComposition
{
	FString Name;
	TArray<FString> Lines;
	FSpellBranches Branches;
};

struct FCooldownEntry
{
	FString Line;
	int32 Until = 0;
};

struct FDecoy
{
	FSimVec Pos;
	int32 Until = 0;
};

struct FWaterState
{
	FComposition Composition;
	int32 Flow = 0;
	FString LastLine;
	int32 LastCastTick = NeverTick;
	int32 LastActivityTick = NeverTick;
	TArray<FCooldownEntry> Cooldowns;
	double Stored = 0.0;
	int32 RootUntil = 0;
	int32 EncasedUntil = 0;
	int32 SlowUntil = 0;
	double SlowMult = 1.0;
	int32 WardUntil = 0;
	int32 SheenUntil = 0;
	int32 HotUntil = 0;
	double HotPerTick = 0.0;
	int32 Crests = 0;
	double Healing = 0.0;
	int32 ControlTicks = 0;
	TOptional<FDecoy> Decoy;
};

struct FInputFrame
{
	FSimVec Move;
	FSimVec Aim{IdleAimX, IdleAimY};
	int32 Slot = 0;
	bool bCast = false;
	bool bAbsorb = false;
	bool bRoll = false;
	bool bSprint = false;
	// VR overlay only. Conformance frames leave these false. The pinned path ignores them.
	bool bPlantStaff = false;
	bool bLiftStaff = false;
	bool bRaiseFireWall = false;
};

inline FInputFrame SimIdleInput(const FSimVec& Aim = FSimVec{IdleAimX, IdleAimY})
{
	FInputFrame Frame;
	Frame.Aim = Aim;
	return Frame;
}

inline int32 SimCooldownUntil(const FWaterState& Water, const FString& Line)
{
	for (const FCooldownEntry& Entry : Water.Cooldowns)
	{
		if (Entry.Line == Line)
		{
			return Entry.Until;
		}
	}
	return 0;
}

inline void SimSetCooldown(FWaterState& Water, const FString& Line, int32 Until)
{
	for (FCooldownEntry& Entry : Water.Cooldowns)
	{
		if (Entry.Line == Line)
		{
			Entry.Until = Until;
			return;
		}
	}
	Water.Cooldowns.Add({Line, Until});
}

inline bool SimHasLine(const FComposition& Composition, const TCHAR* Line)
{
	for (const FString& Entry : Composition.Lines)
	{
		if (Entry == Line)
		{
			return true;
		}
	}
	return false;
}

// Hit carried by a projectile, a telegraph, or a direct resolve. types.ts Hit.
struct FHit
{
	int32 OwnerId = 0;
	int32 ActivationId = 0;
	double Damage = 0.0;
	FString Family;
	int32 Tier = 0;
	FSimVec Source;
	bool bBolt = false;
	bool bReaction = false;
	FString Delivery;
	// Fire spells copy these. Water hits leave them at the defaults, so absorb and shields stay as they were.
	double HeatOnHit = 0.0;
	bool bSuppressPerfect = false;
	bool bPierceShields = false;
};

struct FFireCooldown
{
	FString SpellId;
	int32 Until = 0;
};

// A cinder trail is the segment the dash actually traveled. Ticks dedup per sim tick and target.
struct FFireTrail
{
	FSimVec From;
	FSimVec To;
	int32 Until = 0;
	int32 NextTick = 0;
	int32 IntervalTicks = 1;
	double Damage = 0.0;
	double HeatOnHit = 0.0;
	int32 Tier = 0;
	FString Family;
	FString SpellId;
};

struct FFireChannel
{
	FString SpellId;
	int32 Until = 0;
	int32 NextTick = 0;
	int32 IntervalTicks = 1;
	int32 TicksDone = 0;
	double Damage = 0.0;
	double HeatOnHit = 0.0;
	double RangeM = 0.0;
	int32 Tier = 0;
	FString Family;
	bool bPierceShields = false;
	bool bPerfectOnlyFirstTick = false;
};

// Heat exists only while bSchool is set. Water actors keep the zeros.
struct FFireState
{
	bool bSchool = false;
	double Heat = 0.0;
	double MaxHeat = 0.0;
	int32 LastGainTick = NeverTick;
	int32 LockUntil = 0;
	double LockValue = 0.0;
	double AbsorbDrainSpellMult = 1.0;
	int32 AbsorbDrainSpellUntil = 0;
	int32 SunfallCastTick = 0;
	int32 SunfallTick = 0;
	bool bSunfallLoosed = false;
	TArray<FFireCooldown> Cooldowns;
	TArray<FFireTrail> Trails;
	TOptional<FFireChannel> Channel;
};

struct FEnemyBrain
{
	FString Id;
	int32 ReadyTick = 0;
	int32 BackoffUntil = 0;
	int32 StunnedUntil = 0;
	int32 AttackIndex = 0;
	bool bDeathQueued = false;
};

// mage-ai.ts MageBrain. Observed ids stay until the activation leaves the threat list.
struct FMageObservation
{
	int32 Id = 0;
	int32 FirstSeenTick = 0;
	bool bReacted = false;
};

struct FMageBrain
{
	double Competence = 0.0;
	int32 NextDecisionTick = 0;
	FInputFrame Input;
	TArray<FMageObservation> Observed;
	int32 DefendUntil = 0;
	int32 PlannedRaiseTick = MagePlanSentinelTick;
	int32 PlannedReleaseTick = 0;
	FSimVec TargetPoint;
	TArray<int32> DecisionTicks;
	TArray<int32> ReactionAges;
	// VR overlay. The fire wall reaction. Not part of the pinned state hash.
	// WallSeenTick is the first inbound bolt of this volley. WallLastSeenTick is the
	// latest one, so a gap shorter than the reaction does not start the timer over.
	int32 WallSeenTick = NeverTick;
	int32 WallLastSeenTick = NeverTick;
};

struct FPendingCast
{
	FString Kind;
	int32 ReleaseTick = 0;
	int32 StartTick = 0;
	FSimVec Aim;
	int32 ActivationId = 0;
	TOptional<FString> SpellId;
	TOptional<double> DamageMult;
	TOptional<int32> TargetId;
};

struct FMetrics
{
	int32 Perfects = 0;
	int32 Blocks = 0;
	int32 Hits = 0;
	double DamageDealt = 0.0;
	double DamageTaken = 0.0;
	double ManaDrained = 0.0;
	double ManaRaised = 0.0;
	double ManaReturned = 0.0;
	int32 Casts = 0;
	int32 Rolls = 0;
};

struct FActor
{
	int32 Id = 0;
	int32 Team = 0;
	FString Label;
	FSimVec Pos;
	FSimVec PreviousPos;
	FSimVec Facing{1.0, 0.0};
	double Radius = 0.0;
	FSimRanks Ranks;
	double Hp = 0.0;
	double MaxHp = 0.0;
	double Mana = 0.0;
	double MaxMana = 0.0;
	double Stamina = 0.0;
	double MaxStamina = 0.0;
	bool bDown = false;
	bool bDummy = false;
	bool bAbsorb = false;
	int32 AbsorbFreshTick = NeverTick;
	int32 ReleaseTick = NeverTick;
	bool bAbsorbExhausted = false;
	FWaterState Water;
	FFireState Fire;
	TOptional<FEnemyBrain> Enemy;
	TOptional<FMageBrain> MageAI;
	TOptional<double> SpeedMps;
	FInputFrame LastInput;
	int32 RollUntil = 0;
	int32 ImmuneUntil = 0;
	int32 RecoveryUntil = 0;
	FSimVec RollDirection{1.0, 0.0};
	int32 StaminaUsedTick = NeverTick;
	int32 CooldownUntil = 0;
	TOptional<FPendingCast> Pending;
	int32 WaveStartTick = 0;
	int32 Tier = 1;
	int32 LastUnlockTick = 0;
	int32 ClockAdvanceTicks = 0;
	TArray<int32> UnlockTicks;
	FMetrics Metrics;
};

struct FProjectile
{
	int32 Id = 0;
	int32 OwnerId = 0;
	int32 ActivationId = 0;
	double Damage = 0.0;
	FString Family;
	int32 Tier = 0;
	FSimVec Source;
	bool bBolt = false;
	bool bReaction = false;
	FString Delivery;
	FSimVec Pos;
	FSimVec PreviousPos;
	FSimVec Velocity;
	double Radius = 0.0;
	double RemainingM = 0.0;
	TArray<int32> HitIds;
	double BurstRadiusM = 0.0;
	bool bPiercing = false;
	bool bReflected = false;
	double HeatOnHit = 0.0;
	bool bSuppressPerfect = false;
	bool bPierceShields = false;
	// Presentation only. The sim does not read these. Spawn leaves bHasAim clear unless a telegraph passed a target.
	FSimVec OriginPos;
	FSimVec AimedAt;
	bool bHasAim = false;
};

struct FZone
{
	int32 Id = 0;
	int32 OwnerId = 0;
	FSimVec Pos;
	double RadiusM = 0.0;
	int32 Until = 0;
	FString Kind;
	double SlowMult = 0.0;
};

struct FTelegraph
{
	int32 Id = 0;
	int32 ActivationId = 0;
	int32 OwnerId = 0;
	FString Family;
	int32 Tier = 0;
	double Damage = 0.0;
	FSimVec Source;
	bool bBolt = false;
	FString Kind;
	FSimVec Origin;
	FSimVec Target;
	int32 ResolveTick = 0;
	int32 StartTick = 0;
	double SpeedMps = 0.0;
	double RangeM = 0.0;
	double WidthM = 0.0;
	double RootS = 0.0;
	double PullM = 0.0;
	bool bSurvivesOwner = false;
	double WallStunS = 0.0;
	double HeatOnHit = 0.0;
	bool bSuppressPerfect = false;
	bool bPierceShields = false;
	// Set once a cast has finished releasing. Interrupt keeps these; it still clears a windup.
	bool bCommitted = false;
};

struct FArenaEvent
{
	int32 Tick = 0;
	FString Kind;
	int32 ActorId = 0;
	TOptional<int32> TargetId;
	double Value = 0.0;
};

struct FRandomDraw
{
	int32 Tick = 0;
	FString Purpose;
	double Value = 0.0;
};

struct FArenaState
{
	int32 Tick = 0;
	uint32 Seed = 1;
	uint32 Rng = 1;
	int32 NextId = 1;
	TArray<FActor> Actors;
	TArray<FProjectile> Projectiles;
	TArray<FTelegraph> Telegraphs;
	TArray<FZone> Zones;
	TArray<FArenaEvent> Events;
	TArray<FRandomDraw> RandomLog;
};

inline FActor* SimFindActor(FArenaState& State, int32 Id)
{
	for (FActor& Actor : State.Actors)
	{
		if (Actor.Id == Id)
		{
			return &Actor;
		}
	}
	return nullptr;
}

inline const FActor* SimFindActor(const FArenaState& State, int32 Id)
{
	for (const FActor& Actor : State.Actors)
	{
		if (Actor.Id == Id)
		{
			return &Actor;
		}
	}
	return nullptr;
}
