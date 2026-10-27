# DF-014 - The fire wall is drawn in Fire's colour, and Fire's colour means "absorb"

Found 2026-10-27 in lane L2 (fire-wall read, gap G4 of `docs/research/GATE-REHEARSAL-VR-2026-10-27.md`). Status: **OPEN,
owner call**. Nothing here is decided in code: L2 kept the colour it found and changed only the shape.

## The rule and the clash

`docs/gameplay/04-threat-language.md`: element colour = magic = absorb (face it with the ward); steel/white = physical =
dodge; black core with a red rim = unblockable = leave. The wall is the Fire school's **defence** (a standing curtain the
player raises), yet `SyncWalls` draws it in `ActiveElementPalette().Fire`, the very colour a Fire *attack* would carry.
In the live bout every magic hit is drawn Water turquoise and only the wall is vermilion
(`04-threat-language.md`, "What the live bout draws"), so today the clash is hidden: no incoming hit is vermilion.
It appears when Brennic's Fire bolts are drawn in Fire (the Fire second-rim-hue lane, DECISIONS 2026-10-27 ruling 3), and
a vermilion wall then reads as one more thing to absorb.

## What L2 did

The wall is now flame cards on the T63/T67 glow layer, in the same Fire colour (so it shares the Fire layer: no new
colour, no new cell, no new draw call). The shape tells it apart from a threat: it stands still, has no shrinking ground
ring, no approach and no bead ring, and it is set apart in flames with the rival visible between them.

## Options (for the owner)

- **A. Keep Fire's colour (recommended).** The wall is the school's own flame; the shape and stillness separate it from a
  threat, and every other colour is taken (steel = dodge, black/red = leave, the other three elements = absorb). Cost: the
  clash returns when Fire bolts are drawn Fire; revisit then with the blind-critic bar of ruling 3.
- **B. A defence tone outside the threat classes** (for example a warm white-gold shared by all four schools' defences).
  Cleanest language, but it needs a new colour and a pass over the dome (which is already drawn without an element
  colour) and is an art decision.
- **C. Dim the wall's shade** so it reads as a backdrop, not a hit. Weakest: dimming does not change the hue the rule keys on.

No signed number or pinned data is involved in any option.
