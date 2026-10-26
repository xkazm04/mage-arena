# Wolf plan: from today to the 18 Nov submission

Written 2026-10-21 on Lynx (Windows 11 ARM64, no Unreal Engine by ruling, `docs/DECISIONS.md` 2026-10-20 "Unreal host"), on
master `e1323e9`. Nothing in this file was built or run in Unreal. Every UE result named here is a target that a Wolf run
must produce. `docs/DECISIONS.md` outranks this file. Where a card and this file differ, the card wins on its own content
and this file wins on order. Anything Lynx could not check is marked **[verify on Wolf]**.

## a. Purpose and how to use it

This is the run order for the x64 UE host, Wolf. All Unreal work happens there (DECISIONS 2026-10-20 "Unreal host"). Lynx
writes cards, docs and Node tooling and reports no UE result.

How a Wolf session uses this file:

1. Read `CLAUDE.md`, `docs/DECISIONS.md` (newest first), then this file. Then read the card of the phase you are on.
2. Work through the phases in order. Section b runs once per machine (re-check it after any engine or plugin change).
   A phase starts only when the phases it depends on are ticked, or when its "Depends on" line allows otherwise.
3. Tick a box (`- [x]`) only when the "Done when" proof has run on Wolf and its real output matches. Add a short line next
   to the box: `Wolf <date>, <commit sha>, <the key output line>`. Never tick on a partial run, a timeout or an exit code
   alone (`build.ps1` returns Build.bat's exit code; confirm `Result: Succeeded` in the build log, as T33-T36 say).
4. Commit the evidence with the tick, in small commits (`<card id>: ...` or `docs(...): ...`, `CLAUDE.md` "Git"). Raw logs
   stay in `runs/` (gitignored, `.gitignore:26`). Review media goes to `kiro/mage-arena-archive/` (DECISIONS 2026-10-07
   "evidence out of git"). What gets committed: the code, the card status line, the card's committed record (for example
   `docs/research/BUDGET-DG4-2026-10.md` section 11 for T37) and the tick here.
5. If a proof fails, do not tick. Write the exact failing line next to the box, stop that phase, and go to the next
   phase that does not depend on it.
6. Owner-blocked items are marked **OWNER**. Do not wait on them: prepare what the card allows, then move on.

Dates. The only hard date is the competition final deadline, **Wed 18 Nov 2026**
(https://start-developer-competition-26.devpost.com/; 12:00 PT per `docs/PROJECT-PLAN.md` line 4). Every other date here
is a soft internal milestone (DECISIONS 2026-10-21, "Deadlines and Q9"). A missed soft date is a re-plan, not a stop.

## b. Machine setup on Wolf

### Step 0: register this app on Wolf (do this first)

Wolf's tooling only sees a project that is registered. Before any other step:

- [ ] parked: OWNER, not attempted by the Wolf builder 2026-10-24. **Fleet registry.** In Wolf's `kiro/ai-registry` checkout (clean tree, `git pull --rebase` first), run
  `node scripts/register-project.mjs --slug mage-arena-vr --path <this checkout> --dry-run`, read it, then run it without
  `--dry-run`. Commit `projects.json` with a `fleet:` subject and push to `origin main`. The script refuses when the
  slug already points at a different checkout on Wolf; re-pointing is a decision, so ask the owner if it refuses. Wolf is already registered as `mage-arena-vr` at `kiro/mage-arena-vr`. If Wolf's checkout is now `kiro/mage-arena` (the GitHub name), the script refuses; ask the owner whether to re-point it.
- [ ] parked: OWNER, not attempted by the Wolf builder 2026-10-24. **Personas.** If the Personas app runs on Wolf, register the checkout as a dev project (Dev Tools, or
  `POST /dev-tools/projects` with name and root path) and run a context scan. The headless App Masters for this
  project run on Lynx: Wolf does not run a second master for it. It reports back through commits on `origin/master`.
- [ ] parked: OWNER, not attempted by the Wolf builder 2026-10-24. Proof: `projects.json` lists the Wolf checkout for `mage-arena-vr`, and the registry commit is on `origin/main`.


Run each step from the repo root in PowerShell. The checkout path on Wolf is `kiro/mage-arena-vr` in the registry's
`projects.json` (`ai-registry/projects.json:107-110`); on Lynx the same repo is `kiro/mage-arena`, remote
`https://github.com/xkazm04/mage-arena`. **[verify on Wolf]** which path and remote Wolf has.

Record once, at the top of the first Wolf commit's evidence: `hostname`, the fleet name Wolf, and the engine build.
**[verify on Wolf]** whether the Windows hostname is literally `Wolf`; if not, record both.

First Wolf evidence: **Wolf = hostname `kaziho`** (x64; not literally `Wolf`), engine UE 5.8.3 CL 58210709, master `e321455`, 2026-10-24.

- [x] Wolf kaziho 2026-10-24 e321455 `"PatchVersion": 3, "Changelist": 58210709, "CompatibleChangelist": 55116800`. **B1. Unreal Engine 5.8.3 at `C:\Program Files\Epic Games\UE_5.8`.** The scripts hard-code this path
  (`apps/vr/tools/build.ps1:4`, `install-xr.ps1:9`, `package-android.ps1:38`, `capture-*.ps1`). Expected version since
  2026-10-03: 5.8.3, Changelist 58210709, CompatibleChangelist 55116800 (`CLAUDE.md` "Build"; `docs/XR-TOOLCHAIN.md`
  "5.8.3 re-check").
  ```powershell
  Get-Content "C:\Program Files\Epic Games\UE_5.8\Engine\Build\Build.version"
  # expected: "PatchVersion": 3, "Changelist": 58210709, "CompatibleChangelist": 55116800
  ```
  If it reads 5.8.4, the launcher updated the engine by surprise, against DECISIONS 2026-10-13 (F12). Stop and tell the
  owner. Do not build until the F12 procedure has run (that entry, steps 1-3).
- [ ] parked: OWNER, not attempted by the Wolf builder 2026-10-24 (B1 still read 5.8.3 that day). **B2. Launcher auto-update off for the 5.8 install** (OWNER, F12; open since `docs/research/STATE-EVAL-2026-10-20.md`
  section 6 risk 7). Epic Games Launcher, Unreal Engine, Library, the 5.8 tile: turn auto-update off. **[verify on Wolf]**
  the exact menu item in the current launcher. Proof: B1's `Build.version` still reads 5.8.3 at every later phase; check
  it before each phase.
- [ ] DEFERRED 2026-10-24, ruling 2 (not needed now; stays parked). parked: OWNER, not attempted by the Wolf builder 2026-10-24. Read only: both `Test-Path` lines print False (Android not installed). **B3. Android target support in the Epic launcher** (OWNER, open since 2026-10-03, `docs/XR-TOOLCHAIN.md` "Owner
  steps" 1-2). Library, the 5.8 tile, the drop-down next to Launch, Options, enable Android, Apply. It is a large
  download (**[verify on Wolf]** the size). Proof:
  ```powershell
  Test-Path "C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Android\UnrealGame.target"   # expected: True
  Test-Path "C:\Program Files\Epic Games\UE_5.8\Engine\Source\ThirdParty\cxademangle"        # expected: True
  ```
  After it is installed, remove the `[InstalledPlatforms]` block from
  `%LOCALAPPDATA%\Unreal Engine\Engine\Config\UserEngine.ini` and keep the SDK/NDK/Java paths (`docs/XR-TOOLCHAIN.md`,
  "Android toolchain actually used", last paragraph).
- [x] Wolf kaziho 2026-10-24 e321455 vswhere `17.14.23` (Community 2022, NativeGame + NativeDesktop); Clang component at `C:/Program Files/Microsoft Visual Studio/2022/Community`. **B4. Visual Studio.** The version that built UE 5.8.2 and 5.8.3 here (2026-10-02, 2026-10-07) is not written down
  anywhere in the repo. **[verify on Wolf]** the version UE 5.8's release notes require. Needed: the workloads "Game
  development with C++" and "Desktop development with C++", MSVC x64/x86 build tools, a Windows 11 SDK, and "C++ Clang
  tools for Windows" (for `apps/vr/tools/lint/tidy.ps1:22`; `format-check.mjs` also finds clang-format there)
  (`docs/research/STATE-EVAL-2026-10-20.md` section 8). Proof:
  ```powershell
  $vsw = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
  & $vsw -latest -products * -requires Microsoft.VisualStudio.Workload.NativeGame Microsoft.VisualStudio.Workload.NativeDesktop -property catalog_productDisplayVersion
  # expected: one version line (write it into this box); empty means a workload is missing
  & $vsw -latest -products * -requires Microsoft.VisualStudio.Component.VC.Llvm.Clang -property installationPath
  # expected: one path
  ```
- [ ] parked: OWNER, not attempted by the Wolf builder 2026-10-24 (install-xr.ps1 not run). Found, read only: main checkout `apps/vr/Game/Plugins/MetaXR` and `MetaXRInteraction`, `VersionName` 1.207.0, Win64 `BuildId` 55116800. **B5. Meta XR v207 and the Meta Interaction SDK v207, project-local** (route R-A, `docs/XR-TOOLCHAIN.md`
  "Decision"). The plugin trees are gitignored; the script recreates them and checks both zips against a pinned SHA-256
  (`install-xr.ps1:52-55`). About 1.16 GB if not cached.
  ```powershell
  powershell -NoProfile -File apps/vr/tools/install-xr.ps1
  # expected: SHA256_OK=UnrealMetaXRPlugin.207.0.zip, SHA256_OK=MetaXRInteractionPackage.207.0.0.zip,
  #           PLUGIN_PRESENT (or PLUGIN_EXTRACTED) for both, PLUGIN_BUILD_ID=55116800, ENGINE_COMPATIBLE_CHANGELIST=55116800;
  #           no PLUGIN_BUILD_ID_MISMATCH and no OWNER_ACTION line
  ```
  An `OWNER_ACTION` line for a login or licence page means the owner accepts the `oculussdk` licence (id 790194175111532)
  in a browser and re-runs (`docs/XR-TOOLCHAIN.md` "Owner steps" 4).
- [ ] parked: OWNER, not attempted by the Wolf builder 2026-10-24. Found, read only: `meta_openxr_simulator.json` present (True), `"device_profile": "Meta Quest 3"`. **B6. Meta XR Simulator v207, Quest 3 profile.** Recorded location:
  `C:\Users\kazda\AppData\Local\MetaXRSimulatorExtract\PFiles\MetaXRSimulator\v207.0` (`docs/XR-TOOLCHAIN.md` "Simulator
  boot"). **[verify on Wolf]** that it is still there. The boot uses `XR_RUNTIME_JSON`, not the HKLM activation script.
  ```powershell
  $sim = "$env:LOCALAPPDATA\MetaXRSimulatorExtract\PFiles\MetaXRSimulator\v207.0"
  Test-Path "$sim\meta_openxr_simulator.json"                                     # expected: True
  Select-String -Path "$sim\config\sim_core_configuration.json" -Pattern '"device_profile"'
  # expected: "device_profile": "Meta Quest 3"
  ```
- [ ] parked: OWNER, not attempted by the Wolf builder 2026-10-24 (Turnkey and -SelfTest not run). Found, read only: `ANDROID_HOME` `C:/Users/kazda/scoop/apps/android-clt/current`, NDK `Pkg.Revision = 28.2.13676358`, `openjdk version "22.0.1"` at `C:/Program Files/Zulu/zulu-22`. **B7. Android SDK, NDK, JDK** (`docs/XR-TOOLCHAIN.md` "Versions on this machine"). Recorded: SDK
  `C:\Users\kazda\scoop\apps\android-clt\current` (`ANDROID_HOME`); NDK 28.2.13676358 (r28c, `NDKROOT`; the engine
  accepts r27c-r29 and pins 27.2.12479018); JDK Zulu 22.0.1 (`JAVA_HOME` `C:\Program Files\Zulu\zulu-22`; Epic documents
  OpenJDK 21.0.3, and 21 is the fallback if Gradle fails on a Java version, F11); platforms android-31/34/36; build-tools
  28.0.3, 34.0.0, 35.0.0, 36.1.0 (36.0.0, which `Android_SDK.json` asks for, is not installed; install it only if the
  package log names it); CMake 3.22.1.
  ```powershell
  $env:ANDROID_HOME; Get-Content "$env:NDKROOT\source.properties" | Select-String Pkg.Revision   # expected: 28.2.13676358
  & "$env:JAVA_HOME\bin\java.exe" -version                                                     # expected: 22.0.1 (Zulu) or 21.x
  & "C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\RunUAT.bat" Turnkey -command=VerifySdk -platform=Android
  # expected (as on 2026-10-03, runs/T05/turnkey-verify-3.txt): Android: (Status=Valid, ... Current_Sdk=r28c ...)
  powershell -NoProfile -File apps/vr/tools/package-android.ps1 -SelfTest                       # expected: exit 0
  ```
- [x] Wolf kaziho 2026-10-24 e321455 `self-test OK (clang-format version 19.1.5): house style passes, misformatted code fails, untouched lines are ignored`. **B8. clang-format** (the format gate refuses to skip when it is missing, STATE-EVAL section 4).
  ```powershell
  node apps/vr/tools/lint/format-check.mjs --self-test
  # expected: "self-test OK (clang-format version <n>): house style passes, misformatted code fails, untouched lines are ignored"
  ```
  Lynx printed clang-format 19.1.5 on 2026-10-21; CI uses Ubuntu's version, so counts are not CI parity.
- [x] Wolf kaziho 2026-10-24 e321455 `ffmpeg version 2025-02-17-git-b92577405b-essentials_build-www.gyan.dev`, `ffprobe version 2025-02-17-git-b92577405b-essentials_build-www.gyan.dev`. **B9. ffmpeg and ffprobe on PATH** (`capture-style-pick.ps1:64`, `capture-colour-audio.ps1:58`, `:72`).
  ```powershell
  ffmpeg -version | Select-Object -First 1; ffprobe -version | Select-Object -First 1   # expected: a version line each
  ```
- [x] Wolf kaziho 2026-10-24 e321455 `git lfs pull` clean; `git lfs ls-files` 394 lines, all marked present (`*`). **B10. Git LFS objects present.**
  ```powershell
  git lfs install; git lfs pull; (git lfs ls-files | Measure-Object -Line).Lines
  # expected: 279 at e1323e9 (measured on Lynx 2026-10-21); more after later LFS commits
  ```
- [x] Wolf kaziho 2026-10-24 e321455 `v24.14.0`; `pin OK: 9 files @ baeac66`, `conformance OK: 45 vectors`. **B11. Node.** Lynx runs v24.22.0. **[verify on Wolf]** the version; any Node 24 line is fine for the gates.
  ```powershell
  node --version; node apps/vr/tools/check-pin.mjs   # expected: v24.x; "pin OK: 9 files @ baeac66" and "conformance OK: 45 vectors"
  ```
- [x] Wolf kaziho 2026-10-24 e321455 `wrote 270 corpus clips`, `wrote 5 noise clips (180.000s)`, `wrote 180 impostor clips`, `wrote 90 mouse clips`; `git status --short` empty. **B12. Sigil test corpus** (gitignored; without it `Sigils.Accuracy` and `Sigils.NoFalseCasts` fail, `CLAUDE.md`
  "Build"). Never run `generate.mjs` bare or with `--templates`.
  ```powershell
  node apps/vr/tools/clipgen/generate.mjs --corpus 30 --seed 1000; node apps/vr/tools/clipgen/mousepaths.mjs; git status --short
  # expected (2026-10-07): 270 corpus, 5 noise, 180 impostor clips, then 90 mouse clips; git status shows no new tracked change
  ```
- [x] Wolf kaziho 2026-10-24 e321455 cold with plugins: `Result: Succeeded`, `Total execution time: 419.88 seconds`. **B13. Cold editor build.**
  ```powershell
  powershell -NoProfile -File apps/vr/tools/build.ps1
  # expected: exit 0 and "Result: Succeeded" in the UBT output (cold, about 393 s without plugins, 743 s with, CLAUDE.md / XR-TOOLCHAIN)
  ```

## c. Phases, critical path first

Each phase: the card, what proves it done, an effort estimate (Wolf session time, not owner time), a soft target date,
and what depends on it. The suite command in the proofs is the one in section d.

Critical path to 18 Nov: **P0 baseline -> P2 T39 -> P3 T37 -> P4 T16 -> P6 T38 -> P8 T41 and the gate record -> P9
device -> P10 submission**, with **P5 (first APK)** in parallel as soon as the owner has done B3. The APK path is the
longest unknown (`docs/research/STATE-EVAL-2026-10-20.md` section 6 risk 3), so P5 is pulled forward rather than left for
the end.

### P0. Baseline on current master (soft: Thu 22 Oct; effort 0.5 day)

- Depends on: section b (B3 is not needed for P0).
- [x] Wolf kaziho 2026-10-24 e321455 `MageArena.` 244/244 pass (45/45 Conformance); `MageArenaDesign.` failed=5, exactly the five known; Node gates green. Run section d's Node gates and both UE suites on clean master. Save `runs/wolf/baseline-<date>/` (logs plus the
  `-ReportExportPath` JSON). Expected: `MageArena.` all green (last count on record 234/234, `docs/design/V2-ROADMAP.md`,
  2026-10-08; the 45 `MageArena.Kernel.Conformance.*` last 45/45 at `b7f1009`, 2026-10-16), and `MageArenaDesign.` red
  in exactly the five known tests: `Duel.FireSeated`, `Duel.AirSeated`, `Session.FullSeated`, `Session.Wave1Seated`,
  `Session.CreaturesSeated` (`CLAUDE.md` "Build"). This is the first UE run since 2026-10-16
  (`docs/research/KPI-2026-10-21.md` K4) and the first K2b reading on current master (K2b table).
- [x] Wolf kaziho 2026-10-24 e321455 `docs/research/WOLF-BASELINE-2026-10-24.md` (244 + 7 rows). Commit the per-test result table as `docs/research/WOLF-BASELINE-2026-10-DD.md` (DD = the run day; host, engine
  build, commit, pass/fail per test, durations). Every later card compares per test against it (T24, T25, T27, T28 and T37
  all require a before/after per-test comparison).
- Not done when: any `MageArena.` test is red, or a sixth `MageArenaDesign.` test is red. Then that red is the first work
  item; record it and ask the App Master before P2.

### P1. Overlay end-state goldens before T39 (recommended; soft: Thu 22 Oct; effort 0.5-1 day)

- Source: `docs/research/ARCHITECTURE-REVIEW-2026-10-21.md` A1 and A3(b). **No card exists**; T39 recommends it before its
  first commit (`apps/vr/tasks/T39-...md`, "Recommended before the first `T39:` commit"). The Claude session orchestrates
  and writes cards (`docs/ORCHESTRATION.md`), so the Wolf session may write the card from A1 and A3(b) as an
  `apps/vr/tasks/T42-...md` (**[verify on Wolf]** the next free id), or skip with a one-line reason here.
- [x] Wolf kaziho 2026-10-24 (T54 commit) card T54 (already written on Lynx; no new id needed). Red: `overlay changed since recording: combat.vr.json recorded d3f20246... now 210c5934...`; green: `MageArena.` 246/246; boot: `vr overlay sha256 d0ee5333..., variant live` next to `pin SHA1 11F9B16C...`. Done when: `MageArena.Overlay.Golden` runs about five fixed-seed bouts on the live overlay and writes
  `apps/vr/data/scenarios-vr/overlay/*.json` with the overlay sha256; it was seen red on a one-byte change to
  `combat.vr.json`; the boot log prints the overlay sha256 and the variant next to the pin hash.
- Why before T39: each `T39: DF-00x` commit then re-records the goldens and its diff shows which bouts that DF moved (A1).

### P2. T39: adopt DF-003/005/006/007/008/009 and re-run MageArenaDesign once (soft: Thu 22 - Sun 25 Oct; effort 2-3 days)

- Card: `apps/vr/tasks/T39-adopt-df-recommendations-and-rerun-design.md` (status "ready for Wolf"). Ruling: DECISIONS
  2026-10-20 "adopt every recommendation"; the full Tiro day, no cut (DECISIONS 2026-10-20 "Cut slice").
- Order inside the card: DF-003 and DF-006 A data, then DF-009, DF-007 B, then the C++ ones (DF-005, DF-006 B, DF-007 A,
  DF-008). One commit per DF, `T39: DF-00x ...`. Hooks, not inline branches: DF-005 in `FVrRuleset::TickSplitLatch`, DF-006
  B in `VrRivals.cpp` `TickRivalPhases`, DF-008 through `MageInput(..., const FVrRuleset*)` (the card's table).
- [x] Wolf kaziho 2026-10-24 5b13463 `MageArena.` 246/246; combat.vr.json wide set live, stale TV text replaced; `fire-duel-competence-1*.json` untouched. DF-003 `wide` live in `apps/vr/data/vr/combat.vr.json` (stale TV text at `:68`, `:134` replaced); the null-path
  goldens `fire-duel-competence-1*.json` untouched.
- [x] Wolf kaziho 2026-10-24 0acf0c2 / a64eab5 / bef9323 / 0f33106 / 7f4926d / 6f00555, each with its literals and goldens; `MageArena.` 249/249 at 6f00555. DF-009, DF-007 B, DF-005, DF-006 B, DF-007 A, DF-008, each committed with the test literals it moved citing their
  overlay field (the card's "Test files each DF commit may update").
- [x] Wolf kaziho 2026-10-24, T39 commits 5b13463 / 0acf0c2 / a64eab5 / bef9323 / 0f33106 / 7f4926d / 6f00555: `MageArena.` 249/249; MageArenaDesign: Wave1Seated + CreaturesSeated green, FireSeated/AirSeated/FullSeated red, accepted by owner ruling DF-011 2026-10-24 ("Accept the reds", `docs/DECISIONS.md`). P2 has landed. Was: NOT MET, Wolf kaziho 2026-10-24 6f00555: `Expected 'brennic-c1 length 38.32s in 45-80' to be true.`, `Expected 'length 23.12s in 45-80' to be true.` (AirSeated), `Expected 'the script did not lose bout 3' to be false.` (FullSeated): explained only by the open DF-011. Met: `pin OK`, `conformance OK: 45 vectors`, `Result: Succeeded`, Conformance 45/45, Wave1Seated and CreaturesSeated green, goldens re-recorded per DF. Done when (all on Wolf, quoted in `runs/T39/REPORT.md` and summarised in the card's status line):
  `node apps/vr/tools/check-pin.mjs` prints `pin OK` and `conformance OK: 45 vectors`; `build.ps1` succeeds; one
  `MageArenaDesign` run on the final overlay where each of the five known reds is green or explained by an accepted DF;
  `MageArena.Kernel.Conformance` 45/45. If P1 landed, the overlay goldens are re-recorded per DF.
- Moves K3 rows 10 and 11 (`docs/research/KPI-2026-10-21.md` K3). Unblocks T41 and the new baseline for T24/T25/T27/T28
  ("If T39 has landed, use its recorded baseline").
- After T39: update `CLAUDE.md`'s known-failure list to the new reds (if any), in the same commit as the card's status.

### P3. T37: shared part materials, D-G4 lever B (soft: Mon 26 Oct; effort 1 day)

- Card: `apps/vr/tasks/T37-shared-part-materials.md`. Way (b), the per-colour cache, unless step 1 finds a per-frame
  colour that cannot be quantised; then stop and report (the card's "The choice this card makes").
- Depends on: P0. Does not share files with T39 except tests; run it after T39 so the design baseline is T39's.
- [x] Wolf kaziho 2026-10-24 f851101 16 casts re-verified (`:1151` ... `:2100`), no per-frame colour that cannot be quantised (three fades: stones, crowd band, Wardstones), so way (b) holds. `runs/T37/recolour-sites.md` written; the 16-cast count re-verified.
- [x] Wolf kaziho 2026-10-24 4352581 `MageArena.` 252/252 with the 3 SharedMaterials tests (red first: `Expected 'part B keeps its colour after A is recoloured (got (R=0.900000,G=0.260000,B=0.020000,A=1.000000))' to be true.`); `MageArenaDesign.` the P2 three reds with identical messages, no other; leverb1-3 `MAGEVR_BUDGET_DONE` x3, draw calls max 73 / 75 / 73 (median 70), live particles max 1,468, GPU emitters 1; `docs/research/BUDGET-DG4-2026-10.md` section 11. Done when: `MageArena.` green including `MageArena.Presentation.SharedMaterials.SameColourSameMaterial`,
  `.RecolourDoesNotLeak` (red line quoted) and `.Bounded`; `MageArenaDesign.` unchanged against the P2 result;
  `powershell -NoProfile -File apps/vr/tools/capture-budget.ps1 -Runs 3 -Prefix leverb` prints `MAGEVR_BUDGET_DONE` three
  times with draw calls max <=150 and particle rows still passing (<=2,000, <=1 GPU emitter); section 11 "Lever B
  shipped" appended to `docs/research/BUDGET-DG4-2026-10.md`. Never set the three config lines the card forbids.
- [x] Wolf kaziho 2026-10-24 8591e6c T46, the instrument this row is read on: the D-G4 capture now draws through the live `ASessionPresentation` (scene parity, `MAGEVR_BUDGET_FAIL scene parity` seen on the pre-fix hound colour). parity1-3 `MAGEVR_BUDGET_DONE` x3, `MAGEVR_BUDGET_PARITY phase=end ok=1`, draw calls median 100 / 101 / 101, max 104 / 104 / 103: still <=150, but up from the proxy's 73-75; GPU-culled all-pass triangles max 423,826 (BasePass max 163,280); `MageArena.` 256/256; `MageArenaDesign.` the same three DF-011 reds; `docs/research/BUDGET-DG4-2026-10.md` section 12.
- [x] Wolf kaziho 2026-10-24 57b5e52 the bead fix, the row restated: spear rings no longer draw 20 stray white bead cubes each (`MageArena.Presentation.Beads.SpearRingHasNone` and `.RingReusedFromMagicHasNone`, red first: `Expected 'frame 0: no bead of the spear ring is visible' to be 0, but it was 20.`); `MageArena.` 258/258; parity1-3 `MAGEVR_BUDGET_DONE` x3, `MAGEVR_BUDGET_PARITY phase=steady ok=1 planned=210 parts/11 tuples drawn=385 parts/23 tuples` (885 before), draw calls median 100 / 100 / 99, max 104 x3: still <=150; GPU-culled BasePass max 139,280 (163,280 before), all-pass max 376,518 (423,826 before), still over 350k on that reading - which reading is the verdict stays OWNER; `docs/research/BUDGET-DG4-2026-10.md` section 13. Ruled 2026-10-24 (owner: "BasePass", `docs/DECISIONS.md`): BasePass max 139,280 passes the 350k row; the all-pass figure stays a logged number, not a gate.
- Moves K3 row 4 (today: fail, median 194, max 201, `BUDGET-DG4-2026-10.md` 10.2-10.3) and re-checks rows 5-7.

### P4. T16: style-pick capture, then the owner picks (soft: Tue 27 Oct capture, owner pick Wed 28 Oct; effort 1-1.5 days)

- Card: `apps/vr/tasks/T16-style-pick-replay-capture.md`, including its "Retry 1" list (the 2026-10-04 capture never
  advanced the sim, every run hit the 12-minute timeout, and the skins only changed the floor tint).
- Depends on: P3 (the skins recolour through the shared cache; a style colour write must be a material swap, T37
  deliverable 3).
- [x] Wolf kaziho 2026-10-24 122e480 (page 3f6c5bd) `capture-style-pick: ok`, exit 0; `MAGEVR_CAPTURE_DONE shots=1870` x4;
  events-greybox/13/25/30.jsonl md5 `7b0715c871ac3399504c4f7a744896d4` x4; `MageArena.Replay.Deterministic`
  `Result={Success}` (`MageArena.` 273/273); `-Styles 99` exits 1 on `FAIL events-99.jsonl is missing`.
  `MageArena.Replay.Deterministic` green; `capture-style-pick.ps1` writes 4 MP4s, 16 stills and 4 event logs, exits
  non-zero if any log is missing or differs, and the 4 logs are identical (md5 of each in the report).
- [x] Wolf kaziho 2026-10-25 e9d8abc UE 5.8.3 CL 58210709 re-run on the post-T38 tree: `capture-style-pick: ok`, exit 0; `MAGEVR_CAPTURE_DONE shots=1870` x4; events-greybox/13/25/30.jsonl md5 `7b0715c871ac3399504c4f7a744896d4` x4 (unchanged from 2026-10-24); 4 MP4s + 16 stills staged outside the repo, `apps/vr/art/style-pick/` restored to HEAD (`docs/research/GATE-CAPTURES-2026-10-25.md`).
- [x] Wolf kaziho 2026-10-25 UE 5.8.3 CL 58210709, T64 (9d75da0): `powershell -NoProfile -ExecutionPolicy Bypass -File apps/vr/tools/capture-style-pick.ps1` on the blank-card fix: `capture-style-pick: ok`, exit 0; `MAGEVR_CAPTURE_DONE shots=1870` x4; events md5 `7b0715c871ac3399504c4f7a744896d4` x4; `MageArena.` 332/332 green; still 4 has no black rectangle (`apps/vr/tasks/T64-death-frame-blank-card.md`).
- [x] Wolf kaziho 2026-10-25 UE 5.8.3 CL 58210709, T22 seam (Athena ruling, 9535620): the crack seam is parented to the wrist cuff and hidden without a shown cuff; test `MageArena.Presentation.CrackSeam.OnCuff` (red on the old tree: seam 1 52.0 cm from the cuff, 2 seams shown; green after); `MageArena.` 333/333 green (332 + the new test); `capture-style-pick: ok`, exit 0; `MAGEVR_CAPTURE_DONE shots=1870` x4; events md5 `7b0715c871ac3399504c4f7a744896d4` x4 (unchanged from T64); death-frame stills before/after in `mage-art-staging/crack-seam-cuff/`: the left seam stays on the cuff, no seam floats on the right.
- [x] Wolf kaziho 2026-10-24 3f6c5bd 4 MP4s + 16 PNGs in `apps/vr/art/style-pick/`, all 20 LFS pointers under the
  existing repo-wide `*.mp4`/`*.png` rules (`git check-attr`: `filter: lfs`; `.gitattributes` unchanged); linked from
  `apps/vr/art/index.html`. `apps/vr/art/style-pick/index.html` with the copied videos and stills (LFS rules checked in `.gitattributes`).
- [x] **OWNER** ANSWERED 2026-10-24, ruling 1 (`docs/DECISIONS.md`, "P4 style: no single style is locked"): no single style is locked; greybox stays the base; the direction is visual quality toward the art concept; 13/25/30 is a combinable pool, not a pick. Original text: picks 13, 25 or 30 from the page. Record the pick in `docs/DECISIONS.md` (the owner's words) before any
  style-dependent work. Until then, T38 uses the greybox look (T38 "The owner decision").
- Moves K3 row 1 (and unblocks row 3, the P15 asset ceilings, which belong to pof, `docs/PROJECT-PLAN.md` section 8 P15;
  no local P15 tool exists in `apps/vr/tools/` **[verify on Wolf]** whether pof can run it here).
- T35 waits for T16 (`apps/vr/tasks/T35-input-clock.md`, "Read first").

### P5. [DEFERRED 2026-10-24, ruling 2, `docs/DECISIONS.md` "P5/P9 deferred"] First APK and the Quest 3 simulator boot on 5.8.3 (soft: Wed 28 Oct; effort 0.5-1.5 days, unknowns likely)

- Status: deferred, ruling 2 ("No need to resolve headset build in this stage").
- Depends on: B3 (**OWNER**: Android component), B5, B7. Can run between other phases on a clean master; it only needs
  one Unreal process at a time.
- [ ] **Simulator boot on 5.8.3** (K3 row 14, K5 step 4; not re-proven since 5.8.2, `docs/XR-TOOLCHAIN.md:197`):
  ```powershell
  $env:XR_RUNTIME_JSON = "$env:LOCALAPPDATA\MetaXRSimulatorExtract\PFiles\MetaXRSimulator\v207.0\meta_openxr_simulator.json"
  & "C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "$PWD\apps\vr\Game\MageArenaVR.uproject" -game -unattended -nosplash -nosound -RenderOffScreen -log -abslog="$PWD\runs\wolf\sim-boot-583.log" -stdout -FullStdOutLogOutput -ExecCmds=Quit
  Select-String -Path runs\wolf\sim-boot-583.log -Pattern "Initialized OpenXR on Meta XR Simulator runtime version 207","MAGEVR_BOOT_OK"
  # expected: both lines (as on 5.8.2, 2026-10-03)
  ```
  Then append a dated "5.8.3 simulator boot" note to `docs/XR-TOOLCHAIN.md` with the two lines.
- [ ] **First APK** (K3 row 13, K5 step 3; never produced, `docs/XR-TOOLCHAIN.md:9`):
  ```powershell
  node apps/vr/tools/check-staged-clips.mjs                    # expected: staged clips OK: 111 files, 25776936 bytes (at e1323e9)
  powershell -NoProfile -File apps/vr/tools/package-android.ps1
  # expected: MARKER_ARM64=yes, MARKER_HAND_PERMISSION=yes, MARKER_HAND_FEATURE=yes, MARKER_TARGET_SDK_34=yes,
  #           MARKER_SUPPORTED_DEVICES=yes value=quest|quest3|quest3s, MARKER_RESULT=PASS; runs/T05/apk-markers.txt written
  ```
  Known fallbacks, in this order, each only after the failure it answers is seen: JDK 21 if Gradle fails on a Java or
  class-file version (F11); build-tools 36.0.0 if the log asks for it; `bBuildForES31=True` for a renderer or SDK-level
  failure (`docs/XR-TOOLCHAIN.md` "Packaging"). Do not raise `TargetSDKVersion` above 34. If the Android C++ compile fails,
  check the 9 clang-only sites of `docs/research/ARCHITECTURE-REVIEW-2026-10-21.md` section 2 "P4 remainder" first.
  Record the result in `docs/XR-TOOLCHAIN.md` (a dated section) with the marker lines.
- [ ] K5 step 5 (the packaged build under the Quest 3 profile) has no script; **[verify on Wolf]** how to run the APK
  build against the simulator, or mark it as a V1 device step.

### P6. T38: the first animated opponent, Brennic (soft: Wed 28 - Fri 30 Oct; effort 1.5-2 days after the asset lands)

- Asset source moved 2026-10-24 (ruling 3, `docs/DECISIONS.md` "Models are built on this machine"; `docs/concepts/VISUAL-PLAN.md` (d)): this machine (RTX 4090), with local TRELLIS.2 under setup by run b83e357f, replaces the Lynx hand-off as the mesh source. Leonardo supplies the multi-view master (visual approvals of 2026-10-24, answer 3).

- Card: `apps/vr/tasks/T38-first-animated-opponent.md`. Source: Claude + owner authoring; the Lynx TRELLIS and Blender
  rigging pipeline may produce the mesh (DECISIONS 2026-10-20 "T38 asset source").
- Depends on: P3 (the figure's materials follow the shared-material rule); the Lynx hand-off (section e, H1). The R0
  checkpoint (Fri 23 Oct, `docs/PROJECT-PLAN.md` R0) cannot be held with an animated opponent; this is flagged as an
  owner decision (section g).
- [ ] Wolf checks before import, from the card: one skeleton and one skinned mesh; exactly the four clips `Idle`, `Cast`,
  `Hit`, `Death`, none empty; LOD0 triangles recorded; one flat colour per material; origin at the feet, +X forward, unit
  scale matching `FBody`; `SOURCE.md` with tool versions and prompts. A failed check goes back to Lynx as an exact defect;
  Wolf does not fix the mesh in the editor.
- [ ] Import to `apps/vr/Game/Content/Characters/Brennic/` (LFS) with `A_Brennic_Idle`, `_Cast`, `_Hit`, `_Death`.
- [x] Wolf kaziho 2026-10-25 a918062 UE 5.8.3 CL 58210709 (desktop proxy, not Quest truth) `MageArena.` 330/330 incl. both
  RivalFigure tests (red first); `MageArenaDesign.` 5/7, only DF-011's two; `capture-rival.ps1` staged all four clips under
  the DINOv3 hold (`mage-art-staging/t38-rival-2026-10-25/runs/T38/`; seed 1 for Idle/Cast/Hit, seed 2 for Death, since
  Brennic wins seed 1); `-Prefix rival` x3: draw calls 120 / 124 max, BasePass max 194,992, particles max 1,468.
  docs/research/T38-RIVAL-2026-10-25.md. Done when: `MageArena.` green including `MageArena.Presentation.RivalFigure.ClipFollowsEvents` and
  `.FallsBackToGreybox`; `capture-rival.ps1` (new) writes `runs/T38/` with all four clips visible;
  `capture-budget.ps1 -Runs 3 -Prefix rival` keeps the triangle row <=350k in all three.
- [ ] **OWNER:** judges the capture (felt; K3 row 2 and the R0 verdict). The card cannot pass that part.

### P7. Lane B and audio: T25, T40, T24, T27, T28 (soft: Thu 29 Oct - Tue 3 Nov; effort 0.5-1 day each)

Run in this order; skip any card whose blocker is still open and come back to it. Each one baselines against P2's
result, and each passes `format-check.mjs` and `unused-members.mjs` (section d).

- [x] Wolf kaziho 2026-10-24 b13cbb7 UE 5.8.3 CL 58210709 `MageArena.` 282/282 (`MageArena.Fist` 9/9, red first: `Expected 'Brennic falls: missio' to be "missio", but it was "intermission".`); `MageArenaDesign.` 5/7, only DF-011's two; flags `arena.tiro.1.w<n>.finish` and `arena.missio.total`. **T25** `apps/vr/tasks/T25-closed-fist-pose-and-spare-or-finish.md` (no owner blocker). Done when `MageArena.Fist.*`
  green (red line quoted first), `MageArena.` green, `MageArenaDesign.` no new red; flags `arena.tiro.1.w<n>.finish` and
  `arena.missio.total` (or the campaign doc's keys).
- [ ] DEFERRED 2026-10-24, ruling 6 (audio delayed; no audio spend). **T40** `apps/vr/tasks/T40-two-music-loops.md`. **OWNER:** the source of the two loops; the owner or the App Master
  (on the owner's go, generation count 2, ElevenLabs) commits them to `apps/vr/art/audio/music/` with `SOURCE.md`. The
  builder never reads `ELEVENLABS_API_KEY`. Done when `import-sfx.ps1 -Run T40` imports both and a second run writes
  nothing, `MageArena.Audio.*` green including Music, and `capture-colour-audio.ps1 -Run T40 -SkipStills` prints
  `MAGEVR_CAPTURE_DONE` with peaks above -60 dBFS. Moves K3 row 9.
- [ ] **T24** `apps/vr/tasks/T24-mudra-seal-detector.md`. **OWNER (C1):** the seal shape, Claude + owner
  (`docs/design/V2-ROADMAP.md` lane B row 8). Start only when C1 is committed. Done when `MageArena.Mudra.*` green (red line
  quoted first; seal at 1.4 s casts, at 1.6 s does not), `MageArena.` green, no new design red.
- [ ] DEFERRED 2026-10-24, ruling 6 (the audio half: voice and chorus generation; no audio spend). **Non-audio half done on
  Wolf 2026-10-25, UE 5.8.3 CL 58210709, `f94d1f7` / `0a58646` / `417477e` / `84a73c9` / `87448b2`:**
  - `Result: Succeeded`.
  - day.json `ritualLineS [12, 14, 12]` and `ritualTimeoutS 45`; the C2 lines and cue-id markers.
  - The voice importer: `missing=27 failed=0`, no WAV.
  - The wiring is unproven end to end, because no asset exists.
  - `MageArena.Voice` 4/4. It was red first: `Expected 'BRE-02 words matches C2's count' to be 6, but it was 5.`
  - `MageArena.` 325/325, `EXIT CODE: 0`.
  - `MageArenaDesign.` 5/7, only DF-011's two.
  - D-G4 max 126 (+0).
  - `dayWithTeachS=212.31` (rushed) and `dayNaturalS=305.72` (natural). Both stop at the bout-3 loss.
  - The intermission shift of later bouts was `DF-013`: RESOLVED 2026-10-25 by owner ruling (a), the kernel is frozen in
    intermissions (T66, 4ac011d). FullSeated now reads `dayWithTeachS=225.83`, `dayNaturalS=311.83`, bout 3 lost at 42.50 s.
  
  What stays open is the generation of the 14 cues and the capture line below.
  **T27** `apps/vr/tasks/T27-voice-and-chorus.md`. **OWNER (C2) - now on the critical path, see H3:** the story
  bible and cue sheet (roadmap row 12),
  and the owner's go for generation with the count stated on the card. Without committed WAVs, build the importer,
  wiring and tests against the manifest and report the wiring as unproven end to end. Done when `MageArena.Voice.*` and
  `MageArena.` green and `capture-colour-audio.ps1 -Run T27 -SkipStills` prints `MAGEVR_CAPTURE_DONE`.
- [ ] UNBLOCKED 2026-10-24 for its non-audio parts (ruling 5, C2 D1-D4 approved; its audio stays deferred, ruling 6). **T28** `apps/vr/tasks/T28-intermission-scenes.md`. **OWNER (C2) - now on the critical path, see H3.** Done when
  `MageArena.Intermission.*` and `MageArena.` green, and `MageArenaDesign.` shows no new red with the
  `Session.FullSeated` time **reported** against 7-10 min (after T47 it is a logged measurement, not a gate: DF-010
  ruled E on 2026-10-21).
- **T27 and T28 are together the only route to the 7-10 min day.** DF-010 was ruled **E + B** on 2026-10-21: the owner
  keeps the target and buys the minutes with content, because the pinned bands cap four bouts at 2.4-3.9 min of
  fighting (`docs/campaign/04-vr-campaign-design.md:137`) and those bands are a signed ruling
  (`docs/DECISIONS.md:308-309`). With the prologue tableau (<= 60 s) and a victory card, these two cards carry every
  minute the day is missing. If C2 does not land, the day stays at about 3 min and that is the submission.

### P8. [T41 sessions DEFERRED 2026-10-24, ruling 4, `docs/DECISIONS.md` "T41 cold-tester sessions deferred"; the protocol stays] T41 cold testers and the desktop core gate record (soft: Fri 30 - Sat 31 Oct; effort 0.5 day Wolf, plus owner hosting)

- Card: `apps/vr/tasks/T41-cold-desktop-playtests.md`. Depends on: P2, P4, P6 (the card's status line). The protocol
  `docs/research/PLAYTEST-PROTOCOL.md` can be written on any host first.
- [ ] Wolf stages the desktop build (a Win64 stage after `check-staged-clips.mjs` prints OK, `CLAUDE.md` "Build") and
  records the commit and stage path in the protocol before the first session. **[verify on Wolf]** the staging command;
  no script for a Win64 stage exists in `apps/vr/tools/`.
- [ ] **OWNER:** finds two cold testers and hosts. The App Master or the owner writes
  `docs/research/PLAYTEST-2026-10-DD.md` (or `-11-DD`) with the K3 row 12 verdict. No session is invented.
- [ ] **Gate record:** a dated `docs/research/` record with the 15 rows of the desktop core gate (`docs/PROJECT-PLAN.md:47`,
  the K3 table in `docs/research/KPI-2026-10-20.md`), each with its Wolf evidence or "not met". Row 15 is the owner's
  (felt). The gate date is soft; the record is what releases the device decision (section g).

### P9. [DEFERRED 2026-10-24, ruling 2, `docs/DECISIONS.md` "P5/P9 deferred"] Device: V1 bring-up, V2 hands, V3 beta and RC (soft: V1 Sun 1 - Wed 4 Nov, V2 Thu 5 - Sun 8 Nov, V3 Mon 9 - Fri 13 Nov)

- Depends on: P5 (an APK with `MARKER_RESULT=PASS`) and **OWNER:** the device order (`docs/PROJECT-PLAN.md` section 12 Q2;
  R14) and the dashboard app with the "Competition" channel (R12). The plan's gates and owner hours are in
  `docs/PROJECT-PLAN.md` section 5 rows V1-V3; this file does not repeat them.
- [ ] V1: `adb install` and launch; the logcat boot marker `MAGEVR_BOOT_OK` within 60 s; joints at >=60 Hz for 5 min;
  cold start <=15 s; first on-device perf capture (`docs/PROJECT-PLAN.md` section 1 gate table). **[verify on Wolf]** adb
  on PATH from the SDK `platform-tools`.
- [ ] V1 also: wire the OpenXR session-state pause (`apps/vr/tasks/BACKLOG.md`, "VR sprint (V1/V3)").
- [ ] V2: the recognizer and ward gates on real hands (plan section 1 V2 row); this needs the owner in the headset.
- [ ] V3: beta Wed 11 Nov and RC Fri 13 Nov on the "Competition" channel; 3 cold device playtests (OWNER).

### P10. Submission package (soft: Sat 14 - Tue 17 Nov; hard: Wed 18 Nov 2026, 12:00 PT)

- Index: `docs/submission/README.md` (10 items, 0 final at 2026-10-20). Item 1 (Q9) is closed by DECISIONS 2026-10-21: it is
  not posted (`docs/submission/ORGANIZER-QUESTION-Q9.md` status). The README row still reads "not sent"; the App Master
  aligns it.
- [ ] Store-upload freeze Mon 16 Nov (soft): the last build uploaded (plan section 5 "Submit" row).
- [ ] Video captured (item 7) from device or, as the fallback the rules accept, the XR Simulator (plan section 11).
  Wolf produces the captures; the owner records and edits (`docs/submission/VIDEO-BEAT-SHEET.md`).
- [ ] APK on the "Competition" channel with the invite URL tested from a second account (item 8).
- [ ] The text items (2-6, 9, 10) are finalised by the owner on any host.

### P11. Only if time is left: the determinism package T33-T36

- Cards: `apps/vr/tasks/T33-kernel-data-injection.md`, `T34-portable-kernel-math.md`, `T35-input-clock.md`,
  `T36-session-tick-and-journal.md` (owner-accepted MOONSHOT M6, M7, M3, M4). Run order: T39 before T33
  (ARCHITECTURE-REVIEW-2026-10-21 A5), T16 before T35; T33 -> T34, T35 -> T36. Effort 1.5-3 days each.
- Their known-red lists name three tests; `CLAUDE.md` names five (A5). Their PinDiff census is moot under the frozen
  baseline (A5); the re-scope is an owner ask (section g). None of them moves a K3 row. They are first in the cut order.

### P12. Cards from the 2026-10-21 context sweep, and where each slots

Written on Lynx by a `/perfect` sweep over 15 contexts (`docs/research/SWEEP-2026-10-21.md`; its S/M remainder is
backlog, not phases). Nothing below was built or run in Unreal, so every one is **[verify on Wolf]** throughout.
These do not get their own slot in the critical path - **they attach to the phases already there**, in this order:

| Card | Slots | Why there |
|---|---|---|
| **T51** ue-arm64 host probe | section b, read-only, **any time Wolf is up** | 10 minutes, one grep of the UBT Windows source, no install. Settles whether hosting Unreal on Lynx is worth a time-boxed attempt. Blocks nothing. |
| **T47** split design gates | **new, between P2 (T39) and P3 (T37)**, 0.5-1 day | **Hard precondition: P2 must land in full first** - T39 and T47 both rewrite `SessionTests.cpp`, and some invariants only go green after T39. Running them concurrently needs a hand-merge. One assertion stays red until DF-010 is ruled (section g). **Done on Wolf kaziho 2026-10-24, UE 5.8.3 CL 58210709, 69b349a / 086854e / f64c685** (precondition: P2 landed by owner ruling DF-011 2026-10-24): `Result: Succeeded`; `MageArena.Invariants` 5/5 (played sigil/bolt/blink/ward clips reach `kernel cast` / `kernel roll actor=` / the kernel ward; the no-sigil and no-bolt mutations turn them red); `MageArena.` 266/266 (Conformance 45/45); `MageArenaDesign.` 5/7, FireSeated (brennic-c15 reached the last phase, 1 of 2) and FullSeated (lost bout 3) red as DF-011's accepted reds, AirSeated green after its band left `bPass`; under `-MageArenaMeasureSelfTest` all 36 `MAGEVR_MEASURE` lines read OUTSIDE and the verdicts are identical. The day-length assertion left `bPass` per DF-010 E. |
| **T44** threat-family hard-fail | **before P11's T33, or rebased onto it** | Both touch `KernelData*.cpp`. Its constraint is that the 45 vectors stay byte-identical, so `check-pin.mjs` is the detector. **Done on Wolf kaziho 2026-10-24, UE 5.8.3 CL 58210709, before T33, `a982baf` ... `e81a946` (tested tree `e81a946`):** `Result: Succeeded`; `check-pin.mjs` `pin OK: 9 files @ baeac66` / `conformance OK: 45 vectors`, `apps/vr/data` untouched, no pinned cell rejected; `MageArena.` 289/289 (284 + UnknownFamilyFailsLoad, GarbageNumberFailsLoad, AbsorbRulesRejectBadNotAgainst, PinnedDataStillLoads, Threats.UnknownFamilyIsNotAbsorbable; red first, e.g. `Expected 'water blockable with a trailing space: the load fails' to be false.`; Conformance 45/45); `MageArenaDesign.` 5/7, only DF-011's FireSeated and FullSeated, MAGEVR_MEASURE lines identical. One `EThreatFamily` / `ParseFamily` / `ParseBlockable` in `KernelDataLoad.h`; `KindFromFamily` fails safe to Unblockable with a logged error; the notAgainst rule now lives once, in `FAbsorbRules::LoadFromText`. For T33: `FAbsorbResolver::Init` still reads the pinned absorb block itself. |
| **T46** budget scene parity | **with P3 (T37) or straight after** | T37 is lever B; T46 is the **instrument** that measures it. Without T46, a lever-B pass can be declared green on a proxy scene that is not the shipping one - and `plausibilityOk` cannot fail today. **Run T46 before T49**: both edit `BudgetStressDriver.cpp`. Done on Wolf kaziho 2026-10-24 at `8591e6c`, straight after P3 (P3 line above). |
| **T49** threat-colour single-source | DEFERRED 2026-10-24, ruling 7 (colour sign-off waits until the visual direction settles). Original slot: **after T46, before P4 (T16)** | So the style-pick frames the owner judges use the corrected HP bar. Re-running `create-budget-fx.ps1 -Force` is one of its deliverables. |
| **T45** capture evidence integrity | **before P4 (T16)**; its **audio half before P7 (T40)** | P4's checklist currently expects `capture-style-pick.ps1` to work as-is, and it does not: the driver writes no DONE/FAIL marker and the launcher substitutes the run's first frame for a missing one. The audio half gates T40's music evidence. **Done on Wolf kaziho 2026-10-24, UE 5.8.3 CL 58210709, `9e42cbe` / `1612780` / `191cad0` / `f92230e` / `197412c`:** `Result: Succeeded`; `MageArena.` 272/272 with the six `MageArena.Capture.*` (FailsWithoutSession red first: `Expected 'fail markers logged by a driver with no session' to be 1, but it was 0.`); `MageArenaDesign.` 5/7, only DF-011's FireSeated and FullSeated; style-pick greybox launch `MAGEVR_CAPTURE_DONE shots=1870`, frames now show the synced duel (before: the empty arena); negative proofs `MAGEVR_CAPTURE_FAIL session did not start bout=99`, `MAGEVR_CAPTURE_FAIL short run seq=180 stage=active`, and the audio launcher fails a header-only and a 20.8 s WAV. Finding for P4: the event log for the same seed and script changed (`1572878c...` -> `7b0715c8...`; the old driver stepped the hands twice a frame) and is now identical across runs and styles. |
| **T43** ward aim side-channel | **before P9 (V1 device bring-up)**, after P2/P3 | The desktop arc gate cannot fail today and the VR path never sets facing, so it must be real before hands reach a headset. Its head/seat facing is the transform MOONSHOT **M2** will own - if no usable facing exists yet, the card says stop and report rather than half-build M2. **Done on Wolf kaziho 2026-10-24, UE 5.8.3 CL 58210709, `110f5f9` / `20fdeea` / `2324143`:** `Result: Succeeded`; `MageArena.Ward` 14/14 with ArcGateRejectsOffAxis, ArcGateAcceptsOnAxis and ReferenceFacingHasAProducer (red first on master's behaviour: `Expected 'raises from a palm aimed 40 deg off the seat facing' to be 0, but it was 1.`); `MageArena.` 284/284 (Conformance 45/45); `MageArenaDesign.` 5/7, only DF-011's FireSeated and FullSeated, MAGEVR_MEASURE lines identical. The reference is the head in the seat frame, written by the session (`ApplyWardReference`); SetThreatFacing deleted with ElevatedThreatFacing. For P9: on a headset the head source is still the control rotation until M2. |
| **T50** seated-momentum guard | any time; **design half blocked** on the owner's Footing reading (section g) | Guard only. Harmless today because the player has no Air school. **Guard half done on Wolf kaziho 2026-10-24, UE 5.8.3 CL 58210709, `dc81bae` ... `bb53109` (tested tree `bb53109`):** `Result: Succeeded`; `check-pin.mjs` `pin OK: 9 files @ baeac66` / `conformance OK: 45 vectors`, pinned and conformance paths untouched; `MageArena.Air.` 16/16 and `MageArena.` 292/292 (289 + PlayerAirWithoutSeatedReadingFails, SeatedPlayerMomentumOnlyDecays, RivalMomentumUnchanged; Conformance 45/45, Overlay.Golden green); red first: `Expected 'a player-side actor with no declared reading is refused' to be false.`; `MageArenaDesign.` 5/7, only FireSeated and FullSeated, 36 MAGEVR_MEASURE lines identical to baseline (Duel.AirSeated lio-c15 lengthS=23.12 won=1.00 before and after). Option (b): `EAirSeatedReading` on `FAirState`, default `None`; `GrantAirSchool` is the one grant site. Design half (the reading) stays open. |
| **T48** earth school | **not scheduled - see section f** | The card itself recommends cutting it past 18 Nov. |
| **T55** no debug text in views | **before any further capture; Unreal lane** | Visual approvals 2026-10-24, answer 6: the overlay text must not appear in any captured or shipped view. Done in 54bea62. |
| **T56** dusk lighting | Unreal lane, after T55 | Answer 6; VISUAL-PLAN (b) shortfall 3. |
| **T57** crowd visibility | Unreal lane, after T56 | Answer 6; VISUAL-PLAN (b) shortfall 3. |
| **T58** spell VFX toward the key art | Unreal lane, after T57 | Answer 6; VISUAL-PLAN (b) shortfall 3. |
| **T59** player hand mesh | Unreal lane, after T58 | Answer 6; VISUAL-PLAN (b) shortfall 3. |
| **T65** crowd tint pass | Unreal lane, after T64 | Athena lifted her T57 deferral 2026-10-25 (DECISIONS). Built in `apps/vr/tasks/T65-crowd-tint.md`; report `docs/research/CROWD-TINT-T65-2026-10-25.md`; D-G4 max 127 (base 127), open. |

## d. Gates before every merge on Wolf

Run on the branch or worktree before merging to master. Record each result with the host name Wolf (section a, step 3):
`Wolf <hostname> <date> <commit> UE 5.8.3 CL 58210709`. Lynx outputs from 2026-10-21 are given as the expected shape.

Node gates (the CI `quality.yml` set plus the repo's own checks):

```powershell
node apps/vr/tools/check-pin.mjs                 # expected: pin OK: 9 files @ baeac66 / conformance OK: 45 vectors
node apps/vr/tools/check-pin.mjs --self-test     # expected: corruption detected: ...combat.json / conformance corruption detected: bolt-hit.json
node --test tools/agy-guard.test.mjs             # expected: pass 30, fail 0
node apps/vr/tools/lint/unused-members.mjs       # expected: unused-members OK: <n> private/protected members in <m> classes
node apps/vr/tools/lint/unused-members.mjs --self-test
node apps/vr/tools/lint/format-check.mjs --base <merge base>   # expected: format-check OK (changed C++ lines only)
node tools/check-action-pins.mjs                 # expected: actions pinned: 6 refs in 3 workflows
node tools/check-commits.mjs                     # expected: check-commits OK: <n> commit subject(s) since <base>
node apps/vr/tools/check-staged-clips.mjs        # expected: staged clips OK (required before any Win64 stage or APK)
```

UE gates. One Unreal build at a time; one `Automation RunTests` filter per editor process; read verdicts from
`-ReportExportPath` (T33-T36 "Constraints"):

```powershell
powershell -NoProfile -File apps/vr/tools/build.ps1      # expected: "Result: Succeeded" in the log, not only exit 0
$ed = "C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
$up = "$PWD\apps\vr\Game\MageArenaVR.uproject"
& $ed $up -ExecCmds="Automation RunTests MageArena.;Quit" -unattended -nullrhi -nosplash -log -ReportExportPath="$PWD\runs\wolf\<run>\MageArena"
& $ed $up -ExecCmds="Automation RunTests MageArenaDesign.;Quit" -unattended -nullrhi -nosplash -log -ReportExportPath="$PWD\runs\wolf\<run>\MageArenaDesign"
```

- `MageArena.`: all green. It contains `MageArena.Kernel.Conformance.*` (45/45, the baseline half of goal 3, DECISIONS
  2026-10-21) and `MageArena.Fire.Golden` (hashes unchanged), plus the card's own filter (for example `MageArena.Mudra`).
- `MageArenaDesign.`: no red beyond the current known list (five before T39, `CLAUDE.md`; T39's result after it).
- When a card touches kernel files, also run `MageArena.Kernel.Conformance` on its own after each kernel file (T33 rule).
- Regenerate the sigil corpus first in a fresh checkout or worktree (B12).
- A gate that could not run (a missing tool, a timeout) is reported as not evaluated, never as a pass.

## e. Hand-offs between Lynx and Wolf

Git is the channel for everything that is text or LFS. Both machines commit to master and push verified work to `origin master` on GitHub (owner, 2026-10-21; `CLAUDE.md` "Git"):
gates green, `git fetch` and rebase (or merge) first, never force-push. GitHub is the transport between the machines.

| # | What | Produced by | Lands at | Wolf picks it up by |
|---|---|---|---|---|
| H1 | T38 rigged mesh (FBX or GLB) with four clips `Idle`, `Cast`, `Hit`, `Death`, plus a `SOURCE.md` draft (tool versions, prompts, seed, date) | Lynx: TRELLIS and Blender rigging (PoF 3D rounds); Claude + owner author | `C:\Users\kazda\kiro\mage-arena-archive\T38\Brennic\` on Lynx, outside git (T38 "Hand-off") | Copy to the same path on Wolf (shared drive or copy, operator's choice, not decided). Wolf runs the T38 checks, imports to `Content/Characters/Brennic/` and commits only the import (LFS) |
| H2 | C1, the mudra seal shape | Claude + owner (on Lynx or anywhere) | a committed design doc (C1 states the path; roadmap lane B row 8) | `git pull`, then P7 T24. Its path is the first line of the T24 report |
| H3 | **RULED 2026-10-24 (ruling 5): D1-D4 approved as recommended, commit 19aa8ee; Wolf no longer waits on the owner for C2 (D4 audio production stays deferred, ruling 6).** **C2, story bible and slice script with the cue sheet - the critical path for the day's length. DRAFTED 2026-10-21** at `docs/campaign/05-c2-slice-story-bible-and-script.md`, with the authored data at `apps/vr/data/vr/story/slice.json`. DF-010 ruled E + B, so T27 and T28 are the only route to 7-10 min. **What Wolf still waits on is the owner's ruling of C2 decisions D1-D4, not the writing** - D4 (voice approval, recommended 14 cues) gates T27; D1-D3 gate both | Claude + owner | committed: the doc above plus `apps/vr/data/vr/story/slice.json` | `git pull`, then P7 T27 and T28 |
| H4 | Two music loops and their `SOURCE.md` | the owner, or the App Master on the owner's go (ElevenLabs, count 2) | `apps/vr/art/audio/music/` (LFS) | `git pull`, `git lfs pull`, then P7 T40 |
| H5 | Voice WAVs, `manifest.json`, `SOURCE.md` | the owner or the App Master after the owner's go | `apps/vr/art/audio/voice/` (LFS) | `git pull`, `git lfs pull`, then P7 T27 |
| H6 | Card amendments, new cards, DECISIONS entries, Node tooling | Lynx (App Master) | `apps/vr/tasks/`, `docs/`, `apps/vr/tools/*.mjs` | `git pull` before each phase; re-read the card and DECISIONS |
| H7 | UE results: card status lines, ticks in this file, committed records (`docs/research/WOLF-BASELINE-*`, BUDGET section 11, XR-TOOLCHAIN sections, PLAYTEST records), code | Wolf | master on Wolf | The App Master on Lynx reads them via git after they reach the shared remote; Lynx never re-states a UE result as its own |

Rules for the shared master:

- Lynx does not edit `apps/vr/Game/Source/**`, `apps/vr/Game/Content/**` or `apps/vr/data/vr/**` while a Wolf phase holds
  them. Wolf does not edit cards it is not running, except to set its own card's status line.
- Before a transfer, Wolf rebases its local commits onto the latest master it has received. Never force-push.
- Run reports (`runs/<ID>/REPORT.md`) are untracked. Their summary goes into the card's status line, so Lynx can read it.

## f. Risks to 18 Nov and a proposed cut order

Risks, highest first:

1. **No UE run since 2026-10-16** (`docs/research/KPI-2026-10-21.md` K4). Every K3 row except four waits on Wolf. P0 is the
   first fix.
2. **No APK ever** (`docs/XR-TOOLCHAIN.md:9`). The owner step B3 has been open since 2026-10-03; the first package will
   surface unknowns (JDK 22 against 21, NDK r28c, build-tools 36.0.0; STATE-EVAL section 6 risk 3). P5 runs early.
3. **R0, fun and animation** (`docs/PROJECT-PLAN.md` section 10 R0). The design suite is red in five places until T39;
   no skeletal mesh exists (T38 "Goal"). The Lynx asset hand-off H1 has no date.
4. **Device timing** (R14): the order follows the gate record, and the gate will not pass in full on 31 Oct (K3 4/15 on
   2026-10-21). Every device day lost comes out of V1-V3.
5. **Owner-blocked inputs:** C1, C2, the music source, the style pick, cold testers, the device, the dashboard channel.
6. **One Unreal host, one build at a time.** Wolf is a single queue; a long T39 delays everything after it.
7. **UE 5.8.4 by surprise** (F12; B2). A silent engine update breaks the plugin match (BuildId 55116800).
8. **Eligibility rests on the provisional acceptance of 2026-10-07** with no organizer answer, since Q9 is not posted
   (DECISIONS 2026-10-21). This is the owner's accepted position, not a task.

Proposed cut order if time runs short (a proposal; **the owner decides**, and the owner declined the plan's cuts of bout 2
and the second school on 2026-10-20, so those are not on this list):

0. **T48, the Earth school — never started rather than cut.** It is above P11 because nothing depends on it and
   its own card recommends not doing it before 18 Nov: there is no slot for Earth in the pinned Tiro tiers (two
   mage waves, both bound), the owner already chose Air over Earth on 2026-10-07, the owner's own date for all
   four schools playable is spring 2027 (`DECISIONS.md:323-326`), and a fourth `ChooseXSpell` would make
   MOONSHOT M10 strictly worse. The card exists and is ready if the owner overrides.
1. P11, the determinism package T33-T36 (moves no K3 row).
2. P1, the overlay goldens (accept the A1 risk; T39 then relies on the design suite alone).
3. T28 intermission scenes (the existing Bout 1 stone pick stays).
4. T27 voice and chorus (the SFX stay).
5. T25 fist and the missio choice (no choice; spare is the default the card already defines).
6. T24 mudra seal (tier IV releases as today; the video uses the Reflection finish, `docs/submission/VIDEO-BEAT-SHEET.md`).
7. From the plan's own R4 list, what the owner has not ruled out: Mirror IV, Lash IV, narrow-FOV mode.

Not on the list (the build cannot be submitted without them): T39, T37, T16 and the style pick, T38, the APK, the device
run, the submission package.

## g. Owner decisions still open

1. ~~**How commits travel between Wolf and Lynx.**~~ Decided 2026-10-21: both machines push verified work to `origin master`
   and merge when needed (`CLAUDE.md` "Git").
2. **B3 Android in the Epic launcher** and **B2 launcher auto-update off** (both open since 2026-10-03 / 2026-10-13).
3. **The UE 5.8.4 day** (F12, DECISIONS 2026-10-13): which day. Proposal: not before P5 has a 5.8.3 APK, so a failure is
   attributable. **[verify on Wolf]** whether 5.8.4 has been released.
4. **The style pick** after P4 (13, 25 or 30).
5. **T38:** how the H1 file crosses to Wolf, when it lands, and the look (greybox flat colour or the picked skin).
6. **The R0 checkpoint:** it was Fri 23 Oct and cannot be met with an animated opponent; when the owner judges it (after P6).
7. **C1** (the seal shape), Claude + owner - not written. **C2** (the story bible and slice script with the cue sheet),
   **drafted 2026-10-21** at `docs/campaign/05-c2-slice-story-bible-and-script.md`. It is the critical path: DF-010 was
   ruled E + B, which makes T27 and T28 the only arithmetic route to the 7-10 min day the owner kept. **What is open is
   now a ruling, not writing time** - C2's own section 11 lists it: D1 the voice of the game, D2 the 213 s time budget,
   D3 three intermission choices instead of the campaign doc's one, D4 voice approval and the cue count (recommended 14
   of 27). D5 cuts the Wardstones line; D7 reports that **CR-008's "four routes" are defined nowhere in the repo**, so
   that CR is either the owner's to define or should be retired.
8. **T40's music source**, and the go for generation (count 2); **T27's** generation go and count.
9. **Two cold desktop testers** for T41, and three cold device testers for V3 (plan section 12 Q6).
10. **The device order** without a full gate pass (STATE-EVAL section 6 risk 4; plan section 12 Q2), and the dashboard app
    and the "Competition" channel (R12).
11. **P1 overlay goldens:** write the card (A1) or skip it. **T33 re-scope** (A5: the PinDiff census is moot).
12. **The cut order** in section f, if time runs short.
14. **DF-010, the Tiro day length** (`docs/design-findings/DF-010-tiro-day-length.md`, opened 2026-10-21). The day
    measures **3.02 min** against the 7-10 min asserted at `Tests/SessionTests.cpp:792`, and that number has
    **neither a document nor a ruling** behind it: the test's own comment cites "Games day 6-10 min" while the
    constant enforces 7.0, and `docs/DECISIONS.md` carries no minute figure. The 3.02 min also came from a run
    that **stopped on a loss**, and the four-bout figure ran Bouts 3-4 as water proxies - **a day that wins all
    four real bouts has never been measured.** Options A-E in the finding; C (harder rivals) is costed as
    arithmetically insufficient. **Blocks one assertion in P12's T47.**
15. **Earth before 18 Nov** (`apps/vr/tasks/T48-earth-school.md`, written 2026-10-21 on the owner's instruction).
    The card recommends cutting it; the owner asked for the card, not the decision. Note the 2026-10-07 ruling
    (`DECISIONS.md:323-326`) already chose Air over Earth and dated all four schools playable to spring 2027.
13. **Submission text:** the launch date (Q5), the entrant and sponsor-funding confirmations (`docs/submission/README.md`
    "Owner decisions this package waits on" 2-5).
