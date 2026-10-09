# T35 - One fixed 72 Hz input clock with epochs, and onset-keyed kernel input

Status: open (needs the build machine: UE 5.8 is not installed on the ARM64 laptop)
Max turns: 320

## Goal
`UHandInputSubsystem` owns input time: every hand source (ward lane, action lane, mouse, later the live headset) emits on one monotonic 72 Hz clock with explicit epochs. The four detectors stop inferring gesture boundaries from clock rewinds, and ward intents carry their motion-onset stamp into the kernel through a declared input delay D (a VR overlay field; D = 0 reproduces today byte for byte). Gesture outcomes stop depending on the desktop's frame rate, and the headset stream (which never rewinds) plugs into a contract that already exists.

Determinism package, accepted by the owner 2026-10-09 (`docs/research/MOONSHOT-2026-10.md`, item M3).
Run order: **T16, then (T33 -> T34) and (T35 -> T36)**. T35 waits for T16.

## Read first
- `CLAUDE.md`, `docs/DECISIONS.md` (they outrank this card if they conflict)
- `docs/research/MOONSHOT-2026-10.md` section **M3 - Put all hand sources on one fixed 72 Hz input clock and feed the kernel by onset**: the full spec (Summary, Description, Flow, Write set,
  Acceptance, Evidence). This card adds only the run rules; where they differ, this card wins.
- Re-verify every premise in that section against the tree before writing code. The section was read on `83372e7`;
  a premise that no longer holds is reported, not built on.
- T16 committed (it changes `Session/ArenaSession.cpp`).

## Deliverables (exact paths)
The write set of the deck section, plus `runs/T35/REPORT.md` (uncommitted).

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
- D defaults to 0, and the D = 0 event logs are byte-identical to the baseline. The D sweep (0, 2, 4 ticks) is a
  table for the owner in the report; this card does not choose D.
- No kernel or combat-number change. If a D large enough to cover detection latency costs too much feedback
  latency, write that up as a candidate change request (an onset field in the kernel InputFrame); do not file it.

## Acceptance (the orchestrator re-runs these; quote their real output in your report)
```
powershell -NoProfile -File apps/vr/tools/build.ps1          # expected: the build log shows "Result: Succeeded"
UnrealEditor-Cmd ... "Automation RunTests MageArena.Input.Clock"           # expected: render-rate, hitch, monotonic, epoch, phase-order, onset, mouse-on-bus pass
git grep -c "Rewind\|rewinds the stamp" -- apps/vr/Game/Source/MageArenaVR/Gestures   # expected: 0
UnrealEditor-Cmd ... "Automation RunTests MageArena.Ward" (then .Blink, .Sigils, .HeldSample)   # expected: identical to baseline
```

## Report
Write `runs/T35/REPORT.md`: base sha; commits; per acceptance case the red evidence and the green evidence (real
lines); the per-test suite comparison before/after; files outside the write set; premises that turned out false; what
is not done and why.
