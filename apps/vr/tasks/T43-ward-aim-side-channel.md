# T43 - Make the ward gate a real test: close the desktop aim side-channel

Status: open (needs Wolf: UE 5.8.3 editor and the `MageArena.Ward` automation suite; written 2026-10-21 on Lynx, where
neither runs - `docs/DECISIONS.md` 2026-10-20 "Unreal host")
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
