# T72 - Fire value contrast against the sand

Status: ran, open for Athena - report docs/research/FIRE-VALUE-T72-2026-10-26.md
Max turns: 120

## Goal
At the 8 m duel distance, Fire (bolt, absorb, perfect) separates from the orange sand by VALUE, not hue: the flame
silhouette sits on a wider and darker backing with a dark rim band at its edge, so the fire reads as a lit shape on a
dark edge against the sand. The hue stays fire vermilion, the heart stays the full element colour (no white or
yellow-white core), and the call cost is T67's. Why now: T67 left Fire "an even orange on an orange floor"
(`docs/research/SPELL-SPECTACLE-R2-T67-2026-10-26.md` sections 3 and 5), and Athena's T69 note names the gap as value
contrast against the sand (`docs/research/HOUND-GLOW-8M-2026-10-26.md`, VISUAL-PLAN (b)(6)). Fire is in every duel at
duel range, so it is the visible gain at 8 m.

## Read first
- `CLAUDE.md`, `docs/DECISIONS.md` (they outrank this card if they conflict)
- `docs/concepts/VISUAL-PLAN.md` section (b) items (4)-(6) and the (c) style block (S1 =
  `apps/vr/art/key-art/key-art-s5101-medium.jpg`)
- `docs/research/SPELL-SPECTACLE-R2-T67-2026-10-26.md` (the method and baseline repeated here),
  `docs/research/SPELL-FLIPBOOK-T63-2026-10-25.md`, `docs/research/HOUND-GLOW-8M-2026-10-26.md`

## Levers, in this order
1. The Fire cells in `apps/vr/tools/vfx/spell-flipbook.mjs`: a wider and darker backing layer and a dark rim band at the
   silhouette edge.
2. Fire card sizes and values only, in `Session/SpellVfx.{h,cpp}`.
3. `M_SpellGlow` parameters only if (1) and (2) cannot reach it, with the reason written down.

## Deliverables (exact paths)
- `apps/vr/tools/vfx/spell-flipbook.mjs` - the Fire cells (flame A and B, embers, crown, spark streak).
- `apps/vr/art/vfx/spell-flipbook.png`, `apps/vr/art/vfx/manifest.json` - the re-rendered atlas (LFS) and manifest.
- `apps/vr/Game/Content/Vfx/T_SpellFlipbook.uasset`, `M_SpellGlow.uasset` - re-imported with
  `create-budget-fx.ps1 -SpellVfx` (LFS).
- `apps/vr/Game/Source/MageArenaVR/Session/SpellVfx.{h,cpp}` - Fire card values, only if used.
- `apps/vr/Game/Source/MageArenaVR/Tests/SpellVfxTests.cpp` - a dark rim band at the fire silhouette (mean shade at or
  under a stated bound, wider than T67's) and no fire shade above 1; run red on the base first.
- `docs/research/FIRE-VALUE-T72-2026-10-26.md` - desktop proxy, not Quest truth; D-G4 table, the measured value contrast,
  the critic table, the gates, open points for Athena.
- `docs/concepts/VISUAL-PLAN.md` - one line (9) in section (b).

## Constraints
- Do not push, and do not touch any repository other than this one. Do not edit `docs/DECISIONS.md`.
- At most one Unreal process machine-wide: build, editor, automation and capture strictly one after another.
- Same call cost as T67: no new draw call, emitter, puff, glow layer or cap change; each glow colour one instanced draw;
  per-instance custom data stays three floats.
- No white or yellow-white hot core: every cell shade at most 1, the heart is the full element colour. No glow brighter
  than its threat. `SpellVfx::GlowColour`, `RimPulse`, `RimPulseColour`, the braziers and the crowd tint do not change.
  Steel never glows. Water, Air and the Unblockable cells and code are unchanged. The dark Fire rim must not read as the
  unblockable's black core with a red rim (the critic checks it).
- Nothing under `apps/vr/data/` changes; no combat number moves. No paid service, no image generation.
- Binaries through Git LFS; never commit `Binaries/`, `Intermediate/`, `Saved/`, `DerivedDataCache/` or `runs/`.
- Evidence in `C:/Users/kazda/mage-art-staging/fire-value-t72-2026-10-26/` only. The hound material is left alone.

## Acceptance (the orchestrator re-runs these; quote their real output in your report)
```
build.ps1                                              # Result: Succeeded
MageArena. automation group (corpus regenerated)       # Found N automation tests, N >= 339, EXIT CODE: 0, no new reds
capture-budget.ps1 -Runs 3 -ExtraArgs -MageArenaCreatureImportScene   # before and after: max draw calls <= 150 and
                                                       # not above the base, BasePass <= 350k, particles + puffs <= 2,000
capture-spell-vfx.ps1 -Run T72-before / -Run T72-after # 11 shots each; 8 m sand crops; edge-band vs sand luminance number
blind critic (up to 3 rounds)                          # fire separation at 8 m, no white core, no unblockable read, no
                                                       # school worse; stop if round 1 reads SAME on every fire pair
spell-flipbook.mjs --check                             # OK on the committed PNG
```

## Report
`docs/research/FIRE-VALUE-T72-2026-10-26.md`; the evidence stays in the staging folder above.
