# Mage Arena VR - agent guide

A seated, hands-first spell duel for Meta Quest, built in **Unreal Engine 5.8** (C++ project in `apps/vr/Game/`). Entry for
the Meta VR Start Developer Competition 2026 (Gaming track; deadline **2026-11-18**). One of three channels of one game:
TV (sister build), VR (this repo), PC (future).

## Read first

1. `docs/DECISIONS.md` - the owner's decisions, newest first. They outrank every other document here.
2. `docs/PROJECT-PLAN.md` - the full plan. Where it conflicts with DECISIONS.md, DECISIONS.md wins.
3. `docs/DESKTOP-INPUT.md` - how the desktop build plays every hand gesture through the one gesture pipeline.
4. `docs/ART-STYLE-STORYBOARD.md` - 30 style studies; the owner's shortlist is 13 / 25 / 30.
5. `docs/ORCHESTRATION.md` - who does what: Grok (`grok-4.7`) implements task cards in `apps/vr/tasks/`, the Claude session
   orchestrates, verifies and commits. Paid services: ElevenLabs only, anything else needs the owner's written approval.

## Current phase (until 2026-10-31): desktop core, no VR

- Build the whole game playable on PC first: keyboard plays recorded hand clips into the gesture pipeline, the mouse
  draws sigils. Never add a shortcut that skips the pipeline and claim it tests a gesture.
- **Greybox only.** No art work before the mechanics work together. The greybox must still use the threat colour
  language: element colour = absorb, steel = dodge, black core with a red rim = leave.
- Forward shading is on from day one (it is what the Quest renderer uses).
- Simulator target when VR starts: the **Meta Quest 3** profile.

## Combat data has one owner

The tuned combat numbers (spells, enemies, tier clock, Flow) live in the PC/TS project `kiro/mage-arena` and its
calibrated simulator. This repo consumes a **pinned commit** of that data (see `apps/vr/data/README.md`) and never edits it in
place; changes go back as change requests. A `combat.json` copied from a contest folder is stale - do not use it.

## Tooling

- **pof** (`kiro/pof`) is the studio tool this game is built with, end to end. When pof lacks a capability the game
  needs, the plan is to build it in pof first (plan section 8), not to work around it here.
- Lessons that would help any game (or the TV/PC channels of this one) are written up for the ai-registry; see
  plan section 9 and the registry's own contribution rules. Upper knowledge layers never name this product.

## Build

`powershell -NoProfile -File apps/vr/tools/build.ps1` (editor target, Win64 Development; verified 2026-10-02: succeeded in
54 s on UE 5.8.2). Call `Build.bat` through PowerShell's `&`; quoting it through `cmd /c` from Git Bash fails.

Binary assets (`.uasset`, `.umap`) go through Git LFS (`.gitattributes`). `Binaries/`, `Intermediate/`, `Saved/`
and `DerivedDataCache/` are never committed.

## Git

Commit locally, in small atomic commits. Never push, force-push or bypass hooks unless the owner asks.
