# DF-001 - The seated player loses Wave 1 with the pinned numbers

Found 2026-10-03 by T08 retry 1 (`runs/T08/REPORT.md`, `runs/T08/wave1-chain.log`). Test: `MageArenaDesign.Session.Wave1Seated`
(kept red on purpose until the design changes; the regular `MageArena.*` suite stays green).

## Measurement
- Seated, no walking, kernel player pinned to the active pad (drift 0), all gestures through the clip pipeline.
- **Lost at 16.45 s** (the pinned window for this wave is 25-40 s). Damage taken 95 (whole pool), dealt 52.3 of the wave's 208 HP.
- With walking/kiting (T08 first run) the same wave was won at 37.63 s - the numbers were calibrated for a walking game.

## Why (from the chain)
- Pads are 3 m apart; a conscript's reach is 2.0 m + 0.38 m radius = 2.38 m. A pad snap does not leave a pack that is already on the pad.
- Blink = the pinned roll: 25 stamina, 0.25 s i-frames, recovery holds the next roll until 0.50 s after the last; stamina 70, regen 20/s after a 0.8 s delay. Roughly one protected blink per ~1.25 s - not enough for four conscripts.
- The ward reduces physical hits only to 70 % (spear 10 -> 7, stone 7 -> 4.9); Wave 1 has no magic, so no perfect absorbs.
- Casting a sigil occupies the hands, so ward and blink are locked out during the clip.

## Options (owner decides; the shared rules stay owned by the desktop/TV channel)
- **A. Seated threat geometry (VR overlay `combat.vr.json`, plan section 3):** enemies stay on the arena floor below the raised
  dais and attack from range (spears thrown, stones slung); melee reach never touches the dais. Threats come to you; you ward,
  blink and cast. Matches the plan's raised-dais fiction. Shared kernel rules unchanged; VR overlay only.
- **B. Seated defence numbers (VR overlay):** stronger physical ward reduction, cheaper/longer blink i-frames, pads further apart.
- **C. VR wave composition:** fewer conscripts, more slingers in the competition slice.
- Orchestrator's recommendation: **A**, with C as fine-tuning, then re-measure; B only if A+C are not enough.
