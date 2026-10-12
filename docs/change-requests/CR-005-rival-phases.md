# CR-005 - Named rivals: HP-gated phases, the collar surge and the granted signature

Status: open. Filed for the desktop/TV kernel channel. Not merged. The balance outcome is not settled (DF-006).

Date: 2026-10-07. Owner decision the same day (`docs/DECISIONS.md`, "v2 vision accepted", DF-003 answered): named
mages fight in HP-gated phases, each break surges both tier clocks by one tier, and the last phase opens with the
rival's signature spell, so the duel always has its climax. This request is the rule as the VR C++ kernel runs it
(card T19, `Kernel/VrRivals.cpp`). Pinned data was not edited; the numbers live in the VR overlay `combat.vr.json`
`rivals`.

## Seam

The ruleset (`FVrRuleset`, the CR-003 seam) carries a list of rivals. With no ruleset, or no `rivals` block, nothing
below happens and the conformance hashes stay on today's path. Phase runtime (bound actor, phases broken, break ticks,
grant pending or used) lives in the ruleset runtime, is cleared on every wave spawn, and is not read by the state hash.

## The rule

- **Binding.** A rival entry names a tier, a wave number and kind, and a school (`brennic`: Tiro, wave 3, semifinal,
  fire). When that wave spawns a mage of that school, the mage is bound as the rival.
- **Breaks.** Once per tick, after all hits of the tick land and before the tier clock, while the bound rival is not
  down: for each unbroken phase in order, if `Hp <= atHpFraction x MaxHp`, the phase breaks. One hit can break several
  phases on one tick, in order. A killing blow below a threshold is the victory, not a break. Brennic: 0.66 and 0.33
  of 95, so 62.7 and 31.35.
- **Event.** Each break emits `phase` with actor = the rival, value = the phase entered (2 or 3), target = the
  threshold index (0 or 1).
- **Surge.** Each break raises the tier of the rival and of every living, non-dummy, non-creature actor of the other
  team by `surgeTiers` (1), capped at `TierCap` (4). Each step is a normal unlock: tier, unlock tick, `unlock` event.
  It ignores `minimumSecondsBetweenUnlocks`; that gap then counts from the surge.
- **Granted signature.** A phase with `opensWith` (Brennic's second: `fire_sunfall`) grants one cast of that spell, at
  most once per duel. It starts on the break tick if the rival is free, otherwise on the first tick with no cast,
  channel, ward, roll, recovery or encasement running. It skips the tier gate, the cooldown and the mana cost (no mana
  is paid) and then sets the normal cooldown. Cast time, telegraph and resolution are the spell's own: Sunfall stays
  unblockable and blinkable. The grant is spent when the cast starts, so an interrupt during the cast loses it.

## Measured (seated VR census, 20 seeds; full table in `docs/design-findings/DF-006-*.md`)

The signature reaches 100 % of duels that enter the last phase, but the duel gets shorter (median 24-42 s against the
45-80 s target). How to lengthen it is open (DF-006); a desktop/TV port should take the rule, not the VR numbers.

## Conformance impact

None on the pinned vectors: the 45 `Kernel.Conformance.*` tests pass, and the semifinal duel with the block removed
matches the pre-T19 digest.
