# T72b - Fire value contrast with flame-lick tips

Status: ran, pass bar not met, Fire cells reverted to T67 (fallback) - report docs/research/FIRE-VALUE-T72B-2026-10-26.md
Max turns: 120

## Goal
Keep T72's value gain (the fire separates from the orange sand at 8 m by a dark edge, not by hue) and give back the flame
look T72 lost. In T72's 8 m crops the bolt read as a soft brown blob and the perfect ring's spark streak read as rope
(`docs/research/FIRE-VALUE-T72-2026-10-26.md` section 4). T72b keeps the dark edge where it helps, narrows it where it
blunted the licks, and returns the streak to T67's. This is the visible 10-31 gain and the step that unblocks the VR push.

## Athena's ruling (2026-10-26): T72 option B, with one more condition
1. Return the spark streak (cell 14, the perfect ring) to T67's cell exactly: its dark tail and its rim and halo values.
   If the T67 streak shares the spark rim/halo code with cells 4-7, give the streak its T67 values without changing the
   other cells.
2. Narrow the Fire flame backing on flame A and B (cells 4, 5) so each lick keeps a crisp tip. The dark band follows the
   outline at about half of T72's growth (T72 grew it by 0.1, so about 0.05), not a filled halo. Re-tune the soft ramp and
   shade so the tips stay sharp. The heart stays the full element colour.
3. Keep T72's flame-star backing (cell 7) as it is.
4. No new hue and no custom-data change: per-instance custom data stays three floats, every cell is a grey mask with shade
   at most 1, and there is no white or yellow-white core. `SpellVfx::GlowColour`, `RimPulse`, `RimPulseColour`, the
   braziers, the crowd tint and the hound material do not change. The Water, Air, SoftGlow, droplet and grit cells stay
   pixel-identical. The call cost is T67's: no new draw call, emitter, puff, glow layer or cap change. Nothing under
   `apps/vr/data/` changes, and no combat number moves. `Session/SpellVfx.{h,cpp}` and `M_SpellGlow` parameters are not
   levers this time.

## Read first
- `CLAUDE.md`, `docs/DECISIONS.md` (they outrank this card if they conflict)
- `apps/vr/tasks/T72-fire-value-contrast.md`, `docs/research/FIRE-VALUE-T72-2026-10-26.md` (the method, the tools and the
  tables repeated here), `docs/research/SPELL-SPECTACLE-R2-T67-2026-10-26.md`
- `docs/concepts/VISUAL-PLAN.md` sections (b) and (c); S1 = `apps/vr/art/key-art/key-art-s5101-medium.jpg`
- T67's cells: `apps/vr/tools/vfx/spell-flipbook.mjs` at 587d085 / 6cbe7d9; T72's at 4a26671

## Deliverables (exact paths)
- `apps/vr/tools/vfx/spell-flipbook.mjs` - cells 4, 5 (narrow backing, crisp tips) and 14 (T67's streak); 6 and 7 unchanged.
- `apps/vr/art/vfx/spell-flipbook.png`, `apps/vr/art/vfx/manifest.json` - re-rendered (LFS).
- `apps/vr/Game/Content/Vfx/T_SpellFlipbook.uasset`, `M_SpellGlow.uasset` - re-imported with `create-budget-fx.ps1
  -SpellVfx` (LFS), no parameter change.
- `apps/vr/Game/Source/MageArenaVR/Tests/SpellVfxTests.cpp` - a test that holds the streak at T67's values (and the streak
  out of the dark-rim bound), and a test that holds a crisp tip on cells 4 and 5 (the dark cap over the licks at most about
  half T72's). Red on the 732ca65 atlas first, the failure lines quoted. `FireShadeNeverAboveOne` and the heart-share test
  stay.
- `docs/research/FIRE-VALUE-T72B-2026-10-26.md` - desktop proxy, not Quest truth: the T67 / T72 / T72b contrast table, the
  D-G4 table, the critic table, the pass-bar verdict, the gates, open points.
- `docs/concepts/VISUAL-PLAN.md` section (b) - one T72b line, and one option-only note: the steel stone reads off-white to
  every critic, a colour-language option for the owner, not a lane.

## Constraints
- Do not push, and do not touch any repository other than this one. Do not edit `docs/DECISIONS.md`.
- At most one Unreal process machine-wide: build, editor, automation and capture strictly one after another.
- Binaries through Git LFS; never commit `Binaries/`, `Intermediate/`, `Saved/`, `DerivedDataCache/` or `runs/`.
- Evidence in `C:/Users/kazda/mage-art-staging/fire-value-t72b-2026-10-26/` only.

## Acceptance (quote the real output in the report)
```
build.ps1                                              # Result: Succeeded
MageArena. automation group (corpus regenerated)       # Found N automation tests, N >= 341, EXIT CODE: 0, no new reds
capture-budget.ps1 -Runs 3 -ExtraArgs -MageArenaCreatureImportScene   # before (732ca65) and after, scene named on every
                                                       # row: max draw calls <= 150 and not above 127, BasePass <= 350k,
                                                       # particles + puffs <= 2,000
capture-spell-vfx.ps1 -Run T72b-before / -Run T72b-after   # 11 shots each; T67 column = T72's T67 stills if byte-valid
fire-contrast.py (T72's method, bolt at 8 m size)      # T67 / T72 / T72b Weber edge-vs-sand, shots 04, 05, 06
blind critic on shuffled T67 / T72 / T72b triples      # fresh agent per round, seeds and keys kept, at most 3 rounds
spell-flipbook.mjs --check                             # OK on the committed PNG
create-budget-fx.ps1 -SpellVfx                         # customdata=3
```

**Pass bar (Athena's), on the fire triples, all three must hold:** T72b separates at least as well as T67; it reads as flame
with no brown, rust or rope named; and it sits closer to S1 than T72.

**If T72b cannot pass after 3 rounds:** revert the Fire cells (4, 5, 7, 14 and the spark rim/halo) to T67 exactly, revert
the tests to match, re-render and re-import the atlas, re-run the build, the suite and D-G4, and report the revert with the
reason.

## Report
`docs/research/FIRE-VALUE-T72B-2026-10-26.md`; the evidence stays in the staging folder above.
