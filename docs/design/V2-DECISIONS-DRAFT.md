# V2 decisions, draft for `docs/DECISIONS.md`

Status: **draft, not in `docs/DECISIONS.md`.** Each entry below is written in the house format (the owner's words, then
what the plan does) and is marked "DRAFT - for owner sign-off". The quotes are the owner's answers as recorded in the v2
vision log of 2026-10-07. Where `docs/DECISIONS.md` already says something, it still wins until the owner signs these.

## 2026-10-07 - V2 is the whole game; the competition build is a slice of it (DRAFT - for owner sign-off)

> "Full v2, carve the slice." Weakest areas: "story, combat depth, the climax and finisher, enemy personality."

- The whole v2 game is designed now. The 18 Nov build is a coherent vertical slice of it, not a separate game.
- Review findings the slice must answer: TV canon never reaches the player; the pitch beats are unbuilt (Reflection,
  Leviathan, the mudra, Sunfall in 0 of 40 seated duels, the Fire mage absent from the default arc, fire colour,
  audio); the split-hand latch; Lash's reach is shorter than the minimum enemy distance; the session runs about 2.8 min
  against 7-10. Roadmap: `docs/design/V2-ROADMAP.md`.

## 2026-10-07 - Story tools: the collar, scenes, branching choices (DRAFT - for owner sign-off)

> "The collar as a narrative device, between-bout scenes, choices that branch."

- Scenes run between bouts only, at the dais rail or on the floor, static, at most 45 s. Choices sit on the three dais
  stones (palm held 0.40 s), never during a bout. Lines are world-locked, at most 10 words.

## 2026-10-07 - The spine is breaking the collar; Brennic is the Ember entrant (DRAFT - for owner sign-off)

> "Breaking the collar (the Wardstones drink spilled arena magic). The Ember mage is Brennic, one of the Four. Tone:
> mythic and operatic."

- The campaign's arc is the Breaking. `K-wardstones-drink` becomes a visible mechanic (the Tithe, CR-004).
- "The Fire mage of the Ember Tent" is **Brennic**. The live arc spawns him, not the Water proxy.

## 2026-10-07 - The collar economy: precision against spectacle (DRAFT - for owner sign-off)

> "Cracks come from perfects, reflections, clean blinks through a black core, interrupts and sparing. They persist and
> open the Breaking. Tithe comes from spilled tier III-IV magic, missed casts and Crests spent on spectacle. It earns
> renown now, and the jailers grow stronger later."

- Rules and proposed numbers: CR-004. Both counters are folds over bout receipts, so saves replay.
- A sigil the recognizer rejects is not a missed cast and pays no Tithe; only a released spell that hits nothing does.

## 2026-10-07 - The collar caps at tier IV; tier V at the Summa's Breaking (DRAFT - for owner sign-off)

> "The collar caps the player at tier IV; tier V is unleashed at the Summa's Breaking."

- The kernel's existing cap (`TierCap` 4) becomes the collar's. Tier V rows are TV's to author (CR-004).

## 2026-10-07 - Septima's collar ritual opens each Games day (DRAFT - for owner sign-off)

> "Septima's collar ritual opens each Games day."

- A static rail scene, at most 45 s: lock, the day's Tithe band, the player's Cracks seen in a polished shield.

## 2026-10-07 - The slice is a full Tiro day of four bouts (DRAFT - for owner sign-off)

> "A full Tiro day of four bouts: prologue, teach, ritual, soldiers, creatures, Brennic's semifinal, then the Tiro
> final against a second-school entrant (Earth or Air, AI opponent only)."

- This answers `docs/campaign/04-vr-campaign-design.md` decision 1 with **four bouts**, against its three-bout
  recommendation. The second school is AI-only; the player stays Water.
- Budget: about 7.0-9.0 min (roadmap section 3).

## 2026-10-07 - Campaign: authored chapters plus a ladder (DRAFT - for owner sign-off)

> "One story Games day per tier (Tiro, Veteranus, Primus, Summa) with named rivals and set-piece boss phases, plus
> optional exhibition bouts."

- Four story days, then exhibitions for replay. The mastery rule from `arena-tiers.json` gates the next story day.

## 2026-10-07 - Choices: missio, secrets, pacts (DRAFT - for owner sign-off)

> "Spare or finish at missio; secrets at the intermissions; pacts with other Tents."

- Canon says nobody dies below the Summa. "Finish" below the Summa is non-lethal. **Proposal: take their collar
  token** (trust down, Tithe +5). Alternatives: refuse their plea, break their staff. Owner to choose.

## 2026-10-07 - Endings (DRAFT - for owner sign-off)

> "Breaking (Cracks, plus K-wardstones-drink, plus allies with trust of 60 or more); Champion (high Tithe, or no
> allies); Betrayed (the Breaking attempted with trust below 60)."

- Same three endings as campaign 04 decision 8; the conditions gain Cracks and Tithe (CR-004).

## 2026-10-07 - The climax is HP-gated boss phases (DRAFT - for owner sign-off)

> "HP-gated boss phases (66 % and 33 %). Each break gives tier clock +1; the last phase opens with the signature
> spell (Brennic: Sunfall). Census: the signature in 100 % of duels that reach the last phase; median 45-80 s; win rate
> at least 70 % at competence 1 and 40-60 % at 1.5."

- Rules: CR-005. The surge moves every mage's clock, the player's included. The opener waives tier and mana once.
- These targets replace DF-003's "Sunfall in at least 70 % of duels" for boss bouts.

## 2026-10-07 - DF-004: ember spit (DRAFT - for owner sign-off)

> "DF-004: ember spit."

- Option A. Row and census: CR-006. The bite stays melee-only.

## 2026-10-07 - Split hands: latch and Flow (DRAFT - for owner sign-off)

> "The latch clears after 0.3 s of idle; a Bolt is allowed at Flow 5 without spending the Crest."

- VR overlay rules, card T17.

## 2026-10-07 - The slice's four signatures (DRAFT - for owner sign-off)

> "Mirror II = Reflection (a VR player preset); the Leviathan / Rain of Orbs pick; the two-hand mudra seal;
> element-correct colour plus threat audio."

- A VR player preset replaces pinned Rotation's Mirror B (Ripple). Cards T18, T23, A10, A12.

## 2026-10-07 - Combat depth: techniques (DRAFT - for owner sign-off)

> "Read the tell and interrupt; named line combos; reflection rallies (up to three returns); pad angles (a flank more
> than 45 degrees off the mage's facing bypasses its ward)."

- Tells and interrupts: CR-005. Combos, rallies beyond one, and pad angles are cut 5 and 6 if time runs out.

## 2026-10-07 - Opponents have personality (DRAFT - for owner sign-off)

> "A signature kit, tells and voiced phase-break taunts, plus an AI style tweak."

- Brennic first (T19, T29). Taunts are pre-rendered, at most 10 words.

## 2026-10-07 - Feedback is diegetic (DRAFT - for owner sign-off)

> "Diegetic collar and crowd; tallies only on the aftermath tablet."

- No numbers during a bout. Perfects, Crests, Cracks, Tithe and time appear on the stone tablet after the day.

## 2026-10-07 - All four schools playable at launch, spring 2027 (DRAFT - for owner sign-off)

> "Playable schools: all four at launch, spring 2027."

- Supersedes campaign 04 decision 9 (ship with Fire plus Earth or Air). Earth and Air need the Footing and Momentum
  readings first.

## 2026-10-07 - The Four, one route each; Cassia first (DRAFT - for owner sign-off)

> "The Four, one route each (Cassia, Brennic, Garran, Iskar). Cassia's route ships first."

- New canon for TV (CR-004, "The four routes"). The slice is Cassia's route.

## 2026-10-07 - First cut: the second school falls back to Corvo (DRAFT - for owner sign-off)

> "First cut if the 23 Oct checkpoint slips: the second school falls back to Corvo (a second Ember entrant)."

- Cut order continues in `V2-ROADMAP.md` section 4.

## 2026-10-07 - ElevenLabs approval extends to voice lines (DRAFT - for owner sign-off)

> "The ElevenLabs approval extends to pre-rendered voice lines for the slice."

- Extends the 2026-10-02 approval (music and SFX). Lines are rendered at authoring time through pof's ElevenLabs
  provider, at most 10 words, shipped as assets; no runtime calls. A card that renders lines states the generation
  count. No other paid service is added.

## 2026-10-07 - Execution: parallel drafts, orchestrator merges (DRAFT - for owner sign-off)

> "Parallel cloud sessions draft; the orchestrator merges. Pushes are allowed to claude/* branches only."

- Extends `ORCHESTRATION.md`: cloud sessions may push to `claude/*` branches; the orchestrator verifies and merges.

## Backlog items these close

| BACKLOG item | Closed? | By |
| --- | --- | --- |
| DF-003 calibration | **Partly.** "How Sunfall should reliably appear" is answered: the phase-3 opener (CR-005). Adopting `wide` or `quarterward` stays open until census v2 (T22). | Boss phases entry |
| DF-004 hound bursts | **Yes**, option A | Ember spit entry |
| Footing and Momentum | **No.** The vision does not rule on them. They block S3 and a new-school final (T28). | - |
| Campaign 04 decision 1 (three or four bouts) | **Yes**: four bouts; Brennic in the semifinal; second-school final, Corvo as fallback | Slice entry |

## Campaign 04's twelve owner decisions, mapped

| # | Decision | Vision answer | State |
| --- | --- | --- | --- |
| 1 | Three or four bouts; the duel opponent | Four bouts; Brennic semifinal; Earth/Air final, Corvo fallback | **Changed** (rec. was three) |
| 2 | Who is the Fire mage; who is the Ember champion | Brennic | **Answered** for the entrant; the champion is **still open** |
| 3 | LLM at runtime | Not addressed; authored scenes and pre-rendered voice imply no | **Still open** |
| 4 | ElevenLabs for voice | Yes, and for the slice, not only after it | **Changed** (earlier than proposed) |
| 5 | Retry against reward | Not addressed; Tithe pays renown | **Still open** |
| 6 | Story ownership with TV | Not addressed; CR-004-006 follow it | **Still open** (implied yes) |
| 7 | Deaths or sine missione | "Spare or finish at missio" | **Changed**: finish must be non-lethal below the Summa; meaning open |
| 8 | Endings | Breaking, Champion, Betrayed | **Answered**, with new conditions |
| 9 | Campaign launch gate | All four schools at launch | **Changed** (stricter) |
| 10 | Port `quest.ts` or a VR runner | Not addressed | **Still open** |
| 11 | Local saves | Not addressed | **Still open** |
| 12 | Campaign length | One story day per tier plus exhibitions | **Changed** (from a mastery ladder of 6-8 days) |

## Open questions

- The meaning of "finish" below the Summa (collar token, refused plea, or broken staff).
- Who is the Ember champion of the last Games?
- Footing and Momentum readings, before any Earth or Air work.
- Decisions 3, 5, 6, 10 and 11 above.
