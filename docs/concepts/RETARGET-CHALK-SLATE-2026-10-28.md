# Retarget to style 30, Chalk & Slate

Owner, 2026-10-28, after the graphics push: "The progress in Quest desktop is impressive, yet the key art needs to
retarget ... the current key art seems to be an unattractive ceiling which we should not spend effort to make to reach
[the] gap between 25 and 40-45 ... prepare [the] project for retargeting of the artstyle which would have impact on all
assets and parts of the push/epic-look progress." Authority: `docs/DECISIONS.md` 2026-10-28 (retarget).

This document is the preparation: the new target, the style contract every asset is held to, what happens to each
piece of the push, how the look is rendered on Quest, and the order of work. It changes no code by itself.

## 1. The target

| Id | File | Role |
|---|---|---|
| **S30-VR** | `apps/vr/art/key-art/style30/s30-vr.jpg` | The in-game target. First-person from the seat. Every capture is judged against it. |
| S30-KEY | `apps/vr/art/key-art/style30/s30-key.jpg` | Mood and key-art reference: line quality, palette, colour reserved for the magic. |

Both were made from the storyboard's own prompts, `30-VR` and `30-KEY` (`docs/ART-STYLE-STORYBOARD.md` section 30), so
the target and the game's design agree: the hands, the 140-degree water ward, the sigil at the fingertip, the bronze
cuff with four runes, three rune circles round the seat, the telegraph ring, the fire mage eight metres ahead, the
wardens on the wall. Style 30 was already on the owner's shortlist (13 / 25 / 30) and is rated **H** for Quest
feasibility there. S1 (`key-art-s5101`) is retired as a target; it stays in the repo as history.

The words drawn into S30-VR (ARENA, PLAYER SEAT, 8m TO MAGE, TELEGRAPH RING, SIGIL, WARD 140 ARC, RUNE CIRCLE x3,
CROWD x100+) are design callouts, not UI the game must draw. Whether a few of them survive as tutorial chalk
annotations is a design option, not a target (section 7).

## 2. What S30-VR actually shows (read it before building anything)

It is not monochrome. It is **coloured chalk and pastel on a dark slate board**, with a strict split:

| Layer | In S30-VR | Rule |
|---|---|---|
| Ground (the board) | Dark blue-grey slate, smudged, visible at the frame edges as a vignette and round the arena | The substrate shows through everywhere the drawing thins out |
| Architecture, stands, crowd, wardens | Grey and white chalk line work, hatching, little colour | Line and value only; at most a faint tint |
| Banners | Small blocks of red, blue and ochre chalk | The only saturated colour outside the magic and the people |
| Sand floor | Warm ochre pastel, footprints, rubbed texture | Warm and mid-value; reads as a drawn surface, not a photo |
| Opponent | Fully coloured: crimson wool, bronze collar, skin, beard; drawn with dark contour lines | The most finished figure in the arena |
| Hands and cuff | Warm, pencil-shaded skin with drawn creases; bronze cuff, runes in colour | The most finished thing in the frame |
| Magic | Saturated: vermilion-orange fire with a hot core, white-blue water strokes, a white-blue sigil, a red telegraph ring | The brightest values and the strongest colour, "freshly drawn" |
| Diagram marks | Construction lines, rune circles in pale chalk on the sand and the slate | Thin, pale, never louder than the magic |

Palette (storyboard section 30, checked against S30-VR): slate `#2A2F33`, chalk white `#EDEDE8`, chalk blue `#7CC4E8`,
chalk red `#F06A4D`, chalk yellow `#F2D06B`; plus the sand ochre and the opponent's crimson as S30-VR paints them.

**The threat colour language survives unchanged** and fits better than before: the magic is the only strong colour, so
element colour = absorb, steel = dodge, black core with a red rim = leave all read against a desaturated world. Steel
becomes a hard white-grey chalk object with a dark contour, not a sticker disc (the T74 finding).

## 3. Style contract (the STYLE BLOCK every generated asset carries)

Medium vocabulary, not adjectives (registry `image-prompt-composition/medium-vocabulary-locking`): name the process.

> Chalk and soft pastel drawn on a dark slate board. Dusty chalk strokes with visible grain, smudged and rubbed fills,
> confident contour lines, cross-hatching for shade, construction lines and pale geometric guides left visible. Grey and
> white chalk for stone, wood and crowd; warm ochre pastel for sand; colour saved for cloth, skin, bronze and above all
> the magic, which is drawn in bright, fresh, saturated chalk.

Opposites, in every prompt: no photograph, no 3D render, no glossy or metallic reflections, no oil paint, no gradients
from a renderer, no lens effects, no text unless asked.

Texture prompts add: flat orthographic view, fills the frame edge to edge, even light with no cast shadow (so the
in-game shading is ours). Character and figure prompts add: drawn on a transparent or flat slate ground, full figure,
readable silhouette.

## 4. Impact on everything the push built

The push branch (`push/epic-look` at `fa82afe`) is about 60% infrastructure that carries over untouched, 30% content
that is regenerated in the new style through the same pipelines, and 10% that is retired.

### Keep as is (infrastructure)

| Piece | Where | Why it carries |
|---|---|---|
| Look switch, flat stays default | `Look/ArenaLook.*` | Gains a style axis (section 6); the tests stay on flat |
| The target shot | `Look/S1ShotCapture.*`, `tools/capture-s1.ps1` | Deterministic clock, two cameras, mobile proxy (`-Mobile`, `-MobileLdr`). Retarget its framing to S30-VR (Brennic 8 m in the concept, see section 7) |
| Material authoring by commandlet | `Look/CreateQuest*Commandlet.*`, `Tools/MaterialGraph.h` | Same route builds the chalk materials headless |
| Studio still tool | `tools/keyart/studio-still.mts` | Same tool, new STYLE BLOCK |
| Texture prep and checks | `tools/look/prep_*`, `seam_check.py`, `grade_compare.py` | Same tiling and normal steps; the grade targets change |
| Judge loop and scorecard method | `GRAPHICS-PUSH-2026-10-28.md` | New 15 elements (section 5) |
| D-G4 budget run, skinned-hands counting | `Budget/*`, `000ec62` | Unchanged; chalk is cheaper to draw than the lit look |
| Skinned player hands, HandFrame driving, thumb clamp | `Look/PlayerHandsLook.*`, `SK_PlayerHands` | Same mesh and rig; new material (pencil-shaded skin) |
| Brennic arms-out Cast clip, rim channel, nameplate lift, 1.1x | `A_Brennic_CastOpen`, `BrennicEpicLook.*` | Pose and presence work is style-free |
| Crowd card system: 804 cards, atlas slots, mirroring, jitter | `Look/CrowdLook.*`, `CrowdPaintedAtlas.h` | Same cards; a new chalk-sketch atlas |
| Wardstones, pylons, braziers, coping, gate behind the rival | `QuestLook::BuildPylons`, `RivalBackdrop.*` | Geometry stays; materials restyle |
| Spell geometry: ward arch with lift, bolt core, halo, trail, heart, cast flames, frozen clock | `Look/QuestSpells.*` | Shapes and timing stay; textures and colours restyle |
| Styled cuff layout, flat rune strips | `Look/CuffLook.*` | S30 draws the same bronze cuff with coloured runes |

### Regenerate in the new style (same pipeline, new content)

| Asset | Today | Chalk & Slate |
|---|---|---|
| Wall, stands, coping, pylons, dais, ledge | Sandstone and flagstone photographic-painterly textures | Grey and white chalk ashlar line work on slate, hatched in the shade |
| Sand | Warm grain with footprints | Ochre pastel with rubbed texture, drawn footprints, pale construction lines |
| Banners | Red and gold sun banner | Red, blue and ochre chalk cloth with drawn folds |
| Sky | Golden-hour painted panorama | Dark slate with smudged chalk cloud strokes and faint architecture guides; no sun disc |
| Crowd atlas | 20 painted coloured figures | Grey and white chalk-sketched cheering figures, a few colour touches |
| Brennic texture set | Painted face, crimson tunic, normal, ORM | Coloured-chalk rendition with dark contours drawn into the albedo; no normal-map realism |
| Hands material | Tanned skin, subsurface (epic) or wrap (quest) | Warm pencil-shaded skin, drawn creases and knuckle lines, chalk-grain overlay |
| Fire | White-gold heart, flame shell, licks | Saturated vermilion chalk strokes with a hot core; keep the heart, swap the textures |
| Ward | Painted turquoise wave | White and chalk-blue strokes forming the 140-degree arc, droplets as chalk dots |
| Environment fire (braziers) | Licking flame cards | Chalk-drawn flame cards (yellow, red) |
| Wardstone caps | Violet rune flame | Violet chalk rune, still the Tithe signal |

### Retire

| Piece | Why |
|---|---|
| The S1 golden-hour grade: `T_EpicGradeLUT`, quest sky regrades, S1 region targets in `grade_compare` | Fitted to S1. Refit to S30-VR, or drop: an unlit chalk look needs almost no grade |
| Lit-realism lighting as the look: Lumen bounce, golden sun, long hard shadows, contact AO tuned for sandstone | Chalk is drawn shading. Light becomes a stepped ramp that selects hatching density, not a photographic sun |
| Epic-only extras (water refraction, bloom on the heart, VSM) | The epic tier has no reason to exist for this style (section 6) |

## 5. The new scorecard (S30-VR, 15 elements, 0-3)

1 opponent figure (coloured, contoured) · 2 opponent face · 3 opponent pose and presence · 4 fire spell · 5 water ward
(140-degree chalk arc) · 6 sigil at the fingertip · 7 telegraph ring · 8 player hands (pencil-shaded skin) · 9 bronze
cuff and runes · 10 sand floor (ochre pastel, footprints) · 11 wall, stands and wardens (grey chalk architecture) ·
12 crowd (sketched) · 13 banners · 14 slate ground, vignette and construction lines · 15 medium (stroke grain, smudge,
line quality across the frame).

Elements 6, 7, 9 and 14 are new against S1. The same blind three-judge loop applies, with the flat greybox as control.
Baseline to record first: today's quest frame against S30-VR (expected low: the push built the wrong medium).

## 6. Rendering on Quest

Chalk & Slate is the easiest of the three shortlisted styles on a tiler: it needs **no post pass**, little lighting and
no reflections. Plan (to be proven by the probe in section 8, not assumed):

- **One tier.** Build the chalk style on the quest path only and judge it on the mobile proxy with MobileHDR off.
  The epic tier is frozen at `fa82afe` as a record; no new epic work.
- **Style as its own axis.** `-MageArenaStyle=painted|chalk`, orthogonal to `-MageArenaLook=flat|quest|epic`, default
  `painted` so every push capture keeps its meaning. A style manifest (`apps/vr/art/style/<id>/style.json`) names the
  texture set, palette, STYLE BLOCK and material parameters, so a later retarget is a new folder plus regeneration.
- **Surface shader (unlit or a 2-3 step lit ramp):** slate base colour; a world-aligned chalk-line texture as the
  drawing; a hatching texture whose density follows a stepped N·L (shade is hatched, not darkened); a triplanar chalk
  grain multiplied in; edge contours either drawn into the textures or from a cheap view-angle term. Inverted-hull
  outlines on hero meshes (Brennic, hands, wardstones) only if the probe shows the texture contours are not enough;
  they double those meshes' triangles and are counted against D-G4.
- **Line boil (strokes redrawn at 8-12 Hz): off by default.** Boiling lines in a headset can be uncomfortable; offer
  it only on the magic (spells "freshly drawn") and test it on the device.
- **Magic stays emissive and additive**, the brightest values in the frame. No bloom needed; halo cards stay.
- **Budget**: D-G4 unchanged (150 draws, 350k BasePass). Expect fewer draws than the lit look (no shadow casters
  needed; the arena can drop its cascades if the drawn shading carries the form).

The registry has no technique yet for drawn (non-photoreal) rendering on a standalone headset; this retarget is the
place to forge one (`standalone-headset-frame-budgets` or a new subject), with the probe's numbers as evidence.

## 7. Owner questions this retarget reopens

1. **Duel distance.** S30-VR, the storyboard prompt and its callout all say the fire mage stands **8 m** ahead; the
   game's bout-start distance is 16.2 m, and the owner kept 16 m on 2026-10-28 for S1. The retarget does not change
   the ruling by itself, but the new target is drawn at 8 m, so the opponent elements will again cap the score at 16 m.
2. **Diagram annotations in the world.** Keep the construction lines and rune circles only (decoration), or also let
   a few chalk labels appear during the tutorial (style 30's own note: "perfect fit for drawing-as-casting and for a
   tutorial")?
3. **Key art for the store.** S30-KEY is a poster, not a game frame; a store key art in this style will need its own
   generation pass once the in-game look is settled.

## 8. Order of work

1. **Probe (one round, about half a day, one builder):** the style axis and manifest, the chalk surface shader on wall,
   stands and sand from 3-4 studio stills, a slate sky, the ward and fire re-textured. Capture the target shot on the
   mobile proxy (MobileHDR off) next to S30-VR; D-G4; one blind judge round with the painted quest frame as control.
   Go / no-go on the shader approach.
2. **Arena and crowd:** all environment textures, chalk crowd atlas, banners, wardstones, braziers.
3. **Hero assets:** Brennic coloured-chalk texture set and contours; hands pencil-skin material; cuff runes.
4. **Magic:** fire, ward, sigil, telegraph ring, rune circles in the new stroke language; threat-colour check.
5. **Judge rounds** after each of 2-4; the final blind panel against S30-VR with today's painted quest frame as the control.

Studio budget: about 30 stills for the whole retarget (each 20 credits, deleted after download).
