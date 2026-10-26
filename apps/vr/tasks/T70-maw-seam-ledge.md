# T70 - the mire maw's seam ledge and ground notch, removed at the cut

Status: open
Max turns: 120

## Goal
The maw's flank seam is matched in colour (0.012 or less per channel, CREATURE-CAST-UE section 1.5), but two blind critics
still see it as geometry: a straight vertical ledge where the s7 front half's skin stands proud of the s7301 back half's,
and a square notch at the ground (section 5). Remove both with a geometry pass at the cut in Blender only, keep the s7
rig fit, the four clips and the colour step, and replace the committed maw under `Content/Characters/MireMaw/` with the
fixed mesh through a delete-first re-import scoped to that one creature.

## Read first
- `CLAUDE.md`, `docs/DECISIONS.md` (they outrank this card if they conflict)
- `docs/research/CREATURE-CAST-UE-2026-10-24.md` (Phase A, section 5 "The maw's seam ledge")
- `docs/research/MAW-MV-2026-10-24.md` (merge_halves, the seam masks), `docs/research/CREATURE-UE-IMPORT-2026-10-24.md`
- `docs/concepts/VISUAL-PLAN.md`, `apps/vr/tools/mesh/README.md`, `apps/vr/tools/mesh/budgets.json`

## Deliverables (exact paths)
- `apps/vr/tools/mesh/seam_ledge.py`, `ledge_lib.py`, `test_ledge_lib.py`, README "The seam ledge": measure the ledge on
  orthographic side depth maps (500 px/m, both flanks) and smooth it on the finished LOD0 (one of section 5's three
  routes: a smoothing band, positions only).
- `Tools/ImportCreatureMeshCommandlet.cpp` `-replace`, `Greybox/CreatureImport.{h,cpp}` `ReplaceablePackageFiles` /
  `ExpectedAssetNames`, `apps/vr/tools/capture-creature-import.ps1 -Replace`, and the test
  `MageArena.CreatureImport.ReplaceOnePackagePerName`.
- `apps/vr/Game/Content/Characters/MireMaw/` replaced (same asset names), only if the stop rule below does not fire.
- `docs/research/MAW-SEAM-LEDGE-T70-2026-10-26.md`: ledge numbers before and after, the critic's verdicts, stills paths,
  each acceptance command with its real output, the commit list. One WOLF-PLAN evidence line, one VISUAL-PLAN line.

## Constraints
- Do not push. No paid service, no image generation. Do not edit `docs/DECISIONS.md`.
- Never touch `apps/vr/data/PINNED.json`, `apps/vr/data/pinned/`, `apps/vr/data/conformance/` or the clip corpus.
- No Brennic or CinderHound asset changes. At most one Unreal process machine-wide.
- Intermediates in `C:/Users/kazda/mage-art-staging/maw-seam-ledge-2026-10-26/`, never in the repo; `.uasset` and
  `.fbx` through LFS.
- Structure: 1 component, watertight, tris within the MireMaw budget and at most 17,490 + 10 %.
- STOP RULE: if the geometry fails structure, the critic reads the ledge as no better, or the import cannot be made
  clean, leave `Content/Characters/MireMaw` alone and commit only this card, the tool change and a declining report.

## Acceptance (the orchestrator re-runs these; quote their real output in your report)
```
python -m unittest apps/vr/tools/mesh/test_ledge_lib.py                  # expected: OK
powershell -NoProfile -File apps/vr/tools/build.ps1                     # expected: Result: Succeeded
UnrealEditor-Cmd ... -ExecCmds="Automation RunTests MageArena.;Quit"    # expected: 338 + new, all green
UnrealEditor-Cmd ... -ExecCmds="Automation RunTests MageArenaDesign.;Quit"  # expected: only the two DF-011 reds
powershell -NoProfile -File apps/vr/tools/capture-rival.ps1             # expected: ok
git lfs ls-files | grep MireMaw                                          # expected: the replaced files
```

## Report
`docs/research/MAW-SEAM-LEDGE-T70-2026-10-26.md` (runs/ is never committed).
