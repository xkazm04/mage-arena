# Static analysis sweep 2026-10-20: the Node tools in `apps/vr/tools`

Second run of the `codebase-static-analysis-sweep` charter (the first, `SWEEP-2026-10.md`, covered `KernelData.cpp`).
Lynx (Windows ARM64) has no Unreal, so the scope is the Node tooling. Base `24c2043`; fix commit `139e832`.
Branch `autopilot/codebase-static-analysis-sweep-22ed77df`.

**Scope.** Edited: `clipgen/`, `glyphs/`, `hud/`, `sfx/`, `lint/`, `vocab-audit/`, `check-staged-clips.mjs`.
Read-only (nothing edited, nothing found worth reporting beyond the shared-helper note): `conformance/`, `check-pin.mjs`.
`apps/vr/data/**` untouched.

## 1. Method

1. A throwaway script (kept outside the repo, in `/tmp`) listed every top-level `function`/`const`/`class` of the 19
   in-scope `.mjs` files and counted whole-word occurrences in the file itself and across `apps/vr/tools`, `tools`,
   `.github`, `docs` (`.mjs .js .ps1 .yml .json .md`). Zero use outside the definition = candidate.
2. A second script listed every export of `clipgen/generate.mjs` (the `export { ... }` block) and `clipgen/corpus.mjs`
   and counted uses in the other tool files.
3. Entry points were traced by grep over `.ps1`, `.github/workflows/*.yml`, `CLAUDE.md`, `context-map.json` and docs.
4. Duplicated helper names across the files (`grep '^function'`, counts of names defined more than once).

## 2. Findings

| # | file:line (base) | Finding | Evidence of non-use | Disposition |
|---|---|---|---|---|
| 1 | `clipgen/corpus.mjs:513` `export function buildNoiseClips` | Exported function with no caller. It repeats the render loop of `writeNoise` (same `NOISE_PLAN`, `renderClip`, 36 s, quiet params) and returns frames instead of writing. | `grep -rn buildNoiseClips apps tools docs .github` found only the definition. The scan reported 1 occurrence in its own file, 0 elsewhere. No `import *` and no `import(` of `corpus.mjs` other than `generate.mjs:1747` (`corpus.writeTemplates` / `corpus.writeCorpus`). | **removed** (`139e832`, 18 lines) |
| 2 | `lint/unused-members.mjs:295` `MAGEARENAVR_API` | Scan hit. It is C++ text inside a self-test fixture string, not a JS symbol. | Inside a template literal of the self-test. | keep (false positive) |
| 3 | `clipgen/generate.mjs:1767` export list: `sub`, `poseHand`, `applyJitter`, `jointQuats`, `stabilize`, `distortAround`, `sampleWard`, `yawDir` | Exported, imported by no other tool file. All eight are used inside `generate.mjs` itself. | Whole-word count outside `generate.mjs` over `apps/vr/tools/**/*.{mjs,ps1}` is 0. Importers are only `corpus.mjs`, `stones.mjs`, `system-gesture.mjs`. | report-only: the list is the clip-synthesis library surface for the three opt-in scripts; trimming it removes no code and changes no output. |
| 4 | `clipgen/corpus.mjs` exports `TEMPLATE_BASE`, `IMPOSTOR_BASE`, `IMPOSTOR_KINDS`, `TEMPLATE_PLAN`, `sigilDuration`, `variedSigilParams`, `sampleVariedSigil`, `sampleImpostor` | Exported, no importer (`mousepaths.mjs` imports `SIGIL_CLASSES`, `MOUSE_BASE`, `buildVariedClip`, `removeLegacyClips`, `testClipsRoot`; `generate.mjs` uses `writeTemplates`, `writeCorpus` through dynamic import). Used inside `corpus.mjs`. | same scan as 3 | report-only (same reason; drop `export` if the owner wants a tight surface). |
| 5 | `hud/generate.mjs`, `sfx/generate.mjs`, `glyphs/generate.mjs`, `vocab-audit/audit.mjs`, `clipgen/stones.mjs`, `clipgen/system-gesture.mjs`, `clipgen/mousepaths.mjs` | No caller in any `.ps1` or workflow. | Callers found: only `CLAUDE.md` (mousepaths), `context-map.json`, docs/task cards. | keep: opt-in generators and audits run by hand; not dead. `quality.yml` runs `lint/unused-members.mjs` and `glyphs/check.mjs`; `tidy.ps1` runs `tidy-lines.mjs`. |
| 6 | duplicated helpers: `fail` x4 (`lint/*`, `vocab-audit`), `clamp01` x3 (`clipgen/generate.mjs:101`, `glyphs/paint.mjs:64`, `hud/generate.mjs:65`), `git` x2 (`format-check.mjs:79`, `tidy-lines.mjs:44`), `run` x3 (`format-check`, `sfx/generate`, `conformance/generate`), `esc` x3, `parseArgs` x6, `main` x10 | Small copies; no shared module exists. | `grep '^function'` name counts | report-only: each tool is a standalone script with Node built-ins only. A shared module would add a file and couple the gate tools (`lint/`) to the generators. Not worth it. |
| 7 | dead branches | None found by reading the unused-symbol hits. No unreferenced file in scope. | | none |

`check-pin.mjs` and `conformance/` were read only for their import graph. `conformance/oracle.mjs` imports the pinned TV
kernel dynamically at run time (`oracle.mjs:19-30`); that is by design and out of scope.

## 3. Before and after

Corpus note: the sigil corpus (gitignored) was absent in the fresh worktree, so the first `vocab-audit` reading ran on
an empty corpus. For a fair comparison the corpus was generated (`generate.mjs --corpus 30 --seed 1000`,
`mousepaths.mjs`) once with the old `corpus.mjs` and once with the new one; the two `audit.mjs` outputs are
byte-identical (`cmp`, 226 lines). `git status` stayed clean after each generation.

| Command | Before (`24c2043`) | After (`139e832`) |
|---|---|---|
| `node apps/vr/tools/check-pin.mjs` | `pin OK: 9 files @ baeac66`, exit 0 | same |
| `node apps/vr/tools/check-pin.mjs --self-test` | not run before | exit 0 (prints `corruption detected: docs/design/baseline-fourteen-nights/design/data/combat.json`, the planted corruption it expects) |
| `node apps/vr/tools/check-staged-clips.mjs` | `staged clips OK: 111 files, 25776936 bytes` | same |
| `node apps/vr/tools/clipgen/validate.mjs` | `111 clips, 0 violations` | same |
| `node apps/vr/tools/glyphs/check.mjs` | exit 0 | identical output |
| `node apps/vr/tools/vocab-audit/audit.mjs` | exit 0 (empty corpus) | with corpus: identical old vs new (above) |
| `node apps/vr/tools/lint/format-check.mjs` | `format-check OK: 0 changed C++ file(s) since 24c2043 (clang-format version 19.1.5)` | same |
| `node apps/vr/tools/lint/unused-members.mjs` | `unused-members OK: 555 private/protected members in 39 classes` | same |
| `node tools/check-commits.mjs` | `0 commit subject(s) since 24c2043` | `1 commit subject(s) since 24c2043`, OK |
| `node --test tools/agy-guard.test.mjs` | not run | 30 pass, 0 fail |

Everything ran on Lynx; none needed Unreal. The Unreal suite was not run and is not claimed.
The `--corpus` run still wrote `5 noise clips (180.000s)`, so `writeNoise` is unaffected.
