---
title: T71 crowd density - stands.tiers 3 -> 5 (option C), 804 cards on the one crowd draw
channel: vr
date: 2026-10-27
status: RAN - desktop proxy, not Quest truth. The blind critic round is the Director's (not run in this lane); the before/after still pairs are in staging.
sources: [docs/DECISIONS.md (2026-10-27 ruling 1), apps/vr/tasks/T71-crowd-density.md, docs/research/CROWD-T57-2026-10-25.md, docs/research/CROWD-TINT-T65-2026-10-25.md, docs/research/GATE-REHEARSAL-VR-2026-10-27.md]
---

# T71 crowd density, option C, 2026-10-27

**Label: desktop proxy (RTX 4090, D3D12, forward shading, `-nohmd`), not Quest truth.** Engine UE 5.8.3 (`Build.version`
5.8.3 read before the first build). Owner ruling 2026-10-27 (DECISIONS, ruling 1) supersedes two lines of the card's
Constraints, for `apps/vr/data/vr/arena-layout.json` and the `Overlay.Golden` files only. No pinned data, nothing under
`apps/vr/art/`, no image generation, no paid service. The file is dated 10-26 because the card names it so.

## Outcome

- `stands.tiers` 3 -> 5: **cards 436 -> 804** (banners stay 14), **crowd top 7.10 m -> 8.80 m**. The two new rows stand on
  real tread boxes the layout already draws (`ArenaGreyboxActor.cpp:191` loops `StandTiers`): no support geometry was added.
- **The first 436 cards did not move.** `Place` draws tiers 0 to 2 first, from the T57/T65 streams in the T57/T65 order, then
  tiers 3 and up from `UpperSeed` / `UpperTintSeed` (`ArenaStands.h`). `Crowd.StableBase` proves position, variant, rect and
  tint equal at index 0..435 between a 3-tier and a 5-tier layout.
- Still one `UInstancedStaticMeshComponent`, `M_Crowd_Atlas`, 8 custom floats; `Crowd.OneBatch` and `Crowd.TintNotThreat` green.
- **Draw calls did not rise** (stress max 110 -> 110, creature-import 119 -> 119). BasePass triangles rose by about 1.7 k.
- **Combat unchanged:** the re-recorded `Overlay.Golden` files differ from the old ones in two hash lines each; no bout's
  outcome, hp or mana line moved.

## 1. Commits (in order)

| Commit | What |
|---|---|
| `7f6bf43` | red tests: `Crowd.Placement` (five tiers, about 800 cards, tiers 3 and 4 hold cards), `Crowd.OneBatch` (804), new `Crowd.StableBase` |
| `636c098` | `Place` upper tiers from their own streams |
| `411e43e` | layout `tiers` 3 -> 5 (alone) |
| `3a53633` | `Overlay.Golden` re-record (alone) |

## 2. Red, then green

Red (7f6bf43 on the T71 base build, `MageArena.Greybox.Crowd`, `TEST COMPLETE. EXIT CODE: -1`):

```
Fail OneBatch   Expected 'T71: 804 cards' to be 804, but it was 436.
Fail Placement  Expected 'five tiers in the layout' to be 5, but it was 3.
                Expected 'about 800 cards (436)' to be true.
                Expected 'tier 3 holds cards' to be true.   Expected 'tier 4 holds cards' to be true.
Fail StableBase Expected 'the first 436 cards keep position, variant, rect and tint' to be 0, but it was 425.
                card 444 after the base sits on tier 0
Success Card, SwitchOff, TintNotThreat, Variants
```

Green (after the code and the layout edit, same filter): `Card`, `OneBatch`, `Placement`, `StableBase`, `SwitchOff`,
`TintNotThreat`, `Variants` all `Result={Success}`; `MAGEVR_ARENA_CROWD cards=804 banners=14 components=1`;
`cards=804`, `per variant 240 180 219 165; per tier 128 148 160 176 192`; `TEST COMPLETE. EXIT CODE: 0`.

`Crowd.Placement` checks every card (tiers 0-4) on its tread top (+-1 cm), inside its box in plan, out of the aisles,
facing the centre within the yaw jitter.

Note on history: between `411e43e` and `3a53633` `MageArena.Overlay.Golden` is red by construction (the layout hash
changed); `3a53633` closes it. The branch tip is green.

## 3. Overlay.Golden

Compare mode after the layout edit, before re-recording: red, `overlay changed since recording: arena-layout.json recorded
c16fb0fe9cba... now 2237c88c8e48...` for all five bouts. Re-record with `-MageArenaRecordOverlayGolden`: `Result={Success}`,
exit 0. Re-run in compare mode: `Result={Success}`, `TEST COMPLETE. EXIT CODE: 0`. `check-overlay.mjs`: `overlay OK: 14 files`.

| Files | `overlaySha256` | `arena-layout.json` hash | Outcome lines |
|---|---|---|---|
| wave1-split, creatures-spit, brennic-c1, lio-c1, firewall-duel (all five) | `a1fbe712c816...` -> `cbb6b8ed569b...` | `c16fb0fe9cba...` -> `2237c88c8e48...` | identical |

Reason: the layout edit. The diff is 2 lines removed and 2 added per file (10 and 10 in all).

## 4. Suite and tools

- `node apps/vr/tools/crowd/verify.mjs`: `OK: 4 variants, atlas 512x1024, 470 cards, 940 tris` (the kit and atlas check,
  unchanged; it does not count the in-game placement).
- `build.ps1`: `Result: Succeeded`.
- `Automation RunTests MageArena.` (`-nullrhi`): 344 `Result={Success}` lines, no failure line,
  `TEST COMPLETE. EXIT CODE: 0` (the 10-27 rehearsal read 342; `Crowd.StableBase` accounts for one, I did not trace the
  other).
- `format-check.mjs`: `format-check OK: 3 changed C++ file(s) since 43e8f2e`.

## 5. D-G4 (`capture-budget.ps1 -Runs 3`, three 60 s runs each; limits: draw calls 150, BasePass 350k)

Base = master `43e8f2e` code, same worktree and day (`t71-base`, `t71-base-creatures`). After = `3a53633`
(`t71-after`, `t71-after-creatures`). Every run `ok=True`, parity ok, `budget: ok (3 runs)`, exit 0.

| Scene | Run | Draw calls median / max, base | Draw calls median / max, after | BasePass tris median / max, base | BasePass tris median / max, after |
|---|---|---|---|---|---|
| D-G4 stress (default) | 1 | 105 / 110 | 105 / 110 | 163,520 / 166,368 | 165,622 / 167,230 |
| D-G4 stress | 2 | 106 / 110 | 106 / 110 | 162,848 / 165,504 | 164,574 / 167,230 |
| D-G4 stress | 3 | 105 / 110 | 106 / 110 | 162,848 / 165,504 | 164,574 / 167,230 |
| creature-import (`-MageArenaCreatureImportScene`) | 1 | 115 / 119 | 115 / 119 | 217,434 / 219,042 | 218,424 / 220,768 |
| creature-import | 2 | 115 / 119 | 116 / 119 | 216,386 / 219,042 | 219,160 / 220,720 |
| creature-import | 3 | 115 / 119 | 116 / 119 | 217,034 / 219,042 | 218,424 / 220,768 |

All runs are under 150 draw calls and 350k BasePass. The creature scene's draw-call max did **not** rise (119 -> 119), so
nothing drew above the base. Live particles and GPU emitters are unchanged (1,468 max, 1).

**A discrepancy to know about.** The 10-27 rehearsal table read stress max 118 and creature-import max 127 on the same
master code; this lane's same-day base reads 110 and 119. Base and after here agree with each other, so the T71 delta
(zero) is sound; the 8-call gap to the rehearsal was not diagnosed (a different worktree and a fresh cold build here). Against
the 127 guide, this scene sits at 119.

## 6. Stills (desktop proxy, in staging, not in the repo)

`C:/Users/kazda/mage-art-staging/crowd-density-t71-2026-10-27/`. The before stills come from the same build with the layout
temporarily at `"tiers": 3` (the code is identical at 3 tiers, `Crowd.StableBase`); the file was restored afterwards and
`git status` was clean. `capture-greybox.ps1` and `capture-spell-vfx.ps1` have no `-Staging` parameter: they write `runs/`
(never committed), and the pngs were copied from there.

| Pair | Before | After |
|---|---|---|
| greybox, 6 shots | `before-greybox/01..06-*.png` | `after-greybox/01..06-*.png` |
| spell VFX, 11 shots | `before-spell-vfx/01..11-*.png` | `after-spell-vfx/01..11-*.png` |

Both captures exited 0 (`capture: ok`, `capture-spell-vfx: OK`, `MAGEVR_CAPTURE_DONE shots=11`); the before capture logged
`cards=436`, the after run `cards=804`. My own reading of `01-initial-view.png`: the stands rise higher on the flanks and
in the middle (the far rows now reach into the upper sky), but the pink band stays and the centre gap at the dais axis is
still open; the step toward S1 is real and modest. That is not the critic's verdict.

## 7. Not done / for the Director

- **The blind critic round** (density, threat-colour class safety, ember flecks over the crowd band) is not part of this
  lane. Run it on the pairs above. Any threat-colour or ember-fleck regression blocks the merge per the card.
- The ember-fleck watch item of T65 now has two more rows behind it; the critic's (c) is the check.
- `docs/concepts/WOLF-PLAN.md`: no box touched, no evidence line.
