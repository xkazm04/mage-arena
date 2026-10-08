# Stack opportunities, 2026-10

Grounded research on what Mage Arena VR runs on, and what upstream says should change.

- **Web search was available.** WebSearch and WebFetch worked, and so did `curl` against the raw pages.
- **How the quotes were checked.** A research sub-agent did the first sweep. I then downloaded every source page again
  on 2026-10-07 and found each quoted sentence in the raw HTML with `grep` (section 6 has the commands and exit codes).
  One page, UploadVR, timed out under `curl` (exit 28, HTTP 000). Its quote comes from WebFetch only, and the finding says so.
- **Labels.** Statements that no source confirmed are marked **unconfirmed**. Statements read from the installed engine
  or plugin source give a path and line, not a URL.

The before-numbers live in `docs/research/BASELINE-2026-10-07.md`. This file changes nothing. Every recommendation
below is a proposal for a later card.

Short paths resolve as follows:

- `Kernel/...`, `Hands/...`, `Combat/...` and `Tests/...` are under `apps/vr/Game/Source/MageArenaVR/`.
- `MetaXR/...` is under the main checkout's `apps/vr/Game/Plugins/`. Those trees are untracked and were read-only.
- `Engine/...` is under `C:\Program Files\Epic Games\UE_5.8\`, the installed engine.

## 1. The stack, as read

| Piece | Value | Where it was read |
|---|---|---|
| Engine | **UE 5.8.3**, Changelist 58210709, CompatibleChangelist 55116800. The docs still say 5.8.2 (CL 56702186). | `C:\Program Files\Epic Games\UE_5.8\Engine\Build\Build.version` |
| Meta XR plugin | `OculusXR` 1.207.0, EngineVersion 5.8.0, BuildId 55116800; 17 modules | main checkout, `apps/vr/Game/Plugins/MetaXR/OculusXR.uplugin` (read-only) |
| Interaction SDK | `OculusInteraction` 1.207.0, `IsExperimentalVersion: true`; depends on `OpenXRHandTracking` and `Niagara` | main checkout, `apps/vr/Game/Plugins/MetaXRInteraction/OculusInteraction.uplugin` (read-only) |
| Project plugins | EnhancedInput; ModelingToolsEditorMode (editor only); OculusXR and OculusInteraction, both `"Optional": true` | `apps/vr/Game/MageArenaVR.uproject:14-19` |
| Game module deps | Core, CoreUObject, Engine, InputCore, EnhancedInput, Json, HeadMountedDisplay, UMG, Slate, SlateCore; private RHI, ImageWrapper. **No OpenXR or OculusXR module.** | `apps/vr/Game/Source/MageArenaVR/MageArenaVR.Build.cs:11-28` (read-only; another worker has uncommitted edits to it) |
| Renderer | `r.ForwardShading=True`, `r.MSAACount=4`, `r.DefaultFeature.AntiAliasing=3`, `r.Mobile.UseHWsRGBEncoding=True` | `apps/vr/Game/Config/DefaultEngine.ini:6-9` |
| Android | arm64 only, Vulkan, no ES3.1, ASTC, data inside APK, `MinSDKVersion=32`, `TargetSDKVersion=32`, plus an `ExtraApplicationSettings` supportedDevices line | `DefaultEngine.ini:35-48` |
| Meta XR settings | `XrApi=NativeOpenXR`, `HandTrackingSupport=ControllersAndHands`, `HandTrackingFrequency=LOW`, `HandTrackingVersion=Default`, `+SupportedDevices=Quest3` | `DefaultEngine.ini:50-56` |
| Android CVars | `vr.MobileMultiView=1`, `r.Mobile.MultiView=1`, `r.Mobile.ShadingPath=0` (under `[ConsoleVariables]`) | `apps/vr/Game/Config/Android/AndroidEngine.ini:5-8` |
| Staged non-asset files | `../Clips` and `../../data/vr` | `apps/vr/Game/Config/DefaultGame.ini:9-10` |

**Latest upstream versions on 2026-10-07:**

- **Meta XR "Unreal Engine 5 Integration":** 207.0 is current.
  - The page shows "Updated: Sep 23, 2026" and lists 207.0, 205.0, 203.0 and 201.0.
  - The page lists no version above 207.0, so the project is already on the latest plugin.
  - Source: https://developers.meta.com/vr/downloads/package/unreal-engine-5-integration/
- **Interaction SDK:** 207.0, "Updated: Sep 22, 2026".
  - Source: https://developers.meta.com/horizon/downloads/package/meta-xr-interaction-sdk-unreal/
- **UE 5.8:** the 5.8.3 hotfix was posted 2026-09-22, and this machine already runs it.
  - Source: https://forums.unrealengine.com/t/5-8-3-hotfix-released/2833315
  - The sub-agent found no 5.8.4. Its only evidence is the forum's hotfix tag list, so treat that as **unconfirmed**.

## 2. Summary

| # | Finding | Change | Cost | When | Headless? | Registry |
|---|---|---|---|---|---|---|
| F1 | Target SDK 32 cannot be uploaded for a new app | `TargetSDKVersion=34` | S | before the 2026-10-31 gate | yes, once packaging works | EXTENDS `engine-pitfall-corpus` |
| F2 | The APK would claim Quest 2 / Pro / 3S | delete `DefaultEngine.ini:48`; assert the manifest | S | before the gate | yes, once packaging works | EXTENDS `ship-pipeline-gating` |
| F3 | Desktop AA key is dead; Quest AA key is missing | `r.AntiAliasingMethod=3`, `r.Mobile.AntiAliasing=3` | S | before the gate | desktop yes | N5 |
| F4 | Mobile HDR is on by default, and the blink vignette needs it | `r.MobileHDR=False`; vignette as geometry | S + M | vignette before the gate; HDR at V1 | desktop yes | N5 |
| F5 | Pinned combat data is not staged into a package | stage `../../data/pinned`; log the pin hash at boot | S | before the gate | yes (Win64 stage) | N7 |
| F6 | Foveation is off, and it can only be set per project | `FoveatedRenderingLevel` + dynamic on | S | V1 | no | N5 |
| F7 | Hand-tracking rate: keep LOW; the clips assume 72 Hz | 30 Hz held-sample clip variant; FMM A/B in V2 | S-M + M | variant before the gate; A/B in V2 | variant yes | N2 |
| F8 | The system gesture must not fire a game action | system-gesture bit in `FHandFrame` and in clips | M | schema and test before the gate; wiring at V1 | test yes | N1 |
| F9 | Engine moved to 5.8.3 under a 5.8.2 record | record it, re-verify plugin load, freeze updates | S | before the gate | yes | EXTENDS `engine-pitfall-corpus` |
| F10 | AppSW on 5.8 is engine frame synthesis, with a default that misses Quest | none now; plan P-4 with the right switches | M | V1, only if over budget | no | N5 |

## 3. Findings

### F1. Target SDK 32 cannot be uploaded for an app created after 2026-03-01

- **Status:** applied 2026-10-07, before `TargetSDKVersion=32`, after `TargetSDKVersion=34`; APK proof pending the engine's Android component.

- **Upstream statements:**
  - Meta's Android 14 post (Nov 24, 2025, updated February 6, 2026):
    "Starting March 1, 2026, all new Meta Horizon apps created in the Developer Dashboard are required to target Android
    14 (API level 34). This means targetSdkVersion must be API level 34, though minSdkVersion will not be impacted and
    can still use API level 32." It adds: "API level 34 will be enforced during binary upload, meaning you will not be
    able to upload binaries with a lower targetSdkVersion level for apps created after March 1, 2026."
    https://developers.meta.com/horizon/blog/meta-quest-apps-android-14-march-1/
  - Meta's manifest requirements (last updated 2026-09-30): apps "must set their targetSdkVersion to 34". The
    allowed range is "32-34 for immersive, 32-36 for 2D".
    https://developers.meta.com/horizon/resources/publish-mobile-manifest/
  - The UE 5.8.2 hotfix notes (2026-08-25): "Bumped up Target SDK to API 36".
    https://forums.unrealengine.com/t/5-8-2-hotfix-released/2746335
    The installed engine agrees: `Engine/Config/BaseEngine.ini:3305` is `TargetSDKVersion=36`.
- **Repo:**
  - `apps/vr/Game/Config/DefaultEngine.ini:47` sets `TargetSDKVersion=32`.
  - `docs/XR-TOOLCHAIN.md:126` names `TargetSDKVersion=36` as a packaging fallback. 36 is above the immersive cap of 34.
- **Recommended change:**
  - Set `TargetSDKVersion=34` and keep `MinSDKVersion=32`.
  - Remove "36" from the fallback list in `XR-TOOLCHAIN.md`.
  - Never leave the key blank, because the engine default is now 36.
  - Read Android 13 and 14 behaviour changes (the post links both) before the first APK.
- **Unconfirmed:** whether the competition app is "created after March 1, 2026". No app id is in the repo, and the
  plan's P11 still lists a one-time dashboard setup, so it most likely is.
- **Cost:** S (one line), plus one repackage.
- **When:** before the 2026-10-31 desktop gate. The plan's P11 release lane wants a dry-run upload in October, and an
  upload with 32 would be refused.
- **Verifiable headless:** yes, once the owner installs the engine Android component: `aapt dump badging` shows
  `targetSdkVersion`. Not in this worktree.
- **Registry:** EXTENDS `engine-pitfall-corpus`. New incident entry: an engine hotfix silently raised a packaging
  default above a store's cap.

### F2. The committed config makes the APK claim Quest 2, Quest Pro and Quest 3S

- **Upstream statements:**
  - Meta's compatibility-mode page (last updated 2026-09-08): "The `com.oculus.supportedDevices` manifest entry lists
    the VR devices that your app supports. The runtime uses this list to decide whether to turn on compatibility mode."
    https://developers.meta.com/horizon/documentation/native/android/os-compatibility-mode/
  - The UE 5.8 release notes (page `updated_at` 2026-06-23): "Adding Meta Quest 3s as supported device when
    bPackageForMetaQuest is enabled".
    https://dev.epicgames.com/documentation/unreal-engine/unreal-engine-5-8-release-notes
- **Repo:**
  - `DefaultEngine.ini:48` still reads
    `ExtraApplicationSettings=<meta-data android:name="com.oculus.supportedDevices" android:value="quest2|questpro|quest3|quest3s" />`.
    It was added in `cb78e83` and no later commit touched it.
  - The plugin's `OculusMobile_APL.xml:353-371` finds an existing `com.oculus.supportedDevices` value and prepends it to
    the list it builds from `+SupportedDevices=Quest3`. That list is `quest|quest3`; `XR-TOOLCHAIN.md:122` explains the
    `quest` token. The expected manifest value is therefore `quest2|questpro|quest3|quest3s|quest|quest3`.
  - `docs/XR-TOOLCHAIN.md:122` and `:180` both say this line was removed. It was not.
- **Recommended change:**
  - Delete `DefaultEngine.ini:48`.
  - Ask the owner whether Quest 3S is in scope. The plugin has `Quest3S` (`OculusXRHMDRuntimeSettings.h:20`), but the
    plan targets Quest 3 only.
  - Make `package-android.ps1` assert the exact `supportedDevices` value, beside its existing three badging markers.
- **Unconfirmed:**
  - That store review tests every declared device. Only community reports say so.
  - What the engine's own 5.8 Quest 3S addition writes next to the APL's tag.
- **Cost:** S.
- **Status:** applied 2026-10-07, before `DefaultEngine.ini:48` carried `ExtraApplicationSettings` naming quest2, questpro, quest3 and quest3s and only `+SupportedDevices=Quest3` was set, after the `ExtraApplicationSettings` line is deleted and `+SupportedDevices=Quest3S` sits under `+SupportedDevices=Quest3` (owner, 2026-10-07 morning: Quest 3 and Quest 3S), and `package-android.ps1` has a `MARKER_SUPPORTED_DEVICES` check required by `MARKER_RESULT=PASS` and recorded in `apk-markers.txt`; proven: the expected value is `quest|quest3|quest3s`, read from `OculusMobile_APL.xml` (`contains="Quest"` line 41 gives `quest|` at 320, `contains="Quest3"` line 44 gives `quest3|` at 335, `contains="Quest3S"` line 45 gives `quest3s|` at 340, the trailing `|` is cut at 368-369 and written at 371; with no existing tag the merge at 353-361 does nothing), the UE 5.8 `UEDeployAndroid.cs` has no `supportedDevices` or `quest3s` text (its `bPackageForMetaQuest` uses at 356-358, 2344, 2612 and 3036 touch other manifest parts), so the engine adds no tag of its own, and `package-android.ps1 -SelfTest` exits 0 with the three verdicts right (the expected value passes, `quest2|questpro|quest3|quest3s|quest|quest3` and `quest|quest3` fail); not proven: the real APK manifest, because the engine's Android component is not installed, so the aapt output formats the extraction regex expects are untested on a real dump; corrected 2026-10-07: "the engine adds no tag of its own" is wrong. In UE 5.8 `Engine/Source/Runtime/Android/AndroidRuntimeSettings/Private/AndroidRuntimeSettings.cpp`, `PostInitProperties` (221-236) calls `HandleXRSupport` (235; the function is at 88-125). While `bPackageForMetaQuest` is true it appends `ExtraApplicationSettings` with `quest2|questpro|quest3|quest3s` when the tag is missing (101-117) and replaces any value that lacks that string (104-110), on every editor or cook launch, and writes the change to the default config file (117). `OculusMobile_APL.xml` (353-372) then merges that value into the manifest, so the APK would claim quest2 and questpro and the `MARKER_SUPPORTED_DEVICES` check would fail. See `docs/XR-TOOLCHAIN.md` (Manifest). Guard applied 2026-10-07: before, `[/Script/AndroidRuntimeSettings.AndroidRuntimeSettings]` had no `ExtraApplicationSettings`, so every editor launch wrote the quest2|questpro tag back into `DefaultEngine.ini`; after, `ExtraApplicationSettings` is one XML comment that names `com.oculus.supportedDevices` and `quest2|questpro|quest3|quest3s` (the two substrings `HandleXRSupport` `Find`s, 101-103), with a `;` comment above it saying not to delete it; proven: an `UnrealEditor-Cmd.exe` launch of the worktree project (`-ExecCmds=QUIT_EDITOR -unattended -nullrhi -nosplash`, exit 0) left `DefaultEngine.ini` byte-identical (`git hash-object` 8690fa6 before and after, `git diff` empty), and the same launch with the guard line removed rewrote it to `ExtraApplicationSettings=<meta-data android:name="com.oculus.supportedDevices" android:value="quest2|questpro|quest3|quest3s" />`; by reading, the comment is inert in the APK: `UEDeployAndroid.cs` (2950-2958) writes the value into `<application>` verbatim, `OculusMobile_APL.xml` (353-364) walks only `meta-data` elements and a comment is not one, and aapt dumps print no comments, so `package-android.ps1`'s matcher never sees it (script unchanged, `-SelfTest` exits 0); a `-game` run did not re-add the tag even without the guard; not proven: the real APK manifest, because the engine's Android component is not installed.
- **When:** before the gate, together with F1.
- **Verifiable headless:** yes, once packaging works. The APL logs "Adding Meta Quest supported devices tag" during
  packaging, and `aapt dump xmltree` shows the manifest.
- **Registry:** EXTENDS `ship-pipeline-gating`. The package gate that checks the boot marker should also check the
  device list.

### F3. The desktop anti-aliasing key is dead in 5.8, and Quest has no MSAA key

- **Upstream statements:**
  - The UE 5.8 release notes: "Removed r.Mobile.AntiAliasing=3 (MSAA) from the Android_OpenXR device profile. It will no
    longer automatically override the AA mode set in project settings, which already defaults to MSAA for VR Template."
    https://dev.epicgames.com/documentation/unreal-engine/unreal-engine-5-8-release-notes
  - Meta (last updated 2026-04-17): "4x MSAA is extremely cheap on Quest devices. As such, Meta recommends this
    anti-aliasing algorithm." The same page lists `r.Mobile.AntiAliasing=3`.
    https://developers.meta.com/horizon/documentation/unreal/gpu-improved-algorithms/
- **Installed engine source (5.8.3):**
  - Desktop reads `r.AntiAliasingMethod`, whose default is `4`, TSR (`Engine/Source/Runtime/Engine/Private/SceneView.cpp:237-238`).
  - The string `r.DefaultFeature.AntiAliasing` appears in **0** files under `Engine/Source/Runtime` and `Engine/Config`.
  - Mobile reads `r.Mobile.AntiAliasing`, whose default is `2`, TAA (`Engine/Source/Runtime/Core/Private/HAL/ConsoleManager.cpp:4719-4721`).
  - With Mobile HDR off, the engine turns AA off unless it is MSAA (`Engine/Source/Runtime/Engine/Private/SceneUtils.cpp:124-133`).
  - The `Android_OpenXR` device profile no longer sets AA (`Engine/Config/BaseDeviceProfiles.ini:1432-1442`).
- **Repo:**
  - `DefaultEngine.ini:8` sets `r.DefaultFeature.AntiAliasing=3`, which is the UE4 name. The desktop build therefore
    most likely renders with the default TSR, not MSAA, even though `CLAUDE.md` keeps forward shading on "from day one"
    so that desktop frames stay honest.
  - Nothing sets `r.Mobile.AntiAliasing`.
  - Separately, `AndroidEngine.ini:8` sets `r.Mobile.ShadingPath=0` under `[ConsoleVariables]`. 5.8 changed the mobile
    default to deferred: "Multi-pass deferred rendering as the default path across mobile platforms" (release notes
    above), and the engine default is `1` (`ConsoleManager.cpp:3933-3938`).
  - The Interaction SDK v207 known issue says to set it "in the project's DefaultEngine.ini" (page updated Sep 22,
    2026, https://developers.meta.com/horizon/downloads/package/meta-xr-interaction-sdk-unreal/).
- **Recommended change:** in `[/Script/Engine.RendererSettings]`:
  - replace line 8 with `r.AntiAliasingMethod=3`;
  - add `r.Mobile.AntiAliasing=3`;
  - also put `r.Mobile.ShadingPath=0` there. Whether the cook honours this read-only CVar from `[ConsoleVariables]` is
    **unconfirmed**, so the section Meta names is the safe place.
  - Then re-take the desktop captures that tests or reviews compare against.
- **Unconfirmed:** the running desktop value. The source shows the key is unregistered, but no run has logged
  `r.AntiAliasingMethod` yet.
- **Cost:** S, plus one capture re-run.
- **Status:** applied 2026-10-07. Before, `DefaultEngine.ini` set `r.DefaultFeature.AntiAliasing=3`, a UE4 key 5.8 never registers, and nothing set the mobile AA. After, `[/Script/Engine.RendererSettings]` sets `r.AntiAliasingMethod=3`, `r.Mobile.AntiAliasing=3` and `r.Mobile.ShadingPath=0`; `Config/Android/AndroidEngine.ini` is unchanged. Proven: a `UnrealEditor.exe -game -RenderOffScreen` run of the worktree project logged, on master's ini, `r.AntiAliasingMethod = "4"` (TSR), `r.Mobile.AntiAliasing = "2"` and `r.Mobile.ShadingPath = "1"`, all `LastSetBy: Constructor` (`r.MSAACount = "4"` was the only AA value the project set), and with the new keys `3`, `3` and `0`, all `LastSetBy: ProjectSetting`. Not proven: the Quest look (no headset run, and whether the cook honours `r.Mobile.ShadingPath` from this section), and no capture was re-taken.
- **When:** before the desktop gate. The gate judges graphics, and the style pick compares frames.
- **Verifiable headless:** desktop yes. A `-game -RenderOffScreen` run with `-ExecCmds="r.AntiAliasingMethod"` logs
  the value, and a capture shows the edges. Quest no. Not in this worktree.
- **Registry:** N5 `standalone-headset-frame-budgets`.

### F4. Mobile HDR is on by default, and the blink vignette only exists while it is on

- **Upstream statement:** Meta's Project Setup Tool lists "Disable Post Processing" and explains: "Disables mobile
  post-processing, including Mobile HDR" (last updated 2026-04-17).
  https://developers.meta.com/horizon/documentation/unreal/unreal-oculus-utilities-window/
- **Installed engine source:**
  - `r.MobileHDR` defaults to `1` (`ConsoleManager.cpp:3926-3931`).
  - On mobile, post-processing runs only when `IsMobileHDR()` is true (`Engine/Source/Runtime/Renderer/Private/PostProcess/PostProcessing.cpp:273-289`).
  - `r.Mobile.UseHWsRGBEncoding` only takes effect when Mobile HDR is off (`SceneUtils.cpp:66-71`).
- **Repo:**
  - `DefaultEngine.ini:9` sets `r.Mobile.UseHWsRGBEncoding=True`, which does nothing while Mobile HDR is on. No line sets
    `r.MobileHDR`.
  - The blink vignette is a camera post-process `VignetteIntensity` (`apps/vr/Game/Source/MageArenaVR/Hands/MageArenaPawn.cpp:279-287`,
    driven at `:138`, `:150` and `:153`). The camera also overrides exposure and bloom (`MageArenaPawn.cpp:66-78`).
  - The plan's budget allows "no full-screen translucency except the blink vignette" (`docs/PROJECT-PLAN.md:353`).
  - The trade-off: keep Mobile HDR on and Quest pays for the whole post chain; turn it off as Meta recommends and the
    vignette disappears.
- **Recommended change:**
  - Before the gate, rebuild the blink vignette as geometry: a camera-attached mesh with an unlit material, so desktop
    and Quest use the same mechanism.
  - At V1, set `r.MobileHDR=False` and drop the post-process overrides for Quest.
- **Unconfirmed:** whether the `PlayerCameraManager` fade (`StartFade`) still renders with Mobile HDR off.
- **Cost:** S for the config; M for the vignette. `Hands/` is not in the list of files under another worker's edits.
- **Status:** vignette part applied 2026-10-07. Before, the blink vignette was the camera post-process `VignetteIntensity` (`MageArenaPawn.cpp` `SetVignette`); the new `06-blink-midfade.png` shot (taken at fade-out 0.50, vignette 0.575 of the 1.15 peak) had a border/centre mean-luma ratio of 0.695 (border 144.14, centre 207.41), the same as `01-initial-view.png` at 0.698, and the camera fade was not visible either, so the desktop blink showed no vignette at all (`ApplyFlatPresentation` turns the tonemapper off, and UE applies both there); draw calls 31 / 36 / 101 (min / median / max). After, `SetVignette` drives four `/Engine/BasicShapes/Plane` quads attached to the seated camera, sharing one dynamic `M_SimpleUnlitTranslucent` tinted black (that material has no Opacity scalar, so the opacity is the alpha of its `Color` vector), 15 cm ahead of the eye (never nearer than `GNearClippingPlane` + 5 cm), outer edge 65 degrees half-angle (130 degrees), no collision, no shadows, translucency sort priority 1000, hidden at intensity 0; `AMageArenaPawn::VignetteAperture(Intensity, Peak)` is the pure mapping (opacity = intensity / peak clamped to 1, inner half-angle from 65 to 15 degrees along its square root, because the desktop camera sees 45 degrees either side and a linear close to 30 degrees put the edge outside the view); `bOverride_VignetteIntensity` is never set. The 06 ratio is 0.566 (border 117.32, centre 207.41), `01` is pixel-identical (mean absolute difference 0.0), `05` differs by 0.022 of 255, draw calls 32 / 40 / 104 (the quads add to the blink frames). Proven: `build.ps1` exit 0 before and after; the full `MageArena` suite is 172 tests (169 pass, the same 3 known failures) before and 175 (172 pass, the same 3 failures) after, with an empty per-test diff of the original 172; the 3 new tests `MageArena.Hands.BlinkVignette.Mapping`, `.Component` and `.NoPostProcess` pass; mutation (the post-process lines put back and the mesh update skipped): `.Component` and `.NoPostProcess` fail, `.Mapping` still passes, restored and green (run on the first mapping constants, before the 15 degree and square-root change, which touched only `VignetteAperture`); `git diff apps/vr/data/conformance/` is empty, the boot log reads 9 files, SHA1 `11F9B16C…E1106236C2FF` (`11F9B16CBBC77C2469704829C518E4106236C2FF`), and `check-pin.mjs` passes. Not proven: the Quest look, `r.MobileHDR=False` (V1; the exposure and bloom overrides stay), and whether `StartFade` (the `PlayerCameraManager` fade) renders with Mobile HDR off; on desktop it did not show in the 06 shot even before this change. The aperture constants live in `MageArenaPawn.cpp` and move to layout data once `ArenaLayout.cpp` is free.
- **When:** vignette before the desktop gate (it is the blink comfort cue); `r.MobileHDR` at the V1 device sprint.
- **Verifiable headless:** the desktop vignette yes, by capture. The Quest look no.
- **Registry:** N5 (overdraw and full-screen effects are N5 scope).

### F5. Pinned combat data is not staged, so a packaged build would start without it

- **Status:** applied 2026-10-07, before `DefaultGame.ini` stages `../Clips` and `../../data/vr`, after it also stages `../../data/pinned` and `KernelData()` logs the pinned directory, file count and SHA-1 once at boot; proven: Win64 editor build and `MageArena.Kernel.PinHash` pass; and a Win64 BuildCookRun stage lists all 9 `PINNED.json` files under `Saved/StagedBuilds/Windows/data/pinned/`; not proven: the packaged exe was not run.

- **Upstream statement:** from the installed engine source; no web page was fetched, so this is **unconfirmed**
  against Epic's online docs.
  - `Engine/Source/Developer/DeveloperToolSettings/Classes/Settings/ProjectPackagingSettings.h:595-599` says
    `DirectoriesToAlwaysStageAsNonUFS` "is used to stage additional files that you manually load without using the UFS
    ... Note: These paths are relative to your project Content directory".
  - UAT stages exactly the listed directories, as `Content + RelativePath` to `StageContent + RelativePath`
    (`Engine/Source/Programs/AutomationTool/Scripts/CopyBuildToStagingDirectory.Automation.cs:837-844`).
- **Repo:**
  - `Kernel/KernelData.cpp:19` reads `FPaths::ProjectDir()` + `../data/pinned`.
  - `Combat/AbsorbResolver.cpp:152-153` reads `../data/pinned/.../combat.json`.
  - `DefaultGame.ini:9-10` stages only `../Clips` and `../../data/vr`.
  - `docs/PROJECT-PLAN.md:330` requires `MageArenaData` to load "from staged non-asset files" and to log "the data hash
    at boot".
- **Recommended change:**
  - Add `+DirectoriesToAlwaysStageAsNonUFS=(Path="../../data/pinned")`.
  - Add a boot log line with the pin hash, so a package without data fails loudly.
- **Unconfirmed:** that the `../..` staged path resolves the same inside an Android APK as on Win64. Prove it on Win64
  first, then on the first APK.
- **Cost:** S.
- **When:** before the desktop gate if a packaged desktop build is shown there; otherwise before the first APK.
- **Verifiable headless:** yes. Run `RunUAT BuildCookRun -platform=Win64 -cook -stage`, check
  `Saved/StagedBuilds/Windows/data/pinned/`, and boot the staged build. Not in this worktree.
- **Registry:** N7 `cross-runtime-rules-conformance` (data pins).

### F6. Foveation is off, and on 5.8 it can only be set per project

- **Upstream statements:**
  - Meta plugin settings (last updated 2026-04-14): "In most cases, dynamic foveation is a better option than using a
    static FFR level".
    https://developers.meta.com/horizon/documentation/unreal/unreal-plugin-settings/
  - The v207 known issue (Updated: Sep 23, 2026): "Changing Fixed Foveated Rendering settings at runtime is not supported
    on UE 5.7+". Workaround: "Use the project-level fixed foveated rendering settings in the MetaXR section of the
    Project Settings".
    https://developers.meta.com/vr/downloads/package/unreal-engine-5-integration/
  - The UE 5.8 notes: "Update FBFoveation CVars to be read-only to avoid crashes when toggling at runtime."
- **Plugin and engine source:**
  - Plugin defaults: `FoveatedRenderingLevel` Off, `bDynamicFoveatedRendering` true
    (`MetaXR/Source/OculusXRHMD/Private/OculusXRHMD_Settings.cpp:25-26`).
  - On session creation under Native OpenXR, the plugin turns on `bIsFBFoveationEnabled` and copies those two values
    into `xr.OpenXRFBFoveationLevel` and `xr.OpenXRFBFoveationDynamic` (`.../OpenXR/OculusXRCoreExtensionPlugin.cpp:73-91`).
  - The runtime setter is compiled out on 5.7 and later (`.../OculusXRFunctionLibraryOpenXR.cpp:274-277`).
  - The engine clamps the level to `XR_FOVEATION_LEVEL_HIGH_FB`
    (`Engine/Plugins/Runtime/OpenXR/Source/OpenXRHMD/Private/FBFoveationImageGenerator.cpp:46`). So the plugin's
    `HighTop` value (4) most likely acts as `High`; **unconfirmed** on device.
- **Repo:**
  - `DefaultEngine.ini:50-56` sets no foveation, so the level is Off.
  - The plan's perf fallback P-1 reads "fixed foveation high (device)" (`docs/PROJECT-PLAN.md:205`), as if it were a
    runtime knob.
- **Recommended change:**
  - Add `FoveatedRenderingLevel=High` (or `Medium`) and `bDynamicFoveatedRendering=True` under
    `[/Script/OculusXRHMD.OculusXRHMDRuntimeSettings]`.
  - Rewrite P-1 as a rebuilt project setting.
  - Never call `SetFoveatedRenderingLevel` at runtime.
  - Check that sigil glyphs and peripheral threat cues stay legible.
- **Cost:** S.
- **When:** at the V1 device sprint. Legibility and the GPU gain need the headset and OVR Metrics.
- **Verifiable headless:** no. Only the config line can be checked.
- **Registry:** N5.

### F7. Keep hand tracking on LOW, but stop assuming the hands arrive at 72 Hz

- **Status:** (b) applied 2026-10-07, before the clips assume 72 Hz pose updates and no test checked slower hands, after `Tests/HeldSampleTests.cpp` models the worst case in memory (each pose held for 1/30 s at camera phases 0, 1/90 and 2/90 s, still emitted at 72 Hz, nothing predicted, no clip file written) and plays the held copy through the real pipeline: `MageArena.Hands.HeldSample.TransformIsReal`, `.Ward`, `.Blink`, `.SigilLines` and `.SigilCorpus`, with a `KnownHeldMisses` ratchet in which a miss outside the table and a table entry that now matches both fail; proven: `build.ps1` exit 0, the full `MageArena` suite 166 tests (163 pass, the same 3 known failures `MageArenaDesign.Duel.FireSeated`, `Session.FullSeated`, `Session.Wave1Seated`) against 161 before, the match count to the 72 Hz result is ward-raise 9/9, blink-left 9/9, blink-right 9/9, blink-back 6/9 (the 3 misses are all `sloppy`: the held stream gives a bolt, not a blink) and sigil-line1, line2 and line3 9/9 each, the ward onset moves later by min 111, median 153 and max 250 ms (held onset 0.125 to 0.292 s against 0.014 to 0.042 s at 72 Hz) and the perfect-window result changes in all 9 cases (72 Hz is perfect at the onset and at onset + 0.15 s; held is perfect at neither in 5 cases and only at the window end in 4), the sigil corpus through the pipeline is 257/270 at 72 Hz and 256/270 held (95% and 94% rounded down) with 0 false casts in both (0/180 impostors, 0/5 noise clips), the floors are 94% and 0; mutation: camera rate 72 turns the hold off and `TransformIsReal` fails (19 source poses against 20 held on `ward-raise.normal`), and `Blink` fails because the 3 known misses now match, removing the `blink-back.sloppy.p1` entry fails `Blink`, and both are restored and green; not proven: the real runtime rate and whether the runtime predicts between updates (held is therefore a worst case), and (c), the LOW against HIGH A/B in V2; verdict: the gesture set survives the 30 Hz worst case partly, because blink and sigil casting hold (except `blink-back.sloppy`, which becomes a bolt) while the ward still raises but its onset, the anchor of the 0.15 s perfect window, arrives 111 to 250 ms late, so a perfect block is lost on held hands. Follow-up applied 2026-10-07: the cause was the speed both detectors measure frame to frame, which reads 0 on a re-sent pose and about 3 times the real speed on the change, so the ward's fast onset walk stopped on a repeat and the blink released on the repeat after a stepped wind-up burst (logged trace, `blink-back.sloppy` phase 0: 1.29 m/s at yaw 6.4 degrees at 0.069 s, released by the 0 m/s repeat at 0.083 s); `Gestures/HeldPointRate.h` now treats a repeat within 0.05 s of the change as no new sample (the ward carries its last measured speed, the blink skips the frame, the next change is measured over the whole hold) and a longer repeat as stillness measured as before, with no threshold changed; before (the new tests with only the detector change reverted): onset +111, +153 and +250 ms (min, median, max), the perfect result at the window edges changed in 9 of 9 cases, the interior hits at the 72 Hz onset + 0.05 s and + 0.10 s perfect in 0 of 9 held cases, and `blink-back.sloppy` a bolt in 3 of 3 phases; after: onset +14, +28 and +42 ms (bound 47.2 ms, 1/30 + 1/72 s, met 9/9), interior hits perfect 9 of 9, the window-edge result still changes in 9 of 9 cases because a held onset lands one or more 72 Hz frames late and the hit at the 72 Hz onset is then before it (logged only: quantization cannot keep exact edges), blink-back 9/9 and every other HeldSample ratchet at 9/9, `KnownHeldMisses` and the new two-sided `KnownHeldWardMisses` both empty; proven: `build.ps1` exit 0, the full `MageArena` suite 166 tests with 163 passing and the same 3 known failures, the sigil corpus 257/270 at 72 Hz and 256/270 held with 0 false casts in both (unchanged: the sigil path does not use these speeds), and the mutation proof (only the detector change reverted, the new tests kept: `HeldSample.Ward` fails in all 9 cases and `HeldSample.Blink` fails on the 3 `blink-back.sloppy` cases; restored, all 5 `HeldSample` tests are green); not proven: the real runtime rate and whether the runtime predicts between updates (a separate research pass), and the staff detector, which has no held test; also not proven: held hands combined with the injected latency of D-G3 (`Ward.OnsetCompensation` still runs at 72 Hz); revised verdict: the gesture set now survives the 30 Hz held worst case for ward, blink and sigil, and a perfect ward survives everywhere except within about 42 ms of the window edges, because the held onset lags by at most 42 ms, which is less than the 60 ms latency that D-G3 tolerates; text applied 2026-10-07: FEASIBILITY G2 (the plan rows say "display time", `CLIP-SCHEMA.md` says the runtime extrapolates to display time and reserves `captureTime`, and PROJECT-PLAN V1 has the `Camera FPS` day-one step) and G4 (F7(c) now says MAX equals HIGH and lists what the A/B logs).

- **Upstream statements:**
  - Meta's Fast Motion Mode page (last updated 2026-08-11; filed under Unity, but it describes the same manifest flag):
    "It is highly recommended to first test out your title without FMM enabled, and then enable it only if you observe
    high tracking loss due to fast hand motion." Also: "FMM optimizations may increase jitter, impacting high accuracy
    interactions like direct touch or typing."
    https://developers.meta.com/horizon/documentation/unity/fast-motion-mode/
  - Meta's Unreal plugin settings: "Higher tracking frequencies will reserve some performance headroom from the app's
    budget" (link in F6).
  - Meta's Unreal hand-tracking overview (last updated 2026-08-31) on FMM: "Provides improved tracking of fast movements
    common in fitness and rhythm apps (60Hz)".
    https://developers.meta.com/horizon/documentation/unreal/unreal-hand-tracking-overview/
  - Third party, UploadVR (Dec 16, 2025): "Normally, Quest's hand tracking samples the tracking cameras at 30Hz."
    https://www.uploadvr.com/quest-hand-tracking-2-4-improves-fast-motion-mode/
    This was read with WebFetch only. Meta's own pages do not state the default rate, so the 30 Hz figure is
    **unconfirmed** first-party.
- **Repo:**
  - `DefaultEngine.ini:54` sets `HandTrackingFrequency=LOW`. The APL writes it to `com.oculus.handtracking.frequency`
    (`OculusMobile_APL.xml:376-382`).
  - `docs/CLIP-SCHEMA.md:33` reads "`hz` | `72`. Schema v1 is the Quest sample rate.", and `:59` says synthetic clips
    sample "at exactly `i / 72` seconds".
  - `docs/PROJECT-PLAN.md:336` and `docs/DESKTOP-INPUT.md:15` both say "joints (72 Hz)".
  - The perfect ward window is 0.15 s (`DESKTOP-INPUT.md:36`), which is about 4.5 camera samples at 30 Hz.
- **Recommended change:**
  - (a) Keep LOW. It is Meta's advice, and FMM jitter works against fingertip sigil drawing.
  - (b) Before the gate, add a held-sample clip variant: each pose held for 1/30 s and still emitted at 72 Hz. Run the
    ward, blink and sigil tests on it. Reword `CLIP-SCHEMA.md` so 72 is the display rate, not "the Quest sample rate".
  - (c) In V2, A/B test LOW against HIGH on the real-hand corpus: blink-flick recall, sigil accuracy, false casts per
    minute, and the headroom cost. MAX is the same as HIGH (Meta's FMM page, FEASIBILITY G4), so the A/B is LOW against
    HIGH. It must log the `Camera FPS` logcat line (to prove which rate ran, 50 or 60 Hz), the CPU and GPU levels from
    OVR Metrics, and the room light.
- **Unconfirmed:** the rate at which the runtime hands predicted joint poses to the app.
- **Cost:** (b) S-M in `apps/vr/tools/clipgen` plus tests; (c) M.
- **When:** (b) before the desktop gate; (c) at V2.
- **Verifiable headless:** (b) yes; (c) no.
- **Registry:** N2 `hand-tracked-timing-windows`.

### F8. The system gesture is reserved, but the hand frame cannot carry it

- **Status:** applied 2026-10-07 on desktop only, before `FHandFrame` had no status field and the ward, blink, sigil and staff detectors read every frame, after `FHandFrame::bSystemGesture` (the last member) is filled by clip schema `mage-arena/hand-clip@2` (optional frame field `sys`, `@1` stays closed) and each of the four detector subsystems drops a hand's frames while the bit is set and cancels what that hand had in progress, through one `FSystemGestureGate` in `Hands/SystemGestureGate.h`; proven: `build.ps1` exit 0 before and after, the full `MageArena` suite 158 tests before (155 pass, 3 known failures `MageArenaDesign.Duel.FireSeated`, `Session.FullSeated`, `Session.Wave1Seated`) and 161 after (158 pass, the same 3 failures), the 3 new tests `MageArena.Hands.SystemGesture.ZeroEvents`, `.GateIsTheCause` (ward, blink, sigil and staff each fire on their clip, none with `sys` forced) and `.SchemaV1Closed` pass, with the gate disabled `GateIsTheCause` and `ZeroEvents` fail (4 events and 1 sigil reject), and `validate.mjs` passes 655 clips including `Clips/system/system-gesture.normal.jsonl`; not proven: the live flag from `OculusXRInput` (V1, needs a `Build.cs` dependency), the both-palms resume gesture in `Session/SessionFlow.cpp`, which is not gated because `Session/` is another worker's lane, a gesture that is cancelled and then continues after the bit clears (no test builds that stream), and replacing the palm-up pause, which is the owner's call.

- **Upstream statement:** VRC.Quest.Input.8 (last updated 2024-07-31): "the system gesture is reserved, and should not
  trigger any other actions within the app". Its test step reads: "place your hand up with an open palm towards your
  headset, and pinch to perform a system gesture. No other gesture events should be processed."
  https://developers.meta.com/vr/resources/vrc-quest-input-8/
- **Engine and plugin source:**
  - OpenXR exposes `XR_HAND_TRACKING_AIM_SYSTEM_GESTURE_BIT_FB` (`Engine/Source/ThirdParty/OpenXR/include/openxr/openxr.h:3917`).
  - The Meta plugin maps it to `EHandStatus::SystemGestureInProgress`
    (`MetaXR/Source/OculusXRInput/Private/OculusXRInputHandTrackingExtensionPlugin.cpp:414-416`).
  - The simulator boot listed `XR_FB_hand_tracking_aim` (`docs/XR-TOOLCHAIN.md:150`).
- **Repo:**
  - `FHandFrame` has no status field (`apps/vr/Game/Source/MageArenaVR/Hands/HandFrame.h:23-30`).
  - The ward accepts a palm-up pose that faces an elevated threat (`Tests/WardTests.cpp:2040-2068`).
  - `docs/DESKTOP-INPUT.md:39` plans pause as a "palm-up menu gesture".
  - `MageArenaVR.Build.cs` has no `OculusXRInput` dependency.
- **Recommended change:**
  - Add a system-gesture bit to `FHandFrame` and to the clip schema (a v2 field).
  - Gate the ward, blink, sigil and pause detectors while the bit is set.
  - Add a synthetic "system gesture" clip (open palm toward the face, then a pinch) whose acceptance is zero game events.
  - Ask the owner to replace the in-game palm-up pause with the OS menu plus OpenXR focus loss, which BACKLOG already
    lists for V1.
  - Do the live wiring through `OculusXRInput` at V1. It needs a `Build.cs` dependency, which waits until the other
    worker has committed that file.
- **Cost:** M.
- **When:** schema and clip test before the desktop gate; live wiring at V1.
- **Verifiable headless:** the clip test yes; the live flag no.
- **Registry:** N1 `drawn-gesture-command-recognition` (the reject class).

### F9. The engine moved to 5.8.3 under a toolchain record that says 5.8.2

- **Status:** applied 2026-10-07, before the records said 5.8.2, after `docs/XR-TOOLCHAIN.md` (a dated re-check section) and the `CLAUDE.md` Build section record 5.8.3 (CL 58210709); proven: `Build.version`, the plugin BuildId match, and a 393 s Win64 editor build in a builder worktree without the MetaXR plugins; not proven: the plugin load and the simulator boot (`MAGEVR_BOOT_OK`) on 5.8.3, and the freeze on launcher auto-updates, which is the owner's call.

- **Upstream statements:**
  - The 5.8.3 hotfix (2026-09-22) includes "OpenXR: retry xrCreateSwapchain without XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT
    when creation fails with it requested."
    https://forums.unrealengine.com/t/5-8-3-hotfix-released/2833315
  - The Meta XR download page shows 207.0 as current (Updated: Sep 23, 2026).
    https://developers.meta.com/vr/downloads/package/unreal-engine-5-integration/
- **Local:**
  - `Build.version` shows PatchVersion 3 and Changelist 58210709. The file is dated 2026-10-03 11:15 +02:00.
  - Both plugins' `UnrealEditor.modules` and the engine's `UnrealEditor.version` share BuildId `55116800`, so the v207
    binaries still match.
- **Repo:** `docs/XR-TOOLCHAIN.md:3` and the `CLAUDE.md` Build section record 5.8.2 / CL 56702186.
- **Recommended change:**
  - Record 5.8.3 in both files.
  - Re-run the T05 checks once on 5.8.3 and record the result: the editor loads the plugin modules, and the simulator
    logs `MAGEVR_BOOT_OK`.
  - Ask the owner to stop launcher auto-updates of UE_5.8 until 2026-11-18.
- **Unconfirmed:** whether the launcher updated the engine on its own or a person did.
- **Cost:** S.
- **When:** before the desktop gate.
- **Verifiable headless:** yes, with the editor `-nullrhi` suite plus the simulator boot. Not in this worktree.
- **Registry:** EXTENDS `engine-pitfall-corpus` (a plugin binary against a different engine build; here the engine
  moved under a pinned plugin).

### F10. If AppSW is ever needed, 5.8 needs different switches from the old guides

- **Upstream statements:**
  - Meta's AppSW page (last updated 2026-05-13): "AppSW requires Mobile Multi-View and Vulkan. For UE versions before
    5.7, the Meta XR Plugin with the OVRPlugin OpenXR backend is also required. For UE 5.7 and later, SpaceWarp support
    is built into the engine."
  - On `xr.OpenXRFrameSynthesis`, the same page says: "(the older CVar is deprecated in 5.7 and produces errors)".
  - It also warns that AppSW "requires modifying your app's materials and render pipeline".
    https://developers.meta.com/horizon/documentation/unreal/unreal-asw/
  - VRC.Quest.Performance.1 (last updated 2025-10-22): "Interactive applications may use a rendering rate (fps) of half
    the refresh rate (such as 30 fps for 60 Hz, 36 fps for 72 Hz), for portions of their experience utilizing
    Application SpaceWarp."
    https://developers.meta.com/horizon/resources/vrc-quest-performance-1/
- **Engine and plugin source:**
  - `xr.OpenXRFrameSynthesis` defaults to `0`. Its help text says it is "Currently only supported when using the Vulkan
    mobile renderer, using mobile multi-view, and r.Velocity.DirectlyRenderOpenXRMotionVectors=True"
    (`Engine/Plugins/Runtime/OpenXR/Source/OpenXRHMD/Private/OpenXRHMD.cpp:175-180`).
  - `xr.PreferFBSpaceWarp` defaults to `false` (`OpenXRHMDModule.cpp:44-48`). Yet the comment at `:568-570` says
    `XR_EXT_frame_synthesis` "is not currently working properly on Oculus runtimes, so XR_FB_space_warp is preferred on
    Quest devices".
  - The Meta plugin's own SpaceWarp code compiles only below 5.7 (`MetaXR/.../OpenXR/OculusXRSpaceWarp.cpp:14`).
- **Repo:**
  - No SpaceWarp setting exists.
  - The plan's fallbacks P-1 to P-3 (`docs/PROJECT-PLAN.md:205`) do not include AppSW.
- **Recommended change:** nothing now. Add AppSW to the plan as P-4, with exactly these switches:
  - `xr.OpenXRFrameSynthesis=True`
  - `r.Velocity.DirectlyRenderOpenXRMotionVectors=True`
  - `xr.PreferFBSpaceWarp=True`

  The cost: TAA and motion blur turn off (harmless with MSAA), and every material needs correct velocity.
- **Unconfirmed:**
  - Whether the Quest runtime exposes `XR_EXT_frame_synthesis` at all.
  - How fast hands and projectiles look under synthesis.
- **Cost:** M.
- **When:** V1, and only if p95 stays above 13.9 ms after P-1 to P-3.
- **Verifiable headless:** no.
- **Registry:** N5.

## 4. Checked, no change needed

- **72 Hz is allowed and is the floor for interactive apps.** VRC.Quest.Performance.1: "Interactive applications must
  use a refresh rate of 72 Hz, 80 Hz, 90 Hz, 96 Hz, 100 Hz or 120 Hz." and "Apps must maintain a rendering rate of at
  least 60 fps" (link in F10). The plan's 13.9 ms budget stands.
- **APK size.** VRC.Quest.Packaging.5 (last updated 2024-07-31): "APK files must be less than 1 GB in size, but can be
  accompanied by multiple expansion files up to 4 GB each."
  https://developers.meta.com/horizon/resources/vrc-quest-packaging-5/
  The 1 GB budget with `bPackageDataInsideApk=True` is consistent.
- **`HandTrackingVersion=Default`.** Plugin settings: "V2 provides improved tracking accuracy. Default uses the
  platform's preferred version" (link in F6). Keep it.
- **CPU/GPU levels.** The plugin defaults are CPU `SustainedLow` and GPU `SustainedHigh`
  (`OculusXRHMD_Settings.cpp:22-23`). Revisit only with a device capture.
- **Late latching.** The sub-agent reports Meta's Native OpenXR page marks it as fork-only. I could not find that
  sentence in the raw page, so this is **unconfirmed**. Leave it off either way, since the project uses the stock engine.
- **Native OpenXR hands.** "OpenXR Hand Tracking plugin must be enabled for hand tracking" (last updated 2026-04-17,
  https://developers.meta.com/horizon/documentation/unreal/unreal-openxr/). The Interaction SDK descriptor already
  enables `OpenXRHandTracking`.
- **Unused Meta modules.** MRUtilityKit, Anchors, Scene, Colocation, Passthrough, Movement, EyeTracker and Telemetry all
  load at `PostConfigInit`. Their APK-size and start-up cost is unmeasured and **unconfirmed**. Look at it with the
  first APK size breakdown at V1.

## 5. Sources

| # | URL | Page date | HTTP on 2026-10-07 |
|---|---|---|---|
| u1 | https://developers.meta.com/horizon/blog/meta-quest-apps-android-14-march-1/ | Nov 24, 2025 (update Feb 6, 2026) | 200 |
| u2 | https://developers.meta.com/horizon/resources/publish-mobile-manifest/ | 2026-09-30 | 200 |
| u3 | https://forums.unrealengine.com/t/5-8-2-hotfix-released/2746335 | 2026-08-25 | 200 |
| u4 | https://forums.unrealengine.com/t/5-8-3-hotfix-released/2833315 | 2026-09-22 | 200 |
| u5 | https://dev.epicgames.com/documentation/unreal-engine/unreal-engine-5-8-release-notes | updated_at 2026-06-23 | 200 |
| u6 | https://developers.meta.com/horizon/documentation/unreal/gpu-improved-algorithms/ | 2026-04-17 | 200 |
| u7 | https://developers.meta.com/horizon/documentation/unreal/unreal-oculus-utilities-window/ | 2026-04-17 | 200 |
| u8 | https://developers.meta.com/horizon/documentation/native/android/os-compatibility-mode/ | 2026-09-08 | 200 |
| u9 | https://developers.meta.com/horizon/documentation/unreal/unreal-plugin-settings/ | 2026-04-14 | 200 |
| u10 | https://developers.meta.com/vr/downloads/package/unreal-engine-5-integration/ | Sep 23, 2026 | 200 |
| u11 | https://developers.meta.com/horizon/documentation/unity/fast-motion-mode/ | 2026-08-11 | 200 |
| u12 | https://developers.meta.com/horizon/documentation/unreal/unreal-hand-tracking-overview/ | 2026-08-31 | 200 |
| u13 | https://www.uploadvr.com/quest-hand-tracking-2-4-improves-fast-motion-mode/ | Dec 16, 2025 (WebFetch) | curl timeout (exit 28) |
| u14 | https://developers.meta.com/vr/resources/vrc-quest-input-8/ | 2024-07-31 | 200 |
| u15 | https://developers.meta.com/horizon/documentation/unreal/unreal-asw/ | 2026-05-13 | 200 |
| u16 | https://developers.meta.com/horizon/resources/vrc-quest-performance-1/ | 2025-10-22 | 200 |
| u17 | https://developers.meta.com/horizon/downloads/package/meta-xr-interaction-sdk-unreal/ | Sep 22, 2026 | 200 |
| u18 | https://developers.meta.com/horizon/resources/vrc-quest-packaging-5/ | 2024-07-31 | 200 |
| u19 | https://developers.meta.com/horizon/documentation/unreal/unreal-openxr/ | 2026-04-17 | 200 |

## 6. Commands run for this file, with exit codes

**Tool calls that are not shell commands, so they have no exit code:**

- WebSearch, 3 queries:
  - "Unreal Engine DirectoriesToAlwaysStageAsNonUFS outside project directory staging"
  - "com.oculus.supportedDevices manifest Meta Quest store review supported devices performance requirement"
  - "Meta Quest hand tracking default frequency 30Hz high frequency 60Hz com.oculus.handtracking.frequency"
- WebFetch, 3 calls: u2 twice, u13 once.
- A research sub-agent made about 78 search and fetch calls. Its log is not reproduced here. Nothing in this file rests
  on it alone: each quote above was re-found by the `grep` commands below, except u13 and the late-latching line, both
  marked.

**Shell, run from the worktree root.** Pages were downloaded into a fresh directory outside the repo. Each
`grep -o -m1 '<quote>' ... | head -1` line prints the quote it found; an empty match would print nothing. The two
logs below are verbatim, except that trailing whitespace was trimmed.

**Log 1: downloads and quote checks**

```
$ curl -sL -A Mozilla/5.0 --max-time 60 -o /tmp/mav-web2-61e06ee9/u1.html -w '%{http_code} %{size_download} u1\n' 'https://developers.meta.com/horizon/blog/meta-quest-apps-android-14-march-1/'
200 622113 u1
[exit 0]

$ curl -sL -A Mozilla/5.0 --max-time 60 -o /tmp/mav-web2-61e06ee9/u2.html -w '%{http_code} %{size_download} u2\n' 'https://developers.meta.com/horizon/resources/publish-mobile-manifest/'
200 678155 u2
[exit 0]

$ curl -sL -A Mozilla/5.0 --max-time 60 -o /tmp/mav-web2-61e06ee9/u3.html -w '%{http_code} %{size_download} u3\n' 'https://forums.unrealengine.com/t/5-8-2-hotfix-released/2746335'
200 41360 u3
[exit 0]

$ curl -sL -A Mozilla/5.0 --max-time 60 -o /tmp/mav-web2-61e06ee9/u4.html -w '%{http_code} %{size_download} u4\n' 'https://forums.unrealengine.com/t/5-8-3-hotfix-released/2833315'
200 26901 u4
[exit 0]

$ curl -sL -A Mozilla/5.0 --max-time 60 -o /tmp/mav-web2-61e06ee9/u5.html -w '%{http_code} %{size_download} u5\n' 'https://dev.epicgames.com/documentation/unreal-engine/unreal-engine-5-8-release-notes'
200 5380840 u5
[exit 0]

$ curl -sL -A Mozilla/5.0 --max-time 60 -o /tmp/mav-web2-61e06ee9/u6.html -w '%{http_code} %{size_download} u6\n' 'https://developers.meta.com/horizon/documentation/unreal/gpu-improved-algorithms/'
200 627550 u6
[exit 0]

$ curl -sL -A Mozilla/5.0 --max-time 60 -o /tmp/mav-web2-61e06ee9/u7.html -w '%{http_code} %{size_download} u7\n' 'https://developers.meta.com/horizon/documentation/unreal/unreal-oculus-utilities-window/'
200 677476 u7
[exit 0]

$ curl -sL -A Mozilla/5.0 --max-time 60 -o /tmp/mav-web2-61e06ee9/u8.html -w '%{http_code} %{size_download} u8\n' 'https://developers.meta.com/horizon/documentation/native/android/os-compatibility-mode/'
200 659798 u8
[exit 0]

$ curl -sL -A Mozilla/5.0 --max-time 60 -o /tmp/mav-web2-61e06ee9/u9.html -w '%{http_code} %{size_download} u9\n' 'https://developers.meta.com/horizon/documentation/unreal/unreal-plugin-settings/'
200 694136 u9
[exit 0]

$ curl -sL -A Mozilla/5.0 --max-time 60 -o /tmp/mav-web2-61e06ee9/u10.html -w '%{http_code} %{size_download} u10\n' 'https://developers.meta.com/vr/downloads/package/unreal-engine-5-integration/'
200 639958 u10
[exit 0]

$ curl -sL -A Mozilla/5.0 --max-time 60 -o /tmp/mav-web2-61e06ee9/u11.html -w '%{http_code} %{size_download} u11\n' 'https://developers.meta.com/horizon/documentation/unity/fast-motion-mode/'
200 675470 u11
[exit 0]

$ curl -sL -A Mozilla/5.0 --max-time 60 -o /tmp/mav-web2-61e06ee9/u12.html -w '%{http_code} %{size_download} u12\n' 'https://developers.meta.com/horizon/documentation/unreal/unreal-hand-tracking-overview/'
200 729679 u12
[exit 0]

$ curl -sL -A Mozilla/5.0 --max-time 60 -o /tmp/mav-web2-61e06ee9/u13.html -w '%{http_code} %{size_download} u13\n' 'https://www.uploadvr.com/quest-hand-tracking-2-4-improves-fast-motion-mode/'
000 0 u13
[exit 28]

$ curl -sL -A Mozilla/5.0 --max-time 60 -o /tmp/mav-web2-61e06ee9/u14.html -w '%{http_code} %{size_download} u14\n' 'https://developers.meta.com/vr/resources/vrc-quest-input-8/'
200 672871 u14
[exit 0]

$ curl -sL -A Mozilla/5.0 --max-time 60 -o /tmp/mav-web2-61e06ee9/u15.html -w '%{http_code} %{size_download} u15\n' 'https://developers.meta.com/horizon/documentation/unreal/unreal-asw/'
200 639987 u15
[exit 0]

$ curl -sL -A Mozilla/5.0 --max-time 60 -o /tmp/mav-web2-61e06ee9/u16.html -w '%{http_code} %{size_download} u16\n' 'https://developers.meta.com/horizon/resources/vrc-quest-performance-1/'
200 702069 u16
[exit 0]

$ curl -sL -A Mozilla/5.0 --max-time 60 -o /tmp/mav-web2-61e06ee9/u17.html -w '%{http_code} %{size_download} u17\n' 'https://developers.meta.com/horizon/downloads/package/meta-xr-interaction-sdk-unreal/'
200 527163 u17
[exit 0]

$ curl -sL -A Mozilla/5.0 --max-time 60 -o /tmp/mav-web2-61e06ee9/u18.html -w '%{http_code} %{size_download} u18\n' 'https://developers.meta.com/horizon/resources/vrc-quest-packaging-5/'
200 669553 u18
[exit 0]

$ LC_ALL=C grep -o -m1 'Starting March 1, 2026, all new Meta Horizon apps created in the Developer Dashboard[^\]*' /tmp/mav-web2-61e06ee9/u1.html | head -1 | cut -c1-330
Starting March 1, 2026, all new Meta Horizon apps created in the Developer Dashboard are required to target Android 14 (API level 34). This means targetSdkVersion must be API level 34, though minSdkVersion will not be impacted and can still use API level 32. This requirement is an update to the current target of
[exit 0]

$ LC_ALL=C grep -o -m1 'API level 34 will be enforced during binary upload[^\]*' /tmp/mav-web2-61e06ee9/u1.html | head -1 | cut -c1-260
API level 34 will be enforced during binary upload, meaning you will not be able to upload binaries with a lower targetSdkVersion level for apps created after March 1, 2026.
[exit 0]

$ LC_ALL=C grep -o -m1 'November 24, 2025' /tmp/mav-web2-61e06ee9/u1.html | head -1 | cut -c1-220
November 24, 2025
[exit 0]

$ LC_ALL=C grep -o -m1 'Apps created since March 1, 2026, must set their targetSdkVersion to 34\|must set their targetSdkVersion to 34' /tmp/mav-web2-61e06ee9/u2.html | head -1 | cut -c1-220
must set their targetSdkVersion to 34
[exit 0]

$ LC_ALL=C grep -o -m1 '32-34 for immersive, 32-36 for 2D' /tmp/mav-web2-61e06ee9/u2.html | head -1 | cut -c1-220
32-34 for immersive, 32-36 for 2D
[exit 0]

$ LC_ALL=C grep -o -m1 'lastUpdated[^,]\{0,20\}' /tmp/mav-web2-61e06ee9/u2.html | head -1 | cut -c1-220
lastUpdated\":\"2026-09-30\"}
[exit 0]

$ LC_ALL=C grep -o -m1 'Bumped up Target SDK to API 36' /tmp/mav-web2-61e06ee9/u3.html | head -1 | cut -c1-220
Bumped up Target SDK to API 36
[exit 0]

$ LC_ALL=C grep -o -m1 'published_time" content="[^"]*"' /tmp/mav-web2-61e06ee9/u3.html | head -1 | cut -c1-220
published_time" content="2026-08-25T13:09:12+00:00"
[exit 0]

$ LC_ALL=C grep -o -m1 'retry xrCreateSwapchain without XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT[^<]*' /tmp/mav-web2-61e06ee9/u4.html | head -1 | cut -c1-220
retry xrCreateSwapchain without XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT when creation fails with it requested.
[exit 0]

$ LC_ALL=C grep -o -m1 'published_time" content="[^"]*"' /tmp/mav-web2-61e06ee9/u4.html | head -1 | cut -c1-220
published_time" content="2026-09-22T13:29:56+00:00"
[exit 0]

$ LC_ALL=C grep -o -m1 'Removed r.Mobile.AntiAliasing=3 (MSAA) from the Android_OpenXR device profile[^<]*' /tmp/mav-web2-61e06ee9/u5.html | head -1 | cut -c1-300
Removed r.Mobile.AntiAliasing=3 (MSAA) from the Android_OpenXR device profile. It will no longer automatically override the AA mode set in project settings, which already defaults to MSAA for VR Template.
[exit 0]

$ LC_ALL=C grep -o -m1 'Multi-pass deferred rendering as the default path across mobile platforms[^<]*' /tmp/mav-web2-61e06ee9/u5.html | head -1 | cut -c1-200
Multi-pass deferred rendering as the default path across mobile platforms (with forward rendering still available as an opt-in).
[exit 0]

$ LC_ALL=C grep -o -m1 'Update FBFoveation CVars to be read-only[^<]*' /tmp/mav-web2-61e06ee9/u5.html | head -1 | cut -c1-220
Update FBFoveation CVars to be read-only to avoid crashes when toggling at runtime.
[exit 0]

$ LC_ALL=C grep -o -m1 'Fix an issue where OpenXR frame synthesis/space warp would not work properly[^<]*' /tmp/mav-web2-61e06ee9/u5.html | head -1 | cut -c1-260
Fix an issue where OpenXR frame synthesis/space warp would not work properly when materials using translucent velocity (such as Niagara particles) were onscreen.
[exit 0]

$ LC_ALL=C grep -o -m1 'Adding Meta Quest 3s as supported device when bPackageForMetaQuest is enabled' /tmp/mav-web2-61e06ee9/u5.html | head -1 | cut -c1-220
Adding Meta Quest 3s as supported device when bPackageForMetaQuest is enabled
[exit 0]

$ LC_ALL=C grep -o -m1 '4x MSAA is extremely cheap on Quest devices. As such, Meta recommends this anti-aliasing algorithm.' /tmp/mav-web2-61e06ee9/u6.html | head -1 | cut -c1-220
4x MSAA is extremely cheap on Quest devices. As such, Meta recommends this anti-aliasing algorithm.
[exit 0]

$ LC_ALL=C grep -o -m1 'r.Mobile.AntiAliasing=3' /tmp/mav-web2-61e06ee9/u6.html | head -1 | cut -c1-220
r.Mobile.AntiAliasing=3
[exit 0]

$ LC_ALL=C grep -o -m1 'lastUpdated[^,]\{0,20\}' /tmp/mav-web2-61e06ee9/u6.html | head -1 | cut -c1-220
lastUpdated\":\"2026-04-17\"}
[exit 0]

$ LC_ALL=C grep -o -m1 'Disables mobile post-processing, including Mobile HDR' /tmp/mav-web2-61e06ee9/u7.html | head -1 | cut -c1-220
Disables mobile post-processing, including Mobile HDR
[exit 0]

$ LC_ALL=C grep -o -m1 'lastUpdated[^,]\{0,20\}' /tmp/mav-web2-61e06ee9/u7.html | head -1 | cut -c1-220
lastUpdated\":\"2026-04-17\"}
[exit 0]

$ LC_ALL=C grep -o -m1 'manifest entry lists the VR devices that your app supports. The runtime uses this list to decide whether to turn on compatibility mode.' /tmp/mav-web2-61e06ee9/u8.html | head -1 | cut -c1-220
manifest entry lists the VR devices that your app supports. The runtime uses this list to decide whether to turn on compatibility mode.
[exit 0]

$ LC_ALL=C grep -o -m1 'lastUpdated[^,]\{0,20\}' /tmp/mav-web2-61e06ee9/u8.html | head -1 | cut -c1-220
lastUpdated\":\"2026-09-08\"}
[exit 0]

$ LC_ALL=C grep -o -m1 'Higher tracking frequencies will reserve some performance headroom from the app' /tmp/mav-web2-61e06ee9/u9.html | head -1 | cut -c1-220
Higher tracking frequencies will reserve some performance headroom from the app
[exit 0]

$ LC_ALL=C grep -o -m1 'V2 provides improved tracking accuracy. Default uses the platform' /tmp/mav-web2-61e06ee9/u9.html | head -1 | cut -c1-220
V2 provides improved tracking accuracy. Default uses the platform
[exit 0]

$ LC_ALL=C grep -o -m1 'In most cases, dynamic foveation is a better option than using a static FFR level' /tmp/mav-web2-61e06ee9/u9.html | head -1 | cut -c1-220
In most cases, dynamic foveation is a better option than using a static FFR level
[exit 0]

$ LC_ALL=C grep -o -m1 'lastUpdated[^,]\{0,20\}' /tmp/mav-web2-61e06ee9/u9.html | head -1 | cut -c1-220
lastUpdated\":\"2026-04-14\"}
[exit 0]

$ LC_ALL=C grep -o -m1 'Changing Fixed Foveated Rendering settings at runtime is not supported on UE 5.7+[^*]*' /tmp/mav-web2-61e06ee9/u10.html | head -1 | cut -c1-260
Changing Fixed Foveated Rendering settings at runtime is not supported on UE 5.7+\n
[exit 0]

$ LC_ALL=C grep -o -m1 'Fixed foveation offsets not being applied on UE 5.8' /tmp/mav-web2-61e06ee9/u10.html | head -1 | cut -c1-220
Fixed foveation offsets not being applied on UE 5.8
[exit 0]

$ LC_ALL=C grep -o -m1 'Updated: [A-Z][a-z]* [0-9]*, 20[0-9]*' /tmp/mav-web2-61e06ee9/u10.html | head -1 | cut -c1-220
Updated: Sep 23, 2026
[exit 0]

$ LC_ALL=C grep -o -m1 'It is highly recommended to first test out your title without FMM enabled, and then enable it only if you observe high tracking loss due to fast hand motion.' /tmp/mav-web2-61e06ee9/u11.html | head -1 | cut -c1-220
It is highly recommended to first test out your title without FMM enabled, and then enable it only if you observe high tracking loss due to fast hand motion.
[exit 0]

$ LC_ALL=C grep -o -m1 'FMM optimizations may increase jitter, impacting high accuracy interactions like direct touch or typing.' /tmp/mav-web2-61e06ee9/u11.html | head -1 | cut -c1-220
FMM optimizations may increase jitter, impacting high accuracy interactions like direct touch or typing.
[exit 0]

$ LC_ALL=C grep -o -m1 'lastUpdated[^,]\{0,20\}' /tmp/mav-web2-61e06ee9/u11.html | head -1 | cut -c1-220
lastUpdated\":\"2026-08-11\"}
[exit 0]

$ LC_ALL=C grep -o -m1 'Provides improved tracking of fast movements common in fitness and rhythm apps (60Hz)' /tmp/mav-web2-61e06ee9/u12.html | head -1 | cut -c1-220
Provides improved tracking of fast movements common in fitness and rhythm apps (60Hz)
[exit 0]

$ LC_ALL=C grep -o -m1 'lastUpdated[^,]\{0,20\}' /tmp/mav-web2-61e06ee9/u12.html | head -1 | cut -c1-220
lastUpdated\":\"2026-08-31\"}
[exit 0]

$ LC_ALL=C grep -o -m1 'the system gesture is reserved, and should not trigger any other actions within the app' /tmp/mav-web2-61e06ee9/u14.html | head -1 | cut -c1-220
the system gesture is reserved, and should not trigger any other actions within the app
[exit 0]

$ LC_ALL=C grep -o -m1 'lastUpdated[^,]\{0,20\}' /tmp/mav-web2-61e06ee9/u14.html | head -1 | cut -c1-220
lastUpdated\":\"2024-07-31\"}
[exit 0]

$ LC_ALL=C grep -o -m1 'AppSW requires Mobile Multi-View and Vulkan[^\]*' /tmp/mav-web2-61e06ee9/u15.html | head -1 | cut -c1-260
AppSW requires Mobile Multi-View and Vulkan. For UE versions before 5.7, the Meta XR Plugin with the OVRPlugin OpenXR backend is also required. For UE 5.7 and later, SpaceWarp support is built into the engine.
[exit 0]

$ LC_ALL=C grep -o -m1 '(the older CVar is deprecated in 5.7 and produces errors)' /tmp/mav-web2-61e06ee9/u15.html | head -1 | cut -c1-220
(the older CVar is deprecated in 5.7 and produces errors)
[exit 0]

$ LC_ALL=C grep -o -m1 'requires modifying your app' /tmp/mav-web2-61e06ee9/u15.html | head -1 | cut -c1-220
requires modifying your app
[exit 0]

$ LC_ALL=C grep -o -m1 'lastUpdated[^,]\{0,20\}' /tmp/mav-web2-61e06ee9/u15.html | head -1 | cut -c1-220
lastUpdated\":\"2026-05-13\"}
[exit 0]

$ LC_ALL=C grep -o -m1 'Interactive applications must use a refresh rate of 72 Hz, 80 Hz, 90 Hz, 96 Hz, 100 Hz or 120 Hz.' /tmp/mav-web2-61e06ee9/u16.html | head -1 | cut -c1-220
Interactive applications must use a refresh rate of 72 Hz, 80 Hz, 90 Hz, 96 Hz, 100 Hz or 120 Hz.
[exit 0]

$ LC_ALL=C grep -o -m1 'lastUpdated[^,]\{0,20\}' /tmp/mav-web2-61e06ee9/u16.html | head -1 | cut -c1-220
lastUpdated\":\"2025-10-22\"}
[exit 0]

$ LC_ALL=C grep -o -m1 'you may need to set `r.Mobile.ShadingPath=0` in the project[^.]*' /tmp/mav-web2-61e06ee9/u17.html | head -1 | cut -c1-220
you may need to set `r.Mobile.ShadingPath=0` in the project's DefaultEngine
[exit 0]

$ LC_ALL=C grep -o -m1 'Updated: [A-Z][a-z]* [0-9]*, 20[0-9]*' /tmp/mav-web2-61e06ee9/u17.html | head -1 | cut -c1-220
Updated: Sep 22, 2026
[exit 0]

$ LC_ALL=C grep -o -m1 'APK files must be less than 1 GB in size[^"\<]*' /tmp/mav-web2-61e06ee9/u18.html | head -1 | cut -c1-220
APK files must be less than 1 GB in size, but can be accompanied by multiple expansion files up to 4 GB each.
[exit 0]

$ LC_ALL=C grep -o -m1 'lastUpdated[^,]\{0,20\}' /tmp/mav-web2-61e06ee9/u18.html | head -1 | cut -c1-220
lastUpdated\":\"2024-07-31\"}
[exit 0]
```

**Log 2: engine, plugin and repo evidence, plus the pin gate**

```
$ grep -o -m1 'lastUpdated[^,]\{0,20\}\|dateModified[^,]\{0,30\}' /tmp/mav-web2-61e06ee9/u5.html | head -1
[exit 0]

$ grep -n -E '^(r\.|vr\.|MinSDK|TargetSDK|ExtraApplication|XrApi|HandTracking|\+SupportedDevices)' apps/vr/Game/Config/DefaultEngine.ini apps/vr/Game/Config/Android/AndroidEngine.ini
apps/vr/Game/Config/DefaultEngine.ini:6:r.ForwardShading=True
apps/vr/Game/Config/DefaultEngine.ini:7:r.MSAACount=4
apps/vr/Game/Config/DefaultEngine.ini:8:r.DefaultFeature.AntiAliasing=3
apps/vr/Game/Config/DefaultEngine.ini:9:r.Mobile.UseHWsRGBEncoding=True
apps/vr/Game/Config/DefaultEngine.ini:46:MinSDKVersion=32
apps/vr/Game/Config/DefaultEngine.ini:47:TargetSDKVersion=32
apps/vr/Game/Config/DefaultEngine.ini:48:ExtraApplicationSettings=<meta-data android:name="com.oculus.supportedDevices" android:value="quest2|questpro|quest3|quest3s" />
apps/vr/Game/Config/DefaultEngine.ini:52:XrApi=NativeOpenXR
apps/vr/Game/Config/DefaultEngine.ini:53:HandTrackingSupport=ControllersAndHands
apps/vr/Game/Config/DefaultEngine.ini:54:HandTrackingFrequency=LOW
apps/vr/Game/Config/DefaultEngine.ini:55:HandTrackingVersion=Default
apps/vr/Game/Config/DefaultEngine.ini:56:+SupportedDevices=Quest3
apps/vr/Game/Config/Android/AndroidEngine.ini:6:vr.MobileMultiView=1
apps/vr/Game/Config/Android/AndroidEngine.ini:7:r.Mobile.MultiView=1
apps/vr/Game/Config/Android/AndroidEngine.ini:8:r.Mobile.ShadingPath=0
[exit 0]

$ grep -n 'DirectoriesToAlwaysStageAsNonUFS' apps/vr/Game/Config/DefaultGame.ini
9:+DirectoriesToAlwaysStageAsNonUFS=(Path="../Clips")
10:+DirectoriesToAlwaysStageAsNonUFS=(Path="../../data/vr")
[exit 0]

$ grep -n 'data/pinned' apps/vr/Game/Source/MageArenaVR/Kernel/KernelData.cpp apps/vr/Game/Source/MageArenaVR/Combat/AbsorbResolver.cpp
apps/vr/Game/Source/MageArenaVR/Kernel/KernelData.cpp:19:	return FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT("../data/pinned"), Relative));
apps/vr/Game/Source/MageArenaVR/Combat/AbsorbResolver.cpp:153:		TEXT("../data/pinned/docs/design/baseline-fourteen-nights/design/data/combat.json")));
[exit 0]

$ sed -n 279,287p apps/vr/Game/Source/MageArenaVR/Hands/MageArenaPawn.cpp
void AMageArenaPawn::SetVignette(double Intensity)
{
	if (!Camera)
	{
		return;
	}
	Camera->PostProcessSettings.bOverride_VignetteIntensity = true;
	Camera->PostProcessSettings.VignetteIntensity = static_cast<float>(Intensity);
}
[exit 0]

$ sed -n 23,30p apps/vr/Game/Source/MageArenaVR/Hands/HandFrame.h
struct FHandFrame
{
	double TimeSeconds = 0.0;
	EControllerHand Hand = EControllerHand::Left;
	float Confidence = 0.0f;
	float Pinch = 0.0f;
	TArray<FHandJointPose> Joints;
};
[exit 0]

$ grep -n '72' docs/CLIP-SCHEMA.md | head -3
33:| `hz` | `72`. Schema v1 is the Quest sample rate. |
37:| `seed` | Integer in `[0, 4294967295]`. Synthetic seed is FNV-1a of `action + NUL + variant`. Recorded clips use `0`. |
59:- Synthetic clips sample at exactly `i / 72` seconds.
[exit 0]

$ grep -n 'TargetSDKVersion' '/c/Program Files/Epic Games/UE_5.8/Engine/Config/BaseEngine.ini'
3305:TargetSDKVersion=36
[exit 0]

$ sed -n 1432,1442p '/c/Program Files/Epic Games/UE_5.8/Engine/Config/BaseDeviceProfiles.ini'
[Android_OpenXR DeviceProfile]
DeviceType=Android
BaseProfileName=Android_Mid
+CVars=r.Android.DisableVulkanSupport=0
+CVars=vr.MobileMultiView=1
+CVars=r.ForceStartInVR=1
+CVars=xr.VRS.DynamicFoveation=1
+CVars=r.Vulkan.VRSFormat=3
+CVars=r.Vulkan.AllowFDMOffset=1
+CVars=sg.ShadowQuality=2
+Cvars=XR.StandAloneStereoOnlyDevice=1
[exit 0]

$ grep -n -A1 'TEXT("r.AntiAliasingMethod"),' '/c/Program Files/Epic Games/UE_5.8/Engine/Source/Runtime/Engine/Private/SceneView.cpp'
237:	TEXT("r.AntiAliasingMethod"),
238-	4,
[exit 0]

$ grep -rIl 'r.DefaultFeature.AntiAliasing' '/c/Program Files/Epic Games/UE_5.8/Engine/Source/Runtime' '/c/Program Files/Epic Games/UE_5.8/Engine/Config' | wc -l
0
[exit 0]

$ sed -n 4719,4722p '/c/Program Files/Epic Games/UE_5.8/Engine/Source/Runtime/Core/Private/HAL/ConsoleManager.cpp'
static TAutoConsoleVariable<int32> CVarMobileDefaultAntiAliasing(
	TEXT("r.Mobile.AntiAliasing"),
	2,
	TEXT("Mobile default AntiAliasingMethod\n"
[exit 0]

$ sed -n 3926,3938p '/c/Program Files/Epic Games/UE_5.8/Engine/Source/Runtime/Core/Private/HAL/ConsoleManager.cpp'
static TAutoConsoleVariable<int32> CVarMobileHDR(
	TEXT("r.MobileHDR"),
	1,
	TEXT("0: Mobile renders in LDR gamma space. (suggested for unlit games targeting low-end phones)\n"
		 "1: Mobile renders in HDR linear space. (default)"),
	ECVF_RenderThreadSafe | ECVF_ReadOnly | ECVF_MobileShaderChange);

static TAutoConsoleVariable<int32> CVarMobileShadingPath(
	TEXT("r.Mobile.ShadingPath"),
	1,
	TEXT("0: Forward shading\n"
		 "1: Deferred shading (Mobile HDR is required for Deferred) (default)"),
	ECVF_RenderThreadSafe | ECVF_ReadOnly);
[exit 0]

$ sed -n 66,87p '/c/Program Files/Epic Games/UE_5.8/Engine/Source/Runtime/Engine/Private/SceneUtils.cpp'
ENGINE_API bool IsMobileColorsRGB()
{
	static auto* MobileUseHWsRGBEncodingCVAR = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.Mobile.UseHWsRGBEncoding"));
	const bool bMobileUseHWsRGBEncoding = (MobileUseHWsRGBEncodingCVAR && MobileUseHWsRGBEncodingCVAR->GetValueOnAnyThread() == 1);

	return !IsMobileHDR() && bMobileUseHWsRGBEncoding;
}

ENGINE_API EAntiAliasingMethod GetDefaultAntiAliasingMethod(const FStaticFeatureLevel InFeatureLevel)
{
	EAntiAliasingMethod AntiAliasingMethod = EAntiAliasingMethod::AAM_None;

	if (InFeatureLevel == ERHIFeatureLevel::ES3_1)
	{
		static auto* MobileAntiAliasingCvar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.Mobile.AntiAliasing"));
		AntiAliasingMethod = EAntiAliasingMethod(FMath::Clamp<int32>(MobileAntiAliasingCvar->GetValueOnAnyThread(), 0, AAM_MAX));

		// Disable antialiasing in GammaLDR mode to avoid jittering.
		if (!IsMobileHDR() && AntiAliasingMethod != EAntiAliasingMethod::AAM_MSAA)
		{
			AntiAliasingMethod = EAntiAliasingMethod::AAM_None;
		}
[exit 0]

$ sed -n 273,289p '/c/Program Files/Epic Games/UE_5.8/Engine/Source/Runtime/Renderer/Private/PostProcess/PostProcessing.cpp'
bool IsPostProcessingEnabled(const FViewInfo& View)
{
	if (View.GetFeatureLevel() >= ERHIFeatureLevel::SM5)
	{
		return
			 View.Family->EngineShowFlags.PostProcessing &&
			!View.Family->EngineShowFlags.VisualizeDistanceFieldAO &&
			!View.Family->EngineShowFlags.VisualizeShadingModels &&
			!View.Family->EngineShowFlags.VisualizeVolumetricCloudConservativeDensity &&
			!View.Family->EngineShowFlags.VisualizeVolumetricCloudEmptySpaceSkipping &&
			!View.Family->EngineShowFlags.ShaderComplexity;
	}
	else
	{
		return View.Family->EngineShowFlags.PostProcessing && !View.Family->EngineShowFlags.ShaderComplexity && IsMobileHDR();
	}
}
[exit 0]

$ sed -n 9,20p '/c/Program Files/Epic Games/UE_5.8/Engine/Plugins/Runtime/OpenXR/Source/OpenXRHMD/Private/FBFoveationImageGenerator.cpp'
static TAutoConsoleVariable<int32> CVarOpenXRFBFoveationLevel(
	TEXT("xr.OpenXRFBFoveationLevel"),
	0,
	TEXT("Possible foveation levels as specified by the XrFoveationLevelFB enumeration.\n")
	TEXT("0 = None, 1 = Low , 2 = Medium, 3 = High.\n"),
	ECVF_RenderThreadSafe | ECVF_ReadOnly);

static TAutoConsoleVariable<bool> CVarOpenXRFBFoveationDynamic(
	TEXT("xr.OpenXRFBFoveationDynamic"),
	false,
	TEXT("Whether dynamically changing foveation based on performance headroom is enabled.\n"),
	ECVF_RenderThreadSafe | ECVF_ReadOnly);
[exit 0]

$ grep -n 'bool bIsFBFoveationEnabled' '/c/Program Files/Epic Games/UE_5.8/Engine/Plugins/Runtime/OpenXR/Source/OpenXRHMD/Public/OpenXRHMDSettings.h'
22:	bool bIsFBFoveationEnabled = false;
[exit 0]

$ sed -n 44,48p '/c/Program Files/Epic Games/UE_5.8/Engine/Plugins/Runtime/OpenXR/Source/OpenXRHMD/Private/OpenXRHMDModule.cpp'
static TAutoConsoleVariable<bool> CVarPreferFBSpaceWarp(
	TEXT("xr.PreferFBSpaceWarp"),
	false,
	TEXT("If true, OpenXR will use XR_FB_space_warp rather than XR_EXT_frame_synthesis to provide frame synthesis when both extensions are available."),
	ECVF_ReadOnly);
[exit 0]

$ sed -n 568,572p '/c/Program Files/Epic Games/UE_5.8/Engine/Plugins/Runtime/OpenXR/Source/OpenXRHMD/Private/OpenXRHMDModule.cpp'
	// These extensions both perform frame synthesis, and require providing motion vector and motion vector depth swapchains at a requested size
	// XR_EXT_frame_synthesis is the more modern and platform-agnostic option and is preferred when available
	// However, it is not currently working properly on Oculus runtimes, so XR_FB_space_warp is preferred on Quest devices
	const bool bSpaceWarpSupported = AvailableExtensions.Contains(XR_FB_SPACE_WARP_EXTENSION_NAME);
	const bool bFrameSynthesisSupported = AvailableExtensions.Contains(XR_EXT_FRAME_SYNTHESIS_EXTENSION_NAME);
[exit 0]

$ sed -n 175,180p '/c/Program Files/Epic Games/UE_5.8/Engine/Plugins/Runtime/OpenXR/Source/OpenXRHMD/Private/OpenXRHMD.cpp'
static TAutoConsoleVariable<bool> CVarOpenXRFrameSynthesis(
	TEXT("xr.OpenXRFrameSynthesis"),
	0,
	TEXT("If true and supported via XR_EXT_frame_synthesis or XR_FB_space_warp, write to and submit motion vector and motion vector depth swapchains for frame synthesis.\n")
	TEXT("Currently only supported when using the Vulkan mobile renderer, using mobile multi-view, and r.Velocity.DirectlyRenderOpenXRMotionVectors=True.\n")
	TEXT("Because normal velocity rendering is disabled when r.Velocity.DirectlyRenderOpenXRMotionVectors=True, temporal anti-aliasing and motion blur will be automatically disabled."),
[exit 0]

$ sed -n 595,599p '/c/Program Files/Epic Games/UE_5.8/Engine/Source/Developer/DeveloperToolSettings/Classes/Settings/ProjectPackagingSettings.h'
	 * This is used to stage additional files that you manually load without using the UFS (Unreal File System) file IO API, eg, third-party libraries that perform their own internal file IO
	 * Note: These paths are relative to your project Content directory
	 */
	UPROPERTY(config, EditAnywhere, Category=Packaging, AdvancedDisplay, meta=(DisplayName="Additional Non-Asset Directories To Copy", RelativeToGameContentDir))
	TArray<FDirectoryPath> DirectoriesToAlwaysStageAsNonUFS;
[exit 0]

$ sed -n 837,844p '/c/Program Files/Epic Games/UE_5.8/Engine/Source/Programs/AutomationTool/Scripts/CopyBuildToStagingDirectory.Automation.cs'
						DirectoryReference InputDir = DirectoryReference.Combine(ProjectContentRoot, RelativePath);
						if (Directory.Exists(InputDir.FullName)==false)
						{
							Logger.LogWarning("Unable to find directory \"{Arg0}\" for staging, retrieved from \"/Script/UnrealEd.ProjectPackagingSettings\" \"{ConfigKeyName}\"", InputDir.FullName, ConfigKeyName);
							continue;
						}
						StagedDirectoryReference OutputDir = StagedDirectoryReference.Combine(StageContentRoot, RelativePath);
						if (bUFS)
[exit 0]

$ grep -n 'XR_HAND_TRACKING_AIM_SYSTEM_GESTURE_BIT_FB =' '/c/Program Files/Epic Games/UE_5.8/Engine/Source/ThirdParty/OpenXR/include/openxr/openxr.h'
3917:static const XrHandTrackingAimFlagsFB XR_HAND_TRACKING_AIM_SYSTEM_GESTURE_BIT_FB = 0x00000040;
[exit 0]

$ sed -n 181,183p /c/Users/kazda/kiro/mage-arena-vr/apps/vr/Game/Plugins/MetaXR/Source/OculusXRHMD/Public/OculusXRHMDRuntimeSettings.h
	/** Note that a higher tracking frequency will reserve some performance headroom from the application's budget. */
	UPROPERTY(config, EditAnywhere, Category = Mobile)
	EOculusXRHandTrackingFrequency HandTrackingFrequency;
[exit 0]

$ grep -n -E 'SuggestedCpuPerfLevel\(|SuggestedGpuPerfLevel\(|FoveatedRenderingLevel\(|bDynamicFoveatedRendering\(|HandTrackingFrequency\(' /c/Users/kazda/kiro/mage-arena-vr/apps/vr/Game/Plugins/MetaXR/Source/OculusXRHMD/Private/OculusXRHMD_Settings.cpp
22:		, SuggestedCpuPerfLevel(EOculusXRProcessorPerformanceLevel::SustainedLow)
23:		, SuggestedGpuPerfLevel(EOculusXRProcessorPerformanceLevel::SustainedHigh)
25:		, FoveatedRenderingLevel(EOculusXRFoveatedRenderingLevel::Off)
26:		, bDynamicFoveatedRendering(true)
33:		, HandTrackingFrequency(EOculusXRHandTrackingFrequency::LOW)
[exit 0]

$ sed -n 73,91p /c/Users/kazda/kiro/mage-arena-vr/apps/vr/Game/Plugins/MetaXR/Source/OculusXRHMD/Private/OpenXR/OculusXRCoreExtensionPlugin.cpp
		if (UOpenXRHMDSettings* OpenXRHMDSettings = GetMutableDefault<UOpenXRHMDSettings>())
		{
			OpenXRHMDSettings->bIsFBFoveationEnabled = true;
		}

		if (IConsoleVariable* VariableRateShadingCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.VRS.Enable")))
		{
			VariableRateShadingCVar->Set(1);
		}

		if (const UOculusXRHMDRuntimeSettings* OculusXRHMDSettings = GetDefault<UOculusXRHMDRuntimeSettings>())
		{
			if (IConsoleVariable* FoveationLevelCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("xr.OpenXRFBFoveationLevel")))
			{
				FoveationLevelCVar->Set(static_cast<int>(OculusXRHMDSettings->FoveatedRenderingLevel));
			}
			if (IConsoleVariable* FoveationDynamicCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("xr.OpenXRFBFoveationDynamic")))
			{
				FoveationDynamicCVar->Set(OculusXRHMDSettings->bDynamicFoveatedRendering);
[exit 0]

$ sed -n 274,277p /c/Users/kazda/kiro/mage-arena-vr/apps/vr/Game/Plugins/MetaXR/Source/OculusXRHMD/Private/OculusXRFunctionLibraryOpenXR.cpp
	void FOculusXRFunctionLibraryOpenXR::SetFoveatedRenderingLevel(EOculusXRFoveatedRenderingLevel level, bool isDynamic)
	{
#if UE_VERSION_OLDER_THAN(5, 7, 0) || defined(WITH_OCULUS_BRANCH)
		if (IConsoleVariable* FoveationLevelCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("xr.OpenXRFBFoveationLevel")))
[exit 0]

$ sed -n 14p /c/Users/kazda/kiro/mage-arena-vr/apps/vr/Game/Plugins/MetaXR/Source/OculusXRHMD/Private/OpenXR/OculusXRSpaceWarp.cpp
#if (defined(WITH_OCULUS_BRANCH) || defined(WITH_OPENXR_BRANCH)) && UE_VERSION_OLDER_THAN(5, 7, 0)
[exit 0]

$ sed -n 353,371p /c/Users/kazda/kiro/mage-arena-vr/apps/vr/Game/Plugins/MetaXR/Source/OculusXRHMD/OculusMobile_APL.xml
					<loopElements tag="meta-data">
						<setStringFromAttribute result="nameString" tag="$" name="android:name"/>
						<setBoolIsEqual result="bIsSupportedDevices" arg1="$S(nameString)" arg2="com.oculus.supportedDevices"/>
						<if condition="bIsSupportedDevices">
							<true>
								<setStringFromAttribute result="existingDevicesString" tag="$" name="android:value"/>
								<log text="Found existing Meta Quest supported devices tag: $S(existingDevicesString)"/>
								<setStringAdd result="devicesString" arg1="$S(existingDevicesString)" arg2="|$S(devicesString)"/>
								<setIntLength result="devicesStringLength" source="$S(devicesString)"/>
								<removeElement tag="$"/>
							</true>
						</if>
					</loopElements>
					<setElement result="supportedDevices" value="meta-data"/>
					<addAttribute tag="$supportedDevices" name="android:name" value="com.oculus.supportedDevices"/>
					<setIntSubtract result="devicesStringLength" arg1="$I(devicesStringLength)" arg2="1"/>
					<setStringSubstring result="devicesString" source="$S(devicesString)" start="0" length="$I(devicesStringLength)"/>
					<log text="Adding Meta Quest supported devices tag: $S(devicesString)"/>
					<addAttribute tag="$supportedDevices" name="android:value" value="$S(devicesString)"/>
[exit 0]

$ sed -n 376,382p /c/Users/kazda/kiro/mage-arena-vr/apps/vr/Game/Plugins/MetaXR/Source/OculusXRHMD/OculusMobile_APL.xml
			<!-- Add Hand Tracking Frequency -->
			<if condition="bEnableHandTracking">
				<true>
					<setElement result="handTrackingFrequency" value="meta-data"/>
					<addAttribute tag="$handTrackingFrequency" name="android:name" value="com.oculus.handtracking.frequency"/>
					<addAttribute tag="$handTrackingFrequency" name="android:value" value="$S(HandTrackingFrequency)"/>
					<addElement tag="application" name="handTrackingFrequency"/>
[exit 0]

$ sed -n 414,416p /c/Users/kazda/kiro/mage-arena-vr/apps/vr/Game/Plugins/MetaXR/Source/OculusXRInput/Private/OculusXRInputHandTrackingExtensionPlugin.cpp
				if (AimState.status & XR_HAND_TRACKING_AIM_SYSTEM_GESTURE_BIT_FB)
				{
					InternalHandsState.HandState[HandIndex].Status |= static_cast<int32>(EHandStatus::SystemGestureInProgress);
[exit 0]

$ grep -h BuildId /c/Users/kazda/kiro/mage-arena-vr/apps/vr/Game/Plugins/MetaXR/Binaries/Win64/UnrealEditor.modules /c/Users/kazda/kiro/mage-arena-vr/apps/vr/Game/Plugins/MetaXRInteraction/Binaries/Win64/UnrealEditor.modules '/c/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor.version'
	"BuildId": "55116800",
	"BuildId": "55116800",
	"BuildId": "55116800"
[exit 0]

$ LC_ALL=C grep -o -m1 '"updated_at":"[^"]*"' /tmp/mav-web2-61e06ee9/u5.html | head -1
"updated_at":"2026-06-23T14:15:16.394Z"
[exit 0]

$ node apps/vr/tools/check-pin.mjs
pin OK: 9 files @ 68a4d68
[exit 0]

$ LC_ALL=C grep -o -m1 'place your hand up with an open palm towards your headset[^\]*' /tmp/mav-web2-61e06ee9/u14.html | head -1 | cut -c1-200
place your hand up with an open palm towards your headset, and pinch to perform a system gesture. No other gesture events should be processed.
[exit 0]

$ LC_ALL=C grep -o -m1 'Interactive applications may use a rendering rate[^\]*' /tmp/mav-web2-61e06ee9/u16.html | head -1 | cut -c1-260
Interactive applications may use a rendering rate (fps) of half the refresh rate (such as 30 fps for 60 Hz, 36 fps for 72 Hz), for portions of their experience utilizing Application SpaceWarp.
[exit 0]

$ LC_ALL=C grep -o -m1 'Apps must maintain a rendering rate of at least 60 fps' /tmp/mav-web2-61e06ee9/u16.html | head -1
Apps must maintain a rendering rate of at least 60 fps
[exit 0]

$ sed -n 46p '/c/Program Files/Epic Games/UE_5.8/Engine/Plugins/Runtime/OpenXR/Source/OpenXRHMD/Private/FBFoveationImageGenerator.cpp'
		int32 SanitisedFoveationLevel = FMath::Clamp(CVarOpenXRFBFoveationLevel->GetInt(), XrFoveationLevelFB::XR_FOVEATION_LEVEL_NONE_FB, XrFoveationLevelFB::XR_FOVEATION_LEVEL_HIGH_FB);
[exit 0]

$ sed -n 90,98p /c/Users/kazda/kiro/mage-arena-vr/apps/vr/Game/Plugins/MetaXR/Source/OculusXRHMD/Public/OculusXRHMDTypes.h
enum class EOculusXRFoveatedRenderingLevel : uint8
{
	Off = 0,
	Low = 1,
	Medium = 2,
	High = 3,
	// High foveation setting with more detail toward the bottom of the view and more foveation near the top
	HighTop = 4
};
[exit 0]

$ curl -sL -A Mozilla/5.0 --max-time 60 -o /tmp/mav-web2-61e06ee9/u19.html -w '%{http_code} %{size_download} u19\n' 'https://developers.meta.com/horizon/documentation/unreal/unreal-openxr/'
200 651869 u19
[exit 0]

$ LC_ALL=C grep -o -m1 'Some features, such as Late Latching, are only available when using the Oculus-VR fork[^\]*' /tmp/mav-web2-61e06ee9/u19.html | head -1 | cut -c1-200
[exit 0]

$ LC_ALL=C grep -o -m1 'OpenXR Hand Tracking plugin must be enabled for hand tracking' /tmp/mav-web2-61e06ee9/u19.html | head -1
OpenXR Hand Tracking plugin must be enabled for hand tracking
[exit 0]

$ LC_ALL=C grep -o -m1 'lastUpdated[^,]\{0,20\}' /tmp/mav-web2-61e06ee9/u19.html | head -1
lastUpdated\":\"2026-04-17\"}
[exit 0]
```

## 7. Feed 2026-10-08

What changed upstream since the 2026-10-07 read, for the stack in section 1. Sections 1 to 6 are unchanged, and they
are the history. Nothing below is applied. No Unreal process ran for this feed: no build, editor, commandlet, cook or
automation run. The engine and the Android toolchain were read from their files only, and nothing was installed.

- **How the quotes were checked.** As in section 6, every page was downloaded with `curl` into a new, empty directory
  outside the repo (`/tmp/mav-feed1008-88ab`). Each quote below was then found in the raw file with `grep` (7.3).
  No WebFetch call was made, so nothing here rests on WebFetch alone.
- **Unconfirmed:** two WebSearch result summaries that no download confirmed (7.3), the date of the Devpost update in
  row 12 (the page shows only "3 days ago"), and the 5.8.4 date in F12 (an inference from the cadence).

### 7.1 What was read

| # | Piece | On 10-07 | Now (10-08) | Page date | URL | Verdict |
|---|---|---|---|---|---|---|
| 1 | Meta XR Unreal Engine 5 Integration | 207.0 ("Updated: Sep 23, 2026") | 207.0. The page lists 201.0, 203.0, 205.0 and 207.0, and nothing above 207.0 | Updated: Sep 23, 2026; 207.0 `release_date` 2026-09-23 | https://developers.meta.com/vr/downloads/package/unreal-engine-5-integration/ | no change |
| 2 | Meta XR Interaction SDK (Unreal) | 207.0 ("Updated: Sep 22, 2026") | 207.0, nothing above it | Updated: Sep 22, 2026; 207.0 `release_date` 2026-09-22 | https://developers.meta.com/horizon/downloads/package/meta-xr-interaction-sdk-unreal/ | no change |
| 3 | UE 5.8 hotfix | 5.8.3 (2026-09-22); "no 5.8.4" came from a tag list only, so it was **unconfirmed** | 5.8.3 is still the newest released hotfix. 5.8.4 is **announced but not released**: an Epic staff post says a fix "will be addressed in 5.8.4" | 5.8.3 thread 2026-09-22 (1 post, no reply); staff post 2026-10-05 | https://forums.unrealengine.com/t/5-8-3-hotfix-released/2833315, https://forums.unrealengine.com/t/2731772, https://forums.unrealengine.com/tag/hotfix | F12 |
| 4 | UE 5.8 release notes | `updated_at` 2026-06-23 | the same; the page has no "5.8.4" | `updated_at` 2026-06-23 | https://dev.epicgames.com/documentation/unreal-engine/unreal-engine-5-8-release-notes | no change |
| 5 | Store: release-build manifest (F1 target SDK 34) | last updated 2026-09-30 | 2026-09-30; "must set their targetSdkVersion to 34" and "32-34 for immersive, 32-36 for 2D" are still there | 2026-09-30 | https://developers.meta.com/horizon/resources/publish-mobile-manifest/ | no change |
| 6 | Store: Android 14 post (F1) | Nov 24, 2025, updated Feb 6, 2026 | the upload-enforcement sentence is still there | Nov 24, 2025 | https://developers.meta.com/horizon/blog/meta-quest-apps-android-14-march-1/ | no change |
| 7 | Store: compatibility mode, `supportedDevices` (F2) | last updated 2026-09-08 | 2026-09-08; the `supportedDevices` sentence is still there | 2026-09-08 | https://developers.meta.com/horizon/documentation/native/android/os-compatibility-mode/ | no change |
| 8 | Store: APK size, VRC.Quest.Packaging.5 | 2024-07-31, under 1 GB | 2024-07-31; the same sentence | 2024-07-31 | https://developers.meta.com/horizon/resources/vrc-quest-packaging-5/ | no change |
| 9 | Store: VRC guideline list (first read) | not read | "APK file size must be less than 1 GB. OBB files must be less than 4 GB." | last updated 2026-08-19 | https://developers.meta.com/vr/resources/publish-quest-req/ | no change |
| 10 | Competition rules page | "November 18, 2026 at 12:00:00 PM PT", 183,553 bytes (`docs/research/FEASIBILITY-2026-10.md:115`) | the same sentence and the same size, 183,553 bytes | no page date | https://start-developer-competition-26.devpost.com/rules | no change |
| 11 | Competition overview page | 166,848 bytes (same source) | "Deadline: Nov 18, 2026 @ 12:00pm PST", 166,848 bytes | no page date | https://start-developer-competition-26.devpost.com/ | no change |
| 12 | Devpost update 46826 (first read) | not read | Advice, not a rule: "define your track and division" by "Friday, October 9", and "New Experience means starting from a blank slate on or after September 24, 2026" | "3 days ago" on 10-08, so about 2026-10-05 (**unconfirmed**) | https://start-developer-competition-26.devpost.com/updates/46826-give-yourself-a-schedule-save-the-date-live-build-session-october-13 | no change to the deadline; the wording goes to the owner (Q9) |
| 13 | Meta blog, competition launch (first read) | `curl` got HTTP 400 on 10-07 (FEASIBILITY:115) | HTTP 200; "November 18, 2026" | no machine-readable date; it names September 24, 2026 as the start | https://developers.meta.com/blog/meta-connect-2026-vr-start-developer-competition/ | no change |
| 14 | Epic: Android requirements for UE 5.8 (first read) | not read; the repo records the engine's `Android_SDK.json` (`docs/XR-TOOLCHAIN.md:80`) | NDK r27c, build-tools 35.0.1, "Java runtime: OpenJDK 21.0.3 2024-04-16", minimum compile SDK 34, recommended target SDK 35 | `updated_at` 2026-06-12 | https://dev.epicgames.com/documentation/en-us/unreal-engine/android-development-requirements-for-unreal-engine | F11 |
| 15 | AGP 8.13 release notes (first read) | not read | Gradle minimum 8.13; JDK minimum 17 | Last updated 2026-10-01 UTC | https://developer.android.com/build/releases/past-releases/agp-8-13-0-release-notes | F11 |
| 16 | Gradle Java compatibility (first read) | not read | Java 22 runs Gradle "8.8 and after" | Gradle User Manual 9.8.1; no page date | https://docs.gradle.org/current/userguide/compatibility.html | F11 |
| 17 | Engine `Android_SDK.json` (local, read-only) | NDK r27c to r29, build-tools 36.0.0, android-36 | the same | file on disk | `Engine/Config/Android/Android_SDK.json` | no change |
| 18 | This machine's toolchain (local, read-only) | NDK r28c; JDK Zulu 22.0.1; build-tools 28.0.3, 34.0.0, 35.0.0 and 36.1.0; platforms 31, 34 and 36 (`docs/XR-TOOLCHAIN.md:71-78`) | the same | `NDKROOT`, `JAVA_HOME`, `ANDROID_HOME` | local | F11 (JDK only) |
| 19 | UE-160597 (BUDGET-DG4 9, lever A risk) | HTTP 403 on 10-08 (MOBILE-INSTANCING 1.6) | HTTP 403 again | none readable | https://issues.unrealengine.com/issue/UE-160597 | **unconfirmed**, no change |

Notes on the rows:

- **Rows 1 and 2.** There is no release above 207.0, so there are no release-note items to list for hand tracking,
  the system gesture, OpenXR, Vulkan or multiview, Niagara, or UE 5.8.
- **Row 3.** This feed looks for 5.8.4's fixed issues that touch Android or Vulkan mobile, Niagara on mobile or GPU
  simulation, occlusion queries (UE-160597), the clang or NDK toolchain, or commandlets. 5.8.4 has no release notes
  yet. The two fixes announced for it touch none of these (F12).
- **Row 14.** Epic recommends target SDK 35, but Meta caps immersive apps at 34 (row 5). F1's 34 stands.
- **Row 14, build-tools.** Epic documents build-tools 35.0.1, and the engine's JSON asks for 36.0.0. Neither is
  installed here. This needs no change: UBT uses the highest installed build-tools that has `aapt`, unless
  `BuildToolsOverride` is set (`AndroidToolChain.cs:1471`, `:1483`, `:1485`), so it would use 36.1.0.
- **Row 14, NDK.** Epic's page names r27c only. The engine's own JSON accepts r27c to r29, and Turnkey accepted r28c
  (`docs/XR-TOOLCHAIN.md:66`). No change.

### 7.2 Findings

Two findings, F11 and F12. Neither is applied.

#### F11. JDK 22 is not Epic's documented JDK 21, but it is inside the range the 5.8 Gradle chain accepts

- **Upstream statements:**
  - Epic's "Android Development Requirements for Unreal Engine" (UE 5.8 documentation, `updated_at` 2026-06-12) shows
    "Current UE Version: 5.8", "NDK Version: r27c", "Build-tools: 35.0.1" and "Java runtime: OpenJDK 21.0.3 2024-04-16".
    https://dev.epicgames.com/documentation/en-us/unreal-engine/android-development-requirements-for-unreal-engine
  - The AGP 8.13 release notes (Last updated 2026-10-01 UTC) open with "The maximum API level that Android Gradle Plugin
    8.13 supports is API level 36.1". The compatibility table that follows lists Gradle with minimum and default 8.13,
    and JDK with minimum and default 17.
    https://developer.android.com/build/releases/past-releases/agp-8-13-0-release-notes
  - Gradle's compatibility matrix (Gradle User Manual 9.8.1, no page date), Java 22 row: toolchains "8.7", running
    Gradle "8.8 and after".
    https://docs.gradle.org/current/userguide/compatibility.html
- **Engine source (read-only):**
  - `Engine/Source/Programs/UnrealBuildTool/Platform/Android/UEDeployAndroid.cs:29` pins
    `com.android.tools.build:gradle:8.13.0`.
  - `UEDeployAndroid.cs:6510-6511` writes `sourceCompatibility` and `targetCompatibility` as `JavaVersion.VERSION_17`.
  - The engine's Gradle wrapper cannot be read, because `Engine/Build/Android` does not exist (the engine's Android
    component is not installed, as F1 says). The AGP table makes Gradle 8.13 the minimum, and that is above Gradle's
    8.8 floor for Java 22.
- **Local:** `JAVA_HOME` is Zulu 22.0.1 (`java -version` prints `openjdk version "22.0.1" 2024-04-16`).
- **Repo:**
  - `docs/PROJECT-PLAN.md:205` says "Whether UE 5.8 accepts NDK r28 and JDK 22 is unverified".
  - `docs/XR-TOOLCHAIN.md:75` records the JDK as Zulu 22.0.1.
  - `apps/vr/tools/install-xr.ps1:26` accepts any JDK of 17 or newer, and `:104` lists `zulu-22` as the first
    candidate.
- **Recommended change:**
  - Keep JDK 22 for the first package. On the Gradle side, the upstream pages answer the open question at
    `PROJECT-PLAN.md:205`: AGP 8.13 needs 17 or newer, and Gradle 8.13 or newer runs on 22.
  - Record at `PROJECT-PLAN.md:205` and `XR-TOOLCHAIN.md:75` that 22 is outside Epic's documented 21.0.3. Name JDK 21
    as the fallback if the first Gradle step fails on a Java or class-file version.
  - Nothing is installed now.
- **Unconfirmed:**
  - The Gradle version the engine's Android component ships.
  - Whether any other plugin in the generated Gradle build has its own JDK cap.

  Both are proven only by the first package run.
- **Cost:** S (two doc lines). Optionally the P2 preflight warns, but does not fail, when the JDK is not 21.
- **When:** before 10-31, before the first APK. F1 and the plan's P11 want a dry-run upload in October.
- **Verifiable headless:** yes, once the Android component is installed: the packaging log's Gradle step. Not in
  this worktree.
- **Registry:** EXTENDS `ship-pipeline-gating`: check the toolchain against the engine's documented versions before
  the first package.

#### F12. A 5.8.4 hotfix is announced, so F9's freeze-or-take decision is due before about 2026-10-20

- **Upstream statements:**
  - Forum bug thread "ShallowWaterRiver simulation does not initialize in Unreal Engine 5.7.4". On 2026-09-18 a user
    (no staff flag) wrote "The fix has been submitted and will be in 5.8.4." On 2026-10-05 Epic's Bug-Reporter
    (staff) wrote "UE-397316 is ‘Closed’ as ‘Fixed’. The issue will be addressed in 5.8.4."
    https://forums.unrealengine.com/t/2731772
  - Forum bug thread "UE5.8 Unable to build Game on macOS 27" (2026-09-25): "it should be in the next 5.8 hotfix".
    https://forums.unrealengine.com/t/2746676
  - The forum's hotfix tag lists 5.8.1 (2026-07-28), 5.8.2 (2026-08-25) and 5.8.3 (2026-09-22), each 28 days apart.
    It has no 5.8.4 topic, and a forum search for "5.8.4 hotfix released" returns 0 posts.
    https://forums.unrealengine.com/tag/hotfix
- **What 5.8.4 is known to carry:**
  - A Niagara scene-capture-2D depth-sampling shader fix (the ShallowWaterRiver thread).
  - A macOS and Xcode 27 build step fix.

  Neither touches the list in the note on row 3 of 7.1. The six `NS_Budget_` proxies in
  `docs/research/NIAGARA-PROXY-2026-10.md` 1.4 name no scene-capture data interface. Its full notes must be read
  when it is posted.
- **Local:** `Build.version` is 5.8.3, Changelist 58210709, CompatibleChangelist 55116800.
- **Repo:**
  - `CLAUDE.md:44` says "The engine has been UE 5.8.3 (CL 58210709) since 2026-10-03."
  - F9 left the launcher freeze as the owner's call.
- **Recommended change:** the owner chooses before 5.8.4 lands.
  - (a) Freeze `UE_5.8` at 5.8.3 until 11-18, as F9 asks.
  - (b) Take 5.8.4 on purpose, on one day: re-check its CompatibleChangelist against the plugins' BuildId 55116800,
    the editor build, the suite and the boot marker, then record it at `CLAUDE.md:44` and in `XR-TOOLCHAIN.md`'s
    re-check section.

  Recommendation: (a). Its known fixes touch nothing here, and the Niagara build (NIAGARA-PROXY 5) and the 10-31 gate
  fall in the same weeks. This is in result.json's questions, not decided here.
- **Unconfirmed:**
  - The date. 2026-10-20 is 2026-09-22 plus the 28-day cadence, not an announcement.
  - Whether the launcher updates the engine on its own (F9).
- **Cost:** S.
- **When:** before 10-31; the decision before about 10-20.
- **Verifiable headless:** the version check, yes (`Build.version`).
- **Registry:** EXTENDS `engine-pitfall-corpus`, the same incident as F9: the engine moving under a pinned plugin.

### 7.3 Commands run for this section, with exit codes

**Tool calls that are not shell commands, so they have no exit code:**

- WebSearch, 2 queries:
  - "Meta VR Start Developer Competition 2026 Gaming track submission deadline"
  - "Meta Horizon Store new app requirements October 2026 targetSdkVersion Quest"
- WebFetch: none.
- Two claims in the search summaries were **not** confirmed by a download, and nothing above rests on them:
  - that a later Meta "Update" post widened the API 34 rule to new uploads of existing apps. It does not change F1,
    which already targets 34.
  - that the VRC list was "updated Aug 19, 2026". That date was later grepped from the raw page (row 9).

**Exploration without an exit-code log.** Reading the downloaded pages to find the quotes was also done with ad hoc
`grep`, `sed` and `node` text extraction. All of those exited 0, except one `ls` of the absent `AGENTS.md`
(exit 2). A `find` and an `ls` showed that `Engine/Build/Android` does not exist; log 11 repeats that `ls`. Every quote
and figure above is re-found in the logged commands below.

**Shell, run from the worktree root.** A small runner printed each command, ran it, and printed `[exit N]`. The logs
are verbatim, except that trailing whitespace was trimmed. `grep -c` exits 1 when it counts 0 matches; logs 3 and 4
use that as evidence that a page lacks "5.8.4". In log 13 the Epic JDK and NDK `grep`s matched nothing: the raw page
has `&nbsp;` where the text shows a space, and `| head -1` hides grep's exit code. Log 14 re-finds both with `.` in
that place.

```
# log 1
$ curl -sL -A Mozilla/5.0 --max-time 60 -o /tmp/mav-feed1008-88ab/u10.html -w '%{http_code} %{size_download} u10\n' 'https://developers.meta.com/vr/downloads/package/unreal-engine-5-integration/'
200 636386 u10
[exit 0]

$ curl -sL -A Mozilla/5.0 --max-time 60 -o /tmp/mav-feed1008-88ab/u17.html -w '%{http_code} %{size_download} u17\n' 'https://developers.meta.com/horizon/downloads/package/meta-xr-interaction-sdk-unreal/'
200 523583 u17
[exit 0]

$ curl -sL -A Mozilla/5.0 --max-time 60 -o /tmp/mav-feed1008-88ab/u4.html -w '%{http_code} %{size_download} u4\n' 'https://forums.unrealengine.com/t/5-8-3-hotfix-released/2833315'
200 26901 u4
[exit 0]

$ curl -sL -A Mozilla/5.0 --max-time 60 -o /tmp/mav-feed1008-88ab/u2.html -w '%{http_code} %{size_download} u2\n' 'https://developers.meta.com/horizon/resources/publish-mobile-manifest/'
200 673188 u2
[exit 0]

$ curl -sL -A Mozilla/5.0 --max-time 60 -o /tmp/mav-feed1008-88ab/u18.html -w '%{http_code} %{size_download} u18\n' 'https://developers.meta.com/horizon/resources/vrc-quest-packaging-5/'
200 664577 u18
[exit 0]

$ curl -sL -A Mozilla/5.0 --max-time 60 -o /tmp/mav-feed1008-88ab/u8.html -w '%{http_code} %{size_download} u8\n' 'https://developers.meta.com/horizon/documentation/native/android/os-compatibility-mode/'
200 655046 u8
[exit 0]

$ curl -sL -A Mozilla/5.0 --max-time 60 -o /tmp/mav-feed1008-88ab/u1.html -w '%{http_code} %{size_download} u1\n' 'https://developers.meta.com/horizon/blog/meta-quest-apps-android-14-march-1/'
200 617721 u1
[exit 0]

# log 2
$ curl -sL -A Mozilla/5.0 --max-time 60 -o /tmp/mav-feed1008-88ab/f-search-584.json -w '%{http_code} %{size_download} f-search-584\n' 'https://forums.unrealengine.com/search.json?q=5.8.4%20hotfix%20released'
200 354 f-search-584
[exit 0]

$ curl -sL -A Mozilla/5.0 --max-time 60 -o /tmp/mav-feed1008-88ab/f-tag-hotfix.json -w '%{http_code} %{size_download} f-tag-hotfix\n' 'https://forums.unrealengine.com/tag/hotfix.json'
200 30250 f-tag-hotfix
[exit 0]

$ curl -sL -A Mozilla/5.0 --max-time 60 -o /tmp/mav-feed1008-88ab/f-announce.json -w '%{http_code} %{size_download} f-announce\n' 'https://forums.unrealengine.com/c/general/announcements/9.json'
200 76496 f-announce
[exit 0]

# log 3
$ curl -sL -A Mozilla/5.0 --max-time 90 -o /tmp/mav-feed1008-88ab/u5.html -w '%{http_code} %{size_download} u5\n' 'https://dev.epicgames.com/documentation/unreal-engine/unreal-engine-5-8-release-notes'
200 5382178 u5
[exit 0]

$ curl -sL -A Mozilla/5.0 --max-time 60 -o /tmp/mav-feed1008-88ab/f-search-58-3.json -w '%{http_code} %{size_download} f-search-58-3\n' 'https://forums.unrealengine.com/search.json?q=%225.8.3%20Hotfix%22'
200 5073 f-search-58-3
[exit 0]

$ LC_ALL=C grep -c '5\.8\.4' /tmp/mav-feed1008-88ab/u5.html
0
[exit 1]

$ LC_ALL=C grep -o -m1 '"updated_at":"[^"]*"' /tmp/mav-feed1008-88ab/u5.html | head -1
"updated_at":"2026-06-23T14:15:16.394Z"
[exit 0]

$ LC_ALL=C grep -o -m1 '5.8.3 Hotfix Released' /tmp/mav-feed1008-88ab/f-tag-hotfix.json | head -1
5.8.3 Hotfix Released
[exit 0]

$ LC_ALL=C grep -o '"title":"5\.8\.[0-9] Hotfix Released"' /tmp/mav-feed1008-88ab/f-tag-hotfix.json
"title":"5.8.3 Hotfix Released"
"title":"5.8.2 Hotfix Released"
"title":"5.8.1 Hotfix Released"
[exit 0]

# log 4
$ curl -sL -A Mozilla/5.0 --max-time 60 -o /tmp/mav-feed1008-88ab/f-search-584b.json -w '%{http_code} %{size_download} f-search-584b\n' 'https://forums.unrealengine.com/search.json?q=%225.8.4%22%20after%3A2026-09-22'
200 3090 f-search-584b
[exit 0]

$ curl -sL -A Mozilla/5.0 --max-time 60 -o /tmp/mav-feed1008-88ab/u4.json -w '%{http_code} %{size_download} u4.json\n' 'https://forums.unrealengine.com/t/2833315.json'
200 34975 u4.json
[exit 0]

$ node -e 'const j=JSON.parse(require("fs").readFileSync(process.argv[1]));console.log("posts",(j.posts||[]).length);for(const p of (j.posts||[]).slice(0,8))console.log(p.created_at.slice(0,10),p.topic_id,(p.blurb||"").slice(0,120))' /tmp/mav-feed1008-88ab/f-search-584b.json
posts 2
2026-10-05 2731772 UE-397316 is 'Closed' as 'Fixed'. The issue will be addressed in 5.8.4.
2026-09-23 2746676 Newly released 5.8.3 does not seem to have the fix. Is it expected as part of 5.8.4?
[exit 0]

$ node -e 'const j=JSON.parse(require("fs").readFileSync(process.argv[1]));console.log(j.title,"created",j.created_at,"last_posted_at",j.last_posted_at,"posts_count",j.posts_count)' /tmp/mav-feed1008-88ab/u4.json
5.8.3 Hotfix Released created 2026-09-22T13:29:55.931Z last_posted_at 2026-09-22T13:29:56.166Z posts_count 1
[exit 0]

$ LC_ALL=C grep -c '5\.8\.4' /tmp/mav-feed1008-88ab/u4.json
0
[exit 1]

# log 5
$ curl -sL -A Mozilla/5.0 --max-time 60 -o /tmp/mav-feed1008-88ab/f-2731772.json -w '%{http_code} %{size_download} f-2731772\n' 'https://forums.unrealengine.com/t/2731772.json'
200 43074 f-2731772
[exit 0]

$ curl -sL -A Mozilla/5.0 --max-time 60 -o /tmp/mav-feed1008-88ab/f-2746676.json -w '%{http_code} %{size_download} f-2746676\n' 'https://forums.unrealengine.com/t/2746676.json'
200 58107 f-2746676
[exit 0]

# log 6
$ for f in u1 u2 u8 u18; do printf '%s ' $f; LC_ALL=C grep -o -m1 'lastUpdated[^,]\{0,20\}' /tmp/mav-feed1008-88ab/$f.html | head -1; done
u1 u2 lastUpdated\":\"2026-09-30\"}
u8 lastUpdated\":\"2026-09-08\"}
u18 lastUpdated\":\"2024-07-31\"}
[exit 0]

$ LC_ALL=C grep -o -m1 'must set their targetSdkVersion to 34' /tmp/mav-feed1008-88ab/u2.html | head -1
must set their targetSdkVersion to 34
[exit 0]

$ LC_ALL=C grep -o -m1 '32-34 for immersive, 32-36 for 2D' /tmp/mav-feed1008-88ab/u2.html | head -1
32-34 for immersive, 32-36 for 2D
[exit 0]

$ LC_ALL=C grep -o -m1 'API level 34 will be enforced during binary upload[^\]*' /tmp/mav-feed1008-88ab/u1.html | head -1 | cut -c1-200
API level 34 will be enforced during binary upload, meaning you will not be able to upload binaries with a lower targetSdkVersion level for apps created after March 1, 2026.
[exit 0]

$ LC_ALL=C grep -o -m1 'APK files must be less than 1 GB in size[^\]*' /tmp/mav-feed1008-88ab/u18.html | head -1 | cut -c1-200
APK files must be less than 1 GB in size, but can be accompanied by multiple expansion files up to 4 GB each.
[exit 0]

$ LC_ALL=C grep -o -m1 'manifest entry lists the VR devices that your app supports[^\]*' /tmp/mav-feed1008-88ab/u8.html | head -1 | cut -c1-200
manifest entry lists the VR devices that your app supports. The runtime uses this list to decide whether to turn on compatibility mode. It also uses the list to choose the compatibility model.
[exit 0]

# log 7
$ curl -sL -A Mozilla/5.0 --max-time 60 -o /tmp/mav-feed1008-88ab/c1.html -w '%{http_code} %{size_download} c1\n' 'https://developers.meta.com/blog/meta-connect-2026-vr-start-developer-competition/'
200 463074 c1
[exit 0]

$ curl -sL -A Mozilla/5.0 --max-time 60 -o /tmp/mav-feed1008-88ab/vrc.html -w '%{http_code} %{size_download} vrc\n' 'https://developers.meta.com/vr/resources/publish-quest-req/'
200 976996 vrc
[exit 0]

$ curl -sL -A Mozilla/5.0 --max-time 60 -o /tmp/mav-feed1008-88ab/i160597.html -w '%{http_code} %{size_download} i160597\n' 'https://issues.unrealengine.com/issue/UE-160597'
403 25244 i160597
[exit 0]

# log 8
$ curl -sL -A 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/141.0.0.0 Safari/537.36' --max-time 60 -o /tmp/mav-feed1008-88ab/dp-overview.html -w '%{http_code} %{size_download} dp-overview\n' 'https://start-developer-competition-26.devpost.com/'
200 166848 dp-overview
[exit 0]

$ curl -sL -A 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/141.0.0.0 Safari/537.36' --max-time 60 -o /tmp/mav-feed1008-88ab/dp-rules.html -w '%{http_code} %{size_download} dp-rules\n' 'https://start-developer-competition-26.devpost.com/rules'
200 183553 dp-rules
[exit 0]

$ curl -sL -A 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/141.0.0.0 Safari/537.36' --max-time 60 -o /tmp/mav-feed1008-88ab/dp-updates.html -w '%{http_code} %{size_download} dp-updates\n' 'https://start-developer-competition-26.devpost.com/updates'
200 129713 dp-updates
[exit 0]

# log 9
$ curl -sL -A 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/141.0.0.0 Safari/537.36' --max-time 60 -o /tmp/mav-feed1008-88ab/dp-46826.html -w '%{http_code} %{size_download} dp-46826\n' 'https://start-developer-competition-26.devpost.com/updates/46826-give-yourself-a-schedule-save-the-date-live-build-session-october-13'
200 128592 dp-46826
[exit 0]

# log 10
$ curl -sL -A Mozilla/5.0 --max-time 90 -o /tmp/mav-feed1008-88ab/a1.html -w '%{http_code} %{size_download} a1\n' 'https://dev.epicgames.com/documentation/en-us/unreal-engine/android-development-requirements-for-unreal-engine'
200 105099 a1
[exit 0]

$ cat '/c/Program Files/Epic Games/UE_5.8/Engine/Config/Android/Android_SDK.json'
{
	"MainVersion": "r27c",
	"MinVersion": "r27c",
	"MaxVersion": "r29",

	"AutoSDKDirectory": "-28",

	"platforms": "android-36",
	"build-tools": "36.0.0",
	"cmake": "3.22.1",
	"ndk": "27.2.12479018"
}
[exit 0]

$ grep -n -E 'MajorVersion|MinorVersion|PatchVersion|"Changelist"|CompatibleChangelist' '/c/Program Files/Epic Games/UE_5.8/Engine/Build/Build.version'
2:	"MajorVersion": 5,
3:	"MinorVersion": 8,
4:	"PatchVersion": 3,
5:	"Changelist": 58210709,
6:	"CompatibleChangelist": 55116800,
[exit 0]

$ echo "NDKROOT=$NDKROOT"; echo "JAVA_HOME=$JAVA_HOME"; echo "ANDROID_HOME=$ANDROID_HOME"
NDKROOT=C:\Users\kazda\scoop\apps\android-clt\current\ndk\28.2.13676358
JAVA_HOME=C:\Program Files\Zulu\zulu-22
ANDROID_HOME=C:\Users\kazda\scoop\apps\android-clt\current
[exit 0]

$ grep -E 'Pkg.Revision|Pkg.ReleaseName' "$NDKROOT/source.properties"
Pkg.Revision = 28.2.13676358
Pkg.ReleaseName = r28c
[exit 0]

$ "$JAVA_HOME/bin/java" -version 2>&1
openjdk version "22.0.1" 2024-04-16
OpenJDK Runtime Environment Zulu22.30+13-CA (build 22.0.1+8)
OpenJDK 64-Bit Server VM Zulu22.30+13-CA (build 22.0.1+8, mixed mode, sharing)
[exit 0]

$ ls "$ANDROID_HOME/platforms" "$ANDROID_HOME/build-tools"
C:\Users\kazda\scoop\apps\android-clt\current/build-tools:
28.0.3
34.0.0
35.0.0
36.1.0

C:\Users\kazda\scoop\apps\android-clt\current/platforms:
android-31
android-34
android-36
[exit 0]

# log 11
$ curl -sL -A Mozilla/5.0 --max-time 60 -o /tmp/mav-feed1008-88ab/g1.html -w '%{http_code} %{size_download} g1\n' 'https://docs.gradle.org/current/userguide/compatibility.html'
200 178863 g1
[exit 0]

$ curl -sL -A Mozilla/5.0 --max-time 60 -o /tmp/mav-feed1008-88ab/g2.html -w '%{http_code} %{size_download} g2\n' 'https://developer.android.com/build/releases/gradle-plugin'
200 281572 g2
[exit 0]

$ grep -n 'ANDROID_TOOLS_BUILD_GRADLE_VERSION = ' '/c/Program Files/Epic Games/UE_5.8/Engine/Source/Programs/UnrealBuildTool/Platform/Android/UEDeployAndroid.cs'
29:		private const string ANDROID_TOOLS_BUILD_GRADLE_VERSION = "com.android.tools.build:gradle:8.13.0";
[exit 0]

$ sed -n 6510,6511p '/c/Program Files/Epic Games/UE_5.8/Engine/Source/Programs/UnrealBuildTool/Platform/Android/UEDeployAndroid.cs'
				BuildGradleContent.AppendLine("\t\tsourceCompatibility JavaVersion.VERSION_17");
				BuildGradleContent.AppendLine("\t\ttargetCompatibility JavaVersion.VERSION_17");
[exit 0]

$ ls '/c/Program Files/Epic Games/UE_5.8/Engine/Build/Android'
[exit 2]

# log 12
$ curl -sL -A Mozilla/5.0 --max-time 60 -o /tmp/mav-feed1008-88ab/g3.html -w '%{http_code} %{size_download} g3\n' 'https://developer.android.com/build/releases/past-releases/agp-8-13-0-release-notes'
200 275697 g3
[exit 0]

# log 13
$ LC_ALL=C grep -o -m1 'Java runtime: OpenJDK 21.0.3 2024-04-16' /tmp/mav-feed1008-88ab/a1.html | head -1
[exit 0]

$ LC_ALL=C grep -o -m1 'Build-tools: 35.0.1' /tmp/mav-feed1008-88ab/a1.html | head -1
Build-tools: 35.0.1
[exit 0]

$ LC_ALL=C grep -o -m1 'NDK Version:  r27c' /tmp/mav-feed1008-88ab/a1.html | head -1
[exit 0]

$ LC_ALL=C grep -o -m1 'Current UE Version: 5.8' /tmp/mav-feed1008-88ab/a1.html | head -1
Current UE Version: 5.8
[exit 0]

$ LC_ALL=C grep -o -m1 '"updated_at":"[^"]*"' /tmp/mav-feed1008-88ab/a1.html | head -1
"updated_at":"2026-06-12T21:00:06.381Z"
[exit 0]

$ LC_ALL=C grep -o -m1 'The maximum API level that Android Gradle Plugin 8.13 supports is API level 36.1' /tmp/mav-feed1008-88ab/g3.html | head -1
The maximum API level that Android Gradle Plugin 8.13 supports is API level 36.1
[exit 0]

$ LC_ALL=C grep -A2 -m1 '<td align="right">Gradle</td>' /tmp/mav-feed1008-88ab/g3.html | sed 's/^ *//'
<td align="right">Gradle</td>
<td align="center">8.13</td>
<td align="center">8.13</td>
[exit 0]

$ LC_ALL=C grep -A2 -m1 '<td align="right">JDK</td>' /tmp/mav-feed1008-88ab/g3.html | sed 's/^ *//'
<td align="right">JDK</td>
<td align="center">17</td>
<td align="center">17</td>
[exit 0]

$ LC_ALL=C grep -o -m1 'Last updated 2026-10-01 UTC' /tmp/mav-feed1008-88ab/g3.html | head -1
Last updated 2026-10-01 UTC
[exit 0]

$ LC_ALL=C grep -A2 -m1 '<p class="tableblock">22</p></td>' /tmp/mav-feed1008-88ab/g1.html | sed 's/^ *//'
<td class="tableblock halign-left valign-top"><p class="tableblock">22</p></td>
<td class="tableblock halign-left valign-top"><p class="tableblock">8.7</p></td>
<td class="tableblock halign-left valign-top"><p class="tableblock">8.8 and after</p></td>
[exit 0]

$ LC_ALL=C grep -o -m1 'Version 9.8.1' /tmp/mav-feed1008-88ab/g1.html | head -1
Version 9.8.1
[exit 0]

$ LC_ALL=C grep -o -m1 'The fix has been submitted and will be in 5.8.4.' /tmp/mav-feed1008-88ab/f-2731772.json | head -1
The fix has been submitted and will be in 5.8.4.
[exit 0]

$ LC_ALL=C grep -o -m1 'The issue will be addressed in 5.8.4.' /tmp/mav-feed1008-88ab/f-2731772.json | head -1
The issue will be addressed in 5.8.4.
[exit 0]

$ node -e 'const j=JSON.parse(require("fs").readFileSync(process.argv[1]));for(const p of j.post_stream.posts)if(/5\.8\.4/.test(p.cooked))console.log(p.created_at,p.username,p.staff?"staff":"")' /tmp/mav-feed1008-88ab/f-2731772.json
2026-09-17T16:37:08.056Z DevonPenney
2026-09-18T15:02:14.921Z DevonPenney
2026-10-05T17:00:21.591Z Bug-Reporter staff
[exit 0]

$ LC_ALL=C grep -o -m1 'it should be in the next 5.8 hotfix' /tmp/mav-feed1008-88ab/f-2746676.json | head -1
it should be in the next 5.8 hotfix
[exit 0]

$ node -e 'const j=JSON.parse(require("fs").readFileSync(process.argv[1]));for(const t of j.topic_list.topics)if(/^5\.8\.\d Hotfix/.test(t.title))console.log(t.created_at,t.id,t.title)' /tmp/mav-feed1008-88ab/f-tag-hotfix.json
2026-09-22T13:29:55.931Z 2833315 5.8.3 Hotfix Released
2026-08-25T13:09:12.557Z 2746335 5.8.2 Hotfix Released
2026-07-28T13:19:53.899Z 2738864 5.8.1 Hotfix Released
[exit 0]

$ node -e 'for(const [a,b] of [["2026-07-28","2026-08-25"],["2026-08-25","2026-09-22"]])console.log(a,"->",b,(new Date(b)-new Date(a))/864e5,"days");console.log("2026-09-22 + 28 days =",new Date(Date.parse("2026-09-22")+28*864e5).toISOString().slice(0,10))'
2026-07-28 -> 2026-08-25 28 days
2026-08-25 -> 2026-09-22 28 days
2026-09-22 + 28 days = 2026-10-20
[exit 0]

# log 14
$ LC_ALL=C grep -o -m1 'Java runtime:.\{1,12\}OpenJDK 21.0.3 2024-04-16' /tmp/mav-feed1008-88ab/a1.html | head -1 | od -c | sed -n 1,3p
0000000   J   a   v   a       r   u   n   t   i   m   e   :   &   n   b
0000020   s   p   ;   O   p   e   n   J   D   K       2   1   .   0   .
0000040   3       2   0   2   4   -   0   4   -   1   6  \n
[exit 0]

$ LC_ALL=C grep -c 'Java runtime:.\{1,12\}OpenJDK 21.0.3 2024-04-16' /tmp/mav-feed1008-88ab/a1.html
2
[exit 0]

$ LC_ALL=C grep -o -m1 'NDK Version:.\{1,14\}r27c' /tmp/mav-feed1008-88ab/a1.html | head -1
NDK Version:&nbsp;&nbsp;r27c
[exit 0]

$ LC_ALL=C grep -o -m1 'Minimum target SDK for compilation:.\{1,12\}SDK 34' /tmp/mav-feed1008-88ab/a1.html | head -1
Minimum target SDK for compilation: SDK 34
[exit 0]

# log 15
$ LC_ALL=C grep -o 'Updated: [A-Z][a-z]* [0-9]*, 2026' /tmp/mav-feed1008-88ab/u10.html /tmp/mav-feed1008-88ab/u17.html
/tmp/mav-feed1008-88ab/u10.html:Updated: Sep 23, 2026
/tmp/mav-feed1008-88ab/u17.html:Updated: Sep 22, 2026
[exit 0]

$ LC_ALL=C grep -o '"version":"2[0-9][0-9]\.[0-9]"' /tmp/mav-feed1008-88ab/u10.html | sort -u
"version":"201.0"
"version":"203.0"
"version":"205.0"
"version":"207.0"
[exit 0]

$ LC_ALL=C grep -o '"version":"2[0-9][0-9]\.[0-9]"' /tmp/mav-feed1008-88ab/u17.html | sort -u
"version":"201.0"
"version":"203.0"
"version":"205.0"
"version":"207.0"
[exit 0]

$ LC_ALL=C grep -o '"release_date":[0-9]*,"version":"207.0"' /tmp/mav-feed1008-88ab/u10.html /tmp/mav-feed1008-88ab/u17.html | sort -u
/tmp/mav-feed1008-88ab/u10.html:"release_date":1790206631,"version":"207.0"
/tmp/mav-feed1008-88ab/u17.html:"release_date":1790106159,"version":"207.0"
[exit 0]

$ node -e 'const j=JSON.parse(require("fs").readFileSync(process.argv[1]));console.log("posts",j.posts.length,"term",j.grouped_search_result.term)' /tmp/mav-feed1008-88ab/f-search-584.json
posts 0 term 5.8.4 hotfix released
[exit 0]

$ LC_ALL=C grep -o -m1 'November 18, 2026 at 12:00:00 PM PT' /tmp/mav-feed1008-88ab/dp-rules.html | head -1
November 18, 2026 at 12:00:00 PM PT
[exit 0]

$ LC_ALL=C grep -o -m1 'Deadline: Nov 18, 2026 @ 12:00pm PST' /tmp/mav-feed1008-88ab/dp-overview.html | head -1
Deadline: Nov 18, 2026 @ 12:00pm PST
[exit 0]

$ LC_ALL=C grep -o -m1 'New Experience</em> means starting from a blank slate on or after' /tmp/mav-feed1008-88ab/dp-46826.html | head -1
New Experience</em> means starting from a blank slate on or after
[exit 0]

$ LC_ALL=C grep -o -m1 'define your track and division</strong>[^<]*' /tmp/mav-feed1008-88ab/dp-46826.html | head -1
define your track and division</strong>—by Friday, October 9
[exit 0]

$ LC_ALL=C grep -o -m1 'November 18, 2026' /tmp/mav-feed1008-88ab/c1.html | head -1
November 18, 2026
[exit 0]

$ wc -c /tmp/mav-feed1008-88ab/dp-rules.html /tmp/mav-feed1008-88ab/dp-overview.html
183553 /tmp/mav-feed1008-88ab/dp-rules.html
166848 /tmp/mav-feed1008-88ab/dp-overview.html
350401 total
[exit 0]

$ node apps/vr/tools/check-pin.mjs
pin OK: 9 files @ baeac66
[exit 0]

# log 16
$ grep -n 'public string GetBuildToolsVersion\|"BuildToolsOverride"\|BestVersionString == "latest"' '/c/Program Files/Epic Games/UE_5.8/Engine/Source/Programs/UnrealBuildTool/Platform/Android/AndroidToolChain.cs'
1471:		public string GetBuildToolsVersion()
1483:			Ini.GetString("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "BuildToolsOverride", out BestVersionString);
1485:			if (BestVersionString == null || String.IsNullOrEmpty(BestVersionString) || BestVersionString == "latest")
[exit 0]

$ git show master:docs/PROJECT-PLAN.md | sed -n 205p | cut -c1-140
- `NDKROOT` is **unset**, and the JDK on PATH is **Zulu 22**. Whether UE 5.8 accepts NDK r28 and JDK 22 is unverified; that is the first thi
[exit 0]

$ git show master:docs/XR-TOOLCHAIN.md | sed -n 75p
| JDK | Zulu 22.0.1, Azul, `C:\Program Files\Zulu\zulu-22` (`JAVA_HOME`) |
[exit 0]

$ git show master:apps/vr/tools/install-xr.ps1 | sed -n '26p;104p'
    return (Get-JdkMajor $JavaHome) -ge 17
    $candidates = @('C:\Program Files\Zulu\zulu-22')
[exit 0]

$ git show master:CLAUDE.md | sed -n 44p
The engine has been UE 5.8.3 (CL 58210709) since 2026-10-03. A fresh worktree without the MetaXR plugins (marked
[exit 0]

# log 17
$ LC_ALL=C grep -o -m1 'UE-397316 is .\{1,4\}Closed.\{1,4\} as .\{1,4\}Fixed.\{1,4\}\. The issue will be addressed in 5.8.4.' /tmp/mav-feed1008-88ab/f-2731772.json | head -1
UE-397316 is ‘Closed’ as ‘Fixed’. The issue will be addressed in 5.8.4.
[exit 0]

$ git show master:docs/XR-TOOLCHAIN.md | sed -n '66p;71p;78p' | cut -c1-120
Android: (Status=Valid, MinAllowed_Sdk=r27c, MaxAllowed_Sdk=r29, Current_Sdk=r28c, Allowed_AutoSdk=r27c, Current_AutoSdk
| Piece | Value |
| CMake | 3.22.1 present |
[exit 0]
```
