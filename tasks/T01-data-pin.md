# T01 - Pin the combat data from the Mage Arena core project

Status: open
Max turns: 60

## Goal
The VR game consumes the tuned combat data owned by the Mage Arena core project, copied from ONE commit and
protected by a checksum gate, so every later task (kernel port, conformance tests) reads numbers that provably match
the calibrated source. Plan: `docs/PROJECT-PLAN.md` section 3 and W0 row of section 5; rules in `data/README.md`.

## Read first
- `CLAUDE.md`, `docs/DECISIONS.md`, `data/README.md`
- The source repository is `C:\Users\kazda\kiro\mage-arena-arena` (git worktree, branch `arena`). **Pinned commit:
  `68a4d68`** (resolve it to the full 40-character SHA with `git -C <source> rev-parse 68a4d68`).

## Deliverables (exact paths)
- `data/pinned/` - byte-exact copies of these files **as they are in commit 68a4d68**, read with
  `git -C C:\Users\kazda\kiro\mage-arena-arena show 68a4d68:<path>` (NEVER from the working tree), keeping each file's
  path relative to the source repo root under `data/pinned/`:
  - `docs/design/baseline-fourteen-nights/design/data/combat.json`
  - `docs/design/baseline-fourteen-nights/design/data/enemies.json`
  - `docs/design/baseline-fourteen-nights/design/data/schools.json`
  - `docs/design/baseline-fourteen-nights/design/data/arena-tiers.json`
  - `docs/design/baseline-fourteen-nights/design/data/spells-water.csv`
  - `docs/design/baseline-fourteen-nights/design/data/stats.csv`
  - `docs/design/reference-the-ledger/design/data/spells-fire.csv`
  - `packages/core/src/arena/data/runtime.json` (the compiled runtime data the calibrated TS kernel actually uses)
  - `art/scale-contract-v1.json`
- `data/PINNED.json` - `{ "source": "mage-arena (worktree mage-arena-arena, branch arena)", "commit": "<full sha>",
  "copied": "<ISO date>", "files": [ { "path": "<path under data/pinned/>", "sha256": "<hex>", "bytes": <n> } ] }`.
- `tools/check-pin.mjs` - Node (built-ins only) gate:
  - default mode: recompute sha256 of every file listed in `PINNED.json`; fail (exit 1) on any mismatch, missing file,
    or an unlisted file under `data/pinned/`; print `pin OK: <n> files @ <short sha>` on success.
  - `--source <path>`: additionally compare each file with `git -C <path> show <commit>:<path>` byte for byte.
  - `--self-test`: copy `data/pinned` to a temp dir, flip one byte, prove the check fails, clean up; exit 0 only if
    the corruption was detected.
- `data/README.md` - replace the "Not pinned yet" line with a short "Current pin" section (commit, date, file count,
  how to verify, how to re-pin).

## Constraints
- Do not commit, push, or modify ANY file in `C:\Users\kazda\kiro\mage-arena-arena` or any other repository.
- Byte-exact copies: do not reformat, re-indent or change line endings of the pinned files. Write bytes from
  `git show` straight to disk (beware of shells that convert line endings; verify with sha256 against `git show`).
- Do not call paid services. Do not edit `docs/DECISIONS.md`.

## Acceptance (the orchestrator re-runs these; quote their real output in your report)
```
node tools/check-pin.mjs                                            # expected: pin OK: 9 files @ 68a4d68
node tools/check-pin.mjs --source C:/Users/kazda/kiro/mage-arena-arena   # expected: pin OK, source match for 9 files
node tools/check-pin.mjs --self-test                                # expected: corruption detected, exit 0
```

## Report
Write `runs/T01/REPORT.md`: the full commit SHA, the file list with sizes, each acceptance command with its real
output, and anything surprising in the data (for example a value in `combat.json` that no longer matches
`runtime.json`).
