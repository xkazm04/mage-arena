---
title: Shared story core (VR reading)
channel: vr
status: reference
verified-against: mage-arena-vr 46643e2; mage-arena-int b1efd46; mage-arena a7964ad (2026-10-07)
sources: [docs/DECISIONS.md, docs/PROJECT-PLAN.md, apps/vr/data/PINNED.json, apps/vr/data/pinned/docs/design/baseline-fourteen-nights/design/data/arena-tiers.json, apps/vr/data/pinned/docs/design/baseline-fourteen-nights/design/data/schools.json, apps/vr/data/pinned/docs/design/baseline-fourteen-nights/design/data/enemies.json, apps/vr/Game/Source/MageArenaVR/Kernel/Games.cpp, apps/vr/Game/Source/MageArenaVR/Session/SessionFlow.cpp, apps/vr/data/vr/strings.json, apps/vr/art/model-sheets/prompts.json, mage-arena-int:docs/MAGE-ARENA-PLAN.md, mage-arena-int:docs/OWNER-NOTES.md, mage-arena-int:docs/design/reconciled/data/characters.json, mage-arena-int:docs/design/reconciled/data/facts.json, mage-arena-int:docs/design/reconciled/data/schools.json, mage-arena-int:docs/design/reconciled/data/relationships.json, mage-arena-int:docs/design/reconciled/data/season.json, mage-arena-int:docs/design/reconciled/data/season-bridge.json, mage-arena-int:docs/design/reconciled/data/arena/arena-tiers.json, mage-arena-int:story/dialogue/scenes/01-opening.json (uncommitted), mage-arena:docs/campaign/premise-and-setting.md (uncommitted)]
---

# Shared story core (VR reading)

The TV/PC channel owns the world of Mage Arena. In the Roman era, elemental mages served a legion. After a battle at
a ford, the legion betrayed them: it collared its own allies and shut them in a warded camp. There they live in four
tents, one per school, and fight in the arena Games for gold and standing. This page keeps only the part of that
world the VR path touches now or will touch soon, and cites the TV source of every item. **Verdict: VR shares the story
core with TV.** The evidence is the shared data pin, the shared kernel vocabulary, the shared player identity and the
plan's explicit "same world, same names". What VR shares is the arena-facing subset. VR runs no camp, cast or scene
data, and today no proper noun reaches the player's eyes.

## Verdict and evidence

| # | Evidence | Where | What it shows |
|---|---|---|---|
| 1 | "Lore and names: Tide Tent, Ember Tent, Hastatus conscript, Funditor, Cinder hound, Mire maw, missio, the collar clock. Same world, same names." | `docs/PROJECT-PLAN.md` section 3, table "What the VR slice shares" | A design decision to share the world |
| 2 | "Three channels of one game: TV/desktop, VR, PC. A mechanic solved in one channel must be reusable by the others." | `docs/DECISIONS.md` 2026-10-02 | The owner frames VR as a channel of one game, not a separate game |
| 3 | VR runs the Tiro Games from `arena-tiers.json`, pinned from `mage-arena` `68a4d68`. That file's `_about` carries the lore rules ("Nobody dies below the Summa (missio at 0 HP)") | `apps/vr/data/PINNED.json`; pinned `arena-tiers.json` | The Games structure is literally the same data |
| 4 | The VR kernel names the player actor `"Cassia"` | `Kernel/Games.cpp:178`, `Session/SessionFlow.cpp:296` (ported from the TS `games.ts`) | Same identity as TV's only playable character (`season-bridge.json` `playableCharacter: "cassia"`, `playableSchool: "water"`) |
| 5 | Bout results are `"missio"` or `"champion"`, with Tiro gold and renown | `Kernel/Games.cpp:207-212` | Same result vocabulary as TV's `GamesResult` |
| 6 | The Fire opponent is labelled `"Tiro entrant · Ember mage"` | `Kernel/Games.cpp:97` | The VR duel is a Tiro entrant from the Ember Tent |
| 7 | The model sheets show the Fire mage "of the Roman era in a wool tunic and cloak ... and a prisoner's collar" | `apps/vr/art/model-sheets/prompts.json` (A04) | The collared-mage premise is in the VR art brief |

Counter-evidence and limits:

- **Art is not shared.** The owner ruled on 2026-10-02 that VR does not inherit Tessera & Lime (`DECISIONS.md`, "Art
  comes last"). This is a look, not a story difference.
- **The player sees no proper noun today.** `apps/vr/data/vr/strings.json` has 24 strings, all of them instructions
  ("Raise your palm to begin.", "A moment to breathe. ..."). "Cassia" and "Ember mage" are kernel labels only. They
  are not on screen.
- **No story data is pinned.** `PINNED.json` lists nine combat files and no cast, facts or scene files.
- **VR names no Fire mage.** The plan says "the Fire mage of the Ember Tent" (section 6.4). No VR source maps that
  role to a TV cast id (see "Characters").

Consequence: TV is the reference owner of the story core. VR consumes it by reference, and pins a committed revision
when it needs story data at runtime. New canon goes back as a change request (README, "The rule for TV material").

## World facts VR relies on

| Fact | VR use | TV source |
|---|---|---|
| The legion collared its own allies after the ford (secret, held by the Four) | Backstory only. VR does not show it yet. | `facts.json` `K-ford-betrayal`; `story/dialogue/scenes/01-opening.json` (uncommitted) |
| Mages wear a ward collar. In camp it holds magic silent. In the arena the "collar clock" unlocks a spell tier every 15 s and resets each bout. | The tier clock (four bracer runes, plan section 6.3). See `../gameplay/03-tier-clock-and-flow.md` | `arena-tiers.json` `_about` ("the collar's tier clock resets each wave"); `MAGE-ARENA-PLAN.md` section a |
| The Ember Tent won the last Games (public) | Motivates the Fire mage as the favourite and "the Ember champion" challenge | `facts.json` `K-ember-won-last-games` |
| Nobody dies below the Summa: a fighter at 0 HP gets missio (the crowd's mercy) | A loss is missio, followed by a retry (page 02) | `arena-tiers.json` `_about`; `MAGE-ARENA-PLAN.md` section d ("the Roman crowd grants missio") |
| The Games are run by the Lanista for senators who bet on them | Not shown yet | `characters.json` `venno` |
| The Wardstones drink spilled arena magic (secret) | Not used. It is the hinge of the Breaking ending. | `facts.json` `K-wardstones-drink` |
| Roman culture is the baseline (camp, Games, the legion as non-magical enemy waves). The mages and the arena have a raw, magical identity of their own. | Enemy roster: Roman soldiers and creatures. The battle-mage look. | `MAGE-ARENA-PLAN.md` decisions D14, D15 |
| Time does not loop. One season, with persistence of trust, renown, mastery and knowledge. | VR has no season (page 03) | `season.json` `persistence`; plan D2 |

## The Games

The arena event the VR run takes place in. Tiro is the tier VR plays. The data is identical in the VR pin and in the
TV reconciled data, apart from the duration bands noted under "Status and gaps".

| Tier | Requires mastery | Wave 1 | Wave 2 | Wave 3 | Wave 4 | Gold (by waves cleared) | Renown | VR |
|---|---|---|---|---|---|---|---|---|
| Tiro Games | 1 | 4 conscripts + 2 slingers | 3 cinder hounds + 1 mire maw | semifinal: `entrant_of_another_tent`, competence 1 | final: `winner_of_other_semifinal`, competence 1.5 | 20 / 40 / 60 / 100 | 5 / 10 / 15 / 25 | played |
| Veteranus Games | 2 | shieldmen, conscripts, netter | thornback, hush moths | semifinal, competence 2 | final, competence 2.5 | 30 / 60 / 90 / 150 | 6 / 12 / 18 / 30 | not played; competence 2.5 is the plan's "Ember champion" level |
| Primus Games | 3 | mixed | creatures | semifinal, competence 3 | `champion_of_another_tent`, competence 3.5 | 40 / 80 / 120 / 200 | 8 / 15 / 22 / 35 | not played |
| The Summa | 4 | `best_entrant_by_standing`, competence 4 | `other_finalist`, competence 4: the final, or the Breaking | - | - | 0 / 0 | 20 / 50 | not played |

Sources: pinned `arena-tiers.json` and `mage-arena-int:docs/design/reconciled/data/arena/arena-tiers.json`.
Rank-up rule (data): "Mastery +1: win a final, or reach a final twice". Summa rule: if trust(player, finalist) ≥ 60
and both hold `K-wardstones-drink`, the final can become the Breaking, a cooperative duet. Attempted with trust
< 60, it becomes the Betrayed ending. Enemy and wave mechanics are in `../gameplay/07-enemies-and-ai.md`.

## Tents and factions

| Faction | School | Members (TV cast ids) | Temperament | VR relevance |
|---|---|---|---|---|
| the Tide Tent | Water | cassia (main), ophel (elder), nysa (rival) | "patient, intelligent, wins before the fight" | The player's tent (the player is a Water mage, `Cassia`) |
| the Ember Tent | Fire | brennic (main), sadruba (elder), corvo (rival) | "hard-headed, aggressive, stronger under tension" | The duel opponent's tent. Fire is implemented in the VR kernel (`docs/schools/FIRE.md`, CR-002). |
| the Stone Tent | Earth | garran (main), ruadh (elder), senna (rival) | "calm, protective, slow to move and hard to move" | Future opponent and school (`../gameplay/05-schools/`) |
| the Gale Tent | Air | iskar (main), kesh (elder), lio (rival) | "restless, free, contemptuous of rules" | Future opponent and school |
| Strays | none (no tent) | fenna, quill | Hungry, distrusted | None in VR |
| The Vigil | Salt (not a school) | septima (warden) | Stops violence and locks the collars | Possible voice at the start of a Games day (page 04) |
| Rome | none | venno (the Lanista) | Runs the Games for the senators | Possible announcer (page 04) |

Sources: tent names and temperaments from the pinned `schools.json` (`tentName`, `temperament`). Members from
`characters.json`. The Strays, Vigil and Rome rows also come from `characters.json`.

## Characters VR shows or could show

"Shows" means present in the VR run today, even if unnamed. "Could show" means a natural fit for the VR stage (the
dais, the arena floor, the grille) that page 04 uses.

| Character | TV id | Role in TV | VR today | VR could show | TV source |
|---|---|---|---|---|---|
| The player: a Water mage | `cassia` ("Cassia of the Lighthouse") | Main of the Tide Tent; the only playable character in TV's current chapter | Shows: the player's own hands. Named only in the kernel label `"Cassia"`. | First-person protagonist. Her voice register ("precise, courteous, conditional sentences") sets the tone of choice labels. | `characters.json`, `season-bridge.json` |
| The Fire mage of the Ember Tent | not mapped. Candidates: `brennic` (main, "Brennic the Red") or `corvo` (rival, "was the Ember Tent's next entrant until the new Fire mage arrived") | The Ember Tent's entrant is chosen by the Tent Trial and by Elder Sadruba ("by blood and Pit results") | Shows: Tiro duel opponent (kernel label "Ember mage"). The live arc still spawns the Water proxy (page 02). | Named entrant with an intro card and a short voiced line | `characters.json`; `relationships.json` (brennic→cassia trust 15, cassia→brennic 15) |
| "The Ember champion" | not in TV data | none (the public fact names a winning tent, not a person) | Planned replay challenge at competence 2.5 (plan section 6.4). Not built. | The last Games' champion, as an exhibition bout | `facts.json` `K-ember-won-last-games` |
| Elder Ophel | `ophel` | Tide elder: "dry, scholarly, footnotes"; trust toward cassia 20 | No | Briefs the player before a Games day | `characters.json`, `relationships.json` |
| Lanista Aulus Venno | `venno` | Runs the Games; "genial showman"; "never enters the camp; he speaks through the grille of the Door" | No | Announces bouts and payouts from above the arena | `characters.json` |
| Warden Septima | `septima` | The Vigil's warden: "ritual, third person" | No | Speaks at the collar ritual that opens a campaign night | `characters.json`; `scenes/01-opening.json` (uncommitted) |
| Other tents' entrants | `garran`, `iskar`, `senna`, `lio`, `nysa` | Mains and rivals | No (the Earth and Air kernels are not built) | Semifinal and final opponents when their schools exist | `characters.json` |
| Soldiers and creatures | enemy ids `conscript` ("Hastatus conscript"), `slinger` ("Funditor"), `cinder_hound`, `mire_maw` | Tiro waves 1 and 2 | Shows: bouts 1 and 2 | Same | pinned `enemies.json` |
| The crowd | none | Grants missio (plan section d) | No (no crowd audio wired into the session yet; an `arena-crowd-loop` cue exists from A05) | Roar at the finisher (plan section 6.4) | `MAGE-ARENA-PLAN.md` section d |

## Key terms for VR writers

| Term | Meaning (TV canon) | Use in VR text |
|---|---|---|
| Tiro | The lowest Games tier (Latin: recruit) | "the Tiro Games" |
| Entrant | The one mage a tent sends to the Games | "the Ember entrant" |
| Missio | Mercy for a fighter at 0 HP. Below the Summa, no fight is lethal. | A loss. Never "you died". |
| Champion | Winning the final of a Games. In TV it is also the name of a season ending. | Only for a won final or for the challenge figure |
| Collar clock | The tier clock that the collar runs in the arena | VR shows it on a bracer (page 03) |
| Sine missione | A bout without mercy, decreed rarely and only from high tiers. Reserved, not built. | Not used in VR |

## Status and gaps

- **Reference only.** Nothing on this page is VR runtime data. Story text never comes from TV at runtime today.
- **The Fire mage has no identity.** No source names the Ember entrant the VR player meets. This is owner decision 2
  on page 04.
- **Duration bands drift.** The pinned Tiro `targetDurationS` (VR pin `68a4d68`: 25-40, 30-45, 45-70) differ from
  `mage-arena-int` `b1efd46` (25-45, 35-60, 45-75). That is a combat-data pin question for
  `../gameplay/10-data-reference.md`, not a story change. It is noted here because the same file carries the Games
  lore.
- **Loop wording is left in the shared data.** `arena-tiers.json` `_about` still says "the last day of every loop"
  and "persists across resets", although TV removed loops (plan D2). VR text must not use loop language.
- **Uncommitted canon.** The story pack (`story/dialogue/`) introduces characters and endings that are not in
  `characters.json` or `season.json` (for example the ending `collared`, and the ids tamar and pell). VR must not
  rely on them until TV commits and reconciles them.

## Open questions

- Should the Fire mage be Brennic (one of the Four, betrayed with Cassia: a personal duel) or Corvo (a sneering rival,
  no shared past)? Page 04 recommends Brennic. This is owner decision 2.
- Does TV want "the Ember champion" as a named character? If so, it is new canon, and a change request to TV.
- Does a VR player ever learn `K-ford-betrayal` on screen, or does the VR campaign stay inside the Games?
