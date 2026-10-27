# L3 - session capture framing (gap G5)

Host `kaziho` (Windows 11, UE 5.8.3), base commit `cac6442`. Capture tool only: no gameplay, C++, data, material or VFX
change; the seed-1 duel outcome and its event log are untouched.

## Before (G5 evidence)

`capture-style-pick.ps1` cut its four stills at the events a quarter, half and three quarters into the log and at the
last one (events 50, 101, 151, 202: ticks 457, 926, 1399, 1870). The camera follows the duel, not the rival, so Brennic
was out of frame in stills 1, 2 and 4 in all four palettes (`before-greybox-sheet.png`; only still 3 shows him).

## What changed

- Each still keeps its quarter aim but snaps to the nearest **Brennic event** (his cast, or a hit on him; actor 2 in the
  log) whose frame shows him. No event or manifest field says whether he is in view, so the tool reads a pixel signal:
  his nameplate track, a flat `93,94,112` bar over his head, drawn unlit. Out of view the arena band of the frame holds
  0-25 such pixels, in view 70-680; the threshold is 60.
- Styles 13, 25 and 30 grade that colour (and 13/25 put it on a pillar), so the test reads the **greybox** frames. The
  camera and event log are identical across styles (counts agree frame for frame on the sampled ticks; the events md5 is
  identical), so the chosen ticks carry over and each style's still is cut from its own frame at that tick. A run
  without greybox in `-Styles` snaps to Brennic events unchecked (`rivalbar=unchecked`).
- A still with no in-view Brennic event is a new failure. Every old success condition is unchanged.

**Seed 2 / his death clip: not done.** The seed does not reach the editor: `StylePickCapture.cpp:202` calls
`Sys->Start(1, true)` and reads only `MageArenaStyle`, `StylePickFrames` and `StylePickBout` from the command line, so
`-ExtraArgs` cannot select it. Option (a) needs a `-MageArenaStylePickSeed=` knob in that C++ file, which is outside this
lane. Seed 1 ends with the player's death (DF-011), so his death clip still appears only in `rival-death-seed2.png`.

## After

All four stills show Brennic (`after-greybox-sheet.png`, `after-greybox-0N.png`; the same ticks in `13-`, `25-`, `30-`).

| Still | Tick | Event | Kind | Nameplate px |
|---|---|---|---|---|
| 1 | 481 | 52 | Brennic cast | 381 |
| 2 | 931 | 102 | Brennic cast | 255 |
| 3 | 1399 | 151 | Brennic cast | 171 |
| 4 | 1477 | 166 | hit on Brennic (his Hit clip) | 374 |

Still 4 is no longer the duel's last event: Brennic is out of frame for the final 90 ticks.

## Wolf evidence

Host `kaziho`, commit `cac64429e061692c01960274728851041fac5ee4` (tool change on top), full four-style run, exit 0:
`capture-style-pick: ok`, four `MAGEVR_CAPTURE_DONE shots=1870` lines, no `MAGEVR_CAPTURE_FAIL`, ffmpeg exit 0 per style,
events md5 `7B0715C871AC3399504C4F7A744896D4` in every pass (greybox, 13, 25, 30; one seed-1 pass each). Stills and MP4s
rewritten in `apps/vr/art/style-pick/`.
