---
title: Brennic back-cloak normal map - Blender half (finish.py --normal on), seed 7
channel: vr
date: 2026-10-25
status: RAN (Blender half of 2) - finish.py bake mode gained an opt-in tangent-space normal bake (91d7fb0); seed 7 re-finished with geometry identical to the bake-mode record (asserted by --expect), re-rigged, evidence rendered; blind critic SAME at 8 m (MINOR under a low raking light), VISIBLE GAIN close up; one regression to watch (over-sharp streaks on the lower inner cloak, front close-ups). The Unreal half (import, material, 8 m captures, D-G4 re-check) is a separate lane. Nothing ships (DINOv3 hold).
sources: [docs/DECISIONS.md (2026-10-25 Athena on a02aa89; 2026-10-24 20:13Z), docs/research/BRENNIC-MESH-REWORK-2026-10-24.md, docs/research/DRAWCALL-BUDGET-2026-10-25.md, apps/vr/tools/assets/finish.py, apps/vr/tools/mesh/finish_lib.py, apps/vr/tools/mesh/finish-args.mjs, apps/vr/tools/mesh/finish.mjs, apps/vr/tools/mesh/README.md]
---

# Brennic back-cloak normal map, 2026-10-25 (Blender half)

**Outcome.** `finish.py` bake mode has an opt-in normal bake (`finish.mjs --normal on`, default off). Seed 7 was
re-finished with it from the same raw GLB, onto the same LOD0, and the result was checked.

- **Geometry: identical.** `--expect` compared the new finished card against the 2026-10-24 bake-mode record and
  found no drift: 23,990 tris, 11,987 verts, 1 section, height 1.79923 m. With the option off, the flat and bake
  modes reproduce their GLBs and the albedo byte for byte.
- **The raw high-poly does carry back-cloak relief, so the finding is not "no relief".** Part of it is mesh geometry,
  and more of it comes from a normal texture that the TRELLIS GLB carries and that nobody had noted before (section 3).
- **Critic (blind, 12 pairs): SAME at the 8 m duel distance** (5 of 6; the back view under a low raking light is
  MINOR). **VISIBLE GAIN close up** (back34 and under raking light), MINOR in the two other close pairs. Unblinded, the
  side with more detail was the normal-on render in every pair the critic named.
- **No seam, faceting or inside-out fold was seen.** One regression is worth watching: over-sharp, glossy vertical
  streaks on the lower inner cloak, in the front close-ups only.
- **What it means for the Unreal half:** at 8 m on the desktop, the map is close to invisible under even light. It may
  read under the T56 dusk, whose key is low. That is the question the 8 m captures must answer, measured, not assumed.
- **Licence hold, unchanged.** Nothing ships until the owner rules on the DINOv3 weights. No Unreal process, TRELLIS
  run, ComfyUI, new seed or image-generation call was used, and at most one Blender process ran at a time.

All outputs are in `C:/Users/kazda/mage-art-staging/brennic-cloak-normal-2026-10-25/` (`<out>/` below). No binary is
in git.

## 1. The change (commit `91d7fb0`)

| File | What |
|---|---|
| `apps/vr/tools/assets/finish.py` | `normal=on`: a Cycles `NORMAL` bake right after the albedo bake (the high-poly still exists); `finish_normal()` after `finish_bake()`. Off: no new code runs and no new record key is written |
| `apps/vr/tools/mesh/finish_lib.py` | option `normal` (`off`/`on`, needs `mode=bake`); `fill_normal_misses`, `normal_deviation`, `back_region_faces`, `png16` / `read_png16` |
| `apps/vr/tools/mesh/finish-args.mjs` | `--normal` (`off` is left out of the list, as `--clamp none` is); `geometryDrift` |
| `apps/vr/tools/mesh/finish.mjs` | `--normal off\|on`, `--expect <reference finish dir>` (exit 1 on drift) |
| tests | `test_finish.py` 26 (was 19 + 7 new), `finish-args.test.mjs` 7 (was 5 + 2 new) |

### What `normal=on` does

1. **Bake.** After the albedo bake, with the same selected-to-active setup: cage extrusion 0.02 m, max ray 0.08 m,
   margin 16 px EXTEND, the repacked UV layout, 2048. The call is
   `bpy.ops.object.bake(type='NORMAL', normal_space='TANGENT', normal_r='POS_X', normal_g='POS_Y', normal_b='POS_Z')`,
   into a float, Non-Color image. Cycles takes the tangents from the LOD0's UV map (MikkTSpace). Every LOD0 face is
   already a triangle (`quads_convert_to_tris` before the UV step), so the basis is the one the FBX's triangles get.
2. **Misses.** The texels the albedo's hit mask marks as missed (same rays) become flat (0.5, 0.5, 1.0). The margin
   is then rebuilt 16 rings out from island texels alone, and everything beyond it is flat. Black is never written.
3. **Statistics.** The angle of each texel's decoded normal from (0, 0, 1) is measured over the island texels and over
   the back region (section 3).
4. **Output.** `T_Brennic_Normal.png`: 16-bit RGB, linear (no gAMA or sRGB chunk), OpenGL Y+. `M_Brennic` gains Image
   Texture (Non-Color) -> Normal Map (tangent, the LOD0 UV map) -> Principled Normal. The GLB exports it as
   `normalTexture`.

## 2. Commands

The raw GLB is unchanged: `C:/Users/kazda/mage-art-staging/brennic-mesh-2026-10-24/s7/brennic-s7.glb`, sha256
`bef2ed78d4d34ccaed2f5544bc95d551fbf843725f6f53d6243c710708e00698`. The runs started at 2026-10-24T23:43Z. Blender
4.2.1 LTS.

```
node apps/vr/tools/mesh/finish.mjs <raw> <out>/regress-flat --class Humanoid --name Brennic --mode flat --expect <rework>/scratch-flat-regression
node apps/vr/tools/mesh/finish.mjs <raw> <out>/regress-bake --class Humanoid --name Brennic --mode bake --expect <rework>/finish-bake
node apps/vr/tools/mesh/finish.mjs <raw> <out>/finish-normal --class Humanoid --name Brennic --mode bake --normal on --expect <rework>/finish-bake
blender -b --python-exit-code 1 -P apps/vr/tools/assets/rig_anim.py -- <out>/finish-normal/Brennic_finish.blend <out>/rig Brennic <out>/rig/Brennic.fbx Cast
cp <out>/finish-normal/T_Brennic_BaseColor.png <out>/finish-normal/T_Brennic_Normal.png <out>/rig/
node apps/vr/tools/mesh/evidence.mjs <out>/rig/Brennic.fbx <out>/evidence Brennic <out>/rig/Brennic_rig.json Cast
```

`<rework>` is `C:/Users/kazda/mage-art-staging/brennic-mesh-rework-2026-10-24`. All six exited 0, and all three
finishes printed "geometry matches".

**Regression, option off.**

- **Flat:** `finish.json` is field for field the same as the 2026-10-24 flat regression run (only `bake_s`, a timing,
  differs). The GLB sha256 `7c6fae2a…` is the same.
- **Bake:** the same, except for one key, `bake.black_after_ring_fill: 0`. That key was added to master by the maw
  refill change after the 2026-10-24 Brennic run; it is not from this change. The GLB sha256 `2233bdfd…` and
  `T_Brennic_BaseColor.png` (`024d4aaa…`) are byte-identical.

## 3. Card, before and after

| Measure | 2026-10-24 bake finish | This finish (`--normal on`) |
|---|---|---|
| Verdict / score | pass / 100, no findings | **pass / 100, no findings** |
| Triangles / vertices / edges | 23,990 / 11,987 / 35,985 | **23,990 / 11,987 / 35,985** |
| Components; watertight | 1; yes | 1; yes |
| Height; min z | 1.79923 m; 0.00027 m | **1.79923 m; 0.00027 m** |
| Sections (materials) | 1 (`M_Brennic`) | **1** (`M_Brennic`, + normal) |
| Finished GLB | 3,563,700 B, `2233bdfd…` | 22,302,280 B (both PNGs embedded), `dd3e0fda2eac9ac1c4c9517760370d16b9147a7e37370da0a76ed63e17bb91d7` |
| finish.py seconds | 34.1 | 47.4 (normal bake 4.6, normal finishing 6.6) |

### Normal-map statistics (the run's `normal` record)

**Back region rule** (`finish_lib.back_region_faces`): a LOD0 face whose unit normal has y > 0.5 (within 60 degrees of
facing +Y, the back; the front faces -Y) and whose centroid is 0.5-1.5 m high. That is 5,823 faces; the cloak covers
nearly all of the back at those heights.

| Texels | Count | > 10 deg | > 20 deg | Mean |
|---|---|---|---|---|
| All island texels | 1,772,680 | 45.0 % | 28.7 % | 16.6 deg |
| All, hit texels only | 1,761,368 | 45.3 % | 28.8 % | 16.7 deg |
| **Back region** | 505,634 | **42.2 %** | **27.6 %** | **15.4 deg** |
| Back region, hit only | 502,555 | 42.5 % | 27.7 % | 15.5 deg |

**Where the relief comes from.** The raw TRELLIS GLB carries more than geometry. A probe (`<out>/scripts/probe_hi.py`)
found smooth custom normals on all 697,440 faces, and a material with its own normal-map texture: a `NORMAL_MAP`
node in `Material_0`. A Cycles NORMAL bake reads the shading normal, so the map carries both. A scratch run with that
link unplugged (`<out>/scripts/finish_geo_only.py`, `<out>/scratch-geo-only/`, not a pipeline option) separates them.
Island size comes from the LOD0's UV layout (`<out>/scripts/nrm_probe.py`):

| Texels | Source | > 10 deg | > 20 deg | > 60 deg | inward (z < 0) | Mean |
|---|---|---|---|---|---|---|
| Back region, islands of >= 20 faces | mesh + TRELLIS normal texture (**the run of record**) | 38.4 % | 23.4 % | 1.1 % | 0.36 % | 12.7 deg |
| Back region, islands of >= 20 faces | mesh geometry only (scratch) | 15.3 % | 5.2 % | 0.8 % | 0.31 % | 6.8 deg |
| All islands under 20 faces | mesh + texture | 79.5 % | 67.4 % | 29.6 % | 9.6 % | 43.0 deg |
| All islands under 20 faces | geometry only | 75.2 % | 63.4 % | 27.8 % | 7.6 % | 40.8 deg |

- **The mesh carries real fold relief on the back cloak**, at a mean of 6.8 degrees. The TRELLIS normal texture
  roughly doubles it: the fine crumple the critic saw.
- **The noise is in the small islands, and it is the layout, not the texture.** Smart UV leaves 1,849 islands of
  under 20 faces (median island: 1 face). They hold 3,882 faces (16 %) and 166,011 texels (9 %), and they bake chaotic
  normals with or without the texture. About three quarters of the map's inward texels are there (16.0k of 21.1k).
  - These are single triangles of the decimated LOD0 at high-curvature spots. Their interpolated normal and the
    high-poly's differ by tens of degrees, and a lone triangle has no neighbours for the margin to agree with.
  - The critic saw no speckle from them. They are tiny in screen space, and the island-only margin keeps them from
    bleeding.
  - A future layout pass (fewer, larger islands) would clean them; it is not needed for this result.
- The map is not a procedural or noise normal: every value is the source's shading normal, read by a ray.

### Miss accounting

| | Value |
|---|---|
| Island texels missed (the albedo's hit mask: same rays, same count) | 11,312 (0.64 %), 8,289 interior |
| Of those, not flat in Blender's raw bake (its EXTEND margin had painted them) | 11,309 |
| Filled to flat (0.5, 0.5, 1.0) | all 11,312 |
| Black texels where a ray hit / anywhere after the fill | 0 / 0 |
| Margin rings rebuilt from island texels | 16 |
| Faces whose UV-centroid texel missed | 226 (of 23,990) |
| Back region: faces / texels missed | 46 faces / 3,079 texels |

The 2026-10-24 doc counted 275 faces by the same centroid rule, but from the hit mask re-read from its 8-bit PNG.
This run reads the float mask in-process. The texel counts agree exactly, and the face difference was not chased.

## 4. Hashes (for the Unreal half)

| File | Bytes | sha256 |
|---|---|---|
| `<out>/rig/Brennic.fbx` | 1,896,588 | `fa04d75df706f24fa89c690d7294a833c4b2c4d83c6bc37cada2adb1b7546358` |
| `<out>/rig/T_Brennic_BaseColor.png` | 2,734,994 | `024d4aaa888d94b2f257be993a96e8ed7a80176cedffaa5b0735620b031facae` (unchanged) |
| `<out>/rig/T_Brennic_Normal.png` | 18,738,398 | `08e34dfc084c5a77853be206779f2a1ec5e06cdc7deffb5e56e7dcc01168e142` |

**The FBX bytes changed:** `dc0a4f38…` (1,895,580 B) -> `fa04d75d…` (1,896,588 B). There are two reasons.

- The material now references `T_Brennic_Normal.png`: 5 more name strings, about +1 KB. The commandlet imports with
  `bImportTextures = false`, so UE ignores the reference.
- The exporter writes a `CreationTimeStamp`.

The mesh and the rig did not change: `Brennic_rig.json` matches the 2026-10-24 record in every field except the FBX
path and the timings (23 bones, the same clips and keys). `evidence.mjs` exited 0 with all six PNGs, and the turnaround
shows the textured figure.

## 5. Green channel

Blender bakes **OpenGL, Y+** (swizzle +X +Y +Z). UE expects **DirectX, Y-**. The PNG stays OpenGL, so the Blender
material and the GLB evidence are self-consistent. The Unreal half must set, on the imported texture:

- `Texture->bFlipGreenChannel = true`
- `Texture->SRGB = false`
- `Texture->CompressionSettings = TC_Normalmap` (BC5 on desktop)
- `Texture->LODGroup = TEXTUREGROUP_CharacterNormalMap`

Forgetting the flip shows as folds lit from below under a top light. Check that on the first capture.

## 6. Critic

**Renders** (`<out>/scripts/onoff.py`): EEVEE, on the finish blend, normal on vs off. Off removes the link into
Principled Normal; everything else is the same.

- **Light L1:** turn.py's light (grey world 0.65 x 0.9, a key sun from the front at 2.2, two back suns at 1.0).
- **Light L2** (supplementary, dusk-like): a low raking key from behind-left at 4.0 and a front fill at 0.6, over a
  world at 0.25.
- **Framings:**
  - `game8m`: the D-G4 desktop view, 1920 x 1080, 90 deg horizontal FOV, the figure 8 m away, camera at 1.2 m.
    Brennic stands about 216 px tall, a screen size of about 0.231.
  - `close`: orthographic, 2.1 m over 2000 px.
- **Views:** front, back34 (back-left three-quarter) and back.

The sheets are in `<out>/critic/`, A/B sides randomised per pair; the key is in `<out>/critic_key.json`.

**Pixel difference on the figure** (on vs off, mean |delta| in 8-bit levels / pixels differing by more than 8):

| View | L1 8 m | L1 close | L2 8 m | L2 close |
|---|---|---|---|---|
| front | 0.97 / 121 | 1.64 / 8,171 | 0.94 / 196 | 1.64 / 11,113 |
| back34 | 0.70 / 13 | 1.20 / 3,655 | 1.23 / 246 | 1.88 / 9,915 |
| back | 0.45 / 6 | 0.74 / 1,872 | 1.40 / 273 | 2.01 / 11,095 |

**Verdicts.** The critic was a fresh agent told only that the pair differs in one surface-shading input. Sides were
unblinded afterwards.

| Pair | Light, view, framing | Verdict | Detailed side = | Regressions |
|---|---|---|---|---|
| 01 | L1 front 8 m | SAME | - | none |
| 03 | L1 back34 8 m | SAME (difference in x3 only) | on | none |
| 05 | L1 back 8 m | SAME (x3 only) | on | none |
| 07 | L2 front 8 m | SAME | - | none |
| 09 | L2 back34 8 m | SAME (x3 only) | on | none |
| 11 | L2 back 8 m | **MINOR**: "a fine mottled crumple texture and clearer hem folds" | on | none |
| 02 | L1 front close | MINOR | on | on: thin, sharp, slightly glossy vertical streaks on the lower inner cloak, "over-sharp fold lines rather than a seam" |
| 04 | L1 back34 close | **VISIBLE GAIN**: "neck and shoulder creases, a ripple across the upper back and long vertical folds. A reads as a flat sheet" | on | none |
| 06 | L1 back close | MINOR | on | on: mottling "a little blotchy rather than clearly like folds", mild |
| 08 | L2 front close | **VISIBLE GAIN** | on | on: the same streaks on the lower inner cloak |
| 10 | L2 back34 close | **VISIBLE GAIN** | on | none |
| 12 | L2 back close | **VISIBLE GAIN** (borderline MINOR) | on | none |

- **Summary.** 8 m: SAME. Close: VISIBLE GAIN. There were no seams along island edges, no faceting, no speckle and no
  folds lit inside-out in any pair.
- **Beyond the cloak.** The critic also saw more relief on the hair (curls), the bracers and the tunic: the map acts
  on the whole figure. That is the hair-curl relief the 2026-10-24 critic found the bake had lost.
- **The streaks.** These sit where the 2026-10-24 misses cluster (the inner cloak hem, 0.5-0.9 m), and where cage
  rays can reach the cloak's other wall. They are a watch item for the UE captures, not a blocker.

## 7. Hand-off: the Unreal half (a separate lane; never at the same time as Blender)

**Inputs** (the shas in section 4; copy them into the lane's staging):

- `<out>/rig/Brennic.fbx`
- `<out>/rig/T_Brennic_BaseColor.png`
- `<out>/rig/T_Brennic_Normal.png`

**Import changes** (all in `apps/vr/Game/**` and `apps/vr/tools/capture-*.ps1`, which this lane did not touch):

1. **`capture-creature-import.ps1`.** Point Brennic at the new FBX and pin its sha, `fa04d75d…`. Keep the BaseColor
   sha, `024d4aaa…` (unchanged). Add a pinned `NormalTexture` / `NormalSha` (`08e34dfc…`).
2. **`ImportCreatureMeshCommandlet`.**
   - Accept an optional `-normal=<png>`.
   - Import it as `T_Brennic_Normal` with the settings in section 5: `bFlipGreenChannel` true, `SRGB` false,
     `TC_Normalmap`, `TEXTUREGROUP_CharacterNormalMap`.
   - Log its size, sRGB, compression and flip on the `MAGEVR_CREATURE_IMPORT texture` line.
   - Keep the FBX's normals (`NormalImportMethod` ImportNormals, the default) and let UE build MikkTSpace tangents
     (`NormalGenerationMethod` MikkTSpace, the default). That matches the bake's basis. Do not import tangents: the
     FBX carries none (`rig_anim.py` exports without `use_tspace`).
3. **`BuildMaterial` (plain path only).** Sample the normal texture with sampler type Normal into the material's
   Normal input. The hound and the maw are unchanged, so leave the ember path alone.

**Captures:** Brennic at 8 m, front, three-quarter and back, under the T56 dusk, normal on vs off (`-nonormal` or a
material switch), for a blind critic. This lane's Blender verdict predicts SAME to MINOR at 8 m. Whether the dusk's low
key makes it MINOR or better is the open question. Look also for the inner-cloak streaks and an inverted green (folds
lit from below).

**D-G4 budget re-check, as a prediction to be measured:** a normal map adds a texture sample, not a mesh section. It
changes no pass's draw list, so draw calls should hold at **121 / 124 of 150**, the DRAWCALL-BUDGET-2026-10-25 policy
numbers. Keep that policy: one section per creature, no per-bone velocity, two-cascade shadows, LOD0 at the near ring.

- BasePass triangles should not change.
- Texture memory grows by one 2048 BC5 with mips: about 5.6 MB on desktop (about 2.5 MB for ASTC 6x6 on Quest), the
  only expected cost.
- If the count moves, that is a finding, not noise.

## 8. Staging (`C:/Users/kazda/mage-art-staging/brennic-cloak-normal-2026-10-25/`)

| Path | What |
|---|---|
| `finish-normal/` | **the run of record**: `Brennic_finish.{glb,blend,json}`, `Brennic_finish_record.json` (with `expect.drift: []`), cards, `T_Brennic_BaseColor.png`, `T_Brennic_Normal.png`, bake and hit-mask PNGs |
| `regress-flat/`, `regress-bake/` | the option-off regression runs |
| `scratch-geo-only/` | the geometry-only normal bake (TRELLIS normal texture unplugged), measurement only |
| `rig/` | `Brennic.fbx`, `Brennic_rigged.blend`, `Brennic_rig.json`, `rig_anim.log`, both textures |
| `evidence/` | `Brennic_turnaround.png`, `Brennic_game_640x480.png`, `Brennic_clip_{Idle,Cast,Hit,Death}.png` |
| `renders/`, `renders-rake/` | on/off renders, L1 and L2 |
| `critic/`, `critic_key.json` | the 12 blind pair sheets and the side key |
| `previews/` | the 8-bit map preview, the deviation heat map, self-look sheets (not shown to the critic) |
| `scripts/` | `probe_hi.py`, `nrm_probe.py`, `finish_geo_only.py`, `onoff.py` |

## 9. Tests

| Run | Result |
|---|---|
| `python -m unittest apps/vr/tools/assets/test_finish.py` | 26 of 26 (Python 3.12, numpy 2.4) |
| `node --test apps/vr/tools/mesh/finish-args.test.mjs` | 7 of 7 |
