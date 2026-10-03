# T11 - Split hands (A) and planted staff (E) for the Water slice

Status: open
Max turns: 300

## Goal
From a seat the player loses Wave 1 at 11.4 s (DF-001): drawing a sigil locks the ward and the blink, so the player can
neither defend nor deal damage fast enough. The owner's design direction (`docs/DECISIONS.md` 2026-10-03, worked out in
`docs/design/SCHOOL-DEFENCES.md`) gives the Water mage two seated defences: **A - split hands** (the off-hand holds the ward
while the casting hand casts weaker spells) and **E - planted staff** (a dome barrier that frees both hands for the two-hand
mudra and the Crest). Implement both and measure their effect on the seated design gate. Numbers are proposals; the
calibration card that follows tunes them.

## Read first
`CLAUDE.md`, `docs/DECISIONS.md`, `docs/design/SCHOOL-DEFENCES.md` (sections A and E - the spec), `docs/DESKTOP-INPUT.md`
(one pipeline), `docs/design-findings/DF-001-seated-wave1.md`, `runs/T10/REPORT.md`; code `Hands/*`, `Gestures/*`,
`Session/*`, `Kernel/*` (incl. `VrRules.*`), data `apps/vr/data/vr/combat.vr.json`, `apps/vr/tools/clipgen/*`.

## Deliverables
1. **Concurrent per-hand playback**: the hand pipeline plays one clip per hand at the same time (left: ward hold; right:
   sigil or bolt) - keys combine naturally (hold Space, press 1-3 or Q). Detectors read each hand independently. Tests:
   ward held while a sigil clip plays -> both the ward and the cast are recognised; existing single-gesture tests stay green.
2. **A - split-hand casting** (VR overlay rule): with the ward held, the casting hand may cast Bolt and lines up to tier II
   at the one-hand power multiplier (overlay field, proposal 0.6), building no Flow; tier III-IV and the Crest are refused
   (logged) unless both hands are free or the staff is planted. No perfect absorb on a ward that is also casting.
3. **E - planted staff**: new clips `staff-plant` / `staff-lift` (normal, slow, sloppy) via `clipgen` (both hands grip and
   thrust down ~25 cm / lift); a detector; kernel adapter rule (VR overlay): a dome barrier around the pad for up to N s
   (proposal 6), mana up front + drain, magic reduction 0.85, physical proposal 0.6, no perfect, unblockables pass; while
   planted both hands are free (mudra/Crest allowed) and blink is refused; lift by gesture, blink attempt, or expiry. Key
   binding for the clip: `F` (staff) through the same `PlayQuickAction` path.
4. **Shared kernel untouched without the overlay**: the 45 TS vectors byte-identical; new rules only through the ruleset seam.
5. **Oracle tests** (`MageArena.Defences.*`) with hand-computed expectations from the overlay values: one-hand multiplier
   applied, tier gate, no Flow, no perfect while split; dome reductions per hit kind, duration and mana, blink refused while
   planted, mudra allowed while planted, unblockable passes.
6. **Re-measure** `MageArenaDesign.Session.Wave1Seated` with a scripted seated player that uses A and E sensibly (clips only,
   no walking). Report victory/defeat, time vs 25-40 s, damage taken/dealt, how often split casting and the staff were used.
   Do not tune to force a win - report the gap; the calibration card handles numbers.
7. Gesture clips captured visibly: extend the capture to show the ward held while a sigil is drawn, and the planted dome;
   screenshots to `runs/T11/shots/`. `runs/T11/REPORT.md`.

## Constraints
Do not commit, push, or modify other repositories; do not edit `apps/vr/data/pinned/` or art. One Unreal build at a time.
Regular `MageArena.*` green; conformance vectors byte-identical. Keys play clips only (one-pipeline rule).

## Acceptance
```
node apps/vr/tools/clipgen/generate.mjs && node apps/vr/tools/clipgen/validate.mjs apps/vr/Game/Clips
node apps/vr/tools/conformance/generate.mjs     # byte-identical
powershell -NoProfile -File apps/vr/tools/build.ps1
"C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "C:\Users\kazda\kiro\mage-arena-vr\apps\vr\Game\MageArenaVR.uproject" -ExecCmds="Automation RunTests MageArena.;Quit" -unattended -nullrhi -nosplash -log
"C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "C:\Users\kazda\kiro\mage-arena-vr\apps\vr\Game\MageArenaVR.uproject" -ExecCmds="Automation RunTests MageArenaDesign.;Quit" -unattended -nullrhi -nosplash -log
```
