# T02 - Hand clips: schema, synthetic generator, and key-driven replay into one hand source

Status: open
Max turns: 220

## Goal
In October nobody wears a headset, so every hand gesture must be playable from the keyboard as a **hand clip**: a
recorded or synthetic stream of hand-joint poses over time, fed into the SAME hand-input source the headset will fill
in November. This card builds the clip format, a generator for the first 27 synthetic clips, and the Unreal side that
plays a clip when a key is pressed. It does NOT build gesture recognition (sigil, ward, blink detectors) - later cards
consume the frames this card produces. Plan: `docs/DESKTOP-INPUT.md` (read it fully), plan section 4 gate D-G1.

## Read first
- `CLAUDE.md`, `docs/DECISIONS.md`, `docs/DESKTOP-INPUT.md`, `docs/ORCHESTRATION.md`
- `Game/MageArenaVR.uproject`, `Game/Source/MageArenaVR/*`, `tools/build.ps1`
- Unreal 5.8 headers for hand keypoints: `EHandKeypoint` (26 values, in `HeadMountedDisplayTypes.h` or the XR headers
  under `C:\Program Files\Epic Games\UE_5.8\Engine\Source\Runtime\HeadMountedDisplay\Public\`). Use that exact
  26-keypoint order so November's device data maps 1:1.

## Deliverables (exact paths)

### 1. The format - `docs/CLIP-SCHEMA.md`
JSON Lines, UTF-8, LF. Line 1 is a header object; every following line is one frame.
- Header: `{ "schema": "mage-arena/hand-clip@1", "name", "action", "variant": "normal|slow|sloppy", "hands": ["L","R"]
  subset, "hz": 72, "space": "seated-origin, metres, +X forward, +Y right, +Z up (Unreal axes, cm converted to m)",
  "keypoints": [ the 26 EHandKeypoint names in enum order ], "source": "synthetic|recorded|mouse", "seed": <int> }`
- Frame: `{ "t": <seconds from clip start, monotonic>, "hand": "L"|"R", "conf": 0..1, "pinch": 0..1,
  "joints": [[x,y,z,qx,qy,qz,qw] x 26] }` (one line per hand per frame).
- Write the rules a recorder in November must follow (same schema, `source: "recorded"`), and the validation rules.

### 2. Generator and validator - `tools/clipgen/` (Node, built-ins only)
- `tools/clipgen/generate.mjs` writes clips to `Game/Clips/<action>.<variant>.jsonl`. A simple procedural hand model
  (palm frame + finger chains with plausible bone lengths) is enough; the joints must move like a hand doing the action.
- The **9 actions** (from DESKTOP-INPUT.md): `sigil-line1`, `sigil-line2`, `sigil-line3` (right index fingertip draws
  a circle of ~20 cm diameter about 40 cm in front of the chest, then a distinct inner stroke per line: line1 a
  vertical bar, line2 a horizontal bar, line3 a diagonal; pinch thumb-index to mark pen-down during drawing, release
  at the end), `mudra` (both hands form a held two-hand pose for 0.6 s), `bolt` (quick right index flick forward),
  `ward-raise` (left palm rises from lap to facing forward at chest height in ~0.25 s, then holds 1.0 s),
  `blink-left`, `blink-right`, `blink-back` (right index flick toward a pad at roughly -60, +60 and 180 degrees around
  the seat).
- The **3 variants** each: `normal`; `slow` (1.6x duration); `sloppy` (seeded jitter of 4-8 mm on fingertips,
  10-20 % shape distortion, a small overshoot, occasional `conf` dips to 0.4). Deterministic: same seed, same bytes.
- `tools/clipgen/validate.mjs [dir]` checks every clip against CLIP-SCHEMA.md (header fields, 26 joints, monotonic
  `t`, unit quaternions within 1e-3, `conf`/`pinch` in range, `hz` spacing within 10 %), prints one line per clip and a
  summary, exits non-zero on any violation.

### 3. Unreal side - `Game/Source/MageArenaVR/Hands/`
- `HandFrame.h`: `FHandJointPose` (location in cm, rotation quat), `FHandFrame` (time, hand, confidence, pinch,
  26 joint poses).
- `IHandSource` (plain C++ interface): `bool GetLatest(EControllerHand Hand, FHandFrame& Out) const`, a frame-sequence
  counter, and a multicast delegate fired once per new frame. November adds a device source behind the same interface.
- `FHandClip` + loader: parse a clip file (JSONL) from `<ProjectDir>/Clips/` into memory, with clear errors.
- `UHandClipPlayer`: a hand source that plays a loaded clip in game time (sample by `t`, linear interpolation between
  frames, slerp for rotations), and can also step deterministically at a fixed dt for tests.
- `UHandInputSubsystem` (`UGameInstanceSubsystem`): owns the active hand source; `PlayQuickAction(FName Action,
  EClipVariant Variant)` loads and plays `<action>.<variant>.jsonl`; exposes the current frames; logs each started
  clip to `LogMageArena`.
- **Key bindings** through Enhanced Input, data-driven (an `InputMappingContext` created in C++ at startup is fine;
  no binary assets required): `1`,`2`,`3` -> sigil-line1/2/3, `4` -> mudra, `Q` -> bolt, `Space` -> ward-raise,
  `A`/`D`/`S` -> blink-left/right/back; holding `Shift` selects `sloppy`, `Ctrl` selects `slow`. The binding calls
  `PlayQuickAction` - the exact same function the tests call. `Esc` is reserved for pause (log only for now).
- A minimal `AMageArenaPlayerController` (or equivalent) that installs the mapping context, and project config so a
  play-in-editor session uses it. Greybox only: no art.

### 4. Headless tests - UE Automation tests under `Game/Source/MageArenaVR/Tests/`
- `MageArena.Clips.LoadAll`: every file in `Game/Clips/` loads; 26 joints; monotonic time.
- `MageArena.Clips.DeterministicReplay`: stepping a clip at fixed dt twice yields identical frame digests.
- `MageArena.Clips.QuickActionPath`: calling `PlayQuickAction` (the key path) and replaying the clip directly yield
  identical frame digests over the clip's duration.
- `Game/Clips/` must be included in packaging later: add it to `DirectoriesToAlwaysStageAsNonUFS` (or the 5.8
  equivalent) in `Game/Config/DefaultGame.ini`.

## Constraints
- Do not commit, push, or touch any repository other than this one. Do not call paid services. Do not edit
  `docs/DECISIONS.md`, `data/pinned/` or `data/PINNED.json`.
- No binary `.uasset`/`.umap` files are required for this card; prefer C++ and config. If you must create one, say why.
- Keep each C++ file focused (one type per file where reasonable). Use `LogMageArena` for logs.
- One Unreal build at a time; use `tools/build.ps1` (it passes `-WaitMutex`). Never clean engine caches.

## Acceptance (the orchestrator re-runs these; quote their real output in your report)
```
node tools/clipgen/generate.mjs && node tools/clipgen/validate.mjs Game/Clips
    # expected: 27 clips listed, 0 violations, exit 0; running generate twice gives identical bytes (show a hash)
powershell -NoProfile -File tools/build.ps1
    # expected: "Result: Succeeded"
"C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "C:\Users\kazda\kiro\mage-arena-vr\Game\MageArenaVR.uproject" -ExecCmds="Automation RunTests MageArena.Clips;Quit" -unattended -nullrhi -nosplash -log
    # expected: the three MageArena.Clips tests reported as Success (quote the result lines from the log)
```

## Report
Write `runs/T02/REPORT.md`: files created, each acceptance command with its real output (trim long logs to the
relevant lines), the clip list with durations, how the procedural hand works in two sentences, anything you could not
do and the exact error, and decisions you made that the card did not specify.
