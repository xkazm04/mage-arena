# Desktop core gate rehearsal, VR channel (2026-10-27)

- **What this is:** a rehearsal of the 31 Oct desktop core gate (`docs/PROJECT-PLAN.md:47`), measured on Wolf, four days
  early. It is not the gate record; the record is re-measured on the final tree on 30-31 Oct (WOLF-PLAN P8). Measurement
  only: no C++, content, data, config or DECISIONS edit was made. Nothing is fixed here; each gap names the lane that would.
- **Evidence line for every Unreal figure below:** `Wolf kaziho 2026-10-27 64c1dc2 UE 5.8.3 CL 58210709`, run from a
  worktree (`autopilot/accepted-idea-delivery-28764f24`, cut from local `master` at `64c1dc2`, T72c). One Unreal process at
  a time; `tasklist` showed none before each launch. MetaXR plugins copied in with `robocopy ... /E`.
- **Evidence folder:** `docs/research/gate-rehearsal-2026-10-27/` (trimmed logs as `.txt`, 10 downscaled stills via LFS).
- **Input:** `docs/research/GATE-RECORD-DRAFT-2026-10-25.md` (the 15 K3 rows) and `docs/research/GATE-CAPTURES-2026-10-25.md`.
- **The 10-27 owner rulings** named in the dispatch (T71 `stands.tiers` 3->5 option C, a second Fire rim hue, a darker
  steel stone, the bead ring) are **not yet in `docs/DECISIONS.md`** at `64c1dc2`. They are read from the dispatch brief
  only, and section 4 marks which rows they would move.

## 1. What ran (steps 1-3)

| Step | Command | Result | Evidence |
|---|---|---|---|
| Corpus | `node apps/vr/tools/clipgen/generate.mjs --corpus 30 --seed 1000`; `node apps/vr/tools/clipgen/mousepaths.mjs` | 270 corpus, 5 noise, 180 impostor, 90 mouse clips; `git status` clean | this run |
| Build | `powershell -NoProfile -ExecutionPolicy Bypass -File apps/vr/tools/build.ps1` | `Result: Succeeded`, exit 0, 1166 s (cold worktree) | `build.txt` |
| `MageArena.` | `UnrealEditor-Cmd ... -ExecCmds="Automation RunTests MageArena.;Quit" -unattended -nullrhi` | **342 / 342**, 0 fail, `TEST COMPLETE. EXIT CODE: 0`; includes `MageArena.Invariants.*`, `Kernel.Conformance`, `Presentation.RivalFigure.*`, `Presentation.NoDebugText.Sources` | `mage-arena-tests.txt` |
| `MageArenaDesign.` | same, filter `MageArenaDesign.` | **5 / 7**; reds `Duel.FireSeated`, `Session.FullSeated` only (DF-011, accepted; ruling 2 of 2026-10-25 keeps them). No other red. | `mage-arena-design-tests.txt` |
| Staged clips | `node apps/vr/tools/check-staged-clips.mjs` | `staged clips OK: 115 files, 26688122 bytes`, exit 0 | this run |
| Full session | `powershell -NoProfile -File apps/vr/tools/capture-style-pick.ps1` | `capture-style-pick: ok`, exit 0; `MAGEVR_CAPTURE_DONE shots=1870` x4; events md5 `7b0715c8...` x4 (= T16 and 2026-10-25); 4 MP4s, 16 stills | `style-pick-capture.txt`, `style-pick-*-sheet.png` |
| D-G4, stress scene | `capture-budget.ps1 -Runs 3 -Prefix rehearsal-1027` | `budget: ok (3 runs)`, exit 0 | `budget-dg4-stress.txt` |
| D-G4, creature-import scene | `capture-budget.ps1 -Runs 3 -Prefix rehearsal-1027-creatures -ExtraArgs -MageArenaCreatureImportScene` | `budget: ok (3 runs)`, exit 0 | `budget-dg4-creature-import.txt` |
| Audio | `capture-colour-audio.ps1 -Run rehearsal-1027-audio -SkipStills`, re-run `-Run rehearsal-1027-audio2` | **FAIL both times**, exit 1: `MAGEVR_CAPTURE_FAIL wav duration 28.565s` / `27.435s expected 30.800s within 1.0s` | `audio-capture.txt` |
| Rival clips (extra, for row 2) | `capture-rival.ps1 -Run rehearsal-1027-rival -Staging <worktree>\runs\rehearsal-1027-rival-stage` | `rival: ok`, exit 0; idle, cast, hit and death sets all present (death on seed 2) | `rival-capture.txt`, `rival-*.png` |
| Win64 packaged stage | none | **No Win64 stage script exists** in `apps/vr/tools/` (only `package-android.ps1`, Android, deferred by ruling 2). Recorded as a gap, not written. | `ls apps/vr/tools` |

The capture scripts write into tracked paths (`apps/vr/art/style-pick/`: 20 files). They were copied to `runs/` and
restored with `git checkout HEAD -- apps/vr/art/style-pick`; `git status` was clean afterwards.

### D-G4 figures, by scene (three 60 s runs each)

| Scene | Draw calls median / max (limit 150 binding, 127 guide) | BasePass tris median / max (limit 350k) | Live particles max (limit 2,000) | GPU emitters max (limit 1) | Parity |
|---|---|---|---|---|---|
| D-G4 stress (default) | 114 / 118, 114 / 118, 113 / 118 | 164,160 / 166,464; 163,896 / 165,456; 163,328 / 164,208 | 1,468 / 1,467 / 1,467 | 1 / 1 / 1 | ok x3 |
| creature-import (`-MageArenaCreatureImportScene`) | 123 / 127, 124 / 127, 124 / 127 | 217,410 / 219,858; 216,386 / 218,994; 216,386 / 219,042 | 1,468 / 1,467 / 1,467 | 1 / 1 / 1 | ok x3 |

All under the binding limits in both scenes. The creature-import scene sits **exactly at the 127 guide** (T65's ceiling),
with 23 calls of headroom to 150. `SK_Brennic` draws at LOD0, 23,990 tris per pass. The live-particle row is the budget
script's count. T72c's table also added 400 "puffs"; with them the creature scene would be 1,868, still under 2,000.

### Audio figures

| Run | WAV length | Expected | Peak | Integrated | Cues | `sim` / `wall` at stop |
|---|---|---|---|---|---|---|
| 2026-10-25 (`e9d8abc`, reference) | 30.677 s | 30.8 +/- 1.0 | -5.9 dBFS | -23.8 LUFS | 142 | 30.800 / 30.800 |
| rehearsal-1027-audio | **28.565 s** | 30.8 +/- 1.0 | -5.6 dBFS | -23.8 LUFS | 134 | **28.850** / 30.814 |
| rehearsal-1027-audio2 | **27.435 s** | 30.8 +/- 1.0 | -5.7 dBFS | -23.9 LUFS | 128 | **27.717** / 30.802 |

Loudness and peak are unchanged (ffmpeg `ebur128`). The WAV follows `sim`, and `sim` fell 2-3 s behind `wall` in the 30.8 s
window. The game thread ran slower than real time during the recording. The host was idle (GPU 1 %, no other Unreal,
Blender or ComfyUI process). The cause was not diagnosed: that needs code reading or a probe, which is a later lane. I
stopped after two failing runs (the three-runs rule).

## 2. Scorecard: the gate's five axes, "at high quality"

The gate's own wording asks for the full session in the chosen style, under budget, with spatialized threat cues, music,
a Fire duel of 45-80 s, testers, an APK, and the owner's felt verdict. The five axes below are read against "high
quality", from the runs above and my own look at the stills. Only the owner can give the felt verdict (row 15). These
grades are the rehearsal's reading.

| Axis | Grade | Why (evidence) |
|---|---|---|
| **Gameplay** | **partly** | Mechanics are solid: 342/342 green, conformance and goldens in, Air duel, Wave 1 and Creatures green. The Fire duel still runs short and the player loses it (`brennic-c1 lengthS=38.32`, `brennic-c15 lengthS=37.50`, both `won=0`). The full day stops at bout 3 (`stage=lost won=2/4 ... day=153.97 s (2.57 min)`, `dayNaturalS=311.83` against 420-600). Owner-accepted (DF-011), but it is not "high quality" pacing against the plan's 7-10 min day. |
| **Graphics** | **partly, not high** | The arena, crowd, braziers and dusk read; no debug text. A player would see: (a) the fire wall (the Fire school's defence) as **flat translucent orange cylinders** filling a third of the view and hiding him (`style-pick-greybox-sheet.png` stills 1, 3, 4; `greybox-tick1399-rival-crop.png`); (b) the rival **absent from view** in stills 1, 2 and 4 and in the closing seconds (`greybox-tick1850-no-rival.png`); (c) a water bolt filling 40 % of the frame at impact (still 2); (d) Fire bursts reading **muddy brown-orange (rust)** in the rival cast frames (`rival-cast-seed1.png`, frames 2-3), the T72/T72b/T72c finding unchanged; (e) the crowd fills a thin band (about 15-20 % of frame height) under a large flat pink sky band, against key art S1's packed bowl (T71). Four palettes, one composition; no style is chosen (ruling 1). |
| **Models** | **partly** | Brennic, the cinder hound and the mire maw are committed under `Content/Characters` (`ad38b9b`, T70 maw `2e1580f`) and draw (`rival: committed cast ok`). The player's hands are still **a clump of beige spheres** (`greybox-tick1850-no-rival.png`, bottom; T59 open). Every other in-play piece is greybox primitives by design (current phase). There are no P15 ceilings and no P15 pass (row 3). |
| **Animation** | **partly** | All four Brennic clips play in-engine (`rival-capture.txt`: idle, cast, hit, death; `rival-death-seed2.png`). In the full-session capture (seed 1) Brennic wins, so **the player never sees his death**. His cast frames are mostly hidden behind the fire-wall slats and the spell VFX (`rival-cast-seed1.png`). The death clip ends in a dark mound that reads as a sack rather than a fallen man at this framing. The player hands have no animation beyond the gesture pose (T59). |
| **Audio** | **not met** | The audio capture **fails its own duration check twice** on this tree (section 1). The 20 imported SFX cues are unchanged in loudness (-23.8 LUFS), but no person has heard them. Two music loops: none (T40, ruling 6). Voice and chorus audio: deferred (T27, ruling 6). |

## 3. The 15 K3 rows

States: **met** / **partly** / **not met** / **owner's**. "Moved" compares with the 2026-10-25 draft. Every Wolf line is
`Wolf kaziho 2026-10-27 64c1dc2 UE 5.8.3 CL 58210709` unless it says otherwise.

| # | Row (K3) | State | Moved | Wolf evidence (command, exit, marker) | 10-27 ruling that would move it |
|---|---|---|---|---|---|
| 1 | The full session in the chosen style | **partly** | = | `capture-style-pick.ps1`, exit 0, `MAGEVR_CAPTURE_DONE shots=1870` x4, `capture-style-pick: ok`, events md5 `7b0715c8...` x4. No chosen style (ruling 1). Rival unreadable in 3 of 4 stills. | T71 option C (crowd density), Fire rim hue, darker steel, bead ring: all four change what this capture shows. None changes the "chosen style" question. |
| 2 | Opponent animated (idle, cast, hit, death; R0) | **partly** (was not met) | up | `capture-rival.ps1`, exit 0, `MAGEVR_CAPTURE_DONE shots=113`, `rival: ok`, four clip sets; `MageArena.Presentation.RivalFigure.ClipFollowsEvents` and `.FallsBackToGreybox` green; assets committed (`ad38b9b`). The R0 animation verdict is the owner's. | none |
| 3 | Every asset under P15 ceilings | **not met** | = | No P15 tool in `apps/vr/tools/`, and no ceilings confirmed (VISUAL-PLAN section g). `SK_Brennic` 23,990 tris LOD0 (budget log). | none |
| 4 | Draw calls <=150 | **met** | = | `capture-budget.ps1 -Runs 3`, exit 0, `budget: ok (3 runs)`: stress max 118 x3; creature-import max 127 x3 (at the 127 guide). | T71 option C (taller stands) may add calls if the tiers are new geometry. The crowd stays one batch, so the expected cost is the stand meshes only. Re-measure after it. |
| 5 | Visible triangles <=350k (BasePass) | **met** | = | stress BasePass max 166,464; creature-import max 219,858. | T71 option C adds crowd instances and stand geometry (BasePass rises, far from 350k). |
| 6 | Live particles <=2,000 | **met** | = | max 1,468 in both scenes (+400 puffs = 1,868 creature scene). | none (a Fire rim hue is a colour, not a count) |
| 7 | <=1 GPU emitter | **met** | = | `gpu emitters max: 1 (cross-check 1)` in all six runs. | none |
| 8 | Threat cues spatialized | **partly** (at risk) | down | `capture-colour-audio.ps1 -SkipStills`, **exit 1 twice**, `MAGEVR_CAPTURE_FAIL wav duration 28.565s` / `27.435s`; loudness -23.8 / -23.9 LUFS, peak -5.6 / -5.7 dBFS. Cues fire (134 / 128) but the recording check fails, and nobody has heard them. | none |
| 9 | Two music loops | **not met** | = | none (T40, ruling 6). | none |
| 10 | Fire duel median 45-80 s | **not met** (accepted red) | = | `MageArenaDesign.Duel.FireSeated` Fail: `brennic-c1.lengthS=38.32`, `brennic-c15.lengthS=37.50`, `brennic-c15 reached the last phase (1 of 2 breaks)` false. DF-011, kept 2026-10-25 ruling 2. | none |
| 11 | The full Tiro day in 7-10 min | **not met** (accepted red, measured) | = | `Session.FullSeated` Fail: `stage=lost won=2/4`, `dayWithTeachS=225.83`, `dayNaturalS=311.83` (target 420-600). Air, Wave 1 and Creatures green (`victoryS` 31.02 and 39.38 inside their windows). | none |
| 12 | 2 cold testers finish | **not met** | = | none (ruling 4). No Win64 stage script exists. | none |
| 13 | APK packaged | **not met** | = | not run (ruling 2). `UnrealGame.target` for Android is still absent (B3). UBT validates the Android SDK (`##PlatformValidate: Android VALID r27c`), but that is not the target. | none |
| 14 | Boots under the Quest 3 profile on 5.8.3 | **not met** | = | not run (ruling 2). | none |
| 15 | Owner verdict on gameplay, graphics, animation, audio (felt) | **owner's** | = | Section 2 is the rehearsal's reading for the owner, not the verdict. | all four rulings feed the graphics verdict |

**Tally:** met 4 (rows 4-7), partly 3 (rows 1, 2, 8), not met 7 (rows 3, 9-14), owner's 1 (row 15). Against the draft:
row 2 up (not met to partly), and row 8 held at partly but now with a failing capture instead of a passing one.

## 4. Gap list, ranked by severity

Severity: **blocker** = the gate row cannot pass as things stand; **major** = a player would see it on the first session,
or a gate measurement is broken; **minor** = visible but not first-impression. **Owner-free** gaps name the smallest single
Unreal lane, in the recommended order (section 5). **Owner-call** gaps give options and a recommendation.

### Blockers

**G1. Audio capture fails its duration check (row 8).** Owner-free.
Evidence: `audio-capture.txt`: `MAGEVR_T26 recording stop audio=30.816 wall=30.814 sim=28.850` then
`MAGEVR_CAPTURE_FAIL wav duration 28.565s expected 30.800s within 1.0s`. The re-run gave 27.435 s. On 2026-10-25 (`e9d8abc`)
`sim=30.800`. Something between `e9d8abc` and `64c1dc2` (T63-T72c: crowd tint, flipbook glow, spell spectacle, committed
characters) makes the game thread fall behind real time in the audio scene. Or the WAV is cut to sim time where it should
follow audio time. Without the capture, row 8 has no passing evidence on the final tree.
Lane: **L1, audio-capture duration** - bisect `e9d8abc..64c1dc2` with the one command, then fix the slow frame or the
recorder's clock. Done when `capture-colour-audio.ps1 -SkipStills` prints `capture-colour-audio: ok` twice in a row.

**G2. Rows 9, 12, 13, 14 are deferred by ruling (music, testers, APK, Quest 3 boot).** Owner-call.
They cannot pass on 31 Oct without lifting rulings 2, 4 and 6.
Options: (a) keep all deferred and record them as not met on 31 Oct; (b) lift ruling 2 for row 14 only (the simulator
boot needs no Android install, `WOLF-PLAN.md:253-260`, about half a day); (c) lift ruling 2 for rows 13-14 (needs the
owner's B3 Android target install first); (d) lift ruling 6 for T40 with a music source.
**Recommendation: (b).** Row 14 is the cheapest of the four, and the device purchase that the gate releases depends on it
most.

**G3. No chosen style (row 1).** Owner-call.
Ruling 1 keeps greybox as the base; the gate says "in the chosen style".
Options: (a) the owner rules that "greybox base with the art-concept pass" satisfies row 1; (b) the owner picks 13, 25 or
30 from this run's sheets (`style-pick-13/25/30-sheet.png`); (c) row 1 stays partly on 31 Oct.
**Recommendation: (a),** stated in DECISIONS, so the record can count row 1 on the capture alone.

### Major

**G4. The rival is hidden behind the fire wall.** Owner-free (presentation only; no signed number).
Evidence: `greybox-tick1399-rival-crop.png`; `style-pick-greybox-sheet.png` stills 1, 3, 4; `rival-cast-seed1.png`.
`ASessionPresentation::SyncWalls` (`Session/SessionPresentation.cpp:1106-1135`) draws eight `Cylinder` slats per wall in
the Fire colour at alpha 0.88. To a player this reads as **flat orange pillars** that block a third of the view and most of
Brennic's cast, not as a wall of fire. It also takes Fire's threat colour (element colour = absorb) for a defence. That
part touches the colour language, so check it against `docs/gameplay/04-threat-language.md` in the lane.
Lane: **L2, fire-wall read** - slats on the flipbook glow layer (T63/T67 cells), lower opacity toward the player, and
`capture-style-pick.ps1` still 3 showing Brennic. Re-check D-G4 (the walls are in the session scene, not the stress scene).

**G5. The rival is out of view for most of the full-session capture.** Owner-free.
Evidence: no Brennic in stills 1, 2 and 4 nor at tick 1850 (`greybox-tick1850-no-rival.png`), in all four palettes. The
capture's seed 1 ends with the player's death (Brennic wins, DF-011), so the stills never show his death clip. That clip
exists and plays (`rival-death-seed2.png`).
Lane: **L3, session capture framing** (capture tool only) - add a seed-2 pass to `capture-style-pick.ps1` (where Brennic
goes down), or cut the stills where the rival is in frame, so the gate's row-1 capture shows the animated opponent. No
gameplay change.

**G6. Fire reads rust (graphics).** Owner-call (10-27 ruling: a second Fire rim hue).
Evidence: `rival-cast-seed1.png` frames 2-3 (brown-orange bursts); T72, T72b and T72c named rust in every round, and the
cells were reverted to T67 (`docs/research/FIRE-VALUE-T72C-2026-10-26.md`).
Options (from FIRE-VALUE-T72 option C): (a) a 4th custom float carrying a deeper red-orange rim; (b) a material rim term,
no custom-data change; (c) leave Fire at T67.
**Recommendation: (b),** if the 10-27 ruling allows a rim hue outside the element colour. It keeps the custom-data shape
and the one call. **Moves row 1 and row 15 (graphics).**

### Minor

**G7. The crowd is thin against S1 (graphics).** Owner-call, already ruled 10-27 (T71 `stands.tiers` 3->5, option C).
Evidence: `style-pick-*-sheet.png`: the stands are a band of about 15-20 % of frame height under a flat pink sky. T71 is
blocked on support geometry (`apps/vr/tasks/T71-crowd-density.md:3`).
Lane: **L4, T71 option C** once the ruling is in DECISIONS (it edits `arena-layout.json`, so the overlay golden must be
re-recorded). **Moves rows 1, 4, 5** (re-measure D-G4: creature scene already at 127).

**G8. Steel stone reads off-white; the bead ring reads as rope.** Owner-call, ruled 10-27 (darker steel, a bead ring change).
Evidence: `docs/concepts/VISUAL-PLAN.md:464-466`; every T72/T72b critic. Not visible in this run's four stills (no steel
throw or perfect window in frame).
Lane: **L5**, one colour-and-shape lane after L4 (the palette and the bead mesh; the `Greybox.*NotThreat` tests guard it).
**Moves rows 1 and 15.**

**G9. Player hands are sphere clumps.** Owner-free (T59, open).
Evidence: `greybox-tick1850-no-rival.png`, bottom. Lane: **T59** player hand mesh. Moves rows 1, 15 (animation, models).

**G10. No P15 pass and no ceilings (row 3).** Owner-call (the ceilings) then pof work (plan section 8: build it in pof
first, not here). Options: (a) the owner confirms VISUAL-PLAN section g ceilings and a pof P15 card follows; (b) row 3 is
recorded not met on 31 Oct. **Recommendation: (a)** for the ceilings now. The pass itself can land after the gate.

**G11. No Win64 stage script (row 12, P8 box 1).** Owner-free, but not an Unreal lane until testers exist (ruling 4).
Lane: a `stage-win64.ps1` card (BuildCookRun Win64, after `check-staged-clips.mjs`), when ruling 4 is lifted.

**G12. Accepted design reds (rows 10, 11).** Owner-ruled (DF-011 kept 2026-10-25). Nothing to do unless the owner reopens
DF-011. The Fire duel at 38 s and the day stopping at bout 3 are what the owner will feel in row 15.

## 5. Recommended order of owner-free Unreal lanes

One Unreal lane at a time. The queued VR ruling lanes (T71, rim hue, steel, bead ring) follow L1-L3:

1. **L1** audio-capture duration (G1): restores row 8's evidence, so it goes first.
2. **L2** fire-wall read (G4): the largest first-impression problem in every still.
3. **L3** session capture framing (G5): tool-only, makes row 1's capture show the rival and his clips.
4. Then the ruled lanes in the dispatch order: **T71 option C** (G7), the **Fire rim hue** (G6), **steel and bead ring**
   (G8). Re-run `capture-budget.ps1 -Runs 3` in both scenes after each one: the creature scene is at the 127 guide.
5. **T59** hands (G9) if a slot is left before 31 Oct.

## 6. WOLF-PLAN P8

No P8 box is ticked. Box 1 (Win64 stage): no script exists (G11). Box 2: owner, deferred (ruling 4). Box 3 (the gate
record): this rehearsal is not the record. It records each row with Wolf evidence, but the record is the 30-31 Oct
re-measure on the final tree. A rehearsal evidence line was added under P8.
