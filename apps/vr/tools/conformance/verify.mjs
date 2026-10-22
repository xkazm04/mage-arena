// Proves the committed conformance vectors are byte-identical to freshly regenerated ones. Node built-ins only.
//
// This is the K2a procedure of docs/research/KPI-2026-10-22.md as one command: copy the tools and data into an
// OS temp directory, drop the copied vectors so every compared file is fresh, run generate.mjs there against the
// TV clone, then compare each committed file byte for byte with its regenerated twin. The repo is only read:
// nothing is written to apps/vr/data/ or apps/vr/Game/Clips/ (.ai/manifest.yaml neverTouch), and `git status
// --short` is asserted unchanged around the run.
//
// Usage: node apps/vr/tools/conformance/verify.mjs [--source <tv clone>] [--tsx <tsx cli.mjs>] [--if-available]
//        node apps/vr/tools/conformance/verify.mjs --self-test
//   --source        the TV clone holding the pin commit; else MAGE_ARENA_ARENA
//   --tsx           the tsx CLI; else TSX_CLI, else the install inside the TV clone
//   --if-available  prerequisites absent: print the skip reason and exit 0 (the pre-push hook uses this)
//   --self-test     no TV clone, no network; proves the comparator and the absent-prerequisites path
//
// Exit codes:
//   0  all N committed vectors are byte-identical to regenerated ones
//   1  a vector differs, the file counts differ, nothing was compared, generation failed, or git status changed
//   2  usage error
//   3  prerequisites absent (no TV clone, or no tsx): nothing was compared, which is NOT a pass
import { spawnSync } from 'node:child_process';
import {
  cpSync,
  existsSync,
  mkdirSync,
  mkdtempSync,
  readFileSync,
  readdirSync,
  rmSync,
  writeFileSync,
} from 'node:fs';
import { tmpdir } from 'node:os';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';

const EXIT_IDENTICAL = 0;
const EXIT_DIFF = 1;
const EXIT_USAGE = 2;
const EXIT_PREREQ = 3;

const here = dirname(fileURLToPath(import.meta.url));
const vrRoot = join(here, '..', '..');
const repoRoot = join(vrRoot, '..', '..');
const committedDir = join(vrRoot, 'data', 'conformance');

function usage(message) {
  console.error(`verify: ${message}`);
  process.exit(EXIT_USAGE);
}

function parseArgs(argv) {
  const opts = { source: null, tsx: null, ifAvailable: false, selfTest: false };
  for (let i = 0; i < argv.length; i++) {
    const arg = argv[i];
    if (arg === '--self-test') opts.selfTest = true;
    else if (arg === '--if-available') opts.ifAvailable = true;
    else if (arg === '--source' || arg === '--tsx') {
      const value = argv[++i];
      if (!value) usage(`${arg} requires a path`);
      opts[arg.slice(2)] = value;
    } else if (arg.startsWith('--source=')) opts.source = arg.slice('--source='.length) || usage('--source requires a path');
    else if (arg.startsWith('--tsx=')) opts.tsx = arg.slice('--tsx='.length) || usage('--tsx requires a path');
    else usage(`unknown argument: ${arg}`);
  }
  if (opts.selfTest && (opts.source || opts.tsx || opts.ifAvailable)) usage('--self-test takes no other option');
  return opts;
}

// Every file of `expectedDir` against its twin in `actualDir`. Returns the names that differ, whatever the reason.
function compareDirs(expectedDir, actualDir) {
  const expected = readdirSync(expectedDir).sort();
  const actual = existsSync(actualDir) ? readdirSync(actualDir).sort() : [];
  const diffs = [];
  for (const name of expected) {
    if (!actual.includes(name)) diffs.push(`${name} (not regenerated)`);
    else if (!readFileSync(join(expectedDir, name)).equals(readFileSync(join(actualDir, name)))) diffs.push(`${name} (bytes differ)`);
  }
  for (const name of actual) if (!expected.includes(name)) diffs.push(`${name} (regenerated, not committed)`);
  return { committed: expected.length, regenerated: actual.length, diffs };
}

// The TV clone and the tsx CLI, or a list of what is missing. Mirrors the candidates generate.mjs tries.
function findPrereqs({ source, tsx, env }) {
  const sourceRepo = source || env.MAGE_ARENA_ARENA || '';
  const missing = [];
  if (!sourceRepo) missing.push('the TV clone (pass --source <path> or set MAGE_ARENA_ARENA)');
  else if (!existsSync(join(sourceRepo, '.git'))) missing.push(`the TV clone: no git repo at ${sourceRepo}`);
  const candidates = [
    tsx,
    env.TSX_CLI,
    sourceRepo && join(sourceRepo, 'packages/core/node_modules/tsx/dist/cli.mjs'),
    sourceRepo && join(sourceRepo, 'node_modules/tsx/dist/cli.mjs'),
  ].filter(Boolean);
  const tsxCli = candidates.find((path) => existsSync(path));
  if (!tsxCli) missing.push(`tsx (pass --tsx <cli.mjs> or set TSX_CLI)${candidates.length ? `; tried ${candidates.join(', ')}` : ''}`);
  return missing.length ? { ok: false, missing } : { ok: true, sourceRepo, tsxCli };
}

function gitStatus() {
  const result = spawnSync('git', ['-C', repoRoot, 'status', '--short'], { encoding: 'utf8', windowsHide: true });
  if (result.error || result.status !== 0) return null;
  return result.stdout;
}

function verify(opts) {
  const prereqs = findPrereqs({ source: opts.source, tsx: opts.tsx, env: process.env });
  if (!prereqs.ok) {
    const lines = prereqs.missing.map((item) => `  - ${item}`).join('\n');
    if (opts.ifAvailable) {
      console.log(`verify: SKIPPED, prerequisites absent, nothing was compared:\n${lines}`);
      return EXIT_IDENTICAL;
    }
    console.error(`verify: PREREQUISITES ABSENT, nothing was compared (this is not a pass):\n${lines}`);
    return EXIT_PREREQ;
  }

  const before = gitStatus();
  if (before === null) {
    console.error('verify: FAIL, `git status --short` could not run, so the repo cannot be proven untouched.');
    return EXIT_DIFF;
  }
  if (!existsSync(committedDir) || readdirSync(committedDir).length === 0) {
    console.error(`verify: FAIL, no committed vectors in ${committedDir}; a run that compared nothing is not a pass.`);
    return EXIT_DIFF;
  }

  const scratch = mkdtempSync(join(tmpdir(), 'mage-conformance-verify-'));
  let outcome = EXIT_DIFF;
  try {
    const scratchVr = join(scratch, 'apps', 'vr');
    mkdirSync(scratchVr, { recursive: true });
    cpSync(join(vrRoot, 'tools'), join(scratchVr, 'tools'), { recursive: true });
    cpSync(join(vrRoot, 'data'), join(scratchVr, 'data'), { recursive: true });
    rmSync(join(scratchVr, 'data', 'conformance'), { recursive: true, force: true });

    const run = spawnSync(process.execPath, [join(scratchVr, 'tools', 'conformance', 'generate.mjs')], {
      cwd: scratchVr,
      encoding: 'utf8',
      maxBuffer: 64 * 1024 * 1024,
      env: { ...process.env, MAGE_ARENA_ARENA: prereqs.sourceRepo, TSX_CLI: prereqs.tsxCli },
    });
    if (run.error || run.status !== 0) {
      console.error(`verify: FAIL, generate.mjs ${run.error ? `did not start: ${run.error.message}` : `exited ${run.status}`}\n${run.stdout || ''}${run.stderr || ''}`);
    } else {
      const { committed, regenerated, diffs } = compareDirs(committedDir, join(scratchVr, 'data', 'conformance'));
      if (committed === 0) {
        console.error('verify: FAIL, compared 0 files; a run that compared nothing is not a pass.');
      } else if (diffs.length > 0 || regenerated !== committed) {
        console.error(`verify: FAIL, ${diffs.length} of ${committed} committed vectors differ (regenerated ${regenerated}):`);
        for (const line of diffs) console.error(`  DIFF ${line}`);
        if (diffs.length === 0) console.error(`  committed ${committed} files, regenerated ${regenerated}`);
      } else {
        outcome = EXIT_IDENTICAL;
        console.log(`verify: OK, all ${committed} committed vectors are byte-identical to regenerated ones.`);
      }
    }
  } finally {
    rmSync(scratch, { recursive: true, force: true });
  }

  const after = gitStatus();
  if (after !== before) {
    console.error(`verify: FAIL, \`git status --short\` changed during the run.\n  before: ${JSON.stringify(before)}\n  after:  ${JSON.stringify(after)}`);
    return EXIT_DIFF;
  }
  return outcome;
}

function selfTest() {
  const root = mkdtempSync(join(tmpdir(), 'mage-conformance-selftest-'));
  const problems = [];
  const expect = (ok, what) => {
    if (!ok) problems.push(what);
  };
  try {
    const a = join(root, 'committed');
    const b = join(root, 'regenerated');
    mkdirSync(a);
    mkdirSync(b);
    for (const [name, body] of [['alpha.json', '{"a":1}\n'], ['beta.json', '{"b":2}\n'], ['gamma.json', '{"c":3}\n']]) {
      writeFileSync(join(a, name), body);
      writeFileSync(join(b, name), body);
    }
    const same = compareDirs(a, b);
    expect(same.diffs.length === 0 && same.committed === 3 && same.regenerated === 3, 'equal fixtures were not reported identical');

    // Mutate one byte of one fixture.
    const bytes = readFileSync(join(b, 'beta.json'));
    bytes[2] ^= 0x01;
    writeFileSync(join(b, 'beta.json'), bytes);
    const mutated = compareDirs(a, b);
    expect(mutated.diffs.length === 1 && mutated.diffs[0].startsWith('beta.json'), `a one-byte mutation was not reported as beta.json alone: ${JSON.stringify(mutated.diffs)}`);

    // A missing and an extra regenerated file are both failures, by name.
    writeFileSync(join(b, 'beta.json'), readFileSync(join(a, 'beta.json')));
    rmSync(join(b, 'gamma.json'));
    writeFileSync(join(b, 'delta.json'), '{}\n');
    const counted = compareDirs(a, b);
    expect(counted.diffs.some((d) => d.startsWith('gamma.json')) && counted.diffs.some((d) => d.startsWith('delta.json')), `a missing or extra file was not named: ${JSON.stringify(counted.diffs)}`);

    // Prerequisites: found when present, reported by name when absent.
    const clone = join(root, 'tv');
    mkdirSync(join(clone, '.git'), { recursive: true });
    const fakeTsx = join(root, 'cli.mjs');
    writeFileSync(fakeTsx, '');
    expect(findPrereqs({ source: clone, tsx: fakeTsx, env: {} }).ok, 'present prerequisites were not found');
    const absent = findPrereqs({ source: join(root, 'no-such-clone'), tsx: null, env: {} });
    expect(!absent.ok && absent.missing.length === 2, `absent prerequisites were not both reported: ${JSON.stringify(absent)}`);

    // The real no-prerequisites path of this script, as a child, with the env names stripped.
    const env = { ...process.env };
    delete env.MAGE_ARENA_ARENA;
    delete env.TSX_CLI;
    const child = spawnSync(process.execPath, [fileURLToPath(import.meta.url), '--source', join(root, 'no-such-clone'), '--tsx', join(root, 'no-such-tsx')], { encoding: 'utf8', env });
    expect(child.status === EXIT_PREREQ, `absent prerequisites exited ${child.status}, not ${EXIT_PREREQ}`);
    expect(/PREREQUISITES ABSENT/.test(child.stderr) && /TV clone/.test(child.stderr) && /tsx/.test(child.stderr), `absent-prerequisites message did not name both: ${child.stderr}`);
    const skipped = spawnSync(process.execPath, [fileURLToPath(import.meta.url), '--if-available', '--source', join(root, 'no-such-clone')], { encoding: 'utf8', env });
    expect(skipped.status === 0 && /SKIPPED/.test(skipped.stdout), '--if-available did not skip cleanly');
  } finally {
    rmSync(root, { recursive: true, force: true });
  }
  if (problems.length > 0) {
    console.error(`verify --self-test: FAIL\n${problems.map((p) => `  - ${p}`).join('\n')}`);
    return EXIT_DIFF;
  }
  console.log('verify --self-test: OK (identical on equal bytes; a one-byte mutation named beta.json; missing/extra files named; absent prerequisites exit 3 with a message; --if-available skips)');
  return EXIT_IDENTICAL;
}

const opts = parseArgs(process.argv.slice(2));
process.exit(opts.selfTest ? selfTest() : verify(opts));
