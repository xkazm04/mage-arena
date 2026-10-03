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

## Decision
Owner, 2026-10-03: **option A** - enemies fight from the arena floor; nothing reaches the dais. Implemented as the VR overlay in card T10.

## After option A (T10, 2026-10-03)
Implemented as the VR overlay `apps/vr/data/vr/combat.vr.json` (enemies held off the dais, conscripts throw; shared rules and
the 45 TS vectors unchanged). Re-measured from a seat: **still lost, now at 11.42 s** (thrown spears reach a seated player
sooner than a lunge). Two gaps, per `runs/T10/REPORT.md`:
- **survival:** ~8.3 HP/s incoming against a 95 HP pool (four conscripts' spears connect about every 2.1 s each);
- **clear rate:** ~4.2 HP/s dealt from a seat (a drawn sigil takes ~2 s and occupies the hands), so the 208 HP wave needs
  ~49 s even if the player lived - above the 25-40 s window.
Standoff distance alone cannot close it; throw cadence alone only stretches survival; wave composition (option C) is the knob
that moves the clear time. The seated loop needs a balance pass of its own - see the calibration card that follows.

## After T11 (split hands + planted staff, 2026-10-03)
Lost at **23.25 s** (was 11.42 s): dealt 106.7 of 208 (4.59 HP/s, a ~45 s clear at that rate), took 95. The dome doubled
survival; damage rate barely moved. The scripted player split-cast once: the palm ward drains 27 mana/s at Nerve 1, so
warding while casting is nearly unaffordable. Next: T12 calibration census (proposal for owner sign-off).
