# A07 - Style-neutral environment kit and arena character sheets (images, no Unreal build)

Status: open
Max turns: 140

## Goal
The greybox arena (card A02) and the later 3D pass need consistent, style-neutral references for the environment:
modular stands, wall and gate pieces, arena props (including the player's stone seat and the rune pad discs), crowd
silhouette cards, and the arcane warden who watches every duel. Same process and look as A04, so the sheets match.

## Read first
- `CLAUDE.md`, `docs/DECISIONS.md`, `docs/ORCHESTRATION.md`, `runs/A01/REPORT.md` (what went wrong with letters)
- `apps/vr/art/environment-kit/prompts.json` - authored prompts. **Use them verbatim.**

## Deliverables
1. **5 images** at each item's `path`. Your image tool first (request the aspect; append "Avoid: <negative>" if there
   is no negative field). Fallback when your tool fails or hits a limit:
   `"$LOCALAPPDATA/agy/bin/agy.exe" -p "Use your image generation tool to create one image from this prompt and save it to <absolute path>: <prompt>" --model gemini-3.8-flash-high --output-format json`
   (accept only `status` SUCCESS, empty `denied_actions`, and an existing file).
2. **Look at every image** and regenerate, at most twice per item, when: any text, letters or numbers appear; a
   turnaround lacks one of its required views or the views differ in scale; a pose sheet has the wrong number of poses;
   a hand has the wrong number of fingers; the subject is wrong.
3. **`apps/vr/art/environment-kit/manifest.json`** (same fields as A01) and **`apps/vr/art/environment-kit/index.html`**
   (opens from disk; one image per row, large, prompt in `<details>`, 16 px+ body text).
4. **`runs/A07/REPORT.md`**: tools used, regenerations and why, and for each sheet one sentence on whether it is usable
   as modelling/animation reference and what a modeller would still have to guess.

## Constraints
Do not commit, push, or touch other repositories. Do not edit `docs/`, `apps/vr/Game/`, `apps/vr/data/`, A01/A03 art,
or `prompts.json`. No Unreal builds (the build lane is busy). JPEGs under 2 MB.

## Acceptance (the orchestrator re-checks and looks at every image)
```
node -e "const m=require('./apps/vr/art/environment-kit/manifest.json');const fs=require('fs');const it=m.items||m;let ok=0;for(const i of it){if(fs.existsSync(i.path)&&fs.statSync(i.path).size>20000)ok++}console.log(ok+'/5 images present')"
```
