# Graphics push archive (2026-10-28/29, Lynx)

This is the record of the last VR graphics push, kept small after the project was decommissioned (DECISIONS
2026-10-29). The plans and judge records are in `docs/concepts/GRAPHICS-PUSH-2026-10-28.md`,
`docs/concepts/RETARGET-CHALK-SLATE-2026-10-28.md` (section 9) and
`docs/research/IMAGE-BASED-25D-RESEARCH-2026-10-29.md`.

| Folder | What |
|---|---|
| `frames/` | 16 judged frames, JPG 85, max 1376 px wide (see the list below) |
| `patches/` | the four experiment branches as `git format-patch master..<branch>` mboxes |
| `pipeline/` | the generation scripts: Flux 2 and ControlNet keys, LTX-2.5 and MiniMax H3 segments, BiRefNet mattes, the atlas packer, the panorama stitcher, batch files and small ComfyUI workflow JSONs |
| `notes/` | the shared hand-off log between the two builders, the sprite-coverage memo and the 2.5D research draft |

The patches hold code, text and **LFS pointers only**. The binaries those pointers name (sprite pages, the panorama,
materials, textures) were not kept, so applying a patch restores the code but not the assets. Re-running the
commandlets and the pipeline regenerates them. To look at a branch:
`git checkout -b push/chalk master && git am docs/archive/graphics-push-2026-10/patches/chalk.mbox`.

| Patch | Commits | What it is |
|---|---|---|
| `quest-look.mbox` | 35 | S1 painted look, mobile renderer tier |
| `epic-look.mbox` | 72 | S1 painted look, desktop tier (merges quest-look) |
| `chalk.mbox` | 147 | Chalk & Slate: 3D materials p1-p11, then the 2.5D route s1-s8 (panorama, sprite, flipbooks) |
| `chalk-hero.mbox` | 144 | The hero assets for chalk: Brennic sprite pipeline and clips, hands, cuff, wiring (`SpritePick`) |

Frames, in judging order (scores are the mean of three blind model judges in the round's own set, /36 unless noted;
the owner read the last one as about 20):

| File | Build | Score |
|---|---|---|
| `baseline-s1-game.jpg` | greybox before the push | 13.7/45 vs S1 |
| `quest-r9-s1-game.jpg`, `epic-r6-s1-game.jpg` | S1 painted look, quest and epic tiers | 25.3 and 26.3/45 vs S1 |
| `chalk-p11-game.jpg` | Chalk & Slate, 3D materials, round 11 | 17.7 |
| `s2-judge-game.jpg` | first 2.5D frame | 17.0 (its set), 21.3 (S3 set) |
| `s4b-16m-game.jpg` | 2.5D, one chalk idiom | 24.0 |
| `s6-16m-game.jpg` | chalk hands, scribbled fire | 25.7 |
| `s7b-16m-game.jpg`, `s7b-4m-game.jpg` | chalk contour on the sprite | 26.3 |
| `s8-16m-game.jpg`, `s8-16m-keyart.jpg`, `s8-7m-game.jpg`, `s8-4m-game.jpg`, `s8-strips-7m-ingame.jpg` | last judged round; the strip shows seven clips at 7 m | 24.3 |
| `s2r8-game-4m.jpg` | the hero branch's last 4 m frame | not judged |
| `s2r5-stereo-4m-flat-top-depth-bottom.jpg` | the flat card against the 28 cm depth card at 4 m (left/right eyes) | not judged |
