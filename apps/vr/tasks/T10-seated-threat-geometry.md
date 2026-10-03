# T10 - Seated threat geometry: enemies fight from the floor, nothing reaches the dais (DF-001 option A)

Status: dispatched 2026-10-03
Max turns: 260

## Goal
Owner decision 2026-10-03 (`docs/DECISIONS.md`, `docs/design-findings/DF-001-seated-wave1.md`): from a seat the player lost
Wave 1 at 16.45 s because melee enemies reach the pads. The VR game keeps enemies **on the arena floor below the raised
dais**; they attack **from range** (conscripts throw their spears, slingers sling). Melee never touches the dais. Make
the seated Wave 1 winnable this way and re-measure honestly - the design gate `MageArenaDesign.Session.Wave1Seated`
is the judge.

## Read first
`CLAUDE.md`, `docs/DECISIONS.md` (newest first), `docs/design-findings/DF-001-seated-wave1.md`, `docs/PROJECT-PLAN.md`
sections 3 (VR overlay `combat.vr.json`) and 6.1, `docs/change-requests/CR-001-blink-and-pads.md`, `runs/T08/REPORT.md`,
code `Session/*`, `Kernel/*` (enemies), data `apps/vr/data/vr/arena-layout.json`.

## Deliverables
1. **VR overlay** `apps/vr/data/vr/combat.vr.json` (VR-owned, documented field by field): the dais boundary as a no-entry
   region for enemy movement (enemies hold at a standoff distance from the dais edge, measured from the layout); per
   enemy kind a VR attack mode - conscripts throw their spear as a projectile (speed, arc, telegraph, cooldown, damage
   taken from their pinned melee numbers unless the overlay states otherwise and why), slingers unchanged; anything the
   overlay overrides is listed with the pinned value it replaces.
2. **Applied as an adapter, not a kernel edit:** the C++ kernel reads the overlay through an explicit seam (e.g. a VR
   ruleset passed at session creation); with no overlay it behaves exactly as now, so **the conformance vectors stay
   byte-identical**. Add a change-request note to CR-001 (or a new CR-003) describing the seam for the desktop/TV channel.
3. **Presentation:** thrown spears fly as steel-coloured projectiles with a ground/target telegraph; enemies visibly hold
   at the dais edge.
4. **Re-measure** `MageArenaDesign.Session.Wave1Seated` (scripted through clips only, no walking, kernel player pinned):
   report victory/defeat, time against the pinned 25-40 s window, damage taken and dealt, blinks, perfects. Do NOT tune
   the shared numbers and do NOT change the script to cheat. If it still loses or lands far outside the window, report
   which knob (wave composition = option C, standoff distance, throw cadence) would close the gap and by how much -
   the orchestrator and the owner decide the next step.
5. Tests: overlay loads; with the overlay no enemy enters the dais region over a full wave; thrown spears are warded at
   the physical reduction and are blinkable; regular `MageArena.*` stays green.
6. Re-capture screenshots into `runs/T10/shots/` (enemies holding at the dais edge, a thrown spear mid-flight, a blink,
   the end state). `runs/T10/REPORT.md` with the measurement table.

## Constraints
Do not commit, push, or modify other repositories; do not edit `apps/vr/data/pinned/` or art. Conformance vectors
byte-identical. One Unreal build at a time.

## Acceptance
```
node apps/vr/tools/conformance/generate.mjs     # byte-identical
powershell -NoProfile -File apps/vr/tools/build.ps1
"C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "C:\Users\kazda\kiro\mage-arena-vr\apps\vr\Game\MageArenaVR.uproject" -ExecCmds="Automation RunTests MageArena.;Quit" -unattended -nullrhi -nosplash -log
"C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "C:\Users\kazda\kiro\mage-arena-vr\apps\vr\Game\MageArenaVR.uproject" -ExecCmds="Automation RunTests MageArenaDesign.;Quit" -unattended -nullrhi -nosplash -log
```
