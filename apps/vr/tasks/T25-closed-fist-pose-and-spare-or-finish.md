# T25 - Closed-fist held pose, and spare or finish at missio, writing the arena flag

Status: **done** on Wolf (kaziho) 2026-10-24, UE 5.8.3 CL 58210709, commits `3ca5590` (fist clips), `3a9f278`
(FistDetector and pose tests), `f33a77f` (missio phase and flags), `9800cc1` (desktop key G), `b13cbb7` (house format;
the tested tree). `Result: Succeeded`; `MageArena.Fist` 9/9 (red first, see Result); `MageArena.` 282/282 (45/45
Conformance); `MageArenaDesign.` 5/7, only DF-011's two accepted reds. See "Result" at the end. (Was: open, written
2026-10-20 on the ARM64 laptop.)
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

## Result (Wolf kaziho, 2026-10-24, UE 5.8.3 CL 58210709)

`runs/` is never committed, so the report is here.

**Built.**
- `Gestures/FistDetector.{h,cpp}`: `FFistDetector`, pure (no actor, no subsystem), fed hand frames, each hand on its own
  clock. A fist is every finger's summed flexion >= 160 degrees (release < 140) and the thumb tip within 5.5 cm of the
  index and middle second knuckles (release 6.5 cm), held `FFistThresholds::HoldS` 0.40 s (teach.json `offerHoldS`).
  One count per hold; a rewind, a frame under confidence 0.2 or the system gesture ends it. `IsOpenHand` (every finger
  under 60 degrees) is the spare's side. Thresholds are measured off the clipgen hands and cited in the header.
- `Session/`: the `missio` phase (`EnterMissio`, `AdvanceMissio`, `ResolveMissio` in `DayFlow.cpp`). In the day, a bout
  won against a bound rival (Brennic, wave n 3; Lio, wave n 4) is recorded, then waits in missio before its
  intermission or aftermath. An open palm raised toward the arena held 0.40 s spares, a fist held 0.40 s finishes,
  no choice in 5 s spares. The kernel does not step in missio; no combat number or kernel result changes.
- Clips: `apps/vr/Game/Clips/fist/fist.{normal,slow,sloppy}.jsonl` and the impostor `fist-loose.normal.jsonl`, from
  the new `apps/vr/tools/clipgen/fist.mjs --canonical` (writes `Clips/fist/` only). `generate.mjs --check` now
  regenerates and compares that folder: `clipgen-check: OK, 81 clips byte-identical`. `corpus-manifest.json` lists the
  four clips; `check-staged-clips.mjs`: `staged clips OK: 115 files`.
- Desktop: `G` plays the `fist` clip (Shift/Ctrl takes); the spare is the held ward palm (`Space`, RMB).
  `docs/DESKTOP-INPUT.md` has both rows.
- `Tests/FistTests.cpp`: `MageArena.Fist.Takes`, `.ImpostorRejected`, `.OpenPalmNeverFist`, `.NeverWard`,
  `.HoldMatchesStones`, `.MissioSpare`, `.MissioFinish`, `.MissioTimeout`, `.NoChoiceMidBout`.

**Flag keys** (through `FArenaFlags`, the keys this card and `docs/campaign/04-vr-campaign-design.md` section 4 name):
`arena.tiro.1.w<n>.finish` (bool, true = finish) and `arena.missio.total` (number).

**Red line, before the detector and the phase** (Ingest stub, no missio): `MageArena.Fist` 3/9, e.g.
`Expected 'fist.normal is one fist held 0.40 s' to be 1, but it was 0.` and
`Expected 'Brennic falls: missio' to be "missio", but it was "intermission".`

**Acceptance on `b13cbb7`.**
- `powershell -NoProfile -ExecutionPolicy Bypass -File apps/vr/tools/build.ps1`: `Result: Succeeded`.
- `Automation RunTests MageArena.Fist`: 9/9 green.
- `Automation RunTests MageArena.`: 282 run, 0 failed (273 before T25 plus the nine), 45/45 `Kernel.Conformance`. Five
  pass with warnings, all pre-existing negative-path logs (corrupt-save fixtures, an HTTP probe timeout).
  `Session.DayOrderFirstLaunch`, `Session.DaySaveEachBout` and `Collar.TabletColumns` now pass through missio.
- `Automation RunTests MageArenaDesign.`: 5/7; red only `Duel.FireSeated` (brennic-c15 reaches 1 of 2 phases) and
  `Session.FullSeated` (loses bout 3), both accepted under DF-011. FullSeated measured `dayWithTeachS=187.85`: it loses
  bout 3 before any missio, so T25 adds nothing to that day.
- `format-check.mjs --base 3350dc5`: `format-check OK: 12 changed C++ file(s)`. `unused-members.mjs`:
  `unused-members OK: 586 private/protected members in 41 classes`.
- Node gates: `pin OK: 9 files @ baeac66`, `conformance OK: 45 vectors`, both self-tests detect corruption, agy-guard
  40/40, `repo-law OK: 4/4`.

**Decisions this card did not specify.**
- `arena.missio.total` counts the rivals spared (missio granted) this day, recounted from the `.finish` flags, so a
  replayed bout is not counted twice. A finish adds 0, but the key is written on every choice. The campaign doc's
  decision 5 ("Missio counts become a flag") may instead mean the player's own defeats. That reading is the owner's.
- Missio follows only bouts with a bound rival (`BoundRival()`), and only in the day (`BeginArc`). `Start()` bouts are
  unchanged.
- The palm is the ward check (height >= 0.20 m, palm within 35 degrees of forward) plus an open hand, on either hand.
  Each hand counts only after it was seen lowered in missio (a clip's last frame stays the hand's latest). The fist
  counts on either hand. The fist detector is reset and the hands stopped on entry, so a fist from the bout does not
  carry over.
- The 5 s timeout is a named constant (`MissioTimeoutS`, DayFlow.cpp), not data: `apps/vr/data/` was outside this
  run's paths.
- The script makes no choice: scripted days spare through the timeout (+5 s per rival won).
- The fist takes are on the ward hand (left), thumb up, so a fist is never a ward. They are not mirrored (like the
  stones): either hand may close the fist.

**Not done / open.**
- No prompt line in missio: `GetPromptText` returns nothing there, because a `strings.json` line
  (`apps/vr/data/vr/strings.json`) was outside this run's paths. The greybox shows no missio cue yet; presentation is
  for T27/T28.
- The staff grip (staff-plant, staff-lift) reads as a fist. Only missio reads the detector, and no staff is held there.
- A quit during the final's missio replays the final (the save still offers it), as a quit in the intermission
  replays the bout.
