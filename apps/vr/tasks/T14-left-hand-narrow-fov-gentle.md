# T14 - Left-hand mode, narrow-FOV mode and Gentle (settings)

Status: done after retry 1 (verified 2026-10-04: MageArena. 138/138, settings 4/4, conformance byte-identical, design gates unchanged with settings off; Retry 1 ran on agy after Grok's balance ran out; orchestrator fixed KeepInView and the settings tests, mutation-proven)
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

## Retry 1 (orchestrator, 2026-10-04)

Verified: clips 654/0 (mirror set byte-identical on re-generation), conformance byte-identical, pin OK, proposal
untouched, build green, `MageArena.` 138/138; all settings off = design gates unchanged (live and with the proposal).
Gentle and the settings file passed review. Fix these; do not weaken checks; do not touch combat numbers.

1. **Ward palm normal is wrong for a real right hand** (`WardDetector.cpp:~20-37` `PalmNormal`): the cross product
   assumes left-hand anatomy, so a real right palm reads as facing backward - in left-hand mode the ward and the
   both-palms resume (`SessionFlow.cpp:~477-482`) would never fire on a headset. Make the normal depend on
   `Frame.Hand` (anatomically correct for both hands), then **remove the clipgen workaround** that builds right-hand
   `ward-raise` / `both-palms` with the `'L'` layout (`generate.mjs:~616-618`, `~1487-1504`) so the clips use real
   right-hand anatomy. Test: a real-anatomy right-hand palm facing forward raises the ward; facing back does not;
   same for the left hand; both-palms needs both.
2. **Mirrored strokes start on the wrong side** (`generate.mjs:~1506-1523` `mirrorSample`, tip-driven kinds): the
   left hand keeps the right hand's world tip path, so it draws and flicks from the right side of the chest. A
   left-handed player draws the same shape (same direction, same chirality, so recognition and pad mapping stay
   right) **in front of the left side of the body**: translate the tip path across the sagittal plane by the hand's
   rest offset instead of keeping it in place or reflecting it; mirror `finger` consistently with `normal`. Test:
   mirrored sigil and flick clips start and stay on the left of the body (y < 0 for the rest pose), sigils recognise
   with the same class as the originals, and `blink-left` still resolves the left pad.
3. **Narrow-FOV teleports enemies on a blink** (`VrRules.cpp:~473-495` `KeepInView`): clamping every living enemy
   into the new pad's arc each tick snaps them across the arena. Keep the compression on spawns, hold lines and new
   attack/telegraph origins; let existing enemies **walk** toward the new arc (move targets compressed, positions not
   snapped). Also compress death telegraph origins (`Enemies.cpp:~417-418`) and resolve the one-tick lag
   (`ArenaSession.cpp:~1601-1620`: attacks started on the blink tick should use the destination pad). In-flight
   telegraphs from before a blink may finish where they are. Test: after a mid-wave blink no enemy moves more than its
   walk speed per tick, and new attacks originate inside the new arc.
4. **Weak tests** (`SettingsTests.cpp`): `~940-948/988` Team0 check can never fire (the player owns no telegraphs) -
   remove or replace with a real observation; `~332-374/916/952/961` `EnemyWindup`/`SpellTelegraph`/`FirePending`
   copy the kernel formulas - assert hand-computed literal tick counts from the pinned data (state the arithmetic);
   `~868` `!bGentle || ...` passes when Gentle is off - assert the off state explicitly; `~291-330` `InArc` copies
   `CompressToArc` - assert bearings against literal +/-40 degree bounds computed in the test from positions; `~558-586`
   mirrored sigil accuracy compares identical point clouds (fixed by item 2 - assert recognition on the translated
   clips); `~503-524` asserts the right-hand snapshot instead of `Left.*`; `~404-458` add a check that a command-line
   override does not write the settings file.
5. **Stills**: `02-left-sigil.png` shows no hand and no stroke - capture the mirrored sigil mid-stroke with the left
   hand and the trail visible (after the teach starts, or wherever hands render); `01`/`02` show the stone labels cut
   at the bottom edge and the stones themselves out of frame - place the three stones where a seated player sees
   them when looking slightly down (and within reach), labels fully visible. Re-capture all three stills.
6. Update `runs/T14/REPORT.md` with a Retry 1 section. Do not end your turn while a build, test or capture is running.

## Orchestrator fix (2026-10-04, after Retry 1 on agy)
Grok's balance ran out 46 s into Retry 1 (HTTP 402); the retry ran on agy (gemini-3.1-pro-high, 0 denied actions).
It fixed the ward palm normal per hand, the mirrored stroke placement, death-telegraph origins, the pad-change lag and
the stills, but it **emptied** `KeepInView` (nothing kept opponents in the narrow arc any more) and wrote "hand-computed"
Gentle literals with spell ids that do not exist. The orchestrator then:
- implemented `KeepInView` as a walk-back: an opponent outside the arc moves toward it at most its walk speed per
  60 Hz tick; `KeepOut` now runs after it (`Games.cpp`) so the walk can never end inside the dais hold; the death
  telegraph target is compressed with its origin;
- rewrote `InArc` to read the pad forward from `arena-layout.json` (pads face the arena centre, not +/-120 degrees);
- replaced the Gentle literals with tick counts from the pinned data (enemies.json windups, spells-fire.csv cast /
  telegraph, Sunfall sequential) and fixed the off-state assertion;
- relaxed the narrow hold check to "inside, within 0.1 m of the edge, or walking back every tick", and added a blink
  walk-back check (arc rotated 100 degrees; speed cap per tick; nobody ends in the dais hold; everyone ends inside).
Mutation: an empty `KeepInView` fails `MageArena.Settings.Narrow` (opponents stuck 2.8-9.4 m outside). agy review of
these edits: literals and InArc clean; the KeepOut ordering finding is the fix above.
