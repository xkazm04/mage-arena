# XR toolchain (D-G0)

Decision date: 2026-10-03. Engine: UE 5.8.2 at `C:\Program Files\Epic Games\UE_5.8` (changelist 56702186, CompatibleChangelist 55116800, branch `++UE5+Release-5.8`). No headset. Quest 3 is the simulator profile. This project stays on UE 5.8.

## Decision

Keep **R-A**: Meta XR v207 and the Meta Interaction SDK v207, installed project-local and enabled in `MageArenaVR.uproject`. The Win64 editor loads both plugins. The same Win64 binary boots against the Meta XR Simulator v207 Quest 3 profile and logs `MAGEVR_BOOT_OK`. The desktop `MageArena.*` suite still passes with the plugins enabled.

An arm64 APK was not produced. The installed engine has no Android target support, so UnrealBuildTool stops in `Core.Build.cs` before cook. Stock OpenXR packaging hits that same missing engine module. The owner step below is what unblocks packaging.

## R-A — Meta XR v207

Official pages, public CDN, no login and no licence click-through on this machine:

- https://developers.meta.com/horizon/downloads/package/unreal-engine-5-integration/ — `UnrealMetaXRPlugin.207.0.zip`, 472628598 bytes, `https://securecdn.oculus.com/binaries/download/?id=28628836283394454`
- https://developers.meta.com/horizon/downloads/package/meta-xr-interaction-sdk-unreal/ — `MetaXRInteractionPackage.207.0.0.zip`, 690834867 bytes, `https://securecdn.oculus.com/binaries/download/?id=28587630994202643`

Both packages report `license_info` path `oculussdk`, id `790194175111532`. `apps/vr/tools/install-xr.ps1` prints that id, refuses a login wall, and refuses an HTML body in place of the zip. Re-run on 2026-10-03 exited 0 with `CACHE_HIT` for both zips, `PLUGIN_PRESENT` for both trees, and `PLUGIN_BUILD_ID=55116800` matching `ENGINE_COMPATIBLE_CHANGELIST=55116800`.

Install layout (gitignored; the script recreates it):

- `apps/vr/Game/Plugins/MetaXR/OculusXR.uplugin` — FriendlyName Meta XR, VersionName 1.207.0, EngineVersion 5.8.0, Installed true
- `apps/vr/Game/Plugins/MetaXRInteraction/OculusInteraction.uplugin` — FriendlyName Meta Interaction SDK, VersionName 1.207.0, EngineVersion 5.8.0, Installed true

Meta investigation 1053459897316880 (v207 claims 5.8 and was built against 5.7, then fails to load) was not reproduced. Both `Binaries\Win64\UnrealEditor.modules` files say `"BuildId": "55116800"`, the same CompatibleChangelist as this engine. The headless editor loaded `OculusXRHMD`, `OculusXRInput`, `OculusXRPassthrough`, `OculusInteraction`, `OculusInteractionPrebuilts`, and `OculusInteractionEditor`. Log `runs/T05/ra-editor.log` opened `10/03/26 03:28:20` and ended:

```
**** TEST COMPLETE. EXIT CODE: 0 ****
```

with 17 `MageArena.*` results of `Success`. After the boot-marker module was compiled, the same command wrote `runs/T05/ra-editor-postboot.log` (`TEST_EXIT=0`, `TEST_SECONDS=32.2`, again 17 `Success`, and `LogMageArena: MAGEVR_BOOT_OK`).

One plugin warning did not fail a test, from `OculusXRPassthroughSubsystem.h`:

```
LogClass: Error: BoolProperty FLightingEstimation_LightingEstimate::bIsTextureBuilt is not initialized properly. Module:OculusXRPassthrough
```

The plugins stay enabled for the desktop editor. They did not need a desktop-off / Android-on split.

## R-B — compile the shipped sources

The zips ship sources and prebuilt Win64 editor binaries. `apps/vr/tools/build.ps1` compiled those Win64 sources while rebuilding the game module. `runs/T05/desktop-build.log`: 518 actions, `Result: Succeeded`, `Total execution time: 743.10 seconds`, wrapper `BUILD_EXIT=0 BUILD_SECONDS=744.6`. One deprecation warning, build still succeeded:

```
OculusXRHMD.cpp(4475,38): warning C4996: 'RHICreateTexture': RHICreateTexture with an implied immediate command list is deprecated
```

Android plugin binaries are not prebuilt in the zip (third-party `.so` files are). UBT never reached an Android compile of the plugin, because it stops earlier on the missing engine module below. A separate R-B attempt is not useful until that module exists.

## R-C — stock OpenXR

Not switched on. R-A loads, and replacing it would drop Meta Interaction SDK poses and gaze. Hand joints would still arrive through the engine XR hand-tracking API, which `IHandSource` can wrap later. Packaging would still fail: the RulesError below is `MageArenaVR -> Launch.Build.cs -> SessionServices.Build.cs -> Core.Build.cs`, before any OculusXR Android code.

## Android toolchain actually used

`RunUAT Turnkey -command=VerifySdk -platform=Android` did not check Android until the NDK was visible and a user-config platform row existed. The first two runs (`runs/T05/turnkey-verify.txt`, `runs/T05/turnkey-verify-2.txt`) exited 0 in a few seconds with:

```
Platform(s) and/or device(s) needed for VerifySdk command. Check parameters or selections.
```

The engine registers Android only when `Engine\Binaries\Android\UnrealGame.target` exists. This install's `Engine\Binaries` has no `Android` directory. `install-xr.ps1` appends a user-config row with no `RequiredFile`, which lets Turnkey and UBT see the platform. It does not install engine binaries. After `NDKROOT` was set, `runs/T05/turnkey-verify-3.txt` (`TURNKEY_EXIT=0`, `TURNKEY_SECONDS=1.3`) says:

```
Android: (Status=Valid, MinAllowed_Sdk=r27c, MaxAllowed_Sdk=r29, Current_Sdk=r28c, Allowed_AutoSdk=r27c, Current_AutoSdk=, Flags="InstalledSdk_ValidVersionExists, Support_FullSdk")
```

Versions on this machine:

| Piece | Value |
| --- | --- |
| SDK | `C:\Users\kazda\scoop\apps\android-clt\current` (`ANDROID_HOME`, already set) |
| NDK | `28.2.13676358`, `Pkg.ReleaseName = r28c` (`NDKROOT`) |
| JDK | Zulu 22.0.1, Azul, `C:\Program Files\Zulu\zulu-22` (`JAVA_HOME`) |
| Platforms | android-31, android-34, android-36 |
| Build-tools | 28.0.3, 34.0.0, 35.0.0, 36.1.0 |
| CMake | 3.22.1 present |

`Engine\Config\Android\Android_SDK.json` asks for NDK `27.2.12479018`, build-tools `36.0.0`, platforms `android-36`, cmake `3.22.1`, and accepts NDK r27c through r29. Turnkey accepted r28c. Build-tools 36.0.0 is not installed; 36.1.0 is. Paths live in the user environment and in `%LOCALAPPDATA%\Unreal Engine\Engine\Config\UserEngine.ini` under `[/Script/AndroidPlatformEditor.AndroidSDKSettings]`. The script clears `ANDROID_SDK_HOME` in the process because UBT treats it as a conflicting SDK root. The engine tree was not edited.

`UserEngine.ini` also contains:

```
[InstalledPlatforms]
+InstalledPlatformConfigurations=(PlatformName="Android", Configuration="Development", PlatformType="Game", Architecture="arm64", ProjectType="Any", bCanBeDisplayed=True)
```

That row is what made the third VerifySdk check Android. After the owner installs the engine Android component, remove this `[InstalledPlatforms]` block if a later VerifySdk or UBT run should go back to the engine's own `RequiredFile` check. Leave the SDK/NDK/Java paths.

## Packaging

`apps/vr/tools/package-android.ps1` dot-sources the install script, prints `OWNER_ACTION` while the engine receipts are missing, and runs:

```
BuildCookRun -project=apps/vr/Game/MageArenaVR.uproject -noP4 -platform=Android -clientconfig=Development -cook -build -stage -pak -archive -archivedirectory=apps/vr/Game/Saved/Android -cookflavor=ASTC -prereqs -unattended -utf8output
```

`runs/T05/package-android.log` stops in rule compilation. No APK, so `runs/T05/apk-markers.txt` was not written. The script exits 1 when the markers are absent. Verbatim:

```
Could not find definition for module 'cxademangle', (referenced via MageArenaVR -> Launch.Build.cs -> SessionServices.Build.cs -> Core.Build.cs)
Result: Failed (RulesError)
Total execution time: 0.88 seconds
Took 1.11s to run dotnet.exe, ExitCode=8
AutomationTool exiting with ExitCode=8 (8)
BUILD FAILED
```

AutomationTool reported `0h 0m 2s`. The same log warns that these Android libraries are not on disk: Oodle `liboo2netandroid.a` and `liboo2coreandroid.a`, Google Play `libplaycore_static.a`, and BLAKE3 `libBLAKE3.a`. Both of these paths are absent:

- `C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Android\UnrealGame.target`
- `C:\Program Files\Epic Games\UE_5.8\Engine\Source\ThirdParty\cxademangle`

Project Android settings (not reached by this package, because UBT died first) in `apps/vr/Game/Config/DefaultEngine.ini`:

- `PackageName=com.magearena.vr`, `bPackageForMetaQuest=True`
- arm64 only, Vulkan on, Vulkan SM5 off, `bBuildForES31=False`, ASTC, data inside the APK
- `MinSDKVersion=32`, `TargetSDKVersion=34`
- `XrApi=NativeOpenXR`, `HandTrackingSupport=ControllersAndHands`, `HandTrackingFrequency=LOW`, `HandTrackingVersion=Default`, `+SupportedDevices=Quest3`

`ControllersAndHands` is the v207 setting that makes `OculusMobile_APL.xml` add `com.oculus.permission.HAND_TRACKING` and `oculus.software.handtracking` (`android:required` false unless the mode is `HandsOnly`). The APL's `contains="Quest"` also matches the string `Quest3`, so the manifest is expected to carry the legacy token `quest` as well as `quest3`. The plugin was not patched. A draft `ExtraApplicationSettings` line that named quest2, questpro, quest3, and quest3s was in fact still in `DefaultEngine.ini` until finding F2 removed it on 2026-10-07 (this section had said it was already gone), because the APL merges any existing `com.oculus.supportedDevices` value into that tag. The declared devices are now Quest 3 and Quest 3S (`+SupportedDevices=Quest3` and `+SupportedDevices=Quest3S`), so the expected manifest value is `quest|quest3|quest3s`. Frame and memory budgets stay measured on Quest 3.

`apps/vr/Game/Config/Android/AndroidEngine.ini` sets `vr.MobileMultiView=1`, `r.Mobile.MultiView=1`, and `r.Mobile.ShadingPath=0` (the packaged-5.8-sample note from the task card). Desktop `r.ForwardShading` stays true.

If the next package, after the engine component is installed, fails on the renderer or the SDK level, the fallback to try is `bBuildForES31=True`. It was not changed ahead of a failure that has not been seen. Do not raise `TargetSDKVersion` to 36 (the engine default since 5.8.2) and do not leave it blank: 34 is the target Meta requires for new apps and the cap for immersive apps (allowed range 32-34 immersive, 32-36 for 2D), see https://developers.meta.com/horizon/resources/publish-mobile-manifest/.

`package-android.ps1` treats an APK as success only when `aapt dump badging` / permissions show `arm64-v8a`, `com.oculus.permission.HAND_TRACKING`, and `oculus.software.handtracking`, and the badging shows `targetSdkVersion:'34'`.

## Simulator boot

Meta XR Simulator v207 is already extracted at `C:\Users\kazda\AppData\Local\MetaXRSimulatorExtract\PFiles\MetaXRSimulator\v207.0`. Its runtime json is `meta_openxr_simulator.json` (`library_path` `.\SIMULATOR.dll`). `config\sim_core_configuration.json` has `"device_profile": "Meta Quest 3"` and `"use_offscreen_mode": true`. That config was not modified. `activate_simulator.ps1` writes `HKLM\SOFTWARE\Khronos\OpenXR\1` and elevates; this boot used `XR_RUNTIME_JSON` instead, pointed at the json above.

Command (Win64, no `-nullrhi`):

```
UnrealEditor-Cmd.exe apps\vr\Game\MageArenaVR.uproject -game -unattended -nosplash -nosound -RenderOffScreen -log -abslog=runs\T05\sim-boot.log -stdout -FullStdOutLogOutput -ExecCmds=Quit
```

`FMageArenaVRModule::StartupModule` logs `LogMageArena: MAGEVR_BOOT_OK`. Completed run, started `2026-10-03T04:21:20+02:00`, process exit 0 at `04:24:46` (about 206 seconds). The engine log says `(Engine Initialization) Total time: 196.15 seconds`. Verbatim:

```
LogHMD: xrCreateInstance succeeded with XR_API_VERSION_1_0
LogHMD: Initialized OpenXR on Meta XR Simulator runtime version 207.0.0
LogMageArena: MAGEVR_BOOT_OK
LogInit: Display: Engine is initialized. Leaving FEngineLoop::Init()
LogLoad: LoadMap: /Engine/Maps/Entry?Name=Player
```

The log lists `XR_EXT_hand_tracking`, `XR_FB_hand_tracking_aim`, and `XR_FB_hand_tracking_mesh`. It reports optional `XR_FB_hand_tracking_capsules` and `XR_EXT_hand_tracking_data_source` as unavailable. It does not print the words `Quest 3`; the profile evidence is the simulator config file.

Non-fatal lines on that run:

- `LogHMD: Failed loading OVRPlugin 1.207.0` (Native OpenXR still created the instance)
- `LogOculusXRColocation` / `Scene` / `Anchors`: event received but no HMD was present, after engine init
- `LoadErrors` for `/OculusInteractionSamples/Materials/MI_UIGradient_Dark` (the samples mount is not in the two v207 zips)

An earlier launch was stopped during derived-data-cache maintenance, before the game module started. That log is `runs/T05/sim-boot-attempt1.log`. It already contained `Initialized OpenXR on Meta XR Simulator runtime version 207.0.0`. The completed evidence is `runs/T05/sim-boot.log`.

## Owner steps

1. Epic Games Launcher → Unreal Engine → Library → the 5.8 install (UE_5.8, 5.8.2-56702186) → the drop-down next to Launch → Options → enable Android → Apply.
2. Confirm both paths exist: `C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Android\UnrealGame.target` and `C:\Program Files\Epic Games\UE_5.8\Engine\Source\ThirdParty\cxademangle`.
3. Re-run `powershell -NoProfile -File apps\vr\tools\package-android.ps1`. Success is an arm64 APK whose badging contains the three markers above.
4. The Oculus SDK licence on the downloaded zips is `oculussdk` id `790194175111532`. The CDN served the files without a click-through. If a later run of `install-xr.ps1` prints `OWNER_ACTION` for a login or an HTML licence page, accept that licence in a browser on the official download page and re-run. The script does not log in and does not click through.

This task does not write those engine files and does not commit the plugin trees or the zip cache.

## Hardening after review (orchestrator, 2026-10-03)

An independent read-only review of the scripts found no writes outside the repository and the user profile, no engine
or HKLM writes, and no credentials. Five defects were fixed by the orchestrator and verified:

- **Integrity:** `install-xr.ps1` now checks each Meta archive against a pinned SHA-256 (`PinnedSha256`) on download and
  on cache hit; a mismatch or an unknown file name stops the script. A new Meta release is a deliberate re-pin.
  Verified: both cached archives print `SHA256_OK`.
- **Fresh clone:** `OculusXR` and `OculusInteraction` are `"Optional": true` in `MageArenaVR.uproject`, so a checkout
  without the (git-ignored) plugin trees still opens. Verified by moving `Game/Plugins` aside: editor load and 17/17
  `MageArena.*` tests pass; with the plugins back, the plugin modules load again.
- **Manifest:** the leftover `ExtraApplicationSettings` `supportedDevices` line was still in `DefaultEngine.ini`; F2
  removed it on 2026-10-07 (earlier text claiming it was already removed was wrong). Declared devices: Quest 3 and
  Quest 3S; budgets stay measured on Quest 3. `package-android.ps1` now asserts the value (`-SelfTest` checks the matcher).
  Every editor launch re-adds the line to `DefaultEngine.ini` (`AndroidRuntimeSettings.cpp:101-117`, called from
  `PostInitProperties` at `:235`), so until a guard lands, restore that file after any UE run and never commit the
  re-added line.
- **Stale APK:** `package-android.ps1` only accepts APKs written after the run started.
- **Portable path:** the scoop Android SDK fallback is built from `%USERPROFILE%`, not a hard-coded user folder.

## 5.8.3 re-check (2026-10-07)

The T05 runs above were on UE 5.8.2; the decision record stays as written. The engine has since moved to 5.8.3.

- **Engine:** `C:\Program Files\Epic Games\UE_5.8\Engine\Build\Build.version` reads version 5.8.3, `Changelist` 58210709, `CompatibleChangelist` 55116800.
- **Plugin match:** the plugin BuildId `55116800` still matches the engine (`docs/research/STACK-OPPORTUNITIES-2026-10.md`, F9 "Local": both plugins' `UnrealEditor.modules` and the engine's `UnrealEditor.version` share it).
- **Editor build:** in a builder worktree on 5.8.3, the editor build exited 0 in 393 s without the MetaXR plugins (run 4f661fa7, commit 1f9b0ea).
- **Not re-proven on 5.8.3:** that the editor loads the plugin modules, and the simulator boot marker `MAGEVR_BOOT_OK`. Both need the plugins in the main checkout, so they are an owner step.
