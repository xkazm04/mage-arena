# T69 - the cinder hound's glow at 8 m, on the committed asset

Status: open
Max turns: 120

## Goal
Athena names the hound's glow at 8 m as the next VR lane. Measure it on the COMMITTED `M_CinderHound` (not the C++
defaults the capture used to apply over it) at 8 m and 5 m under the T56 dusk, with the fixed instrument. Record whether
the committed material already carries the chosen soft crack bloom. If the glowing share at 8 m is still thin, tune it
through the soft bloom radius or intensity or a rim, never the tonemapper or the exposure. The cores stay orange: no
yellowed pixel, no white core.

## Read first
- `CLAUDE.md`, `docs/DECISIONS.md` (2026-10-24 cinder hound entries)
- `docs/research/CREATURE-CAST-UE-2026-10-24.md` section 2.4 (the candidates and the glow.py measure)
- `docs/research/CREATURE-UE-IMPORT-2026-10-24.md`, `docs/concepts/VISUAL-PLAN.md`
- `apps/vr/Game/Source/MageArenaVR/Greybox/CreatureImport.h` (`FEmberParams`), `CreatureImportCapture.cpp`

## Deliverables (exact paths)
- `CreatureImportCapture.cpp`: the "chosen" shots read the committed material's own parameter values and log them.
- `apps/vr/Game/Content/Characters/CinderHound/` only if a parameter changes (re-import through the commandlet).
- `docs/research/HOUND-GLOW-8M-2026-10-26.md` with before/after stills, share, core RGB, yellowed count, D-G4 over three
  runs, build and `MageArena.` pass count.
- One line in `docs/concepts/VISUAL-PLAN.md` and, where its row covers it, a WOLF-PLAN evidence line.

## Constraints
- Do not push. No paid service. Pinned data and every signed number untouched.
- No tonemapper or exposure change. One Unreal process at a time.

## Acceptance (the orchestrator re-runs these; quote their real output in your report)
```
build.ps1                                   # Result: Succeeded
MageArena. automation group                 # no new reds (the DF-011 MageArenaDesign reds are known)
capture-creature-import.ps1 -Budget -BudgetRuns 3   # draw calls <= 150, BasePass triangles within 350k
```

## Report
`docs/research/HOUND-GLOW-8M-2026-10-26.md`.
