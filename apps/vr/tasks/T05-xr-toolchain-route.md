# T05 - XR toolchain route: Meta XR v207 on UE 5.8, Android APK, Quest 3 simulator boot

Status: done (route R-A verified 2026-10-03; APK waits on the owner enabling Android in the Epic launcher)
Max turns: 260

## Goal
Gate D-G0 (plan section 4) must choose the toolchain route by **Fri 9 Oct**: can this UE 5.8.2 project load Meta's v207
plugins, package an arm64 Android APK with hand tracking, and boot under the Meta XR Simulator's **Quest 3** profile?
No headset exists. This card is an investigation with evidence: try the routes in order, record what each produced,
and leave the project on the best working route WITHOUT breaking the desktop build or any `MageArena.*` test.

## Read first
- `CLAUDE.md`, `docs/DECISIONS.md` (no WebXR pivot; desktop-first October; Quest 3 profile for simulators; paid tools
  need the owner; Start membership approved), `docs/PROJECT-PLAN.md` section 4 (D-G0 row and machine facts: UE 5.8.2
  installed, NDK 28.2 present, `NDKROOT` unset, JDK on PATH is Zulu 22), section 10 risk R1.
- Known issue: Meta investigation 1053459897316880 - v207 OculusXR / OculusInteraction plugins claim UE 5.8 support but
  were built against 5.7.0 and fail to load. Packaged 5.8 samples may need `r.Mobile.ShadingPath=0`.

## Routes, in order (stop at the first that fully works, but record evidence for every route you tried)
- **R-A: Meta XR v207 binaries.** Obtain the official "Meta XR Plugin" / "Unreal Engine 5 Integration" v207 package
  for UE 5.8 from Meta's official download page or Meta's official GitHub (no login circumvention; if a download needs
  the owner's login or a licence click-through, stop this route and write exactly what the owner must do). Install it
  **project-local** under `apps/vr/Game/Plugins/` (never into the engine folder). Load test: headless editor run of the
  automation suite with the plugin enabled; capture module-load errors verbatim.
- **R-B: Meta XR v207 from source in the project.** If the package ships plugin sources, let UBT compile them
  against 5.8 inside the project. Record compile errors verbatim and how far it got.
- **R-C: stock OpenXR.** Enable the engine's `OpenXR` and `OpenXRHandTracking` plugins (and what they need) instead.
  This loses Meta's Interaction SDK poses; note that hand joints still arrive through the engine's XR hand-tracking
  API, which the project's `IHandSource` can wrap later.

For whichever route loads:
1. **Android packaging.** Configure the Android toolchain the way UE 5.8 expects (use `RunUAT Turnkey -command=VerifySdk
   -platform=Android` to learn the required SDK/NDK/JDK; install what it asks for - local dependency installs are
   allowed; set paths in project or user config, not by editing engine files). Package with `RunUAT BuildCookRun
   -platform=Android -cookflavor=ASTC` (arm64, Vulkan, multiview where the route supports it, hand-tracking permission
   in the manifest). Success is judged by markers (the APK file exists, size, `aapt dump badging` / `aapt2` permissions
   showing the hand-tracking feature), not by the exit code alone.
2. **Simulator boot.** Install the Meta XR Simulator if it is obtainable without the owner's login; run the Win64
   build (or PIE/standalone) against it with the **Quest 3** profile and reach a boot marker in the log (add a
   `LogMageArena` line on startup if needed). If the simulator needs the owner, say exactly what to do.
3. **Desktop stays green.** The XR plugins must not break the desktop build or the `MageArena.*` suite. If a plugin
   cannot coexist with the desktop editor, keep it disabled by default and enable it only for Android/XR builds, and
   document how.

## Deliverables
- `apps/vr/tools/install-xr.ps1` - repeatable setup of the chosen route (download/verify/install plugin, set Android
  paths), idempotent; never stores credentials.
- `apps/vr/tools/package-android.ps1` - the BuildCookRun invocation and the APK marker checks.
- `docs/XR-TOOLCHAIN.md` - the decision record: each route tried, the verbatim evidence, the chosen route, what it
  loses, Android toolchain versions actually used, and the exact steps the owner must do (if any).
- Project/config changes needed by the chosen route. **Do not commit plugin binaries or downloaded archives**: add
  them to `.gitignore`; the install script re-creates them.
- `runs/T05/REPORT.md` - summary, evidence, timings (editor load, package time), and what remains for the owner.

## Constraints
- Do not commit, push, or touch any other repository. Do not modify the engine installation folder.
- No paid services; no account creation; no login circumvention. Stop a route and report when it needs the owner.
- One Unreal build at a time. Other workers may be generating images in `apps/vr/art/` - leave that folder alone.

## Acceptance (the orchestrator re-runs these)
```
powershell -NoProfile -File apps/vr/tools/build.ps1                                   # desktop: Result: Succeeded
"C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "C:\Users\kazda\kiro\mage-arena-vr\apps\vr\Game\MageArenaVR.uproject" -ExecCmds="Automation RunTests MageArena;Quit" -unattended -nullrhi -nosplash -log
                                                                                      # all MageArena.* Success
powershell -NoProfile -File apps/vr/tools/package-android.ps1                         # APK + marker checks (if reached)
type docs\XR-TOOLCHAIN.md                                                             # route decision with evidence
```
