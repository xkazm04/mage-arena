# T49 - The enemy HP bar stops using the reserved threat pair, and the unblockable red is defined once

Status: builder work done on Wolf 2026-10-24 (branch `autopilot/accepted-idea-delivery-ec553e8a`); **OWNER: colour sign-off
pending** from the frames in the report below. Not closed, and no WOLF-PLAN box is ticked, until the owner signs. (Written
2026-10-21 on Lynx, where no Unreal install and no renderer exist - `docs/DECISIONS.md` 2026-10-20 "Unreal host".)
Max turns: 150

## Goal
Nothing but the unblockable threat and its telegraph pairs a near-black body with a red rim, and the unblockable red lives in
one place. Owner ruling 2026-10-21: the enemy HP bar's colour is a violation of the threat language and is to be fixed.
Repo law (`CLAUDE.md`): element colour = absorb, steel = dodge, black core with a red rim = leave, and that pair is reserved
for the unblockable and its telegraph. Today every enemy carries a near-black bar back behind a red fill, which is that pair.
The same pair is copied into the stress proxy, and the reserved red is hand-typed in several files with two different values.

## Read first
- `CLAUDE.md`, `docs/DECISIONS.md` (they outrank this card if they conflict)
- `docs/ORCHESTRATION.md` role table: only the owner certifies what the eyes judge. This card produces evidence for the owner;
  it does not certify its own colours.
- Code (symbols are the anchor; line numbers are where they stood at the 2026-10-24 refresh, `d4bbdaf` plus the bead fix):
  - **2026-10-24 refresh.** Since `6bb7f44` (T46) the HP colours live only in `Session/PresentationLook.cpp`: the
    file-scope `HpBack(0.02f, 0.02f, 0.02f)` and `HpFill(0.65f, 0.08f, 0.05f)` (:26-27), read through
    `PresentationLook::HpBackColour()` and `PresentationLook::HpFillColour()` (declared in `Session/PresentationLook.h`
    :28-30). The colour values did not change; only their home did.
  - `Session/SessionPresentation.cpp`: the enemy body builder sets `Created.HpBack = MakePart(TEXT("Cube"),
    PresentationLook::HpBackColour())` and `Created.HpFill = MakePart(TEXT("Cube"), PresentationLook::HpFillColour())`
    (:191-192). The same `HpFillColour()` also colours the player cuff `HpBar` (:533) - decide on purpose whether that one
    changes too (Deliverable 1).
  - `Budget/BudgetStressPlan.cpp`: since T46 the stress plan keeps no copy; its census reads
    `PresentationLook::HpBackColour()` / `HpFillColour()` (:186-187), and the stress capture draws the real
    `ASessionPresentation`. The old `BudgetStressDriver.cpp` `HpBackColour` / `HpColour` copy is gone.
  - The reserved pair: `Session/ElementColour.cpp` `Palette.UnblockableBody (0.006995, 0.006995, 0.008023)` and
    `Palette.UnblockableRim (0.745404, 0.014444, 0.009134)` (:51-52), read back through `ActiveElementPalette()`.
  - Other typings of the unblockable red found on 2026-10-21: `apps/vr/data/vr/arena-layout.json` (threat of kind
    `unblockable`, `rim` and `telegraph`, :204-205); `Budget/BudgetFx.cpp` `RimRedColour(0.8f, 0.02f, 0.02f)` (:33, **a
    different value** from the layout's 0.745404/0.014444/0.009134); and the test oracle `Tests/ColourTests.cpp` `Red`
    (:26), which is meant to stay a literal. The style files `apps/vr/data/vr/styles/style-13.json`, `style-25.json` and
    `style-30.json` set `unblockableRim` on purpose as per-style overrides of the palette; they are not copies to remove.
  - Judged not violations, leave alone: `MageFireBody (0.30, 0.06, 0.03)` and `HoundBody (0.28, 0.07, 0.02)` are dark
    maroon, not near-black; `SignPlateColour` and `GlyphPlateColour` are near-black with no red.
- `Budget/BudgetFx.cpp` `BudgetStress::FxRows()`: the `NS_Budget_Telegraph` row uses `RimRedColour`. The Niagara asset
  `Content/Budget/NS_Budget_Telegraph.uasset` is **baked from that table** by `apps/vr/tools/create-budget-fx.ps1`.
- Tests: `Tests/GreyboxTests.cpp` (`IsBlackCore`, `IsRedRim`, the layout threat checks), `Tests/ColourTests.cpp`.

## Deliverables (exact paths)
1. `Session/PresentationLook.cpp` (the HP colours' one home since the 2026-10-24 refresh; `Session/SessionPresentation.cpp`
   only reads them): the enemy HP bar (back and fill) no longer pairs near-black with red. Pick a scheme that
   stays a **readable health bar** at the seated viewing distance (the bar is 70 x 8 x 8 cm over the enemy, seen from the
   dais) and that collides with neither the element palette (water turquoise, fire ember orange, air green, earth) nor the
   steel/dodge cue (white/grey). Keep clear contrast between back and fill; the back must not be near-black, and no part of
   the bar may be a red rim on a dark core. State the colours chosen and why in the report. Decide and state whether the
   player cuff `HpBar` (`SessionPresentation.cpp` :533, same constant) keeps its red; it is not a black pair, but check it against the law too.
2. `Budget/BudgetStressDriver.cpp`: the stress proxy's HP colours are the same constants as the game's, not a second copy, so
   they cannot diverge (move them to one shared header, for example next to `ElementColour.h`, or read the game's).
   (2026-10-24 refresh: already met by T46 - `Budget/BudgetStressPlan.cpp` reads `PresentationLook`; verify, do not redo.)
3. Single-source the unblockable red: `FElementPalette` (`ElementColour.h/.cpp`) is the one definition in code.
   `BudgetFx.cpp` reads `ActiveElementPalette().UnblockableRim` (or the palette default) instead of `RimRedColour`;
   reconcile 0.8/0.02/0.02 against 0.745404/0.014444/0.009134 in favour of the palette value and say so. For the layout
   JSON, either keep it and add a test that its unblockable `rim` and `telegraph` equal the palette default within 1e-4, or
   make the loader fill them from the palette. Do not edit `apps/vr/data/**` by hand in this card; report what the owner must
   change if a data edit is needed.
4. Re-bake the telegraph asset: run `powershell -NoProfile -File apps/vr/tools/create-budget-fx.ps1 -Force` (the script is
   otherwise idempotent and would leave the old red in `NS_Budget_Telegraph`). Leave the rebuilt `.uasset` files in the work
   tree (binary assets go through Git LFS) and quote the commandlet output. If only the telegraph asset differs, say the other
   five rebuilt identical.
5. The guard (cheap version): a test in `Tests/GreyboxTests.cpp` (or `Tests/ColourTests.cpp`) asserting the arena layout has
   **exactly one** threat whose body satisfies `IsBlackCore` and whose rim satisfies `IsRedRim`, and that the enemy HP bar
   colours the game builds satisfy neither (expose the HP colours from the shared constants so the test can read them). Also
   assert the equality in Deliverable 3 so the red cannot drift again.
6. Evidence for the owner: one capture frame showing an enemy **with its HP bar** and an **unblockable telegraph** in the same
   image. Use the `apps/vr/tools/capture-wave1.ps1 -Run T10` threat-geometry stills if one already frames both; otherwise
   extend that capture (and say how) so one still does. Also keep a before frame of the same view from the previous commit.
   Save under `runs/T49/shots/` and name the files in the report. **The owner certifies the colours** from those frames; the
   report lists what was captured and must not claim the bar "reads well" or "looks right".

## Constraints
- Do not commit, push, or touch any repository other than this one.
- Do not call paid services. Do not edit `docs/DECISIONS.md`.
- Never edit `apps/vr/data/pinned/**`. No combat number changes: this is colour only.
- Greybox only: flat colours, no textures, no art pass. Forward shading stays on.
- Do not change `MageFireBody`, `HoundBody`, `SignPlateColour`, `GlyphPlateColour`, or the style files' `unblockableRim`.
- Do not change the draw-call count: the HP bar stays two parts per enemy (D-G4 is one total of 150, see T46).

## Acceptance (the orchestrator re-runs these; quote their real output in your report)
```
powershell -NoProfile -File apps/vr/tools/build.ps1                       # expected: editor target builds
<UE automation: MageArena.Colour and MageArena.Greybox>                   # expected: pass, including the new exactly-one-threat and red-equality tests
<UE automation: the MageArena suite>                                       # expected: no new failure beyond the known list in CLAUDE.md
powershell -NoProfile -File apps/vr/tools/create-budget-fx.ps1 -Force     # expected: six systems rebuilt, NS_Budget_Telegraph carries the palette red
node apps/vr/tools/check-pin.mjs                                           # expected: OK (no combat data touched)
```
Then the frames in Deliverable 6. They are a hand-off, not a pass: the owner's look is the acceptance for the colour itself.

## Report
Write `runs/T49/REPORT.md`: what you built (paths), the HP colours chosen and why, each acceptance command with its real
output (run on Wolf: host name and engine build), the frames for the owner (paths), the decision on the cuff `HpBar`, anything
you could not do and the exact error, and decisions you made that the card did not specify.

## Report (2026-10-24, Wolf)

The same text is in `runs/T49/REPORT.md` on Wolf, next to the frames. Neither is committed: `runs/` is never committed
(`tools/check-repo-law.mjs`).

Run on Wolf: host `kaziho` (x64), UE 5.8.3 (Changelist 58210709, `++UE5+Release-5.8`), 2026-10-24. Branch
`autopilot/accepted-idea-delivery-ec553e8a`, base `8c36813`. MetaXR plugins copied (robocopy /E) from the main checkout.

**Colour sign-off - OWNER: pending.** This report lists what was built, measured and captured. It does not judge how the
bar looks. The owner certifies the colours from the frames below.

### What was built

| Commit | Paths | What |
|---|---|---|
| `a7cd3ac` | `Session/PresentationLook.cpp`, `Session/PresentationLook.h` | HP back and fill leave the near-black/red pair |
| `a2fdb37` | `Budget/BudgetFx.cpp` | `NS_Budget_Telegraph` reads `FElementPalette::Greybox().UnblockableRim`; `RimRedColour 0.8/0.02/0.02` is gone |
| `512f8db` | `Tests/GreyboxTests.cpp` | `MageArena.Greybox.ThreatPairReserved`, `MageArena.Greybox.UnblockableRed` |
| `0409617` | `Content/Budget/NS_Budget_*.uasset` (6, LFS) | re-baked from the palette-reading table |

(All source paths are under `apps/vr/Game/Source/MageArenaVR/`.)

Deliverable 2 was already met by T46, and I verified it instead of redoing it. `Budget/BudgetStressPlan.cpp:186-187` adds the census
rows from `PresentationLook::HpBackColour()` / `HpFillColour()`. `BudgetStressDriver.cpp` has no HP colour of its own
(`grep HpBack|HpColour` finds only the PresentationLook include and `LoadStyleFromCommandLine`).

### HP colours chosen, and why

| Part | Before (8c36813) | After (linear) | about sRGB |
|---|---|---|---|
| back | 0.02, 0.02, 0.02 (near-black) | **0.12, 0.04, 0.10** (dusky plum) | 97, 56, 89 |
| fill | 0.65, 0.08, 0.05 (red) | **0.80, 0.05, 0.60** (magenta) | 231, 63, 203 |

- The hue is magenta because every other one is taken: water turquoise, fire ember orange, air wind green and earth ochre
  are element cues. Steel grey/white is the dodge cue and red is the leave cue. The cuff already uses mana blue
  (0.08, 0.25, 0.75) and clock gold (0.75, 0.55, 0.12), and the Wardstones glow Tithe violet (0.50, 0.26, 0.95), so I
  ruled those out too.
- The back is a dark tone of the fill, so it should read as an empty bar. Its brightest channel is 0.12, against
  `IsBlackCore` (< 0.08 on every channel), and the guard test holds it at >= 0.10.
- The fill/back luminance ratio (WCAG formula on linear values) is about 2.7; the guard test holds it at >= 2.5.
- Still two parts per enemy, both `Cube`, so the draw-call count does not change (D-G4).

**Cuff `HpBar` (`SessionPresentation.cpp:533`): it changes with the fill, on purpose.** It reads the same
`HpFillColour()`, and I left it that way. The old red (0.65, 0.08, 0.05) was not a black pair, but it sat near the reserved rim
red and next to the Fire ember, right by the player's hand. One HP colour on the field and the wrist keeps "magenta =
HP" a single rule. The cuff has no back part, so nothing on it is near-black.

### Unblockable red, single source

- `FElementPalette::Greybox().UnblockableRim = (0.745404, 0.014444, 0.009134)` is the one definition in code. The palette
  value wins over the old `BudgetFx.cpp` copy (0.8, 0.02, 0.02). `ActiveElementPalette()` is not used for the bake:
  the bake writes one asset, so it takes the greybox default rather than a per-process style override.
- I did not edit `apps/vr/data/vr/arena-layout.json` (no hand edits to data). `MageArena.Greybox.UnblockableRed` holds its
  unblockable `rim`, `telegraph` and also `body` to the palette within 1e-4, and holds the D-G4 telegraph row to the
  palette rim within 1e-6. **No data edit is needed:** the layout already carries the palette values.
- `Tests/ColourTests.cpp` `Red` stays a literal (untouched). The style files' `unblockableRim`, `MageFireBody`,
  `HoundBody`, `SignPlateColour` and `GlyphPlateColour` are untouched.

### Acceptance (real output)

**Build** - `powershell -NoProfile -ExecutionPolicy Bypass -File apps/vr/tools/build.ps1` on the final tree:
```
Result: Succeeded
Total execution time: 1.59 seconds
```
(The first cold build in this worktree: `Result: Succeeded`, `Total execution time: 470.09 seconds`. The build that
compiled the colour change: `[1/5] Compile [x64] BudgetFx.cpp`, `[2/5] Compile [x64] PresentationLook.cpp`, `Result:
Succeeded`.)

**New tests, red first** - with 8c36813's `PresentationLook.cpp` and `BudgetFx.cpp` put back in the work tree
(touched, rebuilt), `Automation RunTests MageArena.Greybox`:
```
Success MageArena.Greybox.Layout
Success MageArena.Greybox.ThreatColours
Fail MageArena.Greybox.ThreatPairReserved
Fail MageArena.Greybox.UnblockableRed
Error: Expected 'HP colour (R=0.020000,G=0.020000,B=0.020000,A=1.000000) is not a black core' to be false.
Error: Expected 'HP back is not near-black' to be true.
Error: Expected 'HP fill is not the cue colour (R=0.550000,G=0.260000,B=0.035000,A=1.000000)' to be true.
Error: Expected 'HP fill is not the cue colour (R=0.745404,G=0.014444,B=0.009134,A=1.000000)' to be true.
Error: Expected 'HP back is not the unblockable core' to be true.
Error: Expected 'NS_Budget_Telegraph colour (R=0.800000,G=0.020000,B=0.020000,A=1.000000) is the palette's' to be true.
```
The old fill (R 0.65) slipped under `IsRedRim` (R > 0.65) by a hair. The distance check is what caught it.

**MageArena.Greybox** (branch, one editor process, `-nullrhi`): 4/4 pass, editor exit 0.
```
Success MageArena.Greybox.Layout
Success MageArena.Greybox.ThreatColours
Success MageArena.Greybox.ThreatPairReserved
Success MageArena.Greybox.UnblockableRed
```
**MageArena.Colour**: 5/5 pass, editor exit 0 (CreatureSchools, HoundEmber, Schools, StyleOverride, Unblockable).

**Full suite** - `Automation RunTests MageArena` (both MageArena. and MageArenaDesign.), sigil corpus regenerated first
(`generate.mjs --corpus 30 --seed 1000`, `mousepaths.mjs`), 08:16:58-08:22:49 local, editor exit 255 (as in the
baselines):
```
267 tests: 264 Success, 3 Fail
Fail MageArenaDesign.Duel.AirSeated
Fail MageArenaDesign.Duel.FireSeated
Fail MageArenaDesign.Session.FullSeated
```
These are exactly the three known failures in CLAUDE.md (DF-011). No new failure. Per-test list:
`runs/T49/automation/full/results.txt`.

**Re-bake** - `powershell -NoProfile -File apps/vr/tools/create-budget-fx.ps1 -Force` (with `-Run T49/budget-fx`):
```
create-budget-fx: running the CreateBudgetFx commandlet -force
MAGEVR_BUDGET_FX NS_Budget_BoltTrail emitters=1 sim=cpu peak=300 inputs=10 -> .../Content/Budget/NS_Budget_BoltTrail.uasset
MAGEVR_BUDGET_FX NS_Budget_SigilTrail emitters=1 sim=cpu peak=90 inputs=10 -> .../Content/Budget/NS_Budget_SigilTrail.uasset
MAGEVR_BUDGET_FX NS_Budget_WardAbsorb emitters=1 sim=cpu peak=120 inputs=9 -> .../Content/Budget/NS_Budget_WardAbsorb.uasset
MAGEVR_BUDGET_FX NS_Budget_PerfectFlash emitters=1 sim=cpu peak=60 inputs=5 -> .../Content/Budget/NS_Budget_PerfectFlash.uasset
MAGEVR_BUDGET_FX NS_Budget_Telegraph emitters=1 sim=cpu peak=300 inputs=10 -> .../Content/Budget/NS_Budget_Telegraph.uasset
MAGEVR_BUDGET_FX NS_Budget_Motes emitters=1 sim=gpu peak=600 inputs=11 -> .../Content/Budget/NS_Budget_Motes.uasset
MAGEVR_BUDGET_FX done created=6 unchanged=0 failed=0
create-budget-fx: ok
```
(The OpenXR-Loader "failed to find active runtime" lines above it are the usual no-headset noise.) All six `.uasset` files
changed bytes. To tell whether the other five changed in content, I ran `-Verify` (dumps every parameter of the saved
systems) on the rebuilt assets and on 8c36813's assets, and diffed the two 506-line dumps. They differ in **one line**:
```
< MAGEVR_BUDGET_DUMP NS_Budget_Telegraph param Constants.Fountain.InitializeParticle.Color : LinearColor = 0.8,0.02,0.02,1
> MAGEVR_BUDGET_DUMP NS_Budget_Telegraph param Constants.Fountain.InitializeParticle.Color : LinearColor = 0.745404,0.014444,0.009134,1
```
So the other five rebuilt parameter-identical. Their bytes still differ, from the re-save alone, so all six are committed.

**check-pin** - `node apps/vr/tools/check-pin.mjs`:
```
pin OK: 9 files @ baeac66
conformance OK: 45 vectors
```

**Lint** - `node apps/vr/tools/lint/unused-members.mjs && node apps/vr/tools/lint/format-check.mjs --base 8c36813`:
```
unused-members OK: 563 private/protected members in 39 classes under apps\vr\Game\Source
format-check OK: 4 changed C++ file(s) since 8c36813 (clang-format version 19.1.5)
```
The other merge-gate tests (agy-guard, check-pin --self-test, check-overlay and --self-test, leonardo-redaction,
check-repo-law) all exit 0.

### Frames for the owner

All in `runs/T49/shots/` on `kaziho`, in this worktree. The folder is not committed: repo law (`tools/check-repo-law.mjs`,
never-committed) forbids tracking `runs/`.

| File | What |
|---|---|
| `before-8c36813-05-tempest-lance-line.png` | 8c36813 colours: Lio (the air mage) with the HP bar over the column, and Lio's Tempest Lance line telegraph (unblockable: black core, red rim) in the same frame. The cuff's red HP bar is bottom left |
| `after-05-tempest-lance-line.png` | the same view on this branch |
| `supplementary-after-T10-02-spear-flight.png` | branch only: Wave 1 conscripts with their HP bars (no unblockable in Wave 1) |

How they were captured: `apps/vr/tools/capture-air.ps1` **unmodified** (the T23 stills), still 5 of 5, which grants Lio
`air_tempest` and shoots at half its windup, facing Lio. Log line, the same in both runs:
`MAGEVR_AIR shot air_tempest windup 36 of 72 ticks lio=(11.80, 14.23) player=(8.38, 7.02) projectiles=0`, then
`MAGEVR_CAPTURE_DONE shots=5`. The before frame used the 8c36813 colours (`PresentationLook.cpp` and `BudgetFx.cpp` as at 8c36813). That binary also held a
capture-driver change I later reverted (below). It does not touch any drawn colour.

Pixels sampled on the bar over Lio: before (43, 43, 43); after (97, 59, 89). Both are the **back** colour (see the first finding).

### What I could not do, and decisions the card did not specify

1. **Finding (pre-existing, outside this card's paths): the enemy HP fill is never visible.** `SessionPresentation.cpp`
   (:1409-1419) sizes the back `HpBarLengthCm x 8 x 8` and the fill `(70 x fraction) x 8 x 8` at the same centre line.
   The fill therefore sits entirely inside the back box, and its only coplanar faces z-fight. In all three frames the bar
   over every enemy shows the back colour only, at full HP and damaged alike. Under the old colours this means the enemy
   bar showed as a plain near-black bar. This card's colour change does not fix readability on its own. A fix without a
   draw-call change is to make the fill slightly thicker than the back (or the back slightly thinner), so the fill shows
   on the side facing the seat. That edit is in `SessionPresentation.cpp` (or new constants in `PresentationLook.h`
   that it reads), which is outside this run's paths. I am asking for it, not making it.
2. **The T10 Wave 1 capture cannot frame the evidence.** Wave 1 has only physical threats (conscript spears), so no T10
   still holds an unblockable. I first extended `Wave1Capture.cpp` with a `-Run T49` (the seated Fire duel, shot on the
   first opponent unblockable). In that duel the only unblockable is Brennic's Sunfall. When he casts it, Brennic
   stands at kernel-mapped X -17.07 m, outside the 16 m ring wall (`arena-layout.json` `radiusXM 16`) and inside the
   stands geometry, about 5.4 m behind the seat (log: `MAGEVR_T49_AIM kind=area eye=X=-1162.500 ... bar=X=-1707.148`).
   No seated view can show him, so I reverted that extension (net zero change to `Wave1Capture.cpp` and
   `capture-wave1.ps1`) and used the existing T23 air capture, which already frames both.
   **Second finding:** in the seated Fire duel, Brennic can stand outside the visible arena wall when he casts his signature.
3. The re-bake was run with `-Run T49/budget-fx`, so its log went to `runs/T49/budget-fx/` and not the default `runs/D-G4-fx/`.
4. The guard tests went into `GreyboxTests.cpp`, so `IsBlackCore`/`IsRedRim` are shared with the existing layout checks. Beyond
   the card I added three checks: the fill keeps a distance from every cue colour, the back is not near-black (>= 0.10),
   and the luminance ratio is >= 2.5. These are greybox guards against drift, not a certification of the look.
5. No WOLF-PLAN box is ticked. The card stays open until the owner signs off the colours.
