# Desktop input: hand gestures as quick actions

Status: design draft, 2026-10-02. Owner requirement: "PC experience saving hand gestures under keybindings as quick
actions." Target: the October desktop build must play the whole game without a headset, and November's VR build must
not need a second input path.

## The rule

**There is one gesture pipeline, and the keyboard feeds it hand data, not game commands.** A key press plays a
*recorded or synthetic hand clip* (a stream of hand-joint poses over time) into the same input layer the headset will
fill in November. The recognizer, the ward detector and the blink detector run on that stream exactly as they will in
VR. A shortcut that bypasses the pipeline and calls `CastLine()` directly would test nothing the headset needs.

```
Headset (Nov)      -> joint stream (72 Hz) --+
Keyboard clip (Oct) -> joint stream (replay) -+--> smoothing -> gesture detectors -> intents -> combat kernel
Mouse draw (Oct)   -> fingertip path --------+         (sigil / ward / blink)
```

## Three input layers

| Layer | What it is | Used for |
|---|---|---|
| **L1 Gesture clips (quick actions)** | Each key replays a saved hand clip through the pipeline: a sigil drawn by a hand, a palm raise, a flick. Clips are data files, authored synthetically now and re-recorded from a real Quest later. Variants per clip (fast, slow, sloppy) test robustness. | Playing the game on PC; automated tests; demo capture |
| **L2 Mouse drawing** | Hold the left mouse button and draw on a plane in front of the camera; the path goes to the sigil recognizer as a fingertip path. The right mouse button raises the ward toward the cursor. | Feeling the drawing mechanic and tuning the recognizer without a headset |
| **L3 Debug intents** | Direct commands for designers (`F1-F4`, pad snaps `F5-F7`, headset-off `F8`, tracking drop `F9`). Never a clip, and never used by tests that claim to cover gestures. | Balance iteration and pause triggers on the desktop |

## Default bindings (draft; owner tunes)

| Action | Gesture (VR) | L1 key (clip) | L2 mouse |
|---|---|---|---|
| Cast line 1 / 2 / 3 | draw sigil: circle + inner stroke for the slotted line | `1` / `2` / `3` | hold LMB, draw |
| Tier IV mudra | two-hand pose | `4` | - |
| Bolt (always available) | flick of the casting finger | `Q` | LMB click |
| Ward (absorb) | raise the off-hand palm toward the threat | hold `Space` (aims at view centre) | hold RMB (aims at cursor) |
| Perfect ward | fresh raise within 0.15 s of impact | tap `Space` on time | press RMB on time |
| Blink to pad left / right / back | finger flick toward the pad | `A` / `D` / `S` | - |
| Planted staff | both hands grip and thrust the staff down; the same key lifts it | `F` | - |
| Pause / resume | palm-up menu gesture | `Esc` (instant). Safety pauses resume with `R` | - |
| Both palms (resume) | both palms raised toward the arena | `R` | - |

Clip variants are selected with a modifier (`Shift` = sloppy, `Ctrl` = slow) so the desktop player can feel what an
imperfect hand does to recognition.

## What the desktop build can and cannot prove

- **Can:** every rule, every timing window, the full 10-minute arc, AI opponents, pacing, audio, the greybox and then
  the art; recognizer accuracy on recorded or mouse-drawn paths; the ward perfect-window logic.
- **Cannot:** real hand-tracking noise and latency, arm fatigue, comfort, true depth of a seated view, glasses FOV.
  These move to November, on the Quest 3 profile in Meta XR Simulator first and a purchased device after.
