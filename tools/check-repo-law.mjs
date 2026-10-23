// Machine guard for the repo laws that no other gate enforces. Node built-ins only.
//
// CLAUDE.md and docs/DECISIONS.md state rules that were, until now, carried only by prose: a sweep or a
// force-add could break one and still pass every gate. One sub-check per law, each independently runnable
// (--only <id>), and a --self-test that breaks each law in a throwaway tree to prove the sub-check fires.
// A guard that cannot fail is worse than no guard, so every sub-check below is proven against a violation.
//
//   build-cs        docs/DECISIONS.md 2026-10-16: MageArenaVR.Build.cs keeps its two `if (Target.bBuildEditor)`
//                   blocks SEPARATE (the Niagara one near the top, the UnrealEd/AssetTools one at the end).
//   lfs-binaries    .gitattributes routes binary extensions through Git LFS REPO-WIDE, not only under
//                   apps/vr/art/ (CLAUDE.md "Build": binary assets go through Git LFS).
//   never-committed CLAUDE.md "Build": Binaries/, Intermediate/, Saved/, DerivedDataCache/ and runs/ are
//                   never committed. .gitignore alone does not stop `git add -f`.
//   node-builtins   CLAUDE.md / repo law: the Node tooling is zero-dependency, Node built-ins only, and
//                   there is no package.json.
//
// Usage: node tools/check-repo-law.mjs [--root <repo>] [--only <id>]... [--list] [--self-test]
import {
  existsSync,
  mkdirSync,
  mkdtempSync,
  readFileSync,
  readdirSync,
  rmSync,
  statSync,
  writeFileSync,
} from 'node:fs';
import { spawnSync } from 'node:child_process';
import { tmpdir } from 'node:os';
import { dirname, join, relative, sep } from 'node:path';
import { fileURLToPath } from 'node:url';

const repoRoot = join(dirname(fileURLToPath(import.meta.url)), '..');

function fail(message) {
  console.error(message);
  process.exit(1);
}

function parseArgs(argv) {
  let root = repoRoot;
  let selfTest = false;
  let list = false;
  const only = [];
  for (let i = 0; i < argv.length; i++) {
    const arg = argv[i];
    if (arg === '--self-test') selfTest = true;
    else if (arg === '--list') list = true;
    else if (arg === '--root') {
      root = argv[++i];
      if (!root) fail('--root requires a path');
    } else if (arg.startsWith('--root=')) {
      root = arg.slice('--root='.length);
      if (!root) fail('--root requires a path');
    } else if (arg === '--only') {
      const id = argv[++i];
      if (!id) fail('--only requires a sub-check id');
      only.push(id);
    } else if (arg.startsWith('--only=')) {
      const id = arg.slice('--only='.length);
      if (!id) fail('--only requires a sub-check id');
      only.push(id);
    } else fail(`unknown argument: ${arg}`);
  }
  return { root, selfTest, list, only };
}

function git(root, args) {
  const result = spawnSync('git', ['-C', root, ...args], {
    encoding: 'utf8',
    windowsHide: true,
    timeout: 60000,
    maxBuffer: 32 * 1024 * 1024,
    env: { ...process.env, GIT_PAGER: '', GIT_TERMINAL_PROMPT: '0', PAGER: '' },
  });
  if (result.error) return { ok: false, error: `git ${args[0]} failed to start: ${result.error.message}` };
  if (result.status !== 0) {
    return { ok: false, error: `git ${args.join(' ')} failed: ${(result.stderr || '').trim()}` };
  }
  return { ok: true, stdout: result.stdout ?? '' };
}

// --- build-cs ---------------------------------------------------------------------------------------

const BUILD_CS = join('apps', 'vr', 'Game', 'Source', 'MageArenaVR', 'MageArenaVR.Build.cs');
const EDITOR_BLOCK = /if\s*\(\s*Target\.bBuildEditor\s*\)/g;
const RULING = 'docs/DECISIONS.md 2026-10-16 "the two Build.cs bBuildEditor blocks stay separate"';

function checkBuildCs(root) {
  const full = join(root, BUILD_CS);
  if (!existsSync(full)) return { ok: false, errors: [`missing file: ${BUILD_CS}`] };
  const text = readFileSync(full, 'utf8');
  const offsets = [];
  for (const match of text.matchAll(EDITOR_BLOCK)) offsets.push(match.index);
  if (offsets.length !== 2) {
    return {
      ok: false,
      errors: [
        `${BUILD_CS}: found ${offsets.length} \`if (Target.bBuildEditor)\` block(s), expected exactly 2.`,
        `  The two blocks must stay separate and must not be folded into one (${RULING}).`,
      ],
    };
  }
  // The ruling names which block is which: Niagara near the top, UnrealEd/AssetTools at the end.
  const first = text.slice(offsets[0], offsets[1]);
  const second = text.slice(offsets[1]);
  const errors = [];
  if (!/NiagaraEditor/.test(first)) {
    errors.push(`${BUILD_CS}: the first bBuildEditor block no longer adds NiagaraEditor (${RULING}).`);
  }
  if (!/UnrealEd/.test(second) || !/AssetTools/.test(second)) {
    errors.push(`${BUILD_CS}: the second bBuildEditor block no longer adds UnrealEd and AssetTools (${RULING}).`);
  }
  if (errors.length) return { ok: false, errors };
  const line = (offset) => text.slice(0, offset).split('\n').length;
  return { ok: true, note: `two separate bBuildEditor blocks at :${line(offsets[0])} and :${line(offsets[1])}` };
}

// --- lfs-binaries -----------------------------------------------------------------------------------

// Extensions that must be LFS-tracked ANYWHERE in the repo. The first group was already repo-wide; the
// second was LFS only under apps/vr/art/, so a Blender source file, an EXR or a stray mp3 elsewhere
// committed as a plain blob and got line-ending translation on top (`* text=auto`).
const LFS_EXTENSIONS = [
  'uasset',
  'umap',
  'fbx',
  'png',
  'wav',
  'ogg',
  'blend',
  'glb',
  'exr',
  'tga',
  'psd',
  'mp3',
  'mp4',
  'jpg',
  'jpeg',
  'webp',
];
// Probe paths outside apps/vr/art/, so an art-only rule cannot satisfy the check.
const LFS_PROBE_DIRS = ['', 'apps/vr/Game/Content/', 'apps/vr/tools/'];

function checkLfsBinaries(root) {
  if (!existsSync(join(root, '.gitattributes'))) {
    return { ok: false, errors: ['missing file: .gitattributes'] };
  }
  const probes = [];
  for (const dir of LFS_PROBE_DIRS) {
    for (const ext of LFS_EXTENSIONS) probes.push(`${dir}repo-law-probe.${ext}`);
  }
  // git check-attr reads the real rules, so this tests the semantics rather than the spelling of a line.
  const out = git(root, ['check-attr', 'filter', 'diff', 'merge', '--', ...probes]);
  if (!out.ok) return { ok: false, errors: [out.error] };
  const seen = new Map();
  for (const line of out.stdout.split('\n')) {
    const match = /^(.*): (filter|diff|merge): (.*)$/.exec(line.trim());
    if (!match) continue;
    if (!seen.has(match[1])) seen.set(match[1], {});
    seen.get(match[1])[match[2]] = match[3];
  }
  const errors = [];
  const reported = new Set(); // one line per extension, naming the first location that is not covered
  for (const probe of probes) {
    const ext = probe.split('.').pop();
    if (reported.has(ext)) continue;
    const attrs = seen.get(probe);
    if (!attrs) {
      reported.add(ext);
      errors.push(`.gitattributes: git check-attr reported nothing for ${probe}`);
      continue;
    }
    for (const key of ['filter', 'diff', 'merge']) {
      if (attrs[key] !== 'lfs') {
        reported.add(ext);
        errors.push(`.gitattributes: *.${ext} is not LFS-tracked at '${probe}' (${key}: ${attrs[key]}).`);
        break;
      }
    }
  }
  if (errors.length) {
    errors.push('  Binary assets go through Git LFS repo-wide (CLAUDE.md "Build"). Append a rule; do not rewrite existing ones.');
    return { ok: false, errors };
  }
  return { ok: true, note: `${LFS_EXTENSIONS.length} binary extensions LFS-tracked at ${LFS_PROBE_DIRS.length} probe locations` };
}

// --- never-committed --------------------------------------------------------------------------------

const NEVER_COMMITTED = /(?:^|\/)(Binaries|Intermediate|Saved|DerivedDataCache|runs)\//;

function checkNeverCommitted(root) {
  // The index, not the worktree: .gitignore stops an accidental add, nothing stops `git add -f`.
  const out = git(root, ['ls-files', '-z']);
  if (!out.ok) return { ok: false, errors: [out.error] };
  const tracked = out.stdout.split('\0').filter((p) => p.length > 0);
  const offenders = tracked.filter((p) => NEVER_COMMITTED.test(p));
  if (offenders.length) {
    const shown = offenders.slice(0, 10);
    const errors = shown.map((p) => `tracked but must never be committed: ${p}`);
    if (offenders.length > shown.length) errors.push(`... and ${offenders.length - shown.length} more`);
    errors.push('  Binaries/, Intermediate/, Saved/, DerivedDataCache/ and runs/ are never committed (CLAUDE.md "Build").');
    return { ok: false, errors };
  }
  return { ok: true, note: `${tracked.length} tracked paths, none under a never-committed directory` };
}

// --- node-builtins ----------------------------------------------------------------------------------

const TOOL_DIRS = [join('tools'), join('apps', 'vr', 'tools'), join('.ai')];
const SCRIPT_RE = /\.(mjs|cjs|js)$/i;
const IMPORT_FROM = /^[^\S\n]*import\b[^'"\n]*?\bfrom\s*['"]([^'"\n]+)['"]/gm;
const IMPORT_BARE = /^[^\S\n]*import\s*['"]([^'"\n]+)['"]/gm;
const IMPORT_DYNAMIC = /\bimport\(\s*['"]([^'"\n]+)['"]\s*\)/g;
const REQUIRE_CALL = /\brequire\(\s*['"]([^'"\n]+)['"]\s*\)/g;

function listScripts(dir) {
  const out = [];
  const walk = (current) => {
    let entries;
    try {
      entries = readdirSync(current, { withFileTypes: true });
    } catch {
      return;
    }
    for (const entry of entries) {
      if (entry.name === 'node_modules' || entry.name === '.git') continue;
      const full = join(current, entry.name);
      if (entry.isDirectory()) walk(full);
      else if (SCRIPT_RE.test(entry.name)) out.push(full);
    }
  };
  walk(dir);
  return out.sort();
}

function isAllowedSpecifier(spec) {
  if (spec.startsWith('./') || spec.startsWith('../')) return true;
  if (spec.startsWith('node:')) return true;
  if (spec.startsWith('file:') || spec.startsWith('data:')) return true;
  if (/^[A-Za-z]:[\\/]/.test(spec) || spec.startsWith('/')) return true;
  return false;
}

function checkNodeBuiltins(root) {
  const errors = [];
  let fileCount = 0;
  for (const packageJson of ['package.json', join('tools', 'package.json'), join('apps', 'vr', 'tools', 'package.json')]) {
    if (existsSync(join(root, packageJson))) {
      errors.push(`${packageJson.split(sep).join('/')} exists: the Node tooling is zero-dependency with no package.json.`);
    }
  }
  for (const dir of TOOL_DIRS) {
    const full = join(root, dir);
    if (!existsSync(full) || !statSync(full).isDirectory()) continue;
    for (const file of listScripts(full)) {
      fileCount++;
      const text = readFileSync(file, 'utf8');
      const rel = relative(root, file).split(sep).join('/');
      for (const re of [IMPORT_FROM, IMPORT_BARE, IMPORT_DYNAMIC, REQUIRE_CALL]) {
        re.lastIndex = 0;
        for (const match of text.matchAll(re)) {
          if (isAllowedSpecifier(match[1])) continue;
          const line = text.slice(0, match.index).split('\n').length;
          errors.push(`${rel}:${line}: non-built-in import '${match[1]}'`);
        }
      }
    }
  }
  if (errors.length) {
    errors.push('  Node built-ins only (`node:` prefix) or a relative path. No runtime dependencies, no package.json.');
    return { ok: false, errors };
  }
  return { ok: true, note: `${fileCount} tool scripts, every import is node: or relative` };
}

// --- registry ---------------------------------------------------------------------------------------

const CHECKS = [
  { id: 'build-cs', title: 'Build.cs keeps two separate bBuildEditor blocks', run: checkBuildCs },
  { id: 'lfs-binaries', title: 'binary extensions are LFS-tracked repo-wide', run: checkLfsBinaries },
  { id: 'never-committed', title: 'no Binaries/Intermediate/Saved/DerivedDataCache/runs is tracked', run: checkNeverCommitted },
  { id: 'node-builtins', title: 'Node tooling imports built-ins only, no package.json', run: checkNodeBuiltins },
];

function byId(id) {
  const found = CHECKS.find((c) => c.id === id);
  if (!found) fail(`unknown sub-check: ${id} (known: ${CHECKS.map((c) => c.id).join(', ')})`);
  return found;
}

// --- self-test --------------------------------------------------------------------------------------
// Each case builds a throwaway tree that breaks exactly one law and asserts the sub-check goes red, then
// asserts the same sub-check is green on the real repo. A sub-check that passes both ways is a dead guard.

function writeFile(root, rel, text) {
  const full = join(root, ...rel.split('/'));
  mkdirSync(dirname(full), { recursive: true });
  writeFileSync(full, text);
  return full;
}

function initRepo(root) {
  const out = git(root, ['init', '-q']);
  if (!out.ok) throw new Error(out.error);
}

const FOLDED_BUILD_CS = `public class MageArenaVR : ModuleRules {
  public MageArenaVR(ReadOnlyTargetRules Target) : base(Target) {
    if (Target.bBuildEditor) {
      PrivateDependencyModuleNames.AddRange(new string[] { "NiagaraEditor", "UnrealEd", "AssetTools" });
    }
  }
}
`;

const SEPARATE_BUILD_CS = `public class MageArenaVR : ModuleRules {
  public MageArenaVR(ReadOnlyTargetRules Target) : base(Target) {
    if (Target.bBuildEditor) {
      PrivateDependencyModuleNames.Add("NiagaraEditor");
    }
    PublicDependencyModuleNames.Add("Core");
    if (Target.bBuildEditor) {
      PrivateDependencyModuleNames.AddRange(new string[] { "UnrealEd", "AssetTools" });
    }
  }
}
`;

// The fixtures below assemble import syntax from pieces on purpose: written out whole, this file's own
// node-builtins sub-check would flag its own test data.
const IMPORT_KEYWORD = ['im', 'port'].join('');
const BAD_STATIC_IMPORT = `${IMPORT_KEYWORD} chalk from 'chalk';\nconsole.log(chalk);\n`;
const BAD_DYNAMIC_IMPORT = `const m = await ${IMPORT_KEYWORD}('yaml');\nexport default m;\n`;
const GOOD_IMPORTS = [
  `${IMPORT_KEYWORD} { join } from 'node:path';`,
  `${IMPORT_KEYWORD} './other.mjs';`,
  `const x = await ${IMPORT_KEYWORD}('../lib/y.mjs');`,
  'export { join, x };',
  '',
].join('\n');

const ART_ONLY_ATTRS = `* text=auto
*.uasset filter=lfs diff=lfs merge=lfs -text
apps/vr/art/**/*.jpg filter=lfs diff=lfs merge=lfs -text
`;

function selfTestCases(root) {
  const cases = [];

  cases.push({
    id: 'build-cs',
    name: 'the two bBuildEditor blocks folded into one',
    build: (tmp) => {
      writeFile(tmp, BUILD_CS.split(sep).join('/'), FOLDED_BUILD_CS);
      return tmp;
    },
    expect: /found 1 `if \(Target\.bBuildEditor\)` block\(s\), expected exactly 2/,
  });
  cases.push({
    id: 'build-cs',
    name: 'two separate blocks are accepted',
    build: (tmp) => {
      writeFile(tmp, BUILD_CS.split(sep).join('/'), SEPARATE_BUILD_CS);
      return tmp;
    },
    expect: null,
  });
  cases.push({
    id: 'lfs-binaries',
    name: 'a .gitattributes that covers binaries only under apps/vr/art/',
    build: (tmp) => {
      initRepo(tmp);
      writeFile(tmp, '.gitattributes', ART_ONLY_ATTRS);
      return tmp;
    },
    expect: /\*\.blend is not LFS-tracked/,
  });
  cases.push({
    id: 'lfs-binaries',
    name: "this repo's own .gitattributes in a throwaway clone",
    build: (tmp) => {
      initRepo(tmp);
      writeFile(tmp, '.gitattributes', readFileSync(join(root, '.gitattributes'), 'utf8'));
      return tmp;
    },
    expect: null,
  });
  cases.push({
    id: 'never-committed',
    name: 'a force-added apps/vr/Game/Binaries path',
    build: (tmp) => {
      initRepo(tmp);
      writeFile(tmp, '.gitignore', 'apps/vr/Game/Binaries/\n');
      writeFile(tmp, 'apps/vr/Game/Binaries/MageArenaVR.dll', 'stub');
      const out = git(tmp, ['add', '-f', 'apps/vr/Game/Binaries/MageArenaVR.dll']);
      if (!out.ok) throw new Error(out.error);
      return tmp;
    },
    expect: /tracked but must never be committed: apps\/vr\/Game\/Binaries\/MageArenaVR\.dll/,
  });
  cases.push({
    id: 'never-committed',
    name: 'an ignored-but-not-added Binaries path',
    build: (tmp) => {
      initRepo(tmp);
      writeFile(tmp, '.gitignore', 'apps/vr/Game/Binaries/\n');
      writeFile(tmp, 'apps/vr/Game/Binaries/MageArenaVR.dll', 'stub');
      writeFile(tmp, 'tools/ok.mjs', "import { readFileSync } from 'node:fs';\n");
      const out = git(tmp, ['add', 'tools/ok.mjs']);
      if (!out.ok) throw new Error(out.error);
      return tmp;
    },
    expect: null,
  });
  cases.push({
    id: 'node-builtins',
    name: 'a bare npm import in a tool script',
    build: (tmp) => {
      writeFile(tmp, 'tools/bad.mjs', BAD_STATIC_IMPORT);
      return tmp;
    },
    expect: /tools\/bad\.mjs:1: non-built-in import 'chalk'/,
  });
  cases.push({
    id: 'node-builtins',
    name: 'a dynamic import of a bare specifier',
    build: (tmp) => {
      writeFile(tmp, 'tools/dyn.mjs', BAD_DYNAMIC_IMPORT);
      return tmp;
    },
    expect: /tools\/dyn\.mjs:1: non-built-in import 'yaml'/,
  });
  cases.push({
    id: 'node-builtins',
    name: 'a package.json at the repo root',
    build: (tmp) => {
      writeFile(tmp, 'package.json', '{"name":"x"}\n');
      return tmp;
    },
    expect: /package\.json exists: the Node tooling is zero-dependency/,
  });
  cases.push({
    id: 'node-builtins',
    name: 'node: and relative imports stay silent',
    build: (tmp) => {
      writeFile(tmp, 'tools/good.mjs', GOOD_IMPORTS);
      writeFile(tmp, 'tools/other.mjs', 'export const a = 1;\n');
      return tmp;
    },
    expect: null,
  });

  return cases;
}

function selfTest(root) {
  const lines = [];
  const problems = [];
  const fired = new Set();

  for (const testCase of selfTestCases(root)) {
    const tmp = mkdtempSync(join(tmpdir(), 'mage-arena-repo-law-'));
    try {
      testCase.build(tmp);
      const result = byId(testCase.id).run(tmp);
      const text = result.ok ? '' : result.errors.join('\n');
      if (testCase.expect) {
        if (result.ok) {
          problems.push(`self-test: ${testCase.id}: ${testCase.name} was NOT detected (the guard is dead)`);
        } else if (!testCase.expect.test(text)) {
          problems.push(`self-test: ${testCase.id}: ${testCase.name} failed for a different reason:\n${text}`);
        } else {
          fired.add(testCase.id);
          lines.push(`  ${testCase.id}: detected ${testCase.name}`);
        }
      } else if (!result.ok) {
        problems.push(`self-test: ${testCase.id}: ${testCase.name} was wrongly flagged:\n${text}`);
      } else {
        lines.push(`  ${testCase.id}: clean on ${testCase.name}`);
      }
    } catch (err) {
      problems.push(`self-test: ${testCase.id}: ${testCase.name} threw: ${err instanceof Error ? err.message : String(err)}`);
    } finally {
      rmSync(tmp, { recursive: true, force: true });
    }
  }

  for (const check of CHECKS) {
    if (!fired.has(check.id)) {
      problems.push(`self-test: ${check.id} has no case that makes it fail: a sub-check with no proven violation is not a guard`);
    }
  }

  if (problems.length) {
    console.error(problems.join('\n'));
    process.exit(1);
  }
  console.log(lines.join('\n'));
  console.log(`self-test OK: all ${CHECKS.length} sub-checks fire on a synthetic violation and stay silent on a clean tree`);
  process.exit(0);
}

// --- main -------------------------------------------------------------------------------------------

function main() {
  const { root, selfTest: doSelf, list, only } = parseArgs(process.argv.slice(2));
  if (list) {
    for (const check of CHECKS) console.log(`${check.id.padEnd(16)} ${check.title}`);
    process.exit(0);
  }
  if (doSelf) selfTest(root);
  const selected = only.length ? only.map(byId) : CHECKS;
  const errors = [];
  const notes = [];
  for (const check of selected) {
    const result = check.run(root);
    if (result.ok) notes.push(`  ${check.id}: ${result.note}`);
    else errors.push(...result.errors.map((line) => (line.startsWith('  ') ? line : `${check.id}: ${line}`)));
  }
  if (errors.length) {
    console.error(errors.join('\n'));
    console.error(`repo-law FAILED: ${errors.filter((l) => !l.startsWith('  ')).length} violation(s)`);
    process.exit(1);
  }
  console.log(notes.join('\n'));
  console.log(`repo-law OK: ${selected.length}/${CHECKS.length} sub-checks`);
}

main();
