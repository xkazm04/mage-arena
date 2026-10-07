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
