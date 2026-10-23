// Checks apps/vr/art/environment-kit/crowd/ against its own manifest.json. Node built-ins only; needs the LFS files pulled.
//   node apps/vr/tools/crowd/verify.mjs
// Re-running crowd.py build must leave this green and `git status` clean (the build is deterministic).
import { createHash } from 'node:crypto';
import { existsSync, readFileSync } from 'node:fs';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';

const root = join(dirname(fileURLToPath(import.meta.url)), '..', '..', 'art', 'environment-kit');
const dir = join(root, 'crowd');
const problems = [];
const check = (ok, msg) => { if (!ok) problems.push(msg); };
const sha = (p) => createHash('sha256').update(readFileSync(p)).digest('hex');
// the JSON files are text: a Windows checkout may have turned LF into CRLF, so hash them as LF
const shaText = (p) => createHash('sha256').update(readFileSync(p, 'utf8').split('\r\n').join('\n')).digest('hex');
const isPow2 = (n) => n > 0 && (n & (n - 1)) === 0;

const man = JSON.parse(readFileSync(join(dir, 'manifest.json'), 'utf8'));
const sheet = join(root, '04-crowd-cards.jpg');
check(existsSync(sheet) && sha(sheet) === man.sheet.sha256, 'sheet sha256 differs from manifest (a pointer file? run git lfs pull)');

const png = readFileSync(join(dir, man.atlas.file));
const [w, h] = [png.readUInt32BE(16), png.readUInt32BE(20)];
check(png.subarray(0, 8).toString('hex') === '89504e470d0a1a0a', 'atlas is not a PNG (an LFS pointer?)');
check(w === man.atlas.sizePx[0] && h === man.atlas.sizePx[1], `atlas size ${w}x${h} differs from manifest`);
check(isPow2(w) && isPow2(h) && w <= 2048 && h <= 2048, `atlas ${w}x${h} is not power-of-two within 2048`);
check(sha(join(dir, man.atlas.file)) === man.atlas.sha256, 'atlas sha256 differs from manifest');

let tris = 0;
for (const [name, v] of Object.entries(man.variants)) {
  const p = join(dir, v.file);
  check(existsSync(p) && sha(p) === v.sha256, `${name}: FBX sha256 differs from manifest`);
  const r = v.uvRect;
  check(r.u0 >= 0 && r.v0 >= 0 && r.u1 <= 1 && r.v1 <= 1 && r.u0 < r.u1 && r.v0 < r.v1, `${name}: uvRect outside 0..1`);
  const [px, py, pw, ph] = v.pxRect;
  check(Math.abs(r.u0 - px / w) < 1e-5 && Math.abs(r.u1 - (px + pw) / w) < 1e-5 && Math.abs(r.v1 - (1 - py / h)) < 1e-5, `${name}: uvRect and pxRect disagree`);
  check(v.tris === 2, `${name}: ${v.tris} tris, expected 2 (one quad)`);
  check(v.boundsM.min[2] === 0, `${name}: pivot is not at the feet`);
}
const pl = join(dir, man.placement.file);
check(shaText(pl) === man.placement.sha256, 'placement.json sha256 differs from manifest');
const cards = JSON.parse(readFileSync(pl, 'utf8')).cards;
check(cards.length === man.placement.cardCount, `placement has ${cards.length} cards, manifest says ${man.placement.cardCount}`);
for (const c of cards) tris += man.variants[c.variant].tris;
check(tris === man.placement.totalTris, `placement tris ${tris}, manifest says ${man.placement.totalTris}`);

if (problems.length) { console.error('crowd verify FAILED:\n- ' + problems.join('\n- ')); process.exit(1); }
console.log(`OK: ${Object.keys(man.variants).length} variants, atlas ${w}x${h}, ${cards.length} cards, ${tris} tris`);
