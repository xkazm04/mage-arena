# CR-006 - The collar ledger: Cracks, Tithe, the tier IV collar cap and tier V at the Breaking

Status: open. VR design record for the overlay, per DECISIONS 2026-10-21. Not merged. DF-009 (`docs/design-findings/DF-009-tithe-is-mostly-missed-bolts.md`)
is open with the owner: the Tithe weights are a proposal and may change, so a port should take the rule and read the
numbers from data. The Breaking and tier V have no C++ implementation yet (see "The rule", last two items).

Date: 2026-10-20 (written from the tree at `cb5101f`). Owner decision 2026-10-07 (`docs/DECISIONS.md`, "v2 vision
accepted", the collar economy): Cracks (clean play, saved across days, open the Breaking) and Tithe (spilled magic:
renown now, a stronger jailer later); the collar caps at tier IV until the Summa's Breaking; "new canon, change request
before it ships". This request is the rule as the VR C++ code runs it (card T22, `apps/vr/tasks/T22-collar-ledger.md`).
Pinned data was not edited; the weights live in the VR data file `apps/vr/data/vr/collar.json`.

## Seam

Two parts, both outside the pinned kernel data.

- **Events (kernel, ruleset path only).** Three events exist only when a ruleset is passed (the CR-003 seam, `Rules`
  non-null): `miss`, `dodge` and `reflectHit`. With no ruleset nothing below is emitted and the 45 conformance vectors
  stay on today's path. Sources: `Kernel/ArenaThreats.cpp` (`miss` ~69 and ~209, `dodge` ~80, ~171 and ~323,
  `reflectHit` ~335) and `Kernel/Water.cpp` (`miss` ~219, gated `Rules && Actor.Team == 0`). Runtime state for them
  (`CollarDamaged`, `ReflectedCasters`) is in `Kernel/VrRules.h` ~226.
- **Ledger (session side, reads only).** `FCollarLedger` (`Session/CollarLedger.{h,cpp}`) reads the kernel event stream
  and the player's Crest counter and never writes the kernel, so no combat number changes (T22 orchestrator check: with
  `miss`, `dodge` and `reflectHit` filtered out, Brennic's duel matches the pre-change digest). The session feeds it
  from `FArenaSession::ObserveCollar` (`Session/ArenaSession.cpp` ~1950).

A TV port that does not run the ledger changes nothing. Whether the new events touch the state hash is [unverified]: the
ledger fields live on the session, not in `FArenaState`, and the hash was not re-run for this document.

## The rule

Only events whose actor is the player count (`Session/CollarLedger.cpp`, `FCollarLedger::Observe`). Each event adds its
weight once. A weight of 0 still counts the event in the breakdown but adds no points (`FCollarLedger::Add`).

**Cracks (clean play)**

- `perfect` event on the player: `cracks.perfect` points.
- `reflect` event by the player: `cracks.reflect` points.
- `reflectHit` event (a projectile the player reflected damages the mage who cast it; emitted when
  `Projectile.bReflected` and the hit target is the recorded caster, `Kernel/ArenaThreats.cpp` ~331):
  `cracks.reflectHitsCaster` points, on top of the `reflect` point.
- `dodge` event on the player (an opposing unblockable resolves while the player is in blink i-frames, or off the
  player when the aim point was inside its shape; `Kernel/ArenaThreats.cpp` ~171 and ~323, via
  `IsUnblockableThreatTo`): `cracks.unblockableBlinked` points.

**Tithe (magic spilled into the sand)**

- A `cast` event by the player whose spell is tier III or IV and an area or multi-projectile shape
  (`FCollarLedger::IsAreaCastTier3Plus`: projectile with count above 1 or a burst radius; cone, ring, wave; a zone only
  if its damage is above 0): `tithe.areaCastTier3Plus` points.
- A `miss` event (a player projectile cast, counted once per activation, not per projectile; or a player area, lane or
  melee that could damage and damaged nobody; `Kernel/ArenaThreats.cpp` ~60-70 and ~207, `Kernel/Water.cpp` ~217):
  `tithe.missedCast` points. Tier 0 Bolts count (this is DF-009).
- Each rise of the player's kernel Crest counter (`FCollarLedger::ObserveCrests`; a rise is one Crest spent, a drop to 0
  on a chained wave is not): `tithe.crest` points. A split Bolt at Flow 5 does not spend a Crest (DECISIONS 2026-10-07,
  split hands).

**Per-bout tally.** `BeginBout` zeroes the bout counts, Cracks and Tithe (`Session/ArenaSession.cpp` ~415,
`Session/DayFlow.cpp` ~211). At a bout end `Session/DayFlow.cpp` ~357 adds the bout's Cracks and Tithe to the day flags
`arena.tiro.1.w<n>.cracks` and `.tithe` over every attempt of that bout (a lost attempt still counts), then
`CommitBout` adds them to the lifetime totals.

**Persistence across days.** `collar.ledger.json` sits next to `bout.txt`: `{ "version": 1, "lifetimeCracks": n,
"lifetimeTithe": n }` (`SaveLifetime` / `LoadLifetime`, `Session/CollarLedger.cpp`; path and save call in
`Session/DayFlow.cpp` ~103 and ~361; load in `Session/SessionFlow.cpp` ~355). A missing file is a new collar at 0, 0; a
corrupt file (bad JSON, wrong version, negative, non-integer, above 1e9) warns and starts at 0. Start over clears the
day's flags and tally but not the lifetime totals (`Session/SessionFlow.cpp` ~1403).

**Tier IV collar cap.** The kernel tier clock stops at `TierCap = 4` (`Kernel/SimConstants.h` ~31; `UpdateClock` returns
when `Actor.Tier + 1 > TierCap`, `Kernel/ArenaKernel.cpp` ~442; the rival surge also stops at it,
`Kernel/VrRivals.cpp` ~50). That cap is the pinned kernel constant; nothing in the ledger code adds a second cap.

**Tier V at the Breaking.** [unverified] No code or data in `apps/vr/Game/Source` or `apps/vr/data/vr` reads the
lifetime Cracks, opens a Breaking, or defines a tier V (a search found only the `TierCap` constant). Only the intent is
on record: `collar.json` `_about` says Cracks "will open the Breaking"; `docs/design/V2-VISION.md` says the Breaking
bout at the Summa "unlocks one uncapped tier V spell per school". The Cracks threshold, the tier V spell rows and how
`TierCap` is lifted are all undefined here; TV must decide them (below).

## Data

All from `apps/vr/data/vr/collar.json` (version 1, strict loader: known keys only, every number needs its `_<key>`
line). Every weight is marked "Proposal, owner sign-off pending".

| Field | Value | Source |
|---|---|---|
| `cracks.perfect` | 1 | `collar.json` |
| `cracks.reflect` | 1 | `collar.json` |
| `cracks.reflectHitsCaster` | 1 | `collar.json` |
| `cracks.unblockableBlinked` | 2 | `collar.json` |
| `cracks.spare`, `cracks.interrupt` | reserved, accepted and ignored (T25, T29) | `collar.json`, `Session/CollarLedger.cpp` |
| `tithe.areaCastTier3Plus` | 2 | `collar.json` |
| `tithe.missedCast` | 1 | `collar.json` |
| `tithe.crest` | 1 | `collar.json` |
| `wardstones.fullGlowTithe` | 30 (presentation only) | `collar.json` |
| `TierCap` | 4 | `Kernel/SimConstants.h` |
| Tier V, Breaking threshold | not defined [unverified] | none |

`combat.vr.json` has no collar block; its `rivals` `surgeTiers` (1) is capped at the same `TierCap` and belongs to CR-005.

Measured once in VR (DF-009, as recorded there): one scripted bout gave 18 Cracks and 3 Tithe; three others gave 0
Cracks and 12, 0 and 81 Tithe (the reference script misses Brennic 81 times in 36 s). The blink, area-cast and Crest
sources were never exercised in play.

## What TV must decide

1. Accept the three ruleset events (`miss`, `dodge`, `reflectHit`) and their emit points, or name different ones.
2. The weights, after DF-009. Its options: A (drop `missedCast` for tier 0 Bolts; the finding recommends it),
   B (raise `fullGlowTithe` to about 100), C (teach the reference script to blink unblockables and spend Crests). The
   owner decides; this request does not.
3. Whether lifetime Cracks and Tithe belong in TV's save, and the Start over rule (Cracks survive).
4. The Breaking: the lifetime Cracks needed, which bout, and what lifts the tier IV cap to tier V for it.
5. The tier V spell per school (rows, mana, damage, telegraph) and whether the clock gains a fifth step.
6. Conformance: no ledger vectors were found under `apps/vr/data/conformance` [unverified]; say whether TV wants VR
   vectors recorded before it re-pins. **Moot:** nobody re-pins from TV (`docs/DECISIONS.md` 2026-10-21, "Combat data: the current pin is VR's frozen baseline").

## Out of scope

- Renown and the "stronger jailer" that Tithe feeds later (DECISIONS names them; no code).
- The Wardstone glow, wrist-cuff flash and aftermath tablet columns (presentation, `Session/SessionPresentation.cpp`,
  `Session/DayFlow.cpp` ~508); no combat effect.
- The reserved `spare` (T25) and `interrupt` (T29) Cracks.
- The Breaking ending conditions (allies, trust, the Knowing) in `V2-VISION.md`.
- Any edit to `apps/vr/data/pinned/**`, and any claim about a build or test run: none was made for this document.
