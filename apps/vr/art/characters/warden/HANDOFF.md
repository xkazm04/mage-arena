# Arcane warden (A07 sheet 05) - Lynx hand-off H1

Made on Lynx (Windows ARM64) on 2026-10-22 on the conscript, slinger and cinder-hound route (Leonardo, TRELLIS.2, Blender). **No Unreal
Engine was run here**, so nothing below is a UE result. Wolf imports it with `apps/vr/tasks/T53-warden-import.md` and runs the T38
pre-import checks again (`apps/vr/tasks/T38-first-animated-opponent.md`, "Hand-off"). The warden awaits the T16 style pick for its look.

## Landing path (in git, LFS)

`apps/vr/art/characters/warden/export/`, committed on a Lynx branch; it reaches origin master through Git LFS (owner ruling 2026-10-21).
sha256 and size per file are in `manifest.json`.

| File | What |
|---|---|
| `SK_Warden.fbx` | the rigged mesh (23-bone mannequin-chain skeleton, bronze staff in the right hand) and the four takes `Idle`, `Cast`, `Hit`, `Death` (FBX 7.4 binary, 1.97 MB) |
| `SOURCE.md` | tool versions, prompts and seeds, the TRELLIS run, the scale record, the clips, known limitations |
| `metrics.json` | `metrics.py` (profile `Warden`) run on the exported FBX, re-imported fresh, cast clip `Cast` |
| `Warden_turnaround.png` | EEVEE, 8 views every 45 degrees, Idle frame 0 |
| `Warden_game_640x480.png` | seated view: eye 1.2 m, the warden 4 m away (assumed, see below), 90 degree horizontal FOV |
| `Warden_clip_{Idle,Cast,Hit,Death}.png` | one still strip per clip at its authored key frames, front three-quarter view |
| `../concept-front-s5101-medium.jpg`, `../leonardo-log.json` | the Leonardo front concept (TRELLIS input) and the call log |
| `work/` | not in git: stays in the archive `C:\Users\kazda\kiro\mage-arena-archive\A07-warden\work\` (TRELLIS raw GLB seed 42, finish, rigged and armed `.blend` files, bake PNG, step JSONs); the patched scripts are in `...\A07-warden\tools\` |

The FBX and the six PNGs are LFS objects, as is the concept JPG; `SOURCE.md`, `metrics.json` and `manifest.json` are plain text.

## Metrics summary (`metrics.json`)

- One skinned mesh `SM_Warden`: 12,078 verts, 24,148 tris (body 24,000 + staff 148; budget 25,000, one warden on screen), watertight,
  winding consistent, 5 components: the body and the 4 pieces of the staff (all 82 staff verts are 100 % on `hand_r`; 0 other floaters).
- One skeleton, 23 bones: `root`, `pelvis`, `spine_01-03`, `neck_01`, `head`, `clavicle|upperarm|lowerarm|hand_l|r`, `thigh|calf|foot|ball_l|r`.
  The armature node is `Armature`.
- Height 1.744 m (target 1.745 m from sheet 05, -0.0 %); min z 0.0004 m; body bbox centre xy (0.0005, -0.0001) m; object scale 1.
- Weights: 0 unweighted vertices, max |sum - 1| = 0, at most 4 influences per vertex.
- Clips (30 fps): `Idle` 61 frames, `Cast` 47, `Hit` 25, `Death` 61 (last pose held).
- Materials: `M_Warden_Robe`, `_Sash`, `_Trim`, `_Skin`, `_Sandal`; each one flat colour; 0 textures in the FBX.
- Leonardo: 1 call, 14 credits. TRELLIS.2: 1 run (seed 42 used, 255 s; no seed rejected).

## Pre-import checks, as measured on Lynx (`metrics.py` profile `Warden`)

| # | Check | Result | Value |
|---|---|---|---|
| 1 | one skeleton | pass | 1 armature, 1 skin deformer |
| 2 | one skinned mesh | pass | 1 mesh, 1 skinned |
| 3 | exactly the four clips, none empty | pass | stacks ['Idle', 'Cast', 'Hit', 'Death']; frames Idle 61, Cast 47, Hit 25, Death 61 |
| 4 | LOD0 triangle count within budget | pass | 24148 tris (budget 25000, one warden on screen) |
| 5 | one flat colour per material, no embedded textures | pass | 5 materials, 0 textures |
| 6 | origin at the feet | pass | min z 0.0004 m, body bbox centre xy [0.0005, -0.0001] m (the staff, tilted in the rest pose, is left out) |
| 7 | +X forward | pass | hand_l y 0.421, hand_r y -0.421, toe dx 0.228 |
| 8 | unit scale, height 1.745 m +-10 % | pass | height 1.7442 m (-0.0 %), object scale [1.0, 1.0, 1.0] |
| 9 | weights normalised | pass | max abs(sum-1) 0.0, unweighted 0, max influences 4 |
| 10 | no floaters (the staff aside) | pass | 5 components, 4 small = the staff (82 of 82 verts on hand_r only), 0 floaters |
| 11 | at most 4 influences per vertex | pass | max 4 |
| 12 | min z at 0 within 1 cm | pass | 0.0004 m |
| 13 | watertight, winding consistent | pass | true, true |

Humanoid T38 checks 4 and 8 (18-30k tris, 1.62-1.98 m) are replaced by the warden's budget and its sheet height; 10 to 13 are new.
Check 6 excludes the staff because the rest pose is an A-pose with the staff pointing out diagonally (see below).

## For Wolf

- The staff is part of the one skinned mesh, rigid on `hand_r`. It is vertical in `Idle` and at the `Cast` key frames; in the A-pose
  reference pose it points out diagonally, so the skeletal mesh bounds in the editor are wider than the body. That is expected, not a defect.
- `Cast` is the left hand's push (the right hand keeps the staff upright). Where a spell effect starts: the `hand_l` bone, key frame 23.
- `Death` ends with the warden kneeling and folded forward, the staff on the floor; the skirt bends with the legs (one skinned shell).
- The game view assumes a 4 m distance: the repo records no duel-watch position for the warden (story bible: dais rail, left). Place it
  by the layout, not by this PNG.
- Scale 1.745 m follows sheet 05 (`export/SOURCE.md`, Scale).

## What a check failure means

If a check fails on Wolf, report the exact defect (which check, the measured value, the file) back to Lynx for a re-export. Do not fix the
mesh, the weights or the clips in the editor, and do not rescale the asset to pass a check.

## Rebuild

```
node <archive>/tools/leonardo.mjs gen --asset warden --view front --seed 5101 --quality MEDIUM
node apps/vr/tools/assets/trellis.mjs run <archive>/work/warden_front_input.jpg warden_s42 42 <archive>/work
node apps/vr/tools/assets/trellis.mjs free
blender -b -P <archive>/tools/finish.py -- <archive>/work/warden_s42.glb <archive>/work Warden -90 4096 0.01 24000 Warden
blender -b -P <archive>/tools/rig_anim.py -- <archive>/work/Warden_finish.blend <archive>/work Warden <archive>/export/SK_Warden.fbx Warden
blender -b -P <archive>/tools/weapon_staff.py -- <archive>/work/Warden_rigged.blend <archive>/work Warden <archive>/export/SK_Warden.fbx
blender -b -P <archive>/tools/metrics.py -- <archive>/export/SK_Warden.fbx <archive>/export/metrics.json Cast Warden
blender -b -P <archive>/tools/render_evidence.py -- <archive>/export/SK_Warden.fbx <archive>/export Warden <archive>/work/Warden_rig.json Cast Warden
```

`<archive>` is `C:\Users\kazda\kiro\mage-arena-archive\A07-warden`. The `tools/` there are patched copies of `apps/vr/tools/assets/`
(the Warden palette set, rig style, staff script, metrics profile and evidence framing), because this run could not edit `apps/vr/tools/`.

## Next (not in this hand-off)

- Merge the Warden patches into `apps/vr/tools/assets/` (a run that may touch the tools), so the rebuild runs from the repo.
- T53 (Wolf): import the warden, run the pre-import checks, place it at the dais rail.
- After the T16 style pick: the warden's look (face, robe cloth, bronze finish); a Septima voice-line lip or gesture pass belongs to the campaign cards.
- A combat or ritual role for the `Cast` clip is not defined in `apps/vr/data/`; it needs a design card before the clip is timed to anything.
