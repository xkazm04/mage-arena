# Repository layout: two repositories, one checkout each

Owner decision 2026-10-07 (`DECISIONS.md`). It supersedes `MONOREPO.md`. The same text is in both repositories.

## The two repositories

| Local folder | GitHub | Channel | Owns |
|---|---|---|---|
| `kiro/mage-arena-tv` | `xkazm04/mage-arena-tv` (was `mage-arena-pc`) | TV / desktop (TypeScript, PixiJS) | **The canon:** combat data and its calibrated simulator (`packages/core`), the story and quests (`story/`, `docs/campaign`), the TV game, its art and audio |
| `kiro/mage-arena-vr` | `xkazm04/mage-arena-vr` (was `mage-arena`) | VR (Unreal 5.8, Meta Quest) | The VR game, the VR data overlay (`apps/vr/data/vr/`), VR-only docs (`docs/gameplay`, `docs/campaign`) |

There is no third repository. The PC channel, when it starts, joins the TV repository (it shares the TypeScript code).
GitHub keeps redirects from the old names.

## What is shared, and how

- **VR consumes a pinned commit of TV**, never a working tree: `apps/vr/data/PINNED.json` plus
  `node apps/vr/tools/check-pin.mjs --source C:/Users/kazda/kiro/mage-arena-tv`. Story and quest data will be pinned the
  same way once TV commits them.
- **Changes flow back as change requests** (`mage-arena-vr/docs/change-requests/`), applied by the TV side, then re-pinned.
- **Decisions:** a decision that changes every channel is recorded in the TV repository (the canon owner) and quoted in
  the VR `DECISIONS.md`; a VR-only decision lives only in VR.

## Parallel work

The long-lived lane folders (`mage-arena-arena`, `-art`, `-audio`, `-core`, `-int`) are retired. Parallel work uses
short-lived worktrees under `.worktrees/` (gitignored) in the repository that owns the work, merged to `main` and removed.

## What stays out of git

- **Review evidence media** (screenshots, comparison proofs, review boards' images, provider raw and upscaled
  generations, audition audio) lives in the outside archive `kiro/mage-arena-archive/<channel>/`, at the same relative
  paths, with a `MANIFEST.json`. Reports (`.md`, `.json`, `.html`, `.jsonl(.gz)`) stay tracked and refer to it by path.
- **Shipped media goes through Git LFS**: TV `*.png, *.jpg, *.mp3, *.wav, *.ttf, ...`; VR `*.uasset, *.umap` and the rest of
  `.gitattributes`.
- Build output, dependencies, local runtime state and secrets are gitignored (`node_modules/`, `dist/`, Unreal
  `Binaries/ Intermediate/ Saved/ DerivedDataCache/`, `.director-runtime/`, `.env*`, `runs/`, `.personas/`).

## The 2026-10-07 restructure (what was done)

1. TV: the `art` and `audio` lanes were merged into `integration`, and `integration` into `main`. The plan's wave table
   kept integration's newer statuses; other doc conflicts kept both sides.
2. 2,161 evidence media files (4.09 GB) were copied to `kiro/mage-arena-archive/tv/`, then removed from all TV history with
   `git filter-repo`. Media still in use moved to LFS with `git lfs migrate import`. The TV repository went from 4.4 GB to
   an 8 MB pack plus 376 MB of LFS objects. The TV gate passes (32 test files, 215 tests, 0 contradictions) and
   `build:game` succeeds.
3. TV commit hashes from before the rewrite changed, except the 13 docs commits already on GitHub. The old-to-new map is
   `mage-arena-tv/docs/history-rewrite-2026-10-07.tsv`. Hashes the VR docs cite: `68a4d68` (the VR pin) is `baeac66`,
   `b1efd46` (integration) is `1ccfd04`, `a7964ad` is unchanged.
4. The local-only gitignored data of the old lane folders (`art/raw/`, the core lane's `docs/waves/W1-evidence/*.jsonl`
   and `.director-runtime/`) was copied to the archive too. The old folders were moved to `kiro/_retired-2026-10-07/`;
   delete them once nothing is missing.
