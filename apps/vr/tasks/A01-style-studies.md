# A01 - Style studies for the 19 Oct style pick (images, no Unreal build)

Status: done (images reviewed by orchestrator 2026-10-03)
Max turns: 160

## Goal
The owner picks one of three shortlisted art styles on Mon 19 Oct (13 Screen-Print Poster, 25 Moonlit Silver
Nocturne, 30 Chalk & Slate - `docs/ART-STYLE-STORYBOARD.md`, `docs/DECISIONS.md` "Art comes last"). Give that choice
real material: the SAME eight game-relevant scenes rendered in each style, side by side, so the comparison is about
style and not about subject. This card generates images only; it never builds Unreal or touches `apps/vr/Game/`.

## Read first
- `CLAUDE.md`, `docs/DECISIONS.md`, `docs/ORCHESTRATION.md` (worker routing: images)
- `apps/vr/art/style-studies/prompts.json` - the authored prompt sheet: 3 styles x 8 assets, each with `prompt`,
  `negative`, `aspect`, and the exact output `path`. **Use the prompts verbatim**; the orchestrator authored them.

## Deliverables
1. **24 images** at the `path` of each item (`apps/vr/art/style-studies/<style>/<asset>.jpg`).
   - Generate with your own image generation tool first. Request the item's aspect ratio if the tool supports it;
     otherwise generate square and note it. Apply the `negative` list if the tool supports negatives; otherwise append
     "Avoid: <negative>" to the prompt.
   - **Fallback when your image tool fails or reaches a limit:** call the Antigravity CLI, which has its own image tool:
     `"$LOCALAPPDATA/agy/bin/agy.exe" -p "Use your image generation tool to create one image from this prompt and save it to <absolute path>: <prompt>" --model gemini-3.8-flash-high --output-format json`
     Check its JSON: `status` must be SUCCESS AND `denied_actions` empty AND the file must exist; otherwise it failed.
   - **Look at every image you produce** (open the file) and regenerate, at most twice per item, when it shows visible
     text, letters or numbers; a hand with the wrong number of fingers; a VR headset or controller; or the wrong subject
     (e.g. a turnaround without three full-body views). Keep the attempt count.
2. **`apps/vr/art/style-studies/manifest.json`**: per item `{style, asset, path, tool ("grok" | "agy"), model if known,
   attempts, aspect_requested, aspect_delivered (w x h), regen_reasons[], seconds}` and a totals block (images per
   tool, failures, approximate cost if your tool reports one).
3. **`apps/vr/art/style-studies/index.html`**: a contact sheet that opens from disk with no server - rows are the 8
   assets, columns are the 3 styles, each cell the image (click opens full size), caption = style name + asset, the
   prompt in a `<details>` under each image. Readable body text (16 px or more), dark neutral background, no external
   resources except optional Google Fonts. Include the three palettes as swatches in the column headers.
4. **`runs/A01/REPORT.md`**: what was generated with which tool, every regeneration and why, anything that failed, and
   your one-paragraph observation per style on how readable the hands and the threat colours are (the game's two
   non-negotiables), without choosing a winner - the owner chooses.

## Constraints
- Do not commit, push, or touch any repository other than this one. Do not edit `docs/`, `apps/vr/Game/`,
  `apps/vr/data/`, or `prompts.json`. No paid services other than your own image tool and the Antigravity CLI seat.
- Do not run Unreal builds or the editor (another worker is building).
- Images are `.jpg`. Keep each under 2 MB.

## Acceptance (the orchestrator re-checks)
```
node -e "const m=require('./apps/vr/art/style-studies/manifest.json');const fs=require('fs');const items=m.items||m;let ok=0;for(const i of items){if(fs.existsSync(i.path)&&fs.statSync(i.path).size>20000)ok++}console.log(ok+'/24 images present')"
    # expected: 24/24 images present
start apps/vr/art/style-studies/index.html     # the orchestrator opens and looks at every cell
```

## Report
See deliverable 4.
