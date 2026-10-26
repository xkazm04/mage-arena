---
title: The mire maw's seam ledge and ground notch - a smoothing band at the cut on the finished LOD0, measured and re-imported
channel: vr
date: 2026-10-26
status: RAN - a Taubin smoothing band at the cut (positions only, 17,490 tris, 1 component, watertight) cut the tallest line of depth jumps in the cut band from 0.458 m to 0.110 m (right flank) and from 0.302 m to 0.128 m (left), under the body's own tallest relief line (0.144 / 0.140 m), and the ground notch from 12.0 cm to 2.6 cm. A fresh blind critic read both flanks as better (right REDUCED BUT VISIBLE, left a soft overlap instead of a ruler-straight seam) and found one new texture smear on the right flank's join. The seam fix was re-run on the moved surface (band step 0.012 or less per channel). The committed maw is replaced through a new delete-first ImportCreatureMesh -replace (9 assets, 0 redirectors); MageArena. 339/339; D-G4 in the creature scene max 127 draw calls.
sources: [docs/research/CREATURE-CAST-UE-2026-10-24.md (section 1, section 5), docs/research/MAW-MV-2026-10-24.md, docs/research/CREATURE-UE-IMPORT-2026-10-24.md, docs/research/HOUND-GLOW-8M-2026-10-26.md, apps/vr/tools/mesh/README.md, apps/vr/tasks/T70-maw-seam-ledge.md]
---

# The mire maw's seam ledge, 2026-10-26 (T70)

**Label: desktop proxy (Wolf, RTX 4090, D3D12, forward shading, `-nohmd`), not Quest truth.** UE 5.8.3 (CL 58210709,
`Build.version` read before the first build). No combat number moved; nothing under `apps/vr/data/` or the clip corpus
was touched. No paid call, no image generation, no ComfyUI.

All intermediates are in `C:/Users/kazda/mage-art-staging/maw-seam-ledge-2026-10-26/` (`<st>/` below).

## Outcome

- **Route: a smoothing band on the remesh**, one of the three CREATURE-CAST-UE section 5 names, applied to the finished
  LOD0 (`apps/vr/tools/mesh/seam_ledge.py smooth`). It moves vertex positions only. Triangles and UVs stay, so the mesh is
  one watertight component by construction, the UV layout still fits the bake, and the s7 rig fit re-runs unchanged.
- **The ledge is measurably smaller and the ground notch nearly gone** (section 2). What remains in the band is no
  taller than the tallest relief line elsewhere on the body (a leg's edge in front of the belly).
- **The blind critic reads both flanks as better**, not as gone. It also found one new artefact: a jagged texture smear on
  the right flank's join (section 4).
- **Structure PASS**: 17,490 tris (the budget is at most 20,000, and the brief's cap is 17,490 + 10 % = 19,239),
  1 component, watertight, height 1.25092 m.
- **The colour step stays at 0.012 or less per channel on the flank bands** after the seam fix is re-run on the moved
  surface (section 3).
- **The committed maw is replaced, same nine asset names**, through the new `ImportCreatureMesh -replace` (section 5).
  The stop rule did not fire.

## 1. Inputs (`<st>/inputs.sha256`)

| File | sha256 |
|---|---|
| finish `.blend` of record (`maw-seam-2026-10-24/finish-uv/MireMaw_finish.blend`) | `ad97ad7e80816c89...` |
| its GLB (`finish-uv/MireMaw_finish.glb`, the 17,490-tri finish) | `d4330d53dcdaefd3...` |
| seam-fixed texture of record (`finish-uv/T_MireMaw_BaseColor.png`) | `d46f288fb31ab978...` |
| source-id bake (`maw-seam-2026-10-24/uvfix/MireMaw_source_id.png`) | `26a85eadf8b904bf...` |
| the unfixed bake (`maw-mv-2026-10-24/finish-bake/T_MireMaw_BaseColor.png`) | `a9e50b1ff0d440fc...` |

`tasklist` showed no Unreal and no Blender before each step. Blender (4.2.1 LTS) and Unreal never ran at the same time.

## 2. The geometry pass and the ledge numbers

### 2.1 What the ledge is

Slices of the merged source at y -0.5 to -0.1 (`<st>/probe/src-slices.png`) show how the two halves meet:

- The s7 front half keeps its front legs down to the ground at x ±1.0 m through the cut planes.
- The s7301 back half is a closed oval with its belly at z 0.2 m there, and its hind legs further back.
- The voxel union therefore shows the front half's cap plane as a vertical wall at y ≈ -0.35 to -0.38, about 46 cm tall on
  the right flank.
- Where the front leg is cut by the plane, a square notch is left at the ground.

### 2.2 The smoothing (the run of record)

```
blender -b maw-seam-2026-10-24/finish-uv/MireMaw_finish.blend --python-exit-code 1 -P apps/vr/tools/mesh/seam_ledge.py -- smooth <st>/fix/finish/MireMaw_finish.blend <st>/fix/smooth.json
```

**Defaults:** centre y -0.32, full weight within 0.12 m, zero past 0.24 m, 120 lambda/mu pairs (0.5 / -0.53), the top 3 cm
pinned.

**Record:**

| Measure | Value |
|---|---|
| Vertices weighted | 1,858 of 8,689 |
| Largest move | 10.4 cm (p95 of the weighted vertices: 5.2 cm) |
| Faces whose normal turned past 90 degrees | 115 |
| Height | 1.25092 m (unchanged) |
| Time | 4.3 s |

**Tries before the record** (`<st>/try/`):

- **40 pairs, core 0.08 / edge 0.20, no pin.** This lost 1.7 cm of height: the top spikes sit in the band. That is why the
  top pin exists. With the pin added, the same settings left the left flank's ledge 0.156 m tall.
- **250 pairs.** This gained little over 120 (0.092 / 0.106 m) and folded more faces (151).
- **An unfold pass** (plain umbrella steps on the folded faces) made the folds worse: 258. It was removed and not kept.

**Card:**

```
node apps/vr/tools/mesh/mesh-card.mjs <st>/fix/finish/MireMaw_finish.glb --stage finished --class MireMaw --out <st>/fix/card-finished.json
mesh-card pass (stage finished, score 100): 17490 tris, 1 components, height 1.25092 m; no findings
```

- Boundary, non-manifold and mis-wound edges: 0 / 0 / 0.
- Area: 18.35 m² (before: 19.11).
- Extent: 2.384 x 2.227 x 1.251 m (unchanged).

### 2.3 The measure

```
blender -b <finish.blend> --python-exit-code 1 -P apps/vr/tools/mesh/seam_ledge.py -- measure <out>.json
```

**How it samples.** Orthographic rays along -X and +X every 2 mm (500 px/m, the critic renders' scale) build a depth map
of each flank: the outermost |x| that each ray hits.

**What it reports**, inside the cut band (y -0.30 ± 0.15) and, for comparison, outside it:

- **The depth ledge:** the tallest line of rows whose depth jumps by 1 cm or more between two adjacent columns. A ledge is
  one tall line at one y; a wart makes a short one.
- **The normal ledge:** the same with the normals turning by 45 degrees or more.
- **The outline step:** within 1 cm of y.
- **The ground notch:** the largest rise of the bottom outline within 1 cm, from a column that touches the ground.

The outline numbers are the same for both flanks, because both flanks share one orthographic silhouette.

| Flank | Measure | Before (`d4330d53`) | After | Outside the band (reference) |
|---|---|---|---|---|
| right (+X) | depth ledge: height of the line / median jump on it / y | **0.458 m** / 15.9 cm / -0.381 | **0.110 m** / 16.2 cm / -0.361 | 0.144 m / 5.4 cm (y 0.701, the hind leg's edge) |
| right | normal ledge (45 degrees) height / median turn | **0.298 m** / 74.8 deg | **0.124 m** / 94.1 deg | 0.088 m / 83.8 deg |
| left (-X) | depth ledge height / median jump / y | **0.302 m** / 9.5 cm / -0.381 | **0.128 m** / 10.6 cm / -0.353 | 0.140 m / 32.0 cm (y -1.053, the jaw) |
| left | normal ledge height / median turn | **0.272 m** / 74.3 deg | **0.108 m** / 60.3 deg | 0.080 m / 81.0 deg |
| both | outline step within 1 cm (band max) | 12.0 cm | 10.8 cm | 36.8 cm |
| both | **ground notch** (band max) | **12.0 cm** | **2.6 cm** | 26.6 cm |

**Reading.**

- The vertical wall is gone as a tall line. Its height fell by 76 % on the right and 58 % on the left.
- What is left in the band is a 11-13 cm line at y -0.36. Its jump is about as deep as before: it is the back edge of
  the front leg against the recessed flank (`<st>/probe/depth-cmp.png`), and it is no taller than the tallest relief line
  outside the band.
- The normal ledge stays a little above the outside reference on both flanks (0.124 / 0.108 against 0.088 / 0.080 m).
- The outline step in the band barely moved (12.0 to 10.8 cm): it is the belly line rising behind the front leg.
- The square ground notch is down to 2.6 cm.

**Renders.**

- Textured, 500 px/m, the seam lane's turn script: `<st>/critic/src/{before,after}/r_{045,090,270,315}.png`.
- Untextured grey, both flanks: `<st>/probe/grey-cmp.png`. The right flank's ledge became a soft wavy crease. The left
  flank keeps a softened vertical crease where the front shoulder ends.

## 3. Texture: the seam fix re-run on the moved surface

The UVs did not change, so the texture of record (`d46f288f`) still maps. But the seam report reads texel positions,
and those moved. Carried unchanged onto the new surface, the right flank's band measured **+0.0149 / +0.0106 / +0.0148**,
over 0.012 (left: -0.009 / -0.008 / -0.008). So `seam_fix.py` was re-run, with its defaults, from the unfixed bake and
the same source-id PNG. The source-id PNG is UV-tied, so it still fits:

```
python apps/vr/tools/mesh/seam_fix.py <st>/fix/finish/MireMaw_finish.glb maw-mv-2026-10-24/finish-bake/T_MireMaw_BaseColor.png <st>/fix/uvfix/T_MireMaw_BaseColor.png --source maw-seam-2026-10-24/uvfix/MireMaw_source_id.png --record <st>/fix/uvfix/seam_fix.json
```

**The new texture:** `e6c4df4293b87ab8d006b717e297f2759358597468cd2fd8759db1caab52bf77`. The tongue texel (571, 658) is still
sRGB (112, 44, 46), the `FlatSlotUv` of the spec.

| Flank | Measure (back - front, R / G / B) | d46f288f on the old surface (record) | e6c4df42 on the new surface |
|---|---|---|---|
| left | band | -0.005 / -0.004 / -0.005 | **-0.0105 / -0.0082 / -0.0095** |
| right | band | +0.010 / +0.008 / +0.012 | **+0.0112 / +0.0081 / +0.0116** |
| left | patches adjacent, signed mean | +0.007 / +0.007 / +0.003 | +0.0004 / +0.0002 / -0.0015 |
| right | patches adjacent, signed mean | -0.006 / -0.006 / -0.006 | **-0.0148 / -0.0125 / -0.0153** |
| left / right | noise floor | 0.046-0.060 / 0.032-0.035 | 0.037-0.054 / 0.032-0.040 |

- **The band step, the 0.012 of record, holds on both flanks.**
- The right flank's adjacent 2 cm patches now differ by up to 0.015, against -0.006 before. That is still under half the
  noise floor, but it is a regression on that one measure.
- A source-id bake re-done on the new surface (`<st>/try/srcid/`) was tried and not kept: the left band came out at
  -0.017.

## 4. The blind critic

**The procedure.**

- **Who:** one fresh agent, given only the five sheets in `<st>/critic/sheets/`. It was not told which panel was fixed,
  nor what fix was expected.
- **The sheets:** right flank, left flank, two three-quarter views, and a quarter-size sheet. The fixed panel was put
  left on some sheets and right on others.
- **The key:** `<st>/critic/KEY.txt`.
  - sheet1: A = before;
  - sheet2: A = after;
  - sheet3: A = after;
  - sheet4: A = before;
  - sheet5: C (top row) = before, D = after.

The critic wrote its per-flank verdict as if B were the newer panel, so its labels are translated below with the key.
Its descriptions are kept.

| Flank | Better panel (critic) | = | Ledge | Ground notch |
|---|---|---|---|---|
| right (sheet1) | B | **after** | **REDUCED BUT VISIBLE**: "the ledge has softened into a crease ... the flat panel is mostly gone and the shading now flows across the join" | **REDUCED**: "B's square gap is narrower, but a vertical step is still there" |
| left (sheet2) | A | **after** | **better**: the after panel is "a soft vertical overlap"; the before panel is "a ruler-straight vertical line ... about 80 % of body height" | **better**: the before panel's "square, flat pink block ... reads as a cut face"; the after panel has "a ragged pink patch ... not a square notch" |

**Other views and new artefacts:**

- **Three-quarter views** (sheets 3 and 4): about the same; the after panel is marginally cleaner in sheet 3 and has a
  softer shoulder crease in sheet 4.
- **Quarter size:** in the after panel the right flank's join "is faint; I would not notice it without looking for it".
  The left flank's join "still reads" in both panels, as a straight line with a pink block at the foot.
- **New artefact (after, right flank):** "a dark, torn-looking texture smear or jagged scrape ... right on the join".
  - It is in the texture, not the geometry: the grey render shows a smooth crease there.
  - The carried texture shows a fainter dark version of it.
  - The texels of the old cap face are now stretched over the crease, and the re-run seam match lightens about 30 of
    them (x 0.41, y -0.29, z 0.39) by more than 10/255 (`<st>/probe/smear-carried-vs-refix.png`).
- **No other new artefact.** The flat slabs on the hind quarter and the hole on the back are in both panels.

**Against the stop rule:** the critic reads the ledge as better on both flanks, not as "no better". The geometry passed
the card, and the import was clean. So the committed maw was replaced.

## 5. The import: delete first, scoped to one creature

**Why a new path.** T68 found that an import over committed assets keeps their old import settings: Brennic came in at
0.018 m and failed the height check.

**What changed (`b8fef93`):**

- **The commandlet:** `ImportCreatureMesh` now refuses a folder that already holds packages, unless `-replace` is given.
  `-replace` deletes exactly the `.uasset` files directly in `/Game/Characters/<Name>`
  (`CreatureImport::ReplaceablePackageFiles`). It never touches `Source/`, and it refuses if any of the packages is
  already loaded. It then rescans the folder and imports the same names. Afterwards it logs the folder's assets and
  redirectors, and fails on any redirector.
- **The script:** `capture-creature-import.ps1 -Replace` passes `-replace` for exactly one `-Import` name. It reads the
  maw from `<st>/fix/rig` by sha256.
- **The test:** `MageArena.CreatureImport.ReplaceOnePackagePerName` (new). It pins two things:
  - the delete list: of a scratch folder holding two packages, a text file and a `Source/` package, the list is exactly
    the two packages;
  - the committed `MireMaw` folder: exactly `ExpectedAssetNames` (9: texture, `M_MireMaw`, `M_MireMaw_Flat`, mesh,
    skeleton, four clips), one package per name, each in a package of its own name, no `ObjectRedirector`, and one
    `.uasset` file per name on disk.
- **Red first: not shown.** The test was green on the committed set before the replace (5/5 `MageArena.CreatureImport`)
  and after it. No polluted folder was built to show it red.

**Rig** (`<st>/fix/rig/`; MAW-MV's commands with the s7 fit `mire-maw-s7202-s7.json`, unchanged):

| Step | Result |
|---|---|
| `beast_parts.py` | 1 part, 17,490 -> 17,752 tris, 0 non-manifold edges |
| `rig_anim.py` | 11 bones, 1.2509 m, 0 zero-weight proxy vertices, 0 fallbacks, clips Idle 61 / Attack 67 / Hit 25 / Death 61 frames |
| FBX | `e7a2cd73c959a523015c68ff0db7f8efd5e8ae094c16eb98c66ebfdc84eca59c`, 1,271,900 bytes |
| `evidence.mjs` | exit 0, 6 PNGs in `<st>/fix/evidence/`: the jaw opens, the tongue lashes out and reels in |

**The import run:**

```
powershell -NoProfile -ExecutionPolicy Bypass -File apps/vr/tools/capture-creature-import.ps1 -Run T70-after-8m -Import MireMaw -Replace -Staging <st>\ue\after-8m
MAGEVR_CREATURE_IMPORT replace deleted /Game/Characters/MireMaw/M_MireMaw   (and the other 8)
MAGEVR_CREATURE_IMPORT mesh /Game/Characters/MireMaw/SK_MireMaw.SK_MireMaw bones=11 lod0tris=17752 lods=1 slots=2 clips=4 bounds origin=(-0.0, -0.0, 62.6) extent=(111.4, 119.2, 62.5) cm height=1.2509 m (expect 1.2509)
MAGEVR_CREATURE_IMPORT packages /Game/Characters/MireMaw assets=9 redirectors=0
MAGEVR_CREATURE_IMPORT done MireMaw saved=9
creature-import: ok
```

**Committed.** `Source/MireMaw.fbx` and `Source/T_MireMaw_BaseColor.png` were replaced by the two inputs of record.

## 6. Stills (`<st>/ue/`, 1920x1080, T56 dusk)

| Folder | What |
|---|---|
| `before-8m/shots/`, `after-8m/shots/` | the committed cast at 8 m (`01-03` the maw front, three-quarter and back; `16-cast-duel`) |
| `before-5m/shots/`, `after-5m/shots/` | the same at 5 m (`-DistanceM 5`) |
| `maw-8m-before-after-2x.png` | the maw's three views at 8 m, 2x crops, before left, after right |
| `cast-quarter-before-after.png` | the cast at quarter size, before and after |

**By eye at 8 m:**

- The two maws read alike.
- These three angles do not show a flank square-on, and the seam was hard to see at this distance before (CREATURE-CAST-UE
  section 2.5).
- The gape, the teeth and the tongue are unchanged.

## 7. D-G4: the creature scene, three runs of 60 s

**The scene:** `-MageArenaCreatureImportScene`, the D-G4 stress scene (20 enemies, 100 projectiles, both hands, 6 Niagara
systems) with Brennic, the hound and the maw standing in it. It ran through
`capture-creature-import.ps1 -Run T70-after-5m -DistanceM 5 -Budget -BudgetRuns 3`, T69's command.

Every run logged `ok=True done=True`, parity ok (210 planned parts / 11 tuples) and `budget: ok (3 runs)`.

| Run (scene) | Draw calls median / max | BasePass tris median / max | SK_MireMaw BasePass |
|---|---|---|---|
| T70-after-5m-with1 (creature scene) | 123 / **127** | 216,242 / 217,122 | 1 draw, 17,752 |
| T70-after-5m-with2 (creature scene) | 124 / **127** | 217,322 / 219,042 | 1 draw, 17,752 |
| T70-after-5m-with3 (creature scene) | 124 / **127** | 217,434 / 219,042 | 1 draw, 17,752 |
| T69, the same creature scene (HOUND-GLOW-8M) | 127 / 131 | max 219,858 | |
| Limit (PROJECT-PLAN section 7) | <= 150 | <= 350,000 | |

- **The creature scene peaks at 127 here, against 131 in T69.** The maw's mesh, sections and materials are the same
  (17,752 tris, one BasePass draw), so the 4 calls are run-to-run variance in the scene, not this change.
- **The spell scene** (127 in T67) was not re-run: nothing in it changed.

## 8. Acceptance commands and their output

| Command | Output |
|---|---|
| `powershell -NoProfile -ExecutionPolicy Bypass -File apps/vr/tools/build.ps1` (cold, base) | `Result: Succeeded`, 625 s |
| the same after the C++ change | `Result: Succeeded` |
| `UnrealEditor-Cmd ... -ExecCmds="Automation RunTests MageArena.;Quit" -unattended -nullrhi` (corpus regenerated first) | `succeeded 332, succeededWithWarnings 7, failed 0, notRun 0` (339 = 338 + `ReplaceOnePackagePerName`), `TEST COMPLETE. EXIT CODE: 0` |
| `... RunTests MageArenaDesign.` | 5 of 7; failed exactly `MageArenaDesign.Duel.FireSeated` and `MageArenaDesign.Session.FullSeated` (DF-011, accepted) |
| `... RunTests MageArena.CreatureImport` before and after the replace | 5/5 both times |
| `powershell ... capture-rival.ps1` | `rival: ok`, exit 0 |
| `capture-creature-import.ps1` before (8 m, 5 m) and after (8 m with `-Import MireMaw -Replace`; 5 m with `-Budget -BudgetRuns 3`) | `creature-import: ok` all four; `MAGEVR_CAPTURE_DONE shots=17` |
| `python -m unittest apps/vr/tools/mesh/test_ledge_lib.py test_seam_lib.py test_merge_halves.py` | `Ran 46 tests ... OK` (13 new) |
| `node apps/vr/tools/lint/unused-members.mjs` / `format-check.mjs` | OK (661 members) / OK (5 changed C++ files) |
| `node tools/check-commits.mjs` | `check-commits OK` |
| the merge gate's test line (agy-guard, check-pin and self-test, check-overlay and self-test, leonardo-redaction, repo-law) | pass 40 / pin OK 9 files, 45 vectors / corruption detected (self-test) / overlay OK / renamed column detected (self-test) / pass 24 / repo-law OK 4/4 |
| `git lfs ls-files \| grep MireMaw` | the nine `.uasset` files with new oids (for example `38a37bad0c SK_MireMaw.uasset`, `11a2cbb0be T_MireMaw_BaseColor.uasset`), `e7a2cd73c9 Source/MireMaw.fbx`, `e6c4df4293 Source/T_MireMaw_BaseColor.png` |

**A side effect:** `capture-rival.ps1` ran with its default `-Staging` (`t38-rival-2026-10-25`), so it re-staged its 125
files there over the T38 lane's copies. Nothing in the repo changed.

## 9. Commits

- `d9e93ee` T70: card for the maw's seam ledge and ground notch at the cut
- `fb017e8` T70: seam_ledge.py measures the merge's ledge on side depth maps and smooths it on the LOD0
- `b8fef93` T70: ImportCreatureMesh -replace deletes one creature's packages first, and a test pins one package per name
- `2e1580f` T70: the committed mire maw replaced by the cut-smoothed mesh, same nine asset names
- the commit carrying this report, the WOLF-PLAN and VISUAL-PLAN lines

## 10. Open questions

1. **The right flank's join smear.** About 30 cap-face texels are stretched over the new crease, and the re-run seam match
   lightens them. Options:
   - a seam-fix option that leaves texels on stretched triangles alone;
   - a re-bake of the whole finish onto the smoothed LOD0 (finish.py's bake on fixed geometry, a new lane step).

   Athena's call whether it matters at 8 m. It did not show in the stills.
2. **The left flank still reads at quarter size** as a straight line with a pink block at the foot. The pink is the
   front leg's texture (the s7 bake), not the seam. A tighter back fit at the merge (route 3) is the remaining geometry
   lever.
3. **The right flank's adjacent 2 cm colour patches** moved from -0.006 to -0.015 (the band holds at 0.012).
4. **The 115 folded faces** are inside the crease. The card cannot see them; the critic did not report a fold.
5. **The broad head-to-rump two-tone and the palette** (MAW-MV critic defect 2) are unchanged. They are out of this lane.
6. **Next lane: crowd density** (Athena's visual order, after this lane settles). It was not started here.
