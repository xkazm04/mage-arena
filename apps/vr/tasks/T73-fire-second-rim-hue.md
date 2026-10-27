# T73 - Fire second rim hue

Status: ran, blind bar not met (rust named on the rim in rounds 1 and 3 of 3; separation won 3 of 3), Fire reverted to T67 (fallback) - report docs/research/FIRE-RIM-HUE-T73-2026-10-27.md
Max turns: 120

## Goal
Fire's edge carries a second, deeper red-orange rim hue, so the edge stands off the sand by hue as well as by value, without
the darker vermilion that T72, T72b and T72c showed critics read as brown, rust or rope
(`docs/research/FIRE-VALUE-T72-2026-10-26.md:150-160`, `FIRE-VALUE-T72B-2026-10-26.md`, `FIRE-VALUE-T72C-2026-10-26.md`).
The Fire cells keep T67's shades (the value T67's critics read as fire). Only the hue of the darkest tones moves, from
T67's ochre toward red. This is owner ruling 3 of 2026-10-27 (`docs/DECISIONS.md`).

## Owner ruling 3 (2026-10-27)
"Fire SECOND RIM HUE lane approved: a 4th custom float or a material rim term carrying a deeper red-orange." Pass bar: blind
critics on shuffled pairs vs S1 at 8 m; the edge must read as Fire, never rust. D-G4 still binds: draw calls at most 150
(127 is the guide), BasePass at most 350k, and every figure names its scene. Under this ruling the custom-data shape and the
`SpellVfx` / `M_SpellGlow` rim parameters are levers. The other T72c constraints stand.

## Route: (ii) a material rim term in M_SpellGlow. Why (i), a 4th per-instance custom float, lost
- **The term:** `Emissive = lerp(Color, RimColor, RimMix x RimWeight(shade)) x shade`, with
  `RimWeight = saturate((RimShadeTop - shade) / (RimShadeTop - RimShadeBottom))`. The darkest tones of a cell (T67's outline,
  the 1-3 px edge band at shade 0.38-0.46) take the rim hue; the body and the heart (shade 0.52 and up) keep Fire's. `RimMix`
  is 0 by default, so every layer but Fire's, and the braziers (their own tints of the same material), draw exactly as before:
  `lerp(a, b, 0) = a`. The presentation sets `RimColor` and `RimMix = 1` only on the tint of `ActiveElementPalette().Fire`.
- **Call cost:** (ii) adds three scalar and one vector parameter on one existing material instance per colour: no draw, no
  component, no instance data. (i) would grow every glow layer's per-instance data from 3 to 4 floats (+33 % custom-data
  upload per card per frame, up to `MaxPuffs` = 500 cards), Water's and Air's layers included.
- **Custom-data shape:** (i) changes `CardCustomFloats` for every school, so Water, Air, SoftGlow, droplet and grit change
  shape even if their pixels do not; (ii) leaves it at 3. And (i) does not remove the material work: one float per card is one
  value per card, so it cannot say *where* in the cell the rim is. It needs the same shade key in the material (or a mask
  channel in the atlas) on top. (i) is (ii) plus a data change that buys nothing the slice needs (a per-card rim strength).
- **Quest mips:** the rim weight is computed from the filtered shade after the sample, so it follows the mip chain the cells
  already have: no new channel to blur and no new ASTC block content. At 8 m the outline's mip texels average toward the
  transparent border (shade toward 0), and they take the rim hue, which is the edge we want. A mask channel (the other way to
  feed either route) would be compressed and filtered separately from the shade and could drift off the outline at low mips.

## How the rim stays apart from the unblockable (black core, red rim = leave)
- **Chroma:** `SpellVfx::FireRimColour` moves Fire's chroma along the line toward the palette's unblockable rim red and stops
  `FireRimLeaveMargin` (1.1 x `BrazierFire::ThreatChromaMargin`) short of it, and never more than half way. In the greybox
  palette that is about sRGB hue 23 deg against Fire's 29 deg and the leave rim's 2 deg.
- **Value:** the rim hue only ever appears at shade 0.52 or less of Fire's brightest channel; the leave rim is drawn opaque at
  full value and pulses brighter (`RimPulseGain` 1.34).
- **Shape:** it is the outline of an orange flame around an orange heart (shade 1 Fire), never a ring around a black core.
  The unblockable's own glow (SoftGlow in its rim red) is another layer, and its `RimMix` stays 0.
- `MageArena.SpellVfx.FireRimHueInItsBand` holds it in all four palettes (greybox, styles 13, 25, 30).

## Read first
- `CLAUDE.md`, `docs/DECISIONS.md` (they outrank this card if they conflict)
- `apps/vr/tasks/T72c-fire-bolt-only.md`, `docs/research/FIRE-VALUE-T72-2026-10-26.md:150-170`,
  `FIRE-VALUE-T72B-2026-10-26.md`, `FIRE-VALUE-T72C-2026-10-26.md` (method, tools, critic protocol)
- `docs/concepts/VISUAL-PLAN.md` (b), (c); `docs/gameplay/04-threat-language.md`;
  `docs/design-findings/DF-014-fire-wall-colour-vs-absorb.md`; S1 = `apps/vr/art/key-art/key-art-s5101-medium.jpg`

## Deliverables (exact paths)
- `apps/vr/Game/Source/MageArenaVR/Session/SpellVfx.{h,cpp}` - `FireRimColour`, `GlowEmissive` (the material's term as a pure
  function, so the tests hold it), the rim constants and `ApplyGlowRim` (sets the rim on Fire's tint).
- `apps/vr/Game/Source/MageArenaVR/Session/SessionPresentation.cpp` - the Fire layer's tint gets the rim. A capture-only
  `-MageArenaFireRim=0` draws it off (T67) in the same build.
- `apps/vr/Game/Source/MageArenaVR/Tools/CreateBudgetFxCommandlet.cpp` - the rim term in `M_SpellGlow`.
- `apps/vr/Game/Source/MageArenaVR/Session/SpellVfxCapture.cpp` - `-MageArenaSpellVfxWall` adds a 12th still, a Fire bolt in
  Fire in front of a posed fire wall (DF-014 evidence; the live bout's palette is untouched).
- `apps/vr/Game/Content/Vfx/M_SpellGlow.uasset`, `T_SpellFlipbook.uasset` - re-imported by `create-budget-fx.ps1 -SpellVfx`
  (LFS). The atlas PNG and manifest are re-rendered with `spell-flipbook.mjs` and stay byte-identical (no cell changes).
- `apps/vr/Game/Source/MageArenaVR/Tests/SpellVfxTests.cpp` - red first on 6a7192c: the rim term is in the material, the
  rim hue is in its band, the other schools and the braziers draw unchanged. `FireShadeNeverAboveOne` and
  `FireCellsHaveHotCoreAndDarkBody` (the heart share) stay.
- `docs/research/FIRE-RIM-HUE-T73-2026-10-27.md` - desktop proxy, not Quest truth.
- `docs/concepts/VISUAL-PLAN.md` (b) - one T73 line. `docs/design-findings/DF-014-fire-wall-colour-vs-absorb.md` - a dated
  "T73 evidence" section, Status left OPEN.

## Constraints
- Do not push; touch no other repository. Do not edit `docs/DECISIONS.md`. Nothing under `apps/vr/data/` changes.
- No new element hue outside Fire's red-orange family. Water, Air, SoftGlow, droplet and grit stay pixel-identical; the
  braziers, the crowd tint and the hound material do not change. Every cell stays a grey mask with shade at most 1.
- DF-014: report only. The wall's colour does not change (it keeps `ActiveElementPalette().Fire`, and with it Fire's rim).
- At most one Unreal process machine-wide. Binaries through Git LFS; never commit `Binaries/`, `Intermediate/`, `Saved/`,
  `DerivedDataCache/` or `runs/`. Evidence in `C:/Users/kazda/mage-art-staging/fire-rim-hue-t73-2026-10-27/` only.

## Acceptance (quote the real output in the report)
```
build.ps1                                                     # Result: Succeeded
MageArena. automation group (corpus regenerated)              # Found N >= 344, EXIT CODE: 0, no reds
capture-budget.ps1 -Runs 3                                    # before (6a7192c) and after, stress scene
capture-budget.ps1 -Runs 3 -ExtraArgs -MageArenaCreatureImportScene   # before and after, creature-import scene
                                                              # draw calls <= 150 (127 guide), BasePass <= 350k, particles + puffs <= 2,000
capture-spell-vfx.ps1 -Run T73-before / -Run T73-after        # 11 shots each (after also -MageArenaSpellVfxWall: 12)
fire-contrast.py                                              # T67 / T72 / T73 Weber edge-vs-sand
spell-flipbook.mjs --check                                    # OK
create-budget-fx.ps1 -SpellVfx                                # the rim term in the log line
```
**Pass bar (ruling 3):** blind critics on shuffled pairs vs S1 at 8 m, a fresh agent per round, seeds and keys kept, at most 3
rounds; T67 (master) against T73; the edge must read as Fire and never as rust.
**If the bar fails after 3 rounds:** revert Fire to T67 (the rim term off and the code, material and tests back to 6a7192c),
re-import, re-run the build, the suite and D-G4, and report, as T72c did.

## Report
`docs/research/FIRE-RIM-HUE-T73-2026-10-27.md`; the evidence stays in the staging folder above.
