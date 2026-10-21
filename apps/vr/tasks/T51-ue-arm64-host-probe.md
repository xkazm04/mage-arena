# T51 - Read-only probe: can UE 5.8 UnrealBuildTool use an arm64-hosted compiler?

Status: open (10-minute read-only job; needs Wolf, the only machine with the engine source; written 2026-10-21 on Lynx)
Max turns: 30

## Goal
Settle, from the engine source, the one question that decides whether hosting Unreal on Lynx (Windows ARM64) is worth a
time-boxed experiment. **The deliverable is a decision input, not a decision**: the owner decides whether to spend a day on an
install. This card installs nothing, builds nothing, downloads nothing.

## Context (carried here because the earlier probe report was session-local and is not in the repo)
- `docs/DECISIONS.md` 2026-10-20 put all Unreal work on Wolf and gave Lynx no Unreal install. On 2026-10-21 the owner asked for
  a real attempt before accepting the split (Lynx is the stronger machine; two boxes is awkward). A desk probe ran the same
  day and returned **"emulation possible but impractical"**.
- Epic's "Windows ARM64 Support in Unreal Engine 5.8" page says the Editor is not supported on ARM64, nor native ARM64 or
  ARM64EC editor builds; the support is "for game targets only"; and "while it is possible to run the x64 editor on ARM64,
  the recommended workflow is to develop on an x64 PC and deploy game builds to ARM64 hardware for testing". ARM64 is a
  5.8 target, not a 5.8 host.
- The Android NDK ships no Windows-ARM64 host (only `windows-x86_64`, r27-r29), so every clang call in a Quest APK build
  would be emulated. The probe recommended **not** moving the APK path to Lynx under any outcome. Wolf stays the authority for
  every APK and every reported build result.
- Lynx has Visual Studio Build Tools 2022 17.14.42, MSVC 14.44.35207, and `...\bin\Hostarm64\x64\cl.exe` (a natively
  arm64-hosted compiler that emits x64), Windows SDK 10.0.26100, 555 GB free. The `.uproject` marks the MetaXR plugins
  `"Optional": true`, so a first editor build needs none. `CLAUDE.md` gives a cold editor build of about 393 s as the
  baseline.
- **Unknown 1 (the high-leverage one):** can UE 5.8 UnrealBuildTool select a `Hostarm64` compiler directory? If yes, C++
  compiles on Lynx could run at native speed. If UBT is x64-host-only, the whole chain is emulated against a baseline nobody
  can predict.
- **Unknown 2 (an inference, not a sourced claim):** under emulation UBT runs as an x64 process, where .NET may report
  `OSArchitecture == Arm64` while `ProcessArchitecture == X64`. If the default Win64 architecture is chosen from
  `OSArchitecture`, UBT would default to arm64, the one configuration Epic says is unsupported. `apps/vr/tools/build.ps1`
  passes no `-Architecture`.

## Read first
- `CLAUDE.md`, `docs/DECISIONS.md` (they outrank this card), `docs/concepts/WOLF-PLAN.md` section b (machine setup on Wolf).

## Deliverables
1. On Wolf, from the engine root (`C:\Program Files\Epic Games\UE_5.8`), search the UBT Windows platform source,
   `Engine\Source\Programs\UnrealBuildTool\Platform\Windows\`, read-only. `VCEnvironment.cs` and `UEBuildWindows.cs` are the
   files most likely to answer it:
   ```
   grep -rniE "Hostarm64|HostArm64|OSArchitecture|ProcessArchitecture|RuntimeInformation" <that folder>
   grep -niE "DefaultArchitecture|WindowsArchitecture|Arm64EC|arm64" UEBuildWindows.cs VCEnvironment.cs
   ```
   If the engine is installed without source, record "no source on Wolf" and stop.
2. Write a plain verdict into this card (a "Verdict" section at the bottom, nothing else edited), answering:
   (a) can UBT use a `Hostarm64` compiler directory, yes or no, with the file and symbol that decides it; and
   (b) how is the default Win64 architecture chosen, and would an emulated x64 UBT on an arm64 OS pick arm64.

## Constraints
- **Read-only. No installation, no build, no download, no change to Wolf's engine or checkout.** Do not run `Build.bat` or UBT.
- **Do not touch the Lynx checkout** or any other machine.
- Do not commit, push, or touch any repository other than this one. Do not call paid services. Do not edit
  `docs/DECISIONS.md`.

## Acceptance
The grep output quoted verbatim (command, then output, with file and line), and the verdict. Nothing is built, so no suite runs.

## What the owner does with the verdict
- **UBT can use a `Hostarm64` compiler:** a time-boxed experiment on Lynx is defensible (one day, editor target only, no
  Meta plugins, no APK). Wolf stays the authority for every reported build result either way.
- **UBT is x64-host-only:** fold back to the 2026-10-20 ruling and spend the effort on the Lynx-to-Wolf handoff instead.

## Report
Write `runs/T51/REPORT.md`: the commands, the verbatim output, the verdict, and anything you could not check and why.
