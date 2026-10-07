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
