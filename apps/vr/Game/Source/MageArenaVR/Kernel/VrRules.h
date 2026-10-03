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

struct FVrRuleset
{
	bool bActive = false;
	double StandoffM = 0.0;
	FVrAabb Dais;
	TArray<FVrAttackMode> Throws;

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
};

// Reads combat.vr.json and checks the dais against arena-layout.json and the pinned rows it names.
bool LoadVrRuleset(FVrRuleset& Out, FString& Error);
