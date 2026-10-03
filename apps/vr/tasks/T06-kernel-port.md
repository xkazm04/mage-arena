# T06 - Port the calibrated arena kernel to C++, proven by conformance vectors from the TypeScript original

Status: done (verified 2026-10-03, agy review: no defects)
Max turns: 320

## Goal
The combat rules (absorb, tier clock, Flow, Bolt and Water lines, projectiles, telegraphs, training dummies and bots)
already exist as a deterministic, calibrated TypeScript kernel in the desktop/TV channel. The VR game must run the
SAME rules (`docs/DECISIONS.md`: one owner for combat data; plan section 3: "the TS engine is the reference and the
balance simulator; the Unreal C++ port is checked against it with shared test traces"). The mechanics gate on Sun 18 Oct
requires **20 conformance vectors green** (plan section 5, D2 row). This card ports the kernel core and builds that
conformance harness. Wiring the kernel to hands, the arena and the HUD is a later card.

## Read first
- `CLAUDE.md`, `docs/DECISIONS.md`, `docs/PROJECT-PLAN.md` sections 3 (shared vs diverging), 5 (D2), 7 (architecture)
- `apps/vr/data/README.md`, `apps/vr/data/PINNED.json` and the pinned files under `apps/vr/data/pinned/`
- The reference source **at the pinned commit `68a4d68`** of `C:\Users\kazda\kiro\mage-arena-arena`
  (`packages/core/src/arena/*.ts`, `packages/core/scripts/compile-arena-data.mjs`). Read it with
  `git -C <that repo> show 68a4d68:<path>`; never modify that repository.
- `apps/vr/Game/Source/MageArenaVR/Combat/AbsorbResolver.*` (T04) - reconcile with the port: one absorb rule, not two.

## Deliverables
1. **Conformance vector generator** `apps/vr/tools/conformance/` (Node):
   - extracts the pinned kernel sources with `git show 68a4d68:...` into `apps/vr/.cache/kernel-68a4d68/` (git-ignored),
     compiles its data the way `compile-arena-data.mjs` does **from `apps/vr/data/pinned/`** (prove the pinned bytes are
     what the compiler reads), and runs it with the `tsx` already installed in that repository's `node_modules`
     (read-only use; installing `tsx` locally is fine too);
   - defines **at least 20 scenarios** as scripted `InputFrame` sequences with fixed seeds, covering: bolt hit; each
     Water line at tiers I-IV; Flow build to 5 and the Crest; perfect absorb at the window edge (inside and 1 tick
     outside); held ward never perfect; physical and unblockable vs ward; tier clock unlocks at 0/15/30/45 s and the
     perfect-absorb advance; mana drain and refund with nerve ranks; a dummy wave and a bot duel to completion; reset
     between waves;
   - writes `apps/vr/data/conformance/*.json` (committed): per scenario the seed, the input script, and checkpoints
     every N ticks with the full event log and the state values the C++ side must reproduce, plus the TS `stateHash`.
2. **C++ port** under `apps/vr/Game/Source/MageArenaVR/Kernel/`: plain C++ (no UObjects in the sim core), fixed step
   at the data's `simStepHz`, seeded RNG reproducing the TS `random(state, purpose)` exactly, data loaded from the
   pinned JSON/CSV files at runtime (no retyped numbers), same event kinds. Port `kernel`, `water`, `geometry`, `math`,
   `training`. Leave `mage-ai` and `games` for the next card, but keep their seams.
3. **Conformance tests** `MageArena.Kernel.Conformance.<scenario>` (nullrhi): replay each vector through the C++ kernel
   and compare event logs exactly (kind, actor, tick) and numeric state within a stated tolerance (say 1e-6 relative;
   justify it). Any divergence prints the first differing tick and field.
4. **Unit tests** porting the intent of `kernel.test.ts`, `water.test.ts`, `training.test.ts` (`MageArena.Kernel.*`).
5. Reconcile T04's `FAbsorbResolver` with the port's `resolveHit`: one implementation, both callers; T04's ward tests
   stay green.
6. `runs/T06/REPORT.md`: files, scenario list, conformance results (20/20 or the exact divergences), tolerance choice,
   timing (kernel step cost on desktop; plan budget <= 1 ms), and anything in the TS source that could not be ported
   faithfully, with the reason.

## Constraints
- Do not commit, push, or modify any other repository (read `mage-arena-arena` via `git show` only; running its
  installed `tsx` is allowed). Do not edit `docs/`, `apps/vr/data/pinned/`, art folders.
- No numbers retyped from the TS source: all tuning comes from the pinned data files. If the TS source hard-codes a
  number, port it as a named constant with a comment citing the TS line.
- One Unreal build at a time. All existing `MageArena.*` tests stay green.
- If full conformance is not reached, do not loosen the tolerance silently: report the divergences.

## Acceptance (the orchestrator re-runs these)
```
node apps/vr/tools/conformance/generate.mjs            # regenerates vectors; byte-identical to the committed ones
node apps/vr/tools/check-pin.mjs                        # pin still OK
powershell -NoProfile -File apps/vr/tools/build.ps1
"C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "C:\Users\kazda\kiro\mage-arena-vr\apps\vr\Game\MageArenaVR.uproject" -ExecCmds="Automation RunTests MageArena;Quit" -unattended -nullrhi -nosplash -log
    # all MageArena.* Success, including >= 20 MageArena.Kernel.Conformance.* tests
```
