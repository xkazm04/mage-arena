# T45 - Capture evidence integrity: a driver must not be able to report success without capturing

Status: **done** on Wolf (kaziho) 2026-10-24, UE 5.8.3 CL 58210709, commits `9e42cbe` (E), `1612780` (shared helper,
eight drivers), `191cad0` (StylePick, tests, style-pick launcher), `f92230e` (colour-audio), `197412c` (tolerance note);
see "Result" at the end. `Result: Succeeded`; `MageArena.` 272/272 with the six `MageArena.Capture.*`;
`MageArenaDesign.` 5/7, only DF-011's two accepted reds; the three negative proofs fail as required. Two readings differ
from this card: `QUIT_EXEC` does quit (A3), and digital silence measures -91 dB, which the old guard did reject (C4).
The T16 event log changed for the same seed and script (a finding, see Result). Was: open, written 2026-10-21 on Lynx,
the ARM64 laptop, where no Unreal install and no renderer exist - `docs/DECISIONS.md` 2026-10-20 "Unreal host"
Max turns: 300

## Goal
Every capture harness either produces the evidence it claims or fails loudly. Today three of them can report success
having captured nothing: `UStylePickCaptureDriver` logs **no** `MAGEVR_CAPTURE_DONE` and **no** `MAGEVR_CAPTURE_FAIL`
at all, so a duel that ends at second 3 yields a 3-frame "success"; `capture-style-pick.ps1` treats its 12-minute
timeout as non-fatal and silently substitutes the run's first frame when a still is missing; and no layer of the
colour-audio path actually checks the audio - not the C++ `WaitWav`, not the cue TSV's `audio_s` column, not the
`-60 dBFS` guard in the launcher, which passes on silence. The captures are what the owner rules on: the T16 style
pick (WOLF-PLAN P4) and the T26/T40 audio sign-off are both decided from these artefacts. A harness that can
manufacture false evidence makes the owner decide from it. When this card is done, each of these harnesses ends in
exactly one of a logged DONE with a machine-checkable manifest, or a logged FAIL with a non-zero exit code.

## Read first
- `CLAUDE.md`, `docs/DECISIONS.md` (they outrank this card if they conflict)
- `docs/ORCHESTRATION.md` - "a claim with no command behind it is not accepted" is the rule this card hardens in code
- `apps/vr/tasks/T16-style-pick-replay-capture.md`, **including its `## Retry 1`** (orchestrator, 2026-10-04). Two of
  its four findings are still live in the tree and this card names their probable causes: Retry 1 item 1 ("empty arena,
  no hands, no Fire mage") matches defect A2 below, and Retry 1 item 2 ("each run ended on the 12-minute script
  timeout") matches defect A3.
- `apps/vr/tasks/T26-element-colour-and-first-audio.md` and `T40-two-music-loops.md` - both sign off through
  `capture-colour-audio.ps1`, so defect C lands before WOLF-PLAN P7
- `docs/concepts/WOLF-PLAN.md` sections c (P4, P7) and d
- `docs/research/MOONSHOT-2026-10.md:1020` (the `AirCapture` `UPROPERTY` finding, under
  `## Incidental S/M defects (untriaged, for the stabilize loop)` at :990) - defect E below is that item; confirm it is
  still open before writing the fix, and if a sibling card already took it, say so and skip E.
- Code, by symbol (line numbers were verified on this branch 2026-10-21 and will rot - navigate by symbol):
  - `Session/StylePickCapture.cpp` - `UStylePickCaptureDriver::Start` (:26), `::Tick` (:63), `::RequestShot` (:139),
    `::ShotFinished` (:151)
  - `Session/ArenaSession.cpp` - `UArenaSessionSubsystem::OnWorldBeginPlay` (:2060; spawns `ASessionPresentation` at
    :2096, starts the StylePick driver at :2152-2158), `::Start` (:2205, `bRunning = bOk` at :2210), `::BeginArc`
    (:2167, `bRunning = bOk` at :2169), `::Tick` (:2281, the `bRunning` gate at :2283, `Presentation->Sync` at :2295,
    `ApplyCamera` at :2297)
  - `Session/ArenaSession.h:587` - `TObjectPtr<UAirCaptureDriver> AirCapture;`
  - A reference driver that does this correctly: `Session/TeachCapture.cpp` - `::Fail` (:100), `::Tick` (:107, the
    `bShotOpen` early return at :113-118), `::Advance` (:125, the 300 s timeout at :129), `::ShotFinished` (:95),
    `EStep::Finish` (:353, `MAGEVR_CAPTURE_DONE shots=%d` at :356)
  - `Session/ColourAudioCapture.cpp` - `::LogNewCues` (`const double Wall = RecordedS;` at :182, the TSV row at
    :187-188), the `t.MaxFPS 60` comment recording the earlier 20.8 s take (:234-235), `EStep::Recording`
    (`RecordedS += FApp::GetDeltaTime()` at :465, the stop log with `mixer=` at :475-476), `EStep::WaitWav` (:480, the
    size-stability test at :484-487), `EStep::Finish` (:497)
  - `apps/vr/tools/capture-style-pick.ps1` - the concurrent `Start-Process` loop (:45-47), `WaitForExit(720000)` (:51),
    the still fallback `Get-ChildItem $FramesDir | Select-Object -First 1` (:81), the `$Identical`-only gate (:115-120)
  - `apps/vr/tools/capture-colour-audio.ps1` - the peak guard (:72-75)
  - `apps/vr/tools/capture-greybox.ps1:182` and `:214` - the launcher shape to copy (it greps the marker and names what
    is missing)

## The defects, as verified on this branch (2026-10-21, Lynx, by reading the tree)
Re-verify each before you change it; a different reading is reported, not assumed.

**A. `UStylePickCaptureDriver` can report success having captured nothing.**

- **A1. No marker at all.** `Session/StylePickCapture.cpp` contains **zero** `MAGEVR_CAPTURE` strings. Every one of the
  nine sibling drivers in `Session/` logs `MAGEVR_CAPTURE_DONE shots=%d` in its `Finish` case and
  `MAGEVR_CAPTURE_FAIL` in `Fail`; `Greybox/GreyboxCapture.cpp` and `Budget/BudgetStressDriver.cpp` do the same under
  their own prefixes. StylePick has neither. `Start()` early-returns twice (`if (!Sys) return;` and
  `if (!Sys->GetSession().Start(1)) return;`), each time leaving `bRunning` false and logging nothing at all, so a
  driver that never started is indistinguishable from one that finished. Its only assertion is a
  `checkf(Session.GetSimSeconds() > 0.0, ...)` at `SequenceIndex == 60`, which is a crash, not a FAIL.
- **A2. It bypasses the only path that makes the world render.** `Start()` calls `FArenaSession::Start` directly
  (`Sys->GetSession().Start(1)`) and never `UArenaSessionSubsystem::Start`. The subsystem's `bRunning` is set in
  exactly two places, `UArenaSessionSubsystem::Start` and `UArenaSessionSubsystem::BeginArc`, and
  `UArenaSessionSubsystem::Tick` returns immediately while it is false. `Presentation->Sync` is called only from that
  `Tick`, and `ApplyCamera` only after it. So the driver advances the kernel on a world whose presentation is never
  synced. **This is the probable cause of T16 Retry 1 item 1**, "every frame of all four videos shows `Sim: 0.00s |
  Last: (Actor 0)`, an empty arena, no hands, no Fire mage": the sim now advances (the tree has the post-Retry-1
  `Session.Advance(1.0 / 60.0, true)`), but nothing syncs the actors that render it. **UNVERIFIED here** - no renderer
  on Lynx. Confirm it on Wolf before fixing, and record what you saw.
- **A3. It quits with a command that may not exist.** `GEngine->Exec(..., TEXT("QUIT_EXEC"))`.
  `QUIT_EXEC` appears **exactly once in the whole repository**, at this line. Eleven other drivers use
  `Controller->ConsoleCommand(TEXT("Quit"))` (`AirCapture.cpp:380`, `CollarCapture.cpp:415`,
  `ColourAudioCapture.cpp:519`, `CreaturesCapture.cpp:560`, `DayCapture.cpp:343`, `PresetCapture.cpp:573`,
  `SettingsCapture.cpp:310`, `TeachCapture.cpp:371`, `Wave1Capture.cpp:524`, `GreyboxCapture.cpp:886`,
  `BudgetStressDriver.cpp:818`). If `QUIT_EXEC` is not an engine console command the process never exits, which
  **matches T16 Retry 1 item 2**, "each run ended on the 12-minute script timeout". Whether it is a real command is
  **UNVERIFIED here**: confirm on Wolf (`Exec` returning false, or the log line the engine prints for an unknown
  command) and record the answer in the report either way.
- **A4. The run can end early and still look complete.** `Tick` ends on `SequenceIndex >= 3600 || GetStage() !=
  "active"`. A duel that leaves `active` at second 3 writes its event log, clears `bRunning` and quits having captured
  3 frames, with nothing anywhere saying the run was short.

**B. A stuck screenshot is a hang with no FAIL, in every driver.** Each driver's `Tick` returns early while
`bShotOpen && !ShotFinished()` (`TeachCapture.cpp:113-118`, `AirCapture.cpp:143-148`), and the global wall-clock
timeout lives in `Advance` (`TeachCapture.cpp:129`) - which that early return never reaches. A screenshot request that
never produces a file therefore stalls until the launcher's own timeout, with no `MAGEVR_CAPTURE_FAIL`. Nothing rejects
`-nullrhi`, under which a screenshot cannot be produced, even though every capture launcher runs `-game
-RenderOffScreen` and the automation suites run `-nullrhi`.

**C. The audio path is unchecked at every layer.**

- **C1. C++ `WaitWav` only checks that a file exists and stopped growing.** It completes when the WAV is non-empty and
  byte-size-stable for 30 frames. No duration check, no peak check, no sample-rate or channel check. A 0.2 s file of
  silence passes.
- **C2. The driver logs the mixer clock and never compares it.** `EStep::Recording` ends by logging
  `audio=<RecordedS> wall=... sim=... mixer=<Clock - RecordStartClock>`, and discards the comparison. The code comment
  at `:234-235` records a previous take that "came out 20.8 s long for 30 s of play" - exactly the failure these two
  numbers would have caught, left as prose instead of an assertion.
- **C3. The cue TSV's `audio_s` column is not the audio clock.** `LogNewCues` writes `Wall = RecordedS`, and
  `RecordedS` is an accumulation of `FApp::GetDeltaTime()`. So every cue timestamp in `brennic-opening-cues.tsv` is
  game-thread wall time, while the WAV it is read against runs on the mixer clock. Under the 20.8-for-30 condition the
  TSV and the WAV disagree by about 30%, and nothing says so.
- **C4. The launcher's silence guard is broken, and this half is verifiable without Unreal.**
  `apps/vr/tools/capture-colour-audio.ps1:72-75`: `$Volume` is `Select-String "max_volume|mean_volume"` over ffmpeg's
  `volumedetect` output; `$Peak` is `[double]` of a `-replace` applied to `(...| Select-Object -First 1).Line`. With no
  `max_volume` line matched, that expression is `$null`, `-replace` yields the empty string, `[double]""` is `0`, and
  `0 -le -60.0` is false - **silence passes**. A fully silent WAV makes ffmpeg print `max_volume: -inf dB`, which the
  `[-0-9.]+` capture does not match either, so the `-replace` leaves the whole line and the cast fails the same way.
  Reproduced in isolation on Lynx (2026-10-21): `Peak=0 silent=False`. There is also no duration assertion anywhere in
  the launcher, so a 20.8 s file passes a 30 s recording.

**D. The launcher manufactures stills and swallows its own timeout.** `apps/vr/tools/capture-style-pick.ps1` launches
all four editors **concurrently** (`Start-Process` in a loop at :45-47, then a second loop of `WaitForExit`), against
`CLAUDE.md`'s one-Unreal-process rule and against the 60 fps cap the audio and render threads need. On timeout (:51-57)
it prints a line, `taskkill`s the editor and **continues to encoding**. It never greps for any `MAGEVR_` marker.
When a chosen still's frame file is absent it silently copies `Get-ChildItem $FramesDir | Select-Object -First 1` -
**the first frame of the run** - as the still for that event tick (:79-82), so four stills of an empty arena can be
presented to the owner as four event moments. Its only exit gate is `$Identical` over the four event logs (:115-120),
which four identical zero-byte logs satisfy; the ffmpeg encode's exit code is never checked.

**E. `AirCapture` is not a `UPROPERTY`.** `Session/ArenaSession.h:587` holds `TObjectPtr<UAirCaptureDriver>
AirCapture;` - the only one of the capture-driver pointers on that subsystem without `UPROPERTY()`. GC does not see
the reference and may collect the driver mid-run; `UAirCaptureDriver::BeginDestroy` only clears `bRunning`, so the run
goes quiet with no FAIL. This is `MOONSHOT-2026-10.md:1020`, still open.

**F. Context the fix must not make worse: the DONE marker is nearly vacuous even where it exists.**
`TeachCapture::ShotFinished` tests only `FileSize(*PendingShot) > 0` and that the request cleared - **no driver checks
pixel content, so a black or uniform PNG passes**. `EStep::Finish` is reachable only after the last shot step, so its
`else` FAIL branch is effectively dead. `Fail()` enters `EStep::Quit`, which quits with **exit code 0** either way, so
the launchers correctly treat the log marker and not the exit code as the gate (`capture-greybox.ps1:2`: "The editor
Quit code is not the gate"). Keep that contract: the marker stays the gate, and this card makes the marker mean
something.

## Deliverables (exact paths)
1. **`UStylePickCaptureDriver` gets the sibling driver's contract** (`Session/StylePickCapture.{h,cpp}`):
   - A `Fail(const TCHAR* Reason)` that logs `MAGEVR_CAPTURE_FAIL <reason>` and quits, and a terminal success that logs
     `MAGEVR_CAPTURE_DONE shots=%d` exactly once. Both early returns in `Start()` become FAILs with distinct reasons.
     Replace the frame-60 `checkf` with a FAIL (`sim did not advance`), so a bad run is evidence, not a crash dump.
   - `Start()` starts the session through `UArenaSessionSubsystem::Start` (or whatever path A2's Wolf check shows
     actually drives `Presentation->Sync`), not `FArenaSession::Start` directly. State in the report which it was and
     what the frames looked like before and after, with the grid images named.
   - Quit through `Controller->ConsoleCommand(TEXT("Quit"))` like the other eleven drivers. Record A3's answer.
   - A short run FAILs: ending before a stated minimum sequence length (a named constant, not a literal at the
     comparison) is `MAGEVR_CAPTURE_FAIL short run seq=<n> stage=<s>`, never a DONE.
   - Keep the deterministic 3600-tick end and the per-tick event log; this card changes no combat number and no event
     mapping.
2. **A shot watchdog, in one place, used by every driver** (suggested: a small helper beside the drivers, for example
   `Session/CaptureShot.{h,cpp}`, or an existing shared header if one already fits - say which you chose and why): a
   pending screenshot that does not land within a stated wall-clock budget is `MAGEVR_CAPTURE_FAIL shot stuck
   name=<n> waited=<s>s`. Each driver's early return consults it, so defect B cannot hang any driver. Also refuse to
   start any screenshot-taking driver under `-nullrhi`: log `MAGEVR_CAPTURE_FAIL no rhi` and quit.
3. **A pixel-sanity floor on every shot** (in the shared helper, so one implementation covers all drivers): a shot is
   accepted only when its PNG is non-empty **and** not uniform - state the test you used (for example: decode, sample
   a fixed grid of pixels, require at least N distinct values or a luminance standard deviation above a stated floor)
   and justify the threshold from real captures, not from a guess. A black or single-colour frame must be a FAIL. If
   decoding a PNG inside the driver proves impractical, implement the floor in the launchers instead and say so.
4. **An evidence manifest written at DONE**, one JSON per capture run next to the shots (suggested
   `runs/<run>/manifest.json`), listing for the run: the git commit, the seed, the capture flags, and for each shot its
   name, path, byte size, the sim tick and bout, the session stage, and the pixel-sanity result. The launchers then
   **diff the manifest** instead of hard-coding shot names. Keep `MAGEVR_CAPTURE_DONE shots=%d` as the marker line so
   every existing launcher keeps working.
5. **The audio path checks the audio** (`Session/ColourAudioCapture.cpp`):
   - `EStep::WaitWav` asserts the WAV's real duration (parse the RIFF header for sample rate, channels, bit depth and
     data-chunk length - the file is written by `StopRecordingOutput`, so its format is known) against `RecordSeconds`
     within a stated tolerance, and FAILs with the measured numbers when it is outside. Justify the tolerance from the
     three recorded runs, not from a guess.
   - The mixer-clock delta and `RecordedS` are **compared**, not just logged: a divergence beyond a stated tolerance is
     `MAGEVR_CAPTURE_FAIL mixer drift audio=%.3f mixer=%.3f`. The 20.8-for-30 condition in the `:234-235` comment must
     fail this check; say in the report what the live numbers were.
   - The cue TSV gains a mixer-clock column (keep `audio_s` as it is so existing readers do not break; add the new
     column at the end and document both in the file's header row and in the card's report). A cue's position in the
     WAV is read from the mixer column.
6. **The launchers fail when they have no evidence** (`apps/vr/tools/capture-style-pick.ps1`,
   `apps/vr/tools/capture-colour-audio.ps1`):
   - `capture-style-pick.ps1`: run the four editors **one at a time** (`CLAUDE.md`, "One Unreal build at a time");
     grep each log for `MAGEVR_CAPTURE_DONE` and name the missing marker on failure, in the shape of
     `capture-greybox.ps1:182,:214`; a `WaitForExit` timeout is a **failure**, not a message; the still for an event
     tick comes from that tick's frame or the run fails - **delete the `Select-Object -First 1` fallback**; check
     ffmpeg's exit code and that each MP4 is non-empty; reject an event log of zero length even when all four match.
   - `capture-colour-audio.ps1`: fix the peak guard so a missing or unparsable `max_volume` line is a **failure**
     (`-inf` included), not a pass - do not let `[double]` of an empty string decide it; and assert the WAV's duration
     from `ffprobe`'s `format=duration` against the requested seconds within the same tolerance as deliverable 5.
7. **`Session/ArenaSession.h:587` gets `UPROPERTY()`** on `AirCapture`, matching its siblings
   (`MOONSHOT-2026-10.md:1020`). One line; keep it in its own commit-sized change so it is easy to see.
8. **Tests** (in a new `Tests/CaptureEvidenceTests.cpp`, or an existing capture test file if one already fits):
   - `MageArena.Capture.StylePick.FailsWithoutSession` - with no startable session, the driver logs
     `MAGEVR_CAPTURE_FAIL` and never `MAGEVR_CAPTURE_DONE`. **Seen RED first** against the current code (which logs
     nothing); quote the red line.
   - `MageArena.Capture.StylePick.ShortRunFails` - a run ended before the minimum sequence length yields FAIL, not
     DONE.
   - `MageArena.Capture.ShotWatchdog.StuckShotFails` - a pending shot that never lands yields FAIL inside the budget
     rather than hanging.
   - `MageArena.Capture.PixelSanity.UniformFrameFails` - a synthesised uniform image fails the floor and a synthesised
     varied one passes. Pure function over a pixel buffer, so it runs under `-nullrhi`.
   - `MageArena.Capture.Audio.DurationMismatchFails` - a WAV header describing 20.8 s fails a 30 s request; one
     describing 30 s passes. Pure function over a header buffer, so it runs under `-nullrhi`.
   - `MageArena.Capture.Manifest.RoundTrips` - a manifest written and read back carries every field of deliverable 4.
9. `runs/T45/REPORT.md` per `## Report` below.

## Constraints
- Do not commit, push, or touch any repository other than this one. Do not call paid services.
- Do not edit `docs/DECISIONS.md`, `CLAUDE.md`, `apps/vr/tasks/BACKLOG.md`, `apps/vr/data/pinned/**`,
  `apps/vr/data/PINNED.json`, `apps/vr/data/conformance/**`, `apps/vr/Game/Clips/**`, or
  `apps/vr/Game/Source/MageArenaVR/MageArenaVR.Build.cs`.
- **Change no combat number, no event mapping and no kernel rule.** This card is instrumentation only. The event logs
  T16 compares must stay byte-identical in content for the same seed and script; if deliverable 1's session-start change
  alters them, that is a finding to report, not something to paper over.
- **Keep the marker as the gate.** `Fail()` quits with exit code 0 by design and the launchers read the log
  (`capture-greybox.ps1:2`). Do not make an exit code the contract; make the marker honest.
- Do not weaken any existing check, and do not touch `bColoursOk` in `Greybox/GreyboxCapture.cpp` (it is a real
  DeltaE2000 test of screen pixels, `:568`).
- The budget capture's parallel-scene problem is **T46's**, not this card's. If a shared helper from deliverable 2 or 3
  is useful to `Budget/BudgetStressDriver.cpp`, wire it there too, but do not change what the stress scene contains.
- `MageArenaDesign.` is compared per test against the WOLF-PLAN P0 baseline, not against `CLAUDE.md`'s older list.
- Fresh checkout or worktree: regenerate the gitignored sigil corpus first
  (`node apps/vr/tools/clipgen/generate.mjs --corpus 30 --seed 1000`, then
  `node apps/vr/tools/clipgen/mousepaths.mjs`). Never run `generate.mjs` bare or with `--templates`.
- One Unreal build at a time (`-WaitMutex`); one `Automation RunTests` filter per editor process; never clean shared
  engine caches.

## Acceptance (the orchestrator re-runs these; quote their real output in your report)

Lynx-verifiable (no Unreal needed - the orchestrator runs these on either machine):
```
node apps/vr/tools/lint/format-check.mjs --self-test && node apps/vr/tools/lint/format-check.mjs --base origin/master
                                 # expected: format-check OK (changed C++ lines only)
node apps/vr/tools/lint/unused-members.mjs && node apps/vr/tools/lint/unused-members.mjs --self-test
                                 # expected: unused-members OK: <n> private/protected members in <m> classes
node apps/vr/tools/check-pin.mjs # expected: pin OK: 9 files @ baeac66 / conformance OK: 45 vectors (unchanged)
powershell -NoProfile -Command "& { $v = @(); $p = [double](($v | Where-Object { $_ -match 'max_volume' } | Select-Object -First 1) -replace '.*max_volume: *([-0-9.]+) dB.*','$1'); Write-Host \"Peak=$p silent=$($p -le -60.0)\" }"
                                 # the pre-fix reproduction of defect C4; expected before the fix: Peak=0 silent=False.
                                 # Quote it, then show the fixed script rejecting the same empty input.
```

Wolf-only (UE build, the automation suites and real captures - do not claim these from any machine without UE):
```
powershell -NoProfile -File apps/vr/tools/build.ps1
                                 # expected: "Result: Succeeded" in the log, not only exit 0
UnrealEditor-Cmd MageArenaVR.uproject -ExecCmds="Automation RunTests MageArena.;Quit" -unattended -nullrhi -nosplash -log -ReportExportPath=runs/T45/MageArena
                                 # expected: all MageArena. green, including the six new MageArena.Capture.* tests.
                                 # The -nullrhi run also proves deliverable 2's no-RHI refusal does not break the suite.
UnrealEditor-Cmd MageArenaVR.uproject -ExecCmds="Automation RunTests MageArenaDesign.;Quit" -unattended -nullrhi -nosplash -log -ReportExportPath=runs/T45/MageArenaDesign
                                 # expected: no red beyond the WOLF-PLAN P0 baseline, per test
powershell -NoProfile -File apps/vr/tools/capture-style-pick.ps1
                                 # expected: four editors run ONE AT A TIME; MAGEVR_CAPTURE_DONE shots=<n> in each of
                                 # the four logs; runs/T16/manifest.json per style with a pixel-sanity pass on every
                                 # shot; 4 MP4s and 16 stills whose manifest sim ticks match the chosen event ticks;
                                 # exit 0. No "12 minute timeout" line.
powershell -NoProfile -File apps/vr/tools/capture-colour-audio.ps1 -Run T45
                                 # expected: MAGEVR_CAPTURE_DONE in both logs; the WAV duration within the stated
                                 # tolerance of 30 s; "capture-colour-audio: ok"; exit 0
```
Three negative proofs, each quoted in the report with the exact command and the exact failing line - a card that only
shows the happy path has not proven this deliverable:
```
# 1. StylePick with no session must FAIL, not go quiet.
#    Run the driver's flag against a world where the session cannot start (state how you arranged it).
#    expected: MAGEVR_CAPTURE_FAIL <reason> in the log, no MAGEVR_CAPTURE_DONE, and the launcher exits non-zero.
# 2. A short run must FAIL.
#    Force the duel to leave "active" early (state how).
#    expected: MAGEVR_CAPTURE_FAIL short run seq=<n> stage=<s>, and no DONE.
# 3. Silence must FAIL the audio launcher.
#    Replace the recorded WAV with a silent file of the same length and re-run the launcher's check half.
#    expected: the launcher names the silence and exits 1. Before the fix the same input prints
#    "capture-colour-audio: ok".
```

## Report
Write `runs/T45/REPORT.md`: what you built (paths); each acceptance command with its **real** output, the three
negative proofs included verbatim; the red line of each new test before it passed; the answers to the two UNVERIFIED
questions (A2: does `Presentation->Sync` run during a StylePick capture, before and after - with the grid images named;
A3: is `QUIT_EXEC` a real engine console command, and what the engine printed); the live numbers for C2 (`RecordedS`
versus the mixer delta over a real 30 s recording) and the measured WAV duration; the pixel-sanity threshold you chose
and the real captures that justify it; the duration tolerance you chose and why; whether deliverable 1 changed the T16
event logs for the same seed and script; whether `MOONSHOT-2026-10.md:1020` was still open when you started; anything
you could not do with the exact error; and every decision this card did not specify.

## Result (Wolf kaziho 2026-10-24, UE 5.8.3 CL 58210709)

Built from the worktree with `build.ps1`, `Result: Succeeded` at every step and at `197412c`. One Unreal process at a
time; verdicts read from `-ReportExportPath`. The full report with every quoted line is `runs/T45/REPORT.md` on Wolf
(gitignored); this section is its committed record.

**Built.** `Session/CaptureEvidence.{h,cpp}` (new; chosen over `CaptureShot` because it also holds the WAV parser and the
manifest): `FCaptureEvidence` records each requested shot, `Poll()` fails one that has not landed and decoded within
`ShotBudgetS` (30 s) as `MAGEVR_CAPTURE_FAIL shot stuck name=<n> waited=<s>s`, and fails a landed PNG under the pixel
floor; `CanCapture()` refuses `-nullrhi` (`MAGEVR_CAPTURE_FAIL no rhi`); `WriteManifest()` runs before every DONE.
Wired into all ten `Session/` drivers. `UStylePickCaptureDriver` has `Fail()`, distinct FAILs for both `Start()` returns
and the frame-60 check, `MinSequenceFrames` = 1200 (`short run seq=<n> stage=<s>`), DONE once after every frame landed,
starts through `UArenaSessionSubsystem::Start`, quits through `Controller->ConsoleCommand("Quit")`, and FAILs when a
frame's kernel tick is not the previous plus one (frame N = tick N under `-usefixedtimestep -fps=60`). Proof knobs:
`-MageArenaStylePickFrames=<n>`, `-MageArenaStylePickBout=<n>`.

**Defects as found on Wolf.** A1, A2, A4, B, C1-C3, D, E (still open at `84b20ed`), F: as written. **A3 differs:**
`QUIT_EXEC` quits - `FParse::Command(&Cmd, TEXT("QUIT"))` (`GameEngine.cpp:1530`) accepts any non-alphanumeric next
character (`Parse.cpp:791`), and the before-run logged `FPlatformMisc::RequestExit(0, UGameEngine::HandleExitCommand)`
and exited after 214 s; so it was not the cause of Retry 1's timeouts. **C4 differs in part:** digital silence measures
`max_volume: -91.0 dB` here (s16 and f32), which the old guard rejected; what passed was a missing `max_volume` line (a
header-only WAV printed `capture-colour-audio: ok`) and any duration (a 20.8 s tone printed `ok`).

**A2, before and after** (greybox, frames 300/1200/1800, `runs/T45/before/grid-before-greybox.jpg` and
`runs/T45/grid-after-greybox.jpg`). Before: `Sim: 5.00s | Last: cast (Actor 1)` ... `Sim: 30.00s | Last: blink (Actor 1)`
over the same empty arena from the same spot, 1888 frames, no marker. After: fire columns and lane markers, a dark red
block mid-arena (the greybox opponent, by position) with a cyan water bolt in flight at 20 s, the seat HUD, the camera on
another pad after a blink at 30 s; `MAGEVR_STYLEPICK end seq=1870 tick=1870 stage=lost sim=31.167`,
`MAGEVR_CAPTURE_DONE shots=1870`, launcher exit 0.

**Finding: the T16 event log changed for the same seed and script.** Before `1572878ccd422c88871a2c2621055511` (210
events), after `7b0715c871ac3399504c4f7a744896d4` (203); the player's first blink moves from tick 31 to 37. The old driver
called `Session.Advance(1/60, true)`, stepping the hand clips, while the clip players also tick themselves, so the hands
ran at twice the kernel's speed; the subsystem path steps them once a frame, as the game does. Not fixed (constraint).
The new path is deterministic: a style-13 launch wrote the identical log (`7B0715C8...`). P4 captures fresh.

**Suites.** `MageArena.` 272/272 (266 + `MageArena.Capture.StylePick.FailsWithoutSession`, `.ShortRunFails`,
`MageArena.Capture.ShotWatchdog.StuckShotFails`, `MageArena.Capture.PixelSanity.UniformFrameFails`,
`MageArena.Capture.Audio.DurationMismatchFails`, `MageArena.Capture.Manifest.RoundTrips`). `MageArenaDesign.` 5/7:
FireSeated `Expected 'brennic-c15 reached the last phase (1 of 2 breaks)' to be true.`; FullSeated `the script did not
lose bout 3`, `the day closes` ("lost"), `all four bouts won` (2) - DF-011's accepted reds with T47's messages.

**Red lines.** FailsWithoutSession against the unchanged driver: `Expected 'fail markers logged by a driver with no
session' to be 1, but it was 0.` The other five guard new code and were turned red by mutating it (reverted): `Expected
'a 20.8 s WAV fails a 30 s request' to be false.`, `Expected 'the manifest reads back (shot 0 is incomplete)' to be
true.`, `Expected 'a single-colour frame fails' to be false.`, `Expected 'a missing file past the budget fails' to be
true.`, `Expected 'a 3 s run (180 frames) does not pass' to be false.`

**Negative proofs.** (1) `capture-style-pick.ps1 -Styles greybox -ExtraArgs "-MageArenaStylePickBout=99"`:
`MAGEVR_CAPTURE_FAIL session did not start bout=99`, no DONE, `capture-style-pick: failed (4 failures)`, exit 1.
(2) `-ExtraArgs "-MageArenaStylePickFrames=180"`: `MAGEVR_CAPTURE_FAIL short run seq=180 stage=active`, no DONE,
exit 1. (3) `capture-colour-audio.ps1 -CheckWav`: header-only WAV `no duration from ffprobe` / `no max_volume line` /
`failed`, exit 1 (before: `ok`); 20.8 s tone `the recording is 20.800 s, expected 30.8 s within 1.0 s`, exit 1 (before:
`ok`); 30 s silence `the recording is silent (peak -91 dB)`, exit 1 (before: the same).

**Audio (C2), three 30.8 s takes.** game/mixer/WAV: 30.803/30.357/30.272 (DONE, `max_volume: -5.6 dB`,
`capture-colour-audio: ok`); 30.811/29.632/29.483 (`MAGEVR_CAPTURE_FAIL mixer drift audio=30.811 mixer=29.632`);
30.813/30.805/30.741 (DONE). The new `mixer_s` cue column shows each lag is one mixer stall between 14 and 22 s of play.
Tolerance 1.0 s for duration and drift: passes start-up latency and the 0.45 s stall, refuses the 1.2 s stall and
20.8-for-30. Cue TSV columns: `audio_s` (game time since the recording started, unchanged) ... `gain_db`, then `mixer_s`
(mixer clock since the recording started; read a cue's WAV position from it).

**Pixel floor.** 32x18 point grid, >= 12 colours at 5 bits a channel and luma sd >= 4.0. Real captures on Wolf: 33
earlier stills/frames min 47 colours / 24.83; 1870 greybox and 1870 style-13 frames min 20 (a blink vignette) / 27.83;
the 2026-10-04 black-with-a-strip frame 6 / 17.29; uniform 1 / 0. It rejects black and flat frames, not an empty arena
(the before frames pass with 63 colours).

**Manifest.** `runs/<run>/shots/manifest.json`; StylePick `runs/T16/shots-<style>/manifest.json`; colour-audio audio
`runs/<run>/audio/manifest.json`. Fields: driver, commit (read from `.git`), seed, the command line, notes, and per shot
name, path, bytes, tick, bout, stage, pixel {ok, width, height, distinct, lumaStdDev, why}. The two launchers read it.

**Not done.** `Greybox/GreyboxCapture.cpp` (outside this run's paths; own 8/20 s shot fallbacks and `bColoursOk`) and
`Budget/BudgetStressDriver.cpp` (takes no screenshots) are not wired. Pre-existing and unchanged: Wave1's global timeout
logs FAIL and can still reach DONE through `WriteBudget`. The T16 capture itself is P4.
