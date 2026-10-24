// Offline proof of the key order, the night roots and the call-settling helpers both Leonardo clients share
// (leonardo.mjs and keyart/leonardo-keyart.mjs).
//
//   node --test apps/vr/tools/assets/leonardo-client.test.mjs
import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { ENV_FILES, NIGHT_ROOTS, isImage, nightArts, nightTotal, pollGeneration, readKey, settleCredits, writeLog } from './leonardo-client.mjs';

test('settleCredits: the API-reported cost wins over the balance difference', () => {
  assert.deepEqual(settleCredits({ reported: 7, before: 100, after: 90, generationId: 'g', expected: 14 }), { credits: 7, creditsSource: 'api' });
});
test('settleCredits: a positive balance difference is used when the API reports none', () => {
  assert.deepEqual(settleCredits({ reported: null, before: 100, after: 94, generationId: 'g', expected: 14 }), { credits: 6, creditsSource: 'balance' });
});
test('settleCredits: a generation with no other evidence counts at the expected cost', () => {
  assert.deepEqual(settleCredits({ reported: null, before: null, after: 90, generationId: 'g', expected: 14 }), { credits: 14, creditsSource: 'fallback' });
  assert.deepEqual(settleCredits({ reported: null, before: 90, after: 90, generationId: 'g', expected: 14 }), { credits: 14, creditsSource: 'fallback' });
});
test('settleCredits: no generation and no evidence costs nothing', () => {
  assert.deepEqual(settleCredits({ reported: null, before: null, after: null, generationId: null, expected: 14 }), { credits: 0, creditsSource: 'no-generation' });
});

test('pollGeneration: returns the finished generation and its status', async () => {
  const seen = [];
  const answers = [{ status: 'PENDING' }, { status: 'COMPLETE', generated_images: [{ url: 'u' }] }];
  const fetchGen = async (id) => { seen.push(id); return answers.shift(); };
  const r = await pollGeneration(fetchGen, 'gid', { pollMs: 0, limitMs: 1000 });
  assert.equal(r.status, 'COMPLETE');
  assert.equal(r.generation.generated_images[0].url, 'u');
  assert.deepEqual(seen, ['gid', 'gid']);
});
test('pollGeneration: gives up with TIMEOUT once the limit has passed', async () => {
  const r = await pollGeneration(async () => ({ status: 'PENDING' }), 'gid', { pollMs: 5, limitMs: 20 });
  assert.equal(r.status, 'TIMEOUT');
});
test('pollGeneration: a missing generation reads as UNKNOWN until the limit', async () => {
  const r = await pollGeneration(async () => undefined, 'gid', { pollMs: 5, limitMs: 20 });
  assert.equal(r.status, 'TIMEOUT');
});

test('isImage: PNG, JPEG and WebP bodies pass, an error page or an empty body does not (scan 2026-10-22-c, P2)', () => {
  assert.ok(isImage(Buffer.from([0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a, 0, 0])));
  assert.ok(isImage(Buffer.from([0xff, 0xd8, 0xff, 0xe0, 0, 0x10])));
  assert.ok(isImage(Buffer.from('RIFF\x10\x00\x00\x00WEBPVP8 ', 'latin1')));
  assert.ok(!isImage(Buffer.from('<html>AccessDenied</html>')));
  assert.ok(!isImage(Buffer.from('RIFF\x10\x00\x00\x00WAVEfmt ', 'latin1')));
  assert.ok(!isImage(Buffer.alloc(0)));
});

// Key order (owner, 2026-10-24): the environment, then personas' .env, then bp's legacy .env. Only temp files and a fake
// key are used; the real .env files are never read.
const FAKE = 'fakekey0-0000-4000-8000-keyorderproof';
const tmpTree = () => fs.mkdtempSync(path.join(os.tmpdir(), 'leonardo-client-'));
function envFiles(dir, personas, bp) {
  const files = [path.join(dir, 'personas.env'), path.join(dir, 'bp.env')];
  if (personas != null) fs.writeFileSync(files[0], personas);
  if (bp != null) fs.writeFileSync(files[1], bp);
  return files;
}

test('readKey: the env var beats personas/.env, which beats bp/.env', () => {
  const dir = tmpTree();
  const files = envFiles(dir, `OTHER=1\r\nLEONARDO_API_KEY="${FAKE}-personas"\r\n`, `LEONARDO_API_KEY=${FAKE}-bp\n`);
  assert.equal(readKey({ env: { LEONARDO_API_KEY: `${FAKE}-env` }, files }), `${FAKE}-env`);
  assert.equal(readKey({ env: {}, files }), `${FAKE}-personas`);
  assert.equal(readKey({ env: { LEONARDO_API_KEY: '  ' }, files }), `${FAKE}-personas`);
  fs.rmSync(files[0]);
  assert.equal(readKey({ env: {}, files }), `${FAKE}-bp`);
  fs.writeFileSync(files[0], 'LEONARDO_API_KEY=\nNEXT=not-a-key\n');
  assert.equal(readKey({ env: {}, files }), `${FAKE}-bp`, 'an empty personas line falls through, never reads the next line');
  fs.rmSync(dir, { recursive: true });
});

test('readKey: with no key anywhere the error names the files it tried and holds no key', () => {
  const dir = tmpTree();
  const files = envFiles(dir, `LEONARDO_KEY_OLD=${FAKE}\n`, null);
  assert.throws(() => readKey({ env: {}, files }), (e) => {
    assert.ok(e.message.includes(files[0]) && e.message.includes(files[1]), e.message);
    assert.ok(!e.message.includes(FAKE));
    return true;
  });
  fs.rmSync(dir, { recursive: true });
});

test('readKey: the default sources are personas/.env, then the legacy bp/.env', () => {
  assert.deepEqual(ENV_FILES, ['C:/Users/kazda/kiro/personas/.env', 'C:/Users/kazda/kiro/bp/.env']);
});

// Night roots: the VR main checkout and every VR sibling worktree count, each folder and each generation once.
test('NIGHT_ROOTS: both VR roots are counted beside the existing ones', () => {
  assert.ok(NIGHT_ROOTS.checkouts.includes('C:/Users/kazda/kiro/mage-arena-vr'));
  assert.ok(NIGHT_ROOTS.worktrees.includes('C:/Users/kazda/.personas/headless-masters/worktrees/mage-arena-vr'));
  assert.ok(NIGHT_ROOTS.checkouts.includes('C:/Users/kazda/kiro/mage-arena'));
  assert.ok(NIGHT_ROOTS.worktrees.includes('C:/Users/kazda/.personas/headless-masters/worktrees/mage-arena'));
});

function nightTree() {
  const dir = tmpTree();
  const roots = { checkouts: [path.join(dir, 'main')], worktrees: [path.join(dir, 'trees'), path.join(dir, 'missing')] };
  for (const t of ['a', 'b']) fs.mkdirSync(path.join(dir, 'trees', t), { recursive: true });
  fs.writeFileSync(path.join(dir, 'trees', 'not-a-tree.txt'), '');
  return { dir, roots };
}
const art = (...p) => path.resolve(...p, 'apps/vr/art');

test('nightArts: lists the root, every main checkout and every sibling worktree, each once', () => {
  const { dir, roots } = nightTree();
  // the root is itself sibling worktree a, so it appears once
  const arts = nightArts(path.join(dir, 'trees', 'a'), roots);
  assert.deepEqual(arts, [art(dir, 'trees', 'a'), art(dir, 'main'), art(dir, 'trees', 'b')]);
  fs.rmSync(dir, { recursive: true });
});

test('nightTotal: the same generation id in the main checkout and a worktree counts once', () => {
  const { dir, roots } = nightTree();
  const day = '2026-10-24';
  const log = { calls: [
    { date: `${day}T16:00:00Z`, generationId: 'g1', credits: 14 },
    { date: `${day}T16:05:00Z`, generationId: 'g2', credits: 14 },
    { date: '2026-10-23T16:00:00Z', generationId: 'g0', credits: 14 },
  ] };
  for (const c of [path.join(dir, 'main'), path.join(dir, 'trees', 'b')]) writeLog(path.join(art(c), 'x', 'leonardo-log.json'), log);
  writeLog(path.join(art(dir, 'trees', 'a'), 'y', 'leonardo-log.json'), { calls: [{ date: `${day}T17:00:00Z`, generationId: 'g3', credits: 6 }] });
  assert.equal(nightTotal(nightArts(path.join(dir, 'trees', 'a'), roots), day), 34);
  fs.rmSync(dir, { recursive: true });
});
