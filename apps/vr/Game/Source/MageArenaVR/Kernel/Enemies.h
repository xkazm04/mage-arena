#pragma once

#include "Kernel/KernelData.h"
#include "Kernel/Seams.h"

const FEnemySpec* EnemySpecFor(const FActor& Actor);
FActor& AddEnemy(FArenaState& State, const FString& Id, const FSimVec& Pos);
TMap<int32, FInputFrame> EnemyInputs(FArenaState& State);
void QueueDeathEffects(FArenaState& State);
