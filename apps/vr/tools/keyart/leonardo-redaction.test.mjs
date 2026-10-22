// Proof that neither Leonardo client leaks its key when the API echoes it back in an error (security scan 2026-10-22, L1),
// and (scan 2026-10-22-b, at the end) that both enforce the night cap over every checkout, log a call before it is
// billed, and never save a failed or non-image download; and (scan 2026-10-22-c) that a sibling worktree's spend counts and
// a finished call saves its image and settles its credits through the shared pollGeneration and settleCredits.
// Dependency-free and offline: each client and the shared leonardo-client.mjs are copied into a temp tree and run with a preload that serves a fake key in
// place of bp's .env, hides every real leonardo-log.json, and replaces fetch with a stub that echoes the Authorization
// header into its response across a truncation boundary. No network call is made and the real .env is never read.
// The nogen case (a 200 without a generationId, its body cut at 300 characters) leaked a key fragment before the fix;
// the json and raw cases (HTTP errors) guard the paths that were already safe. The two -escaped cases spell the key as
// JSON \u escapes, which JSON.parse decodes only after the raw body was redacted (security scan 2026-10-22-b, S3).
//
//   node --test apps/vr/tools/keyart/leonardo-redaction.test.mjs
import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { spawnSync } from 'node:child_process';
import { fileURLToPath, pathToFileURL } from 'node:url';

const REPO = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../../../..');
// The call recipe both clients import; it is copied beside them so the temp tree runs the real shared code.
const SHARED = 'apps/vr/tools/assets/leonardo-client.mjs';
const KEY = 'fakekey0-0000-4000-8000-redactproof1';

const PRELOAD = `
import fs from 'node:fs';
import path from 'node:path';
const TMP = path.resolve(process.env.REDACT_TMP);
const KEY = process.env.REDACT_KEY;
const CASE = process.env.REDACT_CASE;
// The main checkout's path is served from TMP/main and the sibling worktrees' folder from TMP/worktrees, so a case can
// plant a log in either; nothing in the real checkout or a real worktree is read.
const MAIN = path.resolve('C:/Users/kazda/kiro/mage-arena').toLowerCase();
const TREES = path.resolve('C:/Users/kazda/.personas/headless-masters/worktrees/mage-arena').toLowerCase();
const map = (p) => {
  if (typeof p !== 'string' || p.startsWith('file:')) return p; // the module loader's URLs and fds pass through
  const f = path.resolve(p);
  if (f.toLowerCase().startsWith(MAIN)) return path.join(TMP, 'main', f.slice(MAIN.length));
  if (f.toLowerCase().startsWith(TREES)) return path.join(TMP, 'worktrees', f.slice(TREES.length));
  return f;
};
const read = fs.readFileSync;
fs.readFileSync = function (p, ...rest) {
  const f = path.resolve(String(map(p)));
  if (f.endsWith('.env')) return 'LEONARDO_API_KEY=' + KEY + '\\n';
  if (f.endsWith('leonardo-log.json') && !f.startsWith(TMP)) throw new Error('outside the test tree');
  return read.call(this, map(p), ...rest);
};
const readdir = fs.readdirSync;
fs.readdirSync = function (p, ...rest) { return readdir.call(this, map(p), ...rest); };
// No real waits: the poll loop sleeps POLL_MS between status calls.
const wait = globalThis.setTimeout;
globalThis.setTimeout = (f, ms, ...a) => wait(f, 0, ...a);
const IMG = 'https://cdn.leonardo.ai/users/u/generations/g1/' + (CASE === 'badext' ? 'img.html' : 'img.jpg');
const reply = (status, doc) => new Response(typeof doc === 'string' || doc instanceof Uint8Array ? doc : JSON.stringify(doc), { status });
// A real PNG signature and header start: the 'ok' case's image.
const PNG = new Uint8Array([0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a, 0, 0, 0, 13, 0x49, 0x48, 0x44, 0x52]);
const IMG_BODY = { cdn404: [404, 'Not Found'], badext: [200, '<script>alert(1)</script>'], html200: [200, '<html>AccessDenied</html>'], ok: [200, PNG] };
// The header spelled as JSON \\u escapes: the raw body holds no copy of the key, only what JSON.parse returns does.
const esc = (t) => [...t].map((c) => '\\\\u' + c.charCodeAt(0).toString(16).padStart(4, '0')).join('');
globalThis.fetch = async (url, init = {}) => {
  if (CASE === 'crash' && String(url).endsWith('/v2/generations')) process.exit(9); // killed while the generation is billed
  if (IMG_BODY[CASE]) {
    if (String(url) === IMG) return reply(...IMG_BODY[CASE]);
    if (String(url).endsWith('/v2/generations')) return reply(200, { generate: { generationId: 'g1' } });
    if (String(url).endsWith('/v1/generations/g1')) return reply(200, { generations_by_pk: { status: 'COMPLETE', generated_images: [{ url: IMG }] } });
  }
  if (!String(url).startsWith('https://cloud.leonardo.ai/')) throw new Error('unexpected url');
  const auth = init.headers?.authorization ?? '';
  let status = 401;
  let body;
  if (CASE === 'json') body = JSON.stringify({ error: 'x'.repeat(463) + auth });
  else if (CASE === 'raw') body = '<html>' + 'x'.repeat(457) + auth + '</html>';
  else if (CASE === 'json-escaped') body = '{"error":"' + 'x'.repeat(463) + esc(auth) + '"}';
  else if (CASE === 'nogen-escaped') { status = 200; body = '{"note":"' + 'x'.repeat(264) + esc(auth) + '"}'; }
  else { status = 200; body = JSON.stringify({ note: 'x'.repeat(264) + auth }); }
  if (!String(url).endsWith('/v2/generations')) status = 401;
  return new Response(body, { status, headers: { 'content-type': 'application/json' } });
};
`;

/** Every 8-character piece of the key: a fragment this long left in output means redaction failed. */
const fragments = (k) => Array.from({ length: k.length - 7 }, (_, i) => k.slice(i, i + 8));

function runClient(rel, args, kase, plant = () => {}) {
  const tmp = fs.mkdtempSync(path.join(os.tmpdir(), 'leo-redact-'));
  try {
    const script = path.join(tmp, rel);
    fs.mkdirSync(path.dirname(script), { recursive: true });
    for (const f of [rel, SHARED]) {
      fs.mkdirSync(path.dirname(path.join(tmp, f)), { recursive: true });
      fs.copyFileSync(path.join(REPO, f), path.join(tmp, f));
    }
    fs.mkdirSync(path.join(tmp, 'apps/vr/art'), { recursive: true });
    plant(tmp);
    const preload = path.join(tmp, 'preload.mjs');
    fs.writeFileSync(preload, PRELOAD);
    const r = spawnSync(process.execPath, ['--import', pathToFileURL(preload).href, script, ...args], {
      encoding: 'utf8',
      env: { ...process.env, REDACT_TMP: tmp, REDACT_KEY: KEY, REDACT_CASE: kase },
    });
    const logs = [];
    const files = [];
    (function walk(d) {
      for (const e of fs.readdirSync(d, { withFileTypes: true })) {
        const p = path.join(d, e.name);
        if (e.isDirectory()) walk(p);
        else if (e.name === 'leonardo-log.json') logs.push(fs.readFileSync(p, 'utf8'));
        else files.push(e.name);
      }
    })(path.join(tmp, 'apps/vr/art'));
    return { out: r.stdout + r.stderr, logs, files, status: r.status };
  } finally {
    fs.rmSync(tmp, { recursive: true, force: true });
  }
}

const CLIENTS = [
  ['apps/vr/tools/assets/leonardo.mjs', ['gen', '--asset', 'brennic', '--view', 'front', '--seed', '7', '--quality', 'LOW']],
  ['apps/vr/tools/keyart/leonardo-keyart.mjs', ['gen', '--id', 'key-art', '--seed', '7', '--quality', 'LOW']],
];

for (const [rel, args] of CLIENTS) {
  for (const kase of ['json', 'raw', 'nogen', 'json-escaped', 'nogen-escaped']) {
    test(`${path.basename(rel)}: a key echoed in a ${kase} response never reaches output or the log`, () => {
      const { out, logs, status } = runClient(rel, args, kase);
      assert.equal(status, 1, `the failed call exits 1:\n${out.replaceAll(KEY, '<KEY>')}`);
      assert.equal(logs.length, 1, `the failed call is logged:
${out.replaceAll(KEY, "<KEY>")}`);
      assert.match(logs[0], /"error"/);
      for (const text of [out, ...logs]) {
        for (const f of fragments(KEY)) assert.ok(!text.includes(f), `key fragment ${f} leaked`);
      }
    });
  }
}

// Spend and download guards (security scan 2026-10-22-b): the same offline harness, no key and no network.
for (const [rel, args] of CLIENTS) {
  const name = path.basename(rel);
  test(`${name}: credits another checkout logged tonight count against the night cap (S1)`, () => {
    const { out, status } = runClient(rel, args, 'night', (tmp) => {
      const dir = path.join(tmp, 'main/apps/vr/art/key-art');
      fs.mkdirSync(dir, { recursive: true });
      const calls = [{ date: new Date().toISOString(), generationId: 'main-1', credits: 398 }];
      fs.writeFileSync(path.join(dir, 'leonardo-log.json'), JSON.stringify({ calls }));
    });
    assert.equal(status, 2, `a call past the night cap is refused before any request:\n${out}`);
    assert.match(out, /REFUSED/);
  });
}

// Headless builders run in sibling worktrees: their logs count too (scan 2026-10-22-c, N2).
for (const [rel, args] of CLIENTS) {
  test(`${path.basename(rel)}: credits a sibling worktree logged tonight count against the night cap (S1, N2)`, () => {
    const { out, status } = runClient(rel, args, 'night', (tmp) => {
      const dir = path.join(tmp, 'worktrees/other-builder/apps/vr/art/characters/brennic');
      fs.mkdirSync(dir, { recursive: true });
      const calls = [{ date: new Date().toISOString(), generationId: 'sib-1', credits: 398 }];
      fs.writeFileSync(path.join(dir, 'leonardo-log.json'), JSON.stringify({ calls }));
    });
    assert.equal(status, 2, `a call past the night cap is refused before any request:\n${out}`);
    assert.match(out, /REFUSED/);
  });
}

for (const [rel, args] of CLIENTS) {
  test(`${path.basename(rel)}: a run killed while the generation is billed still leaves its ledger row (S2)`, () => {
    const { out, logs, status } = runClient(rel, args, 'crash');
    assert.equal(status, 9, `the stub kills the run at the POST:\n${out}`);
    assert.equal(logs.length, 1, 'the call was logged before the request');
    const calls = JSON.parse(logs[0]).calls;
    assert.equal(calls.length, 1);
    assert.equal(calls[0].status, 'PENDING');
    assert.equal(calls[0].credits, 6, 'the reservation holds the expected LOW cost, so the caps count it');
  });
}

for (const [rel, args] of CLIENTS) {
  for (const kase of ['cdn404', 'badext', 'html200']) {
    test(`${path.basename(rel)}: a ${kase} image download writes no file and fails the run (P1)`, () => {
      const { out, logs, files, status } = runClient(rel, args, kase);
      assert.deepEqual(files, [], 'nothing but the log is written');
      assert.equal(status, 1, `the run fails:\n${out}`);
      const [call] = JSON.parse(logs[0]).calls;
      assert.equal(call.file, null);
      assert.match(call.error ?? '', /image/);
    });
  }
}

// The finished call through the shared pollGeneration, saveImage and settleCredits (scan 2026-10-22-c, N3): the image is
// saved, and the row is settled (here at the expected cost, as neither the API nor the balance reports one), not left at 0.
for (const [rel, args] of CLIENTS) {
  test(`${path.basename(rel)}: a finished call saves its image and settles its credits (N3)`, () => {
    const { out, logs, files, status } = runClient(rel, args, 'ok');
    assert.equal(status, 0, `the run succeeds:\n${out}`);
    assert.equal(files.length, 1, 'the image is written');
    assert.match(files[0], /-s7-low\.jpg$/);
    const [call] = JSON.parse(logs[0]).calls;
    assert.equal(call.status, 'COMPLETE');
    assert.equal(call.bytes, 16);
    assert.equal(call.credits, 6);
    assert.equal(call.creditsSource, 'fallback');
  });
}
