# L2 - fire wall read (gap G4)

Host `kaziho` (Windows 11, UE 5.8.3 CL 58210709), base commit `c5a49fe`, change commits listed at the end. Presentation
only: no signed number, no pinned data.

## Cause

`ASessionPresentation::SyncWalls` drew eight `Cylinder` slats per wall (16x36x120 cm each) on the translucent curtain
material in the Fire colour at alpha 0.88. Seen from the seat they read as flat orange pillars that filled a third of the
view and hid Brennic's cast (`docs/research/gate-rehearsal-2026-10-27/greybox-tick1399-rival-crop.png`,
`style-pick-greybox-sheet.png` stills 1, 3, 4, `rival-cast-seed1.png`).

## Change

1. The slats are gone. Each wall is eight **flame cards** on the existing glow layer: `SpellVfx::WallFlamePuffs` emits one
   `FireFlameA`/`FireFlameB` card (the T63/T67 cells; no new cell) per slat position, flickering out of step, standing on
   the sand. The cards join the Fire layer that Fire bolts already use, so the wall costs no new colour or material.
2. Opacity toward the player: a card is 70 cm wide and 105 cm tall, against 1.3 m between neighbours on the 4 m, 150 degree
   wall (about 55 % of the arc is open), and its top is below the 1.2 m seated eye line. The rival shows between the flames
   and above them. The flame cell is itself clear outside its silhouette. Opacity is by shape, not a per-card alpha: the
   glow material has no per-instance alpha, and a new one would need a new material.
3. Colour against `04-threat-language.md`: kept as found (Fire). The rule that element colour means absorb clashes with a
   defence in that colour once Fire bolts are drawn Fire. That needs an owner ruling, so it is written up as
   `docs/design-findings/DF-014-fire-wall-colour-vs-absorb.md` and not decided here.

New test `MageArena.SpellVfx.WallFlamesLeaveTheRivalVisible` holds the one-card-per-flame, Fire-cell, on-the-sand, gap and
below-eye-line facts. `SyncWalls` now runs after `GlowFrame.Reset` (it needs the eye) and takes the frame time.

## Stills (`capture-style-pick.ps1 -Styles greybox`, seed 1, 1870 frames, `ok for greybox`)

Before (rehearsal, `c5a49fe` without the change): `docs/research/gate-rehearsal-2026-10-27/style-pick-greybox-sheet.png`,
`greybox-tick1399-rival-crop.png`. After, 960 px wide, in `docs/research/L2-fire-wall/`:

- `after-greybox-03.png` (tick 1399): Brennic stands in clear view, one flame beside him, no pillar across him.
- `after-greybox-01.png` (tick 457): the wall as separate flames over open sand. Brennic is not in frame, as in the
  rehearsal's still 1 (a framing matter, G4's second half, not the wall).
- `after-greybox-04.png` (tick 1870, the death frame).

## Tests and budget

| Check | Result |
|---|---|
| `build.ps1` | `Result: Succeeded` (twice: before and after the test was added) |
| `MageArena.` full suite | 342 / 342 (336 pass, 6 pass with warnings), 0 failed, before the new test; the new test passes in the `MageArena.SpellVfx` run (12 / 12), so the suite is 343 |
| `MageArenaDesign.` | 4 pass, 2 red: `Duel.FireSeated`, `Session.FullSeated` (the DF-011 reds) |
| `capture-budget.ps1 -Runs 3` stress | `budget: ok (3 runs)`; draw calls max 111 / 111 / 111 (rehearsal 118); BasePass max 165,456 / 165,456 / 165,504 (rehearsal 166,464); particles max 1,468 / 1,468 / 1,468 |
| same, `-MageArenaCreatureImportScene` | `budget: ok (3 runs)`; draw calls max 123 / 123 / 119 (rehearsal 127); BasePass max 218,994 / 218,994 / 217,938 (rehearsal 219,858); particles max 1,467 / 1,468 / 1,468 |

Honest reading of the budget: the stress and creature-import scenes **do not raise a fire wall** (the rehearsal said the
same), so these figures measure the machine and the build, not this change, and they sit at or below the rehearsal's.
By construction the wall in a session is now one layer shared with the Fire bolts instead of eight translucent draws, and
costs at most 8 puffs per wall of the 500 puff cap. No in-session wall draw-call figure was captured.
