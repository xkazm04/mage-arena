# Architecture review 2026-10: `apps/vr/Game/Source/MageArenaVR`

The first run of the `codebase-architecture-review` charter. It is report-only. It changes no code, and it ran no
build, no editor and no automation run. It reads the committed tree at `393a65f` (`master`), on 2026-10-07. Every
number below was measured with a command listed in section 5.

Milestone 3 needs hotspots #2 (`Kernel/ArenaKernel.cpp`, 1350 lines) and #3 (`Kernel/VrRules.cpp`, 1167 lines) split
before 2026-10-31 (`BASELINE-2026-10-07.md` section 2.2; #1 was done in sweep `b44ec46`). Section 2 picks the seams.

**The T16 caveat.** Card `apps/vr/tasks/T16-style-pick-replay-capture.md` is open. Its worker has uncommitted edits to
`Session/ArenaSession.cpp`, `Session/ArenaSession.h`, `Session/SessionPresentation.cpp`, `Greybox/ArenaLayout.cpp` and
`MageArenaVR.Build.cs`. This review read only the committed versions. A finding in those files is marked
**pending T16**. Nothing here asks T16 to change course. T16 uses `StateHash` and the event log for its replay test;
the split below keeps both unchanged.

## 1. Findings

Ranked by what they cost if left: first before 2026-10-31, then at V1 (1-18 Nov).

### F1. The VR overlay is written as inline branches inside the pinned kernel port

- **Problem.** `ArenaKernel.cpp` is the C++ port of the TS kernel, and VR overlay rules are threaded through it as
  `if (Rules ...)` branches, so every new seated rule edits the file that conformance protects.
- **Evidence.** 67 lines of `ArenaKernel.cpp` mention `Rules`; 11 are `if (Rules` branches. The player power rule is
  written three times, in two shapes: `ArenaKernel.cpp:552-555` (bolt release), `:663-670` (split-hand cast) and
  `:680-683` (normal cast). The split-hand copy does not check `Rules->bActive`; the other two do. Today that is
  unreachable from the session: a failed load returns false (`VrRules.cpp:915`) and both callers abort
  (`Session/SessionFlow.cpp:288`, `Session/ArenaSession.cpp:258`). Four of the file's seven commits are overlay cards
  (T10 `3f182cc`, T11 `1459532`, T12 `623bdff`, T14 `b3b651e`).
- **Cost.** DECISIONS 2026-10-03 adds Earth B (stone form) and Air D (air form) after Fire C and Water A/E. At the
  current pattern each adds more branches to `StartCast` (`:589-707`) and `ResolveHit` (`:1066-1134`). Each one is a
  chance to change the `Rules == nullptr` path, which only the 45 vectors guard. Before 10-31 that is churn in hotspot
  #2 right after it is split. At V1 it is the file every seated tuning change has to touch.
- **Change.** Move the power rule into one `FVrRuleset` method (for example `PlayerPowerMult(const FActor&, const
  FSpell*)`) and call it from the three sites. Keep today's results exactly, including the missing `bActive` check on
  the split path; whether that asymmetry is wanted is a separate decision. Then set one rule for new defences: each
  enters `ArenaKernel.cpp` as one call to a named `FVrRuleset` method at an existing hook, the way `TickStaff`,
  `TickWalls` and `BurnCrossers` already do (`:1226-1247`).
- **Size.** S.
- **When.** Before 10-31, as the commit after the split in section 2.
- **Lane.** The kernel's conformance to the TS source.

### F2. `FArenaSession` is the largest unit in the module, and it was never ranked (pending T16)

- **Problem.** One class carries the whole session: bout start, input routing, comfort, pause, teach and the arc.
- **Evidence.** The class body is `Session/ArenaSession.h:87-369` (283 lines) with 97 method declarations. 104
  member definitions are split over two files: 54 in `Session/ArenaSession.cpp` (1938 lines) and 50 in
  `Session/SessionFlow.cpp` (1372 lines). The bout-start block that loads the ruleset is written twice
  (`ArenaSession.cpp:250-262` and `SessionFlow.cpp:281-291`). BASELINE section 2.2 excluded these files, so they are
  in no hotspot list.
- **Cost.** Before 10-31: none that can be acted on, because T16 holds the files. At V1: the live hand source, the
  device-loss path and Earth/Air defences all land here. Two workers cannot edit this class at the same time.
- **Change.** After T16 commits, re-run BASELINE's code script to rank these files. Then split `FArenaSession` by
  responsibility, starting with one shared bout-start function for the two duplicated blocks.
- **Size.** L.
- **When.** After T16 commits; the split itself at V1, or later if the re-rank says so.
- **Lane.** T16.

### F3. The V1 live hand source has no slot, and a plain `Build.cs` dependency would break every worktree build

- **Problem.** `IHandSource` exists, but the subsystem that every detector reads from is hard-wired to two clip
  players, and the MetaXR plugins it would need are optional and absent from builder worktrees.
- **Evidence.** One class implements `IHandSource` (`Hands/HandClipPlayer.h:16`). `Hands/HandInputSubsystem.h` owns
  `WardPlayer` and `ActionPlayer` as `UHandClipPlayer`; its `GetLatest` merge (`HandInputSubsystem.cpp:170-191`) is
  written in terms of those two players. 29 files reference `UHandInputSubsystem`. `MageArenaVR.Build.cs` names no
  Oculus module today. The `.uproject` marks `OculusXR` and `OculusInteraction` `Optional` (lines 17-18), and
  `apps/vr/Game/Plugins/MetaXR/` is git-ignored (`.gitignore:10`). CLAUDE.md records worktree builds without the
  plugins (393 s, 2026-10-07). STACK-OPPORTUNITIES F8 lists the live `bSystemGesture` flag from `OculusXRInput` as
  not proven, waiting on this dependency.
- **Cost.** At V1: adding `"OculusXRInput"` unconditionally to `PublicDependencyModuleNames` makes every worktree
  without the 6.2 GB plugin tree fail to build. The merge gate cannot see that, because worktrees only run the pin
  check. A device source bolted into the clip-lane merge would also give the detectors two meanings of "latest frame".
- **Change.** Two parts. (a) In `Build.cs`, add the Oculus dependency only when
  `Plugins/MetaXR/OculusXR.uplugin` exists under the project directory, and set a define
  (`MAGEARENA_WITH_METAXR=0|1`). The device source `.cpp` compiles to nothing when it is 0. (b) Give
  `UHandInputSubsystem` one optional `IHandSource*` live source. When it is set, `GetLatest` and `OnHandFrame` forward
  from it, and the two clip lanes stay the desktop path. Detectors keep binding to the subsystem, so the 29 files do
  not change.
- **Size.** M.
- **When.** Agree the design before 10-31; land it at V1. Part (a) waits for T16, which holds `Build.cs`.
- **Lane.** Hands/Gestures, and T16 for `Build.cs`.

### F4. Three JSON loops still bind a temporary pair per field

- **Problem.** The loop pattern that sweep finding 5 fixed in `KernelData.cpp` is still in three places.
- **Evidence.** `Session/SessionFlow.cpp:157`, `Tests/KernelConformanceTests.cpp:393` and `:1257` iterate
  `FJsonObject::Values` as `const TPair<FString, TSharedPtr<FJsonValue>>&`. In UE 5.8 the key type is
  `UE::FSharedString`, so each iteration builds a temporary pair. Sweep `b44ec46` (finding 5, commit `73f73a2`)
  showed that clang-cl with `/W4 -Werror` rejects this; MSVC accepts it.
- **Cost.** At V1: the Quest build compiles with clang. If that build treats the warning as an error, the first
  Android compile fails in the conformance test and the session arc. Not proven: no Android compile has run here.
- **Change.** The same edit as `73f73a2`: `const auto&` and one explicit `FString Key`.
- **Size.** S.
- **When.** Before V1. Any time; `SessionFlow.cpp` is not a T16 file.
- **Lane.** The kernel's conformance to the TS source (the conformance test), and the session.

### F5. Five loader helpers in `VrRules.cpp` have external linkage

- **Problem.** The anonymous namespace closes at `VrRules.cpp:320`, so the proposal helpers after it are global
  functions.
- **Evidence.** `ProposalRequested` (`:536`), `InRange` (`:546`), `ReadOptional` (`:555`), `ReadCount` (`:570`) and
  `ApplyProposal` (`:590`) are declared in no header and called from no other file (0 hits outside `VrRules.cpp`).
- **Cost.** Small, and it grows with the split. A same-named function in any other file is a link error, and
  `ReadCount` and `InRange` are generic names. The sweep's own rule is to keep helper names unique for unity blobs.
- **Change.** Put them in the anonymous namespace when they move in section 2.
- **Size.** S.
- **When.** Before 10-31, inside the `VrRules.cpp` split commit.
- **Lane.** The kernel's conformance to the TS source (overlay side; the pinned path does not call them).

## 2. Splitting hotspots #2 and #3

Both files should be split. Both have a seam where nothing crosses but a declared function, so the split is a move
of whole functions, with no edits inside them.

### 2.1 `Kernel/ArenaKernel.cpp` (1350 lines, 50 top-level definitions, 7 commits)

**Commit A: `Kernel/StateHash.cpp`.**
- Moves: `MixByte` to `MixZone` (`:26-399`, 17 functions) into an anonymous namespace, and `StateHash`
  (`:1297-1339`). About 420 lines.
- No header change. `StateHash` stays declared in `ArenaKernel.h`, so its 24 includers do not change.
- Why this seam: nothing crosses it. No `Mix*` call exists outside those two ranges (0 hits). The hash changes when
  `SimTypes.h` gains a field (`5cdb678`), not when a rule changes. It is C++-only: the header says the TS hash is a
  different function, so it is not part of the TS port. The field-guard test then points at one small file.
- Also update the comment at `Tests/StateHashCoverageTests.cpp:400`, which names `ArenaKernel.cpp` as the home of
  the `Mix` functions. Comment only.

**Commit B: `Kernel/ArenaThreats.cpp` and a module-private `Kernel/ArenaThreats.h`.**
- Moves: `HitListContains` (`:21-24`), `KindFromFamily` (`:401-412`), `UpdateTelegraphs` (`:709-814`),
  `WallClaimsProjectile` (`:816-830`), `UpdateProjectiles` (`:832-951`), `ResolveHit` (`:1066-1134`) and
  `SpawnProjectile` (`:1136-1166`). About 365 lines.
- The new header declares only `UpdateTelegraphs` and `UpdateProjectiles`. `ResolveHit` (24 outside call sites) and
  `SpawnProjectile` (15) stay declared in `ArenaKernel.h`.
- Why this seam: `StepArena` has two phases. The actor phase is the per-actor loop (`UpdateWard`, `MoveActor`,
  `ReleaseCast`, `StartCast`). The world phase runs after it (`:1246-1247`): telegraphs, then projectiles. Hit
  resolution is the rule every school calls. Only the two world-phase calls cross the seam. The moved functions call
  no actor-phase helper (0 hits for `Maximum`, `UpdateWard`, `MoveActor`, `ReleaseCast`, `StartCast`, `Mix*`).
- What stays in `ArenaKernel.cpp`: `Maximum`, the actor phase, the small public functions (`SimTicks` to
  `Interrupt`, `UpdateClock`), `StepArena`, `ResetWave` and `FFixedStepper`. About 565 lines.

If the budget allows only one commit, do A. It is the bigger cut and needs no new header.

### 2.2 `Kernel/VrRules.cpp` (1167 lines, 46 top-level definitions, 5 commits)

**Commit C: `Kernel/VrRulesLoad.cpp`.**
- Moves: the anonymous helpers `:20-319` (`GVrDataDirOverride`, `VrFile`, `Fail`, `LoadJsonFile`, the `Need*` readers,
  `Near`, `FindEnemy`, `FindAttack`, `ReadLayoutMeasure`, `ReadThrow`), the proposal helpers `:536-647` (into the
  anonymous namespace, finding F5), `SetVrDataDirForTest` (`:649-652`) and `LoadVrRuleset` (`:654-920`). About 690
  lines.
- What stays in `VrRules.cpp`: `FVrAabb`, the hold-box and narrow-view geometry (`:322-534`), the fire walls
  (`:922-1066`) and the staff and split-hand runtime (`:1068-1167`). About 480 lines.
- No header change. `VrRules.h` keeps every declaration; its 14 includers do not change.
- Why this seam: the loader reads files, the command line and the environment once per bout. The rest is sim code
  that `StepArena` calls every tick. No loader helper is used in the kept part (0 hits). Every overlay card (T10, T11,
  T12, T14) added fields to the loader; the geometry changed less.

**Commit D (optional): split `LoadVrRuleset`.** It is 267 lines (`:654-920`). Split it per JSON section (dais and
layout, movement and throws, split hands and staff, fire wall, pressure and power, comfort) the way sweep `b44ec46`
split `KernelData::Load`. Do it after commit C, in its own commit.

### 2.3 What must not change

- `ArenaKernel.h`, `VrRules.h` and `SimTypes.h` declarations. Commit B adds one private header and nothing else.
- Every moved statement, unchanged. The order of calls in `StepArena`, unchanged.
- The `Rules == nullptr` path. The 45 vectors in `apps/vr/data/conformance/` stay byte-identical (`git diff` on that
  folder is empty) and `MageArena.Kernel.Conformance` passes.
- All `MageArena.Kernel.*` tests: 32 declarations, 76 results with the conformance vectors (sweep section 2.1).
- `StateHash` coverage: `Tests/StateHashCoverageTests.cpp` passes, and the hash values that the determinism tests
  compare do not move.
- No new anonymous-namespace name that already exists in another `.cpp` (unity blobs). Grep each name first, as the
  sweep did. `Fail`, `NeedNumber`, `NeedBool`, `NeedString` and `NeedObject` already exist in both `KernelData.cpp`
  and `VrRules.cpp` with different signatures; they are overloads and must stay that way.
- The boot hash log line (9 files, SHA1 `11F9B16C…36C2FF`).

### 2.4 The proof the sweep must show

- `powershell -NoProfile -File apps/vr/tools/build.ps1` exits 0 after each commit.
- The full `MageArena` suite pass count is unchanged against a run on the base commit before the first change. F8
  reported 158 of 161 at `1e60a1c`, with the three known reds. A per-test diff of the before and after results is
  empty.
- `node apps/vr/tools/check-pin.mjs` exits 0.
- Line counts of the old and new files, before and after, from `wc -l`.

## 3. Declined

- `UpdateClock` is exported in `ArenaKernel.h` with 0 outside callers. Harmless; not worth an edit.
- `KernelData.cpp` and `VrRules.cpp` both define `Fail` and four `Need*` helpers. Their error sinks differ, so one
  shared helper set would add a parameter everywhere for no gain.
- `Hands/MageArenaPlayerController.cpp:12` includes `Session/ArenaSession.h`, a back edge from input to session. One
  include, for the pause and headset-removed keys. Revisit only if the hand code becomes its own module.
- `Hands/MageSettings.cpp:364-371` loads the whole VR ruleset to read three narrow-view numbers. One extra JSON read
  at startup.
- The kernel as its own UE module. `Kernel/` already includes only `Kernel/` (60) and `Combat/` (1), and it has no
  consumer outside this module.
- The largest test files (`WardTests.cpp` 2077, `DefenceTests.cpp` 1449, `KernelConformanceTests.cpp` 1356). Their
  size is cases, not structure.
- `Session/SessionPresentation.cpp` (1406 lines). Pending T16; F2's re-rank covers it.
- `ParseFireSpell` in `KernelData.cpp`. Already sweep finding 6; not repeated here.

## 4. What this review did not do

- No build and no test run. Every claim about behaviour comes from reading code and from earlier runs that are
  cited (sweep `b44ec46`, STACK-OPPORTUNITIES F8).
- It did not read the uncommitted T16 edits, so F2 and the `Build.cs` part of F3 may move when T16 commits.
- F4's Android failure is a risk, not a measurement.

## 5. Commands run for this file, with exit codes

Run in this worktree at `393a65f` on 2026-10-07, from the repo root unless the path says otherwise. `S` is
`apps/vr/Game/Source/MageArenaVR`. For a pipeline, the exit code is the last stage's. `grep -c` exits 1 when it counts
0; here that 0 was the expected result.

| # | Command | Exit | Result |
|---:|---|---:|---|
| 1 | `git rev-parse --short HEAD` | 0 | `393a65f` |
| 2 | `wc -l $S/Kernel/ArenaKernel.cpp $S/Kernel/VrRules.cpp $S/Session/ArenaSession.cpp $S/Session/SessionFlow.cpp $S/Session/SessionPresentation.cpp $S/Session/ArenaSession.h` | 0 | 1350, 1167, 1938, 1372, 1406, 421 |
| 3 | `git log --follow --no-merges --format='%h %s' -- $S/Kernel/ArenaKernel.cpp` | 0 | 7 commits: `5cdb678`, `b3b651e`, `623bdff`, `1459532`, `3f182cc`, `4d7596e`, `050dd16` |
| 4 | `git log --follow --no-merges --format='%h %s' -- $S/Kernel/VrRules.cpp` | 0 | 5 commits: `c9ae6f1`, `b3b651e`, `623bdff`, `1459532`, `3f182cc` |
| 5 | `grep -nE '^[A-Za-z].*\(' $S/Kernel/ArenaKernel.cpp \| grep -v ';$' \| wc -l` | 0 | 50 |
| 6 | `grep -nE '^[A-Za-z].*\(' $S/Kernel/VrRules.cpp \| grep -v ';$' \| wc -l` | 0 | 46 |
| 7 | `grep -c 'Rules' $S/Kernel/ArenaKernel.cpp` | 0 | 67 |
| 8 | `grep -c 'if (Rules' $S/Kernel/ArenaKernel.cpp` | 0 | 11 |
| 9 | `grep -n 'Rules->Power\.\(BoltDamage\|LineDamage\)' $S/Kernel/ArenaKernel.cpp` | 0 | lines 552, 554, 663, 665, 667, 669, 682 |
| 10 | `grep -n 'Out.Split.bEnabled = true\|Out.bActive = ' $S/Kernel/VrRules.cpp` | 0 | lines 825, 904, 915 |
| 11 | `grep -n 'LoadVrRuleset(Rules' $S/Session/SessionFlow.cpp $S/Session/ArenaSession.cpp $S/Hands/MageSettings.cpp` | 0 | `SessionFlow.cpp:288`, `ArenaSession.cpp:258`, `MageSettings.cpp:366` |
| 12 | `grep -n 'Mix[A-Z]' $S/Kernel/ArenaKernel.cpp \| awk -F: '$1>399 && ($1<1297\|\|$1>1339)' \| wc -l` | 0 | 0 |
| 13 | `awk 'NR>=401&&NR<=412 \|\| NR>=709&&NR<=951 \|\| NR>=1066&&NR<=1166' $S/Kernel/ArenaKernel.cpp \| grep -cE '\b(Maximum\|UpdateWard\|MoveActor\|ReleaseCast\|StartCast\|Mix[A-Za-z]+)\('` | 1 | 0 |
| 14 | `awk '!(NR>=401&&NR<=412 \|\| NR>=709&&NR<=951 \|\| NR>=1066&&NR<=1166)' $S/Kernel/ArenaKernel.cpp \| grep -cE '\b(HitListContains\|KindFromFamily\|WallClaimsProjectile\|UpdateTelegraphs\|UpdateProjectiles)\('` | 0 | 3: the `HitListContains` definition at `:21` (it moves too), and the calls at `:1246`, `:1247` |
| 15 | `grep -rnE '\bResolveHit\(' --include=*.cpp $S \| grep -vc Kernel/ArenaKernel.cpp` | 0 | 24 |
| 16 | `grep -rnE '\bSpawnProjectile\(' --include=*.cpp $S \| grep -vc Kernel/ArenaKernel.cpp` | 0 | 15 |
| 17 | `grep -rnE '\bUpdateClock\(' --include=*.cpp $S \| grep -v Kernel/ArenaKernel.cpp \| wc -l` | 0 | 0 |
| 18 | `grep -rlE '#include "Kernel/ArenaKernel.h"' --include=*.cpp --include=*.h $S \| wc -l` | 0 | 24 |
| 19 | `grep -rlE '#include "Kernel/VrRules.h"' --include=*.cpp --include=*.h $S \| wc -l` | 0 | 14 |
| 20 | `grep -rln 'FVrRuleset' --include=*.cpp --include=*.h $S \| wc -l` | 0 | 23 |
| 21 | `sed -n '536p;546p;555p;570p;590p' $S/Kernel/VrRules.cpp` | 0 | the five helper signatures, none `static` |
| 22 | `grep -rnE '\b(ProposalRequested\|InRange\|ReadOptional\|ReadCount\|ApplyProposal)\(' --include=*.cpp --include=*.h $S \| grep -v Kernel/VrRules.cpp \| wc -l` | 0 | 0 |
| 23 | `grep -nE '\b(VrFile\|Fail\|LoadJsonFile\|NeedNumber\|NeedBool\|NeedString\|NeedObject\|NeedArray2\|Near\|FindEnemy\|FindAttack\|ReadLayoutMeasure\|ReadThrow\|GVrDataDirOverride)\b' $S/Kernel/VrRules.cpp \| awk -F: '($1>=320 && $1<536) \|\| $1>920' \| wc -l` | 0 | 0 |
| 24 | `grep -nE '^bool (Fail\|NeedNumber)\(' $S/Kernel/KernelData.cpp $S/Kernel/VrRules.cpp` | 0 | `KernelData.cpp:24`, `:70`; `VrRules.cpp:33`, `:58`; different parameter lists |
| 25 | `sed -n '87,369p' $S/Session/ArenaSession.h \| grep -cE '\(.*\)( const)?( override)?;'` | 0 | 97 |
| 26 | `grep -cE '^[^ /].*FArenaSession::' $S/Session/ArenaSession.cpp $S/Session/SessionFlow.cpp` | 0 | 54 and 50 |
| 27 | `grep -rn 'IHandSource' --include=*.h --include=*.cpp $S \| grep -c 'public IHandSource'` | 0 | 1 |
| 28 | `grep -rl 'UHandInputSubsystem' --include=*.cpp --include=*.h $S \| wc -l` | 0 | 29 |
| 29 | `sed -n '170,191p' $S/Hands/HandInputSubsystem.cpp \| grep -c 'Player'` | 0 | 6 |
| 30 | `grep -c 'OculusXR' $S/MageArenaVR.Build.cs` | 1 | 0 |
| 31 | `grep -n 'Optional' apps/vr/Game/MageArenaVR.uproject` | 0 | lines 17, 18 |
| 32 | `git check-ignore -v apps/vr/Game/Plugins/MetaXR/OculusXR.uplugin` | 0 | `.gitignore:10` |
| 33 | `grep -rn 'const TPair<FString, TSharedPtr<FJsonValue>>&' --include=*.cpp --include=*.h $S` | 0 | `SessionFlow.cpp:157`, `KernelConformanceTests.cpp:393`, `:1257` |
| 34 | `grep -rn '#include "Session/' $S/Hands` | 0 | `MageArenaPlayerController.cpp:12` |
| 35 | `grep -rhoE '#include "(Hands\|Gestures\|Kernel\|Combat\|Greybox\|Session)/' $S/Kernel \| sort \| uniq -c` | 0 | `Kernel/` 60, `Combat/` 1 |
| 36 | `git ls-files apps/vr/data/conformance \| wc -l` | 0 | 45 |
| 37 | `grep -rhE -A1 'IMPLEMENT_(SIMPLE\|COMPLEX)_AUTOMATION_TEST' $S/Tests \| grep -c '"MageArena.Kernel'` | 0 | 32 |
| 38 | `git ls-files $S \| grep -cE '\.(cpp\|h)$'` | 0 | 110 |
| 39 | `grep -n 'Status:' apps/vr/tasks/T16-style-pick-replay-capture.md` | 0 | `Status: open` |
| 40 | `node apps/vr/tools/check-pin.mjs` | 0 | `pin OK: 9 files @ 68a4d68` |
