# DF-004 - Bout 2's creatures cannot reach a seated player, so the hounds teach nothing

Found 2026-10-04 by T15 (`runs/T15/verify2/`, `runs/T15/verify3/`). Status: open, for the owner.

## Measurement
- Bout 2 (3 cinder hounds + 1 mire maw) is won from a seat at **33.05 s** on the live overlay (inside the pinned
  30-45 s), taking only **14 damage**, with **0 perfect absorbs**. With the DF-003 proposal: 27.30 s (outside).
- The plan (section 6, row 3:00-4:20) says the hound death bursts are "perfect practice", and `enemies.json` gives the
  hound `teaches: "perfect absorb on death bursts"`.

## Why
- The hound death burst (`onDeath.ember_burst`) is a **1.5 m** radius, 0.6 s delay, magic, at the hound's position.
  The hound bite has no reach beyond melee.
- DF-001 option A (owner, 2026-10-03) keeps every enemy body outside the dais footprint plus a 0.6 m standoff. The
  centre pad sits 2.9 m behind the dais front face, so the nearest a hound can die is about **3.5 m** from the
  player - more than twice the burst radius. The side pads are similar or farther.
- So the bursts never reach the seat: there is nothing to absorb, no perfect is possible, and the hounds' bites cannot
  land either. Bout 2 is decided by the mire maw alone. A scripted perfect was added (T15) and a unit test proves a
  well-timed ward on a burst that *does* reach the player scores a perfect - but in the real bout none arrive.

## Options (owner decides)
- **A. Ember spit (VR overlay):** a dying hound looses its ember burst as a slow magic projectile toward the player
  (same damage, magic family, ring telegraph), so it can be absorbed and perfected from the seat. The hound bite
  becomes a short leap at the dais lip that reaches the front pad only, or stays melee-only (hounds then threaten by
  pressure, not damage). Closest to the plan's teaching intent; the conscript throw (T10) is the precedent.
- **B. Larger burst radius (VR overlay):** ~4 m bursts reach the front pad when hounds die at the lip. Simple, but it
  depends on where the hound dies and does not reach the side pads reliably.
- **C. Let hounds onto the dais lip:** an exception to DF-001 option A for creatures. Reaches the player, but reopens
  the seated-threat geometry the owner closed.
- Orchestrator's recommendation: **A** - it keeps DF-001's geometry, keeps the burst readable as magic, and makes
  "perfect practice" true from every pad. Numbers would go through the calibration census like DF-003.
