---
id: 0002
kind: decision
scope: path:apps/vr/data/vr
date: 2026-10-05
supersedes: null
refs: [0004, 0005]
---

Seated (VR-only) combat numbers live only in the overlay `apps/vr/data/vr/combat.vr.json`; the pinned copy under
`apps/vr/data/pinned/` and the conformance vectors under `apps/vr/data/conformance/` stay byte-identical. **Why:** the
pinned data was calibrated for a walking game (DF-001: the seated player lost Wave 1 at 16.45 s; walking, the same wave
was won at 37.63 s), but the shared kernel rules belong to `kiro/mage-arena`. The owner chose option A on 2026-10-03:
enemies stay on the arena floor and attack the dais from range, implemented as the overlay (T10), with the shared rules
and the 45 TS vectors unchanged. A seated balance change is an overlay edit the owner signs off; a shared-rule change is a
change request (`docs/change-requests/`) followed by a re-pin.
