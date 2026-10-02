# Owner decisions

Newest first. Quotes are the owner's words; the lines under them are what the plan does about it.

## 2026-10-02 - Who does the work, what may be paid for, and the real risk

> "For music and effects use ElevenLabs services ... other paid tools not allowed without specific request and
> reasoning, like for example Tripo credits for 3D generation and animations if no other choice."

- Music and SFX: **ElevenLabs**, key `ELEVENLABS_API_KEY` (the owner pointed at the personas `.env`; the variable
  actually lives in `kiro/pof/.env`, which is pof's audio provider config). Read it by name; never copy it into this repo.
- **No other paid tool** without a written request and reasoning to the owner first. MiVRy (paid recognizer licence)
  is therefore out: the recognizer is the in-house $Q port. Tripo-style 3D/animation credits only on request, when
  there is no other choice.

> "Risk is in the game design and animations of 3D models - that's something we did not prove to master for a long
> time. I believe we don't overcome it in next 3 weeks, then we reconsider the plan."

- **Top risk: game design quality and 3D model animation**, not the toolchain. **Reconsider checkpoint Fri 23 Oct**
  (three weeks from this decision): if fun-in-greybox and animated 3D opponents are not proven by then, the plan is
  reconsidered with the owner rather than pushed on.

> "For execution use Grok CLI and Grok 4.7 model to do most of the heavy lifting ... it can do most work if you keep
> yourself as orchestrator and provide detailed plan. We then take over together once more challenging and creative
> tasks will remain, keeping Grok as pawn until usage depleted."

- **Execution model:** Grok CLI (`grok-4.7`) implements; the Claude session orchestrates with detailed task cards,
  verifies every result itself and commits. Creative and hard calls (game feel, design judgement, art choice,
  anything Grok fails twice) go to Claude with the owner. Grok runs until its usage is depleted. Protocol:
  `ORCHESTRATION.md`.

## 2026-10-02 - Desktop first, VR from November

> "Headset not decided yet, we will need to have PC experience saving hand gestures under keybindings as quick
> actions. For emulators/simulators we just pick one device most used/popular to test against. Once the game core
> plays well and art/animations resolved, I can invest into any device to port and experiment - probably Meta Quest 3
> 512GB."

- October is a **desktop build** of the same Unreal project. Every hand gesture is reachable from the keyboard and
  mouse (see `DESKTOP-INPUT.md`), through the SAME gesture pipeline the headset will feed later.
- Simulator/emulator target: **Meta Quest 3** profile only.
- No headset is bought, and no on-device gate exists, before the core game plays well and art and animation are resolved.

> "No to WebXR pivot, I would say the core game design VR-less should take until the end of October. We will know by
> this date whether gameplay, graphics, models and animations, audio on high quality level and we can invest full
> speed into VR."

- The WebXR pivot is **rejected**. The engine is Unreal.
- **Gate on 2026-10-31 (desktop core gate):** gameplay, graphics, models, animation and audio at high quality, VR-less.
  Only after it does the VR port start at full speed (1-18 Nov, competition deadline 2026-11-18).

> "Start program not yet - but I will apply."

> "Create in ai-registry subtree in game development specific to Mage Arena - it is game we work in parallel on TV
> platform too, with potential to become PC game in the future. Once one channel masters certain mechanic other can
> benefit so the same game design problems are not repeated."

- Three channels of one game: **TV/desktop** (`kiro/mage-arena`), **VR** (this repo), **PC** (future). A mechanic
  solved in one channel must be reusable by the others.
- Placement chosen by the owner the same day: **hybrid**. Generic lessons -> `game-production` subjects (no product
  names); channel realisations -> `applications/mage-arena-<channel>--<technique>.md`; game facts -> registry
  `memory/` namespace `mage-arena` (seeded in registry commit `3f35c804`, including the mechanic-by-channel matrix).

> "Lets scaffold repo in kiro/mage-arena-vr."

## 2026-10-02 - Art comes last

> "I shortlisted 13, 25, 30 to have in mind throughout the project. Starting style-less with any baseline wireframe
> convenient to prove the game mechanics. If all aspects of the game will work together, then we spend effort to
> finetune the graphical side and art, as technical limitations will enforce us to prefer simplistic styles over
> realism and high fidelity."

- Greybox until the mechanics work together. Shortlist: **13 Screen-Print Poster, 25 Moonlit Silver Nocturne,
  30 Chalk & Slate** (`ART-STYLE-STORYBOARD.md`). The PC game's Tessera & Lime is not inherited.
- The greybox still honours the threat colour language (element colour = absorb, steel = dodge, black core = leave).
