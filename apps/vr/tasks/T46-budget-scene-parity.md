# T46 - Budget scene parity: D-G4 must measure the shipping scene, and its cross-check must be able to fail

Status: open (needs the build machine: UE 5.8.3 editor and `capture-budget.ps1`; written 2026-10-21 on Lynx, the
ARM64 laptop, where no Unreal install and no renderer exist - `docs/DECISIONS.md` 2026-10-20 "Unreal host")
Max turns: 300

## Goal
The D-G4 draw-call row is measured on the scene the game actually ships, and its plausibility cross-check can fail.
Today neither holds. The row is **fail** at median 194, max 201 against a target of **one total of ≤150 draw calls for
the whole frame** (owner ruling `docs/DECISIONS.md` 2026-10-16, no separate particle line;
`docs/research/BUDGET-DG4-2026-10.md` 10.2-10.3), and the lever that closes it is carded as **T37**. But the capture
that will declare that row green spawns **no `ASessionPresentation`** at all: it is a hand-built parallel scene that
re-implements the presentation's parts, colours and HP-bar geometry from copied constants, and already disagrees with
it on one enemy colour. So T37 can be declared green on the proxy while the real session is still over budget, or the
reverse, and nobody would know from the artefacts. Alongside it, the capture's own `plausibilityOk` cross-check
compares a value with itself and **can never be false** - the code comment says so in as many words. When this card is
done, the ≤150 row is measured on the shipping presentation or a parity check fails the moment the proxy and the real
scene diverge; and `plausibilityOk` is either a genuine independent cross-check or honestly relabelled.

This card is about the **instrument**, not the lever. It does not re-propose any part of T37.

## Read first
- `CLAUDE.md`, `docs/DECISIONS.md` (they outrank this card if they conflict). **2026-10-16: draw calls are ONE total
  budget of ≤150 for the whole frame, with no separate particle line.**
- `docs/research/BUDGET-DG4-2026-10.md` sections **4** (why `GNumPrimitivesDrawnRHI` is incomplete - read it before
  choosing a new cross-check source), **7** and **7.7**, **8**, **9** (section 9 wins where it and 7.7 differ), and
  **10** (10.2-10.3 hold the current fail)
- `docs/research/MOBILE-INSTANCING-2026-10.md` sections 1.2-1.7 and 5, and section 9 choice 3 of BUDGET-DG4 - the three
  config lines that must never be set
- `apps/vr/tasks/T37-shared-part-materials.md` **in full**, especially deliverable 5 and its acceptance line "draw
  calls max <=150 in all three summaries". That acceptance reads the **proxy**; this card is what makes it mean the
  game. Do not duplicate any T37 deliverable.
- `apps/vr/tasks/BACKLOG.md:13` - the circularity of the triangle cross-check is **already recorded** there. This card
  **confirms and sizes** it; it does not claim to discover it.
- `docs/research/MOONSHOT-2026-10.md:26` - "Compile the Quest flavour on every gate: clang parity, **Shipping proof**,
  no dev code" is a **declined** moonshot item and a durable no. Do not propose a Shipping-build gate, in any form, as
  part of this card.
- `docs/concepts/WOLF-PLAN.md` section c **P3** (T37) and section d. This card lands **with P3**: either immediately
  before it, so T37's capture is trustworthy when it runs, or immediately after it, re-stating the row on the parity
  instrument. Say which you did and why.
- Code, by symbol (line numbers were verified on this branch 2026-10-21 and will rot - navigate by symbol):
  - `Greybox/ArenaGreyboxActor.cpp` - `UArenaGreyboxSubsystem::OnWorldBeginPlay`, the capture branch at :376-391
    (`-MageArenaGreyboxCapture`, then `-MageArenaBudgetStress` at :380, `BudgetStress->Start()` at :383), with the
    comment "the session stays idle"
  - `Session/ArenaSession.cpp:2096` - `InWorld.SpawnActor<ASessionPresentation>(...)` inside
    `UArenaSessionSubsystem::OnWorldBeginPlay` (:2060), the **only** place the presentation is spawned
  - `Budget/BudgetStressDriver.cpp` - the file-local `MakePart` (:130, default path `Greybox::Tint` at :140), the HP
    bar block (:265-276), `UBudgetStressDriver::Start` (`bSharedMaterials = FParse::Param(..., TEXT(
    "MageArenaBudgetSharedMaterials"))` at :193), the shared-cache selection at :245,
    `UBudgetStressDriver::CountVisibleLod0` (:585) and its sample call (:651)
  - `Budget/BudgetStressDriver.h:122` - `bool bSharedMaterials = false;` (the opt-in default)
  - `Budget/BudgetStressPlan.cpp:5-11` - the re-typed colours and the comment pointing at
    "`Session/SessionPresentation.cpp:34-41, :1094-1101`", and `MakeEnemy` (:19-47) with the re-typed body sizes
  - `Budget/BudgetStressPlan.h:13-15` - `EnemyCount = 20`, `ProjectileCount = 100`, `HandCount = 2`
  - `Greybox/GreyboxCapture.cpp` - `UGreyboxCaptureDriver::SampleBudget` (:444; the admission comment at :449-452 and
    `const int32 Measured = VisibleLod0;` at **:453**), `UGreyboxCaptureDriver::CountVisibleLod0` (:381),
    `UGreyboxCaptureDriver::WriteBudgetFile` (:894; the `source` string at :898-899, `bWithinBudget` at :927,
    `bPlausible` at **:930-944**), and `bColoursOk` (set at **:568**)
  - `Session/SessionPresentation.cpp` - `ASessionPresentation::MakePart` (:175), the enemy colours (:41-42 and the
    `BodyFor` block at :206-213), the body sizes (the `Sync` block at **:1358-1368**), `EnsureCuff` (:463),
    `EnsureTeachVisuals` (:621), `EnsureDome` (:839, **translucent** at :863), `SyncWalls` (:950, **translucent**
    curtains at :971), `EnsureComfortVisuals` (:1033), `EnsurePhaseVisuals` (:1196), `EnsureCollarVisuals` (:1820),
    `SyncTablet` (:1943), `SyncOrbPaths` (:2007), `SyncAirVisuals` (:2145)
  - `apps/vr/tools/capture-budget.ps1` - the default flags, and the marker gate at :178-186
    (`MAGEVR_BUDGET_DONE`, the two `MAGEVR_BUDGET_POP` lines, `MAGEVR_BUDGET_FAIL`)
  - `Tests/BudgetStressTests.cpp`

## The defects, as verified on this branch (2026-10-21, Lynx, by reading the tree)
Re-verify each before you change it; a different reading is reported, not assumed.

**A. The stress capture measures a parallel scene, not the shipping one.**

- **A1. No presentation actor exists in it.** `UArenaGreyboxSubsystem::OnWorldBeginPlay` starts
  `UBudgetStressDriver` under `-MageArenaGreyboxCapture -MageArenaBudgetStress`, with the comment "**the session stays
  idle**". `ASessionPresentation` is spawned only from `UArenaSessionSubsystem::OnWorldBeginPlay`
  (`Session/ArenaSession.cpp:2096`), and nothing under `Budget/` includes or references it. So the measured frame
  contains none of the presentation's own parts.
- **A2. The driver re-implements the presentation from copied constants.** It has its **own** file-local `MakePart`,
  its **own** HP-bar geometry (70 x 8 x 8 cm, 18 cm above the body) with the colours copied verbatim into
  `HpBackColour` / `HpColour` (`BudgetStressDriver.cpp:55-56`), and `BudgetStressPlan.cpp` **re-types** the enemy
  colours and body sizes under a comment that points at `SessionPresentation.cpp:34-41, :1094-1101` - **line numbers,
  which rot.**
- **A3. They have already drifted, and the comment's coordinates are already wrong.** Verified here:
  - `BudgetStressPlan.cpp`'s `HoundColour(0.8f, 0.3f, 0.05f)` versus the presentation's `HoundBody(0.28f, 0.07f,
    0.02f)` (`SessionPresentation.cpp:54`). **Different colour, same enemy.** With a per-colour material cache (T37)
    the number of distinct materials is a function of the distinct colour set, so a wrong colour is not cosmetic in a
    draw-call measurement - it is a wrong bucket count.
  - `BudgetStressPlan.cpp:5`'s coordinates are stale on both halves: the colours are at `SessionPresentation.cpp:41-42`
    and :206-213, and the body sizes at :1358-1368. Nothing at :34-41 or :1094-1101 is either.
  - Conscript, slinger and mire-maw colours and all four body sizes still agree - which is exactly why the one that
    drifted went unnoticed.
- **A4. The stress scene is missing most of what the shipping frame draws.** The presentation builds the cuff
  (`EnsureCuff`), the teach visuals (`EnsureTeachVisuals`), the dome (`EnsureDome`), the walls and curtains
  (`SyncWalls`), the comfort visuals (`EnsureComfortVisuals`), the phase and rival flare (`EnsurePhaseVisuals`), the
  collar visuals (`EnsureCollarVisuals`), the tablet (`SyncTablet`), the orb paths (`SyncOrbPaths`) and the air
  visuals (`SyncAirVisuals`). The stress scene has none of them. Notably `SessionPresentation.cpp:863` and `:971` use
  `Greybox::TranslucentMaterial`, so the shipping frame has a translucency pass fed by presentation parts and the
  stress frame has **none**: a pass that cannot instance the way opaque parts do is absent from the measurement that
  decides the opaque lever. The stress populations are fixed constants (`BudgetStressPlan.h:13-15`: 20 enemies, 100
  projectiles, 2 hands) rather than derived from what a live presentation holds.
- **A5. Sharing is opt-in and defaults off, so the default capture measures the unshared scene.** `MakePart`'s default
  path is `Greybox::Tint` - one MID per part - and the cache is used only when `bSharedMaterials` is true, which is
  `false` by default (`BudgetStressDriver.h:122`) and set only by `-MageArenaBudgetSharedMaterials`
  (`BudgetStressDriver.cpp:193`). `capture-budget.ps1` does not pass that flag. So after T37 ships lever B into the
  presentation, a default `capture-budget.ps1` run still measures the **unshared** proxy. T37 deliverable 5 points the
  driver at the same `SharedTint`, which closes the material-sharing divergence **and only that one**: nothing in T37
  stops the part list, the populations, the sizes, the colours or the HP geometry drifting again.

**B. The plausibility cross-check can never fail.** In `UGreyboxCaptureDriver::SampleBudget`,
`const int32 Measured = VisibleLod0;` - the same value - and both are appended to separate arrays
(`MeasuredTriangles`, `VisibleLod0Triangles`). `WriteBudgetFile` then tests
`MeasuredTriangles[Index] < VisibleLod0Triangles[Index]`, which compares identical values and **is never true**. The
only live condition left is `MaxVisible > 0`, that is: "something was inside the frustum at least once". The code
comment at :449-452 says this outright ("The plausibility floor is the same sum"), and the emitted JSON's `source`
field (:898-899) tells the reader the capture "fails when measuredTriangles is below the floor" - a condition that
cannot occur. The counter is **duplicated** in `UBudgetStressDriver::CountVisibleLod0`, so fixing one leaves the other
with the same reading of the scene and no shared test.

**C. What is real and must not be disturbed.** `bColoursOk` (`GreyboxCapture.cpp:568`,
`bAll && ColourRows.Num() >= 5`) **is** a genuine check - DeltaE2000 over sampled screen pixels against the threat
palette. Leave it exactly as it is. The budget JSON's honest self-labelling ("desktop proxy - not Quest truth", and
section 4's account of why `GNumPrimitivesDrawnRHI` is incomplete) is also right and must survive this card: the point
is to make the **scene** the shipping one, not to claim a desktop number is a device number.

## Deliverables (exact paths)
1. **`runs/T46/parity-census.md`** (uncommitted), written **first**, before any code: the shipping presentation's
   drawn parts versus the stress scene's, as a table - part, mesh/shape, size, colour, count, material, pass (opaque
   or translucent) - with the file:line of each side **and the symbol beside it**. Re-verify every reading in A2-A5
   above and record each one as confirmed or corrected. This table is the specification for deliverable 3's tolerance
   and the evidence for the card; if it shows the divergence is smaller or larger than A4 says, that is the finding.
2. **The capture measures the shipping scene, if that is reachable.** Preferred: the D-G4 stress capture runs with a
   live `ASessionPresentation` in the world and draws its parts through it, so the measured frame is the session's
   frame plus the stress population. Say exactly how you did it (a scripted session under the capture flag; the
   presentation spawned and synced by the stress driver; or another route) and what it cost. If this is not reachable
   inside this card - for instance because the stress population cannot be expressed as kernel actors - **stop after
   deliverable 1 and report why**, with the specific obstacle, and deliver 3 instead. Do not half-build it.
3. **A scene-parity check that fails on divergence** (required either way - it is the guard that keeps deliverable 2
   honest, and the fallback if deliverable 2 proves unreachable). Both the live presentation and the stress scene emit
   a comparable census of their drawn parts; the cheap version is a hash or a count of **distinct (mesh, colour,
   material, pass) tuples** plus the per-tuple part count, from each side. The check fails the capture with
   `MAGEVR_BUDGET_FAIL scene parity <what diverged>` when the two differ beyond a tolerance **stated in the card's
   report and justified from deliverable 1's table** - not a tolerance picked to make today's tree pass. Requirements:
   - It must fail **today**, before any fix, on the `HoundColour` / `HoundBody` divergence of A3. Quote that red run.
   - The constants in `Budget/BudgetStressPlan.cpp` stop being an independent copy: the stress plan takes its colours
     and body sizes **from the presentation's own source** (a shared header or accessor), or the parity check covers
     them so a future copy cannot drift silently. Prefer single-sourcing; if you single-source them, say what the
     shared home is and why both sides can reach it.
   - Replace every `file:line` reference in `Budget/` comments with a **symbol** reference. `BudgetStressPlan.cpp:5`
     is wrong on both coordinates today; a comment that rots is how A3 happened.
4. **`plausibilityOk` becomes real or becomes honest.** Pick one and justify it from a Wolf measurement, not from
   reasoning:
   - **Real:** give it a genuinely independent source. Candidates, in the order the research suggests:
     `GNumPrimitivesDrawnRHI` (already recorded as `rhiPrimitivesDrawn`, but BUDGET-DG4 section 4 and the JSON's own
     `source` field at :898-899 say it misses GPU-scene indirect draws, so any margin must be justified by measurement
     on Wolf); `stat RHI` or `stat SceneRendering` counters; a CSV-profiler channel. Whatever you choose, the check
     must be able to fail: show it failing on a deliberately wrong frustum or a deliberately wrong counter, and quote
     that run.
   - **Honest:** rename the field to what it actually tests (for example `trianglesSampled`, "at least one frame had
     in-frustum geometry"), fix the misleading `source` string at :898-899, and remove the dead comparison. State in
     the report that D-G4's triangle row then has **no** independent cross-check on the desktop and that the real
     number comes from the device at V1 - which is what `BACKLOG.md:13` already says.
   Either way, the duplicated counter is **single-sourced**: one implementation of the visible-LOD0 sum used by both
   `UGreyboxCaptureDriver` and `UBudgetStressDriver`, with one test over it.
5. **T37's default is explicitly verified.** After T37 lands, a **default** `capture-budget.ps1` run - no extra flags -
   must measure the shared-material path, because that is what ships. Prove it from the capture's own log
   (`MAGEVR_BUDGET_MATERIALS mids=<n> colours=<n> parts=<n>`, `BudgetStressDriver.cpp:843`) and from the emitted
   summary, and quote both. If the default is still the unshared path, that is a **finding to report**, and the fix is
   the default flip in this card (it is this card's instrument, not T37's lever).
6. **Tests** (in `Tests/BudgetStressTests.cpp`, or a new `Tests/SceneParityTests.cpp` if the existing file does not
   fit - say which and why). All must run under `-nullrhi`, so the census comparison is a pure function over two part
   lists:
   - `MageArena.Budget.Parity.DivergentColourFails` - two censuses differing only in one enemy colour fail the check.
     **Seen RED first** against today's constants, i.e. the real `HoundColour` / `HoundBody` pair; quote the red line.
   - `MageArena.Budget.Parity.IdenticalScenesPass` - two identical censuses pass.
   - `MageArena.Budget.Parity.MissingPassFails` - a census with no translucent pass fails against one that has one
     (A4's concrete gap).
   - `MageArena.Budget.Triangles.SingleSourced` - the shared visible-LOD0 counter returns the same value for the same
     input through both call sites.
   - If deliverable 4 took the "real" option: a test that the new cross-check fails on a wrong counter.
7. **`docs/research/BUDGET-DG4-2026-10.md` gets a new numbered section** in the house style of section 8 (provenance,
   results per run, verdicts) recording what the instrument now measures and re-stating the ≤150 row on the parity
   instrument. **Sections 0-10 are not edited**, and if T37 has already appended its section 11, this one is section
   12. If the row's verdict changes because the instrument changed, that is the headline of the section and it must be
   stated plainly in both directions (a number that got better and a number that got worse are both results).
8. `runs/T46/REPORT.md` per `## Report` below.

## Constraints
- Do not commit, push, or touch any repository other than this one. Do not call paid services.
- Do not edit `docs/DECISIONS.md`, `CLAUDE.md`, `apps/vr/tasks/BACKLOG.md`, `apps/vr/data/pinned/**`,
  `apps/vr/data/PINNED.json`, `apps/vr/data/conformance/**`, `apps/vr/Game/Clips/**`, or
  `apps/vr/Game/Source/MageArenaVR/MageArenaVR.Build.cs`.
- **Do not re-do T37.** The per-colour material cache, the recolour-as-swap rule and the distinct-material cap are
  T37's deliverables. This card changes the **instrument** and, in deliverable 5, only the instrument's **default**.
- **Do not propose a Shipping-build gate** in any form: `MOONSHOT-2026-10.md:26` is a declined item and a durable no.
- **One total draw-call budget of ≤150 for the whole frame**, no separate particle line (DECISIONS 2026-10-16). Do not
  reintroduce a per-pass or per-particle target as a verdict row; per-pass splits stay diagnostics.
- **Do not shrink the stress population to make a number pass** (BUDGET-DG4 7.7, "alternatives that lost"). If
  deliverable 2 makes the scene heavier and the row moves further from ≤150, report the number; do not trim the scene.
- Do not weaken `bColoursOk` (`GreyboxCapture.cpp:568`) or any other check that currently works, and keep the budget
  JSON's "desktop proxy - not Quest truth" labelling and section 4's honesty about `GNumPrimitivesDrawnRHI`.
- Greybox materials stay unlit. Never set `r.Mobile.SupportGPUScene=0`, `r.MeshDrawCommands.DynamicInstancing=0` or
  `r.Mobile.MeshSortingMethod=1` (BUDGET-DG4 section 9, choice 3).
- Presentation and instrumentation only: **no combat number changes**, and the colour language holds (element colour =
  absorb, steel = dodge, black core with a red rim = leave). Where single-sourcing a colour would change what the
  presentation draws, the **presentation** is the source of truth and the stress plan moves to it, never the reverse.
- `MageArenaDesign.` is compared per test against the WOLF-PLAN P0 baseline, not against `CLAUDE.md`'s older list.
- Fresh checkout or worktree: regenerate the gitignored sigil corpus first
  (`node apps/vr/tools/clipgen/generate.mjs --corpus 30 --seed 1000`, then
  `node apps/vr/tools/clipgen/mousepaths.mjs`). Never run `generate.mjs` bare or with `--templates`.
- One Unreal build at a time (`-WaitMutex`); one `Automation RunTests` filter per editor process; never clean shared
  engine caches.

## Acceptance (the orchestrator re-runs these; quote their real output in your report)

Lynx-verifiable (no Unreal needed):
```
node apps/vr/tools/lint/format-check.mjs --self-test && node apps/vr/tools/lint/format-check.mjs --base origin/master
                                 # expected: format-check OK (changed C++ lines only)
node apps/vr/tools/lint/unused-members.mjs && node apps/vr/tools/lint/unused-members.mjs --self-test
                                 # expected: unused-members OK: <n> private/protected members in <m> classes
node apps/vr/tools/check-pin.mjs # expected: pin OK: 9 files @ baeac66 / conformance OK: 45 vectors (unchanged)
grep -rn "SessionPresentation.cpp:" apps/vr/Game/Source/MageArenaVR/Budget/
                                 # expected after deliverable 3: no matches (every reference is a symbol, not a line)
```

Wolf-only (UE build, the automation suites and real captures - do not claim these from any machine without UE):
```
powershell -NoProfile -File apps/vr/tools/build.ps1
                                 # expected: "Result: Succeeded" in the log, not only exit 0
UnrealEditor-Cmd MageArenaVR.uproject -ExecCmds="Automation RunTests MageArena.;Quit" -unattended -nullrhi -nosplash -log -ReportExportPath=runs/T46/MageArena
                                 # expected: all MageArena. green, including the new MageArena.Budget.Parity.* and
                                 # MageArena.Budget.Triangles.SingleSourced tests
UnrealEditor-Cmd MageArenaVR.uproject -ExecCmds="Automation RunTests MageArenaDesign.;Quit" -unattended -nullrhi -nosplash -log -ReportExportPath=runs/T46/MageArenaDesign
                                 # expected: no red beyond the WOLF-PLAN P0 baseline, per test
powershell -NoProfile -File apps/vr/tools/capture-budget.ps1 -Runs 3 -Prefix parity
                                 # expected: MAGEVR_BUDGET_DONE x3; both MAGEVR_BUDGET_POP lines at full count; NO
                                 # MAGEVR_BUDGET_FAIL scene parity; the three summaries carry the parity census (or
                                 # its hash) and the relabelled or newly-sourced plausibility field; the draw-call
                                 # row is reported as measured, whatever it is
powershell -NoProfile -File apps/vr/tools/capture-greybox.ps1
                                 # expected: MAGEVR_CAPTURE_DONE, the six pngs and budget.json; coloursOk still true
                                 # (unchanged); the plausibility field matches deliverable 4's choice
```
Two negative proofs, each quoted with the exact command and the exact failing line - a card that only shows the happy
path has not proven deliverable 3 or 4:
```
# 1. The parity check must fail on today's real divergence.
#    Run it against the pre-fix constants (HoundColour 0.8/0.3/0.05 vs HoundBody 0.28/0.07/0.02).
#    expected: MAGEVR_BUDGET_FAIL scene parity <...>, naming the enemy and the two colours, and a non-zero
#    launcher result. Quote it.
# 2. The plausibility field must be able to fail (deliverable 4, "real" option only).
#    Feed it a deliberately wrong counter or frustum (state how).
#    expected: plausibilityOk false and the capture failing. If you took the "honest" option instead, quote the
#    relabelled field and the corrected JSON "source" string, and state here that there is no desktop cross-check.
```

## Report
Write `runs/T46/REPORT.md`: the deliverable 1 parity table with every reading of A2-A5 marked confirmed or corrected;
which route deliverable 2 took, or the specific obstacle that stopped it; the parity tolerance you chose and the rows
of the table that justify it; which option deliverable 4 took and the Wolf measurement that decided it; the two
negative proofs verbatim; the red line of each new test before it passed; the `MAGEVR_BUDGET_MATERIALS` line and the
summary field proving deliverable 5, and whether the default needed a flip; the draw-call row as measured on the
parity instrument, compared with the 194 / 201 of BUDGET-DG4 10.2-10.3, stated plainly whichever way it moved;
whether this card ran before or after WOLF-PLAN P3 and why; each acceptance command with its **real** output; anything
you could not do with the exact error; and every decision this card did not specify.
