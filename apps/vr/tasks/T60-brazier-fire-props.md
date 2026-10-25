# T60 - braziers as fire props

Status: open
Max turns: 120

## Goal
The four arena braziers read as fire props, closer to key art S1: an iron bowl that reads as a bowl on its stand, and a
soft, slowly licking flame in an environment colour of its own. Today the bowl is a plain cylinder and the flame a lit
cone in the style's `projectiles.fire`, which is the fire spell's threat colour (absorb): a prop in the corner of the eye
must never read as an incoming fire bolt, an absorb cue or the unblockable's red rim. Athena's ruling on 7fd7d11
(`docs/DECISIONS.md`, 2026-10-25) names this card as the last item in the arena order before the 31 Oct desktop core
gate. No combat number changes.

## Read first
- `CLAUDE.md`, `docs/DECISIONS.md` (they outrank this card if they conflict)
- `docs/research/SPELL-VFX-T58-2026-10-25.md` (the one-draw-per-colour glow layer, `M_SpellGlow`)
- `docs/research/CROWD-T57-2026-10-25.md` (the report shape, the D-G4 re-run and the switch pattern)
- `docs/research/DRAWCALL-BUDGET-2026-10-25.md` (D-G4 with the creature scratch)
- `apps/vr/Game/Source/MageArenaVR/Greybox/ArenaGreyboxActor.cpp` (`BuildProps`), `Greybox/ArenaLayout.cpp` (`ApplyStyle`)
- `apps/vr/art/key-art/key-art-s5101-medium.jpg` (S1)

## Deliverables (exact paths)
- `apps/vr/Game/Source/MageArenaVR/Greybox/` - the bowl as a bowl on the existing brazier ISM (no new draw component,
  shadows as today); the flame as one instanced glow layer for all four braziers (at most 1 to 2 draw components) with a
  slow, irregular flicker; the switch `-MageArenaNoBrazierFire`, logged like `MAGEVR_ARENA_CROWD`.
- The flame colour derived in code as an environment colour (amber or ochre, dimmer than every threat glow), never read
  from `projectiles.fire` or `arena.flame`; the `projectiles.fire` override removed with a comment saying why.
- `apps/vr/Game/Source/MageArenaVR/Tests/` - `MageArena.Greybox.BrazierFlameNotThreat`: the flame colour is out of every
  threat class by chromaticity under the greybox palette and every shipped style.
- `docs/research/BRAZIERS-T60-2026-10-25.md` - the report in the T57 report's shape.

## Constraints
- Do not push, or touch any repository other than this one.
- Do not call paid services. No image generation.
- No combat number changes: `apps/vr/data/PINNED.json`, `apps/vr/data/pinned/` and `apps/vr/data/conformance/` are
  never edited. `arena-layout.json` changes only if a bowl-shape field is truly needed.
- The threat colour language stays: element colour = absorb, steel = dodge, black core with a red rim = leave.
  `MageArena.Colour.*`, `Greybox.ThreatColours`, `Greybox.ThreatPairReserved` and `ThreatDemo` stay unchanged and green.
- The flame's flicker is a slow, broad lick, distinct from T58's ember trail (no rising sparks).
- Budget: D-G4 max at most 130 (target at most 128) from 126 today; live particles plus puffs at most 2,000. Over either
  hard limit: stop and report instead of merging.
- One Unreal process at a time on this machine. Blender and ComfyUI stay down. New `.uasset` files only through Git LFS.
  Nothing TRELLIS-derived is committed.

## Acceptance (the orchestrator re-runs these; quote their real output in your report)
```
powershell -NoProfile -File apps/vr/tools/build.ps1   # expected: Result: Succeeded on the commit of record
Automation RunTests MageArena. -nullrhi   # expected: 306 + the new tests, all Success, EXIT CODE: 0
capture-budget.ps1 (D-G4, three 60 s runs before and after, creature scratch in)   # expected: max <= 130 (target <= 128); -MageArenaNoBrazierFire parity hash reported
blind critic on shuffled before/after stills   # expected: NO on (b) a brazier flame mistaken for a fire-spell threat, an absorb colour or the red rim, and NO on (c) a threat colour or silhouette regressed, in the final round
```

## Report
Write `docs/research/BRAZIERS-T60-2026-10-25.md` in the T57 report's shape (status line, outcome, how it was run,
file:line change table, budget, critic, every critic round recorded). Set this card's status to done and note in
`docs/concepts/VISUAL-PLAN.md`'s arena order that the braziers are done.
