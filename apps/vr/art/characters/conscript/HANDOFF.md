# Conscript (T15, Bout 2) - Lynx hand-off H1

Made on Lynx (Windows ARM64) on 2026-10-21 on the Brennic route (T38). No Unreal Engine was run here, so nothing below is a UE result. Wolf runs
the T38 pre-import checks again on import (`apps/vr/tasks/T38-first-animated-opponent.md`, "Hand-off").

## Landing path (in git, LFS)

`apps/vr/art/characters/conscript/export/`, committed on a Lynx branch; the Director merges it and it reaches origin master through Git LFS (owner ruling 2026-10-21). Every file below was
copied byte for byte from the Lynx archive `C:\Users\kazda\kiro\mage-arena-archive\Conscript\`; sha256 and size per file are in `manifest.json`.

| File | What |
|---|---|
| `SK_Conscript.fbx` | the rigged mesh with the four takes `Idle`, `Attack`, `Hit`, `Death` (FBX 7.4 binary, 1.8 MB) |
| `SOURCE.md` | tool versions, prompts, seeds, date, Leonardo model, licence line, known limitations |
| `metrics.json` | `apps/vr/tools/assets/metrics.py` run on the exported FBX (re-imported fresh), attack clip `Attack` |
| `Conscript_turnaround.png` | EEVEE, 8 views, Idle frame 0 |
| `Conscript_game_640x480.png` | seated view: eye 1.2 m, creature 6 m away, 90 degree horizontal FOV |
| `Conscript_clip_{Idle,Attack,Hit,Death}.png` | one still strip per clip, at its authored key frames |
| `work/` | not in git: stays in the archive `C:\Users\kazda\kiro\mage-arena-archive\Conscript\work\` (TRELLIS raw GLB seed 42, finish and rig `.blend` files, the base-colour bake PNG) |

The FBX and the seven PNGs are LFS objects (`git lfs ls-files`); `SOURCE.md` and `metrics.json` are plain text. The concept jpgs are LFS objects too (`apps/vr/art/**/*.jpg`).

## Metrics summary (`metrics.json`)

- One skinned mesh `SM_Conscript`: 11,989 verts, 23,978 tris, 1 component, 0 floaters, watertight, winding consistent.
- One skeleton: 23 bones with UE5 mannequin names (no finger bones). The armature node is named `Armature`.
- Height 1.80 m; min z 0 m; bbox centre xy (-0.0001, -0.0002) m; object scale 1.
- Weights: 0 unweighted vertices, max |sum - 1| = 0, at most 4 influences per vertex.
- Clips (30 fps): `Idle` 61 frames, `Attack` 36, `Hit` 25, `Death` 61 (last pose held).
- Materials: `M_Conscript_Mail`, `M_Conscript_Iron`, `M_Conscript_Linen`, `M_Conscript_Leather`, `M_Conscript_Wood`, `M_Conscript_Skin`; each one flat colour; 0 textures in the FBX.
- Leonardo: 2 calls, 42 credits (`leonardo-log.json`). TRELLIS.2: 1 run (seed 42, 391 s).

## T38 Wolf pre-import checks, as measured on Lynx

| # | Check | Result | Value |
|---|---|---|---|
| 1 | one skeleton | pass | 1 armature, 1 skin deformer |
| 2 | one skinned mesh | pass | 1 mesh, 1 skinned |
| 3 | exactly the four clips, none empty | pass | stacks ['Idle', 'Attack', 'Hit', 'Death']; frames Idle 61, Attack 36, Hit 25, Death 61 |
| 4 | LOD0 triangle count recorded | pass | 23978 tris (target 18000-30000, scene budget 350000) |
| 5 | one flat colour per material, no embedded textures | pass | 6 materials, 0 textures |
| 6 | origin at the feet | pass | min z -0.0 m, bbox centre xy [-0.0001, -0.0002] m |
| 7 | +X forward | pass | hand_l y 0.465, hand_r y -0.466, toe dx 0.289 |
| 8 | unit scale, height 1.62-1.98 m | pass | height 1.7995 m, object scale [1.0, 1.0, 1.0] |
| 9 | Tool versions and prompts in `SOURCE.md` | pass | ComfyUI 0.42.1, TRELLIS.2 int8, Blender 5.2.2, Leonardo `openai/gpt-image-2.5-flare` |

Checks 1-8 come from `metrics.py`; check 3 reads the clip set Idle, Attack, Hit, Death (the T15 card and DECISIONS.md name no clips for this
soldier, so the default set was used). Check 9 is the file itself. `metrics.py` also tests that weights are normalised: pass.
The originality check (`docs/submission/RULES-CHECKLIST.md` row 6) is still open and is not part of this hand-off.

## Rebuild

```
node apps/vr/tools/assets/leonardo.mjs gen --asset conscript --view front --seed 4101 --quality MEDIUM
node apps/vr/tools/assets/trellis.mjs run apps/vr/art/characters/conscript/concept-front-s4101-medium.jpg conscript_s42 42 <archive>/work
node apps/vr/tools/assets/trellis.mjs free
blender -b -P apps/vr/tools/assets/finish.py -- <archive>/work/conscript_s42.glb <archive>/work Conscript 0 4096 0.01 24000 Conscript
blender -b -P apps/vr/tools/assets/rig_anim.py -- <archive>/work/Conscript_finish.blend <archive>/work Conscript <archive>/SK_Conscript.fbx Thrust
blender -b -P apps/vr/tools/assets/metrics.py -- <archive>/SK_Conscript.fbx <archive>/metrics.json Attack
blender -b -P apps/vr/tools/assets/render_evidence.py -- <archive>/SK_Conscript.fbx <archive> Conscript <archive>/work/Conscript_rig.json Attack
```
