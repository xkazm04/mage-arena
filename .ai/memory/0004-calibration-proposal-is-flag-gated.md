---
id: 0004
kind: gotcha
scope: path:apps/vr/data/vr
date: 2026-10-05
supersedes: null
refs: [0002, 0003]
---

`apps/vr/data/vr/calibration-proposal.json` is written by the seated census (T12) and applies only when the run passes
`-MageArenaProposal` (or `MageArenaProposal=1` / `true`). Nothing in it is copied into the live `combat.vr.json` without
the owner's sign-off; as of DF-003 the lowest-penalty set (`wide`) is left in the proposal, unsigned, because it misses
several targets. A census is only as good as the rule it measures: the first T12 census (600 rows) measured a fire wall
that destroyed the caster's own bolts and charged her Heat for them, which lengthened every duel with a wall, and it had to
be discarded and re-run after the fix. Re-run the census after any rule fix before comparing numbers.
