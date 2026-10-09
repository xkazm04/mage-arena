# T33 - Inject kernel data into the sim, and a pin-diff census for re-pins

Status: open (needs the build machine: UE 5.8 is not installed on the ARM64 laptop)
Max turns: 320

## Goal
The kernel steps whatever data the arena carries instead of the process-wide `KernelData()` singleton, and the loader takes an explicit source instead of the `GTextOverrides` global. On that seam, `MageArenaDesign.Census.PinDiff` runs the same seeded bouts against two pinned data sets in one process and reports the per-metric delta, and every census output records the pin and Air-sheet hashes it ran on. Every TV re-pin (how CR-004 to CR-008 come home) then gets a measured behaviour diff instead of a byte check.

Determinism package, accepted by the owner 2026-10-09 (`docs/research/MOONSHOT-2026-10.md`, item M6).
Run order: **T16, then (T33 -> T34) and (T35 -> T36)**. T33 has no code dependency on T16 and may start first.

## Read first
- `CLAUDE.md`, `docs/DECISIONS.md` (they outrank this card if they conflict)
- `docs/research/MOONSHOT-2026-10.md` section **M6 - Inject kernel data into the sim and add a pin-diff census for re-pins**: the full spec (Summary, Description, Flow, Write set,
  Acceptance, Evidence). This card adds only the run rules; where they differ, this card wins.
- Re-verify every premise in that section against the tree before writing code. The section was read on `83372e7`;
  a premise that no longer holds is reported, not built on.

## Deliverables (exact paths)
The write set of the deck section, plus `runs/T33/REPORT.md` (uncommitted).

## Constraints
- Do not commit, push, or touch any repository other than this one.
- Do not call paid services. Do not edit `docs/DECISIONS.md`, `apps/vr/data/pinned/` or any TV combat number.
- **Test first.** Every acceptance case is written as a test (or a counted lint) and seen RED on the unchanged code
  before the change; the report quotes the red line. A case that never failed proves nothing.
- Baseline the full `MageArena.` and `MageArenaDesign.` suites before the change and compare per test after. Known red
  on a clean tree: `MageArenaDesign.Duel.FireSeated`, `Session.FullSeated`, `Session.Wave1Seated`. Any other change
  is a regression unless this card names it.
- Fresh checkout: regenerate the sigil corpus first (`CLAUDE.md`, Build). One Unreal build at a time
  (`-WaitMutex`); one `Automation RunTests` filter per editor process; read verdicts from `-ReportExportPath`.
- Stay inside the section's write set; list any file outside it in the report with the reason.
- `apps/vr/tools/build.ps1` exits 0 when `Build.bat` is missing (found 2026-10-09): confirm the build really ran
  from its log, not from the exit code.
- **Invariant:** the 45 `MageArena.Kernel.Conformance.*` vectors and the `MageArena.Fire.Golden` hashes do not
  change by one bit. `FArenaState::Data` is non-owning and NOT hashed. Run both after each `Kernel/` file of the codemod.
- Keep `Session/` edits to the call sites the seam needs; T35 and T36 restructure that code next.

## Acceptance (the orchestrator re-runs these; quote their real output in your report)
```
powershell -NoProfile -File apps/vr/tools/build.ps1          # expected: the build log shows "Result: Succeeded"
UnrealEditor-Cmd ... "Automation RunTests MageArena.Kernel.DataInjection"   # expected: StepsVariant, Interleaved pass
UnrealEditor-Cmd ... "Automation RunTests MageArena.Kernel.NoGlobalData"    # expected: 0 KernelData() in Kernel/*.cpp beyond the default arg
UnrealEditor-Cmd ... "Automation RunTests MageArena.Kernel.Conformance"     # expected: 45/45, identical to baseline
UnrealEditor-Cmd ... "Automation RunTests MageArena.Fire.Golden"            # expected: pass, hashes unchanged
node apps/vr/tools/check-pin.mjs --export <pin commit> <dir>                # expected: writes a loadable candidate dir
UnrealEditor-Cmd ... "Automation RunTests MageArenaDesign.Census.PinDiff" -MageArenaPinB=<copy of pin A>   # expected: 0 delta on every metric
the same with one enemy's hp +50 % in the candidate                         # expected: non-zero delta only on bouts that spawn it
```

## Report
Write `runs/T33/REPORT.md`: base sha; commits; per acceptance case the red evidence and the green evidence (real
lines); the per-test suite comparison before/after; files outside the write set; premises that turned out false; what
is not done and why.
