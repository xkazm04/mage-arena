# T54 - Overlay end-state goldens, recorded before T39

Status: open, **blocking T39** (needs Wolf, the x64 UE host; written 2026-10-22 on Lynx, where none of it runs). Record the goldens on
today's overlay before T39's first commit. Source: `docs/research/ARCHITECTURE-REVIEW-2026-10-21.md` A1 (and A3b, below);
`docs/concepts/WOLF-PLAN.md` P1.
Max turns: 120

## Goal
T39 changes six overlay rules (DF-003/005/006/007/008/009) and today nothing records what the live overlay does. The 45 null-path
conformance vectors and the `Fire.Golden` files (`TryCreateGames` with no ruleset) cannot see any of those changes. Add
`MageArena.Overlay.Golden`: about five fixed-seed bouts on the **live** overlay, each written as an end-state file, so every
`T39: DF-00x` commit shows in its diff exactly which bouts that DF moved. This is the cheap subset of MOONSHOT M8: end states, not
tick vectors. Size M. Nothing in `apps/vr/data/pinned` changes.

## Read first
- `CLAUDE.md`, `docs/DECISIONS.md` (they outrank this card), `docs/research/ARCHITECTURE-REVIEW-2026-10-21.md` A1 (lines 99-119) and A3
  (b) (lines 162-163)
- `apps/vr/tasks/T39-adopt-df-recommendations-and-rerun-design.md` (the card this one gates), `docs/concepts/WOLF-PLAN.md` P1
- Code: `Tests/FireTests.cpp` (`MageArena.Fire.Golden`, `RunDuel`, `ScenarioPath`: the summary format to reuse), `Tests/RivalTests.cpp`
  and `Tests/DayTests.cpp` (how a bout is run with `&Rules`), `Kernel/VrRulesLoad.cpp` (`LoadVrRuleset`, the variant switch at `:658-662`),
  `Kernel/KernelData.cpp:719` (the boot log)

## Deliverables (exact paths)
1. `MageArena.Overlay.Golden` (new test file under `Source/MageArenaVR/Tests/`, in the `MageArena` suite). It runs about five fixed-seed
   bouts through `TryCreateGames(..., &Rules, bAirFinal)` with the **live** overlay (`LoadVrRuleset` on `apps/vr/data/vr`, not a copy):
   1. wave 1 with split hands;
   2. the creature bout (ember spit);
   3. Brennic c1 (the HP-gated phases);
   4. Lio c1 (the Air final, `bAirFinal`);
   5. a fire-wall duel.
   Each bout is replayed twice in the test and the two hashes must agree (determinism), as `Fire.Golden` does.
2. `apps/vr/data/scenarios-vr/overlay/*.json`, one file per bout. Each holds the `Fire.Golden` summary (hash, ticks, outcome, hp, mana,
   same fields and formatting) plus **event counts by kind**, and a header recording:
   - the sha256 of `apps/vr/data/vr/combat.vr.json`, `apps/vr/data/vr/collar.json` and `apps/vr/data/vr/schools/spells-air.csv`;
   - whether `-MageArenaProposal` was set (live or proposal; the goldens are recorded live, and the test refuses to compare a proposal run).
3. Seen red first: the test fails with the message `overlay changed since recording` when one byte of `combat.vr.json` changes (compare the
   recorded hashes before comparing any bout). Report the red run (flip one byte in a temp copy or the live file, then restore) and
   the green run.
4. A recording path: one named switch (for example `-MageArenaRecordOverlayGolden`) rewrites the files; the default run only compares.
   It must never write `scenarios-vr/fire-duel-competence-*.json`, which stay null-path baselines.
5. (A3b, folded in) log the overlay sha256 and the variant (live or proposal) next to the pin hash at boot (`KernelData.cpp:719`, or
   wherever `LoadVrRuleset` succeeds), and in the census and design CSV headers. The Node side already prints an overlay sha256
   (`apps/vr/tools/check-overlay.mjs`); the two must agree for the same bytes, or the report says why not.
6. `runs/T54/REPORT.md` (untracked).

## Sequencing rule (this is the point of the card)
- Record the goldens on **today's overlay**, before T39's first commit, and commit them as `T54: ...`.
- Each `T39: DF-00x` commit then re-records the goldens (the overlay hash header changes in every file, which is expected). The commit
  message or the report lists every bout file whose hash, ticks, outcome, hp, mana or event counts moved, and explains each. A bout that
  moved and that the DF did not mean to touch (for example DF-003's `wide` fire wall changing the creature bout) is a finding, not a
  re-record.

## Constraints
- Do not commit anything but the files above, do not push, do not touch any repository other than this one. No paid services.
- Do not edit `docs/DECISIONS.md`, `apps/vr/data/pinned/**` or the two `fire-duel-competence-*.json` goldens. No combat number moves.
- All Unreal work runs on Wolf. Lynx never reports a UE build or test result.
- Regenerate the sigil corpus first (`CLAUDE.md`); never run `generate.mjs` bare.

## Acceptance (the orchestrator re-runs these on Wolf; quote their real output)
```
node apps/vr/tools/check-pin.mjs                                     # expected: pin OK
powershell -NoProfile -File apps/vr/tools/build.ps1                  # expected: editor target builds
UnrealEditor-Cmd MageArenaVR.uproject -ExecCmds="Automation RunTests MageArena.Overlay.Golden;Quit" -unattended -nullrhi -nosplash -log
                                                                      # expected: green on the recorded overlay; red with
                                                                      # "overlay changed since recording" after one byte of combat.vr.json changes
UnrealEditor-Cmd MageArenaVR.uproject -ExecCmds="Automation RunTests MageArena;Quit" -unattended -nullrhi -nosplash -log
                                                                      # expected: no new failure beyond the known list in CLAUDE.md
```

## Report
Write `runs/T54/REPORT.md`: the five bouts (seed, wave, school, what each exercises), the recorded file list with each file's overlay hashes,
the seen-red run and the green run (real output; host name and engine build), the boot-log line, anything not done and the exact error.
