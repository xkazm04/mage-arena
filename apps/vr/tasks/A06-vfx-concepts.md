# A06 - VFX concept frames in the three shortlisted styles (images, no Unreal build)

Status: open
Max turns: 120

## Goal
The two non-negotiables of the game are readable hands and an instantly readable threat language. Show, in each of
the three shortlisted styles, the four effects the player must read at a glance: a water bolt in flight, the ward
absorbing a hit, the perfect-absorb flash, and the unblockable telegraph. These frames feed the 19 Oct style pick and
later the Niagara effects. Same process as A01/A04.

## Read first
`CLAUDE.md`, `docs/DECISIONS.md`, `runs/A01/REPORT.md` (letters problem), `apps/vr/art/vfx-concepts/prompts.json`
(authored - use verbatim).

## Deliverables
1. **12 images** at each item's `path`, your image tool first (request 16:9; append "Avoid: <negative>" if needed);
   fallback `"$LOCALAPPDATA/agy/bin/agy.exe" -p "Use your image generation tool to create one 16:9 image from this prompt and save it to <absolute path>: <prompt>" --model gemini-3.8-flash-high --output-format json`
   (accept only SUCCESS with empty `denied_actions` and an existing file). If your tool repeatedly ignores the subject,
   use the fallback for that item (it followed subjects better in A04).
2. **Look at every image**; regenerate at most twice per item for: text/letters/numbers, wrong finger count, headset or
   controller, wrong subject (e.g. no shield arc in 02, no black core in 04).
3. `apps/vr/art/vfx-concepts/manifest.json` and `index.html` (rows = the four effects, columns = the three styles, prompt
   in `<details>`, 16 px+ text, opens from disk).
4. `runs/A06/REPORT.md`: tools, regenerations and why, and one sentence per style on how fast each threat reads.

## Constraints
Do not commit, push, or touch other repositories; do not edit `docs/`, `apps/vr/Game/`, earlier art or `prompts.json`;
no Unreal builds; JPEGs under 2 MB.

## Acceptance
```
node -e "const m=require('./apps/vr/art/vfx-concepts/manifest.json');const fs=require('fs');const it=m.items||m;let ok=0;for(const i of it){if(fs.existsSync(i.path)&&fs.statSync(i.path).size>20000)ok++}console.log(ok+'/12 images present')"
```
