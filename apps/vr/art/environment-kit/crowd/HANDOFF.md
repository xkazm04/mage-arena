# Arena crowd cards - Lynx hand-off (for Wolf)

Made on Lynx (Windows ARM64) on 2026-10-22 with Blender and Node only (`SOURCE.md`). No Unreal Engine was run, so nothing here
is a UE result. Wolf imports and places it under card `apps/vr/tasks/T52-crowd-cards-import.md`, after T42 (the kit) is in.
**The crowd is optional**: with no crowd assets the stands are the bare kit tiers (or the greybox) and everything else works.

## Landing path (in git, LFS)

`apps/vr/art/environment-kit/crowd/`; Wolf imports to **`Content/Environment/ArenaKit/Crowd/`** (under the kit's
`Content/Environment/ArenaKit/`). `.gitattributes` already routes `*.fbx` and `*.png` through LFS.

| File | What |
|---|---|
| `crowd_atlas.png` | 512 x 1024 RGBA8, straight alpha (0 or 255), 8 px colour-bled gutter; the four figures, native resolution |
| `export/SM_Crowd_<Variant>.fbx` | 4 quad cards (2 tris each, FBX 7.4 binary): SeatedCalm, SeatedLean, StandingCheer, StandingPoint |
| `manifest.json` | per variant: file, UV rect (v up from the atlas bottom), pixel rect, height and width (m), tris, verts, bounds, sha256; the atlas; the sheet and its provenance; the placement summary (470 cards, 940 tris) |
| `placement.json` | every card: `variant`, `locationM` (Unreal axes, metres, the pivot at the feet on the tread top), `yawDeg` (about +Z, +X toward +Y), uniform `scale` |
| `crowd_lineup.png` | the four cards side by side, front view, with 0, 1 and 2 m ticks |
| `stands_seated_640x480.png` | the kit arena plus the crowd from the seated eye (-12, 0, 1.8 m), 90 degree horizontal FOV: the same eye as `kit3d/arena_seated_640x480.png` |
| `SOURCE.md` | the sheet (path, sha256, provenance), the method, the command line |

## Cards

| Variant | Height (m) | Width (m) | UV rect (u0 v0 u1 v1) | Cards placed |
|---|---|---|---|---|
| SeatedCalm | 1.300 | 0.680 | 0.0156 0.5850 0.4414 0.9922 | 141 |
| SeatedLean | 1.175 | 0.670 | 0.4727 0.6240 0.8926 0.9922 | 124 |
| StandingCheer | 1.989 | 0.855 | 0.0156 0.0811 0.4355 0.5693 | 110 |
| StandingPoint | 1.750 | 1.050 | 0.4668 0.1396 0.9824 0.5693 | 95 |

Seated height is the head above the feet (1.30 m); standing is 1.75 m. The cheering card is taller because its arms are raised.

## Placement

470 cards, 940 tris (the kit's assembly is 10,620), on the 34 straight and curved stand tiers: three rows per tier (treads at
z = 3.50, 4.35 and 5.20 m, with 136, 160 and 174 cards), 0.55 m apart along the row, 0.30 m clear at each end, mid-tread.
The rows follow the tier instance's Y scale; on a curved tier they follow the 8.65 m arc. Cards face the arena centre;
alternate cards sit 6 cm back so neighbours do not z-fight. The four `TierStair` aisles are empty. Seed `20261022`.

## Pre-import checks (Lynx measured 2026-10-22; Wolf re-checks)

| # | Check | Lynx result |
|---|---|---|
| C1 | each FBX opens: 1 mesh, 0 armatures, 0 animation stacks, 4 verts, 2 tris, one material slot `M_Crowd_Atlas`, no embedded texture | the build re-imports each FBX and asserts tris and height; pass x 4 |
| C2 | origin at the feet: min z 0, y centred between the feet, x = 0 | pass x 4 (`manifest.json` `boundsM`) |
| C3 | +X forward: the card's normal is +X and the figure reads upright and unmirrored from +X; StandingPoint points to the viewer's right | pass in Blender (`crowd_lineup.png`); **Wolf: confirm it is not mirrored after the import** (the kit pieces were symmetric, so the Blender-to-UE axis flip was never exercised on a figure) |
| C4 | unit scale: StandingPoint 175 cm, SeatedCalm 130 cm in UE | `manifest.json` `variants[*].heightM` |
| C5 | atlas: power of two, at most 2048, alpha 0 or 255 only | 512 x 1024, pass (`node apps/vr/tools/crowd/verify.mjs`) |
| C6 | every sha256 in `manifest.json` matches the file | pass (`verify.mjs`) |

A failed check goes back to Lynx as an exact defect for a re-export; do not repaint the atlas or fix the cards in the editor.

## Import notes

- Atlas: sRGB, mips on (the gutter is colour-bled for that), bilinear; ASTC on Quest, BC7 on desktop. Not a UI texture group.
- Material `M_Crowd_Atlas` (make it once, one for all four meshes): atlas as Base Color, atlas alpha as Opacity Mask, Blend Mode
  **Masked** (clip 0.5), the same lit basic setup as the kit's `M_Arena_*`, two-sided off, cast shadow off. Not translucent:
  that would sort 470 cards and cost overdraw.
- Meshes: static, no collision, no LOD generation, normals from the file. One FBX per variant, so there is nothing to combine.
- Placement: one `UInstancedStaticMeshComponent` per variant (4 in all), all on the same material, instances from
  `placement.json` (metres to centimetres, `yawDeg`, uniform `scale`); no collision, no shadows. State in the T52 report whether
  `placement.json` is staged with the game or ported to a data table.
- The cards are **grey clay** with the sheet's one muted red accent baked into the atlas. They await the **T16 style pick**: a
  later pass repaints or regenerates the atlas against the chosen style (a texture swap; the meshes and placement stay).

## Next (not in this hand-off)

- The arcane warden (A07 sheet 05, `05-warden-turnaround.jpg`) as a rigged character: a TRELLIS GPU job, on its own run.
