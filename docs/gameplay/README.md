---
title: Gameplay (VR channel)
channel: vr
status: partial
verified-against: 1fb1b8b (2026-10-07); anchors into files changed since 46643e2 re-verified
sources:
  - "docs/DECISIONS.md"
  - "docs/PROJECT-PLAN.md"
  - "docs/DESKTOP-INPUT.md"
  - "apps/vr/Game/Source/MageArenaVR"
  - "apps/vr/data"
---

# Gameplay - VR channel

This folder is the structured, as-built reference for how the VR channel of Mage Arena plays: a seated, hands-first
spell duel on a raised dais. It documents the VR channel only. The TV/PC channel (`kiro/mage-arena-tv`) plays differently
(walking, mouse and keyboard, a six-week season) and is mentioned only where VR deliberately diverges from the shared,
pinned combat data. The campaign and story side is in [`../campaign/`](../campaign/README.md).

Every page describes what the **code and data** do at the verified commit. Where the plan says something else, the page
documents the build and lists the difference under "Status and gaps". Nothing here is a wish list, except where a page
says `status: proposed`.

## Authority order

1. `docs/DECISIONS.md` - owner decisions, newest first. They outrank everything below.
2. Code (`apps/vr/Game/Source/MageArenaVR/`) and data: the VR overlay `apps/vr/data/vr/*.json`, and the pinned shared data
   `apps/vr/data/pinned/**`, which is owned by `kiro/mage-arena-tv` and never edited here (see `apps/vr/data/README.md`).
3. These pages.
4. `docs/PROJECT-PLAN.md` section 6. It is partly superseded; the pages name each superseded statement.
5. The records behind them: `docs/design/`, `docs/design-findings/DF-*`, `docs/change-requests/CR-*`, `apps/vr/tasks/T*`.

## Page format

Every page starts with YAML front matter: `title`, `channel: vr`, `status`, `verified-against`, `sources`, `data`
(the data files and keys it reads numbers from). Then a summary, rule and number tables (value, unit, source key),
"Status and gaps", and "Open questions".

| Status | Meaning |
|---|---|
| `implemented` | Built and covered by tests or captures at the verified commit |
| `partial` | Some of it is built; the page says which part |
| `designed` | Decided or specified, with no code yet |
| `proposed` | A recommendation awaiting the owner |

## Pages

| Page | Covers | Status |
|---|---|---|
| [01-body-and-space](01-body-and-space.md) | Seat, dais, the three rune pads, threat arc, narrow-FOV mode, floor against dais | partial |
| [02-verbs-and-gestures](02-verbs-and-gestures.md) | Bolt, sigil line cast (circle + bar), Ward and perfect ward, Blink, Seal, split hands, planted staff | partial |
| [03-tier-clock-and-flow](03-tier-clock-and-flow.md) | Tier clock, perfects, Flow beads, Crest | partial |
| [04-threat-language](04-threat-language.md) | Absorb / dodge / leave colours and shapes, reaction bands, steel chip | partial |
| [05-schools/](05-schools/README.md) | The four schools; [Water](05-schools/water.md), [Fire](05-schools/fire.md), [Earth and Air](05-schools/earth-and-air.md) | partial |
| [06-school-defences](06-school-defences.md) | Defences A-E as built (split hands, staff, fire wall, stone form, air form) | partial |
| [07-enemies-and-ai](07-enemies-and-ai.md) | Soldiers, creatures, dummies, the mage AI and its competence levels, the Games | partial |
| [08-session-arc](08-session-arc.md) | The whole run, from cold start through teach and bouts to retry | partial |
| [09-pause-comfort-accessibility](09-pause-comfort-accessibility.md) | Pause and resume, left-hand mode, Gentle, narrow FOV, colour doubling | partial |
| [10-data-reference](10-data-reference.md) | Every data file the build reads: owner, keys, units, loader, how to change it | implemented |

Related, not duplicated here: [`../DESKTOP-INPUT.md`](../DESKTOP-INPUT.md) (desktop play through the gesture pipeline),
[`../CLIP-SCHEMA.md`](../CLIP-SCHEMA.md) (hand clip format), [`../schools/FIRE.md`](../schools/FIRE.md) (Fire in detail),
[`../design/SCHOOL-DEFENCES.md`](../design/SCHOOL-DEFENCES.md) (where the defences were designed).

## Largest gaps between the plan and the build (2026-10-07)

The pages give the detail and the code locations. These are the ones a reviewer should know first:

- **The Fire mage is not in the default run.** `bFireMages` defaults to false, so the duels are Water-proxy mages; Fire
  runs only in tests and captures.
- **The run is the pinned four-wave Tiro ladder, not the plan's three bouts.** Missing beats: the Tide Orb tier IV
  branch pick, the Fire mage intro card, the slow-motion finisher, the victory card, the champion replay.
- **The tier IV Seal (mudra) is not detected.** Tier IV casts from the sigil alone.
- **Bolt is a forward index flick aimed along the view, with no soft-lock**, not the plan's pinch-snap.
- **Mirror tier II is Ripple, not Reflection.** The pinned `Rotation` preset uses branch B, so the plan's signature
  "catch and return the fireball" moment is not in the run.
- **The ward arc follows the view aim, not the palm normal**, and the perfect window starts when the raise reaches the
  kernel, not at motion onset.
- **There is no audio.** No drone, chime, bell or flank cue, so the "colour doubled by sound" rule is half met.
- **The HUD is one head-locked cuff** showing tier or Flow, not two wrist bracers.
- **Earth and Air, stone form and air form have no code.** The defence numbers are not signed off.
- **The seated run is short and unbalanced.** It completes in about 2.8 min with the calibration proposal, and the live
  overlay loses bout 1 at 23.25 s (DF-001 to DF-004).

## Maintenance

- When code or data changes a documented rule, update the page in the same commit and move `verified-against`.
- A change to pinned data is a change request to `kiro/mage-arena-tv` (`docs/change-requests/`), never an edit here.
