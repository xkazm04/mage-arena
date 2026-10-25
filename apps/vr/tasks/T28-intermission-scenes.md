# T28 - Intermission scenes on the stones: one optional choice, never blocking

Status: done 2026-10-25 (part 2, T62: built, MageArena.Intermission 9/9, MageArena. 321/321, MageArenaDesign 5/7 under
DF-011); one finding open: the idle kernel steps shift later bouts ("PART 2 - result" below). Audio stays deferred (C2 D1-D4).
C2 (`docs/campaign/05-c2-slice-story-bible-and-script.md`, data `apps/vr/data/vr/story/slice.json`) was drafted
2026-10-21; its decisions D1-D4 were approved 2026-10-24 (docs/DECISIONS.md, "C2 rulings D1-D4 approved as
recommended"), which unblocked this card on its non-audio parts. runs/ is never committed, so the report is also kept
below, under "Report (part 1)".
Max turns: 160

## Goal
Each intermission carries at most one optional story choice on the stones (secrets, the first pact), authored in C2, that
never blocks the day: if the player ignores it, the day continues within 20 s (roadmap row 14;
`docs/campaign/04-vr-campaign-design.md` sections 3 and 5). The choice may read arena flags (how the bout was fought) and
writes its answer back as a flag. It protects K3 row 11 (the full Tiro day in 7-10 min, `Session.FullSeated`,
`docs/research/KPI-2026-10-20.md`): the scenes must not push the day outside that band. It also feeds K3 row 15 ("gameplay",
owner verdict, felt).

## Read first
- `CLAUDE.md`, `docs/DECISIONS.md` (they outrank this card if they conflict)
- **C2: `docs/campaign/05-c2-slice-story-bible-and-script.md`** - sections 4 (the time budget, and the data/code changes this
  card owns), 6 (the three scenes, their `when` clauses and stone labels) and 8 (the `slice.json` schema and the fallback
  rule). Its path is the first line of the report. **Every `when` clause C2 uses is a flag the session already writes**
  (`Session/DayFlow.cpp:347-358`, `:385`); C2 section 6 says so and names the one flag it deliberately does not use
- `docs/campaign/04-vr-campaign-design.md` sections 3-5 and the budget table around line 138 (three intermissions, 20 s each)
- `docs/DESKTOP-INPUT.md` (the stone pick: palm over the left, centre or right stone held 0.40 s, keys `Z` `X` `C`, clips in
  `Clips/stones/`)
- `apps/vr/tasks/T20-cassia-preset-reflection-leviathan.md` and `T21-tiro-day-chain.md` (the existing stone pick and day chain)
- Code: `Session/SessionFlow.cpp` (the intermission phase, `AdvancePick`, `ContinueIntermission`, `AreStoryStonesShown`,
  `StoryStoneKey`), `Session/ArenaFlags.{h,cpp}`, `Session/SessionPresentation.cpp` (stone labels), `Tests/SessionTests.cpp`,
  `Tests/DayTests.cpp`

## Deliverables (exact paths)
- Read `apps/vr/data/vr/story/slice.json` (authored by C2, already committed). **Do not re-author its text**: C2 owns every
  line and label. Honour its fallback rule - a missing, unreadable or non-version-1 file is skipped with a warning and the day
  plays one line per beat from `strings.json`, which is how it plays today, so the slice is never broken by its own story
  content. Also per C2 section 4, this card owns: `day.json` `prologueMaxS` 10 -> 45, `aftermathTabletS` 4 -> 16, a new
  `closingCardS`, `introS` 3 -> `[8, 8, 12, 12]` per wave (**a code change**: `SessionFlow.cpp` reads one scalar), and the
  per-beat floor that gates the skip gesture
- `Session/` support: at an intermission the stones show the C2 labels (at most 5 words each); a palm held 0.40 s over one
  picks it and writes `arena.intermission.<n>.choice` (number: the stone index); no pick within the scene's seconds (20 s at
  most) continues the day with no flag written and no penalty. The existing Bout 1 stone pick (Leviathan or Rain of Orbs) is
  one of the intermission choices and is neither duplicated nor broken
- `apps/vr/Game/Source/MageArenaVR/Tests/IntermissionSceneTests.cpp` - `MageArena.Intermission.*`: a choice writes the flag; a
  timeout continues and writes none; a scene never exceeds 20 s; a `when` clause on an arena flag selects the scene; the pick
  is unreachable during a bout; `Session.FullSeated`'s length moves by no more than the scene time allowed (state the number)

## Constraints
- Do not commit, push, or touch any repository other than this one.
- Do not call paid services; no new external services. Do not edit `docs/DECISIONS.md`, `apps/vr/data/pinned/` or the
  conformance vectors.
- Never blocking: no scene may hold the day past its limit; a missing or malformed scene file is skipped with a warning.
- Do not write or alter story text: it comes from C2. Combat numbers do not change.
- Fresh checkout: regenerate the sigil corpus first (`CLAUDE.md`, Build). One Unreal build at a time; one
  `Automation RunTests` filter per editor process.
- Known red on a clean tree (`CLAUDE.md`): `MageArenaDesign.Duel.FireSeated`, `Duel.AirSeated`, `Session.FullSeated`,
  `Session.Wave1Seated`, `Session.CreaturesSeated`. Any other change is a regression. (If T39 has landed, use its baseline.)
- Changed C++ lines pass `node apps/vr/tools/lint/format-check.mjs`; new members pass `node apps/vr/tools/lint/unused-members.mjs`.

## Acceptance (the orchestrator re-runs these; quote their real output in your report)
```
powershell -NoProfile -File apps/vr/tools/build.ps1                     # expected: exit 0
UnrealEditor-Cmd MageArenaVR.uproject -ExecCmds="Automation RunTests MageArena.Intermission;Quit" -unattended -nullrhi -nosplash -log
                                                                         # expected: all MageArena.Intermission.* green
UnrealEditor-Cmd MageArenaVR.uproject -ExecCmds="Automation RunTests MageArena.;Quit" -unattended -nullrhi -nosplash -log
                                                                         # expected: all MageArena. green
UnrealEditor-Cmd MageArenaVR.uproject -ExecCmds="Automation RunTests MageArenaDesign.;Quit" -unattended -nullrhi -nosplash -log
                                                                         # expected: no red beyond the known list; FullSeated time reported
node apps/vr/tools/lint/format-check.mjs && node apps/vr/tools/lint/unused-members.mjs
                                                                         # expected: OK
```

## Report
Write `runs/T28/REPORT.md`: the C2 path, what you built (paths), each acceptance command with its real output, the day-length
effect, anything you could not do and the exact error, and decisions the card did not specify.

## Report (part 1, 2026-10-25; the same text as runs/T28/REPORT.md)

C2: `docs/campaign/05-c2-slice-story-bible-and-script.md` (sections 4, 6, 8), data `apps/vr/data/vr/story/slice.json`
(read, never re-authored). DECISIONS 2026-10-24 "C2 rulings D1-D4 approved as recommended": T28 proceeds on its
non-audio parts; audio stays deferred.

**Built: NO.** Another lane held the machine's single Unreal slot. No UnrealEditor, UnrealEditor-Cmd, build.ps1 or
capture ran. Nothing below has been compiled or run in Unreal. Branch `autopilot/accepted-idea-delivery-0e219ccf`,
commits `7523e85`, `6772342`, `f573dfe` (+ the card commit).

### What was built (paths)

- `apps/vr/Game/Source/MageArenaVR/Session/StorySlice.{h,cpp}` (new): `FStorySlice` loads slice.json strictly. A
  missing, unreadable, non-version-1 or malformed file loads nothing and is never half-read. It keeps the scenes
  (floor <= natural <= max <= 20 s, three stones, labels of 5 words or fewer, `writes` must be
  `arena.intermission.<n>.choice`), the when/andWhen clauses (first match wins; a flag that is not set or has the wrong
  type does not match), and every beat's `floorS`.
- `Session/ArenaFlags.{h,cpp}`: `FArenaFlags::IntermissionKey(n)`.
- `Session/ArenaSession.h`, `DayFlow.cpp`, `SessionFlow.cpp`, `ArenaSession.cpp`:
  - day.json: `introS` is now an array (or a scalar), read per Tiro wave index by `FDayTuning::IntroSeconds`. The new
    `closingCardS` is required.
  - Beat floors: the prologue skip and the closing-card skip work, and their stone shows, only from the beat's
    `floorS`. The script plays its stone clip at the floor. The closing card ends itself at `closingCardS`.
  - Scenes, in the day only: `GetActiveScene`, `AdvanceScene`, `BeginScene`, `AdvanceSceneChoice`,
    `WriteSceneChoice`, `SceneLine`, `ResetScene`.
    - The scene's lines show one at a time on the prompt, each for its `dwellS`. After them comes the old line that
      names the gesture.
    - Scenes 2 and 3 offer their own stones. A palm held offerHoldS writes the stone index.
    - Scene 1 runs the T20 pick unchanged (`AdvancePick`). `ChoosePick` also writes `arena.intermission.1.choice`.
    - A raised palm below the scene's floor is dropped. Nothing is kept for later.
    - `maxS` continues the day with no flag and no charge.
    - Without a slice the day plays exactly as before, and a warning is logged.
- `Session/SessionPresentation.cpp`: the scene's labels and the hold glow on the three stones.
- `Session/DayCapture.cpp`, `CollarCapture.cpp`, `TeachCapture.cpp`: the waits for the 16 s tablet and the 6 s prologue
  floor are longer.
- `apps/vr/data/vr/day.json`:
  - prologueMaxS 10 -> 45
  - introS 3 -> [8, 8, 12, 12]
  - aftermathTabletS 4 -> 16
  - closingCardS 34 (new)
- `Tests/IntermissionSceneTests.cpp` (new), `MageArena.Intermission.*`:
  - SliceData
  - WhenSelectsScene
  - ChoiceWritesFlag
  - TimeoutWritesNone
  - NeverPast20
  - FloorGatesPalm
  - PickUnreachableInBout
  - WithoutSlice
  - DayLengthBudget
- `Tests/DayTests.cpp`, `Tests/SessionTests.cpp` (FullSeated): the tests follow the new timings. The intermission palm is
  raised once the floor has passed.

### Node checks (real output, 2026-10-25, on the committed branch)

```
$ node apps/vr/tools/lint/format-check.mjs
format-check OK: 15 changed C++ file(s) since deaa15b (clang-format version 19.1.5)
$ node apps/vr/tools/lint/unused-members.mjs
unused-members OK: 624 private/protected members in 44 classes under apps\vr\Game\Source
$ node apps/vr/tools/check-overlay.mjs
overlay OK: 14 files
overlay sha256 9bc7dae9e0b62ca1ce39e5a01d4bea917a7a71d76edee5cf2e2ae53f8fb2e765
$ node apps/vr/tools/check-pin.mjs
pin OK: 9 files @ baeac66
conformance OK: 45 vectors
$ node tools/check-commits.mjs
check-commits OK: 3 commit subject(s) since deaa15b
```
The merge gate's other steps (agy-guard test, the pin and overlay self-tests, the leonardo redaction test, repo-law) also
passed locally. No Node test reads day.json or slice.json other than check-overlay (JSON syntax).

### Expected day-length effect (from the day.json and slice.json deltas)

FullSeated plays the rushed path: every skip lands at its floor, and the palm is raised at each scene's floor.

| Change | Delta |
|---|---|
| Prologue skip at its 6 s floor (first-launch part) | +6 s |
| Intros 3 s -> 8, 8, 12, 12 s | +28 s (4 bouts) |
| Intermissions: 0 s -> their 3 s floor | +9 s (3 scenes) |
| Tablet 4 -> 16 s | +12 s |
| Closing card: the 6 s floor | +6 s |
| **A FullSeated day that closes** | **+61 s** |
| **Today's FullSeated, which stops at bout 3 (DF-011)** | **+31 s** (6 + 5 + 5 + 9 + 3 + 3) |

The intermissions add 9 s (6 s on today's run). The three scenes are allowed 60 s (3 x maxS 20), so the scenes move
FullSeated by no more than the scene time allowed. `MageArena.Intermission.DayLengthBudget` asserts these numbers from
the loaded data.

The natural path (lines read, C2's natural column) is 213 s of story against T21's ~54 s. That is C2's 7.0-8.5 min day.
This lane cannot measure it.

Risk, unverified: the kernel steps idly during an intermission. If those steps consume RNG, longer intermissions could
shift the later bouts' outcomes, and with them the +31 s.

### Decisions the card did not specify

1. **The floor gates the skip gestures only:**
   - the prologue's centre stone
   - the closing card's centre stone
   - an intermission's raised palm
   
   It does not gate a stone *choice*: a deliberate 0.40 s hold is not a reflex, and the T20 pick stays untouched. The
   ritual (floor 10) is left to T27, which owns its timings. The intro and tablet floors are not used, because those
   beats have no skip today.
2. **A palm raised below a floor is dropped, not deferred**, as C2 says: "below this the skip gesture does nothing". A
   stone hold completed before the prologue floor does not skip either. The player holds again from the floor.
3. **A skip stone shows only from its floor.** The prologue's stone appears at 6 s, the closing card's 6 s after the
   tablet.
4. **closingCardS = 34**, C2's max, used as the closing card's timeout from the end of the tablet. Before, the closing
   card waited forever for the stone.
5. **introS accepts a scalar or an array of 1-8 numbers.** A wave past the end uses the last entry.
6. **The centre stone of scenes 2 and 3 writes 1**, the same as the T20 pick's centre stone. C2's "(0, 2; none on
   timeout)" reads as the meaningful stances. A centre hold is still recorded, so it can be told apart from a timeout.
   A raised palm writes nothing.
7. **The scene's lines replace the intermission prompt while they stand.** Once they are done, the old
   `intermission`/`intermission.pick` line returns, naming the gesture.
8. **When the T20 pick is offered, it is the scene's choice** even if a scene lacked `keepsExistingPick`, so the pick
   can never be bypassed.
9. **Scenes only in the day (BeginArc).** The single-bout `Start()` path (PresetTests) is unchanged.
10. **A skipped slice logs a Warning, not an Error.** The day still begins.

### Out-of-path test edits the new day.json values need (not made: outside this lane's paths)

The mandated timings turn these tests red until the following edits land:

| Tests | Edit |
|---|---|
| MageArena.Collar.LifetimeSurvives, MageArena.Collar.TabletColumns | `Tests/CollarTests.cpp:372` `RunWhile(Session, TEXT("intro"), 5.0)` -> `15.0` |
| MageArena.Fist.MissioSpare, MissioFinish, MissioTimeout, NoChoiceMidBout and the no-rival case | `Tests/FistTests.cpp:162` `MissioRunWhile(Session, TEXT("intro"), 5.0)` -> `15.0` |
| MageArena.Pause.TrackingLoss | `Tests/PauseTests.cpp:230` `Enter < 16.0` -> `Enter < 51.0` |
| MageArena.Pause.TeachResumes | `Tests/PauseTests.cpp:506` `Elapsed < 16.0` -> `Elapsed < 51.0` |
| MageArena.Teach.MissRepeats | `Tests/TeachTests.cpp:308` `16.0` -> `51.0` |
| MageArena.Teach.PerfectOrThree | `Tests/TeachTests.cpp:411` `Elapsed < 45.0` -> `Elapsed < 80.0` |

The comments there that say "10 s" become "45 s". The Pause and Teach edits follow from prologueMaxS 45, the Collar and
Fist edits from intros of up to 12 s.

`SessionInvariantTests` (FullSeated twin) and `SharedMaterialTests` raise the palm on entering an intermission. That palm
is now below the floor and is dropped, so each intermission ends at its 20 s timeout. Both tests stay inside their
guards. They are not red, only slower.

### PART 2 - Unreal proof owed

On Wolf, with the single Unreal slot free. Copy the MetaXR plugins into the worktree first (robocopy, never a junction).
Build.version must read 5.8.3. Regenerate the sigil corpus (CLAUDE.md, Build).

```
powershell -NoProfile -ExecutionPolicy Bypass -File apps/vr/tools/build.ps1
    # must show "Result: Succeeded" in the log (not only exit 0). First risk: compile errors, since this code was never built.
UnrealEditor-Cmd MageArenaVR.uproject -ExecCmds="Automation RunTests MageArena.Intermission;Quit" -unattended -nullrhi -nosplash -log
    # all 9 MageArena.Intermission.* green
UnrealEditor-Cmd MageArenaVR.uproject -ExecCmds="Automation RunTests MageArena.;Quit" -unattended -nullrhi -nosplash -log
    # all MageArena. green, MageArena.Invariants.* included, AFTER the six out-of-path edits above;
    # without them expect the Collar/Fist/Pause/Teach reds listed there and no others
UnrealEditor-Cmd MageArenaVR.uproject -ExecCmds="Automation RunTests MageArenaDesign.;Quit" -unattended -nullrhi -nosplash -log
    # no red beyond the known list (Duel.FireSeated, Session.FullSeated under DF-011); quote the
    # "MAGEVR_MEASURE Session.FullSeated" line and compare dayWithTeachS with the previous run: expected +31 s (+61 s if the day closes)
node apps/vr/tools/lint/format-check.mjs && node apps/vr/tools/lint/unused-members.mjs
    # OK (both pass now)
```

### PART 2 - result (2026-10-25, T62, `docs/research/BRAZIERS-T62-2026-10-25.md` section 3)

- **Build:** `Result: Succeeded` on the first build (cold worktree, 417.86 s). The T28 C++ needed **no compile fix**.
- **The six out-of-path test edits** landed as the table above lists them (T62 commit "T28: the six owed test edits");
  `TeachTests.cpp:308` was also clang-formatted, since format-check reads changed lines.
- **MageArena.Intermission:** `Found 9 automation tests`, 9 x `Result={Success}`, `EXIT CODE: 0`.
- **MageArena.:** `Found 321 automation tests`, 321 x Success, `EXIT CODE: 0` (after T62's golden re-record).
- **MageArenaDesign.:** 5 / 7; only `Duel.FireSeated` and `Session.FullSeated` red (DF-011).
  `MAGEVR_MEASURE Session.FullSeated dayWithTeachS=205.31` against 187.85 (T56 log): **+17.46 s, not +31 s.**
- **The RNG risk is real.** The timing deltas are exactly as predicted (prologue +5.99, intros +5 +5 +9, intermissions
  +3.02 +3.00: +31.0 s), but the fights moved: bout 1 39.38 = 39.38 s; bout 2 27.50 -> 27.47 s; **bout 3 (Brennic)
  42.50 -> 29.00 s**, still lost, cracks 2 -> 0. The session pumps the kernel with idle input in the intermission
  (`Session/SessionFlow.cpp:1462`), and the kernel draws from one sequential stream (`Kernel/ArenaKernel.cpp:362`),
  the likely cause: a longer idle phase changes the state a later bout starts from (not traced further). Not changed here: it needs a ruling (freeze the
  kernel during the intermission, or re-seed each bout), because either moves the FullSeated and design-gate numbers.
