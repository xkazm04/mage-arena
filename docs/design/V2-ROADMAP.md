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
| 12 | C2 | Story bible v0 and the slice script: ritual words, Brennic's taunts, crowd chorus, intermission secrets, the Wardstones line (10 words or fewer each) | Claude + owner |
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
   proposal, and the owner picks from those numbers.
