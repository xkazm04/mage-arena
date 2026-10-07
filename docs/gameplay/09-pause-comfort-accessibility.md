---
title: Pause, comfort and accessibility
channel: vr
status: partial
verified-against: 1fb1b8b (2026-10-07); anchors into files changed since 46643e2 re-verified
sources: [docs/DECISIONS.md, docs/PROJECT-PLAN.md, docs/DESKTOP-INPUT.md, apps/vr/Game/Source/MageArenaVR/Session/ArenaSession.h, apps/vr/Game/Source/MageArenaVR/Session/ArenaSession.cpp, apps/vr/Game/Source/MageArenaVR/Session/SessionFlow.cpp, apps/vr/Game/Source/MageArenaVR/Session/SessionPresentation.cpp, apps/vr/Game/Source/MageArenaVR/Session/SettingsCapture.cpp, apps/vr/Game/Source/MageArenaVR/Hands/MageSettings.h, apps/vr/Game/Source/MageArenaVR/Hands/MageSettings.cpp, apps/vr/Game/Source/MageArenaVR/Hands/MageArenaPawn.cpp, apps/vr/Game/Source/MageArenaVR/Hands/MageArenaPlayerController.cpp, apps/vr/Game/Source/MageArenaVR/Kernel/VrRules.h, apps/vr/Game/Source/MageArenaVR/Kernel/Enemies.cpp, apps/vr/Game/Source/MageArenaVR/Kernel/Fire.cpp, apps/vr/Game/Source/MageArenaVR/Gestures/WardDetector.h, apps/vr/tasks/T13-teach-and-pause.md, apps/vr/tasks/T14-left-hand-narrow-fov-gentle.md, apps/vr/tasks/BACKLOG.md]
data: [apps/vr/data/vr/teach.json (trackingLossS, resumeStepSeconds, resumeStepCount, offerHoldS), apps/vr/data/vr/strings.json (pause, resume.*, settings.*), apps/vr/data/vr/combat.vr.json (narrowFov, gentle), apps/vr/data/vr/arena-layout.json (blink, mouseLook, cameraFovDeg, threats, spawnArc)]
---

# Pause, comfort and accessibility

The kernel is fixed-step, so a pause simply stops stepping it. Three safety triggers pause the session (application
focus loss, headset removal, 1.5 s of lost hand tracking in a bout); resuming needs **both palms raised** and a 3-2-1
count, after which telegraphs continue exactly where they stopped. Quitting mid-bout saves the bout index so the next
launch offers to continue. Three player settings - **left-hand mode**, **narrow view** and **Gentle** - default off,
persist in a small JSON file, and are changed by palming three stones on the dais before the teach. The seated body
never moves except the blink snap, which fades the camera through black. The colour language is doubled by shape (ring,
elongated steel, rimmed black core), but **not by sound**: the build plays no audio at all.

## Pause triggers

| Trigger | Detection | Desktop stand-in | Pauses when | Source |
|---|---|---|---|---|
| Focus loss | `FCoreDelegates::ApplicationWillDeactivateDelegate` (not bound when unattended) | switch windows | any running phase | `ArenaSession.cpp` `OnAppDeactivate`, `PollPlatform`; `NotifyFocusLost` |
| Headset removed | HMD worn state goes Worn -> NotWorn | F8 | any running phase | `PollPlatform`; `MageArenaPlayerController.cpp:147-156` |
| Hand tracking lost | frames arrive but none has confidence > 0, for `trackingLossS` | F9 toggles untracked frames | only in `active` (a bout), not in cold, teach or intermission | `SessionFlow.cpp:433-456` `PollTracking` |
| Esc | key | Esc | any | `ArenaSession.cpp` `TogglePause` |

| Value | Number | Unit | Source key |
|---|---|---|---|
| Tracking loss before a pause | 1.5 | s | `teach.json` `trackingLossS` |
| Resume count steps | 3 | steps | `teach.json` `resumeStepCount` |
| Seconds per count step | 1.0 | s | `teach.json` `resumeStepSeconds` |

The three safety triggers arm a **resume gate**; Esc does not: Esc toggles the pause instantly and clears any gate or
count. While paused, `FArenaSession::Advance` only runs the resume check: the kernel, the teach clock, AI timers, mana,
cooldowns and the tier clock all stand still.

## Resume

| Step | Rule | Source |
|---|---|---|
| Prompt while paused | `pause` "Paused" on the dais plaque; cuff mask `pause` | `SessionFlow.cpp:556-559`; `SessionPresentation.cpp` `SyncCuff` |
| Gate | both palms raised: both hands' confidence >= 0.2, palm height >= 0.20 m, palm normal within 35 deg of forward | `SessionFlow.cpp:463-486`; `WardDetector.h` `RaiseHeightM`, `RaiseAngleDeg` |
| Count | "3", "2", "1" (`resume.3/2/1`), 1.0 s each; lowering either palm resets it | `SessionFlow.cpp:488-526` |
| Continue | the kernel steps again from the paused tick | `SetPaused(false)` |
| No free hits | the state hash before the pause equals the hash at the first stepped tick after it (minus one tick) | `MageArena.Pause.*` (T13 deliverable 4) |
| Pause in the teach | resumes the same teach step | T13 deliverable 4 |

### Resume after quitting

| Item | Build | Source |
|---|---|---|
| Save file | `Saved/MageArena/bout.txt`, text `bout=<Tiro wave index>` | `SessionFlow.cpp:177-183, 247-255` |
| Written | on quit while a bout is `active`, and when an intermission advances | `NotifyQuit`; `SessionFlow.cpp:1313-1317` |
| Next launch | `offer` phase: palm = start that bout from its beginning; both palms held 0.40 s = restart from the teach | `BeginArc`, `ContinueOffer` |
| Invalid file | missing, junk, or outside Tiro's waves: logged, arc starts at the teach | `ReadSavedBout` |

### Cold start

The `-game` launch goes straight to the arena with no login, account or menu (`BeginArc(1, false)`,
`ArenaSession.cpp:1802`). The plan's target of 15 s or less from launch to interactive (section 6.6) has **not been
measured**.

## Settings

| Setting | Values (default first) | Persisted key | Console command | Command-line override | Dais stone (clip cm, seated origin) |
|---|---|---|---|---|---|
| Casting hand | right, left | `hand` | `MageArena.Settings.Hand left|right` | `-MageArenaHand=` | left stone (55, -28, 18) |
| View | wide, narrow | `fovMode` | `MageArena.Settings.Fov wide|narrow` | `-MageArenaFov=` | centre stone (55, 0, 18) |
| Gentle | off, on | `gentle` | `MageArena.Settings.Gentle on|off` | `-MageArenaGentle=` | right stone (55, 28, 18) |

- File: `Saved/MageArena/settings.json`, e.g. `{"hand":"right","fovMode":"wide","gentle":false}`. A missing, corrupt or
  incomplete file keeps the defaults; a command-line override does not write the file (`MageSettings.h:6-8`,
  `MageSettings.cpp:298-317`).
- Stones: a palm or index tip within 6 cm of a stone toggles that setting once per touch. Live only in the `cold` phase
  and never for a scripted player (`ArenaSession.cpp:489-560` `PollComfortToggles`). Labels come from `strings.json`
  `settings.*`.
- Settings are applied to the ruleset when a bout starts (`ApplyComfort` in `Start`) and the narrow arc is re-centred
  on each accepted blink.

### Left-hand mode

| Effect | Build | Source |
|---|---|---|
| Hands | the casting hand is the left, the ward hand the right | `FMageSettings::CastingHand`, `WardHand` |
| Clips | keys play the mirrored clip set under `Game/Clips/mirror/` (generated by `clipgen`; drawn in front of the left side of the body, same shape and direction) | `HandClip.cpp:84-91`; T14 Retry 1 item 2 |
| Detectors | ward palm normal computed per hand; blink flick direction still resolves the pad on the player's left/right | T14 Retry 1 items 1-2 |
| Cuff | moves to the other side of the view (Y +26 cm instead of -26 cm) | `SessionPresentation.cpp` `SyncCuff` |

### Narrow view (FOV-aware design)

| Field | Value | Unit | Source key |
|---|---|---|---|
| Arc for spawns, hold lines, new telegraph origins, death-burst origins | +-40 | deg around the active pad's forward | `combat.vr.json` `narrowFov.spawnArcDeg` |
| Offered when the device horizontal FOV is under | 90 | deg | `narrowFov.offerBelowDeg` |
| Desktop camera FOV while on | 70 | deg | `narrowFov.cameraFovDeg` |
| Wide desktop camera FOV | 90 | deg | `arena-layout.json` `cameraFovDeg` |

- The offer replaces the cold prompt with `settings.offer.narrow` "Narrow view? Palm the centre stone." It is never
  forced (`SessionFlow.cpp:560-566`).
- "Device FOV" is the layout camera FOV (90) unless `-MageArenaDeviceFov=` is given (`MageSettings.cpp:257-279`), so the
  desktop build is never offered narrow view by default and no HMD FOV is queried yet.
- Opponents already on the field walk back toward the arc at their walk speed after a blink rather than snapping
  (`VrRules.cpp` `KeepInView`); in-flight telegraphs from before a blink finish where they are.
- Plan reference: VR glasses about 70 x 66 deg against Quest 3's 110 x 96 deg (section 6.1).

### Gentle

| Field | Value | Applies to | Source |
|---|---|---|---|
| Opponent telegraph multiplier | 2 | enemy attack windups (`Enemies.cpp:62`), Fire mage cast windup and area telegraphs (`Fire.cpp:186, 286`); not the player | `combat.vr.json` `gentle.telegraphMult` |
| Opponent mage competence | 1 | every opponent mage at spawn (`Games.cpp:111-114`) | `gentle.competence` |

Gentle does **not** lengthen Water-proxy mage spell telegraphs (`Water.cpp` `TrySpellCast` does not call
`VrOpponentTelegraph`) or hound death-burst delays (`Enemies.cpp` `QueueDeathEffects`).

## Comfort rules

| Rule | Build | Source |
|---|---|---|
| Locomotion | none; the player is pinned to the active pad every tick | `ArenaSession.cpp` `SeatOnActivePad` |
| Blink | camera fade to black 0.12 s, snap to the pad, fade in 0.2 s, with a vignette ramp to 1.15. The vignette is four black quads attached to the camera, not a post-process: they frame a clear opening that closes from 65 deg to 15 deg half-angle (square-root ramp) as opacity rises to 1 | `arena-layout.json` `blink.fadeOutS`, `fadeInS`, `vignettePeak`; `MageArenaPawn.cpp` `SetVignette`, `VignetteAperture`, `UpdateVignetteQuads`; `MageArenaPawn.h` `VignetteQuads` |
| Threats from behind | none; spawn markers within +-70 deg of the centre pad's forward | `arena-layout.json` `spawnArc.limitDeg` |
| Desktop mouse look | yaw +-100 deg around the pad facing; pitch -32 / +18 deg; frozen while a mouse button is down | `arena-layout.json` `mouseLook` |
| Prompts | a plaque in the world, 220 cm ahead of and 162 cm above the active pad | `SessionPresentation.cpp:650-680` |
| Wrist cuff (HP, mana, stamina, clock bars, Flow/tier masks, wrist cues) | **attached to the camera** at lower left (58, -26, -14 cm) | `SessionPresentation.cpp:366-436` |

Commit `92c77e1` (after the verified commit, 2026-10-07) replaced the post-process vignette with four camera-attached
quads because the flat presentation's tonemapper-off never showed the post-process vignette; the fade timings are
unchanged.

## Colour, shape and sound doubling

From `arena-layout.json` `threats.order` (greybox, visual only) and [04-threat-language.md](04-threat-language.md):

| Family | Colour | Shape cue | Ground ring | Sound cue |
|---|---|---|---|---|
| Magic: water | turquoise | sphere r 0.38 m, not rimmed | shrinking ring (1.7 m to 0.28 m) in the element colour | none |
| Magic: fire | vermilion | sphere r 0.22 m, not rimmed | ring in the element colour | none |
| Physical: steel | grey (0.68) | elongated 1.7 m shaft, r 0.11 m (the thrown spear) | grey ring | none |
| Unblockable | black core | rimmed with a red rim (`rimScale` 1.18) | red ring | none |

## Status and gaps

- **Sound doubling is absent.** Plan 6.7: "magic threats have a ring, steel is angular, black-core has a rim plus a low
  drone". No audio component exists in `apps/vr/Game/Source` (no `USoundBase`, `UAudioComponent` or `PlaySound`); the
  teach's "bell" is the log line `cue bell perfect-absorb`. Starter SFX exist as files (A05) but are not wired (BACKLOG).
- **Head-locked HUD.** Plan 6.7: "Head-locked UI is avoided; the HUD is diegetic on the wrists and the dais." Build: the
  cuff is parented to the camera (`SessionPresentation.cpp:378`), so it is head-locked on desktop; prompts are on the
  dais as planned.
- **Steel shape.** Plan 6.7: "steel is angular". Build: steel is an elongated grey shaft; "angular" is not otherwise
  expressed (BACKLOG notes an earlier grey-sphere spear).
- **Narrow view offer** compares a configured FOV, not the device's (no HMD query yet).
- **Gentle** covers enemy windups and Fire mage casts only (see above). Plan 6.7: "doubles telegraph times".
- **Settings only before the teach.** The stones work in `cold` only; afterwards only console commands change settings.
- **OpenXR focus loss** is not wired; only the desktop deactivate delegate and the worn-state poll (BACKLOG).
- **Quest 3S** stays declared and untested (owner, 2026-10-07); narrow view bounds only the FOV part of that risk.
- **Cold-start time** not measured against the 15 s target.

## Open questions

- Should settings be reachable after the teach (for example in the intermission), not only on the cold dais?
- Which audio cues are required for the shape/sound doubling, and when are they wired (ElevenLabs is the approved
  source)?
- Should the cuff be attached to the ward wrist (diegetic) in the VR build, as the plan says, rather than the camera?
