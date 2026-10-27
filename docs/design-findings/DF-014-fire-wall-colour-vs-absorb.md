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

## T73 evidence (2026-10-27; report only, Status stays OPEN, owner call)

T73 (owner ruling 3, `docs/research/FIRE-RIM-HUE-T73-2026-10-27.md`) gave Fire's layer a second rim hue. The wall kept
`ActiveElementPalette().Fire` (option A) and so shared that rim. The fire bolts of the live bout were not redrawn. A
capture-only switch, `-MageArenaSpellVfxWall` (`Session/SpellVfxCapture.cpp`), posed the Fire mage 8 m out with a fire wall
at the overlay's distance and arc facing the seat, and a Fire bolt drawn in Fire 3.5 m out in front of it. Frames:
`C:/Users/kazda/mage-art-staging/fire-rim-hue-t73-2026-10-27/T73-after/12-fire-bolt-wall.png` (rim on) and
`T73-rimoff/12-fire-bolt-wall.png` (T67's Fire). The three blind T73 critics (seeds 7301-7303, one fresh agent a round) were
asked whether the wall reads as a threat to absorb or as a defence:

- Round 1: "a standing barrier, more like a hazard or a defence than an incoming attack ... more like the opponent's
  barrier, or simply arena fire, than the player's own."
- Round 2: "a defence, a standing barrier, not an incoming attack ... nothing in the frame says whose it is."
- Round 3: "reads as a defence ... It is still ambiguous: the flames are exactly the element orange that means 'absorb',
  and nothing says whose barrier it is."

All three read a defence, from shape and stillness, as option A expects. A finding that is new to this DF: all three could
pick the Fire bolt out of the wall's flames "only with effort", "only just" or "only barely". The only cue was the bolt's
opaque core: "The colour gives you no help." T73 itself was reverted (its blind bar failed on "rust"), so the live game
draws T67's Fire. Nothing here picks an option.
