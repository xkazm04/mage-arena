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

## 6. v2/slice before it merges (2026-10-08, at e78871c)

The second run of the charter, on 2026-10-08. It is report-only and docs-only. It reads `v2/slice` at `e78871c` only
through git (`git show`, `git grep`, `git diff`, `git log`, `git merge-tree`), plus one `git archive` of
`apps/vr/Game/Source` into a scratch directory outside the repo (`/tmp/v2s-e78871c`). It ran no checkout, no merge and
no Unreal process, and it did not read the main checkout's uncommitted T16 files.

- **Branches.** The merge base is `76dec98`. Master was `2f089be` when this run started and `772eb42` by the end
  (`772eb42` changes tools and docs only, no C++). The slice is 24 commits ahead and 18 behind (the brief said 17). 8 of
  the 24 are merges: the `v2/T*` branches, and `e78871c` itself, which merges master `76dec98` into the slice. The slice
  is not in master (`git merge-base --is-ancestor e78871c master` exits 1). The roadmap's "merged to master" (V2-ROADMAP,
  "Status 2026-10-08") means master merged into the slice.
- **Scope.** The slice changes 22 files under `Kernel/` and 29 under `Session/`. It adds 36 C++ files and modifies 41.
- **Notation.** Every `file:line` is at `e78871c` unless it says otherwise. `S` is `apps/vr/Game/Source/MageArenaVR`
  and `K` is `$S/Kernel/KernelData.cpp`. Commands are numbered as in section 6.7.

**A correction to SWEEP's lane A list.** The list says v2/slice does not contain `73f73a2`. That is true of the SHA. The
same change is in the slice's history as master's `39efdd3` (command 10), and so are the four other sweep-1 splits. The
three `TPair` loops in `KernelData.cpp` that the list names (`:1293`, `:1385`, `:1406`) are new T23 code, not a fix
that was lost.

### 6.1 Q1. A split plan for `Kernel/KernelData.cpp` at e78871c

**Nothing to carry from master.** No master commit after the merge base touches `KernelData.cpp` or `KernelData.h`
(command 6: 0).
- Master's blob is the merge base's blob (`9194d80`, command 8).
- The merged tree's blob is the slice's blob (`ad6b630`, command 9). So the ranges below are also the ranges on master
  right after the merge.
- The slice has one commit on the file: `4bebb2d` (T23, the Momentum readers and the air rows; 1918 to 2541 lines).
- Re-run command 6 right before the split. If it is no longer empty, derive the ranges again.

**The shape of the file.** 2541 lines, 77 top-level function definitions (command 13), and two anonymous namespaces:
- **The pinned loader** (`:14-2058`). First the shared readers, `:17-231`: `GTextOverrides`, `PinnedPath`, `Fail`,
  `LoadText`, `LoadJson`, `NeedNumber`, `NeedInt`, `NeedBool`, `NeedString`, `NeedObject`, `NeedVec`, `MatchGroup`,
  `MatchDouble`, `FCsvTable`, `LoadCsv`, `Cell` and `LinearStat`. Then one block per data file. It ends in `Load`
  (`:2033-2057`), whose call order is the pin-hash order.
- **The T23 air-row parser** (`:2089-2394`). It has its own error sink, `AirFail(FString&, ...)` at `:2093`.

The call graph has four clusters. No cluster calls into another (command 22: 0 in each direction). Each reaches the rest
of the file only through the shared readers, and is reached only from `Load` or from a public function.

**The seam, set up in commit A.** A cluster that moves still calls the shared readers. They sit in an anonymous
namespace, which another `.cpp` cannot see. So commit A makes these changes; each later commit only adds lines to the
new header:

- `:14` `namespace` becomes `namespace KernelDataLoad`. Its closing brace at `:2058` stays.
- After that brace, one new line: `using namespace KernelDataLoad;`. The public functions at `:2505-2535` then still
  find `Load`, `PinnedPath` and `GTextOverrides` with no edit.
- A new module-private header, `Kernel/KernelDataLoad.h`, declares inside `namespace KernelDataLoad` only what crosses
  a seam.
- Each new `.cpp` wraps its moved loader functions in `namespace KernelDataLoad { ... }`. The bodies then find the
  readers by ordinary lookup, and no statement changes. Public functions (`FindFireSpell`, and everything commit A
  moves) stay at file scope, outside the wrapper, so `KernelData.h` still matches them.

**Why this exposes nothing new:**
- **Inside a unity blob.** An unnamed namespace is already visible to every later file of the blob, through its
  implicit using-directive. The file-scope `using` does the same.
- **Outside a blob.** The readers gain external linkage, but under `KernelDataLoad::`. They cannot meet the anonymous
  `Fail` and `Need*` of `VrRulesLoad.cpp:35`, `Greybox/ArenaLayout.cpp:11`, `Combat/AbsorbResolver.cpp:11` or
  `Session/PlayerPreset.cpp:13`. Inside a `KernelDataLoad` body, an unqualified `Fail` finds `KernelDataLoad::Fail`
  first.
- **No name clashes.** None of the names `KernelDataLoad`, `KernelDataAirRows`, `KernelDataSchools`, `KernelDataWater`
  or `KernelDataRoster` exists in the tree (command 24: 0). None of the 43 file-local names that move is used in any
  other file (command 23: 0).

**Commit A: `Kernel/KernelDataAirRows.cpp`, the VR-owned air rows.**
- **Moves** `:2072-2503`: 432 lines, 13 functions and one constant.
  - `FindAirSpell` (`:2072-2082`) and `KernelDataAirSpellsPath` (`:2084-2087`).
  - The whole second anonymous namespace, `:2089-2394`. It stays anonymous. It holds `AirHeader`, `AirFail`,
    `StrictNumber`, `CellNumber`, `MatchCell`, `CountWord`, `ParseAirShape`, `ParseAirDamage`, `ParseAirMomentum`,
    `ParseAirNotes` and `PublishAirCatalog`.
  - `ParseAirSpellsCsv` (`:2396-2503`).
- **Crosses out:** `MatchGroup`, 5 calls (`:2103`, `:2330`, `:2339-2341`), and the public `KernelData()` (`:2074`).
- **Crosses in:** `KernelDataAirSpellsPath` (`:1582`) and `ParseAirSpellsCsv` (`:1594`), both from `LoadAirSpells`, both
  already public.
- **Stays:** `LoadAirSpells` (`:1580-1599`). It reads `GTextOverrides` and calls `Fail`, and its place in `Load`
  (`:2051`) is part of the step order.
- **`KernelDataLoad.h`:** `MatchGroup`. The new file has `using KernelDataLoad::MatchGroup;` before its anonymous
  namespace. That line, the includes and nothing else are not moved text.
- **Declarations that stay in `KernelData.h`:** `FindAirSpell` (`:441`), `ParseAirSpellsCsv` (`:445`) and
  `KernelDataAirSpellsPath` (`:446`).
- **Includers that change:** none of `KernelData.h`'s 36 (command 25). `KernelDataLoad.h` gets 2 includers.
- **Includes of the new file:** `Kernel/KernelData.h`, `Kernel/KernelDataLoad.h`, `Internationalization/Regex.h`
  (`FRegexPattern` in `MatchCell`), `Misc/Paths.h` and `<cmath>`.
- **Why this seam.**
  - The rows are VR-owned and not pinned (`KernelData.h:434`). They change when the owner signs off
    `docs/schools/AIR.md` or TV accepts CR-007, not when the pin moves.
  - The parser has its own error sink and no `FKernelData`.
  - The tests reach it through the public `ParseAirSpellsCsv` (`Tests/AirTests.cpp`, 3 calls).

**Commit B: `Kernel/KernelDataSchools.cpp`, `spells-fire.csv` and `schools.json` (Heat and Momentum).**
- **Moves** `:909-1576` and `:2060-2070`: 679 lines, 14 functions. They are `SameNumber`, `RequireReach`,
  `PublishFireCatalog`, `ParseFireSpell` (`:946-1110`), `LoadNumberField`, `LoadHeatGain`, `LoadHeatDecay`,
  `ParseHeatThreshold`, `LoadHeatThresholds`, `ParseMomentumThreshold` (`:1290-1334`), `LoadMomentum` (`:1336-1451`),
  `LoadHeat` (`:1453-1513`), `LoadFireSpells` (`:1515-1576`) and the public `FindFireSpell` (`:2060-2070`, which goes
  after the wrapper, at file scope).
- **Crosses out** (call counts, command 16): `Fail` 43, `MatchDouble` 19, `NeedObject` 6, `NeedString` 4, `PinnedPath`
  2, `LoadJson` 1, `LoadText` 1, `NeedNumber` 1, `MatchGroup` 1 (`:1394`), and the public `KernelData()`.
- **Crosses in:** `LoadFireSpells` and `LoadHeat`, both from `Load` (`:2050`).
- **`KernelDataLoad.h` adds:** `PinnedPath`, `Fail`, `LoadText`, `LoadJson`, `NeedNumber`, `NeedString`, `NeedObject`,
  `MatchDouble`, `LoadFireSpells` and `LoadHeat`.
  - `LoadText` stays defined in `KernelData.cpp`. It owns the pin chain and the override map, so the order of reads
    and the pin hash do not change.
- **Declarations that stay in `KernelData.h`:** `FindFireSpell` (`:440`).
- **Includers that change:** none of the 36. `KernelDataLoad.h` gets 3 includers.
- **Includes of the new file:** `Kernel/KernelData.h`, `Kernel/KernelDataLoad.h`, `Dom/JsonObject.h` and `<cmath>`.
- **Why this seam.**
  - It is the largest block, and T23 grew it.
  - DECISIONS 2026-10-03 ("all four schools in every kernel") puts Earth's identity reader here next.
  - It holds sweep 1's open finding 6, `ParseFireSpell` at 165 lines. It also holds T23's `LoadMomentum`, a new
    function over 100 lines (116).
  - Lane A's three `TPair` loops move with it.

**Commit C: `Kernel/KernelDataWater.cpp`, `spells-water.csv`.**
- **Moves** `:354-685`: 332 lines, 10 functions. They are `RequireSpellColumns`, `FindLashArc`, `SpellFromRow`,
  `ParseWaterProjectile`, `ParseWaterLash`, `ParseWaterMire`, `ParseWaterMend`, `ParseWaterMirror`, `ParseWaterLine` and
  `BuildSpells`.
- **Crosses out** (command 18): `MatchDouble` 26, `Cell` 19, `Fail` 4, and the type `FCsvTable` (3 uses).
- **Crosses in:** `BuildSpells`, from `Load` (`:2045`).
- **`KernelDataLoad.h` adds:** `Cell`, `BuildSpells`, and the `FCsvTable` struct itself (`:153-157`).
  - The struct moves whole into the header, because `LoadCsv` (`:159`) and `LoadStats` (`:1841`) keep using it.
  - This is the only move in the plan that is not a whole function.
- **Declarations in `KernelData.h`:** none involved.
- **Includers that change:** none of the 36. `KernelDataLoad.h` gets 4 includers.
- **Includes of the new file:** `Kernel/KernelData.h`, `Kernel/KernelDataLoad.h` and `<cmath>`.

**Commit D: `Kernel/KernelDataRoster.cpp`, `enemies.json` and `arena-tiers.json`.**
- **Moves** `:233-352` and `:687-907`: 341 lines, 9 functions. They are `OptionalNumber`, `ParseAttack`, `ParseEnemy`,
  `LoadEnemies`, `ParseWaveSpawn`, `ParseArenaWave`, `ReadIntList`, `ParseArenaTier` and `LoadArenaTiers`.
- **Crosses out** (command 20): `NeedNumber` 17, `Fail` 16, `NeedString` 10, `NeedInt` 5, `NeedObject` 2, `PinnedPath`
  1, `LoadJson` 1.
- **Crosses in:** `LoadEnemies` and `LoadArenaTiers`, from `Load` (`:2050`).
- **`KernelDataLoad.h` adds:** `NeedInt`, `LoadEnemies` and `LoadArenaTiers`.
- **Declarations in `KernelData.h`:** none involved.
- **Includers that change:** none of the 36. `KernelDataLoad.h` gets 5 includers.
- **Includes of the new file:** `Kernel/KernelData.h`, `Kernel/KernelDataLoad.h`, `Dom/JsonObject.h` and `<cmath>`.

**What stays in `KernelData.cpp`: 31 functions, about 760 lines.** This is the pin chain and its order, in one file.
- The shared readers, `:1-231` (without `FCsvTable`).
- `:1578-2057`: `LoadAirSpells`, `LoadGamesTuning`, `CheckReferences`, `LoadAbsorb`, `LoadCombat`,
  `CheckNerveAuthority`, `LoadStats`, `LoadScaleContract`, `LoadTrainingTuning`, `LoadWaterPresets`, `LoadRuntime` and
  `Load`.
- The public functions at `:2505-2541`: `KernelData`, `KernelDataPinnedPath`, `LoadKernelDataWithOverrides` and
  `WaterLines`.

**`KernelDataLoad.h` at the end** declares, in `namespace KernelDataLoad`:
- the struct `FCsvTable`;
- the readers `PinnedPath`, `Fail`, `LoadText`, `LoadJson`, `NeedNumber`, `NeedInt`, `NeedString`, `NeedObject`,
  `MatchGroup`, `MatchDouble` and `Cell`;
- the steps `BuildSpells`, `LoadEnemies`, `LoadArenaTiers`, `LoadFireSpells` and `LoadHeat`.

`KernelData.h` changes in no commit. It keeps all 8 declarations at `:440-456` and its 36 includers.

**Order.** A first: it sets up the namespace, and only one reader crosses its seam. Then B (the largest block, and the
one that churns), then C, then D. If the budget allows one commit, do A. If it allows two, do A and B: together they
take 1111 lines out.

**The grep that proves nothing crosses.** Commands 14 to 24.
- **Out:** each commit's moved ranges are grepped for every name defined in the part that stays. The hits must be the
  "crosses out" list above.
- **In:** the rest of the file is grepped for every name that moves. The hits must be only the "crosses in" lines.
- Command 22 shows that the clusters do not call each other.
- Command 23 shows that no other file uses a moved file-local name.

**The proof the sweep must show.** Section 2.4, and in addition:
- `git diff apps/vr/data/` is empty.
- The boot hash log line is unchanged (the pin chain stays in `LoadText`).
- `MageArena.Kernel.*`, `MageArena.Air.*` (12 declarations in `AirTests.cpp`) and `MageArena.Fire.*` (9) all pass.
- No new anonymous-namespace name is added. The plan adds only `KernelDataLoad` (named) and keeps every moved name.

### 6.2 Q2. The top 10 hotspots, re-ranked on v2/slice

Command 27 is `node code-v2.mjs e78871c 2026-09-08`, run from the repo root (exit 0, 115 s). It reads 158 C++ files and
61532 lines. The formula is appendix B's: lines at `e78871c` times commits in
`git log e78871c --no-merges --follow --since=2026-09-08`.

| # | File (under `apps/vr/Game/Source/MageArenaVR/`) | Lines | Changes (30 d) | Lines × changes | Baseline rank (`11b9bb2`) |
|---|---|---:|---:|---:|---|
| 1 | `Kernel/KernelData.cpp` | 2541 | 14 | 35574 | #1 (1754 × 7) |
| 2 | `Session/ArenaSession.cpp` | 2290 | 15 | 34350 | excluded (T16) |
| 3 | `Session/SessionPresentation.cpp` | 2268 | 14 | 31752 | excluded (T16) |
| 4 | `Tests/HeldSampleTests.cpp` | 1464 | 10 | 14640 | - |
| 5 | `Kernel/VrRulesLoad.cpp` | 1102 | 11 | 12122 | - (split from #3) |
| 6 | `Session/SessionFlow.cpp` | 1633 | 7 | 11431 | #4 |
| 7 | `Tests/SessionTests.cpp` | 870 | 13 | 11310 | #5 |
| 8 | `Kernel/ArenaKernel.cpp` | 603 | 13 | 7839 | #2 (1350 × 7) |
| 9 | `Session/ArenaSession.h` | 594 | 13 | 7722 | excluded (T16) |
| 10 | `Hands/MageArenaPlayerController.cpp` | 451 | 15 | 6765 | #6 |

How to read it:

- **Nothing is excluded.** The baseline excluded seven files because they were uncommitted T16 work in the main
  checkout. Here every file is read from its committed blob at `e78871c`. This is F2's re-rank: `ArenaSession.cpp`
  and `SessionPresentation.cpp` are #2 and #3, `ArenaSession.h` is #9.
- **`--follow` inflates `VrRulesLoad.cpp`.** It counts 11 commits with `--follow` and 6 without (command 28), because
  git follows the file back through the move `48759c5` into `VrRules.cpp`'s history. On 6 its score would be 6612, just
  under #10 (6765).
- **KernelData's 14 changes** include the 5 sweep-1 refactor commits (`6603a35` to `39efdd3`). The formula counts
  refactors as churn, as the sweep noted.
- **`Tests/HeldSampleTests.cpp` is not slice work.** The slice has no commit on it. The slice has 1464 lines and
  master has 1435 because master changed the file after the merge base.

The adapted script differs from appendix B in two places: the usage comment, and the `EXCLUDE` set, which is empty.

```js
// Code health of the C++ under apps/vr/Game/Source at one commit. Read-only: reads git
// objects and git log, prints to stdout, writes nothing.
// Usage (from the repo root): node code.mjs [rev] [since]   (defaults 11b9bb2, 2026-09-07); run here as: node code-v2.mjs e78871c 2026-09-08
import { execFileSync } from 'node:child_process';

const REV = process.argv[2] || '11b9bb2';
const SINCE = process.argv[3] || '2026-09-07';
// Adapted 2026-10-08 for review section 6: nothing excluded. The excluded files were the main checkout's
// uncommitted T16 work; at e78871c every file is read from its committed blob.
const EXCLUDE = new Set([]);
const git = (args) => execFileSync('git', args, { maxBuffer: 1 << 28 });
const tree = git(['ls-tree', '-r', '-z', REV, '--', 'apps/vr/Game/Source']).toString('utf8').split('\0').filter(Boolean)
  .map((l) => { const [meta, path] = l.split('\t'); return { oid: meta.split(' ')[2], path }; })
  .filter((e) => /\.(cpp|h)$/.test(e.path));
const skipped = tree.filter((e) => EXCLUDE.has(e.path)).map((e) => e.path);
const files = tree.filter((e) => !EXCLUDE.has(e.path));
const blobs = execFileSync('git', ['cat-file', '--batch'], { input: files.map((f) => f.oid).join('\n') + '\n', maxBuffer: 1 << 28 });
let pos = 0;
for (const f of files) {
  const nl = blobs.indexOf(0x0a, pos);
  const size = Number(blobs.subarray(pos, nl).toString().split(' ')[2]);
  const body = blobs.subarray(nl + 1, nl + 1 + size);
  pos = nl + 1 + size + 1;
  let n = 0; for (const c of body) if (c === 0x0a) n++;
  f.lines = n;
  f.changes = git(['log', REV, '--no-merges', '--follow', `--since=${SINCE}`, '--format=%h', '--', f.path]).toString().split('\n').filter(Boolean).length;
  f.score = f.lines * f.changes;
}
const short = (p) => p.replace('apps/vr/Game/Source/MageArenaVR/', '');
const row = (c) => console.log(`| ${c.join(' | ')} |`);
console.log(`rev ${REV}; since ${SINCE}; C++ files ${tree.length}; excluded ${skipped.length}; measured ${files.length}; lines ${files.reduce((a, f) => a + f.lines, 0)}`);
console.log(`excluded: ${skipped.join(', ')}`);
console.log('\n## all C++ files by line count (path relative to apps/vr/Game/Source/MageArenaVR/)');
row(['#', 'file', 'lines', 'changes (30 d)', 'lines x changes']);
[...files].sort((a, b) => b.lines - a.lines || a.path.localeCompare(b.path)).forEach((f, i) => row([i + 1, short(f.path), f.lines, f.changes, f.score]));
console.log('\n## top 10 hotspots by lines x changes');
row(['#', 'file', 'lines', 'changes (30 d)', 'lines x changes']);
[...files].sort((a, b) => b.score - a.score || a.path.localeCompare(b.path)).slice(0, 10).forEach((f, i) => row([i + 1, short(f.path), f.lines, f.changes, f.score]));
```

Its output starts `rev e78871c; since 2026-09-08; C++ files 158; excluded 0; measured 158; lines 61532`, and its top 10
is the table above.

### 6.3 Q3. F1 to F5 on v2/slice

- **F1: worse.** `if (Rules` branches went from 8 to 11 in `ArenaKernel.cpp` and from 3 to 8 in `ArenaThreats.cpp`;
  lines naming `Rules` went from 54 to 65 and from 14 to 30 (master to slice, command 35). Five slice cards edit the two
  files (T17, T19, T20, T22, T23). The power rule is still written three times: `ArenaKernel.cpp:171-173`, `:286-292`
  and `:304-305`. The split-hand copy (`:286`, `:290`) still has no `bActive` check.
- **F2: worse.** The class body is `Session/ArenaSession.h:108-529`: 422 lines with 136 method declarations (master:
  283 and 97). There are 143 member definitions in three files: `ArenaSession.cpp` 60 (2290 lines), `SessionFlow.cpp` 58
  (1633), and the new `DayFlow.cpp` 25 (614, T21). The bout-start ruleset block is still written twice
  (`ArenaSession.cpp:248-259` and `SessionFlow.cpp:303-314`). The class's two main files rank #2 and #3 (section 6.2).
- **F3: unchanged, and its reach grew.** One class still implements `IHandSource` (`Hands/HandClipPlayer.h:16`).
  `HandInputSubsystem.cpp/.h` are unchanged (command 45; `GetLatest` is at `:171`). `Build.cs` has no `OculusXR`. 39
  files now reference `UHandInputSubsystem` (31 on master). One new fact for part (a): T26 added the module's first
  conditional dependency (`MageArenaVR.Build.cs:31`, `if (Target.bBuildEditor)`), which is the same shape F3 proposes
  for MetaXR.
- **F4: resolved on master, worse on the slice.** Master has 0 `const TPair<FString, TSharedPtr<FJsonValue>>&` loops
  (`2a76013`). The slice has 15, and the merged tree has 12 (command 47). The merge resolves the three F4 named. The 12
  left are lane A's list.
- **F5: resolved.** The anonymous namespace of `VrRulesLoad.cpp` runs from `:22` to `:768` and holds the five helpers
  (`:656` to `:710`). The three external functions after it (`:770`, `:779`, `:784`) are declared in `VrRules.h`.
  - A scan of every non-test `.cpp` on the slice finds no file-scope free function that no header declares (command
    49).
  - The same scan finds exactly F5's five functions at `393a65f` (command 51), so it can see the problem.
  - Its only hits in `Tests/` are five helpers at `DefenceTests.cpp:1341-1678`. They predate the slice: master has them
    at `:967-1304`.

### 6.4 Q4. The merge surface

`git merge-tree --write-tree --name-only master v2/slice` exits 0 and prints one line, the merged tree, with no
conflict section:

```
04ec19d5520cee91db964b2edce7dbaf6c26aa57
```

At the start of this run (master `2f089be`) the same command also exited 0, with tree `1e95c2958632c6885403fd0c130f36b627603a58`.

**The merge is clean.** Since the merge base, master changed 38 files and the slice changed 139. Two files are changed
on both sides, in different hunks:

| File | Master | Slice | In the merged tree |
|---|---|---|---|
| `Session/CreaturesCapture.cpp` | `740f5e8` (first pawn without the loop) | `a0e24a0` (T18) | master's fix kept, `:129` |
| `Session/SessionFlow.cpp` | `2a76013` (`const auto&` field loop) | `f37f660` (T20), `397e6f0` (T21), `c467976` (T22) | master's fix kept, `:147` |

**What merge-tree cannot see.** T16's uncommitted work in the main checkout. The slice changes four of T16's five
files:

| File | Slice change |
|---|---:|
| `Session/ArenaSession.cpp` | +414 / -62 |
| `Session/ArenaSession.h` | +178 / -5 |
| `Session/SessionPresentation.cpp` | +932 / -70 |
| `MageArenaVR.Build.cs` | +8 / -1 |

`Greybox/ArenaLayout.cpp` is unchanged on the slice. This review did not read T16's edits, so it cannot say whether they
conflict. Whichever of T16 and the slice lands second resolves those files by hand.

### 6.5 Q5. New structural findings the slice introduces

Ranked by what they will cost after the merge. Settled decisions are treated as constraints, not findings:
DECISIONS, CR-004, CR-005, the DF items, and the overlay-only rule.

**N1. One capture driver per card, each wired into the session by hand.**
- **Evidence.**
  - `Session/` has 9 capture drivers. The slice adds 5: `AirCapture`, `CollarCapture`, `ColourAudioCapture`,
    `DayCapture` and `PresetCapture` (command 54).
  - Each is its own `UObject, FTickableGameObject` (for example `AirCapture.h:13`), with its own `FindPawn`, `RepoPath`
    and `Fail`: 26 such definitions in the 9 files (command 55). `RepoPath` at `AirCapture.cpp:108` and
    `DayCapture.cpp:83` is identical apart from the class name.
  - `UArenaSessionSubsystem` holds one forward declaration (`ArenaSession.h:35-43`) and one `TObjectPtr`
    (`:571-593`) per driver.
  - `OnWorldBeginPlay` has one command-line flag and one `if` block per driver (`ArenaSession.cpp:2076-2150`, 9
    `NewObject` calls).
- **Cost.**
  - Lane B's T24, T25, T27 and T28 will each add one more driver, and each edits two files that T16 holds.
  - The clang error that `740f5e8` fixed in five drivers is back in the five new ones (lane A's list), because every
    copy carries its own `FindPawn`.
- **Refactor.** A `UCaptureDriver` base that owns `FindPawn`, `RepoPath`, `Fail` and `Start`, plus a table from flag to
  class and one `TObjectPtr<UCaptureDriver>` in the subsystem. The drivers keep their bodies. Size M.
- **Gate.** It does not compete with 10-31 if it is done once, after the merge and T16 and before T24, because each
  later lane B card then does less work.

**N2. `ASessionPresentation::Sync` takes every card's visuals.**
- **Evidence.**
  - `SessionPresentation.cpp:1283-1780`, 498 lines. At the merge base it was 345 (`:1062-1406`).
  - The file grew by 932 lines and lost 70, in 7 slice commits. `SyncOrbPaths` (`:1969-2105`, 137 lines) and
    `SyncAirVisuals` (`:2107-2268`, 162 lines) are new.
  - It is hotspot #3.
- **Cost.** The art pass before the 10-31 gate edits this function, and so will T25's and T28's visuals. T16 holds the
  file.
- **Refactor.** Split `Sync` by what it drives, calling the parts in today's order. It already has six blocks: actors
  (`:1303`), projectiles (`:1412`), shots (`:1558`), telegraphs (`:1572`), player (`:1704`) and events (`:1746`). This
  is a move-only split, like sweep 1's split of `Load`. Size M.
- **Gate.** It competes a little. The best time is after T16 and before the art pass touches the file.

**N3. The reference script lives inside `FArenaSession` and grows with every threat.**
- **Evidence.**
  - `DecideScript` (`ArenaSession.cpp:1339-1615`) is 277 lines; it was 212 at the merge base.
  - `TrySunfallEscape` (`:1195-1322`) is 128 lines; it was 82.
  - `ScanThreats` (`:871-980`) is 110 lines.
  - `DecideScript` runs only for a scripted session (`:2018-2021`). It is the player that the census and `FullSeated`
    measure with.
- **Cost.**
  - T18 (ember ward), T19 (Sunfall) and T23 (Air) each added to it, and lane C's interrupts will too.
  - A change to the script moves the numbers the owner is deciding DF-006 to DF-008 on.
  - It is the part of F2 that grows fastest.
- **Refactor.** Move the three functions and their constants into an `FScriptedPlayer` (`Session/ScriptedPlayer.cpp`).
  It reads the session through a const view and acts through `PlayAction`. This is the first concrete step of F2's
  split. Size M.
- **Gate.** It does not compete if done after the merge. It waits for T16 (`ArenaSession.cpp`).

**N4. `VrRulesLoad.cpp` is now what every overlay card edits.**
- **Evidence.**
  - It went from 706 to 1102 lines. Five of the slice's eight code cards edit it (`3e3b3bc`, `e69d600`, `a0e24a0`,
    `397e6f0`, `4bebb2d`).
  - `LoadVrRuleset` (`:784-1102`) is 319 lines; it was 267.
  - The new `ReadRivals` (`:388-515`) is 128 lines.
  - It is hotspot #5.
- **Refactor.**
  - First, move the rival and Tiro-final readers (`ReadRivals`, `ReadTiroFinal`, `:388-589`) to
    `Kernel/VrRivalsLoad.cpp`, next to `VrRivals.cpp`.
  - Then do section 2.2's commit D: one reader per JSON section.
  - Size S to M.
- **Gate.** It does not compete. `VrRulesLoad.cpp` is not a T16 file, so this can run right after the merge.

**N5. A fourth copy of the JSON readers with an `FString` error sink.**
- **Evidence.**
  - The anonymous `bool Fail(FString&, const FString&)` now exists, with the same signature, in four files:
    `AbsorbResolver.cpp:11`, `ArenaLayout.cpp:11`, `VrRulesLoad.cpp:35` and the slice's `PlayerPreset.cpp:13`.
    `PlayerPreset.cpp` also has its own `NeedString` (`:19`), `NeedNumber` (`:43`) and `NeedBranch`.
  - The merged tree has 12 hand-written `FJsonObject::Values` loops in 7 files.
- **Cost.**
  - If two of the four identical anonymous functions land in one unity blob, the build fails with a redefinition
    error. Which files share a blob depends on file count and size, and the slice adds 36 files. This is not proven:
    no unity build log was read, and sweeps 1 and 2 saw these files compiled alone.
  - Every new data file brings another loop to fix by hand.
- **Refactor.** One module-internal `Json/JsonRead.h` with the `FString`-sink readers and a `ForEachField` that builds
  the `FString` key once. Move the four copies onto it; `KernelData.cpp` keeps its own (section 3: its sink differs).
  Size M.
- **Gate.** It competes only if done instead of lane A's by-hand fixes. Do lane A as written, and this after 10-31.

### 6.6 What this run did not do

- **No build and no test run.** N5's unity risk and lane A's diagnostics are pattern matches, not compiles.
- **The T16 edits were not read.** Q4 cannot say whether they conflict with the slice.
- **The roadmap's `MageArena.` 234/234 on the merged tree is quoted, not checked.**
- **The plan in 6.1 has not been compiled.** The first unknown is whether `Kernel/KernelDataLoad.h` needs more than
  `Dom/JsonObject.h` and `KernelData.h` to declare the readers. The sweep that runs it finds out with the first build.

### 6.7 Commands run for this section, with exit codes

Run in this worktree on 2026-10-08, from the repo root unless the path says otherwise.
- `S` is `apps/vr/Game/Source/MageArenaVR`, `K` is `$S/Kernel/KernelData.cpp`, and `T` is `04ec19d`.
- For a pipeline, the exit code is the last stage's. For a `;` list, it is the last command's. `grep -c` exits 1 when
  it counts 0; here that 0 was the expected result.
- The moved and kept name sets are these:

```
STAY='PinnedPath|Fail|LoadText|LoadJson|NeedNumber|NeedInt|NeedBool|NeedString|NeedObject|NeedVec|MatchGroup|MatchDouble|LoadCsv|Cell|LinearStat|LoadAirSpells|LoadGamesTuning|CheckReferences|LoadAbsorb|LoadCombat|CheckNerveAuthority|LoadStats|LoadScaleContract|LoadTrainingTuning|LoadWaterPresets|LoadRuntime|Load|KernelData|KernelDataPinnedPath|LoadKernelDataWithOverrides|WaterLines'
A_IN='AirFail|StrictNumber|CellNumber|MatchCell|CountWord|ParseAirShape|ParseAirDamage|ParseAirMomentum|ParseAirNotes|PublishAirCatalog|ParseAirSpellsCsv|KernelDataAirSpellsPath|FindAirSpell'
B_IN='SameNumber|RequireReach|PublishFireCatalog|ParseFireSpell|LoadNumberField|LoadHeatGain|LoadHeatDecay|ParseHeatThreshold|LoadHeatThresholds|ParseMomentumThreshold|LoadMomentum|LoadHeat|LoadFireSpells|FindFireSpell'
C_IN='RequireSpellColumns|FindLashArc|SpellFromRow|ParseWaterProjectile|ParseWaterLash|ParseWaterMire|ParseWaterMend|ParseWaterMirror|ParseWaterLine|BuildSpells'
D_IN='OptionalNumber|ParseAttack|ParseEnemy|LoadEnemies|ParseWaveSpawn|ParseArenaWave|ReadIntList|ParseArenaTier|LoadArenaTiers'
LOCAL = A_IN without its three public names, plus AirHeader, B_IN without FindFireSpell, C_IN and D_IN (43 names)
awk ranges: A 'NR>=2072&&NR<=2503'   B '(NR>=909&&NR<=1576)||(NR>=2060&&NR<=2070)'   C 'NR>=354&&NR<=685'   D '(NR>=233&&NR<=352)||(NR>=687&&NR<=907)'
```

| # | Command | Exit | Result |
|---:|---|---:|---|
| 1 | `git rev-parse v2/slice master` | 0 | `e78871c2…`, `772eb423…` |
| 2 | `git merge-base master v2/slice` | 0 | `76dec983…` |
| 3 | `git rev-list --count 76dec98..v2/slice`; `76dec98..master`; `--merges --count 76dec98..v2/slice` | 0 | 24, 18, 8 |
| 4 | `git diff --stat 76dec98 v2/slice -- $S/Kernel \| tail -1`; the same for `$S/Session` | 0 | 22 files (+2843 −58); 29 files (+7959 −186) |
| 5 | `git show e78871c:$K \| wc -l`; `git show master:$K \| wc -l` | 0 | 2541; 1918 |
| 6 | `git log --oneline 76dec98..master -- $K $S/Kernel/KernelData.h \| wc -l` | 0 | 0 |
| 7 | `git log --oneline 76dec98..v2/slice -- $K $S/Kernel/KernelData.h` | 0 | `4bebb2d` only |
| 8 | `git rev-parse master:$K 76dec98:$K` | 0 | `9194d805…` twice |
| 9 | `git rev-parse $T:$K e78871c:$K` | 0 | `ad6b630e…` twice |
| 10 | `git merge-base --is-ancestor 39efdd3 v2/slice` | 0 | ancestor |
| 11 | `git merge-base --is-ancestor 73f73a2 v2/slice` | 1 | not an ancestor (the sweep branch's SHA) |
| 12 | `git archive e78871c apps/vr/Game/Source \| tar -x -C /tmp/v2s-e78871c` | 0 | 158 `.cpp`/`.h` files in the scratch tree |
| 13 | `git show e78871c:$K \| grep -E '^[A-Za-z].*\(' \| grep -v ';$' \| wc -l` | 0 | 77 |
| 14 | `git show e78871c:$K \| awk 'NR>=2072&&NR<=2503' \| grep -oE "\b($STAY)\(\|\b(GTextOverrides\|FCsvTable)\b" \| sort \| uniq -c` | 0 | `KernelData(` 1, `MatchGroup(` 5 |
| 15 | `git show e78871c:$K \| awk '!(NR>=2072&&NR<=2503){print NR": "$0}' \| grep -E "\b($A_IN)\(\|\bAirHeader\b"` | 0 | `:1582`, `:1594` |
| 16 | command 14 with range B | 0 | `Fail(` 43, `MatchDouble(` 19, `NeedObject(` 6, `NeedString(` 4, `PinnedPath(` 2, `KernelData(`, `LoadJson(`, `LoadText(`, `MatchGroup(`, `NeedNumber(` 1 each |
| 17 | command 15 with range B and `$B_IN` | 0 | `:2050` only |
| 18 | command 14 with range C | 0 | `MatchDouble(` 26, `Cell(` 19, `Fail(` 4, `FCsvTable` 3 |
| 19 | command 15 with range C and `$C_IN` | 0 | `:2045` only |
| 20 | command 14 with range D | 0 | `NeedNumber(` 17, `Fail(` 16, `NeedString(` 10, `NeedInt(` 5, `NeedObject(` 2, `LoadJson(` 1, `PinnedPath(` 1 |
| 21 | command 15 with range D and `$D_IN` | 0 | `:2050` only |
| 22 | `git show e78871c:$K \| awk 'NR>=2072&&NR<=2503' \| grep -cE "\b($B_IN\|$C_IN\|$D_IN)\("`, and the same for B, C and D against the other three | 1 | 0, 0, 0, 0 |
| 23 | `git grep -nwE "$LOCAL" e78871c -- apps/vr/Game/Source \| grep -v Kernel/KernelData.cpp \| wc -l` | 0 | 0 |
| 24 | `git grep -nwE 'KernelDataLoad\|KernelDataAirRows\|KernelDataSchools\|KernelDataWater\|KernelDataRoster' e78871c -- apps/vr/Game/Source \| wc -l` | 0 | 0 |
| 25 | `git grep -lE '#include "Kernel/KernelData.h"' e78871c -- $S \| wc -l` | 0 | 36 |
| 26 | `git show e78871c:$S/Kernel/KernelData.h \| grep -nE '^[A-Za-z].*\(.*\);'` | 0 | lines 440, 441, 445, 446, 448, 452, 453, 456 |
| 27 | `node code-v2.mjs e78871c 2026-09-08` (the script in 6.2, run from a scratch path) | 0 | the table in 6.2; 158 files, 61532 lines; 115 s |
| 28 | `git log e78871c --no-merges --follow --since=2026-09-08 --format=%h -- $S/Kernel/VrRulesLoad.cpp \| wc -l`; the same without `--follow` | 0 | 11; 6 |
| 29 | `git merge-tree --write-tree --name-only master v2/slice` (master `772eb42`) | 0 | `04ec19d5…`, no conflicts |
| 30 | the same at the start of the run (master `2f089be`) | 0 | `1e95c295…`, no conflicts |
| 31 | `comm -12` of `git diff --name-only 76dec98 master` and `76dec98 v2/slice`, both sorted | 0 | `CreaturesCapture.cpp`, `SessionFlow.cpp` (38 and 139 files) |
| 32 | `git log --format=%h 76dec98..master -- <file>` and `git log --no-merges --format=%h 76dec98..v2/slice -- <file>`, for the two files | 0 | `740f5e8` \| `a0e24a0`; `2a76013` \| `c467976 397e6f0 f37f660` |
| 33 | `git grep -n 'for (const auto& Pair : Root->Values)' $T -- $S/Session/SessionFlow.cpp`; `git grep -n 'return It ? \*It : nullptr;' $T -- $S/Session/CreaturesCapture.cpp` | 0 | `:147`; `:129` |
| 34 | `git diff --numstat 76dec98 e78871c -- <the five T16 paths>` | 0 | four files, as in 6.4; `ArenaLayout.cpp` absent |
| 35 | `git show <rev>:$S/Kernel/<f>.cpp \| grep -c 'if (Rules'` and `grep -c 'Rules'`, for master and `e78871c` | 0 | ArenaKernel 8/54 to 11/65; ArenaThreats 3/14 to 8/30 |
| 36 | `git show e78871c:$S/Kernel/ArenaKernel.cpp \| grep -n 'Rules->Power\.\(BoltDamage\|LineDamage\)\|Rules->bActive && Actor.Team == 0'` | 0 | 171, 173, 286, 288, 290, 292, 304, 305, 481 |
| 37 | `git log --no-merges --format=%h 76dec98..e78871c -- $S/Kernel/ArenaKernel.cpp $S/Kernel/ArenaThreats.cpp` | 0 | `c467976 4bebb2d f37f660 e69d600 3e3b3bc` |
| 38 | `git show e78871c:$S/Session/ArenaSession.h \| grep -nE '^class FArenaSession\|^};'` | 0 | `108`, closing `529` |
| 39 | `sed -n '108,529p'` of that header, and `sed -n '87,369p'` of master's, each `\| grep -cE '\(.*\)( const)?( override)?;'` | 0 | 136; 97 |
| 40 | `git show e78871c:$S/Session/<f> \| grep -cE '^[^ /].*FArenaSession::'` and `wc -l`, for `ArenaSession.cpp`, `SessionFlow.cpp`, `DayFlow.cpp` | 0 | 60/2290, 58/1633, 25/614 |
| 41 | `git grep -n 'LoadVrRuleset(Rules' e78871c -- $S/Session $S/Hands` | 0 | `ArenaSession.cpp:255`, `SessionFlow.cpp:310`, `MageSettings.cpp:366` |
| 42 | `git grep -n 'public IHandSource' e78871c -- $S`; `git show e78871c:$S/MageArenaVR.Build.cs \| grep -c OculusXR` | 1 | `HandClipPlayer.h:16`; 0 |
| 43 | `git show e78871c:$S/MageArenaVR.Build.cs \| grep -n bBuildEditor` | 0 | `:31` |
| 44 | `git grep -l UHandInputSubsystem <rev> -- "$S/*.cpp" "$S/*.h" \| wc -l`, master and `e78871c` | 0 | 31; 39 |
| 45 | `git diff --quiet 76dec98 e78871c -- $S/Hands/HandInputSubsystem.cpp $S/Hands/HandInputSubsystem.h` | 0 | unchanged |
| 46 | `git grep -c 'const TPair<FString, TSharedPtr<FJsonValue>>&' master -- apps/vr/Game/Source` | 1 | 0 |
| 47 | the same at `e78871c` and at `$T`, summed | 0 | 15; 12 |
| 48 | `git show e78871c:$S/Kernel/VrRulesLoad.cpp \| grep -nE '^namespace$\|^}$' \| sed -n '1p;27p'` | 0 | `22`, `768` |
| 49 | `node linkage.mjs . <every non-test .cpp>` in `/tmp/v2s-e78871c/$S` (script below) | 0 | no output |
| 50 | the same over `Tests/*.cpp` | 0 | `DefenceTests.cpp:1341`, `:1374`, `:1381`, `:1616`, `:1678` |
| 51 | the same at `393a65f` (scratch archive), `Kernel/VrRules.cpp` | 0 | `:536`, `:546`, `:555`, `:570`, `:590` (F5's five) |
| 52 | `git show master:$S/Tests/DefenceTests.cpp \| grep -nE '^[A-Za-z].*\b(OpenFireWall\|RaiseInput\|StepCaster\|WatchWallRaise\|ArmWallMage)\('` | 0 | 967, 1000, 1007, 1242, 1304 |
| 53 | `git grep -nE 'NewObject<U[A-Za-z0-9]+CaptureDriver>' e78871c -- $S/Session/ArenaSession.cpp \| wc -l`; `TObjectPtr<U…CaptureDriver>` in `ArenaSession.h` | 0 | 9; 9 |
| 54 | `git diff --name-status 76dec98 e78871c -- $S/Session \| grep -E '^A.*Capture\.cpp' \| wc -l` | 0 | 5 |
| 55 | `git grep -nE '::(FindPawn\|RepoPath\|Fail)\(' e78871c -- "$S/Session/*Capture.cpp" \| wc -l` | 0 | 26 |
| 56 | `git show <rev>:$S/Session/SessionPresentation.cpp \| awk '/^void ASessionPresentation::Sync\(/{s=NR} s&&/^}/{print s"-"NR; exit}'`, `76dec98` and `e78871c`; the same for `SyncOrbPaths` and `SyncAirVisuals` | 0 | 1062-1406; 1283-1780; 1969-2105, 2107-2268 |
| 57 | the same for `FArenaSession::DecideScript`, `TrySunfallEscape` and `ScanThreats` in `ArenaSession.cpp` | 0 | 1153-1364 to 1339-1615; 1070-1151 to 1195-1322; 871-980 |
| 58 | the same for `LoadVrRuleset` (both revs) and `ReadRivals` in `VrRulesLoad.cpp` | 0 | 440-706 to 784-1102; 388-515 |
| 59 | `git log --no-merges --format=%h 76dec98..e78871c -- $S/Kernel/VrRulesLoad.cpp` | 0 | `4bebb2d 397e6f0 a0e24a0 e69d600 3e3b3bc` |
| 60 | `git grep -nE '^bool (Fail\|Need[A-Za-z]+)\(' e78871c -- <PlayerPreset, VrRulesLoad, ArenaLayout, AbsorbResolver> \| grep 'bool Fail('` | 0 | four `bool Fail(FString&, const FString&)` |
| 61 | `git show e78871c:$S/Session/SessionPresentation.cpp \| awk 'NR>=1283 && NR<=1780 && /^\t(for\|if) \(/ {print NR}'` | 0 | 1285, 1303, 1412, 1558, 1572, 1704, 1746, 1776 |
| 62 | `git rev-parse --short v2/slice`, at the end | 0 | `e78871c` |
| 63 | `node apps/vr/tools/check-pin.mjs` | 0 | `pin OK: 9 files @ baeac66` |

`linkage.mjs` lists free functions defined at file scope (outside any anonymous namespace, not `static`, no `::`) that
no header in the module declares. It reads files only.

```js
// Usage: node linkage.mjs <module dir> <file.cpp>...
import { readFileSync, readdirSync, statSync } from 'node:fs';
import { join } from 'node:path';
const root = process.argv[2];
const headers = [];
const walk = (d) => { for (const n of readdirSync(d)) { const p = join(d, n); if (statSync(p).isDirectory()) walk(p); else if (p.endsWith('.h')) headers.push(readFileSync(p, 'utf8')); } };
walk(root);
for (const rel of process.argv.slice(3)) {
  const lines = readFileSync(join(root, rel), 'utf8').split(/\r?\n/);
  let depth = 0; const anon = []; let pendingAnon = false;
  lines.forEach((l, i) => {
    if (/^namespace\s*\{?$/.test(l.trim())) pendingAnon = true;
    if (depth === 0 && anon.length === 0 && /^[A-Za-z].*\(/.test(l) && !/;\s*$/.test(l) && !/::/.test(l.split('(')[0])
      && !/^(static|namespace|class|struct|enum|template|#|IMPLEMENT_|DEFINE_|UE_|BEGIN_|END_)/.test(l)) {
      const name = (l.split('(')[0].match(/([A-Za-z_][A-Za-z0-9_]*)\s*$/) || [])[1];
      if (name && !headers.some((h) => new RegExp('\\b' + name + '\\s*\\(').test(h))) console.log(`${rel}:${i + 1}: ${name}`);
    }
    for (const c of l) {
      if (c === '{') { depth++; if (pendingAnon) { anon.push(depth); pendingAnon = false; } }
      if (c === '}') { if (anon.length && anon[anon.length - 1] === depth) anon.pop(); depth--; }
    }
  });
}
```
