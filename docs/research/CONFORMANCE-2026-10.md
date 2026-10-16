# Kernel conformance proof, 2026-10-16: MEASURED (45 of 45 green, planted mutation fails 3, restored green)

**Verdict against PROJECT-PLAN.md:513 (at least 20 green vectors plus one planted-mutation failure): met.** 45 vectors
ran and passed on the unmodified kernel; one changed constant in the kernel made 3 of them fail; with the constant put
back, all 45 pass again.

- Build hash (base commit, `git rev-parse HEAD`): `0a060b78911f3294898294f241b942b5218ab7c7`, worktree without the
  MetaXR plugins, UE 5.8, Win64 Development editor target, 2026-10-16 (local CEDT).
- Reconcile: `git grep -i mutation` over `docs/` outside this file found no earlier measured conformance result; the only
  hit for the bar is `docs/PROJECT-PLAN.md:513`.

## Commands and exit codes

| Step | Command | Exit | Duration |
|---|---|---|---|
| Pin | `node apps/vr/tools/check-pin.mjs` (prints `pin OK: 9 files @ baeac66`) | 0 | instant |
| Corpus | `node apps/vr/tools/clipgen/generate.mjs --corpus 30 --seed 1000`, then `node apps/vr/tools/clipgen/mousepaths.mjs` (270 corpus, 5 noise, 180 impostor, 90 mouse clips; `git status` stayed clean) | 0 | seconds |
| Cold build | `powershell -NoProfile -File apps/vr/tools/build.ps1` (107 actions) | 0 | 194 s (19:06:49 to 19:10:05) |
| Mutation build | same command | 0 | about 93 s |
| Restore build | same command | 0 | about 153 s |
| Each test run | `UnrealEditor-Cmd.exe <worktree>/apps/vr/Game/MageArenaVR.uproject -ExecCmds="Automation RunTests MageArena.Kernel.Conformance; Quit" -unattended -nullrhi` | see table | see table |

All builds and runs were background commands polled until they exited.

## Three runs

| Run | Vectors run | Passed | Failed | Exit code | Duration (wall, incl. editor start) |
|---|---|---|---|---|---|
| Green (unmodified) | 45 | 45 | 0 | 0 | 147 s (first launch after the cold build) |
| Planted mutation | 45 | 42 | 3 | 255 | 18 s |
| Restored | 45 | 45 | 0 | 0 | 17 s |

The test's own count: `Found 45 automation tests based on 'MageArena.Kernel.Conformance'`. It registers one test per
`*.json` file in `apps/vr/data/conformance/` (`KernelConformanceTests.cpp:1336`), and the 45 test names match the 45 file
names exactly (checked with `diff`), so every file counts as a vector; none is a support file. The log ends with
`TEST COMPLETE. EXIT CODE: 0` on the green and restored runs.

## Planted mutation

One constant in `apps/vr/Game/Source/MageArenaVR/Kernel/SimConstants.h`: `TierCap` (`kernel.ts:216`) from `4` to `5`
(the highest tier the tier clock may reach). Rebuilt, run. Failing vectors:

- `creature-bout`: `tick 3276 pre-ops state.eventCount: expected 211 got 213`
- `duel-competence-1-5`: `tick 3300 pre-ops state.eventCount: expected 346 got 348`
- `soldiers-bout`: `tick 3300 pre-ops state.eventCount: expected 238 got 240`

The other 42 vectors still passed, so the mutation was caught by long bouts that reach the cap, not by every vector.
After the run the constant was put back, the file was byte-identical to HEAD, and the restore build and run above passed.
`git diff -- apps/vr/Game/ apps/vr/data/` is empty and `git status` is clean; `check-pin.mjs` still prints
`pin OK: 9 files @ baeac66`. No source change is committed; this document is the only change.

## Provenance (from the code)

- The vectors come from the reference TypeScript kernel, not the port: `apps/vr/tools/conformance/oracle.mjs:1` ("Runs
  the pinned TypeScript kernel and writes one JSON vector per scenario"), importing `kernel.ts` and `catalog.ts` from
  the extracted kernel (`oracle.mjs:20-21`). `generate.mjs:96-99` checks the pinned bytes against `PINNED.json`. Neither
  script was run in this proof; the committed vectors were used as they are.
- Tolerance the test enforces: `Tests/KernelConformanceTests.cpp:1049-1050` rejects any vector looser than 1e-6
  relative / 1e-9 absolute. The test is registered at `:1330` as `MageArena.Kernel.Conformance`.

## Not proven

- Only one mutation was planted, and it is caught by 3 vectors. The other 42 are not shown to catch anything; a
  per-vector sensitivity map would need more mutations (rules in air, heat, water, enemies, mage-ai).
- The vectors were not regenerated from the TypeScript oracle in this run, so agreement with a fresh oracle run is
  unmeasured; the pin check shows the pinned data bytes only.
- The device, a packaged build, and any vectors `v2/slice` adds.
- The full `MageArena` suite was not run (the known failures `FireSeated`, `FullSeated`, `Wave1Seated`, `AirSeated`,
  `CreaturesSeated` belong to another lane).
- Durations: the green run's 147 s includes the first editor launch after a cold build; the 17 to 18 s runs are warm.

## History

The 2026-10-09 attempt (base `be7583a`) measured nothing: its cold worktree build was still at action 27 of 80 when the
10 minute foreground tool limit stopped it, and the three runs were recorded as not measured. This run did the same
steps with the build and test runs as background commands; the cold build took 194 s.
