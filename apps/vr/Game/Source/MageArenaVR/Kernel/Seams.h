#pragma once

// mage-ai.ts and games.ts stay on the next card.
//
// games.ts stepGames is: EnemyInputs, overlay the player input, mageInput when MageAI is set,
// StepArena, QueueDeathEffects, then the phase machine (spawn, payout, wave advance).
// EnemyInputs and QueueDeathEffects live in Kernel/Enemies.h because resolveHit and the soldier
// vector need them. The phase machine, arena-tiers.json and the mage competence table do not.
//
// FActor::MageAI is the attachment point attachMageAI would fill. Nothing in this port writes it.
// A games driver can call EnemyInputs + StepArena + QueueDeathEffects without linking mage-ai.
