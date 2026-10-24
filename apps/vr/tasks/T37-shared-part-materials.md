# T37 - Shared part materials in SessionPresentation (D-G4 lever B)

Status: done on Wolf (kaziho) 2026-10-24, UE 5.8.3 CL 58210709, commit 4352581; Done-when met. Way (b), the per-colour
cache: step 1 re-verified the 16 casts and found no unquantisable per-frame colour. Build `Result: Succeeded`;
MageArena. 252/252 (the 3 new SharedMaterials tests, RecolourDoesNotLeak seen red first); MageArenaDesign. per test
equal to P2 (FireSeated, AirSeated, FullSeated red with identical messages, DF-011; the card's list of five below is
stale since T39); `capture-budget.ps1 -Runs 3 -Prefix leverb` draw calls median 70, max 73 / 75 / 73, live particles
max 1,468, 1 GPU emitter; `docs/research/BUDGET-DG4-2026-10.md` section 11. The pass is on the stress proxy until T46
(scene parity). Report: runs/T37/REPORT.md. (Was: open, written 2026-10-20 on the ARM64 laptop.)
Max turns: 240

## Goal
The session's greybox parts that share a mesh and a colour also share one material, so dynamic instancing merges them,
and the D-G4 draw-call row (one total of <=150 for the whole frame, owner ruling 2026-10-16) can pass on the desktop
proxy. Today the row **fails**: median 194, max 201 with the 6 `NS_Budget_` systems (`docs/research/BUDGET-DG4-2026-10.md`
10.2-10.3). Lever A (occlusion queries off) is shipped. Lever B is the lever that closes the gap (7.7, 8.5, 9): in the
diagnostic variant it took Basepass from 118 to 21 draws, and A + B together gave median 58-59, max 62 (7.2, 7.4). It is
a 31 Oct desktop core gate row, and nothing else on the plan closes it.

## Read first
- `CLAUDE.md`, `docs/DECISIONS.md` (2026-10-16: draw calls are one total budget; the two `bBuildEditor` blocks in
  `MageArenaVR.Build.cs` stay separate - this card does not touch `Build.cs`)
- `docs/research/BUDGET-DG4-2026-10.md` sections 7.7 and 9 (section 9 wins where they differ), 10
- `docs/research/MOBILE-INSTANCING-2026-10.md` sections 1.2-1.7 and 5 (both ways merge on Quest; the greybox stays
  unlit; three config lines must never be set)
- `apps/vr/tasks/BACKLOG.md`, the "Presentation (D-G4 lever B ...)" item
- Code: `Session/SessionPresentation.cpp` (`MakePart` at about line 175, and every
  `Cast<UMaterialInstanceDynamic>(...->GetMaterial(0))` followed by `SetVectorParameterValue(TEXT("Color"), ...)`:
  16 such casts at `3394744`), `Greybox/GreyboxUtil.cpp` (`Greybox::Tint`), `Budget/BudgetStressDriver.{h,cpp}`
  (`FBudgetMaterialCache`, `BudgetSharedTint`, `-MageArenaBudgetSharedMaterials`), `Tests/ColourTests.cpp`,
  `Tests/BudgetStressTests.cpp`.

## The choice this card makes (App Master, 2026-10-20)
Way **(b)**, a per-colour material cache, unless step 1 below finds a site that writes a continuously varying colour
every frame (a fade or lerp that cannot be quantised to a small set). Reasons: (b) is the variant D-G4 measured (the
driver's `BudgetSharedTint`); (a) carries the unread UE-160597 device risk (section 9, choice 2). If step 1 finds such a
site, stop after step 1 and report it; the App Master then decides (a) for those parts.

## Deliverables (exact paths)
1. `runs/T37/recolour-sites.md` (uncommitted): every place in `Session/` that creates or recolours a part material, with
   file:line, the part, and whether the colour is one of a fixed set (palette entry, element colour, HP band) or varies
   per frame. Re-verify the 16-cast count first; a different count is reported, not assumed.
2. A shared cache owned by the presentation actor: a `UPROPERTY()`-held map from colour to `UMaterialInstanceDynamic*`
   (held by a UPROPERTY so GC sees it; the driver's raw `TMap` is a diagnostic and is not copied as is), with a
   `SharedTint(Colour)` helper. Put the helper where both `SessionPresentation` and `Budget/` can call it (for example
   `Greybox/GreyboxUtil.{h,cpp}`), so the driver stops keeping its own copy.
3. `MakePart` takes its material from the cache. Every runtime recolour becomes a **swap** (`SetMaterial(0,
   SharedTint(NewColour))`) and never a `SetVectorParameterValue` on a shared instance. The cuff and glyph materials
   (`CuffMaterial`, `GlyphMaterial`) and the translucent curtains keep their own instances unless step 1 shows they are
   duplicated per part.
4. Any per-frame colour that remains is quantised (state a step, for example 1/16 per channel) so the cache stays
   bounded; log the distinct-material count at the end of a session (`MAGEVR_PRESENTATION_MATERIALS distinct=<n>`).
5. The stress driver's default path follows the shipping presentation (section 7.7, last bullet): the parts use the same
   `SharedTint`. `-MageArenaBudgetSharedMaterials` may stay as a no-op alias or be removed with its test updated.
6. Tests (in `Tests/ColourTests.cpp` or a new `Tests/SharedMaterialTests.cpp`):
   - `MageArena.Presentation.SharedMaterials.SameColourSameMaterial`: two parts made with one colour hold the same
     material object; two colours hold two.
   - `MageArena.Presentation.SharedMaterials.RecolourDoesNotLeak`: recolouring part A leaves part B (same start colour)
     unchanged. Seen RED by first writing it against a version that calls `SetVectorParameterValue` on the shared
     instance; quote the red line.
   - `MageArena.Presentation.SharedMaterials.Bounded`: a full scripted session (`FullSeated` script) ends with a
     distinct count under a stated cap.
7. `docs/research/BUDGET-DG4-2026-10.md` gets a new section 11 "Lever B shipped" in the house style of section 8
   (provenance, results per run, verdicts). Sections 0-10 are not edited.

## Constraints
- Do not commit, push, or touch any repository other than this one. Do not call paid services.
- Do not edit `docs/DECISIONS.md`, `apps/vr/data/pinned/`, the conformance vectors, or `MageArenaVR.Build.cs`.
- Greybox materials stay unlit. Never set `r.Mobile.SupportGPUScene=0`, `r.MeshDrawCommands.DynamicInstancing=0` or
  `r.Mobile.MeshSortingMethod=1` (section 9, choice 3).
- Presentation only reads kernel state: no combat number changes, and the colour language holds (element colour =
  absorb, steel = dodge, black core with a red rim = leave). `MageArena.Colour.*` stays green unchanged.
- Fresh checkout: regenerate the sigil corpus first (`CLAUDE.md`, Build). One Unreal build at a time; one
  `Automation RunTests` filter per editor process.
- Baseline the full `MageArena.` and `MageArenaDesign.` suites before the change and compare per test after. Known red
  on a clean tree (`CLAUDE.md`): `MageArenaDesign.Duel.FireSeated`, `Duel.AirSeated`, `Session.FullSeated`,
  `Session.Wave1Seated`, `Session.CreaturesSeated`. Any other change is a regression.
- Changed C++ lines pass `node apps/vr/tools/lint/format-check.mjs`; new members pass
  `node apps/vr/tools/lint/unused-members.mjs`.

## Acceptance (the orchestrator re-runs these; quote their real output in your report)
```
powershell -NoProfile -File apps/vr/tools/build.ps1                     # expected: exit 0
UnrealEditor-Cmd MageArenaVR.uproject -ExecCmds="Automation RunTests MageArena.;Quit" -unattended -nullrhi -nosplash -log
                                                                         # expected: all MageArena. green, incl. the 3 new tests
UnrealEditor-Cmd MageArenaVR.uproject -ExecCmds="Automation RunTests MageArenaDesign.;Quit" -unattended -nullrhi -nosplash -log
                                                                         # expected: exactly the 5 known reds, numbers unchanged
powershell -NoProfile -File apps/vr/tools/capture-budget.ps1 -Runs 3 -Prefix leverb
                                                                         # expected: MAGEVR_BUDGET_DONE x3; draw calls max <=150 in
                                                                         # all three summaries; particle rows still pass (<=2,000, <=1 GPU)
node apps/vr/tools/lint/format-check.mjs && node apps/vr/tools/lint/unused-members.mjs
                                                                         # expected: OK
```
If the draw-call max is still above 150 with (b) on master, report the per-pass split (Basepass, translucency, shadow,
other) from the capture; do not shrink the stress population (7.7, "alternatives that lost").

## Report
Write `runs/T37/REPORT.md`: the recolour-site table, what you built (paths), each acceptance command with its real
output, the red lines of the new tests, the three `leverb` summaries, anything you could not do and the exact error,
and decisions the card did not specify.
