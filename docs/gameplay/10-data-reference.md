---
title: Gameplay data reference
channel: vr
status: implemented
verified-against: 1fb1b8b (2026-10-07); anchors into files changed since 46643e2 re-verified
sources: [CLAUDE.md, docs/DECISIONS.md, docs/PROJECT-PLAN.md, apps/vr/data/README.md, docs/design-findings/DF-003-calibration-proposal.md, apps/vr/Game/Source/MageArenaVR/Kernel/KernelData.h, apps/vr/Game/Source/MageArenaVR/Kernel/KernelData.cpp, apps/vr/Game/Source/MageArenaVR/Combat/AbsorbResolver.cpp, apps/vr/Game/Source/MageArenaVR/Kernel/VrRules.h, apps/vr/Game/Source/MageArenaVR/Kernel/VrRules.cpp, apps/vr/Game/Source/MageArenaVR/Greybox/ArenaLayout.cpp, apps/vr/Game/Source/MageArenaVR/Hands/MageSettings.cpp, apps/vr/Game/Source/MageArenaVR/Hands/MageArenaPawn.cpp, apps/vr/Game/Source/MageArenaVR/Session/SessionFlow.cpp, apps/vr/Game/Source/MageArenaVR/Tests/KernelConformanceTests.cpp, apps/vr/Game/Source/MageArenaVR/Tests/FireTests.cpp, apps/vr/tools/check-pin.mjs, apps/vr/tools/conformance/generate.mjs]
data: [apps/vr/data/PINNED.json, apps/vr/data/pinned/**, apps/vr/data/vr/combat.vr.json, apps/vr/data/vr/arena-layout.json, apps/vr/data/vr/teach.json, apps/vr/data/vr/strings.json, apps/vr/data/vr/calibration-proposal.json, apps/vr/data/scenarios-vr/*.json, apps/vr/data/conformance/*.json]
---

# Gameplay data reference

The VR build reads two kinds of gameplay data. **Pinned data** (`apps/vr/data/pinned/**`, nine files) is a byte copy of
commit `baeac66` of `kiro/mage-arena-tv` (it was `68a4d68` on the `arena` branch before the 2026-10-07 history rewrite; the bytes are unchanged), owned by the desktop/TV channel and
its calibrated simulator; it is never edited here, and a change is a change request to that project followed by a
re-pin. **VR overlay data** (`apps/vr/data/vr/*.json`) is owned by this repo and holds everything a seated VR player
needs that the shared rules do not: the arena layout and pads, the threat-geometry overlay, the seated defences,
comfort settings, the teach and the prompt strings. Two test-data folders complete the set: `conformance/` (45 vectors
generated from the TypeScript oracle; they prove the C++ port) and `scenarios-vr/` (the C++ kernel's own reference for
Fire, which the TS kernel does not have). The calibration proposal in the overlay folder is **not live**.

## Pinned data (owner: `kiro/mage-arena-tv`, commit `baeac66`)

### Manifest and gate

| File | Purpose | Keys | How it is checked |
|---|---|---|---|
| `apps/vr/data/PINNED.json` | manifest of the pin | `source`, `commit` (`baeac6641790dcf526ec6d3bde35985553f84028`), `copied` (2026-10-02), `files[]` (`path`, `sha256`, `bytes`) | `node apps/vr/tools/check-pin.mjs` (also `--source <repo>` byte compare, `--self-test`) |
| `apps/vr/data/README.md` | the pin rules and the re-pin procedure | - | - |

At load the kernel logs `kernel data: pinned dir <dir>, <n> files, SHA1 <hash>`, a SHA-1 chained over the text of each
file in read order (`KernelData.cpp:1890`). If any pinned file fails to load, `KernelData().bReady` is false and no bout
starts.

How to change any pinned file: file a change request (`docs/change-requests/CR-*.md`) to the desktop/TV channel; after
it lands there, re-pin from a **commit** (never a working tree) with `git -C <source> show <commit>:<path>`, rewrite
`PINNED.json`, run the three `check-pin.mjs` commands, and regenerate the conformance vectors. A `combat.json` from a
contest folder is stale and must not be used.

### `docs/design/baseline-fourteen-nights/design/data/combat.json`

Loaded by `KernelData.cpp:1523` (most blocks) and `Combat/AbsorbResolver.cpp:153` (`absorb`). Units: seconds, metres,
mana, stamina, fractions.

| Key | Meaning | Values | Read by C++ | VR use |
|---|---|---|---|---|
| `simStepHz` | kernel step rate | 60 Hz | yes | fixed step |
| `arena.geometryAuthority` | points to the scale contract | `art/scale-contract-v1.json` | via scale contract | - |
| `movement` | walk 4.5 m/s, sprint 6.5 m/s, sprint cost 15/s | - | yes | not fed: the seated player never walks |
| `roll` | 4.0 m, 0.35 s, i-frames 0.25 s, 25 stamina, recovery 0.15 s | - | yes | **blink** is one roll edge (cost and i-frames), position overwritten by the pad |
| `staffStrike` | range 1.6 m, windup 0.25 s, recovery 0.30 s, 12 stamina, 6 damage | - | yes | range only: a bolt flick inside 1.6 m is suppressed |
| `stamina` | regen 20/s after 0.8 s | - | yes | yes |
| `aimMode` | soft-lock cone 60 deg, sticky 1.0 s, hold 0.2 s | - | **no** | not implemented |
| `tierClock` | unlocks at 0/15/30/45 s, perfect advances 2.0 s, min 6.0 s between, resets each wave | - | yes | yes |
| `absorb` | 140 deg arc, raise 5 mana, drain `30 - 3 * nerveRank`/s, re-raise gap 0.12 s, reductions magic 0.85 / physical 0.30 / unblockable 0 / outside arc 0, perfect window 0.15 s, mana return formula, clock advance 2.0 s, not against unblockable or physical | - | yes (`AbsorbResolver`) | the palm ward |
| `lines` | 3 slots, branches at tiers 2 and 4 | - | yes | yes |
| `flow` | different line within 2.0 s, max 5, +0.06 per stack, reset on same line twice or 3.0 s idle, Crest 0 mana x1.5, perfect +1 | - | yes | yes |
| `threatLanguage` | descriptive text | - | no | presentation follows it ([04-threat-language.md](04-threat-language.md)) |
| `reactionBands` | positional telegraph >= 0.40 s, unblockable >= 0.80 s, no player death before 5.0 s | - | the two telegraph floors (spell lint) | lint |
| `betweenWaves` | heal 0.30 of missing HP, mana 1.0, stamina 1.0, clock reset | - | yes | intermission |
| `pacingTargets` | soldier 25-40 s, creature 30-45 s, duel 45-80 s, dead-air bucket 2.0 s | - | `deadAirBucketS` only | the windows are literals in the design tests |

### `.../data/enemies.json`

Loaded by `KernelData.cpp:1847`. See [07-enemies-and-ai.md](07-enemies-and-ai.md).

| Key | Meaning | Units |
|---|---|---|
| `soldiers[]` | conscript, shieldman, slinger, netter: `hp`, `speedMps`, `attacks[]` (`family`, `damage`, `windupS`, `recoveryS`, `cooldownS`, `rangeM`, `projectileMps`, `effect`, `telegraph`), `behaviour` (some numbers are parsed from this text, e.g. "back off 1 s"), `keepDistanceM`, `frontBlockDeg` | HP, m/s, s, m |
| `creatures[]` | cinder_hound (with `onDeath` burst, `packSize`), mire_maw, thornback (`armourMultVsBolt`), hush_moth | same |
| `mages.competence[]` | levels 1-4: `reactionDelayS`, `absorbChanceVsAbsorbable`, `perfectAbsorbChance`, `aimErrorDeg`, `decisionCadenceS`, `usesTierCurve` (text) | s, probability, deg |
| `mages.caps` | `reactionDelayMinS` 0.25, `perfectAbsorbChanceMax` 0.45 | s, probability |
| `echo` | the player's Echo (camp loop) | not used in VR |

### `.../data/schools.json`

Loaded by `KernelData.cpp:1290` (`LoadHeat`). Only `schools[fire].combatIdentity` is read (Heat). Water's Flow numbers
are read from `combat.json` `flow`; Earth and Air, `campBias` and `nonSchools` are not read. See
[05-schools/README.md](05-schools/README.md).

### `.../data/arena-tiers.json`

Loaded by `KernelData.cpp:881`; spawned enemy ids are checked against `enemies.json` (`KernelData.cpp:1488`).

| Key | Meaning | VR use |
|---|---|---|
| `tiers[]` | Tiro, Veteranus, Primus, Summa: `waves[]` (`n`, `kind`, `spawns[]` of `{enemy, count}` or `{mage, competence}`, `targetDurationS`), `payoutGold[]`, `renown[]`, `requiresMastery`, `rankUpOn` | only `tiers[0]` (Tiro) is played (`Games.cpp` `TiroTier`) |
| `tentTrial` | camp pit bout | not read |

### `.../data/spells-water.csv`

Loaded by `KernelData.cpp:1846`; a missing column fails the load (`KernelData.cpp:364`). Columns: `line`, `tier`,
`branch`, `name`, `shape` (parsed into kind, speed, radius, count, spread, effect), `cast_s`, `cooldown_s`, `mana`,
`damage`, `blockable`, `telegraph_s`, `range_m`, `notes`. 24 rows: the Bolt plus 23 rows for Tide Orb, Lash, Mire, Mend
and Mirror tiers I-IV with their branches. Every row is listed in [05-schools/water.md](05-schools/water.md).

### `.../data/stats.csv`

Loaded by `KernelData.cpp:1653`. Rows `vigor`, `focus`, `nerve` give the arena formulas (max HP `80 + 15*rank`, max
stamina `60 + 10*rank`, max mana `80 + 15*rank`, regen `6 + 1.5*rank`/s, absorb drain `30 - 3*rank`/s, perfect return
multiplier `1 + 0.1*rank`). The nerve numbers must equal `combat.json` `absorb` or the load fails
(`KernelData.cpp:1613`). `guile`, `renown`, `gold`, `mastery` and the camp columns are not used.

### `docs/design/reference-the-ledger/design/data/spells-fire.csv`

Loaded by `KernelData.cpp:1351` into `FireSpells` and `FireCatalog`. Columns: `id`, `name`, `tier`, `shape`, `cast_s`,
`cooldown_s`, `mana`, `damage`, `blockable`, `telegraph_s`, `range_m`, `heat_gain`, `notes` (unquoted, contains
commas). Ten rows; see [05-schools/fire.md](05-schools/fire.md) and [FIRE.md](../schools/FIRE.md).

### `packages/core/src/arena/data/runtime.json`

Loaded by `KernelData.cpp:1794`. Authored supplements the baseline lacks.

| Key | Meaning | Units |
|---|---|---|
| `geometry` | `arenaCentre` (16, 62), `mageRadiusM` 0.38, `projectileRadiusM` 0.12, `staffArcDeg` 90 | m, deg |
| `training` | default ranks (1/1/1), drill positions, dummy HP 10000, drill attacks (magic 14, physical 7, charge 22), bot leads, report and performance settings | m, s, HP |
| `water` | `fanSpeedMps` 12, `returnWaveSpeedMps` 12, `returnWaveRadiusM` 1, `decoyRadiusM` 0.5 (not read), `tombCursorRadiusM` 2, report settings, `presets[]` Undertow / Mirror tide / Rotation | m/s, m |
| `games` | player/enemy spawn (8, 10) / (24, 10), jitter 0.8 m, melee range 1.2 m, ranged range 24 m, net/tongue ranges, maw cooldown 3.5 s, hound orbit 3.5 m, conscript backoff 3 m, mage preferred distance 5-8 m, strafe 2.4 s, aim lead 0.45, defence hold 0.35 s, `referenceCompetence` 2, `referencePreset` Rotation, `opponentPresets` [Undertow, Mirror tide], census settings | m, s |
| `presentation` | `maxFrameDeltaS` 0.25 (frame cap for the fixed stepper), perfect flash, bell, performance budgets | s, Hz, ms |

### `art/scale-contract-v1.json`

Loaded by `KernelData.cpp:1668`. The VR kernel reads only `arena_metres` [192, 144] (the kernel's arena bounds),
`minimum_combatant_centre_separation_metres` 6, and `absorb.angle_degrees` 140, which must equal `combat.json`
`absorb.arcDeg` or the load fails. The PC camera contract in the same file does not apply to VR (first person, seated).

## VR overlay data (owner: this repo, VR channel)

How to change: edit the file in this repo. Values marked "Proposal ... Not signed off" in the file need the owner's
sign-off before they count as tuned (DECISIONS 2026-10-03: calibration may move all four knob families, VR overlay
only, owner signs off). The loaders validate shape and ranges; a failed load stops the session from starting.

### `apps/vr/data/vr/combat.vr.json`

Loaded by `LoadVrRuleset` (`VrRulesLoad.cpp:440` onward) at every bout start and by `FMageSettings` for the narrow-view
numbers. Units: metres, seconds, mana, degrees. Applied only when a session passes the ruleset into `TryCreateGames`;
the conformance path passes null.

| Key | Meaning | Values | Validation |
|---|---|---|---|
| `version`, `units`, `axes` | schema version (must be 1) and frame notes | 1, metres | version checked |
| `daisNoEntry` | box enemies may not enter: `centreM`, `halfExtentM`, `standoffM` | (-11.7, 0), (2.6, 4.4), 0.6 | must equal `arena-layout.json` `dais` |
| `attacks[]` | per-enemy mode: conscript `throw` (damage 10, windup 0.6 pinned; `projectileMps` 14, `rangeM` 9 replace pinned; `arcApexM` 0.75, `spearLengthM` 1.7, `spearRadiusM` 0.11 presentation), slinger `unchanged` | - | pinned fields must match; slinger may not override anything |
| `movement.throwerClosesToLip` | throwers walk to the hold line during back-off | true | must be true |
| `splitHands` | `oneHandPower` 0.6, `maxTier` 2 | proposal | (0, 1], integer 0-4 |
| `plantedStaff` | `durationS` 6, `manaUpFront` 8, `drainPerSecond` 4, `magicReduction` 0.85, `physicalReduction` 0.6 | proposal | ranges checked |
| `fireWall` | `distanceM` 4, `arcDeg` 90, `durationS` 3, `manaCost` 12, `heatPerStop` 4, `burnDamage` 12 | proposal | ranges checked |
| `narrowFov` | `spawnArcDeg` 40, `offerBelowDeg` 90, `cameraFovDeg` 70 | - | ranges checked |
| `gentle` | `telegraphMult` 2, `competence` 1 | - | ranges checked |
| `_about`, `_*` | explanations; ignored by the loader | - | - |

### `apps/vr/data/vr/arena-layout.json`

Loaded by `FArenaLayout::LoadFromFile` (`ArenaLayout.cpp:264`, greybox and session), `LoadVrRuleset` (dais, pads, chest
height, steel shaft), `AMageArenaPawn` (camera, blink, mouse look) and `FMageSettings` (`cameraFovDeg`). Unreal axes,
metres, origin at the centre of the sand.

| Key | Meaning | Values |
|---|---|---|
| `floor`, `wall`, `stands` | oval sand 32 x 20 m (radii 16, 10), 3 m ring wall, three stand tiers | m |
| `dais` | raised platform: centre (-11.7, 0, 0.3), size 5.2 x 8.8 x 0.6 | m |
| `pads` | three rune pads on a 12 m arc, 3 m chord spacing: left (-11.625, -2.976), centre (-12, 0), right (-11.625, 2.976), top z 0.6 | m |
| `seat`, `seatedEyeHeightAbovePadM`, `eyeForwardOfSeatedOriginM`, `eyeAboveSeatedOriginM` | seat block; eye 1.2 m above the pad; seated origin 0.62 m below and 0.08 m behind the eyes | m |
| `cameraFovDeg`, `lookTargetM` | desktop camera 90 deg, looking at (0, 0, 0.9) | deg, m |
| `mouseLook` | yaw limit 100 deg, pitch -32 / +18 deg, 0.08 deg/pixel | deg |
| `blink` | fade out 0.12 s, fade in 0.2 s, vignette peak 1.15, debug keys F5/F6/F7 | s |
| `spawnArc`, `spawnMarkers[]`, `markerPost` | marker posts within +-70 deg at 8 m and 15 m from the centre pad | m, deg |
| `braziers[]`, `banners[]`, `colours`, `lighting` | greybox props, flat linear colours, dusk light | - |
| `threats` | threat-language demo: chest height 1.35 m, ring 1.7 -> 0.28 m, and `order[]` water / fire / steel / unblockable (colour, shape, flight time, landing) | m, s |
| `hands` | hand drawing and the 140 deg ward ribbon | m |

### `apps/vr/data/vr/teach.json`

Loaded by `FArenaSession::LoadTeachData` (`SessionFlow.cpp:63-145`) at `BeginArc` and at every `Start`. A missing file or
key logs a warning and keeps the built-in default in `ArenaSession.h:40-78` (same values today). No `_about` fields.
Meanings are in [08-session-arc.md](08-session-arc.md) (teach) and
[09-pause-comfort-accessibility.md](09-pause-comfort-accessibility.md) (pause).

| Key group | Keys and values | Units |
|---|---|---|
| Cold and scripted cues | `coldHoldS` 1.5, `absorbLeadS` 0.55, `perfectLeadS` 0.22, `blinkLeadS` 0.40 | s |
| Ward glob | `wardDistanceM` 8.0, `globSpeedMps` 4.0, `wardWindupS` 1.0, `globDamage` 12.0, `globRadiusM` 0.28, `globMarkerWidthM` 0.6, `globRangeMarginM` 4.0, `globSettleS` 0.8, `perfectRingS` 1.1, `perfectTries` 3 | m, m/s, s, HP, count |
| Beats | `beatAfterAbsorbS` 1.6, `beatAfterWardS` 16.0, `beatAfterSigilS` 14.0, `beatAfterBlinkS` 14.0 | s |
| Sigil | `dummyHp` 200.0, `dummyDistanceM` 6.0, `sigilPromptS` 8.0, `sigilRetryS` 8.0 | HP, m, s |
| Blink lane | `blinkWindupS` 1.4, `blinkWidthM` 1.2, `blinkRangeM` 8.0, `blinkDamage` 18.0, `blinkSettleS` 0.5 | s, m, HP |
| Pause and offers | `trackingLossS` 1.5, `resumeStepSeconds` 1.0, `resumeStepCount` 3, `offerHoldS` 0.40 | s, count |

The key is `resumeStepSeconds`, not `countStepS`, because Unreal's JSON object is case-insensitive and `countStepS` would
collide with `countSteps` (`SessionFlow.cpp:115`).

### `apps/vr/data/vr/strings.json`

Loaded by `FArenaSession::LoadStrings` (`SessionFlow.cpp:147-166`); the arc refuses to start without the key
`cold.start`. A missing key prints the key itself. Flat map of key to English text: `cold.start`, `offer.continue`,
`offer.restart` (not referenced in the session code), `teach.ward`, `teach.ward.perfect`, `teach.sigil`, `teach.blink`,
`pause`, `resume.3/2/1`, `wave.flow`, `wave.clock`, `intermission`, `settings.*` (stone labels and the narrow offer).
One table for later localisation.

### `apps/vr/data/vr/calibration-proposal.json` (not live)

| Item | Value |
|---|---|
| Status | written by the T12 seated census (set `wide`); **not signed off**; DF-003 is an open owner decision |
| Applied | only when `-MageArenaProposal` is on the command line or the environment variable `MageArenaProposal` is `1` or `true` (`VrRulesLoad.cpp:321-329`); then `ApplyProposal` (`VrRulesLoad.cpp:375-432`) overrides the overlay after `combat.vr.json` loads |
| `knobs.wave` | `conscriptCount` -1, `slingerCount` -1 (-1 = keep the pinned count; integer -1 to 8), `spawnDelayS` 0 |
| `knobs.pressure` | `attackCadence` 1, `enemyDamage` 1, `fireMageDamage` 1 (multipliers, 0-3) |
| `knobs.power` | `boltDamage` 1, `lineDamage` 1, `manaRegen` 1.25 (player only) |
| `knobs.defence` | `wardDrainSplit` 1, `oneHandPower` 1, `staffDurationS` 14, `staffManaUpFront` 8, `staffDrainPerSecond` 1.5, `staffMagicReduction` 0.92, `staffPhysicalReduction` 0.90, `fireWallDistanceM` 4, `fireWallArcDeg` 150, `fireWallDurationS` 12, `fireWallManaCost` 6, `fireWallHeatPerStop` 4, `fireWallBurnDamage` 12 |
| `measured` | 20 seeds, `targetsMet` false, wave in window 0.85, wave HP median 0.172, wave time median 39.05 s, duel c1 / c1.5 medians 44.83 / 40.56 s, Sunfall 0.05 / 0.15, win 0.95 / 0.75 |
| How to change | re-run the census (T12 tooling); never copy numbers into `combat.vr.json` without the owner's sign-off |

### `apps/vr/data/vr/styles/` (in progress, uncommitted)

Style skins `style-13.json`, `style-25.json`, `style-30.json` for the T16 style-pick capture, selected with
`-MageArenaStyle=`. Presentation only; they change no simulation number. Not part of 46643e2.

## Test data

### `apps/vr/data/conformance/*.json` (45 vectors)

| Item | Value |
|---|---|
| Owner | generated, never hand-edited: `node apps/vr/tools/conformance/generate.mjs` runs the TypeScript oracle at the pinned commit (`MAGE_ARENA_ARENA`, default `C:/Users/kazda/kiro/mage-arena-tv`) |
| Loaded by | `Tests/KernelConformanceTests.cpp:23` (`MageArena.*` suite) |
| Keys | `name`, `note`, `seed`, `driver` (`arena` 32, `games` 7, `training` 4, `mage` 1, `enemies` 1), `trainingKind`, `checkpointEvery`, `tolerance` (relative 1e-6, absolute 1e-9), `ticks`, `replaySetup`, `setup` (actors, ranks, compositions), `frames`, `ops`, `events`, `checkpoints`, and `games` for games-driven vectors |
| Rule | must regenerate byte-identically; the VR overlay is never passed on this path (rules null) |
| How to change | only by re-pinning and regenerating |

### `apps/vr/data/scenarios-vr/*.json`

| Item | Value |
|---|---|
| Owner | the VR C++ kernel (reference for Fire until the desktop/TV channel ports it; DECISIONS 2026-10-03) |
| Files | `fire-duel-competence-1.json` (seed 40000, wave 2), `fire-duel-competence-1-5.json` (seed 40001, wave 3) |
| Keys | `seed`, `wave`, `outcome`, `phase`, `ticks`, `durationS`, `hash` (C++ FNV state hash), `sunfallCast`, `sunfallRelease`, `sunfall`, `hp`, `mana`, `wavesCleared`, `kind` |
| Loaded by | `Tests/FireTests.cpp:487-489, 1205` (compares a fresh run field by field) |
| How to change | only when Fire rules or pinned data change on purpose; kept separate from the TS vectors so those stay byte-identical |

## Other files the build reads (not tuning)

| Path | What | Loaded by |
|---|---|---|
| `apps/vr/Game/Clips/<action>.<variant>.jsonl`, `Clips/mirror/` | recorded or synthetic hand clips (keys play them) | `HandClip.cpp:84-91` |
| `apps/vr/Game/Clips/templates/` | sigil recognizer templates | `SigilRecognizerSubsystem.cpp:15` |
| `apps/vr/art/hud/mask/*.png` | cuff masks | `SessionPresentation.cpp` |
| `Saved/MageArena/settings.json`, `Saved/MageArena/bout.txt` | player settings, saved bout | `MageSettings.cpp:298`, `SessionFlow.cpp:177-183` |

## Status and gaps

- `combat.json` `aimMode` and `threatLanguage`, `runtime.json` `water.decoyRadiusM` and `water.reportCastCadenceS`, and
  `pacingTargets` other than `deadAirBucketS` are not read by the C++ build.
- Plan section 3 lists the overlay's intended contents as "pads, blink, sigil timing, ward-detection latency,
  narrow-FOV spawn arc". Build: pads and blink fades are in `arena-layout.json`; blink cost is the pinned roll; sigil
  timing and ward-detection thresholds are in code and clips (`WardDetector.h` constants), not in the overlay.
- `combat.vr.json` carries proposal values (split hands, planted staff, fire wall) that are live but not signed off.

## Open questions

- Should the seated design-gate windows (25-40, 30-45, 45-80 s) be read from `pacingTargets` instead of test literals?
- Should a VR-owned player preset (for Reflection and the Leviathan pick) live in the overlay, since `runtime.json`
  presets are pinned?
