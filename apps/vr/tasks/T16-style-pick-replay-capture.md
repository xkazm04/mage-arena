# T16 - Style-pick capture: one duel replay re-skinned in styles 13, 25 and 30 (P18)

Status: open - deliverable 2 done (f933f5c, style skins + sampler; not yet loaded in the engine); 1 and 3-6 need the build machine
Max turns: 320

## Goal
Gate **Mon 19 Oct, style pick** (`docs/PROJECT-PLAN.md` section 5 row D3, tool row P18): the owner chooses the art
style from **one duel replay re-skinned in the three shortlisted styles** - 13 Screen-Print, 25 Moonlit Silver,
30 Chalk & Slate (`docs/DECISIONS.md`) - **unlit**, recorded as desktop review video. Plan acceptance for P18: "the
replay agrees with the event log at every cast; three skins show identical event times". This card is presentation
and tooling only; it does not depend on the balance decisions (DF-003, DF-004) and changes no combat number.

## Read first
`CLAUDE.md`, `docs/DECISIONS.md` (style shortlist, threat colours), `docs/PROJECT-PLAN.md` sections 5 (D2-D3), 6.7
(colour language doubled by shape and sound) and the P18 row; art `apps/vr/art/style-studies/` (13-screen-print,
25-moonlit-silver, 30-chalk-slate: duel, key art, glyphs, Fire mage turnaround, arena wide, cuff HUD, threat language,
enemy lineup), `apps/vr/art/index.html`, `apps/vr/art/audio/sfx/`; code `Session/*` (presentation, captures),
`Greybox/*`, `Hands/*`; existing capture scripts `apps/vr/tools/capture-*.ps1`.

## Deliverables
1. **Deterministic replay with an event log**: run the seated Fire duel (competence 1, seed 1, the reference seated
   script on the live overlay) once and write an event log (`runs/T16/events.jsonl`: tick, sim time, event kind -
   cast, hit, absorb, perfect, blink, wall, death - with ids). Replaying the same seed and script must reproduce the
   identical log (byte-identical) and the identical kernel state hash at the end. A regular test
   `MageArena.Replay.Deterministic` proves it (two runs, same log, same hash).
2. **Style skins as data**: `apps/vr/data/vr/styles/style-13.json`, `style-25.json`, `style-30.json` - unlit colour
   palettes and simple material parameters (base colours, outline/edge treatment, ground tone, sky tone) for the arena,
   dais and pads, soldiers/creatures/Fire mage, projectiles, telegraphs, the dome and wall, hands and the cuff. Sample
   the colours from the style-study images (write down which image and region each colour came from in a `_source`
   field). **Keep the threat language**: magic = ring, steel = angular, unblockable = black core with red rim - the
   style may recolour within each family but never swap their meaning or shapes. A skin is selected with
   `-MageArenaStyle=13|25|30` (default greybox, unchanged).
3. **Review capture (P18)**: a script `apps/vr/tools/capture-style-pick.ps1` that, for greybox and each style, plays
   the same replay in `-game` at 1920x1080, records 60 fps frames, and encodes `runs/T16/style-<id>.mp4` with ffmpeg,
   with an on-video overlay of the sim time and the last event from the log (clip fired, recognizer result, cast name).
   Add the starter SFX on cast, hit, absorb and perfect if the A05 files can be played without new assets; otherwise say
   so. Also write 4 stills per style at the same four event ticks (`runs/T16/shots/<id>-0N.png`).
4. **Identical event times across skins**: each capture writes its own event log; a check (script or test) asserts
   the logs of greybox, 13, 25 and 30 are identical - skins change pixels, never the simulation.
5. **Owner review page** `apps/vr/art/style-pick/index.html` (linked from `apps/vr/art/index.html`): the three videos
   side by side (plus greybox), the four stills per style in a grid at the same moments, the event list, and one line
   per style on what was kept from its study. Videos and stills referenced from `runs/` are not committed - copy the
   final MP4s and stills into `apps/vr/art/style-pick/` (LFS covers mp4/png? check `.gitattributes` and add rules for
   the new folder if needed).
6. No regression: conformance byte-identical; `MageArena.*` green; design gates unchanged with no style flag.
7. `runs/T16/REPORT.md` from real outputs: files, the event-log equality results, video durations and sizes, and a
   short description of what each style still shows.

## Evidence rules (binding)
Run every command yourself and wait for it to finish; quote real output. **Never write placeholder or simulated files**
(no dummy PNG/MP4). If a step cannot be done, write that in the report and stop rather than faking it. Do not end
your turn while a build, test or capture is running.

## Constraints
Do not commit, push, or modify other repositories; do not edit `apps/vr/data/pinned/`, the style-study images, the
calibration proposal or combat numbers. Unlit only. One Unreal build at a time; one `Automation RunTests` filter per
editor process.

## Acceptance (the orchestrator re-runs these and watches the videos)
```
node apps/vr/tools/conformance/generate.mjs     # byte-identical
powershell -NoProfile -File apps/vr/tools/build.ps1
UnrealEditor-Cmd MageArenaVR.uproject -ExecCmds="Automation RunTests MageArena.;Quit" -unattended -nullrhi -nosplash -log
powershell -NoProfile -File apps/vr/tools/capture-style-pick.ps1    # 4 MP4s + 16 stills + 4 identical event logs
```

## Retry 1 (orchestrator, 2026-10-04)

Verified: build green, conformance byte-identical, overlay/proposal untouched, `MageArena.` 142/142 incl.
`MageArena.Replay.Deterministic`. The orchestrator re-ran `capture-style-pick.ps1` to completion (your session ended
while it ran and its background tasks were killed). Result (`runs/T16/cmp/grid-*.jpg`, frames 300/1200/2400/3600 of each
skin side by side): **the capture never runs the duel.** Every frame of all four videos shows `Sim: 0.00s | Last: (Actor 0)`,
an empty arena, no hands, no Fire mage. Fix these; evidence rules above still bind.

1. **The simulation does not advance in `UStylePickCaptureDriver`** (`Session/StylePickCapture.cpp`, `Start()` ~26-60 and
   the tick): the session is started (`SetBout(2, true)`, `Start(1)`) but never stepped. Step the scripted session by
   exactly one kernel tick (1/60 s) per captured frame, so frame N is sim tick N in every skin; assert sim time > 0 after
   the first second and log it. Use the seated Fire duel at competence 1, seed 1 (as the card says); confirm the Fire mage
   and the hands are on screen.
2. **Each run ended on the 12-minute script timeout**, so the end-of-run event log was never written, no stills were
   cut, and the "identical" check passed on missing files. Write `runs/T16/events-<style>.jsonl` as events happen (append
   per tick) or end the run deterministically at a fixed tick count (e.g. 3600 ticks = 60 s) so it always completes well
   inside the timeout; make the script **fail** (non-zero exit, clear message) if any of the four logs is missing or
   differs; cut the 4 stills per style at the same event ticks.
3. **The skins barely differ** (`grid-*.jpg`): only the floor tint changes; the sky band, roof, stands and walls stay the
   greybox orange/blue/grey in every style. Apply the style data to every greybox surface (sky panels, stands, walls,
   roof, banners, braziers, ground), so each skin reads as its study at a glance - still unlit, still the threat language.
4. Run the full capture yourself **in the foreground and wait for it** (about 4 x 12 min is too long: with item 2 each
   run should take a few minutes). Look at the frames and stills; describe them in `runs/T16/REPORT.md` with real
   numbers (frames, durations, event counts, the md5 of each event log).
