# T18 - Cinder hounds spit embers (DF-004 option A)

Status: open
Max turns: 260

## Goal
Owner decision 2026-10-07 (`docs/DECISIONS.md`, "v2 vision accepted": DF-004 option A). From the seat no hound death
burst (radius 1.5 m) reaches the player (nearest death ~3.5 m), so Bout 2 has zero perfects and teaches nothing
(`docs/design-findings/DF-004-creatures-cannot-reach-the-seat.md`). After this card **cinder hounds hold at the dais
hold line and spit slow, telegraphed magic embers from range**, and **a dying hound throws one last ember** at the
player. Embers are magic, so a well-timed fresh ward perfects them: Bout 2 becomes perfect practice. It is a VR overlay
attack; pinned `enemies.json` is not edited, and the TV change request (CR-004) is written by the orchestrator later.

## Read first
- `CLAUDE.md`, `docs/DECISIONS.md`, `docs/design/V2-ROADMAP.md` (lane A row 2), `DF-004-...md`,
  `docs/change-requests/CR-003-threat-geometry.md` (the thrower overlay this extends), `docs/gameplay/04-threat-language.md`.
- Data: pinned `enemies.json` `cinder_hound` (~line 86: HP 25, speed 6, packSize 3, `bite` physical 6 / windup 0.35 /
  recovery 0.4, `onDeath ember_burst` magic tier 0 damage 5 radius 1.5 delay 0.6); `apps/vr/data/vr/combat.vr.json`
  (`attacks`, `daisNoEntry`); pinned `combat.json` (`absorb` windows, telegraph floor); `arena-tiers.json` Tiro wave 1.
- Code: `Kernel/VrRulesLoad.cpp` (`LoadVrRuleset` ~440, attacks loop ~501-571, `ReadThrow` ~221, `Out.Throws` ~696 -
  today only conscript `throw` and slinger `unchanged` are accepted), `Kernel/Enemies.cpp` (`FindThrow` ~256 and ~356,
  behaviour parse ~331, death-burst telegraph ~429), `Kernel/ArenaThreats.cpp` (absorb, perfect ~294-317),
  `Session/ArenaSession.cpp` ~902, `Session/SessionPresentation.cpp` ~1187 (projectile and telegraph drawing),
  `Tests/SessionTests.cpp` (`CreaturesSeated`, `DeathBurstPerfect`), `tools/capture-creatures.ps1`.

## Deliverables
1. **Overlay entry** in `combat.vr.json` `attacks` for `cinder_hound`, mode `"spit"`, every number with a `_why`:
   family magic, tier 0, damage 5 (the pinned `ember_burst` damage, not a new number), windup 0.6 s (above the 0.40 s
   telegraph floor; long enough to read), projectile 7 m/s (half the 14 m/s steel speed, so it reads as slow and gives
   the perfect window room), range 9 m (the thrower reach from CR-003), and a `deathEmber: true` flag. The hound keeps
   its pinned `bite` row; with `spit` it holds at the hold line like a thrower (reuse the CR-003 steering) and spits
   instead of biting. The loader accepts `spit` only for `cinder_hound`; anything else still fails the load.
2. **Death ember**: when a spitting hound dies, after the pinned `ember_burst` delay (0.6 s), one ember projectile with
   the spit numbers is launched from the death position at the player. The pinned 1.5 m area burst still happens
   unchanged. If the death position is beyond the range, the ember still flies (state that choice in a comment).
3. **Presentation**: the ember reads in the magic threat language (ring, element colour, never black with a red rim):
   a glowing ember projectile with the spit telegraph at the hound's mouth during the windup. While here, fix the
   backlog item: the perfect-cue beads on magic telegraphs are shown only inside the absorb perfect window
   (`combat.json` `absorb.perfectWindowS`), like the teach's white ring, not for the whole telegraph.
4. **Seated script**: the reference seated script wards each ember timed for the perfect window (as the T15 death-burst
   script does), so `CreaturesSeated` logs perfects.
5. **Regular tests** (`MageArena.Creatures.*`), literals with sources: a spitting hound schedules a magic projectile
   at 7 m/s from the hold line and never enters the dais box; a fresh ward timed inside the perfect window scores a
   perfect on a spit ember (hand-computed timing from windup, distance and speed); a death spawns exactly one ember 36
   ticks after the death tick (0.6 s at 60 Hz) and still the pinned burst; with no ruleset the hound bites exactly as
   before (null path).
6. **Measure, do not tune:** run `MageArenaDesign.Session.CreaturesSeated` live and with `-MageArenaProposal` and report
   time, perfects, damage taken, outcome against the pinned 30-45 s window (do not change the window). Also run
   `MageArenaDesign.Session.FullSeated` and report.
7. **Capture** with `tools/capture-creatures.ps1` (extend it if needed) into `runs/T18/shots/`: a hound spitting at the
   lip, an ember in flight with the perfect cue inside its window, a perfected ember, a death ember. Look at every
   still and describe it in the report.

## Constraints
- Do not commit, push, or touch any repository other than this one. No paid services. Do not edit
  `docs/DECISIONS.md`, `apps/vr/data/pinned/` or `calibration-proposal.json`.
- No change on the null-ruleset path: conformance stays byte-identical.
- Regenerate the sigil corpus as `CLAUDE.md` says before the full suite in a fresh checkout.
- One Unreal build at a time; one `Automation RunTests` filter per editor process. Do not end your turn while a build,
  test or capture is running. Never write placeholder evidence.

## Acceptance (the orchestrator re-runs these and looks at every still)
```
node apps/vr/tools/conformance/generate.mjs && git status --porcelain apps/vr/data/conformance   # no output
powershell -NoProfile -File apps/vr/tools/build.ps1
UnrealEditor-Cmd MageArenaVR.uproject -ExecCmds="Automation RunTests MageArena.;Quit" -unattended -nullrhi -nosplash -log
UnrealEditor-Cmd MageArenaVR.uproject -ExecCmds="Automation RunTests MageArenaDesign.Session;Quit" -unattended -nullrhi -nosplash -log   # + -MageArenaProposal
powershell -NoProfile -File apps/vr/tools/capture-creatures.ps1   # stills in runs/T18/shots/
```

## Report
Write `runs/T18/REPORT.md`: files, every acceptance command with real output, the measured CreaturesSeated and
FullSeated numbers (live and proposal, before and after), a description of each still, and every decision the card did
not specify.
