# DF-015 - Style 25's steel sits inside the black core class, style 30's on its edge

Found 2026-10-27 in lane T74 (darker steel, owner ruling 4 of 2026-10-27; `apps/vr/tasks/T74-darker-steel.md`,
`docs/research/DARKER-STEEL-T74-2026-10-27.md`). Status: **OPEN, owner call** (it bears on the style pick, 13 / 25 / 30).
Nothing here is decided in code: T74 changed only the greybox steel, then restored it when its blind bar failed, and
left the three style files as they are.

## The rule and the clash

The threat language: element colour = absorb, steel = dodge, black core with a red rim = leave
(`docs/gameplay/04-threat-language.md`). The tests name the black core class as every channel under 0.08
(`IsBlackCore`, `Tests/GreyboxTests.cpp`, `Tests/BrazierTests.cpp`), and T74's brief asks for steel's brightest channel
"clearly above" that line. The greybox steel is 0.68 (T74's gunmetal, reverted, was 0.16, twice the line). The three
style studies' steels (`apps/vr/data/vr/styles/style-*.json` `projectiles.steel`, sampled from each study's
`07-threat-language.jpg`) are:

| Style | Steel (linear) | Brightest channel | x the 0.08 line | In the black core class? |
|---|---|---|---|---|
| 13 screen print | 0.0762, 0.1022, 0.1046 | 0.105 | 1.31 | no |
| 25 moonlit silver | 0.0497, 0.0561, 0.0742 | 0.074 | 0.93 | **yes** (every channel under 0.08) |
| 30 chalk slate | 0.0595, 0.0723, 0.0887 | 0.089 | 1.11 | no, by 0.009 |

With `-MageArenaStyle=25`, a dodge stone is drawn in a colour the tests class as the unblockable's black core. Only the
missing red rim and the shape tell them apart. By chroma all three stay far from every element and the leave red: T74's
`MageArena.Colour.SteelStandsApart` (in `6c54f1c`, reverted with the steel in `87f44de`) held that in all four palettes,
and held its brightness floor (at least 0.12) for the greybox palette only, so that it could stay green while this was open.

## Options (for the owner)

- **A. Lift each style's steel to at least 1.5 x the line (0.12) at its own chroma (recommended).** For example, style
  25 x 1.62 = 0.081, 0.091, 0.120, and style 30 x 1.35 = 0.080, 0.098, 0.120. It keeps each study's hue, separates dodge
  from leave by value as the greybox does, and would let `SteelStandsApart` hold the floor in every palette. Cost: the
  style no longer matches its study's sample exactly (each file's `_provenance` line would record the lift). The crowd
  tint bound (`ArenaStands.cpp`, tints under style 25's steel luminance 0.056) gets more room, never less.
- **B. Leave the styles as sampled.** Moonlit silver is a dark palette, and shape and the rim still separate the two
  classes. Cost: the colour language breaks at its value step in the darkest style, the one where a near-black stone and
  a black core are hardest to tell apart at 8 m.
- **C. Decide at the style pick.** Only the picked style's steel needs lifting, so it can be done then.
