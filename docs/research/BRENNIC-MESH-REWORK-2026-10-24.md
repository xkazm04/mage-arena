---
title: Brennic mesh rework - texture-preserving finish (finish.py --mode bake), seed 7 re-run
channel: vr
date: 2026-10-24
status: RAN - bake mode merged beside the flat mode (021910b); seed 7 re-finished, re-rigged and re-rendered; structural card PASS (unchanged numbers); fresh critic verdict REWORK (brooch and buckle bronze: yes; face, sandal straps, bracer bands: no). At equal scale the raw TRELLIS mesh fails the same three, so the bake did not lose them. Nothing ships (DINOv3 hold).
sources: [docs/research/BRENNIC-MESH-2026-10-24.md, apps/vr/tools/mesh/README.md, apps/vr/tools/assets/finish.py, apps/vr/tools/mesh/finish_lib.py, apps/vr/tools/mesh/finish.mjs, apps/vr/tools/mesh/budgets.json, docs/concepts/VISUAL-PLAN.md (c)]
---

# Brennic mesh rework, 2026-10-24

**Outcome.** `finish.py` has a second mode, `bake`. It keeps every geometry step of the flat finish, then keeps the
TRELLIS albedo as the LOD0's texture instead of replacing it with six flat colours. The flat mode is still there and is
still the default. Seed 7 was re-finished in bake mode from the same raw GLB, then re-rigged, re-rendered and judged by
a fresh critic.

- **Structural card: PASS, numbers unchanged.** The geometry path is the same, so the card is the same. **That is not a
  quality claim.**
- **Critic verdict: REWORK.**
  - The bake fixes what per-face classification broke: a bronze brooch where the flat finish had a skin-coloured
    teardrop, a bronze buckle where it had an iron-grey one, brows and beard in place, and no saw-toothed six-colour
    regions.
  - It does **not** give a readable face, sandal straps or bracer bands.
- **What the follow-up found.** The critic then judged the raw seed-7 mesh at the same scale and camera. The raw mesh
  fails the same three. The first run read the raw face, straps and bands from small renders (the raw is 1 m tall in
  that camera); at equal scale they are not there. The rest of the gap is the source, not finishing.
- **Licence hold, unchanged.** Nothing from this run ships until the owner rules on the DINOv3 weight licence. No
  TRELLIS run, no new seed, no image-generation call and no Unreal process was used in this lane.

All outputs are in `C:/Users/kazda/mage-art-staging/brennic-mesh-rework-2026-10-24/` (`<out>/` below). No binary is in
git.

## The change (commit `021910b`)

| File | What |
|---|---|
| `apps/vr/tools/assets/finish.py` | `mode=bake` branch after the shared bake; the flat code is unchanged, only indented under `else:` |
| `apps/vr/tools/mesh/finish_lib.py` | the Blender-free half: option parsing (`mode`, `clamp`, `strength`, `levels`, `cage`, `ray`, `margin`), sRGB/Lab, the style clamp, UV coverage, miss accounting, miss fill, texel density |
| `apps/vr/tools/mesh/finish-args.mjs` | the finish.py argument list; `--mode flat` builds exactly the pre-bake list |
| `apps/vr/tools/mesh/finish.mjs` | `--mode`, `--clamp`, `--strength`, `--levels`, `--cage`, `--ray`, `--margin` |
| `apps/vr/tools/mesh/export_glb.py` | embeds the texture (PNG) when the mesh has one; flat GLBs stay image-free |
| `apps/vr/tools/mesh/budgets.json` | `finish.bakeTextureRes: 2048` (why: below) |
| `apps/vr/tools/mesh/README.md` | "Finish modes" |
| tests | `apps/vr/tools/mesh/finish-args.test.mjs` (5) and `apps/vr/tools/assets/test_finish.py` (19, numpy only) |

### What bake mode does, after the shared steps

The shared steps are: import, floaters, scale to 1.8 m with the feet at z = 0, voxel remesh 0.01 m, collapse to
24,000, triangulate, clean, and smart UV (66 degrees, margin 0.003).

1. **Repack.** `bpy.ops.uv.pack_islands(rotate=True, margin_method='FRACTION', margin=0.003, shape_method='CONCAVE')`.
   Smart project alone used 30.3 % of the UV square on seed 7 (523 texels/m at 2048). The repack gives 42.4 %
   (619 texels/m). The margin is about 6 px between islands at 2048.
2. **Hit mask.** Every material slot of the source is swapped for a white emission. Then an `EMIT` bake runs
   selected-to-active, with the same cage and no margin, into a float image. A texel inside a UV island that no ray
   reached reads 0. The mask is saved as `Brennic_bake_hitmask.png`.
3. **Albedo bake.** The source's Metallic, Transmission and Emission are zeroed first (the step the flat mode already
   had: a metallic surface would otherwise bake dark in a diffuse colour pass). Then Cycles CPU `DIFFUSE` runs with
   **colour pass only** (no direct, no indirect), 1 sample, selected-to-active.
   - cage extrusion 0.02 m;
   - max ray distance 0.08 m;
   - margin 16 px, EXTEND;
   - 2048 x 2048.
4. **Misses.** `finish_lib.uv_coverage` rasterises the LOD0 UV triangles at texel centres. The misses are the island
   texels whose hit-mask value is below 0.5. They are filled ring by ring from their known 8-neighbours. Every island
   texel is then checked for black (all channels <= 1.5/255).
5. **Optional style clamp.** See below.
6. **One material.** `M_Brennic` takes `T_Brennic_BaseColor.png` as its base colour, with roughness 0.8, metallic 0 and
   one material slot for all 23,990 faces.

### Bake size: 2048, and why

| Size | Texels per metre on Brennic | ASTC 6x6 with mips (approx.) |
|---|---|---|
| 1024 | about 310 | about 0.6 MB |
| **2048** | **619** (measured) | **about 2.5 MB** |

- **At the duel distance.** The rival stands 6 m away (`render_evidence.py`'s game view). On a Quest 3 (about 20 px
  per degree at the centre of the lens, an approximation) 1 m at 6 m spans about 180 screen px. Both sizes
  oversample that: 1024 by about 1.7 texels per pixel, 2048 by about 3.4.
- **Closer than that.** At 2 m (an intro or a close kill camera, 540 px/m on screen), 1024 undersamples (0.57 texels
  per pixel) and 2048 is about 1:1. The critic's close-up renders (1,429 px/m) need more than either.
- **Downsizing is free; upsizing is not.** UE can serve a 2048 texture at 1024 on Quest through its texture LOD group
  or max size without a re-bake. Going the other way needs a re-bake.
- **Source size.** The source albedo is 4096 on a 697k-triangle mesh, so 2048 is not past what it carries.

The flat mode's classification bake stays at 4096 (`finish.bakeRes`): it never ships.

### How the bake was checked for black and missed texels

| Seed 7, 2048 | Value |
|---|---|
| Island texels (UV coverage) | 1,772,680 |
| Missed by every ray (hit mask) | 11,312 (0.64 %); 8,289 of them not on an island's edge texel |
| Their albedo before the fill | none black: min channel max 0.129, mean sRGB (0.297, 0.172, 0.147). Blender's EXTEND margin already reached into the interior holes |
| Fill | 4 rings |
| Black texels in the islands, before and after the fill | **0 / 0** |
| Black texels where a ray hit | 0 |

**Where the misses are.** A face counts here when the texel at its UV centroid missed: 275 of 23,990 faces. By height:

| Height band | Missed faces |
|---|---|
| 0.50-0.90 m (inner cloak hem, between the legs) | 149 |
| 0.90-1.20 m | 58 |
| 1.20-1.50 m | 58 |
| 1.50-1.85 m (the head) | 3 |
| below 0.50 m | 7 |

Of the 275, 122 face front.

**Cage choice.** A scratch run with cage 0.03 m and ray 0.12 m (`<out>/scratch-cage03/`) halved the misses: 6,149
texels, 4,372 interior, 190 faces. It was not adopted. A ray that starts 3 cm out can start beyond the cloak's inner
wall and paint cloak onto the tunic beneath it, and the misses it saves are filled anyway. The 0.02 / 0.08 cage is
the one the flat mode has used since T38.

**A first bake mode run, before the repack, is not kept.** With smart project only (30.3 % UV share) it counted 8,012
missed texels, and 14 island texels stayed black after the fill. Their cause was not established before that layout
was replaced. With the repacked layout the count is 0, and the black-texel check is now a recorded number on every
run (`bake.dark_in_islands_after_fill`).

### The style clamp (optional, off by default)

`--clamp posterize|hue|both`, `--strength` (0 < s <= 0.6, default 0.35), `--levels` (>= 4, default 8).

- **posterize** moves CIE L* a fraction `s` of the way to the nearest of `levels` steps. Hue and chroma are untouched.
- **hue** turns each texel's Lab hue a fraction `s x min(1, chroma / 8)` of the way to the nearest anchor hue. Lightness
  and chroma are untouched, and greys do not move.
  - The anchors are the palette set's colours plus the VISUAL-PLAN (c) accents: bronze, gold, crimson and iron grey.
  - The element colours (water turquoise #19E0E0, fire vermilion #FF3B1F) are reserved for magic and are never
    anchors.
- **It never replaces the texture with flat colours.**
  - Strength is capped below 1 and neither step touches chroma.
  - `test_finish.py` checks that at the strongest allowed setting (s = 0.6, 4 levels), more than 60 % of the local L*
    detail and more than 300 distinct colours survive.
  - `clamp.detail_kept` is recorded on every run.

On seed 7, `--clamp both` at the defaults gives `detail_kept` 0.934 with a mean shift of 0.0079 sRGB. The critic found
it almost identical to the unclamped bake: slightly blotchier skin, a pale forehead patch and a little more streaking.
**The unclamped bake is the run of record.**

## Commands (the seed-7 re-run)

The raw GLB is unchanged: `C:/Users/kazda/mage-art-staging/brennic-mesh-2026-10-24/s7/brennic-s7.glb`, sha256
`bef2ed78d4d34ccaed2f5544bc95d551fbf843725f6f53d6243c710708e00698`, the same sha as the first run's record. The
re-run started at 17:31:01Z.

```
node apps/vr/tools/mesh/finish.mjs <s7>/brennic-s7.glb <out>/finish-bake --class Humanoid --name Brennic --mode bake
node apps/vr/tools/mesh/finish.mjs <s7>/brennic-s7.glb <out>/finish-bake-clamp --class Humanoid --name Brennic --mode bake --clamp both
blender -b --python-exit-code 1 -P apps/vr/tools/assets/rig_anim.py -- <out>/finish-bake/Brennic_finish.blend <out>/rig Brennic <out>/rig/Brennic.fbx Cast
cp <out>/finish-bake/T_Brennic_BaseColor.png <out>/rig/      # rig_anim.py writes the FBX with path_mode STRIP
node apps/vr/tools/mesh/evidence.mjs <out>/rig/Brennic.fbx <out>/evidence Brennic <out>/rig/Brennic_rig.json Cast
```

Blender is 4.2.1 LTS throughout (`blender.mjs`; `evidence.mjs` adapts the EEVEE id as b21fb22 does).

**Flat-mode regression.** `finish.mjs` without `--mode` (`<out>/scratch-flat-regression/`) reproduced bf4a34e's
`finish.py` record field for field:

- 697,440 hi tris, 57 floaters dropped, 121,100 voxel tris;
- 23,990 tris, 11,987 verts, height 1.7992 m;
- the same faces per material: Tunic 3,737, Cloak 5,947, Leather 8,869, Iron 312, Skin 4,879, Hair 246;
- the same face k-means.

## Card numbers, before and after

| Measure | bf4a34e flat finish | This bake finish (`Brennic_card_finished.json`) |
|---|---|---|
| Verdict / score | pass / 100, no findings | **pass / 100, no findings** |
| Triangles | 23,990 | 23,990 (Humanoid band 18,000-30,000) |
| Vertices | 11,987 | 11,987 |
| Components | 1 | 1 |
| Watertight; boundary / non-manifold / mis-wound edges; degenerate faces | yes; 0 / 0 / 0; 0 | yes; 0 / 0 / 0; 0 |
| Height | 1.79923 m | 1.79923 m (band 1.62-1.98 m) |
| Min z / bbox centre xy | 0.00027 m / (-0.00054, -0.00013) m | 0.00027 m / (-0.00054, -0.00013) m |
| Materials | 6 flat colours, no image | 1, `M_Brennic` + `T_Brennic_BaseColor.png` 2048 |
| Finished GLB | 896,556 bytes, sha256 `7c6fae2a...` | 3,563,700 bytes (texture embedded), sha256 `2233bdfda646c0cc7ea1f01a2ec443d6425b55284e837dbc3047277c47165a36` |
| finish.py seconds | 20.1 (4096 bake 10.8 s) | 34.1 (UV and repack 12.8 s, hit-mask bake 5.3 s, albedo bake 4.9 s, texture 4.0 s) |

The clamped variant's card is identical; its GLB is 3,588,000 bytes, sha256 `db35af80...`.

## Rig and evidence

`rig_anim.py` ran unchanged and exited 0 in 6.4 s wall. Its record matches the first run:

- 23 bones, height 1.7992 m, arms 57.2 degrees down on both sides;
- proxy heat on 42,752 tris, 0 zero-weight proxy vertices, 0 nearest-bone fallbacks;
- the same four clips (Idle 61, Cast 47, Hit 25, Death 61 frames) with the same keys.

The FBX is 1,895,580 bytes, sha256 `dc0a4f38eef1b8486f72e1ec81859bcba1605be3df412c4c6c72368b203967b0`.

`evidence.mjs` exited 0 in 5.9 s with all six PNGs. The turnaround shows the textured figure, so the texture survives
the FBX round trip once it sits next to the FBX.

## Renders

The scratch scripts are in `<out>/scripts/`, outside the repo.

- **turn.py**: the first run's script, copied unchanged.
- **sheet.py**: the first run's script, copied unchanged.
- **heads.py**: uses the first run's head crop, box (160, 0, 400, 260) of the 560x1000 render, scaled x2. The box was
  recovered from its `cmp-head.png` by image matching, mean error 0.05/255.
- **rawhead.py**: raw crop box (216, 226, 344, 369), mean error 1.9/255, with the left side view.
- **closeup.py**: turn.py's camera and light at 3x (1680x3000), front and back. With a height argument it scales the
  raw mesh to the finished frame.
- **critic_crops.py**: the same metric box cut from the master (676 px/m, feet at y 1289, both measured by background
  segmentation) and from each close-up.

The same sheets as the first run, in `<out>/renders/`:

- `cmp-front-back.png`: master front | bake | raw | master back | bake | raw;
- `cmp-head.png`;
- `raw-head.png`;
- `bake-8views.png` (the first run's `fin-8views.png`);
- `raw-8views.png`.

Added in this run:

- `cmp-flat-bake-raw.png`: flat front | bake front | flat back | bake back;
- `cmp-head-flat-bake.png`: master | flat | bake head.

## Critic

The critic was a fresh agent with no code, no docs and no knowledge of which variant was which. It judged only the
images:

- per-feature crop sheets `<out>/critic/*.png`: A = master, B = bake, C = bake + clamp;
- `cmp-front-back.png`, `cmp-head.png` and `bake-8views.png`;
- `bake-clamp/clamp_0_front.png`;
- `evidence/Brennic_game_640x480.png`.

The critic is still one model's reading.

| Feature | Bake (B) | Image | Bake + clamp (C) | What the critic saw |
|---|---|---|---|---|
| Face readable (eyes, brows, mouth) | **no** | `critic/face.png` | no | Brows read well. The eyes are tiny white specks with no iris or pupil. No mouth line: a skin patch in the beard under the moustache |
| Brooch bronze | **yes** | `critic/brooch-collar.png` | yes | Present, with an ochre/bronze rim. An off-white, bone-coloured blotch covers about half its face, and the domed relief is flattened |
| Sandal straps visible | **no** | `critic/sandals.png` | no | Feet read as a dark brown sock or boot with toes through a hole; no crossing straps or ankle band |
| Bracer bands visible | **no** | `critic/bracers.png` | no | A plain brown cuff with mottled shading; no separate stacked bands |
| Buckle bronze | **yes** | `critic/belt-buckle.png` | yes | Present and muted khaki-brass, not iron grey. The shape is ragged, with an irregular opening |

**Other defects the critic listed:**

- The iron collar is a matte blue-grey smear with no ring edge or plate.
- Jagged cloak-to-tunic edge across the chest, with red flecks at the cloak and collar edges.
- The hair is a smooth helmet with no curls.
- A dark scribble on the back of the neck.
- Colour drift: the tunic is a desaturated maroon against the master's brick red, the whole figure is muddier, and the
  skin is pinker.
- No hood on the back, and a bell-shaped cloak flare.
- A small dark tab at the cloak hem.
- Faint shin wraps.
- The belt has lost its holes and keeper.
- Fuzzy hem edges.

At game distance (`Brennic_game_640x480.png`, about 50 px tall) the tunic, cloak, forearms and shins read; the face,
brooch and buckle do not, as expected at that size. The missing hood is the cloak gather that the BRENNIC-MV-MASTER
record ruled out ("front wins").

### Verdict: REWORK

The critic's reasons, in its order:

1. no readable eyes or mouth;
2. the "few broad bands" rule fails on the straps, bracers, collar plate and shin wraps;
3. texture artefacts: the brooch blotch, saw-toothed cloak edges, the neck scribble and the ragged buckle;
4. colour and shape drift.

It ruled REWORK rather than reject because the silhouette, palette roles and proportions are close.

### Follow-up: is it the bake or the source?

The same critic judged `<out>/critic/raw-vs-bake/*.png`: A = master, R = the raw seed-7 mesh scaled to 1.7992 m in
the same camera and light, B = bake.

| Feature | Raw (R) | Bake clearly worse than raw? | Note |
|---|---|---|---|
| Face readable | **no** | no | The same eye specks and skin patch for a mouth. The raw keeps curl relief in the hair, which the bake smooths |
| Brooch bronze | yes | **yes** | The raw reads polished gold/bronze with a highlight; the bake shows a flat cream blotch there |
| Sandal straps | **no** | no | Almost identical: a sock or boot shape, no straps |
| Bracer bands | **no** | no | A plain cuff in the raw as well |
| Buckle bronze | yes | no | The same beige-brass; the raw's frame is a little crisper |

The critic's reading: where the bake is worse (the collar's dark gloss and relief, the brooch's highlight, the hair
curls, small tunic folds), the detail comes from geometry plus metallic and gloss shading, not from painted colour.
The face, the straps and the bands are missing in the raw in both colour and shape, so the bake did not cause them.

This session's own look at `raw-vs-bake/face.png` and `brooch-collar.png` agrees. That look is anchored, so it is
recorded, not counted.

**What this changes in the record.** BRENNIC-MESH-2026-10-24 said the raw seed-7 mesh "carries the face, the brooch,
the collar and the sandal straps in its texture". At the scale the critic now judged it, the brooch and buckle are
there, the collar is there as shape plus metal response, and the face, straps and bands are not. The finishing fix
was still needed: per-face classification is what made the brooch skin and the buckle iron, and it made the
saw-toothed regions. It is now done, and it is all a bake can recover.

## Staging (`C:/Users/kazda/mage-art-staging/brennic-mesh-rework-2026-10-24/`)

| Path | What |
|---|---|
| `front-s6101-medium.jpg`, `back-s6103-medium.jpg` | copies of the masters (the critic's reference) |
| `finish-bake/` | **the run of record**: `Brennic_finish.{glb,blend,json}`, `Brennic_finish_record.json`, `Brennic_card_raw.json`, `Brennic_card_finished.json`, `T_Brennic_BaseColor.png` (sha256 `024d4aaa...`), `Brennic_bake_basecolor.png` (the bake before fill), `Brennic_bake_hitmask.png` |
| `finish-bake-clamp/` | the same with `--clamp both` |
| `scratch-cage03/`, `scratch-flat-regression/` | the cage comparison and the flat-mode regression run |
| `rig/` | `Brennic.fbx`, `Brennic_rigged.blend`, `Brennic_rig.json`, `rig_anim.log`, the texture copy |
| `evidence/` | `Brennic_turnaround.png`, `Brennic_game_640x480.png`, `Brennic_clip_{Idle,Cast,Hit,Death}.png` |
| `renders/` | the comparison sheets above, `bake/`, `bake-clamp/`, `raw-s7/`, `closeup/` |
| `critic/`, `critic/raw-vs-bake/` | the per-feature sheets the critic judged |
| `scripts/` | `turn.py`, `sheet.py`, `heads.py`, `rawhead.py`, `closeup.py`, `critic_crops.py`, `probe_raw.py`, `dark_probe.py` |

## Tests

| Run | Result |
|---|---|
| `node --test apps/vr/tools/mesh/*.test.mjs` | 30 of 30 (gate-lib 20, blender-compat 5, finish-args 5) |
| `python -m unittest apps/vr/tools/assets/test_finish.py` | 19 of 19 (Python 3.12, numpy 2.4) |

## Not done here, and what would move the verdict

- **Metallic, roughness and normal.** The bake carries base colour only, so the collar's iron sheen and the brooch's
  shine are lost (the critic's one "bake worse than raw" finding). A follow-up can bake the source's
  metallic/roughness (and a normal map) onto the same UVs and feed them to the material. The finish slot "matte, one
  flat colour per material" in VISUAL-PLAN (c) argues against it, so it is a style question first.
- **Face, straps, bands.** They are absent in the reconstruction itself, so no finishing mode can recover them. The
  routes are a new input or seed (excluded from this lane), geometry or texture paint-over, or a multi-view
  reconstruction. Each is a separate ask.
- **The UE import.** T56 holds the one Unreal slot.
