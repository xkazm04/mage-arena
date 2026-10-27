---
title: T73 Fire second rim hue - built as a material rim term, missed the blind "never rust" bar, reverted to T67
channel: vr
date: 2026-10-27
status: RAN AND REVERTED - desktop proxy, not Quest truth. Route (ii), a rim term in M_SpellGlow, gave Fire's darkest tones a deeper red-orange (greybox sRGB hue 22.6 deg against Fire's 29.4 and the leave red's 2.4), with no draw-call or custom-data change (D-G4 +0 in both scenes). The bolt's edge-to-sand Weber went from -0.394 (T67) to -0.487 (T72 -0.569), and the blind critics put T73's bolt ahead on separation in 3 of 3 rounds and closer to S1 in 2 of 3. But rust was named on T73's edge in rounds 1 and 3, so by ruling 3's letter the bar failed and Fire went back to T67 byte for byte. The rim work stays in a7a4129..8b25887. DF-014 re-check recorded, Status OPEN.
sources: [apps/vr/tasks/T73-fire-second-rim-hue.md, docs/DECISIONS.md (2026-10-27 ruling 3), docs/research/FIRE-VALUE-T72-2026-10-26.md, docs/research/FIRE-VALUE-T72B-2026-10-26.md, docs/research/FIRE-VALUE-T72C-2026-10-26.md, docs/design-findings/DF-014-fire-wall-colour-vs-absorb.md, apps/vr/art/key-art/key-art-s5101-medium.jpg]
---

# T73 Fire second rim hue, 2026-10-27

**Label: desktop proxy (D3D12, forward shading, `-nohmd`), not Quest truth.** Engine UE 5.8.3 (`Build.version` read before
the first build). Nothing under `apps/vr/data/` changed, and no combat number moved. The braziers, the crowd tint and the
hound material are unchanged. Evidence is in `C:/Users/kazda/mage-art-staging/fire-rim-hue-t73-2026-10-27/` (`logs/`,
`tools/`, `T73-before/`, `T73-after/`, `T73-rimoff/`, `dg4/`, `look/`, `critic-N/`, `critic-keys/`,
`critic-verdicts-unblinded.md`).

## 1. Route chosen: (ii) a material rim term in M_SpellGlow

`Emissive = lerp(Color, RimColor, RimMix x saturate((RimShadeTop - shade) / (RimShadeTop - RimShadeBottom))) x shade`, with
the band at 0.52 / 0.44 of the cell's shade. In the atlas (`shade-hist.py`), the Fire flame cells' 1-3 px edge band sits at
0.38-0.46 (mean 0.459 / 0.466 on cells 4 / 5), so the outline takes the rim hue and the body (0.52+) and the heart keep Fire's.
`RimMix` defaults to 0 and was set (with `RimColor`) only on the tint of `ActiveElementPalette().Fire`
(`SpellVfx::GlowRimFor`, `ApplyGlowRim`). Every other tint, and the braziers' own tints of the same material, drew
`Color x shade` exactly. `-MageArenaFireRim=0` drew T67 in the same build.

Why (i), a 4th per-instance custom float, lost (as the card records):
- **Call cost:** (ii) is four parameters on an existing material instance per colour: no draw, component or instance
  data. (i) grows every layer's per-card data from 3 to 4 floats (+33 % upload, up to 500 cards a frame).
- **Custom-data shape:** (i) changes `CardCustomFloats` for Water, Air, SoftGlow, droplet and grit too. And one float a card
  cannot say *where* in the cell the rim is, so (i) still needs the same shade key in the material: it is (ii) plus a data change.
- **Quest mips:** the weight is computed from the filtered shade after the sample, so it follows the existing mip chain with
  no new channel to blur or compress. A separate mask channel would be filtered apart from the shade and could drift off
  the outline at low mips.

**The rim colour** (`SpellVfx::FireRimColour`): Fire's chroma moved along the line toward the palette's unblockable rim red,
stopped 0.165 (1.1 x `BrazierFire::ThreatChromaMargin`) short of it and never more than half way, at Fire's own brightest
channel, never above Fire in any channel. The engine's own numbers (`MAGEVR_T73_RIM`, `logs/test-green-editor.log`):

| Palette | Fire (linear) | Rim (linear) | Fire sRGB / hue | Rim sRGB / hue | Leave hue | rim-Fire / rim-leave chroma | rim luminance / Fire's |
|---|---|---|---|---|---|---|---|
| greybox | 0.900, 0.260, 0.020 | 0.900, 0.166, 0.017 | (243, 139, 39) / 29.4 | (243, 113, 35) / 22.6 | 2.4 | 0.105 / 0.165 | 0.822 |
| style 13 | 0.880, 0.209, 0.017 | 0.880, 0.159, 0.015 | (241, 126, 35) / 26.5 | (241, 111, 33) / 22.6 | 1.8 | 0.057 / 0.165 | 0.894 |
| style 25 | 0.982, 0.305, 0.027 | 0.982, 0.192, 0.025 | (253, 150, 46) / 30.1 | (253, 121, 43) / 22.2 | 2.5 | 0.115 / 0.165 | 0.811 |
| style 30 | 0.831, 0.250, 0.036 | 0.831, 0.167, 0.033 | (235, 137, 53) / 27.7 | (235, 113, 51) / 20.5 | 0.3 | 0.101 / 0.165 | 0.832 |

**Kept apart from the black-core/red-rim unblockable:** by chroma (at least 0.165 from the leave red in every palette, about
20 deg of hue), by value (only at shade 0.52 or less, while the leave rim is opaque at full value and pulses brighter), and by
shape (the outline of an orange flame round an orange heart, never a ring round a black core). No critic read the
unblockable class in any version.

A bug was found and fixed on the way (`8b25887`): `FLinearColor::GetMax` counts alpha, so the first build computed the
step on unnormalised colours (greybox rim 0.833, 0.154, 0.015). The mismatch between the test's chroma distances and a
Python check of the same rule showed it. The captures for the critics were all taken after the fix.

## 2. Tests, red first (on 6a7192c's material and stub functions, `74c2e3e`)

Added to `SpellVfxTests.cpp`: `GlowMaterialHasRimTerm`, `FireRimHueInItsBand` and `GlowRimLeavesOtherLayersUnchanged`
(no colour but Fire's gets a rim, and with `RimMix` 0 the term is `Color x shade` to the bit at all 256 shade levels, for
Water, Air, Earth, steel, the leave red and black, the rim pulse and both brazier layers). Red (`logs/test-red-editor.log`:
`Found 15 automation tests based on 'MageArena.SpellVfx'`, `EXIT CODE: -1`), among the lines:

- `Expected 'M_SpellGlow has the RimColor parameter' to be not null.`
- `Expected 'M_SpellGlow has RimMix, 0 by default (missing)' to be true.`
- `Expected 'the emissive lerps from Color to RimColor' to be true.`
- `Expected 'greybox: the rim is a second hue, not Fire's chroma (0.000 from it, at least 0.08)' to be true.`
- `Expected 'greybox: the rim hue (29.4 deg) is a deeper red-orange than Fire (29.4) and well off the leave red (2.4)' to be true.`
- `Expected 'greybox: the edge is darker by hue at T67's shade (luminance 0.1666 vs T67 0.1666, at most 0.92 of it)' to be true.`
- `Expected 'greybox: Fire's layer draws the rim' to be true.`

Two test corrections before green, both in `a7a4129`: the shade comparisons compare RGB only (`FLinearColor x float` also
scales alpha), and the "second hue" floor dropped from 0.08 to 0.05. Style 13's Fire sits nearest the leave red and has only
0.057 of room inside the 0.165 margin. The hue test (at least 3 deg deeper than Fire) held in all four palettes. Green on
T73: `Found 15 automation tests based on 'MageArena.SpellVfx'`, `EXIT CODE: 0`. `FireShadeNeverAboveOne` and
`FireCellsHaveHotCoreAndDarkBody` (the heart share) were untouched and green. The custom-data count stayed 3 (asserted).

## 3. Edge contrast (`fire-contrast.py`, Weber edge-vs-sand, the bolt scaled 3.5/8 to its 8 m size)

| Version | 04 bolt (edge / sand / Weber) | 05 absorb | 06 perfect |
|---|---|---|---|
| T67 (`T73-before`, 6a7192c) | 0.1836 / 0.3030 / **-0.394** | -0.298 | -0.168 |
| T72 (T72b's `T72b-before` capture) | 0.1281 / 0.2972 / **-0.569** | -0.539 | -0.452 |
| **T73** (`T73-after`) | 0.1555 / 0.3030 / **-0.487** | -0.353 | -0.168 |

The rim buys about two thirds of T72's bolt gain at T67's value (no darker shade): the step comes from hue alone. The
perfect ring does not move: its bead streak (cell 14) sits at shade 0.5-0.6, above most of the band.

## 4. D-G4 (`capture-budget.ps1 -Runs 3`, the scene named on every row; each set ended `budget: ok (3 runs)`)

| Run | Scene | Draw calls median / max | BasePass tris median / max | Live particles max | Puffs | Glow layers |
|---|---|---|---|---|---|---|
| before 1-3 (6a7192c) | stress | 105 / 110, 105 / 110, 105 / 110 | 165,622 / 167,230; 164,574 / 167,230; 164,574 / 167,230 | 1,468 / 1,467 / 1,468 | 408 | 2 |
| T73 1-3 | stress | 105 / 110, 105 / 110, 106 / 110 | 149,230 / 167,230; 164,910 / 166,478; 158,430 / 167,230 | 1,468 / 1,468 / 1,467 | 408 | 2 |
| revert 1-3 (T67) | stress | 106 / 110, 106 / 110, 105 / 110 | 164,574 / 167,230; 164,574 / 167,230; 165,622 / 167,230 | 1,467 / 1,468 / 1,468 | 408 | 2 |
| before 1-3 (6a7192c) | creature-import | 116 / 119, 116 / 119, 116 / 119 | 219,160 / 220,768; 218,424 / 220,768; 219,048 / 220,768 | 1,468 / 1,468 / 1,467 | 408 | 2 |
| T73 1-3 | creature-import | 116 / 119, 116 / 119, 116 / 119 | 218,384 / 220,768; 218,112 / 220,768; 218,112 / 220,768 | 1,468 / 1,468 / 1,468 | 408 | 2 |
| revert 1-3 (T67) | creature-import | 116 / 119, 116 / 119, 116 / 119 | 219,160 / 220,720; 219,048 / 220,720; 218,424 / 221,680 | 1,468 / 1,467 / 1,468 | 408 | 2 |
| Limit | | max <= 150 (127 guide) | <= 350,000 | particles + puffs <= 2,000 | | |

The call cost did not move: +0 draw calls in both scenes, BasePass within noise, particles plus puffs at most 1,876.

## 5. The blind critic (shuffled T67 / T73 pairs vs S1 at 8 m)

Three fresh agents, one per round, each seeing only its own folder: S1, three A/B pairs (04 bolt with the 8 m crop, 05
absorb, 06 perfect; set order and sides shuffled), and the T73 wall still. Seeds 7301-7303; keys in `critic-keys/`; the words
are quoted in `critic-verdicts-unblinded.md`.

| Round | Bolt separation | T73 bolt edge words | Reads as flame (T73 / T67) | Bolt closer to S1 | Rust, brown or rope named on T73 | On T67 | Absorb / perfect |
|---|---|---|---|---|---|---|---|
| 1 (7301) | **T73** | "thin dark burnt-orange/rust contour line" | weakly / weakly | T67, slightly | **rust, slight brown** (bolt) | mild mud | equal / equal (rope on both) |
| 2 (7302) | **T73** | "thin red-orange outline, crisp flame tongues" | yes / weakly | **T73** | none ("Not rust or brown") | brown/mud | T73 by a hair / equal (rope on both) |
| 3 (7303) | **T73** | "thin burnt red-orange outline, crisp tongues" | yes / weakly | **T73** | **"slight rust"** (bolt's outermost line; absorb's deepest shading) | brown/mud | T73 / about equal (rope on both) |

No round named a white core, an unblockable read or a steel read on either version.

**Verdict on ruling 3's bar:** "the edge must read as Fire, never rust". The edge read as Fire in rounds 2 and 3 ("yes") and
weakly in round 1, where T67's did too. But rust was named on T73's edge in rounds 1 and 3. By the letter the bar failed after
three rounds, so the card's fallback ran: `git revert` of `8b25887`, `46e1eed`, `a7a4129` and `74c2e3e` (`56fa123`) puts
`SpellVfx.{h,cpp}`, `SessionPresentation.cpp`, `CreateBudgetFxCommandlet.cpp`, `SpellVfxTests.cpp`, `M_SpellGlow.uasset` and
`T_SpellFlipbook.uasset` back to 6a7192c's bytes (`git diff 6a7192c -- apps/vr/Game/Content apps/vr/art` is empty). The
restored uassets are the committed bytes, not a re-import (a re-import is not byte-reproducible, T72c). The atlas PNG and
manifest never changed (`spell-flipbook.mjs --write` rewrote the same pixels, `c8185da4...`). Only the capture driver's
`-MageArenaSpellVfxWall` still remains (evidence tooling, off by default).

## 6. DF-014 re-check (report only; nothing decided)

The wall kept `ActiveElementPalette().Fire` (option A), and with T73 it shared Fire's layer, so it took the rim too. The still
(`T73-after/12-fire-bolt-wall.png`, rim off in `T73-rimoff/`) was taken through the capture-only `-MageArenaSpellVfxWall`:
the Fire mage 8 m out, a fire wall posed at the overlay's distance and arc facing the seat, and a Fire bolt drawn in Fire
3.5 m out in front of it. The live bout's palette was not touched. The same critics were asked whether the wall reads as a
threat to absorb or as a defence:

| Round | Threat or defence | Bolt vs wall |
|---|---|---|
| 1 | "a standing barrier, more like a hazard or a defence than an incoming attack ... more like the opponent's barrier, or simply arena fire, than the player's own" | "Only with effort", by the bolt's solid disc core |
| 2 | "a defence, a standing barrier, not an incoming attack ... I would guess it is the opponent's barrier, but nothing in the frame says whose it is" | "only just", by the disc core |
| 3 | "reads as a defence ... It is still ambiguous: the flames are exactly the element orange that means 'absorb', and nothing says whose barrier it is" | "Only barely ... The colour gives you no help" |

All three read a defence, not an incoming attack, from shape and stillness. All three also had trouble picking the Fire
bolt out of the wall's flames: the only cue is the bolt's opaque core, since colour, outline and silhouette are shared.
The second finding is new and belongs to DF-014's owner call. It is appended there as "T73 evidence", Status left OPEN.

## 7. Gates

- `build.ps1`: `Result: Succeeded` on 6a7192c, on the red state, on T73 (after the fix) and on the revert.
- `MageArena.SpellVfx` on T73: `Found 15 ...`, `EXIT CODE: 0`.
- `MageArena.` on the revert (corpus regenerated with `generate.mjs --corpus 30 --seed 1000` and `mousepaths.mjs`):
  `Found 344 automation tests based on 'MageArena.'`, `EXIT CODE: 0`, no reds (344 = the T71 count: the three T73 tests
  left with the revert).
- `create-budget-fx.ps1 -SpellVfx` (T73): `MAGEVR_BUDGET_FX M_SpellGlow blend=translucent shading=unlit twosided=1
  instanced=1 flipbook=T_SpellFlipbook customdata=3 rim=RimColor,RimMix,RimShadeTop=0.52,RimShadeBottom=0.44`,
  `done created=0 unchanged=6 failed=0`.
- `spell-flipbook.mjs --check`: `OK 512x512, pixels c8185da4a9c1de79187d0282c05fe55f161afc458eb9acc0202870a1e0ec46a7`
  (before, during and after).
- `capture-spell-vfx.ps1 -Run T73-before`: `MAGEVR_CAPTURE_DONE shots=11`. `-Run T73-after -ExtraArgs
  -MageArenaSpellVfxWall`: `shots=12`. `T73-rimoff` (`-MageArenaSpellVfxWall -MageArenaFireRim=0`, passed through
  `-Command` with `@()` because `-File` flattens the array): `shots=12`.
- Lint: `unused-members OK: 662 private/protected members in 46 classes`, `format-check OK`. Tests: `agy-guard`, `check-pin`
  (and `--self-test`), `check-overlay` (and `--self-test`), `leonardo-redaction`, `repo-law OK: 4/4 sub-checks`, all exit 0.
  `check-commits OK`.
- The overlay golden was not touched (no layout edit). At most one Unreal process ran at a time. `Binaries`, `Intermediate`,
  `Saved` and `runs/` are not committed; the uassets stayed LFS pointers.
- **Pixel identity of Water, Air, SoftGlow, droplet and grit** rests on the unchanged atlas and on the bit-exact
  `RimMix 0` test. The stills cannot show it: the capture is not frame-deterministic (Water's bolt shot differs between two
  runs of the same build, `T73-before` against `T73-rimoff`).

## 8. Open points (for Athena and the owner)

- **The word bar against the measured gain.** T73 won separation in 3 of 3 rounds (bolt), read as flame "yes" in 2 of 3
  where T67 read "weakly" in all 3, and was closer to S1 in 2 of 3. Its rust words were "rust" once and "slight rust" once.
  T67 drew "brown/mud" in all three rounds. As in T72c, "rust" is hard to separate from a dark red-orange edge on orange
  sand: a darker-than-sand red-orange is, by name, rust. If the owner rules the bar met on separation and S1 likeness,
  `a7a4129..8b25887` restores T73 (cherry-pick, re-import with `create-budget-fx.ps1 -SpellVfx`). As written, the rule says no.
- **A lighter variant was not tried** within the three rounds: a rim term that shifts hue without the darker value (the band
  at a higher shade, so the edge stays T67's value with a redder hue), or a smaller shift (more than 0.165 from the leave
  red). Both trade Weber contrast for distance from "rust". That needs a new lane.
- **DF-014:** the bolt hides among the wall's flames (section 6). That is a shape and colour question for the owner's call.
- **Quest truth** is unmeasured: ASTC on the 0.38-0.46 band and whether the rim term holds at the Quest's mips at 8 m.
