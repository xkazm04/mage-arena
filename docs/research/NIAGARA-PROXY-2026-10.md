# Where the D-G4 stress scene's Niagara systems come from, and how they are counted

**Label: source reading, not a measurement.** No Unreal process ran for this file: no editor, no commandlet, no build
and no automation. Every verdict below comes from engine source, engine config, plugin content files and one upstream
page. The particle rows stay **not exercised** until a builder lands the outline in section 5 and runs it.

The question comes from `docs/research/BUDGET-DG4-2026-10.md` section 9, point 6 (`ef53807`). The plan's stress scene
holds 6 Niagara systems (`docs/PROJECT-PLAN.md:218`). It is judged on live particles ≤2,000 and at most 1 GPU emitter,
over three 60 s runs, and it is packaged for V1's first day (R6, `docs/PROJECT-PLAN.md:549`). The driver has none:
`Budget/BudgetStressDriver.cpp:699` writes `niagaraSystems` 0, and `:731-733` write `liveParticles` 0, `gpuEmitters` 0
and "not exercised".

## 0. Provenance

- **Engine:** `C:\Program Files\Epic Games\UE_5.8\Engine`. `Build/Build.version`: 5.8.3, Changelist 58210709,
  CompatibleChangelist 55116800, `++UE5+Release-5.8`.
- **Niagara plugin:** `Engine/Plugins/FX/Niagara`, `Niagara.uplugin` version 1.0, `"EnabledByDefault": true`,
  `"CanContainContent": true`. Its runtime modules are `NiagaraCore`, `Niagara`, `NiagaraShader` and
  `NiagaraVertexFactories`. `NiagaraEditor` is an Editor module.
- **Date:** 2026-10-08.
- **Base:** `ef53807` on `master`. Lane A's `v2/slice` (`e78871c`, merge base `76dec98`) was read with `git diff` and
  `git show` only.
- **Reconcile:** `git grep -n -i niagara -- apps/vr/Game docs/research` (exit 0) found the driver's zero
  (`BudgetStressDriver.cpp:699`), BASELINE 390/414/505, BUDGET-DG4 47/139/208/633-635, STACK-OPPORTUNITIES 27/591 and
  the `niagaraSystems: 0` lines in `docs/research/budget-dg4/*.summary.json`. None of them answers Q1 or Q2.
- **Paths:** `N/` is `Engine/Plugins/FX/Niagara/Source/Niagara/`. `NE/` is `.../Source/NiagaraEditor/`. `C/` is
  `Engine/Plugins/FX/Niagara/Content/`. Other engine paths are relative to `Engine/`.
- **Who read what.** Two search agents read the engine for Q2 and Q3. I re-read every line marked **(re-read)** myself.
  Lines marked **(agent)** come from an agent and were not re-read. Everything in Q1 and Q4 I read myself.
- **Content reading.** `.uasset` files are binary. The facts I give about template assets come from a scan of each
  file's name table (`node`, a script outside the repo). A name in the table shows what the asset references. It does
  not prove a property's value. Each such fact is marked **(name table)**. A builder confirms them with the commandlet's
  dump step (5.2, step 3).

## 1. Q1: where can 6 Niagara systems come from?

The project has no authored Content (`git ls-files apps/vr/Game/Content` prints nothing, BASELINE 1.6). The game map
is the engine's empty `Entry` map (`apps/vr/Game/Config/DefaultEngine.ini:24-25`). The systems must run in the
desktop proxy (`-game -RenderOffScreen`, a real D3D12 RHI) and be carried by a packaged APK.

### 1.1 Route (a): use the engine's shipped systems and templates as they are

What the plugin ships, under `C/DefaultAssets/Templates/`:

| Folder | Assets | Kind |
|---|---|---|
| `Systems/` | AttributeReaderTrails, DirectionalBurst, DirectionalBurstLightweight, FountainLightweight, MinimalLightweight, RadialBurst, SimpleExplosion | `UNiagaraSystem` templates |
| `Emitters/` | BlowingParticles, ConfettiBurst, DirectionalBurst, DynamicBeam, Fountain, HangingParticulates, LocationBasedRibbon, Minimal, OmnidirectionalBurst, RecycleParticlesInView, SimpleSpriteBurst, SingleLoopingParticle, StaticBeam, UpwardMeshBurst | standalone emitter templates |
| `BehaviorExamples/`, `CascadeConversion/` | 23 examples, `CompletelyEmpty` | teaching assets |

**The system templates (name table):**

| Asset path | Sim target | Renderers | Spawn | Cookable? |
|---|---|---|---|---|
| `/Niagara/DefaultAssets/Templates/Systems/SimpleExplosion` | CPU (only `CPUSim` present) | 4 sprite, 2 mesh refs; `M_Gnomon_Alpha`, `S_Arrow` | 3 bursts (Omnidirectional, SimpleSprite, UpwardMesh) | yes, as a system |
| `.../Systems/RadialBurst` | CPU | sprite, ribbon | bursts plus a ribbon trail leader | yes |
| `.../Systems/DirectionalBurst` | CPU | sprite, ribbon | burst plus location events | yes |
| `.../Systems/AttributeReaderTrails` | mixed: `GPUComputeSim` appears 13 times | sprite | bursts, reads another emitter | yes |
| `.../Systems/FountainLightweight` | **GPU**, see below | sprite | stateless spawn info | yes |
| `.../Systems/DirectionalBurstLightweight` | **GPU** | sprite | stateless | yes |
| `.../Systems/MinimalLightweight` | **GPU** | sprite | stateless | yes |

- **Maximum or typical live particles: not readable from source.** They live in each emitter's spawn module inputs
  (rapid-iteration parameters) and in `AllocationMode` / `PreAllocationCount`
  (`N/Classes/NiagaraEmitter.h:382`, `:389`). Every template carries both names, with no value I can read without
  loading the asset. Most templates are one-shot bursts: they spawn once, then complete. A burst that completes after
  1-2 s gives a 60 s window of zeros, which is useless as a budget instrument.
- **Materials.** Every sprite and ribbon template uses `/Niagara/DefaultAssets/DefaultSpriteMaterial` or
  `DefaultRIbbonMaterial` (sic). Both name tables hold `BLEND_Additive`, `MSM_Unlit`, `bUsedWithNiagaraSprites` and
  `bUsedWithNiagaraRibbons` **(name table)**. That matches the plan's "additive" particle row.
- **"Lightweight" means GPU.** The three Lightweight systems are stateless emitters (`UNiagaraStatelessEmitter`,
  `N/Internal/Stateless/NiagaraStatelessEmitter.h:31-32`). Their sim target is computed, not chosen.
  `UNiagaraStatelessEmitter::ComputeSimTarget` returns `CPUSim` only when an enabled renderer cannot run on the GPU.
  Otherwise it returns `GPUComputeSim` (`N/Private/Stateless/NiagaraStatelessEmitter.cpp:587-600`, re-read). A sprite
  renderer can, so **each Lightweight sprite emitter is a GPU emitter.** Only one of them fits the budget row.
- **Standalone emitter templates are editor-only.** `UNiagaraEmitter::IsEditorOnly` returns true for an emitter whose
  outer is not a system that holds it (`N/Private/NiagaraEmitter.cpp:899-922`, re-read). The 14 `Emitters/` assets
  cannot be spawned or cooked by themselves. They are only starting points for a system.
- **Cooking.** Route (a) references the templates by soft path from C++. A string in code is serialised in no
  package, so the cooker never finds it. The default cook starts from the maps (`Entry`) and `DirectoriesToAlwaysCook`
  (`Editor/UnrealEd/Private/CookOnTheFlyServer.cpp:8670-8705`). The setting takes plugin paths too, "/PluginName/Folder"
  (`Developer/DeveloperToolSettings/Classes/Settings/ProjectPackagingSettings.h:563-568`). So route (a) needs
  `+DirectoriesToAlwaysCook=(Path="/Niagara/DefaultAssets/Templates/Systems")` in `DefaultGame.ini`, which cooks all
  7 systems (about 3.2 MB of uncooked assets, mostly editor graph). A primary asset rule would also work, but needs an
  `AssetManagerSettings` block the project does not have. `DirectoriesToAlwaysCook` is the rule lane A already uses
  for `/Game/Audio` (`git diff master...v2/slice -- apps/vr/Game/Config/DefaultGame.ini`).
- **Tuning.** A template cannot be edited in place: it is engine content. No template is a looping, steady CPU source
  with a known rate.
- **R8** (`docs/PROJECT-PLAN.md:551`, "no Fab or stock packs"). Engine plugin content ships with the engine. It is not
  a Fab or marketplace pack. It is still stock content that nobody in this project made, and route (a) would put it,
  unchanged, into the APK.

**Route (a) verdict:** it runs and it cooks with one `DirectoriesToAlwaysCook` line. It is still the wrong instrument:
the templates are bursts with unknown, untunable counts. Three of the seven are GPU, so the row cannot be sized
honestly from them.

### 1.2 Route (b): a commandlet makes `NS_Budget_*` assets from the templates, committed under `Content/Budget/`

- **The editor does exactly this when a user picks a template.** `UNiagaraSystemFactoryNew::FactoryCreateNew` copies
  the template with `StaticDuplicateObject`, clears `TemplateAssetDescription` and `Category`, and calls
  `RequestCompile(false)` (`NE/Private/NiagaraSystemFactoryNew.cpp:109-176`, re-read). For emitter templates it creates
  an empty system, runs `InitializeSystem` and adds each emitter (`:154-168`). The factory class is not exported
  (`NE/Public/NiagaraSystemFactoryNew.h:16`). Only its static `InitializeSystem` is `NIAGARAEDITOR_API` (`:35`).
- **What a commandlet can call from the `Niagara` runtime module alone, in an editor build:**
  - `StaticDuplicateObject` (CoreUObject) on a system template.
  - `UNiagaraSystem::AddEmitterHandle(UNiagaraEmitter&, FName, FGuid)`, `NIAGARA_API`
    (`N/Classes/NiagaraSystem.h:332`). Its body is inside `#if WITH_EDITORONLY_DATA` (`N/Private/NiagaraSystem.cpp:3017-3031`).
  - `UNiagaraEmitter::GetLatestEmitterData()`, `NIAGARA_API` (`N/Classes/NiagaraEmitter.h:729-730`). It reaches the
    plain properties: `SimTarget` (`:336`), `bLocalSpace` (`:308`), `CalculateBoundsMode` and `FixedBounds`
    (`:350-357`), and `AllocationMode` / `PreAllocationCount` (`:382-389`). A GPU emitter needs fixed bounds. Otherwise
    the system's asset tags count it as `GPUSimsMissingFixedBounds` (`N/Private/NiagaraSystem.cpp:1871-1872`).
  - Each script's `RapidIterationParameters`, a `FNiagaraParameterStore` declared outside the editor-only blocks
    (`N/Classes/NiagaraScript.h:883`). Spawn rate, burst count, lifetime, size and colour live there.
  - `UNiagaraSystem::RequestCompile` and `WaitForCompilationComplete(bool bIncludingGPUShaders, ...)`
    (`N/Classes/NiagaraSystem.h:446`, `:452`). They are editor-only, which is fine in a commandlet.
  - `UPackage::SavePackage`, as lane A's `ImportSfxCommandlet` does
    (`git show v2/slice:apps/vr/Game/Source/MageArenaVR/Tools/ImportSfxCommandlet.cpp`, lines 112-123).
  
  **A new system also needs its system-script graph, and that lives in `NiagaraEditor`.** The factory builds it with
  `InitializeSystem`, which creates the `UNiagaraScriptSource` and `UNiagaraGraph` and adds the required system-update
  module through `FNiagaraStackGraphUtilities` (`NE/Private/NiagaraSystemFactoryNew.cpp:178-211`, re-read). It then
  adds each emitter with `FNiagaraEditorUtilities::AddEmitterToSystem`, which is `NIAGARAEDITOR_API`
  (`NE/Public/NiagaraEditorUtilities.h:279`). A bare `AddEmitterHandle` on a system with no graph would skip that
  wiring. So the commandlet needs `NiagaraEditor` as an **editor-only** dependency (4.1), with its body under
  `#if WITH_EDITOR` as `ImportSfxCommandlet` does. The compiler also lives in `NiagaraEditor`. The plugin loads that
  module in any editor process, a commandlet included (`Niagara.uplugin`, module `NiagaraEditor`, `Type: Editor`).
- **Cooking.** The driver loads the systems by soft path, so `DirectoriesToAlwaysCook` is again required:
  `+DirectoriesToAlwaysCook=(Path="/Game/Budget")`. The cooker then follows the systems' hard references to
  `/Niagara/DefaultAssets/DefaultSpriteMaterial` and `DefaultRIbbonMaterial`, which cook with them. GPU shaders for
  the one GPU emitter are compiled at cook time for every targeted format (agent: `N/Private/NiagaraScript.cpp:3379-3408`).
- **What is committed.** About 6 `.uasset` files under `apps/vr/Game/Content/Budget/`, through Git LFS
  (`.gitattributes`). This is the first committed content in the project. The commandlet is idempotent: it rebuilds
  from the templates and the table, so the assets can be regenerated and reviewed.
- **R8.** The assets are made by this project's own tool, in this repo, after 24 Sep. What they copy from the engine is
  the default emitter graph and the default additive material, which every new Niagara system in any project starts
  from. No Fab or stock pack is involved. The owner may still want this recorded; it is in `questions`.
- **pof.** Plan item P9 ("Niagara parametrize and budget", `docs/PROJECT-PLAN.md:396`) is this capability in pof:
  "duplicate a template system, set user parameters such as colour, size, rate and lifetime, report emitter count, sim
  target and max particles". CLAUDE.md says such a capability is built in pof first. The commandlet is the stress
  scene's instrument, not the art pipeline. The owner or App Master decides whether it waits for P9 or is P9's first,
  game-side half (in `questions`).

**Route (b) verdict:** it works in a cooked build, it can be tuned to an honest size, and it respects R8. It costs one
commandlet, one LFS folder and one `DirectoriesToAlwaysCook` line.

### 1.3 Route (c): build a system at runtime from C++ alone

**There is none in a cooked build.**

- Adding an emitter to a system is editor-only: `AddEmitterHandle` is compiled only `#if WITH_EDITORONLY_DATA`
  (`N/Private/NiagaraSystem.cpp:3017-3031`, re-read).
- Compiling is editor-only: `UNiagaraSystem::RequestCompile` is declared inside the `WITH_EDITORONLY_DATA` block
  `N/Classes/NiagaraSystem.h:434-534`, and `UNiagaraScript::RequestCompile` is inside `:1207-1278`
  (`N/Classes/NiagaraScript.h`). Without a compile, a stateful emitter has no VM bytecode and no GPU shader.
- The stateless emitter needs no script compile, but its runtime data is built from editor data.
  `BuildCompiledDataSet` runs only `#if WITH_EDITORONLY_DATA`
  (`N/Private/Stateless/NiagaraStatelessEmitter.cpp:602-604`), and its add-renderer and spawn-info setters are
  `#if WITH_EDITOR` (`N/Internal/Stateless/NiagaraStatelessEmitter.h:141-170`). It would also be a GPU emitter (1.1).
- Neither cook rule nor R8 applies: there is nothing to cook.

### 1.4 Q1 verdict

**Take route (b).** A commandlet duplicates engine templates into 6 `NS_Budget_*` systems under `/Game/Budget/`, sets
the table below, compiles, saves, and the assets are committed through LFS. `DefaultGame.ini` gains
`+DirectoriesToAlwaysCook=(Path="/Game/Budget")`.

**The 6 proxy systems.** Each stands for one effect the game will have. The four A06 effects are the water bolt in
flight, the ward absorbing a hit, the perfect-absorb flash and the unblockable telegraph
(`apps/vr/tasks/A06-vfx-concepts.md`, Goal). Two more come from the plan: the sigil trail (gesture pipeline,
`docs/PROJECT-PLAN.md:349`) and the blink vignette, the one full-screen translucency the plan allows (`:366`).

| # | Asset | From template | Stands for | Sim | Renderer, material | Spawn | Life | Peak live | Placed |
|---|---|---|---|---|---|---|---|---|---|
| 1 | `NS_Budget_BoltTrail` | `Emitters/Fountain` | water bolt trail | CPU | sprite, default additive, 4-8 cm | 600/s | 0.5 s | 300 | attached to projectile 0, so it moves |
| 2 | `NS_Budget_SigilTrail` | `Emitters/LocationBasedRibbon` | fingertip sigil stroke | CPU | ribbon, default additive, 1 cm wide | 90/s | 1.0 s | 90 | attached to the right index tip joint |
| 3 | `NS_Budget_WardAbsorb` | `Emitters/OmnidirectionalBurst` | ward absorbs a hit | CPU | sprite, element colour | burst 120 every 0.5 s | 0.4 s | 120 | in front of the dais, at palm height |
| 4 | `NS_Budget_PerfectFlash` | `Emitters/SimpleSpriteBurst` | perfect-absorb flash | CPU | sprite, white, 1 flash sprite plus sparks | burst 60 every 1.0 s | 0.3 s | 60 | as #3 |
| 5 | `NS_Budget_Telegraph` | `Emitters/Fountain`, zero velocity, ring spawn | unblockable telegraph (black core, red rim) | CPU | sprite, red rim colour | 300/s | 1.0 s | 300 | on a far-ring enemy |
| 6 | `NS_Budget_Motes` | `Emitters/HangingParticulates` with `SimTarget = GPUComputeSim`, fixed bounds 4 m | the one GPU emitter: ambient arena motes / blink vignette budget | **GPU** | sprite, 1-2 cm | 300/s | 2.0 s | 600 | over the centre pad |

- **Total peak: 1,470 live, 74% of the 2,000 row; 1 GPU emitter, the row's maximum.** Every CPU system stays at or
  under 300 maximum particles, the per-system ceiling P9's budget report enforces (`docs/PROJECT-PLAN.md:396`).
  `AllocationMode = FixedCount` with `PreAllocationCount` equal to each peak, so no system can exceed its row.
- **Why this is honest.** It is not trivially under: three-quarters of the row, with both particle kinds and the only
  GPU slot used. It is not beyond the game: each figure is one effect's plausible share, and the six together are what
  one busy moment of a duel shows (a bolt in flight, a stroke drawn, a hit absorbed, a perfect, a telegraph).
- **What it does not model.** The stress scene has 100 projectiles but one trail. One system per effect gathers that
  effect's whole particle share at one spot, so screen coverage differs from the game. Sprites are small (1-8 cm) to
  respect "small screen coverage". These are authored figures, not tuned ones. They live in one table in code (5.2,
  step 2), so the session presentation can replace them later.
- **Colour language.** Colours follow the greybox rule in CLAUDE.md: element colour for #3 (absorb), and
  #5 red on a black core. They are set through each emitter's colour input.
- **Templates for #1-#6 are CPU in the name table** (only `CPUSim` present; **name table**), except #6, which the
  commandlet switches to GPU. `BlowingParticles`, `ConfettiBurst` and `RecycleParticlesInView` carry a
  `GPUComputeSim` name and are not used.

## 2. Q2: how does the capture count live particles and GPU emitters per frame?

### 2.1 What not to read

- **Stat counters.** `STAT_NiagaraNumParticles` ("# CPU Particles", `N/Private/NiagaraStats.h:9`, re-read) is added only
  at the end of a stateful CPU emitter tick (`N/Private/NiagaraEmitterInstanceImpl.cpp:2051`, re-read). GPU emitters
  never add to it. `STAT_NiagaraGPUParticles` is declared (`N/Private/NiagaraGpuComputeDispatch.cpp:47`, re-read), and
  a search of the whole plugin source finds no other line, so it always reads 0. The header is in `Private/`, so a
  game module cannot name these stats anyway. They also need `stat Niagara` or a stats capture to collect (agent:
  `Runtime/Core/Public/Stats/Stats.h:270-279`).
- **CSV.** No CSV stat carries a particle count. The `Particles` category is off by default
  (`Runtime/Engine/Private/Particles/ParticlePerfStats.cpp:7`, re-read), is switched by `fx.DetailedCSVStats` (`:1224-1225`,
  re-read), and its count is system instances, not particles (agent: `:1297`, `:1306`). `NiagaraGpuCompute` writes
  dispatch counts only (agent: `NiagaraGpuComputeDispatch.cpp:60`, `:1883`, `:2260`).
- **Console commands.** `fx.Niagara.Debug.Hud` shows totals on screen (agent: `N/Private/NiagaraDebugHud.cpp:205`,
  `:1836-1853`). It is a picture, not a number the driver can read.

### 2.2 The API to use (public, from C++)

This is the debug HUD's own method (`N/Private/NiagaraDebugHud.cpp:1257-1299`, re-read):

1. Iterate `TObjectIterator<UNiagaraComponent>`, keep those in the driver's world, skip `IsComplete()`.
2. `Component->GetSystemInstanceController()` (inline, `N/Public/NiagaraComponent.h:408-409`, re-read), then
   `GetSystemInstance_Unsafe()` (inline, `N/Public/NiagaraSystemInstanceController.h:83`, re-read). The name says
   unsafe because a concurrent tick may be running. Call
   `FNiagaraSystemInstance::WaitForConcurrentTickAndFinalize()` first (`NIAGARA_API`,
   `N/Public/NiagaraSystemInstance.h:243`, re-read). The driver ticks as an `FTickableGameObject`, after the world's
   tick groups.
3. `SystemInstance->GetEmitters()` (`N/Public/NiagaraSystemInstance.h:277-278`, re-read). For each emitter skip
   `IsDisabled()`, and read:
   - `GetNumParticles()` (`NIAGARA_API virtual`, `N/Classes/NiagaraEmitterInstance.h:72`, re-read);
   - `GetSimTarget()` (inline, `:54`) to split CPU from GPU;
   - `GetExecutionState()` (`:65`). An emitter counts as a live GPU emitter when its sim target is GPU and it is
     `Active`.
4. Cross-check with `FNiagaraSystemInstance::ActiveGPUEmitterCount`, a public field
   (`N/Public/NiagaraSystemInstance.h:681`, re-read). It is reset and recounted in every concurrent tick
   (`N/Private/NiagaraSystemInstance.cpp:2669`, `:2739`, re-read).

No per-world counter exists (agent: `N/Public/NiagaraWorldManager.h`). `UNiagaraFunctionLibrary` has no count helper
(agent: `N/Public/NiagaraFunctionLibrary.h:194-206`).

### 2.3 Do GPU counts lag? Yes, and they over-report

- `FNiagaraEmitterInstance::GetNumParticles` for a GPU emitter returns `GPUExecContext->GetCurrentNumInstances()` once a
  fence has passed. Before that it returns `TotalSpawnedParticles` as a "guess"
  (`N/Private/NiagaraEmitterInstance.cpp:34-55`, re-read).
- The render thread sets that count to previous + spawned, capped at the buffer maximum, before the dispatch
  (`N/Private/NiagaraGpuComputeDispatch.cpp:877-896`, re-read). Deaths are subtracted only when the instance-count
  readback lands (`:738-775`; first lines re-read).
- Only one readback is in flight at a time (`bEnqueueCountReadback = !HasPendingGPUReadback()`, `:800`, and the
  check at `:969-973`, re-read). It is collected when `FRHIGPUBufferReadback::IsReady()` is true
  (`N/Private/NiagaraGPUInstanceCountManager.cpp:624-632`, re-read).
- **So the lag is not a fixed number of frames.** It is at least one render-thread frame, plus however long the GPU
  readback takes, typically a few frames. Between readbacks the count is an upper bound: spawns are added at once,
  deaths later.
- **The stateless (Lightweight) emitter is different.** Its count is analytic, computed from the seed and age
  (`N/Private/Stateless/NiagaraStatelessEmitterInstance.cpp:280-283`, re-read). That is another reason #6 is a stateful
  GPU emitter: the proxy exercises the readback path the device will run.
- **For the budget this bias is safe:** a reported GPU count can only exceed the true count. The JSON says so
  (`gpuCountNote`).

### 2.4 Real RHI, `-RenderOffScreen` and `-nullrhi`

- `capture-budget.ps1` launches `-game -RenderOffScreen -ResX=1920 -ResY=1080 -nohmd ... -csvprofile`
  (`apps/vr/tools/capture-budget.ps1`, header; BUDGET-DG4 section 0). The RHI is D3D12 (`driver.rhi`).
  `-RenderOffScreen` keeps a real RHI. Nothing in Niagara reads it (agent), so CPU and GPU sims run as on screen.
- **`-nullrhi` breaks every count, CPU included.** `FApp::CanEverRender()` is false with `-nullrhi`
  (`Runtime/Core/Public/Misc/App.h:400-407`, re-read). `UNiagaraComponent::ActivateInternal` then returns without
  activating (`N/Private/NiagaraComponent.cpp:1323-1332`, re-read). Every count reads 0. Commandlets are also excluded
  by `CanEverRender` (same line), so the creation commandlet cannot measure its own systems.
- The automation suite runs headless under `-nullrhi` (`ArenaGreyboxActor.cpp:54`, `HandPresentationComponent.cpp:76`
  skip their work on it). **A suite test can never read a live count.** The counter itself must be tested on synthetic
  input (5.3), and the capture must refuse a zero (5.3, guard).

### 2.5 JSON fields the driver should write

Keep `liveParticles` and `gpuEmitters` as numbers, so `capture-budget.ps1` and the committed summaries keep their
shape. Their meaning becomes the **window maximum**, the figure the row is judged on. Beside them:

| Field | Type | Meaning |
|---|---|---|
| `liveParticles` | number | max over the window of the per-frame total |
| `gpuEmitters` | number | max over the window of active GPU emitters |
| `liveParticlesSpread` | `{n,min,median,p95,max}` | as the other spreads (`Spread`, `BudgetStressDriver.cpp:83-98`) |
| `liveParticlesCpu`, `liveParticlesGpu` | spreads | split by sim target |
| `gpuEmittersCrossCheck` | number | max of the summed `ActiveGPUEmitterCount` |
| `niagaraSystemsLive` | `{min,max}` | live components per frame; min must be 6 |
| `zeroParticleFrames` | number | frames in the window with a total of 0 |
| `particlesExercised` | bool | true only when all 6 systems were live on every frame and `liveParticles` > 0 |
| `particlesNote` | string | "exercised" or the reason not |
| `gpuCountNote` | string | "GPU counts are a delayed upper bound (instance-count readback)" |
| `niagara.allowGpuParticles` | bool | `FNiagaraUtilities::AllowGPUParticles()` at spawn time |
| `niagara.systems` | array | per system: asset path, emitters, sim target each, authored peak |

`scene.niagaraSystems` becomes the plan's count (6) instead of the literal 0 at `:699`. `samples.json` gains
`liveParticles`, `liveParticlesGpu` and `gpuEmitters` series.

### 2.6 Q2 verdict

Count with the public instance API: `UNiagaraComponent` → system instance → `GetEmitters()` → `GetNumParticles()` and
`GetSimTarget()`, after `WaitForConcurrentTickAndFinalize`, every frame of the window. No stat or CSV column gives the
number. GPU counts lag by a variable number of frames and over-report, which is the safe side. The counts need a real
RHI: the capture's flags have one, and `-nullrhi` gives 0 for everything.

## 3. Q3: does a Niagara GPU emitter run on Quest 3 under this project's config?

The path: UE 5.8.3, Android, Vulkan, mobile forward, feature level ES3_1, shader platform `VULKAN_ES3_1_ANDROID`,
mobile multiview, device profile `Meta_Quest_3` (MOBILE-INSTANCING section 0).

### 3.1 The switches

- **Gate.** `FNiagaraUtilities::AllowGPUParticles()` = `GNiagaraAllowGPUParticles && GNiagaraAllowComputeShaders &&
  GRHISupportsDrawIndirect` (`N/Private/NiagaraCommon.cpp:1047-1055`, re-read). The cvars are
  `fx.NiagaraAllowComputeShaders` = 1 and `fx.NiagaraAllowGPUParticles` = 1, the second marked `ECVF_Scalability` so
  device profiles can set it (`:34-48`, re-read). Draw-indirect support defaults to true
  (`Runtime/RHI/Public/RHIGlobals.h:107`, re-read). No shader-platform or feature-level test exists in this gate
  (agent: `NiagaraCommon.h:1596-1606`, `:1627-1629`).
- **Device profiles** (`Config/BaseDeviceProfiles.ini`, re-read):
  - `[Meta_Quest_3]` (`:1444-1450`) inherits `Android_OpenXR` and sets no `fx.` cvar.
  - Only `[Oculus_Quest]`, the first Quest, turns GPU particles off: "disable running Niagara compute shaders on the
    GPU. This can cause hard to trace crashes.", `fx.NiagaraAllowGPUParticles=0`, `FX.AllowGPUSorting=0`
    (`:1477-1482`).
  - The profile is matched by HMD name (`:895`). The project declares Quest 3 in the manifest
    (`apps/vr/Game/Config/DefaultEngine.ini:67-68`, `+SupportedDevices=Quest3`, `Quest3S`), so the device does not
    fall back to an older profile (see 3.2).
- **The project's config** sets no `fx.` cvar. The Meta XR plugin's config in the main checkout has none either
  (`grep` of `apps/vr/Game/Plugins/*/Config`, read only).
- **Quality level.** Android limits Niagara quality to 0-1 (`Config/Android/AndroidEngine.ini:122-123`, re-read).
  `Android_Mid` sets `sg.EffectsQuality=1` (`Config/BaseDeviceProfiles.ini:1060`, re-read), and `[EffectsQuality@1]`
  maps to `fx.Niagara.QualityLevel=0` (`Config/Android/AndroidScalability.ini:186-197`, re-read). **Quest 3 runs Niagara
  at quality Low.** An emitter whose platform set leaves out Low is disabled there (agent:
  `N/Private/NiagaraEmitter.cpp:2167-2170`, `NiagaraPlatformSet.cpp:352-377`). The commandlet must leave all quality
  levels on.
- **No CPU fallback.** A GPU emitter that is not allowed is refused (`N/Private/NiagaraComponentSettings.cpp:344-347`,
  re-read). The instance goes to `Disabled` (`N/Private/NiagaraEmitterInstanceImpl.cpp:250-254`, re-read). It does not
  switch to CPU. On a device that refuses it, the motes vanish silently. In the capture, `IsDisabled()` emitters are
  skipped (2.2), so the GPU count drops to 0 and `niagara.allowGpuParticles` says why.
- **Other caveats (agent, not re-read):**
  - Empty RHI, adapter and GPU deny lists by default (`NiagaraComponentSettings.cpp:160-290`).
  - A float16 UAV check bans GPU emitters that use compressed attributes (`fx.Niagara.GpuEmitterCheckFloat16Support`).
    #6 keeps attribute compression off.
  - Low-latency GPU translucency is off on the mobile path. `IsGpuTranslucentThisFrame` returns false for any shading
    path other than deferred (`N/Private/NiagaraRendererProperties.cpp:1465-1471`, re-read), so GPU sprites draw one
    frame late.
  - Distance-field and ray-traced collision are unavailable (`r.DistanceFields=0` on Android). #6 uses no collision.
- **Multiview.** Niagara has no mobile-multiview code of its own (agent). Its stereo path keys off
  `bIsInstancedStereoEnabled` (agent: `NiagaraRendererSprites.cpp:892`, `:998`). In `SceneView.cpp:971-992` (re-read),
  instanced stereo and mobile multiview are exclusive, and `bIsMobileMultiViewEnabled` sets `MultiViewCount = 2`.
  So Niagara takes its non-instanced-stereo path under multiview. No source blocks it. Whether both eyes draw
  correctly is a device question.

### 3.2 Upstream

- Epic Developer Community Forums, "Niagara GPU particles Sim on Meta Quest 2/3?",
  https://forums.unrealengine.com/t/niagara-gpu-particles-sim-on-meta-quest-2-3/1646543, fetched 2026-10-08. Fetched
  once through a summarising fetch tool, not read raw:
  - 2024-01-27, the asker: a GPU emitter worked over PCVR but not in a packaged Quest 3 app; CPU worked.
  - 2024-04-30: "For some reason Meta Quest 3 uses Oculus Quest device profile". That profile is the one that sets
    `fx.NiagaraAllowGPUParticles=0` (3.1).
  - 2024-06-06, an Epic staff account (VictorLerp): "We added a device profile for Quest 3 in 5.4". Without Quest 3 in
    the manifest's `com.oculus.supportedDevices`, "your Quest 3 will run in Quest 2 compatibility mode, and load the
    Quest 2 device profile."
  - That matches the source: the risk is the profile, not the GPU. This project already names Quest 3 in the manifest
    (3.1). STACK-OPPORTUNITIES F2 and the `DefaultEngine.ini:54-59` comment record why the tag list is guarded.
- A web search summary also named `r.Mobile.SupportGPUParticles` and a `GRHISupportsAtomicUInt64` check (bugnet.io). I
  did not re-fetch them, and neither appears in the gate above. Do not rely on them.

### 3.3 P-1's "CPU-sim Niagara only" fallback: what it costs

P-1 (`docs/PROJECT-PLAN.md:218`) is "CPU-sim Niagara only, flipbook sprites instead of GPU emitters".

- **In the proxy:** one cell. Row #6's sim target becomes CPU. The commandlet is re-run, its asset re-committed, and
  the build re-cooked. With 600 particles it also needs `FixedCount` 600 on the CPU, which costs game-thread time the
  desktop CSV already records (`GameThreadTime`) and the device must confirm.
- **In the game:** the same flip per GPU emitter, and P9's report already flags every GPU emitter
  (`docs/PROJECT-PLAN.md:396`).
- **Not by cvar.** A project device-profile override `fx.NiagaraAllowGPUParticles=0` does not fall back: it disables
  the emitter (3.1). Use it only to check what is lost.

### 3.4 Q3 verdict

**Yes, from source.** On `Meta_Quest_3` with Vulkan, as this project configures it, nothing turns a GPU emitter off:
both cvars are 1, draw indirect is supported, the deny lists are empty, and the profile sets no `fx.` line. Three
conditions remain:

- the emitter's platform set includes quality Low;
- it avoids compressed attributes and mobile-unsupported data interfaces;
- the device loads the Quest 3 profile, not the first Quest's.

V1 confirms all three on the device: the "GPU emitters are enabled/disabled for RHI" log line (agent:
`NiagaraComponentSettings.cpp`), `gpuEmitters` = 1 in the device capture, and the motes visible in both eyes.

## 4. Q4: the module change

### 4.1 Build.cs

`v2/slice` changes the `PrivateDependencyModuleNames` block (master lines 24-28: `"ImageWrapper"` gains a comma, then
an `AudioMixer` comment and entry) and appends an `if (Target.bBuildEditor)` block after it, before the closing braces
(`git diff master...v2/slice -- .../MageArenaVR.Build.cs`). Any edit inside those lines, or a second
`bBuildEditor` block at the same place, conflicts.

**Put both Niagara lines above the `PublicDependencyModuleNames` block,** right after master line 10
(`PrivateIncludePaths.Add(ModuleDirectory);`). The public block (master lines 11-23) stays unchanged between the two
edits, so a three-way merge takes both:

```csharp
		PrivateIncludePaths.Add(ModuleDirectory);
		// D-G4: the stress scene spawns and counts Niagara systems (Budget/).
		PrivateDependencyModuleNames.Add("Niagara");
		if (Target.bBuildEditor)
		{
			// D-G4: Tools/CreateBudgetFxCommandlet builds the NS_Budget_ systems from engine templates.
			PrivateDependencyModuleNames.Add("NiagaraEditor");
		}
		PublicDependencyModuleNames.AddRange(new[]
```

- **Private, not public.** Only `.cpp` files include Niagara headers. The driver's header forward-declares
  `UNiagaraComponent` and `UNiagaraSystem`.
- **A second `bBuildEditor` block, on purpose.** After both branches land, the file has two such blocks: this one and
  lane A's at the end. Folding them into one is a follow-up commit on master after the merge, not part of either
  branch. Adding `"NiagaraEditor"` inside lane A's block before it merges would conflict.
- **Nothing else.** `Niagara` brings `NiagaraCore`, `NiagaraShader`, `NiagaraVertexFactories`, `RenderCore` and
  `VectorVM` as its own public dependencies (`Engine/Plugins/FX/Niagara/Source/Niagara/Niagara.Build.cs:32-46`). The
  commandlet saves with `UPackage::SavePackage` (CoreUObject), so it needs neither `UnrealEd` nor `AssetTools`.
- **Cooked builds.** `NiagaraEditor` is never linked into the game or the APK: `Target.bBuildEditor` is false there,
  and every use sits under `#if WITH_EDITOR`.

### 4.2 .uproject

`Niagara` is `EnabledByDefault` (`Niagara.uplugin`), and `OculusInteraction` already depends on it
(STACK-OPPORTUNITIES:27). Still name it, so a module dependency never rests on a default. `v2/slice` does not touch the
`.uproject`. Add it after `EnhancedInput`:

```json
		{ "Name": "EnhancedInput", "Enabled": true },
		{ "Name": "Niagara", "Enabled": true },
```

### 4.3 DefaultGame.ini

`v2/slice` appends two lines at the end of the file (after master line 14). Put this one **directly under the section
header**, after master line 11, so lines 12-14 separate the two edits:

```ini
[/Script/UnrealEd.ProjectPackagingSettings]
; D-G4: the stress scene loads its Niagara systems by path (no hard reference), so the cook must be told to keep them.
+DirectoriesToAlwaysCook=(Path="/Game/Budget")
+DirectoriesToAlwaysStageAsNonUFS=(Path="../Clips")
```

### 4.4 Every file the implementation touches

| File | Change | Collides with v2/slice? |
|---|---|---|
| `apps/vr/Game/Source/MageArenaVR/MageArenaVR.Build.cs` | 6 lines (4.1) | no, if placed as in 4.1 |
| `apps/vr/Game/MageArenaVR.uproject` | 1 line (4.2) | no |
| `apps/vr/Game/Config/DefaultGame.ini` | 2 lines (4.3) | no, if placed as in 4.3 |
| `apps/vr/Game/Source/MageArenaVR/Budget/BudgetFx.h`, `.cpp` (new) | the 6-row table, and the pure counter aggregation | no |
| `apps/vr/Game/Source/MageArenaVR/Budget/BudgetStressDriver.h`, `.cpp` | spawn the 6, count each frame, write 2.5's fields, refuse a zero | no (v2/slice does not touch `Budget/`) |
| `apps/vr/Game/Source/MageArenaVR/Tools/CreateBudgetFxCommandlet.h`, `.cpp` (new) | route (b) | no: a new file beside lane A's `ImportSfxCommandlet` |
| `apps/vr/tools/create-budget-fx.ps1` (new) | runs the commandlet, as lane A's `import-sfx.ps1` does | no |
| `apps/vr/Game/Content/Budget/NS_Budget_*.uasset` (new, LFS) | the 6 systems | no |
| `apps/vr/Game/Source/MageArenaVR/Tests/BudgetStressTests.cpp` | new tests (4.5) | no |
| `apps/vr/tools/capture-budget.ps1` | print the particle rows; the gate also needs `particlesExercised` | no |
| `docs/research/BUDGET-DG4-2026-10.md` | a new section with the three runs; sections 3 and 9.6 stay as history | not touched by v2/slice |

### 4.5 Tests

All run under `-nullrhi` in the `MageArena` suite, so none of them reads a live count (2.4).

- **`MageArena.Budget.FxPlan`:** the table has 6 rows, exactly 1 GPU, every CPU row ≤300 peak. The summed peak is
  ≤2,000 and ≥1,000, so the proxy is neither over budget nor trivially under. Every asset path is under `/Game/Budget/`.
- **`MageArena.Budget.ParticleCount`:** the aggregation function takes a list of synthetic emitter records (sim target,
  execution state, particle count) and returns CPU and GPU totals and the active GPU emitter count. Cases:
  - disabled emitters are skipped;
  - a GPU emitter counts once only while `Active`;
  - an empty list gives 0.
- **`MageArena.Budget.ZeroIsNotAPass`:** the verdict function returns "not exercised", never "pass", when
  `niagaraSystemsLive.min` < 6, when `liveParticles` = 0, or when any frame of the window has a total of 0. It returns
  "fail" over 2,000 or over 1 GPU emitter, and "pass" otherwise.
- **The live guard is in the capture.** When the verdict is "not exercised", the driver logs
  `MAGEVR_BUDGET_FAIL particles not exercised` and `capture-budget.ps1` marks the run failed. So a zero can only reach a
  committed summary as a failure.
- **Assets present:** the plan test also loads each `NS_Budget_*` with `LoadObject` (loading works without an RHI). It
  checks emitter count and sim targets against the table. This catches a commandlet run that was not committed.

## 5. Implementation outline (for a Sonnet builder, once `Build.cs` is clean)

### 5.1 Preconditions

- `MageArenaVR.Build.cs` has no uncommitted line in the checkout the build runs from (the operator's `// touch` line is
  gone or committed).
- One Unreal process at a time (operator rule). The commandlet, the build, the suite and the captures run one after
  another, never side by side.
- The build machine has the MetaXR plugins (a worktree does not, so a worktree cannot run step 4 onward).

### 5.2 Steps

1. **Module.** Apply 4.1, 4.2 and 4.3 exactly. Build: `powershell -NoProfile -File apps/vr/tools/build.ps1`.
2. **The table.** Add `Budget/BudgetFx.h/.cpp`: a `struct FBudgetFxRow { Name, TemplatePath, bGpu, PeakLive, SpawnPerS
   or Burst/PeriodS, LifeS, SizeCm, Colour, Attach }`, and `BudgetStress::FxRows()` returning the 6 rows of 1.4. Keep it
   pure data, as `BudgetStressPlan` is. Add the pure counter
   `FBudgetParticleCount CountEmitters(TConstArrayView<FBudgetEmitterSample>)` and the verdict function.
3. **Commandlet, dump first.** Add `Tools/CreateBudgetFxCommandlet` (body under `#if WITH_EDITOR`, as
   `ImportSfxCommandlet`). For each row:
   - a new `UNiagaraSystem` in a new package `/Game/Budget/<Name>`;
   - `UNiagaraSystemFactoryNew::InitializeSystem(System, true)`;
   - `FNiagaraEditorUtilities::AddEmitterToSystem(*System, *Template, Template->GetExposedVersion().VersionGuid)`, with
     the emitter template loaded from `/Niagara/DefaultAssets/Templates/Emitters/<T>`. This is the factory's own path
     (`NE/Private/NiagaraSystemFactoryNew.cpp:154-168`). Do not start from a Lightweight system template: it is
     stateless and GPU (1.1);
   - then log every `RapidIterationParameters` name and type of every script of the emitter (`GetScripts`,
     `N/Classes/NiagaraEmitter.h:423`).

   Run it once with `-dump` and read the names. Spawn rate, burst count, lifetime, sprite size and colour inputs are
   named there. The names could not be read from source.
4. **Commandlet, set and save.** Set the rapid-iteration values from the table.
   - Emitter data: `SimTarget` (GPU for #6 only), `AllocationMode = FixedCount`, `PreAllocationCount = PeakLive`.
   - #6 only: `CalculateBoundsMode = Fixed`, `FixedBounds` a 4 m box.
   - Leave every quality level in the platform set on, and attribute compression off.
   - Call `RequestCompile(true)` and `WaitForCompilationComplete(true)`, then save as `ImportSfxCommandlet` does.
   - Log `MAGEVR_BUDGET_FX <name> emitters=<n> sim=<cpu|gpu> peak=<n>` for each.
   - Run `UnrealEditor-Cmd <uproject> -run=CreateBudgetFx`, through `apps/vr/tools/create-budget-fx.ps1`.
   - Commit the 6 assets through LFS (`git lfs ls-files` must list them).
5. **Driver.**
   - In `SpawnPopulation`, `LoadObject<UNiagaraSystem>` each row's path.
   - Spawn with `UNiagaraFunctionLibrary::SpawnSystemAttached` (bolt trail on `ShotParts[0]`, sigil trail on the
     right index joint) or `SpawnSystemAtLocation` (the rest), with `bAutoDestroy = false`.
   - Keep the components in a `UPROPERTY` array. Fail with `MAGEVR_BUDGET_FAIL` if any load or spawn returns null.
   - Bursts (#3, #4) loop through the emitter's own loop settings from the table, not by re-activating.
   - In `SampleFrame`, gather `FBudgetEmitterSample`s by 2.2 and call `CountEmitters`.
   - Add the per-frame figures to new arrays. Extend `CountPopulation`/`MAGEVR_BUDGET_POP` with `niagara=<live>`, and
     make `PopulationFull` require 6.
   - In `WriteResults`, write 2.5's fields, and `scene.niagaraSystems` = the row count.
   - Run the verdict function. On "not exercised", call `Fail`.
6. **Script.** In `capture-budget.ps1`, print `live particles min/median/p95/max`, `gpu emitters max` and
   `particlesExercised`. Count a run as ok only when `$Driver.particlesExercised` is true.
7. **Tests.** Add 4.5's tests.
8. **Suite.** Regenerate the corpus as CLAUDE.md says (`node apps/vr/tools/clipgen/generate.mjs --corpus 30 --seed
   1000`, then `node apps/vr/tools/clipgen/mousepaths.mjs`). Run the full `MageArena` suite.
9. **Captures.** `powershell -NoProfile -File apps/vr/tools/capture-budget.ps1 -Runs 3 -Prefix fx`. Commit the three
   summaries under `docs/research/budget-dg4/`, and a new BUDGET-DG4 section with the particle rows.

### 5.3 Acceptance

- `build.ps1` exits 0.
- The `MageArena` suite: the previous count plus the 3 new tests, all passing. The only failures allowed are those
  CLAUDE.md lists as known on a clean tree (`MageArenaDesign.Duel.FireSeated`, `Session.FullSeated`,
  `Session.Wave1Seated`), and only if they also fail on the base.
- `create-budget-fx.ps1` logs 6 `MAGEVR_BUDGET_FX` lines, exactly one with `sim=gpu`. A second run writes nothing new
  (`git status` clean).
- Three 60 s runs, each with `MAGEVR_BUDGET_DONE`, both `MAGEVR_BUDGET_POP` lines showing `niagara=6`, and in
  `driver.json`:
  - `particlesExercised: true`;
  - `liveParticles` > 0, expected near the authored 1,470 peak (GPU over-reports, 2.3);
  - `gpuEmitters` = 1;
  - `zeroParticleFrames` = 0.

  The verdict per row is whatever the numbers say. A failed row is evidence too.
- `node apps/vr/tools/check-pin.mjs` prints OK, and `git status --porcelain apps/vr/data` prints nothing.
- **For V1 (R6):** the APK's cook log lists `/Game/Budget/NS_Budget_*`. That proves `DirectoriesToAlwaysCook` took.

## 6. What this file cannot prove

- The templates' real maximum particle counts and parameter names. They are in binary assets, and step 3 dumps them.
- That `InitializeSystem` and `AddEmitterToSystem` run cleanly in a commandlet with no editor UI, and that the compile
  finishes headless. Both are plain editor-module calls with no Slate in the paths read, but no commandlet has run here.
- Any device behaviour: the GPU emitter in both eyes, the readback lag on Adreno, the CPU cost of 870 CPU particles on
  the Quest game thread.
- The R8 reading of engine template content (1.2) is mine, not the owner's.
