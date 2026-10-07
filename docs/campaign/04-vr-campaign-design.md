---
title: VR campaign design (post-competition)
channel: vr
status: proposed
verified-against: mage-arena-vr 46643e2; mage-arena-int b1efd46; mage-arena a7964ad (2026-10-07)
sources: [docs/DECISIONS.md, docs/PROJECT-PLAN.md, docs/design/SCHOOL-DEFENCES.md, apps/vr/data/vr/strings.json, apps/vr/data/vr/teach.json, apps/vr/data/README.md, apps/vr/Game/Source/MageArenaVR/Session/ArenaSession.h, apps/vr/Game/Source/MageArenaVR/Kernel/Games.h, apps/vr/Game/Source/MageArenaVR/Hands/MageSettings.h, apps/vr/data/pinned/docs/design/baseline-fourteen-nights/design/data/arena-tiers.json, apps/vr/data/pinned/docs/design/baseline-fourteen-nights/design/data/stats.csv, mage-arena-int:packages/core/src/quest.ts (uncommitted), mage-arena-int:packages/core/src/quest.md (uncommitted), mage-arena-int:packages/core/src/quest-lint.ts (uncommitted), mage-arena-int:story/dialogue/schema.md (uncommitted), mage-arena-int:story/dialogue/manifest.md (uncommitted), mage-arena-int:story/dialogue/scenes/01-opening.json (uncommitted), mage-arena-int:story/dialogue/scenes/11-summa-climax.json (uncommitted), mage-arena-int:docs/design/reconciled/data/characters.json, mage-arena-int:docs/design/reconciled/data/relationships.json, mage-arena-int:docs/design/reconciled/data/season.json, mage-arena-int:docs/design/reconciled/data/season-bridge.json, mage-arena-int:docs/design/reconciled/data/facts.json, mage-arena-int:docs/design/reconciled/data/arena/arena-tiers.json, mage-arena-int:docs/MAGE-ARENA-PLAN.md]
---

# VR campaign design (post-competition)

**Status: proposed. None of this is built or approved.** The VR campaign proposed here is a **ladder of Games days**.
Each launch plays one Games day of 6 to 10 minutes, seated on the dais. A short eve scene comes first. The four bouts
of a tier follow, with an optional choice at each intermission. A short aftermath scene closes the day. Mastery
earned in the arena climbs the TV tier ladder: Tiro, Veteranus, Primus, then the Summa. At the Summa, trust built in
earlier scenes decides between the champion, breaking and betrayed endings. Story arrives diegetically: voices from
the floor and the grille, words on the dais rail. Every choice is a palm held over one of three stones. The
narrative data reuses the TV quest decision-graph model (`QuestNode`, `QuestChoice`, `WhenClause`, effects, checks)
and its scene format unchanged, with a VR staging block that the TV runtime ignores. The arena writes flags that
scenes read, so the player's own play is the main "check". The design has no LLM at runtime, no camp, no calendar
and no menus. Every number stays deterministic and replayable.

## 1. Structure

| Unit | What it is | Length target | Source of the shape |
|---|---|---|---|
| **Prologue** (one time) | The collaring at the Door as a static tableau (adapted from TV `dlg_opening_ford`), then the teach | ≤ 3 min (scene ≤ 60 s, plus the teach, **measured** at 63 s in T13) | TV scene 01 (uncommitted); VR teach (page 02) |
| **Games day** (one per launch) | Eve scene → wave 1 → intermission → wave 2 → intermission → semifinal → intermission → final → aftermath scene | 6-10 min | `arena-tiers.json`: four waves per tier |
| **Tier** | Tiro, Veteranus, Primus: repeat Games days until mastery rises | 1-3 Games days each | `rankUpOn`: "Mastery +1: win a final, or reach a final twice" |
| **Summa** | Two bouts. The second is the final, or the Breaking duet, or betrayal. Then the ending card. | ≤ 8 min | Summa tier: trust ≥ 60 plus `K-wardstones-drink` |
| **Whole campaign** | Prologue + 3 tiers + Summa | 4 Games days minimum, about 6-8 typical | Matches TV's six Games per season without a calendar |

Opponents follow the tier data literally: the semifinal is an entrant of another tent, and the final is the winner
of the other semifinal. The Tide's three rival tents are Ember, Stone and Gale, so a canon-consistent Games day
needs **two different non-Water schools**. **Recommendation: the campaign does not ship until Fire and one of Earth
or Air run in the VR kernel** (owner decision 9). Named entrants come from the TV cast: brennic (Ember), garran
(Stone), iskar (Gale) as mains, and corvo, senna, lio as rivals.

Between the bouts, the intermission stays as it is today (`betweenWaves` heal and refill; "A moment to breathe").
It gains at most **one** optional choice, which never blocks: if no stone is touched in 20 s, the run continues.

## 2. How story is delivered in VR

| Rule | Decision | Why |
|---|---|---|
| Stage | Speakers stand on the arena floor (entrants), at the grille above the gate (Venno), or at the dais rail (Ophel, Septima). All are inside the front ±70° arc, or ±40° in narrow view. | Nothing comes from behind (plan section 6.1). Narrow-FOV mode. |
| Text | Lines appear world-locked on the dais rail, never head-locked. One line at a time. At most 10 words a line. | Comfort (plan section 6.7). TV notes the same 10-word VO cap (`manifest.md`, uncommitted). |
| Voice | Lines are **pre-rendered** at authoring time, through pof's ElevenLabs provider, then shipped as audio assets and spatialised from the speaker. No network calls at runtime. | ElevenLabs is the only approved paid service (`DECISIONS.md` 2026-10-02). The approval names music and effects, so voice needs owner decision 4. |
| No LLM at runtime | Scenes are authored graphs. No Director, no generated lines, no planner. | The Director is cut (page 03, row 6). A standalone APK has no CLI. Cold start ≤ 15 s with no account. Owner decision 3. |
| Scene length | ≤ 45 s per scene, ≤ 2 scenes per Games day (eve and aftermath), and one optional intermission choice | Session budget. Story never outweighs combat. |
| Skipping | Palm the centre stone while no choice is shown to skip to the next choice, or to the end of the scene | Reuses the existing stone gesture (`settings.offer.narrow`: "Palm the centre stone"). Both palms stay reserved for "start over" (`offer.continue`). |
| Facts | After each Games day, the result fact and the rumour fact (TV wording from `season-bridge.json` `rumours`) appear on a stone tablet on the dais | Replaces the Hollow Board (page 03, row 8) |

## 3. Choices by gesture

- **Three stones, three choices at most.** Every node shows two or three visible choices: the TV linter already
  requires at least two per non-terminal node (`quest-lint.ts`). Each choice is engraved on the left, centre or right
  stone. These are the same stones as the comfort settings.
- **Selection** is one palm held over a stone for 0.40 s. This is `teach.json` `offerHoldS`, the hold already used
  for offers. The stone glows during the hold. Lowering the palm early cancels.
- **No choices during a bout.** The ward hand and the casting hand are busy, and a palm raised over a stone must
  never be read as a ward. The scene runner is a separate session phase, like `offer` and `intermission` today.
- **Labels** have at most 5 words and are written in the player's voice register ("precise, courteous, conditional
  sentences", `characters.json` cassia).
- **`cost`** on a choice (TV: gold or stocks) is allowed only for renown or gold, and only if the owner keeps gold
  (owner decision 5).

## 4. Checks: the arena is the dice

TV checks roll `floor(seededUnit(seed, day, actor, action) × 20) + 1 + stat ≥ dc` (`quest.md`). VR keeps that rule
for any shared scene, and adds a VR-native check that needs no new schema: **arena flags read by `when` clauses**.

| Arena flag (written by the VR session after each bout) | Example `when` |
|---|---|
| `arena.<tier>.<day>.w<n>.won` (bool), `.attempts` (int), `.perfects` (int) | `{ "flag": "arena.tiro.1.w3.perfects", "op": "gte", "value": 3 }` |
| `arena.finisher` (`"leviathan"`, `"reflect"`, `"other"`) | Brennic's line differs if you returned his own fireball |
| `arena.missio.total` (int) | Venno mocks or praises the crowd's patience |
| `arena.sunfall.blinked` (bool) | Elder Ophel notes that you read the unstoppable spell |

Dice checks (`QuestCheck`) stay supported for scenes shared with TV. Their stat comes from the player's sheet:
cassia vigor 1, focus 3, nerve 1, guile 2 (`characters.json`), plus any VR rank gains. `day` is the Games-day index
and `actor` is `"cassia"`. **VR-authored scenes should prefer arena flags over dice.** A choice that depends on
how you fought is the VR identity.

## 5. Progression and unlocks

| Track | Rule | Persisted | Source |
|---|---|---|---|
| Mastery (tier) | +1 when you win a final, or reach a final twice. Mastery 4 opens the Summa. | yes | `arena-tiers.json` `rankUpOn`, `requiresMastery` |
| Renown (standing) | Tier renown table by waves cleared. It decides the Summa's `best_entrant_by_standing`. | yes | `arena-tiers.json` `renown` |
| Gold | **Recommended: not shown and not saved** until something spends it. TV gear (talisman, garment) is not built. | no | `stats.csv`; page 03, row 12 |
| Trust toward entrants | `effects.trust` from scenes (TV key semantics: a bare id means `player>npc`), plus fixed deltas for duel outcomes. It starts from TV's `relationships.json` (cassia→brennic 15, →garran 25, →iskar 10). | yes | `quest.ts` `applyQuestEffects`; `relationships.json` |
| Knowings | `effects.knowing` from scenes. Only `K-wardstones-drink` matters, for the Breaking. | yes | `facts.json`, Summa rule |
| Tier IV branch | Picked at the eve of each Games day: Leviathan Orb or Rain of Orbs on Tide Orb. Kept until changed. | yes | Plan section 6.3 |
| Spell lines | Mend and Mire (cut from the slice) unlock as alternatives to Lash or Mirror at Veteranus and Primus. Data from `spells-water.csv`. | yes | Plan section 6.3 cut list; `../gameplay/05-schools/` |
| Defences | Water's split hands (A) and planted staff (E) are available from the start, because the seated balance needs them (`SCHOOL-DEFENCES.md`). New defences arrive only with new playable schools. | n/a | `DECISIONS.md` 2026-10-03 |
| Stat ranks | At most one rank per Games day, chosen at the aftermath (for example "Train with Ophel: focus +1"), within the `stats.csv` rank range. The arena formulas stay TV's. | yes | `season-bridge.json` (camp → arena formulas) |
| Other schools as the player | After the campaign ships for Water. Each needs its kernel and its seated defence first. | future | `DECISIONS.md` 2026-10-03 |

## 6. Persistence and save

One file, `Saved/MageArena/campaign.json`. It is written atomically (write a temporary file, then rename), and the
previous good copy is kept as `campaign.prev.json`. This mirrors the TV W7 guarantees (versioned, atomic,
previous-good recovery). `settings.json` stays separate. `bout.txt` is replaced by `position`. No account and no
cloud (owner decision 11).

| Field | Meaning |
|---|---|
| `version` | Schema version. A newer save is refused, and an older one is migrated or refused, never guessed. |
| `storyPin` | The TV commit the scenes were authored against. A mismatch logs a warning and still loads. |
| `campaignSeed` | Chosen once at the prologue |
| `position` | `{ tier, gamesDay, phase, wave }`. Resume always restarts the current wave from its start, as today. |
| `mastery`, `renown`, `ranks`, `branch`, `lines` | Progression (section 5) |
| `trust`, `knowings`, `flags` | The quest `flags` bag plus the TV-shaped effects |
| `receipts[]` | One per finished bout: `{ tier, gamesDay, wave, seed, attempt, outcome, ticks, stateHash }`. A receipt applies once. |
| `choices[]` | `{ questId, nodeId, choiceIndex }` in order. With the receipts, this rebuilds the save. |
| `ending` | Set at the Summa |

## 7. Determinism

- The kernel is fixed-step and deterministic (`FArenaSession::GetStateHash`; conformance vectors are byte-identical
  to the TS kernel). The bout seed is derived from `(campaignSeed, tier, gamesDay, wave)` and **stays the same across
  retries**, as the live session already does (`Start(BoutSeed)`).
- Campaign state is a **pure fold** over the initial state, the receipts and the choice log. A save can be verified
  by replaying that fold. Bout outcomes can be verified by replaying recorded inputs against `stateHash`, as TV does.
  Recording inputs also makes the plan's future async ghost duels cheap.
- The scene runtime must match TV's `quest.ts` exactly: `evalWhen` defaults (a missing flag is 0, `''` or false,
  and `eq`/`ne` compare numeric strings as numbers), the "later wins" rule for flag merges, the order of variants,
  and the check roll. **Recommendation: port the runtime to C++ and hold it to conformance vectors generated from
  the TS implementation**, the same method as the combat kernel (owner decision 10). Doing so needs the TV quest
  runtime to be committed first.

## 8. Session length and comfort

| Segment | Budget |
|---|---|
| Eve scene | ≤ 45 s |
| Four bouts | 2.4-3.9 min by the pinned Tiro target bands (25-40, 30-45, 45-70, 45-80 s). Higher tiers have no bands in the data yet. |
| Three intermissions | ≤ 20 s each |
| Aftermath scene | ≤ 45 s |
| **Games day** | **6-10 min**, within the plan's 10-minute session |

Comfort rules carry over unchanged (`../gameplay/09-pause-comfort-accessibility.md`): seated only, no camera motion
in scenes, world-locked text, colour doubled by shape and sound, Gentle and left-hand modes, and pause on focus loss,
headset removal or 1.5 s of lost tracking. A scene pauses and resumes like a bout.

## 9. What comes from TV unchanged, and what is VR's

| From TV, unchanged (pinned, never edited) | VR-specific (owned here) |
|---|---|
| World facts (`facts.json`), cast ids, names, bios and voice registers (`characters.json`) | Staging: where each speaker stands, timing, the stone mapping |
| Tents, schools and temperaments (`schools.json`) | The Games-day segmentation and the session phases |
| The tier ladder, waves, competence, payouts, renown and rank-up rule (`arena-tiers.json`) | Arena flags written by the VR session |
| Result vocabulary (missio, champion) and "no death below the Summa" | The retry rule and its reward treatment |
| Ending names and the Summa's Breaking condition (`season.json`, `arena-tiers.json`) | Which endings VR offers (champion, breaking, betrayed) |
| The quest graph schema and runtime semantics (`quest.ts`, `schema.md`), once committed | VR scene content, authored in the TV schema |
| Starting relationships (`relationships.json`) | The save file format |

A VR scene that adds canon (a new fact, character, ending or flag meaning shared with TV) goes to TV as a change
request **before** it ships. Purely staged or VR-only lines do not.

## 10. Data schema sketch

The VR campaign file lives in `apps/vr/data/vr/campaign/campaign.json`. Scenes are TV-schema quests, each with an
extra `vr` block. TV's schema says the runtime ignores unknown quest meta keys (`schema.md`, "Authoring /
story-graph join keys; ignored by runtime if unknown").

```json
{
  "version": 1,
  "storyPin": { "source": "mage-arena-int", "commit": "<committed sha>",
                "files": ["docs/design/reconciled/data/characters.json", "docs/design/reconciled/data/facts.json",
                          "docs/design/reconciled/data/relationships.json"] },
  "player": { "character": "cassia", "school": "water", "tent": "tide" },
  "prologue": { "scene": "vr_prologue_door", "then": "teach" },
  "ladder": [
    { "tier": "tiro", "games": [
      { "id": "tiro-1", "eve": "vr_eve_tiro_1",
        "opponents": { "semifinal": "brennic", "final": "garran" },
        "intermissions": ["vr_int_branch", null, null],
        "aftermath": "vr_after_tiro" } ] }
  ],
  "summa": { "semifinal": "best_entrant_by_standing", "final": "other_finalist",
             "breaking": { "trustAtLeast": 60, "knowing": "K-wardstones-drink" } },
  "endings": ["champion", "breaking", "betrayed"]
}
```

```json
{
  "id": "vr_eve_tiro_1", "title": "Tiro Eve", "giver": "ophel", "kind": "vr-eve", "start": "n0",
  "nodes": [
    { "id": "n0", "text": "Ophel: 'The Ember won the last Games.'",
      "lines": [ { "speaker": "ophel", "text": "The Ember won the last Games." },
                 { "speaker": "ophel", "text": "Their entrant fights you first." } ],
      "choices": [
        { "label": "Then I read him first", "to": "t_read", "effects": { "flags": { "eve.stance": "read" } } },
        { "label": "Tell me about Brennic", "to": "t_ask" },
        { "label": "Leviathan, if it comes", "when": { "flag": "branch", "op": "eq", "value": "leviathan" },
          "to": "t_read" } ] } ],
  "terminals": [
    { "id": "t_read", "text": "Ophel nods.", "effects": { "trust": { "ophel": 1 } } },
    { "id": "t_ask", "text": "Ophel: 'Short, loud, and climbing.'", "effects": { "knowing": [] } } ],
  "vr": { "stage": "rail", "speakers": { "ophel": "right" },
          "voice": { "n0": ["vo/ophel/eve_tiro_1_a.ogg", "vo/ophel/eve_tiro_1_b.ogg"] } }
}
```

The lines in the example are placeholders that show the shape. They are not canon. The TV linter (`lintQuestPack`)
must pass on every VR scene file unchanged.

## 11. Rollout order

1. The owner decides the items below.
2. TV commits the quest runtime and story schema. VR pins them, with the cast, facts and relationships, through the
   hash-checked pin.
3. VR ports the scene runtime and its conformance vectors. It adds the `scene` session phase and the stone choice.
4. The arena writes flags and receipts, and the campaign save replaces `bout.txt`.
5. The first content: the prologue and Tiro (eve, branch intermission, aftermath), against Ember and the second
   school.
6. Veteranus, Primus and the Summa, with change requests to TV for any new canon.

## Owner decisions needed

| # | Decision | Recommendation |
|---|---|---|
| 1 | Competition slice: three bouts (the plan table) or four Tiro waves (the data and the live chain)? Who is the duel opponent? | **Three bouts**, ending on the Tiro **semifinal** against the Ember entrant. Also make the live arc spawn the Fire mage (it spawns the Water proxy today, page 02). A Tiro final needs a second non-Water school, which does not exist yet. |
| 2 | Who is "the Fire mage of the Ember Tent", and who is "the Ember champion"? | The entrant is **Brennic** (one of the Four, collared with Cassia: a personal duel; TV trust 15 both ways). The champion stays an unnamed figure for "the Ember won the last Games" until TV names one, which would be a change request. |
| 3 | LLM Director (or any LLM) at runtime in VR? | **No.** Authored graphs only. |
| 4 | Extend the ElevenLabs approval from music and effects to pre-rendered voice lines (post-competition)? | **Yes**, pre-rendered at authoring time, at most 10 words a line, no runtime calls |
| 5 | Retry against reward: VR retries a lost bout. TV ends the Games day on missio. | Retries are unlimited and progression continues. **Renown is paid only for waves won on the first attempt.** Gold is not shown. Missio counts become a flag. |
| 6 | Story ownership: TV owns canon; VR authors scenes in the TV schema and sends new canon back as change requests. Also, TV must commit `quest.ts` and `story/` before VR pins them. | **Yes** |
| 7 | Deaths or sine missione bouts in VR? | **None.** Missio below the Summa, as in the TV data. |
| 8 | Endings offered in VR | **Champion, breaking and betrayed.** Revolt and martyr stay TV-only. |
| 9 | The campaign launch gate | Ship only when **Fire plus Earth or Air** run in the VR kernel |
| 10 | Scene runtime: port TV `quest.ts` to C++ with conformance vectors, or build a VR-only runner? | **Port**, held to TS-generated vectors |
| 11 | Saves: local only, or an account or cloud? | **Local only**, no login (plan section 6.6) |
| 12 | Campaign length | Mastery-gated ladder: **at least 4 Games days plus the prologue**, about 6-8 typical |

## Status and gaps

- **Proposed only.** No code, data or art exists for any section on this page.
- **It depends on uncommitted TV work.** The quest runtime and story schema are uncommitted on `mage-arena-int`.
  This design must not be built on them until they are committed and pinned.
- **Higher tiers have no target durations** in the pinned `arena-tiers.json`, so the Games-day length above
  Tiro is unverified.
- **The seated balance is open.** Tiro itself is not winnable from the seat on the live overlay yet (DF-003,
  DF-004). The campaign sits on top of that work and does not replace it.

## Open questions

- Does the prologue's ford scene work as a static tableau, or does it need camera motion? It must not have any
  (comfort). The proposal assumes a static tableau at the Door.
- Should the Tent Trial come back as a short VR bout (page 03, row 11), or is the player always the entrant?
- How many intermission choices per Games day before the pacing suffers? The proposal says one; it is a guess, not a
  measurement.
