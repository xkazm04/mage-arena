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

**Answered 2026-10-28 (`docs/DECISIONS.md`):** (1) the duel distance stays 16 m; (2) chalk labels are allowed in the
tutorial, so the drawn-diagram language runs through the whole game; (3) the probe in section 8 is approved. Question 3
below (store key art) stays open until the in-game look settles.


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

## 9. Results

**Preparation (`push/chalk`, 2026-10-28):** the style axis `-MageArenaStyle=painted|chalk` (`a0ddd93`), style
manifests and README (`a557e9b`), the studio prompt composer with `--style/--slot/--dry` (`9737bdc`), separate chalk
asset folders (`89c7b70`), and the S30 target shot `capture-s1.ps1 -Target s30` (`637def1`; S30-VR puts Brennic at
0.344 of the frame height, matched by the key-art camera at FOV 35.5 and pitch -7.0; the game camera stays at 16.2 m).

**Probe 1 (`e12ed95`):** an unlit chalk surface shader (world-aligned chalk still, hatching stepped by N·L, grain) on
the arena and a slate sky; fire and ward re-textured. 0 extra draws, 6.6 ms median, the same on the mobile proxy.
Blind judges vs S30-VR (mean of 3, /45): chalk p1 **15.0**, painted quest 13.3, flat 10.3. The medium scored 0-1 in
every frame ("plain CG"). Diagnosis: wrong polarity (dark hatching on grey-filled stone reads as cobblestone; on slate,
light is added chalk and shadow is bare slate), no contour lines, no slate board around the player, colour everywhere
instead of only on the magic, the robe, the banners and the hands. Probe 2 addresses these without a post pass.

**Owner redirect (2026-10-28, late):** the arena and crowd become one chalk-drawn 2D backdrop under the chalk style;
probe effort goes to the opponent, surface, spells, hands and the chalk medium, which alone decide continue or stop.
For the judges, elements 11-13 (wall, crowd, banners) are reported but left out of the deciding score; the deciding
score is elements 1-10, 14 and 15, out of 36.

**Probe 2 (`6ee08c5`):** chalk polarity inverted (light is added chalk, shade is bare slate), white chalk contours
along the arena's box edges, a slate board round the seat with rune circles and construction lines, a slate sky, the
sigil and telegraph ring in the shot. Not judged (an intermediate frame); D-G4 p95 151 from the new outlines.

**Probe 3 (`b9782f6`, owner redirect applied):** one chalk-drawn arena panorama replaces the 3D arena and crowd under
chalk; pastel and hatched hands with a broken contour; ward as a continuous chalk-wave band; pale ochre pastel sand;
Brennic in a stepped chalk material. D-G4 median 115, p95 118 (from 151); 7.0 ms. Blind judges vs S30-VR (mean of 3):
full /45: chalk p3 **20.7**, painted quest 13.3, chalk p1 12.7, flat 10.7; deciding /36: p3 **14.7**, painted 9.7,
p1 9.0, flat 7.3. Open deciding points: the hands' storytelling (left open palm with the ward, right index drawing
the sigil), the cuff out of frame, the telegraph ring too faint, the backdrop's photographic finish and its seam, the
wardstones as black brick, and the fire's glossy core. The opponent stays capped by the 16 m ruling (judges estimate
+4-5 at 8 m).

**Probes 4-5 (`a3328af`, `21aa2a5`):** the backdrop redrawn as chalk line work fading into a slate sky; a bold red
chalk telegraph ring; the right hand posed from the recorded `sigil-line2` clip (shot only) with the sigil pinned to
the fingertip; both wrists, sleeves and the bronze cuff in frame (shot-only hand offsets, -20 degree look); a curved,
rubbed sand-to-slate edge; spawn posts hidden under chalk. D-G4 median 111, p95 113; 6.8 ms. Blind judges vs S30-VR
(mean of 3): full /45: chalk p5 **24.0**, chalk p3 19.3, painted quest 13.3; deciding /36: p5 **18.7**, p3 13.7,
painted 9.0. One judge puts the ceiling of this approach at about 34-36/45 with the opponent kept at 16 m. One judge
calls the remaining gap "mostly direction": the opponent's staging at 16 m, one unifying medium across sand, wave,
fire and pillars (no post pass on Quest, so per-material), and the hard foreground band. Open defects: the
wardstones ignore their chalk brightness (cause not found), the cuff's runes face away, the fire still a compact ball,
the sleeves untextured.

**Gate 30/36 rounds (owner, 2026-10-28 night).** p6 (`b7f5190`, not judged): stroke fireball, wardstone colour-space
fix, bigger sigil; the ward regressed into a corner splash and the sand into radial streaks. p7 (`b7dfff7`, with Tier
B's chalk Brennic `49ec3c1`: drawn albedo, casting lunge, 1.15x): judged with p5 as control, mean of 3: p7 full 22.3,
deciding **16.7**; p5 re-judged 22.7 / 17.0. Flat: Brennic, the sigil and the gesture gained, the ward and the ring
regressed. Judge-to-judge spread is about 3 points per frame, so a round needs a gain of several points to show.
One judge's realistic ceiling at 16 m: 38-40/45 (face capped at 2; hands, cuff and medium at 2 without heavy work).
From p8 on, every handover carries a per-element no-regression check against the last judged frame.

**p8 (`72ddf5d`, with Tier B's chalk hands, bronze cuff and wrist HUD, Brennic's screen-constant contour):** judged
with p7 as control, mean of 3: both 22.3/45 and **17.3/36** deciding. The third flat round (17.0, 16.7, 17.3). The
judges' repeated reason is the medium (1/3 every round): the per-material chalk leaves "a 3D render with a chalk
overlay". **Change of approach for p9:** section 6's "no post pass" is revisited. The project ships MobileHDR on (the
engine default), where the mobile renderer supports a post-process material, so p9 measures one cheap full-screen
chalk pass (depth edges as chalk strokes, one shared grain, posterize, slate vignette) for its look and its GPU cost.
The suite is 346/349 (the three known Lynx reds; six passes log expected warnings).

**p9 (`26aecf1`, with Tier B's hero-5: aged bronze cuff with engraved HUD channels, outlined and hatched hands, a
wider lunge):** the first mobile chalk post pass (`M_Q_ChalkPost`: depth edges as chalk strokes, shared grain,
posterize, slate vignette; MobileHDR on only; +0.72 ms GPU on the ES3.1 proxy at 1920x1080 on the desk GPU, p95
spikes unexplained), the fireball back in flight on the line from the thrust hand, the ring beside it, slate wardstones.
Blind judges (mean of 3, deciding /36): **p9 without the pass 21.3**, with the pass 20.3, p8 control 17.3 (stable).
The plateau broke (+4). The pass gives the medium (+1) but its depth edges halo the hands and its grain muddies the
cuff and face (net -1); p10 fixes it with a near-field fade and lower grain. The judges' itemised next gains sum to
about 30: the pass without halos (+2), an oval sand-to-slate edge (+2-3), a true pointing finger with the sigil at
its tip (+1-2), the ward as a taller curtain on the left (+1), a metallic cuff (+1), pencil instead of ink on the hands
(+1). The opponent stays near 4/9 at 16 m. One new timing red (`Kernel.EnemyStepCost`, 66.8 us vs 60) passed alone.

**p10 (`2a1e515`, with hero-6: a shot-only pointing hand with the sigil on the index tip, pencil hand outline, lighter
bronze cuff):** the post pass fixed (no edges or banding within 1.5 m, half grain, none on emissive; steady cost
**+0.35 ms GPU** on the true ES3.1 path, MobileHDR on, 1920x1080 on the desk GPU; p9's 9 ms p95 was a first-launch
shader warm-up), an oval sand fade, a taller ward on the left, mid-value wardstones. Blind judges (mean of 3, deciding
/36): p10 with and without the pass **19.3**; the p9 control re-judged **18.0** (21.3 in its own round): judge-set
noise is about ±2-3 on the mean, so a round's gain is read against its control in the same set. Two judges put the
ceiling of this approach at about 37/45 with the opponent at its 16 m size (opponent rows about 5/9), which is about
**29-30/36 deciding**: the owner's 30 gate is at the ceiling for 16 m, and 36 is out of reach without moving the
opponent. Next losses to fix: the sand fade read as "a blob over black void", the ground too black, the ward split into
pieces, the cuff reads as woven card, the hands as comic ink.

**p11 (`11eb2b3`, with hero-7: a lit bronze cuff, hands without the ink hull, a stronger Brennic rim):** the sand
fills the floor again with a wide soft fade, the slate lifted to charcoal with chalk dust, a continuous ward curtain
with no printed crest, a denser fireball, the ring under it, 2D chalk-drawn obelisk wardstones. Blind judges (mean of
3, deciding /36): p11 **17.7**, p10 control 17.0 (19.3 in its own round). Sand, ring, cuff and medium gained; the new
ward reads as "glass or fog curtains" and gave its point back.

**Plateau read (after p5-p11):** the deciding score has stayed in the 17-21 band across six judged rounds while
individual elements improved, with a judge-set spread of about ±2-3. The repeated blockers in every round: the
opponent at 16 m (rows 1-3 about 3-4 of 9; every judge asks for about 8 m, estimated +4-5); the frame still mixes a
drawn backdrop with 3D hands and effects (medium 1-2); the cuff's HUD stripes read as "rainbow stripes on a tan tube"
whatever its material; the fireball's hot core reads as "a white placeholder dot". The 30/36 gate is not in reach on
this trajectory at 16 m; the owner decides how to proceed.

**New yardstick (owner, 2026-10-29): asset quality and style fit, not 1:1 with S30, distance not scored.** Six classes
(opponent, sand, near floor and slate, spells, hands and cuff, chalk treatment), each scored 0-3 for fidelity at a
2020s game-art standard and 0-3 for chalk-on-slate style fit (/36). Animation sheets are judged on their own (/12).

**S2 combined 2.5D frame (`push/chalk` `bf49e62`: a Flux 2 ControlNet panorama of the static scene projected from the
seat, Tier B's painted Brennic sprite and LTX fire flipbook; hands still hero-7) vs p11 (`11eb2b3`):** blind judges,
mean of 3: **S2 17.0** (18, 16, 17), **p11 17.3** (18, 16, 18), a tie that hides a trade.
- **Fidelity rose:** S2 about 9 against 7, carried by the opponent (2 from every judge) and the fire (2).
- **Style fit fell:** S2 7-8 against 9-11.
  - The ControlNet panorama reads as a grey engraving on light stone, while p11's backdrop, white chalk line on
    near-black, was the strongest chalk element in either build.
  - The painted mage has no chalk line, so the frame mixes four styles: engraved backdrop, painted mage, woodcut hands,
    vector floor.
- **Shared by both builds:**
  - The hands and cuff ("toy-like rainbow stripes").
  - A hard sand-to-slate seam that clips the ward curtains.
  - A magenta marker above the gate.
- **Animation 6/12** from all three judges: Death has headless, melting frames; Hit loses the face; Idle motion is
  invisible at sheet scale; Cast reads well.

Read: the image-based route lifts fidelity as predicted. The next gains are bringing every generated asset into one
chalk idiom:
- a dark-ground chalk panorama,
- a chalk contour and grain baked into the sprite,
- pastel hands and an engraved cuff,
- clean sand and seam,
- repaired Death and Hit clips.
That work is round S3.

**S3 (`push/chalk` `77fba63`: the ControlNet panorama redone as chalk on dark slate, a floor face that reaches straight
down, the water flipbook in the ward with a faded foot, the HP bar hidden; Tier B's round 2-3 taken: repaired Hit and
Death, a chalk contour on the sprite, pastel hands, a cuff without stripes):** three builds in one judge set (/36,
mean of 3): **s3b 20.7** (21, 20, 21), **s2 21.3** (22, 20, 22), **p11 15.3** (15, 15, 16).
- **Both 2.5D builds beat p11 by 5-6 when judged in the same set.** The earlier S2-vs-p11 tie was a difference in how
  each judge set anchored its scores.
- **What s3b gained:** the chalk backdrop, the stroked ward and grain on the slate.
- **What s3b lost:**
  - The sand: peach and blotchy, against s2's ochre raked strokes.
  - The hands: the smooth pastel repaint reads as "3D plastic". The hatched line-art hands score higher.
- **Brennic's new contour does not show at game scale.**
- **Animation 7.3/12:** Death ends in a kneel and never falls.
- **Every build:** stair-stepped chalk rings and wall line, and leftovers that read as debug (purple corner orbs, stray
  circles by the sigil).

Combining the best class from each build, the judges' estimates come to about 24-25 before the round S4 fixes.

**S4 (`push/chalk` `0d78685`: s2's ochre sand in the chalk panorama, a lighter chalk-blue ward, antialiased rings, the
debug-looking leftovers gone, a contact shadow; Tier B's round 4: hatched line-art hands, engraved cuff runes, a
two-tone contour on Brennic, a Death that falls; plus 7 m and 4 m shot variants and a 4 m stereo pair):** three builds
in one judge set (/36, mean of 3): **s4b 24.0** (26, 24, 22), s3b 16.7 (16, 19, 15), p11 14.7 (16, 14, 14).
- **Score:** +7 over the in-set control. One judge gave chalk treatment 3/3 for the first time.
- **Brennic at duel distance** (Tier B's trace: 3.5-8 m, median 6.9 m):
  - At 7 m: 2/2 from two judges.
  - At 4 m the sprite is visibly upscaled (256 px cells).
  - The stereo pair reads as a flat cardboard cut-out. Two of three judges call that a major presence break.
- **Animation:** about 1.6/3 per clip over 7 clips.
  - Death ends in a rigid in-plane rotation.
  - The cape pops in and out between clips.
  - The scale grows mid-clip.
  - The new clips use a different matte (dark fringe vs cream rim).
- **The judges' path to 30 (all three agree, each item about +2):**
  - Brennic: 512 px cells, one matte and contour pass, and depth (a depth card).
  - Sand: non-radial, filtered chalk strokes with scuffs.
  - Spells: drawn flame tongues, ward strands, a drawn ring.
  - One grain and outline weight across the frame.
  - Smaller items: hands without the wood-grain read, dusty floor lines.

  Their estimates: 30-33.

**S5 (`push/chalk` `d01c7a1`: randomised pastel sand with footprints, a drawn flame over the fire flipbook, the ward as
dense chalk-water strokes with a foam crest, a broken chalk ring, one shared grain pass; Tier B's round 5: 512 px cells
on five pages, one matte and contour pass, cross-hatch hands that follow the form, a Death that ends lying down, a
28 cm depth card):** three builds in one judge set (/36, mean of 3): **s5b 24.7** (25, 24, 25), s4b 20.0
(19, 20, 21), p11 13.3.
- **Score:** +4.7 over s4b in the same set.
- **What gained:**
  - The ward ("close to 3", "convincing chalk-blue strokes").
  - The fire ("hand-drawn flame").
  - The sand (style 3 from one judge).
- **Depth card:** two judges measured the 4 m stereo pair by block matching. Both found the 1-3 px spread inside the figure
  sits within measurement noise, so the stills cannot show whether a 28 cm depth card helps. A body about 30 cm deep at
  4 m gives about 1 px here and about 4 arcmin on a headset. Only a headset can settle it.
- **Still holding the score back:**
  - Brennic's dark outline and painted shading (not chalk).
  - The cape popping in and out.
  - Hit with no flinch, and Strafe that reads as idle.
  - Woodcut hands and a CG cuff.
  - The hard sand/slate seam and an elliptical spotlight edge on the sand.
  - A flat white fireball core.

  Every judge puts 30 within reach from these items.
- **Texture memory:** about 131 MiB on desktop; about 47 MiB for the sprite pages at ASTC 6x6 on Quest (not measured on
  the device).

**S6 (`push/chalk` `681abaa`: a chalk contour and pastel highlights on the hands in their own material, chalk over the
cuff and sleeve, the sand fading irregularly into the slate, a scribbled fireball core held off Brennic's body at
4-7 m, rune circles with uneven pressure; Brennic is still the round-5 sprite):** three builds in one judge set (/36,
mean of 3): **s6 25.7** (28, 26, 23), s5b 23.0 (24, 24, 21), p11 13.7.
- **Where s6 won:** every judge preferred it on hands, spells and chalk consistency.
- **Remaining items the judges name:**
  - Brennic's ink cut-out edge and painted shading (Tier B's chalk contour pass is in the pipeline).
  - The cardboard-flat skin fill on the hands.
  - Near slate detail, and a dark half-disc under the ward.
  - The sand's light-pool edge and blob footprints.
  - A busy ward stroke pattern.
- **Reaching 30:** each judge puts it within the hands plus Brennic.
- **A machine note from this round:** the box ran out of memory twice.
  - First, ComfyUI with Flux and H3 loaded took about 57 GB of the 64 GB unified memory, while orphaned generation scripts
    kept submitting jobs.
  - Second, a Meta XR Simulator server left running since 2026-10-28 00:16 held about 85 GB committed. It was stopped with
    the owner's approval.
  - From now on, ComfyUI and Unreal take turns.

**S7 (`push/chalk` `77c5ae7`: s7 surfaces (near-slate dust and scuffs, chalk-rimmed footprints, a wider-stroke ward, a
rougher ring) plus the sky light as a fixed cubemap under chalk, which removes the 829-draw recapture frame; Tier B's
round 6: a pale chalk contour and hatched pastel shading on Brennic, re-rendered Hit (flinch), Strafe (steps), Death
(falls to the ground), Cast (hand intact), a pastel hand fill, Ward/Strafe/Sunfall wired with the `SpritePick` test):**
three builds in one judge set (/36, mean of 3): **s7b 26.3** (27, 26, 26), s6 25.7 (26, 26, 25), p11 13.7.
- **Brennic gained:** style 3 from two judges.
- **The ward regressed:** the wider diagonal strokes read as rain or light shafts.
- **Animation in game at 7 m:** Hit, Death, Ward and Strafe 2/3; Idle and Sunfall 1/3.
- **Worst defect:** the hand fire sits on the hidden rig's hand, so it floats off the painted hand and hangs over the
  body in Death.
- **Remaining items the judges name:**
  - A ward drawn as water.
  - A dark arc under the ward's base.
  - The fireball's pixel fringe.
  - Hands as pastel on toned paper (no white blotches or palm stripes).
  - The hand-fire anchor.
  - An Idle that moves.
  - Sunfall's size jump.

  The judges' estimates for these: 30-32.
