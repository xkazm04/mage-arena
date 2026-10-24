---
title: The creature cast in UE - the maw's flank seam matched on the bake, the maw imported beside Brennic and the hound, a soft crack bloom for the hound at 8 m, the whole cast and the D-G4 budget
channel: vr
date: 2026-10-24
status: RAN - Phase A (Blender only) matched and feathered the maw's flank seam on the finished bake in UV space, with the geometry byte-identical (the 17,490-tri finish d4330d53 is not re-run); the measured colour step at the seam fell from up to 0.038 to 0.012 or less per channel on both flanks, but two fresh blind critics read the seam as IMPROVED BUT VISIBLE, because the geometric ledge at the cut (no re-mesh allowed) now carries it. Phase B (one Unreal lane, Blender and ComfyUI down) imported the maw beside Brennic and the hound, chose a soft crack bloom for the hound's glow at 8 m (cracks stay orange, 0 yellowed pixels), took 17 stills and re-ran D-G4 with all three: 134 / 137 of 150 draw calls, 191k of 350k BasePass. Nothing imported is committed (DINOv3 hold).
sources: [docs/DECISIONS.md (2026-10-24, Athena 20:13Z and 20:46Z), docs/research/MAW-MV-2026-10-24.md, docs/research/CREATURE-UE-IMPORT-2026-10-24.md, docs/research/DUSK-LIGHTING-T56-2026-10-24.md, apps/vr/tools/mesh/README.md]
---

# The creature cast in UE, 2026-10-24

**Label: desktop proxy (RTX 4090, D3D12, forward shading, `-nohmd`), not Quest truth.** Engine UE 5.8.3 (CL 58210709,
`Build.version` read before the first build). No combat number moved: nothing under `apps/vr/data/` was touched, and the
dusk is T56's preset applied in code.

**Outcome.**

- **Phase A, the maw's flank seam.** The colour step is matched and feathered on the baked texture, in UV space.
  - The preferred route (match the s7301 back texture in the overlap band, then re-merge and re-finish) ran first. It
    kept the geometry bit-identical but overshot, so it was not kept (section 1.3).
  - The **flank bands either side of the seam now differ by 0.012 or less per channel** (before: up to 0.038). The
    2 cm patch pairs at the seam differ by 0.007 or less systematically.
  - Two fresh blind critics read it as **IMPROVED BUT VISIBLE**. The colour step is smaller, but the **geometric ledge
    and the ground notch at the cut** still show the seam. A texture cannot remove those, and re-meshing was out of
    scope.
- **Phase B, one Unreal lane.**
  - The maw imports cleanly: 11 bones, 17,752 tris, 1.2509 m, 2 slots (the tongue capsule flat dark red), four clips.
    It stands 4.0 m left at 8 m beside Brennic (1.4 m left) and the hound (1.4 m right).
  - **The hound's glow:** a soft crack bloom, the key averaged over a ring of taps, lighting only the hide *around* a
    crack.
    - It lifts the glowing share of the hound's pixels from 17.9 % to 27.4 % (front) and from 16.9 % to 25.3 %
      (three-quarter).
    - The crack cores keep the base orange (255, 170, 68).
    - It yellows no pixel. Raising `EmberIntensity` instead turned the cores yellow (255, 219, 86).
  - **Budget with all three:** **134 / 137 draw calls of 150** (median / max; +25 on the run's baseline, under the 145
    flag) and **191k of 350k BasePass** (+53.8k, the three LOD0s exactly).

Code: `3d39d96` and `defefa6` (Phase A mesh tools), `a93611c` (DECISIONS), `9250cfd` (Phase B: spec, material, capture
driver, script, test). The stills, budget and suite runs came from the build of the `9250cfd` source before it was
committed. The commit changed nothing in it.

## Phase A: the maw's flank seam (Blender only)

Wall time: 20:50Z (input sha256 check) to 21:27Z (the last Blender process gone), 37 min including the code and tests.
**No ComfyUI, no TRELLIS, no re-mesh, no Unreal process.** `tasklist` showed no Unreal and no listener on 8188 at the start.

Outputs are in `C:/Users/kazda/mage-art-staging/maw-seam-2026-10-24/` (`<seam>/` below).

### 1.1 Inputs (`<seam>/inputs.sha256`)

| File | sha256 |
|---|---|
| front `creature-mesh-2026-10-24/maw/s7/maw-s7.glb` | `f6fd79988c0c2190ba1df8f3c3736b25b173114598359d537041f6d7fde89f23` |
| back `maw-mv-2026-10-24/back-only/maw-back-s7.glb` | `ac2c926e2639dbdcc5e4163880660a15c916e4b612e9ebed8bba8b8b19f45c24` |
| merge `maw-mv-2026-10-24/merge/try2/maw-merged.glb` | `5571783e3b358f17a7e6a0073e7664bc1ee6cdf8165906cd45b7c601ec902546` |
| finish `maw-mv-2026-10-24/finish-bake/MireMaw_finish.glb` | `d4330d53dcdaefd317a993ce71b13caeb76b2888452490db232405564f97ce4a` |
| bake `.../finish-bake/T_MireMaw_BaseColor.png` | `a9e50b1ff0d440fcea3cbce057bd7d5cfe862e3f2dc9eab5b65039ae2faed312` |
| the 6fc79bf rig `maw-mv-2026-10-24/rig/MireMaw.fbx` | `5befa0e0a20182fd0bdface113405c99b71c847186e5a22e5bb83bfe258ba18a` |

### 1.2 The pipeline is byte-deterministic

An unchanged rerun of MAW-MV's commands (`<seam>/rerun/`) reproduced both files byte for byte:

- `merge_halves.py`, `-50 315 -0.3 0.05 1.25` → `5571783e...`;
- `finish.mjs ... --mode bake --rot 0 --palette MireMawOpenGape` → finish GLB `d4330d53...` and texture `a9e50b1f...`.

So a texture change upstream of the bake cannot move the geometry by itself. That was the test the brief set for the
preferred route.

### 1.3 The preferred route ran and overshot: the back texture matched before the merge

`merge_halves.py --match-back` (new) measures both TRELLIS base-colour textures on the faces each half has strictly
inside the overlap band, y -0.35 to -0.25. It skips the cap planes and weights each corner by its face area. It then
moves the back texture (`Material_0.001`, 4096²) onto the front's per-channel mean and spread (gain at most 1.5).

- **Band statistics** (sRGB; 30,208 front and 26,379 back faces):
  - front mean (0.334, 0.241, 0.149), std (0.089, 0.067, 0.081);
  - back mean (0.287, 0.187, 0.105), std (0.032, 0.027, 0.012).
- **Gain** 1.5 on every channel (clamped). Offset (-0.096, -0.040, -0.008).
- **Geometry unchanged.** The re-finish (`<seam>/finish-bake/`, finish GLB `8a990198...`) has positions, UVs and
  triangle indices **identical, array for array**, to `d4330d53`. The card passes: 17,490 tris, 1.25092 m.
- **It overshot.** The band's front statistics include the s7's lighter jaw, throat and belly. So the back came out
  lighter and yellower than the flank it meets.
  - Measured with the seam report of 1.4: the right flank's band went from -0.002 / -0.021 / +0.001 to
    **+0.036 / +0.026 / +0.042**, and the left flank's adjacent patches to +0.070 / +0.056 / +0.051.
  - Render: `<seam>/renders/src-route_*.png`.
- **Not kept.** The route stays in the tools as a documented option (mesh README "The flank seam").

### 1.4 The route of record: match and feather on the baked texture, in UV space

New in `apps/vr/tools/mesh/`: `bake_source_id.py`, `seam_fix.py`, `seam_lib.py` and `test_seam_lib.py`.

**1. Which half each texel came from (`bake_source_id.py`).**

- It opens the finish `.blend` (the LOD0 with its UVs) and imports the merged source. It poses the source as finish.py
  poses its HI (23 components under 0.5 % dropped, scale 1.000).
- It gives the front material grey 0.5 and the back white, and bakes EMIT selected-to-active with finish.py's cage
  (0.02) and ray (0.08), no margin.
- Its hit set equals the finish's own hit mask to **2 texels in 1,132,936**: same pose, same rays.
- **The switch is not the plane.** It sits at y -0.27 to -0.31 on the flanks: the seam texels' y has median -0.338,
  p5 -0.463, p95 -0.266. The remesh surface lies between the two skins in the overlap band. A fix keyed on y = -0.30
  alone mislabelled a strip on each flank. That was an earlier try, which left the left flank's band at -0.03.
- Misses (the 174,944 texels the finish filled from neighbours) take the side of the nearest hit texel on the surface.

**2. The fix (`seam_fix.py`, defaults).**

- **Every texel a position.** Every texel of the 2048² bake gets its surface point, from the LOD0's UV triangles
  rasterized with their positions.
- **Local means at anchors.** At 4,855 anchors along the seam (every 8th back texel within 5 cm of it), the mean colour
  is taken within 8 cm on each side.
- **Every back texel moves** from its anchors' back mean onto their front mean. The local shift fades into the
  seam-wide one over 15 cm from the seam, so a hind leg takes the seam's general shift, not a far anchor's. The spread
  is kept (gain 1).
- **The feather.** Texels within 3 cm of the seam are blended toward their 1.5 cm surface neighbourhood mean, by a
  tent weight that is full on the seam.
- **The margin** takes its nearest island colour.

**3. Command.**

```
blender -b <maw-mv>/finish-bake/MireMaw_finish.blend --python-exit-code 1 -P apps/vr/tools/mesh/bake_source_id.py -- <maw-mv>/merge/try2/maw-merged.glb <seam>/uvfix/MireMaw_source_id.png 1.25
python apps/vr/tools/mesh/seam_fix.py <maw-mv>/finish-bake/MireMaw_finish.glb <maw-mv>/finish-bake/T_MireMaw_BaseColor.png <seam>/uvfix/T_MireMaw_BaseColor.png --source <seam>/uvfix/MireMaw_source_id.png --record <seam>/uvfix/seam_fix.json
```

The source-id bake took 7.9 s; the fix 93 s. Two runs gave byte-identical output (`d46f288f...`).

**4. Iterations.**

- `uvfix-v1/`: gain 1.25, no fade. The first critic found a yellow-olive streak across mid-flank and olive spill on the
  hind leg.
- `uvfix-v3-g125f15/`: gain 1.25 with the fade.
- `uvfix-v2-g1f15/` = `uvfix/`: gain 1 with the fade, **the run of record**.

### 1.5 The number

**What is measured.** These are sRGB means, 0-1, on the texture of record. On each flank (left -X, right +X) the
report takes 2 cm patches at z 0.2-0.9 m every 10 cm, on the outer skin, on each side of the seam. "Adjacent" is 0-2 cm
from the seam (inside the feather); "out" is 4-6 cm (outside it, so the match alone).

- **Noise floor:** the same pair taken on one texture (two front patches, 0-2 and 4-6 cm out). It is 0.03-0.06 per
  channel: the warts make 2 cm patches noisy. So the *signed* mean over heights is the systematic step, and the brief's
  0.02 target is read on it and on the 5 cm bands.
- **Band:** the flank band, |x| > 0.4 m, z 0.2-0.9 m, within 5 cm of the seam, back minus front.

| Flank | Measure (back - front, R / G / B) | Before (bake `a9e50b1f`) | After (`d46f288f`) |
|---|---|---|---|
| left | band | **-0.029 / -0.038 / -0.028** | **-0.005 / -0.004 / -0.005** |
| left | patches adjacent, signed mean of 8 | +0.025 / +0.004 / +0.011 | +0.007 / +0.007 / +0.003 |
| left | patches adjacent, mean abs | 0.031 / 0.022 / 0.034 | 0.013 / 0.011 / 0.016 |
| left | patches out, signed mean | -0.004 / -0.022 / -0.019 | 0.000 / -0.003 / -0.010 |
| left | noise floor | 0.047 / 0.039 / 0.062 | 0.046 / 0.038 / 0.060 |
| right | band | -0.002 / **-0.021** / +0.001 | +0.010 / +0.008 / +0.012 |
| right | patches adjacent, signed mean of 7 | -0.017 / **-0.033** / -0.020 | -0.006 / -0.006 / -0.006 |
| right | patches adjacent, mean abs | 0.030 / 0.035 / 0.024 | 0.011 / 0.009 / 0.008 |
| right | patches out, signed mean | -0.017 / -0.032 / -0.009 | +0.010 / +0.006 / +0.014 |
| right | noise floor | 0.032 / 0.025 / 0.036 | 0.032 / 0.026 / 0.035 |

- **The systematic step is under 0.02 on every channel of both flanks.** The adjacent patches differ by about a
  third of the noise floor.
- **The green deficit of the back half is gone.** Before, G was 0.021-0.038 lower behind the seam on both flanks:
  Athena's "front greener, back browner".
- **The right flank is now slightly lighter behind** (+0.010). That is the seam-wide shift reaching a flank that was
  nearly matched before. It is within the target.

### 1.6 The picture

- **Side renders**, same orthographic camera and lights as MAW-MV's turn script, 500 px/m, `<seam>/renders/`:
  `before_{045,090,270,315}.png`, `final_{045,090,270,315}.png`, `src-route_*.png`.
- **Unlit albedo views** (Workbench, flat): `flat-before_*`, `flat-uvfix_*`, `flat-src-route_*`.
- **By eye.** The olive front against the orange-brown back is now olive-brown on both sides at the seam on both flanks.
  A broad two-tone impression remains between the head end and the rump. The straight vertical ledge where the front
  half's skin stands proud of the back half, and the square notch at the ground, are unchanged: they are geometry.

### 1.7 The critics (fresh, blind, images only, judging only the seam)

**The procedure.**

- **Sheets.** A/B sheets of the four views plus a quarter-size sheet. The fixed panel was put left on some sheets and
  right on others; the key is in `critic*/KEY.txt`.
- **Critic 1** saw `uvfix-v1`. "IMPROVED BUT VISIBLE". It found the colour step much smaller, and new artefacts: a
  yellow-olive streak across mid-flank, olive and yellow spill onto the hind leg and foot, and a leftover red-brown
  patch behind the seam. That led to the fade and gain 1.
- **Critic 2** (a new agent) saw the run of record (`<seam>/critic2/`). Its verdict follows, condensed. Its words are
  kept; only the layout changed.

**Critic 2's verdict.**

> The seam colour step is only slightly weaker in the better panel, and the body still reads as two tones in every
> view. Verdict: **IMPROVED BUT VISIBLE**, close to NOT FIXED. A straight geometric edge along the join stays the same
> in both panels and is now the main thing that shows the seam.
>
> - **Sheet 1:** B (the fix) is a little weaker. "The back side right next to it is pulled toward olive, so the hue
>   jump is smaller. It is still a step, not a blend."
> - **Sheet 2:** B (the fix) is weaker. The area just behind the line is olive-brown and closer to the front tone. It
>   is softer at mid-flank, but the step is still visible at the top and near the ground.
> - **Geometry:** "a long straight vertical edge running the full height of the body; a ledge where the front shell
>   overlaps the back; a rectangular notch at the ground". A pink patch sits at the foot of the join "in both panels, so
>   the correction did not cause it".
> - **At quarter size:** all four panels still read as two tones; the fixed ones (B, C) are slightly better than the
>   others.
> - **New artefacts:** probably none. At most, a faint dull, desaturated band on the back side of the join in one view.

**Read against the order.**

- The colour step at the seam is measurably under control: 0.012 or less per channel on the bands, under the brief's
  0.02.
- To a viewer, the seam is still there:
  - the geometric ledge and the notch draw the line;
  - the head-to-rump tone change still reads as two halves (the critic: "close to NOT FIXED").
- Removing the ledge needs a geometry change at the cut. Options: a wider overlap, a smoothing pass on the remesh
  across the band, or a re-merge with the back fitted tighter to the front's flank. That was outside this lane (no
  re-mesh). The 45-minute box for Phase A had run, so the maw was carried into Phase B with this texture.
- Defects 2-4 of MAW-MV's critic (palette, dome, legs) are the source's and were not chased.

### 1.8 Re-rig and records (the s7 fit, unchanged)

The finish `.blend` was copied with the texture of record beside it (`<seam>/finish-uv/`). Then MAW-MV's commands ran
with `apps/vr/tools/mesh/beast-fits/mire-maw-s7202-s7.json`: `beast_parts.py`, `rig_anim.py`, and `evidence.mjs ...
Attack MireMaw`.

| Measure | Value |
|---|---|
| beast_parts.py | 1 part (the tongue capsule), 17,490 -> **17,752 tris**, 0 non-manifold edges, `M_MireMaw`, `M_MireMaw_Tongue` |
| rig_anim.py | 11 bones, height 1.2509 m, proxy heat on 202,592 tris, 0 zero-weight proxy vertices, 0 nearest-bone fallbacks, 5,207 rule-weighted vertices |
| **FBX** `<seam>/rig/MireMaw.fbx` | 1,270,876 bytes, **`6498f40c8ca4efd7da32abc903f2f537bd1e155e8b4756b9fbfb255ea8bbdcfd`** (6fc79bf: 1,270,908, `5befa0e0`; the EXACT-boolean tongue union returns its vertices in another order each run) |
| **Texture** `<seam>/rig/T_MireMaw_BaseColor.png` | **`d46f288fb31ab978e3ba033d044d6ee7184fec6a88e49cf42459a9dd27ed6e1c`** |
| evidence.mjs | exit 0, 6 PNGs (`<seam>/evidence/`) |

**Blender was down before Phase B:** `tasklist | grep -i -E "blender|unreal"` printed nothing at 21:27Z, and nothing
listened on 8188.

## Phase B: one Unreal lane

At most one Unreal process ran at any time. The script waits for the lane and refuses Blender or port 8188. Each build,
import, capture, budget and suite run started only after the previous one had exited. The MetaXR plugins were copied in
with `robocopy ... /E` (a copy). Build 1 (cold) and build 2 (incremental) both logged `Result: Succeeded`.

### 2.1 What was built (`9250cfd`)

| Piece | Where |
|---|---|
| `MireMaw` spec: 1.2509 m, plain material, `FlatSlot` "Tongue" -> flat linear (0.20, 0.012, 0.012) in `M_MireMaw_Flat`; a `SceneLateralM` per spec | `Greybox/CreatureImport.{h,cpp}` |
| The flat material and slot routing (fails if no slot matches) | `Tools/ImportCreatureMeshCommandlet.cpp` |
| The hound's glow terms: soft crack bloom (`EmberSoftHlsl`, 12 taps on rings of `SoftRadiusUv` 0.008 and half that, x (1 - mask)) and a Fresnel rim, as `SoftIntensity` / `RimIntensity` / `RimExponent`; the C++ mirror `EmberSoftMask` | `Greybox/CreatureImport.{h,cpp}`, commandlet |
| `SpawnScene` spawns every spec at its spot (`-MageArenaCreatureImportScene`, the budget scene) | `Greybox/CreatureImport.cpp` |
| The capture: plate, maw x3, hound mask + 5 glows x2 views (held pose), cast | `Greybox/CreatureImportCapture.{h,cpp}` |
| The maw source by sha256, the new shot list, `-GlowIntensity/-GlowRim/-GlowSoft` overrides, staging `creature-cast-ue-2026-10-24` | `apps/vr/tools/capture-creature-import.ps1` |
| Pins | `MageArena.CreatureImport.EmberKey` |

### 2.2 Import (three FBX, sha256 checked by the script before every run)

| Asset | FBX sha256 | Texture sha256 | Result |
|---|---|---|---|
| Brennic | `dc0a4f38...67b0` | `024d4aaa...acae` | 23 bones, 23,990 tris, 1 slot, 1.7992 m, 4 clips (as CREATURE-UE-IMPORT) |
| CinderHound | `a650bc3e...5449` | `b36529c7...e08d` | 26 bones, 11,796 tris, 2 slots (both `M_CinderHound`), 1.3569 m, 4 clips |
| **MireMaw** | **`6498f40c...bdcfd`** | **`d46f288f...6e1c`** | **11 bones, 17,752 tris, 1 LOD, 2 slots** (`M_MireMaw` -> `M_MireMaw`, `M_MireMaw_Tongue` -> `M_MireMaw_Flat`), **height 1.2509 m** (expect 1.2509), bounds extent 111.4 x 119.2 x 62.5 cm, clips Idle 2.000 s / 61 keys, Attack 2.200 / 67, Hit 0.800 / 25, Death 2.000 / 61 |

- The maw's `head` bone sits at (45.1, 0, 86.1) cm. `PlaceCreature` reads forward (1.00, 0.00) and yaws it 153.4 deg to
  face the centre pad from its spot.
- The commandlet logged the maw texture as "0x0 PF_Unknown" when it compiled (it was still compiling asynchronously). At
  capture time it is 2048² DXT1, 2.69 MB with mips, the same as the other two.
- The scratch folder was deleted by the script's `finally` after both capture runs. `git status` showed only source
  files.

### 2.3 Placement at 8 m

All three stand 8 m ahead of the centre pad (x -4 m), at lateral **Brennic -1.4 m, hound +1.4 m, maw -4.0 m**.

- The near-centre marker post is on the centre line, so nobody stands there.
- Turned 26.6 deg to face the player, the maw is 3.1 m across (2.38 x 2.23 m footprint), spanning y -5.6 to -2.4. That
  leaves 0.5 m to Brennic's 1.03 m span and 1.3 m to the near-left post at (-6.86, -6.13).
- The hound is 2.59 m long, pointing at the player. The braziers (-2, ±7.2) and banners (x ±9.4) are clear.
- In the cast still no post stands through anybody (`16-cast-duel.png`). The test pins the spots: |lateral| >= 1 m,
  maw-to-Brennic and Brennic-to-hound clearances.

### 2.4 The hound's glow at 8 m

The candidates differ only in the hound's material parameters, set on a dynamic instance per slot. **No tonemapper,
exposure or post-process change** (bloom stays 0 and the tonemapper off, as the pawn sets them).

**How each candidate was shot.** The hound's Idle clip is held from its first shot, so every candidate and its mask
shot show one pose.

**How the pixels were measured** (`creature-cast-ue-2026-10-24/scripts/glow.py`):

- **Hound pixels:** the shadowless mask shot (04, 10) against the empty plate (00), any channel off by more than 0.04.
- **Glow pixels:** hound pixels with hue 5-50 deg, S > 0.45 and V > 0.55.
- **Core:** the median of the brightest 3 % of hound pixels, the crack centres.
- **Yellowed:** a glow pixel with R >= 0.98 and G >= 200/255.
- **Not comparable with CREATURE-UE-IMPORT.** That doc's "9 %, (251, 153, 66)" used a stricter, unrecorded threshold.
  The base row below is this lane's measure of the same look.

| Candidate (EmberIntensity / SoftIntensity / RimIntensity) | Front: glow % of 8,513 px | glow median sRGB | core sRGB | yellowed px | body median | Three-quarter: glow % of 14,727 px | core | yellowed |
|---|---|---|---|---|---|---|---|---|
| base, bc630e0 (1.6 / 0 / 0) | 17.9 | (194, 122, 65) | (255, 171, 68) | 0 | (75, 35, 30) | 16.9 | (255, 169, 68) | 0 |
| intensity (3.0 / 0 / 0) | 18.0 | (197, 125, 71) | **(255, 219, 86)** | **192** | (75, 35, 30) | 17.2 | (255, 211, 84) | **230** |
| rim (1.6 / 0 / 0.8, exponent 3) | **37.1** | (185, 103, 51) | (255, 172, 69) | 0 | **(86, 42, 30)** | 27.2 | (255, 170, 68) | 0 |
| soft (1.6 / 1.0 / 0) | 25.7 | (190, 116, 60) | (255, 170, 68) | 0 | (79, 38, 30) | 23.8 | (255, 168, 68) | 0 |
| **chosen: soft (1.6 / 1.2 / 0)** | **27.4** | **(190, 115, 59)** | **(255, 170, 68)** | **0** | (79, 38, 30) | **25.3** | **(255, 168, 68)** | **0** |

No pixel clipped to pale yellow-white (R and G both >= 0.97) in any shot.

**Chosen: the soft crack bloom at 1.2.** Three reasons:

- **It widens the glow without changing its colour.** It adds about 9.5 points of glowing pixels at 8 m: the veins
  read as continuous lit bands instead of flecks (`glow-sheet-front.png`, `glow-sheet-three-quarter.png`). The crack
  cores keep the bc630e0 orange, because the bloom lights the hide *around* a crack (x (1 - mask)).
  - **The try1 history.** In the exploration run (try1, `creature-cast-ue-2026-10-24/try1/`) the bloom was added on
    top of the cores too. The cores then went to (255, 192, 76), with 145 yellowed pixels. The halo-only form is the
    fix of record.
- **More `EmberIntensity` cannot brighten an already-clipped red.** Red is saturated at 1.6 already. A higher intensity
  only raises green, so the cores turn yellow: (255, 219, 86), 192 yellowed pixels. The glow area does not grow
  (18.0 %). This is option 2 of CREATURE-UE-IMPORT section 6, now measured.
- **The rim lights the whole hide.** It gives the largest number (37 %), but most of it is a brown-orange sheen over
  the body: the body median rises from (75, 35, 30) to (86, 42, 30). The rim loses the dark hide the cracks read
  against, and makes the hound read "hot brown" rather than "dark beast with lava veins".

The pins of `MageArena.CreatureImport.EmberKey` for the key itself (thresholds, classes) **did not change**. New pins:

- the bloom over a ring of hide is under 0.02, and with 3 of 12 taps on a crack it is 0.2-0.3;
- the soft HLSL reads every input;
- a lit crack (`EmberColour.G x (EmberIntensity + RimIntensity)` = 0.40) and its halo (`G x (SoftIntensity +
  RimIntensity)` = 0.30) keep linear green under 0.6;
- the chosen glow is the bloom.

The red step was not demonstrated in a separate build in this lane. These pins went in with the default they check.

### 2.5 The stills (`C:/Users/kazda/mage-art-staging/creature-cast-ue-2026-10-24/shots/`, 1920x1080, T56 dusk, 8 m)

| File | What |
|---|---|
| `00-plate.png` | the empty arena (for the masks) |
| `01-maw-front.png`, `02-maw-three-quarter.png`, `03-maw-back.png` | the maw at its spot |
| `04-`/`10-hound-mask-*.png` | the hound without its shadow, base glow |
| `05-09-hound-glow-{base,intensity,rim,soft,chosen}-front.png`, `11-15-...-three-quarter.png` | the candidates |
| `16-cast-duel.png` | **the whole cast for the owner**: maw, Brennic, hound at 8 m |
| `maw-crop-sheet-2x.png`, `cast-crop-2x.png`, `glow-sheet-{front,three-quarter}.png` (folder root) | crops |

**An honest look:**

- **The maw (01-03) reads as the maw.** A squat brown warty toad about 2.4 m wide, with the gape open toward the
  player: cream teeth on both jaws, a red mouth and tongue, a dark throat. It is the key-art identity at duel distance.
  - The three-quarter view keeps the gape and shows the hind legs as lumps.
  - The back is one closed brown mound with a lighter strip at the foot of the join. At 8 m in this back-lit dusk the
    flank seam is hard to see. The ledge reads as a soft vertical shading line on the left flank.
  - The palette is the source's mud-brown, not the reference's ochre (MAW-MV critic defect 2, not chased).
- **The hound (09, 15) with the bloom.** The veins read as glowing orange bands on a dark-red body. Head, shoulder,
  flank and leg veins connect; eyes and paw pads glow. It is clearly a fire beast at 8 m. CREATURE-UE-IMPORT's base
  look read as flecks. The cores are orange, not yellow.
- **The cast (16)** reads as a rival flanked by his two beasts: the maw large and low on the left, Brennic upright in
  the centre-left, the glowing hound on the right.
  - Scale reads: the maw's mound is as wide as Brennic is tall, and the hound is a big dog beside him.
  - Each casts its long dusk shadow toward the viewer.
  - The maw is the least saturated of the three and leans on its open red mouth for its read.

### 2.6 Budget: D-G4 with all three (capture-budget.ps1 via the script, 1 baseline + 2 runs)

**How it was run.**

- Each run sampled 60 s after a 10 s warm-up, with 20 enemies, 100 projectiles, both hands and 6 Niagara systems.
- Every run logged `MAGEVR_BUDGET_DONE`, no `MAGEVR_BUDGET_FAIL`, `particlesExercised` true and parity ok (210
  planned parts / 11 tuples).
- In the `with` runs all three stood at their spots at 8 m, in view. BasePass is the `r.MeshDrawCommands.DumpStats`
  reading (6 dumps a run).
- Staging: `budget/creature-cast-{without1,with1,with2}/`.

| Run | Draw calls median / max | BasePass tris median / max | ShadowDepth tris | All passes median / max | Live particles median / max | Peak working set | GPU ms median |
|---|---|---|---|---|---|---|---|
| CREATURE-UE-IMPORT with (Brennic + hound) | 124 / 128 | 172,794-173,066 / 175,450 | 103,108 | 579,794-581,624 / 587,756 | 1,399 | 3,202-3,290 MB | 0.87 |
| this run, without1 (same build, no meshes) | 109 / 113 | 137,320 / 139,616 | 31,536 | 401,532 / 408,402 | 1,393 / 1,468 | 3,054 MB | 0.85 |
| **with1** (all three) | **134 / 137** | **191,146 / 194,978** | **138,612** | 669,714 / 681,552 | 1,393 / 1,468 | 2,847 MB | 0.88 |
| **with2** | **133 / 136** | **191,506 / 193,202** | **138,612** | 670,454 / 676,500 | 1,396 / 1,468 | 2,895 MB | 0.88 |
| **Limit** (PROJECT-PLAN section 7) | **<=150** | **<=350,000** | | | **<=2,000** | | |

- **Draw calls: +25** (109 -> 134 median, 113 -> 137 max). That is five mesh sections (Brennic 1, hound 2, maw 2)
  times base pass, depth prepass, velocity and two shadow cascades. The maw added 10 of them.
  - **13 calls of headroom remain at the max.**
  - **The max is 137, under the 145 flag.**
  - Any fourth creature with two sections (+10) would land at about 147. Merging the maw's tongue into the body
    material would save 5.
- **BasePass +53.8k** (137,320 -> 191,326 mean of the medians): the three LOD0s, 23,990 + 11,796 + 17,752 = 53,538.
  The row stands at **55 % of 350k**.
- **ShadowDepth +107.1k** (31,536 -> 138,612), the three meshes in both CSM cascades (2 x 53.5k). It is logged, not
  gated. Quest has the same 2 cascades.
- **Texture memory: +8.07 MB.** Three 2048² DXT1 textures, 2.69 MB each with mips (the capture's `memory.*` notes).
  Mesh resources add +1.90 MB (0.80 + 0.36 + 0.74). The process working set moved within run-to-run noise.
- **Live particles unchanged** (1,393-1,396 median, 1,468 max, of 2,000).
- **The LOD0 estimate still ignores skeletal meshes.** `lod0InFrustumTriangles` stayed 146,882-146,930, as
  CREATURE-UE-IMPORT found. The gate reads BasePass, which counts them.
- **Materials:** the three meshes add 4 materials over 5 slots: `M_Brennic`, `M_CinderHound` x2 slots, `M_MireMaw`,
  `M_MireMaw_Flat`.

## 3. Gates

| Gate | Result |
|---|---|
| `build.ps1` | `Result: Succeeded` (build 1, cold; build 2 after the halo change, the build every run of record used) |
| `Automation RunTests MageArena.` (`-nullrhi`, sigil corpus regenerated first: `generate.mjs --corpus 30 --seed 1000`, `mousepaths.mjs`) | **found 295, 295 `Result={Success}`, `TEST COMPLETE. EXIT CODE: 0`**. 295 as at bc630e0: no new test, `MageArena.CreatureImport.EmberKey` gained pins. `MageArena.Input.CaptureFlags` green (it reads the script's flags) |
| `node apps/vr/tools/lint/unused-members.mjs` / `format-check.mjs` | OK (597 members / 7 changed C++ files) |
| `python -m unittest` `test_seam_lib.py` `test_merge_halves.py` `test_beast_fit.py` `assets/test_finish.py` | 62 OK (18 new in `test_seam_lib.py`) |
| `node --test apps/vr/tools/mesh/*.test.mjs` (4 files) | pass |
| the merge gate's test line (agy-guard, check-pin and self-test, check-overlay and self-test, leonardo-redaction, repo-law) | each exit 0 |

`MageArenaDesign` was not run: no combat path changed.

## 4. Nothing ships

- **The DINOv3 hold stands.** No mesh, FBX, texture, `.uasset` or still from this lane ships.
- The imported assets are only in staging: `creature-cast-ue-2026-10-24/uasset/`, 25 files, 20 MB.
- No ASSET-LEDGER row; no pinned data touched; no image generated; no paid call.

## 5. Open questions

- **The maw's seam ledge.** The colour step is measured away, but two blind critics still see the seam, mostly as
  geometry: a vertical ledge and a ground notch at the cut. Fixing that needs a geometry pass at the cut (a wider
  overlap, a smoothing band on the remesh, or a tighter back fit to the front's flank). That is a re-mesh, which this
  lane was told not to do. Athena's call whether it is worth a lane.
- **The maw's broad two-tone.** The head end (olive, from the s7) and the rump (brown, from s7301) still read as two
  halves at quarter size. A whole-body palette pass (the style clamp of `finish.mjs`, or a texture pass toward the
  reference's ochre) would fold that into MAW-MV critic defect 2, the source palette.
- **The glow choice is a style call.** The bloom at 1.2 is measured and chosen here. Athena may prefer the stronger
  rim read, or a bloom with a larger `SoftRadiusUv`. Both are parameters: `-GlowSoft` / `-GlowRim` re-run without a
  rebuild.
- **The hound's mouth-lining slot** still takes `M_CinderHound` (CREATURE-UE-IMPORT question), so it glows (and now
  blooms) when the jaw opens.
- **Draw-call headroom:** 13 calls at the max with three creatures. The next creature, or more sections, needs a
  budget look (merge small slots such as the maw's tongue, or share materials).
- **The LOD0 estimate ignores skeletal meshes** (unchanged; a budget card outside these paths).
