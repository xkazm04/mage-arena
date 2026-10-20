# T40 - Import the two music loops and prove they play

Status: open, blocked on an owner input: the source of the two loops (see "The owner input"). The import and the proof run on
the build machine (Wolf, the x64 UE host: UE 5.8.3 editor). Written 2026-10-20 on the ARM64 laptop, where none of it runs.
Max turns: 120

## Goal
The session has two music loops, one for the arena and one for the intermission and aftermath, imported as `USoundWave`
assets and playing through the same audio route as the SFX. It moves K3 row 9 ("two music loops", 0: none imported,
`docs/research/KPI-2026-10-20.md`) and, with the capture, the audio half of K3 row 8. Whether the music sounds right is the
owner's call (K3 row 15, felt) and this card does not claim it.

## The owner input (left open on purpose)
Where the two loops come from is the owner's decision, not the builder's. The plan's route is ElevenLabs music, two 60 s loops
(`docs/PROJECT-PLAN.md` section 8, P13; `docs/DECISIONS.md` 2026-10-02: ElevenLabs for music and SFX). Stock music and Fab
packs are a risk to the originality rule (plan section 12, Q8) and are not allowed. The builder calls no service and reads no
key. The owner (or the App Master on the owner's go, with the generation count stated: 2) puts the loops in
`apps/vr/art/audio/music/` with a `SOURCE.md` (service, prompt, seed or version, date); the builder starts from that.
If the folder is empty, stop and report blocked.

## Read first
- `CLAUDE.md`, `docs/DECISIONS.md` (they outrank this card if they conflict)
- `apps/vr/tasks/T26-element-colour-and-first-audio.md` (the route this card extends)
- `apps/vr/tools/import-sfx.ps1` (idempotent import through the `ImportSfx` commandlet, `Tools/ImportSfxCommandlet.cpp`, ids from
  `apps/vr/art/audio/sfx/manifest.json`), `apps/vr/tools/capture-colour-audio.ps1` (30 s master-submix capture), and the
  existing crowd loop `apps/vr/art/audio/sfx/arena-crowd-loop.wav` as the loop-flag precedent
- Code: the T26 cue player and `Session/SessionPresentation.cpp`; `Tests/AudioTests.cpp`

## Deliverables (exact paths)
- `apps/vr/art/audio/music/` - the two WAV loops (LFS), `manifest.json` in the SFX manifest's shape (id, file, loop true) and
  `SOURCE.md`; all supplied as the owner input above, committed as given. Loudness balance is measured and reported, not changed
- The commandlet and `apps/vr/tools/import-sfx.ps1` extended with a folder choice (or a sibling `import-music.ps1`), still
  idempotent by source MD5 and loop flag; assets under `apps/vr/Game/Content/Audio/Music/`
- Session wiring: the arena loop plays from the start of a bout to its end, the second loop in intermissions and the aftermath;
  a 1.0 s crossfade at the switch; music sits below the threat cues (a stated bus or volume value, with the SFX loudness item
  in `apps/vr/tasks/BACKLOG.md` as the reason); both loops non-spatialised music, threat cues stay spatialised
- `Tests/AudioTests.cpp` (or `Tests/MusicTests.cpp`): `MageArena.Audio.Music.*` - both assets exist, both loop, the arena loop is
  active during a bout and the second in an intermission, and no threat cue loses spatialisation

## Constraints
- Do not commit, push, or touch any repository other than this one.
- **No new external services; the builder calls none** and never reads `ELEVENLABS_API_KEY`. Do not edit `docs/DECISIONS.md`.
- Every asset passes the P15 ceilings for audio if a ceiling exists for it (`docs/PROJECT-PLAN.md` section 8, P15); report the
  file sizes against it.
- Binary assets go through Git LFS (`.gitattributes`). Do not touch `apps/vr/data/pinned/` or the conformance vectors.
- One Unreal build at a time. Known red on a clean tree (`CLAUDE.md`): `MageArenaDesign.Duel.FireSeated`, `Duel.AirSeated`,
  `Session.FullSeated`, `Session.Wave1Seated`, `Session.CreaturesSeated`. Any other change is a regression.
- Changed C++ lines pass `node apps/vr/tools/lint/format-check.mjs`; new members pass `node apps/vr/tools/lint/unused-members.mjs`.

## Acceptance (the orchestrator re-runs these; quote their real output in your report)
```
powershell -NoProfile -File apps/vr/tools/build.ps1                     # expected: exit 0
powershell -NoProfile -File apps/vr/tools/import-sfx.ps1 -Run T40       # expected: MAGEVR_IMPORT_SFX done (both loops imported); a second run writes nothing
UnrealEditor-Cmd MageArenaVR.uproject -ExecCmds="Automation RunTests MageArena.Audio;Quit" -unattended -nullrhi -nosplash -log
                                                                         # expected: all MageArena.Audio.* green, including Music
powershell -NoProfile -File apps/vr/tools/capture-colour-audio.ps1 -Run T40 -SkipStills
                                                                         # expected: MAGEVR_CAPTURE_DONE; runs/T40/audio/*.wav peak above -60 dBFS with music present
node apps/vr/tools/lint/format-check.mjs && node apps/vr/tools/lint/unused-members.mjs
                                                                         # expected: OK
```

## Report
Write `runs/T40/REPORT.md`: the source of the loops as the owner gave it, what you built (paths), each acceptance command with
its real output, file sizes and loudness numbers, anything you could not do and the exact error, and decisions the card did
not specify. State plainly that the sound was not heard by a person.
