# T23 - Air as an AI opponent: Momentum, ten spells, Air Form, and the Gale rival in the Tiro final

Status: done with a design finding (verified 2026-10-07: MageArena. 203/203; AirSeated red on length, Lio median 21-24 s and the player wins every duel - DF-008)
Max turns: 400

## Goal
Owner decision 2026-10-07 (`docs/DECISIONS.md`: the second school for the Tiro final is **Air**; cut to Corvo if this
is not green by Fri 23 Oct). After this card the C++ kernel runs an **Air mage as an AI opponent**: the Momentum
resource from pinned `schools.json`, the ten spells of the VR proposal sheet, defence D (Air Form), an Air AI, and a
named Gale rival (**Lio**, slice default) in the **Tiro final** (wave 4, competence 1.5) with HP-gated phases and
**Tempest Lance** as the last-phase signature (the T19 rule). The player stays Water. Fire (`docs/schools/FIRE.md`,
card T09) is the template for every step.

## Read first
- `CLAUDE.md`, `docs/DECISIONS.md`, `docs/design/V2-ROADMAP.md` (lane A row 7), **`docs/schools/AIR.md`** (the design
  and its open points), `docs/schools/FIRE.md`, `docs/change-requests/CR-002-fire-school.md`, `CR-005-rival-phases.md`,
  `docs/design/SCHOOL-DEFENCES.md` section D, `docs/gameplay/04-threat-language.md`.
- Data: `apps/vr/data/vr/schools/spells-air.csv` (VR proposal, the source of every Air number), pinned `schools.json`
  `air.combatIdentity`, pinned `arena-tiers.json` Tiro wave 4 (final, competence 1.5), `combat.vr.json` (`rivals`).
- Code: `Kernel/Fire.{h,cpp}` (the school pattern: resource tick, catalog, casts, shapes, Sunfall granted cast
  `StartGrantedFireCast`), `Kernel/MageAI.cpp` (`ChooseFireSpell` ~263, `MageInput`, `AttachMageAI`), `Kernel/Games.cpp`
  (`SpawnWave`, Fire opt-in ~98-115), `Kernel/VrRivals.cpp` and `VrRules*` (rival binding and granted signature),
  `Kernel/Water.cpp` (Twin Tides shape, `ReflectProjectile`), `Session/ArenaSession.cpp` (`SetBout`, `bFireMages`),
  `Session/SessionPresentation.cpp` (`FamilyColour`, telegraphs, opponent bodies from T20), `Tests/FireTests.cpp`,
  `Tests/RivalTests.cpp`, `Tests/RivalCensusTests.cpp`, `Tests/SessionTests.cpp`.

## Deliverables
1. **Air in the kernel** (`Kernel/Air.{h,cpp}`, mirroring Fire): Momentum with the pinned gain, decay and thresholds
   (a non-air actor ignores it); `absorbDrainMult` 1.2; the perfect-absorb **deflect** (redirect along the aim, tier <=
   own tier); `spellsPierce` read as "pierces one extra target" (AIR.md open point 2). The CSV loads strictly (bad cell
   = load failure), the notes column unquoted as in Fire. Every spell row resolves:
   - reused shapes: Gale Dart, Shear (twin), Slipstream (dash, no trail), Downdraft (ground circle), Eye of the Storm
     (self lock, Furnace pattern), Tempest Lance (line, unblockable), Cyclone (channel, per-tick ids);
   - new shapes: **Veering Bolt** (a projectile on a fixed-curvature arc that enters 60 degrees off the line of sight;
     state the arc maths in a comment and keep it deterministic), **Squall** (four projectiles 0.25 s apart, each its own
     activation id), **Air Form** (3 s; a hit is evaded with chance 60 % physical, 30 % magic, drawn from the kernel RNG
     so replays stay deterministic; unblockables never evaded; needs Momentum >= 40 and spends 40).
2. **Air AI**: `ChooseAirSpell` on the Fire model, by competence: keeps Momentum up with Slipstream and movement, raises
   Air Form when a player cast is in flight and Momentum allows, prefers Veering Bolt and Squall at level 2+, Tempest
   Lance once tier IV is unlocked and the player is not blinking. Movement stays off the dais (CR-003 rules).
3. **The rival**: `rivals.lio` in `combat.vr.json` (school air, Tiro wave 4 final, phases 0.66 / 0.33, surge 1,
   `opensWith: air_tempest`), `_why` lines citing DECISIONS and AIR.md. The session's Tiro final spawns the Air mage
   (the semifinal stays Brennic, Fire); a session flag like `bFireMages` or the rival block decides, documented.
4. **Presentation (greybox)**: Air threats in the threat language (magic = ring, never black with a red rim except the
   unblockable Tempest Lance and its line telegraph). Air's canon colour `#5f9e98` sits close to the Water turquoise;
   pick a greybox Air colour that is clearly distinct from Water on screen (pale wind white-green, or add a streak
   pattern) and say what you chose. The Veering Bolt's arc and the Tempest Lance line are drawn during their telegraphs.
5. **Tests**:
   - Regular `MageArena.Air.*`, literals with sources: Momentum gain and decay over hand-counted ticks; both threshold
     effects; deflect on a perfect; each row's mana, cooldown, damage and telegraph; Veering Bolt's arc endpoint;
     Squall's four release ticks; Air Form's evasion over a fixed seed (count expected hits); unblockable not evaded;
     Tempest Lance as the granted signature at the last break; with Fire or Water mages nothing changes (the T19 digest
     test and all conformance vectors still pass).
   - `MageArenaDesign.Duel.AirSeated`: the seated player against Lio, competence 1.5, phases on; asserts the DF-003
     targets (length 45-80 s, signature cast in a duel reaching the last phase, player win). It may be red; report it.
   - Extend `MageArenaDesign.Census.Rivals` with the Lio duel (20 seeds, live and proposal, competence 1 and 1.5), CSV and
     summary to `runs/T23/census/`.
6. **No regression**: `MageArena.*` green; every `MageArenaDesign.*` before and after.
7. **Capture** into `runs/T23/shots/`: Lio on the floor, a Veering Bolt arc telegraph, a Squall volley, Air Form
   active, the Tempest Lance line telegraph. Look at each and describe it.

## Constraints
- Do not commit, push, or touch any repository other than this one. No paid services. Do not edit
  `docs/DECISIONS.md`, `apps/vr/data/pinned/` or `calibration-proposal.json`. Do not edit the Air CSV numbers: if a
  row cannot work as written, implement the closest literal reading, mark it **Owner:** in a new `## Kernel readings`
  section you append to `docs/schools/AIR.md`, and report it.
- Measure, do not tune. Regenerate the sigil corpus as `CLAUDE.md` says before the full suite.
- One Unreal build at a time; one `Automation RunTests` filter per editor process. Do not end your turn while a build,
  test, census or capture is running. Never write placeholder evidence.

## Acceptance (the orchestrator re-runs these and looks at every still)
```
powershell -NoProfile -File apps/vr/tools/build.ps1
UnrealEditor-Cmd MageArenaVR.uproject -ExecCmds="Automation RunTests MageArena.;Quit" -unattended -nullrhi -nosplash -log
UnrealEditor-Cmd MageArenaVR.uproject -ExecCmds="Automation RunTests MageArenaDesign.Duel;Quit" -unattended -nullrhi -nosplash -log     # + -MageArenaProposal
UnrealEditor-Cmd MageArenaVR.uproject -ExecCmds="Automation RunTests MageArenaDesign.Census.Rivals;Quit" -unattended -nullrhi -nosplash -log
<the capture command you add>   # stills in runs/T23/shots/
```
(The conformance generator is broken on every branch, see BACKLOG; the 45 `MageArena.Kernel.Conformance.*` tests are
the conformance evidence.)

## Report
`runs/T23/REPORT.md` (or the final reply, if the worker cannot write it): files, every acceptance command with real
output, the Lio census against the targets, the kernel readings marked Owner, a description of each still, and every
decision the card did not specify.

## Orchestrator verification (2026-10-07)
A Claude subagent implemented the card in worktree `v2/T23` (cut once by a usage limit and resumed). The orchestrator
re-ran the build (green) and `MageArena.` (203 Success, `EXIT CODE: 0`) and looked at the Veering Bolt still (the arc
swings wide and curves into the seat; Lio a dark-green column). Fire and Water results are byte-identical; Brennic's
census lines unchanged. Mutation check failed three Air tests as expected. Nine kernel readings are marked Owner in
`AIR.md`. Accepted choices: air colour a light wind green (distinct from the Water turquoise), the Tempest Lance as a
chest-height beam (a ground strip hides behind the dais lip), the Veering Bolt side by cast id (no RNG draw). Finding:
Lio is far too weak (DF-008). Pre-existing gap surfaced: Fire's area and line rows skip the planted-staff dome while
Air's pass it.
