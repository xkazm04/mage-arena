---
title: T63 the spell flipbook pass - eye-facing flipbook cards on the shared glow layer, at T58's call cost
channel: vr
date: 2026-10-25
status: RAN - desktop proxy, not Quest truth. Code commits 06c73a6..a85a6a7 (atlas 5ebd656). Every T58 glow puff is now an eye-facing card that shows one cell of a procedural 512x512 flipbook atlas, chosen per instance through custom data. Water has spray sheets, droplet clusters and a splash crown, Fire has upright flame licks with a darker ember rim, embers and a flame star, and Air has swirls, wisps and a gust. Each glow colour stays one instanced draw. D-G4 is 122 / 126 in all three before runs and in all six after runs (+0). Translucent triangles fell from 406k to 23k, and live particles plus glow puffs stay at 1,469 + 400. MageArena. passed 328/328 (325 at the base plus 3 new tests), and MageArenaDesign is red only on FireSeated and FullSeated (DF-011). In the final blind critic round the after frame won all 9 school pairs (7 VISIBLE GAIN, 2 MINOR), with no class break and no glow brighter than its threat. Nothing ships: the DINOv3 hold stands.
sources: [docs/DECISIONS.md (2026-10-25 Athena on b872dcd), apps/vr/tasks/T63-spell-flipbook-pass.md, docs/research/SPELL-VFX-T58-2026-10-25.md, docs/concepts/VISUAL-PLAN.md (b) shortfall 3 and the arena order, (c) style block, apps/vr/art/key-art/key-art-s5101-medium.jpg, docs/research/CROWD-T57-2026-10-25.md section 6]
---

# T63 the spell flipbook pass, 2026-10-25

**Label: desktop proxy (D3D12, forward shading, `-nohmd`), not Quest truth.** Engine UE 5.8.3 (CL 58210709, read from
`Build.version` before the first build). No combat number moved: nothing under `apps/vr/data/pinned/`, `PINNED.json`,
`apps/vr/data/conformance/` or the overlay was touched. The kernel, bout difficulty, DF-011 and DF-013 were not touched.
`SpellVfx::GlowColour`, `RimPulse`, `RimPulseColour` and `Greybox/BrazierFire.*` are unchanged. The braziers and the
crowd tint were not touched.

**Outcome.** The T58 critics' S1 gap was "soft spheres": no texture, a sparse and oversized spray, and fire washed out on
the sand. Every spell effect now draws textured shapes instead:

- **Water:** the bolt's shell is a noisy sheet of water hugging the core, with three curling crests and a ring of fine
  drops. The trail puffs each show a run of fine drops, so the spray is denser without more puffs. The absorb is a solid
  splash crown with fingers and flung drops, and the perfect ring is a braid of droplet streaks.
- **Fire:** the shell is upright flame licks over the core, with a darker ember rim along every edge (contrast on the
  orange sand). Its interior is layered by noise, and it flickers between two cells and their mirrors at 10 Hz. The
  trail is ember clusters, the absorb a flame star with spark streaks, and the perfect ring a chain of spark streaks.
- **Air:** the shell is five wisps spiralling round the core, the trail is long wavy wisps, the absorb a gust that
  unwinds into wisps, and the ring a braid of wisps.
- **Unblockable:** its shell shows the SoftGlow cell, T58's far-hemisphere falloff baked into the atlas. The rim and its
  pulse are untouched code. **Steel** still never glows.

**Cost.** Nothing new is drawn. The puff count, the 500 cap and the colour grouping are T58's. Only the mesh (a 2-triangle
card in place of the engine sphere), the material and the per-instance custom data changed. D-G4: **+0 draw calls** (max
126 of 150 before and after), and the translucent pass fell from 406k to 23k triangles.

The evidence is in `C:/Users/kazda/mage-art-staging/spell-flipbook-t63-2026-10-25/` (`<out>/` below):

- the before stills (`T63-before/`) and the after stills of each critic round (`T63-after-r1..r3/`);
- the three critic rounds (`critic-N/`, with each key kept apart in `critic-N-key/blind-key.json`);
- the nine D-G4 summaries (`budget/`), the atlas previews over the sand (`preview/`), and every build, commandlet,
  capture, budget and suite log (`logs/`).

The only binaries in git are the atlas PNG (91 KB) and two small `.uasset` files, all three in LFS.

## 1. How it was run

At most one Unreal process ran at any time: builds, commandlet, captures, budget runs and suites ran strictly one after
another. Blender and ComfyUI stayed down. No paid service and no image generation was used.

1. The MetaXR plugins were copied in with `robocopy /E`. The sigil corpus was regenerated (`generate.mjs --corpus 30
   --seed 1000`, then `mousepaths.mjs`), and `git status` stayed clean.
2. **Baseline build** of `3aabf27` (master `cda7f58` plus the card): `Result: Succeeded` (931.72 s cold).
3. **Before stills** on the baseline binary: `capture-spell-vfx.ps1 -Run T63-before` printed `MAGEVR_CAPTURE_DONE
   shots=11` and `capture-spell-vfx: OK`. The before is the real T58 build, not a switch.
4. **D-G4 before:** three runs on the baseline binary (section 4), ending `budget: ok (3 runs)`.
5. **The T63 builds:** three, each `Result: Succeeded` (22.5 s, 14.8 s, 24.5 s), and none failed to compile. The second
   and third carried the look changes of section 3 (crown and ring card sizes).
6. `create-budget-fx.ps1 -SpellVfx` (rerun after each atlas revision):
   `MAGEVR_BUDGET_FX T_SpellFlipbook 512x512 srgb=0 mipgen=0 group=13 ...` and
   `MAGEVR_BUDGET_FX M_SpellGlow blend=translucent shading=unlit twosided=1 instanced=1 flipbook=T_SpellFlipbook customdata=3`.
   The six Niagara systems reported `unchanged`.
7. **The suites,** on the final binary and the final assets (the tree of `a85a6a7`), both `-nullrhi`:
   - `Automation RunTests MageArena.`: `Found 328 automation tests based on 'MageArena.'`, 328 x `Result={Success}`,
     `TEST COMPLETE. EXIT CODE: 0`. It includes `MageArena.SpellVfx.ColoursKeepThreatClass`, `OwnersGlowTheirSchool` and
     `PuffsStayInBudget`, plus the three new tests `FlipbookKeepsOneLayerPerColour`, `FlipbookCardsFaceTheEye` and
     `FlipbookAtlasIsQuestSane`, all `Success`. The brief's reference count was 306; the tree at `cda7f58` already had
     325 (328 minus the 3 new tests), and T63 adds 3. A mid-run suite on the first atlas was also 328/328.
   - `Automation RunTests MageArenaDesign.`: `Found 7`, five `Success`; `Result={Fail}` only for
     `MageArenaDesign.Duel.FireSeated` and `MageArenaDesign.Session.FullSeated` (the two DF-011 reds; `EXIT CODE: -1`
     from them). No other red.
8. **D-G4 after:** three runs on the round-2 atlas and three on the final atlas (section 4).
9. **The blind critic:** three rounds (section 5).

The creature `.uasset` files were copied into `Content/CreatureImportScratch` for each D-G4 set and deleted straight
after, before every suite and every commit.

**The new tests and red-first.** The three new tests exercise new API (`ECell`, `CardTransform`, `FGlowFrame`, the atlas
asset), so they cannot compile against the baseline. They were green on their first run. They were not run red first.

## 2. What changed (file:line, at a85a6a7)

| Change | Where |
|---|---|
| The atlas generator: 16 cells drawn from signed distances, strokes, drops and sparks; `--write`, `--check` (re-renders and compares pixels) and `--preview` | `apps/vr/tools/vfx/spell-flipbook.mjs` |
| The atlas (512x512 RGBA, R=G=B shade, A opacity) and its manifest (cell table, pixel sha256 `976be383...`) | `apps/vr/art/vfx/spell-flipbook.png` (LFS), `apps/vr/art/vfx/manifest.json` |
| The cells, the flicker rate, the card fields of a puff | `Session/SpellVfx.h:41` (`ECell`), `:64` (`FlipbookFps` 10), `:72` (`FPuff`: `Cell`, `bMirror`, `bStreak`, `Roll`) |
| The custom data (U scale, U offset, V offset), the card transform, the card oversize | `Session/SpellVfx.h:106`, `:117`; `SpellVfx.cpp:283` (`CardCustomData`), `:304` (`CardTransform`) |
| One frame of puffs grouped by colour, cards facing the eye, the 500 cap | `Session/SpellVfx.h:123`; `SpellVfx.cpp:342` (`Reset`), `:353` (`Add`) |
| The cell choice per puff: shell flicker and roll, trail clusters, crowns, streaks | `Session/SpellVfx.cpp:53` (`ShellFrame`), `:59` (`ShellCell`), `:140` (`BoltPuffs`), `:217` (`SplashPuffs`), `:259` (`PerfectRingPuffs`) |
| Card size scales for clusters, crowns and ring beads | `Session/SpellVfx.h:161`, `:163`, `:165` |
| The presentation: reset with the eye, queue, plane layers with 3 custom floats | `Session/SessionPresentation.cpp:1407`, `:1656`, `:2444`, `:2465`, `:2470`, `:2486`; member `Session/SessionPresentation.h` (`SpellVfx::FGlowFrame GlowFrame`) |
| `T_SpellFlipbook` import and the new `M_SpellGlow` | `Tools/CreateBudgetFxCommandlet.cpp:252` (`ImportSpellFlipbook`), `:315` (`CreateSpellGlowMaterial`); `apps/vr/tools/create-budget-fx.ps1`; assets `apps/vr/Game/Content/Vfx/T_SpellFlipbook.uasset`, `M_SpellGlow.uasset` (LFS) |
| Tests | `Tests/SpellVfxTests.cpp:246` (FlipbookKeepsOneLayerPerColour), `:342` (FlipbookCardsFaceTheEye), `:409` (FlipbookAtlasIsQuestSane) |

**The material** (`M_SpellGlow`): unlit, translucent (alpha blend), two-sided, `bUsedWithInstancedStaticMeshes`.
`UV = (u x d0 + d1, v / 4 + d2)` from three `PerInstanceCustomData` floats. `Emissive = Color x tex.R` and
`Opacity = saturate(tex.A x GlowGain 1.0)`. The shade is at most 1, so a pixel blends toward the threat colour or a
darker tone of the same chroma, and never past it or to another hue. The T58 rule (alpha blend, not additive) holds.
Forward shading, the tonemapper and the exposure are untouched.

**The atlas** (`T_SpellFlipbook`): 512x512, linear (sRGB off), mips from the Effects group, default compression,
`MaxTextureSize` 512. Every cell keeps its content inside 0.9 of the cell, and its border is clear. The generator and
`FlipbookAtlasIsQuestSane` both check this, so a card never shows its square.

| Row | Col 0 | Col 1 | Col 2 | Col 3 |
|---|---|---|---|---|
| Water | shell A | shell B | spray (trail) | crown (absorb) |
| Fire | flame A | flame B | embers (trail) | flame star (absorb) |
| Air | swirl A | swirl B | wisp (trail, streaks) | gust (absorb) |
| Shared | soft glow (unblockable, T58 falloff) | droplet streak (water/earth splash and ring) | spark streak (fire splash and ring) | grit (earth) |

## 3. Decisions the card did not specify

1. **A camera-facing card, not a sub-UV on the sphere.** The engine sphere's UVs are a lat-long wrap, so a flipbook on
   them would smear at the poles. Projecting a view-space UV onto the sphere in the material needs each instance's
   centre and radius, and on an instanced component that is extra material math per pixel. A card is simpler and
   carries more:
   - its 0..1 UV maps straight to a cell;
   - its 2 triangles replace the sphere's (D-G4's translucent pass went from 406k to 23k triangles);
   - its orientation is computed on the CPU in a pure, testable function (`CardTransform`).
   It faces the eye through the puff's centre, so for any pixel inside a bolt's silhouette the near half of the opaque
   core is in front of the card. The core keeps its exact threat colour, the property T58's far-hemisphere material
   existed for. Upright cards keep the atlas top up, so flames lick upward. A streak lies along its flight as seen, and
   shortens toward round head-on, as the sphere did.
2. **The frame comes from per-instance custom data, not from material time.** The capture stills are posed, so a frame
   picked by material time would differ from one run to the next. CPU-picked cells are deterministic for a given time
   and seed, and testable. The cell is three floats per instance, so all cells of a colour share one material instance
   and one layer: **one draw call per glow colour**, as T58 (`FlipbookKeepsOneLayerPerColour`, D-G4 `glowLayers` 2).
3. **One atlas for all schools, not one sheet per school.** A sheet per school would need a texture per layer, and a
   layer is keyed by colour, not by school, or three samplers in one material. One 512x512 atlas keeps one material and
   one sample. Each school's "sheet" is a 4-cell row (128 px cells), within the brief's 512 limit.
4. **The atlas is authored in Node, imported by the commandlet.** The generator writes a PNG, so the look can be
   iterated and previewed over the sand without Unreal. `--check` proves determinism on the decoded pixels (zlib bytes
   may differ between Node versions), and the commandlet imports the PNG through the editor's own importer (the T57
   crowd pattern).
5. **No thin film over the sand.** Round-1 renders showed the problem: a part-transparent teal over the orange sand
   mixes toward grey, and green toward khaki (complementary colours). Every school cell is therefore crisp and mostly
   opaque. The soft falloff survives only in the unblockable's SoftGlow cell, unchanged from T58.
6. **The darker rim is a shade, not a hue.** Fire contrast on the sand comes from a darker band along every edge (shade
   0.5, then 0.62 mid, then 0.72-1.0 noise-streaked inside). This is the same chroma at lower luminance, so it stays in
   the fire class. A shade of 0.35 read "slightly brown" to the round-2 critic (a look note, not a class break), so the
   final atlas lifts it to 0.5. Round 3 still notes a brownish fire edge as a look note (section 5).
7. **Card scales.** A cell's content does not fill its card the way a sphere filled its puff, so three constants keep the
   on-screen footprint near T58's without more puffs:
   - `CardOversize` 1/0.9, for the clear border;
   - `ClusterCardScale` 1.5, for the trail clusters;
   - `CrownCardScale` 1.4 and `RingCardScale` 1.7, for the absorb crown and the ring beads.
   The puff count and the 500 cap are T58's.
8. **The report is this doc,** not `runs/T63/`: `runs/` is never committed (repo law).

## 4. D-G4 before and after

`capture-budget.ps1 -Runs 3 -ExtraArgs -MageArenaCreatureImportScene` used the T58/T62 settings: 20 enemies, 100
projectiles, both hands, 6 Niagara systems and the three-creature cast, a 10 s warm-up, then 60 s of samples with 6
dumps a run, offscreen at 1920x1080. Every run printed `ok=True done=True`, the scene parity was ok (planned hash
`855ac9ba...`), and each set ended `budget: ok (3 runs)`.

| Run | Draw calls median / max | BasePass tris (dump) median / max | Translucency tris (dump) median / max | Live particles median / max | Glow puffs | Glow layers |
|---|---|---|---|---|---|---|
| before 1 (baseline 3aabf27) | 122 / 126 | 193,944 / 195,504 | 406,323 / 406,338 | 1,393 / 1,467 | 400 | 2 |
| before 2 | 122 / 126 | 193,832 / 195,552 | 406,318 / 406,330 | 1,393 / 1,468 | 400 | 2 |
| before 3 | 122 / 126 | 193,944 / 195,552 | 406,318 / 406,340 | 1,394 / 1,469 | 400 | 2 |
| after 1 (round-2 atlas) | 122 / 126 | 192,896 / 195,552 | 23,132 / 23,146 | 1,394 / 1,468 | 400 | 2 |
| after 2 | 122 / 126 | 193,944 / 195,456 | 23,003 / 23,128 | 1,392 / 1,467 | 400 | 2 |
| after 3 | 122 / 126 | 191,768 / 194,256 | 23,020 / 23,042 | 1,395 / 1,467 | 400 | 2 |
| **final 1** (final atlas, the a85a6a7 tree) | **122 / 126** | 192,896 / 195,552 | 23,133 / 23,146 | 1,398 / 1,468 | 400 | 2 |
| **final 2** | **122 / 126** | 192,896 / 195,552 | 23,133 / 23,144 | 1,397 / 1,469 | 400 | 2 |
| **final 3** | **122 / 126** | 192,896 / 195,552 | 23,138 / 23,144 | 1,399 / 1,469 | 400 | 2 |
| Limit | <=126 (Athena's ceiling; +1 at most) | <=350,000 | | particles + puffs <=2,000 | | |

- **Draw calls: +0.** The max is 126 in every run, before and after, against Athena's ceiling of 126 and the
  150 budget. Headroom stays 24 calls.
- **Visible triangles:** BasePass is flat at 192-194k median (a puff never draws in the base pass). The translucent
  pass fell by 383k triangles: 400 cards at 2 triangles replace 400 engine spheres.
- **Particles:** live Niagara is unchanged (max 1,469), and the 400 glow puffs sit under the 500 cap. Counted together,
  that is **1,869 of 2,000**, the same as T58's 1,868 within run noise.
- **Glow layers:** exactly 2 in every sample of every run (hound fire and maw water), with the flipbook on.

## 5. The stills and the blind critic

The stills come from `capture-spell-vfx.ps1`: water, fire and air bolts, each with an absorb and a perfect absorb; the
unblockable telegraph; and the steel stone. They were captured before (the baseline binary) and after (the T63 binary).
The after manifests record commit `3aabf27`, because the code was not yet committed when they were captured; the code
in the binary is that of `06c73a6..a85a6a7`.

Each round's pair image shows both full frames at half size, plus a 1:1 crop of the effect region (the box of the
before/after difference). The pairs were shuffled with seeds 6301, 6302 and 6303, and A and B were swapped at random.
The keys sit outside the critic's folder. Each round used a fresh general-purpose agent that saw only the images and
S1, and never saw code or a key. Rounds 1 and 2 drove the fixes in section 3; round 3 judged the final atlas.

**Round 3 (final), unblinded:**

| Shot | Closer to S1 | Strength | Colour class (critic) | Glow brighter than threat |
|---|---|---|---|---|
| 01 water bolt | after | MINOR | turquoise; "wispy fringe darkens slightly toward smoke-teal at its edges" (look note) | none |
| 02 water absorb | after | VISIBLE GAIN | turquoise, none | none |
| 03 water perfect | after | VISIBLE GAIN | "a bit darker and shaded, but still turquoise, not grey" (look note) | none |
| 04 fire bolt | after | VISIBLE GAIN | ember orange, "no yellow, no smoke"; the fringe "edges toward brown at the rim" (look note) | none |
| 05 fire absorb | after | VISIBLE GAIN | orange, "no yellow"; slightly brownish mottling (look note) | none |
| 06 fire perfect | after | MINOR | darker amber, near-brown edges, low contrast on the sand (look note) | none |
| 07 air bolt | after | VISIBLE GAIN | green, none | none |
| 08 air absorb | after | VISIBLE GAIN | "same mint green, no lime" | none |
| 09 air perfect | after | VISIBLE GAIN | "mint green, not lime"; thinner strands, weaker outline at distance (look note) | none |
| 10 unblockable telegraph | SAME (no water/fire content) | MINOR | "the black core holds in both"; the before rim read wider and brighter | none |
| 11 steel stone | SAME | SAME | steel, no glow in either | none |

- **No class break in any round.** No fire drifted to yellow, no air to lime, no black core turned into a red disc, and
  steel never glowed. No glow was brighter than its threat in any round.
- **Rounds 1 and 2:** the after frame won all 9 school pairs in both. Round 1 had 9 VISIBLE GAIN; round 2 had 8 VISIBLE
  GAIN and 1 MINOR (the air bolt). Round 1 named the effects as "one flat shade", so round 2 added the layered interior
  and the darker rim. Round 2 then read a slight brown on the fire edges, so round 3 lifted the rim shade from 0.35 to
  0.5 and thickened the air swirls.
- **The unblockable rim (round 3 pair 10, round 1 pair 09).** T63 changes no rim code: `RimPulse`, `RimPulseColour` and
  the rim placement are untouched (`git diff cda7f58..a85a6a7` has no rim line). The rim pulses in 4 steps at 1.6 Hz on the wall clock, and the capture
  does not pin that clock. A before and an after still therefore catch different pulse steps by chance: round 1 caught
  the after rim on a low step, round 2 caught the same step in both (SAME), and round 3 caught the before rim on a high
  step. Every round read the black core with a red rim intact. This is harness noise, not a T63 change. Pinning the
  pulse phase in the capture is listed in section 6.
- **What all three critics say still falls short of S1:** every effect is an opaque, evenly lit cutout. There is no
  translucent liquid body, no white foam crest or specular drop (ruled out for absorb cues: ElementColour owns
  brightness), no hotter inner core gradient, and no motion read. The rings read as "a wreath", and fire edges toward
  brown at its darker rim.

## 6. Open points for Athena

- **The crowd tint next** (the order Athena named, after this card). The T57 critic's watch item applies to both sides:
  the crowd palette must stay below the ember and rim-red range. T63 made the ember flecks crisper (sparks with a darker
  rim) but not warmer. Their chroma is still `ElementColour`'s ember orange, so a warmer crowd still threatens them.
- **Quest truth.** Every number here is the desktop proxy. On Quest the following are assumed, not measured:
  - the overdraw of 400 alpha-blended cards (smaller than the spheres' in screen area, but not measured);
  - ASTC compression of a mask atlas (the shade channel may band);
  - per-instance custom data on the Vulkan mobile path (the T57 crowd also assumes it).
- **The brazier flame can reuse the flipbook.** The atlas's flame cells and the card path are school-agnostic, and the
  brazier's "spectacle belongs to the flipbook pass" (DECISIONS 2026-10-25 on c69ef865). A brazier flame would need its
  own environment-colour cells, or the same flame cells drawn in `BrazierFire`'s colour. Doing it in a later card costs
  one more glow layer (one draw call) for the brazier colour, unless the braziers stay in their own batch. Their colours
  must not change, and `BrazierFlameNotThreat` must keep holding.
- **Look follow-ups the critics named,** all inside the colour rules and at no draw-call cost:
  - a softer, see-through liquid body (a mid-alpha shade of the same chroma, not over the sand);
  - a few more and finer drops in the crown and the ring;
  - a third flame frame, or a slower flicker;
  - a rim shade between 0.5 and 0.62 if the brown read persists on device.
- **The capture's rim pulse.** `SpellVfxCapture` should pin the pulse phase (or pass a fixed time to `RimPulse` in
  capture runs), so before and after telegraph stills compare like with like. That is a harness change for the next
  card that touches the capture.
- **The six `NS_Budget_*` proxies** still stand in for VFX that T58/T63 draw for real (T58 section 6). That is unchanged
  and still the budget owner's call.
