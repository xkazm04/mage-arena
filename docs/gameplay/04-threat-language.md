---
title: Threat language
channel: vr
status: partial
verified-against: 1fb1b8b (2026-10-07); anchors into files changed since 46643e2 re-verified (none cited, or by symbol still valid)
sources:
  - docs/DECISIONS.md
  - docs/PROJECT-PLAN.md
  - docs/design-findings/DF-001-seated-wave1.md
  - docs/design-findings/DF-004-creatures-cannot-reach-the-seat.md
  - docs/change-requests/CR-002-fire-school.md
  - docs/change-requests/CR-003-threat-geometry.md
  - apps/vr/Game/Source/MageArenaVR/Session/SessionPresentation.cpp
  - apps/vr/Game/Source/MageArenaVR/Greybox/ThreatDemo.cpp
  - apps/vr/Game/Source/MageArenaVR/Kernel/ArenaThreats.cpp
  - apps/vr/Game/Source/MageArenaVR/Kernel/Fire.cpp
  - apps/vr/Game/Source/MageArenaVR/Kernel/Catalog.cpp
  - apps/vr/Game/Source/MageArenaVR/Kernel/KernelData.cpp
  - apps/vr/Game/Source/MageArenaVR/Kernel/VrRules.h
  - apps/vr/Game/Source/MageArenaVR/Combat/AbsorbResolver.h
  - apps/vr/Game/Source/MageArenaVR/Session/ArenaSession.cpp
data:
  - apps/vr/data/pinned/docs/design/baseline-fourteen-nights/design/data/combat.json#threatLanguage
  - apps/vr/data/pinned/docs/design/baseline-fourteen-nights/design/data/combat.json#absorb.reduction
  - apps/vr/data/pinned/docs/design/baseline-fourteen-nights/design/data/combat.json#absorb.perfect.notAgainst
  - apps/vr/data/pinned/docs/design/baseline-fourteen-nights/design/data/combat.json#reactionBands
  - apps/vr/data/pinned/docs/design/baseline-fourteen-nights/design/data/combat.json#roll.iFramesS
  - apps/vr/data/pinned/docs/design/baseline-fourteen-nights/design/data/enemies.json
  - apps/vr/data/pinned/reference-the-ledger/design/data/spells-fire.csv
  - apps/vr/data/vr/arena-layout.json#threats
  - apps/vr/data/vr/combat.vr.json#attacks
  - apps/vr/data/vr/combat.vr.json#plantedStaff
  - apps/vr/data/vr/combat.vr.json#gentle
---

# Threat language

Every incoming hit belongs to one of three kernel families, and each family has one correct answer and one look. **Magic**
(element colour, shrinking ground ring) is absorbed: face it with the ward, and a fresh raise just before impact is a
perfect. **Physical** (white or steel) is dodged: the ward only chips 30% off it, so the answer is a blink. **Unblockable**
(black core, red rim) is left: no ward or dome reduces it; only the blink's i-frames or being out of its area avoid it.
The rules are the pinned shared rules, used unchanged; the VR overlay only adds the dome's reductions and moves who throws
what. The greybox draws the colours and rings, but in the live bout every magic hit is drawn in the Water turquoise,
including the Fire mage's fire; the sound half of the "colour doubled by shape and sound" rule is not built.

## The three families

| Family | Pinned wording | Player answer | Ward reduction | Perfect possible | Dome reduction (overlay) | Blink i-frames |
|---|---|---|---|---|---|---|
| magic ("absorb") | "element colour, glow, shrinking ring: face it and absorb" | ward, timed for a perfect | 0.85 | yes | 0.85 | avoid (0 damage) |
| physical ("dodge") | "white/steel: roll" | blink | 0.30 | no | 0.6 | avoid |
| unblockable ("leave") | "black core, jagged red rim: leave or break with a staff strike" | blink off the pad / out of the area | 0 | no | 0 | avoid |

Sources: `combat.json#threatLanguage`, `#absorb.reduction`, `#absorb.perfect.notAgainst`, `#roll.iFramesS`;
`combat.vr.json#plantedStaff.magicReduction`, `#plantedStaff.physicalReduction`; `ArenaThreats.cpp:277-310`. A hit
outside the ward's 140° arc gets no ward reduction (`combat.json#absorb.reduction.outsideArc` = 0). The dome is not
directional. Where the ward and the dome both apply, the larger reduction wins (`ArenaThreats.cpp:299-307`).

"Break with a staff strike" is not available seated: enemies are held off the dais, and the session does not turn a bolt
into the pinned staff strike (`ArenaSession.cpp:753-757`).

## Steel chip

What a physical hit costs the seated player, by defence (derived from the reductions above and the pinned damage rows):

| Threat | Raw damage | Ward up (×0.70) | Dome (×0.40) | Blink in i-frames | Source |
|---|---|---|---|---|---|
| Conscript thrown spear | 10 | 7 | 4 | 0 | `combat.vr.json#attacks[0].damage` (pinned `spear_lunge`) |
| Slinger stone | 7 | 4.9 | 2.8 | 0 | `enemies.json#slinger.sling_stone.damage` |
| Cinder hound bite (melee; cannot reach the seat) | 6 | 4.2 | 2.4 | 0 | `enemies.json#cinder_hound.bite.damage` |

A fresh ward never perfects steel. DF-001 measured that four conscripts' spears overwhelm a seated ward; the defences on
[06-school-defences.md](06-school-defences.md) are the response.

## Look and shape

### Colours (linear RGB)

| Class | Body | Rim | Ground telegraph | Source key |
|---|---|---|---|---|
| Water (magic) | turquoise (0.009721, 0.745404, 0.745404) | none | turquoise | `arena-layout.json#threats.order[water]` |
| Fire (magic) | vermilion (0.760525, 0.05448, 0.013702) | none | vermilion | `arena-layout.json#threats.order[fire]` |
| Steel (physical) | steel grey (0.68, 0.68, 0.68) | none | steel | `arena-layout.json#threats.order[steel]` |
| Unblockable | near black (0.006995, 0.006995, 0.008023) | red (0.745404, 0.014444, 0.009134) | red | `arena-layout.json#threats.order[unblockable]` |

### Shapes in the layout threat set (greybox demo, `ThreatDemo.cpp`)

| Class | Shape | Size | Flight time | Source key |
|---|---|---|---|---|
| Water | sphere | r 0.38 m | 1.6 s | `arena-layout.json#threats.order[0]` |
| Fire | sphere | r 0.22 m | 1.2 s | `arena-layout.json#threats.order[1]` |
| Steel | elongated shaft | 1.7 m long, r 0.11 m | 1.2 s | `arena-layout.json#threats.order[2]` |
| Unblockable | sphere with a rim torus | r 0.46 m, rim scale 1.18 | 2.0 s | `arena-layout.json#threats.order[3]`, `#threats.rimScale` |
| All | shrinking ground ring at the landing point | radius 1.7 m to 0.28 m | - | `arena-layout.json#threats.telegraphStartRadiusM`, `#telegraphEndRadiusM` |

Projectiles fly at chest height, 1.35 m above the sand (`arena-layout.json#threats.chestHeightAboveSandM`).

### What the live bout draws (`SessionPresentation.cpp`, committed file)

| Kernel object | Drawn as | Lines |
|---|---|---|
| Colour rule | player-owned or `magic` → Water turquoise; `unblockable` → black body; anything else → steel | `:66-81` |
| Conscript spear (enemy projectile with an aim point, owner `conscript`) | steel shaft sized from the overlay (1.7 m, r 0.11 m), on a presentation arc cresting 0.75 m above chest height; steel ring 1.6 m across at its aim point | `:1160-1196` |
| Other projectiles | sphere, diameter max(18 cm, 2 × kernel radius), family colour, no landing ring | `:1197-1205`, `:1215-1222` |
| Ground telegraph (area, melee, projectile windup) | disc in family colour shrinking from 150 cm to 28 cm radius over the windup; unblockable adds a red rim at 2.2 × the radius | `:1302-1334` |
| Lane telegraph | strip at least 1.8 m wide in family colour; unblockable adds a red rim | `:1262-1300` |
| Magic area telegraph (for example a hound's death burst) | white bead ring as the perfect-window cue | `:1335-1340` |
| Fire wall | vermilion curtain (alpha 0.88) | `:858` |

The kernel has no `fire` family: Fire spells resolve as `magic` (`Fire.cpp:38-41`; CR-002). So in the bout the Fire mage's
bolts, flick, lance and rings are drawn in Water turquoise; only the fire wall is drawn vermilion.

### Sound

None. There is no audio playback code under `apps/vr/Game/Source/`. The teach logs `cue bell perfect-absorb` to the
chain (`ArenaSession.cpp:1455-1460`, committed file) but plays nothing.

## Reaction bands

| Band | Value | Unit | Source key | Enforced in the build |
|---|---|---|---|---|
| Minimum positional telegraph | 0.40 | s | `combat.json#reactionBands.minimumTelegraphPositionalS` | spell-catalog lint for zones (`Catalog.cpp:148-151`) |
| Minimum unblockable telegraph | 0.80 | s | `combat.json#reactionBands.minimumTelegraphUnblockableS` | spell-catalog lint (`Catalog.cpp:144-147`) |
| Player not downed before | 5.0 | s into a bout | `combat.json#reactionBands.playerDeathNotBeforeS` | not read by the C++ kernel |
| Gentle mode | opponent telegraph windups × 2 | multiplier | `combat.vr.json#gentle.telegraphMult`; `VrRules.h:190-197` | yes, when the player turns Gentle on |

Seated threat timings that matter (pinned unless marked):

| Threat | Family | Telegraph / windup | Speed | Source |
|---|---|---|---|---|
| Conscript spear (VR throw) | physical | 0.6 s windup | 14 m/s, range 9 m (overlay) | `combat.vr.json#attacks[0]` |
| Slinger stone | physical | 0.5 s windup, 2.2 s cooldown | 14 m/s | `enemies.json#slinger` |
| Mire maw bog glob | magic, tier 1 | 0.7 s windup | 9 m/s | `enemies.json#mire_maw.bog_glob` |
| Mire maw tongue pull | physical, line telegraph | 1.0 s windup | - | `enemies.json#mire_maw.tongue_pull` |
| Cinder hound death burst | magic, tier 0, r 1.5 m | 0.6 s delay | - | `enemies.json#cinder_hound.onDeath` |
| Fire mage Sunfall | unblockable, r 3.5 m, 95 dmg | rooted 0.8 s cast, then 1.2 s shadow | - | `spells-fire.csv` row `fire_sunfall`; CR-002 |

Enemy behaviour and the full rosters are on [07-enemies-and-ai.md](07-enemies-and-ai.md).

## Status and gaps

Built: the three families and their reductions; the steel chip; the dome's added reductions (overlay); unblockable passing
ward and dome; blink i-frames against every family; the colour rule and shrinking rings in greybox; the red rim on
unblockable telegraphs; the white perfect-window ring on magic area telegraphs; Gentle's doubled windups.

Plan versus build:

- **Element colour.** DECISIONS 2026-10-02 and PROJECT-PLAN §6.3: element colour means absorb. Build: every `magic` hit is
  drawn in Water turquoise (`SessionPresentation.cpp:66-81`), so the Fire mage's absorbable fire is turquoise. The layout
  data and the greybox threat demo do carry a vermilion fire class.
- **Shape and sound doubling.** PROJECT-PLAN §6.7: magic has a ring, steel is angular, black-core has a rim plus a low drone.
  Build: rings and the rim exist; a slinger stone is a sphere, not angular; there is no drone or any other sound.
- **Steel ring.** Only the thrown spear gets a landing ring; a slinger stone and the Fire mage's bolts fly without one.
- **Player-death floor.** `reactionBands.playerDeathNotBeforeS` (5 s) is pinned but not loaded or enforced by the C++
  kernel (no reference outside the pinned file).
- **"Break with a staff strike."** Part of the pinned unblockable rule; not reachable seated.
- **Perfect practice in bout 2.** `enemies.json` says hounds teach "perfect absorb on death bursts"; from the seat the
  bursts never reach the player (DF-004, open). The mire maw's bog glob is the only magic projectile in that bout.

## Open questions

- Sunfall's radius (3.5 m plus the 0.38 m body radius) is larger than the 3.0 m pad spacing. If the meteor is aimed at
  the centre pad, both side pads are inside it, so moving one pad does not leave it; only i-frames timed on the impact
  tick, or the far side pad from a side-pad aim, avoid it. Derived from `spells-fire.csv`, `arena-layout.json#pads` and
  `ArenaThreats.cpp:98-101`; not measured. Is "leave" meant to be achievable from every pad?
- Should the kernel carry an element tag (or the presentation read the owner's school) so fire reads as fire?
- What should the steel and black-core sounds be, and where do they come from (ElevenLabs is the approved source)?
