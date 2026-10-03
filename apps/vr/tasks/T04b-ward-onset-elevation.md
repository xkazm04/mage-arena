# T04b - Ward onset: bridge stillness only while elevated (follow-up to T04)

Status: done (verified 2026-10-03, agy review: no defects)
Max turns: 120

## Goal
T04's retry chose an onset tail of 0.40 s from a "settle at the top" clip. An independent review after T04 was
accepted found that the walk-back only tests speed, so it can also bridge 0.25-0.40 s of stillness **resting in the
lap** back into an unrelated earlier twitch. That marks a fresh raise stale or expires the 0.15 s window early. Fix it
without losing the settle case.

## Findings to fix (each with a test that fails before the fix)
1. `WardDetector.cpp:~128-132` - in `FindOnset`, bridge sub-threshold frames only while the palm is elevated (above the
   lower height threshold); stillness in the lap stops the walk. Test: lower at t, lap twitch at t+0.06, still lap
   0.25 s, then a raise: onset must be the raise's own motion onset and the raise fresh (gap > 0.12 s).
2. `WardDetector.cpp:~127` - on the first raise of a stream (`!bEverLowered`) the bound is effectively minus infinity;
   bound it at the start of the history (or the first observed lowered sample). Test: lap fidget then first raise.
3. `WardDetector.cpp:~111` - when the palm normal is near vertical, `PalmFacing` falls back to `ReferenceFacing()` which
   can keep a vertical component toward an elevated target; flatten it. Test: elevated threat, palm up.

Keep every existing `MageArena.*` test green and the settle case within +/-1 frame for injected latency <= 60 ms.

## Acceptance
```
node apps/vr/tools/clipgen/generate.mjs && node apps/vr/tools/clipgen/validate.mjs apps/vr/Game/Clips
powershell -NoProfile -File apps/vr/tools/build.ps1
"C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "C:\Users\kazda\kiro\mage-arena-vr\apps\vr\Game\MageArenaVR.uproject" -ExecCmds="Automation RunTests MageArena;Quit" -unattended -nullrhi -nosplash -log
```
Report to `runs/T04b/REPORT.md`. Do not commit; no paid services; one Unreal build at a time.
