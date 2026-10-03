# A03 - Sigil glyphs and tier runes as exact vector art (no Unreal build)

Status: open
Max turns: 140

## Goal
Two pieces of art must be exact, not painted: the **sigils** the player draws (the art must match what the recognizer
accepts, or the tutorial teaches the wrong shape) and the four **tier runes** on the bronze cuff (image generators kept
turning "runes" into Latin letters in A01 - see `runs/A01/REPORT.md`). This card produces both as deterministic vector
sources plus texture exports in the three shortlisted palettes, for the UI, the tutorial ghost-trail and the VFX.

## Read first
- `CLAUDE.md`, `docs/DECISIONS.md`, `docs/DESKTOP-INPUT.md` (the sigil vocabulary), `docs/CLIP-SCHEMA.md`
- `apps/vr/Game/Clips/templates/*.jsonl` (the recognizer templates) and
  `apps/vr/Game/Source/MageArenaVR/Gestures/SigilStrokeBuilder.cpp` (plane projection: how a 3D fingertip path becomes
  the 2D shape the recognizer sees - reuse the same math in the tool so the art equals the recognized shape)
- `apps/vr/art/style-studies/prompts.json` (`palette` per style) and the A01 images for the look of each style

## Deliverables
1. **`apps/vr/tools/glyphs/`** - a deterministic generator (Node; npm dependencies allowed but pinned in a local
   `package.json` with a lockfile, or Python with Pillow) that:
   - reads the `normal`-variant templates for line1, line2, line3 and the mudra clip, projects them exactly as the
     stroke builder does, smooths and normalises them, and emits **canonical sigil paths**: `sigil-line1.svg`,
     `sigil-line2.svg`, `sigil-line3.svg`, `sigil-mudra.svg` (stroke paths in drawing order, with a small start dot and
     direction arrow variant `*-guide.svg` for the tutorial);
   - authors the **four tier runes** `rune-tier1..4.svg` as geometric marks built only from circles, arcs, dots and
     short radial ticks (tier I = one dot in a ring, II = two, III = three, IV = a closed ring with a four-point star);
     no straight-stroke combinations that resemble Latin letters or digits;
   - exports PNG textures with alpha at 256, 512 and 1024 px: white-on-transparent masks (for engine tinting) plus
     one coloured glow version per shortlisted style using that style's palette (13 screen-print, 25 moonlit-silver,
     30 chalk-slate), to `apps/vr/art/glyphs/`.
   - Running it twice produces byte-identical outputs (state how you guarantee this, e.g. fixed rasteriser and no
     timestamps in PNG metadata).
2. **`apps/vr/art/glyphs/index.html`** - a preview sheet (opens from disk): every glyph and rune in every style and the
   mask, on dark and light backgrounds, plus each sigil's recognised template overlaid in a thin line so a reviewer can
   see the art matches the recognizer shape.
3. **Recognizer round-trip check** - a small automated check (Node or Python is fine; no Unreal) that resamples each
   canonical SVG path back into a point cloud and confirms it is nearest to its own class among the templates (same $Q
   distance as `QPointCloudRecognizer`: n = 32, normalised), printing the distances. If the C++ recognizer's exact
   normalisation cannot be reproduced, say so and show the nearest-class result anyway.
4. **`runs/A03/REPORT.md`** - files, how determinism is guaranteed, the round-trip distances, and anything you could
   not do.

## Constraints
- Do not commit, push, or touch any repository other than this one. Do not edit `docs/`, `apps/vr/Game/` (read only),
  `apps/vr/data/`, or the A01 images. No paid services; no image-generation model for these assets (they must be exact).
- Do not run Unreal builds or the editor (the build lane is busy).
- PNGs and SVGs go under `apps/vr/art/glyphs/`; PNGs are fine as normal files if each is under 300 KB.

## Acceptance (the orchestrator re-runs these)
```
<your generator command>            # twice; outputs byte-identical (show hashes)
<your round-trip check command>     # each sigil nearest to its own class
start apps/vr/art/glyphs/index.html # the orchestrator looks at every glyph
```
