// Proof that neither Leonardo client leaks its key when the API echoes it back in an error (security scan 2026-10-22, L1).
// Dependency-free and offline: each client is copied into a temp tree and run with a preload that serves a fake key in
// place of bp's .env, hides every real leonardo-log.json, and replaces fetch with a stub that echoes the Authorization
// header into its response across a truncation boundary. No network call is made and the real .env is never read.
// The nogen case (a 200 without a generationId, its body cut at 300 characters) leaked a key fragment before the fix;
// the json and raw cases (HTTP errors) guard the paths that were already safe.
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
const KEY = 'fakekey0-0000-4000-8000-redactproof1';

const PRELOAD = `
import fs from 'node:fs';
import path from 'node:path';
const TMP = path.resolve(process.env.REDACT_TMP);
const KEY = process.env.REDACT_KEY;
const CASE = process.env.REDACT_CASE;
const read = fs.readFileSync;
fs.readFileSync = function (p, ...rest) {
  const f = path.resolve(String(p));
  if (f.endsWith('.env')) return 'LEONARDO_API_KEY=' + KEY + '\\n';
  if (f.endsWith('leonardo-log.json') && !f.startsWith(TMP)) throw new Error('outside the test tree');
  return read.call(this, p, ...rest);
};
globalThis.fetch = async (url, init = {}) => {
  if (!String(url).startsWith('https://cloud.leonardo.ai/')) throw new Error('unexpected url');
  const auth = init.headers?.authorization ?? '';
  let status = 401;
  let body;
  if (CASE === 'json') body = JSON.stringify({ error: 'x'.repeat(463) + auth });
  else if (CASE === 'raw') body = '<html>' + 'x'.repeat(457) + auth + '</html>';
  else { status = 200; body = JSON.stringify({ note: 'x'.repeat(264) + auth }); }
  if (!String(url).endsWith('/v2/generations')) status = 401;
  return new Response(body, { status, headers: { 'content-type': 'application/json' } });
};
`;

/** Every 8-character piece of the key: a fragment this long left in output means redaction failed. */
const fragments = (k) => Array.from({ length: k.length - 7 }, (_, i) => k.slice(i, i + 8));

function runClient(rel, args, kase) {
  const tmp = fs.mkdtempSync(path.join(os.tmpdir(), 'leo-redact-'));
  try {
    const script = path.join(tmp, rel);
    fs.mkdirSync(path.dirname(script), { recursive: true });
    fs.copyFileSync(path.join(REPO, rel), script);
    fs.mkdirSync(path.join(tmp, 'apps/vr/art'), { recursive: true });
    const preload = path.join(tmp, 'preload.mjs');
    fs.writeFileSync(preload, PRELOAD);
    const r = spawnSync(process.execPath, ['--import', pathToFileURL(preload).href, script, ...args], {
      encoding: 'utf8',
      env: { ...process.env, REDACT_TMP: tmp, REDACT_KEY: KEY, REDACT_CASE: kase },
    });
    const logs = [];
    (function walk(d) {
      for (const e of fs.readdirSync(d, { withFileTypes: true })) {
        const p = path.join(d, e.name);
        if (e.isDirectory()) walk(p);
        else if (e.name === 'leonardo-log.json') logs.push(fs.readFileSync(p, 'utf8'));
      }
    })(path.join(tmp, 'apps/vr/art'));
    return { out: r.stdout + r.stderr, logs, status: r.status };
  } finally {
    fs.rmSync(tmp, { recursive: true, force: true });
  }
}

const CLIENTS = [
  ['apps/vr/tools/assets/leonardo.mjs', ['gen', '--asset', 'brennic', '--view', 'front', '--seed', '7', '--quality', 'LOW']],
  ['apps/vr/tools/keyart/leonardo-keyart.mjs', ['gen', '--id', 'key-art', '--seed', '7', '--quality', 'LOW']],
];

for (const [rel, args] of CLIENTS) {
  for (const kase of ['json', 'raw', 'nogen']) {
    test(`${path.basename(rel)}: a key echoed in a ${kase} response never reaches output or the log`, () => {
      const { out, logs, status } = runClient(rel, args, kase);
      assert.equal(status, 1, `the failed call exits 1:\n${out.replaceAll(KEY, '<KEY>')}`);
      assert.equal(logs.length, 1, 'the failed call is logged');
      assert.match(logs[0], /"error"/);
      for (const text of [out, ...logs]) {
        for (const f of fragments(KEY)) assert.ok(!text.includes(f), `key fragment ${f} leaked`);
      }
    });
  }
}
