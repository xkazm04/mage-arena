# DF-005 - The split latch clear cannot bring perfects back inside one ward hold

Found 2026-10-07 by T17. Status: open, owner decision needed.

The owner's rule (DECISIONS 2026-10-07, split hands): "the perfect latch clears after the casting hand is idle about
0.3 s", so that perfects return while the ward stays up. T17 built it (18 ticks at 60 Hz). But a perfect needs a
**fresh** ward: pinned `combat.json` `absorb.perfectWindowS` is 0.15 s (9 ticks), counted from the ward raise. A split
cast needs the ward already up, so the earliest clear (18 ticks after the last cast) is always outside the window. In
play the clear changes nothing except the split ward drain multiplier (`wardDrainSplit`: 1 in the proposal, absent live).
No design run triggered it (the reference script drops the ward before 18 idle ticks).

## Options
- **A. Re-arm on clear (recommended):** when the latch clears, the ward counts as fresh again for one
  `perfectWindowS`, so a player who stops casting and waits about 0.3 s can perfect the next shot without dropping the
  ward. VR overlay only. Risk: a player could alternate casting and re-arming; the 18-tick wait bounds it.
- **B. Leave it:** dropping and re-raising the ward is the only way back to perfects (today's rule, plus the drain).
- **C. Longer window while split-casting:** a VR-only perfect window measured from the latch clear, separate from the
  pinned one.
