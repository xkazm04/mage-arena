---
title: Cinder hound and mire maw identity masters, key-art look
channel: vr
date: 2026-10-24
status: RAN - 2 calls, 2 images, 36 credits; both masters pass the image-to-3D input gate and are offered for acceptance (acceptance is the owner's or Athena's, nothing is marked approved here)
sources: [docs/DECISIONS.md (2026-10-24 Visual approvals, answer 4), docs/concepts/VISUAL-PLAN.md (c) (d), docs/research/BRENNIC-MV-MASTER-2026-10-24.md, apps/vr/art/key-art/still-08-creatures-s5103-medium.jpg, apps/vr/art/key-art/key-art-s5101-medium.jpg]
---

# Creature masters, 2026-10-24

Owner answer 4 replaces the grey-hound and moss-frog concepts with masters in the key-art look: a dark-red, lava-cracked
cinder hound and a brown, toothed, toad-like mire maw. This lane made one master per creature with the Brennic procedure
(style block of VISUAL-PLAN (c) with the reference-sheet light override, two `image_reference` images, a plain background,
the manual input gate). Both first calls passed, so no retry was spent.

## Budget

`node apps/vr/tools/refs/creature-masters.mjs budget` (counts every VR log, each generation once):

| Count | Before (first call) | After (last call) | Cap |
|---|---|---|---|
| Leonardo images today (UTC 2026-10-24) | 6 | 8 | 30 (owner) |
| Credits today | 104 | 140 | 400 (`NIGHT_CAP`) |
| This lane's images | 0 | 2 | 6 |
| Latched by a quota or 402 error | no | no | - |
| Style block drift against VISUAL-PLAN (c) | none | none | - |

No quota, 402 or credit error was seen. The calls cost 18 credits each (two references; see the Brennic doc's finding).

## Calls

Logs: `apps/vr/art/characters/cinder-hound/master/leonardo-log.json`, `apps/vr/art/characters/mire-maw/master/leonardo-log.json`
(prompt, seed, model, credits, reference ids and upload ids per row; `public: false`). Images are in
`C:/Users/kazda/mage-art-staging/creature-masters-2026-10-24/` (outside every repo, each with a `.sha256` beside it);
no image is committed. `creature-masters.mjs verify` prints OK for both.

| # | Date (UTC) | Creature | Seed | Size | Prompt sha256 | Credits | Output (staging) | sha256 |
|---|---|---|---|---|---|---|---|---|
| 1 | 2026-10-24 | hound | 7101 | 1376x768 | `9e640ccf06d664bb023336657ef6437b2d713234ce00251579a3b38cbe5e8184` | 18 | `hound-s7101-medium.jpg` | `123bf7d3305f475a4b3782dc88a4d7e4718a900423c0233e9260eb7aab6edd1d` |
| 2 | 2026-10-24 | maw | 7201 | 1376x768 | `1c9b7ec303ec93e7be1b4b0b815a92bffc81a06323a1671e440dac18f7efde03` | 18 | `maw-s7201-medium.jpg` | `a47fff60be448c5faec7e2dc3e2e9666db51b3430bcc38bf1922b9a069ba54eb` |

References for both calls, in order: still-08 (`73741f26...edbdfc`, the creature look) and S1 (`81a487c9...bcb1de4`, style
only). The lead sentence says they are references and that the arena, sand, hands, water, sparks, flames and the other
creatures must not be drawn. Script: `apps/vr/tools/refs/creature-masters.mjs` (new; it reuses `leonardo-client.mjs`
unchanged, and refuses above 6 lane images, 30 day images, or after a latched error). The client was not changed, so no
test was added.

## Input gate

`measure.py` (deterministic half), then the manual gate by this session as observer: SCORE is the rounded-down mean of
isolation / silhouette / pose / light / detail. State: **ran**. Both images are in the 3/4 canonical pose with the whole
body in frame.

| Image | bg | height share | margins (top / bottom) | edge contact | shadow band | element share |
|---|---|---|---|---|---|---|
| hound s7101 | 198 grey, border plain 1.0 | 0.908 | 0.042 / 0.051 | none (tail tip 0.08 from the right edge) | 0.004 | 0.082 (the lava, by design) |
| maw s7201 | 205 grey, border plain 1.0 | 0.811 | 0.100 / 0.089 | none | 0.004 | 0.0 |

| Image | Criteria (I/S/P/L/D) | SCORE | DEFECTS | VERDICT |
|---|---|---|---|---|
| hound s7101 | 9 / 8 / 8 / 8 / 8 | **8** | - | **pass** |
| maw s7201 | 9 / 8 / 8 / 8 / 7 | **7** | - | **pass** |

- **Hound.** One dog, plain grey, side three-quarter, all four legs apart and clear of the body, four paws flat, tail
  straight out, mouth closed, ears up, head turned slightly toward the camera. Dark red faceted plates with broad
  orange-vermilion lava cracks, glowing eyes and paws. No flames, sparks or ground shadow. Margins are a little tighter
  than the Brennic views (top 4 %), but nothing is cropped.
- **Maw.** One toad, plain grey, front three-quarter, squat, front legs planted wide with clawed feet, closed mouth with a
  row of large blunt cream teeth, two yellow eyes, warty mud-brown hide in large lumpy blocks, lighter tan belly. Detail is
  scored 7: the many small warts are finer than the style block's "large blocks", and the hind legs are partly hidden
  behind the body, so the rear leg shape is a guess for a reconstruction.

## Side-by-side against still-08

| Point | still-08 | hound s7101 | maw s7201 |
|---|---|---|---|
| Hide | dark red-brown faceted body, bright orange flame-cracks | same: dark red facets, orange cracks, glowing eyes and paws | brown, warty, toad-like, a lighter tan belly |
| Silhouette | running hounds, lean, pointed ears | lean, long legs, pointed ears; stands still, so more doberman than the running pose | squat mound, wide head, teeth ring; more upright front legs than the lying one in the still |
| Mouth | maw's open toothed gape | hound's mouth closed (by brief) | closed with all teeth showing along the lip line (the still has an open gape) |
| Fire | flames and sparks around the hounds | none, the lava is flat glowing colour (by brief, the gate excludes effects) | none |
| Finish | rendered, soft, matte | same finish, slightly crisper facets | same, softer lumps |

Both read as the same game as still-08 and the north-star key art. The hound's cracks are more regular and wider than the
still's, which is fine for a mesh and a texture. The maw is a little more crocodile-like in the snout than the still's
rounder gape; the owner may prefer a retry with an open mouth if the animation card needs it.

## Offered for acceptance

- **Cinder hound: hound-s7101** (`123bf7d3...edd1d`, staging path above). Gate 8, pass.
- **Mire maw: maw-s7201** (`a47fff60...54eb`, staging path above). Gate 7, pass, with the two notes above.

Nothing is marked approved: acceptance is the owner's or Athena's. Neither is a multi-view set, only the single
three-quarter master the brief asked for; a front/back pair would need its own calls (the Brennic doc's Ask 4 applies).
Lane budget left: 4 calls of 6, day budget 22 images of 30.
