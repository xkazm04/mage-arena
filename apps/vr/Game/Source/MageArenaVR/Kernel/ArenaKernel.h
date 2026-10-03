#pragma once

#include "Kernel/SimTypes.h"

struct FSimHitResult
{
	double Damage = 0.0;
	bool bPerfect = false;
};

struct FFixedStepper
{
	double Accumulated = 0.0;
	double Advance(double ElapsedS, const TFunctionRef<void()>& Update);
};

int32 SimTicks(double Seconds);
double SimSeconds(int32 TickCount);
double SimDt();
double DrainPerSecond(double NerveRank);
double PerfectReturn(int32 Tier, double NerveRank);

FArenaState CreateArena(uint32 Seed = 1);
double ArenaRandom(FArenaState& State, const FString& Purpose);
FActor& AddMage(FArenaState& State, int32 Team, const FSimVec& Pos, const FString& Label = TEXT("Water mage"), const FSimRanks* Ranks = nullptr);
void Emit(FArenaState& State, const TCHAR* Kind, const FActor& Actor, double Value = 0.0, const TOptional<int32>& TargetId = {});
void Interrupt(FArenaState& State, FActor& Actor);
FSimHitResult ResolveHit(FArenaState& State, FActor& Target, const FHit& Hit);
FProjectile& SpawnProjectile(FArenaState& State, const FHit& Hit, const FSimVec& Origin, const FSimVec& Direction, double SpeedMps, double RangeM, TOptional<double> Radius = {});
void UpdateClock(FArenaState& State, FActor& Actor);
void StepArena(FArenaState& State, const TMap<int32, FInputFrame>& Inputs);
void StepArena(FArenaState& State);
void ResetWave(FArenaState& State, FActor& Actor);

// FNV-1a over a canonical dump of this process. Equal hashes mean two C++ runs match.
// The TypeScript stateHash is FNV-1a of JSON.stringify and is gated by the field compare, not this string.
FString StateHash(const FArenaState& State);
