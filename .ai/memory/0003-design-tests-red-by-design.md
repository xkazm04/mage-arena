---
id: 0003
kind: gotcha
scope: path:apps/vr/Game/Source/MageArenaVR/Tests
date: 2026-10-05
supersedes: null
refs: [0004, 0008]
---

`MageArenaDesign.*` automation tests are measurements of an open design question and are kept red on purpose until the
owner changes the design (DF-001: `MageArenaDesign.Session.Wave1Seated`); the green gate is `MageArena.*`. Never tune
overlay numbers, loosen a threshold or skip a test to turn a design test green: the seated numbers are the owner's sign-off
(DF-003 left its census proposal unsigned for exactly this reason). When a run shows red session-simulation tests
(the round-1 sweep saw `Duel.FireSeated`, `Session.FullSeated`, `Session.Wave1Seated` red at 144 of 147), check whether
they are design tests and whether they fail identically on the base commit before attributing them to the change.
