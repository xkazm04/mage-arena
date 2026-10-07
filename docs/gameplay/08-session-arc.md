---
title: Session arc
channel: vr
status: partial
verified-against: 46643e2 (2026-10-07)
sources:
  - "docs/DECISIONS.md"
  - "docs/PROJECT-PLAN.md"
  - "apps/vr/Game/Source/MageArenaVR/Session/ArenaSession.h"
  - "apps/vr/Game/Source/MageArenaVR/Session/ArenaSession.cpp"
  - "apps/vr/Game/Source/MageArenaVR/Session/SessionFlow.cpp"
  - "apps/vr/Game/Source/MageArenaVR/Session/SessionPresentation.cpp"
  - "apps/vr/Game/Source/MageArenaVR/Session/TeachCapture.cpp"
  - "apps/vr/Game/Source/MageArenaVR/Kernel/Games.cpp"
  - "apps/vr/Game/Source/MageArenaVR/Kernel/ArenaKernel.cpp"
  - "apps/vr/Game/Source/MageArenaVR/Tests/SessionTests.cpp"
  - "apps/vr/Game/Source/MageArenaVR/Tests/TeachTests.cpp"
  - "apps/vr/tasks/T13-teach-and-pause.md"
  - "apps/vr/tasks/T15-creatures-bout-and-session-chain.md"
  - "apps/vr/tasks/T16-style-pick-replay-capture.md"
  - "apps/vr/tasks/BACKLOG.md"
data:
  - "apps/vr/data/vr/teach.json"
  - "apps/vr/data/vr/strings.json"
  - "apps/vr/data/pinned/docs/design/baseline-fourteen-nights/design/data/combat.json (betweenWaves, tierClock, pacingTargets)"
  - "apps/vr/data/pinned/docs/design/baseline-fourteen-nights/design/data/arena-tiers.json (tiers[tiro])"
  - "apps/vr/data/pinned/packages/core/src/arena/data/runtime.json (water.presets[Rotation], games.opponentPresets)"
---

# Session arc

A desktop `-game` launch auto-starts the arc with seed 1 (`UArenaSessionSubsystem::OnWorldBeginPlay` ->
`BeginArc(1, false)`, `ArenaSession.cpp:1802`): a **cold start** on the centre pad, a **teach** of three steps (ward with
a perfect, a sigil, a blink) inside the arena, then the pinned **Tiro** bouts in order - soldiers, creatures, a
competence-1 semifinal and a competence-1.5 final - with an **intermission** between bouts that applies the pinned
`betweenWaves` heal and refill. A palm raise moves the arc on at every gate; holding both palms restarts from the teach.
A loss offers an immediate retry of the same bout. Everything the plan puts after the last bout (Fire mage intro card,
slow-motion finisher, victory card, champion replay) and the Tide Orb IV branch pick are **not built**, and the duels
use Water-proxy mages, not the Fire mage. The whole arc is one `FGames` per bout attempt; the phase string
`FGames::Phase` is the arc's state.

## Phase machine

| Phase | Entered from | Prompt (`strings.json` key) | Leaves on | To |
|---|---|---|---|---|
| `cold` | `BeginArc` with no valid save file | `cold.start` "Raise your palm to begin." (or `settings.offer.narrow` when narrow view is offered) | a ward raise (palm) | `teach` |
| `offer` | `BeginArc` with a valid `bout.txt` | `offer.continue` "Raise your palm to continue. Both palms to start over." | palm: continue; both palms held 0.40 s: restart | the saved bout (`active`) or `teach` |
| `teach` | `cold`, or both palms in any offer phase | per step, see below | teach finished | `active` (bout 1) |
| `active` | `Start()` or `TryAdvanceGames` | none; wrist cues | all enemies and enemy hazards gone / player down | `intermission` or `complete` / `lost` |
| `intermission` | a bout won (not the last) | `intermission` "A moment to breathe. Raise your palm to continue." | palm: next bout; both palms 0.40 s: teach | `active` / `teach` |
| `lost` | player down | `offer.continue` | palm: retry the same bout; both palms 0.40 s: teach | `active` / `teach` |
| `complete` | the final (bout 4) won | none | nothing | stays |

Sources: `SessionFlow.cpp:316-377` (`BeginArc`), `:550-601` (`GetPromptText`), `:1237-1372` (`AdvanceArc`);
`ArenaSession.cpp:694-718` (`HandleWardRaised`); `Games.cpp:217-332` (`StepGames`, `TryAdvanceGames`).

A palm continue fires when the palm raise has been seen and both palms are not raised; "both palms" requires both hands'
confidence >= 0.2, palm height >= 0.20 m and palm facing within 35 deg of forward (`FWardThresholds`), held for
`teach.json` `offerHoldS` 0.40 s (`SessionFlow.cpp:1291-1333`).

## Cold start

| Item | Value | Source |
|---|---|---|
| Arena | quiet arena: the player "Cassia" on the centre pad, Rotation composition, no enemies | `SessionFlow.cpp` `MakeQuietArena` |
| Tier clock | frozen at tier 1 while not in a bout | `FreezeTeachClock` |
| Settings | the three dais stones (hand, view, gentle) are live only in `cold` | `ArenaSession.cpp` `PollComfortToggles`; see [09-pause-comfort-accessibility.md](09-pause-comfort-accessibility.md) |
| Scripted start delay | 1.5 s before the scripted palm (tests and captures only) | `teach.json` `coldHoldS` |
| Launch-to-interactive time | not measured | plan target 15 s or less (section 6.6) |

## Teach

All steps run in the arena with no fail state: the player's HP and damage taken are refunded every tick, the tier clock
stays at tier 1, mana is refilled at each step, and stamina before the blink step (`SessionFlow.cpp`
`StepTeachTick`, `EnterTeachStep`). A miss repeats the step.

| Step | Prompt key (text) | Threat | Passes when | Next |
|---|---|---|---|---|
| `ward` | `teach.ward` "Raise your palm." | a magic glob from a dummy 8 m ahead: marker for 1.0 s, then a projectile at 4 m/s, radius 0.28 m, 12 damage, tier 1 | a block or a perfect within impact + 0.8 s | `gap` (1.6 s), then `perfect` |
| `perfect` | `teach.ward.perfect` "Raise as it arrives." | the same glob | a perfect (white ring 1.1 s); after 3 blocks without a perfect, the perfect is waived | `ward-beat` (16 s) |
| `sigil` | `teach.sigil` "Circle, then the bar." (glyph `sigil-line1` on the dais) | a 200 HP dummy 6 m ahead | the dummy takes damage (Tide Orb, line 1) | `sigil-beat` (14 s); a miss after 8 + 8 s with no clip playing |
| `blink` | `teach.blink` "Blink off the pad." | an unblockable lane from the active pad, 8 m long, 1.2 m wide, 18 damage, windup 1.4 s | the lane resolves after the player blinked to another pad, with no damage | `blink-beat` (14 s), then bout 1 |

Numbers from `teach.json`: `wardDistanceM` 8.0, `wardWindupS` 1.0, `globSpeedMps` 4.0, `globRadiusM` 0.28, `globDamage`
12.0, `globSettleS` 0.80, `beatAfterAbsorbS` 1.6, `perfectRingS` 1.1, `perfectTries` 3, `beatAfterWardS` 16.0,
`dummyHp` 200.0, `dummyDistanceM` 6.0, `sigilPromptS` 8.0, `sigilRetryS` 8.0, `beatAfterSigilS` 14.0, `blinkRangeM` 8.0,
`blinkWidthM` 1.2, `blinkDamage` 18.0, `blinkWindupS` 1.4, `blinkSettleS` 0.50, `beatAfterBlinkS` 14.0. The scripted
lead times `absorbLeadS` 0.55, `perfectLeadS` 0.22 and `blinkLeadS` 0.40 drive only the scripted player. With these
values the glob reaches the player 170 ticks (2.83 s) after the step starts (`GlobImpactS`: 60 windup ticks plus
ceil(7.34 m / (4 m/s / 60)) = 111 move ticks, minus 1).

Measured: the teach takes **63.306 s** on the scripted clip player (T13 status line). Flow and the tier clock are not
taught here; each gets one wrist cue during the first bout (below).

`Session/TeachCapture.*` drives the same teach for the capture stills (`-MageArenaTeachCapture`); it adds no rules.

## Bouts

Every bout uses the player composition `Rotation` (`runtime.json` `water.presets[2]`), the VR overlay
(`combat.vr.json`) and the comfort settings. The player sits on the centre pad at the start of each bout.

| Bout | Tiro wave | Opponents | Pinned target | Measured (reference seated script, seed 1) |
|---|---|---|---|---|
| 1 | soldiers | 4 Hastatus conscripts, 2 Funditores | 25-40 s | live: **lost at 23.25 s**; proposal: won in window (DF-003 `wide`, 17/20 seeds) |
| 2 | creatures | 3 cinder hounds, 1 mire maw | 30-45 s | live: won 33.05 s, 0 perfects; proposal: 27.30 s (outside) |
| 3 | semifinal | Water-proxy mage, competence 1, preset Undertow | 45-70 s | not reported separately for the water proxy |
| 4 | final | Water-proxy mage, competence 1.5, preset Mirror tide | 45-80 s | not reported separately for the water proxy |

Whole run (`MageArenaDesign.Session.FullSeated`, teach skipped): live stops at bout 1; with the proposal it completes in
105.35 s, about 2.8 minutes with the 63.3 s teach added (T15 card). Opponent detail: [07-enemies-and-ai.md](07-enemies-and-ai.md).

Wrist cues during a bout (`ArenaSession.cpp:1466-1482`): the first tier unlock shows `wave.clock` "The clock advanced."
and the first Flow stack shows `wave.flow` "Flow is rising.", each for 4.0 s (a code literal), once per `Start()`.

## Intermission

| Effect | Value | Source key |
|---|---|---|
| Heal | 30% of missing HP | `combat.json` `betweenWaves.healFractionOfMissingHp` 0.30 |
| Mana | refilled to max | `betweenWaves.manaRefill` 1.0 |
| Stamina | refilled to max | `betweenWaves.staminaRefill` 1.0 |
| Tier clock | reset to tier 1 at the next bout start | `betweenWaves.tierClockReset`, `tierClock.resetsEachWave` |
| Projectiles, telegraphs, zones, Flow, Heat | cleared | `ArenaKernel.cpp:530-557` `ResetWave` |
| Other actors | removed; the next wave spawns | `Games.cpp:300-332` |
| Save file | `bout=<next wave index>` written | `SessionFlow.cpp:1313-1317` |
| Duration | until the player raises a palm | no timer |
| Branch pick | **none** | - |

The regular test `MageArena.Session.ChainAdvance` checks HP 61.75, mana 95 and stamina 70 after an advance (literals from
`stats.csv` and `combat.json`, per the T15 card).

## Loss and retry

| Item | Build | Source |
|---|---|---|
| Kernel result | `missio` (gold and renown by waves cleared); not shown to the player | `Games.cpp` `GamesResult` |
| Prompt | `offer.continue` | `SessionFlow.cpp:576-579` |
| Retry | palm: `Start(BoutSeed)` at the same bout index, fresh actors at full HP, same seed | `SessionFlow.cpp:1323-1326` |
| Retry time | as soon as the palm event arrives; no countdown | - |
| Start over | both palms held 0.40 s: the teach, save file cleared | `SessionFlow.cpp:1293-1303` |

## Plan versus build

| Plan section 6.4 beat | Plan time | Build |
|---|---|---|
| Cold start to the dais at dusk; "Raise your palm to begin." | 0:00-0:15 | built; launch time not measured |
| Teach: glob ward + perfect (white ring and bell), circle then bar on a dummy, black-core lane blink | 0:15-1:30 | built; 63.3 s; the bell is a log line only (no audio) |
| Bout 1 soldiers | 1:30-2:40 | built; lost from a seat on the live overlay (DF-001) |
| Intermission: heal 30% of missing, refill, clock reset; pick the Tide Orb IV branch | 2:40-3:00 | heal/refill/reset built; **branch pick not built** (Rain of Orbs fixed) |
| Bout 2 creatures, death bursts as perfect practice | 3:00-4:20 | built; bursts never reach the seat (DF-004) |
| Fire mage walks in, intro card with the PC portrait | 4:20-4:40 | **not built** |
| Bout 3 duel vs the Fire mage, competence 1 then 1.5 on retry or for the final; Sunfall at about 45 s | 4:40-6:30 | **two bouts** (semifinal 1, final 1.5) against **Water-proxy** mages; the Fire mage is opt-in only (`SetBout(wave, true)`) |
| Finisher: Leviathan Orb or a reflected fireball, 0.5 s slow motion, crowd roar | in bout 3 | **not built**; the player has neither Leviathan Orb nor Reflection (Rotation preset B branches) |
| Victory card: perfects, Crests, time | 6:30-7:00 | **not built**; `complete` shows no prompt |
| Champion replay at competence 2.5 | 6:30-7:00 | **not built** |
| Loss grants missio and an instant retry (5 s or less) | - | retry built; missio is a kernel result string only |
| Session 10 minutes or less | - | proposal run: about 2.8 min including the teach |

## Status and gaps

- The arc is functional end to end in code; on the live overlay a seated player loses bout 1 (known red
  `Session.Wave1Seated`, `Session.FullSeated`).
- **In progress (uncommitted, T16):** a style-pick replay capture that runs the Fire duel (`SetBout(2, true)`, seed 1)
  and style skins under `apps/vr/data/vr/styles/`. Not part of the committed arc.
- Quitting during an intermission writes nothing (`NotifyQuit` saves only in `active`, `SessionFlow.cpp:397-403`), so a
  quit between bout 1 and bout 2 restarts at the teach, and a quit in a later intermission resumes the bout that was
  just won (the save still holds its index). Read from code, not tested.
- BACKLOG: `FullSeated` logs a stale bout index after the intermission advance (cosmetic); the perfect-cue beads on
  magic area telegraphs show for the whole telegraph instead of the perfect window.
- The 4.0 s wrist-cue duration is a code literal, not data.

## Open questions

- Which of the missing beats (Fire mage as default duel, branch pick, finisher, victory card, champion replay) are in
  scope for the 2026-10-31 desktop gate?
- Should the arc be three bouts as planned or the pinned four-wave Tiro?
- Should loss show the missio result and the victory card show payout, given the camp and season are cut?
