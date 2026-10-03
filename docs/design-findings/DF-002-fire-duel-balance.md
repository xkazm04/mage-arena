# DF-002 - The Fire mage, implemented from the data, wins every duel before Sunfall can appear

Found 2026-10-03 by T09 (`runs/T09/REPORT.md`, scenarios `apps/vr/data/scenarios-vr/fire-duel-competence-*.json`).

## Measurement
- Fire school implemented faithfully from `spells-fire.csv` and `schools.json` (oracle tests compute expectations by hand
  from the data; an independent review found no defects).
- Reference player preset (competence 2, Water) against the Ember mage:
  - competence 1 (seed 40000): **loss at 16.0 s**, waves cleared 0;
  - competence 1.5 (seed 40001): **loss at 18.4 s**, waves cleared 0.
- The plan's duel band is 45-80 s; Sunfall unlocks at tier IV (45 s) and is never cast because the duels end first.

## Why it matters
The Fire numbers were never balance-tested in any channel: the desktop/TV kernel only ever ran Water proxies (T07). The
data is authored, not calibrated. It is also unmeasured from a seat (DF-001 applies: these duels use the reference
walking preset, not the seated adapter).

## Options (owner decides)
- **A. VR calibrates Fire** with a headless census in the C++ kernel (many seeds, both competences, seated adapter with the
  T10 overlay), proposes Fire numbers that land the 45-80 s band, and the owner signs them off; the proposal goes back to the
  desktop/TV channel through CR-002 so the shared data changes once.
- **B. The desktop/TV channel calibrates Fire** with its own TS simulation tooling (it owns the data); VR waits and re-pins.
- Orchestrator's recommendation: **A**, after T10 lands, because the seated geometry changes the numbers anyway and VR is
  the channel that needs the duel first.
