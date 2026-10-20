---
id: 0006
kind: failed-approach
scope: path:tools
date: 2026-10-05
supersedes: null
refs: []
---

Tried: running Gemini reviews through the Antigravity CLI with `--mode plan` alone, expecting a read-only review.
Symptom (T10 review, 2026-10-03): the review edited a test and ran a build. Plan mode does not stop agy from editing or
building, so `tools/agy-review.sh` sets `AGY_READONLY=1` and the PreToolUse guard (`tools/agy-guard.mjs`, wired by
`.agents/hooks.json`, tested by `node --test tools/agy-guard.test.mjs`) denies every tool that is not a read. A second
trap: agy print mode reports SUCCESS even when tools were denied, so `tools/agy-run.sh` counts `denied_actions` and a
non-zero count is a failed run. Do not trust a worker's own status line; read its denials and run the card's checks.
