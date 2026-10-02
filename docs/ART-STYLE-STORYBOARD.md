# Mage Arena VR: art-style storyboard, 30 direction pairs for Leonardo

Written 2026-10-02. A companion to `PROJECT-PLAN.md` in this folder. These are **look studies**: images that ask "what could this game look like", not promises about gameplay, frame rate or final assets. No named artist, studio, film or franchise appears in any prompt (originality rule from the PC art pipeline, `mage-arena-art/art/STYLE.md`).

## Owner shortlist (2026-10-02)

**13 Screen-Print Poster, 25 Moonlit Silver Nocturne, 30 Chalk & Slate** - kept in mind throughout the project.
The game is built style-less (greybox) until all mechanics work together; the art pass then picks one of the three
at the W4 art gate (see `PROJECT-PLAN.md` section 1). All three are flat or unlit looks in which magic is the only
strong colour, which is also what the greybox must already respect.

## How to use this file

1. **Two images per style.** `VR` emulates what the player sees in the headset; `CONCEPT` is key art that wraps the style. Generate both before judging a style: a look that only works as key art is a portrait style, not a game style.
2. **Fair comparison.** Every VR prompt uses the same scene and every concept prompt uses the same scene. Only the opening `Style:` block changes. If you rewrite a scene, rewrite it for all 30, or you are comparing scenes, not styles.
3. **Leonardo settings (suggested).** VR image: **16:9 landscape** (the headset view is wider than tall; crop the centre ~70% to preview the Meta VR Glasses' narrower ~70 deg FOV). Concept image: **2:3 portrait** (poster). Use the model you find most prompt-faithful for illustration; generate 4 per prompt and keep the best. Hands fail often in image models; reroll for hands before judging a style on them.
4. **Negative prompt.** Paste the shared negative (below) plus the style's extra line.
5. **Score each pair** in the scoring sheet at the end. Prompts are under ~1,500 characters so they fit Leonardo's prompt box; if your model truncates, trim the scene sentence about the stands first.

### What every VR image is testing (the game's real constraints)

- **Hands first.** Your two bare hands are the hero: the right finger drawing a sigil, the left palm raising the water ward. A style where the hands disappear into the background fails.
- **Threat colour language** (from the B/2 combat design): an element-coloured spell with a shrinking ring means *absorb it*; steel-white means *dodge*; a black core with a jagged red rim means *unblockable, leave*. The style must keep magic the most saturated, most readable thing in frame.
- **Seated, two-foot radius.** The three rune circles (blink pads) sit within arm's reach of the seat.
- **Diegetic HUD.** The tier clock lives on the bronze prisoner's cuff (four runes, lit as tiers unlock); no floating UI panels.
- **Quest reality.** Each style carries a Quest feasibility rating: **H** = standard stylised techniques at 72 Hz, **M** = needs care or faking, **L** = expensive, likely key-art only. Ratings are the host's engineering judgement, not measurements.

### Shared scene blocks (already included in every prompt below)

**VR scene:**

> First-person view from a seated player's eyes inside an open oval Roman-era sand arena, wide 100-degree field of view, eye height of a seated adult. In the lower foreground, the player's own two bare hands, no gloves, no devices: the right index finger traces a glowing circular sigil of water-light in the air, two concentric rings with one bold inner stroke, droplets trailing the fingertip; the left palm is raised forward, projecting a curved translucent ward shaped like a 140-degree arc of rippling water. A bronze prisoner's cuff on the left wrist has four inlaid runes, two of them lit. Three faint rune circles are set in the sand around the player's seat, within arm's reach. Eight metres ahead, a broad bearded fire mage in scorched crimson wool with a bronze collar hurls a vermilion fireball; a thin shrinking ring telegraphs where it will land. Tiered stone stands with a watching crowd, banners, and grey-robed arcane wardens on the wall. Clear readable silhouettes, the hands and the incoming spell are the focal points.

**Concept scene:**

> Key art for an original Roman-era elemental-mage arena game. Cassia, a sharp-featured water mage woman in her thirties, dark hair bound with sea-blue cord, layered sea-blue and undyed linen robes, a bronze prisoner's collar with four inlaid runes, stands braced on arena sand drawing a large luminous sigil of concentric water rings with one hand while her other palm holds back a wave of vermilion fire thrown by a bearded fire mage in crimson. Behind them, towering stands, Roman standards, and grey-robed wardens watching from the wall. Dramatic, heroic, poster composition, strong silhouette.

**Shared negative prompt:**

```
text, letters, captions, watermark, logo, signature, UI labels, frame border, extra fingers, six fingers, fused fingers, missing fingers, deformed hands, twisted wrists, game controller, VR headset visible, gloves, smartphone, modern clothing, sunglasses, guns, cars, cropped hands, duplicate characters, blurry, low resolution, jpeg artifacts
```

## Index

| # | Style | Family | Quest | One line |
|---|---|---|---|---|
| 01 | [Tessera & Lime (owner baseline)](#01-tessera-lime) | Antique-rooted | H | The PC game's chosen look, carried into 3D: fresco planes and mosaic edges. |
| 02 | [Painted Marble & Gilt](#02-painted-marble) | Antique-rooted | M | Roman statuary as it really looked: bright polychrome pigment on carved marble. |
| 03 | [Pompeii Red Fresco](#03-pompeii-red) | Antique-rooted | H | The walls of a Pompeian villa as the whole world: deep red panels, black dados, painted columns. |
| 04 | [Bronze & Verdigris](#04-bronze-patina) | Antique-rooted | M | A world cast in bronze: patinated metal everything, spells as molten enamel light. |
| 05 | [Terracotta Figurines](#05-terracotta) | Antique-rooted | H | Hand-sculpted clay figurines and kiln-fired architecture, matte and tactile. |
| 06 | [Black-Figure Vase](#06-black-figure) | Antique-rooted | H | Ancient vase painting wrapped into 3D: black silhouettes with incised lines on orange clay. |
| 07 | [Gold Tesserae Nocturne](#07-gold-mosaic) | Antique-rooted | M | Late-antique gold mosaic shimmer: a night arena under domes of glittering tesserae. |
| 08 | [Encaustic Portrait Wax](#08-encaustic) | Antique-rooted | L | Warm wax-painted realism of ancient mummy portraits, intimate and haunting. |
| 09 | [Gouache Storybook](#09-gouache-storybook) | Painterly | H | Opaque gouache illustration: friendly, readable, rich colour, a picture-book arena. |
| 10 | [Illuminated Codex](#10-codex-margins) | Painterly | H | A world painted in the margins of a manuscript: gold-leaf initials, vellum, ink vines. |
| 11 | [Watercolour on Travertine](#11-travertine-wash) | Painterly | M | Loose watercolour washes over pale travertine: airy, rainy, melancholic. |
| 12 | [Woodcut Chronicle](#12-woodcut) | Painterly | H | Relief-print drama: carved black lines, two spot colours, chronicle-woodcut energy. |
| 13 | [Screen-Print Poster](#13-screenprint) | Painterly | H | Flat three-colour poster art, halftones and bold shapes, like a travel poster for the Games. |
| 14 | [Baroque Chiaroscuro](#14-chiaroscuro) | Painterly | L | Dark oil-painting drama: figures carved out of black by a single hard light. |
| 15 | [Comic Ink Cel](#15-ink-cel) | Painterly | H | Bold inked comic panels in 3D: heavy blacks, cel shading, kinetic spell lines. |
| 16 | [Low-Poly Faceted](#16-faceted) | Stylised 3D | H | Clean low-poly facets, gradient skies, a crisp modern-indie VR look. |
| 17 | [Stop-Motion Clay](#17-claymation) | Stylised 3D | H | Plasticine stop-motion: thumbprints, wobbly sets, handmade warmth. |
| 18 | [Layered Papercraft](#18-papercraft) | Stylised 3D | H | Cut-paper layers and paper-sculpture figures, shadows between sheets. |
| 19 | [Tabletop Miniature](#19-miniature) | Stylised 3D | M | Hand-painted wargame miniatures on a sand-table board, tilt-shift macro feel. |
| 20 | [Stained Glass & Lead](#20-stained-glass) | Stylised 3D | M | Light through coloured glass: lead lines, jewel tones, glowing translucency. |
| 21 | [Hand-Painted Stylised Fantasy](#21-handpainted) | Stylised 3D | H | Chunky hand-painted textures and bold proportions: a polished stylised fantasy game. |
| 22 | [Soft Cel Painted](#22-soft-cel) | Stylised 3D | H | Clean cel-shaded characters over lush painted backgrounds, cinematic and expressive. |
| 23 | [Torchlit Night Games](#23-torchlit-night) | Atmosphere | M | The Games after dark: torches, braziers, embers, and spells as the brightest lights. |
| 24 | [Sandstorm Sepia](#24-sandstorm-sepia) | Atmosphere | M | Dust-bleached arena in a golden haze, desaturated except the magic. |
| 25 | [Moonlit Silver Nocturne](#25-moonlit-silver) | Atmosphere | H | A blue-silver monochrome world where only magic has colour. |
| 26 | [Mixed-Reality Sand Circle](#26-mr-sand-circle) | Atmosphere | H | Passthrough: the arena floor and opponent appear inside your real room, Quest 3 mixed reality. |
| 27 | [Sacred Geometry Lightlines](#27-sacred-geometry) | Magical | H | Magic as light geometry: the arena and spells drawn as luminous vector lines. |
| 28 | [Astral Orrery](#28-astral-orrery) | Magical | M | The arena floats under a brass orrery and a star map; sigils are constellations. |
| 29 | [Living Water Glasswork](#29-living-water) | Magical | L | A world of clear glass and water: caustics, refractions, liquid architecture. |
| 30 | [Chalk & Slate](#30-chalk-slate) | Magical | H | A world drawn in chalk on slate: sigils, figures and arena as living diagrams. |

## Family: Antique-rooted

<a id="01-tessera-lime"></a>
### 01. Tessera & Lime (owner baseline)

*The PC game's chosen look, carried into 3D: fresco planes and mosaic edges.*

- **Palette:** chalk #EDE6D6, terracotta #B65A3A, ochre #C69A4A, mineral blue #3E7FA6, turquoise #7FC6C4, vermilion #D9472B
- **Quest feasibility:** **H** - Unlit/toon materials with painted albedo atlases and mosaic edge decals; baked light. Cheapest route on Quest and continuity with the PC game.
- **What to judge:** Does the fresco flatness still read with stereo depth? Do the hands separate from a busy mosaic background?

**01-VR** (16:9)

```
Style: hand-drawn digital fusion of Roman fresco and mosaic. Broken umber contours around flat chalk-lime and terracotta forms; small irregular painted tessera shapes cluster at architectural edges while open ground stays broad matte pigment. Mineral blue and pale turquoise carry water, vermilion carries fire, dull ochre carries stone and bronze. Flat fresco colour planes with rubbed plaster gaps, sun-bleached civic grandeur and captivity. First-person view from a seated player's eyes inside an open oval Roman-era sand arena, wide 100-degree field of view, eye height of a seated adult. In the lower foreground, the player's own two bare hands, no gloves, no devices: the right index finger traces a glowing circular sigil of water-light in the air, two concentric rings with one bold inner stroke, droplets trailing the fingertip; the left palm is raised forward, projecting a curved translucent ward shaped like a 140-degree arc of rippling water. A bronze prisoner's cuff on the left wrist has four inlaid runes, two of them lit. Three faint rune circles are set in the sand around the player's seat, within arm's reach. Eight metres ahead, a broad bearded fire mage in scorched crimson wool with a bronze collar hurls a vermilion fireball; a thin shrinking ring telegraphs where it will land. Tiered stone stands with a watching crowd, banners, and grey-robed arcane wardens on the wall. Clear readable silhouettes, the hands and the incoming spell are the focal points.
```

**01-CONCEPT** (2:3)

```
Style: hand-drawn digital fusion of Roman fresco and mosaic. Broken umber contours around flat chalk-lime and terracotta forms; small irregular painted tessera shapes cluster at architectural edges while open ground stays broad matte pigment. Mineral blue and pale turquoise carry water, vermilion carries fire, dull ochre carries stone and bronze. Flat fresco colour planes with rubbed plaster gaps, sun-bleached civic grandeur and captivity. Key art for an original Roman-era elemental-mage arena game. Cassia, a sharp-featured water mage woman in her thirties, dark hair bound with sea-blue cord, layered sea-blue and undyed linen robes, a bronze prisoner's collar with four inlaid runes, stands braced on arena sand drawing a large luminous sigil of concentric water rings with one hand while her other palm holds back a wave of vermilion fire thrown by a bearded fire mage in crimson. Behind them, towering stands, Roman standards, and grey-robed wardens watching from the wall. Dramatic, heroic, poster composition, strong silhouette.
```

**Negative (shared + extra):** `photorealistic, 3D render, glossy`

<a id="02-painted-marble"></a>
### 02. Painted Marble & Gilt

*Roman statuary as it really looked: bright polychrome pigment on carved marble.*

- **Palette:** marble #F2EEE6, Egyptian blue #1F4E9C, cinnabar #C0392B, malachite #2E8B57, gold leaf #D4AF37
- **Quest feasibility:** **M** - PBR-lite marble with baked AO and vertex-colour pigment; avoid real subsurface on Quest, fake it with a rim term.
- **What to judge:** Is it beautiful and original, or does it feel like a museum walk? Are magic effects still the most saturated thing on screen?

**02-VR** (16:9)

```
Style: polychrome painted marble sculpture come alive. Every figure and wall is carved white marble with bright ancient pigments brushed on: cinnabar red, Egyptian blue, malachite green, gold leaf on hems and collars. Subtle chisel marks, soft subsurface glow in the stone, crisp sculpted folds, museum-quality craft, warm daylight. First-person view from a seated player's eyes inside an open oval Roman-era sand arena, wide 100-degree field of view, eye height of a seated adult. In the lower foreground, the player's own two bare hands, no gloves, no devices: the right index finger traces a glowing circular sigil of water-light in the air, two concentric rings with one bold inner stroke, droplets trailing the fingertip; the left palm is raised forward, projecting a curved translucent ward shaped like a 140-degree arc of rippling water. A bronze prisoner's cuff on the left wrist has four inlaid runes, two of them lit. Three faint rune circles are set in the sand around the player's seat, within arm's reach. Eight metres ahead, a broad bearded fire mage in scorched crimson wool with a bronze collar hurls a vermilion fireball; a thin shrinking ring telegraphs where it will land. Tiered stone stands with a watching crowd, banners, and grey-robed arcane wardens on the wall. Clear readable silhouettes, the hands and the incoming spell are the focal points.
```

**02-CONCEPT** (2:3)

```
Style: polychrome painted marble sculpture come alive. Every figure and wall is carved white marble with bright ancient pigments brushed on: cinnabar red, Egyptian blue, malachite green, gold leaf on hems and collars. Subtle chisel marks, soft subsurface glow in the stone, crisp sculpted folds, museum-quality craft, warm daylight. Key art for an original Roman-era elemental-mage arena game. Cassia, a sharp-featured water mage woman in her thirties, dark hair bound with sea-blue cord, layered sea-blue and undyed linen robes, a bronze prisoner's collar with four inlaid runes, stands braced on arena sand drawing a large luminous sigil of concentric water rings with one hand while her other palm holds back a wave of vermilion fire thrown by a bearded fire mage in crimson. Behind them, towering stands, Roman standards, and grey-robed wardens watching from the wall. Dramatic, heroic, poster composition, strong silhouette.
```

**Negative (shared + extra):** `dull grey, plain white statue`

<a id="03-pompeii-red"></a>
### 03. Pompeii Red Fresco

*The walls of a Pompeian villa as the whole world: deep red panels, black dados, painted columns.*

- **Palette:** Pompeian red #A3241E, black #1B1714, ochre #CC8E35, cream #EADBC0, sea green #4F8A7A
- **Quest feasibility:** **H** - Unlit textured architecture with a single shared fresco atlas; figures as stylised low-poly with painted textures.
- **What to judge:** Red-heavy world vs vermilion fire: can the player still read the fire threat? (Likely needs fire shifted to orange-white.)

**03-VR** (16:9)

```
Style: Pompeian wall-painting world. Deep cinnabar-red panels framed by black dados and thin painted architectural borders, trompe-l'oeil columns and garlands, figures painted in confident ancient fresco brushwork with ochre skin tones and white highlights, slightly faded and crackled plaster surface, warm and theatrical. First-person view from a seated player's eyes inside an open oval Roman-era sand arena, wide 100-degree field of view, eye height of a seated adult. In the lower foreground, the player's own two bare hands, no gloves, no devices: the right index finger traces a glowing circular sigil of water-light in the air, two concentric rings with one bold inner stroke, droplets trailing the fingertip; the left palm is raised forward, projecting a curved translucent ward shaped like a 140-degree arc of rippling water. A bronze prisoner's cuff on the left wrist has four inlaid runes, two of them lit. Three faint rune circles are set in the sand around the player's seat, within arm's reach. Eight metres ahead, a broad bearded fire mage in scorched crimson wool with a bronze collar hurls a vermilion fireball; a thin shrinking ring telegraphs where it will land. Tiered stone stands with a watching crowd, banners, and grey-robed arcane wardens on the wall. Clear readable silhouettes, the hands and the incoming spell are the focal points.
```

**03-CONCEPT** (2:3)

```
Style: Pompeian wall-painting world. Deep cinnabar-red panels framed by black dados and thin painted architectural borders, trompe-l'oeil columns and garlands, figures painted in confident ancient fresco brushwork with ochre skin tones and white highlights, slightly faded and crackled plaster surface, warm and theatrical. Key art for an original Roman-era elemental-mage arena game. Cassia, a sharp-featured water mage woman in her thirties, dark hair bound with sea-blue cord, layered sea-blue and undyed linen robes, a bronze prisoner's collar with four inlaid runes, stands braced on arena sand drawing a large luminous sigil of concentric water rings with one hand while her other palm holds back a wave of vermilion fire thrown by a bearded fire mage in crimson. Behind them, towering stands, Roman standards, and grey-robed wardens watching from the wall. Dramatic, heroic, poster composition, strong silhouette.
```

**Negative (shared + extra):** `modern paint, glossy`

<a id="04-bronze-patina"></a>
### 04. Bronze & Verdigris

*A world cast in bronze: patinated metal everything, spells as molten enamel light.*

- **Palette:** bronze #8C6239, polished #D9A55B, verdigris #4A9C8C, enamel turquoise #3FD0D4, molten orange #FF7A1A
- **Quest feasibility:** **M** - Metal PBR with baked reflections (single cubemap); bloom only on spells. Readability risk: monochrome world needs strong value contrast.
- **What to judge:** Does a near-monochrome world make the threat colours pop, or does everything blur into brown?

**04-VR** (16:9)

```
Style: everything is cast bronze sculpture, arena walls, crowd, mages, with deep verdigris patina in the recesses and polished gold-bronze on worn edges. Spells are glowing vitreous enamel, water as luminous turquoise glass, fire as molten orange enamel. Heavy, sculptural, monumental, dramatic low sun glinting on metal. First-person view from a seated player's eyes inside an open oval Roman-era sand arena, wide 100-degree field of view, eye height of a seated adult. In the lower foreground, the player's own two bare hands, no gloves, no devices: the right index finger traces a glowing circular sigil of water-light in the air, two concentric rings with one bold inner stroke, droplets trailing the fingertip; the left palm is raised forward, projecting a curved translucent ward shaped like a 140-degree arc of rippling water. A bronze prisoner's cuff on the left wrist has four inlaid runes, two of them lit. Three faint rune circles are set in the sand around the player's seat, within arm's reach. Eight metres ahead, a broad bearded fire mage in scorched crimson wool with a bronze collar hurls a vermilion fireball; a thin shrinking ring telegraphs where it will land. Tiered stone stands with a watching crowd, banners, and grey-robed arcane wardens on the wall. Clear readable silhouettes, the hands and the incoming spell are the focal points.
```

**04-CONCEPT** (2:3)

```
Style: everything is cast bronze sculpture, arena walls, crowd, mages, with deep verdigris patina in the recesses and polished gold-bronze on worn edges. Spells are glowing vitreous enamel, water as luminous turquoise glass, fire as molten orange enamel. Heavy, sculptural, monumental, dramatic low sun glinting on metal. Key art for an original Roman-era elemental-mage arena game. Cassia, a sharp-featured water mage woman in her thirties, dark hair bound with sea-blue cord, layered sea-blue and undyed linen robes, a bronze prisoner's collar with four inlaid runes, stands braced on arena sand drawing a large luminous sigil of concentric water rings with one hand while her other palm holds back a wave of vermilion fire thrown by a bearded fire mage in crimson. Behind them, towering stands, Roman standards, and grey-robed wardens watching from the wall. Dramatic, heroic, poster composition, strong silhouette.
```

**Negative (shared + extra):** `rust, dirty, low contrast`

<a id="05-terracotta"></a>
### 05. Terracotta Figurines

*Hand-sculpted clay figurines and kiln-fired architecture, matte and tactile.*

- **Palette:** terracotta #C66B3D, slip white #EFE3D3, umber #6B4226, glaze blue #2F8FC1, glaze orange #F08A24
- **Quest feasibility:** **H** - Matte vertex-lit clay, normal maps for fingerprints, no reflections: very Quest-friendly.
- **What to judge:** Is the clay charm worth the loss of drama? Does it feel like a toy or like a world?

**05-VR** (16:9)

```
Style: hand-modelled terracotta clay figurines and architecture, kiln-fired orange-brown clay with fingerprint texture, traces of original white slip and faded pigment, chunky charming proportions, soft matte surfaces, gentle studio daylight, spells as bright painted glaze in sea-blue and flame orange. First-person view from a seated player's eyes inside an open oval Roman-era sand arena, wide 100-degree field of view, eye height of a seated adult. In the lower foreground, the player's own two bare hands, no gloves, no devices: the right index finger traces a glowing circular sigil of water-light in the air, two concentric rings with one bold inner stroke, droplets trailing the fingertip; the left palm is raised forward, projecting a curved translucent ward shaped like a 140-degree arc of rippling water. A bronze prisoner's cuff on the left wrist has four inlaid runes, two of them lit. Three faint rune circles are set in the sand around the player's seat, within arm's reach. Eight metres ahead, a broad bearded fire mage in scorched crimson wool with a bronze collar hurls a vermilion fireball; a thin shrinking ring telegraphs where it will land. Tiered stone stands with a watching crowd, banners, and grey-robed arcane wardens on the wall. Clear readable silhouettes, the hands and the incoming spell are the focal points.
```

**05-CONCEPT** (2:3)

```
Style: hand-modelled terracotta clay figurines and architecture, kiln-fired orange-brown clay with fingerprint texture, traces of original white slip and faded pigment, chunky charming proportions, soft matte surfaces, gentle studio daylight, spells as bright painted glaze in sea-blue and flame orange. Key art for an original Roman-era elemental-mage arena game. Cassia, a sharp-featured water mage woman in her thirties, dark hair bound with sea-blue cord, layered sea-blue and undyed linen robes, a bronze prisoner's collar with four inlaid runes, stands braced on arena sand drawing a large luminous sigil of concentric water rings with one hand while her other palm holds back a wave of vermilion fire thrown by a bearded fire mage in crimson. Behind them, towering stands, Roman standards, and grey-robed wardens watching from the wall. Dramatic, heroic, poster composition, strong silhouette.
```

**Negative (shared + extra):** `plastic, shiny`

<a id="06-black-figure"></a>
### 06. Black-Figure Vase

*Ancient vase painting wrapped into 3D: black silhouettes with incised lines on orange clay.*

- **Palette:** clay orange #D9853B, gloss black #151210, incised #E9A35E, white #F1E9DA, purple-red #7A2A3A
- **Quest feasibility:** **H** - Unlit two-tone shader with incised-line textures; silhouettes are ideal for hand-tracked readability.
- **What to judge:** Silhouette-only figures: can you read the opponent's casting pose at 8 m? Is it too abstract for a VR body?

**06-VR** (16:9)

```
Style: ancient black-figure vase painting translated into a three-dimensional world. Glossy black silhouette figures with fine incised orange lines for muscles and folds, set against warm orange clay surfaces, meander key borders on every architectural band, limited palette of black, orange clay, white and a touch of purple-red, spells drawn as flowing white line patterns. First-person view from a seated player's eyes inside an open oval Roman-era sand arena, wide 100-degree field of view, eye height of a seated adult. In the lower foreground, the player's own two bare hands, no gloves, no devices: the right index finger traces a glowing circular sigil of water-light in the air, two concentric rings with one bold inner stroke, droplets trailing the fingertip; the left palm is raised forward, projecting a curved translucent ward shaped like a 140-degree arc of rippling water. A bronze prisoner's cuff on the left wrist has four inlaid runes, two of them lit. Three faint rune circles are set in the sand around the player's seat, within arm's reach. Eight metres ahead, a broad bearded fire mage in scorched crimson wool with a bronze collar hurls a vermilion fireball; a thin shrinking ring telegraphs where it will land. Tiered stone stands with a watching crowd, banners, and grey-robed arcane wardens on the wall. Clear readable silhouettes, the hands and the incoming spell are the focal points.
```

**06-CONCEPT** (2:3)

```
Style: ancient black-figure vase painting translated into a three-dimensional world. Glossy black silhouette figures with fine incised orange lines for muscles and folds, set against warm orange clay surfaces, meander key borders on every architectural band, limited palette of black, orange clay, white and a touch of purple-red, spells drawn as flowing white line patterns. Key art for an original Roman-era elemental-mage arena game. Cassia, a sharp-featured water mage woman in her thirties, dark hair bound with sea-blue cord, layered sea-blue and undyed linen robes, a bronze prisoner's collar with four inlaid runes, stands braced on arena sand drawing a large luminous sigil of concentric water rings with one hand while her other palm holds back a wave of vermilion fire thrown by a bearded fire mage in crimson. Behind them, towering stands, Roman standards, and grey-robed wardens watching from the wall. Dramatic, heroic, poster composition, strong silhouette.
```

**Negative (shared + extra):** `photorealistic, gradients`

<a id="07-gold-mosaic"></a>
### 07. Gold Tesserae Nocturne

*Late-antique gold mosaic shimmer: a night arena under domes of glittering tesserae.*

- **Palette:** gold #C9A24A, lapis #22356F, night #0F1530, ivory #EDE3CC, ruby #9B1B30
- **Quest feasibility:** **M** - Mosaic as normal-mapped tiles with a cheap glint shader; restrict glitter to the backdrop so it doesn't shimmer in stereo.
- **What to judge:** Does the glitter cause stereo shimmer discomfort? Is it too solemn for a fight?

**07-VR** (16:9)

```
Style: late-antique gold mosaic world at dusk. Domes and walls covered in glittering gold and lapis tesserae that catch torchlight, figures rendered as mosaic with stylised almond eyes and flat drapery, deep blue night sky set with gold star tiles, spells sparkle as moving tesserae, solemn and sacred. First-person view from a seated player's eyes inside an open oval Roman-era sand arena, wide 100-degree field of view, eye height of a seated adult. In the lower foreground, the player's own two bare hands, no gloves, no devices: the right index finger traces a glowing circular sigil of water-light in the air, two concentric rings with one bold inner stroke, droplets trailing the fingertip; the left palm is raised forward, projecting a curved translucent ward shaped like a 140-degree arc of rippling water. A bronze prisoner's cuff on the left wrist has four inlaid runes, two of them lit. Three faint rune circles are set in the sand around the player's seat, within arm's reach. Eight metres ahead, a broad bearded fire mage in scorched crimson wool with a bronze collar hurls a vermilion fireball; a thin shrinking ring telegraphs where it will land. Tiered stone stands with a watching crowd, banners, and grey-robed arcane wardens on the wall. Clear readable silhouettes, the hands and the incoming spell are the focal points.
```

**07-CONCEPT** (2:3)

```
Style: late-antique gold mosaic world at dusk. Domes and walls covered in glittering gold and lapis tesserae that catch torchlight, figures rendered as mosaic with stylised almond eyes and flat drapery, deep blue night sky set with gold star tiles, spells sparkle as moving tesserae, solemn and sacred. Key art for an original Roman-era elemental-mage arena game. Cassia, a sharp-featured water mage woman in her thirties, dark hair bound with sea-blue cord, layered sea-blue and undyed linen robes, a bronze prisoner's collar with four inlaid runes, stands braced on arena sand drawing a large luminous sigil of concentric water rings with one hand while her other palm holds back a wave of vermilion fire thrown by a bearded fire mage in crimson. Behind them, towering stands, Roman standards, and grey-robed wardens watching from the wall. Dramatic, heroic, poster composition, strong silhouette.
```

**Negative (shared + extra):** `modern, flat vector`

<a id="08-encaustic"></a>
### 08. Encaustic Portrait Wax

*Warm wax-painted realism of ancient mummy portraits, intimate and haunting.*

- **Palette:** honey #C8914F, umber #3B2A1E, olive #6E6B3A, gold #C59B3C, blood red #7E1E1E
- **Quest feasibility:** **L** - Painterly realism is expensive on Quest; achievable only as painted textures on modest geometry, no dynamic lights. Good for portraits and key art more than the arena.
- **What to judge:** Is this a key-art/portrait style only? Would it survive as a real-time world?

**08-VR** (16:9)

```
Style: ancient encaustic wax painting, thick translucent beeswax pigment laid with a hot spatula, visible wax ridges, warm honeyed skin tones, large expressive eyes, dark umber backgrounds, gold leaf accents on collars and wreaths, intimate soulful realism with a hand-made antique surface. First-person view from a seated player's eyes inside an open oval Roman-era sand arena, wide 100-degree field of view, eye height of a seated adult. In the lower foreground, the player's own two bare hands, no gloves, no devices: the right index finger traces a glowing circular sigil of water-light in the air, two concentric rings with one bold inner stroke, droplets trailing the fingertip; the left palm is raised forward, projecting a curved translucent ward shaped like a 140-degree arc of rippling water. A bronze prisoner's cuff on the left wrist has four inlaid runes, two of them lit. Three faint rune circles are set in the sand around the player's seat, within arm's reach. Eight metres ahead, a broad bearded fire mage in scorched crimson wool with a bronze collar hurls a vermilion fireball; a thin shrinking ring telegraphs where it will land. Tiered stone stands with a watching crowd, banners, and grey-robed arcane wardens on the wall. Clear readable silhouettes, the hands and the incoming spell are the focal points.
```

**08-CONCEPT** (2:3)

```
Style: ancient encaustic wax painting, thick translucent beeswax pigment laid with a hot spatula, visible wax ridges, warm honeyed skin tones, large expressive eyes, dark umber backgrounds, gold leaf accents on collars and wreaths, intimate soulful realism with a hand-made antique surface. Key art for an original Roman-era elemental-mage arena game. Cassia, a sharp-featured water mage woman in her thirties, dark hair bound with sea-blue cord, layered sea-blue and undyed linen robes, a bronze prisoner's collar with four inlaid runes, stands braced on arena sand drawing a large luminous sigil of concentric water rings with one hand while her other palm holds back a wave of vermilion fire thrown by a bearded fire mage in crimson. Behind them, towering stands, Roman standards, and grey-robed wardens watching from the wall. Dramatic, heroic, poster composition, strong silhouette.
```

**Negative (shared + extra):** `cartoon, cel shading`

## Family: Painterly

<a id="09-gouache-storybook"></a>
### 09. Gouache Storybook

*Opaque gouache illustration: friendly, readable, rich colour, a picture-book arena.*

- **Palette:** sand #E8C98A, sky #8FC3E0, crimson #C2402F, sea #2C7DA0, leaf #6A9A4A
- **Quest feasibility:** **H** - Hand-painted albedo textures, unlit or soft-lit, painted skybox: the classic stylised-VR recipe.
- **What to judge:** Does it feel too young for a story about captivity and betrayal?

**09-VR** (16:9)

```
Style: opaque gouache storybook illustration, chunky brush strokes, matte layered paint, soft rounded shapes, warm sunlight with cool blue shadows, slightly exaggerated proportions, rich saturated but harmonious colours, hand-painted texture visible everywhere, charming and adventurous. First-person view from a seated player's eyes inside an open oval Roman-era sand arena, wide 100-degree field of view, eye height of a seated adult. In the lower foreground, the player's own two bare hands, no gloves, no devices: the right index finger traces a glowing circular sigil of water-light in the air, two concentric rings with one bold inner stroke, droplets trailing the fingertip; the left palm is raised forward, projecting a curved translucent ward shaped like a 140-degree arc of rippling water. A bronze prisoner's cuff on the left wrist has four inlaid runes, two of them lit. Three faint rune circles are set in the sand around the player's seat, within arm's reach. Eight metres ahead, a broad bearded fire mage in scorched crimson wool with a bronze collar hurls a vermilion fireball; a thin shrinking ring telegraphs where it will land. Tiered stone stands with a watching crowd, banners, and grey-robed arcane wardens on the wall. Clear readable silhouettes, the hands and the incoming spell are the focal points.
```

**09-CONCEPT** (2:3)

```
Style: opaque gouache storybook illustration, chunky brush strokes, matte layered paint, soft rounded shapes, warm sunlight with cool blue shadows, slightly exaggerated proportions, rich saturated but harmonious colours, hand-painted texture visible everywhere, charming and adventurous. Key art for an original Roman-era elemental-mage arena game. Cassia, a sharp-featured water mage woman in her thirties, dark hair bound with sea-blue cord, layered sea-blue and undyed linen robes, a bronze prisoner's collar with four inlaid runes, stands braced on arena sand drawing a large luminous sigil of concentric water rings with one hand while her other palm holds back a wave of vermilion fire thrown by a bearded fire mage in crimson. Behind them, towering stands, Roman standards, and grey-robed wardens watching from the wall. Dramatic, heroic, poster composition, strong silhouette.
```

**Negative (shared + extra):** `photorealistic, harsh`

<a id="10-codex-margins"></a>
### 10. Illuminated Codex

*A world painted in the margins of a manuscript: gold-leaf initials, vellum, ink vines.*

- **Palette:** vellum #EADCB8, iron ink #2B2118, lapis #26428B, vermilion #D23B26, gold #D2A93E
- **Quest feasibility:** **H** - Unlit inked shader with vellum overlay; gilded sigils map perfectly to drawn-gesture trails.
- **What to judge:** Strong match for drawn sigils. Does the vellum ground make depth flat in the headset?

**10-VR** (16:9)

```
Style: illuminated manuscript brought to life, warm vellum ground, iron-gall ink outlines, flat egg-tempera colours in lapis, vermilion and verdigris, burnished gold leaf on halos of magic and on borders, delicate ink vine ornaments framing the scene, spells drawn as gilded calligraphic loops. First-person view from a seated player's eyes inside an open oval Roman-era sand arena, wide 100-degree field of view, eye height of a seated adult. In the lower foreground, the player's own two bare hands, no gloves, no devices: the right index finger traces a glowing circular sigil of water-light in the air, two concentric rings with one bold inner stroke, droplets trailing the fingertip; the left palm is raised forward, projecting a curved translucent ward shaped like a 140-degree arc of rippling water. A bronze prisoner's cuff on the left wrist has four inlaid runes, two of them lit. Three faint rune circles are set in the sand around the player's seat, within arm's reach. Eight metres ahead, a broad bearded fire mage in scorched crimson wool with a bronze collar hurls a vermilion fireball; a thin shrinking ring telegraphs where it will land. Tiered stone stands with a watching crowd, banners, and grey-robed arcane wardens on the wall. Clear readable silhouettes, the hands and the incoming spell are the focal points.
```

**10-CONCEPT** (2:3)

```
Style: illuminated manuscript brought to life, warm vellum ground, iron-gall ink outlines, flat egg-tempera colours in lapis, vermilion and verdigris, burnished gold leaf on halos of magic and on borders, delicate ink vine ornaments framing the scene, spells drawn as gilded calligraphic loops. Key art for an original Roman-era elemental-mage arena game. Cassia, a sharp-featured water mage woman in her thirties, dark hair bound with sea-blue cord, layered sea-blue and undyed linen robes, a bronze prisoner's collar with four inlaid runes, stands braced on arena sand drawing a large luminous sigil of concentric water rings with one hand while her other palm holds back a wave of vermilion fire thrown by a bearded fire mage in crimson. Behind them, towering stands, Roman standards, and grey-robed wardens watching from the wall. Dramatic, heroic, poster composition, strong silhouette.
```

**Negative (shared + extra):** `photo, 3D render`

<a id="11-travertine-wash"></a>
### 11. Watercolour on Travertine

*Loose watercolour washes over pale travertine: airy, rainy, melancholic.*

- **Palette:** travertine #DCD3C2, wash grey #9AA3A6, ink #3D3F45, water #3C8DBC, fire #E2552D
- **Quest feasibility:** **M** - Watercolour post-process is costly; fake with painted textures and a paper overlay. Edges blur in stereo: test it.
- **What to judge:** Do soft bleeding edges hurt depth reading in VR? Is the melancholy right for the game?

**11-VR** (16:9)

```
Style: loose watercolour illustration on rough cold-pressed paper, pale travertine stone washes, granulating pigments, soft wet-in-wet bleeding edges, pencil underdrawing visible in places, rain-damp atmosphere, restrained palette of warm greys with saturated accents only for magic. First-person view from a seated player's eyes inside an open oval Roman-era sand arena, wide 100-degree field of view, eye height of a seated adult. In the lower foreground, the player's own two bare hands, no gloves, no devices: the right index finger traces a glowing circular sigil of water-light in the air, two concentric rings with one bold inner stroke, droplets trailing the fingertip; the left palm is raised forward, projecting a curved translucent ward shaped like a 140-degree arc of rippling water. A bronze prisoner's cuff on the left wrist has four inlaid runes, two of them lit. Three faint rune circles are set in the sand around the player's seat, within arm's reach. Eight metres ahead, a broad bearded fire mage in scorched crimson wool with a bronze collar hurls a vermilion fireball; a thin shrinking ring telegraphs where it will land. Tiered stone stands with a watching crowd, banners, and grey-robed arcane wardens on the wall. Clear readable silhouettes, the hands and the incoming spell are the focal points.
```

**11-CONCEPT** (2:3)

```
Style: loose watercolour illustration on rough cold-pressed paper, pale travertine stone washes, granulating pigments, soft wet-in-wet bleeding edges, pencil underdrawing visible in places, rain-damp atmosphere, restrained palette of warm greys with saturated accents only for magic. Key art for an original Roman-era elemental-mage arena game. Cassia, a sharp-featured water mage woman in her thirties, dark hair bound with sea-blue cord, layered sea-blue and undyed linen robes, a bronze prisoner's collar with four inlaid runes, stands braced on arena sand drawing a large luminous sigil of concentric water rings with one hand while her other palm holds back a wave of vermilion fire thrown by a bearded fire mage in crimson. Behind them, towering stands, Roman standards, and grey-robed wardens watching from the wall. Dramatic, heroic, poster composition, strong silhouette.
```

**Negative (shared + extra):** `harsh outlines, neon`

<a id="12-woodcut"></a>
### 12. Woodcut Chronicle

*Relief-print drama: carved black lines, two spot colours, chronicle-woodcut energy.*

- **Palette:** paper #EFE6D2, ink #141210, sea blue #2B6CB0, vermilion #D9381E
- **Quest feasibility:** **H** - Unlit hatching shader with screen- or object-space hatch textures; two spot colours make threats unmissable.
- **What to judge:** Hatching can shimmer in VR; does it hold up at 72 Hz? Is two-colour too austere?

**12-VR** (16:9)

```
Style: hand-carved woodcut relief print, bold black gouged lines and hatching, rough paper grain, only two spot colours printed slightly off-register, sea-blue for water and vermilion for fire, dramatic carved light rays, heroic chronicle-illustration energy. First-person view from a seated player's eyes inside an open oval Roman-era sand arena, wide 100-degree field of view, eye height of a seated adult. In the lower foreground, the player's own two bare hands, no gloves, no devices: the right index finger traces a glowing circular sigil of water-light in the air, two concentric rings with one bold inner stroke, droplets trailing the fingertip; the left palm is raised forward, projecting a curved translucent ward shaped like a 140-degree arc of rippling water. A bronze prisoner's cuff on the left wrist has four inlaid runes, two of them lit. Three faint rune circles are set in the sand around the player's seat, within arm's reach. Eight metres ahead, a broad bearded fire mage in scorched crimson wool with a bronze collar hurls a vermilion fireball; a thin shrinking ring telegraphs where it will land. Tiered stone stands with a watching crowd, banners, and grey-robed arcane wardens on the wall. Clear readable silhouettes, the hands and the incoming spell are the focal points.
```

**12-CONCEPT** (2:3)

```
Style: hand-carved woodcut relief print, bold black gouged lines and hatching, rough paper grain, only two spot colours printed slightly off-register, sea-blue for water and vermilion for fire, dramatic carved light rays, heroic chronicle-illustration energy. Key art for an original Roman-era elemental-mage arena game. Cassia, a sharp-featured water mage woman in her thirties, dark hair bound with sea-blue cord, layered sea-blue and undyed linen robes, a bronze prisoner's collar with four inlaid runes, stands braced on arena sand drawing a large luminous sigil of concentric water rings with one hand while her other palm holds back a wave of vermilion fire thrown by a bearded fire mage in crimson. Behind them, towering stands, Roman standards, and grey-robed wardens watching from the wall. Dramatic, heroic, poster composition, strong silhouette.
```

**Negative (shared + extra):** `smooth gradients, photorealistic`

<a id="13-screenprint"></a>
### 13. Screen-Print Poster

*Flat three-colour poster art, halftones and bold shapes, like a travel poster for the Games.*

- **Palette:** paper #F1E7CF, teal #1E6F73, sunset orange #E8743B, deep plum #3B2340, mustard #E2B33C
- **Quest feasibility:** **H** - Flat unlit colours with a halftone shadow texture; minimal draw cost and very legible.
- **What to judge:** Graphic and cheap: is it distinctive enough for a craft award?

**13-VR** (16:9)

```
Style: vintage screen-print poster, flat bold shapes, four colours only plus paper, visible halftone dots in the shadows, slight misregistration, strong graphic composition, sun disc and long shadows, heroic simplified figures. First-person view from a seated player's eyes inside an open oval Roman-era sand arena, wide 100-degree field of view, eye height of a seated adult. In the lower foreground, the player's own two bare hands, no gloves, no devices: the right index finger traces a glowing circular sigil of water-light in the air, two concentric rings with one bold inner stroke, droplets trailing the fingertip; the left palm is raised forward, projecting a curved translucent ward shaped like a 140-degree arc of rippling water. A bronze prisoner's cuff on the left wrist has four inlaid runes, two of them lit. Three faint rune circles are set in the sand around the player's seat, within arm's reach. Eight metres ahead, a broad bearded fire mage in scorched crimson wool with a bronze collar hurls a vermilion fireball; a thin shrinking ring telegraphs where it will land. Tiered stone stands with a watching crowd, banners, and grey-robed arcane wardens on the wall. Clear readable silhouettes, the hands and the incoming spell are the focal points.
```

**13-CONCEPT** (2:3)

```
Style: vintage screen-print poster, flat bold shapes, four colours only plus paper, visible halftone dots in the shadows, slight misregistration, strong graphic composition, sun disc and long shadows, heroic simplified figures. Key art for an original Roman-era elemental-mage arena game. Cassia, a sharp-featured water mage woman in her thirties, dark hair bound with sea-blue cord, layered sea-blue and undyed linen robes, a bronze prisoner's collar with four inlaid runes, stands braced on arena sand drawing a large luminous sigil of concentric water rings with one hand while her other palm holds back a wave of vermilion fire thrown by a bearded fire mage in crimson. Behind them, towering stands, Roman standards, and grey-robed wardens watching from the wall. Dramatic, heroic, poster composition, strong silhouette.
```

**Negative (shared + extra):** `photorealistic, noisy`

<a id="14-chiaroscuro"></a>
### 14. Baroque Chiaroscuro

*Dark oil-painting drama: figures carved out of black by a single hard light.*

- **Palette:** shadow #120E0B, flesh #C68B5E, umber #4B3423, gold light #F2C46D, sea light #4FB3D9
- **Quest feasibility:** **L** - Needs strong dynamic lighting to sell; on Quest it must be baked with magic as emissive. High risk of muddy darks in the headset.
- **What to judge:** Dark scenes on Quest LCDs lose detail: is the drama worth the risk?

**14-VR** (16:9)

```
Style: dark baroque oil painting, dramatic chiaroscuro, a single hard warm light carving figures out of deep shadow, rich glazes, visible impasto in the highlights, gritty realistic textures, sweat and dust, the magic as the brightest light source in the scene. First-person view from a seated player's eyes inside an open oval Roman-era sand arena, wide 100-degree field of view, eye height of a seated adult. In the lower foreground, the player's own two bare hands, no gloves, no devices: the right index finger traces a glowing circular sigil of water-light in the air, two concentric rings with one bold inner stroke, droplets trailing the fingertip; the left palm is raised forward, projecting a curved translucent ward shaped like a 140-degree arc of rippling water. A bronze prisoner's cuff on the left wrist has four inlaid runes, two of them lit. Three faint rune circles are set in the sand around the player's seat, within arm's reach. Eight metres ahead, a broad bearded fire mage in scorched crimson wool with a bronze collar hurls a vermilion fireball; a thin shrinking ring telegraphs where it will land. Tiered stone stands with a watching crowd, banners, and grey-robed arcane wardens on the wall. Clear readable silhouettes, the hands and the incoming spell are the focal points.
```

**14-CONCEPT** (2:3)

```
Style: dark baroque oil painting, dramatic chiaroscuro, a single hard warm light carving figures out of deep shadow, rich glazes, visible impasto in the highlights, gritty realistic textures, sweat and dust, the magic as the brightest light source in the scene. Key art for an original Roman-era elemental-mage arena game. Cassia, a sharp-featured water mage woman in her thirties, dark hair bound with sea-blue cord, layered sea-blue and undyed linen robes, a bronze prisoner's collar with four inlaid runes, stands braced on arena sand drawing a large luminous sigil of concentric water rings with one hand while her other palm holds back a wave of vermilion fire thrown by a bearded fire mage in crimson. Behind them, towering stands, Roman standards, and grey-robed wardens watching from the wall. Dramatic, heroic, poster composition, strong silhouette.
```

**Negative (shared + extra):** `flat colours, cartoon`

<a id="15-ink-cel"></a>
### 15. Comic Ink Cel

*Bold inked comic panels in 3D: heavy blacks, cel shading, kinetic spell lines.*

- **Palette:** ink #111111, sand #E3C07E, crimson #C62828, sea #1976D2, white #FAFAFA
- **Quest feasibility:** **H** - Inverted-hull outlines + two-tone ramp shader: proven stylised-VR technique, very cheap.
- **What to judge:** Outlines on hands help tracking readability. Is the comic energy on-tone?

**15-VR** (16:9)

```
Style: bold inked comic art, thick confident black outlines, heavy spot blacks, two-tone cel shading, kinetic speed lines and impact bursts around spells, saturated flat colours, dynamic foreshortening, gritty but clean. First-person view from a seated player's eyes inside an open oval Roman-era sand arena, wide 100-degree field of view, eye height of a seated adult. In the lower foreground, the player's own two bare hands, no gloves, no devices: the right index finger traces a glowing circular sigil of water-light in the air, two concentric rings with one bold inner stroke, droplets trailing the fingertip; the left palm is raised forward, projecting a curved translucent ward shaped like a 140-degree arc of rippling water. A bronze prisoner's cuff on the left wrist has four inlaid runes, two of them lit. Three faint rune circles are set in the sand around the player's seat, within arm's reach. Eight metres ahead, a broad bearded fire mage in scorched crimson wool with a bronze collar hurls a vermilion fireball; a thin shrinking ring telegraphs where it will land. Tiered stone stands with a watching crowd, banners, and grey-robed arcane wardens on the wall. Clear readable silhouettes, the hands and the incoming spell are the focal points.
```

**15-CONCEPT** (2:3)

```
Style: bold inked comic art, thick confident black outlines, heavy spot blacks, two-tone cel shading, kinetic speed lines and impact bursts around spells, saturated flat colours, dynamic foreshortening, gritty but clean. Key art for an original Roman-era elemental-mage arena game. Cassia, a sharp-featured water mage woman in her thirties, dark hair bound with sea-blue cord, layered sea-blue and undyed linen robes, a bronze prisoner's collar with four inlaid runes, stands braced on arena sand drawing a large luminous sigil of concentric water rings with one hand while her other palm holds back a wave of vermilion fire thrown by a bearded fire mage in crimson. Behind them, towering stands, Roman standards, and grey-robed wardens watching from the wall. Dramatic, heroic, poster composition, strong silhouette.
```

**Negative (shared + extra):** `photorealistic, soft blur`

## Family: Stylised 3D

<a id="16-faceted"></a>
### 16. Low-Poly Faceted

*Clean low-poly facets, gradient skies, a crisp modern-indie VR look.*

- **Palette:** sand #E9C88C, stone #B9A58C, sky top #7DB7E8, sky low #F6D6A8, water #2AA7D9, fire #FF6A3D
- **Quest feasibility:** **H** - Flat-shaded low-poly is the fastest to build and to hit 72 Hz; the risk is looking generic.
- **What to judge:** The safest technical choice: does it have any soul?

**16-VR** (16:9)

```
Style: clean low-poly 3D, visible flat-shaded facets, soft gradient sky, simple geometric figures with expressive poses, crisp ambient occlusion, pastel-warm sand and stone with saturated magic effects, minimalist and elegant indie game look. First-person view from a seated player's eyes inside an open oval Roman-era sand arena, wide 100-degree field of view, eye height of a seated adult. In the lower foreground, the player's own two bare hands, no gloves, no devices: the right index finger traces a glowing circular sigil of water-light in the air, two concentric rings with one bold inner stroke, droplets trailing the fingertip; the left palm is raised forward, projecting a curved translucent ward shaped like a 140-degree arc of rippling water. A bronze prisoner's cuff on the left wrist has four inlaid runes, two of them lit. Three faint rune circles are set in the sand around the player's seat, within arm's reach. Eight metres ahead, a broad bearded fire mage in scorched crimson wool with a bronze collar hurls a vermilion fireball; a thin shrinking ring telegraphs where it will land. Tiered stone stands with a watching crowd, banners, and grey-robed arcane wardens on the wall. Clear readable silhouettes, the hands and the incoming spell are the focal points.
```

**16-CONCEPT** (2:3)

```
Style: clean low-poly 3D, visible flat-shaded facets, soft gradient sky, simple geometric figures with expressive poses, crisp ambient occlusion, pastel-warm sand and stone with saturated magic effects, minimalist and elegant indie game look. Key art for an original Roman-era elemental-mage arena game. Cassia, a sharp-featured water mage woman in her thirties, dark hair bound with sea-blue cord, layered sea-blue and undyed linen robes, a bronze prisoner's collar with four inlaid runes, stands braced on arena sand drawing a large luminous sigil of concentric water rings with one hand while her other palm holds back a wave of vermilion fire thrown by a bearded fire mage in crimson. Behind them, towering stands, Roman standards, and grey-robed wardens watching from the wall. Dramatic, heroic, poster composition, strong silhouette.
```

**Negative (shared + extra):** `high detail texture, photorealistic`

<a id="17-claymation"></a>
### 17. Stop-Motion Clay

*Plasticine stop-motion: thumbprints, wobbly sets, handmade warmth.*

- **Palette:** plasticine sand #E2B97A, crimson #B8322A, sea #2D78B8, cardboard #A9875A, cotton #F4F1EC
- **Quest feasibility:** **H** - Matte clay shading with normal-mapped thumbprints; animate on twos for the stop-motion feel.
- **What to judge:** Is handmade charm right for a betrayal story, or does it undercut the stakes?

**17-VR** (16:9)

```
Style: stop-motion claymation, plasticine figures with visible thumbprints and slight lumps, handmade miniature set with cardboard and real sand, soft studio lighting with gentle shadows, charming slightly crooked proportions, spells as translucent coloured resin and cotton-wool smoke. First-person view from a seated player's eyes inside an open oval Roman-era sand arena, wide 100-degree field of view, eye height of a seated adult. In the lower foreground, the player's own two bare hands, no gloves, no devices: the right index finger traces a glowing circular sigil of water-light in the air, two concentric rings with one bold inner stroke, droplets trailing the fingertip; the left palm is raised forward, projecting a curved translucent ward shaped like a 140-degree arc of rippling water. A bronze prisoner's cuff on the left wrist has four inlaid runes, two of them lit. Three faint rune circles are set in the sand around the player's seat, within arm's reach. Eight metres ahead, a broad bearded fire mage in scorched crimson wool with a bronze collar hurls a vermilion fireball; a thin shrinking ring telegraphs where it will land. Tiered stone stands with a watching crowd, banners, and grey-robed arcane wardens on the wall. Clear readable silhouettes, the hands and the incoming spell are the focal points.
```

**17-CONCEPT** (2:3)

```
Style: stop-motion claymation, plasticine figures with visible thumbprints and slight lumps, handmade miniature set with cardboard and real sand, soft studio lighting with gentle shadows, charming slightly crooked proportions, spells as translucent coloured resin and cotton-wool smoke. Key art for an original Roman-era elemental-mage arena game. Cassia, a sharp-featured water mage woman in her thirties, dark hair bound with sea-blue cord, layered sea-blue and undyed linen robes, a bronze prisoner's collar with four inlaid runes, stands braced on arena sand drawing a large luminous sigil of concentric water rings with one hand while her other palm holds back a wave of vermilion fire thrown by a bearded fire mage in crimson. Behind them, towering stands, Roman standards, and grey-robed wardens watching from the wall. Dramatic, heroic, poster composition, strong silhouette.
```

**Negative (shared + extra):** `glossy, cel shading`

<a id="18-papercraft"></a>
### 18. Layered Papercraft

*Cut-paper layers and paper-sculpture figures, shadows between sheets.*

- **Palette:** kraft #C9A57A, cream #F3EAD8, indigo #2D3E73, coral #E5674E, teal #2A9D8F
- **Quest feasibility:** **H** - Unlit planes with paper textures; stacked layers give strong stereo depth for free.
- **What to judge:** Strong stereo depth from layers; does paper feel too fragile for fire magic?

**18-VR** (16:9)

```
Style: layered cut-paper diorama, figures and arena built from folded and cut coloured paper with crisp edges, soft drop shadows between paper layers, visible paper fibre texture, warm light from above, spells as curling paper ribbons and confetti-like droplets. First-person view from a seated player's eyes inside an open oval Roman-era sand arena, wide 100-degree field of view, eye height of a seated adult. In the lower foreground, the player's own two bare hands, no gloves, no devices: the right index finger traces a glowing circular sigil of water-light in the air, two concentric rings with one bold inner stroke, droplets trailing the fingertip; the left palm is raised forward, projecting a curved translucent ward shaped like a 140-degree arc of rippling water. A bronze prisoner's cuff on the left wrist has four inlaid runes, two of them lit. Three faint rune circles are set in the sand around the player's seat, within arm's reach. Eight metres ahead, a broad bearded fire mage in scorched crimson wool with a bronze collar hurls a vermilion fireball; a thin shrinking ring telegraphs where it will land. Tiered stone stands with a watching crowd, banners, and grey-robed arcane wardens on the wall. Clear readable silhouettes, the hands and the incoming spell are the focal points.
```

**18-CONCEPT** (2:3)

```
Style: layered cut-paper diorama, figures and arena built from folded and cut coloured paper with crisp edges, soft drop shadows between paper layers, visible paper fibre texture, warm light from above, spells as curling paper ribbons and confetti-like droplets. Key art for an original Roman-era elemental-mage arena game. Cassia, a sharp-featured water mage woman in her thirties, dark hair bound with sea-blue cord, layered sea-blue and undyed linen robes, a bronze prisoner's collar with four inlaid runes, stands braced on arena sand drawing a large luminous sigil of concentric water rings with one hand while her other palm holds back a wave of vermilion fire thrown by a bearded fire mage in crimson. Behind them, towering stands, Roman standards, and grey-robed wardens watching from the wall. Dramatic, heroic, poster composition, strong silhouette.
```

**Negative (shared + extra):** `photorealistic, metal`

<a id="19-miniature"></a>
### 19. Tabletop Miniature

*Hand-painted wargame miniatures on a sand-table board, tilt-shift macro feel.*

- **Palette:** board sand #D8B47A, crimson #A8231F, sea blue #1F6FB2, bronze #A87632, flock green #6B7F3A
- **Quest feasibility:** **M** - Painted-miniature textures are cheap; fake depth of field in VR is a comfort risk, so keep it for screenshots only.
- **What to judge:** The DOF look cannot exist in a headset: is the style still good without it?

**19-VR** (16:9)

```
Style: hand-painted tabletop wargame miniatures, crisp brushed highlights and washes, slightly glossy varnish, flocked sand base and sculpted terrain, macro photography with shallow depth of field, rich saturated paint, spells as clear tinted resin effects. First-person view from a seated player's eyes inside an open oval Roman-era sand arena, wide 100-degree field of view, eye height of a seated adult. In the lower foreground, the player's own two bare hands, no gloves, no devices: the right index finger traces a glowing circular sigil of water-light in the air, two concentric rings with one bold inner stroke, droplets trailing the fingertip; the left palm is raised forward, projecting a curved translucent ward shaped like a 140-degree arc of rippling water. A bronze prisoner's cuff on the left wrist has four inlaid runes, two of them lit. Three faint rune circles are set in the sand around the player's seat, within arm's reach. Eight metres ahead, a broad bearded fire mage in scorched crimson wool with a bronze collar hurls a vermilion fireball; a thin shrinking ring telegraphs where it will land. Tiered stone stands with a watching crowd, banners, and grey-robed arcane wardens on the wall. Clear readable silhouettes, the hands and the incoming spell are the focal points.
```

**19-CONCEPT** (2:3)

```
Style: hand-painted tabletop wargame miniatures, crisp brushed highlights and washes, slightly glossy varnish, flocked sand base and sculpted terrain, macro photography with shallow depth of field, rich saturated paint, spells as clear tinted resin effects. Key art for an original Roman-era elemental-mage arena game. Cassia, a sharp-featured water mage woman in her thirties, dark hair bound with sea-blue cord, layered sea-blue and undyed linen robes, a bronze prisoner's collar with four inlaid runes, stands braced on arena sand drawing a large luminous sigil of concentric water rings with one hand while her other palm holds back a wave of vermilion fire thrown by a bearded fire mage in crimson. Behind them, towering stands, Roman standards, and grey-robed wardens watching from the wall. Dramatic, heroic, poster composition, strong silhouette.
```

**Negative (shared + extra):** `full-size realistic`

<a id="20-stained-glass"></a>
### 20. Stained Glass & Lead

*Light through coloured glass: lead lines, jewel tones, glowing translucency.*

- **Palette:** lead #1E1E24, ruby #B3122E, sapphire #1E4FA3, amber #E3A21A, emerald #1E8F5A
- **Quest feasibility:** **M** - Emissive glass with dark outlines is cheap if opaque; real translucency and light pools are costly, so bake them.
- **What to judge:** Luminous everywhere: does the magic still stand out when the world itself glows?

**20-VR** (16:9)

```
Style: stained-glass world, every surface made of coloured glass panes held by dark lead lines, jewel tones glowing with backlight, figures as leaded glass mosaics, sun beams casting coloured light pools on the sand, spells as molten glowing glass, sacred and luminous. First-person view from a seated player's eyes inside an open oval Roman-era sand arena, wide 100-degree field of view, eye height of a seated adult. In the lower foreground, the player's own two bare hands, no gloves, no devices: the right index finger traces a glowing circular sigil of water-light in the air, two concentric rings with one bold inner stroke, droplets trailing the fingertip; the left palm is raised forward, projecting a curved translucent ward shaped like a 140-degree arc of rippling water. A bronze prisoner's cuff on the left wrist has four inlaid runes, two of them lit. Three faint rune circles are set in the sand around the player's seat, within arm's reach. Eight metres ahead, a broad bearded fire mage in scorched crimson wool with a bronze collar hurls a vermilion fireball; a thin shrinking ring telegraphs where it will land. Tiered stone stands with a watching crowd, banners, and grey-robed arcane wardens on the wall. Clear readable silhouettes, the hands and the incoming spell are the focal points.
```

**20-CONCEPT** (2:3)

```
Style: stained-glass world, every surface made of coloured glass panes held by dark lead lines, jewel tones glowing with backlight, figures as leaded glass mosaics, sun beams casting coloured light pools on the sand, spells as molten glowing glass, sacred and luminous. Key art for an original Roman-era elemental-mage arena game. Cassia, a sharp-featured water mage woman in her thirties, dark hair bound with sea-blue cord, layered sea-blue and undyed linen robes, a bronze prisoner's collar with four inlaid runes, stands braced on arena sand drawing a large luminous sigil of concentric water rings with one hand while her other palm holds back a wave of vermilion fire thrown by a bearded fire mage in crimson. Behind them, towering stands, Roman standards, and grey-robed wardens watching from the wall. Dramatic, heroic, poster composition, strong silhouette.
```

**Negative (shared + extra):** `dull, matte`

<a id="21-handpainted"></a>
### 21. Hand-Painted Stylised Fantasy

*Chunky hand-painted textures and bold proportions: a polished stylised fantasy game.*

- **Palette:** sand #E6C18A, stone #9C8B78, crimson #C8382E, sea #2E86C1, gold trim #E0B44A
- **Quest feasibility:** **H** - The mainstream stylised-VR recipe: painted albedo, baked lighting, unlit or simple lit. Proven at 72 Hz.
- **What to judge:** The most shippable look: is it too familiar to win a craft award?

**21-VR** (16:9)

```
Style: polished stylised fantasy game art, hand-painted textures with painted highlights and no photo detail, chunky exaggerated proportions, big hands and readable silhouettes, warm saturated palette, soft rim light, crisp clean shapes, vibrant spell effects with painted swirls. First-person view from a seated player's eyes inside an open oval Roman-era sand arena, wide 100-degree field of view, eye height of a seated adult. In the lower foreground, the player's own two bare hands, no gloves, no devices: the right index finger traces a glowing circular sigil of water-light in the air, two concentric rings with one bold inner stroke, droplets trailing the fingertip; the left palm is raised forward, projecting a curved translucent ward shaped like a 140-degree arc of rippling water. A bronze prisoner's cuff on the left wrist has four inlaid runes, two of them lit. Three faint rune circles are set in the sand around the player's seat, within arm's reach. Eight metres ahead, a broad bearded fire mage in scorched crimson wool with a bronze collar hurls a vermilion fireball; a thin shrinking ring telegraphs where it will land. Tiered stone stands with a watching crowd, banners, and grey-robed arcane wardens on the wall. Clear readable silhouettes, the hands and the incoming spell are the focal points.
```

**21-CONCEPT** (2:3)

```
Style: polished stylised fantasy game art, hand-painted textures with painted highlights and no photo detail, chunky exaggerated proportions, big hands and readable silhouettes, warm saturated palette, soft rim light, crisp clean shapes, vibrant spell effects with painted swirls. Key art for an original Roman-era elemental-mage arena game. Cassia, a sharp-featured water mage woman in her thirties, dark hair bound with sea-blue cord, layered sea-blue and undyed linen robes, a bronze prisoner's collar with four inlaid runes, stands braced on arena sand drawing a large luminous sigil of concentric water rings with one hand while her other palm holds back a wave of vermilion fire thrown by a bearded fire mage in crimson. Behind them, towering stands, Roman standards, and grey-robed wardens watching from the wall. Dramatic, heroic, poster composition, strong silhouette.
```

**Negative (shared + extra):** `photorealistic, gritty`

<a id="22-soft-cel"></a>
### 22. Soft Cel Painted

*Clean cel-shaded characters over lush painted backgrounds, cinematic and expressive.*

- **Palette:** sky #8EC5E8, cloud #F6F1E7, sand #E7C68E, crimson #D0453A, sea #2B8BD1
- **Quest feasibility:** **H** - Cel ramp shader + painted skybox; cheap and expressive, works in narrow FOV.
- **What to judge:** Emotional and readable: does it fit a Roman setting or feel imported?

**22-VR** (16:9)

```
Style: soft cel-shaded characters with clean lines and two-tone shading over lush painterly backgrounds, luminous skies with towering clouds, expressive faces, flowing cloth, glowing sparkles in spell effects, cinematic and emotional colour script. First-person view from a seated player's eyes inside an open oval Roman-era sand arena, wide 100-degree field of view, eye height of a seated adult. In the lower foreground, the player's own two bare hands, no gloves, no devices: the right index finger traces a glowing circular sigil of water-light in the air, two concentric rings with one bold inner stroke, droplets trailing the fingertip; the left palm is raised forward, projecting a curved translucent ward shaped like a 140-degree arc of rippling water. A bronze prisoner's cuff on the left wrist has four inlaid runes, two of them lit. Three faint rune circles are set in the sand around the player's seat, within arm's reach. Eight metres ahead, a broad bearded fire mage in scorched crimson wool with a bronze collar hurls a vermilion fireball; a thin shrinking ring telegraphs where it will land. Tiered stone stands with a watching crowd, banners, and grey-robed arcane wardens on the wall. Clear readable silhouettes, the hands and the incoming spell are the focal points.
```

**22-CONCEPT** (2:3)

```
Style: soft cel-shaded characters with clean lines and two-tone shading over lush painterly backgrounds, luminous skies with towering clouds, expressive faces, flowing cloth, glowing sparkles in spell effects, cinematic and emotional colour script. Key art for an original Roman-era elemental-mage arena game. Cassia, a sharp-featured water mage woman in her thirties, dark hair bound with sea-blue cord, layered sea-blue and undyed linen robes, a bronze prisoner's collar with four inlaid runes, stands braced on arena sand drawing a large luminous sigil of concentric water rings with one hand while her other palm holds back a wave of vermilion fire thrown by a bearded fire mage in crimson. Behind them, towering stands, Roman standards, and grey-robed wardens watching from the wall. Dramatic, heroic, poster composition, strong silhouette.
```

**Negative (shared + extra):** `photorealistic, gritty`

## Family: Atmosphere

<a id="23-torchlit-night"></a>
### 23. Torchlit Night Games

*The Games after dark: torches, braziers, embers, and spells as the brightest lights.*

- **Palette:** night #0E1220, torch #F29E3D, ember #FF5A1F, sand shadow #3A2C22, water glow #36C3E8
- **Quest feasibility:** **M** - Baked torchlight + emissive spells; avoid many dynamic lights. Dark scenes need a brightness floor on Quest.
- **What to judge:** Magic reads beautifully at night, but can you read the opponent's body and the hands?

**23-VR** (16:9)

```
Style: night arena lit only by torches and iron braziers, warm firelight pools on dark sand, deep blue-black shadows, drifting embers, painterly semi-realistic rendering, glowing magic as the strongest light source, tense gladiatorial atmosphere. First-person view from a seated player's eyes inside an open oval Roman-era sand arena, wide 100-degree field of view, eye height of a seated adult. In the lower foreground, the player's own two bare hands, no gloves, no devices: the right index finger traces a glowing circular sigil of water-light in the air, two concentric rings with one bold inner stroke, droplets trailing the fingertip; the left palm is raised forward, projecting a curved translucent ward shaped like a 140-degree arc of rippling water. A bronze prisoner's cuff on the left wrist has four inlaid runes, two of them lit. Three faint rune circles are set in the sand around the player's seat, within arm's reach. Eight metres ahead, a broad bearded fire mage in scorched crimson wool with a bronze collar hurls a vermilion fireball; a thin shrinking ring telegraphs where it will land. Tiered stone stands with a watching crowd, banners, and grey-robed arcane wardens on the wall. Clear readable silhouettes, the hands and the incoming spell are the focal points.
```

**23-CONCEPT** (2:3)

```
Style: night arena lit only by torches and iron braziers, warm firelight pools on dark sand, deep blue-black shadows, drifting embers, painterly semi-realistic rendering, glowing magic as the strongest light source, tense gladiatorial atmosphere. Key art for an original Roman-era elemental-mage arena game. Cassia, a sharp-featured water mage woman in her thirties, dark hair bound with sea-blue cord, layered sea-blue and undyed linen robes, a bronze prisoner's collar with four inlaid runes, stands braced on arena sand drawing a large luminous sigil of concentric water rings with one hand while her other palm holds back a wave of vermilion fire thrown by a bearded fire mage in crimson. Behind them, towering stands, Roman standards, and grey-robed wardens watching from the wall. Dramatic, heroic, poster composition, strong silhouette.
```

**Negative (shared + extra):** `daylight, flat`

<a id="24-sandstorm-sepia"></a>
### 24. Sandstorm Sepia

*Dust-bleached arena in a golden haze, desaturated except the magic.*

- **Palette:** dust #CBB58E, sepia #7A6345, haze #E3D3B0, water #1FA2E0, fire #FF4B1F
- **Quest feasibility:** **M** - Fog is cheap on Quest if it is height fog, not volumetric. Desaturation makes threat colours shine.
- **What to judge:** Strongest threat readability of all? Or too drab for a 10-minute session?

**24-VR** (16:9)

```
Style: desaturated sepia arena in a dry sandstorm, golden dust haze, sun as a pale disc, weathered stone and leather, everything muted ochre and dust brown except the magic, which is fully saturated sea-blue and vermilion, gritty historical realism. First-person view from a seated player's eyes inside an open oval Roman-era sand arena, wide 100-degree field of view, eye height of a seated adult. In the lower foreground, the player's own two bare hands, no gloves, no devices: the right index finger traces a glowing circular sigil of water-light in the air, two concentric rings with one bold inner stroke, droplets trailing the fingertip; the left palm is raised forward, projecting a curved translucent ward shaped like a 140-degree arc of rippling water. A bronze prisoner's cuff on the left wrist has four inlaid runes, two of them lit. Three faint rune circles are set in the sand around the player's seat, within arm's reach. Eight metres ahead, a broad bearded fire mage in scorched crimson wool with a bronze collar hurls a vermilion fireball; a thin shrinking ring telegraphs where it will land. Tiered stone stands with a watching crowd, banners, and grey-robed arcane wardens on the wall. Clear readable silhouettes, the hands and the incoming spell are the focal points.
```

**24-CONCEPT** (2:3)

```
Style: desaturated sepia arena in a dry sandstorm, golden dust haze, sun as a pale disc, weathered stone and leather, everything muted ochre and dust brown except the magic, which is fully saturated sea-blue and vermilion, gritty historical realism. Key art for an original Roman-era elemental-mage arena game. Cassia, a sharp-featured water mage woman in her thirties, dark hair bound with sea-blue cord, layered sea-blue and undyed linen robes, a bronze prisoner's collar with four inlaid runes, stands braced on arena sand drawing a large luminous sigil of concentric water rings with one hand while her other palm holds back a wave of vermilion fire thrown by a bearded fire mage in crimson. Behind them, towering stands, Roman standards, and grey-robed wardens watching from the wall. Dramatic, heroic, poster composition, strong silhouette.
```

**Negative (shared + extra):** `colourful environment, neon`

<a id="25-moonlit-silver"></a>
### 25. Moonlit Silver Nocturne

*A blue-silver monochrome world where only magic has colour.*

- **Palette:** moon silver #C9D3DE, slate #4C5A6E, night blue #1A2235, water #19E0E0, fire #FF3B1F
- **Quest feasibility:** **H** - Unlit tinted monochrome + emissive spells: cheap, and the clearest possible threat language.
- **What to judge:** The cleanest gameplay read. Does a colourless world feel cold for a 10-minute session?

**25-VR** (16:9)

```
Style: moonlit nocturne in blue-silver monochrome, cool silver highlights on stone and cloth, soft blue shadows, the entire world colourless except the magic, which glows in pure saturated colour, turquoise water and vermilion fire, quiet, dreamlike, elegant. First-person view from a seated player's eyes inside an open oval Roman-era sand arena, wide 100-degree field of view, eye height of a seated adult. In the lower foreground, the player's own two bare hands, no gloves, no devices: the right index finger traces a glowing circular sigil of water-light in the air, two concentric rings with one bold inner stroke, droplets trailing the fingertip; the left palm is raised forward, projecting a curved translucent ward shaped like a 140-degree arc of rippling water. A bronze prisoner's cuff on the left wrist has four inlaid runes, two of them lit. Three faint rune circles are set in the sand around the player's seat, within arm's reach. Eight metres ahead, a broad bearded fire mage in scorched crimson wool with a bronze collar hurls a vermilion fireball; a thin shrinking ring telegraphs where it will land. Tiered stone stands with a watching crowd, banners, and grey-robed arcane wardens on the wall. Clear readable silhouettes, the hands and the incoming spell are the focal points.
```

**25-CONCEPT** (2:3)

```
Style: moonlit nocturne in blue-silver monochrome, cool silver highlights on stone and cloth, soft blue shadows, the entire world colourless except the magic, which glows in pure saturated colour, turquoise water and vermilion fire, quiet, dreamlike, elegant. Key art for an original Roman-era elemental-mage arena game. Cassia, a sharp-featured water mage woman in her thirties, dark hair bound with sea-blue cord, layered sea-blue and undyed linen robes, a bronze prisoner's collar with four inlaid runes, stands braced on arena sand drawing a large luminous sigil of concentric water rings with one hand while her other palm holds back a wave of vermilion fire thrown by a bearded fire mage in crimson. Behind them, towering stands, Roman standards, and grey-robed wardens watching from the wall. Dramatic, heroic, poster composition, strong silhouette.
```

**Negative (shared + extra):** `sepia, warm daylight`

<a id="26-mr-sand-circle"></a>
### 26. Mixed-Reality Sand Circle

*Passthrough: the arena floor and opponent appear inside your real room, Quest 3 mixed reality.*

- **Palette:** real room neutrals, sand #E5C489, rune glow #6FE3E0, fire #FF6A2B
- **Quest feasibility:** **H** - Quest 3 passthrough + a small stylised arena slice: least geometry, highest cosy factor, natural for glasses.
- **What to judge:** Is the arena still epic without walls? Is MR the Productivity of games: novel or a gimmick?

**26-VR** (16:9)

```
Style: mixed reality capture, the player's real modern living room seen in camera passthrough with soft natural light, a circle of stylised arena sand and three rune circles projected on the real floor around the chair, a stylised opponent mage and floating stone stand fragments rendered in clean hand-painted style, magic glowing over the real room. First-person view from a seated player's eyes inside an open oval Roman-era sand arena, wide 100-degree field of view, eye height of a seated adult. In the lower foreground, the player's own two bare hands, no gloves, no devices: the right index finger traces a glowing circular sigil of water-light in the air, two concentric rings with one bold inner stroke, droplets trailing the fingertip; the left palm is raised forward, projecting a curved translucent ward shaped like a 140-degree arc of rippling water. A bronze prisoner's cuff on the left wrist has four inlaid runes, two of them lit. Three faint rune circles are set in the sand around the player's seat, within arm's reach. Eight metres ahead, a broad bearded fire mage in scorched crimson wool with a bronze collar hurls a vermilion fireball; a thin shrinking ring telegraphs where it will land. Tiered stone stands with a watching crowd, banners, and grey-robed arcane wardens on the wall. Clear readable silhouettes, the hands and the incoming spell are the focal points.
```

**26-CONCEPT** (2:3)

```
Style: mixed reality capture, the player's real modern living room seen in camera passthrough with soft natural light, a circle of stylised arena sand and three rune circles projected on the real floor around the chair, a stylised opponent mage and floating stone stand fragments rendered in clean hand-painted style, magic glowing over the real room. Key art for an original Roman-era elemental-mage arena game. Cassia, a sharp-featured water mage woman in her thirties, dark hair bound with sea-blue cord, layered sea-blue and undyed linen robes, a bronze prisoner's collar with four inlaid runes, stands braced on arena sand drawing a large luminous sigil of concentric water rings with one hand while her other palm holds back a wave of vermilion fire thrown by a bearded fire mage in crimson. Behind them, towering stands, Roman standards, and grey-robed wardens watching from the wall. Dramatic, heroic, poster composition, strong silhouette.
```

**Negative (shared + extra):** `fantasy room, medieval room`

## Family: Magical

<a id="27-sacred-geometry"></a>
### 27. Sacred Geometry Lightlines

*Magic as light geometry: the arena and spells drawn as luminous vector lines.*

- **Palette:** deep indigo #0C0F2E, line cyan #5FE3FF, line gold #F5C451, fire line #FF4D2E, white #F4F8FF
- **Quest feasibility:** **H** - Unlit additive lines: very cheap, matches the drawn-sigil mechanic exactly, ideal for glasses.
- **What to judge:** The sigils are the world: thrilling, or too abstract to feel Roman?

**27-VR** (16:9)

```
Style: luminous line-art world of sacred geometry, arena and figures defined by glowing thin vector lines and wireframe circles on deep indigo, concentric rings, radial grids and constellations of runes, soft glow and bloom, spells as intricate rotating mandalas of light, precise and mystical. First-person view from a seated player's eyes inside an open oval Roman-era sand arena, wide 100-degree field of view, eye height of a seated adult. In the lower foreground, the player's own two bare hands, no gloves, no devices: the right index finger traces a glowing circular sigil of water-light in the air, two concentric rings with one bold inner stroke, droplets trailing the fingertip; the left palm is raised forward, projecting a curved translucent ward shaped like a 140-degree arc of rippling water. A bronze prisoner's cuff on the left wrist has four inlaid runes, two of them lit. Three faint rune circles are set in the sand around the player's seat, within arm's reach. Eight metres ahead, a broad bearded fire mage in scorched crimson wool with a bronze collar hurls a vermilion fireball; a thin shrinking ring telegraphs where it will land. Tiered stone stands with a watching crowd, banners, and grey-robed arcane wardens on the wall. Clear readable silhouettes, the hands and the incoming spell are the focal points.
```

**27-CONCEPT** (2:3)

```
Style: luminous line-art world of sacred geometry, arena and figures defined by glowing thin vector lines and wireframe circles on deep indigo, concentric rings, radial grids and constellations of runes, soft glow and bloom, spells as intricate rotating mandalas of light, precise and mystical. Key art for an original Roman-era elemental-mage arena game. Cassia, a sharp-featured water mage woman in her thirties, dark hair bound with sea-blue cord, layered sea-blue and undyed linen robes, a bronze prisoner's collar with four inlaid runes, stands braced on arena sand drawing a large luminous sigil of concentric water rings with one hand while her other palm holds back a wave of vermilion fire thrown by a bearded fire mage in crimson. Behind them, towering stands, Roman standards, and grey-robed wardens watching from the wall. Dramatic, heroic, poster composition, strong silhouette.
```

**Negative (shared + extra):** `photorealistic, muddy`

<a id="28-astral-orrery"></a>
### 28. Astral Orrery

*The arena floats under a brass orrery and a star map; sigils are constellations.*

- **Palette:** midnight #121A3A, brass #B8873B, star #F7F2DA, nebula violet #6A4C9C, water #3FC8F0, fire #FF6633
- **Quest feasibility:** **M** - Painted skybox + a few animated brass rings: cheap. Stars can shimmer in stereo; keep them large.
- **What to judge:** Breaks from the Roman captivity tone: escapist highlight or lore problem?

**28-VR** (16:9)

```
Style: celestial fantasy, the arena floor floating under a vast brass orrery with turning rings and planets, a deep starry sky with painted nebulae, constellation lines that become spell sigils, brass and midnight blue materials, soft cosmic glow, wonder and awe. First-person view from a seated player's eyes inside an open oval Roman-era sand arena, wide 100-degree field of view, eye height of a seated adult. In the lower foreground, the player's own two bare hands, no gloves, no devices: the right index finger traces a glowing circular sigil of water-light in the air, two concentric rings with one bold inner stroke, droplets trailing the fingertip; the left palm is raised forward, projecting a curved translucent ward shaped like a 140-degree arc of rippling water. A bronze prisoner's cuff on the left wrist has four inlaid runes, two of them lit. Three faint rune circles are set in the sand around the player's seat, within arm's reach. Eight metres ahead, a broad bearded fire mage in scorched crimson wool with a bronze collar hurls a vermilion fireball; a thin shrinking ring telegraphs where it will land. Tiered stone stands with a watching crowd, banners, and grey-robed arcane wardens on the wall. Clear readable silhouettes, the hands and the incoming spell are the focal points.
```

**28-CONCEPT** (2:3)

```
Style: celestial fantasy, the arena floor floating under a vast brass orrery with turning rings and planets, a deep starry sky with painted nebulae, constellation lines that become spell sigils, brass and midnight blue materials, soft cosmic glow, wonder and awe. Key art for an original Roman-era elemental-mage arena game. Cassia, a sharp-featured water mage woman in her thirties, dark hair bound with sea-blue cord, layered sea-blue and undyed linen robes, a bronze prisoner's collar with four inlaid runes, stands braced on arena sand drawing a large luminous sigil of concentric water rings with one hand while her other palm holds back a wave of vermilion fire thrown by a bearded fire mage in crimson. Behind them, towering stands, Roman standards, and grey-robed wardens watching from the wall. Dramatic, heroic, poster composition, strong silhouette.
```

**Negative (shared + extra):** `daylight, modern`

<a id="29-living-water"></a>
### 29. Living Water Glasswork

*A world of clear glass and water: caustics, refractions, liquid architecture.*

- **Palette:** glass #D7F1F4, caustic #9DE7F0, deep water #0E6C8F, sand #EADBB8, fire #FF5A1A
- **Quest feasibility:** **L** - Real refraction/transparency is the most expensive look on Quest: overdraw and sorting. Only viable heavily faked.
- **What to judge:** Stunning in a still, likely impossible at 72 Hz: is a faked version still worth it?

**29-VR** (16:9)

```
Style: translucent glass and living water world, arena walls and stands made of clear sculpted glass and suspended water, rippling caustic light patterns on pale sand, refractions and soft reflections, figures with water-glass accents, fire as fierce orange against cool transparent blues, serene and otherworldly. First-person view from a seated player's eyes inside an open oval Roman-era sand arena, wide 100-degree field of view, eye height of a seated adult. In the lower foreground, the player's own two bare hands, no gloves, no devices: the right index finger traces a glowing circular sigil of water-light in the air, two concentric rings with one bold inner stroke, droplets trailing the fingertip; the left palm is raised forward, projecting a curved translucent ward shaped like a 140-degree arc of rippling water. A bronze prisoner's cuff on the left wrist has four inlaid runes, two of them lit. Three faint rune circles are set in the sand around the player's seat, within arm's reach. Eight metres ahead, a broad bearded fire mage in scorched crimson wool with a bronze collar hurls a vermilion fireball; a thin shrinking ring telegraphs where it will land. Tiered stone stands with a watching crowd, banners, and grey-robed arcane wardens on the wall. Clear readable silhouettes, the hands and the incoming spell are the focal points.
```

**29-CONCEPT** (2:3)

```
Style: translucent glass and living water world, arena walls and stands made of clear sculpted glass and suspended water, rippling caustic light patterns on pale sand, refractions and soft reflections, figures with water-glass accents, fire as fierce orange against cool transparent blues, serene and otherworldly. Key art for an original Roman-era elemental-mage arena game. Cassia, a sharp-featured water mage woman in her thirties, dark hair bound with sea-blue cord, layered sea-blue and undyed linen robes, a bronze prisoner's collar with four inlaid runes, stands braced on arena sand drawing a large luminous sigil of concentric water rings with one hand while her other palm holds back a wave of vermilion fire thrown by a bearded fire mage in crimson. Behind them, towering stands, Roman standards, and grey-robed wardens watching from the wall. Dramatic, heroic, poster composition, strong silhouette.
```

**Negative (shared + extra):** `opaque, dull`

<a id="30-chalk-slate"></a>
### 30. Chalk & Slate

*A world drawn in chalk on slate: sigils, figures and arena as living diagrams.*

- **Palette:** slate #2A2F33, chalk white #EDEDE8, chalk blue #7CC4E8, chalk red #F06A4D, chalk yellow #F2D06B
- **Quest feasibility:** **H** - Unlit chalk textures on simple geometry; perfect fit for drawing-as-casting and for a tutorial.
- **What to judge:** Brilliant for teaching the sigils: does it sustain a whole game?

**30-VR** (16:9)

```
Style: chalk drawing on dark slate, the arena, crowd and mages sketched in white and coloured chalk with dusty strokes and smudges, construction lines and geometric annotations visible, spells as bright coloured chalk circles that seem freshly drawn, playful and scholarly. First-person view from a seated player's eyes inside an open oval Roman-era sand arena, wide 100-degree field of view, eye height of a seated adult. In the lower foreground, the player's own two bare hands, no gloves, no devices: the right index finger traces a glowing circular sigil of water-light in the air, two concentric rings with one bold inner stroke, droplets trailing the fingertip; the left palm is raised forward, projecting a curved translucent ward shaped like a 140-degree arc of rippling water. A bronze prisoner's cuff on the left wrist has four inlaid runes, two of them lit. Three faint rune circles are set in the sand around the player's seat, within arm's reach. Eight metres ahead, a broad bearded fire mage in scorched crimson wool with a bronze collar hurls a vermilion fireball; a thin shrinking ring telegraphs where it will land. Tiered stone stands with a watching crowd, banners, and grey-robed arcane wardens on the wall. Clear readable silhouettes, the hands and the incoming spell are the focal points.
```

**30-CONCEPT** (2:3)

```
Style: chalk drawing on dark slate, the arena, crowd and mages sketched in white and coloured chalk with dusty strokes and smudges, construction lines and geometric annotations visible, spells as bright coloured chalk circles that seem freshly drawn, playful and scholarly. Key art for an original Roman-era elemental-mage arena game. Cassia, a sharp-featured water mage woman in her thirties, dark hair bound with sea-blue cord, layered sea-blue and undyed linen robes, a bronze prisoner's collar with four inlaid runes, stands braced on arena sand drawing a large luminous sigil of concentric water rings with one hand while her other palm holds back a wave of vermilion fire thrown by a bearded fire mage in crimson. Behind them, towering stands, Roman standards, and grey-robed wardens watching from the wall. Dramatic, heroic, poster composition, strong silhouette.
```

**Negative (shared + extra):** `photorealistic, 3D render`

## Scoring sheet

Score 1-5. **Hands** = are both hands and the sigil clear? **Threat** = does the incoming spell read instantly? **Mood** = right for betrayal, captivity and arena glory? **Original** = would a judge remember it? **Quest** = the H/M/L above, adjusted by what you see. **Glasses** = does the centre-70% crop still work?

| # | Style | Hands | Threat | Mood | Original | Quest | Glasses | Keep? | Note |
|---|---|---|---|---|---|---|---|---|---|
| 01 | Tessera & Lime (owner baseline) | | | | | H | | | |
| 02 | Painted Marble & Gilt | | | | | M | | | |
| 03 | Pompeii Red Fresco | | | | | H | | | |
| 04 | Bronze & Verdigris | | | | | M | | | |
| 05 | Terracotta Figurines | | | | | H | | | |
| 06 | Black-Figure Vase | | | | | H | | | |
| 07 | Gold Tesserae Nocturne | | | | | M | | | |
| 08 | Encaustic Portrait Wax | | | | | L | | | |
| 09 | Gouache Storybook | | | | | H | | | |
| 10 | Illuminated Codex | | | | | H | | | |
| 11 | Watercolour on Travertine | | | | | M | | | |
| 12 | Woodcut Chronicle | | | | | H | | | |
| 13 | Screen-Print Poster | | | | | H | | | |
| 14 | Baroque Chiaroscuro | | | | | L | | | |
| 15 | Comic Ink Cel | | | | | H | | | |
| 16 | Low-Poly Faceted | | | | | H | | | |
| 17 | Stop-Motion Clay | | | | | H | | | |
| 18 | Layered Papercraft | | | | | H | | | |
| 19 | Tabletop Miniature | | | | | M | | | |
| 20 | Stained Glass & Lead | | | | | M | | | |
| 21 | Hand-Painted Stylised Fantasy | | | | | H | | | |
| 22 | Soft Cel Painted | | | | | H | | | |
| 23 | Torchlit Night Games | | | | | M | | | |
| 24 | Sandstorm Sepia | | | | | M | | | |
| 25 | Moonlit Silver Nocturne | | | | | H | | | |
| 26 | Mixed-Reality Sand Circle | | | | | H | | | |
| 27 | Sacred Geometry Lightlines | | | | | H | | | |
| 28 | Astral Orrery | | | | | M | | | |
| 29 | Living Water Glasswork | | | | | L | | | |
| 30 | Chalk & Slate | | | | | H | | | |

## After the round

Shortlist 3-5 styles, then run one follow-up pair per finalist with a different scene (the soldiers wave, and the moment of a perfect absorb) before choosing. Fusions are welcome (for example, one style's world with another's spell language). Record the choice beside the plan, as the PC game did in `mage-arena-art/art/OWNER-CHOICE.md`.

