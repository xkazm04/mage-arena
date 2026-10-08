# Owner decisions

Newest first. Quotes are the owner's words; the lines under them are what the plan does about it.

## 2026-10-08 - D-G4 particle systems: a commandlet here now, pof P9 later

> Owner, 2026-10-08, asked "The 10-31 budget's particle rows need 6 Niagara systems. The reading (6015e16) says to make
> them with a small commandlet in this repo, from the engine's own default emitter templates. CLAUDE.md says a capability
> pof lacks is built in pof first (pof P9 is that capability), and R8 bans stock packs. Which way?": "Commandlet here
> now, pof P9 later"

- **What binds:** the 6 `NS_Budget_` systems of `docs/research/NIAGARA-PROXY-2026-10.md` 1.4 are made in this repo by
  `Tools/CreateBudgetFxCommandlet` (route (b), 1.2) from the engine's default Niagara emitter templates and default
  materials. They are committed under `apps/vr/Game/Content/Budget/` through LFS and cooked into the V1 APK through
  `+DirectoriesToAlwaysCook=(Path=/Game/Budget)` (R6). The build starts once `MageArenaVR.Build.cs` is clean.
- **The R8 reading** (`docs/PROJECT-PLAN.md:551`, "no Fab or stock packs"): engine default templates that this
  project's own tool duplicated and parametrized after 24 Sep are not a stock pack. The limit: the ruling does not cover
  shipping engine content unchanged (route (a), 1.1), engine sample content, or any Fab or marketplace pack.
- **The pof-first rule** (`CLAUDE.md`, Tooling; `docs/PROJECT-PLAN.md:396`, P9): this is an exception for the stress
  scene's measuring instrument, which is not the art pipeline. pof P9 may absorb the commandlet later. This ruling does
  not change `CLAUDE.md`.
- **The constraint that forced it:** the particle rows of the 10-31 desktop core gate (D-G4) need real systems that can
  be tuned. Route (a) gives untunable bursts (1.1). Route (c) cannot exist in a cooked build (1.3). pof has no P9 yet.
- Lost: "pof P9 first" (it would have held the particle build, and the 10-31 gate would have reported the particle rows
  "not exercised"). Lost: "Desktop proxy only, not in the APK" (the commandlet without the `DirectoriesToAlwaysCook`
  line, so the device particle row (R6) would have waited for assets from pof P9).
- Not part of this ruling: the build still waits for the operator's uncommitted `// touch` line in
  `MageArenaVR.Build.cs` (NIAGARA-PROXY 5.1).

## 2026-10-07 - v2 vision accepted; the 18 Nov build is its Tiro-day slice

> Owner, 2026-10-07, after the vision interview (waves 0-5, answers verbatim in `docs/design/V2-VISION.md`): "Take
> please https://github.com/xkazm04/mage-arena/pull/1, merge, sync with local main, and start to execute implementation
> per the design doc"

- The one-page synthesis in `V2-VISION.md` is the v2 direction. The slice build order, card list and canon change
  requests are in `docs/design/V2-ROADMAP.md`. The vision file stays the history; this entry is what binds.
- **DF-003 answered: HP-gated phases.** Named mages get 2-3 HP-gated phases (proposal 66 % and 33 % HP); each break
  surges both tier clocks by one tier and swells the crowd; the last phase opens with the rival's signature (Brennic:
  Sunfall). Census targets: signature in 100 % of duels that reach the last phase, median 45-80 s, win rate >= 70 % at
  competence 1 and 40-60 % at 1.5. The clock surge is shared semantics, so it goes to TV as a change request; until then
  it is a VR overlay rule.
- **DF-004 answered: option A, ember spit.** Hounds spit slow, telegraphed magic embers from range; the death burst
  throws one last ember. A VR overlay attack until the TV change request lands.
- **Split hands:** the perfect latch clears after the casting hand is idle about 0.3 s; a split Bolt at Flow 5 is
  allowed and does not spend the Crest. Overlay rules, no change request.
- **The collar economy:** Cracks (clean play, saved across days, open the Breaking) and Tithe (spilled magic: renown
  now, a stronger jailer later). The collar caps at tier IV until the Summa's Breaking. New canon, change request
  before it ships.
- **Slice (18 Nov):** prologue, teach, a full Tiro day (ritual, soldiers, creatures, Brennic in the semifinal, the final
  against a second school). **First cut if the Fri 23 Oct checkpoint slips: the second school**; the final becomes
  Corvo, a second Ember entrant on the Fire kernel.
- **ElevenLabs approval extends to pre-rendered voice lines** (ritual, taunts, crowd chorus, 10 words or fewer each),
  generated from a written cue sheet with the count stated on the card. No runtime calls.
- **Second school for the Tiro final: Air** (owner, 2026-10-07, asked "Earth, Air, or Corvo now?": "Air"). The Gale Tent
  entrant (Iskar or Lio) as an AI opponent with defence D (air form). Card T23; Corvo stays the 23 Oct fallback.
- Four routes, one per member of the Four (Cassia, Brennic, Garran, Iskar); Cassia's (Water) is the slice. All four
  schools playable at launch (spring 2027).
- Lost: the cautious campaign proposal (90 s of story per day, static tableaux, no personal arc), sliders for Sunfall
  (earlier unlock or a scripted beat), DF-004 options B and C.

## 2026-10-07 - Q9: reuse of the TV kernel, data and design accepted provisionally, and we ask

> Operator, 2026-10-07, asked "Q9, due before 12 Oct: may the VR entry reuse the TV kernel, combat data and design, all
> made on 1-2 Oct? The rules re-read today still say No pre-existing codebases.": "Accept now and ask too". Note: "record
> the reuse as provisionally accepted in DECISIONS.md with the dated evidence, and draft the question for the operator to
> post to the organizers before 12 Oct. A no comes back as a scope ask."

- Provisionally accepted: the VR entry reuses the TV rules kernel, the pinned combat data and the game design. All were
  authored on 1-2 Oct 2026, after the window opened on 24 Sep.
- Evidence, rules (`/rules`, fetched 2026-10-07, HTTP 200, 183,554 bytes, `grep -F` exit 0): "conceived of and built within
  the competition window (starting September 24, 2026). No pre-existing codebases, no shipped titles, no early access
  builds repurposed". Also "The Project must be ORIGINAL to each Entrant".
- Evidence, dates (read-only git on `kiro/mage-arena-tv`):
  - `git log --reverse --format='%h %ad %cd %s' --date=iso | head -5`: first commit `1e9de1c` at 2026-10-01 23:46:39 +0200;
    kernel commit `13256c6` at 2026-10-02 00:08:42 +0200.
  - `git log -1 --format='%h %ad %cd %s' --date=iso baeac66`: the pinned combat data commit, 2026-10-02 13:48:12 +0200
    (it was `68a4d68` before the 2026-10-07 history rewrite, see `REPO-LAYOUT.md`).
  - `git log --format='%ad' --date=short | sort | head -1`: 2026-10-01, the earliest author date in the TV repo.
  - This repo: `git log --reverse --format='%h %ad %s' --date=iso | head -1`: `5bc06ee` at 2026-10-02 17:09:32 +0200.
  - `git grep` for any date before 2026-09-24 at `1e9de1c` and at `baeac66`: no hit in either.
  - Caveat: author and committer dates are equal for the three commits above, but 5 of 116 TV commits differ by seconds to
    minutes, so the rewrite did touch some dates. The earliest committer date is also 2026-10-01.
- The constraint that forced the choice: the rule says "No pre-existing codebases"; a breach means disqualification
  (PROJECT-PLAN R8, very high impact); and the rules page names no contact channel. It links the competition's Discussions
  page (`/forum_topics`) and the Devpost contact page, with no mailto and no manager email.
- Lost: accept on the dates without asking (a silent disqualification risk we can cheaply remove).
- Lost: ask first and keep Q9 open until the reply (no reply is guaranteed, and the plan cannot wait on one).
- Disclosure, pof: the studio tool `kiro/pof` predates the window (PROJECT-PLAN section 2, eligibility paragraph, and section 8 decision (b);
  `build-5_8-game.log` is dated 2026-06-18). It is editor tooling and does not ship in the APK (the `MageArenaDev` row in section 7). The drafted
  question asks about it separately, so the owner can cut that part.
- Next: the owner posts the drafted question before 12 Oct 2026, from `docs/submission/ORGANIZER-QUESTION-Q9.md`
  (status: draft, not sent). A "no" from the organizers comes back to the owner as a scope decision.

## 2026-10-07 - Two repositories, TV and VR; evidence out of git

> "We have now two repos for TV and VR, all the other should ideally disappear split into those two, gitignore carefully
> not to overweight github repos." Then: "Approved procedure in your recommended order. Names: mage-arena-tv,
> mage-arena-vr. Review evidence outside archive. PNG evidence can be dropped."

- Two repositories, one checkout each: `kiro/mage-arena-tv` (GitHub `mage-arena-tv`, was `mage-arena-pc`) owns the canon
  (combat data, simulator, story, quests); `kiro/mage-arena-vr` (GitHub `mage-arena-vr`, was `mage-arena`) owns the VR
  channel. VR keeps consuming TV through the pin. This supersedes `MONOREPO.md` (the monorepo plan).
- The lane folders are retired; parallel work uses short-lived `.worktrees/`.
- Review evidence media lives in `kiro/mage-arena-archive/`, not in git, and was dropped from TV history. Shipped media
  is in LFS. What was done, and the old-to-new commit map: `REPO-LAYOUT.md`.

## 2026-10-07 - Quest 3S ships untested

> Operator, 2026-10-07 11:15 UTC, asked "Quest 3S is declared, but the planned device is a Quest 3. Should we get a 3S for
> one test session, or ship 3S untested?": "Ship 3S untested". Note: "Operator accepts the risk: Quest 3S stays declared
> and untested; record the acceptance in DECISIONS.md and FEASIBILITY risk 3."

- Quest 3S stays declared (`+SupportedDevices=Quest3S` in `apps/vr/Game/Config/DefaultEngine.ini`). There is no 3S session
  in V3 and no 3S on the 31 Oct device order.
- What carries over: the performance budgets, by chip and eye buffer (FEASIBILITY Q3).
- What stays untested: sigil legibility on the lower-resolution panel, peripheral threat cues (lens and FOV), hand-tracking
  quality, and the 3S camera rate. The narrow-FOV mode (D3) bounds the FOV part only.
- The constraint that forced the choice: the planned device is a Quest 3, and a 3S would be a separate purchase or loan.
- Lost: get a 3S for one V3 session (sigil legibility, peripheral threat cues, the `Camera FPS` line, one perf capture).
- Lost: Quest 3 only (remove `+SupportedDevices=Quest3S`).

## 2026-10-07 - Supported headsets: Quest 3 and Quest 3S

> Operator, 2026-10-07 morning: chose Quest 3 and Quest 3S.

- The APK declares Quest 3 and Quest 3S only (`+SupportedDevices=Quest3` and `+SupportedDevices=Quest3S`; the
  `ExtraApplicationSettings` line that also named Quest 2 and Quest Pro is gone). Delivered as finding F2.
- The milestone-4 frame and memory budgets stay measured on Quest 3.

## 2026-10-03 - Seated balance: calibrate with every knob, and invent school-specific defences

> Calibration may use: wave composition, enemy pressure, player spell power, defence (ward/blink). And: "Invent class
> specific defense mechanisms. We can have designs: A. One hand holds magic barrier, second casts weaker spells. B. Earth
> class can cast stone form (invulnerability to cast the heaviest spell) C. Fire class can cast fire wall first to protect
> against enemies and projectiles D. Air class can take air form with high percentage of dodge mechanism E. Mages wielding
> staff can put staff into ground with magical barrier and cast two handed destructive spells"

- The seated balance calibration (DF-001, DF-002) may move all four knob families, VR overlay only; the owner signs off the
  final numbers.
- New design direction: **each school gets its own seated defence**, turned into concrete mechanics in
  `docs/design/SCHOOL-DEFENCES.md` (gesture, rule, cost, how it is measured). Water - the competition slice - gets A
  (one-hand barrier + one-hand weaker casting) and E (planted staff: a standing barrier that frees both hands for a
  two-handed spell); Fire gets C (fire wall, also used by the Fire mage AI); Earth B (stone form); Air D (air form).
- These are game-design additions for every channel, built VR-first like the schools, and offered back via change requests.

## 2026-10-03 - Seated combat: enemies fight from the floor, nothing reaches the dais (DF-001)

> Option "A: ranged from the dais" for DF-001 (`docs/design-findings/DF-001-seated-wave1.md`).

- From a seat the player lost Wave 1 at 16.45 s with the pinned, walking-calibrated numbers. Decision: **enemies stay on the
  arena floor below the raised dais and attack from range** (spears thrown, stones slung); melee reach never touches the
  dais. Threats come to the player; the player wards, blinks and casts.
- Implemented as a **VR-only overlay** (`combat.vr.json`); the shared kernel rules and the pinned data stay unchanged and the
  conformance vectors stay byte-identical. Re-measure the seated Wave 1; wave composition (option C) is the fine-tuning
  knob if needed. Card T10.

## 2026-10-03 - Sigil shape, and all four schools in every kernel

> "c. circle + bar"

- The canonical sigil is **a circle plus a bar**; the bar's angle selects the line (vertical / horizontal / diagonal),
  as the recognizer, the glyphs and `DESKTOP-INPUT.md` already do. The plan's teach step is corrected.

> "d. all kernels need all 4 schools - I would start on your side"

- Every channel's kernel will implement **all four schools** (Water, Fire, Earth, Air), not only Water. The pinned
  desktop/TV kernel has Water only (its duels are water proxies).
- **VR starts:** the C++ kernel implements the missing schools first, data-driven from the shared data files, and each
  school is offered back to the desktop/TV channel as a change request with its rules, data and test scenarios.
  Fire (with Heat) comes first because the competition slice ends with the Fire mage duel; Earth and Air follow.
- Consequence for conformance: for the new schools the C++ kernel is the reference until the desktop/TV channel ports
  them; their scenarios are kept separate from the TS-generated vectors so the existing 45 stay byte-identical.

## 2026-10-02 - One repository for all channels

> "try to at some point merge the codebases so we don't split folder for each subproject"

- Home: **this public repository, renamed `mage-arena`** (owner's choice over a private home; the whole game design
  becomes public). Order: **VR first, the PC/TV lane branches later** at a quiet point of their orchestrator.
- Plan and phases: `MONOREPO.md`. Phase A runs right after T03 is settled and before T04 is dispatched.

## 2026-10-02 - Start program approved

> "Start program approved, we can charge its benefits whenever the situation calls for it, asset library can be a
> good fallback situation."

- Eligibility condition met (membership is required at submission time).
- Start benefits (priority technical support, mentorship, office hours, software credits, the **Meta Asset Library**)
  may be used when the situation calls for it, without a separate request.
- The Asset Library is the named **fallback for risk R0** (3D models and animation). Each asset's licence is checked
  against the competition's "original, not a wrapper" rule before use, and its use is recorded.

## 2026-10-02 - Who does the work, what may be paid for, and the real risk

> "For music and effects use ElevenLabs services ... other paid tools not allowed without specific request and
> reasoning, like for example Tripo credits for 3D generation and animations if no other choice."

- Music and SFX: **ElevenLabs**, key `ELEVENLABS_API_KEY` (the owner pointed at the personas `.env`; the variable
  actually lives in `kiro/pof/.env`, which is pof's audio provider config). Read it by name; never copy it into this repo.
- **No other paid tool** without a written request and reasoning to the owner first. MiVRy (paid recognizer licence)
  is therefore out: the recognizer is the in-house $Q port. Tripo-style 3D/animation credits only on request, when
  there is no other choice.

> "Risk is in the game design and animations of 3D models - that's something we did not prove to master for a long
> time. I believe we don't overcome it in next 3 weeks, then we reconsider the plan."

- **Top risk: game design quality and 3D model animation**, not the toolchain. **Reconsider checkpoint Fri 23 Oct**
  (three weeks from this decision): if fun-in-greybox and animated 3D opponents are not proven by then, the plan is
  reconsidered with the owner rather than pushed on.

> "For execution use Grok CLI and Grok 4.7 model to do most of the heavy lifting ... it can do most work if you keep
> yourself as orchestrator and provide detailed plan. We then take over together once more challenging and creative
> tasks will remain, keeping Grok as pawn until usage depleted."

- **Execution model:** Grok CLI (`grok-4.7`) implements; the Claude session orchestrates with detailed task cards,
  verifies every result itself and commits. Creative and hard calls (game feel, design judgement, art choice,
  anything Grok fails twice) go to Claude with the owner. Grok runs until its usage is depleted. Protocol:
  `ORCHESTRATION.md`.

## 2026-10-02 - Desktop first, VR from November

> "Headset not decided yet, we will need to have PC experience saving hand gestures under keybindings as quick
> actions. For emulators/simulators we just pick one device most used/popular to test against. Once the game core
> plays well and art/animations resolved, I can invest into any device to port and experiment - probably Meta Quest 3
> 512GB."

- October is a **desktop build** of the same Unreal project. Every hand gesture is reachable from the keyboard and
  mouse (see `DESKTOP-INPUT.md`), through the SAME gesture pipeline the headset will feed later.
- Simulator/emulator target: **Meta Quest 3** profile only.
- No headset is bought, and no on-device gate exists, before the core game plays well and art and animation are resolved.

> "No to WebXR pivot, I would say the core game design VR-less should take until the end of October. We will know by
> this date whether gameplay, graphics, models and animations, audio on high quality level and we can invest full
> speed into VR."

- The WebXR pivot is **rejected**. The engine is Unreal.
- **Gate on 2026-10-31 (desktop core gate):** gameplay, graphics, models, animation and audio at high quality, VR-less.
  Only after it does the VR port start at full speed (1-18 Nov, competition deadline 2026-11-18).

> "Start program not yet - but I will apply."

> "Create in ai-registry subtree in game development specific to Mage Arena - it is game we work in parallel on TV
> platform too, with potential to become PC game in the future. Once one channel masters certain mechanic other can
> benefit so the same game design problems are not repeated."

- Three channels of one game: **TV/desktop** (`kiro/mage-arena`), **VR** (this repo), **PC** (future). A mechanic
  solved in one channel must be reusable by the others.
- Placement chosen by the owner the same day: **hybrid**. Generic lessons -> `game-production` subjects (no product
  names); channel realisations -> `applications/mage-arena-<channel>--<technique>.md`; game facts -> registry
  `memory/` namespace `mage-arena` (seeded in registry commit `3f35c804`, including the mechanic-by-channel matrix).

> "Lets scaffold repo in kiro/mage-arena-vr."

## 2026-10-02 - Art comes last

> "I shortlisted 13, 25, 30 to have in mind throughout the project. Starting style-less with any baseline wireframe
> convenient to prove the game mechanics. If all aspects of the game will work together, then we spend effort to
> finetune the graphical side and art, as technical limitations will enforce us to prefer simplistic styles over
> realism and high fidelity."

- Greybox until the mechanics work together. Shortlist: **13 Screen-Print Poster, 25 Moonlit Silver Nocturne,
  30 Chalk & Slate** (`ART-STYLE-STORYBOARD.md`). The PC game's Tessera & Lime is not inherited.
- The greybox still honours the threat colour language (element colour = absorb, steel = dodge, black core = leave).
