# T27 - Voice and crowd chorus from the C2 cue sheet

Status: open. **C2 is drafted: `docs/campaign/05-c2-slice-story-bible-and-script.md`** (2026-10-21), with the cue sheet in
its section 9 - 27 cues, every one 10 words or fewer. Still blocked, but now on the **owner's ruling of C2 decisions D1-D4**,
not on the writing. **D4 is the one that gates this card: voice is not yet approved** (the ElevenLabs approval in
`docs/DECISIONS.md` names SFX and music). Recommended go: **14 cues, blocks A + B**. Until D4 is ruled, build the importer,
wiring and tests against the manifest and generate nothing. The wiring runs on the build machine (Wolf, the x64 UE host: UE 5.8.3 editor).
The generation step needs the owner's go (see Constraints). Written 2026-10-20 on the ARM64 laptop, where none of it runs.
Max turns: 160

## Goal
The slice speaks: the ritual words, Brennic's phase-break taunts and the crowd chorus play as pre-rendered lines, wired to
the collar ritual, the phase breaks and the aftermath (roadmap row 13). Lines are 10 words or fewer each and pre-rendered;
there are no runtime calls (`docs/DECISIONS.md`, the approval for pre-rendered voice lines). It shares the mix and spatial
route of K3 row 8 ("threat cues spatialized", `docs/research/KPI-2026-10-20.md`) and feeds K3 row 15 ("audio", owner
verdict, felt). It moves no mechanical K3 row.

## Read first
- `CLAUDE.md`, `docs/DECISIONS.md` (they outrank this card if they conflict; read the voice-lines entry and the paid-service rule)
- **C2: `docs/campaign/05-c2-slice-story-bible-and-script.md`** - sections 5 (the lines), 9 (the cue sheet and the A-P cut
  blocks) and 10 (the `_story.*` marker convention this card must apply). Its path is the first line of the report. This card
  also owns, per C2 section 4: `day.json` `ritualLineS` -> an array, `ritualTimeoutS` 20 -> 45, the `strings.json` `story.*`
  lines and `_story.*` markers, and the `DayTests.cpp` marker assertion (C2 section 10)
- `docs/ORCHESTRATION.md` "Budgets and limits" (ElevenLabs is the only approved paid service; a card that generates states the count)
- `apps/vr/tasks/T26-element-colour-and-first-audio.md`, `apps/vr/tools/import-sfx.ps1` and `capture-colour-audio.ps1` (the
  import and capture route this card follows), `apps/vr/art/audio/sfx/manifest.json`
- Code: the T26 cue player under `Source/MageArenaVR/`, `Session/SessionPresentation.cpp`, `Session/SessionFlow.cpp` (ritual,
  phase breaks, aftermath), `Tests/AudioTests.cpp`

## Deliverables (exact paths)
- `apps/vr/art/audio/voice/` - the rendered lines as WAV (LFS) plus `manifest.json` (id, text, speaker, source cue-sheet row,
  generation date, service and voice id) and `SOURCE.md` (how they were made, by whom, the count). Produced from the C2 cue
  sheet by the owner or the App Master after the owner's go; the builder works with whatever is committed there
- An importer for the voice folder on the `ImportSfx` commandlet route (a folder argument or a sibling script in
  `apps/vr/tools/`, idempotent by source MD5 like `import-sfx.ps1`); assets under `apps/vr/Game/Content/Audio/Voice/`
- The wiring in `Session/`: each cue-sheet row plays at its event (ritual step, phase break, aftermath beat), spatialised from
  the speaker's position (crowd chorus from the stands), skipped silently if its asset is missing, never blocking the flow
- `apps/vr/Game/Source/MageArenaVR/Tests/VoiceTests.cpp` - `MageArena.Voice.*`: every manifest id has an imported asset; every
  cue-sheet row maps to one trigger; a missing asset does not stall the ritual; no line exceeds 10 words (read from the manifest)

## Constraints
- Do not commit, push, or touch any repository other than this one.
- **No new external services, and the builder calls none.** The roadmap names ElevenLabs for the render. Generation happens
  only on the owner's go, with the count stated on this card once C2 exists (one generation per cue-sheet row; expected
  lines: to be filled from C2). The builder never reads `ELEVENLABS_API_KEY`, never calls the API, and adds no runtime call.
  If the voice WAVs are not committed, build the importer, the wiring and the tests against the manifest and report the
  wiring as unproven end to end.
- Do not edit `docs/DECISIONS.md`, `apps/vr/data/pinned/` or the conformance vectors. No new dependency.
- Lines come from C2 verbatim; do not write or alter story text.
- Fresh checkout: regenerate the sigil corpus first (`CLAUDE.md`, Build). One Unreal build at a time.
- Known red on a clean tree (`CLAUDE.md`): `MageArenaDesign.Duel.FireSeated`, `Duel.AirSeated`, `Session.FullSeated`,
  `Session.Wave1Seated`, `Session.CreaturesSeated`. Any other change is a regression. (If T39 has landed, use its baseline.)
- Changed C++ lines pass `node apps/vr/tools/lint/format-check.mjs`; new members pass `node apps/vr/tools/lint/unused-members.mjs`.

## Acceptance (the orchestrator re-runs these; quote their real output in your report)
```
powershell -NoProfile -File apps/vr/tools/build.ps1                     # expected: exit 0
UnrealEditor-Cmd MageArenaVR.uproject -ExecCmds="Automation RunTests MageArena.Voice;Quit" -unattended -nullrhi -nosplash -log
                                                                         # expected: all MageArena.Voice.* green
UnrealEditor-Cmd MageArenaVR.uproject -ExecCmds="Automation RunTests MageArena.;Quit" -unattended -nullrhi -nosplash -log
                                                                         # expected: all MageArena. green
powershell -NoProfile -File apps/vr/tools/capture-colour-audio.ps1 -Run T27 -SkipStills
                                                                         # expected: MAGEVR_CAPTURE_DONE; runs/T27/audio/*.wav peak above -60 dBFS
node apps/vr/tools/lint/format-check.mjs && node apps/vr/tools/lint/unused-members.mjs
                                                                         # expected: OK
```
The capture is a proxy: whether the voice sounds right is the owner's call (felt) and is not claimed.

## Report
Write `runs/T27/REPORT.md`: the C2 path, the line count and where each plays, what you built (paths), each acceptance command
with its real output, anything you could not do and the exact error, and decisions the card did not specify.
