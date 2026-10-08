# Does the Quest build merge draws? (D-G4 lever B)

**Label: source reading, not a device measurement.** No Unreal process ran for this file. Every verdict below comes
from engine source, engine config and upstream pages. V1 must still confirm each one on a Quest 3.

The question comes from `docs/research/BUDGET-DG4-2026-10.md` 7.7. Lever B (one material per colour) took Basepass
from 118 to 21 draws on the desktop proxy, because dynamic instancing merged the 160 stress parts (7.2). 7.7 lets lane A
deliver B in one of two ways: (a) one shared material that reads its colour from custom primitive data, or (b) a
per-colour MID cache. It left open whether the mobile forward renderer merges such draws at all.

## 0. Provenance

- **Engine:** `C:\Program Files\Epic Games\UE_5.8\Engine`, `Build/Build.version`: 5.8.3, Changelist 58210709,
  CompatibleChangelist 55116800, `++UE5+Release-5.8`. The stock launcher build: `WITH_OCULUS_BRANCH` is not defined.
- **Date:** 2026-10-08.
- **Project config read:** `apps/vr/Game/Config/DefaultEngine.ini` and `Config/Android/AndroidEngine.ini` at `b99bbeb`.
  Also the Meta XR plugin 1.207.0 in the main checkout (`apps/vr/Game/Plugins/MetaXR/`, untracked, read only).
- **Reconcile:** `git grep -n -i -E 'SupportGPUScene|GPUScene|dynamic instancing|custom primitive data' -- docs apps/vr/Game/Config`
  (exit 0) found only BUDGET-DG4 lines 131, 166, 235, 312, 421 and 423. None of them answers Q1.
- **Paths:** engine files are under `Engine/Source/Runtime/` unless they start with `Config/` or `Shaders/`.
  Renderer files are under `Renderer/Private/` unless noted.
- **The path in question:** Android, Vulkan, mobile forward (`r.Mobile.ShadingPath=0`), feature level ES3_1, shader
  platform `VULKAN_ES3_1_ANDROID`, mobile multiview on, `Meta_Quest_3` device profile.

## 1. Q1: does the Quest build merge draws?

### 1.1 GPU scene is on for mobile, by default

- `r.Mobile.SupportGPUScene` still exists in 5.8. It defaults to **1**, is `ECVF_ReadOnly` and is described as
  "required for auto-instancing (only Mobile feature level)" (`Core/Private/HAL/ConsoleManager.cpp:4059-4064`).
- On a mobile platform, `UseGPUScene` reads only this cvar (`RenderCore/Private/RenderUtils.cpp:1775-1783`, through
  `FReadOnlyCVARCache::MobileSupportsGPUScene`, `RenderCore/Public/ReadOnlyCVARCache.h:66-73`, cached at
  `RenderCore/Private/ReadOnlyCVARCache.cpp:114` and `:134`). The data-driven `bSupportsGPUScene` flag is not read
  for mobile. The `VULKAN_ES3_1_ANDROID` section does not set it anyway (`Config/Android/DataDrivenPlatformInfo.ini:83-107`).
- Only a `PROJECT_CVAR_MOBILE_SUPPORTS_GPUSCENE` define in the project's build files could override the cvar
  (`ReadOnlyCVARCache.h:69-70`). The project has none.
- Nothing sets it: there are no matches in `Config/BaseEngine.ini`, `Config/Android/BaseAndroidEngine.ini`,
  `Config/BaseDeviceProfiles.ini` (the chain is `Meta_Quest_3` → `Android_OpenXR` → `Android_Mid`, `:1432-1449`), the
  project's two ini files, or the Meta XR plugin's `Config/*.ini`.
- The cvar is part of the mobile shader key (`MobGPUSc`, `RenderCore/Private/Shader.cpp:2043-2051`). Changing it
  later rebuilds every mobile shader.
- `r.Mobile.UseGPUSceneTexture`, which Epic's page below still names, has no definition in 5.8.3 source. Only a stale
  comment is left (`SceneRendering.h:1624`).

**As configured, the project runs GPU scene on Quest.** No line needs to change.

### 1.2 Mesh-draw-command merging on that path, and what it needs

- **The gate.** `IsDynamicInstancingEnabled` = `r.MeshDrawCommands.DynamicInstancing` (default 1) and `UseGPUScene`
  (`SceneRendering.cpp:292-296`, `:305-309`). With GPU scene on, every pass compacts identical commands:
  `SetupDrawCommands(..., true, ...)` (`MeshDrawCommands.cpp:1166-1174`).
- **The key.** A cached command gets a state bucket only when GPU scene is on (`MeshPassProcessor.cpp:1953-1956`,
  `:2088-2100`). Two commands share a bucket when they match on pipeline, stencil, shader bindings, vertex streams,
  index buffer, first index, primitive count and instance count (`Renderer/Public/MeshPassProcessor.h:1333-1346`).
  The material's uniform buffer is one of the shader bindings, so **each MID is its own bucket**.
- **Primitive data does not split buckets.** When the vertex factory takes a primitive id, the per-primitive uniform
  buffer is not bound (`ShaderBaseClasses.cpp:503-517`). The engine's static mesh vertex factory does on mobile:
  attribute 13 on both feature levels (`Engine/Private/LocalVertexFactory.cpp:539`, the flag at `:674`, and
  `RenderCore/Private/VertexFactory.cpp:351-371`).
- **The merge.** `FInstanceCullingContext::SetupDrawCommands` folds consecutive commands with the same bucket and the
  same culling payload (LOD and screen size) into one draw (`InstanceCulling/InstanceCullingContext.cpp:1527-1545`).
  The mobile base pass builds this context with `EBatchProcessingMode::UnCulled` (`MobileShadingRenderer.cpp:537`).
  So a single-instance static mesh is a direct draw (`InstanceCullingContext.cpp:1531`).
- **Quest difference 1: the uniform-buffer-view path, 32 parts per draw.** Mobile platforms with uniform buffer objects
  fetch GPU-scene data through a uniform buffer window (`RenderCore/Private/RenderUtils.cpp:496-499`).
  `VULKAN_ES3_1_ANDROID` sets `bSupportsUniformBufferObjects = true` (`DataDrivenPlatformInfo.ini:100`). On that
  path a merged draw holds at most `16 KB / 512 B = 32` primitives (`InstanceCullingContext.cpp:1510`,
  `Core/Public/HAL/Platform.h:694-696`, `Shaders/Shared/SceneDefinitions.h:118`). The desktop D3D12 path has no cap.
- **Quest difference 2: no merge for indirect draws.** On the same path, a command that draws indirectly (ISMs,
  forced instance culling) is never merged (`InstanceCullingContext.cpp:1531-1534`, comment at `:1532`). The hand ISMs
  are already one draw each, so this costs nothing here.
- **The sort keeps buckets together.** `r.Mobile.MeshSortingMethod` defaults to -1, which falls back to
  `r.MeshSortingMethodWithoutEarlyZ` = 0, "sort by state" (`MeshDrawCommands.cpp:28-43`, `:365-368`). Method 1
  (strict front to back) sorts by depth before bucket (`:276-298`, `:370-390`) and would interleave buckets. Neither
  the engine config nor the project sets either cvar.
- **Lit materials split further.** The mobile base pass binds a per-primitive reflection capture buffer, a
  lighting-channel light buffer and a CSM flag (`MobileBasePass.cpp:690-720`). CSM receivers get their own bucket bit
  (`MeshDrawCommands.cpp:258-266`). The greybox materials are unlit, so none of this applies today. It applies to
  the art pass.

### 1.3 Way (a): one material, colour from custom primitive data

- Custom primitive data lives in the GPU-scene primitive record (`GPUScene.cpp:2456-2461`), not in the shader
  bindings. Parts with the same mesh and material keep one bucket, whatever their data.
- On the Quest path each merged instance reads its own record. The mobile writer copies all 9 float4s per draw
  instance, and the loader reads them by `DrawInstanceId` (`Shaders/Private/SceneDataMobileWriter.ush:150-159`,
  `Shaders/Private/SceneDataMobileLoader.ush:168-177`). Nine float4s fit in the 512-byte slot.
- A recolour is `SetCustomPrimitiveData*` → `FScene::UpdateCustomPrimitiveData` → a primitive update
  (`Engine/Private/Components/PrimitiveComponent.cpp:2681-2691`, `RendererScene.cpp:1961-1964`). No render state is
  rebuilt, and per-frame fades need no quantising.

**Verdict (a): merges on Quest as configured,** up to 32 parts per draw.

### 1.4 Way (b): one MID per colour, shared

- Parts that share one MID and one mesh share one bucket (1.2), so they merge, up to 32 parts per draw.
- A recolour is a `SetMaterial` swap, which calls `MarkRenderStateDirty` (`Engine/Private/Components/MeshComponent.cpp:63`,
  `:112`). The proxy and its cached commands are rebuilt on the game and render threads. That is CPU cost per swap,
  not a draw. A fade that swaps every frame pays it every frame.

**Verdict (b): merges on Quest as configured,** up to 32 parts per draw.

### 1.5 Mobile multiview

- Under mobile multiview the view is not instanced stereo. The two modes are exclusive (`Engine/Private/SceneView.cpp:970-992`),
  and `IsInstancedStereo()` is false (`:1129-1132`). The base pass therefore uses `EInstanceCullingMode::Normal` with
  one view id (`MobileShadingRenderer.cpp:515-523`) and an instance factor of 1 (`MeshDrawCommands.cpp:1420`).
- Only the primary view renders (`SceneRendering.h:1877-1894`). The base pass is a multiview render pass with
  `MultiViewCount = 2` (`MobileShadingRenderer.cpp:2116`), so the GPU broadcasts each merged draw to both eyes.
- The "multi-view goes generic" branch applies only to more than one view id outside stereo mode
  (`InstanceCullingContext.cpp:1468`). It does not apply here.

**Merging composes with multiview.** One merged draw covers both eyes.

### 1.6 What could turn it off

- **A Meta fork rule.** The Meta XR plugin's setup tool has a "Disable Support GPUScene" rule. It writes
  `r.Mobile.SupportGPUScene=False` under `[/Script/Engine.RendererSettings]` when GPU scene meets Emulated Uniform
  Buffers, Mobile Uniform Local Lights, XR soft occlusions or late latching
  (`Plugins/MetaXR/Source/OculusXRProjectSetupTool/Private/Rules/OculusXRRenderingRules.h:353-370`,
  `.cpp:377-405`). It compiles only under `WITH_OCULUS_BRANCH`, so it does not exist on this stock engine. If the
  project ever moves to Meta's engine fork, or enables one of those four features, merging goes away.
- **The Epic page is older than the source.** "Using Mesh Auto-Instancing on Mobile Devices"
  (https://dev.epicgames.com/documentation/unreal-engine/using-mesh-auto-instancing-on-mobile-devices-in-unreal-engine,
  labelled "Unreal Engine 5.8 Documentation", no date on the page, read 2026-10-08) says:
  - "Learn to use this Beta feature, but use caution when shipping with it."
  - "r.Mobile.SupportGPUScene enables auto-instancing on mobile devices."
  - "Enabling this feature will cause shaders to be rebuilt for mobile platforms."
  - "Auto-instancing on mobile mainly benefits projects that are heavily CPU-bound rather than GPU-bound."

  It tells you to add `r.Mobile.SupportGPUScene=1` and `r.Mobile.UseGPUSceneTexture=1`. In 5.8.3 the first is already
  the default, and the second no longer exists (1.1). The page does not mention Quest, multiview or custom primitive data.
- **No Meta or Epic page found on GPU scene on Quest** (background search, 2026-10-08). Bug UE-160597
  (https://issues.unrealengine.com/issue/UE-160597) is reported in search snippets as "custom primitive data changes
  not reflected on Android Vulkan" (a 5.0.3 regression). The page returned HTTP 403 on 2026-10-08, so its status
  is unknown. V1 must check that a recolour shows on the device under way (a).

### 1.7 Q1 verdicts

| Way | Verdict |
|---|---|
| (a) shared material + custom primitive data | **merges on Quest as configured**, at most 32 parts per draw |
| (b) per-colour MID cache | **merges on Quest as configured**, at most 32 parts per draw |

No config change is needed. The changes to avoid: `r.Mobile.SupportGPUScene=0`, `r.MeshDrawCommands.DynamicInstancing=0`
and `r.Mobile.MeshSortingMethod=1`.

## 2. Q2: HZB occlusion and occlusion feedback on mobile forward

### 2.1 `r.HZBOcclusion=1`: does not work on this path

- The cvar defaults to 0 (`SceneVisibility.cpp:123-132`). The per-view state turns HZB on for any non-OpenGL platform
  whose data-driven info allows it, with no feature-level check (`:3227-3230`). `bSupportsHZBOcclusion` defaults to
  true (`RHI/Private/DataDrivenShaderPlatformInfo.cpp:153`), and the Android file does not override it.
- With HZB on, visibility only records bounds (`SceneVisibility.cpp:2982-2985`). The tests run only in the deferred
  renderer (`DeferredShadingRenderer.cpp:624-627`). The mobile renderer never calls `Submit`, and it builds an HZB only
  for SSAO, SSR or instance occlusion culling (`MobileShadingRenderer.cpp:3472-3497`).
- The readback object exists only at SM5 and above (`SceneOcclusion.cpp:820-834`). Once a frame is marked valid
  (`SceneVisibility.cpp:3292-3295`), `MapResults` locks that readback (`SceneOcclusion.cpp:845-858`). On ES3_1 it
  is null.

**Verdict:** it does not work on the 5.8.3 mobile forward renderer. At best nothing is culled, and at worst the read
dereferences a null readback. It costs no draws, because none are issued. It is **not** a fallback for lever A.

### 2.2 `r.OcclusionFeedback.Enable=1`: works on mobile, but only with queries on

- Default 0, "Currently works only with a mobile rendering" (`SceneVisibility.cpp:134-139`). It is set up per view at
  ES3_1 only (`:3280-3290`, `RendererScene.cpp:398-405`). The `Meta_Quest_3` profile sets it to 0 explicitly
  (`Config/BaseDeviceProfiles.ini:1448`). The project would have to override that profile, for example in a project
  `Meta_Quest_3` device profile.
- **It needs `r.AllowOcclusionQueries=1`.** The feedback draws are issued inside the same `BeginOcclusionTests` flush
  (`SceneOcclusion.cpp:1405-1410`). The mobile renderer calls it only when `DoOcclusionQueries()` is true
  (`MobileShadingRenderer.cpp:995-1000`), and visibility disables submissions when it is false
  (`SceneVisibility.cpp:5182-5186`). So it **replaces** lever A (7.7 ships A as queries off). It cannot run on top of A.
- **Draws:** the per-primitive hardware query branch is replaced by a feedback entry (`SceneVisibility.cpp:2975-2980`).
  The boxes are batched 512 to a draw (`SceneOcclusion.cpp:1803-1815`, the draw loop from `:1938`). Each frame also
  adds one UAV clear pass and one readback copy pass (`:1695-1717`). With the stress scene's ~190 tested primitives,
  that is **1 draw** against BeginOcclusionTests ≈155 on desktop. It also turns the Adreno separate-pass mode off
  (`MobileShadingRenderer.cpp:686`), so the boxes draw in the main pass.
- **Cost not seen in source:** pixel-shader UAV writes per box pixel, and results that lag by the buffered frames.
  Epic's reason for setting 0 on Quest 3 is not given in source.

**Verdict:** it works on the 5.8.3 mobile forward renderer. It costs about 1 draw per 512 tested primitives plus two
small passes, keeps culling, and needs occlusion queries left on. It is the only real fallback if A's triangle cost
bites. It is unproven on the device, and the engine's own Quest 3 profile turns it off.

## 3. Q3: does a multiview occlusion query cost one draw or two?

**One draw for both eyes** (source only).

- The batchers pass `NumInstances = 2` only for instanced stereo (`SceneRendering.cpp:999-1000`). Multiview is not
  instanced stereo (1.5), so each batch is one `DrawIndexedPrimitive` with one instance (`SceneOcclusion.cpp:479-495`).
- Only the primary view renders, so queries are allocated and drawn once (`SceneRendering.h:1877-1894`).
- The query pass is a multiview pass. The Adreno pass copies the base pass's `MultiViewCount`
  (`MobileShadingRenderer.cpp:2305-2311`). The Vulkan RHI reserves `MultiViewCount` query slots per query
  (`VulkanRHI/Private/VulkanQuery.cpp:140-149`): one slot per eye, one draw call.

The RHI draw counter therefore sees one draw per query batch. Whether the driver pays for two eyes in GPU time is a
device question.

## 4. Q4: upstream releases since 2026-10-07

- **UE 5.8.4:** not shipped. The forum hotfix tag lists "5.8.3 Hotfix Released", September 22, 2026, as its newest
  (https://forums.unrealengine.com/tag/hotfix/11407, read 2026-10-08).
- **Meta XR UE5 Integration:** still 207.0, "Updated: Sep 23, 2026"
  (https://developers.meta.com/vr/downloads/package/unreal-engine-5-integration/, read 2026-10-08).
- **Interaction SDK:** still 207.0, "Updated: Sep 22, 2026"
  (https://developers.meta.com/horizon/downloads/package/meta-xr-interaction-sdk-unreal/, read 2026-10-08).

## 5. What this changes

**For lane A's choice.** Both ways merge on Quest. Merging does not decide the choice. The recolour path does:

- (a) recolours with no render-state rebuild and needs no cache or quantising. It needs one new material that reads
  custom primitive data, and V1 must see that a recolour shows on the device (UE-160597).
- (b) reuses the proven desktop path (`BudgetSharedTint`). Each colour change rebuilds the part's proxy, and fades
  must be quantised.
- **Recommendation:** (a) when T26's colour function writes colours often (fades, hit flashes), (b) when colours change
  rarely. Either way, keep the greybox materials unlit, or the lit bindings in 1.2 split buckets again.

**For reading D-G4 on the device.**

- Expect Basepass on Quest a little above the desktop 21. Every bucket over 32 parts splits: the 75 water spheres
  become 3 draws, not 1. Every other stress bucket holds 25 or fewer. So that is about +2 draws, still far under 150.
- Draws are per frame, not per eye (1.5, 3).
- If V1 sees Basepass near 118 on the device, first check `GPUScene: Enabled` in the Meta plugin's log line
  (`Plugins/MetaXR/Source/OculusXRHMD/Private/OculusXRHMD.cpp:5401-5403`).
- 7.7's fallback list changes. `r.HZBOcclusion` is out. Occlusion feedback stays as the only fallback, and it replaces
  A rather than joining it.

## 6. What V1 must still confirm on the device

- Basepass draws on a Quest 3 with B, and that the 32-part split matches.
- `GPUScene: Enabled` in the device log.
- Under (a), that a `SetCustomPrimitiveData*` recolour shows on Android Vulkan.
- One query draw per batch under multiview, from the RHI draw count with A off.
- If occlusion feedback is tried: its draw count, its GPU time and how well it culls, against A's triangle cost.

## 7. Commands run for this file

| Command | Exit |
|---|---|
| `git grep -n -i -E 'SupportGPUScene\|GPUScene\|dynamic instancing\|custom primitive data' -- docs apps/vr/Game/Config` | 0 |
| `grep`/`sed` reads of the engine tree and the plugin, as cited | 0 |
| `node apps/vr/tools/check-pin.mjs` | see the run's result.json |
