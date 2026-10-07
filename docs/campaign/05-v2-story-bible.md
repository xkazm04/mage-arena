---
title: v2 story bible - Breaking the collar
channel: vr
status: proposed
sources: [docs/DECISIONS.md, docs/PROJECT-PLAN.md, docs/campaign/01-story-core.md, docs/campaign/02-vr-run-as-campaign.md, docs/campaign/03-tv-divergence.md, docs/campaign/04-vr-campaign-design.md, docs/schools/FIRE.md, docs/design/SCHOOL-DEFENCES.md, docs/gameplay/05-schools/water.md, docs/gameplay/05-schools/earth-and-air.md, docs/design-findings/DF-003-calibration-proposal.md, docs/design-findings/DF-004-creatures-cannot-reach-the-seat.md, apps/vr/tasks/BACKLOG.md, apps/vr/data/vr/strings.json, apps/vr/data/vr/teach.json, apps/vr/data/pinned/docs/design/baseline-fourteen-nights/design/data/arena-tiers.json, apps/vr/data/pinned/docs/design/baseline-fourteen-nights/design/data/spells-water.csv, apps/vr/Game/Source/MageArenaVR/Kernel/Games.cpp, apps/vr/Game/Source/MageArenaVR/Kernel/Water.cpp, apps/vr/Game/Source/MageArenaVR/Kernel/MageAI.h, apps/vr/Game/Source/MageArenaVR/Session/ArenaSession.h, owner v2 vision log 2026-10-07 (not on GitHub)]
---

# v2 story bible - Breaking the collar

**Status: proposed. Nothing on this page is built, and most of it is new canon that needs a change request (CR) to TV.**
The v2 game is one spine: **Breaking the collar**. Cassia enters the Tiro Games an obedient entrant. Over four
authored Games days (Tiro, Veteranus, Primus, the Summa) she learns that the Wardstones drink the magic spilled in the
arena (`K-wardstones-drink`, existing canon, `01-story-core.md`). At the Summa she breaks the collar, wins as Rome's
champion, or is betrayed. Two counters carry the moral layer: **Cracks** (precision: earned by reading the fight) and
**Tithe** (spectacle: spilled magic that the crowd loves and the stones drink). The voice is mythic and operatic: a
ritual that repeats each Games day, a crowd that answers like a chorus, and no line over 10 words. Canon used here is
only what `01-story-core.md` summarises. Everything else is marked **new canon: needs a CR** and collected at the end.

## 1. The spine across four tiers (Cassia's route)

Each tier is one hand-authored story Games day: a ritual, four bouts, a named rival with a set-piece boss phase, and
the intermission choices. Optional exhibition bouts sit between the story days (the ladder) and earn mastery and Cracks.
The tier data stays TV's: semifinal against an entrant of another tent, final against the winner of the other
semifinal, mastery +1 for a won final (`arena-tiers.json:85`).

| Tier | Chapter | Cassia learns | Crossing (semifinal) | Final | Mirror at the ritual | Set piece |
|---|---|---|---|---|---|---|
| Tiro | The Collar Holds | The stones brighten when magic spills. She *hears* "the Wardstones drink". | Brennic (Ember) | Lio (Gale); fallback Corvo | whole | Brennic's Sunfall |
| Veteranus | The Tithe | The crowd pays for spilled magic; Venno courts her. The Knowing is earned at an intermission secret. | Garran (Stone) | Corvo (Ember) | marked | Garran's stone form under the heaviest cast |
| Primus | The Mirror | Septima sees the cracks each dawn and says nothing. Why? | Iskar (Gale) | Senna (Stone) | cracked | Iskar's air form in a storm |
| Summa | The Breaking | Tier V exists; the collar was withholding it. | best entrant by standing (`arena-tiers.json:243`) | the other finalist, or the Breaking | breaking | the Breaking duet, one tier V spell per school |

The arc of the protagonist is obedience, doubt, knowledge, choice. The mirror clause of Septima's ritual (section 3)
marks each step. **New canon: needs a CR** (the chapter structure, which rival stands at which tier).

### Four routes, one ladder

The Four (Cassia, Brennic, Garran, Iskar) were collared together at the ford (`K-ford-betrayal`, `01-story-core.md`).
v2 ships four routes, one per member of the Four, on **one ladder template**. Cassia's route is the slice and ships first.

- **The template.** Each story Games day has four entrants, one per tent (canon: "the one mage a tent sends",
  `01-story-core.md`, key terms). One semifinal is a **crossing**: the route's protagonist against another member of
  the Four. The other semifinal is between the rivals of the two remaining tents. The protagonist's final is against
  that semifinal's winner.
- **The crossings are a round robin.** The Four pair off three times, once per tier, so every route meets each other
  member of the Four exactly once before the Summa.

| Tier | Crossing A | Crossing B |
|---|---|---|
| Tiro | Cassia - Brennic | Garran - Iskar |
| Veteranus | Cassia - Garran | Brennic - Iskar |
| Primus | Cassia - Iskar | Brennic - Garran |

- **Where the routes cross.** A crossing bout is authored once and played from both sides: Cassia's Tiro semifinal is
  Brennic's Tiro semifinal. Its scenes, taunts and the missio choice are shared data, with the speaker and the stage
  swapped. Each route is its own telling (in each, the protagonist wins), not one shared timeline. In Cassia's telling,
  Stone and Gale send their rivals (Senna, Lio) to the Tiro; crossing B belongs to Garran's and Iskar's routes.
- **How the four sides read the same ladder.**

| Route | School | Temperament (canon, `schools.json`) | What the route is about | Pull toward |
|---|---|---|---|---|
| Cassia | Water | patient, intelligent | learning the truth before acting | Precision |
| Brennic | Fire | aggressive, stronger under tension | an outsider in his own tent; the crowd is the only family offered | Spectacle |
| Garran | Earth | protective, slow to move | carrying the others; whether to risk them | Precision, at a cost |
| Iskar | Air | restless, contemptuous of rules | breaking is easy for him; trust is hard | the Breaking alone |

**New canon: needs a CR** (four playable routes; TV's current chapter has Cassia only, `season-bridge.json`).

## 2. The cast

Combatants carry a **signature kit as data on the existing mage AI** (`Kernel/MageAI.h`): a signature spell, a
defence, a tell, three voiced phase-break taunts (10 words or fewer) and one AI style tweak. A boss rival has 2-3
HP-gated phases (proposal: breaks at 66 % and 33 % HP). Each break surges both collars (tier clock +1) and swells the
crowd. The last phase opens with the signature. Earth and Air spell names are placeholders: their spell lists are
not in the pin (`apps/vr/data/pinned/.../data/` holds `spells-water.csv` only). Personalities beyond the tent
temperament and the `01-story-core.md` rows are **new canon: needs a CR**.

### Brennic the Red (Ember, one of the Four) - Tiro semifinal

- **Role:** Cassia's mirror. Same ford, same collar, opposite answer.
- **Motive:** He arrived in the Ember Tent as the new Fire mage and took Corvo's place (canon reading of Corvo's bio,
  `01-story-core.md`). The tent tolerates him. The crowd loves him. He fights for the only family on offer.
- **Arc:** At Tiro, Cassia beats him and holds his missio. Spared, he begins to doubt the crowd and can reach the
  Summa as her Breaking partner. Finished, he loses his place to Corvo and becomes the voice of Spectacle.
- **Signature:** **Sunfall** (`fire_sunfall`, tier IV, unblockable meteor, radius 3.5 m, `docs/schools/FIRE.md:53`).
  Threat language: black core, red rim: leave the pad.
- **Defence:** fire wall (school defence C, `SCHOOL-DEFENCES.md`).
- **Tell:** he plants his staff and his collar flares orange for the 0.8 s rooted cast. A staff strike in that
  window cancels the cast (`FIRE.md:60`). For the player, any hit in the tell window interrupts and earns a Crack.
- **AI tweak:** Sunfall first from tier IV (`FIRE.md:70`); aggression rises as his HP falls.
- **Taunts:** opening "We knelt at the same ford, Cassia. Stand, then." (9). At 66 %: "The Ember never loved me. The
  crowd might." (8). At 33 %, before Sunfall: "Look up, Cassia. Even the sun answers me." (8).

### Lio (Gale rival) - Tiro final (recommended)

- **Role:** Spectacle in person. The crowd's darling, who plays to the stands.
- **Motive:** Gale's contempt for rules, turned into showmanship. If the crowd loves him, the collar feels light.
- **Arc:** Tiro final; Veteranus other semifinal (loses to Corvo); a Summa candidate by standing. He never learns.
- **Signature:** placeholder **Sky Blades**: a fan of steel wind blades across the front pads. Steel: blink.
- **Defence:** air form (school defence D). **Tell:** he spins and the air around him goes silver.
- **AI tweak:** blinks between floor marks every few seconds; casts more when the crowd swells.
- **Taunts:** "Look at them! They came for me, Tide." (8). "Faster! Louder! Give them something to remember!" (7).
  "Watch the sky blades, little lighthouse." (6).

### Corvo (Ember rival) - Tiro fallback, Veteranus final

- **Role:** the displaced heir. **Motive:** "the Ember Tent's next entrant until the new Fire mage arrived"
  (`01-story-core.md`); he wants Brennic's place and Rome's notice.
- **Arc:** Venno's wildcard at Tiro if the second school slips; the Ember entrant from Veteranus on if Brennic was
  finished. He tithes freely and grows stronger as the stones do.
- **Signature:** **Wrath** (`fire_wrath`, tier IV beam, 10 m for 2 s, `FIRE.md:54`). **Tell:** he kneels and
  breathes in; his mouth glows. **AI tweak:** prefers area rows (Ring, Pyre).
- **Taunts:** "The new Fire mage fell. Now the true one." (9). "I waited three Games for this sand." (7). "Burn with me,
  Tide. Let the stands see." (8).

### Garran (Stone, one of the Four) - Veteranus crossing

- **Role:** the protector. Cassia's highest starting trust (25, `relationships.json` via page 04).
- **Motive:** keep the Four alive; spill as little as he can. **Arc:** the steadiest ally if pacts hold; the hardest
  betrayal if the Stone pact is broken.
- **Signature:** placeholder **Mountain's Weight**, the heaviest Earth cast, channelled inside stone form (defence B).
  **Tell:** both fists drawn to the chest; dust lifts. **AI tweak:** rarely blinks; builds footing.
- **Taunts:** "I will not spill more than I must." (8). "Stone waits. Stone has always waited." (6). "Then I carry it.
  Brace, Cassia." (6).

### Senna (Stone rival) - Tiro other semifinal, Primus final

- **Role:** the pact-maker. Beaten by Lio at Tiro, she offers Cassia the first Tent pact.
- **Motive:** Stone's honour; she hates the Lanista's spectacle. **Arc:** an honest ally of convenience; she keeps a
  pact exactly as long as it is kept.
- **Signature:** placeholder **Quarry Hail**, a slung volley of stones (steel: blink). **Tell:** she sets her heel and
  a ring of gravel rises. **AI tweak:** answers every Tithe with a heavier volley.
- **Taunts:** "Stone does not bend for Lanistas." (6). "Every stone you crack, I set again." (7). "Fall slowly, Tide.
  The quarry is patient." (7).

### Iskar (Gale, one of the Four) - Primus crossing

- **Role:** the rebel without patience. **Motive:** freedom now, alone if need be.
- **Arc:** the first to guess the stones' secret; the likeliest partner for a Breaking attempted too early.
- **Signature:** placeholder **Eye of the Storm**: air form, then a ring of blades closing on the pads. **Tell:** the
  wind drops to silence for one breath. **AI tweak:** baits wards with feints.
- **Taunts:** "A collar is only a rule. I break rules." (9). "Catch the wind, Cassia. You cannot." (6). "Sky above,
  sand below, and you between." (7).

### Nysa (Tide rival) - exhibitions

- **Role:** the rival inside Cassia's own tent; she wants the entrant's place. She fights Cassia only in the ladder's
  exhibition bouts (one entrant per tent forbids a Games meeting).
- **Signature:** **Drown Coil** (`lash,4`, `spells-water.csv`). **Tell:** she draws the circle slowly, showing off.
- **Taunts:** "The Tide sent the wrong daughter." (6). "I know every line you draw." (6). "Mirror me, then. See who
  blinks first." (7).

### Non-combatants

| Character | Stage | Role | Motive and arc |
|---|---|---|---|
| Elder Ophel (`ophel`) | rail | Mentor; "dry, scholarly". Gives the intermission secrets. | Has suspected the stones for years. Tells only what is asked. |
| Warden Septima (`septima`) | rail | The Vigil; "ritual, third person". Opens each Games day with the collar ritual. | Reads the cracks every dawn and stays silent. At the Summa her silence ends, for or against, by Tithe. |
| Lanista Aulus Venno (`venno`) | grille | The showman; "speaks through the grille of the Door". | Pays renown for spectacle. He is the Tithe's salesman, not its master. |
| The crowd | grille (the stands above the gate) | The chorus | Grants missio, roars at spilled magic, falls silent at a Crack. |

## 3. The collar ritual (Septima)

Every story Games day opens with the same ritual, after the teach on the first day and after the eve on later days.
It takes 30-40 s and has no fail state.

1. Septima stands at the dais rail with the key. "Septima asks for the wrists." (5)
2. **Both wrists offered:** both hands forward, palms up, wrists together, held 0.40 s (`teach.json` `offerHoldS`).
   This is a new pose. It must not be read as "both palms raised" (start over, resume: `strings.json`
   `offer.continue`, `../gameplay/09-pause-comfort-accessibility.md`): the palm normal points up, not forward.
3. The bracers lock with a low bell. Septima: "The collar holds. The Wardstones drink. The Games begin." (9) The
   chorus answers: "The Games begin!" (3)
4. **The dais mirror.** A bronze mirror rises from the dais floor, front centre, below eye level, and shows the
   player's own collar (the player cannot see it in first person, `03-tv-divergence.md` row 29). Septima reads it in
   one line that changes with the Crack count: "Septima holds the mirror. The collar is whole." (8) / "... The collar
   is marked." (8) / "Septima sees the cracks. Septima says nothing." (7) / "Septima sees the breaking. Septima
   chooses." (6). Each crack leaks a thin light that the mirror shows.

The repeated sentence "The Wardstones drink" is heard first at the Door (prologue) as a rule of the Vigil. Its meaning
turns at the Tiro aftermath, when Cassia sees spilled magic sink into the stones. **New canon: needs a CR** (the
ritual, its words, the mirror, the wrist cuffs as part of the collar).

## 4. Cracks and Tithe: the moral layer

| Counter | Earned by (proposal) | Felt now | Changes later |
|---|---|---|---|
| **Crack marks** | perfect absorb +1; a reflection perfected back +1 (each return in a rally, up to 3); a clean blink through a black core (blink inside its last 0.40 s) +1; an interrupt inside a tell window +2; spare at missio +3 | light leaks from the collar and wrists; **the crowd hushes** for one beat | every 10 marks open one **Crack**, persistent, shown in the dais mirror. Cracks gate the Breaking. |
| **Tithe** | each cast of a tier III-IV area row +2; a cast that hits nothing +1; a Crest spent on an area row +2; finish at missio +3 | the Wardstones pulse, **the crowd chants**, Venno praises; renown +1 per 2 Tithe (proposal) | strengthens the jailers: the Vigil's wardstones at the Summa; at Tithe 40 or more, the Breaking is not offered |

- **How scenes read it.** Scenes read `collar.marks`, `collar.cracks` and `collar.tithe` as flags (page 06 lists them per beat). Venno's
  lines warm with Tithe; Ophel's cool. Septima's mirror line reads only Cracks.
- **How the chorus reads it.** Three refrains only: "Missio! Missio!" (mercy), "More! Spill it! More!" (Tithe) and
  "The Games begin!" (ritual). A Crack gets silence, never a cheer. The player learns that the crowd loves what feeds
  the stones.
- **How endings read it.** The ending card shows the mirror: the cracks and the stones' glow side by side.
- **The first Crack is authored.** The Tiro aftermath always opens the first Crack, so the slice ends on it; the marks
  earned that day set how bright it leaks. The 10-mark rule applies from the next day. This guarantees the slice's
  last beat for every player (owner question 3).
- **Tier V.** The collar withholds tier V. The tier clock still stops at IV (`combat.json` `tierClock`). The Breaking
  unleashes one tier V spell per school (Water's placeholder: **Deluge**).

**New canon: needs a CR** (Cracks, Tithe, tier V, the Vigil feeding on Tithe).

## 5. The three endings

The tier data says: at the Summa, if trust(player, finalist) ≥ 60 and **both** hold `K-wardstones-drink`, the final
can become the Breaking; attempted with trust below 60 it becomes the Betrayed ending (`arena-tiers.json:257`). v2
adds the Cracks and Tithe gates. At the Summa's last intermission the stones offer, in order:

| Condition at the last intermission | Stones offered | Ending |
|---|---|---|
| Cracks below 4, or Tithe 40 or more, or the player lacks the Knowing | none (the final is played) | **Champion** on a won final |
| Gates met | left "Break it with [finalist]", centre "Break it alone", right "Fight the final" | see below |
| ...with the finalist, trust ≥ 60 and the finalist holds the Knowing | | **Breaking**: the duet, both tier V spells at the Wardstones |
| ...with the finalist, trust below 60 or the finalist lacks the Knowing | | **Betrayed**: the finalist turns at the Vigil's word |
| ...alone, or "Fight the final" | | **Champion**: the collar cracks and holds; Rome crowns its champion |

The vision log's "attempted without allies" is read here as "Break it alone" (owner question 4). Missio still applies
at the Summa below the final's last phase (owner question 2).

## 6. The three choice families

All choices are made on the three dais stones, a palm held 0.40 s, never during a bout (`04-vr-campaign-design.md`
section 3). If no stone is touched in 20 s, the default is taken and the day goes on.

| Family | Where | Options | Changes now | Changes later |
|---|---|---|---|---|
| **Spare or finish** | after a won bout against a named rival, at their missio | left stone, open palm: **grant missio**. Right stone, fist: **take the collar token**. Timeout: the crowd grants missio. | spare: Crack marks +3, trust +15. Finish: Tithe +3, renown +5, trust -20. | A finished rival loses standing (Brennic loses his place to Corvo). Tokens are shown on the aftermath tablet; at the Summa, each token is a voice the Vigil can use against you (Betrayed lines). |
| **Secrets** | intermissions | two secrets, choose one; Ophel tells it | the chosen one is spoken (a tell, a weakness, a fact) | knowledge flags; the stones line seeds `story.wardstones.suspect`, which brings the Knowing one tier earlier |
| **Pacts** | intermissions, with another tent's entrant or rival | accept or decline | trust +10 with that tent's members | the tent's elder gives a secret at the next story day; a pact broken (finish against that tent) costs trust -30 |

**Finish is non-lethal below the Summa.** Nobody dies below the Summa (`arena-tiers.json` `_about`; `01-story-core.md`).
v2's proposal: "finish" means **refusing the plea and taking the rival's collar token**, a bronze tag that marks the
loser as unfit to enter. Alternatives for the owner: breaking the rival's staff, or turning the thumb to the crowd so
that Venno, not the player, takes the token. No death, no blood, no killing blow (owner question 2).

## 7. Voice rules (mythic, operatic)

- **10 words or fewer per line**, every speaker, every language. Choice labels 5 words or fewer
  (`04-vr-campaign-design.md` section 3).
- **Ritual repetition.** The ritual is the same words every Games day. Only the mirror clause changes. Venno's opening
  call keeps one shape: "Senators! The [tier] Games! [hook]!"
- **The chorus.** Three refrains, no others. It answers lines, it never speaks alone over a telegraph.
- **Third person for Septima** (canon register "ritual, third person"). Ophel is dry and short. Venno is warm and loud.
  Cassia has **no voice**: the player is her, and her register ("precise, courteous, conditional") lives in the stone
  labels.
- **Stage.** Entrants speak from the floor, Venno and the chorus from the grille and the stands above the gate, Ophel
  and Septima from the dais rail. All are in the front arc, ±70° (±40° in narrow view). No line comes from behind.
- **Text.** One world-locked caption at a time, on the rail. No head-locked UI.
- **Timing.** Taunts play only at phase breaks, during the 1.5 s phase-break surge, when no new telegraph starts
  (proposal). Never during a perfect window.
- **Words.** Keep TV words: missio, entrant, Tiro, Summa, the Ember Tent. Latin only where canon already uses it.
- **Production.** Pre-rendered with ElevenLabs at authoring time, no runtime calls (vision log, Wave 5).

## 8. New canon: needs a CR

| # | New canon | Where used | Depends on |
|---|---|---|---|
| 1 | Cracks (marks, threshold, persistence, the mirror) | sections 3-5 | `K-wardstones-drink` |
| 2 | Tithe, and the Vigil's jailers growing stronger on it | sections 4-5 | `K-wardstones-drink` |
| 3 | Tier V, withheld by the collar; one per school at the Breaking | section 4 | the tier clock |
| 4 | Septima's collar ritual: words, both-wrists gesture, wrist cuffs, the dais mirror | section 3 | `septima` |
| 5 | Four playable routes on one ladder; the round-robin crossings | section 1 | `K-ford-betrayal` |
| 6 | Rival placement per tier (Lio, Corvo, Senna, Garran, Iskar, Nysa) | section 2 | `characters.json` |
| 7 | Rival personalities, signatures, tells, taunts beyond the canon bios | section 2 | `characters.json` |
| 8 | Earth and Air signature names (Mountain's Weight, Quarry Hail, Sky Blades, Eye of the Storm) and Water's tier V (Deluge) | sections 2, 4 | the Earth and Air spell lists |
| 9 | "Finish" as taking the collar token (non-lethal) | section 6 | "nobody dies below the Summa" |
| 10 | Corvo as Venno's wildcard, a second Ember entrant at one Games | section 2 | one entrant per tent |
| 11 | Brennic as "the new Fire mage" who displaced Corvo | section 2 | Corvo's bio |
| 12 | The Cracks and Tithe gates added to the Summa rule; "Break it alone" as Champion | section 5 | `arena-tiers.json:257` |
| 13 | Pacts between tents as a choice with lasting trust | section 6 | `relationships.json` |

## Status and gaps

- **Proposed only.** No scene runtime, no flag store and no voice exist. The live arc shows 24 instruction strings and
  no proper noun (`strings.json`; `02-vr-run-as-campaign.md`).
- **The Ember mage is not in the live arc.** `bFireMages` defaults to false (`ArenaSession.h:316`), so the duels spawn
  `"Tiro entrant · Water proxy"` (`Games.cpp:97`). Brennic's whole beat depends on that switch.
- **Sunfall has never been seen seated** (0 of 40 duels, vision log; `BACKLOG.md` DF-003). The phase surges in section
  2 are the proposed mechanism; they are not simulated.
- **Mirror II is Ripple in the live preset** (`Water.cpp:348`), not Reflection. Reflection rallies need the
  Reflection preset first.
- **No Earth or Air kernel**, and no spell list for them in the pin. Their signatures are names only.
- **Mark values and thresholds are authored guesses**, not measured. The Tithe sources need a census pass, like DF-003.

## Open questions

1. Do you approve **Lio (Air)** as the Tiro final, with Corvo as the fallback (page 06 gives the story reasons)?
2. What does "finish" mean below the Summa: the collar token (recommended), the staff, or the crowd's thumb? And is
   the Summa also non-lethal in VR (page 04 decision 7 says yes)?
3. Is the first Crack authored (always shown at the Tiro aftermath), or must it be earned?
4. Is "the Breaking attempted without allies" the "Break it alone" stone (Champion), distinct from Betrayed?
5. Are Cassia's stone labels enough voice for her, or does she get spoken lines?
6. Does the crowd chorus count as a fourth stage position (the stands), or stay "grille"?
