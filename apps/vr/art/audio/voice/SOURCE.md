# Voice lines - source

**No WAV was generated.** This folder holds the cue sheet only. Audio production is deferred by the owner's ruling
"2026-10-24 - Music and audio deferred; the visual side comes first" (`docs/DECISIONS.md`): "Delay music and audio until
to focus on visual side". That ruling names T27's voice generation and allows no audio spend while it stands. No
ElevenLabs call was made, no API key was read, and no other service was used.

- **The cue sheet:** `manifest.json`, 27 rows, copied verbatim from C2 section 9
  (`docs/campaign/05-c2-slice-story-bible-and-script.md`). Each row has id, text, speaker, C2's word count, where it plays,
  its cut block, the source row and the WAV's path under this folder (C2 section 9: `<speaker>/<cue>.wav`; the
  unvoiced prologue rows, speaker null, sit at the folder root). `generated`, `service` and `voiceId` are null on every
  row: nothing was made.
- **The count, when the ruling is lifted:** C2 decision D4 is approved at **14 cues, blocks A + B** (`docs/DECISIONS.md`,
  "2026-10-24 - C2 rulings D1-D4 approved as recommended"): RIT-01..03, OPH-05, BRE-01, BRE-02, LIO-01, LIO-02 (A) and
  VEN-01..04, VEN-06, VEN-07 (B). One generation per row. Whoever renders them fills the three null fields and adds
  their own ledger entry under the ElevenLabs conditions of 2026-10-21.
- **The import:** `apps/vr/tools/import-voice.ps1` runs the ImportSfx commandlet with `-kind=voice`. Each row whose WAV
  exists becomes `/Game/Audio/Voice/<id>`, idempotent by the source MD5 as `import-sfx.ps1` is. A row with no WAV is
  counted as missing, not failed.
- **At runtime:** the session raises a cue when its line stands (`Session/VoiceLines.h`). A cue with no imported asset is
  skipped silently and the day runs on the text alone (C2 section 8: voice is additive).

Written by the T27 builder (Claude, headless) on Wolf, 2026-10-25.
