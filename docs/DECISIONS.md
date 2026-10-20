# Owner decisions

Newest first. Quotes are the owner's words; the lines under them are what the plan does about it.

## 2026-10-21 - Combat data: the current pin is VR's frozen baseline

> Operator, 2026-10-21, relayed through the App Master (answering the gate ask): "A, frozen baseline."

- **What binds:** the pin recorded in `apps/vr/data/PINNED.json` is VR's frozen baseline. `node apps/vr/tools/check-pin.mjs`
  and the 45 conformance vectors stay a merge gate against that baseline, unchanged. `apps/vr/data/pinned/**` is still never
  edited. Every VR combat-data change goes into the VR overlay (`apps/vr/data/vr/`). CR-001..CR-007 become VR design records
  for overlay changes, not requests to TV, and nobody re-pins from TV.
- **Brief goal 3 reads in effect:** "baseline unchanged (check-pin green, 45 vectors byte-identical) and the VR overlay
  passes its checks".
- **Supersedes:** the "pending that ask" lines of the 2026-10-20 "VR first" entry below (its body is not edited). The
  gate question it left open is ruled here. The docs that entry listed are aligned in the same change: `docs/REPO-LAYOUT.md`,
  `CLAUDE.md` "Combat data", `apps/vr/data/README.md` and the headers of `docs/change-requests/CR-00x`.
- **Q9:** the operator's words: "post a revised Q9; the operator posts it." The revision is being drafted in
  `docs/submission/ORGANIZER-QUESTION-Q9.md`; the status there records the date and link once the operator has posted it. It
  has not been posted.
- **Cross-channel record:** the TV-side record of this ruling is still owed.
- Lost: B (VR-owned data, rework of check-pin); C (TV stays the data channel until 18 Nov).

## 2026-10-20 - VR first: the TV data and design are the baseline, the TV game becomes obsolete

> Operator, 2026-10-20, relayed through the App Master (answering Q9): "It would be preferred to use data and game design as the baseline, from this baseline TV game will become obsolete and VR suprerior with main focus"

- **What binds:** VR (this repo) is the main product. The TV game is obsolete as a product. The TV combat data and game
  design, as pinned in `apps/vr/data/PINNED.json`, are the starting baseline.
- **What stays in force until the owner rules on the gate:** the pinned data stays as it is (`apps/vr/data/pinned/**` is
  not edited), `node apps/vr/tools/check-pin.mjs` and the 45 conformance vectors stay a merge gate, and CR-001..CR-007 keep
  their current status. The owner has not yet ruled how the conformance gate changes (frozen baseline, VR-owned data, or
  keeping TV as the data channel). That is an open scope ask, and this entry does not decide it.
- **Superseded in part, pending that ask (none of these is edited here):**
  - DECISIONS 2026-10-07 "Two repositories, TV and VR" (the line that TV owns the canon);
  - `docs/REPO-LAYOUT.md` (the "The canon" row and "Changes flow back as change requests");
  - `CLAUDE.md` "Combat data has one owner";
  - the "Filed for the desktop/TV kernel channel" lines in `docs/change-requests/CR-00x`;
  - the brief's key goal 3 ("Keep combat data conformant with the TV game").
- **Q9:** reusing the operator's own data and design still stands (DECISIONS 2026-10-07, Q9 entry). Whether Q9 is posted
  or withdrawn is still open and is with the owner. `docs/submission/ORGANIZER-QUESTION-Q9.md` is not changed.
- **Cross-channel record:** REPO-LAYOUT says a decision that changes every channel is recorded in the TV repository
  (`kiro/mage-arena-tv`). That TV-side record is still owed; it has not been written.
- Lost: TV as the main product or co-equal channel.

## 2026-10-20 - Cut slice: no cut, the 31 Oct desktop core gate plans the full Tiro day

> Operator, 2026-10-20, relayed through the App Master: "keep every bout; plan the 31 Oct gate on the full Tiro day and
> flag the risk."

- **What binds:** this rules the 18 Oct pivot. Bout 2 (creatures) stays, and the second school stays in: **Air**, the Gale
  Tent final with Lio (`docs/change-requests/CR-007-air-school.md`; DECISIONS 2026-10-07). The 31 Oct desktop core gate
  is planned on the full Tiro day, not on a cut slice.
- **The risk, in plain words:** the 18 Oct mechanics gate was missed, and the 23 Oct second-school checkpoint is three
  days away. The plan's own fallbacks are the two things this ruling declines: PROJECT-PLAN section 1 says a missed 18 Oct
  gate slips at most to 21 Oct "and cut bout 2", and section 10 R4 lists the cut order (Mirror IV, Lash IV, bout 2,
  narrow-FOV mode). DECISIONS 2026-10-07 names the second school as the first cut if the 23 Oct checkpoint slips. Keeping
  everything leaves the 31 Oct gate with no scope to give back. DF-008 (Lio loses every duel in about 20-24 s) is also
  still open.
- Sections read: PROJECT-PLAN section 1 (milestone and gate rows, the 18 Oct and 31 Oct slip rows), section 5 (D2 and D4
  rows), section 6 (the Tiro-day timeline, Bout 2 creatures), section 10 (R0, R4).
- Lost: the cut options (cut bout 2; cut the second school; port a two-bout slice).
- Not part of this ruling: PROJECT-PLAN.md is not edited; where it conflicts, this entry wins.

## 2026-10-20 - DF-003, DF-005, DF-006, DF-007, DF-008, DF-009: adopt every recommendation

> Operator, 2026-10-20, relayed through the App Master: "adopt each DF's recommended option; card one design-suite
> re-run."

- **What binds:** the recommended option of each finding, in the finding's own words:
  - DF-003: its Recommendation section: leave `wide` in the proposal file (`calibration-proposal.json`); the finding
    itself says do not sign it off.
  - DF-005: **A. Re-arm on clear** (the ward counts as fresh again for one `perfectWindowS` after the latch clears; VR
    overlay only).
  - DF-006: **B. A phase-break beat**, with **A** (adopt the `wide` proposal).
  - DF-007: **A and B together; re-measure** (A: ward some embers, not all, in the reference script; B: slower spit
    cadence, 1.5 s, VR overlay).
  - DF-008: **A. AI first** (Lio wards like the Fire mage, uses Air Form against the player's bolts, spends tier II-III
    spells once unlocked; re-measure).
  - DF-009: **A. Count only spectacle** (`missedCast` 0 for Bolts, kept for tier I+ casts).
- **Where the work goes:** the implementation and the one `MageArenaDesign` re-run are a task card for the UE host
  (Wolf, see the host entry below). Nothing here changes code or data. The design suite stays **red** until that run; no
  result is claimed by this entry.
- Lost: the other options of each finding (DF-005 B and C; DF-006 C and D; DF-007 C and D; DF-008 B and C; DF-009 B and C).

## 2026-10-20 - T38 asset source: Claude + owner authoring

> Operator, 2026-10-20, relayed through the App Master: "authored rig with four clips; Lynx's local TRELLIS + Blender
> rigging pipeline (PoF 3D rounds) may feed it."

- **What binds:** the first animated opponent (card T38) comes from an authored rig with four clips (idle, cast, hit,
  death), authored by Claude and the owner. The local TRELLIS and Blender rigging pipeline on Lynx (PoF 3D rounds) may
  feed it.
- Lost: the Meta Asset Library as the source, and paid 3D or animation credits (PROJECT-PLAN R0's fallback and its paid
  option).

## 2026-10-20 - Unreal host: all Unreal work on Wolf (x64); Lynx gets no Unreal install

> Operator, 2026-10-20, relayed through the App Master: "all Unreal work stays on the x64 machine (Wolf). Lynx is the
> Node/docs/orchestration host and does not get an Unreal install. Write UE work as cards/source for Wolf; never claim UE
> results from Lynx."

- **What binds:** UE builds, automation suites and content work run on Wolf only. Lynx writes cards and source for Wolf and
  reports no UE build or test result.
- Lost: a UE install on Lynx.

## 2026-10-16 - D-G4 draw calls are one total budget, and the two Build.cs bBuildEditor blocks stay separate

> Athena, 2026-10-16, under the operator's delegation, answered run `ef6642f6`'s two questions. Recorded as relayed; the
> wording is not quoted beyond the rulings.

**1. Draw calls: one total of <=150, no separate particle line**

- **What binds:** the D-G4 draw-call row is a single total budget of <=150 for the whole frame. Particle systems get no
  draw-call line of their own. The measured gap, median 194 and max 201 against 150
  (`docs/research/BUDGET-DG4-2026-10.md` section 10.2 and 10.3, commit `ca28cce`), belongs to lever B in the
  presentation lane. Lever B's target now includes the particles: the 6 `NS_Budget_` systems add about 11 draw calls at
  the median (8.5 measured median 183 without them), and they are closed by lever B along with the rest.
- **The constraint:** the plan's section 7 Quest 3 budget is one frame. The GPU draws one frame, so a particle
  allowance carved out of it would not give the frame more room.
- Lost: a separate particle draw-call allowance (for example 150 plus a particle line). It would have turned the row
  into a pass on paper with the frame still over 150.
- The row stays **fail** until lever B is on master and the stress driver follows the shipping presentation (section 9).

**2. Build.cs: the two bBuildEditor blocks stay separate**

- **What binds:** `apps/vr/Game/Source/MageArenaVR/MageArenaVR.Build.cs` keeps its two `if (Target.bBuildEditor)`
  blocks apart, as the spec required: the D-G4 `NiagaraEditor` block near the top (for the commandlet that builds the
  `NS_Budget_` systems) and the T26 `UnrealEd` and `AssetTools` block at the end (for the SFX import commandlet). A
  sweep, cleanup or refactor must not fold them into one.
- **The constraint:** the spec (`docs/research/NIAGARA-PROXY-2026-10.md` section 4.1) placed the Niagara lines above
  the public dependency block so a three-way merge with lane A's edit to the end of the file takes both. A second block
  in the same place conflicts, and each block belongs to a different lane's feature.
- Lost: folding them into one block. Section 4.1 had called that a follow-up commit on master after the merge. It is
  not wanted.
- Not part of this ruling: neither research doc nor `Build.cs` is edited.

## 2026-10-16 - Plan milestone 3, "Asset footprint reduced": measured by waste, not by tracked size

> Athena, 2026-10-16, under the operator's delegation, answered the App Master's scope ask. The ask was whether
> milestone 3's goal compares tracked bytes at or below `11b9bb2`, or something else
> (`docs/research/BASELINE-2026-10-07.md` section B, last bullets). Recorded as relayed; the wording is not quoted beyond
> the ruling.

- **What binds:** the goal measures WASTE, not tracked size. It is met by all three of: 0 unexplained duplicate groups,
  no texture over budget, and no test-only content in the staged `Clips` folder. It is not met or missed by tracked bytes
  against `11b9bb2`.
- **The constraint:** the re-measure at `ddbd3c1` (`BASELINE-2026-10-07.md` section B, commit `9fef818`) found tracked
  bytes at 66,776,864 against 61,109,210 at `11b9bb2`, +9.3 %. The growth is real game content: the first audio import
  (20 SFX uassets, 2.5 MB of LFS), clips, source and docs. Every possible removal recovers under 1 MB: the mudra mirror
  duplicates are 622,918 bytes and kept by design (A.6), and the test-only clips are 311,952 bytes. Together they could
  not close the 5,667,654-byte gap.
- **Where it stands against the three tests** (as of the `ddbd3c1` re-measure): the 2 duplicate groups are the mudra
  mirrors, explained in A.6; 0 texture assets exist and 0 images are over 2048 px; the staged `Clips` folder still holds
  the test-only `Clips/system/` and `Clips/ward/` clips.
- **The action this orders:** move `Clips/system` and `Clips/ward` out of the staged folder. It is being delivered in a
  separate run and is **not done** by this entry. Until it lands, the third test is open and the goal is not met.
- Lost: (a) keep tracked bytes as the measure, which can only be met by deleting real content. (b) Measure staged
  package bytes, where A.3 already removed 188 MB (188,292,369 bytes), so it would read as met while the test-only clips
  still ship.
- Not part of this ruling: the research docs are not edited; they still ask the question as open.

## 2026-10-13 - F12: take UE 5.8.4 on purpose, on one planned day

> Owner, 2026-10-13, answered in the Athena session. The ask was F12 of `docs/research/STACK-OPPORTUNITIES-2026-10.md`,
> "(a) freeze at 5.8.3 until 11-18, or (b) take 5.8.4 on purpose, on one day?": "(b)". The owner's wording is not quoted
> beyond the choice; the answer is recorded as relayed.

- **What binds:** UE 5.8.4 is taken on purpose, on one planned day after it posts, rather than freezing at 5.8.3 until
  11-18. The engine is not updated by surprise and is not touched before that day.
- **The constraint:** 5.8.4 is expected around 2026-10-20 (inferred from the 28-day cadence), inside the sprint to the
  10-31 desktop core gate. The engine and both v207 MetaXR plugins match today on CompatibleChangelist 55116800.
- **The day's procedure** (`STACK-OPPORTUNITIES-2026-10.md` 8.3 and F9):
  1. Before installing, read the posted 5.8.4 notes and compare the new `Build.version` CompatibleChangelist with the
     plugins' BuildId 55116800. If they differ, stop and raise it: the plugins must be rebuilt or updated first.
  2. Cold `apps/vr/tools/build.ps1`.
  3. The full `MageArena` suite, with a per-test diff against the 5.8.3 run.
  4. `node apps/vr/tools/check-pin.mjs`, and the pin hash in the boot log.
  5. Update `docs/XR-TOOLCHAIN.md` and the `CLAUDE.md` Build line.
  - Never take it on a day another UE build or automation run is in flight.
- **Owner action still open:** launcher auto-update for `UE_5.8` should be off, so the update lands on the chosen day and
  not by surprise. Unverified whether it is on now.
- Lost: (a) freeze at 5.8.3 until 11-18. The 10-13 feed recommended it because both known 5.8.4 fixes (UE-397316,
  UE-396802) sit outside the row-3 list. The owner chose otherwise.
- Not part of this ruling: `CLAUDE.md` and `XR-TOOLCHAIN.md` still say 5.8.3 until the day's procedure runs.

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
