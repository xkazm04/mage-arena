# T15 - Bout 2 (creatures) in the greybox, and the full session chain

Status: open
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
