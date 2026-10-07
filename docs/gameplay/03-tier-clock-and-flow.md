---
title: Tier clock and Flow
channel: vr
status: partial
verified-against: 1fb1b8b (2026-10-07); anchors into files changed since 46643e2 re-verified (none cited, or by symbol still valid)
sources:
  - docs/PROJECT-PLAN.md
  - docs/DESKTOP-INPUT.md
  - docs/design/SCHOOL-DEFENCES.md
  - apps/vr/tasks/A08-cuff-hud.md
  - apps/vr/Game/Source/MageArenaVR/Kernel/ArenaKernel.cpp
  - apps/vr/Game/Source/MageArenaVR/Kernel/ArenaThreats.cpp
  - apps/vr/Game/Source/MageArenaVR/Kernel/Water.cpp
  - apps/vr/Game/Source/MageArenaVR/Kernel/Catalog.cpp
  - apps/vr/Game/Source/MageArenaVR/Kernel/SimConstants.h
  - apps/vr/Game/Source/MageArenaVR/Session/ArenaSession.cpp
  - apps/vr/Game/Source/MageArenaVR/Session/SessionFlow.cpp
  - apps/vr/Game/Source/MageArenaVR/Session/SessionPresentation.cpp
data:
  - apps/vr/data/pinned/docs/design/baseline-fourteen-nights/design/data/combat.json#tierClock
  - apps/vr/data/pinned/docs/design/baseline-fourteen-nights/design/data/combat.json#flow
  - apps/vr/data/pinned/docs/design/baseline-fourteen-nights/design/data/combat.json#absorb.perfect
  - apps/vr/data/pinned/docs/design/baseline-fourteen-nights/design/data/combat.json#betweenWaves
  - apps/vr/data/vr/combat.vr.json#splitHands
  - apps/vr/data/vr/strings.json#wave.flow
  - apps/vr/data/vr/strings.json#wave.clock
  - apps/vr/art/hud/mask
---

# Tier clock and Flow

Two pinned shared systems shape the player's damage over a bout, and the VR channel uses both unchanged. The **tier
clock** unlocks tiers I to IV at 0, 15, 30 and 45 seconds into a bout; every perfect ward advances it by 2 seconds,
unlocks are at least 6 seconds apart, and it resets every bout. Each line sigil casts the line's current tier. **Flow** is
a 0-5 stack: casting a different line within 2 seconds of the last cast adds one, a perfect ward adds one, repeating a
line or 3 seconds without a cast or perfect resets it, and each stack adds 6% damage. At 5 stacks the next cast is a
**Crest**: free and ×1.5 damage, after which Flow returns to 0. Split-hand casts are outside Flow. The build shows both on
one camera-attached cuff (one picture at a time, chosen by priority) with four bars, not on two wrist bracers as the plan
describes.

## Tier clock

### Rules

| Rule | Value | Unit | Source key |
|---|---|---|---|
| Tier I unlocks at | 0 | s into the bout | `combat.json#tierClock.unlockAtSeconds.1` |
| Tier II unlocks at | 15 | s | `combat.json#tierClock.unlockAtSeconds.2` |
| Tier III unlocks at | 30 | s | `combat.json#tierClock.unlockAtSeconds.3` |
| Tier IV unlocks at | 45 | s | `combat.json#tierClock.unlockAtSeconds.4` |
| Advance per perfect ward | 2.0 (120 ticks) | s | `combat.json#tierClock.perfectAbsorbAdvanceS`, `#absorb.perfect.tierClockAdvanceS` |
| Minimum gap between unlocks | 6.0 | s | `combat.json#tierClock.minimumSecondsBetweenUnlocks` |
| Highest tier | 4 | - | `SimConstants.h:31` (`TierCap`) |
| Resets | every bout (wave) | - | `combat.json#tierClock.resetsEachWave`, `#betweenWaves.tierClockReset` |

### How the kernel runs it (`ArenaKernel.cpp:430-446`, `ArenaKernel.cpp:511-517`)

- Every actor starts a bout at tier I (`AddMage`, `ResetWave`, `ArenaKernel.cpp:391`, `ArenaKernel.cpp:536`).
- After each tick's combat, the next tier unlocks when
  `ticks since bout start + advance ticks ≥ ticks(unlockAt[next])` **and** `ticks since the last unlock ≥ ticks(6.0)`.
  At most one tier unlocks per tick. The kernel emits an `unlock` event.
- A perfect ward adds `ticks(2.0)` to the actor's advance (`ArenaThreats.cpp:316`). Because of the 6 s gap, perfects can
  bring an unlock forward but cannot make two unlocks closer than 6 s.
- The same clock runs for every mage, including AI opponents; the Fire mage's Sunfall waits for its own tier IV (see
  [05-schools/](05-schools/) and [07-enemies-and-ai.md](07-enemies-and-ai.md)).
- During the teach the session holds the player at tier I by restarting the clock (`SessionFlow.cpp:733-738`).

### What a tier changes

A line sigil casts the row for (line, current tier, branch) (`Catalog.cpp:60`); Bolt is always tier 0. The tier rows of
the player's lines are in [02-verbs-and-gestures.md](02-verbs-and-gestures.md#kernel-effect). Mirror's tier II row is
passive under the player's preset, so the Mirror sigil casts nothing at tier II. With the ward held (split hands) only
tiers 0-II may be cast (`combat.vr.json#splitHands.maxTier` = 2).

## Flow

### Rules

| Rule | Value | Unit | Source key |
|---|---|---|---|
| Gain on a cast | +1 when the line differs from the last cast line and the cast is within 2.0 s (120 ticks, inclusive) of the last cast | stack | `combat.json#flow.differentLineWithinS` |
| Gain on a perfect ward | +1 | stack | `combat.json#flow.perfectAbsorbGives` |
| Maximum | 5 | stacks | `combat.json#flow.max` |
| Damage per stack | +0.06 × Flow (after this cast's change) | multiplier | `combat.json#flow.perStackDamage` |
| Reset on the same line twice | to 0 | - | `combat.json#flow.resetOnSameLineTwice` |
| Reset on idle | to 0 after 3.0 s (180 ticks) without a cast or a perfect | s | `combat.json#flow.resetAfterIdleS` |
| Crest trigger | the next cast when Flow is 5 | - | `Water.cpp:23-28` |
| Crest mana cost | 0 | mana | `combat.json#flow.crest.manaCost` |
| Crest damage | ×1.5 (replaces the per-stack bonus) | multiplier | `combat.json#flow.crest.damageMult` |
| After a Crest | Flow 0 | - | `Water.cpp:24-28` |
| Bout start | Flow 0 | - | `ResetWave` → `ResetWater`, `ArenaKernel.cpp:555`, `Water.cpp:412-416` |

### How the kernel runs it (`Water.cpp:19-44`, `Water.cpp:76-137`, `Water.cpp:279-286`, `Water.cpp:342-343`)

- Order inside one cast: if Flow is 5 the cast is a Crest (Flow 0); else if the line equals the last line, Flow 0; else if
  there was a last cast within 2 s, Flow + 1. Then the last line and the activity time are updated.
- A cast of a **different** line more than 2 s but less than 3 s after the last one leaves Flow unchanged.
- Bolt counts as its own line (`bolt`). Bolt then a sigil line within 2 s is +1; two Bolts in a row reset Flow. A Bolt
  can be the Crest (free, ×1.5).
- The Crest applies to the next accepted cast of any slot; there is no separate Crest gesture.
- A perfect ward adds a stack and refreshes the idle timer, but does not change the last line.
- **Split-hand casts** (ward held, staff not planted) do not touch Flow at all: no gain, no reset, no activity refresh,
  and a cast while Flow is 5 is refused as `crest` (`ArenaKernel.cpp:263-270`). The idle timer keeps running, so a run of
  split casts longer than 3 s lets Flow reset.

## Between bouts

| Rule | Value | Unit | Source key |
|---|---|---|---|
| Heal | 30% of missing HP | fraction | `combat.json#betweenWaves.healFractionOfMissingHp` |
| Mana / stamina | refilled to max | fraction | `combat.json#betweenWaves.manaRefill`, `#staminaRefill` |
| Tier clock | back to tier I at 0 s, advance cleared | - | `ArenaKernel.cpp:530-557` |
| Flow | 0 (Water state rebuilt, composition kept) | - | `Water.cpp:412-416` |

## Presentation (greybox)

One cuff, built in `SessionPresentation.cpp:366-436` and updated in `SessionPresentation.cpp:438-521` (line numbers from
the committed file; the working tree has unrelated uncommitted edits to it).

| Element | What it shows | Source |
|---|---|---|
| Placement | attached to the camera at (58, −26, −14) cm; Y flips to +26 in left-hand mode, so it sits on the ward-hand side of the view | `SessionPresentation.cpp:377-380`, `:441-446` |
| Bars | HP, mana, stamina, and clock progress (seconds into the bout plus advance, over the next tier's unlock second) | `SessionPresentation.cpp:417-418`, `:458-464` |
| Face picture | one A08 mask at a time, chosen in this order: `pause`; `perfect-advance` for 0.45 s after a perfect; `unlock-flash` for 0.45 s after an unlock; `crest-ready` at Flow 5; `flow-1`..`flow-5`; `tier4`, `tier3`, `tier2`; `idle` | `SessionPresentation.cpp:479-515`; masks in `apps/vr/art/hud/mask/` |
| Text cue | "Flow is rising." the first time Flow rises in a bout; "The clock advanced." on the first unlock in a bout; each shown 4 s | `strings.json#wave.flow`, `#wave.clock`; `ArenaSession.cpp:1462-1482`; `SessionFlow.cpp:612-619` |

No sound plays on an unlock or a Crest; there is no audio playback code under `apps/vr/Game/Source/`.

## Status and gaps

Built: the tier clock, perfect advance, the 6 s gap and the per-bout reset; Flow gain, resets, per-stack damage and the
Crest, all from pinned data; the split-hand exclusion from Flow (VR overlay); the cuff with bars, the state masks and two
text cues.

Plan versus build:

- **Bracers.** PROJECT-PLAN §6.3: the clock is "four runes on the casting wrist's bracer", Flow is "0-5 beads on the ward
  bracer", and the hand glows at 5. A08 and DESKTOP-INPUT describe one cuff on the off-hand wrist. Build: one cuff,
  attached to the camera (head-locked), not to a tracked wrist; tier and Flow share one face and are never shown at the
  same time; no hand glow at Crest-ready.
- **Head-locked UI.** PROJECT-PLAN §6.7 says head-locked UI is avoided and the HUD is diegetic on the wrists. The cuff is
  head-locked in the build.
- **Unlock chime.** PROJECT-PLAN §6.3: unlocks play a chime from the wrist. Not built (no audio).
- **Flow damage per stack.** PROJECT-PLAN §6.3 does not mention the 6% per stack; the pinned data and the build apply it.
- **Crest with split hands.** SCHOOL-DEFENCES A: the Crest needs both hands. The build refuses any split cast at Flow 5,
  including a Bolt, so a player who keeps the ward up at Flow 5 cannot cast until the ward is lowered or the staff is
  planted (`ArenaKernel.cpp:263-267`, `ArenaSession.cpp:623-637`).

## Open questions

- Does each `flow-N` mask also show the lit tier sockets? The code shows exactly one mask, so if the flow masks do not
  include the tier runes, the tier is invisible whenever Flow is above 0. The A08 state list treats them as separate
  states. Not checked against the images.
- Should the cuff move onto a tracked wrist once hand tracking is live, and which wrist (casting, per the plan, or ward,
  per A08)?
- Should split casts refresh the Flow idle timer, so that holding the ward does not silently cost the stacks built before?
