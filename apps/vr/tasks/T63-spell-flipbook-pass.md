# T63 - the spell flipbook pass

Status: open
Max turns: 120

## Goal
The magic reads like key art S1's water and fire VFX, not like soft spheres. The shared glow layer of T58 (`M_SpellGlow`)
gets a flipbook texture: Water a droplet and splash sheet, Fire a flame and ember sheet, Air a wisp sheet. Each glow
colour stays ONE instanced draw, so the call count is T58's.

This is the spectacle pass that `docs/DECISIONS.md` 2026-10-25 "Athena, on b872dcd (T58 spell VFX)" deferred ("a
flipbook flame and water texture on the shared glow layer, at the same call cost"), and item (4) of the arena order in
`docs/concepts/VISUAL-PLAN.md` (b). It spends the three T58 critics' S1 gaps where they cost no draw call:
1. a finer, denser-looking spray inside the texture, not more puffs;
2. more contrast for fire on the orange sand (a darker ember rim, still inside the fire class).

## Read first
- `CLAUDE.md`, `docs/DECISIONS.md` (they outrank this card if they conflict)
- `docs/research/SPELL-VFX-T58-2026-10-25.md` (all of it, sections 3-6 above all)
- `docs/concepts/VISUAL-PLAN.md` section (b) shortfall 3, the arena order, and the style block in section (c)
- `apps/vr/art/key-art/key-art-s5101-medium.jpg` (S1, the north-star frame)
- `docs/research/CROWD-T57-2026-10-25.md` section 6 (the ember-fleck watch item)

## Deliverables (exact paths)
- The flipbook atlas, authored procedurally and deterministically by committed source (at most 512x512 per sheet, mips
  on), and the material change in `Tools/CreateBudgetFxCommandlet.cpp` (`apps/vr/tools/create-budget-fx.ps1 -SpellVfx`).
- The frame pick per instance (per-instance custom data or time) in `Session/SpellVfx.{h,cpp}` and
  `Session/SessionPresentation.cpp`.
- A test that each glow colour stays one layer with the flipbook on, beside `Tests/SpellVfxTests.cpp`.
- The report `docs/research/SPELL-FLIPBOOK-T63-2026-10-25.md`, in the T58 report's shape.

## Constraints
- Do not push, and touch no repository other than this one. Do not call paid services. Do not edit `docs/DECISIONS.md`.
- The colour language stays exact: the opaque core keeps its threat colour; element colour = absorb, steel = dodge and
  never glows, black core with a red rim = leave. No white hot core on absorb cues. `SpellVfx::GlowColour` and the
  constants in `Greybox/BrazierFire.{h,cpp}` do not change. The air green is not re-picked.
- The braziers and the crowd tint are out of scope.
- No combat number changes: `apps/vr/data/PINNED.json`, `apps/vr/data/pinned/` and `apps/vr/data/conformance/` are
  never edited, and no overlay value moves. Bout difficulty, the kernel, DF-011 and DF-013 are not touched.
- Fits the Quest budget of `docs/PROJECT-PLAN.md` section 7; forward shading stays on. One Unreal process at a time.

## Acceptance (the orchestrator re-runs these; quote their real output in the report)
```
powershell -NoProfile -File apps/vr/tools/build.ps1   # expected: Result: Succeeded
MageArena. automation group   # expected: all green, SpellVfx tests included, plus the new one-layer test
MageArenaDesign               # expected: only Duel.FireSeated and Session.FullSeated red (DF-011)
capture-budget.ps1 -Runs 3    # expected: draw-call max <= 126 (+1 at most), BasePass <= 350k, particles + puffs <= 2,000
capture-spell-vfx.ps1 before and after, blind critic   # expected: no class regression, a VISIBLE GAIN in the school pairs
```

## Report
`docs/research/SPELL-FLIPBOOK-T63-2026-10-25.md`: outcome; a file:line table; decisions the card did not specify (card
or sub-UV, and why); the budget table before and after; the critic table unblinded; open points for Athena. Evidence
stills go to `C:/Users/kazda/mage-art-staging/spell-flipbook-t63-2026-10-25/`, not the repo.
