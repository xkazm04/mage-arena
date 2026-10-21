# T38 - The first animated opponent: Brennic with idle, cast, hit and death, driven by kernel events

Status: decided 2026-10-20 (`docs/DECISIONS.md` 2026-10-20), source is AI-assisted authoring by Claude + owner (option 2): an
authored rig with four clips (idle, cast, hit, death), and Lynx's local TRELLIS + Blender rigging pipeline (PoF 3D
rounds) may produce the mesh and rig. Build and verify happen on Wolf (the x64 UE host); nothing on Lynx counts as a UE
result. Written 2026-10-20 on the ARM64 laptop for the Fri 23 Oct R0 checkpoint.
Max turns: 240

## Goal
One named opponent, Brennic (Ember Tent, the Tiro semifinal), stands in the greybox arena as a rigged, skinned 3D figure
that plays four animations - idle, cast, hit, death - chosen by the kernel's events, in place of his greybox body. It is
the "first animated opponent" the owner judges at the R0 reconsider checkpoint (`docs/PROJECT-PLAN.md` R0, section 10;
`docs/DECISIONS.md` 2026-10-02: "Risk is in the game design and animations of 3D models"). As of `3394744` the source tree
has no skeletal mesh, no animation asset and no `UAnimInstance` (`grep -rln "SkeletalMesh\|AnimSequence\|AnimInstance"
apps/vr/Game/Source` finds nothing); `Content/` holds only `Audio/SFX` and `Budget`.

## The owner decision (answered 2026-10-20: option 2, see Status)
Where the model and the four clips come from. The plan's options, in its own order:
1. **Meta Asset Library** (Start benefit, named fallback for R0, `docs/DECISIONS.md` 2026-10-02). Each asset's licence is
   checked against the "original, not a wrapper" rule and its use recorded. Free.
2. **AI-assisted authoring by Claude + owner** (`docs/ORCHESTRATION.md`, 3D row). No subscription covers it.
3. **Tripo-style paid credits**, only on a written request with reasoning (`docs/DECISIONS.md` 2026-10-02).
Not allowed: Fab or stock packs, engine sample content (the R8 reading in `docs/DECISIONS.md` 2026-10-08).
The owner also names the look: greybox-compatible (one flat colour per material, the Fire mage sheets in
`apps/vr/art/model-sheets/` from A04) or the style pick's skin, which is still open (T16).

## Hand-off: Lynx makes the mesh, Wolf imports and verifies
Lynx (Windows ARM64, no Unreal) may run TRELLIS and the Blender rigging step and write one rigged FBX (or GLB) with the
four clips named `Idle`, `Cast`, `Hit`, `Death`, plus a `SOURCE.md` draft (tool versions, prompts, seed, date).
- **Landing path:** `apps/vr/art/characters/brennic/export/` (Git LFS). Transport: git LFS, pushed to origin master (owner,
  2026-10-21, `docs/DECISIONS.md`). Wolf pulls, runs the pre-import checks below and imports to
  `Content/Characters/Brennic/` as before; only that import is committed there (LFS). The `work/` folder (`.blend`
  files, raw GLB, bake PNG) stays in the Lynx archive `C:\Users\kazda\kiro\mage-arena-archive\T38\Brennic\work\`.
- **Wolf checks before import:** one skeleton and one skinned mesh; exactly the four clips, none empty; LOD0 triangle
  count recorded (budget <=350k total with hands, arena and 20 actors); one flat colour per material, no embedded
  textures beyond the greybox rule; origin at the feet, +X forward, unit scale matching `FBody`; tool versions and
  prompts recorded in `SOURCE.md`. If a check fails, Wolf reports the exact defect for a Lynx re-export instead of
  fixing the mesh in the editor.
- Build, the two tests, the capture and `capture-budget.ps1` run on Wolf only. The report states "run on Wolf" with the
  host name and engine build.

## Read first
- `CLAUDE.md`, `docs/DECISIONS.md` (they outrank this card)
- `docs/PROJECT-PLAN.md` section 7 (presentation only reads kernel state; triangle row <=350k for hands + arena + 20 actors)
- `Session/SessionPresentation.{h,cpp}` (`FBody`, `BodyFor`, where the rival's body and `RivalFlare` are made and
  recoloured), `Kernel/SimTypes.h` and the event log the presentation already reads (cast, hit, death)
- `apps/vr/tasks/T37-shared-part-materials.md` (lands the shared-material rule; the opponent's materials follow it)

## Deliverables (exact paths)
1. The imported assets under `apps/vr/Game/Content/Characters/Brennic/` (LFS): one skeletal mesh, its skeleton and
   physics asset, four animation sequences named `A_Brennic_Idle`, `_Cast`, `_Hit`, `_Death`. A `SOURCE.md` beside them:
   origin, licence text or link, date, and who checked it against the rule.
2. `Session/RivalFigure.{h,cpp}`: an actor component that owns the skeletal mesh and plays the clip the kernel's events
   select (idle by default; cast on the rival's cast event; hit on a damage event; death on the bout's end at 0 HP, held).
   It reads events only; it never writes a number.
3. `SessionPresentation` uses the figure for Brennic when the assets load, and the greybox body when they do not (a fresh
   clone without LFS still runs the suite).
4. Tests: `MageArena.Presentation.RivalFigure.ClipFollowsEvents` (a scripted event list gives the expected clip sequence,
   headless, no rendering), and `MageArena.Presentation.RivalFigure.FallsBackToGreybox` (missing assets).
5. A capture for the owner: `apps/vr/tools/capture-rival.ps1` writes `runs/T38/` stills or a short video of one seated
   Brennic duel replay (seed 1, the reference seated script), with the four clips visible.

## Constraints
- Do not commit, push, or touch any repository other than this one. Paid services only if the owner chose option 3 in
  writing.
- Do not edit `docs/DECISIONS.md`, the pinned data or the conformance vectors. Do not change any combat number.
- Triangle budget: the figure's LOD0 is stated in the report; the D-G4 triangle row is re-measured with
  `capture-budget.ps1` and stays <=350k.
- Known red tests on a clean tree are the five in `CLAUDE.md`; any other change is a regression.

## Acceptance (the orchestrator re-runs these; quote their real output in your report)
```
powershell -NoProfile -File apps/vr/tools/build.ps1                     # expected: exit 0
UnrealEditor-Cmd MageArenaVR.uproject -ExecCmds="Automation RunTests MageArena.;Quit" -unattended -nullrhi -nosplash -log
                                                                         # expected: all green, incl. the 2 new tests
powershell -NoProfile -File apps/vr/tools/capture-rival.ps1              # expected: runs/T38/ capture with all four clips
powershell -NoProfile -File apps/vr/tools/capture-budget.ps1 -Runs 3 -Prefix rival
                                                                         # expected: triangle row <=350k in all three
```
The owner judges the capture (**felt**); the card cannot pass that part itself.

## Report
Write `runs/T38/REPORT.md`: the source and licence record, what you built (paths), each acceptance command with its
real output, the triangle count, anything you could not do and the exact error, and decisions the card did not specify.
