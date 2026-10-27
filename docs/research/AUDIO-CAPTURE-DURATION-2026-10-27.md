# Audio capture duration (L1, gap G1 of the 10-27 gate rehearsal)

- **Question:** why `capture-colour-audio.ps1 -SkipStills` fails its own WAV duration check on the rehearsal tree
  (`MAGEVR_CAPTURE_FAIL wav duration 28.565s` / `27.435s expected 30.800s within 1.0s`,
  `docs/research/gate-rehearsal-2026-10-27/audio-capture.txt`), and what the smallest root-cause fix is.
- **Answer:** a one-time ~1.9 s game-thread frame inside the recorded window. The first frame that draws the
  Brennic semifinal builds his committed figure (`SK_Brennic`: mesh build plus two LOD reductions in the editor). The
  recorder was right to follow the mixer: the frame was wrong to be in the window. The fix is a warm-up Start of the same
  duel before the recorded Start (`b34b6c5`). The tolerance (1.0 s), the window (30.8 s), the bout, the seed and the
  check are unchanged.
- **Evidence line:** `Wolf kaziho 2026-10-27 b34b6c5 UE 5.8.3 CL 58210709`, worktree
  `autopilot/accepted-idea-delivery-ec8e16ab` (cut from local `master` at `d4782df`). `Build.version` read 5.8.3 /
  58210709 before the build. One Unreal process at a time; `tasklist` checked before each launch. MetaXR plugins copied in
  with `robocopy ... /E`. Key line: `MAGEVR_T26 recording stop audio=30.808 wall=30.807 sim=30.800 mixer=30.805 cues=149
  phase=active longest=0.020`, then `wav duration=30.805`, `capture-colour-audio: ok`.

## 1. Cause

Reproduced once on the worktree tip before any change (run `l1-repro`, tree `d4782df`):

```
[10.42.16:220][ 30] MageArena.Session.Start seed=1 scripted=1 ok=1
[10.42.16:220][ 30] MAGEVR_T26 recording start brennic=2 audioclock=18.901
[10.42.16:364][ 31] LogSkeletalMesh: Building Skeletal Mesh SK_Brennic...
[10.42.16:800][ 31] LogSkeletalMeshReduction: Reducing skeletal mesh for LOD 1
[10.42.16:800][ 31] LogSkeletalMeshReduction: Reducing skeletal mesh for LOD 2
[10.42.18:144][ 31] LogSkeletalMesh: ... SK_Brennic: The derived data key is different after the build. Save the asset to avoid rebuilding it.
[10.42.18:150][ 31] LogSkeletalMesh: Built Skeletal Mesh [1.79s] /Game/Characters/Brennic/SK_Brennic.SK_Brennic
[10.42.18:154][ 31] MAGEVR_RIVAL_FIGURE loaded /Game/Characters/Brennic/SK_Brennic.SK_Brennic lod0tris=23990 lods=3 ...
[10.42.47:027][761] MAGEVR_T26 recording stop audio=30.804 wall=30.802 sim=29.083 mixer=30.805 cues=134 phase=active
[10.42.47:563][794] MAGEVR_T26 wav duration=28.800 ... MAGEVR_CAPTURE_FAIL wav duration 28.800s expected 30.800s within 1.0s
```

Frame 31, the first frame after the recorded `Start`, runs from 16.220 to 18.154: **1.93 s in one frame**.
`ASessionPresentation::UsesRivalFigure` loads the figure on the first duel frame that needs it, and
`CreatureImport::PrepareMesh` (editor-only) merges sections and generates two LODs, then the mesh rebuilds. That one frame
explains both symptoms:

- **sim behind wall.** `FArenaSession::Advance` clamps a frame to `MaxFrameDeltaS` (0.25 s) and at most 8 steps, so
  the 1.93 s frame gave the kernel 0.25 s. The sim lost about 1.7 s (29.083 against 30.802 wall).
- **WAV shorter than the mixer.** No source plays during the stall (the first cue, `arena-crowd-loop`, has
  `mixer_s=1.963`), so the master submix goes silent and auto-disables after its `AutoDisableTime`. In UE 5.8,
  `FMixerSubmix::ProcessAudio` returns early on a disabled submix *before* it appends to `RecordingData`
  (`AudioMixerSubmix.cpp` 1410-1449 against the append at 1686-1692). The mixer clock still advances
  (`FMixerDevice::UpdateAudioClock` runs per rendered buffer, so `mixer=30.805`), but the recording skips those buffers.
  So the WAV follows neither sim nor wall: it drops the silent stretch, and the stretch is the stall.

Why it started after `e9d8abc` (10-25, sim 30.800, WAV 30.677 s): there, the figure loaded from the scratch folder
`/Game/CreatureImportScratch`, which a clean tree does not have, so the presentation drew the greybox body and nothing
was built. `ad38b9b` committed the cast under `Content/Characters` and `16ffa56` pointed the runtime at
`/Game/Characters`, so from then on every fresh audio run builds `SK_Brennic` in its first recorded frame. The stall
length varies with the host (1.61 to 2.13 s across the five runs here). That fits the rehearsal's two different
shortfalls (2.2 s and 3.4 s).

**No bisect was run.** The probe pins the frame, its cause and the commit that brought the asset into the path, so a
bisect over 57 commits would add no information. The other suspects (T63-T72c: crowd tint, flipbook glow, spell
spectacle) add no frame anywhere near 0.13 s. The longest recorded frame after the fix is 0.019-0.024 s with all of them
present.

The cause is a slow game-thread frame inside the window, not a recorder on the wrong clock. Recording to wall or audio
time would not fix it: the WAV would still miss the stall, or hold 1.9 s of silence while the sim stood still.

## 2. Fix (`b34b6c5`, `fix(audio-capture): ...`)

`Session/ColourAudioCapture.cpp`, audio mode only:

- New steps `WarmDuel` / `WaitWarm`. The driver starts the same duel (`SetBout(2, true)`, `SetAirFinal(true)`,
  `Start(1, true)`) once, and the run does not record it. It waits for 20 consecutive frames under 0.05 s (fail after
  60 s), then `StartRecording` starts the duel again from scratch and records it exactly as before. The presentation actor
  lives for the whole world, so the figure loaded in the warm-up is reused. The stills path does the same thing: it
  starts three bouts in one process.
- Evidence only: the stop line adds `longest=` (the longest frame inside the window), and the manifest notes add `simS`
  and `longestFrameS`, so a later stall shows up in the log and is not only inferred from a short WAV.

Nothing else changed: same 30.8 s window, 1.0 s tolerance, master-submix recording, seed and scripted seat, and the same
two checks (driver: WAV header duration; script: ffprobe duration and peak).

## 3. Before / after

| Run | Tree | WAV s | sim / wall s | mixer s | longest frame s | peak dBFS | integrated LUFS | cues | result |
|---|---|---|---|---|---|---|---|---|---|
| rehearsal-1027-audio | 64c1dc2 | 28.565 | 28.850 / 30.814 | 30.805 | (not logged) | -5.6 | -23.8 | 134 | FAIL |
| rehearsal-1027-audio2 | 64c1dc2 | 27.435 | 27.717 / 30.802 | 30.805 | (not logged) | -5.7 | -23.9 | 128 | FAIL |
| l1-repro | d4782df | 28.800 | 29.083 / 30.802 | 30.805 | 1.93 (from the log stamps) | -5.6 | -23.9 | 134 | FAIL |
| l1-fix-1 | b34b6c5 source, uncommitted | 30.656 | 30.800 / 30.814 | 30.656 | 0.020 | -5.7 | -23.9 | 149 | ok |
| l1-fix-2 | b34b6c5 source, uncommitted | 30.805 | 30.800 / 30.811 | 30.805 | 0.024 | -5.7 | -23.8 | 149 | ok |
| **l1-final-1** | **b34b6c5** | **30.805** | **30.800 / 30.812** | 30.827 | 0.019 | -5.7 | -23.8 | 148 | **ok** |
| **l1-final-2** | **b34b6c5** | **30.805** | **30.800 / 30.807** | 30.805 | 0.020 | -5.7 | -23.8 | 149 | **ok** |
| reference 10-25 | e9d8abc | 30.677 | 30.800 / 30.800 | - | - | - | -23.8 | 142 | ok |

The done-when rule is met: two consecutive `capture-colour-audio: ok` on the committed tree (l1-final-1, l1-final-2),
and two more before that on the same source. Peak and loudness are unchanged. The cue count rises from 128-134 to
148-149 because the kernel now plays the full 30.8 s; before, it lost 1.7-3.1 s of play. Against 10-25's 142, the
difference comes from the cues added since then (T63-T72c). The first cue of the recorded run is the recorded duel's own
`arena-crowd-loop` at `audio_s=0.017`, so nothing from the warm-up leaks into the window. The warm-up absorbs the build
(`Built Skeletal Mesh [1.61-2.13s]`, `warm-up done wall=2.06-2.62 frames=22 figure=1`).

## 4. Gates (Wolf kaziho, UE 5.8.3 CL 58210709)

- `build.ps1`: `Result: Succeeded` (cold 695 s on d4782df; incremental 19 s with the fix).
- `MageArena.`: **342 / 342** `Result={Success}`, `EXIT CODE: 0`.
- `MageArenaDesign.`: 5 / 7; reds `Duel.FireSeated` and `Session.FullSeated` only (DF-011, accepted). `AirSeated`,
  `CreaturesSeated`, `Wave1Seated`, `Rivals` and `Sweep` are green.
- `capture-budget.ps1 -Runs 3` (D-G4): default scene `budget: ok (3 runs)`, draw calls max 118 / 118 / 118 (median
  113-114). Creature scene (`-ExtraArgs -MageArenaCreatureImportScene`) `budget: ok (3 runs)`, max 127 / 127 / 127
  (median 123-124), still at the 127 guide and unchanged against the rehearsal.
- Node gates: lint (`unused-members`, `format-check`) and the test chain, as the merge gate runs them.

## 5. Left open (not in this lane)

- The stall is real in the game, not only in the capture. In an editor `-game` session, the first Brennic frame of a
  day still takes about 1.9 s. The log names the cheap cure: `The derived data key is different after the build. Save
  the asset to avoid rebuilding it.` Re-saving `Content/Characters/Brennic/SK_Brennic` with the LODs that `PrepareMesh`
  generates would stop the runtime rebuild. Another option is to load the figure when the presentation spawns rather than
  on the first duel frame. Both touch content or presentation code outside this lane's paths. `PrepareMesh` is
  `WITH_EDITOR`, so a packaged build only pays the synchronous `LoadObject`, which nobody has measured yet.
- The rehearsal's row 8 now has a passing capture. "Nobody has heard them" stays an owner item.
