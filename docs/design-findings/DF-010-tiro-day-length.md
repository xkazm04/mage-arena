# DF-010 - The Tiro day is 3 minutes, the gate asserts 7-10, and no finding owns the number

> **Ruled 2026-10-21: E + B.** The owner keeps the 7-10 min target and buys the minutes with content. **E:** the
> day-length assertion leaves `bPass` and becomes a logged `INSIDE`/`OUTSIDE` measurement - `T47` carries that edit now,
> unblocked. **B:** the prologue tableau, `T27` and `T28` and a victory card supply the missing minutes, which makes
> **C2 (the story bible and slice script) the critical path** - it does not exist yet and both cards are blocked on it.
> A, C, D and "keep the gate" are rejected; no pinned band, win rate or pacing window is touched. The plan's number is
> restated only once T27/T28 land **and** a day is measured that wins all four real bouts - which has never happened.

Found 2026-10-21 on Lynx (ARM64, no Unreal) from the committed measurements and the source. Nothing here was built or
run; every figure is quoted from a recorded run. Status: **RULED 2026-10-21 - option E + B**
(`docs/DECISIONS.md`, the 2026-10-21 "DF-010 ruled" entry, newest first). The measurement below stands as recorded; the
Options and Recommendation sections are kept as the reasoning the ruling was made on, not as live choices.

Why this is a finding and not a patch: `.ai/memory/0003-design-tests-red-by-design.md` says never tune a number,
loosen a threshold or skip a test to turn a `MageArenaDesign` test green, because the seated numbers are the owner's
sign-off. The day-length assertion is the one red condition in the suite that **no accepted finding owns**, so
`T39`'s acceptance ("green, or red and explained by an accepted DF",
`apps/vr/tasks/T39-adopt-df-recommendations-and-rerun-design.md`, Goal) cannot be met for it either way. This file is
the missing owner.

## Measurement

| | Value | Where |
|---|---|---|
| Asserted day, with the teach | **7.0-10.0 min** | `SessionTests.cpp:792` in `FMageArenaFullSeated::RunTest` (`MageArenaDesign.Session.FullSeated`), bounds `PlanLoS` / `PlanHiS` at `:671-672` |
| Measured day, with the teach | **3.02 min** | `apps/vr/tasks/T21-tiro-day-chain.md:84` (orchestrator verification, 2026-10-07); rounded to "3.0 min" in `docs/design-findings/DF-006-phases-shorten-the-duel.md:33`, `docs/research/KPI-2026-10-20.md:113`, `docs/research/STATE-EVAL-2026-10-20.md:54` |
| Bouts alone | **105.35 s** (about 1.76 min), all four complete | `docs/campaign/02-vr-run-as-campaign.md:100` (T15) |
| Teach alone | **63.306 s**, simulated | `docs/campaign/02-vr-run-as-campaign.md:30` (T13) |
| The plan text the bounds come from | "one 7-10 minute session: a 60-80 s teach, then three bouts" | `docs/PROJECT-PLAN.md:71` |

Three things about the 3.02 min that a reader must not lose:

1. **It is not a finished day.** The run that produced it lost Bout 3 to Brennic at 35.82 s and stopped there
   (`T21-tiro-day-chain.md:82-84`; `FullSeated` ends the run on a loss before the final, `SessionTests.cpp:736-739`).
   The 105.35 s four-bout figure comes from an earlier run whose Bouts 3-4 were water proxies
   (`T21-tiro-day-chain.md:83-84`). **A day that wins all four real bouts has never been measured.**
2. **It is a lower bound on a human's day.** The reference script instant-continues every intermission: `FullSeated`
   broadcasts `OnWardRaised` the moment the stage turns to `intermission` (`SessionTests.cpp:730-733`, the broadcast at `:732`), so the
   intermission costs ~0 s. The intros are 3 s each (T21's accepted worker choice, `T21-tiro-day-chain.md:85`), the
   teach is skipped in the day session and measured in a separate first-launch pass
   (`SessionTests.cpp:716`, and the separate first-launch pass at `:677-703`), and the prologue tableau is **not built**
   (`docs/campaign/02-vr-run-as-campaign.md:15`) though the plan budgets it at <= 60 s
   (`docs/campaign/04-vr-campaign-design.md:26`). Every beat that is meant to take a human's time is either absent or
   zero-cost in the measurement.
3. **The bound asserted is stricter than the document the test cites.** The test's own comment points at
   "`docs/campaign/04-vr-campaign-design.md` section 1, Games day 6-10 min" (`SessionTests.cpp:667-668`), and that
   document does say 6-10 min (`:12`, `:27`, `:140`). The constants are 7.0 and 10.0. The lower bound the suite
   enforces has no document and no ruling behind it.

## Why the day is short

Not because the bouts are badly tuned. Because **the fighting is the only thing in the day that has length**, and the
pinned bands cap it below the target on their own.

The pinned Tiro target bands are 25-40, 30-45, 45-70 and 45-80 s, which `docs/campaign/04-vr-campaign-design.md:137`
itself totals as **2.4-3.9 min of fighting**. So even a day in which every bout ran to the *top* of its pinned band
gives 3.9 min of fight, and reaching 7 min needs **more than 3 minutes of non-fight content inside the day** - content
that is carded and unbuilt (`apps/vr/tasks/T27-voice-and-chorus.md`, `T28-intermission-scenes.md`, both "open, blocked
on C2").

What the bouts actually measure (medians, 20 seeds each, from the censuses):

| Bout | Live median | Proposal median | Source |
|---|---|---|---|
| 1, soldiers | lost at 23.25 s | won | `DF-001-seated-wave1.md` ("After T11"); `T21:82` |
| 2, creatures | 33.05 s before DF-007, 52.02 s after | 31.02 s | `DF-007-ember-spit-slows-bout-2.md` table |
| 3, Brennic (Fire) | **31.23 s** at c1, 24.43 s at c1.5 | **41.87 s** at c1, 38.96 s at c1.5 | `DF-006-phases-shorten-the-duel.md` table |
| 4, Lio (Air) | **23.27 s** at c1, 24.09 s at c1.5 | 20.92 / 21.17 s | `DF-008-lio-too-weak.md:8-11` |

The scripted reference player wins a named-rival duel in roughly 21-42 s against a signed target median of 45-80 s
(`docs/DECISIONS.md:308`). DF-006 and DF-008 already own that gap and the owner has already ruled on them
(`docs/DECISIONS.md` 2026-10-20, "adopt every recommendation"); DF-006 B adds about 6 s per rival duel, so roughly
**+12 s to the day**. T39 closing every accepted finding at its best case moves 3.02 min to something near 3.5 min.
Nothing in flight can reach 7.

## What is the owner's to move, and what is not

- **The 7-10 min number is plan text, not a ruling.** `grep` of `docs/DECISIONS.md` finds no minute figure for the day;
  the 2026-10-07 "v2 vision accepted" entry commits to "a full Tiro day" and names no length. The number lives in
  `docs/PROJECT-PLAN.md:71`, `docs/campaign/02-vr-run-as-campaign.md:101` and
  `docs/campaign/04-vr-campaign-design.md:12`, `:27`, `:140` (as 6-10), and in two constants in a test. The owner may
  restate it. **Nobody else may**, and memory 0003 is why.
- **The bout bands are a ruling and are not in scope here.** Median 45-80 s and win rate >= 70 % at competence 1 /
  40-60 % at 1.5 are the owner's signed targets at `docs/DECISIONS.md:308-309`. This finding does not propose moving
  them, and option C below is costed on the fact that it cannot be taken without touching them.
- **The pinned pacing windows are not anyone's to move here.** Bout 2's 30-45 s window is
  `combat.json pacingTargets.creatureWaveS`, pinned, and the test says so in as many words: "Do not move the window to
  make a bout pass" (`SessionTests.cpp:207`). Out of scope.

## Options

**A. Accept a shorter day; restate the plan's number to what the slice is.**
Set the plan and campaign text to the measured shape (about 3-5 min for the day with the teach), keep a wide sanity
band in the gate (for example 2-15 min, to catch a hang or a zero-length day) and report the exact figure.
*Cost:* five document edits (`PROJECT-PLAN.md:71`, `campaign/02:101`, `campaign/04:12`, `:27`, `:140`) and one
submission-facing claim: `docs/submission/VIDEO-BEAT-SHEET.md:62` already flags "about ten minutes" as a target and not
a measurement, and would have to say three. The slice reads as a tight demo rather than a session. Cheapest in work,
most expensive in how the entry presents.

**B. Lengthen the day with content.**
Build the beats the plan already cards: the prologue tableau (<= 60 s), voiced ritual, taunts and aftermath (T27), the
intermission scenes with their one optional choice (T28), a victory card. These are the minutes the measurement
currently prices at zero, and a human who does not instant-continue an intermission spends them even today.
*Cost:* T27 and T28 are both blocked on **C2, the story bible and slice script - an owner design call, not a worker
task** (both cards' status lines). So this option's first cost is the owner's own writing time, before any Wolf time.
It buys non-interactive minutes: a judge's hands-on time stays near 2 min. It is also the only option that raises the
day without touching a signed number.

**C. Make the rivals harder so the bouts last longer.**
*Cost: arithmetically insufficient on its own, and not free.* The pinned bands cap four bouts at 3.9 min
(`campaign/04:137`), so even perfect compliance leaves the day under 5 min with the teach. Pushing past the bands means
re-opening median 45-80 s and the win rates at `docs/DECISIONS.md:308-309`, which DF-006 and DF-008 were just ruled
against doing (their adopted options aim *into* the band, not past it). Taking C means superseding a 2026-10-07 ruling.
Recommended only as a component, never as the answer.

**D. More bouts per day (five or six instead of four).**
*Cost:* canon. The day is four waves per tier, from `arena-tiers.json` (`campaign/04:27`), and the four bouts are the
slice's named cast (soldiers, creatures, Brennic, the Air final). A fifth bout is new content inside a 4-week deadline
and a new rival or wave composition. It also re-opens the 2026-10-20 "no cut" ruling's scope in the other direction.

**E. Rule the assertion measurement-only and move it out of the gate.**
Day length becomes a logged number in the `FullSeated` report (the log line at `SessionTests.cpp:783-785` already
prints it with `INSIDE` / `OUTSIDE`), and the gate keeps only the invariants that must always hold: the day closes, no
infinite loop, the script does not lose a bout. The target is re-set once a day has been measured that wins four real
bouts, and properly once a human-shaped population exists (MOONSHOT **M9**, accepted,
`docs/research/MOONSHOT-2026-10.md:21`, `:716` - it names "the 7-10 min day" as one of the numbers measured today on a
single omniscient script).
*Cost:* no gate on day length until then, and the owner must say so out loud, because memory 0003 forbids a worker
doing it. It settles nothing about what the day *should* be: it only stops a 3-vs-7 arithmetic gap from masking
regressions in the four real invariants bundled beside it.

## Recommendation

**E now, then A or B as the owner's call - and say which in the same breath.**

E first because the present assertion is not a fair reading of the shipping day in either direction: it is measured on
a run that stopped on a lost bout, with every intermission instant-continued and the prologue unbuilt, and it enforces
a 7.0 lower bound that no document states. Leaving it inside `bPass` keeps four real invariants
(`SessionTests.cpp:788-791`: no infinite loop, the script wins every bout, the day closes, four bouts won) hidden
behind a number that cannot go green. Moving it out makes the gate **strictly stronger**, because those four then fail
on their own.

Then the product question, which is the owner's alone: **a 3-minute day and a 7-10-minute day are different
submissions.** Three minutes is legible, tight and finishable inside a judge's attention; it also reads as a vertical
slice rather than a game session, and the entry's own video sheet currently promises ten minutes. Seven to ten minutes
reads as a session, and on the numbers above it can only be bought with option B's scenes - which means the owner
writing C2 - because the fighting is capped at 3.9 min by bands the owner has already signed. There is no arithmetic
route from the current content to 7 min, so this is a content decision dressed as a test failure.

If the owner wants the longer day: **E + B**, and restate the plan's number only once T27/T28 have landed and a
four-bout day has been measured. If the owner is content with the shorter day: **E + A**, and the beat sheet and plan
say three to five minutes honestly. **C and D are not recommended**: C re-opens a signed ruling for a gain that cannot
close the gap, and D buys minutes with content the deadline does not have.

## What this does not decide

- It does not touch DF-006's or DF-008's adopted options, or any pinned number.
- It does not change `T39`'s scope. T39 adopts the accepted findings and re-runs the suite; the day-length condition
  stays red through T39 and is explained by **this finding being open**, which is the honest status line.
- The mechanical reshaping of the suite (moving measurements out of `bPass`, and giving the repo a green
  gesture-to-kernel invariants test) is carded separately as `apps/vr/tasks/T47-split-design-gates.md`. T47 carries
  option E's edit **only if this finding is ruled**; until then T47 leaves the day-length assertion where it is and
  says so.
