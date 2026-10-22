// Offline proof of the call-settling helpers both Leonardo clients share (leonardo.mjs and keyart/leonardo-keyart.mjs).
//
//   node --test apps/vr/tools/assets/leonardo-client.test.mjs
import test from 'node:test';
import assert from 'node:assert/strict';
import { isImage, pollGeneration, settleCredits } from './leonardo-client.mjs';

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
