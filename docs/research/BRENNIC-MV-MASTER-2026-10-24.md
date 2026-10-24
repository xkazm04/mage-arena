---
title: Brennic multi-view master reference (VISUAL-PLAN (d), steps 0-2)
channel: vr
date: 2026-10-24
status: in progress (step-0 schema declared before any call)
---

# Brennic multi-view master reference, 2026-10-24

## Step-0 judge schema (declared before any call)

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
