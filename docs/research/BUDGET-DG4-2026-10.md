# D-G4 desktop count budgets: three 60 s runs of the stress scene

**Label: desktop proxy, synthetic clip-driven hands, not Quest truth; cannot prove 72 Hz.**

This fills the count rows of gate D-G4 (`docs/PROJECT-PLAN.md:218`, the budget table at `:358-371`, the 11 Oct
evidence row at `:512`). It replaces the A02 proxy (`docs/research/BASELINE-2026-10-07.md:385-392`), which measured
only the empty greybox. A failed row is evidence too, as the plan says, so the scene was not tuned to pass.

## 0. Provenance

- **Build hash:** `8bcd3351f23dd37247674342ed7abdd1882ba4a5` (`8bcd335`, `feat(budget): D-G4 stress scene capture mode`).
  The editor binaries were built from the C++ in that commit with
  `powershell -NoProfile -File apps/vr/tools/build.ps1`, which exited 0. The last edit before the commit was to
  `capture-budget.ps1` only, so no C++ changed after the build. All four launches below ran at that commit.
- **Date:** 2026-10-07, about 22:49-22:57 local time.
- **Engine:** UE 5.8.3, `UnrealEditor-Cmd.exe -game`, Development, D3D12 (`driver.rhi`), forward shading from the
  project config.
- **Machine:** GPU NVIDIA GeForce RTX 4090 (`GRHIAdapterName`), CPU AMD Ryzen 7 7800X3D 8-Core Processor
  (`FPlatformMisc::GetCPUBrand`). Other workers were building and testing on the same machine. Each launch waited
  until no `UnrealEditor`, `UnrealEditor-Cmd`, `UnrealBuildTool` or `ShaderCompileWorker` was running.
- **Command:** `powershell -NoProfile -File apps/vr/tools/capture-budget.ps1 -Runs 3 -Prefix run` (exit 0). Each
  launch is:

  ```
  UnrealEditor-Cmd.exe <repo>/apps/vr/Game/MageArenaVR.uproject -game -RenderOffScreen -ResX=1920 -ResY=1080
    -unattended -nosplash -nohmd -stdout -FullStdOutLogOutput -log -abslog=<repo>/runs/DG4/runN/capture.log
    -MageArenaGreyboxCapture -MageArenaBudgetStress -MageArenaBudgetRun=runN -csvprofile
  ```

  The per-launch timeout is 10 minutes, and the driver gives up by itself at 330 s. No run came near either limit:
  the launches took 84, 89 and 121 s of wall time.
- **Success gate:** `MAGEVR_BUDGET_DONE` plus two `MAGEVR_BUDGET_POP` lines at full count, never the editor exit code.
- **Raw output:** `runs/DG4/runN/` (gitignored): `capture.log`, `driver.json`, `samples.json` (every frame),
  `dg4-runN.csv` (the CSV profile of the sample window) and `mesh-draw-dumps/`. The committed summaries are
  `docs/research/budget-dg4/run1.summary.json`, `run2.summary.json`, `run3.summary.json` and `diag1.summary.json`.

## 1. The scene as built

The scene is built from the parts the game draws today (`Budget/BudgetStressPlan.cpp`, `Budget/BudgetStressDriver.cpp`):

| Element | Count | Parts each | How it is built |
|---|---|---|---|
| Greybox arena | 1 | as built | `AArenaGreybox`, unchanged: shell, props, lights, sky panels. |
| Enemy proxies | 20 | 3 (body, HP back, HP fill) | The sizes, colours and HP bar of `SessionPresentation.cpp:91-160` and `:1094-1139`. Each part is one `UStaticMeshComponent` with its own tinted unlit MID, as `MakePart` does. Kinds cycle conscript, slinger, cinder_hound, mire_maw (5 each). 10 stand on the 8 m ring and 10 on the 15 m ring (the spawn-marker radii), measured from the centre pad. |
| Projectiles | 100 | 1 | 75 water spheres (30 cm) and 25 steel spears (cylinder, 22 cm x 1.7 m, arcing 0.75 m), as `ShotFor` builds them. The spear's ground ring is not drawn, because the brief counts a shot as 1 part. Each shot flies from its enemy's chest height to the dais in front of the centre pad, then respawns on arrival. Phases are spread so all 100 are in the air at every moment. |
| Hands | 2 | 26 joint spheres and 25 bone capsules each | The existing `UHandPresentationComponent`, driven through `UHandInputSubsystem` by `Game/Clips/both-palms.normal.jsonl` (hands L and R, 154 frames, 1.056 s), restarted whenever it ends. |
| Niagara systems | 0 of the plan's 6 | - | The module has no Niagara dependency, and `MageArenaVR.Build.cs` is off limits. No effect exists yet. |

**Placement:** the plan's arc is +/-70 degrees, but the floor is an ellipse (16 m x 10 m half-axes) and the centre pad
sits at x = -12 m. Each ring is clipped to the widest bearing that keeps the enemy 1 m inside the floor edge, where the
wall stands: +/-66.0 degrees at 8 m and +/-36.5 degrees at 15 m (`MAGEVR_BUDGET_PLAN` in every log). The far markers in
`arena-layout.json` sit at +/-35 degrees for the same reason. `MageArena.Budget.StressPopulation` checks the counts
(20 / 100 / 2), the radii, the arc, and that every enemy and every lane end lies on the floor.

**Population, every run** (log lines `MAGEVR_BUDGET_POP`):

| Run | Steady state (start of window) | End of window | Fewest live projectiles in any sampled frame |
|---|---|---|---|
| run1 | enemies=20 projectiles=100 hands=2 | enemies=20 projectiles=100 hands=2 | 100 |
| run2 | enemies=20 projectiles=100 hands=2 | enemies=20 projectiles=100 hands=2 | 100 |
| run3 | enemies=20 projectiles=100 hands=2 | enemies=20 projectiles=100 hands=2 | 100 |

The hand presentation held 102 instances throughout (52 joints and 50 bones). The lanes had relaunched 8,552 times by
the end of each window.

**Timeline per launch:** the driver first waits for a stable frame, as A02 does (30 frames under 50 ms with no shader
compiling, and at least 2.5 s). It then spawns the population. **Warm-up** is at least 10 s after the spawn, plus 60
frames in a row with no shader compiling. It took 10.0 s in every run. The 60 s sample window follows, timed on the
wall clock, with one sample per frame.

## 2. Results per run

The frame rate is uncapped, so n is in the tens of thousands. Min, median, p95 and max are over every frame of the
window. Frame time is `FApp::GetDeltaTime`. Game-thread and GPU time come from the CSV (`GameThreadTime`, `GPUTime`).
Peak memory is the process's peak working set (`FPlatformMemory::GetStats().PeakUsedPhysical`) for the **editor binary
running `-game`**, not a packaged build.

### run1 (n = 34,146 frames)

| Measure | min | median | p95 | max |
|---|---|---|---|---|
| Draw calls (`GNumDrawCallsRHI`) | 226 | 310 | 321 | 333 |
| Triangles, RHI `GNumPrimitivesDrawnRHI` (lower bound, all passes) | 63,495 | 170,986 | 195,319 | 214,771 |
| Triangles, LOD0 in-frustum sum (computed estimate) | 88,592 | 124,528 | 126,448 | 127,408 |
| Triangles, GPU-culled BasePass (mesh draw command readback, 6 sampled frames) | 59,776 | 103,352 | 115,312 | 115,312 |
| Triangles, GPU-culled all passes (same 6 frames) | 160,640 | 261,072 | 293,696 | 293,696 |
| Live particles / GPU emitters | 0 / 0, not exercised | | | |
| Frame time, ms | 0.82 | 1.60 | 2.22 | 980.6 |
| Game thread, ms | 0.76 | 1.23 | 1.85 | 31.3 |
| GPU, ms | 0.58 | 0.70 | 0.81 | 1.57 |
| Peak working set | 2,306 MB | | | |

### run2 (n = 35,330 frames)

| Measure | min | median | p95 | max |
|---|---|---|---|---|
| Draw calls | 227 | 310 | 321 | 335 |
| Triangles, RHI (lower bound) | 65,395 | 172,077 | 195,127 | 214,635 |
| Triangles, LOD0 in-frustum (computed estimate) | 88,592 | 124,528 | 126,448 | 127,408 |
| Triangles, GPU-culled BasePass (6 frames) | 59,776 | 104,768 | 114,928 | 114,928 |
| Triangles, GPU-culled all passes (6 frames) | 160,640 | 265,344 | 292,544 | 292,544 |
| Live particles / GPU emitters | 0 / 0, not exercised | | | |
| Frame time, ms | 0.80 | 1.58 | 2.08 | 1,058.1 |
| Game thread, ms | 0.77 | 1.20 | 1.81 | 22.2 |
| GPU, ms | 0.59 | 0.70 | 0.78 | 1.48 |
| Peak working set | 2,357 MB | | | |

### run3 (n = 27,971 frames)

| Measure | min | median | p95 | max |
|---|---|---|---|---|
| Draw calls | 227 | 310 | 321 | 331 |
| Triangles, RHI (lower bound) | 68,307 | 171,381 | 190,297 | 212,185 |
| Triangles, LOD0 in-frustum (computed estimate) | 88,592 | 124,528 | 126,448 | 127,408 |
| Triangles, GPU-culled BasePass (6 frames) | 77,840 | 91,568 | 111,888 | 111,888 |
| Triangles, GPU-culled all passes (6 frames) | 213,072 | 223,456 | 283,520 | 283,520 |
| Live particles / GPU emitters | 0 / 0, not exercised | | | |
| Frame time, ms | 0.81 | 1.69 | 2.74 | 3,933.6 |
| Game thread, ms | 0.78 | 1.11 | 1.95 | 54.5 |
| GPU, ms | 0.59 | 0.72 | 0.97 | 13.0 |
| Peak working set | 2,303 MB | | | |

The frame-time maximum in every run is a single frame at the start of the window: the CSV capture begins on that
frame. Run 3 has fewer frames and a wider tail; other workers share this machine and the cause was not isolated. The driver's own
GPU reading (`RHIGetGPUFrameCycles`) agrees with the CSV: medians of 0.70, 0.70 and 0.72 ms.

**Where the draw calls go** (CSV `DrawCall/*` medians, all three runs alike). These categories are recorded on the
render timeline and swing between 0 and double from frame to frame. Treat them as shares, not exact counts:
BeginOcclusionTests 155, Basepass 118, Prepass 11, SlateUI 2. The rest of the 310 is uncategorised. Two things set
this level: hardware occlusion queries (one draw per tested primitive), and one component with its own MID per part,
as `SessionPresentation` builds them, so dynamic instancing cannot merge the 160 stress parts.

## 3. Verdicts (D-G4 count rows, desktop proxy)

| Row | Target | Measured, worst of the three runs | Verdict |
|---|---|---|---|
| Draw calls | ≤150 | median 310, max 335 | **fail**, in all three runs and in every sampled frame (min 226) |
| Visible triangles | ≤350k | GPU-culled BasePass max 115,312 (18 sampled frames); all passes max 293,696 (same frames) and 312,992 per frame in the diagnostic window; LOD0 estimate max 127,408; RHI lower bound max 214,771 | **pass** on desktop, with the limits in section 4: the complete counter is sampled, not per-frame |
| Live particles | ≤2,000 | 0 | **not exercised**: no effect exists yet (no Niagara in the module) |
| GPU emitters | ≤1 | 0 | **not exercised**: no effect exists yet |

The time rows (frame time 13.9 ms, game thread ≤6 ms) are not D-G4 verdicts. The numbers in section 2 are a desktop
proxy and cannot prove 72 Hz on Quest. The draw-call target is written for multiview on Quest. This desktop view is a
single view at 1920x1080, so the failure is not a device number either. It is the desktop count for this scene with
today's presentation.

## 4. The triangle counter

**Why `GNumPrimitivesDrawnRHI` is incomplete (UE 5.8.3 source, `Engine/Source/Runtime/...`):**

- The RHI counts primitives only for direct draws. `RHI/Public/RHIStats.h:137-143` (`RHI_DRAW_CALL_STATS`) adds
  `Prims * Instances`. `RHI/Public/RHIStats.h:145` (`RHI_DRAW_CALL_INC`) adds only a draw.
- D3D12 direct draws use the first macro: `D3D12RHI/Private/D3D12Commands.cpp:1224` (`RHIDrawPrimitive`) and `:1267`
  (`RHIDrawIndexedPrimitive`). Indirect draws use the second: `:1235` (`RHIDrawPrimitiveIndirect`) and `:1307`
  (`RHIMultiDrawIndexedPrimitiveIndirect`, which `RHIDrawIndexedPrimitiveIndirect` forwards to at `:1325-1328`).
  The argument buffer lives on the GPU, so the CPU cannot know the primitive count.
- The per-frame totals become the globals at `RHI/Private/GPUProfiler.cpp:2101-2102`, and the CSV columns
  `RHI/DrawCalls` and `RHI/PrimitivesDrawn` at `:2107-2108`.
- A mesh draw goes indirect when the instance-culling context says so: `Renderer/Private/MeshPassProcessor.cpp:1334`
  (`bDoOverrideArgs`) and `:1348-1367` (`SubmitDrawEnd`). `Renderer/Private/InstanceCulling/InstanceCullingContext.cpp:1531`
  sets `bUseIndirectDraw` for any command with instance runs or more than one instance, and at `:1771-1776` it passes
  the indirect-args buffer to the draw. The hand joints and bones, and the arena's instanced shell, are multi-instance
  ISMs, so they escape the RHI count.
- **No console variable makes the RHI count complete.** The `NumInstances > 1` branch at `:1531` has no cvar gate. The
  cvars that touch it, `r.InstanceCulling.ForceInstanceCulling` (`:41-45`) and the single-instance mode (`:1507`),
  only send more draws indirect. Turning dynamic instancing off would not change an ISM. So no diagnostic pass with such
  a cvar was run.
- **The RHI figure is not a ceiling either.** It sums every pass: the depth prepass draws the same triangles as the
  base pass, the velocity pass draws the moving parts, and each occlusion test draws a box. So it lands above the
  base-pass visible count (about 171k against 104k at the median). It is a lower bound only for "all triangles
  submitted in all passes".

**The complete counter used here.** Development builds carry the mesh draw command stats
(`Engine/Public/MeshDrawCommandStatsDefines.h:6`, `MESH_DRAW_COMMAND_STATS (!UE_BUILD_SHIPPING)`). They read the
GPU-culled indirect args back and multiply each draw's visible instance count by its primitive count
(`Renderer/Private/MeshDrawCommandStats.cpp:360-401`). The driver calls `r.MeshDrawCommands.DumpStats DG4<run>`
(`:720-731`) at 5, 15, 25, 35, 45 and 55 s into each window. Each call writes one frame per pass and per draw
(`:617-718`). `capture-budget.ps1` sums the frame by pass. This counts what the GPU drew after frustum culling. It does
not reflect occlusion culling, which happens afterwards. The plan's "visible triangles" is best matched by
**BasePass** (each opaque triangle once). DepthPass repeats it exactly, so "all passes" is close to twice that plus
the velocity pass.

**Diagnostic pass: the per-frame counter (`diag1`).** The same stats export one total per frame to the CSV when the
category is on (`MeshDrawCommandStats.cpp:470-471`, `:593-612`, column `MeshDrawCommandStats/Untracked`, all passes).
The command was `capture-budget.ps1 -Runs 1 -Prefix diag -ExtraArgs "-csvCategories=MeshDrawCommandStats"`, a CSV
flag on the command line only, with no rendering cvar. **It crashed the render thread**, as an earlier trial had:

- The engine ensure `DrawData.VisibleInstanceCount <= DrawData.TotalInstanceCount` failed at
  `MeshDrawCommandStats.cpp:395` on the first collected frame.
- About 14 s into the window (58 s in the trial), the render thread hit an index-out-of-bounds assertion
  (`Array.h:1339`, "index 0 into an array of size 0") in a Renderer to D3D12RHI call. The release engine ships no
  symbols, so the frame names are unknown.
- `MAGEVR_BUDGET_DONE` never logged, so the pass is **failed and incomplete**. In the 7,236 frames recorded before the
  crash: all-pass GPU-culled triangles had median 276,800, p95 298,208 and max 312,992. Draw calls had median 310 and
  max 330. RHI primitives had median 171,553 and max 207,453 (`docs/research/budget-dg4/diag1.summary.json`).
- With the per-frame category off, collection runs only on the 6 dump frames. Four launches in that mode (one trial
  and the three runs) all finished cleanly.

**What this proves and what it does not.** The base-pass figure, the per-frame RHI figure and the LOD0 estimate are
all far below 350k: the largest is 214,771, from the RHI. That holds even for all passes per frame (312,992). The
complete counter, though, is sampled on 18 frames over three runs, plus 7,236 per-frame values from a pass that
crashed. It is not a full 60 s per-frame series. The triangle row therefore passes for this desktop scene. The device
number still comes in V1 (`apps/vr/tasks/BACKLOG.md:13`). The A02 cross-check stays circular: the LOD0 sum is still
its own floor, and this doc does not use it as a verdict.

## 5. Not done here, and why

- **No Niagara.** The plan's scene has 6 Niagara systems. The module has no Niagara dependency, and its `Build.cs` is
  out of bounds for this task. The particle rows stay not exercised until an effect exists. A zero is never a pass.
- **pof perf-capture is not hooked up.** The plan names `pof/src/lib/profiling/perf-capture.ts` as the noise-floor gate
  (P6 desktop half). Feeding these CSVs into it is a later step.
- **No APK.** The plan also packages the scene into the APK for V1. Nothing was packaged, staged or cooked.
- **`GreyboxCapture.cpp` is unchanged.** The driver keeps its own copy of the LOD0 sampler, so A02's
  `capture-greybox.ps1` and `budget.json` are untouched.

## 6. Reproduce

1. `powershell -NoProfile -File apps/vr/tools/build.ps1` (editor target, Win64 Development).
2. `powershell -NoProfile -File apps/vr/tools/capture-budget.ps1 -Runs 3 -Prefix run` writes `runs/DG4/run1..3/` and
   prints a summary. It exits 0 only when every launch logged `MAGEVR_BUDGET_DONE` and full population lines.
3. Optional diagnostic: append `-Prefix diag -Runs 1 -ExtraArgs "-csvCategories=MeshDrawCommandStats"`. Expect the
   render-thread crash described in section 4.

The automation suite after this change (`Automation RunTests MageArena`, `-nullrhi`): 176 tests, 173 pass. The 3
failures are the known ones (`MageArenaDesign.Duel.FireSeated`, `Session.FullSeated`, `Session.Wave1Seated`).
`MageArena.Budget.StressPopulation` and `MageArena.Input.CaptureFlags` pass.

## 7. Levers for the draw-call row

**Label: diagnostic. Shipping settings are unchanged, and the section 3 verdicts stay as measured.** Every run here is
the same desktop proxy as sections 1-3: the same scene, warm-up rule, 60 s window, population checks and six `DumpStats`
frames. Only the command-line switches named per row differ. No `Config/*.ini` and no `SessionPresentation` code changed.

Section 2 names two levers: hardware occlusion queries (about 155 of the 310 draws), and one MID per part (which stops
dynamic instancing). Each lever was measured alone, then the two together.

### 7.1 Provenance

- **Build:** `369a020` (`feat(budget): shared-material and occlusion-off diagnostic variants`), built with
  `powershell -NoProfile -File apps/vr/tools/build.ps1` (exit 0, 330 s, no warnings). All six launches ran at that commit.
- **Date:** 2026-10-08, launches started 00:23-00:43 local time. The machine, engine, RHI and resolution are the same as
  in section 0. Before each launch, `capture-budget.ps1` waited until no `UnrealEditor`, `UnrealEditor-Cmd`,
  `UnrealBuildTool` or `ShaderCompileWorker` was running (`Wait-UnrealLane`: a 60 s poll, for up to 30 minutes). The
  first launch waited for another worker's editor.
- **Commands.** Each exited 0. Each launch logged `MAGEVR_BUDGET_DONE` and two full `MAGEVR_BUDGET_POP` lines:

  | Variant | Command (`powershell -NoProfile -File apps/vr/tools/capture-budget.ps1 ...`) | Runs |
  |---|---|---|
  | control, no lever | `-Runs 1 -Prefix ctrl` | ctrl1 |
  | A: occlusion queries off | `-Runs 1 -Prefix occl -ExtraArgs "-dpcvars=r.AllowOcclusionQueries=0"` | occl1 |
  | B: shared materials | `-Runs 1 -Prefix shared -ExtraArgs "-MageArenaBudgetSharedMaterials"` | shared1 |
  | A and B | `-Runs 3 -Prefix both -ExtraArgs "-dpcvars=r.AllowOcclusionQueries=0 -MageArenaBudgetSharedMaterials"` | both1-3 |

- **Population, every run:** `enemies=20 projectiles=100 hands=2` at steady state and at the end of the window. 102 hand
  instances. No sampled frame had fewer than 100 live projectiles. Warm-up took 10.0 s.
- **Committed summaries:** `docs/research/budget-dg4/{ctrl1,occl1,shared1,both1,both2,both3}.summary.json`. The raw
  CSVs, logs and dumps stay in the gitignored `runs/DG4/`.

**Lever A: how it is applied, and the proof that it took effect.** `r.AllowOcclusionQueries` is an
`FAutoConsoleVariableRef` on `GOcclusionCullEnabled` (`Renderer/Private/SceneVisibility.cpp:395-402`), which
`DoOcclusionQueries` reads (`:404-407`). In non-shipping builds, `-dpcvars=` is applied when the device profile is
applied (`Engine/Private/DeviceProfiles/DeviceProfileManager.cpp:737-773`, inside `#if !UE_BUILD_SHIPPING` at `:719`),
at `ECVF_SetByDeviceProfile` priority. The project config does not set the cvar, so nothing outranks the command line.
Each of the four lever-A logs has `Setting CommandLine Device Profile CVar: [[r.AllowOcclusionQueries:0]]` and no
priority warning. The CSV category `DrawCall/BeginOcclusionTests` is **0 in every frame** (median 0, max 0) in occl1 and
both1-3. It is 155 in ctrl1 and shared1.

**Lever B: how it is applied, and the proof.** With `-MageArenaBudgetSharedMaterials`, the driver's own `MakePart` asks
`BudgetSharedTint` for each colour. `BudgetSharedTint` keeps one `Greybox::Tint` of `Greybox::UnlitOpaqueMaterial()` per
`FLinearColor` and reuses it. Counts, meshes, colours, motion and the HP-bar scale are as before. At steady state the
driver reads the material back from all 160 parts. Every lever-B run logged
`MAGEVR_BUDGET_MATERIALS mids=8 colours=8 parts=160`: 8 distinct materials, one per distinct colour (4 enemy kinds,
HP back, HP fill, water, steel). Without the switch, ctrl1 and occl1 log no such line. The automation test
`MageArena.Budget.SharedMaterials` checks the same rule without a renderer.

### 7.2 Results per variant

The tables give min / median / p95 / max over every frame of the 60 s window. Draw calls are `GNumDrawCallsRHI`.
GPU-culled triangles come from the six `DumpStats` frames per run. Frame and GPU times come from the driver
(`FApp::GetDeltaTime`, `RHIGetGPUFrameCycles`). The peak working set is that of the editor binary running `-game`.

| Run | n | Draw calls | RHI primitives | GPU-culled BasePass tris | GPU-culled all-pass tris | Frame ms | GPU ms | Peak WS |
|---|---|---|---|---|---|---|---|---|
| ctrl1 (no lever) | 31,919 | 225 / 310 / 321 / 331 | 72,143 / 171,307 / 190,343 / 213,931 | 91,072 / 112,144 / 114,480 / 114,480 | 225,376 / 284,192 / 291,200 / 291,200 | 0.85 / 1.67 / 2.49 / 1,623 | 0.60 / 0.73 / 1.07 / 1.63 | 2,277 MB |
| occl1 (A) | 27,072 | 165 / 183 / 186 / 189 | 201,771 / 207,595 / 213,163 / 222,171 | 90,512 / 121,808 / 123,792 / 123,792 | 251,424 / 313,728 / 318,656 / 318,656 | 0.82 / 1.69 / 3.51 / 3,011 | 0.82 / 1.07 / 5.10 / 15.3 | 4,177 MB |
| shared1 (B) | 16,064 | 176 / 214 / 217 / 221 | 64,005 / 169,944 / 190,493 / 214,827 | 63,824 / 106,424 / 114,448 / 114,448 | 170,832 / 267,056 / 291,008 / 291,008 | 0.86 / 2.29 / 9.56 / 1,605 | 0.60 / 1.03 / 1.61 / 10.6 | 2,305 MB |
| both1 (A+B) | 26,860 | 39 / 58 / 59 / 59 | 196,377 / 202,233 / 207,179 / 212,939 | 88,592 / 122,352 / 123,792 / 123,792 | 245,664 / 314,336 / 318,656 / 318,656 | 0.80 / 1.62 / 3.64 / 1,265 | 0.79 / 1.02 / 4.94 / 10.3 | 4,288 MB |
| both2 (A+B) | 44,252 | 40 / 59 / 60 / 60 | 193,673 / 201,579 / 206,955 / 213,451 | 89,552 / 116,464 / 123,792 / 123,792 | 248,544 / 302,560 / 318,656 / 318,656 | 0.76 / 1.23 / 1.85 / 978 | 0.52 / 0.61 / 0.92 / 1.56 | 2,498 MB |
| both3 (A+B) | 39,450 | 40 / 59 / 62 / 62 | 201,769 / 207,595 / 213,163 / 219,211 | 88,592 / 119,376 / 124,752 / 124,752 | 245,664 / 307,904 / 321,536 / 321,536 | 0.79 / 1.45 / 2.01 / 1,009 | 0.53 / 0.64 / 0.95 / 1.68 | 2,661 MB |

As in section 2, the frame-time maximum is the single frame on which the CSV capture starts. Live particles and GPU
emitters stay 0, not exercised.

**`DrawCall/*` CSV medians.** These are shares: as section 2 warns, they swing from frame to frame. "Other" is the
total median minus the four categories.

| Run | Total | BeginOcclusionTests | Basepass | Prepass | SlateUI | Other |
|---|---|---|---|---|---|---|
| D-G4 run1-3 | 310 | 155 | 118 | 11 | 2 | 24 |
| ctrl1 | 310 | 155 | 118 | 11 | 2 | 24 |
| occl1 (A) | 183 | **0** | 145 | 11 | 2 | 25 |
| shared1 (B) | 214 | 155 | **21** | 11 | 2 | 25 |
| both1 / both2 / both3 | 58 / 59 / 59 | **0** | 21 | 10 / 11 / 11 | 2 | 25 |

ctrl1 repeats D-G4 on today's binary and today's machine: median 310, max 331, the same category split.

**Change against D-G4** (run1-3: median 310, max 335):

- **A alone:** median 310 → 183 (-127), max 335 → 189. The 155 query draws are gone, but Basepass rises from 118 to
  145, because primitives that occlusion used to cull are now drawn. Triangles rise with it: the BasePass median goes
  from 112k to 122k, the all-pass max from 293,696 to 318,656, and the RHI median from 171k to 208k. All stay under 350k.
- **B alone:** median 310 → 214 (-96), max 335 → 221. Basepass drops from 118 to 21: dynamic instancing merges the 160
  stress parts into a few draws per mesh and colour. The occlusion tests stay at 155, because each primitive is still
  tested on its own. Triangles do not change within the sampling, since the same geometry is drawn.
- **A and B together:** median 58-59, p95 59-62 and max 62 across three runs. Every sampled frame is under 150.
  Triangles are as for A alone: all-pass max 321,536, BasePass max 124,752.

**Read frame time and memory with care.** occl1 and both1 have a peak working set of 4.2-4.3 GB and a GPU p95 near
5 ms. both2 and both3 ran with the same switches and have 2.5-2.7 GB and a GPU p95 under 1 ms. Their GPU medians (0.61
and 0.64 ms) are below the control's 0.73 ms. So the switches alone do not explain the outliers. Other workers were
using the machine, and the cause was not isolated. As in section 3, the time numbers are a desktop proxy.

### 7.3 Does the Quest renderer issue occlusion-query draws? Yes, under the same cvar

The project ships on the mobile forward renderer (`Config/Android/AndroidEngine.ini`: `r.Mobile.ShadingPath=0`,
`r.Mobile.MultiView=1`), on Vulkan. From the UE 5.8.3 source (paths under `Engine/Source/Runtime/Renderer/Private/`
unless noted):

- **The gate is shared.** `FMobileSceneRenderer` calls the same `DoOcclusionQueries()` (`SceneVisibility.cpp:404-412`,
  which reads `r.AllowOcclusionQueries`) before it allocates and renders queries. It does so at
  `MobileShadingRenderer.cpp:995-1000` (full depth prepass) and at `:2123-2129`, `:2614-2619` and `:2830-2835` (the
  base-pass paths without a full prepass). The Adreno-mode decision at `:686` reads it too. The renderer has no
  mobile-only cvar that turns the queries on or off.
- **The default is hardware queries.** `r.HZBOcclusion` defaults to 0, documented as "Hardware occlusion queries"
  (`SceneVisibility.cpp:123-131`, applied at `:3227-3230`). The mobile alternative, `r.OcclusionFeedback.Enable`
  (`:134-139`), defaults to 0. The engine's `Meta_Quest_3` device profile sets it to 0 explicitly and sets
  `r.Mobile.AdrenoOcclusionMode=1` (`Engine/Config/BaseDeviceProfiles.ini:1444-1449`). The project and its Android config
  set none of these.
- **The draws come from the same code.** `FMobileSceneRenderer::RenderOcclusion` calls the shared `BeginOcclusionTests`
  (`SceneOcclusion.cpp:1556-1560`), which flushes the shared batchers (`:1378-1395`). Each batch is one
  `DrawIndexedPrimitive` inside a render query (`SceneOcclusion.cpp:479-495`). Primitives that were visible last frame
  go through the individual batcher, one box per draw (batch size 1, `SceneRendering.cpp:999`). Primitives that were
  occluded last frame are grouped, 16 to a draw (`SceneRendering.h:422`, `SceneRendering.cpp:1000`, chosen at
  `SceneVisibility.cpp:2989-3023`).
- **On Quest the queries get their own pass.** With `r.Mobile.AdrenoOcclusionMode=1` on Vulkan, the queries leave the
  main pass and run in a separate depth-read render pass after it (`MobileShadingRenderer.cpp:2225-2230`, and `:2305`
  onward). The batchers take an instance count for instanced stereo (`SceneRendering.cpp:999-1000`). This doc did not
  check whether mobile multiview takes that path, and so whether a query costs one draw for both eyes on the device.

**So lever A is not desktop-only.** The Quest build issues hardware occlusion-query draws, about one per tested
primitive that was visible last frame. `r.AllowOcclusionQueries=0` removes them there, through the same gate. The size
of the saving on the device still has to be measured on the device (V1). Multiview, the separate Adreno pass and the
device's own view all differ from this desktop view.

### 7.4 Verdict per lever (desktop proxy, diagnostic)

| Lever | Median / max draws | Reaches ≤150 on desktop? |
|---|---|---|
| A: occlusion queries off | 183 / 189 | **no** |
| B: one material per colour | 214 / 221 | **no** |
| A and B together | 58-59 / 62 (3 runs) | **yes**, in every frame of all three runs. The max of 62 is under half the limit. |

Neither lever reaches the row alone. The pair clears it with room to spare. At 58-59 what remains is Basepass 21, Other
(uncategorised) about 25, Prepass 11 and SlateUI 2.

### 7.5 The shipping change each lever implies (not made here)

- **A: one config line.** Quest only: `r.AllowOcclusionQueries=0` under `[ConsoleVariables]` in
  `apps/vr/Game/Config/Android/AndroidEngine.ini`, or `+CVars=r.AllowOcclusionQueries=0` in a project `Meta_Quest_3`
  device profile. Every platform: `r.AllowOcclusionQueries=False` under `[/Script/Engine.RendererSettings]` in
  `DefaultEngine.ini` (the "Occlusion Culling" project setting, `Engine/Classes/Engine/RendererSettings.h:380`).
  **Cost:** nothing hidden behind other geometry is culled any more. In this open arena that adds about 10k BasePass
  triangles at the median, and about 28k all-pass triangles at the max (321,536 against the 350k row), plus the vertex
  and pixel work to draw them. An arena with more occluders (walls, pillars, props) would lose more, and the triangle
  row would need re-measuring with A on.
- **B: a per-colour material cache in `SessionPresentation::MakePart`.** `SessionPresentation.cpp:91-98` (as of
  4bd6e1c) would keep the same kind of `FLinearColor` → MID map that `BudgetSharedTint` keeps. **Cost:** recolouring at
  runtime. `SessionPresentation` writes `Color` into a part's own MID at `:1119`, `:1189`, `:1221`, `:1286`, `:1297`
  and `:1321`, and builds a fresh MID at `:1250`. With shared MIDs, each of those writes would recolour every part of that
  colour. Each one must become a swap to the cached MID of the new colour (`SetMaterial(0, ...)`), or move to
  per-primitive custom data on a material that reads it. A colour that changes every frame, such as a fade, would also
  grow the cache without bound unless its steps are quantised. `SessionPresentation.cpp` is dirty in the owner's
  checkout, so this change belongs to whoever owns that file.

### 7.6 Reproduce

1. `powershell -NoProfile -File apps/vr/tools/build.ps1`.
2. The four commands in the 7.1 table. Each prints its summary, and exits 0 only when every launch logged
   `MAGEVR_BUDGET_DONE` and full population lines.

The automation suite at `369a020` (`Automation RunTests MageArena`, `-nullrhi`), run after the corpus was regenerated
with the CLAUDE.md commands: 177 tests, 174 pass. The 3 failures are the known ones (`MageArenaDesign.Duel.FireSeated`,
`Session.FullSeated` and `Session.Wave1Seated`). `MageArena.Budget.SharedMaterials`, `MageArena.Budget.StressPopulation`
and `MageArena.Input.CaptureFlags` pass.

### 7.7 Decision

**Who and when.** The App Master took this decision on 2026-10-08. It is not an owner decision, so it is not in
`docs/DECISIONS.md`. The owner reads it at the Sun 11 Oct go/no-go.

**Constraint.** The desktop core gate needs the session scene inside the section 7 count budgets, among them ≤150 draw
calls (`docs/PROJECT-PLAN.md:47`). The budget table holds the row as "Draw calls (multiview) ≤150"
(`docs/PROJECT-PLAN.md:364`). Gate D-G4 measures it on the desktop proxy over three 60 s runs
(`docs/PROJECT-PLAN.md:218`). Section 3 measured median 310 and max 335, which fails. Neither lever reaches the row
alone (7.4).

**Choice.** The row is met with both levers together.

- **A and B together** gave a median of 58-59 and a max of 62 over three runs (7.2, 7.4). That leaves about 88 draws
  under the limit.
- **A alone** (occlusion queries off) gave a median of 183 and a max of 189. **B alone** (one material per colour)
  gave 214 and 221.
- **Lever A** ships as one line, `r.AllowOcclusionQueries=False` under `[/Script/Engine.RendererSettings]` in
  `apps/vr/Game/Config/DefaultEngine.ini`. It applies to every platform. It does not go in `AndroidEngine.ini` alone.
  The desktop build is the Quest proxy for every count row, and the mobile renderer reads the same gate (7.3). A
  Quest-only line would leave the desktop gate measuring a culling setup the device never runs, and every capture would
  need `-dpcvars`. A separate delivery run makes this change and re-runs D-G4 with shipping settings. This section does
  not.
- **Lever B** belongs to lane A. `SessionPresentation.cpp` is dirty in the owner's checkout, lane A's `v2/slice`
  rewrites it, and card T26 is building a single colour function there. The requirement: parts with the same mesh and
  colour share one material, so dynamic instancing can merge them, and no runtime recolour writes into a shared
  material. Lane A picks one of two ways:
  - (a) One shared material that reads its colour from custom primitive data set per part. It needs no cache, and a
    recolour is one call. Whether such draws still merge on the mobile forward renderer is not verified, and it must
    be checked.
  - (b) A per-colour MID cache keyed by `FLinearColor`, like the driver's `BudgetSharedTint`. Every runtime colour write
    becomes a swap to the cached MID of the new colour, and any per-frame fade is quantised so the cache stays bounded.
- The item for lane A is in `apps/vr/tasks/BACKLOG.md`.
- Until B is on master, the section 3 draw-call verdict stays **fail** as measured. Once B is on master, the App Master
  switches the stress driver's default path to follow it (`Budget/`), so D-G4 measures the shipping presentation.

**Alternatives that lost.**

- **A alone or B alone.** Neither reaches 150 (7.4).
- **A smaller stress population.** It games the measure, because the population is the plan's.
- **HZB occlusion (`r.HZBOcclusion=1`) or occlusion feedback (`r.OcclusionFeedback.Enable=1`).** Not measured here.
  Section 7.3 only notes that both cvars exist and default to 0; it did not check how either behaves on the mobile
  renderer. Either would keep some culling with fewer query draws. This is the fallback if the device shows that A's
  triangle cost bites.
- **A Quest-only config line.** See the reason under lever A above.
- **Asking the owner for scope relief on the row.** Unnecessary, because the pair clears it.

**Costs.**

- **A:** geometry that occlusion used to cull is now drawn. The BasePass median rises by about 10k (112,144 to 121,808).
  The BasePass max is 124,752, and the all-pass max is 321,536 against 350k. An arena with more occluders would lose
  more, so the triangle row is re-measured with A on (7.5).
- **B:** the runtime recolour path changes (7.5).

**What V1 must confirm on the device.**

- Whether a multiview query costs one draw for both eyes (7.3 did not check).
- The real draw count.
- A's triangle cost.
- That no engine device profile for Quest sets `r.AllowOcclusionQueries` at a higher priority than the project
  setting.

**Revert.** Delete the one config line.

## 8. Lever A shipped

**Label: desktop proxy, synthetic clip-driven hands, not Quest truth; cannot prove 72 Hz.** Lever A of 7.7 is now in the
project config, so the runs below use shipping settings: no `-dpcvars`, no other switch. Sections 0-7.7 are unchanged.

### 8.1 Provenance

- **Config commit:** `90d394d` (`feat(config): occlusion queries off on every platform (D-G4 lever A)`). It adds
  `r.AllowOcclusionQueries=False` to `[/Script/Engine.RendererSettings]` in `apps/vr/Game/Config/DefaultEngine.ini` and
  rewrites the first comment line of `Config/Android/AndroidEngine.ini`, which was stale. No cvar changed in that file.
  No C++ changed. The scene, driver and `capture-budget.ps1` are the 7.1 ones. All four launches ran at `90d394d` on a
  binary built from it.
- **Date:** 2026-10-08, launches about 07:53-08:04 local time. Machine, engine (UE 5.8.3), RHI and resolution as in
  section 0.
- **Commands and exit codes:**

  | Step | Command | Exit |
  |---|---|---|
  | Build | `powershell -NoProfile -File apps/vr/tools/build.ps1` (118 s) | 0 |
  | Shipping settings | `powershell -NoProfile -File apps/vr/tools/capture-budget.ps1 -Runs 3 -Prefix ship` | 0 |
  | Occlusion-on control | `powershell -NoProfile -File apps/vr/tools/capture-budget.ps1 -Runs 1 -Prefix occlon -ExtraArgs "-dpcvars=r.AllowOcclusionQueries=1"` | 0 |
  | Corpus | `node apps/vr/tools/clipgen/generate.mjs --corpus 30 --seed 1000`, then `node apps/vr/tools/clipgen/mousepaths.mjs` | 0 |
  | Suite | `UnrealEditor-Cmd ... -ExecCmds="Automation RunTests MageArena; Quit" -nullrhi` | 255 (as in BASELINE) |

  Every launch logged `MAGEVR_BUDGET_DONE` and two `MAGEVR_BUDGET_POP` lines at full count
  (`enemies=20 projectiles=100 hands=2`, 102 hand instances, 8,553 launches at the end). Warm-up took 10.0 s. Wall times
  were 133, 89, 86 and 95 s. Each launch started only when `Wait-UnrealLane` saw no Unreal process.
- **Committed summaries:** `docs/research/budget-dg4/{ship1,ship2,ship3,occlon1}.summary.json`.
- **Suite:** 178 tests, 175 pass. The 3 failures are the known ones (`MageArenaDesign.Duel.FireSeated`,
  `Session.FullSeated`, `Session.Wave1Seated`). `MageArena.Budget.StressPopulation`, `MageArena.Budget.SharedMaterials`
  and `MageArena.Input.CaptureFlags` pass.

### 8.2 Which key, which priority, and does anything outrank it

- **The key.** The "Occlusion Culling" project setting is `URendererSettings::bOcclusionCulling` with
  `ConsoleVariable="r.AllowOcclusionQueries"` (`Engine/Source/Runtime/Engine/Classes/Engine/RendererSettings.h:379-382`).
  The editor writes a setting like this under the cvar's name, not the property's: `LoadConfig` and the save path set
  `Key = CVarName` (`CoreUObject/Private/UObject/Obj.cpp:3029-3033`, `:3769-3773`, `:4107-4111`, all `WITH_EDITOR`). So
  the ini key is `r.AllowOcclusionQueries`, as for the `r.ForwardShading` line already in this section.
- **The priority.** At start-up, `Launch/Private/LaunchEngineLoop.cpp:2817` calls
  `ApplyCVarSettingsFromIni("/Script/Engine.RendererSettings", GEngineIni, ECVF_SetByProjectSetting)`, so every key in
  the section is set as a cvar at `ECVF_SetByProjectSetting` (the same table is at `Core/Private/HAL/ConsoleManager.cpp:593`).
  This path is not editor-only, so it applies in a game or packaged build on Win64 and on Android. Both read
  `DefaultEngine.ini`, and `AndroidEngine.ini` is layered on top without setting this cvar. Device profile,
  scalability, console and command line all rank above project setting.
- **Proof in the logs.** `LogConfig: Set CVar [[r.AllowOcclusionQueries:0]]` appears once in each of ship1-3 (and in
  occlon1) at the project-setting stage, with no other switch on the command line.
- **Override check.** `r.AllowOcclusionQueries` appears in exactly one engine config line,
  `Engine/Config/BaseDeviceProfiles.ini:1569`, in the `LinuxArm64` device profile (`+CVars=r.AllowOcclusionQueries=False`,
  device-profile priority). It sets the same value, and that profile is not Windows, Android, Android_Vulkan,
  Meta_Quest_3 or OculusQuest*. Nothing in the engine's `BaseEngine.ini`, `BaseScalability.ini`, `Windows/` and `Android/`
  configs, the other device profiles, the engine's `Platforms/` and Meta plugin folders or this project's `Config/`
  sets it. **None outranks the project setting.** The device itself is still for V1.
- **Control.** In occlon1 the log has the project line first (`Set CVar [[r.AllowOcclusionQueries:0]]`, line 545) and
  then `LogDeviceProfileManager: Setting CommandLine Device Profile CVar: [[r.AllowOcclusionQueries:1]]` and
  `Set CVar [[r.AllowOcclusionQueries:1]]` (lines 828-829), as 7.1 describes. The command line outranks the project
  setting, so `-dpcvars=...=1` is a valid way to measure with occlusion back on. occlon1 reproduces the old numbers,
  so the config line is the only difference between it and ship1-3.

### 8.3 Results

min / median / p95 / max over every frame of the 60 s window. GPU-culled triangles come from the six `DumpStats` frames
per run. Frame and GPU times are from the CSV (`FrameTime`, `GPUTime`), peak working set from the driver.

| Run | n | Draw calls | RHI primitives | GPU-culled BasePass tris | GPU-culled all-pass tris | Frame ms | GPU ms | Peak WS |
|---|---|---|---|---|---|---|---|---|
| ship1 | 37,700 | 164 / 183 / 185 / 186 | 193,673 / 201,579 / 206,955 / 213,451 | 119,824 / 122,352 / 124,752 / 124,752 | 308,800 / 314,336 / 321,536 / 321,536 | 0.79 / 1.44 / 2.04 / 1,723 | 0.57 / 0.67 / 1.06 / 2.75 | 3,275 MB |
| ship2 | 40,657 | 165 / 183 / 186 / 188 | 201,769 / 207,595 / 213,163 / 219,211 | 90,512 / 122,352 / 124,752 / 124,752 | 251,424 / 314,336 / 321,536 / 321,536 | 0.79 / 1.43 / 1.98 / 1,073 | 0.57 / 0.68 / 1.08 / 1.86 | 2,443 MB |
| ship3 | 39,739 | 164 / 183 / 185 / 186 | 193,675 / 201,579 / 206,955 / 213,451 | 89,552 / 120,848 / 124,752 / 124,752 | 248,544 / 310,848 / 321,536 / 321,536 | 0.79 / 1.47 / 2.00 / 997 | 0.56 / 0.68 / 1.06 / 1.87 | 2,553 MB |
| occlon1 (`-dpcvars=...=1`) | 35,326 | 225 / 310 / 320 / 332 | 71,441 / 173,031 / 191,655 / 213,675 | 68,480 / 96,808 / 114,928 / 114,928 | 178,672 / 242,192 / 292,544 / 292,544 | 0.76 / 1.58 / 2.22 / 910 | 0.60 / 0.73 / 1.10 / 1.65 | 2,288 MB |

The frame-time maximum is the single frame on which the CSV capture starts, as before. Particles and GPU emitters stay
0, not exercised. ship1's peak working set (3,275 MB) is higher than ship2-3 (2.4-2.6 GB); 7.2 saw the same spread with
other workers on the machine, and the cause was not isolated.

**`DrawCall/*` CSV medians** (shares, as in section 2). `DrawCall/BeginOcclusionTests` is **0 in every frame** (min,
median and max 0, n = 37,700 / 40,657 / 39,739) in ship1-3, with no switch. "Other" is the total minus the four
categories; it includes Translucency (median 16) and RenderVelocities (2).

| Run | Total | BeginOcclusionTests | Basepass | Prepass | SlateUI | Other |
|---|---|---|---|---|---|---|
| D-G4 run1-3 | 310 | 155 | 118 | 11 | 2 | 24 |
| occl1 (7.2, `-dpcvars=...=0`) | 183 | 0 | 145 | 11 | 2 | 25 |
| ship1 / ship2 / ship3 | 183 / 183 / 183 | **0** | 145 | 11 | 2 | 25 |
| occlon1 | 310 | 155 | 118 | 10 | 2 | 25 |

### 8.4 Change

- **Against occl1 (7.2).** The shipped config reproduces the command-line variant: draw calls 183 against 183, max
  186-188 against 189, Basepass 145 against 145, BasePass triangle max 124,752 against 123,792, all-pass max 321,536
  against 318,656, RHI median 202-208k against 208k. The config line does what `-dpcvars=...=0` did.
- **Against D-G4 run1-3 (median 310, max 335).** Draw calls fall by 127 at the median and by 147 at the max. The 155
  query draws are gone, and Basepass rises from 118 to 145. The BasePass triangle median rises from 91.6-104.8k to
  120.8-122.4k, the all-pass max from 293,696 to 321,536, and the RHI median from about 171k to 202-208k.
- **Against occlon1.** occlon1 (310 / 155 / 118) matches ctrl1 and D-G4, so nothing else differs between the shipping
  runs and the old ones.

### 8.5 Updated verdicts (desktop proxy)

| Row | Target | Measured with lever A shipped | Verdict |
|---|---|---|---|
| Draw calls | ≤150 | median 183, max 188 (ship1-3); min 164 | **fail**, in every sampled frame of all three runs |
| Visible triangles | ≤350k | GPU-culled BasePass max 124,752; all passes max 321,536 (18 sampled frames); RHI lower bound max 219,211 | **pass** on desktop, with the limits in section 4 |
| Live particles | ≤2,000 | 0 | **not exercised** |
| GPU emitters | ≤1 | 0 | **not exercised** |

- **Draw calls: what remains, by category.** 33 draws at the median (38 at the max) over the limit. The median 183 is
  Basepass 145, Other 25 (Translucency 16, RenderVelocities 2 and uncategorised), Prepass 11 and SlateUI 2. Basepass
  is the only category that can close the gap: lever B takes it from 118 to 21 (7.2, both1-3), which puts the total at
  58-62. The row stays **fail** until lever B is on master and the stress driver follows the shipping presentation
  (7.7).
- **Triangles, re-judged with A on.** BasePass max 124,752 is 36% of 350k. The all-pass figure sums the depth prepass,
  base pass, velocity and translucency passes, so it counts most triangles twice or more; its max is 321,536, which is
  92% of 350k, 28k under the limit. It is a sampled maximum (6 frames per run), not a per-frame series (section 4). On
  the plan's reading (each opaque triangle once, BasePass) the row passes with room. On the all-pass reading it passes
  by 8%. An arena with more occluders than this open greybox would add to it (7.5). V1 measures the device number.

### 8.6 Reproduce

1. `powershell -NoProfile -File apps/vr/tools/build.ps1`.
2. Shipping settings: `powershell -NoProfile -File apps/vr/tools/capture-budget.ps1 -Runs 3 -Prefix ship`. No
   `-ExtraArgs`. Expect `DrawCall/BeginOcclusionTests` 0 in every frame and a draw-call median of 183.
3. With occlusion back on: `powershell -NoProfile -File apps/vr/tools/capture-budget.ps1 -Runs 1 -Prefix occlon -ExtraArgs "-dpcvars=r.AllowOcclusionQueries=1"`.
   Expect a median of about 310 and `Setting CommandLine Device Profile CVar: [[r.AllowOcclusionQueries:1]]` in the log.
   The switch works in non-shipping builds only.
4. Revert: delete the `r.AllowOcclusionQueries=False` line and its comment from `DefaultEngine.ini`.

## 9. Decision 7.7 amended

**Label: source reading, not a device measurement.** Two merged runs changed 7.7: `docs/research/MOBILE-INSTANCING-2026-10.md`
(3c099c9, below "MI") and section 8 (d98ae66, 743021a). Sections 0-8.6 are unchanged, and 7.7 stays as written, as the
history. Where 7.7 and this section differ, this section wins.

**Who and when.** The App Master took this decision on 2026-10-08. It is not an owner decision, so it is not in
`docs/DECISIONS.md`. The owner reads it, with 7.7, at the Sun 11 Oct go/no-go.

**Constraint.** The row is still ≤150 draw calls (`docs/PROJECT-PLAN.md:47`, 7.7). With lever A shipped, the desktop
proxy has median 183 and max 188 (8.5). Lever B is still the lever that closes the gap (8.5). 7.7 gave a fallback that
does not run and asked questions that now have answers.

**Choice.**

1. **The lever A fallback changes.**
   - `r.HZBOcclusion=1` is struck. It does not work on the 5.8.3 mobile forward renderer (MI 2.1).
   - Occlusion feedback (`r.OcclusionFeedback.Enable=1`) is the only fallback. It **replaces** lever A instead of
     joining it, because it needs `r.AllowOcclusionQueries=1` (MI 2.2).
   - The engine's `Meta_Quest_3` profile sets it to 0 (`Config/BaseDeviceProfiles.ini:1448`, MI 2.2), so using it
     needs a project device-profile override.
   - It is tried only if V1 shows that A's triangle cost bites. The figures to weigh: all-pass max 321,536, which is 92%
     of 350k, and BasePass max 124,752 (8.5).
2. **Lever B.**
   - Both ways merge on Quest as configured, at most 32 parts per merged draw (MI 1.2 to 1.4, 1.7).
   - The device Basepass with B is expected near the desktop 21, plus about 2 (MI 5; desktop 21 from 7.2).
   - Merging therefore no longer decides lane A's choice between (a) and (b) in 7.7. The recolour path does (MI 5).
     Use (a) when colours change often (fades, hit flashes). Use (b) when they change rarely.
   - The greybox materials stay unlit. Lit materials split the buckets again (MI 1.2, 5).
   - (a) carries UE-160597 as an unread device risk (MI 1.6).
3. **Config lines that must never be set,** because each undoes the merge (MI 1.7):
   - `r.Mobile.SupportGPUScene=0`
   - `r.MeshDrawCommands.DynamicInstancing=0`
   - `r.Mobile.MeshSortingMethod=1`

   Moving to Meta's engine fork, or enabling one of the four features named in MI 1.6, reopens the question.
4. **Answered questions, off the V1 list.**
   - 7.3's open question is answered from source: under multiview, one occlusion-query draw covers both eyes (MI 3).
   - 7.7's "no device profile outranks the project setting" is answered by 8.2: `r.AllowOcclusionQueries` appears in
     one engine config line, `Engine/Config/BaseDeviceProfiles.ini:1569`, in the `LinuxArm64` profile, which sets the
     same value. **None outranks the project setting.** The device itself is still for V1.
5. **A02's draw-call figure was measured with occlusion queries on.**
   - `docs/research/BASELINE-2026-10-07.md` section 3, Draw calls row: desktop proxy median 30 (`d09d1cb`). It stays as
     the before, and baselines are not re-captured.
   - Section 8 is the after-A desktop figure for the count rows (8.3, 8.5).
6. **The particle rows.**
   - The plan's stress scene has 6 Niagara systems (`docs/PROJECT-PLAN.md:218`). The driver has none (8.5: "not
     exercised"; section 5: no Niagara dependency), and a zero is never a pass.
   - The App Master adds the 6 systems to the stress driver (`Budget/`) once the module can take a Niagara dependency in
     `MageArenaVR.Build.cs`.
   - That must land in time for the 31 Oct gate, and in time for V1 to package the scene on day one (R6,
     `docs/PROJECT-PLAN.md:549`).
   - Until then both particle rows stay **not exercised**.

**Alternatives that lost.**

- **`r.HZBOcclusion=1` as the fallback.** It does not run on this renderer (MI 2.1).
- **Occlusion feedback on top of A.** It cannot run with queries off (MI 2.2).
- **Choosing (a) or (b) by whether it merges.** Both merge (MI 1.7).

**Costs.**

- **Occlusion feedback, if tried:** a project device-profile override, a cost not seen in source (pixel-shader UAV
  writes per box pixel, results that lag by the buffered frames), and the engine's own Quest 3 profile turns it off
  (MI 2.2).
- **(a):** one new material that reads custom primitive data, and the UE-160597 device risk (MI 5, 1.6).
- **(b):** each colour change rebuilds the part's proxy, and fades must be quantised (MI 1.4, 5).

**What V1 must confirm on the device.**

- The real draw count with B, and whether the 32-part split matches (MI 6).
- `GPUScene: Enabled` in the device log (MI 5, 6).
- Under (a), whether a `SetCustomPrimitiveData*` recolour shows on Android Vulkan (MI 6).
- A's triangle cost (7.7, 8.5).
- If occlusion feedback is tried: its draws, GPU time and culling, against A's triangle cost (MI 6).

**Caveat from MI.** These upstream quotes came through a search agent and were not re-fetched: the forum hotfix tag, the
Meta download pages and the UE-160597 snippet (MI 1.6, 4). The UE-160597 page returned HTTP 403.

**Revert.** This section is a record, so reverting means deleting it.
