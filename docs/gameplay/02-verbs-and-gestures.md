---
title: Verbs and gestures
channel: vr
status: partial
verified-against: 46643e2 (2026-10-07)
sources:
  - docs/DECISIONS.md
  - docs/PROJECT-PLAN.md
  - docs/DESKTOP-INPUT.md
  - docs/CLIP-SCHEMA.md
  - docs/design/SCHOOL-DEFENCES.md
  - docs/design-findings/DF-001-seated-wave1.md
  - docs/change-requests/CR-001-blink-and-pads.md
  - apps/vr/tasks/T11-split-hands-and-staff.md
  - apps/vr/tasks/BACKLOG.md
  - apps/vr/Game/Source/MageArenaVR/Gestures/SigilStrokeBuilder.h
  - apps/vr/Game/Source/MageArenaVR/Gestures/QPointCloudRecognizer.h
  - apps/vr/Game/Source/MageArenaVR/Gestures/SigilRecognizerSubsystem.h
  - apps/vr/Game/Source/MageArenaVR/Gestures/WardDetector.h
  - apps/vr/Game/Source/MageArenaVR/Gestures/WardDetector.cpp
  - apps/vr/Game/Source/MageArenaVR/Gestures/BlinkDetector.h
  - apps/vr/Game/Source/MageArenaVR/Gestures/BlinkDetector.cpp
  - apps/vr/Game/Source/MageArenaVR/Gestures/StaffDetector.h
  - apps/vr/Game/Source/MageArenaVR/Gestures/StaffDetector.cpp
  - apps/vr/Game/Source/MageArenaVR/Combat/AbsorbResolver.h
  - apps/vr/Game/Source/MageArenaVR/Combat/AbsorbResolver.cpp
  - apps/vr/Game/Source/MageArenaVR/Hands/MageArenaPlayerController.cpp
  - apps/vr/Game/Source/MageArenaVR/Kernel/ArenaKernel.cpp
  - apps/vr/Game/Source/MageArenaVR/Kernel/ArenaThreats.cpp
  - apps/vr/Game/Source/MageArenaVR/Kernel/Catalog.cpp
  - apps/vr/Game/Source/MageArenaVR/Kernel/Water.cpp
  - apps/vr/Game/Source/MageArenaVR/Kernel/VrRules.cpp
  - apps/vr/Game/Source/MageArenaVR/Session/ArenaSession.cpp
  - apps/vr/Game/Source/MageArenaVR/Session/ArenaSession.h
  - apps/vr/tools/clipgen/generate.mjs
  - apps/vr/tools/clipgen/corpus.mjs
data:
  - apps/vr/data/pinned/docs/design/baseline-fourteen-nights/design/data/combat.json#absorb
  - apps/vr/data/pinned/docs/design/baseline-fourteen-nights/design/data/combat.json#roll
  - apps/vr/data/pinned/docs/design/baseline-fourteen-nights/design/data/combat.json#stamina
  - apps/vr/data/pinned/docs/design/baseline-fourteen-nights/design/data/combat.json#staffStrike
  - apps/vr/data/pinned/docs/design/baseline-fourteen-nights/design/data/combat.json#lines
  - apps/vr/data/pinned/docs/design/baseline-fourteen-nights/design/data/spells-water.csv
  - apps/vr/data/pinned/docs/design/baseline-fourteen-nights/design/data/stats.csv
  - apps/vr/data/pinned/packages/core/src/arena/data/runtime.json#water.presets
  - apps/vr/data/pinned/packages/core/src/arena/data/runtime.json#training.defaultRanks
  - apps/vr/data/vr/combat.vr.json#splitHands
  - apps/vr/data/vr/combat.vr.json#plantedStaff
  - apps/vr/data/vr/arena-layout.json#hands
  - apps/vr/data/vr/teach.json
---

# Verbs and gestures

The seated player has four verbs that reach the kernel - Bolt, line cast (a drawn sigil), Ward and Blink - plus the
planted staff, which is a Water school defence. Every verb starts as hand-joint data: on the headset from hand tracking,
on the desktop from a recorded clip that a key replays, or from a mouse path. The same detectors read that stream and turn
it into one kernel input frame per 60 Hz tick ([DESKTOP-INPUT.md](../DESKTOP-INPUT.md), clip format in
[CLIP-SCHEMA.md](../CLIP-SCHEMA.md)). Bolt and Blink share one flick detector and are told apart by direction. The canonical
sigil is a circle plus a bar, and the bar's angle selects the line (owner decision 2026-10-03). The tier IV seal (mudra)
has a clip and a key but no detector and no rule: in the build a tier IV line casts from the sigil alone. Split hands and
the planted staff are summarised here and specified in [06-school-defences.md](06-school-defences.md).

## Pipeline

| Stage | What it is | Code |
|---|---|---|
| Hand source | headset joints, or a clip replayed by a key, or the mouse | `Hands/HandInputSubsystem.*`, `Hands/HandClipPlayer.*`, `Gestures/MouseSigilCapture.*` |
| Detectors | sigil (stroke builder + $Q), ward, flick (bolt and blink), staff | `Gestures/*` |
| Session adapter | turns detector events into input-frame fields; pins the body to the pad; feeds blink as a roll edge | `Session/ArenaSession.cpp` (`Handle*`, `StepKernel`) |
| Kernel | applies costs, windows and effects at 60 Hz | `Kernel/ArenaKernel.cpp`, `Kernel/Water.cpp`, `Kernel/ArenaThreats.cpp`, `Kernel/VrRules.cpp` |

Kernel tick: `simStepHz` 60 (`combat.json#simStepHz`). Seconds become ticks by `ceil(seconds × 60)` (`ArenaKernel.cpp:318-321`).

Player stats come from the pinned default ranks, Vigor 1, Focus 1, Nerve 1 (`runtime.json#training.defaultRanks`):

| Stat | Formula | Value at rank 1 | Source |
|---|---|---|---|
| Max HP | 80 + 15 × Vigor | 95 | `stats.csv#vigor` |
| Max stamina | 60 + 10 × Vigor | 70 | `stats.csv#vigor` |
| Max mana | 80 + 15 × Focus | 95 | `stats.csv#focus` |
| Mana regen | 6 + 1.5 × Focus per second | 7.5 /s | `stats.csv#focus` |
| Ward drain | 30 − 3 × Nerve per second held | 27 /s | `stats.csv#nerve`, `combat.json#absorb.drainPerSecond` |

Desktop keys (all play clips into the pipeline; see [DESKTOP-INPUT.md](../DESKTOP-INPUT.md)): `1`/`2`/`3` sigils, `4` mudra,
`Q` bolt, hold `Space` ward, `A`/`D`/`S` blink left/right/back, `F` staff plant (lift when planted), `R` both palms
(`MageArenaPlayerController.cpp:30-43`). Hold left mouse to draw a sigil; hold right mouse to ward toward the cursor.

## Bolt

Always available, no drawing. Slot 0 of the kernel input.

| Item | Value | Unit | Source |
|---|---|---|---|
| Gesture | casting-hand index flick forward while pointing | - | `BlinkDetector.h:188-206` |
| Pointing gate | index reach / middle reach ≥ 1.50 | ratio | `FBlinkThresholds::PointRatio` |
| Angular-rate gate | ≥ 1.40 | rad/s | `FBlinkThresholds::MinOmegaRadPerS` |
| Tip speed gate | ≥ 1.20 (horizontal) | m/s | `FBlinkThresholds::MinTipSpeedMps` |
| Fires when | tip speed falls under 0.55 × the peak | - | `FBlinkThresholds::FallFraction` |
| Forward band (bolt, not blink) | yaw within ±35 of body forward | deg | `FBlinkThresholds::ForwardYawDeg`; `BlinkDetector.cpp:50-58` |
| Re-arm | tip under 0.60 m/s for 0.15 s, or a clip rewind | m/s, s | `FBlinkThresholds::RearmSpeedMps`, `RearmQuietS` |
| Spell | Rain Needle, tier 0 | - | `spells-water.csv` row `bolt` |
| Mana | 3 | mana | `spells-water.csv#bolt.mana` |
| Cooldown | 0.25 | s | `spells-water.csv#bolt.cooldown_s` |
| Damage | 2.05 | HP | `spells-water.csv#bolt.damage` |
| Projectile | 20 m/s, range 12 m, cast time 0 (releases on the press tick) | - | `spells-water.csv#bolt` |
| Family | absorbable (kernel family `magic`) | - | `spells-water.csv#bolt.blockable` |
| Aim (human) | the view direction (head on device, mouse look on desktop) | - | `ArenaSession.cpp` `ApplyViewAim` |

Session rules (`ArenaSession.cpp:745-766`): a flick that arrives while a sigil clip is playing is ignored; a bolt is
suppressed when an enemy is within staff range + 0.05 m (the pinned staff strike is not used from the seat); with the ward
up and the staff not planted it is a split cast (below). A cast request that the kernel has not accepted after 90 ticks
(1.5 s) is dropped and logged `kernel cast rejected` (`ArenaSession.cpp:645`, `ArenaSession.cpp:1642-1654`).

Bolt counts as the line `bolt` for Flow: alternating Bolt with a sigil line builds Flow, two Bolts in a row reset it
([03-tier-clock-and-flow.md](03-tier-clock-and-flow.md)).

## Line cast (sigil)

Draw a circle, then a bar through it. The bar angle picks the slot; the slot picks the line in the fixed loadout; the line
casts its **current unlocked tier**.

### Gesture to slot to line

| Bar | Recognizer label | Slot | Line (Rotation preset) | Desktop key |
|---|---|---|---|---|
| vertical | `line1` | 1 | Tide Orb (`tide_orb`) | `1` |
| horizontal | `line2` | 2 | Lash (`lash`) | `2` |
| diagonal, lower left to upper right | `line3` | 3 | Mirror (`mirror`) | `3` |

Sources: `corpus.mjs:35-37`, `CLIP-SCHEMA.md` (seated space), `ArenaSession.cpp:660-692`, `Catalog.cpp:45-82`,
`runtime.json#water.presets[Rotation]` (`lines: [tide_orb, lash, mirror]`, branches Lash B, Mirror B, Tide Orb B). The
session always starts the player on the Rotation preset (`ArenaSession.cpp:240-245`).

### Recognizer

| Item | Value | Unit | Source |
|---|---|---|---|
| Draw style A (default) | pinch is the pen: down at ≥ 0.7, up below 0.5 | pinch | `SigilStrokeBuilder.h:65-71`, `PinchDown`, `PinchUp` |
| Draw style B | one stroke while the tip moves; 0.150 s still ends it | s | `StillGapSeconds` |
| Loop test | start-end gap ≤ 0.40 of the path, or net turn ≥ 300 | ratio, deg | `ClosedStartEndRatio`, `LoopTurnDegrees` |
| Minimum loop path | 25 | cm | `MinLoopPathCm` |
| Inner stroke minimum | 0.34 × loop diagonal | ratio | `MinInnerStrokeRatio` |
| Bare circle wait before reject | 0.40 | s | `InnerWaitSeconds` |
| Classifier | $Q point cloud, 32 points, 64 × 64 grid | - | `QPointCloudRecognizer.h:182-191` |
| Reject distance | 26000 | $Q distance | `FQPointCloudRecognizer::DefaultRejectDistance` |
| Synthetic draw time, normal clip | 2.11 (approach 0.28, circle 1.00, lift 0.16, bar 0.45, release 0.22) | s | `generate.mjs:63-65` |

The accuracy corpus and gates are described in [CLIP-SCHEMA.md](../CLIP-SCHEMA.md) and card T03; this page does not repeat
them. While a sigil clip plays, the casting hand is busy: a flick is not read as a bolt or blink by the session.

### Kernel effect

`TrySpellCast` (`Water.cpp:76-156`): the spell is the row for (line, current tier, branch). Mana is paid on the press
(Crest: 0, see page 03). The line goes on its own cooldown. The cast releases after `max(cast_s, telegraph_s)`. A passive
row cannot be cast.

| Tier | Tide Orb (B) | Lash (B) | Mirror (B) |
|---|---|---|---|
| I | Bubble Shot: 10 mana, 7 dmg, projectile 12 m/s, cd 2.5 s | Tide Lash: 8 mana, 8 dmg, cone 90° × 3 m, cd 2.0 s | Sheen: 8 mana, self 3 s, absorbed magic refunds +50% mana |
| II | Crash Orb: 16 mana, 12 dmg, burst r 2 m, cd 4.0 s | Riptide: 14 mana, 9 dmg, cone 3 m + push 3 m, cd 5.0 s | Ripple: passive, perfect absorb releases a 7.5 dmg ring r 3 m (cannot be cast) |
| III | Twin Tides: 24 mana, 11 dmg each, two shots ±10° | Maelstrom Lash: 22 mana, 15 dmg, ring r 3 m | Mirage: 20 mana, decoy 3 s |
| IV | Rain of Orbs: 40 mana, 9 dmg each, 5-orb fan 50° | Drown Coil: 40 mana, 25 dmg, **unblockable**, grab 2.5 m, root 1.5 s | Return Tide: 20 mana, releases stored absorbed damage (max 40) |

All values from `spells-water.csv` (pinned). Full rows, including cast and telegraph times, are in
[10-data-reference.md](10-data-reference.md) and the Water school page under [05-schools/](05-schools/).

## Ward

The off-hand palm raised toward the threats. It is the pinned absorb (`combat.json#absorb`), used verbatim.

### Detector (`WardDetector.h:28-37`, `WardDetector.cpp:250-300`)

| Item | Value | Unit | Source |
|---|---|---|---|
| Hand | ward hand (left by default; swaps in left-hand mode) | - | `FMageSettings::WardHand()` |
| Raised when | palm normal within 35 of the reference facing and palm ≥ 0.20 m above the seated origin | deg, m | `RaiseAngleDeg`, `RaiseHeightM` |
| Lowered when | palm normal beyond 50, or palm under 0.16 m | deg, m | `LowerAngleDeg`, `LowerHeightM` |
| Reference facing | the threat facing when a threat is targeted, else the aim facing | - | `WardDetector.h:59-60` |
| Motion onset | walk back from the raise to the palm-speed rising edge, ≥ 0.30 m/s, tail ≤ 29/72 s | m/s, s | `OnsetSpeedMps`, `OnsetTailS` |
| Fresh raise | first raise from an observed lowered palm, or a re-raise ≥ 0.12 s after the last lower | s | `WardDetector.cpp:270-276`, `combat.json#absorb.minReleaseBeforeReRaiseS` |

Desktop: hold `Space` (aims at the view centre) or hold right mouse (aims at the cursor). A tap timed to the hit is the
desktop perfect.

### Kernel (`ArenaKernel.cpp:22-70`, `ArenaThreats.cpp:277-345`)

| Rule | Value | Unit | Source key |
|---|---|---|---|
| Raise cost | 5 | mana | `combat.json#absorb.raiseCostMana` |
| Raise refused when | mana under 5, rolling, a cast pending, or less than 0.12 s since release | - | `ArenaKernel.cpp:34-48` |
| Drain while held | 30 − 3 × Nerve (27 at Nerve 1) | mana/s | `combat.json#absorb.drainPerSecond` |
| Ward drops when | mana runs out (then stays down until the palm is lowered) | - | `ArenaKernel.cpp:60-68` |
| Arc | 140, centred on the actor's facing (the direction to the input aim) | deg | `combat.json#absorb.arcDeg`, `#absorb.facing` |
| Reduction, magic | 0.85 | fraction removed | `combat.json#absorb.reduction.magic` |
| Reduction, physical | 0.30 | fraction removed | `combat.json#absorb.reduction.physical` |
| Reduction, unblockable | 0 | - | `combat.json#absorb.reduction.unblockable` |
| Reduction outside the arc | 0 | - | `combat.json#absorb.reduction.outsideArc` |

The greybox draws the ward as a translucent 140° ribbon 0.48 to 0.52 m in front of the off hand
(`arena-layout.json#hands.wardArc*`).

### Perfect ward

| Rule | Value | Unit | Source key |
|---|---|---|---|
| Window | hit resolves no more than 0.15 s (9 ticks, inclusive) after a fresh raise | s | `combat.json#absorb.perfect.windowS`; `ArenaThreats.cpp:293` |
| Families | magic only; never physical or unblockable | - | `combat.json#absorb.perfect.notAgainst` |
| Damage removed | 100% | - | `combat.json#absorb.perfect.reduction` |
| Mana returned | 10 × incoming tier × (1 + 0.1 × Nerve); tier 0 returns 4 (11 per tier at Nerve 1), capped at max mana | mana | `combat.json#absorb.perfect.manaReturned` |
| Tier clock advance | 2.0 | s | `combat.json#absorb.perfect.tierClockAdvanceS` |
| Flow | +1 | stack | `combat.json#flow.perfectAbsorbGives` |
| Mirror B (Ripple, tier ≥ II) | a 7.5 dmg ring of r 3 m around the player | - | `Water.cpp:344-366` |
| Not possible while split-casting | the fresh window is cleared for a ward that is also casting | - | `ArenaThreats.cpp:294-297` |
| Window does not scale | same for everyone; Nerve scales drain and return only | - | `combat.json#absorb.twoAxis` |

Timing anchor in the build: the kernel's fresh tick is the tick on which the session first passes `bAbsorb` (the raise
event from the detector), not the detector's motion-onset time. The onset time is logged and not used to back-date the
raise (`ArenaSession.cpp:694-718`, `ArenaKernel.cpp:42`). The onset-timed resolver `FAbsorbResolver`
(`AbsorbResolver.cpp:327-347`) is exercised by the ward tests only.

The teach step for the ward and the perfect (slow glob, white ring, three tries) uses `teach.json`; see
[08-session-arc.md](08-session-arc.md).

## Blink

A flick toward a pad: a snap teleport between the three rune pads with the pinned roll's cost and invulnerability.

### Detector (shared with Bolt, `BlinkDetector.cpp:50-76`)

| Flick yaw (from body forward, clip space) | Result |
|---|---|
| within ±35° | Bolt |
| −120° < yaw < −35° | Blink left |
| 35° < yaw < 120° | Blink right |
| beyond ±120° | Blink back |

Pad resolution (`BlinkDetector.cpp:36-48`): left = active pad − 1, right = active pad + 1, clamped to 0..2; back = the
centre pad. A flick toward a pad that does not exist (left from the left pad) resolves to the active pad, and the blink
still happens in place with its cost and i-frames. Desktop: `A`, `D`, `S` play the three blink clips (synthetic yaws −60,
+60, 180, `CLIP-SCHEMA.md`). `F5`-`F7` are L3 debug snaps that bypass the kernel.

### Session and kernel

The session drops the ward on a blink (`ArenaSession.cpp:727-743`) and holds the request until the first tick the kernel
can accept a roll; if the timing is legal but stamina is short, the blink is dropped and logged
(`ArenaSession.cpp:1568-1575`). On that tick it sends one roll edge with zero move input, switches the active pad, and
re-pins the body after the step (CR-001 adapter; `ArenaSession.cpp:1600-1641`).

| Rule | Value | Unit | Source key |
|---|---|---|---|
| Stamina cost | 25 | stamina | `combat.json#roll.staminaCost` |
| Invulnerability | 0.25 (all hits, including unblockable, resolve to 0) | s | `combat.json#roll.iFramesS`; `ArenaThreats.cpp:280-283` |
| Busy time | 0.35 roll + 0.15 recovery: next blink no sooner than 0.50 s after the last | s | `combat.json#roll.durationS`, `#roll.recoveryS` |
| Stamina pool / regen | 70 / 20 per second after 0.8 s without spending | stamina | `stats.csv#vigor`, `combat.json#stamina` |
| Blocked by | root (Water root, Font cast, Fire root) | - | `ArenaKernel.cpp:76-78` |
| Side effects | interrupts a pending cast; drops a held ward | - | `ArenaKernel.cpp:95-101` |
| While the staff is planted | the blink lifts the staff and does not move or roll | - | `ArenaKernel.cpp:79-84` |
| Presentation | fade 0.12 s out, 0.2 s in, vignette | s | [01-body-and-space.md](01-body-and-space.md#blink-presentation-the-only-camera-motion) |

Enemies aim at the live kernel position, so they re-acquire the new pad; a projectile in flight keeps its point (CR-001).

## Seal (tier IV mudra)

| Item | Build |
|---|---|
| Clip | `mudra` exists (key `4`); CLIP-SCHEMA: both hands meet in front of the chest, palms facing, fingers up, held 0.60 s, pinch about 0.85 |
| Detector | none. `FStaffDetector` is written so a mudra does not pass as a staff plant (`StaffDetector.h:305-308`) |
| Kernel rule | none. A tier IV line casts from its sigil alone once the clock reaches tier IV (`Catalog.cpp:60`) |
| Effect of playing the clip | it uses the ward hand, so it stops a held ward clip (`HandInputSubsystem.h:8-13`); the scripted player also drops its ward when it plays it (`ArenaSession.cpp:592-598`) |

## Split hands and planted staff

Both are VR overlay rules on by default (`combat.vr.json#splitHands`, `#plantedStaff`); full rules, costs and status in
[06-school-defences.md](06-school-defences.md).

- **Split hands:** with the ward held and the staff not planted, Bolt and lines up to tier II cast at 0.6 power, build no
  Flow, and the ward cannot perfect; tier III-IV lines and the Crest are refused (`ArenaKernel.cpp:197-285`).
- **Planted staff:** both hands grip and thrust down (detector: both pinches ≥ 0.70, palms within 0.12 m across, 0.18 m
  of travel inside 1.5 s; `StaffDetector.cpp:8-14`). A dome for 6 s, 8 mana up front plus 4 mana/s. While planted a cast
  is a full two-hand cast, and a blink lifts the staff instead of moving.

## Status and gaps

| Verb | Status |
|---|---|
| Bolt | implemented |
| Line cast (sigil) | implemented |
| Ward and perfect | implemented (desktop timing; real hand latency is a November item) |
| Blink | implemented through the CR-001 roll adapter |
| Seal (mudra) | designed; clip only |
| Split hands, planted staff | implemented as VR overlay proposals, not signed off |

Plan versus build:

- **Bolt gesture.** PROJECT-PLAN §6.2: "open toward a target, then a quick pinch-snap" with a 60° soft-lock and head-gaze
  assist (`combat.json#aimMode`). Build: a forward index flick, aimed along the view; no soft-lock code exists in `Source/`.
- **Sigil inner stroke.** PROJECT-PLAN §6.2 says Tide Orb = dot, Lash = horizontal, Mirror = vertical. Superseded by
  DECISIONS 2026-10-03 (circle + bar). Build: vertical = Tide Orb, horizontal = Lash, diagonal = Mirror.
- **Mirror tier II.** PROJECT-PLAN §6.3 fixes Mirror II to Reflection (branch A) as "the signature VR moment". Build:
  the session uses the pinned Rotation preset, whose Mirror branch is B (Ripple); reflection never occurs for the player
  (`Water.cpp:371-372` requires branch A).
- **Tide Orb IV branch pick at the first intermission** (PROJECT-PLAN §6.3). Not built; Rotation fixes Tide Orb to B
  (Rain of Orbs). Lash II is B (Riptide), which matches the plan.
- **Ward arc direction.** PROJECT-PLAN §6.2 centres the arc on the palm normal. Build: on the kernel's facing, the
  direction to the view aim (the pinned "aim direction if aiming"); the detected palm facing is logged only.
- **Ward timing from motion onset.** PROJECT-PLAN §4 (D-G3) times the perfect from motion onset. The detector computes
  onset, but the session's kernel input uses the raise-event tick (see Perfect ward).
- **Seal.** PROJECT-PLAN §6.2: after the tier IV sigil, both hands form the mudra (palms forward, thumbs and index
  fingers forming a triangle) within 1.5 s. Build: no gate; and the clip's pose (palms facing each other, CLIP-SCHEMA)
  differs from the plan's triangle. BACKLOG notes the mudra glyph still needs a deliberate design.
- **Mouse bolt.** DESKTOP-INPUT lists "LMB click" for Bolt. No such binding exists in `MageArenaPlayerController.cpp`;
  the left button only draws.
- **Blink cost.** PROJECT-PLAN §6.2 puts the blink cost in overlay data "starting from roll's 25". The overlay has no blink
  field; the pinned roll values apply (CR-001, "Not in this request").

## Open questions

- Should a tier IV line wait for the mudra, and within what window? The 1.5 s in the plan is not in any data file.
- Should the perfect window be anchored to motion onset in the session (back-dating the kernel's fresh tick), and by how
  much at most? Not specified in data.
- Should a blink toward a non-existent pad be refused instead of rolling in place?
- Is Mirror meant to be branch A (Reflection) for the VR player? That needs a VR loadout instead of the pinned Rotation
  preset.
