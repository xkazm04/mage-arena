# School defences - seated mechanics from the owner's five ideas

Owner direction 2026-10-03 (`docs/DECISIONS.md`): each school gets its own defence, so a seated mage can survive while
casting. This document turns the five ideas into mechanics that fit the systems already built (palm ward, sigils, the
two-hand mudra, blink between pads, mana, stamina, tier clock, Flow, Heat). **Every number below is a starting proposal
for the calibration sweep, not a balance claim**; the owner signs off the calibrated values. Status: design draft, for the
owner's review.

## Why they are needed (the measurement)
From a seat the player loses Wave 1 at 11.4 s (DF-001 after option A): ~8.3 HP/s incoming against a 95 HP pool, and only
~4.2 HP/s dealt, because drawing a sigil takes ~2 s **and locks both the ward and the blink while the hand draws**. Every
mechanic below attacks one of those two gaps.

| | Idea | School | Fixes | Hands |
|---|---|---|---|---|
| **A** | Split hands: one hand holds the barrier, the other casts weaker spells | all (Water first) | the hand lock - defend and attack at once | off-hand ward + casting hand |
| **E** | Planted staff: a standing barrier that frees both hands for a two-handed spell | staff mages (Water first) | survival while channelling the heaviest spells | both, then both free |
| **C** | Fire wall: a wall that stops projectiles and burns crossers | Fire | incoming projectiles before they arrive | casting hand sweep |
| **B** | Stone form: invulnerable while channelling the heaviest spell | Earth | survival during the big cast | both fists |
| **D** | Air form: a window of high dodge | Air | incoming damage by evasion | both palms |

## A - Split hands (Water first; the baseline every school gets)
- **Gesture:** keep the off-hand ward raised (Space) and draw with the casting hand at the same time (1-3, Q). Today the
  clip system plays one gesture at a time; A needs **concurrent per-hand playback** (left hand holds the ward clip while the
  right hand plays a sigil clip) - the hands are independent in VR, so the pipeline must be too.
- **Rule:** while the ward is held, the casting hand may cast **Bolt and lines up to tier II** at a **one-hand power
  multiplier** (proposal 0.6) and does not build Flow; tiers III-IV and the Crest need both hands (A ends) or a planted
  staff (E). The ward keeps draining mana as now; no perfect absorb is possible on a ward that is also casting (perfects stay
  a two-hand skill moment).
- **Measured by:** seated Wave 1 damage dealt per second while taking hits (today 4.2 HP/s), and survival time.

## E - Planted staff (staff mages; the Water mage carries a staff in the data)
- **Gesture:** both hands close as if gripping a staff and thrust down ~25 cm ("plant"); repeat the gesture, or blink, to
  pull it up. New clips `staff-plant` / `staff-lift` (normal/slow/sloppy).
- **Rule:** planting raises a **dome barrier around the pad** for up to N s (proposal 6 s), paid in mana up front plus a
  drain: magic reduction like the ward (0.85), physical reduction stronger than the palm (proposal 0.6), no perfect. While
  planted, **both hands are free**: the two-hand mudra (tier IV) and the Crest can be cast without ward or blink. While
  planted the mage **cannot blink** (anchored) - the trade-off. Unblockables still pass the dome (the threat language holds).
- **Measured by:** survival while channelling tier IV; how often the Crest/mudra lands in Wave 1 and the duel.

## C - Fire wall (Fire school; the Fire mage AI uses it too)
- **Gesture:** the casting hand sweeps horizontally, palm down, low in front of the body.
- **Rule:** a wall of fire at a set distance in front of the pad (proposal 4 m, 90-degree arc, 3 s): absorbable and physical
  projectiles that cross it are destroyed; enemies that cross take burn damage; **unblockables pass through**. Costs mana,
  and each projectile it stops gives Heat (ties the defence into the Fire resource). The Fire mage AI raises it when
  projectiles are inbound - which also lengthens the Fire duel toward the 45-80 s band (DF-002).
- **Measured by:** projectiles stopped per wall; duel length against the Fire mage.

## B - Stone form (Earth)
- **Gesture:** both fists clenched and drawn to the chest, held.
- **Rule:** the body turns to stone for a short window (proposal 2.5 s): **invulnerable**, cannot ward or blink, and may
  channel the heaviest Earth spell without interruption. Paid with **Footing**.
- **Seated reading of Footing (answers the open Earth question):** Footing builds while the mage **stays on one pad**
  (stillness = not blinking), instead of "moving below a speed" - a seated player is always still, so the pad is the unit.
  Stone form spends the stacks.

## D - Air form (Air)
- **Gesture:** both palms open and sweep outward, as if dispersing.
- **Rule:** for a window (proposal 3 s) the body is half-air: a high chance to evade each hit (proposal 60 % physical,
  30 % magic; unblockables are not evaded), blink costs reduced. Paid with **Momentum**.
- **Seated reading of Momentum (answers the open Air question):** Momentum builds from **consecutive blinks and quick
  casting cadence** instead of running speed. Air form spends it.

## Order of work (proposal)
1. **T11** - A (concurrent per-hand clips + split-hand casting) and E (planted staff) for the Water slice, with gesture clips,
   VR-overlay rules, oracle tests and the effect on the seated design gate.
2. **T12** - C fire wall (player Fire later; Fire mage AI now) and the **calibration sweep** over the allowed knobs (wave
   composition, enemy pressure, player power, defence) for the seated Wave 1 and the Fire duel; a proposal of numbers for
   the owner's sign-off.
3. B and D with the Earth and Air school cards, after the owner confirms the Footing/Momentum readings above.
