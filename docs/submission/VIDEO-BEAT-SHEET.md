Status: draft, for the owner

# Video beat sheet and script (under 3 minutes)

Item 6 of the K6 list (`docs/research/KPI-2026-10-20.md` K6). Item 7, the captured and public video, is a separate item
and **does not exist**: no capture is on record. The owner finalises this file.

## Rules this video must meet

From `docs/PROJECT-PLAN.md` section 2:

- Under 3 minutes, public on YouTube or Vimeo. Judges need not watch past 3 minutes.
- Shows the project as viewed on a Quest device or via the XR Simulator or an equivalent emulator. Emulator footage is
  allowed, but has no real hands (section 11, "Fallback").
- Real captured gameplay carries the pitch; generated video is at most garnish.
- English, or English subtitles. No logos, brand marks or advertising. Nobody is shown or named except the entrant.
- Lead with deliberate play, not twitch play (section 2, "Genre fit").

## Timing

The plan's own beats (`docs/PROJECT-PLAN.md` section 11) end at 2:55. This sheet keeps the same order and ends at 2:50,
which leaves 10 seconds of margin under 3:00. Durations in seconds:

| # | Time | Dur | Beat | Footage |
|---|---|---|---|---|
| 1 | 0:00-0:10 | 10 | Hook: a fireball comes at the camera, a palm rises, a white ring and a bell, the fireball flies back (Mirror Reflection). Title | **Needs footage that does not exist.** Reflection is built as a player preset (`apps/vr/tasks/T20-cassia-preset-reflection-leviathan.md:3`), but it never fires in the scripted census, so no capture shows it. Needs a style (T16) |
| 2 | 0:10-0:28 | 18 | Close-up of the hands: the circle, then the bar, then Tide Orb. Caption: "Draw spells. No controllers." | **Needs footage.** Needs a device or simulator with hands. Desktop clip replay exists in the editor only |
| 3 | 0:28-0:38 | 10 | Real-world shot: the entrant seated in a chair, hands only. Caption: "Seated. Ten-minute matches." | **Needs footage.** A camera shot of the owner; nothing on record |
| 4 | 0:38-1:23 | 45 | The duel against the Fire mage: wrist runes igniting (tier clock), Flow beads to a Crest, a black-core Sunfall, a flick blink to safety | **Needs footage.** Also depends on design fixes: the Fire duel test is red (`CLAUDE.md`, known failures) and Sunfall was cast in 0 of 40 seated duels (`docs/design/V2-VISION.md:47`). Needs the animated opponent (T38) |
| 5 | 1:23-1:43 | 20 | The first-minute teach: palm, sigil, blink | **Needs footage.** The teach is built (card `T13`, done 2026-10-04) but not captured in a chosen style |
| 6 | 1:43-2:13 | 30 | The finisher: the two-hand seal, Leviathan Orb, slow motion, crowd | **Needs footage that cannot exist yet.** No seal detector exists (C1 not written, T24 blocked); do not promise this beat until T24 lands. Fallback: cut to a reflected fireball |
| 7 | 2:13-2:35 | 22 | Comfort: narrow-FOV mode, left-hand mode, pause and resume by raising both palms | **Needs footage.** The features are built (cards `T13`, `T14`, done 2026-10-04) but not captured |
| 8 | 2:35-2:50 | 15 | Future-plans card (four schools, story days, ghost duels), the launch date, the title | Graphic only. The launch date is [confirm: Q5]. Can be made on any host once the date is set |
| | **Total** | **170** | **2:50** | |

Sum check: 10 + 18 + 10 + 45 + 20 + 30 + 22 + 15 = 170 seconds = 2:50.

## Script (voice-over, English)

Short lines, plain words. Read at a calm pace; the video leads with reading and timing, not speed. The owner rewrites
these in their own voice.

1. (0:00) No voice. Sound only: the fireball, the bell. Title card: the game's name. [confirm: final title]
2. (0:10) "No controllers. Draw a circle, then a bar, and the spell is yours."
3. (0:28) "You play seated. A match takes about ten minutes."
4. (0:38) "Read the threat. Colour means absorb it with your palm. Steel means blink. A black core means leave the pad."
5. (0:48) "Your wrist clock unlocks stronger spells. Vary your casts and your Flow builds to a Crest."
6. (1:23) "The first minute teaches itself: palm, sigil, blink. No menus."
7. (1:43) "At the climax, both hands make the seal." Only if the seal exists; otherwise use the Reflection finish and the line
   "Catch the fireball. Send it back."
8. (2:13) "Built for comfort: a narrow-view mode, a left-hand mode, and the game pauses when tracking is lost."
9. (2:35) "Next: all four schools, more story days, and duels against recorded play. Coming [confirm: launch date]."

Lines 4 and 5 run over the duel in beat 4 (0:38 to 1:23). Total spoken text is about 100 words, which fits 170 seconds
with room for the music and sound.

## What the beats claim, and where the repo says it

| Claim | Source |
|---|---|
| Circle, then a bar; no controllers | `docs/DECISIONS.md` 2026-10-03 "Sigil shape"; `docs/PROJECT-PLAN.md` section 2, "Hands" |
| Seated; ten-minute matches | `docs/PROJECT-PLAN.md` sections 6.1 and 6.4. The day measured 3.0 min against a 7-10 min target (`apps/vr/tasks/T21-tiro-day-chain.md:3`), so "about ten minutes" is a target, not a measurement |
| Colour language | `docs/PROJECT-PLAN.md` section 6.3 |
| Wrist clock, Flow, Crest | `docs/PROJECT-PLAN.md` section 6.3 |
| Narrow-FOV, left-hand, pause and resume | `docs/PROJECT-PLAN.md` sections 6.1, 6.6, 6.7; cards `T13`, `T14` |
| Four schools, story days, ghost duels | `docs/design/V2-ROADMAP.md` "Spring 2027"; `docs/PROJECT-PLAN.md` section 6.8 |

## Footage that does not exist yet, and what delivers it

| Needed | Why it is missing | Delivered by |
|---|---|---|
| Any capture of the finished look | No style picked; T16 open (`apps/vr/tasks/T16-style-pick-replay-capture.md:3`) | Milestone 2, Wolf |
| The animated Fire mage | No skeletal mesh or animation asset (STATE-EVAL section 2) | T38, milestone 2, Wolf |
| A passing Fire duel to film | Design suite red until T39 | T39, milestone 2, Wolf |
| The seal | No detector; shape not set | C1, then T24 (not on the milestone list; at risk) |
| Footage on a Quest device or the simulator with hands | No APK has been produced; the boot under the Quest 3 profile is not re-proven on 5.8.3 (`docs/XR-TOOLCHAIN.md:9`, `:197`) | Milestone 3 (K5), Wolf |
| The owner on camera, seated | Nothing recorded | The owner, any time |
| Music | No loop imported (T40, blocked on an owner input) | T40, Wolf |
| Voice-over recording and edit | Not started | The owner, after the footage |
| Upload, public link | Not started | The owner, milestone 4 (17 Nov) |

## Open points for the owner

- Brand marks: the rules ban logos and brand names (`docs/PROJECT-PLAN.md` section 2). Decide whether captions may say
  the headset's name; the sheet above does not.
- Whether beat 6 stays with the seal or switches to the Reflection finish. Decide after T24's status is known.
- Fallback if device capture fails: the XR Simulator, which the rules accept (`docs/PROJECT-PLAN.md` section 11).
