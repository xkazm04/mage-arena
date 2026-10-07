// Builds the conformance vectors for T06.
// Extracts the pinned kernel, proves the compiler reads the pinned bytes, and
// records scripted traces with the TypeScript oracle.
import { createHash } from 'node:crypto';
import { spawnSync } from 'node:child_process';
import {
  copyFileSync,
  existsSync,
  mkdirSync,
  readFileSync,
  rmSync,
  writeFileSync,
} from 'node:fs';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';

const toolsDir = dirname(fileURLToPath(import.meta.url));
const vrRoot = join(toolsDir, '..', '..');
const repoRoot = join(vrRoot, '..', '..');
const manifestPath = join(vrRoot, 'data', 'PINNED.json');
const pinnedDir = join(vrRoot, 'data', 'pinned');
const outDir = join(vrRoot, 'data', 'conformance');
const sourceRepo = process.env.MAGE_ARENA_ARENA || 'C:/Users/kazda/kiro/mage-arena-tv';
const tsxCandidates = [
  process.env.TSX_CLI,
  join(sourceRepo, 'packages/core/node_modules/tsx/dist/cli.mjs'),
  join(sourceRepo, 'node_modules/tsx/dist/cli.mjs'),
].filter(Boolean);

const compilerReads = [
  'docs/design/baseline-fourteen-nights/design/data/combat.json',
  'docs/design/baseline-fourteen-nights/design/data/enemies.json',
  'docs/design/baseline-fourteen-nights/design/data/arena-tiers.json',
  'docs/design/baseline-fourteen-nights/design/data/spells-water.csv',
  'docs/design/baseline-fourteen-nights/design/data/stats.csv',
  'art/scale-contract-v1.json',
];
const runtimeRel = 'packages/core/src/arena/data/runtime.json';

function fail(message) {
  console.error(message);
  process.exit(1);
}

function sha256(buf) {
  return createHash('sha256').update(buf).digest('hex');
}

function run(cmd, args, opts = {}) {
  const result = spawnSync(cmd, args, { encoding: 'utf8', maxBuffer: 64 * 1024 * 1024, ...opts });
  if (result.error) fail(`${cmd} failed to start: ${result.error.message}`);
  if (result.status !== 0) {
    fail(`${cmd} ${args.join(' ')} exited ${result.status}\n${result.stdout || ''}\n${result.stderr || ''}`);
  }
  return result;
}

function runBuffer(cmd, args) {
  const result = spawnSync(cmd, args, { encoding: 'buffer', maxBuffer: 64 * 1024 * 1024 });
  if (result.error) fail(`${cmd} failed to start: ${result.error.message}`);
  if (result.status !== 0) fail(`${cmd} ${args.join(' ')} exited ${result.status}\n${result.stderr?.toString() || ''}`);
  return result.stdout;
}

const manifest = JSON.parse(readFileSync(manifestPath, 'utf8'));
if (!/^[0-9a-f]{40}$/.test(String(manifest.commit))) {
  fail(`PINNED.json commit "${manifest.commit}" is not a full 40-hex SHA (TV repo ${sourceRepo})`);
}
const expected = new Map(manifest.files.map((file) => [file.path, file]));
const tsxCli = tsxCandidates.find((path) => existsSync(path));
if (!tsxCli) fail(`tsx not found; tried:${tsxCandidates.map((path) => `\n  ${path}`).join('')}`);
if (!existsSync(join(sourceRepo, '.git'))) fail(`source repo not found: ${sourceRepo}`);
if (spawnSync('git', ['-C', sourceRepo, 'cat-file', '-e', `${manifest.commit}^{commit}`]).status !== 0) {
  fail(`PINNED.json commit ${manifest.commit} does not exist in the TV repo ${sourceRepo}`);
}

const cacheName = `kernel-${manifest.commit.slice(0, 7)}`;
const cacheRoot = join(vrRoot, '.cache', cacheName);
rmSync(cacheRoot, { recursive: true, force: true });
mkdirSync(cacheRoot, { recursive: true });

const listed = run('git', ['-C', sourceRepo, 'ls-tree', '-r', '--name-only', manifest.commit, '--', 'packages/core/src/arena', 'packages/core/scripts/compile-arena-data.mjs']).stdout
  .split(/\r?\n/)
  .filter(Boolean);
if (listed.length < 10) fail('git ls-tree returned too few kernel files');
for (const rel of listed) {
  const bytes = runBuffer('git', ['-C', sourceRepo, 'show', `${manifest.commit}:${rel}`]);
  const dest = join(cacheRoot, ...rel.split('/'));
  mkdirSync(dirname(dest), { recursive: true });
  writeFileSync(dest, bytes);
}

function installPinned(rel) {
  const row = expected.get(rel);
  if (!row) fail(`compiler reads ${rel}, which is not in PINNED.json`);
  const src = join(pinnedDir, ...rel.split('/'));
  const bytes = readFileSync(src);
  if (sha256(bytes) !== row.sha256 || bytes.length !== row.bytes) {
    fail(`pinned bytes for ${rel} do not match PINNED.json`);
  }
  const dest = join(cacheRoot, ...rel.split('/'));
  mkdirSync(dirname(dest), { recursive: true });
  writeFileSync(dest, bytes);
  const readBack = readFileSync(dest);
  if (sha256(readBack) !== row.sha256) fail(`cache copy of ${rel} does not match the pin`);
  return dest;
}

const compilerPaths = compilerReads.map(installPinned);
installPinned(runtimeRel);
console.log('Pinned bytes installed for the compiler:');
for (const path of compilerPaths) console.log(`  ${sha256(readFileSync(path))}  ${path}`);

const compiler = join(cacheRoot, 'packages/core/scripts/compile-arena-data.mjs');
run(process.execPath, [compiler], { cwd: dirname(compiler) });
const generated = readFileSync(join(cacheRoot, 'packages/core/src/arena/data.generated.ts'), 'utf8');
const combat = JSON.parse(readFileSync(compilerPaths[0], 'utf8'));
if (!generated.includes(`"simStepHz": ${combat.simStepHz}`) || !generated.includes(`"arcDeg": ${combat.absorb.arcDeg}`)) {
  fail('data.generated.ts does not contain the pinned combat block');
}
console.log('Compiler read the pinned combat, spells, stats, enemies, tiers and scale contract.');

mkdirSync(outDir, { recursive: true });
const oracle = join(toolsDir, 'oracle.mjs');
run(process.execPath, [tsxCli, oracle, '--out', outDir], {
  cwd: vrRoot,
  env: { ...process.env, CONFORMANCE_KERNEL_CACHE: cacheRoot },
});
console.log('Conformance vectors written.');
