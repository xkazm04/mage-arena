# DF-013 - The intermission's idle kernel steps move the later bouts of the day

Found 2026-10-25 on Wolf (UE 5.8.3 CL 58210709). T28 part 2 first saw it in T62's run, and T27 measured it on branch
`autopilot/accepted-idea-delivery-51306867`. Status: **OPEN, needs an owner ruling.** Nothing in this file is decided in
code. The two options below were built only as uncommitted experiments, measured, and reverted.
Raw logs (gitignored): `runs/T27/design1.log` (the branch as committed), `runs/T27/expA.log`, `runs/T27/expB.log`.

## What happens

The chained Tiro day does not start each bout the same way:

- **Bout 1** (`StartDayBout` -> `FArenaSession::Start`) and every retry after a loss call `TryCreateGames`, which builds a
  fresh arena from the day's seed (`Kernel/Games.cpp:224`, `CreateArena(Seed)`).
- **Bouts 2-4** continue the same arena. `ContinueIntermission` calls `TryAdvanceGames` (`Session/SessionFlow.cpp`
  `ContinueIntermission`; `Kernel/Games.cpp:347`). That keeps `State.Tick`, the one sequential random stream
  (`Kernel/ArenaKernel.cpp:362`, `ArenaRandom`), `NextId` and the rest of `FArenaState`.

During an intermission the session pumps the kernel with idle input every 1/60 s: the `Pump` in
`FArenaSession::AdvanceArc`'s offer / intermission / lost branch (`Session/SessionFlow.cpp:1468-1469` at `dc0a49f`; T28
cited the branch at :1462).
So a longer intermission hands bout 2, 3 or 4 a different starting state, and the seated script's duel then goes
another way. The pinned bands judge the length of the fight, so moving it with story time is a design-number problem.
It is not a cosmetic one.

## Evidence

The reference script is the same throughout (`MageArenaDesign.Session.FullSeated`, seed 1). Only the intermission
seconds differ.

| Run | Intermissions 1 / 2 | Bout 1 | Bout 2 | Bout 3 (Brennic, lost) |
|---|---|---|---|---|
| T56 log, before T28 (the palm at once) | about 0 / 0 s | 39.38 s | 27.50 s | **42.50 s** |
| T62, T28's 3 s floors (`BRAZIERS-T62-2026-10-25.md` section 3) | 3.02 / 3.00 s | 39.38 s | 27.47 s | **29.00 s** |
| T27 rushed, same floors (`design1.log`) | 3.01 / 3.01 s | 39.38 s | 27.47 s | 29.00 s |
| T27 natural, C2's natural seconds (`design1.log`) | 16.01 / 18.01 s | 39.38 s | 27.78 s | **36.10 s** |

Bout 1 never moves: a fresh arena starts it. Bouts 2 and 3 move with the intermission before them, and bout 3 by up to
13.5 s. With the story beats (T27/T28) the intermission is meant to vary with the player's pace, so the same play would
give different fights in a rushed day and in a natural one.

`MAGEVR_MEASURE Session.FullSeated` on the branch: `dayWithTeachS=212.31` (rushed), `dayNaturalS=305.72` (natural).

## Options, each measured

Each option was a one-line uncommitted patch in `SessionFlow.cpp`. The editor was rebuilt (`Result: Succeeded`) and
`MageArenaDesign.Session.FullSeated` was run once. The test plays the rushed day and then the natural day.

### (a) Freeze the kernel during intermissions

No `StepArena` while the stage is `intermission`. The offer and the lost phase keep their pump.

| Path | Bout 1 | Bout 2 | Bout 3 | Day |
|---|---|---|---|---|
| rushed | 39.38 s | 27.50 s | 42.50 s, lost | `dayWithTeachS` 212.31 -> **225.83** (+13.52) |
| natural | 39.38 s | 27.50 s | 42.50 s, lost | `dayNaturalS` 305.72 -> **311.83** (+6.11) |

- **Fully decoupled.** The rushed and natural fights are identical, and both equal the T56 (pre-T28) numbers to the
  hundredth.
- The natural day minus the rushed day is then exactly the story dwell difference (86.00 s).
- Outcomes are unchanged: won, won, lost bout 3 (DF-011 stays as it is).
- **Cost:** anything still in flight when the last opponent falls stays frozen until the next bout spawns. The player's
  tier clock is already frozen in the pump (`FreezeTeachClock`), so nothing else is known to depend on these steps.

### (b) Re-seed the kernel's random stream per bout

Before `TryAdvanceGames` the patch set
`State.Rng = BoutSeed * 2654435761 + (wave + 2) * 40503`. The tick, ids and the rest stay continued.

| Path | Bout 1 | Bout 2 | Bout 3 | Day |
|---|---|---|---|---|
| rushed | 39.38 s | 28.03 s | **6.67 s**, lost | `dayWithTeachS` 212.31 -> **190.53** (-21.78) |
| natural | 39.38 s | 27.52 s | 31.78 s, lost | `dayNaturalS` 305.72 -> **301.14** (-4.58) |

- **Not decoupled.** Rushed and natural still differ (bout 3 6.67 s against 31.78 s). The random stream is therefore
  not the only channel: the continued tick or other carried state also reaches the next bout. That was not traced
  further.
- It moves every chained bout's numbers anyway. Making (b) work would mean also resetting the tick, which in effect
  means a fresh arena per bout, as bout 1 already gets. That variant was not measured.

Neither option touches a pinned number or band, and neither changes the single-bout design tests (`Duel.*`,
`Census.*`, `Wave1Seated` and `CreaturesSeated` start each bout with `Start()`). Those were not re-run under the
patches. Only the chained day (`Session.FullSeated` and its `MageArena.Invariants.FullSeated` twin) is expected to move.
`MageArena.` was not run under either patch.

## Recommendation

**(a).** It is the smallest change, it removes the coupling completely, and it restores the pre-T28 fight numbers
exactly. It also matches the rule that nothing fights in an intermission. The rushed and natural day measurements of
T27 / C2 section 4 then differ only by story time, which is what they are meant to compare. (b) is not recommended: it
moves the numbers without decoupling them.

The ruling is the owner's, because either option moves `Session.FullSeated`'s logged numbers and the bout-3 time
DF-011 accepted (29.00 s today; 42.50 s under (a)).
