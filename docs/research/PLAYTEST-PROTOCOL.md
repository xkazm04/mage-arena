# Cold desktop playtests: protocol (T41)

Written 2026-10-24 on a host with no Unreal build, as the protocol half of `apps/vr/tasks/T41-cold-desktop-playtests.md`.
**No session has run. No tester has been found. No build has been staged.** This file is the protocol and a blank record
template; it is evidence of nothing. It moves K3 row 12 ("2 cold testers finish", `docs/research/KPI-2026-10-20.md`) only
after the sessions run and a dated record is committed. K3 row 15 ("felt") is the owner's verdict alone; the debriefs
below are input to it, not a reading of it.

Where this file and `docs/DECISIONS.md` disagree, DECISIONS.md wins.

## 1. Who counts as cold

A tester is cold only if all of these hold, asked before the session and written in the record:

- has never played this game or any earlier build of it;
- has never watched a recording, stream, screenshot set or demo of it;
- is not part of the project (no one who has worked on it, reviewed it, or been briefed on its rules);
- has not been told how the game plays by anyone, including the person who found them.

One of the two may be a non-gamer. The two testers should not have talked about the game to each other before both have
played. If any answer fails, the person may still play, but the session is logged as **not cold** and does not count toward
K3 row 12.

## 2. The build

Record these before session 1. They are deliberately blank: nothing here may be filled in from memory or guessed.

| Field | Value |
|---|---|
| Commit hash | **to be recorded on Wolf before session 1** |
| Stage path | **to be recorded on Wolf before session 1** |
| `node apps/vr/tools/check-staged-clips.mjs` printed OK | **to be recorded on Wolf before session 1** |
| Engine | **to be recorded on Wolf before session 1** (expected UE 5.8.3) |
| Stated resolution and window mode | **to be recorded on Wolf before session 1** (owner to state; see section 7) |

Status of staging: `docs/concepts/WOLF-PLAN.md` P8 notes that no script for a Win64 stage exists in `apps/vr/tools/`, so
the staging command is still to be verified on Wolf. This protocol names none. The build is the desktop build with the
full Tiro day, made after T39 and T16/T38 land. Every session uses the same commit; if the commit changes between sessions,
the changed hash is written in each record and the verdict says so.

## 3. Setup

- One PC, a mouse and a keyboard, the stated resolution (section 2), speakers or headphones at a comfortable level.
  No headset.
- The tester sits at the keyboard. The host sits to the side or behind, out of the tester's sight line, with this protocol
  and a clock.
- The tester gets **the key sheet below and nothing else**: no rules text, no tips, no earlier footage. The in-game teach
  and HUD are the rest of the help.
- Launch the staged build fresh for each session. The host does not touch the keyboard after launch.
- Screen capture, if any, is of the game window only. No camera on the tester, no microphone on the room (section 8).

### Key sheet (one page, handed over printed)

Derived from `docs/DESKTOP-INPUT.md` (default bindings, as of df30a87). The page carries exactly this, titled "Keys":

```
KEYS

Mouse
  Hold LEFT button and draw   draw a spell sigil in the air in front of you
  Hold RIGHT button           raise your ward hand toward the cursor
                              (it only raises while the cursor is near the middle of the view)

Keyboard
  1  2  3       cast spell line 1 / 2 / 3
  4             tier IV mudra
  Q             bolt (always available)
  Space (hold)  raise your ward hand toward the middle of the view
  Space (tap, just as a hit lands)  perfect ward
  A  /  D  /  S blink to the left pad / the right pad / the back pad
  F             plant the staff (press again to lift it)
  Z  /  X  /  C choose the left / centre / right stone (hold about half a second)
  G             closed fist (finish a fallen rival)
  Esc           pause
  R             resume after a safety pause (both palms)

Hold Shift for a sloppier hand, Ctrl for a slower one.
```

The sheet adds no word of strategy. The designer-only keys (`F1`-`F9`) are not on the sheet and the host never mention
them. If the build's keys differ from `docs/DESKTOP-INPUT.md` at the recorded commit, the host corrects the sheet to match
the build before session 1 and notes the difference in the record; the sheet is never improved beyond that.

Note for the host, not the tester: the mouse ward raises only within 35 degrees of the view centre, and the bolt has no
mouse binding (T43 notes in DESKTOP-INPUT). A tester stuck on either is a stall to log (section 4), not a question to
answer.

## 4. The host script and the rule of silence

Read these lines aloud in this order and nothing else. Do not paraphrase, do not add reassurance.

1. Before launch: "Thank you for helping. We are testing the game, not you. You cannot do this wrong."
2. "I will ask you four yes-or-no questions first." Ask the four cold questions of section 1 ("Have you played this game
   before? Have you watched or seen any recording or pictures of it? Do you work on it or have you been briefed on it?
   Has anyone told you how it plays?") and record the answers.
3. "On the page in front of you are the keys. That page and what is on the screen are all the help there is."
4. "I cannot answer questions about how to play while you play. If you are stuck, that is useful for us to see. Please say
   out loud what you are thinking and what you expected."
5. "You can stop at any time and you do not have to give a reason."
6. "Your name will not be recorded. Nothing with your face or voice will be kept."
7. Launch the build. Say: "Begin when you are ready."
8. When the aftermath screen appears, or the tester stops: "Thank you. I have three questions."
9. Ask the three debrief questions, in this order, and write the answers close to the tester's own words:
   1. "What was unclear?"
   2. "What felt good?"
   3. "Would you play it again?"
10. Close: "That is all. Thank you."

### The rule of silence

During play the host answers **no** question about how to play: not what a key does, not what a cue means, not whether the
tester is doing it right. The reply to any question is one fixed line: "I can't help with that. Do what you would do."
The host does not point at the screen, the key sheet or the keyboard, and does not react to success or failure.

A stuck tester is timed:

- The host starts the clock when the tester cannot progress (no progress visible on screen, or the tester says they are
  stuck).
- At **30 s** with no progress the stall is logged: the moment (the game state on screen), the wall time, and the cause
  **as said by the tester** (not the host's reading).
- At **3 min** of the same stall the stall is logged as final, not fixed, and the host asks, "Do you want to keep going or
  stop here?" If the tester keeps going, play continues untouched. If the stall lasts a further 3 minutes, the host may end
  the session; it is recorded as **not finished** with the bout reached.
- A restart by the host makes the session **helped**: it does not count as an unhelped finish. The host restarts only if
  the build itself crashed or hung, and records exactly that.
- A tester who loses a bout retries by the game's own offer. Retries are allowed and counted; the host never starts one.

## 5. What is recorded per session

One row per session in the table below, with the debrief and the stall list beside it. Write it during or straight after
the session, from the host's notes only.

| Field | Meaning |
|---|---|
| Tester id | Anonymous: `T-A`, `T-B`, ... No names, no initials, no contact detail |
| Cold? | The four answers of section 1, and yes/no for the whole |
| Date | Calendar date of the session |
| Commit | The hash from section 2 as built for this session |
| Start / end wall time | Local clock, from launch to the end of play |
| Bout reached | Cold start, teach, bout 1 soldiers, bout 2 creatures, bout 3 duel, victory or loss card, aftermath |
| Retries per bout | Count of bout restarts after a loss, for each bout |
| Stalls over 30 s | Each: the moment, the wall time, the cause as said by the tester, the duration, how it ended |
| Debrief | The three answers, in the tester's words |
| Aftermath reached | Yes or no; unhelped yes or no |

Terms: **finished** means the aftermath screen is reached (won or lost final; retries allowed) with no restart by a helper.
**Unhelped** means the host said nothing about how to play and restarted nothing.

## 6. The K3 row 12 verdict rule

- **Pass** only if **both** testers were cold, both **finished**, and both finished **unhelped**.
- With **fewer than two finished sessions**, the reading stays **0**. The record states how many sessions ran and how many
  finished, as numbers, even when that is `0 ran, 0 finished`.
- A not-cold, helped or unfinished session is recorded in full but counts as not finished for this row.
- Every stall over 30 s becomes a defect line in the dated record for the stabilise loop; findings that are bugs get a
  one-line pointer appended to `apps/vr/tasks/BACKLOG.md`. A blocker found in a session is filed; fixing it is another card.
- The row 12 line in the record reads: `K3 row 12: <pass | 0>, <n> of 2 testers finished unhelped (<m> sessions ran)`.
- Only the App Master or the owner writes the verdict, from the host's notes. A builder does not invent a session.

## 7. Open questions for the owner

- The resolution and window mode for section 2 (not decided; the card asks for a stated one).
- Who the two cold testers are and who hosts (WOLF-PLAN P8: owner).
- Whether the 3-minute stall timing should differ for a non-gamer. This protocol uses the card's 3 minutes unchanged.

## 8. Privacy

- No names, no faces, no audio of a person in this repo or in the submission (the contest forbids identifiable persons in
  the submission; keep it out of the repo as well).
- Tester ids are anonymous letters; the host keeps no mapping from id to person in the repo.
- No camera or microphone recording of the tester. Screen capture, if used, shows the game window only and is not committed
  unless it is free of any person.
- The record quotes the debrief in words only, trimmed of anything that would identify the speaker.

## 9. Blank template for `docs/research/PLAYTEST-2026-10-DD.md`

Copy this to `docs/research/PLAYTEST-2026-10-DD.md` (DD = the day the second session ends; `-11-DD` if it slips into
November) and fill it from the host's notes. Do not create the dated file until a session has actually run.

````markdown
# Cold desktop playtests: PLAYTEST-2026-10-DD

Protocol: `docs/research/PLAYTEST-PROTOCOL.md`. Written by: <App Master or owner>, from the host's notes.

## Build
- Commit: <hash>
- Stage path: <path>
- check-staged-clips.mjs: <OK line>
- Engine and resolution: <UE version, resolution, window mode>
- Sessions ran: <n>. Finished unhelped: <m>.

## Tester T-A
| Field | Value |
|---|---|
| Cold (four answers) | |
| Date | |
| Commit | |
| Start / end wall time | |
| Bout reached | |
| Retries per bout | teach: , bout 1: , bout 2: , bout 3: |
| Aftermath reached | yes / no; unhelped yes / no |

Stalls over 30 s:
| Moment | Wall time | Cause (tester's words) | Duration | How it ended |
|---|---|---|---|---|

Debrief
1. What was unclear:
2. What felt good:
3. Would you play again:

## Tester T-B
(same table, stalls and debrief)

## Verdict
K3 row 12: <pass | 0>, <n> of 2 testers finished unhelped (<m> sessions ran)

## Defect lines for the stabilise loop
- <one line per stall: moment, cause, severity> (bugs also appended to `apps/vr/tasks/BACKLOG.md`)

## For the owner (row 15, felt)
The debrief answers above are input only. Row 15 is the owner's verdict.
````
