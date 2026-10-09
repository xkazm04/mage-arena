# Moonshot sweep 2026-10-09: 16 cards, 12 accepted

scan-sweep v4.5.0, ad-hoc lens `moonshot-architect` (not in the registered lens set, so it earns no coverage credit),
all 8 contexts, 2 L/XL cards per context, one read-only scout per context on `83372e7`. Triaged with the owner on
2026-10-09. Accepted items are the follow-up backlog; declined titles are a durable "no" and must not be re-proposed.
Corroboration counts the scouts that reached the same spine on their own. Premises are the scouts' readings of the
tree on 2026-10-09 and are re-verified by the builder before any code is written.

## Decision table

| Id | Item | Contexts | Size | Impact | Risk | Gate | Corr. | Lands | Decision |
|---|---|---|---|---|---|---|---|---|---|
| M1 | Resolve gesture conflicts in one arbiter, not by reading the playing clip name | sigil-recognition, hand-input-pipeline | XL | 9 | 5 | architecture | 2 | after T16 releases ArenaSession.cpp; before T24/T25 | accepted |
| M2 | Own one seated frame: map XR tracking space into seated-origin space once | arena-greybox | XL | 9 | 5 | architecture | 1 | before V1 device bring-up (4 Nov); before any XR IHandSource | accepted |
| M3 | Put all hand sources on one fixed 72 Hz input clock and feed the kernel by onset | hand-input-pipeline | XL | 8 | 6 | architecture | 2 | after T16; defines the clock F3 stamps; land before M4 | accepted |
| M4 | Step the whole session on the sim tick and journal its inputs for exact replay | duel-session | XL | 9 | 6 | architecture | 2 | after M3 and T16 | accepted |
| M5 | Compile pinned enemy prose into typed archetypes at load and gate pin bumps | spell-schools-and-ai | XL | 7 | 3 | architecture | 1 | any time; low risk, good first item | accepted |
| M6 | Inject kernel data into the sim and add a pin-diff census for re-pins | combat-sim-core | XL | 8 | 4 | architecture | 1 | after M5 (both touch KernelData load) | accepted |
| M7 | Make the kernel bit-identical on Win64, Quest and V8, and prove it on device | combat-sim-core | XL | 7 | 5 | architecture | 1 | desktop half any time; device half with V1 | accepted |
| M8 | Ship every VR overlay rule as replayable conformance vectors, not prose CRs | game-tests | XL | 8 | 5 | contract | 1 | after M5; CR format change needs TV agreement | accepted |
| M9 | Census a human-shaped player population through the real hand pipeline | game-tests | L | 8 | 4 | none | 1 | after ARCH-REVIEW N3 (FScriptedPlayer) | accepted |
| M10 | Replace the three hand-coded spell pickers with one data-driven rival brain | spell-schools-and-ai | XL | 8 | 6 | direction | 1 | after M9 is preferred (judge the brain against skill bands) | accepted |
| M11 | Lift the Tiro day off FGames.Phase into a typed, data-authored day graph | duel-session | XL | 8 | 5 | architecture | 1 | after ARCH-REVIEW F2/N1 and T16 | accepted |
| M12 | Enrol each player's own sigils in the teach and calibrate reject per player | sigil-recognition | L | 8 | 4 | direction | 1 | recognizer layer first (no gate); teach step after owner OK | accepted |
| - | Give the repo one verify command with a per-test ledger and a red ratchet | build-config | L | 8 | 3 | policy-tighten | 1 | - | declined |
| - | Compile the Quest flavour on every gate: clang parity, Shipping proof, no dev code | build-config | XL | 9 | 5 | architecture | 1 | - | declined |
| - | Gate every threat palette on CVD-aware legibility, in data and in frame | arena-greybox | XL | 8 | 3 | policy-tighten | 1 | - | declined |

Cards: M6 = T33, M7 = T34, M3 = T35, M4 = T36 (Determinism package, run order T16 -> T33 -> T34 and T35 -> T36; build machine only).
M3 and M4 are related but separate builds (the input clock, then the session on the sim tick with an input journal).
M1 folds two cards that describe one build. Gates `architecture` and `direction` were approved for the backlog by this
triage; `contract` (M8) still needs the TV repo's agreement before its CR format changes.

## M1a - Resolve gesture conflicts in one arbiter, not by reading the playing clip name

Context `sigil-recognition` · spine: gesture intent arbitration · XL · impact 9 · effort 6 · risk 5 · gate architecture

### Summary
Put one arbiter between the hand stream and the session. It owns the single OnHandFrame subscription and the single system-gesture gate, steps every detector core in a fixed order each frame, and turns their candidate events into one ordered stream of intents through explicit hand-claim rules. Today these conflicts are settled in the session by checking which clip is playing. On a headset no clip is playing, so those rules switch off silently.

### Description
The detectors are independent `UGameInstanceSubsystem`s. Each binds `OnHandFrame` and carries its own `FSystemGestureGate` (4 copies). None of them knows what the others claim. A two-hand staff grip pinches the casting hand, which is pen-down for the sigil builder. A pointing index finger drawing a fast bar is also a flick candidate for the blink detector. A both-palms raise is also a single ward raise. Today `FArenaSession` sorts this out with `ClipAction()`, the name of the desktop clip that is playing: `HandleSigil` drops a sigil while a bolt or blink clip plays, `HandleBolt` drops a bolt while a sigil clip plays, `IsSigilDrawing` ignores pen-down outside sigil clips, and `HandleWardRaised` ignores the palm while `both-palms` plays. The playing clip name is test metadata, and the headset has none. From V1 on, all four rules evaluate to "no clip", and the conflicts reach the player.

The synthetic clips hide one of these collisions completely. `staff-plant`, `staff-lift` and `mudra` end still pinched (0.9 / 0.9 / 0.85), so no test plays the grip release, which on real hands is the pen-up that ends the builder's stroke. That stroke has no loop, so `Decide` returns Rejected, `OnSigilRejected` fires, and the session pushes a `sigil-reject` cue and the reject visual after every staff plant. Two more hand-pose detectors live in `Session/` (`StonesTouched`, `BothPalmsRaised`), outside the system-gesture gate (F8 lists the both-palms one as not proven). Each new pose card (T24 mudra, T25 fist) would add another subscriber and another clip-name guard.

The move: `UGestureArbiterSubsystem` holds the detector cores (`FWardDetector`, `FBlinkDetector`, `FStaffDetector`, `FSigilStrokeBuilder` + `FQPointCloudRecognizer`, and the both-palms and stone pose tests moved out of Session). Each frame runs in three phases, following the registry technique `update-order-and-frame-coherence` (sense, decide, apply). Sense: every core ingests the frame. Decide: candidates are resolved against per-hand claims (a two-hand grip claims both hands, and a stroke that opened inside the claim is cancelled silently, not rejected; an open sigil pen claims the casting hand against flicks until pen-up plus a short hold; both palms claim the ward hand against a single raise). Apply: one `FGestureIntent` stream with kind, hand, onset time and payload. The existing per-detector delegates become thin views of that stream, so `HandPresentationComponent` and `GreyboxCapture` keep working. Following `event-dispatch-versus-direct-call`, it stays a synchronous subscription: nothing here has to happen later, only somewhere else.

### Flow
1. Write `GestureCrossTalkTests.cpp` first: the clip-by-intent matrix with the action name hidden, plus the two in-memory release cases. Record which cells are red on today's tree (that is the Before figure the card has not yet measured).
2. Add `GestureIntent.h` and `UGestureArbiterSubsystem`, which owns the single bind and gate and steps the cores in a fixed order. The detector subsystems re-publish from the arbiter (their delegates and accessors keep their signatures).
3. Add the claim rules one at a time, each turning one red cell green: grip claims hand, so no reject on release; pen claims the casting hand against flicks; both palms claim the ward hand.
4. Move `BothPalmsRaised` and `StonesTouched` into the arbiter as pose cores. The session reads intents, and the system gate now covers both.
5. Delete the five `ClipAction()` guards in the session handlers. Matrix green, the full suite at the same 3 known failures, recognizer and HeldSample numbers unchanged.
6. T24 and T25 then add a core and a claim row each, not a subsystem and a session guard.

### Expected impact
The Quest build (V1, 1-4 Nov) gets the same gesture semantics the desktop build was tested with, instead of whatever the detectors happen to do when no clip name is available. That closes the largest gap left by "one pipeline": the desktop already feeds hand data, but the session still uses the clip name to decide what it meant. The real-hand gates of V2 (at most 1 false cast per minute, the 85 % cross-user gate) also measure the arbiter instead of five ad-hoc guards. Every later pose card costs one core and one claim row. The full cross-talk matrix becomes a permanent regression instrument; today only the blink row exists (`BlinkTests.cpp:124-139`).

### Evaluation
Claim: with the clip name hidden, every action clip yields only its own intent, and the staff and mudra grip release no longer produce a sigil reject.
Before: 5 gesture-meaning decisions read `ClipAction()` (`ArenaSession.cpp:720, 773, 780, 837`, `SessionFlow.cpp:503`); 4 `OnHandFrame` subscriptions and 4 `FSystemGestureGate` copies in `Gestures/`; 2 pose detectors outside the gate in `Session/` (`ArenaSession.cpp:577`, `SessionFlow.cpp:506`); `staff-plant`, `staff-lift` and `mudra` clips end pinched (0.9, 0.9, 0.85), so 0 tests play a grip release; the cross-detector test coverage is 1 row (blink) of an 11-action x 5-detector matrix.
After: predicted all matrix cells green; 0 `ClipAction()` calls in the five handler bodies; 1 subscription and 1 gate.
Method: simulation, three cases walked by reading the code. (A = today, B = arbiter.) Case 1, staff plant then release on live hands. A: the pinch at 0.9 opens a builder stroke (`SigilStrokeBuilder.cpp` AddPinch, PinchDown 0.7). On release `Decide` finds no loop and returns Rejected (`SigilStrokeBuilder.cpp` Decide). Then `SigilRecognizerSubsystem.cpp:178` broadcasts and `ArenaSession.cpp:758` pushes `sigil-reject`. B: the grip claim cancels the stroke and nothing is emitted. Case 2, a fast horizontal bar while pointing. A: the blink core sees a pointing hand moving sideways. Whether it fires depends on the speed and omega gates (1.2 m/s, 1.4 rad/s), and only the clip guard at `:837` stopped a bolt, while `HandleBlink` has no guard at all. B: the pen claim holds the flick until pen-up plus the hold. Case 3, both palms in an intermission. A: the left palm also raises the ward, and `HandleWardRaised` returns early only when the clip name is `both-palms` (`:780`); live, it sets `bOfferContinue`. B: the both-palms claim removes the single raise. What would falsify it: the matrix shows that with the name hidden, today's tree already yields only the expected intents (the release case included). Then the guards are dead code, and the card shrinks to the de-duplication (M).
Result: unmeasurable until the instrument exists. The instrument is `GestureCrossTalk.Matrix` with the clip name hidden, which is step 1 of this card.
Gate: architecture. It changes how every gesture reaches the session, and the owner should agree to the claim rules (in particular the pen-claim hold time, which trades bolt responsiveness against false blinks).

**Depends on:**
- T16 releases Session/ArenaSession.cpp (it holds the file)
- lands before T24 (mudra detector) and T25 (fist pose), so those two cards plug into the arbiter instead of adding a fifth and sixth OnHandFrame subscriber
- ARCHITECTURE-REVIEW F3 (one optional live IHandSource in UHandInputSubsystem) is the V1 seam it feeds from; it does not need F3 first

**Write set:**
- `apps/vr/Game/Source/MageArenaVR/Gestures/GestureArbiter.h`
- `apps/vr/Game/Source/MageArenaVR/Gestures/GestureArbiter.cpp`
- `apps/vr/Game/Source/MageArenaVR/Gestures/GestureIntent.h`
- `apps/vr/Game/Source/MageArenaVR/Gestures/SigilRecognizerSubsystem.h`
- `apps/vr/Game/Source/MageArenaVR/Gestures/SigilRecognizerSubsystem.cpp`
- `apps/vr/Game/Source/MageArenaVR/Gestures/BlinkDetector.h`
- `apps/vr/Game/Source/MageArenaVR/Gestures/BlinkDetector.cpp`
- `apps/vr/Game/Source/MageArenaVR/Gestures/WardDetector.h`
- `apps/vr/Game/Source/MageArenaVR/Gestures/WardDetector.cpp`
- `apps/vr/Game/Source/MageArenaVR/Gestures/StaffDetector.h`
- `apps/vr/Game/Source/MageArenaVR/Gestures/StaffDetector.cpp`
- `apps/vr/Game/Source/MageArenaVR/Session/ArenaSession.h`
- `apps/vr/Game/Source/MageArenaVR/Session/ArenaSession.cpp`
- `apps/vr/Game/Source/MageArenaVR/Session/SessionFlow.cpp`
- `apps/vr/Game/Source/MageArenaVR/Tests/GestureCrossTalkTests.cpp`

**Acceptance (write as failing tests first):**
1. GestureCrossTalk.Matrix: every action clip (blink-left/right/back, bolt, both-palms, mudra, sigil-line1/2/3, staff-plant, staff-lift, ward-raise) x {normal, slow, sloppy} played with the action name hidden (UHandInputSubsystem::GetActionName returns empty, as on a headset) yields exactly its expected intent and zero intents of any other kind
2. GestureCrossTalk.StaffRelease: staff-plant.normal followed by an in-memory grip release (both pinches 0.9 -> 0.0 over 5 frames, built like HeldSampleTests, no tracked clip rewritten) yields 1 StaffPlant intent, 0 SigilRejected intents and 0 "sigil-reject" session cues
3. GestureCrossTalk.MudraRelease: the same release appended to mudra.normal yields 0 SigilRejected intents
4. GestureCrossTalk.BothPalms: both-palms.normal with the name hidden yields a BothPalms intent and no single WardRaised intent reaches FArenaSession in the intermission and offer phases
5. Source check: FArenaSession::HandleSigil, HandleBolt, HandleWardRaised, IsSigilDrawing and IsBothPalmsClip contain no ClipAction() call (grep count 0 for those five bodies; scripted-player uses at ArenaSession.cpp:989/1144/1170/1358/1802 may stay)
6. Source check: Gestures/ holds exactly 1 OnHandFrame() subscription and 1 FSystemGestureGate (today 4 and 4); MageArena.Hands.SystemGesture.ZeroEvents and .GateIsTheCause stay green and now also cover both-palms and the stones
7. Full MageArena suite: same pass count plus the new tests, the same 3 known failures (MageArenaDesign.Duel.FireSeated, Session.FullSeated, Session.Wave1Seated); Sigils.Accuracy, Blink, Ward and HeldSample numbers unchanged

**Evidence:**

```
apps/vr/Game/Source/MageArenaVR/Session/ArenaSession.cpp:720-724 `HandleSigil`: `if (Action == TEXT("bolt") || Action.StartsWith(TEXT("blink"))) return;`
apps/vr/Game/Source/MageArenaVR/Session/ArenaSession.cpp:771-773 `IsSigilDrawing`: comment "A pinch that is not a sigil (a staff plant's grip, played as a clip) also puts the hand builder's pen down"; guarded by `ClipAction().StartsWith(TEXT("sigil"))`
apps/vr/Game/Source/MageArenaVR/Session/ArenaSession.cpp:780 `HandleWardRaised`: `const bool bBoth = ClipPlaying() && ClipAction() == TEXT("both-palms");`
apps/vr/Game/Source/MageArenaVR/Session/ArenaSession.cpp:837 `HandleBolt`: `if (ClipAction().StartsWith(TEXT("sigil"))) return;`; `HandleBlink` (:817-833) has no guard
apps/vr/Game/Source/MageArenaVR/Session/ArenaSession.cpp:755-759 `HandleSigilRejected` pushes `sigil-reject` with no guard; consumers ArenaAudio.cpp:533, HandPresentationComponent.cpp:152
apps/vr/Game/Source/MageArenaVR/Session/ArenaSession.cpp:663-666 `ClipAction()` returns `Hands->GetActionName()`, i.e. `ActionPlayer->GetClip().Action` (HandInputSubsystem.cpp:205-211): only a desktop clip has one
apps/vr/Game/Source/MageArenaVR/Gestures/SigilRecognizerSubsystem.cpp:175-179 builder Rejected -> `OnSigilRejected.Broadcast(-1.0)` logged "no circle"
Clip pinch read with node over Game/Clips: staff-plant.normal first/max/last 0.9/0.9/0.9; staff-lift.normal 0.9/0.9/0.9; mudra.normal 0/0.85/0.85 - no release frame, so the collision is never played
grep `OnHandFrame().AddUObject` in Gestures/: BlinkDetector.cpp:277, SigilRecognizerSubsystem.cpp:115, StaffDetector.cpp:145, WardDetector.cpp:369 (4); grep `FSystemGestureGate Gate;`: BlinkDetector.h:174, SigilRecognizerSubsystem.h:52, StaffDetector.h:82, WardDetector.h:147 (4)
Session-resident pose detectors outside the gate: ArenaSession.cpp:577 `StonesTouched` (Hands->GetLatest at :611/:615), SessionFlow.cpp:506 `BothPalmsRaised` (GetLatest at :514); docs/research/STACK-OPPORTUNITIES-2026-10.md F8 status: "the both-palms resume gesture in Session/SessionFlow.cpp, which is not gated"
Cross-detector coverage today: Tests/BlinkTests.cpp:124-139 (sigil, mudra, ward-raise and bolt clips through the blink detector only); no staff x sigil or both-palms x ward test (grep -i "staff.*sigil|sigil.*staff" Tests/*.cpp: 0 behavioural hits)
Planned growth: docs/design/V2-ROADMAP.md lane B rows 9 (T24 mudra seal detector) and 10 (T25 closed-fist held pose) - two more pose detectors with no arbitration seam
```

## M1b - Run the session oracle-blind: arbitrate gestures from hand data, not clip names

Context `hand-input-pipeline` · spine: device-parity input mode · XL · impact 8 · effort 6 · risk 5 · gate architecture

### Summary
The session decides what a gesture meant by asking the desktop key path which clip is playing. Ten gameplay sites read the clip name or the ward key: a sigil during a bolt clip is dropped, a bolt during a sigil clip is dropped, blink flicks are flushed when the clip ends, the both-palms ritual is recognised by its file name. On a headset none of these answers exist, so every one of those filters silently turns off at V1. Build a device-parity mode that hides the answers, and a small gesture arbiter that resolves the same conflicts from hand data, so the desktop build tests what the headset will do.

### Description
The project rule is "never a shortcut that skips the pipeline and claims it tests a gesture". These are the mirror image: the pipeline runs, but its output is overruled by an answer key the headset will not have. They arrived honestly (T08 integration, T11 split hands, T21 ritual) because the detectors are independent and nothing arbitrates between them - a bolt flick is also a short stroke, a sigil stroke has flick-speed segments, a staff grip is a pinch. The corpus never measures this: the 180 impostors are bare, dot, zigzag, hook, arc and swipe drawings, not the game's own other gestures.

The move has three parts. (1) An instrument: a blind switch on UHandInputSubsystem that leaves the clip lanes playing but exposes no clip identity or key state to gameplay, only to the scripted player and capture drivers through an explicit script view (the scripted player is a hand driving keys, so it may know). (2) A GestureArbiter in Gestures/ that receives every detector's candidate intent with its hand, epoch and stamp and applies declared rules - one intent per casting-hand epoch, sigil beats bolt only once the circle is closed, a flick that started during pen-down belongs to the stroke, two palms raised is the ritual and not a ward - all from frames. (3) The session handlers subscribe to the arbiter instead of the four detectors, and the ten oracle reads go. The cross-gesture matrix and the blind reference runs are the evidence; whatever the arbiter cannot yet resolve stays visible in a ratchet rather than hidden behind a file name.

Registry: the planned N6 tracked-input-record-and-replay note ("synthetic clips as pre-device proxies and what they missed") and behavioural-discriminators-over-symbolic-pass (runtime-observation-evidence): today's green suite is partly a symbolic pass, because the label decides the outcome.

### Flow
1. Add the blind switch and a script view; move DecideScript/TryScriptBlink/PlayBlinkToward and the capture drivers onto the view. Source test counts gameplay oracle reads (red at 10).
2. Write DeviceParityTests: the 36-clip cross-gesture matrix in blind mode, recording what each clip yields today (expected red cells: bolt/blink vs sigil, staff grip vs pen-down, both-palms vs ward).
3. Add game-gesture impostor kinds to the generated corpus; run NoFalseCasts.
4. Build GestureArbiter with one rule per red cell; route the session handlers through it; delete the ten reads and the clip-end flush.
5. Run Wave1Seated and FullSeated blind vs oracle; ratchet any diff; hand the owner the table.

### Expected impact
- Converts an invisible V1 regression (every filter switching off on device, during the 1-18 Nov window) into a red cell on the desktop before 10-31.
- Gives the cold desktop testers of the 10-31 gate the same arbitration the headset will have, so their false-cast and missed-gesture reports transfer.
- Makes the F3 live source a drop-in: the session no longer cares whether a clip, the mouse or a headset produced the frames.
- One place for the gesture-conflict rules that T24 (mudra) and T25 (fist pose) will add to, instead of more clip-name checks.

### Evaluation
Claim: removing clip-name and key-state reads from gameplay and arbitrating from frames makes desktop gesture outcomes predictive of device outcomes; today they are not.
Before: 10 gameplay oracle sites (list in evidence) plus 18 legitimate reads in the scripted player; 0 of 6 impostor kinds are game gestures; no test plays the game's gestures against each other through the session.
After: predicted - 0 gameplay oracle reads; a cross-gesture matrix with every cell green or ratcheted; blind and oracle reference logs equal or diff-listed.
Method: simulation only. Case A, Q (bolt) while a sigil stroke is mid-air on the same hand: today HandleBolt is dropped because ClipAction starts with "sigil" (ArenaSession.cpp:837) - blind, both detectors may fire and the kernel gets a bolt and a cast; the arbiter assigns the flick to the open stroke. Case B, A (blink-left): today any sigil the stroke builder completes is dropped by name (:720-723) and the pending flick is flushed when the clip ends (:2002-2005) - blind, the flush must come from the detector's quiet rule, which Blink.Stream already proves on a rewind-free stream. Case C, R (both-palms) in the ritual: today HandleWardRaised returns because the clip is named both-palms (:780) - blind, the left palm raising is a ward onset unless the arbiter sees the right palm rising inside the same window. Falsified if the blind matrix is all green with no arbiter (then the filters were dead weight and the card shrinks to deleting them).
Result: unmeasurable until step 2 runs; the instrument is DeviceParityTests (the blind matrix) and the blind-vs-oracle event-log diff of the reference runs.
Gate: architecture (gameplay input routing moves to an arbiter; no combat number changes; the census may move if the scripted player's gestures were being rescued by a filter, which the diff will show).

**Depends on:**
- ARCHITECTURE-REVIEW-2026-10 N3 (FScriptedPlayer split; the script may keep reading clip state, gameplay may not) - helpful, not required
- ARCHITECTURE-REVIEW-2026-10 F3 (live source slot) - this card is what makes that slot safe to fill
- T24 mudra detector (the arbiter must include it when it lands)
- T16 committed (holds Session/ArenaSession.cpp)

**Write set:**
- `apps/vr/Game/Source/MageArenaVR/Hands/HandInputSubsystem.h`
- `apps/vr/Game/Source/MageArenaVR/Hands/HandInputSubsystem.cpp`
- `apps/vr/Game/Source/MageArenaVR/Gestures/GestureArbiter.h`
- `apps/vr/Game/Source/MageArenaVR/Gestures/GestureArbiter.cpp`
- `apps/vr/Game/Source/MageArenaVR/Session/ArenaSession.h`
- `apps/vr/Game/Source/MageArenaVR/Session/ArenaSession.cpp`
- `apps/vr/Game/Source/MageArenaVR/Session/SessionFlow.cpp`
- `apps/vr/Game/Source/MageArenaVR/Tests/DeviceParityTests.cpp`
- `apps/vr/tools/clipgen/corpus.mjs (game-gesture impostor kinds; generated test clips only, no tracked clip rewritten)`

**Acceptance (write as failing tests first):**
1. Blind mode (-MageArenaDeviceParity, and a test switch): gameplay code cannot read the playing clip name or the ward key state; a source test fails if any Session/*.cpp outside the scripted player and capture drivers calls ClipAction(), ClipPlaying(), IsWardHeld(), IsActionPlaying() or GetActionName() (10 sites today, 0 after).
2. Cross-gesture matrix: each of the 12 action clips x 3 variants (and the mirror set in left-hand mode) played through the full pipeline in blind mode yields exactly its own intent: no sigil from bolt or blink, no bolt from a sigil stroke, no blink from a bolt, no single-palm ward from both-palms, no sigil pen-down from a staff grip. Misses go in a two-sided KnownCrossMisses ratchet like HeldSample's.
3. Reference run parity: Wave1Seated and FullSeated in blind mode produce the same kernel event log and StateHash as oracle mode, or the diff is listed in a ratchet the owner reads.
4. Corpus: impostor kinds gain bolt-flick, blink-flick, staff-grip and ward-raise (fingertip of the casting hand); Sigils.NoFalseCasts stays at 0 false casts.
5. Blink commit without clip end: the pending flick reaches the kernel from the detector's own quiet rule on a stream with no clip boundaries (the Advance clip-end FlushPending is removed) and Blink.Stream stays green.
6. Ritual: a both-palms pose (two palms raised, read from frames) gates HandleWardRaised in blind mode; ritual and resume tests stay green.

**Evidence:**

```
Gameplay oracle reads (awk over Session/ArenaSession.cpp and Session/SessionFlow.cpp by enclosing function): IsSplitPose ArenaSession.cpp:681 (IsWardHeld && GetActionName starts "sigil"); HandleSigil :720-723 (drop sigil if clip is bolt or blink*); IsSigilDrawing :773; HandleWardRaised :780 (both-palms by clip name); HandleBolt :837 (drop bolt if clip is sigil*); CanBlink :989; StepKernel :1802 (aim slot from clip name); Advance :2002-2005 (Blinks->FlushPending when the clip stops); ObserveTeach SessionFlow.cpp:1163 (!ClipPlaying); teach step SessionFlow.cpp:1474 (drop bAbsorb unless ClipPlaying or IsWardHeld). Script-only reads (legitimate): DecideScript 10, TryScriptBlink 1, PlayBlinkToward 4.
Count: grep of ClipAction()|ClipPlaying()|IsWardHeld()|IsActionPlaying()|GetActionName() outside Tests/: ArenaSession.cpp 30, SessionFlow.cpp 3, PresetCapture.cpp 1, BudgetStressDriver.cpp 1, HandInputSubsystem.cpp 2.
Origin: git log -S on the HandleSigil and HandleBolt filters -> 04be2c1 (T08 Wave 1 integration), i.e. added when the detectors were first wired together.
Corpus gap: apps/vr/tools/clipgen/corpus.mjs:48 IMPOSTOR_KINDS = [bare, dot, zigzag, hook, arc, swipe] - none of the game's own gestures.
Live path has no clip: ARCHITECTURE-REVIEW-2026-10 F3 (live IHandSource forwarded by the subsystem; clip lanes stay the desktop path), so GetActionName() is empty and IsWardHeld() false on device (HandInputSubsystem.cpp:200-212, h:36).
Rule: CLAUDE.md "Never add a shortcut that skips the pipeline and claim it tests a gesture"; DESKTOP-INPUT.md "The rule" - the recognizer, ward and blink detectors run on the stream "exactly as they will in VR".
```

## M2 - Own one seated frame: map XR tracking space into seated-origin space once

Context `arena-greybox` · spine: seated frame authority · XL · impact 9 · effort 6 · risk 5 · gate architecture

### Summary
Every gesture in the game is authored and recognised in one frame, seated-origin space (hip midpoint at seat height, +X = the pad's forward), and the layout that defines that frame lives in this context. Nothing in the tree maps a device's tracking space into it: the only IHandSource is the clip player, the pawn hard-codes the seated origin, the ward, sigil and hand-drawing code assume +X is forward, and no test moves a clip into another frame. On a Quest the joints arrive in tracking space with the floor at z = 0 and forward wherever the player recentred. Build one seated-frame authority: derived from arena-layout.json on the desktop and from the head pose at recentre on the device, applied once at the hand-source boundary and to the pawn, so nothing downstream ever sees a device frame.

### Description
1. FSeatFrame (Hands/SeatFrame): origin, flat forward and up of the seated origin in world and in tracking space; built from FArenaLayout (pad, seat, eye offsets) and, on device, from the HMD pose at recentre (head minus eyeForwardOfSeatedOriginM along flat forward minus eyeAboveSeatedOriginM up).
2. IHandSource declares the frame of the joints it emits (seated for clips, tracking for a device source); UHandInputSubsystem applies tracking-to-seated once, so WardDetector, SigilStrokeBuilder and the blink detector keep their seated-space contracts untouched.
3. The pawn's SeatedOrigin and camera come from FSeatFrame; the desktop is the identity case and proves the seam with every existing test. HandPresentationComponent stops assuming +X (HandPresentationComponent.cpp:317-321) and reads the frame's forward.
4. A frame-invariance test generator re-expresses committed clips in synthetic tracking frames: the desktop way to rehearse the device without a device, through the same pipeline, never a shortcut around it.
5. docs/gameplay/01-body-and-space.md gets the fourth frame row (device tracking space), and CLIP-SCHEMA's recorder section points at the authority instead of telling the recorder to convert on its own.
Registry: engine-integration/runtime-observation-evidence deterministic-headless-timestep (the sweep runs on the fixed 72 Hz clip clock) and behavioural-discriminators-over-symbolic-pass (onset and verdict equality, not 'transform applied'); systems-canon/realtime-combat-semantics real-time-timers-with-an-escapable-window (the 0.15 s perfect window is anchored on an onset the frame must not shift).

### Flow
arena-layout.json (pad, seat, eye offsets) -> FSeatFrame (desktop: identity to world) | HMD pose at recentre -> FSeatFrame (device) -> UHandInputSubsystem: IHandSource frame (tracking) -> seated joints -> WardDetector / sigil / blink (unchanged) -> kernel InputFrame. Pawn camera and SeatedOrigin <- FSeatFrame; HandPresentation and the settings stones attach to SeatedOrigin as today.

### Expected impact
V1 (Wed 4 Nov) starts with a pipeline that already works in a device-shaped frame, instead of finding on day one that wards miss when the player recentred 40 degrees off, or that the raise-height gate means nothing under a floor origin. P3's recorder writes seated-space clips through one tested function, so the V2 corpus is comparable with the October clips. It also removes duplicate copies of the seat geometry.

### Evaluation
Claim: without a seated-frame authority the gesture pipeline is correct only when the device frame happens to equal the clip frame.
Before: implementers of IHandSource = 1 (Hands/HandClipPlayer.h:16, clips only); tests that rigidly transform a clip frame = 0 (git grep 'FTransform|TransformPosition|RotateVector' over Tests/*.cpp -> no hit, 203 automation tests); XRSystem use = 1 site, the worn-state poll (Session/ArenaSession.cpp:2178-2186); seated-origin literal (-8, 0, 58) at Hands/MageArenaPawn.cpp:47, recomputed by ConfigureFromLayout at :93-115; forward assumed +X at Greybox/HandPresentationComponent.cpp:317-321, Gestures/SigilStrokeBuilder.cpp:13, Greybox/GreyboxCapture.cpp:723-724; docs/gameplay/01-body-and-space.md:59-63 lists 3 frames and no device frame.
After: Invariance green over the 9-step yaw x 3-height x origin-shift sweep with identical verdicts; the bypass run red at |yaw| >= 40; Identity shows an empty per-test diff.
Method: simulation. Case 1: the player recentres 45 deg left of the pad's forward and raises the ward straight ahead of their body: A, the palm normal sits 45 deg off ReferenceFacing (+X), over RaiseAngleDeg 35 (docs/gameplay/02-verbs-and-gestures.md:178), no ward, the perfect is lost; B, the frame removes the yaw and the raise matches the clip. Case 2: local-floor origin, hand resting on the lap: A, the palm is about 0.6 m above a 'seated origin' at the floor, so the >= 0.20 m raise-height gate always passes; B, the origin is the hip and the gate measures as authored. Case 3: desktop: A and B identical, all tests unchanged. Falsifier: if the runtime already reports hand joints in a frame anchored to the recentred forward with a hip-height origin, cases 1 and 2 do not occur and the card shrinks to its layout and test parts.
Result: unmeasurable without the device; the instruments that decide it are MageArena.SeatFrame.Invariance on desktop now and the V1 day-one recentre check on the Quest 3.
Gate: architecture (a new seam between hand sources and the gesture pipeline; the clip contract and all detectors stay as they are).

**Depends on:**
- T05 route R-A (done)
- the V1 device bring-up (PROJECT-PLAN, Wed 4 Nov) and the P3 device recorder consume it
- lands before any XR IHandSource implementation

**Write set:**
- `apps/vr/Game/Source/MageArenaVR/Hands/SeatFrame.h`
- `apps/vr/Game/Source/MageArenaVR/Hands/SeatFrame.cpp`
- `apps/vr/Game/Source/MageArenaVR/Greybox/ArenaLayout.h`
- `apps/vr/Game/Source/MageArenaVR/Greybox/ArenaLayout.cpp`
- `apps/vr/Game/Source/MageArenaVR/Hands/MageArenaPawn.h`
- `apps/vr/Game/Source/MageArenaVR/Hands/MageArenaPawn.cpp`
- `apps/vr/Game/Source/MageArenaVR/Hands/IHandSource.h`
- `apps/vr/Game/Source/MageArenaVR/Hands/HandInputSubsystem.h`
- `apps/vr/Game/Source/MageArenaVR/Hands/HandInputSubsystem.cpp`
- `apps/vr/Game/Source/MageArenaVR/Greybox/HandPresentationComponent.cpp`
- `apps/vr/Game/Source/MageArenaVR/Tests/SeatFrameTests.cpp`
- `docs/gameplay/01-body-and-space.md`
- `docs/CLIP-SCHEMA.md`

**Acceptance (write as failing tests first):**
1. MageArena.SeatFrame.Layout: the pawn's SeatedOrigin relative location equals (-eyeForwardOfSeatedOriginM, 0, seatedEyeHeightAbovePadM - eyeAboveSeatedOriginM) from arena-layout.json through FSeatFrame, and no other code computes it (the literal (-8, 0, 58) at MageArenaPawn.cpp:47 is gone).
2. MageArena.SeatFrame.Identity: on desktop the tracking-to-seated transform is identity; the full MageArena suite has an empty per-test result diff against the tree before the change.
3. MageArena.SeatFrame.Invariance: every committed ward, blink and sigil clip (normal and sloppy) re-expressed in a synthetic tracking frame (yaw -60..+60 deg in 15 deg steps, origin shift up to +/-0.5 m, head 1.05-1.35 m above a local-floor origin) and fed through UHandInputSubsystem with the matching calibration gives the same gesture verdicts, and onsets within one sample, as the raw clip.
4. The same sweep with the authority bypassed fails, so the test can see the defect: ward-raise misses at |yaw| >= 40 deg against RaiseAngleDeg 35, and the raise-height gate passes for a hand resting at lap height under a floor origin.
5. MageArena.SeatFrame.Recentre: a recentre mid-session re-derives the frame from the head pose (seated origin = head - eyeForward along flat forward - eyeAbove up); the first gesture after it resolves as authored, and a gesture that straddles the recentre does not cast.
6. Hands, ward arc and settings stones draw at the same world positions on desktop: the 01-initial-view and 03-ward-arc shots are pixel-identical before and after.

**Evidence:**

```
Seated space contract: docs/CLIP-SCHEMA.md:34 ('seated-origin, metres, +X forward'), :104 (origin = hip midpoint at seat height), :152 (the recorder converts on its own: 'Do not write a stage-space or OpenXR-space clip'). | Layout owns the seat geometry: apps/vr/Game/Source/MageArenaVR/Greybox/ArenaLayout.h:88-92 (SeatSizeM, SeatedEyeHeightAbovePadM 1.2, EyeForwardOfSeatedOriginM 0.08, EyeAboveSeatedOriginM 0.62). | Hard-coded seated origin: apps/vr/Game/Source/MageArenaVR/Hands/MageArenaPawn.cpp:43-47 SetRelativeLocation(FVector(-8.0, 0.0, 58.0)), recomputed at :93-115; the camera sits at a fixed EyeCm, no XR camera path. | Only hand source: grep -rn 'public IHandSource' -> apps/vr/Game/Source/MageArenaVR/Hands/HandClipPlayer.h:16 only. | Forward = +X assumptions: apps/vr/Game/Source/MageArenaVR/Greybox/HandPresentationComponent.cpp:317-321 ('Clip space +X is the seated forward'), apps/vr/Game/Source/MageArenaVR/Gestures/SigilStrokeBuilder.cpp:13, apps/vr/Game/Source/MageArenaVR/Gestures/WardDetector.cpp:14 and :107 (ForwardVector fallback), apps/vr/Game/Source/MageArenaVR/Greybox/GreyboxCapture.cpp:723-724 ('Centre-pad forward is +X, which is the clip forward'). | No frame-moving test: git grep -n 'FTransform|TransformPosition|RotateVector' -- Tests/*.cpp -> 0 hits. | XR use today: apps/vr/Game/Source/MageArenaVR/Session/ArenaSession.cpp:2178-2186 worn state only. docs/PROJECT-PLAN.md:345 promises a 'Seated XR pawn'; :48 the V1 gate (Wed 4 Nov) needs hands on device; docs/gameplay/01-body-and-space.md:59-63 frames table has no device frame. | Ward thresholds that depend on the frame: docs/gameplay/02-verbs-and-gestures.md:178 (palm normal within 35 deg of the reference facing, palm >= 0.20 m above the seated origin).
```

## M3 - Put all hand sources on one fixed 72 Hz input clock and feed the kernel by onset

Context `hand-input-pipeline` · spine: deterministic input clock · XL · impact 8 · effort 7 · risk 6 · gate architecture

### Summary
Make UHandInputSubsystem the single owner of input time: every source (ward lane, action lane, mouse, and the V1 live source) emits on one monotonic 72 Hz clock with explicit stream epochs, detectors stop inferring gesture boundaries from clock rewinds, and gesture intents carry their onset stamp into the kernel through a declared input delay, so the perfect ward is judged where the hand moved, not where the classifier caught up.

### Description
Today the "72 Hz joint stream" exists only in the clip files. At runtime, UHandClipPlayer::Step emits exactly one sample per call with whatever dt the render loop hands it, so the detectors see 144 samples/s on a fast desktop, 72 on Quest and one sample across a whole flick on a 0.25 s hitch. Each player keeps its own clock that restarts at 0 on Play, the mouse capture keeps a third, and the frames of both lanes interleave on one delegate. Four detectors have learned to treat a backwards stamp as "a new gesture starts" - a signal that only the desktop clip player produces; a headset stream never rewinds (the blink latch bug of 2026-10-05 was exactly this). Finally the ward detector computes a motion-onset time (the D-G3 design) and the session throws it away: HandleWardRaised logs it and sets a boolean that the kernel reads on its next tick, after a measured 0.167 s key-to-detection latency against a 0.15 s perfect window, which the reference script papers over with EmberRaiseLeadS.

The move: (1) an input clock in the subsystem (fixed 1/72 s accumulator, bounded catch-up), which samples every lane by its own clip time but stamps frames with input-clock time; (2) an epoch id on FHandFrame (kept last, like bSystemGesture) raised on Play, source switch and tracking resume; detectors reset on the epoch and their rewind code goes; (3) the mouse fingertip enters the bus as source=mouse frames, as DESKTOP-INPUT.md already draws it; (4) the session steps hands, detectors and kernel in one declared phase order in live play and tests alike; (5) the kernel runs a declared D ticks behind the input clock and intents enter at the tick that contains their onset - delay-based input as fighting games use it - with D in the VR overlay and D = 0 reproducing today byte for byte. No combat number changes; the kernel is untouched. If D large enough to cover detection latency costs too much feedback latency, the follow-up is a change request for an onset field in the kernel InputFrame (gate: contract), which this card makes measurable but does not file.

Registry techniques: deterministic-headless-timestep (pin the rate, decouple from wall clock) and update-order-and-frame-coherence (one writer per quantity; time here) in engine-integration; the planned N2 hand-tracked-timing-windows note (onset timestamps, tracking loss as a state) is the knowledge this produces.

### Flow
1. Input clock and fixed-rate emission in UHandInputSubsystem + UHandClipPlayer (players become samplers; the subsystem ticks them). Tests: render-rate and hitch cases.
2. Epoch field and monotonic stamping; port the four detectors from rewind to epoch; delete the rewind branches. Tests: monotonic clock, all gesture suites unchanged.
3. Mouse capture publishes fingertip frames on the bus instead of calling IngestMouse.
4. Session: one phase order for live and test stepping; intents carry onset stamps; kernel input queue keyed by input-clock tick with delay D (default 0).
5. Measure D: run the D-G3 injected-latency cases and the census at D = 0, 2, 4 ticks and hand the owner the perfect-rate and feedback-latency table.
6. Update CLIP-SCHEMA.md (emission rule) and DESKTOP-INPUT.md (clock and delay).

### Expected impact
- The desktop core gate (10-31) stops depending on the tester's frame rate: the same key on any machine feeds the detectors the same samples.
- V1: the live source plugs into a clock and an epoch contract that already exist, so detectors run unchanged on a stream that never rewinds - the class of bug that latched the blink detector cannot recur.
- D-G3 becomes provable in the integrated session (today only FAbsorbResolver unit tests use the onset), and the scripted EmberRaiseLeadS compensation can be retired at the census rerun.
- Tests and live play share one phase order, so a passing headless run means the same thing as a desktop session.

### Evaluation
Claim: one fixed-rate monotonic input clock with epochs and onset-keyed kernel input makes gesture outcomes independent of render rate, hitches and clip boundaries, and lets the perfect ward be judged at motion onset.
Before: one emitted sample per Step call with render dt (HandClipPlayer.cpp:121-183, Tick :329-335; 0 occurrences of 72 in HandClipPlayer.cpp); 3 independent clocks (HandClipPlayer.cpp:22 twice via two players, MouseSigilCapture.cpp:51); 4 detectors with rewind inference; OnsetTime used only in a log line (ArenaSession.cpp:776-807); measured 0.167 s gesture latency vs a 0.15 s window (ArenaSession.cpp:1328-1331); live hands tick in their own FTickableGameObject while tests step them inside Advance (ArenaSession.cpp:1996-1999 vs :2284).
After: predicted - frame counts equal across dt; zero rewind branches; kernel raise on the onset tick +/-1 for latency <= 60 ms at D = 4; D = 0 byte-identical event logs.
Method: simulation only. Case A, bolt.normal at 144 fps: today about 2x the samples of 72 fps reach the blink detector, HeldPointRate measures every one; after, the identical 72 Hz set. Case B, Space held then 1 pressed: today the casting-hand lane restarts at t=0 while the ward lane keeps counting, so frames of two clocks interleave; after, both stamped on one clock and the sigil builder resets on the epoch, not the rewind. Case C, ember at 0.15 s window with 0.167 s detection latency: today the raise lands after the window closes unless the script leads by 0.25 s; after with D = 4 ticks (0.067 s) a raise whose onset is inside the window lands inside it when detection lag <= 0.067 s - not yet the full 0.13-0.15 s onset-to-confirm of ward-raise, which is why D is measured, not assumed. Falsified if the census perfect rate at D = 4 is unchanged from D = 0, or if any gesture suite changes result when only the clock changes.
Result: unmeasurable until built; the instrument is the new MageArena.Input.Clock.* tests plus the D sweep through the existing census (MageArenaDesign.*) and the D-G3 injected-latency cases.
Gate: architecture (session stepping order and an input-delay knob; no kernel or combat-number change; D is a VR overlay field the owner signs off).

**Depends on:**
- ARCHITECTURE-REVIEW-2026-10 F3 (live IHandSource slot in UHandInputSubsystem; this card defines the clock that slot must stamp)
- T16 committed (holds Session/ArenaSession.cpp)

**Write set:**
- `apps/vr/Game/Source/MageArenaVR/Hands/HandFrame.h`
- `apps/vr/Game/Source/MageArenaVR/Hands/IHandSource.h`
- `apps/vr/Game/Source/MageArenaVR/Hands/HandClipPlayer.h`
- `apps/vr/Game/Source/MageArenaVR/Hands/HandClipPlayer.cpp`
- `apps/vr/Game/Source/MageArenaVR/Hands/HandInputSubsystem.h`
- `apps/vr/Game/Source/MageArenaVR/Hands/HandInputSubsystem.cpp`
- `apps/vr/Game/Source/MageArenaVR/Gestures/WardDetector.cpp`
- `apps/vr/Game/Source/MageArenaVR/Gestures/BlinkDetector.cpp`
- `apps/vr/Game/Source/MageArenaVR/Gestures/SigilRecognizerSubsystem.cpp`
- `apps/vr/Game/Source/MageArenaVR/Gestures/StaffDetector.cpp`
- `apps/vr/Game/Source/MageArenaVR/Gestures/MouseSigilCapture.h`
- `apps/vr/Game/Source/MageArenaVR/Gestures/MouseSigilCapture.cpp`
- `apps/vr/Game/Source/MageArenaVR/Session/ArenaSession.cpp`
- `apps/vr/Game/Source/MageArenaVR/Session/SessionFlow.cpp`
- `apps/vr/Game/Source/MageArenaVR/Tests/InputClockTests.cpp`
- `apps/vr/data/vr/combat.vr.json (one input-delay field, VR overlay, not a combat number)`
- `docs/CLIP-SCHEMA.md`
- `docs/DESKTOP-INPUT.md`

**Acceptance (write as failing tests first):**
1. Render-rate independence: bolt.normal driven through Tick with dt = 1/144 s and again with dt = 1/30 s broadcasts the same number of frames per hand (round(duration*72)+1) on the same i/72 timestamp grid, byte-equal joint data.
2. Hitch: a single 0.25 s Tick in the middle of blink-left.normal still broadcasts every 1/72 s sample in order (bounded catch-up), and the blink detector fires exactly once with the same yaw as the no-hitch run.
3. Monotonic clock: ward hold (Space) + mudra on the action lane + a second Play of any clip + a mouse stroke: every frame on UHandInputSubsystem::OnHandFrame has TimeSeconds strictly greater than the previous frame of the same hand. Today the stamp falls back to 0 at each Play.
4. Explicit epochs: each new clip or source change raises an epoch id on the frame; the ward, blink, sigil and staff detectors reset on the epoch, and their stamp-rewind branches are deleted (grep -c "Rewind\|rewinds the stamp" in Gestures/*.cpp goes to 0) with Ward.*, Blink.*, Sigils.Accuracy, Sigils.NoFalseCasts and HeldSample.* unchanged.
5. One phase order: live play and Advance(dt, true) step hands -> detectors -> kernel in the same declared order; a test that ticks the world with the session subsystem and the clip player registered in either order yields an identical kernel event log.
6. Onset into the kernel: with the input delay set to D = 4 kernel ticks, a ward raise with injected detection latency 0, 30 and 60 ms produces the kernel absorb raise on the tick containing the motion onset (+/-1 tick), the D-G3 criterion; with D = 0 the log is byte-identical to today.
7. Mouse on the bus: a held left-button stroke enters as source=mouse right-index frames on the same clock (no direct IngestMouse call); the 90 mouse clips keep their accuracy to the clip.

**Evidence:**

```
apps/vr/Game/Source/MageArenaVR/Hands/HandClipPlayer.cpp:160-182 - Step advances SampleTime by DeltaSeconds and calls EmitSample once; no 1/Hz accumulator; :329-335 Tick passes render DeltaTime straight to Step. grep -c 72 HandClipPlayer.cpp = 0.
apps/vr/Game/Source/MageArenaVR/Hands/HandClipPlayer.cpp:18-35 - Play sets TimeSeconds = 0 and Sequence = 0; UHandInputSubsystem owns two such players (HandInputSubsystem.h:62-66) and forwards both onto one delegate (HandInputSubsystem.cpp:42-45).
apps/vr/Game/Source/MageArenaVR/Gestures/MouseSigilCapture.cpp:51,60,65 - a third clock (TimeSeconds += DeltaSeconds) and a direct IngestMouse call that never touches OnHandFrame, against DESKTOP-INPUT.md:15-17 which draws the mouse joining the joint stream.
Rewind inference: WardDetector.cpp:223-226 (NoteRewind on a backwards stamp), BlinkDetector.cpp:152-157 ("A new clip rewinds the stamp. Finish the previous flick, then arm."), SigilRecognizerSubsystem.cpp:151-157 ("A new clip restarts at t=0"), StaffDetector.cpp:46-49 (ResetWindow on rewind). BlinkDetector.cpp:200-201 and .claude/scan-history/scan-sweep.jsonl "blink-latch" (31c34f7): "Blink detector latches after the first flick on a stream with no clock rewind".
Onset dropped: Session/ArenaSession.cpp:776-807 HandleWardRaised(OnsetTime, Facing) uses OnsetTime only in Note(...) and sets bAbsorb = true; kernel input is the boolean at ArenaSession.cpp:1874 and SessionFlow.cpp:1214. The onset is exercised only in a unit test (Tests/WardTests.cpp:533-600, MageArena.Ward.OnsetCompensation, detector + FAbsorbResolver, no session).
Session/ArenaSession.cpp:1328-1331 - "Measured T18: SetWardHeld to HandleWardRaised is 0.167 s (10 ticks)... EmberGestureLatencyS = 0.167" against combat.json absorb.perfect.windowS 0.15; docs/PROJECT-PLAN.md:217 D-G3 acceptance "perfect event at the authored tick +/-1 for injected latency <=60 ms after onset compensation".
Two phase orders: live UArenaSessionSubsystem::Tick calls Session.Advance(DeltaTime, false) (ArenaSession.cpp:2284) while hands tick as separate FTickableGameObjects (HandClipPlayer.cpp:262-272); tests call Advance(dt, true), which steps hands first (ArenaSession.cpp:1996-1999).
docs/CLIP-SCHEMA.md "hz: 72 is the rate a clip is emitted at" - a rule the runtime does not enforce; docs/PROJECT-PLAN.md:344 MageArenaInput rule "Hand frames carry display time and are quantized to ticks" - not built (no tick quantization anywhere in Hands/).
```

## M4 - Step the whole session on the sim tick and journal its inputs for exact replay

Context `duel-session` · spine: deterministic session replay · XL · impact 9 · effort 8 · risk 6 · gate architecture

### Summary
Move every piece of session logic (the reference script, the teach, the day clocks, stone and palm holds, tracking loss, the resume count) onto the 60 Hz sim tick. Record each session as a tick-stamped journal of what entered the gesture pipeline: hand frames, mouse sigil samples, view aim, and platform notices (focus, headset, quit). A journal then replays a whole day headless and bit-identically, at any render rate, through the same detectors.

### Description
The kernel is fixed-step. The session around it is not. `FArenaSession::Advance` takes the engine's frame delta: it steps the hands by that delta, runs `DecideScript` once per frame, and then runs 0 to 8 kernel ticks. Eleven session clocks (TeachClock, PhaseClock, PickClock, OfferHold, StoryHold, ResumeElapsed, LostTrackingS, ArcClock and others) add up frame deltas. So the census and the design gates the owner decides DF-006 to DF-008 on are measured at one render rate (1/72 s in SessionTests, 1/60 s in DayTests). They are not properties of the game. Quest 3 can run at 72, 90 or 120 Hz. PROJECT-PLAN section 7 already sets the contract ("Hand frames carry display time and are quantized to ticks", "Emits a VR InputFrame per sim tick"), and nothing in Hands/ or Gestures/ does that quantization.

This card builds the game-side seam that P3 (trace recorder) and P5 (scenario driver, "scripted replay of an owner session") assume and do not specify:
1. **A session tick loop.** Advance only accumulates time. Each sim tick runs, in order: pull this tick's inputs (from a live or clip source, or from a journal), step the detectors, decide (script, teach, day graph), step the kernel, drain events. Clips step per tick on desktop; live frames are stamped with the tick that consumed them. Presentation interpolates and owns no logic.
2. **A session journal.** `FSessionJournal` is an `IHandSource` decorator (`Hands/IHandSource.h` is already the one seam) plus a side channel for mouse ingests, view aim and platform notices. It writes JSONL with a header (build, seed, preset, pin commit, settings, save-dir snapshot) and one record per tick that had input. Replay swaps the decorator into replay mode. The detectors still run, so replay never skips the gesture pipeline (CLAUDE.md rule). The gesture events seen at recording time are kept as an oracle: a replay must produce them at the same ticks.
3. **Fail closed.** A journal with a header mismatch, a gap, or a tick out of order refuses to replay and names the tick (laws L10, L12).

Techniques: `runtime-observation-evidence/deterministic-headless-timestep` (pin the rate, decouple it from the wall clock, declare it in the run's metadata) and `gameplay-runtime-patterns/update-order-and-frame-coherence` (gather, decide, apply in one declared step order). Registry note N6 `tracked-input-record-and-replay` (plan section 9) gets its first real application.

### Flow
Live or clip frames -> JournalHandSource (stamp tick, append) -> detectors (sigil, ward, blink, staff) -> session decide per tick (script / teach / day) -> kernel StepGames -> events -> chain + journal oracle. Replay: journal -> JournalHandSource(replay) -> the same detectors -> the same decide -> the same kernel. The result is a diff of chain, event log, StateHash, flags and collar files against the recording.

### Expected impact
- The census, FullSeated and every design gate measure the game, not the frame rate. The DF numbers the owner decides on stop moving when the render rate or machine changes.
- A V1/V2 device session (a missed ritual lock, a false cast, a tracking-loss pause) can be replayed on desktop, unchanged, through the same pipeline. Today a device bug can only be described, not reproduced.
- The capture drivers can shoot stills by tick from a replay, so the shots are the same on every run. They stop depending on wall-clock waits (all 9 drivers use FPlatformTime).
- The desktop core gate (31 Oct) gets a "replay the owner's play session to victory" check that is exact, not approximate.

### Evaluation
Claim: With all session logic on the sim tick, the same seed and inputs give an identical chain and final StateHash at 60, 72, 90 and 120 Hz render rates, and a journal recorded from a human or device session replays bit-identically through the gesture pipeline.
Before: 11 frame-delta accumulators in Session/SessionFlow.cpp and Session/DayFlow.cpp. DecideScript runs once per Advance call, not per tick (ArenaSession.cpp:2018-2021). Hands step by the frame delta (ArenaSession.cpp:1996-1999). No test runs one session at two render rates. Design gates run at 1/72, day and collar tests at 1/60. No session input recorder exists. The contract at PROJECT-PLAN.md:344 has 0 implementing hits in Hands/ and Gestures/.
After: simulation, three cases walked under A (today) and B (this card). (a) Reference script, 72 Hz against 90 Hz. A: 72 or 90 decisions per sim second against 60 kernel steps. The 0.08 s melee window (ArenaSession.cpp:1362) is sampled about 5.8 or 7.2 times, and clips start on frame edges 13.9 ms or 11.1 ms apart. EmberRaiseLeadS 0.25 s against a 0.15 s perfect window means a one-frame shift can win or lose a perfect, so bout times and perfect counts can differ between rates. B: 60 decisions per second at both rates, so the chain is identical. (b) The teach perfect step. A: TeachClock takes the whole frame delta before that frame's ticks run (SessionFlow.cpp:1472 against :1482-1487). The ward-raise cue fires at impact minus 0.22 s, rounded to a frame edge, so a 33 ms hitch can turn a perfect into a block. B: the cue fires on a tick count. (c) A device report that the ritual did not lock. A: there is nothing to replay. B: the device journal replays on desktop and the both-palms hold is seen at the same tick. Falsifier: if FullSeated on today's tree gives identical chains and hashes at 1/60, 1/72, 1/90 and 1/120, the frame-rate half of the claim is wrong; the journal half still stands.
Method: simulation. The instrument that decides it is acceptance case 1 (MageArena.Session.FrameRateInvariance), run on the current tree first, where it is expected to fail.
Result: unmeasured. The invariance test is the instrument; it has not been run (scouts do not build).
Gate: architecture. The census numbers measured through the script are re-taken once on the tick loop, and the owner's open DF items should quote the re-run.

**Depends on:**
- T16 (open; holds ArenaSession.cpp/.h; brings the scripted-duel event log and MageArena.Replay.Deterministic, which this card generalises to human/device input and the whole day)
- ARCH-REVIEW N3 FScriptedPlayer (the script moves out first, then decides per tick)
- PROJECT-PLAN P3 trace recorder and P5 scenario driver (pof side; they consume the journal format; not re-proposed)

**Write set:**
- `apps/vr/Game/Source/MageArenaVR/Session/SessionJournal.h`
- `apps/vr/Game/Source/MageArenaVR/Session/SessionJournal.cpp`
- `apps/vr/Game/Source/MageArenaVR/Hands/JournalHandSource.h`
- `apps/vr/Game/Source/MageArenaVR/Hands/JournalHandSource.cpp`
- `apps/vr/Game/Source/MageArenaVR/Hands/HandInputSubsystem.h`
- `apps/vr/Game/Source/MageArenaVR/Hands/HandInputSubsystem.cpp`
- `apps/vr/Game/Source/MageArenaVR/Gestures/SigilRecognizerSubsystem.cpp`
- `apps/vr/Game/Source/MageArenaVR/Session/ArenaSession.h`
- `apps/vr/Game/Source/MageArenaVR/Session/ArenaSession.cpp`
- `apps/vr/Game/Source/MageArenaVR/Session/SessionFlow.cpp`
- `apps/vr/Game/Source/MageArenaVR/Session/DayFlow.cpp`
- `apps/vr/Game/Source/MageArenaVR/Tests/JournalTests.cpp`
- `apps/vr/Game/Source/MageArenaVR/Tests/SessionTests.cpp`

**Acceptance (write as failing tests first):**
1. MageArena.Session.FrameRateInvariance: the FullSeated reference day (seed 1, scripted) advanced at Dt 1/60, 1/72, 1/90 and 1/120 gives an identical chain (wall-time fields excluded) and an identical final StateHash.
2. MageArena.Journal.RoundTrip: a day driven by clip key actions at 1/72, recorded with the journal on, then replayed headless at 1/90, reproduces the kernel event log, the chain, the final StateHash, bout.txt, the arena flags and collar.ledger.json byte for byte.
3. MageArena.Journal.PipelineInLoop: the journal stores hand frames and mouse samples, not gesture events. Replaying with the sigil reject threshold changed diverges at the first affected tick, and replaying unchanged reproduces every recorded gesture event (sigil, ward, blink, staff) at the same tick.
4. MageArena.Journal.PlatformNotices: a recording with a headset-removed pause and a both-palms resume replays the pause, the 3-2-1 count and the resume at the same ticks.
5. MageArena.Journal.FailClosed: a journal whose header (build, pin commit, preset, seed) does not match, or that has a tick gap or reordering, refuses to replay and logs the offending tick. Nothing is stepped.
6. Session clocks are tick counts: grep '+= DeltaSeconds' over Session/SessionFlow.cpp and Session/DayFlow.cpp returns 0, and DecideScript, DecideTeach and AdvanceDay run once per sim tick (a counter test: 60 calls per sim second at any Dt).
7. A capture driven from a journal replay (the T13 teach stills) requests every shot at the same tick on two runs.

**Evidence:**

```
apps/vr/Game/Source/MageArenaVR/Session/ArenaSession.cpp:1985-2033 Advance(DeltaSeconds): Hands->Step(DeltaSeconds) at :1996-1999; DecideScript() once per call at :2018-2021, before the tick loop at :2026-2031 (Steps < 8). | apps/vr/Game/Source/MageArenaVR/Session/ArenaSession.cpp:2273-2290 UArenaSessionSubsystem::Tick passes the engine frame DeltaTime straight into Session.Advance. | grep '+= DeltaSeconds' Session/*.cpp gives 11 frame-clock accumulators: DayFlow.cpp:419 (StoryHold), :538 (PhaseClock), :565 (OfferHold); SessionFlow.cpp:493 (LostTrackingS), :550 (ResumeElapsed), :1346 (ArcClock), :1397 (OfferHold), :1472-1473 (TeachClock, TeachElapsed), :1544 (PickClock), :1570 (PickHold). | apps/vr/Game/Source/MageArenaVR/Session/SessionFlow.cpp:1472-1487: TeachClock += DeltaSeconds runs before that frame's StepTeachTick loop, and ObserveTeach (:1109, :1126, :1163, :1183) compares this frame-advanced clock with tick-derived GlobImpactS/BlinkImpactS (:709-722). | apps/vr/Game/Source/MageArenaVR/Session/ArenaSession.cpp:1330-1336 EmberRaiseLeadS 0.25 and EmberGestureLatencyS 0.167 were measured "on this rig" (T18) through the frame-sampled pipeline; :1362 sets the 0.08 s melee window. | Tests mix render rates: SessionTests.cpp:119,211,261,400,510,673 use 1/72 (design gates Wave1Seated, FireSeated, AirSeated, FullSeated); DayTests.cpp:36 and CollarTests.cpp:37 use 1/60; PresetTests.cpp:598 uses 0.05. No test runs one session at two rates. | docs/PROJECT-PLAN.md:344: 'Emits a VR InputFrame per sim tick ... Hand frames carry display time and are quantized to ticks.' git grep 'quantiz' over apps/vr/Game/Source finds only a test comment (HeldSampleTests.cpp:60). | apps/vr/Game/Source/MageArenaVR/Hands/IHandSource.h:11-24 is the single input seam (GetLatest, GetFrameSequence, OnHandFrame) that a recording or replay decorator wraps. No recorder exists (git grep TraceRecorder: 0 hits). | All 9 capture drivers time their steps with FPlatformTime::Seconds() (grep -c: 3-4 each), e.g. CreaturesCapture.cpp:183-189 and :291 (60 s wall timeouts). | apps/vr/tasks/T16-style-pick-replay-capture.md: Status open; its replay covers one scripted Fire duel by seed and script, not human input or the day.
```

## M5 - Compile pinned enemy prose into typed archetypes at load and gate pin bumps

Context `spell-schools-and-ai` · spine: load-time data contract compilation · XL · impact 7 · effort 4 · risk 3 · gate architecture

### Summary
The enemy AI reads the pinned TV roster's English prose ('back off 1.2 s', 'uses 2 engagement slots', 'drain 6 mana') with a regex every tick, inside the step, and crashes on a mismatch only in builds with checks on. Compile every magnitude the kernel needs into typed fields when the pin loads, resolve each enemy id to an archetype once, and run the same compiler as a contract check before any pin bump.

### Description
`Extract` (Enemies.cpp:15-37) caches regex answers behind a static map and a lock (dc04e03 made it a cache, not a compile per tick) and ends in `checkf(bMatched, ...)`. It has 11 call sites over 9 patterns, read at tick time: net width and root (:72-73), tongue pull (:80), charge lane, range and wall stun (:85-87), conscript back-off (:120, and every tick at :246), hound engagement slots (every tick for every enemy that reaches :333), moth drain (:343). The loader stores those strings unchecked (KernelDataRoster.cpp:50-52, :62) even though it already owns a failing matcher, `KernelDataLoad::MatchDouble` (KernelDataLoad.h), which the spell loader uses.

Three consequences:
- **Contract.** The TV repo owns these cells and rewords them freely. A reworded cell passes the load and the pin bump, then trips `checkf` the first tick that enemy acts. In a build with checks compiled out (Games.cpp:385 notes that `checkf` drops its expression there) the capture is empty and `Atod` gives 0, so a Quest APK would silently play conscripts with no back-off, hounds with 0 engagement slots and moths with no drain.
- **Steering by name.** 19 enemy and attack id compares in Enemies.cpp choose behaviour (`conscript`, `mire_maw`, `thornback`, `netter`, `hush_moth`, `cinder_hound`, `net_cast`, `tongue_pull`, `thorn_charge`), with 6 more in VrRulesLoad.cpp. A new roster creature for the slice's Bout 2 or a TV rename falls through to default steering without a word.
- **Quest cost.** Each tick's lookup builds an `FString` key and takes a lock per conscript and hound (the reason `MageArena.Kernel.EnemyStepCost` exists).

The move (registry: design-canon-as-executable-law, parse-thresholds-from-prose, canon-as-single-source-of-thresholds, shape-check-vs-content-invariant): an `FEnemyArchetype` compiled in `LoadEnemies` with typed magnitudes, patterns written against the skeleton words with `MatchDouble`, failing load with the id and field; an archetype enum per id (the id-to-archetype table is VR-owned, so no TV data changes); `EnemyInputs` and `ScheduleAttack` read typed fields only. The same compiler runs headless as `check-pin-contract.mjs` (or an automation test on a candidate data dir) before `apps/vr/data` re-pins, so a rewording becomes a change request conversation, not a crash.

### Flow
1. Add typed fields and the archetype enum to `FEnemySpec`; compile them in `KernelDataRoster.cpp` with `MatchDouble`; fail load on a missing match or an unmapped id.
2. Golden test: compiled values equal today's Extract results for every roster row.
3. Replace the 11 Extract calls and the 19 id compares in Enemies.cpp; point the VrRulesLoad throw and spit modes at archetypes.
4. Delete Extract, its cache and lock; run conformance, RNG vectors, wave goldens and EnemyStepCost.
5. Add the pin-contract check and call it from the pin bump procedure in `apps/vr/data/README.md`.

### Expected impact
The pinned-data contract with the TV repo moves from 'fails at tick time, in some builds' to 'fails at load and before the re-pin, naming the cell'. The Quest APK cannot silently zero an enemy's behaviour. New creatures and the Bout 2 tuning become an archetype row, not another name compare, and the step loses its per-tick string work.

### Evaluation
Claim: compiling enemy prose at load removes every tick-time text read and turns a reworded TV cell into a load failure, with no behaviour change on today's pin.
Before: 11 Extract call sites over 9 patterns in Enemies.cpp, 3 of them reached every tick (:246, :333, :343); 0 of those magnitudes validated at load (KernelDataRoster.cpp:50-62); 19 enemy and attack id compares in Enemies.cpp, 6 in VrRulesLoad.cpp.
After: prediction. (1) Today's pin: A and B produce the same magnitudes, so the hashes are identical (falsified if any golden moves). (2) TV rewords the conscript back-off: A loads green, then a Development build stops on `checkf` within the first seconds of Wave 1 and a checks-off build plays a back-off of 0; B fails the load and the pin check names `conscript.behaviour`. (3) A new roster creature id: A steers it with the default walk-and-stop branch; B refuses to load until the id has an archetype. Falsifier: if an unmatched `FRegexMatcher` capture is not empty in a checks-off build, case 2's silent-zero reading is wrong (the crash reading still holds).
Method: simulation (three cases walked under A and B); the deciding instruments are the new archetype golden test, a fixture roster with one reworded cell, and EnemyStepCost before and after.
Result: unmeasurable until built; instruments named above.
Gate: architecture (consumption seam only; no pinned number or TV file is edited).

**Depends on:**
- dc04e03 (regex cache, the 2026-10-05 stabilize fix this replaces)
- BACKLOG: conformance generator broken since the pin rewrite (until fixed, the 45 MageArena.Kernel.Conformance.* tests are the byte-identity evidence)

**Write set:**
- `apps/vr/Game/Source/MageArenaVR/Kernel/KernelData.h`
- `apps/vr/Game/Source/MageArenaVR/Kernel/KernelDataRoster.cpp`
- `apps/vr/Game/Source/MageArenaVR/Kernel/EnemyArchetypes.h`
- `apps/vr/Game/Source/MageArenaVR/Kernel/EnemyArchetypes.cpp`
- `apps/vr/Game/Source/MageArenaVR/Kernel/Enemies.cpp`
- `apps/vr/Game/Source/MageArenaVR/Kernel/VrRulesLoad.cpp`
- `apps/vr/Game/Source/MageArenaVR/Tests/EnemyArchetypeTests.cpp`
- `apps/vr/tools/check-pin-contract.mjs`

**Acceptance (write as failing tests first):**
1. A fixture roster whose conscript behaviour reads 'backs away for 1.2 s' instead of 'back off 1.2 s' fails the kernel data load with an error naming the enemy id, the field and the pattern (today it loads green).
2. After load every roster enemy carries typed fields (BackoffS, EngagementSlots, NetWidthM, NetRootS, PullM, ChargeLaneM, ChargeRangeM, WallStunS, DrainManaPerS) equal to today's Extract results, checked row by row.
3. Enemies.cpp has no Extract, FRegexPattern or FScopeLock left, and the 19 `TEXT("<enemy or attack id>")` compares there become a switch on an archetype enum resolved at load.
4. A roster id with no archetype mapping fails load instead of falling into default steering.
5. The 45 conformance tests, the 8 RNG vectors and the wave goldens keep their hashes; MageArena.Kernel.EnemyStepCost stays under the 1 ms budget with a lower mean.
6. A pin-contract check run against a candidate TV data commit prints the compiled archetype table and its diff against the current pin, and exits non-zero when any cell fails to compile.

**Evidence:**

```
apps/vr/Game/Source/MageArenaVR/Kernel/Enemies.cpp:15-37 Extract (static TMap cache, FCriticalSection, `checkf(bMatched, TEXT("Missing enemy magnitude: %s / %s"))`, `FCString::Atod(*Matcher.GetCaptureGroup(1))`); `grep -c 'Extract(' Enemies.cpp` = 12 lines (the definition at :15 plus 11 calls at :72, :73, :80, :85, :86, :87, :120, :246, :280, :333, :343); per-tick reads at :246 (conscript), :333 (hound engagement slots), :343 (moth drain). Id compares: `grep -on 'TEXT("(conscript|mire_maw|thornback|netter|hush_moth|cinder_hound|slinger|net_cast|tongue_pull|thorn_charge)")'` gives Kernel/Enemies.cpp 19, Kernel/VrRulesLoad.cpp 6, Kernel/Games.cpp 2, Session/* 18. Loader stores the prose unchecked: Kernel/KernelDataRoster.cpp:50-52 (`TryGetStringField` effect, telegraph, onWallHit), :62 (`NeedString behaviour`). The failing matcher already exists: Kernel/KernelDataLoad.h `bool MatchDouble(const FString& Source, const FString& Pattern, double& Out, const FString& What, FKernelData& Data)`. Checks-off note: Kernel/Games.cpp:385 'a build with checks compiled out drops the expression inside check and checkf'. Prior work: git log dc04e03 'fix(spell-schools-and-ai): stop compiling a regex every tick for enemy behaviour text'; Tests/EnemyStepCostTests.cpp comment 'the two waves whose enemies read their behaviour text every tick'. Not in BACKLOG.md, scan-sweep.jsonl, ab.jsonl or V2-ROADMAP.
```

## M6 - Inject kernel data into the sim and add a pin-diff census for re-pins

Context `combat-sim-core` · spine: injectable simulation data context · XL · impact 8 · effort 6 · risk 4 · gate architecture

### Summary
Turn the process-wide `KernelData()` singleton into data the simulation carries: `FArenaState` holds a `const FKernelData*`, every kernel read goes through it, and the loader takes an explicit source (a pinned directory plus overrides) instead of the `GTextOverrides` global. On top of that seam, build the instrument the TV data contract is missing: a **pin-diff census** that runs the same seeded bouts against two data sets in one process and reports the per-metric delta, and an in-process sweep over variants of the VR-owned Air sheet.

### Description
Today the kernel cannot simulate any data but the one copy loaded at first call. `KernelData()` is a function-local static (`Kernel/KernelData.cpp:1825-1842`); 81 calls in 14 `Kernel/` files and 99 `SimTicks`/`SimDt`/`SimSeconds` calls (each reads `KernelData().SimStepHz`, `ArenaKernel.cpp:333-346`) bind every step to it. The loader already has a test seam (`LoadKernelDataWithOverrides`, `KernelData.cpp:1849-1855`), but its result can only be inspected, never stepped: its 6 call sites (`KernelLoadTests.cpp`, `AirTests.cpp`) only check load verdicts.

The consequence is that the census, which is the instrument every owner decision (DF-003 to DF-009) is made on, can sweep only `FVrRuleset` overlay knobs (`Tests/CensusTests.cpp:520-560`). It cannot answer the two questions the slice is about to ask:
1. **Re-pin safety.** CR-004 to CR-008 go to TV (V2-ROADMAP "Change requests to TV"). When TV accepts one and the VR re-pins, the overlay rule moves into pinned data. The re-pin procedure (`apps/vr/data/README.md`, "Re-pin") checks bytes only and has no behaviour step: nobody can show that "pin B without overlay" plays like "pin A plus overlay", which is the CR's whole claim.
2. **Air sheet tuning.** DF-008 (Lio loses every duel in about 20-24 s) may be answered by changing the VR-owned `spells-air.csv`. That file is in `FKernelData` (`KernelData.h:434-437`), so each variant needs a file edit and a fresh process.

The move (registry: *one authority per quantity* L3, *a verdict is bound to the content it judged* L6; technique `per-cell-seed-derivation-for-order-independence` for the diff's seeding):
- `FArenaState::Data` (non-owning, not hashed, so `StateHash` goldens stay bit-identical); `CreateArena(Seed, const FKernelData& = KernelData())`; `SimData(State)` replaces `KernelData()` in `Kernel/`; `SimTicks(Data, s)`; `Geometry` and `Catalog` take the data.
- `FKernelDataSource { PinnedDir; AirSheetPath; Overrides }` and `LoadKernelData(Source)`; `GTextOverrides` (`KernelData.cpp:18`) goes away. `KernelData()` stays as the default instance for the session.
- `MageArenaDesign.Census.PinDiff` with `-MageArenaPinB=<dir>`: the same seeds, bouts and reference script against A and B, writing `runs/pin-diff/<pinA>-<pinB>.json` with each metric's delta and both pin hashes. `check-pin.mjs --export <commit> <dir>` writes a candidate directory from `git show`, as the re-pin procedure already reads it.
- Census output records `pinHash` and an air-sheet hash (today neither is written: `CensusTests.cpp:486`, `RivalCensusTests.cpp:428` write `seeds` only).

The migration is mechanical and the guard already exists: the 45 conformance vectors and the hash goldens must not change by one bit.

### Flow
1. Add `Data` to `FArenaState`, the `CreateArena` default argument and `SimData`; compile with `KernelData()` still in place.
2. Codemod `Kernel/` file by file (`ArenaKernel`, `Water`, `Fire`, `Air`, `Enemies`, `MageAI`, `Games`, `Training`, `ArenaThreats`, `Geometry`, `Catalog`, `VrRules`); run the conformance vectors and `MageArena.Fire.Golden` after each file.
3. Replace the `GTextOverrides` global with `FKernelDataSource`; keep `LoadKernelDataWithOverrides` as a wrapper.
4. Add a lint test: no `KernelData()` in `Kernel/*.cpp` except the default argument.
5. Build `Census.PinDiff` and the air-sheet sweep; stamp pin hashes into every census output.
6. Run PinDiff with A = B (zero delta control), then with a candidate where one enemy's HP is +50 % (seeded red).
7. Document the behaviour step in `apps/vr/data/README.md` "Re-pin" (orchestrator, after verification).

### Expected impact
- Every re-pin, which is how CR-004 to CR-008 come home, gets a measured behaviour diff instead of a byte check. That makes the TV canon contract two-way and safe, ahead of the 31 Oct gate.
- DF-008 and later Air sheet decisions get census numbers per variant in one run, the same way DF-003 got them for overlay knobs.
- Tests can step modified data (for example an edge-case enemy) without touching the pinned files.
- It removes the kernel's last mutable global, which a parallel census (seeds across threads) needs.

### Evaluation
- **Claim:** after the seam, one process can simulate two data sets side by side with bit-identical results for the default set, and a re-pin's behaviour delta becomes one command.
- **Before:** `KernelData()` is a static singleton (`KernelData.cpp:1825-1842`). There are 81 calls in `Kernel/` (248 in 42 module files) and 99 `Sim*` timing calls in `Kernel/` that read it. 0 of 6 `LoadKernelDataWithOverrides` call sites step a sim. The census varies 0 data fields (knob sets only, `CensusTests.cpp:520-560`). 0 census outputs carry `PinHash`.
- **After (predicted):** 0 `KernelData()` calls in `Kernel/*.cpp` beyond the default argument; 45/45 vectors and both Fire goldens unchanged; PinDiff A=A gives 0 delta on every metric; the +50 % HP candidate gives a non-zero delta on the bout that spawns that enemy and 0 on the bouts that do not.
- **Method:** simulation, three cases walked under A (today) and B (after). (1) TV accepts CR-005 and the VR re-pins. A: copy the bytes, run the census in a new checkout, compare to an older run by hand, and two runs differ in both pin and code. B: one `PinDiff` run, with the delta per bout and both hashes in one JSON. (2) DF-008 Air variant: A needs one edited CSV and one process start per variant; B runs N variants in one census. (3) A regression test for an edge enemy: A has to override the file and can only check that it loads; B steps the overridden data. The prediction is falsified if any conformance vector or golden hash changes after the codemod, or if the A=A diff is not exactly zero (a hidden global that the seam missed).
- **Result:** unmeasured. The instrument that decides it is the `Census.PinDiff` A=A control plus the 45 vectors and the `Fire.Golden` hashes, run after step 2.
- **Gate:** architecture (it touches every kernel file and the `FArenaState` shape; no pinned data edited).

**Depends on:**
- sweep 5 KernelData.cpp split (be7583a, done)
- T16 landing before the Session/*.cpp call sites move
- ARCHITECTURE-REVIEW F1 (PlayerPowerMult) optional, lands first if open

**Write set:**
- `apps/vr/Game/Source/MageArenaVR/Kernel/SimTypes.h`
- `apps/vr/Game/Source/MageArenaVR/Kernel/ArenaKernel.h`
- `apps/vr/Game/Source/MageArenaVR/Kernel/ArenaKernel.cpp`
- `apps/vr/Game/Source/MageArenaVR/Kernel/KernelData.h`
- `apps/vr/Game/Source/MageArenaVR/Kernel/KernelData.cpp`
- `apps/vr/Game/Source/MageArenaVR/Kernel/KernelDataLoad.h`
- `apps/vr/Game/Source/MageArenaVR/Kernel/KernelDataWater.cpp`
- `apps/vr/Game/Source/MageArenaVR/Kernel/KernelDataRoster.cpp`
- `apps/vr/Game/Source/MageArenaVR/Kernel/Geometry.h`
- `apps/vr/Game/Source/MageArenaVR/Kernel/Geometry.cpp`
- `apps/vr/Game/Source/MageArenaVR/Kernel/Catalog.h`
- `apps/vr/Game/Source/MageArenaVR/Kernel/Catalog.cpp`
- `apps/vr/Game/Source/MageArenaVR/Kernel/Water.cpp`
- `apps/vr/Game/Source/MageArenaVR/Kernel/Fire.cpp`
- `apps/vr/Game/Source/MageArenaVR/Kernel/Air.cpp`
- `apps/vr/Game/Source/MageArenaVR/Kernel/Enemies.cpp`
- `apps/vr/Game/Source/MageArenaVR/Kernel/MageAI.cpp`
- `apps/vr/Game/Source/MageArenaVR/Kernel/Games.cpp`
- `apps/vr/Game/Source/MageArenaVR/Kernel/Training.cpp`
- `apps/vr/Game/Source/MageArenaVR/Kernel/ArenaThreats.cpp`
- `apps/vr/Game/Source/MageArenaVR/Kernel/VrRules.cpp`
- `apps/vr/Game/Source/MageArenaVR/Kernel/VrRulesLoad.cpp`
- `apps/vr/Game/Source/MageArenaVR/Session/SessionFlow.cpp`
- `apps/vr/Game/Source/MageArenaVR/Session/ArenaSession.cpp`
- `apps/vr/Game/Source/MageArenaVR/Tests/KernelDataInjectionTests.cpp`
- `apps/vr/Game/Source/MageArenaVR/Tests/CensusPinDiffTests.cpp`
- `apps/vr/Game/Source/MageArenaVR/Tests/CensusTests.cpp`
- `apps/vr/Game/Source/MageArenaVR/Tests/RivalCensusTests.cpp`
- `apps/vr/tools/check-pin.mjs`

**Acceptance (write as failing tests first):**
1. MageArena.Kernel.DataInjection.StepsVariant: a variant loaded with movement.walkMps doubled, stepped 60 ticks of full Move, moves the mage 2x the distance of the default-data arena, and KernelData().PinHash is unchanged
2. MageArena.Kernel.DataInjection.Interleaved: two arenas on two data sets, stepped alternately for 600 ticks, end with the same StateHash as each stepped alone
3. MageArena.Kernel.NoGlobalData: a source lint finds 0 KernelData() calls in Kernel/*.cpp outside the CreateArena default argument
4. All 45 MageArena.Kernel.Conformance.* vectors and MageArena.Fire.Golden hashes are bit-identical before and after the migration
5. MageArenaDesign.Census.PinDiff with -MageArenaPinB pointing at a copy of pin A reports exactly 0 delta on every metric (control); with one enemy's hp +50 % it reports a non-zero delta only on bouts that spawn that enemy
6. Every census JSON (Census.Sweep, RivalCensus, PinDiff) carries pinHash and airSheetHash of the data it ran
7. A PinDiff whose candidate dir fails to load refuses to run and names the load error (no partial report)

**Evidence:**

```
- `Kernel/KernelData.cpp:1825-1842`: `static const FKernelData Loaded = []() {...}()` is the only data instance the kernel can step.
- `Kernel/KernelData.cpp:18`: `const TMap<FString, FString>* GTextOverrides = nullptr;`, a mutable global set only inside `LoadKernelDataWithOverrides` (`:1849-1855`).
- grep `KernelData()`: 81 calls in `Kernel/` (Air 7, ArenaKernel 14, ArenaThreats 2, Catalog 9, Enemies 4, Fire 6, Games 7, Geometry 4, KernelData 3, MageAI 7, Training 4, VrRules 2, VrRulesLoad 7, Water 5); 248 calls in 42 files module-wide (Session 6 files, Tests 21 files).
- grep `Sim(Ticks|Dt|Seconds)(`: 99 in `Kernel/`, 280 module-wide; `ArenaKernel.cpp:333-346` reads `KernelData().SimStepHz`.
- grep `LoadKernelDataWithOverrides`: `Tests/KernelLoadTests.cpp:39,50,246,253`, `Tests/AirTests.cpp:333,336`, all load-verdict checks; none calls `CreateArena` or `StepArena` with the result.
- `Tests/CensusTests.cpp:520-560`: candidates `opendome`, `wide`, `halfward` and `quarterward` set only ruleset knobs (`ManaRegen`, `StaffDrain`, `WallArc`, `WardDrain`...).
- grep `PinHash` outside `KernelData.cpp`: only `KernelLoadTests.cpp:240-255` and `AirTests.cpp:338`; census writers record `seeds` (`CensusTests.cpp:486,771`, `RivalCensusTests.cpp:428,471`) and no data hash.
- `Kernel/KernelData.h:434`: "The air rows are VR-owned (not pinned), so their file does not enter PinHash": the sheet DF-008 would tune is hashed nowhere.
- `apps/vr/data/README.md` "Re-pin": copy, rewrite `PINNED.json` and run `check-pin.mjs`. These are byte checks only.
- `Kernel/Seams.h:5-7`: the overlay is already injected (`FVrRuleset*` on `TryCreateGames`); the data is not, so the asymmetry is the gap.
- Not a re-proposal: ARCHITECTURE-REVIEW section 3 declines "the kernel as its own UE module" (packaging), and this card is a data seam; the scan history (`loader-refs`, `state-hash`) is unrelated.
```

## M7 - Make the kernel bit-identical on Win64, Quest and V8, and prove it on device

Context `combat-sim-core` · spine: cross-platform deterministic simulation · XL · impact 7 · effort 6 · risk 5 · gate architecture

### Summary
Make the kernel's numbers the same bits on every runtime it will run on (Win64 MSVC, Quest ARM64 clang, and the TV kernel's V8), and prove it on the device. Route every transcendental through one portable `SimMath` (an fdlibm-lineage `sin`/`cos`/`atan2` and V8's `hypot` algorithm), turn off floating-point contraction for kernel code, and add a packaged-build self-test (`-MageArenaKernelSelfTest`) that runs the 45 conformance vectors and the Fire goldens and prints one `MAGEVR_KERNEL_HASH` marker that the V1 device lane compares with Win64.

### Description
The kernel is deterministic per build but not per platform, and nothing measures that difference.
- **Platform math.** 21 calls in kernel code go straight to the C library where the result is not exact: `std::hypot` (`SimMath.h:19`, `Geometry.cpp:23,32`, `VrRules.cpp:82,188`), `std::cos`/`std::sin` (`SimMath.h:66,99-100`, `Air.cpp:215,233,237,255,256`, `VrRules.cpp:200`), `std::atan2` (`Air.cpp:220`, `VrRules.cpp:193`), plus `FMath::Cos` in the ward arc test (`Combat/AbsorbResolver.cpp:144`). `SimLength` is `hypot` and is reached from 104 `SimLength`/`SimDistance`/`SimUnit`/`SimInArc`/`SimRotate` sites in `Kernel/`. MSVC's UCRT and Android's bionic implement these differently, and V8 (the TV kernel) uses an fdlibm port for sin/cos/atan2 and its own `hypot`.
- **Contraction.** No kernel file or `MageArenaVR.Build.cs` controls FP contraction (grep `fp contract|FP_CONTRACT|ffp-contract`: 0). Clang may fuse `a*b+c` into an ARM64 `fmadd` (for example `SimDot`, `SimSegmentHit`'s discriminant, `SimMath.h:37-40,77-90`), and MSVC x64 does not. Whether UBT's Android flags already prevent it is **not verified here** (the engine's UBT sources are not installed at the recorded path), and that is the card's first read.
- **No device evidence path.** All 203 automation tests are `EditorContext` only (0 `ClientContext`), so none can run in an APK. `data/conformance/` is not staged (`Config/DefaultGame.ini:12-14` stages Clips, `data/vr` and `data/pinned`). The vectors are checked at 1e-6 relative / 1e-9 absolute (`Tests/KernelConformanceTests.cpp:29-30`, enforced at `:1048`), and their 279 `stateHash` fields (TS JSON hashes) are never compared bit for bit.

Why it matters for the contest: November is "the largest risk in the plan" (PROJECT-PLAN, about 18 headset days). The plan's evidence model is replay: device hand traces replayed headless on Win64 (P3/P5, "full-session playtests: scripted replay of an owner session reaching victory"), with ghost duels later. If Quest and Win64 differ by one ulp in a ward arc test (`SimInArc` with a 1e-12 epsilon at the ±69/71° boundary the plan tests), a device bug ("that perfect did not register") cannot be reproduced on the desktop, and the census numbers the owner signed off are not proven to be the game on the headset (registry L1 *unmeasured is not a pass*, L12 *an instrument proves it had input*). Matching V8 also lets the TV channel check a VR change request by hash instead of by tolerance.

### Flow
1. Read UBT's Win64 and Android compile flags for contraction and fast-math; record them in `docs/XR-TOOLCHAIN.md`.
2. Add `Kernel/SimMathPortable.cpp`: an fdlibm-lineage `SimSin`/`SimCos`/`SimAtan2` and a `SimHypot` with V8's scaled, compensated algorithm. Compile that TU and `SimMath.h` users with contraction off (`#pragma clang fp contract(off)` / `#pragma fp_contract(off)` behind one macro in `SimMath.h`).
3. Add `tools/conformance/math-oracle.mjs`: seeded inputs, `Math.sin/cos/atan2/hypot`, FNV hash per function, committed expected hashes. Add the C++ twin `MageArena.Kernel.Math.V8Oracle`.
4. Replace the 21 call sites and `AbsorbResolver.cpp:144`; regenerate the two Fire golden hashes (`data/scenarios-vr/`) in their own commit with before and after values; check that the 45 vectors still pass and report each vector's maximum deviation.
5. Add `-MageArenaKernelSelfTest`: a non-shipping boot path that runs the vectors and the golden duels without the automation framework and prints `MAGEVR_KERNEL_HASH=` and `MAGEVR_KERNEL_SELFTEST=PASS|FAIL`. Stage `data/conformance` in non-shipping configs. `package-android.ps1` records that the path is compiled in.
6. Win64 staged build: the marker equals the editor's. V1 (pof device lane, adb logcat): the device marker equals Win64's. That becomes a row on the V1 checklist.

### Expected impact
- A device run and its desktop replay agree bit for bit, so November bugs are reproduced on the desktop instead of re-tested on the headset. This is the cheapest way to save headset hours.
- The census sign-offs (DF-003 to DF-009) carry over to the Quest build by proof, not by assumption.
- The conformance check with TV can tighten from 1e-6 toward exact, and a CR can quote a hash that TV reproduces.
- It opens the planned async ghost duels and replay re-render (PROJECT-PLAN sections 5 and 8, P10) without platform caveats.

### Evaluation
- **Claim:** after the change, `MAGEVR_KERNEL_HASH` is identical on Win64 editor, a Win64 staged build and Quest 3. The portable functions match node's `Math.*` on 100k seeded inputs each.
- **Before:** 21 non-exact libm call sites in `Kernel/` plus 1 in `Combat/`; 104 `SimLength`-family call sites in `Kernel/`; 0 contraction controls; 0 of 203 tests runnable in a packaged build; `data/conformance` not staged; vectors gated at 1e-6 / 1e-9; 0 of 279 vector `stateHash` fields compared.
- **After (predicted):** 0 `std::(sin|cos|atan2|hypot)` in `Kernel/`; V8 oracle 4/4 hashes equal; 45/45 vectors pass and the reported maximum deviation drops; editor, staged Win64 and (at V1) device markers equal.
- **Method:** simulation now, measurement at V1. (1) A 120 s Brennic duel replayed from a device trace on Win64. A: the ward arc test at 69.9999° flips on one platform after a fused multiply, the duel forks and the reproduction fails. B: both evaluate the same bits. (2) The census median on Win64 against Quest: in A it is assumed equal; in B the marker proves the same kernel. (3) A TV reviewer checks CR-005 by running the VR vector. A: within 1e-6. B: the same hash under V8. The prediction is falsified if the oracle hashes differ for any function (the port is not V8-equal), or if the Win64 staged marker differs from the editor's (a build-config difference, found before the device arrives).
- **Result:** unmeasurable until V1 for the device row. The deciding instruments are the node oracle and `Math.V8Oracle` (available now, headless) and the `MAGEVR_KERNEL_HASH` marker on device.
- **Gate:** architecture (it changes the kernel's numeric basis and regenerates the hash goldens; no pinned data edited).

**Depends on:**
- BACKLOG: conformance generator fix (only to extend vectors; not needed for the oracle)
- V1 device lane (pof adb/logcat pull, PROJECT-PLAN P3) for the device row only
- STACK-OPPORTUNITIES F5 (pinned data staged, applied)

**Write set:**
- `apps/vr/Game/Source/MageArenaVR/Kernel/SimMath.h`
- `apps/vr/Game/Source/MageArenaVR/Kernel/SimMathPortable.cpp`
- `apps/vr/Game/Source/MageArenaVR/Kernel/Air.cpp`
- `apps/vr/Game/Source/MageArenaVR/Kernel/Geometry.cpp`
- `apps/vr/Game/Source/MageArenaVR/Kernel/VrRules.cpp`
- `apps/vr/Game/Source/MageArenaVR/Combat/AbsorbResolver.cpp`
- `apps/vr/Game/Source/MageArenaVR/Tests/KernelMathTests.cpp`
- `apps/vr/Game/Source/MageArenaVR/Tests/KernelConformanceTests.cpp`
- `apps/vr/Game/Source/MageArenaVR/Session/KernelSelfTest.cpp`
- `apps/vr/Game/Config/DefaultGame.ini`
- `apps/vr/tools/conformance/math-oracle.mjs`
- `apps/vr/data/conformance/math-oracle.json`
- `apps/vr/data/scenarios-vr/fire-duel-competence-1.json`
- `apps/vr/data/scenarios-vr/fire-duel-competence-1-5.json`
- `apps/vr/tools/package-android.ps1`
- `docs/XR-TOOLCHAIN.md`

**Acceptance (write as failing tests first):**
1. MageArena.Kernel.Math.V8Oracle: 100k seeded inputs each for SimSin, SimCos, SimAtan2 and SimHypot hash (FNV-1a over the bits) to the values math-oracle.mjs computes with node's Math.*; 4/4 equal
2. Seeded red for the oracle: perturbing one polynomial coefficient in SimSin changes its hash, and the test fails
3. Source lint: 0 std::(sin|cos|atan2|hypot) and 0 FMath::Cos in Kernel/ and Combat/AbsorbResolver.cpp outside SimMathPortable.cpp
4. MageArena.Kernel.Math.NoContraction: a*b+c on inputs where the fused and unfused results differ returns the unfused value (passes on Win64 now and in the Android self-test later)
5. All 45 conformance vectors pass, and the test logs each vector's maximum absolute deviation (reported, not loosened)
6. -MageArenaKernelSelfTest in a Win64 staged (non-editor) build prints MAGEVR_KERNEL_SELFTEST=PASS and a MAGEVR_KERNEL_HASH equal to the editor run's
7. V1 checklist row: the same marker pulled from Quest 3 logcat equals the Win64 value

**Evidence:**

```
- grep `std::(cos|sin|atan2|hypot)` in `Kernel/`: `Air.cpp:215,220,233,237,255,256` (10 calls), `Geometry.cpp:23,32` (2 hypot; `:47` fmod is exact), `SimMath.h:19,66,99,100` (4), `VrRules.cpp:82,188,193,200` (5). That is 21 calls, plus `Combat/AbsorbResolver.cpp:144` `FMath::Cos` in the ward arc limit.
- grep `Sim(Length|Distance|Unit|InArc|Rotate)(`: 104 call sites in `Kernel/*.cpp`, 175 module-wide.
- grep `fp contract|FP_CONTRACT|ffp-contract` over the module and `MageArenaVR.Build.cs`: 0 hits; Build.cs sets no compiler flags.
- grep `EAutomationTestFlags::(EditorContext|ClientContext)` in `Tests/`: 203 EditorContext, 0 ClientContext.
- `Config/DefaultGame.ini:12-14`: stages `../Clips`, `../../data/vr` and `../../data/pinned`, not `data/conformance` (45 files).
- `Tests/KernelConformanceTests.cpp:29-30` (`Rel = 1.0e-6`, `Abs = 1.0e-9`), `:1048` (it refuses only looser values). grep `stateHash` in the test: 0. The vectors hold 279 `stateHash` fields across 45 files.
- `Kernel/ArenaKernel.h:41-43`: the C++ `StateHash` is "FNV-1a over a canonical dump of this process. Equal hashes mean two C++ runs match", so it is process-scoped by its own doc comment.
- `Tests/FireTests.cpp:1185-1215`: `MageArena.Fire.Golden` compares duel hashes to `data/scenarios-vr/fire-duel-competence-1*.json` (Win64-only goldens).
- PROJECT-PLAN: the November risk paragraph (about 18 headset days), the P3/P5 trace replay rows, and "async ghost duels ... the deterministic kernel makes these cheap later" (section 5).
- `tools/package-android.ps1:179-192`: a MARKER_* convention exists to extend.
- Not verified: UBT's actual Android `-ffp-contract` setting and the UCRT, bionic and V8 ulp differences on the 21 sites. Flow step 1 and the oracle decide them.
```

## M8 - Ship every VR overlay rule as replayable conformance vectors, not prose CRs

Context `game-tests` · spine: executable cross-channel rule contract · XL · impact 8 · effort 7 · risk 5 · gate contract

### Summary
Turn the change-request channel to TV from prose into executable law: the VR C++ kernel records its overlay rules (ember spit, rival phases and the clock surge, the Fire and Air schools, the collar events) into conformance vectors in the same JSON schema the 45 pinned TS vectors use, extended with an overlay block. The VR suite replays them tick by tick, and each CR ships its vectors as the definition TV ports against.

### Description
Today conformance runs in one direction only. The 45 vectors in apps/vr/data/conformance were recorded by the TS oracle and prove the C++ port matches TV on the null-overlay path. Everything VR invents (CR-001 to CR-005 written, CR-006 and CR-007 queued in V2-ROADMAP) goes back to TV as a markdown table. CR-004 even states 'Conformance impact: None'. That means two things. (1) Inside VR, the overlay branches that ARCHITECTURE-REVIEW F1 counts in ArenaKernel.cpp (65 lines mention Rules) are guarded tick by tick by nothing. The guards are two Fire.Golden end-state summaries and one rival MD5 digest, and Air, ember spit, the split latch and the collar have no golden at all. (2) Across channels, TV must re-derive each rule from prose, and a one-tick reading difference (for example 'released after the pinned burst delay') shows up only after a re-pin, as unexplained census drift. The move is a recorder (test-only code, behind a command-line switch) that writes FArenaState checkpoints plus the overlay fields, a replay driver that passes the VR ruleset, a manifest with sha256 like PINNED.json, and a CR template whose binding part is the vector list. Technique: design-canon-as-executable-law (registry, systems-canon): a canon that a machine replays, not a number restated in prose. The canon owner does not change: TV still decides, and once TV accepts a CR and re-pins, its oracle regenerates the vectors and they move from conformance-vr/ to conformance/ byte-identical. That is the proof the CR landed as specified.

### Flow
1. VectorSchema.h: the existing schema (driver, seed, ticks, frames, ops, checkpoints, events, tolerance) plus overlay {rulesetSha256, variant, airFinal} and fire, air and rival blocks on actors and state.
2. VectorRecorder.cpp: drive a scripted scenario (games driver with rules, or arena setup with ops) and emit checkpoints every N ticks, at op ticks and at named ticks (release, break, grant).
3. KernelConformanceTests.cpp: Replay() learns the overlay block, CompareActor learns fire, air and rival, and a ConformanceVr complex test enumerates conformance-vr/.
4. Record the first set: ember-spit-bout, brennic-phases-c1, brennic-phases-c15, lio-final-c15, split-latch-clear, fire-wall-volley, collar-tally. Manifest and check script.
5. CR files get a Vectors section. CR-006 and CR-007 are written vectors-first.
6. On TV acceptance and re-pin, TV's oracle regenerates them and VR moves them to conformance/.

### Expected impact
- Every overlay change in the 23 Oct to 18 Nov window (DF-005 to DF-009 tuning, lane C interrupts and rallies) fails at the first diverging tick with a field path, instead of as a silently moved census median.
- CR-004 to CR-007 become portable without a VR engineer explaining them. That is the TV-canon data contract the plan depends on for the PC channel.
- It also replaces the stalled byte-identical acceptance line of every card (BACKLOG: generator broken) for the overlay half.

### Evaluation
Claim: overlay rules gain tick-level, field-addressed regression and a machine-checkable cross-channel definition.
Before: 45 vectors, all null-overlay (KernelConformanceTests.cpp has 0 references to Fire, Air or Rules; TryCreateGames at :1112 passes no ruleset). Overlay end-state goldens: 2 (FireTests.cpp:1185-1188) + 1 digest (RivalTests.cpp:36). Tick-level overlay vectors: 0. CRs with an executable definition: 0 of 5.
After: predicted >= 7 VR overlay vectors replayed per run, all 5+ CRs citing vector sha256s, and an injected 1-tick drift in any overlay rule localised to tick and field.
Method: simulation, three cases under A (today) and B (vectors). Case 1, F1's power-rule fold accidentally drops the split-path multiplier. A: the 45 vectors pass (null path), Fire.Golden catches it only if the fire duel's end hash moves (it reports the hash, not where). Air has no golden, so an Air-final change is missed outright. B: lio-final-c15 fails at the first split cast with actors[0].hp. Case 2, TV ports CR-004 and releases the death ember one tick late. A: found only after the re-pin, as a DF-007 timing shift. B: TV's oracle replay of ember-spit-bout fails before merge. Case 3, the owner adopts proposal knobs. A: the census reruns, nothing pins the per-tick behaviour. B: re-recording the proposal vectors is an explicit, reviewable diff. Falsified if the overlay fields cannot be recorded deterministically (two recordings differ) or if TV declines to replay them. The VR-local regression value then remains, but the cross-channel half is void.
Result: unmeasurable until built. The deciding instrument is the acceptance test 'Mutation proof' plus one TV-side replay of ember-spit-bout.
Gate: contract. It changes the form in which change requests are handed to the canon owner (kiro/mage-arena-tv). It edits no pinned number.

**Depends on:**
- BACKLOG: conformance generator broken since the 2026-10-07 pin rewrite (not required: this recorder is C++, no tsx; fixing it lets TV regenerate the pinned 45 alongside)
- ARCHITECTURE-REVIEW F1 (one FVrRuleset method per overlay hook) - lands better after, not blocked by it

**Write set:**
- `apps/vr/Game/Source/MageArenaVR/Tests/KernelConformanceTests.cpp`
- `apps/vr/Game/Source/MageArenaVR/Tests/Conformance/VectorSchema.h`
- `apps/vr/Game/Source/MageArenaVR/Tests/Conformance/VectorRecorder.cpp`
- `apps/vr/Game/Source/MageArenaVR/Tests/OverlayVectorTests.cpp`
- `apps/vr/data/conformance-vr/`
- `apps/vr/data/conformance-vr/MANIFEST.json`
- `apps/vr/tools/conformance/check-vr-vectors.mjs`
- `docs/change-requests/CR-004-hound-ember-spit.md`
- `docs/change-requests/CR-005-rival-phases.md`
- `docs/change-requests/README.md`

**Acceptance (write as failing tests first):**
1. MageArena.Kernel.ConformanceVr.<name> is a complex test enumerating apps/vr/data/conformance-vr/*.json; with the directory empty it fails with 'No VR overlay vectors' (same NoVectors rule as KernelConformanceTests.cpp:1344-1356)
2. A vector with driver 'games' and an 'overlay' block (combat.vr.json sha256 + a named ruleset: live | proposal) replays through TryCreateGames(..., Rules, bAirFinal) and fails if the recorded combat.vr.json sha256 differs from the loaded file
3. CompareActor also compares the fire block (heat, lockUntil, sunfallCastTick, sunfallTick, bSunfallLoosed, cooldowns, trails count), the air block (momentum, form, cooldowns, volley) and FVrRuleset.RivalRuntime (phasesBroken, breakTicks, grantTick); flipping any one of them in a recorded checkpoint fails at that tick with the field path in the message
4. Recorder round trip: recording ember-spit-bout (seed 40003, creatures, live overlay) twice gives byte-identical JSON, and replaying it passes; recording with -MageArenaProposal gives a different file that also replays
5. Mutation proof: changing the CR-004 death-ember release from 36 to 37 ticks makes ConformanceVr.ember-spit-bout fail naming 'projectiles' or 'events' at the release tick, while all 45 pinned Kernel.Conformance vectors still pass
6. check-vr-vectors.mjs verifies MANIFEST.json (path, sha256, bytes, recordedAt commit, CR id) like check-pin.mjs and has a --self-test that flips one byte and must fail
7. Every open CR file (CR-004, CR-005, and CR-006/CR-007 when written) has a 'Vectors' section listing the conformance-vr files and sha256 that define the rule; a lint test fails when a CR names a vector missing from MANIFEST.json

**Evidence:**

```
apps/vr/data/conformance/: 45 *.json (ls | wc -l). KernelConformanceTests.cpp: grep -c 'Fire\.|\.Air\.' = 0; grep 'Rules' = no hits; :1112 `TryCreateGames(Games, Seed, Preset, StartWave, bReference)`, no Rules argument, although Games.h:66 takes `const FVrRuleset* Rules` and `bAirFinal`. CompareActor :424-624 compares base and water fields only (flow, crests, decoy...). FireTests.cpp:1185-1188: the only fire goldens are 2 end-state summaries (hash, ticks, hp) in data/scenarios-vr/. RivalTests.cpp:36: one MD5 digest of the null path. ArenaKernel.cpp: grep -c Rules = 65 (ARCHITECTURE-REVIEW F1: 11 `if (Rules` branches). docs/change-requests/CR-004-hound-ember-spit.md 'Conformance impact: None: the 45 Kernel.Conformance.* tests pass'. V2-ROADMAP 'Change requests to TV': CR-004..CR-008, 'Numbers stay overlay-only until TV accepts'. BACKLOG: generate.mjs ~62 hard-codes the pre-rewrite hash, tsx not installed. oracle.mjs:257-277 already models drivers training/games/mage/enemies, so the schema is shared ground.
```

## M9 - Census a human-shaped player population through the real hand pipeline

Context `game-tests` · spine: player-skill proxy census · L · impact 8 · effort 6 · risk 4 · gate none

### Summary
Every balance number the owner decides on (DF-003 bands, DF-006 to DF-009, the 7-10 min day) is measured on one omniscient, zero-latency, normal-clip-only scripted player. Give the census a seeded population of human-shaped players (reaction delay, slow and sloppy gestures, 30 Hz or 25 Hz hands, onset latency, sigil misdraws from the test corpus), played through the same hand pipeline. The slice's difficulty is then reported per skill band, not per opponent competence alone.

### Description
The census and the four MageArenaDesign bouts all run FArenaSession with SetScripted(true). DecideScript reads exact threat ETAs from the kernel's telegraphs and projectiles (ArenaSession.cpp:871-980, 1361-1363: blinks start when the ETA is inside 0.22-0.30 s), and every action plays EClipVariant::Normal (10 call sites in ArenaSession.cpp, 0 slow or sloppy). So the census seeds vary only the AI's RNG. The player is the same perfect machine every time (FullSeated's own comment: 'the script is deterministic, so a retry of bouts 1-3 loses again'). Meanwhile the suite already owns every ingredient of a human-shaped player, each proven in isolation and never fed into a bout: sloppy-variant sigil accuracy with a 0.90 floor (SigilTests.cpp:513), 180 impostor and 270 corpus clips, held and extrapolated 30/25 Hz stream models with ratchets (HeldSampleTests.cpp:31-41, 175-199), and ward latency injection to 100 ms (WardTests.cpp:635). The move: lift the stream models out of the test file into a Hands-side decorator (HandStreamModel), add a seeded PlayerProfile that the extracted FScriptedPlayer consults (reaction delay distribution, variant mix, stream model, latency, chance of a corpus misdraw), and run a population census: bands novice / competent / expert / oracle across wave 1, creatures, Brennic and Lio. Techniques: proxy-driver-limit-declaration (every figure carries what the stand-in was and 'human sample: none'; a result tops out at 'behaves', never 'felt') and per-cell-seed-derivation-for-order-independence (profile randomness keyed by cell identity, so adding a band never re-rolls another). The gesture pipeline stays the only input path, which this phase's rule requires.

### Flow
1. Move MakeHeldClip, MakeExtrapolatedClip and the latency warp into Hands/HandStreamModel, applied on PlayLoadedClip. HeldSampleTests calls the moved code, ratchets unchanged.
2. PlayerProfile plus player-profiles.json (VR data, not combat numbers): bands with delay mean and sd, variant weights, stream model, latency, misdraw rate.
3. FScriptedPlayer (N3) asks the profile before each PlayAction: delay the decision, pick the variant, sometimes play an impostor or corpus clip for a sigil.
4. The oracle profile is the identity, proven byte for byte against today's chains.
5. PopulationCensusTests: (band x scenario x seed) grid, CSV and summary with a proxy declaration, structure-only asserts. RivalCensusTests gains a band column.
6. Report win rate, median time, perfects per minute and Sunfall escapes per band beside the DF-003 targets. The owner reads bands; nothing is tuned by the card.

### Expected impact
- Before the 18 Oct mechanics gate and the 23 Oct cut decision, the owner sees whether the Tiro day is beatable by a non-perfect player, not just by the script. Today the script wins DF-008's Lio duel in about 20-24 s, and nothing says whether a novice wins it at all.
- Gesture-pipeline regressions (onset lag, a variant that stops recognising) show up as a band's win rate moving, the first end-to-end link from recogniser quality to game outcome.
- The Quest port (V1) gets a desktop prior for the 30 Hz camera case per bout, not per gesture.

### Evaluation
Claim: bout outcomes depend on player skill along axes the census cannot vary today, and a banded census exposes that before device playtests.
Before: census players = 1 script and 3 policy variants of it (CensusTests.cpp:561-580), 0 reaction delay, 0 non-normal clips in bouts (grep EClipVariant::Slow|Sloppy in Session/ArenaSession.cpp, SessionFlow.cpp, DayFlow.cpp = 0), 0 bouts on modelled 30 Hz hands. Variance source per row: the kernel seed only (CensusTests.cpp:729-733, RivalCensusTests.cpp:407-409).
After: predicted 4 bands x 4 scenarios x 20 seeds = 320 rows. The prediction is a monotone win-rate gradient oracle >= expert >= competent >= novice, with at least one scenario where the novice band misses the DF-003 70 % floor that the script meets.
Method: simulation, three cases. Case 1, Brennic c1. A: the script wins (FireSeated's assertion), and DF-003's 'win >= 70 % at competence 1' is read as met. B: a 0.25 s reaction delay starts blinks after the 0.22-0.30 s window, so Sunfall connects and the novice band's win rate is visible separately. Case 2, Wave 1 (known red). A: one deterministic loss, unknown whether it is a script defect or a balance defect. B: if the expert band also loses, it is balance; if only the script loses, it is the script. Case 3, a ward detector change adds 40 ms of onset lag. A: HeldSample ratchets stay green inside their 47.2 ms bound, and the census does not move because it plays 72 Hz normal clips. B: the perfects-per-minute column of the held30 band drops. Falsified if all bands land within +-5 pp of the oracle on every scenario: player skill would then not move outcomes, a design finding in itself.
Result: unmeasurable until built. The instrument is MageArenaDesign.Census.Population at 20 seeds, compared with the oracle band.
Gate: none. It adds a measuring instrument and VR-only profile data, and touches no pinned number. Restating DF-003 targets per band would be a later owner direction.

**Depends on:**
- ARCHITECTURE-REVIEW 6.5 N3: FScriptedPlayer extraction (Session/ScriptedPlayer.cpp) - the profile plugs into it
- HeldSampleTests stream models (F7(b), already landed) - moved, not rewritten

**Write set:**
- `apps/vr/Game/Source/MageArenaVR/Hands/HandStreamModel.h`
- `apps/vr/Game/Source/MageArenaVR/Hands/HandStreamModel.cpp`
- `apps/vr/Game/Source/MageArenaVR/Session/PlayerProfile.h`
- `apps/vr/Game/Source/MageArenaVR/Session/PlayerProfile.cpp`
- `apps/vr/Game/Source/MageArenaVR/Session/ScriptedPlayer.cpp`
- `apps/vr/Game/Source/MageArenaVR/Tests/HeldSampleTests.cpp`
- `apps/vr/Game/Source/MageArenaVR/Tests/PopulationCensusTests.cpp`
- `apps/vr/Game/Source/MageArenaVR/Tests/RivalCensusTests.cpp`
- `apps/vr/data/vr/player-profiles.json`

**Acceptance (write as failing tests first):**
1. PlayerProfile 'oracle' (0 reaction delay, normal clips only, 72 Hz, 0 latency) reproduces today's scripted bout byte for byte: the FullSeated chain log and the Wave1Seated / FireSeated end StateHash are unchanged
2. A profile's draws come from a seed derived from the cell key 'pop|<profile>|<scenario>|<seed>' (per-cell seed derivation), never from the kernel RNG: State.RandomLog of a bout is identical between profile 'oracle' and profile 'novice' up to the first player input that differs
3. Profile 'novice' plays sloppy and slow clip variants at the configured mix through Hands->PlayQuickAction (no kernel shortcut): the chain log shows 'gesture sigil' lines whose clip variant matches the mix within +-5 pp over 200 casts
4. HandStreamModel held30 / extrap30 / held25 and injected latency are the code HeldSampleTests now calls; all 11 HeldSample tests stay green with KnownHeldMisses and the other ratchets unchanged
5. Reaction delay: with delay d, the first PlayAction answering a telegraph comes no earlier than telegraph.StartTick + d (checked over a creatures bout)
6. MageArenaDesign.Census.Population writes runs/POP/population.csv and summary.json with one row per (profile, scenario, seed), plus a proxy declaration block (driver policy, input path, coverage, seeds, 'human sample: none') derived from the profile file, and asserts structure only (row count, finite numbers, every cell present)
7. Zero is not a pass: a profile file with an empty band list fails the census test instead of writing an empty summary

**Evidence:**

```
Session/ArenaSession.cpp:659 `Hands->PlayQuickAction(Action, EClipVariant::Normal);` (PlayAction, the script's only gesture entry); grep -c EClipVariant::Normal ArenaSession.cpp = 10, Slow|Sloppy in ArenaSession.cpp/SessionFlow.cpp/DayFlow.cpp = 0. ArenaSession.cpp:1361-1363 blink windows from exact kernel ETAs; :897, :940 ScanThreats reads Games.State.Telegraphs/Projectiles directly; no reaction delay or RNG in DecideScript (grep Reaction|Delay|Rand: none). ArenaSession.h:19-29 FSeatedPolicy has 8 fields, all thresholds, none for skill. CensusTests.cpp:561-580 ExtraPolicies = eager-split, no-split, no-plant; :729-733 seeds 1..20 feed Session.Start only. RivalCensusTests.cpp:293-297 targets keyed on opponent Competence; :431 scenario string 'seated reference script' is the only proxy declaration. SessionTests.cpp:668-669 'the script is deterministic, so a retry of bouts 1-3 loses again'. SigilTests.cpp:509-514 floors (sloppy 0.90, mouse 0.90). HeldSampleTests.cpp:31-41 GCameraHz 30 / 25 and phases, :175-199 stream models, all test-local. WardTests.cpp:635 latencies 0-100 ms injected only into a detector, never a bout. V2-ROADMAP status: 'DF-008 Lio loses every duel in ~20-24 s'.
```

## M10 - Replace the three hand-coded spell pickers with one data-driven rival brain

Context `spell-schools-and-ai` · spine: data-driven opponent decision layer · XL · impact 8 · effort 7 · risk 6 · gate direction

### Summary
The opponent mage is three hand-written spell pickers, one per school, that rank spells by raw damage and bolt special beats on as code. Every duel-pacing finding the owner is deciding (DF-006, DF-008) is an AI-behaviour problem that can only be answered by editing C++. Move the overlay path onto one utility-scored rival brain whose priorities, beats and defence habits are rows in `combat.vr.json`, and make it emit a decision trace the census reads.

### Description
Today `ChooseFireSpell` (MageAI.cpp:282-365), `ChooseAirSpell` (:396-498) and the water `ChooseSpell` (:500-593) each copy the competence curve (level < 2 draws at random, :333-338, :472-476, :549-554) and each value a row by `Damage` or `Damage * Count` (:350, :489, :578). Nothing scores what the hit is worth against the target's live defence, so Lio throws Gale Darts into a planted-staff dome that cuts them to about 1 damage (DF-008: 53 of 54 casts). Session beats are hard-coded: Sunfall mana banking twice (:293-317 and again in the fire-wall check :743-752), Tempest Lance first (:407), Air Form on any aimed threat (:428). The defence side is split: `React` absorbs magic or rolls (:623-641), the fire wall is fire-only (:697), Air Form sits in the spell picker.

The move (registry: behaviour-model-selection, utility scoring for an agent with many simultaneously relevant options; decision-trace-as-evidence; commitment-and-recovery-windows; balance-validation combat-pacing-and-dramatic-arc):
- `RivalBrain` scores each legal action (cast row, ward, roll, wall, form, hold) with a small fixed set of considerations: expected damage after the target's current reduction, mana kept for a banked signature, threat answered, tempo (time since last cast), phase state. Weights and beats are per rival in the existing `rivals` block (Brennic, Lio, Corvo).
- The competence curve stays the dial on inputs (reaction, absorb, perfect, aim, cadence), unchanged; competence also scales score noise instead of switching between a random draw and a sort.
- The pinned path (null ruleset) keeps today's `ChooseSpell` byte-for-byte; conformance and the T07 RNG vectors are its guard.
- Each decision appends a trace row (perceived, candidates, scores, choice, reason) that `RivalCensusTests` writes beside the CSV, so the owner's DF tables carry the why, not only the outcome.

This is also the seam lane C needs: T29's interrupt tell window is the brain's committed decision made visible, and T30 rallies need a mage-side reflect choice; both become rows, not new branches.

### Flow
1. Extract the shared competence and legality code (`FireOfferable`, `AirOfferable`, water legality) behind one `LegalActions(State, Self)` for all three schools; null path still calls the old pickers.
2. Add `RivalBrain::Decide` with the considerations above and a trace sink; wire it in `MageInput` only when `Rules && Rules->bActive` and the actor is a bound or school mage.
3. Express Sunfall banking, the Tempest opener and Air Form as playbook rows in `combat.vr.json` and delete their overlay-path code; keep the T19 granted cast in `VrRivals.cpp`.
4. Census: run the DF-006 and DF-008 sets with the playbooks, write traces, regenerate both tables for the owner.
5. Owner tunes weights in data; no C++ edit per tuning pass.

### Expected impact
The mechanics gate (18 Oct) requires the phased Brennic duel in the census band; 0 of 10 census rows in DF-006 and DF-008 reach the 45-80 s median. The levers the owner has been offered are sheet numbers (combat data, change request to TV) or hard-coded AI tweaks per card. A data-driven brain turns the duel's pacing into an overlay tuning loop that stays out of the pinned numbers and out of `MageAI.cpp`, gives Brennic and Lio distinct personalities for the slice, and makes every census answer explainable by trace. It is the cheapest route to DF-006 B and DF-008 A at once.

### Evaluation
Claim: one utility-scored, data-driven rival brain lets the Brennic and Lio duels reach the 45-80 s band and Lio's c1.5 win rate fall into 40-60 % by data edits only.
Before: 3 pickers (MageAI.cpp:282-593, about 280 lines) with the competence curve copied 3 times; 0 decision traces; DF-008 live c1: 53 of 54 Lio casts are Gale Dart, 0 blocks, median 23.27 s, win 1.00; DF-006 + DF-008: 0 of 10 set rows inside the 45-80 s median band.
After: prediction only. (1) Lio c1.5 vs the seated script with a planted dome: A picks a dart at random among useful rows (Level 1.5 < 2 takes the random draw, :549 pattern at :472); B scores the dart at about 1 damage after the dome's 0.85 and prefers holding mana for Veering Bolt or Squall or warding, so the duel lengthens and the cast mix shifts. (2) Brennic's first phase break: A casts on the next decision; B holds 3 s on a beat row, about +6 s over two breaks (DF-006 B estimate). (3) Player bolt inbound at Lio: A absorbs with AbsorbChance or forms on any aimed threat; B compares ward, roll and Air Form by mana and family, so blocks rise above 0. Falsifier: on the same seeds, if the median moves less than 2 s and darts stay above 90 % of casts, the bottleneck is the sheet numbers (DF-008 B), not the AI.
Method: simulation (three cases walked under A and B); the deciding instrument is the existing RivalCensusTests run on the DF-006 and DF-008 seed sets with the playbook, plus the new trace.
Result: unmeasurable until the census runs with the brain; instrument named above.
Gate: direction (owner answers DF-008 A and signs off the playbook weights; pinned path and combat numbers untouched).

**Depends on:**
- T19 rival phases (done)
- T23 Air opponent (done)
- DF-008 option A 'AI first' (owner answer)
- ARCHITECTURE-REVIEW F1 rule: overlay enters the kernel as one FVrRuleset method per hook

**Write set:**
- `apps/vr/Game/Source/MageArenaVR/Kernel/RivalBrain.h`
- `apps/vr/Game/Source/MageArenaVR/Kernel/RivalBrain.cpp`
- `apps/vr/Game/Source/MageArenaVR/Kernel/MageAI.cpp`
- `apps/vr/Game/Source/MageArenaVR/Kernel/MageAI.h`
- `apps/vr/Game/Source/MageArenaVR/Kernel/VrRules.h`
- `apps/vr/Game/Source/MageArenaVR/Kernel/VrRivals.cpp`
- `apps/vr/Game/Source/MageArenaVR/Kernel/VrRivalsLoad.cpp`
- `apps/vr/data/vr/combat.vr.json`
- `apps/vr/Game/Source/MageArenaVR/Tests/RivalBrainTests.cpp`
- `apps/vr/Game/Source/MageArenaVR/Tests/RivalCensusTests.cpp`

**Acceptance (write as failing tests first):**
1. With a null ruleset MageInput is unchanged: the 8 RNG conformance vectors, MageArena.Fire.Golden and the Air goldens keep their hashes (the brain runs only on the overlay path).
2. A playbook row in combat.vr.json (no C++ edit) that holds a rival's casts for 3 s after a phase break gives 0 rival 'cast' events in the 180 ticks after each break (DF-006 option B expressed as data).
3. MageArena.Fire.SunfallBeat stays green when Sunfall banking (MageAI.cpp:293-317, :743-752) and the Tempest priority (:407) are playbook rows instead of code.
4. Every overlay decision emits a trace record (perceived threats, candidate rows, score per row, chosen row, reason); replaying one census seed's trace reproduces the chosen-slot sequence exactly.
5. Candidate scoring uses expected damage after the target's live reduction (ward, planted-staff dome): against a planted dome, the Lio playbook picks Gale Dart in fewer than 50 % of cast decisions once tier II is open (DF-008 live: 53 of 54 casts were darts).
6. Lio blocks at least one player bolt per duel on median in the DF-008 census sets (today 0 blocks).
7. The census writes a per-set row (median, win rate, signature rate, cast mix) for Brennic and Lio from the playbook, regenerating the DF-006 and DF-008 tables from one command.

**Evidence:**

```
apps/vr/Game/Source/MageArenaVR/Kernel/MageAI.cpp:282-365 ChooseFireSpell, :396-498 ChooseAirSpell, :500-593 ChooseSpell (water); competence curve copied at :333-338, :472-476, :549-554 (`if (Profile.Level < 2.0)` random draw); valuation `Left.Spell->Damage` :350, `L.Damage * Count` :489, `Left.Spell->Damage * Count` :578; Sunfall banking :293-317 and again in ConsiderFireWall :743-752; Tempest priority :407; Air Form on any aimed threat :414-433; React absorb-or-roll only :623-641; fire wall fire-only :697. docs/design-findings/DF-008-lio-too-weak.md table: live c1 median 23.27 s win 1.00, live c1.5 24.09 s, proposal 20.92 / 21.17 s; '53 of Lio's 54 casts in the live duel are Gale Darts ... Lio never wards the player's bolts (0 blocks)'. docs/design-findings/DF-006-phases-shorten-the-duel.md: six rows, medians 24.43-44.83 s, all below 45. docs/design/V2-ROADMAP.md gate table: 18 Oct 'the phased Brennic duel in the census targets band'; lane C T29/T30 rows. Not proposed elsewhere: BACKLOG.md, scan-sweep.jsonl, ab.jsonl and V2-ROADMAP rows contain no AI-architecture card (DF-008 A is an option, not a card). Registry: systems-canon/agent-behaviour-authoring/techniques/behaviour-model-selection.md (utility scoring), decision-trace-as-evidence.md.
```

## M11 - Lift the Tiro day off FGames.Phase into a typed, data-authored day graph

Context `duel-session` · spine: data-driven session flow · XL · impact 8 · effort 7 · risk 5 · gate architecture

### Summary
Replace the eleven string phases the session writes into the ported `FGames.Phase` with a typed session phase, run by a day graph loaded from `day.json` v2. Each phase row carries its prompt key, its stones, its scripted cue, and its exits. An exit is a trigger from a closed vocabulary (palm, both-palms-hold, stone-hold:N, timeout, bout-won, bout-lost, saved-bout) that names the next phase. FGames.Phase goes back to the kernel's own four values.

### Description
`FGames` is the games.ts port (Kernel/Games.h:6). Its kernel writes only four phases: active, lost, intermission and complete (Games.cpp:232, 300, 337, 373). The session writes eight more VR-only names into the same field: cold, offer, teach, prologue, ritual, intro, aftermath and closed. The flow is then rebuilt from string tests spread across every function that has to know the phase. There are 99 such tests and assignments in Session/*.cpp: SessionFlow 28, DayFlow 22, ArenaSession 14, CollarCapture 9, CreaturesCapture 7, SessionPresentation 5, and others. Tests/ has 59 more. The graph itself exists only as a prose comment (ArenaSession.h:145-156), so the law and its check are two sources (law L4). This is the session-side twin of architecture review F1, where the overlay is written into the kernel port.

The roadmap keeps adding phases. T25 needs a spare-or-finish choice at missio, T28 needs intermission scenes on the stones, and spring 2027 brings the Veteranus, Primus and Summa days, exhibition bouts and the Breaking bout. Each of these is a new string threaded by hand through IsArcPhase, AdvanceArc, AdvanceDay, GetPromptText, HandleWardRaised, NotifyQuit, SkipTeach, SyncComfortVisuals and the capture drivers. The v2 slice alone grew ArenaSession.cpp by +414/-62 and the header by +178 (architecture review 6.4).

The card:
1. **`ESessionPhase` or `FName` session state** owned by `FDayRunner`. FGames.Phase holds only kernel phases, and the runner maps kernel transitions (bout won, lost, complete) to graph triggers.
2. **`day.json` v2.** It adds `phases[]` with id, prompt key (checked against strings.json at load), stones (labels per index), scripted cue (the clip the reference script plays), clock (seconds, converted to ticks), on-enter actions from a closed list (enter-teach, start-bout, record-bout, write-save, clear-save), and `exits[]` (trigger -> phase). The v1 timings move in unchanged.
3. **A validator that fails closed.** It rejects an unknown phase, an unreachable phase, a dead end (other than closed), an unknown trigger or action, and a missing string key. Each error names the phase.
4. **Triggers come only from the gesture pipeline or the kernel.** No key shortcut can satisfy a trigger.

Technique: `gameplay-runtime-patterns/data-driven-type-objects-over-subclass-growth` (a closed vocabulary of named behaviours selected by a data field, the cheapest shape for conduct). The few-fixed-beats-many-flexible beat discipline from `story-structure` shapes the phase rows. Law L14 holds because the climactic beats (ritual, final, aftermath) stay fixed rows that no picker places. The format is channel-neutral by design: DECISIONS 2026-10-02 says "a mechanic solved in one channel must be reusable by the others", and TV owns story and quests (DECISIONS 2026-10-07). The graph can go to TV as a proposal later. It is not a combat number.

### Flow
day.json v2 -> loader and validator -> FDayGraph -> FDayRunner (current phase, tick clock). Gesture events (palm, both palms, stone hold) and kernel transitions become triggers; the runner walks exits[] and runs on-enter actions. Prompt, stones and the scripted cue are read from the phase row. Presentation and the captures read the typed phase.

### Expected impact
- T25, T28 and later days become mostly data: a phase row plus strings, and one new trigger kind only when the gesture is new.
- The session stops writing VR state into the ported kernel struct, so conformance and the TV contract see only TV phases.
- Two workers can add phases without colliding in ArenaSession.cpp, the file the review ranks hotspot #2 (it is excluded pending T16).
- The day flow becomes one inspectable artifact (a graph the owner can read) instead of 99 scattered string tests.

### Evaluation
Claim: A typed, data-authored day graph lets new session phases (T25, T28, spring days) land as data and stops VR phases leaking into the games.ts struct, while today's day plays out identically.
Before: FGames.Phase holds 11 names, 8 of them VR-only. Session/*.cpp has 99 string phase tests and assignments, and Tests/ has 59. The phase graph exists only as prose (ArenaSession.h:145-156). day.json carries 5 timings (DayFlow.cpp:64-70). The v2 slice grew ArenaSession.cpp by +414/-62 and ArenaSession.h by +178.
After: simulation, three cases under A (today) and B (the graph). (a) T28, an intermission scene. A: edits the intermission block of AdvanceArc (SessionFlow.cpp:1381-1466), IsArcPhase (DayFlow.cpp:33-37), GetPromptText (SessionFlow.cpp:593-679), HandleWardRaised (ArenaSession.cpp:785-800), NotifyQuit (SessionFlow.cpp:436-446), SyncComfortVisuals (SessionPresentation.cpp:1045-1050) and a capture driver. B: a "scene" row (prompt key, stones, exits stone-hold:0/2 -> intro and timeout 20 s -> intro) plus strings, with no C++. (b) T25, spare or finish at missio. A: a new string threaded through about 6 functions. B: a row plus one new trigger kind (fist-hold) in the vocabulary. (c) The spring Veteranus day. A: copy or branch DayFlow.cpp (614 lines). B: a new day file reusing the triggers. Falsifier: the claim fails if T28 built on the graph still needs C++ edits beyond the trigger vocabulary and presentation, or if FullSeated's phase-transition chain lines differ from today's.
Method: simulation. The instrument that decides it is acceptance case 2 (the equivalence golden on FullSeated) plus a count of C++ files T28 touches when it lands on the graph.
Result: unmeasured. The equivalence golden and T28's diffstat decide it.
Gate: architecture. If TV adopts the graph format for story days, that is a separate change request (contract).

**Depends on:**
- ARCH-REVIEW F2 (split FArenaSession) and N1 (capture driver base): the runner lands in the split-out day unit
- T16 (open; holds ArenaSession.cpp/.h and SessionPresentation.cpp)
- C2 story bible (prompt keys in strings.json)

**Write set:**
- `apps/vr/data/vr/day.json`
- `apps/vr/Game/Source/MageArenaVR/Session/DayGraph.h`
- `apps/vr/Game/Source/MageArenaVR/Session/DayGraph.cpp`
- `apps/vr/Game/Source/MageArenaVR/Session/SessionPhase.h`
- `apps/vr/Game/Source/MageArenaVR/Session/ArenaSession.h`
- `apps/vr/Game/Source/MageArenaVR/Session/ArenaSession.cpp`
- `apps/vr/Game/Source/MageArenaVR/Session/SessionFlow.cpp`
- `apps/vr/Game/Source/MageArenaVR/Session/DayFlow.cpp`
- `apps/vr/Game/Source/MageArenaVR/Session/SessionPresentation.cpp`
- `apps/vr/Game/Source/MageArenaVR/Session/TeachCapture.cpp`
- `apps/vr/Game/Source/MageArenaVR/Session/SettingsCapture.cpp`
- `apps/vr/Game/Source/MageArenaVR/Session/CreaturesCapture.cpp`
- `apps/vr/Game/Source/MageArenaVR/Tests/DayGraphTests.cpp`
- `apps/vr/Game/Source/MageArenaVR/Tests/DayTests.cpp`

**Acceptance (write as failing tests first):**
1. MageArena.DayGraph.Validate: day.json v2 loads, and the validator rejects each of: an exit to an unknown phase, an unreachable phase, a non-terminal dead end, an unknown trigger or on-enter action, and a prompt key missing from strings.json. Each error names the phase id and the load fails closed.
2. MageArena.DayGraph.Equivalence: the FullSeated reference day on the graph produces the same ordered phase-transition chain lines ('teach cold', 'day prologue ...', 'teach complete', intro, 'Session victory', 'day closed') and the same final StateHash as the pre-graph tree recorded in a golden.
3. MageArena.DayGraph.KernelPhaseClean: stepping a full day, FGames.Phase holds only active, intermission, lost or complete on every frame. The session phase is a separate typed field.
4. MageArena.DayGraph.DataOnlyPhase: a test day.json that adds a 'scene' phase between intermission and intro (prompt key, stone labels, exits stone-hold:0 -> intro, stone-hold:2 -> intro, timeout 20 s -> intro) runs with no C++ change, and the scripted cue from the row reaches it.
5. GetPromptText, IsArcPhase and the phase routing in HandleWardRaised read the phase row: grep "Phase == TEXT(" over Session/SessionFlow.cpp and Session/DayFlow.cpp returns 0.
6. MageArena.DayGraph.TriggerVocabulary: every trigger kind maps to a gesture-pipeline event or a kernel transition. A test asserts that no trigger can be satisfied by a key shortcut or a direct setter.
7. The v1 timings (prologueMaxS, ritualLineS, ritualTimeoutS, introS, aftermathTabletS) keep their values in v2, and the T21 DayTests stay green unchanged.

**Evidence:**

```
apps/vr/Game/Source/MageArenaVR/Kernel/Games.h:6 '// games.ts.' and :32 'FString Phase;'. The kernel writes only active, lost, intermission and complete (Games.cpp:232, :300, :337, :373). | The session writes 8 VR-only names into it: SessionFlow.cpp:405 offer, :411 cold, :961 teach; DayFlow.cpp:245 prologue, :278 ritual, :310 intro, :374 aftermath, :394 closed. | grep -c 'Phase == TEXT|Phase != TEXT|GetStage() == TEXT|GetStage() != TEXT|Phase = TEXT' gives 99 hits over Session/*.cpp (SessionFlow 28, DayFlow 22, ArenaSession 14, CollarCapture 9, CreaturesCapture 7, SessionPresentation 5, DayCapture 4, SettingsCapture 3, PresetCapture 2, five files with 1) and 59 over Tests/*.cpp. | The phase graph is prose only: apps/vr/Game/Source/MageArenaVR/Session/ArenaSession.h:145-156. | Phase routing is spread out: DayFlow.cpp:33-37 IsArcPhase (10 names); SessionFlow.cpp:593-679 GetPromptText (13 phase branches); ArenaSession.cpp:785-800 HandleWardRaised; SessionFlow.cpp:436-446 NotifyQuit; :417-424 SkipTeach; :1321-1488 AdvanceArc; DayFlow.cpp:512-614 AdvanceDay; SessionPresentation.cpp:1045-1050 and :1222. | day.json v1 holds only 5 timings (DayFlow.cpp:64-70). | docs/research/ARCHITECTURE-REVIEW-2026-10.md:553-554: the slice diff is ArenaSession.cpp +414/-62 and ArenaSession.h +178; 6.2 ranks ArenaSession.cpp hotspot #2 (excluded pending T16). | docs/design/V2-ROADMAP.md: T25 (spare or finish at missio, writes the arena flag), T28 (intermission scenes on the stones, 20 s, never blocking), and spring 2027 Veteranus, Primus and Summa days, exhibition bouts and the Breaking bout. | docs/DECISIONS.md 2026-10-02 ('A mechanic solved in one channel must be reusable by the others') and 2026-10-07 (TV owns canon including story and quests).
```

## M12 - Enrol each player's own sigils in the teach and calibrate reject per player

Context `sigil-recognition` · spine: per-user gesture template adaptation · L · impact 8 · effort 5 · risk 4 · gate direction

### Summary
Let the teach collect three of the player's own draws per spell line, through the normal stroke pipeline. The draws are checked before they are kept, added as a separate user layer of $Q templates, and given a reject distance calibrated on that layer, and they are saved per casting hand. On desktop the owner's mouse draws enter the same way, so the recognizer gets its first human data in October rather than at V2.

### Description
Every template and every test draw in the tree comes from one generator. All 36 templates in `Clips/templates/` are `"source":"synthetic"`. The corpus is from `generate.mjs --corpus`, and the "mouse" clips are `buildVariedClip(..., 'mouse')` from the same `corpus.mjs` (`mousepaths.mjs:5,22`). So the accuracy and reject numbers are in-distribution. The plan assumed something else: "Templates are the owner's draws (5 per class) ... mouse draws in October" (`PROJECT-PLAN.md:351`), and the D-G2 gate reads "on the owner's held-out mouse draws ... >= 85 % for a second person" (`:216`). Risk R2, strangers' accuracy, is rated High impact (`:545`), and judges are strangers. The reject cut is one global constant, 26000, tuned on that synthetic corpus with a 2405 margin over the worst correct match (`QPointCloudRecognizer.h:32-39`). A person whose circle is oval, or whose bar leans 20 degrees, has no way to move it. The templates are also extracted in StyleA only (`SigilRecognizerSubsystem.cpp:77-78`), while StyleB strokes include the reach-in, which is the other half of the A/B choice V2 has to make.

$Q was designed for user-dependent templates, and adding one is cheap (`AddTemplate`; Classify bounds by a lower-bound sort). The layer stays small (at most 5 per class), and it is separate from the base, which keeps its role as the floor. What is missing is the discipline around it, and that is the core of this card. A draw is accepted only if (a) the stroke builder emitted a Gesture for it (no bypass: the enrolment consumes the same points `Dispatch` would classify), (b) it is not closer to another class than to the prompted one, either in the base set or among the user's own draws, and (c) it lies inside a ceiling relative to the base reject. Each class then gets a reject distance from the spread of its user draws, clamped between a floor and the global 26000, so a tight user set cannot widen acceptance for impostors. The teach hook is one step ("draw each line three times"), and that is a design call for the owner, hence the direction gate. The registry technique `time-to-competence-measurement` sets how the teach reports it: report the held-out accuracy after enrolment, not "enrolment completed".

### Flow
1. `userstyle.mjs` writes a gitignored skewed-user corpus (per-user affine bias plus overshoot, a new seed band). The test records base-only top-1 on it as Before.
2. Recognizer: an origin tag per template, `ClearUser()`, and a per-class reject table that defaults to the global cut (an empty layer is bit-identical to today).
3. `FSigilEnrollment`: the acceptance checks (builder Gesture, class separation, ceiling) with typed refusal reasons, and calibration of the per-class reject.
4. Subsystem: `BeginEnrol(Line)` / `EndEnrol()`. While enrolling, `Dispatch` routes the Gesture points to enrolment instead of `OnSigilCast`. Save and load per casting hand under `Saved/`, beside the collar ledger's lifetime file.
5. Teach hook in `SessionFlow.cpp`: three draws per line, after the existing teach of line 1, skippable. The mouse path (`IngestMouse`) enrols the same way on desktop.
6. The tests in the acceptance list, then the owner draws 3 x 3 with the mouse: the first human numbers for the D-G2 line.

### Expected impact
It addresses R2 and the V2 cross-user gate (>= 85 %) in the one way a fixed template set cannot: the second person and the judges get templates shaped like their own hands. It also turns the October owner-mouse gate from synthetic into human data, and gives V2 a tool for the StyleA/StyleB decision (enrol in each style, compare). Against the kill criterion at `PROJECT-PLAN.md:93` ("recognizer below 85 % after re-recording ... held hand-signs"), it is a cheap lever that lands before the fallback that "kills the differentiator" (R3 at `:216`).

### Evaluation
Claim: three enrolled draws per class lift a skewed user's held-out top-1 to >= 95 % and leave impostor rejection and noise false casts at their floors.
Before: human draws in the recognizer's inputs = 0 (36/36 templates synthetic, the corpus and the 90 mouse clips from the same generator); reject = 1 global distance (26000); template style = StyleA only; user-template API = none (`LoadTemplatesFromDirectory` begins with `Recognizer.Reset()`); per-user persistence = none. The skewed-user base-only top-1 has not been measured; it is step 1.
After: predicted 45 templates (36 + 9), held-out top-1 >= 95 % on the skewed user, impostor reject >= 90 % per class, 0 noise casts, classify p95 <= 0.5 ms.
Method: simulation, three cases. Case 1, a user whose bar leans 20 degrees toward the next class's diagonal. A: the nearest base template is the other line, or the distance passes 26000, so the draw is misread or rejected. B: the user's own leaning templates sit closest, and their class wins. Case 2, an impostor (open arc) after enrolment. A: rejected (49796 or more against the 26000 cut). B: the per-class cut is at most 26000, and user templates are circle-plus-bar shapes, so the nearest distance does not shrink much; it is still rejected, and the NoFalseCasts test decides it. Case 3, a poisoned enrolment (a line2 draw offered as line1). A: no enrolment exists. B: the separation check finds it closer to line2 and refuses it, so the layer is unchanged. What would falsify it: the skewed-user Before is already >= 95 % (then $Q's normalisation absorbs the bias and the card is not needed), or enrolment lifts accuracy only by widening the cut enough to let impostors through.
Result: unmeasurable until the instrument exists. The instrument is `Sigils.Enroll.SkewedUser` with `NoFalseCasts` on the enrolled set, steps 1 and 3 of this card. On real hands it is the pof P4 harness at V2.
Gate: direction. Adding an enrolment step to the teach changes the player's first minutes, which is the owner's call. The recognizer layer alone (steps 1-4) is gate none and can land first.

**Depends on:**
- T13 teach flow exists (hook point only)
- pof P4 gesture eval harness and P3 hand-trace recorder (PROJECT-PLAN section 8) measure it on real hands in V2; this card does not re-propose them
- independent of the arbiter card; if both land, enrolment draws come through the arbiter's Gesture intent

**Write set:**
- `apps/vr/Game/Source/MageArenaVR/Gestures/QPointCloudRecognizer.h`
- `apps/vr/Game/Source/MageArenaVR/Gestures/QPointCloudRecognizer.cpp`
- `apps/vr/Game/Source/MageArenaVR/Gestures/SigilEnrollment.h`
- `apps/vr/Game/Source/MageArenaVR/Gestures/SigilEnrollment.cpp`
- `apps/vr/Game/Source/MageArenaVR/Gestures/SigilRecognizerSubsystem.h`
- `apps/vr/Game/Source/MageArenaVR/Gestures/SigilRecognizerSubsystem.cpp`
- `apps/vr/Game/Source/MageArenaVR/Session/SessionFlow.cpp`
- `apps/vr/Game/Source/MageArenaVR/Tests/SigilEnrollmentTests.cpp`
- `apps/vr/tools/clipgen/userstyle.mjs`

**Acceptance (write as failing tests first):**
1. Sigils.Enroll.Layer: after 3 accepted enrolments per class the recognizer holds 36 base + 9 user templates; ClearUser() restores classification on the 270-clip corpus bit-identically (same labels, same distances) to a recognizer that never enrolled
2. Sigils.Enroll.SkewedUser: on a gitignored skewed-user corpus (userstyle.mjs: a fixed per-user rotation, aspect ratio, bar-angle bias and overshoot, 30 held-out draws per class, seeds outside the template, corpus and mouse bands) base-only top-1 is recorded as Before; after enrolling 3 of that user's draws per class, top-1 on the held-out 90 is >= 95 % and >= Before + 10 points
3. Sigils.Enroll.NoFalseCasts: with any enrolled set from the SkewedUser case, every impostor class still rejects >= 90 % and the 5 noise clips give 0 casts (the existing NoFalseCasts floors)
4. Sigils.Enroll.Poison: a line2 draw offered as line1, a bare circle and an impostor offered for any class are each refused with a typed reason, and the user layer is unchanged
5. Sigils.Enroll.Persist: save, then a new game instance loads, round-trips the user layer byte-identically; a left-hand casting setting keeps its own set (switching hands does not apply the right-hand set)
6. Sigils.Enroll.StyleB: draws enrolled in StyleB raise StyleB top-1 on the skewed user to at least the StyleA figure (templates are StyleA-only today)
7. Classify p95 with 36 + 15 templates stays <= 0.5 ms (the D-G2 desktop proxy line), and Sigils.Accuracy, NoFalseCasts and HeldSample.SigilCorpus are unchanged with an empty user layer

**Evidence:**

```
for f in apps/vr/Game/Clips/templates/*.jsonl; do head -c 2000 $f | grep -o '"source":"[a-z]*"'; done | sort | uniq -c -> 36 "source":"synthetic" (12 per line; 24 normal, 12 sloppy)
apps/vr/tools/clipgen/mousepaths.mjs:1-5,22 "Mouse-drawn sigil clips" are `buildVariedClip(action, stroke, 'normal', seed, 'mouse', true, 'mouse')` from corpus.mjs - generated, not drawn
docs/PROJECT-PLAN.md:351 "Templates are the owner's draws (5 per class), stored as data ... mouse draws in October, re-recorded from real hands in V2" - not what the tree holds
docs/PROJECT-PLAN.md:216 D-G2: ">=95% top-1 on the owner's held-out mouse draws (3 x 30, templates drawn separately); >=85% for a second person"; :545 R2 "Recognizer accuracy for strangers ... Medium / High"; :93 kill criterion at <85% on real hands
apps/vr/Game/Source/MageArenaVR/Gestures/QPointCloudRecognizer.h:32-39 one global DefaultRejectDistance = 26000, "2405 above the worst correct match" on the synthetic corpus; QPointCloudRecognizer.cpp:424 `Result.bReject = Best > RejectDistance;`
apps/vr/Game/Source/MageArenaVR/Gestures/SigilRecognizerSubsystem.cpp:37 `Recognizer.Reset()` at the top of LoadTemplatesFromDirectory - no layer survives a reload; :77-78 templates extracted with `ESigilDrawStyle::StyleA` only ("Templates are authored with pinch as the pen")
apps/vr/Game/Source/MageArenaVR/Gestures/SigilRecognizerSubsystem.cpp:15 templates load from Clips/templates at Initialize; 10 test files reload the same directory (grep LoadTemplatesFromDirectory Tests/: CensusTests.cpp:701, CollarTests.cpp:316, DayTests.cpp:54, ...)
apps/vr/Game/Source/MageArenaVR/Tests/SigilTests.cpp (Accuracy, lines 335-530): buckets synthetic and "mouse" only; floors normal/slow >= 0.95, sloppy >= 0.90, mouse >= 0.90, impostor reject >= 0.90
Persistence precedent: Session/CollarLedger.cpp:311-323 `SaveLifetime` writes a file across days; no equivalent for recognizer data
grep -rn -i "enrol|user template|personal template|per-player" docs apps/vr/tasks -> 0 hits (not planned); pof P3/P4 (PROJECT-PLAN.md:403,406) record and evaluate corpora but never adapt templates
```

## Incidental S/M defects (untriaged, for the stabilize loop)

Unverified one-liners the scouts noticed on the way. Each needs its premise checked before it is fixed.

### arena-greybox

- apps/vr/Game/Source/MageArenaVR/Greybox/ArenaGreyboxActor.cpp:108-111 - DestroyStockStage destroys every AStaticMeshActor in any game world; the first authored map (A07 kit, art pass) loses its meshes. Limit it to the Entry map's grid floor.
- apps/vr/Game/Source/MageArenaVR/Greybox/ThreatDemo.cpp:286,348,404 - flight progress and IsShowcaseMoment use the FPlatformTime wall clock, so a pause, hitch or time dilation desyncs the demo and the capture's showcase timing depends on the machine.
- apps/vr/Game/Source/MageArenaVR/Greybox/ThreatDemo.cpp:301,312,322 - each Launch creates new MIDs (Greybox::Tint, outer Rig) while ClearFlights destroys only the components; MIDs pile up per console Launch until the rig dies.
- apps/vr/Game/Source/MageArenaVR/Greybox/HandPresentationComponent.cpp:193 - FParse::Param(FCommandLine::Get(), "nullrhi") scans the command line every tick; BeginPlay already disables the tick under -nullrhi, so cache the flag.
- docs/gameplay/09-pause-comfort-accessibility.md:140 - lists Magic: fire as 'vermilion'; T26 moved it to ember orange (arena-layout.json threats.order[fire], ElementColour.cpp:42-45).

### build-config

- apps/vr/Game/Source/MageArenaVR/Session/AirCapture.cpp:101 (+CollarCapture.cpp:77, ColourAudioCapture.cpp:113, DayCapture.cpp:76, PresetCapture.cpp:109) - 740f5e8's clang -Wunreachable-code-loop-increment pattern is back; an Android compile errors here (fix as in 740f5e8)
- apps/vr/Game/Source/MageArenaVR/Session/ArenaSession.cpp:2229 - TActorIterator loop with an unconditional break, likely the same clang error; use `TActorIterator<...> It(W); Pawn = It ? *It : nullptr;`
- apps/vr/tasks/*.md - 26 acceptance lines in 17 cards use the old root C:\Users\kazda\kiro\mage-arena-vr (T02:86 and T03:78 also miss apps\vr); not runnable as written in this checkout
- apps/vr/tools/package-android.ps1:36 - every APK run logs to runs\T05 and deletes the previous package-android.log (:58), overwriting the T05 evidence; take a run id like the capture scripts
- apps/vr/tools/*.ps1 - the engine root 'C:\Program Files\Epic Games\UE_5.8' is hard-coded in 15 scripts with no UE_ROOT override (a 5.8.4 side-by-side install per STACK-OPPORTUNITIES F12 needs 15 edits)

### combat-sim-core

- apps/vr/Game/Source/MageArenaVR/Kernel/KernelData.cpp:1197-1198 - arena_metres read with AsNumber() with no number/finite/positive check; 0 or a string gives ArenaWidthM 0, and Geometry.cpp:9-16 Axes then divides by min(Rx,Ry)=0 instead of failing the load with a name
- apps/vr/Game/Source/MageArenaVR/Kernel/KernelData.cpp:1115-1118 - lines.branchAtTiers entries go through Value->AsNumber() without an IsValid or type check; a non-number entry silently becomes branch tier 0
- apps/vr/Game/Source/MageArenaVR/Tests/CensusTests.cpp:486,771 and Tests/RivalCensusTests.cpp:428,471 - the census proposal and summary JSON record seeds but not KernelData().PinHash or the unhashed spells-air.csv, so a sign-off is not bound to the data it judged (registry L6)
- apps/vr/Game/Source/MageArenaVR/Kernel/Enemies.cpp:15-36 - Extract builds a Pattern+Text FString key and takes a global FCriticalSection on every call (per enemy per tick); parse the magnitudes once at load into FEnemySpec
- apps/vr/Game/Source/MageArenaVR/Kernel/ArenaKernel.cpp:369-380 - every RNG draw stores a Printf-built purpose FString (MageAI.cpp:335,474,551,625,628,875; Air.cpp:599) in RandomLog, which is never trimmed in a bout, and StateHash.cpp:490 rehashes the whole log on each call (per-tick hashing in PauseTests grows with the number of draws)

### duel-session

- apps/vr/Game/Source/MageArenaVR/Session/ArenaSession.h:587 - AirCapture is a TObjectPtr with no UPROPERTY, the only one of the 9 capture drivers without it, so GC does not see the reference and can collect the driver in the middle of a -MageArenaAirCapture run.
- apps/vr/Game/Source/MageArenaVR/Session/DayFlow.cpp:71 - the loop 'const TPair<FString, TSharedPtr<FJsonValue>>& Pair : Root->Values' binds a converted temporary. SessionFlow.cpp:145-146 records this as a clang error on Android (-Wrange-loop-construct). 11 such loops remain: KernelData.cpp:612,704,725; VrRulesLoad.cpp:357,401; ArenaAudio.cpp:158,184; ArenaFlags.cpp:125; CollarLedger.cpp:42,103; DayFlow.cpp:71. This is an APK build risk; it was not compiled here.
- apps/vr/Game/Source/MageArenaVR/Session/SessionPresentation.cpp:1294 - shot ghosts (0.28 s), hit flashes (0.16 or 1.2 s) and the phase flare time out on FPlatformTime::Seconds() while SetFrameHold freezes the kernel for a still, so a capture can lose the flash it waited for. Keying them to sim seconds fixes it.
- apps/vr/Game/Source/MageArenaVR/Session/TeachCapture.cpp:172,198,249,288,314 and SettingsCapture.cpp:183 - the stills wait for English prompt text ('Raise your palm to begin.', 'Raise as it arrives.', ...) instead of the strings.json key, so a C2 copy edit to apps/vr/data/vr/strings.json:2-8 fails the T13 and T14 captures by timeout.
- context-map.json (duel-session) - lists Session/StylePickCapture.cpp/.h, which do not exist, and leaves out DayFlow.cpp, ArenaFlags, CollarLedger, PlayerPreset, ElementColour, ArenaAudio, SessionCue.h and 5 newer capture drivers. Session/ holds 35 files and the map lists 15.

### game-tests

- apps/vr/Game/Source/MageArenaVR/Tests/CensusTests.cpp:816 (with :114-118, :501) - MageArenaDesign.Census.Sweep at its default 20 seeds overwrites the tracked apps/vr/data/vr/calibration-proposal.json (git ls-files lists it), so one run of the design suite dirties the tree; write to runs/T12/census and copy by hand, or gate the write on an explicit flag
- apps/vr/Game/Source/MageArenaVR/Tests/SessionTests.cpp:546, :636, :808 - `if (!Session.Start(1)) return false;` returns without Rig.Close, leaving a rooted UGameInstance (AddToRoot at :37) alive for the rest of the run
- apps/vr/Game/Source/MageArenaVR/Tests/SessionTests.cpp:836 - death-burst resolve delay hard-coded `0.6 * 60.0` ('enemies.json death burst delay') instead of read from KernelData; a re-pin that moves the delay silently desynchronises this test's perfect window
- apps/vr/Game/Source/MageArenaVR/Tests/WardTests.cpp:76-80, :132-137 - FWardReport is a process-wide static that every Ward test mutates and rewrites Saved/Reports/ward-timing.json from its destructor, so the report's contents depend on which subset of tests ran (a filtered run writes nulls for the rest)
- context-map.json (game-tests file_paths) and Tests/StateHashCoverageTests.cpp:8 name Tests/ReplayTests.cpp, which is not in the tree (no git history); the map also omits 17 of the 35 test files (Air, Audio, BlinkVignette, BudgetStress, CaptureFlag, Collar, Colour, Creature, Day, EnemyStepCost, HeldSample, KernelLoad, Preset, Rival, RivalCensus, StateHashCoverage, SystemGesture) - rescan

### hand-input-pipeline

- apps/vr/Game/Source/MageArenaVR/Session/SessionFlow.cpp:501 - IsBothPalmsClip() is declared (ArenaSession.h:369) and defined but has no caller; dead code.
- apps/vr/Game/Source/MageArenaVR/Hands/HandInputSubsystem.cpp:52-63 - PlayQuickAction re-reads and re-parses the clip JSONL on every key press (sigil-line2.slow.jsonl is 446 KB); no cache, a likely hitch on Quest.
- apps/vr/Game/Source/MageArenaVR/Hands/MageArenaPawn.cpp:166,232,423 - the blink fade and vignette run on FPlatformTime::Seconds (wall clock), so pause, time dilation and headless Advance(dt) stepping do not drive them deterministically.
- apps/vr/Game/Source/MageArenaVR/Hands/HandClipPlayer.cpp:121-183 - Step has no upper bound on dt: one long frame jumps across a whole flick and emits a single sample, so a hitch can drop a blink or bolt (card 1 fixes it structurally; a clamp is the S fix).
- apps/vr/Game/Source/MageArenaVR/Hands/MageArenaPawn.cpp:152-161 - Tick calls FMageSettings::EnsureLoaded and Camera->SetFieldOfView every frame even when nothing changed.

### sigil-recognition

- apps/vr/Game/Source/MageArenaVR/Gestures/MouseSigilCapture.cpp:52-68 - IngestMouse is called only while LMB is down and on the release frame, so the mouse builder never advances PenUpTime: a bare mouse circle never times out (InnerWaitSeconds 0.40, SigilStrokeBuilder.cpp:270), no reject cue plays, and any later drag, seconds afterwards, becomes its inner stroke
- apps/vr/Game/Source/MageArenaVR/Session/CollarCapture.cpp:127 and Session/DayCapture.cpp:120 - RaisePalm broadcasts UWardDetectorSubsystem::OnWardRaised directly, skipping the gesture pipeline (CLAUDE.md: never a shortcut that skips the pipeline); play ward-raise through the hand clip lane instead
- apps/vr/Game/Source/MageArenaVR/Gestures/SigilRecognizerSubsystem.cpp:175-179 - every builder rejection is logged "no circle" and broadcast as -1.0, including a bare circle that timed out and an inner mark that was too short; the reason the teach and the audio would need is lost
- apps/vr/Game/Source/MageArenaVR/Gestures/StaffDetector.cpp:60-76 - the staff detector has no held-sample (F7(b)) handling and no HeldSample test (STACK-OPPORTUNITIES F7 "not proven: the staff detector"); its only cue is first-to-last palm Z in a 1.5 s window
- apps/vr/Game/Source/MageArenaVR/Gestures/SigilStrokeBuilder.cpp:131 - FitPlane fixes the view direction at world +X ("Seated origin looks along +X"); the mouse plane faces the camera (MouseSigilCapture.cpp:27-35), so a camera yawed past 90 degrees (side-pad view) would mirror the projected sigil - worth a test if the pawn turns on blink

### spell-schools-and-ai

- apps/vr/Game/Source/MageArenaVR/Kernel/Water.cpp:144 - a water mage's windup ignores VrOpponentTelegraph, so Gentle mode doubles fire (Fire.cpp:183), air and enemy telegraphs but not a water-proxy opponent's.
- apps/vr/Game/Source/MageArenaVR/Kernel/MageAI.cpp:163,181 - fire and air channel threats share the id -Enemy.Id for the whole channel, so React's bReacted memory answers only the first of Brand Wrath's or Cyclone's eight ticks and drops the ward for the rest (FIRE.md calls the stable id intended; worth an owner check).
- apps/vr/Game/Source/MageArenaVR/Kernel/Water.cpp:166 - `check(Spell)` after FindSpellById is the only guard before `Spell->Damage`; with checks compiled out an unknown pending id dereferences null.
- apps/vr/Game/Source/MageArenaVR/Kernel/Games.cpp:120 - opponent preset index is `Games.Wave - 2`, hard-wiring Tiro's mage waves to indices 2 and 3; any other tier or StartWave layout trips the checkf.
- apps/vr/Game/Source/MageArenaVR/Kernel/MageAI.cpp:873 - aim lead divides by BoltSpeed from the school's bolt row with no zero guard; a 0 or missing speed gives an infinite lead and a NaN aim.
