# T36 - Session logic on the sim tick, and an input journal for exact replay

Status: open (needs the build machine: UE 5.8 is not installed on the ARM64 laptop)
Max turns: 400

## Goal
All session logic (the reference script, the teach, day clocks, holds, tracking loss, the resume count) runs once per 60 Hz sim tick, and a session is recorded as a tick-stamped journal of what entered the gesture pipeline (hand frames, mouse samples, view aim, platform notices). A journal replays a whole day headless and bit-identically at any render rate through the same detectors, so a device session can be reproduced on the desktop and the census measures the game, not the frame rate.

Determinism package, accepted by the owner 2026-10-09 (`docs/research/MOONSHOT-2026-10.md`, item M4).
Run order: **T16, then (T33 -> T34) and (T35 -> T36)**. T36 follows T35.

## Read first
- `CLAUDE.md`, `docs/DECISIONS.md` (they outrank this card if they conflict)
- `docs/research/MOONSHOT-2026-10.md` section **M4 - Step the whole session on the sim tick and journal its inputs for exact replay**: the full spec (Summary, Description, Flow, Write set,
  Acceptance, Evidence). This card adds only the run rules; where they differ, this card wins.
- Re-verify every premise in that section against the tree before writing code. The section was read on `83372e7`;
  a premise that no longer holds is reported, not built on.
- T35 committed (the clock this card steps on); T16's event log (this card generalises it).
- `docs/research/ARCHITECTURE-REVIEW-2026-10.md` N3 (the reference script inside `FArenaSession`).

## Deliverables (exact paths)
The write set of the deck section, plus `runs/T36/REPORT.md` (uncommitted).

## Constraints
- Do not commit, push, or touch any repository other than this one.
- Do not call paid services. Do not edit `docs/DECISIONS.md`, `apps/vr/data/pinned/` or any TV combat number.
- **Test first.** Every acceptance case is written as a test (or a counted lint) and seen RED on the unchanged code
  before the change; the report quotes the red line. A case that never failed proves nothing.
- Baseline the full `MageArena.` and `MageArenaDesign.` suites before the change and compare per test after. Known red
  on a clean tree: `MageArenaDesign.Duel.FireSeated`, `Session.FullSeated`, `Session.Wave1Seated`. Any other change
  is a regression unless this card names it.
- Fresh checkout: regenerate the sigil corpus first (`CLAUDE.md`, Build). One Unreal build at a time
  (`-WaitMutex`); one `Automation RunTests` filter per editor process; read verdicts from `-ReportExportPath`.
- Stay inside the section's write set; list any file outside it in the report with the reason.
- `apps/vr/tools/build.ps1` exits 0 when `Build.bat` is missing (found 2026-10-09): confirm the build really ran
  from its log, not from the exit code.
- **First commit, before any tick-loop work: minimal N3.** Move `DecideScript` into `FScriptedPlayer`
  (`Session/ScriptedPlayer.*`), reading the session through a const view and acting only through `PlayAction`.
  Behaviour unchanged: the FullSeated chain and final StateHash at Dt 1/72 equal the baseline exactly.
- Run `MageArena.Session.FrameRateInvariance` on the unchanged tree first and quote the result. It is the
  instrument: if it passes before the change, the frame-rate half of the claim is false and the report says so.
- The journal stores inputs, never gesture events, so replay always runs the gesture pipeline (CLAUDE.md).
- Census numbers measured through the script are re-taken once on the tick loop; list before/after per open DF
  item in the report for the owner.

## Acceptance (the orchestrator re-runs these; quote their real output in your report)
```
powershell -NoProfile -File apps/vr/tools/build.ps1          # expected: the build log shows "Result: Succeeded"
UnrealEditor-Cmd ... "Automation RunTests MageArena.Session.FrameRateInvariance"   # expected: red on base (quoted), green after
UnrealEditor-Cmd ... "Automation RunTests MageArena.Journal"               # expected: RoundTrip, PipelineInLoop, PlatformNotices, FailClosed pass
git grep -c "+= DeltaSeconds" -- apps/vr/Game/Source/MageArenaVR/Session/SessionFlow.cpp apps/vr/Game/Source/MageArenaVR/Session/DayFlow.cpp   # expected: 0
```

## Report
Write `runs/T36/REPORT.md`: base sha; commits; per acceptance case the red evidence and the green evidence (real
lines); the per-test suite comparison before/after; files outside the write set; premises that turned out false; what
is not done and why.
