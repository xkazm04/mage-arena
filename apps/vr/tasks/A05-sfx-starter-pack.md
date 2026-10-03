# A05 - SFX starter pack for the greybox (ElevenLabs, no Unreal build)

Status: open
Max turns: 120

## Goal
The mechanics gate on Sun 18 Oct judges feel, and a duel without feedback sounds cannot be judged. Generate a
style-neutral starter pack of 20 sound effects from the authored cue sheet so every mechanic (draw, cast, reject, ward,
perfect absorb, blink, tier unlock, Flow, Crest) and every threat has a sound. They are placeholders: after the style
pick they are kept, remixed or replaced.

## Read first
- `CLAUDE.md`, `docs/DECISIONS.md` (ElevenLabs is the one approved paid service; key by name only),
  `apps/vr/art/audio/sfx/cues.json` (authored; use the texts verbatim, prefixed with `common`).

## Deliverables
1. **`apps/vr/tools/sfx/generate.mjs`** (Node, built-ins only): reads `cues.json`, calls ElevenLabs
   `POST https://api.elevenlabs.io/v1/sound-generation` with `{ text, duration_seconds, prompt_influence: 0.4 }` (and the
   loop flag if the API supports one), and writes the returned audio as `apps/vr/art/audio/sfx/src/<id>.mp3`.
   - **The key:** read `ELEVENLABS_API_KEY` at run time by parsing `C:\Users\kazda\kiro\pof\.env` (or the environment
     if already set). Never print it, log it, write it to any file, or put it on a command line.
   - **Budget:** one generation per cue, plus at most one retry per cue on an HTTP error; stop the run and report on a
     401/402/quota response. Skip cues whose output already exists (resumable). Print the request count at the end.
2. **Conversion:** with `ffmpeg` (on PATH), convert each mp3 to `apps/vr/art/audio/sfx/<id>.wav`, 48 kHz, 16-bit, mono
   (stereo only for `arena-crowd-loop`), trimmed of leading silence, peak-normalised to -1 dBFS. For `loop: true` cues,
   apply a short crossfade so the loop point is seamless and say how.
3. **`apps/vr/art/audio/sfx/manifest.json`**: per cue id, source text, requested and actual duration, sample rate,
   peak dBFS, loop flag, file paths, sha256 of the wav.
4. **`apps/vr/art/audio/sfx/index.html`**: a listening page that opens from disk (one row per cue: id, text, an
   `<audio controls>` element for the wav, duration), so the owner can audition the pack in a browser.
5. **`runs/A05/REPORT.md`**: requests made, failures, any cue that came out wrong (voices, music, wrong sound) and how
   you handled it (at most one regeneration per cue for those reasons), and total generated seconds.

## Constraints
Do not commit, push, or touch other repositories. Do not edit `cues.json`, `docs/`, `apps/vr/Game/`. No Unreal builds.
Never expose the API key. No other paid service.

## Acceptance (the orchestrator re-checks and listens to a sample)
```
node -e "const m=require('./apps/vr/art/audio/sfx/manifest.json');const fs=require('fs');const c=m.cues||m;let ok=0;for(const k of c){if(fs.existsSync(k.wav||k.path))ok++}console.log(ok+'/20 wav present')"
```
