// Installs the local pre-push gate. Node built-ins only.
//
// WHY: CLAUDE.md "Git" says agents push verified work straight to `origin master`, but the three workflows
// are `on: pull_request`, so on the path this repo actually uses no gate ran at all. .ai/manifest.yaml said
// "No local hook is installed yet" and .github/CODEOWNERS cited a lefthook.yml that was never in the tree.
// This is that missing hook, as plain git plumbing rather than a new dependency (the tooling is zero-dependency).
//
// WHAT IT INSTALLS: .git/hooks/pre-push (the repo's own hooks dir; core.hooksPath is left alone so nothing
// outside .git/ is touched and no tracked file has to carry an executable bit). The hook runs
//   1. the `test` capability from .ai/manifest.yaml  (agy guard + the pinned-data sha256 gate + its self-test)
//   2. node tools/check-repo-law.mjs                (the laws no other gate enforces)
//   3. node tools/check-commits.mjs                 (commit subjects; CLAUDE.md "Git")
// and then delegates to the Git LFS pre-push hook, which is preserved verbatim, not reimplemented: the
// original is moved to .git/hooks/pre-push.lfs and exec'd last, so `git lfs pre-push` still runs and large
// files still upload. Removing this hook is `git checkout`-proof in the other direction too: --uninstall
// puts the LFS hook back exactly as it was.
//
// Both steps together are under the 10 s budget the `always` gate set keeps, so the hook does not
// discourage pushing. A push that must go out regardless: `git push --no-verify`.
//
// Usage: node tools/install-hooks.mjs [--check] [--uninstall] [--root <repo>]
//   (no flags)   install or refresh the hook; idempotent, prints exactly what it did
//   --check      report whether the hook is installed and current; writes nothing. Exit 1 if it is not.
//   --uninstall  remove the hook and restore the preserved LFS hook
import { chmodSync, existsSync, readFileSync, renameSync, rmSync, statSync, writeFileSync } from 'node:fs';
import { spawnSync } from 'node:child_process';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';

const repoRoot = join(dirname(fileURLToPath(import.meta.url)), '..');

const MARKER = 'mage-arena pre-push gate';
const VERSION = 3;
const VERSION_LINE = `# ${MARKER} v${VERSION} - installed by tools/install-hooks.mjs; re-run it to refresh.`;

function fail(message) {
  console.error(message);
  process.exit(1);
}

function parseArgs(argv) {
  let root = repoRoot;
  let check = false;
  let uninstall = false;
  for (let i = 0; i < argv.length; i++) {
    const arg = argv[i];
    if (arg === '--check') check = true;
    else if (arg === '--uninstall') uninstall = true;
    else if (arg === '--root') {
      root = argv[++i];
      if (!root) fail('--root requires a path');
    } else if (arg.startsWith('--root=')) {
      root = arg.slice('--root='.length);
      if (!root) fail('--root requires a path');
    } else fail(`unknown argument: ${arg}`);
  }
  if (check && uninstall) fail('--check and --uninstall are separate modes');
  return { root, check, uninstall };
}

function git(root, args) {
  const result = spawnSync('git', ['-C', root, ...args], {
    encoding: 'utf8',
    windowsHide: true,
    timeout: 30000,
    env: { ...process.env, GIT_PAGER: '', GIT_TERMINAL_PROMPT: '0', PAGER: '' },
  });
  if (result.error) return { ok: false, error: `git ${args[0]} failed to start: ${result.error.message}` };
  if (result.status !== 0) return { ok: false, error: `git ${args.join(' ')} failed: ${(result.stderr || '').trim()}` };
  return { ok: true, stdout: (result.stdout ?? '').trim() };
}

// The hooks directory git will actually run: core.hooksPath when set, else <git-dir>/hooks.
function hooksDir(root) {
  const configured = git(root, ['config', '--get', 'core.hooksPath']);
  const gitDir = git(root, ['rev-parse', '--absolute-git-dir']);
  if (!gitDir.ok) return { ok: false, error: gitDir.error };
  if (configured.ok && configured.stdout) {
    // An absolute core.hooksPath is used as is; a relative one resolves against the worktree root.
    const path = configured.stdout;
    const abs = /^([A-Za-z]:[\\/]|\/)/.test(path) ? path : join(root, path);
    return { ok: true, dir: abs, viaConfig: true };
  }
  return { ok: true, dir: join(gitDir.stdout, 'hooks'), viaConfig: false };
}

function hookBody() {
  return `#!/bin/sh
${VERSION_LINE}
# Edit tools/install-hooks.mjs and re-run it; changes made here are overwritten on the next install.
# Bypass for a push that must go out regardless: git push --no-verify
set -e

repo="$(git rev-parse --show-toplevel)"
cd "$repo"

if ! command -v node >/dev/null 2>&1; then
  printf >&2 '%s\\n' "pre-push: node is not on PATH, so the repo's gates cannot run. Install Node, or push with --no-verify and say so."
  exit 1
fi

# The four groups are independent, so they run concurrently: sequentially they take about 10 s, which is
# the whole budget. Output is captured and printed only for a group that fails, so a green push is quiet
# and a red one is readable instead of four interleaved streams.
tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT

set +e
node --test tools/agy-guard.test.mjs >"$tmp/guard" 2>&1 &
pid_guard=$!
{ node apps/vr/tools/check-pin.mjs && node apps/vr/tools/check-pin.mjs --self-test; } >"$tmp/pin" 2>&1 &
pid_pin=$!
{ node apps/vr/tools/check-overlay.mjs && node apps/vr/tools/check-overlay.mjs --self-test; } >"$tmp/overlay" 2>&1 &
pid_overlay=$!
node tools/check-repo-law.mjs >"$tmp/law" 2>&1 &
pid_law=$!
# Commit subjects: the same gate .github/workflows/commits.yml runs. Bare, it resolves its own base from
# @{upstream} (else master, else origin/master), which is the branch being pushed - the right range here.
node tools/check-commits.mjs >"$tmp/subjects" 2>&1 &
pid_subjects=$!
wait $pid_guard;    rc_guard=$?
wait $pid_pin;      rc_pin=$?
wait $pid_overlay;  rc_overlay=$?
wait $pid_law;      rc_law=$?
wait $pid_subjects; rc_subjects=$?
set -e

failed=0
report() {
  # $1 label, $2 exit code, $3 output file
  if [ "$2" -eq 0 ]; then
    printf '%s\\n' "pre-push: OK   $1"
  else
    printf '%s\\n' "pre-push: FAIL $1 (exit $2)"
    sed 's/^/    /' "$3"
    failed=1
  fi
}
report "test: agy worker guard" "$rc_guard" "$tmp/guard"
report "test: pinned-data hash gate" "$rc_pin" "$tmp/pin"
report "test: overlay check" "$rc_overlay" "$tmp/overlay"
report "repo law" "$rc_law" "$tmp/law"
report "commit subjects" "$rc_subjects" "$tmp/subjects"

if [ "$failed" -ne 0 ]; then
  printf >&2 '%s\\n' "pre-push: BLOCKED. Fix the above, or push with --no-verify and say in the report that you bypassed the gate."
  exit 1
fi

printf '%s\\n' "pre-push: gates green"

# Git LFS, preserved verbatim from the hook this one replaced. Without it, LFS objects are not uploaded.
if [ -x "$(dirname "$0")/pre-push.lfs" ]; then
  exec "$(dirname "$0")/pre-push.lfs" "$@"
fi
if command -v git-lfs >/dev/null 2>&1; then
  exec git lfs pre-push "$@"
fi
printf >&2 '%s\\n' "pre-push: git-lfs was not found on PATH; LFS objects were NOT pushed."
exit 2
`;
}

function readIfExists(path) {
  try {
    return readFileSync(path, 'utf8');
  } catch {
    return null;
  }
}

function describe(root) {
  const dirInfo = hooksDir(root);
  if (!dirInfo.ok) return { ok: false, error: dirInfo.error };
  const hookPath = join(dirInfo.dir, 'pre-push');
  const lfsPath = join(dirInfo.dir, 'pre-push.lfs');
  const current = readIfExists(hookPath);
  const isOurs = current !== null && current.includes(MARKER);
  const isCurrent = current !== null && current === hookBody();
  const looksLikeLfsOnly = current !== null && !isOurs && /git\s+lfs\s+pre-push/.test(current);
  return {
    ok: true,
    dir: dirInfo.dir,
    viaConfig: dirInfo.viaConfig,
    hookPath,
    lfsPath,
    current,
    isOurs,
    isCurrent,
    looksLikeLfsOnly,
    lfsPreserved: existsSync(lfsPath),
  };
}

function install(root) {
  const state = describe(root);
  if (!state.ok) fail(state.error);
  if (!existsSync(state.dir)) fail(`hooks directory does not exist: ${state.dir}`);
  const done = [];

  if (state.isCurrent) {
    console.log(`install-hooks: already installed and current at ${state.hookPath} (v${VERSION}); nothing to do.`);
    console.log(`install-hooks: Git LFS hook preserved at ${state.lfsPath}: ${state.lfsPreserved ? 'yes' : 'no (delegates to `git lfs pre-push`)'}`);
    return 0;
  }

  if (state.current !== null && !state.isOurs) {
    // Somebody else's pre-push: preserve it rather than clobber it. For the git-lfs hook this is exactly
    // what the installed hook execs at the end, so LFS keeps working.
    if (existsSync(state.lfsPath)) {
      fail(
        `install-hooks: ${state.hookPath} is not ours and ${state.lfsPath} already exists, so preserving it would overwrite that backup.\n` +
          '  Inspect both by hand; nothing was changed.',
      );
    }
    if (!state.looksLikeLfsOnly) {
      console.log(`install-hooks: NOTE ${state.hookPath} is an existing hook that is not the Git LFS one.`);
      console.log('install-hooks: it is preserved and still run last, but read it to be sure that is what you want.');
    }
    renameSync(state.hookPath, state.lfsPath);
    try {
      chmodSync(state.lfsPath, 0o755);
    } catch {
      /* a filesystem without POSIX modes; git for Windows runs the hook regardless */
    }
    done.push(`preserved the existing pre-push hook as ${state.lfsPath} (it is exec'd last, so Git LFS still runs)`);
  }

  writeFileSync(state.hookPath, hookBody(), { encoding: 'utf8' });
  try {
    chmodSync(state.hookPath, 0o755);
  } catch {
    /* see above */
  }
  done.push(`${state.isOurs ? 'refreshed' : 'installed'} ${state.hookPath} (v${VERSION})`);

  for (const line of done) console.log(`install-hooks: ${line}`);
  console.log(
    `install-hooks: on push it runs the \`test\` capability, tools/check-repo-law.mjs and tools/check-commits.mjs, then Git LFS. Bypass: git push --no-verify.`,
  );
  console.log(`install-hooks: hooks directory ${state.viaConfig ? 'from core.hooksPath' : 'is the default <git-dir>/hooks'}; core.hooksPath was not changed.`);
  console.log('install-hooks: .git/hooks is not tracked, so every clone and worktree runs this once for itself.');
  return 0;
}

function check(root) {
  const state = describe(root);
  if (!state.ok) fail(state.error);
  if (state.isCurrent) {
    console.log(`install-hooks --check: installed and current at ${state.hookPath} (v${VERSION}).`);
    console.log(`install-hooks --check: Git LFS hook preserved at ${state.lfsPath}: ${state.lfsPreserved ? 'yes' : 'no (delegates to `git lfs pre-push`)'}`);
    return 0;
  }
  if (state.isOurs) {
    console.error(`install-hooks --check: ${state.hookPath} is ours but out of date (expected v${VERSION}). Run: node tools/install-hooks.mjs`);
    return 1;
  }
  if (state.current === null) {
    console.error(`install-hooks --check: NOT installed: no hook at ${state.hookPath}.`);
  } else {
    console.error(`install-hooks --check: NOT installed: ${state.hookPath} is a different hook (${state.looksLikeLfsOnly ? 'the Git LFS hook' : 'unrecognised'}).`);
  }
  console.error('install-hooks --check: a push would run no gate at all. Run: node tools/install-hooks.mjs');
  return 1;
}

function uninstall(root) {
  const state = describe(root);
  if (!state.ok) fail(state.error);
  if (!state.isOurs) {
    console.log(`install-hooks --uninstall: nothing of ours at ${state.hookPath}; nothing was changed.`);
    return 0;
  }
  rmSync(state.hookPath, { force: true });
  if (state.lfsPreserved) {
    renameSync(state.lfsPath, state.hookPath);
    console.log(`install-hooks --uninstall: removed the gate and restored the preserved hook at ${state.hookPath}.`);
  } else {
    console.log(`install-hooks --uninstall: removed ${state.hookPath}. No preserved hook to restore.`);
  }
  return 0;
}

function main() {
  const { root, check: doCheck, uninstall: doUninstall } = parseArgs(process.argv.slice(2));
  if (!existsSync(root) || !statSync(root).isDirectory()) fail(`not a directory: ${root}`);
  if (doCheck) process.exit(check(root));
  if (doUninstall) process.exit(uninstall(root));
  process.exit(install(root));
}

main();
