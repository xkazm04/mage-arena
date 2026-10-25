# T64 - the death frame's blank text card

Status: done (plate fixed, guarded; the two white bars are a designed effect, see Open question)
Max turns: 120

## Goal
The style-pick death frame (tick 1870) showed a black rectangle across the upper view and two bare white bars
(`docs/research/GATE-CAPTURES-2026-10-25.md`, Stills findings). The black rectangle was a text card shown before its
text; the capture now cannot contain such a frame, and a test and the capture driver fail on one.

## Diagnosis
- The rectangle is `DaisPlate`, the 196 x 36 cm prompt plate in near black (`SignPlateColour`,
  `Session/SessionPresentation.cpp:55`, built at `EnsureTeachVisuals`, shown at `SyncTeachVisuals`). Its text is `DaisWidget`, a
  `UWidgetComponent` carrying an `STextBlock`.
- A widget component draws into its render target only while it is visible (`UWidgetComponent::ShouldDrawWidget` needs
  `IsVisible()`, engine `WidgetComponent.cpp:1369`). The plate and the text were made visible on the same sync, so on the
  first frame of a prompt the plate stood with no text on it. The death frame is the first frame of the "lost" prompt
  ("Palm: fight again..."), and the capture ends on it, so the still cut at the last event is that frame.
- The same pattern is in `SyncTablet` (tablet slab and text) and in the stone labels (`SyncComfortVisuals`, shown with
  the stones on a lost final, a pick or a scene).
- Not palette-bound: all four styles' stills 4 are cut from tick 1870, and the plate is geometry, not a style.
- Confirmed in the engine: before the fix `CountBlankCards()` was 1 on the first frame of the day's opening prompt in
  `MageArena.Presentation.Cards.NoBlankCard` (red, 4 of 4 frames), and 0 after.

## Fix
`Session/SessionPresentation.h` `FCardReveal`: a card is wanted, then primed for `PrimeSyncs` (2) syncs, then shown. During the
prime the widget is visible (so it draws) but `SetHiddenInGame(true)`; the plate, slab and label show on the sync after.
`CountBlankCards()` counts cards drawn without a drawn widget. `UStylePickCaptureDriver::Record` fails the run
(`MAGEVR_CAPTURE_FAIL`, so `capture-style-pick.ps1` exits 1) on a frame with a blank card. No kernel, combat or data change.

## Test
- `MageArena.Presentation.Cards.NoBlankCard` (red on the old tree, green now) and `MageArena.Presentation.Cards.RevealAfterPrime`.
- The capture driver's per-frame check (same predicate).

## Evidence
- Wolf kaziho (UE 5.8.3), worktree `544cc51a`, commit 9d75da0 plus the docs commits. `build.ps1`: `Result: Succeeded`.
- `Automation RunTests MageArena.` (corpus regenerated first): 332 completed, 332 success (-nullrhi, `runs/full.log`).
- `capture-style-pick.ps1` (all four styles): `capture-style-pick: ok`, exit 0, `MAGEVR_CAPTURE_DONE shots=1870` x4,
  events md5 `7b0715c871ac3399504c4f7a744896d4` x4 (unchanged). `apps/vr/art/style-pick/` was restored to HEAD.
- Stills 4 before and after: `C:/Users/kazda/mage-art-staging/gate-2026-10-25/style-pick-fix/{before,after}/`. After: the black
  rectangle is gone; the upper view is the crowd stands, two grey pillars with purple caps, banners and the sky band.

## Open question for the owner: the two white bars
They are not a card. A diagnostic listing the visible components near the eye at tick 1870 (temporary, removed) found
two cube parts 21 cm long and 50 cm apart at 63 cm from the eye, the T22 collar crack seams (`CrackSeams`,
`EnsureCollarVisuals`, one cube per wrist at camera-relative (56, +-26, -10.5)). They flash for `CrackSeamS` on each Crack, and
the death is a Crack. The left seam sits on the top edge of the wrist cuff; there is no cuff on the right wrist, so the right seam
is a bare bar, and the left one reads as a bare bar over the spell-ring panel. This is the designed effect, left alone. Options:
(A) keep it; (B) draw the seam only where a cuff is; (C) draw the mirror cuff. Recommendation: B, a few lines, but it
is a design call (T22 says both wrists).
