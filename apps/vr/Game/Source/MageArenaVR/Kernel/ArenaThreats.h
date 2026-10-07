#pragma once

#include "Kernel/ArenaKernel.h"

// Module-private. The world phase of StepArena: telegraphs resolve first, then projectiles fly and hit.
// ResolveHit and SpawnProjectile are the public half of this file and stay declared in ArenaKernel.h.
void UpdateTelegraphs(FArenaState& State, FVrRuleset* Rules);
void UpdateProjectiles(FArenaState& State, FVrRuleset* Rules);
