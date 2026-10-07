# One repository for every Mage Arena channel

> **Superseded 2026-10-07.** The owner chose two repositories (TV and VR) instead; see `REPO-LAYOUT.md` and
> `DECISIONS.md`. Phase A below happened and stays as history; Phase B will not happen.

Owner decision 2026-10-02 (`DECISIONS.md`): "merge the codebases so we don't split folder for each subproject". The
home is **this public repository**, renamed from `mage-arena-vr` to **`mage-arena`** on GitHub. VR moves first; the
PC/TV lane branches fold in later, at a quiet point of their own orchestrator.

## Target layout

```
mage-arena/                       github.com/xkazm04/mage-arena (public)
  apps/vr/                        Unreal project (today's Game/), VR tools, VR task cards
  apps/pc-tv/                     TypeScript/PixiJS client (from the PC/TV project's lane branches)
  packages/core/                  deterministic TS combat kernel + the single source of combat data
  art/  audio/                    assets, one folder per channel where they differ
  docs/                           DECISIONS.md (all channels), one plan per channel, shared design canon
  tools/                          repo-wide tools (orchestration, worker dispatch, checks)
```

When `packages/core/` lives in the same repository, the VR build reads the combat data by path and the pin
(`data/PINNED.json`, `tools/check-pin.mjs`) becomes a conformance check against `packages/core` instead of a copy.

## Phase A - this repository becomes the monorepo shell - DONE 2026-10-03

Done: `Game/`, `data/`, `tasks/` and the VR tools moved to `apps/vr/` with history (120 renames); every path updated;
all gates re-run green (pin, 608 clips, build, 7/7 `MageArena.*` tests, 63 committed clips byte-identical); GitHub
repository renamed `mage-arena`. Historical task cards T01-T03 keep the paths they were written with.

Done by the orchestrator, not a worker (it is history-structural work):
1. Move `Game/`, VR-only `tools/` (`build.ps1`, `clipgen/`) and `tasks/` under `apps/vr/`; keep `docs/` and the
   orchestration tools (`grok-run.sh`, `agy-run.sh`, `agy-guard.mjs`) at the root; `git mv` so history follows.
2. Update every path: `tools/build.ps1`, card templates, `.gitattributes` (`apps/vr/Game/Clips/**`), `.gitignore`,
   `.agents/hooks.json`, `CLAUDE.md`, `README.md`, `docs/*.md`, the antigravity write allow-list
   (`write_file(<repo root>)` stays valid).
3. Re-run every gate: build, `MageArena.*` automation tests, clip validation, pin check.
4. `gh repo rename mage-arena` (GitHub keeps redirects from the old name); update `origin`. Push only when the owner asks.
5. The local folder stays `kiro/mage-arena-vr` until Phase B, because `kiro/mage-arena` is still the PC/TV repository.

## Phase B - the PC/TV project joins (at its orchestrator's quiet point, owner-scheduled)

1. Before anything goes public, scan the PC/TV history for secrets and for anything the owner wants kept private.
2. Import its history with `git subtree add` (or a filter-repo prefix move) into `apps/pc-tv/` and `packages/core/`,
   from its integration branch once its lanes (arena, art, audio, core) are merged there.
3. Retire its worktree folders (`mage-arena-arena`, `-art`, `-audio`, `-core`, `-int`) and its standalone repository
   only after its orchestrator is re-pointed at the monorepo; then rename the local folder to `kiro/mage-arena`.
4. Replace the VR data pin with a path dependency on `packages/core` plus the cross-runtime conformance vectors.
5. Move the registry matrix rows' evidence links to the new paths.

## What does not change

- One orchestrator per channel can still run; channels work in their own `apps/<channel>/` folder and isolate
  concurrent work with short-lived worktrees that are removed after merge, not with long-lived lane folders.
- The commit identity is `xkazm04`; nothing is pushed without the owner's request.
