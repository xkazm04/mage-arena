# Mage Arena VR

**A seated, hands-first spell duel.** Draw sigils in the air to cast, raise your palm to absorb, flick a finger to
blink. No controllers, no standing up: the whole fight happens within arm's reach of your chair.

Mage Arena VR is an entry for the **Meta VR Start Developer Competition 2026** (Gaming track, New Experience
division; entries close **18 Nov 2026**). It is built in **Unreal Engine 5.8** for **Meta Quest**, and it is played on
the desktop first: through October every hand gesture is a key that replays a recorded hand clip through the same
gesture pipeline the headset will feed in November.

> Status, 2 Oct 2026: project scaffolded, combat data pinned, hand-clip pipeline running with headless tests.
> Greybox only - art comes after the mechanics are proven.

---

## The game in one minute

You are a water mage of the Roman era, betrayed, collared and forced to fight in the arena. Each bout is short and
readable:

| Verb | Hands | Keyboard (October build) |
|---|---|---|
| **Cast** | the casting hand draws a sigil: a circle plus an inner stroke chooses one of three spell lines; a two-hand mudra seals the strongest tier | `1` `2` `3` lines, `4` mudra, `Q` quick bolt; mouse drawing also works |
| **Absorb** | raise the off-hand palm toward the threat: a 140-degree ward; raise it within 0.15 s of impact for a *perfect* absorb that refunds mana and speeds up your spell tiers | hold `Space` (or right mouse button) |
| **Blink** | flick a finger toward one of three rune pads around your seat | `A` `D` `S` |

Underneath sits a calibrated combat model shared with the other builds of the game: a **tier clock** (stronger spell
tiers unlock every 15 s, faster with perfect absorbs), **Flow** (rotating different lines builds a free *Crest*
cast), and a **threat colour language** (an element-coloured spell means absorb it, steel means dodge, a black core
means get out of the way).

A session is about ten minutes: a short teach, then three bouts - soldiers, creatures, and a duel against a Fire mage.

## One game, three channels

Mage Arena is one design delivered through separate builds, so a mechanic solved once is reused everywhere:

| Channel | Where | State |
|---|---|---|
| Desktop / TV | `apps/pc-tv/` + `packages/core/` - joining this repository from its own (TypeScript core, owns the calibrated combat data) | combat kernel calibrated by headless simulation |
| **VR** | **`apps/vr/`** (Unreal Engine 5.8) | desktop-first build in progress |
| PC | - | a possibility for later |

This is becoming **one repository for every channel** ([`docs/MONOREPO.md`](docs/MONOREPO.md)): VR has moved into
`apps/vr/`; the desktop/TV project follows. Until it arrives, VR consumes a **pinned commit** of the combat data
(`apps/vr/data/`, verified by `node apps/vr/tools/check-pin.mjs`) and never edits it in place.

## Schedule

```mermaid
gantt
    title Mage Arena VR - October on the desktop, November in the headset
    dateFormat YYYY-MM-DD
    axisFormat %d %b
    section Desktop (no headset)
    Scaffold, data pin, hand clips              :done,    w0, 2026-10-02, 3d
    Feasibility: toolchain, sigils, ward, budget :active,  d1, 2026-10-05, 7d
    Toolchain route chosen                       :milestone, m0, 2026-10-09, 0d
    Mechanics in greybox                         :         d2, 2026-10-12, 7d
    Mechanics gate                               :milestone, m1, 2026-10-18, 0d
    Art style pick (13 / 25 / 30)                :milestone, m2, 2026-10-19, 0d
    Art, animation and audio sprint              :         d3, 2026-10-19, 13d
    Design and animation checkpoint              :milestone, m3, 2026-10-23, 0d
    Content freeze                               :milestone, m4, 2026-10-25, 0d
    Desktop core gate                            :milestone, m5, 2026-10-31, 0d
    section VR (device)
    Device bring-up                              :         v1, 2026-11-01, 4d
    Real hands: re-record and re-tune            :         v2, 2026-11-05, 4d
    Hands gate                                   :milestone, m6, 2026-11-08, 0d
    Beta, release candidate, playtests           :         v3, 2026-11-09, 5d
    Store upload freeze                          :milestone, m7, 2026-11-16, 0d
    Video, form, submission                      :         s1, 2026-11-14, 5d
    Competition deadline                         :milestone, m8, 2026-11-18, 0d
```

| Date | Gate | Passes when |
|---|---|---|
| Fri 9 Oct | **Toolchain route** | Meta XR v207 plugins load in UE 5.8 (or a fallback route is chosen), an APK packages, the build boots in the Meta XR Simulator Quest 3 profile |
| Sun 11 Oct | **Desktop feasibility** | clips, sigil recognition on mouse and synthetic paths, ward timing and the performance budget measured on the desktop |
| Sun 18 Oct | **Mechanics gate** | wave 1 plays end to end in greybox on clips and mouse alone; 20 conformance vectors match the desktop/TV kernel; sigils >= 95 % for the owner and >= 85 % for a second person, <= 1 false cast per minute |
| Mon 19 Oct | **Style pick** | one of the shortlisted styles - Screen-Print Poster, Moonlit Silver Nocturne, Chalk & Slate - chosen from captures of the same duel |
| Fri 23 Oct | **Design and animation checkpoint** | the duel is fun in greybox and the first opponent animates convincingly; otherwise the plan is reconsidered |
| Sat 31 Oct | **Desktop core gate** | the full session in the chosen style within Quest budgets, two cold testers finish, the APK boots in the simulator; passing it releases the headset purchase |
| Sun 8 Nov | **Hands gate** | the October numbers re-measured on real hands |
| Mon 16 Nov | **Store upload freeze** | the release candidate is on the competition release channel |
| Wed 18 Nov | **Submission** | build, video under three minutes, and the form - 12:00 PM Pacific |

The full plan, with every gate's numbers, fallbacks and risks, is in [`docs/PROJECT-PLAN.md`](docs/PROJECT-PLAN.md).

## How it is built

The work is orchestrated: a Claude session writes task cards from the plan, AI workers implement them headless, and
the orchestrator re-runs every acceptance check itself before committing. Creative and judgement calls stay with the
owner.

| Work | Who |
|---|---|
| Implementation from task cards | Grok (`grok-4.7`) first; Gemini via the Antigravity CLI as second worker and reviewer |
| Images (glyphs, UI, concept art) | Gemini image generation |
| Music and trailer clips | the owner, from prompt sheets |
| Sound effects | ElevenLabs |
| Game feel, art direction, animation acceptance | the owner, with the orchestrator |

Task cards and their status live in [`apps/vr/tasks/`](apps/vr/tasks/); the protocol is [`docs/ORCHESTRATION.md`](docs/ORCHESTRATION.md).

| Card | What | State |
|---|---|---|
| T01 | Pin the combat data with a checksum gate | done |
| T02 | Hand clips: schema, 27 synthetic clips, key-driven replay, headless tests | done |
| T03 | Sigil recognition on mouse and clip paths | next |
| T04 | Ward detection and the perfect-absorb window | next |
| T05 | Meta XR toolchain route | next |

## Repository layout

```
apps/
  vr/                         the VR channel
    Game/                     Unreal project (MageArenaVR.uproject, C++ module MageArenaVR)
      Source/MageArenaVR/     Hands/ (clips, hand input), Gestures/ (sigils), Tests/ (MageArena.*)
      Clips/                  hand clips (JSONL), generated by apps/vr/tools/clipgen
    data/                     combat data pinned from the desktop/TV build (PINNED.json + pinned/)
    tasks/                    task cards for the AI workers, and the backlog
    tools/                    build.ps1, clipgen/, check-pin.mjs
  pc-tv/                      (coming) the desktop/TV channel
packages/core/                (coming) the shared TypeScript combat kernel and data
docs/                         decisions (all channels), plans, design docs, storyboard, orchestration
tools/                        worker dispatchers (grok-run.sh, agy-run.sh) and the worker guard
```

## Build and test

Requirements: Windows 11, Unreal Engine 5.8 (`C:\Program Files\Epic Games\UE_5.8`), Visual Studio 2022, Node 20+.

```powershell
powershell -NoProfile -File apps/vr/tools/build.ps1                     # editor target, Win64 Development
node apps/vr/tools/check-pin.mjs                                         # pinned combat data is intact
node apps/vr/tools/clipgen/generate.mjs; node apps/vr/tools/clipgen/validate.mjs apps/vr/Game/Clips
& "C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" `
  apps\vr\Game\MageArenaVR.uproject -ExecCmds="Automation RunTests MageArena;Quit" -unattended -nullrhi -nosplash -log
```

Then open `apps/vr/Game/MageArenaVR.uproject` and press Play: the number keys, `Q`, `Space` and `A`/`D`/`S` replay hand clips
(hold `Shift` for a sloppy hand, `Ctrl` for a slow one).

## Art boards

Everything produced for the art direction so far - style studies for the 19 Oct pick, VFX concepts, the session storyboard, model and environment references, exact glyphs and the wrist HUD, and the SFX starter pack - is linked from one page: [`apps/vr/art/index.html`](apps/vr/art/index.html) (open from disk).

## Documents

- [`docs/DECISIONS.md`](docs/DECISIONS.md) - the owner's decisions, newest first; they outrank everything else
- [`docs/PROJECT-PLAN.md`](docs/PROJECT-PLAN.md) - scope, gates, schedule, tooling, risks
- [`docs/DESKTOP-INPUT.md`](docs/DESKTOP-INPUT.md) - how hand gestures are played from the keyboard and mouse
- [`docs/CLIP-SCHEMA.md`](docs/CLIP-SCHEMA.md) - the hand-clip format shared by synthetic clips and device recordings
- [`docs/ART-STYLE-STORYBOARD.md`](docs/ART-STYLE-STORYBOARD.md) - 30 art-direction studies and the shortlist
- [`docs/ORCHESTRATION.md`](docs/ORCHESTRATION.md) - how the AI workers are directed and verified

## License

No license is granted yet: all rights reserved. The setting, characters, names and designs are original to the Mage
Arena project.
