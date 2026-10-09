# Kernel conformance proof, 2026-10-09: NOT MEASURED (build did not finish)

**Verdict against PROJECT-PLAN.md:513 (at least 20 green vectors plus one planted-mutation failure): not met by
this document. No test was run.** The worktree editor build did not finish before the 11:20 local cut-off, so the
green, mutated and restored runs are all 'not measured'. Everything below is what reading the code established.

## What happened (desktop editor, -nullrhi: none of it reached a run)

- Base commit `be7583a9b0221a44c894c382f297a88b00a92582`, worktree without the MetaXR plugins, 2026-10-09.
- Reconcile: `git grep` over master's `docs/` finds no planted-mutation result for `MageArena.Kernel.Conformance`
  (the only hit is the plan's own bar, `docs/PROJECT-PLAN.md:513`).
- No Unreal process ran at start. Command: `powershell -NoProfile -File apps/vr/tools/build.ps1`, started 10:45 local.
  It was at action 27 of 80 (cold build) when the 10 minute tool limit stopped it at about 10:55; it had printed
  no error. The next 11:09 check left 11 minutes, too few for a restart plus three runs and two more rebuilds
  (the 2026-10-07 cold build took about 393 s, this one was slower, about 20 actions in 16 minutes by the 11:01 log).

| Run | Vectors run | Passed | Exit code | Duration |
|---|---|---|---|---|
| Green | not measured | not measured | not measured | not measured |
| Planted mutation | not measured | not measured | not measured | not measured |
| Restored | not measured | not measured | not measured | not measured |

No mutation was planted; `git diff -- apps/vr/Game/` was empty throughout.

## Provenance (read from the code, not run)

- `apps/vr/data/conformance/` holds 45 files at the base commit.
- The vectors come from the reference TypeScript kernel, not the port: `apps/vr/tools/conformance/oracle.mjs:1`
  ("Runs the pinned TypeScript kernel and writes one JSON vector per scenario"), importing `kernel.ts` and
  `catalog.ts` from the extracted kernel (`oracle.mjs:20-21`). `generate.mjs:2` extracts the pinned kernel,
  `generate.mjs:96-99` checks the pinned bytes against `PINNED.json`, and `generate.mjs:124-125` runs the oracle.
  Neither script was run.
- Pinned data: `node apps/vr/tools/check-pin.mjs` prints `pin OK: 9 files @ baeac66`.
- Tolerance the test enforces: `Tests/KernelConformanceTests.cpp:1049-1050` rejects any vector whose tolerance is
  looser than 1e-6 relative / 1e-9 absolute ("tolerance ... is looser than 1e-6 / 1e-9"); the vector files carry the
  `tolerance` object it reads (`:1041-1045`). The test is registered at `:1330` as `MageArena.Kernel.Conformance`.

## Not proven

- Everything the bar asks for: vectors green, a planted mutation caught, the restored run.
- The device, a packaged build, and any vectors `v2/slice` adds.
- Whether the 45 files all count as vectors (some may be support files): the test's own count was not read.

## To finish

Re-dispatch with a warm build (or at least 40 minutes), then do steps 3 to 6 of the brief unchanged.
