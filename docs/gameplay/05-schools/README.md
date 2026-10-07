---
title: Schools overview
channel: vr
status: partial
verified-against: 46643e2 (2026-10-07)
sources: [docs/DECISIONS.md, docs/PROJECT-PLAN.md, docs/schools/FIRE.md, docs/design/SCHOOL-DEFENCES.md, docs/change-requests/CR-002-fire-school.md, apps/vr/Game/Source/MageArenaVR/Kernel/Catalog.cpp, apps/vr/Game/Source/MageArenaVR/Kernel/Games.cpp, apps/vr/Game/Source/MageArenaVR/Session/SessionFlow.cpp, apps/vr/Game/Source/MageArenaVR/Session/ArenaSession.cpp]
data: [apps/vr/data/pinned/docs/design/baseline-fourteen-nights/design/data/schools.json (schools), apps/vr/data/pinned/docs/design/baseline-fourteen-nights/design/data/spells-water.csv, apps/vr/data/pinned/docs/design/reference-the-ledger/design/data/spells-fire.csv, apps/vr/data/pinned/packages/core/src/arena/data/runtime.json (water.presets, games.opponentPresets), apps/vr/data/pinned/docs/design/baseline-fourteen-nights/design/data/combat.json (lines)]
---

# Schools overview

Mage Arena has four schools, one per tent: Water (Tide), Fire (Ember), Earth (Stone) and Air (Gale). In the VR build
only **Water is playable**: the player always casts the pinned `Rotation` preset (Tide Orb, Lash, Mirror, branch B on
every line). **Fire is built as an opponent school** (Heat, ten spells, the Fire mage AI and its fire wall) but is not in
the default session, whose duels use Water-proxy mages. **Earth and Air are designed only.** The owner decided on
2026-10-03 that every channel's kernel implements all four schools, VR first; Fire was done first because the slice
ends on the Fire mage duel.

## The four schools

| School | Tent | Colour (`schools.json`) | Resource | VR status | Page |
|---|---|---|---|---|---|
| Water | the Tide Tent | `#2f7fb5` | Flow 0-5 | **playable** (player) and opponent (water-proxy duelists) | [water.md](water.md) |
| Fire | the Ember Tent | `#d4582a` | Heat 0-100 | **opponent only**; opt-in per bout; not in the default arc | [fire.md](fire.md), detail in [FIRE.md](../../schools/FIRE.md) |
| Earth | the Stone Tent | `#7a6a3a` | Footing 0-6 | **designed** (stone form), no code | [earth-and-air.md](earth-and-air.md) |
| Air | the Gale Tent | `#5f9e98` | Momentum 0-100 | **designed** (air form), no code | [earth-and-air.md](earth-and-air.md) |

The greybox threat colours (turquoise for water, vermilion for fire) come from `apps/vr/data/vr/arena-layout.json`
`threats.order`, not from the `schools.json` colours; see [04-threat-language.md](../04-threat-language.md).

## Slice loadout

| Item | Plan (section 6.3) | Build | Source |
|---|---|---|---|
| Lines | Tide Orb, Lash, Mirror, fixed | Tide Orb, Lash, Mirror, fixed (preset `Rotation`) | `runtime.json` `water.presets[2]`; `SessionFlow.cpp:50-60`; `ArenaSession.cpp:239` |
| Bolt | Rain Needle, always available | Rain Needle on slot 0 (bolt flick) | `spells-water.csv` row `bolt` |
| Lash II | fixed to Riptide | Riptide (branch B) | Rotation `lash: "B"` |
| Mirror II | fixed to **Reflection** | **Ripple** (branch B) | Rotation `mirror: "B"` |
| Tide Orb IV | player picks Leviathan Orb or Rain of Orbs at the first intermission | **Rain of Orbs fixed**; no pick | Rotation `tide_orb: "B"`; no branch code in `Session/` |
| Mend, Mire | cut as player lines | not in the player loadout; used by opponent presets | `runtime.json` `water.presets` |
| Tier IV release | sigil, then a two-hand mudra within 1.5 s | sigil alone; the `mudra` clip has no session effect | `ArenaSession.cpp` `PlayAction` |
| Slots | 3 | 3 | `combat.json` `lines.slots` |

## Per-school status

| School | Resource code | Spells | Player | Opponent | Defence (SCHOOL-DEFENCES) | Change request |
|---|---|---|---|---|---|---|
| Water | Flow in `Kernel/Water.cpp` | 24 rows (Bolt + 23 line rows), all ported | yes (Rotation) | yes (Undertow, Mirror tide presets) | A split hands and E planted staff, live overlay values, not signed off | none needed (pinned TS kernel has Water) |
| Fire | Heat in `Kernel/Fire.cpp` | 10 rows, all ported | no | yes, opt-in (`bFireMages`) | C fire wall, AI only, live overlay values, not signed off | CR-002, open |
| Earth | none | none in pinned data | no | no | B stone form, design draft | none |
| Air | none | none in pinned data | no | no | D air form, design draft | none |

## Status and gaps

- Plan section 6.3 versus build: Mirror II and the Tide Orb IV branch pick differ (see the loadout table); the source is
  the pinned preset choice `Rotation`, which cannot be edited in this repo.
- Plan section 6.4 puts the Fire mage in Bout 3; the default arc runs Water-proxy duelists in Tiro bouts 3 and 4
  (`Games.cpp:96-118`).
- Plan section 6.8 cuts "the other three schools as playable"; the owner's 2026-10-03 decision requires all four in
  every kernel. Fire is in the VR kernel as an opponent; Earth and Air are not started.

## Open questions

- Should VR own a slice-specific player preset (Reflection, Leviathan pick), and where would it live, given that presets
  are pinned data?
- When does the Fire mage become the default Bout 3 opponent?
- See the open questions on each school page.
