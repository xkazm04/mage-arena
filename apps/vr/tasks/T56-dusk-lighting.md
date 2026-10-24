# T56 - dusk lighting

Status: done (2026-10-24, `b82042e`; see "Result" below)
Max turns: 120

## Goal
The arena is lit toward the key art's golden-hour dusk: low warm key light, long soft shadows, orange-to-lilac sky, per the approved style block (c) 'Light' slot. Forward shading is kept.

Owner answer 6 of 2026-10-24 (`docs/DECISIONS.md`, "2026-10-24 - Visual approvals"): the presentation gap is written as cards. This card answers shortfall 3 of `docs/concepts/VISUAL-PLAN.md` (b), "Scene dressing". No combat number changes, and the work fits the Quest budget of `docs/PROJECT-PLAN.md` section 7 (<=150 draw calls, <=350k visible triangles, <=2,000 particles).

## Read first
- `CLAUDE.md`, `docs/DECISIONS.md` (they outrank this card if they conflict)
- `docs/concepts/VISUAL-PLAN.md` section (b), shortfall 3, and the approved style block in section (c)
- `apps/vr/art/key-art/` (S1 is the north-star frame)

## Deliverables (exact paths)
- Arena lighting and sky setup in the desktop and Quest-bound maps/code, named in the report.

## Constraints
- Do not commit, push, or touch any repository other than this one.
- Do not call paid services. Do not edit `docs/DECISIONS.md`.
- No combat number changes: `apps/vr/data/pinned/` is never edited, and no overlay value moves.
- Fits the Quest budget of `docs/PROJECT-PLAN.md` section 7; forward shading stays on.
- The threat colour language stays: element colour = absorb, steel = dodge, black core with a red rim = leave.
- One Unreal process at a time on this machine.

## Acceptance (the orchestrator re-runs these; quote their real output in your report)
```
powershell -NoProfile -File apps/vr/tools/build.ps1   # expected: Result: Succeeded
MageArena. automation group   # expected: green
capture stills beside key art S1   # expected: warm key, long shadows, dusk sky; measured draw calls within budget
```

## Report
Write `runs/T56/REPORT.md`: what you built (paths), each acceptance command with its real output, anything you
could not do and the exact error, and decisions you made that the card did not specify.

## Result (2026-10-24)

The report text is here because `runs/` is never committed; `runs/T56/REPORT.md` holds the same text locally. The full
write-up, the budget table and the side-by-side with key art S1 are in `docs/research/DUSK-LIGHTING-T56-2026-10-24.md`.

**What was built.** `FArenaLayout::ApplyDuskLight` (`Greybox/ArenaLayout.cpp`) sets a low warm sun (pitch -20, yaw 165,
2.4 lux, (1.0, 0.50, 0.16)) with shadows on (45 m, 2 cascades), a rose-lilac sky fill, a five-band gold-to-lilac dome and
a warm height fog from 20 m to 50 m; `AArenaGreybox` (`Greybox/ArenaGreyboxActor.cpp`) opts the stands, wall, dais,
seats, braziers, banners and markers into shadows, sets the sun's shadow settings, spawns the fog and builds the banded
dome. Quest: `r.Shadow.CSM.MaxMobileCascades=2`, `r.Shadow.MaxCSMResolution=1024` (`Config/Android/AndroidEngine.ini`).
Test: `MageArena.Greybox.DuskLight` (`Tests/GreyboxTests.cpp`); `StyleSkin` also covers the fog. Desktop and Quest use
the same code path (the greybox is built at runtime; there is no map asset to change).

**Acceptance, real output.**
- `powershell -NoProfile -File apps/vr/tools/build.ps1`: `Result: Succeeded`.
- `Automation RunTests MageArena.`: `Found 294 automation tests based on 'MageArena.'`, 294 `Result={Success}`, `TEST
  COMPLETE. EXIT CODE: 0`. `MageArenaDesign`: 5/7, only `Duel.FireSeated` and `Session.FullSeated` red (DF-011).
- Captures beside key art S1 (outside the repo, `C:/Users/kazda/mage-art-staging/t56-dusk-lighting-2026-10-24/`):
  warm key, shadows about 2.7 times object height falling toward the player, gold-to-lilac sky. `capture-greybox.ps1`
  `capture: ok`, threat CIEDE2000 unchanged (max 2.20). `capture-budget.ps1 -Prefix after -Runs 3`: draw calls max 113
  (before 104), BasePass max 140,624 (before 139,232), live particles max 1,468, parity ok, `MAGEVR_BUDGET_DONE` in
  every run.

**Could not do / not done.** The rival and the session parts cast no shadow (they are built in `Session/`, outside this
card's paths). The sky is stepped bands, not a smooth gradient. Materials, crowd, VFX, hands and meshes are T57-T59 and
the mesh lane. No parity with S1 is claimed.

**Decisions the card did not specify.** (1) The preset is code applied over `arena-layout.json`'s lighting block,
because that file is a T54 golden overlay file (editing it fails `MageArena.Overlay.Golden`); the overlay sha256 is
unchanged. (2) A style greys the fog at its own luminance, as it already did the sun and sky fill. (3) The sun sits at
yaw 165 (ahead-left), as S1's glow is left of centre. (4) Fog is cut off at 50 m so the dome keeps its band colours.
(5) The dome bands sit in the seated camera's visible 10-29 deg window.
