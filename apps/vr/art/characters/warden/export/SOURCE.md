# Arcane warden (A07 sheet 05) - source record (draft, Lynx)

AI-assisted (Claude + owner); shipping rests on the owner's ruling of 2026-10-21 (docs/DECISIONS.md, asset originality) and its
licence ledger row (`docs/submission/ASSET-LEDGER.md`, Group 9).

- Date: 2026-10-22, made on Lynx (Windows ARM64). **No Unreal Engine was run here**; nothing below is a UE result.
- Subject: the arcane warden of `apps/vr/art/environment-kit/05-warden-turnaround.jpg`: a tall figure in a long grey hooded robe with a
  stern bare face under the hood and a simple bronze staff. The sheet is grey clay, so the colours (slate robe, charcoal sash, bronze
  trim, pale skin, dark sandals) come from the Leonardo concept's palette blocks.
- File: `SK_Warden.fbx` (FBX 7.4 binary, 1.97 MB). One skeleton (23 bones, the UE5 mannequin chains), one skinned mesh `SM_Warden`
  (24,148 tris: body 24,000 + staff 148; 12,078 verts), 5 flat-colour materials, no textures, four takes `Idle` (61 frames), `Cast` (47),
  `Hit` (25), `Death` (61, last pose held) at 30 fps. +X forward, Z up, origin at the feet, 1.744 m to the hood top.
- Scripts: the repo's `apps/vr/tools/assets/` scripts, copied and patched in the archive (`C:\Users\kazda\kiro\mage-arena-archive\A07-warden\tools\`)
  because this run may not edit `apps/vr/tools/**`: `finish.py` (palette set `Warden`), `rig_anim.py` (style `Warden`: robe leg fit,
  left-hand Cast), `weapon_staff.py` (new, adapted from `weapon.py`), `metrics.py` (profile `Warden`), `render_evidence.py` (framing
  `Warden`), `leonardo.mjs` (asset `warden`). The patches are to be merged into the repo tools by a run that may touch them.

## Tools and versions

| Step | Tool | Version / model |
|---|---|---|
| Concept (front) | Leonardo API | `openai/gpt-image-2.5-flare`, `public: false`, prompt_enhance OFF |
| Image to 3D | ComfyUI 0.42.1 (http://127.0.0.1:8188) | TRELLIS.2 int8 (`trellis_2_int8_convrot.safetensors`), workflow `trellis-api.json` |
| Remesh, bake, rig, staff, animate, export, metrics, evidence | Blender 5.2.2 LTS (Windows ARM64) | voxel remesh, Cycles CPU bake, ARMATURE_AUTO heat weights, FBX exporter, EEVEE |

ComfyUI was not running when this run began (the brief said it was). No instance existed, so it was started once with the command line of
`C:\Users\kazda\ai\ComfyUI\run_nvidia_gpu.bat`; it is still up. No MiniMax-H3 output and no FLUX model was used.

## Concept (Leonardo)

One call, 14 credits (log: `apps/vr/art/characters/warden/leonardo-log.json`; the project total tonight was 14 of 400; the log's `runCap` 56
is this run's own cap for the warden). Front, seed 5101, MEDIUM, 768x1376, 2026-10-22T08:46:01Z: `concept-front-s5101-medium.jpg`.
The prompt is in the log: an A-pose front view of the warden (hood, stern face, plain collar band, wide sleeves, sash, sandals), the left
hand open and the right a loosely curled empty fist, **no staff** (the staff is hand-authored). The sheet's own front panel was not used
as the TRELLIS input: it is grey with a grid behind it and the staff crosses the hand.

## TRELLIS.2

- Input: `concept-front-s5101-medium.jpg` (a copy in the archive as `warden_front_input.jpg`). DecimateMesh (node 186) to 700,000 faces
  before UnwrapMesh and the 4096 bake. The queue was empty before the job; one job at a time; `POST /free` ran after it, before the
  Blender bakes.
- Run 1, seed 42 (255 s, 2026-10-22): **used**. 9 small floaters were dropped in Blender. **No seed was rejected**: the first run gave a
  clean robe, hood, hands and sandals, so no second run was made.
- The GLB faces +X; `finish.py` ran with rot_z -90 so the figure faces -Y as the rig expects.

## Blender

1. `finish.py` (set `Warden`): import with merge_vertices; scaled to **1.745 m** with the feet at z = 0; voxel remesh at 10 mm; collapse to
   24,000 tris; smart UV; Cycles CPU bake at 4096 (record only). Each face takes the nearest part colour in CIE Lab, smoothed over its
   neighbours: `M_Warden_Robe`, `_Sash`, `_Trim` (bronze collar band and hem), `_Skin`, `_Sandal`.
2. `rig_anim.py` (style `Warden`): the mannequin chains (`root`, `pelvis`, `spine_01-03`, `neck_01`, `head`, clavicle, upperarm, lowerarm and
   hand per side, thigh, calf, foot and ball per side: 23 bones). The legs are fitted to the feet under the robe (z < 3 % of the height
   and |x| < 15 cm), not to the skirt. Heat weights on a watertight 16 mm voxel proxy (14,808 verts), transferred by nearest proxy vertex
   (0 fallback), smoothed x4, limited to 4 influences, normalised.
3. `weapon_staff.py`: a **hand-authored** bronze staff (parametric bmesh, 148 tris: shaft, butt cap, collar, ball head), 1.62 m as on the
   sheet, built vertical in the fist of `hand_r` at the Idle pose, carried back to the rest pose through hand_r's inverse Idle skinning
   matrix, joined into the one skinned mesh, 100 % weighted to `hand_r`, in the warden's own `Trim` material. At the Cast keys 9, 17, 23
   and 31 `hand_r` is re-solved so the staff stays upright.
4. `metrics.py` re-imports the FBX fresh and writes `metrics.json` (profile `Warden`); `render_evidence.py` renders the evidence from the FBX.

## Clips (30 fps, hand-keyed)

- `Idle` (keys 0, 30, 60; loop): arms hang at 82 degrees below horizontal, the right fist on the staff, slow breath.
- `Cast` (keys 0, 9, 17, 23, 31, 46): the left hand casts: the torso turns and the open palm rises (frame 9), lifts above the shoulder
  (17), then pushes forward at arm's length (23) and holds (31); the right arm lifts the staff a little and the staff stays upright. The
  release frame is a Lynx choice: the warden has no combat data in `apps/vr/data/`.
- `Hit` (keys 0, 4, 10, 24): torso twisted back, the left arm raised to shield the face, recover.
- `Death` (keys 0, 6, 20, 34, 48, 60; last pose held): recoil, knees buckle, kneel, collapse forward; the staff follows the right hand.

## Scale (recorded)

The A07 sheets carry no unit. Sheet 05's grid is the 42.6 px grid of A04 sheet 03, where the conscript is a known 1.80 m over 626 px
(2.875 mm/px). On sheet 05 the front figure is 606 px from the hood top to the soles (side 605, back 608; measured as dark pixels in each
panel, the staff excluded) -> **1.745 m**; the staff from its butt to the ball top is about 565 px -> 1.62 m. Measured on the FBX: height
1.7442 m (-0.0 %), object scale 1. Duel-watch distance: the repo records none (the story bible puts Warden Septima at the dais rail,
left), so `Warden_game_640x480.png` assumes **4 m** in front of the seated eye at 1.2 m with a 90 degree horizontal FOV. This is an
assumption.

## Known limitations

- Greybox finish: flat colours from a per-face split, so the bronze hem and collar edges are ragged and the sash has a few specks.
- The face is a flat skin colour: no eyes or brows (the T16 style pick decides the look).
- The robe is one skinned shell: when the legs move in Death the skirt bends with them and folds a little, with no cloth simulation.
- Rest pose: the arms are in a 53 degree A-pose and the staff, rigid on `hand_r`, points out diagonally there (it is upright only in
  Idle and at the Cast keys). `metrics.json` therefore reports the bbox centre without the staff (check 6); the staff's 82 verts are the
  4 small components of check 10.
- No LODs beyond LOD0. No IK. `Cast` is a presentation clip with no combat timing behind it.
- Not run: any Unreal Engine step. No Unreal Engine was run here.

## Archive (not in git)

`C:\Users\kazda\kiro\mage-arena-archive\A07-warden\work\`: `warden_s42.glb` (TRELLIS raw), the input JPG, the finish, rigged and armed
`.blend` files, the bake PNG, `Warden_rig.json`, `Warden_weapon.json`, `trellis-runs.jsonl`, the logs; `tools\` holds the patched scripts.
