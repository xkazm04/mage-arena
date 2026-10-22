# T53 - Import the arcane warden, run the pre-import checks, place it at the dais rail

Status: open (needs Wolf: UE 5.8 editor; written 2026-10-22 on Lynx, where none of it runs; do it after T42, the arena kit)
Max turns: 120

## Goal
The warden who watches every duel is a rigged character, not a greybox box: import `SK_Warden.fbx` (one skinned mesh, 23-bone
mannequin-chain skeleton, a bronze staff in the right hand, takes `Idle`, `Cast`, `Hit`, `Death`) from
`apps/vr/art/characters/warden/export/` to `Content/Characters/Warden/`, re-run the pre-import checks, and show it standing at the dais rail
(Warden Septima, `docs/campaign/05-c2-slice-story-bible-and-script.md`, dais rail left) playing `Idle`, with `Cast` available for the ritual.
The warden is **optional**: with the assets missing the rail is empty and no test or capture changes. **The warden awaits the T16 style
pick**: this card imports the greybox flat colours; it does not paint, light or restyle.

## Read first
- `CLAUDE.md`, `docs/DECISIONS.md` (they outrank this card; 2026-10-21 assets cross through Git LFS)
- `apps/vr/art/characters/warden/HANDOFF.md` (landing path, file table, the 13 checks as measured on Lynx, import notes),
  `export/SOURCE.md`, `manifest.json`, `export/metrics.json`
- `apps/vr/tasks/T38-first-animated-opponent.md` ("Hand-off", the pre-import checks) and `apps/vr/tasks/T52-crowd-cards-import.md` (the card shape)
- `apps/vr/tasks/T42-arena-kit-import.md` (where the dais and its rail are)
- Code: `Greybox/ArenaGreyboxActor.{h,cpp}`, `Session/SessionPresentation.{h,cpp}` (how the other characters are spawned and their materials)

## Deliverables (exact paths)
1. Pre-import checks re-run on Wolf, reported one by one with pass or fail: one skeleton and one skinned mesh; exactly the four clips
   `Idle`, `Cast`, `Hit`, `Death`, none empty (Lynx: 61, 47, 25, 61 frames at 30 fps); LOD0 24,148 tris (budget 25,000); one flat colour
   per material, no embedded textures; origin at the feet, +X forward, unit scale; height 1.745 m +-10 %; weights normalised, at most 4
   influences, 0 unweighted vertices; no floaters (the 4 small components are the staff, on `hand_r` only). Then confirm the FBX sha256
   against `manifest.json`. A failure is reported as the exact defect for a Lynx re-export, **never fixed in the editor**.
2. `Content/Characters/Warden/SK_Warden.uasset`, its skeleton and physics asset, and the four animation sequences, committed through LFS.
3. The material set: **five** materials, each a flat colour, no textures, named as the FBX has them: `M_Warden_Robe` (slate grey),
   `M_Warden_Sash` (charcoal), `M_Warden_Trim` (bronze, also the staff), `M_Warden_Skin`, `M_Warden_Sandal`. Base Color from the values in
   `export/metrics.json` (`base_color_linear`); Roughness 0.8, Metallic 0. No per-import duplicates (`M_Warden_Robe_1` is a defect).
   Lever B (`T37-shared-part-materials.md`) decides whether they join the shared set; do not widen the material count past five.
4. Placement: a skeletal mesh actor at the dais rail, left, on the `Idle` loop, facing the arena (the mesh faces +X, the same axis as the
   arena); the position comes from `apps/vr/data/vr/arena-layout.json`, not from the Lynx render (its 4 m is an assumption). Missing asset:
   log one line `MAGEVR_ARENA_WARDEN missing=<asset>` and place nothing.
5. A test: with the asset the actor exists, plays `Idle` and its feet are on the dais top; without it the arena still builds.
6. Evidence: a seated capture of the warden at the rail next to `export/Warden_game_640x480.png`.

## Import notes
- Import as a skeletal mesh with its own skeleton (do not retarget onto the mannequin in this card). Import units: the FBX is in metres,
  Unreal scale 100; check that the height reads 174.5 cm.
- In the A-pose reference the staff points out diagonally from `hand_r`; the editor bounds are wider than the body. That is expected.
- `Cast` is the left hand's push (spell effects would start at `hand_l`, key frame 23); the staff stays upright. No gameplay is wired to it here.
- Forward shading stays on; shadows follow the arena's own settings.

## Constraints
- Do not commit, push, or touch any repository other than this one. Do not call paid services. Do not edit `docs/DECISIONS.md`.
- Do not edit `apps/vr/data/**` or `apps/vr/art/characters/warden/**` (a mesh or clip defect goes back to Lynx).
- Greybox phase: no art pass, no new textures, no cloth, no IK, no retarget.

## Acceptance (the orchestrator re-runs these; quote their real output in your report)
```
powershell -NoProfile -File apps/vr/tools/build.ps1          # expected: editor target builds
<UE automation: the MageArena suite>                          # expected: no new failure beyond the known list in CLAUDE.md
<UE automation: the new warden placement test>                # expected: pass, with and without the asset
node apps/vr/tools/check-pin.mjs                              # expected: OK (no combat data touched)
```

## Report
Write `runs/T53/REPORT.md`: what you built (paths), each acceptance command with its real output (run on Wolf: host name and engine
build), the pre-import checks one by one, the measured height and triangle count in the editor, the material names as imported, anything you
could not do and the exact error, and decisions the card did not specify.
