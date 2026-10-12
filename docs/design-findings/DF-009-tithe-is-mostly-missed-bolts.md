# DF-009 - Tithe is mostly missed bolts; Cracks come from one bout

Found 2026-10-07 by T22. Status: open, owner decision needed (the `collar.json` weights are a proposal).

| Run | Bout | Outcome | Cracks | Tithe |
|---|---|---|---|---|
| live, CreaturesSeated | 2 | won 45.02 s | 18 (perfect 8, reflect 6, reflected hit 4) | 3 (missed) |
| proposal, FullSeated | 1 | won 39.38 s | 0 | 12 (missed) |
| proposal, FullSeated | 2 | won 27.23 s | 0 | 0 |
| proposal, FullSeated | 3 | lost 35.82 s | 0 | 81 (missed) |

The reference script misses Brennic 81 times in 36 s, so with `missedCast` 1 the Wardstones sit at full glow (30) for
most of any mage bout, and Tithe stops meaning "spectacle". The script never blinks through an unblockable, never casts
a tier III-IV area spell and never spends a Crest, so those sources are untested in play.

## Options
- **A. Count only spectacle (recommended):** drop `missedCast` to 0 for Bolts (tier 0) and keep it for tier I+ casts;
  Tithe then tracks the loud play the vision meant (area casts, Crests, wasted line spells).
- **B. Keep the weight, raise the Wardstones' full glow** from 30 to about 100.
- **C. Teach the reference script** to blink unblockables and spend Crests, so the census measures every source.
