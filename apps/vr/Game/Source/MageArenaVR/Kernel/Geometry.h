#pragma once

#include "Kernel/SimMath.h"

// geometry.ts. The oval comes from the scale contract and runtime centre, not a 32 by 20 court.

bool ArenaContains(const FSimVec& Point, double Inset = 0.0);
FSimVec ConstrainToArena(const FSimVec& Point, double Inset = 0.0);
FSimVec OpeningPosition(int32 Index, int32 Count, const FSimVec& Jitter = FSimVec{0.0, 0.0});
