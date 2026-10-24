---
title: Brennic multi-view master reference (VISUAL-PLAN (d), steps 0-2)
channel: vr
date: 2026-10-24
status: RAN - step 0 passed (B 3/5 roles, B above C); front and back pass the input gate, the side fails twice (off-axis, head turned); the three-view set fails, front + back offered as a pair; 6 calls, 6 images, 104 credits
sources: [docs/concepts/VISUAL-PLAN.md (c) (d) (e) (f), C:/Users/kazda/.personas/standup/2026-10-24/say-mage-visual-approvals.md, C:/Users/kazda/.personas/standup/2026-10-24/say-mage-leonardo-key.md, apps/vr/tools/assets/leonardo-client.mjs, https://docs.leonardo.ai/docs/gpt-image-25-flare.md, https://docs.leonardo.ai/recipes/uploading-an-image.md]
---

# Brennic multi-view master reference, 2026-10-24

**Outcome: ran. The set is partly offered.** The first attempt (d444ae02, ce8b4e9) stopped before its first call because
the client's key file `C:/Users/kazda/kiro/bp/.env` was gone. The owner answered at about 18:35
(`say-mage-leonardo-key.md`, "Use personas/.env key"). e6d751e then made the two allowed client changes: `readKey` takes
the `LEONARDO_API_KEY` environment variable, else `C:/Users/kazda/kiro/personas/.env`, else the legacy `bp/.env`, and
`NIGHT_ROOTS` now counts the VR checkout and every VR worktree. The lane then resumed at step 0:

- **Step 0:** arms B and C ran. B holds 3 of the 5 palette roles against C's 2, so it passes the bar. B's composite (11)
  is above C's (10).
- **Views:** front s6101 and back s6103 pass the input gate. The right side failed twice (s6102, s6104) with the same
  defects: `off-axis-view` and `head-turned`.
- **View set:** the three-view set fails (`view-set-inconsistent`, through the side). Front + back pass all five checks
  as a two-view set, and that pair is what is offered for acceptance.
- **Spend:** 6 calls, 6 images, 104 credits (lane 6 of 12, day 6 of 30). No quota or 402 error was seen.

**Approval this lane relies on.** The STYLE BLOCK of VISUAL-PLAN (c), style sheet S1-S5 and the Brennic identity master
were "approved 2026-10-24 by Athena under owner delegation"
(`C:/Users/kazda/.personas/standup/2026-10-24/say-mage-visual-approvals.md`, answers 1+2). Leonardo is the provider, with
at most 30 images that day (answer 3). `docs/DECISIONS.md` (7f8103c) and VISUAL-PLAN (c)/(g) (f299f2a) now record the
approval.

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

`node apps/vr/tools/refs/leonardo-ref.mjs budget` on 2026-10-24 (UTC). The first attempt ran it between 16:22 and 16:32,
and this run ran it before its first call (16:40) and after its last (16:45):

| Count | Before (16:40) | After (16:45) | Cap |
|---|---|---|---|
| Leonardo images today (every VR log, each generation once) | **0** | **6** | 30 (owner) |
| Credits today | 0 | 104 | 400 (`NIGHT_CAP`) |
| The client's own night total (`clientNightTotal`) | 0 | 104 | - |
| This lane's images | 0 | 6 | 12 |
| Latched by a quota or 402 error | no | no | - |
| Style block drift against VISUAL-PLAN (c) | none | none | - |

The day was counted over every `leonardo-log.json` of the main VR checkout and every VR worktree. Before this run, none
had a call dated 2026-10-24. After it, the only calls dated that day are this lane's six.

**Finding, fixed in e6d751e: the client's night roots missed the VR checkouts.** `leonardo-client.mjs` counted only
`C:/Users/kazda/kiro/mage-arena` and `worktrees/mage-arena`: the PC repo and a folder that does not exist. It now also
counts `kiro/mage-arena-vr` and every folder under `worktrees/mage-arena-vr`, and each apps/vr/art folder only once.
`nightTotal` still counts each generation id once. With the fix, the client's total equals this lane's day count (104 =
104). `leonardo-ref.mjs` still adds the -vr roots itself. `dayCount` reads each log file once (a case-folded path set)
and each generation once, so the overlap does not double-count, and the code was kept.

**Finding: a call with references costs 18 credits, not 14.** Every MEDIUM call that sent the two references was
reported by the API at 18 credits. Arm C, which sent none, cost the expected 14. `COST.MEDIUM` (14) is what
`leonardo-ref.mjs` reserves and checks against `NIGHT_CAP` before a call. Each referenced call is therefore
under-reserved by 4. That is harmless within today's caps (104 of 400), but the next lane that plans a credit budget for
referenced calls should use 18.

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

## What the lane sends (dry-checked, then sent as declared)

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
(and that sentence) are the only difference between the arms. All calls are MEDIUM. The five planned calls
were budgeted at 14 credits each (70). The API billed the referenced ones at 18 (see the budget finding).

## Calls

Every call is a row in `apps/vr/art/characters/brennic/mv-master/leonardo-log.json` (6 rows in `calls`, 2 in `uploads`,
`totalCredits` 104, `latched` null). The log and this table agree: 6 calls. All were MEDIUM and `public: false`, and
every one returned COMPLETE with its credits reported by the API (`creditsSource: api`). Outputs are in
`C:/Users/kazda/mage-art-staging/brennic-mv-2026-10-24/` (shortened to `staging/` below), outside every repo, each with
a `.sha256` file beside it. `leonardo-ref.mjs verify` prints OK for all six. No image is in git.

| # | Date (UTC) | Id | Seed | Prompt sha256 | Credits | Output | sha256 |
|---|---|---|---|---|---|---|---|
| 1 | 2026-10-24T16:41:01Z | arm-b | 6001 | `e70f88aec3d8678c06b56446d145b6c7fe3125627427750b0500d543eeb2779a` | 18 | `staging/arm-b-s6001-medium.jpg` | `17056f5a4cbba397208ca25477a5f79c7704263064876a46c78357bb394e0f0b` |
| 2 | 2026-10-24T16:41:32Z | arm-c | 6001 | `17014d481b58aced55b7d1fb74e2758ef82f042577844fd4ecde495f49dd8b21` | 14 | `staging/arm-c-s6001-medium.jpg` | `62f4fd7be7b846cd9e8ba3b1fc531d2a5995077aea0a169129d17ffb9383690f` |
| 3 | 2026-10-24T16:42:32Z | front | 6101 | `d80787fefa8217541b606a8861443653acfba5db8838f97644c534c5b6633fe0` | 18 | `staging/front-s6101-medium.jpg` | `94d95257497af4ae8dd737311f1e91915466585c9536e5415f6e425f4ed181cf` |
| 4 | 2026-10-24T16:43:06Z | side | 6102 | `71f36917df4e102a45529593e2983121202da189035d5ec8ecd40530db82b832` | 18 | `staging/side-s6102-medium.jpg` | `bd7c5d7dfb92f6ad1b994637287ffd640507f90c92e9014b4f5180a3e5e4230b` |
| 5 | 2026-10-24T16:43:26Z | back | 6103 | `4c814316a5736fcea593255c660cf0fdf957a556421859b30a76b3a27a7fda98` | 18 | `staging/back-s6103-medium.jpg` | `d0fb9ac74cca0763f369f66f442b80b590ee307ea08951f0eebf9d6215c09ab6` |
| 6 | 2026-10-24T16:44:40Z | side (retry) | 6104 | `71f36917df4e102a45529593e2983121202da189035d5ec8ecd40530db82b832` | 18 | `staging/side-s6104-medium.jpg` | `00522178b36d2af78d8c28107ed0480d14c94d203c32f6da16ee5d20e6753977` |

Every prompt sha256 equals the one declared before the run (table above). Call 6 is the only retry. It was made for the
side's failed gate, with a new seed and the same declared prompt. The two reference uploads (16:41:01 and 16:41:02, init
image ids `0c11a1e6-...` for the master and `d6d0195e-...` for S1, both OK) were made once and reused by calls 1 and 3-6.

## Step-0 judge schema (declared and committed in 102bc43, before any call)

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

## Step-0 result: PASS

Each arm was read once, before any Brennic call. B and C were read side by side; A was read from the sheet file.

| Role | A (S1, the anchor) | B (conditioned) | C (unconditioned) |
|---|---|---|---|
| Ground | held: ochre sand floor, warm mid-grey stone walls | not held (no ground, by design) | not held (no ground, by design) |
| Structure accents | held: crimson banners with gold trim, bronze braziers | held: gold trim on tunic, cloak and sash ends; bronze brooch, arm bands and circlet; no other accent hue | held: crimson cloak with gold trim, bronze brooch, arm bands and circlet |
| Characters | held: earth-red and charcoal-brown garments, tan skin | held: muted brick-red tunic and cloak, charcoal-brown sash, mid-brown leather sandals, warm tan skin | **not held**: the main garment (the ankle-length tunic) is cream/off-white, which is outside the role |
| Absorb (reserved) | held: element colour only on the magic (`element_share` 0.0366, all of it in the water ward and the fireball) | held (`element_share` 0.0) | held (`element_share` 0.0) |
| Dodge | not held (no plain steel surface reads) | not held (no steel or iron on him) | not held (no steel or iron on him) |
| **Roles held** | 4 / 5 | **3 / 5** | 2 / 5 |
| Shape complexity | 4 (large blocks; the crowd is fine detail) | 4 (a few large cloth blocks, the sash as broad bands; the cloak's drape folds are the only small parts) | 4 (large blocks, read as faceted planes) |
| Finish | 4 (soft, matte, no outlines; some fine texture in the crowd) | 4 (matte, one colour per material, soft broad shading, no outlines, no halftone) | 4 (matte and flat per material; faceted low-poly shading rather than the sheet's soft rounded forms) |
| **Composite** (roles + shape + finish) | 12 | **11** | 10 |

- **Bar: PASS.** B holds 3 of 5 roles. That is the minimum, and the roles it holds are all the ones a ground-free,
  steel-free subject can show.
- **B against C:** B is above C by 1 on roles (Characters). Shape and finish are equal. Composite 11 against 10, so the
  loud warning does not fire. The finishes differ in kind: B has the master's soft, rounded render, and C has
  hard-faceted planes.
- **Routing:** B's request carried both `image_reference` ids (`0c11a1e6-...`, `d6d0195e-...`, type UPLOADED) and returned
  COMPLETE. The logged routing fields are `guidanceScale: null` and `generation_elements: []`. The generation object
  echoes no reference field, so the response itself proves nothing about whether the references were used.
- **Evidence that the references were used:** B moved to the sheet's palette roles (brick red and charcoal brown, as on
  the Brennic master) and to the master's finish. The front view s6101 then came out as a near-copy of the master. Both
  are strong signs that the conditioning path works, which is the risk consistency-control-arm names.

The margin is thin (3 roles, +1 composite), but the bar is absolute and B meets it. The Brennic views went ahead.

## Input gate (step 2) and view-set gate (step 1)

`apps/vr/tools/mesh/input-gate.mjs` is not on `master` (checked at the start), so the gate was applied by hand, by an
observer that is not the generator:

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

`measure.py` on the six new images (all 768x1376, `border_plain` 1.0, `touches_edge` false, `element_share` 0.0):

| Image | bg | height_share | margin_top | margin_bottom | centre_x | shadow_below |
|---|---|---|---|---|---|---|
| arm-b s6001 | 210 | 0.8968 | 0.0501 | 0.0531 | 0.4954 | 0.0014 |
| arm-c s6001 | 202 | 0.9302 | 0.0349 | 0.0349 | 0.4948 | 0.0011 |
| front s6101 | 211 | 0.8844 | 0.0531 | 0.0625 | 0.4980 | 0.0039 |
| side s6102 | 210 | 0.8874 | 0.0531 | 0.0596 | 0.4987 | 0.0096 |
| back s6103 | 211 | 0.8757 | 0.0538 | 0.0705 | 0.4954 | 0.0019 |
| side s6104 | 210 | 0.8888 | 0.0523 | 0.0589 | 0.5169 | 0.0015 |

**SCORE convention:** the five criteria are scored separately (isolation / silhouette / pose / light / detail). SCORE is
their mean, rounded down. A defect whose severity in VISUAL-PLAN (d) is "fail" makes the VERDICT fail whatever the
SCORE. Every gate below is state **ran**.

| Image | sha256 | State | Criteria (I/S/P/L/D) | SCORE | DEFECTS | VERDICT |
|---|---|---|---|---|---|---|
| arm-b s6001 (probe only) | `17056f5a...0b` | ran | 9 / 7 / 8 / 8 / 7 | 7 | - | pass (probe; not a reconstruction input) |
| arm-c s6001 (probe only) | `62f4fd7b...0f` | ran | 9 / 8 / 8 / 8 / 7 | 8 | - | pass (probe; not a reconstruction input) |
| **front s6101** | `94d95257497af4ae8dd737311f1e91915466585c9536e5415f6e425f4ed181cf` | ran | 9 / 8 / 9 / 8 / 8 | **8** | - | **pass** |
| side s6102 | `bd7c5d7dfb92f6ad1b994637287ffd640507f90c92e9014b4f5180a3e5e4230b` | ran | 9 / 6 / 3 / 8 / 8 | 6 | `off-axis-view`, `head-turned` | **fail** |
| **back s6103** | `d0fb9ac74cca0763f369f66f442b80b590ee307ea08951f0eebf9d6215c09ab6` | ran | 9 / 8 / 8 / 8 / 7 | **8** | - | **pass** |
| side s6104 | `00522178b36d2af78d8c28107ed0480d14c94d203c32f6da16ee5d20e6753977` | ran | 9 / 6 / 3 / 8 / 8 | 6 | `off-axis-view`, `head-turned` | **fail** |

What the observer saw:

- **front s6101.** A near-copy of the approved master: the same face, beard, curly hair, iron collar with its front plate,
  bronze brooch, brick-red tunic, belt with a square buckle, charcoal cloak, forearm and shin bands, and sandals. The
  A-pose is clean: the arms are clear of the torso and the cloak, the hands are open with no fingers merged, the legs
  are apart. The light is soft and even, and the shadow band is a trace (0.0039). It has no defect from the list.
- **back s6103.** A clean back view: the back of the curly head, the beard visible at the jaw, the collar, the cloak
  hanging to the back of the knees with the red tunic hem below it, red sleeves at the shoulders, the forearm and shin
  bands, the sandal heels. The arms are in the same A-pose and the view is symmetric. Detail is scored 7 for two small
  additions the front does not show: a square plate at the back of the collar, and a hood-shaped gather at the neck.
- **side s6102 and s6104.** Not an exact profile. The head is in true right profile, but the torso is turned toward the
  camera by roughly 20-30 degrees: the chest and the buckle's face show, and the far (left) arm stands out in front of
  the body on the image's right. That makes the view's azimuth unknown (`off-axis-view`), and it turns the head
  relative to the body (`head-turned`, a fail for a face that matters). The near arm lies across the body's silhouette
  (silhouette 6). Identity and finish otherwise hold. The retry, with a new seed, is nearly pixel-identical to the
  first. With references attached, the seed hardly moves the composition (the model page does not list `seed`), so a
  third seed was not spent: it would almost certainly repeat the failure, and the stop rule is three straight fails.

### View-set gate (the five multi-view-master-reference checks)

| Check | front s6101 + back s6103 | + side (s6102 or s6104) |
|---|---|---|
| 1. Each view individually | pass (both gate at 8) | **fail** (the side fails its input gate) |
| 2. Scale and framing | pass: height share 0.884 / 0.876, top margin 0.053 / 0.054, centre 0.498 / 0.495 | pass on the numbers (s6102: 0.887, 0.053, 0.499; s6104: 0.889, 0.052, 0.517) |
| 3. Feature correspondence | pass: hair, beard, collar, cloak length, forearm and shin bands, sandals and sleeve colour all match across front and back | **fail**: the features match, but they cannot be placed at the side's 90 degrees while its azimuth is unknown |
| 4. Lighting | pass: the same soft even studio light, shadow band 0.004 / 0.002 | pass (0.0096 / 0.0015) |
| 5. Detail agreement | pass with notes: the back's rear collar plate and the hood-shaped gather are not on the front, so the front wins (no rear plate; the gather is the cloak bunched at the neck) | as front + back |

**Verdict.** The three-view set **fails** (`view-set-inconsistent`, through the side). The **front + back pair passes
all five checks.**

**Views offered for acceptance:** front s6101 (`94d95257...81cf`) and back s6103 (`d0fb9ac7...9ab6`), as a matched
two-view set. Both are in staging only. The approved master `concept-front-s3801-medium.jpg` stays the master and wins
every disagreement. s6101 agrees with it and matches the back's framing, so the pair can go to the image-to-3D step
together. **The side is not offered.** VISUAL-PLAN (d) step 1 says a failed set is reconstructed "from the master alone
and record it"; the choice between that and the front + back pair is the owner's (Ask 4).

## How the lane ran (the record's resume script, as executed)

Each line ran only after the one before it passed its gate:

```
node apps/vr/tools/refs/leonardo-ref.mjs budget                      # 0 images, 0 credits
node apps/vr/tools/refs/leonardo-ref.mjs gen --id arm-b --seed 6001  # COMPLETE, 18
node apps/vr/tools/refs/leonardo-ref.mjs gen --id arm-c --seed 6001  # COMPLETE, 14
python apps/vr/tools/refs/measure.py C:/Users/kazda/mage-art-staging/brennic-mv-2026-10-24/arm-*.jpg
#   judged A/B/C: B holds 3 of 5 roles, composite 11 against C's 10, so step 0 passes
node apps/vr/tools/refs/leonardo-ref.mjs gen --id front --seed 6101  # COMPLETE, 18, gate pass
node apps/vr/tools/refs/leonardo-ref.mjs gen --id side --seed 6102   # COMPLETE, 18, gate fail
node apps/vr/tools/refs/leonardo-ref.mjs gen --id back --seed 6103   # COMPLETE, 18, gate pass
python apps/vr/tools/refs/measure.py C:/Users/kazda/mage-art-staging/brennic-mv-2026-10-24/*.jpg
node apps/vr/tools/refs/leonardo-ref.mjs gen --id side --seed 6104   # retry for the failed gate: COMPLETE, 18, gate fail
python apps/vr/tools/refs/measure.py C:/Users/kazda/mage-art-staging/brennic-mv-2026-10-24/side-s6104-medium.jpg
node apps/vr/tools/refs/leonardo-ref.mjs verify                      # OK x6
node apps/vr/tools/refs/leonardo-ref.mjs budget                      # 6 images, 104 credits
```

Left within today's caps: 6 lane images (of 12), 24 day images (of 30), and 296 credits (of 400).

## Ask

1. ~~**Which Leonardo key may this lane use?**~~ **Answered by the owner, 2026-10-24 ~18:35**
   (`say-mage-leonardo-key.md`): "Use personas/.env key", with an environment variable override first and bp/.env kept
   as a legacy fallback. Done in e6d751e.
2. ~~**Fix the client's night roots.**~~ **Answered by the same owner answer.** Done in e6d751e: `NIGHT_ROOTS` counts
   `kiro/mage-arena-vr` and every `worktrees/mage-arena-vr/*`, so `leonardo.mjs`, `leonardo-keyart.mjs` and this lane
   all see VR spend.
3. ~~**Record the 2026-10-24 style and sheet approval.**~~ **Done.** It is recorded in `docs/DECISIONS.md` (7f8103c) and
   VISUAL-PLAN (c)/(g) (f299f2a). The brief for this run named 67a8ff8, which is the T55-T59 cards for answer 6 of the
   same standup.
4. **Which input does Brennic's reconstruction take?** Options:
   - (a) the front s6101 + back s6103 pair as offered (recommended: both pass the gate, and the pair agrees on all five
     set checks);
   - (b) the master alone, as VISUAL-PLAN (d) step 1 says for a failed set;
   - (c) first authorise one more side attempt with a **re-declared side prompt** (a new sha256, committed before the
     call). For example: "seen exactly edge-on, the far arm hidden behind the body, the chest and the belt buckle not
     visible, the head facing the same way as the body", at most 2 calls inside the lane's remaining 6. A new seed with
     the current prompt is not worth spending: s6102 and s6104 are near-identical.
5. **Credit cost of referenced calls.** `COST.MEDIUM` is 14, but a MEDIUM call with two image references bills 18. Should
   the client get a referenced-call cost, so the night-cap reservation is exact? That is a `leonardo-client.mjs` change
   outside today's two allowed edits.
