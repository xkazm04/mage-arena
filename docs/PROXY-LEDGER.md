# Proxy ledger: what the stand-in hands are, and what they have missed

Written 2026-10-22 at `aea4226`. Every hand-input figure in this repository so far comes from stand-in input. This
file says what produced each kind of stand-in, which figures carry that basis, what the stand-ins are known to miss,
and which claims wait for real hands. It changes no figure, threshold, test or gate verdict.

## 1. The basis vocabulary

A figure's basis is what produced its input, not the channel the input entered through.

| Basis | What produced it | Where it lives | Clip `source` value |
|---|---|---|---|
| **generated** | `tools/clipgen/generate.mjs` (templates, corpus, impostors, noise, L1 clips) | `Game/Clips` and `Game/TestData` (113 committed); `Game/TestClips` corpus, impostors and noise (regenerated, gitignored) | `synthetic` |
| **generated mouse-shaped** | `tools/clipgen/mousepaths.mjs`, which calls the same `buildVariedClip` as the corpus (`corpus.mjs`) | `Game/TestClips/mouse` (90 clips, regenerated, gitignored) | `mouse` |
| **modelled stream** | a generated clip resampled by `Tests/HeldSampleTests.cpp` (held or extrapolated, 30 or 25 Hz) | in memory, per test | n/a |
| **human mouse** | the owner's or a tester's live L2 draws through `UMouseSigilCapture` | none recorded yet | n/a |
| **recorded** | a device session written by the recorder in the clip schema | none yet (V1/V2) | `recorded` |

At `aea4226` all 113 committed clips carry `"source":"synthetic"`. The `mouse` value names the channel the clip
pretends to come from, not a person: those clips come from the same generator as the corpus, so the D-G2 "mouse"
figures are in-distribution (`docs/research/MOONSHOT-2026-10.md`, sigil-recognition card). No human-mouse and no
recorded figure exists anywhere in the tree.

**Rule for new figures:** write the basis in the same cell or sentence as the number, using the words above
("generated corpus 256/270", not "corpus 256/270" or "recorded corpus 256/270"). A generated or modelled figure may
unblock work and set a threshold. It is never reported as a result about hands.

## 2. Figures on record and their labels (inventory, 2026-10-22)

Hand-input figure statements were counted in `README.md`, `docs/PROJECT-PLAN.md`, `docs/CLIP-SCHEMA.md`,
`docs/research/FEASIBILITY-2026-10.md`, `docs/research/STATE-EVAL-2026-10-20.md`,
`docs/research/BASELINE-2026-10-07.md`, the three `docs/research/KPI-*.md` files and
`docs/submission/HAND-INTERACTION.md`: lines carrying a measured ratio or percentage about a hand rule (accuracy,
impostor, noise or false casts, perfects, onset, blink, re-arm, reach). Gate targets and pending rows are excluded.

| Count | Before this file | After this file |
|---|---|---|
| Figure statements | 31 | 31 (none changed) |
| Basis stated in the number's own cell or sentence | 0 (two FEASIBILITY lines say "model", which labels the stream, not the input) | 3 (the submission table) |
| Basis stated in the same row | 3 | 3 |
| Wording that names the wrong basis | 4 | 3, in two dated reports, listed below, not rewritten |
| Figure statements in `FEASIBILITY-2026-10.md` | 25, and the file never says its clips are generated | unchanged; this file is their label |

The four wrong-basis wordings:

1. `docs/submission/HAND-INTERACTION.md:80` "Sigil accuracy on the recorded corpus". Fixed here, because the file is
   the live draft of the submission form: it now reads "generated corpus".
2. `docs/research/STATE-EVAL-2026-10-20.md:48` "A recorded corpus figure is held accuracy 256/270". Dated report, left
   as written. Read "recorded" as "generated".
3. `docs/research/FEASIBILITY-2026-10.md:232` and `:235` "mouse 87/90". Dated report, left as written. Read "mouse" as
   "generated mouse-shaped".

One more sentence makes the same claim without a number: `docs/DESKTOP-INPUT.md` "recognizer accuracy on recorded or
mouse-drawn paths" is what the desktop build *can* prove, and neither path exists yet.

## 3. Miss ledger: where real hands contradicted a stand-in

| Date | Rule | What the stand-in said | What the recording showed | What the stand-in lacked |
|---|---|---|---|---|
| | | | | |

**Empty: the stand-ins have never been checked against real hands.** There are no recorded clips and no human-mouse
draws. Every figure in section 2 is unchecked in this sense. Rows are added the first time a recorded or human figure
disagrees with a generated one, and a row is never deleted when the generator is fixed: the ledger is what says which
kinds of claim a stand-in can support.

Not ledger rows, because no real hand was involved, but the misses the stand-ins are already known to hide
(review findings and model-against-model results, labelled as such):

| Found | Rule | Property the stand-in lacks | Source |
|---|---|---|---|
| 2026-10-03 review | ward onset walk-back | a hand that settles at the top for more than 80 ms | `apps/vr/tasks/T04-ward-detection.md` retry 1, item 2 |
| 2026-10-03 review | ward freshness | a hand that enters tracking already raised | same, item 3 |
| 2026-10-07 model | sigil impostor rejection | camera-rate overshoot at zigzag corners (9 and 30 false casts on modelled streams, fixed in 9f824a3) | `docs/research/FEASIBILITY-2026-10.md` G1 |
| 2026-10-07 model | blink re-arm | camera-update speed spikes (47 late re-arms on modelled streams, fixed) | `Tests/HeldSampleTests.cpp` `KnownModelMisses` comment |
| 2026-10-21 measure | style A pen, staff grip | `pinch` and thumb-index distance that disagree; the generator places the thumb from the pinch | `docs/CLIP-SCHEMA.md`, "The synthetic corpus cannot disagree with itself" |
| 2026-10-09 review | sigil accuracy, reject cut | any hand that did not come from the generator: templates and test draws share one generator | `docs/research/MOONSHOT-2026-10.md`, sigil-recognition card |

## 4. Claims that wait for real hands

A stand-in corpus is retired for a rule only when recorded clips (or, for D-G2, human mouse draws) cover that rule's
cases and section 3 has no open row for it. Until then the generated corpus stays, labelled, beside the real one.

| Rule | Stand-in figure on record | Waits for | Retire the stand-in when |
|---|---|---|---|
| Sigil top-1 (D-G2, V2) | generated corpus 256/270 to 258/270; generated mouse-shaped 87/90 | owner and second-person mouse draws (D-G2); V2 180-trace corpus | both exist and are reported beside the generated figure |
| Sigil impostor and noise rejection | generated 0/180, 0/5 (fixed seed bands, so the held-out seed does not vary them) | V2 recordings with real off-sigil motion | recorded false casts per minute are measured |
| `InnerTailRatio` 0.53, `MinInnerStrokeRatio` 0.28 | both bounds read from generated clips and modelled streams (`Gestures/SigilStrokeBuilder.h:52-60`) | V2 recorded sigils | the recorded reach distribution is inside both margins |
| Ward onset and perfect window (D-G3) | modelled streams, onset +13.9 to +41.7 ms, perfects 36/36 | V2 flash-method latency | recorded onset error is measured |
| Blink count and re-arm | modelled streams 27/27 | V2 recordings | recorded re-arm is measured |
| Style A pen and staff grip | generated clips agree by construction (0 of 8,995 frames apart) | V1 `captureTime` and V2 recordings | recorded frames where `pinch` and the tips disagree are counted |
