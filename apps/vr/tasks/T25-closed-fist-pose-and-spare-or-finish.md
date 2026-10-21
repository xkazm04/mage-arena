# T25 - Closed-fist held pose, and spare or finish at missio, writing the arena flag

Status: open (needs the build machine, Wolf, the x64 UE host: UE 5.8.3 editor and the automation suite; written
2026-10-20 on the ARM64 laptop, where none of it runs)
Max turns: 160

## Goal
When a rival falls to 0 HP the bout ends in "missio" (nobody dies below the Summa, `docs/campaign/04-vr-campaign-design.md`
section 4 and decision 7). The player then chooses: an open palm held over the opponent spares, a closed fist held finishes
(roadmap lane B row 10). This card adds the closed-fist held pose to the gesture pipeline and the missio choice, and writes
the result to the arena flag map for the story layer to read. It moves the "gameplay" part of K3 row 15 (owner verdict,
felt; `docs/research/KPI-2026-10-20.md`); K3 has no mechanical row for it, so the proof is the suite and the clip replay.

## Read first
- `CLAUDE.md`, `docs/DECISIONS.md` (they outrank this card if they conflict)
- `docs/design/V2-ROADMAP.md` lane B row 10; `docs/campaign/04-vr-campaign-design.md` sections 3-4 (no choice during a bout;
  the arena flags; `arena.finisher` and `arena.missio.total` as models for a result flag)
- `docs/DESKTOP-INPUT.md` (every pose is a recorded clip through the one pipeline; the palm over a stone is held 0.40 s)
- Code: `Gestures/StaffDetector.{h,cpp}` and `Gestures/WardDetector.{h,cpp}` (held-pose detectors), `Hands/HandInputSubsystem`,
  `Session/ArenaFlags.{h,cpp}` (`SetBool`, `SetNumber`, `TiroKey`), `Session/ArenaSession.{h,cpp}` and `Session/SessionFlow.cpp`
  (phases, `AreStoryStonesShown`, the lost and aftermath handling), `Kernel/Games.{h,cpp}`, `Tests/SessionTests.cpp`,
  `Tests/HandClipTests.cpp`

## Deliverables (exact paths)
- `apps/vr/Game/Source/MageArenaVR/Gestures/FistDetector.{h,cpp}` - closed-fist held pose (all four fingers curled, thumb
  over them), held 0.40 s like the stone hold; thresholds named and cited; pure and tickable without an actor
- A missio choice phase in `Session/` after a rival falls: open palm held 0.40 s = spare, fist held 0.40 s = finish; no
  choice in 5 s = spare (the default never blocks). It is not reachable during a bout. The result is written with the existing
  flag writer: `arena.tiro.1.w<n>.finish` (bool, true = finish) and the running count `arena.missio.total` (number); if the
  campaign doc names other keys, use those and say so
- Clips for the fist under `apps/vr/Game/Clips/`: normal, slow and sloppy takes plus an impostor (a loose half-curl) that must
  not count, generated through `apps/vr/tools/clipgen/` (add files only; never run `generate.mjs` bare or with
  `--templates`). A desktop key for it in `docs/DESKTOP-INPUT.md`, played as a clip, never as a direct event
- `apps/vr/Game/Source/MageArenaVR/Tests/FistTests.cpp` - `MageArena.Fist.*`: pose detected on the three takes, impostor not;
  spare, finish and timeout each write the expected flag; no flag is written mid-bout; an open palm over a stone is never
  read as a fist and a fist is never read as a ward

## Constraints
- Do not commit, push, or touch any repository other than this one.
- Do not call paid services. Do not edit `docs/DECISIONS.md`, `apps/vr/data/pinned/` or the conformance vectors.
- Spare or finish changes no combat number and no kernel result; it is a flag and a presentation beat. If a consumer needs a
  kernel change, stop and file a change request instead.
- Fresh checkout: regenerate the sigil corpus first (`CLAUDE.md`, Build). One Unreal build at a time; one
  `Automation RunTests` filter per editor process.
- Baseline `MageArena.` and `MageArenaDesign.` before the change and compare per test after. Known red on a clean tree
  (`CLAUDE.md`): `MageArenaDesign.Duel.FireSeated`, `Duel.AirSeated`, `Session.FullSeated`, `Session.Wave1Seated`,
  `Session.CreaturesSeated`. Any other change is a regression. (If T39 has landed, use its recorded baseline.)
- Changed C++ lines pass `node apps/vr/tools/lint/format-check.mjs`; new members pass `node apps/vr/tools/lint/unused-members.mjs`.

## Acceptance (the orchestrator re-runs these; quote their real output in your report)
```
powershell -NoProfile -File apps/vr/tools/build.ps1                     # expected: exit 0
UnrealEditor-Cmd MageArenaVR.uproject -ExecCmds="Automation RunTests MageArena.Fist;Quit" -unattended -nullrhi -nosplash -log
                                                                         # expected: all MageArena.Fist.* green
UnrealEditor-Cmd MageArenaVR.uproject -ExecCmds="Automation RunTests MageArena.;Quit" -unattended -nullrhi -nosplash -log
                                                                         # expected: all MageArena. green
UnrealEditor-Cmd MageArenaVR.uproject -ExecCmds="Automation RunTests MageArenaDesign.;Quit" -unattended -nullrhi -nosplash -log
                                                                         # expected: no red beyond the known list
node apps/vr/tools/lint/format-check.mjs && node apps/vr/tools/lint/unused-members.mjs
                                                                         # expected: OK
```
Quote the red line of `MageArena.Fist` seen before the detector exists (tests written first).

## Report
Write `runs/T25/REPORT.md`: what you built (paths), the flag keys written, each acceptance command with its real output,
the red line, anything you could not do and the exact error, and decisions the card did not specify.
