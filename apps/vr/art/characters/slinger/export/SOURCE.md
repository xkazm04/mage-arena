# Slinger (T15, Bout 2 creature) - source record (draft, Lynx)

AI-assisted (Claude + owner, option 2); shippability pending the originality check in docs/submission/RULES-CHECKLIST.md row 6

- Date: 2026-10-21, made on Lynx (Windows ARM64). No Unreal Engine was run; nothing here is a UE result.
- Subject: Roman auxiliary slinger (cap helmet, studded jerkin, short tunic, stone bag and net pouch, laced shin wraps), drawn to match `apps/vr/art/model-sheets/04-slinger-turnaround.jpg`.
- File: `SK_Slinger.fbx` (FBX 7.4 binary, 1.8 MB). One skeleton (23 bones, UE5 mannequin names), one skinned mesh
  `SM_Slinger` (24,000 tris, 12,002 verts), 7 flat-colour materials, no textures, four takes
  `Idle` (61 frames), `Attack` (47), `Hit` (25), `Death` (61, last pose held) at 30 fps. +X forward, Z up, origin at the feet, 1.80 m.
- Repo scripts (kiro/mage-arena, `apps/vr/tools/assets/`): `leonardo.mjs --asset slinger`, `trellis.mjs` + `trellis-api.json`,
  `finish.py` (palette set `Slinger`), `rig_anim.py` (attack style `Sling`), `metrics.py`, `render_evidence.py`.

## Tools and versions

| Step | Tool | Version / model |
|---|---|---|
| Concept (front, turnaround) | Leonardo API | model `openai/gpt-image-2.5-flare`, `public: false`, prompt_enhance OFF |
| Image to 3D | ComfyUI 0.42.1 (http://127.0.0.1:8188), PyTorch 2.16.0.dev20261012+cu134 | TRELLIS.2: `trellis_2_int8_convrot.safetensors`, shape VAE `trellis_2_shape_vae_bf16`, texture VAE `trellis_2_texture_vae_bf16`, DINOv3 `dino_v3_L_naf_fp32`, BiRefNet background removal; workflow lynx-bench `media/3d-trellis-api.json` |
| Remesh, bake, segment, rig, animate, export | Blender 5.2.2 LTS (Windows ARM64, build 2026-09-15) | voxel remesh, Cycles CPU bake, ARMATURE_AUTO heat weights, FBX exporter (io_scene_fbx) |

No MiniMax-H3 output and no FLUX model was used at any step.

## Concept (Leonardo)

Two calls, 56 credits for this asset (log: `apps/vr/art/characters/slinger/leonardo-log.json`; the cap of 100 credits covers conscript and slinger together).
The front view was the TRELLIS input.

- front (seed 4201, MEDIUM, 768x1376, 14 credits, 2026-10-21T11:29:38.239Z):

  > Full-body orthographic front view of a Roman auxiliary slinger, a lean young man with a plain clean-shaven face: a close-fitting cap helmet with short cheek flaps; a short-sleeved tunic ending above the knee with a studded leather jerkin over the chest; a leather sling coiled and tucked into the wide belt at his right hip; a large leather shoulder bag of stones on a strap across the chest, hanging at his left hip, with a small net pouch of stones at his right hip; laced leather wraps from the ankles to below the knees and simple leather sandals. He stands in a neutral A-pose: arms straight, held out and angled about 35 degrees down away from the torso so they do not touch the body or the bags, open empty hands with fingers together, legs slightly apart, feet flat. The sling stays coiled at the belt. No fire, no effects. He faces the camera directly, symmetrical, the whole figure from head to sandals visible with empty margin around it, centred on a plain flat light grey background, no scenery, no ground, a single character. Game-ready 3D character model reference for a modeller: simple large readable shapes, chunky proportions, big hands, matte surfaces, one flat colour per material with no pattern and no texture detail. Palette blocks: tunic undyed linen beige, jerkin dark brown leather, cap helmet iron grey, belt, sling, shin wraps and sandals mid brown leather, stone bags warm tan leather with muted red-brown straps, skin warm tan, hair mid brown. Soft even studio light, no cast shadows, no text, no labels, no grid, no watermark.

- turnaround (seed 4202, MEDIUM, 1376x768, 14 credits, 2026-10-21T11:50:48.976Z):

  > Character turnaround sheet of a Roman auxiliary slinger, a lean young man with a plain clean-shaven face: a close-fitting cap helmet with short cheek flaps; a short-sleeved tunic ending above the knee with a studded leather jerkin over the chest; a leather sling coiled and tucked into the wide belt at his right hip; a large leather shoulder bag of stones on a strap across the chest, hanging at his left hip, with a small net pouch of stones at his right hip; laced leather wraps from the ankles to below the knees and simple leather sandals. Three full-body orthographic views of the same character side by side at the same scale and height: front view, right side view, back view. He stands in a neutral A-pose: arms straight, held out and angled about 35 degrees down away from the torso so they do not touch the body or the bags, open empty hands with fingers together, legs slightly apart, feet flat. The sling stays coiled at the belt. No fire, no effects. Plain flat light grey background, no scenery, no ground. Game-ready 3D character model reference for a modeller: simple large readable shapes, chunky proportions, big hands, matte surfaces, one flat colour per material with no pattern and no texture detail. Palette blocks: tunic undyed linen beige, jerkin dark brown leather, cap helmet iron grey, belt, sling, shin wraps and sandals mid brown leather, stone bags warm tan leather with muted red-brown straps, skin warm tan, hair mid brown. Soft even studio light, no cast shadows, no text, no labels, no grid, no watermark.

## TRELLIS.2

- Input: `concept-front-s4201-medium.jpg`. One run: seed 42 on all four samplers (nodes 3, 12, 18, 23),
  DecimateMesh (node 186) to 700,000 faces before UnwrapMesh and the 4096 texture bake, 375 s, 2026-10-21T11:40:21.359Z.
  The first seed was usable, so no other seed was tried. The queue was empty before the job; `POST /free` ran after it.
- Raw output: 693,549 tris, 287 components (285 small floaters dropped in Blender).

## Blender

1. `finish.py` (palette set `Slinger`): import with merge_vertices; floaters under 0.5 % of faces dropped; scaled to 1.80 m with the feet at
   z = 0; voxel remesh at 10 mm (71,956 tris); collapse to 24,000 tris; smart UV; Cycles CPU
   high-to-low bake of base colour at 4096 (`work/Slinger_bake_basecolor.png`, a record only, not referenced by the FBX). Each face then takes the
   nearest part colour in CIE Lab (reference colours tuned by k-means on this bake), smoothed over its neighbours, plus the
   placement rules of the palette set, and gets one flat-colour material: `M_Slinger_Linen`, `M_Slinger_Jerkin`, `M_Slinger_Iron`, `M_Slinger_Leather`, `M_Slinger_Bag`, `M_Slinger_Skin`, `M_Slinger_Hair`.
2. `rig_anim.py` (style `Sling`): 23-bone UE5-mannequin chain (root, pelvis, spine_01-03, neck_01, head, clavicle/upperarm/lowerarm/
   hand, thigh/calf/foot/ball per side; no finger bones) fitted to the mesh. Heat weights on a watertight 16 mm voxel proxy,
   transferred by nearest proxy vertex (0 fallback vertices), smoothed, limited to 4 influences, normalised. The four
   clips are hand-keyed. Attack follows the slinger brief: wind the right arm up behind the head, whirl over, release forward and down, follow through. The T15 card and DECISIONS.md name no clips for this soldier, so the default set Idle, Attack, Hit, Death is used.
   Each clip is an NLA strip, so each FBX take carries the clip's own name.
3. `metrics.py` re-imports the FBX and writes `metrics.json`; `render_evidence.py` renders the evidence from the FBX.

## Known limitations

- The face is a flat-colour read; there are no eye or mouth materials.
- The sling itself is coiled at the belt in the concept and did not survive TRELLIS as a separate shape; no sling prop is modelled.
- The bag, the net pouch and the strap are weighted like the body, so they follow the spine and thighs.
- The net pouch of stones bakes grey and is re-labelled to the Bag material by a height rule in `finish.py`.
- No finger bones, so the hands stay rigid. No LODs beyond LOD0.
