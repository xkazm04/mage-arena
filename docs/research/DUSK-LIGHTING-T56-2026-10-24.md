# T56 dusk lighting: the greybox arena under key art S1's golden-hour light

**Label: desktop proxy (RTX 4090, D3D12, forward shading, `-nohmd`), not Quest truth.** Card:
`apps/vr/tasks/T56-dusk-lighting.md`. Shortfall 6 of `docs/concepts/VISUAL-PLAN.md` section (b) ("The light is flat and
shadowless"), answered under shortfall group 3 (scene dressing); the target is the approved style block's 'Light' slot
(section c): "default golden-hour dusk, low warm key light, long soft shadows, orange-to-lilac sky". North-star frame:
`apps/vr/art/key-art/key-art-s5101-medium.jpg` (S1).

Code commit: `b82042e` (`T56: light the greybox arena as key art S1's golden-hour dusk`). Engine UE 5.8.3 (CL 58210709).
No combat number moved: `apps/vr/data/pinned/`, `PINNED.json`, conformance, the clip corpus and every overlay file are
untouched; `node apps/vr/tools/check-overlay.mjs` still prints `overlay sha256
600f75c37572affa53d8bd2c8dd41afac6c8e8ec46e2af7efa8b7ab51fed7f07`, the hash the T54 goldens recorded.

## 1. What changed (file:line at `b82042e`)

**Where the light comes from.** The lighting numbers were not in C++: `FArenaLayout::LoadFromFile` reads them from the
`lighting` block of `apps/vr/data/vr/arena-layout.json` (which also said "Shadows stay off. No height fog"). That file is
one of the four files `MageArena.Overlay.Golden` hashes (`Tests/OverlayGoldenTests.cpp:31-32`), so editing it would have
failed the golden gate and moved an overlay byte. The dusk preset therefore lives in code and is applied over the JSON
block, before a style:

| Change | Where |
|---|---|
| New layout fields: sun source angle, shadow distance and cascades, dome band count, height fog (on, colours, density, falloff, start, cutoff) | `Greybox/ArenaLayout.h:166-191` |
| `ApplyDuskLight()`: sun pitch -12 -> **-20** deg, yaw 180 -> **165** (ahead-left of the seated view, behind the opponent), 8 -> **2.4** lux, colour (1.0, 0.48, 0.16) -> **(1.0, 0.50, 0.16)**; shadows **on**, 45 m, 2 cascades, 2 deg source angle; sky fill 1.4 x blue (0.40, 0.50, 0.75) -> **0.8 x rose-lilac (0.62, 0.40, 0.42)**; dome horizon / upper / zenith -> gold (0.98, 0.52, 0.20) / rose (0.66, 0.34, 0.32) / lilac (0.42, 0.27, 0.40) in **5 bands**; fog density 0.035, falloff 0.2, start **20 m**, cutoff **50 m** | `Greybox/ArenaLayout.cpp:356-390` |
| `SkyBandColour()`: band colours horizon -> upper -> zenith | `Greybox/ArenaLayout.cpp:392-397` |
| `LoadFromFile` applies the preset after the JSON lighting block and before `-MageArenaStyle` | `Greybox/ArenaLayout.cpp:539` |
| `ApplyStyle` also greys the fog at its own luminance (as it already did the sun and the sky fill) | `Greybox/ArenaLayout.cpp:345-351` |
| Shadow casters: stands + wall, dais, seats, braziers, banner poles, bars, cloth, marker posts (the greybox helpers make every part shadowless; sand, pads, flames and dome stay so) | `Greybox/ArenaGreyboxActor.cpp:34-46`, `:147`, `:224` |
| Sun: source angle, movable shadow distance, cascade count | `Greybox/ArenaGreyboxActor.cpp:284-291` |
| Height fog actor (warm inscatter, directional inscatter toward the sun, start and cutoff) | `Greybox/ArenaGreyboxActor.cpp:295-312` |
| Sky dome: 5 bands placed in the seated camera's visible 10-29 deg window (gold just above the stands, lilac at the frame top) | `Greybox/ArenaGreyboxActor.cpp:352-402` |
| Quest config: `r.Shadow.CSM.MaxMobileCascades=2`, `r.Shadow.MaxCSMResolution=1024` | `Config/Android/AndroidEngine.ini:9-12` |
| Test `MageArena.Greybox.DuskLight` (forward shading on; sun pitch in [-25, -15]; sun ahead of the seated view; shadows on, 1-2 cascades, distance reaches the far stands; warm key; fill 35-120 % of the sun on the sand and rose-lilac; >=3 bands, gold horizon, lilac top, darkening upward; fog on, thin, starting past the farthest spawn marker and cut off before the dome; no light, sky or fog colour in a threat class) | `Tests/GreyboxTests.cpp:412-497` |
| `MageArena.Greybox.StyleSkin` also checks the two fog colours turn neutral at the greybox luminance under a style | `Tests/GreyboxTests.cpp:373-379` |

`r.ForwardShading=True` (`Config/DefaultEngine.ini:5`) is untouched and now pinned by the test. Nothing old was pinned
by value in `GreyboxTests.cpp`: the style test compares a style with the loaded greybox, so it follows the new light
without an edit beyond the fog lines.

## 2. Budget, before and after (D-G4 stress scene, `capture-budget.ps1`)

Each run: 60 s sample after a 10 s warm-up, 20 enemies, 100 projectiles, both hands, 6 Niagara systems. Every run
logged `MAGEVR_BUDGET_DONE`, no `MAGEVR_BUDGET_FAIL`, `particlesExercised` true and scene parity ok. BasePass is the
GPU-culled `r.MeshDrawCommands.DumpStats` reading (6 dumps per run) that judges the 350k row (DECISIONS 2026-10-24).

| Run | Draw calls median / max | BasePass tris median / max | ShadowDepth tris | All passes (logged, not gated) median / max | Live particles median / max | Parity |
|---|---|---|---|---|---|---|
| before1 | 99 / 104 | 137,792 / 139,232 | (no pass) | 371,700 / 375,666 | 1,399 / 1,468 | ok |
| after1 | 109 / 113 | 138,296 / 140,624 | 31,536 | 404,228 / 411,438 | 1,399 / 1,468 | ok |
| after2 | 109 / 113 | 138,056 / 139,664 | 31,536 | 403,672 / 408,538 | 1,397 / 1,468 | ok |
| after3 | 109 / 113 | 137,488 / 139,664 | 31,536 | 402,304 / 408,822 | 1,400 / 1,468 | ok |
| **Limit** | **<=150** | **<=350,000** | | | **<=2,000** | |

The dusk costs **+10 draw calls** (median 99 -> 109, max 104 -> 113: the shadow-depth pass over the 9 caster components at
2 cascades, the fog pass and the 3 extra dome bands) and a 31,536-triangle shadow-depth pass. BasePass does not move
(the shadow pass is a separate pass). All rows stay inside `docs/PROJECT-PLAN.md` section 7. The before reading is one
run (it was taken before any edit, so a re-run would have needed the old build); the after readings are three.
after1's drawn-part count was 425 against 385 in the other three runs; its parity check still passed (planned 210 parts /
11 tuples in all four).

The greybox capture scene (`capture-greybox.ps1`, the A02 scripted shots) moved from draw calls 23 / 28 / 46
(min / median / max) to 32 / 35 / 55, triangles 89,520 -> 89,664 max; `capture: ok` both times. A repeat on the binary
built from `b82042e` itself read 33 / 38 / 56 and drew a pixel-identical `01-initial-view.png` (the captures above came
from the build just before a no-op clamp edit to the band count; behaviour is the same).

## 3. Threat colour language under the warm light

The threats, telegraphs and session parts use the unlit material, so the sun cannot tint them, and the fog starts at
20 m, past the farthest spawn marker (15.1 m from the seated eye). The A02 capture's CIEDE2000 probe (threshold 10) reads
the same before and after: water body 0.40, fire body 0.82, steel body 0.61, unblockable body 2.20, unblockable rim 0.86
(`coloursOk` true). By eye in `04-threats-midflight.png` and the style-pick stills: turquoise water, ember-orange fire,
the steel ring and the black core with its red rim all read against the tan sand and the dusk sky. No sky band, fog or
light colour falls in a threat class (the new test checks it; the first draft of the rose band did fall in the ember
class and was moved).

## 4. Captures (outside the repo; nothing is committed)

Root: `C:/Users/kazda/mage-art-staging/t56-dusk-lighting-2026-10-24/`

- Greybox, before: `before/greybox/01-initial-view.png` ... `06-blink-midfade.png`, `before/greybox/budget.json`
- Greybox, after: `after/greybox/01-initial-view.png` ... `06-blink-midfade.png`, `after/greybox/budget.json`
- Style-pick smoke (`-Styles greybox, 13`, seed-1 Brennic duel), before and after: `before/style-pick/greybox-01..04.png`,
  `13-01..04.png`, `style-greybox.mp4`, `style-13.mp4`; the same names under `after/style-pick/`
- Budget json: `before/budget/before1/{driver,summary,samples}.json`, `after/budget/after1..after3/{driver,summary,samples}.json`
- Logs: `logs/` (builds, style-pick, budget, automation)

The key frames for the comparison below: `before/greybox/01-initial-view.png` against `after/greybox/01-initial-view.png`,
and `after/style-pick/greybox-03.png`, each beside `apps/vr/art/key-art/key-art-s5101-medium.jpg`.

## 5. Side by side with key art S1 (honest)

Pixel samples are sRGB from the 1920x1080 captures and the 1376x768 key art.

**Now matches, roughly:**
- **Sun angle and direction.** The key is low and ahead of the player, behind the opponent, so shadows run toward the
  viewer, as in S1 (Brennic's shadow falls toward the hands). Before, the sun was even lower (12 deg) but cast nothing.
- **Shadow length and softness.** Braziers, banner poles and cloth throw shadows about 2.7 times their height across the
  sand; the near banners' shadows reach the dais edge. Edges are soft at 1024-2048 texels over 45 m. S1's shadows are
  softer still and fall from people, not posts.
- **Warmth.** Lit sand is now (195, 138, 95) against S1's (188-206, 124-135, 69-76); before it was a bleached
  (255, 216, 160). A banner's shadow on the sand is (130, 97, 83). The side walls on their shadow side read mauve-grey
  (116, 95, 95) and the far wall through the haze (168, 128, 100), against S1's darker warm brown walls (110-113, 75-80,
  56-65); before they were blue-grey (138, 142, 162) and a washed (253, 198, 167).
- **Sky gradient.** The dome runs gold just above the stands (252, 189, 122; S1's horizon glow is 250, 176-196,
  100-105), through peach (232, 174, 138) and rose (211, 156, 150) to lilac-mauve at the frame top (192, 149, 160; S1's
  top is 193, 143, 120). Before, it was a flat orange band (194, 122, 65) under a navy one (65, 76, 110) with a hard
  edge.

**Still does not match, and why:**
- **The gradient is stepped.** Five flat unlit bands show their edges; S1 has a continuous gradient with clouds. A smooth
  sky wants a gradient material or a sky texture (an art asset), which is past greybox scope.
- **Contrast and saturation are lower than S1.** S1 is a painted frame with deeper shadows and more saturated sand; the
  greybox has flat albedos, no texture, no ambient occlusion and no tonemapper (the pawn's camera runs manual exposure,
  bias 0, tonemapper off). The fog adds a light veil on the far stands.
- **The opponent and every session actor cast no shadow.** The session parts (rival, enemies, projectiles) are built in
  `Session/SessionPresentation.cpp` through the greybox helper, which makes every part shadowless; opting them in is
  outside this card's paths. S1's strongest shadow read is Brennic's. The rival mesh itself is the mesh lane's (T38).
- **Materials, crowd, VFX, hands and meshes** are T57 (crowd visibility), T58 (spell VFX), T59 (hand mesh) and the
  Brennic mesh lane. The stands are still empty blocks, the braziers are cones, the projectiles flat spheres. No lighting
  change can stand in for them.
- **No emitted light from the braziers.** S1's torches glow; the greybox flames are colour only (a point light per
  brazier would cost forward-shading light slots and was not part of this card).

No claim of parity with S1 is made: the light, shadow direction and sky colours moved toward it; the frame does not
look like it.

## 6. Side effect: style palettes render darker

`ApplyStyle` keeps the greybox light's luminance and drops its hue (T16). The dusk sun on the sand is dimmer than the
old 8-lux light was, so a style renders darker than before. Style 13's sampled sand is about sRGB (183, 129, 63): before
it rendered at about (217, 166, 107), after at about (141, 117, 84). The capture gate passes either way, but if the
style pool is used to judge palettes, a style-specific exposure or light level is a follow-up.

## 7. Gates run

- `powershell -NoProfile -ExecutionPolicy Bypass -File apps/vr/tools/build.ps1`: `Result: Succeeded` (final build 30.6 s).
- `Automation RunTests MageArena.` (`-nullrhi`): found 294 tests, 294 `Result={Success}`, 0 failed, `TEST COMPLETE. EXIT
  CODE: 0`. Includes `MageArena.Greybox.DuskLight` and `MageArena.Overlay.Golden`.
- `Automation RunTests MageArenaDesign`: found 7, 5 `Success`, 2 `Fail` (section 8).
- Red first: with the `ApplyDuskLight();` call commented out, `MageArena.Greybox.DuskLight` failed (pitch -12 outside the
  band, shadows off, 0 cascades, blue fill, navy top band, fog off); restored and rebuilt, it passed with the rest of
  `MageArena.Greybox` (6/6).
- `node apps/vr/tools/lint/unused-members.mjs` OK, `node apps/vr/tools/lint/format-check.mjs` OK (4 changed C++ files).
- The node test gate (agy-guard, check-pin + self-test, check-overlay + self-test, leonardo-redaction, repo-law): all OK.

## 8. MageArenaDesign

`Census.Rivals`, `Census.Sweep`, `Duel.AirSeated`, `Session.CreaturesSeated`, `Session.Wave1Seated`: Success.
`Duel.FireSeated` and `Session.FullSeated`: Fail, the two known reds accepted under DF-011. 5/7, the same two reds as
before this card; no new red.

## 9. Questions this leaves

- **The JSON lighting block is now overridden.** `arena-layout.json`'s `lighting` block (and its note "Shadows stay off.
  No height fog") no longer describes what renders. Moving the dusk numbers into the JSON needs the T54 overlay goldens
  re-recorded (`-MageArenaRecordOverlayGolden`) and a written reason, so it is a separate card, not done here.
- **The rival casts no shadow.** Opting the session's parts into shadows is a one-line change per part in
  `Session/SessionPresentation.cpp` (outside T56's paths) at a measured shadow-pass cost.
- **Styles render darker** under the dusk luminance (section 6).
