# T44 - Fail loudly on an unknown threat family

Status: open (needs Wolf: UE 5.8.3 editor and the `MageArena.` / `MageArenaDesign.` automation suites; written
2026-10-21 on Lynx, where neither runs - `docs/DECISIONS.md` 2026-10-20 "Unreal host")
Max turns: 320

## Goal
A combat-data cell the loaders do not understand stops the load with a named error instead of silently becoming the
most forgiving thing in the game. Today every unrecognised `family` or `blockable` value lands on `Magic`, which is
both absorbable and perfect-eligible, so a typo in a threat row turns an unblockable attack into free Flow - the
threat colour language (element colour = absorb, steel = dodge, black core with a red rim = leave) would lie to the
player and no test would notice. Air is the only school that already hard-fails on an unrecognised cell; this card
lifts Air's pattern into the shared loader header and applies it to Heat, Water, the roster and the absorb rules, and
adds the range checks that are missing. **It changes no number the sim computes on today's pinned data** - it only
turns silent defaults into loud load failures.

## THE HARDEST CONSTRAINT - read this first
**The 45 conformance vectors must stay byte-identical, and `node apps/vr/tools/check-pin.mjs` must still print exactly:**
```
pin OK: 9 files @ baeac66
conformance OK: 45 vectors
```
The pin is VR's **frozen baseline** (`docs/DECISIONS.md` 2026-10-21, "Combat data: the current pin is VR's frozen
baseline"). `apps/vr/data/pinned/**`, `apps/vr/data/PINNED.json` and `apps/vr/data/conformance/**` are **never edited**
(`.ai/manifest.yaml` `boundaries.neverTouch`) and nobody re-pins from TV. If any new strictness would reject a cell
that exists in today's pinned CSV or JSON, that is not a reason to edit the data and it is not a reason to loosen the
check into a default: **stop, report the exact cell and file, and let the Director take it to the owner.** A VR-side
value change, if one is ever wanted, goes in the VR overlay `apps/vr/data/vr/` through a `docs/change-requests/CR-*`
design record - not in this card.

## Read first
- `CLAUDE.md`, `docs/DECISIONS.md` (newest first; 2026-10-21 frozen baseline, 2026-10-20 VR first)
- `apps/vr/data/README.md` - what is pinned and how `check-pin.mjs` verifies it
- `docs/research/MOONSHOT-2026-10.md` items **M5** ("Compile pinned enemy prose into typed archetypes at load and gate
  pin bumps", accepted, XL) and **M6** ("Inject kernel data into the sim and add a pin-diff census", accepted = card
  **T33**). This card is the **small, shippable front half** of M5's load-time validation and must not grow into M5:
  see "Scope wall".
- `docs/concepts/WOLF-PLAN.md` - P11 holds the determinism package (T33-T36) as "only if time is left". This card is
  deliberately independent of T33/T36 and can land before them, but it touches `KernelData*.cpp`, which T33 also
  touches: land this **before** T33 or rebase it onto T33, never in parallel.
- Code, by symbol (line numbers verified on Lynx 2026-10-21 - trust the symbol, the lines will move):
  - `Kernel/ArenaThreats.cpp` - `KindFromFamily` (`:20`, body to `:31`). The whole defect in six lines:
    `"physical"` -> `Physical`, `"unblockable"` -> `Unblockable`, **everything else -> `Magic`**.
  - `Kernel/KernelData.cpp` - the `notAgainst` enforcement (`:344-356`, inside the absorb load): it requires
    `notAgainst` to be exactly `{physical, unblockable}` with no `magic`, citing `kernel.ts:57`. This is the **only**
    place that enforcement exists, and the constant is `AbsorbNotAgainstCount`.
  - `Kernel/KernelDataAir.cpp` - **the pattern to copy.** `StrictNumber` (`:39`) and `CellNumber` (`:50`) refuse a
    non-numeric cell with a named error; `ParseAirSpellsCsv` (`:335`) uses `CellNumber` for tier, cast_s, cooldown_s,
    mana, telegraph_s and range_m (`:383-388`) and the `blockable` chain at `:402-416` ends in
    `return AirFail(Error, ... "air row %s: blockable cell not understood: %s" ...)`. Note Air admits a **fourth**
    family value, `"n/a"` (`:410-412`), for self-shaped spells, checked against the shape at `:423`.
  - `Kernel/KernelDataWater.cpp` - `SpellFromRow`: `Spell.Family = *Cell(Row, TEXT("blockable")) ==
    TEXT("UNBLOCKABLE") ? TEXT("unblockable") : TEXT("magic")` (`:60`) - a silent default. The numeric cells around it
    use `FCString::Atoi` (`:49`) and `FCString::Atod` (`:56`, `:57`, `:58`, `:59`, `:61`, `:62`), which **return 0 on
    garbage** rather than refusing.
  - `Kernel/KernelDataHeat.cpp` - the fire-spell row: `Spell.Blockable = Values[8]` (`:660`) then
    `Spell.Family = Spell.Blockable == TEXT("UNBLOCKABLE") ? TEXT("unblockable") : Spell.Blockable ==
    TEXT("absorbable") ? TEXT("magic") : Spell.Blockable` (`:661`) - **the raw cell passes through as the family** for
    any other value, so a trailing space yields a family that is neither `"unblockable"` nor `"magic"`, every
    `== "unblockable"` check misses it, and `KindFromFamily` hands back `Magic`. Numeric cells use `Atoi` (`:653`) and
    `Atod` (`:655-657`, `:662-663`).
  - `Kernel/KernelDataRoster.cpp` - `ParseAttack` reads `family` with `NeedString` and **no enum check** (`:28`); the
    `onDeath` `FDeathSpec` does the same (`:118`).
  - `Combat/AbsorbResolver.cpp` - `FAbsorbRules::LoadFromPinnedFile` (`:171`, body to `:274`): it checks
    `ArcDeg > 0.0` and `MinReleaseS >= 0.0` (`:201`) and `WindowS > 0.0` (`:240`), parses `perfect.notAgainst` entries
    as non-empty strings (`:260-270`) and **never checks their content**. `FAbsorbResolver::Init` (`:313-325`) calls it
    directly, so the desktop resolver reads the pinned file **independently of `KernelData`'s loader** - bypassing the
    `notAgainst` enforcement at `KernelData.cpp:344-356`, any test override, and any future VR overlay. There is also
    no `[0, 1]` range check on the reduction fractions.
  - `Kernel/KernelDataLoad.h` - the shared loader header, where the lifted helpers belong.
  - `Kernel/Air.cpp:41` and `Kernel/Fire.cpp:40` - both already map a `"n/a"` family to `"magic"` at the point of use.
    Read them before choosing how `ParseFamily` treats `"n/a"`; they are the existing contract.

## Two sibling findings that belong to this same seam
The Wolf session should consider these **together with the above**, not as separate work:
- **One `FamilyKind` enum with one `ParseFamily`.** The string `"magic"` / `"unblockable"` / `"physical"` / `"n/a"`
  currently exists as free-form text in five loaders plus `KindFromFamily`'s comparison. One parse function, one enum,
  one error message is the shape that makes the hard-fail enforceable rather than five copies of a check.
- **`KindFromFamily`'s permissiveness and the loaders' permissiveness are one chain, not two bugs.** Hardening only
  the loaders leaves `KindFromFamily` as a silent fallback for any family that reaches it from elsewhere (the VR
  overlay, a test fixture, a future school); hardening only `KindFromFamily` moves the failure from load time to
  mid-combat. Both ends, or the hole stays open.

## Scope wall
- This card does **not** build M5 (typed archetypes compiled from the enemy prose) and does not add a pin-bump gate.
  It adds *validation at load* and one shared parse. If the work starts turning into an archetype compiler, stop and
  report.
- It does **not** change the sim, the kernel maths, the resolver's behaviour on valid data, or any tuned constant.
- It does **not** add a new data file, a new overlay field, or a new CR.

## Deliverables (exact paths)
1. `runs/T44/family-sites.md` (uncommitted): every site that produces, parses or compares a threat family, with
   file:line, the symbol, the accepted values there, and what currently happens on an unknown value. Re-verify every
   premise listed above; a premise that no longer holds is reported, not built on.
2. **One family type and one parser.** A `FamilyKind` enum (or the existing `EAbsorbHitKind` extended - choose and
   justify) and a single `ParseFamily(const FString& Cell, FamilyKind& Out, FString& Error)` in a shared header
   (`Kernel/KernelDataLoad.h` is the natural home). It accepts exactly the values the pinned data actually contains -
   enumerate them from the pinned files in step 1, including Air's `"n/a"` - and returns a named error for anything
   else. `KindFromFamily` is rewritten on top of it and **no longer has a silent fallback**: an unknown family is a
   load-time refusal upstream, and if one still reaches `KindFromFamily` it is a hard error (a `check`/ensure plus the
   safest kind, `Unblockable`, **not** `Magic` - a wrong refusal is survivable, a wrong free perfect is not).
3. **Air's strict helpers lifted and reused.** Move `StrictNumber` and `CellNumber` from `KernelDataAir.cpp` into
   `Kernel/KernelDataLoad.h` (keeping Air's behaviour byte-identical) and replace the bare `FCString::Atoi` /
   `FCString::Atod` cell reads in `KernelDataWater.cpp` (`SpellFromRow`) and `KernelDataHeat.cpp` (the fire-spell row)
   with them, so a corrupt cooldown, cost or cast-time cell refuses the load instead of yielding 0.
4. **Water, Heat and the roster parse the family strictly.** `KernelDataWater.cpp:60`, `KernelDataHeat.cpp:661`,
   `KernelDataRoster.cpp:28` and `:118` all go through `ParseFamily`. The Heat pass-through of the raw `Blockable`
   cell as the family is gone.
5. **The absorb rules validate their own contract.** `FAbsorbRules::LoadFromPinnedFile` enforces, with named errors:
   `perfect.notAgainst` is exactly `{physical, unblockable}` (the same rule as `KernelData.cpp:344-356` - lift it so
   there is **one** copy, and say in the report which site now owns it), every reduction fraction is in `[0, 1]`, and
   `RaiseCostMana`/`MoveSpeedWhileHeldMult` are non-negative. State in the report whether `FAbsorbResolver::Init`
   should keep reading the file independently or should take its rules from `KernelData`; if you change that, the
   conformance vectors are the proof it changed nothing.
6. **Tests, each seen RED before the change** (`Tests/KernelDataTests.cpp` and/or a new `Tests/FamilyParseTests.cpp` -
   follow the existing kernel-test file layout):
   - `MageArena.KernelData.UnknownFamilyFailsLoad`: a fixture row whose `blockable`/`family` cell is an unknown value
     (include the trailing-space case, `"absorbable "`, which is the realistic typo) fails the load with an error
     naming the file, the row id and the cell. One case per loader: Water, Heat, roster attack, roster `onDeath`.
   - `MageArena.KernelData.GarbageNumberFailsLoad`: a non-numeric `cooldown_s` in a Water row and in a Heat row fails
     the load instead of producing 0.
   - `MageArena.KernelData.AbsorbRulesRejectBadNotAgainst`: an absorb fixture whose `perfect.notAgainst` is empty, or
     contains `magic`, is refused by `FAbsorbRules::LoadFromPinnedFile` (today it is accepted). Add the matching
     out-of-range reduction case.
   - `MageArena.KernelData.PinnedDataStillLoads`: the real pinned files load clean under all the new strictness, with
     the same spell, enemy and rule counts as before (quote the counts).
   - `MageArena.Threats.UnknownFamilyIsNotAbsorbable`: whatever `KindFromFamily` does with an unknown family, the
     result is **not** perfect-eligible. This is the test that encodes "fail in the safe direction".
   Each red line is quoted verbatim in the report. A case that never failed proves nothing.

## Constraints
- Do not commit, push, or touch any repository other than this one.
- Do not call paid services. Do not edit `docs/DECISIONS.md`, `apps/vr/tasks/BACKLOG.md`, `apps/vr/data/pinned/**`,
  `apps/vr/data/PINNED.json`, `apps/vr/data/conformance/**` or `apps/vr/Game/Clips/**`.
- **No value the sim computes on today's pinned data may change.** The 45 conformance vectors are the proof. A vector
  that moves means the change was not purely additive strictness - revert and report, do not re-record the vector.
- New test fixtures are **new fixture files or in-memory strings**, never edits to the pinned data. If a fixture needs
  to live on disk, put it where the existing kernel tests put theirs and say where in the report.
- No new runtime dependency. The Node tooling stays zero-dependency `.mjs`.
- Error strings follow Air's house style: `"<school|file> row <id>: <what was wrong>: <the cell>"`. An error with no
  row id and no cell value is not acceptable - the point of this card is that the failure says what to fix.
- Fresh checkout or worktree: regenerate the gitignored sigil corpus first -
  `node apps/vr/tools/clipgen/generate.mjs --corpus 30 --seed 1000` then `node apps/vr/tools/clipgen/mousepaths.mjs`.
  Never run `generate.mjs` bare or with `--templates`; both rewrite tracked clips.
- One Unreal build at a time (`-WaitMutex`); one `Automation RunTests` filter per editor process; read verdicts from
  `-ReportExportPath`. `apps/vr/tools/build.ps1` has exited 0 when `Build.bat` was missing (found 2026-10-09): confirm
  the build really ran from its log line, not from the exit code.
- Baseline the full `MageArena.` and `MageArenaDesign.` suites **before** the change and compare per test after. Known
  red on a clean tree (`CLAUDE.md`, as of 9f338f0): `MageArenaDesign.Duel.FireSeated`, `Duel.AirSeated`,
  `Session.FullSeated`, `Session.Wave1Seated`, `Session.CreaturesSeated`. Any other change is a regression.

## Acceptance (the orchestrator re-runs these; quote their real output in your report)
Every UE command below is **Wolf-only**. Lynx has no Unreal install and cannot produce any of this output. The two
`node` gates **can** be run anywhere and are the ones that protect the baseline.
```
node apps/vr/tools/check-pin.mjs && node apps/vr/tools/check-pin.mjs --self-test
    # expected, verbatim: "pin OK: 9 files @ baeac66" then "conformance OK: 45 vectors", then the self-test OK.
    # Anything else is a FAIL and the card stops here.

git status --porcelain -- apps/vr/data
    # expected: EMPTY. No file under apps/vr/data/ is modified by this card.

powershell -NoProfile -File apps/vr/tools/build.ps1
    # expected: the build log contains "Result: Succeeded" (read the log, not the exit code)

UnrealEditor-Cmd MageArenaVR.uproject -ExecCmds="Automation RunTests MageArena.;Quit" -unattended -nullrhi -nosplash -log -ReportExportPath=runs/T44/all
    # expected: identical to the pre-change baseline, plus the new tests green:
    #   MageArena.KernelData.UnknownFamilyFailsLoad, .GarbageNumberFailsLoad,
    #   .AbsorbRulesRejectBadNotAgainst, .PinnedDataStillLoads, MageArena.Threats.UnknownFamilyIsNotAbsorbable

UnrealEditor-Cmd MageArenaVR.uproject -ExecCmds="Automation RunTests MageArenaDesign.;Quit" -unattended -nullrhi -nosplash -log -ReportExportPath=runs/T44/design
    # expected: exactly the 5 known reds, their numbers UNCHANGED. A moved number means the sim changed - FAIL.

git grep -n "TEXT(\"magic\")" -- apps/vr/Game/Source/MageArenaVR/Kernel
    # expected: only inside ParseFamily / the family->string helpers. A loader still defaulting to "magic" is a FAIL.

node apps/vr/tools/lint/format-check.mjs --self-test && node apps/vr/tools/lint/format-check.mjs --base origin/master
    # expected: OK
node apps/vr/tools/lint/unused-members.mjs && node apps/vr/tools/lint/unused-members.mjs --self-test
    # expected: OK
```
The **red evidence** is part of acceptance: the report quotes the real failing line of each new test run against
unchanged `master`. Without it the card has not proven the hole was open, and the orchestrator will not accept it.

## Report
Write `runs/T44/REPORT.md`: base sha and the Wolf host name and engine build; the family-site table; what you built
(paths); each acceptance command with its real output, with the `check-pin.mjs` lines quoted verbatim; the RED line of
each new test on unchanged master and its green line after; the per-test suite comparison before/after; which site now
owns the single `notAgainst` enforcement and whether `FAbsorbResolver::Init` still reads the pinned file
independently; how `ParseFamily` treats `"n/a"` and why; any pinned cell the new strictness would have rejected (with
the exact file, row and value - and the fact that you stopped rather than edited it); premises that turned out false;
files outside `Kernel/`, `Combat/` and `Tests/`; and anything you could not do, with the exact error.
