---
title: School defences
channel: vr
status: partial
verified-against: 46643e2 (2026-10-07)
sources:
  - docs/DECISIONS.md
  - docs/design/SCHOOL-DEFENCES.md
  - docs/design-findings/DF-001-seated-wave1.md
  - docs/design-findings/DF-002-fire-duel-balance.md
  - docs/design-findings/DF-003-calibration-proposal.md
  - docs/change-requests/CR-002-fire-school.md
  - apps/vr/tasks/T11-split-hands-and-staff.md
  - apps/vr/tasks/T12-fire-wall-and-calibration.md
  - apps/vr/tasks/BACKLOG.md
  - apps/vr/Game/Source/MageArenaVR/Kernel/VrRules.h
  - apps/vr/Game/Source/MageArenaVR/Kernel/VrRules.cpp
  - apps/vr/Game/Source/MageArenaVR/Kernel/ArenaKernel.cpp
  - apps/vr/Game/Source/MageArenaVR/Kernel/ArenaThreats.cpp
  - apps/vr/Game/Source/MageArenaVR/Kernel/MageAI.cpp
  - apps/vr/Game/Source/MageArenaVR/Kernel/Fire.cpp
  - apps/vr/Game/Source/MageArenaVR/Gestures/StaffDetector.h
  - apps/vr/Game/Source/MageArenaVR/Gestures/StaffDetector.cpp
  - apps/vr/Game/Source/MageArenaVR/Hands/HandInputSubsystem.h
  - apps/vr/Game/Source/MageArenaVR/Hands/MageArenaPlayerController.cpp
  - apps/vr/Game/Source/MageArenaVR/Session/ArenaSession.cpp
  - apps/vr/Game/Source/MageArenaVR/Session/SessionPresentation.cpp
data:
  - apps/vr/data/vr/combat.vr.json#splitHands
  - apps/vr/data/vr/combat.vr.json#plantedStaff
  - apps/vr/data/vr/combat.vr.json#fireWall
  - apps/vr/data/vr/calibration-proposal.json#knobs.power
  - apps/vr/data/vr/calibration-proposal.json#knobs.defence
  - apps/vr/data/vr/calibration-proposal.json#measured
  - apps/vr/data/pinned/docs/design/baseline-fourteen-nights/design/data/combat.json#absorb
  - apps/vr/data/pinned/docs/design/baseline-fourteen-nights/design/data/schools.json
---

# School defences

The owner asked on 2026-10-03 for one seated defence per school, so a mage who cannot walk can survive while casting
(DECISIONS.md; design origin [SCHOOL-DEFENCES.md](../design/SCHOOL-DEFENCES.md)). This page is the as-built reference.
Two defences are built for the player, both as VR overlay rules in `combat.vr.json`: **A, split hands** (cast weaker spells
with the ward still up) and **E, planted staff** (a dome that frees both hands). **C, the fire wall**, is built for the AI
Fire mage only. **B, stone form** (Earth) and **D, air form** (Air) are designed and have no code. Every number below is
the live overlay value, which is a proposal: none is signed off. A calibration census (DF-003) wrote a different, also
unsigned, set to `calibration-proposal.json`, applied only with the `-MageArenaProposal` switch. The shared kernel and the
pinned data are untouched: without the overlay ruleset none of these rules run.

## Summary

| Id | Defence | School | For whom | Status | Live data |
|---|---|---|---|---|---|
| A | Split hands | all (Water first) | player | implemented (overlay proposal) | `combat.vr.json#splitHands` |
| E | Planted staff | staff mages (Water first) | player | implemented (overlay proposal) | `combat.vr.json#plantedStaff` |
| C | Fire wall | Fire | AI Fire mage only | partial | `combat.vr.json#fireWall` |
| B | Stone form | Earth | - | designed | none |
| D | Air form | Air | - | designed | none |

The player is always the Water mage on the Rotation preset in the build, so A and E are the player's defences.

## A - Split hands

### Gesture

Hold the ward with the off hand and cast with the casting hand at the same time. The hand pipeline plays one clip per
hand: the ward lane holds the ward hand while the action lane plays a sigil or bolt clip, and each detector reads its own
hand (`HandInputSubsystem.h:8-13`). Desktop: hold `Space` and press `1`-`3` or `Q`.

### Rule (`ArenaKernel.cpp:197-285`, `ArenaThreats.cpp:294-297`)

A cast is a split cast when the overlay is active, the player's ward is up, and the staff is not planted.

| Rule | Value | Unit | Source key |
|---|---|---|---|
| Damage multiplier | 0.6 | × spell damage | `combat.vr.json#splitHands.oneHandPower` |
| Highest tier allowed | 2 (Bolt is tier 0 and always allowed) | tier | `combat.vr.json#splitHands.maxTier` |
| Tier III-IV line | refused, logged `defence refuse tier` | - | `ArenaKernel.cpp:258-262` |
| Any split cast at Flow 5 (would be a Crest) | refused, logged `defence refuse crest` | - | `ArenaKernel.cpp:263-267` |
| Flow | no gain, no reset, no idle refresh | - | `ArenaKernel.cpp:268-269`, `Water.cpp:124-127` |
| Mana | the spell's normal cost | mana | `Water.cpp:85` |
| Ward drain while split | × 1 (live); the ward keeps draining 27 mana/s at Nerve 1 | multiplier | `calibration-proposal.json#knobs.defence.wardDrainSplit`; `ArenaKernel.cpp:56-59` |
| Perfect ward | impossible from the first accepted split cast until the ward is lowered | - | `ArenaThreats.cpp:294-297`; latch cleared in `ArenaKernel.cpp:489-492` |

The session checks the same refusals before sending the cast, so a refused split sigil never reaches the kernel
(`ArenaSession.cpp:623-637`). The "split-casting" latch is set by the first accepted split cast and stays on for the rest
of that ward hold, so one split Bolt costs every later perfect on that hold.

### Measurement

DF-001 (after T11): the seated Wave 1 was still lost, at 23.25 s, with one split cast; "the palm ward drains 27 mana/s at
Nerve 1, so warding while casting is nearly unaffordable". DF-003: the reference script almost never split-casts in the
Fire duel.

## E - Planted staff

### Gesture and detector (`StaffDetector.cpp:8-14`, `StaffDetector.cpp:38-95`)

Both hands grip as if holding a shaft and thrust down; the reverse lifts it.

| Gate | Value | Unit |
|---|---|---|
| Grip | both pinches ≥ 0.70 | pinch |
| Hands together | palm Y difference < 0.12 | m |
| Hands sampled together | timestamps within 0.05 | s |
| Plant | mean palm height falls ≥ 0.18 within a 1.5 s window | m, s |
| Lift | mean palm height rises ≥ 0.18 within 1.5 s | m, s |
| Repeat | one event per gesture until the grip breaks or the clip rewinds | - |

Desktop: `F` plays `staff-plant`, or `staff-lift` while planted (`MageArenaPlayerController.cpp:288`). A mudra does not
pass as a plant (`StaffDetector.h:305-308`). The staff gesture uses the ward hand, so it stops a held ward clip.

### Rule (`VrRules.cpp:1126-1167`, `ArenaThreats.cpp:299-307`, `ArenaKernel.cpp:79-84`, `ArenaKernel.cpp:197-201`)

| Rule | Value | Unit | Source key |
|---|---|---|---|
| Up-front cost | 8, paid on the plant tick; refused (`staff-mana`) if mana is short | mana | `combat.vr.json#plantedStaff.manaUpFront` |
| Drain | 4 per second, from the plant tick, while planted | mana/s | `combat.vr.json#plantedStaff.drainPerSecond` |
| Duration | 6 (360 ticks), then lifts | s | `combat.vr.json#plantedStaff.durationS` |
| Other lifts | the lift gesture; a blink attempt; mana running out | - | `VrRules.cpp:1132-1139`, `:1159-1166`; `ArenaKernel.cpp:79-84` |
| Magic reduction | 0.85 (a hit keeps 15%) | fraction removed | `combat.vr.json#plantedStaff.magicReduction` |
| Physical reduction | 0.6 (a 10-damage spear becomes 4) | fraction removed | `combat.vr.json#plantedStaff.physicalReduction` |
| Unblockable | 0 (passes) | - | `VrRules.cpp:1103-1118` |
| Direction | all round (not the ward's 140° arc) | - | `ArenaThreats.cpp:299-307` |
| With the ward | the larger of the two reductions applies; the dome never perfects, a ward perfect still counts | - | `ArenaThreats.cpp:299-307` |
| Casting while planted | full two-hand cast: full power, Flow builds, tiers III-IV and the Crest allowed | - | `ArenaKernel.cpp:199-201` |
| Blink while planted | refused: the blink edge lifts the staff and the player stays on the pad | - | `ArenaKernel.cpp:79-84` |
| Full 6 s plant | 8 + 24 = 32 | mana | `combat.vr.json#plantedStaff._drainPerSecond` |

Session detail: while planted, a cast drops the held ward (the session keeps the ward only for split casts,
`ArenaSession.cpp:689`, `ArenaSession.cpp:764`).

Presentation: a turquoise dome over the pad, shell alpha 0.10 and ribs alpha 0.80 (`SessionPresentation.cpp:756-757`,
committed file). BACKLOG notes one rib meridian runs up the forward line of sight.

### Measurement

DF-001 after T11: survival roughly doubled (lost at 23.25 s against 11.42 s); damage rate barely moved. DF-003: "The plant
is load-bearing": never planting loses the wave and shortens both duels.

## C - Fire wall

### Who raises it

Only an actor of the Fire school (`VrRules.cpp:988-1015`); in the build that is the AI Fire mage. The player has no fire
wall: the player is never a Fire actor and there is no sweep gesture, clip or detector.

AI trigger (`MageAI.cpp:538-608`): the mage raises the wall when a hostile magic or physical projectile's remaining path
would cross a wall in front of it, after its competence reaction delay, if it has the mana, unless it is saving mana for
Sunfall at tier IV or is casting Sunfall that tick. Raising replaces that tick's cast and ward.

### Rule (`VrRules.cpp:922-1057`, `ArenaThreats.cpp:165-210`, `ArenaThreats.cpp:249-257`)

| Rule | Value | Unit | Source key |
|---|---|---|---|
| Distance in front of the caster | 4 | m | `combat.vr.json#fireWall.distanceM` |
| Arc | 90 (45 either side of the caster's facing at the raise tick) | deg | `combat.vr.json#fireWall.arcDeg` |
| Duration | 3 (180 ticks) | s | `combat.vr.json#fireWall.durationS` |
| Cost | 12, once on the raise tick | mana | `combat.vr.json#fireWall.manaCost` |
| Walls per caster | one at a time | - | `VrRules.cpp:994-997` |
| Position | fixed where it was raised; it does not follow the caster | - | `VrRules.cpp:1007-1008` |
| Stops | magic and physical projectiles whose path crosses the arc line (a chord entering and leaving the ring counts) | - | `VrRules.cpp:922-962` |
| Passes | unblockable projectiles; the caster's own and its team's projectiles | - | `ArenaThreats.cpp:32-46`, `:170-171` |
| Heat per stopped projectile | 4, to the caster | Heat | `combat.vr.json#fireWall.heatPerStop` |
| Burn | 12 magic damage to an opposing body whose movement crosses the arc; a ward can reduce it | HP | `combat.vr.json#fireWall.burnDamage`; `VrRules.cpp:1017-1057` |

Presentation: a vermilion curtain (`SessionPresentation.cpp:858`).

### Measurement

DF-003: the first census measured a wall that destroyed the caster's own bolts; that bug was fixed (T12 retry 1). With the
fix, a longer, cheaper wall (`shade`: 12 s, 6 mana) "is a weapon aimed at the player". From a seat on the live overlay the
player wins 17 of 20 Fire duels at competence 1 and 12 of 20 at 1.5, and Sunfall is cast in none of those 40 duels.

## B - Stone form (Earth): designed only

From SCHOOL-DEFENCES B, not built: both fists clenched and drawn to the chest, held; the body turns to stone for a short
window (proposal 2.5 s), invulnerable, cannot ward or blink, may channel the heaviest Earth spell; paid with Footing. The
seated reading of Footing (builds while the mage stays on one pad) is a proposal awaiting the owner (BACKLOG). No Earth
school, data block or code exists in the build.

## D - Air form (Air): designed only

From SCHOOL-DEFENCES D, not built: both palms sweep outward; for a window (proposal 3 s) the body evades hits by chance
(proposal 60% physical, 30% magic, unblockables not evaded) and blink costs less; paid with Momentum. The seated reading of
Momentum (consecutive blinks and casting cadence) awaits the owner (BACKLOG). No Air school, data block or code exists.

## Calibration proposal (not live)

`calibration-proposal.json` (DF-003 set `wide`), applied only with `-MageArenaProposal` or `MageArenaProposal=1`
(`VrRules.cpp:536-544`, `VrRules.cpp:590-647`):

| Knob | Live overlay | Proposal | Unit |
|---|---|---|---|
| Split one-hand power | 0.6 | 1.0 | × damage |
| Ward drain while split | × 1 | × 1 | multiplier |
| Player mana regen | × 1 | × 1.25 | multiplier |
| Staff duration | 6 | 14 | s |
| Staff up-front | 8 | 8 | mana |
| Staff drain | 4 | 1.5 | mana/s |
| Staff magic reduction | 0.85 | 0.92 | fraction |
| Staff physical reduction | 0.6 | 0.90 | fraction |
| Fire wall distance | 4 | 4 | m |
| Fire wall arc | 90 | 150 | deg |
| Fire wall duration | 3 | 12 | s |
| Fire wall cost | 12 | 6 | mana |
| Fire wall Heat per stop | 4 | 4 | Heat |
| Fire wall burn | 12 | 12 | HP |

Measured with the proposal over 20 seeds (`calibration-proposal.json#measured`): `targetsMet` false; Wave 1 won inside
25-40 s on 0.85 of seeds; median HP left on wins 0.17; Fire duel medians 44.8 s (competence 1) and 40.6 s (1.5); Sunfall in
0.05 and 0.15 of duels. DF-003 recommends leaving it unsigned.

## Status and gaps

Built: A and E for the player, C for the AI Fire mage, the proposal switch and its census. Not built: B, D, and any
player-side fire wall.

Design versus build (SCHOOL-DEFENCES is the design; this page cites the code):

- **A, "the ward keeps draining mana as now"**: as designed, and DF-001 names that drain as the reason split casting is
  rarely affordable. The `wardDrainSplit` knob exists but is 1 in both the live overlay and the proposal.
- **A, "no perfect absorb on a ward that is also casting"**: built more strictly than the sentence reads; after one split
  cast the rest of that ward hold cannot perfect, even when the hand is no longer casting.
- **A, Crest refused**: built as a refusal of every split cast at Flow 5, including Bolt (design says the Crest needs both
  hands; it does not say a Bolt is refused).
- **E, gesture**: design "thrust down ~25 cm"; detector threshold 0.18 m (the synthetic clips move about 25 cm,
  `CLIP-SCHEMA.md`).
- **E, "both hands are free: the two-hand mudra (tier IV) and the Crest can be cast"**: tier IV and the Crest are allowed;
  the mudra has no detector, so tier IV needs no mudra in any case ([02-verbs-and-gestures.md](02-verbs-and-gestures.md#seal-tier-iv-mudra)).
- **C, "costs mana, and each projectile it stops gives Heat"**: as designed. "Enemies that cross take burn damage": built,
  but in the build only the seated player opposes the Fire mage and the player's position is pinned to a pad, so the burn
  has nothing to hit in the duel (derived, not measured).
- **C, player Fire later**: SCHOOL-DEFENCES says "player Fire later; Fire mage AI now". Still AI only.
- **Owner sign-off**: DECISIONS 2026-10-03 says the owner signs off the final numbers. No value on this page is signed off.

## Open questions

- Should the split-casting latch clear when the casting hand is idle again, so perfects return without lowering the ward?
- Is refusing a split Bolt at Flow 5 intended, or should only the Crest-boosted cast be refused (for example by casting the
  Bolt without consuming the Crest)?
- Footing and Momentum seated readings (BACKLOG, owner): needed before B and D can be built.
- Does the fire wall need a player-side gesture for the VR slice, given the slice's player is Water?
