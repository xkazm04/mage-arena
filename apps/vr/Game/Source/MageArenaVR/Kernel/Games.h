#pragma once

#include "Kernel/SimTypes.h"
#include "Kernel/VrRules.h"

// games.ts. Tiro is arena-tiers index 0, the same tier the pinned machine plays.

struct FSpawnRecord
{
	int32 Wave = 0;
	int32 Id = 0;
	FString Kind;
	double X = 0.0;
	double Y = 0.0;
};

struct FGamesResult
{
	FString Kind;
	int32 WavesCleared = 0;
	int32 Gold = 0;
	int32 Renown = 0;
	bool bFinalReached = false;
	bool bFinalWon = false;
};

struct FGames
{
	FArenaState State;
	int32 PlayerId = 0;
	int32 Wave = 0;
	FString Phase;
	int32 WavesCleared = 0;
	int32 WaveStartTick = 0;
	TOptional<FGamesResult> Result;
	TArray<FSpawnRecord> SpawnLog;
	// False keeps the pinned water-proxy duelists. Fire scenarios opt in.
	bool bFireMages = false;
	// T23 (DECISIONS 2026-10-07: the Tiro final is the second school, Air). With bFireMages set, the final (wave kind
	// "final") spawns an air mage instead of a fire one; the semifinal stays Fire. False keeps the pre-T23 fire final.
	bool bAirFinal = false;
	// Set only when a session passes the VR overlay. Conformance leaves this empty.
	TOptional<FVrRuleset> VrRules;
};

struct FFightReport
{
	uint32 Seed = 0;
	int32 Wave = 0;
	FString Outcome;
	double DurationS = 0.0;
	double Hp = 0.0;
	double Mana = 0.0;
	double Stamina = 0.0;
	int32 Perfects = 0;
	int32 IncomingHits = 0;
	double DamageDealt = 0.0;
	double DamageTaken = 0.0;
	int32 DeadAirBuckets = 0;
	int32 Buckets = 0;
	FString Hash;
	bool bFinite = false;
};

// False when the wave index is outside Tiro. Does not checkf: the caller is a test.
bool TryCreateGames(FGames& Out, uint32 Seed, const FComposition* Composition = nullptr, int32 StartWave = 0, bool bReferencePlayer = false, bool bFireMages = false, const FVrRuleset* Rules = nullptr, bool bAirFinal = false);
// "water", "fire" or "air": the school of the mage that wave index Wave spawns in these games.
FString GamesMageSchool(const FGames& Games, int32 Wave);
void StepGames(FGames& Games, const FInputFrame* PlayerInput = nullptr);
// False unless the phase is intermission.
bool TryAdvanceGames(FGames& Games);
// Null while the bout is still in progress. Settled results stay put.
const FGamesResult* GamesResult(FGames& Games);
FFightReport RunFight(uint32 Seed, int32 Wave, double MaxSeconds);
