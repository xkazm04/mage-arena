# T19 - Named rivals fight in HP-gated phases; the last phase opens with the signature (DF-003)

Status: open
Max turns: 320

## Goal
Owner decision 2026-10-07 (`docs/DECISIONS.md`, "v2 vision accepted": DF-003 answered). Today the seated Fire duel
ends near 30-45 s and Sunfall (tier IV, unlocked at 45 s) is cast in 0-3 of 20 duels, so the duel has no climax.
After this card a **named rival** (the slice's first is **Brennic**, the Ember mage) fights in **HP-gated phases**:
when the rival's HP crosses each threshold, the phase breaks, **both tier clocks surge by one tier**, a `phase` event
fires (the crowd swell hangs on it), and **the last phase opens with the rival's signature spell** (Brennic:
`fire_sunfall`). Census targets: the signature in 100 % of duels that reach the last phase, duel median 45-80 s, player
win rate >= 70 % at competence 1 and 40-60 % at competence 1.5. This is a VR overlay rule now; the orchestrator writes
the TV change request (CR-005) after verification.

## Read first
- `CLAUDE.md`, `docs/DECISIONS.md`, `docs/design/V2-ROADMAP.md` (lane A row 3), `docs/design/V2-VISION.md` waves 3-4,
  `docs/design-findings/DF-002-fire-duel-balance.md`, `DF-003-calibration-proposal.md`, `docs/schools/FIRE.md`,
  `docs/gameplay/03-tier-clock-and-flow.md`, `07-enemies-and-ai.md`.
- Data: pinned `combat.json` `tierClock` (unlock 1:0 2:15 3:30 4:45, `minimumSecondsBetweenUnlocks` 6,
  `perfectAbsorbAdvanceS` 2), `spells-fire.csv` (`fire_sunfall`: tier 4, mana 70, damage 95, unblockable),
  `arena-tiers.json` Tiro waves 2-3, `apps/vr/data/vr/combat.vr.json`, `calibration-proposal.json`.
- Code: `Kernel/ArenaKernel.cpp` (`UpdateClock` ~430-446, `AddMage` ~368-382: max HP 95 at vigor 1, `Emit` ~398),
  `Kernel/SimConstants.h` (`TierCap` 4), `Kernel/Fire.cpp` (tier gate ~153, Sunfall cast and loose ~185-205, ~293),
  `Kernel/MageAI.cpp` (`ChooseFireSpell` ~263, `MageInput` ~650, `AttachMageAI` ~637), `Kernel/Games.cpp`
  (`SpawnWave` ~80-125, Fire mages ~98-115), `Kernel/VrRules*.{h,cpp}`, `Session/SessionPresentation.cpp`,
  `Tests/CensusTests.cpp` (`MageArenaDesign.Census.Sweep` ~674, `ScoreOf` ~237, CSV ~409, outputs ~762/797),
  `Tests/SessionTests.cpp` (`Duel.FireSeated` ~243).

## Deliverables
1. **Overlay block** `rivals` in `combat.vr.json`, every number with a `_why` citing the DECISIONS entry:
   `brennic: { school: "fire", appliesTo: <how the session picks this mage: the Fire mage of the Tiro semifinal>,
   phases: [ { atHpFraction: 0.66 }, { atHpFraction: 0.33, opensWith: "fire_sunfall" } ], surgeTiers: 1 }`.
   Strict parsing as the other overlay keys; absent block = today's behaviour.
2. **Phase rule** (kernel, ruleset path only):
   - A break happens on the tick the rival's HP first falls to or below `atHpFraction x MaxHp` (95 x 0.66 = 62.7,
     95 x 0.33 = 31.35). One hit crossing two thresholds breaks both, in order, on that tick.
   - On each break **both** the rival's and the player's tier rise by `surgeTiers` (capped at `TierCap`), each emitting
     the normal `unlock` event; the surge ignores `minimumSecondsBetweenUnlocks` (it is the point of the break) and the
     normal clock then continues from the new tier. Emit `Emit(..., "phase", rival, -, phaseIndex)`.
   - `opensWith`: on that break the rival's next action is the signature spell, as a **granted cast**: it ignores the
     tier gate and the mana cost once, and uses the normal cast time, telegraph and resolution (Sunfall stays
     unblockable and blinkable). It fires at most once per duel; a Sunfall the AI already loosed normally does not stop
     the granted one. If the rival is mid-cast, the granted cast starts when that cast ends.
   - Keep phase runtime state in the ruleset runtime or a non-hashed side struct, so the conformance state hash is
     untouched on the null path; say in the report what you chose.
3. **Presentation (greybox)**: on a `phase` event, a short, readable beat: the rival flares in its element colour, a
   ring pulse on both wrist cuffs (the surge), and the crowd band brightens (the swell placeholder; audio is T26). On
   the screen log, `Phase 2/3`. No new threat colours.
4. **Census**: extend `MageArenaDesign.Census.Sweep` (or add `MageArenaDesign.Census.Rivals`) to run the seated Brennic
   duel at competence 1 and 1.5 over 20 seeds, **live and with the proposal**, with phases on, and report per set:
   duel median, win rate, the share of duels reaching each phase, the signature rate among duels reaching the last
   phase, and Sunfall loosed / blinked / hit. New CSV columns for phases. Write the official copy to
   `runs/T19/census/` and nothing into `calibration-proposal.json` (do not move any knob; the owner picks).
5. **Tests**:
   - Regular `MageArena.Rivals.*`, literals with sources: thresholds 62.7 and 31.35 break on the right tick; a 70-damage
     hit from 95 HP breaks both phases in order; the surge raises both tiers by 1 and caps at 4; the granted Sunfall
     starts on the last break with mana below 70 and at tier 2; it fires only once; with no `rivals` block the duel is
     byte-for-byte today's (compare event logs of a fixed seed).
   - Update `MageArenaDesign.Duel.FireSeated` to run with Brennic's phases and assert the new targets (signature cast
     in a duel reaching the last phase, length 45-80 s, player wins at competence 1). It may stay red; report it.
6. **No regression:** conformance byte-identical; `MageArena.*` green; report every `MageArenaDesign.*` before and after.

## Constraints
- Do not commit, push, or touch any repository other than this one. No paid services. Do not edit
  `docs/DECISIONS.md`, `apps/vr/data/pinned/` or `calibration-proposal.json`. Do not tune knobs to hit the targets:
  measure and report; the owner decides.
- Regenerate the sigil corpus as `CLAUDE.md` says before the full suite in a fresh checkout.
- One Unreal build at a time; one `Automation RunTests` filter per editor process. Do not end your turn while a build,
  test or census is running. Never write placeholder evidence.

## Acceptance (the orchestrator re-runs these)
```
node apps/vr/tools/conformance/generate.mjs && git status --porcelain apps/vr/data/conformance   # no output
powershell -NoProfile -File apps/vr/tools/build.ps1
UnrealEditor-Cmd MageArenaVR.uproject -ExecCmds="Automation RunTests MageArena.;Quit" -unattended -nullrhi -nosplash -log
UnrealEditor-Cmd MageArenaVR.uproject -ExecCmds="Automation RunTests MageArenaDesign.Duel;Quit" -unattended -nullrhi -nosplash -log   # + -MageArenaProposal
UnrealEditor-Cmd MageArenaVR.uproject -ExecCmds="Automation RunTests MageArenaDesign.Census;Quit" -unattended -nullrhi -nosplash -log # census table in runs/T19/census/
```

## Report
Write `runs/T19/REPORT.md`: files, every acceptance command with real output, the census table (live and proposal,
both competences) against the four targets, the arithmetic behind each test literal, and every decision the card did
not specify.
