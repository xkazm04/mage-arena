# Security scan 2026-10-22: every code path that reads an API key

Second run of the `codebase-security-scan` charter. Lynx (Windows ARM64, no Unreal). Base `6fbf478`, branch
`autopilot/codebase-security-scan-59adb914`. Baseline: `docs/research/SECURITY-SCAN-2026-10-21.md` (`58808aa`).

Why now: since the baseline, `7c2a0bb` moved the ElevenLabs key to `ELEVEN_LABS_API_KEY` in bp's `.env`, the music
generator (T40) arrived, `leonardo-keyart.mjs` added a second Leonardo client, and six `leonardo-log.json` files were
committed under `apps/vr/art/`.

Scope: `apps/vr/tools/sfx/generate.mjs`, `apps/vr/tools/music/generate.mjs` (ElevenLabs),
`apps/vr/tools/assets/leonardo.mjs`, `apps/vr/tools/keyart/leonardo-keyart.mjs` (Leonardo), the six committed Leonardo
logs, and what the ElevenLabs scripts write (`apps/vr/art/audio/music/elevenlabs-log.json`, both `manifest.json`, the
`%TEMP%\a05-sfx-ledger.json` ledger). The scan used code reading and pattern matching only. No `.env` file was opened,
no key value was read or printed, no generator ran against a live API, and no network call was made. The redaction
proof runs the Leonardo clients offline, with a fake key and a stubbed `fetch`.

## What matters

1. **No key, token or Authorization header is in the tree, the six Leonardo logs, the music and SFX logs, or the
   history** (382 commits). Nothing needs rotating.
2. **One real leak path, now fixed in both Leonardo clients (L1, low).** A Leonardo response without a
   `generationId` was cut to 300 characters before redaction ran. A key the API echoed across that cut would have
   left a fragment in stdout and in the committed `leonardo-log.json`. Fixed in `c2b29a6` and `458c6b9`.
   `apps/vr/tools/keyart/leonardo-redaction.test.mjs` proves the fix: it fails on the old code and passes now.
3. **The worker guard no longer matches the key names (G2, low-medium, open).** `tools/agy-guard.mjs:23` blocks
   `ELEVENLABS_API_KEY`, the name before `7c2a0bb`. It does not block `ELEVEN_LABS_API_KEY` or `LEONARDO_API_KEY`.
   The file is outside this task's paths, so the fix below is left to the owner.
4. Baseline G1 is **closed**. `e1323e9` added `.env`, `.env.*` and `!.env.example` to the root `.gitignore`
   (`:50-52`), and `git check-ignore -v` now matches `.env`, `apps/vr/tools/keyart/.env` and `apps/vr/tools/assets/.env`.

## Findings, by real severity

| # | Severity | Where | What | Status |
| --- | --- | --- | --- | --- |
| L1 | Low (fixed) | `assets/leonardo.mjs:321`, `keyart/leonardo-keyart.mjs:223` (before the fix `:320` and `:222`); redaction at `:343` / `:245` | `no generationId in response: ${JSON.stringify(created).slice(0, 300)}` cut the response first. The `catch` then redacted with an exact match (`redact`, `:233` / `:132`), which cannot find half a key. The leftover fragment reached `console.log` (`:351` / `:253`) and `entry.error` in the committed log. For this to happen, a 200 response must echo the caller's key, which Leonardo is not known to do. That is why the severity is low. | **Fixed in `c2b29a6` and `458c6b9`.** `call()` now redacts the body as it arrives, before any parse or cut (`:248` / `:147`). Every value derived from a response is clean from then on. Proof: section 3. |
| G2 | Low-medium | `tools/agy-guard.mjs:23` | The PreToolUse guard for agy workers denies `/ELEVENLABS_API_KEY\|\.env\b/i`. The ElevenLabs name is `ELEVEN_LABS_API_KEY` since `7c2a0bb` (`sfx/generate.mjs:22`, `music/generate.mjs:20`), and `LEONARDO_API_KEY` was never listed. A worker command such as `node -e "console.log(process.env.ELEVEN_LABS_API_KEY)"` passes the guard. The variable only exists if the operator exports it into the worker's shell; the scripts themselves read the file, and `.env` is still blocked. | Open: outside this task's paths. Fix: `/ELEVEN_?LABS_API_KEY\|LEONARDO_API_KEY\|\.env\b/i`, plus one case per name in `tools/agy-guard.test.mjs`. |
| D1 | Info | `.ai/manifest.yaml:48` | `secretsFrom` still says `ELEVENLABS_API_KEY: process env, else kiro/pof/.env`. Since `7c2a0bb` it is `ELEVEN_LABS_API_KEY` from bp's `.env`, and `LEONARDO_API_KEY` (bp `.env`, read by both Leonardo clients) is missing. | Open: outside this task's paths. Fix: name both keys and bp's `.env`. |
| E1 | Info | `sfx/generate.mjs:128`, `music/generate.mjs:93` | `spawn('ffmpeg', …)` passes no `env`, so ffmpeg inherits `process.env`. When the operator sets the key in the environment (`:96` / `:64`), ffmpeg's environment holds it. ffmpeg does not print its environment, and the key is never on a command line. | Accepted. If wanted: `spawn(bin, args, { env: { ...process.env, ELEVEN_LABS_API_KEY: undefined } })`. |
| M1 | Info (spend, not secrecy) | `music/generate.mjs:275`, `:321-326` | A network error or the 600 s timeout in `fetch` throws past the log write, so a call that may have been billed leaves no entry in `elevenlabs-log.json`. The key does not leak: `main().catch` sanitizes (`:367`). `sfx/generate.mjs:507-515` logs network errors. | Report only (not a security fix). Fix if wanted: catch in `generate()` and log `{ status: 0, outcome: 'network-error' }` as sfx does. |
| R1 | Info | `assets/leonardo.mjs:233`, `keyart/leonardo-keyart.mjs:132` | `redact` replaces only the exact key. A response that reflects it re-encoded (URL- or base64-encoded) would pass through. The ElevenLabs `sanitize` (`sfx:47-52`, `music:56-61`) also strips any `sk_…` token. | Accepted: the key is a plain token with no characters that JSON or URLs re-encode, and no known Leonardo response reflects it. |

## 1. Per script: the four checks

### `apps/vr/tools/sfx/generate.mjs` (ElevenLabs, sound effects): clean

1. **Read at run time, no default.** `loadKey()` (`:95-124`) reads `process.env.ELEVEN_LABS_API_KEY` (`:96-97`), else
   parses `C:\Users\kazda\kiro\bp\.env` (`:21`, `:98-121`). It throws `:102` / `:123` if the key is missing. Those
   messages name the variable and the file but no value. It is called once, at `:774`, after argument parsing and the
   self-test. No literal key and no fallback value.
2. **Never reaches output.** The key is used only in the `xi-api-key` header (`:475`) to the fixed HTTPS endpoint
   (`:25`, no query string). It never reaches a command line: `run()` (`:126-143`) spawns ffmpeg with an argument
   array. Every error and stack that is logged goes through `sanitize()`: `:140`, `:270`, `:516`, `:555`, `:819`,
   `:823-824`, `:872-873`, `:952`, `:959`. The unsanitized `log()` lines (`:517`, `:550`, `:557`, `:758-762`, `:810`,
   `:816`, `:854`, `:869`, `:945-953`) print only ids, numbers and status codes. `StopRun` (`:546`) carries only status
   and cue id.
3. **Writes.** `sanitize()` (`:47-52`) removes the exact key and any `sk_…` token first, then cuts to 300 characters,
   so it never cuts a key in half. Its one pre-cut, `stderr.slice(-600)` at `:140`, is ffmpeg's output, which never
   held the key. The ledger (`:508-515`, `:531-539`) holds id, ok, seconds, status, attempt and character cost. The
   call log (`:507`, `:522-530`) holds id, prompt, seconds, attempt, status, outcome and characters. The manifest
   (`:906-943`) and `index.html` hold cue data. None of these fields can hold a secret.
4. **Committed output.** `apps/vr/art/audio/sfx/manifest.json` has 15 calls and no field named like a secret. Its 35
   strings of 32 or more characters are all `sha256` values.

### `apps/vr/tools/music/generate.mjs` (ElevenLabs, music, T40): clean

This file was not covered by the baseline.

1. `loadKey()` (`:63-89`) is the same reader as sfx: environment first (`:64-65`), else bp's `.env` (`:19`). It
   throws with the name only, has no default, and is called only for a generating run (`:307`; `--convert-only`
   never reads the key).
2. The key goes only in the `xi-api-key` header (`:277`). The URL's only query parameter is `output_format`
   (`:275`). ffmpeg gets an argument array (`:91-103`), and its stderr is sanitized (`:100`). A failed call's body is
   sanitized before it is stored or printed (`:324`, `:332`), and so is the top-level error (`:367`).
3. `sanitize()` (`:56-61`) redacts before it cuts. `elevenlabs-log.json` entries (`:282-287`) hold id, time,
   endpoint, model, prompt, length, format, status, bytes, elapsed time, `song-id` and the regeneration reason. The
   manifest (`:352-358`) holds cue measurements and hashes.
4. **Committed output.** `elevenlabs-log.json` has 2 calls, no `error` field and no field named like a secret. The
   two `song_id` values are 20 characters long and carry no `sk_` prefix: response identifiers, not keys.
   `music/manifest.json` has only `sha256` values as long strings.

### `apps/vr/tools/assets/leonardo.mjs` (Leonardo, concept art T38/T15): one finding, fixed (L1)

1. `readKey()` (`:235-239`) reads `LEONARDO_API_KEY` from `C:/Users/kazda/kiro/bp/.env` (`:25`) only, never the
   environment, and only inside `gen()` (`:305`), after both credit caps have been checked. The error at `:237`
   names the file, not a value. No literal key and no default.
2. The key goes only in the `authorization` header of `call()` (`:244`) to the fixed API (`:26`). The image download
   (`:335`) sends **no** credentials to the CDN URL. No URL carries the key: `/v1/me`, `/v2/generations` and
   `/v1/generations/<id>`. The `REFUSED` lines (`:297`, `:301`) run before the key is read. `balance()` swallows its
   errors (`:261`). Every other error ends in the `catch` at `:342-344`, which redacts.
3. **Redaction.** `redact` (`:233`) runs on the whole response body as it arrives (`:248`, since `c2b29a6`) and on
   every caught message (`:343`). `writeLog` (`:218-230`) writes a fixed set of fields: date, view, model, prompt,
   seed, quality, size, `generationId`, status, credits, file, bytes and the redacted `error`.
4. The five committed character logs are clean (section 2).

### `apps/vr/tools/keyart/leonardo-keyart.mjs` (Leonardo, key art): one finding, fixed (L1)

The same recipe as `leonardo.mjs`; the clients stay separate, because merging them is the duplication sweep's job.

1. `readKey()` (`:134-138`) reads from bp's `.env` (`:23`) only, inside `gen()` (`:207`), after the caps (`:197-204`).
   The error names the file, not a value. No default.
2. The key goes in the `authorization` header only (`:143`). The image download (`:237`) sends no credentials. No key
   in a URL. The `REFUSED` lines come before the key is read. `balance()` swallows its errors.
3. `redact` (`:132`) runs on the response body as it arrives (`:147`, since `458c6b9`) and in the `catch` (`:245`).
   `writeLog` (`:117-129`) and `manifest()` (`:260-303`) write fixed fields only: prompt, seed, quality, credits,
   purpose and file paths.
4. `apps/vr/art/key-art/leonardo-log.json` is clean (section 2).

## 2. The committed logs and the history

Every check below matched by pattern. Only counts, file names and commit ids were printed, never a matched value.

Committed logs: each file was parsed, and every field name and string was tested.

| File | Calls | Field names like a secret | Words `authorization`/`bearer`/`api_key`/`xi-api`/`token`/`secret`/`sk_…` | UUID-shaped strings that are not a `generationId` | `error` entries |
| --- | --- | --- | --- | --- | --- |
| `characters/brennic/leonardo-log.json` | 2 | none | none | 0 of 2 | 0 |
| `characters/cinder-hound/leonardo-log.json` | 2 | none | none | 0 of 2 | 0 |
| `characters/conscript/leonardo-log.json` | 2 | none | none | 0 of 2 | 0 |
| `characters/mire-maw/leonardo-log.json` | 2 | none | none | 0 of 2 | 0 |
| `characters/slinger/leonardo-log.json` | 2 | none | none | 0 of 2 | 0 |
| `key-art/leonardo-log.json` | 7 | none | none | 0 of 7 | 0 |
| `audio/music/elevenlabs-log.json` | 2 | none | none | 0 | 0 |
| `audio/music/manifest.json`, `audio/sfx/manifest.json` | 0 / 15 | none | none | 0 | 0 |

Field names in the Leonardo logs: `card, asset, model, runCap, nightCap, totalCredits, note, calls, date, view|id,
public, prompt, seed, quality, width, height, generationId, status, credits, creditsSource, file, bytes`.

History (382 commits, `git log --all -p`):

```sh
P='(sk_[A-Za-z0-9]{20,}|Bearer [A-Za-z0-9._-]{16,}|["'\'']?(authorization|xi-api-key|api[_-]?key|LEONARDO_API_KEY|ELEVEN_?LABS_API_KEY)["'\'']?\s*[:=]\s*["'\'']?[A-Za-z0-9_-]{16,}|AKIA[0-9A-Z]{16}|ghp_[A-Za-z0-9]{30,}|github_pat_|-----BEGIN [A-Z ]*PRIVATE KEY)'
git grep -nIiE "$P" | cut -d: -f1,2                       # tracked tree, file:line only
git log --all -p --no-color --format='@@C %h' -- . ':!*.uasset' ':!*.umap' \
  | awk '/^@@C /{c=$2} /^\+\+\+ b\//{f=substr($0,7)} /^[+-][^+-]/{print c" "f"\t"substr($0,2)}' \
  | grep -iE "$P" | cut -f1 | sort -u                     # every added or removed line, commit + file only
git log --all -p --format= -- 'apps/vr/art/characters/*/leonardo-log.json' apps/vr/art/key-art/leonardo-log.json \
  | grep '^+' | grep -oiE '"[A-Za-z]+": *"<uuid>"|<uuid>' | grep -vc '"generationId"'   # 0
git log --all --name-only --format= | grep -iE '(^|/)\.env|secret|credential|\.pem$|\.key$|\.keystore$|\.jks$|\.p12$|id_rsa'
```

- Tree: one hit, `docs/research/SECURITY-SCAN-2026-10-21.md:78`. That line is the baseline's own grep command, so the
  pattern matched itself. There is no value in it.
- History: one hit, the same line, added in `58808aa`. No key, token or Authorization value has ever been committed.
- Leonardo log history: every UUID-shaped string ever added is a `generationId` value. Matches for
  `error|authori|bearer|token|key` in the added lines are only the words `key-art` in file paths and ids.
- No `.env`, key, keystore or certificate file has ever been committed.
- Baseline S1 (the Android File Server `SecurityToken` in `DefaultEngine.ini`) is out of this scan's scope and
  unchanged.

## 3. Fix and proof

### L1, `c2b29a6` (`leonardo.mjs`) and `458c6b9` (`leonardo-keyart.mjs`): redact a response before it is cut

```js
  // Redacted before it is parsed or cut: a key echoed in the body and then cut in half would slip past redact.
  const text = redact(await res.text(), key);
```

Proof: `node --test apps/vr/tools/keyart/leonardo-redaction.test.mjs`. It needs no dependencies and no network. Each
client is copied into a `mkdtemp` tree, so `ROOT` and its logs live there. It runs with a `--import` preload that:

- returns a fake key for any `.env` read (the real bp `.env` is never read);
- hides every real `leonardo-log.json` (the night-cap scan of the main checkout and the worktrees finds nothing);
- replaces `fetch` with a stub that answers with the Authorization header echoed across a cut point.

The test fails if any 8-character piece of the fake key reaches stdout, stderr or the written log. There are three
cases per client:

- `nogen`: a 200 without a `generationId`, with the key at characters 280-316 of the 300-character cut. This is the
  leak.
- `json`: a 401 with a JSON body, with the key across the inner 500-character cut.
- `raw`: a 401 with a non-JSON body, with the key across both inner cuts.

`json` and `raw` already passed before the fix. The final `.slice(0, 500)` of the whole message is always shorter
than the prefix plus the inner cut, so it drops the cut key. Those two cases stay as guards.

Before the fix (`6fbf478`):

```
✔ leonardo.mjs: a key echoed in a json response never reaches output or the log
✔ leonardo.mjs: a key echoed in a raw response never reaches output or the log
✖ leonardo.mjs: a key echoed in a nogen response never reaches output or the log
✔ leonardo-keyart.mjs: a key echoed in a json response never reaches output or the log
✔ leonardo-keyart.mjs: a key echoed in a raw response never reaches output or the log
✖ leonardo-keyart.mjs: a key echoed in a nogen response never reaches output or the log
  AssertionError [ERR_ASSERTION]: key fragment fakekey0 leaked
```

After `458c6b9`: `ℹ tests 6`, `ℹ pass 6`, `ℹ fail 0`. The test at `c2b29a6` covers only `leonardo.mjs` (3 pass); the
key-art cases are added with the key-art fix.

## 4. Gates on the fixed tree

| Command | Result |
| --- | --- |
| `node apps/vr/tools/lint/unused-members.mjs && node apps/vr/tools/lint/format-check.mjs` | `unused-members OK: 555 private/protected members in 39 classes`; `format-check OK: 0 changed C++ file(s) since 6fbf478 (clang-format version 19.1.5)` |
| `node --test tools/agy-guard.test.mjs` | 30 pass, 0 fail |
| `node apps/vr/tools/check-pin.mjs` | `pin OK: 9 files @ baeac66`, `conformance OK: 45 vectors`, exit 0 |
| `node apps/vr/tools/check-pin.mjs --self-test` | the planted corruptions detected, exit 0 |
| `node --test apps/vr/tools/keyart/leonardo-redaction.test.mjs` | 6 pass, 0 fail |
| `node tools/check-commits.mjs` | OK |
| typecheck | none configured |

No Unreal build or suite was run, and none is claimed. The fixes touch only the two Node clients.
