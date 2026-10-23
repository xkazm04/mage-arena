# T28 - Intermission scenes on the stones: one optional choice, never blocking

Status: open. **C2 is drafted: `docs/campaign/05-c2-slice-story-bible-and-script.md`** (2026-10-21), with the three
intermission scenes in its section 6 and the authored data at **`apps/vr/data/vr/story/slice.json`** (committed, inert until
this card reads it). Still blocked, but now on the **owner's ruling of C2 decisions D1-D3**, not on the writing. D3 is the one
that gates this card: C2 authors **three** intermission choices, which deviates from
`docs/campaign/04-vr-campaign-design.md` section 2 ("one optional intermission choice") and follows this card's own wording. Runs on the build machine (Wolf, the x64 UE host: UE 5.8.3 editor). Written 2026-10-20 on the ARM64
laptop, where none of it runs.
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
