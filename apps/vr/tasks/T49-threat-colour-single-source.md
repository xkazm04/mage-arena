# T49 - The enemy HP bar stops using the reserved threat pair, and the unblockable red is defined once

Status: open (needs Wolf: UE 5.8.3 editor, `create-budget-fx.ps1` and a capture script; written 2026-10-21 on Lynx, where
no Unreal install and no renderer exist - `docs/DECISIONS.md` 2026-10-20 "Unreal host")
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
