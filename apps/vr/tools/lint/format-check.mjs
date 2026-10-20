// Changed-line clang-format gate for apps/vr/Game/Source. Node built-ins only.
// Only lines this branch added or changed are checked (against the upstream merge base, else master),
// so the existing tree is never reformatted and two workers do not fight over untouched code.
// The style is apps/vr/Game/Source/.clang-format. A missing clang-format is a failure, never a skip.
//
// Usage: node apps/vr/tools/lint/format-check.mjs [--base <ref>] [--self-test]   (FORMAT_BASE=<ref> also works)
// clang-format is found in CLANG_FORMAT, on PATH, or in a Visual Studio install (VC\Tools\Llvm\x64\bin).
// Runs on every pull request in .github/workflows/quality.yml; run it yourself before proposing a C++ change.
import { spawnSync } from 'node:child_process';
import { copyFileSync, existsSync, mkdtempSync, readdirSync, rmSync, writeFileSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';

const here = dirname(fileURLToPath(import.meta.url));
const repoRoot = join(here, '..', '..', '..', '..');
const sourceRel = 'apps/vr/Game/Source';
const styleFile = join(repoRoot, sourceRel, '.clang-format');

function fail(message) {
  console.error(message);
  process.exit(1);
}

function parseArgs(argv) {
  let base = process.env.FORMAT_BASE || null;
  let selfTest = false;
  for (let i = 0; i < argv.length; i++) {
    const arg = argv[i];
    if (arg === '--self-test') selfTest = true;
    else if (arg === '--base') {
      base = argv[++i];
      if (!base) fail('--base requires a git ref');
    } else if (arg.startsWith('--base=')) base = arg.slice('--base='.length);
    else fail(`unknown argument: ${arg}`);
  }
  return { base, selfTest };
}

function run(cmd, args, options = {}) {
  return spawnSync(cmd, args, { cwd: repoRoot, encoding: 'utf8', windowsHide: true, maxBuffer: 64 * 1024 * 1024, ...options });
}

function findClangFormat() {
  const candidates = [];
  if (process.env.CLANG_FORMAT) candidates.push(process.env.CLANG_FORMAT);
  candidates.push('clang-format');
  // Linux images often install versioned binaries only (clang-format-18).
  if (process.platform !== 'win32') for (let v = 21; v >= 15; v--) candidates.push(`clang-format-${v}`);
  if (process.platform === 'win32') {
    for (const programs of [process.env['ProgramFiles'], process.env['ProgramFiles(x86)']]) {
      const vsRoot = programs && join(programs, 'Microsoft Visual Studio');
      if (!vsRoot || !existsSync(vsRoot)) continue;
      for (const version of readdirSync(vsRoot)) {
        let editions = [];
        try {
          editions = readdirSync(join(vsRoot, version));
        } catch {
          continue;
        }
        for (const edition of editions) {
          candidates.push(join(vsRoot, version, edition, 'VC', 'Tools', 'Llvm', 'x64', 'bin', 'clang-format.exe'));
        }
      }
    }
    candidates.push(join(process.env['ProgramFiles'] || 'C:\\Program Files', 'LLVM', 'bin', 'clang-format.exe'));
  }
  for (const candidate of candidates) {
    const probe = spawnSync(candidate, ['--version'], { encoding: 'utf8', windowsHide: true });
    if (!probe.error && probe.status === 0) return { path: candidate, version: probe.stdout.trim() };
  }
  return null;
}

function git(args) {
  const result = run('git', args);
  if (result.error) fail(`git failed to start: ${result.error.message}`);
  return result;
}

function resolveBase(explicit) {
  if (explicit) {
    const r = git(['merge-base', 'HEAD', explicit]);
    if (r.status !== 0) fail(`cannot find a merge base with ${explicit}: ${r.stderr.trim()}`);
    return r.stdout.trim();
  }
  const upstream = git(['rev-parse', '--abbrev-ref', '--symbolic-full-name', '@{upstream}']);
  const refs = upstream.status === 0 ? [upstream.stdout.trim(), 'master', 'origin/master'] : ['master', 'origin/master'];
  for (const ref of refs) {
    const r = git(['merge-base', 'HEAD', ref]);
    if (r.status === 0 && r.stdout.trim()) return r.stdout.trim();
  }
  fail('format-check: no upstream, master or origin/master to compare against; pass --base <ref>');
}

// Added or changed line ranges per file, from a zero-context diff of base..working tree.
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

function checkFile(clang, file, lineRanges) {
  const args = ['--dry-run', '--Werror', '--style=file'];
  for (const [from, to] of lineRanges || []) args.push(`--lines=${from}:${to}`);
  args.push(file);
  const r = spawnSync(clang, args, { cwd: repoRoot, encoding: 'utf8', windowsHide: true, maxBuffer: 64 * 1024 * 1024 });
  if (r.error) fail(`clang-format failed to start: ${r.error.message}`);
  return { ok: r.status === 0, output: `${r.stderr || ''}${r.stdout || ''}`.trim() };
}

function missingTool() {
  fail([
    'format-check: clang-format not found (PATH, CLANG_FORMAT, Visual Studio VC\\Tools\\Llvm, Program Files\\LLVM).',
    'Install it with the Visual Studio "C++ Clang tools for Windows" component, or `winget install LLVM.LLVM`,',
    'or point CLANG_FORMAT at clang-format.exe. The gate does not skip: an unchecked format is not a pass.',
  ].join('\n'));
}

function selfTest() {
  const clang = findClangFormat();
  if (!clang) missingTool();
  if (!existsSync(styleFile)) fail(`self-test: style file missing: ${styleFile}`);
  const tmp = mkdtempSync(join(tmpdir(), 'mage-arena-format-'));
  const problems = [];
  try {
    copyFileSync(styleFile, join(tmp, '.clang-format'));
    const good = join(tmp, 'Good.cpp');
    const bad = join(tmp, 'Bad.cpp');
    writeFileSync(good, 'void Foo()\n{\n\tint32 Count = 1;\n}\n');
    writeFileSync(bad, 'void Foo(){int32 Count=1;}\n');
    const goodResult = checkFile(clang.path, good, null);
    const badResult = checkFile(clang.path, bad, null);
    if (!goodResult.ok) problems.push(`self-test: a house-style file was rejected\n${goodResult.output}`);
    if (badResult.ok) problems.push('self-test: a misformatted file passed');
    // Changed-line mode: an untouched misformatted line must not fail when only a clean line is checked.
    const mixed = join(tmp, 'Mixed.cpp');
    writeFileSync(mixed, 'void Foo()\n{\n\tint32 Count = 1;\n}\nvoid Bar(){int32 X=1;}\n');
    if (!checkFile(clang.path, mixed, [[3, 3]]).ok) problems.push('self-test: an unchanged misformatted line failed the changed-line check');
    if (checkFile(clang.path, mixed, [[5, 5]]).ok) problems.push('self-test: a changed misformatted line passed');
  } finally {
    rmSync(tmp, { recursive: true, force: true });
  }
  const parsed = changedRanges('+++ b/apps/vr/Game/Source/X.cpp\n@@ -3,0 +4,2 @@\n@@ -9 +11 @@\n@@ -20,4 +23,0 @@\n');
  const xr = JSON.stringify(parsed.get('apps/vr/Game/Source/X.cpp'));
  if (xr !== '[[4,5],[11,11]]') problems.push(`self-test: diff parsing gave ${xr}`);
  if (problems.length) fail(problems.join('\n'));
  console.log(`self-test OK (${clang.version}): house style passes, misformatted code fails, untouched lines are ignored`);
  process.exit(0);
}

function main() {
  const { base, selfTest: doSelf } = parseArgs(process.argv.slice(2));
  if (doSelf) selfTest();
  const clang = findClangFormat();
  if (!clang) missingTool();
  const baseSha = resolveBase(base);
  const diff = git(['diff', '-U0', '--no-color', '--no-ext-diff', '--diff-filter=AMR', baseSha, '--', sourceRel]);
  if (diff.status !== 0) fail(`git diff failed: ${diff.stderr.trim()}`);
  const ranges = changedRanges(diff.stdout);
  const failures = [];
  let checked = 0;
  for (const [file, lineRanges] of ranges) {
    if (!/\.(h|hpp|cpp|inl)$/i.test(file) || !lineRanges.length || !existsSync(join(repoRoot, file))) continue;
    checked++;
    const result = checkFile(clang.path, file, lineRanges);
    if (!result.ok) failures.push(result.output || `${file}: clang-format reported a difference`);
  }
  if (failures.length) {
    console.error(failures.join('\n'));
    console.error(`format-check: ${failures.length} file(s) have changed lines that differ from apps/vr/Game/Source/.clang-format.`);
    console.error('Fix them with: clang-format -i --lines=<from>:<to> <file>   (only the lines you changed)');
    process.exit(1);
  }
  console.log(`format-check OK: ${checked} changed C++ file(s) since ${baseSha.slice(0, 7)} (${clang.version})`);
}

main();
