# Mage Arena VR - v2 vision log

Status: **in progress** (vision interview with the owner, in waves of single- and multi-select questions).
Started 2026-10-07. Each wave records the owner's answers verbatim, then what the design does about them. When the
interview ends, the settled points move to `docs/DECISIONS.md` and become task cards; this file stays the history.

Rank: this is a proposal file (rank 5 in `docs/campaign/README.md`). It never overrides DECISIONS, the plan, code or TV canon.

## Method

| Wave | Topic | Output |
|---|---|---|
| 0 | v2 scope, story canon, story tools, weakest areas | framing (below) |
| 1 | Story pillars: Cassia's arc, the antagonist, tone, the collar as narrative | story bible v0 |
| 2 | Campaign structure: slice vs full campaign, Games days, scenes, branching, VR delivery | campaign outline v2 |
| 3 | Combat core: DF-003 and DF-004, the split-hands trade-off, a reliable climax | combat fixes + census targets |
| 4 | Combat depth: techniques, combos, opponent personalities, boss phases, school defences | combat v2 spec |
| 5 | Fit to tech and schedule: what lands by 18 Nov, what goes to spring 2027, Grok-sized cards | v2 roadmap |
| 6 | Synthesis and sign-off | DECISIONS entries + task cards |

## Wave 0 - framing (2026-10-07)

| Question | Owner's answer |
|---|---|
| What v2 means | **Full v2, carve the slice**: design the whole v2 game now; the 18 Nov build is a coherent vertical slice of it (its prologue / first chapter). |
| Story canon | Lives in the repo `docs/` (`docs/campaign/`, `docs/gameplay/`), which read TV canon (`kiro/mage-arena-tv`) by reference. |
| Story tools wanted | **Collar as narrative device**, **between-bout scenes**, **choices that branch**. (Not picked: voiced opponent barks.) |
| Weakest areas | **All four**: no real story, combat lacks depth and mastery, unreliable climax and finisher, enemy variety and personality. |

### Review findings that frame the next waves

From reading `docs/campaign/`, `docs/gameplay/`, `docs/design-findings/` and the backlog at `35cd067`.

**Story**
1. TV canon is rich: the Four betrayed at the ford, four Tents with temperaments, Brennic and Cassia, the Wardstones
   that drink spilled arena magic, and the Breaking ending. **None of it reaches the VR player.** All 24 strings are
   instructions, and no name, stake or motive appears on screen.
2. The campaign proposal (`04-vr-campaign-design.md`) is structurally sound: a ladder of Games days, three stones for
   choices, arena flags as checks, a TV quest-graph port. It is cautious on drama, though: at most 90 s of story per
   day, static tableaux, Cassia has no personal arc, and the conflict lives in the TV camp, which VR cut.
3. **The strongest unused hook is the collar.** In the arena the collar runs the tier clock, and in canon the
   Wardstones drink arena magic. A link between how you fight (perfects, reflections) and the collar's story is
   possible without new systems.

**Gameplay**
4. **The beats the pitch video promises are not in the build:** Mirror Reflection (the player has Ripple), Leviathan
   Orb (Rain of Orbs is fixed), the mudra seal (no detector), Sunfall (cast in 0 of 40 seated duels), the Fire mage
   (the default arc spawns Water proxies), fire colour (rendered turquoise), and no audio at all.
5. **Depth:** split hands kill perfects for the rest of a ward hold. Lash's 3 m reach is shorter than the ~3.5 m
   minimum enemy distance. A split Bolt is refused at Flow 5. Bolt has no soft-lock. The skill ceiling is
   perfect-timing plus line rotation, and little else.
6. **Pacing:** with the proposal values the full session is about 2.8 min against a 7-10 min target. That gap is room
   for story beats and for longer, phased duels.

## Wave 1 - story pillars (2026-10-07)

| Question | Owner's answer |
|---|---|
| Story spine | **Breaking the collar**: Cassia learns the Wardstones drink the magic spilled in the arena; the arc runs from obedient entrant to the Breaking at the Summa. |
| The Ember mage | **Brennic, one of the Four**: collared with Cassia at the ford, so it is a personal duel. |
| Tone | **Mythic and operatic**: ritual language, a chorus-like crowd, the weight of the gods on the collar. |
| Collar mechanics | **All four**: cracks from mastery, a collar ritual each Games day, power the collar withholds, jailers feed on your magic. |

### What the design does with it (proposal for Wave 2 onward)

- **One collar, two pulls: Precision against Spectacle.** Every bout produces two quantities from the existing kernel
  events: **Cracks** (perfect absorbs, reflections, a clean blink through a black core, sparing) and **Tithe** (magic
  spilled into the sand: tier III-IV area spells, missed casts, Crests spent on spectacle). Cracks persist across Games
  days and open the Breaking. Tithe feeds the Wardstones: the crowd and the Lanista love it (renown), the jailers grow
  stronger (later bouts gain wards and harder opponents). This turns "fight cleanly or fight loudly" into the moral
  choice, without a menu.
- **The collar withholds tier V.** Tier IV stays the cap the collar allows. Crack thresholds unseal story beats, and the
  Breaking bout at the Summa unlocks one uncapped tier V spell per school: the climax the duel lacks today.
- **The collar ritual opens every Games day.** Warden Septima locks the collar while the player offers both wrists (the
  existing both-palms pose). The ritual words are the day's mythic framing, and the crack count shows as light leaking
  from the collar's seams in a mirror held to the player.
- **Operatic tone within VR limits.** Spoken lines stay at 10 words or fewer, world-locked. The opera comes from
  ritual repetition, a crowd chorus (chants that react to Cracks against Tithe), music and staging, not from long text.
- **Canon impact.** Cracks, Tithe and tier V are new canon, so they go to TV as one change request before they ship.
  `K-wardstones-drink` already supports Tithe.

## Wave 2 - campaign structure (2026-10-07)

| Question | Owner's answer |
|---|---|
| Collar economy | **Precision against Spectacle**: Cracks from clean play (saved, open the Breaking); Tithe from spilled magic (renown now, a stronger jailer later). |
| Story in the 18 Nov slice | **A full Tiro day, four bouts**: prologue, teach, ritual, soldiers, creatures, Brennic in the semifinal, then the Tiro final against a second school. |
| Full campaign shape | **Authored chapters plus a ladder**: one hand-authored story Games day per tier (Tiro, Veteranus, Primus, Summa), each with a named rival and a set-piece boss phase; optional exhibition bouts between them for mastery and Cracks. |
| Branching choices | **Spare or finish at missio**, **secrets at intermissions**, **pacts with other Tents**. (Not picked as its own branch: an ending decided by fighting style alone. The collar economy still feeds the endings through Cracks.) |

### Consequences

- **The slice needs a second opponent school by mid-November.** The Tiro final is "the winner of the other semifinal",
  an Earth (Stone Tent: Garran or Senna) or Air (Gale Tent: Iskar or Lio) entrant. That means one more school in the
  kernel as an **AI opponent only** (spells, defence B or D, AI tuning), plus its CR to TV. This is the slice's largest
  new scope item. Wave 5 sizes it against the 23 Oct checkpoint and the 31 Oct gate, and sets the fallback: a second
  Fire entrant (Corvo), which needs no new school.
- **Endings are gated by three things:** Cracks (the collar must be cracked enough), the Knowing `K-wardstones-drink`
  (learned through intermission secrets), and trust or pacts (who stands with you at the Summa; spare or finish moves
  it). Champion: high Tithe, or the Breaking attempted without allies. Betrayed: the Breaking attempted when trust is
  below 60. Breaking: all three met.
- **Choices use the stones, never during a bout.** Spare or finish is the one exception to "choices only in scenes".
  It happens at missio, when the bout is already over: an open palm spares, a closed fist finishes. The fist is a new
  held pose (also designed for Earth's stone form, not built yet).
- **Slice ending:** the day closes on the aftermath of the final. The first crack is visible if the player earned it,
  and the hook is the line that the Wardstones drink.

## Wave 3 - combat core (2026-10-07)

| Question | Owner's answer |
|---|---|
| Reliable climax (DF-003) | **HP-gated boss phases**: named mages get 2-3 HP-gated phases; each break surges both collars (tier clock +1) and swells the crowd; the last phase opens with the rival's signature spell (Brennic: Sunfall). |
| Bout 2 perfect practice (DF-004) | **Option A, ember spit**: hounds spit slow, telegraphed magic embers from range, and a death burst throws one last ember. |
| Split hands | **The latch clears, and a Bolt is allowed at Flow 5**: perfects return after the casting hand has been idle about 0.3 s; a split Bolt at Flow 5 does not spend the Crest. |
| Signature moments in the slice | **All four**: Mirror II = Reflection (a VR player preset), the Leviathan / Rain of Orbs pick at the first intermission, the two-hand mudra seal for tier IV, and element-correct colour plus threat audio. |

### What changes

- **DF-003 gets a direction instead of a knob set.** The census targets become: the rival's signature spell in 100 % of
  duels that reach the last phase, duel median 45-80 s, win rate at least 70 % at competence 1 and 40-60 % at 1.5. Phase
  thresholds (proposal: 66 % and 33 % HP) and the surge live in the VR overlay. The kernel needs one new rule: on a
  phase break the mage's tier clock jumps one tier. That is a CR to TV, since the clock is shared semantics.
- **DF-004 is closed as option A.** It needs a new ember attack row in `enemies.json` (a CR to TV), or, until that
  lands, a VR overlay attack.
- **Split hands:** two overlay rules (latch idle time 0.3 s, Bolt allowed at Flow 5 without Crest). No CR needed.
- **The signatures need a VR-owned player preset** (Mirror A, Tide Orb IV picked on the stones) in the overlay, because
  the pinned presets cannot be edited. The mudra needs a pose detector, a 1.5 s data window and a deliberate seal shape
  (BACKLOG). Audio is the first `UAudioComponent` in the build: A05 SFX plus new threat cues (ElevenLabs).

## Wave 4 - combat depth (2026-10-07)

| Question | Owner's answer |
|---|---|
| Mastery techniques | **All four**: read the tell and interrupt, named line combos, reflection rallies, pad angles. |
| Opponent personality | **Signature kit, tells and taunts**: a signature spell, a defence, readable tells, a voiced phase-break taunt of 10 words or fewer, and an AI style tweak, all authored data on the existing mage AI. |
| Cracks and Tithe feedback | **Diegetic collar and crowd**: light leaks from the collar and wrists on a Crack, the Wardstones pulse and the crowd chants on Tithe; exact tallies only on the aftermath tablet. |
| Playable schools | **All four at launch** (spring 2027). |

### What changes

- **Techniques, as rules:**
  - *Interrupt*: a tell window before every tier III-IV mage cast. A hit landing inside it staggers the mage, cancels
    the cast and gives a Crack.
  - *Combos*: named effects keyed to line order inside the Flow window, one per three-line order. These are data rows
    keyed by the last three casts.
  - *Rallies*: a reflected projectile can be perfected back. Each exchange adds speed and damage, capped at three
    returns. The kernel already has `ReflectProjectile`, so it needs a mage-side reflect and a rally counter.
  - *Pad angles*: the ward arc test already uses angle. A flank shot from a side pad more than 45 degrees off the
    mage's facing ignores the mage's ward. A pad charge is a later idea.
- **Tension to settle in Wave 5:** the Wave 1 spine is Cassia's story, and four playable schools means four
  protagonists. Canon offers a natural answer: the **Four** betrayed at the ford are the four Tent mains (Cassia,
  Brennic, Garran, Iskar), so each school is one of the Four, on one shared ladder seen from four sides.

## Wave 5 - fit to schedule and execution (2026-10-07)

| Question | Owner's answer |
|---|---|
| Protagonist with four playable schools | **The Four, one route each**: Cassia (Water), Brennic (Fire), Garran (Earth), Iskar (Air), one shared ladder seen from four sides. Cassia's route is the slice and ships first. |
| First cut if the 23 Oct checkpoint slips | **The second school goes; Corvo is the fallback**: the Tiro final becomes a second Ember entrant on the Fire kernel already built. |
| Voice | **Yes, for the slice**: ElevenLabs approval extends to pre-rendered voice lines (ritual, taunts, crowd chorus, 10 words or fewer), with no runtime calls. To be recorded in DECISIONS. |
| Execution | **Parallel cloud sessions**: 2-3 Claude cloud sessions draft the design pack in parallel; this session reviews and merges. |

## v2 in one page (synthesis, for the owner's sign-off)

**Story.** Four mages were betrayed at the ford and collared by the legion that used them. Now they fight in the Games,
and the Wardstones drink every drop of magic spilled on the sand. Each launch is one Games day: Septima's collar ritual,
four bouts, scenes at the intermissions, an aftermath. The tone is mythic and operatic, carried by ritual, a crowd
chorus and voiced lines of 10 words or fewer. One route per member of the Four; Cassia (Water) ships first.

**The collar is the system.** Clean play (perfects, reflections, interrupts, sparing) cracks the collar for good.
Spilled spectacle pays the Tithe: renown now, a stronger jailer later. The collar caps you at tier IV until the Summa's
Breaking unleashes tier V. Endings: **Breaking** (enough Cracks, the Knowing `K-wardstones-drink`, allies with trust of
60 or more), **Champion** (the jailers fed), **Betrayed** (the Breaking attempted without enough trust). Choices: spare
(open palm) or finish (fist) at missio, whom to hear at intermissions, pacts with other Tents.

**Combat.** Named rivals with a signature kit, tells and taunts, fought in HP-gated phases that guarantee a climax
(Brennic's last phase opens with Sunfall). Mastery means reading tells to interrupt, named line combos, reflection
rallies and pad angles. Split hands: the latch clears and a Bolt is allowed at Flow 5. The hounds spit embers.
Signatures: Mirror Reflection, the Leviathan pick, the mudra seal, fire that reads as fire, and threat audio.

**Slice (18 Nov).** Prologue, teach, a full Tiro day: soldiers, creatures, Brennic (semifinal, phased), then the final
against a second school (fallback Corvo). It ends on the first crack and the Wardstones line.

**Campaign (spring 2027).** One authored story day per tier (Tiro, Veteranus, Primus, Summa) with exhibition bouts
between them, four routes, and all four schools playable.

**Canon.** Cracks, Tithe, tier V, the phase-break clock surge, the ember spit and the four-route framing go to TV as change
requests before they ship.

## Design pack drafted (2026-10-07)

Three cloud sessions drafted the pack from this log; this session copied their files in and reviewed them. Every file
has status **proposed**, and nothing in it is built, simulated or signed off.

| Part | Files |
|---|---|
| Story | `docs/campaign/05-v2-story-bible.md`, `docs/campaign/06-v2-tiro-day-script.md` (beat-by-beat slice script, 54-line VO manifest) |
| Combat | `docs/gameplay/11-combat-v2.md` (rules mapped to kernel and overlay), `docs/gameplay/12-combat-v2-census.md` (targets, tests, tuning order) |
| Canon and plan | `docs/change-requests/CR-004-collar-economy.md`, `CR-005-boss-phases-and-clock-surge.md`, `CR-006-cinder-hound-ember-spit.md`, `docs/design/V2-ROADMAP.md`, `docs/design/V2-DECISIONS-DRAFT.md` |

### Merge review: inconsistencies between the drafts

- **Tithe for finishing at missio:** +1 in `11-combat-v2.md` but 5 in CR-004. One value must win before T26.
- **"Undertow"** (this log's combo example) is already Lash tier II branch A in `spells-water.csv`. The combat draft does
  not use it; no combo may take that name.
- **Tiro final default:** the story draft recommends **Lio (Air)** for theme. The roadmap recommends building **Corvo
  as the default** with Air as the upgrade, which reverses the owner's Wave 5 order. Footing and Momentum are still open
  and block any Air or Earth opponent.

### Owner questions raised by the drafts (deduplicated)

1. **Finish at missio:** all three drafts converge on taking the opponent's **collar token** (non-lethal; costs trust, pays Tithe). Confirm, and fix the Tithe value.
2. **Voice:** sign the ElevenLabs voice extension as a DECISIONS entry (drafted) before any line is rendered.
3. **Tiro final:** Lio (Air) with Corvo as the fallback (Wave 5 order), or Corvo by default (roadmap)? Corvo as a second Ember entrant needs a canon frame (Venno's wildcard, a CR).
4. **Guaranteed Sunfall:** the phase-3 opener is scripted (no tier or mana cost, cannot be interrupted), and the seated Sunfall radius drops from 3.5 m to 2.4 m so it can be left from a pad. Accept?
5. **Combo window:** three sigils cannot fit the 2.0 s Flow window (one sigil clip is 2.11 s). Accept a separate 2.6 s combo window?
6. **Phase surge:** does a phase break also advance the player's tier clock?
7. **New poses:** the mudra (palms-facing recommended over the triangle); Septima's both-wrists ritual pose, which may collide with both-palms start-over and resume; spare and finish as a palm or fist held over the stones.
8. **First Crack:** authored for every player at the Tiro aftermath (story draft), or earned?
9. **The Tithe secret:** when does the player learn that the Tithe feeds the Wardstones (`K-wardstones-drink`)?
10. **Endings:** the story draft separates Champion ("break it alone") from Betrayed ("break it with a finalist whose trust is below 60"). Confirm.
11. **Four routes:** these rewrite TV's single playable character, so CR-004 needs TV's acceptance.
12. **Session length:** with every bout at its floor the Tiro day is about 6.2 min, under the 7-10 min target.
13. **Still open from campaign page 04:** decisions 3 (LLM at runtime), 5 (retry against reward), 6 (story ownership), 10 (scene runtime port) and 11 (saves).
