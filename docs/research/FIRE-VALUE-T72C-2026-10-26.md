---
title: T72c Fire bolt only - built, proven by atlas diff, missed the blind bolt bar, reverted to T67
channel: vr
date: 2026-10-26
status: RAN AND REVERTED - desktop proxy, not Quest truth. T72c restored only T72b's flame A/B cells (4, 5) on T67 (Athena's option C). The atlas diff proves it (cells 4 and 5 = d948720's, every other cell = f557790's, 0 differing pixels) and the bolt keeps its value gain (-0.557 Weber vs T67 -0.394, T72 -0.569). The bolt-only blind critic then named rust on T72c in all three rounds and ranked it last on separation in round 3, so parts (a) and (b) of the bar failed and the card's fallback ran. The Fire cells are T67's again, byte-identical, MageArena. 342/342. T72c's cells stay in c24e16c.
sources: [apps/vr/tasks/T72c-fire-bolt-only.md, apps/vr/tasks/T72b-fire-lick-tips.md, docs/research/FIRE-VALUE-T72B-2026-10-26.md, docs/concepts/VISUAL-PLAN.md (b), (c), apps/vr/art/key-art/key-art-s5101-medium.jpg]
---

# T72c Fire bolt only, 2026-10-26

**Label: desktop proxy (D3D12, forward shading, `-nohmd`), not Quest truth.** Engine UE 5.8.3 (`Build.version` read before
the first build). Nothing under `apps/vr/data/` changed, and no combat number moved. `Session/SpellVfx.{h,cpp}`,
`SpellVfx::GlowColour`, `RimPulse`, `RimPulseColour`, the braziers, the crowd tint and the hound material are unchanged.
`M_SpellGlow` was re-imported with the atlas (`customdata=3`, no parameter change). Evidence is in
`C:/Users/kazda/mage-art-staging/fire-value-t72c-2026-10-26/` (`logs/`, `tools/`, `atlas/`, `T72c-before/`, `T72c-after/`,
`critic-N/`, `critic-N-key/`, `critic-verdicts-unblinded.md`).

## 1. What changed

`spell-flipbook.mjs`: `flameLayer` takes `grow`, `tipGrow` and `soft` (without them it is T67's layer), and `spark()` takes a
`dark` flag (T72's rim band 0.26 and halo 0.22; default T67's 0.5 / 0.55). Only `fireFlame` (cells 4, 5) passes them: the
backing grown 0.05 tapering to 0.01 at each lick tip, T67's crisp edge ramp, rim 0.22 and shade 0.28-0.34, the heart 0.46, and
`dark` sparks. The star (cell 7, `fireCrown`), the embers (6) and the streak (14) call the unchanged defaults.

## 2. Proof by atlas diff (`tools/atlas-diff.mjs`, differing RGBA pixels per cell)

| Cell | new vs d948720 (T72b) | new vs f557790 (T67) |
|---|---|---|
| 4 flame A | **0** | 2,753 (the intended change) |
| 5 flame B | **0** | 2,898 (the intended change) |
| 6 embers | 4,316 (T72b had T72's) | **0** |
| **7 flame star** | 4,036 (T72b had T72's) | **0** |
| **14 spark streak** | 0 | **0** |
| 0-3, 8-13, 15 (Water, Air, SoftGlow, droplet, grit) | 0 | **0** |

`spell-flipbook.mjs --check`: `OK 512x512, pixels 9aedd68a...`. Edge band 1-3 px and dark cap over the lick tips, cells 4 / 5:
0.372 / 0.379 and 3.05 / 3.23 px (T67 0.469 / 0.476 and 0 px; T72 11.26 / 8.61 px). Cells 7 and 14 hash to T67's
(FNV-1a `0xbea049f2`, `0xd64783f6`).

## 3. Tests, red first

Added to `SpellVfxTests.cpp`: `FireLickTipsStayCrisp` (cap 1.0 to 4.5 px and edge band 0.15 to 0.40 on cells 4 and 5),
`SparkStreakIsT67sCell` and `FlameStarIsT67sCell` (hash pins). Red on the f557790 (T67) atlas (`logs/test-red.log`, `Found 11
automation tests based on 'MageArena.SpellVfx'`, `EXIT CODE: -1`):

- `Expected 'fire cell 4: the dark cap over the lick tips is thin but present (mean 0.00 px over 79 columns, 1.0 to 4.5)' to be true`
- `Expected 'fire cell 4: the 1-3 px edge band is a dark ember tone (mean shade 0.469, 0.15 to 0.40)' to be true`
- `Expected 'fire cell 5: the dark cap over the lick tips is thin but present (mean 0.00 px over 80 columns, 1.0 to 4.5)' to be true`
- `Expected 'fire cell 5: the 1-3 px edge band is a dark ember tone (mean shade 0.476, 0.15 to 0.40)' to be true`

The star and streak pins were green on T67 by design (they guard the other cells; the star pin was split out into its own test
after the red run, to reach 343). Green on T72c: 11/11, `EXIT CODE: 0`, and the full suite `Found 343 automation tests based on
'MageArena.'`, `EXIT CODE: 0`. `FireShadeNeverAboveOne` and the heart-share test stayed. After the revert the tests went back to
f557790's, so the suite is `Found 342`, `EXIT CODE: 0`.

## 4. Bolt contrast (`fire-contrast.py`, shot 04 scaled 3.5/8 to the 8 m size)

| Version | edge | sand | Weber |
|---|---|---|---|
| T67 (`T72c-before`, f557790) | 0.1837 | 0.3030 | -0.394 |
| T72 (T72b's `T72b-before` capture) | 0.1281 | 0.2972 | -0.569 |
| **T72c** | 0.1342 | 0.3030 | **-0.557** |

(Information only; shots 05 / 06: T72c is T67's, -0.309 / -0.183 against T67 -0.327 / -0.182, T72 -0.498 / -0.465.)

## 5. D-G4 (`capture-budget.ps1 -Runs 3 -ExtraArgs -MageArenaCreatureImportScene`)

Every row is the **creature-import scene**. Each set ended `budget: ok (3 runs)`.

| Run | Scene | Draw calls median / max | BasePass tris median / max | Live particles max | Puffs | Glow layers |
|---|---|---|---|---|---|---|
| before 1-3 (f557790) | creature-import | 127 / 130, 126 / 129, 127 / 131 | 216,418 / 218,242; 216,698 / 219,042; 217,434 / 219,042 | 1,468 / 1,468 / 1,467 | 400 | 2 |
| T72c 1-3 | creature-import | 127 / 131, 127 / 131, 127 / 130 | 215,962 / 218,242; 217,010 / 219,042; 216,698 / 219,042 | 1,468 / 1,468 / 1,467 | 400 | 2 |
| revert 1-3 (T67 cells) | creature-import | 127 / 130, 124 / 127, 127 / 131 | 217,010 / 219,042; 217,178 / 219,042; 217,434 / 219,042 | 1,468 / 1,468 / 1,467 | 400 | 2 |
| Limit | | max <= 150, not above 127 | <= 350,000 | particles + puffs <= 2,000 | | |

The call cost did not move: draw calls are +0, BasePass within noise, particles + puffs at most 1,868. **The "not above 127"
line is not met by the baseline either:** f557790 itself reads max 129-131 in this worktree on this run (T72b's table read 127),
so the line is host noise, not T72c; the 150 limit holds in every row.

## 6. The blind critic, bolt only (shot 04)

Three fresh agents, each seeing only its folder (S1 and one A/B/C bolt set with a full frame, an 8 m crop x2 and a side-by-side
strip), shuffled T67 / T72 / T72c, seeds 7401-7403, keys in `critic-N-key/`. Run as three parallel rounds of one image set.

| Round (A / B / C) | Separation | T72c's flame-read words | Closer to S1 |
|---|---|---|---|
| 1 (T67 / T72c / T72) | T72c > T72 > T67 | "clearest tongues", slight sticker, "thin dark **rust** rim" | T72c > T67 > T72 |
| 2 (T72 / T72c / T67) | T72c > T67 > T72 | "most flame-like, tall pointed tips", slight sticker, "a bit of **rust** in the lower body" | T72c > T67 > T72 |
| 3 (T72c / T72 / T67) | T72 > T67 > **T72c** | "sticker/blob", "slightly **brown and rust**", fewest, shortest tips | T67 > T72c > T72 |

T72's words were brown, rust, mud, blob and wreath or rope in every round. T67's included "sticker", "brownish rim" and
"wreath feel" (round 2). No critic named a white core in any version.

## 7. Verdict on Athena's bar (the bolt)

| Part | Result |
|---|---|
| (a) separates at least as well as T67 | **Fails in round 3** (T72c ranked last); holds in rounds 1 and 2. The Weber number holds (-0.557 vs -0.394). |
| (b) reads as flame, no brown, rust or rope named | **Fails in all three rounds**: rust (r1, r2), brown and rust (r3). Rope was never named for T72c. |
| (c) closer to S1 than T72 | Holds in all three rounds. |

By the letter the bar failed after three rounds, so the fallback ran: `spell-flipbook.mjs`, the PNG, the manifest, both uassets
and the tests were restored to f557790 (`spell-flipbook.mjs --check` OK, 0 differing pixels against f557790's atlas, `git diff
f557790 -- apps/vr` shows only the card). The uassets are restored from f557790's bytes: a re-import is not byte-reproducible
(same size, different oid), so the committed ones were kept; the re-import log says `customdata=3`, `unchanged=6`, `failed=0`.

## 8. Gates

- `build.ps1`: `Result: Succeeded` (T72c state and revert state).
- `MageArena.` (corpus regenerated): T72c `Found 343`, `EXIT CODE: 0`; revert `Found 342`, `EXIT CODE: 0`, no reds.
- Lint (`unused-members`, `format-check`) OK; `check-pin`, `check-overlay`, `agy-guard`, `leonardo-redaction`, `check-repo-law` OK.
- `gitleaks detect` reports 1 leak, pre-existing (`Game/Config/DefaultEngine.ini`, commit `ceb2455`, 2026-10-02, a `SecurityToken`
  key); `--log-opts f557790..HEAD` finds none.
- `capture-spell-vfx.ps1 -Run T72c-before` and `-Run T72c-after`: 11 shots each, `MAGEVR_CAPTURE_DONE shots=11`.
- LFS: png and uassets are LFS pointers (`git lfs ls-files`).

## 9. Open points

- **The bar's word test is noisy.** T67 itself drew "brownish rim" and "sticker", and rust is hard to separate from the orange
  fill the bolt is made of. T72b's bolt got none of those words in three rounds; T72c's identical cells got rust twice and
  brown once. With one image set per round the critic had less to compare against (T72b showed 11 sets). The T72c cells are in
  c24e16c if Athena rules the bar met on the Weber number and part (c); the rule as written says no.
- The Weber gain on the bolt (-0.557) is real and measured; it is what a restore would buy.
- Option only (not a lane): the bead ring (cell 14) reads as rope even in T67's own cell, a shape problem of the bead ring.
