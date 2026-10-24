# T47 - Split the design suites' invariants from their measurements

Status: **done** on Wolf (kaziho) 2026-10-24, UE 5.8.3 CL 58210709, commits `69b349a` (step 1), `086854e` (step 2),
`f64c685` (step 3); see "Result" at the end. `Result: Succeeded`; `MageArena.Invariants` 5/5; `MageArena.` 266/266
(Conformance 45/45); `MageArenaDesign.` 5/7 (FireSeated and FullSeated red as DF-011's accepted reds); self-test
verdicts identical. Was: ready for Wolf (the x64 UE host) **after T39 has landed** - see "Order against T39", which is a hard
precondition, not a preference. Written 2026-10-21 on Lynx (ARM64, no Unreal install, `docs/DECISIONS.md` 2026-10-20
"Unreal host"): nothing in this card was built or run, and every line number below was read from the source on
2026-10-21 at `66645cc`.
Max turns: 200

## Goal

The five `MageArenaDesign` suites each bundle real invariants and open measurements into one `bPass`, so while a suite
is red a **regression in any bundled invariant looks identical to the known failure**. After this card each scenario has
a **green invariants test** that asserts only what must always hold, and every band, window, median and win rate is a
**reported number** that cannot fail a gate. The gate that remains is strictly stronger than today's, because the
invariants fail on their own instead of hiding behind a number nobody expects to go green. Nothing is tuned, no
threshold is loosened, and no number moves: that is T39's job and the owner's.

The one invariant the repo does not have at all today is **gestures reaching the kernel**. The only positive
gesture-to-kernel assertions anywhere are `SessionTests.cpp:179-182` inside the red `Session.Wave1Seated`
(`gesture ward`, `gesture sigil`, `kernel roll actor=`, `kernel cast`). Verified 2026-10-21: every other chain
assertion in the tree is negative (`Tests/PresetTests.cpp:503-506`, "the reach is not a ward / blink / sigil"), and the
one green perfect-absorb test, `MageArena.Session.DeathBurstPerfect`, drives the detector delegate directly
(`SessionTests.cpp:811`, `:852`: `Rig.Wards->OnWardRaised.Broadcast(...)`) rather than playing a clip - so it proves
the kernel rule, not the pipeline. **There is no green test in this repo asserting that a played hand clip produces a
kernel effect at session level.** This card delivers the first one.

## Read first

- `CLAUDE.md`, `docs/DECISIONS.md` newest first (they outrank this card), in particular 2026-10-16 (two of the five
  reds ruled not-regressions) and 2026-10-21 "Deadlines and Q9" (18 Nov is the only hard date)
- `.ai/memory/0003-design-tests-red-by-design.md` - never tune, loosen or skip to turn a design test green. This card
  obeys it by **separating** the two kinds of assertion, not by weakening either.
- `apps/vr/Game/Source/MageArenaVR/Tests/SessionTests.cpp` - the file this card reshapes, all five
  `MageArenaDesign` tests: `FMageArenaWave1Scripted` (`:94`), `FMageArenaCreaturesSeated` (`:189`),
  `FMageArenaFireSeated` (`:251`), `FMageArenaAirSeated` (`:362`), `FMageArenaFullSeated` (`:658`)
- `apps/vr/Game/Source/MageArenaVR/Tests/RivalCensusTests.cpp` (`MageArenaDesign.Census.Rivals` at `:341`, 20 seeds per
  set, `-MageArenaCensusSeeds=` at `:397-399`) and `Tests/CensusTests.cpp` (`MageArenaDesign.Census.Sweep` at `:677`).
  **The census harness already exists**; this card reuses it and writes no new player model.
- `docs/design-findings/DF-006-phases-shorten-the-duel.md`, `DF-007-...`, `DF-008-...` (the measured medians and win
  rates), and **`docs/design-findings/DF-010-tiro-day-length.md`** (**ruled 2026-10-21, E + B** - it settles one line of
  this card; see "The day-length assertion")
- `apps/vr/tasks/T39-adopt-df-recommendations-and-rerun-design.md`
- `docs/concepts/WOLF-PLAN.md` sections c (P0-P2) and d (the gate commands and their expected output)
- `docs/ORCHESTRATION.md` - "a claim with no command behind it is not accepted"

## Order against T39 (hard; the two touch the same file)

**T39 lands first, in full, and this card starts from T39's recorded result.** Three reasons, all checkable:

1. T39's acceptance is "each of the five known failures ends green, or stays red and is explained by an accepted DF"
   (its Goal). That clause is written against the suite **as it is shaped today**. Reshaping first destroys the only
   measurement the owner ordered: DECISIONS 2026-10-20 says "card one design-suite re-run", singular.
2. T39 edits literals in this same file - its "Test files each DF commit may update" section names
   `SessionTests.cpp` explicitly. A T47 rewrite in flight means a hand-merge of two rewrites of one 870-line file.
3. Some invariants are only green after T39. `Session.Wave1Seated`'s `phase == intermission` (`:173`) and
   `waves cleared >= 1` (`:174`) are failing today because the reference script **loses** Wave 1 (DF-001, lost at
   23.25 s). Those are genuine invariant failures, not measurements, and this card must not hide them.

Before the first `T47:` commit, prove T39 is in:

```powershell
git log --oneline -15 -- apps/vr/Game/Source/MageArenaVR/Tests/SessionTests.cpp   # expected: the T39: DF-00x commits present
git log --oneline -1 apps/vr/tasks/T39-adopt-df-recommendations-and-rerun-design.md
```

If any `T39: DF-00x` commit is missing, **stop and report**. Record T39's final per-test `MageArenaDesign` table as
this card's before-table; every after-number is compared against it per test, as WOLF-PLAN section d requires.

**Proof, Wolf kaziho 2026-10-24.** T39 landed in full: `git log --oneline --grep="^T39"` lists `5b13463` DF-003,
`0acf0c2` DF-009, `a64eab5` DF-007 B, `bef9323` DF-005, `0f33106` DF-006 B, `7f4926d` DF-007 A, `6f00555` DF-008 (no T39
commit touched `SessionTests.cpp`, so the file log shows none; the card's first command is answered by the grep). Its
three remaining reds (FireSeated, AirSeated, FullSeated) are explained by an accepted DF: the owner ruled DF-011 on
2026-10-24, "Accept the reds" (`docs/DECISIONS.md` 2026-10-24), so P2 counts as landed and this precondition holds.

## What is an invariant and what is a measurement

Read from the source on 2026-10-21. "Gate" = stays in `bPass`, in the new green invariants test. "Report" = moves out
of `bPass` into a logged measurement. **Where a condition is genuinely a gate it stays a gate**; this table is not a
list of things to relax.

| Test | Line | Condition | Verdict |
|---|---|---|---|
| Wave1Seated | `:173` `:174` | phase `intermission`; waves cleared >= 1 | **Gate** (outcome; see "outcome-independent" below) |
| Wave1Seated | `:175-176` | victory within the 25-40 s pinned window (`PinnedWindowLo/Hi`, `:116-117`) | Report |
| Wave1Seated | `:177` | the player stayed on a pad, worst < 1e-3 m | **Gate** |
| Wave1Seated | `:178-182` | blinked; `gesture ward`; `gesture sigil`; `kernel roll actor=`; `kernel cast` | **Gate - the one this card is for** |
| Wave1Seated | `:183` | exactly one `Session victory` chain line | **Gate** (outcome) |
| CreaturesSeated | `:243` | won bout 2 | **Gate** (outcome) |
| CreaturesSeated | `:244-245` | victory inside the 30-45 s window (`:208-209`) | Report. The window is pinned `combat.json pacingTargets.creatureWaveS` and the comment at `:207` says "Do not move the window to make a bout pass" - so it is **reported, not moved**, while DF-007 + T39 retune the bout |
| FireSeated | `:277` | `combat.vr.json` names a rival | **Gate** |
| FireSeated | `:343` | Brennic is bound | **Gate** |
| FireSeated | `:344` | duel length in 45-80 s (`Lo`/`Hi`, `:265-266`) | Report, as a **population median** |
| FireSeated | `:345` `:346` | reached the last phase; the signature was cast in it | **Gate**, as a population rate: DF-006 and DF-008 measure signature 100 % in every set, so it is met everywhere and is a real invariant |
| FireSeated | `:347` | stayed on a pad | **Gate** |
| FireSeated | `:350` | the player wins at competence 1 | Report, as a **population win rate** |
| AirSeated | `:479` `:480` | Lio bound; Lio is the air mage (`Lio->Air.bSchool`) | **Gate** |
| AirSeated | `:481` | length in 45-80 s (`:375-376`) | Report (median) |
| AirSeated | `:482` `:483` | reached the last phase; signature cast | **Gate** |
| AirSeated | `:484` | stayed on a pad | **Gate** |
| AirSeated | `:485` | the player wins | Report (win rate) |
| FullSeated | `:788` | did not stop in an infinite loop (`Guard < 72*1800`) | **Gate** |
| FullSeated | `:789` | the script did not lose a bout | **Gate** |
| FullSeated | `:790` `:791` | the day closes; all four bouts won | **Gate** |
| FullSeated | `:792` | the day with the teach is in the plan's 7-10 min (`PlanLoS`/`PlanHiS`, `:671-672`) | **Measurement** - DF-010 ruled E; see below |

Two conditions in the source are already measurement-only and stay that way: the collar tallies
(`SessionTests.cpp:775-778`, "T22, measured, not asserted") and the windup/shot rows (`:477-478`). They are the house
shape this card extends.

### Outcome-independent gesture invariants (the important design point)

The gesture-to-kernel invariants must be asserted **without depending on winning the bout**, or they inherit the
balance question and go red whenever the script loses. So each invariants test:

1. opens the same `FSessionRig` and session as its design twin (same bout index, same `SetScripted(true)`, same
   overlay - no new input path; the clips still go through `FArenaSession::PlayAction` ->
   `UHandInputSubsystem::PlayQuickAction` at `Session/ArenaSession.cpp:645-662`, which is the one gesture pipeline
   `CLAUDE.md` requires);
2. advances a **fixed, bounded window** (a wall of simulated seconds, not "until the phase changes") long enough for
   the script to have drawn a sigil, blinked and warded - choose it from the recorded chains, state the number in the
   report, and make it a named constant with a comment;
3. asserts, over the chain in that window: at least one `gesture sigil` **and** a following `kernel cast`; at least
   one blink accept **and** a `kernel roll actor=`; at least one `gesture ward` **and** an absorb
   (`Session/ArenaSession.cpp:807` writes `gesture ward onset=... -> absorb`, and `FActor::Metrics.Perfects` or
   `bAbsorb` is the kernel side - use whichever the recorded chain supports and say which in the report);
4. asserts the player stayed on a pad and the rival binding (`Brennic bound`, `Lio->Air.bSchool`), which need no
   outcome either;
5. asserts no infinite loop.

Outcome conditions (won the bout, the day closes, four bouts won, exactly one victory line) stay **gates** but belong
in the outcome invariants test for their scenario, which may legitimately be red after T39 if the balance is still
open - and then it is red for a stated, specific reason instead of a bundled one. Report which of the outcome
invariants are green after T39 and which are not, per scenario.

### The day-length assertion (`FullSeated:792`): DF-010 is ruled, move it

`docs/design-findings/DF-010-tiro-day-length.md` was **ruled on 2026-10-21, option E + B** (`docs/DECISIONS.md`, the
2026-10-21 "DF-010 ruled" entry). The measured day is 3.02 min (`apps/vr/tasks/T21-tiro-day-chain.md:84`) against the
asserted 7.0-10.0 min, and the finding shows the gap cannot be closed by anything carded.

**Do this:** move `:792` out of `bPass`. Day length becomes a logged measurement in the `FullSeated` report - the log
line at `SessionTests.cpp:783-785` already prints it with `INSIDE` / `OUTSIDE`, so keep that line and keep the
`PlanLoS`/`PlanHiS` constants at `:671-672` as the reported target, unchanged. **Cite the ruling in the commit
message.** The other four `FullSeated` invariants (`:788-791`) then fail on their own, which is the whole point: they
stop being masked by a number that cannot go green.

You have this licence **only because the owner ruled**; memory 0003 forbids a worker loosening a design gate
unilaterally, and that is precisely why DF-010 existed. Nothing else in the suite gains the same licence from it.

What the ruling does **not** give you: the 7-10 min target stands. Do not change any plan text about the day's length
(`docs/PROJECT-PLAN.md:71`, `docs/campaign/02-vr-run-as-campaign.md:101`, `docs/campaign/04-vr-campaign-design.md:12`,
`:27`, `:140`) or the beat sheet. Under option B the number is reached with content (the prologue tableau, `T27`,
`T28`, a victory card) and is restated only once those land **and** a day has been measured that wins all four real
bouts. This card neither argues for a shorter day nor delivers the longer one.

## Populations, not single seeds

`docs/DECISIONS.md:308-309` signed **statistics**: "median 45-80 s" and "win rate >= 70 % at competence 1 and 40-60 %
at 1.5". `FireSeated` asserts both on one scripted seed (`:344`, `:350`, run twice at `:355-356`), where a win is
probability 0 or 1 - and at competence 1.5 a 40-60 % win rate means a single-seed assertion is **designed to fail
about half the time**. `AirSeated`'s own comment says as much: "Measured, not tuned: this test may be red, and the
report says so" (`:374`).

The smallest honest step, with no new code paths:

- The median and the win rates are reported **from the existing census**, `MageArenaDesign.Census.Rivals`
  (`RivalCensusTests.cpp:341`), which already runs 20 seeds per set for both Brennic and Lio and writes JSON. Add the
  signed targets to its summary rows so each number is printed beside the target it is measured against, and print the
  per-set median and win rate explicitly. Change no census arithmetic and no seed count default.
- The census keeps asserting only **structure** (`RivalCensusTests.cpp:497`, row count) plus the one target that is met
  in every recorded set: **signature 100 % in duels that reach the last phase** (DF-006 and DF-008 tables). That is a
  population gate, and it is new.
- `Duel.FireSeated` and `Duel.AirSeated` keep their single-seed run as a **narrative trace** (the chain log and the
  windup rows are useful evidence), with their band and win conditions reported, not asserted.

**Do not re-propose MOONSHOT M9.** The full version of this - a human-shaped player population through the real hand
pipeline - is accepted as M9 (`docs/research/MOONSHOT-2026-10.md:21`, `:716`) and lands after `FScriptedPlayer`
(ARCH-REVIEW N3). M9 names "the 7-10 min day" and the DF-003 bands as exactly the numbers measured today on one
omniscient script. This card adds no player model, no profile data and no new band; it only stops a statistic being
asserted as a coin flip. Say in the report which reported numbers M9 should later re-measure per skill band.

## Deliverables (exact paths)

- `apps/vr/Game/Source/MageArenaVR/Tests/SessionTests.cpp` - the five design tests keep their names and their
  measurements, with the Report rows above moved out of `bPass` into logged measurement lines.
- A new test file, `apps/vr/Game/Source/MageArenaVR/Tests/SessionInvariantTests.cpp`, holding the invariants tests.
  **Namespace: `MageArena.Invariants.<Scenario>`** - `Wave1Seated`, `CreaturesSeated`, `FireSeated`, `AirSeated`,
  `FullSeated` (five tests, or fewer if one covers several scenarios honestly; say which and why).
  *Why `MageArena.` and not `MageArenaDesign.`:* memory 0003 names `MageArena.*` as **the green gate**, and WOLF-PLAN
  section d requires `MageArena.` all green while allowing `MageArenaDesign.` "no red beyond the current known list".
  A new always-green test placed in the red namespace inherits an exception list it does not need, and a reader
  scanning "MageArenaDesign: 5 red" would not notice a sixth. In `MageArena.` the tolerance is zero. If you judge
  otherwise, say why in the report with the gate text you are relying on, and do not move it silently.
- A stable, greppable prefix on every measurement line, so a measurement can be read out of an automation log without
  parsing prose. Proposal: `MAGEVR_MEASURE <test> <name>=<value> target=<lo>-<hi> INSIDE|OUTSIDE`. The existing lines
  at `SessionTests.cpp:166-169`, `:237-240`, `:783-785` already print the band and `INSIDE`/`OUTSIDE`; keep their
  content and give them the prefix.
- A self-test switch, `-MageArenaMeasureSelfTest`, that forces every reported measurement to read `OUTSIDE` (for
  example by shifting each reported target window off the measured value). Under it the suite must still report the
  same pass/fail verdict per test. This is the acceptance proof that the measurements can no longer fail a gate.
- `runs/T47/REPORT.md` (untracked).

## Constraints

- Do not commit, push, or touch any repository other than this one. Do not call paid services.
- Do not edit `docs/DECISIONS.md`, `CLAUDE.md`, `docs/concepts/WOLF-PLAN.md` or `apps/vr/tasks/BACKLOG.md`. Report the
  one-line change `CLAUDE.md`'s Build section needs for the new test names and the orchestrator applies it.
- Do not edit `apps/vr/data/pinned/**`, `apps/vr/data/PINNED.json`, `apps/vr/data/conformance/**` or
  `apps/vr/Game/Clips/**`. **No combat number, band, window or threshold changes value in this card** - not in data,
  not in a test constant. A changed value is a failed card, even if it turns something green.
- Do not delete a measurement. Every number the five suites print today is still printed afterwards, with the same
  arithmetic, so T39's recorded figures stay comparable.
- Do not add a shortcut that reaches the kernel without the gesture pipeline. The invariants tests play clips through
  `PlayQuickAction` like the design suites do; broadcasting a detector delegate instead (as
  `MageArena.Session.DeathBurstPerfect` does at `SessionTests.cpp:811`) would defeat the card's purpose.
- All Unreal work runs on Wolf. Never report a UE result from any other host.
- Regenerate the sigil corpus first in a fresh checkout or worktree (`CLAUDE.md`); never run `generate.mjs` bare or
  with `--templates`.
- One atomic commit per step, subject `T47: <summary>`: (1) the measurement prefix and self-test switch, no verdict
  changes; (2) the new invariants file; (3) the Report rows moved out of `bPass`. Step 1 before step 3 so the
  before/after logs are comparable.

## Acceptance (the orchestrator re-runs these on Wolf; quote their real output)

Node gates, on Lynx or Wolf:

```powershell
node apps/vr/tools/check-pin.mjs              # expected: pin OK: 9 files @ baeac66 / conformance OK: 45 vectors
node apps/vr/tools/check-pin.mjs --self-test  # expected: corruption detected: ...combat.json / conformance corruption detected: bolt-hit.json
node tools/check-commits.mjs                  # expected: check-commits OK: <n> commit subject(s) since <base>
node apps/vr/tools/lint/format-check.mjs --base <merge base>   # expected: format-check OK (changed C++ lines only)
node .ai/doctor.mjs                           # expected: no error
```

UE gates, **Wolf only**, one build and one `Automation RunTests` filter per editor process, verdicts read from
`-ReportExportPath` (WOLF-PLAN section d):

```powershell
powershell -NoProfile -File apps/vr/tools/build.ps1
# expected: "Result: Succeeded" in the log, not only exit 0

$ed = "C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
$up = "$PWD\apps\vr\Game\MageArenaVR.uproject"

& $ed $up -ExecCmds="Automation RunTests MageArena.Invariants;Quit" -unattended -nullrhi -nosplash -log -ReportExportPath="$PWD\runs\T47\Invariants"
# expected: ALL GREEN, 0 failures. Every gesture-to-kernel assertion included: a played sigil clip -> `kernel cast`,
# a blink clip -> `kernel roll actor=`, a ward clip -> an absorb. This is the card's primary deliverable.

& $ed $up -ExecCmds="Automation RunTests MageArena.;Quit" -unattended -nullrhi -nosplash -log -ReportExportPath="$PWD\runs\T47\MageArena"
# expected: all green, including the new MageArena.Invariants.* tests and MageArena.Kernel.Conformance 45/45;
# per-test comparison against T39's recorded baseline shows no test that was green turning red

& $ed $up -ExecCmds="Automation RunTests MageArenaDesign.;Quit" -unattended -nullrhi -nosplash -log -ReportExportPath="$PWD\runs\T47\Design"
# expected: no red beyond T39's recorded list, and the per-test verdicts IDENTICAL to T39's table except where a
# Report row was removed from bPass, each such change named in the report with its line
```

The two proofs this card exists for:

```powershell
# 1. The invariants are green and they really are gestures-to-kernel.
Select-String -Path "$PWD\runs\T47\Invariants\*" -Pattern "kernel cast|kernel roll actor=|absorb"
# expected: each of the three appears in a PASSING MageArena.Invariants.* test's assertion text

# 2. A measurement can no longer fail the suite.
& $ed $up -ExecCmds="Automation RunTests MageArenaDesign.;Quit" -MageArenaMeasureSelfTest -unattended -nullrhi -nosplash -log -ReportExportPath="$PWD\runs\T47\DesignSelfTest"
# expected: every MAGEVR_MEASURE line reads OUTSIDE, AND the per-test pass/fail verdicts are byte-identical to the
# runs\T47\Design table above. A verdict that moves under the switch means a measurement is still inside bPass.
```

Also quote, as a negative control that the invariants are not vacuous:

```powershell
# Mutation check (local only, reverted before the commit; show the diff you made and then `git diff --stat` empty).
# Break the sigil clip path in the scripted player so no sigil is played, re-run MageArena.Invariants:
# expected: the sigil -> kernel cast assertion FAILS. An invariants test that stays green here is asserting nothing.
```

A gate that could not run is reported as **not evaluated**, never as a pass.

## Report

Write `runs/T47/REPORT.md`:

- The T39 commits found, and T39's per-test `MageArenaDesign` before-table.
- Every acceptance command with its real output, the host name and engine build
  (`Wolf <hostname> <date> <commit> UE 5.8.3 CL 58210709`).
- The verdict table above as you implemented it: every condition you moved out of `bPass`, with its old line, and
  every condition you left as a gate **including any you decided is a gate against this card's table, with the reason**.
- The fixed advance window you chose for the gesture invariants and the recorded chain you chose it from.
- Which outcome invariants are green after T39 and which are not, per scenario, with the DF that owns each red.
- That you moved `FullSeated:792` out of `bPass` per the DF-010 ruling, the commit that did it, and the report line
  showing the day's measured length with `INSIDE` / `OUTSIDE`.
- The one-line `CLAUDE.md` Build-section change the new test names need (do not apply it).
- Which reported numbers MOONSHOT M9 should later re-measure per skill band.
- Anything you could not do and the exact error.

## Result (Wolf kaziho 2026-10-24, UE 5.8.3 CL 58210709)

Built from the worktree with `build.ps1`, `Result: Succeeded` for each step. Each suite ran in its own editor process,
verdicts read from `-ReportExportPath`. Raw logs: `runs/T47/` (gitignored), on Wolf.

**Before-table (T39's result, re-measured at `0e987cf`, code identical to `9412598`).** `MageArenaDesign.` 4/7:

| Test | Before | After `f64c685` | Why it moved |
|---|---|---|---|
| `Census.Rivals` | green | green (plus the new signature gate) | - |
| `Census.Sweep` | green | green | - |
| `Session.Wave1Seated` | green | green | - |
| `Session.CreaturesSeated` | green | green | - |
| `Duel.AirSeated` | red: `length 23.12s in 45-80` (old `:481`) | **green** | its only failure was a Report row |
| `Duel.FireSeated` | red: c1 length 38.32 s (`:344`), c1 wins (`:350`), c15 length 37.50 s (`:344`), c15 reached the last phase 1 of 2 (`:345`) | red: `brennic-c15 reached the last phase (1 of 2 breaks)` only | length and win left `bPass` |
| `Session.FullSeated` | red: lost bout 3 (`:789`), day closes (`:790`), 2 of 4 won (`:791`), day 187.85 s in 7-10 min (`:792`) | red: lost bout 3, day closes, 2 of 4 won | the day length left `bPass` (DF-010 E) |

Under `-MageArenaMeasureSelfTest` all 36 `MAGEVR_MEASURE` lines read `OUTSIDE` (0 `INSIDE`) and the per-test verdicts
and error lines are identical to the normal run. `MageArena.` 266/266, `MageArena.Kernel.Conformance` 45/45, no test that
was green turned red. `MageArena.Invariants` 5/5.

**Verdict table as implemented.** Moved out of `bPass` (still printed, same arithmetic and constants): CreaturesSeated
window (old `:244-245`); FireSeated length (old `:344`) and the c1 win (old `:348-351`); AirSeated length (old `:481`)
and win (old `:485`); FullSeated day with the teach (old `:792`). Kept as gates: the rival binding, reached the last
phase, signature in the last phase, on a pad, every outcome (won bout 2; did not lose a bout, the day closes, four bouts
won; Wave 1's phase, waves cleared, victory line). One row differs from the card's table: Wave1Seated `:175-176` asserts
victory within the **120 s cap** (`GenerousCap`), not the 25-40 s pinned window - that window was already only logged -
so it stays a gate as an outcome bound, and the window is now the `victoryS` measure line. Census.Rivals gains one
gate: the signature in every duel that reaches the last phase, on every set where one did (20/20, 18/18 Brennic; 20/20
Lio; the `-off` sets reach none).

**The gesture invariants** (`Tests/SessionInvariantTests.cpp`, `MageArena.Invariants.<Scenario>`, five tests). Each opens
its design twin's session and plays the reference script through `PlayAction` -> `PlayQuickAction` for a fixed window
of active time: **10 s** (`GestureWindowS`; every first ward, cast and blink lands inside 3 s in the recorded chains, and
the shortest bout ends at 23.12 s) and **25 s** for bout 2 (`CreaturesWindowS`; its first `kernel roll` is at 18.08 s,
the bout ends at 31.02 s). Asserted: a played bolt clip -> `gesture bolt` -> `kernel cast`; a blink clip -> `gesture
blink` -> `kernel roll actor=`; a ward clip -> `gesture ward ... -> absorb` with the kernel ward up (`FActor::bAbsorb`,
the kernel side the recorded chains support: every scenario records 0 perfects and one ward, at onset 0.028 s); the seat
on a pad; the bindings; and for FullSeated the day ending inside the twin's guard. **A played sigil clip -> `gesture
sigil` -> `kernel cast`** is asserted in Wave1Seated, CreaturesSeated and FullSeated. The Fire and Air duels' reference
script draws no sigil at all (0 `gesture sigil` lines in `runs/T19/brennic-c1-chain.log` and
`runs/T23/lio-c15-chain.log`), so there the cast invariant is the bolt's; making the script draw would change the
measuring stick (DF-011 C, not adopted). Window tallies: Wave1 sigil 1->1, bolt 25->25, blink 1->1, ward 1 raise;
Creatures 1->1, 73->73, 2->2, 1; Fire 0, 30->30, 2->2, 1; Air 0, 29->29, 2->2, 1; Full 1->1, 25->25, 1->1, 1.

**Negative controls** (local only, reverted, `git diff --stat` empty after): `if (Name.StartsWith(TEXT("sigil")))
return;` in `FArenaSession::PlayAction` -> Wave1/Creatures/Full fail `a played sigil clip -> gesture sigil -> kernel cast
(0 of 0)`; `if (Name == TEXT("bolt")) return;` -> all five fail `a played bolt clip -> gesture bolt -> kernel cast (0 of
0)`.

**Outcome invariants after T39**, per scenario: Wave1 green; Creatures green; Air green (Lio won at 23.12 s); Fire red,
brennic-c15 reaches only 1 of 2 phase breaks (DF-011); Full red, the script loses bout 3 to Brennic, so the day does not
close and 2 of 4 bouts are won (DF-011).

**DF-010:** the day-length assertion left `bPass` in `f64c685`; the day reads `MAGEVR_MEASURE Session.FullSeated
dayWithTeachS=187.85 target=420.00-600.00 OUTSIDE`. The 7-10 min target and the plan text are unchanged.

**`CLAUDE.md` Build section, one line (not applied, outside this run's paths):** "Since T47 (2026-10-24, `f64c685`), two
`MageArenaDesign` reds are known, both accepted under DF-011: `Duel.FireSeated` (brennic-c15 reaches 1 of 2 phases) and
`Session.FullSeated` (loses bout 3 to Brennic); `Duel.AirSeated` is green, its band now a `MAGEVR_MEASURE` line. The green
gate `MageArena.` includes `MageArena.Invariants.*`."

**For MOONSHOT M9** to re-measure per skill band: the Brennic and Lio census medians and win rates (c1, c1.5), the
Wave 1 and bout 2 victory times against 25-40 s and 30-45 s, and the day with the teach against 7-10 min.
