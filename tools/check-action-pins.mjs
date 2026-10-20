// Pin gate for GitHub Actions refs in .github/workflows. Node built-ins only.
// Every `uses:` must be a local ./ path, a docker:// image pinned by @sha256 digest, or
// owner/repo[/path]@<40-hex commit sha> followed by a `# vX.Y.Z` comment. Dependabot
// (.github/dependabot.yml) bumps the sha and that comment together, so the ref stays readable.
// Run: node tools/check-action-pins.mjs [--self-test]
import { existsSync, readFileSync, readdirSync, statSync } from 'node:fs';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';

const repoRoot = join(dirname(fileURLToPath(import.meta.url)), '..');
const workflowsDir = join(repoRoot, '.github', 'workflows');

function fail(message) {
  console.error(message);
  process.exit(1);
}

function parseArgs(argv) {
  let selfTest = false;
  for (const arg of argv) {
    if (arg === '--self-test') selfTest = true;
    else fail(`unknown argument: ${arg}`);
  }
  return { selfTest };
}

const USES = /^\s*(?:-\s+)?uses:\s*(.*)$/;
const VALUE = /^(['"]?)([^'"\s#]+)\1\s*(?:#\s*(.*))?$/;
const ACTION_SHA = /^[A-Za-z0-9_.-]+\/[A-Za-z0-9_.-]+(?:\/[^@\s]+)?@[0-9a-f]{40}$/;
const DOCKER_DIGEST = /^docker:\/\/[^@\s]+@sha256:[0-9a-f]{64}$/;
const VERSION_COMMENT = /^v\d/;

// One workflow's text -> the refs it holds and the ones that are not pinned.
function checkText(text, file) {
  const refs = [];
  const errors = [];
  const lines = text.split(/\r?\n/);
  for (let i = 0; i < lines.length; i++) {
    const m = USES.exec(lines[i]);
    if (!m) continue;
    const where = `${file}:${i + 1}`;
    const v = VALUE.exec(m[1].trim());
    if (!v) {
      errors.push(`${where}: cannot read the uses: value: ${m[1].trim()}`);
      continue;
    }
    const ref = v[2];
    const comment = (v[3] ?? '').trim();
    refs.push(ref);
    if (ref.startsWith('./')) continue;
    if (ref.startsWith('docker://')) {
      if (!DOCKER_DIGEST.test(ref)) errors.push(`${where}: ${ref} is not pinned to an @sha256 digest`);
      continue;
    }
    if (!ACTION_SHA.test(ref)) {
      errors.push(`${where}: ${ref} is not pinned to a full commit SHA`);
    } else if (!VERSION_COMMENT.test(comment)) {
      errors.push(`${where}: ${ref} has no '# vX.Y.Z' version comment for Dependabot`);
    }
  }
  return { refs, errors };
}

function checkWorkflows() {
  if (!existsSync(workflowsDir) || !statSync(workflowsDir).isDirectory()) {
    return { ok: false, errors: [`missing directory: ${workflowsDir}`] };
  }
  const files = readdirSync(workflowsDir).filter((name) => /\.ya?ml$/.test(name)).sort();
  const errors = [];
  let refCount = 0;
  for (const name of files) {
    const result = checkText(readFileSync(join(workflowsDir, name), 'utf8'), `.github/workflows/${name}`);
    refCount += result.refs.length;
    errors.push(...result.errors);
  }
  if (!refCount) errors.push(`no uses: refs found in ${workflowsDir}`);
  if (errors.length) return { ok: false, errors };
  return { ok: true, refCount, fileCount: files.length };
}

function selfTest() {
  const sha = '0123456789abcdef0123456789abcdef01234567';
  const digest = 'a'.repeat(64);
  const pinned = [
    `      - uses: actions/checkout@${sha} # v4.2.2`,
    `      - uses: "actions/setup-node@${sha}" # v4.4.0`,
    `    uses: octo/repo/.github/workflows/ci.yml@${sha} # v1`,
    '      - uses: ./.github/actions/local',
    `      - uses: docker://alpine@sha256:${digest}`,
  ];
  const unpinned = [
    '      - uses: actions/checkout@v4',
    '      - uses: actions/checkout@main',
    '      - uses: actions/checkout@11bd719',
    `      - uses: actions/checkout@${sha}`,
    '      - uses: docker://alpine:3.20',
  ];
  const problems = [];
  const clean = checkText(pinned.join('\n'), 'pinned.yml');
  if (clean.refs.length !== pinned.length) problems.push(`pinned sample: read ${clean.refs.length} of ${pinned.length} refs`);
  problems.push(...clean.errors.map((err) => `pinned sample flagged: ${err}`));
  unpinned.forEach((line, i) => {
    const dirty = checkText(line, 'unpinned.yml');
    if (dirty.errors.length !== 1 || !dirty.errors[0].startsWith('unpinned.yml:1: ')) {
      problems.push(`unpinned sample ${i + 1} not flagged: ${line.trim()}`);
    }
  });
  if (problems.length) {
    console.error(`self-test failed\n${problems.join('\n')}`);
    process.exit(1);
  }
  console.log(`self-test: ${pinned.length} pinned refs pass, ${unpinned.length} unpinned refs flagged`);
  process.exit(0);
}

function main() {
  const { selfTest: doSelf } = parseArgs(process.argv.slice(2));
  if (doSelf) selfTest();
  const result = checkWorkflows();
  if (!result.ok) fail(result.errors.join('\n'));
  console.log(`actions pinned: ${result.refCount} refs in ${result.fileCount} workflows`);
}

main();
