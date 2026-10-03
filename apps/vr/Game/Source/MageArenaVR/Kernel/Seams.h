#pragma once

// mage-ai.ts lives in Kernel/MageAI.h. games.ts lives in Kernel/Games.h.
// stepGames order: EnemyInputs, mageInput for every living mage brain, optional player overlay,
// StepArena, QueueDeathEffects, then the win/loss check. AdvanceGames calls ResetWave.
// VR threat geometry, split hands, and the planted staff are an optional FVrRuleset on TryCreateGames
// (see CR-003). Null is the pinned path: EnemyInputs and StepArena get no overlay, KeepOut does not run,
// and the new input flags stay false. Desktop and the 45 conformance vectors keep passing null.
