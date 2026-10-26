# T67 - spell spectacle round 2

Status: ran, open for Athena - report docs/research/SPELL-SPECTACLE-R2-T67-2026-10-26.md
Max turns: 120

## Goal
Fire reads with more volume and stronger contrast against the orange sand, closer to key art S1, at the same call cost
as T63. The levers are the Fire flipbook cells (denser layered flame licks, a darker outline, a full-colour heart), the
Fire card sizes in `Session/SpellVfx.{h,cpp}` and, if needed, the `M_SpellGlow` parameters. Water, Air and the
Unblockable stay at least as good.

## Read first
- `CLAUDE.md`, `docs/DECISIONS.md` (they outrank this card if they conflict)
- `docs/concepts/VISUAL-PLAN.md` section (b) and the (c) style block (S1 = `apps/vr/art/key-art/key-art-s5101-medium.jpg`)
- `docs/research/SPELL-FLIPBOOK-T63-2026-10-25.md` (the method repeated here)

## Deliverables (exact paths)
- The revised Fire cells in `apps/vr/tools/vfx/spell-flipbook.mjs`, the atlas and manifest in `apps/vr/art/vfx/`, the
  re-imported `Content/Vfx` assets (LFS).
- The Fire card sizes in `Session/SpellVfx.{h,cpp}` and tests in `Tests/SpellVfxTests.cpp`.
- `docs/research/SPELL-SPECTACLE-R2-T67-2026-10-26.md` and one line in `docs/concepts/VISUAL-PLAN.md`.

## Constraints
- No new draw call, emitter, puff or cap change: each glow colour stays one instanced draw. D-G4 max draw calls at or under
  127 in every run, triangles at or under 350k.
- `SpellVfx::GlowColour`, `RimPulse`, `RimPulseColour`, the braziers and the crowd tint do not change. Steel never glows.
  No glow brighter than its threat: the cells' shade stays at most 1, no white hot core.
- Pinned data, conformance vectors and the overlay untouched; no combat number moves. No paid service, no image generation.
- One Unreal process at a time. Do not push.

## Acceptance
```
build.ps1                             # Result: Succeeded
MageArena. automation group           # 0 failures, at or above 336 passes, the new tests included
capture-budget.ps1 -Runs 3            # draw-call max <= 127 in every run, triangles <= 350k
capture-spell-vfx.ps1 before/after + blind critic   # fire pairs won, no class regression
```
