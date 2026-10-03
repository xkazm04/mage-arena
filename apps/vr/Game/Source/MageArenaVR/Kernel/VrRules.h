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

struct FVrRuleset
{
	bool bActive = false;
	double StandoffM = 0.0;
	FVrAabb Dais;
	TArray<FVrAttackMode> Throws;
	FVrSplitHands Split;
	FVrPlantedStaff Staff;
	FVrStaffRuntime StaffRuntime;
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
};

// Reads combat.vr.json and checks the dais against arena-layout.json and the pinned rows it names.
bool LoadVrRuleset(FVrRuleset& Out, FString& Error);
