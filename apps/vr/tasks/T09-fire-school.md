# T09 - Fire school and Heat in the C++ kernel (VR-first), with an independent test oracle

Status: open (queued after T08 - both touch the kernel; one Unreal build at a time)
Max turns: 300

## Goal
Owner decision 2026-10-03 (`docs/DECISIONS.md`): every channel's kernel implements all four schools, and **VR starts**.
The pinned desktop/TV kernel has Water only - its duels are water proxies - while the competition slice ends with the
Fire mage duel. Implement the **Fire school** first: its Heat resource and its spells, data-driven from the shared
data, used by the Fire mage AI in the duel. Offer it back to the desktop/TV channel as a change request.

## Read first
- `CLAUDE.md`, `docs/DECISIONS.md` (newest entries), `docs/PROJECT-PLAN.md` section 6 (Fire mage duel, Sunfall),
  `apps/vr/tasks/T06-kernel-port.md`, `T07-mage-ai-and-games.md`, `runs/T07/REPORT.md` (Fire rows loaded, not cast)
- Data (pinned, read-only): `.../baseline-fourteen-nights/design/data/schools.json` (`fire.combatIdentity`: Heat range,
  gains, decay, thresholds Kindled/Blazing), `.../reference-the-ledger/design/data/spells-fire.csv` (10 spells: tiers,
  shapes, cast/cooldown/mana/damage, blockable, telegraph, range, heat_gain, notes), `combat.json`, `stats.csv`
- Code: `Kernel/*` (catalog, kernel step, water lines, mage AI, games), `Tests/*`

## Deliverables
1. **Heat** as the Fire school's resource exactly as `schools.json` defines it: every gain rule, decay after the idle
   delay, the thresholds and their effects (spell damage multipliers, absorb drain and stamina regen multipliers at
   Blazing). Every number read from the data; where the data's wording is ambiguous, choose the most literal reading,
   record it in `docs/schools/FIRE.md`, and mark it for the owner.
2. **Fire spells** from `spells-fire.csv`: Bolt (Ember Dart), the lines and tiers, shapes (projectile, cone, ring,
   ground zone, dash), blockable classes (`absorbable`, `physical`, `unblockable` - Sunfall is unblockable with its
   telegraph), cooldowns and mana; implemented in a `Fire.*` module parallel to `Water.*`, through the same catalog.
3. **Fire mage AI**: the duel opponent casts Fire spells (including Sunfall at about 45 s as the plan's session table
   describes) instead of the water proxy, keeping the competence model from T07. Put a switch so the water proxy stays
   available for the existing conformance vectors.
4. **Independent oracle tests** (`MageArena.Fire.*`): there is no TS reference for Fire, so record-and-replay alone
   would prove nothing. Each test computes its expected numbers **by hand from the data in the test itself** (e.g.
   heat after N bolt hits with decay; damage at Kindled vs Blazing; Sunfall bypassing a held ward; mana and cooldown
   gating) and asserts the kernel matches. Then add golden VR-reference scenarios under
   `apps/vr/data/scenarios-vr/` (separate from the TS vectors) for a full Fire duel at competence 1 and 1.5.
5. **No regression:** the 45 TS conformance vectors stay byte-identical and green; every `MageArena.*` test green.
6. **Change request** `docs/change-requests/CR-002-fire-school.md` for the desktop/TV channel: the rules as implemented,
   every interpretation of ambiguous data, the scenario files they can replay, and what their TS port must match.
7. `runs/T09/REPORT.md`: files, the oracle tests and their hand-computed expectations, the duel outcomes, step cost,
   and every data ambiguity you resolved.

## Constraints
Do not commit, push, or modify other repositories; do not edit `apps/vr/data/pinned/` or art. No retyped tuning:
everything from the pinned data, interpretations documented. One Unreal build at a time.

## Acceptance (the orchestrator re-runs these)
```
node apps/vr/tools/conformance/generate.mjs     # 45 TS vectors byte-identical
powershell -NoProfile -File apps/vr/tools/build.ps1
"C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "C:\Users\kazda\kiro\mage-arena-vr\apps\vr\Game\MageArenaVR.uproject" -ExecCmds="Automation RunTests MageArena;Quit" -unattended -nullrhi -nosplash -log
```
