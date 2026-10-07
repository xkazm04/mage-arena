# CR-006 — Cinder hound ember spit

Status: draft. Filed for the TV channel (`kiro/mage-arena-tv`, the owner of `enemies.json`). Not merged. Not built.

Date: 2026-10-07. Source: the owner's v2 vision log, wave 3: "DF-004: ember spit", which is option A of
`docs/design-findings/DF-004-creatures-cannot-reach-the-seat.md`. Draft decision entry in
`docs/design/V2-DECISIONS-DRAFT.md`.

## Why

A dying cinder hound looses `ember_burst`: magic, tier 0, damage 5, radius 1.5 m, delay 0.6 s (pinned `enemies.json`
`creatures[cinder_hound].onDeath`). Under the seated threat geometry (CR-003) no enemy body enters the dais footprint
plus 0.6 m standoff, so the nearest a hound can die is about 3.5 m from the player. The burst never reaches the seat.
T15 measured Bout 2 at 33.05 s with **0 perfect absorbs** and 14 damage taken. The hound's `teaches` field ("perfect
absorb on death bursts") is false from a seat.

## What the port implements

A second `onDeath` mode for the cinder hound: on death the hound **spits** its ember at the player as a slow magic
projectile, instead of bursting in place.

The attack row, as proposed for `enemies.json`:

```json
"onDeathSpit": {
  "id": "ember_spit",
  "family": "magic",
  "tier": 0,
  "damage": 5,
  "delayS": 0.6,
  "projectileMps": 6.0,
  "rangeM": 12.0,
  "radiusM": 0.3,
  "aim": "player_pad",
  "telegraph": "ring"
}
```

Rules:

- Damage 5, magic family, tier 0 and the 0.6 s delay are the pinned `ember_burst` numbers. Only the delivery changes.
- After the delay the projectile spawns at the hound's death position at chest height and flies at the player's
  current pad (the pad at release, not a homing shot). A blink moves the player out of the line.
- Speed 6 m/s. From the nearest death point (about 3.5 m) the flight is about 0.58 s. From a typical death point at
  the hold line (5-7 m) it is 0.8-1.2 s. Both exceed the 0.40 s positional reaction band (plan section 6.3) and leave
  room for the 0.15 s perfect window (`combat.json` `absorb.perfectWindowS`).
- A ward absorbs it as magic. A fresh ward perfects it: mana back, tier clock +2 s, Flow +1, and one Crack (CR-004).
- The threat shows the element colour with a shrinking ring, the same language as every absorbable threat.
- Three hounds die at different times, so the bout offers up to three spaced perfect chances from every pad.
- **The bite stays melee-only.** A hound cannot reach the dais, so it threatens by pressure and by its spit, not by
  biting. A leap at the dais lip (DF-004 option A's variant) is not in this request.

Selection: `onDeathSpit` is used when the bout's ruleset says the player is seated (the VR overlay, CR-003's
`FVrRuleset`). Without a ruleset the pinned `ember_burst` runs, so every existing vector and TV bout is unchanged. TV may
also adopt the spit for any channel where the player is out of burst reach.

## Where VR puts it until TV answers

As with CR-003, VR implements the row in its overlay, `apps/vr/data/vr/combat.vr.json`, under a `creatures.cinder_hound`
key. Pinned `enemies.json` is not edited.

## What the census checks (Bout 2, `MageArenaDesign.Session.CreaturesSeated`)

| Target | Value | Source |
| --- | --- | --- |
| Bout length | 30-45 s | `arena-tiers.json` Tiro wave 2 |
| Spits that reach the player's pad | ≥ 2 of 3 per bout, on ≥ 80 % of seeds | DF-004 intent |
| Perfect absorbs on spits, reference script | ≥ 1 per bout | plan section 6.4 ("perfect practice") |
| Damage taken | not above the Wave 1 median | keeps Bout 2 a breather |

## Scenarios the port can replay

To be added with the build card (T21 in `docs/design/V2-ROADMAP.md`). Hand checks:

- A hound killed 4.0 m from the centre pad releases the spit 36 ticks after death. It reaches the pad 40 ticks later
  (4.0 m at 6 m/s).
- A ward raised 6 ticks before impact perfects it. A stale ward (held 2 s) chips it to 5 × 0.15, the same chip CR-002 measures on a bolt.
- A blink to another pad after release: the spit misses and deals 0.
- With no ruleset the hound bursts in place at radius 1.5 m, and the hash matches today's vector.

## Not in this request

No change to the hound's HP, speed, bite, pack size or behaviour. No change to the mire maw. No change to the 45
conformance vectors.

## Open questions

- Should a missed spit leave a short burning patch on the floor (presentation only), so the hound's death still reads as
  a burst from the dais?
