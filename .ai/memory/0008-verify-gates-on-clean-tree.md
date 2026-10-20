---
id: 0008
kind: convention
scope: repo
date: 2026-10-05
supersedes: null
refs: [0003]
---

Run a change's verification gate on a tree that holds only that change. The round-1 scan-sweep (arena-greybox, 2026-10-05)
could not attribute three red session tests because the tree carried another session's uncommitted `ArenaSession` edits;
it had to prove they failed identically with its own change reverted. Round 2 (build-config) held `MageArenaVR.Build.cs`
untouched for the same reason (a foreign edit in flight). When the tree is shared, commit or set aside your own work
with a WIP commit (never a bare `git stash`, the stack is shared between worktrees), and say in the report which reds
predate the change.
