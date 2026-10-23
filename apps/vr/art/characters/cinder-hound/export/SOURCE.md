# Cinder hound (T15, Bout 2 beast) - source record (draft, Lynx)

AI-assisted (Claude + owner); shipping rests on the owner's ruling of 2026-10-21 (docs/DECISIONS.md, asset originality) and its
licence ledger row, which a later run writes.

- Date: 2026-10-21, made on Lynx (Windows ARM64). No Unreal Engine was run; nothing here is a UE result.
- Subject: a lean, long-bodied hound of dark cinder with broad ember-orange bands (collar, shoulders, hips, ankles), glowing
  eyes and black paws, drawn to match `apps/vr/art/model-sheets/05-cinder-hound-turnaround.jpg` (silhouette) and the T15 card
  (low, long and fast, ember-orange accents). Its pose set follows sheet `07-cinder-hound-key-poses.jpg`.
- File: `SK_CinderHound.fbx` (FBX 7.4 binary, 1.4 MB). One skeleton (26 bones), one skinned mesh `SM_CinderHound` (11,784 tris,
  5,894 verts, one watertight component), 3 flat-colour materials, no textures, four takes `Idle` (61 frames), `Attack` (36),
  `Hit` (25), `Death` (61, last pose held) at 30 fps. +X forward, Z up, origin at the feet, 1.36 m to the ear tips, 1.93 m
  nose to tail tip.
- Repo scripts (`apps/vr/tools/assets/`): `leonardo.mjs --asset cinder-hound`, `trellis.mjs` + `trellis-api.json`,
  `finish.py` (palette set `CinderHound`), `beast_parts.py` (`CinderHound`), `rig_anim.py` (style `Hound`), `metrics.py`
  (profile `CinderHound`), `render_evidence.py` (framing `CinderHound`).

## Tools and versions

| Step | Tool | Version / model |
|---|---|---|
| Concept (front, turnaround) | Leonardo API | model `openai/gpt-image-2.5-flare`, `public: false`, prompt_enhance OFF |
| Image to 3D | ComfyUI 0.42.1 (http://127.0.0.1:8188) | TRELLIS.2 int8 (`trellis_2_int8_convrot.safetensors`, shape and texture VAEs bf16, DINOv3, BiRefNet), workflow `trellis-api.json` |
| Remesh, bake, parts, rig, animate, export | Blender 5.2.2 LTS (Windows ARM64) | voxel remesh, Cycles CPU bake, EXACT booleans, ARMATURE_AUTO heat weights, FBX exporter |

No MiniMax-H3 output and no FLUX model was used at any step.

## Concept (Leonardo)

Two calls, 28 credits for this asset (log: `apps/vr/art/characters/cinder-hound/leonardo-log.json`; the run cap was 112 credits
for the hound and the maw together, which spent 56; the project's total for the night was 238 of 400 after these calls).

- front (seed 4301, MEDIUM, 768x1376, 14 credits, 2026-10-21T12:25:33.881Z): `concept-front-s4301-medium.jpg`
- turnaround (seed 4302, MEDIUM, 1376x768, 14 credits, 2026-10-21T12:25:57.311Z): `concept-turnaround-s4302-medium.jpg`

The full prompts are in the log (`node apps/vr/tools/assets/leonardo.mjs prompt --asset cinder-hound --view front|turnaround`
prints them). The turnaround puts a wide side view first, then front and back.

## TRELLIS.2

- Input: the side panel of `concept-turnaround-s4302-medium.jpg` (columns 0-821 of 1376, full height, cropped in Blender; the
  crop is `work/hound_side_input.png` in the archive). A front view hides a quadruped's length, so the side panel was used.
  DecimateMesh (node 186) to 700,000 faces before UnwrapMesh and the 4096 texture bake. The queue was empty before each job;
  one job at a time; `POST /free` ran after the batch, before the Blender bakes.
- Run 1, seed 42 (105 s, 2026-10-21T12:27:28Z): rejected. Both hind legs came out as long sticks trailing back beside the tail,
  so only the front legs reached the ground.
- Run 2, seed 7 (120 s, 2026-10-21T12:37:30Z): used. Four legs on the ground, tail, ears; 699,737 tris, 5 components (3 small
  floaters dropped in Blender).

## Blender

1. `finish.py` (set `CinderHound`): import with merge_vertices; floaters under 0.5 % of faces dropped; scaled to **1.36 m**
   (see Scale) with the feet at z = 0; voxel remesh at 6 mm (164,928 tris; the legs and tail are 2-5 cm thick); collapse to
   11,500 tris; smart UV; Cycles CPU bake of base colour at 4096 (`work/CinderHound_bake_basecolor.png`, a record only). Each
   face takes the nearest part colour in CIE Lab (reference colours from the k-means of this bake), smoothed over its
   neighbours: `M_CinderHound_Cinder` (9,611 faces), `M_CinderHound_Ember` (679), `M_CinderHound_Paw` (1,210).
2. `beast_parts.py` (`CinderHound`): a mouth slit cut along the muzzle with an EXACT boolean (a flat ellipsoid 1.4 cm tall at the
   lip line, closing towards the cheek), lined with the Ember material, so the jaw opens onto a glowing mouth. **Hand-authored**
   parametric geometry, no generated source. 284 tris added; the mesh stays one watertight piece.
3. `rig_anim.py` (style `Hound`): a quadruped chain fitted to the mesh profile: `root`, `pelvis`, `spine_01-03`, `neck_01-02`,
   `head`, `jaw`, `tail_01-03`, front legs `front_upper/front_lower/front_paw_l|r`, hind legs
   `back_thigh/back_shin/back_hock/back_paw_l|r` (26 bones). Heat weights on a watertight 10 mm voxel proxy (60,160 tris),
   transferred by nearest proxy vertex (0 fallback vertices), smoothed x4, limited to 4 influences, normalised; then a rule
   splits the muzzle (627 vertices): below the lip line to `jaw`, above it to `head`, and the weights are limited and normalised
   again. The four clips are hand-keyed (below). Each clip is an NLA strip, so each FBX take carries the clip's own name.
4. `metrics.py` re-imports the FBX and writes `metrics.json` (profile `CinderHound`); `render_evidence.py` renders the evidence
   from the FBX.

## Clips (30 fps, hand-keyed)

- `Idle` (keys 0, 30, 60; loop): sheet 07 pose 1, low and watchful: head carried down at back level, legs a little bent, slow
  breath, a look to one side and back, the tail low and swaying.
- `Attack` (keys 0, 9, 18, 23, 35): the **ember spit** of the VR overlay (`apps/vr/data/vr/combat.vr.json`, cinder_hound mode
  spit, windupS 0.6, read only): crouch and draw the head back and down (frame 9), thrust the head forward with the jaw open
  40 degrees at the release (frame 18 = 0.6 s), hold, recover by frame 35. The pinned bite (windupS 0.35) is not what the VR
  game plays (DF-004 option A), so the clip follows the overlay.
- `Hit` (keys 0, 4, 10, 24): sheet 07 pose 3: knocked sideways, the spine bent, the head jerked up and away, the jaw open, the
  tail tucked, recover.
- `Death` (keys 0, 6, 18, 32, 46, 60; last pose held): sheet 07 pose 4 without the break-up (that is a VFX job in UE): rears with
  the jaw open, the legs buckle, rolls onto its left side and lies with the legs out.

## Scale (recorded)

The A04 sheets carry no unit and their grids differ (sheet 05: 21.3 px; sheets 03 and 06: 42.6 px). The scale is the pixel
scale of sheet 03, where the conscript is a known 1.80 m over 626 px (2.875 mm/px, which is also the 42.6 px grid that sheets
03 and 06 share; sheet 05's grid is that grid halved). Sheet 05, side view: ear tips to paws 473 px -> **1.36 m**; nose to
tail tip 650 px -> 1.87 m. Measured on the FBX: height 1.359 m (-0.1 %), length 1.926 m (+3 %). Note: the in-game greybox
hound is a 0.40 x 0.40 x 1.20 m box (`SessionPresentation.cpp`); this scale follows the sheet, not the greybox.

## Known limitations

- Greybox finish: flat colours from a per-face split, so the ember bands have ragged, saw-tooth edges at 11.8k tris.
- The mouth slit's Ember lining shows as a thin orange line along the closed mouth. That reads as a glowing mouth, and it is a
  side effect of the cut.
- No IK: in the crouching keys the paws slide slightly along the floor; the ear and tail tips are rigid.
- Death ends lying on the side, without the body breaking into embers (sheet 07 pose 4): the break-up and every ember or spark
  effect belong to UE VFX.
- No LODs beyond LOD0. The +X forward check uses the head bone after the FBX re-import, whose tail Blender re-orients (head dx
  0.067 m); the paws give left +Y, right -Y.
