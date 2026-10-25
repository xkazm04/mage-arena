# Owner decisions

Newest first. Quotes are the owner's words; the lines under them are what the plan does about it.

## 2026-10-25 - Athena, on c69ef865 (T61 braziers): the flame colour ACCEPTED, the near positions NOT

> Athena, 2026-10-25 08:06Z (owner delegation), on c69ef865 (`docs/research/BRAZIERS-T61-2026-10-25.md`). The ruling is
> recorded as the dispatch brief relayed it, not verbatim.

- **The flame colour is ACCEPTED.** It reads only weakly as fire; that is accepted for now. Spectacle belongs to the later
  flipbook pass (DECISIONS 2026-10-25 on b872dcd). The colour constants in `Greybox/BrazierFire.{h,cpp}` do not change.
- **The near-brazier positions (-3.5, +/-7.8) are NOT accepted.** From the centre seat no brazier may stand behind any
  demo threat lane (the fire bolt, the steel line, the unblockable) in `04-threats-midflight` and
  `10-unblockable-telegraph`. Prefer the wall or behind the seat; keep the pair mirrored; inside the oval, clear of the
  banners, the crowd and the dais. The clearance test widens to every demo threat lane.
- **One Unreal lane, in this order:** the brazier positions, the T28 part-2 proof, one overlay golden re-record (hash
  lines only), then D-G4 and a blind critic. The card is T62 (`apps/vr/tasks/T62-brazier-lanes-t28-proof-golden.md`).
- No signed number moves.

## 2026-10-25 - Athena, on deaa15b (T60 braziers): the bowls ACCEPTED, the flame NOT; option B

> Athena, 2026-10-25 (owner delegation), on deaa15b (`docs/research/BRAZIERS-T60-2026-10-25.md`, section 6). The
> ruling is recorded as the dispatch brief relayed it, near-verbatim.

- **The T60 bowls and the colour-safety work are ACCEPTED.** The `projectiles.fire` override is gone and
  `Greybox.BrazierFlameNotThreat` holds it.
- **The flame is NOT accepted as fire.** It reads as smoke or a khaki shrub.
- **She WITHDRAWS her own "dimmer than any threat glow" clause:** it cannot produce a flame against the lit wall.
- **Option B.** The heart becomes a pale, desaturated warm white or yellow, with a luminance above the rim's (0.169
  greybox) and below Fire's and Earth's (0.38 / 0.31 greybox). It stays held off every threat class by chroma, with the
  same `BrazierFlameNotThreat` test, and the shape stays a slow, broad lick. Hue and shape separate it from the
  vermilion bolt, not darkness. A blind critic must read (a) fire and (b) no threat confusion.
- **Move the near-right brazier at (-2, -7.2)** off the demo unblockable's flight line from the centre seat, as a
  layout-only change.
- **Remove the stale `colours.flame` and the cone note** from `arena-layout.json`.
- **One Unreal lane, and a D-G4 re-check.** The card is T61 (`apps/vr/tasks/T61-brazier-flame-reads-as-fire.md`).
- The DINOv3 hold stands. No signed number moves.

## 2026-10-25 - Athena, on 7fd7d11 (T57 crowd and stands): ACCEPTED; the braziers next

> Athena, 2026-10-25 (owner delegation), on 7fd7d11 (`docs/research/CROWD-T57-2026-10-25.md`). The ruling is recorded
> as the dispatch brief relayed it, not verbatim.

- **T57 is ACCEPTED.** The stands read as an audience.
- **The D-G4 max stays 126 of 150.**
- **The crowd tint pass is DEFERRED,** not done now: warm, varied tunics and the red stand banners, on the atlas or the
  per-instance custom data, kept muted.
- **NEXT: the braziers as fire props** (T60, `apps/vr/tasks/T60-brazier-fire-props.md`). The greybox flame takes the
  style's `projectiles.fire`, the fire spell's threat colour; the brazier flame gets its own environment colour, out of
  every threat class.
- The DINOv3 hold stands. No signed number moves.

## 2026-10-25 - Athena, on b872dcd (T58 spell VFX): ACCEPTED as a readability step; T57 crowd next

> Athena, 2026-10-25 (owner delegation), on b872dcd (`docs/research/SPELL-VFX-T58-2026-10-25.md`). The ruling is
> recorded as the dispatch brief relayed it, not verbatim.

- **T58 is ACCEPTED as a readability step,** at +2 draw calls.
- **Its best fix is the leave cue:** the black core with the red rim draws again.
- **The spectacle pass is DEFERRED to a later card** and is not done now: a flipbook flame and water texture on the
  shared glow layer, at the same call cost.
- **The order: T57 crowd and stands, then the braziers as fire props.** In key art S1 the packed stands are half the
  image; ours are empty blocks. The crowd goes in as instanced cards or impostors, at one or two draw calls, in the dusk
  light, and changes no gameplay read. It re-checks the budget (about 23 calls of headroom).
- The DINOv3 hold stands. No signed number moves.

## 2026-10-25 - Athena, on 570e063 (cloak normal, Blender half): the Unreal half DEFERRED, the arena next

> Athena, 2026-10-25 (owner delegation), on 570e063 and the Blender half
> (`docs/research/BRENNIC-CLOAK-NORMAL-2026-10-25.md`). The ruling is recorded as the dispatch brief relayed it, not
> verbatim.

- **The relief is real, but it reads SAME at the 8 m duel ring.** It is a VISIBLE GAIN only close up.
- **The Unreal half is DEFERRED.** A 2048 normal map per character costs Quest texture memory for a gain the player
  does not see at duel distance.
- **`finish --normal` stays as a tool** for hero close-ups and for characters met near (the mentor, the school
  teacher). Revisit if a close-range moment appears.
- **The visual gap is now the arena,** in order of visible gain per Unreal hour: (1) T58 spell VFX, (2) T57 crowd and
  stands, (3) the braziers as fire props. All of it fits inside the roughly 26 draw calls of headroom (124 of 150),
  with a D-G4 re-check.
- The DINOv3 hold stands. No signed number moves.

## 2026-10-25 - Athena, on a02aa89 (draw-call budget): ACCEPTED

> Athena, 2026-10-25 (owner delegation), on a02aa89 (`docs/research/DRAWCALL-BUDGET-2026-10-25.md`). The ruling is
> recorded as the dispatch brief relayed it, not verbatim.

- **a02aa89 is ACCEPTED.** The creature cast's cost fell from +25 to +12 draw calls, and a blind critic read the stills
  as SAME.
- **The honest skeletal count is a real gain.**
- **LOD0 stays at the 8 m ring.** The LOD1 quality loss (critic MINOR) is not traded for BasePass, which sits at 191k of
  350k and does not bind.
- **Shadow-off stays rejected** (grounding).
- **The planning number:** about 26 draw calls of headroom at the max (124 of 150). At about 4 calls per creature after
  the cuts, that is roughly 5-6 more single-section bodies before the next budget pass.
- **Next: Brennic's back-cloak normal pass, Blender first, then Unreal, never both at the same time.** The Blender half
  is `docs/research/BRENNIC-CLOAK-NORMAL-2026-10-25.md`.
- Nothing ships: the DINOv3 hold stands. No signed number or pinned band changes.

## 2026-10-25 - Athena: ce1d3ca ACCEPTED; the draw-call budget before any further asset

> Athena, 2026-10-25 (shortly after 00:00Z, owner delegation), on ce1d3ca
> (`docs/research/CREATURE-CAST-UE-2026-10-24.md`: the maw beside Brennic and the hound, the soft crack bloom, the cast at
> 134 / 137 of 150 draw calls). The ruling is recorded as the dispatch brief relayed it, not verbatim.

- **ce1d3ca is ACCEPTED.**
- **The hound's soft crack bloom carries at 8 m** (`SoftIntensity` 1.2; the cracks stay orange).
- **The maw's seam ledge is left until a re-mesh is worth it.** The colour step is fixed; the geometric ledge at the cut
  stays for now.
- **The order changes: the draw-call budget comes before any further asset.** Three creatures cost +25 draw calls
  (D-G4 max 137 of 150), and the wave and crowd plans need more bodies. This is plan milestone 4 (desktop count budgets
  met), and it gates every later creature.
- **Brennic's back-cloak normal pass comes after the budget card.**
- Run as `docs/research/DRAWCALL-BUDGET-2026-10-25.md`. Nothing ships: the DINOv3 hold stands. No signed number or
  pinned band changes.

**The creature budget policy the card settled** (the defaults of `CreatureImport::FCreatureBudget`, applied when a
creature spawns, each with a command-line switch to measure it off). Athena may overrule any line.

- **Before:** every mesh section costs a draw in the base pass, the depth prepass, velocity and both shadow cascades.
- **After:** the three creatures add +12 / +11 draw calls over the no-creature baseline instead of +24 / +23 (D-G4 121 /
  124 of 150), and a blind critic read the stills as SAME.
- **Sections: one per creature.** The slots fold into the largest, in UE. The maw's tongue samples a body texel of its
  colour. Saves 5 calls per two-slot creature.
- **Velocity: no per-bone velocity** (`bPerBoneMotionBlur` off). A creature standing at its spot draws no velocity
  pass. The project runs MSAA (no TAA or TSR), and Quest draws velocity only for TAA or space warp. Saves 1 call per
  section. Revisit if space warp is turned on.
- **Shadows: stay in the two cascades at the duel distance.**
  - Off would save 2 calls per section, but a blind critic read it as a visible loss of grounding at 8 m.
  - Inset (per-object) shadows cost more calls on desktop and draw none on Quest by default. Not used.
  - A distance cutoff exists as a lever, default off; the far ring was not shot.
- **LODs: LOD1 (50 % of the triangles) from 12 m and LOD2 (25 %) from 20 m**, for the seated Quest 3 eye: the near ring
  (8 m) keeps LOD0, the far ring (15 m) draws LOD1.
  - LOD1 at 8 m would cut 27k BasePass triangles, at a MINOR critic read. It stays an open question, not a default.
- **Headroom:** a further creature costs 4 calls at the near ring. A fourth lands at about 128 and a fifth at about 132
  of 150.

## 2026-10-24 - Athena 20:46Z: the maw ACCEPTED as a working asset; split-and-merge is the path; the seam fix ordered

> Athena, 2026-10-24 20:46Z (owner delegation), on 6fc79bf (`docs/research/MAW-MV-2026-10-24.md`: the two-view
> TRELLIS.2 run failed, the split-and-merge fallback won). On the seam: "the front half reads greener and the back half
> browner on both flanks". The rest of the ruling is recorded below as the dispatch brief relayed it, not verbatim.

- **The mire maw (the merged s7 front + s7301 back, 17,490 tris, s7 fit rig) is ACCEPTED as a working asset** (working,
  not final), as of 6fc79bf.
- **Two-view TRELLIS.2 is retired on this checkpoint.** The installed checkpoint has no `proj_linear` weights, so
  `Pixal3DMultiViewConditioning` only averages the two views' tokens, and the maw run returned 2,994 specks. The
  `--back` path stays in the tools but is not run again on this checkpoint.
- **Split-and-merge is the path** for a creature whose single-view back is torn (`merge_halves.py`, mesh README).
- **The flank seam fix is ordered**, in Blender, with no re-mesh: match the back half's albedo to the front half's
  across the cut, then feather across it. The palette, dome and leg defects are the source's and are not chased.
- **Then ONE Unreal lane:** import the maw beside Brennic and the hound, tune the hound's glow at 8 m (without
  touching the tonemapper or the exposure), capture the whole cast, and re-check the D-G4 budget. Blender is closed
  before Unreal starts (the RAM order of 19:11Z).
- Run as `docs/research/CREATURE-CAST-UE-2026-10-24.md`.
- Nothing ships: the DINOv3 hold stands. No signed number or pinned band changes.

## 2026-10-24 - Athena 20:13Z: Brennic and the cinder hound ACCEPTED as working in-engine assets

> Athena, 2026-10-24 20:13Z (owner delegation), on bc630e0 (`docs/research/CREATURE-UE-IMPORT-2026-10-24.md`: Brennic and
> the cinder hound imported into UE under the T56 dusk).

- **bc630e0 is ACCEPTED as working in-engine assets** (working, not final).
  - **Brennic reads as a person at duel distance.**
  - **The hound's ember cracks read at 5 m, and weakly at 8 m.**
- **Budget:** 124 of 150 draw calls, 173k of 350k BasePass.
- **Two notes for later lanes** (not now):
  - **Hound glow at 8 m:** raise it through `EmberIntensity`, a rim, or a soft crack bloom.
  - **Brennic's flat olive back cloak:** the normal-map pass.
- **Next:** the maw two-view run, then the maw UE import in a separate lane, **never at the same time** (the RAM order of
  19:11Z: Unreal first, then TRELLIS and Blender). The two-view run is `docs/research/MAW-MV-2026-10-24.md`.
- Nothing ships: the DINOv3 hold stands. No signed number or pinned band changes.

## 2026-10-24 - Athena 19:11Z: the maw needs a back view; the multi-view ban is lifted for it

> Athena, 2026-10-24 19:11Z (owner delegation), on `docs/research/CREATURE-MESH-2026-10-24.md`.

- **Cinder hound: a working asset, not a final one.** Its glow comes from an Unreal emissive material, not from the mesh or
  its texture (the source image has no glow to bake).
- **Mire maw: the front is ACCEPTED, the back is REJECTED.** The unseen back of the TRELLIS mesh is a torn shell that
  finishing closes with a fan, so **one front image is not enough for the maw**.
- **The multi-view ban is lifted for the maw two-view path only** (it was "no front/back pairs for creatures yet", 17:04Z).
  The maw gets a Leonardo back view drawn from the accepted master s7202 and a two-view TRELLIS.2 run. Built and tested in
  `docs/research/MAW-BACK-2026-10-24.md`; the run itself is not done in that lane.
- **RAM order: Unreal first, then TRELLIS and Blender.** Nothing from the TRELLIS/Blender group starts while an Unreal
  process is up.
- Nothing ships: the DINOv3 hold stands.

## 2026-10-24 - Visual approvals: Leonardo for new inputs, key-art creature canon, 18-30k soldiers, style delegated to Athena

> Owner, 2026-10-24 evening, on `docs/concepts/VISUAL-PLAN.md` section (g). Chosen options, verbatim:
> answer 3 (provider) "Leonardo (Recommended)"; answer 4 (creature canon) "Key-art look (Recommended)";
> answer 5 (soldier triangles) "18-30k humanoid band". Answers 1 and 2 (style block, reference sheet) were delegated by
> the owner to Athena. Answer 6 (presentation gap) is the owner's direction to write it as cards.

- **Answer 3, provider: "Leonardo (Recommended)".** New reconstruction inputs come from Leonardo, covered by the
  2026-10-21 originality ruling. agy calls are for a non-shipping control arm only. **Leonardo cap today: at most 30
  images**, every call logged with provenance (ASSET-LEDGER and the provider ledger), and the wave **stops at the first
  quota error**.
- **Answer 4, creature canon: "Key-art look (Recommended)".** The cinder hound is dark-red and lava-cracked; the mire maw
  is a brown, toothed toad. Their identity rows on the reference sheet are **replaced** by new Leonardo masters in that
  look (about 4 calls); the earlier grey-hound and moss-frog concepts no longer stand as canon.
- **Answer 5, soldier triangles: "18-30k humanoid band".** Conscript and Slinger use the same 18-30k humanoid band as every
  other humanoid. The 8,000 (12,000 ceiling) soldier request is **superseded**.
- **Answers 1 and 2, delegated by the owner to Athena.** Athena **approves** the VISUAL-PLAN section (c) style block as
  written (it matches the key art: stylised finished-game-model rendering, warm golden-hour dusk, crimson, gold and bronze
  structure, element colours reserved for magic, no letters, halftone or chalk on 3D); **key art S1 as the north-star
  frame**; **style sheet S1-S5**; and the identity masters **Brennic, Conscript, Slinger and Warden** as listed. Hound and
  maw masters are replaced per answer 4. Recorded as "approved 2026-10-24 by Athena under owner delegation".
- **Answer 6, presentation gap.** Lighting, crowd visibility, spell VFX, debug text over the view and the hand mesh become
  cards (T55-T59, `apps/vr/tasks/`). **The debug overlay text (for example "Last: cast (Actor 1)") must not appear in any
  captured or shipped view.**
- **Next:** Brennic multi-view master reference (front, side, back, canonical A-pose, plain background) from Leonardo within
  the cap, then the input gate, then local TRELLIS.2 once its setup lane merges, then the mesh card, finishing and rig.
- **Creature rulings, Athena 2026-10-24 17:04Z (owner delegation: "Athena judges it"),** on `docs/research/CREATURE-MASTERS-2026-10-24.md`:
  - **Cinder hound s7101 is ACCEPTED as the identity master.** Still-08's leaner, more feral look belongs to the pose and
    the clips, not to this master. Nothing more is generated for the hound.
  - **Mire maw s7201 is NOT accepted: the mouth is closed.** The huge open gape is the maw's identity in still-08, and the
    rig path has no shape keys, so a closed mouth leaves the jaw bone opening onto nothing. **Maw rework lane:** at most 2
    Leonardo calls (a second only if the first fails the script's gate), same prompt shape, mouth wide open showing the throat
    and teeth, same front three-quarter pose on a plain background (`creature-masters.mjs --variant open-gape`). Seed 7202
    ran (1 call, 18 credits, gate pass) and is a **candidate**; whether it is accepted is Athena's call, not recorded here.
  - **No front/back pairs for creatures yet.** The Brennic mesh lane first proves whether one image is enough for TRELLIS.
- **Athena, 17:19Z on bf4a34e:** the hound and maw meshes wait for the texture-preserving finish and are **not** built on
  the flat finish (the Brennic mesh ran finish-only; the critic said rework).
- **Athena, 17:24Z (owner delegation), on the maw rework candidate:** "mire maw s7202 ACCEPTED as the maw identity
  master, replacing s7201 (closed mouth); matches still-08: open gape, dark throat, tongue, cream teeth on both jaws,
  brown warted toad body; throat-capping risk noted for the mesh lane". The maw master is
  `maw-open-gape-s7202-medium.jpg` (sha256 `12a569aa...e6a4`, `docs/research/CREATURE-MASTERS-2026-10-24.md`); s7201
  stays on record as rejected.
- **Athena, 17:56Z (owner delegation), on bf7262a (the Brennic rework):**
  - **The bake finish is ADOPTED as the pipeline default for characters and creatures.** `finish.mjs` defaults to
    `--mode bake`; the flat mode stays an option (`--mode flat` builds exactly the pre-bake argument list).
  - **Brennic LOD0 is a working asset** (seed 7, bake finish), not a final one.
  - **Her critic reading of the Brennic face is PARTIAL**, not the fresh critic's "no".
  - **Planned order:** the hound s7101 and maw s7202 meshes on the bake finish (run 2026-10-24,
    `docs/research/CREATURE-MESH-2026-10-24.md`), then the UE import of Brennic once the Unreal slot frees, then a later
    normal-map detail pass.
- **What is therefore NOT touched:** the pinned combat data (no combat number changes), the threat colour language and the
  Quest budgets of `docs/PROJECT-PLAN.md` section 7. The seven rulings of this afternoon below are not repeated here.

## 2026-10-24 - P4 style: no single style is locked; the direction is visual quality, chasing the art concept

> Owner, 2026-10-24 afternoon, on the P4 style pick (13, 25 or 30 from `apps/vr/art/style-pick/`): "You can combine the
> style, arenas and pallettes, you can pick all 4. But the visual side is in tragic state. If it helps pick Greybox and
> focus on chasing the asset and graphical quality drawn in art concept."

- **No single style is locked.** Styles 13, 25 and 30 are a **pool** of palettes and arena treatments to combine freely,
  not a pick. Greybox stays the **base** the game is built on.
- **The direction is visual quality: chase the art concept.** The target is the assets and graphics the concept already
  shows (`apps/vr/art/index.html`: the A01-A09 studies, the model and kit sheets, the key art, the character concepts).
  `docs/concepts/VISUAL-PLAN.md` turns that into a measured target: an inventory of the concept, the observed gap, a
  draft style block and reference sheet for the owner's approval, and a gated per-asset pipeline.
- **The P4 OWNER item is answered by this ruling** (WOLF-PLAN P4, "picks 13, 25 or 30 from the page"). T38's "The owner
  decision" (greybox look until the pick) is answered the same way: greybox base, concept as the quality target.
- **What is therefore NOT touched:** the threat colour language (`CLAUDE.md`: element colour = absorb, steel = dodge,
  black core with a red rim = leave). Every combined palette keeps it. The style JSONs under `apps/vr/data/vr/styles/`
  stay as they are until a card changes them.
- WOLF-PLAN row marks follow in the next WOLF-PLAN change (the file is held by T50 today).

## 2026-10-24 - P5/P9 deferred: no headset build at this stage

> Owner, 2026-10-24 afternoon, on P5 (first APK and the Quest 3 simulator boot): "No need to resolve headset build in this
> stage"

- **P5 and P9 are deferred.** No APK, no simulator boot proof and no device bring-up (V1-V3) are worked on now.
- **B3 (Android target support in the Epic launcher) is not needed now.** It stays an open OWNER item, parked, not asked.
- **What is therefore NOT touched:** the Quest budgets in `docs/PROJECT-PLAN.md` section 7 (<=150 draw calls, <=350k
  visible triangles, <=2,000 particles). The desktop build keeps being measured against them, because every asset made
  now must still fit the headset later.
- WOLF-PLAN row marks follow in the next WOLF-PLAN change (the file is held by T50 today).

## 2026-10-24 - Models are built on this machine, along the ai-registry golden paths

> Owner, 2026-10-24 afternoon, on where the 3D models come from: "Creating models is why we are on this device with all
> tooling and resources, consult with ai-registry the right direction"

- **This machine builds the character and prop models** (RTX 4090 24 GB, CUDA 12.4, Blender 4.2 at
  `C:/Program Files/Blender Foundation/Blender 4.2`, UE 5.8.3). No local image-to-3D model is installed yet. Open-weight
  local tools may be installed in a dedicated folder outside every repo. TRELLIS.2 is the one the 2026-10-21 asset
  ruling names; any other tool needs its licence read and recorded before an asset made with it can ship.
- **The route is the ai-registry golden paths**, resolved through
  `C:/Users/kazda/kiro/ai-registry/knowledge/<domain>/index.json`: visual-style-locking (a style block and an approved
  reference sheet first), image-to-3d-input-gating, generated-mesh-acceptance, mesh-finishing-for-engine-readiness,
  generated-asset-world-scale, asset-class-poly-budgeting, animatable-character-design, character-identity-continuity
  and production-pipeline-phasing (downstream cost is never spent on an upstream defect). `docs/concepts/VISUAL-PLAN.md`
  is the plan that applies them, with the resolved file of each.
- **Order:** Brennic first (T38: rig and the four clips Idle, Cast, Hit, Death), then the other rivals, then arena props.
- **Spend:** paid image calls stay inside the day's caps and their ledgers. **Every shipped asset needs an ASSET-LEDGER
  row** (`docs/submission/ASSET-LEDGER.md`, the 2026-10-21 ruling); an asset with no row does not ship.
- **What is therefore NOT touched:** the T38 Wolf pre-import checks and the D-G4 triangle row (<=350k). The Lynx-made
  exports under `apps/vr/art/characters/*/export/` stay as the current baseline until a rebuilt asset passes every gate.
- WOLF-PLAN row marks follow in the next WOLF-PLAN change (the file is held by T50 today).

## 2026-10-24 - T41 cold-tester sessions deferred; the protocol stays

> Owner, 2026-10-24 afternoon, on T41 (two cold desktop testers): "No need testers for the current state we have now"

- **T41 sessions are deferred.** No tester is recruited and no `PLAYTEST-*` record is written now.
- **The protocol stays** (`docs/research/PLAYTEST-PROTOCOL.md`, `apps/vr/tasks/T41-cold-desktop-playtests.md`), ready
  for when the visual state is worth testing. K3 row 12 stays "not met"; no session is invented.
- WOLF-PLAN row marks follow in the next WOLF-PLAN change (the file is held by T50 today).

## 2026-10-24 - C2 rulings D1-D4 approved as recommended

> Owner, 2026-10-24 afternoon, on `docs/campaign/05-c2-slice-story-bible-and-script.md` section 11: "Approve the story
> script rulings"

- **D1-D4 are approved as recommended:** D1 the script is the voice of the game; D2 the 213 s story time budget of
  section 4 (54 s floor); D3 three intermission choices, not one; D4 voice approved at the recommended 14 cues, blocks
  A + B. The C2 document records each as approved, citing this section.
- **T28 (intermission scenes) may proceed on its non-audio parts.** D1-D3 were what it waited on.
- **D4's audio production is deferred** by the audio ruling below: the voice is approved as content, but no cue is
  generated now. T27 can build its importer and wiring against the manifest only.
- **What is therefore NOT touched:** D5-D7 (not part of this ruling; they never blocked work), every combat number and
  pinned band (C2 section 12), and DF-010's condition for restating the day's length.
- WOLF-PLAN row marks follow in the next WOLF-PLAN change (the file is held by T50 today).

## 2026-10-24 - Music and audio deferred; the visual side comes first

> Owner, 2026-10-24 afternoon, on the audio cards: "Delay music and audio until to focus on visual side"

- **Deferred:** T40 (the two music loops), T27's voice and chorus generation, and the PC audio branch `935a3a33`.
- **No audio spend.** No ElevenLabs call is made for music, voice or chorus while this ruling stands.
- **What is therefore NOT touched:** the 20 SFX already imported (A05) and the ElevenLabs approval and ledger conditions
  of 2026-10-21. D4 (above) stays approved; only its production waits.
- WOLF-PLAN row marks follow in the next WOLF-PLAN change (the file is held by T50 today).

## 2026-10-24 - T49 colour sign-off deferred until the visual direction settles

> Owner, 2026-10-24 afternoon, on the T49 HP-bar colours: "Defer healthbar colors after we resolve the visual side of
> game, they will relate to overall theme"

- **T49's colour sign-off is deferred** until the visual direction (the first ruling above, `docs/concepts/VISUAL-PLAN.md`)
  settles. The builder work on Wolf (branch `autopilot/accepted-idea-delivery-ec553e8a`) stays as evidence; the card is
  not closed and no WOLF-PLAN box is ticked.
- **What is therefore NOT touched:** the 2026-10-21 ruling that the near-black-with-red HP bar violates the threat
  language. That still stands; only the choice of the replacement colours waits for the theme.
- WOLF-PLAN row marks follow in the next WOLF-PLAN change (the file is held by T50 today).

## 2026-10-24 - D-G4 triangles: the BasePass reading is the verdict

> Owner, 2026-10-24 day standup, asked which triangle reading judges the D-G4 visible-triangle row
> (`docs/research/BUDGET-DG4-2026-10.md` 13.4): "BasePass".

- **The row is judged on the GPU-culled BasePass reading** (each opaque triangle once, as 8.5 judged it). On the
  shipping presentation after the bead fix (`57b5e52`) it reads max **139,280**, which **passes** the 350k budget.
- **The all-pass figure stays a logged number, not a gate.** Captures keep reporting it (max 376,518 at `57b5e52`) so a
  later rise is visible, but it cannot fail the row.
- **What is therefore NOT touched:** the 350k budget, the other D-G4 rows (draw calls <=150, live particles <=2,000,
  GPU emitters <=1), and section 4's caveat that V1 measures the device number.

## 2026-10-24 - DF-011 ruled: accept the three reds; P2 counts as landed

> Owner, 2026-10-24 day standup, on `docs/design-findings/DF-011-after-t39-brennic-still-wins.md`: "Accept the reds".

- **DF-011 is ruled and closed, option A (accept and measure).** `MageArenaDesign.Duel.FireSeated`, `Duel.AirSeated`
  and `Session.FullSeated` (it loses bout 3 to Brennic) stay red as **known reds**, explained by this accepted DF. T39
  stays as built (`5b13463` ... `6f00555`).
- **P2 counts as landed with them accepted.** T39's Done-when ("each of the five known reds green or explained by an
  accepted DF") is met by this ruling. That unblocks T47 (WOLF-PLAN P12, between P2 and P3).
- **What is therefore NOT touched:** the census targets and pinned bout bands (`docs/DECISIONS.md` 2026-10-07, "DF-003
  answered"), the overlay as T39 built it, and DF-011's options B, C and D, none of which is adopted. A later card that
  moves one of the three reds cites a new ruling.

## 2026-10-21 - DF-010 ruled: the day-length assertion leaves the gate, and the day is lengthened with content (E + B)

> Owner, 2026-10-21, on `docs/design-findings/DF-010-tiro-day-length.md`, asked which of five options: "E + B - buy the
> longer day with scenes".

- **DF-010 is ruled and closed.** Options A, C, D and "keep the gate" are rejected. The 7-10 min day stands as the
  target; the plan text is **not** restated down to the measured 3 min.
- **E binds now.** The day-length assertion (`SessionTests.cpp:792`, bounds `PlanLoS`/`PlanHiS` at `:671-672`) leaves
  `bPass` and becomes a logged measurement, `INSIDE`/`OUTSIDE`, in the `FullSeated` report. This is the owner saying it
  out loud, which is what `.ai/memory/0003-design-tests-red-by-design.md` requires: no worker could do it.
  `apps/vr/tasks/T47-split-design-gates.md` carries the edit, and its "blocked on DF-010" condition is now satisfied.
  The gate becomes **strictly stronger**: the four real invariants beside it (`SessionTests.cpp:788-791` - no infinite
  loop, the script wins every bout, the day closes, four bouts won) then fail on their own instead of hiding behind a
  number that cannot go green.
- **B is the route to the number.** The minutes come from content the plan already cards and the measurement prices at
  zero: the prologue tableau (<= 60 s, `docs/campaign/04-vr-campaign-design.md:26`), voiced ritual, taunts and aftermath
  (`apps/vr/tasks/T27-voice-and-chorus.md`), the intermission scenes with their one optional choice
  (`apps/vr/tasks/T28-intermission-scenes.md`), and a victory card. This is the only option that raises the day without
  reopening a signed number.
- **What is therefore NOT touched:** the pinned bout bands and win rates (`docs/DECISIONS.md:308-309`), the pinned
  pacing windows, and DF-006's and DF-008's adopted options. The fighting stays capped at 2.4-3.9 min
  (`docs/campaign/04-vr-campaign-design.md:137`) by the owner's own signature; the day grows around it, not through it.
- **C2 is now the critical path, and it is the owner's and Claude's to write.** T27 and T28 are both blocked on it
  (the story bible and slice script with the cue sheet; `docs/concepts/WOLF-PLAN.md` H3, roadmap lane B row 12), it does
  not exist in the repo as of this ruling, and no Wolf time on either card can start before it lands. Choosing B means
  choosing that writing time, ahead of the 18 Nov deadline.
- **When the plan's number may be restated:** only once T27/T28 have landed **and** a day has been measured that wins
  all four real bouts. No such day has ever been measured: the 3.02 min run stopped on a lost Bout 3, and the 105.35 s
  four-bout figure ran Bouts 3-4 as water proxies (`apps/vr/tasks/T21-tiro-day-chain.md:82-84`). Until both hold,
  `docs/PROJECT-PLAN.md:71` and the campaign docs keep 7-10 (6-10) and
  `docs/submission/VIDEO-BEAT-SHEET.md:62` keeps flagging ten minutes as a target, not a measurement.
- **Accepted cost:** a judge's hands-on time stays near 2 min - B buys non-interactive minutes. And there is no gate on
  day length between this ruling and that re-measurement; the number is logged and watched, not enforced.

## 2026-10-21 - Asset originality: AI-made assets ship with a licence ledger (RULES-CHECKLIST row 6)

> Owner, 2026-10-21 (11:49Z), on RULES-CHECKLIST row 6: "accept, with a licence ledger. Assets made with Leonardo, TRELLIS.2 and ElevenLabs may ship, on the reading that the entrant authors them by prompting, selecting and editing, and holds each service's licence. The owner knowingly accepts the risk that Meta reads "original" to exclude AI output."

- **What binds:** assets made with Leonardo, TRELLIS.2 and ElevenLabs may ship. The reading is that the entrant authors
  them by prompting, selecting and editing, and holds each service's licence. Row 6 of `docs/submission/RULES-CHECKLIST.md`
  is closed on this entry. Other AI tools (the Grok and Antigravity image tools behind the A01-A09 studies, sheets and
  storyboards) are not named by the ruling and are not covered by it.
- **Accepted risk:** the rules (Official Rules section 4, "Originality") say the Project "must be ORIGINAL to each
  Entrant" and never mention AI output. Meta may read "original" to exclude AI-made assets, and the Sponsor "may at any
  time request written proof of ownership or proper license". A reading against the entrant can mean disqualification.
  The owner accepts this knowingly. It is not removed by the ledger.
- **Condition: the licence ledger.** `docs/submission/ASSET-LEDGER.md` carries, for every shipped asset, a source line
  (tool, model, the log that records the call, the human edits) and a licence line (the terms that grant it and any
  condition). An asset with no ledger row does not ship. A row whose terms were not read, or whose plan condition is not
  confirmed, is open and its class does not ship until it closes. As written on 2026-10-21, the ElevenLabs conditions
  (a paid plan, and the music "Studio Games" carve-out) and the Leonardo terms (not read) are open in the ledger.
- **Where it is used:** a hand-modelled Blender part (no AI) is the entrant's own work and takes its own ledger row.
- Lost: certainty on the originality rule. Nothing in the repo can settle how Meta reads it.

## 2026-10-21 - Asset transport and ElevenLabs as the audio source

> Owner, 2026-10-21: "through git LFS; same for creatures and props; push to origin master"

- Lynx-made assets (FBX, PNG) cross to Wolf through Git LFS in this repo, pushed to origin master, for characters,
  creatures and props alike.

> Owner, 2026-10-21: ElevenLabs is the source for the SFX and the T40 music. The key is `ELEVEN_LABS_API_KEY` in
> `C:/Users/kazda/kiro/bp/.env`, read at runtime, never printed or committed, and every call is logged next to the assets.

- Pipeline: `C:/Users/kazda/ai/lynx-bench/ASSET-PIPELINE.md`.

## 2026-10-21 - Deadlines and Q9: 18 Nov is the only hard date; Q9 is not posted

> Operator, 2026-10-21, relayed through the App Master: "No need to post anything, final deadline is Nov 18, 2026, other
> ones were soft milestones."

- **What binds, Q9:** the organizer question is not posted. Q9 is closed as not needed.
  `docs/submission/ORGANIZER-QUESTION-Q9.md` records this in its status line and is otherwise unchanged. The provisional
  acceptance of the reuse (2026-10-07, Q9 entry) is not changed by this entry.
- **What binds, dates:** the only hard date is the competition final deadline, Wed 18 Nov 2026
  (https://start-developer-competition-26.devpost.com/; 12:00 PT per `docs/PROJECT-PLAN.md` line 4). Every other dated
  gate in the plan, the KPI readings and the cards is a soft internal milestone: among them 12 Oct (Q9), 18 Oct
  (mechanics), 19 Oct (style pick), 23 Oct (R0), 25 Oct (content freeze), 31 Oct (desktop core gate), 4 Nov (V1),
  8 Nov (V2), 11 and 13 Nov (beta, RC) and 16 Nov (store-upload freeze). A missed soft milestone is a re-plan, not a stop.
- **Supersedes:** the Q9 line of the 2026-10-21 frozen-baseline entry below ("post a revised Q9; the operator posts it")
  and the open Q9 line of the 2026-10-20 "VR first" entry (their bodies are not edited). PROJECT-PLAN section 10 R8 and
  section 12 Q9 ("the owner posts ...") and the item 1 row of `docs/submission/README.md` are stale on this point; they
  are not edited here, and where they conflict this entry wins.
- **Where it is used:** `docs/concepts/WOLF-PLAN.md` dates every phase as a soft milestone against 18 Nov.
- Lost: an organizer answer on the reuse question.

## 2026-10-21 - Combat data: the current pin is VR's frozen baseline

> Operator, 2026-10-21, relayed through the App Master (answering the gate ask): "A, frozen baseline."

- **What binds:** the pin recorded in `apps/vr/data/PINNED.json` is VR's frozen baseline. `node apps/vr/tools/check-pin.mjs`
  and the 45 conformance vectors stay a merge gate against that baseline, unchanged. `apps/vr/data/pinned/**` is still never
  edited. Every VR combat-data change goes into the VR overlay (`apps/vr/data/vr/`). CR-001..CR-007 become VR design records
  for overlay changes, not requests to TV, and nobody re-pins from TV.
- **Brief goal 3 reads in effect:** "baseline unchanged (check-pin green, 45 vectors byte-identical) and the VR overlay
  passes its checks".
- **Supersedes:** the "pending that ask" lines of the 2026-10-20 "VR first" entry below (its body is not edited). The
  gate question it left open is ruled here. The docs that entry listed are aligned in the same change: `docs/REPO-LAYOUT.md`,
  `CLAUDE.md` "Combat data", `apps/vr/data/README.md` and the headers of `docs/change-requests/CR-00x`.
- **Q9:** the operator's words: "post a revised Q9; the operator posts it." The revision is being drafted in
  `docs/submission/ORGANIZER-QUESTION-Q9.md`; the status there records the date and link once the operator has posted it. It
  has not been posted.
- **Cross-channel record:** the TV-side record of this ruling is still owed.
- Lost: B (VR-owned data, rework of check-pin); C (TV stays the data channel until 18 Nov).

## 2026-10-20 - VR first: the TV data and design are the baseline, the TV game becomes obsolete

> Operator, 2026-10-20, relayed through the App Master (answering Q9): "It would be preferred to use data and game design as the baseline, from this baseline TV game will become obsolete and VR suprerior with main focus"

- **What binds:** VR (this repo) is the main product. The TV game is obsolete as a product. The TV combat data and game
  design, as pinned in `apps/vr/data/PINNED.json`, are the starting baseline.
- **What stays in force until the owner rules on the gate:** the pinned data stays as it is (`apps/vr/data/pinned/**` is
  not edited), `node apps/vr/tools/check-pin.mjs` and the 45 conformance vectors stay a merge gate, and CR-001..CR-007 keep
  their current status. The owner has not yet ruled how the conformance gate changes (frozen baseline, VR-owned data, or
  keeping TV as the data channel). That is an open scope ask, and this entry does not decide it.
- **Superseded in part, pending that ask (none of these is edited here):**
  - DECISIONS 2026-10-07 "Two repositories, TV and VR" (the line that TV owns the canon);
  - `docs/REPO-LAYOUT.md` (the "The canon" row and "Changes flow back as change requests");
  - `CLAUDE.md` "Combat data has one owner";
  - the "Filed for the desktop/TV kernel channel" lines in `docs/change-requests/CR-00x`;
  - the brief's key goal 3 ("Keep combat data conformant with the TV game").
- **Q9:** reusing the operator's own data and design still stands (DECISIONS 2026-10-07, Q9 entry). Whether Q9 is posted
  or withdrawn is still open and is with the owner. `docs/submission/ORGANIZER-QUESTION-Q9.md` is not changed.
- **Cross-channel record:** REPO-LAYOUT says a decision that changes every channel is recorded in the TV repository
  (`kiro/mage-arena-tv`). That TV-side record is still owed; it has not been written.
- Lost: TV as the main product or co-equal channel.

## 2026-10-20 - Cut slice: no cut, the 31 Oct desktop core gate plans the full Tiro day

> Operator, 2026-10-20, relayed through the App Master: "keep every bout; plan the 31 Oct gate on the full Tiro day and
> flag the risk."

- **What binds:** this rules the 18 Oct pivot. Bout 2 (creatures) stays, and the second school stays in: **Air**, the Gale
  Tent final with Lio (`docs/change-requests/CR-007-air-school.md`; DECISIONS 2026-10-07). The 31 Oct desktop core gate
  is planned on the full Tiro day, not on a cut slice.
- **The risk, in plain words:** the 18 Oct mechanics gate was missed, and the 23 Oct second-school checkpoint is three
  days away. The plan's own fallbacks are the two things this ruling declines: PROJECT-PLAN section 1 says a missed 18 Oct
  gate slips at most to 21 Oct "and cut bout 2", and section 10 R4 lists the cut order (Mirror IV, Lash IV, bout 2,
  narrow-FOV mode). DECISIONS 2026-10-07 names the second school as the first cut if the 23 Oct checkpoint slips. Keeping
  everything leaves the 31 Oct gate with no scope to give back. DF-008 (Lio loses every duel in about 20-24 s) is also
  still open.
- Sections read: PROJECT-PLAN section 1 (milestone and gate rows, the 18 Oct and 31 Oct slip rows), section 5 (D2 and D4
  rows), section 6 (the Tiro-day timeline, Bout 2 creatures), section 10 (R0, R4).
- Lost: the cut options (cut bout 2; cut the second school; port a two-bout slice).
- Not part of this ruling: PROJECT-PLAN.md is not edited; where it conflicts, this entry wins.

## 2026-10-20 - DF-003, DF-005, DF-006, DF-007, DF-008, DF-009: adopt every recommendation

> Operator, 2026-10-20, relayed through the App Master: "adopt each DF's recommended option; card one design-suite
> re-run."

- **What binds:** the recommended option of each finding, in the finding's own words:
  - DF-003: its Recommendation section: leave `wide` in the proposal file (`calibration-proposal.json`); the finding
    itself says do not sign it off.
  - DF-005: **A. Re-arm on clear** (the ward counts as fresh again for one `perfectWindowS` after the latch clears; VR
    overlay only).
  - DF-006: **B. A phase-break beat**, with **A** (adopt the `wide` proposal).
  - DF-007: **A and B together; re-measure** (A: ward some embers, not all, in the reference script; B: slower spit
    cadence, 1.5 s, VR overlay).
  - DF-008: **A. AI first** (Lio wards like the Fire mage, uses Air Form against the player's bolts, spends tier II-III
    spells once unlocked; re-measure).
  - DF-009: **A. Count only spectacle** (`missedCast` 0 for Bolts, kept for tier I+ casts).
- **Where the work goes:** the implementation and the one `MageArenaDesign` re-run are a task card for the UE host
  (Wolf, see the host entry below). Nothing here changes code or data. The design suite stays **red** until that run; no
  result is claimed by this entry.
- Lost: the other options of each finding (DF-005 B and C; DF-006 C and D; DF-007 C and D; DF-008 B and C; DF-009 B and C).

## 2026-10-20 - T38 asset source: Claude + owner authoring

> Operator, 2026-10-20, relayed through the App Master: "authored rig with four clips; Lynx's local TRELLIS + Blender
> rigging pipeline (PoF 3D rounds) may feed it."

- **What binds:** the first animated opponent (card T38) comes from an authored rig with four clips (idle, cast, hit,
  death), authored by Claude and the owner. The local TRELLIS and Blender rigging pipeline on Lynx (PoF 3D rounds) may
  feed it.
- Lost: the Meta Asset Library as the source, and paid 3D or animation credits (PROJECT-PLAN R0's fallback and its paid
  option).

## 2026-10-20 - Unreal host: all Unreal work on Wolf (x64); Lynx gets no Unreal install

> Operator, 2026-10-20, relayed through the App Master: "all Unreal work stays on the x64 machine (Wolf). Lynx is the
> Node/docs/orchestration host and does not get an Unreal install. Write UE work as cards/source for Wolf; never claim UE
> results from Lynx."

- **What binds:** UE builds, automation suites and content work run on Wolf only. Lynx writes cards and source for Wolf and
  reports no UE build or test result.
- Lost: a UE install on Lynx.

## 2026-10-16 - D-G4 draw calls are one total budget, and the two Build.cs bBuildEditor blocks stay separate

> Athena, 2026-10-16, under the operator's delegation, answered run `ef6642f6`'s two questions. Recorded as relayed; the
> wording is not quoted beyond the rulings.

**1. Draw calls: one total of <=150, no separate particle line**

- **What binds:** the D-G4 draw-call row is a single total budget of <=150 for the whole frame. Particle systems get no
  draw-call line of their own. The measured gap, median 194 and max 201 against 150
  (`docs/research/BUDGET-DG4-2026-10.md` section 10.2 and 10.3, commit `ca28cce`), belongs to lever B in the
  presentation lane. Lever B's target now includes the particles: the 6 `NS_Budget_` systems add about 11 draw calls at
  the median (8.5 measured median 183 without them), and they are closed by lever B along with the rest.
- **The constraint:** the plan's section 7 Quest 3 budget is one frame. The GPU draws one frame, so a particle
  allowance carved out of it would not give the frame more room.
- Lost: a separate particle draw-call allowance (for example 150 plus a particle line). It would have turned the row
  into a pass on paper with the frame still over 150.
- The row stays **fail** until lever B is on master and the stress driver follows the shipping presentation (section 9).

**2. Build.cs: the two bBuildEditor blocks stay separate**

- **What binds:** `apps/vr/Game/Source/MageArenaVR/MageArenaVR.Build.cs` keeps its two `if (Target.bBuildEditor)`
  blocks apart, as the spec required: the D-G4 `NiagaraEditor` block near the top (for the commandlet that builds the
  `NS_Budget_` systems) and the T26 `UnrealEd` and `AssetTools` block at the end (for the SFX import commandlet). A
  sweep, cleanup or refactor must not fold them into one.
- **The constraint:** the spec (`docs/research/NIAGARA-PROXY-2026-10.md` section 4.1) placed the Niagara lines above
  the public dependency block so a three-way merge with lane A's edit to the end of the file takes both. A second block
  in the same place conflicts, and each block belongs to a different lane's feature.
- Lost: folding them into one block. Section 4.1 had called that a follow-up commit on master after the merge. It is
  not wanted.
- Not part of this ruling: neither research doc nor `Build.cs` is edited.

## 2026-10-16 - Plan milestone 3, "Asset footprint reduced": measured by waste, not by tracked size

> Athena, 2026-10-16, under the operator's delegation, answered the App Master's scope ask. The ask was whether
> milestone 3's goal compares tracked bytes at or below `11b9bb2`, or something else
> (`docs/research/BASELINE-2026-10-07.md` section B, last bullets). Recorded as relayed; the wording is not quoted beyond
> the ruling.

- **What binds:** the goal measures WASTE, not tracked size. It is met by all three of: 0 unexplained duplicate groups,
  no texture over budget, and no test-only content in the staged `Clips` folder. It is not met or missed by tracked bytes
  against `11b9bb2`.
- **The constraint:** the re-measure at `ddbd3c1` (`BASELINE-2026-10-07.md` section B, commit `9fef818`) found tracked
  bytes at 66,776,864 against 61,109,210 at `11b9bb2`, +9.3 %. The growth is real game content: the first audio import
  (20 SFX uassets, 2.5 MB of LFS), clips, source and docs. Every possible removal recovers under 1 MB: the mudra mirror
  duplicates are 622,918 bytes and kept by design (A.6), and the test-only clips are 311,952 bytes. Together they could
  not close the 5,667,654-byte gap.
- **Where it stands against the three tests** (as of the `ddbd3c1` re-measure): the 2 duplicate groups are the mudra
  mirrors, explained in A.6; 0 texture assets exist and 0 images are over 2048 px; the staged `Clips` folder still holds
  the test-only `Clips/system/` and `Clips/ward/` clips.
- **The action this orders:** move `Clips/system` and `Clips/ward` out of the staged folder. It is being delivered in a
  separate run and is **not done** by this entry. Until it lands, the third test is open and the goal is not met.
- Lost: (a) keep tracked bytes as the measure, which can only be met by deleting real content. (b) Measure staged
  package bytes, where A.3 already removed 188 MB (188,292,369 bytes), so it would read as met while the test-only clips
  still ship.
- Not part of this ruling: the research docs are not edited; they still ask the question as open.

## 2026-10-13 - F12: take UE 5.8.4 on purpose, on one planned day

> Owner, 2026-10-13, answered in the Athena session. The ask was F12 of `docs/research/STACK-OPPORTUNITIES-2026-10.md`,
> "(a) freeze at 5.8.3 until 11-18, or (b) take 5.8.4 on purpose, on one day?": "(b)". The owner's wording is not quoted
> beyond the choice; the answer is recorded as relayed.

- **What binds:** UE 5.8.4 is taken on purpose, on one planned day after it posts, rather than freezing at 5.8.3 until
  11-18. The engine is not updated by surprise and is not touched before that day.
- **The constraint:** 5.8.4 is expected around 2026-10-20 (inferred from the 28-day cadence), inside the sprint to the
  10-31 desktop core gate. The engine and both v207 MetaXR plugins match today on CompatibleChangelist 55116800.
- **The day's procedure** (`STACK-OPPORTUNITIES-2026-10.md` 8.3 and F9):
  1. Before installing, read the posted 5.8.4 notes and compare the new `Build.version` CompatibleChangelist with the
     plugins' BuildId 55116800. If they differ, stop and raise it: the plugins must be rebuilt or updated first.
  2. Cold `apps/vr/tools/build.ps1`.
  3. The full `MageArena` suite, with a per-test diff against the 5.8.3 run.
  4. `node apps/vr/tools/check-pin.mjs`, and the pin hash in the boot log.
  5. Update `docs/XR-TOOLCHAIN.md` and the `CLAUDE.md` Build line.
  - Never take it on a day another UE build or automation run is in flight.
- **Owner action still open:** launcher auto-update for `UE_5.8` should be off, so the update lands on the chosen day and
  not by surprise. Unverified whether it is on now.
- Lost: (a) freeze at 5.8.3 until 11-18. The 10-13 feed recommended it because both known 5.8.4 fixes (UE-397316,
  UE-396802) sit outside the row-3 list. The owner chose otherwise.
- Not part of this ruling: `CLAUDE.md` and `XR-TOOLCHAIN.md` still say 5.8.3 until the day's procedure runs.

## 2026-10-08 - D-G4 particle systems: a commandlet here now, pof P9 later

> Owner, 2026-10-08, asked "The 10-31 budget's particle rows need 6 Niagara systems. The reading (6015e16) says to make
> them with a small commandlet in this repo, from the engine's own default emitter templates. CLAUDE.md says a capability
> pof lacks is built in pof first (pof P9 is that capability), and R8 bans stock packs. Which way?": "Commandlet here
> now, pof P9 later"

- **What binds:** the 6 `NS_Budget_` systems of `docs/research/NIAGARA-PROXY-2026-10.md` 1.4 are made in this repo by
  `Tools/CreateBudgetFxCommandlet` (route (b), 1.2) from the engine's default Niagara emitter templates and default
  materials. They are committed under `apps/vr/Game/Content/Budget/` through LFS and cooked into the V1 APK through
  `+DirectoriesToAlwaysCook=(Path=/Game/Budget)` (R6). The build starts once `MageArenaVR.Build.cs` is clean.
- **The R8 reading** (`docs/PROJECT-PLAN.md:551`, "no Fab or stock packs"): engine default templates that this
  project's own tool duplicated and parametrized after 24 Sep are not a stock pack. The limit: the ruling does not cover
  shipping engine content unchanged (route (a), 1.1), engine sample content, or any Fab or marketplace pack.
- **The pof-first rule** (`CLAUDE.md`, Tooling; `docs/PROJECT-PLAN.md:396`, P9): this is an exception for the stress
  scene's measuring instrument, which is not the art pipeline. pof P9 may absorb the commandlet later. This ruling does
  not change `CLAUDE.md`.
- **The constraint that forced it:** the particle rows of the 10-31 desktop core gate (D-G4) need real systems that can
  be tuned. Route (a) gives untunable bursts (1.1). Route (c) cannot exist in a cooked build (1.3). pof has no P9 yet.
- Lost: "pof P9 first" (it would have held the particle build, and the 10-31 gate would have reported the particle rows
  "not exercised"). Lost: "Desktop proxy only, not in the APK" (the commandlet without the `DirectoriesToAlwaysCook`
  line, so the device particle row (R6) would have waited for assets from pof P9).
- Not part of this ruling: the build still waits for the operator's uncommitted `// touch` line in
  `MageArenaVR.Build.cs` (NIAGARA-PROXY 5.1).

## 2026-10-07 - v2 vision accepted; the 18 Nov build is its Tiro-day slice

> Owner, 2026-10-07, after the vision interview (waves 0-5, answers verbatim in `docs/design/V2-VISION.md`): "Take
> please https://github.com/xkazm04/mage-arena/pull/1, merge, sync with local main, and start to execute implementation
> per the design doc"

- The one-page synthesis in `V2-VISION.md` is the v2 direction. The slice build order, card list and canon change
  requests are in `docs/design/V2-ROADMAP.md`. The vision file stays the history; this entry is what binds.
- **DF-003 answered: HP-gated phases.** Named mages get 2-3 HP-gated phases (proposal 66 % and 33 % HP); each break
  surges both tier clocks by one tier and swells the crowd; the last phase opens with the rival's signature (Brennic:
  Sunfall). Census targets: signature in 100 % of duels that reach the last phase, median 45-80 s, win rate >= 70 % at
  competence 1 and 40-60 % at 1.5. The clock surge is shared semantics, so it goes to TV as a change request; until then
  it is a VR overlay rule.
- **DF-004 answered: option A, ember spit.** Hounds spit slow, telegraphed magic embers from range; the death burst
  throws one last ember. A VR overlay attack until the TV change request lands.
- **Split hands:** the perfect latch clears after the casting hand is idle about 0.3 s; a split Bolt at Flow 5 is
  allowed and does not spend the Crest. Overlay rules, no change request.
- **The collar economy:** Cracks (clean play, saved across days, open the Breaking) and Tithe (spilled magic: renown
  now, a stronger jailer later). The collar caps at tier IV until the Summa's Breaking. New canon, change request
  before it ships.
- **Slice (18 Nov):** prologue, teach, a full Tiro day (ritual, soldiers, creatures, Brennic in the semifinal, the final
  against a second school). **First cut if the Fri 23 Oct checkpoint slips: the second school**; the final becomes
  Corvo, a second Ember entrant on the Fire kernel.
- **ElevenLabs approval extends to pre-rendered voice lines** (ritual, taunts, crowd chorus, 10 words or fewer each),
  generated from a written cue sheet with the count stated on the card. No runtime calls.
- **Second school for the Tiro final: Air** (owner, 2026-10-07, asked "Earth, Air, or Corvo now?": "Air"). The Gale Tent
  entrant (Iskar or Lio) as an AI opponent with defence D (air form). Card T23; Corvo stays the 23 Oct fallback.
- Four routes, one per member of the Four (Cassia, Brennic, Garran, Iskar); Cassia's (Water) is the slice. All four
  schools playable at launch (spring 2027).
- Lost: the cautious campaign proposal (90 s of story per day, static tableaux, no personal arc), sliders for Sunfall
  (earlier unlock or a scripted beat), DF-004 options B and C.

## 2026-10-07 - Q9: reuse of the TV kernel, data and design accepted provisionally, and we ask

> Operator, 2026-10-07, asked "Q9, due before 12 Oct: may the VR entry reuse the TV kernel, combat data and design, all
> made on 1-2 Oct? The rules re-read today still say No pre-existing codebases.": "Accept now and ask too". Note: "record
> the reuse as provisionally accepted in DECISIONS.md with the dated evidence, and draft the question for the operator to
> post to the organizers before 12 Oct. A no comes back as a scope ask."

- Provisionally accepted: the VR entry reuses the TV rules kernel, the pinned combat data and the game design. All were
  authored on 1-2 Oct 2026, after the window opened on 24 Sep.
- Evidence, rules (`/rules`, fetched 2026-10-07, HTTP 200, 183,554 bytes, `grep -F` exit 0): "conceived of and built within
  the competition window (starting September 24, 2026). No pre-existing codebases, no shipped titles, no early access
  builds repurposed". Also "The Project must be ORIGINAL to each Entrant".
- Evidence, dates (read-only git on `kiro/mage-arena-tv`):
  - `git log --reverse --format='%h %ad %cd %s' --date=iso | head -5`: first commit `1e9de1c` at 2026-10-01 23:46:39 +0200;
    kernel commit `13256c6` at 2026-10-02 00:08:42 +0200.
  - `git log -1 --format='%h %ad %cd %s' --date=iso baeac66`: the pinned combat data commit, 2026-10-02 13:48:12 +0200
    (it was `68a4d68` before the 2026-10-07 history rewrite, see `REPO-LAYOUT.md`).
  - `git log --format='%ad' --date=short | sort | head -1`: 2026-10-01, the earliest author date in the TV repo.
  - This repo: `git log --reverse --format='%h %ad %s' --date=iso | head -1`: `5bc06ee` at 2026-10-02 17:09:32 +0200.
  - `git grep` for any date before 2026-09-24 at `1e9de1c` and at `baeac66`: no hit in either.
  - Caveat: author and committer dates are equal for the three commits above, but 5 of 116 TV commits differ by seconds to
    minutes, so the rewrite did touch some dates. The earliest committer date is also 2026-10-01.
- The constraint that forced the choice: the rule says "No pre-existing codebases"; a breach means disqualification
  (PROJECT-PLAN R8, very high impact); and the rules page names no contact channel. It links the competition's Discussions
  page (`/forum_topics`) and the Devpost contact page, with no mailto and no manager email.
- Lost: accept on the dates without asking (a silent disqualification risk we can cheaply remove).
- Lost: ask first and keep Q9 open until the reply (no reply is guaranteed, and the plan cannot wait on one).
- Disclosure, pof: the studio tool `kiro/pof` predates the window (PROJECT-PLAN section 2, eligibility paragraph, and section 8 decision (b);
  `build-5_8-game.log` is dated 2026-06-18). It is editor tooling and does not ship in the APK (the `MageArenaDev` row in section 7). The drafted
  question asks about it separately, so the owner can cut that part.
- Next: the owner posts the drafted question before 12 Oct 2026, from `docs/submission/ORGANIZER-QUESTION-Q9.md`
  (status: draft, not sent). A "no" from the organizers comes back to the owner as a scope decision.

## 2026-10-07 - Two repositories, TV and VR; evidence out of git

> "We have now two repos for TV and VR, all the other should ideally disappear split into those two, gitignore carefully
> not to overweight github repos." Then: "Approved procedure in your recommended order. Names: mage-arena-tv,
> mage-arena-vr. Review evidence outside archive. PNG evidence can be dropped."

- Two repositories, one checkout each: `kiro/mage-arena-tv` (GitHub `mage-arena-tv`, was `mage-arena-pc`) owns the canon
  (combat data, simulator, story, quests); `kiro/mage-arena-vr` (GitHub `mage-arena-vr`, was `mage-arena`) owns the VR
  channel. VR keeps consuming TV through the pin. This supersedes `MONOREPO.md` (the monorepo plan).
- The lane folders are retired; parallel work uses short-lived `.worktrees/`.
- Review evidence media lives in `kiro/mage-arena-archive/`, not in git, and was dropped from TV history. Shipped media
  is in LFS. What was done, and the old-to-new commit map: `REPO-LAYOUT.md`.

## 2026-10-07 - Quest 3S ships untested

> Operator, 2026-10-07 11:15 UTC, asked "Quest 3S is declared, but the planned device is a Quest 3. Should we get a 3S for
> one test session, or ship 3S untested?": "Ship 3S untested". Note: "Operator accepts the risk: Quest 3S stays declared
> and untested; record the acceptance in DECISIONS.md and FEASIBILITY risk 3."

- Quest 3S stays declared (`+SupportedDevices=Quest3S` in `apps/vr/Game/Config/DefaultEngine.ini`). There is no 3S session
  in V3 and no 3S on the 31 Oct device order.
- What carries over: the performance budgets, by chip and eye buffer (FEASIBILITY Q3).
- What stays untested: sigil legibility on the lower-resolution panel, peripheral threat cues (lens and FOV), hand-tracking
  quality, and the 3S camera rate. The narrow-FOV mode (D3) bounds the FOV part only.
- The constraint that forced the choice: the planned device is a Quest 3, and a 3S would be a separate purchase or loan.
- Lost: get a 3S for one V3 session (sigil legibility, peripheral threat cues, the `Camera FPS` line, one perf capture).
- Lost: Quest 3 only (remove `+SupportedDevices=Quest3S`).

## 2026-10-07 - Supported headsets: Quest 3 and Quest 3S

> Operator, 2026-10-07 morning: chose Quest 3 and Quest 3S.

- The APK declares Quest 3 and Quest 3S only (`+SupportedDevices=Quest3` and `+SupportedDevices=Quest3S`; the
  `ExtraApplicationSettings` line that also named Quest 2 and Quest Pro is gone). Delivered as finding F2.
- The milestone-4 frame and memory budgets stay measured on Quest 3.

## 2026-10-03 - Seated balance: calibrate with every knob, and invent school-specific defences

> Calibration may use: wave composition, enemy pressure, player spell power, defence (ward/blink). And: "Invent class
> specific defense mechanisms. We can have designs: A. One hand holds magic barrier, second casts weaker spells. B. Earth
> class can cast stone form (invulnerability to cast the heaviest spell) C. Fire class can cast fire wall first to protect
> against enemies and projectiles D. Air class can take air form with high percentage of dodge mechanism E. Mages wielding
> staff can put staff into ground with magical barrier and cast two handed destructive spells"

- The seated balance calibration (DF-001, DF-002) may move all four knob families, VR overlay only; the owner signs off the
  final numbers.
- New design direction: **each school gets its own seated defence**, turned into concrete mechanics in
  `docs/design/SCHOOL-DEFENCES.md` (gesture, rule, cost, how it is measured). Water - the competition slice - gets A
  (one-hand barrier + one-hand weaker casting) and E (planted staff: a standing barrier that frees both hands for a
  two-handed spell); Fire gets C (fire wall, also used by the Fire mage AI); Earth B (stone form); Air D (air form).
- These are game-design additions for every channel, built VR-first like the schools, and offered back via change requests.

## 2026-10-03 - Seated combat: enemies fight from the floor, nothing reaches the dais (DF-001)

> Option "A: ranged from the dais" for DF-001 (`docs/design-findings/DF-001-seated-wave1.md`).

- From a seat the player lost Wave 1 at 16.45 s with the pinned, walking-calibrated numbers. Decision: **enemies stay on the
  arena floor below the raised dais and attack from range** (spears thrown, stones slung); melee reach never touches the
  dais. Threats come to the player; the player wards, blinks and casts.
- Implemented as a **VR-only overlay** (`combat.vr.json`); the shared kernel rules and the pinned data stay unchanged and the
  conformance vectors stay byte-identical. Re-measure the seated Wave 1; wave composition (option C) is the fine-tuning
  knob if needed. Card T10.

## 2026-10-03 - Sigil shape, and all four schools in every kernel

> "c. circle + bar"

- The canonical sigil is **a circle plus a bar**; the bar's angle selects the line (vertical / horizontal / diagonal),
  as the recognizer, the glyphs and `DESKTOP-INPUT.md` already do. The plan's teach step is corrected.

> "d. all kernels need all 4 schools - I would start on your side"

- Every channel's kernel will implement **all four schools** (Water, Fire, Earth, Air), not only Water. The pinned
  desktop/TV kernel has Water only (its duels are water proxies).
- **VR starts:** the C++ kernel implements the missing schools first, data-driven from the shared data files, and each
  school is offered back to the desktop/TV channel as a change request with its rules, data and test scenarios.
  Fire (with Heat) comes first because the competition slice ends with the Fire mage duel; Earth and Air follow.
- Consequence for conformance: for the new schools the C++ kernel is the reference until the desktop/TV channel ports
  them; their scenarios are kept separate from the TS-generated vectors so the existing 45 stay byte-identical.

## 2026-10-02 - One repository for all channels

> "try to at some point merge the codebases so we don't split folder for each subproject"

- Home: **this public repository, renamed `mage-arena`** (owner's choice over a private home; the whole game design
  becomes public). Order: **VR first, the PC/TV lane branches later** at a quiet point of their orchestrator.
- Plan and phases: `MONOREPO.md`. Phase A runs right after T03 is settled and before T04 is dispatched.

## 2026-10-02 - Start program approved

> "Start program approved, we can charge its benefits whenever the situation calls for it, asset library can be a
> good fallback situation."

- Eligibility condition met (membership is required at submission time).
- Start benefits (priority technical support, mentorship, office hours, software credits, the **Meta Asset Library**)
  may be used when the situation calls for it, without a separate request.
- The Asset Library is the named **fallback for risk R0** (3D models and animation). Each asset's licence is checked
  against the competition's "original, not a wrapper" rule before use, and its use is recorded.

## 2026-10-02 - Who does the work, what may be paid for, and the real risk

> "For music and effects use ElevenLabs services ... other paid tools not allowed without specific request and
> reasoning, like for example Tripo credits for 3D generation and animations if no other choice."

- Music and SFX: **ElevenLabs**, key `ELEVENLABS_API_KEY` (the owner pointed at the personas `.env`; the variable
  actually lives in `kiro/pof/.env`, which is pof's audio provider config). Read it by name; never copy it into this repo.
- **No other paid tool** without a written request and reasoning to the owner first. MiVRy (paid recognizer licence)
  is therefore out: the recognizer is the in-house $Q port. Tripo-style 3D/animation credits only on request, when
  there is no other choice.

> "Risk is in the game design and animations of 3D models - that's something we did not prove to master for a long
> time. I believe we don't overcome it in next 3 weeks, then we reconsider the plan."

- **Top risk: game design quality and 3D model animation**, not the toolchain. **Reconsider checkpoint Fri 23 Oct**
  (three weeks from this decision): if fun-in-greybox and animated 3D opponents are not proven by then, the plan is
  reconsidered with the owner rather than pushed on.

> "For execution use Grok CLI and Grok 4.7 model to do most of the heavy lifting ... it can do most work if you keep
> yourself as orchestrator and provide detailed plan. We then take over together once more challenging and creative
> tasks will remain, keeping Grok as pawn until usage depleted."

- **Execution model:** Grok CLI (`grok-4.7`) implements; the Claude session orchestrates with detailed task cards,
  verifies every result itself and commits. Creative and hard calls (game feel, design judgement, art choice,
  anything Grok fails twice) go to Claude with the owner. Grok runs until its usage is depleted. Protocol:
  `ORCHESTRATION.md`.

## 2026-10-02 - Desktop first, VR from November

> "Headset not decided yet, we will need to have PC experience saving hand gestures under keybindings as quick
> actions. For emulators/simulators we just pick one device most used/popular to test against. Once the game core
> plays well and art/animations resolved, I can invest into any device to port and experiment - probably Meta Quest 3
> 512GB."

- October is a **desktop build** of the same Unreal project. Every hand gesture is reachable from the keyboard and
  mouse (see `DESKTOP-INPUT.md`), through the SAME gesture pipeline the headset will feed later.
- Simulator/emulator target: **Meta Quest 3** profile only.
- No headset is bought, and no on-device gate exists, before the core game plays well and art and animation are resolved.

> "No to WebXR pivot, I would say the core game design VR-less should take until the end of October. We will know by
> this date whether gameplay, graphics, models and animations, audio on high quality level and we can invest full
> speed into VR."

- The WebXR pivot is **rejected**. The engine is Unreal.
- **Gate on 2026-10-31 (desktop core gate):** gameplay, graphics, models, animation and audio at high quality, VR-less.
  Only after it does the VR port start at full speed (1-18 Nov, competition deadline 2026-11-18).

> "Start program not yet - but I will apply."

> "Create in ai-registry subtree in game development specific to Mage Arena - it is game we work in parallel on TV
> platform too, with potential to become PC game in the future. Once one channel masters certain mechanic other can
> benefit so the same game design problems are not repeated."

- Three channels of one game: **TV/desktop** (`kiro/mage-arena`), **VR** (this repo), **PC** (future). A mechanic
  solved in one channel must be reusable by the others.
- Placement chosen by the owner the same day: **hybrid**. Generic lessons -> `game-production` subjects (no product
  names); channel realisations -> `applications/mage-arena-<channel>--<technique>.md`; game facts -> registry
  `memory/` namespace `mage-arena` (seeded in registry commit `3f35c804`, including the mechanic-by-channel matrix).

> "Lets scaffold repo in kiro/mage-arena-vr."

## 2026-10-02 - Art comes last

> "I shortlisted 13, 25, 30 to have in mind throughout the project. Starting style-less with any baseline wireframe
> convenient to prove the game mechanics. If all aspects of the game will work together, then we spend effort to
> finetune the graphical side and art, as technical limitations will enforce us to prefer simplistic styles over
> realism and high fidelity."

- Greybox until the mechanics work together. Shortlist: **13 Screen-Print Poster, 25 Moonlit Silver Nocturne,
  30 Chalk & Slate** (`ART-STYLE-STORYBOARD.md`). The PC game's Tessera & Lime is not inherited.
- The greybox still honours the threat colour language (element colour = absorb, steel = dodge, black core = leave).
