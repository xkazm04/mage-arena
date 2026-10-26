# T68 - capture-rival.ps1 draws the committed cast from /Game/Characters

Status: open
Max turns: 120

## Goal
`capture-rival.ps1` has been broken on master since 16ffa56: the commandlet writes into `/Game/Characters`
(`CreatureImport::CharactersRoot`), the script still imports Brennic, the hound and the maw before every run, and a
re-import over the committed Brennic reads at scale 1 and fails the commandlet's height check (0.018 m against 1.7992 m).
The script must draw the committed cast and import nothing, and keep every output it had: stills, crop sheets, manifest,
clip timeline, SHA256SUMS.txt, budget runs. `capture-creature-import.ps1` is audited for the same failure.

## Read first
- `CLAUDE.md`, `docs/DECISIONS.md` (they outrank this card if they conflict)
- `docs/research/CREATURE-UE-IMPORT-2026-10-24.md`, `docs/research/CREATURE-CAST-UE-2026-10-24.md`
- `apps/vr/tools/capture-rival.ps1`, `apps/vr/tools/capture-creature-import.ps1`

## Deliverables (exact paths)
- `apps/vr/tools/capture-rival.ps1` - no import, no scratch folder, no sha256 source table; refuses to start when a
  committed asset is missing, an LFS pointer, or `Content/Characters` is not clean; fails when a run changes it.
- `apps/vr/tools/capture-creature-import.ps1` - the audit: it fails the same way (its default run imported all three
  over the committed assets), so imports are opt-in (`-Import <names>`, for an empty folder only), with the same
  clean-tree guard; `-DistanceM` passes `-MageArenaCreatureDistanceM`.

## Constraints
- Do not push. No paid service. Pinned data, conformance vectors and signed numbers untouched.
- One Unreal process at a time.

## Acceptance (the orchestrator re-runs these; quote their real output in your report)
```
capture-rival.ps1 -Budget -BudgetPrefix t68rival   # rival: ok, the Brennic scale check not hit, D-G4 ok
git status --short apps/vr/Game/Content/Characters   # empty after the run
```

## Report
`docs/research/HOUND-GLOW-8M-2026-10-26.md` section "T68" (runs/ is never committed).
