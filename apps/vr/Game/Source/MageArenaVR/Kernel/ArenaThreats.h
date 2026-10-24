#pragma once

#include "Combat/AbsorbResolver.h"
#include "Kernel/ArenaKernel.h"

// Module-private. The world phase of StepArena: telegraphs resolve first, then projectiles fly and hit.
// ResolveHit and SpawnProjectile are the public half of this file and stay declared in ArenaKernel.h.
void UpdateTelegraphs(FArenaState& State, FVrRuleset* Rules);
void UpdateProjectiles(FArenaState& State, FVrRuleset* Rules);
// The absorb kind ResolveHit gives a hit's family. Declared here so a test can ask it about a family no loader admits.
EAbsorbHitKind KindFromFamily(const FString& Family);
