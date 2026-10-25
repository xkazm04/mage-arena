# T52 - Import and place the crowd cards in the arena stands, and check draw calls against D-G4

Status: superseded by T57 (2026-10-25). T42 never ran, so the crowd stands on the greybox tiers instead of the kit's
treads: one instanced engine plane with per-instance atlas rects (+2 draws), 436 cards, `-MageArenaNoCrowd`
(docs/research/CROWD-T57-2026-10-25.md). The four FBX cards and placement.json stay for the kit, if T42 runs.
Was: open (needs Wolf: UE 5.8 editor, the MetaXR plugins and `capture-budget.ps1`; written 2026-10-22 on Lynx, where
none of it runs; do it after T42, the arena kit)
Max turns: 150

## Goal
The stands the seated player looks at hold a crowd: 470 spectator cards (4 figure variants, 940 triangles) imported from
`apps/vr/art/environment-kit/crowd/` to `Content/Environment/ArenaKit/Crowd/`, placed on the kit's three stand treads from
`placement.json`, **batched so the whole crowd adds few draw calls** against the D-G4 total of 150 per frame (DECISIONS
2026-10-16: one total budget, no separate line for particles or anything else). The cards are grey clay with one muted red
accent; the T16 style pick later swaps the atlas, not the meshes. **The crowd is optional**: with the assets missing, or under
a launch flag `-MageArenaNoCrowd`, the stands are the bare kit tiers and no test or capture changes.

## Read first
- `CLAUDE.md`, `docs/DECISIONS.md` (they outrank this card; 2026-10-16 D-G4 draw calls are one total of <=150 and the row is
  still **fail** until lever B lands, so the crowd must not widen the gap; 2026-10-21 assets cross through Git LFS)
- `apps/vr/art/environment-kit/crowd/HANDOFF.md` (landing path, cards, pre-import checks C1-C6, import notes), `SOURCE.md`
  (the sheet, its Grok provenance, how it was cut), `manifest.json`, `placement.json`
- `apps/vr/tasks/T42-arena-kit-import.md` and `apps/vr/art/environment-kit/kit3d/HANDOFF.md` (the tiers the crowd stands on, the
  Unreal-axis placement, the six shared materials)
- `docs/research/MOBILE-INSTANCING-2026-10.md` (what merges and what does not on the Quest path: an ISM is one draw each and is
  never merged; the three config lines it forbids stay unset) and `docs/research/BUDGET-DG4-2026-10.md`
- `apps/vr/tasks/T37-shared-part-materials.md` (lever B)
- Code: `Greybox/ArenaGreyboxActor.{h,cpp}` (where T42 places the kit), `Greybox/GreyboxUtil.{h,cpp}` (`Greybox::MakeInstances`)

## Deliverables (exact paths)
1. Pre-import checks C1-C6 from `crowd/HANDOFF.md` on Wolf, with **C3 answered explicitly**: after the import StandingPoint
   points to the viewer's right and no figure is mirrored. A failure is reported as an exact defect for a Lynx re-export, not
   fixed in the editor. Do not repaint the atlas.
2. `Content/Environment/ArenaKit/Crowd/SM_Crowd_<Variant>.uasset` (4), `T_Crowd_Atlas.uasset` and one `M_Crowd_Atlas.uasset`
   (atlas as Base Color, alpha as Opacity Mask, Blend Mode **Masked**, clip 0.5, two-sided off, cast shadow off, no translucency),
   committed through LFS. All four meshes use that one material; no per-import duplicates (`M_Crowd_Atlas_1` is a defect).
3. Placement: one `UInstancedStaticMeshComponent` per variant (4 in all), instances from `placement.json` (metres to
   centimetres, `yawDeg`, uniform `scale`, Unreal axes, same convention as the kit's `assembly.instances`), no collision, no
   shadow, the same component set-up the hand ISMs use. State in the report whether `placement.json` is staged with the game or
   its content is ported to a data table. Missing asset: log one line `MAGEVR_ARENA_CROWD missing=<asset>` and place nothing.
4. A test: instance counts against `manifest.json` (470 in all: SeatedCalm 141, SeatedLean 124, StandingCheer 110,
   StandingPoint 95), every instance on a tread top (z 3.50, 4.35 or 5.20 m, +- 1 cm) and inside the arena ring; with the crowd
   absent the arena still builds.
5. Evidence: a seated capture next to `crowd/stands_seated_640x480.png` (same eye, -12, 0, 1.8 m), and the draw-call
   measurement below.

## Draw-call estimate (Lynx, from the source reading; Wolf measures it)
- **Estimate: +4 draw calls** (one per variant ISM, one shared material, masked, no shadow pass), and **at most +8** if the
  mobile path splits an ISM by culling bucket. Not 470: `MOBILE-INSTANCING-2026-10.md` ("Quest difference 2") says an ISM draws indirectly
  and is never merged with another command on the Quest path, so each ISM is exactly one draw regardless of its instance count.
  The per-draw cost of 470 instances is vertex work (940 triangles in all, 8.9% of the kit's 10,620) and masked-pixel overdraw,
  not draw calls.
- Against the budget: 4 of 150 is 2.7%. The row is at median 194 and max 201 today and stays **fail** until lever B lands;
  the crowd is reported as +4, not hidden inside lever B's target.
- If the measured increase is above +8, the fallback is one mesh and **one** ISM: a single quad whose UV rect comes from per
  instance custom data (`PerInstanceCustomData`, 4 floats), so the whole crowd is +1 draw. Do not enable the three config
  lines the research note forbids to get there; report the number and stop.

## How Wolf measures it
Use `apps/vr/tools/capture-budget.ps1` (it reads the `RHI/DrawCalls` stat and prints min, median, p95 and max): three runs on
the same seated scene, the same build, same machine, each as the median over its capture window:
1. kit with **no crowd** (`-MageArenaNoCrowd`),
2. kit **with** the crowd,
3. (if 2 is above +8) the single-ISM fallback.
Report the difference of the medians and of the maxima, per pass if the script splits them (`drawCallsBasePass`,
`drawCallsPrepass`). Also report triangles (expect +940) and the GPU frame time if the script gives it. Run the same pair once
on the desktop proxy and, when a Quest 3 is attached, once on the headset; say which is which. Do not add the crowd to a number
and call the sum a budget result: a row is a measurement or it is not claimed.

## Constraints
- Do not commit, push, or touch any repository other than this one.
- Do not call paid services. Do not edit `docs/DECISIONS.md`.
- Do not edit `apps/vr/data/**`, `apps/vr/art/environment-kit/kit3d/**` or `apps/vr/art/environment-kit/crowd/**` (a card or atlas
  defect goes back to Lynx).
- Greybox phase: no art pass, no extra texture, no new materials beyond `M_Crowd_Atlas`.
- Forward shading stays on. Do not set the three config lines `docs/research/MOBILE-INSTANCING-2026-10.md` forbids.
- The arcane warden is not in this card.

## Acceptance (the orchestrator re-runs these; quote their real output in your report)
```
powershell -NoProfile -File apps/vr/tools/build.ps1          # expected: editor target builds
node apps/vr/tools/crowd/verify.mjs                           # expected: OK: 4 variants, atlas 512x1024, 470 cards, 940 tris
<UE automation: the MageArena suite>                          # expected: no new failure beyond the known list in CLAUDE.md
<UE automation: the new crowd instance-count test>            # expected: pass, 470 instances
<capture-budget.ps1 with and without -MageArenaNoCrowd>       # expected: both render; medians reported; the difference stated
node apps/vr/tools/check-pin.mjs                              # expected: OK (no combat data touched)
```

## Report
Write `runs/T52/REPORT.md`: what you built (paths), each acceptance command with its real output (run on Wolf: host name and
engine build), the C1-C6 results (C3 in a sentence), the draw-call, triangle and frame-time numbers with and without the crowd
(medians and maxima, desktop proxy and headset), the estimate above against what was measured, anything you could not do and
the exact error, and decisions the card did not specify.
