## What & why
<!-- One or two sentences. Name the task card (apps/vr/tasks/<id>-*.md) or the decision in docs/DECISIONS.md this serves. -->

## Before you write code
- [ ] Read `docs/DECISIONS.md` (it outranks every other document) and the card's context files
- [ ] Combat numbers: the pinned copy under `apps/vr/data/pinned/` is never edited; a change goes back as a change request, seated tuning goes in `apps/vr/data/vr/combat.vr.json`
- [ ] No shortcut that skips the gesture pipeline is presented as testing a gesture (`docs/DESKTOP-INPUT.md`)

## The verification gate
Paste the result line of each command you ran. A claim with no command behind it is not accepted (`docs/ORCHESTRATION.md`).
- [ ] `powershell -NoProfile -File apps/vr/tools/build.ps1` exits 0
- [ ] `MageArena.*` automation tests green (README "Build and test"); `MageArenaDesign.*` may stay red by design (DF-001)
- [ ] `node apps/vr/tools/check-pin.mjs` prints `pin OK`
- [ ] `node apps/vr/tools/lint/unused-members.mjs` prints `unused-members OK`
- [ ] `node apps/vr/tools/lint/format-check.mjs` prints `format-check OK` (changed C++ lines only)
- [ ] `node .ai/doctor.mjs` reports 0 fail
- [ ] `node tools/check-commits.mjs` prints `check-commits OK` (subjects are `<card id>: ` or `type(scope): `)
- [ ] The card's own acceptance commands, run by the orchestrator, not only by the worker

## AI-assisted contributions
- Worker and model (Grok `grok-4.7`, `agy` + model, or the Claude session): <!-- -->
- Run report: `runs/<id>/REPORT.md` (git-ignored; quote what matters here)
- Commits carry a `Co-Authored-By` trailer for the model that wrote them
- Anything only a headset or the owner can judge (feel, fun, art) is listed here as **not verified**

## Notes for reviewers (human or agent)
<!-- Risky spots, follow-ups, and anything deliberately left out. -->
