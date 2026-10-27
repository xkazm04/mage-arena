# T75 - Bead ring

Status: ran, blind bar not met (beads, not rope, in 3 of 3 rounds; but the reward read only "weakly" in 9 of 9 cells and the
water beads read as water bolts in 2 of 3), ring restored to T67 (fallback) - report docs/research/BEAD-RING-T75-2026-10-27.md
Max turns: 120

## Goal
The perfect-absorb ring at the palm reads as a ring of BEADS, not rope. Critics read it as rope even in T67's own Fire
cell in all three T72b rounds (`docs/research/FIRE-VALUE-T72B-2026-10-26.md:199`; `FIRE-VALUE-T72C-2026-10-26.md:125`;
`docs/concepts/VISUAL-PLAN.md` (b), the "Option only" bead-ring note). This is owner ruling 4 of 2026-10-27
(`docs/DECISIONS.md`), the bead-ring half; the steel half was T74. A shape change only: every school keeps its colour
(`GlowColour`), and its cell where a round head exists.

## Where the rope comes from (read before any change)
- `SpellVfx::PerfectRingPuffs` (`Session/SpellVfx.cpp`) places 20 beads on a ring widening from 8 to 26 cm. Each is a
  streak card along the tangent, `Bead x 2.4 x RingCardScale` long (6 x 2.4 x 1.7 = 24.5 cm at age 0, Fire x 1.3 =
  31.8 cm) against 2.5 cm of arc per bead at radius 8 cm. Every card overlaps about ten neighbours, so the heads and tails
  merge into one twisted band: rope. The ring's cells are the splash's (`StreakCell`: Water and Earth the droplet streak,
  Fire the spark streak, Air the wisp).
- The streak cells (13, 14) are a round head at +U with a tail behind it. The head is a bead already; only the tail and the
  spacing make the rope. The wisp (10) has no head.
- The atlas is full (16 of 16 cells), so a new bead cell would replace another one. M_SpellGlow maps
  U = u x d0 + d1 from the card's custom data, so a card can show a part of a cell's width with no material change.
- `SplashPuffs` shares the streak cells and must not change.

## Deliverables (exact paths)
- `apps/vr/Game/Source/MageArenaVR/Session/SpellVfx.h` / `.cpp` - fewer beads, each a near-round bead (a streak cell's
  head, its tail cropped off), sized to a share of the chord between neighbours so a gap stays open through age 0..1.
- `apps/vr/Game/Source/MageArenaVR/Tests/SpellVfxTests.cpp` - `MageArena.SpellVfx.PerfectRingBeadsApart`, red first on
  today's ring: at age 0, 0.5 and 0.9 neighbouring bead cards do not overlap along the ring and the gap is at least a
  stated fraction of the bead length; the bead card shows a round head, unstretched.
- Flipbook re-render (`apps/vr/tools/vfx/`, `apps/vr/art/vfx/`, `apps/vr/Game/Content/Vfx/`) ONLY if no existing cell can
  serve as a round bead, and then no other cell changes (atlas diff).
- `docs/research/BEAD-RING-T75-2026-10-27.md` - desktop proxy, not Quest truth.
- `docs/concepts/VISUAL-PLAN.md` (b) - one (14) T75 line. This card's status.

## Constraints
- Do not push; touch no other repository. Do not edit `docs/DECISIONS.md`. No combat number, `apps/vr/data/PINNED.json`,
  `apps/vr/data/pinned/`, `apps/vr/data/conformance/` or clip corpus edit. No Leonardo calls.
- `SplashPuffs` does not change. Every existing `MageArena.SpellVfx.*` and `MageArena.Greybox.*NotThreat` test stays green
  with its margin intact; the T72b "spark streak is T67's cell exactly" test changes only on a re-render, with the reason.
- At most one Unreal process machine-wide. Never commit `Binaries/`, `Intermediate/`, `Saved/`, `DerivedDataCache/` or
  `runs/`. Evidence in `C:/Users/kazda/mage-art-staging/bead-ring-t75-2026-10-27/` only.

## Acceptance (quote the real output in the report)
```
build.ps1                                                     # Result: Succeeded
MageArena. automation group (corpus regenerated)              # Found N >= 345, no new reds beyond the DF-011 two
capture-budget.ps1 -Runs 3                                    # before and after, stress scene
capture-budget.ps1 -Runs 3 -ExtraArgs -MageArenaCreatureImportScene   # before and after, creature-import scene
                                                              # draw calls do not rise, <= 150; BasePass <= 350k
capture-spell-vfx.ps1 -Run T75-before / -Run T75-after        # 11 shots each
```
Report the gap / bead ratio at age 0, 0.5 and 0.9 for every school.

**Blind bar (ruling 4, its own words):** shuffled before/after pairs of the perfect ring for every school (Fire first, the
cell the critics named) at the capture's ring distance, plus one 8 m frame, a fresh critic per round, 3 rounds, seeds and
keys kept. Pass when in all 3 rounds the after ring reads as beads, not rope, still reads as the perfect-absorb reward in
its school's colour, and is never read as a threat (element bolt, steel, black core). S1 is shown beside the pairs and
"closer to S1" is recorded as a column, but it does not gate this lane (the S1 question for greybox lanes is with the owner).
**If the bar fails:** revert the ring to today's code byte for byte in its own commit, re-run the build, the suite and
D-G4, and report; the T75 commits stay on history either way.

## Report
`docs/research/BEAD-RING-T75-2026-10-27.md`; the evidence stays in the staging folder above.
