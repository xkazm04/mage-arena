---
title: TV campaign mechanisms and their VR treatment
channel: vr
status: designed
verified-against: mage-arena-vr 46643e2; mage-arena-int b1efd46; mage-arena a7964ad (2026-10-07)
sources: [docs/PROJECT-PLAN.md, docs/DECISIONS.md, apps/vr/Game/Source/MageArenaVR/Kernel/Games.cpp, apps/vr/Game/Source/MageArenaVR/Session/SessionFlow.cpp, mage-arena-int:docs/MAGE-ARENA-PLAN.md, mage-arena-int:docs/design/reconciled/data/season.json, mage-arena-int:docs/design/reconciled/data/season-bridge.json, mage-arena-int:docs/design/reconciled/data/death-reservation.json, mage-arena-int:docs/design/reconciled/data/parley.json, mage-arena-int:docs/design/reconciled/data/arena/arena-tiers.json, mage-arena-int:packages/core/src/camp.ts, mage-arena-int:packages/core/src/season.ts, mage-arena-int:packages/core/src/parley.ts, mage-arena-int:packages/director/src, mage-arena-int:packages/core/src/quest.ts (uncommitted), mage-arena-int:story/dialogue/manifest.md (uncommitted), mage-arena:docs/campaign/status.md (uncommitted)]
---

# TV campaign mechanisms and their VR treatment

The TV/PC campaign is a six-week season. It has a camp on an hour-based calendar, thirteen closed intents chosen each
night by an LLM Director with a deterministic planner behind it, Parley, the Tent Trial, the weekly Games, and
relationships, saves with replay receipts, a quest graph and a story pack (the last two uncommitted), plus reserved
deaths and endings. The VR competition slice keeps the arena half and the world, and cuts the camp half
(`docs/PROJECT-PLAN.md` section 6.8). This page maps each TV mechanism to its VR treatment. "Today" is the competition
slice. "Future" is what page 04 proposes. The reasons use five fixed tags: **seated** (the player sits on the dais and
never walks), **session** (sessions of 10 minutes or less), **hands** (hands-only input, no controller, no keyboard
in the headset), **comfort** (VR comfort rules), and **scope** (the competition deadline of 2026-11-18 and the plan's
cut list).

## Treatment legend

| Treatment | Meaning |
|---|---|
| kept | Used in VR with TV semantics unchanged |
| adapted | Used in VR in a different form. The semantics are kept and the presentation or input changes. |
| cut | Not in VR now and not proposed |
| future | Not in the competition slice. Page 04 proposes it for the post-competition campaign. |

## Divergence table

| # | TV mechanism | TV status (`b1efd46` + uncommitted) | VR today | VR future (page 04) | Reason |
|---|---|---|---|---|---|
| 1 | **World, factions and Games lore** (collared mages, tents, Vigil, Lanista) | authored data | kept (implicit; no names on screen) | kept, shown on screen | One game, three channels (`DECISIONS.md` 2026-10-02) |
| 2 | **Season**: six weeks, a Games day every week, persistence of stats, trust, debt, renown, gold, knowledge, mastery and arcs (`season.json`) | built | cut | adapted: a **ladder of Games days**, one per session, with no calendar | session; scope (plan section 6.8 cuts the season and the time loop) |
| 3 | **Time loop** (contest B/2: ring reset, Echo) | removed in TV too (plan D2) | cut | cut | Both channels dropped it. Loop wording left in `arena-tiers.json` is a known TV defect. |
| 4 | **Camp map and hour calendar**: 8 places, 14 waking hours, activity hours, Persona-style day (`season.json`, `camp.ts`) | built | cut | cut | seated (no walking between places); hands (no menus); session |
| 5 | **13 closed intents** (TRAIN, WORK, SCHEME, ...) with contests and caps | built | cut | cut as a player verb set. A small number of between-bout choices replace it (page 04). | hands: a verb list is a menu; session |
| 6 | **LLM Director** (Claude CLI, Ollama, offline planner; night transaction; cache; budget) | built | cut | **cut at runtime** unless the owner approves (page 04, owner decision 3) | scope; paid-service rule (only ElevenLabs is approved, `DECISIONS.md` 2026-10-02); cold start ≤15 s with no account (plan section 6.6); a standalone Quest APK has no CLI |
| 7 | **Deterministic planner and phrase bank** | built | cut | cut (there are no NPC days to plan) | No camp simulation |
| 8 | **Hollow Board and journal** (morning facts, private Knowings) | built | cut | adapted: one **dais tablet** shows the result facts of the last Games day | comfort (no head-locked UI, plan section 6.7); session |
| 9 | **Listening act** (45 s real-time minigame that earns a Knowing) | built | cut | cut | No camp nights |
| 10 | **Parley** (typed text or three authored cards, only at Knowing moments) | built (W6) | cut | adapted: **three cards become three stones**. No typed text. | hands (no keyboard; voice input is unproven and cut); the three-card form fits the three dais stones |
| 11 | **Tent Trial** (unarmed best-of-three press, brace or feint; decides the entrant) | partial (Water only) | cut. The player is simply the Tide entrant. | future, optional: press, brace and feint map onto ward, blink and Bolt in a short bout | session; scope |
| 12 | **Games entry and rewards**: gold, renown, ally trust, rumour facts, receipts (`season-bridge.json`) | partial (Tiro, weeks 1-2) | partial: the kernel computes gold, renown and the missio or champion result, and **nothing shows or keeps them** | kept: renown and mastery saved; gold only if something spends it | Same `FGamesResult` as TV (`Games.cpp:198-214`) |
| 13 | **Tiro waves** (soldiers, creatures, semifinal, final) | built | kept, seated: enemies attack from the floor, nothing reaches the dais (DF-001) | kept | seated (`DECISIONS.md` 2026-10-03) |
| 14 | **Higher tiers** (Veteranus, Primus, Summa) and **mastery rank-up** | planned (W9); mastery not wired | cut (only the plan's "Ember champion" replay at competence 2.5) | future: the ladder spine | scope (plan section 6.8 cuts tiers II-IV) |
| 15 | **Missio** at 0 HP below the Summa | built | adapted: a loss gives an **instant retry of the same bout with the same seed**. In TV, missio ends that Games day. | kept as a retry, with the reward rule on page 04 | session ("No bout ends the session unsatisfied", plan section 6.4) |
| 16 | **Camp stats into arena fighter** (`campPlayerSnapshot`: vigor, focus and nerve set HP, mana and absorb) | built | cut: a fixed fighter from the pinned stats at default ranks | adapted: ranks rise from Games-day choices, not from camp training | No camp. The arena formulas stay TV's (`stats.csv`). |
| 17 | **Poison, fatigue, hunger** feeding the arena | built | cut | cut | No camp |
| 18 | **Composition screen** (five lines, branches by mastery) | built | adapted: fixed loadout Tide Orb, Lash, Mirror, plus one tier IV branch pick at the first intermission. **The pick is not built yet** (page 02). | adapted: branch picks and line unlocks as dais choices | hands; plan section 6.3 |
| 19 | **Other playable schools** (Fire, Earth, Air as season entrants) | planned (W8) | cut for the player. Fire runs as the AI opponent only. | future: VR builds every school first (`DECISIONS.md` 2026-10-03). Playable once each kernel and its seated defence exist. | scope; `../gameplay/05-schools/`, `../gameplay/06-school-defences.md` |
| 20 | **Relationships**: trust, debt, loyalty, bond of the Four, Strays, fifth tent | built (some states have no consequences yet) | cut | adapted: **trust toward named entrants only**, moved by scene choices and duel outcomes | session. Trust is what the Summa's Breaking needs. |
| 21 | **Quest decision graph** (`QuestNode`, `QuestChoice`, `WhenClause`, effects, checks) | in progress, **uncommitted** | none | **kept as the data model**, with a C++ runtime port held to conformance vectors | Reuse a tested, deterministic format instead of inventing one (page 04) |
| 22 | **Story pack** (15 scenes, camp days 1-42) | in progress, **uncommitted**, not reviewed by the owner | none | adapted: VR uses the **scene format**, and only those beats that happen at the arena or at the Door (opening ford, Summa) | Most of the scenes are camp scenes |
| 23 | **Deaths and Plot objects**, the Vigil's attention, executions | reserved (`death-reservation.json` `enabled: false`) | cut | cut | scope; comfort. No source asks for death in VR. |
| 24 | **Lethal arena bouts** (sine missione) | planned (W9 hooks) | cut | cut unless the owner rules otherwise (owner decision 7) | comfort; consistent with "no death below the Summa" |
| 25 | **Endings**: breaking, champion, betrayed, revolt, martyr (`season.json`) | planned (W12) | cut | adapted: **champion, breaking and betrayed** only. These three are decided in the arena. Revolt and martyr need camp systems VR does not run. | session; seated |
| 26 | **Saves, receipts, replay verification** (versioned, atomic, previous-good slot, byte-identical replay) | built (W7) | partial: one `bout=N` line; settings JSON | adapted: a versioned campaign save, with bout receipts verified by kernel replay | Same determinism argument (the kernel is fixed-step) |
| 27 | **Intro portraits** | TV discarded the A2 portraits (D12). New ones are blocked on A6 and A2b. | none | VR-made opponent card in the chosen VR style | `DECISIONS.md` 2026-10-02 (VR does not inherit the TV art style) |
| 28 | **Voice** | TV notes a VO cap of 10 words per line (`manifest.md`, uncommitted). No VO is built. | cut (plan section 1 cuts voice acting) | future: pre-rendered voice lines, only if the owner extends the ElevenLabs approval to voice (owner decision 4) | Paid-service rule |
| 29 | **Collar clock** as the tier clock | built | adapted: shown as **four runes on the casting wrist's bracer** (plan section 6.3). In first person, the player cannot see their own collar. | adapted | comfort; diegetic HUD |
| 30 | **Async ghost duels**, **PvP** | not planned in TV | cut (plan section 6.8) | future, outside this campaign | scope |

## What this means for VR writers

- Write **inside the Games**. The dais, the arena floor, the grille above, and the intermission are VR's whole
  stage. Anything that happens in the camp reaches the player as a short line or a fact, never as a place to visit.
- **Never invent canon** to cover a cut system. If a VR beat needs a new fact, character or ending, file a change
  request to TV first (README, "The rule for TV material").
- **Keep TV words.** Missio, entrant, Tiro, the Ember Tent. Do not coin VR synonyms.

## Status and gaps

- The "VR today" column is checked against code at `46643e2` (page 02). The "VR future" column is a proposal and
  depends on the owner decisions listed on page 04.
- The TV statuses come from the TV export `mage-arena:docs/campaign/status.md` (uncommitted, 2026-10-07). They were
  spot-checked against the committed `mage-arena-int` data and code named in `sources`. They were not re-run.
- Row 15 is a real semantic divergence: TV ends the Games day on missio, VR retries. Any shared reward logic must
  treat VR retries explicitly (page 04, owner decision 5).

## Open questions

- Is the Tent Trial (row 11) worth a VR form, or is the VR player always the Tide's entrant?
- Do revolt and martyr (row 25) stay TV-only endings? Page 04 assumes they do.
