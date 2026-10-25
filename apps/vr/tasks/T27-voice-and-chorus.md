# T27 - Voice and crowd chorus from the C2 cue sheet

Status: **non-audio half done; audio deferred, ruling 6** (`docs/DECISIONS.md` 2026-10-24, "Music and audio deferred").
Wolf 2026-10-25, UE 5.8.3, branch `autopilot/accepted-idea-delivery-51306867`: the importer, wiring and tests are built
against the manifest, no WAV is generated, and the wiring is **unproven end to end** (no asset exists to play). See
"Report (non-audio half)" below. Still open: the generation of the 14 approved cues (C2 D4, blocks A + B) once ruling 6 is
lifted, then the capture acceptance line. The chorus beats (`slice.json` `chorus`) are not voice rows and are not wired.
History: C2 was drafted 2026-10-21 (`docs/campaign/05-c2-slice-story-bible-and-script.md`, cue sheet in section 9) and
D1-D4 were approved 2026-10-24. Written 2026-10-20 on the ARM64 laptop.
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

## Report (non-audio half, 2026-10-25, Wolf; runs/ is never committed, so the report is here)

C2: `docs/campaign/05-c2-slice-story-bible-and-script.md` (sections 4, 5, 9 and 10). The cue sheet has 27 cues; no WAV
was generated (ruling 6). No API key was read and no paid service was called. Commits: `f94d1f7`, `0a58646`, `417477e`,
`84a73c9`, `87448b2` (the natural measurement), and `e117ec4` (DF-013).

### What was built (paths)

- `apps/vr/data/vr/day.json`: `ritualLineS` 3 -> `[12, 14, 12]` and `ritualTimeoutS` 20 -> 45.
  - `Session/DayFlow.cpp` reads the array: one number or 1-8, and every line but the last must start before the timeout.
  - The ritual shows its lines one at a time.
  - Both palms count from the ritual beat's 10 s floor (`GetRitualOfferS`), or once the offer line stands when the slice
    is skipped.
- `apps/vr/data/vr/strings.json`: the `story.*` lines are C2 section 5's, verbatim.
  - The prologue's and the ritual's placeholders stay as their beats' fallbacks, as section 5 says.
  - Every `_story.*` marker is its cue id (section 10).
- `Tests/DayTests.cpp` `MageArena.Session.DayStrings`: each marker matches a cue id in the cue sheet, and the line is
  that cue's text or the fallback of the slice beat that carries it.
- `apps/vr/art/audio/voice/manifest.json`: 27 rows (id, text, speaker, words, plays, block, source, file); generated,
  service and voiceId are null.
- `apps/vr/art/audio/voice/SOURCE.md`: no WAV, and why.
- `Tools/ImportSfxCommandlet.cpp`: `-destination=` and `-kind=voice` (MD5-idempotent; a row with no WAV counts as
  missing, not failed), run by `apps/vr/tools/import-voice.ps1`.
- `Session/StorySlice.{h,cpp}`: every beat's lines with cue and speaker, the taunts, `VoiceTriggers`.
- `Session/VoiceLines.{h,cpp}`: the cue record, the manifest loader and the speakers' greybox anchors.
- `Session/ArenaSession.cpp`, `DayFlow.cpp`, `SessionFlow.cpp`:
  - With the slice loaded each beat shows its C2 lines.
  - When a line with a cue starts to stand, `TrackVoice` raises a voice cue.
  - A bound rival's phase break raises its taunt (`DrainEvents`).
- `Session/ArenaAudio.{h,cpp}` `SyncVoice`: plays `/Game/Audio/Voice/<cue>` from the speaker's place.
  - Septima and Ophel stand at the rail, left and right; Venno at the grille; a rival at the rival's position; the
    prologue is positionless.
  - A cue with no asset is skipped silently (`MAGEVR_VOICE ... asset=none, skipped`).
- `Tests/VoiceTests.cpp`, `MageArena.Voice.*`: TenWordsOrFewer, TriggerPerRow, MissingAssetNoStall, AssetsImported.
- `Tests/SessionTests.cpp` FullSeated: the rushed day exactly as before, then a natural day (`SetScriptNatural`) logged as
  `dayNaturalS`. It is not a gate.

### Acceptance, real output

- `build.ps1`: `Result: Succeeded`, exit 0. It compiled first time; the cold baseline took 1075.60 s.
- `MageArena.Voice`: it was red first on C2's own word count: `Expected 'BRE-02 words matches C2's count' to be 6, but it
  was 5.` C2 counts "Sunfall. Nothing catches this. Move." as 6 words; the text has 5 and is unchanged, so the
  manifest's `words` is 5. After that: `Found 4 automation tests`, 4 x Success, `EXIT CODE: 0`.
  `MAGEVR_VOICE assets unproven: no WAVs (0 of 27 rows, 0 assets imported)`.
- `MageArena.`: `Found 325 automation tests`, 325 x Success, `EXIT CODE: 0`. That is 321 plus the 4 Voice tests. The
  first run was 323/325: `Preset.StonePickSaves` and `Session.DayFlags` still waited 25 s for the old 20 s ritual
  timeout, and both were fixed. The overlay goldens did not move, so there was no re-record.
- `MageArenaDesign.`: `Found 7`. Only `Duel.FireSeated` and `Session.FullSeated` are red, as accepted under DF-011.
  There is no new red, and the count is still 7 because the natural day runs inside FullSeated.
- `import-voice.ps1`: with no WAV it printed `nothing to import` and exited 0. `-Force` gave
  `MAGEVR_IMPORT_SFX done kind=voice imported=0 unchanged=0 missing=27 failed=0 -> /Game/Audio/Voice`.
- `import-sfx.ps1`: `unchanged=20`, so the SFX path stays idempotent. See the note at the end.
- D-G4 (`capture-budget.ps1 -Runs 3 -ExtraArgs -MageArenaCreatureImportScene`, the creature cast copied in and deleted
  afterwards): draw calls max 126 / 126 / 126 (median 122), parity hash 37cb3568, `budget: ok (3 runs)`. That is +0.
- `format-check OK: 19 changed C++ file(s)`, `unused-members OK: 631`, `pin OK: 9 files @ baeac66`,
  `conformance OK: 45 vectors`.
- The capture acceptance line (`capture-colour-audio.ps1 -Run T27`) was **not run**: with no WAV there is no voice to
  capture.

### The day, rushed and natural (C2 section 4, target 420-600 s)

`MAGEVR_MEASURE Session.FullSeated dayWithTeachS=212.31 target=420.00-600.00 OUTSIDE` (rushed; 205.31 before, +7.00 s
for the ritual's 10 s floor).

`MAGEVR_MEASURE Session.FullSeated dayNaturalS=305.72 target=420.00-600.00 OUTSIDE` (natural).

Both days stop at the bout-3 loss (DF-011), so neither is a whole day.

| Beat | Rushed s | Natural s | C2 floor / natural |
|---|---|---|---|
| prologue | 6.89 | 36.89 | 6 / 36 |
| teach | 63.31 | 63.31 | 63 measured |
| ritual | 10.56 | 38.56 | 10 / 38 |
| intros 1 / 2 / 3 | 8 / 8 / 12 | 8 / 8 / 12 | 8 / 8 / 12 |
| intermissions 1 / 2 | 3.01 / 3.01 | 16.01 / 18.01 | 3 / 16, 3 / 18 |
| bouts 1 / 2 / 3 (fight) | 39.38 / 27.47 / 29.00 lost | 39.38 / 27.78 / 36.10 lost | band 145-235 for four |

Arithmetic only, not measured: the natural day misses intermission 3 (18), intro 4 (12), the final's fight, two missio
timeouts (about 5 each), the tablet (16) and the closing card (31). With a final of 45-80 s (the pinned band) a
completed natural day would be about 305.72 + 87 + 45-80, which is 438-473 s, inside 420-600. The rushed day would be
about 212.31 + 3 + 12 + 10 + 16 + 6 + 45-80, which is 304-339 s, outside, as C2 predicts.

Natural moved bouts 2 and 3 (27.47 -> 27.78 s, 29.00 -> 36.10 s): the intermission idle steps,
`docs/design-findings/DF-013-intermission-idle-steps-shift-bouts.md`. Freezing the kernel in intermissions (option a)
decouples them and restores 39.38 / 27.50 / 42.50 s on both paths; re-seeding the random stream (option b) does not
decouple them. It needs an owner ruling.

### Decisions the card did not specify

1. **The prologue's and the ritual's fallback texts were kept, as C2 section 5 says.** Section 10's example shows
   RIT-01's text in `story.ritual.septima`; section 5 says the placeholders stay as fallbacks. Section 5 was followed:
   with the slice loaded the RIT and PRO lines show; without it the fallbacks show.
2. **The tablet's marker is `unvoiced`.** C2 gives the tablet heading no cue (section 9 has no row; `slice.json` cue is
   null). DayStrings allows `unvoiced` only for a line `slice.json` also carries with no cue.
3. **The ritual's offer counts from its 10 s floor,** not from the offer line (which now stands from 26 s), so the
   rushed path is C2's floor column.
4. **OPH-10 has one trigger,** the closing card's second line (`closing#2`), shared by the won and lost variants. A day
   closed before that line still shows it: the closed phase keeps the card's last line.
5. **The speakers are greybox anchors.** No rail, grille or gate mesh exists yet. Septima and Ophel stand 150 cm left and
   right of the rail prompt; Venno at the far end of the oval, 4.5 m up.
6. **The taunts are voice only** (C2 section 7) and fire in the day only, when the kernel's phase event has a matching
   `atHpFraction`.
7. **Not done:** the chorus beats (they are not cue-sheet rows), and no `.gitkeep` under `Content/Audio/Voice/` (no
   asset exists).

Note for the App Master: on master, 15 of the 35 rows in `art/audio/sfx/manifest.json` (the conscript, slinger, hound
and maw cues, gate and cheer) are not imported. Running `import-sfx.ps1` imports them as new untracked `.uasset` files.
This lane removed the 15 it created, because they are outside its paths.
