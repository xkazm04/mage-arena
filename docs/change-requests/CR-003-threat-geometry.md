# CR-003 — Threat geometry: enemies stay off the dais

Status: open. Filed for the desktop/TV kernel channel. Not merged.

Date: 2026-10-03. Owner decision the same day (`docs/DECISIONS.md`, DF-001 option A): from a seat the player lost Wave 1 at 16.45 s because melee reached the pads. Enemies stay on the arena floor below the raised dais and attack from range. Conscripts throw their spear. Slingers sling. Melee does not touch the dais.

Pinned `combat.json`, `enemies.json`, `runtime.json`, and `arena-tiers.json` are not edited. No shared combat number is retuned. The VR numbers live in `apps/vr/data/vr/combat.vr.json`.

## Seam

`TryCreateGames` takes an optional `FVrRuleset*`, default null. Null is today's behavior: conscripts lunge, nobody is kept off a dais, and the conformance hashes stay on that path. The ruleset is stored on `FGames`, not on `FArenaState`. The state hash does not read it.

When the pointer is non-null and `bActive`:

- `StepGames` passes it into `EnemyInputs`. A conscript whose overlay mode is `throw` gets that mode's `projectileMps` and `rangeM` copied onto a local `FAttackSpec` before the attack is scheduled. The pinned spec is not written. `ScheduleAttack` is unchanged, so a copied speed makes the telegraph a projectile and the conscript's backoff stays the pinned windup, recovery, and "back off 1 s".
- After the stock desired velocity (including the 0.8× range stop and the backoff), a thrower is steered to the hold line. At the line, the inward component is stripped. Slingers are not steered.
- After `StepArena`, `KeepOut` pushes any enemy body that is inside the hold box back outside it. The hold box is the layout dais footprint expanded by `standoffM`. The dais is layout, not a kernel field.

The one shared-path edit is the projectile range read: a shot uses `Attack.RangeM` when that optional is set, and `rangedRangeM` when it is not. No pinned projectile attack sets `rangeM` (`sling_stone` and `bog_glob` set `projectileMps` only), so the no-ruleset path schedules the same ranges as before.

Presentation fields on `FProjectile` (`OriginPos`, `AimedAt`, `bHasAim`) are written at spawn and are not hashed. The sim does not read them.

## What VR passes

The VR session loads `combat.vr.json` at start and passes the ruleset. If that load fails, the session does not start. Desktop and the conformance runner keep passing null.

The overlay, field by field:

- Dais no-entry from `arena-layout.json`: centre `[-11.7, 0]`, half-extent `[2.6, 4.4]`, standoff `0.6` (the dais height, not a combat tune).
- Conscript `spear_lunge` becomes a throw. Damage `10` and windup `0.6` stay the pinned melee row. Speed `14` replaces a pinned null and matches `sling_stone`. Range `9` replaces pinned `2.0`, because `2.0` cannot cross the gap from the hold line to a pad. Arc, shaft length, and shaft radius are presentation.
- Slinger `sling_stone` is unchanged. No numeric override.
- `throwerClosesToLip` is true, so a thrower walks to the lip during the backoff window instead of standing still beyond 3 m. The ready delay itself is the pinned one.

A thrown spear is physical. A ward reduces it by `absorb.reduction.physical` (`0.30`, so 10 becomes 7). A fresh ward does not perfect it. Blink is still one roll edge and the pad pin from CR-001. The i-frames cover the spear the same way they cover a stone.

## Conformance impact

None on the pinned vectors. `node apps/vr/tools/conformance/generate.mjs` stays byte-identical. Tests that call `TryCreateGames` without a ruleset, including the 45 vectors, do not load the overlay.

## Not in this request

No new enemy, no change to wave composition, no change to the 25–40 s Tiro window, and no blink field. Wave composition remains the fine-tuning knob if the seated bout misses that window. The dais does not become a kernel field until a channel wants it shared.
