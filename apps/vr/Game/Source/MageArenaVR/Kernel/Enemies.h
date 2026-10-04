#pragma once

#include "Kernel/KernelData.h"
#include "Kernel/Seams.h"

struct FVrRuleset;

const FEnemySpec* EnemySpecFor(const FActor& Actor);
FActor& AddEnemy(FArenaState& State, const FString& Id, const FSimVec& Pos);
// Rules null is the pinned path. A ruleset is the VR overlay passed in at session creation.
TMap<int32, FInputFrame> EnemyInputs(FArenaState& State, const FVrRuleset* Rules = nullptr);
struct FVrRuleset;

void QueueDeathEffects(FArenaState& State, const FVrRuleset* Rules = nullptr);
