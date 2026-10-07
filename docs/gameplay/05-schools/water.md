---
title: Water school (as built)
channel: vr
status: partial
verified-against: 1fb1b8b (2026-10-07); anchors into files changed since 46643e2 re-verified
sources:
  - "docs/DECISIONS.md"
  - "docs/PROJECT-PLAN.md"
  - "docs/design/SCHOOL-DEFENCES.md"
  - "docs/design-findings/DF-001-seated-wave1.md"
  - "docs/design-findings/DF-003-calibration-proposal.md"
  - "apps/vr/Game/Source/MageArenaVR/Kernel/Catalog.cpp"
  - "apps/vr/Game/Source/MageArenaVR/Kernel/Water.cpp"
  - "apps/vr/Game/Source/MageArenaVR/Kernel/KernelData.cpp"
  - "apps/vr/Game/Source/MageArenaVR/Kernel/Games.cpp"
  - "apps/vr/Game/Source/MageArenaVR/Session/ArenaSession.cpp"
  - "apps/vr/Game/Source/MageArenaVR/Session/SessionFlow.cpp"
  - "apps/vr/tools/clipgen/generate.mjs"
data:
  - "apps/vr/data/pinned/docs/design/baseline-fourteen-nights/design/data/spells-water.csv"
  - "apps/vr/data/pinned/docs/design/baseline-fourteen-nights/design/data/combat.json (tierClock, flow, absorb, lines)"
  - "apps/vr/data/pinned/docs/design/baseline-fourteen-nights/design/data/schools.json (schools[water])"
  - "apps/vr/data/pinned/docs/design/baseline-fourteen-nights/design/data/stats.csv"
  - "apps/vr/data/pinned/packages/core/src/arena/data/runtime.json (water, games.referencePreset, games.opponentPresets)"
  - "apps/vr/data/vr/combat.vr.json (splitHands, plantedStaff)"
  - "apps/vr/data/vr/calibration-proposal.json (knobs.power, knobs.defence)"
---

# Water school (as built)

Water is the only school the player can cast in the VR build. All five Water lines (Tide Orb, Lash, Mirror, Mire, Mend)
plus the Bolt are ported into the C++ kernel from the pinned TypeScript kernel and read every number from the pinned
`spells-water.csv`; nothing is retyped. The player always plays the pinned `Rotation` preset: Tide Orb, Lash and Mirror,
with **branch B on every line** (Riptide, Ripple, Rain of Orbs). The plan's intermission branch pick and its fixed
Mirror II = Reflection are not built. A line casts the tier the tier clock has unlocked, never a lower one. Two VR-only
overlay rules sit on top: split hands (one-hand casting while the ward is held) and the planted staff. Water is also the
school of the opponent mages in the default session (pinned "water proxy" duelists).

## Player loadout in the VR session

| Slot | Gesture (sigil) | Line | Branch in use | Source |
|---|---|---|---|---|
| 0 | bolt flick (pinch-snap clip `bolt`, key Q) | Bolt (Rain Needle) | - | `ArenaSession.cpp` `HandleBolt`; `Catalog.cpp` `SpellFor` slot 0 |
| 1 | circle + vertical bar (`sigil-line1`, key 1) | Tide Orb | B at tier IV: Rain of Orbs | `runtime.json` `water.presets[Rotation]`; `generate.mjs:63` |
| 2 | circle + horizontal bar (`sigil-line2`, key 2) | Lash | B at tier II: Riptide | same; `generate.mjs:64` |
| 3 | circle + diagonal bar (`sigil-line3`, key 3) | Mirror | B at tier II: Ripple | same; `generate.mjs:65` |

- The composition is chosen in code by name: `FindRotationPreset()` (`SessionFlow.cpp:50-60`) for the arc and the teach,
  `FindRotation()` in `FArenaSession::Start` (`ArenaSession.cpp:239`) for every bout.
- Rotation (`runtime.json` line 30): `lines ["tide_orb", "lash", "mirror"]`, `branches {"lash": "B", "mirror": "B", "tide_orb": "B"}`.
- Gesture detail and recognition: see [02-verbs-and-gestures.md](../02-verbs-and-gestures.md).

## How a line resolves to a spell

| Rule | Value | Source |
|---|---|---|
| Line slots | 3 | `combat.json` `lines.slots` |
| Branch tiers | II and IV | `combat.json` `lines.branchAtTiers` [2, 4] |
| Spell cast by a line | the row with `line`, `tier == actor's unlocked tier`, and an empty branch or the composition's branch | `Catalog.cpp` `SpellFor` |
| Tier unlocks | 0 / 15 / 30 / 45 s from bout start | `combat.json` `tierClock.unlockAtSeconds` |
| Perfect absorb advances the clock | 2.0 s | `combat.json` `tierClock.perfectAbsorbAdvanceS` |
| Minimum gap between unlocks | 6.0 s | `combat.json` `tierClock.minimumSecondsBetweenUnlocks` |
| Clock reset | every bout | `combat.json` `tierClock.resetsEachWave`; `ArenaKernel.cpp` `ResetWave` |
| Passive rows (Mirror II) | cannot be cast; active while slotted at tier >= 2 | `Water.cpp` `TrySpellCast` (`Kind == "passive"` refused) and `WaterAbsorbed` |

Tier clock, Flow and the cuff display are described in [03-tier-clock-and-flow.md](../03-tier-clock-and-flow.md).

## Flow (Water's resource)

| Rule | Value | Unit | Source |
|---|---|---|---|
| Range | 0-5 | stacks | `combat.json` `flow.max`; `schools.json` water `combatIdentity.range` |
| Gain | +1 when a different line is cast within 2.0 s | s | `flow.differentLineWithinS` |
| Per-stack damage | +0.06 per stack | multiplier | `flow.perStackDamage` |
| Reset | same line twice, or 3.0 s with no cast | s | `flow.resetOnSameLineTwice`, `flow.resetAfterIdleS` |
| Crest at 5 | next spell costs 0 mana and deals x1.5 | multiplier | `flow.crest` |
| Perfect absorb | +1 Flow | stacks | `flow.perfectAbsorbGives` |
| Split-hand casts | build no Flow; a Crest is refused while the ward is held | - | `combat.vr.json` `splitHands`; `ArenaSession.cpp` `WouldSplitRefuse` |

## Spell numbers by line and tier

All rows are verbatim from `spells-water.csv` at pin `baeac66`. Units: seconds, mana, HP, metres. "Blockable" maps to
the kernel family: `absorbable` = magic, `UNBLOCKABLE` = unblockable, `n/a` = no hit.

### Bolt (always on slot 0)

| Tier | Name | Shape | cast_s | cooldown_s | mana | damage | blockable | telegraph_s | range_m |
|---|---|---|---|---|---|---|---|---|---|
| 0 | Rain Needle | projectile 20 m/s | 0.00 | 0.25 | 3 | 2.05 | absorbable | 0.00 | 12 |

The CSV note says that inside 1.6 m the bolt button becomes a staff strike. The VR session instead **suppresses** a bolt
flick when the nearest living enemy is within `staffStrike.rangeM` + 0.05 m (`ArenaSession.cpp` `HandleBolt`); there is
no staff strike in VR.

### Tide Orb (player slot 1)

| Tier | Branch | Name | Shape | cast_s | cooldown_s | mana | damage | blockable | telegraph_s | range_m | In the VR loadout |
|---|---|---|---|---|---|---|---|---|---|---|---|
| I | - | Bubble Shot | projectile 12 m/s | 0.15 | 2.5 | 10 | 7 | absorbable | 0.15 | 12 | yes |
| II | - | Crash Orb | projectile 12 m/s; bursts r 2 m | 0.20 | 4.0 | 16 | 12 | absorbable | 0.20 | 12 | yes |
| III | - | Twin Tides | two projectiles 12 m/s at +-10 deg | 0.25 | 5.0 | 24 | 11 each | absorbable | 0.25 | 12 | yes |
| IV | A | Leviathan Orb | slow projectile 6 m/s r 1.5 m | 0.60 | 14.0 | 45 | 40 | UNBLOCKABLE | 1.00 | 14 | no (opponent preset Undertow) |
| IV | B | Rain of Orbs | 5-orb fan 50 deg | 0.40 | 10.0 | 40 | 9 each | absorbable | 0.40 | 10 | **yes (fixed)** |

Rain of Orbs projectiles fly at `runtime.json` `water.fanSpeedMps` = 12 m/s (the CSV row gives no speed).

### Lash (player slot 2)

| Tier | Branch | Name | Shape | cast_s | cooldown_s | mana | damage | blockable | telegraph_s | range_m | In the VR loadout |
|---|---|---|---|---|---|---|---|---|---|---|---|
| I | - | Tide Lash | cone 90 deg x 3 m | 0.10 | 2.0 | 8 | 8 | absorbable | 0.10 | 3 | yes |
| II | A | Undertow | cone 3 m + pull 3 m | 0.20 | 5.0 | 14 | 9 | absorbable | 0.20 | 3 | no |
| II | B | Riptide | cone 3 m + push 3 m | 0.20 | 5.0 | 14 | 9 | absorbable | 0.20 | 3 | **yes (fixed)** |
| III | - | Maelstrom Lash | ring 360 deg r 3 m | 0.30 | 8.0 | 22 | 15 | absorbable | 0.30 | 3 | yes |
| IV | - | Drown Coil | grab 2.5 m; root 1.5 s | 0.40 | 16.0 | 40 | 25 | UNBLOCKABLE | 0.80 | 2.5 | yes |

Every Lash row reaches 2.5-3 m from the caster. Enemies are held outside the dais footprint plus 0.6 m
(`combat.vr.json` `daisNoEntry`), and the centre pad is 2.9 m behind the dais front face, so from the centre pad the
nearest enemy body is about 3.5 m away (DF-004 arithmetic). Lash therefore rarely reaches anything from a seat; this
is inferred from the geometry and has not been measured separately.

### Mirror (player slot 3)

| Tier | Branch | Name | Shape | cast_s | cooldown_s | mana | damage | blockable | telegraph_s | range_m | In the VR loadout |
|---|---|---|---|---|---|---|---|---|---|---|---|
| I | - | Sheen | self 3 s: absorbed magic refunds +50% mana | 0.10 | 8.0 | 8 | 0 | n/a | 0.00 | 0 | yes |
| II | A | Reflection | passive: a perfect absorb reflects the projectile (only projectiles of tier <= own unlocked tier) | 0.00 | 0.0 | 0 | as reflected | n/a | 0.00 | 0 | no (opponent preset Mirror tide) |
| II | B | Ripple | passive: a perfect absorb releases a ring r 3 m | 0.00 | 0.0 | 0 | 7.5 | absorbable | 0.00 | 3 | **yes (fixed)** |
| III | - | Mirage | decoy for 3 s draws soft-lock | 0.20 | 12.0 | 20 | 0 | n/a | 0.00 | 6 | yes |
| IV | - | Return Tide | release stored absorbed damage as a wave (max 40) | 0.30 | 18.0 | 20 | stored (max 40) | absorbable | 0.30 | 8 | yes |

- Return Tide stores 50% of damage absorbed since the last release (CSV note, parsed into `StoredFraction`,
  `KernelData.cpp:630`); the wave travels at `water.returnWaveSpeedMps` 12 m/s with radius `water.returnWaveRadiusM` 1 m.
- Mirage aims the decoy as a point; opponent mages aim at the decoy while it exists (`MageAI.cpp` `MageInput`).
- A perfect absorb is possible only against magic (`combat.json` `absorb.perfect.notAgainst` = unblockable, physical).
  Bout 1 has no magic threats, so Ripple cannot fire there.

### Mire and Mend (not in the player loadout; used by opponent presets)

| Line | Tier | Name | Shape | cast_s | cooldown_s | mana | damage | blockable | telegraph_s | range_m |
|---|---|---|---|---|---|---|---|---|---|---|
| mire | I | Puddle | ground r 2.5 m; slow 30% for 4 s | 0.20 | 6.0 | 10 | 0 | n/a | 0.40 | 10 |
| mire | II | Fog Bank | ground r 3 m for 5 s; blocks line of sight; breaks soft-lock | 0.30 | 10.0 | 18 | 0 | n/a | 0.40 | 10 |
| mire | III | Freeze Field | ground r 3 m; root 1.0 s | 0.30 | 12.0 | 28 | 5 | absorbable | 0.80 | 10 |
| mire | IV | Glacier Tomb | encase one target 2 s (immune and silenced) | 0.50 | 20.0 | 40 | 0 | n/a | 0.80 | 8 |
| mend | I | Cool Draught | self heal 12 | 0.40 | 8.0 | 12 | heal 12 | n/a | 0.00 | 0 |
| mend | II | Tide Ward | self: absorb drain -50% for 4 s | 0.20 | 12.0 | 15 | 0 | n/a | 0.00 | 0 |
| mend | III | Spring | self heal 30 over 5 s | 0.40 | 16.0 | 25 | heal 30 over 5 s | n/a | 0.00 | 0 |
| mend | IV | Font | self: restore 60 mana (rooted while casting) | 0.60 | 30.0 | 0 | 0 | n/a | 0.00 | 0 |

Opponent compositions (`runtime.json` `water.presets`, chosen by `games.opponentPresets` in `Games.cpp:99-101`):

| Tiro bout | Preset | Lines | Branches |
|---|---|---|---|
| 3 (semifinal, competence 1) | Undertow | lash, tide_orb, mire | lash A (Undertow), mirror A, tide_orb A (Leviathan Orb) |
| 4 (final, competence 1.5) | Mirror tide | mirror, mend, tide_orb | lash A, mirror A (Reflection), tide_orb B (Rain of Orbs) |

## The player's pools (ranks 1)

Default ranks are vigor 1, focus 1, nerve 1 (`runtime.json` `training.defaultRanks`); formulas from `stats.csv`.

| Quantity | Formula | Value at rank 1 | Unit |
|---|---|---|---|
| Max HP | 80 + 15 x vigor | 95 | HP |
| Max stamina | 60 + 10 x vigor | 70 | stamina |
| Max mana | 80 + 15 x focus | 95 | mana |
| Mana regen | 6 + 1.5 x focus | 7.5 | mana/s |
| Ward drain while held | 30 - 3 x nerve | 27 | mana/s |
| Perfect mana return | 10 x incoming tier x (1 + 0.1 x nerve); tier 0 returns 4 | 11 per incoming tier | mana |

## VR overlay rules that change Water casting

| Rule | Live value | Proposal value | Unit | Source key | Status |
|---|---|---|---|---|---|
| One-hand power (ward held, staff not planted) | 0.6 | 1.0 | damage multiplier | `combat.vr.json` `splitHands.oneHandPower`; `calibration-proposal.json` `knobs.defence.oneHandPower` | proposal, not signed off |
| Highest tier castable one-handed | II | (not in proposal) | tier | `splitHands.maxTier` | proposal, not signed off |
| Planted staff lifts the split gate (both hands free) | yes | yes | - | `combat.vr.json` `plantedStaff` | proposal, not signed off |
| Player bolt / line damage | 1 / 1 | 1 / 1 | multiplier | `knobs.power.boltDamage`, `lineDamage` | proposal only |
| Player mana regen | 1 | 1.25 | multiplier | `knobs.power.manaRegen` | proposal only |

The dome numbers and the split-hand gestures are in [06-school-defences.md](../06-school-defences.md). The proposal file
is applied only with `-MageArenaProposal` (see [10-data-reference.md](../10-data-reference.md)).

## Status and gaps

Built:
- All 24 rows of `spells-water.csv` (the Bolt plus 23 line rows) in the C++ kernel, verified by the 45 conformance vectors (lash-1..4, mirror-1..4,
  tide-orb-1..4b, mire-1..4, mend-1..4 and others in `apps/vr/data/conformance/`).
- Seated loadout Rotation with sigils for three lines; split hands and planted staff on the live overlay.

Plan versus build:
- **Mirror II.** Plan section 6.3: "Mirror's tier II is fixed to Reflection" (the "signature VR moment"). Build: the
  Rotation preset has `mirror: "B"`, so the player gets **Ripple**, not Reflection (`runtime.json:30`,
  `SessionFlow.cpp:50-60`). Reflection exists in the kernel (`Water.cpp` `ReflectProjectile`) but only the Mirror tide
  opponent has it.
- **Tide Orb IV branch pick.** Plan section 6.3: the player picks Leviathan Orb or Rain of Orbs at the first
  intermission. Build: no branch pick exists; Rain of Orbs (B) is fixed. Leviathan Orb, the plan's "unblockable
  finisher", is not available to the player.
- **Lash II.** Plan: fixed to Riptide. Build: matches (Rotation `lash: "B"`).
- **Seal (tier IV mudra).** Plan section 6.2: tier IV is released by a two-hand mudra within 1.5 s after the sigil.
  Build: a `mudra` clip exists (key 4), but no session code consumes it; tier IV casts from the sigil alone
  (`ArenaSession.cpp` `PlayAction` only drops the ward for `mudra`).
- **Sigil strokes.** Plan section 6.2: Tide Orb = dot, Lash = horizontal slash, Mirror = vertical stroke. Owner
  decision 2026-10-03 made circle + bar canonical; the build maps line 1 = vertical, line 2 = horizontal,
  line 3 = diagonal (`generate.mjs:63-65`). The plan's per-line stroke table is superseded.
- **Bolt in melee.** CSV: inside 1.6 m the bolt becomes a staff strike. Build: the bolt is suppressed instead
  (`HandleBolt`). Not a numeric change, but a rule difference.
- `runtime.json` `water.decoyRadiusM` (0.5) and `water.reportCastCadenceS` are not read by the C++ kernel (no match in
  `Kernel/*.cpp`).

## Open questions

- Should the player's Mirror II become Reflection (a VR-specific composition, which would need a VR-owned preset, since
  the pinned presets cannot be edited), and should the Tide Orb IV branch pick be built at the first intermission?
- Is the tier IV seal (mudra) still wanted, now that split hands and the planted staff gate tier III-IV differently?
- Lash's 3 m reach versus the ~3.5 m minimum enemy distance from a seat: is Lash a useful seated line at all?
