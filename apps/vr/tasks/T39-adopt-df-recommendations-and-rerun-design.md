# T39 - Adopt the DF-003/005/006/007/008/009 recommendations and re-run MageArenaDesign once

Status: built on Wolf (kaziho) 2026-10-24, UE 5.8.3 CL 58210709; Done-when NOT met. All six DFs landed, one commit each (5b13463 DF-003/006 A, 0acf0c2 DF-009, a64eab5 DF-007 B, bef9323 DF-005, 0f33106 DF-006 B, 7f4926d DF-007 A, 6f00555 DF-008), T54 goldens re-recorded per DF. check-pin `pin OK` / `conformance OK: 45 vectors`, build `Result: Succeeded`, MageArena.Kernel.Conformance 45/45, MageArena. 249/249. The one MageArenaDesign run on the final overlay: Wave1Seated and CreaturesSeated green; FireSeated (brennic-c1 lost at 38.32 s), AirSeated (23.12 s) and FullSeated (lost bout 3) stay red, explained only by the open DF-011, so "each green or explained by an accepted DF" fails. Report: runs/T39/REPORT.md. (Was: ready for Wolf, after T54.) Owner ruling `docs/DECISIONS.md` 2026-10-20: "adopt each DF's recommended
option; card one design-suite re-run." Written 2026-10-20 on Lynx (ARM64, no Unreal); nothing here was built or run.
Max turns: 240

Amended 2026-10-21 and again 2026-10-22 on Lynx per ARCHITECTURE-REVIEW-2026-10-21 A4 (the second pass points DF-003 at T54's goldens and makes T54 a precondition) (docs only; nothing built or run): the DF-003 row no
longer lists the two null-path goldens, the "Test files each DF commit may update" section is new, DF-005, DF-006 B and
DF-008 are steered to the overlay hooks, and the DF-003 commit also fixes the stale text in `combat.vr.json`. The owner
ruling, Goal and Max turns are unchanged.

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
| DF-003 | `wide` knob set (physical block 0.90, magic block 0.92, wall 12 s / 6 mana / 150 deg, thicker dome, regen, one-hand power). The DF leaves `wide` in the proposal file as the lowest-penalty set and does not sign it off; the 2026-10-20 ruling does. Sunfall comes through DF-006's phases (DECISIONS 2026-10-07). | Overlay | `apps/vr/data/vr/calibration-proposal.json` values move into the live `apps/vr/data/vr/combat.vr.json`. The same commit replaces the stale "until the TV change request lands" text at `combat.vr.json:68` and "until the TV change request (CR-005) lands" at `:134` (TV is obsolete, DECISIONS 2026-10-21). The two `Fire.Golden` files under `scenarios-vr` are null-path baselines the overlay cannot move: leave them alone. The goldens that show what this DF moved are T54's (`apps/vr/data/scenarios-vr/overlay/*.json`, `T54-overlay-end-state-goldens.md`): re-record them in this commit and explain every moved file. Tests that hard-code live values: see "Test files each DF commit may update" |
| DF-005 | A: re-arm on clear - when the split latch clears the ward counts as fresh for one `perfectWindowS`. VR overlay only; the 18-tick wait bounds it. | UE C++ (rule) plus overlay flag | Put the rule in `FVrRuleset::TickSplitLatch` (`Kernel/VrRules.cpp:465`, declared `VrRules.h:252`, called from `Kernel/ArenaKernel.cpp:506`), not in a pinned-port file. `Kernel/VrRules.{h,cpp}`, `Kernel/VrRulesLoad.cpp`, `Tests/DefenceTests.cpp`; flag in `apps/vr/data/vr/combat.vr.json` |
| DF-006 | A + B: adopt `wide` (as DF-003) and add a phase-break beat: on each break the rival is warded and does not cast for about 3 s while the crowd swells and the taunt plays. VR overlay only. The T21 addendum applies: measure the chained day as well as the standalone duel. | Overlay (A); UE C++ (B) | A: as DF-003. B: build the beat in `Source/MageArenaVR/Kernel/VrRivals.cpp` inside `FVrRuleset::TickRivalPhases` (`VrRivals.cpp:60`, declared `VrRules.h:276`, called at `ArenaKernel.cpp:536`), with `Kernel/VrRules.{h,cpp}` for any new field and no new inline branch in `Kernel/Enemies.cpp` (it already has overlay branches); the phase data in `apps/vr/data/vr/`, the voiced line in `audio.json` / `strings.json`; re-run the `DF-006-census/` census |
| DF-007 | A + B together: the reference script wards every second ember (the script, not the game), and the ember-spit cadence becomes 1.5 s instead of the pinned 0.4 s recovery plus windup. Re-measure. | UE C++ (test script); Overlay (cadence) | A: `Source/MageArenaVR/Tests/SessionTests.cpp`, `Session/CreaturesCapture.cpp` (reference script). B: spit cadence in `apps/vr/data/vr/combat.vr.json`, read through `Kernel/Enemies.cpp` |
| DF-008 | A: AI first - Lio wards like the Fire mage, uses Air Form against the player's bolts (not any threat), and spends tier II-III (Veering Bolt, Squall, Downdraft) instead of darts once unlocked. Re-measure. B (sheet numbers) and C (dome rule) are not adopted. | UE C++ | Use an `FVrRuleset`-gated hook the way `ConsiderFireWall` is wired: `MageInput(State, Actor, const FVrRuleset* Rules)` in `Kernel/MageAI.cpp:807` (declared `MageAI.h:21`, null Rules = pinned path) with new fields in `Kernel/VrRules.{h,cpp}` and `Kernel/VrRulesLoad.cpp`. Touch `Kernel/Enemies.cpp` / `Kernel/Games.cpp` only if no hook can carry a rule, and then record in the report why not. Tests in `Tests/AirTests.cpp`; re-run the `DF-008-census/` census |
| DF-009 | A: count only spectacle - `missedCast` 0 for Bolts (tier 0), kept for tier I+ casts. | Overlay (plus C++ if the tier condition needs code) | `apps/vr/data/vr/collar.json`; `Source/MageArenaVR/Session/CollarLedger.cpp`, `Tests/CollarTests.cpp` |

VR overlay data: DF-003, DF-006 option A, DF-007 option B, DF-009 (and the flag for DF-005). UE C++: DF-005, DF-006
option B, DF-007 option A, DF-008. Order: DF-003/006A data first, then DF-009, DF-007B, then the C++ ones; one atomic
commit per DF (`T39: DF-00x ...`).

## Test files each DF commit may update
Several tests hard-code numbers that come from the live `combat.vr.json` or `collar.json`, so each DF commit may edit
literals in these files under `apps/vr/Game/Source/MageArenaVR/Tests/` (found by grep 2026-10-21):
`DefenceTests.cpp` (e.g. `FireWallDuration` `UntilTick == 181` at `:1520`, `:1424`; mana 12 at `:1411`; burn 12 at
`:1495`; heat per stop at `:1412`, `:1443`; one-hand power at `:664`), `CreatureTests.cpp`, `RivalTests.cpp` (phase
thresholds, `:26`), `DayTests.cpp`, `SessionTests.cpp`, `CollarTests.cpp` (cites `collar.json`). Also read
`CensusTests.cpp`, `RivalCensusTests.cpp` and `SettingsTests.cpp`, which cite `combat.vr.json`.
Rule: every literal you change cites, in its comment or the commit message, the overlay field it now follows (for example
`combat.vr.json fireWall.durationS 12 = 720 ticks, UntilTick 721`). Change no test for any other reason.

Required before the first `T39:` commit: `T54-overlay-end-state-goldens.md` (review A1) is recorded on today's overlay, so each
`T39: DF-00x` commit re-records `apps/vr/data/scenarios-vr/overlay/*.json` and its diff shows which bouts that DF moved. Also
recommended: review A3b (log the overlay sha256 and variant at boot), folded into T54. Review: `docs/research/ARCHITECTURE-REVIEW-2026-10-21.md`.

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
