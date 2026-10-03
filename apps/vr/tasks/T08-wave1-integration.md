# T08 - Wave 1 end to end: gestures drive the kernel, the kernel drives the greybox, the cuff shows the clock

Status: done after retry 1 - integration verified; seated Wave 1 loses (design finding DF-001, owner decision)
Max turns: 320

## Goal
Gate M2 (Sun 18 Oct, plan section 5 D2 row): **Wave 1 plays end to end in greybox on clips and mouse alone.** Every part
exists separately - hand clips and keys (T02), sigil recognition (T03), the palm ward (T04/T04b), the greybox arena and
desktop pawn (A02), the C++ kernel with AI and bouts (T06/T07), the cuff HUD art (A08). This card connects them into
one playable loop and adds the last missing gesture, the **blink** (finger flick toward a pad).

## Read first
- `CLAUDE.md`, `docs/DECISIONS.md`, `docs/DESKTOP-INPUT.md` (the one-pipeline rule), `docs/PROJECT-PLAN.md` sections 3
  (VR kernel extension: blink + pads is a change request to the rule-owning channel), 6 (body and space, blink,
  session table: Wave 1 = soldiers), `apps/vr/tasks/BACKLOG.md`
- Code: `Hands/*`, `Gestures/*`, `Greybox/*`, `Kernel/*`, `Combat/*`; data `apps/vr/data/vr/arena-layout.json`;
  art `apps/vr/art/hud/` (A08 cuff states) and `apps/vr/art/glyphs/` (A03)

## Deliverables
1. **Blink detector** (`Gestures/BlinkDetector.*`): casting-hand pointing pose plus a wrist/index angular-velocity spike;
   the flick direction resolves the target pad (left / centre / right, relative to the active pad). Driven by the
   existing `blink-left/right/back` clips through the hand pipeline (A/D/S keys) - the F5-F7 debug blink stays debug.
   Tests: each blink clip variant (normal, slow, sloppy) resolves the right pad; sigil, ward and bolt clips never
   trigger a blink.
2. **Kernel adapter** (`Game/` side, e.g. `Session/ArenaSession.*`): owns an `FArenaState`, steps it at the data's fixed
   rate, and maps events both ways:
   - sigil cast (line, tier from the clock) -> the kernel's cast input; bolt clip -> Bolt; ward raise/lower with onset
     time -> the kernel's absorb input; blink -> the VR adapter below;
   - kernel actors, projectiles, telegraphs and hits -> greybox presentation (soldiers and slingers as simple capsules
     in their colours, projectiles with the A02 unlit threat colours, ground telegraphs, a hit flash, an HP bar per
     enemy as a simple world-space bar);
   - the player's HP, mana, stamina, tier clock and Flow -> the cuff.
3. **Blink as a VR adapter, not a kernel edit:** blink = snap the player's kernel position to the target pad plus the
   kernel's existing dodge semantics (invulnerability window and stamina cost from the pinned roll data). Write
   `docs/change-requests/CR-001-blink-and-pads.md` for the desktop/TV channel (what VR needs, why, the adapter used
   meanwhile, and the conformance impact). Kernel conformance vectors stay byte-identical.
4. **Wrist cuff in engine**: a small mesh or widget component on the off-hand wrist (desktop: lower-left of view) that
   shows the A08 states (tier sockets lit per unlocked tier, progress arcs, Flow pips, Crest-ready) driven by the kernel
   - use the A08 PNG masks as textures via runtime texture import or a material instance; no new binary assets if it
   can be avoided (if a `.uasset` is unavoidable, say why).
5. **Wave 1 session**: a `MageArena.Session.Start` console command (and auto-start in `-game`) running the pinned Tiro
   wave 1 (conscripts and slingers) against the player; victory and defeat end states with a log line; pause/resume on
   `Esc` freezing the kernel step.
6. **Scripted playthrough test** `MageArena.Session.Wave1Scripted`: drives the whole loop through `PlayQuickAction`
   only (no direct kernel calls): wards on incoming bolts, casts sigils, blinks on steel, and must reach victory within
   the wave's time budget; plus `MageArena.Session.Defeat` (doing nothing loses). Log the gesture -> kernel event chain.
7. **Capture**: extend `capture-greybox.ps1` (or a sibling script) to record the scripted wave as screenshots at key
   moments (first soldiers, a perfect absorb, a sigil cast hitting, a blink, victory) into `runs/T08/shots/`, and a short
   frame-sequence or video if cheap (ffmpeg is on PATH).
8. `runs/T08/REPORT.md`: files, test outputs, the gesture -> event chain from the log, screenshots list, budget numbers
   with enemies on screen, and what is still placeholder.

## Constraints
- Do not commit, push, or modify other repositories; do not edit `apps/vr/data/pinned/` or the art boards.
- The one-pipeline rule: keys play clips; nothing on A/D/S/1-4/Q/Space calls the kernel directly. Debug keys stay F-keys.
- Kernel conformance vectors stay byte-identical; every existing `MageArena.*` test stays green. One Unreal build at a time.

## Acceptance (the orchestrator re-runs these and looks at every screenshot)
```
node apps/vr/tools/conformance/generate.mjs     # byte-identical
powershell -NoProfile -File apps/vr/tools/build.ps1
"C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "C:\Users\kazda\kiro\mage-arena-vr\apps\vr\Game\MageArenaVR.uproject" -ExecCmds="Automation RunTests MageArena;Quit" -unattended -nullrhi -nosplash -log
<the capture command you add>                   # screenshots in runs/T08/shots/
```

## Retry 1 (orchestrator, 2026-10-03)

The loop works (gesture -> kernel chain, 93/93 tests, the cuff and presentation in the shots), but the win is not the
game we are making. The seated player NEVER walks: only blinks between the three pads (`docs/DECISIONS.md`,
plan 6.1). Fix these; keep every test green; do not weaken checks.

1. **No walking.** `ArenaSession.cpp:~925` sets `Input.Move = ComputeKiteMove(...)`, so the kernel player kites at walk
   speed while the seated camera stays on the pad - the simulation and the view disagree. Remove all move input; pin
   the kernel player to the active pad's position at all times (blink is the only position change, via the CR-001
   adapter). Update CR-001 to say so.
2. **Then measure honestly.** Re-run `Wave1Scripted` without walking. If victory still lands inside the pinned 25-40 s
   window, good. If it does not, **do not tune numbers and do not change the script to cheat**: let the test require
   victory within a generous cap (e.g. 120 s), log the measured time against the 25-40 s window, and report it as a
   design finding for the owner (the window was calibrated for the walking desktop/TV game). Also report how much damage
   the player takes from a seat.
3. **Vacuous time check:** `SessionTests.cpp:~103` stops the loop at the 40 s limit, so the `sim <= Limit` assertion at
   ~134 cannot fail. Loop to the generous cap and assert on the measured time.
4. Review findings: Wave 1 capture does not suppress mouse look (`MageArenaPlayerController.cpp:~131`, add the Wave 1
   capture flag); unscripted aim reads the camera component instead of the control rotation (`ArenaSession.cpp:~1132`);
   a gesture blink during `FadeIn` re-seats without a fade (`MageArenaPawn.cpp:~174`); add the missing `blink-back` from
   pad 2 case in `BlinkTests.cpp`.
5. Re-capture `runs/T08/shots/` (and the frame sequence) from the seated, non-walking run.

Note: the tilted horizon in `04-blink.png` is perspective from the side pad (yaw toward the arena centre), not a camera
roll - no change needed there.
