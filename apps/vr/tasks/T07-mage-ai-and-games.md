# T07 - Port the mage AI and the games phase machine; vectors that exercise the RNG

Status: done (verified 2026-10-03, agy review: no defects)
Max turns: 300

## Goal
T06 ported the arena kernel core with 37 green conformance vectors, but none of them draws a random number: the parts of
the TypeScript kernel that use the RNG - the AI mages (`mage-ai.ts`) and the bout/wave phase machine (`games.ts`) - were
deferred. The competition slice needs both: three bouts (soldiers, creatures, the Fire mage duel at competence 1 and 1.5)
and a bot that plays like an opponent (plan section 6, session table; section 5 D2/D3). Port them faithfully and extend
the conformance harness so the RNG is proven by scenarios, not only by a checksum.

## Read first
- `CLAUDE.md`, `docs/DECISIONS.md`, `apps/vr/tasks/T06-kernel-port.md`, `runs/T06/REPORT.md`, `apps/vr/tasks/BACKLOG.md`
- Reference at pinned commit `68a4d68` of `C:\Users\kazda\kiro\mage-arena-arena` (read via `git show` only):
  `packages/core/src/arena/mage-ai.ts`, `games.ts`, `geometry.ts` (+ `geometry.test.ts`, `games.test.ts`), and the
  data they read (`arena-tiers.json`, `spells-fire.csv` - both already pinned under `apps/vr/data/pinned/`).
- The C++ kernel under `apps/vr/Game/Source/MageArenaVR/Kernel/` and the generator under `apps/vr/tools/conformance/`.

## Deliverables
1. Port `mage-ai.ts` and the `games.ts` phase machine into `Kernel/` (same rules: no retyped tuning, constants cited).
   Fire-school spells come from the pinned `spells-fire.csv`.
2. Extend the generator with **at least 8 new vectors that consume the RNG** (each records `randomDraws > 0`): a mage
   duel at competence 1 and at 1.5 to completion, a full Tiro wave sequence, the creature bout (cinder hounds with
   death bursts, mire maw), wave resets, and the arena-tier ladder step the slice uses. Keep all 37 existing vectors
   byte-identical.
3. Port the intent of `games.test.ts` and `geometry.test.ts` (`MageArena.Kernel.*`).
4. Fold in the two T06 backlog notes: move the 0.8 stop factor (`Enemies.cpp:~266`) to `SimConstants.h` with its
   citation; tick the RNG-coverage item once the new vectors pass.
5. `runs/T07/REPORT.md`: files, the new scenario list with `randomDraws`, conformance results, step cost with the AI
   running, and anything not ported faithfully with the reason.

## Constraints
Do not commit, push, or modify other repositories; do not edit `docs/`, `apps/vr/data/pinned/`, art. Tolerance stays as
in T06 (relative 1e-6, absolute 1e-9). One Unreal build at a time; every existing `MageArena.*` test stays green.

## Acceptance (the orchestrator re-runs these)
```
node apps/vr/tools/conformance/generate.mjs    # all vectors regenerate byte-identically; >= 45 vectors
powershell -NoProfile -File apps/vr/tools/build.ps1
"C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "C:\Users\kazda\kiro\mage-arena-vr\apps\vr\Game\MageArenaVR.uproject" -ExecCmds="Automation RunTests MageArena;Quit" -unattended -nullrhi -nosplash -log
```
