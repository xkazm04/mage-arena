# T71 - crowd density toward S1

Status: done 2026-10-27 (option C, owner ruling 2026-10-27, DECISIONS ruling 1): `stands.tiers` 3 -> 5, 804 cards, crowd top 7.10 m -> 8.80 m, the first 436 cards unchanged, one draw; D-G4 max 110 (stress) and 119 (creature-import) in every run, BasePass max 167,230 and 220,768; `MageArena.` 344 green, `Overlay.Golden` re-recorded (hash lines only). The blind critic round is the Director's, still open. Report: `docs/research/CROWD-DENSITY-T71-2026-10-26.md`. (Was: BLOCKED 2026-10-26, before any Unreal work; the options below are the record.)
Max turns: 160

## Goal
The stands read as the packed bowl of key art S1 (`apps/vr/art/key-art/key-art-s5101-medium.jpg`): rows climb well above
today's three tread rows, and the pink sky band no longer sits where people should be. This closes the main gap the T57
critic left open (`docs/research/CROWD-T57-2026-10-25.md` sections 5 and 6, "Density toward S1"): the stands fill 25-30 % of
the frame height against S1's upper half. Today the crowd is 436 cards and 14 banners on one instanced component (213 of
450 visible at D-G4, 426 tris).

## Read first
- `CLAUDE.md`, `docs/DECISIONS.md` (they outrank this card if they conflict): the T57 ruling on 7fd7d11, the T65 tint
  lift, and the D-G4 entries of 2026-10-16 (one total draw-call budget) and 2026-10-24 (the BasePass triangle reading)
- `docs/concepts/VISUAL-PLAN.md` (b) shortfall 3 and "The arena order"
- `docs/research/CROWD-T57-2026-10-25.md` (all), `docs/research/CROWD-TINT-T65-2026-10-25.md` (the D-G4 table, the critic
  rounds, the ember-fleck watch item)
- `Greybox/ArenaStands.{h,cpp}`, `Greybox/CreateCrowdAssetsCommandlet.cpp`, `Tests/CrowdTests.cpp`, `apps/vr/tools/crowd/`

## Deliverables (exact paths)
- `apps/vr/Game/Source/MageArenaVR/Greybox/ArenaStands.{h,cpp}` - one or more extra instance bands behind and above the
  top tier, on the SAME `UInstancedStaticMeshComponent` with the SAME material `M_Crowd_Atlas` and the T65 custom data
  (8 floats, the seeded tint stream). The new rows take their own seeded stream, so no existing card moves. Back rows
  lean on the standing variants.
- `apps/vr/Game/Source/MageArenaVR/Tests/CrowdTests.cpp` - red first: `Crowd.Placement` covers the new bands (on support,
  out of aisles, facing the floor), `Crowd.OneBatch` the new instance count; `Crowd.TintNotThreat` still holds.
- `docs/research/CROWD-DENSITY-T71-2026-10-26.md` - the report, in the T57/T65 shape.
- `docs/concepts/VISUAL-PLAN.md` (a T71 line under "The arena order"), `docs/concepts/WOLF-PLAN.md` (an evidence line if
  a box is touched).

## Constraints
- At most one Unreal process machine-wide. Build.version 5.8.3. Blender and ComfyUI stay down. No paid service, no image
  generation, no push.
- Do NOT make the greybox stands taller and do NOT edit `apps/vr/data/vr/arena-layout.json`: that needs its own ruling.
  If the bands need support geometry, stop and report the need instead of building it.
- Nothing under `apps/vr/data/` or `apps/vr/art/` changes. Crowd assets are regenerated only by
  `create-crowd-assets.ps1 -Force`.
- One draw per pass: one BasePass and one DepthPass row for the crowd. D-G4 `capture-budget.ps1 -Runs 3` on the after
  build plus a same-day three-run base: max draw calls at or under 150, BasePass visible triangles at or under 350k, in
  every run. Each row of the D-G4 table names its scene.
- A blind critic on shuffled before/after pairs against S1: (a) density, (b) threat-colour class safety, (c) the ember
  flecks over the crowd band. Any (b) or (c) regression blocks the merge.
- Staging: `C:/Users/kazda/mage-art-staging/crowd-density-t71-2026-10-26/`, passed explicitly as `-Staging`.

## Acceptance (the orchestrator re-runs these; quote their real output in your report)
```
node apps/vr/tools/crowd/verify.mjs                      # expected: OK before and after
powershell -NoProfile -ExecutionPolicy Bypass -File apps/vr/tools/build.ps1   # expected: Result: Succeeded
Automation RunTests MageArena. (-nullrhi)                # expected: >= 339 tests, 0 failed, EXIT CODE 0
capture-greybox.ps1, capture-spell-vfx.ps1 -Staging ...  # expected: ok, before and after
capture-budget.ps1 -Runs 3 (after and base)              # expected: max <= 150, BasePass tris <= 350k, parity ok
node tools/check-commits.mjs                             # expected: pass
```

## Blocked (2026-10-26)

**Nothing stands behind or above the top tread.** The greybox stands are one ring of boxes per tier, each running from
the ground to its tread top (`ArenaGreyboxActor.cpp:191`, `ArenaStands.cpp` `StandBox`). Tier 2's top is 5.20 m and its back
edge is 5.30 m outside the wall. Beyond it there is only the dusk dome, 70 m out (`BuildSky`). A band of cards behind and above
the top tread would float in the air. `Crowd.Placement` could not truthfully assert "on support" for it, and hiding the
feet behind the front rows would only fake support. Under the card's own rule, the build stops here.

**What each route buys, from the centre seat** (a scratch model of `StandBox` and `Place` in
`C:/Users/kazda/mage-art-staging/crowd-density-t71-2026-10-26/geometry/density.mjs`. At 3 tiers it gives 436 cards, the
count `Crowd.Placement` asserts. The screen fractions are a pinhole estimate, not a capture):

| Stand tiers | Cards (aisles out) | Crowd top (a 1.9 m figure on the top tread) | Far end: crowd top, % of frame from the top | Flanks |
|---|---|---|---|---|
| 3 (today) | 436 | 7.10 m | 28 % | about 14 % |
| 4 | 612 | 7.95 m | 27 % | about 10 % |
| 5 | 804 | 8.80 m | 25 % | 5-13 % |
| 6 | 1,012 | 9.65 m | 24 % | about 10 % |

The far end barely climbs: it is about 33 m out, so each extra tier (+0.85 m) lifts its top edge only about 1 % of the frame.
Most of the gain is on the flanks and the near side stands. That is where the T57 critic saw "25-30 % of the image height"
against S1's upper half.

### Options for Athena

- **A. Taller stands through the layout (recommended).** `stands.tiers` 3 -> 5 in `apps/vr/data/vr/arena-layout.json`.
  `ArenaCrowd::Place`, `PlaceBanners` and the greybox shell already loop over `Layout.StandTiers`. The extra tiers take the
  back-tread mix (`PickVariant` clamps the tier to the third MIX row: the most standing figures). So this is a ONE-NUMBER
  layout change with no new C++: 804 cards on real treads, the same one crowd component and the same one stone component
  (+64 stone instances, +0 draws expected). Costs: a layout ruling (this card forbids it), an Overlay.Golden re-record,
  and a D-G4 re-check. The SessionPresentation crowd placeholder band (`SessionPresentation.cpp:1405`) also follows
  `StandTiers` and moves up with it. Seed caveat: `Place` draws tiers inside each segment, so more tiers re-seed every card
  after segment 0 (the positions stay on their treads, but the variant and tint per slot change). Option C below keeps
  them stable if that matters.
- **B. Support boxes behind the top tier, outside the layout.** Extra stone boxes on the existing stone component, built
  by the crowd code, with a crowd band on them. This is support geometry that the card explicitly forbids building, and it
  amounts to taller stands that the layout does not know about. Not recommended: A gives the same thing honestly.
- **C. A's geometry with the existing cards held stable.** As A, but the rows on tiers 3 and up are drawn from their own
  seeded stream after the existing 436, so no T57/T65 card changes variant or tint. This is about 20 lines in `Place`, plus
  `Crowd.Placement` extended to the new tiers. It is worth it only if Athena wants the before/after pairs to differ by the
  new rows alone.
- **D. Denser rows on today's treads (no ruling needed, but not the goal).** A second row per 1.55 m tread (436 -> about
  870 cards, still one draw). The top edge does not rise. The rows already overlap vertically (seated figures are 1.3 m
  against a 0.85 m rise), so the likely gain is small and the sky band stays. Not built: it does not reach "rows climbing
  well above today's three".

**The question:** is the layout change of option A (or C) ruled in? If so, this card reopens with
`arena-layout.json` added to its paths, and the D-G4, stills, blind critic and suite run as written above.

## Report
`docs/research/CROWD-DENSITY-T71-2026-10-26.md` once the card runs. Until then, the "Blocked" section above is the record.
`runs/` is never committed.
