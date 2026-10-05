// Changed-line clang-tidy for apps/vr/Game/Source. Node built-ins only. Called by tidy.ps1 -Strict after UBT has
// written the compile database; Node runs clang-tidy itself because Windows PowerShell 5.1 strips the quotes out of
// the --line-filter JSON when it passes it to a native exe.
// Only diagnostics on lines this branch added or changed are reported (--line-filter), and each one is an error
// (--warnings-as-errors=*), so untouched code never fails a change. Silence a false positive on its own line with
// `// NOLINT(<check>): <reason>`.
//
// Usage: node apps/vr/tools/lint/tidy-lines.mjs --tidy <clang-tidy> -p <compile db dir> [--base <ref>]
//        node apps/vr/tools/lint/tidy-lines.mjs --print [--base <ref>]   (the line filter and files, no clang-tidy)
//        node apps/vr/tools/lint/tidy-lines.mjs --self-test
import { spawnSync } from 'node:child_process';
import { existsSync } from 'node:fs';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';

const here = dirname(fileURLToPath(import.meta.url));
const repoRoot = join(here, '..', '..', '..', '..');
const sourceRel = 'apps/vr/Game/Source';

function fail(message) {
  console.error(message);
  process.exit(1);
}

function parseArgs(argv) {
  const opts = { base: 'master', tidy: null, db: null, print: false, selfTest: false };
  for (let i = 0; i < argv.length; i++) {
    const arg = argv[i];
    const value = () => {
      const v = argv[++i];
      if (!v) fail(`${arg} requires a value`);
      return v;
    };
    if (arg === '--self-test') opts.selfTest = true;
    else if (arg === '--print') opts.print = true;
    else if (arg === '--base') opts.base = value();
    else if (arg === '--tidy') opts.tidy = value();
    else if (arg === '-p') opts.db = value();
    else fail(`unknown argument: ${arg}`);
  }
  return opts;
}

function git(args) {
  const r = spawnSync('git', args, { cwd: repoRoot, encoding: 'utf8', windowsHide: true, maxBuffer: 64 * 1024 * 1024 });
  if (r.error) fail(`git failed to start: ${r.error.message}`);
  if (r.status !== 0) fail(`git ${args.join(' ')} failed: ${r.stderr.trim()}`);
  return r.stdout;
}

// Added or changed line ranges per file, from a zero-context diff. Same parse as format-check.mjs, which cannot be
// imported because it runs its gate on load.
export function changedRanges(diffText) {
  const ranges = new Map();
  let file = null;
  for (const line of diffText.split('\n')) {
    if (line.startsWith('+++ ')) {
      const path = line.slice(4).trim();
      file = path === '/dev/null' ? null : path.replace(/^b\//, '');
      if (file && !ranges.has(file)) ranges.set(file, []);
      continue;
    }
    const hunk = line.match(/^@@ -\d+(?:,\d+)? \+(\d+)(?:,(\d+))? @@/);
    if (hunk && file) {
      const start = Number(hunk[1]);
      const count = hunk[2] === undefined ? 1 : Number(hunk[2]);
      if (count > 0) ranges.get(file).push([start, start + count - 1]);
    }
  }
  return ranges;
}

const CPP = /\.(cpp|h|hpp|inl)$/i;

// clang-tidy's --line-filter value. A file with no ranges is left out: an entry without `lines` would let every
// line of it through. clang-tidy matches `name` as a suffix of the path it compiled, so each file is listed with
// both separators (UBT writes Windows paths into the compile database).
export function lineFilter(ranges) {
  const filter = [];
  for (const [file, lines] of ranges) {
    if (!CPP.test(file) || !lines.length) continue;
    filter.push({ name: file, lines });
    if (file.includes('/')) filter.push({ name: file.replaceAll('/', '\\'), lines });
  }
  return filter;
}

// The translation units to run: each changed .cpp, and for a changed header the .cpp that shares its stem (the TU
// that includes it first, as in tidy.ps1). A header with no such .cpp is returned in `unchecked`.
export function translationUnits(ranges, exists) {
  const units = new Set();
  const unchecked = [];
  for (const [file, lines] of ranges) {
    if (!lines.length) continue;
    if (/\.cpp$/i.test(file)) {
      if (exists(file)) units.add(file);
    } else if (/\.(h|hpp|inl)$/i.test(file)) {
      const cpp = file.replace(/\.(h|hpp|inl)$/i, '.cpp');
      if (exists(cpp)) units.add(cpp);
      else unchecked.push(file);
    }
  }
  return { units: [...units].sort(), unchecked };
}

function selfTest() {
  const problems = [];
  const diff = [
    '+++ b/apps/vr/Game/Source/MageArenaVR/A.cpp',
    '@@ -3,0 +4,2 @@',
    '@@ -9 +11 @@',
    '+++ b/apps/vr/Game/Source/MageArenaVR/A.h',
    '@@ -7,2 +7,3 @@',
    '+++ b/apps/vr/Game/Source/MageArenaVR/Gone.cpp',
    '@@ -20,4 +19,0 @@',
    '+++ b/apps/vr/Game/Source/MageArenaVR/Lonely.h',
    '@@ -1 +1 @@',
    '+++ b/apps/vr/Game/Source/MageArenaVR/notes.txt',
    '@@ -1 +1 @@',
  ].join('\n');
  const ranges = changedRanges(diff);
  const a = JSON.stringify(ranges.get('apps/vr/Game/Source/MageArenaVR/A.cpp'));
  if (a !== '[[4,5],[11,11]]') problems.push(`diff parsing gave ${a} for A.cpp`);
  if (ranges.get('apps/vr/Game/Source/MageArenaVR/Gone.cpp')?.length !== 0) problems.push('a deletion-only hunk produced a range');

  const filter = lineFilter(ranges);
  const names = filter.map((f) => f.name);
  const expected = [
    'apps/vr/Game/Source/MageArenaVR/A.cpp',
    'apps\\vr\\Game\\Source\\MageArenaVR\\A.cpp',
    'apps/vr/Game/Source/MageArenaVR/A.h',
    'apps\\vr\\Game\\Source\\MageArenaVR\\A.h',
    'apps/vr/Game/Source/MageArenaVR/Lonely.h',
    'apps\\vr\\Game\\Source\\MageArenaVR\\Lonely.h',
  ];
  if (JSON.stringify(names) !== JSON.stringify(expected)) problems.push(`line filter names: ${JSON.stringify(names)}`);
  if (filter.some((f) => !f.lines || !f.lines.length)) problems.push('a line filter entry has no lines (would pass every line)');

  const present = new Set(['apps/vr/Game/Source/MageArenaVR/A.cpp']);
  const { units, unchecked } = translationUnits(ranges, (f) => present.has(f));
  if (JSON.stringify(units) !== '["apps/vr/Game/Source/MageArenaVR/A.cpp"]') problems.push(`translation units: ${JSON.stringify(units)}`);
  if (JSON.stringify(unchecked) !== '["apps/vr/Game/Source/MageArenaVR/Lonely.h"]') problems.push(`unchecked headers: ${JSON.stringify(unchecked)}`);

  if (problems.length) fail(`self-test failed\n${problems.join('\n')}`);
  console.log('self-test OK: changed lines parsed, deletion-only files skipped, both separators listed, headers mapped to their .cpp');
  process.exit(0);
}

function main() {
  const opts = parseArgs(process.argv.slice(2));
  if (opts.selfTest) selfTest();
  const mergeBase = git(['merge-base', 'HEAD', opts.base]).trim();
  const diff = git(['diff', '-U0', '--no-color', '--no-ext-diff', '--diff-filter=AMR', mergeBase, '--', sourceRel]);
  const ranges = changedRanges(diff);
  const filter = lineFilter(ranges);
  const { units, unchecked } = translationUnits(ranges, (f) => existsSync(join(repoRoot, f)));
  for (const header of unchecked) console.warn(`tidy-lines: ${header} has no same-stem .cpp; its changed lines are not checked`);
  if (opts.print) {
    console.log(JSON.stringify({ base: mergeBase, units, lineFilter: filter }, null, 2));
    process.exit(0);
  }
  if (!units.length || !filter.length) {
    console.log(`tidy-lines: no changed C++ lines since ${mergeBase.slice(0, 7)}`);
    process.exit(0);
  }
  if (!opts.tidy || !opts.db) fail('tidy-lines: --tidy <clang-tidy> and -p <compile db dir> are required (tidy.ps1 -Strict passes both)');
  const args = ['-p', opts.db, '--quiet', '--warnings-as-errors=*', `--line-filter=${JSON.stringify(filter)}`];
  args.push(...units.map((f) => join(repoRoot, f)));
  console.log(`tidy-lines: ${opts.tidy} on ${units.length} file(s), changed lines since ${mergeBase.slice(0, 7)}`);
  const r = spawnSync(opts.tidy, args, { cwd: repoRoot, stdio: 'inherit', windowsHide: true });
  if (r.error) fail(`clang-tidy failed to start: ${r.error.message}`);
  if (r.status !== 0) {
    console.error('tidy-lines: clang-tidy reported a finding on a changed line. Fix it, or add `// NOLINT(<check>): <reason>` on that line.');
  }
  process.exit(r.status ?? 1);
}

main();
