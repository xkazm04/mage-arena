# Mage Arena VR: project plan for the Meta VR Start Developer Competition 2026

Written 2026-10-02 by the lead-analyst session (Claude Opus 5.5). This is a plan, not a status report: nothing below has been built, and every "measured" number is a target until a command produces it.
Competition: Gaming track, New Experience division. Submission deadline **Wed 18 Nov 2026**; winners about 11 Dec 2026.
Sibling project: the PC Mage Arena (`C:\Users\kazda\kiro\mage-arena`, plan `docs/MAGE-ARENA-PLAN.md`). This VR slice is a sibling, not a replacement. Section 3 covers what the two share.

Labels used throughout: **authored** (a number someone chose), **simulated** (produced by a headless sim), **measured** (produced by a command on named hardware), **felt** (only the owner can say it). A claim with no command behind it is **unverifiable**.

---

## 1. Executive summary

**The bet.** There is no hands-only, seated spell duel on Quest where you draw sigils (prior-art scan: War of Wizards is drawn with controllers, Drakheir uses hands-only PvE poses, Waltz of the Wizard is a sandbox, and Lost Sigil and Hand Wizardo are prototypes). Mage Arena VR fills that gap with three verbs, each mapped to the most reliable hand-tracking signal:
- **draw** a sigil with the casting hand (a circle plus an inner stroke picks a spell line);
- **raise** the off-hand palm as a 140° directional ward with a 0.15 s perfect window;
- **flick** to blink between three rune pads.

Under those verbs sits the B/2 "Fourteen Nights" combat model: the tier clock, Flow, and the threat colour language. A deterministic kernel already exists and is calibrated in the PC sibling's TypeScript `core` (W2-W4 committed; see section 3).

The second bet is about tooling. This is **pof's first end-to-end game**. Every gap pof has (and it has no VR, Android-device, store or on-device profiling support today) becomes a pof work item, sequenced just ahead of the game work that needs it. Every measured lesson is forged into the registry's `game-production` bundle.

**Owner decision, 2026-10-02 - art comes last.** The game is built **style-less first**: a greybox / wireframe
baseline chosen only for convenience, until every mechanic works together on device (sigil casting, palm ward, blink,
tier clock, Flow, threat readability, the 10-minute arc). Only then is effort spent on art, and technical limits are
expected to favour simple styles over realism and high fidelity. Shortlist kept in mind throughout:
**13 Screen-Print Poster**, **25 Moonlit Silver Nocturne**, **30 Chalk & Slate** (`ART-STYLE-STORYBOARD.md`).
Consequences: no art work before the mechanics gate; the greybox must still honour the threat colour language
(element colour = absorb, steel = dodge, black core = leave), because all three shortlisted styles rely on colour
only for magic; the art pass in W5 picks ONE of the three at an **art gate at the end of W4**, decided on a
device capture of the greybox duel re-skinned in each candidate (unlit shaders only).

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

**Kill and pivot criteria** (detail in sections 4 and 9):

| When | Trigger | Action |
|---|---|---|
| Wed 7 Oct | No hand-tracked APK running on the owner's headset through any of the three toolchain routes (section 4, G0/G1) | Pivot P-B: ship as WebXR on the existing TS kernel. The rules accept hosted WebXR links. This gives up the Unreal half of Pillar 1. The owner must say yes. |
| Sun 11 Oct | Sigil recognition below 85% top-1 on the owner's held-out draws after fallback R1 | Replace drawn sigils with held hand-signs (pose selection). This keeps hands-only play but loses the differentiator. If poses are also below 90%: stop. |
| Sun 11 Oct | Owner lands fewer than 1 perfect in 10 at a 0.20 s window | The ward becomes hold-to-absorb with no perfect window, and the tier clock runs on time only. This is a large design loss; it needs the owner's call. |
| Sun 25 Oct | Wave 1 not playable on device at a p95 frame time of 13.9 ms or better (72 Hz) | Cut to two bouts (soldiers, then the duel). |
| Sun 8 Nov | Beta not on the "Competition" release channel | Freeze features; only polish and fixes after that. |

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
- **Originality:** the project must be original to the entrant, public domain, or Meta-provided. Treat Fab or Marketplace packs and stock music as a risk until confirmed. Prefer generated or authored assets (section 10, Q8).

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
   - If it is not merged by Sun 18 Oct, the VR repo carries a thin adapter package, `vr-sim/`. It imports the core at a pinned commit and wraps it. The divergence is logged in the VR plan. This keeps the PC project's orchestration (Sonnet, Astra, Fable) undisturbed.
4. **pof's place.** pof drives the VR project: harness, MCP, gates, assets. It does not orchestrate the PC project.

The two projects meet at four seams only:
- the data pin;
- change requests;
- read-only reuse of art;
- one shared registry harvest. PC supplies the RPG, camp and Director evidence; VR supplies the immersive and Unreal evidence.

---

## 4. Feasibility week (Mon 5 Oct - Sun 11 Oct): a hard gate

**Machine facts, read today:**
- UE **5.8.2** is installed (`C:\Program Files\Epic Games\UE_5.8`, launcher manifest `5.8.2-56702186`). There is no 5.7 install.
- The engine ships stock `OpenXR`, `OpenXRHandTracking` and `XRBase` plugins and `Engine/Platforms/Android`.
- **Meta XR plugins are not installed.**
- Android command-line tools are present:
  - `ANDROID_HOME=C:\Users\kazda\scoop\apps\android-clt\current`;
  - NDK `28.2.13676358`;
  - build-tools 34-36.1;
  - platforms 31, 34 and 36;
  - `adb` on PATH.
- `NDKROOT` is **unset**, and the JDK on PATH is **Zulu 22**. Whether UE 5.8 accepts NDK r28 and JDK 22 is unverified; that is the first thing G0 checks.
- **Known open Meta bug (investigation 1053459897316880, status "Investigating"):** the v207 OculusXR, OculusInteraction, MetaXRHaptics and OculusPlatform plugins claim 5.8 support but are built against 5.7.0, and modules fail to load. The ISDK v207 download page lists both 5.8 and 5.7 as supported. Packaged 5.8 samples may need `r.Mobile.ShadingPath=0`.

**Prerequisite (owner, day 0):** Start-program membership, developer mode on the headset, and an app created on the developer dashboard (Q1, Q2).

| Gate | What is tested | Go (numeric) | Measured by | Fallbacks, in order |
|---|---|---|---|---|
| **G0 Toolchain** (Mon-Tue) | A new `MageArenaVR` project on 5.8.2 with Meta XR v207 (Meta XR + ISDK) loads in the editor and packages an arm64 Vulkan APK | Editor loads with no module-load errors. `BuildCookRun -platform=Android` exits 0 and produces an APK. | pof P2 lane (headless), UBT/UAT logs judged by markers, not exit code alone | **F0a:** put the plugin sources in the project `Plugins/` folder and let UBT compile them against 5.8 (plausible, unverified; Meta notes manual-build docs are "unclear or missing"). **F0b:** install UE 5.7.x (supported by ISDK v207; costs a few hours of download plus disk). **F0c:** stock `OpenXR` + `OpenXRHandTracking` only (already installed). This loses ISDK pose detection and gaze, so pose detection gets written by hand (about 1 day). |
| **G1 Hands on device** (Tue-Wed) | Packaged APK on the owner's Quest; joint stream from both hands | Joints at 60 Hz or more for 5 min with ≥95% of frames at high confidence while hands are in the drawing zone. Cold start to an interactive scene in 15 s or less. | On-device logging pulled by adb (P3); stopwatch for cold start | Pivot P-B if all of F0a-F0c fail by Wed 7 Oct |
| **G2 Sigil recognition** (Wed-Fri) | A $Q multistroke recognizer ported to C++, over fingertip paths projected onto a best-fit plane. Classes: 3 slotted inner strokes plus a reject class. Two drawing styles: **A** two strokes with pinch as pen-down; **B** one continuous stroke. | **≥95% top-1** on the owner's **held-out** set (3 classes × 30 draws = 90, templates drawn separately); **≥85%** for one other person (3 × 10 = 30); **≤1 false cast per minute** in 3 min of non-sigil play (ward raises, blinks, waving); **≤30 ms p95** recognizer compute on device; **≤900 ms p50** from pen-down to cast event | P3 corpus + P4 eval harness (headless replay, deterministic confusion matrix) | **R1:** cut the inner strokes to the three most distinct shapes and tune the reject threshold on the corpus. **R2:** a MiVRy evaluation. Its free tier is capped at 100 recognitions per session, so shipping needs a paid per-project licence (Q7). **R3:** held hand-signs via pose detection, which kills the differentiator (see section 1). |
| **G3 Ward and perfect window** (Thu-Sat) | Off-hand palm raise (pose + palm normal + height), timestamped by **motion onset** (joint velocity), not by classifier output. A slow magic dummy at the front, a stream thrower (3 bolts 0.2 s apart). | Raise-detection latency **p95 ≤ 60 ms** (flash method from the W2 card: the screen flashes on detection, filmed at 240 fps, 30 or more raises). Owner lands **≥3 perfects in 10** after 5 minutes. **0** perfects from holding the ward up permanently. **≤1** false raise per minute while casting. | Owner in headset plus phone slow-motion video; scenario replay headless (P5 skeleton) | **W1:** window 0.20 s, from data (the W2 card's documented step, "do not widen by feel"). **W2:** onset-compensation tuning. **W3:** design kill as in section 1. |
| **G4 Quest frame budget** (Fri-Sun) | A stress scene: greybox arena, 20 enemy proxies, 100 projectiles, 6 simultaneous Niagara systems, both hands rendered, mobile forward + Vulkan, baked lighting | **≥72 Hz** with **p95 frame time ≤13.9 ms** and **≤1 hitch (>27.8 ms) per minute** over three 60 s captures; GPU utilisation not pinned at maximum for the run | P6 device perf capture (OVR Metrics or the UE CSV profiler pulled by adb), the existing noise-floor gate in `pof/src/lib/profiling/perf-capture.ts` | **P-1:** fixed foveation high, CPU-sim Niagara only, flipbook sprites instead of GPU emitters. **P-2:** cap at 72 Hz, halve particle budgets. **P-3:** lower render scale. |
| **G5 Build loop** (all week) | Edit → APK → install → launch | Incremental loop **≤15 min**; boot marker in logcat within 60 s of launch | P2 | Accept a slower loop, but batch device sessions |

**Go/no-go meeting Sun 11 Oct** (owner, 30 min): read the G0-G5 table filled with measured numbers and pick the drawing style (A or B).

**Owner time in week 1:** about 5 hours in the headset (corpus recording 1.5 h, ward tests 1 h, perf and smoke 1 h, slack 1.5 h) plus one other person for 20 minutes.

---

## 5. Week-by-week schedule to 18 November

**Rhythm** (mirrors the PC plan's wave rule): a design note first, data first, tests with content assertions, a green gate, an owner check for anything only the owner can judge, one commit per item.
- The game repo is a new git repo, `C:\Users\kazda\kiro\mage-arena-vr` (Q4), with the UE project inside it. pof attaches to it as a second UE project (P0).

| Week (dates) | Milestone and game deliverable | pof work it depends on (section 7) | Registry output (section 8) | Verification: headless / owner in headset |
|---|---|---|---|---|
| **W0** Fri 2 - Sun 4 Oct | **M0 Ready:** empty `MageArenaVR` project (5.8.2), Meta XR v207 downloaded, data pin to the arena commit `4b21c57`, VR overlay drafted | **P0** second-project support; **P1** editor-only bridge hygiene; **P2** start | Written subject proposal for a new `immersive-interaction` category (format of `ai-registry/docs/subject-proposal-*.md`) | Headless: project opens, data hash check. Owner: none. |
| **W1** 5 - 11 Oct | **M1 Feasibility verdict** (section 4): recognizer, ward, stress scene, build loop | **P2** Quest APK lane, **P3** hand-trace recorder and corpus, **P4** gesture eval, **P6** device perf capture (thin versions) | Phase-1 expert drafts: `drawn-gesture-command-recognition`, `hand-tracked-timing-windows`, `standalone-headset-frame-budgets`, each hardened with 2-4 web searches. First engine-pitfall entries (the plugin version mismatch; editor-only APIs in a runtime module). | Headless: P4 confusion matrix; perf gate on pulled captures. **Owner: about 5 h.** |
| **W2** 12 - 18 Oct | **M2 Duel core on device:** C++ kernel port (absorb, tier clock, Flow, Bolt, projectiles, telegraphs, dummies, bots), gesture pipeline producing the kernel `InputFrame`, the three pads and blink, wrist rune HUD, greybox arena. A dummy duel is playable. | **P7** conformance gate, **P8** data sync, **P5** XR scenario driver (record-replay hands headless) | Draft `cross-runtime-rules-conformance` once the first 20 vectors are green | Headless: kernel unit tests (port of the TS list), 20 conformance vectors, a replayed-hands scenario reaching "perfect" at the expected tick. **Owner: 1.5 h** (ward feel, blink comfort). |
| **W3** 19 - 25 Oct | **M3 Water and Wave 1:** Tide Orb, Lash, Mirror (tiers I-IV, branches), the tier IV two-hand mudra seal, Flow and Crest, soldiers wave (conscripts, slingers), missio and retry. **VR census:** the TS oracle with the VR reference controller (measured sigil time, blink, no walking) re-tunes bands; changes go into overlay data only. | **P9** Niagara parametrize tool, **P12** VR canon for judge rubrics; uses P4/P5/P6 continuously | Drafts: `seated-immersive-arena-design`, `field-of-view-aware-signalling` (after two owner sessions plus one outside tester) | Headless: census report (medians in band), recognizer regression gate, perf gate on the Wave 1 scene. **Owner: 2 h.** |
| **W4** 26 Oct - 1 Nov | **M4 Content complete:** creatures wave (cinder hounds with death bursts, mire maw), the Fire mage AI (competence 1 and 1.5, Heat, Sunfall unblockable), onboarding (first 60-80 s), pause and resume, session flow, left-hand mode, narrow-FOV mode. **Systems freeze Sun 1 Nov.** | **P13** audio and music lane (SFX exists; music provider is new); **P14** catalog read-only mirror of shared data (for art and VFX briefs) | Draft `tracked-input-record-and-replay` with numbers (regressions caught headless versus on device) | Headless: full-session scripted replay (all three bouts) reaches the victory state; census of the Fire duel. **Owner: 2.5 h.** |
| **W5** 2 - 8 Nov | **M5 Beta on the "Competition" channel:** art pass in ONE shortlisted style (13 Screen-Print, 25 Moonlit Silver or 30 Chalk & Slate; chosen at the W4 art gate), applied to arena, opponents and spells, VFX polish, spatial audio, comfort pass, perf to budget, 3 outside playtests (seated, hands-only, cold) | **P11** store upload lane, **P10** video lane (pull plus ffmpeg) | Draft `deadline-slice-scoping` (the measured cut log so far) | Headless: perf gate (p95 ≤13.9 ms), package size, boot smoke on device via adb. **Owner: 3 h** plus hosting 3 playtests (about 1.5 h). |
| **W6** 9 - 15 Nov | **M6 Release candidate Fri 13 Nov:** fixes only; video captured and cut Sat-Sun 14-15 Nov; form copy written | P10 replay re-render (optional) | Evidence collection for the post-submission reconcile | Headless: RC regression (all gates). **Owner: 4 h** (video capture, final play-through, form review). |
| **Submit** Mon 16 Nov (target), buffer until Wed 18 Nov | Channel build frozen, invite URL tested from a second account, video public, form submitted | — | — | Owner: 1 h |
| **After** 19 Nov - 11 Dec | Freeze (no changes allowed) | pof retrospective | Phase-2 reconcile of every drafted subject (draft → forged), EXTENDS harvest, autonomy-coverage ledger (section 8) | — |

**Total owner time in the headset:** about 20-22 hours over 6.5 weeks. That is the binding constraint (risk R9).

---

## 6. Game spec for the competition slice

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
| **Ward** | Off-hand open palm raised in front, palm normal forward | 140° arc centred on the palm normal (projected on the ground plane). Raise costs 5 mana and drains while held; perfect = a fresh raise ≤0.15 s before impact, magic only (`combat.json` `absorb`). Perfect gives mana back, advances the clock 2 s and adds 1 Flow. | Big, deliberate, occlusion-safe, and it reads like Doctor Strange. Timing is measured from motion onset (G3). |
| **Blink** | Casting hand flicks toward a pad (pointing pose plus wrist angular velocity over a threshold, direction resolves the pad) | Replaces roll: 0.25 s invulnerability and a stamina cost (overlay data, starting from roll's 25). It is the answer to steel and black-core threats. | Directional, one motion. Fallback: head-gaze at a pad plus a pinch (ISDK gaze + pinch, if G0 keeps ISDK). |
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

**Project:** `MageArenaVR` on UE 5.8.2 (or 5.7 if F0b), mobile forward shading, Vulkan, multiview, MSAA 4x, baked lighting, no Lumen or Nanite. If Meta XR v207 loads: OculusXR plus ISDK for hands, poses and gaze. Otherwise stock OpenXR plus OpenXRHandTracking.

| Module | Type | Responsibility | Key rule |
|---|---|---|---|
| `MageArenaSim` | Runtime, pure C++ (no UObject in the step) | Fixed 60 Hz kernel ported from TS `packages/core/src/arena` (VR subset): actors, absorb state machine, tier clock, Flow, projectiles with swept hits, telegraphs, zones, hit dedup per activation, Down as a state tag, enemy brains, mage AI, Tiro waves, seeded RNG with a draw log, event log. | Double precision to match JS numbers; no wall clock, no unseeded random, no per-step allocation. Same tick order as TS. |
| `MageArenaData` | Runtime | Loads the pinned shared JSON/CSV plus `combat.vr.json` from staged non-asset files; logs the data hash at boot. | Data wins over code; a hash mismatch against the pin manifest fails the dev build (P8). |
| `MageArenaInput` | Runtime | `IHandSource` (live ISDK/OpenXR \| replay file \| synthetic); feature extraction (pinch strength, palm normal, joint velocities, confidence); `SigilRecognizer` ($Q port, plane projection, reject threshold); `WardDetector` (onset timestamp); `BlinkDetector`; `MudraDetector`. Emits a VR `InputFrame` per sim tick. | Hand frames carry sensor time and are quantized to ticks. Tracking loss is a state, not "no input". |
| `MageArenaGame` | Runtime | Seated XR pawn, dais and pads, presentation (Niagara pools, spatial audio, wrist HUD), session flow (teach → bouts → victory), pause/resume, settings (hand, FOV mode, Gentle). | Presentation only **reads** kernel state and events; it never writes a number. |
| `MageArenaTests` | Development only | Kernel unit tests (mirror of the 47 TS tests where in scope), conformance vectors, corpus replay tests, scenario tests. | Judged by abslog markers (pof convention). |
| `MageArenaDev` | Editor only | pof bridge hooks, recorder controls. | Never packaged (eligibility and size). |

**Gesture pipeline:** joints (72 Hz) → smoothing (One-Euro filter) → pen state (pinch, or point pose for style B) → stroke buffer → resample to 32 points per stroke → PCA plane fit, projected to 2D → $Q match against the templates for the 3 slotted lines → reject if the score is below threshold or the circle is missing → `CastLine(line)` event.
- The tier IV path waits up to 1.5 s for `MudraDetector`.
- Templates are the owner's recorded draws (5 per class), stored as data so they can be tuned without code.

**Determinism and headless balance:**
- The TS oracle runs the census with a VR reference controller whose sigil time, misrecognition rate and blink timing are sampled from the **measured** corpus distributions (G2). This makes the balance numbers honest for hands, not for a mouse.
- The C++ runtime is held to the oracle by P7.
- Headless UE runs (`-game -RenderOffScreen` with XR disabled and the replay hand source) give T3 behavioural evidence and T4 frames without a headset (P5).

**Performance budget for Quest 3 / 3S at 72 Hz** (authored targets; G4 confirms or revises them):

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

Thermal behaviour is recorded in every perf capture (CPU and GPU levels, if the capture exposes them; unverified until G4).

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
| VR and hand-tracking integration | **Nothing** (0 hits for OpenXR, MetaXR, HandTracking, XR Simulator) | Total | **P3 Hand-trace recorder and corpus** (game-side `UHandTraceRecorder` writes JSONL joint frames with sensor time and confidence; pof `src/lib/device/corpus.ts` pulls by adb, labels by prompt ("draw Lash now"), versions by hash; MCP `pof_corpus_pull`/`pof_corpus_list`), **M**. Acceptance: 180 labelled traces pulled and schema-valid; a re-pull is byte-identical; the corpus manifest records device, build and date. **P5 XR scenario driver** (extend the `UScenarioController` pattern: replay hand source in `-game -RenderOffScreen` with XR off, timeline of trace files, observe kernel events, capture spectator frames), **M**/**L**. Acceptance: the scenario "perfect vs glob dummy" yields a `perfect` event at the authored tick ±1 and a frame showing the white ring, with no headset attached. | Recording needs the owner; everything after is headless |
| Build and packaging | `src/lib/packaging/` (UAT generator with an Android branch, `uat-command-generator.ts:185-188`; Android profile arm64, SDK 26/34, `build-profiles.ts`); `smoke-test` Win64 only; MCP `pof_package_preflight` | No APK ever built; no Quest settings; NDKROOT unset; JDK 22 compatibility unknown; no device deploy | **P2 Quest APK lane** (Quest build profile: arm64, Vulkan, multiview, ASTC, Meta manifest flags, hand-tracking permission; toolchain preflight for SDK, NDK, JDK versions against the engine's requirement; signing; MCP `pof_quest_package`, `pof_device_install`, `pof_device_launch`, `pof_device_logcat`), **M**. Acceptance: from a clean checkout one call produces an APK, installs, launches, and finds `MAGEVR_BOOT_OK` in logcat within 60 s; failures are classified as preflight, compile, cook, package, install or launch, and recorded in `build_history` with platform Android. | Headless with a headset plugged in by USB (owner wears it only for smoke) |
| Testing (device and XR Simulator) | UE automation plus functional tests, ScenarioController (`Source/PoF/Testing/ScenarioController.h`), L3/L4 drain (`src/lib/test-gate-runner/`) | No XR Simulator flow. The simulator's hand support is four canned poses (aim, poke, pinch, grab) driven by mouse and keyboard (Meta docs), so **it cannot draw sigils**. Real coverage comes from replayed traces (P5). | **P16 XR Simulator smoke** (launch the PIE or Windows build under the Meta XR Simulator; assert boot, pause and resume, menu poke), **S**. Acceptance: three scripted checks pass on Windows without a headset. Gesture and timing truth stays with P4/P5 plus the owner. | Headless (narrow scope) |
| Automated playtest and eval | Autonomous builder gates; judge; anim-critique; vision router | No recognizer accuracy harness; no bot "plays" VR | **P4 Gesture eval harness** (`src/lib/gesture-eval/`: run corpus replay through `MageArenaInput` built as a test binary or automation test; confusion matrix, false casts per minute on noise traces, compute latency; regression gate with thresholds from section 4), **M**. Acceptance: two runs give identical matrices; a planted bad template drops accuracy and fails the gate; the report carries n, corpus hash and build hash. Full-session playtests: scripted replay of an owner session (P5) reaching victory. | Headless; feel is the owner's |
| Performance on Quest | `src/lib/profiling/perf-capture.ts` noise-floor gate (pure); UE CSV events absent ("UNVERIFIED", `src/types/observation.ts:189-192`) | No on-device capture | **P6 Device perf capture** (adb-driven OVR Metrics Tool CSV or UE `-csvprofile` on device, pull, feed the existing gate; `CSV_EVENT` markers per bout in the game), **M**. Acceptance: three captures of the stress scene give p50, p95 and hitch counts with a measured noise floor; a planted 2 ms sleep in the game thread fails the gate. | Headless with the headset on the desk (proximity sensor taped, or a dev setting) |
| Video capture for submission | ffmpeg re-encode, Veo, Blender renders; UE gives PNG sequences only; no Movie Render Queue | No headset capture; no trailer pipeline | **P10 Video lane** (pull the headset's built-in recordings by adb; an ffmpeg assembly script for title cards, cuts, captions and loudness; **optional** deterministic replay re-rendered from a spectator camera via LevelSequence, proven headless in fleet memory 2026-08-17), **S** for pull plus assembly, **M** for replay re-render. Acceptance: a 2:50 cut assembled from an EDL file; replay re-render reproduces a recorded duel at 1080p60. | Capture needs the owner; assembly is headless |
| Store and release channel | Nothing beyond version bump (`src/lib/packaging/version-manager.ts`) | Total | **P11 Release lane** (`ovr-platform-util` upload to channel "Competition" with app id and token from env; record build id; channel and invite checklist), **S**. Acceptance: the build appears on the channel; the invite URL opens on a second account. | Headless, plus a one-time owner dashboard setup |

**Order of pof work** ("implement right away", sequenced ahead of need):
1. **Day 1-2:** P0, P1, then P2.
2. **Days 2-5:** P3 and P4 (thin), then P6 (thin).
3. **Week 2:** P8, P7, P5.
4. **Week 3:** P9, P12, P15.
5. **Week 4:** P13, P14.
6. **Week 5:** P11, P10, P16.

**Roughly 30-40 agent-days** of pof work run in parallel with about 25-30 agent-days of game work. Both are supervised through the existing harness, which needs `maxConcurrent = 1` while checkpointing (autonomous-builder doc, "Safety rails").

**What stays human no matter what pof builds:**
- whether a perfect feels earned;
- comfort;
- the corpus drawing;
- the cross-user sample;
- art and audio taste;
- the video's camera work;
- the dashboard and app setup;
- the final submit.

pof makes everything else checkable without a headset: build, install, boot, recognizer accuracy on recorded data, kernel conformance, balance census, perf on pulled captures, and frame evidence from replays.

**Decisions this pillar implies:**
- (a) **Do not route spells through GAS.** pof's spellbook pipeline authors GAS abilities with their own numbers (`docs/features/spellbook/README.md`). Here the kernel owns every number, so GAS would create a second authority.
- (b) **Do not start from PoF's UE project** (eligibility, plus its Android-breaking runtime code).

---

## 9. Pillar 2: forging the learning into the knowledge registry

### 9.1 Domain decision: extend `game-production`, do not found `game-development`

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
| N6 | `tracked-input-record-and-replay` | immersive-interaction | Recording real tracked input as versioned corpora and replaying it headlessly; the limits of emulators; device-in-the-loop evidence tiers | `runtime-observation-evidence`, `test-harness` |
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
| W1 end (11 Oct) | N1, N2, N5 Phase-1 drafts; two pitfall entries | Feasibility numbers with n, date, device, build hash and command (G2-G4). **A failed gate is evidence too** (negative results are written down). |
| W2 end | N7 draft | ≥20 green conformance vectors plus one planted-mutation failure |
| W3 end | N3, N4 drafts | ≥2 owner sessions labelled **felt (owner, n sessions)** plus ≥1 outside tester, observations recorded before interpretation |
| W4 end | N6 draft | A count of regressions caught by replay versus caught only on device |
| W5 end | N8 draft | The dated cut log (what was cut, when, by which gate) |
| 19-30 Nov | Phase-2 reconcile of N1-N8 (status `forged`), EXTENDS harvest, N9 drafted from the measured autonomy ledger | Applications under `stack: cpp` (and `node` for TS-oracle evidence) with `verified_on` / `verified_against`, anchors as `path:line "quote"`; `node scripts/gate.mjs --lane knowledge` green |
| After 11 Dec | An outcome note (the competition result is context, **never** quality evidence) | — |

**Evidence rules for this project:**
1. A lesson needs a measurement (n, unit, date, device, build hash, command) or a reproducible failure with its repro. A "vibe" is never evidence.
2. Owner feel is admissible only as **felt**, with a session count. Agent self-reports are T0 and never certify anything ("no gate self-certifies", law in `_laws.md`).
3. Numbers live in applications. Upper layers state rules without product names: no Quest, Meta, Unreal or Mage Arena.
4. Evidence pointers stay in local `.evidence.local.md` files (registry privacy rule, `ai-registry/CONTRIBUTING.md:82-89`).
5. This plan does not edit the registry. The owner, or a `/forge`/`/deepen` session in the registry, performs each step.

**The autonomy-coverage ledger (feeds N9)** is kept from day 1 in the game repo, one row per lifecycle phase:
- agent-hours;
- owner-hours (in the headset and out of it);
- which gates ran headless;
- which needed a human;
- which pof feature moved a phase from human to headless.

That table is the only honest answer to "what is the level of tooling for autonomous LLM development".

---

## 10. Risk register

| # | Risk | Likelihood | Impact | Mitigation |
|---|---|---|---|---|
| R1 | **Unreal hands toolchain:** the v207 plugin/5.8 mismatch (open, no workaround); few hands-first Quest hits are built in Unreal, so there is little community help | High | High | G0 on days 1-2 with three fallbacks (project-plugin build, UE 5.7, stock OpenXR); Pivot P-B (WebXR on the TS kernel) decided by Wed 7 Oct |
| R2 | **Recognizer accuracy for strangers:** judges are not the owner; air-drawing varies | Medium | High | Only 3 classes plus reject; a cross-user gate at ≥85%; teach the circle first; templates from 2 people; R1-R3 fallbacks |
| R3 | **Perfect window unreachable through tracking latency or jitter** | Medium | High | Onset timestamps; the 0.20 s data step; measured with the flash method; the design kill is explicit |
| R4 | **Deadline:** 6.5 weeks, 2 pillars, a new engine path | High | High | Feasibility first; systems freeze 1 Nov; RC 13 Nov; submit 16 Nov; cut order: Mirror IV, then Lash IV, then bout 2, then narrow-FOV mode |
| R5 | **First game for pof:** pof's assumptions (ARPG, GAS, Win64, one project) do not fit, and pof work crowds out game work | High | Medium | pof items are thin and test-first; P0/P1/P2 before anything else; GAS bypassed; an item may ship "thin" if its acceptance test passes |
| R6 | **Quest performance with Niagara and translucency** | Medium | High | G4 stress scene in week 1; CPU-sim particles; budgets per class; a perf gate every week |
| R7 | **Kernel divergence** between the TS oracle and the C++ port, or drift in shared data (the PC stream is actively editing) | Medium | Medium | Commit pins only; P7 vectors; P8 drift alarm; change requests through the PC orchestrator; the `vr-sim/` adapter as a fallback |
| R8 | **Eligibility:** "no pre-existing codebases"; reuse of the PC kernel and art; licensed assets | Low-Medium | Very high (disqualification) | A new repo and project; PC material dated after 24 Sep; no Fab or stock packs; ask on the Devpost forum before 12 Oct (Q9) |
| R9 | **Owner time:** about 20-22 headset hours, concentrated in W1, W5 and W6 | High | High | Device sessions batched and scripted (a checklist per session); everything else headless; a second tester recruited early |
| R10 | **Genre fit:** action is outside the listed Gaming genres; Stage 1 viability | Low-Medium | Medium | Pitch strategy and reading; lead the video with deliberate perfects and composition |
| R11 | **Comfort complaints** (blink, threats at the edge of view) | Low | Medium | Snap plus vignette, ±70° default arc, nothing from behind, Gentle mode |
| R12 | **Store or dashboard setup delays** (membership, app creation, channel) | Medium | High | Owner does Q1/Q2 on day 0; P11 is tested by uploading the week-1 APK to the channel early |
| R13 | **Paid quotas run out** (Tripo credits are already exhausted per fleet memory; ElevenLabs; MiVRy licence) | Medium | Low-Medium | Low-poly authored arena; a small asset list; budget guard per provider |

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

**Description outline** (for 500 words; confirm the limit):
1. Inspiration: a Roman-era arena of elemental mages, and the wish to cast spells by hand rather than by button.
2. What it is: three verbs (draw, ward, blink), a tier clock and Flow, ten minutes.
3. How it was built: Unreal, Meta hands, a deterministic combat kernel shared with a PC sibling, and an agent-driven toolchain (pof) that verifies builds, recognizer accuracy, balance and performance headlessly.
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

1. Are you a **Start program** member, and is a developer-dashboard app created? (Eligibility; R12.)
2. Which **headset** (Quest 3 / 3S / other), is developer mode on, and can it stay on the desk by USB for headless perf and replay runs?
3. **Hours in the headset per week** you can commit. The plan assumes about 20-22 hours in total, concentrated in W1, W5 and W6.
4. **Repo location and kernel authority:** a new `kiro/mage-arena-vr` repo, and approval for the mage-arena orchestrator to accept a blink and pad extension to the arena kernel (or should VR fork with an adapter)?
5. **Target launch date** for the form (default: spring 2027).
6. **A second person** for the cross-user sigil sample in week 1 (20 minutes) and 3 cold playtesters in week 5.
7. **Paid tools** if needed: a MiVRy licence (fallback R2), a music provider, more 3D or image credits.
8. **Asset policy:** generated and authored only, or are licensed packs acceptable after checking the rules?
9. **Eligibility check:** are you comfortable reusing the PC kernel, data and art created 1-2 Oct, after the 24 Sep window opened? Should we ask on the Devpost forum?

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
