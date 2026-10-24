---
title: The draw-call budget of the creature cast - an honest LOD0 count, the draws per creature and pass, and the cuts that hold up
channel: vr
date: 2026-10-25
status: RAN - the visible-LOD0 estimate now counts skinned meshes (lod0InFrustumSkeletalTriangles, 53,538 with the cast in view, part of the total). Every creature's draws were measured per pass from the r.MeshDrawCommands.DumpStats rows. Each lever was measured on its own in one build. Two cuts hold up and are the new spawn defaults: a real UE-side section merge (one section per creature; the maw's tongue onto a body texel of its colour) and per-bone velocity off. Together they take the cast from +24 to +12 draw calls over the no-creature baseline (median 133 -> 121, max 136 -> 124 of 150). A fresh blind critic read the before and after stills as SAME. Shadows stay in the cascades: turning them off saves 9 calls, but a blind critic read it as VISIBLE LOSS of grounding at 8 m. Inset shadows cost 2 more calls. Generated LODs (LOD1 50 %, LOD2 25 %) take over past the near ring; at 8 m they read MINOR, so LOD0 stays at the duel distance, and BasePass at 8 m does not drop. Nothing imported is committed (DINOv3 hold).
sources: [docs/DECISIONS.md (2026-10-25, Athena), docs/research/CREATURE-CAST-UE-2026-10-24.md (section 2.6), docs/research/BUDGET-DG4-2026-10.md, apps/vr/data/vr/arena-layout.json (spawnMarkers)]
---

# The draw-call budget of the creature cast, 2026-10-25

**Label: desktop proxy (RTX 4090, D3D12, forward shading, MSAA 4x, `-nohmd`), not Quest truth.** Engine UE 5.8.3 (CL
58210709; `Build.version` was read before the first build). No combat number moved, and nothing under `apps/vr/data/`
was touched.

**Outcome.**

- **The honest count.** `Greybox::CountVisibleLod0Split` adds the LOD0 render-data triangles of every visible
  `USkinnedMeshComponent` whose bounds touch the frustum.
  - `driver.json` and `budget.json` gain a field, `lod0InFrustumSkeletalTriangles`. The skinned triangles are also in
    `lod0InFrustumTriangles`, so an old row compares with the total minus the new field.
  - With the cast in view the estimate reads **200,4xx (median), of which 53,538 skeletal**: exactly Brennic, the hound
    and the maw at LOD0. Before, it stayed at 146.9k.
  - `MageArena.Budget.Triangles.Skeletal` pins the count.
- **The cost, per creature and pass.** Before the cuts, every mesh section cost 5 draws: base pass, depth prepass,
  velocity and two shadow cascades. Brennic has 1 section and costs 5 draws; the hound and the maw have 2 sections each
  and cost 10 each. That is 25 mesh draw commands, measured as +24 RHI calls on the median (section 2).
- **The cuts (section 3).** Each lever was measured alone, on top of the uncut cast. The two that held up are the new
  defaults in `CreatureImport::FCreatureBudget`:
  - **Section merge, -9 calls.** Done in UE, with no Blender and no faked number: the mesh's polygon groups are
    relabelled onto one material slot and the mesh is rebuilt, so the render data has one section.
  - **Per-bone velocity off, -4 calls.** A creature standing at its spot draws no velocity.
  - Shadow off (-9) is a visible loss and was rejected. Inset shadows (+2) were rejected. The LODs change nothing at
    8 m by design.
- **After (section 4): 121 / 124 draw calls (median / max) in both runs, against 109 / 113 without creatures: +12 / +11,
  the target.** The max fell from 137 (CREATURE-CAST) and 136 (this build, uncut) to 124. That leaves 26 calls under
  150 and 21 under the 145 flag.
  - The velocity pass fell from 134k to 80.7k triangles, and all passes together from 669k to 615-617k.
  - **BasePass did not improve: 190.5-191.6k, the three LOD0s.** Only LOD1 at 8 m would cut it (-27.4k), and a blind
    critic read that as MINOR. It is a question for Athena (section 6), not a default.
- **Headroom.** A fourth or fifth creature with the cuts costs **4 calls** each (one section x base, prepass, two
  cascades): about 128 and 132 at the max. Without the merge, a two-slot creature costs 8 calls.

Code: the commits on `autopilot/accepted-idea-delivery-959ace93` (count, cuts, capture parsing, this doc). The runs of
record all came from one build (build 2). Build 3 differs from it only by one clang-format line break in
`CreatureImport.cpp`; the full suite ran on build 3.

## 1. How it was run

At most one Unreal process ran at any time. Every build, budget run, capture and suite run started only after the
previous one had exited, and `capture-budget.ps1` waits for the lane itself.

- The MetaXR plugins were copied in with `robocopy ... /E` (a copy).
- Build 1 failed at link time: the merge first used `FMeshDescription`'s exported mutators, and `MageArenaVR.Build.cs`
  does not link `MeshDescription`, which is outside this card's paths. The merge was rewritten on the description's
  inline accessors (section 3.1).
- Builds 2 and 3 logged `Result: Succeeded`.

**The creatures.** The 25 `.uasset` files of `creature-cast-ue-2026-10-24/uasset/` were copied into
`apps/vr/Game/Content/CreatureImportScratch` for the runs. That is the same import as CREATURE-CAST, so the FBX was not
re-imported, and Blender stayed down. The folder was deleted before the suite and before any commit.

**The budget runs.** `capture-budget.ps1` ran with the CREATURE-CAST 2.6 settings:

- 20 enemies, 100 projectiles, both hands and 6 Niagara systems;
- a 10 s warm-up, then 60 s of samples, with 6 `r.MeshDrawCommands.DumpStats` frames per run;
- 1920x1080 offscreen, through `drawcall-budget-2026-10-25/scripts/run-budget.ps1`.

The creatures come in with `-MageArenaCreatureImportScene`. Every lever has a switch, so one build measures it on and
off:

| Switch | Effect |
|---|---|
| `-MageArenaCreatureUncut` | the levers as before this card (imported sections, per-bone velocity, cascades, LOD0 only) |
| `-MageArenaCreatureMerge=0/1` | the section merge |
| `-MageArenaCreatureVelocity=0/1` | per-bone velocity (`bPerBoneMotionBlur`) |
| `-MageArenaCreatureShadow=cascades/inset/off`, `-MageArenaCreatureShadowMaxM=<m>` | the shadow mode, and no shadow beyond a distance |
| `-MageArenaCreatureLods=0/1`, `-MageArenaCreatureForceLod=<n>` | the generated LODs, and a forced LOD (a measurement switch) |
| `-MageArenaCreatureOnly=<Name>` | one creature alone (not needed: the dumps name each mesh) |

Every run logged `MAGEVR_BUDGET_DONE`, no `MAGEVR_BUDGET_FAIL`, `particlesExercised` true and parity ok (210 planned
parts / 11 tuples). Staging: `C:/Users/kazda/mage-art-staging/drawcall-budget-2026-10-25/budget/<run>/`, with the
summary, driver, samples, log and the six dumps of each.

**The per-pass numbers** come from `capture-budget.ps1`, which now reads more out of each dump:

- **Draws per pass:** each dump row is one mesh draw command (one draw, instanced or not).
- **Each `SK_*` resource's draws and triangles per pass**, with the LOD and the section (segment) each row drew.
- **Every `DrawCall/<category>` column of the csv profile:** the RHI's own draw calls by pass category. They agree with
  the dump rows for the base pass, prepass and shadows (37 / 15 / 10 after the cuts). The RHI's `RenderVelocities`
  category (8) counts more draws than the opaque `Velocity` rows (3), and it moves with them (13 uncut).

**The stills.** These were taken with the creature-import capture (`-MageArenaCreatureImportCapture`, 17 shots at 8 m
under the T56 dusk), once per variant: uncut, after, shadow off, LOD1 forced and LOD2 forced (`scripts/run-stills.ps1`).

- `scripts/compare.py` measures the pixel change of each variant against the after stills.
- It writes A/B crop pairs (2x) and full frames, in a shuffled order recorded in `critic/blind-key.json`, for three
  shots: the cast (16), the maw front (01) and the hound front (09).
- One fresh blind critic per variant judged its three pairs from the images alone. It was told that idle-pose noise is
  expected, and nothing about which image was which or what had changed.

## 2. The draws per creature and pass (before the cuts)

From `dc-uncut1` (`-MageArenaCreatureUncut`, the CREATURE-CAST scene in this build). Each cell is the median per dumped
frame of draws / triangles. ShadowDepth is both CSM cascades together.

| Creature | Sections | BasePass | Depth prepass | Velocity | ShadowDepth (2 cascades) | Draws |
|---|---|---|---|---|---|---|
| Brennic (`SK_Brennic`) | 1 | 1 / 23,990 | 1 / 23,990 | 1 / 23,990 | 2 / 47,980 | **5** |
| Cinder hound (`SK_CinderHound`) | 2 | 2 / 11,796 | 2 / 11,796 | 2 / 11,796 | 4 / 23,592 | **10** |
| Mire maw (`SK_MireMaw`) | 2 | 2 / 17,752 | 2 / 17,752 | 2 / 17,752 | 4 / 35,504 | **10** |
| **Cast** | 5 | 5 / 53,538 | 5 / 53,538 | 5 / 53,538 | 10 / 107,076 | **25** |

- No other pass draws a creature: no translucency, no custom depth, and no occlusion tests
  (`r.AllowOcclusionQueries=False`).
- Mesh draw command rows per pass, the scene as a whole (median of the 6 dumped frames):

| Run | BasePass | DepthPass | ShadowDepth | Velocity | TranslucencyAfterDOF | TranslucentVelocity | RHI draw calls median / max |
|---|---|---|---|---|---|---|---|
| dc-without1 (no creatures) | 34 | 12 | 4 | 3 | 38 | 6 | 109 / 113 |
| dc-uncut1 (the cast, uncut) | 39 | 16.5 | 14 | 8 | 38 | 6 | 133 / 136 |
| dc-with1 (the cast, after the cuts) | 37 | 15 | 10 | 3 | 38 | 6 | 121 / 124 |

- **The RHI total moved by +24 / +23 with the uncut cast.** In the CREATURE-CAST build it moved +25 / +24, with the cast
  at 134 / 137. The difference is run noise of one call, and the 25 mesh draws are exact.

## 3. The levers, one at a time

Each lever ran alone on top of the uncut cast (`-MageArenaCreatureUncut` plus one switch), one run each. "vs uncut" is
the change against `dc-uncut1` (133 / 136).

| Run | Lever | Draw calls median / max | vs uncut | Per creature (Brennic / hound / maw draws) | BasePass tris | ShadowDepth | Velocity tris | Verdict |
|---|---|---|---|---|---|---|---|---|
| dc-uncut1 | none | 133 / 136 | - | 5 / 10 / 10 | 190,546 | 138,612 | 134,234 | the before row |
| dc-merge1 | (a) section merge | 124 / 128 | **-9 / -8** | 5 / 5 / 5 | 191,594 | 138,612 | 134,234 | **kept** (critic SAME) |
| dc-shadowoff1 | (b) shadow off | 124 / 127 | -9 / -9 | 3 / 6 / 6 | 190,546 | 31,536 | 134,234 | **rejected**: VISIBLE LOSS |
| dc-inset1 | (b) inset shadow | 135 / 139 | +2 / +3 | 4 / 8 / 8 (+ projection) | 191,674 | 85,074 | 134,002 | **rejected**: costs more |
| dc-velocity1 | (c) per-bone velocity off | 129 / 132 | **-4 / -4** | 4 / 8 / 8 | 191,594 | 138,612 | 80,696 | **kept** (critic SAME) |
| dc-lods1 | (d) LODs generated, picked by screen size | 134 / 137 | +1 / +1 (noise) | 5 / 10 / 10, all LOD0 | 191,594 | 138,612 | 134,234 | **kept**: LOD0 at 8 m, by design |
| dc-lod11 | (d) LOD1 forced | 134 / 137 | +1 / +1 | 5 / 10 / 10 at LOD1 | 163,180 | 85,064 | 107,460 | for the far ring; MINOR at 8 m |
| dc-lod21 | (d) LOD2 forced | 133 / 136 | 0 / 0 | 5 / 10 / 10 at LOD2 | 150,388 | 58,296 | 94,076 | past the far ring; MINOR at 8 m |

### 3.1 (a) The section merge: in UE, real, and SAME

- **How it works.** `CreatureImport::PrepareMesh` runs on the loaded asset, in memory, before the creature spawns.
  - It sets every polygon group of the LOD0 mesh description to the largest group's material slot name. The tongue's
    own corners first take a body texel's UV.
  - It commits the description and rebuilds the mesh.
  - The build matches faces to material slots by name (`FLODUtilities::AdjustImportDataFaceMaterialIndex`), so the
    whole mesh lands in one slot and one section.
- **No topology changes.** Polygons, vertices and normals are untouched.
- **Why it relabels instead of moving polygons.** `FMeshDescription`'s polygon-group mutators live in the
  `MeshDescription` module, which `MageArenaVR.Build.cs` does not link. That file is outside this card's paths
  (section 6), so the merge uses only the description's inline accessors.
- **The hound.** Its two slots (`M_CinderHound`, `M_CinderHound_Ember`) both took `M_CinderHound`. They fold with no UV
  moving (`slots=2->1 moved=205`).
- **The maw.** Its tongue capsule (192 triangles, 576 corners, none shared with the body) had its own flat material,
  `M_MireMaw_Flat`, linear (0.20, 0.012, 0.012), sRGB (124, 29, 29).
  - Its UVs now all point at texel (571, 658) of `T_MireMaw_BaseColor.png`, sRGB (112, 44, 46).
  - That is the closest patch on the bake whose every texel within 8 px stays within 0.102 (RGB distance, 0-1) of the
    flat colour (`scripts/tongue_texel.py`). The 8 px radius keeps it on the colour through mip 3.
  - The tongue is a slightly duller red than its flat material.
- **The result in the render data:** one section each (`lod0=... tris/1 sections` in `MAGEVR_CREATURE_BUDGET`). The dump
  rows draw segment 0 only, so the hound and the maw cost 5 draws each instead of 10.
- **The critic** (uncut against after, which differ by the merge and the velocity cut): **SAME on all three pairs.**
  - "The teeth, the tongue's red shading, the orange eye and the dark inside of the mouth all match."
  - The pixel change against the uncut stills is idle-pose noise. On the maw front, 1,982 of 41,645 subject pixels
    change by more than 0.04, all on the edges of the head and teeth (`critic/diff-uncut-01.png`). The maw's back
    changes by 17 px.
- **A section merge in Blender is not needed.** The expected delta (-5 per two-slot creature) is what UE measured.

### 3.2 (b) The shadow policy: the cascades stay

- **Shadow off** saves 9 / 9 calls: 2 cascades x 5 sections, less 1 of noise. ShadowDepth falls back to the
  no-creature 31,536.
  - **Blind critic: VISIBLE LOSS** on the cast and the hound.
  - "Brennic's shadow is the clearest grounding cue in the frame. Without it, figures float." On the hound: "the paws
    barely seem to touch the ground."
  - On the low maw it was SAME or MINOR: its body hides its shadow.
  - The pixel change is 11,670 subject pixels on the cast, against 3,498 for the uncut/after pose noise.
- **Inset (per-object) shadows** draw 1 shadow depth per section instead of 2. The run still costs **+2 / +3 calls**:
  each per-object shadow adds its own projection, which the cascades do not need. On Quest (mobile) an inset caster
  draws no shadow at all without `r.Mobile.EnableInsetShadows`, so it would lose the shadow there. Rejected.
- **A distance cutoff** (`ShadowMaxDistanceM`) is in place as a lever, default 0 (cast at any distance).
  - Every creature in the budget scene stands at 8 m, where turning off is the visible loss above.
  - The far ring (15 m) was not shot, so no cutoff is claimed (section 6).
  - The far cascade alone cannot be targeted per primitive: UE has no per-cascade caster flag, and `bCastFarShadow`
    only covers the distance cascades beyond the dynamic shadow distance.

### 3.3 (c) The velocity pass: per-bone off

- **Why the velocity draws are waste.** The project renders MSAA 4x (`r.AntiAliasingMethod=3`), so no TAA or TSR reads
  the motion vectors. Quest draws velocity only for TAA or space-warp motion vectors (`FVelocityMeshProcessor`:
  "Only enabled on mobile when TAA is enabled or we're rendering OpenXR motion vectors").
- **Why per-bone matters.** A skinned mesh with `bPerBoneMotionBlur` is "always has velocity" (`bAlwaysHasVelocity`),
  so it draws a velocity pass every frame while it idles in place.
- **With it off**, a creature draws velocity only on a frame its transform moves. Standing at its spot it draws none:
  **-4 / -4 calls**, and the velocity pass falls from 134,234 to 80,696 triangles.
- **What is lost.** Per-bone vectors for desktop motion blur on the idle animation. That cannot show in a still, and the
  stills turn motion blur off (`r.MotionBlurQuality 0`).
- **The critic** saw the uncut/after pair as SAME.
- If Quest space warp is ever turned on, an animated creature would want its per-bone vectors back (section 6).

### 3.4 (d) The LODs: past the near ring

`PrepareMesh` adds LOD1 (50 % of LOD0's triangles) and LOD2 (25 %) with the engine's skeletal reduction (the
`SkeletalMeshReduction` plugin, which loads in `-game`), then rebuilds.

- The screen sizes are chosen so that, for the seated Quest 3 eye, LOD1 takes over at 12 m and LOD2 at 20 m. The
  formula is `ScreenSizeAt`: the engine's bounds diameter over the view, with Quest 3's projection multiple of 0.45.
  - Near ring 8 m: LOD0.
  - Far ring 15 m: LOD1.
  - Beyond 20 m: LOD2.
- On the desktop's 90 deg view (multiple 0.889) the same thresholds land at about 24 m and 40 m, so the D-G4 scene at
  8 m draws LOD0 (`dc-lods1`: all rows LOD 0).

| Creature | Bounds sphere | LOD0 tris | LOD1 tris (screen size) | LOD2 tris (screen size) | Screen size at its 8 m spot, Quest / desktop | Draws per LOD |
|---|---|---|---|---|---|---|
| Brennic | 105.3 cm | 23,990 | 11,994 (0.079) | 5,996 (0.047) | 0.117 / 0.231 | same: sections do not change with the LOD |
| Cinder hound | 155.9 cm | 11,796 | 5,896 (0.117) | 2,948 (0.070) | 0.173 / 0.341 | same |
| Mire maw | 174.7 cm | 17,752 | 8,874 (0.131) | 4,436 (0.079) | 0.176 / 0.347 | same |

Forced on the whole cast at 8 m (`dc-lod11`, `dc-lod21`; uncut sections):

- LOD1 cuts BasePass by 27.4k (190.5k -> 163.2k) and ShadowDepth by 53.5k.
- LOD2 cuts BasePass by 40.2k and ShadowDepth by 80.3k.
- Neither changes the draw calls.
- **The critics:**
  - LOD1 at 8 m: **MINOR**. "In B the bumpy texture on the upper back and left flank reads a little softer", on the maw
    only; Brennic and the hound SAME.
  - LOD2 at 8 m: **MINOR, leaning toward a loss** on the maw. "A loses fine surface relief and outline bumpiness on the
    toad."
- So LOD0 stays at the duel distance, and the LODs are kept for the far ring and beyond. There the creature is half
  the size on screen or less, which this card did not shoot.

## 4. D-G4 before and after

All rows except the first ran in the same build, with the same settings as CREATURE-CAST 2.6. "All passes" is the sum
of the dumped mesh draw command triangles.

| Run | Draw calls median / max | BasePass tris median / max | Depth prepass | ShadowDepth | Velocity | All passes median / max | Live particles median / max | LOD0 estimate median (of which skeletal) | Peak working set | GPU ms median |
|---|---|---|---|---|---|---|---|---|---|---|
| **before**: CREATURE-CAST with1 (all three, its build) | **134 / 137** | 191,146 / 194,978 | 191,146 | 138,612 | 134,018 | 669,714 / 681,552 | 1,393 / 1,468 | 146,9xx (skinned uncounted) | 2,847 MB | 0.88 |
| dc-without1 (no creatures, this build) | 109 / 113 | 138,056 / 139,664 | 138,056 | 31,536 | 80,696 | 403,996 / 408,806 | 1,397 / 1,468 | 146,882 (0) | 3,311 MB | 0.85 |
| dc-uncut1 (all three, uncut, this build) | 133 / 136 | 190,546 / 193,202 | 190,546 | 138,612 | 134,234 | 668,528 / 676,492 | 1,394 / 1,467 | 200,420 (53,538) | 2,941 MB | 0.88 |
| **after: dc-with1** (all three, the cuts) | **121 / 124** | 190,546 / 193,202 | 190,546 | 138,612 | 80,696 | 615,008 / 622,978 | 1,396 / 1,467 | 200,372 (53,538) | 2,849 MB | 0.87 |
| **after: dc-with2** | **121 / 124** | 191,586 / 193,202 | 191,586 | 138,612 | 80,464 | 617,218 / 622,726 | 1,397 / 1,467 | 200,404 (53,538) | 3,084 MB | 0.89 |
| **Limit** (PROJECT-PLAN section 7) | **<=150** | **<=350,000** | | | | | **<=2,000** | | | |

- **Draw calls: the cast now adds +12 / +11 over the baseline** (109 -> 121 median, 113 -> 124 max), against +25 / +24
  before.
  - The max fell by 13 from the before row (137 -> 124).
  - The 12 are exact: 3 creatures x 1 section x (base, prepass, 2 cascades). The dump rows of the after run are
    1 / 1 / 0 / 2 per creature.
  - A smoke run of the same defaults earlier in the build read 121 / 125.
- **BasePass: flat** (190.5-191.6k, 55 % of 350k): the three LOD0s, 53,538 over the baseline.
  - The cuts are draw-call cuts. The only BasePass lever at 8 m is LOD1 at the near ring (-27.4k), read MINOR, so it is
    not a default (section 6).
- **ShadowDepth: unchanged at 138,612** (the cast in both cascades). It is logged, not gated.
- **Velocity: -53.5k** (134k -> 80.7k, back to the baseline).
- **All passes: -51 to -54k.**
- **The new skeletal field reads 53,538**: Brennic 23,990 + hound 11,796 + maw 17,752, all in the frustum.
  `lod0InFrustumTriangles` minus it is 146.9k, the old static-only figure. The baseline row reads 0.
- **Live particles, working set and GPU time** moved within run-to-run noise. GPU time is 0.85-0.89 ms; the working set
  spreads 2.8-3.4 GB across all runs, creatures or not.
- **Texture and mesh memory.** The textures are unchanged (+8.07 MB, CREATURE-CAST). The LODs add 75 % to the three
  meshes' vertex data, about +1.4 MB over CREATURE-CAST's +1.90 MB of mesh resources (estimated from the triangle
  shares, not measured).

### 4.1 Headroom for a fourth and a fifth creature

- **The after max is 124: 26 calls under the 150 limit, 21 under the 145 flag.**
- A creature imported under these defaults costs **4 calls** at the near ring, whatever its slot count: one section x
  base, prepass and two cascades.
  - A fourth creature lands at about **128**, a fifth at about **132**.
  - That leaves 18 calls for the wave and crowd plans' other bodies.
- **BasePass** grows by the new creature's LOD0 triangles at the near ring. Two more creatures of about 18k each would
  reach about 227k of 350k.
  - At the far ring the LODs halve that, and ShadowDepth halves with it.
- Without the merge, a two-slot creature costs 8 calls. Without per-bone velocity off, an idling creature costs 1 more
  per section.

## 5. Gates

| Gate | Result |
|---|---|
| `build.ps1` | build 1 `Result: Failed` (link: `MeshDescription` not linked, section 1); builds 2 and 3 `Result: Succeeded` |
| `Automation RunTests MageArena.` (`-nullrhi`; sigil corpus regenerated first: `generate.mjs --corpus 30 --seed 1000`, then `mousepaths.mjs`) | **found 298 (295 + 3 new), 298 `Result={Success}`, `TEST COMPLETE. EXIT CODE: 0`**, on build 3 |
| New tests | `MageArena.Budget.Triangles.Skeletal` (two skinned meshes ahead count, one behind and one hidden do not; static share unchanged; both call sites; a forced LOD still counts LOD0), `MageArena.CreatureImport.SectionMerge` (the merge plan: target, moved triangles, the tongue's own corners, a shared corner left), `MageArena.CreatureImport.BudgetPolicy` (the defaults, the uncut set, the ring distances, the screen-size formula) |
| `node apps/vr/tools/lint/unused-members.mjs` / `format-check.mjs` | OK (599 members / 10 changed C++ files) |
| the merge gate's test line (agy-guard, check-pin and self-test, check-overlay and self-test, leonardo-redaction, repo-law) | each exit 0 |

- `MageArenaDesign` was not run: no combat path changed.
- The skeletal test was not run red on the old code. With the old count it reads 0 skinned triangles against the
  expected 2 x the cube's.

## 6. Nothing ships

- **The DINOv3 hold stands.** No mesh, FBX, texture, `.uasset` or still from this lane is committed.
  - The scratch folder was deleted before any commit.
  - The cuts are code: `CreatureImport::FCreatureBudget` and `PrepareMesh`. They change the loaded asset in memory and
    never save it.
- Staging: `C:/Users/kazda/mage-art-staging/drawcall-budget-2026-10-25/`.
  - `budget/`: 12 runs (the 11 of record and the smoke run).
  - `stills/`: 5 variants x 17 shots.
  - `critic/`: pairs, key, `pixel-diff.json` and the critics' verdicts as quoted here.
  - `scripts/`, `logs/`, `tests/`.
- No ASSET-LEDGER row, no pinned data touched, no image generated, no paid call.

## 7. Open questions

- **LOD1 at the near ring.** Starting LOD1 at 8 m would cut BasePass by 27.4k (190.5k -> 163.2k) and ShadowDepth by
  53.5k. The blind critic read it as MINOR: a slightly softer maw back, with Brennic and the hound SAME. The default
  keeps LOD0 at the duel distance. It is Athena's call, if the triangle budget, rather than the draw calls, becomes
  the limit.
- **The cuts in a cooked build.** `PrepareMesh` runs in the editor binary, which is what the desktop core uses. A
  cooked Quest build needs the same cut baked at import.
  - `Tools/ImportCreatureMeshCommandlet.cpp` should call `CreatureImport::PrepareMesh` before it saves. That file is
    outside this card's paths.
  - A real polygon-group merge (rather than the slot relabel) would need `MeshDescription` in `MageArenaVR.Build.cs`.
    That file is also outside the paths, and the relabel already gives one section.
- **A shadow cutoff for the far ring.** At 15 m, shadow off would save 2 calls per merged creature. It was not shot.
  A 15 m still pair (`-MageArenaCreatureDistanceM=15`, shadow on and off) and a critic would settle it.
- **The tongue colour.** The merged tongue samples sRGB (112, 44, 46) instead of its flat (124, 29, 29). The critic saw
  no difference. An exact match would need a tongue-coloured patch painted into the bake, which is a texture change,
  not done here.
- **Space warp.** If Quest Application SpaceWarp is turned on, an animated creature needs per-bone motion vectors back
  (`-MageArenaCreatureVelocity=1`, or a per-creature default). That brings back 1 velocity draw per section.
