# T24 - Mudra seal detector: a 1.5 s data window that gates the tier IV cast

Status: open, blocked on C1 (the seal shape, a deliberate design by Claude + owner, `docs/design/V2-ROADMAP.md` lane B
row 8; not a worker task). Runs on the build machine (Wolf, the x64 UE host): UE 5.8.3 editor and the automation suite.
Written 2026-10-20 on the ARM64 laptop, where none of it runs. Start only when C1 is committed.
Max turns: 160

## Goal
The tier IV cast needs a seal: after the line sigil at tier IV, both hands form the mudra within 1.5 s, and only then does
the tier IV spell release (`docs/PROJECT-PLAN.md` section 5 "Seal (tier IV)"; roadmap row 9). Today the mudra has no detector
(`docs/design/V2-VISION.md` line 47) and the glyph is an arch traced from a synthetic clip (BACKLOG, A03 review). This card
adds a pose detector that reads the C1 shape through the one gesture pipeline (`HandInputSubsystem`, like `WardDetector` and
`StaffDetector`) and makes the seal gate the tier IV cast. It moves the "gameplay" part of K3 row 15 (owner verdict, felt;
`docs/research/KPI-2026-10-20.md`) and delivers one of the four signature moments of the slice. K3 has no mechanical row for
it, so the proof is the suite and the clip replay.

## Read first
- `CLAUDE.md`, `docs/DECISIONS.md` (they outrank this card if they conflict)
- C1 (the seal shape) once written: its path is the first line of this card's report; without it, stop and report blocked
- `docs/DESKTOP-INPUT.md` (tier IV mudra is key `4`, played as a recorded two-hand clip; no shortcut may skip the pipeline)
- `docs/PROJECT-PLAN.md` section 5, the "Seal (tier IV)" row; `docs/design/V2-VISION.md` lines 115 and 127
- Code: `Gestures/WardDetector.{h,cpp}`, `Gestures/StaffDetector.{h,cpp}` (pose detectors to copy the shape of),
  `Gestures/HeldPointRate.h`, `Hands/HandInputSubsystem.{h,cpp}`, `Session/ArenaSession.{h,cpp}` (where the tier IV cast is
  released), `Tests/WardTests.cpp`, `Tests/HandClipTests.cpp`, `Tests/SystemGestureTests.cpp`
- Clips: the existing synthetic mudra clip under `apps/vr/Game/Clips/` and `apps/vr/tools/clipgen/`

## Deliverables (exact paths)
- `apps/vr/Game/Source/MageArenaVR/Gestures/MudraDetector.{h,cpp}` - a detector over the two-hand joint frames that matches the
  C1 shape, with a data window of 1.5 s from the line sigil's completion; thresholds in a named struct, each with the C1
  citation; pure and tickable without an actor, like `WardDetector`
- The binding in `Hands/HandInputSubsystem` (the same event path as the ward; a mudra stops the ward hand, as the header
  already states) and the gate in `Session/ArenaSession.cpp`: at tier IV the cast releases only on a seal inside the window;
  no seal in 1.5 s drops the charged sigil through the existing cancel path. Kernel data is not touched.
- Clips under `apps/vr/Game/Clips/` from `apps/vr/tools/clipgen/` for the C1 shape: normal, slow and sloppy takes, plus
  three impostor poses (open palms, fists, one hand only) that must not seal. Only add files; never run `generate.mjs` bare
  or with `--templates` (it rewrites tracked clips, `CLAUDE.md`)
- `apps/vr/Game/Source/MageArenaVR/Tests/MudraTests.cpp` - `MageArena.Mudra.*`: normal, slow and sloppy takes seal; the
  impostors do not; a seal at 1.4 s casts and one at 1.6 s does not; a seal with no line sigil does nothing
- The glyph `apps/vr/art/glyphs/sigil-mudra*` redrawn from the C1 shape only if C1 says so; otherwise left alone
- `docs/DESKTOP-INPUT.md`: update the tier IV row if the clip or key changes

## Constraints
- Do not commit, push, or touch any repository other than this one.
- Do not call paid services. Do not edit `docs/DECISIONS.md`, `apps/vr/data/pinned/` or the conformance vectors.
- Do not decide the seal shape: it comes from C1. If C1 is missing or ambiguous, stop and report.
- No shortcut around the pipeline: the desktop build plays the seal as a clip through `HandInputSubsystem`.
- Combat numbers do not change; the window (1.5 s) is the roadmap's, any other value is a change request.
- Fresh checkout: regenerate the sigil corpus first (`CLAUDE.md`, Build). One Unreal build at a time; one
  `Automation RunTests` filter per editor process.
- Baseline the full `MageArena.` and `MageArenaDesign.` suites before the change and compare per test after. Known red on a
  clean tree (`CLAUDE.md`): `MageArenaDesign.Duel.FireSeated`, `Duel.AirSeated`, `Session.FullSeated`, `Session.Wave1Seated`,
  `Session.CreaturesSeated`. Any other change is a regression. (If T39 has landed, use its recorded baseline instead.)
- Changed C++ lines pass `node apps/vr/tools/lint/format-check.mjs`; new members pass `node apps/vr/tools/lint/unused-members.mjs`.

## Acceptance (the orchestrator re-runs these; quote their real output in your report)
```
powershell -NoProfile -File apps/vr/tools/build.ps1                     # expected: exit 0
UnrealEditor-Cmd MageArenaVR.uproject -ExecCmds="Automation RunTests MageArena.Mudra;Quit" -unattended -nullrhi -nosplash -log
                                                                         # expected: all MageArena.Mudra.* green
UnrealEditor-Cmd MageArenaVR.uproject -ExecCmds="Automation RunTests MageArena.;Quit" -unattended -nullrhi -nosplash -log
                                                                         # expected: all MageArena. green
UnrealEditor-Cmd MageArenaVR.uproject -ExecCmds="Automation RunTests MageArenaDesign.;Quit" -unattended -nullrhi -nosplash -log
                                                                         # expected: no red beyond the known list
node apps/vr/tools/lint/format-check.mjs && node apps/vr/tools/lint/unused-members.mjs
                                                                         # expected: OK
```
Quote the red line of `MageArena.Mudra` seen before the detector exists (tests written first).

## Report
Write `runs/T24/REPORT.md`: the C1 path and the shape as implemented, what you built (paths), each acceptance command with
its real output, the red line, anything you could not do and the exact error, and decisions the card did not specify.
