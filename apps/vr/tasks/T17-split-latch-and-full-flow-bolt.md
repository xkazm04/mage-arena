# T17 - Split hands: the perfect latch clears on idle, and a split Bolt at Flow 5

Status: done with a design finding (verified 2026-10-07: MageArena. 175/175, design numbers unchanged live and proposal; the idle clear cannot restore a perfect inside one hold under the pinned 0.15 s window - DF-005)
Max turns: 200

## Goal
Owner decision 2026-10-07 (`docs/DECISIONS.md`, "v2 vision accepted", split hands): today a split cast latches
"split casting" until the ward drops, so one split cast kills perfect absorbs for the rest of the ward hold; and at
Flow 5 even a split Bolt is refused for "crest". After this card the latch **clears once the casting hand has been idle
for about 0.3 s** (perfects come back while the ward stays up), and **a split Bolt at Flow 5 is allowed and does not
spend the Crest**. Both are VR overlay rules; the pinned data and the null-ruleset path do not change.

## Read first
- `CLAUDE.md`, `docs/DECISIONS.md` (they outrank this card), `docs/design/V2-ROADMAP.md` (lane A row 1),
  `docs/design/SCHOOL-DEFENCES.md` section A (lines ~22-30).
- Code (`apps/vr/Game/Source/MageArenaVR/`): `Kernel/VrRules.h` (`FVrRuleset::SplitCasting` ~149,
  `IsSplitCasting` / `SetSplitCasting` ~168, `FVrSplitHands` ~29), `Kernel/VrRules.cpp` (~367 `ClearRuntime`,
  ~388-404, `Refuse` ~423), `Kernel/VrRulesLoad.cpp` (`splitHands` parse ~579-604), `Kernel/ArenaKernel.cpp`
  (`StartCast` ~258-310: the tier and crest refusals and the latch set; ~489-492 the latch clear; ~56 split ward drain),
  `Kernel/ArenaThreats.cpp` (~294-297: the latch forces `bFreshWindow = false`), `Kernel/Water.cpp` (~23-42 Crest,
  ~85), `Session/ArenaSession.cpp` (`WouldSplitRefuse` ~624-638 and its callers), `Tests/DefenceTests.cpp`
  (`MageArena.Defences.SplitBolt` ~334).
- Data: `apps/vr/data/vr/combat.vr.json` (`splitHands`), pinned `combat.json` (`flow.max` 5, `crest`).

## Deliverables
1. **Overlay keys** in `combat.vr.json` `splitHands`, each with a `_why` line citing the DECISIONS entry:
   `latchClearIdleS: 0.3` and `boltAtFullFlow: true`. Parsed in `VrRulesLoad.cpp` with the same strictness as the
   existing keys (missing or wrong type fails the load). Absent keys mean today's behaviour, so the calibration
   proposal and old fixtures still load.
2. **Latch clear on idle** (kernel, ruleset path only): the latch also clears when the casting hand has had no split
   cast starting, in windup or releasing for `latchClearIdleS` (convert to ticks with `SimDt()`, 60 Hz: 0.3 s = 18
   ticks; state the rounding rule in a comment). Keep the existing clear on ward drop. Store the idle counter in the
   ruleset runtime state (cleared by `ClearRuntime`), not in `FArenaState`, so the state hash does not change.
3. **Split Bolt at Flow 5**: with `boltAtFullFlow`, a split cast of the tier-0 Bolt at `Flow >= FlowMax` is accepted:
   it resolves at the split power (`oneHandPower`), Flow stays at its value, `Water.Crests` does not increase, and the
   Crest is not consumed, so the next two-hand line cast still gets it. Every other split cast at full Flow is still
   refused for "crest", and the tier refusal is unchanged. `WouldSplitRefuse` in the session must agree with the kernel
   (one shared helper is better than two copies).
4. **Regular tests** in `Tests/DefenceTests.cpp` (`MageArena.Defences.*`), every assertion able to fail, expectations
   as literals with their source stated:
   - `SplitLatchClearsOnIdle`: ward up, split cast, then a magic projectile arrives inside the perfect window at
     17 ticks of idle (no perfect) and, in a second run, at 18 ticks (perfect scored, `perfect` event emitted).
   - `SplitLatchStillOnWhileCasting`: a second split cast before 18 ticks keeps the latch.
   - `SplitBoltAtFullFlow`: Flow 5, split Bolt accepted, damage = Bolt damage x 0.6 (hand-computed), Flow 5 after,
     Crests unchanged; then a two-hand line cast gets the Crest (damage x 1.5).
   - `SplitLineAtFullFlowRefused`: a split tier-1 line cast at Flow 5 is still refused for "crest".
   - Update `SplitBolt` only where it asserts the old behaviour, and say in the report which assertion changed and why.
5. **No regression:** conformance byte-identical; `MageArena.*` green; run `MageArenaDesign.Duel` and
   `MageArenaDesign.Session` live and with `-MageArenaProposal`, and report each design test's numbers before and after
   (they are measured, not tuned; the three known reds may stay red).

## Constraints
- Do not commit, push, or touch any repository other than this one. Do not call paid services. Do not edit
  `docs/DECISIONS.md`, `apps/vr/data/pinned/` or `calibration-proposal.json`.
- No change on the null-ruleset path. Do not tune any other number.
- Before the full suite in a fresh checkout, regenerate the sigil corpus exactly as `CLAUDE.md` says (never run
  `generate.mjs` bare or with `--templates`).
- One Unreal build at a time; one `Automation RunTests` filter per editor process. Do not end your turn while a build or
  test is running.

## Acceptance (the orchestrator re-runs these; quote their real output in your report)
```
node apps/vr/tools/conformance/generate.mjs && git status --porcelain apps/vr/data/conformance   # byte-identical: no output
powershell -NoProfile -File apps/vr/tools/build.ps1                                               # build succeeded
UnrealEditor-Cmd MageArenaVR.uproject -ExecCmds="Automation RunTests MageArena.;Quit" -unattended -nullrhi -nosplash -log   # all pass
UnrealEditor-Cmd MageArenaVR.uproject -ExecCmds="Automation RunTests MageArenaDesign.Duel;Quit" -unattended -nullrhi -nosplash -log      # + -MageArenaProposal; numbers reported
UnrealEditor-Cmd MageArenaVR.uproject -ExecCmds="Automation RunTests MageArenaDesign.Session;Quit" -unattended -nullrhi -nosplash -log   # + -MageArenaProposal; numbers reported
```

## Report
Write `runs/T17/REPORT.md`: files changed, each acceptance command with its real output (pass counts, the design
numbers before and after), the tick arithmetic, anything you could not do with the exact error, and every decision
the card did not specify.

## Orchestrator verification (2026-10-07)
Grok's balance was exhausted (HTTP 402), so a Claude subagent implemented the card in worktree `v2/T17`. The orchestrator
re-ran the build (green) and `MageArena.` (175 Success, `EXIT CODE: 0`) and read the diff. Design numbers were diffed
identical before and after, live and proposal. The conformance command fails on master too, before this card
(`PINNED.json commit baeac66... is not 68a4d68`: `generate.mjs` predates the 2026-10-07 pin rewrite; backlog).
Finding DF-005: the perfect window (pinned 0.15 s) starts at the ward raise, and a split cast needs the ward already up,
so the 18-tick idle clear always lands after the window; the latch now only affects the split ward drain.
