# Fire school — Heat and the ten spells

Date: 2026-10-03. VR C++ kernel. The desktop/TV port is `docs/change-requests/CR-002-fire-school.md`.

Pinned files, read only:

- `schools.json` `fire.combatIdentity` — Heat range, gains, decay, Kindled, Blazing.
- `spells-fire.csv` — ten rows. The notes column is unquoted and contains commas.
- `combat.json`, `stats.csv`, `runtime.json` — staff strike, absorb, ranks, step rate. Not retyped.

Water stays the default duel. `TryCreateGames(..., bFireMages)` opts a Tiro mage into Fire. The water composition is still attached and unused for casts. The 24-spell water catalog is unchanged. Fire rows live in `FireSpells` / `FireCatalog`.

Every number below is the pinned cell. Where the cell can be read two ways, the kernel takes the literal reading and the line is marked **Owner:**.

## Heat

Resource name `heat`, range `[0, 100]`. A non-fire actor ignores it. `bSchool` is the switch.

Gains, from `combatIdentity.gain`:

- `perBoltHit` 2. This is the same payment as Ember Dart's `2 per hit`. One connecting bolt adds 2, not 4. **Owner:** the CSV cell is what a spell pays; the schools.json key is that same rule, checked equal by the test, not a second grant.
- `perFlickTarget` 6. Same reading for Flick Flame's `6 per target`.
- `perHitTakenUnabsorbed` 12. Paid to a fire target when the hit's incoming damage is above 0 and the absorb reduction is 0. A chip, a perfect, or a fully negated blow pays nothing. A fire caster still receives the spell's own heat when the blow connects, including a perfect absorb. **Owner:** "unabsorbed" is read as reduction 0, so a partial ward is absorbed.
- `perSecondEnemyWithin3m` 3, per enemy, per second. The metres are parsed from the key. Two enemies inside the ring add 6 per second. A downed enemy does not count. **Owner:** the key does not say "per enemy". One enemy matches a flat 3 per second. The per-enemy reading is the one that stays true when a second body steps inside.

Decay: 6 per second, starting once `afterNoGainSeconds` (2.0) have elapsed since the last gain, and not refreshing that timer. Proximity is a gain. The resource tick runs at the start of the actor step, before the same-tick release and before projectile hits. While Heat is 0, or no gain has ever been recorded, decay does nothing.

Thresholds, `heat >= at`. A higher band replaces each effect it names. It does not multiply the lower band.

- Kindled at 50: spell damage ×1.15.
- Blazing at 80: spell damage ×1.3, absorb drain ×1.5, stamina regen ×0.7. At 100 the damage multiplier is 1.3, not 1.15×1.3.

The damage multiplier is snapshotted when the outgoing number is written. A projectile or a placed area keeps the heat from its release. Each trail tick and each beam tick reads live heat.

`perfectAbsorbExtra` must contain "none" or the load fails. A perfect absorb grants Fire no extra heat beyond the shared mana refund. **Owner:** the camp line "start a bout at 30 Heat against a feud target" is `campBias`, not `combatIdentity`. Arena duels start at 0.

## Spells

Slot is the CSV row, 0 through 9. A row whose tier is above the actor's tier cannot be cast. Mana is spent at the press. The cooldown is `Tick + SimTicks(cooldown_s)` and the next press is legal once `Tick` reaches it. Cast-time 0 releases on the same tick.

Windup is `max(cast_s, telegraph_s)`, matching Water, except Sunfall. Families: `UNBLOCKABLE` → `unblockable`, `absorbable` → `magic`, anything else kept as written (`n/a` on the two self rows). A damage cell that starts with a digit parses with `Atod`, so "14 per 0.25 s tick" is damage 14.

| Id | Tier | Kind | What the kernel does |
| --- | --- | --- | --- |
| fire_bolt | 0 | projectile 18 m/s | Damage 8, mana 4, cooldown 0.30 s, range 12, heat 2 on a connecting hit. |
| fire_flick | 1 | cone 60° × 4 m | Damage 20, heat 6 per target. A centre past 4 m is excluded. The arc is strict. |
| fire_cinderstep | 1 | dash 5 m, trail 2 s | Instant step along aim, then the arena clamp. No stamina, no invulnerability. The trail is the segment actually travelled. Tick 6 damage and 3 heat every 0.5 s, four ticks. No authored width: the test is the segment against the body radius. Targets dedup per owner per sim tick. |
| fire_kindle | 1 | self | Cast 0.40 s, rooted, +25 heat. |
| fire_lance | 2 | line 14 m | Damage 32, heat 4 per target. `piercing` sets `bPierceShields`, so a Scutum front block does not apply. The family stays `magic`, so a ward still absorbs it. |
| fire_ring | 2 | ring r 3 m | Damage 18, heat 5 per target, knockback 2 m when damage gets through and the absorb was not perfect. The ring radius is the proximity radius, so a body the ring can hit is inside the heat ring for the windup. |
| fire_pyre | 3 | ground circle r 3 m | One detonation. Windup is `max(0.50, 0.80)` = 0.80 s. The caster is not rooted. Hit when the distance to the placed point is within radius plus body radius. |
| fire_furnace | 3 | self | Cast 0.50 s. Heat is forced to 100 for 6 s. Gains during the lock are discarded and decay does not run. When the lock ends, the idle is already past 2 s, so decay starts that tick. "Doubled" is ×2 and stacks with Blazing's ×1.5, so absorb drain is ×3 while both apply. **Owner:** the note says the drain "is doubled", not "replaces the threshold". |
| fire_sunfall | 4 | meteor r 3.5 m | Unblockable, damage 95, heat 10 per target, mana 70. See below. |
| fire_wrath | 4 | beam 10 m for 2 s | Cast 0.20 s, then a tick of 14 every 0.25 s, eight ticks. Heat 2 per tick. Each tick has its own activation. Perfect absorb only on the first; later ticks suppress the fresh window and a held ward chips them (14 × 0.15). The beam follows the current facing. No authored width. |

Bolt inside `staffStrike.rangeM` (1.6 m) on slot 0 is the existing staff strike (stamina 12, interrupts). Fire does not add a second copy of that rule.

**Owner:** Sunfall's note says the shadow grows for 1.2 s after a rooted 0.8 s cast, so those two times are sequential (0.8 then 1.2). Every other row, including Pyre's 0.50 cast and 0.80 telegraph, uses the longer of the two columns as one windup. Pyre's "telegraph ring fills" is that single windup, not a second phase.

Sunfall, after the cast releases: an area telegraph, radius stored as width, damage snapshotted, `bCommitted`, `bSurvivesOwner`. A staff strike during the pending cast cancels it and does not loose the meteor. A later interrupt, or the owner going down, keeps the shadow and it still lands. A ward does not reduce `unblockable`, fresh window or not.

Downed actors leave the step, so their trails and beams stop. The committed Sunfall shadow is the exception, because it is a telegraph with `bSurvivesOwner`.

## Fire mage

Competence is the T07 model: reaction, absorb, perfect, aim error, cadence, and the level-1 random draw versus the level-2 sort. Aim lead uses the same expression as Water, with Ember Dart's 18 m/s in place of the water bolt speed. Water mages still use the water bolt speed, so their draws do not move.

Spell choice, when `bSchool` is set, walks the ten rows instead of the water lines. Useful means damage above 0, or a zone. Kindle and Furnace are not useful while an attack is legal. A dash is not range-gated. Lines and beams are threats along the segment. A dash is not a threat. An active beam is a threat with a stable id.

**Owner:** from tier IV until the first Sunfall actually releases, if Sunfall is legal (tier, cooldown, mana, range) it is the cast, and that choice does not draw the spell-choice random. If mana is short of 70, only a spell that leaves `mana - cost >= 70` may be cast; if none can, the mage withholds offense and regens. The cheapest pinned spell costs 4, so a pool that can pay a spell and still hold 70 already holds 70, and the opening cast is Sunfall itself. The live effect below 70 is the withhold. Defence rolls still happen. An interrupt leaves the loosed flag clear, so the next decision tries again. This is an addition on top of level 1's random curve so the session's "Sunfall at about 45 s" has a defined mechanism. The plan's named subset (Ember Dart, Flick Flame, Ring of Cinders, Brand Lance, Pyre Circle, Sunfall) is not a filter: all ten rows are eligible once their tier is open.

The free duel is not retuned if it ends outside the 45–80 s band, or if the reference mage dies before Sunfall. The recorded outcome is the outcome. The beat itself is the unit test `MageArena.Fire.SunfallBeat`.

## Tests

`MageArena.Fire.*` computes the expected numbers from the literals above. `apps/vr/data/scenarios-vr/fire-duel-competence-1.json` and `fire-duel-competence-1-5.json` are the VR reference for a full duel (seeds 40000 and 40001, the same seeds as the water vectors, and not those vectors). The hash is the C++ FNV state hash.
