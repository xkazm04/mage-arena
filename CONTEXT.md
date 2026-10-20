# CONTEXT: . (repository root, the VR channel of Mage Arena)

> Co-located, agent-readable context for this module. Keep it short and TRUE. When the code here
> changes materially, update this file in the same change (the doctor flags it as stale otherwise).

## Owns
The Meta Quest channel of Mage Arena: a seated, hands-first spell duel in Unreal Engine 5.8 (`apps/vr/Game`), built
desktop-first until 2026-10-31, where recorded hand clips drive the same gesture pipeline the headset will.

## Public contract
- `apps/vr/data/PINNED.json` + `apps/vr/data/pinned/`: combat data copied byte-exact from one commit of `kiro/mage-arena`.
  Consumed, never edited; `node apps/vr/tools/check-pin.mjs` is the gate.
- `apps/vr/data/conformance/`: event-log vectors from the TS oracle. The C++ kernel must reproduce them tick-exact.
- `apps/vr/data/vr/combat.vr.json`: the only place seated (VR-only) numbers live. The owner signs them off.
- `apps/vr/Game/Clips/` and `docs/CLIP-SCHEMA.md`: the hand-clip format every gesture test replays.
- `MageArena.*` automation tests are the green gate; `MageArenaDesign.*` are design measurements, red on purpose.

## Invariants: never break
- Every gesture goes through the one pipeline (clip or hand source -> detector -> session). No shortcut is called a gesture test.
- The pinned data and the conformance vectors stay byte-identical; changes go back as change requests (`docs/change-requests/`).
- Greybox keeps the threat colour language: element colour = absorb, steel = dodge, black core with a red rim = leave.
- Forward shading stays on. No art before the mechanics work together.
- Secrets are read by name at run time (ElevenLabs key from env or `kiro/pof/.env`) and never written here.
- Workers never commit; the orchestrator runs the card's checks itself, then commits (`docs/ORCHESTRATION.md`).

## Key files
- `CLAUDE.md`: agent guide; `docs/DECISIONS.md`: owner decisions, which outrank every other document.
- `apps/vr/Game/Source/MageArenaVR/`: `Hands/` (clips, input), `Gestures/` (sigil, ward, blink, staff), `Kernel/` (combat port),
  `Session/` (bout flow and presentation), `Greybox/`, `Tests/`.
- `apps/vr/tools/`: `build.ps1`, `check-pin.mjs`, `lint/` (unused members, changed-line format, clang-tidy), generators.
- `apps/vr/tasks/`: task cards for the workers; `tools/`: worker dispatchers and the agy guard.

## Data flow
Pinned JSON -> `Kernel/KernelData` (+ the `combat.vr.json` overlay) -> `FGames` simulation. Hand frames from a clip player
(desktop) or the headset -> detectors in `Gestures/` -> `FArenaSession` events -> kernel ticks -> `ASessionPresentation`.

## Decisions & memory
- `docs/DECISIONS.md`, `docs/design-findings/DF-001..DF-004`, `docs/change-requests/CR-001..CR-003`.
- `.ai/memory/`: 0002 seated tuning lives in the overlay, 0003 design tests are red by design, 0004 the calibration
  proposal is flag-gated, 0005 teaching claims are measured from the seat, 0006 agy plan mode is not read-only,
  0007 dead UCLASS members need the zero-consumer lint, 0008 verify gates on a clean tree.
