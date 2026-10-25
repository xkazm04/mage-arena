# Gate captures, K3 rows 8 and 1 (2026-10-25)

Measurement only. No C++, content, pinned data or clip corpus changed. Feeds `docs/research/GATE-RECORD-DRAFT-2026-10-25.md`
rows 1 and 8. Both rows stay **partly met** (see the end).

- Host: Wolf (`kaziho`), run from a worktree of this repo, one Unreal process at a time (`tasklist` showed none before each launch).
- Commit: `e9d8abc` (T38 merged). Tree clean before the runs.
- Engine: UE 5.8.3, CL 58210709 (`Build.version`: Minor 8, Patch 3, `++UE5+Release-5.8`).
- Build first (the worktree had no Binaries): `powershell -NoProfile -ExecutionPolicy Bypass -File apps/vr/tools/build.ps1`
  printed `Result: Succeeded`, exit 0, 235 s. The untracked MetaXR plugins were not copied in (optional in the `.uproject`).

## Row 8: threat cues spatialized

Command: `powershell -NoProfile -File apps/vr/tools/capture-colour-audio.ps1 -Run GATE-R8-2026-10-25 -SkipStills`. Exit 0.

```
LogMageArena: MAGEVR_T26 recording stop audio=30.803 wall=30.800 sim=30.800 mixer=30.805 cues=142 phase=active
LogMageArena: MAGEVR_T26 wav duration=30.677 rate=48000 channels=2 bits=16
LogMageArena: MAGEVR_CAPTURE_DONE mode=audio shots=0
capture-colour-audio: duration=30.677333
capture-colour-audio: ... mean_volume: -26.1 dB
capture-colour-audio: ... max_volume: -5.9 dB
capture-colour-audio: ok
```

- The T45 `Test-Recording` check passed: 30.677 s against 30.8 s within 1.0 s, peak -5.9 dBFS (floor -60).
- WAV `runs/GATE-R8-2026-10-25/audio/brennic-opening.wav`, 5,890,092 bytes, md5 `621091d5415226340ee57ab17daa758d`.
- The script prints no LUFS. I measured it with `ffmpeg -af ebur128`: **integrated -23.8 LUFS**, against T26's -23.2 LUFS
  (2026-10-07). 0.6 LU quieter; 142 cues logged in the cue table.
- Staged for listening: `C:/Users/kazda/mage-art-staging/gate-2026-10-25/audio/` (the WAV and `brennic-opening-cues.tsv`).
- A person has not heard it. That stays the owner's.

## Row 1: full session capture

Command: `powershell -NoProfile -File apps/vr/tools/capture-style-pick.ps1` (default styles greybox, 13, 25, 30). Exit 0.

The first launch was stopped by the session's 20 minute tool limit while ffmpeg was encoding the last MP4 (all four
`MAGEVR_CAPTURE_DONE` and the four event-log md5s had printed). That is a harness stop, not a script FAIL. I re-ran the whole
command with a long timeout; the figures below are from the second, complete run (the first gave the same four md5s).

```
MAGEVR_CAPTURE_DONE shots=1870 manifest=.../runs/T16/shots-greybox/manifest.json   (x4: greybox, 13, 25, 30)
capture-style-pick: <style> manifest frames=1870 commit=e9d8abc6b1b2212330438fe9b2e931b2b9f348d0 seed=1   (x4)
capture-style-pick: events-greybox.jsonl md5=7B0715C871AC3399504C4F7A744896D4
capture-style-pick: events-13.jsonl      md5=7B0715C871AC3399504C4F7A744896D4
capture-style-pick: events-25.jsonl      md5=7B0715C871AC3399504C4F7A744896D4
capture-style-pick: events-30.jsonl      md5=7B0715C871AC3399504C4F7A744896D4
capture-style-pick: ok
```

- Event logs: `7b0715c871ac3399504c4f7a744896d4` x4 (203 events each), identical to each other **and identical to T16's**
  `7b0715c871ac3399504c4f7a744896d4`. Nothing to explain: T38's change after the down (`a918062`) is in the rival capture
  path and did not move this seed-1 style-pick log.
- Outputs: 4 MP4s (4.99 to 5.96 MB), 16 stills (1920x1080), 4 event logs.
- The script writes its stills, MP4s and `index.html` into the tracked `apps/vr/art/style-pick/` (it modified 20 files there).
  I copied everything first, then restored that folder with `git checkout HEAD -- apps/vr/art/style-pick`; `git status` was clean.
  The T16 owner-pick set is untouched.
- Staged: `C:/Users/kazda/mage-art-staging/gate-2026-10-25/style-pick/` (`shots/`, 4 MP4s, 4 event logs,
  `art-style-pick-written/` the script's own output folder, and `greybox.png`, `13.png`, `25.png`, `30.png` 2x2 contact sheets).

### Stills findings (I looked at all 16, as 2x2 sheets per style)

- Each style shows the same four moments (ticks 457 hit, 926 cast, 1399 cast, 1870 death) in a different palette:
  greybox warm sand/orange, 13 olive and brown, 25 blue-teal, 30 grey-teal.
- No debug overlay text (no stat lines, FPS, log text, tick counters) in any still at the sheet's 640 px width.
  The only text-like elements are the diegetic spell-ring pips and a white bar pair. Not checked at full size pixel by pixel.
- Arena is populated: crowd stands, pillars, braziers, fire and water spells, threat pillars (orange) and water bolts.
- Creatures: still 3 shows a small figure with a fire and a water spell at range; still 4 (the death frame) shows a large
  black rectangle across the upper view and two bare white bars, and a clump of spheres at the bottom (the player hands).
  **No readable rival or creature body in stills 1, 2 and 4.** Still 2 is filled by a water spell. This is
  consistent with the animated Brennic being a different capture (`capture-rival.ps1`, T38), not this one.
  The black rectangle and white bars look like unlabelled placeholder geometry; I cannot tell from the still what they are.
- T55 (no debug text in views) was not judged here beyond the above.

**Fix (T64, 2026-10-25).** The black rectangle was the prompt plate (`DaisPlate`) shown on the first frame of the "lost"
prompt, before its widget had drawn (a widget component draws only while visible). Plate, tablet slab and stone labels now
prime hidden in game for two syncs; `MageArena.Presentation.Cards.*` pins it and the capture driver fails a frame with a
blank card. Re-run on the fixed tree: `capture-style-pick: ok`, events md5 unchanged; the new still 4 has no rectangle
(before and after in `mage-art-staging/gate-2026-10-25/style-pick-fix/`). The two white bars are the T22 collar crack seams
(a death is a Crack), a designed flash, left alone: the right one has no cuff under it. See `apps/vr/tasks/T64-death-frame-blank-card.md`.

## Verdicts

- Row 8 stays **partly met**: re-measured on 5.8.3 with the T45 launcher, but nobody has heard it.
- Row 1 stays **partly met**: a full-session capture of the greybox base in four palettes exists on the current tree;
  whether it satisfies "the full session in the chosen style" is the owner's reading of ruling 1.
