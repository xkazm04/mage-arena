---
title: Enemies and AI
channel: vr
status: partial
verified-against: 1fb1b8b (2026-10-07); anchors into files changed since 46643e2 re-verified
sources:
  - "docs/DECISIONS.md"
  - "docs/PROJECT-PLAN.md"
  - "docs/design-findings/DF-001-seated-wave1.md"
  - "docs/design-findings/DF-003-calibration-proposal.md"
  - "docs/design-findings/DF-004-creatures-cannot-reach-the-seat.md"
  - "docs/change-requests/CR-001-blink-and-pads.md"
  - "docs/change-requests/CR-003-threat-geometry.md"
  - "docs/schools/FIRE.md"
  - "apps/vr/Game/Source/MageArenaVR/Kernel/Games.h"
  - "apps/vr/Game/Source/MageArenaVR/Kernel/Games.cpp"
  - "apps/vr/Game/Source/MageArenaVR/Kernel/Enemies.cpp"
  - "apps/vr/Game/Source/MageArenaVR/Kernel/MageAI.cpp"
  - "apps/vr/Game/Source/MageArenaVR/Kernel/VrRules.h"
  - "apps/vr/Game/Source/MageArenaVR/Kernel/VrRules.cpp"
  - "apps/vr/Game/Source/MageArenaVR/Kernel/Training.h"
  - "apps/vr/Game/Source/MageArenaVR/Session/ArenaSession.cpp"
  - "apps/vr/Game/Source/MageArenaVR/Session/SessionFlow.cpp"
  - "apps/vr/tasks/T07-mage-ai-and-games.md"
  - "apps/vr/tasks/T15-creatures-bout-and-session-chain.md"
data:
  - "apps/vr/data/pinned/docs/design/baseline-fourteen-nights/design/data/enemies.json (soldiers, creatures, mages)"
  - "apps/vr/data/pinned/docs/design/baseline-fourteen-nights/design/data/arena-tiers.json (tiers[tiro].waves)"
  - "apps/vr/data/pinned/packages/core/src/arena/data/runtime.json (games, training)"
  - "apps/vr/data/vr/combat.vr.json (daisNoEntry, attacks, movement, narrowFov, gentle)"
  - "apps/vr/data/vr/teach.json (dummyHp, dummyDistanceM, wardDistanceM)"
---

# Enemies and AI

Every opponent in the VR build is a kernel actor driven by the pinned rules: soldiers and creatures by the enemy brain
(`Kernel/Enemies.cpp`, a port of `enemies.ts`), mages by the mage AI (`Kernel/MageAI.cpp`, a port of `mage-ai.ts`), and
the bout sequence by the Games phase machine (`Kernel/Games.*`, a port of `games.ts`). The session plays the **Tiro**
tier: soldiers, creatures, a competence-1 semifinal and a competence-1.5 final. The VR overlay (`combat.vr.json`)
changes one thing about all of them: **enemies stay on the arena floor and attack from range; nothing enters the dais**
(owner, DF-001 option A). That rule makes the conscript throw its spear, and it is also why the cinder hounds cannot
reach a seated player (DF-004, open). Mages aim at the player's live position, which the session pins to the active
pad, so they re-aim at the new pad on their next decision after a blink.

## The Tiro bouts the session plays

`Games.cpp` always plays `arena-tiers.json` tier index 0 (Tiro).

| Bout (wave index) | Kind | Spawns | Total HP | Target duration | Source |
|---|---|---|---|---|---|
| 1 (0) | soldiers | 4 conscript, 2 slinger | 208 | 25-40 s | `tiers[0].waves[0]` |
| 2 (1) | creatures | 3 cinder_hound, 1 mire_maw | 175 | 30-45 s | `tiers[0].waves[1]` |
| 3 (2) | semifinal | 1 mage, competence 1 | 95 (ranks 1) | 45-70 s | `tiers[0].waves[2]` |
| 4 (3) | final | 1 mage, competence 1.5 | 95 (ranks 1) | 45-80 s | `tiers[0].waves[3]` |

- Spawn positions: `OpeningPosition` around `runtime.json` `games.enemySpawn` (24, 10) with jitter +-0.8 m
  (`games.spawnJitterM`), in kernel metres (`Games.cpp` `SpawnWave`). In narrow view they are compressed into the arc
  (see below).
- Mage HP: mages use the same ranks as the player (`training.defaultRanks`), so 80 + 15 = 95 HP.
- Payout on a result: `payoutGold` [20, 40, 60, 100] and `renown` [5, 10, 15, 25] by waves cleared (`GamesResult`);
  the VR build computes them but shows neither.

## Soldiers

| Enemy | HP | Speed | Attack (as played in VR) | Damage | Windup | Cadence | Range | Family | Source |
|---|---|---|---|---|---|---|---|---|---|
| Hastatus conscript | 38 | 3.8 m/s | **thrown spear** (overlay; pinned is a 2.0 m lunge) | 10 | 0.6 s | windup 0.6 + recovery 0.5, then "back off 1 s" (about 2.1 s per throw) | 9 m (overlay), projectile 14 m/s | physical | `enemies.json` `soldiers[conscript]`; `combat.vr.json` `attacks[0]` |
| Funditor (slinger) | 28 | 3.6 m/s | sling stone (pinned, unchanged) | 7 | 0.5 s | cooldown 2.2 s | `games.rangedRangeM` 24 m, projectile 14 m/s; keeps 8-10 m | physical | `enemies.json` `soldiers[slinger]`; `combat.vr.json` `attacks[1]` |

- A ward reduces physical hits to 70% (`combat.json` `absorb.reduction.physical` 0.30): spear 10 -> 7, stone 7 -> 4.9.
  Physical hits never perfect. Blink i-frames (the pinned roll, 0.25 s) avoid them; see
  [04-threat-language.md](04-threat-language.md).
- The conscript keeps walking to the hold line during its back-off instead of standing still
  (`combat.vr.json` `movement.throwerClosesToLip` true).
- The overlay loader refuses a conscript entry whose damage or windup differs from the pinned row, or whose replaced
  `rangeM` / `projectileMps` do not match the pinned values (`ReadThrow`, `VrRulesLoad.cpp:221-319`).
- Not spawned in Tiro, but ported: Scutum bearer (`shieldman`, 70 HP, front block 120 deg) and Net-thrower (`netter`,
  45 HP, root 1.0 s). They appear in the Veteranus and Primus tiers only.

## Creatures

| Enemy | HP | Speed | Attacks | Behaviour in the kernel | Source |
|---|---|---|---|---|---|
| Cinder hound (pack of 3) | 25 | 6.0 m/s | bite: physical 6, windup 0.35 s, recovery 0.4 s, melee range `games.defaultMeleeRangeM` 1.2 m | two hounds engage, the third circles at `games.houndOrbitM` 3.5 m | `enemies.json` `creatures[cinder_hound]` |
| - its death burst | - | - | `ember_burst`: magic, tier 0, damage 5, radius 1.5 m, delay 0.6 s, at the hound's position | queued as an area telegraph that survives its owner (`Enemies.cpp` `QueueDeathEffects`) | `onDeath` |
| Mire maw | 100 | 1.5 m/s | bog_glob: magic tier 1, 14 dmg, windup 0.7 s, projectile 9 m/s; tongue_pull: physical 4 dmg, pull 4 m, windup 1.0 s, lane `games.tongueWidthM` 0.7 m x `games.tongueRangeM` 10 m | stationary once within its current attack's range; attacks alternate; cooldown `games.mawArtilleryCooldownS` 3.5 s | `enemies.json` `creatures[mire_maw]` |

- The session re-seats the player on the active pad after every kernel step (`ArenaSession.cpp` `SeatOnActivePad`), so
  a tongue pull's displacement does not persist. This is read from the code, not measured.
- Not spawned in Tiro, but ported: Thornback (unblockable lane charge) and Hush moth (mana drain on contact).
- The `echo` entry in `enemies.json` is not used.

## The seated rule: enemies fight from the floor (DF-001 option A)

| Field | Value | Unit | Source key |
|---|---|---|---|
| Dais no-entry box centre | (-11.7, 0) | m, layout axes | `combat.vr.json` `daisNoEntry.centreM` (= `arena-layout.json` `dais.centreM`) |
| Half extent | (2.6, 4.4) | m | `daisNoEntry.halfExtentM` (= half of `dais.sizeM` [5.2, 8.8]) |
| Standoff (hold line outside the footprint) | 0.6 | m | `daisNoEntry.standoffM` (= `dais.heightM`) |
| Centre pad to dais front face | 2.9 | m | `combat.vr.json` `_rangeM` note (pad x = -12, face x = -9.1) |
| Nearest enemy body to the centre pad | about 3.5 | m | DF-004 |

After every kernel step, `FVrRuleset::KeepOut` pushes any enemy inside the hold box back outside (`Games.cpp:239-248`).
The loader checks that the overlay box equals the layout dais (`VrRulesLoad.cpp:489-494`, "dais box or standoff does not match").
Consequences, measured:

| Gate | Live overlay | With `-MageArenaProposal` | Source |
|---|---|---|---|
| `MageArenaDesign.Session.Wave1Seated` (window 25-40 s) | lost at 23.25 s (dealt 106.7 of 208) | won inside the window (17 of 20 census seeds for `wide`) | DF-001 "After T11"; DF-003 |
| `MageArenaDesign.Session.CreaturesSeated` (window 30-45 s) | won at 33.05 s, 14 damage taken, 0 perfects | won at 27.30 s (outside, red) | DF-004; T15 card |

DF-004: a hound can die no closer than about 3.5 m, more than twice its 1.5 m burst radius, so no death burst reaches the
seat and the hounds' "teaches: perfect absorb on death bursts" does not happen. Options A (ember spit as a projectile,
recommended), B (larger radius), C (hounds on the lip) await the owner.

## Mage AI

Every mage (player reference bot, Water proxy, Fire mage) runs `MageInput` with a competence profile. The profile is
interpolated linearly between the integer rows of `enemies.json` `mages.competence`, then clamped by `mages.caps`
(`MageAI.cpp` `MageCompetence`).

| Competence | Reaction delay | Absorb chance vs absorbable | Perfect chance | Aim error | Decision cadence | Spell choice | Used in VR |
|---|---|---|---|---|---|---|---|
| 1 | 0.55 s | 0.40 | 0.05 | 12 deg | 0.50 s | random draw (level < 2) | Tiro semifinal; Gentle forces it |
| 1.5 | 0.50 s | 0.50 | 0.10 | 10 deg | 0.45 s | random draw | Tiro final |
| 2 | 0.45 s | 0.60 | 0.15 | 8 deg | 0.40 s | best spell for its tier | reference bot (`games.referenceCompetence`) |
| 2.5 | 0.40 s | 0.675 | 0.225 | 6.5 deg | 0.375 s | best spell for its tier | not reachable (plan's "Ember champion") |
| 3 | 0.35 s | 0.75 | 0.30 | 5 deg | 0.35 s | also baits absorbs before unblockables (level >= 3) | not used |
| Caps | min 0.25 s | - | max 0.45 | - | - | - | `mages.caps` |

Values for 1.5 and 2.5 are computed by the interpolation from the pinned rows; the caps do not bind at these levels.

Movement and aim (`runtime.json` `games`):

| Rule | Value | Unit | Source key |
|---|---|---|---|
| Preferred distance to the target | 5-8 | m | `magePreferredDistanceM` |
| Strafe direction flips every | 2.4 | s | `mageStrafePeriodS` |
| Aim lead | 0.45 of the bolt's flight time x target velocity | fraction | `mageAimLeadFraction` |
| Defence hold | 0.35 | s | `mageDefenceHoldS` |

### Pad targeting and re-acquire after a blink

- The kernel has no pad id. The session writes the player's position and previous position to the active pad after
  every step (`SeatOnActivePad`), so the target is always a pad centre and its velocity is zero.
- A mage recomputes its aim only at its decision ticks (every `decisionCadenceS`). After a blink it aims at the new pad
  at its next decision: up to 0.50 s later at competence 1 and 0.45 s at 1.5. There is no separate re-acquire delay.
- Soldiers and creatures copy the target point into a telegraph at windup; an attack already winding up, and any
  projectile in flight, keep the old point (CR-001). The next attack uses the new pad.
- CR-001 (open) asks the shared kernel for a real blink input and a pad id so a telegraph can name the pad.

### Water-proxy duelists (default) and the Fire mage (opt-in)

| Opponent | When | Composition | Label in the spawn log | Source |
|---|---|---|---|---|
| Water proxy | Tiro bouts 3 and 4 in the default arc | Undertow (bout 3), Mirror tide (bout 4) | "Tiro entrant - Water proxy", kind "Water proxy 1" / "1.5" | `Games.cpp:96-118`; `runtime.json` `games.opponentPresets` |
| Ember (Fire) mage | only when a bout is started with `SetBout(wave, true)` | Fire school (`Fire.bSchool`); Water composition attached but unused for casts | "Tiro entrant - Ember mage", kind "Ember mage 1" / "1.5" | `Games.cpp:106-118`; [fire.md](05-schools/fire.md) |

The Fire mage adds: the first-Sunfall rule (from tier IV, Sunfall is the cast when legal; below 70 mana it withholds
offense), Heat-driven damage, and the fire wall raised when a hostile projectile would cross it (after its reaction
delay; `MageAI.cpp` `ConsiderFireWall`). See [FIRE.md](../schools/FIRE.md).

## Dummies

| Dummy | Where | HP | Attacks | Source |
|---|---|---|---|---|
| Teach training dummy | teach steps; 8 m in front for the ward glob, 6 m for the sigil and blink steps | 200 | none of its own; the teach spawns the glob and the lane in its name | `SessionFlow.cpp` `PlaceDummy`, `ArmThreat`; `teach.json` `dummyHp`, `wardDistanceM`, `dummyDistanceM` |
| Training dummy (kernel drills) | tests and conformance only (`Kernel/Training.*`) | 10000 | every 3 s from 1 s: magic (14, tier 1, 9 m/s), physical (7, 14 m/s) or charge (22, lane 2 m x 12 m) | `runtime.json` `training` |
| Greybox threat demo | `Greybox/ThreatDemo.cpp`, captures | - | four visual-only threats, no damage | `arena-layout.json` `threats` |

## Games phase machine (`Kernel/Games.h`)

| Function | Does | Notes |
|---|---|---|
| `TryCreateGames(Out, Seed, Composition, StartWave, bReferencePlayer, bFireMages, Rules)` | new arena, player "Cassia" at `games.playerSpawn`, spawns `StartWave` | false if the wave is outside Tiro; `Rules` null = pinned path (conformance) |
| `StepGames(Games, PlayerInput)` | enemy inputs, mage inputs, one kernel step, then (overlay) `KeepInView` and `KeepOut`, death effects, end check | `active` -> `lost` when the player is down; -> `intermission` or `complete` when no enemy and no enemy hazard remain |
| `TryAdvanceGames(Games)` | from `intermission`: next wave, `ResetWave` (betweenWaves heal/refill, clock reset), clears all other actors, spawns | false unless the phase is `intermission` |
| `GamesResult(Games)` | `champion` (final won) or `missio` (lost), gold, renown | settled once |
| `RunFight(Seed, Wave, MaxSeconds)` | headless reference fight (competence-2 Rotation bot) with dead-air buckets | census and tests |

Session-only phases (`cold`, `offer`, `teach`) are strings the session writes into `FGames::Phase`; see
[08-session-arc.md](08-session-arc.md).

## Comfort settings that change opponents

| Setting | Effect on opponents | Value | Source |
|---|---|---|---|
| Narrow view | spawns, hold lines, new telegraph origins and death-burst origins compressed into +-40 deg of the active pad's forward; existing enemies walk back toward the arc at most their walk speed per tick | 40 deg | `combat.vr.json` `narrowFov.spawnArcDeg`; `VrRules.cpp` `CompressToArc`, `KeepInView` |
| Gentle | opponent mages attached at competence 1; enemy attack windups and Fire mage cast/telegraph windups x2 | 1, x2 | `combat.vr.json` `gentle`; `Enemies.cpp:62`, `Fire.cpp:186, 286`, `Games.cpp:111-114` |

## Status and gaps

- **Fire mage not in the default arc.** Plan section 6.4: Bout 3 is the Fire mage duel. Build: `BeginArc(1, false)`
  (`ArenaSession.cpp:1802`) and the arc never sets the Fire flag, so bouts 3 and 4 are Water proxies (`Games.cpp:96-118`).
- **Four bouts, not three.** Plan 6.4: one duel bout, competence 1 then 1.5 "on retry or for the final". Build: the
  pinned Tiro machine plays a competence-1 semifinal and a competence-1.5 final as separate bouts.
- **Champion replay (competence 2.5)** (plan 6.4, 6:30-7:00) is not built; Tiro contains no 2.5 opponent (2.5 is the
  Veteranus final).
- **Hounds teach nothing from a seat** (DF-004, open). Plan 6.4 calls the death bursts "perfect practice".
- **Gentle does not slow Water-proxy casts or hound death-burst delays**: `VrOpponentTelegraph` is called only from
  `Enemies.cpp:62` (attack windups) and `Fire.cpp:186, 286`. Plan 6.7 says "doubles telegraph times".
- **No pad id in the kernel** (CR-001 open); targeting a pad is a side effect of the seat pin.
- **Flankers.** Plan 6.1: flankers up to +-100 deg, announced by spatial audio and an edge chevron. Build: spawn markers
  stay within +-70 deg (`arena-layout.json` `spawnArc.limitDeg`); no flank logic, chevron or audio exists.
- The aim "soft-lock in a 60 deg cone" (`combat.json` `aimMode`) is not read by the C++ kernel (no `aimMode` key in
  `KernelData.cpp`).

## Open questions

- DF-004: which option (A ember spit, B larger burst, C hounds on the lip) makes Bout 2 teach the perfect absorb?
- Should the default arc be three bouts with one Fire duel (plan) or the pinned four-wave Tiro with a semifinal and a
  final, and if four, which of them is the Fire mage?
- Should Gentle also lengthen Water-proxy telegraphs and death-burst delays?
