---
title: The VR run as the campaign today
channel: vr
status: partial
verified-against: mage-arena-vr 46643e2; mage-arena-int b1efd46; mage-arena a7964ad (2026-10-07)
sources: [docs/PROJECT-PLAN.md, docs/DECISIONS.md, apps/vr/data/vr/strings.json, apps/vr/data/vr/teach.json, apps/vr/Game/Source/MageArenaVR/Session/ArenaSession.h, apps/vr/Game/Source/MageArenaVR/Session/ArenaSession.cpp, apps/vr/Game/Source/MageArenaVR/Session/SessionFlow.cpp, apps/vr/Game/Source/MageArenaVR/Kernel/Games.h, apps/vr/Game/Source/MageArenaVR/Kernel/Games.cpp, apps/vr/Game/Source/MageArenaVR/Hands/MageSettings.h, apps/vr/Game/Source/MageArenaVR/Tests/SessionTests.cpp, apps/vr/tasks/T13-teach-and-pause.md, apps/vr/tasks/T15-creatures-bout-and-session-chain.md, apps/vr/tasks/A09-session-storyboard.md, apps/vr/tasks/BACKLOG.md, apps/vr/art/session-storyboard/prompts.json, apps/vr/data/pinned/docs/design/baseline-fourteen-nights/design/data/arena-tiers.json]
---

# The VR run as the campaign today

The VR campaign today is one seated run of the Tiro Games. A cold start on the dais, a 60-80 s teach with no fail state,
then the Tiro bouts with intermissions between them: soldiers, creatures, and mage duels. A loss is missio, and the
player retries the same bout at once. The only progress kept across launches is the index of the current bout, in a
one-line save file. The plan frames the run as a story: dusk, the Fire mage of the Ember Tent walks in, a finisher,
the crowd, a victory card, and the Ember champion challenge. Most of that framing is not built. What exists is the
mechanical spine: cold start, teach, bout chain, intermission, retry and continue. It shows the player instructions
only, never a name or a line of story. This page records what is built, against the committed code at `46643e2`
(read with `git show HEAD:...`, so the uncommitted edits in the working tree are excluded). The mechanics of each beat
are in `../gameplay/08-session-arc.md`.

## Beats: plan against code

Planned times come from `docs/PROJECT-PLAN.md` section 6.4. The A09 storyboard (`apps/vr/art/session-storyboard/`,
12 frames) draws the same beats at 0:00, 0:20, 0:35, 0:50, 1:10, 1:45, 2:45, 3:15, 4:25, 5:15, 6:15 and 6:35.

| Planned time | Beat (plan) | Built? | Evidence in code and data | Story element on screen |
|---|---|---|---|---|
| 0:00-0:15 | Cold start at dusk: "Raise your palm to begin." | implemented | Phase `cold` (`SessionFlow.cpp` `BeginArc`). Prompt `cold.start`. If the device FOV is small, it offers narrow view first (`settings.offer.narrow`). | Dusk sky (greybox panels) |
| - | Continue offer when a bout was saved | implemented (not in the plan table; plan section 6.6) | Phase `offer`, prompt `offer.continue`: "Raise your palm to continue. Both palms to start over." | none |
| 0:15-1:30 | Teach: ward, perfect, sigil (circle and bar), blink | implemented. **Measured** 63.306 s (T13) | `ETeachStep` Ward, Perfect, Sigil, Blink (`ArenaSession.h`). Timings in `teach.json`. Each step repeats until done. HP loss is refunded (`RefundTeachPlayer`). | Prompts `teach.ward`, `teach.ward.perfect`, `teach.sigil`, `teach.blink`. A water dummy. |
| 1:30-2:40 | Bout 1, soldiers (Tiro wave 1) | implemented. Balance red (see "Status and gaps"). | `Start()` → `TryCreateGames(..., BoutWave=0, ...)` | Conscripts and slingers on the floor below the dais |
| 2:40-3:00 | Intermission: heal 30% of missing HP, refill, clock reset; **pick the Tide Orb tier IV branch** | partial: the intermission works, **the branch pick is not built** | `TryAdvanceGames` (`Games.cpp`). Prompt `intermission`: "A moment to breathe. Raise your palm to continue." No branch code exists in `Session/`. | none |
| 3:00-4:20 | Bout 2, creatures | implemented. **Simulated** 33.05 s on the live overlay, inside 30-45 s (T15). | Tiro wave 2 | Cinder hounds, mire maw. Their death bursts cannot reach the seat (DF-004). |
| 4:20-4:40 | The Fire mage of the Ember Tent walks in (intro card using the PC portrait) | **not built** | No intro, card or portrait code in `Session/` | none |
| 4:40-6:30 | Bout 3, duel: competence 1, then 1.5 on retry or for the final. Sunfall at about 45 s. | partial: see "What the duel is" | Tiro wave 3 (`semifinal`, competence 1), then wave 4 (`final`, competence 1.5) | A mage on the floor. In the live arc it is the Water proxy. |
| 6:15 | Finisher: Leviathan Orb or a reflected fireball, 0.5 s slow motion, crowd roar | **not built** (no slow motion, no crowd audio in session code) | - | - |
| 6:30-7:00 | Victory card: perfects, Crests, time. Offer "the Ember champion" (competence 2.5) as a replay challenge. | **not built** | Phase `complete` returns an empty prompt (`GetPromptText`) | nothing |

### What the duel is

- The live chain plays **all four Tiro waves**. `TryAdvanceGames` advances until `Tiro.Waves.Num()` (4), so the
  session is: teach, soldiers, creatures, semifinal (competence 1), final (competence 1.5), complete. The plan table
  shows three bouts. The pinned data has four.
- **The live arc does not spawn the Fire mage.** `bFireMages` defaults to false (`ArenaSession.h`), and only
  `SetBout(Wave, true)` sets it. In committed code, only the `MageArenaDesign.Duel.FireSeated` test and the T12 capture run (`Wave1Capture.cpp`)
  call it; so does the uncommitted T16 style-pick capture. `BeginArc` (the `-game` start) and the `FullSeated` test never call it.
  The duel bouts in a normal run therefore spawn `"Tiro entrant · Water proxy"` (`Games.cpp:97`), as the TS kernel
  does. This was found by reading the code. No run was made for this page.
- Fire duel measurements exist only from `FireSeated` (live overlay, T15 card): competence 1 won in 29.47 s,
  competence 1.5 lost in 22.92 s.

## Narrative framing

| Element | Plan | Today |
|---|---|---|
| Setting | The dais of an arena at dusk | Greybox ring floor, raised dais, three rune pads, nine flat sky panels (BACKLOG, A02) |
| Who the player is | A Water mage (fixed loadout Tide Orb, Lash, Mirror) | The same, unnamed on screen. Kernel label `"Cassia"`. |
| Why they fight | Implicit: the Tiro Games | Never stated. No string names the Games, a tent or the stakes. |
| Opponent identity | "The Fire mage of the Ember Tent" with an intro card | None on screen |
| Loss | "Loss grants missio and an instant retry of the same bout" | The kernel result kind is `"missio"` (`Games.cpp:207`). The word is never shown. The prompt after a loss reuses `offer.continue`. |
| Victory | Victory card, then the Ember champion challenge | Nothing is shown. The kernel computes `"champion"` together with gold and renown, and nothing displays them. |
| Voice | Cut for the slice ("voice acting", plan section 1) | None |

Every player-facing string is in `apps/vr/data/vr/strings.json` (24 keys). None names a person, a place or a faction.

## Progression and persistence

| What | Kept? | Where and how | Evidence |
|---|---|---|---|
| Current bout index | yes, across launches | `Saved/MageArena/bout.txt`, a single line `bout=N` | `SaveFilePath`, `WriteSavedBout`, `ReadSavedBout` (`SessionFlow.cpp`) |
| ...when written | On quit during an active bout (`NotifyQuit`). When an intermission advances to the next bout. | | `SessionFlow.cpp` (`NotifyQuit`; the `TryAdvanceGames` branch) |
| ...when cleared | When the player accepts the continue offer (`ContinueOffer`; the file is written again at the next intermission or quit). On "both palms" (start over). | | `ContinueOffer`, offer-hold branch |
| ...when ignored | A corrupt line, junk, or an index outside the Tiro waves. The run then starts from the teach, with a warning in the log. | | `ReadSavedBout` |
| Comfort settings: casting hand, wide or narrow view, Gentle | yes | `Saved/MageArena/settings.json`. Changed by palming the settings stones on the dais. A missing or corrupt file keeps the defaults. | `Hands/MageSettings.h` |
| Teach completion | no | Every fresh launch without a saved bout plays the teach again | `BeginArc` |
| Gold, renown, missio or champion result | no | Computed per bout in `FGamesResult`, then dropped | `Kernel/Games.h`, `Games.cpp:198-214` |
| Branch picks, unlocks, perfects, best times | no | Not built | - |
| Any story flag | no | Not built | - |

## Retry, pause and quit rules

| Situation | Rule | Evidence |
|---|---|---|
| Bout lost (player down) | Phase `lost`. One palm restarts **the same bout with the same seed**, so the bout replays identically: the kernel is deterministic. Both palms held for 0.40 s (`offerHoldS`) start over from the teach and clear the save. | `StepGames` sets `lost`. The offer branch of `AdvanceArc` (`SessionFlow.cpp`) → `Start(BoutSeed)`. |
| Bout won, not the last | Phase `intermission`. One palm advances (heal, refill, clock reset) and saves the bout index. | `TryAdvanceGames`, `WriteSavedBout` |
| Last bout won | Phase `complete`. Nothing follows. | `StepGames`, `GetPromptText` |
| Teach | No fail state. Each step repeats until it is done. HP and damage are refunded. | T13 card, `RefundTeachPlayer` |
| Focus lost, headset removed, or hand tracking lost for 1.5 s in combat | Pause: the kernel stops stepping. Resume with both palms raised, then 3-2-1. | `HoldPause`, `PollTracking`, `teach.json` `trackingLossS`, `resumeStepCount` |
| Quit mid-bout | The next launch offers that bout from its start | `NotifyQuit`, `ReadSavedBout` |
| Skip the teach | A debug console command only (`MageArena.Session.SkipTeach`) | T13 card |

Pause, resume and comfort details are in `../gameplay/09-pause-comfort-accessibility.md`.

## Session length

| Segment | Plan | Measured or simulated |
|---|---|---|
| Teach | 60-80 s | 63.306 s, **simulated** with a scripted clip run (T13) |
| Bouts, live overlay | about 5 min across three bouts | The seated reference script **loses Wave 1** at 23.25 s (T15, `FullSeated` red) |
| Bouts, calibration proposal (`-MageArenaProposal`) | - | All bouts complete in 105.35 s, about 2.8 min with the teach (T15) |
| Whole session | 7-10 min | Not reached. The proposal run is about a third of the target. |

These are scripted-controller figures, not human play. Balance is owned by DF-003 and DF-004 (BACKLOG). Arc
timing is in `../gameplay/08-session-arc.md`.

## Status and gaps

- **Built:** cold start, continue offer, teach, the bout chain over the four Tiro waves, intermissions with
  `betweenWaves`, missio retry of the same bout, the saved bout, pause and resume, and the comfort settings.
- **Not built:** the Tide Orb tier IV branch pick, the Fire mage intro, the Fire mage in the live arc, the
  finisher's slow motion and crowd, the victory card, the Ember champion challenge, any visible reward, and any
  proper noun on screen.
- **Plan and data disagree on the bout count:** three bouts in the plan table, four Tiro waves in the data and in
  the chain. Owner decision 1 on page 04.
- **"Intro card using the PC portrait" cannot be met as written.** TV discarded all A2 portraits (`MAGE-ARENA-PLAN.md`
  D12), and VR does not inherit the TV art style (`DECISIONS.md` 2026-10-02). The intro card must be VR-made, in the
  chosen VR style.
- **Accepting the continue offer clears the save** before the bout is played. A crash during that bout falls back
  to the teach on the next launch.
- Known red design gates on a clean tree: `Duel.FireSeated`, `Session.FullSeated`, `Session.Wave1Seated`
  (`CLAUDE.md`).

## Open questions

- Should a loss show the word "missio" (a diegetic crowd mercy) instead of the generic continue prompt? Page 04
  recommends yes.
- Should the competition slice end after the semifinal (the plan's three bouts) or after the Tiro final (the data's
  four waves)?
- Should the slice show gold and renown at all, when nothing spends them?
