---
title: T67 spell spectacle round 2 - Fire volume and contrast on the T63 glow layer, at the same call cost
channel: vr
date: 2026-10-26
status: RAN - desktop proxy, not Quest truth. Fire has nested flame layers, a darker outline and a full-colour heart in its atlas cells, and larger shell, ember, flame-star and ring-bead cards, at the same puff count. D-G4 max draw calls 127 in all six runs (before and after alike), BasePass 219k, particles + puffs 1,868 of 2,000. MageArena. passed 338/338 (336 at the base plus 2 new tests). Blind critic, round 2, after vs before - all 3 fire pairs won by the after frame (1 VISIBLE GAIN bolt, 1 MINOR absorb, 1 VISIBLE GAIN ring), no school pair worse. Athena's visual acceptance is pending.
sources: [apps/vr/tasks/T67-spell-spectacle-r2.md, docs/research/SPELL-FLIPBOOK-T63-2026-10-25.md, docs/concepts/VISUAL-PLAN.md (b), (c), apps/vr/art/key-art/key-art-s5101-medium.jpg]
---

# T67 spell spectacle round 2, 2026-10-26

**Label: desktop proxy (D3D12, forward shading, `-nohmd`), not Quest truth.** Engine UE 5.8.3 (`Build.version` read before
the first build). Nothing under `apps/vr/data/pinned/`, `PINNED.json`, `apps/vr/data/conformance/` or the overlay was
touched, and no combat number moved. `SpellVfx::GlowColour`, `RimPulse`, `RimPulseColour`, the braziers and the crowd tint
are unchanged. Water, Air and the Unblockable cells, cards and code are unchanged.

Evidence: `C:/Users/kazda/mage-art-staging/spell-spectacle-r2-t67-2026-10-26/` (`<out>/`): the before stills
(`T67-before/`), the after stills of each round (`T67-after-r1/`, `T67-after-r2/`), the critic images (`critic-N/`) and their
keys (`critic-N-key/blind-key.json`), the atlas previews over the sand (`preview-before/`, `preview-r1/`, `preview-r2/`),
the D-G4 summaries (`budget-before/`, `budget-after/`), the rival replay (`rival2/`) and every build, capture, budget and
suite log (`logs/`).

## 1. What changed

| Lever | Change | Where |
|---|---|---|
| Flame cells A and B | Four nested flame layers (a dark backing outline at shade 0.4-0.46, the outer body 0.52-0.68, a mid flame 0.74-0.9, a heart 0.94-1.0), ten licks on the outer layer, heat rising from the base | `apps/vr/tools/vfx/spell-flipbook.mjs` (`flameLayer`, `fireFlame`) |
| Embers cell | Eleven sparks (six before), larger toward the head | same (`fireEmbers`) |
| Flame star | A twelve-lick star, a nine-lick mid star and a six-lick heart, each with its own shades, and sixteen sparks | same (`fireCrown`, `burst`, `sdfLayer`) |
| Spark streak | A wider dark tail with a lit core and two trailing sparks | same (`sparkStreak`) |
| Card sizes | Fire shell 1.15 wide x 1.45 tall of the T63 shell; ember cluster x1.25; flame star x1.15; streak beads (splash, ring) x1.3. Puff count and centres unchanged | `Session/SpellVfx.h` (`FireShellWidth`, `FireShellHeight`, `FireEmberScale`, `FireCrownScale`, `FireStreakScale`), `SpellVfx.cpp` (`BoltPuffs`, `SplashPuffs`, `PerfectRingPuffs`) |
| Atlas, assets | Re-rendered atlas and manifest, `T_SpellFlipbook.uasset` and `M_SpellGlow.uasset` re-imported (`create-budget-fx.ps1 -SpellVfx`), all in LFS | `apps/vr/art/vfx/`, `apps/vr/Game/Content/Vfx/` |
| Tests | `FireShellHasVolume`, `FireCellsHaveHotCoreAndDarkBody` | `Tests/SpellVfxTests.cpp` |

`M_SpellGlow` parameters were not changed (`GlowGain` stays 1). The cells' shade stays at most 1, so no pixel is brighter
than the threat colour, and there is no white hot core: the "heart" is the full element colour, the outer layers are
darker tones of it. Per-instance custom data is unchanged (still three floats), so each colour is one layer.

## 2. D-G4 before and after

`capture-budget.ps1 -Runs 3 -ExtraArgs -MageArenaCreatureImportScene` (20 enemies, 100 projectiles, both hands, 6 Niagara
systems, the three-creature cast from `/Game/Characters`, 60 s of samples, offscreen 1920x1080). All six runs ended
`ok=True done=True`, scene parity ok (hash `40507278...`), each set `budget: ok (3 runs)`. The baseline is master `2d5d46a`
(its max is 127, not T63's 126: the cast is the DINOv3 import).

| Run | Draw calls median / max | BasePass tris median / max | Translucency tris median / max | Live particles median / max | Glow puffs | Glow layers |
|---|---|---|---|---|---|---|
| before 1 (2d5d46a) | 123 / 127 | 216,698 / 219,906 | 23,137 / 23,152 | 1,398 / 1,467 | 400 | 2 |
| before 2 | 124 / 127 | 216,866 / 219,042 | 23,136 / 23,144 | 1,398 / 1,469 | 400 | 2 |
| before 3 | 124 / 127 | 216,386 / 219,042 | 23,139 / 23,148 | 1,397 / 1,468 | 400 | 2 |
| after 1 | 123 / 127 | 216,386 / 219,042 | 23,011 / 23,134 | 1,395 / 1,467 | 400 | 2 |
| after 2 | 124 / 127 | 216,866 / 219,042 | 23,116 / 23,132 | 1,391 / 1,468 | 400 | 2 |
| after 3 | 124 / 127 | 217,346 / 218,994 | 23,022 / 23,138 | 1,396 / 1,468 | 400 | 2 |
| Limit | max <= 127 | <= 350,000 | | particles + puffs <= 2,000 | cap 500 | |

Draw calls +0 (127 before and after), triangles flat, puffs 400 under the 500 cap, layers 2. Particles plus puffs: 1,869 at most.

## 3. The blind critic

Pairs from `capture-spell-vfx.ps1` (water, fire, air bolt / absorb / perfect, the unblockable telegraph, the steel stone), each
a half-size full-frame pair plus a crop, A and B shuffled (seeds 6701, 6702), key kept in `critic-N-key/`. Each round a fresh
agent saw only the images and S1. Round 1 drove the fixes (a darker backing outline, a taller and wider shell, larger spark
beads, a richer spark streak); round 2 judged the final atlas and sizes. The loop stopped after 2 of the allowed 3 rounds.

**Round 1 (unblinded):** fire bolt after, MINOR to VISIBLE; fire absorb after, VISIBLE GAIN; fire perfect SAME. The critic
named the flat disc and the thin ring. Water pairs 01 and 02 went to the before frame (MINOR); the water cells and code are
untouched, so that is capture noise.

**Round 2 (final, unblinded):**

| Shot | Closer to S1 | Strength | Colour class | Glow brighter than threat |
|---|---|---|---|---|
| 01 water bolt | after | MINOR | turquoise | none |
| 02 water absorb | SAME | SAME | turquoise | none |
| 03 water perfect | after | MINOR | turquoise | none |
| 04 fire bolt | after | VISIBLE GAIN | ember orange, no drift ("outer rim a bit darker, separates from the sand better") | none |
| 05 fire absorb | after | MINOR | orange | none |
| 06 fire perfect | after | VISIBLE GAIN | "slightly deeper orange, not yellow or brown" | none |
| 07 air bolt | SAME | SAME | green | none |
| 08 air absorb | SAME | SAME | green | none |
| 09 air perfect | after | MINOR | green | none |
| 10 unblockable telegraph | SAME | SAME | black core, red rim | none |
| 11 steel stone | SAME | SAME | steel, no glow | none |

No class break, no glow brighter than its threat, and no pair got worse. What the critics still say falls short of S1 for fire:
a yellow-white hot core, a glowing halo that lifts the fire off the sand, and a flame that fills the frame. The first two are
ruled out by the colour language (no white hot core on absorb cues; ElementColour owns brightness); the third is a size
call. Fire on orange sand stays the weakest contrast in the set, as in T63.

## 4. Gates

- `build.ps1`: `Result: Succeeded` for the baseline, the red-test build and each of the two revisions (`logs/build-*.log`).
- **Tests, red first:** the two new tests were run against the baseline atlas and sizes (`logs/test-red.log`):
  `FireShellHasVolume` failed (shell 52.8 x 52.8 cm) and `FireCellsHaveHotCoreAndDarkBody` failed (cell 4 dark 0.43, heart 0.06;
  cell 5 dark 0.37; cell 7 heart 0.03). A ring-bead assertion was added to `FireShellHasVolume` with the round-2 code and was
  not run red.
- `MageArena.`: `Found 338 automation tests`, 338 x `Result={Success}`, `TEST COMPLETE. EXIT CODE: 0`
  (`logs/suite-final.log`); the base had 336.
- `capture-rival.ps1` **does not run on master**, with or without this change: its scratch import of Brennic now writes
  `/Game/Characters/...` and fails the scale check (`MAGEVR_CREATURE_IMPORT fail Brennic height 0.0180 m is more than 5 % off
  1.7992 m`), since the Characters import (`16ffa56`). Its replay was run with the script's own arguments against the committed
  `/Game/Characters`: `MAGEVR_CAPTURE_DONE shots=113`, no `MAGEVR_CAPTURE_FAIL`, every shot on disk at the measured size and
  past the pixel floor (`rival2/`). The script needs a fix by a card that touches it (it is not in this card's paths).
- The atlas generator's `--check` passes on the committed PNG. `Binaries`, `Intermediate` and `Saved` were not committed.

## 5. Open points for Athena

- **Visual acceptance is pending.** Fire is bigger and layered with a darker outline, but still an even orange on an orange floor.
- **Quest truth** is unmeasured: the overdraw of the larger fire cards (the shell is now 1.15 x 1.45 of T63's) and ASTC banding
  on the darker shades.
- **The task's path list** omitted `apps/vr/tools/vfx/spell-flipbook.mjs` and `apps/vr/art/vfx/`, where the atlas source lives;
  they were edited because the atlas cells are the first lever. Flagged for the Director.
- **capture-rival.ps1** is stale since `16ffa56` (section 4).
- The next lever is size, not shade: a fireball that dominates the frame as in S1 is a card-scale and bolt-core call for a
  later card.
