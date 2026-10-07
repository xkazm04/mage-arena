---
title: Earth and Air schools (status)
channel: vr
status: designed
verified-against: 46643e2 (2026-10-07)
sources:
  - "docs/DECISIONS.md"
  - "docs/PROJECT-PLAN.md"
  - "docs/design/SCHOOL-DEFENCES.md"
  - "apps/vr/tasks/BACKLOG.md"
data:
  - "apps/vr/data/pinned/docs/design/baseline-fourteen-nights/design/data/schools.json (schools[earth], schools[air])"
---

# Earth and Air schools (status)

Earth (the Stone Tent, resource Footing) and Air (the Gale Tent, resource Momentum) exist only as pinned data and as
design proposals. No VR code reads their `combatIdentity`, no spell table exists for either school in the pinned data,
and no Earth or Air opponent or player loadout is built. The owner decided on 2026-10-03 that every channel's kernel
implements all four schools, VR first, in the order Fire, then Earth and Air. Both schools' resources are defined by
walking speed in the pinned data, which a seated VR player never has, so each needs a seated reading before it can be
built; SCHOOL-DEFENCES proposes one, and the owner has not confirmed it.

## Pinned data (not read by the VR build)

| School | Tent | Colour | Resource | Range | Gain | Decay | Thresholds | Perfect absorb extra |
|---|---|---|---|---|---|---|---|---|
| Earth | the Stone Tent | `#7a6a3a` | footing | 0-6 | `stackPerHalfSecondBelowMps` 1.0 (the key reads as one stack per 0.5 s while moving below 1.0 m/s; no code interprets it yet) | `clearOnRollOrSprint` | at 1: damage taken -0.04 and spell power +0.04 per stack; at 3: absorb drain x0.8 | +2 Footing |
| Air | the Gale Tent | `#5f9e98` | momentum | 0-100 | `perSecondMovingAbove4Mps` 20 | `perSecondStill` 25 | at 40: spells pierce 1, roll stamina x0.6; at 80: spell range x1.3 | deflect: an absorbed projectile of tier <= own unlocked tier is redirected along the aim direction |

Air also carries `absorbDrainMult` 1.2. All values from `schools.json` at pin `68a4d68`. The pinned data has
`spells-water.csv` and `spells-fire.csv` only; there is no Earth or Air spell file.

## Designed seated defences (SCHOOL-DEFENCES.md)

| Idea | School | Gesture | Rule (proposal numbers) | Paid with |
|---|---|---|---|---|
| B - Stone form | Earth | both fists clenched and drawn to the chest, held | invulnerable for 2.5 s; cannot ward or blink; may channel the heaviest Earth spell without interruption | Footing |
| D - Air form | Air | both palms open and sweep outward | for 3 s: 60% chance to evade a physical hit, 30% a magic hit, unblockables not evaded; blink costs reduced | Momentum |

Proposed seated readings (owner confirmation pending, BACKLOG item "OWNER (design, before the Earth and Air cards)"):

| Resource | Pinned meaning | Proposed seated meaning |
|---|---|---|
| Footing | builds while standing still (below 1.0 m/s); cleared by a roll or sprint | builds while the mage stays on one pad (not blinking); Stone form spends it |
| Momentum | builds while moving faster than 4 m/s; decays while still | builds from consecutive blinks and a quick casting cadence; Air form spends it |

## Status and gaps

- **Designed, not built.** No `Earth.cpp` / `Air.cpp`, no Footing or Momentum fields, no gesture clips for stone form or
  air form (searched `apps/vr/Game/Source` for footing, momentum, earth, gale: no matches).
- **Plan versus decisions.** Plan section 6.8 lists "the other three schools as playable" as cut. The owner's decision of
  2026-10-03 ("all kernels need all 4 schools") outranks it for the kernel; it does not say that Earth or Air must be
  playable in the competition slice. The slice itself still ends on the Fire duel.
- **Order of work** (SCHOOL-DEFENCES, "Order of work"): B and D come with the Earth and Air school cards, after the owner
  confirms the seated Footing and Momentum readings. No task card exists for them in `apps/vr/tasks/` at this commit.
- **Data gap.** The desktop/TV channel owns the combat data; Earth and Air spell tables would have to be authored there
  (or proposed by VR through a change request, as CR-002 did for Fire) and then re-pinned.

## Open questions

- Does the owner confirm the seated readings (Footing = staying on one pad; Momentum = consecutive blinks and cadence)?
- Who authors the Earth and Air spell tables: the desktop/TV channel, or VR through a change request?
- Are Earth and Air meant to appear in the competition slice (as opponents, or as playable schools), or only in the kernel?
