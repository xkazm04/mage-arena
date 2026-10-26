---
title: The cinder hound's glow at 8 m and 5 m on the committed asset, and capture-rival.ps1 on /Game/Characters (T68, T69)
status: RAN - desktop proxy (not Quest truth). The committed M_CinderHound already carries the chosen soft crack bloom; no parameter changed, no asset re-imported.
cards: apps/vr/tasks/T68-capture-rival-characters.md, apps/vr/tasks/T69-hound-glow-8m.md
host: Wolf kaziho, UE 5.8.3 CL 58210709, branch autopilot/accepted-idea-delivery-fd621233
---

# Hound glow at 8 m (T69) and the capture scripts (T68)

Stills, runs and logs are under `C:/Users/kazda/mage-art-staging/t69-hound-glow-2026-10-26/` and
`.../t68-rival-2026-10-26/` (PNG is LFS and `runs/` is never committed, so nothing of them is in the repo).

## T68 - the instrument

- **The failure, reproduced.** `capture-rival.ps1` imported Brennic through the ImportCreatureMesh commandlet before every
  run. Since 16ffa56 the commandlet writes into `/Game/Characters` (`CreatureImport::FolderPath`), which holds the
  committed assets. Run alone on a clean worktree it ended `MAGEVR_CREATURE_IMPORT fail Brennic height 0.0180 m is more
  than 5 % off 1.7992 m (scale)` (also `clips=0`): a re-import over an existing asset ignores the import scale. So the
  failure is the script re-importing, which the script fix removes; the commandlet itself is unchanged.
- **The fix.** `capture-rival.ps1` imports nothing and has no scratch folder or sha256 source table. It refuses to start
  when one of the six committed meshes or materials is missing or an LFS pointer, or `Content/Characters` is not clean,
  and fails when the run leaves a tracked file there changed. Outputs are the same: stills, crop sheets, manifest, clip
  timeline, SHA256SUMS.txt, budget runs, logs.
- **Proof.** `capture-rival.ps1 -Run T68-rival -Budget -BudgetPrefix t68rival` ended `rival: ok`: `MAGEVR_RIVAL_FIGURE
  loaded /Game/Characters/Brennic/SK_Brennic.SK_Brennic lod0tris=23990`, `MAGEVR_CAPTURE_DONE shots=113`, all four clip
  sets, 3 budget runs ok, 125 files staged with SHA256SUMS.txt, `git status` clean under `Content/Characters`. The
  committed Brennic stands 179.9 cm (the spawn log in the creature capture; the commandlet's expected 179.92 cm), so the
  scale the import check guards holds on the committed asset.
- **Audit of `capture-creature-import.ps1`: it failed the same way** (its default run imported all three over the
  committed assets, Brennic first). Imports are now opt-in (`-Import <names>`, for an empty folder only), with the same
  clean-tree guard; `-DistanceM` passes `-MageArenaCreatureDistanceM`.

## T69 - before numbers on the committed material

`CreatureImportCapture.cpp` used to apply the C++ defaults over the material for its "chosen" shots, so it could not say
what the asset carries. It now reads the loaded material first. The log line of the 8 m run:

`MAGEVR_CREATURE_IMPORT committed M_CinderHound ember=1.600 soft=1.200 rim=0.000 softRadiusUv=0.0080 read=1`

That equals the chosen glow of CREATURE-CAST-UE section 2.4 (1.6 / 1.2 / 0, radius 0.008): the committed hound already has
the soft crack bloom. Same measure as that section (`glow.py`: hound pixels from the shadowless mask against the empty
plate; glow = hue 5-50 deg, S > 0.45, V > 0.55; core = median of the brightest 3 %; yellowed = R >= 0.98 and G >= 200/255).
Hound pixel counts move about 1-2 % between runs, so shares are good to about a point.

| Committed, T56 dusk | Front glow % | core sRGB | yellowed | Three-quarter glow % | core sRGB | yellowed |
|---|---|---|---|---|---|---|
| 8 m (before = after) | 27.2 | (255, 169, 68) | 0 | 25.2 | (255, 168, 68) | 0 |
| 5 m | 24.2 | (255, 171, 68) | 0 | 23.2 | (255, 171, 68) | 0 |
| 8 m, CREATURE-CAST record | 27.4 | (255, 170, 68) | 0 | 25.3 | (255, 168, 68) | 0 |
| 8 m, base without bloom (this run) | 17.8 | (255, 171, 68) | 0 | 17.0 | (255, 169, 68) | 0 |

No pixel is pale yellow-white (R and G both >= 0.97) in any shot. The bloom is the 10-point lift over the base; the
committed asset has it.

## Decision: no change

- A stronger soft bloom was measured, not applied: `-GlowSoft 2.0` (the "soft" candidate, ember 1.6, rim 0) at 8 m gives
  33.2 % front / 29.2 % three-quarter, cores (255, 168, 67) and (255, 167, 68), yellowed 0, body median unchanged
  (79, 38, 30) / (84, 40, 31). It is available, stays orange and moves no core. It is a number, not a look: the share
  counts lit hide beside the cracks, and Athena's note (below) says the gap that remains is value contrast against the
  sand, which a wider halo does not buy. A wider halo needs the hound material re-imported, and re-importing over a
  committed asset fails the scale check shown above, so it also needs an empty-folder import path first. That choice is
  Athena's; recommendation: keep 1.2 until the value-contrast look (a darker backing or a rim against the sand) is
  chosen, and then change both together.
- The tonemapper and the exposure were not touched. The asset is untouched.

Athena, 10-26, recorded, not a lane: Fire reads orange on orange sand; the remaining gap is VALUE contrast (a darker
backing or rim against the sand), not hue; no white core.

## Gates

- `build.ps1`: `Result: Succeeded` (twice, the second after the capture change).
- `MageArena.`: 338 passed, 0 failed (`TEST COMPLETE. EXIT CODE: 0`); no new reds, the DF-011 `MageArenaDesign` reds were
  not run here and are unchanged.
- D-G4 over three runs (`capture-creature-import.ps1 -DistanceM 5 -Budget -BudgetRuns 3`, the three figures in the scene,
  desktop proxy): draw calls max 131, 131, 131 (median 127); BasePass triangles max 219,858 / 218,994 / 219,042, all
  within 350k. The `capture-rival.ps1` budget runs (`t68rival1-3`) also passed with the cast drawn from `/Game/Characters`.
