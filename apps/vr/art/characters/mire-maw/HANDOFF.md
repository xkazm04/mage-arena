# Mire maw (T15, Bout 2) - Lynx hand-off H1

Made on Lynx (Windows ARM64) on 2026-10-21 on the conscript and slinger route (Leonardo, TRELLIS.2, Blender). No Unreal Engine
was run here, so nothing below is a UE result. Wolf imports it and runs the T38 pre-import checks again
(`apps/vr/tasks/T38-first-animated-opponent.md`, "Hand-off"); the beast checks that replace the humanoid ones are listed below.

## Landing path (in git, LFS)

`apps/vr/art/characters/mire-maw/export/`, committed on a Lynx branch; it reaches origin master through Git LFS (owner ruling
2026-10-21). Every file below was copied byte for byte from the Lynx archive
`C:\Users\kazda\kiro\mage-arena-archive\T15\mire-maw\export\`; sha256 and size per file are in `manifest.json`.

| File | What |
|---|---|
| `SK_MireMaw.fbx` | the rigged mesh (11 bones: feet, body, head, jaw, four tongue bones) and the four takes `Idle`, `Attack`, `Hit`, `Death` (FBX 7.4 binary, 1.3 MB) |
| `SOURCE.md` | tool versions, prompts and seeds, the three TRELLIS runs, the hand-made face, the scale record, the clips, known limitations |
| `metrics.json` | `apps/vr/tools/assets/metrics.py` (profile `MireMaw`) run on the exported FBX, re-imported fresh, attack clip `Attack` |
| `MireMaw_turnaround.png` | EEVEE, 8 views every 45 degrees, Idle frame 0 |
| `MireMaw_game_640x480.png` | seated view: eye 1.2 m, the maw 6 m away, 90 degree horizontal FOV |
| `MireMaw_clip_{Idle,Attack,Hit,Death}.png` | one still strip per clip, at its authored key frames, front three-quarter view (wide, for the tongue) |
| `work/` | not in git: stays in the archive `C:\Users\kazda\kiro\mage-arena-archive\T15\mire-maw\work\` (the three TRELLIS raw GLBs, the side-view crop, the finish, parts and rigged `.blend` files, the base-colour bake PNG, the step JSONs) |

The FBX and the seven PNGs are LFS objects; `SOURCE.md` and `metrics.json` are plain text. The concept JPGs are LFS objects too.

## Metrics summary (`metrics.json`)

- One skinned mesh `SM_MireMaw`: 9,883 verts, 19,786 tris (budget 20,000), one component, 0 floaters, watertight, winding
  consistent.
- One skeleton, 11 bones: `root`, `foot_l`, `foot_r`, `body`, `body_upper`, `head`, `jaw`, `tongue_01-04`. The armature node is
  `Armature`.
- Height 1.249 m (target 1.25 m from sheet 06, -0.1 %); width 1.281 m, depth 1.278 m; min z 0.0014 m; bbox centre xy
  (-0.0003, 0.0) m; object scale 1.
- Weights: 0 unweighted vertices, max |sum - 1| = 0, at most 4 influences per vertex.
- Clips (30 fps): `Idle` 61 frames, `Attack` 67 (tongue pull: lash at frames 30-36 after the pinned 1.0 s windup, about 2.4 m),
  `Hit` 25, `Death` 61 (last pose held).
- Materials: `M_MireMaw_Skin`, `M_MireMaw_Belly`, `M_MireMaw_Moss`, `M_MireMaw_Mouth`, `M_MireMaw_Tongue`, `M_MireMaw_Eye`,
  `M_MireMaw_Pupil`; each one flat colour; 0 textures in the FBX.
- Leonardo: 2 calls, 28 credits (`leonardo-log.json`). TRELLIS.2: 3 runs (front seed 42 used without its face; front seed 7 and
  side seed 42 rejected as open shells).

## Pre-import checks, as measured on Lynx (`metrics.py` profile `MireMaw`)

| # | Check | Result | Value |
|---|---|---|---|
| 1 | one skeleton | pass | 1 armature, 1 skin deformer |
| 2 | one skinned mesh | pass | 1 mesh, 1 skinned |
| 3 | exactly the four clips, none empty | pass | stacks ['Idle', 'Attack', 'Hit', 'Death']; frames Idle 61, Attack 67, Hit 25, Death 61 |
| 4 | LOD0 triangle count within budget | pass | 19786 tris (budget 20000, one maw) |
| 5 | one flat colour per material, no embedded textures | pass | 7 materials, 0 textures |
| 6 | origin at the feet | pass | min z 0.0014 m, bbox centre xy [-0.0003, -0.0] m |
| 7 | +X forward | pass | foot_l y 0.3, foot_r y -0.29, jaw dx 0.018 |
| 8 | unit scale, height 1.25 m +-10 % | pass | height 1.2488 m (-0.1 %), object scale [1.0, 1.0, 1.0] |
| 9 | weights normalised | pass | max |sum-1| 0.0, unweighted 0, max influences 4 |
| 10 | no floaters | pass | 1 component(s), 0 floaters |

The humanoid T38 checks 4, 7 and 8 do not apply to the maw; the profile replaces them with its budget, its own bones and the
sheet scale. Check 10 is new for the beasts.

## For Wolf

- The tongue stretches by **bone scale**: `tongue_01` keys `scale.y` up to 6.0 in `Attack` (2.4 in `Death`), and
  `tongue_02-04` inherit it. Import with the animation's scale curves kept; a tongue that stays in the mouth during `Attack`
  means the scale track was dropped.
- Scale: 1.25 m follows sheet 06 (see `export/SOURCE.md`, Scale). The greybox maw is 0.80 m tall and 1.50 m wide; if the game
  wants that, scale the actor, not the asset.
- The 4 m pull and the line telegraph are game logic; the clip only shows the lash and the reel-in. `bog_glob` has no clip of its
  own (exactly four clips were asked for); `Attack` with a VFX glob from the mouth, or a later clip, can cover it.

## Rebuild

```
node apps/vr/tools/assets/leonardo.mjs gen --asset mire-maw --view front --seed 4401 --quality MEDIUM
node apps/vr/tools/assets/leonardo.mjs gen --asset mire-maw --view turnaround --seed 4402 --quality MEDIUM
node apps/vr/tools/assets/trellis.mjs run apps/vr/art/characters/mire-maw/concept-front-s4401-medium.jpg mire_maw_s42 42 <archive>/work
node apps/vr/tools/assets/trellis.mjs free
blender -b -P apps/vr/tools/assets/finish.py -- <archive>/work/mire_maw_s42.glb <archive>/work MireMaw 0 4096 0.01 17500 MireMaw
blender -b -P apps/vr/tools/assets/beast_parts.py -- <archive>/work/MireMaw_finish.blend <archive>/work MireMaw MireMaw
blender -b -P apps/vr/tools/assets/rig_anim.py -- <archive>/work/MireMaw_parts.blend <archive>/work MireMaw <archive>/export/SK_MireMaw.fbx Maw
blender -b -P apps/vr/tools/assets/metrics.py -- <archive>/export/SK_MireMaw.fbx <archive>/export/metrics.json Attack MireMaw
blender -b -P apps/vr/tools/assets/render_evidence.py -- <archive>/export/SK_MireMaw.fbx <archive>/export MireMaw <archive>/work/MireMaw_rig.json Attack MireMaw
```
