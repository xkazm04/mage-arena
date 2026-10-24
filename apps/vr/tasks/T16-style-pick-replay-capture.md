# T16 - Style-pick capture: one duel replay re-skinned in styles 13, 25 and 30 (P18)

Status: **done up to the owner's pick** on Wolf (kaziho) 2026-10-24, UE 5.8.3 CL 58210709, commits `60215f6` (style on
every greybox surface), `603bd32` (a style that applies nothing FAILs), `122e480` (stills span the duel), `3f6c5bd`
(review page and media). `capture-style-pick.ps1` exit 0, four identical event logs (md5
`7b0715c871ac3399504c4f7a744896d4`); `MageArena.` 273/273; `MageArenaDesign.` 5/7, only DF-011's two accepted reds. See
"Result" at the end. **OWNER:** the pick of 13, 25 or 30 from `apps/vr/art/style-pick/index.html` (WOLF-PLAN P4) is open.
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

## Result (Wolf kaziho 2026-10-24, UE 5.8.3 CL 58210709)

Built from the worktree with `build.ps1`, `Result: Succeeded` at every step. One Unreal process at a time. The full
report with every quoted line is `runs/T16/REPORT.md` on Wolf (gitignored); this section is its committed record.

**Retry 1, item by item.**
1. *The sim advances one kernel tick per frame:* already done by T45 (`191cad0`). The driver starts the duel through
   `UArenaSessionSubsystem::Start`, the launcher runs `-usefixedtimestep -fps=60`, and a frame whose tick is not the
   previous one plus one FAILs. Today, every run: `MAGEVR_STYLEPICK sim after 1s: 1.000000`, `end seq=1870 tick=1870`.
2. *A deterministic end; failing on a missing or differing log:* already done by T45 (`191cad0`). The run ends at 3600
   frames or at the end of the duel, a run under 1200 frames FAILs, and the launcher exits 1 on a missing, empty or
   differing log. Added here: `603bd32` FAILs a style that applies no arena colour (a missing or mistyped file used to
   record the greybox under the style's name). Proof: `-Styles 99` logged `MAGEVR_CAPTURE_FAIL style 99 applied no arena
   colours (...style-99.json)`, then the launcher printed `FAIL events-99.jsonl is missing` and exited 1. The "differs"
   branch was read, not run: `-ExtraArgs` reaches every style, so no launch makes one style's log differ.
3. *The style on every greybox surface:* done here (`60215f6`). The 10-04 loader read `arena`, `groundTone`, `skyTone`
   and `dome` as colour arrays, but the f933f5c schema nests them, so the only arena colour a style changed was the flame.
   `FArenaLayout::ApplyStyle` now applies 17 colours: the 14 `arena` keys, with the wall split from the stands; the flame
   from `projectiles.fire`; and both sky tones, the middle band at their midpoint. Under a style the sun and the sky
   light become neutral greys of the greybox light's luminance. `arena.flame` is not read: style 25 samples it as the
   leave red (0.84, 0.07, 0.04) above a near-black brazier. Each styled run logged `arena style: 17 colours applied`.
   `MageArena.Greybox.StyleSkin` (new) holds it, including "no painted surface is the leave red".
4. *A full capture to completion with a real report:* done here. All four styles ran to completion in one invocation
   (about 21 min); the run held the turn, nothing was detached. `runs/T16/REPORT.md` is written from those outputs.

**Capture** (code `122e480`): `MAGEVR_CAPTURE_DONE shots=1870` x4, `stage=lost sim=31.167` (Brennic wins: DF-011's
accepted red). Four MP4s, 1920x1080, 60 fps, 1870 frames, 31.17 s, 2.65-2.95 MB. Sixteen stills at ticks 457 (hit),
926 (cast), 1399 (cast) and 1870 (death): events 50/101/151/202 of 203, a quarter, half and three quarters into the
log, plus its last event (`122e480`; event indices 2/5/15/30 had cut all four stills inside the first 5.2 s). Event logs,
203 events each (145 casts, 52 hits, 5 blinks, 1 death): greybox, 13, 25 and 30 all md5
`7b0715c871ac3399504c4f7a744896d4`, the log T45 found.

**The greybox default.** No `-MageArenaStyle` applies nothing (`WallColour == Stone`, one shared stands-and-wall ISM, the
dusk light). Measured against a baseline built from `85d104c`'s five C++ files in this worktree: the same event log;
1794 of 1870 frames byte-identical. The 76 that differ all fall within 40 ticks after an event: the pawn's blink fade
and the presentation's flashes run on `FPlatformTime::Seconds()` (wall clock), not on the sim tick. That variance was
there before this change. The steady frames are identical.

**Not done, by the card's own terms.** No SFX in the videos: the driver records frames only, and the colour-audio capture
is a separate driver. The overlay shows `Sim: <s> | Last: <kind> (Actor <id>)`, without the clip fired, the recognizer
result or the cast name. Bodies, the wrist cuff and the HP bar keep the greybox colours in every skin: most of the sampled
`figures` colours are near-black (max channel < 0.1), which T49 reserves for the unblockable core.

**Suites.** `MageArena.` 273/273 (272 + `MageArena.Greybox.StyleSkin`), including `MageArena.Replay.Deterministic`.
`MageArenaDesign.` 5/7: `Duel.FireSeated` and `Session.FullSeated` fail, the accepted reds of DF-011.
`check-pin.mjs`: `pin OK: 9 files @ baeac66`, `conformance OK: 45 vectors`.
