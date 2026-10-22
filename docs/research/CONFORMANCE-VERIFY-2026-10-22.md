# Conformance verifier, 2026-10-22

Plan goal 5, "Conformance held on every merge": the first half is `check-pin.mjs` (a gate in
`.github/workflows/quality.yml`); the second half, "the 45 vectors regenerate byte-identical", was a shell block re-typed
into each KPI note (K2a in `docs/research/KPI-2026-10-22.md`). It is now one tool.

**Tool:** `apps/vr/tools/conformance/verify.mjs` (Node built-ins only, no dependency). It copies `apps/vr/tools` and
`apps/vr/data` into an OS temp directory, drops the copied vectors, runs `generate.mjs` there against the TV clone, compares
each committed file in `apps/vr/data/conformance/` byte for byte with its regenerated twin, removes the temp directory, and
asserts `git status --short` is unchanged around the run. It never writes the repo.

**Env / options:** `--source <tv clone>` else `MAGE_ARENA_ARENA`; `--tsx <cli.mjs>` else `TSX_CLI`, else the tsx install inside
the clone (same candidates as `generate.mjs`). There is deliberately no default clone path.
`--if-available` turns "prerequisites absent" into a printed skip with exit 0 (the pre-push hook uses it).

**Exit codes**

| code | meaning |
|---|---|
| 0 | all N committed vectors are byte-identical to regenerated ones |
| 1 | a vector differs (each listed by name), the counts differ, nothing was compared, generation failed, or git status changed |
| 2 | usage error |
| 3 | prerequisites absent (no TV clone, or no tsx): nothing was compared, which is not a pass |

`--self-test` needs no clone and no network: equal fixtures report identical; a one-byte mutation of one fixture fails and
names that file; a missing and an extra regenerated file are named; the real no-prerequisites path exits 3 with a message
naming both missing items; `--if-available` skips cleanly.

**Hook:** `tools/install-hooks.mjs` is now v4. The pre-push gate runs `verify.mjs --self-test` always and
`verify.mjs --if-available` (the full comparison only when the prerequisites are present; otherwise it prints the skip reason
and does not block). `--check` / `--uninstall` and the preserved LFS delegation were exercised on a scratch repo. The whole
hook with the comparison skipped took 4.6 s here, inside the 10 s budget.

## Runs on this machine (Lynx, Windows ARM64, 2026-10-22)

```
$ node apps/vr/tools/conformance/verify.mjs --self-test
verify --self-test: OK (identical on equal bytes; a one-byte mutation named beta.json; missing/extra files named; absent prerequisites exit 3 with a message; --if-available skips)
exit 0

$ env -u MAGE_ARENA_ARENA -u TSX_CLI node apps/vr/tools/conformance/verify.mjs
verify: PREREQUISITES ABSENT, nothing was compared (this is not a pass):
  - the TV clone (pass --source <path> or set MAGE_ARENA_ARENA)
  - tsx (pass --tsx <cli.mjs> or set TSX_CLI)
exit 3

$ node apps/vr/tools/conformance/verify.mjs --source $TEMP/kpi-d4fde2ae/tv --tsx $TEMP/kpi-d4fde2ae/tsx/node_modules/tsx/dist/cli.mjs
verify: OK, all 45 committed vectors are byte-identical to regenerated ones.
exit 0   (about 3 s; git status --short unchanged)
```

A full comparison **was possible**: the TV clone (pin commit `baeac66`) and tsx left in `$TEMP/kpi-d4fde2ae` by the earlier KPI
run were reused read-only. This is a Node-side result only. No UE build, automation suite, capture, APK or device result is
claimed; the C++ `MageArena.Kernel.Conformance.*` side stays a Wolf measurement.
