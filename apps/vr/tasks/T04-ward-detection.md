# T04 - Palm ward detection and the perfect-absorb window

Status: retry 1 dispatched 2026-10-03
Max turns: 220

## Goal
The off-hand raised palm is the 140-degree directional ward; a **fresh** raise no more than 0.15 s before a magic hit
resolves is a **perfect** absorb (full block, mana back, tier clock +2 s). The defining risk is timing: the detector
must timestamp a raise by **motion onset** (joint velocity), not by when a pose classifier finally agrees, or the
perfect window is eaten by detection latency. Plan section 4 gate D-G3; rules from the pinned data:
`apps/vr/data/pinned/docs/design/baseline-fourteen-nights/design/data/combat.json` -> `absorb` (arcDeg 140, raiseCostMana 5,
minReleaseBeforeReRaiseS 0.12, perfect.windowS 0.15, perfect.notAgainst [unblockable, physical]). Read the numbers
from that file at runtime or via a generated header; never retype them.

## Read first
- `CLAUDE.md`, `docs/DECISIONS.md`, `docs/DESKTOP-INPUT.md`, `docs/CLIP-SCHEMA.md`, `docs/ORCHESTRATION.md`
- `docs/PROJECT-PLAN.md` section 4 (D-G3 row), section 6 (ward), `apps/vr/tasks/BACKLOG.md`
- `apps/vr/Game/Source/MageArenaVR/Hands/*`, `apps/vr/Game/Source/MageArenaVR/Gestures/*` (from T03), `apps/vr/tools/clipgen/*`

## Deliverables

### 1. Fix first - `UHandClipPlayer::Step` (apps/vr/tasks/BACKLOG.md)
After a clip ends, `Step` must not re-emit frames or advance the sequence counter unless a **hold** is active (below).
Add a regression test `MageArena.Clips.NoEmitAfterEnd`. Tick the backlog item.

### 2. Hold-type quick actions
`Space` (and right mouse button) must hold the ward for as long as the key is held: play `ward-raise` up to its hold
pose, keep emitting the hold pose **with fresh timestamps** while held (this is the legitimate continuous stream, not
the bug above), and on release play a short lowering motion (add `ward-lower.<variant>.jsonl` to the generator, or
reverse the raise). The key path and the test path call the same function.

### 3. Detector - `apps/vr/Game/Source/MageArenaVR/Gestures/WardDetector.*`
- Palm pose: palm normal from the palm/wrist/metacarpal keypoints; "raised" = palm normal within 35 degrees of the
  ward facing (forward from the seated origin, or toward the current threat when one is targeted) and hand height above
  a lap threshold. Hysteresis on both.
- **Onset timestamp:** when a raise is confirmed, walk back through the recent frames to the moment palm speed first
  exceeded the onset threshold; that time is the raise time used for the perfect rule (`FWardEvent.OnsetTime`).
- **Fresh raise rule:** a raise only counts as fresh if the ward was lowered for >= `minReleaseBeforeReRaiseS` before
  it. A permanently held ward can never produce a perfect.
- Events: `OnWardRaised(OnsetTime, Facing)`, `OnWardLowered`, broadcast by a subsystem like T03's.

### 4. Absorb resolver - `apps/vr/Game/Source/MageArenaVR/Combat/AbsorbResolver.*` (pure logic, no actors)
`Resolve(HitTime, HitKind {magic, physical, unblockable}, IncomingTier, HitDirection, WardState) -> {Reduction,
bPerfect, ManaReturned, TierClockAdvanceS}` using the pinned rules exactly (arc 140, reductions per kind, perfect
window 0.15 s measured from **OnsetTime**, not against physical or unblockable, mana formula with nerveRank = 0 for
now). This is the seed of the C++ kernel port; keep it data-driven and unit-tested.

### 5. Tests - `apps/vr/Game/Source/MageArenaVR/Tests/WardTests.cpp`
- `MageArena.Ward.OnsetCompensation`: generate ward-raise clips with injected detection latency 0, 20, 40, 60, 80,
  100 ms (shift when the pose becomes recognisable, keep the motion onset fixed); with a hit scheduled exactly at the
  authored perfect tick, the resolver reports `bPerfect` within +/- 1 frame of the authored tick for latency <= 60 ms.
  Report the table for all six latencies.
- `MageArena.Ward.HeldNeverPerfect`: a ward held for 10 s takes a stream of 3 magic bolts 0.2 s apart: 0 perfects,
  every bolt reduced by the `magic` reduction.
- `MageArena.Ward.KindsAndArc`: physical never perfect; unblockable never reduced; a hit from outside the 140-degree arc
  is not reduced.
- `MageArena.Ward.KeyHoldPath`: the `Space` hold path produces exactly one `OnWardRaised` and one `OnWardLowered`.
- Report: write `apps/vr/Game/Saved/Reports/ward-timing.json` (the latency table, per-test results).

## Constraints
- Do not commit, push, or touch any other repository. No paid services. Do not edit `docs/DECISIONS.md` or `data/`.
- Do not change the pinned numbers or widen the window. If onset compensation cannot meet +/-1 frame at <= 60 ms,
  report measured numbers and what you tried; the plan's fallback (window 0.20 s from data) is the owner's decision.
- One Unreal build at a time (`apps/vr/tools/build.ps1`). No binary assets.

## Acceptance (the orchestrator re-runs these; quote their real output in your report)
```
node apps/vr/tools/clipgen/generate.mjs && node apps/vr/tools/clipgen/validate.mjs apps/vr/Game/Clips      # 0 violations, deterministic
powershell -NoProfile -File apps/vr/tools/build.ps1                                         # Result: Succeeded
"C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "C:\Users\kazda\kiro\mage-arena-vr\apps\vr\Game\MageArenaVR.uproject" -ExecCmds="Automation RunTests MageArena;Quit" -unattended -nullrhi -nosplash -log
    # every MageArena.* test Success (Clips, Sigils, Ward), read apps/vr/Game/Saved/Logs/MageArenaVR.log
type apps\vr\Game\Saved\Reports\ward-timing.json
```

## Report
Write `runs/T04/REPORT.md`: files, acceptance outputs, the latency table, thresholds chosen (onset speed, angle,
hysteresis) and why, how the resolver reads the pinned data, anything you could not do with the exact error.

## Retry 1 (orchestrator, 2026-10-03)

All 12 `MageArena.*` tests pass and the latency table is good, but an independent review (items 1 and 3 verified in the
code by the orchestrator) shows real-hand failure modes the synthetic tests cannot see. Fix each AND add a test that
fails before the fix:

1. **Onset walks into the previous lowering** (`WardDetector.cpp:115-131`): `FindOnset` has no lower bound, so a quick
   re-raise latches onto the downward stroke and reports an onset *before* `LoweredTime` (the `KindsAndArc` short gap
   is -0.083 s, which makes its "not fresh" assertion pass for the wrong reason). Bound the walk-back at the last
   lowering. Test: rapid lower-then-raise clips; onset must be >= LoweredTime, freshness decided by the true gap.
2. **80 ms walk-back tail** (`WardDetector.h:24`): a real hand that decelerates and settles at the top for more than
   80 ms before confirmation returns `confirm - 0.08 s`, not the motion onset. Add a "settle" clip variant (rise, then
   0.15-0.30 s nearly still at the top, then confirm) and make onset still land within +/-1 frame for injected latency
   <= 60 ms. Choose the tail from that test, not by feel; report it.
3. **Starting already raised counts as fresh** (`WardDetector.cpp:209`): with `!bEverLowered` the first raise is always
   fresh, so a hand that enters tracking already in the ward pose can perfect. A raise is fresh only after an observed
   lowered state. Test: a clip that starts in the raised pose and stays there under a bolt stream: 0 perfects.
4. **Hold resume after a finished lowering** (`HandClipPlayer.cpp:84-87`): `CanResumeHold` returns true when the clip
   stopped, so the next Space press resumes instead of reloading (and ignores a new Shift/Ctrl variant). Test: hold,
   release fully, hold again with Shift - the sloppy clip plays.
5. **Space consumed by Enhanced Input** (`MageArenaPlayerController.cpp:~144`): with `bConsumeInput = true` the
   held-key polling in `PlayerTick` never sees Space, so aim does not follow the view while held, and releasing RMB while
   Space is held drops the ward. Fix and cover with a test of the hold-state logic (no rendering needed).
6. **Degenerate hit direction counts as inside the arc** (`AbsorbResolver.cpp:~128`): a hit with zero horizontal
   component returns "inside". Treat it as outside (or resolve by the 3D direction) and assert it in `KindsAndArc`.

Keep the 0.15 s window and every existing test. Report before/after numbers, the new tests, and the chosen tail.
