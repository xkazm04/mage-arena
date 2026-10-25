# Desktop core gate record - DRAFT (2026-10-25)

- **Status: DRAFT.** Not a gate record yet.
- **Read at commit:** `cda7f58` (`T27: status - non-audio half done (audio deferred, ruling 6); report and WOLF-PLAN P7 evidence`), branch cut from local `master`, 2026-10-25.
- **The final record is re-measured on Wolf on 30-31 Oct.** Nothing in this draft was run: it was written on a host with no Unreal process (WOLF-PLAN P8 allows it, T63 holds the machine's one Unreal slot). Every state below is read from committed records. Where a record is older than the newest code, the row says so.
- Gate: `docs/PROJECT-PLAN.md:47` (Desktop core gate, Sat 31 Oct). Row numbers and order: K3, `docs/research/KPI-2026-10-20.md:89-117`. WOLF-PLAN P8: `docs/concepts/WOLF-PLAN.md:340-351` ("each with its Wolf evidence or 'not met'").
- Wolf = hostname `kaziho`, UE 5.8.3 CL 58210709 (`docs/concepts/WOLF-PLAN.md:56`).
- K3 counts a row only on a dated committed pass on 5.8.3. States: **met** / **partly met** / **not met** / **owner's**. Lines in quotes are copied from the cited file.

## What is still moving before 31 Oct

| Item | State at `cda7f58` | Source |
|---|---|---|
| **T63** spell flipbook | In flight. No `T63-*` card exists in this tree, so nothing about it is recorded here. Its row effect, if any, is rows 1 and 4-7 (visuals, D-G4), and it holds the one Unreal slot. | the task brief; `ls apps/vr/tasks/T63*` finds nothing |
| **T28** intermission scenes | `Status: done 2026-10-25 (part 2, T62 ...); one finding open: the idle kernel steps shift later bouts`. Audio stays deferred. | `apps/vr/tasks/T28-intermission-scenes.md:3` |
| **T27** voice and chorus | Non-audio half done on Wolf 2026-10-25; the audio half (14 cues) is **deferred, ruling 6**. | `WOLF-PLAN.md:311-329`; `docs/DECISIONS.md:319` |
| **T41** cold-tester sessions | **Deferred, ruling 4**: "No need testers for the current state we have now". Protocol stays. | `docs/DECISIONS.md:295-302` |
| **DF-011** (Brennic still wins) | Ruled 2026-10-24 "Accept the reds". Closed, but it keeps rows 10-11 red as written. | `docs/DECISIONS.md:353`; `docs/design-findings/DF-011-after-t39-brennic-still-wins.md` |
| **DF-013** (intermission idle steps move bouts 2-4) | **OPEN, awaiting an owner ruling.** | `docs/design-findings/DF-013-intermission-idle-steps-shift-bouts.md:4` |
| **T55** no debug text in views | Status line: `in flight, Unreal lane dispatched at wake bb6753ed` (may be stale). | `apps/vr/tasks/T55-no-debug-text-in-views.md:3` |
| P5 / P9 (APK, simulator, device) | **Deferred, ruling 2.** | `docs/DECISIONS.md:261-271` |
| T40 music | **Deferred, ruling 6.** | `docs/DECISIONS.md:319-327`; `WOLF-PLAN.md:303` |

## The 15 rows

| # | Gate item and pass condition (K3, quoted) | State | Evidence found (quoted) | If not met: the command or card that would produce it; WOLF-PLAN phase |
|---|---|---|---|---|
| 1 | "The full session in the chosen style" | **partly met** | T16 capture ran on Wolf: "`capture-style-pick: ok`, exit 0; `MAGEVR_CAPTURE_DONE shots=1870` x4; events-greybox/13/25/30.jsonl md5 `7b0715c871ac3399504c4f7a744896d4` x4" (`WOLF-PLAN.md:234-236`, `122e480`, `3f6c5bd`). There is no chosen style: "No single style is locked ... Greybox stays the **base**" (`docs/DECISIONS.md:242-249`, ruling 1). T16 status: "done up to the owner's pick" (`apps/vr/tasks/T16-...md:3`). **Wolf evidence 2026-10-25, `e9d8abc`, UE 5.8.3:** `capture-style-pick: ok`, exit 0, `MAGEVR_CAPTURE_DONE shots=1870` x4, events md5 `7b0715c8...` x4 (same as T16), 4 MP4s + 16 stills; the stills show no debug text but no readable rival in three of four views (`docs/research/GATE-CAPTURES-2026-10-25.md`). | The condition's wording ("chosen style") was answered by ruling 1, not by a pick. Whether "full session, greybox base with the art-concept pass" satisfies it is **the owner's call**. Evidence to attach on 30-31 Oct: `powershell -NoProfile -File apps/vr/tools/capture-style-pick.ps1` on the final tree (after T63/T55), then the owner reads the stills. **P4**, plus the visual lane T55-T63 (`WOLF-PLAN.md:401-405`). |
| 2 | "Opponent animated (idle, cast, hit, death; the gate's animation verdict, R0)" | **not met** | Brennic and the hound were imported into UE by a commandlet "into a scratch Content folder that was deleted again" (`docs/research/CREATURE-UE-IMPORT-2026-10-24.md:5`): "Clips `Idle` 2.000 s / 61 keys, `Cast` 1.533 / 47, `Hit` 0.800 / 25, `Death` 2.000 / 61" (`:65`). Accepted "as working in-engine assets (working, not final)" (`docs/DECISIONS.md:158-163`). But `apps/vr/Game/Content` holds only `Audio/ Budget/ Environment/ Vfx/`, and `T38-first-animated-opponent.md:3` still reads "Status: decided". | Needs T38 Done-when (`WOLF-PLAN.md:290-293`): import to `Content/Characters/Brennic/`; `MageArena.Presentation.RivalFigure.ClipFollowsEvents` and `.FallsBackToGreybox` green; `capture-rival.ps1` (new, not yet written) writes `runs/T38/` with all four clips; `capture-budget.ps1 -Runs 3 -Prefix rival`. The animation verdict is the owner's. **P6**. |
| 3 | "Every asset under P15 ceilings" | **not met** | none. "no local P15 tool exists in `apps/vr/tools/`" (`WOLF-PLAN.md:244-245`). Per-asset metrics exist in the import reports (Brennic "23,990 LOD0 tris", `CREATURE-UE-IMPORT-2026-10-24.md:65`), but the ceilings are unconfirmed: "The plan's own P15 example sets the soldier ceiling at 8k. That is a budget the owner should confirm" (`docs/concepts/VISUAL-PLAN.md:747`). | A pof P15 pass over every imported asset (`docs/PROJECT-PLAN.md` section 8 P15); **[verify on Wolf]** whether pof can run it. The owner confirms the ceilings first (VISUAL-PLAN section g). No card exists for the pass. **P4** note, `WOLF-PLAN.md:244-245`. |
| 4 | "Draw calls <=150 (one total, owner 2026-10-16)" | **met** (re-measure) | "the RHI total stays at max 126 of 150" (`docs/research/CROWD-T57-2026-10-25.md:24`); "Max 126 of 150, the same as T61's 122 / 126: **+0**" (`docs/research/BRAZIERS-T62-2026-10-25.md:183`, `dc0a49f`). Earlier: "draw calls median 100 / 100 / 99, max 104 x3" (`WOLF-PLAN.md:225`, `57b5e52`). The 2026-10-16 fail (median 194, max 201) is superseded. | Re-measure on the final tree: `powershell -NoProfile -File apps/vr/tools/capture-budget.ps1 -Runs 3 -Prefix <run>`, expect `MAGEVR_BUDGET_DONE` x3 and `MAGEVR_BUDGET_PARITY ... ok=1`. **P3** (T37, T46). T63 and T55-T59 can move this number. |
| 5 | "Visible triangles <=350k" | **met** (re-measure) | Ruled: "The row is judged on the GPU-culled BasePass reading ... max **139,280**, which **passes** the 350k budget. The all-pass figure stays a logged number, not a gate" (`docs/DECISIONS.md:341-347`, `57b5e52`). Later: BasePass "sits at 191k of 350k and does not bind" (`docs/DECISIONS.md:83-95`, `a02aa89`). | As row 4. The BasePass figure with the creatures in the scene is the one to quote on 30-31 Oct. **P3**. |
| 6 | "Live particles <=2,000" | **met** (re-measure) | "live particles max 1,468, GPU emitters 1" (`WOLF-PLAN.md:219`, `4352581`); "`particlesExercised` true and parity ok" (`DRAWCALL-BUDGET-2026-10-25.md:80`). | As row 4. T63 (spell flipbook) can move the particle count; re-check after it lands. **P3**. |
| 7 | "<=1 GPU emitter" | **met** (re-measure) | "GPU emitters 1" (`WOLF-PLAN.md:219`, `docs/research/BUDGET-DG4-2026-10.md` section 11). | As row 6. **P3**. |
| 8 | "Threat cues spatialized" | **partly met** | "20 SFX playing spatialised (not yet heard by a person)" (`apps/vr/tasks/T26-element-colour-and-first-audio.md:3`, 2026-10-07; the same card line: "audio capture -23.2 LUFS"). The audio capture launcher's integrity was fixed later (T45 audio half: it "fails a header-only and a 20.8 s WAV", `WOLF-PLAN.md:397`). 15 more SFX cues exist but are not imported (`KPI-2026-10-22.md`, `7c2a0bb`). **Wolf evidence 2026-10-25, `e9d8abc`, UE 5.8.3:** `capture-colour-audio.ps1 -Run GATE-R8-2026-10-25 -SkipStills` exit 0, `MAGEVR_CAPTURE_DONE mode=audio`, WAV 30.677 s, peak -5.9 dBFS, integrated -23.8 LUFS (T26: -23.2), md5 `621091d5...`; staged for the owner to listen (`docs/research/GATE-CAPTURES-2026-10-25.md`). Still not heard by a person. | Re-run on 5.8.3 with the T45 launcher: `powershell -NoProfile -File apps/vr/tools/capture-colour-audio.ps1 -Run <id> -SkipStills`, expect `MAGEVR_CAPTURE_DONE`. A person must hear it (owner). Audio is deferred (ruling 6), but the 20 SFX already imported are explicitly untouched (`docs/DECISIONS.md:319-327`). **P7**. |
| 9 | "Two music loops" | **not met** | none. `T40-two-music-loops.md:3`: "Status: open, blocked on an owner input". **Deferred, ruling 6**: "No audio spend ... no ElevenLabs call is made for music" (`docs/DECISIONS.md:319-327`). | T40: `import-sfx.ps1 -Run T40`, `MageArena.Audio.*` green, `capture-colour-audio.ps1 -Run T40 -SkipStills` (`WOLF-PLAN.md:303-307`). Needs the owner to lift ruling 6 and give the source. **P7**. |
| 10 | "Fire duel median 45-80 s (`MageArenaDesign.Duel.FireSeated`)" | **not met** (red, accepted) | After T39: "`Expected 'brennic-c1 length 38.32s in 45-80' to be true.`" (`WOLF-PLAN.md:205`, `6f00555`); CLAUDE.md: "`Duel.FireSeated` (brennic-c15 reaches 1 of 2 phases)". Accepted: DF-011 "Accept the reds" (`docs/DECISIONS.md:353`). Current: `MageArenaDesign.` "5/7, only DF-011's two" (`WOLF-PLAN.md:319`, 2026-10-25). | The accepted red is not a pass. Green needs a new owner ruling on DF-011 (option other than A, `DF-011-...md:40`). Re-measure: `Automation RunTests MageArenaDesign.` (`WOLF-PLAN.md:426-432`). **P2** (landed by ruling). |
| 11 | "The full Tiro day in 7-10 min (`Session.FullSeated`; the other three design tests, Air, Wave 1 and Creatures, travel with it)" | **not met** (day length now a measurement, not a gate) | DF-010 E: the day-length assertion left `bPass` (T47, `WOLF-PLAN.md:393`, `69b349a`/`086854e`/`f64c685`); the 7-10 min "stays the plan's, measured, not enforced" (`KPI-2026-10-22.md`). Measured 2026-10-25: "`dayWithTeachS=212.31` (rushed) and `dayNaturalS=305.72` (natural). Both stop at the bout-3 loss." (`WOLF-PLAN.md:321`). `Session.FullSeated` loses bout 3 to Brennic (CLAUDE.md). `Session.Wave1Seated` and `Session.CreaturesSeated` green; `Duel.AirSeated` green since T47. | A full-day win has never been measured (`WOLF-PLAN.md` g.14). Needs DF-011 (bout-3 loss) and DF-013 (idle steps shift bouts 2-4, OPEN) rulings, then `Automation RunTests MageArenaDesign.Session.FullSeated`. T27/T28 audio carries the missing minutes and is deferred. **P2**, **P7**. |
| 12 | "2 cold testers finish" | **not met** | none. "K3 row 12 stays 'not met'; no session is invented." (`docs/DECISIONS.md:295-302`, ruling 4). Protocol: `docs/research/PLAYTEST-PROTOCOL.md`. | Stage a Win64 build after `node apps/vr/tools/check-staged-clips.mjs` prints OK (no staging script exists, `WOLF-PLAN.md:344-346`); the owner finds two testers; the record is `docs/research/PLAYTEST-2026-10-DD.md`. Deferred, ruling 4. **P8**, card T41. |
| 13 | "APK packaged (`package-android.ps1` `MARKER_RESULT=PASS`)" | **not met** | none. "never produced" (`docs/XR-TOOLCHAIN.md:9`, cited at `WOLF-PLAN.md:261`). B3 Android target: "both `Test-Path` lines print False (Android not installed)" (`WOLF-PLAN.md:72`). **Deferred, ruling 2.** | `node apps/vr/tools/check-staged-clips.mjs`, then `powershell -NoProfile -File apps/vr/tools/package-android.ps1`; expect `MARKER_RESULT=PASS` (`WOLF-PLAN.md:261-267`). Needs the owner's B3 install and lifting ruling 2. **P5**. |
| 14 | "Build boots under the Quest 3 profile on 5.8.3" | **not met** | none on 5.8.3. Last proof "booted on 5.8.2 (2026-10-03)" (`KPI-2026-10-20.md`, `docs/XR-TOOLCHAIN.md:197`). Simulator present: `"device_profile": "Meta Quest 3"` (`WOLF-PLAN.md:105`, read only). **Deferred, ruling 2.** | The `XR_RUNTIME_JSON` boot in `WOLF-PLAN.md:253-260`; expect "Initialized OpenXR on Meta XR Simulator runtime version 207" and `MAGEVR_BOOT_OK`. Cheap and needs no Android, but ruling 2 says not now. **P5**. |
| 15 | "Owner verdict on gameplay, graphics, animation, audio (**felt**)" | **owner's** - open | none. Not for a builder to fill. | The owner plays the final Wolf build and rules on the four axes. |

## Tally (draft, no new number)

Read from the table: **met 4** (rows 4-7, each to be re-measured on the final tree), **partly met 2** (rows 1, 8), **not met 8** (rows 2, 3, 9-14), **owner's 1** (row 15). The KPI readings since 2026-10-20 stood at 4/15 (rows 5, 6, 7, 8). Since then row 4 moved from fail to pass on Wolf (`WOLF-PLAN.md:224-225`), and row 8 is carried as partly met because its evidence is the 2026-10-07 run, "not yet heard by a person". A K3 count of record is for the 30-31 Oct re-measure.

## Gap list

Not-met and partly-met rows, sorted by which can still close before 31 Oct **without an owner ruling**.

**A. Can close with Wolf work alone (no ruling needed)**

1. **Row 8** (partly): re-run `capture-colour-audio.ps1 -SkipStills` on the final tree. The "heard by a person" part is the owner's ear, but the run needs no ruling.
2. **Row 2**: T38 import and `capture-rival.ps1` (the assets exist and were accepted as working; the animated-clip verdict is the owner's felt call, row 15).
3. **Row 1** (partly): the final-tree style-pick/full-session capture after T63/T55 land. Whether it satisfies "chosen style" is a reading of ruling 1, owner's.
4. **Row 3**: needs the P15 pass tool and ceilings. The ceilings are an owner confirmation (VISUAL-PLAN section g), so this is borderline: Wolf can run it only if pof can.

**B. Blocked on an owner ruling that exists as a decision to reverse or give**

5. **Row 11**: DF-011 bout-3 loss and DF-013 (OPEN).
6. **Row 10**: DF-011 accepted red, green only by a new ruling.

**C. Deferred by explicit ruling, will stay not met unless lifted**

7. **Row 14** (ruling 2), **row 13** (ruling 2, plus the B3 owner install), **row 12** (ruling 4, plus two testers), **row 9** (ruling 6, plus a music source).

**D. Owner's**

8. **Row 15**: felt verdict.

Rows 4-7 are met and only need the 30-31 Oct re-measure on the final tree (T63, T55-T59 and any import can move them).
