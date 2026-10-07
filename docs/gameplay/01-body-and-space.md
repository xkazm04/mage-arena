---
title: Body and space
channel: vr
status: partial
verified-against: 1fb1b8b (2026-10-07); anchors into files changed since 46643e2 re-verified
sources:
  - docs/DECISIONS.md
  - docs/PROJECT-PLAN.md
  - docs/design-findings/DF-001-seated-wave1.md
  - docs/design-findings/DF-004-creatures-cannot-reach-the-seat.md
  - docs/change-requests/CR-001-blink-and-pads.md
  - docs/change-requests/CR-003-threat-geometry.md
  - apps/vr/Game/Source/MageArenaVR/Greybox/ArenaLayout.cpp
  - apps/vr/Game/Source/MageArenaVR/Greybox/ArenaLayout.h
  - apps/vr/Game/Source/MageArenaVR/Hands/MageArenaPawn.cpp
  - apps/vr/Game/Source/MageArenaVR/Hands/MageArenaPawn.h
  - apps/vr/Game/Source/MageArenaVR/Hands/MageSettings.cpp
  - apps/vr/Game/Source/MageArenaVR/Kernel/VrRules.h
  - apps/vr/Game/Source/MageArenaVR/Kernel/VrRules.cpp
  - apps/vr/Game/Source/MageArenaVR/Kernel/Games.cpp
  - apps/vr/Game/Source/MageArenaVR/Kernel/Geometry.cpp
  - apps/vr/Game/Source/MageArenaVR/Kernel/KernelData.cpp
  - apps/vr/Game/Source/MageArenaVR/Kernel/Enemies.cpp
  - apps/vr/Game/Source/MageArenaVR/Kernel/Fire.cpp
  - apps/vr/Game/Source/MageArenaVR/Session/ArenaSession.cpp
  - apps/vr/Game/Source/MageArenaVR/Tests/GreyboxTests.cpp
  - apps/vr/Game/Source/MageArenaVR/Greybox/ThreatDemo.cpp
data:
  - apps/vr/data/vr/arena-layout.json#floor
  - apps/vr/data/vr/arena-layout.json#dais
  - apps/vr/data/vr/arena-layout.json#pads
  - apps/vr/data/vr/arena-layout.json#seat
  - apps/vr/data/vr/arena-layout.json#seatedEyeHeightAbovePadM
  - apps/vr/data/vr/arena-layout.json#cameraFovDeg
  - apps/vr/data/vr/arena-layout.json#mouseLook
  - apps/vr/data/vr/arena-layout.json#blink
  - apps/vr/data/vr/arena-layout.json#spawnArc
  - apps/vr/data/vr/arena-layout.json#spawnMarkers
  - apps/vr/data/vr/combat.vr.json#daisNoEntry
  - apps/vr/data/vr/combat.vr.json#narrowFov
  - apps/vr/data/pinned/packages/core/src/arena/data/runtime.json#games.playerSpawn
  - apps/vr/data/pinned/packages/core/src/arena/data/runtime.json#games.enemySpawn
  - apps/vr/data/pinned/packages/core/src/arena/data/runtime.json#geometry
  - apps/vr/data/pinned/art/scale-contract-v1.json#arena_metres
---

# Body and space

The player sits on a backless stone seat on one of three rune pads on a raised dais at one end of an oval sand arena.
The player never walks: the session pins the kernel body to the active pad on every tick, and a blink between pads is the
only position change. Enemies stay on the sand below the dais (owner decision 2026-10-03, DF-001 option A), held outside the
dais footprint plus a 0.6 m standoff, and attack from range. The visible arena, the pads and the camera come from
`arena-layout.json`; the keep-out box and the narrow-view numbers come from the VR combat overlay `combat.vr.json`; the
kernel's own walkable bound comes from the pinned shared data. The threat-arc limits (±70° threats, ±100° flank, nothing
behind) are met by where the build spawns enemies and by the camera yaw clamp; the kernel does not enforce them as a rule.

## Frames and units

| Frame | Axes | Units | Origin | Source |
|---|---|---|---|---|
| Layout (Unreal world) | +X from the dais toward the arena centre, +Y to the seated player's right, +Z up | metres in data, centimetres at runtime | centre of the sand ellipse, on the sand (z = 0) | `arena-layout.json#axes`, `#origin` |
| Kernel (2D ground plane) | same X/Y directions | metres | pinned kernel frame; the centre pad maps to `games.playerSpawn` (8, 10) | `combat.vr.json#axes`, `runtime.json#games.playerSpawn` |
| Clip space (hands) | same axes | centimetres | seated origin (clip hip), 0.62 m below and 0.08 m behind the eyes | `arena-layout.json#eyeAboveSeatedOriginM`, `#eyeForwardOfSeatedOriginM` |

Kernel position = `playerSpawn` + (layout position − centre pad position). The shift is applied when the ruleset is built
(`VrRulesLoad.cpp:572-575`, `LoadVrRuleset`) and when the session converts pads (`ArenaSession.cpp:560-572`, `PadKernel`).

## Arena and dais

| Quantity | Value | Unit | Source key |
|---|---|---|---|
| Sand floor shape | ellipse, 32 × 20 (radii 16 × 10) | m | `arena-layout.json#floor.radiusXM`, `#floor.radiusYM` |
| Ring wall height | 3 | m | `arena-layout.json#wall.heightM` |
| Stands | 3 stepped tiers outside the wall | - | `arena-layout.json#stands.tiers` |
| Dais centre (box centre) | (−11.7, 0, 0.3) | m, layout | `arena-layout.json#dais.centreM` |
| Dais size | 5.2 × 8.8 × 0.6 | m | `arena-layout.json#dais.sizeM` |
| Dais top above sand | 0.6 | m | `arena-layout.json#dais.heightM` |
| Dais no-entry half extent | 2.6 × 4.4 | m | `combat.vr.json#daisNoEntry.halfExtentM` (must equal half of `dais.sizeM`, checked at load, `VrRulesLoad.cpp:489-494`) |
| Standoff (hold line outside the footprint) | 0.6 | m | `combat.vr.json#daisNoEntry.standoffM` (must equal `dais.heightM`, checked at load) |
| Dais footprint in kernel metres | x 5.7 to 10.9, y 5.6 to 14.4 | m, kernel | derived from the rows above |
| Hold box in kernel metres | x 5.1 to 11.5, y 5.0 to 15.0 | m, kernel | derived (footprint expanded by standoff) |
| Centre pad to dais front face | 2.9 | m | `combat.vr.json#attacks[0]._rangeM` (pad x −12, face x −9.1) |
| Centre pad to hold line straight ahead | 3.5 | m | derived (2.9 + 0.6) |
| Kernel walkable bound | ellipse 192 × 144 centred on (16, 62) | m, kernel | `scale-contract-v1.json#arena_metres`, `runtime.json#geometry.arenaCentre`; `Geometry.cpp:25-37` |

### Floor versus dais (DF-001)

- Every enemy body, listed in the overlay or not, is pushed back outside the hold box after each kernel step
  (`FVrRuleset::KeepOut`, `VrRules.cpp:206-223`; called from `Games.cpp:247`).
- Conscripts throw their spear instead of lunging (range 9 m, 14 m/s); slingers keep the pinned sling. Details and
  numbers: [07-enemies-and-ai.md](07-enemies-and-ai.md) and [04-threat-language.md](04-threat-language.md).
- A thrower walks to the hold line during its pinned backoff window and holds there (`combat.vr.json#movement`).
- Melee reach never touches a pad. The bolt gesture is not turned into the pinned staff strike from the seat: the session
  suppresses a bolt when an enemy is within staff range (`ArenaSession.cpp:753-757`), and the hold box keeps bodies
  out of that range in practice.

## Rune pads and the seat

| Quantity | Value | Unit | Source key |
|---|---|---|---|
| Pad count | 3: index 0 left (−Y), 1 centre, 2 right (+Y) | - | `arena-layout.json#pads.items` |
| Pad arc | circle of radius 12 about the arena centre | m | `arena-layout.json#pads.arcRadiusM` |
| Chord spacing between neighbours | 3.0 (authored test band 2.5 to 3.5) | m | `arena-layout.json#pads.spacingM`, `#pads._note` |
| Left pad | (−11.625, −2.97647, 0.6) | m, layout | `arena-layout.json#pads.items[0].positionM` |
| Centre pad | (−12, 0, 0.6) | m, layout | `arena-layout.json#pads.items[1].positionM` |
| Right pad | (−11.625, 2.97647, 0.6) | m, layout | `arena-layout.json#pads.items[2].positionM` |
| Pad disc / ring radius | 0.58 / 0.78 | m | `arena-layout.json#pads.discRadiusM`, `#pads.ringRadiusM` |
| Seat block | 0.5 × 0.46 × 0.42, backless | m | `arena-layout.json#seat.sizeM` |
| Session start pad | centre (1) | - | `ArenaSession.cpp:263`, `ArenaSession.cpp:292` |
| Pad facing | horizontal direction from the pad toward the arena centre | - | `ArenaLayout.cpp:484` (`FlatForwardM`); `arena-layout.json#mouseLook._note` |

The pad is a layout concept, not a kernel field (CR-001 asks the shared kernel for a real pad id; not merged).

## Camera and head

| Quantity | Value | Unit | Source key |
|---|---|---|---|
| Eye height above the active pad top | 1.2 | m | `arena-layout.json#seatedEyeHeightAbovePadM` |
| Desktop camera horizontal FOV (wide) | 90 | deg | `arena-layout.json#cameraFovDeg` |
| Initial look target | (0, 0, 0.9) | m, layout | `arena-layout.json#lookTargetM` |
| Mouse-look yaw clamp around the pad facing | ±100 | deg | `arena-layout.json#mouseLook.yawLimitDeg`; `ClampLookRotation`, `MageArenaPawn.cpp:289-291` |
| Pitch down / up | 32 / 18 | deg | `arena-layout.json#mouseLook.pitchDownDeg`, `#pitchUpDeg` |
| Mouse sensitivity | 0.08 | deg/pixel | `arena-layout.json#mouseLook.sensitivityDegPerPixel` |

Hands are drawn in clip space parented to the body, so mouse look turns the head, not the hands. Look is suppressed while
a mouse button is down (sigil drawing, ward aiming). On a headset the yaw clamp does not apply to real head motion; it is a
desktop control.

## Blink presentation (the only camera motion)

A gesture blink calls `AMageArenaPawn::PresentBlink` (`ArenaSession.cpp` `ApplyCamera`): fade out, snap the camera to the
target pad, fade in, with a comfort vignette. No smooth motion.

| Quantity | Value | Unit | Source key |
|---|---|---|---|
| Fade out | 0.12 | s | `arena-layout.json#blink.fadeOutS` |
| Fade in | 0.2 | s | `arena-layout.json#blink.fadeInS` |
| Vignette peak | 1.15 | - (intensity) | `arena-layout.json#blink.vignettePeak` |
| Debug pad snaps (L3, not a gesture) | F5 left, F6 centre, F7 right | key | `arena-layout.json#blink.keys` |

The blink's combat rules (stamina, i-frames, pad resolution) are in [02-verbs-and-gestures.md](02-verbs-and-gestures.md#blink).
The vignette was re-implemented as camera-attached geometry in commit 92c77e1, after the verified commit; the timing values
above did not change.

## Threat arc

| Rule | Value | Where it holds in the build |
|---|---|---|
| Default threat arc around the centre pad forward | ±70 deg | Layout spawn markers sit at bearings 0, ±35, ±50 (`arena-layout.json#spawnArc`, `#spawnMarkers`); `GreyboxTests.cpp:128-129` asserts each marker is inside ±70. Kernel spawns open at `games.enemySpawn` (24, 10), 16 m straight ahead of the centre pad, with 0.8 m jitter (`runtime.json#games`, `Geometry.cpp:39-48`). |
| Flank limit | ±100 deg | Camera yaw clamp only (`arena-layout.json#mouseLook.yawLimitDeg`). |
| Nothing from behind | - | Follows from the forward spawn and the dais at the −X end; not a kernel rule. |
| Flanker announcement (spatial audio, edge chevron) | - | Not built. No audio code and no chevron exist in `Source/`. |

The layout `spawnMarkers` are used by the greybox and the threat demo (`ThreatDemo.cpp:133`); the bout itself spawns
from the pinned kernel spawn point.

## Narrow-FOV mode

Plan D3: for devices with a narrow field of view. The option is a runtime flag (`FMageSettings`), off by default, and is
never forced.

| Quantity | Value | Unit | Source key |
|---|---|---|---|
| Spawn and telegraph arc while on | ±40 about the active pad forward | deg | `combat.vr.json#narrowFov.spawnArcDeg` |
| Offered when device or desktop horizontal FOV is strictly under | 90 | deg | `combat.vr.json#narrowFov.offerBelowDeg`; `MageSettings.cpp:277-280` |
| Desktop camera FOV while on | 70 | deg | `combat.vr.json#narrowFov.cameraFovDeg`; `MageArenaPawn.cpp:159-160` |
| How the player accepts | palm on the centre stone during the cold start | - | `strings.json#settings.offer.narrow`; `ArenaSession.cpp:489-558` |

What the flag changes in the kernel (`VrRules.cpp:157-204`, `CompressToArc`, `KeepInView`, `Games.cpp:78-85`, `Enemies.cpp:58`, `Enemies.cpp:436`,
`Fire.cpp:195`, `Fire.cpp:283-284`):

- New enemy spawn positions are compressed to the arc (bearing clamped to ±40°, distance kept), then pushed out of the hold
  box if needed.
- Hold points, enemy telegraph origins and Fire-mage cast aims and telegraph targets are compressed to the arc.
- An enemy already outside the arc walks back toward the nearest arc point at no more than its own walk speed per tick
  (`KeepInView`). Positions are never snapped, so a blink does not teleport enemies.
- The arc is centred on the active pad's kernel position and flat forward, rewritten by the session every tick
  (`ApplyComfort`, `ArenaSession.cpp:462-487`).

Because the layout camera is 90°, the desktop bout is not offered narrow view by default; `-MageArenaDeviceFov=` or the
settings stones can still turn it on. The player-facing settings flow is in
[09-pause-comfort-accessibility.md](09-pause-comfort-accessibility.md).

## Status and gaps

Built:

- Seat, dais, three pads, the pad pin and the blink snap with fade and vignette (`ArenaSession.cpp:1486-1491`,
  `ArenaSession.cpp:1597-1641`).
- Dais no-entry hold box for every enemy, thrown spear, thrower closes to the lip (VR overlay, CR-003).
- Narrow-FOV compression of spawns, hold points and telegraph origins, and the 70° desktop camera.
- Camera yaw clamp ±100°.

Designed only, or differs from the plan:

- **Flanker cues.** PROJECT-PLAN §6.1 announces flankers with spatial audio and an edge chevron. Neither exists in the
  build (no audio playback code under `apps/vr/Game/Source/`).
- **Threat arc as a rule.** PROJECT-PLAN §6.1 states ±70° / ±100° / nothing behind as rules. The build meets them by
  spawn placement and the camera clamp only; no kernel check rejects or redirects a threat from outside the arc in wide
  mode.
- **Kernel bound versus visible arena.** The kernel clamps bodies to the pinned 192 × 144 m ellipse
  (`KernelData.cpp:1677-1689`, `Geometry.cpp:25-37`), not to the 32 × 20 m sand oval drawn by the greybox. PROJECT-PLAN
  §6.1 calls the kernel "a 2D ground plane (32 × 20 m in B/2 data)". Nothing in the VR overlay clamps enemies to the
  visible sand.
- **Seated reach.** DF-004 (open): hounds die about 3.5 m from the centre pad, outside their 1.5 m death burst, so the
  creatures bout cannot teach the perfect absorb from the seat. This is a direct consequence of the floor-versus-dais rule.
- **Pads in the kernel.** CR-001 (open) asks the shared kernel for pad positions and a blink input. Until it merges, the
  VR session feeds a roll edge and overwrites the position (adapter).

## Open questions

- Can an enemy end up behind the seated player in wide mode? `KeepOut` pushes a body to the nearest hold-box edge
  (`PushOutsideHold`, `VrRules.cpp:72-102`), and the back edge (kernel x = 5.1) is behind the centre pad (x = 8). A hound orbiting at
  `games.houndOrbitM` 3.5 m around the player would cross that edge. Not measured.
- Should the VR overlay clamp enemy bodies to the visible 32 × 20 m sand, or should the greybox oval grow to match the
  kernel bound? Neither document states which is intended.
- How is the ±100° flank limit meant to apply on a headset, where the camera clamp does not exist?
