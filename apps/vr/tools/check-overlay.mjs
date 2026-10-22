// Overlay gate for apps/vr/data/vr/. Node built-ins only. Read-only over the data tree: the self-test works on a
// temp copy. Finding A3(a), docs/research/ARCHITECTURE-REVIEW-2026-10-21.md. It checks that the overlay parses and
// that CSV headers match the loader; it does not reimplement the loader's semantic rules (docs/research/OVERLAY-CHECK-2026-10-22.md).
import { createHash } from 'node:crypto';
import { cpSync, mkdtempSync, readFileSync, readdirSync, rmSync, writeFileSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { dirname, join, relative, sep } from 'node:path';
import { fileURLToPath } from 'node:url';

const repoRoot = join(dirname(fileURLToPath(import.meta.url)), '..');
const defaultOverlayDir = join(repoRoot, 'data', 'vr');

// Required CSV headers, keyed by path relative to the overlay root.
// Source: apps/vr/Game/Source/MageArenaVR/Kernel/KernelDataAir.cpp:30 (AirHeader), compared at :348 after the
// loader drops a leading U+FEFF (:338-341) and trims the end of line 1 (TrimEnd).
const CSV_HEADERS = {
  'schools/spells-air.csv':
    'id,name,tier,shape,cast_s,cooldown_s,mana,damage,blockable,telegraph_s,range_m,momentum_gain,notes',
};

function fail(message) {
  console.error(message);
  process.exit(1);
}

function parseArgs(argv) {
  let selfTest = false;
  let verbose = false;
  for (const arg of argv) {
    if (arg === '--self-test') selfTest = true;
    else if (arg === '--verbose') verbose = true;
    else fail(`unknown argument: ${arg}`);
  }
  return { selfTest, verbose };
}

// The loader drops a leading U+FEFF before it reads a file.
const LINE_BREAK = new RegExp(['\r\n', '\n', '\r'].join('|'));

function loaderText(buf) {
  const text = buf.toString('utf8');
  return text.charCodeAt(0) === 0xfeff ? text.slice(1) : text;
}

// Every overlay file is text; hash it as LF so a CRLF checkout (core.autocrlf) names the same overlay.
function toLf(buf) {
  const out = [];
  for (let i = 0; i < buf.length; i++) if (!(buf[i] === 0x0d && buf[i + 1] === 0x0a)) out.push(buf[i]);
  return Buffer.from(out);
}

function sha256(buf) {
  return createHash('sha256').update(buf).digest('hex');
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
  return out.sort();
}

function jsonError(text, err) {
  const m = /position (\d+)/.exec(err.message);
  if (!m || /line [0-9]+ /.test(err.message)) return err.message;
  const pos = Number(m[1]);
  const before = text.slice(0, pos).split('\n');
  return `${err.message} (line ${before.length}, column ${before[before.length - 1].length + 1})`;
}

function checkOverlay(dir) {
  const errors = [];
  let files;
  try {
    files = listFiles(dir);
  } catch (err) {
    return { ok: false, errors: [`cannot list ${dir}: ${err.message}`], files: [] };
  }
  const hashes = [];
  for (const rel of files) {
    const buf = readFileSync(join(dir, ...rel.split('/')));
    const lf = toLf(buf);
    hashes.push({ rel, sha256: sha256(lf), buf, lf });
    if (rel.endsWith('.json')) {
      const text = loaderText(buf);
      try {
        JSON.parse(text);
      } catch (err) {
        errors.push(`${rel}: ${jsonError(text, err)}`);
      }
    }
  }
  for (const [rel, header] of Object.entries(CSV_HEADERS)) {
    const entry = hashes.find((f) => f.rel === rel);
    if (!entry) {
      errors.push(`${rel}: missing (the loader requires it)`);
      continue;
    }
    const first = loaderText(entry.buf).split(LINE_BREAK, 1)[0].trimEnd();
    if (first !== header) errors.push(`${rel}: header is '${first}', loader requires '${header}'`);
  }
  for (const f of hashes) {
    if (f.rel.startsWith('schools/') && f.rel.endsWith('.csv') && !(f.rel in CSV_HEADERS)) {
      errors.push(`${f.rel}: CSV under schools/ has no header rule in check-overlay.mjs`);
    }
  }
  return { ok: errors.length === 0, errors, files: hashes };
}

function overlayHash(files) {
  const h = createHash('sha256');
  for (const f of files) {
    h.update(f.rel, 'utf8');
    h.update('\n');
    h.update(f.lf);
  }
  return h.digest('hex');
}

function selfTest() {
  const tmp = mkdtempSync(join(tmpdir(), 'mage-arena-overlay-'));
  const lines = [];
  let ok = true;
  try {
    cpSync(defaultOverlayDir, tmp, { recursive: true });
    const clean = checkOverlay(tmp);
    if (!clean.ok) {
      console.error(`self-test: clean copy failed\n${clean.errors.join('\n')}`);
      process.exit(1);
    }
    lines.push(`clean copy passes (${clean.files.length} files)`);
    const jsonRel = 'combat.vr.json';
    const jsonBuf = readFileSync(join(tmp, jsonRel), 'utf8');
    const cases = [
      { name: 'truncated JSON', rel: jsonRel, text: jsonBuf.slice(0, Math.floor(jsonBuf.length / 2)) },
      { name: 'JSON with a trailing comma', rel: 'day.json', text: null },
      { name: 'CSV with a renamed column', rel: 'schools/spells-air.csv', text: null },
    ];
    const original = new Map();
    for (const c of cases) original.set(c.rel, readFileSync(join(tmp, ...c.rel.split('/')), 'utf8'));
    cases[1].text = original.get('day.json').replace(/\}\s*$/, ',}');
    cases[2].text = original.get(cases[2].rel).replace('cooldown_s', 'cooldown');
    for (const c of cases) {
      const full = join(tmp, ...c.rel.split('/'));
      writeFileSync(full, c.text);
      const dirty = checkOverlay(tmp);
      const hit = !dirty.ok && dirty.errors.some((e) => e.startsWith(`${c.rel}:`));
      if (hit) lines.push(`detected ${c.name}: ${dirty.errors.find((e) => e.startsWith(`${c.rel}:`))}`);
      else {
        ok = false;
        lines.push(`NOT DETECTED ${c.name} in ${c.rel}`);
      }
      writeFileSync(full, original.get(c.rel));
    }
  } finally {
    rmSync(tmp, { recursive: true, force: true });
  }
  if (ok) console.log(lines.join('\n'));
  else console.error(`self-test failed\n${lines.join('\n')}`);
  process.exit(ok ? 0 : 1);
}

function main() {
  const { selfTest: doSelf, verbose } = parseArgs(process.argv.slice(2));
  if (doSelf) selfTest();
  const result = checkOverlay(defaultOverlayDir);
  if (!result.ok) {
    console.error(result.errors.join('\n'));
    process.exit(1);
  }
  if (verbose) for (const f of result.files) console.log(`${f.sha256}  ${f.rel}`);
  console.log(`overlay OK: ${result.files.length} files`);
  console.log(`overlay sha256 ${overlayHash(result.files)}`);
}

main();
