# Desktop core gate rehearsal 2, VR channel (2026-10-28)

- **What this is:** the second rehearsal of the 31 Oct desktop core gate (`docs/PROJECT-PLAN.md:47`). It re-measures the
  15 K3 rows on the tree after the five gap lanes of rehearsal 1 (`GATE-REHEARSAL-VR-2026-10-27.md`) and T71, T73, T74, T75 and
  T59 landed. It is not the gate record; that is re-measured on the final tree on 30-31 Oct (WOLF-PLAN P8). **Desktop
  proxy, not Quest truth.** Measurement only: no C++, content, data, material, VFX, script, test or DECISIONS edit was made.
  A failing check is recorded as a finding, not fixed.
- **Evidence line for every Unreal figure below:** `Wolf kaziho 2026-10-28 7bdfe2c UE 5.8.3 CL 58210709`, run from a
  worktree (`autopilot/accepted-idea-delivery-0cb3bd25`, cut from local `master` at `7bdfe2c`). `Build.version` read
  `"PatchVersion": 3, "Changelist": 58210709` before the build. There was one Unreal process at a time, and `tasklist` showed none
  before each launch (the run script waited on it; every step's line is in the step log). The MetaXR plugins were copied in with
  `robocopy ... /E`. The corpus was regenerated first.
- **Evidence folder:** `docs/research/gate-rehearsal-2026-10-28/` (trimmed logs as `.txt`, 10 downscaled stills via LFS).
  The capture scripts' MP4s, frames and run output stayed in the gitignored `runs/` folder or outside the repo
  (`C:\Users\kazda\scratch-gate1028\`). `apps/vr/art/` is byte-identical to HEAD (section 1, last paragraph).
- **What changed since 10-27 (`64c1dc2..7bdfe2c`, 44 commits):** L1 audio warm-up (`b34b6c5`), L2 fire wall as flame cards
  (`aca4baf`), L3 stills cut where Brennic is in frame (`6fd17c0`) plus the seed knob (`3a948d0`..`08646c2`), T71 five
  crowd tiers (`411e43e`, `636c098`), T59 hand mesh (`535ee97`). T73 (Fire rim hue), T74 (darker steel) and T75 (bead ring)
  ran and were **reverted** after missing their blind bars (`56fa123`, `87f44de`, `73a5e28`), so Fire, steel and the
  perfect ring draw as at T67 / 0.68 on this tree. DF-014 and DF-015 were filed, both OPEN.

## 1. What ran

| Step | Command | Result | Evidence |
|---|---|---|---|
| Engine | `cat "C:\Program Files\Epic Games\UE_5.8\Engine\Build\Build.version"` | `"MajorVersion": 5, "MinorVersion": 8, "PatchVersion": 3, "Changelist": 58210709` | this run |
| Corpus | `node apps/vr/tools/clipgen/generate.mjs --corpus 30 --seed 1000`; `node apps/vr/tools/clipgen/mousepaths.mjs` | `wrote 270 corpus clips`, `5 noise clips (180.000s)`, `180 impostor clips`, `90 mouse clips`; `git status` clean | this run |
| Build | `powershell -NoProfile -ExecutionPolicy Bypass -File apps/vr/tools/build.ps1` | `Result: Succeeded`, `Total execution time: 1770.75 seconds` (cold worktree, 661 actions), exit 0 | `build.txt` |
| `MageArena.` | `UnrealEditor-Cmd ... -ExecCmds="Automation RunTests MageArena.;Quit" -unattended -nullrhi -nosplash -log -ReportExportPath=...` | **347 / 347** `Result={Success}`, 0 `Result={Fail}`, `TEST COMPLETE. EXIT CODE: 0`, process exit 0; report `succeeded=341 succeededWithWarnings=6 failed=0`. Includes 5 `MageArena.Invariants.*`, 45 `Kernel.Conformance.*`, `Hands.MeshNotSpheres`, `Hands.MeshFollowsPose`, `Capture.StylePick.SeedKnob`, `Greybox.Crowd.*`, `Overlay.Golden`, `Presentation.RivalFigure.*`, `Presentation.NoDebugText.Sources` | `mage-arena-tests.txt` |
| `MageArenaDesign.` | same, filter `MageArenaDesign.` | **5 / 7**, `EXIT CODE: -1`, process exit 255. The only reds are `Duel.FireSeated` and `Session.FullSeated` (DF-011, accepted; 2026-10-25 ruling 2). `AirSeated`, `CreaturesSeated`, `Wave1Seated`, `Census.Rivals` and `Census.Sweep` are green | `mage-arena-design-tests.txt` |
| Staged clips | `node apps/vr/tools/check-staged-clips.mjs` | `staged clips OK: 115 files, 26688122 bytes`, exit 0 | this run |
| Full session | `powershell -NoProfile -ExecutionPolicy Bypass -File apps/vr/tools/capture-style-pick.ps1` (seed 1, four styles) | `capture-style-pick: ok`, exit 0; `MAGEVR_CAPTURE_DONE shots=1870` x4; `end seq=1870 tick=1870 stage=lost sim=31.167` x4; events md5 `7B0715C871AC3399504C4F7A744896D4` x4 (= 10-27, = L3); manifests `commit=7bdfe2c...` | `style-pick-capture.txt`, `style-pick-*-sheet.png` |
| Audio, run 1 | `capture-colour-audio.ps1 -Run gate1028-audio1 -SkipStills` | **ok**, exit 0: `recording stop audio=30.805 wall=30.803 sim=30.800 mixer=30.805 cues=147 phase=active longest=0.020`, `wav duration=30.784`, ffprobe `duration=30.784000`, `max_volume: -5.7 dB` | `audio-capture.txt` |
| Audio, run 2 (straight after) | `capture-colour-audio.ps1 -Run gate1028-audio2 -SkipStills` | **ok**, exit 0: `recording stop audio=30.814 wall=30.811 sim=30.800 mixer=30.827 cues=149 phase=active longest=0.022`, `wav duration=30.827`, ffprobe `duration=30.826667`, `max_volume: -6.0 dB` | `audio-capture.txt` |
| Rival clips (row 2) | `capture-rival.ps1 -Run gate1028-rival -Staging C:\Users\kazda\scratch-gate1028\rival-stage` | `rival: ok`, exit 0; `MAGEVR_CAPTURE_DONE shots=113`; `cast set seed1-cast-a1 (5 frames)`, `hit set seed1-hit-a16 (4 frames)`, `idle set seed1-idle-a4 (4 frames)`, `death set seed2-death-a1 (8 frames)` | `rival-capture.txt`, `rival-*.png` |
| D-G4, stress scene | `capture-budget.ps1 -Runs 3 -Prefix gate1028` | `budget: ok (3 runs)`, exit 0 | `budget-dg4-stress.txt` |
| D-G4, creature-import scene | `capture-budget.ps1 -Runs 3 -Prefix gate1028-creatures -ExtraArgs -MageArenaCreatureImportScene` | `budget: ok (3 runs)`, exit 0 | `budget-dg4-creature-import.txt` |
| Win64 packaged stage | none | Still **no Win64 stage script** in `apps/vr/tools/` (`ls apps/vr/tools \| grep -i stage` gives only `check-staged-clips.mjs`). Recorded as a gap (G11), not written. | `ls apps/vr/tools` |

`capture-style-pick.ps1` rewrites the tracked seed-1 set in `apps/vr/art/style-pick/`. Its output was copied out of the
repo and restored with `git checkout HEAD -- apps/vr/art/style-pick`. Afterwards `git status --short apps/vr/art` and
`git diff --stat HEAD -- apps/vr/art` were both empty. `capture-rival.ps1` staged outside the repo and left
`Content/Characters` clean (`rival: committed cast ok`).

### D-G4 figures, by scene (three 60 s runs each)

| Scene | Draw calls median / max (limit 150 binding, 127 guide) | BasePass tris median / max (limit 350k) | Live particles max (limit 2,000) | GPU emitters max (limit 1) | Parity |
|---|---|---|---|---|---|
| D-G4 stress (default) | 103 / 108, 104 / 108, 103 / 108 | 137,846 / 139,518; 137,958 / 139,518; 137,958 / 139,518 | 1,467 / 1,468 / 1,468 | 1 / 1 / 1 | ok x3 (hash `ad0ccf03...`) |
| creature-import (`-MageArenaCreatureImportScene`) | 113 / 117, 113 / 117, 113 / 117 | 190,448 / 193,104; 191,384 / 193,104; 190,928 / 193,056 | 1,467 / 1,468 / 1,467 | 1 / 1 / 1 | ok x3 |

Against 10-27 (stress max 118, creature-import max 127, BasePass max 166,464 / 219,858): **-10 draw calls in both
scenes and about -27k BasePass triangles**. The creature scene now has 10 calls of headroom under the 127 guide, where on
10-27 it sat exactly on it. The figures reproduce T59's own after-numbers exactly (`HANDS-T59-2026-10-27.md`: stress
max 108, creature-import max 117). `handInstances` fell from 102 to 52, and T59 accounts for -2 calls. The rest of the drop
was already present on T71's base (110 / 119, "cause not diagnosed" in that report). This run does not diagnose it either.
`SK_Brennic` draws at LOD0, 23,990 tris per pass. The creature scene draws it twice, as on 10-27: the session figure plus
the scene's own import.

### Audio figures

| Run | WAV length | Expected | Peak | Integrated | Cues | `sim` / `wall` at stop | `longest=` | Result |
|---|---|---|---|---|---|---|---|---|
| 10-27 rehearsal, run 1 (`64c1dc2`) | 28.565 s | 30.8 +/- 1.0 | -5.6 dBFS | -23.8 LUFS | 134 | 28.850 / 30.814 | (not logged) | FAIL |
| 10-27 rehearsal, run 2 (`64c1dc2`) | 27.435 s | 30.8 +/- 1.0 | -5.7 dBFS | -23.9 LUFS | 128 | 27.717 / 30.802 | (not logged) | FAIL |
| **gate1028-audio1** (`7bdfe2c`) | **30.784 s** | 30.8 +/- 1.0 | -5.7 dBFS | -23.8 LUFS | 147 | **30.800** / 30.803 | **0.020 s** | **ok** |
| **gate1028-audio2** (`7bdfe2c`, straight after) | **30.827 s** | 30.8 +/- 1.0 | -6.0 dBFS | -23.8 LUFS | 149 | **30.800** / 30.811 | **0.022 s** | **ok** |

Peak and loudness are from ffmpeg `ebur128`. The L1 warm-up absorbs the first Brennic frame (`warm-up done wall=2.517
frames=22 figure=1` / `wall=2.329`), so the recorded window holds no frame over 0.022 s. That is the L1 done-when rule
("`capture-colour-audio: ok` twice in a row") holding on the final-tree candidate.

## 2. Scorecard: the gate's five axes, "at high quality"

As on 10-27, these grades are the rehearsal's reading from the runs above and my own look at the stills. The felt verdict is the
owner's (row 15). Stills are 1920x1080 frames of the seated view; the four sheets are 2x2 at 640x360 per still.

| Axis | Grade | Moved | Why (evidence) |
|---|---|---|---|
| **Gameplay** | **partly** | = | Mechanics hold: 347/347 green, 45/45 conformance, the overlay golden, Air, Wave 1 and Creatures green. The Fire duel still runs short and the player loses it (`brennic-c1.lengthS=38.32`, `brennic-c15.lengthS=37.50`, `won=0.00` both). The day still stops at bout 3 (`stage=lost won=2/4 ... day=153.97 s (2.57 min)`, `dayNaturalS=311.83` against 420-600). Every figure is byte-for-byte the 10-27 one. DF-011, owner-accepted. |
| **Graphics** | **partly** (better than 10-27, not high) | up within partly | What a player sees now: (a) **the fire wall reads as fire.** The orange cylinder slats of 10-27 are gone, and each wall is a row of soft flame cards with ember flecks (`style-pick-greybox-sheet.png`, all four stills; compare `gate-rehearsal-2026-10-27/style-pick-greybox-sheet.png`). (b) **The crowd is a packed bowl.** Five tiers of figures with raised arms, banners and some red/blue tunics fill roughly the upper third of the frame, against a thin band on 10-27 (`style-pick-greybox-sheet.png`, `style-pick-13/25/30-sheet.png`). A flat pink/lilac sky band (greybox) or flat slate sky (25, 30) still caps the top 10-20 %. (c) **Brennic is in frame in all four stills**, two of them at the frame edge (still 1 at the right edge, cut by the frame; still 2 at the left edge). (d) The flame cards closest to the seat are large: one covers the lower centre of stills 3 and 4 up to about mid-frame, and one stands over Brennic's body in still 1 (`greybox-tick481-rival-crop.png`). The wall no longer hides the rival, but it still crowds the near view. (e) Fire is T67 byte for byte (T73 reverted). In these stills the flame cards and the Fire burst (still 4) read as a pale, muted peach-orange, not a saturated fire orange; the Fire bolt core in still 3 is a flat orange disc. The T72-T73 critics' "rust" verdict was blind; I did not re-run it, so I do not re-grade it. (f) The water bolt no longer fills 40 % of the frame in any still (it was 10-27 still 2). (g) No debug text (`NoDebugText.Sources` green). Four palettes, one composition; no style chosen (G3). |
| **Models** | **partly** | up within partly | **The player's hand is a palm and fingers, not a sphere clump.** In the run's last frame (tick 1870) it is a curled fist of rounded finger segments on a palm slab, skin-toned and smooth-shaded (`greybox-tick1870-hand-crop.png`; 10-27's `greybox-tick1850-no-rival.png` showed the spheres). It is **not visible in any of the four gate stills**, which show only the cuff bars and the ward segments at the bottom. Brennic, the cinder hound and the mire maw draw from `Content/Characters` (`rival: committed cast ok`). Everything else is greybox primitives by design. No P15 ceilings and no P15 pass (row 3). |
| **Animation** | **partly** | = | All four Brennic clips play in-engine (`rival-capture.txt`; `rival-idle-seed1.png`, `rival-cast-seed1.png`, `rival-hit-seed1.png`, `rival-death-seed2.png`). In the full-session stills he is caught casting (stills 1-3: arm raised, a Fire charge at the hand) and hit (still 4: a water bolt on his chest). In `rival-cast-seed1.png`, frame 3 shows his cast pose clear of the wall; frames 2 and 4-5 are behind a Fire bolt or flame card. The death clip still ends in a dark mound that reads as a sack, not a fallen man (`rival-death-seed2.png`, frames 5-8), and the full-session capture (seed 1) never reaches it (L3 follow-up: seed 2 ends 9 ticks into Death). The hands pose per bone (`Hands.MeshFollowsPose` green; the fist at tick 1870), but no still shows a gesture. |
| **Audio** | **partly** (was not met) | up | The capture passes its own duration check **twice in a row** on this tree (section 1). Loudness is unchanged (-23.8 LUFS), and the cues fire with their positions in the cue table (147 / 149 rows). No person has heard them. No music loops (T40; 2026-10-24 "Music and audio deferred"). Voice and chorus deferred (T27, same ruling). |

## 3. The 15 K3 rows

States: **met** / **partly** / **not met** / **owner's**. "Moved" compares with the 2026-10-27 rehearsal. Every Wolf line is
`Wolf kaziho 2026-10-28 7bdfe2c UE 5.8.3 CL 58210709` unless it says otherwise. A row that was not re-run because a
ruling defers it says `= (not re-run, ruling ...)`; the ruling names are `docs/DECISIONS.md` headings.

| # | Row (K3) | State | Moved | Wolf evidence (command, exit, marker) |
|---|---|---|---|---|
| 1 | The full session in the chosen style | **partly** | = | `capture-style-pick.ps1`, exit 0, `MAGEVR_CAPTURE_DONE shots=1870` x4, `capture-style-pick: ok`, events md5 `7B0715C8...` x4. Brennic in frame in 4 of 4 stills (10-27: 1 of 4): tick 481 event 52 cast `rivalbar=379`; tick 931 event 102 cast `253`; tick 1399 event 151 cast `171`; tick 1477 event 166 hit `374`. The content moved up (fire wall, crowd, rival), but no style is chosen (2026-10-24 "P4 style: no single style is locked"), so the state holds. |
| 2 | Opponent animated (idle, cast, hit, death; R0) | **partly** | = | `capture-rival.ps1`, exit 0, `MAGEVR_CAPTURE_DONE shots=113`, `rival: ok`, four clip sets (`seed2-death-a1 (8 frames)`); `MAGEVR_RIVAL_FIGURE loaded ... lod0tris=23990 lods=3 ... idle=2.000 cast=1.533 hit=0.800 death=2.000 s`; `Presentation.RivalFigure.ClipFollowsEvents` and `.FallsBackToGreybox` green. Death is not in the full-session capture. The R0 animation verdict is the owner's. |
| 3 | Every asset under P15 ceilings | **not met** | = | Re-checked: no P15 tool in `apps/vr/tools/` (`grep -rli P15 apps/vr/tools` empty), no ceilings confirmed (G10). `SK_Brennic` 23,990 tris LOD0 (budget log). |
| 4 | Draw calls <=150 | **met** | = | `capture-budget.ps1 -Runs 3`, exit 0, `budget: ok (3 runs)` in both scenes: stress max 108 x3, creature-import max 117 x3 (10-27: 118 / 127). Under the 127 guide in both. |
| 5 | Visible triangles <=350k (BasePass) | **met** | = | stress BasePass max 139,518; creature-import max 193,104 (10-27: 166,464 / 219,858). |
| 6 | Live particles <=2,000 | **met** | = | max 1,468 in both scenes. |
| 7 | <=1 GPU emitter | **met** | = | `gpu emitters max: 1 (cross-check 1)` in all six runs. |
| 8 | Threat cues spatialized | **partly** | up (the evidence; the state holds) | `capture-colour-audio.ps1 -SkipStills` twice in a row, **exit 0 both**, `capture-colour-audio: ok` x2; `recording stop ... sim=30.800 ... longest=0.020` / `longest=0.022`; WAV 30.784 / 30.827 s of 30.8 +/- 1.0; -23.8 LUFS. 10-27 failed both runs. Partly because no person has heard the cues. |
| 9 | Two music loops | **not met** | = (not re-run, ruling 2026-10-24 "Music and audio deferred") | T40 card only (`apps/vr/tasks/T40-two-music-loops.md`). |
| 10 | Fire duel median 45-80 s | **not met** (accepted red) | = | `MageArenaDesign.Duel.FireSeated` `Result={Fail}`: `brennic-c1.lengthS=38.32`, `brennic-c15.lengthS=37.50` (target 45.00-80.00), `won=0.00` both; `Expected 'brennic-c15 reached the last phase (1 of 2 breaks)' to be true`. DF-011, kept 2026-10-25 ruling 2. |
| 11 | The full Tiro day in 7-10 min | **not met** (accepted red, measured) | = | `Session.FullSeated` `Result={Fail}`: `stage=lost won=2/4 fights=109.38 s day=153.97 s (2.57 min) with teach=225.83 s (3.76 min)`, `dayWithTeachS=225.83`, `dayNaturalS=311.83` (target 420.00-600.00). Air, Wave 1 and Creatures green (`victoryS=31.02`, `39.38` inside their windows). |
| 12 | 2 cold testers finish | **not met** | = (not re-run, ruling 2026-10-24 "T41 cold-tester sessions deferred") | Still no Win64 stage script (section 1). |
| 13 | APK packaged | **not met** | = (not re-run, ruling 2026-10-24 "P5/P9 deferred") | Only `MageArenaVR.Target.cs` and `MageArenaVREditor.Target.cs` under `apps/vr/Game/Source/`; the Android target (B3) is the owner's. |
| 14 | Boots under the Quest 3 profile on 5.8.3 | **not met** | = (not re-run, ruling 2026-10-24 "P5/P9 deferred") | none. |
| 15 | Owner verdict on gameplay, graphics, animation, audio (felt) | **owner's** | = | Section 2 is the rehearsal's reading for the owner, not the verdict. |

**Tally:** met 4 (rows 4-7), partly 3 (rows 1, 2, 8), not met 7 (rows 3, 9-14), owner's 1 (row 15), which is the same count
as 10-27. No row changed state. Row 8 moved up in its evidence: the capture that failed twice on 10-27 passes twice. Rows 1, 4
and 5 moved inside their states (rival in 4 of 4 stills; 10 draw calls more headroom). Two scorecard axes improved: Audio
from not met to partly, and Graphics and Models within partly.

## 4. Gap list

Severity as on 10-27. **Owner-free** = a lane can close it without a ruling; **owner-call** = it waits on the owner.
The owner-calls already pending are **G2, G3, B1, B2, M5, DF-014, DF-015 and G10**. They are referenced here, not
re-argued (options and recommendations: `GATE-REHEARSAL-VR-2026-10-27.md` section 4 for G2, G3, G10;
`docs/design-findings/DF-014-fire-wall-colour-vs-absorb.md`; `docs/design-findings/DF-015-style-steels-near-black-core.md`;
`docs/concepts/WOLF-PLAN.md` section b for B1/B2; `docs/research/MOONSHOT-2026-10.md` for M5).

### Closed since 10-27 (re-measured here)

- **G1 audio capture duration: closed** (L1, `b34b6c5`). ok x2 on this tree (section 1).
- **G5 rival out of view: closed for the stills** (L3). He is in frame in 4 of 4. His Death is still not in the full-session
  capture (see G13).
- **G7 thin crowd: closed** (T71). A packed five-tier bowl in all four styles.
- **G9 sphere hands: closed** (T59). Palm and fingers (`greybox-tick1870-hand-crop.png`). See G14 for the stills.

### Still open

| Gap | Rows | Severity | Owner-free / owner-call | State on this tree |
|---|---|---|---|---|
| **G2** music, testers, APK, Quest 3 boot deferred | 9, 12, 13, 14 | blocker | **owner-call (pending)** | unchanged; not re-run. |
| **G3** no chosen style | 1 | blocker | **owner-call (pending)** | unchanged; four sheets in this folder. |
| **G4** fire wall crowds the near view | 1, 15 | major (was: hides the rival) | **owner-free** for the shape and placement (presentation only); the colour is **DF-014 (owner-call, pending)** | L2 turned the pillars into flame cards; the near cards still cover the lower centre of stills 3-4 and stand over Brennic in still 1 (`greybox-tick481-rival-crop.png`). |
| **G6** Fire reads muted / rust | 1, 15 | major | **owner-call**: ruling 3's lane (T73) ran and missed its "never rust" bar, and Fire is back at T67 (`FIRE-RIM-HUE-T73-2026-10-27.md`). A retry or a changed bar is the owner's. | Flame cards and the burst read pale peach-orange here (`style-pick-greybox-sheet.png` still 4). |
| **G8** steel off-white, bead ring as rope | 1, 15 | minor | **owner-call**: T74 and T75 ran and reverted; DF-015 (pending) | not in frame in this run's stills. |
| **G10** no P15 pass, no ceilings | 3 | blocker for row 3 | **owner-call (pending)** | unchanged. |
| **G11** no Win64 stage script | 12 (P8 box 1) | minor until testers exist | **owner-free**, but waits on the T41 ruling | unchanged. |
| **G12** accepted design reds (DF-011) | 10, 11 | owner-ruled | owner-ruled (2026-10-25 ruling 2), nothing to do | byte-identical figures. |
| **G13** Brennic's Death not in the full-session capture; death pose reads as a sack | 2, 15 | minor | **owner-free** (capture driver: run past the intermission start, in-view test without the nameplate; `L3-SESSION-CAPTURE-2026-10-27.md` "Seed 2 follow-up"). The pose itself is an asset/animation question for R0 (owner's verdict). | `rival-death-seed2.png` frames 5-8. Waiting by the dispatch (`rival-death-seed2.png` already proves the clip plays). |
| **G14** the new hands are in none of the four gate stills | 1, 15 | minor | **owner-free** (capture framing only) | only the run's last frame shows a hand (`greybox-tick1870-hand-crop.png`). |
| **G15** the first Brennic frame still builds `SK_Brennic` in-game (about 2 s) | 15 | minor | **owner-free** (re-save the asset with its LODs, or load the figure at spawn; `AUDIO-CAPTURE-DURATION-2026-10-27.md` section 5) | not measured in a session here. The warm-up absorbs it in the capture (`warm-up done wall=2.517 frames=22 figure=1`). |

B1 (engine 5.8.3) read correctly today. B2 (launcher auto-update off) and M5 have no gate row and are only listed as pending.

## 5. WOLF-PLAN P8

No P8 box is ticked: the gate record box is ticked on 31 Oct. One rehearsal evidence line was added under P8 beside the
10-27 one.
