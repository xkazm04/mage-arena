# Forward-path feature allowance, 2026-10

The closed list of rendering features the Quest build may use on the mobile forward path, one row per feature. Each
row names the line that enforces it. A row with no enforcing line is marked **unenforced**, and until it has one the
code is the allowance, not this file and not the plan.

- Written 2026-10-21 against `master` at `d25ecc7`, from an audit of config, data, code and the plan (ai-registry
  technique `forward-path-feature-allowance`, subject `standalone-headset-frame-budgets`).
- Desk phase: no row carries a device cost yet. A row that gains one records it here with the capture it came from.
- When two content classes want different rows, the stricter row is the default and the looser one is an exception
  with its owner.
- This file changes no config. It names what the tree already decides and what it leaves to engine defaults.

## Allowance

| Row | Allowed | Enforced by | Status |
|---|---|---|---|
| Shading path | mobile forward on Quest; desktop forward on the desk | `apps/vr/Game/Config/DefaultEngine.ini:6`, `apps/vr/Game/Config/Android/AndroidEngine.ini:8` | decided |
| Multisampling | 4x MSAA on desktop and Quest | `apps/vr/Game/Config/DefaultEngine.ini:7-10` (STACK F3) | decided; resolve on chip depends on the post-pass row |
| Real-time shadows | none; blob shadows only | `apps/vr/data/vr/arena-layout.json` `lighting.castShadows`, every greybox `SetCastShadow(false)`, `docs/PROJECT-PLAN.md` section 7 Lighting row | decided |
| Dynamic lights | plan: one dynamic light at most, baked lightmaps | **unenforced.** The greybox spawns a movable directional and a movable sky light (`apps/vr/Game/Source/MageArenaVR/Greybox/ArenaGreyboxActor.cpp` `BuildLights`) and no lightmaps | open: does a movable sky light count against the one? Due at the art pass, before lightmaps are baked |
| Post passes | proposed: none by default; a post pass is allowed only by name, with its device cost | **unenforced.** No line sets `r.MobileHDR`, so the engine default (on) runs the mobile post chain on Quest; the camera exposure and bloom overrides (`MageArenaPawn.cpp` `ConfigureFromLayout`) apply to it; `r.Mobile.UseHWsRGBEncoding=True` does nothing while it is on (STACK F4) | open: `r.MobileHDR=False` at V1, after checking that the blink's `StartFade` still renders with it off |
| Full-screen translucency | none, except the blink vignette (four camera quads) | `docs/PROJECT-PLAN.md` section 7 Particles row; `MageArenaPawn.cpp` `UpdateVignetteQuads` | decided |
| Screen-space effects (SSR, SSAO) | none | not built by the mobile renderer on this path (`MOBILE-INSTANCING-2026-10.md` section 2.1); no project setting | decided by the engine; a setting that enables one needs a row here first |
| Decals | proposed: mesh decals, or depth-projected decals on Vulkan; never a decal that reads surface normals | **unenforced.** No decal in the tree. Style 01 (`docs/ART-STYLE-STORYBOARD.md`) assumes mosaic edge decals. On Vulkan the 5.8.3 mobile decal path reads depth through a subpass fetch even with Mobile HDR off (`Engine/Source/Runtime/Renderer/Private/MobileDecalRendering.cpp`, `DoesPlatformSupportDecals`) | open: decide the decal form at the style pick |
| Foveation | off (engine default) | **unenforced** (STACK F6) | open: V1, not headless-verifiable |
| Desk parity | the desk runs desktop forward, not mobile forward, so mobile-only rows are unmeasured on the desk | every desk number is labelled not Quest truth (`docs/research/BUDGET-DG4-2026-10.md` section 3) | decided |

Ten rows: six decided, four open. The decided rows are the ones an earlier finding forced; the open rows are the ones
no finding had reached.
