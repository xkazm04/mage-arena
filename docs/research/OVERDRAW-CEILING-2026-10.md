# Overdraw and transparency ceiling, 2026-10

What the D-G4 particle row measures, set beside what a tile-based GPU pays for blended content. D-G4 judges
particles by count (`apps/vr/Game/Source/MageArenaVR/Budget/BudgetFx.h:13` `MaxLiveParticles = 2000`). Fill cost
is coverage times layers, not count. This file computes coverage for the six proxy rows and the blink vignette, and
proposes the ceiling rows the budget table does not have yet.

- Written 2026-10-22 against `master` at `c1b94cc`, applying ai-registry technique
  `overdraw-and-transparency-ceiling` (subject `standalone-headset-frame-budgets`).
- Desk phase: these are geometry estimates, not device readings. No row here is a Quest cost.
- This file changes no config, no code and no D-G4 verdict. The harness is
  `apps/vr/tools/budget/fill-estimate.mjs` (`node apps/vr/tools/budget/fill-estimate.mjs`).

## The question, chosen to falsify

The plan's particle row already says "additive, small screen coverage" (`docs/PROJECT-PLAN.md:366`), and the proxy
keeps sprites small "to respect small screen coverage" (`docs/research/NIAGARA-PROXY-2026-10.md` section 1.4). If
the count row tracked fill on this content, so that the rows that are most of the count are most of the fill and no
placement can move fill much at a fixed count, then a separate ceiling would be redundant here. The harness checks
that.

- **Target:** whether the count row ranks the proxy rows the way fill does, and how far fill moves at a fixed count.
- **Floor:** every D-G4 verdict and every count figure stays as measured (1,468 max live, pass). Nothing is changed.

## Method

Each live sprite is a camera-facing square of side `s` at distance `d` from the eye, drawn in full. Additive blending
gets no early rejection, so every covered pixel is shaded. Its share of one eye buffer is `(s/d)^2` over the
tangent-plane area of the view, `(2 tan 55 deg)(2 tan 48 deg)`. That uses the published Quest 3 field of view, 110 by
96 degrees, which is an assumption. The sum of shares is the mean number of blended layers per pixel. It does not
depend on resolution. Sizes are uniform over each row's range (`BudgetFx.cpp` `FxRows`).

Distances come from the tree:

| Row | Placed (`BudgetStressDriver.cpp` `SpawnFx`) | Distance from the eye |
|---|---|---|
| WardAbsorb, PerfectFlash | pad + 0.6 m forward, + 1.1 m up; eye at pad + 1.2 m (`arena-layout.json` `seatedEyeHeightAbovePadM`) | 0.61 m |
| BoltTrail | attached to projectile 0, near-ring lane ending 0.6 m forward, 0.7 m left, at chest 1.35 m (`BudgetStressPlan.cpp`) | 0.93 m at arrival; the trail spans the last 6 m (12 m/s, 0.5 s life) |
| SigilTrail | right index tip; 1 cm ribbon, taken as a 30 cm stroke | about 0.45 m (assumption) |
| Motes | pad + 2.5 m forward, + 1.3 m up | 2.5 m |
| Telegraph | the farthest enemy | 15 m |

## Result

| Row | Live | Share of count | Mean layers | Share of fill |
|---|---|---|---|---|
| WardAbsorb | 120 | 8.2% | 0.191 | 59.6% |
| PerfectFlash | 60 | 4.1% | 0.095 | 29.8% |
| BoltTrail, at arrival | 300 | 20.4% | 0.027 | 8.5% |
| Motes | 600 | 40.8% | 0.0035 | 1.1% |
| SigilTrail | 90 | 6.1% | 0.0023 | 0.7% |
| Telegraph | 300 | 20.4% | 0.0008 | 0.2% |
| **Proxy total** | **1,470** | | **0.32** | |
| Blink vignette, closed | 4 quads | not counted | **0.955** | |

**The falsifier did not fire.**

- **The count row ranks the rows backwards.** The two rows at palm height are 12% of the count and 89% of the fill.
  Motes and the telegraph are 61% of the count and 1.3% of the fill.
- **Placement moves fill at a fixed count.** Swap the telegraph's and the ward's positions and the count is still
  1,470, but mean fill goes from 0.32 to 0.61 layers. At its worst the count row allows 2,000 sprites at 8 cm at the
  palm point, which is 5.5 mean layers. The count row reads both as a pass.
- **The largest blended layer is outside the row.** The proxy's sixth system "stands for" the ambient motes and the
  blink vignette's budget (`NIAGARA-PROXY-2026-10.md` section 1.4, row 6). Fully closed, the vignette's four quads
  (`apps/vr/Game/Source/MageArenaVR/Hands/MageArenaPawn.cpp` `UpdateVignetteQuads`, inner half-angle 15 degrees)
  cover 95.5% of each eye buffer. That is three times the whole proxy. The motes row it stands in for adds 0.0035.
  The blink's camera fade (`StartFade`) is a further full-screen layer on top of it.
- **The peak sits where the player looks.** The ward and flash sprites sit at the palm point, where the player looks
  while drawing. If they spread over a 15-30 cm radius, that region carries about 2.4 to 9.5 layers (the burst
  templates' real spread is not readable from source). The view-wide mean is 0.32.

## Proposed ceiling rows (owner decision points)

These rows sit beside the count rows. They do not replace them, because count still drives simulation cost on the
game thread. None of them is decided. The numbers are for the V1 capture to set.

| Row | Proposed | Status |
|---|---|---|
| Blended layers, mean over the view | a number per busy moment, read from the harness on the desk and from a GPU capture at V1 | **open**: set at V1 |
| Blended layers, peak over the gaze region (palm and sigil area) | a number per content class; ward, flash and sigil share it | **open**: set at V1 |
| Effect distance | each effect class is costed at its nearest legal distance to the eye, not at the distance where it was authored | **open**: the ward's 0.6 m and the bolt's 0.93 m arrival are the current nearest points |
| Comfort overlays | the vignette and the camera fade count in the ceiling, and a blink is costed on a busy frame | **open**: needs the V1 check of `StartFade` with `r.MobileHDR=False` (FORWARD-ALLOWANCE, post-pass row) |
| PerfectFlash sprite | the "1 flash sprite" of the table has no authored size; a flash is usually the largest sprite of a duel | **open**: size it before the session presentation replaces the proxy |

## What this cannot show

- **No pixel was shaded on a headset.** The table shows where fill sits and how the count row misranks it. It does
  not price any of it. A GPU capture of the stress scene at V1, with the vignette closed on one frame, is what
  would turn these into costs.
- **The model counts full quads.** Additive sprites with mostly empty texture still cost their full quad, so this is
  the right measure for them. It ignores the bolt's fountain velocity and the bursts' spread, which move the peak
  but not the ranking.
- **The proxy gathers each effect at one spot** (`NIAGARA-PROXY-2026-10.md` section 1.4, "screen coverage differs from
  the game"). The ceiling rows are what lets the session presentation be judged on coverage when it replaces the proxy.
