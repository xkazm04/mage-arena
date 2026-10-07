---
title: VR campaign - index
channel: vr
status: partial
verified-against: mage-arena-vr 46643e2; mage-arena-int b1efd46; mage-arena a7964ad (2026-10-07)
sources: [docs/DECISIONS.md, docs/PROJECT-PLAN.md, apps/vr/data/README.md, apps/vr/data/PINNED.json, docs/change-requests/CR-002-fire-school.md, mage-arena-int:docs/MAGE-ARENA-PLAN.md, mage-arena-int:docs/design/reconciled/data/season.json, mage-arena:docs/campaign/README.md]
---

# VR campaign - index

For the VR channel, "campaign" means the story-bearing progression a player goes through in the headset. Today that is
one seated run of about 10 minutes: a cold start, a teach, then the Tiro Games bouts on the dais, with retry and a
saved bout. There is no camp, no calendar, no season and no LLM Director. Those are cut for the competition
(`docs/PROJECT-PLAN.md` section 6.8). The run is set in the same world as the TV/PC game: the collared mages, the
tents, the Games, missio. That world, the "story core", belongs to the TV/PC channel. This folder documents only the
VR path. It uses TV material as a read-only reference and proposes the VR campaign mechanism for after the
competition. Mechanics such as gestures, the tier clock, threats and enemies are in `../gameplay/` and are linked
from here, not repeated.

## Pages

| Page | What it holds | Status |
|---|---|---|
| [01-story-core.md](01-story-core.md) | The world, the Games, the tents, the factions, and the characters the VR path shows or could show, each with its TV source. Also the verdict on whether VR shares the TV story core. | reference |
| [02-vr-run-as-campaign.md](02-vr-run-as-campaign.md) | The competition slice as the VR campaign today: its beats, framing, the story elements on screen, what is saved and the retry rules, all checked against code and data | partial |
| [03-tv-divergence.md](03-tv-divergence.md) | Each TV campaign mechanism, how VR treats it (kept, adapted, cut or future), and why | designed |
| [04-vr-campaign-design.md](04-vr-campaign-design.md) | The proposed post-competition VR campaign: structure, story delivery, choices by gesture, progression, saving, a data schema sketch, and the owner decisions it needs | proposed |

Mechanics pages (written separately in `../gameplay/`): `01-body-and-space`, `02-verbs-and-gestures`,
`03-tier-clock-and-flow`, `04-threat-language`, `05-schools/`, `06-school-defences`, `07-enemies-and-ai`,
`08-session-arc`, `09-pause-comfort-accessibility`, `10-data-reference`.

## Authority order

Where two sources disagree, the higher one wins. A lower page that contradicts a higher one is a defect in the lower page.

| Rank | Source | Scope |
|---|---|---|
| 1 | `docs/DECISIONS.md` (owner decisions, newest first) | Everything in this repo |
| 2 | `docs/PROJECT-PLAN.md` | The VR plan; sections 3, 6.4 and 6.8 frame this folder |
| 3 | Code and data in this repo at the commit named in `verified-against` | What the VR channel actually does |
| 4 | TV/PC story and campaign data (`mage-arena-int:docs/design/reconciled/data/*.json`) | World, cast, factions, Games ladder, endings. VR reads these and never edits them. |
| 5 | These `docs/campaign/` pages | A description and a proposal. They never override ranks 1 to 4. |

Combat numbers follow `CLAUDE.md` ("Combat data has one owner"): the pinned commit in `apps/vr/data/PINNED.json` is
authoritative for VR, and the VR overlay `apps/vr/data/vr/combat.vr.json` sits on top of it.

## Status legend

| Status | Meaning |
|---|---|
| implemented | Built in this repo and covered by tests or captures. The evidence is named on the page. |
| partial | Part is built and part is planned. The page lists which part is which. |
| designed | Decided in the plan or by the owner, but not (fully) built |
| proposed | A recommendation that has no owner decision behind it yet |
| reference | Material owned by another channel, summarised for VR readers |

Labels for numbers follow the plan: **authored** (chosen by someone), **simulated** (a headless run), **measured**
(a command on named hardware), **felt** (only the owner can judge).

## TV/PC references used by these pages

The TV/PC game lives in the repository `mage-arena`. It has two working trees, and these pages read from both. All
reads were read-only on 2026-10-07.

| Reference | Repo and commit | Committed? | Used for |
|---|---|---|---|
| Reconciled campaign data: `docs/design/reconciled/data/` (`characters.json`, `facts.json`, `season.json`, `season-bridge.json`, `schools.json`, `relationships.json`, `parley.json`, `death-reservation.json`, `arena/arena-tiers.json`) | `mage-arena-int` (branch `integration`), `b1efd464d534f1c491d1cd8128b0a432c8f9554e` | yes | Story core: world facts, cast, tents, Games ladder, endings |
| TV plan `docs/MAGE-ARENA-PLAN.md`, owner concept `docs/OWNER-NOTES.md` | `mage-arena-int` `b1efd46` | yes | TV decisions D1 to D15 and the death system (plan section d) |
| Camp, season, Parley and Director code: `packages/core/src/{camp.ts,camp-session.ts,season.ts,parley.ts}`, `packages/director/src/` | `mage-arena-int` `b1efd46` | yes | What the TV campaign runs |
| Quest decision-graph runtime: `packages/core/src/quest.ts`, `quest-lint.ts`, `quest.md`, `quest.test.ts`, `packages/core/scripts/lint-quests.ts` | `mage-arena-int` working tree on top of `b1efd46` | **no (uncommitted, work in progress)** | The data model that page 04 proposes to reuse |
| Story pack: `story/dialogue/` (`schema.md`, `manifest.md`, `pack.json`, `scenes/01..15-*.json`) | `mage-arena-int` working tree | **no (uncommitted, work in progress)** | Scene format and story beats |
| Contest baselines: `docs/design/baseline-fourteen-nights/` (B/2), `docs/design/reference-the-ledger/` (B/1) | `mage-arena` (branch `main`), `a7964ad93d8a7b2835d0e1caaba553fdc4e9f253` | yes | History of the design. The VR data pin comes from B/2. |
| Campaign and gameplay export: `docs/campaign/*.md` and `docs/campaign/data/`, `docs/gameplay/*.md` | `mage-arena` working tree on top of `a7964ad` | **no (uncommitted, created 2026-10-07)** | A secondary summary only. Every fact cited here was checked against the committed data above where it exists. |
| VR combat data pin | `mage-arena` (worktree `mage-arena-arena`, branch `arena`), `68a4d68d315856c89339d331ac0db7f4784d566f` | yes | The arena tiers, enemies and schools that VR runs (`apps/vr/data/PINNED.json`) |

The VR commit in `verified-against` is `46643e2`. The working tree was at `75b5cc4` when these pages were written. The
three commits between them touch only the blink vignette, the greybox capture and one research note. None of them
changes a file cited here (`git diff --stat 46643e2 HEAD`).

## The rule for TV material

- **VR never edits TV material.** Reading `mage-arena`, `mage-arena-int` or any other worktree is allowed. Writing,
  staging or committing there is not. That includes the uncommitted work in progress cited above.
- **VR never copies TV story data into this repo by hand.** It links to the TV files. When VR needs story data at
  runtime, it pins a committed revision through the same hash-checked mechanism as the combat data
  (`apps/vr/data/README.md`). It never pins from a working tree.
- **Changes go back as change requests.** Every change to shared canon goes to the TV/PC channel as a change request:
  a new world fact, character, ending, schema field or rule. Change requests live in `docs/change-requests/`
  (`CR-NNN-<slug>.md`, following `CR-002-fire-school.md`). The TV owner decides. VR uses the change only after it
  lands and is re-pinned.
- **VR-only presentation stays here.** The dais, the stones, the gesture mapping, the session segmentation and VR
  scene staging are VR's own. They do not need a change request unless they change canon.

## Status and gaps

- The VR campaign is the competition run alone. It is partly built: page 02 lists the built beats and the missing
  ones (intro card, branch pick, victory card and champion challenge are missing).
- No story data (cast, facts or scenes) is pinned in this repo. The only story-bearing data VR runs is the pinned
  `arena-tiers.json` (Tiro Games), plus the kernel labels ported from the TS kernel.
- The TV quest runtime and story pack are uncommitted. Any reuse proposed on page 04 waits for them to be committed,
  and then pinned.

## Open questions

- Who owns VR-authored scenes if the TV story pack later adopts them? Page 04 proposes an answer: TV owns canon, and
  VR owns staging.
- Should TV story data be pinned now, before page 04 is approved? Recommendation: no. Pin it only when a VR scene
  runner exists.
