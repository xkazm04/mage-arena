# T13 - The first minute (teach) and pause / resume

Status: open
Max turns: 320

## Goal
The session in `docs/PROJECT-PLAN.md` section 6 opens with a 60-80 s teach inside the arena (no menus, no fail state)
and must pause and resume safely (section 6.6). Neither depends on the balance numbers the owner is deciding (DF-003),
so this card builds both on the live overlay. Plan rows: 0:00-0:15 cold start ("Raise your palm to begin."), 0:15-1:30
teach, then Wave 1.

## Read first
`CLAUDE.md`, `docs/DECISIONS.md`, `docs/PROJECT-PLAN.md` sections 6 (session table rows 0:00-1:30) and 6.6,
`docs/DESKTOP-INPUT.md` (one-pipeline rule), `apps/vr/tasks/BACKLOG.md`; code `Session/*` (ArenaSession, presentation,
Wave1Capture), `Hands/*`, `Gestures/*`, `Kernel/*` (spawning projectiles, telegraphs, dummies); data
`apps/vr/data/vr/*.json`; art `apps/vr/art/hud/` and `apps/vr/art/glyphs/` (A03/A08 for cues).

## Deliverables
1. **Session flow** `ColdStart -> Teach -> Wave1 -> ...` in `ArenaSession` (the later bouts stay as they are today).
   Cold start shows "Raise your palm to begin." and waits for a ward raise. Debug console command to skip the teach
   (`MageArena.Session.SkipTeach`) for testing only.
2. **Teach**, all in the arena, each step repeating until done, **no fail state** (no HP loss, no defeat):
   1. *Ward:* a slow water glob from the front - raise the palm to absorb it. Then a second glob timed so a well-timed
      raise is a **perfect**, shown by a white ring and a bell cue (use the existing perfect event; a placeholder sound is
      fine). Step completes on one absorb plus one perfect (the perfect is encouraged, not required forever: after 3 tries
      without a perfect, accept a normal absorb so nobody is stuck).
   2. *Sigil:* draw **the circle, then the bar** (owner decision: circle + bar is canonical) - Tide Orb hits a
      training dummy. The dummy is a kernel actor that does not attack. Show the glyph to trace (A03) on the dais.
   3. *Blink:* a black-core lane lights the active pad (the unblockable language) - flick to blink to another pad before
      it lands. Missing it deals no damage in the teach; it just repeats.
   Flow and the tier clock are **not** taught here (one wrist cue each during Wave 1; leave a TODO hook if absent).
   Prompts are short diegetic text on the dais or wrist (one string table for later localisation; no head-locked UI).
   Teach data (glob speeds, perfect timing, dummy HP, lane timing, retry counts) in a VR overlay data file, not code.
3. **Pause** (section 6.6): the kernel stops stepping. Triggers: application focus loss (desktop window deactivation;
   VR: the OpenXR session losing focus), headset removal (hook the XR event; desktop: a debug key `F8`), and **1.5 s of
   lost hand tracking during combat** (the hand pipeline reports no tracked frames; desktop: a debug key `F9` that drops
   tracking, plus a clip-level way to simulate it in tests). Esc keeps working as today. While paused nothing moves:
   projectiles, telegraphs, AI timers, mana, cooldowns, the tier clock.
4. **Resume:** raise **both palms** (new clip `both-palms` normal/slow/sloppy via `clipgen`, key `R` through
   `PlayQuickAction`), then a 3-2-1 count, then stepping resumes. Telegraphs resume exactly where they were - no free
   hits during or right after the count (state hash before pause == state hash at the first stepped tick after resume,
   minus the one tick). A pause during the teach resumes the teach step.
5. **Resume after quitting:** quitting mid-session stores the current bout index (a small save file under the project's
   Saved dir); the next launch offers to start at the beginning of that bout (raise palm = continue, both palms held =
   restart from the teach). Kernel conformance untouched.
6. **Tests** (`MageArena.Teach.*`, `MageArena.Pause.*`), driven through clips (`PlayQuickAction`) only:
   teach completes on clips in order; each step repeats on a miss and never damages or defeats the player; the
   perfect-or-3-tries rule; pause freezes the kernel (hash unchanged across N wall-clock ticks) for each trigger;
   tracking-loss pause fires at 1.5 s in combat and not outside combat or during the teach glob's absence; resume needs
   both palms and the full count; telegraph timing identical after resume; save/continue restores the bout start.
   Expectations hand-computed from the data in the test; every assertion able to fail.
7. **Capture**: stills of cold start, each teach step (ward with the perfect ring, the circle-then-bar glyph and Tide Orb
   hitting the dummy, the blink lane), the pause state and the 3-2-1 count, into `runs/T13/shots/`.
8. `runs/T13/REPORT.md`: files, tests, teach duration on the scripted clip player (target 60-80 s at normal clip speed;
   report the measured time), and what is placeholder.

## Constraints
Do not commit, push, or modify other repositories; do not edit `apps/vr/data/pinned/` or art boards. Do not touch the
calibration proposal or live combat numbers. Keys play clips only (one-pipeline rule); debug keys stay F-keys. One
Unreal build at a time. Do not end your turn while a build, test or capture is still running.

## Acceptance (the orchestrator re-runs these and looks at every screenshot)
```
node apps/vr/tools/clipgen/generate.mjs && node apps/vr/tools/clipgen/validate.mjs apps/vr/Game/Clips
node apps/vr/tools/conformance/generate.mjs     # byte-identical
powershell -NoProfile -File apps/vr/tools/build.ps1
"C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "C:\Users\kazda\kiro\mage-arena-vr\apps\vr\Game\MageArenaVR.uproject" -ExecCmds="Automation RunTests MageArena.;Quit" -unattended -nullrhi -nosplash -log
"C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "C:\Users\kazda\kiro\mage-arena-vr\apps\vr\Game\MageArenaVR.uproject" -ExecCmds="Automation RunTests MageArenaDesign.Session;Quit" -unattended -nullrhi -nosplash -log   # unchanged: lost 23.25 s
<the capture command you add>                   # screenshots in runs/T13/shots/
```
