# Pinned combat data

The authoritative combat data (spells, enemies, tier clock, Flow, arena tiers) is owned by the Mage Arena core
project, `kiro/mage-arena` and its arena worktree, where a headless simulator calibrated it (plan section 3).

This folder will hold a **copy pinned to one committed revision** of that data, with the source commit recorded in
`PINNED.json` (`{ "repo": "...", "commit": "...", "files": [...], "copied": "<date>" }`). Rules:

- Copy only from a **commit**, never from a working tree (the source has uncommitted work in progress).
- Never edit the copy. A change is a change request to the core project, then a re-pin.
- The contest copy under `firetv/.contest/arena/mage-arena/` is **stale** (pre-calibration numbers). Do not use it.

Not pinned yet: the first pin is a wave-1 task.
