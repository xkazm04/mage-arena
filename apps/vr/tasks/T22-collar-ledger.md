# T22 - The collar ledger: Cracks and Tithe

Status: done with a design finding (verified 2026-10-07: MageArena. 214/214; Tithe dominated by missed bolts - DF-009)
Max turns: 260

## Goal
Owner decision 2026-10-07 (`docs/DECISIONS.md`, "v2 vision accepted": the collar economy, Precision against
Spectacle). Every bout produces two quantities from kernel events: **Cracks** (clean play: perfect absorbs,
reflections, an unblockable dodged by a blink) and **Tithe** (magic spilled into the sand: the player's tier III-IV
area casts, casts that hit nothing, Crests). Cracks persist across Games days and will open the Breaking (spring 2027);
Tithe feeds the Wardstones (renown now, a stronger jailer later). After this card both are tallied, saved across days,
shown **diegetically** during play (greybox) and **exactly only on the aftermath tablet**. No combat number changes:
the ledger reads events, it never writes the kernel. The weights below are proposals the owner may change; they live in
data.

## Read first
- `CLAUDE.md`, `docs/DECISIONS.md`, `docs/design/V2-ROADMAP.md` (lane A row 6), `docs/design/V2-VISION.md` waves 1, 2
  and 4 ("One collar, two pulls", the diegetic feedback answer), `docs/campaign/04-vr-campaign-design.md` section 4.
- Cards T19 (phase events), T20 (`reflect` event), T21 (day phases, `ArenaFlags`, aftermath tablet, `bout.txt`).
- Code: `Kernel/SimTypes.h` (`FArenaEvent` ~375, `FMetrics`), `Kernel/ArenaKernel.cpp` (`Emit`), `Kernel/ArenaThreats.cpp`
  (perfect, hit, reflect), `Kernel/Water.cpp` (casts, Crest ~23-42, projectiles), `Session/ArenaSession.cpp` (the
  `EventCursor` consumers ~1434-1484), `Session/ArenaFlags.{h,cpp}`, `Session/DayFlow.cpp`,
  `Session/SessionPresentation.cpp` (cuff, tablet, stands), `Tests/DayTests.cpp`.

## Deliverables
1. **Ledger data** `apps/vr/data/vr/collar.json`, strict loader, `_why` on every number citing the DECISIONS entry and
   marked "proposal, owner sign-off pending":
   - Cracks: `perfect` 1, `reflect` 1 (a reflected projectile that hits its caster: +1 more), `unblockableBlinked` 2
     (an unblockable telegraph that resolves while the player is in blink i-frames or off its area).
   - Tithe: `areaCastTier3Plus` 2 (a player cast of tier III or IV with an area or multi-projectile shape), `missedCast`
     1 (a player projectile or area that expires without damaging anyone), `crest` 1.
   - Reserved keys with no source yet, accepted and ignored: `spare` (T25), `interrupt` (T29).
2. **Missing events** on the ruleset path only (the null path and conformance stay unchanged): `miss` (actor = owner,
   value = spell tier) when a player-owned projectile or area resolves with no damage; `dodge` when an unblockable
   resolves without hitting a blinking or out-of-area target. Say where each is emitted.
3. **Tally**: a session-side ledger reads the event stream through the existing cursor pattern, per bout and per day;
   writes `arena.tiro.1.w<n>.cracks` / `.tithe` into `ArenaFlags`; keeps lifetime totals in `collar.ledger.json` next to
   `bout.txt` (old installs without it start at 0; a corrupt file is treated like a corrupt save). Start over (both
   palms) clears the day's tally but **not** the lifetime Cracks (state why in a comment: the collar remembers).
4. **Diegetic feedback (greybox)**: on each Crack, a short light leak from the wrist cuffs (a bright seam flash, never
   black with a red rim); on each Tithe point, the Wardstones pulse - add two greybox Wardstone pillars at the arena
   edge, inside the front 70 degree arc, that glow brighter with the day's Tithe. No numbers on screen during a bout.
5. **Aftermath tablet**: add "Cracks" and "Tithe" columns per bout and the day's totals, plus the lifetime Cracks line.
6. **Tests** (`MageArena.Collar.*`), literals with sources: each Crack and Tithe source adds its weight once on a
   scripted event; a reflected hit adds the extra point; an area tier III cast that misses adds 2 + 1; the null-ruleset
   path emits no `miss` or `dodge` (conformance tests still pass); lifetime Cracks survive save, load and start over; the
   tablet text includes both columns. Also log Cracks and Tithe per bout in `MageArenaDesign.Session.FullSeated` and
   `CreaturesSeated` (measured, not asserted).
7. **Capture** into `runs/T22/shots/`: a Crack flash at the wrists after a perfect, the Wardstones glowing after Tithe,
   the aftermath tablet with both columns. Look at each and describe it.

## Constraints
- Do not commit, push, or touch any repository other than this one. No paid services. Do not edit
  `docs/DECISIONS.md`, `apps/vr/data/pinned/` or `calibration-proposal.json`. No combat number changes.
- Keys play clips only. Regenerate the sigil corpus as `CLAUDE.md` says before the full suite.
- One Unreal build at a time; one `Automation RunTests` filter per editor process. Do not end your turn while a build,
  test or capture is running. Never write placeholder evidence.

## Acceptance (the orchestrator re-runs these and looks at every still)
```
powershell -NoProfile -File apps/vr/tools/build.ps1
UnrealEditor-Cmd MageArenaVR.uproject -ExecCmds="Automation RunTests MageArena.;Quit" -unattended -nullrhi -nosplash -log
UnrealEditor-Cmd MageArenaVR.uproject -ExecCmds="Automation RunTests MageArenaDesign.Session;Quit" -unattended -nullrhi -nosplash -log   # + -MageArenaProposal
<the capture command you add>   # stills in runs/T22/shots/
```
(The conformance generator is broken on every branch, see BACKLOG; the 45 `MageArena.Kernel.Conformance.*` tests are
the conformance evidence.)

## Report
`runs/T22/REPORT.md` (or the final reply): files, every acceptance command with real output, Cracks and Tithe per
bout in the measured runs, a description of each still, and every decision the card did not specify.

## Orchestrator verification (2026-10-07)
A Claude subagent implemented the card in worktree `v2/T22`. The orchestrator re-ran the build (green) and
`MageArena.` (214 Success, `EXIT CODE: 0`) and looked at the Wardstones still (two violet pillars at the arena edge, no
numbers on screen). With `miss`, `dodge` and `reflectHit` filtered out, Brennic's duel matches the pinned pre-change
digest, so no combat changed. Mutation check failed `Collar.MissedCasts` as expected. Accepted choices: a third event
`reflectHit` (the only exact way to award the reflected-hit point), a miss per cast not per projectile, Crests read from
the kernel counter, lifetime Tithe kept as well as Cracks, Wardstones at +-24 degrees. Finding DF-009 (weights).
