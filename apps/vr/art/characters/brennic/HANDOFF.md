# Brennic (T38) - Lynx hand-off H1

Made on Lynx (Windows ARM64) on 2026-10-21. No Unreal Engine was run here, so nothing below is a UE result. Wolf runs
the T38 checks again on import (`apps/vr/tasks/T38-first-animated-opponent.md`, "Hand-off").

## Archive path (outside git)

`C:\Users\kazda\kiro\mage-arena-archive\T38\Brennic\`

| File | What |
|---|---|
| `SK_Brennic.fbx` | the rigged mesh with the four takes `Idle`, `Cast`, `Hit`, `Death` (FBX 7.4 binary, 1.9 MB) |
| `SOURCE.md` | tool versions, prompts, seeds, date, Leonardo model, licence line, known limitations |
| `metrics.json` | `apps/vr/tools/assets/metrics.py` run on the exported FBX (re-imported fresh) |
| `Brennic_turnaround.png` | EEVEE, 8 views, Idle frame 0 |
| `Brennic_game_640x480.png` | seated view: eye 1.2 m, rival 6 m away, 90 degree horizontal FOV |
| `Brennic_clip_{Idle,Cast,Hit,Death}.png` | one still strip per clip, at its authored key frames |
| `work/` | TRELLIS raw GLB (seed 42), finish and rig `.blend` files, the base-colour bake PNG (a record only) |

How the files cross from Lynx to Wolf is the operator's choice (T38 "Hand-off").

## Metrics summary (`metrics.json`)

- One skinned mesh `SM_Brennic`: 11,988 verts, 23,976 tris, 1 component, 0 floaters, watertight, winding consistent.
- One skeleton: 23 bones with UE5 mannequin names (root, pelvis, spine_01-03, neck_01, head; clavicle, upperarm,
  lowerarm, hand, thigh, calf, foot, ball per side; no finger bones). The armature node is named `Armature`.
- Height 1.80 m; min z 0.0 m; bbox centre xy (0.000, 0.000) m; object scale 1.
- Weights: 0 unweighted vertices, max |sum - 1| = 0.0, at most 4 influences per vertex: normalised yes.
- Clips (30 fps): `Idle` 61 frames (2.0 s loop), `Cast` 47 (1.53 s), `Hit` 25 (0.8 s), `Death` 61 (2.0 s, last pose held).
- Materials: `M_Brennic_Tunic`, `_Cloak`, `_Leather`, `_Iron`, `_Skin`, `_Hair`, each one flat colour; 0 textures in the FBX.
- Leonardo: 2 calls, 28 credits (`leonardo-log.json`). TRELLIS.2: 1 run (seed 42, 240 s).

## T38 Wolf pre-import checks, as measured on Lynx

| # | Check | Result | Value |
|---|---|---|---|
| 1 | One skeleton | pass | 1 armature, 1 skin deformer |
| 2 | One skinned mesh | pass | 1 mesh, skinned |
| 3 | Exactly the four clips, none empty | pass | takes `Idle`, `Cast`, `Hit`, `Death`; 61 / 47 / 25 / 61 frames |
| 4 | LOD0 triangle count recorded | pass | 23,976 tris (target 18-30k; scene budget 350k) |
| 5 | One flat colour per material, no embedded textures | pass | 6 materials, 0 textures |
| 6 | Origin at the feet | pass | min z 0.0 m, centre xy (0.000, 0.000) |
| 7 | +X forward | pass | hand_l at y +0.446, hand_r at y -0.445, toes at +0.296 m in x |
| 8 | Unit scale (`FBody` mage is 180 cm) | pass | 1.7999 m, scale 1; exported with FBX_SCALE_ALL |
| 9 | Tool versions and prompts in `SOURCE.md` | pass | ComfyUI 0.42.1, TRELLIS.2 int8, Blender 5.2.2, Leonardo `openai/gpt-image-2.5-flare` |

Checks 1-8 come from `metrics.py`; check 9 is the file itself. The originality check
(`docs/submission/RULES-CHECKLIST.md` row 6) is still open and is not part of this hand-off.

## Rebuild

```
node apps/vr/tools/assets/leonardo.mjs gen --view front --seed 3801 --quality MEDIUM
node apps/vr/tools/assets/trellis.mjs run apps/vr/art/characters/brennic/concept-front-s3801-medium.jpg brennic_s42 42 <archive>/work
node apps/vr/tools/assets/trellis.mjs free
blender -b -P apps/vr/tools/assets/finish.py -- <archive>/work/brennic_s42.glb <archive>/work Brennic 0 4096 0.01 24000
blender -b -P apps/vr/tools/assets/rig_anim.py -- <archive>/work/Brennic_finish.blend <archive>/work Brennic <archive>/SK_Brennic.fbx
blender -b -P apps/vr/tools/assets/metrics.py -- <archive>/SK_Brennic.fbx <archive>/metrics.json
blender -b -P apps/vr/tools/assets/render_evidence.py -- <archive>/SK_Brennic.fbx <archive> Brennic <archive>/work/Brennic_rig.json
```
