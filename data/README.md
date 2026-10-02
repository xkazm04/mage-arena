# Pinned combat data

The authoritative combat data (spells, enemies, tier clock, Flow, arena tiers) is owned by the Mage Arena core
project, `kiro/mage-arena` and its arena worktree, where a headless simulator calibrated it (plan section 3).

This folder holds a **copy pinned to one committed revision** of that data. The manifest is `PINNED.json`
(`{ "source", "commit", "copied", "files": [ { "path", "sha256", "bytes" } ] }`). Rules:

- Copy only from a **commit**, never from a working tree (the source has uncommitted work in progress).
- Never edit the copy. A change is a change request to the core project, then a re-pin.
- The contest copy under `firetv/.contest/arena/mage-arena/` is **stale** (pre-calibration numbers). Do not use it.

## Current pin

- Commit `68a4d68d315856c89339d331ac0db7f4784d566f` (`68a4d68`) of `mage-arena` (worktree `mage-arena-arena`, branch `arena`). Copied 2026-10-02. Nine files under `data/pinned/`, at the same paths as in that commit. Sizes and sha256 are in `data/PINNED.json`.
- Verify: `node tools/check-pin.mjs`. Byte-compare to the source commit: `node tools/check-pin.mjs --source C:/Users/kazda/kiro/mage-arena-arena`. Prove a flipped byte fails the gate: `node tools/check-pin.mjs --self-test`.
- Re-pin: choose a commit, read each file with `git -C <source> show <commit>:<path>` (never the working tree), write those bytes under `data/pinned/`, rewrite `data/PINNED.json`, then run the three commands above. Do not reformat the files or change their line endings.
