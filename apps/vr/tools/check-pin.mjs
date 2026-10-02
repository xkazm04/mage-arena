// Hash gate for data/PINNED.json. Node built-ins only.
// git show is captured as a Buffer. A shell redirect would rewrite line endings.
import { createHash } from 'node:crypto';
import { spawnSync } from 'node:child_process';
import {
  cpSync,
  existsSync,
  mkdtempSync,
  readFileSync,
  readdirSync,
  rmSync,
  statSync,
  writeFileSync,
} from 'node:fs';
import { tmpdir } from 'node:os';
import { dirname, join, relative, sep } from 'node:path';
import { fileURLToPath } from 'node:url';

const repoRoot = join(dirname(fileURLToPath(import.meta.url)), '..');
const manifestPath = join(repoRoot, 'data', 'PINNED.json');
const defaultPinnedDir = join(repoRoot, 'data', 'pinned');

function fail(message) {
  console.error(message);
  process.exit(1);
}

function parseArgs(argv) {
  let source = null;
  let selfTest = false;
  for (let i = 0; i < argv.length; i++) {
    const arg = argv[i];
    if (arg === '--self-test') {
      selfTest = true;
    } else if (arg === '--source') {
      source = argv[++i];
      if (!source) fail('--source requires a path');
    } else if (arg.startsWith('--source=')) {
      source = arg.slice('--source='.length);
      if (!source) fail('--source requires a path');
    } else {
      fail(`unknown argument: ${arg}`);
    }
  }
  if (selfTest && source) fail('--self-test and --source are separate modes');
  return { source, selfTest };
}

function sha256(buf) {
  return createHash('sha256').update(buf).digest('hex');
}

function loadManifest() {
  let raw;
  try {
    raw = readFileSync(manifestPath);
  } catch (err) {
    fail(`cannot read ${manifestPath}: ${err.message}`);
  }
  let manifest;
  try {
    manifest = JSON.parse(raw.toString('utf8'));
  } catch (err) {
    fail(`PINNED.json is not valid JSON: ${err.message}`);
  }
  if (!manifest || !Array.isArray(manifest.files)) fail('PINNED.json has no files array');
  if (typeof manifest.commit !== 'string' || !/^[0-9a-f]{7,40}$/i.test(manifest.commit)) {
    fail('PINNED.json commit is missing or not a hex sha');
  }
  return manifest;
}

function insidePath(root, relPosix) {
  if (typeof relPosix !== 'string' || relPosix.length === 0 || relPosix.includes('\\')) return null;
  const parts = relPosix.split('/');
  if (parts.some((part) => part === '' || part === '.' || part === '..')) return null;
  return join(root, ...parts);
}

function listFiles(dir) {
  const out = [];
  const walk = (current) => {
    for (const entry of readdirSync(current, { withFileTypes: true })) {
      const full = join(current, entry.name);
      if (entry.isDirectory()) walk(full);
      else out.push(relative(dir, full).split(sep).join('/'));
    }
  };
  walk(dir);
  return out;
}

function gitShow(source, commit, relPath) {
  const result = spawnSync('git', ['-C', source, 'show', `${commit}:${relPath}`], {
    maxBuffer: 16 * 1024 * 1024,
    windowsHide: true,
    timeout: 30000,
    env: { ...process.env, GIT_PAGER: '', GIT_TERMINAL_PROMPT: '0', PAGER: '' },
  });
  if (result.error) return { ok: false, error: `git show failed to start for ${relPath}: ${result.error.message}` };
  if (result.status !== 0) {
    const errText = (result.stderr ?? Buffer.alloc(0)).toString('utf8').trim();
    return { ok: false, error: `git show ${commit}:${relPath} failed: ${errText}` };
  }
  return { ok: true, buffer: result.stdout ?? Buffer.alloc(0) };
}

function checkTree(manifest, pinnedDir, source) {
  const errors = [];
  const listed = new Set();
  if (!existsSync(pinnedDir) || !statSync(pinnedDir).isDirectory()) {
    errors.push(`missing directory: ${pinnedDir}`);
    return { ok: false, errors };
  }
  for (const entry of manifest.files) {
    const rel = entry && entry.path;
    if (listed.has(rel)) errors.push(`duplicate manifest path: ${rel}`);
    listed.add(rel);
    const full = insidePath(pinnedDir, rel);
    if (!full) {
      errors.push(`invalid manifest path: ${rel}`);
      continue;
    }
    if (!existsSync(full) || !statSync(full).isFile()) {
      errors.push(`missing file: ${rel}`);
      continue;
    }
    const buf = readFileSync(full);
    if (sha256(buf) !== entry.sha256) errors.push(`sha256 mismatch: ${rel}`);
    if (typeof entry.bytes === 'number' && buf.length !== entry.bytes) {
      errors.push(`size mismatch: ${rel} (${buf.length} bytes, manifest ${entry.bytes})`);
    }
    if (source) {
      const remote = gitShow(source, manifest.commit, rel);
      if (!remote.ok) errors.push(remote.error);
      else if (!remote.buffer.equals(buf)) errors.push(`source mismatch: ${rel}`);
    }
  }
  let found;
  try {
    found = listFiles(pinnedDir);
  } catch (err) {
    errors.push(`cannot list ${pinnedDir}: ${err.message}`);
    found = [];
  }
  for (const path of found) {
    if (!listed.has(path)) errors.push(`unlisted file: ${path}`);
  }
  if (errors.length) return { ok: false, errors };
  return { ok: true, count: manifest.files.length };
}

function selfTest(manifest) {
  const tmp = mkdtempSync(join(tmpdir(), 'mage-arena-pin-'));
  let exitCode = 1;
  let line = 'self-test failed';
  try {
    cpSync(defaultPinnedDir, tmp, { recursive: true });
    const clean = checkTree(manifest, tmp, null);
    if (!clean.ok) {
      line = `self-test: clean copy failed\n${clean.errors.join('\n')}`;
    } else if (!manifest.files.length) {
      line = 'self-test: manifest has no files to corrupt';
    } else {
      const rel = manifest.files[0].path;
      const full = insidePath(tmp, rel);
      const buf = readFileSync(full);
      if (buf.length < 1) {
        line = `self-test: ${rel} is empty`;
      } else {
        buf[0] ^= 0x01;
        writeFileSync(full, buf);
        const dirty = checkTree(manifest, tmp, null);
        const detected = !dirty.ok && dirty.errors.some((err) => err === `sha256 mismatch: ${rel}`);
        if (detected) {
          exitCode = 0;
          line = `corruption detected: ${rel}`;
        } else if (dirty.ok) {
          line = 'self-test: corruption was not detected';
        } else {
          line = `self-test: check failed for a different reason\n${dirty.errors.join('\n')}`;
        }
      }
    }
  } catch (err) {
    exitCode = 1;
    line = `self-test: ${err instanceof Error ? err.message : String(err)}`;
  } finally {
    rmSync(tmp, { recursive: true, force: true });
  }
  if (exitCode === 0) console.log(line);
  else console.error(line);
  process.exit(exitCode);
}

function main() {
  const { source, selfTest: doSelf } = parseArgs(process.argv.slice(2));
  const manifest = loadManifest();
  if (doSelf) selfTest(manifest);
  const result = checkTree(manifest, defaultPinnedDir, source);
  if (!result.ok) {
    console.error(result.errors.join('\n'));
    process.exit(1);
  }
  const short = manifest.commit.slice(0, 7);
  console.log(`pin OK: ${result.count} files @ ${short}`);
  if (source) console.log(`pin OK, source match for ${result.count} files`);
}

main();
