# Brennic (T38) - source record (draft, Lynx)

AI-assisted (Claude + owner, option 2); shippability pending the originality check in docs/submission/RULES-CHECKLIST.md row 6

- Date: 2026-10-21, made on Lynx (Windows ARM64). No Unreal Engine was run; nothing here is a UE result.
- File: `SK_Brennic.fbx` (FBX 7.4 binary, 1.9 MB). One skeleton (23 bones, UE5 mannequin names), one skinned mesh
  `SM_Brennic` (23,976 tris, 11,988 verts), six flat-colour materials, no textures, four takes `Idle` (61 frames),
  `Cast` (47), `Hit` (25), `Death` (61, last pose held) at 30 fps. +X forward, Z up, origin at the feet, 1.80 m.
- Repo scripts (kiro/mage-arena, `apps/vr/tools/assets/`): `leonardo.mjs`, `trellis.mjs` + `trellis-api.json`,
  `finish.py`, `rig_anim.py`, `metrics.py`, `render_evidence.py`, `preview.py`.

## Tools and versions

| Step | Tool | Version / model |
|---|---|---|
| Concept (front, turnaround) | Leonardo API | model `openai/gpt-image-2.5-flare`, `public: false`, prompt_enhance OFF |
| Image to 3D | ComfyUI 0.42.1 (http://127.0.0.1:8188), PyTorch 2.16.0.dev20261012+cu134 | TRELLIS.2: `trellis_2_int8_convrot.safetensors`, shape VAE `trellis_2_shape_vae_bf16`, texture VAE `trellis_2_texture_vae_bf16`, DINOv3 `dino_v3_L_naf_fp32`, BiRefNet background removal; workflow lynx-bench `media/3d-trellis-api.json` |
| Remesh, bake, segment, rig, animate, export | Blender 5.2.2 LTS (Windows ARM64, build 2026-09-15) | voxel remesh, Cycles CPU bake, ARMATURE_AUTO heat weights, FBX exporter (io_scene_fbx) |

No MiniMax-H3 output and no FLUX model was used at any step.

## Concept (Leonardo)

Derived from the A04 Fire mage sheets (`apps/vr/art/model-sheets/01-fire-mage-key-poses.jpg`,
`02-fire-mage-cast-sequence.jpg`): bare-headed, bearded, no hood. Two calls, 28 credits in total
(log: `apps/vr/art/characters/brennic/leonardo-log.json`). The front view was the TRELLIS input.

- front (seed 3801, MEDIUM, 768x1376, 14 credits, 2026-10-21T09:49:08.920Z):

  > Full-body orthographic front view of Brennic, a broad, stocky, middle-aged fire mage of the Roman era: bare-headed with short dark curly hair, no hood, a full dark beard, a strong brow. He wears a plain knee-length wool tunic with short sleeves, belted at the waist with a wide leather belt; a heavy wool cloak draped over both shoulders like a shawl, fastened at the chest and hanging behind him to the back of the knees; leather wraps on both forearms; a plain iron prisoner's collar; wrapped shins and simple leather sandals. He stands in a neutral A-pose: arms straight, held out and angled about 35 degrees down away from the torso so they do not touch the body or the cloak, open empty hands with fingers together, legs slightly apart, feet flat. No weapon, no staff, no fire, no effects. He faces the camera directly, symmetrical, the whole figure from head to sandals visible with empty margin around it, centred on a plain flat light grey background, no scenery, no ground, a single character. Game-ready 3D character model reference for a modeller: simple large readable shapes, chunky proportions, big hands, matte surfaces, one flat colour per material with no pattern and no texture detail. Palette blocks: tunic muted brick red, cloak dark charcoal brown, belt, wraps and sandals mid brown leather, collar dark iron grey, skin warm tan, hair and beard dark brown. Soft even studio light, no cast shadows, no text, no labels, no grid, no watermark.

- turnaround (seed 3802, MEDIUM, 1376x768, 14 credits, 2026-10-21T09:49:34.851Z):

  > Character turnaround sheet of Brennic, a broad, stocky, middle-aged fire mage of the Roman era: bare-headed with short dark curly hair, no hood, a full dark beard, a strong brow. He wears a plain knee-length wool tunic with short sleeves, belted at the waist with a wide leather belt; a heavy wool cloak draped over both shoulders like a shawl, fastened at the chest and hanging behind him to the back of the knees; leather wraps on both forearms; a plain iron prisoner's collar; wrapped shins and simple leather sandals. Three full-body orthographic views of the same character side by side at the same scale and height: front view, right side view, back view. He stands in a neutral A-pose: arms straight, held out and angled about 35 degrees down away from the torso so they do not touch the body or the cloak, open empty hands with fingers together, legs slightly apart, feet flat. No weapon, no staff, no fire, no effects. Plain flat light grey background, no scenery, no ground. Game-ready 3D character model reference for a modeller: simple large readable shapes, chunky proportions, big hands, matte surfaces, one flat colour per material with no pattern and no texture detail. Palette blocks: tunic muted brick red, cloak dark charcoal brown, belt, wraps and sandals mid brown leather, collar dark iron grey, skin warm tan, hair and beard dark brown. Soft even studio light, no cast shadows, no text, no labels, no grid, no watermark.

## TRELLIS.2

- Input: `concept-front-s3801-medium.jpg`. One run: seed 42 on all four samplers (nodes 3, 12, 18, 23),
  DecimateMesh (node 186) to 700,000 faces before UnwrapMesh and the 4096 texture bake, 240 s, 2026-10-21T09:50Z.
  The first seed was usable, so no other seed was tried. The queue was empty before the job; `POST /free` ran after it.
- Raw output: 695,551 tris, 151 components (150 small floaters dropped in Blender).

## Blender

1. `finish.py`: import with merge_vertices; floaters under 0.5 % of faces dropped; scaled to 1.80 m with the feet at
   z = 0; voxel remesh at 10 mm (122,364 tris, watertight); collapse to 23,976 tris; smart UV; Cycles CPU
   high-to-low bake of base colour at 4096 (`work/Brennic_bake_basecolor.png`, a record only, not referenced by the
   FBX). Each face then takes the nearest part colour in CIE Lab, smoothed over its neighbours, and gets one flat-colour
   material: Tunic, Cloak, Leather, Iron, Skin, Hair (the concept's palette blocks).
2. `rig_anim.py`: 23-bone UE5-mannequin chain (root, pelvis, spine_01-03, neck_01, head, clavicle/upperarm/lowerarm/
   hand, thigh/calf/foot/ball per side; no finger bones) fitted to the mesh. Heat weights on a watertight 16 mm voxel proxy,
   transferred by nearest proxy vertex (0 fallback vertices), smoothed, limited to 4 influences, normalised. The four
   clips are hand-keyed (Cast follows sheet 02: stand, crouch and draw back, wind-up, release, follow-through, recover).
   Each clip is an NLA strip, so each FBX take carries the clip's own name.
3. `metrics.py` re-imports the FBX and writes `metrics.json`; `render_evidence.py` renders the evidence from the FBX.

## Known limitations

- The face is a flat-colour read (skin, hair, beard); there are no eye or mouth materials.
- The cloak is weighted by proxy heat like the body, so it follows the spine and thighs rather than simulating cloth.
  In the Death kneel it folds over the back.
- Cast: in the crouch key both arms open outward instead of the right arm drawing back low. The wind-up, release and
  follow-through read as sheet 02 shows. Hit: on the way back to idle the shield arm swings wide.
- No finger bones, so the hands stay rigid. No LODs beyond LOD0.
