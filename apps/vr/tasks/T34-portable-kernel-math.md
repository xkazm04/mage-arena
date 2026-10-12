# T34 - Bit-identical kernel maths on Win64, Quest and V8 (desktop half)

Status: open (needs the build machine: UE 5.8 is not installed on the ARM64 laptop)
Max turns: 320

## Goal
Every transcendental the kernel uses goes through one portable `SimMath` (fdlibm-lineage sin/cos/atan2 and V8's hypot algorithm) with floating-point contraction off, matched bit for bit against node's `Math.*`. A non-shipping `-MageArenaKernelSelfTest` boot path prints `MAGEVR_KERNEL_HASH`, so the V1 device lane can prove the Quest kernel is the Win64 kernel. The device comparison itself is a V1 checklist row, not this card.

Determinism package, accepted by the owner 2026-10-09 (`docs/research/MOONSHOT-2026-10.md`, item M7).
Run order: **T16, then (T33 -> T34) and (T35 -> T36)**. T34 follows T33.

## Read first
- `CLAUDE.md`, `docs/DECISIONS.md` (they outrank this card if they conflict)
- `docs/research/MOONSHOT-2026-10.md` section **M7 - Make the kernel bit-identical on Win64, Quest and V8, and prove it on device**: the full spec (Summary, Description, Flow, Write set,
  Acceptance, Evidence). This card adds only the run rules; where they differ, this card wins.
- Re-verify every premise in that section against the tree before writing code. The section was read on `83372e7`;
  a premise that no longer holds is reported, not built on.
- T33 committed (this card lands on its seam).

## Deliverables (exact paths)
The write set of the deck section, plus `runs/T34/REPORT.md` (uncommitted).

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
- First step: read UBT's real Win64 and Android contraction / fast-math flags (engine sources or the `.rsp`
  files) and record them in `docs/XR-TOOLCHAIN.md` before writing any maths.
- The portable functions are written from published algorithms; state their provenance and licence in a header comment.
- The two Fire goldens are regenerated in their OWN commit with before and after hashes. Tolerances are never
  loosened; the conformance test logs each vector's maximum deviation.
- Acceptance 7 (the Quest logcat marker) goes onto the V1 checklist; say where.

## Acceptance (the orchestrator re-runs these; quote their real output in your report)
```
powershell -NoProfile -File apps/vr/tools/build.ps1          # expected: the build log shows "Result: Succeeded"
node apps/vr/tools/conformance/math-oracle.mjs --check                      # expected: 4/4 hashes equal the committed file
UnrealEditor-Cmd ... "Automation RunTests MageArena.Kernel.Math"            # expected: V8Oracle 4/4, NoContraction pass; seeded red quoted
git grep -nE "std::(sin|cos|atan2|hypot)|FMath::Cos" -- apps/vr/Game/Source/MageArenaVR/Kernel apps/vr/Game/Source/MageArenaVR/Combat/AbsorbResolver.cpp   # expected: only SimMathPortable.cpp
UnrealEditor-Cmd ... "Automation RunTests MageArena.Kernel.Conformance"     # expected: 45/45
<staged Win64 game build> -MageArenaKernelSelfTest                          # expected: MAGEVR_KERNEL_SELFTEST=PASS and the editor's MAGEVR_KERNEL_HASH
```

## Report
Write `runs/T34/REPORT.md`: base sha; commits; per acceptance case the red evidence and the green evidence (real
lines); the per-test suite comparison before/after; files outside the write set; premises that turned out false; what
is not done and why.
