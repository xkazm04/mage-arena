Status: draft, for the owner

# Rules checklist

Item 9 (rules) and item 10 (sponsor funding) of the K6 list (`docs/research/KPI-2026-10-20.md` K6). The rules come from
the Devpost rules page as the plan quotes them (`docs/PROJECT-PLAN.md` section 2, "Rules the plan did not list", re-read
2026-10-07). The owner finalises; no row is signed off by this draft.

Marks:

- **met**: the repo records the evidence and nothing is left to do for the rule.
- **open**: work or an audit is still to do, and the evidence path says where it will land.
- **for the owner**: only the owner can confirm it.
- **closed**: the owner ruled on it and the ruling is recorded. What the ruling makes a condition stays open in the ledger it names.

No row is **met** yet. Row 6 is **closed** by an owner ruling (not met: the ledger it requires still has open rows). The build, the art, the story text and the audio are not final, so a rule that depends on them
cannot be checked today.

## The checklist

| # | Rule (quoted from the plan) | Mark | Evidence now | What closes it |
|---|---|---|---|---|
| 1 | **English:** "Projects must be entirely in the English language or include English subtitles" (`docs/PROJECT-PLAN.md` section 2) | open | The game's text and voice are not final: the story bible and slice script (C2) and the voice cue sheet (T27) have no card text yet (`docs/design/V2-ROADMAP.md` lane B rows 12-13; `docs/research/STATE-EVAL-2026-10-20.md` section 2) | A read-through of every on-screen string, voice line and the video voice-over, after C2 and T27 land |
| 2 | **Free of charge:** "Projects must be made available free of charge" (section 2) | open | The plan treats the competition build as free; no store price is decided (`docs/PROJECT-PLAN.md` section 11, "Launch date": a store release is only proposed) | The owner confirms no paywall or paid item in the Competition build; the code is read for any purchase path |
| 3 | **No logos or ads:** "Projects must not contain any commercial or corporate advertising" (section 2) | open | The game is greybox only; no art style is picked (`apps/vr/tasks/T16-style-pick-replay-capture.md:3`). Nothing branded is recorded in the repo | An audit of the build, the video and the store art for brand marks, after the style lands; the video sheet's caption question (`VIDEO-BEAT-SHEET.md`, "Open points") |
| 4 | **No identifiable persons:** "Projects may not mention or depict any identifiable person other than Entrants" (section 2) | open | The characters are fictional named mages and rivals from the game's own story (`docs/campaign/`, `docs/design/V2-VISION.md`). The portrait in the plan came from the PC sibling (`docs/PROJECT-PLAN.md` section 6.4) | Read the final names, voices and portraits for any real likeness. The video shows only the entrant. The owner confirms the entrant is the only person on camera |
| 5 | **Released Meta tools only:** "Use of Meta tools, SDKs, or features that have not been released to the Start Program or the general public is not permitted" (section 2) | open | The project enables `OculusXR` and `OculusInteraction` as optional plugins (`apps/vr/Game/MageArenaVR.uproject:18-19`); the decision keeps Meta XR v207 and Interaction SDK v207 (`docs/XR-TOOLCHAIN.md:7`). That they are released versions is not recorded in the repo | The owner confirms both versions are public releases. A review of every MetaXR plugin option for an experimental flag. Both are Wolf-side checks |
| 6 | **Asset originality:** the project must be original to the entrant, public domain, or Meta-provided (section 4, "Originality"). "No Fab or stock packs" (plan section 10, R8) | closed | Owner ruling 2026-10-21 (`docs/DECISIONS.md`, "2026-10-21 - Asset originality: AI-made assets ship with a licence ledger (RULES-CHECKLIST row 6)"): Leonardo, TRELLIS.2 and ElevenLabs assets may ship, with the entrant as author by prompting, selecting and editing; the owner accepts the risk that Meta reads "original" to exclude AI output. The condition is the ledger `docs/submission/ASSET-LEDGER.md`, which has a source line and a licence line per asset. Rule text and verdicts: "Rule 6: quoted text and verdicts" below | Nothing for the row. The ledger's open rows (Leonardo terms not read; ElevenLabs paid-plan and music "Studio Games" conditions; Grok and Antigravity images not covered by the ruling; sources unknown) close in the ledger, not here |
| 7 | **Sponsor funding:** the Project "must not have previously received or currently be receiving direct funding from Sponsor" (section 2) | for the owner | The Start program membership was approved (`docs/DECISIONS.md` 2026-10-02, "Start program approved"). Whether it carries direct funding is not recorded; the plan marks it "[confirm, owner]" | The owner states in writing that no direct funding was or is received. [confirm] |

## Further rules that touch the same form

These are not in the list the task named, but the plan records them and a reviewer will ask.

| Rule | Mark | Evidence now | What closes it |
|---|---|---|---|
| **New Experience:** built inside the window that opened 24 Sep 2026, with no pre-existing codebase (section 2) | for the owner | Reuse of the TV kernel, data and design is provisionally accepted (`docs/DECISIONS.md` 2026-10-07, Q9). The question to the organizers is drafted and was due before 12 Oct; its status is "draft, not sent" (`docs/submission/ORGANIZER-QUESTION-Q9.md`) | The owner posts it and records the answer. A "no" is a scope decision for the owner |
| **One entry per individual** (section 2) | for the owner | The TV channel cannot also be entered by the same individual | The owner confirms the entrant identity and that the TV channel is not entered. [confirm] |
| **Sibling TV channel not released or shown as a shipped title** | for the owner | Listed as unconfirmed in `docs/submission/ORGANIZER-QUESTION-Q9.md`, "Before you post" | [confirm] |
| **No AI-generated video as the pitch** (section 6, Polish & Presentation: "Don't lean on AI-generated video to carry the pitch.") | open | Guidance in the judging criteria, not a prohibition in section 4 (fetched 2026-10-21). The video plan uses captured gameplay only (`VIDEO-BEAT-SHEET.md`); no video exists | Check the final cut |
| **Video** under 3 min, public, shows a Quest device or simulator (section 2) | open | The beat sheet sums to 2:50; no capture exists | Item 7, milestone 4 |
| **Build:** an APK on the "Competition" channel with an invite URL (section 2) | open | No APK has ever been produced (`docs/XR-TOOLCHAIN.md:9`); the dashboard app and the channel are not recorded as created (`docs/PROJECT-PLAN.md` section 12, question 1) | Milestones 3 and 4 (K5, K6 item 8) |
| **Freeze:** the entry cannot change after the entry period and must stay available until winners are announced (section 2) | open | Deadline: 18 Nov 2026, 12:00 PT, which is 20:00 UTC and 21:00 in Prague (section 2) | Submit before the deadline; keep the channel up |
| **Min 60 fps** (Technical criterion, section 2) | open | Draw calls fail the 150 budget: median 194, max 201 (`docs/research/BUDGET-DG4-2026-10.md` 10.3). No device frame rate is recorded | T37 and the device run |

## Asset ledger (feeds rule 6)

| Asset group | Source on record | Mark | Evidence |
|---|---|---|---|
| Game code, kernel, data, design | Authored by the entrant after 24 Sep; the TV part is the Q9 question | for the owner | `docs/DECISIONS.md` 2026-10-07 (Q9) |
| Editor tooling from before the window (pof) | Pre-dates the window; does not ship in the APK. Disclosed in the Q9 draft as question 2 | for the owner | `docs/DECISIONS.md` 2026-10-07 "Disclosure, pof"; `docs/PROJECT-PLAN.md` section 2 |
| Sound effects (20 imported, 15 more added) | Generated with ElevenLabs, the one paid service the owner approved | open: terms read 2026-10-21 (ledger); commercial use needs a paid plan, and the plan in use is not recorded. 35 cues now: 20 from A05 and 15 for Bout 2 | `docs/DECISIONS.md` 2026-10-02; `apps/vr/tasks/T26-element-colour-and-first-audio.md:3` |
| Music (two loops) | ElevenLabs Music `music_v1`, 2 calls 2026-10-21, `apps/vr/art/audio/music/elevenlabs-log.json` | open: Eleven Music terms exclude "Studio Games" from the Free-to-Enterprise-Lite media rights as read; the owner confirms the plan and that a VR game is outside that carve-out | `apps/vr/tasks/T40-two-music-loops.md:3` |
| Voice lines | Pre-rendered ElevenLabs, from a written cue sheet; not made yet | open | `docs/DECISIONS.md` 2026-10-07 ("ElevenLabs approval extends"); T27 |
| Animated opponent (rig and four clips): Brennic, conscript, slinger | Leonardo concept, local TRELLIS.2 mesh, Blender rig and clips (ledger rows) | open: covered by the row 6 ruling; Leonardo terms not read, so the class is open | `docs/DECISIONS.md` 2026-10-20 "T38 asset source" |
| Particle effects (6 Niagara systems) | Made by this repo's commandlet from the engine's default templates; not a stock pack. Does not cover engine sample content or Fab packs | open: ruled, but the final cooked list is not checked | `docs/DECISIONS.md` 2026-10-08 "D-G4 particle systems" |
| Art style | Not picked; the shortlist is studies of the owner's choice | open | `docs/ART-STYLE-STORYBOARD.md`; T16 |
| Lynx AI-assisted asset pipeline (T38 onward): Leonardo images, local TRELLIS.2 meshes, Blender, ElevenLabs | Per "Rule 6: quoted text and verdicts" (2026-10-21) | closed by the 2026-10-21 ruling (row 6). Service terms: TRELLIS.2 read (MIT), ElevenLabs read (paid plan needed), Leonardo **not read** (HTTP 403 and a bot checkpoint), so its condition is open | https://start-developer-competition-26.devpost.com/rules sections 4 and 6; `C:/Users/kazda/ai/lynx-bench/ASSET-PIPELINE.md`; `docs/submission/ASSET-LEDGER.md` |
| Meta Asset Library | Named fallback for R0, not used. Each asset's licence is checked before use and recorded | open if used | `docs/DECISIONS.md` 2026-10-02 "Start program approved" |
| Fab, Marketplace and stock packs | Not used. "No Fab or stock packs" | open: a plan rule, the shipped tree is not yet audited | `docs/PROJECT-PLAN.md` section 10, R8 |

## Rule 6: quoted text and verdicts

Fetched 2026-10-21 (UTC 09:47) from https://start-developer-competition-26.devpost.com/rules. WebFetch returned
"HTTP 403 Forbidden"; `curl` with a browser user agent returned HTTP 200, and the text below is from that copy. Section
numbers are those of the Official Rules on that page.

**(a) Originality and ownership**

- Section 4, Entry Requirements, "Originality": "The Project must be ORIGINAL to each Entrant (or include content that is in the public domain or that is provided to Entrant by Meta specifically for use in the Contest)."
- Section 4, "No Violation of 3rd Party Rights or Harms": "Projects must not infringe, misappropriate, plagiarize, or violate any rights of any third-party including, without limitation, copyright (including moral rights), trademark, trade secret, or rights of privacy or publicity."
- Section 4: "Sponsor may at any time request written proof of ownership or proper license or use of any material comprising an Entry. Failure to evidence such proof timely or to Sponsor's satisfaction may result in ineligibility and disqualification of the Entrant and/or Entry."
- Section 4, Guidelines, "Original, not a wrapper": "Core experience is Entrant-built. Thin wrappers around existing platforms or services (YouTube, ChatGPT, etc.) are highly discouraged."
- Section 9, License: "All Entries and Projects remain the intellectual property of the individuals or Organizations that developed them."

**(b) AI-generated or AI-assisted art, 3D models, audio, video**

- The rules contain **no sentence** that allows or forbids AI-generated art, 3D models or audio, and none that requires disclosure of AI use.
- Section 6, Polish & Presentation (video guidance, not an eligibility rule): "On your video/trailer: show real gameplay that honestly represents the experience. Lead with the story and what makes your project distinct. Don't lean on AI-generated video to carry the pitch."
- Section 5: "AI tools may also be used to assist in the judging, but final decisions will be made by human judges." (about judges, not entries)
- Prize text for "Best Agentic Interaction": "Apps where AI is a core part of the experience ... procedural content ..." shows AI-using entries are in scope, but says nothing about AI-made assets.

**(c) Third-party tools and services**

- Section 4, "Permitted Development Tools": "If a Project uses or relies upon any third-party tools, code, or assets, the participant must be authorized to use them in accordance with any terms and conditions or licensing requirements of the tools, code, or assets."
- Section 4: "Entrants must at all times comply with the letter and spirit of the Meta VR Start Program Terms, Supplemental Meta Platforms Technologies Terms of Service ... and Meta Terms of Services ..., and any other use terms, policies, documentation, and other guidelines applicable to any platforms used to develop, host, operate, testing, distribute, and display any Entry". The Devpost Terms of Service are incorporated by reference.
- Linked terms: the Supplemental Meta Platforms Technologies Terms and the Devpost Terms were fetched (HTTP 200 each), but a search of the raw text found no AI-content clause (the Meta page is script-rendered, so this is **not a full read**). The Meta VR Start Program Terms and Developer Content Guidelines were **not** read. The service terms were read on 2026-10-21 in the second run recorded in `docs/submission/ASSET-LEDGER.md`: TRELLIS.2 and ElevenLabs were read; Leonardo was **not read** (HTTP 403 from the terms page, a Vercel security checkpoint on the app mirror).

**Verdict per source class** (allowed / allowed-with-conditions / forbidden / unclear)

| Source class | Verdict | Conditions and reason |
|---|---|---|
| Leonardo images (openai/gpt-image-2.5-flare) | unclear (originality); allowed-with-conditions (third-party terms) | The rules do not say whether AI output is "ORIGINAL to each Entrant". Ruled shippable 2026-10-21 (row 6). Condition: Leonardo's terms (and, for this model, the upstream provider's terms) must grant commercial use and ownership of output to the entrant. **Not read: HTTP 403 on https://leonardo.ai/terms-of-service, 429 checkpoint on app.leonardo.ai; the condition stays open.** The paid plan in use is not recorded. Written proof of licence must be available on request |
| Local TRELLIS.2 meshes from those images | unclear (originality); allowed-with-conditions | Inherits the image verdict, since the meshes derive from the Leonardo images. Ruled shippable 2026-10-21. Read 2026-10-21: the Microsoft repository and the model card say "This model and code are released under the MIT License." No use restriction on output; the nvdiffrast and nvdiffrec dependencies have their own licences and are not shipped. Condition met for the model; the Leonardo input condition above still applies |
| Blender rigging and animation by Claude + owner | allowed for the Blender work; unclear for the part Claude writes | Blender is a released tool and the owner authors and directs the work. Claude's contribution is AI-assisted, which the rules do not address; same open question as above |
| ElevenLabs SFX/music (pending a key) | unclear (originality); allowed-with-conditions | Ruled shippable 2026-10-21. Read 2026-10-21 (ledger): Terms of Use section 1(c) allows commercial use only for paid users, and the plan in use is not recorded, so that condition is open. Music: the Eleven Music terms exclude "Studio Games" for the lower plans, open. 35 SFX cues and 2 music loops are generated |

**Answered 2026-10-21 (11:49Z): "accept, with a licence ledger"** (`docs/DECISIONS.md`). The question below is kept as asked.

**Question for the owner (not posted; DECISIONS 2026-10-21: Q9 is not posted):** "The Official Rules require that the
Project be 'ORIGINAL to each Entrant' and that third-party assets be used 'in accordance with any terms and conditions or
licensing requirements', but say nothing about AI-generated art, meshes or audio. Do you accept shipping assets made with
Leonardo, TRELLIS.2 and ElevenLabs on the reading that the entrant who prompts, selects and edits them is their author and
holds the service's licence, with the risk that Meta reads 'original' to exclude AI output? If yes, the builder reads the
three services' terms and records the licence line per asset. If no, which classes are cut?"

## What the owner does with this file

- Rows marked **for the owner** need a written answer and a date. Then update this file.
- No row becomes **met** until its evidence is a committed record. This draft sets none.
- The checklist must be re-run on the submission build, not on this greybox.
