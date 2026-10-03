#pragma once

#include "CoreMinimal.h"

#include <cstdint>

// Algorithm sentinels from the pinned TypeScript kernel. Combat tuning stays in the pinned files.

// kernel.ts:12 — ceil bias so an exact tick boundary does not step up.
inline constexpr double TickCeilBias = 1.0e-9;
// kernel.ts:95
inline constexpr double ManaExhaustedEpsilon = 1.0e-9;
// kernel.ts:213
inline constexpr double ProjectileRemainEpsilon = 1.0e-9;
// kernel.ts:35 — absorbFreshTick, releaseTick and staminaUsedTick start at -1e9.
inline constexpr int32 NeverTick = -1000000000;
// math.ts:12
inline constexpr double ArcDotEpsilon = 1.0e-12;
// geometry.ts:12
inline constexpr double ArenaContainsEpsilon = 1.0e-12;
// geometry.ts:8
inline constexpr double ArenaInsetMinFactor = 0.001;
// kernel.ts:257
inline constexpr double StepperEpsilon = 1.0e-12;
// kernel.ts:23
inline constexpr uint32 MulberryAdd = 0x6d2b79f5u;
// kernel.ts:29
inline constexpr int32 RankMin = 1;
inline constexpr int32 RankMax = 5;
// kernel.ts:216
inline constexpr int32 TierCap = 4;
// types.ts:34 — idleInput's default aim when the caller passes none.
inline constexpr double IdleAimX = 32.0;
inline constexpr double IdleAimY = 10.0;
// kernel.ts:248-251
inline constexpr uint32 FnvOffset = 2166136261u;
inline constexpr uint32 FnvPrime = 16777619u;
// math.ts uses Math.PI. This is that IEEE-754 double.
inline constexpr double SimPi = 3.141592653589793;
// kernel.ts:57 treats only family "magic" as perfectable. The loader checks notAgainst is exactly the other two.
inline constexpr int32 AbsorbNotAgainstCount = 2;
// enemies.ts:58 — stop and let the windup play once inside this fraction of the attack range.
inline constexpr double EnemyStopRangeFactor = 0.8;
// mage-ai.ts attachMageAI — plannedRaiseTick starts at 1e9 so a fresh brain is not already absorbing.
inline constexpr int32 MagePlanSentinelTick = 1000000000;
