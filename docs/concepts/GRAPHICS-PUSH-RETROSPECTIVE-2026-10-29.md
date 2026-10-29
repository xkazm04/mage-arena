# Graphics push retrospective (2026-10-28/29, Lynx)

The owner decommissioned the VR project after a two-day graphics push (DECISIONS 2026-10-29). This is the honest
account of how the process fell short. Each shortcoming is written so a future project, including a revived PC
channel, can act on it.

The record behind it:
- `docs/concepts/GRAPHICS-PUSH-2026-10-28.md` and `RETARGET-CHALK-SLATE-2026-10-28.md` section 9;
- `docs/research/IMAGE-BASED-25D-RESEARCH-2026-10-29.md`;
- `docs/archive/graphics-push-2026-10/`.

## What happened, in numbers

The table shows each phase, how many rounds it ran, and its best score. Scores are the mean of three blind model
judges against an in-set control.

| Phase | Rounds | Best score | Control in the same set |
|---|---|---|---|
| S1 painted look (3D materials, two tiers) | 3 judge rounds | 25.3 / 26.3 of 45 | baseline 13.7 |
| Chalk & Slate, 3D materials | p1-p11 | 21.3 of 36 (p9) | flat 17-21 band |
| Chalk & Slate, image-based 2.5D | s2-s8 | 26.3 of 36 (s7b) | p11 13.7-17.3 |
| Owner's own read of the last round | | about 20 of 36 | |

Two days of compute ran two Opus builders, about 20 judge panels and about 60 GPU-hours of local generation. The
final look sat about 10 points under the owner's gate by the owner's eye.

## The shortcomings

### 1. The target art was chosen before anyone asked whether our toolkit could make it

The owner picked the S1 key art, then retargeted to Chalk & Slate, on how the concept looked. Nobody tested first
whether a local generative toolkit, driven by agents and with no human artist, could produce that look's hardest
elements:
- first-person hands that read as drawn,
- an animated opponent with volume,
- one coherent medium across the frame.

The first eleven chalk rounds were spent learning that 3D materials could not do it. The next eight learned that
2.5D got halfway.

**Do differently:** run a toolkit-fit spike before choosing or committing to an art direction. Build only the two or
three hardest assets in the candidate style (for a hands-first game: the hands and one animated opponent), judge
them in motion and in context, and pick the direction the toolkit can actually reach.

### 2. We polished the cheap layers first and the hardest asset last

Round order followed what the builders could move fastest: the backdrop, the sand, the sky, the ward, the fire. The
hands were the most-seen asset in a hands-first game, and they got a dedicated look only in round 2 of the 2.5D
phase. They then cycled through wood grain, plastic, cardboard and white smears across eight rounds and never
reached a 2 in style from most judges.

**Do differently:** order the work by salience to the player and by risk, not by how fast the team can move it. The
asset the player looks at most, and the one most likely to fail, goes first.

### 3. The registry's own stop rule was not applied

The concept-scene-parity golden path already said: *when an element's cap sits in a stage whose demonstrated output
has not moved it for two rounds, stop funding it and record the limit.* The hands met that condition by about s4.
The scores plateaued at 24-26 for four rounds (s4b-s8) before anyone wrote "no". The loop consulted the registry
when it planned a round, never when it read a round's results.

**Do differently:** put the stop rule in the round template itself. After every judge round, list each element's
movement over the last two rounds. Any element flat for two rounds needs either a different stage (a new technique
or tool, not more polish) or a written limit.

### 4. The model judges were never calibrated against the owner

Blind model panels with an in-set control removed one kind of bias (story-telling). They did not remove leniency.
When the owner finally scored the last round, the read was about 20 against the panels' 24-26, so the panels were
4-5 points generous.

The panels also contradicted themselves between sets:
- the S7 panel preferred thin drawn ward strands over broad sheets, and the S8 panel preferred the reverse;
- s7b scored 26.3 in one set and 25.3 in the next.

We acted on single-panel preferences and steered the ward into a regression.

**Do differently:**
- Have the owner blind-score the first round and every few rounds after it, and carry the offset beside every panel
  total.
- Never reverse a decision on one panel's preference; require the owner or two agreeing panels.
- Judge motion, not only stills. The panels read stereo flatness and animation from sheets and strips, where it is
  barely visible.

### 5. We judged at a framing the game never shows

For most of the push Brennic was judged at 16 m, his spawn. A replay trace taken late (round 4 of the 2.5D phase)
showed the opponent:
- fights at 3.5-8 m (median 6.9 m),
- moves 81% of the time,
- casts or is hit while moving 93% of the time.

The sprite route was chosen and polished on a frame that flattered it.

**Do differently:** measure the play first. Before judging, record where the camera, the hands and the opponent
actually are in play, and judge at those distances and in motion. A one-hour trace would have changed the route
decision.

### 6. Mixed media was accepted as a temporary state and never resolved

The 2.5D frame combined three production media: a painted backdrop, a video-diffusion sprite and 3D hands. Each
moved on its own track, so every round each one landed in a slightly different idiom (engraving, painted
illustration, woodcut, vector). The judges' "four styles in one frame" comment from s2 was still true in s8.

**Do differently:** choose one primary medium for the frame, or fund a unifying pass that every layer goes through
from round one, not as a late fix.

### 7. Generated characters were costed by the frame, not by the game

The sprite route's real cost showed up only in the coverage audit:
- 4 of about 18 states covered,
- about 1-1.5 h of generation per clip with retries,
- about 60 MB of pages for one opponent.

The verdict was "feasible for one showcase opponent, not for a roster", and the game has a roster (soldiers, hounds,
a maw, a second mage). That audit should have come before the route was chosen.

**Do differently:** cost a character pipeline against the full state list of every character the game needs, before
judging one frame of one character.

### 8. Two builders shared one unified-memory machine without an operating protocol

On this ARM machine, GPU memory is system memory:
- ComfyUI with Flux and H3 loaded took about 57 GB of 64 GB.
- Orphaned generation scripts kept submitting jobs after a builder reported them stopped.
- A leftover XR simulator process held about 85 GB committed.

Two rounds were cut short by memory pressure, and one builder's "stopped" report was wrong.

**Do differently:**
- Generation and engine runs take turns under one lock with a memory gate.
- A builder's stop is verified from the process list, not from its own report.
- Host hygiene (stray simulators, model caches) is checked at session start.

### 9. Process was strong where it was mechanical, and that was not enough

What worked:
- the switch-gated looks (no regressions to the default build);
- the shared hand-off log with cherry-pick lines;
- the blind panels with in-set controls;
- the D-G4 budget runs;
- the suite as a gate.

The image-based route also gave real wins:
- **The projected painted backdrop:** cheap and stable in stereo.
- **Drawn fire and ward flipbooks.**
- **The sprite wiring:** half a day, with the kernel untouched.

None of that machinery could supply taste. The gap the owner saw was art-direction quality, and in this setup the
only source of taste was a generative model steered by prompts and judged by other models.

**Do differently:**
- Treat the human (owner or hired artist) as the art director in the loop from round one, with paint-overs or
  marked-up frames as the steering signal.
- Let agents do the production work around that direction.

### 10. The knowledge existed, and the loop did not pull it in at the moment it mattered

The ai-registry already held the techniques this push lacked:
- an approved reference sheet (one anchor that every later asset is made against);
- "no gate self-certifies" (a builder does not grade its own round);
- declaring the producer per asset class (kit, script or generator, decided by a paired trial);
- named benchmark anchors per rubric level;
- the two-round stop rule.

The builders consulted the registry while planning, and the judge loop never did. The second research round
(`reports/Opus built PC games recent evidence.md` in the workspace) found the same habits in the creators who
are shipping convincing agent-built 3D games:
- a target chosen from several in-game mock-ups;
- a short style file plus one anchor asset;
- routing each asset class to kit, script or generator;
- a separate skeptical critic that compares rendered frames pairwise and is checked against the owner;
- nothing done until the running game is recorded and watched;
- winning procedures written back into skills.

They also pay for image, 3D and rigging services ($100-500 per game), where this push stayed local-only.

**Do differently:** make these the round template, not background reading. A round may not start without a
chosen target, a style file and an anchor per asset class. A round may not close without several candidates,
an isolated critic's pairwise pick, the owner's anchor, and a watched capture of the running game.

## What a revived PC version should take from this

1. **Pick the art direction by toolkit fit.**
   - Prefer directions where the hardest assets are the toolkit's strengths: flat painted cards, illustrated stills,
     a stylized look with a strong post pass applied uniformly.
   - Avoid directions that need hand-drawn line quality on animated 3D, or detailed first-person hands.
   - The PC channel drops the stereo and VR-hand constraints, which removes two of the three blockers.
2. **Run a vertical slice of the hardest assets first**, judged by the owner in motion, before any polish rounds.
3. **Calibrate the model judges against the owner** and keep the offset beside every total.
4. **Build the stop rule into the round template.**
5. **Cost characters against the full state list and roster** before choosing a character pipeline.

The research behind the next steps is in the deep-research report written for this retrospective, and the lessons
that hold for any game go to the ai-registry game-production bundle.
