---
title: Brennic multi-view master reference (VISUAL-PLAN (d), steps 0-2)
channel: vr
date: 2026-10-24
status: BLOCKED before the first call - the Leonardo key file is gone; 0 calls, 0 images, 0 credits
sources: [docs/concepts/VISUAL-PLAN.md (c) (d) (e) (f), C:/Users/kazda/.personas/standup/2026-10-24/say-mage-visual-approvals.md, apps/vr/tools/assets/leonardo-client.mjs, https://docs.leonardo.ai/docs/gpt-image-25-flare.md, https://docs.leonardo.ai/recipes/uploading-an-image.md]
---

# Brennic multi-view master reference, 2026-10-24

**Outcome: blocked, no image made.** The lane is fully built and dry-checked, and it stopped at the first `gen` because
the Leonardo client's key file `C:/Users/kazda/kiro/bp/.env` does not exist. bp now holds only `.env.local`, which has no
`LEONARDO_API_KEY`. No HTTP request reached Leonardo, so no call is logged. The day's count is unchanged at 0 images and
0 credits. Nothing is offered for acceptance.

Other projects' `.env` files do carry a `LEONARDO_API_KEY` (`pof`, `image`, `personas` and others). Using one would
change the client's key handling, which this lane must import unchanged, and might bill another account. That is the
operator's call, so it was not made here (see "Ask" at the end).

**Approval this lane relies on.** The STYLE BLOCK of VISUAL-PLAN (c), style sheet S1-S5 and the Brennic identity master
were "approved 2026-10-24 by Athena under owner delegation"
(`C:/Users/kazda/.personas/standup/2026-10-24/say-mage-visual-approvals.md`, answers 1+2). Leonardo is the provider, with
at most 30 images that day (answer 3). Neither `docs/DECISIONS.md` nor VISUAL-PLAN (c)/(g) records this approval yet
(both still read "awaiting owner approval"). This lane may not edit them.

## Registry sources

Each was resolved through `C:/Users/kazda/kiro/ai-registry/knowledge/<domain>/index.json` (`subjects[slug].file`;
techniques from the entry's `techniques` list, read beside the subject file).

| Slug | Resolved as |
|---|---|
| visual-style-locking | subject, media-generation: `knowledge/media-generation/visual-generation/visual-style-locking/visual-style-locking.md` |
| approved-reference-sheet | technique of visual-style-locking: `.../visual-style-locking/techniques/approved-reference-sheet.md` |
| multi-view-master-reference | technique of image-to-3d-input-gating: `.../image-to-3d-input-gating/techniques/multi-view-master-reference.md` |
| image-to-3d-input-gating | subject, game-production: `knowledge/game-production/asset-production/sourcing-economics/image-to-3d-input-gating/image-to-3d-input-gating.md` |
| character-identity-continuity | subject, media-generation: `knowledge/media-generation/visual-generation/character-identity-continuity/character-identity-continuity.md` |

Also read: `consistency-control-arm.md` and `score-defect-verdict-protocol.md`, the step-0 and step-2 protocols.

## Budget check (run first, as the brief asks)

`node apps/vr/tools/refs/leonardo-ref.mjs budget` on 2026-10-24 (UTC), before the attempt and again after it (between 16:22 and 16:32 UTC):

| Count | Value | Cap |
|---|---|---|
| Leonardo images today (every VR log, each generation once) | **0** | 30 (owner) |
| Credits today | 0 | 400 (`NIGHT_CAP`) |
| This lane's images | 0 | 12 |
| Latched by a quota or 402 error | no | - |
| Style block drift against VISUAL-PLAN (c) | none | - |

The day was counted over 35 `leonardo-log.json` files: the main VR checkout and every VR worktree. Each has calls only
from 2026-10-21 and 2026-10-22, none dated 2026-10-24.

**Finding: the client's night roots miss the VR checkouts.** `leonardo-client.mjs` adds `C:/Users/kazda/kiro/mage-arena`
and `C:/Users/kazda/.personas/headless-masters/worktrees/mage-arena` to the night. Those are the PC repo and a folder that
does not exist. The VR main checkout (`kiro/mage-arena-vr`) and its sibling worktrees (`worktrees/mage-arena-vr/*`) are
never counted, so two VR lanes running at once each see only their own spend. `leonardo-ref.mjs` adds the -vr roots to
its own day count, read only. The client is untouched, as the brief requires. It also leaves `leonardo.mjs` and
`leonardo-keyart.mjs` exposed (see "Ask").

## Route: references are supported (no deviation)

Leonardo's model page for `openai/gpt-image-2.5-flare` documents `parameters.guidances.image_reference`. It takes up to
16 images; each is `{ image: { id, type } }` with type `UPLOADED`, `GENERATED`, `VARIATION`, `URL` or `BASE64`; the
model "does not use `strength`". The upload is v1 `init-image`: POST `{extension}`, receive `{id, url, fields}`, then
POST the form fields and then `file` to the presigned URL with no authorization header. The page does not list `seed`;
T38 sent one and its calls completed, so the lane sends it and notes that it may be ignored.

So the identity reference travels as an image, and no "identity in text only" deviation is needed. The
consistency-control-arm technique warns that a provider may accept references and silently ignore them. Arm B therefore
logs the generation's reference and guidance fields (`routing` in the log row) as evidence of where the conditioned
request landed.

## What the lane sends (dry-checked, not yet run)

**References.** Only two images, each refused unless its sha256 matches the approved file:

| Ref | File | sha256 | Role in the prompt |
|---|---|---|---|
| master (= S5) | `apps/vr/art/characters/brennic/concept-front-s3801-medium.jpg` | `34b43291deda6db6739413fc38febe4b5b0ee0a1701d1c3392cff78c8d3263d7` | arm B: style; views: identity, the front view that wins any disagreement |
| S1 | `apps/vr/art/key-art/key-art-s5101-medium.jpg` | `81a487c9be00501d5c211bfcff2c0c95b9cd09d3504d88eb4b01fccf2bcb1de4` | style only, "never its scene, lighting, people, crowd, fire, water or camera" |

The Brennic master is also style sheet S5. Arm B and the Brennic views therefore send the same two images in the same
order. The conditioning path the probe measures is the one the views use, so no re-probe is needed between them
(consistency-control-arm: re-probe when the reference count changes).

**Prompt.** Every prompt is built in this order:

1. The reference lead, when references are sent: "The attached images are ... references, not content to copy".
2. The six slots of the STYLE BLOCK in full, with the light slot set to its named reference-sheet override ("soft even
   studio light, no cast shadow").
3. The action block: the full-body canonical A-pose, a single subject, a plain flat light-grey background, one view per
   image, the same 768x1376 size and 85 % height framing for every view, then an "Avoid:" list of exclusions.

Before every call, the script checks every block phrase against VISUAL-PLAN (c) and refuses to run if one drifted.

| Id | Seed planned | Refs | Prompt sha256 | Chars |
|---|---|---|---|---|
| arm-b | 6001 | master, S1 | `e70f88aec3d8678c06b56446d145b6c7fe3125627427750b0500d543eeb2779a` | 2885 |
| arm-c | 6001 | none | `17014d481b58aced55b7d1fb74e2758ef82f042577844fd4ecde495f49dd8b21` | 2617 |
| front | 6101 | master, S1 | `d80787fefa8217541b606a8861443653acfba5db8838f97644c534c5b6633fe0` | 3369 |
| side | 6102 | master, S1 | `71f36917df4e102a45529593e2983121202da189035d5ec8ecd40530db82b832` | 3532 |
| back | 6103 | master, S1 | `4c814316a5736fcea593255c660cf0fdf957a556421859b30a76b3a27a7fda98` | 3468 |

Arm C's action text is exactly arm B's. Its prompt is shorter only because it has no reference lead, so the references
(and that sentence) are the only difference between the arms. All calls are MEDIUM (14 credits each); the five planned
calls would cost 70 credits.

## Calls

| # | Date (UTC) | Id | Seed | Prompt sha256 | Credits | Output | sha256 |
|---|---|---|---|---|---|---|---|
| - | - | - | - | - | - | - | - |

**No call was made.** The single `gen --id arm-b --seed 6001` attempt stopped in `readKey()` before the first request:
no init-image upload, no generation, no PENDING row. `apps/vr/art/characters/brennic/mv-master/leonardo-log.json` holds
`calls: []` and `uploads: []`, so the log and this table agree at zero. No image exists, in staging or in git.

## Step-0 judge schema (declared and committed in b9960e1, before any call)

Arm A is style sheet S1 (`apps/vr/art/key-art/key-art-s5101-medium.jpg`, no new call). Arm B is a new subject (an arena
herald, not on the sheet) conditioned on the sheet images this lane sends (the Brennic master, which is S5, and S1). Arm
C is the same prompt, seed and size with no reference. One observer (this session, not the generator) reads A, B and C
through this schema.

**Palette roles (5).** Each is held or not held. A role with nothing to show counts as not held.

| Role | Held when |
|---|---|
| Ground | any ground or stone in the image is warm ochre sand or warm mid-grey stone (under the reference-sheet override there is none, so not held) |
| Structure accents | the costume's accents (trim, metal bands, circlet, brooch) come from deep crimson cloth with gold trim, bronze, iron grey, with no other accent hue |
| Characters | the main garments and skin are muted earth reds, charcoal browns, mid-brown leather, warm tan skin |
| Absorb (reserved) | no element colour on the non-magic subject: `measure.py` `element_share` under 0.005 |
| Dodge | any steel or iron surface is a plain unsaturated grey (no chrome, no blue sheen); not held if there is none |

**Shape complexity (1-5).** 5 is a few large material blocks with straps as broad bands; 1 is fine pattern and many
small parts.

**Finish (1-5).** 5 is matte, one flat colour per material, soft broad tonal variation, no outlines, no halftone, no
chalk grain; 1 is fine texture or a non-3D idiom.

**Bar (absolute, asserted).** Arm B holds at least 3 of the 5 palette roles. **Reported either way:** B against C on
roles held, shape and finish (their sum is the composite), with a loud warning if B's composite is at or below C's.
**Routing:** arm B's request must carry its two `image_reference` ids and return COMPLETE; the generation's routing
fields are logged. **If the bar fails:** no Brennic call is made and the lane stops.

**Step-0 result: not run** (blocked). With no B and no C there is no B-against-C reading. Step 0 has neither passed nor
failed, so no Brennic call may be made until it runs.

## Input gate (step 2) and view-set gate (step 1)

`apps/vr/tools/mesh/input-gate.mjs` is not on `master` (checked at the start), so the gate would be applied by hand:

- SCORE 0-10 on isolation, silhouette, pose, light and detail;
- DEFECTS from the closed list in VISUAL-PLAN (d);
- VERDICT: 7 or more passes, 5-6 is prepare and re-gate, under 5 fails;
- each verdict bound to the image's sha256, with the state ran, could-not-run or opted-out.

The view set then gets the five multi-view checks, and the front master wins every disagreement.
`apps/vr/tools/refs/measure.py` supplies the deterministic half: border plainness, subject box and height share, margins,
edge contact, the shadow band under the feet, and element-colour share. On the approved master it reads:

```
concept-front-s3801-medium.jpg  sha256 34b43291...  768x1376  bg (208,208,208) border_plain 1.0
bbox height_share 0.8794  margin_top 0.0552  margin_bottom 0.0654  centre_x 0.498  touches_edge false
shadow_below 0.0026  element_share 0.0
```

S1 reads `element_share 0.0366` (the water ward and the fireball), which shows the element detector works.

| Image | sha256 | State | SCORE | DEFECTS | VERDICT |
|---|---|---|---|---|---|
| front (new) | - | could-not-run (no image: blocked) | - | - | - |
| side | - | could-not-run (no image: blocked) | - | - | - |
| back | - | could-not-run (no image: blocked) | - | - | - |

View-set gate: **could-not-run**. **Views offered for acceptance: none.**

## How to resume (once a key is in place)

The lane is mechanical from here. Each line runs only if the one before it passed its gate:

```
node apps/vr/tools/refs/leonardo-ref.mjs budget
node apps/vr/tools/refs/leonardo-ref.mjs gen --id arm-b --seed 6001
node apps/vr/tools/refs/leonardo-ref.mjs gen --id arm-c --seed 6001
python apps/vr/tools/refs/measure.py C:/Users/kazda/mage-art-staging/brennic-mv-2026-10-24/arm-*.jpg
#   judge A/B/C with the schema above; stop here if B holds fewer than 3 roles
node apps/vr/tools/refs/leonardo-ref.mjs gen --id front --seed 6101
node apps/vr/tools/refs/leonardo-ref.mjs gen --id side --seed 6102
node apps/vr/tools/refs/leonardo-ref.mjs gen --id back --seed 6103
python apps/vr/tools/refs/measure.py C:/Users/kazda/mage-art-staging/brennic-mv-2026-10-24/*.jpg
node apps/vr/tools/refs/leonardo-ref.mjs verify
```

Retries are only for gate failures, with a new seed, and the script refuses once the lane reaches 12 images, the day
reaches 30, or a quota or 402 error has latched it.

## Ask

1. **Which Leonardo key may this lane use?** `C:/Users/kazda/kiro/bp/.env` is gone, and `leonardo-client.mjs` reads only
   that file. The options:
   - (a) restore `LEONARDO_API_KEY` to `bp/.env` (no code change; recommended, because the client and its redaction
     test stay exactly as they are);
   - (b) name another project's `.env` as the source, which is a client change (`ENV_FILE`) for a lane allowed to edit
     `apps/vr/tools/assets/`;
   - (c) defer the Brennic master.
2. **Fix the client's night roots** (in a lane allowed to edit `apps/vr/tools/assets/`): add `kiro/mage-arena-vr` and
   `worktrees/mage-arena-vr`, so `leonardo.mjs` and `leonardo-keyart.mjs` also count the VR checkout and its sibling
   worktrees.
3. **Record the 2026-10-24 style and sheet approval** in `docs/DECISIONS.md` and VISUAL-PLAN (c)/(g), as the standup text
   asks. This lane relies on it but may not edit either file.
