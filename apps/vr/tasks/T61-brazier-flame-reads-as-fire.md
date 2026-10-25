# T61 - the brazier flame reads as fire (option B)

Status: open (2026-10-25, `docs/research/BRAZIERS-T61-2026-10-25.md`). Held: build Succeeded; check-overlay and check-pin
OK; D-G4 max 126 (<= 128), parity ok=True; BrazierFlameNotThreat green in all four palettes (least chroma 0.198); the
critic read no threat colour in the new flame. Failed: (a) the critic reads it as fire only weakly ("a pale candle or gas
tuft") after three rounds; (b) the moved near braziers stand behind the fire bolt and the steel line in
04-threats-midflight; MageArena. is 310/311, because MageArena.Overlay.Golden needs a re-record after the layout move
(hash lines only; the patch is not committed, since apps/vr/data/scenarios-vr/ is outside this lane).
Max turns: 120

## Goal
The four brazier flames read as fire to a blind critic and still read as no threat. T60's bowls stand (accepted), but
its flame, held dimmer than every threat glow, reads as smoke or a khaki shrub against the lit wall. Athena's ruling on
deaa15b (`docs/DECISIONS.md`, 2026-10-25) withdraws that clause and picks option B: a pale, desaturated warm white or
yellow heart, brighter than the unblockable's rim and dimmer than Fire and Earth, separated from the vermilion bolt by
hue and shape, not darkness. The near-right brazier moves off the demo unblockable's flight line, and the stale flame
fields leave the layout. This is the last arena-dressing item before the 31 Oct desktop core gate. No combat number
changes.

## Read first
- `CLAUDE.md`, `docs/DECISIONS.md` (they outrank this card if they conflict)
- `docs/research/BRAZIERS-T60-2026-10-25.md` (all of it, section 6 above all), `apps/vr/tasks/T60-brazier-fire-props.md`
- `apps/vr/Game/Source/MageArenaVR/Greybox/BrazierFire.h`, `BrazierFire.cpp`, `Tests/BrazierTests.cpp`
- `apps/vr/data/vr/arena-layout.json`

## Deliverables (exact paths)
- `apps/vr/Game/Source/MageArenaVR/Greybox/BrazierFire.{h,cpp}` - the heart (and the body, at or below the heart)
  re-derived in code as a pale, desaturated warm white or yellow. Per palette in force (greybox, styles 13, 25, 30):
  the heart's luminance strictly above the rim's and strictly below the dimmer of Fire's and Earth's. Never read from a
  style. The shape, the two instanced layers on `M_SpellGlow` and the switch stay; no new material, no `.uasset`.
- `apps/vr/Game/Source/MageArenaVR/Tests/BrazierTests.cpp` - `Greybox.BrazierFlameNotThreat` asserts the new band for
  all four palettes instead of "dimmer than the dimmest threat glow", and keeps every other assertion (the 0.15 chroma
  margin off water, fire, air, earth, steel, the rim, the core, the style's `projectiles.fire` and `arena.flame`; the
  `MinBrightest` floor; not ember orange, red rim or black core; warm R > G > B). Red first on the T60 constants.
- `apps/vr/data/vr/arena-layout.json` - the brazier on the demo unblockable's flight line moved so that, from the centre
  seat, no part of its bowl or flame overlaps that line or its rim in `04-threats-midflight` and
  `10-unblockable-telegraph`; inside the oval, off the centre flight line, clear of the banners, the crowd and the dais.
  `colours.flame` removed and `_propNote` rewritten (bowl and glow flame, no cone).
- `docs/research/BRAZIERS-T61-2026-10-25.md` - the report in the T60 report's shape.

## Constraints
- Do not push, or touch any repository other than this one. No paid service, no image generation.
- `apps/vr/data/PINNED.json`, `apps/vr/data/pinned/` and `apps/vr/data/conformance/` are never edited;
  `check-overlay.mjs` and `check-pin.mjs` stay green.
- Do not loosen the 0.15 chroma margin to pass a test. A near-white heart can drift toward steel: the test must catch it.
- No option C (lights or an emissive rim) without a ruling.
- The threat colour language stays: element colour = absorb, steel = dodge, black core with a red rim = leave.
- One Unreal process machine-wide; Blender and ComfyUI stay down.
- Budget: D-G4 max at most 128 target, hard stop at 130; BasePass tris under 350k; live particles plus 400 spell puffs
  plus 20 flame puffs at most 2,000; parity ok=True. Past a hard stop: report instead of committing.
- Blind critic: a fresh agent each round, no code and no key, shuffled A/B, at most three rounds. If round 3 still fails
  (a), commit the best state that passes (b) with the report and say so.

## Acceptance (the orchestrator re-runs these; quote their real output in your report)
```
powershell -NoProfile -File apps/vr/tools/build.ps1   # expected: Result: Succeeded on the commit of record
Automation RunTests MageArena. -nullrhi   # expected: 310 + any new, all Success, EXIT CODE: 0
node apps/vr/tools/check-overlay.mjs; node apps/vr/tools/check-pin.mjs   # expected: both OK
capture-budget.ps1 -Runs 3 -ExtraArgs -MageArenaCreatureImportScene   # expected: max <= 128 (hard stop 130), parity ok=True
blind critic on shuffled before/after stills   # expected: (a) YES fire, (b) NO on every after image, (c) no regression
```

## Report
Write `docs/research/BRAZIERS-T61-2026-10-25.md` in the T60 report's shape (label, colour table per palette with the
band bounds and least chroma distance, the new brazier position and why, the check output, D-G4 before and after, every
critic round quoted, file:line changes, open items). Point T60's section 6 at it, note the flame in
`docs/concepts/VISUAL-PLAN.md`'s arena order, and set this card's status to done only if every acceptance holds.
