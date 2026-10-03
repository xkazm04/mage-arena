# A02 - Greybox arena, seated desktop view, visible hands, threat-colour placeholders, screenshots

Status: open
Max turns: 260

## Goal
Nothing in the game is visible yet: clips, sigils and the ward exist only as data and tests. Build the first view the
owner can look at - greybox, no art (`docs/DECISIONS.md`: art comes last; the greybox must honour the threat colour
language) - and capture it automatically, so every later card can be judged by eye as well as by tests. Also measure
the desktop proxies of the Quest budget (plan section 6.1 table; gate D-G4 counts).

## Read first
- `CLAUDE.md`, `docs/DECISIONS.md`, `docs/DESKTOP-INPUT.md`, `docs/PROJECT-PLAN.md` section 6 (6.1 body and space,
  threat language, performance budget) and section 4 (D-G4 row)
- `apps/vr/Game/Source/MageArenaVR/**` (hand source, clip player, sigil and ward subsystems, game mode, controller, pawn)
- `apps/vr/art/environment-kit/*.jpg` and `apps/vr/art/model-sheets/*.jpg` for proportions only (no textures in greybox)
- `apps/vr/data/pinned/.../combat.json` (`threatLanguage`, projectile and telegraph timing where present)

## Deliverables
1. **Layout data** `apps/vr/data/vr/arena-layout.json` (VR-owned, not pinned): oval sand floor about 32 x 20 m; ring
   wall about 3 m high; three stepped stand tiers behind it; a raised dais at one end (about 0.6 m) with **three rune pads
   in an arc, left / centre / right, about 3 m apart**; a backless low stone seat block on each pad; seated eye height
   1.2 m above the pad top, facing the arena centre; enemy spawn markers at 8 m and 15 m inside a +/-70 degree arc;
   a few braziers and banner poles. Units and every value documented in the file.
2. **Procedural greybox** (C++, no binary assets): a world subsystem or builder actor that spawns the layout at
   BeginPlay in the existing default map using the engine's basic shapes (`/Engine/BasicShapes/*`) and
   `BasicShapeMaterial` dynamic instances for flat colours (sand, stone, darker dais, pad rings). Low triangle counts.
   One directional light plus sky light at most; no dynamic shadows required.
3. **Seated desktop pawn**: camera at the active pad's seated eye height, looking at the arena; mouse look clamped to
   the threat arc (+/-100 degrees yaw, modest pitch); no locomotion. A `BlinkToPad(int)` function snaps between pads with
   a short camera fade (comfort vignette). Bind it ONLY to debug keys `F5/F6/F7` marked L3 debug - the A/D/S gesture
   path is a later card (blink detection) and must not be bypassed.
4. **Visible hands**: draw the active `IHandSource` frames as a simple hand skeleton (small spheres at the 26 joints,
   thin capsules for bones) relative to the camera at the clip's coordinates, so key-played clips (1-4, Q, Space,
   Shift/Ctrl variants) are visible. When a sigil is cast or rejected, flash a ring in front of the hand (green / grey);
   when a ward is raised, show a translucent 140-degree arc in front of the off hand.
5. **Threat-colour placeholders**: a demo spawner (console command `MageArena.Demo.Threats`) that launches, in turn, from
   the spawn markers toward the active pad at chest height: a water orb (turquoise), a fire bolt (vermilion), a steel
   spear (white/grey elongated), and an unblockable (black core with a red rim), each with a shrinking ground ring
   telegraph at its landing point (ring colour = element; unblockable ring red). Visual only (no damage yet).
6. **Automated capture**: a script `apps/vr/tools/capture-greybox.ps1` that runs the game (`-game -RenderOffScreen
   -ResX=1920 -ResY=1080`, real RHI, no `-nullrhi`), plays a scripted sequence (initial view; a `sigil-line2` clip with
   the cast flash; a ward raise with the arc; the four threats mid-flight; a blink to the left pad) and saves
   screenshots to `runs/A02/shots/*.png`. It also records draw calls, primitives/triangles and frame time (e.g. `stat
   RHI` / `stat unit` or the CSV profiler) into `runs/A02/budget.json`, labelled **desktop proxy - not Quest truth**.
7. **Tests** (`MageArena.Greybox.*`, nullrhi-safe): layout loads; pad spacing within 2.5-3.5 m; seated eye height 1.2 m
   above the pad; every spawn marker inside +/-70 degrees and at 8 or 15 m; the demo spawner produces the four threat
   kinds with the correct colour classes.
8. `runs/A02/REPORT.md`: files, test and capture outputs, the budget numbers against the plan's targets (draw calls
   <= 150, triangles <= 350k), and what is still placeholder.

## Constraints
- Do not commit, push, or touch other repositories; do not edit `docs/`, `apps/vr/data/pinned/`, or the art folders.
- No binary `.uasset`/`.umap` (C++, config, data only). Keep the Meta XR plugins enabled; the desktop view is the
  focus. One Unreal build at a time. All existing `MageArena.*` tests must stay green.

## Acceptance (the orchestrator re-runs these and looks at every screenshot)
```
powershell -NoProfile -File apps/vr/tools/build.ps1
"C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "C:\Users\kazda\kiro\mage-arena-vr\apps\vr\Game\MageArenaVR.uproject" -ExecCmds="Automation RunTests MageArena;Quit" -unattended -nullrhi -nosplash -log
powershell -NoProfile -File apps/vr/tools/capture-greybox.ps1     # screenshots + budget.json
```
