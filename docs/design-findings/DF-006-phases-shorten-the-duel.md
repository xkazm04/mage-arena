# DF-006 - Rival phases guarantee Sunfall but shorten the duel

Found 2026-10-07 by T19. Status: open, owner decision needed. Census: `DF-006-census/` (20 seeds, seated Brennic duel,
Tiro semifinal, 120 s cap; copied from `runs/T19/census/`).

The DF-003 answer (DECISIONS 2026-10-07) is built as an overlay: phases at 66 % and 33 % HP, a one-tier surge on both
collars per break, and a granted Sunfall opening the last phase.

| Set | Comp. | Median s | Win | Reach 33 % | Signature / last phase |
|---|---|---|---|---|---|
| live | 1 | 31.23 | 0.65 | 0.95 | 19/19 |
| live | 1.5 | 24.43 | 0.15 | 0.95 | 19/19 |
| proposal (`wide`) | 1 | 41.87 | 0.70 | 1.00 | 20/20 |
| proposal (`wide`) | 1.5 | 38.96 | 0.60 | 0.95 | 19/19 |
| live, no phases | 1 / 1.5 | 30.47 / 28.52 | 0.85 / 0.60 | - | 0 Sunfall |
| proposal, no phases | 1 / 1.5 | 44.83 / 40.56 | 0.95 / 0.75 | - | 1 / 3 Sunfall |

Targets: signature 100 % (met everywhere), median 45-80 s (missed everywhere), win >= 70 % at c1 and 40-60 % at c1.5
(met only on the proposal). The surge also raises Brennic's own tier, and the granted Sunfall ends fights sooner, so the
phases cost 0-4 s instead of adding length.

## Options (can combine)
- **A. Adopt the `wide` proposal (DF-003)** for the slice: it alone puts both win rates in band with phases on.
- **B. A phase-break beat (recommended with A):** on each break the rival is warded and does not cast for about 3 s
  while the crowd swells and the taunt plays (the voiced line in the vision). Two breaks add about 6 s, so about 48 s on
  the proposal at c1, and the climax gets a stage. VR overlay only.
- **C. Named rivals at vigor rank 2** (max HP 110 instead of 95): longer by HP, every number else unchanged.
- **D. Surge the player's collar only** on a break, not the rival's: changes the vision's "both collars".

## Addendum 2026-10-07 (T21, the chained day)
In the full Tiro day on the proposal, the seat wins Bouts 1-2 and then **loses Bout 3 to Brennic at 35.82 s**, carrying
Bout 2's damage through the `betweenWaves` heal (30 % of missing HP). The standalone duel (fresh HP) is won at
competence 1. Whatever lengthens the duel must also be measured in the chain, not only standalone. The day is 3.0 min
with the teach against the plan's 7-10 min.
