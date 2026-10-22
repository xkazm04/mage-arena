// Clip corpus manifest. Node built-ins only.
// The clip header is a closed ten-field contract (HandClip.cpp ExpectFieldCount), so provenance
// cannot ride in the clip. It rides here, keyed by the clip's path under Game/Clips.
// Usage: node tools/clipgen/manifest.mjs --check [dir]    exit 1 on an unlisted or changed clip
//        node tools/clipgen/manifest.mjs --write [dir]    rebuild corpus-manifest.json
// A clip that is not listed, or whose bytes differ from the listed sha256, is quarantined:
// it may be used to explore and may not be counted by a gate.
import crypto from 'node:crypto';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const here = path.dirname(fileURLToPath(import.meta.url));
const MANIFEST = path.join(here, 'corpus-manifest.json');
const mode = process.argv[2];
const dir = path.resolve(process.cwd(), process.argv[3] || path.join('Game', 'Clips'));

if (mode !== '--check' && mode !== '--write') {
  console.error('usage: manifest.mjs --check|--write [dir]');
  process.exit(2);
}

function walk(d, out) {
  for (const e of fs.readdirSync(d, { withFileTypes: true })) {
    const p = path.join(d, e.name);
    if (e.isDirectory()) walk(p, out);
    else if (e.name.endsWith('.jsonl')) out.push(p);
  }
}

const files = [];
walk(dir, files);
files.sort();

const actual = new Map();
for (const f of files) {
  const bytes = fs.readFileSync(f);
  const header = JSON.parse(bytes.toString('utf8').split('\n', 1)[0]);
  const id = path.relative(dir, f).split(path.sep).join('/');
  actual.set(id, {
    sha256: crypto.createHash('sha256').update(bytes).digest('hex'),
    schema: header.schema,
    source: header.source,
    seed: header.seed,
    performer: header.source === 'recorded' ? 'unrecorded' : 'clipgen',
    runtime: header.source === 'recorded' ? 'unrecorded' : 'none',
    device: header.source === 'recorded' ? 'unrecorded' : 'none',
    intent: header.action,
  });
}

if (mode === '--write') {
  const body = { schema: 'mage-arena/clip-manifest@1', clips: Object.fromEntries(actual) };
  fs.writeFileSync(MANIFEST, `${JSON.stringify(body, null, 1)}\n`);
  console.log(`wrote ${actual.size} clips`);
  process.exit(0);
}

if (!fs.existsSync(MANIFEST)) {
  console.error('no corpus-manifest.json: every clip is quarantined');
  process.exit(1);
}
const listed = JSON.parse(fs.readFileSync(MANIFEST, 'utf8')).clips;
const problems = [];
for (const [id, now] of actual) {
  if (!listed[id]) problems.push(`unlisted ${id}`);
  else if (listed[id].sha256 !== now.sha256) problems.push(`changed ${id}`);
}
for (const id of Object.keys(listed)) if (!actual.has(id)) problems.push(`missing ${id}`);
for (const p of problems) console.log(p);
console.log(`${actual.size} clips, ${problems.length} quarantined`);
process.exit(problems.length === 0 ? 0 : 1);
