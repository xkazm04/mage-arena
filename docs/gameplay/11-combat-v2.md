---
title: Combat v2 - kernel and overlay rules
channel: vr
status: proposed
verified-against: 35cd067 (2026-10-07)
sources:
  - "docs/DECISIONS.md"
  - "docs/PROJECT-PLAN.md (6.2, 6.3, 6.4, 6.7)"
  - "owner v2 vision log, 2026-10-07 (not yet in git)"
  - "docs/design/SCHOOL-DEFENCES.md"
  - "docs/design-findings/DF-003-calibration-proposal.md"
  - "docs/design-findings/DF-004-creatures-cannot-reach-the-seat.md"
  - "docs/gameplay/03-tier-clock-and-flow.md, 04-threat-language.md, 07-enemies-and-ai.md, 05-schools/water.md, 05-schools/fire.md"
  - "docs/campaign/04-vr-campaign-design.md"
  - "apps/vr/Game/Source/MageArenaVR/Kernel/ArenaKernel.cpp, ArenaThreats.cpp, Water.cpp, Fire.cpp, MageAI.cpp, SimTypes.h, SimConstants.h, VrRules.h"
  - "apps/vr/Game/Source/MageArenaVR/Session/ArenaSession.cpp, SessionPresentation.cpp"
  - "apps/vr/Game/Source/MageArenaVR/Gestures/WardDetector.h, StaffDetector.h"
  - "apps/vr/tools/clipgen/generate.mjs, docs/CLIP-SCHEMA.md"
data:
  - "apps/vr/data/vr/combat.vr.json (new blocks proposed below)"
  - "apps/vr/data/vr/presets.vr.json (new file, proposed)"
  - "apps/vr/data/pinned/.../combat.json (tierClock, flow, absorb, aimMode) - read only"
  - "apps/vr/data/pinned/.../spells-water.csv, reference-the-ledger/.../spells-fire.csv - read only"
---

# Combat v2 - kernel and overlay rules

This page turns the owner's v2 combat answers into rules the C++ kernel and the VR overlay can carry. Every number
here is **authored** (a proposal for the census); nothing is simulated or measured yet unless a row says so. Each
mechanic names its home: the VR overlay `combat.vr.json`, a VR player preset, or shared kernel semantics that need a
**CR to TV**. All new rules run only when a session passes a `FVrRuleset` (`VrRules.h:125`), so the pinned path and the
45 conformance vectors stay byte-identical. New kernel events are emitted only on the overlay path. The kernel keeps
`TierCap = 4` (`SimConstants.h:31`): tier V is reserved for the Breaking at the Summa and has no rows.

## 0. Ground rules for every mechanic

- Each new `combat.vr.json` block carries `"enabled"`; `false` or absent is today's behaviour, so the two Fire
  reference scenarios in `apps/vr/data/scenarios-vr/` keep their numbers.
- New runtime fields sit outside the pinned state hash, like the fire-wall reaction fields (`SimTypes.h:229-233`).
- New events go through `Emit` (`ArenaKernel.cpp:398-407`) only when `Rules != nullptr`; the session, the census and
  the collar ledger read them (`DrainEvents`, `ArenaSession.cpp:1431`).
- Presentation is diegetic (the mage, the wrists, the Wardstones, the crowd); voice lines are 10 words or fewer,
  pre-rendered with ElevenLabs (`DECISIONS.md` 2026-10-02; vision log waves 4-5).

### New kernel events (overlay path only)

| Event | Actor | TargetId | Value | Emitted when |
|---|---|---|---|---|
| `phase` | rival mage | - | new phase (2 or 3) | HP crosses a phase threshold (section 1) |
| `surge` | each mage | rival | new tier | a phase break lifts a collar one tier |
| `signature` | rival mage | player | tier | the last-phase opener starts |
| `tell` | opponent mage | player | tier | a tier III-IV cast enters its tell (section 2) |
| `stagger` | opponent mage | hitter | tier of the lost cast | a hit inside a tell or channel cancels the cast |
| `reflect` | reflector | original owner | return count 1-3 | a perfect returns a projectile (section 4) |
| `flank` | struck mage | shooter | degrees off facing | a side-pad shot bypasses the ward (section 5) |
| `evade` | player | threat owner | 0 | an unblockable aimed at the player resolves with 0 damage after a blink in its telegraph |
| `miss` | player | - | tier | a line activation resolves with no target hit |
| `seal` | player | - | 1 sealed, 0 failed | the tier IV seal window closes (section 8) |
| `combo` | player | - | combo index 1-6 | the third line of a named order lands (section 3) |

The existing `cast`, `hit`, `perfect`, `down`, `roll`, `interrupt` and `unlock` events
(`ArenaKernel.cpp:101, 313, 426, 444`; `ArenaThreats.cpp:317, 336, 342`) keep their meaning.

## 1. HP-gated boss phases

**Rule.** A named rival has three phases. Phase 2 starts when its HP first falls to 66% of max, phase 3 at 33%. A hit
that would cross a threshold leaves the rival exactly on it; the excess is discarded, so no single hit skips a phase
(a 40-damage Leviathan Orb at 70 HP leaves Brennic on 62.7, not 30). On a break: the rival is immune for 1.0 s and
drops its pending cast without a `stagger` (no Crack); both collars surge one tier (player and rival, capped at IV,
`LastUnlockTick` set to the break tick, so the 6 s gap of `UpdateClock`, `ArenaKernel.cpp:430-446`, still spaces the
next timed unlock); the crowd swells. Phase 3 opens with the rival's signature (below). Losing at 0 HP is **missio,
non-lethal**: the kernel's `down` (`ArenaThreats.cpp:337-343`) is a kneel, never a death, below the Summa.

**Signature opener.** At the first decision after the phase-3 break, the rival casts its signature regardless of the
tier clock, the cooldown and its mana (the mana is topped up to the cost: "the crowd feeds him"). The opener is
**not interruptible**: its tell shows the black core and red rim, meaning "leave". Every later cast of the same
spell follows the normal rules and can be interrupted. This replaces the `Self.Tier >= 4` gate of the first-Sunfall
beat (`MageAI.cpp:274-283`) for named rivals only; DF-003 measured Sunfall in 0 of 40 seated duels because duels end
before tier IV (`DF-003-calibration-proposal.md`, row `baseline`).

**Seated Sunfall radius.** The pinned meteor is r 3.5 m (`spells-fire.csv` `fire_sunfall`); pads are 3.0 m apart
(`arena-layout.json` `pads.spacingM`), so from the centre pad no pad leaves it (`04-threat-language.md`, open
question). The overlay replaces the radius for the seated duel with 2.4 m (authored): 2.4 + 0.38 body radius < 3.0,
so any adjacent pad is out. Replaced field and reason are written as in the conscript `replaces` list
(`combat.vr.json` `attacks[0].replaces`).

**Style per phase (Brennic, data on the existing mage AI).**

| Phase | Name | Style tweak (authored) | Voice (10 words or fewer) |
|---|---|---|---|
| 1 | Measured | pinned competence curve; Kindle allowed first | - |
| 2 | Kindled | `decisionCadenceMult` 0.85; spell weights Flick 2, Lance 2, Ring 1.5; fire-wall reaction ×0.7 | "The collar sings, Cassia. Let it." |
| 3 | Sunfall | opener Sunfall; then weights Furnace 2, Wrath 2; perfect chance against his own returned fire 0.35 | "Sunfall. Leave, or kneel in the ash." |

**Data (overlay, new block `rivals`).**

```json
"rivals": { "enabled": true, "brennic": { "school": "fire", "hpMult": 1.0, "defence": "fireWall",
  "phases": { "thresholdsFrac": [0.66, 0.33], "clampAtThreshold": true, "breakImmuneS": 1.0, "surgeTiers": 1 },
  "signature": { "spellId": "fire_sunfall", "openerPhase": 3, "ignoresTier": true, "ignoresCooldown": true,
                 "manaFloor": true, "openerInterruptible": false, "seatedRadiusM": 2.4 },
  "returnsOwnFire": { "perfectChance": 0.35, "catchHoldS": 0.35 },
  "style": [ {}, { "decisionCadenceMult": 0.85, "wallReactionMult": 0.7, "weights": { "fire_flick": 2, "fire_lance": 2, "fire_ring": 1.5 } },
             { "weights": { "fire_furnace": 2, "fire_wrath": 2 } } ],
  "taunts": { "phase2": "taunt.brennic.phase2", "phase3": "taunt.brennic.phase3" } } }
```

`weights` multiply the uniform draw of `ChooseFireSpell` (`MageAI.cpp:263-347`); the draw count stays one per
decision, so RNG consumption does not change shape. Taunt keys go to `strings.json`.

**Lives:** overlay (`combat.vr.json#rivals`). Phases and the signature opener are offered to TV as a **CR** (shared
semantics: a rival with phases), but VR does not wait for it. **Hooks:** `ResolveHit` (`ArenaThreats.cpp:277-345`)
clamps and emits `phase`; `UpdateClock` gets a surge entry; `MageInput` (`MageAI.cpp:650-732`) reads the phase style;
`ChooseFireSpell` reads the opener. **Presentation:** Brennic's collar flares and the player's wrist cuffs flash on
`surge`; unlock chime from the wrist; crowd swell; taunt spatialised from Brennic. The last hit of phase 3 plays 0.5 s
of presentation slow motion (session time scale only; the kernel steps unchanged) and Brennic kneels. **Measured by:**
census `phase_reached`, `signature_s`, `signature_evaded`; tests `MageArena.Phases.*`.

## 2. Tells and interrupts

**Rule.** Every tier III-IV cast by an opponent mage gets a **tell**: a VR-only lead before the pinned windup, during
which the mage is rooted and cannot ward. The tell window is lead + pinned windup. A player hit inside the window that
deals at least 2.0 HP after reductions **staggers** the mage: `Interrupt` runs (`ArenaKernel.cpp:409-428`; mana and
cooldown stay spent, as pinned), the mage gets `RecoveryUntil` +0.8 s and no ward, a `stagger` event fires, and the
player gains a Crack. A mage cannot be staggered again for 3.0 s. A hit during a Brand Wrath channel staggers the same
way (the pinned interrupt already cancels the channel, `CancelFireChannel`). Committed telegraphs (the meteor shadow)
are not cancelled (`SimTypes.h:371`). Gentle doubles the lead with the windup (`VrOpponentTelegraph`, `VrRules.h:190`).

| Cast (Fire, `spells-fire.csv`) | Tier | Pinned windup | Lead (authored) | Tell window | Interruptible |
|---|---|---|---|---|---|
| Pyre Circle | III | 0.8 s (max of cast and telegraph, `Fire.cpp:186`) | 0.6 s | 1.4 s | yes |
| Furnace Heart | III | 0.5 s | 0.6 s | 1.1 s | yes |
| Sunfall, phase-3 opener | IV | 0.8 s cast, then 1.2 s shadow | 0.9 s | 1.7 s | **no** |
| Sunfall, later casts | IV | same | 0.9 s | 1.7 s | yes |
| Brand Wrath | IV | 0.2 s, then 2.0 s channel | 0.9 s | 1.1 s + channel | yes |

Why 2.0 HP: a Bolt (Rain Needle, 2.05, `spells-water.csv`) qualifies, so the cheapest answer works if it is fast. Why
the lead: a seated answer is reaction (about 0.3 s) + flick (0.12-0.20 s, `generate.mjs:67`) + bolt flight
(5-8 m at 20 m/s, 0.25-0.40 s), about 0.7-0.9 s; Furnace's pinned 0.5 s could never be read.

```json
"tells": { "enabled": true, "leadS": { "3": 0.6, "4": 0.9 }, "rooted": true, "noWard": true, "interruptMinDamage": 2.0,
           "staggerS": 0.8, "staggerImmuneS": 3.0, "appliesTo": "opponentMages" }
```

**Lives:** overlay. "A hit inside a tell interrupts" changes shared semantics (pinned only lets a staff strike
interrupt, `combat.json` `staffStrike.interruptsCasts`, `ArenaKernel.cpp:188-191`): **CR to TV**. **Hooks:**
`TryFireCast` (`Fire.cpp:172-207`) adds the lead to `Pending.ReleaseTick` and emits `tell`; `ResolveHit` checks the
target's open tell and calls `Interrupt`. **Presentation:** the mage's palms gather a sigil in its element colour, the
collar glows, a rising hum; the crowd hushes. A stagger is a visible recoil and a crack of sound. No text. **Measured
by:** census `tells`, `interrupts`; tests `MageArena.Tells.*`.

## 3. Named line combos

**Rule.** Three different lines cast in one order, each within the step window of the previous line cast, fire a
named effect on the third cast. Bolts in between neither count nor break the chain; repeating a line breaks it; the
window slides (the last three line casts). Split-hand casts count. Flow is untouched: combos sit beside it.

**Window conflict.** The owner asked for the Flow window (2.0 s, `combat.json#flow.differentLineWithinS`). A normal
sigil clip lasts 2.11 s (0.28 + 1.00 + 0.16 + 0.45 + 0.22, `generate.mjs:63-65`), so two back-to-back sigils land
about 2.1 s apart and no three-line order fits 2.0 s (derived from the clip spec, not measured). Proposal: an overlay
step window of 2.6 s (authored), Flow keeps its pinned 2.0 s. Owner question below.

| Order | Name | Effect on the third cast (authored) | Seated purpose |
|---|---|---|---|
| Tide Orb, Lash, Mirror | **Slack Water** | for 1.0 s the next magic hit on a raised ward (held or fresh) is a perfect | a sure catch, so a Reflection rally can start |
| Lash, Tide Orb, Mirror | **Still Pool** | +2.0 s tier-clock advance (one perfect's worth; the 6 s gap still holds) | reach tier IV sooner by craft |
| Tide Orb, Mirror, Lash | **Riptide Snare** | every target the Lash hits is rooted 1.0 s and loses a windup (soldier throw or mage cast) | clears the lip |
| Mirror, Tide Orb, Lash | **Breakwater** | each target the Lash hits returns 6 mana (max 18) | sustain under steel |
| Lash, Mirror, Tide Orb | **Twin Current** | the Tide Orb fires one more orb at +10 deg, same damage | burst at range |
| Mirror, Lash, Tide Orb | **High Tide** | the Tide Orb soft-locks to the nearest enemy in a 30 deg cone and its hit always counts for a stagger | the missing soft-lock as a reward (`combat.json` `aimMode` is not read today, `07-enemies-and-ai.md`) |

**Lash seated fix.** Every Lash row reaches 2.5-3 m (`spells-water.csv`); the nearest enemy body is about 3.5 m from
the centre pad (DF-004). Proposal: the player's Lash rows gain +1.5 m reach on the overlay (cone and ring 3 → 4.5 m,
Drown Coil 2.5 → 4.0 m), and Riptide's push also cancels a soldier's windup. Lash becomes the line that guards the
lip; mages at 5-8 m (`runtime.json` `magePreferredDistanceM`) stay out of its reach, which is the trade.

```json
"combos": { "enabled": true, "stepWithinS": 2.6, "boltBreaks": false, "orders": [
  ["tide_orb","lash","mirror","slack_water"], ["lash","tide_orb","mirror","still_pool"], ["tide_orb","mirror","lash","riptide_snare"],
  ["mirror","tide_orb","lash","breakwater"], ["lash","mirror","tide_orb","twin_current"], ["mirror","lash","tide_orb","high_tide"] ] },
"lashSeated": { "enabled": true, "reachBonusM": 1.5, "pushCancelsWindup": true }
```

**Lives:** overlay (both). Combos are VR mastery; offered to TV later as a CR only if they prove fun. **Hooks:**
`TrySpellCast` (`Water.cpp:76-156`) records the line history per actor; `ReleaseSpell` (`Water.cpp:158`) applies the
effect; `ApplyControl` (`Water.cpp:53-75`) gains the windup cancel. **Presentation:** the three sigil glyphs hang a
moment over the dais rail and fuse on the third; a named water sound. **Measured by:** census `combos` by name; tests
`MageArena.Combos.Orders`, `MageArena.Combos.LashSeatedReach`.

## 4. Reflection rallies

**Rule.** The player's Mirror II becomes **Reflection** through a VR player preset (the pinned Rotation has
`mirror: "B"`, Ripple, `runtime.json:30`; the pinned presets are read only). A perfect returns a magic projectile of
tier ≤ the reflector's tier (`ReflectProjectile`, `Water.cpp:369-398`). Today a returned projectile can never return
again (`bReflected`). v2 replaces that flag on the overlay path with a return count: **at most three returns per
projectile**, each return ×1.15 speed and ×1.25 damage of the shot it returns. The rival may return its **own** fire
when it comes back (mage-side reflect): Brennic catches the ember, holds it 0.35 s in his palm (readable), and throws
it back. Return 1 is the player's, return 2 Brennic's, return 3 the player's; after three, no one can reflect it.

Ember Dart (`spells-fire.csv`: 18 m/s, 8 dmg) over 6.5 m: the original flies 0.36 s; return 1 (player) 20.7 m/s,
10 dmg, 0.31 s; return 2 (Brennic, after the 0.35 s hold) 23.8 m/s, 12.5 dmg, 0.27 s; return 3 (player) 27.4 m/s,
15.6 dmg, 0.24 s. Derived from the steps, not measured.

Crack: a player return that hits +1; a return 3 that hits +1 more. Only `fire_bolt` is a projectile among the Fire
rows, so the rally is the ember, the pitch video's "catch a fireball" (`PROJECT-PLAN.md`, video 0:00-0:08).

```json
// presets.vr.json (new):
{ "presets": [ { "name": "Cassia", "lines": ["tide_orb","lash","mirror"], "branches": { "tide_orb": "pick", "lash": "B", "mirror": "A" } } ] }
// combat.vr.json:
"rally": { "enabled": true, "maxReturns": 3, "speedStep": 1.15, "damageStep": 1.25 }
```

**Lives:** preset (VR-owned, `presets.vr.json`) plus overlay `rally`. The return cap is shared semantics: **CR**.
**Hooks:** `ReflectProjectile` and its call (`ArenaThreats.cpp:230-233`); `React` (`MageAI.cpp:438-497`) uses the
rival's `returnsOwnFire.perfectChance` against a returned projectile it owns; the session picks the composition
(`FindRotation`, `ArenaSession.cpp:239`; `FindRotationPreset`, `SessionFlow.cpp:50-60`). **Presentation:** the ember
keeps its fire colour on every return (presentation element field, section 11); each return adds a brighter trail and
a higher ping; the crowd rises with the count. **Measured by:** census `reflects`, `rally_max`; tests
`MageArena.Reflect.*`.

## 5. Pad angles (flank)

**Rule.** A player shot whose source is more than 45 deg off the struck mage's facing **at the shot's release**
ignores its ward (no reduction, no perfect) and emits `flank`. The release tick is used because the pinned AI turns
to face a threat it reacts to (`Result.Aim = Brain.TargetPoint`, `MageAI.cpp:719-723`), so a check at impact could
never fire. A mage faces its last aim, refreshed only on decision ticks (`MageAI.cpp:713`, every 0.45-0.50 s at
competence 1-1.5). The flank therefore comes from a **cross-blink** (side pad to side pad, then a bolt before the next
decision) or a **Mirage** decoy (Mirror III) pulling the mage's aim (`MageAI.cpp:710`).

Geometry (derived from `arena-layout.json` pads, not measured): seen from a mage straight ahead at distance d, the
side pads are 2·atan(2.976 / (d − 0.375)) apart: 65 deg at 5 m, 49 deg at 7 m, 43 deg at 8 m. A cross-blink flanks
when the mage is closer than about 7.5 m; centre to side (33 deg at 5 m) never does.

```json
"flank": { "enabled": true, "minOffFacingDeg": 45, "judgedAt": "release", "bypasses": ["ward", "frontBlock"] }
```

**Lives:** overlay; offered as a CR with the pad id (CR-001 asks the shared kernel for a pad id). **Hooks:**
`SpawnProjectile` (`ArenaThreats.cpp:347-377`) stores the angle at release; `ResolveHit` skips `bGuarded` and
`bShielded` when it is over the limit. **Presentation:** the shot leaves a side-lit trail; the mage's ward ring flares
too late. No text. **Measured by:** census `flank_hits`; test `MageArena.Flank.SidePadBypass`.

## 6. Split-hand changes

| Change | Today | v2 rule | Data |
|---|---|---|---|
| Latch | set by a split cast (`ArenaKernel.cpp:281, 310`), cleared only on a ward-down tick (`:489-492`); while set, no fresh window (`ArenaThreats.cpp:294-297`) | also clears when the actor has no pending cast and 0.3 s have passed since its last split cast released | `splitHands.latchClearIdleS: 0.3` |
| Bolt at Flow 5 | any split cast at Flow 5 is refused as `crest` (`ArenaKernel.cpp:263-267`; session mirror `WouldSplitRefuse`, `ArenaSession.cpp:623-637`) | a split **Bolt** (slot 0) is allowed, pays its own 3 mana, keeps Flow at 5 (the cast path already skips `FlowCast` with `bSuppressFlow`, `Water.cpp:84-85, 124`) and refreshes the idle timer so the Crest waits | `splitHands.boltAtCrest: true`, `splitHands.boltHoldsCrest: true` |

A perfect still needs a fresh raise (`MageArena.Ward.HeldNeverPerfect`). Lines at Flow 5 with the ward held stay
refused: the Crest needs both hands (SCHOOL-DEFENCES A). The existing test `MageArena.Defences.TierAndCrest`
asserts the old Bolt refusal and must be updated with the card. **Lives:** overlay. **Measured by:** census
`perfects` while `split_casts > 0`; tests `MageArena.Defences.SplitLatchClears`, `MageArena.Defences.SplitBoltAtCrest`.

## 7. DF-004 ember spit (option A)

**Rule.** A cinder hound at the hold line spits a slow magic ember at the player; its death burst becomes one last
ember thrown at the player instead of the 1.5 m area. Embers are tier 0 magic projectiles, so they can be absorbed and
perfected (4 mana back, +2 s clock, +1 Flow, pinned `combat.json#absorb.perfect`). The bite stays melee only and never
reaches the seat: hounds threaten by pressure and embers.

```json
"emberSpit": { "enabled": true, "enemyId": "cinder_hound", "damage": 5, "family": "magic", "tier": 0, "windupS": 0.8,
               "projectileMps": 6, "rangeM": 10, "radiusM": 0.25, "cooldownS": 3.0, "deathEmber": true, "deathDelayS": 0.6 }
```

Damage 5 and delay 0.6 are the pinned `onDeath.ember_burst` values (`enemies.json`); the rest is authored. At 6 m/s an
ember from the lip (3.5-6 m) flies 0.6-1.0 s with a shrinking ring at its aim point, so the 0.15 s perfect window is
readable. **Lives:** overlay, like the conscript throw (`combat.vr.json#attacks`). **Hooks:** `Enemies.cpp`
attack scheduling and `QueueDeathEffects`. **Presentation:** a glowing ember in fire colour with a ring chime.
**Measured by:** census `embers_reachable`, `ember_perfects`; tests `MageArena.Creatures.EmberSpit`,
`MageArena.Creatures.DeathEmber`.

## 8. Mudra seal (tier IV)

**Pose: recommend the clip's palms-facing pose** (both hands meet in front of the chest, palms facing each other,
fingers up, thumb and index together, `CLIP-SCHEMA.md:113`; `generate.mjs:66`) over the plan's triangle (palms forward,
thumbs and index fingers forming a triangle, `PROJECT-PLAN.md` 6.2). Reasons: palms forward is the ward pose (raise
within 35 deg of forward at 0.20 m, `WardDetector.h:30-33`), and two palms forward is the "both palms" restart gesture
of the offer phases (`08-session-arc.md`), so the triangle would raise a ward or restart the arc. Palm normals facing
each other sit about 90 deg off forward, well past the 50 deg lower angle. The staff detector already rejects a mudra
(`StaffDetector.h:16`). The glyph still needs a deliberate design (BACKLOG, mudra glyph).

**Rule.** When a line sigil is recognised at tier IV, the session holds the cast for up to 1.5 s. A seal inside the
window releases the tier IV row (`FInputFrame` gains `bSealed`). If the window closes, or the player blinks or raises
the ward inside it, the **tier III row of that line is cast instead** at its own mana (Tide Orb → Twin Tides, Lash →
Maelstrom Lash, Mirror → Mirage), and `seal` fires with 0. A failed seal is never a dead hand. While the ward is held
and the staff is not planted, tier IV is refused anyway (`splitHands.maxTier` 2); a planted staff frees both hands.

Detector (authored): both hands confidence ≥ 0.5; palm normals within 30 deg of facing each other; palm gap ≤ 0.12 m;
pinch ≥ 0.7; neither palm within 50 deg of forward; held 0.25 s. The clip holds 0.60 s after a 0.30 s approach
(`generate.mjs:66`), so the seal lands about 0.55 s into the window.

```json
"seal": { "enabled": true, "windowS": 1.5, "holdS": 0.25, "failFallsBackToTier": 3 }
```

**Lives:** overlay plus a new `MudraDetector` (plan section 7 names it). **Measured by:** census `seal_ok`,
`seal_fail`; tests `MageArena.Seal.Window`, `MageArena.Hands.SealDetector`.

## 9. Leviathan / Rain of Orbs pick

At the first intermission the three dais stones offer Tide Orb IV: left **Leviathan Orb** (A: unblockable, 6 m/s,
r 1.5 m, 40 dmg, 45 mana), right **Rain of Orbs** (B: five orbs over 50 deg, 9 each, 40 mana), centre **keep last**
(first time: Leviathan, the pitch beat). A palm held 0.40 s (`teach.json` `offerHoldS`); no stone in 20 s keeps the
default (`04-vr-campaign-design.md` section 1). The pick sets `branches.tide_orb` of the `Cassia` preset and is saved
with the bout. It has a collar meaning: Rain of Orbs is an area spell and pays Tithe; Leviathan does not.
**Lives:** preset plus session (`SessionFlow.cpp` intermission). **Measured by:** test `MageArena.Session.TideIvPick`;
census rows split by branch.

## 10. The collar economy as event mapping

The ledger lives in the session (`FCollarLedger`), fed only by kernel events, so it is deterministic and replays.

| Collar | Source | Kernel event read | Per event | Sub-cap per bout |
|---|---|---|---|---|
| Crack | perfect absorb | `perfect` on the player | +1 | 2 |
| Crack | a reflection that hits | `reflect` by the player, then its `hit` | +1 (+1 more for return 3) | 2 |
| Crack | clean blink through a black core | `evade` | +1 | 2 |
| Crack | interrupt | `stagger` with TargetId = player | +1 | 2 |
| Crack | sparing at missio | scene flag (not kernel) | +1 | - (per day) |
| Tithe | tier III-IV area spell | `cast` by the player whose row is ring, wave or fan (Maelstrom, Rain of Orbs, Return Tide) | +1 | - |
| Tithe | missed cast | `miss` | +1 | 2 |
| Tithe | Crest spent on spectacle | a Crest on a Tithe row | +1 extra | - |
| Tithe | finishing at missio | scene flag; non-lethal below the Summa | +1 | - (per day) |

Caps: **4 Cracks and 4 Tithe per bout** (authored). Cracks persist in the save and open the Breaking (threshold owned
by the campaign pages). Tithe earns renown now (+2 renown per Tithe on the aftermath tablet) and banks for the Games
day. **The jailers grow stronger:** each later bout of the same day starts its opponents with +1.0 s tier-clock
advance per banked Tithe (cap 6 s) and +3% opponent damage per Tithe (cap +15%), through `FVrPressure`
(`VrRules.h:87-92`). The collar never unlocks tier V; the Breaking is the only source, at the Summa.

```json
"collar": { "enabled": true, "crackCapPerBout": 4, "titheCapPerBout": 4, "renownPerTithe": 2, "titheClockAdvanceS": 1.0,
            "titheClockCapS": 6.0, "titheDamageStep": 0.03, "titheDamageCap": 0.15 }
```

**Lives:** the mapping and caps are VR; the collar, Cracks, Tithe and tier V are canon: **CR to TV**.
**Presentation:** on a Crack, light leaks from the collar and the wrist cuffs with a glass-crack sound; on a Tithe,
the Wardstones pulse and the crowd chants low. Tallies appear only on the aftermath tablet.
**Measured by:** census `cracks`, `tithe`; tests `MageArena.Collar.Ledger`, `MageArena.Collar.TitheStrengthens`.

## 11. Threat colour by element, and the audio cues

Today every `magic` hit is drawn Water turquoise (`FamilyColour`, `SessionPresentation.cpp:66-81`); the kernel has no
element. v2 adds a presentation-only `Element` to `FProjectile` and `FTelegraph` (beside `OriginPos`, which the sim does
not read, `SimTypes.h:329-332`), set from the caster's school and kept on reflection. Family still wins: unblockable is
black with a red rim whatever its element (Sunfall), physical is steel.

Element colours (linear RGB): Water (0.0097, 0.745, 0.745) and Fire (0.761, 0.054, 0.014) exist in
`arena-layout.json#threats.order`; authored: Earth ochre (0.55, 0.30, 0.05), not red because the rim is reserved, and
Air violet (0.35, 0.20, 0.75), not white because white is steel. A colour-blind check is still owed.

| Cue | Trigger | Source | Notes |
|---|---|---|---|
| ring chime | a magic threat enters the perfect window (0.15 s before impact) | the threat, pitched by element | the absorb cue |
| perfect bell | `perfect` on the player | the ward palm | the teach already logs it (`ArenaSession.cpp:1455-1460`) |
| steel whistle | a physical projectile in flight | the projectile | Doppler |
| black-core drone | an unblockable telegraph from start to resolve | the telegraph origin | rising pitch |
| unlock chime | `unlock` or `surge` on the player | the casting wrist | plan 6.3 |
| tell hum, stagger crack | `tell`, `stagger` | the mage | |
| Crack | ledger Crack | the wrists | glass crack |
| Tithe | ledger Tithe | the Wardstones | stone thrum |
| crowd | murmur (default); swell (`phase`); hush (signature tell to impact); Crack chant "Cas-si-a"; Tithe chant (low, drinking); roar (finisher, victory) | the stands, front arc | ElevenLabs, pre-rendered |

## Status and gaps

- Nothing here is built; every value is authored and the census (`12-combat-v2-census.md`) will measure it.
- The cuff is head-locked today (`03-tier-clock-and-flow.md`), and there is no audio playback code under
  `apps/vr/Game/Source/` (`04-threat-language.md`); the diegetic light and section 11 need both.
- The Fire mage is not in the default arc (`BeginArc(1, false)`, `ArenaSession.cpp:1802`); Brennic needs the Fire flag
  on the semifinal. Earth and Air colours are listed only for completeness; no Earth or Air kernel exists.

## Open questions

- Combos: accept an overlay step window of 2.6 s, or keep the pinned 2.0 s and shorten the sigil clip?
- Is the phase-3 opener being uninterruptible acceptable, given the pinned Sunfall note "can be interrupted by a staff
  strike" (`spells-fire.csv`)? Seated, no staff strike exists.
- Should Tithe also carry across Games days (the campaign's "jailers grow stronger" over a season), or reset each day?
- The second-school final: if Corvo (Ember) is the fallback, is his signature Brand Wrath (absorbable, a different
  answer from Sunfall)?
- Brennic's voice lines are drafts; TV owns his voice and canon.
