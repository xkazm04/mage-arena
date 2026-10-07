# CR-005 — Boss phases, the tier-clock surge, and tells with interrupts

Status: draft. Filed for the TV channel (`kiro/mage-arena-tv`, the owner of the kernel rules and the combat data). Not
merged. Not built.

Date: 2026-10-07. Source: the owner's v2 vision log, wave 3 ("Climax: HP-gated boss phases") and wave 4 ("read the tell
and interrupt"), drafted into `docs/design/V2-DECISIONS-DRAFT.md`. The measured problem it answers is DF-003: from a
seat, the Fire mage cast Sunfall in 0 of 40 duels on the live overlay, because duels end before the 45 s tier IV unlock
(`docs/design-findings/DF-003-calibration-proposal.md`). The census found no overlay that lands both the Wave 1 gate and
the Sunfall rate. This request moves the signature spell off the clock and onto the fight's shape.

Pinned data was not edited. No combat number was retuned. Numbers marked "proposal" go through the census.

## What the port implements

### HP-gated phases

A **boss** is a mage actor with a `phases` block. Non-boss mages and every enemy are unchanged.

| Phase | Entered when | Proposal |
| --- | --- | --- |
| 1 | Bout start | The boss's normal kit at its tier |
| 2 | HP falls to 66 % of max or below | Kit widens (per-boss, data) |
| 3 | HP falls to 33 % of max or below | Opens with the **signature spell**, then the full kit |

Rules:

- **No skipped phase.** Damage that would carry HP past a threshold stops at the threshold. The excess is discarded.
  A single 95-damage hit cannot jump a boss from phase 1 to phase 3.
- **The break.** On entering phase 2 or 3 the boss enters a **phase-break window** of 2.0 s (proposal): it takes no
  damage, casts nothing, and plays its taunt line (at most 10 words, voiced). Projectiles, areas and telegraphs already
  in flight keep resolving. The player's actions are not paused.
- **Phase state is hashed.** `phase` and the break timer join the state hash, so replays verify them. Existing
  vectors do not contain a boss and stay byte-identical.

### The tier-clock surge (shared clock semantics)

The collar clock is the arena's, not one mage's. A phase break is a surge of spilled magic, so it moves **every mage
in the bout**, the player included.

- On a phase break, each living mage in the bout immediately unlocks its next tier, if it is below the collar cap
  (tier IV, CR-004).
- The 6 s minimum gap is **waived for that unlock only**. The gap timer restarts from the surge tick.
- The clock's own schedule continues. A surge does not reset it or shift later unlock times.
- At most one surge per phase break, so a bout has at most two surges.

Consequence: the player always reaches the last phase at tier III or higher, so Leviathan Orb (tier IV) is close at
hand when the boss's signature arrives. The surge is symmetric, so it never hands the boss an edge the player lacks.

### The signature opener

- On entering the last phase, after the break window, the boss's **first decision is its signature spell**
  (Brennic: `fire_sunfall`).
- For that one cast the tier requirement and the mana cost are waived. The cast is a set piece, the same as the plan's
  "scripted beat" option in DF-003. Cooldown applies from it as normal.
- The opener's tell **cannot be interrupted** (the boss is still in its break pose). Later signature casts in the same
  phase use the normal tier, mana and tell rules.
- This replaces CR-002's tier IV bank rule (hold 70 mana for Sunfall) for a boss. A non-boss Fire mage keeps CR-002.

### Tells and interrupts

- **Every AI cast of tier III or higher, and every signature cast, has a tell**: a held pose plus an audio cue, lasting
  at least 0.80 s (the unblockable reaction band, plan section 6.3). The tell is `max(cast_s, telegraph_s, 0.80)`.
- **An interrupt** is a player hit that connects with the caster during its tell and carries `interrupts: true`.
  Proposal: Lash rows at every tier, a reflected projectile (Mirror II Reflection), and any Crest cast. Bolt does not
  interrupt, so bolt spam cannot lock a boss.
- An interrupted cast is cancelled. Its mana stays spent and its cooldown starts. CR-002's "a staff strike during the
  pending Sunfall cast cancels it" becomes one case of this rule.
- Each interrupt is one CR-004 Cracks event (3 Cracks).
- The same rule applies to the player: an AI hit with `interrupts: true` during a player's line-cast windup cancels it.

### Techniques that ride on the same events (VR presentation, no kernel change beyond the above)

| Technique | Rule | Kernel need |
| --- | --- | --- |
| Reflection rally | A reflected projectile can be reflected back by the AI and again by the player, up to 3 player returns per projectile. Each return keeps its damage and adds 10 % (proposal). | Mirror II Reflection on the VR player preset, and a reflect chance on the boss AI |
| Named line combos | Flow rule, unchanged. A named sequence (for example Tide Orb, Lash, Mirror inside 2 s each) plays a crowd call. | None; presentation reads cast events |
| Pad angles | A projectile whose flight direction is more than 45° off the target mage's facing ignores that mage's ward (flank). Applies to AI wards only. | New ward rule, VR overlay first |

## Data the port reads

```json
"phases": {
  "thresholds": [0.66, 0.33],
  "breakWindowS": 2.0,
  "clockSurge": true,
  "signature": "fire_sunfall",
  "kitByPhase": [
    ["fire_bolt", "fire_flick", "fire_kindle"],
    ["fire_bolt", "fire_flick", "fire_lance", "fire_ring", "fire_wall"],
    ["fire_bolt", "fire_lance", "fire_pyre", "fire_sunfall", "fire_wrath", "fire_wall"]
  ],
  "taunts": ["brennic.phase2", "brennic.phase3"]
}
```

The block sits on the boss entry in `enemies.json` `mages`. Taunt ids point at VR voice assets. TV may keep them as
text. `fire_wall` is the SCHOOL-DEFENCES C row, which lives in the VR overlay today.

## Census targets (owner, vision log wave 3)

| Target | Value |
| --- | --- |
| Signature cast in duels that reach the last phase | 100 % |
| Median duel length | 45-80 s |
| Player win rate, competence 1 | ≥ 70 % |
| Player win rate, competence 1.5 | 40-60 % |

These replace the DF-003 Sunfall target ("Sunfall in at least 70 % of duels") for boss bouts. The census reports, per
seed, the phase reached, the surge ticks, the opener tick, interrupts landed, and the player's tier at the opener.

## Scenarios the port can replay

To be added with the build card (T19 in `docs/design/V2-ROADMAP.md`). Hand checks the build must reproduce:

- A boss at 70 HP of 100 hit for 40 ends at 66, enters phase 2, and is untouchable for 120 ticks.
- A boss at 67 hit for 95 ends at 66, not 33.
- A player at tier II with 3 s since the last unlock reaches tier III on the surge tick.
- A player already at tier IV gains nothing from a surge.
- On phase 3 entry, Sunfall is the first cast after the window with the boss at 0 mana.
- A Lash hit during a Pyre tell cancels it. A Bolt hit does not.

## Not in this request

No change to tier clock times, Flow, absorb, or any water or fire spell row. No boss data for opponents other than
Brennic. No change to the 45 conformance vectors. The Brennic kit split per phase is a proposal for the census, not a
tuning.

## Open questions

- Should the surge also apply to non-mage enemies in a mixed bout? This request says no: only mages have tiers.
- Should the opener's mana waiver show in the fiction (the collar surge feeds it), or stay silent?
- The pad-angle flank rule weakens AI wards from the side pads. Is that a VR-only rule, or shared canon?
