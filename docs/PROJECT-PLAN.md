# Mage Arena VR: project plan for the Meta VR Start Developer Competition 2026

Written 2026-10-02 by the lead-analyst session (Claude Opus 5.5). This is a plan, not a status report: nothing below has been built beyond the repo scaffold (`b26d438`), and every "measured" number is a target until a command produces it.
Competition: Gaming track, New Experience division. Submission deadline **Wed 18 Nov 2026**; winners about 11 Dec 2026.
Sibling project: the PC Mage Arena (`C:\Users\kazda\kiro\mage-arena`, plan `docs/MAGE-ARENA-PLAN.md`). This VR slice is a sibling, not a replacement. Section 3 covers what the two share.

Revised 2026-10-02 after the owner's decisions in DECISIONS.md; superseded content was rewritten, not kept.

Labels used throughout: **authored** (a number someone chose), **simulated** (produced by a headless sim), **measured** (produced by a command on named hardware), **felt** (only the owner can say it). A claim with no command behind it is **unverifiable**.

---

## 1. Executive summary

**The bet.** There is no hands-only, seated spell duel on Quest where you draw sigils (prior-art scan: War of Wizards is drawn with controllers, Drakheir uses hands-only PvE poses, Waltz of the Wizard is a sandbox, and Lost Sigil and Hand Wizardo are prototypes). Mage Arena VR fills that gap with three verbs, each mapped to the most reliable hand-tracking signal:
- **draw** a sigil with the casting hand (a circle plus an inner stroke picks a spell line);
- **raise** the off-hand palm as a 140° directional ward with a 0.15 s perfect window;
- **flick** to blink between three rune pads.

Under those verbs sits the B/2 "Fourteen Nights" combat model: the tier clock, Flow, and the threat colour language. A deterministic kernel already exists and is calibrated in the PC sibling's TypeScript `core` (W2-W4 committed; see section 3).

The second bet is about tooling. This is **pof's first end-to-end game**. Every gap pof has (and it has no VR, Android-device, store or on-device profiling support today) becomes a pof work item, sequenced just ahead of the game work that needs it. Every measured lesson is forged into the knowledge registry (where it lands is pending, section 9).

**Owner decisions, 2026-10-02 (`DECISIONS.md`): desktop first, art last, VR in November.**
- **October is a desktop build** of the same Unreal project: keys play recorded or synthetic hand clips through the
  one gesture pipeline, the mouse draws sigils (`DESKTOP-INPUT.md`). Simulator target: the **Quest 3** profile only.
  A device (probably Quest 3 512 GB) is bought only after the desktop core gate.
- **The engine stays Unreal.** The WebXR pivot is rejected.
- **Art comes last:** greybox (keeping the threat colour language) until the mechanics work together, then ONE of
  **13 Screen-Print Poster**, **25 Moonlit Silver Nocturne**, **30 Chalk & Slate** (`ART-STYLE-STORYBOARD.md`).

**The tension, resolved in the schedule.** The owner wants gameplay, graphics, models, animation and audio at high
quality by 31 Oct, and no art before the mechanics work together. Both fit only if the mechanics are proven by
mid-month: a **mechanics gate on Sun 18 Oct** (greybox), then a 13-day **art, animation and audio sprint** (19-31 Oct)
in one style, while bouts 2-3 and onboarding are built on mechanics that no longer move. High quality in 13 days
means one style and a small asset list within Quest budgets, not breadth. A mechanics slip shrinks the sprint; it
never moves into November.

**Gates:**

| Gate | Date | Go |
|---|---|---|
| Feasibility | Sun 11 Oct | D-G0 to D-G5 (section 4) measured; toolchain route chosen by Fri 9 Oct |
| **Mechanics gate** | Sun 18 Oct | Dummy duel and Wave 1 play end to end on L1 clips and L2 mouse only; 20 conformance vectors green; recognizer ≥95% (owner) and ≥85% (second person) on held-out mouse draws, ≤1 false cast per minute; Wave 1 replay reaches victory, census in band; verbs and seal play well together (**felt**) |
| **Desktop core gate** | Sat 31 Oct | The full session in the chosen style; every asset under P15 ceilings; the session scene inside section 7 count budgets (≤150 draw calls, ≤350k triangles, ≤2,000 particles, ≤1 GPU emitter); threat cues spatialized, two music loops; Fire duel median 45-80 s; 2 cold testers finish; APK packaged, build boots under the Quest 3 profile; owner verdict on gameplay, graphics, animation, audio (**felt**). A pass releases the device purchase. |
| V1 Device | Wed 4 Nov | Hand-tracked APK on the device: joints ≥60 Hz for 5 min, ≥95% high-confidence frames; cold start ≤15 s; first device perf capture |
| V2 Hands | Sun 8 Nov | On real hands: recognizer ≥95% / ≥85%, ≤1 false cast per minute, ≤30 ms p95 compute; ward p95 ≤60 ms; owner ≥3 perfects in 10; p95 frame time ≤13.9 ms, ≤1 hitch per minute |
| Beta / RC | Wed 11 / Fri 13 Nov | Beta on the "Competition" channel; RC after 3 cold device playtests |
| **Store-upload freeze** | Mon 16 Nov | Last build uploaded; the buffer to Wed 18 Nov is for the video and form only |

**The November risk, stated plainly.** Every headset-dependent step falls in about 18 days: buying and receiving the
device, developer mode, the toolchain on hardware (Meta XR v207 is built against 5.7 and fails to load on 5.8, an
open Meta bug), recognizer and ward tuning on real hands, comfort, real-GPU performance, playtests, store upload and
video. A feasibility failure the old plan would have found on 11 Oct now surfaces on 8 Nov with ten days left; about
19 owner headset hours fall inside 16 days; a late delivery comes straight out of V1. It is the largest risk in the
plan. October cannot remove it but can shrink it, without a headset:
1. **Toolchain:** v207 loads on 5.8.2 or a fallback (F0a-F0c) is chosen by Fri 9 Oct; a hand-tracking APK is
   packaged weekly (P2); the Windows build boots under the Quest 3 profile (P16). Unverified until V1: the device.
2. **Store:** once membership and a dashboard app exist, an October APK goes to the "Competition" channel (P11).
3. **Recognizer:** tuned on mouse paths and noisy synthetic clips with P4 gates running, so V2 is a re-run of a
   fixed protocol on real hands, not new work; L1 clips are re-recorded from the device.
4. **Ward:** onset detection tested on clips with injected latency (0-100 ms); the 0.20 s step is ready as data.
5. **Performance by proxy:** the desktop forward renderer gates count budgets; desktop frame time is never Quest truth.
6. **Sessions and order:** V1-V3 checklists written in October; model and seller chosen so the order goes out 31 Oct (Q2).

**The scope cut for 18 Nov:**
- **Kept:**
  - one school (Water, three lines plus Bolt);
  - one 7-10 minute session: a 60-80 s teach, then three bouts (soldiers, creatures, a Fire mage duel);
  - three AI opponent families running on the same kernel;
  - pause and resume;
  - left-hand mode and a narrow-FOV mode.
- **Cut, and kept as "future plans":**
  - the camp, the LLM Director and the time loop or season;
  - the other three playable schools;
  - Mend and Mire as player lines;
  - the staff strike;
  - async ghost duels;
  - any netcode;
  - voice acting.

**Kill and pivot criteria** (detail in sections 4 and 5):

| When | Trigger | Action |
|---|---|---|
| Fri 9 Oct | Meta XR v207 does not load on 5.8.2 after F0a | Choose F0b (UE 5.7) or F0c (stock OpenXR) now, not in November. |
| Sun 11 Oct | Recognizer below 85% top-1 on held-out mouse draws after fallback R1 | Held hand-signs (pose selection); the differentiator goes. |
| Sun 18 Oct | Mechanics gate missed | Slip at most to Wed 21 Oct and cut bout 2 (creatures); the art sprint shrinks to 10 days. |
| Sat 31 Oct | Desktop core gate missed | The owner decides: buy the device and port a cut slice (two bouts), or stop. No default is assumed. |
| Wed 4 Nov | No hand-tracked APK on the device | The fallback route prepared in October; if none works by Fri 6 Nov, the owner decides whether to submit. |
| Sun 8 Nov | On real hands: recognizer below 85% after re-recording, or fewer than 1 perfect in 10 at 0.20 s | Held hand-signs (poses also below 90%: stop); or hold-to-absorb with the clock on time only. Owner's call. |
| Sun 8 Nov | p95 frame time above 13.9 ms on the full session | P-1 to P-3 (section 4), then two bouts. |
| Wed 11 Nov | Beta not on the "Competition" channel | Freeze features; fixes and polish only. |

---

## 2. What the competition actually requires (checked 2026-10-02)

These come from the Devpost rules page (`start-developer-competition-26.devpost.com/rules`) and the Meta announcement blog.
- **Hands:** "Fully usable with hands end-to-end. Controller support optional." Gaming may be "hands-first (required) or eyes and hands".
- **Build:** an APK on the developer dashboard "under a new release channel named 'Competition'". Hosted links are allowed only for IWSDK/WebXR apps.
- **Video:** under 3 minutes, on YouTube or Vimeo and public, showing the project "as viewed on a Meta Quest device or via XR Simulator". Judges need not watch past 3 minutes.
- **Description:** must cover inspiration, how you built it, future plans and **a target launch date**. The rules page I read gives no word limit, so the brief's "≤500 words" is unverified. Plan for 500 and confirm on the form itself.
- **New Experience division:** "conceived of and built within the competition window (starting September 24, 2026). No pre-existing codebases, no shipped titles, no early access builds repurposed."
- **Judging:**
  - Stage 1 is a pass/fail viability check.
  - Stage 2 scores four criteria at **25% each**: Innovation & Creativity; Experience Design (seated, hands-first); Technical Implementation ("strategic use of platform tools", hand tracking, gaze, FoV-aware design, **min 60 fps**); Polish & Presentation (UI/UX, art direction, sound design).
  - AI tools may assist judging; humans decide.
- **Freeze:** the entry cannot change after the entry period, and must stay available until winners are announced.
- **Genre fit:** the blog lists the Gaming genres as "puzzle, strategy, casual, social, narrative". An action duel is not on that list. Pitch it as a **strategy duel of reading and timing**: compositions, threat language, and the tier clock as a resource decision. Lead the video with deliberate play, not twitch play.
- **Originality:** the project must be original to the entrant, public domain, or Meta-provided. Treat Fab or Marketplace packs and stock music as a risk until confirmed. Prefer generated or authored assets (section 12, Q8).

Consequence for eligibility: the game repo must be **new**. Do not build inside pof's existing `PoF.uproject`, which predates the window (it was on 5.8 by June; `build-5_8-game.log` is dated 2026-06-18). The PC sibling's kernel and art were created on 2026-10-01/02, after the window opened (first commit `1e9de1c`, Thu 1 Oct 23:46). Reusing them is defensible, but confirm it (Q9). pof itself is tooling and does not ship in the APK, provided the bridge stays editor-only (P1).

---

## 3. The sibling: what the VR slice shares with the PC Mage Arena, and what diverges

**State of the PC project, measured from its repos today:**
- **Plan:** owner-approved `mage-arena/docs/MAGE-ARENA-PLAN.md`. It covers Windows, mouse and keyboard, TS core plus PixiJS, the Director via the Claude CLI, a six-week season, and deaths via Plot objects (section d).
- **Roles:** a Sonnet orchestrator, Codex Astra implementing, Fable 5.1 reviewing (section i).
- **The `arena` worktree has a working deterministic kernel:**
  - W2 `2494ab9`, W3 `2126f57`, W4 `4b21c57`;
  - `packages/core/src/arena/*.ts`, 2,449 lines;
  - 47 core tests;
  - a 2,000-fights-per-wave census. Medians are 37.6 / 40.5 / 46.4 / 58.1 s, all inside the authored bands, with no timeouts (`mage-arena-arena/docs/waves/W4-tiro-games.md`, "Final gate result").
- **That census already retuned B/2's numbers:**
  - Rain Needle damage went from 6 to 2.05;
  - other direct Water damage was halved;
  - conscript HP 40 to 38, slinger 30 to 28, mire maw 120 to 100;
  - all of it was edited in the authoritative CSV/JSON (same file, "Calibration log").
  - **The firetv contest copy of B/2 is therefore stale for combat numbers.** The VR slice must consume the tuned data, not the contest original.
- **Uncommitted work:** the `arena` worktree has 20 modified files right now, including `combat.json` (its `arena` geometry now defers to `art/scale-contract-v1.json`). The VR project pins only **committed** revisions, never a working tree.
- **Art:** the owner chose **Tessera & Lime** for the PC game on 2026-10-02 (`mage-arena-art/art/OWNER-CHOICE.md`). Portraits (A2), camp assets (A4) and icons with spell mappings (A5) are committed in the art worktree. **The VR game does not inherit it** - see the owner's art decision under section 1.

**What the VR slice shares:**

| Shared | Source | How VR uses it |
|---|---|---|
| Absorb rules (140° arc, 0.15 s perfect, raise cost, drain, reductions, mana return, clock advance) | `combat.json` `absorb`, `tierClock`, `flow`, `threatLanguage`, `reactionBands`, `betweenWaves` | Verbatim. The palm ward is the absorb. |
| Water spell lines, as tuned | `spells-water.csv` at a pinned arena commit | Three lines plus Bolt. Numbers are re-censused for VR input (below). |
| Enemy roster and AI mage competence ladder | `enemies.json` (tuned HP), `arena-tiers.json` Tiro waves 1-3 | Soldiers, creatures, the mage duel. |
| School identities (Water Flow, Fire Heat) | `schools.json` | Player Flow; the Fire mage's Heat. |
| Fire spells for the AI opponent | `reference-the-ledger/design/data/spells-fire.csv` (B/1: Ember Dart, Flick Flame, Brand Lance, Ring of Cinders, Pyre Circle, Sunfall) | AI-only subset until the PC W8 authors Fire lines in B/2 format. |
| Kernel semantics and test list | `mage-arena-arena/packages/core` (tests: window boundaries, arc ±69/71°, stream, families, determinism) | The C++ port must pass the same cases (conformance vectors, P7). |
| Census method | `report:w4` (seeded fights, medians in band, dead-air buckets, no forced outcomes) | Re-run with a **VR reference controller**. |
| Lore and names | Tide Tent, Ember Tent, Hastatus conscript, Funditor, Cinder hound, Mire maw, missio, the collar clock | Same world, same names. |
| Art direction | Tessera & Lime palette, fresco rendering, broken umber contours (`mage-arena-art/art/STYLE.md`) | **Not shared (owner, 2026-10-02).** The VR look is chosen later from the shortlist 13 / 25 / 30 in `ART-STYLE-STORYBOARD.md`; until then the game is greybox. PC portraits may still serve as opponent intro cards if the chosen style accepts them. |

**What must diverge:**

| Area | PC | VR |
|---|---|---|
| Engine | TS core plus PixiJS | Unreal C++ (port of the kernel subset) |
| View | Oblique 55° camera, character 4-6% of screen height (`scale-contract-v1.json`) | First person, seated. The scale contract does not apply. |
| Input | WASD, mouse aim, right button absorbs, Space rolls | Sigils, palm ward, flick blink, two-hand mudra |
| Movement | Walk, sprint, roll, staff strike | Three pads plus blink. No walking, no strike. |
| Loadout | Composition screen, five lines | Fixed three-line loadout plus one branch pick |
| Meta layer | Camp, Director, season, deaths | Cut (future plans) |
| Assets | Hand-drawn billboards | Low-poly 3D, Niagara, spatial audio |

**Recommendation on single source of truth: yes for data and rule semantics, no for implementation.**
1. **Data authority stays in the mage-arena repo.** The VR repo imports a pinned commit through a hash-checked sync (P8). VR-only parameters live in a VR-owned overlay, `combat.vr.json`: pads, blink, sigil timing, ward-detection latency, narrow-FOV spawn arc. That follows the registry law "one authority per quantity" (`ai-registry/knowledge/game-production/_laws.md`, anchor `one-authority-per-quantity`).
2. **The TS kernel is the rules oracle and the balance engine.** It already runs 2,000 seeded fights per wave in seconds. The UE C++ kernel is a runtime port, held to the oracle by conformance vectors: the same scripted input traces must produce the same event logs, tick-exact, with float tolerance on positions (P7).
3. **VR needs one kernel extension.** That is a blink input plus pad positions, replacing move, roll and strike. It is a change to shared semantics, so it is **filed as a change request** to the mage-arena orchestrator's status table for the owner to approve. VR sessions never edit mage-arena.
   - If it is not merged by Sun 11 Oct (the kernel port starts in D1 and the mechanics gate is 18 Oct), the VR repo carries a thin adapter package, `vr-sim/`. It imports the core at a pinned commit and wraps it. The divergence is logged in the VR plan. This keeps the PC project's orchestration (Sonnet, Astra, Fable) undisturbed.
4. **pof's place.** pof drives the VR project: harness, MCP, gates, assets. It does not orchestrate the PC project.

The two projects meet at four seams only:
- the data pin;
- change requests;
- read-only reuse of art;
- one shared registry harvest. PC supplies the RPG, camp and Director evidence; VR supplies the immersive and Unreal evidence.

---

## 4. Feasibility week (Mon 5 Oct - Sun 11 Oct): desktop and toolchain gates

**Machine facts, read 2026-10-02:**
- The repo and `Game/MageArenaVR.uproject` exist (`b26d438`): one Runtime module, Enhanced Input, forward shading on (`Game/Config/DefaultEngine.ini`). The editor target builds in 54 s on UE 5.8.2 (**measured**, `CLAUDE.md`). No XR plugin is enabled yet.
- UE **5.8.2** is installed (`C:\Program Files\Epic Games\UE_5.8`, launcher manifest `5.8.2-56702186`). There is no 5.7 install.
- The engine ships stock `OpenXR`, `OpenXRHandTracking` and `XRBase` plugins and `Engine/Platforms/Android`.
- **Meta XR plugins are not installed.**
- Android command-line tools are present:
  - `ANDROID_HOME=C:\Users\kazda\scoop\apps\android-clt\current`;
  - NDK `28.2.13676358`;
  - build-tools 34-36.1;
  - platforms 31, 34 and 36;
  - `adb` on PATH.
- `NDKROOT` is **unset**, and the JDK on PATH is **Zulu 22**. Whether UE 5.8 accepts NDK r28 and JDK 22 is unverified; that is the first thing D-G0 checks.
- **Known open Meta bug (investigation 1053459897316880, status "Investigating"):** the v207 OculusXR, OculusInteraction, MetaXRHaptics and OculusPlatform plugins claim 5.8 support but are built against 5.7.0, and modules fail to load. The ISDK v207 download page lists both 5.8 and 5.7 as supported. Packaged 5.8 samples may need `r.Mobile.ShadingPath=0`.

**Prerequisite (owner):** apply to the Start program (not done yet; eligibility risk until it is), then create the developer-dashboard app and its "Competition" channel (Q1). Developer mode waits for the device.

**No gate this week needs a headset.** What each cannot prove returns in V1-V2 with the on-device numbers of section 1 (plus ≤900 ms p50 pen-down to cast; a hitch is >27.8 ms).

| Gate | What is tested | Go (numeric) | Measured by | Fallbacks, in order |
|---|---|---|---|---|
| **D-G0 Toolchain** (Mon-Wed) | Meta XR v207 (Meta XR + ISDK) in `MageArenaVR` on 5.8.2; an arm64 Vulkan multiview APK with the hand-tracking permission; the Windows build under the XR Simulator **Quest 3** profile | Editor loads with no module-load errors; `BuildCookRun -platform=Android` produces an APK (judged by markers, not exit code alone); the simulator run reaches the boot marker. **Cannot prove:** the APK on a device. | P2 (packaging half), P16, UBT/UAT logs | **F0a:** plugin sources in project `Plugins/`, compiled by UBT against 5.8 (unverified; Meta's manual-build docs are "unclear or missing"). **F0b:** UE 5.7.x (supported by ISDK v207). **F0c:** stock `OpenXR` + `OpenXRHandTracking`; loses ISDK poses and gaze (about 1 day of hand-written pose detection). Chosen by Fri 9 Oct. |
| **D-G1 Gesture clips** (Mon-Wed) | The clip schema (JSONL joint frames with sensor time and confidence; P3 writes the same schema on device), the replay `IHandSource`, L1 keys bound per `DESKTOP-INPUT.md` | 9 actions × 3 variants (normal, slow, sloppy) = 27 clips, schema-valid; key-played and headless replay of one clip give identical event logs | P17, P5 | Fewer variants; the mudra as one keyframed pose |
| **D-G2 Sigil recognition on desktop paths** (Wed-Fri) | $Q multistroke ported to C++, over L2 mouse paths and synthetic clip fingertips projected onto a best-fit plane; 3 slotted inner strokes plus reject; drawing styles **A** (two strokes, pinch as pen-down) and **B** (one stroke) as clip variants | **≥95% top-1** on the owner's held-out mouse draws (3 × 30, templates drawn separately); **≥85%** for a second person (3 × 10); **≥90%** on sloppy variants; **≤1 false cast per minute** over 3 min of noise clips; ≤0.5 ms per stroke end (desktop proxy). **Cannot prove:** air-drawn accuracy, or the A/B choice (final in V2). | P4 on the mouse and synthetic corpus | **R1:** the three most distinct inner strokes, reject threshold tuned on the corpus. **R2:** MiVRy (paid licence to ship, Q7). **R3:** held hand-signs via pose detection, which kills the differentiator. |
| **D-G3 Ward and perfect window** (Thu-Sat) | Ward detector timestamped by **motion onset** (joint velocity), not classifier output, on raise clips with injected latency of 0-100 ms; slow dummy and stream thrower (3 bolts 0.2 s apart); `Space` and RMB at the desk | The `perfect` event at the authored tick ±1 for injected latency ≤60 ms after onset compensation; **0** perfects from a permanently held ward; owner ≥3 perfects in 10 at the desk. **Cannot prove:** real tracking latency or arm fatigue. | P5 scenario replay; owner at the desk | **W1:** window 0.20 s, from data ("do not widen by feel"). **W2:** onset-compensation tuning. **W3:** design kill (section 1). |
| **D-G4 Budget proxy** (Fri-Sun) | The stress scene (greybox arena, 20 enemy proxies, 100 projectiles, 6 Niagara systems, both hands) on the Win64 forward renderer, also packaged into the APK for V1 | Draw calls ≤150, visible triangles ≤350k, live particles ≤2,000, ≤1 GPU emitter, over three 60 s runs; desktop frame time labelled **not Quest truth**. **Cannot prove:** 72 Hz on Quest. | P6 desktop half (UE `-csvprofile` and stat counts into the existing noise-floor gate, `pof/src/lib/profiling/perf-capture.ts`) | **P-1:** fixed foveation high (device), CPU-sim Niagara only, flipbook sprites instead of GPU emitters. **P-2:** cap at 72 Hz, halve particle budgets. **P-3:** lower render scale (device). |
| **D-G5 Build loop** (all week) | Edit → Win64 → headless replay; edit → APK | Win64 edit-to-replay **≤10 min**; APK package **≤15 min** | P2, P5 | Nightly APK builds |

**Go/no-go meeting Sun 11 Oct** (owner, 30 min): read the table filled with measured numbers, confirm the toolchain route and pick the provisional drawing style (A or B).

**Owner time in week 1:** about 2.5 hours at the desk (mouse corpus 45 min, ward timing 20 min, play and review 1 h, slack) plus a second person for 15 minutes. No headset.

---

## 5. Week-by-week schedule to 18 November

**Rhythm** (mirrors the PC plan's wave rule): a design note first, data first, tests with content assertions, a green gate, an owner check for anything only the owner can judge, one commit per item.
- The game repo is `C:\Users\kazda\kiro\mage-arena-vr` (scaffolded, `b26d438`), with the UE project in `Game/`. pof attaches to it as a second UE project (P0).
- October weeks are **D1-D4** (desktop, no headset); November weeks are **V1-V3** (device).

| Week (dates) | Milestone and game deliverable | pof work it depends on (section 8) | Registry output (section 9) | Verification: headless / owner |
|---|---|---|---|---|
| **W0** Fri 2 - Sun 4 Oct | **M0 Ready:** repo and project scaffolded, editor target builds (done, `b26d438`). Remaining: data pin to arena `4b21c57` (`data/PINNED.json`), `combat.vr.json` overlay, clip schema | **P0** second-project support; **P1** editor-only bridge hygiene; **P2** start | Subject proposal for an `immersive-interaction` category (format of `ai-registry/docs/subject-proposal-*.md`); landing waits on the location question (section 9) | Headless: project opens, data hash check. Owner: Start application, dashboard app. |
| **D1** 5 - 11 Oct | **M1 Desktop feasibility verdict** (section 4); the C++ kernel port starts (no headset sessions compete). | **P2** (APK, no install), **P16** (Quest 3 profile), **P17** clip lane, **P4** on the mouse and synthetic corpus, **P6** desktop half, **P11** dry run if the app exists | Phase-1 drafts of `drawn-gesture-command-recognition`, `hand-tracked-timing-windows` (2-4 web searches each); first engine-pitfall entries (plugin version mismatch; editor-only APIs in a runtime module) | Headless: P4 confusion matrix, APK packaged, simulator boot. **Owner: about 2.5 h at the desk.** |
| **D2** 12 - 18 Oct | **M2 Mechanics gate Sun 18 Oct** (greybox): kernel port (absorb, tier clock, Flow, Bolt, projectiles, telegraphs, dummies, bots), clips and mouse to the kernel `InputFrame`, pads and blink, wrist rune HUD, Tide Orb, Lash, Mirror (tiers I-IV), the mudra seal, Crest, the soldiers wave, missio and retry. **Provisional VR census** on clip and mouse timings (**authored**), overlay data only; re-run on real hands in V2. | **P8** data sync, **P7** conformance gate, **P5** scenario driver (desktop replay), **P18** capture (for the style pick) | Draft `cross-runtime-rules-conformance` once 20 vectors are green | Headless: kernel unit tests (port of the TS list), 20 conformance vectors, Wave 1 replay to victory, census in band. **Owner: 2 h** plus the gate review (30 min, **felt**). |
| **D3** 19 - 25 Oct | **M3 Style pick and content:** style chosen Mon 19 Oct from P18 captures of one duel replay re-skinned in 13 / 25 / 30 (unlit); the art, animation and audio sprint begins. Creatures wave (cinder hounds with death bursts, mire maw), the Fire mage AI (competence 1 and 1.5, Heat, Sunfall unblockable), onboarding (first 60-80 s), pause and resume, session flow, left-hand mode (mirrored clips), narrow-FOV mode (camera FOV on desktop). **Content freeze Sun 25 Oct.** | **P15** asset budgets, **P9** Niagara, **P13** music and spatial cues, **P12** VR canon, **P14** catalog mirror | Phase-1 drafts of `seated-immersive-arena-design`, `field-of-view-aware-signalling` | Headless: full-session replay to victory, Fire duel census, P15 on every import. **Owner: 3 h** (style pick, play). |
| **D4** 26 - 31 Oct | **M4 Desktop core gate Sat 31 Oct** (section 1): the style on arena, opponents, spells and HUD; opponent animation (idle, cast, hit, death) and hand animation; music and spatial SFX; 2 cold desktop testers; core APK packaged, simulator-booted, uploaded if the app exists; V1-V3 checklists; device order ready | **P10** (assembly of desktop review cuts), **P11** | Draft `tracked-input-record-and-replay` (regressions caught by replay versus by a person) | Headless: every October gate re-run. **Owner: 3 h** plus the gate meeting (1 h) and hosting playtests (1 h). |
| **V1** Sun 1 - Wed 4 Nov | **Device bring-up:** delivery, developer mode, install, hands on device, on-device perf of the full session. **Gate Wed 4 Nov.** | **P2** install and launch half, **P3** device recorder, **P6** device half | Phase-1 draft of `standalone-headset-frame-budgets`, with the first device captures | Headless: logcat boot marker within 60 s; perf gate on pulled captures. **Owner: 4 h.** |
| **V2** Thu 5 - Sun 8 Nov | **Real hands:** 180-trace corpus (P3), L1 clips re-recorded, templates and reject threshold re-tuned, style A or B fixed, ward latency by the flash method, census on **measured** distributions, perf to budget. **Hands gate Sun 8 Nov.** | P3, P4, P5, P6 | N1, N2 evidence; the desktop-to-hands delta is the lesson | Headless: P4 on the real-hand corpus; census; perf gate. **Owner: 5 h** plus a second person for 20 min. |
| **V3** Mon 9 - Fri 13 Nov | **Beta Wed 11 Nov, RC Fri 13 Nov:** comfort pass, narrow-FOV check, pause on headset removal and tracking loss, device-only art fixes, 3 cold device playtests | **P11** release lane, **P10** headset pull | N3, N4 evidence (**felt**); draft `deadline-slice-scoping` from the cut log | Headless: RC regression (all gates), package size, boot smoke via adb. **Owner: 4 h** plus hosting 3 playtests (about 1.5 h). |
| **Submit** Sat 14 - Wed 18 Nov | Video Sat-Sun 14-15 Nov; **store-upload freeze Mon 16 Nov**; invite URL tested from a second account; form Mon 16 Nov (target), buffer to Wed 18 Nov | P10 replay re-render (optional) | Evidence collection for the post-submission reconcile | **Owner: 4 h** (video, final play-through, form). |
| **After** 19 Nov - 11 Dec | Freeze (no changes allowed) | pof retrospective | Phase-2 reconcile of every drafted subject (draft → forged), EXTENDS harvest, autonomy-coverage ledger (section 9) | — |

**Total owner time:** about 13 hours at the desk in October and about 19 hours in the headset across 1-16 Nov. The November block is the binding constraint (risk R9).

---

## 6. Game spec for the competition slice

This spec is the VR game; in October the desktop feeds the same pipeline (`DESKTOP-INPUT.md`), and what it cannot prove is checked in V1-V3.

### 6.1 Body and space

- **Seated with yaw comfort.** The player sits facing forward, inside a two-foot radius. Hands work in a "drawing box" from roughly chest to face height, 25-60 cm in front of the body. The ward hand works in front and to its own side.
- **The arena** is a ring floor seen from a raised dais. The kernel is a 2D ground plane (32 × 20 m in B/2 data), presented in 3D. Projectiles fly at chest height; ground telegraphs are drawn on the floor.
- **Three rune pads** sit on the dais in an arc (left, centre, right, about 3 m apart in the virtual world). **Blink** is a snap teleport between pads, with a short vignette and no smooth motion (comfort).
- **Threat arc:** threats come from in front within ±70° by default. Flankers reach ±100° at most and are announced by spatial audio plus an edge chevron. Nothing ever comes from behind (seated players cannot be asked to turn around).
- **Narrow-FOV mode** (for VR glasses, about 70 × 66° against Quest 3's 110 × 96°) compresses spawn and telegraph origins to ±40°. It is offered automatically on a small FOV, and it is the "FoV-aware design" the Technical criterion names.

### 6.2 The three verbs, plus a seal

| Verb | Gesture | Kernel effect (data) | Why this signal |
|---|---|---|---|
| **Bolt** (Rain Needle) | Casting hand open toward a target, then a quick pinch-snap | Projectile at 20 m/s along the hand ray, soft-lock in a 60° cone with head-gaze assist, sticky for 1 s (`combat.json` `aimMode`). Damage as tuned (2.05). | Pinch is the most robust tracked event. Bolt must be spammable without drawing. |
| **Line cast** | Draw a **circle** (arms the cast), then the line's **inner stroke**: Tide Orb = a tap or dot at the centre, Lash = a horizontal slash, Mirror = a vertical stroke. Release casts toward the hand ray or soft-lock. | The line casts its **current unlocked tier**, which climbs in place on the collar clock (B/2 rule). Cast time = data `cast_s` plus the measured draw time (the census uses the measured distribution). | The circle is a consistent "pen-down" frame for the recognizer, and a single shared arming shape keeps the vocabulary to 3 + reject. |
| **Ward** | Off-hand open palm raised in front, palm normal forward | 140° arc centred on the palm normal (projected on the ground plane). Raise costs 5 mana and drains while held; perfect = a fresh raise ≤0.15 s before impact, magic only (`combat.json` `absorb`). Perfect gives mana back, advances the clock 2 s and adds 1 Flow. | Big, deliberate, occlusion-safe, and it reads like Doctor Strange. Timing is measured from motion onset (D-G3, then on real hands in V2). |
| **Blink** | Casting hand flicks toward a pad (pointing pose plus wrist angular velocity over a threshold, direction resolves the pad) | Replaces roll: 0.25 s invulnerability and a stamina cost (overlay data, starting from roll's 25). It is the answer to steel and black-core threats. | Directional, one motion. Fallback: head-gaze at a pad plus a pinch (ISDK gaze + pinch, if D-G0 keeps ISDK). |
| **Seal (tier IV)** | After the line sigil at tier IV, both hands form the mudra (palms forward, thumbs and index fingers forming a triangle) within 1.5 s | Releases the tier IV spell: Leviathan Orb (unblockable) or Rain of Orbs on Tide Orb; Drown Coil on Lash; Return Tide on Mirror. | Two-hand poses are what ISDK pose detection does well. It makes the climax physical. |

### 6.3 Loadout, tier clock, Flow, threat language

- **Loadout:** fixed for the slice: **Tide Orb, Lash, Mirror** (the "Rotation" and "Mirror tide" ideas from `spell-sheet-water.md`).
  - At the first intermission the player picks Tide Orb's tier IV branch: Leviathan Orb (unblockable finisher) or Rain of Orbs.
  - Mirror's tier II is fixed to **Reflection**: a perfect absorb sends the projectile back. This is the signature VR moment, catching a fireball with your palm and returning it.
  - Mend and Mire are cut as player lines. Lash's tier II is fixed to Riptide (push), because the player cannot walk away.
- **Tier clock:** tiers at 0 / 15 / 30 / 45 s, minus 2 s per perfect, at least 6 s between unlocks, reset each bout. The clock is shown as **four runes on the casting wrist's bracer** that ignite. Unlocks play a chime from the wrist.
- **Flow:** 0-5 beads on the ward bracer. Casting a different line within 2 s adds a bead; repeating a line, or 3 s idle, resets it; a perfect adds 1. At 5 beads the next cast is a **Crest** (free, ×1.5), and the hand glows.
- **Threat language** (B/2, kept exact):
  - element colour with a shrinking ring = face it and ward;
  - white or steel = blink (warding steel only chips it, 0.30 reduction);
  - black core with a jagged red rim = leave the pad.
- **Reaction bands** stay as authored: positional telegraphs ≥0.40 s, unblockables ≥0.80 s, and no player Down before 5 s.

### 6.4 Session arc (10 minutes or less)

| Time | Beat |
|---|---|
| 0:00-0:15 | Cold start to the dais at dusk. "Raise your palm to begin." |
| 0:15-1:30 | **Teach (first 60-80 s, in the arena, no menus):**<br>1. A slow water glob comes from the front: raise your palm (ward). A second glob is timed for a perfect, shown by a white ring and a bell.<br>2. Draw the circle, then the dot: Tide Orb hits a dummy.<br>3. A black-core lane lights your pad: flick to blink.<br>Flow and the clock are taught by one wrist cue each, during Wave 1. Each step repeats until done, with no fail state. |
| 1:30-2:40 | **Bout 1, soldiers** (Tiro wave 1: conscripts and slingers, tuned HP). Steel teaches the blink. |
| 2:40-3:00 | Intermission: heal 30% of missing HP, refill, clock reset (`betweenWaves`); pick the Tide Orb IV branch. |
| 3:00-4:20 | **Bout 2, creatures** (3 cinder hounds whose death bursts are perfect practice, plus 1 mire maw). |
| 4:20-4:40 | The Fire mage of the Ember Tent walks in (intro card using the PC portrait). |
| 4:40-6:30 | **Bout 3, duel** (competence 1, then 1.5 on retry or for the final). Heat rises; Sunfall comes at about 45 s and must be blinked. **The finisher:** Leviathan Orb or a reflected fireball, with a 0.5 s slow-motion crowd roar. |
| 6:30-7:00 | Victory card: perfects, Crests, time. Offer "the Ember champion" (competence 2.5) as a replay challenge. |

- **Loss** grants missio and an instant retry of the same bout (5 s or less). No bout ends the session unsatisfied.

### 6.5 AI opponents

- **They use the kernel's own mage AI** (`mage-ai.ts`): the same input frame as the player, data-driven reaction delay, aim error, cadence and absorb probabilities, with caps of 0.25 s minimum reaction and 0.45 maximum perfect chance (`enemies.json` `mages`).
- **The VR change:** the AI targets the player's **pad** and has to re-acquire after a blink.
- **The Fire mage** uses B/1 Fire spells mapped to tiers: Ember Dart as its Bolt, Flick Flame and Ring of Cinders at tiers I-II, Brand Lance at II, Pyre Circle at III, Sunfall at IV. Its Heat follows `schools.json`. Balance target: the median duel in the 45-80 s band (`arena-tiers.json`), checked by the census.
- **Known issue to respect:** competence level 4 over-defends and can be weaker than level 3 (W4 note). The slice uses levels 1, 1.5 and 2.5 only.

### 6.6 Pause, resume, cold start

- **Kernel pause:** the kernel is fixed-step, so pausing means not stepping. It triggers on system-menu focus loss, on headset removal, and after **1.5 s of lost hand tracking** in combat.
- **Resume:** raise both palms, then a 3-2-1 count. Telegraphs resume exactly where they were (no "free" hits on resume).
- **Resume after quitting:** quitting mid-session resumes at the start of the current bout.
- **Cold start target:** 15 s or less from launch to interactive, with no login, no account, and no menus before the first gesture.

### 6.7 Comfort and accessibility

- Seated only; no smooth locomotion, no camera motion except the blink snap with its vignette. Head-locked UI is avoided; the HUD is diegetic on the wrists and the dais.
- Left-hand mode swaps the casting and ward hands.
- Colour language is doubled by **shape and sound**: magic threats have a ring, steel is angular, black-core has a rim plus a low drone. This also serves colour-blind players.
- Sessions are short; there is a "Gentle" option that applies competence 1 to every opponent and doubles telegraph times (from data).

### 6.8 What is cut (written as "future plans" in the form)

- the camp, with its Persona-style calendar;
- the LLM Director with a closed verb vocabulary;
- the season (PC) and the time loop (B/2);
- the other three schools as playable;
- the full composition screen and the cut lines Mend and Mire;
- tiers II-IV of the Games;
- async ghost duels (your recorded input versus a friend: the deterministic kernel makes these cheap later);
- realtime PvP;
- VR glasses tuning beyond narrow-FOV mode.

---

## 7. Tech architecture in Unreal

**Project:** `Game/MageArenaVR.uproject` on UE 5.8.2 (or 5.7 if F0b), mobile forward shading, Vulkan, multiview, MSAA 4x, baked lighting, no Lumen or Nanite. Forward shading is on from day one (`r.ForwardShading=True`). If Meta XR v207 loads: OculusXR plus ISDK for hands, poses and gaze. Otherwise stock OpenXR plus OpenXRHandTracking. The scaffold has one module, `MageArenaVR`; the split below is the target, made in D1-D2.

| Module | Type | Responsibility | Key rule |
|---|---|---|---|
| `MageArenaSim` | Runtime, pure C++ (no UObject in the step) | Fixed 60 Hz kernel ported from TS `packages/core/src/arena` (VR subset): actors, absorb state machine, tier clock, Flow, projectiles with swept hits, telegraphs, zones, hit dedup per activation, Down as a state tag, enemy brains, mage AI, Tiro waves, seeded RNG with a draw log, event log. | Double precision to match JS numbers; no wall clock, no unseeded random, no per-step allocation. Same tick order as TS. |
| `MageArenaData` | Runtime | Loads the pinned shared JSON/CSV plus `combat.vr.json` from staged non-asset files; logs the data hash at boot. | Data wins over code; a hash mismatch against the pin manifest fails the dev build (P8). |
| `MageArenaInput` | Runtime | `IHandSource` (live ISDK/OpenXR \| replay clip \| synthetic clip), plus the L2 mouse path entering as a fingertip path (`DESKTOP-INPUT.md`); L1 key bindings that play clips; feature extraction (pinch strength, palm normal, joint velocities, confidence); `SigilRecognizer` ($Q port, plane projection, reject threshold); `WardDetector` (onset timestamp); `BlinkDetector`; `MudraDetector`. Emits a VR `InputFrame` per sim tick. | Hand frames carry sensor time and are quantized to ticks. Tracking loss is a state, not "no input". |
| `MageArenaGame` | Runtime | Seated XR pawn and a desktop pawn (camera at seated head height on the same dais), dais and pads, presentation (Niagara pools, spatial audio, wrist HUD), session flow (teach → bouts → victory), pause/resume, settings (hand, FOV mode, Gentle). | Presentation only **reads** kernel state and events; it never writes a number. |
| `MageArenaTests` | Development only | Kernel unit tests (mirror of the 47 TS tests where in scope), conformance vectors, corpus replay tests, scenario tests. | Judged by abslog markers (pof convention). |
| `MageArenaDev` | Editor only | pof bridge hooks, recorder controls. | Never packaged (eligibility and size). |

**Gesture pipeline:** joints (72 Hz) → smoothing (One-Euro filter) → pen state (pinch, or point pose for style B) → stroke buffer → resample to 32 points per stroke → PCA plane fit, projected to 2D → $Q match against the templates for the 3 slotted lines → reject if the score is below threshold or the circle is missing → `CastLine(line)` event.
- The tier IV path waits up to 1.5 s for `MudraDetector`.
- Templates are the owner's draws (5 per class), stored as data so they can be tuned without code: mouse draws in October, re-recorded from real hands in V2.

**Determinism and headless balance:**
- The TS oracle runs the census with a VR reference controller whose sigil time, misrecognition rate and blink timing are sampled from corpus distributions. In October those come from clips and mouse draws and are labelled **authored**; in V2 they are replaced by the **measured** real-hand corpus. Only the V2 census is honest for hands.
- The C++ runtime is held to the oracle by P7.
- Headless UE runs (`-game -RenderOffScreen` with XR disabled and the replay hand source) give T3 behavioural evidence and T4 frames without a headset (P5).

**Performance budget for Quest 3 at 72 Hz** (authored targets; the count rows are gated on desktop from D-G4, the time rows only on the device from V1):

| Budget | Target |
|---|---|
| Frame time | 13.9 ms; p95 ≤13.9 ms, ≤1 hitch per minute |
| CPU game thread (sim + input + presentation) | ≤6 ms; kernel step ≤1 ms; recognizer ≤0.5 ms per stroke end |
| Draw calls (multiview) | ≤150 |
| Visible triangles | ≤350k (hands + arena + 20 actors) |
| Particles | ≤2,000 live, CPU sim preferred; at most one GPU emitter; additive, small screen coverage; no full-screen translucency except the blink vignette |
| Textures | ASTC; ≤500 MB resident |
| Package | APK ≤1 GB |
| Lighting | Baked lightmaps, one dynamic light at most, blob shadows |

Thermal behaviour is recorded in every device perf capture (CPU and GPU levels, if the capture exposes them; unverified until V1).

---

## 8. Pillar 1: pof through the whole lifecycle

**What pof is today** (file-grounded):
- **Shape:** a Next.js studio app with 147 contexts in 8 groups (`pof/CLAUDE.md`).
- **The UE project** lives outside the repo at `C:\Users\kazda\Documents\Unreal Projects\PoF\PoF.uproject`: `EngineAssociation 5.8`, modules `PoF` and `PoFEditor`, with GAS, StateTree, MetaHuman and others enabled and **no XR plugin**.
  - `pof/ue/` holds only the Python `PoFToolset` plugin.
- **The engine bridge:** the `PillarsOfFortuneBridge` plugin on `:30040` (`docs/features/harness-llm-unreal/llm-ue-interface.md` §2).
- **MCP tools:** **56 pof MCP tools** (counted in `tools/pof-mcp/src/tools/*.ts`), plus Epic's 5.8 first-party MCP with "28 toolsets / 350+ tools" (`docs/ue58-mcp-phase2-tool-map.md`), plus the PoFToolset gap-fillers (`docs/ue58-mcp-convergence-plan.md`, Slice 2, 2026-06-18).
- **The autonomous builder** (`src/lib/harness/`, `docs/features/harness-llm-unreal/autonomous-builder.md`): plan → Claude Code sessions → gates (typecheck, lint, test, UBT compile, abslog-judged UE tests, opt-in game-boots visual gate) → self-heal → git checkpoints, under a budget governor.
- **Domain:** pof is ARPG-shaped (GAS spellbook pipeline, `docs/features/spellbook/README.md`). It has never built Android or touched VR.
- **Two facts with direct consequences:**
  1. **The last standalone (non-editor) 5.8 game build failed.** `pof/build-5_8-game.log` (2026-06-18) shows `ScenarioController.cpp(182)` "GetActorLabel is not a member of AActor" and `VSPlayerMovementTest.cpp(100-101)` `UBlueprint::Status`. These are editor-only APIs in runtime code. Any Android target that includes those modules will fail the same way.
  2. **On-device evidence does not exist.** In the pof DB, `build_history` holds only Win64 rows. L4 perceptual gates are 20 deferred and 0 passed. `perf-capture.ts` has never ingested a real capture (`src/types/observation.ts:189-192` marks it UNVERIFIED).

**Size key:** S is 1 agent-day or less, M is 2-4, L is 5 or more. Owner review is not included.

| Lifecycle phase | pof today (cite) | Gap for this game | pof feature to build (where, size, acceptance) | Headless or human |
|---|---|---|---|---|
| Concept and design docs | Catalog Concept Brief steps with canon injection (`src/lib/catalog/pipelines/*.ts`, canon in `src/lib/catalog/canon/`); judge rubrics (`src/lib/judge/rubrics.ts`); GDD synthesizer (`src/lib/gdd-synthesizer.ts`) | Canon is ARPG (`docs/catalog/ARPG-LAWS.md`); no seated, hands or comfort canon. The design source of truth is the mage-arena data plus this plan, not pof's catalog. | **P12 VR canon pack** (`src/lib/catalog/canon/` profile "seated-hands-vr": comfort, threat arc, telegraph minimums, no head-locked UI), **S**. Acceptance: a judge run on this plan's section 6 flags a planted violation ("a threat from behind") and passes the clean text. | Headless |
| Game-design data and tuning | 34 catalog pipelines with L0-L4 acceptance (`docs/catalog/WIRING-AND-ACCEPTANCE.md`); UE DataTable seeding (`Content/Python/seed_generated_abilities.py`) | The data authority is external (mage-arena). pof must not become a second authority. No gesture or sigil data model. | **P8 Shared-data pin and sync** (`src/lib/data-pin/`, MCP `pof_data_pin_check`), **S**. Acceptance: a pin manifest (repo, commit, file hashes); a planted upstream edit reports drift until re-pinned; the import refuses a dirty working tree. **P14 Catalog read-only mirror** (spell and enemy rows mirrored from the pin, marked non-authoritative, so art and VFX briefs see real numbers), **S**. Acceptance: a mirrored row cannot be edited; its numbers match the pinned CSV. | Headless |
| Spell and combat balance simulation | Seeded Monte-Carlo ARPG sim (`src/lib/combat/simulation-engine.ts`; MCP `pof_combat_simulate`, `pof_balance_baseline`) | The ARPG stat model does not fit. The right oracle is the mage-arena TS kernel. pof lacks a way to run an external sim and gate on its report. | **P7 Cross-runtime conformance and census gate** (`src/lib/conformance/`, harness custom gate), **M**. Acceptance: runs the TS oracle on N scripted traces and the UE automation test on the same traces; event logs agree tick-exact on 20 vectors; a planted 1-tick shift of the perfect window in C++ fails the gate; census medians are read from the oracle report and gated against `arena-tiers.json` bands. | Headless |
| Assets: VFX (Niagara) | `vfx` pipeline briefs and LOD bands (`src/lib/catalog/pipelines/vfx.ts`); `PoFNiagaraTools` list and spawn only; Epic's Niagara toolset "ships 0 callable tools" (`docs/ue58-mcp-phase2-tool-map.md:31`); 0 `NS_*` assets in the PoF project | Cannot author or parametrize emitters; no mobile particle budget | **P9 Niagara parametrize and budget** (`ue/PoFToolset` Python: duplicate a template system, set user parameters such as colour, size, rate and lifetime, report emitter count, sim target and max particles), **M**. Acceptance: builds `NS_TideOrb` from a template with 5 parameters; a headless capture shows it; the budget report flags a GPU emitter or more than 300 max particles. **Honest limit:** emitter graphs are still hand- or agent-edited in the editor. | Mostly headless; look and feel is the owner's |
| Assets: 2D art (sigils, runes, banners) | Leonardo and Qwen-Image providers (`src/lib/visual-gen/image-providers.ts:48-76`); icon sets; style DNA | No glyph or path-art tool. Style authority for VR is the owner's shortlist in `ART-STYLE-STORYBOARD.md` (13 / 25 / 30), not the PC art worktree. | No pof feature needed. Sigil glyphs are **authored as vector strokes from the recognizer templates** (game repo), so the art matches the recognized shapes exactly. Banners reuse PC portraits. | Headless; selection by owner |
| Assets: 3D arena and characters | Tripo, Hunyuan3D, TRELLIS, TripoSR runners plus Blender finish plus glb import proven on 5.8 (`src/lib/visual-gen/*-runner.ts`; fleet memory); MetaHuman conform | "Zero real meshes across 36 bestiary candidates" (fleet memory) and Tripo credit exhaustion; no Quest polygon or texture budgets (`src/lib/packaging/size-verdict.ts:106` only checks 4 GB) | **P15 Quest asset budget profile** (`src/lib/visual-gen` gates: per-class triangle and texture ceilings for mobile XR, ASTC check on import), **S**. Acceptance: importing a 60k-triangle "soldier" fails the ceiling (8k); a compliant one passes. Opponents stay stylized and low-poly; MetaHumans are **out** (too heavy for Quest). | Headless |
| Assets: audio | ElevenLabs `/v1/sound-generation` only (`src/lib/audio-gen/providers/elevenlabs.ts:9`); UE import (`Content/Python/import_duel_audio.py`) | No music provider, no TTS ("the `tts` AudioKind has NO provider", fleet memory) | **P13 Music lane plus spatial cue check** (a music provider behind the existing provider seam; a spatialization flag check for threat cues), **M**. Acceptance: two 60 s loops generated and imported; every threat-cue SoundCue has attenuation and spatialization on (scan). Voice stays cut. | Headless; mix is felt |
| UE scaffolding, C++ and Blueprint via MCP | Epic MCP toolsets (Blueprint, asset, material, data tables, LiveCoding, AutomationTest); `pof_ue_compile`/`pof_ue_build`; autonomous builder with UBT and abslog gates | pof is configured for one UE project via `.env` `POF_UE_UPROJECT`. The bridge plugin's runtime module plus editor-only calls break non-editor targets. | **P0 Second-project profile** (per-project UE env: uproject, engine, target platform; MCP tools take a project id), **S**. Acceptance: `pof_ue_status` reports `MageArenaVR` without editing `.env`; PoF still works. **P1 Editor-only hygiene gate** (preflight scan for editor-only APIs in Runtime modules, e.g. `GetActorLabel`, `UBlueprint::Status`; the bridge's runtime part excluded from Android), **S**. Acceptance: the scan names both known PoF offenders; `MageArenaVR` Android compiles with the bridge enabled in the editor and absent from the APK. | Headless |
| Desktop gesture input (October) | pof scenario runs replay timed **keys** and capture PNG frames (`pof/scn-5_8-walk.json`: `inputs: [{key, start, duration}]`); nothing for hand data | No way to author, vary, mirror or version hand clips | **P17 Gesture-clip lane** (`src/lib/gesture-clips/`, MCP `pof_clip_*`: the P3 schema; a synthetic generator from sigil strokes and pose keyframes with fast, slow and sloppy variants and an authored noise model of jitter, dropouts and confidence dips; trim, retime and left-right mirror; an L2 mouse draw saved as a clip; the key-to-clip table of `DESKTOP-INPUT.md`; a hash manifest), **M**. Acceptance: the 27 D-G1 clips schema-valid and byte-identical on re-generation; a mirrored clip plays left-hand mode; in V2 a P3 device trace opens unchanged. | Headless; feel is the owner's |
| Desktop capture for review | PNG frames only; ffmpeg already samples video (`src/lib/visual-gen/footage-gate.ts`) | Reviews and the style pick (three skins of one replay) need video with audio | **P18 Desktop review capture** (a P5 replay on Win64 recorded at 60 fps with audio; an ffmpeg MP4 overlaid with the clip fired and the recognizer result), **S**. Acceptance: the overlay of a 60 s Wave 1 replay agrees with the event log at every cast; three skins show identical event times. | Headless; judging it is the owner's |
| VR and hand-tracking integration | **Nothing** (0 hits for OpenXR, MetaXR, HandTracking, XR Simulator) | Total | **P3 Hand-trace recorder and corpus** (V1-V2, writing the P17 schema) (game-side `UHandTraceRecorder` writes JSONL joint frames with sensor time and confidence; pof `src/lib/device/corpus.ts` pulls by adb, labels by prompt ("draw Lash now"), versions by hash; MCP `pof_corpus_pull`/`pof_corpus_list`), **M**. Acceptance: 180 labelled traces pulled and schema-valid; a re-pull is byte-identical; the corpus manifest records device, build and date. **P5 XR scenario driver** (built in D2 on desktop, where it is the main October test path; extend the `UScenarioController` pattern: replay hand source in `-game -RenderOffScreen` with XR off, timeline of trace files, observe kernel events, capture spectator frames), **M**/**L**. Acceptance: the scenario "perfect vs glob dummy" yields a `perfect` event at the authored tick ±1 and a frame showing the white ring, with no headset attached. | Recording needs the owner; everything after is headless |
| Build and packaging | `src/lib/packaging/` (UAT generator with an Android branch, `uat-command-generator.ts:185-188`; Android profile arm64, SDK 26/34, `build-profiles.ts`); `smoke-test` Win64 only; MCP `pof_package_preflight` | No APK ever built; no Quest settings; NDKROOT unset; JDK 22 compatibility unknown; no device deploy | **P2 Quest APK lane** (packaging half in D1 against no device; install, launch and logcat in V1. Quest build profile: arm64, Vulkan, multiview, ASTC, Meta manifest flags, hand-tracking permission; toolchain preflight for SDK, NDK, JDK versions against the engine's requirement; signing; MCP `pof_quest_package`, `pof_device_install`, `pof_device_launch`, `pof_device_logcat`), **M**. Acceptance: from a clean checkout one call produces an APK, installs, launches, and finds `MAGEVR_BOOT_OK` in logcat within 60 s; failures are classified as preflight, compile, cook, package, install or launch, and recorded in `build_history` with platform Android. | Headless with a headset plugged in by USB (owner wears it only for smoke) |
| Testing (device and XR Simulator) | UE automation plus functional tests, ScenarioController (`Source/PoF/Testing/ScenarioController.h`), L3/L4 drain (`src/lib/test-gate-runner/`) | No XR Simulator flow. The simulator's hand support is four canned poses (aim, poke, pinch, grab) driven by mouse and keyboard (Meta docs), so **it cannot draw sigils**. Real coverage comes from replayed traces (P5). | **P16 XR Simulator smoke** (launch the PIE or Windows build under the Meta XR Simulator's **Quest 3** profile, the only simulator target; assert boot, pause and resume, menu poke), **S**. Moved to D1. Acceptance: three scripted checks pass on Windows without a headset. Gesture and timing truth stays with P4/P5 plus the owner. | Headless (narrow scope) |
| Automated playtest and eval | Autonomous builder gates; judge; anim-critique; vision router | No recognizer accuracy harness; no bot "plays" VR | **P4 Gesture eval harness** (`src/lib/gesture-eval/`: run corpus replay through `MageArenaInput` built as a test binary or automation test; confusion matrix, false casts per minute on noise traces, compute latency; regression gate with thresholds from section 4), **M**. Acceptance: two runs give identical matrices; a planted bad template drops accuracy and fails the gate; the report carries n, corpus hash and build hash. Full-session playtests: scripted replay of an owner session (P5) reaching victory. | Headless; feel is the owner's |
| Performance on Quest | `src/lib/profiling/perf-capture.ts` noise-floor gate (pure); UE CSV events absent ("UNVERIFIED", `src/types/observation.ts:189-192`) | No on-device capture | **P6 Device perf capture** (desktop half in D1: UE `-csvprofile` on Win64 plus stat counts, gating the count budgets only; device half in V1: adb-driven OVR Metrics Tool CSV or UE `-csvprofile` on device, pull, feed the existing gate; `CSV_EVENT` markers per bout in the game), **M**. Acceptance: three captures of the stress scene give p50, p95 and hitch counts with a measured noise floor; a planted 2 ms sleep in the game thread fails the gate. | Headless with the headset on the desk (proximity sensor taped, or a dev setting) |
| Video capture for submission | ffmpeg re-encode, Veo, Blender renders; UE gives PNG sequences only; no Movie Render Queue | No headset capture; no trailer pipeline | **P10 Video lane** (pull the headset's built-in recordings by adb; an ffmpeg assembly script for title cards, cuts, captions and loudness; **optional** deterministic replay re-rendered from a spectator camera via LevelSequence, proven headless in fleet memory 2026-08-17), **S** for pull plus assembly, **M** for replay re-render. Acceptance: a 2:50 cut assembled from an EDL file; replay re-render reproduces a recorded duel at 1080p60. | Capture needs the owner; assembly is headless |
| Store and release channel | Nothing beyond version bump (`src/lib/packaging/version-manager.ts`) | Total | **P11 Release lane** (`ovr-platform-util` upload to channel "Competition" with app id and token from env; record build id; channel and invite checklist), **S**. First used in October (no device needed to upload). Acceptance: the build appears on the channel; the invite URL opens on a second account. | Headless, plus a one-time owner dashboard setup |

**Order of pof work** (sequenced ahead of need; headset-dependent halves in November):
1. **D1, days 1-3:** P0, P1, P2 (packaging half), P16.
2. **D1, days 3-7:** P17, P4 (thin, on the mouse and synthetic corpus), P6 (desktop half), P11 dry run if the app exists.
3. **D2:** P8, P7, P5 (desktop replay), then P18 (needed for the style pick on Mon 19 Oct).
4. **D3:** P15, P9, P13, P12, P14.
5. **D4:** P10 (desktop cuts), P11 (core APK). **V1:** P2, P3, P6 device halves. **V3:** P10 headset pull, P11 release.

**Roughly 33-44 agent-days** of pof work (P17 and P18 added) run in parallel with about 25-30 agent-days of game work. Both are supervised through the existing harness, which needs `maxConcurrent = 1` while checkpointing (autonomous-builder doc, "Safety rails").

**What stays human no matter what pof builds:**
- whether a perfect feels earned;
- comfort;
- the corpus drawing;
- the cross-user sample;
- art and audio taste;
- the video's camera work;
- the dashboard and app setup;
- the final submit.

pof makes everything else checkable without a headset: build, install, boot, recognizer accuracy on recorded data (on clips and mouse draws in October, on real hands from V2), kernel conformance, balance census, perf on pulled captures, and frame evidence from replays.

**Decisions this pillar implies:**
- (a) **Do not route spells through GAS.** pof's spellbook pipeline authors GAS abilities with their own numbers (`docs/features/spellbook/README.md`). Here the kernel owns every number, so GAS would create a second authority.
- (b) **Do not start from PoF's UE project** (eligibility, plus its Android-breaking runtime code).

---

## 9. Pillar 2: forging the learning into the knowledge registry

**Pending: where cross-channel Mage Arena knowledge lives.** The owner asked for a Mage Arena game-development subtree shared by the TV, VR and future PC channels, so a mechanic one channel masters is not re-solved by the others (`DECISIONS.md`). It is being settled with the owner; this plan does not decide it. One constraint holds: upper registry layers carry no product names (`ai-registry/docs/rkb-profile.md:21-28`). Until then, channel-neutral solutions are recorded in this repo's `docs/` with the channel that proved them; the drafts below are written either way and land where the owner decides.

### 9.1 Domain recommendation: extend `game-production`, do not found `game-development`

The owner asked for a "game-development" domain. My recommendation is to deliver it as an **extension of the existing `game-production` bundle**. The reasons are honest overlap, not convenience:

1. **The overlap is already there.** `game-production` has 54 forged subjects in 7 categories (`ai-registry/knowledge/game-production/index.json`, `taxonomy.json`) and was forged from pof itself (`ai-registry/librarian/projects.md:23`). The lesson areas the owner names already have homes:

   | Lesson area | Subjects that already cover it |
   |---|---|
   | RPG development | `arpg-systems-canon`, `game-economy-tuning`, `ability-authoring-to-engine` |
   | Combat | `realtime-combat-semantics`, `combat-pacing-and-dramatic-arc` |
   | Balance | `encounter-balance-simulation` |
   | Teaching | `learning-curve-and-teaching-design` |
   | Unreal | all 8 `engine-integration` subjects (the Unreal specifics live in applications) |
   | Autonomous LLM tooling | `unattended-build-loop`, `production-prompt-architecture`, `engine-pitfall-corpus`, `judgeable-spec-authoring` |
   | Lifecycle | `production-governance` |

2. **The registry's split rule says one bundle until a category earns a split** (`ai-registry/knowledge/README.md:70-72`). The bundle explains why it is one: every category shares one denylist (engine and tool names) and one evidence spine (`knowledge/game-production/index.md`).
   - A VR or Unreal domain would ban the same vocabulary the `game` purity profile already bans: `Unreal|UE5|Blueprints`… in `ai-registry/scripts/check-bundles.mjs:181-185`. It fails the split test.

3. **"Unreal" and "Quest" cannot be subject names anyway.** Upper layers must carry no product names (`ai-registry/docs/rkb-profile.md:21-28`). Unreal and Quest lessons land as **applications** with a C++ stack. The game bundle's `index.md` declares no `stacks:`, so add `stacks: [cpp]`; otherwise a C++ application fails as "unknown stack" (`check-bundles.mjs:84, 293-296`).

4. **What is genuinely new is immersive interaction.** A strict grep finds **zero** VR, XR, hand-tracking, comfort or headset coverage anywhere in the registry. That earns **one appended category** (slot 8 of 10; categories are append-only), not a domain.

**Required mechanics in the same change:**
- extend the `game` purity regex with headset, runtime and platform vendor names, so they cannot leak into upper layers;
- add `stacks: [cpp]`;
- register `mage-arena-vr` in `ai-registry/projects.json` and `librarian/projects.md`;
- follow `docs/harvest-brief.md`: classify each item as NEW or EXTENDS, name neighbours, write boundary paragraphs.

Laws are closed during a harvest. Proposed laws go in the report, not into `_laws.md`. Agent-run lessons that are not game-specific (model and effort choice, run budgeting) go to `agent-operations` (`model-and-effort-selection`, `agent-run-budgeting`, `deterministic-run-verification`).

### 9.2 Subject map

**Already covered, do not duplicate** (cite and extend instead): `realtime-combat-semantics`, `encounter-balance-simulation`, `combat-pacing-and-dramatic-arc`, `learning-curve-and-teaching-design`, `difficulty-design-and-adaptation`, `perf-regression-gating`, `ship-pipeline-gating`, `runtime-observation-evidence`, `engine-integration-safety`, `engine-pitfall-corpus`, `unattended-build-loop`, `playtest-signal-to-defect`, `spatial-audio-scene-authoring`, `asset-class-poly-budgeting`, `shader-budget-authoring`, `generative-artifact-gating`, `design-canon-as-executable-law`, `production-coverage-measurement`. Plus `software-engineering`'s `test-harness`, `codegen`, `packaging` and `release-pipeline`, and `media-generation`'s `sound-effect-generation` and `generated-music-acceptance`.

**New subjects** (slugs are proposals; status starts `draft`):

| # | Slug | Category | One-line scope | Main neighbours (boundary) |
|---|---|---|---|---|
| N1 | `drawn-gesture-command-recognition` | **immersive-interaction** (new) | Drawn and posed hand gestures as commands: pen-down segmentation, plane projection, template recognizers with a reject class, vocabulary distinctiveness, held-out and cross-user acceptance, accuracy and latency go/no-go | `learning-curve-and-teaching-design` (teaching the vocabulary) |
| N2 | `hand-tracked-timing-windows` | immersive-interaction | Reward windows (perfect blocks) under tracking latency and confidence: onset timestamps, photon-to-detection measurement, windows in data, tracking loss as a state | `realtime-combat-semantics` owns windows in general; N2 owns the sensor |
| N3 | `seated-immersive-arena-design` | immersive-interaction | The seated play envelope: yaw comfort band, threat placement, teleport or blink in place of locomotion, pause and resume on focus loss, short complete sessions | `combat-pacing-and-dramatic-arc` |
| N4 | `field-of-view-aware-signalling` | immersive-interaction | Threat readability across wide and narrow FOV devices: peripheral cues, spatial audio as a threat channel, diegetic body-anchored HUD | `spatial-audio-scene-authoring` |
| N5 | `standalone-headset-frame-budgets` | immersive-interaction | Authoring budgets for mobile-GPU XR: forward shading, baked light, overdraw, particles, multiview draw calls, thermal headroom, per content class | `perf-regression-gating` (gating), `shader-budget-authoring` |
| N6 | `tracked-input-record-and-replay` | immersive-interaction | Recording real tracked input as versioned corpora and replaying it headlessly; synthetic clips as pre-device proxies and what they missed; the limits of emulators; device-in-the-loop evidence tiers | `runtime-observation-evidence`, `test-harness` |
| N7 | `cross-runtime-rules-conformance` | engine-integration (9th) | One rules authority, two runtimes (reference sim and shipped engine): data pins, golden traces, tolerance, divergence logging | `design-canon-as-executable-law` (may fold into it at review) |
| N8 | `deadline-slice-scoping` | production-governance (7th) | Shipping a vertical slice against a fixed external date: numeric feasibility gates, named fallbacks, kill and pivot, cut logs, submission-first ordering | `production-work-prioritization` |
| N9 | `autonomy-coverage-ledger` | craft-judgment (7th) | Measuring, per lifecycle phase, what an agent toolchain verifies without a human versus what needs a person (in a headset); the honest level of autonomous LLM game development | `production-coverage-measurement`, `unattended-build-loop` (may fold at review) |

**EXTENDS** (new techniques proposed for existing subjects):
- **`realtime-combat-semantics`:** "a window is measured from the sensor's motion onset, not the classifier's".
- **`encounter-balance-simulation`:** "the reference controller samples measured input-modality latency".
- **`learning-curve-and-teaching-design`:** "the first minute teaches each verb in-world with no fail state".
- **`ship-pipeline-gating`:** "boot marker on a sideloaded device package".
- **`perf-regression-gating`:** "pulled on-device captures as the only frame-time truth".
- **`engine-pitfall-corpus`:** new incident entries:
  - a plugin binary built for a different engine minor version;
  - editor-only APIs in runtime modules breaking non-editor targets (the PoF incident above);
  - the emulator that cannot express the gesture under test.
- **`unattended-build-loop`:** "the human-in-headset gate is declared unverifiable to the loop, never inferred".

**RPG lessons are honestly thin in this slice** because the camp and progression are cut. The PC sibling is the RPG evidence source (Director, season, economy, deaths), harvested on its own cadence.

### 9.3 Forging cadence and evidence rules

The registry's process is: Phase 1, an expert draft hardened with 2-4 web searches before reading the repo; then Phase 2, reconcile against the repo, where each claim becomes confirmed (cited in an application), a deviation, or an upward lesson (`ai-registry/docs/forge-brief.md:12-23`, `docs/rkb-profile.md:254-266`). Status moves `draft → forged → reconciled` (`check-bundles.mjs:83`; there is no "hardened" status).

| When | Output | Evidence gate before it may land |
|---|---|---|
| W0 (3-4 Oct) | Subject proposal for `immersive-interaction` (sections as in `docs/subject-proposal-module-design.md`: gap measured, proposed techniques, boundaries, open questions) | The zero-coverage grep, recorded with its pattern |
| D1 end (11 Oct) | N1, N2 Phase-1 drafts; two pitfall entries | D-G0 to D-G4 numbers with n, date, build hash and command, labelled mouse or synthetic, never hands. **A failed gate is evidence too.** |
| D2 end (18 Oct) | N7 draft | ≥20 green conformance vectors plus one planted-mutation failure |
| D4 end (31 Oct) | N6 draft | Regressions caught by replay versus by a person |
| V1 end (4 Nov) | N5 Phase-1 draft | Device captures with n, date, device, build hash and command |
| V2 end (8 Nov) | N1, N2 evidence; N6 extended | Real-hand numbers beside October's for the same gates |
| V3 end (13 Nov) | N3, N4 drafts; N8 draft | ≥2 owner headset sessions labelled **felt (owner, n sessions)** plus ≥1 outside tester, observed before interpreted; the dated cut log |
| 19-30 Nov | Phase-2 reconcile of N1-N8 (status `forged`), EXTENDS harvest, N9 drafted from the measured autonomy ledger | Applications under `stack: cpp` (and `node` for TS-oracle evidence) with `verified_on` / `verified_against`, anchors as `path:line "quote"`; `node scripts/gate.mjs --lane knowledge` green |
| After 11 Dec | An outcome note (the competition result is context, **never** quality evidence) | — |

**Evidence rules for this project:**
1. A lesson needs a measurement (n, unit, date, device, build hash, command) or a reproducible failure with its repro. A "vibe" is never evidence.
2. Owner feel is admissible only as **felt**, with a session count. Agent self-reports are T0 and never certify anything ("no gate self-certifies", law in `_laws.md`).
3. Numbers live in applications. Upper layers state rules without product names: no Quest, Meta, Unreal or Mage Arena.
4. Evidence pointers stay in local `.evidence.local.md` files (registry privacy rule, `ai-registry/CONTRIBUTING.md:82-89`).
5. This plan does not edit the registry. The owner, or a `/forge`/`/deepen` session in the registry, performs each step, at the location the owner settles.

**The autonomy-coverage ledger (feeds N9)** is kept from day 1 in the game repo, one row per lifecycle phase:
- agent-hours;
- owner-hours (at the desk, in the headset);
- which gates ran headless;
- which needed a human;
- which pof feature moved a phase from human to headless.

That table is the only honest answer to "what is the level of tooling for autonomous LLM development".

---

## 10. Risk register

| # | Risk | Likelihood | Impact | Mitigation |
|---|---|---|---|---|
| R1 | **Unreal hands toolchain:** the v207 plugin/5.8 mismatch (open, no workaround); few hands-first Quest hits are built in Unreal, so there is little community help. With WebXR rejected there is no off-ramp. | High | High | D-G0 with three fallbacks, route chosen by Fri 9 Oct; weekly APK and Quest 3 profile boot all October; device boot unverified until V1 |
| R2 | **Recognizer accuracy for strangers:** judges are not the owner; air-drawing varies | Medium | High | Only 3 classes plus reject; a cross-user gate at ≥85% (on mouse in D1, on hands in V2); teach the circle first; templates from 2 people; R1-R3 fallbacks |
| R3 | **Perfect window unreachable through tracking latency or jitter** | Medium | High | Onset timestamps; injected-latency clips in D-G3; the 0.20 s data step; the flash method in V2; the design kill is explicit |
| R4 | **Deadline in two halves:** all VR work in about 18 days; a hands failure surfaces on 8 Nov, not 11 Oct | High | Very high | The six October mitigations (section 1); V-gates 4 and 8 Nov; upload freeze 16 Nov; cut order: Mirror IV, Lash IV, bout 2, narrow-FOV mode |
| R5 | **First game for pof:** pof's assumptions (ARPG, GAS, Win64, one project) do not fit, and pof work crowds out game work | High | Medium | pof items are thin and test-first; P0/P1/P2 before anything else; GAS bypassed; an item may ship "thin" if its acceptance test passes |
| R6 | **Quest performance with Niagara and translucency**, unmeasurable until V1 | Medium | High | Count budgets (D-G4) and P15 from week 1; CPU-sim particles; the stress scene packaged so V1 measures it on day one; P-1 to P-3 |
| R7 | **Kernel divergence** between the TS oracle and the C++ port, or drift in shared data (the PC stream is actively editing) | Medium | Medium | Commit pins only; P7 vectors; P8 drift alarm; change requests through the PC orchestrator; the `vr-sim/` adapter as a fallback |
| R8 | **Eligibility:** the Start program application is not made yet; "no pre-existing codebases"; reuse of the PC kernel and art; licensed assets | Medium | Very high (disqualification) | Owner applies in week 1 (Q1); a new repo and project; PC material dated after 24 Sep; no Fab or stock packs; ask on the Devpost forum before 12 Oct (Q9) |
| R9 | **Owner time:** about 19 headset hours inside 1-16 Nov (13 desk hours in October) | High | High | Checklists written in D4; everything else headless; a second tester recruited in October |
| R10 | **Genre fit:** action is outside the listed Gaming genres; Stage 1 viability | Low-Medium | Medium | Pitch strategy and reading; lead the video with deliberate perfects and composition |
| R11 | **Comfort complaints** (blink, threats at the edge of view) | Low | Medium | Snap plus vignette, ±70° default arc, nothing from behind, Gentle mode |
| R12 | **Store or dashboard setup delays** (membership, app creation, channel) | Medium | High | Owner applies and creates the app in October; P11 uploads an October APK, no device needed |
| R13 | **Paid quotas run out** (Tripo credits are already exhausted per fleet memory; ElevenLabs; MiVRy licence) | Medium | Low-Medium | Low-poly authored arena; a small asset list; budget guard per provider |
| R14 | **Device arrives late:** ordered after the 31 Oct gate; delivery days come out of V1 | Medium | High | Model and seller chosen in October, order on 31 Oct (Q2); all device-free V1 work done in D4 |
| R15 | **Desktop feel does not transfer:** keys are not hand timing, mouse paths are not air paths | High | Medium-High | October numbers labelled mouse or synthetic; V2 re-runs the same gates on real hands, re-records clips, re-runs the census |
| R16 | **Art, animation and audio in 13 days** at high quality | Medium | Medium-High | One simple style; a small asset list; P15 from the first import; content frozen 25 Oct; a slip shrinks the sprint, never moves it |

---

## 11. Submission plan

**Video beats** (under 3 min; the best material first; on-device capture plus one real-world camera of the owner seated):

| Time | Beat |
|---|---|
| 0:00-0:08 | A fireball comes at the camera. A palm rises. White ring, bell. The fireball flies back (Mirror Reflection). Title. |
| 0:08-0:25 | Close-up of the hands: the circle, then the stroke, then Tide Orb. Caption: "Draw spells. No controllers." |
| 0:25-0:35 | Real-world shot: the owner seated in a chair, hands only. Caption: "Seated. Ten-minute matches." |
| 0:35-1:25 | The duel against the Ember mage: wrist runes igniting (tier clock), Flow beads to a Crest, a black-core Sunfall, a flick blink to safety. |
| 1:25-1:45 | First-minute teach montage (palm, sigil, blink). |
| 1:45-2:15 | The finisher: a two-hand seal, Leviathan Orb, slow motion, crowd. |
| 2:15-2:40 | Narrow-FOV mode, left-hand mode, pause and resume by raising both palms. |
| 2:40-2:55 | Future plans card (four schools, the camp, ghost duels), the launch date, the title. |

Fallback if device capture fails: the rules accept video "via XR Simulator" (weaker: no real hands).

**Description outline** (for 500 words; confirm the limit):
1. Inspiration: a Roman-era arena of elemental mages, and the wish to cast spells by hand rather than by button.
2. What it is: three verbs (draw, ward, blink), a tier clock and Flow, ten minutes.
3. How it was built: Unreal, Meta hands, a desktop-first month in which every gesture was a replayable hand clip, a deterministic combat kernel shared with a PC sibling, and an agent-driven toolchain (pof) that verifies builds, recognizer accuracy, balance and performance headlessly.
4. Future plans: the four schools, the camp, ghost duels, VR glasses tuning.
5. Target launch date (Q5).

**Hand-interaction write-up angle:** "Each verb is mapped to the most reliable hand signal".
- **Pinch for Bolt.**
- **Shape for lines:** a template recognizer with a reject class. Quote the measured accuracy, n and the cross-user figure.
- **Palm pose plus orientation for the ward:** the perfect window is measured from motion onset; quote the measured latency.
- **Flick for blink.**
- **A two-hand pose for the seal.**
- **Designed against tracking failure:** hands never overlap in the layout, tracking loss pauses the game, and nothing asks for fine finger reads at speed.
- Close with the FoV-aware design and pause/resume.

**Launch date:** a store release in spring 2027, aligned with VR glasses availability, is my proposed default. The owner decides (Q5).

---

## 12. Open questions only the owner can answer

Answered 2026-10-02 and removed: the WebXR pivot (rejected), the repo location, the headset choice (deferred to the desktop core gate).

1. **Start program:** you will apply; when, and when will the dashboard app and "Competition" channel exist? (R8, R12.)
2. **Device order timing:** may it be ordered on Sat 31 Oct from a seller chosen in October (delivery ≤3 days), and stay on the desk by USB for headless runs? (R14.)
3. **Hours you can commit:** the plan assumes about 13 at the desk in October and 19 in the headset in 1-16 Nov.
4. **Kernel authority:** may the mage-arena orchestrator accept a blink and pad extension to the arena kernel by Sun 11 Oct, or should VR wrap the core with an adapter?
5. **Target launch date** for the form (default: spring 2027).
6. **A second person** for the sigil samples (D1 mouse, V2 hands), plus 2 cold desktop testers (D4) and 3 cold device playtesters (V3).
7. **Paid tools** if needed: a MiVRy licence (fallback R2), a music provider, more 3D or image credits.
8. **Asset policy:** generated and authored only, or are licensed packs acceptable after checking the rules?
9. **Eligibility check:** are you comfortable reusing the PC kernel, data and art created 1-2 Oct, after the 24 Sep window opened? Should we ask on the Devpost forum?
10. **Registry location (pending):** where cross-channel Mage Arena knowledge (TV / VR / PC) lives (section 9).

---

### Sources consulted (read-only)

- **Design:**
  - `firetv/.contest/arena/mage-arena/entries/claude-claude-opus-5-5_xhigh/variant-2/` (`NOTES.md`, `design/README.md`, `spell-sheet-water.md`, `data/combat.json`, `schools.json`, `enemies.json`, `arena-tiers.json`, `spells-water.csv`, `waves/plan.md`, `waves/W2-arena-kernel.md`);
  - `firetv/.contest/arena/mage-arena/data/OWNER-NOTES.md`.
- **PC sibling:**
  - `mage-arena/docs/MAGE-ARENA-PLAN.md`;
  - `mage-arena-arena/docs/waves/W4-tiro-games.md`, `docs/OWNER-CHECKS.md`, `packages/core/scripts/compile-arena-data.mjs`, `packages/core/src/arena/types.ts`;
  - `mage-arena-art/art/OWNER-CHOICE.md`, `STYLE.md`, `scale-contract-v1.json`;
  - `mage-arena/docs/design/reference-the-ledger/design/data/spells-fire.csv`.
- **pof:**
  - `CLAUDE.md`, `.claude/fleet-memory.md`;
  - `docs/features/harness-llm-unreal/*`, `docs/features/spellbook/README.md`;
  - `docs/ue58-mcp-*.md`, `docs/dual-execution-program.md`, `docs/visual-generation-roadmap.md`;
  - `src/lib/packaging/uat-command-generator.ts`, `src/lib/audio-gen/providers/elevenlabs.ts`;
  - `tools/pof-mcp/src/tools/*.ts`;
  - `build-5_8-game.log`;
  - the PoF UE project's `PoF.uproject`, `Config/`, `BuildPackage.bat`.
- **Registry:**
  - `README.md`, `CONTRIBUTING.md`, `knowledge/README.md`, `knowledge/game-production/{index.md,index.json,taxonomy.json,_laws.md}`;
  - `docs/rkb-profile.md`, `docs/forge-brief.md`, `docs/harvest-brief.md`;
  - `scripts/check-bundles.mjs`, `projects.json`, `librarian/projects.md`.
- **External (fetched 2026-10-02):**
  - Devpost rules (`start-developer-competition-26.devpost.com/rules`);
  - Meta competition blog;
  - Meta feedback investigation 1053459897316880;
  - the Meta XR Interaction SDK Unreal download page (v207, 22 Sep 2026);
  - the Meta XR Simulator hand-tracking docs;
  - the MiVRy site (licence terms).
