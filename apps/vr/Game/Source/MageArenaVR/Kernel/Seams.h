#pragma once

// mage-ai.ts lives in Kernel/MageAI.h. games.ts lives in Kernel/Games.h.
// stepGames order: EnemyInputs, mageInput for every living mage brain, optional player overlay,
// StepArena, QueueDeathEffects, then the win/loss check. AdvanceGames calls ResetWave.
