# Security scan 2026-10-22-b: the shared Leonardo client and its two CLIs

Third run of the `codebase-security-scan` charter. Lynx (Windows ARM64, no Unreal). Base `6684617`, branch
`autopilot/codebase-security-scan-3dc9d800`. Earlier scan the same day: `docs/research/SECURITY-SCAN-2026-10-22.md`
(L1, G2, D1, E1, M1, R1). Its findings are not repeated here, only their status.

Why now: since that scan, `b248609` moved both Leonardo CLIs onto one shared client
(`apps/vr/tools/assets/leonardo-client.mjs`), and `1d6f03f` changed the agy worker guard. No scan had read the
merged code.

Scope: `apps/vr/tools/assets/leonardo-client.mjs`, both of its callers (`assets/leonardo.mjs` and
`keyart/leonardo-keyart.mjs`, the only importers found by `grep -rl leonardo-client apps/vr/tools tools`),
`tools/agy-guard.mjs` at `1d6f03f`, and what they write: `leonardo-log.json` beside the assets, the downloaded
images, and key art's `manifest.json` and `index.html`. The ElevenLabs scripts have not changed since the earlier scan
(`git log 6fbf478..HEAD -- apps/vr/tools/sfx apps/vr/tools/music` is empty), so its results for them still hold. Method:
code reading, plus offline runs of the CLIs with a fake key, a stubbed `fetch` and a preload that hides every real log.
No `.env` was opened, no key value was read or printed, and no live API call was made.

Line numbers are for the fixed tree unless a line says "base". Base is `6684617`.

## What matters

1. **No key is in the tree or in the history since the earlier scan.** The earlier scan's pattern over
   `git log 6fbf478..HEAD -p` gives 0 hits, and `git grep` over the tree gives 0. The one Leonardo log added since then
   (`apps/vr/art/characters/warden/leonardo-log.json`, `c03aa44`) has 1 call, no `error` field, and no
   secret-like field or word. Its only UUID is its `generationId`. Nothing needs rotating.
2. **The 400-credit night cap was only as strong as the CLI that ran (S1, medium, fixed).** The shared client exports
   `NIGHT_CAP` and `nightTotal`, but `call()` enforces nothing. Each caller checks the caps itself. The key-art CLI
   summed only its own tree's logs. Spend logged tonight from the main checkout or a sibling worktree did not count, so
   the cap could be passed.
3. **A billed call could be left with no ledger row (S2, medium, fixed).** The row was written only after the call
   ended, up to 240 s of polling later. A run killed in that window, or a second run started during it, spent credits
   that neither the cap nor the ledger could see.
4. **The redaction added for L1 could be bypassed by JSON escapes (S3, low, fixed).** `call()` redacted the raw body,
   but `JSON.parse` decodes `\uXXXX` escapes after that, so a key the API spelled escaped came back as plain text.
5. **Image downloads were trusted (P1, low, fixed).** A CDN error page was saved as the image and the run still reported
   COMPLETE. The file also took any extension the response URL had.
6. **The agy guard still lets a worker read the key (G3, low-medium, open).** It checks only a command line. A worker's
   file-read tool, a glob such as `.e*` and an environment dump all pass it. The file is outside this task's paths.

## Findings, by real severity

| # | Severity | Where | What | Status |
| --- | --- | --- | --- | --- |
| S1 | Medium (spend) | base `keyart/leonardo-keyart.mjs:98` | `nightTotal = () => nightTotalOf([ART], today())` covered only this tree's `apps/vr/art`. `assets/leonardo.mjs` (base `:182-190`) also scanned the main checkout and every sibling worktree. Headless builders run in sibling worktrees, so a key-art run there saw only its own logs and could take the project past 400 credits in one night. | **Fixed in `895a065`.** The checkout list is now `nightArts(root)` in the shared client (`leonardo-client.mjs:111-123`). Both CLIs call it (`assets/leonardo.mjs:181`, `keyart/leonardo-keyart.mjs:100`). |
| S2 | Medium (spend) | base `assets/leonardo.mjs:269`, `keyart/leonardo-keyart.mjs:181` | The only `writeLog` came after the POST and the poll loop. Between the billed POST and that write: (a) Ctrl-C, a kill or a crash left a paid generation with no row, and a rerun spent again; (b) a second run, from either CLI or any checkout, read totals without this call and passed the caps. | **Fixed in `582e547`.** A `PENDING` row at the expected cost (`creditsSource: "reserved"`) is written before the first request (`assets/leonardo.mjs:226`, `keyart/leonardo-keyart.mjs:149`). The final write replaces it. A leftover `PENDING` row means the run died; it still counts, at the expected cost. Residual risk: see R2. |
| S3 | Low (secret) | base `leonardo-client.mjs:36-38` | `redact` ran on the raw body. A body with the key written as `\u0066\u0061…` has no literal copy. `JSON.parse` then decoded it, and `JSON.stringify(created).slice(0, 300)` in the "no generationId" error (`assets/leonardo.mjs:233`, `keyart/leonardo-keyart.mjs:156`) cut it in half, beyond the catch's exact-match redaction. A fragment reached stdout and the committed log. As with L1, this needs the API to echo the caller's key, which Leonardo is not known to do. | **Fixed in `8d00097`.** After the parse, `call()` checks for the key again in the parsed value and redacts it (`leonardo-client.mjs:39-40`). |
| P1 | Low (integrity) | base `assets/leonardo.mjs:252-260`, `keyart/leonardo-keyart.mjs:164-172` | The image URL from the API response was fetched with no `res.ok` check. A 403/404 body was written as `concept-….jpg` and logged COMPLETE with its byte count, ready to be committed. `ext` came straight from the URL path, so `.html` or `.mjs` files could land in `apps/vr/art/`. Plain `http:` was accepted. No credentials were sent and the path could not leave the folder (`path.extname` cannot contain a separator). | **Fixed in `b90dae7`.** The shared `saveImage(url, dir, name)` (`leonardo-client.mjs:72-91`) requires `https:` and an image extension (`.png .jpg .jpeg .webp`) and throws on a non-2xx. It never puts the URL, which may be signed, in an error. A run without its file now exits 1 (`assets/leonardo.mjs:260`, `keyart/leonardo-keyart.mjs:183`). |
| G3 | Low-medium (secret) | `tools/agy-guard.mjs:8`, `:23` | `1d6f03f` fixes G2 for the cases it lists. But the guard reads only `args.CommandLine`/`args.command`. The following all return `allow` (checked by piping each tool call into the guard; nothing was read): a file-read tool call such as `{"name":"view_file","args":{"AbsolutePath":"C:/Users/kazda/kiro/bp/.env"}}` (the command is empty, so no rule runs); `type C:\Users\kazda\kiro\bp\.e*` (a glob, so the `\.env\b` rule misses it); `Get-ChildItem env:` and `printenv` (they dump every exported variable). | Open: outside this task's paths. Fix: test the rule against `JSON.stringify(call.args ?? {})` as well as `cmd`. Add `/\benv:|\bprintenv\b|^\s*(set\|env)\s*$\|\.e[*?]/i` to the secrets rule. Add the four inputs above as deny cases in `tools/agy-guard.test.mjs`, building the read-tool case's `toolCall` by hand. A regex guard stops mistakes, not a determined worker: the real control is that the key lives outside every worktree. |
| R2 | Low (spend, residual) | `assets/leonardo.mjs:204-226`, `keyart/leonardo-keyart.mjs:126-149` | After S2, the gap between the cap check and the `PENDING` write is only `readKey()` plus a JSON write, a few milliseconds. Two runs started in the same instant can still both pass. | Accepted. Fix if wanted: take a lock with `fs.openSync(path.join(os.tmpdir(), 'mage-arena-leonardo.lock'), 'wx')` around check-and-reserve. Retry while it exists for less than 5 s, and break it when it is older than 300 s. |
| C1 | Low (spend) | `leonardo-client.mjs:12`, `:57-70`; checks at `assets/leonardo.mjs:206-213`, `keyart/leonardo-keyart.mjs:128-135` | The caps are checked against the expected cost `COST[quality]`, and `HIGH: 60` is a guess. A call that costs more than expected overshoots the cap by the difference. `reportedCost` takes the first numeric field named like `cost`/`credit` anywhere in the response, up to depth 4. A balance-like field (`…Credits: 3000`) would be logged as the cost: an over-count, the safe direction. A `credit: 0` field would be an under-count. | Report only. Fix: refuse `HIGH` until one call has measured it (`creditsSource` `api` or `balance`). In `reportedCost`, accept only names that match `/cost$/i` or `/^(api)?credit(s)?cost/i`. |
| N1 | Low (spend) | `assets/leonardo.mjs:250-257`, `keyart/leonardo-keyart.mjs:173-180` | A POST that fails on the network after Leonardo accepted it (no response, so no `generationId`) is logged with `credits: 0` (`no-generation`), unless the balance moved by the time of the second `balance()` call. The generation may still be billed. | Report only. Fix: when the POST threw without an HTTP status (the message does not contain `-> HTTP`), keep the expected cost with `creditsSource: 'unknown'` rather than 0. |
| U1 | Info (spend, owner's call) | `leonardo-client.mjs:126` | "Per night" is the UTC day. UTC midnight is 01:00 CET / 02:00 CEST, so a session from 22:00 to 03:00 Prague time has two 400-credit budgets. | Owner decision: what is "a night"? If it should be one session, key the day on local time minus 12 h. |
| H1 | Info | `leonardo-client.mjs:79-83` | After P1 the image URL must be https with an image extension, but its host is not pinned. This repo records no CDN host (the logs keep no URLs), and pinning a guessed host would make the next paid call fail at the download. | Fix after one observed call: add `if (!u.hostname.endsWith('.leonardo.ai')) throw …` (or the host the call shows) to `saveImage`. |
| D1 | Info (carried) | `.ai/manifest.yaml:48` | `secretsFrom` still names `ELEVENLABS_API_KEY` from pof's `.env`, and does not name `LEONARDO_API_KEY`. | Still open, outside this task's paths. Fix as in the earlier scan. |
| T1 | Info (gate) | `.ai/manifest.yaml:33` | `apps/vr/tools/keyart/leonardo-redaction.test.mjs` now proves L1, S1, S2, S3 and P1, but the merge gate's `test` command does not run it. | Recommendation, outside this task's paths: add `&& node --test apps/vr/tools/keyart/leonardo-redaction.test.mjs` to `quality.test`. It is offline, needs no dependencies and takes about 3 s. |

Earlier findings: **L1** stays fixed (the client still redacts the raw body at `leonardo-client.mjs:36`, and S3 extends
the fix). **G2** is closed by `1d6f03f` (`tools/agy-guard.mjs:23`, 34/34 guard tests pass). **R1** is narrowed by S3: the
JSON-escape form is now covered, and URL or base64 forms are still accepted, for the reasons the earlier scan gave.
**E1** and **M1** concern the ElevenLabs scripts, which have not changed, so the earlier scan's status for them stands.

## The four questions, against the merged shape

**(a) Can `LEONARDO_API_KEY` reach output?** `readKey()` (`leonardo-client.mjs:23-27`) reads bp's `.env` only. It is
called only after both caps pass. Its error names the variable and the file, not a value. The key goes only into the
`authorization` header of `call()` (`:32`) for the fixed `API` origin (`:10`). `saveImage` sends no headers. The
paths a key could take to output:

- **2xx body.** Redacted raw (`:36`) and parsed (`:40`). Every value the CLIs log is taken from the parsed value:
  `generationId`, `status`, the URL and the cost.
- **Non-2xx body.** The thrown message is built from the redacted value (`:41`) and redacted again in each CLI's catch.
- **Thrown `fetch` error.** Undici's `fetch failed` holds no header. An invalid header value would be echoed in
  undici's message, but the exact key is in that message, so the catch's `redact` removes it.
- **`balance()`.** It swallows every error (`:51-53`).
- **Retry.** Neither CLI retries. The poll loop only repeats a GET, through `call()`.
- **Stack trace.** After the key is read, the only code outside the `try` is `balance` (silent), `writeLog` (an fs error
  holds a path) and `console.log` of the logged fields.

The `.env` is never cached, and the key is never in a URL, a cache key, the manifest or `index.html`. Since the
fixes, the redaction test covers both CLIs with the key echoed raw, in JSON, and JSON-escaped across every truncation
boundary. **ELEVEN_LABS_API_KEY**: no change since the earlier scan.

**(b) Is the cap enforced by the shared client?** No, and it still is not: the client holds the shared parts
(`NIGHT_CAP`, `nightArts`, `nightTotal`), and each CLI checks them before `readKey()`. Moving the check into `call()`
would also block the status GETs of a generation that has already been billed. The gaps were in coverage, not in where
the check lives: S1 made both CLIs count the same set of logs, and S2 made a call visible to the caps before it is
billed. Double spend by a second CLI: only the millisecond race R2 is left. Double spend by a retry: there is no
automatic retry. A manual rerun now sees the dead run's `PENDING` row.

**(c) Risky patterns.**

- **Paths from API responses:** P1, fixed.
- **Shell interpolation:** none. Neither CLI spawns a process. `--out` must be a bare file name (`path.basename`
  check). `--id`, `--asset` and `--view` are looked up in fixed tables.
- **Unpinned remote URLs:** the API origin is a constant. The CDN host is H1.
- **Output escaping:** key art's `index.html` escapes every log-derived string through `esc`. `LABEL` is a constant.

## Proof for each fix

Run `node --test apps/vr/tools/keyart/leonardo-redaction.test.mjs`. It is the earlier scan's harness, extended with
three changes:

- The preload serves the main checkout's path from the temp tree, so a case can plant a log there and nothing real is
  read.
- `setTimeout` runs at once, so there is no 5 s poll wait.
- The `fetch` stub has one mode per case.

Each new case was run before its fix and failed:

| Fix | Case (both CLIs unless stated) | Before the fix | After |
| --- | --- | --- | --- |
| S3 `8d00097` | `nogen-escaped`: a 200 without a `generationId`, with the key written as `\u` escapes across the 300-character cut | `✖ … nogen-escaped …: key fragment fakekey0 leaked` (both). `json-escaped` passed and stays as a guard: the outer 500-character cut already drops the key. | pass |
| S1 `895a065` | `night`: the main checkout holds a log dated today with 398 credits, and a LOW call costs 6 | `leonardo-keyart.mjs` ✖: it made the call and exited 1 instead of 2. `leonardo.mjs` passed: it already scanned the main checkout. | both exit 2, `REFUSED` |
| S2 `582e547` | `crash`: the stub ends the process at the POST | ✖ for both: no log existed | 1 row, `PENDING`, 6 credits |
| P1 `b90dae7` | `cdn404`: the CDN answers 404. `badext`: the URL ends `.html` | ✖ for both: `concept-front-s7-low.jpg` / `key-art-s7-low.jpg` (the 404 body) and `….html` were written | no file, exit 1, `error` names the image |

Final run: `ℹ tests 18`, `ℹ pass 18`, `ℹ fail 0`.

## Gates on the fixed tree

| Command | Result |
| --- | --- |
| `node apps/vr/tools/lint/unused-members.mjs && node apps/vr/tools/lint/format-check.mjs` | `unused-members OK: 555 …`; `format-check OK: 0 changed C++ file(s) since 6684617` |
| `node --test tools/agy-guard.test.mjs` | 34 pass, 0 fail |
| `node apps/vr/tools/check-pin.mjs` | `pin OK: 9 files @ baeac66`, `conformance OK: 45 vectors` |
| `node apps/vr/tools/check-pin.mjs --self-test` | exit 0 |
| `node --test apps/vr/tools/keyart/leonardo-redaction.test.mjs` | 18 pass, 0 fail |
| `node tools/check-commits.mjs` | OK |
| `leonardo-keyart.mjs total`, `leonardo.mjs night` (read only, no key) | both report `14` credits tonight; before S1 they could disagree |
| typecheck | none configured |

No Unreal build or suite was run, and none is claimed. No Leonardo call was made: the fixes are proven offline, and
their first live run will be the operator's next generation.
