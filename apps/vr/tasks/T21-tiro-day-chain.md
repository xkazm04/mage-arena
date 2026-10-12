# T21 - The Tiro day: ritual, four bouts, aftermath, in one session

Status: done, FullSeated red (verified 2026-10-07: MageArena. 206/206; the day loses Bout 1 live and Bout 3 to Brennic on the proposal; 3.0 min with the teach against 7-10)
Max turns: 320

## Goal
Owner decision 2026-10-07 (`docs/DECISIONS.md`, "v2 vision accepted": the slice is a full Tiro day). Today the session
is cold -> teach -> Bout 1 soldiers -> Bout 2 creatures -> Bout 3 the Fire duel -> victory. After this card it is
**the Tiro day**: cold -> (first launch: prologue placeholder -> teach) -> **collar ritual** -> Bout 1 soldiers ->
intermission (the T20 Tide Orb pick) -> Bout 2 creatures -> intermission -> **Bout 3 semifinal: Brennic** (Fire, the
T19 phases) -> intermission -> **Bout 4 final** (pinned Tiro wave 4, competence 1.5) -> **aftermath**. The final's
opponent is a seam: the Fire kernel's second Ember entrant (**Corvo**, the DECISIONS fallback) until T23's Air rival
lands, then a one-line data switch. Story text is placeholder for card C2; every structure is built now.

## Read first
- `CLAUDE.md`, `docs/DECISIONS.md`, `docs/design/V2-ROADMAP.md` (lane A row 5), `docs/design/V2-VISION.md` (waves 1-2:
  the ritual, the slice day, the slice ending), `docs/campaign/04-vr-campaign-design.md` sections 1-3 (staging, text
  rules: world-locked, 10 words or fewer per line, one line at a time; choices on the stones, never blocking).
- Cards and their verification notes: T13 (teach, pause, save), T15 (session chain), T18, T19, T20.
- Data: pinned `arena-tiers.json` Tiro waves 1-4, `combat.json` `betweenWaves`, `apps/vr/data/vr/strings.json`,
  `teach.json`, `presets.json`, `combat.vr.json` (`rivals`).
- Code: `Session/SessionFlow.cpp` (phases ~367, `AdvanceArc` ~1237, save `bout.txt` ~177-257, `BothPalmsRaised` ~463),
  `Session/ArenaSession.{h,cpp}` (`SetBout`, `bFireMages`), `Session/SessionPresentation.cpp` (prompts, stones),
  `Kernel/Games.cpp` (`SpawnWave`), `Tests/SessionTests.cpp` (`FullSeated`, `Chain*`).

## Deliverables
1. **Session phases** (names are yours, document them): `prologue` (first launch only, before the teach: a static
   placeholder tableau line for up to 10 s, skippable by palming the centre stone), `ritual` (before Bout 1 on every
   launch: Warden Septima's placeholder line, then the player offers both wrists - the existing both-palms pose - held
   for `teach.json` `offerHoldS`; a soft timeout of 20 s continues anyway), the four bouts with intermissions between
   each (the T20 pick stays after Bout 1 only), and `aftermath` (after the final, win or lose: a greybox stone tablet on
   the dais showing per-bout results and times, then the closing placeholder line; palm the centre stone to end).
2. **Bout 3 and Bout 4 opponents**: Bout 3 is Tiro wave 3 with Brennic (Fire, phases from `rivals.brennic`). Bout 4 is
   Tiro wave 4 (competence 1.5); a new `combat.vr.json` key (for example `tiroFinal: { school: "fire", rival: null }`,
   with `_why` citing the DECISIONS cut rule) makes it a Fire mage with no phases (Corvo) until T23; switching it to
   `{ school: "air", rival: "lio" }` must be the only change T23's merge needs. Validate the key strictly.
3. **Strings**: every new line in `strings.json` under `story.*` keys, each 10 words or fewer, each with a sibling
   `_story.<key>: "placeholder for C2"` marker so C2 can find them. Lines appear world-locked on the dais rail, one at a
   time, never head-locked.
4. **Save and resume**: `bout.txt` stores the next bout across all four (and the T20 pick); continue from any bout;
   defeat in any bout offers that bout again; the ritual replays on every launch, the prologue and teach only on the
   first. Old save files still load.
5. **Arena flags for later cards** (no consumers yet): after each bout write `arena.tiro.1.w<n>.won`, `.attempts`,
   `.perfects`, and `.time` into a session-owned flag map (in memory plus a small JSON next to `bout.txt`), following
   `04-vr-campaign-design.md` section 4. T22 (collar ledger) and T28 (scenes) read them.
6. **Tests**:
   - Regular `MageArena.Session.Day*`: the phase order on a first launch and on a resume; the ritual both-palms hold
     and its 20 s timeout; the final is wave 4 at competence 1.5 with a Fire mage and no rival under the default key,
     and an Air value fails the load until an Air school exists (or is accepted, if T23 is merged first - say which);
     save and load at each bout; defeat in Bout 4 offers Bout 4; flags written per bout with hand-checked values.
   - `MageArenaDesign.Session.FullSeated` plays the whole day (ritual auto-held by the script, prologue and teach
     skipped as today) live and with `-MageArenaProposal`; it logs per-bout outcome and time, the total with and without
     the teach, against the plan's 7-10 minutes. Measured, not tuned; it may be red.
7. **Capture** into `runs/T21/shots/`: the ritual (prompt and both palms), the Bout 3 intro, the Bout 4 intro, the
   aftermath tablet. Look at each and describe it.

## Constraints
- Do not commit, push, or touch any repository other than this one. No paid services. Do not edit
  `docs/DECISIONS.md`, `apps/vr/data/pinned/` or `calibration-proposal.json`. No combat number changes.
- Keys play clips only; debug keys stay F-keys. Regenerate the sigil corpus as `CLAUDE.md` says before the full suite.
- One Unreal build at a time; one `Automation RunTests` filter per editor process. Do not end your turn while a build,
  test or capture is running. Never write placeholder evidence (placeholder story text is fine and is marked).

## Acceptance (the orchestrator re-runs these and looks at every still)
```
powershell -NoProfile -File apps/vr/tools/build.ps1
UnrealEditor-Cmd MageArenaVR.uproject -ExecCmds="Automation RunTests MageArena.;Quit" -unattended -nullrhi -nosplash -log
UnrealEditor-Cmd MageArenaVR.uproject -ExecCmds="Automation RunTests MageArenaDesign.Session;Quit" -unattended -nullrhi -nosplash -log   # + -MageArenaProposal
<the capture command you add>   # stills in runs/T21/shots/
```
(The conformance generator is broken on every branch, see BACKLOG; the 45 `MageArena.Kernel.Conformance.*` tests are
the conformance evidence.)

## Report
`runs/T21/REPORT.md` (or the final reply, if the worker cannot write it): files, every acceptance command with real
output, the measured day (per bout and total, live and proposal), a description of each still, and every decision the
card did not specify.

## Orchestrator verification (2026-10-07)
A Claude subagent implemented the card in worktree `v2/T21` (cut once by a usage limit and resumed from its working
tree). The orchestrator re-ran the build (green) and `MageArena.` (206 Success, `EXIT CODE: 0`) and looked at the
aftermath tablet still. Mutation checks failed the Day tests as expected. Measured day (FullSeated): live loses Bout 1
at 23.25 s (known); the proposal wins Bouts 1-2 and loses Bout 3 to Brennic at 35.82 s while carrying Bout 2's damage
(the earlier green 103 s baseline ran Bouts 3-4 as water proxies), 3.02 min with the teach - added to DF-006. Accepted
worker choices: `day.json` pacing file, a 3 s un-stepped intro before every bout, the save kept on continue (a T13
change, needed to know the teach is done), flags reset at the next day's Bout 1, two real input fixes (a lingering
both-palms clip name; a stale palms pose restarting the teach). For T23's merge: besides `tiroFinal`, the session's
school-to-kernel flag in `Start` and `ContinueIntermission` needs the Air case. Stills are staged (2.5 s bouts).
Presentation backlog: the cuff overlaps the tablet's last row; the hands stay raised after the last clip frame.
