# Arena environment kit - Lynx hand-off H1 (for Wolf)

Made on Lynx (Windows ARM64) on 2026-10-21 with Blender only (`SOURCE.md`). No Unreal Engine was run here, so nothing below is
a UE result. Wolf imports and places it under card `apps/vr/tasks/T42-arena-kit-import.md`. **The greybox stays the
fallback**: `AArenaGreybox` keeps building the boxes whenever the kit assets are missing.

## Landing path (in git, LFS)

`apps/vr/art/environment-kit/kit3d/`, reaching origin master through Git LFS (owner ruling 2026-10-21; `.gitattributes`
already routes `*.fbx` and `*.png` there). Wolf imports to **`Content/Environment/ArenaKit/`**.

| File | What |
|---|---|
| `export/SM_Arena_<Piece>.fbx` | 11 static meshes (FBX 7.4 binary): WallStraight, WallCurved, GateArch, TierStraight, TierCurved, TierStair, Dais, RunePad, Seat, Brazier, Banner |
| `manifest.json` | per piece: file, tris, verts, components, bounds (m), materials, pivot, what it replaces, sha256, bytes; the six materials and their colours; the **assembly**: every instance with location, yaw and scale in Unreal axes, and the triangle total |
| `metrics.json` | `env_kit.py metrics`, static profile, run on the FBX files re-imported fresh: checks S1-S9 per piece, K1-K4 for the kit |
| `kit_lineup.png` | every piece once, three-quarter orthographic view |
| `arena_seated_640x480.png` | the assembled arena from the seated eye: 1.2 m above the centre pad top (eye at -12, 0, 1.8 m), facing the arena centre (`lookTargetM`), 90 degree horizontal FOV |
| `arena_overview.png` | the assembled arena from above the far stands, for the reader |
| `SOURCE.md` | tools, script, date, "no AI step" |
| `work/` | not in git: `C:/Users/kazda/kiro/mage-arena-archive/A07-kit3d/work/env_kit.blend` |

## Pre-import checks, as measured on Lynx (`metrics.json`, all pass)

The T38 Wolf checks, read for static meshes: one mesh and no skeleton or clips; a triangle count recorded against the budget;
one flat colour per material, no embedded textures; origin at the base, +X forward, unit scale matching `FBody` (metres in
Blender, the same FBX export options as the characters). If a check fails on Wolf, report the exact defect for a Lynx
re-export instead of fixing the mesh in the editor.

| # | Check (per piece) | Result |
|---|---|---|
| S1 | one static mesh: 1 mesh, 0 armatures, 0 animation stacks | pass x 11 |
| S2 | materials only from the shared set, flat colour, 0 textures in the FBX | pass x 11 |
| S3 | closed (every edge in two triangles), winding consistent; none is open by intent | pass x 11 |
| S4 | no floaters: every part rests on z = 0 or touches a part that does | pass x 11 |
| S5 | origin at the base: min z 0, origin inside the footprint | pass x 11 |
| S6 | +X forward: walls, gate and tiers face +X with the body at -X and the treads rising away; banner cloth on +X; one brazier leg on +X; dais and seat X/Y extents per the layout; the pad is radially symmetric | pass x 11 |
| S7 | unit scale (object scale 1) and dimensions against the layout: wall 3.0 m high, body 0.45 m; tiers 4.65 m deep, treads 3.50 and 5.20 m; dais 5.2 x 8.8 x 0.6 m; seat 0.50 x 0.46 x 0.42 m; brazier rim 0.74 m, bowl 0.42 m; banner 3.3 m, cloth 1.15 m | pass x 11 |
| S8 | no degenerate triangles | pass x 11 |
| S9 | manifest matches the file (tris, verts, sha256) | pass x 11 |

| # | Check (kit) | Result |
|---|---|---|
| K1 | at most 6 materials in the whole kit, all from the shared set | pass: 6 (`M_Arena_Sand`, `_Stone`, `_StoneDark`, `_Wood`, `_Iron`, `_Cloth`) |
| K2 | the arena assembled per `arena-layout.json` within 60,000 tris | pass: **10,620** (10,368 for the 84 kit instances + 252 for a 64-sided sand floor stand-in) |
| K3 | every piece re-opened from its FBX | pass: 11 of 11 |
| K4 | the assembly covers the layout | pass: 32 of 32 wall chords (the gate covers 2), stands behind 30 chords, 3 pads, 3 seats, 1 dais, 4 braziers, 4 banners |

## Which layout entries each piece replaces

| Piece | Instances | Replaces in `AArenaGreybox::BuildShell` / `BuildProps` | Pivot |
|---|---|---|---|
| WallStraight | 20 | a ring-wall chord box (the side chords) | base, middle of the arena face |
| WallCurved | 10 | a ring-wall chord box where the ellipse's radius of curvature is under 12 m (three chords each side of both ends) | base, middle of the arena face |
| GateArch | 1 | the two wall chord boxes at the far (+X) end, theta -11.25..11.25 degrees, at (15.69, 0, 0), yaw 180 | base, middle of the arena face |
| TierStraight | 24 | the three stand boxes behind a side chord (two shortened ones flank each stair) | base, middle of the front edge |
| TierCurved | 10 | the three stand boxes behind an end chord | base, middle of the front edge |
| TierStair | 4 | an aisle in the stands behind chords 6, 9, 22 and 25 (about +-73 and +-107 degrees) | base, middle of the front edge |
| Dais | 1 | the dais box (`dais`) | base centre |
| RunePad | 3 | the pad disc cylinder and its 16 ring boxes (`pads`) | base centre, on the dais top |
| Seat | 3 | the seat box (`seat`), on the pad | base centre |
| Brazier | 4 | the brazier stand and bowl (`braziers`); the flame cone stays | base centre |
| Banner | 4 | the banner pole, crossbar and cloth (`banners`) | base of the pole |

Not replaced: the sand floor (`floor`), the spawn marker posts, the brazier flame cones, the threats and the hands.

## Placement

`manifest.json` `assembly.instances` lists every instance in Unreal axes (the layout's): `locationM` (x, y, z in metres),
`yawDeg` (about +Z, +X toward +Y) and `scale` (Y is the length along the chord). The ring follows `BuildShell`'s maths: 32
chords, the wall's arena face on the chord, x1.04 overlap, the tiers at wall + gap with the greybox's radius scale. Non-uniform
Y scale on a wall or tier instance is intended (0.87-1.08 on walls; 0.36-1.15 on straight tiers).

## Import notes

- Import `M_Arena_*` once: on the first FBX let the importer create the six materials (or make them from the colours in
  `manifest.json`), then import every other piece with material search set to find existing assets by name, so all 84
  instances share six materials and merge under T37 instancing.
- Static mesh, no skeleton, Combine Meshes on, normals from the file (flat), no LOD generation needed. Collision: simple box
  or the mesh itself for the dais, pads and seat; the walls and tiers are behind the play space.

## Next (not in this hand-off)

- The arcane warden (A07 sheet 05) as a rigged character: its own card.
- The crowd cards (A07 sheet 04) for the stands: their own card.
