# Security scan 2026-10-22-c: the shared Leonardo client after the poll/settle refactor

Fourth run of the `codebase-security-scan` charter. Lynx (Windows ARM64, no Unreal). Base `3a5ea8e`, branch
`autopilot/codebase-security-scan-eb88a565`. Earlier scans the same day: `SECURITY-SCAN-2026-10-22.md` (L1, G2, D1, E1,
M1, R1) and `SECURITY-SCAN-2026-10-22-b.md` (S1, S2, S3, P1, G3, R2, C1, N1, U1, H1, D1, T1). Their findings are not
repeated here; their open items are listed at the end with their status.

Why now: `3a5ea8e` moved the poll loop and the credit-settlement rule out of `assets/leonardo.mjs` and
`keyart/leonardo-keyart.mjs` into the shared `assets/leonardo-client.mjs` as `pollGeneration` (`:76-85`) and
`settleCredits` (`:88-93`). That is the path the S1/S2/S3/P1 fixes hardened.

Scope: `git show 3a5ea8e`, then `leonardo-client.mjs`, `leonardo.mjs`, `leonardo-keyart.mjs` and their two tests as
they stand. Method: code reading, the offline harness (fake key, stubbed `fetch`, preload that hides every real log),
and mutation runs (a shared function or a CLI line broken on purpose, the harness run, the file restored with
`git checkout`). No `.env` was opened, no key value was read or printed, and no Leonardo call was made.
No credits were spent. No Unreal result is claimed.

Line numbers are for the fixed tree (`20cf212`) unless a line says "base".

## Result in one paragraph

The refactor is behaviour-preserving for all four guarantees. `pollGeneration` and `settleCredits` handle no strings
and no key. An error thrown inside the poll still reaches each CLI's `catch`, which redacts it before cutting it
(`leonardo.mjs:245`, `leonardo-keyart.mjs:168`). The old and new settle rules are the same four branches in the same
order. There is one difference: when a poll GET throws, the row's status is now `ERROR`. Before, it kept the last API
status (for example `PENDING`). That changes the label only; the credits are the same. The harness runs the real
shared file (it copies `leonardo-client.mjs` into its temp tree), so no guarantee is proven only through a copy that
no longer exists. The scan found one real gap and two coverage gaps:

- **P2.** A 200 non-image body was saved as the image. It is fixed in `20cf212`.
- **N2 and N3.** These are harness blind spots: the sibling-worktree branch of the night cap, and the settle wiring of a
  finished call. Both are closed in `a5fd747`.

## Findings, by real severity

| Id | Severity | Where | Finding | Status |
| --- | --- | --- | --- | --- |
| P2 | Low (integrity) | `leonardo-client.mjs:109-121`; base `:102-114` | P1 checked https, the URL's extension and `res.ok`, but not the body. A CDN can answer 200 with an HTML error page at a `.jpg` URL (an expired signed link, an access-denied page). That page was written as `concept-front-s7-low.jpg` / `key-art-s7-low.jpg` and logged `COMPLETE` with its byte count, ready to be committed. Guarantee (d) says a non-image download is never saved; this one was. | **Fixed in `20cf212`.** `isImage` (`:98-101`) checks for a PNG, JPEG or WebP signature (the formats `IMAGE_EXT` admits), and `saveImage` throws before the write (`:117`). The error names the byte count, never the URL. |
| N2 | Low (spend, coverage) | `keyart/leonardo-redaction.test.mjs`, preload; `leonardo-client.mjs:146-154` | The S1 harness case planted a log only in the main checkout. For the sibling worktrees folder, the preload read the real folder and hid every log in it. So the branch of `nightArts` that counts sibling worktrees was never exercised. Headless builders run in those worktrees, and they are the reason S1 was written. A regression there would have stayed green. | **Closed in `a5fd747`.** The preload serves the worktrees folder from the temp tree; it no longer reads the real one. A new case per CLI plants 398 credits in a sibling worktree and expects exit 2 with `REFUSED`. Mutation proof: with the sibling loop of `nightArts` removed, both new cases fail. |
| N3 | Low (spend, coverage) | `leonardo.mjs:248`, `leonardo-keyart.mjs:171`; `leonardo-client.mjs:76-93` | No harness case ran a call to `COMPLETE` and saved a file. No case checked the settled credits of a row. `leonardo-client.test.mjs` proves `settleCredits` alone, but not the CLIs' `Object.assign(entry, settleCredits(…))`. If that line were dropped, rows would be logged at `credits: 0`, the caps would under-count, and every test would still pass. | **Closed in `a5fd747`.** A new `ok` case per CLI serves a PNG and expects exit 0, one saved file, `COMPLETE`, `bytes: 16`, `credits: 6`, `creditsSource: fallback`. Mutation proof: with the `fallback` branch of `settleCredits` removed, both new cases fail. With the `Object.assign` line in `leonardo-keyart.mjs` replaced by `void (…)`, the key-art case fails. |
| T2 | Info (gate) | `.ai/manifest.yaml:34`, the App Master's gate list | `leonardo-client.test.mjs` (added in `3a5ea8e`) is run by no gate. The App Master's merge gate `test` command also omits `leonardo-redaction.test.mjs`, although the manifest's `capabilities.test.command` includes it (T1). | Open: outside this task's paths. See questions. |

Nothing else was found. In particular, no path in the refactored code reaches the key; see check (a).

## The four guarantees: named checks

### (a) The key never reaches the console, a log file, an error, a thrown message or an env dump: holds

What carries a string that might hold the key:

| Path | Why it is clean |
| --- | --- |
| `call()` response body, `leonardo-client.mjs:36` | The body is redacted before it is parsed or cut. |
| After `JSON.parse`, `:40` | If the parsed value contains the key (spelled as `\u` escapes in the raw body), the parsed value is redacted again (S3). |
| `{ raw: text.slice(0, 500) }`, `:38` | It is cut from text that is already redacted. |
| HTTP error, `:41` | `${method} ${url}`: `url` is the constant `API` plus a `generationId` taken from the redacted JSON. The body is the redacted `json`, cut at 500 characters after redaction. |
| `no generationId` error, `leonardo.mjs:233`, `leonardo-keyart.mjs:156` | `JSON.stringify(created).slice(0, 300)`, where `created` is already redacted (the 300-character cut of L1/S3). |
| `pollGeneration`, `:76-85` | It builds no string. An error from `fetchGen` (which is `call()`) propagates unchanged into the CLI's `catch`. |
| CLI `catch`, `leonardo.mjs:245`, `leonardo-keyart.mjs:168` | `redact(e.message ?? e, key).slice(0, 500)`: redacted, then cut. |
| `settleCredits`, `:88-93` | Numbers and fixed labels only. |
| `saveImage` errors, `:111-117` | The protocol, the extension cut at 16 characters, the HTTP status, or a byte count. Never the URL, never the key. |
| `readKey` error, `:25` | It names the file, not the value. |
| `balance()`, `:46-54` | It swallows every error. |
| `console.log` lines, `leonardo.mjs:250`, `leonardo-keyart.mjs:173` | Status, ids, credits, file name and the already-redacted `entry.error`. |
| Env | Neither CLI reads `process.env` or prints it. Neither spawns a process. |

- **Redirects.** Node v24.22.0's `fetch` drops `Authorization` on a cross-origin redirect. A redirect within the same
  origin stays on `cloud.leonardo.ai`.
- **Code outside the `try`.** After `readKey()`, only `balance` (silent), `writeLog` (fs errors only) and
  `settleCredits` (pure) run outside it.
- **Harness.** The `json` and `raw` cases (the 500-character cut), the `nogen` case (the 300-character cut) and the two
  `-escaped` cases run for both CLIs against the shared `call()`. They check every 8-character fragment of the key in
  stdout, stderr and the log: 10/10 pass. The harness reaches this guarantee through the shared function.

### (b) The 400-credit night cap counts every checkout for both CLIs and refuses with exit 2: holds

- Both CLIs call `nightTotalOf(nightArts(ROOT), …)` from the shared client (`leonardo.mjs:181`,
  `leonardo-keyart.mjs:100`). `nightArts` covers this tree, the main checkout and every sibling worktree (`:146-154`).
  Both CLIs check `night + COST[quality] > NIGHT_CAP` and exit 2 before `readKey()`, so before any request
  (`leonardo.mjs:210-213`, `leonardo-keyart.mjs:132-135`).
- The cap now also counts settled rows correctly through the shared `settleCredits` (N3).
- Harness: the S1 case (main checkout, 398 credits) and the new N2 case (sibling worktree, 398 credits) pass for both
  CLIs: 4/4. Before `a5fd747` the sibling-worktree branch was not reached (N2).

### (c) A call is logged PENDING before its request and survives a crash mid-request: holds

- The `PENDING` row (`creditsSource: reserved`, the expected cost) is written before `balance()` and the POST
  (`leonardo.mjs:226`, `leonardo-keyart.mjs:149`). The refactor did not touch this.
- A crash inside the shared `pollGeneration` leaves the same row, because the final write comes after it.
- Harness: the `crash` case kills the process at the POST and finds one `PENDING` row at 6 credits, for both CLIs: 2/2.
  The `PENDING` write lives in each CLI, and the harness runs each CLI, so this is reached directly.

### (d) A failed or non-image download is never saved: holds after `20cf212`

- `saveImage` (shared) requires `https:`, an image extension and `res.ok`, and now a PNG, JPEG or WebP body (P2).
- A run without its file exits 1 (`leonardo.mjs:251`, `leonardo-keyart.mjs:174`).
- Harness: `cdn404`, `badext` and the new `html200` pass for both CLIs (6/6). `html200` failed for both before the
  fix: `actual: [ 'concept-front-s7-low.jpg' ]` and `actual: [ 'key-art-s7-low.jpg' ]`. The new `ok` case (2/2) proves
  that a real image is still saved. `isImage` has its own unit cases in `leonardo-client.test.mjs` (PNG, JPEG and WebP
  pass; HTML, a RIFF/WAVE body and an empty body do not).

## Earlier open items, status after this scan

- R2 (millisecond race between the cap check and the `PENDING` write), C1 (`HIGH` cost a guess, `reportedCost` too
  loose), N1 (a network-failed POST settles at 0), U1 ("night" is the UTC day) and H1 (CDN host not pinned): unchanged
  by `3a5ea8e`, still open as recorded in `SECURITY-SCAN-2026-10-22-b.md`.
- N1 now sits in the shared `settleCredits` (`:92`, the `no-generation` branch), so its fix would be one change for
  both CLIs.
- R1 (exact-match redaction only): unchanged; a key echoed only as a fragment or re-encoded would still pass through.
  This is accepted for the same reason as before.

## Commands run on the fixed tree (`20cf212`)

| Command | Result |
| --- | --- |
| `node --test apps/vr/tools/keyart/leonardo-redaction.test.mjs` | 24/24 pass (18 at base; +2 N2, +2 html200, +2 ok) |
| `node --test apps/vr/tools/assets/leonardo-client.test.mjs` | 8/8 pass (7 at base; +1 `isImage`) |
| `node --test tools/agy-guard.test.mjs` | 40/40 pass |
| `node apps/vr/tools/check-pin.mjs` | `pin OK: 9 files @ baeac66`, `conformance OK: 45 vectors` |
| `node apps/vr/tools/check-pin.mjs --self-test` | exit 0 |
| `node apps/vr/tools/lint/unused-members.mjs` | `unused-members OK: 555 private/protected members in 39 classes` |
| `node apps/vr/tools/lint/format-check.mjs` | `format-check OK: 0 changed C++ file(s)` |
| `node tools/check-commits.mjs` | OK (the subjects since `3a5ea8e`) |
| `git grep` / `git log 3a5ea8e~1..HEAD -p` for `LEONARDO_API_KEY=<value>` or `Bearer <16+ chars>` | only the harness's fake key line (`leonardo-redaction.test.mjs:46`) |
