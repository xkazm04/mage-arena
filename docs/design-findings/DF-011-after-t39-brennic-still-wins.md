# DF-011 - After T39 the seat still loses to Brennic, and both school duels run short

Found 2026-10-24 on Wolf (hostname `kaziho`, UE 5.8.3 CL 58210709) by T39's one `MageArenaDesign` run on the final
overlay, branch commit `6f00555`. Status: **OPEN, needs an owner ruling.** Nothing in this file is decided in code.
Raw logs: `runs/T39/final/` (gitignored); report: `runs/T39/REPORT.md`.

T39 built every recommended option of DF-003, DF-005, DF-006 (A + B), DF-007 (A + B), DF-008 (A) and DF-009 (A), as
ruled on 2026-10-20. Two of the five known design reds are now green (`Session.Wave1Seated`, `Session.CreaturesSeated`).
Three stay red, and no accepted DF explains them:

| Test | Before T39 (Wolf baseline, `e321455`) | After T39 (`6f00555`) | Target |
|---|---|---|---|
| `Duel.FireSeated` brennic-c1 | lost, 32.88 s, dealt 88.1 | lost, 38.32 s, dealt 73.8, Sunfall cast, 2 breaks | won, 45-80 s |
| `Duel.FireSeated` brennic-c15 | lost, 22.92 s | lost, 37.50 s, 1 break | 45-80 s, reaches the last phase |
| `Duel.AirSeated` lio-c15 | won, 26.42 s, Lio 0 blocks | won, 23.12 s, Lio 4 blocks, 2 perfects | 45-80 s |
| `Session.FullSeated` | lost bout 1 at 23.25 s | won bouts 1-2 (39.38 s, 27.50 s), lost bout 3 (Brennic) at 42.50 s | four bouts won |
| `Census.Rivals` live c1 / c1.5 | median 31.23 / 24.43 s, win 0.65 / 0.15 | median 43.47 / 39.39 s, win 0.50 / 0.25 | 45-80 s, win >= 0.70 / 0.40-0.60 |

`FullSeated` also fails its day-length line (187.85 s with the teach); that one is DF-010's (ruled E: it leaves the
gate in T47) and is not part of this finding.

## What the numbers say

- **Brennic.** The wide set (DF-003/006 A) plus the phase-break beat (DF-006 B) lengthen his duel by about 12 s at c1
  (31.23 -> 43.47 s census median) but cost the player win rate: c1 0.65 -> 0.50. Without the phases the same overlay
  wins 0.95 at c1 (`live-off` row, 44.83 s). The beat makes him untouchable for 3 s at each break, and the granted
  Sunfall then lands after it; the seated script spends those 6 s of bolts on a warded target. The chained day loses
  exactly where DF-006's addendum predicted: bout 3, carrying bout 2's damage.
- **Lio.** DF-008 A works as built (Lio now blocks and perfects the seat's bolts; before T39 the pinned reaction rule
  could not see a bolt at all), but the duel got shorter, not longer: the seat still kills him in 23 s. One variant of
  the new `wardManaFloor` (30 instead of 15) gave 22.67 s, so that knob is not the lever.
- **Bout 2.** Green at 31.02 s, but with 0 perfects: on the wide set the seat plants its 14 s dome, and a planted seat
  does not ward embers, so DF-007 A's "ward every second ember" never fires. This is DF-007's own "proposal, after" row
  (31.02 s, 0 perfects) reproduced; it is noted here, not a red.

## Options

- **A. Accept and measure.** Keep T39 as built. `Duel.FireSeated`, `Duel.AirSeated` and `FullSeated`'s bout-3 loss stay
  red as known failures until content or a later card moves them. Costs nothing; the 31 Oct gate row for the duels
  stays "not met".
- **B. A beat that only silences.** During DF-006 B's beat Brennic stops casting (the taunt) but is not warded, so the
  seat's bolts land. Expected: higher win rate, a little shorter. It narrows the ruled DF-006 B wording ("warded"), so
  it needs the owner.
- **C. Teach the reference script the unblockables.** The script never blinks Sunfall or Tempest deliberately (DF-009
  C noted the same gap). A measurement fix, no game number moves; re-measure after. It may not be enough alone.
- **D. Lio's sheet numbers (DF-008 B: Gale Dart 7 -> 9, or Air Form evade 30 % -> 45 %).** Rejected on 2026-10-20;
  re-opening it is the owner's call. It is the only lever on the table that lengthens the Air duel.

## Recommendation

**C first, then B if Brennic is still red; ask again on D.** C changes only the measuring stick, so it can run at
once and tells whether the Brennic red is the game or the script. B is the smallest game change that keeps the beat's
stage. For Lio no non-number lever is left: the owner either re-opens DF-008 B (D) or accepts A for the Air duel.
