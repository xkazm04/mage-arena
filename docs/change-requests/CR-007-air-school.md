# CR-007 - The second school: Air, Momentum and the Gale rival Lio

Status: open. VR design record for the overlay, per DECISIONS 2026-10-21. Not merged. DF-008 (`docs/design-findings/DF-008-lio-too-weak.md`)
is open: Lio loses every duel in about 20-24 s, so the Air balance is not settled. The 23 Oct Air-or-Corvo cut is the
owner's call (`docs/DECISIONS.md`, 2026-10-07); this request records the Air rule whichever way that goes, and is filed
for the TV channel only if Air stays.

Date: 2026-10-20. The author of this request built and ran nothing; every line is read from the sources it cites. Pinned
data was not edited. No combat number was retuned.

## Owner decision it implements

`docs/DECISIONS.md`, 2026-10-07: "Second school for the Tiro final: Air" (owner, asked "Earth, Air, or Corvo now?":
"Air"). The Gale Tent entrant as an AI opponent with defence D (air form); card T23; Corvo stays the 23 Oct fallback.
Behind it sit the 2026-10-03 ruling that every channel's kernel implements all four schools, with the VR C++ kernel the
reference for a new school until the TV channel ports it, and the school-defences direction (Air gets D, air form;
`docs/design/SCHOOL-DEFENCES.md` section D, as cited by `docs/schools/AIR.md`). The roadmap row is
`docs/design/V2-ROADMAP.md` CR-007 "The second school", card T23.

## Seam

The Air kernel is a school switch on the actor (`FActor::Air.bSchool`). A non-air actor is untouched by every Air call
(`Kernel/Air.h`, header comment). The session chooses the Air final: `FGames.bAirFinal` turns the Tiro final into an
Air mage only when the school mages are on (`Kernel/Games.cpp` `MageSchool`); the semifinal stays Fire. The
`tiroFinal` block of `apps/vr/data/vr/combat.vr.json` names `school: air`, `rival: lio`. With no overlay, no school
mages and `bAirFinal` false, nothing changes: the final is the pinned water-proxy duelist and the 45
`Kernel.Conformance.*` vectors stay on today's path. The Air sheet is VR-owned and is read without entering the pin
hash (`docs/schools/AIR.md`, Kernel readings, Load).

## The rule

Citations: `AIR.md` = `docs/schools/AIR.md` (Kernel readings); `Air.cpp`, `Air.h`, `KernelDataAir.cpp`, `MageAI.cpp`,
`VrRivals.cpp`, `Games.cpp` are in `apps/vr/Game/Source/MageArenaVR/Kernel/`; `schools.json` is the pinned
`apps/vr/data/pinned/docs/design/baseline-fourteen-nights/design/data/schools.json` (read only). Line numbers are
approximate.

### Momentum

- Range `[0, 100]`; gain 20 per second while moving above 4 m/s; decay 25 per second while still (`schools.json`
  `air.combatIdentity`; applied in `Air.cpp` `UpdateAirResource`, ~469-483).
- Speed is this tick's displacement `|Pos - PreviousPos|`, read after the move and before the same tick's release.
  "Still" is zero displacement; between 0 and 4 m/s Momentum neither gains nor decays (`AIR.md`, Momentum gain). A
  Slipstream dash does not count as movement; it pays its own +30 (`AIR.md`).
- Thresholds apply when `momentum >= at`; a higher band replaces each effect it names (`AIR.md`, Thresholds). At 40:
  `spellsPierce 1` and roll stamina x0.6 (`schools.json`). At 80: spell range x1.3. Range x1.3 applies to projectile
  flight range, line and beam length and Downdraft's placed distance, not to the dash distance (`AIR.md`).
- `spellsPierce 1` is read as "a connecting, non-perfected projectile flies on through one more body"; the count is
  snapshotted at release (`AIR.md`, Kernel readings; `Air.cpp` `PierceLeft`). The alternative reading (a spell passes
  one ward) is an open owner point in `AIR.md`.
- Absorb drain x1.2 for every air actor's ward (`schools.json`; `Air.h` `AirAbsorbDrainMult`).
- Perfect absorb deflects a projectile of tier <= the air actor's unlocked tier along its facing, same speed, damage and
  tier; the deflected bolt is marked reflected and is not reflected or deflected again. For an air actor the deflect
  replaces the water Mirror reflection (`schools.json`; `Air.cpp` `AirTryDeflect` ~620-644; `AIR.md`, Deflect).
- A hit pays its Momentum to the thrower after it lands; an evaded hit pays none (`Air.cpp` `AirOnHitResolved`
  ~610-615; `AIR.md`, Air Form).

### The ten spells

Rows are `apps/vr/data/vr/schools/spells-air.csv`; the numbers are in the data table below. The load is strict: a header
change, a non-numeric cell, an unknown shape, damage, blockable or momentum_gain cell, a dash whose reach disagrees with
`range_m`, or Air Form notes without the two evade chances and the Momentum need are load failures
(`KernelDataAir.cpp`; `AIR.md`, Load).

| Tier | Row | Rule as the kernel runs it | Source |
|---|---|---|---|
| 0 | `air_bolt` Gale Dart | The always-available bolt: 24 m/s, 7 damage, +2 Momentum per hit | csv; `Air.cpp` |
| 1 | `air_shear` Shear | Two projectiles, a straight fan of +-12 deg (24 deg total), one activation. The blades diverge, they do not converge as the csv note says; beyond 2.35 m both pass a body on the aim line | csv; `AIR.md` Shear |
| 1 | `air_slipstream` Slipstream | 6 m dash, no stamina, no trail, +30 Momentum instant. The AI's main Momentum source | csv; `Air.cpp` ~446 |
| 2 | `air_veer` Veering Bolt | Circular arc from caster C to aim point T (clamped to range), leaving 60 deg off the line C->T and arriving 60 deg on the other side: R = L / (2 sin E), arc 2ER (1.209 L at 60 deg). Side by activation-id parity (even launches left), so no random draw. Past T it flies straight for the unused range | `AIR.md` Veering Bolt; `Air.h` `AirVeerArc`, `AirVeerSide` |
| 2 | `air_downdraft` Downdraft | Ground circle r 2.5 m at a point, 26 damage, +5 Momentum per target | csv; `AIR.md` |
| 2 | `air_form` Air Form | Defence D; see below | csv; `AIR.md`; `Air.cpp` ~439 |
| 3 | `air_squall` Squall | Four 14 m/s projectiles of 11. Windup 18 ticks; the first orb leaves on the release tick, the next three 15 ticks apart, each its own activation id (each can be perfected), thrown from where the caster stands then toward the aim point fixed at the press. Hands are busy during the volley; an interrupt drops the unthrown orbs | csv; `AIR.md` Squall; `Air.h` `AirChannelBusy` |
| 3 | `air_eye` Eye of the Storm | Momentum set and held at 100 for 5 s (gains discarded, no decay). Absorb drain x1.5 while it lasts, stacking with x1.2 to x1.8. Not rooted | csv; `AIR.md` Eye |
| 4 | `air_tempest` Tempest Lance | One windup of max(0.90, 1.20) = 1.2 s, caster rooted, interruptible by a staff strike. The 16 m line resolves at release from the caster along the aim fixed at the press, 85 damage, unblockable: a ward does nothing; a blink's i-frames or standing off the line do | csv; `AIR.md` Tempest Lance |
| 4 | `air_cyclone` Cyclone | The Brand Wrath channel: eight 0.25 s ticks of 12 along the current facing, each its own activation id; every tick may be perfected | csv; `AIR.md` Cyclone |

Downdraft, Tempest Lance and Cyclone pass the VR ruleset into the hit resolution, so the planted-staff dome reduces
them like any projectile; Fire's area and line rows do not (`Air.cpp` ~105 comment; `AIR.md`, Rulesets; DF-008 option
C). `Pressure.FireMageDamage` is not applied to the air mage (`AIR.md`; `VrRivals.cpp` ~123).

### Defence D, Air Form

- Cast 0.15 s, cooldown 14 s, mana 10, lasts 3 s, the window opens at release (csv; `AIR.md`).
- It needs Momentum >= 40 and spends 40, both checked and paid at the press with the mana; under Eye of the Storm the
  lock restores 100 on the next tick (`AIR.md`; `Air.cpp` `BeginAirCast` ~146-158).
- Each hit that reaches a formed air actor (after the roll and encase immunity, before the ward) draws once from the
  kernel RNG, key `air <id> evade <activation>`. Below 0.6 for physical, below 0.3 for every other blockable family, the
  hit is evaded: no ward, no damage, no metrics, no Momentum to the thrower, an `evade` event, and a projectile flies on
  through the body. Unblockable hits never draw (`Air.cpp` `AirTryEvade` ~591-605; `AIR.md`).

### The Air AI

`ChooseAirSpell` (`MageAI.cpp` ~396-480). Competence is the T07 model shared with Fire. In order, with no spell draw for
the first three:

1. Tempest Lance when the actor is tier IV or above, the row is legal and the target's roll is not running. No mana
   banking, unlike Sunfall.
2. Air Form when an aimed, non-unblockable threat is seen, the form is down and the row is legal.
3. Slipstream while Momentum is below Air Form's need (40), dashed along the current move, sideways when standing.
4. Otherwise a pick among the useful rows (damage above 0, or a zone), excluding dash and form: below level 2 one random
   draw; level 2 and above sorts Veering Bolt and Squall first, then damage x count, ties by slot.

A dash end is kept out of the dais hold and inside the narrow-view arc (`AIR.md`, The air AI; `Air.cpp`
`KeepOpponentPoint` ~125).

### The Gale rival Lio in the Tiro final

- `rivals.lio` (`apps/vr/data/vr/combat.vr.json`): school `air`, applies to tier `tiro`, wave 4, kind `final` (the
  pinned final, one mage at competence 1.5, `arena-tiers.json`). Phases at 0.66 and 0.33 of max HP, surge 1 tier per
  break, the last phase `opensWith: air_tempest`. The phase, surge and granted-cast rules are CR-005 unchanged.
- The granted Tempest Lance skips the tier gate, the cooldown and the mana cost, once per duel, and is cast in the
  rival's own school through `StartGrantedAirCast` (`VrRivals.cpp` ~118-127).
- `tiroFinal` is `{ school: air, rival: lio }`; its `_why` records that without a rival the final is Corvo, a Fire mage
  with no phases (`combat.vr.json`).
- The semifinal stays Brennic (Fire). The calibration sweep keeps the pre-T23 fire final (`AIR.md`, The final).

## Data

| Field | Value | Source file |
|---|---|---|
| Momentum range | 0 to 100 | `schools.json` `air.combatIdentity` |
| Momentum gain | 20 per s above 4 m/s | `schools.json` |
| Momentum decay | 25 per s still | `schools.json` |
| Threshold 40 | spellsPierce 1, rollStaminaMult 0.6 | `schools.json` |
| Threshold 80 | spellRangeMult 1.3 | `schools.json` |
| absorbDrainMult | 1.2 | `schools.json` |
| Perfect absorb | deflect, tier <= own unlocked tier | `schools.json` |
| Gale Dart | tier 0, 24 m/s, cast 0, cd 0.30, mana 4, dmg 7, range 14, +2 per hit | `spells-air.csv` |
| Shear | tier 1, 2 x 22 m/s at +-12 deg, cast 0.12, cd 3.0, mana 12, dmg 9 each, range 12, +4 per hit | `spells-air.csv` |
| Slipstream | tier 1, dash 6 m, cast 0, cd 5.0, mana 12, +30 instant | `spells-air.csv` |
| Veering Bolt | tier 2, 16 m/s, entry 60 deg, cast 0.45, cd 7.0, mana 22, dmg 24, telegraph 0.50, range 14, +6 per hit | `spells-air.csv` |
| Downdraft | tier 2, r 2.5 m, cast 0.40, cd 9.0, mana 20, dmg 26, telegraph 0.70, range 12, +5 per target | `spells-air.csv` |
| Air Form | tier 2, 3 s, cast 0.15, cd 14.0, mana 10, evade 60 % physical / 30 % magic, needs and spends 40 Momentum | `spells-air.csv` |
| Squall | tier 3, 4 x 14 m/s 0.25 s apart, cast 0.30, cd 11.0, mana 34, dmg 11 each, telegraph 0.30, range 12, +3 per hit | `spells-air.csv` |
| Eye of the Storm | tier 3, cast 0.40, cd 16.0, mana 28, Momentum locked at 100 for 5 s, absorb drain x1.5 | `spells-air.csv` |
| Tempest Lance | tier 4, line 16 m, cast 0.90, cd 25.0, mana 70, dmg 85, unblockable, telegraph 1.20, +10 per target | `spells-air.csv` |
| Cyclone | tier 4, beam 10 m for 2.0 s, cast 0.20, cd 20.0, mana 60, 12 per 0.25 s tick, telegraph 0.20, +2 per tick | `spells-air.csv` |
| Lio phases | 0.66, 0.33 (the second opens with `air_tempest`) | `combat.vr.json` `rivals.lio` |
| Lio surge | 1 tier per break | `combat.vr.json` `rivals.lio` |
| Lio slot | tier tiro, wave 4, kind final | `combat.vr.json` `rivals.lio`; pinned `arena-tiers.json` |
| Final school | air, rival lio | `combat.vr.json` `tiroFinal` |
| Final competence | 1.5 | pinned `arena-tiers.json` Tiro wave 4 |
| Evade chances | 0.6 physical, 0.3 other blockable (parsed from the csv notes) | `spells-air.csv`; `KernelDataAir.cpp` |
| Lio max HP | 95 at vigor 1 per the `_why` text of `rivals.lio`; [unverified] in code | `combat.vr.json` |

## VR-only today

The C++ kernel is the reference for all of it; the pinned desktop/TV kernel has Water only.

- Every row above: the Momentum resource and thresholds, deflect, the pierce reading, all ten spells, Air Form's RNG
  draw, the Air AI, `bAirFinal`, `tiroFinal` and `rivals.lio`.
- `spells-air.csv` is VR-owned. It has no pinned counterpart, and no tuning is signed off (`AIR.md`: proposal, owner
  sign-off pending).
- The `rivals` block is itself a VR overlay until CR-005 lands.

## What TV must decide

1. Whether Air stays after 23 Oct, or the final is Corvo (a Fire entrant, no phases).
2. `spellsPierce 1`: one extra target (the VR reading) or a ward pass (`AIR.md` open point 2).
3. Whether to accept the sheet as drafted or wait for the DF-008 outcome (AI first, sheet numbers, or the dome rule;
   options A, B and C).
4. One dome rule for Air and Fire area and line rows (DF-008 option C).
5. Lio or Iskar as the Gale rival (`AIR.md` open point 1; the slice default is Lio).
6. How a TV AI reads "moving above 4 m/s" for a non-seated actor. The seated reading is deferred to a future playable
   Air (`AIR.md`, Design intent).

## Out of scope

- A playable Air for the player and the seated Momentum reading.
- Earth, and the other schools' defences.
- Any retune. DF-008 reports Lio at median 21-24 s against a 45-80 s target and a win rate of 1.00 against 0.40-0.60 at
  competence 1.5; its remedies are not applied here.
- Presentation: the Air greybox colour, the Veering Bolt arc and Tempest Lance line telegraphs, Lio's body.
- Conformance vectors for Air. The existing 45 are unaffected by design; Air scenarios are kept separate
  (`docs/DECISIONS.md`, 2026-10-03). No run of them is claimed here.
- Any edit to `apps/vr/data/pinned/**`.
