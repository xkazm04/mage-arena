---
product: "Mage Arena VR"
stack: "Unreal Engine 5.8.3 (CL 58210709) C++ game under apps/vr/Game/, forward shading, Meta Quest 3 target; zero-dependency Node .mjs tooling under apps/vr/tools/ and tools/; combat data is a frozen TS-kernel pin (apps/vr/data/pinned/) plus a VR overlay (apps/vr/data/vr/)"
vault: []                        # none on this machine -> <repo>/.perfect/ (gitignored)
vault_subdir: Perfect
base_branch: master
wave_size: 4                     # owner cap, 2026-10-21: 4 builder subagents concurrent, never more
lot_caps: {}
pool_target: 40                  # one-pass full sweep, not an accumulating pool - see "Round shape"
round_shape: pool
cooldown_rounds: 2
commit_format: "<type>(<context>): <title>"   # type from tools/check-commits.mjs TYPES; card work uses "<card id>: <summary>"
context_map: context-map.json
active_runs_ledger: ""
locale_count: 1
---

# /perfect overlay for Mage Arena VR

Written 2026-10-21 on **Lynx** (Windows 11 ARM64, no Unreal install - `docs/DECISIONS.md` 2026-10-20 "Unreal host").
Everything below was derived from THIS repo (`CLAUDE.md`, `docs/DECISIONS.md`, `docs/ORCHESTRATION.md`,
`.github/workflows/*.yml`, `.ai/manifest.yaml`, `.gitignore`, `tools/check-commits.mjs`) and from gate commands
actually run here, not imported from another project's overlay.

## The machine split - read this before planning any wave

All Unreal work (builds, the `MageArena*` automation suite, content, packaging) runs on **Wolf**, the x64 host.
Lynx has no Unreal install, so **no wave on Lynx can gate a C++ change by compiling it.** The owner has asked
(2026-10-21) for a real attempt at hosting UE on Lynx before accepting that split permanently; until that attempt
returns a verdict, the rule below holds.

- **Lynx-verifiable** (a wave may build AND gate these): `apps/vr/tools/**` and `tools/**` Node tooling, the data
  overlay `apps/vr/data/vr/`, `.ai/**`, `.github/workflows/**`, docs, and task cards.
- **Wolf-owed** (a wave may propose and triage these, and write the card, but may NOT claim a verified C++ result):
  everything under `apps/vr/Game/`. A C++ direction lands as a task card in `apps/vr/tasks/` per
  `docs/ORCHESTRATION.md`, not as an unverified commit.
- Never claim a UE build or test result from Lynx (`DECISIONS.md` 2026-10-20, verbatim: "never claim UE results from Lynx").

## Round shape

The owner's shape (2026-10-21) is a **single full sweep**, not an accumulating pool: scout every in-scope context up
front, then triage in one sitting. Only **L and XL** directions go to the owner's gate; **S and M** are recorded
straight into the backlog without a gate. After triage, accepted work drains through builders, 4 concurrent at most,
**Opus for L/XL lots, Sonnet for S/M lots**.

## Scope

In scope (14 contexts): `combat-sim-core`, `spell-schools-and-ai`, `combat-data-pin`,
`conformance-scenarios-schools`, `hand-input-pipeline`, `sigil-recognition`, `duel-session-core`,
`session-capture-harnesses`, `arena-greybox`, `budget-fx-stress`, `kernel-and-sim-tests`,
`session-and-input-tests`, `build-config`, `quality-lint-and-guards`, plus the provisional context
`vr-node-tooling` (the 25 `apps/vr/tools/` scripts no mapped context claims - see "Context sources").

Out of scope this round (owner, 2026-10-21, mechanics-first): `cuff-hud-art`, `glyph-and-rune-art`,
`sfx-imported-assets`, `sfx-wav-sources-a`, `style-studies-gallery`. `CLAUDE.md`'s current phase is greybox-only
and `apps/vr/tasks/BACKLOG.md` already parks the art items for the art pass.

## Gates

All of the `always` set was run on Lynx on 2026-10-21 and was **green in under 10 s total**. That is the integration
gate; it is confirmation, not discovery.

- always: `node apps/vr/tools/lint/unused-members.mjs && node apps/vr/tools/lint/unused-members.mjs --self-test`,
  `node tools/check-action-pins.mjs`, `node --test tools/agy-guard.test.mjs` (~5 s),
  `node apps/vr/tools/check-pin.mjs && node apps/vr/tools/check-pin.mjs --self-test`,
  `node tools/check-commits.mjs`, `node .ai/doctor.mjs`.
- when any C++ under `apps/vr/Game/Source/` changed: `node apps/vr/tools/lint/format-check.mjs --self-test && node
  apps/vr/tools/lint/format-check.mjs --base origin/master` (clang-format 19.1.5 resolves here even though it is not
  on PATH - verified 2026-10-21).
- when `apps/vr/data/**` changed: `node apps/vr/tools/check-pin.mjs` must still report `pin OK: 9 files @ baeac66`
  and `conformance OK: 45 vectors`. `apps/vr/data/pinned/**`, `apps/vr/data/PINNED.json` and
  `apps/vr/data/conformance/**` are never edited (`.ai/manifest.yaml` `boundaries.neverTouch`).
- when a packaged build or staged clips changed: `node apps/vr/tools/check-staged-clips.mjs` must print OK.
- when a `.github/workflows/*` changed: `node tools/check-action-pins.mjs` (every action pinned to a commit SHA with
  a version comment).
- **UE gate - Wolf only, NOT runnable here**: `powershell -NoProfile -File apps/vr/tools/build.ps1` and the
  `MageArena*` / `MageArenaDesign*` automation suites. A wave that touches `apps/vr/Game/` is reported
  **compile-unverified** and its card states the proof Wolf must run. Known-red on a clean tree as of 9f338f0:
  `MageArenaDesign.Duel.FireSeated`, `Duel.AirSeated`, `Session.FullSeated`, `Session.Wave1Seated`,
  `Session.CreaturesSeated` (owner ruling 2026-10-16: `Duel.AirSeated` and `Session.CreaturesSeated` are not new
  regressions).
- visual: **none available on Lynx.** There is no renderer here, so any wave touching presentation, greybox or FX is
  reported **visually unverified** and the card names the captures Wolf must take.
- builder: the `always` set, plus the `when` gates covering files in the builder's own write set. A builder never
  runs `build.ps1` and never claims a UE result.
- Before the full suite in a fresh checkout (Wolf): regenerate the gitignored sigil corpus with
  `node apps/vr/tools/clipgen/generate.mjs --corpus 30 --seed 1000` then
  `node apps/vr/tools/clipgen/mousepaths.mjs`. Never run `generate.mjs` bare or with `--templates` - both rewrite
  tracked clips. **Enforced since f33bf18:** a bare run exits 2 and rewriting tracked clips needs `--canonical`.
  The safe verification command is `node apps/vr/tools/clipgen/generate.mjs --check`, which regenerates into a
  temp directory and diffs against the tracked clips; add it to the `when clips changed` gate.

## Class B

Append-only or anchored-insert only; re-read immediately before each edit and anchor on a string unique to your change.

- `apps/vr/tasks/BACKLOG.md` - append a bullet; never rewrite the list.
- `docs/submission/ASSET-LEDGER.md` - append a row (an asset with no ledger row does not ship, DECISIONS 2026-10-21).
- `docs/REPO-LAYOUT.md`, `README.md`, `docs/PROJECT-PLAN.md` - edit only the section your direction owns.
- `.github/workflows/quality.yml` - anchored insert of a new step only.
- `.ai/consults.jsonl`, `.ai/applied.jsonl`, `.personas/memory-outbox.jsonl` - append-only JSONL, never rewritten.

## Class C

Director-only; builders *report* what they need and the Director applies it once at quiescence.

- the git index, `context-map.json`, `.personas/**` (written by the Personas app).
- `docs/DECISIONS.md` - the owner's record. No worker and no builder edits it (`docs/ORCHESTRATION.md` role table).
- `apps/vr/data/pinned/**`, `apps/vr/data/PINNED.json`, `apps/vr/data/conformance/**`, `apps/vr/Game/Clips/**` -
  `.ai/manifest.yaml` `boundaries.neverTouch`; regenerated byte-identically by their generators, never hand-edited.
- `.ai/manifest.yaml`, `.claude/perfect/config.md`, `.gitignore`.
- `apps/vr/Game/Source/MageArenaVR/MageArenaVR.Build.cs` - its two `if (Target.bBuildEditor)` blocks stay separate
  by owner ruling (DECISIONS 2026-10-16). Any edit here is the Director's.

## Repo law

Pasted verbatim into every brief.

- Read `CLAUDE.md`, then `docs/DECISIONS.md` **newest first**, then `docs/PROJECT-PLAN.md`. DECISIONS.md outranks
  every other document in the repo; where the plan and DECISIONS disagree, DECISIONS wins.
- **Combat data:** the pin in `apps/vr/data/PINNED.json` is VR's frozen baseline (DECISIONS 2026-10-21). Never edit
  `apps/vr/data/pinned/**` in place and never re-pin from TV. Every combat-data change goes in the VR overlay
  `apps/vr/data/vr/`. `check-pin.mjs` and the 45 conformance vectors stay a merge gate. A `combat.json` copied from a
  contest folder is stale - do not use it.
- **Current phase (until 2026-10-31): desktop core, no VR, greybox only.** Keyboard plays recorded hand clips into
  the one gesture pipeline; the mouse draws sigils. Never add a shortcut that skips the pipeline and call it a
  gesture test (`docs/DESKTOP-INPUT.md`). No art work before the mechanics work together.
- **Threat colour language holds even in greybox:** element colour = absorb, steel = dodge, black core with a red
  rim = leave. Black-with-red-rim is reserved for the unblockable threat and its telegraph - no other effect, prop
  or UI may use that combination (`apps/vr/tasks/BACKLOG.md`, art rule).
- Forward shading is on from day one; it is what the Quest renderer uses. Draw calls are ONE total budget of <=150
  for the whole frame, with no separate particle line (DECISIONS 2026-10-16).
- **Commits:** `<card id>: summary` (T08, A02, CR-003, DF-001) or `<type>(<scope>): summary` with type in
  feat, fix, docs, style, refactor, perf, test, build, ci, chore, revert. `node tools/check-commits.mjs` and
  `.github/workflows/commits.yml` enforce it. Small atomic commits.
- Binary assets (`.uasset`, `.umap`, FBX, PNG) go through Git LFS. `Binaries/`, `Intermediate/`, `Saved/`,
  `DerivedDataCache/` and `runs/` are never committed.
- Paid services: ElevenLabs only. Anything else needs the owner's written approval (`docs/ORCHESTRATION.md`).
  No new runtime dependencies; the Node tooling is zero-dependency `.mjs` and stays that way.
- **pof** (`kiro/pof`) is the studio tool this game is built with. When pof lacks a capability the game needs, the
  plan is to build it in pof, not to work around it here (PROJECT-PLAN section 8).
- Reuse before building: grep `apps/vr/Game/Source/MageArenaVR/` and `apps/vr/tools/` for an existing mechanism
  first. The kernel's tuned constants live in `SimConstants.h` with a citation back to the pinned TS source - a
  hard-coded number without that citation is a finding.
- **Out-of-scope walls:** `docs/DECISIONS.md`, the pinned data and conformance trees, `apps/vr/Game/Clips/**`,
  `.env*`, `runs/`, `kiro/mage-arena-archive/`, and any other repository. `git push` only when the owner says so.

## Context sources

`context-map.json` (generator `personas-context-scan`, `generated_at` 2026-10-20T19:33Z, version 2, 19 contexts /
201 files / 10 groups) is both the queue and the name source for `.personas/memory-outbox.jsonl` coverage nodes.
The semantic scan runs inside the Personas desktop app and cannot be triggered from a CLI session.

Audited on Lynx 2026-10-21 against `git ls-files` (1074 tracked files): **0 dead paths**, 25 unmapped source files
(all `apps/vr/tools/**` scripts, plus 2 under `.ai/`). Nine contexts sit below the app's own 10-30 files-per-context
band, all of them the 1-2 file asset contexts - a semantic rescan would likely consolidate them. Treated as fresh
enough to be the queue; the unmapped tool scripts are scouted as a **provisional** context `vr-node-tooling` which
the app does not know, so its coverage nodes are attributed to `build-config` instead.

Docs mapped per context: `docs/PROJECT-PLAN.md` (all), `docs/DESKTOP-INPUT.md` (hand-input-pipeline,
sigil-recognition), `apps/vr/data/README.md` (combat-data-pin, conformance-scenarios-schools),
`docs/research/BUDGET-DG4-2026-10.md` and `docs/research/MOBILE-INSTANCING-2026-10.md` (budget-fx-stress,
arena-greybox), `docs/design/SCHOOL-DEFENCES.md` (spell-schools-and-ai), `docs/concepts/WOLF-PLAN.md` (build-config).

## Opportunity arcs

- The one hard date is the **Meta VR Start Developer Competition deadline, Wed 18 Nov 2026, 12:00 PT**. Every other
  dated gate in the plan is a soft internal milestone (DECISIONS 2026-10-21); a missed soft date is a re-plan.
- The active arc is the **31 Oct desktop-core gate**: the whole game playable on PC through the one gesture
  pipeline, greybox, before any VR work.
- `docs/concepts/WOLF-PLAN.md` is the run order for the x64 host and binds the ORDER of UE work.
- Lessons that generalize go to the ai-registry (PROJECT-PLAN section 9); upper knowledge layers never name this
  product.

## Vetoes

Settled or already-captured - never re-suggest as a new direction. **Check all three before proposing:**

- `docs/research/MOONSHOT-2026-10.md` - a prior sweep with **12 accepted L/XL items M1-M12**, each with a write set
  and acceptance cases, plus **3 declined titles that are a durable "no"** and ~40 untriaged incidental S/M defects.
  Several M items are already carded: M3 = T35 (input clock), M4 = T36 (session tick and journal), M6 = T33 (kernel
  data injection), M7 = T34 (portable kernel math). Re-proposing an M item is waste; extending one is a card update.
- `apps/vr/tasks/BACKLOG.md` - 37 lines of found-between-cards items, many `[x]` done with the fixing card named.
- `apps/vr/tasks/T*.md` - T20-T42 are live cards. An idea that is already a card is not a direction.
- TV as a product is obsolete; nobody re-pins from TV and `docs/change-requests/CR-*` are VR design records, not
  requests to TV (DECISIONS 2026-10-21).
- A separate particle draw-call allowance (DECISIONS 2026-10-16). One total of <=150, no second line.
- A UE install on Lynx was ruled out 2026-10-20; the owner reopened it 2026-10-21 as an attempt, so a *feasibility
  probe* is allowed, but "move UE work to Lynx" is not a proposable direction until that probe returns a verdict.

## User taste

- **Evidence over claims.** `docs/ORCHESTRATION.md`: "accepts a Grok 'done' without running the checks" is the one
  thing the orchestrator never does. A claim with no command behind it is not accepted. This repo's own docs mark
  anything unchecked as `[verify on Wolf]` - match that discipline exactly.
- Outcome-value over cosmetic churn. Engine depth, not chrome: the data model, the kernel, the gesture pipeline, the
  session graph, the conformance contract. The greybox is deliberately ugly and making it less ugly is not a direction.
- The owner wants only **L and XL** at the gate (2026-10-21); S and M are recorded, not discussed.
- Mechanics before art until the 31 Oct desktop-core gate.
- The owner re-tests their own rulings when the facts change (the UE-host ruling, 2026-10-21). A ruling is not a wall
  to hide behind - but it binds until they move it.

## Skill improvement log

- 2026-10-21 (adoption round, Lynx): the overlay's first job in a two-machine repo is the **machine split**. The
  method assumes the Director can gate what its builders write; here 10 of 19 contexts compile only on another box,
  so `## Gates` needed a third state beyond green/red - *ungateable here* - and the brief needed a rule that a
  C++ direction lands as a task card, not a commit. Worth promoting to the method: a gate set should declare what it
  CANNOT verify, because a silent absence reads as a pass.
- 2026-10-21: `scripts/link-registry.mjs` linked nothing until the project had a checkout for THIS machine in the
  registry's `projects.json` - and `register-project.mjs` correctly refused because the registry checkout held a
  sibling session's uncommitted file. Adoption in a linked-skill fleet has a per-machine prerequisite the skill does
  not mention.
- 2026-10-21: this repo arrives with a dense prior campaign (`MOONSHOT-2026-10.md` M1-M12 + 37 BACKLOG lines + 23
  live cards). The `## Vetoes` section is doing more work than `## Gates` here: the cheapest way to waste a sweep
  would be re-deriving M1-M12.
