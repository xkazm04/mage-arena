# Orchestration: Claude plans and verifies, Grok builds

Owner decision 2026-10-02 (`DECISIONS.md`): Grok CLI on `grok-4.7` does most of the implementation; the Claude
session is the orchestrator. Creative and hard work comes back to Claude and the owner. Grok runs until its usage is
depleted.

## Roles

| Role | Does | Never does |
|---|---|---|
| **Owner** | decides; certifies feel, fun, art and anything only hands or eyes can judge | - |
| **Claude (orchestrator)** | writes task cards from the plan; dispatches Grok; **verifies every result by running the acceptance commands itself**; commits; updates the plan, the decisions and the registry matrix; takes the creative and hard tasks with the owner | accepts a Grok "done" without running the checks |
| **Grok (`grok-4.7`, worker 1)** | implements one task card in this repo; runs the card's checks; writes `runs/<task>/REPORT.md` | commits, pushes, edits `data/` pinned files except in a pin task, touches other repos, calls paid services, changes `docs/DECISIONS.md` |
| **Gemini via Antigravity CLI (`agy`, worker 2)** | same contract as Grok, dispatched with `tools/agy-run.sh`; also **code review** of Grok's output and **image generation** (concept art, glyphs, UI, textures) on the AI Ultra seat | everything Grok never does; settings allow-list + `.agents/hooks.json` guard enforce it |

## Worker routing (owner decisions 2026-10-02)

| Work | Worker | Why |
|---|---|---|
| Implementation from a card (C++, tools, data, tests) | Grok `grok-4.7` first | the bulk budget; proven on T01, T02 |
| Second implementation or a card Grok failed once | `agy` with `gemini-3.8-flash-high`; hard ones `gemini-3.1-pro-high` | a different model family breaks a repeated failure |
| Review of a finished card before the orchestrator commits | `agy` `gemini-3.8-flash-high`, read-only prompt | cheap second pair of eyes (it found a real replay bug in T02) |
| Images: concept art, sigil/rune glyphs, UI, texture drafts | `agy` built-in image tool (1024x1024, ~30 s) | headless and covered by the subscription |
| Music loops, trailer clips | the **owner** in Flow Music / Flow (browser only); Claude writes prompt sheets | no headless route on the subscription |
| SFX | ElevenLabs (`ELEVENLABS_API_KEY`) | Google has no SFX tool |
| 3D models, rigging, animation | Claude + owner; fallback Meta Asset Library; paid credits only on request | no subscription covers it (risk R0) |

The Gemini **API** (Veo, Lyria, Imagen by call) is metered even with AI Ultra: it is a paid tool and needs the owner's
written approval. Quota is shared across `agy` models and subagents; keep fan-out low. Print mode reports SUCCESS
even when tools were denied - `tools/agy-run.sh` records `denied_actions`, and a non-zero count is a failed run.

## The loop

1. **Card.** Claude writes `tasks/<id>-<slug>.md` from `tasks/_TEMPLATE.md`: goal, context files to read, exact
   deliverables (paths), constraints, acceptance commands with expected output, and what to put in the report.
2. **Dispatch.** `bash tools/grok-run.sh tasks/<card>.md` runs Grok headless in this repo and stores its JSON result,
   stderr and timing under `runs/<id>/` (git-ignored).
3. **Verify.** Claude reads `runs/<id>/REPORT.md` and `git status`, then runs the acceptance commands itself. A claim
   with no command behind it is not accepted.
4. **Settle.** Pass: Claude commits only the card's paths with message `<id>: <summary>`, ticks the card's status line.
   Fail: one retry with a correction appended to the card (`## Retry 1`). A second failure escalates to Claude + owner,
   and the card records why Grok could not do it (that is evidence for the registry's autonomy-coverage ledger).
5. **Record.** Mechanics proven by a task update the registry matrix
   (`ai-registry/memory/semantic/mage-arena-mechanic-channel-matrix.md`) at the next checkpoint, not per task.

## What goes to Grok and what does not

| Grok (heavy lifting) | Claude + owner (creative and hard) |
|---|---|
| C++ ports of specified rules with test vectors; data pins and converters; clip schema, generators and replay; build, package and profiling scripts; greybox levels from a written layout; test harnesses; Niagara/material setup from a spec; ElevenLabs batch generation from a written cue sheet | game-feel tuning; deciding what is fun; onboarding beats; animation direction and acceptance; art-style pick and art acceptance; anything needing a headset; choices the plan leaves open |

## Budgets and limits

- Grok usage: the owner's plan; run until depleted, then Claude takes the queue. Each card states a `max-turns`.
- ElevenLabs: allowed for music and SFX from a written cue sheet; a card that calls it states the expected number of
  generations. Key: `ELEVENLABS_API_KEY` from `kiro/pof/.env`, read by name at run time, never written to this repo.
- Any other paid service: not allowed without the owner's written approval (`DECISIONS.md`).
- Unreal builds: one at a time (`-WaitMutex`); a card never cleans shared engine caches.
