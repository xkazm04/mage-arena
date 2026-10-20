// Commit-subject gate. Node built-ins only.
// Every non-merge commit a branch adds must open with one of the repo's two subject forms:
//   type(scope)!: summary   conventional, type in TYPES, scope and ! optional
//   T08: summary            a task card (apps/vr/tasks/), change request or design finding id (docs/ORCHESTRATION.md step 4)
// plus git's own `Revert "..."`. Only the branch's commits are read, never history, so the gate holds from day one.
// Run: node tools/check-commits.mjs [--base <ref>] [--self-test]   (default base: upstream, else master, else origin/master)
//      node tools/check-commits.mjs --subjects <file>              (one subject per line; CI feeds it from the PR)
// Runs on every pull request in .github/workflows/commits.yml.
import { spawnSync } from 'node:child_process';
import { readFileSync } from 'node:fs';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';

const repoRoot = join(dirname(fileURLToPath(import.meta.url)), '..');

const TYPES = ['feat', 'fix', 'docs', 'style', 'refactor', 'perf', 'test', 'build', 'ci', 'chore', 'revert'];
const CONVENTIONAL = new RegExp(`^(?:${TYPES.join('|')})(?:\\([a-z0-9][a-z0-9._/-]*\\))?!?: \\S`);
const CARD_ID = '(?:[TA]\\d{2}[a-z]?|CR-\\d{3}|DF-\\d{3})';
const CARD = new RegExp(`^${CARD_ID}(?:[,/+] ?${CARD_ID})*: \\S`);
const REVERT = /^Revert "/;

function fail(message) {
  console.error(message);
  process.exit(1);
}

function parseArgs(argv) {
  let base = null;
  let subjects = null;
  let selfTest = false;
  for (let i = 0; i < argv.length; i++) {
    const arg = argv[i];
    if (arg === '--self-test') selfTest = true;
    else if (arg === '--base' || arg === '--subjects') {
      const value = argv[++i];
      if (!value) fail(`${arg} requires a value`);
      if (arg === '--base') base = value;
      else subjects = value;
    } else fail(`unknown argument: ${arg}`);
  }
  return { base, subjects, selfTest };
}

export function subjectOk(subject) {
  return CONVENTIONAL.test(subject) || CARD.test(subject) || REVERT.test(subject);
}

function git(args) {
  const r = spawnSync('git', args, { cwd: repoRoot, encoding: 'utf8', windowsHide: true, maxBuffer: 64 * 1024 * 1024 });
  if (r.error) fail(`git failed to start: ${r.error.message}`);
  return r;
}

function resolveBase(explicit) {
  const upstream = explicit ? null : git(['rev-parse', '--abbrev-ref', '--symbolic-full-name', '@{upstream}']);
  const refs = explicit ? [explicit] : upstream.status === 0 ? [upstream.stdout.trim(), 'master', 'origin/master'] : ['master', 'origin/master'];
  for (const ref of refs) {
    const r = git(['merge-base', 'HEAD', ref]);
    if (r.status === 0 && r.stdout.trim()) return r.stdout.trim();
  }
  fail(`check-commits: no merge base with ${refs.join(', ')}; pass --base <ref>`);
}

function branchSubjects(base) {
  const baseSha = resolveBase(base);
  const r = git(['log', '--no-merges', '--format=%s', `${baseSha}..HEAD`]);
  if (r.status !== 0) fail(`git log failed: ${r.stderr.trim()}`);
  return { subjects: r.stdout.split('\n').filter((s) => s.trim()), from: `since ${baseSha.slice(0, 7)}` };
}

function fileSubjects(path) {
  let text;
  try {
    text = readFileSync(path, 'utf8');
  } catch (err) {
    fail(`check-commits: cannot read ${path}: ${err.message}`);
  }
  return { subjects: text.split(/\r?\n/).filter((s) => s.trim()), from: `from ${path}` };
}

function selfTest() {
  const good = [
    'fix(hand-input-pipeline): stand mouse look down for every capture run',
    'chore(game-tests): scan-sweep round 5 ledger',
    'feat: fire wall',
    'refactor(arena)!: drop the legacy pad layout',
    'ci: bump actions/checkout',
    'T08: wave 1 integration',
    'T04b: ward onset elevation',
    'A02: greybox arena',
    'CR-003: threat geometry request',
    'T11/T12: split hands, fire wall',
    'Revert "T08: wave 1 integration"',
  ];
  const bad = [
    'Update stuff',
    'wip',
    'fix:missing space',
    'Fix(arena): capital type',
    'feature: not a type',
    'fixup! fix(arena): thing',
    'squash! T08: thing',
    't08: lower-case id',
    'T8: one digit',
    'fix(Arena): capital scope',
    'Merge master into ascent/runner',
  ];
  const problems = [];
  for (const s of good) if (!subjectOk(s)) problems.push(`accepted form rejected: ${s}`);
  for (const s of bad) if (subjectOk(s)) problems.push(`bad subject accepted: ${s}`);
  if (problems.length) fail(`self-test failed\n${problems.join('\n')}`);
  console.log(`self-test: ${good.length} accepted subjects pass, ${bad.length} bad subjects flagged`);
  process.exit(0);
}

function main() {
  const { base, subjects: subjectsFile, selfTest: doSelf } = parseArgs(process.argv.slice(2));
  if (doSelf) selfTest();
  const { subjects, from } = subjectsFile ? fileSubjects(subjectsFile) : branchSubjects(base);
  const bad = subjects.filter((s) => !subjectOk(s));
  if (bad.length) {
    console.error(bad.map((s) => `  ${s}`).join('\n'));
    console.error(`check-commits: ${bad.length} of ${subjects.length} commit subject(s) ${from} match neither form.`);
    console.error(`Use "type(scope): summary" (type: ${TYPES.join(', ')}) or "<card id>: summary" (T08, A02, CR-003, DF-001).`);
    console.error('Reword them (git commit --amend for the last one, else recreate the commits); merge commits are not checked.');
    process.exit(1);
  }
  console.log(`check-commits OK: ${subjects.length} commit subject(s) ${from}`);
}

main();
