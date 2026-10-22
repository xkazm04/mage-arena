# Overlay check - 2026-10-22

Delivers architecture-review finding A3(a) (`ARCHITECTURE-REVIEW-2026-10-21.md`, "Fix: two cards"). Tool:
`apps/vr/tools/check-overlay.mjs` (Node built-ins only, read-only over `apps/vr/data/`).

## Overlay sha256 of today's live overlay

```
overlay OK: 14 files
overlay sha256 d0ee5333db8992db01580107aa45ddd2c7ce0d016a63427129e17896947daecd
```

Computed over the sorted relative file list under `apps/vr/data/vr/`: for each file, its relative path, a newline, then its
bytes with CRLF folded to LF (so a Windows checkout with `core.autocrlf` names the same overlay as Linux CI). `--verbose`
prints the per-file sha256 behind it (same LF folding). Any overlay edit changes this number; quote it next to the pin hash
when a census or playtest result is recorded.

## What it covers

1. Every `.json` under `apps/vr/data/vr/`, recursively (14 files today, 13 of them JSON), parses. A failure is reported as
   `path: message (line, column)`. A leading U+FEFF is dropped first, as the C++ loader does.
2. Every CSV under `schools/` has the exact header the loader requires. Today that is `schools/spells-air.csv` against
   `AirHeader` (`KernelDataAir.cpp:30`, compared at `:348` after the BOM strip and `TrimEnd`). A new CSV under `schools/`
   with no header rule in the tool fails the check, so a new school cannot slip in unchecked.
3. One overlay sha256 (above).
4. `--self-test` copies the overlay to a temp directory, asserts the clean copy passes, then asserts the check fails on a
   truncated JSON, a JSON with a trailing comma and a CSV with a renamed column. It exits non-zero if any of them passes.

Seen red first: with the check logic stubbed out, `--self-test` printed `self-test failed`, `clean copy passes (14 files)`,
then `NOT DETECTED` for all three injected defects, exit 1. With the logic in, all three are detected, exit 0.

Wired into the `test` step of `.github/workflows/quality.yml` and into the pre-push hook (`tools/install-hooks.mjs`, hook
version 3: re-run `node tools/install-hooks.mjs` to pick it up).

## What it does NOT cover

- It does not reimplement the C++ loader's semantic rules. A file that parses can still fail `LoadVrRuleset`
  (`VrRulesLoad.cpp`): wrong `version`, missing keys, out-of-range numbers, a dais box that disagrees with
  `arena-layout.json`, CSV rows with the wrong cell count or an unknown enum. Only the Wolf strict loader says that.
- It does not check JSON shape (schemas) of any file, nor CSV cell values.
- It says nothing about live versus proposal (`calibration-proposal.json` is only checked to parse). Which variant a run
  used, and logging the overlay hash and variant at boot, is A3(b); it is folded into A1's Wolf card and is not part of this
  run.
- The header list is copied from the loader by hand (file:line cited in the tool). If the loader's column list changes, this
  tool fails loudly on the live CSV and must be updated with it.
