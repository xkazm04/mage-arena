#pragma once

// mage-ai.ts lives in Kernel/MageAI.h. games.ts lives in Kernel/Games.h.
// stepGames order: EnemyInputs, mageInput for every living mage brain, optional player overlay,
// StepArena, QueueDeathEffects, then the win/loss check. AdvanceGames calls ResetWave.
// VR threat geometry is an optional FVrRuleset on TryCreateGames (see CR-003). Null is the pinned path:
// EnemyInputs gets no overlay, and KeepOut does not run. Desktop keeps passing null.
