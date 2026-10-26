# T72c - Fire bolt only: T72b's flame A/B cells on T67

Status: open
Max turns: 120

## Goal
Bring back only the gain T72b's bolt earned, and nothing else. T72b's bolt (cells 4 and 5, the flame A/B cells) met all three
of Athena's bars in every blind round; the bar failed only on cells her own rulings had locked, the flame star (cell 7, kept as
T72's, read as rust or mud) and the spark streak (cell 14, T67's, read as rope)
(`docs/research/FIRE-VALUE-T72B-2026-10-26.md`). T72c restores T72b's cells 4 and 5 exactly as `d948720` drew them and leaves
every other cell at T67 (`f557790`). This is the visible 10-31 Fire gain under goal 1.

## Athena's ruling (2026-10-26): OPTION C
Quoted from the dispatch of this task: "Athena ruled OPTION C at 14:01Z. T72b's bolt met all three of her bars in every blind
round. The bar failed only on cells her own rulings had locked (the star, cell 7, and the streak, cell 14). ... T72c brings
back only the bolt's gain." The ruling: restore only the flame A/B cells (4, 5); the star (7) and the streak (14) stay at T67;
the blind critic judges the bolt only (shot 04), and the absorb and the ring are proven by the atlas diff alone.

## Read first
- `CLAUDE.md`, `docs/DECISIONS.md` (they outrank this card if they conflict)
- `apps/vr/tasks/T72b-fire-lick-tips.md`, `docs/research/FIRE-VALUE-T72B-2026-10-26.md` (its method, tools, critic protocol and
  tables are repeated here), `docs/concepts/VISUAL-PLAN.md` sections (b) and (c); S1 = `apps/vr/art/key-art/key-art-s5101-medium.jpg`
- `git show d948720 -- apps/vr/tools/vfx/spell-flipbook.mjs`

## Deliverables (exact paths)
- `apps/vr/tools/vfx/spell-flipbook.mjs` - cells 4, 5 only: backing grown 0.05 tapering to 0.01 at the lick tips (`tipGrow`),
  T67's crisp edge ramp, rim 0.22, shade 0.28-0.34, heart 0.46, T72's dark sparks. The sparks' rim/halo and `flameLayer` growth
  are options only these two cells pass; cells 6, 7 and 14 keep T67's.
- `apps/vr/art/vfx/spell-flipbook.png`, `manifest.json` - re-rendered (LFS). Proof by atlas diff: cells 4 and 5 are
  pixel-identical to `d948720`'s, every other cell to `f557790`'s (cells 7 and 14 named).
- `apps/vr/Game/Content/Vfx/T_SpellFlipbook.uasset`, `M_SpellGlow.uasset` - re-imported with `create-budget-fx.ps1 -SpellVfx`
  (LFS), customdata=3, no parameter change.
- `apps/vr/Game/Source/MageArenaVR/Tests/SpellVfxTests.cpp` - tests that hold a crisp tip on cells 4 and 5 (the dark cap at most
  about half T72's, and present), and that hold cells 7 and 14 at T67. Red on the `f557790` atlas first, failure lines quoted.
  `FireShadeNeverAboveOne` and the heart-share test stay.
- `docs/research/FIRE-VALUE-T72C-2026-10-26.md` - desktop proxy, not Quest truth: the atlas-diff table, the T67 / T72 / T72c
  bolt contrast, the D-G4 table, the critic table, the verdict on each part of the bar, the gates, open points.
- `docs/concepts/VISUAL-PLAN.md` section (b) - one T72c line, and one option-only note: the bead ring (cell 14) reads as rope
  even in T67's own cell, a shape problem of the bead ring, an option for the owner, not a lane.

## Constraints
- Do not push, and do not touch any repository other than this one. Do not edit `docs/DECISIONS.md`.
- At most one Unreal process machine-wide: build, editor, automation and capture strictly one after another.
- No new hue; per-instance custom data stays three floats; every cell a grey mask with shade at most 1; no white core.
  `SpellVfx::GlowColour` / `RimPulse` / `RimPulseColour`, the braziers, the crowd tint and the hound material do not change.
  Water, Air, SoftGlow, droplet and grit stay pixel-identical. Call cost stays T67's. Nothing under `apps/vr/data/` changes.
  `Session/SpellVfx.{h,cpp}` and `M_SpellGlow` parameters are not levers.
- Binaries through Git LFS; never commit `Binaries/`, `Intermediate/`, `Saved/`, `DerivedDataCache/` or `runs/`.
- Evidence in `C:/Users/kazda/mage-art-staging/fire-value-t72c-2026-10-26/` only.

## Acceptance (quote the real output in the report)
```
build.ps1                                              # Result: Succeeded
MageArena. automation group (corpus regenerated)       # Found N automation tests, N >= 343, EXIT CODE: 0, no new reds
capture-budget.ps1 -Runs 3 -ExtraArgs -MageArenaCreatureImportScene   # before (f557790) and after, scene on every row:
                                                       # max draw calls <= 127 (and <= 150), BasePass <= 350k, particles + puffs <= 2,000
capture-spell-vfx.ps1 -Run T72c-before / -Run T72c-after   # 11 shots each
fire-contrast.py (bolt at 8 m size)                    # T67 / T72 / T72c Weber edge-vs-sand
blind critic on shuffled T67 / T72 / T72c bolt sets     # fresh agent per round, seeds and keys kept, at most 3 rounds
spell-flipbook.mjs --check                             # OK on the committed PNG
create-budget-fx.ps1 -SpellVfx                         # customdata=3
```

**Pass bar (Athena's), on the bolt (shot 04), all three must hold:** (a) T72c separates from the sand at least as well as T67;
(b) it reads as flame, with no brown, rust or rope named; (c) it sits closer to S1 than T72.

**If the bolt fails any part after 3 rounds:** revert `spell-flipbook.mjs`, the PNG, the manifest, both uassets and the tests to
`f557790` exactly, re-import, re-run the build, the suite and D-G4, and report the revert with the reason.

## Report
`docs/research/FIRE-VALUE-T72C-2026-10-26.md`; the evidence stays in the staging folder above.
