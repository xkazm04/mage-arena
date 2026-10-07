# T26 - Element-correct colour, and the first audio: A05 SFX and threat cues, spatialised

Status: open
Max turns: 300

## Goal
Owner decision 2026-10-07 (`docs/DECISIONS.md`, "v2 vision accepted"; `V2-VISION.md` wave 3 signature moments:
"element-correct colour plus threat audio"). Two gaps the pitch promises and the build lacks: **fire renders
turquoise** (every magic-family threat draws in the Water colour, including Brennic's bolts and the hound embers), and
**the build has no audio at all**. After this card each element draws in its own colour wherever it is a threat or a
cast, and the 18 A05 SFX play from their sources, spatialised, with the threat cues placed at the threat. The greybox
rules hold: no art pass, the threat language stays (element colour = absorb, steel = dodge, black core with a red rim
= leave), and the colour language is doubled by sound (plan section 6.7).

## Read first
- `CLAUDE.md`, `docs/DECISIONS.md`, `docs/design/V2-ROADMAP.md` (lane B row 11), `docs/PROJECT-PLAN.md` sections 6.3 and
  6.7, `docs/gameplay/04-threat-language.md`, `apps/vr/tasks/A05-sfx-starter-pack.md`, `apps/vr/art/audio/sfx/cues.json`
  and `manifest.json` (18 cues, 48 kHz 16-bit, mono except the stereo crowd loop; Git LFS), the BACKLOG audio item
  (peak-normalised only: balance relative loudness when wiring).
- Code: `Session/SessionPresentation.cpp` (`WaterColour` ~36, `FireColour` ~38, `FamilyColour` ~108 and its callers,
  the T23 `AirColour` and `ThreatColour`, projectile meshes defaulting to Water ~198, the T18 ember drawing),
  `Session/ArenaSession.cpp` (event cursor), `Kernel/SimTypes.h` (`FArenaEvent` kinds), `MageArenaVR.Build.cs`,
  `apps/vr/data/vr/styles/` (style overrides of the colours).
- Registry (read-only, before designing the mix): `ai-registry` subject for spatial audio scene authoring (resolve via
  `<registry>/knowledge/<domain>/index.json`, see `.claude/rules/ai-registry-access.md`).

## Deliverables
1. **Colour**: one function decides a threat's or cast's colour from the **owner's school** and the family: Water
   turquoise, Fire ember orange, Air the T23 wind green, Earth reserved; steel stays steel; an unblockable stays black
   with a red rim whatever its school. Player-owned casts use the player's school (Water). Hound embers are Fire-school
   magic, so ember orange. The style files keep overriding the base colours. Remove every other path that picks Water by
   default for magic.
2. **Audio import**: the 18 WAVs imported as `USoundWave` assets under `Game/Content/Audio/SFX/` (committed through LFS
   per `.gitattributes`), by a repeatable script (an editor commandlet or Python in the editor), not by hand; the script
   is in `apps/vr/tools/` and reruns idempotently. A data table `apps/vr/data/vr/audio.json` maps kernel and session
   events to cues with a per-cue gain (dB) that balances the peak-normalised pack (impacts loud, UI and Flow quiet; the
   values with a `_why`), attenuation and concurrency limits.
3. **Playback**: a small audio component on the session presentation plays cues from the event stream (the cursor
   pattern), **spatialised at the source** (the projectile, the telegraph, the enemy, the cuff for UI cues), with
   concurrency limits so a Squall or a soldier wave does not stack 20 voices. Threat cues: `fire-bolt-incoming` on an
   incoming fire projectile, `unblockable-telegraph` on any unblockable telegraph, `hound-snarl` on a hound spit windup,
   ward raise, hold loop and absorb, `perfect-absorb`, `blink`, `hit-taken`, casts, `sigil-*`, `crest`, `flow-stack`,
   `tier-unlock` (also on a phase surge), `ui-pause`, and the crowd loop under the whole bout (louder on a phase break).
   Muted under `-nullrhi` and in unattended tests without failing.
4. **Tests**: regular `MageArena.Colour.*` (each school's projectile, telegraph and cast colour; unblockable always black
   with a red rim; the hound ember orange; a style override wins) and `MageArena.Audio.*` (every `audio.json` cue names an
   existing asset; each mapped event produces exactly one play request with the right cue and position, through a
   test seam that records requests instead of playing; the concurrency cap holds under a Squall).
5. **Capture**: stills into `runs/T26/shots/` of a fire bolt, a hound ember, an Air Squall and a Water bolt in one or two
   frames side by side; and an audio capture: a 30 s WAV or video with sound of the Brennic duel opening
   (`runs/T26/audio/`). Look at the stills; describe what is heard at each cue in the report (time, cue, source).

## Constraints
- Do not commit, push, or touch any repository other than this one. **No paid services**: the SFX already exist; do
  not generate audio. Do not edit `docs/DECISIONS.md`, `apps/vr/data/pinned/` or `calibration-proposal.json`. No
  combat number changes.
- Regenerate the sigil corpus as `CLAUDE.md` says before the full suite.
- One Unreal build at a time; one `Automation RunTests` filter per editor process. Do not end your turn while a build,
  test, import or capture is running. Never write placeholder evidence.

## Acceptance (the orchestrator re-runs these, looks at every still and listens to the audio capture)
```
<the import command you add>   # idempotent: a second run changes nothing (git status clean)
powershell -NoProfile -File apps/vr/tools/build.ps1
UnrealEditor-Cmd MageArenaVR.uproject -ExecCmds="Automation RunTests MageArena.;Quit" -unattended -nullrhi -nosplash -log
<the capture command you add>   # stills in runs/T26/shots/, audio in runs/T26/audio/
```
(The conformance generator is broken on every branch, see BACKLOG; the 45 `MageArena.Kernel.Conformance.*` tests are
the conformance evidence.)

## Report
`runs/T26/REPORT.md` (or the final reply): files, every acceptance command with real output, the gain table and why,
a description of each still and of the audio capture, and every decision the card did not specify.
