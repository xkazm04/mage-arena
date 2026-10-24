# T58 - spell VFX toward the key art

Status: open
Max turns: 120

## Goal
Spell effects read like the key art's ward and bolt VFX (A06 concepts) while the threat colour language stays exact. Particle counts stay at or under 2,000.

Owner answer 6 of 2026-10-24 (`docs/DECISIONS.md`, "2026-10-24 - Visual approvals"): the presentation gap is written as cards. This card answers shortfall 3 of `docs/concepts/VISUAL-PLAN.md` (b), "Scene dressing". No combat number changes, and the work fits the Quest budget of `docs/PROJECT-PLAN.md` section 7 (<=150 draw calls, <=350k visible triangles, <=2,000 particles).

## Read first
- `CLAUDE.md`, `docs/DECISIONS.md` (they outrank this card if they conflict)
- `docs/concepts/VISUAL-PLAN.md` section (b), shortfall 3, and the approved style block in section (c)
- `apps/vr/art/key-art/` (S1 is the north-star frame)

## Deliverables (exact paths)
- Niagara or particle system changes for the existing spells, named in the report.

## Constraints
- Do not commit, push, or touch any repository other than this one.
- Do not call paid services. Do not edit `docs/DECISIONS.md`.
- No combat number changes: `apps/vr/data/pinned/` is never edited, and no overlay value moves.
- Fits the Quest budget of `docs/PROJECT-PLAN.md` section 7; forward shading stays on.
- The threat colour language stays: element colour = absorb, steel = dodge, black core with a red rim = leave.
- One Unreal process at a time on this machine.

## Acceptance (the orchestrator re-runs these; quote their real output in your report)
```
powershell -NoProfile -File apps/vr/tools/build.ps1   # expected: Result: Succeeded
MageArena. automation group   # expected: green, threat-colour tests unchanged
capture stills of each school   # expected: closer to key art S1; particles <= 2,000
```

## Report
Write `runs/T58/REPORT.md`: what you built (paths), each acceptance command with its real output, anything you
could not do and the exact error, and decisions you made that the card did not specify.
