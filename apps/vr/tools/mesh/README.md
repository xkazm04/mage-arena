# Mesh lane: image to engine-ready mesh, gated at every step

Scripts that take one concept image to a finished LOD0 mesh on the x64 host (RTX 4090), with a mechanical gate before
and after each expensive step. No generated asset enters git here: inputs are copied, and every output goes outside
the repo (the proof below used `C:/Users/kazda/ai-tools/runs/proof/`). Node scripts use built-ins only; Blender 4.2
LTS headless does the measuring (set `BLENDER` to use another `blender.exe`).

| File | Role |
|---|---|
| `input-gate.mjs` + `image_stats.py` | gate 1: can this image (or view set) be reconstructed? Verdict JSON with defect codes |
| `comfy.mjs` | start / guard / run / free / stop the local ComfyUI; `run` hands the job to `../assets/trellis.mjs` unchanged, or to `trellis-mv.mjs` with `--back` |
| `trellis-mv.mjs` + `trellis-mv.test.mjs` | the two-view (front + back) TRELLIS.2 workflow builder and runner, and its offline test |
| `merge_halves.py` + `halves_lib.py` (+ `test_merge_halves.py`) | split-and-merge: the front half of the front-image mesh and the back half of a back-image mesh, posed in the finish frame and joined into one textured source GLB for `finish.mjs --rot 0` (see "Split and merge") |
| `bake_source_id.py`, `seam_fix.py` + `seam_lib.py` (+ `test_seam_lib.py`) | the flank seam of a merge, fixed on the finished bake in UV space: which half each texel came from, a local colour match of the back half onto the front, a feather across the seam, and the seam report (see "The flank seam") |
| `seam_ledge.py` + `ledge_lib.py` (+ `test_ledge_lib.py`) | the geometric ledge and ground notch of a merge at the cut: measured on orthographic side depth maps, and smoothed on the finished LOD0 (positions only) (see "The seam ledge") |
| `mesh-card.mjs` + `mesh_metrics.py` | gates 2 and 3: the structural card of a GLB, FBX or .blend |
| `finish.mjs` + `export_glb.py` | raw card, `../assets/finish.py` (runs on Blender 4.2.1), export, finished card. `--mode bake` (default): the TRELLIS albedo baked onto LOD0 as `T_<Name>_BaseColor.png`; `--mode flat`: one flat palette colour per material (see "Finish modes") |
| `finish-args.mjs` + `finish-args.test.mjs` | the finish.py argument list finish.mjs builds (flat stays the pre-bake list exactly) |
| `finish_lib.py` (+ `../assets/test_finish.py`) | finish.py's Blender-free logic: mode and option parsing, sRGB/Lab, the style clamp, UV coverage, bake-miss accounting and filling (`python -m unittest apps/vr/tools/assets/test_finish.py`) |
| `beast_fit.py` (+ `test_beast_fit.py`), `beast-fits/*.json` | the Bout 2 beasts' fit: joints, jaw/tongue weight rules and hand-made parts for `../assets/beast_parts.py` and `../assets/rig_anim.py` (STYLE Hound, Maw). Without a fit file both scripts use the 2026-10-21 tables; a mesh with other proportions gets its own file (see "Beasts") |
| `budgets.json` | per-class height and triangle targets, each citing its source (T38 card, `metrics.py` TRI_BUDGET_LOD0 and BEASTS) |
| `gate-lib.mjs` + `gate-lib.test.mjs` | the pure scoring both gates use (`node --test apps/vr/tools/mesh/gate-lib.test.mjs`) |
| `components.py`, `blender.mjs` | numpy connected components; the Blender runner |
| `evidence.mjs` + `blender-compat.mjs` | the T38 evidence renders: `../assets/render_evidence.py` (unchanged on disk) run from a temp copy whose EEVEE id fits this Blender; fails on a Blender error or a missing PNG |

The rules come from the ai-registry game-production subjects image-to-3d-input-gating and generated-mesh-acceptance:
findings are codes from a closed vocabulary (never prose), an unmeasured criterion keeps a verdict off pass, ambiguity
resolves toward the harsher verdict, and **a pass is never a quality claim** (both JSONs carry that notice).

## Pipeline order

```
1. input gate     node apps/vr/tools/mesh/input-gate.mjs <concept.jpg> --out <dir>/1-input-gate.json
                  (a view set: --front a --side b --back c [--require front,side,back])
2. TRELLIS.2      node apps/vr/tools/mesh/comfy.mjs start
                  node apps/vr/tools/mesh/comfy.mjs run <concept.jpg> <label> 42 <dir>     (guard first, then trellis.mjs)
                  node apps/vr/tools/mesh/comfy.mjs free
3. raw card       \
4. finishing       > node apps/vr/tools/mesh/finish.mjs <dir>/<label>.glb <dir>/finish --class Humanoid --name <Name> --mode bake
5. finished card  /
6. stop           node apps/vr/tools/mesh/comfy.mjs stop
```

Gate at each step:

| Step | Proceeds on | Stops on |
|---|---|---|
| 1 input gate | exit 0 (pass, warn) | exit 1: `fail` (needs another input) or `prepare` (fix the image, e.g. remove the background, and gate it again from scratch) |
| | exit 3 `incomplete`: only see-only criteria unmeasured. Proceeding records the input as **ungated** on them (the JSON's `ungated` line); today every character input lands here, because no judge that can see is wired in | |
| 2 guard | queue empty, free RAM >= 8 GB, GPU utilisation < 40 %, VRAM outside this ComfyUI < 6 GB | otherwise waits, looking again every 2 min |
| 3 raw card | every finding has remedy `finish` | a `reroll` or `neither` code (`empty-mesh`, `degenerate-bbox`, `too-many-parts`, `tris-under-budget`): finish.mjs refuses |
| 5 finished card | exit 0 (pass, warn) | exit 1 `fail`, exit 3 `incomplete` |

Exit codes for all three gates: 0 proceed, 1 refuse, 3 incomplete, 2 usage or error.

### Input gate criteria

Measured (on a block-mean copy at most 512 px long): `single-subject` (8-connected foreground components; any
component at least 5 % of the largest is a subject), `plain-background` (border band: 98 % of its pixels near the median
colour, the four sides' medians within 0.04, luminance 0.15-0.95 or transparent), `subject-in-frame` (margin under 1 % is
a hard fail, under 3 % a warning), `subject-resolution` (subject long side >= 512 source px), `view-set-completeness`
(the required views exist), `view-set-consistency` (each view's subject height within 10 % of the master's, feet line
within 5 % of the frame, background colour within 0.1). UNMEASURED, so the verdict cannot pass: `pose-canonicality`,
`silhouette-clarity`, `lighting-neutrality`, `detail-focus`, and in a set `view-feature-correspondence`,
`view-lighting-consistency`.

Codes: `no-subject`, `multiple-subjects`, `subject-touches-frame`, `view-missing`, `view-framing-mismatch` (fail);
`background-not-plain` (prepare); `background-extreme-value`, `margin-tight`, `subject-low-resolution`,
`view-background-mismatch` (warn).

### Mesh card

Vertices are welded by exact position, then: triangles, connected components and their face histogram, specks
(floaters: under max(8 faces, 0.5 % of the faces), the same 0.5 % finish.py drops), boundary and non-manifold edges,
watertight, inconsistently wound edges, degenerate faces, bbox, height, min z and bbox centre. `--stage raw` grades what
finishing resolves as warn; `finished` (or an undeclared stage, the harsher reading) grades it fail. Without `--class`,
world scale and the triangle budget are UNMEASURED and the card cannot pass.

| Code | Remedy | Evidence for the remedy |
|---|---|---|
| `floaters`, `not-watertight`, `winding-inconsistent`, `tris-over-budget`, `scale-off`, `origin-off` | finish | each present on the proof's raw card and gone on its finished card |
| `degenerate-faces` | finish | the voxel remesh rebuilds every face; not yet observed on a raw card |
| `empty-mesh`, `degenerate-bbox` | reroll | a bad draw; a fresh seed can differ |
| `too-many-parts`, `tris-under-budget` | neither | finish keeps separate parts above 0.5 % and cannot add detail |

## Finish modes

Both modes share every geometry step (floaters, scale and height, voxel remesh, collapse to the class target, smart UV),
so the structural card is the same; they differ in what the surface carries.

`finish.mjs` defaults to `bake` (DECISIONS 2026-10-24, Athena 17:56Z: the pipeline default for characters and
creatures). `finish.py` called directly (the HANDOFF rebuild lines) still defaults to `flat`, so those lines rebuild
what they built.

- **`--mode flat`** (an option, the T38 recipe): every face takes the nearest palette colour of its baked texels. Six
  flat materials, no image. It loses everything smaller than a colour region (Brennic 2026-10-24: face, brooch, collar
  plate, straps; saw-toothed region edges). Kept for comparison and for assets that really are flat blocks.
- **`--mode bake`** (the default): the UV islands are repacked (`pack_islands`, concave, margin 0.003), the TRELLIS albedo is baked
  selected-to-active (Cycles DIFFUSE, colour pass only, metallic zeroed first) at `budgets.finish.bakeTextureRes`
  (2048), cage extrusion 0.02 m, max ray distance 0.08 m, margin 16 px EXTEND. A second bake of the source in white
  emission (no margin) is the **hit mask**: an island texel no ray reached reads 0. Misses are counted
  (`bake.missed`, `missed_interior`), filled from their neighbours, and every island texel is checked for black
  (`dark_in_islands_after_fill`, must be 0). When that fill leaves black (`black_after_ring_fill` > 0: it took colour
  from Blender's margin around unhit islands), every miss is filled again from the unfilled bake, from hit island texels
  alone (`refill_rings`), and misses no island path reaches take the median hit colour (`orphan_black_filled`). A
  bake with no black after the first fill (Brennic, the hound) is byte-identical to before. One material `M_<Name>` with `T_<Name>_BaseColor.png`; the GLB embeds it.
  `--clamp posterize|hue|both` (`--strength` <= 0.6, `--levels` >= 4) pulls the texture part of the way toward the
  palette set plus the VISUAL-PLAN (c) accents (bronze, gold, crimson, iron); it never replaces it (`clamp.detail_kept`).
- **`--normal on`** (bake mode only, off by default; off reproduces the bake record, GLB and albedo byte for byte): the
  source's shading normal (its mesh plus its own normal texture) is baked with the albedo's rays (same cage, ray distance,
  margin, repacked UVs, size) as Cycles `NORMAL` in `TANGENT` space, on the LOD0's own MikkTSpace tangents (every face
  is already a triangle, as in the FBX). `T_<Name>_Normal.png` is 16-bit RGB, linear (no gAMA or sRGB chunk), **OpenGL
  Y+**: UE wants DirectX Y-, so the importer sets `bFlipGreenChannel` (with `SRGB` off and `TC_Normalmap`). Misses (the
  albedo's hit mask) become flat (0.5, 0.5, 1.0), never black; the margin is rebuilt from island texels alone, flat
  beyond it. `normal.deviation_*` records the share of texels more than 10 and 20 degrees from flat and the mean, for the
  whole map and the back region (face normal y > 0.5, centroid 0.5-1.5 m). `M_<Name>` takes it through a Normal Map
  node, and the GLB carries it as `normalTexture`.
- **`--expect <reference finish dir>`** fails the run (exit 1) when the finished card's geometry (tris, verts, edges,
  components, height, bbox, area) or the section count differs from that run's: a surface pass must not move the mesh.
- **Rigging a bake finish:** `rig_anim.py` exports the FBX with `path_mode='STRIP'`, so copy `T_<Name>_BaseColor.png`
  (and `T_<Name>_Normal.png`) next to the FBX before `evidence.mjs` or a UE import.

Measured on Brennic seed 7 (docs/research/BRENNIC-MESH-REWORK-2026-10-24.md): finish.py 34 s at 2048, 0.64 % of island
texels missed (mostly the inner cloak and between the legs), 0 black texels after the fill. On the 2026-10-24 maw (a torn
shell filled by FILL) 23 % of island texels missed and 174,922 were black after the first fill; the refill left 0
(docs/research/CREATURE-MESH-2026-10-24.md).

`--normal on` on Brennic seed 7 (docs/research/BRENNIC-CLOAK-NORMAL-2026-10-25.md): +4.6 s bake, +6.6 s finishing, an
18.7 MB 16-bit PNG; geometry identical; a blind critic read SAME at 8 m and a visible gain close up. 1,849 UV islands
under 20 faces (16 % of the faces, 9 % of the texels) bake noisy normals; they hold three quarters of the texels where the map turns inward.

## Beasts

The cinder hound and the mire maw are finished like any class (`--class CinderHound`, `MireMaw`), then get parts and a
rig in Blender:

```
blender -b --python-exit-code 1 -P apps/vr/tools/assets/beast_parts.py -- <finish>/<Name>_finish.blend <rig> <Name> <CinderHound|MireMaw> [<fit.json>]
blender -b --python-exit-code 1 -P apps/vr/tools/assets/rig_anim.py -- <rig>/<Name>_parts.blend <rig> <Name> <rig>/<Name>.fbx <Hound|Maw> [<fit.json>]
cp <finish>/T_<Name>_BaseColor.png <rig>/
node apps/vr/tools/mesh/evidence.mjs <rig>/<Name>.fbx <evidence> <Name> <rig>/<Name>_rig.json Attack <CinderHound|MireMaw>
```

The joints, the jaw and tongue weight rules and the parts (the hound's Ember mouth slit; the maw's mouth slot, tongue and
eyes) were measured on the 2026-10-21 meshes and scale with the height only. A mesh with other proportions needs its own
fit file in `beast-fits/` (`beast_fit.py` says what it must hold; every key is required, none falls back). Measure it from
leg-column centroids and a side or section render of the finished mesh. The 2026-10-24 key-art meshes have
`cinder-hound-s7101-s42.json` and `mire-maw-s7202-s7.json`. Without a fit file the scripts reproduce the old tables (on
the 2026-10-24 hound: the same bones, clips, records and vertex set; the weights differ only at float-noise level, as
two runs of the unchanged scripts do, because the EXACT boolean returns its vertices in a different order each run).
`render_evidence.py` frames the beasts for the 2026-10-21 sizes, so its turnaround crops the longer 2026-10-24 hound and
the wider maw.

## Two views (front + back): `--back`, for the mire maw

```
node apps/vr/tools/mesh/comfy.mjs run <front.jpg> <label> <seed> <outdir> --back <back.jpg> --dry-run     print the workflow, nothing else
node apps/vr/tools/mesh/comfy.mjs run <front.jpg> <label> <seed> <outdir> --back <back.jpg> [--faces N]  guard, then the real job
```

`--dry-run` needs no ComfyUI, no GPU and no upload (the offline test is `node --test apps/vr/tools/mesh/trellis-mv.test.mjs`);
it prints the JSON that would be POSTed to `/prompt` and writes nothing. With `--back`, `trellis-mv.mjs` uploads both
images (`t38_<label>_front.*`, `t38_<label>_back.*`), clones the single-image view chain (LoadImage, RemoveBackground,
ImageCropToMask at pad 1.1) for the back, and replaces `Trellis2Conditioning` (front only) with the installed pack's
`Pixal3DMultiViewConditioning` (front at 0 degrees, back at 180, fov 20). The installed checkpoint has no `proj_linear`
weights, so this averages the views' DINOv3 class and register tokens in the model's global attention mode: the back steers
the whole object, there is no per-voxel projection. Without `--back` nothing changes. A real run is a GPU job: run it only
after the Unreal slot is free (see `docs/research/MAW-BACK-2026-10-24.md` for the order).

## Split and merge (front half + back half), for a torn back

When the two-view run does not close the back (on the 2026-10-24 maw it returned a cloud of 2,994 specks), reconstruct the
back image alone as a single-image job and merge the halves in Blender:

```
node apps/vr/tools/mesh/comfy.mjs run <back.jpg> <label> <seed> <outdir>      then comfy.mjs free, comfy.mjs stop
blender -b --factory-startup --python-exit-code 1 -P apps/vr/tools/mesh/merge_halves.py -- <front.glb> <back.glb> <merged.glb> <rot_front> <rot_back> <cut_y> <overlap> <height> [<record.json>] [--match-back]
node apps/vr/tools/mesh/finish.mjs <merged.glb> <finish> --class <Class> --name <Name> --mode bake --rot 0 ...
```

- `rot_front` turns the front mesh so its face looks down -Y (the value `--rot` would take); `rot_back` turns the back
  mesh so its back-image side looks down +Y. Both are turns, never reflections (a mirror would swap the flanks and invert
  the winding; the script refuses a reflecting pose). Find them from renders: a silhouette match against the image
  leaves a front/back ambiguity of 180 degrees, so pick the side that shows the image's features.
- The back mesh is scaled to the front's full height and its 1-99 percentile footprint (`halves_lib.fit_halves`), so a
  flap at one edge does not set the scale. Each half is bisected at `cut_y +- overlap`, its cut loops are capped, and the
  halves are joined with both TRELLIS textures. `finish.py`'s voxel remesh (and FILL) is the merge; the finished card is
  the watertight check. The record (`<merged>_merge.json`) has the fit, the faces kept, the cap faces and a 1 cm voxel trial.
- Pick `cut_y` behind everything the front half must keep (for the maw: the throat wall at y -0.55, so -0.30). The
  seam is a straight vertical line on both flanks where the two textures and surfaces meet; the bake does not blend it
  (fix the colour step with the next section; a step in the surface's shape stays).
- `python -m unittest apps/vr/tools/mesh/test_merge_halves.py` (numpy only). First run: docs/research/MAW-MV-2026-10-24.md.

### The flank seam (colour match and feather, on the bake)

The two TRELLIS textures differ in colour (the 2026-10-24 maw: olive front, browner back), so the bake shows a straight
colour step on both flanks. Fix it on the finished texture; the geometry is only read, so the finish stays byte-identical:

```
blender -b <finish>/<Name>_finish.blend --python-exit-code 1 -P apps/vr/tools/mesh/bake_source_id.py -- <merged.glb> <fix>/<Name>_source_id.png <height_m>
python apps/vr/tools/mesh/seam_fix.py <finish>/<Name>_finish.glb <finish>/T_<Name>_BaseColor.png <fix>/T_<Name>_BaseColor.png --source <fix>/<Name>_source_id.png --record <fix>/seam_fix.json
```

- `bake_source_id.py` bakes, with finish.py's pose, cage and ray, which half each texel took its colour from (128 front,
  255 back, 0 missed). The bake's switch wanders a few cm around the cut plane (the remesh surface lies between the two
  skins in the overlap band), so the plane alone (`--cut`, the fallback) mislabels a strip on each flank. Its hit set
  must equal the finish's hit mask; on the maw it did to 2 texels in 1.13 M.
- `seam_fix.py` rasterizes the LOD0's UV triangles to give every texel its surface point, moves every back texel from the
  back's local mean colour onto the front's (measured within 5 cm of the seam at anchors along it, 8 cm around each,
  so a flank is compared with the same flank; the local shift fades into the seam-wide one over 15 cm, so a hind leg
  does not take a far anchor's shift; the spread is kept, `--max-gain` 1), blends texels within 3 cm of the seam toward
  their 1.5 cm neighbourhood mean (the feather), and gives the bake margin its nearest island colour. On the maw a
  1.25 contrast gain and no fade left yellow streaks and olive spill on the hind leg (a blind critic's reading).
- The record has the seam report before and after: 2 cm patches either side of the seam on both flanks at z 0.2-0.9 m
  (adjacent, and 4-6 cm out, outside the feather), their noise floor (two patches on one texture), and 5 cm flank bands.
- Copy the new `T_<Name>_BaseColor.png` beside the finish `.blend` before `beast_parts.py` and `rig_anim.py` (the FBX
  carries no texture), and beside the FBX for evidence and the UE import.
- `merge_halves.py --match-back` is the other route: it moves the back TRELLIS texture onto the front's statistics in the
  overlap band before the join. On the maw it overshot (the band's front statistics are dominated by the lighter jaw and
  belly, so the back came out lighter than the flank it meets); the bake fix above is the one of record.
- `python -m unittest apps/vr/tools/mesh/test_seam_lib.py` (numpy and scipy). First run: docs/research/CREATURE-CAST-UE-2026-10-24.md.

### The seam ledge (geometry at the cut, on the finished LOD0)

The finish's voxel remesh is the union of the two capped halves, so where the front half's skin stands proud of the back
half's, the front's cap plane shows as a straight vertical ledge down each flank and a square notch where a leg is cut at
the ground (two blind critics on the 2026-10-24 maw). A texture fix cannot remove that. Smooth the LOD0 at the cut, then
re-run the seam fix on the moved surface and rig as before:

```
blender -b <finish>/<Name>_finish.blend --python-exit-code 1 -P apps/vr/tools/mesh/seam_ledge.py -- measure <before>/ledge.json
blender -b <finish>/<Name>_finish.blend --python-exit-code 1 -P apps/vr/tools/mesh/seam_ledge.py -- smooth <fix>/finish/<Name>_finish.blend <fix>/smooth.json
blender -b <fix>/finish/<Name>_finish.blend -P apps/vr/tools/mesh/export_glb.py -- <fix>/finish/<Name>_finish.glb
node apps/vr/tools/mesh/mesh-card.mjs <fix>/finish/<Name>_finish.glb --stage finished --class <Class> --out <fix>/card-finished.json
python apps/vr/tools/mesh/seam_fix.py <fix>/finish/<Name>_finish.glb <the unfixed bake> <fix>/uvfix/T_<Name>_BaseColor.png --source <the source-id png> --record <fix>/uvfix/seam_fix.json
blender -b <fix>/finish/<Name>_finish.blend --python-exit-code 1 -P apps/vr/tools/mesh/seam_ledge.py -- measure <fix>/ledge.json
```

- `smooth` runs Taubin lambda/mu pairs of the umbrella operator on `SM_<Name>`'s vertices, each scaled by a band weight
  around the cut (default centre y -0.32, full within 0.12 m, zero past 0.24 m, 120 pairs) with the top 3 cm pinned, so
  the height holds. Triangles and UVs do not change: the card stays one watertight component at the same triangle count.
  The record counts the faces whose normal turned past 90 degrees (folds).
- Re-run `seam_fix.py` from the unfixed bake with the same source-id PNG (it is UV-tied, so it still fits): its anchors and
  report read texel positions, which moved. Carrying the old fixed texture onto the moved surface measured 0.015 on the
  maw's right flank band, over the 0.012 of record; the re-run measured 0.011.
- `measure` casts rays along -X and +X every 2 mm (500 px/m, the critic renders' scale) and reports per flank the tallest
  line of 1 cm depth jumps in the band (default the cut -0.30 +- 0.15 m) against the tallest outside it, the same for 45
  degree normal turns, the outline steps and the ground notch, with the depth maps in `<out>.npz`.
- `python -m unittest apps/vr/tools/mesh/test_ledge_lib.py` (numpy, scipy for one test). First run:
  docs/research/MAW-SEAM-LEDGE-T70-2026-10-26.md.

## Starting and stopping the local ComfyUI

The install is outside every repo (`C:/Users/kazda/ai-tools/`, recorded with sources, licences and sha256 in
`C:/Users/kazda/ai-tools/MODELS.md`): ComfyUI 0.42.1 (tag v0.42.1) with its own Python 3.12 venv, torch 2.14.1+cu126, and
the five weights `trellis-api.json` loads (`trellis_2_int8_convrot`, `trellis_2_shape_vae_bf16`,
`trellis_2_texture_vae_bf16`, `dino_v3_L_naf_fp32`, `birefnet`). `MAGEVR_COMFY_DIR` points elsewhere.

- `comfy.mjs start` refuses if anything already answers on 127.0.0.1:8188, starts `main.py` detached, records its pid in
  `<install>/magevr-comfy.pid`, logs to `<install>/user/magevr-comfy.log`, and waits until `/system_stats` answers (72 s
  cold on Wolf).
- `comfy.mjs stop` kills that pid's process tree only, after checking it is still that install's `main.py`. Run it when
  the GPU work is done. It never touches Unreal or anything it did not start.
- `comfy.mjs status` prints the guard's view (RAM, GPU, queue, Unreal processes seen).

Rebuild of the install (PowerShell or Git Bash, in `C:/Users/kazda/ai-tools`):
`git clone --depth 1 --branch v0.42.1 https://github.com/comfyanonymous/ComfyUI.git`, then in `ComfyUI`:
`uv venv --python 3.12 venv`, `uv pip install --python venv/Scripts/python.exe torch==2.14.1 torchvision --index-url
https://download.pytorch.org/whl/cu126`, `uv pip install --python venv/Scripts/python.exe -r requirements.txt`, and the
five files from the URLs in MODELS.md into `models/{diffusion_models,vae,clip_vision,background_removal}/`.

## Proof run (2026-10-24, Wolf, RTX 4090, no Unreal running)

Input: a copy of `apps/vr/art/characters/brennic/concept-front-s3801-medium.jpg` (sha256 34b43291de...) in
`C:/Users/kazda/ai-tools/runs/proof/`. Every output stayed there.

| Step | Seconds | Result |
|---|---|---|
| 1 input gate | 1.8 | `incomplete`, no findings. 1 subject; background flat sRGB 0.816 grey (100 % of the border near it, side spread 0); margins 5.5 / 5.2 / 5.5 / 6.1 % (left, top, right, bottom); subject long side 1,220 px; the four see-only criteria unmeasured |
| 2 TRELLIS.2 | 195 | `trellis.mjs` unchanged, seed 42 on all four samplers, DecimateMesh 700,000; status success; 56,252,116-byte GLB. VRAM peak 12,388 MiB of 24,564 (baseline 2,092, so +10,296 MiB). Guard before it: free RAM 21.4 GB, GPU 19 %, 2,093 MiB VRAM held by other apps, queue empty (Lynx, ARM64: 240 s) |
| 3 raw card | 4.4 | `warn`, score 10: 694,923 tris, 334,794 verts, 40 components = 1 part + 39 floaters holding 0.41 % of the faces; not watertight (1,552 boundary, 10,147 non-manifold edges); 494 inconsistently wound edges; 0 degenerate faces; height 1.001 m, min z -0.501 m. Codes `floaters`, `not-watertight`, `winding-inconsistent`, `tris-over-budget`, `scale-off`, `origin-off`, all remedy `finish` |
| 4 finishing | 22.7 + 7.6 export | finish.py (Brennic palette, voxel 0.01 m, target 24,000, bake 4096 in 13.2 s): 116 floaters dropped at import (finish.py's own weld counts 117 components), voxel remesh 118,132 tris, collapse to 23,988, 2 more floaters dropped, six flat-colour materials |
| 5 finished card | 1.1 | **`pass`, score 100**: 23,988 tris (Humanoid budget 18,000-30,000), 11,984 verts, 1 component, 0 floaters, watertight, winding consistent, 0 degenerate faces; **height 1.799 m** (band 1.62-1.98 m), min z 0.00003 m, bbox centre xy (-0.0001, 0.0002) m, extent 1.004 x 0.420 x 1.799 m |

Before and after: 694,923 -> 23,988 triangles; 1.001 -> 1.799 m tall; 40 -> 1 component.

The card is structural. A Workbench look at the finished GLB (scratch, not kept) showed the A-pose figure with tunic,
cloak, belt, wraps and skin in their palette colours; that look is a person's check, not the card's.

Also proven on real pixels: the turnaround sheet `concept-turnaround-s3802-medium.jpg` fails the single-image gate
(`multiple-subjects`, 3 figures, plus `margin-tight` at 1.75 %); its three figures cut into front / side / back pass the
set checks (back view height -0.55 % and feet line -0.71 % against the master) and land `incomplete`; leaving out the
side view fails `view-missing`.

## Known gaps

- torch cu126 is what the brief asked for (CUDA 12.x), but ComfyUI 0.42.1 turns comfy-kitchen's CUDA backend off below
  cu130 (it logs "You need pytorch with cu130 or higher to use optimized CUDA operations"), so remesh and decimate take
  their scipy / CPU fallbacks. The run still took 195 s. Lynx ran cu134, so the raw meshes are not bit-comparable
  between the hosts (raw by finish.py's weld: Lynx 695,551 tris and 151 components, Wolf 694,873 tris and 117 components).
- `apps/vr/tools/assets/preview.py` and `render_evidence.py` do not run on Blender 4.2 as they are (they set
  `BLENDER_EEVEE`, which 4.2 names `BLENDER_EEVEE_NEXT`, and Blender exits 0 after the traceback unless given
  `--python-exit-code`); `finish.py` and `rig_anim.py` do. Run the evidence through `evidence.mjs`:
  `node apps/vr/tools/mesh/evidence.mjs <in.fbx> <outdir> <Name> <outdir>/<Name>_rig.json Cast`.
- `node --test apps/vr/tools/mesh/` (a directory) fails to load on Node 24; name the files:
  `node --test apps/vr/tools/mesh/*.test.mjs`. The Python half: `python -m unittest apps/vr/tools/assets/test_finish.py`
  (numpy only).
- The see-only input criteria and the card's shape quality need a judge that can see; until pof has one, a character
  input is at best `incomplete`.
