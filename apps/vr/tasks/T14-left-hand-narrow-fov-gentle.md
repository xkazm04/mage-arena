# T14 - Left-hand mode, narrow-FOV mode and Gentle (settings)

Status: open
Max turns: 320

## Goal
Plan D3 (`docs/PROJECT-PLAN.md` sections 5, 6 and 6.7) lists three player settings that do not depend on the balance
numbers the owner is deciding (DF-003): **left-hand mode** (mirrored clips; the casting and ward hands swap),
**narrow-FOV mode** (for VR glasses at about 70 x 66 degrees against Quest 3's 110 x 96: spawn and telegraph origins
compressed to +/-40 degrees, offered automatically on a small FOV - this is the "FoV-aware design" the judging
criterion names), and **Gentle** (competence 1 for every opponent and doubled telegraph times, from data). Build all
three as settings with defaults off, persisted, and leave the live game unchanged when they are off.

## Read first
`CLAUDE.md`, `docs/DECISIONS.md`, `docs/PROJECT-PLAN.md` sections 3 (the overlay holds the narrow-FOV spawn arc),
6.1-6.7, `docs/DESKTOP-INPUT.md`, `docs/CLIP-SCHEMA.md`, `apps/vr/tasks/BACKLOG.md`; code `Hands/*`, `Gestures/*`
(sigil, ward, blink, staff, palms detectors), `Session/*` (incl. `SessionFlow.cpp`, presentation, cuff), `Kernel/VrRules.*`,
`Kernel/MageAI.*`, `Kernel/Enemies.*`; data `apps/vr/data/vr/combat.vr.json`, `arena-layout.json`, `teach.json`;
tool `apps/vr/tools/clipgen/*`.

## Deliverables
1. **Settings** (`hand` = right|left, `fovMode` = wide|narrow, `gentle` = bool) in a small JSON file under the project's
   Saved dir, loaded at start, safe on a missing or corrupt file (defaults: right, wide, false). Console commands to set
   each (`MageArena.Settings.Hand left`, ...), and a diegetic way to change them before the teach (e.g. three dais
   toggles poked or palmed; keep it minimal, greybox). Strings in `strings.json`.
2. **Left-hand mode**: `clipgen` gains a left-right **mirror** transform (mirror about the body's sagittal plane, swap
   the hand tag and the joint layout correctly, keep timing); generate the mirrored set for every action clip
   (sigils, bolt, ward hold, blink, staff, palms) as data, byte-identical on re-generation, validated by
   `validate.mjs`. In left-hand mode the keys play the mirrored clips; detectors read the casting hand as left and
   the ward hand as right; blink flick directions resolve correctly (a flick to the player's left still means the
   left pad); the cuff moves to the other wrist. Tests: every detector recognises its mirrored clips (normal, slow,
   sloppy) with the same outcome as the right-handed originals; sigil recognition accuracy on mirrored clips equals the
   originals; split hands and the staff work mirrored.
3. **Narrow-FOV mode**: a VR overlay field (`narrowFov.spawnArcDeg` 40, plus the FOV threshold that triggers the
   offer, e.g. horizontal FOV < 90 degrees) compresses enemy spawn positions, hold lines and telegraph origins into
   +/-40 degrees of the seated forward axis around the active pad; desktop camera FOV switches to 70 degrees
   horizontal. Offered automatically (a dais prompt) when the device/desktop FOV is under the threshold; never forced.
   Tests: with narrow mode on, every spawn and telegraph origin in Wave 1 and the Fire duel lies within +/-40 degrees
   of the forward axis of the pad the player is on at spawn time (hand-computed bounds); with it off, the positions are
   byte-identical to today (state-hash check over a scripted bout).
4. **Gentle**: competence 1 for every opponent and telegraph times x2, read from an overlay field (`gentle.telegraphMult`
   2, `gentle.competence` 1). Tests: telegraph windups doubled (hand-computed from the pinned data), opponent competence
   forced to 1; off = unchanged.
5. **Measure, do not tune**: run `MageArenaDesign.Session` and `MageArenaDesign.Duel` with each setting on (one process
   per filter) and report the outcomes next to the live ones; Gentle and narrow-FOV are expected to change them.
   Gates are not changed and no combat number is retuned.
6. **No regression**: with all settings at default, the regular suite, the 45 conformance vectors and both design gates
   are byte-for-byte the same as today (Wave1Seated lost 23.25 s; FireSeated c1 won 29.47 s, c1.5 lost 22.92 s).
7. **Capture**: stills of the settings toggles, a left-hand sigil being drawn (mirrored), narrow-FOV Wave 1 with
   enemies inside the arc at 70 degrees FOV, into `runs/T14/shots/`.
8. `runs/T14/REPORT.md`: files, tests, the measured outcomes per setting, what is placeholder.

## Constraints
Do not commit, push, or modify other repositories; do not edit `apps/vr/data/pinned/`, art boards, the calibration
proposal or live combat numbers. Keys play clips only; debug keys stay F-keys. One Unreal build at a time. Do not end
your turn while a build, test or capture is still running.

## Acceptance (the orchestrator re-runs these and looks at every screenshot)
```
node apps/vr/tools/clipgen/generate.mjs && node apps/vr/tools/clipgen/validate.mjs apps/vr/Game/Clips
node apps/vr/tools/conformance/generate.mjs     # byte-identical
powershell -NoProfile -File apps/vr/tools/build.ps1
"C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "C:\Users\kazda\kiro\mage-arena-vr\apps\vr\Game\MageArenaVR.uproject" -ExecCmds="Automation RunTests MageArena.;Quit" -unattended -nullrhi -nosplash -log
"...UnrealEditor-Cmd.exe" "...MageArenaVR.uproject" -ExecCmds="Automation RunTests MageArenaDesign.Session;Quit" -unattended -nullrhi -nosplash -log   # default: lost 23.25 s
"...UnrealEditor-Cmd.exe" "...MageArenaVR.uproject" -ExecCmds="Automation RunTests MageArenaDesign.Duel;Quit" -unattended -nullrhi -nosplash -log      # default: unchanged
<the capture command you add>                   # screenshots in runs/T14/shots/
```
