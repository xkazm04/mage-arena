---
title: v2 Tiro day - the competition slice script
channel: vr
status: proposed
sources: [docs/campaign/05-v2-story-bible.md, docs/campaign/02-vr-run-as-campaign.md, docs/campaign/04-vr-campaign-design.md, docs/campaign/01-story-core.md, docs/PROJECT-PLAN.md, docs/schools/FIRE.md, docs/design-findings/DF-003-calibration-proposal.md, docs/design-findings/DF-004-creatures-cannot-reach-the-seat.md, docs/gameplay/05-schools/water.md, apps/vr/data/vr/strings.json, apps/vr/data/vr/teach.json, apps/vr/data/pinned/docs/design/baseline-fourteen-nights/design/data/arena-tiers.json, apps/vr/Game/Source/MageArenaVR/Kernel/Games.cpp, apps/vr/Game/Source/MageArenaVR/Session/SessionFlow.cpp, apps/vr/Game/Source/MageArenaVR/Session/ArenaSession.h, owner v2 vision log 2026-10-07 (not on GitHub)]
---

# v2 Tiro day - the competition slice script

**Status: proposed. Not built.** This is the 18 Nov slice as a beat-by-beat script: the prologue and the first
chapter ("The Collar Holds") of Cassia's route (`05-v2-story-bible.md`). It runs **8:25-10:05** without retries, inside
the 8-12 min target. Every spoken line is written out with its speaker, its stage position (floor, grille or rail) and
its id. The VO manifest at the end is the ElevenLabs render list. The flags follow the `arena.<tier>.<day>.w<n>.*`
style of `04-vr-campaign-design.md` section 4. Cassia has no voice: her words are the stone labels (5 words or fewer).
Every choice is a palm held 0.40 s over a dais stone (`teach.json` `offerHoldS`), never during a bout; after 20 s
with no stone touched, the default is taken.

## Timing

| # | Beat | Start | Length | Basis |
|---|---|---|---|---|
| 1 | Cold start | 0:00 | 15 s | plan section 6.4 |
| 2 | Prologue: the collaring at the Door | 0:15 | 45 s | static tableau, 7 lines |
| 3 | Teach | 1:00 | 65 s | **measured** 63.3 s (T13, `02-vr-run-as-campaign.md`) |
| 4 | Septima's ritual | 2:05 | 35 s | 4 lines and one gesture |
| 5 | Bout 1, soldiers | 2:40 | 5 s call + 25-40 s | pinned Tiro band (`04` section 8) |
| 6 | Intermission 1: the branch pick | ~3:25 | 25 s | |
| 7 | Bout 2, creatures | ~3:50 | 5 s call + 30-45 s | pinned band |
| 8 | Intermission 2: Brennic walks in; a secret | ~4:35 | 45 s | |
| 9 | Bout 3, Brennic (semifinal) | ~5:20 | 45-80 s | pinned band 45-70; vision target median 45-80 |
| 10 | Missio: spare or finish | ~6:30 | 25 s | |
| 11 | Intermission 3: the final announced; Senna's pact | ~6:55 | 35 s | |
| 12 | Bout 4, the Tiro final | ~7:30 | 45-80 s | pinned band |
| 13 | Aftermath: the first crack | ~8:40 | 50 s | |

Fixed time (beats 1-4, 6, 8, 10, 11, 13 and four 5 s bout calls) is about 360 s. Bouts add 145-245 s at the pinned
bands, so the day runs 8:25-10:05. Each retry adds the bout again. **Simulated** today: all bouts on the calibration
proposal take 105 s (`02-vr-run-as-campaign.md`), below the bands; the combat pack must lengthen them.

## Beat 1 - Cold start (0:00, 15 s)

Dusk on the dais. The existing prompt: "Raise your palm to begin." (`strings.json` `cold.start`). No voice. Crowd
murmur from the stands above the gate.
**Writes:** nothing. **Reads:** `campaign.position` (a saved day offers continue, as `offer.continue` does today).

## Beat 2 - Prologue: the collaring at the Door (0:15, 45 s)

A static tableau on the arena floor, in front of the dais: the great Door with its grille, a file of legionaries, the
Four kneeling in a row, Septima with four open collars. No camera motion. Light changes only. The player watches;
nothing is asked. At the last line the bracers close on the player's own wrists (the tier-clock runes of the HUD,
plan section 6.3).

| Id | Speaker | Position | Line |
|---|---|---|---|
| pro.venno.01 | Venno | grille | "Four mages from the ford. Rome thanks its allies." |
| pro.brennic.01 | Brennic | floor | "They promised us land, Cassia. Not iron." |
| pro.iskar.01 | Iskar | floor | "I kneel to no Vigil." |
| pro.garran.01 | Garran | floor | "Kneel, Iskar. Live. We fight another day." |
| pro.septima.01 | Septima | floor | "Septima collars what Rome fears." |
| ritual.septima.holds | Septima | floor | "The collar holds. The Wardstones drink. The Games begin." |
| chorus.begin | Chorus | grille | "The Games begin!" |

**Writes:** `story.prologue.seen = true`. **Reads:** `story.prologue.seen` (skip on later launches; palm the centre
stone to skip, `04` section 2).

## Beat 3 - Teach (1:00, 65 s)

The existing teach, unchanged (`ETeachStep` Ward, Perfect, Sigil, Blink; prompts `teach.*` in `strings.json`). Ophel
frames it from the rail. The water dummy stays.

| Id | Speaker | Position | Line |
|---|---|---|---|
| teach.ophel.01 | Ophel | rail | "Hands first, Cassia. The arena forgives nothing else." |
| teach.ophel.02 | Ophel | rail | "Enough. Septima is waiting." |

**Writes:** `teach.done = true`. **Reads:** none.

## Beat 4 - Septima's ritual (2:05, 35 s)

Septima walks to the dais rail (front centre). The ritual is the one in `05-v2-story-bible.md` section 3. Gesture:
both wrists offered (both hands forward, palms up, wrists together, 0.40 s). The bracers lock with a low bell. The
bronze mirror rises at the front of the dais and shows the collar, whole.

| Id | Speaker | Position | Line | When |
|---|---|---|---|---|
| ritual.septima.asks | Septima | rail | "Septima asks for the wrists." | on arrival |
| ritual.septima.again | Septima | rail | "Septima asks again. The wrists, entrant." | 10 s without the gesture |
| ritual.septima.takes | Septima | rail | "Septima takes what is not offered." | 20 s without the gesture; the ritual goes on |
| ritual.septima.holds | Septima | rail | "The collar holds. The Wardstones drink. The Games begin." | the bracers lock |
| chorus.begin | Chorus | grille | "The Games begin!" | after the line above |
| ritual.mirror.whole | Septima | rail | "Septima holds the mirror. The collar is whole." | the mirror rises |

**Writes:** `ritual.tiro.1.done = true`, `ritual.tiro.1.offered` (true if the wrists were offered, false if taken).
**Reads:** `collar.cracks` (0 → the "whole" line).

## Beat 5 - Bout 1, soldiers (2:40)

Tiro wave 1: 4 conscripts and 2 slingers (`arena-tiers.json` `tiers[0].waves[0]`). Steel teaches the blink. The
wrist cues for Flow and the clock play as today (`wave.flow`, `wave.clock`).

| Id | Speaker | Position | Line | When |
|---|---|---|---|---|
| b1.venno.01 | Venno | grille | "Senators! The Tiro Games! New collars, new blood!" | bout call |
| b1.venno.02 | Venno | grille | "Rome's own soldiers against the Tide. Wager freely!" | bout call |
| chorus.spill | Chorus | grille | "More! Spill it! More!" | each Tithe event, at most once per 8 s |
| b1.venno.03 | Venno | grille | "The Tide stands. Clear the sand." | bout won |
| chorus.missio | Chorus | grille | "Missio! Missio!" | player at 0 HP (any bout) |
| venno.missio.player | Venno | grille | "The crowd is generous. Again." | after `chorus.missio`; the same bout restarts with the same seed |

**Writes:** `arena.tiro.1.w1.won`, `.attempts`, `.perfects`; `collar.marks`, `collar.tithe` (running);
`arena.missio.total`. **Reads:** none.

## Beat 6 - Intermission 1: the branch pick (25 s)

`betweenWaves` heal and refill as today (`TryAdvanceGames`, `Games.cpp`). The text prompt `intermission` stays. Two
stones light: the left shows a single great orb, the right a scatter of small ones.

| Id | Speaker | Position | Line | When |
|---|---|---|---|---|
| i1.ophel.01 | Ophel | rail | "Your orb will deepen at the fourth rune. Choose." | on entry |
| i1.ophel.02a | Ophel | rail | "The deep, then. No ward can hold it." | left stone |
| i1.ophel.02b | Ophel | rail | "The many, then. Let it rain." | right stone or timeout |

Stones: left **"The deep: Leviathan"**, right **"The many: Rain of Orbs"**. Default: Rain of Orbs, the live preset
(`water.md`, slot 1). The tier IV cast then needs the two-hand mudra seal within 1.5 s (plan section 6.2).
**Writes:** `arena.branch` (`"leviathan"` or `"rain"`). **Reads:** none.

## Beat 7 - Bout 2, creatures (~3:50)

Tiro wave 2: 3 cinder hounds and 1 mire maw. **Ember spit** (DF-004 option A): hounds spit slow, telegraphed magic
embers, and each death throws one last ember toward the pad, so every pad gets perfect practice. Each perfect on an
ember is a Crack mark, and the leak of light from the wrists is first seen here.

| Id | Speaker | Position | Line | When |
|---|---|---|---|---|
| b2.venno.01 | Venno | grille | "Hounds from the pits. Mind the embers, Tide." | bout call |

(`chorus.spill`, `chorus.missio` and `venno.missio.player` as in beat 5.)
**Writes:** `arena.tiro.1.w2.won`, `.attempts`, `.perfects`; `collar.marks`, `collar.tithe`. **Reads:** none.

## Beat 8 - Intermission 2: Brennic walks in; a secret (45 s)

The gate opens under the grille. Brennic walks to his floor mark, about 8 m out, centre, and stands. The crowd roars
without words. This replaces the plan's intro card (`02-vr-run-as-campaign.md`: the PC portrait cannot be used).
Brennic's name appears once on the rail, world-locked.

| Id | Speaker | Position | Line | When |
|---|---|---|---|---|
| i2.venno.01 | Venno | grille | "The Ember Tent sends Brennic the Red!" | the gate opens |
| i2.brennic.01 | Brennic | floor | "The same ford, Cassia. A different sand." | at his mark |
| i2.ophel.01 | Ophel | rail | "I can tell you one thing. Choose." | after Brennic |
| i2.ophel.02a | Ophel | rail | "He plants his staff before the sun falls. Strike then." | left stone |
| i2.ophel.02b | Ophel | rail | "Watch the stones after a great spell. They brighten." | right stone |

Stones: left **"How does he fall?"**, right **"What do the stones do?"**. Timeout: no secret.
**Writes:** `secret.tiro.1` (`"tell"`, `"stones"` or `"none"`); `story.wardstones.suspect = true` on the right stone.
**Reads:** none.

## Beat 9 - Bout 3, Brennic, the Tiro semifinal (~5:20, 45-80 s)

The live arc must spawn the Ember mage (`bFireMages`, `ArenaSession.h:316`; today it spawns the Water proxy,
`Games.cpp:97`). Competence 1. Three HP-gated phases (proposal; the combat pack owns the numbers):

| Phase | HP | Kit | On entry |
|---|---|---|---|
| 1 | 100-66 % | Ember Dart, Flick Flame, Ring of Cinders | taunt p1 |
| 2 | 66-33 % | adds Brand Lance, Pyre Circle, the fire wall | both collars surge (tier clock +1); crowd swells; taunt p2 |
| 3 | below 33 % | **opens with Sunfall** (`FIRE.md:53`, `:70`) | surge again; Brennic at tier IV at least; taunt p3; then the tell |

Sunfall's tell: he plants his staff, his collar flares orange, 0.8 s rooted. A hit inside it cancels the cast
(`FIRE.md:60`): an interrupt, +2 marks. Once loosed, the shadow is black with a red rim: leave the pad. A clean blink
through it is +1 mark. A reflected fireball returned as the killing blow sets the finisher.

| Id | Speaker | Position | Line | When |
|---|---|---|---|---|
| b3.brennic.p1 | Brennic | floor | "We knelt at the same ford, Cassia. Stand, then." | bout start |
| b3.brennic.p2 | Brennic | floor | "The Ember never loved me. The crowd might." | HP crosses 66 % |
| b3.brennic.p3 | Brennic | floor | "Look up, Cassia. Even the sun answers me." | HP crosses 33 %, before Sunfall |
| b3.brennic.int | Brennic | floor | "You read me. Like at the ford." | Sunfall interrupted |

**Writes:** `arena.tiro.1.w3.won`, `.attempts`, `.perfects`; `arena.tiro.1.w3.phase` (highest reached);
`arena.sunfall.cast`, `arena.sunfall.blinked`, `arena.sunfall.interrupted`; `arena.finisher` (`"leviathan"`,
`"reflect"` or `"other"`); `collar.marks`, `collar.tithe`. **Reads:** `arena.branch` (the finisher), `secret.tiro.1`
(none in play; Ophel's tell is the player's knowledge only).

## Beat 10 - Missio: spare or finish (~6:30, 25 s)

Brennic is down on one knee at 0 HP. Below the Summa, nobody dies (`arena-tiers.json` `_about`). The left stone shows
an open palm, the right a fist. A palm held over the left stone spares; a fist held over the right stone finishes:
**finish takes his collar token** (non-lethal; owner question 2 in page 05). The chorus reads the day: if
`collar.tithe` is above `collar.marks`, it chants `chorus.spill` over his plea; otherwise `chorus.missio`.

| Id | Speaker | Position | Line | When |
|---|---|---|---|---|
| m.venno.01 | Venno | grille | "Missio is asked. The sand is yours to answer." | on entry |
| m.brennic.01 | Brennic | floor | "Missio, Cassia. Not for me. For the ford." | after Venno |
| m.brennic.02a | Brennic | floor | "The ford remembers this, Cassia." | spare |
| m.venno.02a | Venno | grille | "Mercy! How very Tide." | after the line above |
| m.brennic.02b | Brennic | floor | "Take it, then. Corvo will thank you." | finish; he unclasps the bronze token |
| m.venno.02b | Venno | grille | "A token taken! Rome adores a lesson." | after the line above, then `chorus.spill` |
| m.venno.02c | Venno | grille | "The crowd grants missio. Rome is patient." | timeout |

Stones: left **"Grant missio"**, right **"Take his token"**.
**Writes:** `missio.brennic` (`"spare"`, `"finish"` or `"crowd"`); spare: marks +3, trust brennic +15; finish: Tithe
+3, renown +5, trust brennic -20, `token.brennic = true`. **Reads:** `collar.tithe`, `collar.marks`.

## Beat 11 - Intermission 3: the final announced; Senna's pact (~6:55, 35 s)

Venno names the winner of the other semifinal, Senna against Lio, fought off stage. Senna, beaten, walks to the left
floor mark with her arm bound. One choice.

| Id | Speaker | Position | Line | When |
|---|---|---|---|---|
| i3.venno.01a | Venno | grille | "Gale beat Stone! The Gale sends Lio, the crowd's darling!" | Air build |
| i3.venno.01b | Venno | grille | "Stone and Gale broke each other. The Ember buys Corvo!" | fallback build |
| i3.senna.01 | Senna | floor | "Win the final, Tide. Stone will remember it." | after Venno |
| i3.senna.02a | Senna | floor | "Then Stone stands with you, while you stand." | left stone |
| i3.senna.02b | Senna | floor | "Then win alone. Stone watches." | right stone or timeout |

Stones: left **"Stone and Tide, then"**, right **"I fight for no tent"**.
**Writes:** `pact.stone` (bool); trust senna +10 and garran +10 on accept; `arena.tiro.1.final` (`"lio"` or `"corvo"`).
**Reads:** build flag (second school present or not).

## Beat 12 - Bout 4, the Tiro final (~7:30, 45-80 s)

Tiro wave 4, competence 1.5. Two phases (66 % and 33 % as for Brennic, proposal). Lio's last phase opens with Sky
Blades (steel: blink). Corvo's opens with Wrath (`FIRE.md:54`). After the win the crowd grants missio itself; the
slice keeps a single spare-or-finish choice, Brennic's.

| Id | Speaker | Position | Line | When |
|---|---|---|---|---|
| b4.lio.p1 | Lio | floor | "Look at them! They came for me, Tide." | bout start |
| b4.lio.p2 | Lio | floor | "Faster! Louder! Give them something to remember!" | HP crosses 66 % |
| b4.lio.p3 | Lio | floor | "Watch the sky blades, little lighthouse." | HP crosses 33 % |
| b4.corvo.p1 | Corvo | floor | "The new Fire mage fell. Now the true one." | bout start (fallback) |
| b4.corvo.p2 | Corvo | floor | "I waited three Games for this sand." | HP crosses 66 % |
| b4.corvo.p3 | Corvo | floor | "Burn with me, Tide. Let the stands see." | HP crosses 33 % |
| b4.venno.win | Venno | grille | "Champion of the Tiro! Rome is entertained." | bout won |

**Writes:** `arena.tiro.1.w4.won`, `.attempts`, `.perfects`; `collar.marks`, `collar.tithe`; `mastery` +1 (a won
final, `arena-tiers.json:85`). **Reads:** `arena.tiro.1.final`.

## Beat 13 - Aftermath: the first crack (~8:40, 50 s)

The floor still glows where the day's magic spilled. A stone tablet rises on the dais with the tallies: perfects,
Crests, time, Crack marks, Tithe, renown (the only place numbers appear). Septima comes to the rail with the mirror.
The first Crack opens (authored, page 05 section 4): a thin light leaks from the collar in the mirror and from both
wrists. The crowd falls silent. Then the spilled light on the floor drains, in threads, into the Wardstones that ring
the arena, and the stones brighten.

| Id | Speaker | Position | Line | When |
|---|---|---|---|---|
| ritual.mirror.marked | Septima | rail | "Septima holds the mirror. The collar is marked." | the crack opens |
| af.brennic.01 | Brennic | floor | "Did you see it, Cassia? The Wardstones drink." | the stones brighten, if `missio.brennic = "spare"` |
| af.ophel.01 | Ophel | rail | "Look at the stones. The Wardstones drink." | the stones brighten, otherwise |

The last word is "drink". Silence, the stones' hum, fade. The rail shows "Veteranus" for 3 s.
**Writes:** `collar.cracks = 1`, `story.wardstones.heard = true`, `campaign.position` (Veteranus). **Reads:**
`missio.brennic`, `collar.marks` (the brightness of the leak), `collar.tithe` (the brightness of the stones).

## The Tiro final: which second school

**Recommendation: Air, with Lio as the Gale entrant.** Reasons, all story:

1. **The day's moral arc closes on its theme.** Brennic tests precision and mercy; Lio, the crowd's darling, tests the
   temptation of Tithe. The slice's last bout is the economy in person, right before the stones are seen to drink.
2. **It keeps Garran for Veteranus.** Garran has Cassia's highest starting trust (25) and is her steadiest Breaking
   partner. Stone's patience fits the middle of the campaign; Senna then serves the slice as the pact-maker, which
   seeds the pact family on day one without a fight.
3. **It avoids two rhyming set pieces.** Stone form shielding the heaviest cast (defence B) would echo Brennic's rooted
   Sunfall. Lio's Sky Blades are steel (blink), after Sunfall's black core (leave): the day teaches the whole threat
   language.
4. **It reads in the seated arc.** Lio's floor-mark blinks stay in the front arc and need no new stagecraft.

The fallback, Corvo, is diegetic: Venno buys the Ember a wildcard (new canon), and if Brennic's token was taken, Corvo
is the man who gains from it. Which school is cheaper to build is for the combat pack, not this page.

## VO manifest (ElevenLabs)

All lines 10 words or fewer, pre-rendered, spatialised from the stage position. 54 lines; `ritual.septima.holds` and
`chorus.begin` are reused in beats 2 and 4 (ritual repetition). The flags each beat writes and reads are listed under
the beat.

| Id | Speaker | Line | Trigger |
|---|---|---|---|
| pro.venno.01 | Venno | Four mages from the ford. Rome thanks its allies. | prologue 1 |
| pro.brennic.01 | Brennic | They promised us land, Cassia. Not iron. | prologue 2 |
| pro.iskar.01 | Iskar | I kneel to no Vigil. | prologue 3 |
| pro.garran.01 | Garran | Kneel, Iskar. Live. We fight another day. | prologue 4 |
| pro.septima.01 | Septima | Septima collars what Rome fears. | prologue 5 |
| ritual.septima.holds | Septima | The collar holds. The Wardstones drink. The Games begin. | prologue end; ritual bracers lock |
| chorus.begin | Chorus | The Games begin! | after `ritual.septima.holds` |
| teach.ophel.01 | Ophel | Hands first, Cassia. The arena forgives nothing else. | teach start |
| teach.ophel.02 | Ophel | Enough. Septima is waiting. | teach end |
| ritual.septima.asks | Septima | Septima asks for the wrists. | ritual start |
| ritual.septima.again | Septima | Septima asks again. The wrists, entrant. | 10 s, no gesture |
| ritual.septima.takes | Septima | Septima takes what is not offered. | 20 s, no gesture |
| ritual.mirror.whole | Septima | Septima holds the mirror. The collar is whole. | mirror, 0 cracks |
| ritual.mirror.marked | Septima | Septima holds the mirror. The collar is marked. | mirror, first crack |
| b1.venno.01 | Venno | Senators! The Tiro Games! New collars, new blood! | bout 1 call |
| b1.venno.02 | Venno | Rome's own soldiers against the Tide. Wager freely! | bout 1 call |
| b1.venno.03 | Venno | The Tide stands. Clear the sand. | bout 1 won |
| chorus.spill | Chorus | More! Spill it! More! | Tithe event (max 1 per 8 s); finish |
| chorus.missio | Chorus | Missio! Missio! | player at 0 HP; Brennic's plea |
| venno.missio.player | Venno | The crowd is generous. Again. | retry after a loss |
| i1.ophel.01 | Ophel | Your orb will deepen at the fourth rune. Choose. | intermission 1 |
| i1.ophel.02a | Ophel | The deep, then. No ward can hold it. | Leviathan picked |
| i1.ophel.02b | Ophel | The many, then. Let it rain. | Rain of Orbs or timeout |
| b2.venno.01 | Venno | Hounds from the pits. Mind the embers, Tide. | bout 2 call |
| i2.venno.01 | Venno | The Ember Tent sends Brennic the Red! | gate opens |
| i2.brennic.01 | Brennic | The same ford, Cassia. A different sand. | Brennic at his mark |
| i2.ophel.01 | Ophel | I can tell you one thing. Choose. | secret offered |
| i2.ophel.02a | Ophel | He plants his staff before the sun falls. Strike then. | secret: tell |
| i2.ophel.02b | Ophel | Watch the stones after a great spell. They brighten. | secret: stones |
| b3.brennic.p1 | Brennic | We knelt at the same ford, Cassia. Stand, then. | bout 3 start |
| b3.brennic.p2 | Brennic | The Ember never loved me. The crowd might. | Brennic HP 66 % |
| b3.brennic.p3 | Brennic | Look up, Cassia. Even the sun answers me. | Brennic HP 33 % |
| b3.brennic.int | Brennic | You read me. Like at the ford. | Sunfall interrupted |
| m.venno.01 | Venno | Missio is asked. The sand is yours to answer. | missio beat |
| m.brennic.01 | Brennic | Missio, Cassia. Not for me. For the ford. | missio plea |
| m.brennic.02a | Brennic | The ford remembers this, Cassia. | spare |
| m.venno.02a | Venno | Mercy! How very Tide. | spare |
| m.brennic.02b | Brennic | Take it, then. Corvo will thank you. | finish |
| m.venno.02b | Venno | A token taken! Rome adores a lesson. | finish |
| m.venno.02c | Venno | The crowd grants missio. Rome is patient. | missio timeout |
| i3.venno.01a | Venno | Gale beat Stone! The Gale sends Lio, the crowd's darling! | final announced (Air) |
| i3.venno.01b | Venno | Stone and Gale broke each other. The Ember buys Corvo! | final announced (fallback) |
| i3.senna.01 | Senna | Win the final, Tide. Stone will remember it. | pact offered |
| i3.senna.02a | Senna | Then Stone stands with you, while you stand. | pact accepted |
| i3.senna.02b | Senna | Then win alone. Stone watches. | pact declined or timeout |
| b4.lio.p1 | Lio | Look at them! They came for me, Tide. | final start |
| b4.lio.p2 | Lio | Faster! Louder! Give them something to remember! | Lio HP 66 % |
| b4.lio.p3 | Lio | Watch the sky blades, little lighthouse. | Lio HP 33 % |
| b4.corvo.p1 | Corvo | The new Fire mage fell. Now the true one. | final start (fallback) |
| b4.corvo.p2 | Corvo | I waited three Games for this sand. | Corvo HP 66 % |
| b4.corvo.p3 | Corvo | Burn with me, Tide. Let the stands see. | Corvo HP 33 % |
| b4.venno.win | Venno | Champion of the Tiro! Rome is entertained. | final won |
| af.brennic.01 | Brennic | Did you see it, Cassia? The Wardstones drink. | aftermath, Brennic spared |
| af.ophel.01 | Ophel | Look at the stones. The Wardstones drink. | aftermath, otherwise |

Voices: 9 (Venno, Septima, Ophel, Brennic, Garran, Iskar, Senna, Lio, Corvo) plus a crowd chorus. Each build ships 50
lines: the Air build skips the four Corvo-path lines, the fallback build the four Lio-path lines.

## Status and gaps

- **None of this is built.** No scene phase, flag store, stone choice outside the settings, voice playback, phase
  breaks, mudra detector or both-wrists pose exists (`02-vr-run-as-campaign.md`; `BACKLOG.md` mudra row).
- **The live arc spawns the Water proxy** in the duels (`Games.cpp:97`; `ArenaSession.h:316`). Beats 9 and 12 need
  the Fire mage on, and beat 12 a second school or the Corvo fallback.
- **Ember spit (beat 7) is DF-004 option A**, approved in the vision log, not built.
- **Bout lengths are not met.** The proposal run is 105 s for all bouts; the script needs 145-245 s.
- **Page 04 budgets 2 scenes of 45 s per Games day.** This slice has five staged beats (prologue, ritual, walk-in,
  missio, aftermath); the timing table above holds them inside 10 min, but page 04's rule needs updating.
- **Card-sized units** for Grok: the scene phase and stone choice; the flag store; the VO player and caption rail;
  HP-gated phases on the mage AI; the both-wrists pose; the aftermath tablet and mirror; ember spit.

## Open questions

1. Air (Lio) or Earth (Senna) for the Tiro final? This page recommends Air.
2. Does a lost bout reset the day's Crack marks and Tithe, or keep what was earned before the loss?
3. Should the final also offer spare or finish, or stay with the crowd's missio as written?
4. Is the "both wrists" pose safe against "both palms raised" (start over and resume) on real hand tracking?
5. Is `chorus.spill` at most once per 8 s enough to make Tithe felt without drowning the threat audio?
