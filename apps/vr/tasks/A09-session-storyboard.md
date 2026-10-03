# A09 - Design storyboard of the 10-minute session (images, no Unreal build)

Status: open
Max turns: 140

## Goal
Game design is half of the owner's top risk (R0). Before the session is built, let the owner see its shape: twelve
rough storyboard frames from cold start through the teach, the three bouts and the finisher to victory (plan section 6
session table). The frames are deliberately a pencil-and-marker sketch so they read as a design draft, not as a style
candidate; only magic is coloured, so the threat language is visible.

## Read first
`CLAUDE.md`, `docs/DECISIONS.md`, `docs/PROJECT-PLAN.md` section 6 (session table), `runs/A01/REPORT.md`,
`apps/vr/art/session-storyboard/prompts.json` (authored; use verbatim; each item also has `time` and `beat`).

## Deliverables
1. **12 frames** at each item's `path`; your image tool first (16:9; append "Avoid: <negative>" if needed); fallback
   `"$LOCALAPPDATA/agy/bin/agy.exe" -p "Use your image generation tool to create one 16:9 image from this prompt and save it to <absolute path>: <prompt>" --model gemini-3.8-flash-high --output-format json`
   (SUCCESS + empty `denied_actions` + existing file). Prefer the fallback for an item where your tool ignores the
   subject twice.
2. **Look at every frame**; regenerate at most twice for: text/letters/numbers, wrong finger count, headset/controllers,
   or a frame that misses its beat (e.g. no shield arc in 02, no pads in 05, no seal in 11).
3. `apps/vr/art/session-storyboard/manifest.json` and `index.html`: a horizontal-scrolling or grid strip in time order,
   each frame captioned with its `time` and `beat` (captions in HTML, never in the image), prompt in `<details>`,
   16 px+ text, opens from disk.
4. `runs/A09/REPORT.md`: tools, regenerations and why, and for each frame one sentence on whether it communicates its
   beat.

## Constraints
Do not commit, push, or touch other repositories; do not edit `docs/`, `apps/vr/Game/`, earlier art or `prompts.json`;
no Unreal builds (the build lane is busy); JPEGs under 2 MB.

## Acceptance
```
node -e "const m=require('./apps/vr/art/session-storyboard/manifest.json');const fs=require('fs');const it=m.items||m;let ok=0;for(const i of it){if(fs.existsSync(i.path)&&fs.statSync(i.path).size>20000)ok++}console.log(ok+'/12 frames present')"
```
