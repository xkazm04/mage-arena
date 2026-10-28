# Graphics push on Lynx, 2026-10-28 to 2026-10-30: two tiers toward S1

Owner, 2026-10-28: the project moved from Wolf to Lynx "because the graphics are terrible and not leading a good way";
"lets do anything necessary to move the graphics of this one 1v1 duels as close to art as possible". Two days, two
directions in parallel, then the owner decides whether the project continues. Authority: `docs/DECISIONS.md` 2026-10-28.

The target is **S1**, `apps/vr/art/key-art/key-art-s5101-medium.jpg`: first-person from the seat, two bare hands
casting a turquoise water ward, Brennic centre frame launching a fireball, a golden-hour sandstone colosseum with
a packed crowd, red banners, braziers on pylons, sand floor. The focus, in order: **opponent, spells, hands, surface**
(floor and arena), then everything else.

## Why the earlier lanes failed (do not repeat)

- Every surface is a flat tint on an engine material (`Greybox::BasicMaterial`, `Greybox::UnlitOpaqueMaterial` =
  the engine debug `LevelColorationUnlitMaterial`), with no texture, normal, AO or roughness variation.
- The pawn switches the tonemapper off, bloom 0 and manual exposure (`AMageArenaPawn::ConfigureFromLayout`,
  `ApplyFlatPresentation`), so light and glow cannot read.
- The sky is a stack of unlit boxes; the sky light is the engine's grey cubemap.
- Lanes T72-T75 tuned one colour at a time against painted art and were judged one at a time. A colour cannot fix an
  unshaded frame (VISUAL-PLAN (b) items 9-14).

What worked in BP's VR app (`kiro/bp`, spark `vr-fidelity-40`, score 5 -> 34/45): an element scorecard from the
concept's own camera; lit, shaded, textured surfaces; the filmic tone curve kept on; a grade fitted only to the excess
(`bp/scripts/lookdev/fit_grade_lut.py`); finishing (bevels, normals, baked AO) over regeneration; hands with regional
skin; isolating each light before retuning a material; draws and frame time logged with each look. Registry:
`game-production/content-pipeline/concept-scene-parity` and its techniques, and
`standalone-headset-frame-budgets/techniques/forward-path-feature-allowance` (Quest: bake AO and lighting, grade in
the shader, no post pass).

## The two directions

| | Tier A: **Quest** (`push/quest-look`) | Tier B: **Epic** desktop (`push/epic-look`) |
|---|---|---|
| Question it answers | Can the build that ships on Quest 3 look like S1? | How close can this game get to S1 at all? |
| Renderer | Mobile-forward-viable features only: forward shading, MSAA, textured lit materials, baked or shader AO, a painted sky dome, at most 1 shadowed directional light plus a few unshadowed point lights, a grade inside the shaders or the mobile tonemapper | Anything desktop: deferred, Lumen GI and reflections, virtual shadow maps, sky atmosphere and volumetric clouds, volumetric fog, bloom, a LUT grade, higher texture resolutions |
| Budget | D-G4 holds: draw calls at most 150, BasePass at most 350k triangles (`BudgetStressDriver`) | No budget; report the frame time |
| Owns the shared assets | Arena surfaces: stone, sand, dais, banner cloth, pillar and brazier textures and materials, the sky panorama, spell VFX textures | Hero assets: Brennic (texture, face, finishing), the player's hands (skinned, regional skin) |

Each tier lands behind a switch, `-MageArenaLook=flat|quest|epic`. **`flat` stays the default**, so the automation
suite, the colour-reading tests and every earlier capture keep their meaning. A shared asset lands in its own commit on
its owner's branch so the other branch can cherry-pick it (the orchestrator relays it). Assets used by both are staged
in `C:/Users/kazda/mage-art-staging/push-1028/`.

## The common yardstick

1. **The S1 shot** (Tier A builds it first, as a separate commit both branches carry): a deterministic capture
   `-MageArenaS1Shot` that seats the player on the centre pad, puts Brennic in his cast pose in front, a Fire bolt in
   flight toward the player, the player's water ward up with both hands in the ward pose, and writes one PNG at the
   game camera (FOV from the layout) plus one at a **key-art camera** (S1's framing: the narrower FOV that puts Brennic
   at S1's frame height). The game-camera frame is the truth (registry `score-only-what-the-device-shows`); the
   key-art frame is the comparison aid. 1376 x 768 like S1. The flags `-MageArenaLook=` apply.
2. **The scorecard**: 15 elements, each 0-3 against S1 (3 = reads like S1, 2 = same idea and lower finish, 1 = present
   and wrong, 0 = absent). 45 is S1.

| # | Element | S1 |
|---|---|---|
| 1 | Opponent body and costume | dark red tunic, heavy cloak, forearm wraps, collar, lit with a warm key and a rim |
| 2 | Opponent face | readable brows, eyes, beard, scowl at game distance |
| 3 | Opponent pose and presence | centre frame, mid-cast, arms out, large in the frame |
| 4 | Fire spell | fireball with flame licks, embers, a white-gold heart, bloom |
| 5 | Water ward | a turquoise arc of water, crest spray, droplets, translucency |
| 6 | Player hands | two bare skinned five-fingered hands, sleeves, skin shading |
| 7 | Sand floor | warm grain, soft shadow pooling, raked texture |
| 8 | Arena wall | sandstone ashlar blocks with joints, coping |
| 9 | Banners | red cloth with gold trim, hanging folds |
| 10 | Stands and crowd | tiered stone, a dense colourful cheering crowd |
| 11 | Pillars and braziers | stone pylons with bowl fires that light their surroundings |
| 12 | Sky | golden-hour gradient, soft clouds, distant hills |
| 13 | Light | low warm sun, long soft shadows, bounce, AO in the corners |
| 14 | Grade and atmosphere | warm orange against teal, light haze, glow |
| 15 | Foreground stone | the parapet and ledge at the bottom of the frame |

3. **Judging**: the builder scores its own frame, then the orchestrator runs three context-free judges per round on the
   game-camera frame beside S1. The day 2 verdict is a blind panel on anonymised frames (baseline, Tier A, Tier B) with
   the baseline as the control. Briefed judges run about 4 points high (BP); the blind number is the one reported.

## Fixed constraints (both tiers)

- The **threat colour language** still reads: element colour = absorb, steel = dodge, black core with a red rim =
  leave. A look that makes a threat ambiguous is a failed look, whatever its score.
- No combat-number, pinned-data or clip-corpus edit (`apps/vr/data/pinned/**`, `PINNED.json`, conformance, clips).
  Camera position and threat distances are gameplay; the key-art camera exists only in the S1 shot.
- `MageArena.` gets **no new red** on the default `flat` look. Lynx baseline (2026-10-28, master with the look switch):
  345 of 348 pass; `Fire.Golden`, `Rivals.NullPath` and `Overlay.Golden` are red on Lynx only (sim hashes under
  emulated x64, `docs/research/UE-ON-LYNX-2026-10-22.md`), green on Wolf. The DF-011 reds in `MageArenaDesign` stay accepted. `build.ps1` prints `Result: Succeeded` before every commit that touches C++.
- Unreal on Lynx: **at most one Unreal process per tier** (two machine-wide), and only with 8 GB or more free RAM.
- Paid images: the owner's Leonardo **web studio** (GPT Image 2.5 Sunburst) through
  `apps/vr/tools/keyart/studio-still.mts`, which downloads each still and deletes the generation from the studio.
  At most 40 stills per tier for the push (20 credits each). Never the API pool; never print a key.
- Generated and binary assets go through Git LFS. Commits are small, with conventional subjects
  (`node tools/check-commits.mjs`). Builders never push; the orchestrator merges and pushes after review.

## Schedule

- **Day 1 (10-28)**: both worktrees built; the S1 shot on both; first lit frame per tier; baseline and first scores.
- **Day 2 (10-29)**: hero assets (opponent, hands) and spells into both tiers; second and third rounds.
- **Day 3 morning (10-30)**: blind panel, cost figures, a one-page verdict for the owner: continue (which tier) or stop.

## Results

(Filled in per round: frame paths, scores, cost figures.) Frames: `C:/Users/kazda/mage-art-staging/push-1028/frames/`.

**Round 1 (2026-10-28, builders' self-scores on the game camera, not yet judged):** baseline 13/45, Quest r1 21/45
(`push/quest-look` `be533fd`, `189ef92`; D-G4 stress median 137 draws, p95 141, BasePass median 146k), Epic r1 23/45
(`push/epic-look` `90d793c`, `ff1f720`; 7.2 ms GPU at 1376x768). Both tiers lifted the arena (walls, sand, sky, sun,
banners) to about 2 each; the opponent, spells and hands stayed at 0-1, and the two tiers look alike, so the gap is
assets and grade, not the renderer. Engine pitfalls: a spawned directional light must be Movable before it is rotated;
virtual shadow maps must be off under forward shading or no shadows draw.

**Owner, 2026-10-28:** Brennic stays at his 16 m duel distance; push his presence instead (rim light, cast glow, pose,
up to 1.15x visual scale). The wardstones may be restyled as stone pylons, keeping the Tithe glow as the signal.

**Judge round 1 (2026-10-28, three blind context-free judges, anonymised game-camera frames vs S1):** baseline
13 / 12 / 14 (mean 13.0); Quest r3 (`1528e3a`) 29 / 25 / 25 (mean 26.3); Epic r2 (`99e5da0`) 25 / 24 / 23 (mean 24.0).
Quest led because it already carried the lit pylon fires and banners (`fcd0865`), which Epic r2 had not taken yet. All
three judges called the remaining gap art and composition, not rendering technology. Every judge named the opponent's
size at 16 m first (elements 1-3 score 1/0/1 in every frame), then the hands (pale, cropped, the HUD over them), the
ward's shape (S1's is an arch framing the fireball) and the glossy fireball core. Set: `push-1028/judge-r1/`.

**Judge round 2 (2026-10-28, three blind judges, frozen-clock S1 shot at pitch -10, baseline re-shot at the same
camera):** baseline 14 / 14 / 14 (mean 14.0); Quest r7 desktop (`e58e2dc`) 23 / 23 / 26 (24.0); Quest r7 on the
mobile renderer with MobileHDR off 23 / 23 / 26 (24.0, called "nearly the same image" as desktop by all three); Epic r6
(`ad912e2`) 26 / 25 / 30 (27.0). Epic led on the golden sky, the grade and the white-gold fireball heart; Quest's
teal-lilac regrade, tuned on region numbers, read as "muddy mauve" (lesson: perception beats region statistics).
Every judge again named the 16 m opponent first. One judge: the arena "is close to shipping quality", but next to S1 on
a store page the gap is the composition (opponent size, hands), not the rendering. Set: `push-1028/judge-r2/`.
