Status: draft, for the owner

# Rules checklist

Item 9 (rules) and item 10 (sponsor funding) of the K6 list (`docs/research/KPI-2026-10-20.md` K6). The rules come from
the Devpost rules page as the plan quotes them (`docs/PROJECT-PLAN.md` section 2, "Rules the plan did not list", re-read
2026-10-07). The owner finalises; no row is signed off by this draft.

Marks:

- **met**: the repo records the evidence and nothing is left to do for the rule.
- **open**: work or an audit is still to do, and the evidence path says where it will land.
- **for the owner**: only the owner can confirm it.

No row is **met** yet. The build, the art, the story text and the audio are not final, so a rule that depends on them
cannot be checked today.

## The checklist

| # | Rule (quoted from the plan) | Mark | Evidence now | What closes it |
|---|---|---|---|---|
| 1 | **English:** "Projects must be entirely in the English language or include English subtitles" (`docs/PROJECT-PLAN.md` section 2) | open | The game's text and voice are not final: the story bible and slice script (C2) and the voice cue sheet (T27) have no card text yet (`docs/design/V2-ROADMAP.md` lane B rows 12-13; `docs/research/STATE-EVAL-2026-10-20.md` section 2) | A read-through of every on-screen string, voice line and the video voice-over, after C2 and T27 land |
| 2 | **Free of charge:** "Projects must be made available free of charge" (section 2) | open | The plan treats the competition build as free; no store price is decided (`docs/PROJECT-PLAN.md` section 11, "Launch date": a store release is only proposed) | The owner confirms no paywall or paid item in the Competition build; the code is read for any purchase path |
| 3 | **No logos or ads:** "Projects must not contain any commercial or corporate advertising" (section 2) | open | The game is greybox only; no art style is picked (`apps/vr/tasks/T16-style-pick-replay-capture.md:3`). Nothing branded is recorded in the repo | An audit of the build, the video and the store art for brand marks, after the style lands; the video sheet's caption question (`VIDEO-BEAT-SHEET.md`, "Open points") |
| 4 | **No identifiable persons:** "Projects may not mention or depict any identifiable person other than Entrants" (section 2) | open | The characters are fictional named mages and rivals from the game's own story (`docs/campaign/`, `docs/design/V2-VISION.md`). The portrait in the plan came from the PC sibling (`docs/PROJECT-PLAN.md` section 6.4) | Read the final names, voices and portraits for any real likeness. The video shows only the entrant. The owner confirms the entrant is the only person on camera |
| 5 | **Released Meta tools only:** "Use of Meta tools, SDKs, or features that have not been released to the Start Program or the general public is not permitted" (section 2) | open | The project enables `OculusXR` and `OculusInteraction` as optional plugins (`apps/vr/Game/MageArenaVR.uproject:18-19`); the decision keeps Meta XR v207 and Interaction SDK v207 (`docs/XR-TOOLCHAIN.md:7`). That they are released versions is not recorded in the repo | The owner confirms both versions are public releases. A review of every MetaXR plugin option for an experimental flag. Both are Wolf-side checks |
| 6 | **Asset originality:** the project must be original to the entrant, public domain, or Meta-provided (section 2, "Originality"). "No Fab or stock packs" (section 10, R8) | open | See "Asset ledger" below. The sources are ruled, but the import list is not final | A final ledger with a source and a licence line for every shipped asset |
| 7 | **Sponsor funding:** the Project "must not have previously received or currently be receiving direct funding from Sponsor" (section 2) | for the owner | The Start program membership was approved (`docs/DECISIONS.md` 2026-10-02, "Start program approved"). Whether it carries direct funding is not recorded; the plan marks it "[confirm, owner]" | The owner states in writing that no direct funding was or is received. [confirm] |

## Further rules that touch the same form

These are not in the list the task named, but the plan records them and a reviewer will ask.

| Rule | Mark | Evidence now | What closes it |
|---|---|---|---|
| **New Experience:** built inside the window that opened 24 Sep 2026, with no pre-existing codebase (section 2) | for the owner | Reuse of the TV kernel, data and design is provisionally accepted (`docs/DECISIONS.md` 2026-10-07, Q9). The question to the organizers is drafted and was due before 12 Oct; its status is "draft, not sent" (`docs/submission/ORGANIZER-QUESTION-Q9.md`) | The owner posts it and records the answer. A "no" is a scope decision for the owner |
| **One entry per individual** (section 2) | for the owner | The TV channel cannot also be entered by the same individual | The owner confirms the entrant identity and that the TV channel is not entered. [confirm] |
| **Sibling TV channel not released or shown as a shipped title** | for the owner | Listed as unconfirmed in `docs/submission/ORGANIZER-QUESTION-Q9.md`, "Before you post" | [confirm] |
| **No AI-generated video as the pitch** (section 2) | open | The video plan uses captured gameplay only (`VIDEO-BEAT-SHEET.md`); no video exists | Check the final cut |
| **Video** under 3 min, public, shows a Quest device or simulator (section 2) | open | The beat sheet sums to 2:50; no capture exists | Item 7, milestone 4 |
| **Build:** an APK on the "Competition" channel with an invite URL (section 2) | open | No APK has ever been produced (`docs/XR-TOOLCHAIN.md:9`); the dashboard app and the channel are not recorded as created (`docs/PROJECT-PLAN.md` section 12, question 1) | Milestones 3 and 4 (K5, K6 item 8) |
| **Freeze:** the entry cannot change after the entry period and must stay available until winners are announced (section 2) | open | Deadline: 18 Nov 2026, 12:00 PT, which is 20:00 UTC and 21:00 in Prague (section 2) | Submit before the deadline; keep the channel up |
| **Min 60 fps** (Technical criterion, section 2) | open | Draw calls fail the 150 budget: median 194, max 201 (`docs/research/BUDGET-DG4-2026-10.md` 10.3). No device frame rate is recorded | T37 and the device run |

## Asset ledger (feeds rule 6)

| Asset group | Source on record | Mark | Evidence |
|---|---|---|---|
| Game code, kernel, data, design | Authored by the entrant after 24 Sep; the TV part is the Q9 question | for the owner | `docs/DECISIONS.md` 2026-10-07 (Q9) |
| Editor tooling from before the window (pof) | Pre-dates the window; does not ship in the APK. Disclosed in the Q9 draft as question 2 | for the owner | `docs/DECISIONS.md` 2026-10-07 "Disclosure, pof"; `docs/PROJECT-PLAN.md` section 2 |
| Sound effects (20 imported) | Generated with ElevenLabs, the one paid service the owner approved | open: confirm the service terms allow this use | `docs/DECISIONS.md` 2026-10-02; `apps/vr/tasks/T26-element-colour-and-first-audio.md:3` |
| Music (two loops) | Not imported; the source is an owner input | open | `apps/vr/tasks/T40-two-music-loops.md:3` |
| Voice lines | Pre-rendered ElevenLabs, from a written cue sheet; not made yet | open | `docs/DECISIONS.md` 2026-10-07 ("ElevenLabs approval extends"); T27 |
| Animated opponent (rig and four clips) | Authored by Claude and the owner; the local TRELLIS and Blender pipeline may feed it. The Meta Asset Library and paid credits were ruled out | open: not made yet | `docs/DECISIONS.md` 2026-10-20 "T38 asset source" |
| Particle effects (6 Niagara systems) | Made by this repo's commandlet from the engine's default templates; not a stock pack. Does not cover engine sample content or Fab packs | open: ruled, but the final cooked list is not checked | `docs/DECISIONS.md` 2026-10-08 "D-G4 particle systems" |
| Art style | Not picked; the shortlist is studies of the owner's choice | open | `docs/ART-STYLE-STORYBOARD.md`; T16 |
| Meta Asset Library | Named fallback for R0, not used. Each asset's licence is checked before use and recorded | open if used | `docs/DECISIONS.md` 2026-10-02 "Start program approved" |
| Fab, Marketplace and stock packs | Not used. "No Fab or stock packs" | open: a plan rule, the shipped tree is not yet audited | `docs/PROJECT-PLAN.md` section 10, R8 |

## What the owner does with this file

- Rows marked **for the owner** need a written answer and a date. Then update this file.
- No row becomes **met** until its evidence is a committed record. This draft sets none.
- The checklist must be re-run on the submission build, not on this greybox.
