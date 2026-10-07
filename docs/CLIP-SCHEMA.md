# Hand clip schema `mage-arena/hand-clip@1` and `@2`

A hand clip is a stream of hand-joint poses over time. The keyboard plays one into `IHandSource`.
November's headset recorder writes the same file (`source: "recorded"`) into that same source.
There is no second gesture path.

## File

- UTF-8, no BOM. Line endings are LF only. The file ends with one LF.
- JSON Lines. Line 1 is the header object. Every later line is one frame for one hand.
- No blank lines. No trailing commas. Objects are closed: unknown fields are invalid.
- On disk, under `apps/vr/Game/Clips/<action>.<variant>.jsonl`.
  `action` and `variant` in the header match that file name.
- Corpus, template, mouse and noise clips live in subfolders of `apps/vr/Game/Clips`
  (`corpus/`, `templates/`, `mouse/`, `noise/`, `impostors/`). A folder may hold many drawings of one
  class, so those files may be `<action>.<variant>.<id>.jsonl`. `<id>` is a seed or token
  (`[A-Za-z0-9_-]+`). The header `action` and `variant` are still the first two name parts.
  The 36 quick-action clips in `apps/vr/Game/Clips` itself keep the unsuffixed name.
- Positions are metres. The Unreal loader multiplies positions by 100 and stores centimetres.
  Quaternions are not scaled.

## Header

Key order is part of the synthetic bytes (recorders may use this same order).

| Field | Rule |
|---|---|
| `schema` | `mage-arena/hand-clip@1` or `mage-arena/hand-clip@2`. See "Schema @2". |
| `name` | Non-empty string. Synthetic clips use the action id. |
| `action` | Non-empty string. The twelve desktop actions are below. |
| `variant` | `normal`, `slow`, or `sloppy`. |
| `hands` | Non-empty subset of `["L","R"]`. `L` before `R` when both are present. No duplicates. |
| `hz` | `72`. 72 is the rate a clip is emitted at. The hands may update more slowly underneath (about 30 Hz at LOW, unconfirmed first-party, see F7 in `docs/research/STACK-OPPORTUNITIES-2026-10.md`). |
| `space` | Exactly `seated-origin, metres, +X forward, +Y right, +Z up (Unreal axes, cm converted to m)`. |
| `keypoints` | The 26 `EHandKeypoint` names, enum order, listed below. |
| `source` | `synthetic`, `recorded`, or `mouse`. |
| `seed` | Integer in `[0, 4294967295]`. Synthetic seed is FNV-1a of `action + NUL + variant`. Recorded clips use `0`. |

## Frame

One JSON object per line, one hand:

| Field | Rule |
|---|---|
| `t` | Seconds from the start of the clip. Finite, `>= 0`. |
| `hand` | `"L"` or `"R"`, and a member of the header `hands`. |
| `conf` | Confidence in `[0, 1]`. |
| `pinch` | Thumb-index pinch in `[0, 1]`. `1` is pen-down (tips touching). `0` is released. |
| `sys` | `@2` only, optional, boolean. See "Schema @2". Absent means `false`. |
| `joints` | Length 26. Each entry is `[x, y, z, qx, qy, qz, qw]`. |

When both hands are present they share a timestamp: the `L` line, then the `R` line, with the same `t`.

### Time

- Each hand's first sample is `t = 0`.
- Per hand, `t` is strictly increasing.
- Across the file, `t` is non-decreasing.
- The gap between consecutive samples of the same hand is within 10% of `1/hz`.
- Synthetic clips sample at exactly `i / 72` seconds.

### Joints

`EHandKeypoint` order (Unreal `HeadMountedDisplayTypes.h`):

`Palm`, `Wrist`, `ThumbMetacarpal`, `ThumbProximal`, `ThumbDistal`, `ThumbTip`,
`IndexMetacarpal`, `IndexProximal`, `IndexIntermediate`, `IndexDistal`, `IndexTip`,
`MiddleMetacarpal`, `MiddleProximal`, `MiddleIntermediate`, `MiddleDistal`, `MiddleTip`,
`RingMetacarpal`, `RingProximal`, `RingIntermediate`, `RingDistal`, `RingTip`,
`LittleMetacarpal`, `LittleProximal`, `LittleIntermediate`, `LittleDistal`, `LittleTip`.

Quaternion `(qx, qy, qz, qw)` is a unit quaternion (`|norm - 1| <= 1e-3`), right-handed.
It rotates the joint frame into seated-origin space.

- Local `+Z` points at the next joint in the chain. A tip uses the incoming bone (parent toward tip).
- Local `+X` is the palm normal (out of the palm, the direction the palm faces), projected so it stays orthogonal to `+Z`.
- Local `+Y` completes the right-handed basis (`Y = Z × X`).
- `q` and `-q` are the same rotation. Writers keep the sign continuous: non-negative dot with the previous sample of that joint.
- `Palm`: `+Z` points from the palm toward the middle-finger MCP, `+X` is the palm normal.
- `Wrist`: `+Z` points from the wrist toward the palm.

Chains: thumb and each finger run metacarpal to tip. The four finger MCP joints stay in the palm; curl rotates the bones beyond them. The thumb chain reaches from its metacarpal toward the pinch target.

## Schema @2

`mage-arena/hand-clip@2` is `@1` plus one optional frame field. Nothing else changes: the header, the time rules and the
joints are the same.

| Field | Rule |
|---|---|
| `sys` | Boolean, optional, written after `pinch` (and before `joints`). Absent means `false`. |

`sys: true` marks a frame on which the runtime would raise the system-gesture bit (the open palm toward the headset, then
the pinch). It mirrors OpenXR `XR_HAND_TRACKING_AIM_SYSTEM_GESTURE_BIT_FB`, and the player copies it into
`FHandFrame::bSystemGesture`. While it is set for a hand, the ward, blink, sigil and staff detectors process no gesture
from that hand and cancel one in progress (Meta VRC.Quest.Input.8). A sample between two frames is blocked when either
neighbour carries the bit.

Objects stay closed: an `@1` file that carries `sys` is invalid, and so is an `@2` `sys` that is not a boolean. Both
schemas load. The `@2` clips live in `apps/vr/Game/Clips/system/`; `generate.mjs` does not rewrite any `@1` clip.

## Seated space

Origin is the midpoint of the hips at seat height. `+X` is forward, `+Y` is right, `+Z` is up.
This is Unreal's axis order. A yaw of 0 points along `+X`; positive yaw turns toward `+Y` (right).

Synthetic anchors (metres), before a sloppy distortion:

- Chest centre `(0.12, 0.00, 0.34)`.
- Sigil plane `x = 0.52` (40 cm in front of the chest). Circle centre `(0.52, 0.00, 0.34)`, radius `0.10` (20 cm diameter), drawn in the YZ plane. The right index tip starts at the bottom and travels counterclockwise as the caster looks forward (bottom, right, top, left, bottom).
- Inner stroke, still pen-down after a short lift: line 1 a vertical bar, line 2 a horizontal bar, line 3 a diagonal (lower left to upper right). Each bar is about 12–14 cm through the circle centre.
- Pinch is `1` on the circle and on the inner stroke, and it drops during the lift between them and again at the release. That is drawing style A (two strokes).
- Mudra: both hands meet in front of the chest, palms facing each other, fingers up, and hold for `0.60` s. Thumb and index rest together (pinch about `0.85`).
- Bolt: the right index flicks forward (`+X`) about 18 cm.
- Ward: the left palm rises from the lap to chest height, palm normal `+X` (facing forward), in `0.25` s, then holds `1.00` s.
- Both palms: each palm rises the same way, left at `y = -0.14` and right at `y = 0.14`, in `0.25` s, then holds `0.80` s. This is the resume gesture.
- Blink: the right index flicks about 16 cm toward a pad at yaw `-60` (left), `+60` (right), or `180` (back).
- Staff plant: both hands grip (pinch `0.90`, palms facing each other) and thrust down about 25 cm, from about chest-and-a-half to the lap, then hold. Staff lift is that thrust reversed.

## Variants

| Variant | What changes |
|---|---|
| `normal` | The durations above. `conf` is `1`. No noise. |
| `slow` | Every phase lasts `1.6×`. Same shape, same `conf`. `hz` stays 72, so there are more samples. |
| `sloppy` | Same duration as `normal`. Seeded fingertip jitter of 4–8 mm. The path is scaled about its anchor by 10–20% (an ellipse instead of the circle, a stretched stroke). The ballistic part of the gesture overshoots, then settles. Two short `conf` dips to `0.4`. |

The synthetic seed is FNV-1a 32-bit of the UTF-8 bytes of `action`, a NUL byte, then `variant`, offset basis `2166136261`, prime `16777619`. The same seed produces the same bytes. `slow` and `normal` record the seed and do not use it for noise.

## Validation

`node apps/vr/tools/clipgen/validate.mjs <dir>` checks every `*.jsonl` in the directory and its subfolders:

- UTF-8 with no BOM, LF only, trailing LF, no blank lines.
- Header fields, types, `schema` (`@1` or `@2`), exact `space`, `hz`, and the 26 keypoint names.
- File name is `<action>.<variant>.jsonl`, or `<action>.<variant>.<id>.jsonl` in a corpus folder. The header `action` and `variant` match the first two name parts.
- 26 joints of 7 finite numbers. Quaternion norm within `1e-3` of 1.
- `conf` and `pinch` in `[0, 1]`. `hand` is declared in the header. `sys` is a boolean and only in `@2`.
- Per-hand `t` starts at 0 and is strictly increasing. File order of `t` is non-decreasing.
- Sample spacing within 10% of `1/hz`.
- Both hands, when present, have the same count and the same timestamps, `L` then `R`.

It prints one line per clip and a summary, and exits non-zero if any clip is invalid.

## Recorder (November)

A device recording is this schema with `source: "recorded"` and `seed: 0`.

- Write at 72 Hz and keep repeated poses as they arrive, with no smoothing and no interpolation: if the hands update slower than 72 Hz, the same pose repeats on consecutive lines. `t` is seconds from the first sample, not the headset clock.
- Keep the gap within 10% of `1/72`. If tracking drops, still write the sample: repeat the last joints and set `conf` low. Do not leave a hole.
- Write one line per tracked hand per sample. Left before right when both are tracked. A one-hand clip lists only that hand.
- Convert device centimetres to metres. Stay in seated-origin space with Unreal axes (`+X` forward, `+Y` right, `+Z` up). Do not write a stage-space or OpenXR-space clip.
- Keypoint index `i` is `EHandKeypoint` value `i`. Do not reorder.
- `pinch` is the runtime thumb-index contact in `[0, 1]` (`1` means the tips are together). `conf` is the runtime tracking confidence in `[0, 1]`.
- Quaternions are unit length, in the joint frame above. Flip the sign so the dot with the previous sample is non-negative.
- `mouse` is reserved for a clip whose index tip follows a mouse stroke and whose other joints are a hand posed around that tip. Same header and frame rules.

The desktop player samples by `t` and interpolates (lerp positions, pinch, and confidence; slerp rotations). A recording that obeys the spacing rule plays without a special case.
