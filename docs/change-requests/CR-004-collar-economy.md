# CR-004 — The collar economy: Cracks, Tithe, the tier IV cap, tier V at the Breaking

Status: draft. Filed for the TV channel (`kiro/mage-arena-tv`, the canon owner). Not merged. Not built.

Date: 2026-10-07. Source: the owner's v2 vision log of the same day (waves 1, 2 and 5), drafted into
`docs/design/V2-DECISIONS-DRAFT.md` and waiting for sign-off. The vision makes **breaking the collar** the spine of the
whole game. The collar today is a tier clock and a backstory prop (`docs/campaign/01-story-core.md`, "World facts").
This request turns it into a persistent economy that the player's style of play feeds, and that decides the endings.

It adds canon: two counters, a cap, a fifth tier, a ritual and a four-route framing. TV owns all of it. VR does not ship
any of it until TV accepts, rejects or reshapes this request. Pinned data was not edited. No combat number was retuned.
Every number below is a **proposal for the census**, not a tuned value.

## What the port implements

### Two counters, both persisted across Games days

| Counter | In-world meaning | Earned by | Spent or read by |
| --- | --- | --- | --- |
| **Cracks** | Fractures in the player's own collar. Precision weakens the ward-iron. | Precise play (table below) | Never spent. Read at the Summa: enough Cracks open the Breaking. |
| **Tithe** | Spilled arena magic. The Wardstones drink it (`facts.json` `K-wardstones-drink`) and the Vigil's wards grow stronger. | Spectacle (table below) | Pays renown at the end of each bout. Read at each Games day's ritual: it tightens the collars of the next day. |

The two pull against each other on purpose: **precision against spectacle**. A player can win every bout either way.

Cracks sources. Each is a kernel event the VR kernel already emits or can emit without new rules (source in brackets):

| Event | Cracks | Rule |
| --- | --- | --- |
| Perfect ward | 1 | Same event that advances the tier clock 2 s (`ArenaThreats.cpp:316`) |
| Reflection return | 2 | A projectile sent back by Mirror II Reflection connects with its original caster. Each return in a rally counts (at most 3 per rally, CR-005 table "Techniques"). |
| Clean blink through a black core | 2 | A blink whose i-frames are active when an `unblockable` area or projectile would have hit the player, and the player takes 0 damage from it |
| Interrupt | 3 | An opponent's telegraphed cast is cancelled by the player during its tell (CR-005, "Tells and interrupts") |
| Spare at missio | 5 | The player chooses to spare an opponent at 0 HP (stone choice after the bout, never during it) |

Tithe sources:

| Event | Tithe | Rule |
| --- | --- | --- |
| Spilled tier III-IV magic | 1 | Each player cast of a tier III or tier IV row. The magic is loud whether it lands or not. |
| Missed cast | 1 | A **released** player spell that deals no damage to any body. A sigil the recognizer rejects releases nothing and pays nothing, so a tracking error is never taxed. |
| Crest spent on spectacle | 2 | A Crest (Flow 5) spent on a tier III or tier IV cast |
| Finish at missio | 5 | The player chooses to finish an opponent at 0 HP (meaning below the Summa: see "Open questions") |

Tithe's two effects:

- **Renown now.** At the end of each bout, `renown += floor(boutTithe / 4)`, capped at the tier's renown for that wave
  (`arena-tiers.json` `renown`). A showy player climbs the standings faster.
- **The jailers grow stronger later.** At the next Games day's collar ritual, the campaign Tithe total sets a band.
  Each band adds +1 s to every tier unlock after tier I for that whole day (the collar holds tighter), at most +3 s.
  Proposed bands: 0-39, 40-79, 80-119, 120 and above. The 6 s minimum gap is unchanged.

### The cap and the fifth tier

- **The collar caps the player at tier IV.** This is today's kernel cap (`SimConstants.h:31`, `TierCap` 4) given a
  reason. The cap applies to every collared mage, so opponents below the Summa are capped too.
- **Tier V exists, and only the Breaking unleashes it.** At the Summa, when the Breaking condition holds, the collar
  breaks during the final. The tier clock then unlocks a tier V one tier step after IV (the normal 6 s gap).
- Tier V needs one spell row per school. **This request does not author them.** VR proposes the gesture only: the
  two-hand mudra seal (plan section 6.2) held for 1.5 s after the line sigil, the same seal tier IV uses, held longer.
  TV authors the rows in the school sheets and the four `spells-*.csv` files.

### The Breaking condition (revises the Summa rule)

The pinned rule is trust ≥ 60 plus `K-wardstones-drink` (`docs/campaign/01-story-core.md`, "The Games"). The vision
adds Cracks:

| Ending | Condition at the Summa final |
| --- | --- |
| **Breaking** | Cracks ≥ the threshold (proposal: 120), **and** the player holds `K-wardstones-drink`, **and** at least one ally of the Four with trust ≥ 60 |
| **Champion** | The Breaking is not attempted, because Tithe is high (proposal: ≥ 120) **or** the player has no ally with trust ≥ 60. The player wins the Summa and stays collared. |
| **Betrayed** | The Breaking is attempted with ally trust below 60. The ally does not answer the seal. |

The Cracks threshold is set by the campaign census (four story days, about 16 bouts). A first estimate from the
seated data: a competent bout gives about 5 perfects, 1 reflection, 1 clean blink and 1 interrupt, so about 12 Cracks,
or about 190 over 16 bouts. 120 is about two thirds of that.

### The collar ritual

**Warden Septima's collar ritual opens each Games day** (`characters.json` `septima`, "ritual, third person"). It is a
static scene at the dais rail, at most 45 s. It does three things in order:

1. Septima locks the collars. The tier clock resets to tier I, as it already does each bout.
2. The Tithe band for the day is applied and spoken (one line, at most 10 words).
3. The player's collar shows its Cracks (diegetic: the collar is seen in a polished shield at the rail, since a
   first-person player cannot see their own neck).

### The four routes

The Four (Cassia of the Tide, Brennic of the Ember, Garran of the Stone, Iskar of the Gale) were collared together
after the ford (`facts.json` `K-ford-betrayal`). The vision makes each of them a **playable route**, one per school.
Today TV names Cassia as the only playable character (`season-bridge.json` `playableCharacter`). This request asks TV to
accept the four-route framing as canon: one route per member of the Four, each with its own school, its own Tent, the
other three as rivals and possible allies, and the same collar economy. Cassia's route ships first.

## Data the port reads

| Field | Where | New or existing |
| --- | --- | --- |
| `collar.cracks.<event>` | `combat.json` (proposed `collar` block) | new |
| `collar.tithe.<event>` | `combat.json` | new |
| `collar.titheRenownDivisor` (4) | `combat.json` | new |
| `collar.titheBands` and `collar.bandUnlockDelayS` (1.0, cap 3.0) | `combat.json` | new |
| `collar.tierCap` (4) and `collar.breakingTier` (5) | `combat.json#tierClock` | new; replaces the constant `TierCap` |
| `summa.breaking.cracksAtLeast` (120), `summa.champion.titheAtLeast` (120) | `arena-tiers.json` Summa tier | new beside the existing trust and knowing fields |
| Tier V rows | `spells-water.csv` and the other three | new, TV-authored |

The counters are a **pure fold over bout receipts** (`docs/campaign/04-vr-campaign-design.md`, section 7). Each receipt
gains `cracks` and `tithe` for that bout. A save can be re-verified by replaying the fold.

## Scenarios the port can replay

None yet. A VR scenario set will be added when the counters are built (proposed card T24 in
`docs/design/V2-ROADMAP.md`): one bout per Cracks event and one per Tithe event, each asserting the exact count, plus
one rejected sigil that asserts Tithe 0.

## Not in this request

No change to the 24 water spells, the 10 fire spells, the tier clock times, Flow, or the 45 conformance vectors. No
tier V spell row. No change to missio below the Summa. The tier-clock surge on a boss phase break is CR-005.

## Open questions

- What does "finish" mean at missio below the Summa, where nobody dies? VR proposes **taking their collar token**
  (a public humiliation that costs trust and pays Tithe). The alternatives are refusing their plea (the crowd's thumb
  turns, the opponent is dragged off) and breaking their staff.
- Does the Tithe band also strengthen enemies (competence, wave size), or only the collar clock? This request proposes
  the clock only, because it is the one lever every school feels the same way.
- Does the player learn that the Wardstones drink the Tithe before the Summa? If Tithe is visible from day one, the
  secret `K-wardstones-drink` is half-revealed. VR proposes calling it only "the Tithe" until the knowing is gained.
