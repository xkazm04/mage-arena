# Sprite route: can the mechanics be wired to it? (S2 round 4, Tier B)

2026-10-29. Branch `push/chalk-hero`. Evidence: a seated Brennic duel replayed twice (seeds 1 and 2, the T38 reference
script, `-MageArenaRivalTrace`, 2,076 ticks, 276 position samples, 259 rival events), the code paths in
`Session/RivalFigure.cpp`, `Session/SessionPresentation.cpp`, `Kernel/MageAI.cpp`, the data in `data/vr/combat.vr.json`,
`data/pinned/.../arena-tiers.json` and the fire and air spell sheets, and the timings of every generation this push made
(`gen/**/*.json`).

## Verdict

The route covers Brennic's look well, but **today it covers four of the roughly eighteen states the duel asks of him, and
the duel asks for things a single front-facing flat card cannot give**. The replay shows:

- He **moves 81 % of the time**, about 4.5 m/s, strafing and closing. There is no locomotion clip, so the card slides.
- He fights at **3.5-8 m, median 6.9 m**, not at the 16 m of the S30 shot (16 m is his spawn). At 7 m a flat card is
  borderline in stereo, and under ~5 m it reads as cardboard (research §3: 0.5 m of body depth is ~1 px of relative
  disparity at 7 m, ~4 px at 3.5 m). It also magnifies the 256 px cells about 2x on a Quest 3 eye.
- His **facing stays on the seat** (median 6°, p90 18°, 5 % of samples over 30°). Yaw-only facing toward the seat point
  holds; no side views are needed for him.

Wiring is the easy part: everything needed is already on the kernel actor and its event log (below). The cost is the art:
about **560 cells at 256 px for Brennic alone (3 pages, ~28 MB ASTC 6x6)**, or ~9 pages if the cells go to 512 px for the
distances he actually fights at. Lio, the hounds, the maw and the soldiers have no figure on any route today.
**Feasible for Brennic as one showcase opponent with a locomotion set and a depth card. Not feasible for every opponent of
the day by 2026-11-18.**

## 1. Every state the game asks of an opponent

Who a seated player sees on the Tiro day (`arena-tiers.json` Tiro): wave 1, four conscripts and two slingers; wave 2, three
cinder hounds and a mire maw; wave 3, **Brennic** (Fire, competence 1, HP-gated phases at 66 % and 33 %, Sunfall opener);
wave 4, **Lio** (Air, competence 1.5, wards bolts, Air Form, Tempest opener). Before that, the teach and tutorial dummy.
`Session.Wave1Seated` and `Session.CreaturesSeated` (MageArena.Invariants) run waves 1 and 2.

How each is drawn today: only Brennic has a figure (`URivalFigureComponent`, four clips picked by `RivalClips::Step` from
`cast`, `hit` with HP removed, and `down`). Every other body is a greybox body (`ASessionPresentation::BodyFor`,
`PresentationLook::Body`). The hound and the maw have imported rigs with Idle/Attack/Hit/Death, but only the capture scene
uses them.

What the replay saw Brennic do (two duels): 123 `cast` events (102 tier 0 bolt, 18 tier 1, 1 tier 2, 2 tier 4 Sunfall),
80 hits on the player, 43 hits taken, 4 `phase` beats, 6 tier `unlock`s, 1 `down`. The player dodged him twice. He did not
ward or roll in these seeds, but the AI has both (`bAbsorb` in `MageAI.cpp`; `bRoll`), and `combat.vr.json` has Lio "ward
like the Fire mage".

| State (who) | Kernel signal | Clip today (3D rig) | Sprite today | How it would be generated |
|---|---|---|---|---|
| Idle (Brennic) | default | Idle | **yes** (H3 loop, 55 cells) | done; motion is subtle (see §4) |
| Cast bolt, tier 0 (Brennic) | `cast` value 0 (83 % of casts) | Cast (CastLunge) | **yes** (38) | done |
| Cast tier 1: Flick cone / Cinder Step dash / Kindle | `cast` value 1, spell id on the slot | Cast | no (plays the bolt cast) | Flux key per spell + H3 first/last; the dash shares the roll/step clip |
| Cast tier 2: Brand Lance / Ring of Cinders | `cast` value 2 | Cast | no | Flux key + H3 |
| Sunfall, tier 4, the phase-3 opener (Brennic) | `cast` value 4; 0.8 s cast | Cast | **proof made** (43) | Flux key (arms raised) + H3 to the lunge key |
| Ward / absorb (Brennic, Lio) | actor `bAbsorb` | none (no ward on the figure) | **proof made** (49: raise, hold, lower) | Flux key + H3; hold cells loop while `bAbsorb` |
| Roll / dodge (both mages) | actor `RollUntil`, `bRoll` | none | no | Flux key + H3 (16 cells); mirror for the other side |
| Strafe left/right (both mages; 81 % of the duel) | actor velocity across the seat line | none: the rig slides | **proof made** (21, right = mirrored card) | Flux step key + H3 cycle; the card moves, the clip steps in place |
| Approach / back off | velocity along the seat line | none | no | Flux keys + H3, 20 cells each |
| Hit (Brennic) | `hit` with HP removed | Hit | **yes** (20) | done; per element only as a tint (the event carries no element) |
| Heavy hit / stagger | none in the kernel today (hit value only) | Hit | no | optional: pick by damage value |
| Phase beat, 3 s (Brennic, Lio) | `phase` event; `phaseBeatS` 3 | Idle (text card only) | no | Flux key (roar) + H3, 36 cells |
| Tier unlock | `unlock` | none | no | a glow on the card, no clip |
| Death (Brennic) | `down` | Death, held | **yes** (77: buckle, kneel, topple, then a 2D tip-over onto the ground) | done; Flux would not paint a single lying figure, so the last beat is the topple frame rotated |
| Victory (player down) | player `down` | Idle | no | Flux key + H3, 36 cells |
| Taunt / story beats | `strings.json` voice lines; no animation hook | none | no | optional 30-cell gesture clip |
| Lio (Air final), all of the above | same signals | **greybox** | no | a full set of its own, ~550 cells; needs a concept first |
| Conscripts x4, slingers x2 (wave 1) | throw, walk, hit, down | **greybox** | no | per type: walk, throw, hit, die, ~8 facings because they cross the field (§2): 100-200 cells each |
| Cinder hounds x3, mire maw (wave 2) | spit, orbit, hit, death ember | **greybox** in the session; rigs only in captures | no | hounds orbit (`houndOrbitM`), so 8 facings x 4 states; the maw is static, 4 states |
| Training dummy (teach) | hit only | greybox | no | one still + hit flash |

## 2. Cost per clip on this box, page budget, and yaw-only facing

Measured on the shared GPU (Tier A's panorama jobs were queued in the same ComfyUI throughout, so these are upper-normal,
not best case):

| Step | Wall time | Notes |
|---|---|---|
| Rig render, Blender 5.2 EEVEE, 768² | ~0.5 s per frame (120 frames in 63 s) | only where a rig clip exists; new states have none |
| Flux 2 key (img2img, 768², 20 steps) | 150-670 s, median 285 s | 85-190 s with the GPU to itself |
| MiniMax H3 segment (22 frames, 768², 8 steps) | 110-490 s, median ~200 s | one segment per key-to-key move |
| LTX-2.5 v2v (49-65 frames) | 72-660 s, median 170 s | dropped for new states: blurred next to dense guides |
| BiRefNet matte (ComfyUI) | 56-600 s per clip | 2-9 s per frame, queue-bound |
| Chalk pass + pack (`pack_atlas.py`) | ~1.5 s per cell | the whole page's bleed fill adds ~10 min |
| Import + build (`-run=CreateRivalSprite`) | ~1 min | |

**Per new clip** (two keys and two H3 segments, a matte, a pack): about **20-35 min of GPU wall time** and ~5 min of CPU,
plus a human look. The judged quality needs 1-3 retries on the action clips (Death took four rounds), so plan
**~1-1.5 h per clip**. The ~14 missing Brennic clips are 2-3 days of generation alone, before Lio or anyone else.

**Page budget for Brennic's full set** (the table's Brennic rows at 24 fps, right-side moves as mirrored cards): ~550 cells.

| Cell size | Pages (4096²) | Memory, ASTC 6x6 + mips | Desktop BC3 + mips |
|---|---|---|---|
| 256 px (today) | 3 | ~28 MB | ~64 MB |
| 384 px | 6 | ~57 MB | ~128 MB |
| 512 px | 9 | ~85 MB | ~192 MB |

At his real distances (median 6.9 m) he is ~270 px tall on the game camera and ~440 px per Quest 3 eye, so 256 px cells
are magnified about 2x; 384-512 px cells are what that distance needs. The research report's ~70 MB budget for all
image-based art cannot hold 512 px cells for even one opponent. Animating on twos (12 fps) halves every figure.

**Yaw-only facing toward the seat point** holds for Brennic: his kernel facing is within 18° of the seat 90 % of the time
(max 96°, during strafes). Bearing from the seat spans about −66° to +95°, but the card turns with him, so the player still
sees his front. It **breaks** for: (a) strafes, where the front pose slides sideways without a step clip; (b) under ~5 m,
where flatness shows in stereo (the fix is a depth card: the card's grid displaced by a per-frame depth map, research §3);
(c) the hounds, whose orbit shows their sides, so they need 4-8 facings.

## 3. Proof clips (made this round)

The three highest-gain missing states, packed on a second page (`T_RivalSprite2`, same 16 x 16 grid, 256 px cells):

- **Strafe** (21 cells): one side step from the idle key (the left; the right is the card mirrored), for the 81 % of the
  duel he is moving.
- **Ward** (49 cells): raise, a 6-frame hold that loops while `bAbsorb` is on, and lower.
- **Sunfall** (43 cells): arms raised to the sky, then the release into the bolt lunge; the phase-3 opener.

Each is two Flux 2 keys (img2img from the idle key at denoise 0.9, the same seed and refs) and two MiniMax H3
first/last-frame segments, matted and chalked like page 1. Contact sheets: `frames/s2r4-sheet-Strafe.png`,
`s2r4-sheet-Ward.png`, `s2r4-sheet-Sunfall.png`. These clips are art only (`T_RivalSprite2.png`, commit `8052e59`): they are not
imported or wired into `RivalClips`. The ward and Sunfall read cleanly; the strafe step is small (one foot lifts), and the
Sunfall release snaps over two frames (the H3 pacing pitfall).

**Wiring** (not done; about half a day): extend `ERivalClip` with Strafe, Ward, Sunfall, Roll and Phase. `SyncRivalFigure`
already has the actor. Pick from actor state with a priority: Death > Hit > Ward (`bAbsorb`) > Roll (`RollUntil`) > cast by
tier (`cast` value) > Phase > Strafe (velocity across the seat line, mirrored by its sign) > Idle. Each clip's cells and
page live in a table. The kernel is untouched, and hits and targets stay on the actor.

## 4. Idle at 16 m in play

Measured on the 55 Idle cells against their mattes, and scaled to the screen:

| Where | Figure height | Head motion | Hand/arm motion |
|---|---|---|---|
| 16 m, game camera (1376 px, 90°) | 119 px | 0.2 px | 3.8 px |
| 16 m, Quest 3 eye (~25 PPD) | 195 px | 0.3 px | 6.3 px |
| 7 m (his median), Quest 3 eye | 444 px | 0.7 px | 14 px |

At 16 m only the arms and hands visibly move (4-6 px). The head turn and breathing are under 1 px, so he reads as standing
still with flexing hands; the true-scale strip is `frames/s2r4-idle-at-16m-gamescale.png` (and `-x4`). At the 7 m he
actually fights at, the arm motion is clear. More life at 16 m needs bigger moves: a weight shift and a cloak sway (the
rig's Idle has neither), or the strafe clip in place of Idle whenever he moves.

## Risks and gaps

- The 16 m figure in the S30 shot is not where the duel happens: the kernel brings him to 3.5-8 m. Any judging done only
  on the S30 frame overstates the sprite route.
- Identity holds across the H3 segments made so far, but every action clip needed 1-3 retries.
- Not measured: the depth card's cost; ASTC block artefacts on the chalk contour; how the card reads in a headset.
