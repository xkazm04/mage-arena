# T48 - Earth school and Footing in the C++ kernel: the full shape, a phase-1 slice, and the honest cost

Status: open (written on Lynx 2026-10-21, no UE run; **recommendation: do not build before 18 Nov** - read the Goal)
Max turns: 400

## Goal

Owner decision 2026-10-03 (`docs/DECISIONS.md` "Sigil shape, and all four schools in every kernel"): "all kernels need
all 4 schools - I would start on your side", and "VR starts". Fire landed as card T09, Air as T23. **Earth never got a
card and does not exist in code.** This card is the missing half of that ruling: Earth as a kernel school with its
Footing resource, its spells, a mage that can use it, and the greybox threat language.

**The orchestrator's recommendation, written for the owner to read first: cut this to after the 18 Nov submission.**
The evidence, all of it checkable:

1. **Earth has nowhere to appear in the shipping slice.** The Tiro day has exactly two mage slots and both are taken.
   Pinned `arena-tiers.json` `tiers[tiro].waves` gives wave `n: 3` `kind: "semifinal"` (one mage, competence 1) and
   `n: 4` `kind: "final"` (one mage, competence 1.5). `rivals.brennic.appliesTo` binds the semifinal (Fire) and
   `rivals.lio.appliesTo` the final (Air) in `apps/vr/data/vr/combat.vr.json`. An Earth rival either displaces Brennic
   or Lio, or it ships as a school nothing spawns. Neither is value.
2. **The owner already chose against Earth for that slot, with Earth on the table.** DECISIONS 2026-10-07:
   "**Second school for the Tiro final: Air** (owner, 2026-10-07, asked "Earth, Air, or Corvo now?": "Air")"
   (`docs/DECISIONS.md:323-324`). Nothing since has reopened it.
3. **The owner's own horizon for playable Earth is spring 2027, not November.** Same entry, last line: "Four routes,
   one per member of the Four (Cassia, Brennic, Garran, Iskar); Cassia's (Water) is the slice. **All four schools
   playable at launch (spring 2027)**" (`docs/DECISIONS.md:325-326`).
4. **It is not on the critical path and there is no slack on that path.** `docs/concepts/WOLF-PLAN.md` section c:
   "Critical path to 18 Nov: **P0 baseline -> P2 T39 -> P3 T37 -> P4 T16 -> P6 T38 -> P8 T41 and the gate record ->
   P9 device -> P10 submission**, with **P5 (first APK)** in parallel". Those phases already fill 22 Oct to 18 Nov at
   their own stated efforts, against risk 6 of section f: "**One Unreal host, one build at a time.** Wolf is a single
   queue; a long T39 delays everything after it." Earth is a 1.5-2 Wolf-day queue item at its smallest and 3.5-5 at
   full shape (see **Cost**), on the one machine that also owes the first APK and the device bring-up.
5. **The last school shipped with its balance unresolved and that tail is still open.** T23 ended in DF-008 ("Lio
   loses every duel in about 20-24 s"), which P2/T39 is still paying off. A third AI school buys a third such tail.

So: **the full shape is specified below so it is ready; phase 1 is specified so it can be started on an hour's notice;
and the orchestrator's advice is to run nothing from this card before the submission is in.** If the owner wants Earth
before 18 Nov anyway, build **phase 1 only** and accept that it is infrastructure, not a feature the judge video can
show.

## The one blocker the card cannot resolve

**Footing has no seated reading, and that is an open owner decision.** `apps/vr/tasks/BACKLOG.md:17` is still open:

> `- [ ] OWNER (design, before the Earth and Air cards) - proposals now in docs/design/SCHOOL-DEFENCES.md (Footing =
> staying on one pad; Momentum = consecutive blinks/casting cadence), confirm or change: in schools.json Earth's
> Footing builds while standing still and Air's Momentum while moving faster than 4 m/s. ... Decide before T10/T11.`

`docs/design/SCHOOL-DEFENCES.md` section B **proposes** "Footing builds while the mage **stays on one pad** (stillness
= not blinking)" and labels every number in the file "a starting proposal for the calibration sweep, not a balance
claim". A proposal is not a ruling. **Do not invent the reading and do not escalate it from this card** - it is a known
open owner item; name it in the report and stop at the gate.

**What that blocker does and does not block.** The pinned Earth rule is
`gain: { stackPerHalfSecondBelowMps: 1.0 }` - one Footing per half-second while moving below 1 m/s. T23 established the
precedent that this needs no seated reading for an **AI** opponent: `docs/schools/AIR.md` "Design intent", last bullet:
"**The AI is not seated.** The opponent stands on the arena floor and can move, so the pinned Momentum rule (speed
above 4 m/s) applies to it unchanged. The seated reading ... is only for a future playable Air." The same holds for
Earth, and it is why the phase split below works: **phase 1 (an Earth mage) is not blocked; phases 2 and 3 (a player
who casts Earth, and Stone form on a seated body) are hard-gated on the owner's Footing ruling.**

## The frozen baseline - read this before touching any data file

DECISIONS 2026-10-21 "Combat data: the current pin is VR's frozen baseline":

- **Never edit `apps/vr/data/pinned/**`, `apps/vr/data/PINNED.json` or `apps/vr/data/conformance/**`.** Not one byte,
  not a comment. Nobody re-pins from TV.
- **Every Earth number goes in the VR overlay**, `apps/vr/data/vr/`, exactly as Air did
  (`apps/vr/data/vr/schools/spells-air.csv`, `combat.vr.json`). There is **no pinned `spells-earth.csv`** - the pinned
  tree holds only `baseline-fourteen-nights/design/data/spells-water.csv` and
  `reference-the-ledger/design/data/spells-fire.csv` (verified 2026-10-21). Earth's spell table is **new VR design**,
  like Air's, and is authored as `apps/vr/data/vr/schools/spells-earth.csv` with a sheet in `docs/schools/EARTH.md`.
- Earth's identity is read from pinned `schools.json`
  (`apps/vr/data/pinned/docs/design/baseline-fourteen-nights/design/data/schools.json`, the `id: "earth"` entry) and
  never retyped: `resource: "footing"`, `range: [0, 6]`, `gain.stackPerHalfSecondBelowMps: 1.0`,
  `decay.clearOnRollOrSprint: true`, thresholds `at: 1` (`perStackDamageTakenMult: -0.04`,
  `perStackSpellPowerMult: 0.04`) and `at: 3` (`absorbDrainMult: 0.8`), `perfectAbsorbExtra: "+2 Footing"`,
  `colour: "#7a6a3a"`.
- **The 45 conformance vectors stay byte-identical and green.** The mechanism that keeps them so is the one `MixAir`
  uses: `Kernel/StateHash.cpp:201-203` (`MixAir`, with its comment "Mixed only for an air actor, so every water and
  fire state hashes as it did before Air existed"), guarded at `:205-208` by `if (!Air.bSchool) { return; }`.
  `MixEarth` is written the same way and is the only acceptable shape. The `scenarios-vr` golden hashes
  (`apps/vr/data/scenarios-vr/fire-duel-competence-1.json`, `-1-5.json`) and `MageArena.Fire.Golden` must not move.
- A `combat.json` copied from a contest folder is stale; do not use one.

Proof, re-run after every data touch: `node apps/vr/tools/check-pin.mjs` must print exactly
`pin OK: 9 files @ baeac66` then `conformance OK: 45 vectors`.

## What Earth costs end to end (the full shape)

Every item is a real file or a real symbol, verified on `perfect/2026-10-21`. The "Air cost" column is what the same
item actually cost in T23, measured from the commits.

| # | Item | Where | Air's measured cost |
|---|---|---|---|
| 1 | Spell sheet: ten Earth rows designed from nothing (no pinned table) | `docs/schools/EARTH.md`, `apps/vr/data/vr/schools/spells-earth.csv` | `docs/schools/AIR.md` 104 lines + `spells-air.csv` 11 lines, commit `f134847` (132 insertions with the card) |
| 2 | `FEarthState` in `SimTypes.h` with a Footing field | `Kernel/SimTypes.h`; the pattern is `struct FAirState` at `:215-234` and `FEarthState Earth;` beside `FAirState Air;` at `:323` | +50 lines in `SimTypes.h` |
| 3 | Resolver: Footing tick, catalog, ten casts, shapes | `Kernel/Earth.{h,cpp}` beside `Kernel/Air.{h,cpp}` | `Air.cpp` 646 + `Air.h` 58 |
| 4 | Strict row loader (bad cell = load failure) | `Kernel/KernelDataEarth.cpp` beside `Kernel/KernelDataAir.cpp` | `KernelDataAir.cpp` 442 lines (split out later by `94aba41`) |
| 5 | `StateHash` coverage, zero-impact-when-off | `MixEarth` in `Kernel/StateHash.cpp`, modelled on `MixAir` at `:203` | +74 lines in `StateHash.cpp` |
| 6 | AI picker | `ChooseEarthSpell` in `Kernel/MageAI.cpp`, beside `ChooseAirSpell` (`:396`), `AirOfferable` (`:369`), `AirUseful` (`:384`), dispatched from `ChooseSpell` (`:500`, Air branch at `:509`) | +166 lines in `MageAI.cpp` |
| 7 | Spawn wiring | `Kernel/Games.cpp`: `MageSchool` (`:37-49`), the school test at `:131` and `Actor->Air.bSchool = true;` at `:133`, labels at `:117-118` and `:155-156`; the flags `bFireMages` (`Games.h:38`) and `bAirFinal` (`Games.h:41`), both in `TryCreateGames` (`Games.h:66`) | +40 `Games.cpp`, +7 `Games.h` |
| 8 | Rival data and binding | `combat.vr.json` `rivals` (Stone Tent entrants are `garran` main, `senna` rival - `docs/campaign/01-story-core.md:81`); `Kernel/VrRivals.cpp`, `Kernel/VrRulesLoad.cpp` | +22 / +11 lines |
| 9 | Presentation and threat language | `Session/SessionPresentation.cpp` (`FamilyColour`, telegraphs); the palette entry **already exists**: `Session/ElementColour.cpp:49` `Palette.Earth = FLinearColor(0.55f, 0.26f, 0.035f);` under the comment at `:48` "Reserved for the Earth school (no Earth caster in the slice)", read from data at `:100`, enum `EElement::Earth` at `ElementColour.h:22` | +194 `SessionPresentation.cpp`, +20 its header |
| 10 | Tests: hand-computed oracle, no TS reference exists | `Tests/EarthTests.cpp` beside `Tests/AirTests.cpp`; plus `Tests/RivalTests.cpp`, `Tests/RivalCensusTests.cpp`, `Tests/CensusTests.cpp` | `AirTests.cpp` 1179 lines + 109 lines of census/rival edits |
| 11 | Capture harness for the stills | `Session/EarthCapture.{h,cpp}` beside `Session/AirCapture.{h,cpp}` | `AirCapture.cpp` 409 + `.h` 63 |
| 12 | Cross-channel record | `docs/change-requests/CR-00x-earth-school.md` (a VR design record now, not a request to TV - DECISIONS 2026-10-21) | `CR-007-air-school.md` |
| 13 | **Footing's seated reading** and Stone form (defence B) on a seated body | `SCHOOL-DEFENCES.md` B; needs `SeatOnActivePad` (`Session/ArenaSession.cpp:1750`, called unconditionally at `:1921` and `:411`) | **Air never paid this.** T23 skipped it with the "the AI is not seated" reading |
| 14 | A player who can cast Earth at all | nothing in the tree does this for any school but Water (see below) | **Air never paid this either** |

### Cost estimate, derived

Air's whole landing was **4473 insertions across 36 files in one commit** (`4bebb2d`, `git show --shortstat`), plus 132
insertions of design sheet and card (`f134847`) and a later 442-line loader split (`94aba41`) - about **4.6k lines**,
one agent run at 400 max turns (cut once by a usage limit and resumed) and roughly **one Wolf day** of build, suite,
census and capture, ending in an unresolved balance finding that T39 is still clearing.

Earth, rows 1-12, is the same work with the same shapes available for reuse, so:

- **Phase 1 (rows 1-8, 10 in part): 1.5-2 Wolf days + 0.5-1 Lynx day** for the spell sheet. Slightly above Air because
  the Earth sheet has no Fire sheet to be derived against the way Air's was (Air's rows were Fire rows times the TV
  lab multipliers; Earth's slow, protective identity has no such donor), and slightly below because `Air.cpp` is now a
  second template beside `Fire.cpp`.
- **Phase 2 (rows 9, 11, 13 for a rival's Stone form, 12): +1-1.5 Wolf days**, and its Stone-form half is gated.
- **Phase 3 (rows 13-14 for the player): +1-2 Wolf days and an unbounded design tail**, because a player-castable
  school is a different feature from a school a mage casts (next section), and Stone form is 2.5 s of
  **invulnerability** that touches every damage path including the unblockable rule.
- **Plus a calibration tail of unknown size**, by the DF-008 precedent: a new AI school arrives unbalanced and the
  census has to be read and the numbers moved.

**Total for the full shape: 3.5-5 Wolf days plus the tail.** Phase 1 alone: 1.5-2 Wolf days. Both are measured against
the Air precedent and neither includes a day of owner review.

## The player-school gap, and which feature this card builds

**No code path gives the player any school but Water.** `Kernel/Games.cpp:127-134` sets `Fire.bSchool` / `Air.bSchool`
only on the team-1 mage it has just spawned in `SpawnWave`; the school itself is chosen by `MageSchool`
(`Games.cpp:37-49`), which returns `"water"` whenever `bFireMages` is false and otherwise `"fire"`, or `"air"` for the
final when `bAirFinal` is set. Nothing sets a school flag on the player actor. So Fire and Air are **rival-only**, and
`Session/ElementColour.h:15` says as much in its own words: "Earth has a colour reserved and no caster yet."

This matters for reading the ruling. DECISIONS 2026-10-03 says "Every channel's kernel will implement **all four
schools**" and "the C++ kernel implements the missing schools first, data-driven from the shared data files". **It says
the kernel implements them. It does not say the player can cast them** - and the same owner's 2026-10-07 entry puts
"All four schools playable at launch" in **spring 2027**. An Earth *rival* therefore satisfies the 2026-10-03 ruling as
written; a player-castable Earth school is a separate, larger feature that the owner has already dated past the
competition.

**This card builds the rival.** Phase 3 is written down so the shape is known, and is not recommended for this year.

## Phases

### Phase 1 (recommended slice if the owner overrides the cut): Earth as a kernel school and an AI mage, not in the slice day

Not gated on any owner decision. Delivers: Earth exists, loads, resolves, is hashed, is tested, and an Earth mage can
be spawned behind a flag. Cuts cleanly - nothing in the shipping path changes, so removing the flag leaves the tree as
it is today.

1. `docs/schools/EARTH.md` and `apps/vr/data/vr/schools/spells-earth.csv`: ten rows, the column shape of
   `spells-air.csv` with `footing_gain` in place of `momentum_gain`, every number sourced in the sheet and marked as a
   proposal for the owner. Identity from pinned `schools.json` `earth.combatIdentity`; temperament "calm, protective,
   slow to move and hard to move". Reuse existing shapes wherever a row can (projectile, cone, ring, ground zone,
   channel, self lock) and justify every genuinely new shape in the sheet.
2. `FEarthState` in `SimTypes.h` carrying **Footing read literally from the pin**: one stack per half-second while the
   actor's speed is below 1 m/s, range 0-6, cleared on a roll or sprint, `+2` on a perfect absorb; thresholds at 1
   stack (damage taken `-0.04` and spell power `+0.04` **per stack**) and at 3 (`absorbDrainMult 0.8`). Where the pin's
   wording is ambiguous, take the **most literal** reading, record it under a `## Kernel readings` section in
   `EARTH.md` marked **Owner:**, and report it. Never retype a number.
3. `Kernel/Earth.{h,cpp}` and `Kernel/KernelDataEarth.cpp` (strict loader: a bad cell is a load failure, as Air's is).
4. `MixEarth` in `StateHash.cpp`, returning immediately when `!Earth.bSchool`, so every Water, Fire and Air hash is
   byte-identical to today.
5. `ChooseEarthSpell` in `MageAI.cpp` on the `ChooseAirSpell` model, dispatched from `ChooseSpell`, with the CR-003
   off-dais movement rules. A slow, protective picker: holds position to bank Footing, spends it on power.
6. Spawn wiring: extend `MageSchool` and the `:127-134` branch so an Earth mage can be spawned, **behind a new flag of
   its own** (the `bAirFinal` pattern). **Default off. The Tiro day is unchanged: wave 3 stays Brennic (Fire), wave 4
   stays Lio (Air).** Only `MageArena.Earth.*` turns the flag on.
7. `Tests/EarthTests.cpp`: an independent oracle, since no TS reference for Earth exists and record-and-replay would
   prove nothing. Each test computes its expectation **by hand, in the test, from the data**, with the source quoted in
   a comment: Footing gain over hand-counted ticks below 1 m/s; the clear on roll; both thresholds including the
   per-stack arithmetic at 1, 3 and 6 stacks; `+2` on a perfect absorb; the cap at 6; each row's mana, cooldown,
   damage, blockable class and telegraph; and one test that asserts an Earth-flag-off session's `StateHash` is
   unchanged.
8. **No regression:** `MageArena.` all green, `MageArena.Kernel.Conformance` 45/45, `MageArena.Fire.Golden` unmoved,
   `MageArenaDesign.` no red beyond the known list in `CLAUDE.md`.

Nothing in phase 1 touches `Session/`, the palette, the capture harnesses or the rival data.

### Phase 2 (after phase 1, still ungated except where marked): an Earth rival that can be seen

9. Greybox presentation: Earth threats in the threat language (`docs/gameplay/04-threat-language.md`) using the ochre
   already at `ElementColour.cpp:49`; check on screen that it reads as distinct from the Fire orange at `:45` and from
   the unblockable rim red, and say what you saw.
10. `rivals.<name>` in `combat.vr.json` (Stone Tent: `garran` main, `senna` rival) with `_why` lines citing DECISIONS,
    plus the `VrRivals.cpp` / `VrRulesLoad.cpp` binding and a signature cast for the last phase (the T19 rule).
11. `Session/EarthCapture.{h,cpp}` and the stills.
12. The VR design record `docs/change-requests/CR-00x-earth-school.md`.
13. **GATED on the owner's Footing ruling:** Stone form (defence B) for the **rival** - the 2.5 s invulnerable window
    of `SCHOOL-DEFENCES.md` B, paid in Footing. A standing rival could use the pinned rule, but the window is
    adversarial against a seated player who cannot walk out of it, so the reading and the duration are the owner's.
14. **Not started before the owner rules:** an Earth rival has no slot in the Tiro day. Phase 2 ends with the rival
    spawnable and captured, **not** placed in the day. Placing it is an owner decision about which of Brennic or Lio
    gives up a slot.

### Phase 3 (not recommended before 18 Nov): Earth the player can cast

15. **GATED:** Footing's seated reading. A seated player never moves, so the pinned "below 1 m/s" is degenerate -
    always true - which is exactly why `SCHOOL-DEFENCES.md` proposes the pad as the unit. Note the measurement hazard
    a sibling card (T50) raises for the analogous Air case: `FArenaSession::SeatOnActivePad`
    (`Session/ArenaSession.cpp:1750`) writes both `Player.Pos` and `Player.PreviousPos` to the pad centre, and it is
    called unconditionally inside the per-tick path at `:1921` (and once more at `:411`), so **any seated resource
    computed from displacement reads zero every tick.** A Footing rule stated in terms of movement is unmeasurable in
    the seated build whichever way the owner words it; it must be stated in terms of the pad, a blink, or a cadence.
16. A way for the player to have a school at all: the missing counterpart to `Games.cpp:127-134`, a school selection,
    and the split-hand rules of T11 for an Earth caster.
17. Stone form for the **player**: 2.5 s invulnerable, cannot ward or blink, channel without interruption. It must not
    break the threat language - an unblockable is a leave cue, and a form that absorbs one teaches the opposite.

## Read first
- `CLAUDE.md`, then `docs/DECISIONS.md` newest first (they outrank this card), especially 2026-10-21 "Combat data: the
  current pin is VR's frozen baseline", 2026-10-21 "Deadlines and Q9", 2026-10-07 (the Air choice and the spring-2027
  line) and 2026-10-03 "all four schools in every kernel".
- `apps/vr/tasks/T09-fire-school.md` and `apps/vr/tasks/T23-air-school-opponent.md` **in full** - the two precedents;
  T23's "Orchestrator verification" section is the most useful page in the repo for this work.
- `docs/design/SCHOOL-DEFENCES.md` (section B, and the "Order of work" item 3 that waits on this ruling),
  `docs/schools/AIR.md` (the sheet format and its nine owner-marked kernel readings), `docs/schools/FIRE.md`.
- `apps/vr/tasks/BACKLOG.md:17` (the open OWNER item), `docs/concepts/WOLF-PLAN.md` sections c, d, f and g.
- `docs/gameplay/04-threat-language.md`, `docs/change-requests/CR-007-air-school.md`, `CR-005-rival-phases.md`.
- Data: pinned `schools.json` `earth`; pinned `arena-tiers.json` Tiro waves; `apps/vr/data/vr/combat.vr.json`;
  `apps/vr/data/vr/schools/spells-air.csv`.
- Code, in this order: `Kernel/Air.{h,cpp}`, `Kernel/KernelDataAir.cpp`, `Kernel/SimTypes.h:215-234` and `:323`,
  `Kernel/StateHash.cpp:203` (`MixAir`), `Kernel/MageAI.cpp:369-520`, `Kernel/Games.cpp:37-49` and `:110-160`,
  `Kernel/Games.h:36-66`, `Session/ElementColour.{h,cpp}`, `Session/AirCapture.{h,cpp}`, `Tests/AirTests.cpp`.

## Deliverables (exact paths)

Phase 1 only, unless the owner extends the card:
- `docs/schools/EARTH.md` - the proposal sheet, its sources, the design intent, and a `## Kernel readings` section with
  every ambiguity marked **Owner:**.
- `apps/vr/data/vr/schools/spells-earth.csv` - ten rows, `spells-air.csv` columns, `footing_gain`.
- `apps/vr/Game/Source/MageArenaVR/Kernel/Earth.h` / `Earth.cpp` - Footing and the ten casts.
- `apps/vr/Game/Source/MageArenaVR/Kernel/KernelDataEarth.cpp` - the strict row loader.
- Edits only, nothing rewritten: `Kernel/SimTypes.h` (`FEarthState`, the `Earth` member), `Kernel/StateHash.cpp`
  (`MixEarth`), `Kernel/MageAI.cpp` (`ChooseEarthSpell` + dispatch), `Kernel/Games.{h,cpp}` (the flag and the branch),
  `Kernel/Catalog.cpp`, `Kernel/ArenaKernel.{h,cpp}` as the Air wiring requires.
- `apps/vr/Game/Source/MageArenaVR/Tests/EarthTests.cpp` - the hand-computed oracle.
- `runs/T48/REPORT.md`.

## Constraints
- Do not commit, push, or touch any repository other than this one. Do not call paid services. Do not edit
  `docs/DECISIONS.md`.
- **Never edit `apps/vr/data/pinned/**`, `apps/vr/data/PINNED.json`, `apps/vr/data/conformance/**`,
  `apps/vr/Game/Clips/**` or `apps/vr/data/vr/calibration-proposal.json`.**
- Do not place an Earth mage in the Tiro day. Do not change which school wave 3 or wave 4 spawns. The new flag defaults
  off.
- Do not invent Footing's seated reading, and do not implement Stone form for the player. Stop at the gate and report.
- Measure, do not tune. Numbers come from the data; interpretations are written down and marked **Owner:**.
- Regenerate the sigil corpus before the full suite in a fresh checkout or worktree (`CLAUDE.md` "Build"):
  `node apps/vr/tools/clipgen/generate.mjs --corpus 30 --seed 1000` then `node apps/vr/tools/clipgen/mousepaths.mjs`.
  Never run `generate.mjs` bare or with `--templates`.
- One Unreal build at a time; one `Automation RunTests` filter per editor process; read verdicts from
  `-ReportExportPath`, never from an exit code alone. Do not end your turn while a build, suite or capture is running.
  Never write placeholder evidence.

## Acceptance (the orchestrator re-runs these; quote their real output in your report)

Node gates - these run anywhere, including Lynx:
```
node apps/vr/tools/check-pin.mjs
# expected, exactly: "pin OK: 9 files @ baeac66" then "conformance OK: 45 vectors"
node apps/vr/tools/check-pin.mjs --self-test
# expected: "corruption detected: ...combat.json" and "conformance corruption detected: bolt-hit.json"
node apps/vr/tools/lint/unused-members.mjs
# expected: "unused-members OK: <n> private/protected members in <m> classes"
node apps/vr/tools/lint/format-check.mjs --base <merge base>
# expected: "format-check OK (changed C++ lines only)"
node tools/check-commits.mjs
# expected: "check-commits OK: <n> commit subject(s) since <base>"
git diff --stat <base> -- apps/vr/data/pinned apps/vr/data/PINNED.json apps/vr/data/conformance
# expected: no output at all (the frozen baseline is untouched)
```

Unreal gates - **Wolf only** (this card was written on Lynx, which has no Unreal install, DECISIONS 2026-10-20 "Unreal
host"; a Lynx session reports these as not evaluated, never as a pass):
```
powershell -NoProfile -File apps/vr/tools/build.ps1
# expected: "Result: Succeeded" in the log, not exit 0 alone

$ed = "C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
$up = "$PWD\apps\vr\Game\MageArenaVR.uproject"

& $ed $up -ExecCmds="Automation RunTests MageArena.Kernel.Conformance;Quit" -unattended -nullrhi -nosplash -log -ReportExportPath="$PWD\runs\T48\conf"
# expected: 45 Success, 0 Fail, EXIT CODE: 0 - and run this on its own after each kernel file (the T33 rule)

& $ed $up -ExecCmds="Automation RunTests MageArena.Earth;Quit" -unattended -nullrhi -nosplash -log -ReportExportPath="$PWD\runs\T48\earth"
# expected: every MageArena.Earth.* test Success, 0 Fail, EXIT CODE: 0

& $ed $up -ExecCmds="Automation RunTests MageArena.;Quit" -unattended -nullrhi -nosplash -log -ReportExportPath="$PWD\runs\T48\MageArena"
# expected: 0 Fail, EXIT CODE: 0; the count is the recorded baseline plus the new MageArena.Earth.* tests, and
# MageArena.Fire.Golden is Success with its hashes unmoved

& $ed $up -ExecCmds="Automation RunTests MageArenaDesign.;Quit" -unattended -nullrhi -nosplash -log -ReportExportPath="$PWD\runs\T48\MageArenaDesign"
# expected: no red beyond the five known failures in CLAUDE.md "Build" (Duel.FireSeated, Duel.AirSeated,
# Session.FullSeated, Session.Wave1Seated, Session.CreaturesSeated), or T39's recorded result if T39 has landed
```

A mutation check is part of acceptance, not an extra: break one Footing constant by hand, confirm the matching
`MageArena.Earth.*` tests go red, revert it, and quote both runs. A test that cannot fail is not evidence.

## Report
Write `runs/T48/REPORT.md`: every file you wrote or edited, every acceptance command with its **real** output pasted,
each kernel reading you marked **Owner:** in `EARTH.md` and why that reading is the literal one, the mutation check
both ways, anything you could not do with the exact error, and every decision this card did not specify. Say
explicitly which phase you stopped at and which gated item stopped you.

## Notes for the orchestrator (Lynx, 2026-10-21)

- Nothing in this card was built or run. No UE result here is a claim.
- `ElementColour.cpp:49` already holds the Earth ochre with `:48` as its comment, so row 9 of the cost table is the
  cheapest item in the card, not a missing one.
- **What belongs elsewhere, for the Director:** `apps/vr/tasks/BACKLOG.md` should name T48 beside the open OWNER
  Footing item (it currently says "Decide before T10/T11", and the Earth card it was waiting for now exists);
  `docs/concepts/WOLF-PLAN.md` section f should carry Earth on the proposed cut list, above P11, as an item never
  started rather than one to drop; `docs/design/SCHOOL-DEFENCES.md` "Order of work" item 3 should point at T48 by id.
