---
title: T74 Darker steel - a gunmetal greybox steel read as dodge and closer to S1, but only weakly as metal; reverted to 0.68
channel: vr
date: 2026-10-27
status: RAN AND REVERTED - desktop proxy, not Quest truth. The greybox palette's steel (the one source every physical projectile and its ring draws) went from 0.68 grey (sRGB 214) to a dark gunmetal 0.13 / 0.14 / 0.16 (sRGB 100 / 104 / 110). All three blind rounds read it as dodge, never as off-white, an element colour or the black core, and closer to S1 than the old stone. But "steel or metal" came back only "weakly" and "flat sticker or disc" "somewhat / partly" in all three rounds, because the stone is an unlit flat fill, so by the bar's letter it failed and the steel went back to 0.68 byte for byte. D-G4 +0 in both scenes. T74 stays in 6c54f1c..bf78191. A style-palette finding is DF-015.
sources: [apps/vr/tasks/T74-darker-steel.md, docs/DECISIONS.md (2026-10-27 ruling 4), docs/research/FIRE-VALUE-T72-2026-10-26.md, docs/research/FIRE-RIM-HUE-T73-2026-10-27.md, docs/design-findings/DF-015-style-steels-near-black-core.md, apps/vr/art/key-art/key-art-s5101-medium.jpg]
---

# T74 Darker steel, 2026-10-27

**Label: desktop proxy (D3D12, forward shading, `-nohmd`), not Quest truth.** Engine UE 5.8.3 (`Build.version` read before
the first build). No combat number, nothing under `apps/vr/data/` and no clip changed. The braziers, the crowd tint and the
threat demo are untouched. Evidence is in `C:/Users/kazda/mage-art-staging/darker-steel-t74-2026-10-27/` (`logs/`,
`tools/`, `T74-before/`, `T74-after/`, the candidate captures `T74-n12/-n18/-n26/-g14/-g20/`, `look/`, `critic-N/`,
`critic-keys/`, `critic-verdicts-unblinded.md`).

## 1. Where the steel look comes from

- **The source of truth** is `FElementPalette::Greybox()` `Palette.Steel` (`Session/ElementColour.cpp:50`), linear 0.68 grey.
  `ThreatLook` gives it to every "physical" projectile and its ring (`SessionPresentation` `ShotFor` / `RingFor`). The
  T58 capture's `11-steel-stone.png` is this stone. No steel cell, texture or material asset exists: a part is a shared tint
  of `Greybox::UnlitOpaqueMaterial()` (`LevelColorationUnlitMaterial`, a flat colour with no shading). So a sphere draws as a
  flat disc in any colour.
- **Styles 13, 25 and 30** already carry their studies' dark slate steels (brightest channel 0.105 / 0.074 / 0.089). They
  apply only with `-MageArenaStyle=<id>`, and the default bout is greybox, so they are not the stone the critics saw. They
  were left as they are (section 6).
- **The threat demo** (`Greybox/ThreatDemo.cpp`) draws its own steel from `apps/vr/data/vr/arena-layout.json`
  `threats.order[steel]` (0.68), which the `IsSteel` class in `MageArena.Greybox.ThreatColours` holds (every channel over
  0.65). That file is outside this lane's paths, and an edit to it re-records `Overlay.Golden`, so it was not touched.
- **The margins that cite steel** (`BrazierFire.h:41` chroma, `ArenaStands.cpp:111` the crowd's luminance bound under style
  25's steel 0.056, `PresentationLook.cpp:28` the HP bar) all stayed true for the new steel: a near-neutral steel's chroma
  hardly moves when it darkens, and its luminance (0.139) stayed far over the crowd's 0.054.

## 2. The value, chosen on captures

Five candidates were drawn in one build, through a scratch one-colour style file (`-MageArenaStyle=t74<id>`, never
committed, deleted after): `ApplyStylePalette` sets only the colours a file names. Stone against the sand below it
(`stone-contrast.py`, Rec.709 luminance of the decoded sRGB):

| Candidate | Linear | Stone sRGB | Stone Y / sand Y | Weber vs sand |
|---|---|---|---|---|
| before (greybox) | 0.68, 0.68, 0.68 | 214, 214, 213 | 0.672 / 0.303 | +1.217 |
| n12 | 0.12 grey | 97, 97, 97 | 0.120 / 0.303 | -0.606 |
| n18 | 0.18 grey | 117, 117, 117 | 0.178 / 0.303 | -0.413 |
| n26 | 0.26 grey | 138, 138, 138 | 0.254 / 0.303 | -0.161 |
| **g14 (T74)** | **0.13, 0.14, 0.16** | **100, 104, 110** | **0.137 / 0.303** | **-0.547** |
| g20 | 0.18, 0.19, 0.215 | 117, 120, 127 | 0.188 / 0.303 | -0.381 |

g14 was picked: a gunmetal with a faint cool cast, which read more as steel than the neutral greys did (look sheets
`look/candidates-crop*.png`). It keeps a strong value step from the sand, and its brightest channel (0.16) is twice the black
core line (0.08). Linear chroma distance (`steel-table.txt`): water 0.809, fire 1.155, air 0.948, earth 1.036, leave red
1.320. Saturation is 0.188. Against the purple-grey Wardstone pillars (0.10 / 0.095 / 0.12), the chroma distance is
0.086 (the neutral greys' is 0.267). Section 5 shows the critics noticed.

## 3. Tests, red first

`MageArena.Colour.SteelStandsApart` (`6c54f1c`). Under the greybox palette and styles 13, 25 and 30, steel is
steel-neutral (saturation at most 0.35, any cast on the cool side). It is at least `BrazierFire::ThreatChromaMargin` (0.15)
off Water, Fire, Air, Earth and the leave red by chroma. The greybox steel's brightest channel sits between 0.12
(1.5 x the black core line) and 0.35. Red on the 0.68 steel (`logs/test-red-editor.log`, `Found 6 automation tests based
on 'MageArena.Colour'`), with exactly one failure:
`Expected 'greybox steel (R=0.680000,G=0.680000,B=0.680000,A=1.000000) brightest channel 0.680 is at most 0.35, not the
old off-white 0.68' to be true.` Green on T74 (`bf78191`), inside the full run below.

## 4. Gates on T74 (`bf78191`)

- `build.ps1`: `Result: Succeeded` (base 1754727, the red state and T74).
- `MageArena.` (corpus regenerated with `generate.mjs --corpus 30 --seed 1000` and `mousepaths.mjs`):
  `Found 345 automation tests based on 'MageArena.'`, 345 `Result={Success}`, no `Result={Fail}`, `EXIT CODE: 0`
  (`logs/test-full-editor.log`). These all passed with their margins unchanged: `Greybox.BrazierFlameNotThreat`,
  `Greybox.Crowd.TintNotThreat`, `Greybox.ThreatColours`, `Greybox.ThreatPairReserved` and
  `SpellVfx.ColoursKeepThreatClass`.
- `capture-spell-vfx.ps1 -Run T74-before` / `-Run T74-after`: `MAGEVR_CAPTURE_DONE shots=11`, `capture-spell-vfx: OK`.
- **D-G4** (`capture-budget.ps1 -Runs 3`, each set `budget: ok (3 runs)`; the scene is named on every row):

| Run | Scene | Draw calls median / max | BasePass tris median / max | Live particles | Puffs |
|---|---|---|---|---|---|
| before 1 (1754727) | stress | 106 / 110 | 164,886 / 166,078 | 1,469 | 408 |
| before 2 | stress | 105 / 110 | 164,406 / 166,430 | 1,468 | 408 |
| before 3 | stress | 106 / 110 | 165,622 / 167,230 | 1,469 | 408 |
| T74 1 (bf78191) | stress | 105 / 110 | 164,406 / 167,230 | 1,469 | 408 |
| T74 2 | stress | 105 / 110 | 164,406 / 166,430 | 1,468 | 408 |
| T74 3 | stress | 105 / 110 | 164,406 / 166,430 | 1,468 | 408 |
| before 1 (1754727) | creature-import | 116 / 119 | 218,424 / 221,680 | 1,464 | 408 |
| before 2 | creature-import | 116 / 119 | 218,632 / 218,912 | 1,466 | 408 |
| before 3 | creature-import | 116 / 119 | 218,736 / 219,664 | 1,465 | 408 |
| T74 1 (bf78191) | creature-import | 116 / 119 | 218,112 / 220,768 | 1,468 | 408 |
| T74 2 | creature-import | 116 / 119 | 219,280 / 220,768 | 1,468 | 408 |
| T74 3 | creature-import | 116 / 119 | 218,112 / 220,768 | 1,468 | 408 |
| Limit | | max <= 150 (127 guide), no rise | <= 350,000 | | |

Draw calls did not rise (stress max 110, creature-import max 119, the same as before and as T73). BasePass is within noise,
with a maximum of 221,680. A colour change cannot add a draw here: a part already had its own shared tint.

## 5. The blind bar (shuffled before/after pairs vs S1 at 8 m)

Three fresh agents, one per round, each seeing only its own folder: S1, a water bolt and the unblockable telegraph as
context (both from the before build), and the stone pair (full frames, the stone cropped and scaled 3.5/8 to its 8 m size,
then x2 nearest, as T72c and T73 did for the bolt), with sides shuffled. Seeds 7401-7403; keys in `critic-keys/`. The words
are quoted in `critic-verdicts-unblinded.md`.

| Round | T74 reads as | Class | Steel or metal | Off-white | Sticker or flat disc | Black core / element | Closer to S1 | 0.68 stone |
|---|---|---|---|---|---|---|---|---|
| 1 (7401) | "dark slate or gunmetal grey ... heavy lump of lead or dark stone" | dodge, "not a confident read" | **weakly** ("painted grey", not metal) | no | **somewhat** | "a small risk in peripheral vision ... low" / no | **T74** | "nearly white", sticker "strongly", class unclear |
| 2 (7402) | "slate blue-grey disc ... painted clay or slate" | dodge, "without much confidence" | **weakly** | no | **somewhat** | no / no | **T74** | "pale grey-white puck", off-white, sticker "strongly" |
| 3 (7403) | "dark slate-blue grey ... slate puck or grey felt disc" | dodge, "mostly because 'dark grey' is what's left" | **weakly** | no | **yes, partly** | "small risk in peripheral vision ... low" / no | **T74** | "white plastic or paper circle", sticker "strongly" |

All three rounds also noted that the T74 stone is nearly the hue of the two slate-blue Wardstone pillars, and that neither
version has shading, a highlight, a ground shadow or a trail.

**Verdict on the bar:** steel must read as steel or metal and as dodge in every round, never as an element colour, the black
core or off-white/sticker. T74 met the class (dodge, 3 of 3), off-white (never), element (never) and S1 (3 of 3). It missed
"steel or metal" ("weakly", 3 of 3), and "sticker" was named "somewhat / partly" in 3 of 3. Two rounds also named a small,
low peripheral black-core risk. By the letter the bar failed, so the card's fallback ran. `87f44de` reverts `bf78191` and
`6c54f1c`. `ElementColour.cpp` and `ColourTests.cpp` are 1754727's bytes again (`git diff 1754727 -- apps/vr/Game/Source` is
empty), and the greybox steel is 0.68.

**Why colour alone could not pass:** the critics' "weakly" and "sticker" both come from the material, not the value. Every
round named the missing highlight or shading as the reason (round 1: "Even a single highlight on B would make it read as
metal"). `LevelColorationUnlitMaterial` draws a uniform fill, so no colour can give a sphere a gradient.

## 6. Gates on the revert (`87f44de`)

- `build.ps1`: `Result: Succeeded`.
- `MageArena.`: `Found 344 automation tests based on 'MageArena.'`, 344 `Result={Success}`, no `Result={Fail}`, `EXIT CODE: 0`
  (`logs/test-revert-editor.log`; 344 = the T73 count, SteelStandsApart left with the revert).
- D-G4 was not re-run on the revert: its C++ is byte-identical to 1754727, whose six runs are the "before" rows above.
- Lint: `unused-members OK`, `format-check OK`. Tests: `agy-guard`, `check-pin` (and `--self-test`), `check-overlay` (and
  `--self-test`), `leonardo-redaction` and `repo-law` were run on the final branch (the result file quotes them).

## 7. Open points (for the owner)

- **Restore T74 with the bar read on its spirit.** The darker steel fixed the named problem: it is never off-white, it reads
  as dodge in 3 of 3 rounds, and it is closer to S1 in 3 of 3. What is left is the unlit material. `git cherry-pick 6c54f1c
  bf78191` restores it (the test with it).
- **Shade the stone (a material lane, not a colour lane).** Draw only the steel parts on a lit or a cheap faked-shading
  material (a fresnel or a baked radial gradient on the unlit base) with one highlight. That is what every critic named. A
  part already has its own shared tint, so the draw count would not change, but it is a new material and a D-G4 re-check.
  Recommended, together with the darker value above.
- **The pillar clash.** The gunmetal sits about 0.09 by chroma from the Wardstone pillars (0.10 / 0.095 / 0.12), and
  every critic saw it. A neutral grey (n12 / n18, 0.27 off the pillars) avoids that at a slightly weaker "steel" read.
- **The threat demo's steel** (`arena-layout.json` 0.68) and `docs/gameplay/04-threat-language.md:83` /
  `09-pause-comfort-accessibility.md:141`, which quote "steel grey (0.68)", follow the palette if it changes again. Both are
  outside this lane's paths. The layout edit re-records `Overlay.Golden`.
- **DF-015:** style 25's steel (brightest 0.074) is inside the black core class, and style 30's (0.089) sits on its edge. That
  is an owner call for the style pick.
- **Quest truth** is unmeasured: how a 0.16 gunmetal reads on the Quest's display and tonemapper at 8 m.
