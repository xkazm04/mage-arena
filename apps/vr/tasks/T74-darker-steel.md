# T74 - Darker steel

Status: ran, blind bar not met (steel read only "weakly" as metal and the flat disc partly a sticker in 3 of 3 rounds;
dodge, never off-white, closer to S1 in 3 of 3), greybox steel restored to 0.68 (fallback) - report docs/research/DARKER-STEEL-T74-2026-10-27.md
Max turns: 120

## Goal
The steel (dodge) stone reads as steel or iron at 8 m, not as the near off-white "flat sticker disc" every critic named in
T72 and T72b (`docs/research/FIRE-VALUE-T72-2026-10-26.md:161-163`; `docs/concepts/VISUAL-PLAN.md` (b), the "Option only"
steel note). Steel stays the dodge colour of the threat language (element colour = absorb, steel = dodge, black core with a
red rim = leave): darker, but never an element colour, the wind green or the black core. This is owner ruling 4 of
2026-10-27 (`docs/DECISIONS.md`), the steel half; the bead ring is a later lane.

## Where the steel look comes from (read before any change)
- **The source of truth:** `FElementPalette::Greybox()` `Palette.Steel` (`Session/ElementColour.cpp:50`), linear 0.68 grey
  (about sRGB 216). Every physical projectile and its ring (`ThreatLook`, family "physical"; `SessionPresentation`
  `ShotFor` / `RingFor`) draws it, on the unlit opaque greybox material (`Greybox::UnlitOpaqueMaterial`): a flat colour,
  which is why a sphere reads as a disc. The T58 capture's `11-steel-stone.png` is this stone.
- **Styles 13, 25 and 30** already carry dark slate steels from their studies (`apps/vr/data/vr/styles/style-*.json`
  `projectiles.steel`, brightest channel 0.074-0.105). They apply only with `-MageArenaStyle=<id>`; the default bout is
  greybox, so they are not the stone the critics saw. They do not change here.
- **Not this stone:** the greybox threat demo (`Greybox/ThreatDemo.cpp`) draws its steel from
  `apps/vr/data/vr/arena-layout.json` `threats.order[steel]` (0.68), held by `MageArena.Greybox.*` `IsSteel` (every channel
  over 0.65). That file is outside this lane's paths and an edit to it re-records `Overlay.Golden`; it stays, and the split
  is reported.
- **Margins that cite steel:** `BrazierFire.h:41` (the flame's chroma 0.15 off steel's), `ArenaStands.cpp:111` (the crowd
  tints sit under the dimmest threat's luminance, style 25's steel 0.056), `PresentationLook.cpp:28` (the HP bar is no
  steel grey), `ColourTests.cpp:17` and `SharedMaterialTests.cpp:34` (the 0.68 literal). A neutral steel's chroma does not
  move when it darkens, so the brazier margin cannot fall; the crowd bound needs steel's luminance over 0.054.

## Deliverables (exact paths)
- `apps/vr/Game/Source/MageArenaVR/Session/ElementColour.cpp` - the greybox `Palette.Steel`, darker, with its reason.
- `apps/vr/Game/Source/MageArenaVR/Tests/ColourTests.cpp` - the greybox literal, and a new
  `MageArena.Colour.SteelStandsApart` (red first on the old steel): in the greybox palette and styles 13, 25 and 30, steel's
  brightest channel is at least 1.5 x the black core class's 0.08, its chroma is steel-neutral (saturation at most 0.25) and at
  least `BrazierFire::ThreatChromaMargin` off Water, Fire, Air, Earth and the leave red, and the greybox steel is not the old
  off-white (brightest channel at most 0.35).
- Every comment or test that quotes the old value (`ColourTests.cpp`, `SharedMaterialTests.cpp` if it means the palette).
- `docs/research/DARKER-STEEL-T74-2026-10-27.md` - desktop proxy, not Quest truth.
- `docs/concepts/VISUAL-PLAN.md` (b) - one (13) T74 line. This card's status.

## Constraints
- Do not push; touch no other repository. Do not edit `docs/DECISIONS.md`. No combat number, `apps/vr/data/PINNED.json`,
  `apps/vr/data/pinned/`, `apps/vr/data/conformance/` or clip corpus edit. No Leonardo calls.
- Every `MageArena.Greybox.*NotThreat` test (BrazierFlameNotThreat, Crowd.TintNotThreat and the rest) stays green with its
  margin intact. A margin is never weakened to fit the new steel; if no steel fits, stop and report.
- At most one Unreal process machine-wide. Never commit `Binaries/`, `Intermediate/`, `Saved/`, `DerivedDataCache/` or
  `runs/`. Evidence in `C:/Users/kazda/mage-art-staging/darker-steel-t74-2026-10-27/` only.

## Acceptance (quote the real output in the report)
```
build.ps1                                                     # Result: Succeeded
MageArena. automation group (corpus regenerated)              # Found N >= 344, no new reds beyond the DF-011 two
capture-budget.ps1 -Runs 3                                    # before and after, stress scene
capture-budget.ps1 -Runs 3 -ExtraArgs -MageArenaCreatureImportScene   # before and after, creature-import scene
                                                              # draw calls do not rise, <= 150; BasePass <= 350k
capture-spell-vfx.ps1 -Run T74-before / -Run T74-after        # 11 shots each
```
**Blind bar (ruling 4):** shuffled before/after pairs vs S1 at 8 m, a fresh critic per round, 3 rounds, seeds and keys kept.
In every round steel must read as steel or metal and as dodge, never as an element colour, the black core or unblockable,
or off-white / sticker. It passes only if all 3 rounds say so.
**If the bar fails:** restore the greybox steel to 0.68 byte for byte in its own commit (as T72b, T72c and T73 did), re-run
the build, the suite and D-G4, and report.

## Report
`docs/research/DARKER-STEEL-T74-2026-10-27.md`; the evidence stays in the staging folder above.
