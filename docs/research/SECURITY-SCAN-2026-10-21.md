# Security scan 2026-10-21: Node and PowerShell tooling, tracked tree and history

First run of the `codebase-security-scan` charter. Lynx (Windows ARM64, no Unreal). Base `607d12b`, branch
`autopilot/codebase-security-scan-00c193b4`. Scope: every tracked file and all of `git log --all -p` for secrets;
`apps/vr/tools/**` for code patterns (read-only glance at `tools/*.sh` and `.github/workflows/`). No Unreal build, no
install, no network call.

## What matters

1. **No secret value is in the tree or the history.** The ElevenLabs key is read by name only. The one token-shaped
   value in the repo is Unreal's Android File Server `SecurityToken` (finding S1): a development-only device token,
   low risk, but worth regenerating before the repository goes public.
2. **`.gitignore` does not ignore `.env` files** (finding G1). Nothing has leaked, but one `git add -A` after
   someone copies the pof `.env` into the repo would commit the ElevenLabs key. The fix is one line in the root
   `.gitignore`, which is outside this task's paths, so it is left to the owner.
3. **Two tool bugs could write or delete outside their folder. Both are fixed** (F1, F2). Neither can be triggered by
   a stranger without a compromised Meta page or a mistyped argument.
4. **No dependencies to audit.** No `package.json`, lockfile or Python requirements file is tracked. Every tool uses
   Node built-ins only. The supply chain is the GitHub Actions (SHA-pinned), the Meta XR zips (SHA-256-pinned) and
   locally installed binaries.

## Findings, by real severity for a single-developer local toolchain

| # | Severity | Where | What | Status |
| --- | --- | --- | --- | --- |
| G1 | Medium | `.gitignore` (whole file); `.ai/guardrails.yaml:16` | `.env`, `.env.local`, `.env.production` and `apps/vr/tools/sfx/.env` are all **not ignored** (`git check-ignore -v` prints nothing for them). The `doctor.mjs` gate (`neverCommit`) checks only the names `.env` and `.env.local`, so `.env.production` or `.env.development` would pass CI too. | Open: outside this task's paths. Fix: add `.env` and `.env.*` lines (with `!.env.example` if one is ever wanted) to the root `.gitignore`, and add `".env.*"` to `neverCommit`. |
| F1 | Medium (fixed) | `apps/vr/tools/install-xr.ps1:232` (was missing before `:255` and `:262`) | The zip file name is scraped from Meta's download page and joined into the download path **before** the SHA-256 check. A page naming `..\..\..\escaped\UnrealMetaXRPlugin.207.0.zip` matched the unanchored `-FilePattern`, and curl wrote that file outside `apps/vr/.cache/xr`. The hash check then failed, but the file was already written. | **Fixed in `bfaae94`**: the name must be a `PinnedSha256` key before curl runs. |
| F2 | Low-medium (fixed) | `apps/vr/tools/capture-budget.ps1:11`, `:135` | `-Prefix` went straight into `Join-Path runs\DG4 "$Prefix$n"`, followed by `Remove-Item -Recurse -Force` as the first step of every run. `-Prefix ..\x` deleted `runs\x1`. With more `..`, it could delete any folder the user can write to. | **Fixed in `5939d72`**: `[ValidatePattern('^[A-Za-z0-9_-]+$')]`. |
| S1 | Low | `apps/vr/Game/Config/DefaultEngine.ini:30`, added in `ceb2455` | `SecurityToken=` of the Android File Server plugin (`bAllowNetworkConnection=True`, `bIncludeInShipping=False`). Anyone with this token, on the same network as a dev-build headset running AFS, can push or pull files on it. It never ships (Shipping excludes AFS). It is not an account credential. | Open (in `Game/`, which this task may not touch). Fix: before the repo goes public, regenerate it in Project Settings > Android File Server, or set `bAllowNetworkConnection=False` and use USB only. History still holds the old value, which is harmless once regenerated. |
| C1 | Low (by design) | `apps/vr/tools/conformance/generate.mjs:23-25`, `:115`, `:125`; `conformance/oracle.mjs:19-30` | Runs code from the TV repo (its compiler and `tsx`) and dynamically imports its kernel `.ts` files. `MAGE_ARENA_ARENA` and `TSX_CLI` from the environment pick which repo and binary run. This trusts the sibling repo completely. | Accepted: the TV repo is the owner's own canon. Keep `MAGE_ARENA_ARENA` unset in agent sessions. |
| C2 | Low | `apps/vr/tools/sfx/generate.mjs:126`, `:143-144`; `lint/format-check.mjs:48`; `lint/tidy-lines.mjs:170` | Tools start `ffmpeg`, `clang-format` and `git` by bare name. On Windows, Node's `spawn` (libuv) looks in the current directory before `PATH`, so a planted `ffmpeg.exe` in the folder you run from would run. | Report only. All the tools are run from the repo root, and nothing tracked is named like that. Fix if wanted: resolve an absolute path once (as `install-xr.ps1:295` does with `System32\tar.exe`). |
| W1 | Info | `install-xr.ps1:125`, `:145-167`; `sfx/generate.mjs:22` | Writes outside the repo, all by design: user environment variables (`ANDROID_HOME`, `NDKROOT`, `JAVA_HOME`), `%LOCALAPPDATA%\Unreal Engine\Engine\Config\UserEngine.ini`, and the SFX spend ledger `%TEMP%\a05-sfx-ledger.json`. | Accepted. Each one is a fixed path and is documented in the script. |
| W2 | Info | `conformance/oracle.mjs:1555` | Deletes every `*.json` in the `--out` folder. Only `generate.mjs:125` calls it, always with `apps/vr/data/conformance`. A hand run with a wrong `--out` would delete JSON there. | Report only. |
| N1 | Info | `sfx/generate.mjs:21`, `:93-121`, `:45-50`, `:457-470` | The only network call that carries a credential. The key comes from the environment, else from the pof `.env` (hard-coded path). It goes only in the `xi-api-key` header to a fixed HTTPS endpoint, never onto a command line, and `sanitize()` strips it (and any `sk_…`) from every logged or manifest error. | Good as is. |

Checked and clean:

- **No shell-string execution anywhere.** Every `spawnSync`/`spawn` in `apps/vr/tools` takes an argument array with
  no `shell: true`. There is no `exec`/`execSync`, `eval`, `new Function`, `Invoke-Expression` or `iex`, and the
  PowerShell scripts use `& <exe> <args>` or `Start-Process -ArgumentList`.
- **Paths from data are contained.** `check-pin.mjs:73` (`insidePath`) rejects a manifest path that leaves the pinned
  folder. `sfx/generate.mjs:72` (`assertInside`) and the cue id check `^[a-z0-9-]+$` (`:756`) do the same for cue files.
  `conformance/generate.mjs:86-92` joins `git ls-tree` paths, which git never lets contain `..`.
- **Other recursive deletes** target fixed or `mkdtemp` folders: `check-pin.mjs:189`, `lint/*`, `glyphs/generate.mjs:645`,
  `hud/generate.mjs:1253`, `sfx/generate.mjs:358/391/739`, `conformance/generate.mjs:79`, and
  `capture-style-pick.ps1:22` (takes no parameters). `import-sfx.ps1 -Run` and `create-budget-fx.ps1 -Run` are joined
  into `runs\` but only ever delete a single log file, not a folder.
- **Downloads.** Only `install-xr.ps1` downloads, over HTTPS from `developers.meta.com`, and pins both archives by
  SHA-256 (`:52-55`). Since F1 the name is checked before the download and the content after it.
- **`.gitignore`** covers `.personas/` (line 28) and `runs/` (line 26, at any depth: `apps/vr/runs/x` is ignored too).

## 1. Dependencies

`git ls-files | grep -iE 'package(-lock)?\.json|pnpm-lock|yarn\.lock|requirements|pyproject|Pipfile|\.npmrc'` finds
nothing. The repo has no Node or Python manifest, so there is no pinned package version to check. Imports across
`apps/vr/tools`, `tools/` and `.ai/*.mjs` are all `node:` built-ins (`child_process`, `crypto`, `fs`, `os`, `path`,
`test`, `assert/strict`, `url`, `zlib`). The one third-party library is Pillow, in `apps/vr/tools/styles/sample-styles.py:23`.
It is unpinned and comes from whatever the local Python has. That is a one-off art script, not a gate.

What the toolchain trusts instead:

| Item | Pin | Note |
| --- | --- | --- |
| `actions/checkout` | `11bd719…` (v4.2.2) | SHA-pinned; `tools/check-action-pins.mjs` enforces it, Dependabot bumps it. No advisory known to me for this version. |
| `actions/setup-node` | `49933ea…` (v4.4.0) | Same. |
| Meta XR v207 zips | SHA-256 in `install-xr.ps1:52-55` | Content-pinned. |
| `ffmpeg`, `clang-format` 19.1.5, `clang-tidy`, `git`, `curl.exe`, `tar.exe` | none (local install) | Out of reach of a read-only scan; see C2. |
| `node_modules` | junction to the main checkout | Untracked, and no tracked manifest describes it, so it was not audited. |

Nothing was installed, upgraded or added.

## 2. Exposed secrets

Commands (run from the repo root; the patterns are in the commands, no value is reproduced here):

```sh
# tracked tree
git grep -nIiE '(sk_[A-Za-z0-9]{20,}|xi-api-key|ELEVEN(LABS)?_API_KEY|api[_-]?key\s*[:=]|AKIA[0-9A-Z]{16}|ghp_[A-Za-z0-9]{30,}|github_pat_|xox[bap]-|-----BEGIN [A-Z ]*PRIVATE KEY|Bearer [A-Za-z0-9._-]{20,}|AIza[0-9A-Za-z_-]{35}|sk-[A-Za-z0-9]{20,})'
# every added line in history
git log --all -p --no-color -- . ':!*.uasset' ':!*.umap' | grep -E '^\+' \
  | grep -iE '(key|token|secret|passw|auth)[A-Za-z_]*["'\'']?\s*[:=]\s*["'\'']?[A-Za-z0-9+/_.-]{20,}'
# secret-like file names ever committed
git log --all --name-only --format= | grep -iE '(^|/)\.env|secret|credential|\.pem$|\.key$|\.keystore$|\.jks$|\.p12$|id_rsa' | sort -u
```

Results (298 commits):

- The tree grep hits only **names**: `ELEVENLABS_API_KEY` in `sfx/generate.mjs`, `tools/agy-guard.mjs:23`, `.ai/manifest.yaml:41`,
  task cards and docs, plus the `xi-api-key` header name at `sfx/generate.mjs:460`. No value.
- The history grep finds one token-shaped value: the AFS `SecurityToken` (S1, `ceb2455`). The other hits were code
  identifiers (`Want.Key = …`, `AuthoredOnset`).
- No `.env`, key, keystore or certificate file has ever been committed.
- The CI secrets `ASCENT_CONFORMANCE_URL` and `ASCENT_CONFORMANCE_TOKEN` are GitHub repository secrets, read by
  `.github/workflows/ai-conformance.yml:54-55` on `schedule`/`workflow_dispatch` only (no `pull_request_target`).
- `.gitignore`: `.personas/` yes, `runs/` yes, `.env*` **no** (G1).

## 3. Fixes and their proof

### F1, `bfaae94`: `install-xr.ps1` checks the archive name before the download

Offline probe (no network). It dot-sources the script, replaces `curl.exe` with a function that serves a fake Meta
page naming `..\..\..\escaped\UnrealMetaXRPlugin.207.0.zip`, and prints every path curl is told to write:

```powershell
# save as $env:TEMP\xr-name.ps1, run: powershell -NoProfile -ExecutionPolicy Bypass -File $env:TEMP\xr-name.ps1 -Script apps\vr\tools\install-xr.ps1
param([string]$Script)
. $Script
$script:CacheDir = Join-Path $env:TEMP 'secscan-xr-cache'
$script:Writes = @()
function curl.exe {
    $o = [array]::IndexOf($args, '-o')
    if ($o -ge 0) {
        $script:Writes += $args[$o + 1]
        $html = '"is_downloadable":true,"file_name":"..\\..\\..\\escaped\\UnrealMetaXRPlugin.207.0.zip","license_info":{"path":"x","id":"1"},"uri":"https://example.invalid/z"'
        if ($script:Writes.Count -eq 1) { [System.IO.File]::WriteAllText($args[$o + 1], $html) }
    } else { 'HTTP/1.1 200 OK' }
    $global:LASTEXITCODE = 0
}
try { Get-MetaPackage -PageUrl 'https://example.invalid/page' -FilePattern 'UnrealMetaXRPlugin\.207' | Out-Null } catch { Write-Host "THROW: $($_.Exception.Message)" }
foreach ($w in $script:Writes) { Write-Host ("CURL_WRITE=" + [System.IO.Path]::GetFullPath($w)) }
```

Before (`607d12b`): curl was told to write outside the cache.

```
DOWNLOAD=https://example.invalid/z -> C:\Users\kazda\AppData\Local\Temp\secscan-xr-cache\..\..\..\escaped\UnrealMetaXRPlugin.207.0.zip
CURL_WRITE=C:\Users\kazda\AppData\Local\Temp\secscan-xr-cache\UnrealMetaXRPlugin_207.html
CURL_WRITE=C:\Users\kazda\AppData\escaped\UnrealMetaXRPlugin.207.0.zip
```

After (`bfaae94`): the script stops before the download, and only the page itself was written.

```
THROW: The official page names ..\..\..\escaped\UnrealMetaXRPlugin.207.0.zip, which is not a pinned archive. Verify the new release and add it to PinnedSha256 deliberately.
CURL_WRITE=C:\Users\kazda\AppData\Local\Temp\secscan-xr-cache\UnrealMetaXRPlugin_207.html
```

The same probe with the real name `UnrealMetaXRPlugin.207.0.zip` still downloads into the cache:
`CURL_WRITE=…\secscan-xr-cache\UnrealMetaXRPlugin.207.0.zip`. It then stops because the fake curl wrote no zip,
which is expected.

### F2, `5939d72`: `capture-budget.ps1 -Prefix` must be a plain folder name

```sh
mkdir -p runs/secscan-victim1 && echo keep > runs/secscan-victim1/precious.txt
powershell -NoProfile -ExecutionPolicy Bypass -File apps/vr/tools/capture-budget.ps1 -Runs 1 -Prefix '..\secscan-victim'
ls runs/secscan-victim1
```

Before (`607d12b`): `precious.txt` was gone. The run deleted `runs\secscan-victim1`, then recreated it for its logs
and failed at `Start-Process` because there is no editor on Lynx.
After (`5939d72`): parameter binding refuses the run before any line executes, and `precious.txt` is still there:

```
Cannot validate argument on parameter 'Prefix'. The argument "..\secscan-victim" does not match the "^[A-Za-z0-9_-]+$" pattern.
```

`-Runs 0 -Prefix run_A-2` still binds and prints `budget: ok (0 runs)`. The test folders under `runs/` (gitignored)
were removed afterwards.

## 4. Gates on the fixed tree

| Command | Result |
| --- | --- |
| `node apps/vr/tools/check-pin.mjs` | `pin OK: 9 files @ baeac66`, exit 0 |
| `node apps/vr/tools/check-pin.mjs --self-test` | `corruption detected: docs/design/baseline-fourteen-nights/design/data/combat.json`, exit 0 (the planted corruption it expects) |
| `node --test tools/agy-guard.test.mjs` | 30 pass, 0 fail |
| `node apps/vr/tools/lint/unused-members.mjs && node apps/vr/tools/lint/format-check.mjs` | `unused-members OK: 555 private/protected members in 39 classes`; `format-check OK: 0 changed C++ file(s) since 607d12b (clang-format version 19.1.5)` |
| typecheck | none configured |

The Unreal suite was not run and is not claimed. Neither fix touches C++ or content.
