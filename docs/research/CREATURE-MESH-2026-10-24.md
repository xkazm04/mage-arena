---
title: Cinder hound s7101 and mire maw s7202 - TRELLIS.2 meshes on the bake finish, beast fit, rig and critic
channel: vr
date: 2026-10-24
status: RAN - hound seed 42 and maw seed 7 finished on the bake finish to structural PASS, rigged (Idle, Attack, Hit, Death) and rendered; both fresh critic verdicts REWORK. Hound - bake equals raw, the losses (no glow, brown not dark red, no facets) are in the source. Maw - the gape is a real open pocket (not capped), the unseen back is a torn shell that finishing closes with a fan. Fixes - bake refill (finish_lib), per-mesh beast fit (beast_parts, rig_anim), finish.mjs defaults to bake. ComfyUI stopped. Nothing ships (DINOv3 hold).
sources: [docs/DECISIONS.md (2026-10-24 Visual approvals, Athena 17:04Z, 17:19Z, 17:24Z, 17:56Z), docs/research/CREATURE-MASTERS-2026-10-24.md, docs/research/BRENNIC-MESH-2026-10-24.md, docs/research/BRENNIC-MESH-REWORK-2026-10-24.md, apps/vr/tools/mesh/README.md, apps/vr/tools/mesh/beast_fit.py, apps/vr/tools/assets/finish.py, apps/vr/tools/assets/beast_parts.py, apps/vr/tools/assets/rig_anim.py]
---

# Creature meshes, 2026-10-24

**Outcome.** Both accepted creature masters went to finished, rigged and animated LOD0 meshes on the bake finish, one
after the other, on Wolf (RTX 4090). No Unreal process was started or touched, and no image was generated.

| | Cinder hound (s7101) | Mire maw (s7202) |
|---|---|---|
| TRELLIS seeds | 42 only (the raw card said finish) | 42, then 7 (seed 42 was shredded; see the maw section) |
| Finished card | **pass, 100**: 11,498 tris (budget 12,000), 1 component, watertight, 1.357 m | **pass, 100**: 17,484 tris (budget 20,000), 2 components, watertight, 1.249 m |
| Bake | 86 texels missed (0.004 %), black 0 / 0 | 293,009 missed (23.4 %), black 0 after the refill (174,922 before it) |
| After parts and rig | 11,796 tris, 26 bones, four clips | 17,708 tris, 11 bones, four clips |
| Critic (fresh, blind to which is raw) | **REWORK**: bake and raw are the same; the fire identity is missing from the source | **REWORK**: the bake beats the raw; open gape with teeth works, the back is a fan, eyes lost |

- **Structural passes are not quality claims.** The critics are the quality reading, and both say rework.
- **Finishing versus source.** For the hound, finishing lost nothing: the bake and the raw were judged equal at equal
  scale, so every gap is in the TRELLIS reconstruction. For the maw, finishing helped (it closed the raw mesh's
  see-through holes in the mouth and the back) but added the fan of the FILL step on the torn back.
- **Maw throat: not capped.** The raw TRELLIS mesh keeps the open gape. A section through the finished mouth shows a
  pocket 0.35-0.45 m deep behind the lips, and **the jaw bone opens onto that cavity**. Seed 42 failed the other way: no
  face at all.
- **Three code changes** were needed (below), each with tests and a regression that shows Brennic and the flat mode
  unchanged.
- **ComfyUI was stopped** at 18:30:23Z, straight after the last TRELLIS run.
- **Licence hold, unchanged.** TRELLIS.2 loads the DINOv3 weights. Nothing from this run ships until the owner rules on
  that licence: not the GLBs, the FBXs or the renders. This was local build and evaluation only.

Wall time: 18:01:56Z (sha256 check) to 18:58:24Z (the last maw evidence PNG), 56.5 min, including the code fixes.
All outputs are in `C:/Users/kazda/mage-art-staging/creature-mesh-2026-10-24/` (`<out>/` below). No binary is in git.

## Inputs

Copied from `C:/Users/kazda/mage-art-staging/creature-masters-2026-10-24/` and checked against the `.sha256` beside each
(`sha256sum -c`: OK) and against CREATURE-MASTERS-2026-10-24:

| Creature | File | sha256 |
|---|---|---|
| hound | `hound-s7101-medium.jpg` | `123bf7d3305f475a4b3782dc88a4d7e4718a900423c0233e9260eb7aab6edd1d` |
| maw | `maw-open-gape-s7202-medium.jpg` | `12a569aa7d0b0d16489506cda399c904118078f4b4e23358e71e6d1e9dd3e6a4` |

## Step 1: input gate

`node apps/vr/tools/mesh/input-gate.mjs <out>/<c>/<master>.jpg --out <out>/<c>/1-input-gate.json`

| Creature | Verdict / exit | Findings | Subjects; foreground share | Background | Margins L / T / R / B | Subject long side |
|---|---|---|---|---|---|---|
| hound | `incomplete` / 3 (1.8 s) | none | 1; 0.234 | opaque grey 0.7765, border 100 % near, side spread 0.007 | 15.3 / 3.5 / 7.4 / 4.7 % | 1,064 px |
| maw | `incomplete` / 3 | none | 1; 0.451 | opaque grey 0.8118, border 100 % near, side spread 0.005 | 6.6 / 7.8 / 5.2 / 8.6 % | 1,214 px |

Both are the proceed-as-ungated case: only the four see-only criteria are unmeasured. CREATURE-MASTERS scored them by
hand (hound 8, maw 8).

## Step 2: ComfyUI and TRELLIS.2

`comfy.mjs start` at 18:02:30Z brought ComfyUI 0.42.1 up as pid 9652 after 42 s. The guard read before the start: free
RAM 38.1 GB, GPU 7 %, VRAM outside ComfyUI 1,210 MiB, no Unreal process.

`node apps/vr/tools/mesh/comfy.mjs run <master> <label> <seed> <out>/<c>/s<seed>`, one job at a time:

| Run | Guard before (free RAM / GPU / other VRAM / queue) | TRELLIS s | VRAM peak (baseline, +delta) MiB | GLB bytes | sha256 |
|---|---|---|---|---|---|
| hound s42 (18:03:22Z) | 33.6 GB / 9 % / 1,586 / 0 | 105 | 10,281 (1,586, +8,695) | 46,835,580 | `af5d7e41fa06bf17542a020360120062cbee533c0231d5a5a8c015e0b137afc7` |
| maw s42 (18:22:01Z) | 32.1 GB / 15 % / 3,880 / 0 | 195 | 15,234 (4,716, +10,518) | 53,535,396 | `c59ef271894ece134c055f9da86cc18913f19e80cdd2436c2fd47363697f813f` |
| maw s7 (18:26:35Z) | 33.7 GB / 10 % / 3,610 / 0 | 211 | 14,522 (4,439, +10,083) | 58,978,704 | `f6fd79988c0c2190ba1df8f3c3736b25b173114598359d537041f6d7fde89f23` |

Free RAM never came near the 8 GB floor, and the guard cleared on its first look every time. The VRAM baseline rose to
about 4.4 GB during the maw runs (other desktop apps); it stayed under the guard's 6 GB.

**ComfyUI stopped.** After maw s7: `comfy.mjs free` (HTTP 200) at 18:30:22Z, `comfy.mjs stop` at 18:30:23Z (pid 9652
and its children 49472 and 32680 terminated). A `netstat` straight after still listed 8188 for a moment; the re-check
showed no listener, and `comfy.mjs status` reads "ComfyUI not answering on 8188". Every later step is Blender only.

## Step 3: raw cards

`node apps/vr/tools/mesh/mesh-card.mjs <glb> --stage raw --class <CinderHound|MireMaw> --out <dir>/3-raw-card.json`

| Run | Verdict | Tris | Components (faces) | Boundary / non-manifold / mis-wound edges | Height (TRELLIS units) | Codes |
|---|---|---|---|---|---|---|
| hound s42 | warn, 40 | 699,802 | 2 (363,216 + 336,586) | 0 / 21 / 0 | 0.520 | not-watertight, tris-over-budget, scale-off, origin-off |
| maw s42 | warn, 40 | 699,738 | 2 (687,864 + 11,874) | 19 / 272 / 0 | 0.463 | the same four |
| maw s7 | warn, 25 | 699,701 | 2 (685,693 + 14,008) | 65 / 975 / 19 | 0.453 | the same four + winding-inconsistent |

Every code is remedy `finish`; no card said `reroll`. Renders of each raw mesh (`<out>/<c>/renders/raw*`) are the look the
card cannot give.

- **Hound s42.** It reads well: four clear legs, the long tail, upright ears, glowing eyes, facing -Y already (rot 0).
  The lava cracks come out as dull rust veins, not glowing lines. One seed was enough.
- **Maw s42: shredded, and the face is missing.** Torn flaps and holes run through the whole shell. Where the gape,
  teeth, tongue and jaws should be there is a torn hollow (`maw/renders/raw-front.png`). The card grades it finish-only,
  because thin double-sided tatters do not trip its codes. **The second seed was spent on this visual reading, not on a
  card `reroll`:** a deviation from the brief's rule, made because the card is blind to shape (the mesh README says so)
  and the brief's cap of 2 seeds allowed it.
- **Maw s7: the pick.** The front shows the open gape, cream teeth on both jaws, the tongue, the dark throat and the
  yellow eyes. The back half, which the master never shows, is a torn shell with holes. It faces about +X (the master's
  three-quarter view), so it is finished with `--rot -50` (`maw/renders/rot-try.png`: -50 puts the gape square to the
  camera).

## Step 4: finishing (bake, 2048)

```
node apps/vr/tools/mesh/finish.mjs <out>/hound/s42/hound-s42.glb <out>/hound/finish-bake --class CinderHound --name CinderHound --mode bake --voxel 0.006 --budgets <out>/budgets-beasts.json
node apps/vr/tools/mesh/finish.mjs <out>/maw/s7/maw-s7.glb <out>/maw/finish-bake --class MireMaw --name MireMaw --mode bake --rot -50 --palette MireMawOpenGape --budgets <out>/budgets-beasts.json
```

- `--voxel 0.006` for the hound is the 2026-10-21 hound value (its legs and tail are 2-5 cm thick).
- `<out>/budgets-beasts.json` is a copy of `budgets.json` with only the two tris targets set to the HANDOFF rebuild lines
  (hound 11,500, maw 17,500), so that the parts added afterwards stay under the 12,000 and 20,000 budgets. The card
  budgets are unchanged.

| Measure | Hound | Maw (run of record) |
|---|---|---|
| finish.mjs seconds (raw card, finish.py, export, finished card) | 2.6, 24.6, 4.7, 0.9 | 24.3, 152.6, 7.7, 16.6 (machine busier; the texture step took 97 s, mostly the refill) |
| Floaters dropped at import / components | 0 / 2 (both halves substantial; the voxel remesh joins them) | 2 / 4 |
| SLAB, FILL | none | no SLAB (`MireMawOpenGape`); FILL star 811 of 4,608 directions missed through the holes |
| Voxel tris -> LOD0 | 321,776 -> 11,498 | 591,276 -> 17,484 (8 low floaters dropped) |
| UV share / texels per metre | 0.479 / 717 | 0.299 / 246 (20.75 m² of surface: the FILL star adds area) |
| Island texels | 2,009,624 | 1,251,604 |
| Missed by every ray (interior) | **86 (1)** | **293,009 (246,035)** |
| Black before the fill / where hit | 0 / 0 | 191,743 / 0 |
| Fill rings | 1 | 30 |
| Black after the ring fill | 0 | 174,922 |
| Refill rings / orphans given the median hit colour | - | 202 / 8,889 (sRGB 0.314, 0.224, 0.137) |
| **Black in the islands after the fill** | **0** | **0** |
| Hit texels darker than 0.08 (every channel) | 279 (0.01 %) | 0 |

**Finished cards.**

| Measure | Hound | Maw |
|---|---|---|
| Verdict / score | **pass / 100**, no findings | **pass / 100**, no findings |
| Triangles / vertices | 11,498 / 5,751 | 17,484 / 8,656 |
| Components | 1 | 2 (17,304 + 180; the 180-face piece is a loose lump under the body, 1 % of the faces, so a part, not a speck) |
| Watertight; boundary / non-manifold / mis-wound; degenerate | yes; 0 / 0 / 0; 0 | yes; 0 / 0 / 0; 0 |
| Height (band) | 1.35695 m (1.224-1.496) | 1.2492 m (1.125-1.375) |
| Min z / bbox centre xy | 0.00032 m / (-0.00015, -0.00014) | 0.00209 m / (-0.00008, -0.00043) |
| Extent x / y / z | 1.077 / 2.592 / 1.357 m | 2.395 / 2.246 / 1.249 m |
| GLB | 3,074,916 bytes, `deebc2d33e97d2cd4479b592aaf7e70fa1b842c28e70beb87771ab6164d5b956` | 3,901,468 bytes, `88ab646a5428af9960defefeffa48346ee66154dfd05e19e5d2a5a5859b0b5a5` |
| `T_<Name>_BaseColor.png` | `b36529c7f1c79e093cde253ba59087f5120e391eec9169b480a33fbb73ece08d` | `4b9282aa73def38b7cc9c33948205eb7fb281e7f5cda90aabd58c33849d47989` |

**Scale, for the owner or Athena.** The class heights are the 2026-10-21 sheet heights (hound 1.36 m to the ear tips,
maw 1.25 m to the top of the mound). At those heights the new key-art bodies are much bigger than the old ones:

- the hound is 2.59 m long, nose to tail tip (the 2026-10-21 hound was 1.93 m);
- the maw is 2.40 m wide and 2.25 m deep (the old one was 1.28 by 1.28 m; the greybox maw is 1.50 m wide).

The proportions follow the masters, so the height is the question. Nothing was rescaled here.

### The maw's first finishes (kept in `<out>/maw/finish-bake-try{1,2,3}-*`)

1. **The MireMaw set as it stood** (SLAB + FILL from the 2026-10-21 seed 42):
   - SLAB removed 47,679 sole faces from a mesh that has no ground slab, and filled 40 holes;
   - 309,452 texels were missed and **184,933 stayed black**. The card still passed.
   - The black was the FILL star patches across the torn back (`maw/renders/bake1` in try1). The front kept its open
     gape: FILL did not cap the mouth.
2. **Filling only whole unreached islands** left 166,993 black. Most misses were reachable, but the ring fill had grown
   Blender's black margin into them.
3. **Refilling only the black texels from island texels** gave 0 black, but left near-black streaks. The first fill had
   mixed margin black into the other misses too.

The fix that stands is in the next section.

## The code changes

| Commit | What | Proof |
|---|---|---|
| `7ed290e` fix(mesh) | `finish_lib.fill_misses` takes an optional `known` source mask and grows only into the misses; `fill_orphans` gives unreachable misses the median hit colour; finish.py refills **every** miss from the unfilled bake, from hit island texels alone, **only when the first fill leaves black**; new palette set `MireMawOpenGape` (MireMaw without SLAB) | `test_finish.py` 21 tests (2 new: island-only fill, orphan median). Brennic seed 7 re-finished: texture `024d4aaa...` and GLB `2233bdfd...`, byte-identical to the rework record. Hound re-finished: byte-identical. Flat Brennic: 697,440 hi tris, 57 floaters, 121,100 voxel tris, 23,990 tris, 11,987 verts, 1.7992 m and faces per material 3,737 / 5,947 / 8,869 / 312 / 4,879 / 246, as bf4a34e |
| `363225e` feat(mesh) | `apps/vr/tools/mesh/beast_fit.py`: the hound and maw joints, jaw/tongue rules and parts that beast_parts.py and rig_anim.py hard-coded, as defaults; a fit file per mesh replaces them, and must give every key. Maw rule x bound (`jawX`, `halfWidth`, `xRamp`; the defaults bound nothing). Fits `beast-fits/cinder-hound-s7101-s42.json`, `beast-fits/mire-maw-s7202-s7.json` | `test_beast_fit.py` 8 tests (the defaults verbatim, refusal of incomplete files, the committed fits). No-fit regression on the new hound below |
| `8f907a7` feat(mesh) | `finish.mjs` defaults to `--mode bake` (Athena 17:56Z); `--mode flat` builds exactly the pre-bake list; finish.py called directly stays flat by default (the HANDOFF rebuild lines); README | `finish-args.test.mjs` 5 tests updated |

**Why the beast fit was needed.** The tables were measured on the 2026-10-21 meshes and scaled with the height only.
On the new hound, the unchanged scripts did the following (`<out>/hound/baseline-unchanged/`):

- they cut the Ember mouth slit into the **neck** (the old nose tip, y -0.96, is the new neck; the new nose is at y -1.30);
- that pushed the mesh to **12,192 tris, over the 12,000 budget**;
- the old muzzle rule put 984 neck and head vertices on the jaw split, so the Attack clip tore the neck open and swung
  the whole face as the jaw (`baseline-unchanged/evidence/CinderHound_clip_Attack.png`).

The maw's gape, teeth and eyes are in the mesh already, so the old slot, tongue and eye parts do not apply to it at all.

**No-fit regression.** The edited scripts, without a fit file, on the same hound finish (`<out>/hound/regress-nofit/`)
against the unchanged scripts (`baseline-unchanged/`, run twice):

| Compared | Unchanged run 1 vs run 2 | Unchanged run 1 vs edited, no fit |
|---|---|---|
| Bones (rest head and tail), clip fcurves, materials, parts and rig records | identical | identical |
| Vertex set | identical (142 vertices in another order) | identical |
| Vertices whose weights differ / largest difference | 26 / below 0.00005 | 38 / below 0.00005 |

The EXACT boolean returns its vertices in a different order on each run, so the unchanged scripts are not bit-stable
themselves. The edited scripts differ from them by no more than they differ from themselves.

## Step 6: parts, rig and evidence

```
blender -b --python-exit-code 1 -P apps/vr/tools/assets/beast_parts.py -- <finish>/<Name>_finish.blend <rig> <Name> <CinderHound|MireMaw> <fit>
blender -b --python-exit-code 1 -P apps/vr/tools/assets/rig_anim.py -- <rig>/<Name>_parts.blend <rig> <Name> <rig>/<Name>.fbx <Hound|Maw> <fit>
cp <finish>/T_<Name>_BaseColor.png <rig>/
node apps/vr/tools/mesh/evidence.mjs <rig>/<Name>.fbx <out>/<c>/evidence <Name> <rig>/<Name>_rig.json Attack <CinderHound|MireMaw>
```

### How each fit was measured

**Hound** (`cinder-hound-s7101-s42.json`):

- **Legs:** 5 cm z-band centroids of the four leg columns.
  - The front paws are at y -0.96, splayed to x ±0.37 / -0.43.
  - The hind legs stand in a stride (left back at y 0.27-0.48, right forward at y 0.04-0.26), so the two sides differ.
- **Head:** a 1000 px/m side render with a grid (`hound/renders/side1000/head-grid.png`).
  - nose tip y -1.30, z about 0.99;
  - lip line z 0.985;
  - jaw hinge y -1.10;
  - the head turns slightly to the hound's left (+X).
- **Spine:** about 0.12 m under the back line.
- **Tail:** follows the measured tail to y +1.29.
- **Mouth slit:** the 2026-10-21 shape (12 x 17 x 0.7 cm), moved onto the new lip line, ending ahead of the jaw rule.

**Maw** (`mire-maw-s7202-s7.json`):

- **Mouth:** a front depth probe (rays along +Y every 2 cm) and a section at x 0.08 (`maw/renders/section-x008.png`).
  - lips at y -0.90 (upper, z 0.90) and -0.97 (lower, z 0.52-0.60);
  - throat wall at y -0.55.
- **Jaw:** the bone hinges at the wall and reaches the lower lip. The jaw rule runs from the wall forward, from z 0.32 up
  to 0.74, and inside |x| 0.42 (ramp 0.10). Without that bound the rule would take the front legs (x ±0.55-0.65,
  y -0.7).
- **Feet:** each foot bone runs along its side from the front paw to the back paw, so both paws stay planted (the 11-bone
  skeleton has one foot per side).
- **The one part:** a capsule tongue on the mouth floor (y -0.42 to -0.80, z 0.67), because the rig stretches only
  Tongue-material faces for the lash. No eyes and no slot are added.

### Records

| Measure | Hound | Maw |
|---|---|---|
| beast_parts.py (wall) | 1 part (mouth slit), 11,498 -> **11,796 tris**, 0 non-manifold edges; materials `M_CinderHound`, `M_CinderHound_Ember` | 1 part (tongue), 17,484 -> **17,708 tris** (9 s), 0 non-manifold; `M_MireMaw`, `M_MireMaw_Tongue` |
| rig_anim.py | 26 bones, height 1.3569 m, proxy heat on 116,736 tris (10 mm), 0 zero-weight proxy vertices, 0 nearest-bone fallbacks, 455 rule-weighted vertices (984 with the old tables), 4.6 s | 11 bones, 1.2492 m, proxy 215,624 tris (16 mm), 0 / 0, 4,202 rule-weighted vertices, 13.6 s (22 s wall) |
| Clips (frames; keys) | Idle 61 (0, 30, 60); Attack 36 (0, 9, 18, 23, 35); Hit 25 (0, 4, 10, 24); Death 61 (0, 6, 18, 32, 46, 60) | Idle 61; Attack 67 (0, 14, 30, 36, 42, 56, 66); Hit 25; Death 61 (0, 8, 24, 40, 60) |
| FBX | 1,468,124 bytes, `a650bc3e0ad23abc34291bd3da0dba35698598c430484c095aafc711d07e5449` | 1,276,364 bytes, `49d89873e414828fd76a87fb521c0d968d6a357f46cd8383b754e9b1af6ade64` |
| evidence.mjs | exit 0, 6 PNGs, 6.1 s | exit 0, 6 PNGs, 6.3 s |

### What the evidence shows, read by eye

- **Hound.**
  - Attack: the jaw opens on the Ember-lined slit at the muzzle tip.
  - Hit: a sideways recoil with a front leg lifted.
  - Death: the legs fold and it rolls onto its side.
  - The game view reads as a four-legged dog at 6 m.
- **Maw.**
  - Attack: the jaw opens wider and the tongue lashes well out of the gape, then reels in.
  - Death: it slumps forward and the face folds down into the body, so the gape closes in the last keys. That reads as
    a collapse, but a clip pass for this skeleton would tune it.
  - The game view reads as a squat creature with a red, tooth-ringed mouth.
- **Framing.** `render_evidence.py` (outside this lane's paths) frames the beasts for the 2026-10-21 sizes, so its
  turnaround crops the 2.59 m hound's nose and tail and the 2.40 m maw's sides. The clip strips and the game view are
  not cropped.

## Step 7: critics

Each critic was a fresh agent with no code and no docs, judging only images, one creature each. In every sheet,
A = the master, P = the bake finish and Q = the raw TRELLIS mesh scaled to the finished height. Raw and bake were
rendered with the same camera, light and px/m, scaled to about the master's px/m. The critic was not told which of P
and Q was which. The sheets are in `<out>/<c>/critic/`. A critic is still one model's reading.

### Hound (`full-side.png`, `head.png`, `front34.png`, game view, Attack strip)

| Feature | Bake (P) | Raw (Q) |
|---|---|---|
| Silhouette and proportions | partial: heavier real-dog build, thick forelegs, big round paws, a thin whip tail | partial (the same geometry) |
| Faceted plate look | **no**: smooth, organic, no flat facets or hard edges | **no** |
| Lava cracks glowing orange | **no**: thin dull rust veins, no glow | **no** (marginally crisper) |
| Dark red base colour | **no**: chocolate brown | **no** (a touch warmer) |
| Eyes glow | partial: small yellow dot | partial |
| Paws glow | **no** | **no** |
| Readable face | yes, as a realistic dog head | yes |

**Bake versus raw:** neither is clearly worse on any feature. They are "almost the same mesh with almost the same
texture", and no difference shows at game distance. **Finishing lost nothing; every gap is in the reconstruction.**
TRELLIS bakes the glowing cracks of the master as plain albedo with no emission, and smooths the faceted plates.

**Other defects:**

- nothing on the body glows;
- the cracks are branching veins, not wide seams between plates;
- the texture is soft and smeary;
- oversized toeless paws;
- the tail lacks the master's tip;
- the ear insides are dark;
- **the Attack clip's Ember mouth lining reads as a flat boxy orange block hanging under the jaw** (this one is ours:
  `beast_parts.py`'s slit and the jaw rule, not the source).

**Verdict: REWORK**, in the critic's order:

1. the fire identity is missing (no emissive cracks, paws or ears; weak eyes);
2. brown, not dark red;
3. no faceted plates (geometry and style);
4. proportion drift;
5. the boxy mouth lining.

It is not a reject: "the silhouette, the upright-eared head and the rigged animation are usable bases". At 6 m it is a
small dark dog, with the cracks invisible and nothing reading as fire.

### Maw (`full-master-view.png`, `mouth.png`, `front.png`, `back.png`, `section-x008.png`, game view, Attack strip)

| Feature | Bake (P) | Raw (Q) |
|---|---|---|
| Open gape | **yes**, about as wide as A's | **yes** |
| Dark throat reads as depth | partial: a dark recess, not capped, but flat-lit with no tunnel gradient | partial: background shows through a sliver behind the tongue |
| Tongue | partial: present in the lower jaw, but a lumpy raspberry mass | partial: its back edge torn away |
| Upper-jaw teeth | **yes**: blunt cream cones (right corner jumbled) | yes (one tooth holed) |
| Lower-jaw teeth | partial: present but splayed and merged | partial: pinholes between them |
| Yellow eyes | **no**: tiny amber dots | **no** |
| Warty brown hide | partial: dark olive-mud, warts mostly flat painted blotches | partial |
| Lighter tan jaw | partial: no cream-tan lip ring | partial (and a seam) |
| Silhouette | partial: squat and wide, legs planted; feet are lumps with no claws; a flap behind the head | partial (a curled ribbon there) |
| Unseen back | **no**: a pleated fan of triangles; a loose lump under the body | **no**: torn open, see-through into the mouth |

**Bake versus raw:** the raw is clearly worse on the throat, the tongue, the lower teeth and the back (see-through holes
from the game side). The bake is worse only on the crest behind the head (the fan's edge). **Finishing improved this
mesh; its remaining faults are the source's torn back, which FILL turns into a fan.**

**Section reading:** "an open cavity, not capped". The lips are at about y -0.92, the opening is about 0.4 m tall, and
the back wall is at y -0.55: about 0.35-0.4 m deep, roughly as deep as the opening is tall. It is a pocket that ends in
a wall, not a tunnel throat.

**Other defects:**

- a ragged pinkish mass at the rear left;
- the lashing tongue reads as a thin flat blade (ours: the capsule part);
- the fan crest stands up behind the head in every Attack frame and reads as "ears" at game distance;
- muddy olive colour with purple blotches, colder than A's caramel;
- no brow ridges, no pale claws.

**Verdict: REWORK** (P as the base), in the critic's order:

1. the back fan is broken geometry that shows in game;
2. the eyes are lost;
3. colour and texture are off (olive, blotchy, no tan lip ring, flat warts);
4. the attack tongue is a flat blade, and the resting tongue is raspberry;
5. disordered lower teeth, no claws.

It is not a reject: "the main bet paid off. The gape is open, the throat is a real pocket about as deep as the mouth is
tall, both jaws have teeth, and the front silhouette is right."

## Maw: the throat-capping risk

Athena warned that image-to-3D often fills in or caps a deep concave throat.

- **Seed 42 did not cap it either.** It failed the opposite way: the whole face came out as a torn hollow with no
  teeth, tongue or jaws (`maw/renders/raw-asis.png`, `raw-front.png`). Cause unproven. One guess, not checked: the
  workflow's BiRefNet background removal could have masked the dark throat as background.
- **Seed 7 keeps the gape open in the raw mesh:** the dark throat recess, the tongue and the teeth are all there.
- **Finishing kept it open.** FILL's rays from the body centre stop at the throat wall, so the star does not reach into
  the mouth. The voxel remesh at 1 cm does not close a 0.4 m opening.
- **Measured on the finished mesh** (`section-x008.png`, the front depth probe): a pocket 0.35-0.45 m deep behind the
  lips, the opening about 0.38 m tall.
- **Jaw bone.** The jaw is hinged at the pocket's back wall (y -0.50, z 0.71) and reaches the lower lip (y -0.97,
  z 0.56), so **it opens onto that cavity**: the Attack strip shows the gape widening with the tongue lashing out of it.
- **Recorded as a reconstruction finding:** the throat is a closed pocket, not the master's tunnel into darkness. It is
  shallower than still-08 suggests, but it is not capped.

## Staging (`C:/Users/kazda/mage-art-staging/creature-mesh-2026-10-24/`)

| Path | What |
|---|---|
| `hound/`, `maw/` | the master copies with `.sha256`, `1-input-gate.json` |
| `hound/s42/`, `maw/s42/`, `maw/s7/` | raw GLBs, `gpu-run.json`, `trellis-runs.jsonl`, `3-raw-card.json` |
| `<c>/finish-bake/` | the runs of record: `<Name>_finish.{glb,blend,json}`, `_finish_record.json`, both cards, `T_<Name>_BaseColor.png`, the unfilled bake and the hit mask |
| `maw/finish-bake-try{1,2,3}-*/` | the three superseded maw finishes (above) |
| `<c>/rig/` | `<Name>_parts.{blend,json}`, `<Name>.fbx`, `<Name>_rigged.blend`, `<Name>_rig.json`, logs, the texture copy |
| `<c>/evidence/` | turnaround, game view, the four clip strips |
| `<c>/critic/` | the sheets the critics judged |
| `<c>/renders/` | raw and finished turnarounds, the hound head grid, the maw rotation trial, depth probe and section |
| `hound/baseline-unchanged/`, `baseline-unchanged-run2/`, `regress-nofit/` | the unchanged-script runs on the new hound and the no-fit regression, with `rigdump.json` |
| `regress-brennic-bake/`, `regress-brennic-flat/`, `regress-hound-bake/` | the finish regressions |
| `budgets-beasts.json`, `scripts/` | the scratch budgets copy; `turn.py`, `sheet.py`, `critic_sheet.py`, `dump_verts.py`, `dump_rig.py`, `profile.py`, `legs.py`, `depth.py` |

## Tests

| Run | Result |
|---|---|
| `node --test apps/vr/tools/mesh/*.test.mjs` | 30 of 30 (gate-lib 20, blender-compat 5, finish-args 5) |
| `python -m pytest apps/vr/tools/assets/test_finish.py apps/vr/tools/mesh/test_beast_fit.py` | 29 of 29 (21 + 8) |

## Nothing ships

- **Licence.** The DINOv3 hold stands: TRELLIS.2 loaded `dino_v3_L_naf_fp32` for all three runs. Neither creature,
  nor any render of them, ships until the owner rules.
- **Asset ledger.** No ASSET-LEDGER row was written.
- **Pinned data.** No pinned combat data was touched.

## What would move the verdicts (not done here)

- **Hound fire identity.** An emissive mask for the cracks, eyes, paws and ear insides, from the master's colours or a
  hue key on the bake; a dark-red albedo shift; a facet pass (flat shading or a coarser decimate with hard normals).
  Each is a style or finishing question for Athena. The mouth-lining shape is ours to redo (a thinner slit, or shaded
  dark rather than flat Ember).
- **Maw back.** A rebuilt closed back (a mirror of the intact left half, a hand retopo of the back, or a second view for
  a multi-view reconstruction, which the creature rulings deferred), plus the eyes as modelled yellow parts. The attack
  tongue wants a rounder capsule and a pink-tan colour.
- **Scale.** The class heights against the new bodies (above): an owner or Athena call.
- **The evidence framing** for the larger beasts is in `render_evidence.py`, outside this lane's paths.
- **The UE import** waits for the Unreal slot (T56).
