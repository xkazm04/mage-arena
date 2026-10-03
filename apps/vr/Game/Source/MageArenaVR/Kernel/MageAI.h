#pragma once

#include "Kernel/SimTypes.h"

// mage-ai.ts. Competence is a dial on inputs. It does not change ranks or resources.

struct FMageProfile
{
	double Level = 0.0;
	double ReactionDelayS = 0.0;
	double AbsorbChance = 0.0;
	double PerfectChance = 0.0;
	double AimErrorDeg = 0.0;
	double DecisionCadenceS = 0.0;
};

FMageProfile MageCompetence(double Level);
void AttachMageAI(FActor& Actor, double Level, int32 Tick = 0);
// Rules null is the pinned path: no fire wall, no extra random draw.
struct FVrRuleset;
FInputFrame MageInput(FArenaState& State, FActor& Actor, const FVrRuleset* Rules = nullptr);
