# Arena crowd cards (A07 follow-up) - source record (Lynx)

Four spectator cut-outs from one model sheet, a quad per figure, and a placement set for the kit's stands. **No new AI step**: no
Leonardo, TRELLIS.2, ComfyUI or other generator was called, 0 credits. The only AI-made input is the committed sheet below.
No Unreal Engine was run; nothing here is a UE result.

- Date: 2026-10-22, made on Lynx (Windows ARM64).
- Script: `apps/vr/tools/crowd/crowd.py` (Blender's bundled Python: `bpy`, `numpy`, stdlib; nothing installed) and
  `apps/vr/tools/crowd/verify.mjs` (Node built-ins).

## The sheet

| | |
|---|---|
| Path | `apps/vr/art/environment-kit/04-crowd-cards.jpg` (Git LFS), 1280 x 720 |
| sha256 | `51a993509d3a758ec1153d7960a6e0868aaaa9a30eb3178d63d306f3301e28e6` (161,799 bytes) |
| Generator | **Grok (xAI)**, not Leonardo: `apps/vr/art/environment-kit/manifest.json` item `04-crowd-cards` says `tool: grok`, `model: null`, 3 attempts, pass 2 kept, 15 s, 16:9 requested and 1280x720 delivered. Regeneration reasons: "pass 1 had four figures and no standing clap, and the pointing figure was in profile"; "pass 3 cropped the upper row, used two scales, and repeated the clap and the cheer; pass 2 kept". The prompt is `prompts.json` item `04-crowd-cards`. There is no `leonardo-log.json` in `environment-kit/`, and no credits were spent on this sheet |
| What it shows | four figures, front view, grey clay on a light grey grid: seated and calm, seated and leaning, standing and cheering with raised arms, standing and pointing. The sheet has **no standing clap** (the prompt asked for one; the kept pass drew the pointing figure instead) |

## What the script does

1. Reads the sheet through Blender's image loader (exact 8-bit values).
2. Background removal: the grid is modelled from the empty top strip and left strip (flat grey 189, lines darker), a pixel is
   background when it is within 5 of that model and grey, and the background is flood-filled in from the border (closed pockets
   that match it are dropped too). Hair and cloth highlights share the grey of the grid lines but not their position, so
   they stay. The mask is then eroded by 1 px so no light halo survives.
3. The four figures are the four runs of occupied columns. The thin stone bench under the two seated figures is cut off
   (rows 437-451 of the sheet, outside the span the figure covers just above): the stands have their own treads. A short
   ledge of a few centimetres remains at the figure's sides where the robe narrows.
4. Packing: one atlas, first power-of-two size up to 2048 that fits, 8 px gutter around each figure with the figure's own
   colour bled into it. Alpha is 0 or 255 (a mask, no blend). Native resolution, no resampling.
5. One quad per figure (2 triangles, flat normal on +X), pivot at the feet centre, one material `M_Crowd_Atlas` with no texture
   in the FBX, FBX 7.4 binary with the kit's export options. The re-imported FBX is checked (tris, base at z = 0, height).
6. Placement: every `TierStraight` and `TierCurved` instance of `kit3d/manifest.json` (34; the four `TierStair` aisles stay
   empty) gets one row per tread, in the kit's Unreal axes, with a seeded RNG.
7. The only non-determinism in a re-run was the FBX header time and the exporter's object ids (`hash()` is randomised per Python
   process); the script pins both, so a re-run reproduces every sha256 in `manifest.json`.

## Scale (the sheet's two classes are not at one scale)

| Class | Rule | Result |
|---|---|---|
| Standing | `StandingPoint` is **1.75 m** head to foot (251.4 px/m) | `StandingCheer` follows at 1.99 m with arms raised |
| Seated | `SeatedCalm` is **1.30 m** head above the feet (320.8 px/m), which puts the sheet's bench at 0.47 m | `SeatedLean` follows at 1.18 m |

## Reproduce

```
B=C:/Users/kazda/ai/tools/blender-5.2.2-windows-arm64/blender.exe
$B -b -P apps/vr/tools/crowd/crowd.py -- build  apps/vr/art/environment-kit/04-crowd-cards.jpg apps/vr/art/environment-kit/crowd apps/vr/art/environment-kit/kit3d C:/Users/kazda/kiro/mage-arena-archive/A07-crowd/work
$B -b -P apps/vr/tools/crowd/crowd.py -- render apps/vr/art/environment-kit/crowd apps/vr/art/environment-kit/kit3d apps/vr/data/vr/arena-layout.json
node apps/vr/tools/crowd/verify.mjs
```

`build` writes `crowd_atlas.png`, `export/SM_Crowd_*.fbx`, `placement.json` and `manifest.json`, and saves the work scene to the archive
(`crowd.blend`, not in git). Verified 2026-10-22: two runs in a row gave identical sha256 for the atlas, the four FBX files,
`placement.json` and `manifest.json`. `render` imports the crowd FBX files and the kit's FBX files (not the build scene) and writes
`crowd_lineup.png` and `stands_seated_640x480.png`; the renders are EEVEE evidence and are not claimed byte-reproducible.

## Known limitations

- Single-sided cards that face the arena centre. Seen from behind or from above at a steep angle they vanish or read as flat.
- The seated cards imply the 0.47 m bench the kit does not have (the robe hides it). Their feet are on the tread top.
- Grey clay with the sheet's one muted red accent (cloak lining and hem); the colour is baked from the sheet. It awaits the T16 style pick.
- Hair and the raised hands have a slightly ragged 1-2 px edge from the cut-out.
- Four variants only, so neighbours repeat. Per-card scale (0.92-1.08), yaw and 0.12 m lateral jitter hide some of it.
- No crowd behind the gate, none on the stairs.
