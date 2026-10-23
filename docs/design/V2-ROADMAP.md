# Mage Arena VR - v2 roadmap (slice first)

Status: **active** from 2026-10-07. Source: `docs/design/V2-VISION.md` (the interview) and the DECISIONS entry
"2026-10-07 - v2 vision accepted". DECISIONS outranks this file; this file outranks the vision log on order and scope.

The slice (18 Nov) is Cassia's Tiro day: prologue, teach, the collar ritual, four bouts (soldiers, creatures, Brennic in
the semifinal, the final against a second school), an aftermath with the first crack and the Wardstones line. Every
row below is built **greybox first** (current phase rule) and measured headless before anyone tunes it.

## Gates this roadmap must meet

| Date | Gate | What the v2 rows must have done by then |
|---|---|---|
| Sun 18 Oct | Mechanics gate (plan section 1) | Lane A rows 1-5 green; the phased Brennic duel in the census targets band; the full Tiro day chains end to end on the reference script |
| Fri 23 Oct | R0 reconsider checkpoint | The second-school decision: if lane A row 7 is not green, cut to Corvo (DECISIONS) |
| Sat 31 Oct | Desktop core gate | Lane B and the slice's voice and SFX in; the census still in band after the art pass |
| Wed 18 Nov | Submission | Lane C only where it is green by the 11 Nov beta |

## Status 2026-10-08 (end of the first execution day)

Done and verified on `v2/slice`, merged to master (`MageArena.` 234/234 on the merged tree; workers were Claude
subagents because Grok's balance was exhausted, HTTP 402): **T17, T18, T19, T20, T21, T22, T23, T26**, with change
requests CR-004 and CR-005 written. The live overlay still loses Wave 1 (known); on the proposal the day loses Bout 3 to
Brennic.

Open for the owner (each file lists options and a recommendation):

| Item | Question |
|---|---|
| `docs/schools/AIR.md` | Sign off the Air spell sheet (a proposal; no canon table existed) and its nine kernel readings |
| Final rival | Lio (default now in data) or Iskar |
| DF-005 | The split latch clear cannot restore a perfect: re-arm the ward on clear? **Ruled 2026-10-20: A, re-arm on clear.** See DECISIONS 2026-10-20 (DF-003, DF-005..009); adopted by T39. |
| DF-006 | Phases shorten the duel; the chained day loses Brennic: adopt `wide` plus a phase-break beat? **Ruled 2026-10-20: B with A (phase-break beat, adopt `wide`).** See DECISIONS 2026-10-20; adopted by T39. |
| DF-007 | Ember spit makes Bout 2 run 52 s live: thin the script's perfects and slow the spit? **Ruled 2026-10-20: A and B together, then re-measure.** See DECISIONS 2026-10-20; adopted by T39. |
| DF-008 | Lio loses every duel in ~20-24 s: AI first (ward, Air Form, tier II-III)? **Ruled 2026-10-20: A, AI first; re-measure.** See DECISIONS 2026-10-20; adopted by T39. |
| DF-009 | Tithe is mostly missed bolts: drop the Bolt miss weight? **Ruled 2026-10-20: A, count only spectacle.** See DECISIONS 2026-10-20; adopted by T39. |
| `collar.json` | The Cracks and Tithe weights (proposal) |
| T26 audio | Listen to `runs/T26/audio/brennic-opening.wav` (not heard by a person yet; local only) |

Next cards: C1 (mudra seal shape, Claude + owner), `apps/vr/tasks/T24-mudra-seal-detector.md`, `apps/vr/tasks/T25-closed-fist-pose-and-spare-or-finish.md`, C2 (story bible and slice script, Claude + owner), `apps/vr/tasks/T27-voice-and-chorus.md`, `apps/vr/tasks/T28-intermission-scenes.md`, `apps/vr/tasks/T40-two-music-loops.md` (the two music loops), `apps/vr/tasks/T41-cold-desktop-playtests.md` (the cold testers); T39 (adopt the DF recommendations and re-run the design suite, on Wolf). CR-006 and CR-007
are filed and open: `docs/change-requests/CR-006-collar-ledger.md` and `docs/change-requests/CR-007-air-school.md`.
Housekeeping: the
conformance generator (BACKLOG), the stalled T16 in the main checkout.

## Status 2026-10-20

Four owner rulings landed today (see `docs/DECISIONS.md`, the four 2026-10-20 entries, which outrank this file):

- Cut slice: no cut. Every bout stays, including Bout 2 and the Air final; the 31 Oct desktop core gate is planned on the
  full Tiro day, with the risk flagged (the 18 Oct gate was missed and the 23 Oct checkpoint is close).
- DF-003, DF-005, DF-006, DF-007, DF-008, DF-009: every recommended option adopted; card T39 implements them and does the
  one design-suite re-run on Wolf. The design suite stays red until that run.
- T38 asset source: an authored rig with four clips, by Claude and the owner; the local TRELLIS and Blender pipeline on
  Lynx may feed it.
- Unreal host: all Unreal work on Wolf (x64); Lynx gets no Unreal install and reports no UE results.

CR-006 and CR-007 are filed and open (paths under "Next cards" above).

## Lane A - combat core (Grok cards, mechanics gate)

| # | Card | What | Depends on | Canon |
|---|---|---|---|---|
| 1 | T17 | Split hands: the perfect latch clears after ~0.3 s casting-hand idle; a split Bolt is allowed at Flow 5 and spends no Crest | - | overlay only, no CR |
| 2 | T18 | Ember spit (DF-004 A): hounds spit slow, telegraphed magic embers from range; the death burst throws one last ember; Bout 2 perfects become possible | - | overlay now; CR-004 |
| 3 | T19 | Named-mage phases (DF-003): HP-gated phases (66 % / 33 %), a tier-clock surge of one tier on each break, the signature spell opens the last phase (Brennic: Sunfall); census against the new targets | - | overlay now; CR-005 |
| 4 | T20 | VR player preset: Mirror II = Reflection, and the Leviathan / Rain of Orbs pick on the stones at the first intermission | - | VR data only |
| 5 | T21 | The Tiro day chain: ritual (both palms), soldiers, creatures, Brennic semifinal, final (Corvo placeholder until row 7), aftermath; `FullSeated` measures the day against 7-10 min | 2, 3 | - |
| 6 | T22 | Collar ledger: Cracks and Tithe tallied from kernel events per bout, saved across days, shown only on the aftermath tablet (greybox) | 5 | CR-006 (Cracks, Tithe, tier V cap) |
| 7 | T23 | Air as an AI opponent (Gale Tent, spells, defence D air form, AI tuning) for the Tiro final | 3 | CR-007 |

## Lane B - signature moments and choice (desktop core gate)

| # | Card | What | Who |
|---|---|---|---|
| 8 | C1 | The mudra seal shape: a deliberate two-hand seal for tier IV (BACKLOG mudra item) | Claude + owner |
| 9 | T24 | Mudra seal detector, 1.5 s data window, the seal gates the tier IV cast | Grok, after C1 |
| 10 | T25 | Closed-fist held pose, and spare (open palm) or finish (fist) at missio; writes the arena flag | Grok |
| 11 | T26 | Element-correct colour (fire reads as fire, not turquoise) and the first `UAudioComponent`: A05 SFX plus threat cues, spatialised | Grok |
| 12 | C2 | Story bible v0 and the slice script: ritual words, Brennic's taunts, crowd chorus, intermission secrets, the Wardstones line (10 words or fewer each). **Drafted 2026-10-21: `docs/campaign/05-c2-slice-story-bible-and-script.md`** + `apps/vr/data/vr/story/slice.json`. 27 cues, 3 intermission scenes, a 213 s natural story budget that puts the day at 7.0-8.5 min. Awaiting the owner on D1-D4; the Wardstones line is **cut** (D5) and **CR-008 is not written** (D7: no "four routes" are defined anywhere in the repo) | Claude + owner |
| 13 | T27 | Voice and chorus from the C2 cue sheet (ElevenLabs, pre-rendered, count on the card), wired to the ritual, phase breaks and the aftermath | Grok |
| 14 | T28 | Intermission scenes on the stones: one optional choice per intermission (secrets, the first pact), never blocking (20 s) | Grok, after C2 |

## Lane C - mastery techniques (by the beta if green, else spring 2027)

| # | Card | What |
|---|---|---|
| 15 | T29 | Interrupt: a tell window before every tier III-IV mage cast; a hit inside it staggers, cancels and gives a Crack |
| 16 | T30 | Reflection rallies: mage-side reflect, perfect a returned projectile back, +speed and +damage per exchange, cap 3 |
| 17 | T31 | Pad angles: a flank shot from a side pad more than 45 degrees off the mage's facing ignores the mage's ward |
| 18 | T32 | Named line combos: data rows keyed by the last three casts inside the Flow window |

## Spring 2027 (not in the slice)

Veteranus, Primus and Summa story days; exhibition bouts; the Breaking bout and tier V; the endings (Breaking,
Champion, Betrayed) and pacts; the other three routes (Brennic, Garran, Iskar); all four schools playable.

## Change requests to TV

Each CR is written by the orchestrator after its card is verified, describing the rule as the VR kernel runs it (the
CR-001 to CR-003 pattern). Numbers stay overlay-only until TV accepts.

| CR | Content | From card |
|---|---|---|
| CR-004 | Ember spit attack and the death-burst ember | T18 |
| CR-005 | Phase breaks and the tier-clock surge | T19 |
| CR-006 | Cracks, Tithe, the tier IV collar cap, tier V at the Breaking | T22 |
| CR-007 | The second school | T23 |
| CR-008 | The four-route framing (story) | C2 |

## Open owner questions

1. Answered 2026-10-07: the second school is **Air** (Gale Tent: Iskar or Lio, defence D). See DECISIONS.
2. DF-003's knob sets (`wide` and the others) stay unadopted: T19 measures phases on the live overlay and on the
   proposal, and the owner picks from those numbers. **Ruled 2026-10-20** (DECISIONS "DF-003, DF-005, DF-006, DF-007,
   DF-008, DF-009: adopt every recommendation"): `wide` stays in the proposal file, not signed off; T39 adopts the
   rulings.
