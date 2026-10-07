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
