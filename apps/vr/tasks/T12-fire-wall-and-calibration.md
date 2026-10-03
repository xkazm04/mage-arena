# T12 - Fire wall (C) for the Fire mage, and the seated calibration census

Status: open (dispatch after T11 is committed)
Max turns: 320

## Goal
Two design gates are red from a seat: Wave 1 is lost at 23.25 s even with split hands and the planted staff (T11), and
the Fire mage wins every duel at 16-18 s, before Sunfall (DF-002). The owner chose **VR calibrates** and allowed every
knob family (`docs/DECISIONS.md` 2026-10-03): wave composition, enemy pressure, player spell power, and the defences.
This card (1) adds the Fire school's defence, the **fire wall** (`docs/design/SCHOOL-DEFENCES.md` C), used by the Fire
mage AI, and (2) builds a **headless census** that sweeps the knobs over many seeds and **proposes** numbers that land both
gates. The proposal is for the owner to sign off: **nothing in the census output is adopted into the live overlay by this
card.**

## Read first
`CLAUDE.md`, `docs/DECISIONS.md`, `docs/design/SCHOOL-DEFENCES.md`, `docs/design-findings/DF-001-seated-wave1.md`,
`DF-002-fire-duel-balance.md`, `docs/schools/FIRE.md`, `runs/T10/REPORT.md`, `runs/T11/REPORT.md`; code `Kernel/*`
(`VrRules.*`, `Fire.*`, mage AI, `Games.*`), `Session/*`, `Tests/SessionTests.cpp`, `Tests/FireTests.cpp`,
`Tests/DefenceTests.cpp`; data `apps/vr/data/vr/combat.vr.json`, `apps/vr/data/scenarios-vr/`.

## What T11 measured (use it)
Seated Wave 1: lost at 23.25 s, dealt 106.7 of 208 (4.59 HP/s; the clear needs ~45 s at that rate), took 95. The script
split-cast **once**: the palm ward drains 27 mana/s at Nerve 1, so holding the ward while casting with the other hand is
nearly unaffordable. The dome (8 up front + 4/s) is the cheaper hold. Damage rate, not only survival, is the gap.

## Deliverables
1. **Fire wall (C) as a VR overlay rule**, `fireWall` in `combat.vr.json` (proposal values from SCHOOL-DEFENCES C:
   4 m in front of the caster, 90-degree arc, 3 s, a mana cost, Heat per projectile stopped). Absorbable and physical
   projectiles that cross it are destroyed; enemies that cross take burn damage; **unblockables pass**. The Fire mage AI
   raises it when player projectiles are inbound (a rule in the AI, competence-scaled reaction like its other choices).
   A null ruleset keeps the pinned path. Greybox presentation: a low orange-red curtain (never black-with-red-rim, which
   is reserved for the unblockable). Oracle tests `MageArena.Defences.FireWall*`, expectations hand-computed from the
   overlay values in the test: a bolt stopped, a physical projectile stopped, an unblockable passing, burn on a crosser,
   Heat gained per stop, mana cost, duration.
2. **A seated Fire duel**: the reference seated player (pads + blink only, ward, split hands, planted staff, sigils;
   no walking - same rules as `Wave1Seated`) against the Fire mage at competence 1 and 1.5. New design gate
   `MageArenaDesign.Duel.FireSeated`: duel length in the 45-80 s band, Sunfall cast at least once, and the player wins at
   competence 1. It is expected to be red until the proposal is signed off; do not weaken it to pass.
3. **The census** (a headless automation test or commandlet under `MageArenaDesign.Census.*`, or a node driver around
   `UnrealEditor-Cmd`, whichever is cheaper per run): for each candidate knob set, run N seeds (at least 20) of seated
   Wave 1 and of the seated Fire duel at both competences; record outcome, time, HP left, damage dealt/taken, split casts,
   plants, walls, perfects, Sunfall casts. Knob families, all as overlay fields (never edits to `apps/vr/data/pinned/`):
   - wave composition (enemy counts and spawn timing for VR Wave 1),
   - enemy pressure (attack cadence, enemy damage multiplier, Fire mage damage multiplier),
   - player power (bolt/line damage multiplier, mana regen multiplier),
   - defences (ward drain while split-casting, `oneHandPower`, staff duration/cost/reductions, fire wall values).
   Also sweep the **scripted player's policy** (how readily it split-casts and plants) and report how sensitive the
   outcome is to it: the scripted player is a proxy for a human, and a balance that only works for one script is a
   finding, not a result.
4. **Targets** (proposal; the owner may change them): Wave 1 seated, reference policy: victory in 25-40 s in >= 80% of
   seeds, HP left 20-60% median. Fire duel seated: 45-80 s median, Sunfall seen in >= 70% of duels; player win rate
   >= 70% at competence 1 and 40-60% at competence 1.5. Prefer the **fewest knobs changed, by the smallest amounts**;
   prefer defence and player-side changes over making enemies weaker where both work.
5. **Outputs, not adoption**: `runs/T12/census/*.csv` (raw), `apps/vr/data/vr/calibration-proposal.json` (the proposed
   knob values, with a `_why` per field and the census numbers it produces), and
   `docs/design-findings/DF-003-calibration-proposal.md` for the owner: the top 2-3 candidate sets with their measured
   outcomes, what each one costs in feel (e.g. "enemies hit 20% softer" vs "ward drain halves while split"), and a
   recommendation. The design gates must be **runnable against the proposal** behind an explicit switch (e.g. a
   `-MageArenaProposal` command-line flag or an env var) so the orchestrator can show green-with-proposal; the default
   run keeps today's overlay, so the gates stay red until sign-off.
6. No regression: `MageArena.*` all green, 45 conformance vectors byte-identical, existing Fire oracle tests unchanged.
7. Capture: one still of the Fire mage's wall stopping a bolt (`runs/T12/shots/`), via the existing capture tooling.
8. `runs/T12/REPORT.md`: files, tests, census size and wall time, the candidate sets and their numbers, sensitivity to the
   script policy, and anything the knobs could not fix.

## Constraints
Do not commit, push, or modify other repositories; do not edit `apps/vr/data/pinned/` or art. Do not change live overlay
values in `combat.vr.json` beyond adding the `fireWall` proposal block; calibrated numbers go only to
`calibration-proposal.json`. One Unreal build at a time; keep the census inside a reasonable wall time (report it).

## Acceptance (the orchestrator re-runs these and looks at every screenshot)
```
node apps/vr/tools/conformance/generate.mjs     # byte-identical
powershell -NoProfile -File apps/vr/tools/build.ps1
"C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "C:\Users\kazda\kiro\mage-arena-vr\apps\vr\Game\MageArenaVR.uproject" -ExecCmds="Automation RunTests MageArena.;Quit" -unattended -nullrhi -nosplash -log
"C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "C:\Users\kazda\kiro\mage-arena-vr\apps\vr\Game\MageArenaVR.uproject" -ExecCmds="Automation RunTests MageArenaDesign.;Quit" -unattended -nullrhi -nosplash -log
<the same design run with the proposal switch>  # shows the proposal's effect
```
