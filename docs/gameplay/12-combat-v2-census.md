---
title: Combat v2 - census and test plan
channel: vr
status: proposed
verified-against: 35cd067 (2026-10-07)
sources:
  - "docs/gameplay/11-combat-v2.md"
  - "owner v2 vision log, 2026-10-07 (not yet in git)"
  - "docs/design-findings/DF-001-seated-wave1.md"
  - "docs/design-findings/DF-003-calibration-proposal.md, DF-003-census/sweep.csv, DF-003-census/summary.json"
  - "docs/design-findings/DF-004-creatures-cannot-reach-the-seat.md"
  - "docs/gameplay/08-session-arc.md"
  - "docs/campaign/04-vr-campaign-design.md"
  - "apps/vr/Game/Source/MageArenaVR/Tests/CensusTests.cpp, SessionTests.cpp, DefenceTests.cpp"
  - "apps/vr/Game/Source/MageArenaVR/Session/ArenaSession.h, ArenaSession.cpp"
  - "apps/vr/tasks/BACKLOG.md, docs/ORCHESTRATION.md"
data:
  - "apps/vr/data/pinned/.../combat.json#pacingTargets - read only"
  - "apps/vr/data/vr/combat.vr.json, apps/vr/data/vr/calibration-proposal.json"
---

# Combat v2 - census and test plan

The census is how the v2 rules on [11-combat-v2.md](11-combat-v2.md) get numbers the owner can sign. It extends the
DF-003 harness (`MageArenaDesign.Census.Sweep`, `CensusTests.cpp`) rather than replacing it: the same seeded,
headless, scripted seated player, now with the v2 techniques, measured bout by bout against the targets below. The
DF-003 nine-knob joint sweep is retired: structural rules fix what the knobs could not (Sunfall, perfects in bout 2),
and each remaining gate is tuned by one knob family, in session order. Targets marked **authored** are this page's
proposal; the pinned windows are from `combat.json#pacingTargets`.

## 1. Seated census targets per bout

| Bout | Opponent | Window | Pass rule | Other targets | Source of the target |
|---|---|---|---|---|---|
| 1 Wave 1 | 4 conscripts, 2 slingers | 25-40 s | won inside the window on ≥ 80% of seeds | median HP left on wins 20-60% | `pacingTargets.soldierWaveS`; DF-003 |
| 2 Creatures | 3 cinder hounds, 1 mire maw | 30-45 s | won inside the window on ≥ 80% of seeds | median `embers_reachable` ≥ 6 per bout (N = 6, authored); median `ember_perfects` ≥ 1 for the reference script | `pacingTargets.creatureWaveS`; DF-004 |
| 3 Semifinal | Brennic (Fire), competence 1 | 45-80 s median | win rate ≥ 70% | Sunfall loosed in 100% of duels that reach phase 3; `phase_reached` = 3 in every win | vision log wave 3 |
| 3 retry / hard | Brennic, competence 1.5 | 45-80 s median | win rate 40-60% | same Sunfall rule | vision log wave 3 |
| 4 Final | second-school entrant, competence 1.5 (fallback: Corvo, Ember) | 45-80 s median | win rate 40-60% | its signature in 100% of duels that reach phase 3 | `pacingTargets.duelS`; authored |
| Whole day | prologue to aftermath | 7-10 min | median inside the window | no bout outside its window on the median seed | vision log wave 0 |

Why 100% is reachable now: the phase gate clamps damage at each threshold, so every win passes through phase 3, and
the phase-3 opener ignores the tier clock and cannot be interrupted (11-combat-v2, section 1). DF-003 measured 0 of 40
seated duels with Sunfall because duels ended near 30 s, before tier IV (`DF-003-calibration-proposal.md`, row
`baseline`). A census row where phase 3 is reached and no Sunfall is loosed is a bug, not a balance miss.

**Day budget (authored, from measured and authored parts).** The full arc runs about 2.8 min today on the proposal
values (`08-session-arc.md`, T15).

| Part | Low | High | Basis |
|---|---|---|---|
| Prologue scene + teach | 60 + 63 s | 60 + 63 s | teach **measured** 63.3 s (T13); scene ≤ 60 s (`04-vr-campaign-design.md`) |
| Collar ritual (Septima) | 30 s | 45 s | scene ≤ 45 s |
| Wave 1, creatures | 25 + 30 s | 40 + 45 s | windows above |
| Brennic, final | 45 + 45 s | 80 + 80 s | windows above |
| Three intermissions (one with the stone pick) | 45 s | 60 s | 15-20 s each, authored |
| Aftermath tablet | 30 s | 45 s | scene ≤ 45 s |
| **Total** | **~6.2 min** | **~8.6 min** | every bout at its floor misses 7 min |

So the day reaches 7-10 min only if the bouts land near the middle of their windows (about 30, 37, 60 and 60 s); the
census checks `day_s` on the median seed, not a sum of floors.

## 2. Census harness changes

| Change | Today | v2 | Source |
|---|---|---|---|
| Scenarios | `wave1`, `duel-c1`, `duel-c15` (`CensusTests.cpp:725-727`) | `wave1`, `creatures`, `brennic-c1`, `brennic-c15`, `final-c15`, `day` | - |
| Seeds | 20 | 20 for waves; 40 for duels (a 70% win rate then has a binomial standard error of about 7 points) | DF-003 used 20 |
| Ruleset | live or `calibration-proposal.json` | live plus the v2 blocks, each switchable, so a row can be run with one block off (an ablation) | 11-combat-v2, section 0 |
| Scorer | one penalty over every gate (`summary.json`) | pass or fail per bout; no cross-gate penalty | this page, section 5 |
| Fire scenarios | `scenarios-vr/fire-duel-*.json` | unchanged, v2 blocks off; new `scenarios-vr/brennic-*.json` with v2 on | `apps/vr/data/scenarios-vr/` |

New CSV columns after the existing `candidate,...,sunfall_casts,sunfall_loosed` header (`CensusTests.cpp:409`):

`phase_reached, phase2_s, phase3_s, signature_s, signature_evaded, tells, interrupts, reflects, rally_max, flank_hits,
combo_<name> (six), seal_ok, seal_fail, tide_iv_branch, embers_reachable, ember_perfects, cracks, tithe, day_s`

Each column reads one kernel event from 11-combat-v2 section 0 (`phase`, `signature`, `evade`, `tell`, `stagger`,
`reflect`, `flank`, `combo`, `seal`) or the collar ledger. `embers_reachable` counts ember projectiles whose path
reaches the player's ward plane, whether or not they hit.

## 3. Reference controller update

The census player is `FArenaSession::DecideScript` (`ArenaSession.cpp:1153-1364`) under an `FSeatedPolicy`
(`ArenaSession.h:15-25`). Today it never casts Mirror, casts Lash only when an enemy is within 2.9 m
(`ArenaSession.cpp:1249`, `:1351`, so almost never seated), and has no tell or seal behaviour. Proposed new policy fields
(authored defaults for the `reference` policy):

| Field | Default | Behaviour in `DecideScript` | Priority (1 = first) |
|---|---|---|---|
| (existing) Sunfall escape | on | `TrySunfallEscape` (`ArenaSession.cpp:1070`): blink off the shadow; with the 2.4 m seated radius any adjacent pad clears it | 1 |
| `bReadTells`, `TellReactionS` | true, 0.35 s | on a `tell` event from a mage (not the opener), flick a Bolt after the reaction if it is ready | 2 |
| `bPerfectMagic`, `PerfectLeadS` | true, 0.10 s | raise the ward fresh when a magic projectile's ETA crosses the lead (embers, Ember Darts, returns); the teach's scripted lead is 0.22 s (`teach.json` `perfectLeadS`) | 3 |
| `bRally` | true | after a `reflect` with count 2 aimed at the player, re-raise fresh at the same lead | 3 |
| `bSeal`, `SealDelayS`, `SealFailEvery` | true, 0.30 s, 0 | play `mudra` this long after a tier IV sigil; fail every Nth seal (0 = never) | 4 |
| `TideIvBranch` | `"A"` | the stone pick for the run (`"A"` Leviathan, `"B"` Rain of Orbs) | at intermission 1 |
| `ComboOrder` | `""` | when set and mana covers it, cast that three-line order | 5 |
| `bFlankAfterBlink` | false | after a blink to a side pad, Bolt at once (spring item) | 5 |
| `LashReachM` | 4.4 | the Lash distance check, replacing the 2.9 m literal, to match the +1.5 m seated reach | 6 |

Split hands: the controller may Bolt with the ward up at Flow 5 (11-combat-v2, section 6); `WouldSplitRefuse` changes
with the kernel.

**Policies run every census.** `reference`; `novice` (`TellReactionS` 0.6, `PerfectLeadS` jittered ±0.08 s by seed,
`SealFailEvery` 3, no combos); `no-tells`; `no-seal` (every seal fails, so tier IV always falls back to III);
`branch-B` (Rain of Orbs); and DF-003's `eager-split`, `no-split`, `no-plant` as regressions. A balance that passes
only for `reference` is a finding, as in DF-003.

## 4. Automation tests to add

Regular tests (`MageArena.*`, run with the suite) prove the rule; design tests (`MageArenaDesign.*`) measure a target and
may stay red until tuning lands, as `Session.Wave1Seated` did.

| Test | Asserts |
|---|---|
| `MageArena.Kernel.PinnedPathV2` | with `Rules == nullptr` and every v2 block compiled in, the 45 conformance vectors are byte-identical and no v2 event is emitted |
| `MageArena.Phases.Thresholds` | a hit crossing 66% leaves the rival exactly on the threshold, emits `phase` 2, immune 1.0 s; same at 33% |
| `MageArena.Phases.Surge` | a break lifts player and rival one tier each, emits `surge`; at tier IV nothing rises (no tier V) |
| `MageArena.Phases.SignatureOpener` | at phase 3 the rival casts Sunfall within one decision at tier II, on cooldown and below 70 mana; a hit in its tell does not stagger |
| `MageArena.Phases.SeatedSunfall` | the opener's area is 2.4 m; a blink to the adjacent pad takes 0 damage and emits `evade` |
| `MageArena.Tells.Window` | a tier III-IV opponent cast emits `tell` and releases after lead + windup; Gentle doubles both |
| `MageArena.Tells.InterruptOnHit` | a ≥ 2.0 HP hit inside the window emits `stagger` and `interrupt`, keeps mana and cooldown spent, recovery +0.8 s; a second hit within 3.0 s does not stagger |
| `MageArena.Tells.NoInterruptOutside` | the same hit after release, or below 2.0 HP, changes nothing |
| `MageArena.Reflect.PlayerPreset` | the `Cassia` preset has Mirror A; a player perfect at tier II returns an Ember Dart |
| `MageArena.Reflect.RallyCap` | returns 1-3 step speed ×1.15 and damage ×1.25; a fourth perfect only absorbs |
| `MageArena.Reflect.MageReturnsOwnFire` | the rival returns only its own returned fire, after a 0.35 s hold |
| `MageArena.Defences.SplitLatchClears` | the latch clears 0.3 s after the last split cast releases; a fresh raise then perfects |
| `MageArena.Defences.SplitBoltAtCrest` | a split Bolt at Flow 5 casts for 3 mana and Flow stays 5; a split line at Flow 5 is still refused (`MageArena.Defences.TierAndCrest` updated to match) |
| `MageArena.Creatures.EmberSpit` | a hound at the hold line spits a tier 0 magic ember that reaches every pad; a fresh raise perfects it |
| `MageArena.Creatures.DeathEmber` | a dying hound throws one ember after 0.6 s; no 1.5 m area burst on the overlay path |
| `MageArena.Seal.Window` | a seal inside 1.5 s casts the tier IV row; a timeout, blink or ward raise casts the tier III row and emits `seal` 0 |
| `MageArena.Hands.SealDetector` | the `mudra` clips (normal, slow, sloppy) fire the seal; `both-palms` and `ward-raise` do not; a mudra never raises the ward |
| `MageArena.Session.TideIvPick` | a 0.40 s palm on the left or right stone sets the branch and survives save and continue; 20 s with no stone keeps the default |
| `MageArena.Collar.Ledger` | each event in 11-combat-v2 section 10 adds its Crack or Tithe; sub-caps and the 4 per bout cap hold |
| `MageArena.Collar.TitheStrengthens` | banked Tithe adds opponent clock advance and damage in the next bout, within the caps |
| `MageArena.Greybox.ElementColour` | a Fire-owned magic projectile draws vermilion, a returned one stays vermilion; unblockable stays black with a red rim |
| `MageArena.Combos.Orders` (spring) | each of the six orders fires its effect on the third line; a Bolt between does not break it; a repeat does |
| `MageArena.Combos.LashSeatedReach` | the player's Tide Lash hits a conscript on the hold line from the centre pad |
| `MageArena.Flank.SidePadBypass` (spring) | a release more than 45 deg off the mage's facing takes no ward reduction and emits `flank`; 45 deg or less is warded |
| `MageArenaDesign.Session.Wave1Seated` (kept) | seed 1 wins inside 25-40 s |
| `MageArenaDesign.Session.CreaturesSeated` (extended) | seed 1 inside 30-45 s, `embers_reachable` ≥ 6, `ember_perfects` ≥ 1 |
| `MageArenaDesign.Duel.BrennicSeated` (replaces `Duel.FireSeated`) | seed 1 at competence 1 and 1.5: inside 45-80 s, phase 3 reached, Sunfall loosed |
| `MageArenaDesign.Session.TiroDaySeated` (replaces `Session.FullSeated`) | seed 1 plays the whole day inside 7-10 min |
| `MageArenaDesign.Census.V2` | runs section 2 and writes `docs/design-findings/DF-005-census/` with pass or fail per bout |

## 5. Tuning order

DF-003 swept nine overlay knobs jointly against every gate at once and found no set that passed both the wave and the
duel ("the two gates still pull apart", DF-003 Recommendation). v2 tunes in this order instead:

1. **Structural rules first, no knobs.** Land the split latch and Flow-5 Bolt, ember spit, Lash reach, the Reflection
   preset, phases with the clamp and the opener, tells. Run the census once on the live overlay (`wide` stays
   unadopted) and record it as the v2 baseline.
2. **Wave 1: one knob family, the planted staff** (`plantedStaff.durationS`, `physicalReduction`). DF-003 showed the
   plant is load-bearing ("A balance that needs the player never to plant clears neither"). Freeze when it passes.
3. **Creatures: one knob, `emberSpit.cooldownS`** (with `windupS` only if perfects stay at 0). Freeze.
4. **Brennic length: `rivals.brennic.hpMult` and `tells.leadS`.** Phases add about 2 s of immunity and the clamp;
   interrupts shorten fights. Freeze when the median sits in 45-80 s at both competences.
5. **Brennic win rate: the phase-2 and phase-3 style** (`decisionCadenceMult`, weights) only. The pinned competence
   rows are not touched. Freeze when competence 1 ≥ 70% and 1.5 sits in 40-60%.
6. **Final**, with the same two steps on its own rival block.
7. **Day length** is checked last; it is the sum of frozen bouts plus authored scenes, never a knob.

A later step may not move an earlier step's knobs; if it must, the earlier gate is re-run and re-frozen. The owner
signs each frozen gate, as for DF-003.

## 6. The 18 Nov slice and spring 2027

Ranked by value per card (a card is one Grok unit, `ORCHESTRATION.md`). Value means: how much of the vision log's
weak areas and pitch beats it closes, and what it unblocks.

| Rank | Item (11-combat-v2 section) | Cards | Why this rank | Due |
|---|---|---|---|---|
| 1 | Split latch clears + Bolt at Flow 5 (6) | 1 | tiny; unblocks perfects with the ward up, which Reflection, embers and Cracks need | 18 Oct |
| 2 | HP phases, surge, signature opener, seated Sunfall radius (1) | 2 | fixes Sunfall 0 of 40 and the unreliable climax | 18 Oct |
| 3 | Census v2 harness + reference controller (this page, 2-3) | 1 | nothing can be signed without it | 18 Oct |
| 4 | Element colour (11) | 1 | fire rendered turquoise is a review finding; small | 18 Oct |
| 5 | DF-004 ember spit + Lash seated reach (7, 3) | 1 | bout 2 finally teaches the perfect; Lash becomes useful | 18 Oct |
| 6 | Mirror II = Reflection preset + rally cap + Brennic returns his fire (4) | 2 | the pitch's opening shot | 23 Oct |
| 7 | Tells, interrupt, stagger (2) | 1 | read the tell; makes Brennic legible; feeds Cracks | 23 Oct |
| 8 | Leviathan / Rain pick on the stones + `presets.vr.json` + save (9) | 1 | the first choice; gives the pick a collar meaning | 23 Oct |
| 9 | Collar ledger, Tithe pressure, diegetic light, aftermath tallies (10) | 2 | the story spine in the hands | 31 Oct |
| 10 | Mudra detector + seal window and fallback (8) | 2 | the physical climax; riskiest gesture | 31 Oct |
| 11 | Audio cue set and playback, crowd states, taunts (11) | 2 | no audio exists; the desktop gate wants threat cues spatialised | 31 Oct |
| - | **Spring 2027:** named line combos (3) | 2 | mastery depth; needs the cadence question settled | spring |
| - | **Spring 2027:** pad-angle flank (5) | 1 | needs the pad id (CR-001) and playtests | spring |
| - | **Spring 2027:** Tithe across Games days, rival kits for Garran and Iskar, the tier V Breaking, Earth and Air threats | - | campaign scope | spring |

Slice total: 16 cards in the 11 days to the 18 Oct gate and the 24 days to 31 Oct. If the 23 Oct checkpoint slips,
ranks 9-11 shrink before anything above them: the ledger keeps events and the tablet but drops the light; the seal keeps
the clip path and drops sloppy-variant tuning.

## Status and gaps

- Nothing here is built. The existing census measured only the live overlay and DF-003's candidates
  (`DF-003-census/sweep.csv`); none of its numbers transfer to v2 rules.
- Known red today on a clean tree: `MageArenaDesign.Duel.FireSeated`, `Session.FullSeated`, `Session.Wave1Seated`
  (CLAUDE.md, as of 24bcdfc). This plan replaces two of them and keeps the third.
- The final's opponent depends on a second school in the kernel or the Corvo fallback; its census row waits for that.
- The `day` scenario needs the scene runner (campaign pages) to report scene durations; until then it sums bouts and
  the authored scene budget.

## Open questions

- Is N = 6 reachable embers per creature bout the right teaching dose, or should it be a perfect-rate target?
- Should the census run the Brennic duel at competence 1.5 at all in the slice, or only the final?
- Do the owner's win-rate bands apply to the `reference` policy only, or must `novice` also clear competence 1?
- Does the owner sign each frozen gate as it lands, or all six together before 31 Oct?
