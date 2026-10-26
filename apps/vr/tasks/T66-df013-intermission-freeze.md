# T66 - DF-013 (a): freeze the kernel during intermissions

Status: built 2026-10-26, commit 4ac011d. `MageArena.` 335/335, `MageArenaDesign.` 5/7 (DF-011's two accepted reds).
Max turns: 60

## Goal
Owner ruling 2026-10-25 (`docs/DECISIONS.md`): option (a) of `docs/design-findings/DF-013-intermission-idle-steps-shift-bouts.md`.
The intermission's idle kernel steps moved the tick and the random stream that bouts 2-4 inherit, so the length of an
intermission changed the next fight. No kernel step now runs in an intermission.

## Change
`apps/vr/Game/Source/MageArenaVR/Session/SessionFlow.cpp`, `FArenaSession::AdvanceArc`, the offer / intermission / lost
`Pump`: `StepArena(Games.State, Inputs, nullptr)` runs only when `Games.Phase != "intermission"`. Offer and lost are as before.
No file under `Kernel/`, `apps/vr/data/` or any band changed.

## FreezeTeachClock and SeatOnActivePad
Both stay in the pump. `FreezeTeachClock` stamps the player's `WaveStartTick` with `Games.State.Tick`, `ClockAdvanceTicks` 0
and `Tier` 1. With the tick frozen it is idempotent: the first intermission frame stamps once, later frames change nothing.
(That first frame is why the test settles one frame before it takes its baseline.) The tier clock the next bout inherits is
the same as before: the tick it was stamped with is the last tick of the won bout. `SeatOnActivePad` only writes the player's
position from the active pad, so it is unaffected by the missing step. Nothing else was found that depended on the idle steps.

## Test
`MageArena.Intermission.FreezesKernel` (`Tests/IntermissionSceneTests.cpp`): the offer still steps the tick; in intermission 1
of a real day, after 600 more frames (10 s) `State.Tick` and `StateHash` are unchanged.
- Without the change (reverted locally, rebuilt): `Expected 'the kernel tick is frozen' to be 2, but it was 602` and
  `Expected 'the state hash is frozen' to be "e12821bd", but it was "f796be45"`.
- With it: Success.
The lost phase's pump is not driven by this test; it is the unchanged code path of the same lambda.

## Results
- `build.ps1`: `Result: Succeeded`.
- Corpus regenerated (`generate.mjs --corpus 30 --seed 1000`, `mousepaths.mjs`); `MageArena.` 335 Success, EXIT CODE 0.
- `MageArenaDesign.Session.FullSeated` (red only on DF-011's accepted bout-3 loss):

| | before (DF-013 table) | after |
|---|---|---|
| bouts rushed | 39.38 / 27.47 / 29.00 s | 39.38 / 27.50 / 42.50 s (lost bout 3) |
| bouts natural | 39.38 / 27.78 / 36.10 s | 39.38 / 27.50 / 42.50 s |
| `dayWithTeachS` | 212.31 | 225.83 |
| `dayNaturalS` | 305.72 | 311.83 |

  `Session.Wave1Seated`, `Session.CreaturesSeated`, `Duel.AirSeated`, `Census.*` green; `Duel.FireSeated` the other accepted red.
- Goldens: no recorded state hash moved (`OverlayGoldenTests`, `ReplayTests`, `StateHashCoverage`, `Invariants.*` all green,
  none re-recorded). No band or design number was re-recorded.
- D-G4 was not run (not in scope).
