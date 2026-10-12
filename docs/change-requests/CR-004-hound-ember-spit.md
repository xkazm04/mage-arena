# CR-004 - Cinder hounds spit embers; a dying hound throws one last ember

Status: open. Filed for the desktop/TV kernel channel. Not merged. Pacing is not settled (DF-007).

Date: 2026-10-07. Owner decision the same day (`docs/DECISIONS.md`, "v2 vision accepted", DF-004 option A): hound
death bursts (radius 1.5 m) cannot reach a seated player held off the dais (CR-003), so the creature bout taught no
perfects. Hounds now spit slow magic embers from range. This request is the rule as the VR C++ kernel runs it (card
T18). Pinned `enemies.json` is not edited; the overlay is `combat.vr.json` `attacks` (`cinder_hound`, mode `spit`).

## Seam

The CR-003 attack-mode seam, extended. With no ruleset nothing changes: hounds bite, and the 45 conformance vectors
pass. A thrower copies only speed and range onto a local `FAttackSpec`; a spitter also copies family, tier, damage and
windup. Hounds with the spit hold at the CR-003 hold line like throwers. The pinned `bite` recovery (0.4 s) and backoff
stay, so an engaged hound spits about once a second.

## The numbers, each tied to a pinned one

| Field | Value | Tied to |
|---|---|---|
| family, tier, damage | magic, 0, 5 | the pinned `cinder_hound.onDeath` `ember_burst` (the loader requires equality) |
| windupS | 0.6 | must be above the positional telegraph floor (0.40 s) |
| projectileMps | 7 | must be above 0 and below the pinned `sling_stone` (14): it must read as slow |
| rangeM | 9 | must equal the conscript throw range (CR-003) |
| deathEmber | true | see below |

## Death ember

With an active ruleset, when a spitting hound dies its pinned 1.5 m `ember_burst` is queued exactly as before, and in
addition one ember projectile with the spit numbers is queued from the death position at the nearest living opponent,
released after the pinned burst delay (0.6 s = 36 ticks). If that target is beyond 9 m, the reach stretches to the aim
point (`max(9, distance)`). The overlay's enemy damage multiplier applies to it as to a spit.

An ember is magic: a ward absorbs it, and a fresh ward inside the perfect window scores a perfect.

## Measured (seated VR, reference script; `docs/design-findings/DF-007-*.md`)

Creature bout perfects went from 0 to 11 on the live overlay; the bout runs 52 s against the pinned 30-45 s window.
A desktop/TV port should take the rule; the cadence is open.

## Conformance impact

None: the 45 `Kernel.Conformance.*` tests pass, and `MageArena.Creatures.NoRulesBite` pins the null path.
