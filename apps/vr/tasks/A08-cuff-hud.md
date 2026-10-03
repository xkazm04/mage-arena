# A08 - The wrist-cuff HUD as exact vector art, every state, three palettes (no Unreal build)

Status: open
Max turns: 140

## Goal
The game has one HUD and it is diegetic: the bronze prisoner's cuff on the off-hand wrist (`docs/DESKTOP-INPUT.md`,
plan section 6). It shows the **tier clock** (four rune sockets, lit as tiers I-IV unlock every 15 s, faster with a
perfect absorb) and **Flow** (0-5 stacks; at 5 the next spell is a free Crest). Design it as exact vector art from the
A03 runes, in every state the player must read at a glance, so the Unreal UI work later is a trace, not a guess.

## Read first
- `CLAUDE.md`, `docs/DECISIONS.md`, `docs/DESKTOP-INPUT.md`
- `apps/vr/data/pinned/docs/design/baseline-fourteen-nights/design/data/combat.json` (`tierClock`, `flow`: read the
  real values - four tiers at 0/15/30/45 s, Flow max 5)
- `apps/vr/art/glyphs/rune-tier1..4.svg` and `apps/vr/tools/glyphs/*` (reuse the palettes and the PNG writer)
- A01/A06 images for each style's look of the cuff (`apps/vr/art/style-studies/*/06-cuff-hud.jpg`)

## Deliverables
1. **`apps/vr/tools/hud/generate.mjs`** (deterministic, same rules as A03: byte-identical reruns): a flat-unrolled cuff
   band (the band as it would be texture-mapped, about 4:1) with four rune sockets holding `rune-tier1..4`, a row of
   five Flow pips along one edge, and a thin progress arc per socket showing time to the next unlock.
2. **States** (SVG + PNG at 1024 px wide, masks + one coloured version per shortlisted palette 13/25/30):
   `idle` (tier I lit only), `tier2`, `tier3`, `tier4` (all lit), `unlock-flash` (the moment a socket lights),
   `perfect-advance` (the progress arc jumping forward by the perfect-absorb advance), `flow-0..5`, `crest-ready`
   (Flow 5), and `pause` (dimmed). Lit vs dark must differ in luminance, not only in hue (colour-blind readable).
3. **`apps/vr/art/hud/index.html`**: every state x palette, on the band and on a wrist mock-up (a simple drawn forearm
   silhouette), plus a "glance test" panel: each state shown at 64 px wide (roughly how big the cuff appears at arm's
   length) so a reviewer can judge readability.
4. **`runs/A08/REPORT.md`**: files, determinism proof, the luminance contrast ratio lit vs dark per palette, and anything
   you could not do.

## Constraints
Do not commit, push, or touch other repositories; do not edit `docs/`, `apps/vr/Game/`, `apps/vr/data/`, or earlier art
(read only); no image-generation models (exact art); no Unreal builds. PNGs under 300 KB each.

## Acceptance (the orchestrator re-runs and looks at every state)
```
node apps/vr/tools/hud/generate.mjs     # twice; outputs byte-identical (show hashes)
start apps/vr/art/hud/index.html
```
