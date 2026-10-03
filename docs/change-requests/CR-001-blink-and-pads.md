# CR-001 — Blink input and rune pads

Status: open. Filed for the desktop/TV kernel channel. Not merged.

Wave 1 in VR needs a blink between three rune pads. PROJECT-PLAN section 3 item 3 and section 6.2: blink replaces move, roll, and the staff strike. That is a change to shared combat semantics, so it belongs in the kernel that both channels run. This repo does not edit that kernel.

## What VR needs

- An input that means blink, distinct from roll. Direction is left pad, right pad, or back to centre, relative to the pad the player is on.
- Three pad positions owned by the kernel, in arena metres. The player's body is on one pad. There is no walk and no staff strike.
- Blink spends the same stamina and grants the same invulnerability window the roll uses today (pinned `combat.json`: 25 stamina, 0.25 s). The destination is the target pad, not a 4 m roll along a stick vector.
- Enemy attacks are scheduled against a pad. After a blink they re-acquire the pad the player is on now. A projectile already in flight keeps the point it was given.

## Adapter used until that lands

`FArenaSession` does not call into kernel source. On a blink gesture it feeds one `bRoll` edge, which is the existing dodge: the kernel deducts the stamina, sets `ImmuneUntil`, and emits `roll`. `Input.Move` stays zero on that edge and on every other tick. The roll's own 4 m travel is overwritten.

The seated player never walks. After every `StepGames`, and at session start, the adapter writes the player's `Pos` and `PreviousPos` to the active pad. Blink is the only position change: an accepted roll switches the active pad, and the same pin then holds the new pad. The kernel's walk is not fed. A previous draft of this adapter set `Input.Move` at the pinned walk speed (4.5 m/s) so the scripted bout could kite. That made the kernel player move while the seated camera stayed on the pad. It is removed.

Pad positions are VR layout, not kernel fields. The centre pad maps to `PlayerSpawn`. Left and right keep the layout's offsets in X/Y. Conformance tests never construct `FArenaSession`, so this write does not touch a hashed state.

Enemies already aim at the live `Pos`. Snapping that position makes them chase the new pad without a kernel change. That is a consequence of the adapter, not the pad field this request asks for. A real pad id on the actor is still the change we want, so a scheduled telegraph can name the pad instead of a point copied at windup.

The pinned Tiro wave 1 window (25–40 s in `arena-tiers.json`) was calibrated for the walking desktop/TV game. This adapter does not retune combat numbers to chase that window.

## Conformance impact

None on the pinned vectors. The kernel binary behavior is unchanged: the same inputs still produce the same state hashes. Blink exists only in the VR session, after `StepGames` returns. `node apps/vr/tools/conformance/generate.mjs` stays byte-identical.

## Not in this request

No new damage, cost, or invulnerability number. Those stay the pinned roll values until the kernel grows a blink of its own.
