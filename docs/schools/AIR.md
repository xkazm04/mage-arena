# Air school - Momentum and ten spells (PROPOSAL, owner sign-off pending)

Date: 2026-10-07. Status: **proposal**, VR-owned until the TV channel accepts it (CR-007). Owner decision the same day:
Air is the second school for the Tiro final (`DECISIONS.md`). Unlike Fire, no pinned Air spell table exists in either
repository, so this sheet is new design, drafted by the orchestrator. The numbers are starting values for the census
(card T23), not tuned.

Data: `apps/vr/data/vr/schools/spells-air.csv` (same columns as the pinned `spells-fire.csv`; `momentum_gain` replaces
`heat_gain`).

## Sources it is built from
- `schools.json` `air.combatIdentity` (pinned, unchanged): Momentum 0-100, gain 20/s while moving above 4 m/s, decay
  25/s while still; at 40 `spellsPierce 1` and roll stamina x0.6; at 80 spell range x1.3; absorb drain x1.2; a perfect
  absorb **deflects** a projectile of tier <= own tier along the aim direction.
- TV `docs/gameplay/data/arena-runtime/lab-schools.json` `air` (read-only): move x1.15, cast x0.75, projectile speed
  x1.35, damage x0.85, control x0.9, body `iskar`, prefix "Gale". The rows apply these against the Fire sheet.
- `docs/design/SCHOOL-DEFENCES.md` D: air form, high evasion for a window, paid with Momentum.

## Design intent
- **Fast and evasive, light per hit.** Fire hits hard and burns; Air arrives quickly and from odd angles. Against a
  seated player that means reading direction (Veering Bolt from the flank) and rhythm (Squall's four beats), which
  trains the perfect timing the tide orb and Reflection reward.
- **The AI is not seated.** The opponent stands on the arena floor and can move, so the pinned Momentum rule (speed
  above 4 m/s) applies to it unchanged. The seated reading (consecutive blinks, casting cadence) is only for a future
  playable Air.
- **Reuse before invention.** Seven rows reuse shapes the kernel already resolves (twin projectiles, dash, ground
  circle, self buff, line, channel). Three are new: the curving projectile, the staggered volley, and Air Form's evasion.
- **Signature: Tempest Lance** (tier IV, unblockable line, 1.2 s drawn telegraph): the rival's last-phase opener (CR-005
  pattern), answered by a blink like Sunfall but along a line instead of a circle.

## Open for the owner
1. Rival for the final: **Lio** (a Gale rival; the slice default here) or **Iskar** (one of the Four, so a more personal
   final but it spends a future protagonist's reveal).
2. `spellsPierce 1` read as "a spell passes through one ward or wall" (a ward still absorbs, but at reduction 0 for the
   first pierce)? Or "pierces one extra target"? T23 takes the second, literal reading unless told otherwise.
3. The ten numbers above are a first pass; the census decides the duel length and win rates (targets as DF-003).

## Kernel readings

Card T23, 2026-10-07. The C++ kernel (`Kernel/Air.{h,cpp}`) runs the sheet above as written. Where a cell can be read two
ways, the kernel takes the closest literal reading and the line is marked **Owner:**. No number in
`spells-air.csv` was changed.

- **Load.** The sheet is VR-owned, so it is read without entering the pin hash (`PinHash` is unchanged). The parse is
  strict: a header change, a numeric cell with anything but digits, an unknown shape, damage, blockable or
  momentum_gain cell, a dash whose reach disagrees with `range_m`, or Air Form notes without their two evade chances and
  the Momentum need are load failures (`MageArena.Air.StrictLoad`). The notes column is unquoted, as in Fire.
- **Momentum gain, "moving above 4 m/s".** Speed is this tick's locomotion: `|Pos - PreviousPos|` over one step, read
  after the move and before the same tick's release, so walking (4.5 m/s), sprinting and rolling count and a Slipstream
  dash does not (it pays its own +30). **Owner:** "still" is read as zero displacement; between 0 and 4 m/s (for
  example walking with the ward held, 4.5 x 0.5 = 2.25 m/s) Momentum neither gains nor decays.
- **Thresholds.** `momentum >= at`; a higher band replaces each effect it names (the Fire reading), so at 80 the pierce
  and the cheaper roll of the 40 band still apply. The roll multiplier applies to the kernel's roll cost and to the AI's
  "can I roll" check.
- **Owner:** `spellsPierce 1` is read as "pierces one extra target" (open point 2): a projectile that connects (and is
  not perfected) flies on for one more body. Lines, beams and placed circles already hit every body in their shape, so
  it changes nothing for them. The count is snapshotted when the projectile is released.
- **Range x1.3 at 80.** Applied at release to projectile flight range, line and beam length and the placed distance of
  Downdraft, and to the AI's range check. Not to the dash distance (`dash 6 m` is a distance, not a range).
- **absorbDrainMult 1.2** applies to every air actor's ward. **Owner:** Eye of the Storm's "Absorb drain x1.5" stacks on
  it (1.2 x 1.5 = 1.8), the Furnace Heart reading.
- **Deflect.** On a perfect absorb, an air actor redirects a projectile of tier <= its unlocked tier along its own
  facing (which the kernel sets from its aim every step), same speed, damage and tier; the deflected bolt cannot be
  reflected or deflected again. **Owner:** for an air actor the deflect replaces the water Mirror reflection its attached
  water preset would give (the final's preset, Mirror tide, has Mirror A).
- **Owner: Shear.** The shape cell (`two projectiles 22 m/s at +-12 deg`) and "Reuses the Twin Tides shape" are taken
  literally: a straight fan of 24 deg total, one activation. The note's "Two blades that converge on the aim point"
  cannot hold for that shape: the blades diverge, and beyond 0.5 / tan 12 = 2.35 m both pass a body standing on the
  aim line. Shear only hits a target that is close or off the aim line.
- **Veering Bolt.** A circular arc from caster C to the aim point T (clamped to the range) that leaves at the entry
  angle E off the line C->T and arrives at E on the other side: R = L / (2 sin E), arc 2ER (E = 60 deg: 1.209 L). The
  side alternates with the activation id's parity (even: launches left), so no random draw is spent. Past T the bolt
  flies straight on for the unused range. **Owner:** the 14 m range limits the chord, not the flight, which is up to
  16.9 m of arc.
- **Squall.** Windup max(0.30, 0.30) = 18 ticks; the first orb leaves on the release tick with the cast's activation,
  the next three 15 ticks apart, each with its own activation id, from where the caster stands then, toward the aim
  point fixed at the press. **Owner:** while the volley runs the caster's hands are busy (no other cast starts), and an
  interrupt drops the orbs not yet thrown.
- **Air Form.** **Owner:** the Momentum need (40) is checked and the spend (40) paid at the press, with the mana; under
  Eye of the Storm the lock restores 100 on the next tick. The 3 s window opens at release. Each hit that reaches a
  formed air actor (after the roll and encase immunity, before the ward) draws once from the kernel RNG ("air <id>
  evade <activation>"); below 0.6 for physical or 0.3 for every other blockable family it is evaded: no ward, no
  damage, no metrics, no Momentum to the thrower, an `evade` event, and a projectile flies on through the body.
  Unblockable hits never draw.
- **Eye of the Storm.** Momentum is set and held at 100 for 5 s (gains discarded, no decay); the cast is not rooted
  (the notes do not say so).
- **Tempest Lance.** One windup, max(0.90, 1.20) = 1.2 s, the caster rooted throughout (the notes say so); the 16 m
  line resolves at release from the caster along the aim fixed at the press, unblockable (a ward does nothing; the
  blink's i-frames or being off the line do). A staff strike during the windup interrupts it. **Owner:** "cast 0.90"
  and "telegraph 1.20" are one windup (the Fire rule for every row but Sunfall), not 0.9 then 1.2.
- **Cyclone.** The Brand Wrath channel: eight 0.25 s ticks of 12 along the current facing, each its own activation;
  every tick may be perfected (the notes do not say "only the first").
- **Rulesets.** Unlike Fire's area and line rows, the air Downdraft, Tempest and Cyclone hits pass the VR ruleset to the
  hit resolution, so the planted-staff dome reduces them like any projectile. `Pressure.FireMageDamage` is the fire
  mage's knob and is not applied to the air mage.
- **The air AI** (`ChooseAirSpell`): the competence curve (level 1 random draw, level 2 sort with Veering Bolt and
  Squall first, then damage x count) picks among the useful rows; on top, without a spell draw: Tempest Lance from tier
  IV whenever it is legal and the target is not mid-roll (no mana banking, unlike Sunfall); Air Form when an aimed,
  blockable threat (projectile, telegraph or windup) is seen, the form is down and Momentum allows; Slipstream while
  Momentum is under Air Form's 40, dashed along the current move (sideways when standing), and a dash end is kept out
  of the dais hold and inside the narrow-view arc.
- **The final.** With the school mages on (`SetBout(..., true)`), `FArenaSession` spawns the Tiro final as Lio's air
  mage (`SetAirFinal`, default on; the semifinal stays Brennic, Fire). `rivals.lio` in `combat.vr.json` binds him
  (phases 0.66 / 0.33, surge 1, `opensWith: air_tempest`). The calibration sweep keeps the pre-T23 fire final.
