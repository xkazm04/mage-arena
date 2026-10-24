# T43 - Make the ward gate a real test: close the desktop aim side-channel

Status: **done** on Wolf (kaziho) 2026-10-24, UE 5.8.3 CL 58210709, commits `110f5f9` (seam, behaviour unchanged),
`20fdeea` (reference from the head; SetThreatFacing deleted with ElevatedThreatFacing), `4ef0c19` and `79d3575`
(DESKTOP-INPUT), `2324143` (the tested tree). `Result: Succeeded`; `MageArena.Ward` 14/14, red first:
`Expected 'raises from a palm aimed 40 deg off the seat facing' to be 0, but it was 1.`; `MageArena.` 284/284 (45/45
Conformance); `MageArenaDesign.` 5/7, only DF-011's two accepted reds. See "Result" at the end. (Was: open, written
2026-10-21 on Lynx.)
Max turns: 280

## Goal
The ward's 35 degree raise arc is measured against a reference the player cannot be wrong about, so the ward test can
actually fail for an angle reason. Today the desktop path feeds the same aim vector into both halves of the comparison:
`AMageArenaPlayerController::AimWard` rotates the synthetic hand clip by the aim yaw **and** tells the detector that the
aim is where to look, so the palm is rotated to point exactly at the reference it is about to be compared against. The
arc gate is self-fulfilling on desktop and absent on a headset (nothing calls either setter there, so the reference
stays a fixed `+X`). When this card is done the reference facing comes from real state - the threat when one is
targeted, otherwise the player's head/seat facing - on both the desktop and the VR path, the dead
`SetThreatFacing` plumbing is either wired to a real producer or gone, and at least one ward test fails when the arc is
wrong and has been **seen red** to prove it. The ward/perfect arc is the core of the game's threat language, so a gate
that cannot fail is the most expensive kind of green in this repo.

## Read first
- `CLAUDE.md`, `docs/DECISIONS.md` (newest first; they outrank this card if they conflict)
- `docs/DESKTOP-INPUT.md` - the whole file (51 lines). It is the specification for the desktop path. `CLAUDE.md`'s
  current-phase rule is literal: "Never add a shortcut that skips the pipeline and claim it tests a gesture."
- `docs/concepts/WOLF-PLAN.md` - this card is not on the critical path to 31 Oct; land it where the plan has room
  (after P2/P3, before P9 device bring-up, because the VR reference facing is what P9 brings up).
- `docs/research/MOONSHOT-2026-10.md` items **M1** (one gesture arbiter) and **M2** (one seated frame: map XR tracking
  space into seated-origin space once). This card deliberately stops short of both: see "Scope wall" below.
- Code, by symbol (line numbers verified on Lynx 2026-10-21 and may have moved - trust the symbol):
  - `Hands/MageArenaPlayerController.cpp` - `AMageArenaPlayerController::AimWard` (`:403`, body to `:418`) and
    `WardAimDirection` (`:420`); the scripted-step caller at `:216` and the live caller at `:392`.
  - `Hands/HandClipPlayer.cpp` - `UHandClipPlayer::SetAimYaw` (`:103`) and `UHandClipPlayer::ApplyAimYaw` (`:248`),
    which rotates every joint of every emitted frame by `AimYawRadians`. `Hands/HandInputSubsystem.cpp:143`
    (`UHandInputSubsystem::SetAimYaw`) forwards to the **ward** player only (`:146`).
  - `Gestures/WardDetector.cpp` - `FWardDetector::SetAimFacing` (`:99`), `FWardDetector::SetThreatFacing` (`:104`),
    `FWardDetector::ReferenceFacing` (`:110`, body to `:113`: `return bThreatTargeted ? ThreatFacing : AimFacing;`),
    its two readers at `:122` (`PalmFacing` fallback) and `:234` (the `AngleToFacingDeg` that feeds the gate), and the
    subsystem wrappers `UWardDetectorSubsystem::SetAimFacing` (`:373`) and `::SetThreatFacing` (`:378`).
  - `Gestures/WardDetector.h` - `FWardThresholds::RaiseAngleDeg = 35.0` (`:30`), `LowerAngleDeg = 50.0` (`:31`), and the
    members `FVector AimFacing = FVector::ForwardVector` (`:101`) and `FVector ThreatFacing` (`:102`).
  - `Greybox/GreyboxCapture.cpp:724` - the only other production writer: `Detector->SetAimFacing(FVector::ForwardVector)`.
  - `Tests/WardTests.cpp` - `MageArena.Ward.KindsAndArc` (`:780`), `MageArena.Ward.KeyHoldPath` (`:1027`, the desktop
    end-to-end path) and `MageArena.Ward.ElevatedThreatFacing` (the only `SetThreatFacing` caller anywhere:
    `:2035` and `:2065`).

## The finding this card fixes (re-verify before changing anything)
1. **The side-channel.** `AimWard` computes one `const FVector Aim = WardAimDirection(bFromCursor)` and sends it two
   ways in the same function: `Hands->SetAimYaw(Atan2(Aim.Y, Aim.X))` rotates the clip, and `Wards->SetAimFacing(Aim)`
   sets the detector's reference. `AngleToFacingDeg(PalmNormal, ReferenceFacing())` therefore compares a rotated palm
   against the rotation that was applied to it. Changing the aim moves both sides together, so no aim can push
   `AngleDeg` past `RaiseAngleDeg`.
2. **The VR path is dead.** `SetThreatFacing` has **zero production callers** (verified: `git grep SetThreatFacing`
   returns only the two declarations, the two definitions, the subsystem forward, and `Tests/WardTests.cpp:2035,2065`).
   `UWardDetectorSubsystem::SetThreatFacing` has no callers at all. On a headset nothing calls either setter, so
   `AimFacing` keeps its initialiser `FVector::ForwardVector` and the arc is measured against a fixed world `+X`
   regardless of where the player is looking or what is coming at them.
3. **Capture agrees with neither.** `Greybox/GreyboxCapture.cpp:724` pins the reference to `+X` explicitly, so the
   capture harness and the live desktop controller measure different things.
4. **A doc defect in the same seam** (sibling finding, confirmed on Lynx 2026-10-21): `docs/DESKTOP-INPUT.md:34` says
   the bolt is cast with an `LMB click`, but `grep -ri click apps/vr/Game/Source/MageArenaVR/{Hands,Gestures,Session}/`
   returns **nothing** - no code implements a click-to-bolt. Either the binding exists under another name and the doc
   names it wrongly, or the feature does not exist. Resolve it in this card (see deliverable 6); do not leave the
   specification describing input that is not there.

## Scope wall (read before designing)
- This card does **not** build M2 ("one seated frame"). It must not invent a seated-origin transform, and it must not
  take a dependency on one. Take the head/seat facing from whatever the session already knows (the active pad and the
  player's kernel facing; `Greybox/` already derives a forward for the stress plan and the hand presentation), and
  leave a one-line comment at the read site saying M2 will own this transform. If the honest conclusion after step 1
  is that no usable head facing exists yet without M2, **stop and report that** rather than half-building M2.
- This card does **not** build M1 (the gesture arbiter). Do not touch the clip-name reading or the other three
  detectors.
- No combat-number change, and no change to the pinned data or the conformance vectors. The arc value itself
  (`arcDeg` in the pinned absorb rules, read by `FAbsorbRules::LoadFromPinnedFile`) is **not** retuned here, and
  `FWardThresholds::RaiseAngleDeg` keeps its value of 35.0.

## Deliverables (exact paths)
1. `runs/T43/reference-facing-sites.md` (uncommitted): every production and test site that writes or reads the ward
   reference facing, with file:line, the symbol, and what the value means there. Re-verify the "zero production
   callers of `SetThreatFacing`" claim and the `grep -i click` result; a different answer is reported, not assumed.
2. **One reference, derived from state.** `FWardDetector`'s reference facing stops being something the input layer
   injects alongside the clip rotation. Concretely:
   - When a threat is targeted, the reference is that threat's direction. Wire `SetThreatFacing` (or whatever single
     setter survives) to a **real producer**: the session already resolves what the player is facing and what is
     incoming (`Session/ArenaSession.cpp`, `FArenaSession::ComputeAim` at `:1618` is the nearest existing mechanism -
     read it before inventing anything).
   - With no threat targeted, the reference is the player's **head/seat facing**, not the aim the clip was rotated by.
   - If you conclude the two setters should collapse into one (`SetReferenceFacing(Facing, bFromThreat)`), do that and
     say so in the report. If you conclude `SetThreatFacing` cannot be wired to a producer inside this card's scope,
     **delete it, its subsystem wrapper, its declarations and `MageArena.Ward.ElevatedThreatFacing`**, and record in
     the report that the elevated-threat behaviour is now untested and why that is the lesser evil. Those are the only
     two acceptable endings - a setter with no production caller is not one of them.
3. **`AimWard` stops double-feeding.** After the change, `AimWard` may still rotate the clip (that is the desktop
   pipeline doing its job - the mouse/keyboard aim is the player's hand, and `docs/DESKTOP-INPUT.md` is explicit that
   desktop input plays through the one pipeline), but it must **not** also set the detector's reference. The detector
   learns the reference from state only. The reference-facing writes in `GreyboxCapture.cpp:724` follow the same rule
   or are deleted.
4. **Desktop and VR measure the same thing.** State in the report, in one paragraph, what the reference facing is on
   each path after the change, and why they are the same quantity. If they are not the same quantity, the card is not
   done.
5. **A ward test that can fail.** In `Tests/WardTests.cpp`:
   - `MageArena.Ward.ArcGateRejectsOffAxis`: a palm whose normal is more than `RaiseAngleDeg` off the reference does
     **not** raise the ward, driven through the production path (controller step or `UHandInputSubsystem`, not by
     calling the detector's setter by hand). **Seen RED on unchanged `master`** - on today's code the aim rotation
     follows the palm and it passes trivially; quote the red line verbatim in the report. A case that never failed
     proves nothing.
   - `MageArena.Ward.ArcGateAcceptsOnAxis`: the matching on-axis case still raises, so the fix did not simply close
     the gate.
   - `MageArena.Ward.ReferenceFacingHasAProducer`: a counted assertion (or a lint) that the surviving reference setter
     has at least one production caller outside `Tests/`. This is the guard that stops the plumbing dying again.
   - Both new arc tests publish into the existing `FWardReport` ledger the way the other `MageArena.Ward.*` tests do
     (`Tests/WardTests.cpp:86-118` and `:245-253` are the label list and the JSON rows - add the new labels to both).
6. **The specification is true again.** Update `docs/DESKTOP-INPUT.md` in the same change: the aim row(s) describe what
   the code now does, and the bolt row (line 34, the `LMB click` cell) either names the binding that really exists or
   is corrected to say the bolt has no mouse binding. Do not add a binding to make the doc true - this card changes the
   doc to match the code, or reports the missing feature as a separate finding for the Director.

## Constraints
- Do not commit, push, or touch any repository other than this one.
- Do not call paid services. Do not edit `docs/DECISIONS.md`, `apps/vr/tasks/BACKLOG.md`, `apps/vr/data/pinned/**`,
  `apps/vr/data/PINNED.json`, `apps/vr/data/conformance/**` or `apps/vr/Game/Clips/**`.
- **No shortcut that skips the pipeline.** Every new test drives hands through `UHandInputSubsystem` /
  `UHandClipPlayer` where the existing desktop tests do. A test that pokes `FWardDetector` directly may exist as a unit
  case **in addition**, never instead.
- No combat-number change, no kernel change, no pin change. `FWardThresholds::RaiseAngleDeg` stays 35.0 and the pinned
  `arcDeg` is untouched.
- Stay out of M1 and M2 territory (see "Scope wall"). List any file you touch outside `Hands/`, `Gestures/`,
  `Greybox/GreyboxCapture.cpp`, `Session/ArenaSession.cpp`, `Tests/WardTests.cpp` and `docs/DESKTOP-INPUT.md` in the
  report with the reason.
- Fresh checkout or worktree: regenerate the gitignored sigil corpus first -
  `node apps/vr/tools/clipgen/generate.mjs --corpus 30 --seed 1000` then `node apps/vr/tools/clipgen/mousepaths.mjs`.
  Never run `generate.mjs` bare or with `--templates`; both rewrite tracked clips.
- One Unreal build at a time (`-WaitMutex`); one `Automation RunTests` filter per editor process; read verdicts from
  `-ReportExportPath`, not from stdout. `apps/vr/tools/build.ps1` has exited 0 when `Build.bat` was missing
  (found 2026-10-09): confirm the build really ran from its log line, not from the exit code.
- Baseline the full `MageArena.` and `MageArenaDesign.` suites **before** the change and compare per test after. Known
  red on a clean tree (`CLAUDE.md`, as of 9f338f0): `MageArenaDesign.Duel.FireSeated`, `Duel.AirSeated`,
  `Session.FullSeated`, `Session.Wave1Seated`, `Session.CreaturesSeated`. Any other change is a regression.
  `MageArena.Ward.*`, `.Blink`, `.Sigils` and `.HeldSample` must be identical to the baseline apart from the new tests.

## Acceptance (the orchestrator re-runs these; quote their real output in your report)
Every UE command below is **Wolf-only**. Lynx has no Unreal install and cannot produce any of this output.
```
powershell -NoProfile -File apps/vr/tools/build.ps1
    # expected: the build log contains "Result: Succeeded" (read the log, not the exit code)

UnrealEditor-Cmd MageArenaVR.uproject -ExecCmds="Automation RunTests MageArena.Ward;Quit" -unattended -nullrhi -nosplash -log -ReportExportPath=runs/T43/ward
    # expected: every MageArena.Ward.* green, including the three new tests
    #           MageArena.Ward.ArcGateRejectsOffAxis, .ArcGateAcceptsOnAxis, .ReferenceFacingHasAProducer

UnrealEditor-Cmd MageArenaVR.uproject -ExecCmds="Automation RunTests MageArena.;Quit" -unattended -nullrhi -nosplash -log -ReportExportPath=runs/T43/all
    # expected: identical to the pre-change baseline, plus the three new tests green

UnrealEditor-Cmd MageArenaVR.uproject -ExecCmds="Automation RunTests MageArenaDesign.;Quit" -unattended -nullrhi -nosplash -log -ReportExportPath=runs/T43/design
    # expected: exactly the 5 known reds, their numbers unchanged

git grep -n "SetAimFacing" -- apps/vr/Game/Source/MageArenaVR/Hands
    # expected: no match - the controller no longer sets the detector's reference

git grep -c "SetThreatFacing" -- apps/vr/Game/Source/MageArenaVR
    # expected: either 0 (deleted with its test) or at least one hit outside Tests/ (wired to a producer).
    #           Only-in-Tests/ is a FAIL and the card is not done.

node apps/vr/tools/lint/format-check.mjs --self-test && node apps/vr/tools/lint/format-check.mjs --base origin/master
    # expected: OK
node apps/vr/tools/lint/unused-members.mjs && node apps/vr/tools/lint/unused-members.mjs --self-test
    # expected: OK (a removed setter must not leave an unused member behind)
node apps/vr/tools/check-pin.mjs
    # expected: "pin OK: 9 files @ baeac66" and "conformance OK: 45 vectors" - no combat data touched
```
The **red evidence** is part of acceptance, not a nicety: the report must quote the real failing line of
`MageArena.Ward.ArcGateRejectsOffAxis` run against unchanged `master`. Without it this card has not proven the gate was
broken, and the orchestrator will not accept it.

## Report
Write `runs/T43/REPORT.md`: base sha and the Wolf host name and engine build; the reference-facing site table; what you
built (paths); each acceptance command with its real output; the RED line of `ArcGateRejectsOffAxis` on unchanged
master and its green line after; the per-test suite comparison before/after; which of the two acceptable endings you
chose for `SetThreatFacing` and why; the one-paragraph statement that desktop and VR measure the same quantity; what
you did about `docs/DESKTOP-INPUT.md:34`; premises in "The finding this card fixes" that turned out false; files
outside the stated set; and anything you could not do, with the exact error.

## Result (Wolf kaziho, 2026-10-24, UE 5.8.3 CL 58210709)

`runs/` is never committed, so the report is here (the same text is in `runs/T43/REPORT.md`, with the site table in
`runs/T43/reference-facing-sites.md`). Base `4c69c10`; tested tree `2324143`.

**Finding re-verified at 4c69c10 (after T25).** All four premises held. `AimWard` sent one aim to `SetAimYaw` and to
`SetAimFacing`; `git grep SetThreatFacing` found only declarations, definitions, the subsystem forward and
`Tests/WardTests.cpp:2035,2065`; `grep -rin click` over `Hands/ Gestures/ Session/` printed nothing. Two line numbers
had moved: the capture write is `Greybox/GreyboxCapture.cpp:656` (not `:724`), and `AimWard` is at `:405-420`. One
premise was incomplete: the clip is drawn in the seat frame (hands are parented to `SeatedOrigin`), but `AimWard`
turned it by the *world* yaw, so on a side pad (seat yaw 14.4 deg) the hand and the seat disagreed by the pad yaw.

**Built.**
- `Gestures/WardDetector.{h,cpp}`: one reference, `SetReferenceFacing(SeatFacing)` (flattened; +X until written), with
  `GetReferenceFacing()`. `SetAimFacing`, `SetThreatFacing`, both subsystem forwards, `AimFacing`, `ThreatFacing`,
  `bThreatTargeted` and `ReferenceFacing()` are gone.
- `Session/ArenaSession.{h,cpp}`: the producer, `UArenaSessionSubsystem::ApplyWardReference`, called from
  `ApplyViewAim` every session tick. It writes the possessed pawn's control rotation in the seat frame, or the seat
  forward in a scripted run or with no possessed pawn. A comment at the read site says MOONSHOT M2 will own the
  transform.
- `Hands/MageArenaPawn.{h,cpp}`: `AMageArenaPawn::ToSeatFrame(World, SeatYawDeg)`, the yaw-only world-to-seat turn
  that both the clip aim and the reference use (no XR tracking; M2 territory untouched).
- `Hands/MageArenaPlayerController.{h,cpp}`: `ApplyWardAim(Hands, SeatAim)` only calls `SetAimYaw`; `AimWard` turns
  the world aim into the seat frame first. The controller no longer includes `WardDetector.h`.
- `Greybox/GreyboxCapture.cpp`: the `SetAimFacing(+X)` write is deleted; the capture gets the reference from the
  session, as play does.
- `Tests/WardTests.cpp`: `MageArena.Ward.ArcGateRejectsOffAxis`, `.ArcGateAcceptsOnAxis` and
  `.ReferenceFacingHasAProducer`, all three in the `FWardReport` label list and the ward-timing JSON `tests` rows.
  `MageArena.Ward.ElevatedThreatFacing` and its only helper `PalmUpFrame` are deleted.
- `docs/DESKTOP-INPUT.md`: the ward rows, a new section "The ward aim and what it is measured against", and the bolt
  row.

**SetThreatFacing: deleted (ending 2), with ElevatedThreatFacing.** Reasons: (1) the kernel is 2D (`FSimVec`), so no
production state can ever produce an elevated threat, and that was the test's only subject. (2) The kernel already
judges the threat direction: `ResolveHit` guards a hit only if `SimInArc(Target.Facing, Hit.Source - Target.Pos,
Absorb.ArcDeg)`, with the pinned `arcDeg`. (3) Wiring a threat direction into the detector would make the scripted
design runs, which hold an unturned ward clip, miss raises against off-axis threats. That would change pinned design
numbers without an owner ruling. What is lost: the palm-up-faces-an-elevated-threat case is untested, and a
straight-up palm can no longer raise at all (it is 90 deg off a flat reference). The `PalmFacing` fallback for it
stays as a guard.

**Desktop and VR measure the same quantity.** On both paths the arc gate is the angle between the ward palm's normal
and the head's flat facing, both in the seat frame the hands are drawn in. The palm comes from the hand stream: a clip
turned by `ApplyWardAim` on desktop, the tracked hand on a headset. The head comes from one writer,
`ApplyWardReference`, which reads the pawn's control rotation minus the seat yaw. That is the same rotation that sets
the kernel facing (`ViewAim`), whose arc then judges the threat. No input layer writes the reference on either path
(the producer test counts 0 writes in `Hands/`). On desktop the control rotation is mouse look. On a headset it is the
same read until M2 maps the HMD into the seat frame. That is the one caveat P9 must close: the frame is right, but the
head source on a headset belongs to M2.

**`DESKTOP-INPUT.md:34`.** No click-to-bolt exists. The bolt comes only from `FBlinkDetector`'s flick in the hand
stream (the `Q` clip), and LMB only streams a fingertip path to the sigil recognizer (`MouseSigilCapture.cpp:52`). The
cell now says the bolt has no mouse binding. No binding was added. Finding for the Director: if the owner wants a
mouse bolt, it needs a card (a mouse flick into the hand pipeline, not a shortcut).

**RED first.** `MageArena.Ward.ArcGateRejectsOffAxis` was run on master's production code, through the
behaviour-preserving seam commit `110f5f9`, which only moves `AimWard`'s body into `ApplyWardAim`:
`Expected 'raises from a palm aimed 40 deg off the seat facing' to be 0, but it was 1.` The first run used aims of
+-40 and +-60 deg, and all four were red. After the fix, the left aims still raised. The authored rise turns the left
palm from about 80 deg right of forward down to forward, so a clip turned left sweeps across the reference on the way
up (and at -40 the 50 deg lower angle keeps it up). That is the detector working, not the side-channel. So the test
now checks that +40/+60/+90 never raise and that -60/-90 do not keep the ward up. That final test was run again on
`110f5f9`'s production files and is red in every case: `... 40 deg ... to be 0, but it was 1.`, `... 60 deg ...`,
`... 90 deg ...`, `Expected 'a palm held -60 deg off the seat facing keeps the ward up' to be false.`, and the same
for -90. Green after: `Ward arc reject aim=40.0 held=40.00 raises=0 end=0`, the same for 60 and 90,
`aim=-60.0 ... raises=1 end=0`, and -90 `raises=0 end=0`. `ArcGateAcceptsOnAxis` (0, +-30) was green both before and
after.

**Acceptance on `2324143`.**
- `build.ps1`: `Result: Succeeded` (read from the log, each build).
- `Automation RunTests MageArena.Ward`: 14/14 green, including the three new tests. The producer test logs
  `Ward reference producers=1 inputLayer=0 removed=0 sites: Session/ArenaSession.cpp x1`.
- `Automation RunTests MageArena.`: 284 run, 0 failed, 45/45 `Kernel.Conformance`. Compared per test against the
  baseline on `4c69c10` (282/282), it is identical except `-ElevatedThreatFacing` and `+ArcGateRejectsOffAxis`,
  `+ArcGateAcceptsOnAxis`, `+ReferenceFacingHasAProducer`. One more test passed with warnings (7 vs 6):
  `Hands.HeldSample.BlinkModels` caught the engine's HTTP probe timing out (`https://www.google.com/generate_204`),
  which is an environment log.
- `Automation RunTests MageArenaDesign.` (on `20fdeea`, the same production code): 5/7. Only `Duel.FireSeated`
  (brennic-c15 reaches 1 of 2) and `Session.FullSeated` (loses bout 3) are red. Those are DF-011's accepted reds, with
  the same assertions as the baseline, and all 36 `MAGEVR_MEASURE` lines are byte-identical to the baseline run.
- `git grep -n SetAimFacing -- .../Hands`: no match. `git grep -c SetThreatFacing -- .../MageArenaVR`: 0.
- `format-check.mjs --base 4c69c10`: `format-check OK: 10 changed C++ file(s)`; its self-test is OK.
  `unused-members.mjs`: `unused-members OK: 584 private/protected members in 41 classes`; its self-test is OK.
- `check-pin.mjs`: `pin OK: 9 files @ baeac66`, `conformance OK: 45 vectors`. Also: agy-guard 40/40, check-overlay
  and both self-tests OK, leonardo-redaction 24/24, `repo-law OK: 4/4`, `check-commits OK`,
  `actions pinned: 6 refs`, `staged clips OK: 115 files`.

**Files outside the card's stated set:** `Hands/MageArenaPawn.{h,cpp}` for `ToSeatFrame`. They are in `Hands/`, so
inside the brief's paths, but the card's list names only the controller. Also `Session/ArenaSession.h` (the brief
allows `Session/`) for the producer's declaration. Nothing is outside the brief's paths.

**Not done / for the Director.** (1) The resume gate (`SessionFlow.cpp:521-527`) and T25's missio palm
(`DayFlow.cpp:405-406`) still measure against a fixed seat forward, not the head reference. T25 logic was out of
scope. (2) The left/right asymmetry above comes from the authored clip; changing the clip or the gate needs an
owner/clip card. (3) On a headset the head source is the control rotation until M2.
