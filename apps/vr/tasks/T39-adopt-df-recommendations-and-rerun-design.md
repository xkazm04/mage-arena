# T39 - Adopt the DF-003/005/006/007/008/009 recommendations and re-run MageArenaDesign once

Status: ready for Wolf (the x64 UE host). Owner ruling `docs/DECISIONS.md` 2026-10-20: "adopt each DF's recommended
option; card one design-suite re-run." Written 2026-10-20 on Lynx (ARM64, no Unreal); nothing here was built or run.
Max turns: 240

## Goal
The six open design findings are answered by their recommended options, built as VR overlay data or UE C++, and the
`MageArenaDesign` suite is run once on the ruled overlay. Each of the five known failures
(`Duel.FireSeated`, `Duel.AirSeated`, `Session.FullSeated`, `Session.Wave1Seated`, `Session.CreaturesSeated`) ends green,
or stays red and is explained by an accepted DF in the report. Gate slice: **the full Tiro day goes to the 31 Oct gate;
no cut** (`docs/DECISIONS.md` 2026-10-20). Nothing in `apps/vr/data/pinned` changes.

## Read first
- `CLAUDE.md`, `docs/DECISIONS.md` (outranks this card), `apps/vr/data/README.md` (pin rules)
- Each DF in `docs/design-findings/` and its census folder (`DF-003-census/`, `DF-006-census/`, `DF-008-census/`)
- `apps/vr/tasks/T38-first-animated-opponent.md` is independent; do not mix its changes into this card's commits

## The rulings, per DF
Files are the ones each DF names or that the repo shows today; the builder confirms them with grep and records any
difference in the report. "Overlay" = VR data under `apps/vr/data/vr` or `apps/vr/data/scenarios-vr` (never `pinned`).

| DF | Recommended option (as the DF names it) | Kind | Files |
|---|---|---|---|
| DF-003 | `wide` knob set (physical block 0.90, magic block 0.92, wall 12 s / 6 mana / 150 deg, thicker dome, regen, one-hand power). The DF leaves `wide` in the proposal file as the lowest-penalty set and does not sign it off; the 2026-10-20 ruling does. Sunfall comes through DF-006's phases (DECISIONS 2026-10-07). | Overlay | `apps/vr/data/vr/calibration-proposal.json` values move into the live `apps/vr/data/vr/combat.vr.json`; scenarios `apps/vr/data/scenarios-vr/fire-duel-competence-1.json`, `fire-duel-competence-1-5.json` |
| DF-005 | A: re-arm on clear - when the split latch clears the ward counts as fresh for one `perfectWindowS`. VR overlay only; the 18-tick wait bounds it. | UE C++ (rule) plus overlay flag | `Source/MageArenaVR/Kernel/VrRules.{h,cpp}`, `Kernel/VrRulesLoad.cpp`, `Tests/DefenceTests.cpp`; flag in `apps/vr/data/vr/combat.vr.json` |
| DF-006 | A + B: adopt `wide` (as DF-003) and add a phase-break beat: on each break the rival is warded and does not cast for about 3 s while the crowd swells and the taunt plays. VR overlay only. The T21 addendum applies: measure the chained day as well as the standalone duel. | Overlay (A); UE C++ (B) | A: as DF-003. B: `Source/MageArenaVR/Kernel/Enemies.cpp`, `Kernel/VrRules.{h,cpp}`, the phase data in `apps/vr/data/vr/`, the voiced line in `audio.json` / `strings.json`; re-run the `DF-006-census/` census |
| DF-007 | A + B together: the reference script wards every second ember (the script, not the game), and the ember-spit cadence becomes 1.5 s instead of the pinned 0.4 s recovery plus windup. Re-measure. | UE C++ (test script); Overlay (cadence) | A: `Source/MageArenaVR/Tests/SessionTests.cpp`, `Session/CreaturesCapture.cpp` (reference script). B: spit cadence in `apps/vr/data/vr/combat.vr.json`, read through `Kernel/Enemies.cpp` |
| DF-008 | A: AI first - Lio wards like the Fire mage, uses Air Form against the player's bolts (not any threat), and spends tier II-III (Veering Bolt, Squall, Downdraft) instead of darts once unlocked. Re-measure. B (sheet numbers) and C (dome rule) are not adopted. | UE C++ | `Source/MageArenaVR/Kernel/Enemies.cpp`, `Kernel/Games.cpp`, `Tests/AirTests.cpp`; re-run the `DF-008-census/` census |
| DF-009 | A: count only spectacle - `missedCast` 0 for Bolts (tier 0), kept for tier I+ casts. | Overlay (plus C++ if the tier condition needs code) | `apps/vr/data/vr/collar.json`; `Source/MageArenaVR/Session/CollarLedger.cpp`, `Tests/CollarTests.cpp` |

VR overlay data: DF-003, DF-006 option A, DF-007 option B, DF-009 (and the flag for DF-005). UE C++: DF-005, DF-006
option B, DF-007 option A, DF-008. Order: DF-003/006A data first, then DF-009, DF-007B, then the C++ ones; one atomic
commit per DF (`T39: DF-00x ...`).

## Deliverables (exact paths)
- The edits in the table, one commit per DF.
- Tests that pin the changed behaviour (e.g. a re-arm test in `Tests/DefenceTests.cpp`).
- `runs/T39/REPORT.md` (untracked).

## Constraints
- Do not commit, push, or touch any repository other than this one. Do not call paid services.
- Do not edit `docs/DECISIONS.md` or `apps/vr/data/pinned/**`; no pinned combat number moves.
- All Unreal work runs on Wolf. Lynx never reports a UE build or test result.
- Regenerate the sigil corpus first (`CLAUDE.md`); never run `generate.mjs` bare.

## Acceptance (the orchestrator re-runs these on Wolf; quote their real output)
```
node apps/vr/tools/check-pin.mjs                                     # expected: pin OK
node apps/vr/tools/check-pin.mjs --self-test                         # expected: flipped byte fails the gate
powershell -NoProfile -File apps/vr/tools/build.ps1                  # expected: exit 0
UnrealEditor-Cmd MageArenaVR.uproject -ExecCmds="Automation RunTests MageArenaDesign;Quit" -unattended -nullrhi -nosplash -log
                                                                      # expected: ONE run on the ruled overlay; each of the five known
                                                                      # failures green, or red and explained by an accepted DF
UnrealEditor-Cmd MageArenaVR.uproject -ExecCmds="Automation RunTests MageArena.Kernel.Conformance;Quit" -unattended -nullrhi -nosplash -log
                                                                      # expected: the 45 conformance vectors green, byte-identical
```
The `MageArenaDesign` run on the final overlay is the evidence; any earlier run after a fix is listed as a rerun.

## Report
Write `runs/T39/REPORT.md`: per DF the option, the files changed and the before/after census numbers; the full
`MageArenaDesign` result with the five known failures marked green or explained; the check-pin output; the host name and
engine build; anything not done and the exact error.
