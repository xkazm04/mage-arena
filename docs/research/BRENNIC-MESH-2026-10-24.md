---
title: Brennic image-to-mesh run from the accepted front + back views (VISUAL-PLAN (d), the mesh lane)
channel: vr
date: 2026-10-24
status: RAN - input gate incomplete (ungated, no findings); 4 TRELLIS.2 seeds, all raw cards finish-only; seed 7 finished to a structural PASS (23,990 tris, 1 component, watertight, 1.799 m); critic verdict REWORK (face and accessories lost in finishing); rigged with all four clips; evidence rendered; ComfyUI stopped. Nothing ships until the owner rules on DINOv3.
sources: [apps/vr/tools/mesh/README.md, docs/research/BRENNIC-MV-MASTER-2026-10-24.md, docs/concepts/VISUAL-PLAN.md (b) (c) (d), apps/vr/tools/assets/trellis-api.json, apps/vr/tools/assets/finish.py, apps/vr/tools/assets/rig_anim.py, apps/vr/tools/assets/render_evidence.py]
---

# Brennic image-to-mesh, 2026-10-24

**Outcome.** Brennic went from the two accepted views to a finished, rigged and animated LOD0 on Wolf (RTX 4090), with
no Unreal process started by this lane. **The structural cards pass, and that is not a quality claim:** the critic
verdict is **rework**. The reconstruction is faithful: the raw TRELLIS.2 mesh carries the face, the brooch, the collar
and the sandal straps in its texture. Finishing then throws them away, because `finish.py` gives each face one of six
flat palette colours. The rework belongs in finishing, not in the input or the seed.

- **Total wall time:** 19 min 32 s, from 16:53:59Z (sha256 check) to 17:13:31Z (the last evidence PNG). The record
  was written after that.
- **Outputs** are all in `C:/Users/kazda/mage-art-staging/brennic-mesh-2026-10-24/` (`<out>/` below). No GLB, FBX,
  blend or PNG is in git.
- **Licence hold: nothing from this run ships until the owner rules on the DINOv3 weight licence** (see the last
  section). Local build and evaluation only.

## Inputs (Athena's pick, 2026-10-24)

Both files were copied into `<out>/` from `C:/Users/kazda/mage-art-staging/brennic-mv-2026-10-24/`. Before the run,
each was checked against the `.sha256` file beside it, and both matched:

| View | File | sha256 |
|---|---|---|
| front (the reconstruction input) | `front-s6101-medium.jpg` | `94d95257497af4ae8dd737311f1e91915466585c9536e5415f6e425f4ed181cf` |
| back (the critic's reference only) | `back-s6103-medium.jpg` | `d0fb9ac74cca0763f369f66f442b80b590ee307ea08951f0eebf9d6215c09ab6` |

`trellis.mjs` takes one image, so TRELLIS.2 saw the front only. No multi-view workflow was built.

## Step 1: input gate

```
node apps/vr/tools/mesh/input-gate.mjs <out>/front-s6101-medium.jpg --out <out>/1-input-gate-front.json
node apps/vr/tools/mesh/input-gate.mjs --front <out>/front-s6101-medium.jpg --back <out>/back-s6103-medium.jpg \
     --require front,back --out <out>/1-input-gate-set.json
```

| Run | Verdict | Exit | Findings | Seconds |
|---|---|---|---|---|
| front alone | `incomplete` | 3 | none | 0.9 |
| front + back set (`--require front,back`; no side, by Athena's ruling) | `incomplete` | 3 | none | 2.0 |

Measured on both views:

- **Subjects:** 1 subject, 1 component. The foreground share is 0.379 (front) and 0.367 (back).
- **Background:** opaque flat grey, sRGB 0.8275. 100 % of the border is near it, and the side spread is 0.
- **Margins (left / top / right / bottom):**
  - front: 5.08 / 5.02 / 5.47 / 5.90 %;
  - back: 4.69 / 5.24 / 5.47 / 6.55 %.
- **Subject long side:** 1,226 px (front) and 1,214 px (back).
- **Set consistency:** back height -0.98 % against the master, feet line -0.65 %, background delta 0.

**Ungated lines, verbatim:**

- front: "not passed: pose-canonicality, silhouette-clarity, lighting-neutrality, detail-focus unmeasured. Generating
  from this input records it as ungated on those criteria."
- set: the same, plus `view-feature-correspondence` and `view-lighting-consistency`.

Exit 3 (`incomplete`, only see-only criteria unmeasured) is the README's proceed-as-ungated case, so the lane went on.
Those criteria were judged by hand in BRENNIC-MV-MASTER (front 8, back 8, the pair passing all five set checks). That
is a person's reading, not the gate's.

## Step 2: ComfyUI

`node apps/vr/tools/mesh/comfy.mjs start` brought ComfyUI 0.42.1 up as pid 53080 after 27 s (torch 2.14.1+cu126,
cudaMallocAsync). The guard cleared on its first look before every seed, so it never had to wait. An
`UnrealEditor-Cmd.exe` belonging to the T55 lane was running at times (tasklist at 16:53 and 17:10). The guard reported
it and nothing touched it.

## Step 3: TRELLIS.2, four seeds, front only

`node apps/vr/tools/mesh/comfy.mjs run <out>/front-s6101-medium.jpg brennic-s<seed> <seed> <out>/s<seed>` used
`trellis.mjs` unchanged: DecimateMesh 700,000, and the seed goes to all four samplers. Each raw card came from
`node apps/vr/tools/mesh/mesh-card.mjs <glb> --stage raw --class Humanoid --out <out>/s<seed>/3-raw-card.json`.

| Seed | TRELLIS s | VRAM peak (baseline, +delta) MiB | GLB bytes | Raw verdict | Tris | Components | Parts / specks (speck face share) | Boundary / non-manifold / mis-wound edges | Height m |
|---|---|---|---|---|---|---|---|---|---|
| 42 | 180 | 12,477 (1,877, +10,600) | 53,321,040 | warn, score 10 | 690,310 | 191 | 5 / 186 (3.28 %) | 2,343 / 12,762 / 684 | 0.99996 |
| **7** | **151** | 13,786 (4,314, +9,472) | 49,552,956 | **warn, score 10** | 697,468 | **14** | **1 / 13 (0.26 %)** | **844 / 4,392 / 319** | 0.99778 |
| 1337 | 196 | 14,076 (4,378, +9,698) | 55,591,120 | warn, score 10 | 692,640 | 75 | 1 / 74 (0.56 %) | 1,525 / 13,232 / 422 | 0.99281 |
| 2026 | 165 | 14,012 (4,316, +9,696) | 52,461,220 | warn, score 10 | 697,013 | 30 | 2 / 28 (0.21 %) | 1,003 / 7,334 / 319 | 0.99398 |

- **Codes.** Every card has the same six codes: `floaters`, `not-watertight`, `winding-inconsistent`,
  `tris-over-budget`, `scale-off` and `origin-off`. All six are remedy `finish`. No `reroll` or `neither` code appeared.
- **Per-card numbers.** No degenerate faces on any card. Raw card time was 2.7-3.6 s.
- **Guard before each seed:**
  - free RAM 28.8 / 26.3 / 22.4 / 21.8 GB;
  - GPU utilisation 9 / 11 / 6 / 6 %;
  - queue 0.
- **Pick: seed 7.**
  - The score ties at 10, so the structure breaks the tie: seed 7 is the only seed with one part and fewest specks.
  - It has the fewest boundary and non-manifold edges.
  - It ties seed 2026 for the fewest mis-wound edges.
  - Seed 42's five substantial parts (its largest is only 585,721 of 690,310 faces) would have been the weakest input
    for a one-part finish.
- **Seed 7 raw GLB:** sha256 `bef2ed78d4d34ccaed2f5544bc95d551fbf843725f6f53d6243c710708e00698`.

After the fourth seed, `comfy.mjs free` (HTTP 200) and `comfy.mjs stop` ran at 17:07:47Z: pid 53080 and its two
children were terminated. `netstat` then showed no listener on 8188, only TIME_WAIT sockets. The python processes left
were Athena's, not ComfyUI's.

## Step 4: finishing

`node apps/vr/tools/mesh/finish.mjs <out>/s7/brennic-s7.glb <out>/finish --class Humanoid --name Brennic` ran in 31.7 s
wall:

- **Timings:** raw card 3.5 s, `finish.py` 20.1 s, export 6.7 s, finished card 1.3 s.
- **finish.py, step by step:**
  - 57 floaters dropped at import (its weld counts 58 components);
  - voxel 0.01 m remesh to 121,100 tris;
  - collapse to 23,990 tris;
  - 1 more floater dropped;
  - 0 non-manifold edges;
  - 4096 bake in 10.8 s.
- **Faces per material:**

| Material | Faces |
|---|---|
| Tunic | 3,737 |
| Cloak | 5,947 |
| Leather | 8,869 |
| Iron | 312 |
| Skin | 4,879 |
| Hair | 246 |

**Finished card (`Brennic_card_finished.json`): PASS, score 100, no findings.**

| Measure | Value |
|---|---|
| Triangles | **23,990** (Humanoid band 18,000-30,000, target 24,000) |
| Vertices | 11,987 |
| Components | **1** (1 part, 0 specks) |
| Watertight | **yes** (0 boundary, 0 non-manifold edges) |
| Mis-wound edges / degenerate faces | 0 / 0 |
| Height | **1.79923 m** (band 1.62-1.98 m) |
| Min z / bbox centre xy | 0.00027 m / (-0.00054, -0.00013) m |
| Extent | 1.0267 x 0.3847 x 1.7992 m |

Before and after: 697,468 -> 23,990 triangles, 0.998 -> 1.799 m tall, 14 -> 1 component. The finished GLB has sha256
`7c6fae2a419879046a60d21b7a8d421b95071a0e255520f942f5ee67756101b9` and is 896,556 bytes.

**This card is structural. It says the geometry is well-formed and inside the class budget, and it says nothing about
whether the result looks like Brennic.** That is the next section.

## Step 5: fixed-camera renders and the perceptual critic

**How the renders were made.** A scratch Blender 4.2 script (`<out>/scripts/turn.py`, not in the repo) imported each
GLB and rendered it with EEVEE Next:

- the same orthographic camera for every view (ortho scale 2.1, 6 m out, aimed at the bbox centre);
- 8 views every 45 degrees: front, front-left 3/4, left, back-left 3/4, back, back-right 3/4, right, front-right 3/4;
- a soft grey world and three suns.

It rendered the finished mesh (`<out>/renders/finished/`) and the raw seed-7 mesh with its TRELLIS texture
(`<out>/renders/raw-s7/`). Comparison sheets are in `<out>/renders/`:

- `cmp-front-back.png`: master front | finished | raw | master back | finished | raw;
- `cmp-head.png`: master head | finished front head | finished back head;
- `raw-head.png`;
- `fin-8views.png`.

After rigging, `render_evidence.py`'s own 8-view turnaround (`<out>/evidence/Brennic_turnaround.png`) shows the same
thing.

The critic read the renders beside front s6101, back s6103 and the VISUAL-PLAN (c) style block. The critic is this
session, not the generator, so it is still one model's reading. It drifts or matches as follows:

| Feature | Master (s6101 / s6103) | Finished mesh | Match? |
|---|---|---|---|
| **Face** | Clear eyes, heavy brows, nose, mouth at rest, full dark beard | A skin-coloured patch with a few dark fragments where the eyes and brows were. Beard and moustache are mid-brown leather with dark specks; no eye, brow or mouth reads | **Drift (fail)**: the style block's "readable faces (clear eyes, brows and mouth at rest)" is not met. The **raw** mesh has the face: eyes, brows, beard and moustache all read in `raw-head.png` |
| **Hair** | Dark curly cap | A smooth mid-brown cap in the Leather colour, plus a dark Hair-coloured blotch at the nape; the curls are gone | **Drift**: the Hair material got only 246 faces, so hair and beard were classified as Leather |
| **Tunic** | Muted brick-red, short sleeves, knee length | Brick-red, short sleeves, knee length, right shape and colour | **Match** in shape and palette, but the sleeve, hem and neckline edges are saw-toothed (frayed skin-coloured and brown teeth) |
| **Belt** | Mid-brown leather belt with a square bronze buckle | A leather band at the waist; the buckle is an iron-grey square with dark holes | **Partial**: the belt reads; the buckle is the wrong colour (iron, not bronze) |
| **Bracers** | Stacked leather bands on the forearms | Plain brown sleeves on the forearms with frayed, saw-toothed ends | **Partial**: position and colour match; the stacked bands and their ridges are lost |
| **Leg wraps and sandals** | Banded shin wraps; open sandals with toe straps | Plain tall leather "boots"; the sandal straps are lost, with skin-coloured blotches on the feet and a frayed ankle line | **Drift**: same defect class as VISUAL-PLAN (b) item 2 |
| **Collar** | Heavy iron collar with a front plate | A flat grey band merged into the cloak's neckline, with no plate | **Drift**: reads as a thin grey ring, as before. The raw mesh has the collar and its sheen |
| **Brooch** | Round bronze brooch at the throat | A skin-coloured teardrop shape at the throat | **Drift**: classified as Skin. The raw mesh shows the round bronze brooch |
| **Cloak and hood** | Charcoal-brown cloak to the back of the knees. The back view adds a hood-shaped gather, which the MV record ruled is the cloak bunched at the neck (front wins) | Charcoal cloak to the back of the knees, with no hood. From behind it flares wide at hip height into a rounded, bell-like outline | **Mostly a match**: length, colour and drape match. The back silhouette is broader and rounder than the master's straight-sided fall. No hood, which agrees with the front-wins ruling |
| **Palette** | Muted earth reds, charcoal browns, mid-brown leather, warm tan skin, iron grey | The same six flat colours, one per material, matte, no outlines | **Match** for the Characters role and the finish slot ("one flat colour per material"). The block edges, though, are ragged rather than clean |
| **Proportions** | Chunky heroic build, big hands, A-pose | Same build, broad shoulders, big mitten hands, same A-pose arm angle (rig fit: arms 57.2 degrees down) | **Match**: TRELLIS reproduced the silhouette and proportions faithfully from the one front view |

### Verdict: REWORK

**Reasons.**

1. **The identity-bearing surface is lost.** The face does not read, and hair and beard are the wrong colour. The style
   block requires readable faces, and character-identity-continuity makes the face the identity carrier.
2. **Four named accessories regress:** brooch, collar plate, banded bracers and wraps, and sandal straps.
3. **Every colour boundary is saw-toothed.** This is the rejection-list item "3D: saw-toothed colour regions from
   per-face colour assignment", whose rule reads "finishing must bake, not classify per face".

These are the same defects VISUAL-PLAN (b) recorded for the T38 export. **The new input did not fix them, because they
are made in finishing.**

**Why not reject.** The raw seed-7 reconstruction is a good match to the master. Silhouette, proportions, tunic, cloak
length, collar, brooch, face and sandals are all there in the raw texture (`raw-head.png`, `cmp-front-back.png`). The
geometry after finishing still matches the silhouette, and the structural card passes. The input and the seed are
worth keeping. What needs rework is the colour step.

**The rework is outside this lane's paths.** `apps/vr/tools/assets/finish.py` already bakes the TRELLIS texture to 4096
(`<out>/finish/Brennic_bake_basecolor.png`). It uses that bake only to classify faces, then replaces it with six flat
colours. Its Hair and Leather REFERENCE colours were also measured on the 2026-10-21 seed-42 bake of the *old* master,
which is why the new input's hair landed in Leather.

The structural pass above is **not** a quality claim. This verdict is the quality reading, and it is rework.

## Step 6: rig, clips and evidence

**Rig** (2.1 s inside Blender, 6.5 s wall). It ran as
`blender -b -P apps/vr/tools/assets/rig_anim.py -- <out>/finish/Brennic_finish.blend <out>/rig Brennic
<out>/rig/Brennic.fbx Cast` on Blender 4.2.1, unchanged, and exited 0. `Brennic_rig.json`:

- **Bones:** 23. Height 1.7992 m. Arms 57.2 degrees down on both sides.
- **Weights:**
  - proxy heat on a 42,752-tri voxel proxy;
  - 0 zero-weight proxy vertices;
  - 0 nearest-bone fallback vertices;
  - 27,246 vertex weights limited to 4.
- **Clips (frames):**

| Clip | Frames | Keys at frame |
|---|---|---|
| Idle | 61 | 0, 30, 60 |
| Cast | 47 | 0, 9, 17, 23, 31, 46 |
| Hit | 25 | 0, 4, 10, 24 |
| Death | 61 | 0, 6, 20, 34, 48, 60 |

- **Outputs:** `<out>/rig/Brennic.fbx` (1,905,100 bytes, sha256
  `f42b861893f46f2c478b7a82c6338b0151af4784b5aafd0dcbbd0cdde2e039b0`) and `Brennic_rigged.blend`.

**Evidence: `render_evidence.py` failed as it is on Blender 4.2, and was fixed inside `apps/vr/tools/mesh/`.**

- **The failure.** The first run died at line 21:
  `TypeError: ... enum "BLENDER_EEVEE" not found in ('BLENDER_EEVEE_NEXT', 'BLENDER_WORKBENCH', 'CYCLES')`. Blender
  4.2 renamed the engine id, and Blender still **exited 0**, because the script was run without
  `--python-exit-code`.
- **The fix.** A scratch copy with only that id changed rendered all six PNGs, so the id is the only 4.2
  incompatibility. Commit `4e7424a` (`fix(mesh)`) adds two files:
  - `apps/vr/tools/mesh/evidence.mjs` runs the unchanged script from a temp copy, adapted by `blender-compat.mjs` (the
    id is picked from `bpy.app.version`, and line numbers are kept). It runs with `--python-exit-code 1` and fails on a
    missing PNG.
  - `apps/vr/tools/mesh/blender-compat.test.mjs` tests it offline against the real script.
- **The rerun.** `node apps/vr/tools/mesh/evidence.mjs <out>/rig/Brennic.fbx <out>/evidence Brennic
  <out>/rig/Brennic_rig.json Cast` exited 0 in 6.7 s and wrote:
  - `Brennic_turnaround.png`;
  - `Brennic_game_640x480.png`;
  - `Brennic_clip_{Idle,Cast,Hit,Death}.png`.

What the evidence shows, read by eye:

- **Cast.** Key 2 opens both arms outward from the body, the fault `SOURCE.md` already admitted for T38. Keys 3-5 raise
  one hand and thrust the other forward, then settle back to Idle.
- **Hit.** The head snaps back and one arm swings high above the shoulder, then recovers.
- **Death.** He stumbles, kneels and bows forward. The cloak folds over his back (also admitted before). No visible
  tearing or interpenetration at strip scale.
- **Hands.** Rigid mittens: no finger bones.
- **Game view (640x480, eye 1.2 m, rival 6 m).** He stands about 100 px tall. Silhouette, tunic red and cloak read; the
  face and accessories do not, at this size, whatever finishing does.

## Step 7: ComfyUI stopped

`comfy.mjs stop` ran at 17:07:47Z, straight after the last GPU job (finishing, rig and evidence are Blender only). It
terminated pid 53080 and its child processes 2624 and 35512. `netstat -ano | grep :8188 | grep LISTEN` printed nothing,
so port 8188 is free.

## torch cu130

**Not done.** TRELLIS.2 took 151-196 s per seed on cu126, with the remesh, decimate and unwrap on their CPU fallbacks.
That is in line with the proof run's 195 s, and it was not a bottleneck: the four seeds took 692 s (11.5 min) of the
19.5-min lane.

The approval needs a measured gain. Measuring it means rebuilding the venv and re-running seeds, which risks the
working install in the middle of a lane, so there are no before and after timings to report.

## DINOv3: licence status

**The TRELLIS.2 workflow loads DINOv3 weights.** `apps/vr/tools/assets/trellis-api.json` has a `CLIPVisionLoader` node
with `clip_name: dino_v3_L_naf_fp32.safetensors`. It was loaded for every seed of this run.

The DINOv3 weight licence is an open OWNER question. **Nothing from this run ships until the owner rules:** not the
GLB, not the FBX, not the renders. Local build and evaluation, as done here, is fine.

## Outputs (all under `C:/Users/kazda/mage-art-staging/brennic-mesh-2026-10-24/`)

| Path | What |
|---|---|
| `front-s6101-medium.jpg`, `back-s6103-medium.jpg` | input copies (sha256 above) |
| `1-input-gate-front.json`, `1-input-gate-set.json` | the input-gate verdicts |
| `s{42,7,1337,2026}/brennic-s*.glb`, `gpu-run.json`, `trellis-runs.jsonl`, `3-raw-card.json` | the four raw meshes, their GPU records and raw cards |
| `finish/Brennic_finish.{glb,blend,json}`, `Brennic_card_raw.json`, `Brennic_card_finished.json`, `Brennic_finish_record.json`, `Brennic_bake_basecolor.png` | finishing and both cards |
| `renders/finished/`, `renders/raw-s7/`, `renders/cmp-*.png`, `renders/*-8views.png`, `renders/raw-head.png` | the fixed-camera critic renders |
| `rig/Brennic.fbx`, `rig/Brennic_rigged.blend`, `rig/Brennic_rig.json`, `rig/rig_anim.log` | rig and clips |
| `evidence/Brennic_turnaround.png`, `Brennic_game_640x480.png`, `Brennic_clip_*.png`, `render_evidence.log` | T38 evidence (the log holds the first, failed 4.2 run) |
| `scripts/turn.py`, `scripts/sheet.py` | the scratch render and contact-sheet scripts |

## Not done here (later lanes)

- **The UE import.** T55 holds the one Unreal slot.
- **Finishing that keeps the bake.** For example: the 4096 base-colour bake as the material's texture, or the bake
  plus flat palette blocks only where the bake is uniform. It also needs the new input's REFERENCE colours. Both live
  in `apps/vr/tools/assets/finish.py`, outside this lane's paths. See the questions in the run's result.

## Test note

`node --test apps/vr/tools/mesh/` with a directory argument fails to load on Node 24.14 ("Cannot find module
...\mesh"). The form is the problem, not the tests: it fails the same way without this lane's files. The files form,
`node --test apps/vr/tools/mesh/*.test.mjs`, passes 25 of 25 (gate-lib 20, blender-compat 5). The mesh README now says
so.
