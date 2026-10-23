# Cinder hound (T15, Bout 2) - Lynx hand-off H1

Made on Lynx (Windows ARM64) on 2026-10-21 on the conscript and slinger route (Leonardo, TRELLIS.2, Blender). No Unreal Engine
was run here, so nothing below is a UE result. Wolf imports it and runs the T38 pre-import checks again
(`apps/vr/tasks/T38-first-animated-opponent.md`, "Hand-off"); the beast checks that replace the humanoid ones are listed below.

## Landing path (in git, LFS)

`apps/vr/art/characters/cinder-hound/export/`, committed on a Lynx branch; it reaches origin master through Git LFS (owner ruling
2026-10-21). Every file below was copied byte for byte from the Lynx archive
`C:\Users\kazda\kiro\mage-arena-archive\T15\cinder-hound\export\`; sha256 and size per file are in `manifest.json`.

| File | What |
|---|---|
| `SK_CinderHound.fbx` | the rigged mesh (26-bone quadruped skeleton) and the four takes `Idle`, `Attack`, `Hit`, `Death` (FBX 7.4 binary, 1.4 MB) |
| `SOURCE.md` | tool versions, prompts and seeds, the TRELLIS runs, the scale record, the clips, known limitations |
| `metrics.json` | `apps/vr/tools/assets/metrics.py` (profile `CinderHound`) run on the exported FBX, re-imported fresh, attack clip `Attack` |
| `CinderHound_turnaround.png` | EEVEE, 8 views every 45 degrees, Idle frame 0 |
| `CinderHound_game_640x480.png` | seated view: eye 1.2 m, the hound 6 m away, 90 degree horizontal FOV |
| `CinderHound_clip_{Idle,Attack,Hit,Death}.png` | one still strip per clip, at its authored key frames, front three-quarter view |
| `work/` | not in git: stays in the archive `C:\Users\kazda\kiro\mage-arena-archive\T15\cinder-hound\work\` (TRELLIS raw GLBs seeds 42 and 7, the side-view crop, the finish, parts and rigged `.blend` files, the base-colour bake PNG, the step JSONs) |

The FBX and the seven PNGs are LFS objects; `SOURCE.md` and `metrics.json` are plain text. The concept JPGs are LFS objects too
(`apps/vr/art/**/*.jpg`).

## Metrics summary (`metrics.json`)

- One skinned mesh `SM_CinderHound`: 5,894 verts, 11,784 tris (budget 12,000: three hounds may be on screen at once), one
  component, 0 floaters, watertight, winding consistent.
- One skeleton, 26 bones: `root`, `pelvis`, `spine_01-03`, `neck_01-02`, `head`, `jaw`, `tail_01-03`,
  `front_upper|front_lower|front_paw_l|r`, `back_thigh|back_shin|back_hock|back_paw_l|r`. The armature node is `Armature`.
- Height 1.359 m (target 1.36 m from sheet 05, -0.1 %); length 1.926 m; min z 0.0001 m; bbox centre xy (0.001, 0.0) m;
  object scale 1.
- Weights: 0 unweighted vertices, max |sum - 1| = 0, at most 4 influences per vertex.
- Clips (30 fps): `Idle` 61 frames, `Attack` 36 (ember spit, release at frame 18 = the overlay's 0.6 s windup), `Hit` 25,
  `Death` 61 (last pose held).
- Materials: `M_CinderHound_Cinder`, `M_CinderHound_Ember`, `M_CinderHound_Paw`; each one flat colour; 0 textures in the FBX.
- Leonardo: 2 calls, 28 credits (`leonardo-log.json`). TRELLIS.2: 2 runs (seed 42 rejected, seed 7 used, 120 s).

## Pre-import checks, as measured on Lynx (`metrics.py` profile `CinderHound`)

| # | Check | Result | Value |
|---|---|---|---|
| 1 | one skeleton | pass | 1 armature, 1 skin deformer |
| 2 | one skinned mesh | pass | 1 mesh, 1 skinned |
| 3 | exactly the four clips, none empty | pass | stacks ['Idle', 'Attack', 'Hit', 'Death']; frames Idle 61, Attack 36, Hit 25, Death 61 |
| 4 | LOD0 triangle count within budget | pass | 11784 tris (budget 12000, three hounds on screen at once) |
| 5 | one flat colour per material, no embedded textures | pass | 3 materials, 0 textures |
| 6 | origin at the feet | pass | min z 0.0001 m, bbox centre xy [0.001, 0.0] m |
| 7 | +X forward | pass | front_paw_l y 0.085, front_paw_r y -0.085, head dx 0.067 |
| 8 | unit scale, height 1.36 m +-10 % | pass | height 1.359 m (-0.1 %), object scale [1.0, 1.0, 1.0] |
| 9 | weights normalised | pass | max |sum-1| 0.0, unweighted 0, max influences 4 |
| 10 | no floaters | pass | 1 component(s), 0 floaters |

The humanoid T38 checks 4, 7 and 8 (18-30k tris, mannequin hands and toes, 1.62-1.98 m) do not apply to a quadruped; the profile
replaces them with the hound's budget, its own bones and the sheet scale. Check 10 is new for the beasts.

## For Wolf

- Scale: 1.36 m to the ear tips follows sheet 05 (see `export/SOURCE.md`, Scale). The greybox hound is a 0.40 m tall box; if the
  game wants the greybox size, scale the actor, not the asset.
- The ember spit leaves the mouth: the jaw tip is the `jaw` bone's tail (about 0.19 m ahead of its head); a socket on `jaw` or
  `head` is where the ember VFX starts. Death bursts, embers and sparks are UE VFX; the mesh has none.
- `Attack` matches the VR overlay's spit (windup 0.6 s, release frame 18). The pinned melee bite is not played by the VR game.

## Rebuild

```
node apps/vr/tools/assets/leonardo.mjs gen --asset cinder-hound --view front --seed 4301 --quality MEDIUM
node apps/vr/tools/assets/leonardo.mjs gen --asset cinder-hound --view turnaround --seed 4302 --quality MEDIUM
# crop columns 0-821 of the turnaround (the side view) to <archive>/work/hound_side_input.png
node apps/vr/tools/assets/trellis.mjs run <archive>/work/hound_side_input.png cinder_hound_s7 7 <archive>/work
node apps/vr/tools/assets/trellis.mjs free
blender -b -P apps/vr/tools/assets/finish.py -- <archive>/work/cinder_hound_s7.glb <archive>/work CinderHound 0 4096 0.006 11500 CinderHound
blender -b -P apps/vr/tools/assets/beast_parts.py -- <archive>/work/CinderHound_finish.blend <archive>/work CinderHound CinderHound
blender -b -P apps/vr/tools/assets/rig_anim.py -- <archive>/work/CinderHound_parts.blend <archive>/work CinderHound <archive>/export/SK_CinderHound.fbx Hound
blender -b -P apps/vr/tools/assets/metrics.py -- <archive>/export/SK_CinderHound.fbx <archive>/export/metrics.json Attack CinderHound
blender -b -P apps/vr/tools/assets/render_evidence.py -- <archive>/export/SK_CinderHound.fbx <archive>/export CinderHound <archive>/work/CinderHound_rig.json Attack CinderHound
```
