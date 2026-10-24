---
title: C2 - slice story bible, script and cue sheet
channel: vr
status: draft for the owner's ruling
verified-against: mage-arena-vr 3cd7cdd (2026-10-21)
sources: [docs/DECISIONS.md, docs/campaign/01-story-core.md, docs/campaign/02-vr-run-as-campaign.md, docs/campaign/04-vr-campaign-design.md, docs/design/V2-ROADMAP.md, docs/design-findings/DF-010-tiro-day-length.md, apps/vr/data/vr/strings.json, apps/vr/data/vr/day.json, apps/vr/data/vr/teach.json, apps/vr/data/vr/combat.vr.json, apps/vr/Game/Source/MageArenaVR/Session/SessionFlow.cpp, apps/vr/Game/Source/MageArenaVR/Session/DayFlow.cpp, apps/vr/Game/Source/MageArenaVR/Session/ArenaFlags.h, apps/vr/Game/Source/MageArenaVR/Tests/DayTests.cpp, apps/vr/tasks/T27-voice-and-chorus.md, apps/vr/tasks/T28-intermission-scenes.md, apps/vr/tasks/T21-tiro-day-chain.md]
---

# C2 - the slice story bible, script and cue sheet

This is **C2**, roadmap lane B row 12 (`docs/design/V2-ROADMAP.md:79`): "Story bible v0 and the slice script: ritual
words, Brennic's taunts, crowd chorus, intermission secrets, the Wardstones line (10 words or fewer each)". It is a
**Claude + owner** deliverable. Written on Lynx 2026-10-21, where no Unreal runs; nothing here was built or measured in
engine.

**Why it exists now and not later.** `DF-010` was ruled **E + B** on 2026-10-21 (`docs/DECISIONS.md`, newest entry): the
owner keeps the 7-10 min day and buys the missing minutes with content instead of restating the number. The pinned Tiro
bands cap four bouts at **145-235 s of fighting**, and that cap is the owner's own signed ruling, so every minute above
it has to come from the beats this document authors. `T27` and `T28` are both blocked on C2, which puts C2 on the
critical path for the 18 Nov submission. **Section 4 is therefore the load-bearing section**: it is the arithmetic that
shows the day reaching 7 min, and it is a design constraint on the text, not a report about it.

**Status: D1-D4 approved as recommended, owner 2026-10-24** (`docs/DECISIONS.md`, "2026-10-24 - C2 rulings D1-D4
approved as recommended"). Section 11 records each ruling; D5-D7 stay open there and never blocked work. **No voice line
is generated yet:** D4 approves the voice and its 14 cues, but its audio production is deferred by the owner's audio
ruling of the same day (`docs/DECISIONS.md`, "2026-10-24 - Music and audio deferred; the visual side comes first").

## 1. Scope: this is one Tiro day, not the campaign

`docs/campaign/04-vr-campaign-design.md` designs the **post-competition** campaign: a ladder of tiers, trust, knowings,
the Summa and the Breaking. It also depends on TV work that is **not committed** (`quest.ts`, `schema.md`,
`story/dialogue/`, that page's own "Status and gaps"). The slice cannot wait for it and must not block on it.

So C2 authors the **one Tiro day the slice ships**, in a self-contained form the VR build reads on its own:

| In scope for C2 | Out of scope, and where it lives |
|---|---|
| The prologue tableau line(s) | The TV `dlg_opening_ford` scene graph |
| The collar ritual (Warden Septima) | A campaign night, the camp, the four tents as places |
| Four bout intros (the Lanista at the grille) | A named eve scene with a dialogue tree |
| Three intermission scenes, one optional choice each | Trust, knowings, gold, stat ranks, the quest runtime |
| Brennic's and Lio's phase-break taunts | Veteranus, Primus, the Summa, the Breaking, the endings |
| The crowd chorus beats | Music loops (`T40`), SFX (A05) |
| The aftermath tablet and the closing card | `season-bridge.json` rumours, the Hollow Board |
| The `_story.*` marker convention and the cue sheet | Any ElevenLabs call (owner's go, D4) |

**Forward compatibility is a requirement, not a hope.** The slice's scene file (section 8) uses `when` clauses over
arena flags, stone-indexed choices and speaker ids that are all **already** the campaign design's vocabulary
(`04-vr-campaign-design.md` sections 3 and 4). When TV commits the quest schema, each scene here becomes one TV-schema
quest node with a `vr` block; nothing authored here has to be thrown away or re-written. C2 adds **no new runtime
concept** - it fills beats the day already has.

## 2. The canon the slice uses

All of it is already established. C2 invents no world facts; where it needed one, that is decision **D5**.

| Fact the player meets | Where it comes from |
|---|---|
| Mages served a legion; after a battle at a ford the legion collared its own allies | `01-story-core.md`, TV `facts.json` `K-ford-betrayal` |
| The collar holds magic silent in camp; in the arena its clock unlocks a tier every 15 s | pinned `arena-tiers.json` `_about`; the bracer runes |
| Nobody dies below the Summa: a fighter at 0 HP is granted **missio** | pinned `arena-tiers.json` `_about` |
| The Ember Tent won the last Games | TV `facts.json` `K-ember-won-last-games` |
| The Games are run by the Lanista for senators who bet on them | TV `characters.json` `venno` |
| The player is **Cassia**, a Water mage of the Tide Tent | `Kernel/Games.cpp:178`; TV `season-bridge.json` |
| **Brennic** of the Ember fights the semifinal; his signature is **Sunfall** | `docs/DECISIONS.md` 2026-10-07; `combat.vr.json` `rivals.brennic` |
| **Lio** of the Gale fights the final; her signature is **Tempest Lance** | `combat.vr.json` `rivals.tiroFinal` (`school: air`, `rival: lio`) |
| The second school is Air, not Earth | `docs/DECISIONS.md` 2026-10-07; `docs/DECISIONS.md:323-326` |

### The cast, and the register each one speaks in

The registers are TV's, from `characters.json` as quoted in `01-story-core.md`. They are binding on the text: they are
what makes four speakers distinguishable with no faces and no subtitles naming them.

| Speaker | Stage position | Register | What they are for in the slice |
|---|---|---|---|
| **Warden Septima** (the Vigil) | dais rail, left | **ritual, third person.** Never says "I". Addresses the player as "Tide". | Opens the day. Her ritual is the only beat that explains the collar. |
| **Lanista Aulus Venno** | the grille above the gate | **genial showman.** Plays to the crowd, not to the player. Never cruel, always selling. | Announces each bout and the result. Carries the crowd's mood. |
| **Elder Ophel** (Tide) | dais rail, right | **dry, scholarly, footnotes.** Understated. Gives the player information, never encouragement. | Speaks in the intermissions. He is the one who tells you how to fight. |
| **Brennic the Red** (Ember) | arena floor, centre | **hard-headed, aggressive, stronger under tension.** Short sentences. | The semifinal. Two phase-break taunts. |
| **Lio** (Gale) | arena floor, centre | **restless, free, contemptuous of rules.** Mocking, light. | The final. Two phase-break taunts. |
| **Cassia** (the player) | first person, unvoiced | **precise, courteous, conditional sentences.** | She never speaks aloud. Her register governs **every stone label**, which are her thoughts. |

**Cassia is deliberately silent.** She has no voiced line in the slice. This is not a budget cut: the player's hands are
the player's, and a voice put in her mouth competes with them. Her only utterance is the label she chooses on a stone.

### Two rules the text must obey, both already enforced by machine

1. **Every `story.*` line is 10 words or fewer.** `Tests/DayTests.cpp:862-865` parses each line and fails above ten.
   The cap is also TV's VO cap. Every line in section 5 carries its word count.
2. **Every stone label is 5 words or fewer**, in Cassia's register (`04-vr-campaign-design.md` section 3). `T28`
   re-states it. Every label in section 6 carries its count.

## 3. What the slice never says

A negative list is as much of a bible as a positive one, and three of these are live mistakes the current placeholders
or the shared data already make.

| Never | Why | Live example |
|---|---|---|
| "You died", "death", or blood as the price of losing | Below the Summa every loss is **missio**, the crowd's mercy. `01-story-core.md`: "A loss. Never 'you died'." | **The current placeholder breaks this.** `story.aftermath.close.lost` is "The sand drinks your blood. The collar holds." Section 5 replaces it. |
| Loop language: "again", "this time", "reset", "the last day of every loop" | TV removed loops (plan D2). The shared `arena-tiers.json` `_about` still carries the old wording; `01-story-core.md` warns VR text must not. | none in the current strings |
| Any proper noun the player has no way to place | Today **no proper noun reaches the player's eyes** (`01-story-core.md`). C2 is the first to introduce them, so each one must arrive attached to a tent or a role: "Brennic of the Ember", not "Brennic". | - |
| `K-ford-betrayal` as an on-screen revelation | It is TV's secret, held by the Four, and the hinge of a story the slice does not tell. The prologue **shows** the collaring; nobody explains it. | see D5 |
| `K-wardstones-drink` or the Breaking | The Breaking is a Summa ending. The slice ends at Tiro. Row 12 names "the Wardstones line", which section 11 **D5** resolves as one line of flavour with no knowing attached. | - |
| "Sine missione" | Reserved for high tiers, not built | - |
| A line that tells the player they did well | Ophel's register is footnotes, and praise from a system the player cannot trust is cheap. The **crowd** carries approval; the cast carries information. | - |

## 4. The time budget: how the day reaches 7-10 minutes

This is the arithmetic DF-010's option B is costed on. **The fighting is fixed and is not C2's to move**: the pinned
bands are 25-40, 30-45, 45-70 and 45-80 s (`arena-tiers.json`, Tiro waves 1-4), so four bouts are **145-235 s**. The
teach is **63 s**, measured in T13 (`02-vr-run-as-campaign.md:30`). Everything else in the day is a beat below.

Each beat gets **three numbers**, and the three columns are the whole point:

- **Floor** - the earliest the player may leave the beat. Below this the skip gesture does nothing. This is what a judge
  who palms through everything experiences.
- **Natural** - what the beat takes at a first-time player's pace, with the lines read and not skipped. **This is also
  the dwell the reference script should use** (see the measurement note below).
- **Max** - the timeout. The beat ends itself; no beat ever waits for the player.

| # | Beat | Phase | Floor | Natural | Max |
|---|---|---|---|---|---|
| 1 | Prologue tableau (the ford) | `prologue` | 6 | 36 | 45 |
| 2 | Collar ritual (Septima) | `ritual` | 10 | 38 | 45 |
| 3 | Bout 1 intro (soldiers) | `intro` | 4 | 8 | 8 |
| 4 | **Intermission 1** - the tier IV branch | `intermission` | 3 | 16 | 20 |
| 5 | Bout 2 intro (creatures) | `intro` | 4 | 8 | 8 |
| 6 | **Intermission 2** - before Brennic | `intermission` | 3 | 18 | 20 |
| 7 | Bout 3 intro (Brennic) | `intro` | 5 | 12 | 12 |
| 8 | **Intermission 3** - before the final | `intermission` | 3 | 18 | 20 |
| 9 | Bout 4 intro (Lio) | `intro` | 5 | 12 | 12 |
| 10 | Aftermath tablet | `aftermath` | 5 | 16 | 18 |
| 11 | Closing card | `aftermath` / `closed` | 6 | 31 | 34 |
| | **Story total** | | **54** | **213** | **242** |

And the day, with the teach and the pinned fight band:

| Path through the day | Story | Teach | Fight | Total | In 7-10? |
|---|---|---|---|---|---|
| Rushed: every beat skipped at its floor, fastest bouts | 54 | 63 | 145 | **262 s = 4.4 min** | no - **and this is reported, not hidden** |
| **Natural, fastest bouts** | 213 | 63 | 145 | **421 s = 7.0 min** | **yes, exactly at the floor** |
| **Natural, mid bouts** | 213 | 63 | 190 | **466 s = 7.8 min** | **yes** |
| **Natural, slowest bouts** | 213 | 63 | 235 | **511 s = 8.5 min** | **yes** |
| Max dwell, slowest bouts | 242 | 63 | 235 | **540 s = 9.0 min** | yes |
| Max, slowest, one lost bout retried (+~40 s) | 242 | 63 | 275 | **580 s = 9.7 min** | yes |

**The natural column is tuned to land 7.0 min at the fast end of the fight band.** That is not a coincidence and it is
not slack: it is the smallest story budget that makes the owner's kept number true in the worst fighting case. Cut any
beat's natural dwell and the fast-fight day falls under 7 again.

**Two honest statements that go with this table.**

- **The rushed path is 4.4 min and must be reported, never designed away.** The alternative is forbidding the skip,
  which would mean up to 3.5 min of unskippable cutscene in a 7-minute session - unacceptable for a judge on a
  deadline, and it would make the entry worse to win an argument with a test. A player who skips everything gets a
  4.4-minute day, and that is a legitimate way to play.
- **A judge's hands-on time stays near 2.4-3.9 min**, because option B buys non-interactive minutes. DF-010's ruling
  says this in as many words. C2 cannot change it; only harder bouts could, and those are a signed ruling.

### Why `Session.FullSeated` will still measure short, and the one-line fix

The 3.02 min measurement is partly an artefact of the reference script: `FullSeated` broadcasts `OnWardRaised` the
instant the stage turns to `intermission` (`SessionTests.cpp:730-733`), so every intermission costs ~0 s, and the
prologue is unbuilt. The script does not play the day, it **rushes** it - so it measures the rushed column, which this
table predicts at 4.4 min.

**C2's deliverable for the measurement is the natural column itself.** Once the beats exist, the reference script
should dwell each beat for its natural seconds rather than instant-continuing, and then the reported number means
something: it is a plausible day rather than a lower bound nobody plays. After `T47` the number is a logged
measurement rather than a gate (DF-010 option E), so this changes a report, not a pass. **`T28` should report the day
both ways - rushed and natural - and MOONSHOT `M9` later re-measures it per skill band.**

### What this budget costs in data and code, and who owns each change

C2 authors content; it does not edit a running file. Every change below is **assigned**, so nothing here is implied:

| Change | From → to | Owner |
|---|---|---|
| `day.json` `prologueMaxS` | `10` → `45` | `T28` |
| `day.json` `ritualLineS` | `3` → array, one dwell per ritual line (section 5) | `T27` |
| `day.json` `ritualTimeoutS` | `20` → `45` | `T27` |
| `day.json` `introS` | `3` → array `[8, 8, 12, 12]`, per wave. **Needs a code change**: `SessionFlow.cpp` reads one scalar | `T28` |
| `day.json` `aftermathTabletS` | `4` → `16` | `T28` |
| `day.json` + `closingCardS` | new, `31` natural / `34` max | `T28` |
| Per-beat **floor** (the skip gate) | new concept; today the centre stone skips the prologue at once | `T28` |
| `strings.json` `story.*` lines | placeholders → section 5 | `T27` |
| `strings.json` `_story.*` markers | `"placeholder for C2"` → cue id (section 10) | `T27` |
| `DayTests.cpp` marker assertion | `== "placeholder for C2"` → matches a cue id in the cue sheet | `T27` |
| `arena.sunfall.blinked` flag | new, written by the session | `T28`, and see D6 |

## 5. The script: every line the day shows

These replace the eleven `story.*` placeholders in `apps/vr/data/vr/strings.json` **and** become the fallback line for
their beat (section 8 explains the fallback rule). Word counts are given because a machine checks them.

The **`fallback` column is the single line that must stand alone** if the scene file is missing or malformed - the
never-blocking rule from `T28`. The `sequence` rows are the full beat, used when the scene file loads.

### Beat 1 - the prologue tableau (`prologue`)

A static tableau of the ford. Nobody explains it; the player reads it. **Unvoiced** (see D4 - this is the cue block to
cut first if the voice count must shrink).

| # | Line | Words | Dwell |
|---|---|---|---|
| fallback | `story.prologue.tableau`: **"The ford. Four mages betrayed, four collars closed."** | 8 | - |
| 1.1 | "The ford. Four mages held the line." | 7 | 11 |
| 1.2 | "The legion they held it for closed four collars." | 9 | 13 |
| 1.3 | "Rome calls this the Games. The collar calls it rent." | 10 | 12 |

The fallback is the existing placeholder, which is good and stays. Line 1.3 is the one that has to earn the beat: it
names the arrangement without naming the secret, and "rent" is what makes the collar a debt rather than a punishment -
which is the premise the Tithe mechanic (`T22`, CR-006) already runs on.

### Beat 2 - the collar ritual (`ritual`)

Septima, dais rail left, ritual register, third person. This is the **only beat that teaches the collar clock**, so the
middle line does double duty: it is ceremony and it is the bracer's tutorial.

| # | Line | Words | Dwell |
|---|---|---|---|
| fallback | `story.ritual.septima`: **"Septima: Kneel, Tide. The collar takes its due."** | 8 | - |
| 2.1 | "The Vigil wakes the collar. Kneel, Tide." | 7 | 12 |
| 2.2 | "It will give back one tier each fifteen seconds." | 9 | 14 |
| fallback | `story.ritual.offer`: **"Septima: Offer both wrists to the collar."** | 7 | - |
| 2.3 | "Offer both wrists. The Vigil does not ask twice." | 9 | 12 |

Both existing placeholders stay as fallbacks and are already in register. 2.2 is the new work: it states the 15 s tier
unlock in-world, which no line in the build does today.

### Beats 3, 5, 7, 9 - the bout intros (`intro`)

Venno at the grille, genial showman. He is selling a card to senators; the player is the product. Each intro ends on the
**threat instruction**, because the colour language is the one thing a cold player must hold
(`CLAUDE.md`: element colour = absorb, steel = dodge, black core with a red rim = leave).

| Key | Line | Words |
|---|---|---|
| `story.bout1.intro` | **"Legion soldiers, for the warm-up. Hold the dais."** | 8 |
| `story.bout2.intro` | **"Cinder hounds and a mire maw. Mind the embers."** | 9 |
| `story.bout3.intro` | **"Brennic of the Ember. The ford remembers you both."** | 9 |
| `story.bout4.intro.lio` | **"Lio of the Gale. She has never lost politely."** | 9 |
| `story.bout4.intro.corvo` | **"Corvo of the Ember, second choice. The final."** | 8 |

`story.bout3.intro` is the existing placeholder and is the best line already in the file - it stays untouched. It does
the thing the whole slice needs: it tells a player who has read nothing that this opponent is personal, in nine words,
and it is the only place `K-ford-betrayal` is allowed to surface (as weather, not as a fact).

`story.bout4.intro.corvo` must stay authored even though Air shipped, because it is the **cut fallback**: if T23's Air
rival is pulled, the final becomes Corvo on the Fire kernel (`combat.vr.json` `rivals.tiroFinal._why`), and
`SessionFlow.cpp:629` defaults to `corvo` when no rival is bound. "Second choice" is the line doing work: it tells the
player the final is a lesser card, which is honest about the fallback rather than hiding it.

### Beats 10, 11 - the aftermath (`aftermath`, `closed`)

| Key | Line | Words |
|---|---|---|
| `story.aftermath.tablet` | **"The day is cut into stone."** | 6 |
| `story.aftermath.close` | **"The crowd keeps your name. The collar keeps its due."** | 10 |
| `story.aftermath.close.lost` | **"Missio. The crowd spares you. The collar keeps its due."** | 10 |
| `story.closed` | **"Rest, Tide. The sand will be raked by dawn."** | 9 |

`story.aftermath.close.lost` is the **correction** flagged in section 3: the current placeholder, "The sand drinks your
blood. The collar holds.", contradicts the missio rule that the pinned data itself states. The replacement names missio
outright, which is also the word the kernel already uses for the result (`Kernel/Games.cpp:207-212`), so the text and
the result vocabulary finally agree.

The won and lost lines deliberately **end identically** - "The collar keeps its due." Winning the Tiro final changes
your standing and changes nothing about the collar. That is the slice's actual ending, and it is the hook for a
campaign the slice does not ship.

`story.closed` drops "The Games resume tomorrow" for "The sand will be raked by dawn": same meaning, no scheduling
promise, and it avoids sounding like the loop language section 3 forbids.

### Brennic's and Lio's phase-break taunts

Both rivals have **two** HP-gated phases, at 66 % and 33 %, and the second break **opens with the signature**
(`combat.vr.json`: Brennic `fire_sunfall`, Lio `air_tempest`, both **unblockable**). So the second taunt is not
flavour - it is the warning for an attack the player must not try to absorb.

| Cue | When | Line | Words |
|---|---|---|---|
| Brennic break 1 | HP ≤ 66 % | **"Still polite, Tide? Hit me properly."** | 6 |
| Brennic break 2 | HP ≤ 33 %, opens Sunfall | **"Sunfall. Nothing catches this. Move."** | 6 |
| Lio break 1 | HP ≤ 66 % | **"Oh, you're actually trying. How sweet."** | 6 |
| Lio break 2 | HP ≤ 33 %, opens Tempest Lance | **"Tempest. Your palm will not help. Run."** | 7 |

Both second taunts say the same mechanical thing in two voices: **your ward is no use, leave**. Brennic orders; Lio
mocks. Neither names a colour, because the black core and red rim are already on screen - the line tells the player what
to *do*, which is what a cold player misses.

**This is the single highest-value voice block in the cue sheet.** The unblockable opener is the one moment the slice
can kill a player who has understood everything except this, and a 6-word line is cheaper than any amount of UI.

### The crowd chorus

Row 12 names the chorus. These are **not voice lines**: they are crowd beats layered over the existing
`arena-crowd-loop` cue (A05), triggered by session events. They carry approval, so the cast never has to.

| Cue | Trigger | What it is |
|---|---|---|
| `chorus.break` | a rival phase break | a swell - the data already says a break "swells the crowd" (`combat.vr.json` `rivals._about`) |
| `chorus.perfect` | a perfect absorb | a short bark of approval, under 1 s, no pitch climb |
| `chorus.missio` | the player reaches 0 HP | a low murmur falling away - mercy, not booing |
| `chorus.champion` | the final is won | the only full roar in the day |

`chorus.champion` is the day's only full roar **by design**: it is the payoff the aftermath line refuses to give in
words, and it is the beat the submission video should end on.

## 6. The three intermission scenes

`T28`'s shape: at most one optional choice per intermission; three stones (left 0, centre 1, right 2); one palm held
**0.40 s** (`teach.json offerHoldS`) picks; no pick within the scene's seconds continues with **no flag written and no
penalty**; the pick is unreachable during a bout. Each scene writes `arena.intermission.<n>.choice` as the stone index.

**Every `when` clause below uses a flag the session already writes** - `arena.tiro.1.w<n>.won` (bool), `.attempts`,
`.perfects`, `.time`, `.cracks`, `.tithe` (numbers), `arena.day.closed` (bool), verified in
`Session/DayFlow.cpp:347-358` and `:385`. No scene depends on a flag that does not exist. The one flag C2 would like and
that is **not** written today is `arena.sunfall.blinked`; it is isolated in decision **D6** and no shipping scene needs
it.

### Intermission 1 - the tier IV branch (after bout 1)

**This scene wraps the pick that already exists and must not break it.** The build already offers Leviathan or Rain of
Orbs here through `PlayerPreset.Pick` with labels `pick.tideOrb.A` / `.B` and `pick.continue`
(`SessionFlow.cpp:1536-1612`). C2 adds Ophel's framing and nothing else: the stones, the hold, the timeout and the
branch effect stay exactly as they are.

| | Content |
|---|---|
| `when` | always |
| Ophel 1.1 | "The collar reaches its fourth rune near the end." (9 words) |
| Ophel 1.2 | "Choose what it spends that rune on." (7 words) |
| Stone 0 | **"Leviathan, if it comes"** (4) - existing `pick.tideOrb.A` |
| Stone 1 | **"Continue"** (1) - existing `pick.continue` |
| Stone 2 | **"Rain of Orbs, rather"** (4) - existing `pick.tideOrb.B` |
| Writes | the existing `tideOrb` branch, **and** `arena.intermission.1.choice` |
| Natural / max | 16 / 20 s |

The labels are the existing strings, re-read in Cassia's register: "if it comes" and "rather" are the conditional,
courteous hedges `characters.json` gives her, and they cost no extra words.

### Intermission 2 - before Brennic (after bout 2)

Ophel tells the player how to fight the semifinal. The **`when` picks which lesson he gives**, from how bout 2 actually
went - the VR identity the campaign design asks for ("a choice that depends on how you fought").

| Variant | `when` | Ophel's line | Words |
|---|---|---|---|
| **2A** read the embers | `arena.tiro.1.w2.perfects` ≥ `2` | "You read the embers. Brennic is louder and slower." | 9 |
| **2B** default | - | "The hounds spat. Brennic throws the same, larger." | 8 |

Then, either way, one line and the choice:

| | Content |
|---|---|
| Ophel 2.3 | "He breaks twice. The second break is unblockable." | 8 |
| Stone 0 | **"Then I take the first"** (5) |
| Stone 1 | **"Continue"** (1) |
| Stone 2 | **"I would rather read him"** (5) |
| Writes | `arena.intermission.2.choice` (0, 2; none on timeout) |
| Natural / max | 18 / 20 s |

**The choice is a stance, not a stat.** Neither option changes a combat number - that would mean touching signed data.
Stone 0 and stone 2 are recorded so the aftermath tablet and a future eve scene can refer to how the player chose to
meet him. Ophel 2.3 is the line that matters: it pre-warns the Sunfall one bout early, so the taunt is a reminder and
not a surprise.

### Intermission 3 - before the final (after bout 3)

The one scene that reacts to the day's most personal outcome.

| Variant | `when` | Line | Words |
|---|---|---|---|
| **3A** beat him first try | `w3.won` = true **and** `w3.attempts` ≤ `1` | Ophel: "He went down once and stayed down. Rare." | 9 |
| **3B** beat him after retries | `w3.won` = true | Ophel: "You lost to him first. He will remember that." | 9 |
| **3C** default | - | Ophel: "The Ember takes the other half of the card." | 9 |

Then the choice:

| | Content |
|---|---|
| Ophel 3.4 | "Lio of the Gale. She does not hold still." | 9 |
| Stone 0 | **"I will hold the centre"** (5) |
| Stone 1 | **"Continue"** (1) |
| Stone 2 | **"I would rather move"** (4) |
| Writes | `arena.intermission.3.choice` |
| Natural / max | 18 / 20 s |

Variant 3B exists because of the rule in section 3 about praise: the slice must have something to say to a player who
**lost and came back**, and "He will remember that" is information, not comfort. 3A and 3B are distinguished by
`attempts`, which the session already writes.

### A documented limit this moves, and why

`04-vr-campaign-design.md` section 2 budgets "≤ 2 scenes per Games day (eve and aftermath), **and one optional
intermission choice**" - one choice for the whole day. `T28`, written later, says "**Each** intermission carries at most
one optional story choice". C2 follows `T28` and authors three, for a reason that is now a ruling: DF-010 option B
needs the minutes, and three 20 s scenes are **60 of the 242 max seconds** in section 4's budget. Drop to one choice and
the natural day loses ~34 s and no longer clears 7 min at the fast end of the fight band.

This is recorded as a deviation rather than taken silently: it is decision **D3**.

## 7. Where each beat stands, and the comfort rules it obeys

Every speaker is inside the front ±70°, or ±40° in narrow view, and no line is head-locked
(`04-vr-campaign-design.md` section 2; plan section 6.1).

| Beat | Speaker position | Text position |
|---|---|---|
| Prologue | no speaker (tableau) | dais rail, world-locked |
| Ritual | Septima, dais rail left | dais rail |
| Bout intros | Venno, the grille above the gate | dais rail |
| Intermissions | Ophel, dais rail right | dais rail, above the three stones |
| Phase taunts | the rival, arena floor centre | **none** - voice only |
| Aftermath, closing | Venno at the grille; tablet on the dais | the tablet |

**The phase taunts carry no text.** The player is mid-duel with both hands committed, and a line on the rail during a
Sunfall telegraph competes with the exact thing it is warning about. If the owner declines voice (D4), the taunts are
cut rather than converted to text - see D4 for what that costs.

## 8. The scene data file: path, schema and the fallback rule

**Path: `apps/vr/data/vr/story/slice.json`.** It sits in the VR overlay beside `day.json` and `strings.json`, which is
where session data lives; it is **not** combat data and touches no pinned file. `T28` should confirm
`node apps/vr/tools/check-pin.mjs` still prints OK after it lands.

The file is authored and committed with C2, and is **inert until `T27`/`T28` read it** - nothing in the build loads it
today, so it cannot turn a gate red on arrival.

**The fallback rule, which is the whole reason the schema looks like this.** Each beat's `fallback` is the key in
`strings.json` that already works. If `slice.json` is missing, unreadable, or not version 1, the day plays exactly as it
does now - one line per beat, from `strings.json`, 54-ish seconds of story - with a warning. This satisfies `T28`'s
"a missing or malformed scene file is skipped with a warning", and it means **the slice is never broken by its own story
content**. The long day is an enhancement over a day that already works.

```json
{
  "version": 1,
  "beats": [
    { "id": "prologue", "phase": "prologue", "fallback": "story.prologue.tableau",
      "floorS": 6, "naturalS": 36, "maxS": 45,
      "lines": [ { "cue": "PRO-01", "speaker": null, "text": "The ford. Four mages held the line.", "dwellS": 11 } ] }
  ],
  "scenes": [
    { "id": "int-1", "intermission": 1, "floorS": 3, "naturalS": 16, "maxS": 20,
      "variants": [ { "id": "1A", "when": null, "lines": ["OPH-01", "OPH-02"] } ],
      "choice": { "writes": "arena.intermission.1.choice", "keepsExistingPick": true,
                  "stones": [ { "index": 0, "label": "pick.tideOrb.A" },
                              { "index": 1, "label": "pick.continue" },
                              { "index": 2, "label": "pick.tideOrb.B" } ] } }
  ]
}
```

Field rules:

- `when` is `null` (always) or `{ "flag": "<arena flag>", "op": "eq|gte|lte", "value": <bool|number> }`, the campaign
  design's own clause shape (section 4). **Variants are evaluated in file order and the first match wins**, so the
  default variant is last and carries `"when": null`. A clause naming a flag that does not exist does not match; it is
  never an error.
- `floorS` ≤ `naturalS` ≤ `maxS`, and `maxS` ≤ 20 for any scene (`T28`).
- `label` is either a `strings.json` key (so the existing pick keeps its existing labels) or an inline string of ≤ 5
  words.
- `cue` joins a line to the cue sheet in section 9. Every line has one, voiced or not, because the cue id is also what
  the `_story.*` marker becomes (section 10).
- A line with no audio asset plays as text for its `dwellS`. **Voice is additive**: the day is complete and correctly
  paced with no WAV in the tree, which is what lets `T27` land its wiring before the owner rules on D4.

## 9. The cue sheet for T27

`T27` needs "the C2 cue sheet" and states "its path is the first line of the report". **That path is this document**,
and the table below is the sheet. Audio assets go at `apps/vr/art/audio/voice/<speaker>/<cue>.wav`, with the
`manifest.json` and `SOURCE.md` that `T27` already specifies.

| Cue | Speaker | Text | Words | Where it plays | Block |
|---|---|---|---|---|---|
| PRO-01 | - | The ford. Four mages held the line. | 7 | prologue | P |
| PRO-02 | - | The legion they held it for closed four collars. | 9 | prologue | P |
| PRO-03 | - | Rome calls this the Games. The collar calls it rent. | 10 | prologue | P |
| RIT-01 | septima | The Vigil wakes the collar. Kneel, Tide. | 7 | ritual | **A** |
| RIT-02 | septima | It will give back one tier each fifteen seconds. | 9 | ritual | **A** |
| RIT-03 | septima | Offer both wrists. The Vigil does not ask twice. | 9 | ritual | **A** |
| VEN-01 | venno | Legion soldiers, for the warm-up. Hold the dais. | 8 | bout 1 intro | B |
| VEN-02 | venno | Cinder hounds and a mire maw. Mind the embers. | 9 | bout 2 intro | B |
| VEN-03 | venno | Brennic of the Ember. The ford remembers you both. | 9 | bout 3 intro | B |
| VEN-04 | venno | Lio of the Gale. She has never lost politely. | 9 | bout 4 intro (air) | B |
| VEN-05 | venno | Corvo of the Ember, second choice. The final. | 8 | bout 4 intro (cut fallback) | D |
| VEN-06 | venno | The crowd keeps your name. The collar keeps its due. | 10 | aftermath, won | B |
| VEN-07 | venno | Missio. The crowd spares you. The collar keeps its due. | 10 | aftermath, lost | B |
| OPH-01 | ophel | The collar reaches its fourth rune near the end. | 9 | intermission 1 | C |
| OPH-02 | ophel | Choose what it spends that rune on. | 7 | intermission 1 | C |
| OPH-03 | ophel | You read the embers. Brennic is louder and slower. | 9 | intermission 2, variant 2A | C |
| OPH-04 | ophel | The hounds spat. Brennic throws the same, larger. | 8 | intermission 2, variant 2B | C |
| OPH-05 | ophel | He breaks twice. The second break is unblockable. | 8 | intermission 2 | **A** |
| OPH-06 | ophel | He went down once and stayed down. Rare. | 8 | intermission 3, variant 3A | C |
| OPH-07 | ophel | You lost to him first. He will remember that. | 9 | intermission 3, variant 3B | C |
| OPH-08 | ophel | The Ember takes the other half of the card. | 9 | intermission 3, variant 3C | C |
| OPH-09 | ophel | Lio of the Gale. She does not hold still. | 9 | intermission 3 | C |
| OPH-10 | ophel | Rest, Tide. The sand will be raked by dawn. | 9 | closing card | C |
| BRE-01 | brennic | Still polite, Tide? Hit me properly. | 6 | Brennic break 1 (66 %) | **A** |
| BRE-02 | brennic | Sunfall. Nothing catches this. Move. | 6 | Brennic break 2 (33 %) | **A** |
| LIO-01 | lio | Oh, you're actually trying. How sweet. | 6 | Lio break 1 (66 %) | **A** |
| LIO-02 | lio | Tempest. Your palm will not help. Run. | 7 | Lio break 2 (33 %) | **A** |

**27 cues, every one 10 words or fewer.** Five speakers; Cassia has none. OPH-10 is listed once and plays in both
closing variants, won and lost, which is why the blocks below total 27 and the scene file places 29 lines.

The **Block** column is the cut order, and it exists so the owner can approve a count rather than a wish list:

| Block | Cues | Count | What it is | Cut? |
|---|---|---|---|---|
| **A** | RIT-01..03, OPH-05, BRE-01, BRE-02, LIO-01, LIO-02 | **8** | The ritual that teaches the collar clock, and every line that warns about an unblockable attack | **Never.** This is the block that changes whether a cold player survives. |
| **B** | VEN-01..04, VEN-06, VEN-07 | 6 | The Lanista: announces and closes the day. Gives the arena a host. | Only under pressure |
| **C** | OPH-01..04, OPH-06..10 | 9 | Ophel in the intermissions | Cut to text first; they are already text-backed |
| **D** | VEN-05 | 1 | The Corvo fallback intro | Only if Air is certain |
| **P** | PRO-01..03 | 3 | The prologue tableau | Cut first; it is already unvoiced in section 5 |

**Recommended count for the owner's go: 14 (blocks A + B).** A alone is 8 and is the minimum that is worth doing at
all. All 28 is the full sheet. The recommendation is A + B because together they make the day feel hosted and warned,
while Ophel's nine lines read perfectly well as text on the rail where the player has time to read them - the
intermissions are the one place in the day where nothing is attacking.

`T27` must not generate any of these before the owner's go (D4), and the generation count belongs on the T27 card once
that go is given.

## 10. The `_story.*` marker convention, and the test it changes

`strings.json` pairs every `story.*` line with a `_story.*` marker, and `Tests/DayTests.cpp:866-868` **asserts the
marker is exactly `"placeholder for C2"`**. So the markers are not decoration: they are a machine-checked promise that
C2 has not landed yet, and replacing the lines without replacing the markers would leave that promise lying.

**The convention C2 sets:** each `_story.*` marker becomes its **cue id** from section 9.

```json
"story.ritual.septima": "The Vigil wakes the collar. Kneel, Tide.",
"_story.ritual.septima": "RIT-01"
```

And `DayTests` changes from asserting the placeholder text to asserting the marker **is a cue id that the cue sheet
carries**. That is a strictly stronger test than the one it replaces: it fails on a line with no cue, on a cue with no
line, and on a typo in either - so the script, the string table and the voice manifest cannot drift apart silently. It
is also the check that catches the most likely real mistake, which is a line edited in `strings.json` and never
re-recorded.

`T27` owns both edits, and it is a **Wolf** card: the assertion runs in the editor, which does not exist on Lynx. C2
specifies the convention and cannot verify it here.

## 11. What only the owner can settle

**D1-D4 are APPROVED as recommended, owner 2026-10-24** ("Approve the story script rulings"; `docs/DECISIONS.md`,
"2026-10-24 - C2 rulings D1-D4 approved as recommended"). They no longer block work: `T28` may proceed on its non-audio
parts. D5-D7 were not part of that ruling and stay open; they never blocked work.

| # | Decision | What C2 assumed, and the cost of the other answer |
|---|---|---|
| **D1** | **Is this the voice of the game?** The whole script, in five registers. **APPROVED as recommended, owner 2026-10-24** (`docs/DECISIONS.md` 2026-10-24, "C2 rulings D1-D4"). | Assumed yes. This is the one decision that cannot be delegated: every line above is a creative proposal, and the owner's taste outranks all of it. Rewriting a line is cheap *now* and costs a re-record after D4. |
| **D2** | **The time budget in section 4.** 213 s of story at natural pace, 54 s floor. **APPROVED as recommended, owner 2026-10-24** (`docs/DECISIONS.md` 2026-10-24, "C2 rulings D1-D4"). | Assumed as tabled, because it is the smallest budget that makes 7 min true at the fast end of the fight band. Shrinking it re-opens DF-010; growing it eats the judge's attention. |
| **D3** | **Three intermission choices, not one.** Deviates from `04-vr-campaign-design.md` section 2, follows `T28`. **APPROVED as recommended, owner 2026-10-24** (`docs/DECISIONS.md` 2026-10-24, "C2 rulings D1-D4"). | Assumed three. One choice costs ~34 s of the natural day and drops the fast-fight path under 7 min. |
| **D4** | **Voice: approved, and at what count?** The ElevenLabs approval (`docs/DECISIONS.md`) names SFX and music; `04-vr-campaign-design.md` calls voice its own decision 4. **APPROVED as recommended (voice, 14 cues, blocks A + B), owner 2026-10-24** (`docs/DECISIONS.md` 2026-10-24, "C2 rulings D1-D4"). **Its audio production is deferred** by the owner's audio ruling of the same day (`docs/DECISIONS.md` 2026-10-24, "Music and audio deferred"): no cue is generated until that ruling is lifted. | Assumed **not yet approved**: nothing is generated, and the day is paced to be complete as text. Recommended go: **14 cues, blocks A + B**. If voice is declined outright, the four phase taunts are cut rather than shown as text (section 7) - and that is the real cost of declining, because they are the unblockable warning. |
| **D5** | **The Wardstones line.** Row 12 names it; the Breaking is a Summa ending the slice does not reach. | Assumed **cut from the slice**: no Wardstones line, no knowing written, so the slice promises nothing it cannot pay off. If the owner wants the hook planted for the campaign, it is one Ophel line in intermission 3 and a `knowing` the slice never reads. |
| **D6** | **`arena.sunfall.blinked`.** The campaign design lists it; the session does not write it. | Assumed **not needed**: no shipping scene uses it. It is the natural `when` for "Ophel notes you read the unstoppable spell", which is the single best use of the flag mechanic in the day. Adding it is small and belongs to `T28`. |
| **D7** | **CR-008, the four-route framing.** The roadmap assigns it to C2 (`V2-ROADMAP.md:108`). | **Not written.** C2 found no "four routes" defined anywhere in the repo, and would be inventing the concept rather than recording it. The owner should say what the four routes are, or retire CR-008. |

## 12. What C2 does not decide

- **No combat number, pinned band, win rate or pacing window.** Not one line above changes a number. The stance choices
  in intermissions 2 and 3 are recorded as flags and read by nothing that fights.
- **It does not restate the day's length in the plan.** DF-010's ruling sets that condition: `T27`/`T28` land **and** a
  day is measured that wins all four real bouts. No such day has been measured.
- **It does not move `FullSeated:792`.** That is `T47`, carrying DF-010 option E.
- **It generates no audio** and names no ElevenLabs voice id. That is `T27` after D4.
- **It does not pin TV story data.** The slice reads `slice.json` and `strings.json` only. When TV commits the quest
  schema, section 1 says how this content migrates.
