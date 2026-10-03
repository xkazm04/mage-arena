# CR-002 — Fire school and Heat

Status: open. Filed for the desktop/TV kernel channel. Not merged.

Date: 2026-10-03. Owner decision the same day: every channel's kernel implements all four schools, and VR starts with Fire because the competition slice ends on the Fire mage duel. The pinned desktop/TV kernel has Water only. Its duels are water proxies. This request is the Fire rules as the VR C++ kernel now runs them.

The full reading, including every spell row, is `docs/schools/FIRE.md`. This request is what a TypeScript port has to match. Pinned data was not edited. No combat number was retuned.

## What the port implements

Heat is Fire's resource, from `schools.json` `fire.combatIdentity` only. Range `[0, 100]`. A non-fire actor ignores Heat, the drain multiplier, and the stamina multiplier.

Gains:

- `perBoltHit` 2. This is the same payment as Ember Dart's `2 per hit`. One connecting bolt adds 2, not 4.
- `perFlickTarget` 6. Same reading for Flick Flame's `6 per target`.
- `perHitTakenUnabsorbed` 12, paid to a fire target when incoming damage is above 0 and the absorb reduction is 0. A chip, a perfect, or a fully negated blow pays nothing here. The caster still receives the spell's own heat when the blow connects, including a perfect absorb.
- `perSecondEnemyWithin3m` 3, per enemy, per second. The metres are the `3` in the key. Two living enemies inside 3 m add 6 per second. A downed enemy does not count.

Decay is 6 per second. It starts once 2.0 s have elapsed since the last gain, and that idle timer does not refresh while decaying. Proximity is a gain, so it resets the idle. The resource tick is at the start of the actor step, before the same-tick release and before projectile hits. Heat 0, or a mage who has never gained, does not decay.

Thresholds apply when `heat >= at`. A higher band replaces each effect it names. It does not multiply the lower band.

- Kindled at 50: spell damage ×1.15.
- Blazing at 80: spell damage ×1.3, absorb drain ×1.5, stamina regen ×0.7. At 100 the damage multiplier is still 1.3.

The damage multiplier is snapshotted when the outgoing number is written. A projectile and a placed area keep the heat from release. Each trail tick and each beam tick read live heat.

`perfectAbsorbExtra` contains "none". A perfect absorb grants Fire no extra heat beyond the shared mana refund. The camp line "start a bout at 30 Heat against a feud target" is `campBias`, not `combatIdentity`. Arena duels start at 0.

## Spells

Ten rows from `spells-fire.csv`, slots 0 through 9, in a Fire catalog beside the 24 water spells. Do not append them to the water list. Families: `UNBLOCKABLE` becomes `unblockable`, `absorbable` becomes `magic`, anything else is kept (`n/a` on the two self rows). A damage cell that starts with a digit parses as that number, so "14 per 0.25 s tick" is damage 14. Mana is spent on the press. Cooldown is `Tick + SimTicks(cooldown_s)`. Cast time 0 releases on the same tick. Windup is `max(cast_s, telegraph_s)`, except Sunfall.

| Id | Tier | Kernel |
| --- | --- | --- |
| fire_bolt | 0 | Projectile 18 m/s, damage 8, mana 4, cooldown 0.30 s, range 12, heat 2 on a connecting hit. |
| fire_flick | 1 | Cone 60° × 4 m, damage 20, heat 6 per target. A centre past 4 m is excluded. |
| fire_cinderstep | 1 | Instant dash 5 m along aim, then the arena clamp. No stamina, no invulnerability. Trail is the segment actually travelled. Damage 6 and heat 3 every 0.5 s, four ticks over 2 s. No authored width. Hits dedup per owner per sim tick. |
| fire_kindle | 1 | Self, cast 0.40 s, rooted, +25 heat, clamped at 100. |
| fire_lance | 2 | Line 14 m, damage 32, heat 4. `piercing` skips a Scutum front block. Family stays `magic`, so a ward still absorbs it. |
| fire_ring | 2 | Ring radius 3 m, damage 18, heat 5, knockback 2 m when damage gets through and the absorb was not perfect. |
| fire_pyre | 3 | One ground detonation, radius 3 m. Windup is `max(0.50, 0.80)` = 0.80 s. The caster is not rooted. |
| fire_furnace | 3 | Self, cast 0.50 s. Heat is forced to 100 for 6 s. Gains during the lock are discarded and decay does not run. The tick the lock ends, the idle has already passed, so decay starts. "Doubled" is ×2 and stacks with Blazing ×1.5, so absorb drain is ×3 while both apply. |
| fire_sunfall | 4 | Meteor radius 3.5 m, unblockable, damage 95, heat 10 per target, mana 70, range 12. |
| fire_wrath | 4 | Beam 10 m for 2 s. Cast 0.20 s, then damage 14 every 0.25 s, eight ticks. Heat 2 per tick. Perfect absorb only on the first tick. Later ticks suppress the fresh window, so a held ward chips them at 14 × 0.15. No authored width. The beam follows current facing. |

Bolt inside the staff range (1.6 m) on slot 0 is the existing staff strike. Fire does not add a second copy.

Sunfall's note says the shadow grows for 1.2 s after a rooted 0.8 s cast, so those times are sequential. Release writes an area telegraph, damage snapshotted, `bCommitted`, `bSurvivesOwner`. A staff strike during the pending cast cancels it and does not loose the meteor. A later interrupt, or the owner going down, keeps the shadow and it still lands. A ward does not reduce `unblockable`. Downed actors leave the step, so their trails and beams stop. The committed shadow is the exception.

## Fire mage

`TryCreateGames` grows a flag, default false. False keeps the pinned water-proxy duelist, including the label `Water proxy` and the water composition. True marks that duelist `bSchool`, label `Ember mage`, and still attaches the water composition so the old state shape is present. The reference player is never a fire school. Existing conformance vectors stay on the default.

Competence is the T07 model: reaction, absorb, perfect, aim error, cadence, and the level-1 random draw versus the level-2 sort. Aim lead uses Ember Dart's 18 m/s for a fire mage and the water bolt speed otherwise. Spell choice walks the ten rows. Useful means damage above 0, or a zone. A dash is not range-gated. Lines and beams are threats along the segment. An active beam is an extra threat. A dash is not a threat.

From tier IV until the first Sunfall releases, if Sunfall is legal it is the cast and that choice does not draw the spell-choice random. If mana is short of 70, only a spell that leaves `mana - cost >= 70` may be cast; if none can, offense is withheld and mana regens. Range or cooldown being the reason Sunfall is illegal falls through to the normal pick. An interrupt leaves the loosed flag clear. Defence rolls still happen.

The plan names six spells for the session narrative. That list is not a filter. All ten rows are eligible once their tier is open.

With the pinned costs the cheapest spell is 4 mana. A pool that can pay a spell and still hold 70 already holds 70, so the opening cast is Sunfall itself. The live effect of the bank rule is the withhold below 70.

## Scenarios the port can replay

These are VR references, separate from the 45 TypeScript conformance vectors. Same seeds as the water duels. The opponent is the fire mage. The player is the reference preset at competence 2. The hash is the C++ FNV state hash, not the TypeScript `JSON.stringify` hash. A port matches the outcome fields. It will not reproduce `hash` unless it copies that mixer.

Both bouts end before tier IV (45 s). Sunfall is not cast. That is the recorded result. Do not retune Heat or the spells to chase the 45–80 s band. `MageArena.Fire.SunfallBeat` is the proof that a living tier-4 fire mage casts Sunfall on the next legal decision.

`apps/vr/data/scenarios-vr/fire-duel-competence-1.json`

- Seed 40000, wave 2, kind `Ember mage 1`.
- Outcome `loss`, phase `lost`, 960 ticks, 16 s.
- Player hp 0, mana 20.875, waves cleared 0.
- `sunfall` 0. Hash `aadba881`.

`apps/vr/data/scenarios-vr/fire-duel-competence-1-5.json`

- Seed 40001, wave 3, kind `Ember mage 1.5`.
- Outcome `loss`, phase `lost`, 1103 ticks, 18.3833… s.
- Player hp 0, mana 18.75, waves cleared 0.
- `sunfall` 0. Hash `90bf9014`.

## Hand checks the port should reproduce

Each expected number is computed from the pinned cells, not from a recorded trace.

- Five connecting bolts at 6 m, no second body inside 3 m: heat 10. Hold through the tick before the 2 s idle, then 60 ticks of decay: heat 4.
- Kindle releases 24 ticks after the press (cast 0.40 s at 60 Hz) and pays 25. A second Kindle from 90 clamps at 100.
- Two enemies inside 3 m for 60 ticks: heat 6. One enemy: heat 3. Leaving the ring starts the idle. It does not drop the heat on that tick.
- Furnace locks at 100 through tick 390 when the press is the first step (release tick 31, lock 360 ticks). A gifted 50 during the lock is discarded. Tick 391 is 100 − 6/60.
- Bolt damage at heat 49, 50, 80, and 100: 8, 9.2, 10.4, 10.4.
- One stamina tick at Blazing, after the 0.8 s delay, from empty: 20/60 × 0.7. Cold: 20/60.
- Sixty ticks of a held ward at Blazing: mana drained 27 × 1.5. After Furnace, the same minute of ticks drains 27 × 3.
- Mana 3 refuses the bolt. A second press inside 18 ticks does not cast. Slot 0 inside 1.6 m is a staff strike: stamina 12, no bolt heat, one proximity tick of 3/60.
- A stale ward chips a bolt to 8 × 0.15. A fresh ward perfects it and the caster still gains 2.
- Sunfall: rooted for 48 ticks, shadow resolves 72 ticks later, damage 95 through a held ward and through a fresh ward, no perfect. A staff strike during the cast cancels it. After release, interrupt and owner death keep the shadow.
- Flick hits a body whose centre is at 4 m and misses 4.05 m and a body 90° off the aim. Ring at 2 m deals 18, knocks 2 m, and heat is 5 plus 19 ticks of proximity.
- Lance into a shieldman facing the caster deals 32.
- Pyre windup is 48 ticks, one hit of 48, no leftover telegraph.
- Cinder moves 5 m, spends no stamina, then four trail ticks for 24 damage and 12 heat. A second copy of the same trail does not add hits.
- Wrath: eight hits, 112 damage, 16 heat. First tick 28 is perfectable. Second tick 43, after a drop and the 8-tick re-raise, chips 2.1. Both ticks pay heat.

## Not in this request

No change to the 24 water spells, the water-proxy default, or the 45 existing vectors. No new tuning constant. Earth and Air are not in this request. The VR design gate `MageArenaDesign.Session.Wave1Seated` is a separate open item and is not part of the Fire port.
