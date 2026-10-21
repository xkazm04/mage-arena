Status: draft, for the owner

# Hand-interaction write-up

Item 5 of the K6 list (`docs/research/KPI-2026-10-20.md` K6). Angle from `docs/PROJECT-PLAN.md` section 11: each verb
is mapped to the most reliable hand signal. The owner finalises.

**Read this first.** Every number below comes from recorded or synthetic hand clips and mouse paths on a desktop. No
real-hand figure exists yet: real hands are measured in the VR phase on the Quest 3 profile and a device
(`docs/DESKTOP-INPUT.md`, "What the desktop build can and cannot prove"; `docs/research/STATE-EVAL-2026-10-20.md`
section 3, sigil row: "Real hands: V2"). Do not paste this into the form until the real-hand rows in the last table
are filled. Each gesture row is marked built, designed or pending.

## Principle

The game asks for the gesture that tracking reads most reliably, one per verb (`docs/PROJECT-PLAN.md` section 11).
Hands never overlap in the layout, tracking loss pauses the game, and nothing asks for fine finger reads at speed
(same section). The player is seated; hands work in a drawing box from roughly chest to face height, 25-60 cm in
front of the body (`docs/PROJECT-PLAN.md` section 6.1).

## One pipeline

Every gesture goes through one pipeline: joint stream, smoothing, gesture detectors (sigil, ward, blink), intents,
combat kernel (`docs/DESKTOP-INPUT.md`, "The rule"). On desktop a key plays a recorded hand clip into it, and the mouse
draws a fingertip path. The headset will fill the same input layer. No shortcut skips the pipeline and claims to test a
gesture (`CLAUDE.md`, "Current phase"). Clip format: `docs/CLIP-SCHEMA.md`. Clips have normal, slow and sloppy
variants (`apps/vr/Game/Clips/`; `docs/DESKTOP-INPUT.md`).

## The gestures

| Verb | Gesture | Why this signal | State |
|---|---|---|---|
| Bolt | Casting hand open toward a target, then a quick pinch-snap. Soft-lock in a 60-degree cone with head-gaze assist | Pinch is the most robust tracked event; Bolt must be spammable without drawing (`docs/PROJECT-PLAN.md` section 6.2) | built on clips (`apps/vr/Game/Clips/bolt.*`) |
| Line cast (sigil) | Draw a circle (arms the cast), then a bar. The bar's angle picks the line: vertical, horizontal or diagonal. Release casts toward the hand ray | The circle is a steady pen-down frame for the recognizer; one shared arming shape keeps the vocabulary to three lines plus reject (`docs/PROJECT-PLAN.md` section 6.2; `docs/DECISIONS.md` 2026-10-03, "Sigil shape": circle + bar) | built for clips and mouse (`Gestures/QPointCloudRecognizer.*`, per STATE-EVAL section 3) |
| Ward | Off-hand open palm raised in front, palm normal forward. A 140-degree arc around the palm normal. A fresh raise within 0.15 s before impact is a perfect | Big, deliberate, hard to occlude; timing is measured from motion onset (`docs/PROJECT-PLAN.md` section 6.2) | built (`Gestures/WardDetector.*`; cards `T04`, `T04b`, `T17`, all done) |
| Blink | Casting hand points and flicks toward a pad; wrist speed over a threshold; direction picks the pad. 0.25 s invulnerable | Directional, one motion (`docs/PROJECT-PLAN.md` section 6.2) | built (`Gestures/BlinkDetector.*`, per STATE-EVAL section 3) |
| Both palms | Both palms raised toward the arena: the ritual and the resume after a pause | `docs/PROJECT-PLAN.md` section 6.6; `docs/DESKTOP-INPUT.md` | built on clips (`Clips/both-palms.*`) |
| Palm over a stone | Reach over a stone beside the seat and hold 0.40 s: comfort settings and the intermission pick | `docs/DESKTOP-INPUT.md` (bindings table) | built on clips (`Clips/stones/`) |
| Mudra seal (tier IV) | Both hands form a deliberate seal within 1.5 s of the line sigil; the seal releases the tier IV spell | Two-hand poses are what pose detection does well (`docs/PROJECT-PLAN.md` section 6.2) | **designed, not built.** The shape is not set (C1, Claude and owner). The detector is `apps/vr/tasks/T24-mudra-seal-detector.md`, blocked on C1. Only a synthetic clip exists (`Clips/mudra.*`) |
| Closed fist (missio) | After a rival falls: an open palm held 0.40 s spares, a closed fist held 0.40 s finishes; no choice in 5 s means spare | A held pose, never needed during a bout (`apps/vr/tasks/T25-closed-fist-pose-and-spare-or-finish.md`) | **designed, not built.** Card T25 is open and needs the Wolf UE host |

The earlier plan wording for the seal is "palms forward, thumbs and index fingers forming a triangle"
(`docs/PROJECT-PLAN.md` section 6.2). Treat it as superseded by C1 once C1 is written; do not quote it as the final
pose.

## Seated defences

A seated player cannot walk away. The seated Wave 1 was lost at 16.45 s with the walking numbers (DF-001,
`docs/DECISIONS.md` 2026-10-03, "Seated combat"). The answer is to keep enemies on the floor below the dais, plus one
defence per school (`docs/design/SCHOOL-DEFENCES.md`; every number there is a proposal, not a balance claim).

| Defence | Gesture | Rule (proposal) | State |
|---|---|---|---|
| Split hands (all schools) | Hold the ward up with one hand and cast with the other | Bolt and lines up to tier II at a one-hand power multiplier (0.6); tiers III-IV and the Crest need both hands | built (`apps/vr/tasks/T11-split-hands-and-staff.md:3`, `T17` done) |
| Planted staff (Water) | Both hands grip and thrust down about 25 cm | A dome barrier for a few seconds that frees both hands for a two-handed spell; no blink while planted | built (`Gestures/StaffDetector.*`, T11) |
| Fire wall (Fire) | Casting hand sweeps horizontally, palm down | A wall that destroys projectiles; the Fire mage AI uses it | built for the AI (`apps/vr/tasks/T12-fire-wall-and-calibration.md:3`) |
| Stone form (Earth) | Both fists drawn to the chest, held | Invulnerable while channelling the heaviest spell | designed, not built (STATE-EVAL section 3) |
| Air form (Air) | Both palms open and sweep outward | A window of high dodge | built for the AI opponent only (`apps/vr/tasks/T23-air-school-opponent.md:3`) |

Which defence the player uses in the slice: Water gets split hands and the planted staff (`docs/DECISIONS.md`
2026-10-03, "Seated balance").

## Designed against tracking failure

- **Pause on loss:** the kernel pauses after 1.5 s of lost hand tracking in combat, on headset removal and on loss of
  system-menu focus. Resume is both palms, then a 3-2-1 count (`docs/PROJECT-PLAN.md` section 6.6; card
  `apps/vr/tasks/T13-teach-and-pause.md`, done 2026-10-04).
- **Left-hand mode** swaps the casting and ward hands. **Gentle** mode applies competence 1 to every opponent and doubles
  telegraph times. **Narrow-FOV mode** compresses spawn and telegraph origins to +/-40 degrees for glasses
  (`docs/PROJECT-PLAN.md` sections 6.1 and 6.7; card `apps/vr/tasks/T14-left-hand-narrow-fov-gentle.md`, done
  2026-10-04).
- **Colour is never the only cue:** shape and sound double the colour language (`docs/PROJECT-PLAN.md` section 6.7).
- **Threats only from the front,** so a seated player never turns (`docs/PROJECT-PLAN.md` section 6.1).
- **In-house recognizer:** a $Q port, because the paid option was ruled out (`docs/DECISIONS.md` 2026-10-02).

## Measured figures, and what is missing

| Figure | Value on record | Source | Limit |
|---|---|---|---|
| Sigil accuracy on the recorded corpus | 256 of 270 correct (94% floor) | `docs/research/BASELINE-2026-10-07.md:841` (read) | Synthetic clips, not real hands. The plan's gate is at least 95% top-1 on the owner's mouse draws (`docs/PROJECT-PLAN.md` section 4, D-G2); no run against that gate is quoted here |
| Impostor casts | 0 of 180 | same line | Same |
| Noise casts | 0 of 5 | same line | Five noise clips is a small sample |
| Line templates distinct | Each template is nearest to itself, margins 5138, 4016, 3027 | `docs/research/STATE-EVAL-2026-10-20.md` section 3 (measured 2026-10-20) | Template geometry only |
| Cross-user accuracy | **pending** | gate: at least 85% for a second person (`docs/PROJECT-PLAN.md` section 4) | Needs a second person (section 12, question 6) |
| Real-hand accuracy and ward latency | **pending** | VR phase, Quest 3 profile, then a device | Needs the Android build: K5 reads 1/5 (`docs/research/KPI-2026-10-20.md`) |
| Mudra and fist detection | **pending** | after C1, T24 and T25 | Not built |

## Open points for the owner

- Choose which pending figures must exist before the form is filed, and by what date.
- Confirm the mudra shape (C1) before this file describes it as final.
- The Quest 3S is declared and untested by owner ruling (`docs/DECISIONS.md` 2026-10-07, "Quest 3S ships untested");
  decide whether the form says so.
