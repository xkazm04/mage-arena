# DF-008 - Lio (Air) loses every duel in about 20-24 s

Found 2026-10-07 by T23. Status: open, owner decision needed. Census: `DF-008-census/` (20 seeds per set, Tiro final,
seated reference script, phases on).

| Set | Comp. | Median s | Win | Tempest at last phase | Tempest blinked / hit / missed |
|---|---|---|---|---|---|
| live | 1.0 | 23.27 | 1.00 | 20/20 | 7 / 0 / 13 |
| live | 1.5 | 24.09 | 1.00 | 20/20 | 8 / 0 / 12 |
| proposal | 1.0 | 20.92 | 1.00 | 20/20 | 5 / 0 / 15 |
| proposal | 1.5 | 21.17 | 1.00 | 20/20 | 9 / 0 / 11 |

Targets (DF-003): signature 100 % (met), median 45-80 s (missed), win >= 0.70 at c1 (met) and 0.40-0.60 at c1.5
(missed: 1.00). Cause: 53 of Lio's 54 casts in the live duel are Gale Darts, which the planted-staff dome cuts to about
1 damage each, and Lio never wards the player's bolts (0 blocks). The ten-row sheet (`docs/schools/AIR.md`, a proposal)
is not the problem as much as the AI's priorities and its missing defence.

## Options
- **A. AI first (recommended):** Lio wards like the Fire mage, uses Air Form against the player's bolts (not only
  "any threat"), and spends tier II-III (Veering Bolt, Squall, Downdraft) instead of darts once unlocked. Re-measure.
- **B. Sheet numbers:** Gale Dart damage 7 -> 9, or Air Form evade on magic 30 % -> 45 %.
- **C. The dome:** Air's area and line rows pass the dome's reduction today (Fire's do not, a pre-existing gap); decide
  one rule for both schools.
