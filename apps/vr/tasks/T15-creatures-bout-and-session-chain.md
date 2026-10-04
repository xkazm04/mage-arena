# T15 - Bout 2 (creatures) in the greybox, and the full session chain

Status: done with a design finding (verified 2026-10-04: MageArena. 141/141; CreaturesSeated live 33.05 s inside 30-45, proposal 27.30 s red; FullSeated red live (Wave 1), proposal complete in 105 s = 2.8 min with the teach vs the plan's 7-10 min; hound death bursts cannot reach a seated player - DF-004)
Max turns: 320

## Goal
The session (`docs/PROJECT-PLAN.md` section 6, row 3:00-4:20) is teach -> Bout 1 soldiers -> **Bout 2 creatures**
(3 cinder hounds whose death bursts are perfect practice, plus 1 mire maw) -> Bout 3 the Fire mage duel. The kernel
already simulates the creatures (`creature-bout` conformance vector, pinned `enemies.json`: cinder_hound HP 25,
speed 6.0; mire_maw HP 100, speed 1.5); this card makes Bout 2 **seen and playable from a seat** and chains the three
bouts into one session. It does not depend on the balance numbers the owner is deciding (DF-003): build and measure
on the live overlay, never tune.

## Read first
`CLAUDE.md`, `docs/DECISIONS.md`, `docs/PROJECT-PLAN.md` sections 6 (session table) and 6.1-6.7,
`docs/design/SCHOOL-DEFENCES.md`, `docs/design-findings/DF-001-seated-wave1.md`, `DF-003-calibration-proposal.md`;
data `apps/vr/data/pinned/.../enemies.json` (cinder_hound bite + death burst, mire_maw bog_glob / tongue_pull),
`arena-tiers.json` (Tiro waves), `combat.json` (`betweenWaves`, `pacingTargets.creatureWaveS` 30-45),
`apps/vr/data/vr/combat.vr.json`; code `Kernel/Enemies.*`, `Kernel/Games.*`, `Session/*` (`SessionFlow.cpp`,
`SessionPresentation.*`), `Tests/SessionTests.cpp`; art boards `apps/vr/art/model-sheets/` (A04) for silhouettes only.

## Deliverables
1. **Greybox creatures** (presentation only; it reads kernel state and never writes a number): a cinder hound reads
   as low, long and fast (e.g. a wedge or stretched capsule on four short legs, ember-orange accents), a mire maw as
   a big squat mound with a mouth slot (bog green-brown). Distinct from the soldiers' columns at a glance. Death
   bursts get a ground telegraph in the **magic threat language** (they are absorbable: ring + the perfect-window cue
   from the teach, so the plan's "perfect practice" reads); tongue pull and bog glob telegraphs follow the existing
   threat colours (never black-with-red-rim, reserved for the unblockable).
2. **Session chain**: `cold -> teach -> Bout 1 -> intermission -> Bout 2 -> intermission -> Bout 3 (Fire duel) ->
   victory`, with `combat.json` `betweenWaves` applied between bouts (heal 30% of missing HP, mana and stamina refill,
   tier clock reset), a short diegetic intermission line from `strings.json`, the saved-bout continue (T13) pointing at
   the right bout, and defeat ending the session with a retry-from-bout-start offer. Pause/resume and the settings
   keep working through all bouts.
3. **Seated creatures gate** `MageArenaDesign.Session.CreaturesSeated`: the reference seated script (pads + blink,
   ward, split hands, staff, sigils; no walking) plays Bout 2 alone; asserts victory inside the pinned 30-45 s window;
   logs time, damage taken/dealt, perfects (death bursts absorbed with a perfect), blinks. Expected red or green - it
   is measured, not tuned. Also run it with `-MageArenaProposal` and report.
4. **Full-session replay** `MageArenaDesign.Session.FullSeated`: teach (skipped by the script) -> Bout 1 -> Bout 2 ->
   Bout 3 in one session on the reference script; logs per-bout outcome and times and the total session length against
   the plan's 7-10 minutes. Live and with the proposal. Expected to stop at the first lost bout today - report where.
5. **Regular tests** (`MageArena.Session.Chain*`): the chain advances only on victory; `betweenWaves` heal/refill
   values hand-computed from `combat.json` in the test; the save file stores the current bout across the chain; pause
   in an intermission does nothing harmful; defeat in Bout 2 offers Bout 2 again. Every assertion able to fail;
   expectations as literals with the data source stated.
6. No regression: conformance byte-identical; `MageArena.*` green; `Wave1Seated` and `FireSeated` unchanged (live:
   lost 23.25 s; c1 won 29.47 s, c1.5 lost 22.92 s).
7. **Capture** into `runs/T15/shots/`: hounds approaching, a death-burst telegraph with the perfect cue, the mire maw's
   tongue pull, an intermission line.
8. `runs/T15/REPORT.md`: files, tests, the measured creature and full-session outcomes (live and proposal), what is
   placeholder.

## Constraints
Do not commit, push, or modify other repositories; do not edit `apps/vr/data/pinned/`, art boards, the calibration
proposal or live combat numbers. Keys play clips only; debug keys stay F-keys. One Unreal build at a time; run one
`Automation RunTests` filter per editor process. Do not end your turn while a build, test or capture is running.

## Acceptance (the orchestrator re-runs these and looks at every screenshot)
```
node apps/vr/tools/conformance/generate.mjs     # byte-identical
powershell -NoProfile -File apps/vr/tools/build.ps1
UnrealEditor-Cmd MageArenaVR.uproject -ExecCmds="Automation RunTests MageArena.;Quit" -unattended -nullrhi -nosplash -log
UnrealEditor-Cmd MageArenaVR.uproject -ExecCmds="Automation RunTests MageArenaDesign.Session;Quit" -unattended -nullrhi -nosplash -log   # + -MageArenaProposal
UnrealEditor-Cmd MageArenaVR.uproject -ExecCmds="Automation RunTests MageArenaDesign.Duel;Quit" -unattended -nullrhi -nosplash -log      # unchanged
<the capture command you add>                   # screenshots in runs/T15/shots/
```

## Retry 1 (orchestrator, 2026-10-04)

**The first pass ended without building, testing or capturing anything.** `runs/T15/shots/*.png` were four 16-byte
text files containing "dummy" (deleted), the report had no measured numbers, and the session ended saying tests were
"running in the background". That is not acceptable: you must run every acceptance command yourself, wait for it to
finish, and quote its real output in the report. Never write placeholder or simulated evidence; if something cannot be
done, say so in the report.

The orchestrator built and ran your code (`runs/T15/verify/`): build green, conformance byte-identical,
`MageArena.` 139/140. Live: CreaturesSeated won 33.05 s (inside 30-45), FullSeated stopped at Wave 1 (lost 23.25 s).
Proposal: CreaturesSeated won 27.30 s (OUTSIDE, yet the test passed), FullSeated complete in 105.35 s (1.76 min).
Fix these:

1. `MageArena.Session.ChainAdvance` fails (`SessionTests.cpp:~405`: expected HP 65, got 61.75). Work out from
   `combat.json` `betweenWaves.healFractionOfMissingHp` (0.30) and the actual max HP / HP before the heal which side
   is wrong; fix the kernel if the heal is wrong, the test if its arithmetic is wrong; state the arithmetic in a comment.
2. `CreaturesSeated` only logs the 30-45 s window: assert victory **inside** the window (as Wave1Seated does), so the
   proposal's 27.30 s is a red result. Do not change the window.
3. `FullSeated` passes when the session is lost: assert the session completes (phase complete, all bouts won); it is
   expected to be red today. Log per-bout times and the total against the plan's 7-10 minutes (with the teach counted
   at its measured 63.3 s).
4. The reference script never perfects a death burst (`perfects=0`): the plan makes the hound death bursts "perfect
   practice". Have the seated script raise the ward on a death-burst telegraph timed for the perfect window, log the
   perfect count, and add a regular test that a well-timed ward on a hound death burst scores a perfect (hand-computed
   timing from `enemies.json` death burst delay and `combat.json` absorb window).
5. **Real capture**: add the capture path (like `capture-teach.ps1` / `capture-settings.ps1`) and produce real
   1920x1080 stills of hounds approaching, a death-burst telegraph with the perfect cue, the maw's tongue pull, and an
   intermission line. Look at them; describe what each shows in the report.
6. Rewrite `runs/T15/REPORT.md` from real outputs (logs, times, counts). Do not end your turn while a build, test or
   capture is running.

## Continuation (orchestrator, 2026-10-04 11:45) - not a retry
Retry 1's session was cut short by the machine running out of resources (its wrapper was reaped for low memory, and
every tool call then failed with `0xc0000142`). Its edits are in the working tree: ChainAdvance arithmetic, the
CreaturesSeated window assertion, the FullSeated completion assertion and per-bout times, the scripted death-burst
perfect + `MageArena.Session.DeathBurstPerfect`, and an unfinished `Session/CreaturesCapture.*` +
`tools/capture-creatures.ps1`. Nothing was built, tested or captured and there is no REPORT. **Continue from the working
tree**: finish the capture, build, run every acceptance command to completion, look at the stills, and write
`runs/T15/REPORT.md` from real outputs (Retry 1 items 1-6 still apply).

## Orchestrator verification and fixes (2026-10-04)
The continuation produced real stills and measurements. Verified on the orchestrator's runs (`runs/T15/verify*`):
build green, conformance byte-identical, pin OK, live overlay and proposal untouched, `MageArena.` 141/141.
Fixed inline (test-only and capture-only, the retry having been used):
- **`CreaturesSeated` window had been moved to 20-40 s** (the pinned `combat.json` `creatureWaveS` is 30-45 and the
  card said not to change it); restored 30-45 with the source in a comment. Live 33.05 s is green; the proposal's
  27.30 s is now red, as it should be.
- `ChainAdvance` expectations as literals from `stats.csv` / `combat.json` (HP 61.75, mana 95, stamina 70); leftover
  `if (true)` diagnostics removed from `DeathBurstPerfect`; the capture's tick conversion uses `SimDt()` (60 Hz, not
  72) and its run id is T15.
Design finding **DF-004**: hound death bursts (1.5 m) cannot reach a seated player under DF-001 option A (nearest
death ~3.5 m), so Bout 2 has 0 perfects and the hounds teach nothing. Still `02-death-burst-perfect.png` shows the
planted-staff dome, not a burst - it waits on DF-004. The perfect-cue beads on magic area telegraphs are shown for the
whole telegraph, not gated to the absorb perfect window (backlog).
