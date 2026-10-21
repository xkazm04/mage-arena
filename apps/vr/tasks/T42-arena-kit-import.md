# T42 - Import and place the arena environment kit (greybox stays the fallback)

Status: open (needs Wolf: UE 5.8 editor, the MetaXR plugins and `capture-budget.ps1`; written 2026-10-21 on Lynx, where
none of it runs)
Max turns: 200

## Goal
The arena every 31 Oct gate capture stands in (T16 style, T38 opponent, D-G4 budget) is built from the Lynx kit instead of
greybox boxes: 11 static meshes from `apps/vr/art/environment-kit/kit3d/export/`, imported once to
`Content/Environment/ArenaKit/`, sharing six materials, and placed by `AArenaGreybox` from the assembly in
`apps/vr/art/environment-kit/kit3d/manifest.json`. **The greybox stays the fallback**: when any kit asset is missing (a fresh
checkout without LFS, a cooked build that did not stage them, a load failure), the actor builds today's boxes for that part,
and every existing test and capture keeps working.

## Read first
- `CLAUDE.md`, `docs/DECISIONS.md` (they outrank this card if they conflict; 2026-10-21: assets cross through Git LFS; asset
  originality and the ledger - the kit's row is `docs/submission/ASSET-LEDGER.md` Group 8, mark ok)
- `apps/vr/art/environment-kit/kit3d/HANDOFF.md` (pre-import checks, which layout entries each piece replaces, import notes),
  `SOURCE.md`, `manifest.json`, `metrics.json`
- `apps/vr/tasks/T38-first-animated-opponent.md` "Hand-off" (the Wolf pre-import checks, read for static meshes)
- `apps/vr/tasks/T37-shared-part-materials.md` (why the six shared materials matter for draw calls)
- `apps/vr/data/vr/arena-layout.json` (read only: the kit was sized from it; do not edit it to fit the meshes)
- Code: `Greybox/ArenaGreyboxActor.{h,cpp}` (`BuildShell`, `BuildProps`), `Greybox/GreyboxUtil.{h,cpp}`
  (`Greybox::MakeInstances`), `Greybox/ArenaLayout.{h,cpp}`

## Deliverables (exact paths)
1. Pre-import checks on Wolf, against `HANDOFF.md`: each FBX opens, one static mesh, the materials are only `M_Arena_*`,
   no textures, origin at the base, +X forward, unit scale (the 3.0 m wall reads 300 cm in UE). A failure is reported as an
   exact defect for a Lynx re-export, not fixed in the editor.
2. `Content/Environment/ArenaKit/SM_Arena_<Piece>.uasset` (11) and `Content/Environment/ArenaKit/Materials/M_Arena_<Name>.uasset`
   (6, flat colours from `manifest.json` `materials`, the same lit basic setup as the greybox), committed through LFS. Every
   mesh's slots point at those six; no per-import duplicates (`M_Arena_Stone_1` and the like are a defect).
3. Placement in `AArenaGreybox`: one `UInstancedStaticMeshComponent` per kit mesh, instances from the manifest's assembly
   (Unreal axes, metres -> centimetres, `yawDeg`, `scale`). Either read the assembly from a copy of
   `manifest.json` staged with the game, or port its maths (it is `BuildShell`'s chord loop: `env_kit.py` `assembly()`); state
   which in the report. The pieces replace the boxes listed in `HANDOFF.md`; the sand floor, marker posts, brazier flame cones,
   threats and hands stay as they are.
4. The fallback: a missing or failed kit mesh leaves that part on its greybox boxes (log one line
   `MAGEVR_ARENA_KIT missing=<piece>`), so the arena is never empty. A launch flag `-MageArenaGreyboxArena` forces the
   greybox for A/B captures.
5. Tests: the existing greybox and capture tests pass unchanged with the kit present and with it absent; a new test checks the
   kit instance counts against the manifest (84 instances: WallStraight 20, WallCurved 10, GateArch 1, TierStraight 24,
   TierCurved 10, TierStair 4, Dais 1, RunePad 3, Seat 3, Brazier 4, Banner 4).
6. Evidence: the seated capture next to `apps/vr/art/environment-kit/kit3d/arena_seated_640x480.png`, and a
   `capture-budget.ps1` run with the kit (draw calls against the D-G4 total of 150, triangles; the kit's assembled count is
   10,620 on Lynx).

## Constraints
- Do not commit, push, or touch any repository other than this one.
- Do not call paid services. Do not edit `docs/DECISIONS.md`.
- Do not edit `apps/vr/data/**` or `apps/vr/art/environment-kit/kit3d/**` (a mesh defect goes back to Lynx).
- Greybox only in this phase: the kit keeps the flat threat-neutral colours; no textures, no art pass.
- Forward shading stays on. Do not set the three config lines `docs/research/MOBILE-INSTANCING-2026-10.md` forbids.

## Acceptance (the orchestrator re-runs these; quote their real output in your report)
```
powershell -NoProfile -File apps/vr/tools/build.ps1          # expected: editor target builds
<UE automation: the MageArena suite>                          # expected: no new failure beyond the known list in CLAUDE.md
<UE automation: the new kit instance-count test>              # expected: pass, 84 instances
<capture with -MageArenaGreyboxArena and without>             # expected: both arenas render; the kit run logs no MAGEVR_ARENA_KIT missing line
node apps/vr/tools/check-pin.mjs                              # expected: OK (no combat data touched)
```

## Report
Write `runs/T42/REPORT.md`: what you built (paths), each acceptance command with its real output (run on Wolf: host name and
engine build), the pre-import check results per piece, the draw-call and triangle numbers with and without the kit, anything
you could not do and the exact error, and decisions you made that the card did not specify.
