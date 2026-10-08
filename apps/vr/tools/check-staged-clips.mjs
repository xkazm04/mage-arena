// Staged-clip guard. DefaultGame.ini stages ../Clips whole, so every file under apps/vr/Game/Clips ships in a package.
// Compares the files on disk there with `git ls-files` of the same repo. Read-only: it never deletes or writes.
// Default root is the repo this script sits in; `--root <path>` checks another checkout (git ls-files only, no git status).
import { spawnSync } from 'node:child_process';
import { existsSync, readdirSync, statSync } from 'node:fs';
import { dirname, join, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';

const CLIPS = 'apps/vr/Game/Clips';

function fail(message) {
  console.error(message);
  process.exit(2);
}

function parseArgs(argv) {
  let root = resolve(dirname(fileURLToPath(import.meta.url)), '..', '..', '..');
  for (let i = 0; i < argv.length; i++) {
    const arg = argv[i];
    if (arg === '--root') {
      if (!argv[i + 1]) fail('--root requires a path');
      root = resolve(argv[++i]);
    } else if (arg.startsWith('--root=')) {
      root = resolve(arg.slice('--root='.length));
    } else {
      fail(`unknown argument: ${arg}`);
    }
  }
  return root;
}

function walk(dir, rel, out) {
  for (const entry of readdirSync(dir, { withFileTypes: true })) {
    const childRel = `${rel}/${entry.name}`;
    if (entry.isDirectory()) {
      walk(join(dir, entry.name), childRel, out);
    } else {
      out.set(childRel, statSync(join(dir, entry.name)).size);
    }
  }
}

function tracked(root) {
  const run = spawnSync('git', ['-C', root, 'ls-files', '-z', '--', CLIPS], { maxBuffer: 64 * 1024 * 1024 });
  if (run.status !== 0) {
    fail(`git ls-files failed in ${root}: ${run.stderr ? run.stderr.toString().trim() : run.error}`);
  }
  return new Set(run.stdout.toString('utf8').split('\0').filter(Boolean));
}

function firstFolder(file) {
  const rest = file.slice(CLIPS.length + 1);
  const slash = rest.indexOf('/');
  return slash < 0 ? '(top level)' : rest.slice(0, slash);
}

const root = parseArgs(process.argv.slice(2));
const clipsDir = join(root, CLIPS);
const onDisk = new Map();
if (existsSync(clipsDir)) walk(clipsDir, CLIPS, onDisk);
const inGit = tracked(root);

const extra = [...onDisk.keys()].filter((f) => !inGit.has(f)).sort();
const missing = [...inGit].filter((f) => !onDisk.has(f)).sort();

if (extra.length === 0 && missing.length === 0) {
  let bytes = 0;
  for (const size of onDisk.values()) bytes += size;
  console.log(`staged clips OK: ${onDisk.size} files, ${bytes} bytes`);
  process.exit(0);
}

console.log(`staged clips MISMATCH in ${root}`);
if (extra.length) {
  const groups = new Map();
  for (const f of extra) {
    const g = groups.get(firstFolder(f)) ?? { files: 0, bytes: 0 };
    g.files++;
    g.bytes += onDisk.get(f);
    groups.set(firstFolder(f), g);
  }
  console.log('Extra on disk, not tracked by git:');
  for (const [name, g] of [...groups].sort()) {
    console.log(`  ${name}: ${g.files} files, ${g.bytes} bytes`);
  }
}
if (missing.length) {
  console.log('Tracked by git, missing on disk:');
  for (const f of missing) console.log(`  ${f}`);
}
let extraBytes = 0;
for (const f of extra) extraBytes += onDisk.get(f);
console.log(`Total: ${extra.length} extra files, ${extraBytes} bytes; ${missing.length} missing files.`);
console.log('Why it matters: DefaultGame.ini stages ../Clips whole, so a package ships every file there.');
process.exit(1);
