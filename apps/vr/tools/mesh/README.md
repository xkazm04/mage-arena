# Mesh lane: image to engine-ready mesh, gated at every step

Scripts that take one concept image to a finished LOD0 mesh on the x64 host (RTX 4090), with a mechanical gate before
and after each expensive step. No generated asset enters git here: inputs are copied, and every output goes outside
the repo (the proof below used `C:/Users/kazda/ai-tools/runs/proof/`). Node scripts use built-ins only; Blender 4.2
LTS headless does the measuring (set `BLENDER` to use another `blender.exe`).

| File | Role |
|---|---|
| `input-gate.mjs` + `image_stats.py` | gate 1: can this image (or view set) be reconstructed? Verdict JSON with defect codes |
| `comfy.mjs` | start / guard / run / free / stop the local ComfyUI; `run` hands the job to `../assets/trellis.mjs` unchanged |
| `mesh-card.mjs` + `mesh_metrics.py` | gates 2 and 3: the structural card of a GLB, FBX or .blend |
| `finish.mjs` + `export_glb.py` | raw card, `../assets/finish.py` (runs on Blender 4.2.1), export, finished card. `--mode flat` (default): one flat palette colour per material; `--mode bake`: the TRELLIS albedo baked onto LOD0 as `T_<Name>_BaseColor.png` (see "Finish modes") |
| `finish-args.mjs` + `finish-args.test.mjs` | the finish.py argument list finish.mjs builds (flat stays the pre-bake list exactly) |
| `finish_lib.py` (+ `../assets/test_finish.py`) | finish.py's Blender-free logic: mode and option parsing, sRGB/Lab, the style clamp, UV coverage, bake-miss accounting and filling (`python -m unittest apps/vr/tools/assets/test_finish.py`) |
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

- **`--mode flat`** (the default, the T38 recipe): every face takes the nearest palette colour of its baked texels. Six
  flat materials, no image. It loses everything smaller than a colour region (Brennic 2026-10-24: face, brooch, collar
  plate, straps; saw-toothed region edges). Kept for comparison and for assets that really are flat blocks.
- **`--mode bake`**: the UV islands are repacked (`pack_islands`, concave, margin 0.003), the TRELLIS albedo is baked
  selected-to-active (Cycles DIFFUSE, colour pass only, metallic zeroed first) at `budgets.finish.bakeTextureRes`
  (2048), cage extrusion 0.02 m, max ray distance 0.08 m, margin 16 px EXTEND. A second bake of the source in white
  emission (no margin) is the **hit mask**: an island texel no ray reached reads 0. Misses are counted
  (`bake.missed`, `missed_interior`), filled from their neighbours, and every island texel is checked for black
  (`dark_in_islands_after_fill`, must be 0). One material `M_<Name>` with `T_<Name>_BaseColor.png`; the GLB embeds it.
  `--clamp posterize|hue|both` (`--strength` <= 0.6, `--levels` >= 4) pulls the texture part of the way toward the
  palette set plus the VISUAL-PLAN (c) accents (bronze, gold, crimson, iron); it never replaces it (`clamp.detail_kept`).
- **Rigging a bake finish:** `rig_anim.py` exports the FBX with `path_mode='STRIP'`, so copy `T_<Name>_BaseColor.png`
  next to the FBX before `evidence.mjs` or a UE import.

Measured on Brennic seed 7 (docs/research/BRENNIC-MESH-REWORK-2026-10-24.md): finish.py 34 s at 2048, 0.64 % of island
texels missed (mostly the inner cloak and between the legs), 0 black texels after the fill.

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
