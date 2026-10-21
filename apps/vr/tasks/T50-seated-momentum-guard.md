# T50 - Seated-momentum guard: a player-castable Air school must declare a seated Momentum reading

Status: open (needs Wolf: UE 5.8.3 editor and the `MageArena.` automation suite; written 2026-10-21 on Lynx, where
neither runs - `docs/DECISIONS.md` 2026-10-20 "Unreal host")
Size: S/M. Max turns: 140

## Goal
A guard, not a design. `FAirResource`'s Momentum is computed from a velocity that is structurally zero for a seated
player, so if a player-castable Air school is ever switched on without a seated reading of Momentum, Momentum can only
decay to zero, Air Form (which needs Momentum) becomes uncastable, and **nothing in the repo would flag the dead
mechanic.** This card adds a load-time or activation-time check that refuses that combination with a named error, plus
a test that fails if a player is given Air with no seated reading declared. It does **not** invent the seated reading -
that is an open owner decision (see "Blocker"), and the guard must be useful before it is answered.

## This is harmless today - say so and keep it that way
The player has no Air school. `Actor->Air.bSchool = true` is set in exactly **one** production place,
`Kernel/Games.cpp:133` (the `else if (School == TEXT("air"))` branch at `:131`, inside the rival spawn), so only rival
mages get Air. For a walking AI rival the mechanic works **correctly**: the rival has real `Move` input, its
`Pos - PreviousPos` is a real displacement, and Momentum gains and decays as the pinned rules intend. Lio is that
rival (`Tests/AirTests.cpp:1010`, `Tests/SessionTests.cpp:480`). So this card must not change a single number for any
rival, and it must not pretend a live bug exists. The risk is **latent**, and the guard exists so that the day someone
adds a player-castable Air school, the build says no instead of shipping a dead mechanic into a seated duel.

## The reading (re-verify before changing anything)
Line numbers verified on Lynx 2026-10-21 - trust the symbol, the lines will move.
- `Kernel/Air.cpp` - `UpdateAirResource` (`:463`). Its own comment at `:460-462` states the contract: "the speed is
  this tick's locomotion (walk, sprint, roll), Pos - PreviousPos over one step ... Above MovingAboveMps it gains; at
  zero displacement ('still') it decays; in between neither runs."
  - `const double Speed = SimDistance(Actor.Pos, Actor.PreviousPos) / SimDt();` (`:476`)
  - gains `Rules.GainPerSecondMoving * SimDt()` only when `Speed > Rules.MovingAboveMps` (`:477-480`)
  - decays by `Rules.DecayPerSecondStill * SimDt()` when `!(Speed > 1.0e-9)`, i.e. exactly zero (`:481-484`)
  - early-returns when `!Actor.Air.bSchool` (`:465-468`), which is why nothing happens today.
- `Session/ArenaSession.cpp` - `FArenaSession::SeatOnActivePad` (`:1750`) sets `Player.Pos = Player.PreviousPos = Seat`
  where `Seat = PadKernel(ActivePad)`. It is called from the bout start (`:411`) and from
  `FArenaSession::StepKernel` (`:1792`) at **`:1921`**, under the comment "The kernel may have travelled along the
  roll this tick. The body stays on the pad." That is **every tick**, after any roll or blink displacement - so the
  seated player's `Pos - PreviousPos` is identically zero on every tick, and no locomotion of any kind (walk, roll,
  blink) can ever register as displacement.
  (Note: the brief that commissioned this card said "a blink also calls it"; the verified behaviour is stronger - the
  reseat is unconditional and per-tick, not blink-specific.)
- `Kernel/Air.h` / `FMomentumRules` - the pinned Momentum rules (`Min`, `Max`, `MovingAboveMps`,
  `GainPerSecondMoving`, `DecayPerSecondStill`), read through `KernelData().Momentum`.
- `apps/vr/tasks/BACKLOG.md` line 17 - the **open** `OWNER (design, before the Earth and Air cards)` item, verbatim:
  "in schools.json Earth's Footing builds while standing still and Air's Momentum while moving faster than 4 m/s. A
  seated VR player never walks, so both need a VR reading (e.g. Footing from staying on one pad, Momentum from
  consecutive blinks). Decide before T10/T11."
- `docs/design/SCHOOL-DEFENCES.md:52-61` - the proposals that answer it: Footing builds while the mage **stays on one
  pad** (`:53`), Momentum builds from **consecutive blinks and quick casting cadence** (`:61`). `:70` notes these land
  "with the Earth and Air school cards, after the owner confirms the Footing/Momentum readings above."

## Blocker, stated plainly
**The seated Momentum reading is an open owner decision** (`apps/vr/tasks/BACKLOG.md` line 17; proposals in
`docs/design/SCHOOL-DEFENCES.md`). This card **must not choose it** and must not implement it. If you find yourself
writing code that computes Momentum from blinks or casting cadence, you are out of scope - stop and report. The guard
is designed to be useful *before* the decision: it makes the missing reading a build/load failure rather than a silent
dead mechanic, and the day the owner rules, whoever implements the reading flips one declaration and the guard goes
green.

## Deliverables (exact paths)
1. **A declaration of the seated reading, as data or as a typed flag.** Pick the smaller option and justify it in the
   report:
   - (a) a per-school field the Air school declares - e.g. a `seatedResourceReading` string on the school's rules,
     read through the **VR overlay** (`apps/vr/data/vr/`), never the pin; or
   - (b) a typed enum on `FAirResource`/the school state, defaulting to "none declared".
   Whichever you choose, "none declared" must be the default, so a future school that forgets is caught.
2. **The guard.** A check that refuses the combination *player-castable Air school* + *no declared seated Momentum
   reading*, with a named error in the Air house style
   (e.g. `"air school: player-castable without a seated momentum reading (BACKLOG owner item, SCHOOL-DEFENCES.md)"`).
   Put it wherever it fires earliest and cheapest - at kernel-data load if the declaration is data, or at the point
   `Air.bSchool` is set for an actor on the player's side if it is state. State in the report which you chose, where it
   fires, and why that site catches every path that could grant a player Air.
3. **Tests, each seen RED before the change** (`Tests/AirTests.cpp`):
   - `MageArena.Air.PlayerAirWithoutSeatedReadingFails`: constructing/activating a player-side actor with
     `Air.bSchool` and no declared reading hits the guard with the named error. On unchanged `master` this test
     **passes a dead mechanic silently** - quote the red line verbatim after the guard exists but before the
     declaration is added, or quote the unchanged-master run showing the mechanic silently dead; say which form of red
     evidence you produced and why.
   - `MageArena.Air.SeatedPlayerMomentumOnlyDecays`: a characterisation test on the **current** behaviour - a seated
     player-side actor given `Air.bSchool` by hand (test fixture only, no production change) has `Speed == 0` on every
     tick and its Momentum monotonically decays to `Rules.Min`, never gaining. This is the test that documents *why*
     the guard exists; it must keep passing.
   - `MageArena.Air.RivalMomentumUnchanged`: a walking rival's Momentum trace over a scripted bout is **identical** to
     the pre-change trace (quote the values). This is the test that proves the guard cost the rivals nothing.
4. One short note appended to `docs/design/SCHOOL-DEFENCES.md`, in the section that holds the Momentum proposal,
   recording that the guard exists and names that open item - **anchored insert of a few lines only, never a rewrite
   of the file.**

## Constraints
- Do not commit, push, or touch any repository other than this one.
- Do not call paid services. Do not edit `docs/DECISIONS.md`, `apps/vr/tasks/BACKLOG.md`, `apps/vr/data/pinned/**`,
  `apps/vr/data/PINNED.json`, `apps/vr/data/conformance/**` or `apps/vr/Game/Clips/**`.
- **Do not choose or implement the seated Momentum reading** (see "Blocker"). Do not touch Earth's Footing either.
- **No behaviour change for any rival, and no number change anywhere.** `node apps/vr/tools/check-pin.mjs` must still
  print `pin OK: 9 files @ baeac66` and `conformance OK: 45 vectors`, and the 45 vectors stay byte-identical. The
  pinned `FMomentumRules` values are not retuned.
- Do not change `UpdateAirResource`'s maths, its thresholds, or `SeatOnActivePad`. The per-tick reseat at
  `ArenaSession.cpp:1921` is correct for a seated duel and stays.
- Keep it small. If the guard is turning into a resource-system refactor, stop and report - that is the Air school
  card's work, not this one's.
- Fresh checkout or worktree: regenerate the gitignored sigil corpus first -
  `node apps/vr/tools/clipgen/generate.mjs --corpus 30 --seed 1000` then `node apps/vr/tools/clipgen/mousepaths.mjs`.
  Never run `generate.mjs` bare or with `--templates`; both rewrite tracked clips.
- One Unreal build at a time (`-WaitMutex`); one `Automation RunTests` filter per editor process; read verdicts from
  `-ReportExportPath`. `apps/vr/tools/build.ps1` has exited 0 when `Build.bat` was missing (found 2026-10-09): confirm
  the build really ran from its log line, not from the exit code.
- Baseline `MageArena.` and `MageArenaDesign.` before the change. Known red on a clean tree (`CLAUDE.md`, as of
  9f338f0): `MageArenaDesign.Duel.FireSeated`, `Duel.AirSeated`, `Session.FullSeated`, `Session.Wave1Seated`,
  `Session.CreaturesSeated`. `Duel.AirSeated` was confirmed red on base 9f338f0 by owner ruling 2026-10-16 and is not
  a new regression - but it is an **Air** test, so compare its failing numbers line by line and report them: a
  different failure there is this card's problem even though the test was already red.

## Acceptance (the orchestrator re-runs these; quote their real output in your report)
Every UE command below is **Wolf-only**. Lynx has no Unreal install and cannot produce any of this output.
```
node apps/vr/tools/check-pin.mjs
    # expected, verbatim: "pin OK: 9 files @ baeac66" then "conformance OK: 45 vectors"
git status --porcelain -- apps/vr/data/pinned apps/vr/data/conformance apps/vr/data/PINNED.json
    # expected: EMPTY

powershell -NoProfile -File apps/vr/tools/build.ps1
    # expected: the build log contains "Result: Succeeded" (read the log, not the exit code)

UnrealEditor-Cmd MageArenaVR.uproject -ExecCmds="Automation RunTests MageArena.Air;Quit" -unattended -nullrhi -nosplash -log -ReportExportPath=runs/T50/air
    # expected: every MageArena.Air.* green, including
    #   .PlayerAirWithoutSeatedReadingFails, .SeatedPlayerMomentumOnlyDecays, .RivalMomentumUnchanged

UnrealEditor-Cmd MageArenaVR.uproject -ExecCmds="Automation RunTests MageArena.;Quit" -unattended -nullrhi -nosplash -log -ReportExportPath=runs/T50/all
    # expected: identical to the pre-change baseline, plus the three new tests green

UnrealEditor-Cmd MageArenaVR.uproject -ExecCmds="Automation RunTests MageArenaDesign.;Quit" -unattended -nullrhi -nosplash -log -ReportExportPath=runs/T50/design
    # expected: exactly the 5 known reds; Duel.AirSeated's failing numbers quoted and compared line by line

node apps/vr/tools/lint/format-check.mjs --self-test && node apps/vr/tools/lint/format-check.mjs --base origin/master
    # expected: OK
node apps/vr/tools/lint/unused-members.mjs && node apps/vr/tools/lint/unused-members.mjs --self-test
    # expected: OK
```

## Report
Write `runs/T50/REPORT.md`: base sha and the Wolf host name and engine build; what you built (paths); which
declaration option (a) or (b) you chose and why; where the guard fires and why that site covers every path to a
player-castable Air school; each acceptance command with its real output; the red evidence for
`PlayerAirWithoutSeatedReadingFails` and what form it took; the rival Momentum trace before and after, side by side;
`Duel.AirSeated`'s failing numbers before and after; the premises above that turned out false; and anything you could
not do, with the exact error. State explicitly, in one line, that the seated Momentum reading itself is still the
owner's open decision and was not chosen here.
