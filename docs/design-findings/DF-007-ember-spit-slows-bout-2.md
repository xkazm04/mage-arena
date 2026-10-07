# DF-007 - Ember spit makes Bout 2 perfect practice, and slower than its window

Found 2026-10-07 by T18. Status: open, owner decision needed.

| Run | CreaturesSeated (pinned window 30-45 s) | Perfects | Damage taken |
|---|---|---|---|
| live, before T18 | 33.05 s inside | 0 | 14.0 |
| live, after | **52.02 s outside**, won with 31.7 HP | **11** | 63.4 |
| proposal, before | 27.30 s outside | 0 | 14.0 |
| proposal, after | 31.02 s inside | **0** | 11.0 |

DF-004 is closed (the hounds now teach perfects), but the reference seat stops bolting to ward each ember, so the live
bout runs about 7 s past the window. On the proposal the 14 s cheap dome keeps the seat planted, and a planted seat
does not ward, so no perfects at all: the dome and the perfect practice compete.

## Options
- **A. Ward some embers, not all:** change the reference script (not the game) to perfect every second ember; it is a
  measure of a reasonable player, not a perfectionist.
- **B. Slower spit cadence:** 1.5 s instead of the pinned 0.4 s recovery plus windup (VR overlay).
- **C. Shorter dome on the proposal** so the seat leaves it during Bout 2 (DF-003 knob).
- **D. Widen the VR window for Bout 2** to 30-55 s (pacing target change, VR only).
Recommendation: A and B together; re-measure.
