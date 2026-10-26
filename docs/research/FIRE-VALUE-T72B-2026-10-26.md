---
title: T72b Fire value contrast with flame-lick tips - built, measured, missed Athena's pass bar, reverted to T67
channel: vr
date: 2026-10-26
status: RAN AND REVERTED - desktop proxy, not Quest truth. T72b (flame A/B backing at half T72's growth with crisp lick tips, the spark streak back to T67) kept T72's value gain on the bolt (-0.57 Weber vs T67 -0.39) and won the bolt on all three of Athena's bars in all three blind rounds. The absorb (cell 7, kept as T72's by ruling 3) was named rust or mud, and the ring (cell 14, T67's by ruling 1) was named rope, in every round. The pass bar therefore failed, and the card's fallback ran. The Fire cells are T67's again, byte-identical, re-imported, MageArena. 342/342, D-G4 max 127. T72b's cells stay in d948720.
sources: [apps/vr/tasks/T72b-fire-lick-tips.md, apps/vr/tasks/T72-fire-value-contrast.md, docs/research/FIRE-VALUE-T72-2026-10-26.md, docs/research/SPELL-SPECTACLE-R2-T67-2026-10-26.md, docs/concepts/VISUAL-PLAN.md (b), (c), apps/vr/art/key-art/key-art-s5101-medium.jpg]
---

# T72b Fire value contrast with flame-lick tips, 2026-10-26

**Label: desktop proxy (D3D12, forward shading, `-nohmd`), not Quest truth.** Engine UE 5.8.3 (`Build.version` read before
the first build). Nothing under `apps/vr/data/` changed, and no combat number moved. `Session/SpellVfx.{h,cpp}`,
`SpellVfx::GlowColour`, `RimPulse`, `RimPulseColour`, the braziers, the crowd tint and the hound material are unchanged.
`M_SpellGlow` was re-imported with the atlas twice (T72b, then the revert) with no parameter change: `customdata=3` both
times.

Evidence is in `C:/Users/kazda/mage-art-staging/fire-value-t72b-2026-10-26/` (`<out>/`):

- the stills: `T72b-before/` (T72 content) and `T72b-after-r1/` (T72b);
- the atlas renders and previews: `atlas/t67`, `atlas/t72`, `atlas/v1`-`v4` (the sweep) and `atlas/t72b`;
- the critic triples `critic-1/`-`critic-3/` and their keys `critic-N-key/blind-key.json` (seeds 7301-7303), plus
  `critic-verdicts-unblinded.md`;
- the D-G4 tables: `budget-before/`, `budget-after/` (T72b) and `budget-revert/`;
- every build, import, test, suite, capture and budget log: `logs/`;
- the scratch tools: `tools/` (`measure.mjs`, `fnv.mjs`, `sheet.py`, `make-triple.py`, and T72's `fire-contrast.py` and
  `dg4-table.py`).

## 1. What T72b built (commit d948720), per Athena's ruling

| Cell | T67 | T72 | T72b |
|---|---|---|---|
| Flame A and B (4, 5), backing | 1.08x the body, 0.40-0.46, hard edge | body grown evenly by 0.1, 0.24-0.36, soft ramp 0.03 | body grown by 0.05, tapered along each tongue to 0.01 at the tip, T67's 0.012 ramp, 0.22-0.34 |
| Flame star (7) | 0.46-0.82 | backing star grown by 0.07, 0.24-0.30 | T72's, unchanged (ruling 3) |
| Sparks in cells 4-7 | rim to 0.5, halo 0.55 | rim 0.26, halo 0.22 | T72's, unchanged |
| Spark streak (14) | tail 0.3 at 0.46, T67 sparks | thin band 0.27 at 0.28, T72 sparks | T67's cell exactly (ruling 1) |

`spark()` takes a `t67` flag, so only the streak's three sparks take T67's rim and halo. Cells 4-7 keep T72's.
`flameLayer` takes a `tipGrow`: the growth runs from `grow` at the tongue's root to `tipGrow` at its tip, following the
tongue's own width taper. With `tipGrow = grow` it is pixel-identical to T72 (checked). Atlas measures (`tools/measure.mjs`):

| Cell | Edge band 1-3 px, T67 / T72 / T72b | Dark cap over the lick tips, mean px, T67 / T72 / T72b | Heart share (>= 0.08) T72b |
|---|---|---|---|
| 4 flame A | 0.469 / 0.298 / 0.372 | 0 / 11.26 / 3.05 | 0.127 |
| 5 flame B | 0.476 / 0.311 / 0.379 | 0 / 8.61 / 3.23 | 0.123 |
| 14 streak | 0.567 / 0.375 / 0.567 | - | 0.197 |

The Water, Air, SoftGlow, droplet and grit cells were pixel-identical to T67 and T72 (per-cell hash), cells 6 and 7 to
T72, and cell 14 to T67. Every cell stayed a grey mask with shade at most 1, with no white core.

## 2. Tests, red first

- **Red on the T72 atlas** (the 732ca65 content, the new tests built in; `logs/test-red.log`): `Found 12 automation tests
  based on 'MageArena.SpellVfx'`, `EXIT CODE: -1`:
  - `Expected 'fire cell 4: the dark cap over the lick tips is thin (mean 11.26 px over 76 columns, at most 4.5)' to be true`
  - `Expected 'fire cell 5: the dark cap over the lick tips is thin (mean 8.61 px over 76 columns, at most 4.5)' to be true`
  - `Expected 'the spark streak is T67's cell (FNV-1a 0x6e768181, T67 0xd64783f6)' to be true`
  - `Expected 'the spark streak keeps T67's light outline (1-3 px band 0.375, at least 0.50)' to be true`
- The C++ hash of T72's cell (0x6e768181) equals the node-side value from `tools/fnv.mjs`, so the two implementations
  agree.
- **Green on T72b** (`logs/test-green.log`): 12/12, `EXIT CODE: 0`.
- `FireShadeNeverAboveOne` and the heart-share test (`FireCellsHaveHotCoreAndDarkBody`) stayed throughout.
  `FireCellsHaveDarkRimAtSilhouette` took the streak out of its bound and loosened flames A/B to an edge of at most 0.40
  with no 4-6 px bound. It was then removed in the revert (section 6), because it holds T72's rim, which T67 does not have.

## 3. The value contrast, T67 / T72 / T72b

T72's `fire-contrast.py`, unchanged: the edge band of the saturated fire footprint where it borders the sand, against the
sand 4-12 px outside, both in linear luminance. Weber = (edge - sand) / sand. The bolt is scaled 3.5/8 to its 8 m size.

- **The T67 column** is T72's T67 stills (`fire-value-t72-2026-10-26/T72-before/shots`, captured from 6afd843). They were
  reused after a byte check: all 11 files match their manifest's byte counts and pixel checks
  (`logs/t67-stills-validity.txt`), and they reproduce T72's measured T67 numbers exactly.
- **The T72 column** is this run's `T72b-before` capture (T72's content, this branch's binary).

| Shot | T67 edge / sand / Weber | T72 edge / sand / Weber | T72b edge / sand / Weber |
|---|---|---|---|
| 04 fire bolt, 8 m size | 0.184 / 0.303 / **-0.394** | 0.128 / 0.297 / **-0.569** | 0.130 / 0.302 / **-0.568** |
| 05 fire absorb | 0.194 / 0.276 / **-0.298** | 0.127 / 0.276 / **-0.539** | 0.138 / 0.286 / **-0.518** |
| 06 fire perfect | 0.227 / 0.272 / **-0.168** | 0.127 / 0.232 / **-0.452** | 0.227 / 0.272 / **-0.167** |

The bolt keeps T72's full gain with a third of T72's dark cap on the licks. The ring is T67's, as ruling 1 asks, so its
contrast is T67's.

## 4. D-G4, before, after and after the revert

`capture-budget.ps1 -Runs 3 -ExtraArgs -MageArenaCreatureImportScene`. Every row is the **creature-import scene**: 20
enemies, 100 projectiles, both hands, 6 Niagara systems, the three-creature cast, 60 s of samples, offscreen 1920x1080.
Every run ended `ok=True done=True`, and each set ended `budget: ok (3 runs)`.

| Run | Scene | Draw calls median / max | BasePass tris median / max | Live particles max | Glow puffs | Glow layers |
|---|---|---|---|---|---|---|
| before 1 (T72 content) | creature-import | 124 / 127 | 217,866 / 219,042 | 1,468 | 400 | 2 |
| before 2 (T72 content) | creature-import | 124 / 127 | 216,386 / 219,042 | 1,469 | 400 | 2 |
| before 3 (T72 content) | creature-import | 123 / 127 | 217,434 / 218,994 | 1,468 | 400 | 2 |
| T72b 1 | creature-import | 123 / 127 | 216,386 / 219,042 | 1,468 | 400 | 2 |
| T72b 2 | creature-import | 124 / 127 | 216,866 / 219,042 | 1,467 | 400 | 2 |
| T72b 3 | creature-import | 123 / 127 | 216,866 / 219,042 | 1,467 | 400 | 2 |
| revert 1 (T67 cells) | creature-import | 124 / 127 | 216,698 / 219,042 | 1,468 | 400 | 2 |
| revert 2 (T67 cells) | creature-import | 123 / 127 | 216,698 / 219,042 | 1,468 | 400 | 2 |
| revert 3 (T67 cells) | creature-import | 124 / 127 | 216,698 / 219,042 | 1,468 | 400 | 2 |
| Limit | | max <= 150, not above 127 | <= 350,000 | particles + puffs <= 2,000 | | |

- Draw calls: +0 in every run.
- Particles + puffs: at most 1,869 (before), 1,868 (T72b) and 1,868 (revert).
- The "before" binary is 732ca65's content plus the two test commits. Test code does not change the scene.

## 5. The blind critic on T67 / T72 / T72b triples

`tools/make-triple.py`: per shot, the three versions in a seeded random order as A, B and C. Each side has a half-size
full frame and a crop, plus a side-by-side strip of the three crops. Crops are as in T72: the bolt and the stone are
scaled to their 8 m size and shown x2 nearest. Seeds 7301, 7302 and 7303; the keys are in `critic-N-key/`. Each round was
a fresh agent that saw only its folder and S1. It was asked about:

- value separation from the sand;
- whether each version reads as flame (naming brown, rust, mud, rope, blob, wreath, sticker or smoke);
- closeness to S1;
- any white core;
- any unblockable read;
- whether any version is worse for its school.

**Round 1 already showed** that the two shots the ruling fixed (05 and 06) were named rust/mud and rope. The only free
lever (cells 4 and 5) feeds the bolt (04) alone, which already passed. So rounds 2 and 3 ran as **replications on the same
images**, with fresh critics and new seeds, not as tuning rounds.

Unblinded (`critic-verdicts-unblinded.md`):

| Shot | Separation r1 / r2 / r3 | Flame-read words for T72b r1 / r2 / r3 | Closer to S1 r1 / r2 / r3 |
|---|---|---|---|
| 04 fire bolt | T72 > **T72b** > T67 / **T72b** > T72 > T67 / T72 > T67 = **T72b** | "crisp, separate pointed tongue tips", slight sticker / "reads best: crisp, separate flame-lick tips" / "crisp lick tips" | **T72b** > T67 > T72 / **T72b** > T67 > T72 / T67 > **T72b** > T72 |
| 05 fire absorb | T72b = T72 > T67 / T72 = T72b > T67 / T72 = T72b > T67 | "mottled mud/rust edge" / "brown or rust at the rim, splat" / "brown/mud edges, smudge" | T67 > T72b = T72 / T72 = T72b > T67 / T72 = T72b > T67 |
| 06 fire perfect | T72 > T67 = T72b (all rounds) | "rope / wreath" (all three versions) / "braided wreath or rope" (all) / "soft smoky-edged rope" (T72b and T67) | T67 = T72b > T72 / T72b = T67 > T72 / T72 > T72b = T67 |

T72's own words, every round, were "brown/rust, blob, blunt top", "muddy brown body" and "brownish dark edge, blob" on the
bolt, and "most rope-like", "thorn wreath" and "thorny brown wreath" on the ring.

**Other schools.** Water, Air, the unblockable and the stone were SAME, except:

- the water bolt, where T67's still reads "smoky/haze fringe";
- the unblockable, where one version's red rim reads thicker.

Both are capture noise: the shell flicker phase and the rim's pulse step. The water and unblockable cells and code did not
change. The stone reads "flat light-grey disc", "near off-white", in all three versions.

**No white or yellow-white core** in any version or round, and nothing read as the unblockable except the unblockable.

### Pass-bar verdict (Athena's, on the fire triples)

| Bar | 04 bolt | 05 absorb | 06 perfect |
|---|---|---|---|
| (1) separates at least as well as T67 | holds (r1, r2 better; r3 equal) | holds (better in all rounds) | holds (equal: T67's cell) |
| (2) reads as flame, no brown, rust or rope named | **holds** (all rounds) | **fails** (rust/mud, all rounds) | **fails** (rope, all rounds; T67 named too) |
| (3) closer to S1 than T72 | **holds** (all rounds) | **fails** (= T72 in all rounds) | holds r1 and r2, fails r3 |

**Verdict: the pass bar is not met.** The two failing shots come from the cells the ruling kept:

- the absorb draws the flame star (cell 7, T72's by ruling 3) and the streak beads;
- the ring draws only the streak (cell 14, T67's by ruling 1).

Nothing inside the ruling could move them. The bolt, the shot the ruling did open, met all three bars in every round.

## 6. The fallback: Fire cells back to T67 (commits 15deb8e, 7672232)

- `spell-flipbook.mjs` and `manifest.json` are byte-identical to 6cbe7d9's. The PNG's LFS oid
  (`7f493b87...`) equals T67's, and so does the pixel hash (`c8185da4...`).
- The atlas was re-imported: `MAGEVR_BUDGET_FX T_SpellFlipbook 512x512 srgb=0 mipgen=0 group=13 md5=b1c31164...` and
  `MAGEVR_BUDGET_FX M_SpellGlow blend=translucent shading=unlit twosided=1 instanced=1 flipbook=T_SpellFlipbook
  customdata=3`.
- Tests: T72's `FireCellsHaveDarkRimAtSilhouette` was removed (it held T72's rim). `FireLickTipsStayCrisp` (T67: 0 px),
  `SparkStreakIsT67sCell`, `FireShadeNeverAboveOne` and the heart-share test stay, and all pass on T67's cells.
- T72b's cells remain in history at d948720. `git revert 7672232 15deb8e` restores them, plus a re-import.

## 7. Gates (final state, T67 cells)

- `build.ps1`: `Result: Succeeded` for the test build (441 s, `logs/build-tests.log`) and the revert build
  (`logs/build-revert.log`). `MageArena.SpellVfx` on the revert: 11 tests, `EXIT CODE: 0`.
- `MageArena.`: corpus regenerated with `generate.mjs --corpus 30 --seed 1000` and `mousepaths.mjs`, and `git status`
  stayed clean.
  - On T72b: `Found 343 automation tests based on 'MageArena.'`, 343 x `Result={Success}`, `EXIT CODE: 0`
    (`logs/suite.log`).
  - On the revert: `Found 342 automation tests based on 'MageArena.'`, 342 x `Result={Success}`,
    `**** TEST COMPLETE. EXIT CODE: 0 ****` (`logs/suite-revert.log`).
  - The DF-011 `MageArenaDesign` reds were not run, and nothing here touches them.
- D-G4: section 4, every set `budget: ok (3 runs)`, max 127.
- `node apps/vr/tools/vfx/spell-flipbook.mjs --check`: `spell-flipbook: OK 512x512, pixels c8185da4...` on the committed PNG.
- At most one Unreal process ran at any time. `Binaries`, `Intermediate`, `Saved` and `runs/` are not committed. The PNG
  and both `.uasset` files are in LFS.

## 8. Open points for Athena

- **The bar and the ruling conflict.** Rulings (1) and (3) fix the two shots that fail bar (2). So "T72b passes" was out of
  reach without touching cells she froze. T72b on the bolt alone is a clean win at 8 m: T72's separation, crisp licks, no
  brown named, and closer to S1 than T72 in every round. Options:
  - (A) Keep the revert (T67 everywhere). This is what the card's fallback did.
  - (B) Restore T72b (`git revert 7672232 15deb8e`, re-import) and accept the absorb's rust read as a known cost.
  - (C) Take T72b's flame A/B only, with the star and the streak at T67. The absorb then loses its value gain (-0.30), and
    the bolt keeps it.
  - Recommendation: (C). The bolt is the shot seen in every duel at duel range. The absorb and the ring happen at the ward,
    1.1 m from the player, where separation from the sand matters least. (C) is one small card, with no new ruling on hue.
- **The ring reads as rope even in T67's own cell**, in all three rounds. Rope is a shape problem of the bead ring, not a
  value one, and it is outside this card.
- **Quest truth** is unmeasured: ASTC on the 0.22-0.34 band, and whether the narrow backing survives the Quest mips at 8 m.
- **The steel stone** reads near off-white, as a "flat sticker disc", to every critic. That is an option for the owner's
  colour language, not a lane (VISUAL-PLAN (b)).
