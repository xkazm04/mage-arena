# Mire maw (T15, Bout 2 beast) - source record (draft, Lynx)

AI-assisted (Claude + owner); shipping rests on the owner's ruling of 2026-10-21 (docs/DECISIONS.md, asset originality) and its
licence ledger row, which a later run writes.

- Date: 2026-10-21, made on Lynx (Windows ARM64). No Unreal Engine was run; nothing here is a UE result.
- Subject: a big squat toad-like bog mound with a wide mouth slot, a moss cap over the head and back, a pale belly, short
  legs with wide feet, two eyes on the brow, drawn to match `apps/vr/art/model-sheets/06-mire-maw-turnaround.jpg` (silhouette)
  and the T15 card (big squat mound with a mouth slot, bog green-brown).
- File: `SK_MireMaw.fbx` (FBX 7.4 binary, 1.3 MB). One skeleton (11 bones), one skinned mesh `SM_MireMaw` (19,786 tris,
  9,883 verts, one watertight component), 7 flat-colour materials, no textures, four takes `Idle` (61 frames), `Attack` (67),
  `Hit` (25), `Death` (61, last pose held) at 30 fps. +X forward, Z up, origin at the feet, 1.25 m tall.
- Repo scripts (`apps/vr/tools/assets/`): `leonardo.mjs --asset mire-maw`, `trellis.mjs` + `trellis-api.json`, `finish.py`
  (palette set `MireMaw`), `beast_parts.py` (`MireMaw`), `rig_anim.py` (style `Maw`), `metrics.py` (profile `MireMaw`),
  `render_evidence.py` (framing `MireMaw`).

## Tools and versions

| Step | Tool | Version / model |
|---|---|---|
| Concept (front, turnaround) | Leonardo API | model `openai/gpt-image-2.5-flare`, `public: false`, prompt_enhance OFF |
| Image to 3D | ComfyUI 0.42.1 (http://127.0.0.1:8188) | TRELLIS.2 int8 (`trellis_2_int8_convrot.safetensors`, shape and texture VAEs bf16, DINOv3, BiRefNet), workflow `trellis-api.json` |
| Remesh, fill, bake, parts, rig, animate, export | Blender 5.2.2 LTS (Windows ARM64) | voxel remesh, Cycles CPU bake, EXACT booleans, ARMATURE_AUTO heat weights, FBX exporter |

No MiniMax-H3 output and no FLUX model was used at any step.

## Concept (Leonardo)

Two calls, 28 credits for this asset (log: `apps/vr/art/characters/mire-maw/leonardo-log.json`; its `totalCredits` 56 is the
hound and maw cap group; the run cap was 112; the project's total for the night was 238 of 400 after these calls).

- front (seed 4401, MEDIUM, 1376x768, 14 credits, 2026-10-21T12:26:27.597Z): `concept-front-s4401-medium.jpg`
- turnaround (seed 4402, MEDIUM, 1376x768, 14 credits, 2026-10-21T12:26:42.523Z): `concept-turnaround-s4402-medium.jpg`

The full prompts are in the log (`node apps/vr/tools/assets/leonardo.mjs prompt --asset mire-maw --view front|turnaround`).
The concepts ask for no reeds and no thin spikes (the sheet's cattails would only become floaters).

## TRELLIS.2 (three runs; none kept the face)

DecimateMesh (node 186) to 700,000 faces before UnwrapMesh and the 4096 texture bake; the queue was empty before each job; one
job at a time; `POST /free` ran after the batch, before the Blender bakes.

- Run 1, front view, seed 42 (390 s, 2026-10-21T12:29:19Z): **used.** A closed mound with legs and feet, the moss cap and the
  belly, but no face (no lips, eyes or nostrils), standing on a fused 1 m ground slab.
- Run 2, front view, seed 7 (225 s, 2026-10-21T12:39:31Z): rejected, an open shell with holes.
- Run 3, the side panel of the turnaround (columns 470-904, `work/maw_side_input.png`), seed 42 (240 s, 2026-10-21T12:43:55Z):
  rejected, an open shell with holes.

The face was made by hand in Blender instead (`beast_parts.py`, below). No fourth TRELLIS run was made.

## Blender

1. `finish.py` (set `MireMaw`): import with merge_vertices; the fused ground slab dropped (faces wholly in the bottom 6 mm of
   import units, 160,616 faces) and the sole holes it leaves closed; floaters under 0.5 % of faces dropped; scaled to **1.25 m**
   (see Scale) with the feet at z = 0; voxel remesh at 10 mm. The seed-42 skin remeshes into a hollow shell 1.5-5 cm thick, so
   `FILL` casts rays from the cavity's centre (0.48 x the height), joins a star sphere whose vertices sit midway in the wall,
   and remeshes again: one solid (168,816 tris; 12 of 4,514 star rays missed and were set short, inside the body). Collapse to
   17,494 tris; smart UV; Cycles CPU bake of base colour at 4096 (`work/MireMaw_bake_basecolor.png`, a record only). Each face
   takes the nearest part colour in CIE Lab (reference colours from the k-means of the bake); the moss bakes speckled, so place
   rules decide where colour alone does not: moss cap and back, a belly ellipse on the front, skin elsewhere.
   `M_MireMaw_Skin` (9,177 faces), `M_MireMaw_Moss` (6,996), `M_MireMaw_Belly` (1,321).
2. `beast_parts.py` (`MireMaw`), **hand-authored** parametric geometry joined with EXACT booleans: a lens-shaped mouth slot cut
   across the front at 0.86 m (9 cm tall at the centre, 46 cm deep, about +-0.42 m wide; `M_MireMaw_Mouth`); a flat tongue
   (17 cm wide, 3.4 cm thick) lying in the slot, rooted in its back wall (`M_MireMaw_Tongue`); two eyes 13 cm across with pupils
   on the brow (`M_MireMaw_Eye`, `M_MireMaw_Pupil`). 2,292 tris added; the mesh stays one watertight piece.
3. `rig_anim.py` (style `Maw`): `root`, `foot_l`, `foot_r` (under root, so the feet stay planted), `body`, `body_upper`, `head`,
   `jaw` (hinged at the slot's back wall), `tongue_01-04` (under the jaw, along the tongue): 11 bones. Heat weights on a
   watertight 16 mm voxel proxy (68,420 tris), transferred by nearest proxy vertex (0 fallback vertices), smoothed x4, limited
   to 4, normalised; then rules (5,886 vertices) set the jaw weight outright (the lower lip in front of the hinge, ramped; zero
   above the slot and behind the hinge) and give tongue weights to tongue vertices only, by their place along the tongue; then
   limited and normalised again. Each clip is an NLA strip, so each FBX take carries the clip's own name.
4. `metrics.py` re-imports the FBX and writes `metrics.json` (profile `MireMaw`); `render_evidence.py` renders the evidence.

## Clips (30 fps, hand-keyed)

- `Idle` (keys 0, 30, 60; loop): a slow breath that lifts the body and throat 12 mm, a small chew of the jaw.
- `Attack` (keys 0, 14, 30, 36, 42, 56, 66): the **tongue pull** (pinned `enemies.json` mire_maw tongue_pull, windupS 1.0,
  effect pull 4 m, read only): lean back and gather (14), lunge with the jaw open 30 degrees at the end of the windup (30), the
  tongue lashes out six times its length, about 2.4 m (36), holds while the body leans back to pull (42), reels in (56), the
  mouth closes (66). The stretch is a bone scale on `tongue_01` along its length (`scale.y` up to 6.0, `tongue_02-04` follow);
  the tongue's width does not change.
- `Hit` (keys 0, 4, 10, 24): knocked back, the head jerks up, the mouth gapes, recover.
- `Death` (keys 0, 8, 24, 40, 60; last pose held): a last gasp, then the mound slumps forward and sinks 0.28 m with the jaw slack
  and the tongue lolling out 2.4 times its length; the feet stay planted.
- The `bog_glob` attack (magic tier 1, windupS 0.7) has no clip of its own: the brief names exactly four clips, and the card asks
  for the tongue pull.

## Scale (recorded)

The A04 sheets carry no unit. The scale is the pixel scale of sheet 03, where the conscript is a known 1.80 m over 626 px
(2.875 mm/px, also the 42.6 px grid that sheets 03 and 06 share). Sheet 06: top of the mound to the feet 435 px -> **1.25 m**;
front width 367 px -> 1.06 m; side depth 410 px -> 1.18 m. Measured on the FBX: height 1.249 m (-0.1 %), width 1.281 m (+21 %
against the sheet; the seed-42 mound is rounder), depth 1.278 m (+8 %). Note: the in-game greybox maw is 0.80 m tall and
1.50 m wide and deep (`SessionPresentation.cpp`); this scale follows the sheet, not the greybox.

## Known limitations

- The face (mouth slot, tongue, eyes) is hand-made in Blender: TRELLIS lost it on all three runs. There are no nostrils, lips or
  pebbles; the eyes are plain spheres with a pupil.
- The colour split is by rules as much as by the bake: the belly is an ellipse and the moss edge a height band, so it is a
  greybox read, not the concept's ragged moss edge. Bake speckle leaves a few skin islands in the moss.
- The feet are where TRELLIS put them: the left foot is forward, the right one back.
- At 2.4 m the tongue is a thin strip in the evidence views (its 17 cm width stays fixed while its length grows six times); the
  4 m pull itself is game logic, not the clip.
- In the last Death keys a small sliver of the mouth slot's corner shows at the back-right as the body slumps.
- No LODs beyond LOD0. Within the 20,000 tri budget with 214 to spare.
