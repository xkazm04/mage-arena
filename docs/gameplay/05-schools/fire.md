---
title: Fire school (as built)
channel: vr
status: partial
verified-against: 46643e2 (2026-10-07)
sources:
  - "docs/schools/FIRE.md"
  - "docs/DECISIONS.md"
  - "docs/PROJECT-PLAN.md"
  - "docs/change-requests/CR-002-fire-school.md"
  - "docs/design/SCHOOL-DEFENCES.md"
  - "docs/design-findings/DF-002-fire-duel-balance.md"
  - "docs/design-findings/DF-003-calibration-proposal.md"
  - "apps/vr/Game/Source/MageArenaVR/Kernel/Fire.cpp"
  - "apps/vr/Game/Source/MageArenaVR/Kernel/MageAI.cpp"
  - "apps/vr/Game/Source/MageArenaVR/Kernel/Games.cpp"
  - "apps/vr/Game/Source/MageArenaVR/Session/ArenaSession.cpp"
  - "apps/vr/Game/Source/MageArenaVR/Tests/SessionTests.cpp"
  - "apps/vr/tasks/T09-fire-school.md"
  - "apps/vr/tasks/T12-fire-wall-and-calibration.md"
data:
  - "apps/vr/data/pinned/docs/design/reference-the-ledger/design/data/spells-fire.csv"
  - "apps/vr/data/pinned/docs/design/baseline-fourteen-nights/design/data/schools.json (schools[fire].combatIdentity)"
  - "apps/vr/data/vr/combat.vr.json (fireWall)"
  - "apps/vr/data/vr/calibration-proposal.json (knobs.defence.fireWall*)"
  - "apps/vr/data/scenarios-vr/fire-duel-competence-1.json"
  - "apps/vr/data/scenarios-vr/fire-duel-competence-1-5.json"
---

# Fire school (as built)

Fire exists in the VR C++ kernel as an **opponent school only**: Heat, all ten rows of `spells-fire.csv`, the Fire mage's
spell choice, and the fire wall defence that the Fire mage AI raises. The player cannot cast Fire. The detailed rules,
row by row, are in [docs/schools/FIRE.md](../../schools/FIRE.md), which stays the authoritative reading of the pinned
cells; this page adds the status, the numbers that matter for the VR session, and the gaps. Fire is offered back to the
desktop/TV channel as CR-002 (open, not merged). The Fire mage is **not** in the default `-game` session: the arc's
duels use Water-proxy mages unless a bout is started with the Fire flag.

## What is implemented

| Part | Where | Data |
|---|---|---|
| Heat resource, gains, decay, Kindled / Blazing thresholds | `Kernel/Fire.cpp` | `schools.json` `fire.combatIdentity` |
| Ten Fire spells (Ember Dart ... Brand Wrath) | `Kernel/Fire.cpp`, parsed in `KernelData.cpp` (`FFireSpell`) | `spells-fire.csv` |
| Fire mage spell choice, including the "first Sunfall" beat | `Kernel/MageAI.cpp` `ChooseFireSpell` | `enemies.json` `mages.competence` |
| Fire wall (SCHOOL-DEFENCES C), raised by the Fire mage AI only | `Kernel/VrRules.cpp` `TickWalls`, `MageAI.cpp` `ConsiderFireWall` | `combat.vr.json` `fireWall` |
| Opt-in per bout | `TryCreateGames(..., bFireMages)` (`Games.h:63`); `FArenaSession::SetBout(wave, true)` | - |
| Reference duels (C++ is the reference for Fire) | `Tests/FireTests.cpp` | `scenarios-vr/fire-duel-competence-1*.json` |

## Heat summary (from FIRE.md and `schools.json`)

| Rule | Value | Unit | Source key |
|---|---|---|---|
| Range | 0-100 | Heat | `combatIdentity.range` |
| Gain per connecting bolt | 2 | Heat | `gain.perBoltHit` |
| Gain per Flick Flame target | 6 | Heat | `gain.perFlickTarget` |
| Gain per unabsorbed hit taken | 12 | Heat | `gain.perHitTakenUnabsorbed` |
| Gain per enemy within 3 m | 3 | Heat/s | `gain.perSecondEnemyWithin3m` |
| Decay | 6 per second after 2.0 s with no gain | Heat/s, s | `decay.perSecond`, `decay.afterNoGainSeconds` |
| Kindled (>= 50) | spell damage x1.15 | multiplier | `thresholds[0]` |
| Blazing (>= 80) | spell damage x1.3, absorb drain x1.5, stamina regen x0.7 | multiplier | `thresholds[1]` |
| Duel start | 0 | Heat | FIRE.md (camp line is `campBias`, not combat) |

## Spells the Fire mage casts (verbatim from `spells-fire.csv`)

| Id | Name | Tier | cast_s | cooldown_s | mana | damage | blockable | telegraph_s | range_m | heat_gain |
|---|---|---|---|---|---|---|---|---|---|---|
| fire_bolt | Ember Dart | 0 | 0.00 | 0.30 | 4 | 8 | absorbable | 0.00 | 12 | 2 per hit |
| fire_flick | Flick Flame | 1 | 0.15 | 3.0 | 12 | 20 | absorbable | 0.15 | 4 | 6 per target |
| fire_cinderstep | Cinder Step | 1 | 0.00 | 6.0 | 15 | 6 per 0.5 s tick | absorbable | 0.00 | 5 | 3 per tick hit |
| fire_kindle | Kindle | 1 | 0.40 | 10.0 | 8 | 0 | n/a | 0.00 | 0 | +25 instant |
| fire_lance | Brand Lance | 2 | 0.60 | 7.0 | 25 | 32 | absorbable | 0.60 | 14 | 4 per target |
| fire_ring | Ring of Cinders | 2 | 0.30 | 9.0 | 22 | 18 | absorbable | 0.30 | 3 | 5 per target |
| fire_pyre | Pyre Circle | 3 | 0.50 | 12.0 | 40 | 48 | absorbable | 0.80 | 10 | 8 per target |
| fire_furnace | Furnace Heart | 3 | 0.50 | 16.0 | 30 | 0 | n/a | 0.00 | 0 | Heat locked at 100 for 6 s |
| fire_sunfall | Sunfall | 4 | 0.80 | 25.0 | 70 | 95 | UNBLOCKABLE | 1.20 | 12 | 10 per target |
| fire_wrath | Brand Wrath | 4 | 0.20 | 20.0 | 60 | 14 per 0.25 s tick | absorbable | 0.20 | 10 | 2 per tick |

All ten rows are eligible once their tier is open; the plan's named subset is not used as a filter (FIRE.md, "Fire
mage"). Sunfall's 0.8 s cast and 1.2 s shadow are sequential, about 2.0 s from cast start to impact.

## Fire wall (Fire mage AI only)

| Field | Live value | Proposal value | Unit | Source key |
|---|---|---|---|---|
| Distance in front of the caster | 4 | 4 | m | `fireWall.distanceM` / `knobs.defence.fireWallDistanceM` |
| Arc | 90 | 150 | deg | `fireWall.arcDeg` / `fireWallArcDeg` |
| Duration | 3 | 12 | s | `fireWall.durationS` / `fireWallDurationS` |
| Mana cost (once, on raise) | 12 | 6 | mana | `fireWall.manaCost` / `fireWallManaCost` |
| Heat per projectile stopped | 4 | 4 | Heat | `fireWall.heatPerStop` / `fireWallHeatPerStop` |
| Burn damage to a body crossing the arc | 12 | 12 | HP | `fireWall.burnDamage` / `fireWallBurnDamage` |

The wall stops magic and physical projectiles; unblockables pass. Every live value is marked "Proposal ... Not signed
off" in `combat.vr.json`. After the DF-003 fix the mage's own bolts pass her wall.

## Measured duels

| Run | Opponent | Result | Source |
|---|---|---|---|
| Walking reference player (competence-2 Water preset), seed 40000 | Ember mage competence 1 | loss at 16.0 s, Sunfall 0 | DF-002; `scenarios-vr/fire-duel-competence-1.json` |
| Walking reference player, seed 40001 | Ember mage competence 1.5 | loss at 18.4 s, Sunfall 0 | DF-002; `fire-duel-competence-1-5.json` |
| Seated census, live overlay, 20 seeds | competence 1 | 17/20 wins, median 30.47 s, Sunfall 0/20 | DF-003 table, row `baseline` |
| Seated census, live overlay, 20 seeds | competence 1.5 | 12/20 wins, median 28.52 s, Sunfall 0/20 | DF-003 table, row `baseline` |
| Seated census, proposal `wide` | competence 1 / 1.5 | 19/20 at 44.83 s, Sunfall 1/20 / 15/20 at 40.56 s, Sunfall 3/20 | DF-003 |
| `MageArenaDesign.Duel.FireSeated` (seed 1, live) | competence 1 / 1.5 | won 29.47 s / lost 22.92 s; red (outside 45-80 s, no Sunfall) | T14 and T15 cards; `SessionTests.cpp` (45/80 literals) |

Plan target: median duel 45-80 s (`combat.json` `pacingTargets.duelS`), Sunfall at about 45 s, must be blinked.

## Status and gaps

- **Not in the default session.** `UArenaSessionSubsystem::OnWorldBeginPlay` calls `BeginArc(1, false)`
  (`ArenaSession.cpp:1802`), and nothing in the arc calls `SetBout(..., true)`; Tiro bouts 3 and 4 spawn "Tiro entrant -
  Water proxy" mages (`Games.cpp:96-118`). The Fire mage appears only in `FireSeated`, the Wave 1 capture
  (`Wave1Capture.cpp:313`) and the T16 style-pick capture (in progress). Plan section 6.4 puts the Fire mage in Bout 3.
- **No player Fire.** The owner's 2026-10-03 decision asks every kernel to implement all four schools; Fire exists only
  as an AI school. A player Fire loadout, gestures and a player-raised wall are not designed for VR.
- **Duel balance unresolved.** The seated duel ends at about 30 s and Sunfall is almost never cast (DF-003, owner
  decision pending in BACKLOG). The plan's "Sunfall at about 45 s, must be blinked" beat does not happen in measured runs.
- **Intro card and PC portrait** (plan section 6.4, 4:20-4:40) are not built.
- **Gentle** doubles the Fire mage's cast and telegraph windups (`Fire.cpp:186, 286`) and forces competence 1.
- FIRE.md marks several readings of ambiguous cells as **Owner:** items; they remain unanswered (see FIRE.md).

## Open questions

- When does the Fire mage enter the default arc (Bout 3 only, or Bouts 3 and 4), and how is that expressed, given that
  the pinned `arena-tiers.json` names generic "entrant_of_another_tent" mages?
- Which DF-003 direction lands both gates (wave and duel), and how should Sunfall reliably appear (earlier unlock,
  scripted beat, different duel target)?
- Is the fire wall a player defence too once a player Fire school exists (SCHOOL-DEFENCES C says "player Fire later")?
