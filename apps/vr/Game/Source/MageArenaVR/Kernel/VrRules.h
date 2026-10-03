#pragma once

#include "Kernel/SimTypes.h"

// VR combat overlay. Absent means the pinned kernel. Session creation is the only caller that passes one.

struct FVrAabb
{
	double MinX = 0.0;
	double MaxX = 0.0;
	double MinY = 0.0;
	double MaxY = 0.0;

	bool Contains(const FSimVec& Point) const;
	FVrAabb Expanded(double Margin) const;
};

struct FVrAttackMode
{
	FString EnemyId;
	bool bThrow = false;
	double ProjectileMps = 0.0;
	double RangeM = 0.0;
	double ArcApexM = 0.0;
	double SpearLengthM = 0.0;
	double SpearRadiusM = 0.0;
};

struct FVrSplitHands
{
	bool bEnabled = false;
	double OneHandPower = 0.6;
	int32 MaxTier = 2;
};

struct FVrPlantedStaff
{
	bool bEnabled = false;
	double DurationS = 6.0;
	double ManaUpFront = 8.0;
	double DrainPerSecond = 4.0;
	double MagicReduction = 0.85;
	double PhysicalReduction = 0.6;
};

// Runtime for one bout. Not part of the pinned state hash. A copied ruleset starts empty.
struct FVrStaffRuntime
{
	bool bPlanted = false;
	int32 ActorId = -1;
	int32 UntilTick = 0;
};

// SCHOOL-DEFENCES C. Proposal values live in combat.vr.json. Calibration may override them.
struct FVrFireWall
{
	bool bEnabled = false;
	double DistanceM = 4.0;
	double ArcDeg = 90.0;
	double DurationS = 3.0;
	double ManaCost = 12.0;
	double HeatPerStop = 4.0;
	double BurnDamage = 12.0;
};

// One raised curtain. Centre and facing are the caster's at the raise tick, before that tick's move.
struct FVrWall
{
	int32 Id = 0;
	int32 OwnerId = -1;
	FSimVec Centre;
	FSimVec Facing{1.0, 0.0};
	double DistanceM = 4.0;
	double ArcDeg = 90.0;
	int32 UntilTick = 0;
};

// Identity leaves the pinned wave. Counts below zero mean "use the pinned count".
struct FVrWaveTune
{
	int32 ConscriptCount = -1;
	int32 SlingerCount = -1;
	double SpawnDelayS = 0.0;
};

// Identity is 1. Cadence scales recovery, backoff, and cooldown, not the telegraph windup.
struct FVrPressure
{
	double AttackCadence = 1.0;
	double EnemyDamage = 1.0;
	double FireMageDamage = 1.0;
};

// Identity is 1. Applied to the seated player (team 0) only.
struct FVrPower
{
	double BoltDamage = 1.0;
	double LineDamage = 1.0;
	double ManaRegen = 1.0;
};

// Multiplier on ward drain while a split cast is latched. Identity is 1.
struct FVrDefenceTune
{
	double WardDrainSplit = 1.0;
};

// True when the segment crosses the arc. OutT is along From -> To. A graze that stays outside does not count.
bool VrWallSegmentCrosses(const FVrWall& Wall, const FSimVec& From, const FSimVec& To, double& OutT);

struct FVrRuleset
{
	bool bActive = false;
	double StandoffM = 0.0;
	FVrAabb Dais;
	TArray<FVrAttackMode> Throws;
	FVrSplitHands Split;
	FVrPlantedStaff Staff;
	FVrFireWall FireWall;
	FVrWaveTune Wave;
	FVrPressure Pressure;
	FVrPower Power;
	FVrDefenceTune Defence;
	FVrStaffRuntime StaffRuntime;
	TArray<FVrWall> Walls;
	int32 WallsRaised = 0;
	TArray<int32> SplitCasting;
	TArray<FString> Refusals;

	const FVrAttackMode* FindThrow(const FString& EnemyId) const;
	FVrAabb HoldBox() const;
	bool InsideDais(const FSimVec& Point) const;
	bool InsideHold(const FSimVec& Point) const;
	// Negative when inside the hold box, zero on the boundary, positive outside.
	double OutsideGap(const FSimVec& Point) const;
	FSimVec HoldPoint(const FSimVec& From, const FSimVec& Goal) const;
	FSimVec Outward(const FSimVec& Point) const;
	FSimVec PushOutsideHold(const FSimVec& Point) const;
	void KeepOut(FArenaState& State) const;

	bool IsPlanted(int32 ActorId) const;
	void Lift(const TCHAR* Why);
	bool IsSplitCasting(int32 ActorId) const;
	void SetSplitCasting(int32 ActorId, bool bCasting);
	// 0 for unblockable and for any other family. The dome never perfects.
	double DomeReduction(const FString& Family) const;
	void TickStaff(FArenaState& State, FActor& Actor, const FInputFrame& Input);
	void Refuse(const FActor& Actor, const TCHAR* Reason);

	const FVrWall* FindWall(int32 OwnerId) const;
	void ExpireWalls(const FArenaState& State);
	void TickWalls(FArenaState& State, FActor& Actor, const FInputFrame& Input);
	void BurnCrossers(FArenaState& State);
	void ClearRuntime();
};

// Reads combat.vr.json. With bHonorProposalSwitch, -MageArenaProposal or MageArenaProposal=1
// overlays apps/vr/data/vr/calibration-proposal.json. The default bout does not.
bool LoadVrRuleset(FVrRuleset& Out, FString& Error, bool bHonorProposalSwitch = true);
