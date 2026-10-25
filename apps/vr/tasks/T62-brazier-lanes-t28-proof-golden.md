# T62 - the near braziers off every threat lane, the T28 Unreal proof, one golden re-record

Status: done 2026-10-25 (`docs/research/BRAZIERS-T62-2026-10-25.md`). Near braziers at (-8, +/-7.8); ClearOfThreatLanes red
then green; T28 built with no fix, Intermission 9/9; goldens re-recorded, hash lines only; MageArena. 321/321, EXIT CODE 0;
MageArenaDesign 5/7 (DF-011); D-G4 max 126 (+0), parity ok; the critic reads no after-build brazier behind a threat. Open
(report section 8): the far-right brazier in the T58 absorb splashes, the backdrop rule, the T28 idle-step shift of bout 3.
Max turns: 160

## Goal
Master green again in one Unreal lane. Athena's ruling on c69ef865 (`docs/DECISIONS.md`, 2026-10-25) accepts the T61
flame colour and rejects the near-brazier positions (-3.5, +/-7.8): the fire bolt and the steel line of
`04-threats-midflight` pass in front of them. In order: (a) re-pick the near pair so that from the centre seat no brazier
stands behind any demo threat lane, under a test; (b) build T28's never-built C++ and prove `MageArena.Intermission.*`,
with the six owed test edits; (c) re-record `MageArena.Overlay.Golden` once, hash lines only; (d) D-G4 and a blind
critic on the final build. No combat number moves.

## Read first
- `CLAUDE.md`, `docs/DECISIONS.md` (they outrank this card if they conflict)
- `docs/research/BRAZIERS-T61-2026-10-25.md` (sections 2.3, 3, 5, 6), `apps/vr/tasks/T61-brazier-flame-reads-as-fire.md`
- `apps/vr/tasks/T28-intermission-scenes.md` (all of it: "Out-of-path test edits" and "PART 2 - Unreal proof owed")
- `apps/vr/Game/Source/MageArenaVR/Tests/BrazierTests.cpp`, `Tests/OverlayGoldenTests.cpp`

## Deliverables (exact paths)
- `apps/vr/data/vr/arena-layout.json` - the near pair re-picked and mirrored; `_propNote` rewritten.
- `apps/vr/Game/Source/MageArenaVR/Tests/BrazierTests.cpp` - `MageArena.Greybox.Brazier.ClearOfThreatLanes`: screen
  boxes from the centre seat, every brazier, every demo lane, every point of the flight. Red on (-3.5, +/-7.8) first.
- T28: compile fixes inside its intent (no C2 text re-authored); the six test edits from the T28 card's table
  (`CollarTests.cpp:372`, `FistTests.cpp:162`, `PauseTests.cpp:230`, `:506`, `TeachTests.cpp:308`, `:411`).
- `apps/vr/data/scenarios-vr/overlay/*.json` - re-recorded once, after (a) and (b) are final; only the
  `arena-layout.json` hash line and `overlaySha256` may change.
- `docs/research/BRAZIERS-T62-2026-10-25.md` - the report. Evidence in `C:/Users/kazda/mage-art-staging/t62-2026-10-25/`.

## Constraints
- At most one Unreal process machine-wide. Build.version 5.8.3. MetaXR plugins copied with robocopy /E. Sigil corpus
  regenerated per CLAUDE.md. `git status` clean of those copies.
- Nothing under `apps/vr/data/pinned/`, `PINNED.json` or `conformance/` changes. The flame colour constants stay.
- No paid service, no image generation, no push.

## Acceptance
```
powershell -NoProfile -ExecutionPolicy Bypass -File apps/vr/tools/build.ps1   # Result: Succeeded
Automation RunTests MageArena.Intermission     # 9 / 9
Automation RunTests MageArena.Overlay.Golden   # green after one re-record; the diff is hash lines only
Automation RunTests MageArena.                 # every test Success, EXIT CODE: 0
Automation RunTests MageArenaDesign.           # 5 / 7, only Duel.FireSeated and Session.FullSeated red (DF-011)
node apps/vr/tools/lint/format-check.mjs && node apps/vr/tools/lint/unused-members.mjs   # OK
node apps/vr/tools/check-overlay.mjs; node apps/vr/tools/check-pin.mjs                   # OK
capture-budget.ps1 -Runs 3 -ExtraArgs -MageArenaCreatureImportScene                      # max <= 150, parity ok
blind critic, placement only                    # no brazier behind or touching a threat
```
