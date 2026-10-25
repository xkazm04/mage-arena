---
title: Visual plan - chasing the art concept, measured
channel: vr
status: draft for the owner's approval (section g)
verified-against: mage-arena-vr cd15d06 (2026-10-24)
sources: [docs/DECISIONS.md, apps/vr/art/index.html, apps/vr/art/*/manifest.json, apps/vr/art/*/prompts.json, apps/vr/art/characters/*/export/metrics.json, apps/vr/art/characters/*/export/SOURCE.md, apps/vr/art/characters/*/HANDOFF.md, apps/vr/art/environment-kit/kit3d/metrics.json, apps/vr/art/environment-kit/kit3d/SOURCE.md, apps/vr/tools/assets/metrics.py, apps/vr/tasks/T38-first-animated-opponent.md, docs/PROJECT-PLAN.md, docs/research/BUDGET-DG4-2026-10.md, docs/submission/ASSET-LEDGER.md, C:/Users/kazda/kiro/mage-arena/docs/PROVIDER-LEDGER.md]
---

# Visual plan: chasing the art concept, measured

The owner ruled on 2026-10-24 (`docs/DECISIONS.md`, "P4 style: no single style is locked") that the visual side is "in
tragic state". Greybox stays the base, and the direction is to chase "the asset and graphical quality drawn in art
concept". A second ruling the same afternoon ("Models are built on this machine") puts model making on this machine
(RTX 4090 24 GB, Blender 4.2, UE 5.8.3), along the ai-registry golden paths. This document turns "chase the art concept"
into a target that can be measured.

- **(a)** What the concept contains, image by image.
- **(b)** How far the game is from it today: observed facts, not impressions.
- **(c)** The style contract every generation call carries.
- **(d)** The gated pipeline each asset walks.
- **(e)** The order of assets, with their world-scale and triangle targets.
- **(f)** The image calls that pay for the reference sheets.
- **(g)** What only the owner can approve.

Written 2026-10-24 on Wolf, docs only. No image, audio or Unreal call was made for it. Every image below was looked at.

**Registry sources.** Each was resolved through `C:/Users/kazda/kiro/ai-registry/knowledge/<domain>/index.json`
(`subjects[slug].file`). Technique files are the `techniques` slugs that index entry lists, read from the `techniques/`
folder beside the subject file. Paths below are relative to `C:/Users/kazda/kiro/ai-registry/`.

| Subject (domain) | Resolved file | Techniques read |
|---|---|---|
| visual-style-locking (media-generation) | `knowledge/media-generation/visual-generation/visual-style-locking/visual-style-locking.md` | `.../techniques/approved-reference-sheet.md`, `style-block-restated-every-call.md`, `rejections-as-negative-evidence.md`, `consistency-control-arm.md` |
| character-identity-continuity (media-generation) | `knowledge/media-generation/visual-generation/character-identity-continuity/character-identity-continuity.md` | - |
| animatable-character-design (media-generation) | `knowledge/media-generation/visual-generation/animatable-character-design/animatable-character-design.md` | - |
| production-pipeline-phasing (media-generation) | `knowledge/media-generation/production-ops/production-pipeline-phasing/production-pipeline-phasing.md` | - |
| image-to-3d-input-gating (game-production) | `knowledge/game-production/asset-production/sourcing-economics/image-to-3d-input-gating/image-to-3d-input-gating.md` | `.../techniques/score-defect-verdict-protocol.md`, `multi-view-master-reference.md`, `canonical-pose-rule.md`, `single-subject-plain-background.md` |
| generated-mesh-acceptance (game-production) | `knowledge/game-production/asset-production/geometry/generated-mesh-acceptance/generated-mesh-acceptance.md` | `.../techniques/structural-scorecard.md`, `defect-code-taxonomy-not-prose.md` |
| mesh-finishing-for-engine-readiness (game-production) | `knowledge/game-production/asset-production/geometry/mesh-finishing-for-engine-readiness/mesh-finishing-for-engine-readiness.md` | - |
| generated-asset-world-scale (game-production) | `knowledge/game-production/asset-production/geometry/generated-asset-world-scale/generated-asset-world-scale.md` | - |
| asset-class-poly-budgeting (game-production) | `knowledge/game-production/asset-production/geometry/asset-class-poly-budgeting/asset-class-poly-budgeting.md` | - |

## (a) Inventory of the art concept

`apps/vr/art/index.html` maps the boards to cards:

| Card | Board |
|---|---|
| A01 | style studies |
| A03 | glyphs |
| A04 | model sheets |
| A05 | SFX (audio; out of scope here) |
| A06 | VFX concepts |
| A07 | environment kit |
| A08 | cuff HUD |
| A09 | session storyboard |

Two later sets are added here: the Leonardo key art and the T38 character concepts.

**Grades.** Each grade is a reconstruction verdict from image-to-3d-input-gating (single subject, plain background,
canonical pose, uncropped, even light). It is never a quality verdict.

| Grade | Meaning |
|---|---|
| **RECON** | Usable as it is. |
| **RECON-MV** | A clean multi-view sheet. A sheet pasted side by side counts as multi-subject (single-subject-plain-background), so each view is cropped to its own image and the views are then gated as a set. |
| **PREP** | Reconstructable only after preparation: background, grid or shadow removed, or each prop cropped out. This is the gate's middle band ("prepare and re-gate"). |
| **MOOD** | A look or composition target only. |
| **RENDER** | A render of a mesh that already exists. |
| **2D-FINAL** | Already a usable 2D texture or decal. |

### A01 style studies (Grok, `grok-imagine-image-quality`, 2026-10-03)

| Path | What it shows | Implies | Grade |
|---|---|---|---|
| `apps/vr/art/style-studies/13-screen-print/01-vr-duel.jpg` | First-person bare hands, cyan concentric sigil, a wavy ward (not an arc), red fire mage, plum stands, mustard sand, sun disc, halftone; letter-like runes on the cuff | Seated first-person framing; dusk arena palette | MOOD |
| `apps/vr/art/style-studies/13-screen-print/02-key-art.jpg` | Poster: Cassia kneeling over blue ripple rings, fire mage hurling a flame wave, eagle banners, sun-ray disc | Key-art composition; Cassia costume | MOOD |
| `apps/vr/art/style-studies/13-screen-print/03-sigil-glyphs.jpg` | 2x2 navy/teal interlocking rings crossed by salmon bars, plum shadow wedges, halftone | Sigil look (the rings drift to interlocking) | MOOD |
| `apps/vr/art/style-studies/13-screen-print/04-fire-mage-turnaround.jpg` | Front, side and back A-pose fire mage, red scorched tunic, navy cloak, collar with letter-like runes, on mustard with a sun disc and shadow bands | Fire mage costume | MOOD |
| `apps/vr/art/style-studies/13-screen-print/05-arena-wide.jpg` | Low seat view of the oval arena: brick-red walls, navy crowd, wardens against a mustard sun, rune circles with letter-like glyphs | Arena layout and dusk palette | MOOD |
| `apps/vr/art/style-studies/13-screen-print/06-cuff-hud.jpg` | Open palm with a teal pool, bronze cuff with letter-like runes, red sand, sun burst | Cuff prop | MOOD |
| `apps/vr/art/style-studies/13-screen-print/07-threat-language.jpg` | Six projectiles in a row on paper (water orb, fire zigzag, shard, air crescent, spear, black red-rimmed orb), frame border | Threat silhouette language | MOOD |
| `apps/vr/art/style-studies/13-screen-print/08-enemy-lineup.jpg` | Conscript with red scutum, slinger mid-swing, ember hound, mud beast, sun burst | Enemy roster silhouettes | MOOD |
| `apps/vr/art/style-studies/25-moonlit-silver/01-vr-duel.jpg` | Near-photoreal night arena, silver crowd and sand, turquoise crescent ward, red fire mage, moon; letters on the cuff | Night arena variant; ward arc shape | MOOD |
| `apps/vr/art/style-studies/25-moonlit-silver/02-key-art.jpg` | Cassia reaching over a glowing turquoise ring, vermilion flame wave, hooded wardens, moonlit tiers | Colourless world, saturated magic | MOOD |
| `apps/vr/art/style-studies/25-moonlit-silver/03-sigil-glyphs.jpg` | Four glowing turquoise water sigils on flat light grey | Sigil decal glow (closest to the brief) | MOOD |
| `apps/vr/art/style-studies/25-moonlit-silver/04-fire-mage-turnaround.jpg` | 3D-render front, side and back A-pose mage, blue-grey skin, dark red cloak, flames on hem and shoulders, glowing collar, flat navy | Fire mage costume; the cleanest A01 sheet | MOOD (emissive flames and a floor patch would bake into a mesh) |
| `apps/vr/art/style-studies/25-moonlit-silver/05-arena-wide.jpg` | Moonlit silver arena, hooded wardens, two red fire pits, three glowing turquoise rune pools, white banners | Night arena; rune pad glow | MOOD |
| `apps/vr/art/style-studies/25-moonlit-silver/06-cuff-hud.jpg` | Grey palm with turquoise wisps, bronze cuff with glowing letter shapes | Cuff material | MOOD |
| `apps/vr/art/style-studies/25-moonlit-silver/07-threat-language.jpg` | Six projectiles evenly spaced on flat dark grey | Projectile silhouette and colour language | MOOD |
| `apps/vr/art/style-studies/25-moonlit-silver/08-enemy-lineup.jpg` | Silver conscript and slinger, a dog in red flames, grey swamp beast oozing turquoise, crescent moon | Roster (turquoise leaks onto a non-magic creature) | MOOD |
| `apps/vr/art/style-studies/30-chalk-slate/01-vr-duel.jpg` | Chalk first-person hands, cyan dotted spiral sigil, wavy blue ward, red mage, doodles, letter-like cuff runes | Chalk VFX look | MOOD |
| `apps/vr/art/style-studies/30-chalk-slate/02-key-art.jpg` | Chalk Cassia over a white/blue concentric ring with construction lines, flame wave, eagle banners | Spell-circle language | MOOD |
| `apps/vr/art/style-studies/30-chalk-slate/03-sigil-glyphs.jpg` | 2x2 sigils on slate with grid and arrow annotations, off-palette purple and green | Sigil shapes | MOOD |
| `apps/vr/art/style-studies/30-chalk-slate/04-fire-mage-turnaround.jpg` | Chalk front, side and back mage, floating colour smudges, hair differs between views | Costume | MOOD |
| `apps/vr/art/style-studies/30-chalk-slate/05-arena-wide.jpg` | Chalk arena, rune circles full of letter-like glyphs, multicolour crowd, navy and gold banners | Arena layout | MOOD |
| `apps/vr/art/style-studies/30-chalk-slate/06-cuff-hud.jpg` | Chalk palm, brass cuff, floating rune circles bearing P, R and X | Cuff (letter-heavy) | MOOD |
| `apps/vr/art/style-studies/30-chalk-slate/07-threat-language.jpg` | Six chalk projectiles on slate with grid and arrows | Threat silhouette language | MOOD |
| `apps/vr/art/style-studies/30-chalk-slate/08-enemy-lineup.jpg` | Chalk conscript in medieval plate, slinger, hound and beast inside chalk rings | Roster silhouettes | MOOD |

### A06 VFX concepts (Grok and agy, 2026-10-03)

| Path | What it shows | Implies | Grade |
|---|---|---|---|
| `apps/vr/art/vfx-concepts/13-screen-print/01-water-bolt.jpg` | Teal orb with a curling ink tail and droplets, red stands, mustard sand, pointing hand | Water bolt silhouette and trail | MOOD |
| `apps/vr/art/vfx-concepts/13-screen-print/02-ward-absorb.jpg` | Raised hand inside a translucent dome swallowing an orange zigzag bolt; Greek ruins behind | Absorb impact (dome is off-brief) | MOOD |
| `apps/vr/art/vfx-concepts/13-screen-print/03-perfect-absorb.jpg` | Faceted turquoise ring at the palm, droplet streaks, cuff glyph | Perfect-absorb burst | MOOD |
| `apps/vr/art/vfx-concepts/13-screen-print/04-unblockable-telegraph.jpg` | Black spiky orb with a red-yellow flame rim, thin red ground ring, hooded caster, yellow sun | Unblockable and its ground telegraph | MOOD |
| `apps/vr/art/vfx-concepts/25-moonlit-silver/01-water-bolt.jpg` | Glowing turquoise orb with a splash tail over moonlit tiers, small red brazier | Water bolt glow | MOOD |
| `apps/vr/art/vfx-concepts/25-moonlit-silver/02-ward-absorb.jpg` | Large glassy turquoise crescent ward with a splash crater where flame enters; medieval rooftops behind | Ward arc and absorb splash | MOOD |
| `apps/vr/art/vfx-concepts/25-moonlit-silver/03-perfect-absorb.jpg` | Ring of turquoise ice chunks around the palm; cuff rune glows red; pagoda roofs | Perfect-absorb burst | MOOD |
| `apps/vr/art/vfx-concepts/25-moonlit-silver/04-unblockable-telegraph.jpg` | Black-core projectile with an orange flame rim, burning red ring on paving, white hooded caster | Unblockable telegraph | MOOD |
| `apps/vr/art/vfx-concepts/30-chalk-slate/01-water-bolt.jpg` | Elevated chalk arena, teal orb with a spiral tail, a real hand drawing with chalk | Trail (framing off-brief) | MOOD |
| `apps/vr/art/vfx-concepts/30-chalk-slate/02-ward-absorb.jpg` | Chalk hand under a turquoise ribbon ward, beam spiralling in, two extra mages in the stands | Ward arc | MOOD |
| `apps/vr/art/vfx-concepts/30-chalk-slate/03-perfect-absorb.jpg` | Braided teal and purple ring framing the palm, cuff with an orange "R" | Perfect-absorb burst | MOOD |
| `apps/vr/art/vfx-concepts/30-chalk-slate/04-unblockable-telegraph.jpg` | Black and purple scribbled projectile with a red rim among many coloured circles | Telegraph (muddied by other circles) | MOOD |

### A04 model sheets (Grok; sheet 02 agy `gemini-3.8-flash-high`, 2026-10-03)

| Path | What it shows | Implies | Grade |
|---|---|---|---|
| `apps/vr/art/model-sheets/01-fire-mage-key-poses.jpg` | Five three-quarter grey-clay poses of a bearded, curly-haired mage (idle, wind-up, release, hit, kneel), orange hands, grid | Brennic key poses for Idle, Cast, Hit and Death | MOOD |
| `apps/vr/art/model-sheets/02-fire-mage-cast-sequence.jpg` | Six side-view grey-clay frames of a fireball throw; this mage is bald and stouter than in 01 | Cast timing (identity does not match 01) | MOOD |
| `apps/vr/art/model-sheets/03-conscript-turnaround.jpg` | Front, side and back A-pose: mail, brimmed helmet, big shield with an orange boss; the shield hides the torso; the sword is sheathed in two views and held in one | Conscript body, shield, gladius | MOOD (views disagree, the shield occludes) |
| `apps/vr/art/model-sheets/04-slinger-turnaround.jpg` | Front, side and back A-pose: cap helmet, riveted jerkin, stone bag, laced greaves, faint grid | Slinger body and gear | PREP (remove the grid; crop the views) |
| `apps/vr/art/model-sheets/05-cinder-hound-turnaround.jpg` | Side, front and top views of a lean hound, cracked grey skin with thin orange seams, grid | Hound body | PREP (remove the grid; no back view) |
| `apps/vr/art/model-sheets/06-mire-maw-turnaround.jpg` | Front, side and back views of a squat frog-like blob, grey clay, moss patch, pebbles and reeds | Maw body | RECON-MV |
| `apps/vr/art/model-sheets/07-cinder-hound-key-poses.jpg` | Four side-view action poses (crouch, leap, rear, shatter) with embers; big-eared, ribby design unlike 05 | Hound attack and death | MOOD |
| `apps/vr/art/model-sheets/08-hand-gestures-first-person.jpg` | Four panels of grey forearms: point with a ring, open palm, pinch, two-hand diamond | First-person gestures | MOOD |

### A07 environment kit (Grok, 2026-10-03) and its two 3D passes (Blender script, no AI step)

| Path | What it shows | Implies | Grade |
|---|---|---|---|
| `apps/vr/art/environment-kit/01-stands-kit.jpg` | Grid sheet of grey stone pieces: straight and curved tiers, ramp, stair, columns, balustrade | Stands kit | PREP (per-prop crop) |
| `apps/vr/art/environment-kit/02-wall-gate-kit.jpg` | Crenellated wall straight and curved, gate arch with a wooden portcullis, balcony | Wall and gate kit (crenellations were dropped by the A07 review) | PREP (per-prop crop) |
| `apps/vr/art/environment-kit/03-arena-props.jpg` | Angled views of benches, a dais disc, a banner on a pole, an iron brazier, a rack, a sand pile, each with a contact shadow | Seat, rune pad, banner, brazier | PREP (crop each prop, remove contact shadows) |
| `apps/vr/art/environment-kit/04-crowd-cards.jpg` | Four grey-clay Roman spectators with red cloak accents: seated calm, seated looking up, cheering, pointing | Crowd card variants | MOOD |
| `apps/vr/art/environment-kit/05-warden-turnaround.jpg` | Front, side and back hooded robed figure with a twisted bronze staff, grey clay on a grid | Warden body and staff | PREP (remove the grid; the staff is a second subject) |
| `apps/vr/art/environment-kit/crowd/crowd_atlas.png` | The four crowd figures cut onto transparency, packed 2x2 | Crowd billboard atlas | 2D-FINAL |
| `apps/vr/art/environment-kit/crowd/crowd_lineup.png` | The four crowd quads rendered in a row on slate blue | Crowd cards (2 tris each) | RENDER |
| `apps/vr/art/environment-kit/crowd/stands_seated_640x480.png` | Seated-eye view: pale sand, plain walls, gate, braziers, banners, dense crowd cards, lilac sky | Seated framing with the crowd | RENDER |
| `apps/vr/art/environment-kit/kit3d/arena_overview.png` | High view of the assembled oval arena in flat colours: orange sand, mauve tiers, dais with three pads | Assembled kit layout | RENDER |
| `apps/vr/art/environment-kit/kit3d/arena_seated_640x480.png` | The same seated view with empty stands, flat warm light, long shadows | Seated framing, empty kit | RENDER |
| `apps/vr/art/environment-kit/kit3d/kit_lineup.png` | All 11 kit pieces in a row on grey | Kit piece inventory | RENDER |

### Key art (Leonardo, `openai/gpt-image-2.5-flare`, seeds 5101-5107)

These seven are the clearest statement of "the art concept" the ruling points at. Section (c) is drawn mostly from them.

| Path | What it shows | Implies | Grade |
|---|---|---|---|
| `apps/vr/art/key-art/key-art-s5101-medium.jpg` | Seated first person, bare hands raised; a turquoise water ward arc meets Brennic's fireball in droplets and sparks; golden dusk, crowd on its feet, crimson banners, torch pylons | The hero target: Brennic's look, ward VFX, arena light | MOOD |
| `apps/vr/art/key-art/still-06-soldiers-s5102-medium.jpg` | Conscripts with big wooden shields and slingers charge; a grey steel spear flies at the player; a small water orb at the hand | Bout 1 soldiers; the steel-means-dodge read | MOOD |
| `apps/vr/art/key-art/still-08-creatures-s5103-medium.jpg` | Three dark-red lava-cracked hounds charge; a brown toothy mire maw by the gate; a water wave hits one hound | Bout 2 creature look (differs from the grey sheets and the concepts) | MOOD |
| `apps/vr/art/key-art/still-09-entrance-s5104-medium.jpg` | Brennic walks out of the gate with fire in both hands past bowl braziers on stone pedestals; cheering crowd | Gate, braziers, entrance staging | MOOD |
| `apps/vr/art/key-art/still-10-sunfall-s5105-medium.jpg` | Brennic raises a huge black sphere with a red rim; a red ring marks the target on the sand | Sunfall telegraph (leave) | MOOD |
| `apps/vr/art/key-art/still-11-finisher-s5106-medium.jpg` | Palms pressed together send a turquoise water wave at Brennic | Finisher VFX | MOOD |
| `apps/vr/art/key-art/still-12-victory-s5107-medium.jpg` | Brennic kneels defeated in steaming sand at sunset; bronze cuff with amber shapes on the player's wrist | Victory beat, cuff look | MOOD |

### Characters (Leonardo concepts, T38 and its follow-ups; exports made on Lynx with TRELLIS.2 and Blender 5.2.2)

| Path | What it shows | Implies | Grade |
|---|---|---|---|
| `apps/vr/art/characters/brennic/concept-front-s3801-medium.jpg` | Brennic full-body front in A-pose on flat light grey: curly hair, full beard, brick-red tunic, charcoal-brown cloak with a round brooch, heavy iron collar, crossed leather wraps, strapped sandals | Brennic master view (it was the TRELLIS input) | RECON |
| `apps/vr/art/characters/brennic/concept-turnaround-s3802-medium.jpg` | The same Brennic front, right side and back, A-pose, flat grey | Brennic view set | RECON-MV |
| `apps/vr/art/characters/brennic/export/Brennic_turnaround.png` | EEVEE, 8 views of `SK_Brennic.fbx`, Idle frame 0 | Current Brennic mesh | RENDER |
| `apps/vr/art/characters/brennic/export/Brennic_game_640x480.png` | Seated view, eye 1.2 m, Brennic at 6 m under a spotlight, about 95 px tall | In-game read | RENDER |
| `apps/vr/art/characters/brennic/export/Brennic_clip_Idle.png` | Idle strip | Idle clip | RENDER |
| `apps/vr/art/characters/brennic/export/Brennic_clip_Cast.png` | Six frames: stand, crouch with both arms open, wind-up, release, follow-through, recover | Cast clip | RENDER |
| `apps/vr/art/characters/brennic/export/Brennic_clip_Hit.png` | Hit strip | Hit clip | RENDER |
| `apps/vr/art/characters/brennic/export/Brennic_clip_Death.png` | Six frames ending in a kneel folded forward; the cloak folds over the back | Death clip | RENDER |
| `apps/vr/art/characters/conscript/concept-front-s4101-medium.jpg` | Roman soldier, A-pose front: helmet with cheek guards, mail, sheathed sword, curved shield, wrapped shins, grey ground | Conscript master view | RECON |
| `apps/vr/art/characters/conscript/concept-turnaround-s4102-medium.jpg` | Same conscript front, side and back on flat grey | Conscript view set | RECON-MV |
| `apps/vr/art/characters/conscript/export/Conscript_turnaround.png` | 8 views of the export: arms lowered, upright spear, shield and sheath at the hips | Current conscript mesh | RENDER |
| `apps/vr/art/characters/conscript/export/Conscript_game_640x480.png` | Conscript about 90 px tall at 6 m | In-game read | RENDER |
| `apps/vr/art/characters/conscript/export/Conscript_clip_Idle.png` | Idle strip | Idle clip | RENDER |
| `apps/vr/art/characters/conscript/export/Conscript_clip_Attack.png` | Spear thrust in five frames | Attack clip | RENDER |
| `apps/vr/art/characters/conscript/export/Conscript_clip_Hit.png` | Head thrown back, left hand raised | Hit clip | RENDER |
| `apps/vr/art/characters/conscript/export/Conscript_clip_Death.png` | Kneel and fold; the spear passes through the body | Death clip | RENDER |
| `apps/vr/art/characters/slinger/concept-front-s4201-medium.jpg` | Roman slinger, A-pose front: cap helmet, studded jerkin, coiled sling, net pouch of stones, shoulder bag, laced shins | Slinger master view | RECON |
| `apps/vr/art/characters/slinger/concept-turnaround-s4202-medium.jpg` | Same slinger front, side and back on flat grey | Slinger view set | RECON-MV |
| `apps/vr/art/characters/slinger/export/Slinger_turnaround.png` | 8 views of the export: two straight rigid cords with a pouch from the right hand | Current slinger mesh | RENDER |
| `apps/vr/art/characters/slinger/export/Slinger_game_640x480.png` | Slinger about 90 px tall at 6 m | In-game read | RENDER |
| `apps/vr/art/characters/slinger/export/Slinger_clip_Idle.png` | Idle strip | Idle clip | RENDER |
| `apps/vr/art/characters/slinger/export/Slinger_clip_Attack.png` | Wind-up, overhead, release, follow-through | Attack clip | RENDER |
| `apps/vr/art/characters/slinger/export/Slinger_clip_Hit.png` | The same motion as the conscript's Hit | Hit clip | RENDER |
| `apps/vr/art/characters/slinger/export/Slinger_clip_Death.png` | Kneel and fold, sling rods poking out | Death clip | RENDER |
| `apps/vr/art/characters/cinder-hound/concept-front-s4301-medium.jpg` | Dark-grey hound square to camera, clean orange bands, glowing eyes, black claws, grey ground | Hound front (hides body length) | RECON |
| `apps/vr/art/characters/cinder-hound/concept-turnaround-s4302-medium.jpg` | Same hound side, front and back on flat grey | Hound view set (the side panel was the TRELLIS input) | RECON-MV |
| `apps/vr/art/characters/cinder-hound/export/CinderHound_turnaround.png` | 8 views of the export | Current hound mesh | RENDER |
| `apps/vr/art/characters/cinder-hound/export/CinderHound_game_640x480.png` | Hound about 70 px tall, a dark post with an orange collar | In-game read | RENDER |
| `apps/vr/art/characters/cinder-hound/export/CinderHound_clip_Idle.png` | Idle strip | Idle clip | RENDER |
| `apps/vr/art/characters/cinder-hound/export/CinderHound_clip_Attack.png` | Crouch, head thrust, jaw open | Attack clip | RENDER |
| `apps/vr/art/characters/cinder-hound/export/CinderHound_clip_Hit.png` | Head jerked up and turned | Hit clip | RENDER |
| `apps/vr/art/characters/cinder-hound/export/CinderHound_clip_Death.png` | Rear up, buckle, lie on the side | Death clip | RENDER |
| `apps/vr/art/characters/mire-maw/concept-front-s4401-medium.jpg` | Squat toad mound front: thick brown lips, nostrils, yellow eyes, shaggy moss cap with pebbles, webbed feet, grey ground | Maw master view | RECON |
| `apps/vr/art/characters/mire-maw/concept-turnaround-s4402-medium.jpg` | Same maw front, side and back with a dripping moss edge | Maw view set | RECON-MV |
| `apps/vr/art/characters/mire-maw/export/MireMaw_turnaround.png` | 8 views of the export: olive blob, yellow sphere eyes, dark mouth slot, blotchy moss | Current maw mesh | RENDER |
| `apps/vr/art/characters/mire-maw/export/MireMaw_game_640x480.png` | Maw small at 6 m, eyes and belly visible | In-game read | RENDER |
| `apps/vr/art/characters/mire-maw/export/MireMaw_clip_Idle.png` | Idle strip | Idle clip | RENDER |
| `apps/vr/art/characters/mire-maw/export/MireMaw_clip_Attack.png` | Jaw opens, thin red tongue lashes out | Attack (tongue pull) clip | RENDER |
| `apps/vr/art/characters/mire-maw/export/MireMaw_clip_Hit.png` | Head tipped back, mouth gaping | Hit clip | RENDER |
| `apps/vr/art/characters/mire-maw/export/MireMaw_clip_Death.png` | Slump and crumple into a shapeless mound | Death clip | RENDER |
| `apps/vr/art/characters/warden/concept-front-s5101-medium.jpg` | Hooded grey-robed man, stern face, bronze collar and hem, charcoal sash, wide sleeves, A-pose, grey ground | Warden master view | RECON |
| `apps/vr/art/characters/warden/export/Warden_turnaround.png` | 8 views of the export with a bronze staff | Current warden mesh | RENDER |
| `apps/vr/art/characters/warden/export/Warden_game_640x480.png` | Warden at an assumed 4 m | In-game read | RENDER |
| `apps/vr/art/characters/warden/export/Warden_clip_Idle.png` | Idle strip | Idle clip | RENDER |
| `apps/vr/art/characters/warden/export/Warden_clip_Cast.png` | Left palm rises and pushes forward | Cast clip | RENDER |
| `apps/vr/art/characters/warden/export/Warden_clip_Hit.png` | Torso twisted back, hand raised | Hit clip | RENDER |
| `apps/vr/art/characters/warden/export/Warden_clip_Death.png` | Fold into a lump; the staff passes through the face | Death clip | RENDER |

### A09 session storyboard (pencil and grey marker; only the magic is coloured)

| Path | What it shows | Implies | Grade |
|---|---|---|---|
| `apps/vr/art/session-storyboard/01-cold-start.jpg` | Empty dusk arena with torches; hands on knees; the cuff glows on the right wrist | Cold-start framing | MOOD |
| `apps/vr/art/session-storyboard/02-teach-ward.jpg` | Turquoise glob drifting in; a raised palm makes a curved ward arc | Teach ward | MOOD |
| `apps/vr/art/session-storyboard/03-teach-perfect.jpg` | Palm catches an orb inside a white timing ring | Perfect-timing cue | MOOD |
| `apps/vr/art/session-storyboard/04-teach-sigil.jpg` | Finger traces a glowing circle-with-bar sigil; a water bolt hits a wooden training dummy | Sigil teach; a training dummy prop | MOOD |
| `apps/vr/art/session-storyboard/05-teach-blink.jpg` | Hand points at floor pads; a black-and-red cracked strip runs under the player | Blink pads, hazard telegraph | MOOD |
| `apps/vr/art/session-storyboard/06-bout-soldiers.jpg` | Shielded soldiers charge, a spear flies in | Bout 1 | MOOD |
| `apps/vr/art/session-storyboard/07-intermission.jpg` | Looking down over the stands at a cuff with runes, floating spell icons | Intermission | MOOD |
| `apps/vr/art/session-storyboard/08-bout-creatures.jpg` | Red dogs charge, a mud monster with teeth, a ward disc | Bout 2 | MOOD |
| `apps/vr/art/session-storyboard/09-fire-mage-entrance.jpg` | Bearded mage in a red cloak strides from the portcullis | Brennic entrance | MOOD |
| `apps/vr/art/session-storyboard/10-duel-sunfall.jpg` | Black flaming meteor overhead, red target rings on stone pads | Sunfall, pad hop | MOOD |
| `apps/vr/art/session-storyboard/11-finisher.jpg` | Palms together, a huge turquoise sphere against the mage's flames | Finisher | MOOD |
| `apps/vr/art/session-storyboard/12-victory.jpg` | Mage kneels in a turquoise puddle at sunset; bronze cuff with teal runes | Victory | MOOD |

### A03 glyphs (script-drawn from the recognizer templates by `apps/vr/tools/glyphs/generate.mjs`)

Each row is the 1024 px file. The `-256` and `-512` siblings exist for all 32 and are drawn separately from the same
vector geometry. The 12 SVG sources sit in `apps/vr/art/glyphs/` (8 glyphs plus 4 `-guide` variants). These are finished
2D art, not reconstruction inputs.

| Path | What it shows | Implies | Grade |
|---|---|---|---|
| `apps/vr/art/glyphs/mask/rune-tier1-1024.png` | tier I rune: ring and one dot; white shape carried in alpha on transparency (tintable mask) | cuff/tier rune decal (mask) | 2D-FINAL |
| `apps/vr/art/glyphs/mask/rune-tier2-1024.png` | tier II rune: ring and two dots; white shape carried in alpha on transparency (tintable mask) | cuff/tier rune decal (mask) | 2D-FINAL |
| `apps/vr/art/glyphs/mask/rune-tier3-1024.png` | tier III rune: ring and three dots; white shape carried in alpha on transparency (tintable mask) | cuff/tier rune decal (mask) | 2D-FINAL |
| `apps/vr/art/glyphs/mask/rune-tier4-1024.png` | tier IV rune: ring and a four-point star; white shape carried in alpha on transparency (tintable mask) | cuff/tier rune decal (mask) | 2D-FINAL |
| `apps/vr/art/glyphs/mask/sigil-line1-1024.png` | sigil line 1: circle with a vertical bar, start dot; white shape carried in alpha on transparency (tintable mask) | sigil tutorial guide decal (mask) | 2D-FINAL |
| `apps/vr/art/glyphs/mask/sigil-line2-1024.png` | sigil line 2: circle with a horizontal bar, start dot; white shape carried in alpha on transparency (tintable mask) | sigil tutorial guide decal (mask) | 2D-FINAL |
| `apps/vr/art/glyphs/mask/sigil-line3-1024.png` | sigil line 3: circle with a 45 degree bar, start dot; white shape carried in alpha on transparency (tintable mask) | sigil tutorial guide decal (mask) | 2D-FINAL |
| `apps/vr/art/glyphs/mask/sigil-mudra-1024.png` | two-hand mudra seal: arch with dots at both feet; white shape carried in alpha on transparency (tintable mask) | sigil tutorial guide decal (mask) | 2D-FINAL |
| `apps/vr/art/glyphs/13-screen-print/rune-tier1-1024.png` | tier I rune: ring and one dot; teal body, sunset-orange misregistration offset, plum outline dotted mustard, transparent | cuff/tier rune decal (13-screen-print) | 2D-FINAL |
| `apps/vr/art/glyphs/13-screen-print/rune-tier2-1024.png` | tier II rune: ring and two dots; teal body, sunset-orange misregistration offset, plum outline dotted mustard, transparent | cuff/tier rune decal (13-screen-print) | 2D-FINAL |
| `apps/vr/art/glyphs/13-screen-print/rune-tier3-1024.png` | tier III rune: ring and three dots; teal body, sunset-orange misregistration offset, plum outline dotted mustard, transparent | cuff/tier rune decal (13-screen-print) | 2D-FINAL |
| `apps/vr/art/glyphs/13-screen-print/rune-tier4-1024.png` | tier IV rune: ring and a four-point star; teal body, sunset-orange misregistration offset, plum outline dotted mustard, transparent | cuff/tier rune decal (13-screen-print) | 2D-FINAL |
| `apps/vr/art/glyphs/13-screen-print/sigil-line1-1024.png` | sigil line 1: circle with a vertical bar, start dot; teal body, sunset-orange misregistration offset, plum outline dotted mustard, transparent | sigil tutorial guide decal (13-screen-print) | 2D-FINAL |
| `apps/vr/art/glyphs/13-screen-print/sigil-line2-1024.png` | sigil line 2: circle with a horizontal bar, start dot; teal body, sunset-orange misregistration offset, plum outline dotted mustard, transparent | sigil tutorial guide decal (13-screen-print) | 2D-FINAL |
| `apps/vr/art/glyphs/13-screen-print/sigil-line3-1024.png` | sigil line 3: circle with a 45 degree bar, start dot; teal body, sunset-orange misregistration offset, plum outline dotted mustard, transparent | sigil tutorial guide decal (13-screen-print) | 2D-FINAL |
| `apps/vr/art/glyphs/13-screen-print/sigil-mudra-1024.png` | two-hand mudra seal: arch with dots at both feet; teal body, sunset-orange misregistration offset, plum outline dotted mustard, transparent | sigil tutorial guide decal (13-screen-print) | 2D-FINAL |
| `apps/vr/art/glyphs/25-moonlit-silver/rune-tier1-1024.png` | tier I rune: ring and one dot; moon-silver ink with a soft water-cyan glow, transparent | cuff/tier rune decal (25-moonlit-silver) | 2D-FINAL |
| `apps/vr/art/glyphs/25-moonlit-silver/rune-tier2-1024.png` | tier II rune: ring and two dots; moon-silver ink with a soft water-cyan glow, transparent | cuff/tier rune decal (25-moonlit-silver) | 2D-FINAL |
| `apps/vr/art/glyphs/25-moonlit-silver/rune-tier3-1024.png` | tier III rune: ring and three dots; moon-silver ink with a soft water-cyan glow, transparent | cuff/tier rune decal (25-moonlit-silver) | 2D-FINAL |
| `apps/vr/art/glyphs/25-moonlit-silver/rune-tier4-1024.png` | tier IV rune: ring and a four-point star; moon-silver ink with a soft water-cyan glow, transparent | cuff/tier rune decal (25-moonlit-silver) | 2D-FINAL |
| `apps/vr/art/glyphs/25-moonlit-silver/sigil-line1-1024.png` | sigil line 1: circle with a vertical bar, start dot; moon-silver ink with a soft water-cyan glow, transparent | sigil tutorial guide decal (25-moonlit-silver) | 2D-FINAL |
| `apps/vr/art/glyphs/25-moonlit-silver/sigil-line2-1024.png` | sigil line 2: circle with a horizontal bar, start dot; moon-silver ink with a soft water-cyan glow, transparent | sigil tutorial guide decal (25-moonlit-silver) | 2D-FINAL |
| `apps/vr/art/glyphs/25-moonlit-silver/sigil-line3-1024.png` | sigil line 3: circle with a 45 degree bar, start dot; moon-silver ink with a soft water-cyan glow, transparent | sigil tutorial guide decal (25-moonlit-silver) | 2D-FINAL |
| `apps/vr/art/glyphs/25-moonlit-silver/sigil-mudra-1024.png` | two-hand mudra seal: arch with dots at both feet; moon-silver ink with a soft water-cyan glow, transparent | sigil tutorial guide decal (25-moonlit-silver) | 2D-FINAL |
| `apps/vr/art/glyphs/30-chalk-slate/rune-tier1-1024.png` | tier I rune: ring and one dot; grainy chalk-white with a slate drop shadow and red/yellow/blue speckle, transparent | cuff/tier rune decal (30-chalk-slate) | 2D-FINAL |
| `apps/vr/art/glyphs/30-chalk-slate/rune-tier2-1024.png` | tier II rune: ring and two dots; grainy chalk-white with a slate drop shadow and red/yellow/blue speckle, transparent | cuff/tier rune decal (30-chalk-slate) | 2D-FINAL |
| `apps/vr/art/glyphs/30-chalk-slate/rune-tier3-1024.png` | tier III rune: ring and three dots; grainy chalk-white with a slate drop shadow and red/yellow/blue speckle, transparent | cuff/tier rune decal (30-chalk-slate) | 2D-FINAL |
| `apps/vr/art/glyphs/30-chalk-slate/rune-tier4-1024.png` | tier IV rune: ring and a four-point star; grainy chalk-white with a slate drop shadow and red/yellow/blue speckle, transparent | cuff/tier rune decal (30-chalk-slate) | 2D-FINAL |
| `apps/vr/art/glyphs/30-chalk-slate/sigil-line1-1024.png` | sigil line 1: circle with a vertical bar, start dot; grainy chalk-white with a slate drop shadow and red/yellow/blue speckle, transparent | sigil tutorial guide decal (30-chalk-slate) | 2D-FINAL |
| `apps/vr/art/glyphs/30-chalk-slate/sigil-line2-1024.png` | sigil line 2: circle with a horizontal bar, start dot; grainy chalk-white with a slate drop shadow and red/yellow/blue speckle, transparent | sigil tutorial guide decal (30-chalk-slate) | 2D-FINAL |
| `apps/vr/art/glyphs/30-chalk-slate/sigil-line3-1024.png` | sigil line 3: circle with a 45 degree bar, start dot; grainy chalk-white with a slate drop shadow and red/yellow/blue speckle, transparent | sigil tutorial guide decal (30-chalk-slate) | 2D-FINAL |
| `apps/vr/art/glyphs/30-chalk-slate/sigil-mudra-1024.png` | two-hand mudra seal: arch with dots at both feet; grainy chalk-white with a slate drop shadow and red/yellow/blue speckle, transparent | sigil tutorial guide decal (30-chalk-slate) | 2D-FINAL |

### A08 cuff HUD (script-drawn by `apps/vr/tools/hud/generate.mjs`; 14 states, 3 palettes and a mask)

The 14 SVG sources sit in `apps/vr/art/hud/` (the neutral bronze master).

| Path | What it shows | Implies | Grade |
|---|---|---|---|
| `apps/vr/art/hud/mask/idle.png` | idle: tier I socket lit, five Flow pips empty; white cuff band in alpha (tintable mask), 1024x256 | wrist-cuff HUD state texture (mask) | 2D-FINAL |
| `apps/vr/art/hud/mask/tier2.png` | tier II unlocked: sockets I-II lit; white cuff band in alpha (tintable mask), 1024x256 | wrist-cuff HUD state texture (mask) | 2D-FINAL |
| `apps/vr/art/hud/mask/tier3.png` | tier III unlocked: sockets I-III lit; white cuff band in alpha (tintable mask), 1024x256 | wrist-cuff HUD state texture (mask) | 2D-FINAL |
| `apps/vr/art/hud/mask/tier4.png` | tier IV unlocked: all four sockets lit; white cuff band in alpha (tintable mask), 1024x256 | wrist-cuff HUD state texture (mask) | 2D-FINAL |
| `apps/vr/art/hud/mask/unlock-flash.png` | unlock flash: socket II with a ticked burst ring; white cuff band in alpha (tintable mask), 1024x256 | wrist-cuff HUD state texture (mask) | 2D-FINAL |
| `apps/vr/art/hud/mask/perfect-advance.png` | perfect absorb: socket II arc with the 2 s advance segment; white cuff band in alpha (tintable mask), 1024x256 | wrist-cuff HUD state texture (mask) | 2D-FINAL |
| `apps/vr/art/hud/mask/flow-0.png` | Flow 0: III half arc, no pip lit; white cuff band in alpha (tintable mask), 1024x256 | wrist-cuff HUD state texture (mask) | 2D-FINAL |
| `apps/vr/art/hud/mask/flow-1.png` | Flow 1: one pip lit; white cuff band in alpha (tintable mask), 1024x256 | wrist-cuff HUD state texture (mask) | 2D-FINAL |
| `apps/vr/art/hud/mask/flow-2.png` | Flow 2: two pips lit; white cuff band in alpha (tintable mask), 1024x256 | wrist-cuff HUD state texture (mask) | 2D-FINAL |
| `apps/vr/art/hud/mask/flow-3.png` | Flow 3: three pips lit; white cuff band in alpha (tintable mask), 1024x256 | wrist-cuff HUD state texture (mask) | 2D-FINAL |
| `apps/vr/art/hud/mask/flow-4.png` | Flow 4: four pips lit; white cuff band in alpha (tintable mask), 1024x256 | wrist-cuff HUD state texture (mask) | 2D-FINAL |
| `apps/vr/art/hud/mask/flow-5.png` | Flow 5: five pips lit; white cuff band in alpha (tintable mask), 1024x256 | wrist-cuff HUD state texture (mask) | 2D-FINAL |
| `apps/vr/art/hud/mask/crest-ready.png` | Crest ready: pips joined by a bar, seal dot, lit rim; white cuff band in alpha (tintable mask), 1024x256 | wrist-cuff HUD state texture (mask) | 2D-FINAL |
| `apps/vr/art/hud/mask/pause.png` | pause: whole cuff dimmed; white cuff band in alpha (tintable mask), 1024x256 | wrist-cuff HUD state texture (mask) | 2D-FINAL |
| `apps/vr/art/hud/13-screen-print/idle.png` | idle: tier I socket lit, five Flow pips empty; mustard band, plum rim, orange offset shadow, paper-lit wells, 1024x256 | wrist-cuff HUD state texture (13-screen-print) | 2D-FINAL |
| `apps/vr/art/hud/13-screen-print/tier2.png` | tier II unlocked: sockets I-II lit; mustard band, plum rim, orange offset shadow, paper-lit wells, 1024x256 | wrist-cuff HUD state texture (13-screen-print) | 2D-FINAL |
| `apps/vr/art/hud/13-screen-print/tier3.png` | tier III unlocked: sockets I-III lit; mustard band, plum rim, orange offset shadow, paper-lit wells, 1024x256 | wrist-cuff HUD state texture (13-screen-print) | 2D-FINAL |
| `apps/vr/art/hud/13-screen-print/tier4.png` | tier IV unlocked: all four sockets lit; mustard band, plum rim, orange offset shadow, paper-lit wells, 1024x256 | wrist-cuff HUD state texture (13-screen-print) | 2D-FINAL |
| `apps/vr/art/hud/13-screen-print/unlock-flash.png` | unlock flash: socket II with a ticked burst ring; mustard band, plum rim, orange offset shadow, paper-lit wells, 1024x256 | wrist-cuff HUD state texture (13-screen-print) | 2D-FINAL |
| `apps/vr/art/hud/13-screen-print/perfect-advance.png` | perfect absorb: socket II arc with the 2 s advance segment; mustard band, plum rim, orange offset shadow, paper-lit wells, 1024x256 | wrist-cuff HUD state texture (13-screen-print) | 2D-FINAL |
| `apps/vr/art/hud/13-screen-print/flow-0.png` | Flow 0: III half arc, no pip lit; mustard band, plum rim, orange offset shadow, paper-lit wells, 1024x256 | wrist-cuff HUD state texture (13-screen-print) | 2D-FINAL |
| `apps/vr/art/hud/13-screen-print/flow-1.png` | Flow 1: one pip lit; mustard band, plum rim, orange offset shadow, paper-lit wells, 1024x256 | wrist-cuff HUD state texture (13-screen-print) | 2D-FINAL |
| `apps/vr/art/hud/13-screen-print/flow-2.png` | Flow 2: two pips lit; mustard band, plum rim, orange offset shadow, paper-lit wells, 1024x256 | wrist-cuff HUD state texture (13-screen-print) | 2D-FINAL |
| `apps/vr/art/hud/13-screen-print/flow-3.png` | Flow 3: three pips lit; mustard band, plum rim, orange offset shadow, paper-lit wells, 1024x256 | wrist-cuff HUD state texture (13-screen-print) | 2D-FINAL |
| `apps/vr/art/hud/13-screen-print/flow-4.png` | Flow 4: four pips lit; mustard band, plum rim, orange offset shadow, paper-lit wells, 1024x256 | wrist-cuff HUD state texture (13-screen-print) | 2D-FINAL |
| `apps/vr/art/hud/13-screen-print/flow-5.png` | Flow 5: five pips lit; mustard band, plum rim, orange offset shadow, paper-lit wells, 1024x256 | wrist-cuff HUD state texture (13-screen-print) | 2D-FINAL |
| `apps/vr/art/hud/13-screen-print/crest-ready.png` | Crest ready: pips joined by a bar, seal dot, lit rim; mustard band, plum rim, orange offset shadow, paper-lit wells, 1024x256 | wrist-cuff HUD state texture (13-screen-print) | 2D-FINAL |
| `apps/vr/art/hud/13-screen-print/pause.png` | pause: whole cuff dimmed; mustard band, plum rim, orange offset shadow, paper-lit wells, 1024x256 | wrist-cuff HUD state texture (13-screen-print) | 2D-FINAL |
| `apps/vr/art/hud/25-moonlit-silver/idle.png` | idle: tier I socket lit, five Flow pips empty; slate/night-blue band, silver rim, water-cyan lamps (no fire swatch), 1024x256 | wrist-cuff HUD state texture (25-moonlit-silver) | 2D-FINAL |
| `apps/vr/art/hud/25-moonlit-silver/tier2.png` | tier II unlocked: sockets I-II lit; slate/night-blue band, silver rim, water-cyan lamps (no fire swatch), 1024x256 | wrist-cuff HUD state texture (25-moonlit-silver) | 2D-FINAL |
| `apps/vr/art/hud/25-moonlit-silver/tier3.png` | tier III unlocked: sockets I-III lit; slate/night-blue band, silver rim, water-cyan lamps (no fire swatch), 1024x256 | wrist-cuff HUD state texture (25-moonlit-silver) | 2D-FINAL |
| `apps/vr/art/hud/25-moonlit-silver/tier4.png` | tier IV unlocked: all four sockets lit; slate/night-blue band, silver rim, water-cyan lamps (no fire swatch), 1024x256 | wrist-cuff HUD state texture (25-moonlit-silver) | 2D-FINAL |
| `apps/vr/art/hud/25-moonlit-silver/unlock-flash.png` | unlock flash: socket II with a ticked burst ring; slate/night-blue band, silver rim, water-cyan lamps (no fire swatch), 1024x256 | wrist-cuff HUD state texture (25-moonlit-silver) | 2D-FINAL |
| `apps/vr/art/hud/25-moonlit-silver/perfect-advance.png` | perfect absorb: socket II arc with the 2 s advance segment; slate/night-blue band, silver rim, water-cyan lamps (no fire swatch), 1024x256 | wrist-cuff HUD state texture (25-moonlit-silver) | 2D-FINAL |
| `apps/vr/art/hud/25-moonlit-silver/flow-0.png` | Flow 0: III half arc, no pip lit; slate/night-blue band, silver rim, water-cyan lamps (no fire swatch), 1024x256 | wrist-cuff HUD state texture (25-moonlit-silver) | 2D-FINAL |
| `apps/vr/art/hud/25-moonlit-silver/flow-1.png` | Flow 1: one pip lit; slate/night-blue band, silver rim, water-cyan lamps (no fire swatch), 1024x256 | wrist-cuff HUD state texture (25-moonlit-silver) | 2D-FINAL |
| `apps/vr/art/hud/25-moonlit-silver/flow-2.png` | Flow 2: two pips lit; slate/night-blue band, silver rim, water-cyan lamps (no fire swatch), 1024x256 | wrist-cuff HUD state texture (25-moonlit-silver) | 2D-FINAL |
| `apps/vr/art/hud/25-moonlit-silver/flow-3.png` | Flow 3: three pips lit; slate/night-blue band, silver rim, water-cyan lamps (no fire swatch), 1024x256 | wrist-cuff HUD state texture (25-moonlit-silver) | 2D-FINAL |
| `apps/vr/art/hud/25-moonlit-silver/flow-4.png` | Flow 4: four pips lit; slate/night-blue band, silver rim, water-cyan lamps (no fire swatch), 1024x256 | wrist-cuff HUD state texture (25-moonlit-silver) | 2D-FINAL |
| `apps/vr/art/hud/25-moonlit-silver/flow-5.png` | Flow 5: five pips lit; slate/night-blue band, silver rim, water-cyan lamps (no fire swatch), 1024x256 | wrist-cuff HUD state texture (25-moonlit-silver) | 2D-FINAL |
| `apps/vr/art/hud/25-moonlit-silver/crest-ready.png` | Crest ready: pips joined by a bar, seal dot, lit rim; slate/night-blue band, silver rim, water-cyan lamps (no fire swatch), 1024x256 | wrist-cuff HUD state texture (25-moonlit-silver) | 2D-FINAL |
| `apps/vr/art/hud/25-moonlit-silver/pause.png` | pause: whole cuff dimmed; slate/night-blue band, silver rim, water-cyan lamps (no fire swatch), 1024x256 | wrist-cuff HUD state texture (25-moonlit-silver) | 2D-FINAL |
| `apps/vr/art/hud/30-chalk-slate/idle.png` | idle: tier I socket lit, five Flow pips empty; ochre chalk band, slate rim, chalk-white lamps, chalk-blue arcs, 1024x256 | wrist-cuff HUD state texture (30-chalk-slate) | 2D-FINAL |
| `apps/vr/art/hud/30-chalk-slate/tier2.png` | tier II unlocked: sockets I-II lit; ochre chalk band, slate rim, chalk-white lamps, chalk-blue arcs, 1024x256 | wrist-cuff HUD state texture (30-chalk-slate) | 2D-FINAL |
| `apps/vr/art/hud/30-chalk-slate/tier3.png` | tier III unlocked: sockets I-III lit; ochre chalk band, slate rim, chalk-white lamps, chalk-blue arcs, 1024x256 | wrist-cuff HUD state texture (30-chalk-slate) | 2D-FINAL |
| `apps/vr/art/hud/30-chalk-slate/tier4.png` | tier IV unlocked: all four sockets lit; ochre chalk band, slate rim, chalk-white lamps, chalk-blue arcs, 1024x256 | wrist-cuff HUD state texture (30-chalk-slate) | 2D-FINAL |
| `apps/vr/art/hud/30-chalk-slate/unlock-flash.png` | unlock flash: socket II with a ticked burst ring; ochre chalk band, slate rim, chalk-white lamps, chalk-blue arcs, 1024x256 | wrist-cuff HUD state texture (30-chalk-slate) | 2D-FINAL |
| `apps/vr/art/hud/30-chalk-slate/perfect-advance.png` | perfect absorb: socket II arc with the 2 s advance segment; ochre chalk band, slate rim, chalk-white lamps, chalk-blue arcs, 1024x256 | wrist-cuff HUD state texture (30-chalk-slate) | 2D-FINAL |
| `apps/vr/art/hud/30-chalk-slate/flow-0.png` | Flow 0: III half arc, no pip lit; ochre chalk band, slate rim, chalk-white lamps, chalk-blue arcs, 1024x256 | wrist-cuff HUD state texture (30-chalk-slate) | 2D-FINAL |
| `apps/vr/art/hud/30-chalk-slate/flow-1.png` | Flow 1: one pip lit; ochre chalk band, slate rim, chalk-white lamps, chalk-blue arcs, 1024x256 | wrist-cuff HUD state texture (30-chalk-slate) | 2D-FINAL |
| `apps/vr/art/hud/30-chalk-slate/flow-2.png` | Flow 2: two pips lit; ochre chalk band, slate rim, chalk-white lamps, chalk-blue arcs, 1024x256 | wrist-cuff HUD state texture (30-chalk-slate) | 2D-FINAL |
| `apps/vr/art/hud/30-chalk-slate/flow-3.png` | Flow 3: three pips lit; ochre chalk band, slate rim, chalk-white lamps, chalk-blue arcs, 1024x256 | wrist-cuff HUD state texture (30-chalk-slate) | 2D-FINAL |
| `apps/vr/art/hud/30-chalk-slate/flow-4.png` | Flow 4: four pips lit; ochre chalk band, slate rim, chalk-white lamps, chalk-blue arcs, 1024x256 | wrist-cuff HUD state texture (30-chalk-slate) | 2D-FINAL |
| `apps/vr/art/hud/30-chalk-slate/flow-5.png` | Flow 5: five pips lit; ochre chalk band, slate rim, chalk-white lamps, chalk-blue arcs, 1024x256 | wrist-cuff HUD state texture (30-chalk-slate) | 2D-FINAL |
| `apps/vr/art/hud/30-chalk-slate/crest-ready.png` | Crest ready: pips joined by a bar, seal dot, lit rim; ochre chalk band, slate rim, chalk-white lamps, chalk-blue arcs, 1024x256 | wrist-cuff HUD state texture (30-chalk-slate) | 2D-FINAL |
| `apps/vr/art/hud/30-chalk-slate/pause.png` | pause: whole cuff dimmed; ochre chalk band, slate rim, chalk-white lamps, chalk-blue arcs, 1024x256 | wrist-cuff HUD state texture (30-chalk-slate) | 2D-FINAL |

**What the inventory says.**

- **Reconstruction-grade input exists only for the characters.** Six Leonardo character fronts and five turnarounds are
  RECON or RECON-MV; the warden has no turnaround. Nothing in A01, A06, A09 or the key art is an input. They are the look
  target only.
- **Props have only PREP sheets** (A07 01-03).
- **The concept contradicts itself on three subjects**, and section (g) asks which one is canon:
  - Mire maw: a brown, lumpy, toothed toad in the key art and storyboard, but a grey frog with a moss back in A04 06 and
    the Leonardo concept.
  - Cinder hound: dark red with lava cracks in the key art, but grey with clean orange bands in the concept.
  - Fire mage: bald in A04 02, and curly-haired and bearded everywhere else (the T38 concept settled that one).

## (b) The gap today

Compared: the 16 in-game captures of one recorded Brennic duel (T16, `3f6c5bd`) against the key art and the Brennic
concept, then the Brennic export against its concept. The captures are `apps/vr/art/style-pick/greybox-01.png` ...
`greybox-04.png`, `13-01.png` ... `13-04.png`, `25-01.png` ... `25-04.png` and `30-01.png` ... `30-04.png`, with the four
MP4s beside them.

### In-game captures against the key art (greybox-01, greybox-03, 13-02, 25-04 and 30-03 looked at in full)

1. **No character exists in the game.** Every enemy and the rival are flat-coloured cylinders: translucent orange for
   the enemies, a brown cylinder with a cyan and orange disc for Brennic (`greybox-03.png`, `30-03.png`). The concept's
   bodies (`key-art-s5101-medium.jpg`) have none of their equivalents on screen. T38's import of `SK_Brennic.fbx` has not
   happened (WOLF-PLAN P6 is unticked).
2. **A debug line sits across the centre of every frame:** "Sim: 7.62s | Last: hit (Actor 1)" (`greybox-01.png`) and the
   same line in all 16 stills.
3. **Magic is untextured primitives.** A projectile is a flat cyan or orange sphere; in `13-02.png` one sphere covers
   about a third of the frame. There are no droplets, sparks, glow or trail. The concept's ward is a turquoise arc of
   water with spray.
4. **Fire is an orange cone on a short pedestal.** No flame, no emitted light and no particle is visible. The concept
   has bowl braziers burning on stone pylons.
5. **The stands are empty stepped blocks.** No crowd is visible in any capture, although the kit has 470 crowd cards
   (`apps/vr/art/environment-kit/crowd/stands_seated_640x480.png`). The concept has a packed, cheering crowd.
6. **The light is flat and shadowless.** Colour planes are evenly lit, no actor casts a visible shadow, and the sky is
   one flat colour. The concept is golden-hour directional light with long soft shadows, a warm sky gradient, sand grain
   and stone block joints.
7. **The player's hands are not hands.** The lower frame shows four stacked flat bars (magenta, blue, green, yellow); in
   `25-04.png` it is a cluster of grey spheres. The concept fills its lower third with two large five-fingered hands.
8. **Cyan guide geometry dominates the centre:** a dashed vertical bar and floor arc segments.
9. **The three styles differ only by flat colour swaps on the same primitives.** 13 tints the walls brick and the sky
   olive; 25 turns everything blue-grey with a large near-black slab above the gate; 30 turns it slate. None of them
   carries the studies' rendering idiom (halftone, moonlit silver, chalk).

### Brennic export against its concept (`Brennic_turnaround.png`, the clip strips, `metrics.json`, `SOURCE.md`)

The mesh is structurally clean and the right size, so this gap is perceptual.

**Structure (`metrics.json`).** Every T38 check passes:

| Measure | Value |
|---|---|
| Triangles | 23,976 |
| Components | 1 (0 floaters) |
| Height | 1.7999 m |
| Bones | 23 |
| Weights | normalised |
| Materials | 6 flat-colour |

**What is wrong, against `concept-front-s3801-medium.jpg`:**

1. **No face.** Eyes, brows and mouth do not read; `SOURCE.md` says so: "a flat-colour read (skin, hair, beard); there
   are no eye or mouth materials".
2. **Accessories lost:**
   - the round cloak brooch is gone;
   - the heavy iron collar reads as a thin grey ring;
   - the crossed forearm and shin wraps are plain bands;
   - the sandal straps are lost, and the feet show skin-coloured blotches.
3. **Colour-region edges are saw-toothed.** The tunic hem, the sleeve edges and the cloak hem look frayed. This comes
   from assigning each face the nearest part colour (`SOURCE.md`, Blender step 1), not from the concept.
4. **The hands are rigid mittens** (no finger bones, `SOURCE.md`).
5. **The animation has visible faults:**
   - Cast's crouch opens both arms outward;
   - Hit's recovery swings an arm wide;
   - in Death the cloak folds over the back (all three admitted in `SOURCE.md`).
6. **The render light is flat studio grey.** At the seated game view (`Brennic_game_640x480.png`) he is about 95 px tall
   in 480: silhouette and the tunic colour read, and no detail does.

### The other exports (same chain, same class of defect)

- **Faces.** No readable face on any humanoid.
- **Hands.** Mitten hands that do not grip the spear (conscript).
- **Rigid parts.**
  - The slinger's sling is two rigid rods.
  - The conscript's shield follows the thigh.
- **Lost concept features.**
  - The maw's lips, nostrils and pebbles are gone, and its arms have melted into the belly.
  - The hound's bands are jagged flame shapes.
  - The hound's eyes do not glow.
- **Interpenetration.**
  - In Death the spear passes through the conscript's body.
  - In Death the staff passes through the warden's face.
- **Game-view size.** Every figure is about 15-20 % of the frame height in its game view.

### What the gap therefore is

| Shortfall | Where it sits | What it is | Which part of this plan answers it |
|---|---|---|---|
| (1) No real assets in the scene | Engine integration (T38 and its siblings) | Not a modelling problem | Not this plan |
| (2) Mesh quality | Faces, accessories, colour edges, hands, skinning faults | Defects a gated pipeline catches before UE | Sections c-e |
| (3) Scene dressing | Light, crowd, VFX, debug text, hand mesh | Presentation work no mesh fixes | Cards T55-T59 (owner answer 6, section g) |

**The arena order** (Athena, 2026-10-25, `docs/DECISIONS.md`): the visual gap in shortfall (3) is the arena, closed in
order of visible gain per Unreal hour. (1) Spell VFX, T58: done, accepted 2026-10-25. (2) Crowd and stands, T57: done,
accepted 2026-10-25 (the crowd tint pass is deferred). (3) Braziers as fire props, T60: done 2026-10-25
(`docs/research/BRAZIERS-T60-2026-10-25.md`): a rounded bowl on the existing brazier batch, and a slow two-layer glow
flame in an environment colour of its own, out of every threat class (shortfall 4's "orange cone" is gone). The bowls
were accepted, the flame was not. T61 (option B, `docs/research/BRAZIERS-T61-2026-10-25.md`, open) gives the flame a
golden-white heart above the rim and below Fire and Earth, and moves the near braziers off the unblockable's line. No
critic round read a threat colour in it, but it reads as fire only weakly, and the moved braziers crowd the fire and
steel lines.

## (c) The style contract

### Draft STYLE BLOCK (approved 2026-10-24 by Athena under owner delegation)

Approval: owner answers 1+2, delegated to Athena, approved as written (`docs/DECISIONS.md`, "2026-10-24 - Visual approvals").

It is written as an attribute grammar (visual-style-locking): fixed slots in plain language, compiled into every call in
full (style-block-restated-every-call). It is drawn from:

- the key art prompts (`apps/vr/art/key-art/manifest.json`) and the T38 concept prompts (`characters/brennic/export/SOURCE.md`),
  which already share most of it;
- the 13/25/30 pool (`apps/vr/art/style-studies/prompts.json`), combined as the owner allowed.

> **Technique:** stylised game concept art in the look of a finished low-poly 3D game model, rendered: large readable
> shapes, chunky heroic proportions, big hands, clean silhouettes that read at six metres.
>
> **Subject treatment:** Roman-era arena world; people and creatures are solid and grounded, with readable faces (clear
> eyes, brows and mouth at rest), costume built from a few large material blocks, straps and wraps shown as a few broad
> bands, never fine pattern.
>
> **Palette with roles:**
>
> | Role | Colours |
> |---|---|
> | Ground | warm ochre sand, warm mid-grey stone |
> | Structure accents | deep crimson cloth with gold trim, bronze, iron grey |
> | Characters | muted earth reds, charcoal browns, mid-brown leather, warm tan skin |
> | Absorb (element colour, reserved for magic) | water turquoise #19E0E0, fire vermilion #FF3B1F |
> | Dodge | plain steel grey |
>
> Black core with a red rim appears only on the one unstoppable spell.
>
> **Finish:** matte surfaces, one flat colour per material with at most soft broad tonal variation, no fine texture
> detail, no outlines, no halftone, no chalk grain.
>
> **Light:** default golden-hour dusk, low warm key light, long soft shadows, orange-to-lilac sky. Override for reference
> sheets: soft even studio light, no cast shadow.
>
> **Exclusions:** no text, letters, numbers, runes that look like letters, logos, watermark, frame border or user
> interface; no extra or disembodied hands, no extra figures; no sun disc or sun-ray burst; no floating smudges or
> construction lines; no off-palette purple or green; no photorealism; no VR headset or controllers.

**The pool, combined.** The owner's ruling allows combining, and this is a proposal for how:

| Style | Proposed use |
|---|---|
| 13 | Its sunset palette (`#E8743B` orange, `#3B2340` plum, `#E2B33C` mustard) feeds the dusk sky and banner accents. |
| 25 | Its night palette (`#1A2235`, `#4C5A6E`, `#C9D3DE`) is an optional night-arena variant through the light slot's scoped override. |
| 30 | Its chalk idiom stays where it already works: the 2D sigil guides and tier runes (A03, A08), never on 3D assets. |

Greybox geometry stays the base. The block governs only what is generated.

**Per-call compilation.** Every call compiles from the block plus an action block. The reference-sheet override is a
named, scoped slot change, recorded with the block. When references ride along, the prompt first says they are style
references, not content to copy. Grok has no negative field, so exclusions travel as an "Avoid:" clause; the A01-A09
manifests already did this.

### Proposed APPROVED REFERENCE SHEET (approved 2026-10-24 by Athena under owner delegation)

Approval: owner answers 1+2 and 4 (`docs/DECISIONS.md`, "2026-10-24 - Visual approvals"). S1-S5 and the Brennic, Conscript, Slinger and Warden masters are approved; the hound and maw rows are replaced (answer 4).

The sheet is made only from existing images. Nothing on it was approved until a human approved it
(approved-reference-sheet: "a render becomes a reference the moment a human approves it, and not before").

**Style sheet** (one style, varied subjects; send count 3, the most relevant three per call):

| # | Path | What it carries |
|---|---|---|
| S1 | `apps/vr/art/key-art/key-art-s5101-medium.jpg` | The hero look: Brennic, ward VFX, dusk arena, crowd (approved 2026-10-24 by Athena under owner delegation; S1 is the north-star frame) |
| S2 | `apps/vr/art/key-art/still-06-soldiers-s5102-medium.jpg` | Soldiers, steel-means-dodge, sand and stands (approved 2026-10-24 by Athena under owner delegation) |
| S3 | `apps/vr/art/key-art/still-09-entrance-s5104-medium.jpg` | Gate, braziers on pylons, banners: the prop vocabulary (approved 2026-10-24 by Athena under owner delegation) |
| S4 | `apps/vr/art/key-art/still-10-sunfall-s5105-medium.jpg` | The leave threat and its ground ring (approved 2026-10-24 by Athena under owner delegation) |
| S5 | `apps/vr/art/characters/brennic/concept-front-s3801-medium.jpg` | The "finished game model" rendering idiom on a plain ground (approved 2026-10-24 by Athena under owner delegation) |

**Identity sheets** (one subject, varied views; character-identity-continuity: identity is carried by an image, never by
prose). One master per character, approved separately:

| Character | Master | Views |
|---|---|---|
| Brennic | `apps/vr/art/characters/brennic/concept-front-s3801-medium.jpg` | `apps/vr/art/characters/brennic/concept-turnaround-s3802-medium.jpg` |
| Conscript | `apps/vr/art/characters/conscript/concept-front-s4101-medium.jpg` | `apps/vr/art/characters/conscript/concept-turnaround-s4102-medium.jpg` |
| Slinger | `apps/vr/art/characters/slinger/concept-front-s4201-medium.jpg` | `apps/vr/art/characters/slinger/concept-turnaround-s4202-medium.jpg` |
| Cinder hound (replaced: key-art look, new Leonardo masters owed (answer 4)) | `apps/vr/art/characters/cinder-hound/concept-front-s4301-medium.jpg` | `apps/vr/art/characters/cinder-hound/concept-turnaround-s4302-medium.jpg` |
| Mire maw (replaced: key-art look, new Leonardo masters owed (answer 4)) | `apps/vr/art/characters/mire-maw/concept-front-s4401-medium.jpg` | `apps/vr/art/characters/mire-maw/concept-turnaround-s4402-medium.jpg` |
| Warden | `apps/vr/art/characters/warden/concept-front-s5101-medium.jpg` | none yet (section f) |

- **Hound and maw are replaced.** The owner chose the key-art look (section g, question 4, answered): the rows above are
  superseded and new Leonardo masters in that look are owed.
- **Approved 2026-10-24 by Athena under owner delegation:** the Brennic, Conscript, Slinger and Warden masters as listed.
- **Faces.** A head close-up crop of each master is the only face on its sheet (approved-reference-sheet, "the
  identity-bearing surface appears exactly once, at maximum scale").

**Send counts, per the provider ledger.**

- Grok "consumes only the first reference" (`C:/Users/kazda/kiro/mage-arena/docs/PROVIDER-LEDGER.md`, A10 notes), so a
  Grok call sends one reference.
- agy calls send at most three.

### Rejections list (negative evidence; never sent as a reference, carried as text exclusions)

Each line is a rejection recorded in a manifest or the export docs, or seen in a kept image.

| What was rejected | Where it is recorded | What the block does about it |
|---|---|---|
| Latin letters in sand circles, on cuffs and collars | `style-studies/manifest.json` regen reasons (13 arena, 13 cuff); seen in `apps/vr/art/style-studies/30-chalk-slate/06-cuff-hud.jpg` and `apps/vr/art/vfx-concepts/30-chalk-slate/03-perfect-absorb.jpg` | Exclusion: no letters or letter-like runes |
| The ward drawn as a flat disc or a dome, not a 140-degree arc | `vfx-concepts/manifest.json` (13 ward, passes 1-2); seen in `apps/vr/art/vfx-concepts/13-screen-print/02-ward-absorb.jpg` | The action block states the arc; VFX are a separate card |
| A bald or hooded figure instead of the bearded fire mage | `model-sheets/manifest.json` (02 regen reasons); seen in `apps/vr/art/model-sheets/02-fire-mage-cast-sequence.jpg` | Identity comes from the Brennic master, never from prose |
| Extra or disembodied hands; a third hand pointing | `session-storyboard/manifest.json` (05 passes 1-2) | Exclusion: no extra hands |
| Sun disc and diagonal shadow bands on sheets meant to be plain | Seen in `apps/vr/art/style-studies/13-screen-print/04-fire-mage-turnaround.jpg` | Exclusion, and the plain-ground override |
| Background drifting off the arena (temples, pagodas, rooftops) | Seen in `apps/vr/art/vfx-concepts/25-moonlit-silver/03-perfect-absorb.jpg` | Subject treatment fixes the setting |
| Floating colour smudges, construction lines, off-palette purple and green | Seen in `apps/vr/art/style-studies/30-chalk-slate/04-fire-mage-turnaround.jpg` | Exclusion |
| Medieval plate armour on a Roman conscript | Seen in `apps/vr/art/style-studies/30-chalk-slate/08-enemy-lineup.jpg` | Subject treatment: Roman era |
| Emissive flames baked onto a turnaround | Seen in `apps/vr/art/style-studies/25-moonlit-silver/04-fire-mage-turnaround.jpg` | Reference sheets: no effects (image-to-3d-input-gating: glow becomes a bulge) |
| 3D: hound hind legs as long sticks (TRELLIS seed 42) | `apps/vr/art/characters/cinder-hound/export/SOURCE.md` | Re-roll from the side view; gate the view set |
| 3D: maw "open shell with holes" (front seed 7, side seed 42) | `apps/vr/art/characters/mire-maw/export/SOURCE.md` | Structural card fails it before finishing |
| 3D: saw-toothed colour regions from per-face colour assignment | `apps/vr/art/characters/brennic/export/SOURCE.md` (finish step 1) | The perceptual critic carries a code for it; finishing must bake, not classify per face |

## (d) The per-asset pipeline

Every asset walks these steps in order. Each step has a gate with a pass rule, and nothing downstream runs on an input
whose upstream gate did not pass (production-pipeline-phasing: "downstream cost must never be spent on an upstream
defect").

**Every gate reports one of three states** (score-defect-verdict-protocol):

- **ran**, with a verdict bound to the exact file it saw (by hash);
- **could not run**;
- **opted out**.

The last two stamp the asset "ungated" for good, and neither is a pass. Defect codes are a closed vocabulary, never
prose.

**Step 0 (once, before any asset): lock the style.** The owner approves the block and the sheet (section g). Then one
consistency-control-arm probe runs:

| Arm | What it is |
|---|---|
| A | An approved key-art image as the anchor (no new call) |
| B | A new subject, conditioned on the sheet |
| C | The same subject, unconditioned |

- **Judge:** one schema covering palette, shape complexity and finish.
- **Pass:** B holds a majority of the named palette roles (at least 3 of the 5).
- **Report:** B against C is reported either way, with a loud warning if B scores at or below C.
- **Fail:** route away from the reference channel, or edit the block. Asset generation does not start.

**Step 1: multi-view master reference.** The approved front is the master; it wins every disagreement.

- **Views:** up to three more (back, profile, three-quarter), each cropped to its own image from a turnaround or
  generated against the identity sheet.
- **Held props are separate subjects** with their own masters: the conscript's spear and shield, the slinger's sling,
  the warden's staff (single-subject-plain-background: "the character with their weapon" is multi-subject).
- **Gate (view set):** the five checks of multi-view-master-reference: each view individually, scale and framing,
  feature correspondence, lighting, detail agreement.
- **Pass:** all five agree.
- **Fail:** reconstruct from the master alone and record it.

**Step 2: input gate.** A separate observer, never the image's generator, scores each image.

- **Output shape:**
  - `SCORE` 0-10 against the five criteria (isolation, silhouette, pose, light, detail);
  - `DEFECTS` from the closed list below;
  - `VERDICT`.
- **Bands:** 7 and above passes; 5-6 means prepare and re-gate from scratch; below 5 fails.

| Code | Class | Severity (derived by the caller from what binds downstream) |
|---|---|---|
| `multi-subject` | isolation | fail: crop or reject |
| `subject-cropped` | isolation | fail |
| `background-not-plain` | isolation | middle band: remove and re-gate |
| `contact-shadow` | isolation | middle band |
| `limb-merge` | silhouette | fail (the rig binds at that junction) |
| `pose-not-canonical` | pose | fail for anything rigged; warn for a static prop |
| `off-axis-view` | pose | fail without a second view |
| `foreshortened-limb` | pose | fail |
| `hands-closed-or-holding` | pose | warn (hands are re-authored) |
| `head-turned` | pose | fail for a face that matters |
| `directional-light` | light | middle band |
| `emissive-or-glow` | light | fail (bakes as a bulge) |
| `text-or-lettering` | detail | fail (text-is-never-geometry) |
| `thin-or-transparent-part` | detail | warn; author it by hand |
| `low-resolution` | detail | middle band |
| `view-set-inconsistent` | set | fail the set; fall back to the master |

**Enforcement.** The gate is called in the same script that queues the image-to-3D job. A fail returns an error instead
of a job.

**Step 3: local open-weight image-to-3D, on this machine.** No generator is installed yet.

- **Generator:** TRELLIS.2 (MIT, `docs/submission/ASSET-LEDGER.md`; named in the 2026-10-21 ruling), installed in a
  dedicated folder outside every repo. Any other tool's licence is read and recorded before an asset made with it can
  ship.
- **Mesh:** generate dense. Record seed, model file, version and wall time.
- **Gate:** the job ran and wrote a mesh. Nothing here is a quality claim.

**Step 4: structural mesh card** (generated-mesh-acceptance, structural-scorecard). Metrics come from a separate tool:
`apps/vr/tools/assets/metrics.py` already measures watertightness, winding, components, floaters, degenerate faces,
height and triangles. The card declares the stage it graded (raw or finished; undeclared stays undeclared).

| Code | Severity | Remedy list |
|---|---|---|
| `empty-mesh` | fail | re-roll |
| `degenerate-bbox` | fail | re-roll |
| `floaters` (by face share, never by count) | fail | neither |
| `too-many-parts` | fail | finish |
| `not-watertight` | warn | - |
| `inconsistent-winding` | warn | - |
| `degenerate-faces` | warn | - |
| `over-density` | warn | finish |
| `budget-overrun` (delivered against requested, 10 % slack) | warn | finish |
| `world-size-off` (only when a target was requested) | warn | - |

- **Pass:** no fail, and every required property measured.
- **Unmeasured:** a property that was not measured shows as `not measured`, which is not a pass. With no target given,
  the size is "unknown", never "fine".

**Step 5: perceptual critic on fixed multi-view renders.** This is the tier the structural card can never replace.

- **Fixed rig:** 8 views at 45 degrees (the existing `*_turnaround.png` camera) plus a head close-up front,
  three-quarter and profile. Even light.
- **Judges:** two independent judge families score content match against the step-1 master and geometric
  plausibility. A verdict that flips when the presentation order is reversed is discarded.
- **Codes:**

| Code | What it catches |
|---|---|
| `identity-drift` | The figure no longer matches the master |
| `face-unreadable` | No readable eyes, brows or mouth |
| `hands-melted` | Hands fused into mittens or blobs |
| `feature-missing:<name>` | A named concept feature is gone (brooch, sandal straps, lips) |
| `colour-edge-sawtooth` | Colour regions with ragged edges |
| `silhouette-loss` | The outline no longer reads |
| `proportion-off` | Body segments out of proportion |

- **Pass:** both judges pass and the verdicts are order-stable. For a rival, the owner also looks; identity is the one
  thing no judge certifies.
- **Spend:** judges run local and open-weight where possible. A paid judge is a spend ask.

**Step 6: finishing, then world scale and the triangle budget** (mesh-finishing-for-engine-readiness,
generated-asset-world-scale, asset-class-poly-budgeting).

- **Order:** reduce, then unwrap, then bake, then bind, each operation naming the mesh version it ran on. Shading has one
  owner. A channel that cannot be derived is named as absent.
- **Scale:** applied once at the import edge: `scale = target extent / measured extent`, the target from section (e),
  never a hand-tuned constant. A humanoid's segment proportions are checked against the reference skeleton.
- **Triangles:** the budget is requested in triangles, delivered triangles are graded against the request (10 % slack),
  and the class ceiling is reported beside it.
- **Proof:** the export is proven by reading it back (export-proven-by-read-back).
- **Pass:** `metrics.py` checks 1-9 for the profile, height inside the target band, triangles inside the request, and
  the read-back matching.

**Step 7: rig and clips.**

- **Skeleton:** the named target is the 23-bone UE5-mannequin chain the exports already use. The mapping is verified
  total over the required bones.
- **Clips:** Idle, Cast, Hit, Death for rivals (T38); Idle, Attack, Hit, Death for soldiers and creatures, as
  `metrics.py`'s clip argument already allows.
- **Gate:** `metrics.py` checks 3 (exactly the four clips, none empty) and 9 (weights normalised), then **played**
  animation, looked at: clip strips rendered and checked for interpenetration (the conscript's spear and the warden's
  staff fail this today) and for the pose each sheet asks for. A rig that only binds is not a passing rig.

**Step 8: UE import, on this machine, one Unreal process at a time.**

- **Before import:** T38's pre-import checks (one skeleton, one skinned mesh, four clips, LOD0 recorded, flat-colour
  materials, origin at the feet, +X forward, unit scale matching `FBody`, `SOURCE.md`).
- **Import:** to `apps/vr/Game/Content/Characters/<Name>/`.
- **Pass:**
  - `MageArena.` green;
  - `capture-budget.ps1 -Runs 3` keeps the triangle row at or under 350k on the BasePass reading (DECISIONS
    2026-10-24);
  - the owner's look at the in-game capture.

**Step 9: ASSET-LEDGER row.** One row in `docs/submission/ASSET-LEDGER.md` per shipped asset: a source line (tool,
model, the log of every call, the human edits) and a licence line. An asset with no row does not ship. A row with an
open condition holds its class back.

## (e) Asset order, scale and triangle targets

Brennic goes first, because he is the R0 opponent the owner judges and his inputs already pass. Then the other
characters with folders under `apps/vr/art/characters/`, then the arena props from the environment kit. Each target
names its source.

| # | Asset | World-scale target | Source of the scale | Requested LOD0 triangles (ceiling) | Source of the triangles |
|---|---|---|---|---|---|
| 1 | Brennic (rival) | 1.80 m, band 1.62-1.98 m | `FBody` mage 180 cm (T38 check 8); `metrics.py` check 8 | 24,000 (30,000) | `metrics.py` `TRI_BUDGET_LOD0 = (18000, 30000)`; T38 check 4 "target 18000-30000" |
| 2 | Conscript | 1.80 m, band 1.62-1.98 m | `metrics.py` Humanoid profile | 24,000 (30,000), owner answer 5 ("18-30k humanoid band"). Superseded proposal: 8,000 (12,000) | `metrics.py` `TRI_BUDGET_LOD0 = (18000, 30000)`. Old source of the superseded 8k: `docs/PROJECT-PLAN.md` section 8 P15: "importing a 60k-triangle 'soldier' fails the ceiling (8k)" |
| 3 | Slinger | 1.80 m, band 1.62-1.98 m | `metrics.py` Humanoid profile | 24,000 (30,000), owner answer 5. Superseded proposal: 8,000 (12,000) | as the conscript |
| 4 | Cinder hound | 1.36 m +-10 % | `metrics.py` `BEASTS['CinderHound']['height']` (A04 sheet scale) | 12,000 | `metrics.py` `BEASTS['CinderHound']['tris']`, "three hounds on screen at once" |
| 5 | Mire maw | 1.25 m +-10 % | `metrics.py` `BEASTS['MireMaw']['height']` | 20,000 | `metrics.py` `BEASTS['MireMaw']['tris']`, "one maw" |
| 6 | Warden (ritual NPC, not a combatant) | 1.74 m measured; target band 1.62-1.98 m | `metrics.py` Humanoid profile | 18,000 (30,000) | `metrics.py` `TRI_BUDGET_LOD0` low end |
| 7 | Gate arch (x1) | 4.12 m wide, 3.60 m high | `apps/vr/data/vr/arena-layout.json` via kit3d check S7 | 8,000 | Share of the kit's assembled `triBudget` 60,000 (`kit3d/metrics.json`) |
| 8 | Brazier (x4) | 0.74 m high, bowl radius 0.42 m | arena-layout via S7 | 3,000 each | as above |
| 9 | Banner (x4) | 1.15 m wide, 3.30 m high | arena-layout via S7 | 1,500 each | as above |
| 10 | Seat (x3) | 0.50 x 0.46 x 0.42 m | arena-layout via S7 | 1,500 each | as above |
| 11 | Dais (x1) | 5.20 x 8.80 x 0.60 m | arena-layout via S7 | 4,000 | as above |

**Why the soldier row was proposed, and how it was settled (owner answer 5, `docs/DECISIONS.md`, "2026-10-24 - Visual approvals": "18-30k humanoid band"; the 8k text below is the superseded proposal).**

- `metrics.py` grades every humanoid at 18-30k, and the current conscript and slinger exports sit at 24,086 and 24,108.
- `docs/PROJECT-PLAN.md` section 7 holds <=350k visible triangles for "hands + arena + 20 actors". Twenty actors at 24k
  is about 480k on actors alone, so the 18-30k band cannot hold for a wave.
- The plan's own P15 example sets the soldier ceiling at 8k. That is a budget the owner should confirm (section g,
  question 5).

**Why the props are split out.**

- The structural kit pieces (walls, tiers, stair, rune pad) stay parametric, made without AI by
  `apps/vr/tools/assets/env_kit.py`. They are modular, and a modular piece tolerates almost no scale error
  (generated-asset-world-scale).
- Only the free-standing props go through image-to-3D. Their sum (8,000 + 12,000 + 6,000 + 4,500 + 4,000 = 34,500)
  plus today's assembled kit (10,620 tris) stays inside the kit's 60,000.

## (f) The image-call budget

**What is left.**

- `C:/Users/kazda/kiro/mage-arena/docs/PROVIDER-LEDGER.md` (read only) holds the 450 hard cap. Its latest line (D2S,
  2026-10-24) reads **409/450**, so **41 calls are left**. The operator's "86 left" figure is older than that line.
- Grok is latched on HTTP 402. agy is the working provider, at 24-50 s per image.
- Leonardo credits are a separate account and are not in that ledger. Owner answer 3 ("Leonardo (Recommended)"): Leonardo is the provider for reconstruction inputs under its own cap, **30 images today**, every call logged with provenance, stop at the first quota error. The agy calls above stay for the non-shipping control arm only.

**Most reference inputs already exist.** Six character masters and five turnarounds are RECON or RECON-MV, and the style
sheet is made of existing images. Cropping views from turnarounds costs no call.

| Use | Calls | Why |
|---|---|---|
| Consistency control arm (step 0): arms B and C | 2 | Arm A is an existing approved image |
| Warden turnaround (back and profile) | 1 | The only character without a view set |
| Prop masters: gate arch, brazier, banner, seat, dais, each front and side | 10 | A07 sheets are PREP only (angled, contact shadows, several props per sheet) |
| Hound and maw re-concept, front and turnaround; the owner picked the key-art look (answer 4), so these are owed, on Leonardo | 4 | Section (g) question 4; 0 if the concepts stand |
| Retry reserve | 10 | The A01-A09 manifests show regenerations on about a third of assets |
| **Planned total** | **27** (23 if the concepts stand) | |
| **Left unallocated** | **14** (18) | For the VFX and dressing cards; nothing above may borrow it without a new line here |

**Rules of the budget.**

- Every call is logged with provenance in the provider ledger and the asset's own manifest.
- A call that hits a latch stops the wave; the remaining work goes to the other provider or waits.
- No call is made before step 0's owner approval. Before approval there is no sheet, and generating against an unapproved
  image quietly promotes it into a standard (approved-reference-sheet, "When not to use it").

## (g) What the owner must approve

1. **[ANSWERED 2026-10-24: approved as written by Athena under owner delegation; `docs/DECISIONS.md`, "2026-10-24 - Visual approvals"]** **The STYLE BLOCK** in section (c), including the proposed combination of the 13/25/30 pool. Approve as written, or
   name the slot to change.
2. **[ANSWERED 2026-10-24: S1-S5 and the Brennic, Conscript, Slinger, Warden masters approved by Athena under owner delegation; hound and maw replaced; `docs/DECISIONS.md`, "2026-10-24 - Visual approvals"]** **The APPROVED REFERENCE SHEET** in section (c): the style sheet S1-S5 and the six identity masters. Each image is
   approved or rejected one by one. A rejected one joins the rejections list with its reason.

The plan also cannot settle these, and asks them here rather than deciding them:

3. **[ANSWERED 2026-10-24: "Leonardo (Recommended)", cap 30 images today; `docs/DECISIONS.md`, "2026-10-24 - Visual approvals"]** **Which provider makes new reference images.** The 2026-10-21 originality ruling covers Leonardo, TRELLIS.2 and
   ElevenLabs, and says the Grok and agy image tools "are not covered". The 41 calls left are agy calls. A mesh
   reconstructed from an agy image would derive from an uncovered tool.
   - **Recommendation:** make new reconstruction inputs with Leonardo (as T38 did, about 14 credits per image), under a
     stated credit count.
   - Use the agy calls only for the control arm, which ships nothing.
4. **[ANSWERED 2026-10-24: "Key-art look (Recommended)"; `docs/DECISIONS.md`, "2026-10-24 - Visual approvals"]** **Hound and maw canon.** Choose either the Leonardo concepts (grey hound with clean orange bands; grey frog maw with a
   moss cap) or the key-art look (dark-red lava hound; brown toothed toad maw).
   - **Recommendation:** the key-art look, because the ruling says chase the concept the key art shows. It costs 4
     calls.
5. **[ANSWERED 2026-10-24: "18-30k humanoid band"; `docs/DECISIONS.md`, "2026-10-24 - Visual approvals"]** **The soldier triangle request** (section e): 8,000 requested, 12,000 ceiling, against `metrics.py`'s 18-30k
   humanoid band.
6. **[ANSWERED 2026-10-24: written as cards T55-T59; `docs/DECISIONS.md`, "2026-10-24 - Visual approvals"]** **The presentation gap** (section b, shortfall 3): light, crowd visibility, VFX, the debug text and a hand mesh are not
   asset work. They need their own cards: `apps/vr/tasks/T55-no-debug-text-in-views.md`, `T56-dusk-lighting.md`, `T57-crowd-visibility.md`, `T58-spell-vfx-toward-key-art.md`, `T59-player-hand-mesh.md`.
