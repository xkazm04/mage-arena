---
title: Brennic and the cinder hound s7101 in Unreal - import, ember material, dusk stills and the D-G4 budget
channel: vr
date: 2026-10-24
status: RAN - both FBX imported (skeletal mesh, skeleton, four clips each) by a new commandlet into a scratch Content folder that was deleted again; hound material with a hue/saturation/value crack key driving an ember emissive and a dark-red body tint; eight 1920x1080 stills under the T56 dusk; D-G4 re-run with both in the scene stays inside every row. Nothing imported is committed (DINOv3 hold).
sources: [docs/DECISIONS.md (2026-10-24 visual approvals, 2026-10-21 licence ledger), docs/research/CREATURE-MESH-2026-10-24.md, docs/research/BRENNIC-MESH-REWORK-2026-10-24.md, docs/research/DUSK-LIGHTING-T56-2026-10-24.md]
---

# Creature UE import, 2026-10-24

**Label: desktop proxy (RTX 4090, D3D12, forward shading, `-nohmd`), not Quest truth.** Engine UE 5.8.3 (CL 58210709).
No combat number moved: nothing under `apps/vr/data/` was touched, and the dusk is T56's preset applied in code.

**Outcome.** Brennic (LOD0, bake finish) and the cinder hound s7101 import cleanly into UE: right height, right
facing, their own skeletons, all four clips each. They stand in the greybox arena under the T56 dusk at 8 m. The hound
gets a material built in code that keys its baked lava cracks into an orange emissive and darkens its body toward
dark red. With both in the D-G4 stress scene, the frame costs **+15 draw calls (124 median / 128 max of 150)** and
**+35.3k BasePass triangles (173k of 350k)**. Brennic reads at duel distance. The hound reads as a dark-red beast
with orange flecks at 8 m and as a lava-veined hound at 5 m. Its glow is real but thin at duel distance (section 6).

Code: `422b6d4` (commandlet, shared module, material, test), `26f88e9` (capture driver, budget scene, script),
`dec42da` (clang-format of the new files, whitespace only). The stills, budget and automation runs came from the
build of the `26f88e9` source before `dec42da` reformatted it (the shots' `manifest.json` names `9938252`, the
checkout's HEAD while that source was still uncommitted). The final build and suite run were on `dec42da` (section 8).

## 1. Inputs (read-only, sha256 checked by the script before every run)

| Asset | File | sha256 | Research doc |
|---|---|---|---|
| Brennic | `C:/Users/kazda/mage-art-staging/brennic-mesh-rework-2026-10-24/rig/Brennic.fbx` | `dc0a4f38eef1b8486f72e1ec81859bcba1605be3df412c4c6c72368b203967b0` | BRENNIC-MESH-REWORK (FBX, 1,895,580 bytes) |
| Brennic | `.../rig/T_Brennic_BaseColor.png` | `024d4aaa888d94b2f257be993a96e8ed7a80176cedffaa5b0735620b031facae` | BRENNIC-MESH-REWORK (`024d4aaa...`, same as `finish-bake/`) |
| Hound | `C:/Users/kazda/mage-art-staging/creature-mesh-2026-10-24/hound/rig/CinderHound.fbx` | `a650bc3e0ad23abc34291bd3da0dba35698598c430484c095aafc711d07e5449` | CREATURE-MESH (FBX, 1,468,124 bytes) |
| Hound | `.../hound/rig/T_CinderHound_BaseColor.png` | `b36529c7f1c79e093cde253ba59087f5120e391eec9169b480a33fbb73ece08d` | CREATURE-MESH (`T_<Name>_BaseColor.png`) |

All four matched. The hound rig folder held its FBX, so Blender was not needed. No TRELLIS, ComfyUI or Blender process
was started, and none was running at any Unreal launch (the script checks for Blender and for a listener on port 8188,
then waits for the Unreal lane).

## 2. What was built

| Piece | Where |
|---|---|
| `UImportCreatureMeshCommandlet` (`-run=ImportCreatureMesh -name= -fbx= -texture=`): texture import, FBX skeletal import, material built in code, slot assignment, shader/texture compile, save under `/Game/CreatureImportScratch/<Name>` | `Tools/ImportCreatureMeshCommandlet.{h,cpp}` |
| Shared module: scratch paths, the two specs (height, ember or plain), `FEmberParams`, the HLSL of the key and its C++ mirror `EmberMask`, `StandPointCm`, `SpawnCreature`, `PlaceCreature` (facing read from the bones), `SpawnScene` | `Greybox/CreatureImport.{h,cpp}` |
| `UCreatureImportCaptureDriver` (`-MageArenaCreatureImportCapture`): eight stills, counters and memory notes in `manifest.json` | `Greybox/CreatureImportCapture.{h,cpp}` |
| `-MageArenaCreatureImportScene`: both meshes spawned beside any other run (used with the D-G4 stress flags) | `Greybox/ArenaGreyboxActor.cpp` (`UArenaGreyboxSubsystem::OnWorldBeginPlay`) |
| Session stays idle for the new capture; mouse look stands down for it | `Session/ArenaSession.cpp`, `Hands/MageArenaPlayerController.cpp` (`IsCaptureRun`) |
| `AssetRegistry` dependency (to find the imported Idle clip) | `MageArenaVR.Build.cs` |
| `MageArena.CreatureImport.EmberKey` | `Tests/CreatureImportTests.cpp` |
| Driver script | `apps/vr/tools/capture-creature-import.ps1` |

Nothing was committed under `Content/`. The script removes `apps/vr/Game/Content/CreatureImportScratch` in a `finally`
block. `git status` stayed clean apart from source files after every run.

## 3. Import settings

| Setting | Value and why |
|---|---|
| Importer | the **legacy FBX factory** (`UFbxFactory`), named on the `UAssetImportTask` so AssetTools skips Interchange; options through `UFbxImportUI` |
| Type | skeletal mesh, `bImportMesh`, `bImportAnimations`; no physics asset; no materials or textures from the FBX (rig_anim.py exports with `path_mode='STRIP'`) |
| **Scale** | **`ImportUniformScale = 100`** for mesh and clips. At 1, Brennic came in 1.8 cm tall: rig_anim.py's `apply_scale_options='FBX_SCALE_ALL'` writes metres into a file the importer reads as centimetres. The commandlet refuses a mesh whose bounds height misses the research doc's by more than 5 %. That check caught the first run |
| **Facing** | `bConvertScene = true`, `bForceFrontXAxis = false`, `bConvertSceneUnit = false`. The meshes face **-Y in Blender** and come in facing **+X** in UE. Measured on the ref pose: Brennic's `ball_l` / `ball_r` at (6.2, -17.4 / +17.3, 3.0) cm and `head` at (2.9, 0, 152.9); the hound's `head` at (102.0, -1.0, 109.0) cm. The runtime does not assume this: `PlaceCreature` takes the mesh forward from the `head` and `ball_*` bones (both read (1.00, -0.01)) and yaws the actor to face the centre pad (Brennic 170.5 deg, hound -169.5 deg at their 8 m spots) |
| **Skeleton** | a new skeleton per mesh (`SK_Brennic_Skeleton`, 23 bones; `SK_CinderHound_Skeleton`, 26 bones). No shared or mannequin skeleton and no retarget |
| Texture | `T_<Name>_BaseColor`, 2048x2048, sRGB, default compression: **DXT1, 2.69 MB with all mips** each |
| Result, Brennic | 23 bones, **23,990 LOD0 tris**, 1 LOD, 1 slot, height **1.7992 m** (bounds extent 19.2 x 51.3 x 90.0 cm). Clips `Idle` 2.000 s / 61 keys, `Cast` 1.533 / 47, `Hit` 0.800 / 25, `Death` 2.000 / 61. Mesh resource 0.80 MB |
| Result, hound | 26 bones, **11,796 LOD0 tris**, 1 LOD, **2 slots** (body + beast_parts' mouth lining), height **1.3569 m**, length 2.59 m along +X. Clips `Idle` 2.000 / 61, `Attack` 1.167 / 36, `Hit` 0.800 / 25, `Death` 2.000 / 61. Mesh resource 0.36 MB |

Triangles, bones, heights and clip keys all match the research docs. Each commandlet run took about 1.5 s of import work
and about 12 s of editor start-up.

## 4. Materials

**Brennic, `M_Brennic`:** base colour from `T_Brennic_BaseColor` (a `TextureSampleParameter2D` named `BaseColor`),
roughness 0.8 and metallic 0, as the Blender `M_Brennic`.

**Hound, `M_CinderHound`:** the same sample goes into a Custom HLSL node (`CreatureImport::EmberHlsl()`), which returns
the shaded body colour in rgb and a crack mask in a. The emissive is mask x `EmberColour` x `EmberIntensity`. Every
number is a material parameter.

**How the mask was derived.** It was measured offline on the encoded PNG (island texels; `ember-key/hue.py` in staging):

| Texel class (value band) | Median sRGB | Hue | Saturation |
|---|---|---|---|
| Body, V 0.2-0.3 (69 % of island texels) | (0.239, 0.145, 0.110) | 15.5 deg | 0.53 |
| Lit body, V 0.3-0.4 | (0.337, 0.176, 0.102) | 18.0 | 0.69 |
| Crack, V 0.4-0.5 | (0.443, 0.216, 0.102) | 19.5 | 0.77 |
| Crack, V 0.5-0.6 | (0.518, 0.247, 0.102) | 20.4 | 0.80 |
| Eyes, V 0.6-0.9 (~1,600 texels) | (0.847, 0.671, 0.235) | 30-41 | 0.73-0.77 |

Hue barely separates body from crack (15 vs 20 deg), so **saturation and value carry the key**. The hue window only
drops texels that are not orange. The shader takes the key on `pow(C, 1/2.2)`, because the sample is linear and the
key was measured on encoded values:

`mask = (1 - smoothstep(HueWidth/2, HueWidth, |hue - HueCentre|)) * smoothstep(SatLow, SatHigh, S) * smoothstep(ValLow, ValHigh, V)`

`body = lerp(C, BodyTint * (lum(C) / lum(BodyTint)) * BodyGain, BodyTintAmount)`; `base = lerp(body, C, mask)`

| Parameter | Value |
|---|---|
| `HueCentreDeg` / `HueWidthDeg` | 20 / 40 (full weight to +-20 deg, zero at +-40: the eyes at 30-41 deg stay in) |
| `SatLow` / `SatHigh` | 0.60 / 0.72 |
| `ValLow` / `ValHigh` | 0.34 / 0.46 |
| `EmberColour` (linear) | (1.0, 0.25, 0.03) |
| `EmberIntensity` | **1.6** (`-MageArenaEmberIntensity=` overrides it through a dynamic instance) |
| `BodyTint` (linear) / `BodyTintAmount` / `BodyGain` | (0.32, 0.03, 0.025) / 0.7 / 1.0 |

About **10.5 % of island texels key above 0.5**: the veins, the paw pads and the eyes. Staging
`ember-key/hound-basecolor-vs-key-mask.png` shows the base colour next to the mask. The test pins the classes above:
body < 0.02, lit body < 0.05, dim crack > 0.8, crack > 0.95, eye > 0.8, and grey, blue and magenta < 0.01. It failed
red with `SatLow` at 0.80 (dim crack 0.000, eye 0.000) and passed green at 0.60.

**Why intensity 1.6 and not more.** The capture camera runs manual exposure with no tonemapper, so an emissive
channel above 1 clips. The first stills (try3: (1, 0.32, 0.05) x 6) clipped the cracks to pale yellow-white. At
(1, 0.25, 0.03) x 1.6, green stays near 0.4 and the cracks render orange: **sRGB median (251, 153, 66)** at 8 m and
(255, 157, 67) at 5 m.

**The hound's second slot** (beast_parts' mouth lining, flat Ember orange) also gets `M_CinderHound`, because the
commandlet gives every slot the one material. Its orange keys as crack, so the lining glows when the jaw opens. In
the Idle stills it is not visible.

## 5. Placement and captures

Both stand on the sand **8 m ahead of the centre pad**, which is the near-centre spawn marker's distance
(`arena-layout.json` `spawnMarkers`) and where the greybox rival stands in a duel. They stand **1.4 m to the left
(Brennic) and right (the hound)**. The first stills put them on the centre line, and the near-centre marker's post
stood through their legs. Each mesh loops its imported `Idle` clip and casts a shadow. The camera stays seated
(`LookAtArena`, pitch -4 deg, 90 deg FOV). For each turnaround the asset turns in place: 0 (front), 45
(three-quarter), 180 (back). The session stays idle (no bout), as in the greybox capture.

Commands:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File apps/vr/tools/build.ps1
# the run of record: import both, eight stills, D-G4 x1 without and x2 with the meshes, copy to staging, delete scratch
powershell -NoProfile -ExecutionPolicy Bypass -File apps/vr/tools/capture-creature-import.ps1 -Run creature-import -Budget -BudgetRuns 2 -BaselineRuns 1
# what it launches, per asset and then for the stills / budget:
UnrealEditor-Cmd <uproject> -run=ImportCreatureMesh -name=CinderHound -fbx=<rig>/CinderHound.fbx -texture=<rig>/T_CinderHound_BaseColor.png -unattended -nosplash -nop4
UnrealEditor-Cmd <uproject> -game -RenderOffScreen -ResX=1920 -ResY=1080 -nohmd -unattended -MageArenaCreatureImportCapture -MageArenaRun=creature-import
capture-budget.ps1 -Runs 2 -Prefix creature-import-with -ExtraArgs @("-MageArenaCreatureImportScene")
```

**Staging: `C:/Users/kazda/mage-art-staging/creature-ue-import-2026-10-24/`**

| Path | What |
|---|---|
| `shots/01-brennic-front.png`, `02-brennic-three-quarter.png`, `03-brennic-back.png` | Brennic at 8 m |
| `shots/04-hound-front.png`, `05-hound-three-quarter.png`, `06-hound-back.png` | the hound at 8 m |
| `shots/07-hound-glow-near.png` | the hound at 5 m, turned 30 deg, to show the glow (at 3.5 m the dais edge hid its front half; the pawn re-applies its FOV every tick, so no zoom) |
| `shots/08-both-duel.png` | both side by side at 8 m |
| `shots/manifest.json` | per-shot pixel checks, counters, memory notes, ember intensity |
| `shots-crop-sheet-2x.png` | 2x crops of 01-06 around the subject |
| `uasset/Brennic/*.uasset`, `uasset/CinderHound/*.uasset` | the 16 imported assets (mesh, skeleton, 4 clips, texture, material each; 13 MB) |
| `ember-key/` | the mask preview and the two analysis scripts |
| `budget/creature-import-{without1,with1,with2}/` | `summary.json`, `driver.json`, `samples.json`, `capture.log` |
| `logs/` | builds, both imports, the capture, the run of record (`final.log`), automation |
| `try2/`, `try3/`, `try4/` | the iterations: try2 failed (shaders still compiling after 120 s in `-game`; the fix compiles them in the commandlet), try3 (ember x 6, clipped), try4 (the settings of record) |

## 6. An honest look at each still

The sun stands low, ahead-left of the seated view and behind the opponents (T56). Both meshes are therefore
**back-lit**: the player sees their shadow side, lit by the rose-lilac sky fill. Their shadows fall toward the viewer.

- **Brennic reads.** At 8 m he is about 200 px tall in the 1080p frame. In the front and three-quarter views you can
  pick out the red tunic, the dark cloak, the belt, the bronze bracers and the brooch, and the bearded head reads as
  a face. The back (03) is one flat dark olive cloak with arms and boots below. He reads clearly as a person and as
  the rival against the tan sand. He is matte (base colour only, no metal sheen), and the soft texture and the
  missing face detail (BRENNIC-MESH-REWORK's critic findings) do not show at this distance. He casts a long soft
  shadow, which the greybox rival does not.
- **The hound at 8 m reads as a fire creature, but weakly.** It is about 130 x 170 px (front, forelegs crouched).
  The body is a dark red-brown, sRGB median (75, 35, 29), no longer the bake's chocolate brown. Orange cracks
  (251, 153, 66) cover about **9 % of its pixels** in the front view. At duel distance the veins break into flecks
  and short dashes; they do not read as a continuous network. The eyes and the paw pads glow, so the glowing toes
  carry the read as much as the veins. The three-quarter view (05) shows the vein network best. The back (06) is
  mostly dark, with a few cracks on the haunches.
- **At 5 m (07) the glow clearly reads.** Branching orange lava veins on a dark-red hide, glowing eyes and ear rims,
  bright paw pads. This is the closest the meshes come to the creature master's "glowing cracks" identity. The
  geometry gaps the critic listed (no faceted plates, a realistic-dog build, a thin tail) are unchanged, since a
  material cannot fix them.
- **Both together (08):** the pair reads as a rival and his beast. Brennic is the stronger read; the hound is
  smaller, darker and lower, and reads mainly through its glowing points.

**Does the hound's glow read at duel distance at dusk?** Partly. It shows as orange flecks and glowing eyes and paws
on a dark silhouette, enough to say "fire beast" beside the greybox's flat shapes. It is not the master's lit-crack
body. Stronger options, each a style call (Athena), not taken here:
1. widen the mask (lower `ValLow`, so more of the dim vein edges glow);
2. raise `EmberIntensity` and accept yellow-white clipping (try3);
3. add a tonemapper or exposure change (that moves the whole T56 frame);
4. bake a wider crack mask offline;
5. add a rim or fresnel term on the body.

## 7. Budget: D-G4 count proxies, before and after (capture-budget.ps1)

Each run sampled 60 s after a 10 s warm-up, with 20 enemies, 100 projectiles, both hands and 6 Niagara systems. Every
run logged `MAGEVR_BUDGET_DONE`, no `MAGEVR_BUDGET_FAIL`, `particlesExercised` true and scene parity ok (planned 210
parts / 11 tuples). BasePass is the GPU-culled `r.MeshDrawCommands.DumpStats` reading (6 dumps a run) that judges the
350k row (DECISIONS 2026-10-24). In the `with` runs both meshes stood at 8 m and were in view: the BasePass rise equals
their LOD0 triangles.

| Run | Draw calls median / max | BasePass tris median / max | ShadowDepth tris | All passes (logged) median / max | Live particles median | Peak working set | GPU ms median |
|---|---|---|---|---|---|---|---|
| T56 after1..3 (DUSK-LIGHTING-T56 section 2) | 109 / 113 | 137,488-138,296 / 139,664-140,624 | 31,536 | 402,304-404,228 / 408,538-411,438 | 1,397-1,400 | - | - |
| this run, without1 (same build, no meshes) | 109 / 113 | 137,608 / 138,656 | 31,536 | 401,942 / 405,542 | 1,399 | 3,267 MB | 0.85 |
| **with1** (Brennic + hound) | **124 / 128** | **172,794 / 175,450** | **103,108** | 579,794 / 587,756 | 1,399 | 3,290 MB | 0.87 |
| **with2** | **124 / 128** | **173,066 / 175,450** | **103,108** | 581,624 / 587,752 | 1,399 | 3,202 MB | 0.87 |
| **Limit** (PROJECT-PLAN section 7) | **<=150** | **<=350,000** | | | **<=2,000** | | |

- **Draw calls +15** (109 -> 124 median, 113 -> 128 max). Three mesh sections (Brennic 1, hound 2) times base pass,
  depth prepass, velocity and two shadow cascades. 22 calls of headroom remain at the max.
- **BasePass +35.3k** (137,608 -> 172,930 mean of the two medians). That is the meshes' 23,990 + 11,796 = 35,786
  LOD0 triangles. The row stands at 49 % of 350k.
- **ShadowDepth +71.6k** (31,536 -> 103,108), the two meshes in **both** CSM cascades (2 x 35.8k). It is logged, not
  gated. The meshes cast shadows; the greybox session parts still do not (T56 section 5). On Quest the cascades are
  also 2 (`r.Shadow.CSM.MaxMobileCascades=2`), so the same doubling applies there.
- **Materials:** the D-G4 driver's `sharedMaterials` (26 distinct over 1,452 presentation parts, 24 tints) counts the
  session presentation's parts only, so it does not move. The two meshes add **2 materials over 3 slots**
  (`M_Brennic`; `M_CinderHound` on both hound slots).
- **Texture memory:** **+5.38 MB** (two 2048x2048 DXT1 textures, 2.69 MB each with mips). Mesh resources add
  +1.16 MB (0.80 + 0.36). The process working set moved within its run-to-run noise (3,267 -> 3,290 / 3,202 MB).
- The D-G4 driver's **LOD0 in-frustum estimate did not move** (146,882 -> 146,834 median). `Greybox::CountVisibleLod0`
  sums static meshes only, so skeletal meshes are invisible to it. The gate reads BasePass, which does count them,
  but any budget that reads the LOD0 estimate (the greybox capture's `budget.json`) under-counts skeletal creatures.
- The stills' own counters (`manifest.json`, previous frame): Brennic alone 36 draw calls / 96,647 RHI primitives,
  the hound alone 41 / 47,871, both 45 / 143,831. These are not the D-G4 scene.

## 8. Gates run

- `build.ps1`: `Result: Succeeded` (final build on `dec42da`).
- `Automation RunTests MageArena.` (`-nullrhi`): **found 295, 295 `Result={Success}`, 0 failed, `TEST COMPLETE.
  EXIT CODE: 0`**, both before `dec42da` and again on the final `dec42da` build. T56 recorded 294 at `b82042e`, and
  no source changed between `b82042e` and `9938252`; the one new test is `MageArena.CreatureImport.EmberKey`. Included:
  `MageArena.Input.CaptureFlags` (it reads the new flag from the script), `MageArena.Greybox.DuskLight` and
  `MageArena.Overlay.Golden`. The sigil corpus was regenerated first (CLAUDE.md).
- `node apps/vr/tools/lint/unused-members.mjs` OK; `node apps/vr/tools/lint/format-check.mjs` OK (11 changed C++
  files).
- `MageArenaDesign` was not run: this change touches no combat path.

## 9. Questions this leaves

- **The hound's glow at duel distance** is thin (section 6). The choice between a wider mask, a hotter emissive, a
  tonemapper or a baked mask is Athena's style call.
- **The hound's mouth-lining slot** takes the ember material and would glow when the jaw opens. Give it its own dark
  material, or keep the glow as "fire mouth"?
- **Scale.** At the class height the hound is 2.59 m long. Beside Brennic (1.80 m) at 8 m it looks right as a big
  beast. The class-height question from CREATURE-MESH stays open for the owner or Athena.
- **The LOD0 estimate ignores skeletal meshes** (section 7). Extending `Greybox::CountVisibleLod0` to skinned
  components belongs to a budget card, outside this lane's paths.
- **The maw s7202 was not imported** (out of this lane's scope). The same commandlet and script take it with one
  more spec row.
- Nothing here ships while the DINOv3 hold stands. The imported assets live only in staging.
