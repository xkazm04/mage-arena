# Conscript (T15, Bout 2 creature) - source record (draft, Lynx)

AI-assisted (Claude + owner, option 2); shippability pending the originality check in docs/submission/RULES-CHECKLIST.md row 6

- Date: 2026-10-21, made on Lynx (Windows ARM64). No Unreal Engine was run; nothing here is a UE result.
- Subject: Roman conscript foot soldier (mail shirt, brimmed helmet with cheek guards, curved shield, short sword at the hip, and a short spear in the right hand), drawn to match `apps/vr/art/model-sheets/03-conscript-turnaround.jpg`.
- File: `SK_Conscript.fbx` (FBX 7.4 binary, 1.8 MB). One skeleton (23 bones, UE5 mannequin names), one skinned mesh
  `SM_Conscript` (24,086 tris, 12,051 verts: 23,978 body + 108 spear), 6 flat-colour materials, no textures, four takes
  `Idle` (61 frames), `Attack` (36), `Hit` (25), `Death` (61, last pose held) at 30 fps. +X forward, Z up, origin at the feet, 1.80 m.
- Repo scripts (kiro/mage-arena, `apps/vr/tools/assets/`): `leonardo.mjs --asset conscript`, `trellis.mjs` + `trellis-api.json`,
  `finish.py` (palette set `Conscript`), `rig_anim.py` (attack style `Thrust`), `weapon.py` (`spear`), `metrics.py`, `render_evidence.py`.

## Tools and versions

| Step | Tool | Version / model |
|---|---|---|
| Concept (front, turnaround) | Leonardo API | model `openai/gpt-image-2.5-flare`, `public: false`, prompt_enhance OFF |
| Image to 3D | ComfyUI 0.42.1 (http://127.0.0.1:8188), PyTorch 2.16.0.dev20261012+cu134 | TRELLIS.2: `trellis_2_int8_convrot.safetensors`, shape VAE `trellis_2_shape_vae_bf16`, texture VAE `trellis_2_texture_vae_bf16`, DINOv3 `dino_v3_L_naf_fp32`, BiRefNet background removal; workflow lynx-bench `media/3d-trellis-api.json` |
| Remesh, bake, segment, rig, animate, export | Blender 5.2.2 LTS (Windows ARM64, build 2026-09-15) | voxel remesh, Cycles CPU bake, ARMATURE_AUTO heat weights, FBX exporter (io_scene_fbx) |

No MiniMax-H3 output and no FLUX model was used at any step.

## Concept (Leonardo)

Two calls, 42 credits for this asset (log: `apps/vr/art/characters/conscript/leonardo-log.json`; the cap of 100 credits covers conscript and slinger together).
The front view was the TRELLIS input.

- front (seed 4101, MEDIUM, 768x1376, 14 credits, 2026-10-21T11:29:18.380Z):

  > Full-body orthographic front view of a Roman conscript foot soldier, a stocky young man with a plain unshaven face: a helmet with a brim, a ridge and hinged cheek guards; a short-sleeved mail shirt reaching mid-thigh with a plain trimmed neck and sleeve hems; a leather belt with a short sword in a plain scabbard hanging at his right hip; loose knee-length linen trousers; wrapped shins and simple leather sandals. A large curved rectangular shield with a round central boss hangs by a strap at his left side, its lower edge at knee height, held clear of his body and of his left arm. He stands in a neutral A-pose: arms straight, held out and angled about 35 degrees down away from the torso so they do not touch the body, the shield or the sword, open empty hands with fingers together, legs slightly apart, feet flat. The sword stays sheathed. No fire, no effects. He faces the camera directly, symmetrical, the whole figure from head to sandals visible with empty margin around it, centred on a plain flat light grey background, no scenery, no ground, a single character. Game-ready 3D character model reference for a modeller: simple large readable shapes, chunky proportions, big hands, matte surfaces, one flat colour per material with no pattern and no texture detail. Palette blocks: mail shirt dark steel grey, helmet lighter iron grey, shield face mid wood brown with the boss muted bronze, trousers and shin wraps warm linen beige, belt, sword scabbard and sandals mid brown leather, skin warm tan, hair dark brown. Soft even studio light, no cast shadows, no text, no labels, no grid, no watermark.

- turnaround (seed 4102, MEDIUM, 1376x768, 14 credits, 2026-10-21T11:50:29.956Z):

  > Character turnaround sheet of a Roman conscript foot soldier, a stocky young man with a plain unshaven face: a helmet with a brim, a ridge and hinged cheek guards; a short-sleeved mail shirt reaching mid-thigh with a plain trimmed neck and sleeve hems; a leather belt with a short sword in a plain scabbard hanging at his right hip; loose knee-length linen trousers; wrapped shins and simple leather sandals. A large curved rectangular shield with a round central boss hangs by a strap at his left side, its lower edge at knee height, held clear of his body and of his left arm. Three full-body orthographic views of the same character side by side at the same scale and height: front view, right side view, back view. He stands in a neutral A-pose: arms straight, held out and angled about 35 degrees down away from the torso so they do not touch the body, the shield or the sword, open empty hands with fingers together, legs slightly apart, feet flat. The sword stays sheathed. No fire, no effects. Plain flat light grey background, no scenery, no ground. Game-ready 3D character model reference for a modeller: simple large readable shapes, chunky proportions, big hands, matte surfaces, one flat colour per material with no pattern and no texture detail. Palette blocks: mail shirt dark steel grey, helmet lighter iron grey, shield face mid wood brown with the boss muted bronze, trousers and shin wraps warm linen beige, belt, sword scabbard and sandals mid brown leather, skin warm tan, hair dark brown. Soft even studio light, no cast shadows, no text, no labels, no grid, no watermark.

## TRELLIS.2

- Input: `concept-front-s4101-medium.jpg`. One run: seed 42 on all four samplers (nodes 3, 12, 18, 23),
  DecimateMesh (node 186) to 700,000 faces before UnwrapMesh and the 4096 texture bake, 391 s, 2026-10-21T11:30:05.448Z.
  The first seed was usable, so no other seed was tried. The queue was empty before the job; `POST /free` ran after it.
- Raw output: 698,273 tris, 67 components (65 small floaters dropped in Blender).

## Blender

1. `finish.py` (palette set `Conscript`): import with merge_vertices; floaters under 0.5 % of faces dropped; scaled to 1.80 m with the feet at
   z = 0; voxel remesh at 10 mm (99,856 tris); collapse to 23,978 tris; smart UV; Cycles CPU
   high-to-low bake of base colour at 4096 (`work/Conscript_bake_basecolor.png`, a record only, not referenced by the FBX). Each face then takes the
   nearest part colour in CIE Lab (reference colours tuned by k-means on this bake), smoothed over its neighbours, plus the
   placement rules of the palette set, and gets one flat-colour material: `M_Conscript_Mail`, `M_Conscript_Iron`, `M_Conscript_Linen`, `M_Conscript_Leather`, `M_Conscript_Wood`, `M_Conscript_Skin`.
2. `rig_anim.py` (style `Thrust`): 23-bone UE5-mannequin chain (root, pelvis, spine_01-03, neck_01, head, clavicle/upperarm/lowerarm/
   hand, thigh/calf/foot/ball per side; no finger bones) fitted to the mesh. Heat weights on a watertight 16 mm voxel proxy,
   transferred by nearest proxy vertex (0 fallback vertices), smoothed, limited to 4 influences, normalised. The four
   clips are hand-keyed. Attack follows the conscript brief: guard with the shield arm, draw the sword hand back, lunge and thrust, hold, recover. The T15 card and DECISIONS.md name no clips for this soldier, so the default set Idle, Attack, Hit, Death is used.
   Each clip is an NLA strip, so each FBX take carries the clip's own name.
3. `metrics.py` re-imports the FBX and writes `metrics.json`; `render_evidence.py` renders the evidence from the FBX.

## Weapon (added 2026-10-21, after the first hand-off)

The Director's review found the Attack clip empty-handed. `weapon.py` (`apps/vr/tools/assets/`, run on `work/Conscript_rigged.blend`) adds a short spear:
a 1.45 m wooden shaft, iron socket, leaf head and butt cap, 108 tris in 4 separate watertight pieces. **Hand-authored in Blender**: parametric
bmesh geometry written for this project, no Leonardo, TRELLIS, ComfyUI or other generated or third-party source, no new image or texture.
It uses the creature's own materials (`M_Conscript_Wood` for the shaft, `M_Conscript_Iron` for the rest), is joined into `SM_Conscript` and weighted
100 % to `hand_r`, so the FBX still holds one skeleton and one skinned mesh. Rigid on one bone, it rests along a fixed axis in Idle, Hit and Death
(spear carried upright and leaning forward at the right hip); at Attack keys 7, 12 and 18 the `hand_r` key is re-solved so the spear points
forward (wrist turn up to 123 degrees), which is what the strike strip shows. 4 floaters in `metrics.json` are these weapon pieces.

## Known limitations

- The face is a flat-colour read; there are no eye or mouth materials.
- The spear is a rigid prop on `hand_r`: it does not bend and the hand does not wrap it (no finger bones). The sheathed sword stays at the hip.
- The shield and the sheathed sword are part of the one mesh and weighted like the body, so the shield follows the left thigh and spine rather than the arm.
- The shield face is told apart from the leather by its place (box rules in `finish.py`), so a few of its edge faces stay Leather or Mail.
- Attack key 7 (draw back) opens both arms outward more than a sword draw would; the thrust at keys 12-18 reads as intended.
- No finger bones, so the hands stay rigid. No LODs beyond LOD0.
