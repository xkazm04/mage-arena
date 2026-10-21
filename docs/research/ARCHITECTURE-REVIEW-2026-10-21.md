# Architecture review 2026-10-21: the overlay seam under the frozen-baseline ruling

The fourth run of the `codebase-architecture-review` charter, and the first since DECISIONS 2026-10-21 ("A, frozen
baseline"). It is report-only. It reads the committed tree at `3d8e862` (`master`) on Lynx. It ran no build, no editor and
no automation run, and it claims no UE result. Every `file:line` is at `3d8e862`. `S` is
`apps/vr/Game/Source/MageArenaVR`. Commands are numbered as in section 6.

**The question.** Under the ruling (the pin in `apps/vr/data/PINNED.json` is VR's frozen baseline, and every VR
combat-data change goes into `apps/vr/data/vr/`), which few structural problems will cost the most before the
2026-10-31 desktop core gate and the 2026-11-18 submission?

**Constraints, not findings.** These are not reopened: DECISIONS (the frozen baseline, no cut of the Tiro day, the
adoption of DF-003 and DF-005 to DF-009, all Unreal work on Wolf, the D-G4 and `Build.cs` rulings), the accepted
MOONSHOT items and their cards, and the overlay-only rule. Brief goal 3 now reads "baseline unchanged (check-pin green,
45 vectors byte-identical) and the VR overlay passes its checks" (DECISIONS 2026-10-21). This review measures the code
against that sentence.

## 0. What the ruling changes structurally

Before 2026-10-21, the overlay was a staging area. A rule lived in `apps/vr/data/vr/` and in `if (Rules ...)` branches
"until TV accepts" it, and a re-pin would then move it into pinned data and the 45 vectors. The ruling removes the way
out. Three consequences:

1. **The null path is permanent and well guarded.** `check-pin.mjs` hashes the nine pinned files, and the 45
   `MageArena.Kernel.Conformance.*` vectors replay the C++ port with `Rules == nullptr`.
2. **The overlay path is now the whole VR product, and it is the half with no recorded behaviour.** All of the
   game's change from here to 18 Nov (T39's six DF adoptions first) goes through it. Section 1 shows that no test
   records a live-overlay run's outcome, and that the overlay has no identity (no hash) anywhere.
3. **Tooling and cards written around a re-pin are stale.** Examples are T33's PinDiff census, M8's "contract" gate,
   the `.ai` manifest's "changes only through a re-pin task", and the "until the TV change request lands" text inside
   `combat.vr.json` itself (section 3).

## 1. How the overlay rules are guarded

### 1.1 Where the overlay enters the kernel

The overlay is one `FVrRuleset*` handed to `StepArena` (`Kernel/ArenaKernel.cpp:457`). The null overload (`:547-549`) is
the baseline path. 44 lines in eight kernel files branch on it (command 7):

| File | Overlay branch lines | Of which check `bActive` | Owner of the file's rules |
|---|---:|---:|---|
| `Kernel/ArenaKernel.cpp` | 15 | 3 | pinned port |
| `Kernel/ArenaThreats.cpp` | 10 | 1 | pinned port |
| `Kernel/Enemies.cpp` | 8 | 5 | pinned port |
| `Kernel/Games.cpp` | 3 | 1 | pinned port |
| `Kernel/Air.cpp` | 3 | 0 | VR-owned school (CR-007) |
| `Kernel/Fire.cpp` | 2 | 0 | pinned port |
| `Kernel/MageAI.cpp` | 2 | 1 | pinned port |
| `Kernel/Water.cpp` | 1 | 0 | pinned port |
| **Total** | **44** | **11** | |

There are two shapes:

- **Hooks**, which are good: one named `FVrRuleset` method called at a fixed point of `StepArena`. These are
  `TickSplitLatch` (`ArenaKernel.cpp:506`, defined at `VrRules.cpp:465`), `TickStaff` and `TickWalls` (`:510-511`),
  `TryGrantedCast` (`:519`), `BurnCrossers` (`:528`), `TickRivalPhases` (`:536`, defined at `VrRivals.cpp:60`) and,
  since `03bcc92`, `PlayerPowerMult` (`:173`, `:290`, `:299`).
- **Inline branches** inside pinned-port functions, for example `Enemies.cpp:50`, `:105`, `:255`, `:358`, `:415` and
  `:452` (the spit and the pressure knobs) and `ArenaThreats.cpp:168`, `:207`, `:318`, `:325`, `:379`, `:451` and `:456`.

The "is the overlay on" test is spelled two ways: `Rules` (33 lines) and `Rules && Rules->bActive` (11 lines).
`bActive` only means "the loader succeeded" (`VrRulesLoad.cpp:1082`, cleared at `:1097`), so the two agree in the
shipped session. They differ only for a hand-built `FVrRuleset` in a test, which defaults to `bActive = false`
(`VrRules.h:191`).

### 1.2 What guards each overlay rule

"Unit" means behaviour tests with literals. Most of them load the live overlay through `LoadVrRuleset` (command 9).

| Rule (card, CR) | Code | Unit tests | Live-overlay golden or digest | Gap |
|---|---|---|---|---|
| Air school (T23, CR-007) | `Air.cpp`, `MageAI.cpp:367-381`, rows in `data/vr/schools/spells-air.csv` | 12 `MageArena.Air.*` (`AirTests.cpp:173-1104`), among them `NothingElseMoves` | **none** | `Duel.AirSeated` is red. DF-008 rewrites Lio's AI with only `Air.Mind` and a census as evidence. The air rows are kept out of `PinHash` on purpose (`KernelData.h:434`) and enter no other hash |
| Ember spit (T18, CR-004) | `Enemies.cpp:358` (throw lookup), the `attacks` block of `combat.vr.json:59-92` | 5 `MageArena.Creatures.*` (`CreatureTests.cpp:124-425`), `Colour.HoundEmber` | **none** | `Session.CreaturesSeated` is red. DF-007 B changes the cadence |
| Split latch (T17) | `VrRules.cpp:453-470` via the `TickSplitLatch` hook | 7 `MageArena.Defences.Split*` (`DefenceTests.cpp:395-735`) | **none** | DF-005 adds re-arm-on-clear here. That is a good fit (it is a hook) |
| Collar (T22, CR-006) | `Session/CollarLedger.cpp`, `ArenaThreats.cpp:67` | 8 `MageArena.Collar.*` (`CollarTests.cpp:408-818`), with `NullPathSilent` | not applicable (events only) | DF-009 is data in `collar.json`, and `Collar.MissedCasts` pins it |
| Rival phases (T19, CR-005) | `VrRivals.cpp` via the `TickRivalPhases` hook | 7 `MageArena.Rivals.*` (`RivalTests.cpp:259-617`) | **null path only**: the MD5 at `RivalTests.cpp:31-34` is the duel without phases | DF-006 B adds a beat. `Duel.FireSeated` is red |
| Fire wall, staff, power (T11, T12) | hooks and `ArenaThreats.cpp:258` | 9 `Defences.FireWall*`, `PlayerPower.*` (`356ac74`) | **none** | DF-003 `wide` changes every number these tests hard-code (1.4) |

### 1.3 The finding: no live-overlay run has a recorded outcome

- `MageArena.Fire.Golden` (`FireTests.cpp:1169-1255`) is the only end-state golden outside the 45 vectors, and it runs
  with **no ruleset**. `RunDuel` calls `TryCreateGames(Games, Seed, Preset, Wave, true, true)` (`FireTests.cpp:448`),
  and the `Rules` parameter defaults to `nullptr` (`Games.h:66`). Its two files in `apps/vr/data/scenarios-vr/` are
  therefore baseline goldens, even though the folder is named "-vr".
- The rival digest is null-path by design (`RivalTests.cpp:31-34`, `:628`).
- `MageArena.Replay.Deterministic` (`ReplayTests.cpp:73-77`) runs the live overlay, but it compares two runs with each
  other, not with a recorded result.
- MOONSHOT M8 counted "two `Fire.Golden` end-state summaries" as overlay guards (`MOONSHOT-2026-10.md:662`). That
  overstated it: the number of live-overlay goldens is **0**.
- The only whole-bout check on the overlay path is the `MageArenaDesign` suite. Five of its tests are already red
  (CLAUDE.md, "Build"). A sixth regression inside one of those five bouts would not change the colour of anything.

## 2. The findings, ranked

Each finding has a severity, file:line evidence and a fix sized as a task card. Lynx is the Node, docs and orchestration
host. Wolf is the x64 UE host (DECISIONS 2026-10-20). None of the cards is written into `apps/vr/tasks/`. This file only
proposes them.

### A1. T39 changes six overlay rules, and nothing records what the overlay did before it. Severity: high. Before 10-31.

- **Evidence.** Section 1.3: there are 0 live-overlay goldens. T39 (`apps/vr/tasks/T39-...md:25-30`) changes DF-003,
  DF-005, DF-006, DF-007, DF-008 and DF-009 in one card. Its acceptance (`:47-57`) is one `MageArenaDesign` run, which
  is already red in five places, plus the 45 null-path vectors. The 45 vectors cannot see any of the six changes.
- **Cost.** T39 is the gate's critical path, and T41's playtests wait on it (`T41-...md` Status). Suppose a DF commit
  also moves a bout it did not mean to touch, for example DF-003's `wide` fire wall changing the creature bout. Before
  31 Oct it then shows up only as a census number drifting inside an already red suite, or in a playtest.
- **Fix (card, Wolf, size M): "Overlay end-state goldens, recorded before T39".** It is the cheap subset of MOONSHOT
  M8: end states, not tick vectors.
  - Add `MageArena.Overlay.Golden`. It runs about five fixed-seed bouts through `TryCreateGames(..., &Rules, bAirFinal)`
    with the **live** overlay: wave 1 with split hands, the creature bout (spit), Brennic c1 (phases), Lio c1 (Air
    final), and a fire-wall duel.
  - Each bout writes the `Fire.Golden` summary (hash, ticks, outcome, hp, mana) plus event counts by kind, into
    `apps/vr/data/scenarios-vr/overlay/*.json`.
  - Each file records the sha256 of `combat.vr.json`, `collar.json` and `schools/spells-air.csv`, and whether
    `-MageArenaProposal` was set.
  - Seen red first: the test fails with "overlay changed since recording" when one byte of `combat.vr.json` changes.
  - Record it on today's overlay **before** T39's first commit. Then each `T39: DF-00x` commit re-records the goldens,
    so its diff shows exactly which bouts the DF moved, and the report explains every moved file.

### A2. The "45 vectors byte-identical" half of goal 3 is checked by nothing. Severity: high. Before 10-31.

- **Evidence.**
  - `check-pin.mjs` hashes only the nine files in `PINNED.json` (`checkTree`, `:108-136`). It never names
    `conformance` (command 11: 0 hits). `apps/vr/data/conformance/` holds 45 files and has no manifest.
  - The UE test compares values at a relative tolerance of 1e-6 and an absolute one of 1e-9
    (`KernelConformanceTests.cpp:29-30`, `:53`), not bytes.
  - The merge gate (`.github/workflows/quality.yml:22`, the same as this run's test gate) runs only check-pin.
  - The only byte check is `tools/conformance/generate.mjs`. It needs a TV clone and `tsx` (`generate.mjs:23-27`;
    BACKLOG `:27`), so it is not a gate.
  - The repo's own boundary lists disagree. `.ai/manifest.yaml:39` lists `apps/vr/data/conformance/` under
    `neverTouch`, while `.ai/guardrails.yaml:10` is `neverTouch: []`, although its comment says "keep in sync with the
    manifest". The brief's boundary list for builders names only `apps/vr/data/pinned/**`.
- **Cost.** A commit that edits a vector, for example to make a red conformance case pass, gets through every Lynx gate.
  The UE suite would accept any edit within 1e-6. That breaks the exact sentence the owner ruled on, unseen.
- **Fix (card, Lynx, size S): "Hash the 45 vectors into the pin gate".**
  - Add a `conformance` array (path, sha256, bytes) to a new `apps/vr/data/CONFORMANCE.json`, generated once from the
    tracked files.
  - Teach `check-pin.mjs` to verify it next to `PINNED.json`, with a `--self-test` that flips one vector byte. It needs
    no workflow change, because the gate already runs check-pin.
  - Set `.ai/guardrails.yaml` `neverTouch` to the manifest's list.
  - Seen red first: the self-test fails before the check exists.

### A3. The overlay has no identity, and "the overlay passes its checks" has no definition on Lynx. Severity: high. Before 10-31.

- **Evidence.**
  - The boot log prints only the pin hash (`KernelData.cpp:719`). The air rows are kept out of it on purpose
    (`KernelData.h:434`).
  - No overlay file enters any hash. Census writers record seeds and no data hash (MOONSHOT M6 evidence,
    `MOONSHOT-2026-10.md:562`; unchanged).
  - The variant can come from an environment variable as well as the command line (`VrRulesLoad.cpp:658-662`), so the
    log does not say whether a run was "live" or "proposal".
  - No Node tool reads `apps/vr/data/vr/` (command 12: 0 hits in `apps/vr/tools`, `tools`, `.github`). A malformed
    `combat.vr.json` passes every Lynx gate and fails only the strict loader on Wolf (`VrRulesLoad.cpp`, `LoadVrRuleset`).
- **Cost.** Every number the owner decides on from now on (T39's re-measures, T41's playtests) is a function of overlay
  bytes that no output names. Two census runs that disagree cannot be told apart from an overlay edit. A JSON typo in a
  Lynx docs or data commit costs one Wolf run to find.
- **Fix: two cards.**
  - **(a) Lynx, size S: `apps/vr/tools/check-overlay.mjs`.** Every `.json` under `apps/vr/data/vr/` parses. Every CSV
    header under `schools/` matches the column list the loader requires. Print one overlay sha256 over the sorted file
    list. Add `--self-test`, which must fail on a truncated JSON. Add it to the test gate. This gives "passes its
    checks" a meaning on Lynx without copying the C++ loader's rules.
  - **(b) Wolf, size S, folded into A1's card:** log `overlay sha256` and the variant (live or proposal) next to the pin
    hash at boot (`KernelData.cpp:719`, or wherever `LoadVrRuleset` succeeds), and in the census and design CSV headers.

### A4. T39's file table has three premise errors. Severity: medium-high. Before T39 starts.

- **Evidence.**
  1. **The goldens it lists are null-path.** The DF-003 row lists `apps/vr/data/scenarios-vr/fire-duel-competence-1.json`
     and `-1-5.json` as files the change touches (`T39-...md:25`). They are recorded with no ruleset (1.3), so the
     `wide` knobs cannot move them. A builder who expects them to move, or "re-records" them, gets confused or edits a
     baseline golden.
  2. **Tests that hard-code live overlay numbers are not listed.** For example:
     - `Defences.FireWallDuration` asserts `UntilTick == 181` from the live `durationS` 3 (`DefenceTests.cpp:1518-1520`;
       `combat.vr.json:121`), and `wide` sets 12 s (`calibration-proposal.json:38`).
     - `:1411-1412` and `:1495` pin a mana cost of 12 and a burn of 12 from the live file.
     - 29 comments in 8 test files cite `combat.vr.json` values (command 10).

     The T39 table names `DefenceTests.cpp` only for DF-005.
  3. **It puts two rules in pinned-port files although hooks exist.** It places DF-006 B (the phase-break beat) in
     `Kernel/Enemies.cpp`, and DF-008 in `Enemies.cpp` and `Games.cpp` (`T39-...md:27`, `:29`). The phase machine is the
     `TickRivalPhases` hook (`VrRivals.cpp:60`, called at `ArenaKernel.cpp:536`), and `Enemies.cpp` already has 8
     inline overlay branches.
- **Cost.** One wasted Wolf run on T39 is about a day out of the ten before the gate. Placing it as written adds more
  inline branches to the files only the null-path vectors guard.
- **Fix (card, Lynx, size S, docs only): "Amend T39 before Wolf picks it up".**
  - Drop the two `scenarios-vr` files from the DF-003 row and point the builder to A1's goldens.
  - List the tests that read live overlay values, from the `combat.vr.json` comments in `DefenceTests`, `CreatureTests`,
    `RivalTests`, `DayTests`, `CollarTests` and `SessionTests`, as files each DF commit may update, with the rule that
    each literal change cites the overlay field.
  - Steer DF-005 to `TickSplitLatch` and DF-006 B to `VrRivals.cpp` / `TickRivalPhases`, and DF-008 to a named
    `FVrRuleset` or `MageAI` hook where it can (otherwise the builder records why not).
  - Let the DF-003 commit also replace the stale "until the TV change request lands" text in `combat.vr.json:68` and
    `:134` (3.1), since it edits that file anyway.

  The orchestrator owns task cards (`docs/ORCHESTRATION.md`), so this review does not edit T39.

### A5. T33 (M6) is half moot, its write set is stale, and it collides with T39. Severity: medium. Before 10-31 (sequencing only).

- **Evidence.**
  - T33's title and goal are "a pin-diff census for re-pins" (`T33-...md:1`, `:7`). Its spec's first use case is
    "Re-pin safety ... when TV accepts one and the VR re-pins" (`MOONSHOT-2026-10.md:474`). The ruling says "nobody
    re-pins".
  - The 16 Oct P1 is still open. M6's write set (`MOONSHOT-2026-10.md:513-543`) does not list
    `Kernel/KernelDataAir.cpp` or `Kernel/KernelDataHeat.cpp`, which call the singleton at `:13` and `:682`.
  - T33 to T36 still list three known reds (`T33-...md:27`, `T34:28`, `T35:28`, `T36:29`). CLAUDE.md lists five.
  - T33's codemod touches every `Kernel/` file, among them `Enemies.cpp`, `Games.cpp` and `VrRules`. Those are T39's
    files.
- **Cost.** If T33 runs before or beside T39 on Wolf, one of them rebases by hand through the kernel in the gate week.
  A T33 worker following its own card fails acceptance 3 or leaves its write set (16 Oct P1).
- **Fix (card, Lynx, size S, docs only): "Re-sequence and re-scope T33".**
  - Put T39 before T33 in the run order.
  - Correct the write set and the known-red list in T33 to T36.
  - Mark the PinDiff census as moot under DECISIONS 2026-10-21. Keep the injection seam and the "census records the
    data hashes" half, which A3(b) partly delivers.
  - Dropping part of an owner-accepted MOONSHOT item is a scope change, so it goes to the owner as an ask (see
    questions). Until then the card only re-sequences.

### Waiting: everything else, and why

These are open, but each one either does not compete with gate work or is held by another lane:

- **A6. The overlay predicate has two spellings** (1.1: 33 `Rules`, 11 `Rules && Rules->bActive`). Severity: low. The
  two agree in the session. A6 needs nothing before 18 Nov beyond A4's rule that new rules enter through hooks. A
  unifying `IsOverlayOn(Rules)` would touch 44 lines in T39's and T33's files during the gate week. It waits until after
  T33. Card: Wolf, S.
- **A7. Tests that pin the live overlay's numbers.** Severity: medium, at V1. Under the ruling, tuning the overlay is the
  only change channel, and each tuning edits literals in files the tuning card did not plan (A4.2). The fix is that rule
  mechanics tests load a fixture overlay through `SetVrDataDirForTest`, which five test files already use (command 13).
  One `Overlay.LiveValues` test then pins the live file. It waits because A4's list covers T39. Card: Wolf, M.
- **Stale ruling text** (3.1). These are docs and data strings. A4 covers the two in `combat.vr.json`. The rest is one
  Lynx docs card, size S, at any time.
- **The 16 Oct findings still open** (section 4): F2/P2, N1/P5, N2, N3, N4 and N5 are all in session or loader files.
  The session files have not been touched since `b7f1009` (command 5: 0 commits), and T16, T35 and T36 hold them. They
  wait for those cards, as the 16 Oct review already ruled. F3 waits for T35 (M3) and the V1 device bring-up.
- **P4 remainder, 9 clang-only sites** (section 4). Severity: medium, at V1. They cost nothing before the first Android
  compile (V1, from 2026-11-01). Six are in files no gate-week card holds (`KernelDataHeat.cpp`, `DayFlow.cpp`) or that
  T39 may touch (`VrRulesLoad.cpp`). Card: Wolf, S, before the first APK, after T39.

## 3. Boundaries the ruling made stale

### 3.1 In code, data and docs (command 14)

| Where | Text | Status |
|---|---|---|
| `apps/vr/data/vr/combat.vr.json:68` | "A VR overlay attack until the TV change request lands." | stale; part of A4 |
| `apps/vr/data/vr/combat.vr.json:134` | "A VR overlay rule until the TV change request (CR-005) lands." | stale; part of A4 |
| `.ai/manifest.yaml:37` | "Pinned data changes only through a re-pin task" | stale: no re-pin (DECISIONS 2026-10-21) |
| `.ai/guardrails.yaml:10` | `neverTouch: []` | out of sync with the manifest; A2 |
| `docs/schools/AIR.md:3` | "VR-owned until the TV channel accepts it (CR-007)" | stale |
| `docs/design/V2-ROADMAP.md:100` | "Numbers stay overlay-only until TV accepts." | stale |
| `docs/change-requests/CR-006-collar-ledger.md:114` | "say whether TV wants VR vectors recorded before it re-pins" | stale open question |
| `docs/change-requests/CR-007-air-school.md:16` | "the reference for a new school until the TV channel ports it" | quotes the 2026-10-03 ruling; the "until" no longer happens |
| `docs/research/MOONSHOT-2026-10.md:683` | M8 "Gate: contract ... needs the TV repo's agreement" | moot: M8 is now VR-internal, and A1 is its cheap subset |
| `apps/vr/tasks/T33-...md:1`, `:7` | the PinDiff census for re-pins | A5 |
| `apps/vr/tasks/T18`, `T19` | "the TV change request ... is written by the orchestrator" | done cards, historical; leave them |

### 3.2 In tooling

- **`check-pin.mjs --source`** byte-compares the pin with a TV clone (`:93-136`). It is still correct as a provenance
  check and harmless. Nothing needs to change.
- **`tools/conformance/generate.mjs`** regenerates the 45 vectors from TV. Under a frozen baseline the vectors never
  regenerate, so it becomes an archival proof that needs a TV clone and `tsx`. That is why A2 moves byte-identity into a
  hash manifest instead.
- **No overlay tool exists** (A3a). The ruling made the overlay the only place VR change happens, and the tooling
  still treats it as plain files.

## 4. The 16 Oct review's findings, one by one

| Finding (16 Oct, section 7) | Status at `3d8e862` | Evidence |
|---|---|---|
| P1: T33's write set predates sweep 6 | **still open**, and part of A5 | M6's write set has no `KernelDataAir.cpp` or `KernelDataHeat.cpp` (`MOONSHOT-2026-10.md:513-543`). The singleton calls are at `KernelDataAir.cpp:13` and `KernelDataHeat.cpp:682`. `T33-...md` last changed in `e9f99db` |
| P2: `ArenaSession.cpp` is #1 and in six write sets | **still open**, unchanged | 2298 lines, 0 commits since `b7f1009` (commands 4 and 5). T16 is still `open` |
| P3 / F1: the power rule written three times | **fixed** | `03bcc92` adds `FVrRuleset::PlayerPowerMult` (`VrRules.cpp:412`, `VrRules.h:259`), called at `ArenaKernel.cpp:173`, `:290` and `:299`. The split path keeps `bRequireActive = false` as the review asked (`VrRules.h:258`). `356ac74` adds `PlayerPower.Method` and `PlayerPower.Paths` (`PlayerPowerTests.cpp:78`, `:113`). The broader F1 rule ("new rules enter as hooks") is still not written into any card, and A4 applies it to T39 |
| P4: 20 clang-only error sites | **partly fixed: 11 of 20 done, 9 left** | `b08d6fd` fixed six `TPair` loops (`ArenaAudio`, `ArenaFlags`, `CollarLedger`, `DayTests`). `5be3e2f` fixed the five capture-driver `TActorIterator` returns. Left: `TPair` at `KernelDataHeat.cpp:394`, `:486`, `:507`, `VrRulesLoad.cpp:357`, `:401` and `DayFlow.cpp:71`, plus the three `break` loops at `ArenaSession.cpp:2237`, `:2274` and `SessionPresentation.cpp:1095` (command 6) |
| P5 / N1: ten capture drivers | **still open** | 10 `Session/*Capture*.cpp` (command 4). No `UCaptureDriver` |
| F2: `FArenaSession` | **still open** | The bout-start block is still duplicated at `ArenaSession.cpp:256` and `SessionFlow.cpp:313` (command 8) |
| F3: no live hand-source slot | **still open** | One `IHandSource` (`Hands/HandClipPlayer.h:16`). `Build.cs` has no `OculusXR`; its two `bBuildEditor` blocks are at `:13` and `:38` and stay apart (DECISIONS 2026-10-16). 40 files reference `UHandInputSubsystem` |
| F4: `TPair` loops | **still open**, as the P4 remainder | above |
| F5: external-linkage helpers | **done** (since `48759c5`) | not re-checked; no commit touched `VrRulesLoad.cpp` since `b7f1009` |
| N2: `ASessionPresentation::Sync` | **still open**, unchanged | `SessionPresentation.cpp` 2306 lines, 0 commits since `b7f1009` |
| N3: the reference script in the session | **still open** | No `Session/ScriptedPlayer.cpp` (command 4). It is the first commit of T36. T39's DF-007 A edits the same script (`T39-...md:28`) |
| N4: `VrRulesLoad.cpp` | **still open** | 1102 lines, 0 commits since `b7f1009`. No `Kernel/VrRivalsLoad.cpp`. T39's DF-005 edits it |
| N5: four `Fail(FString&)` sinks | **still open** | `AbsorbResolver.cpp:11`, `ArenaLayout.cpp:11`, `VrRulesLoad.cpp:35`, `PlayerPreset.cpp:13` |
| 2.2 commit D: split `LoadVrRuleset` | **still open**, under N4 | as N4 |
| 7.4: the asset footprint is unmeasured since `4765085` | **still open**, not structural | not re-run |
| **Moot under the ruling** | M6's PinDiff half (A5), M8's contract gate and its "move vectors to `conformance/` on re-pin" step (3.1), and CR-006's question 6 | DECISIONS 2026-10-21 |

## 5. What this run did not do

- No build, no editor and no automation run. Every claim about behaviour comes from reading code. Section 1.2's "none"
  means no test with a recorded live-overlay outcome was found by the greps in section 6, not that a run showed one is
  missing.
- It did not count every test literal that `wide` changes (A4.2 gives examples and a way to find them). That count is
  the A4 card's work.
- It edited nothing outside this file: no card, no tool, no data.

## 6. Commands run, with exit codes

Run on 2026-10-21 in this worktree at `3d8e862`, from the repo root unless the path says otherwise. For a pipeline, the
exit code is the last stage's.

| # | Command | Exit | Result |
|---:|---|---:|---|
| 1 | `git log --oneline b7f1009..HEAD \| wc -l` | 0 | 59 |
| 2 | `git log --oneline --no-merges b7f1009..HEAD -- apps/vr/Game/Source` | 0 | 11 commits, among them `03bcc92`, `356ac74`, `b08d6fd`, `5be3e2f` |
| 3 | `git show --stat 03bcc92 356ac74 b08d6fd 5be3e2f` | 0 | the files in section 4 |
| 4 | `wc -l $S/Session/{ArenaSession.cpp,ArenaSession.h,SessionFlow.cpp,DayFlow.cpp,SessionPresentation.cpp} $S/Kernel/{VrRulesLoad,VrRules,KernelData}.cpp`; `ls $S/Session/*Capture*.cpp \| wc -l`; `ls $S/Session/ScriptedPlayer.cpp $S/Kernel/VrRivalsLoad.cpp` | 0; 0; 2 | 2298, 597, 1636, 614, 2306, 1102, 570, 747; 10; neither exists |
| 5 | `git log --oneline b7f1009..HEAD -- $S/Session/<f>` for the four session files; the same for `Kernel/VrRulesLoad.cpp` and `Hands/HandInputSubsystem.cpp` | 0 | 0 commits each |
| 6 | `git grep -n 'const TPair<FString, TSharedPtr<FJsonValue>>&' -- $S`; `grep -n -A3 'for (TActorIterator' $S/Session/ArenaSession.cpp $S/Session/SessionPresentation.cpp \| grep break` | 0 | 6 loops; 3 `break` loops |
| 7 | in `$S/Kernel`: `grep -c 'if (Rules\|if (!Rules\|Rules &&\|Rules ?' <f>.cpp` and `grep -c Rules`; the same piped to `grep -c bActive` | 0 | the table in 1.1; 44 total, 11 with `bActive` |
| 8 | `grep -n 'LoadVrRuleset(' $S/Session/*.cpp $S/Hands/*.cpp` | 0 | `ArenaSession.cpp:256`, `SessionFlow.cpp:313`, `MageSettings.cpp:366` |
| 9 | `grep -noE 'AUTOMATION_TEST\([A-Za-z0-9_]+, "[^"]+"' $S/Tests/*.cpp`; per test file `grep -c 'LoadVrRuleset('` | 0 | the test names in 1.2; `LoadVrRuleset` in Air 2, Collar 1, Creature 2, Defence 4, Rival 4, VrOverlay 3, Day 2, Session 2, Fire 0 |
| 10 | `grep -c 'combat.vr.json' $S/Tests/*.cpp` (non-zero only, summed) | 0 | 29 in 8 files |
| 11 | `grep -n conformance apps/vr/tools/check-pin.mjs` | 1 | 0 hits |
| 12 | `grep -rn 'data/vr' .github tools apps/vr/tools --include=*.mjs --include=*.yml --include=*.ps1` | 1 | 0 hits |
| 13 | `grep -rln SetVrDataDirForTest $S/Tests` | 0 | 5 files: Creature, Day, Defence, Rival, Settings |
| 14 | `grep -rnoiE '.{0,60}(until (the )?TV\|TV accepts\|TV change request\|re-?pin\|change request lands\|when TV).{0,40}' apps/vr/data/vr apps/vr/Game/Source apps/vr/tools apps/vr/tasks docs/change-requests docs/design docs/schools` | 0 | the rows in 3.1; 0 hits in `Source` |
| 15 | `sed -n 425,470p $S/Tests/FireTests.cpp`; `grep -n TryCreateGames $S/Kernel/Games.h` | 0 | `RunDuel` passes no ruleset (`:448`); `Games.h:66` defaults `Rules = nullptr` |
| 16 | `grep -nE 'fireWall(DurationS\|ManaCost\|ArcDeg)' apps/vr/data/vr/calibration-proposal.json`; `grep -n -A12 '"fireWall"' apps/vr/data/vr/combat.vr.json` | 0 | `wide`: 12 s, 6 mana, 150 deg (`:36-39`); live: 3 s, 12 mana, 90 deg (`:119-122`) |
| 17 | `grep -n 'bActive = ' $S/Kernel/VrRulesLoad.cpp`; `sed -n 185,215p $S/Kernel/VrRules.h` | 0 | `:1082` true, `:1097` false; `bActive = false` at `VrRules.h:191` |
| 18 | `grep -n 'Known red\|known red' apps/vr/tasks/T3[3-6]*.md` | 0 | three tests named in each of T33 to T36 |
| 19 | `grep -n -A6 boundaries .ai/manifest.yaml`; `sed -n 1,25p .ai/guardrails.yaml` | 0 | `:37-39`; `neverTouch: []` at `:10` |
| 20 | `node apps/vr/tools/check-pin.mjs`; `node apps/vr/tools/check-pin.mjs --self-test` | 0; 0 | `pin OK: 9 files @ baeac66`; `corruption detected: ...combat.json` |
