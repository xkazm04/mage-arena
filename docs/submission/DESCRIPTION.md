Status: draft, for the owner

# Submission description and tagline

Items 2 (description) and 3 (tagline) of the K6 list (`docs/research/KPI-2026-10-20.md` K6). The required parts are
inspiration, how it was built, future plans and a target launch date (`docs/PROJECT-PLAN.md` section 2, "Description").
The overview page suggests 500 words or less; that is a suggestion, not a hard cap (same section).

The text between the two rules is what the owner pastes into the form. It carries no citations; the sources are in the
table below it. A [confirm] marker is something only the owner can state. Nothing here claims an APK, a device run or a
video: none is on record (`docs/XR-TOOLCHAIN.md:9`, no APK has been produced). The owner finalises.

## Tagline

> Sit down, raise your palm and draw spells in the air. A hands-only duel of reading and timing. No controllers.

Length: 110 characters, spaces included (limit 140, `docs/PROJECT-PLAN.md` section 2, "Tagline").

## Description (paste-ready draft)

---

**Inspiration.** Mage Arena is a duel in a Roman-era arena of elemental mages. We wanted to cast spells with our hands,
not with buttons. You sit, raise a palm to ward off a spell, and draw a circle and a bar in the air to cast.

We pitch it as a strategy duel of reading and timing, not a twitch game. Every threat has a colour language. An element
colour means absorb it with your ward. Steel means blink away. A black core with a red rim means leave the pad.

**What you do.** Three verbs and a seal. Bolt is an open hand and a quick pinch. Line casts are drawn: a circle arms the
cast, then a bar picks the spell. The ward is an open palm held up, and a perfect block comes if you raise it just before
impact. Blink is a flick toward one of three rune pads. A tier clock on the wrist unlocks stronger spells as a bout goes
on, and Flow rewards varied casting. You play seated, and threats only come from the front, so you never turn around.

**How it was built.** Unreal Engine 5.8 and C++. We built the whole game on a desktop first. Every hand gesture is a
recorded hand clip that the keyboard plays into the same gesture pipeline the headset uses, so the recognizer and the
detectors are tested on the stream they will see on a Quest. The combat rules are a deterministic kernel, with data
shared with a sibling TV version of the same game, pinned to one commit and checked against 45 test vectors. Task cards,
automated tests and an agent-driven toolchain check builds, recognizer accuracy and balance without a headset. Sound
effects and music are generated with ElevenLabs. [confirm: final list of generated and authored assets]

**Where it stands.** The combat core, the gesture pipeline and the first sound effects run on desktop. The art style,
the animated opponent, the music loops and the Quest build are still to come. [confirm: rewrite this paragraph on the
submission day to match the real state of the build]

**Future plans.** The 18 November build is one day of an apprentice's tournament: a teaching bout, soldiers, creatures,
a named rival and a final against a second school, Air. After it come all four schools (Water, Fire, Earth, Air) as
playable, each with its own seated defence, the other story days and routes, and later ghost duels against recorded play.

**Launch date.** [confirm: target date, Q5, owner decides]. The plan's proposed default is a store release in spring
2027, aligned with VR glasses availability. The game is free of charge.

---

Word count of the text between the rules: 444 words with the [confirm] markers, so about 55 words are free for the
owner to add once the build state and the launch date are known. Counted by splitting on whitespace.

## Sources for each claim

| Claim | Source |
|---|---|
| Roman-era arena of elemental mages; the wish to cast by hand | `docs/PROJECT-PLAN.md` section 11, "Description outline" 1 |
| Strategy duel of reading and timing | `docs/PROJECT-PLAN.md` section 2, "Genre fit" |
| Colour language: element = absorb, steel = dodge, black core with red rim = leave | `CLAUDE.md` "Current phase"; `docs/PROJECT-PLAN.md` section 6.3 |
| Bolt, line casts (circle then bar), ward, perfect window, blink, pads | `docs/PROJECT-PLAN.md` section 6.2; `docs/DECISIONS.md` 2026-10-03 "Sigil shape" (circle + bar) |
| Tier clock on the wrist, Flow | `docs/PROJECT-PLAN.md` section 6.3 |
| Seated, threats from the front only | `docs/PROJECT-PLAN.md` section 6.1 |
| Unreal Engine 5.8, C++ | `CLAUDE.md` (title paragraph) |
| Desktop first; keyboard plays clips into one gesture pipeline | `docs/DESKTOP-INPUT.md` "The rule"; `docs/DECISIONS.md` 2026-10-02 "Desktop first" |
| Deterministic kernel, shared data, pinned commit, 45 vectors | `docs/REPO-LAYOUT.md`; `apps/vr/data/README.md`; `docs/research/KPI-2026-10-20.md` K2 |
| Task cards, tests, agent toolchain | `docs/ORCHESTRATION.md`; `docs/PROJECT-PLAN.md` section 8 |
| Sound effects and music from ElevenLabs | `docs/DECISIONS.md` 2026-10-02 "Who does the work". Music is not imported yet: `apps/vr/tasks/T40-two-music-loops.md:3` (blocked on an owner input) |
| What runs now; what is still to come | `docs/research/STATE-EVAL-2026-10-20.md` section 3; `docs/research/KPI-2026-10-20.md` K3 |
| The 18 Nov slice: Tiro day, Air final | `docs/design/V2-ROADMAP.md` intro; `docs/DECISIONS.md` 2026-10-20 "Cut slice: no cut" |
| Four schools playable later, school defences | `docs/DECISIONS.md` 2026-10-07 "v2 vision accepted", 2026-10-03 "Seated balance"; `docs/design/SCHOOL-DEFENCES.md` |
| Other days and routes, ghost duels | `docs/design/V2-ROADMAP.md` "Spring 2027"; `docs/PROJECT-PLAN.md` section 6.8 |
| Spring 2027 default, owner decides | `docs/PROJECT-PLAN.md` section 11 "Launch date", section 12 question 5 |
| Free of charge | `docs/PROJECT-PLAN.md` section 2, "Free of charge" |

## Open points for the owner

- Launch date (Q5). The sibling TV channel's release state and the entrant's identity are also [confirm] items
  (`docs/submission/ORGANIZER-QUESTION-Q9.md`, "Before you post").
- "Sound effects and music are generated" is true of the approved plan, not of the repo today: 20 SFX are imported, no
  music loop is (`docs/research/STATE-EVAL-2026-10-20.md` section 3, Audio row).
- The "Where it stands" paragraph goes stale as cards land. Rewrite it last.
