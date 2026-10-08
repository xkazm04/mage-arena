# Device feasibility, 2026-10: Quest 3 and Quest 3S

Report-only research on what the desktop cannot answer before the V1 device sprint. It covers the hand-tracking rate,
prediction between camera updates, Quest 3S against Quest 3, and the rules for a hands-only app.

- **Scope.** This file changes nothing else. Every recommendation is a proposal for a later card, for the file's owner.
  It does not repeat what `docs/research/STACK-OPPORTUNITIES-2026-10.md` settled (F6, F7, F8). It does not cover the
  supportedDevices rewrite or the ward and blink detector fix, which other deliveries own.
- **How the quotes were checked.** A research sub-agent did the first sweep. I then downloaded every source page again
  on 2026-10-07 and found each quoted phrase in the raw HTML with `grep` (section 6). Meta pages split some sentences
  across markup, so a few quotes were found in pieces; section 6 shows each piece.
- **Labels.** **Confirmed** means a first-party page or the installed source says it. **Unconfirmed** means no
  first-party source says it. Installed source is given as a path and line.
- **Short paths.** `MetaXR/...` is under the main checkout's `apps/vr/Game/Plugins/` (untracked, read-only).
  `Engine/...` is under `C:\Program Files\Epic Games\UE_5.8\` (UE 5.8.3). `Source/...` is
  `apps/vr/Game/Source/MageArenaVR/`.

## 1. Answers

### Q1. Hand-tracking rate and cost

| Setting | Rate | Status |
|---|---|---|
| LOW (default) | 30 Hz | **Confirmed** as Meta's "Default mode" rate (s1). No Meta page names a device or says "LOW", so LOW = default mode is inferred from s6 and s4. |
| HIGH | 60 Hz (Fast Motion Mode) | **Confirmed** (s2, s3). 50 Hz where mains power is 50 Hz (s2). |
| MAX | the same as HIGH | **Confirmed** (s2). v207 still offers it (`MetaXR/Source/OculusXRHMD/Public/OculusXRHMDTypes.h:144-149`). |
| Quest 3 against Quest 3S | no device-specific figure | **Unconfirmed.** FMM lists "Quest 2, Quest Pro, Quest 3, and all future devices" (s2), so 3S is covered only by "all future devices". |

- **Cost, as Meta states it:**
  - "Higher tracking frequencies will reserve some performance headroom from the app's budget" (s4). **Confirmed**, but with no number.
  - The only numbers are from 2021 and Quest 2: LOW apps "are downclocked to CPU level 3 and GPU level 3", and HIGH apps
    "to CPU level 3 and GPU level 2" (s5). **Unconfirmed** for Quest 3 and 3S.
  - Meta also says HIGH costs quality: "The increased tracking rate can create a jitter" (s1). FMM "Tracking quality
    regresses in low lighting conditions" (s2).
  - **Thermal:** no first-party statement found.
- **The installed knob is the manifest only.** The APL writes `com.oculus.handtracking.frequency`
  (`MetaXR/Source/OculusXRHMD/OculusMobile_APL.xml:376-382`). Nothing in the v207 source changes the rate at runtime.
  The OpenXR frequency hint (s6) is not in the engine's OpenXR 1.1.46 headers, and Meta calls it "advisory only".
  Wide Motion Mode is compiled only under `OVR_INTERNAL_CODE` (`MetaXR/Source/OculusXRHMD/Private/OculusXRHMD.cpp:3487-3493`).
- **The real rate can be read on device with no code.** The FMM page says to run `adb logcat -e "Camera FPS"`, which
  prints "HandTrackingService: Hand Tracking Processed FPS = 58.5 Hz / Camera FPS = 60.1" (s2). Whether the same line
  appears at LOW is **unconfirmed**.

### Q2. Does the runtime predict between camera updates?

**Yes, by default. Confirmed.** Meta: "By default, xrLocateHandJointsEXT returns hand poses extrapolated to the
requested display time. The runtime predicts hand positions forward in time to reduce perceived latency between
tracking and rendering" (s7).

- **OpenXR text:**
  - `XrHandJointsLocateInfoEXT`: "time is an XrTime at which to locate the hand joints" (s8). The extension says nothing more.
  - The general rule for future times, at `xrLocateSpace`: "For a time in the future, the runtime should locate the
    spaces based on the runtime's most up-to-date prediction" (s8).
  - `XR_META_hand_tracking_unextrapolated_poses` is the opt-out: "the runtime must provide the latest calculated hand
    poses without temporal extrapolation" (s8). It also returns a `captureTime`, "indicating when the tracking data was
    captured" (s7).
- **The time this repo's v207 path passes:**
  - The engine's `xrWaitFrame` predicted display time goes to every extension plugin once per game frame:
    `Engine/Plugins/Runtime/OpenXR/Source/OpenXRHMD/Private/OpenXRHMD.cpp:4275` calls `UpdateDeviceLocations(true)`,
    and `:1900` passes `FrameState.predictedDisplayTime`. The late updates at `:3799` and `:3993` pass `false`, so they
    do not re-sample the hands.
  - The Meta path sets `HandLocateInfo.time = InternalHandsState.PredictedDisplayTime`
    (`MetaXR/Source/OculusXRInput/Private/OculusXRInputHandTrackingExtensionPlugin.cpp:356`).
  - The engine path sets `LocateInfo.time = DisplayTime`
    (`Engine/Plugins/Runtime/OpenXRHandTracking/Source/OpenXRHandTracking/Private/OpenXRHandTracking.cpp:238`).
  - Both paths therefore get extrapolated poses.
  - The Meta path then writes the display time into both `RequestedTimeStamp` and `SampleTimeStamp`
    (`...HandTrackingExtensionPlugin.cpp:643-644`). `XrHandJointLocationsEXT` has no time field
    (`Engine/Source/ThirdParty/OpenXR/include/openxr/openxr.h:2618-2624`). So the app never sees when the cameras sampled.
- **Verdict for F7(b):** the held model (`Source/Tests/HeldSampleTests.cpp:19-22`) is a worst case, not the real
  case. The real stream is extrapolated: fresh-looking poses every frame, a lag at motion onset of up to one camera
  period, and overshoot when the hand stops. Meta does not state its extrapolation model or horizon (**unconfirmed**).

### Q3. Quest 3S against Quest 3

| Item | Quest 3 | Quest 3S | Status |
|---|---|---|---|
| Chip and RAM | XR2 Gen 2, 8 GB | "the same power and performance as Quest 3" | **Confirmed** (s10) |
| Panel per eye | 2064x2208 | 1832x1920 | **Confirmed** (s9) |
| Default eye buffer (100% render scale) | 1680x1760 | 1680x1760 | **Confirmed** (s9) |
| Refresh rates | 72, 80, 90, 96, 100, 120 | the same; "does not support rates above 120" | **Confirmed** (s11, updated 2026-10-01) |
| Lenses, FOV, tracking cameras | | | **Unconfirmed.** The meta.com spec pages served only a cookie wall. |
| Hand-tracking quality | | | **Unconfirmed.** No Meta page says it differs. |

- **What this means for section 7.** The budgets were set on Quest 3. Both headsets have the same chip and render the
  same default eye buffer, so the section 7 GPU and CPU budgets (`docs/PROJECT-PLAN.md:345`) carry over to 3S at the
  default render scale. A higher pixel density set per device would break that.
- **What it does not settle.** The 3S panel has fewer pixels. Its lens and FOV are unverified. Sigil glyph legibility
  and peripheral threat cues therefore remain a 3S question (risk 3).

### Q4. Rules for a hands-only app

Section 2 of the plan (checked 2026-10-02) lists the competition rules but no store or VRC rules. None of the pages
below changed after 2026-10-02. The newest is the refresh-rate page (2026-10-01). Two are new to this repo:

- **VRC.Quest.Input.7** (2024-07-31): "the app must properly respect when input is switched between controllers and
  hands" (s12).
- **Meta app compatibility page** (2026-09-04): "Declaring oculus.software.handtracking with required="true" means hands
  only". Also: "Do not ship the declaration before the hand-input path works" (s13).
- The Unreal setup page says that with Hands Only, "A user must have hand tracking enabled on their device to use your
  app" (s14).

Against the repo:

| Rule | What the repo has | Gap |
|---|---|---|
| System gesture reserved (Input.8) | F8 gate, desktop only | none new; the live flag is V1 work (F8) |
| Hand permission and feature in the manifest | `package-android.ps1:161-162` asserts both | none |
| Hands-only declared, or the controller switch handled (Input.7) | `HandTrackingSupport=ControllersAndHands` (`DefaultEngine.ini:52`), so the APL writes `required="false"` (`MetaXR/Source/OculusXRHMD/OculusMobile_APL.xml:202`); no controller path (0 files in `Source/` mention `MotionController`) | **yes**, finding G3 |
| Tracking loss | the plan pauses on it (`PROJECT-PLAN.md:229`) | none; no VRC covers it (Input.1 to Input.8 checked; 9 to 11 are 404; Input.6 is retired) |

**Not re-read:** the Devpost rules page. curl got a "planned maintenance" page, and WebFetch got HTTP 403. Section 2's
competition rules are therefore **not re-checked** (log 1).

- **Status:** re-read 2026-10-07 (about 14:45Z). `curl` with a browser User-Agent got HTTP 200 and 183,553 bytes from `/rules` (the page varies by a byte between fetches; 183,554 was seen earlier), and HTTP 200 and 166,848 bytes from the overview page. Raw HTML is in `runs/rules/` (gitignored). The Meta blog and download pages (`developers.meta.com`) answered `curl` with HTTP 400, so no Meta announcement blog was read; the overview page carries the same requirements text. Against PROJECT-PLAN section 2: **confirmed** with a quote: hands ("Fully usable with hands end-to-end. Controller support optional."), the "Competition" release channel, video under 3 minutes, description contents and target launch date, New Experience wording and the 2026-09-24 window start, the four 25% criteria and "min 60 fps", the freeze after the entry period, availability until the Winner Announcement, originality, the Gaming genre list, and Start membership "by the time of submission". **Newly settled:** the description length is only "suggested length: 500 words or less" (overview page; the rules page gives no limit); the build also needs an Invite URL ("so judges can access and test your Project."). **Dates:** the entry period ends "November 18, 2026 at 12:00:00 PM PT" (the owner's 2026-11-18 holds; the cut-off is noon PT, not end of day). **Not in the plan:** one Entry per individual; projects free of charge; English only; no identifiable persons and no brand or corporate logos; no unreleased Meta tools; "Don't lean on AI-generated video to carry the pitch."; the form also has a 140-character tagline; the video may show an "equivalent emulator". Nothing was changed or contradicted; no rule was found missing. Quotes are grep-verified against the raw HTML (result.json lists the commands).

### Q5. What changes a threshold, a test or the V1 plan

- **Thresholds:** none now. Prediction changes what the detectors see, but the size of the change is a model until the
  device measures it. The ward and blink detector fix running beside this pass owns any threshold change.
- **Tests:** G1 adds a variant to F7(b) that extrapolates instead of holding, plus a 25 Hz bound.
- **V1 plan:** G2 adds a day-one rate and prediction probe and corrects the recorder's time rule. G3 moves the manifest
  to hands-only.

## 2. Summary

| # | Finding | Change | Cost | When | Headless? | Registry |
|---|---|---|---|---|---|---|
| G1 | The runtime extrapolates by default, so F7(b)'s held model is the worst case only | add an extrapolated variant and a 25 Hz held bound to `HeldSampleTests` | S-M | before the 2026-10-31 gate | yes | N2 |
| G2 | The v207 path never exposes when the cameras sampled, and the recorder rule assumes held poses | V1 day-one probe (`logcat` rate, unextrapolated poses); fix the "sensor time" and "same pose repeats" text | M | V1 (text before the gate) | text yes, data no | N6, N2 |
| G3 | The manifest says controllers are supported, but the game has no controller path | `HandTrackingSupport=HandsOnly` at V1; assert `required="true"` | S | V1, after hands work on device | yes (manifest) | EXTENDS `ship-pipeline-gating` |
| G4 | MAX equals HIGH; HIGH costs headroom, adds jitter, and runs at 50 Hz on 50 Hz mains | reword F7(c); log the camera rate, GPU level and room light in the A/B | S | V2 | no | N2, N5 |

## 3. Findings

### G1. The runtime extrapolates by default, so F7(b)'s held model is the worst case only

- **Upstream statements:**
  - Meta, unextrapolated poses page (updated 2026-04-24): "By default, xrLocateHandJointsEXT returns hand poses
    extrapolated to the requested display time. The runtime predicts hand positions forward in time to reduce perceived
    latency between tracking and rendering." https://developers.meta.com/horizon/documentation/native/android/native-openxr-hand-tracking-unextrapolated-poses/
  - Meta, hands technology page (updated 2026-08-17): "Default mode: Offers hand tracking at 30Hz."
    https://developers.meta.com/horizon/design/hands-technology/
  - Meta, FMM page (updated 2026-08-11): "most other countries are at 240V 50Hz, so they will see 50Hz in FMM".
    https://developers.meta.com/horizon/documentation/unity/fast-motion-mode/
- **Installed source:** both hand paths ask for the predicted display time (`OculusXRInputHandTrackingExtensionPlugin.cpp:356`,
  `OpenXRHandTracking.cpp:238`), once per game frame (`OpenXRHMD.cpp:4275`, `:1900`).
- **Repo:**
  - `Source/Tests/HeldSampleTests.cpp:19-22` says "nothing predicts between updates". `:27` fixes the camera at 30 Hz.
  - The ward onset rises at `OnsetSpeedMps = 0.30` (`Source/Gestures/WardDetector.h:33`). The blink arms at
    `MinTipSpeedMps = 1.20` (`Source/Gestures/BlinkDetector.h:44`).
- **Why it matters:**
  - F7(b) found that held hands move the ward onset 111 to 250 ms late and lose the perfect block.
  - Extrapolation cannot see a raise start from rest. The first camera sample that shows motion should still arrive
    within one period (33 ms at 30 Hz), and from then on the poses move every frame. So the real onset lag is probably
    much smaller than the held lag. That is a hypothesis.
  - Extrapolation also overshoots when the hand stops. That can raise blink tip speed or delay the blink re-arm.
- **Recommended change:**
  - Add a second transform beside `MakeHeldClip`. At each 72 Hz frame, extrapolate linearly from the last two camera
    samples to the frame time, capped at one camera period. Run the same ward, blink and sigil cases on it and report
    the onset delta and false blinks.
  - Add a 25 Hz held bound. Whether the default rate drops to 25 Hz on 50 Hz mains is **unconfirmed**: Meta states
    50 Hz only for FMM.
  - Keep the held variant as the floor.
  - Label the extrapolated variant a model. Meta does not state its own extrapolation method.
- **Cost:** S-M, one test file plus the transform.
- **When:** before the 2026-10-31 gate. It tells the gate whether the ward perfect window is at risk on real hands, or
  only in the worst case.
- **Verifiable headless:** yes.
- **Registry:** N2 `hand-tracked-timing-windows`.
- **Status:** applied 2026-10-07, tests only (no detector, threshold or window changed). Before, `Tests/HeldSampleTests.cpp` had the held 30 Hz stream and nothing else. After, it also has `MakeExtrapolatedClip`, a **model**: linear from the last two camera samples to the frame time, capped at one camera period; rotations move by the same fraction; pinch is held. Both models run at 30 and 25 Hz, at camera phases 0, 1/90 and 2/90 s. Six new tests (`HeldSample.ModelsAreReal`, `.Latency`, `.WardModels`, `.BlinkModels`, `.CorpusModels`, `.Staff`) check against two-sided ratchets, and every entry is explained in the code.
  - **Latency** (D-G3 model of `Ward.OnsetCompensation`, 0 to 100 ms, ward-raise normal, slow and sloppy):
    - The onset error does not change with latency on any stream. At 72 Hz it is 0 ms: go 12/12.
    - On the held and the extrapolated 30 Hz streams it is +13.9 to +41.7 ms, depending only on the camera phase. The go predicate (onset within one 72 Hz frame) holds in 12/36 cases on each stream; the 48 misses are listed.
    - The hit at the authored tick and the interior probes (onset + 0.05 and + 0.10 s) are perfect in 36/36 cases on each stream.
  - **Ward onset** against 72 Hz: +13.9 / +27.8 / +41.7 ms (min / median / max) on held30, extrap30, held25 and extrap25 alike. It is within the bound (47.2 ms at 30 Hz, 53.9 ms at 25 Hz) in 9/9 cases per stream, and the interior probes are perfect in 9/9.
  - **Blink:**
    - Count and direction match 72 Hz in 27/27 cases on every stream, with no false blink and no bolt.
    - Re-arm within the bound: held30 18/27 (slow 167 to 236 ms late), extrap30 14/27, held25 27/27, extrap25 2/27 (up to 194 ms late).
    - The extrapolated stream fires the flick 55 to 125 ms early, before the tip peaks, and that early fire is what delays its re-arm.
  - **Sigil corpus** (phase 0, noise 0/5 everywhere):

    | Stream | Accuracy | Impostor casts |
    |---|---|---|
    | 72 Hz | 257/270 | 0/180 |
    | held30 | 256/270 | 0/180 |
    | extrap30 | 253/270 | 9/180 |
    | held25 | 258/270 | 0/180 |
    | extrap25 | 237/270 (87%) | 30/180 |

    Every false cast is an `impostor-zigzag`. With pinch extrapolated as well, extrap30 gave 254/270 and 30 casts.
  - **Staff:** lift and plant match 72 Hz in 18/18 cases on held30 and on extrap30.
- **Proven:**
  - `build.ps1` exit 0.
  - The full `MageArena` suite: 172 tests, 169 pass. The failures are the same 3 known ones (`MageArenaDesign.Duel.FireSeated`, `Session.FullSeated`, `Session.Wave1Seated`); before there were 166 tests and 163 passed.
  - Mutation proofs, each restored and green afterwards:
    - The extrapolation returning its source, with 25 Hz set to 72: 99 + 99 "differs from its source" and 93 + 66 held25 pose-count failures.
    - A zero extrapolation fraction: 99 + 99 "differs from the held stream" failures.
    - No latency slowdown: 9 "confirm tracks the injected latency" failures.
    - No still tail on the blink clips: 24 "did not fire and re-arm" failures.
- **Not proven:**
  - The real camera rate, and whether it drops to 25 Hz.
  - Meta's real extrapolation method and horizon, and whether pinch is extrapolated.
  - Photon-to-detection latency.
  - Phases beyond 2/90 s. They cover 22 ms of the 33 ms (30 Hz) and 40 ms (25 Hz) camera periods.
- **D-G3 verdict:** the perfect window holds to 60 ms on both the held worst case and the extrapolated model: the hit at the authored tick and the interior hits are perfect in every case. Strict "authored tick ±1 frame" timing does not hold: the onset lands 14 to 42 ms late, from the camera grid, not from latency. W1 is not needed and would not move the onset. W2 (stamping the onset earlier by the camera lag, which needs G2's rate) is what would restore ±1 frame. Separately, the extrapolated model raises two findings for later runs: zigzag false casts (a recognizer fix) and a late blink re-arm (a detector fix).
- **Status:** zigzag false casts fixed 2026-10-07 in 9f824a3, in the stroke builder (`Gestures/SigilStrokeBuilder`); $Q and its 26000 reject are unchanged. The blink's early fire and late re-arm were **still open** then; they are fixed in the next Status entry (2026-10-08).
  - **Diagnosis:**
    - extrap30: 9 zigzags cast as line1 ×6, line2 ×2 and line3 ×1, at $Q 3389 to 6765.
    - extrap25: 30 zigzags cast as line1 ×20, line3 ×7 and line2 ×3. 28 are two-stroke, at $Q 3891 to 6342. Two lost the pen-up and cast as one stroke, at 15238 and 15516.
    - Every distance is far under the reject. Most are under the correct sigil-line2 casts (extrap30 892 to 20450) and the sloppy correct matches at 14659 and 23595. $Q scales the cloud, so a small zigzag inside a circle looks to it like a circle with a short mark. The stroke builder's inner-stroke gate is the only thing between them.
    - What the builder saw: the zigzag's inner stroke has 40 points on every stream.
      - Its path is ≤ 0.273 of the loop diagonal at 72 Hz, 9.75 to 9.99 cm (0.341 to 0.362) on extrap30 and 11.14 to 11.74 cm (0.359 to 0.408) on extrap25. The gate is 0.34.
      - Its absolute turning is 240 to 254° on extrap30 and 551 to 586° on extrap25.
      - Its reach (the bounding-box diagonal) is 4.6 to 5.4 cm, 0.152 to 0.188 of the loop diagonal, on every stream.
      - A correct sigil-line2 inner stroke has a path of 12.8 to 27.4 cm and a reach of 12.1 to 17.1 cm (0.410 to 0.705).
    - Why: linear extrapolation runs ahead through each zigzag corner, and through the start of the stroke, by up to one camera period of motion. The next camera sample snaps it back. Each overshoot is counted twice in the path and barely at all in the reach. The 25 Hz period is longer, so the overshoot is longer.
    - The same effect costs extrap25 its sigil-line2 accuracy. Of the 22 wrong line2 clips, 15 fired on the circle alone. A circle-only tail has a path of about 20 cm on extrap25 against about 11 cm on held25, up to 0.769 of the loop diagonal. That passes the one-stroke `InnerTailRatio` (0.65), so the bare circle was taken as a circle drawn through into its line: 14 cast a wrong label and 1 was rejected by $Q. The line stroke after it then rejected as "no circle". The other 7 were $Q label confusions. Why line2 suffers most (and line3 next, with 7 early fires) was not isolated.
  - **Fix:** both builder gates measure the inner mark's reach (the bounding-box diagonal), not its path. A spell line is a straight reach across the circle, so its reach is close to its path; a jagged or overshooting mark's reach stays small. The thresholds sit between the measured classes:
    - later strokes: `MinInnerStrokeRatio` 0.28. Marks reach ≤ 0.200 on every stream; lines reach ≥ 0.385 at 72 Hz and ≥ 0.351 on the modelled streams.
    - one-stroke tail: `InnerTailRatio` 0.53. Circle-only tails reach ≤ 0.40 at 72 Hz and ≤ 0.475 on the modelled streams; drawn-through templates and mouse clips reach ≥ 0.595.
  - **Before and after**, seed 1000 corpus, phase 0:

    | Stream | Accuracy before | Accuracy after | Impostor casts before | after | Noise casts before | after |
    |---|---|---|---|---|---|---|
    | 72 Hz | 257/270 | 257/270 | 0/180 | 0/180 | 0/5 | 0/5 |
    | held30 | 256/270 | 256/270 | 0/180 | 0/180 | 0/5 | 0/5 |
    | extrap30 | 253/270 | 253/270 | 9/180 | 0/180 | 0/5 | 0/5 |
    | held25 | 258/270 | 258/270 | 0/180 | 0/180 | 0/5 | 0/5 |
    | extrap25 | 237/270 (87.8%) | 251/270 (93.0%) | 30/180 | 0/180 | 0/5 | 0/5 |

    - `Sigils.Accuracy` is unchanged: normal 87/90, slow 88/90, sloppy 82/90, mouse 87/90, and every impostor kind rejected 30/30. `Sigils.NoFalseCasts` is 0 over 180 s. The templates still load 12/12/12 with none skipped.
    - All 19 extrap25 misses left are $Q labels (line2 as line3 ×10, line1 as line3 ×5, line3 as line1 ×4), not the gate.
    - The `CorpusModels` ratchet is now extrap30 253/0, held25 258/0, extrap25 251/0.
  - **Held-out corpus, seed 2000** (before → after): 72 Hz 258 → 258; held30 254 → 254; held25 258 → 258; extrap30 251 → 251, impostor casts 9 → 0; extrap25 221/270 (81.9%) → 243/270 (90.0%, exactly the floor), impostor casts 30 → 0. Noise casts are 0/5 throughout, and `Sigils.Accuracy` is unchanged (normal 88/90, slow 87/90, sloppy 83/90, mouse 87/90).
    - Limit: `generate.mjs` seeds the impostor and noise clips from fixed bands, not from `--seed`. The held-out check therefore tests corpus accuracy only, and the false-cast figures are the same 180 impostors.
  - **Mutation proof:** reverting `SigilStrokeBuilder.cpp` and `.h` to 35cd067, under the new ratchet, fails `CorpusModels` on 3 assertions: extrap25 correct 237 ≠ 251, extrap25 false casts 30 ≠ 0, extrap30 false casts 9 ≠ 0. Restored, it is green.
  - **Proven:**
    - `build.ps1` exit 0 before the change (121 s), and exit 0 on the clean tree after 9f824a3.
    - The full `MageArena` suite: 175 tests and 172 pass, both before and after. The per-test diff is empty, and the failures are the 3 known ones.
    - The boot log reads 9 files, SHA1 `11F9B16CBBC77C2469704829C518E4106236C2FF`.
    - `check-pin.mjs` passes.
  - **Not proven:** real Quest input. The finding and the fix rest on the G1 extrapolation **model**.
- **Status:** the blink's early fire and late re-arm fixed 2026-10-08 in `Gestures/BlinkDetector` only. The arming gates (`PointRatio`, `MinOmegaRadPerS`, `MinTipSpeedMps`, the yaw gates), every existing `FBlinkThresholds` value and the public API are unchanged.
  - **Diagnosis.** The per-frame tip speed, hottest speed, fire and re-arm frames were logged, first from a Node replica of the detector and both stream models (it reproduced the ratchet's 18/14/27/2 exactly), then checked against `HeldSample.BlinkModels`. Three cases:
    - extrap30 `blink-left.slow` p0: each camera update shows as a one-frame spike (the snap to the new sample), and the frames after it run on at the older, slower velocity. The spike reads 1.462 m/s at 0.1667 s; the frame before it read 0.077. The next two frames read 0.565, under 0.55 × 1.462 = 0.804, so the flick fired at 0.1806 s. That is 125 ms before 72 Hz (0.3056 s). The real tip went on to 2.473 m/s at 0.2083 s. After the stop, the overshoot snapped back against the flick at 1.554, 0.947, 0.630, 0.727 and 0.630 m/s (0.347 to 0.472 s). Each is over `RearmSpeedMps` (0.60), so the quiet clock kept resetting, and the re-arm came at 0.6250 s, 167 ms late.
    - extrap25 `blink-left.normal` p0: the same, larger. A 4.730 m/s spike, then 1.223 (< 2.60), so it fired at 0.1389 s, 56 ms early. Snap-backs of 4.272, 1.955, 1.093 and 0.949 m/s up to 0.403 s put the re-arm at 0.5556 s, 69 ms late (bound 53.9).
    - held30 `blink-left.slow` p0: no early fire (0.3472 s, 42 ms late, from the grid). The recoil after the flick peaks at 0.541 m/s at 72 Hz, just under 0.60. Held samples change every 2 or 3 frames for a 1/30 s update, so `FHeldPointRate` divides one camera step by 27.8 or 41.7 ms. The recoil then reads 0.640 and 0.635 m/s (0.472 and 0.500 s) and resets the quiet clock. The re-arm waits for the tip to stop: 0.6944 s, 236 ms late.
    - So the early fire is the camera grid read as the fall. The late re-arm is motion that is not there: the overshoot (extrapolated) or the update-time rounding (held). Neither depends on the exact size of the overshoot.
  - **Fix:** three changes in `FBlinkDetector::Ingest`. Each new constant's comment names the classes it sits between.
    - The fall is the speed **along the hottest sample's direction**, and it is judged before the sample may become the hottest. With the grid veto alone, extrap25 `sloppy` at a phase outside the three tested (11/12 of a period) turned the stop's 9.08 m/s snap-back (yaw 117°) into a new hottest sample and fired the wrong way. Measured along the flick, a snap-back is a fall.
    - A fall counts only when the speed over `FallSpanS` (40 ms: one 25 Hz period, three frames at 72 Hz) is under `FallSpanPeakFraction` (0.875) of its peak since arming. Over a whole camera period the speed is still at its peak while the tip speeds up. Measured on the 12 blink and bolt clips, on all five streams at three phases:
      - every fall that fired reads at most 0.854 (0.843 at 72 Hz);
      - every vetoed grid dip reads at least 0.900, and 206 of the 223 read exactly 1.000;
      - spans of 25 to 40 ms give the same results. A 45 ms span (four frames) lags the 72 Hz fall and moves four 72 Hz fires.
    - A latched sample also counts as quiet when the tip is back within `RearmSpeedMps` of where it was `RearmSpanMinS` to `RearmSpanMaxS` (40 to 120 ms) ago. An overshoot and its snap-back cancel over that span, and so does the held rounding. A recoil that keeps going at 0.60 m/s never cancels, so the latch still holds through it. Measured around these values:
      - a lower end of 28 to 50 ms, or an upper end of 100 to 150 ms, gives the same results;
      - an upper end of 80 ms leaves held30 at 24/27;
      - 200 ms loses cases on three streams.
    - Rejected along the way, all measured in the replica:
      - A minimum time from peak to fall. 72 Hz `sloppy` falls 27.8 ms after its peak, as long as an extrap25 dip lasts.
      - A peak held over two frames. It moves the 72 Hz `bolt.normal` fire.
      - A net-displacement re-arm over the whole quiet window. It cancelled the rest of the flick against the recoil, and on extrap30 `sloppy` p0 it re-armed into a false second blink.
  - **72 Hz, before and after:** from `MageArena.Blink.Timing72`, a new test that pins these values. Frames are 72 Hz indices from the clip start, with a 0.5 s still tail. The before column is the same test on the base detector (the mutation run):

    | Clip | Before: count, direction, fire / event / re-arm frame | After |
    |---|---|---|
    | blink-left normal | 1 left, 14 / 11 / 35 | identical |
    | blink-left slow | 1 left, 22 / 17 / 33 | identical |
    | blink-left sloppy | 1 left, 12 / 10 / 38 | identical |
    | blink-right normal | 1 right, 14 / 11 / 35 | identical |
    | blink-right slow | 1 right, 22 / 17 / 33 | identical |
    | blink-right sloppy | 1 right, 12 / 10 / 36 | identical |
    | blink-back normal | 1 back, 14 / 11 / 35 | identical |
    | blink-back slow | 1 back, 22 / 17 / 33 | identical |
    | blink-back sloppy | 1 back, 12 / 10 / 36 | identical |
    | bolt normal | 1 bolt, 15 / 12 / 38 | identical |
    | bolt slow | 1 bolt, 24 / 19 / 35 | identical |
    | bolt sloppy | 1 bolt, 14 / 11 / 38 | identical |

    The re-arm did not move either, so the 72 Hz reference of `BlinkModels` is the same before and after. `Blink.Clips` (sigil, mudra and ward clips: no blink, no bolt) and `Blink.Stream` pass unchanged.
  - **Modelled streams, before and after:** from `HeldSample.BlinkModels`, 27 cases per stream. The bound is in brackets. Fire is the fire frame against 72 Hz; negative is early.

    | Stream | Re-arm within bound, before | after | Re-arm delay ms, before | after | Fire vs 72 Hz ms, before | after |
    |---|---|---|---|---|---|---|
    | held30 (47.2 ms) | 18/27 | 27/27 | -27.8 to +236.1 | -27.8 to +41.7 | +13.8 to +41.7 | +13.8 to +55.5 |
    | extrap30 (47.2 ms) | 14/27 | 27/27 | +13.9 to +180.6 | -13.9 to +41.7 | -125.0 to -27.8 | -27.8 to +13.9 |
    | held25 (53.9 ms) | 27/27 | 27/27 | -41.7 to +41.7 | -41.7 to +41.7 | 0.0 to +55.5 | 0.0 to +69.4 |
    | extrap25 (53.9 ms) | 2/27 | 27/27 | +27.8 to +194.4 | -27.8 to +41.7 | -125.0 to -13.9 | -27.8 to +27.7 |

    - Early fire by variant, before → after:

      | Stream | normal | slow | sloppy |
      |---|---|---|---|
      | extrap30 | -69 to -56 → -28 to 0 ms | -125 to -111 → -28 to -14 ms | -56 to -28 → 0 to +14 ms |
      | extrap25 | -69 to -42 → 0 to +14 ms | -125 to -97 → -28 to -14 ms | -56 to -14 → +14 to +28 ms |

      Cases that fire early: extrap30 27/27 → 15/27, extrap25 27/27 → 9/27. None is now more than 27.8 ms (two frames) early. The blink's time stamp is the hottest sample's time, so it moves less than the fire frame.
    - The cost: on the held streams some fires now come one frame (13.9 ms) later, where the span veto waits out a stepped sample. held30 sloppy goes from +41.6 to +55.5 ms. held25 normal goes from +41.7 to +55.6 ms, and held25 sloppy from +55.5 to +69.4 ms.
    - Count and direction match 72 Hz in 27/27 cases on every stream, with no false blink and no bolt. `KnownModelMisses` lost all 47 `.rearm` entries and is now empty. `KnownHeldMisses` stays empty.
    - Off the tested grid, in the replica only (not in the suite), at 12 camera phases each, with the extrapolation horizon at one period and at half a period:
      - Every extrapolated stream from 20 to 60 Hz re-arms within its bound in 108/108 cases. Before, the range was 8 to 108.
      - Held streams: 25 Hz 93 → 105, 30 Hz 81 → 99, 36 Hz 93 → 96, 45 Hz 72 → 108, 60 Hz 81 → 108. At 20 Hz it is worse: 90 → 87.
      - Count and direction match in 108/108 on every stream from 25 to 60 Hz, before and after. Held 20 Hz is 105/108 before and after.
  - **Proven:**
    - `build.ps1` exits 0: 120 s with the fix, then 17 s for the mutation build and 16 s for the restore.
    - The full `MageArena` suite, after regenerating the corpus with the CLAUDE.md commands: 178 tests, 175 pass. The failures are the 3 known ones (`MageArenaDesign.Duel.FireSeated`, `Session.FullSeated`, `Session.Wave1Seated`). Every `HeldSample.*` test passes, as do every `Blink.*` test (`Clips`, `Stream`, `Timing72`) and `Census.Sweep`. The full suite was not run on the base commit in this run; the base figures come from the mutation run.
    - Mutation proof: the base `BlinkDetector.h` and `.cpp`, run under the new ratchet, fail `HeldSample.BlinkModels` with 47 "re-arms late or not at all and is not in KnownModelMisses" errors, one per removed entry. `Timing72` still passes, as it should, because 72 Hz did not move. With the fix restored, the `Blink` and `HeldSample` tests are 14/14 green.
    - `check-pin.mjs` prints `pin OK: 9 files @ baeac66`, `conformance/generate.mjs` exits 0, and `git status --porcelain apps/vr/data` prints nothing.
  - **Not proven:** real Quest input. The fix rests on the G1 extrapolation **model** and on the held worst case. It was designed not to depend on the model's exact overshoot: in the replica it holds with the horizon cut to half a period, and at 20 to 60 Hz. But Meta's extrapolation method, its horizon and its filtering are unknown, and a filtered runtime may show no spikes at all. A second flick within 0.2 s is covered only through the re-arm bound. No two-flick stream runs on the modelled streams.

### G2. The v207 path never exposes when the cameras sampled, and the recorder rule assumes held poses

- **Upstream statements:**
  - The same Meta page describes `captureTime`: a "value indicating when the tracking data was captured. Applications
    can use this timestamp to understand the age of the tracking data relative to the predicted display time."
  - OpenXR 1.1 spec, `XR_META_hand_tracking_unextrapolated_poses`: "the runtime must provide the latest calculated hand
    poses without temporal extrapolation." https://registry.khronos.org/OpenXR/specs/1.1/html/xrspec.html
  - FMM page: run `adb logcat -e "Camera FPS"`; expected output "HandTrackingService: Hand Tracking Processed FPS =
    58.5 Hz / Camera FPS = 60.1".
- **Installed source:**
  - `SampleTimeStamp` is set to the display time (`OculusXRInputHandTrackingExtensionPlugin.cpp:644`).
  - `XrHandJointLocationsEXT` has no time field (`openxr.h:2618-2624`).
  - The engine headers (OpenXR 1.1.46, `openxr.h:29`) do not declare the unextrapolated extension: grep finds nothing (log 2).
  - OVRPlugin 207 exports `ovrp_GetUnextrapolatedHandState` (`MetaXR/Source/Thirdparty/OVRPlugin/OVRPlugin/Include/OVR_Plugin.h:1016`),
    but no `.cpp` in the plugin calls it. Whether it works under `XrApi=NativeOpenXR` is **unconfirmed**.
- **Repo:**
  - `docs/PROJECT-PLAN.md:202`, `:331` and `:390` promise frames "with sensor time". The v207 path cannot provide that.
  - `docs/CLIP-SCHEMA.md:149` says that when the hands update slower than 72 Hz, "the same pose repeats on consecutive
    lines". With default extrapolation it will not. A device recording will look smooth at 72 Hz, and its repeats
    cannot measure the camera rate.
- **Recommended change:**
  - Before the gate, in text only:
    - change "sensor time" to "display time" in the three plan rows;
    - rewrite `CLIP-SCHEMA.md:149` to say recorded poses are extrapolated to display time by default;
    - add an optional capture-time field for a later schema version.
  - On V1 day one, as a 10-minute owner step:
    - run `adb logcat -e "Camera FPS"` at LOW with hands in view, on each headset available;
    - record the line.
    - This settles Q1 for that device.
  - In the P3 recorder, at V1:
    - check `xrEnumerateInstanceExtensionProperties` for `XR_META_hand_tracking_unextrapolated_poses`;
    - if it is there, log the unextrapolated poses and `captureTime` beside the predicted ones;
    - declare the structs by hand from the spec, since the engine header lacks them.
    - That gives the real camera rate and the prediction horizon on every trace. It also lets V2 replay either stream.
  - This needs an OpenXR module dependency in `MageArenaVR.Build.cs`. Another worker holds that file, as in F8.
- **Status:** text applied 2026-10-07 in 8e30100. Before, the plan rows said "sensor time" and `CLIP-SCHEMA.md` said repeated poses mark the camera rate. After, `PROJECT-PLAN.md:202`, `:331` and `:390` say "display time"; `CLIP-SCHEMA.md:149` says recorded poses are extrapolated to display time by default and reserves `captureTime` for a later schema version; the V1 week row (`PROJECT-PLAN.md:227`) has the `Camera FPS` day-one step in its measured-by text.
  - **Proven:** the text is in place on master.
  - **Not proven or not done:** the day-one probe and the recorder's `captureTime`, both V1. The V1 day-one probe also checks whether the runtime extrapolates pinch strength. In G1's model, extrapolated pinch tripled the zigzag false casts at 30 Hz, from 9/180 to 30/180.
- **Cost:** M (text S).
- **When:** the text before the gate; the probe and recorder at V1.
- **Verifiable headless:** the text and the schema yes; the rate and horizon no.
- **Registry:** N6 `tracked-input-record-and-replay` (record what the runtime extrapolated, and when it sampled) and N2.

### G3. The manifest says controllers are supported, but the game has no controller path

- **Upstream statements:**
  - VRC.Quest.Input.7 (updated 2024-07-31): "the app must properly respect when input is switched between controllers
    and hands." https://developers.meta.com/horizon/resources/vrc-quest-input-7/
  - Meta app compatibility (updated 2026-09-04): "Declaring oculus.software.handtracking with required="true" means
    hands only." Also: "Do not ship the declaration before the hand-input path works."
    https://developers.meta.com/vr/essentials/app-compatibility/
  - Unreal hand tracking (updated 2026-04-14), Hands Only: "A user must have hand tracking enabled on their device to
    use your app". https://developers.meta.com/vr/documentation/unreal/unreal-hand-tracking/
- **Installed source:** the APL writes `android:required="$B(bHandsOnly)"` (`OculusMobile_APL.xml:202`). Only
  `HandsOnly` makes it true. `HandsOnly` also writes the permission and the frequency tag (`:196-202`, `:376-382`), so
  the comment at `DefaultEngine.ini:50` ("ControllersAndHands is what writes the hand-tracking manifest tags") is true
  of both modes.
- **Repo:**
  - `apps/vr/Game/Config/DefaultEngine.ini:52` sets `HandTrackingSupport=ControllersAndHands`.
  - `apps/vr/tools/package-android.ps1:162` asserts the feature name only, not `required`.
  - No source file reads a motion controller.
  - The competition asks for "Fully usable with hands end-to-end. Controller support optional" (`PROJECT-PLAN.md:102`).
  - A player who starts with controllers in hand gets a game that waits for hands. Nothing tells them to put the
    controllers down, which is what Input.7 checks.
- **Recommended change:**
  - At V1, once hands drive the game on device, set `HandTrackingSupport=HandsOnly`.
  - Extend `package-android.ps1` to assert `oculus.software.handtracking` with `required="true"`.
  - Do it after hands work, as Meta's page says, not before the gate.
  - The alternative is to keep `ControllersAndHands` and add a "put the controllers down" screen on the switch. That
    is more work for no gain in a hands-only game.
- **Status:** not applied. Scheduled for V1, after hands work on device: `HandsOnly`, plus a `required="true"` assertion in `package-android.ps1`, as Meta's page says.
- **Cost:** S.
- **When:** V1, after the first on-device hands run.
- **Verifiable headless:** yes, the manifest marker.
- **Registry:** EXTENDS `ship-pipeline-gating` (assert the manifest's input declaration, not only its presence).

### G4. MAX equals HIGH; HIGH costs headroom, adds jitter, and runs at 50 Hz on 50 Hz mains

- **Upstream statements:**
  - FMM page (updated 2026-08-11): "There is no difference between High and Max values as both track hands at high
    frequency." Also: "Tracking quality regresses in low lighting conditions". And: 50 Hz in FMM on 50 Hz mains.
  - Hands technology page (updated 2026-08-17): "FMM offers a tracking rate to 60Hz" and "The increased tracking rate
    can create a jitter".
  - Meta v28 notes (2021-04-30, Quest 2): HIGH apps "are downclocked to CPU level 3 and GPU level 2", LOW apps to
    "CPU level 3 and GPU level 3". https://developers.meta.com/vr/blog/oculus-developer-release-notes-v28/
- **Installed source:** LOW, HIGH and MAX exist (`OculusXRHMDTypes.h:144-149`). They only reach the manifest
  (`OculusMobile_APL.xml:376-382`).
- **Repo:**
  - STACK F7(c) reads "Never choose MAX" (`docs/research/STACK-OPPORTUNITIES-2026-10.md:293`). It is the same as HIGH, so the rule is true but
    the reason is not "MAX is worse".
  - The A/B lists recall, accuracy, false casts and "the headroom cost", with no instrument.
- **Recommended change, for the STACK owner and the V2 card:**
  - Reword F7(c): MAX equals HIGH.
  - Make the A/B log:
    - the `Camera FPS` logcat line, to prove which rate ran (50 or 60 Hz);
    - the CPU and GPU levels from OVR Metrics, because HIGH may cap the GPU a level lower (2021 figure, **unconfirmed**
      on Quest 3);
    - the room light, since FMM regresses in low light.
  - Keep LOW as the default (F7(a)).
- **Status:** text applied 2026-10-07 in 6562a93. Before, STACK F7(c) said "Never choose MAX" with no reason and the A/B had no instrument. After, F7(c) says MAX equals HIGH, that the A/B is LOW against HIGH, and that it logs the `Camera FPS` line, the OVR Metrics CPU and GPU levels, and the room light.
  - **Not done:** the A/B itself, V2.
- **Cost:** S on top of F7(c)'s M.
- **When:** V2.
- **Verifiable headless:** no.
- **Registry:** N2 and N5 `standalone-headset-frame-budgets`.

## 4. Feasibility risks

Ranked by how likely each one is to sink the entry.

1. **The ward perfect window on real hands (plan R3).**
   - **The evidence:** the default rate is 30 Hz (confirmed), and the poses are extrapolated (confirmed). F7(b) shows
     the window is lost on held hands. The real case lies between held and ideal.
   - **What retires it:** the onset lag measured on device. The flash method (V2) gives photon-to-detection. The G2
     probe gives the camera rate and the extrapolation horizon.
   - **How early:** the rate on V1 day one (10 minutes). The lag at V2 (8 Nov).
   - **Desktop part:** G1 can retire the model question before 10-31. It shows whether the detector survives an
     extrapolated 30 Hz or 25 Hz stream, so the gate knows if W1 (the 0.20 s window) or W2 (onset compensation) is
     needed. It cannot retire the true latency.
     - **Status 2026-10-07 (G1, 1a7487d):** on modelled 30 Hz hands, held and extrapolated, the hit at the authored tick
       and the interior hits are perfect in 36/36 cases, with 0 to 100 ms of injected latency. The onset lands 14 to 42 ms
       late from the camera grid, independent of latency, so strict ±1 frame holds in 12/36. W1 is not needed. W2 waits
       for the camera rate from the V1 day-one probe. The true latency still needs V2.
2. **Tracking loss and jitter on fast gestures.**
   - **The evidence:** blink is a flick (arming at 1.2 m/s). Meta advises FMM only "if you observe high tracking loss
     due to fast hand motion" (STACK F7), and FMM costs jitter and headroom.
   - **What retires it:** on V1, 20 blink flicks per direction at LOW, with `isActive` and confidence logged per frame.
     If the loss rate is high, run the G4 A/B early, not at V2.
   - **How early:** V1 (4 Nov).
   - **Desktop part:** only the authored dropout noise in the clips. It cannot say how often the real runtime drops a
     fast hand.
3. **Quest 3S is declared but may never be tried.**
   - **The evidence:** the owner chose Quest 3 and 3S (DECISIONS 2026-10-07). The device named is a Quest 3
     (`docs/DECISIONS.md:122`). The 3S has a lower-resolution panel, and its lens, FOV and hand-tracking quality are
     unconfirmed first-party.
   - **Owner choice (2026-10-07):** ship 3S untested; the risk is accepted (DECISIONS 2026-10-07, Quest 3S ships untested).
   - **What retires it:** one 3S session in V3: sigil legibility, peripheral threat cues, the `Camera FPS` line, and a
     perf capture. The owner chose not to hold one, so the risk stays open and accepted.
   - **How early:** V3, only if a 3S is ever borrowed or bought. No owner decision is pending before the 31 Oct order.
   - **Desktop part:** the narrow-FOV mode (D3) bounds the FOV part. Perf carries over by chip and eye buffer (Q3).
     Legibility and tracking do not.

## 5. Checked, no change needed

- **LOW stays.** Default mode is 30 Hz, and HIGH adds jitter (s1). F7(a) holds.
- **Hands are sampled once per game frame.** The late update does not re-sample them (`OpenXRHMD.cpp:3799`, `:3993`
  pass `false`). The detectors see game-thread poses, which is what the clips model.
- **Both hand paths use the same time.** The engine `OpenXRHandTracking` and the Meta `OculusXRInput` both pass the
  predicted display time, so it does not matter which one feeds the live `IHandSource`.
- **72 Hz on 3S.** The same refresh rates are supported on both headsets (s11, 2026-10-01).
- **Section 7 budgets on 3S.** Same chip and RAM (s10), same default eye buffer (s9). No separate 3S budget is needed
  at the default render scale.
- **Hand permission and feature.** `package-android.ps1:161-162` asserts both. Without them, hands are not enumerated
  (Meta native hand tracking page).
- **System gesture (VRC.Quest.Input.8).** Unchanged since 2024-07-31. F8 covers it.
- **Tracking loss.** No VRC requires a behaviour. The plan's pause on tracking loss goes beyond the rules.
- **Wide Motion Mode.** It is not in the shipped v207 Unreal code path (`OculusXRHMD.cpp:3487-3493`), so there is
  nothing to configure.
- **Frequency hint extension.** It is advisory, and the engine headers lack it. The manifest stays the knob.

## 6. Sources

| # | URL | Page date | HTTP on 2026-10-07 |
|---|---|---|---|
| s1 | https://developers.meta.com/horizon/design/hands-technology/ | 2026-08-17 | 200 |
| s2 | https://developers.meta.com/horizon/documentation/unity/fast-motion-mode/ | 2026-08-11 | 200 |
| s3 | https://developers.meta.com/horizon/documentation/unreal/unreal-plugin-settings/ (LOW/HIGH/MAX list) | 2026-04-14 | 200 |
| s4 | same as s3 (the headroom sentence) | 2026-04-14 | 200 |
| s5 | https://developers.meta.com/vr/blog/oculus-developer-release-notes-v28/ | 2021-04-30 | 200 |
| s6 | https://developers.meta.com/horizon/documentation/native/android/native-openxr-hand-tracking-frequency-hint/ | 2026-04-24 | 200 |
| s7 | https://developers.meta.com/horizon/documentation/native/android/native-openxr-hand-tracking-unextrapolated-poses/ | 2026-04-24 | 200 |
| s8 | https://registry.khronos.org/OpenXR/specs/1.1/html/xrspec.html | spec 1.1.63 (extension last modified 2025-09-30) | 200 |
| s9 | https://developers.meta.com/horizon/documentation/native/android/os-render-scale/ | 2025-01-09 | 200 |
| s10 | https://developers.meta.com/vr/blog/start-building-meta-quest-3s-3-launch-spatial-sdk-2D-mixed-reality/ | blog (date not checked) | 200 |
| s11 | https://developers.meta.com/horizon/documentation/unreal/unreal-change-display-refresh-rate/ | 2026-10-01 | 200 |
| s12 | https://developers.meta.com/horizon/resources/vrc-quest-input-7/ | 2024-07-31 | 200 |
| s13 | https://developers.meta.com/vr/essentials/app-compatibility/ | 2026-09-04 | 200 |
| s14 | https://developers.meta.com/vr/documentation/unreal/unreal-hand-tracking/ | 2026-04-14 | 200 |

**Read by the sub-agent only, not re-downloaded by me:**

- VRC.Quest.Input.1 to 8 (9 to 11 return 404).
- The Hands 2.2 blog (Aug 21, 2023; "up to 40% latency reduction in typical usage" was re-found, log 1).
- The native "Enable Hand Tracking" page ("Apps that do not specify these flags will not see hand devices enumerated").
- The Devpost rules page: maintenance page with HTTP 200, and 403 under WebFetch, so it was **not read** in the first pass. It was **read** later on 2026-10-07 (HTTP 200; see the Status line under "Not re-read" in Q4).

## 7. Commands run for this file, with exit codes

**Tool calls that are not shell commands, so they have no exit code:**

- A research sub-agent ran 12 WebSearch queries and about 60 fetches. Its log is not reproduced here.
- Nothing in this file rests on that sweep alone. Each quote was re-found by the `grep` commands below. The exceptions
  are the items listed as sub-agent-only in section 6.

**Shell, run from the worktree root on 2026-10-07.** Pages went into a fresh `mktemp -d` directory outside the repo.
The script that ran the downloads lived in a separate directory.

- An empty match prints nothing.
- The `[exit N]` after a grep is grep's own status (`PIPESTATUS[0]`).
- For `xrspec`, the HTML was tag-stripped with `sed -e 's/<[^>]*>//g'` into `xrspec.txt` (copied as `xrspectxt.html`).
- Three long phrases failed on the first pass, because Meta and Khronos split them across markup. Their pieces were
  re-checked in log 1b.

**Log 1: downloads and quote checks**

```
$ curl -sL -A Mozilla/5.0 --max-time 60 -o /tmp/mav-feas-web-ktRj/htech.html -w '%{http_code} %{size_download} htech\n' 'https://developers.meta.com/horizon/design/hands-technology/'
200 640678 htech
[exit 0]
$ curl -sL -A Mozilla/5.0 --max-time 60 -o /tmp/mav-feas-web-ktRj/ups.html -w '%{http_code} %{size_download} ups\n' 'https://developers.meta.com/horizon/documentation/unreal/unreal-plugin-settings/'
200 694028 ups
[exit 0]
$ curl -sL -A Mozilla/5.0 --max-time 60 -o /tmp/mav-feas-web-ktRj/fmm.html -w '%{http_code} %{size_download} fmm\n' 'https://developers.meta.com/horizon/documentation/unity/fast-motion-mode/'
200 675112 fmm
[exit 0]
$ curl -sL -A Mozilla/5.0 --max-time 60 -o /tmp/mav-feas-web-ktRj/fhint.html -w '%{http_code} %{size_download} fhint\n' 'https://developers.meta.com/horizon/documentation/native/android/native-openxr-hand-tracking-frequency-hint/'
200 641116 fhint
[exit 0]
$ curl -sL -A Mozilla/5.0 --max-time 60 -o /tmp/mav-feas-web-ktRj/bv28.html -w '%{http_code} %{size_download} bv28\n' 'https://developers.meta.com/vr/blog/oculus-developer-release-notes-v28/'
200 656699 bv28
[exit 0]
$ curl -sL -A Mozilla/5.0 --max-time 60 -o /tmp/mav-feas-web-ktRj/xrspec.html -w '%{http_code} %{size_download} xrspec\n' 'https://registry.khronos.org/OpenXR/specs/1.1/html/xrspec.html'
200 24851290 xrspec
[exit 0]
$ curl -sL -A Mozilla/5.0 --max-time 60 -o /tmp/mav-feas-web-ktRj/unext.html -w '%{http_code} %{size_download} unext\n' 'https://developers.meta.com/horizon/documentation/native/android/native-openxr-hand-tracking-unextrapolated-poses/'
200 658927 unext
[exit 0]
$ curl -sL -A Mozilla/5.0 --max-time 60 -o /tmp/mav-feas-web-ktRj/b22.html -w '%{http_code} %{size_download} b22\n' 'https://developers.meta.com/vr/blog/hand-tracking-22-response-time-meta-quest-developers/'
200 638845 b22
[exit 0]
$ curl -sL -A Mozilla/5.0 --max-time 60 -o /tmp/mav-feas-web-ktRj/rscale.html -w '%{http_code} %{size_download} rscale\n' 'https://developers.meta.com/horizon/documentation/native/android/os-render-scale/'
200 698767 rscale
[exit 0]
$ curl -sL -A Mozilla/5.0 --max-time 60 -o /tmp/mav-feas-web-ktRj/b3s.html -w '%{http_code} %{size_download} b3s\n' 'https://developers.meta.com/vr/blog/start-building-meta-quest-3s-3-launch-spatial-sdk-2D-mixed-reality/'
200 694607 b3s
[exit 0]
$ curl -sL -A Mozilla/5.0 --max-time 60 -o /tmp/mav-feas-web-ktRj/refresh.html -w '%{http_code} %{size_download} refresh\n' 'https://developers.meta.com/horizon/documentation/unreal/unreal-change-display-refresh-rate/'
200 704557 refresh
[exit 0]
$ curl -sL -A Mozilla/5.0 --max-time 60 -o /tmp/mav-feas-web-ktRj/vrcin7.html -w '%{http_code} %{size_download} vrcin7\n' 'https://developers.meta.com/horizon/resources/vrc-quest-input-7/'
200 673785 vrcin7
[exit 0]
$ curl -sL -A Mozilla/5.0 --max-time 60 -o /tmp/mav-feas-web-ktRj/appcompat.html -w '%{http_code} %{size_download} appcompat\n' 'https://developers.meta.com/vr/essentials/app-compatibility/'
200 603970 appcompat
[exit 0]
$ curl -sL -A Mozilla/5.0 --max-time 60 -o /tmp/mav-feas-web-ktRj/uht.html -w '%{http_code} %{size_download} uht\n' 'https://developers.meta.com/vr/documentation/unreal/unreal-hand-tracking/'
200 716412 uht
[exit 0]
$ grep -o -m1 'Offers hand tracking at 30Hz' /tmp/mav-feas-web-ktRj/htech.html | head -1
Offers hand tracking at 30Hz
[exit 0]
$ grep -o -m1 'FMM offers a tracking rate to 60Hz' /tmp/mav-feas-web-ktRj/htech.html | head -1
FMM offers a tracking rate to 60Hz
[exit 0]
$ grep -o -m1 'The increased tracking rate can create a jitter' /tmp/mav-feas-web-ktRj/htech.html | head -1
The increased tracking rate can create a jitter
[exit 0]
$ grep -o -m1 'Higher tracking frequencies will reserve some performance headroom' /tmp/mav-feas-web-ktRj/ups.html | head -1
Higher tracking frequencies will reserve some performance headroom
[exit 0]
$ grep -o -m1 'There is no difference between High and Max values' /tmp/mav-feas-web-ktRj/fmm.html | head -1
[exit 1]
$ grep -o -m1 'they will see 50Hz in FMM' /tmp/mav-feas-web-ktRj/fmm.html | head -1
they will see 50Hz in FMM
[exit 0]
$ grep -o -m1 'Hand Tracking Processed FPS' /tmp/mav-feas-web-ktRj/fmm.html | head -1
Hand Tracking Processed FPS
[exit 0]
$ grep -o -m1 'Tracking quality regresses in low lighting conditions' /tmp/mav-feas-web-ktRj/fmm.html | head -1
Tracking quality regresses in low lighting conditions
[exit 0]
$ grep -o -m1 'By default, the runtime uses a power-efficient tracking frequency' /tmp/mav-feas-web-ktRj/fhint.html | head -1
By default, the runtime uses a power-efficient tracking frequency
[exit 0]
$ grep -o -m1 'The frequency hint is advisory only' /tmp/mav-feas-web-ktRj/fhint.html | head -1
The frequency hint is advisory only
[exit 0]
$ grep -o -m1 'low frequency hand tracking are downclocked to CPU level 3 and GPU level 3' /tmp/mav-feas-web-ktRj/bv28.html | head -1
low frequency hand tracking are downclocked to CPU level 3 and GPU level 3
[exit 0]
$ grep -o -m1 'high frequency hand tracking are downclocked to CPU level 3 and GPU level 2' /tmp/mav-feas-web-ktRj/bv28.html | head -1
high frequency hand tracking are downclocked to CPU level 3 and GPU level 2
[exit 0]
$ grep -o -m1 'time is an XrTime at which to locate the hand joints' /tmp/mav-feas-web-ktRj/xrspectxt.html | head -1
time is an XrTime at which to locate the hand joints
[exit 0]
$ grep -o -m1 'most up-to-date prediction of how the world will be at that future time' /tmp/mav-feas-web-ktRj/xrspectxt.html | head -1
[exit 1]
$ grep -o -m1 'without temporal extrapolation' /tmp/mav-feas-web-ktRj/xrspectxt.html | head -1
without temporal extrapolation
[exit 0]
$ grep -o -m1 'returns hand poses extrapolated to the requested display time' /tmp/mav-feas-web-ktRj/unext.html | head -1
[exit 1]
$ grep -o -m1 'The runtime predicts hand positions forward in time' /tmp/mav-feas-web-ktRj/unext.html | head -1
The runtime predicts hand positions forward in time
[exit 0]
$ grep -o -m1 'captureTime' /tmp/mav-feas-web-ktRj/unext.html | head -1
captureTime
[exit 0]
$ grep -o -m1 'up to 40% latency reduction in typical usage' /tmp/mav-feas-web-ktRj/b22.html | head -1
up to 40% latency reduction in typical usage
[exit 0]
$ grep -o -m1 '2064x2208' /tmp/mav-feas-web-ktRj/rscale.html | head -1
2064x2208
[exit 0]
$ grep -o -m1 '1832x1920' /tmp/mav-feas-web-ktRj/rscale.html | head -1
1832x1920
[exit 0]
$ grep -o -m1 '1680x1760' /tmp/mav-feas-web-ktRj/rscale.html | head -1
1680x1760
[exit 0]
$ grep -o -m1 'same power and performance as Quest 3' /tmp/mav-feas-web-ktRj/b3s.html | head -1
same power and performance as Quest 3
[exit 0]
$ grep -o -m1 'does not support rates above 120' /tmp/mav-feas-web-ktRj/refresh.html | head -1
does not support rates above 120
[exit 0]
$ grep -o -m1 'respect when input is switched between controllers and hands' /tmp/mav-feas-web-ktRj/vrcin7.html | head -1
respect when input is switched between controllers and hands
[exit 0]
$ grep -o -m1 'means hands only' /tmp/mav-feas-web-ktRj/appcompat.html | head -1
means hands only
[exit 0]
$ grep -o -m1 'Do not ship the declaration before the hand-input path works' /tmp/mav-feas-web-ktRj/appcompat.html | head -1
Do not ship the declaration before the hand-input path works
[exit 0]
$ grep -o -m1 'A user must have hand tracking enabled on their device to use your app' /tmp/mav-feas-web-ktRj/uht.html | head -1
A user must have hand tracking enabled on their device to use your app
[exit 0]
$ grep -o -m1 'lastUpdated[^,]*' /tmp/mav-feas-web-ktRj/htech.html | head -1
"lastUpdated\":\"2026-08-17\"}
[exit 0]
$ grep -o -m1 'lastUpdated[^,]*' /tmp/mav-feas-web-ktRj/ups.html | head -1
"lastUpdated\":\"2026-04-14\"}
[exit 0]
$ grep -o -m1 'lastUpdated[^,]*' /tmp/mav-feas-web-ktRj/fmm.html | head -1
"lastUpdated\":\"2026-08-11\"}
[exit 0]
$ grep -o -m1 'lastUpdated[^,]*' /tmp/mav-feas-web-ktRj/fhint.html | head -1
"lastUpdated\":\"2026-04-24\"}
[exit 0]
$ grep -o -m1 'lastUpdated[^,]*' /tmp/mav-feas-web-ktRj/unext.html | head -1
"lastUpdated\":\"2026-04-24\"}
[exit 0]
$ grep -o -m1 'lastUpdated[^,]*' /tmp/mav-feas-web-ktRj/rscale.html | head -1
"lastUpdated\":\"2025-01-09\"}
[exit 0]
$ grep -o -m1 'lastUpdated[^,]*' /tmp/mav-feas-web-ktRj/refresh.html | head -1
"lastUpdated\":\"2026-10-01\"}
[exit 0]
$ grep -o -m1 'lastUpdated[^,]*' /tmp/mav-feas-web-ktRj/vrcin7.html | head -1
"lastUpdated\":\"2024-07-31\"}
[exit 0]
$ grep -o -m1 'lastUpdated[^,]*' /tmp/mav-feas-web-ktRj/appcompat.html | head -1
"lastUpdated\":\"2026-09-04\"}
[exit 0]
$ grep -o -m1 'lastUpdated[^,]*' /tmp/mav-feas-web-ktRj/uht.html | head -1
"lastUpdated\":\"2026-04-14\"}
[exit 0]
```

**Log 1b: the three split phrases, re-checked in pieces**

```
$ grep -o -m1 'There is no difference between' /tmp/mav-feas-web-ktRj/fmm.html | head -1
There is no difference between
[exit 0]
$ grep -o -m1 'as both track hands at high frequency' /tmp/mav-feas-web-ktRj/fmm.html | head -1
as both track hands at high frequency
[exit 0]
$ grep -o -m1 'up-to-date prediction' /tmp/mav-feas-web-ktRj/xrspec.txt | head -1
up-to-date prediction
[exit 0]
$ grep -o -m1 'For a time in the future' /tmp/mav-feas-web-ktRj/xrspec.txt | head -1
For a time in the future
[exit 0]
$ grep -o -m1 'to the requested display time. The runtime predicts hand positions forward in time to reduce perceived latency' /tmp/mav-feas-web-ktRj/unext.html | head -1
to the requested display time. The runtime predicts hand positions forward in time to reduce perceived latency
[exit 0]
$ grep -o -m1 'HandTrackingService: Hand Tracking Processed FPS' /tmp/mav-feas-web-ktRj/fmm.html | head -1
HandTrackingService: Hand Tracking Processed FPS
[exit 0]
$ grep -o -m1 'Quest 2, Quest Pro, Quest 3, and all future devices' /tmp/mav-feas-web-ktRj/fmm.html | head -1
Quest 2, Quest Pro, Quest 3, and all future devices
[exit 0]
```

In the raw HTML of `fmm`, "High" and "Max" sit in `<strong>` children between the two pieces. In `unext`,
`xrLocateHandJointsEXT` and "extrapolated" sit in code and emphasis children before "to the requested display time".
In `xrspec.txt`, the sentence wraps after "based", which the tag strip keeps as a line break.

**Log 2: installed source.** `$P` is `/c/Users/kazda/kiro/mage-arena-vr/apps/vr/Game/Plugins/MetaXR/Source`, and `$E` is
`/c/Program Files/Epic Games/UE_5.8/Engine`. The paths are written out in full in the commands as run; they are
shortened here.

```
$ grep -n 'HandTracking' apps/vr/Game/Config/DefaultEngine.ini
52:HandTrackingSupport=ControllersAndHands
53:HandTrackingFrequency=LOW
54:HandTrackingVersion=Default
[exit 0]
$ grep -n 'oculus.software.handtracking\|com.oculus.handtracking.frequency' '$P/OculusXRHMD/OculusMobile_APL.xml'
202:						<addFeature android:name="oculus.software.handtracking" android:required="$B(bHandsOnly)"/>
380:					<addAttribute tag="$handTrackingFrequency" name="android:name" value="com.oculus.handtracking.frequency"/>
[exit 0]
$ sed -n 144,149p '$P/OculusXRHMD/Public/OculusXRHMDTypes.h'
enum class EOculusXRHandTrackingFrequency : uint8
{
	LOW,
	HIGH,
	MAX,
};
[exit 0]
$ grep -n 'HandLocateInfo.time\|SampleTimeStamp = \|RequestedTimeStamp = ' '$P/OculusXRInput/Private/OculusXRInputHandTrackingExtensionPlugin.cpp'
356:			HandLocateInfo.time = InternalHandsState.PredictedDisplayTime;
643:			InternalHandsState.HandState[HandIndex].RequestedTimeStamp = InternalHandsState.PredictedDisplayTime;
644:			InternalHandsState.HandState[HandIndex].SampleTimeStamp = InternalHandsState.PredictedDisplayTime;
[exit 0]
$ grep -n 'LocateInfo.time' '$E/Plugins/Runtime/OpenXRHandTracking/Source/OpenXRHandTracking/Private/OpenXRHandTracking.cpp'
238:	LocateInfo.time = DisplayTime;
[exit 0]
$ grep -n 'UpdateDeviceLocations(' '$E/Plugins/Runtime/OpenXR/Source/OpenXRHMD/Private/OpenXRHMD.cpp'
1840:void FOpenXRHMD::UpdateDeviceLocations(bool bUpdateOpenXRExtensionPlugins)
1900:				Module->UpdateDeviceLocations(Session, PipelineState.FrameState.predictedDisplayTime, PipelineState.TrackingSpace->Handle);
3799:		UpdateDeviceLocations(false);
3993:				UpdateDeviceLocations(false);
4275:	UpdateDeviceLocations(true);
[exit 0]
$ grep -n 'XR_CURRENT_API_VERSION XR_MAKE\|typedef struct XrHandJointLocationsEXT' '$E/Source/ThirdParty/OpenXR/include/openxr/openxr.h'
29:#define XR_CURRENT_API_VERSION XR_MAKE_VERSION(1, 1, 46)
2618:typedef struct XrHandJointLocationsEXT {
[exit 0]
$ grep -rlc 'unextrapolated\|frequency_hint' '$E/Source/ThirdParty/OpenXR/include/openxr'
[exit 1]
$ grep -n 'ovrp_GetUnextrapolatedHandState' '$P/Thirdparty/OVRPlugin/OVRPlugin/Include/OVR_Plugin.h'
1016:OVRP_EXPORT ovrpResult ovrp_GetUnextrapolatedHandState(ovrpHand hand, ovrpHandState3* handState);
[exit 0]
$ grep -rn 'GetUnextrapolatedHandState' '$P' --include=*.cpp
[exit 1]
$ grep -n 'bWideMotionModeEnabled\|OVR_INTERNAL_CODE' '$P/OculusXRHMD/Private/OculusXRHMD.cpp' | sed -n 1,4p
2686:#if defined(OVR_INTERNAL_CODE)
3487:#ifdef OVR_INTERNAL_CODE
3488:		if (Settings->bWideMotionModeEnabled)
3493:#endif // OVR_INTERNAL_CODE
[exit 0]
$ grep -rc 'MotionController' apps/vr/Game/Source/MageArenaVR | grep -v ':0' | wc -l
0
[exit 0]
$ grep -n 'same pose repeats' docs/CLIP-SCHEMA.md
149:- Write at 72 Hz and keep repeated poses as they arrive, [...] the same pose repeats on consecutive lines. [...]
[exit 0]
$ grep -n 'sensor time' docs/PROJECT-PLAN.md
202:| **D-G1 Gesture clips** [...] JSONL joint frames with sensor time and confidence [...]
331:| `MageArenaInput` [...] Hand frames carry sensor time and are quantized to ticks. [...]
390:| VR and hand-tracking integration [...] writes JSONL joint frames with sensor time and confidence [...]
[exit 0]
$ grep -n 'Never choose MAX' docs/research/STACK-OPPORTUNITIES-2026-10.md
293:    minute, and the headroom cost. Never choose MAX.
[exit 0]
$ grep -n 'GCameraHz = \|GBoundHz = ' apps/vr/Game/Source/MageArenaVR/Tests/HeldSampleTests.cpp
27:constexpr double GCameraHz = 30.0;
29:constexpr double GBoundHz = 30.0;
[exit 0]
$ grep -n 'OnsetSpeedMps = \|MinTipSpeedMps = ' apps/vr/Game/Source/MageArenaVR/Gestures/WardDetector.h apps/vr/Game/Source/MageArenaVR/Gestures/BlinkDetector.h
apps/vr/Game/Source/MageArenaVR/Gestures/WardDetector.h:33:	static constexpr double OnsetSpeedMps = 0.30;
apps/vr/Game/Source/MageArenaVR/Gestures/BlinkDetector.h:44:	static constexpr double MinTipSpeedMps = 1.20;
[exit 0]
```

The long lines from `CLIP-SCHEMA.md` and `PROJECT-PLAN.md` are cut with `[...]`; everything else is verbatim.

**Log 3: the gate**

```
$ node apps/vr/tools/check-pin.mjs
pin OK: 9 files @ 68a4d68
[exit 0]
```

No build, no UnrealEditor and no UnrealBuildTool was run for this file.
