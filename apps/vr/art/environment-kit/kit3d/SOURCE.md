# Arena environment kit (A07 3D pass) - source record (Lynx)

The entrant's own Blender work. **No AI step**: no Leonardo, no TRELLIS.2, no ComfyUI, no other generator or paid service,
0 credits. Every piece is parametric geometry written by hand in a script (bmesh), sized from the layout and shaped after the
A07 sheets, which are reference only and are not shipped or traced.

- Date: 2026-10-21, made on Lynx (Windows ARM64). No Unreal Engine was run; nothing here is a UE result.
- Script: `apps/vr/tools/assets/env_kit.py` (three modes: `build`, `metrics`, `render`). Patterns copied from `weapon.py`
  (bmesh lofts, FBX export options), `metrics.py` (FBX file parse, mesh topology) and `render_evidence.py` (EEVEE evidence).
- Dimensions: `apps/vr/data/vr/arena-layout.json`, read at run time and never edited (wall, stands, dais, pads, seat,
  braziers, banners, colours, seated eye, camera FOV).
- Shape reference: `apps/vr/art/environment-kit/01-stands-kit.jpg`, `02-wall-gate-kit.jpg`, `03-arena-props.jpg`, with the
  A07 review in `apps/vr/tasks/BACKLOG.md` applied: **no crenellations** (a plain parapet: string course and coping), and the
  player seat is a **backless low block**.

## Tools and versions

| Step | Tool | Version |
|---|---|---|
| Geometry, export | Blender 5.2.2 LTS (Windows ARM64), `C:/Users/kazda/ai/tools/blender-5.2.2-windows-arm64/blender.exe` | bmesh, FBX exporter (FBX 7.4 binary) |
| Checks | the same Blender, `env_kit.py metrics` | FBX re-imported fresh per piece, numpy, `mathutils.bvhtree` |
| Evidence renders | the same Blender, `env_kit.py render` | EEVEE, from the exported FBX files |

## How it was made (reproduce)

```
B=C:/Users/kazda/ai/tools/blender-5.2.2-windows-arm64/blender.exe
$B -b -P apps/vr/tools/assets/env_kit.py -- build   apps/vr/data/vr/arena-layout.json apps/vr/art/environment-kit/kit3d C:/Users/kazda/kiro/mage-arena-archive/A07-kit3d/work
$B -b -P apps/vr/tools/assets/env_kit.py -- metrics apps/vr/data/vr/arena-layout.json apps/vr/art/environment-kit/kit3d
$B -b -P apps/vr/tools/assets/env_kit.py -- render  apps/vr/data/vr/arena-layout.json apps/vr/art/environment-kit/kit3d
```

`build` writes the 11 FBX files and `manifest.json` and saves the work scene to the archive (`env_kit.blend`, not in git).
`metrics` re-imports every FBX into an empty scene and writes `metrics.json`. `render` imports the FBX files (not the build
scene) and writes the renders. The geometry is deterministic; an FBX re-export differs in its header timestamp only, so
`manifest.json` records the sha256 of the files committed here, and `metrics` check S9 fails if they differ.

## The pieces

All triangulated, flat shaded, one mesh per FBX, origin on z = 0, +X forward, metres, unit scale, no textures.

| Piece | Tris | Built from |
|---|---|---|
| `SM_Arena_WallStraight` | 48 | body 0.45 x 3.0 m and three plain bands (plinth, string course at 2.10 m, coping), 3.0 m long at the face, ends splayed by half a 32-gon corner |
| `SM_Arena_WallCurved` | 272 | the same profile swept on an 8 m inner radius over 2.2 m of chord, 8 steps |
| `SM_Arena_GateArch` | 536 | 4.0 m wide, 3.6 m high block, a 2.2 m opening with a round arch springing at 1.6 m, arch rings on both faces, coping, pier plinths; a closed wooden portcullis (7 uprights, 4 rails) with iron spikes |
| `SM_Arena_TierStraight` | 28 | the three tier section in one piece: treads 1.55 m deep at 3.50, 4.35 and 5.20 m, solid to the ground, 3.0 m at the front, splayed ends |
| `SM_Arena_TierCurved` | 140 | the same section swept on 8.65 m (the curved wall's radius plus wall and gap) |
| `SM_Arena_TierStair` | 52 | an aisle section 1.2 m wide: six half rises of 0.425 m, same footprint and top |
| `SM_Arena_Dais` | 36 | 5.2 x 8.8 x 0.6 m, plinth, waist and top slab |
| `SM_Arena_RunePad` | 576 | disc 0.9 m radius, 0.04 m thick, one carved circular groove at 0.70-0.86 m (the layout's ring), 15 mm deep, its floor in Sand; 48 sides |
| `SM_Arena_Seat` | 36 | backless low block 0.50 x 0.46 x 0.42 m: plinth, waist and top slab |
| `SM_Arena_Brazier` | 408 | iron bowl 0.42 m radius, 0.26 m deep, its rim at 0.74 m, on three splayed legs with foot pads (one leg on +X); 24 sides |
| `SM_Arena_Banner` | 92 | pole 3.3 m (0.06 m radius) with an iron cap, crossbar 1.15 m, cloth 1.15 x 1.85 x 0.05 m with a swallowtail foot, hung on the +X side |

Materials, the one shared set (flat base colours, linear, from the layout's `colours`): `M_Arena_Sand` (sand),
`M_Arena_Stone` (stone), `M_Arena_StoneDark` (dais), `M_Arena_Wood` (bannerPole), `M_Arena_Iron` (brazier),
`M_Arena_Cloth` (bannerCloth).

## Known limitations

- Greybox finish: flat colours, no bevels or stone joints, no LODs (the pieces are small enough that LOD0 is the only level).
- The assembly fits the straight and curved pieces to the ellipse's 32 chords by a per-instance Y scale (0.87-1.08 on the
  walls, 0.36-1.15 on the straight tiers where an aisle shortens them); the ring is a polygon like the greybox's, not a true
  ellipse.
- The stair modules sit between two shortened straight tiers; their splayed ends overlap the aisle by a few centimetres at
  the back of the top tread.
- No stands behind the gate: the gate is a passage. The gate's portcullis is part of the gate mesh and cannot open; a split
  piece is a later card if a scene needs it.
- Brazier flames, embers and the threat telegraphs stay VFX or greybox parts; they are not in the kit.
- The renders' lighting is evidence lighting (the layout's warm colour, raised to 25 degrees so faces read), not the UE dusk.
