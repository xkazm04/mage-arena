#!/usr/bin/env node
// T38 concept art: one Leonardo generation per call, logged. Dependency-free (Node's fetch and fs only).
// Adapted from kiro/bp scripts/lookdev/leonardo.mjs (read, not imported).
//
//   node apps/vr/tools/assets/leonardo.mjs gen --view front --seed 101 --quality MEDIUM [--out name.png]
//   node apps/vr/tools/assets/leonardo.mjs total      the run's credit total from the log
//   node apps/vr/tools/assets/leonardo.mjs prompt --view front    print the composed prompt, no call
//
// The key is LEONARDO_API_KEY, read at run time from bp's untracked .env. It is never printed, logged or written
// anywhere; neither is the authorization header. Every call is "public": false. Every call, failed ones included,
// is appended to apps/vr/art/characters/brennic/leonardo-log.json. The logged total may never pass RUN_CAP.
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const ROOT = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../../../..');
const DIR = path.join(ROOT, 'apps/vr/art/characters/brennic');
const LOG = path.join(DIR, 'leonardo-log.json');
const ENV_FILE = 'C:/Users/kazda/kiro/bp/.env';
const API = 'https://cloud.leonardo.ai/api/rest';
const MODEL = 'openai/gpt-image-2.5-flare';
const RUN_CAP = 120; // this run's share of the project's 400 per night
const COST = { LOW: 6, MEDIUM: 14, HIGH: 60 }; // LOW and MEDIUM measured by bp on 2026-10-20 (1376x768); HIGH a high guess
const POLL_MS = 5000;
const POLL_LIMIT_MS = 240000;

// Brennic as the A04 Fire mage sheets draw him (model-sheets 01 and 02), with greybox flat colour per material.
const SUBJECT =
  'Brennic, a broad, stocky, middle-aged fire mage of the Roman era: bare-headed with short dark curly hair, no hood, ' +
  'a full dark beard, a strong brow. He wears a plain knee-length wool tunic with short sleeves, belted at the waist with ' +
  'a wide leather belt; a heavy wool cloak draped over both shoulders like a shawl, fastened at the chest and hanging ' +
  'behind him to the back of the knees; leather wraps on both forearms; a plain iron prisoner\'s collar; wrapped shins ' +
  'and simple leather sandals.';
const LOOK =
  'Game-ready 3D character model reference for a modeller: simple large readable shapes, chunky proportions, big hands, ' +
  'matte surfaces, one flat colour per material with no pattern and no texture detail. Palette blocks: tunic muted brick ' +
  'red, cloak dark charcoal brown, belt, wraps and sandals mid brown leather, collar dark iron grey, skin warm tan, hair ' +
  'and beard dark brown. Soft even studio light, no cast shadows, no text, no labels, no grid, no watermark.';
const POSE =
  'He stands in a neutral A-pose: arms straight, held out and angled about 35 degrees down away from the torso so they ' +
  'do not touch the body or the cloak, open empty hands with fingers together, legs slightly apart, feet flat. No ' +
  'weapon, no staff, no fire, no effects.';
const VIEWS = {
  front: {
    width: 768,
    height: 1376,
    text: `Full-body orthographic front view of ${SUBJECT} ${POSE} He faces the camera directly, symmetrical, the whole ` +
      `figure from head to sandals visible with empty margin around it, centred on a plain flat light grey background, ` +
      `no scenery, no ground, a single character. ${LOOK}`,
  },
  turnaround: {
    width: 1376,
    height: 768,
    text: `Character turnaround sheet of ${SUBJECT} Three full-body orthographic views of the same character side by side ` +
      `at the same scale and height: front view, right side view, back view. ${POSE} Plain flat light grey background, ` +
      `no scenery, no ground. ${LOOK}`,
  },
};

const args = process.argv.slice(2);
const cmd = args[0];
const opt = (name, dflt) => {
  const i = args.indexOf(`--${name}`);
  return i > 0 && i + 1 < args.length ? args[i + 1] : dflt;
};

const readLog = () => (fs.existsSync(LOG) ? JSON.parse(fs.readFileSync(LOG, 'utf8')).calls : []);
const runTotal = (log = readLog()) => log.reduce((n, e) => n + (Number(e.credits) || 0), 0);
function writeLog(calls) {
  fs.mkdirSync(DIR, { recursive: true });
  const doc = {
    card: 'T38',
    asset: 'Brennic concept turnaround',
    model: MODEL,
    runCap: RUN_CAP,
    totalCredits: runTotal(calls),
    note: 'One entry per Leonardo call, failed calls included. credits come from the API response (creditsSource api), else the account balance difference, else the expected cost (fallback).',
    calls,
  };
  fs.writeFileSync(LOG, JSON.stringify(doc, null, 2) + '\n');
}

/** `text` with every copy of the key replaced: whatever goes to the log or stdout passes through here. */
const redact = (text, key) => (key ? String(text).replaceAll(key, '[redacted]') : String(text));

function readKey() {
  const m = fs.readFileSync(ENV_FILE, 'utf8').match(/^\s*LEONARDO_API_KEY\s*=\s*(.+?)\s*$/m);
  if (!m) throw new Error(`LEONARDO_API_KEY not found in ${ENV_FILE}`);
  return m[1].replace(/^['"]|['"]$/g, '');
}

async function call(key, method, url, body) {
  const res = await fetch(url, {
    method,
    headers: { accept: 'application/json', authorization: `Bearer ${key}`, ...(body ? { 'content-type': 'application/json' } : {}) },
    body: body ? JSON.stringify(body) : undefined,
  });
  const text = await res.text();
  let json;
  try { json = JSON.parse(text); } catch { json = { raw: text.slice(0, 500) }; }
  if (!res.ok) throw new Error(`${method} ${url} -> HTTP ${res.status}: ${JSON.stringify(json).slice(0, 500)}`);
  return json;
}

/** Sum of every numeric token pool on the account; the difference across a call is what it cost. */
async function balance(key) {
  try {
    const me = await call(key, 'GET', `${API}/v1/me`);
    const u = me.user_details?.[0] ?? {};
    return Object.entries(u).filter(([k, v]) => /tokens$/i.test(k) && typeof v === 'number').reduce((n, [, v]) => n + v, 0);
  } catch {
    return null;
  }
}

/** A cost the API reports itself, if it does: any numeric field named like cost or credit. */
function reportedCost(obj, depth = 0) {
  if (!obj || typeof obj !== 'object' || depth > 4) return null;
  for (const [k, v] of Object.entries(obj)) {
    if (/cost|credit/i.test(k)) {
      if (typeof v === 'number') return v;
      if (v && typeof v === 'object' && typeof v.amount === 'number') return v.amount;
    }
  }
  for (const v of Object.values(obj)) {
    const c = reportedCost(v, depth + 1);
    if (c != null) return c;
  }
  return null;
}

const sleep = (ms) => new Promise((r) => setTimeout(r, ms));

async function gen() {
  const viewId = opt('view', 'front');
  const view = VIEWS[viewId];
  if (!view) throw new Error(`unknown view ${viewId}; known: ${Object.keys(VIEWS).join(', ')}`);
  const seed = Number(opt('seed', '1'));
  const quality = opt('quality', 'MEDIUM').toUpperCase();
  if (!(quality in COST)) throw new Error('quality must be LOW, MEDIUM or HIGH');
  if (!Number.isInteger(seed)) throw new Error('seed must be an integer');
  const outName = opt('out', `concept-${viewId}-s${seed}-${quality.toLowerCase()}`);
  if (path.basename(outName) !== outName) throw new Error('--out is a file name inside the brennic folder');

  const log = readLog();
  if (runTotal(log) + COST[quality] > RUN_CAP) {
    console.error(`REFUSED: run total ${runTotal(log)} + expected ${COST[quality]} at ${quality} would pass ${RUN_CAP}`);
    process.exit(2);
  }

  const key = readKey();
  const before = await balance(key);
  const body = {
    model: MODEL,
    public: false,
    parameters: { width: view.width, height: view.height, prompt_enhance: 'OFF', quality, quantity: 1, seed, prompt: view.text },
  };
  const entry = {
    date: new Date().toISOString(), view: viewId, model: MODEL, public: false, prompt: view.text, seed, quality,
    width: view.width, height: view.height, generationId: null, status: 'ERROR', credits: 0, creditsSource: null, file: null,
  };
  let reported = null;
  try {
    const created = await call(key, 'POST', `${API}/v2/generations`, body);
    entry.generationId = created.generate?.generationId ?? created.generationId ?? null;
    reported = reportedCost(created);
    if (!entry.generationId) throw new Error(`no generationId in response: ${JSON.stringify(created).slice(0, 300)}`);
    const t0 = Date.now();
    let g;
    for (;;) {
      await sleep(POLL_MS);
      g = (await call(key, 'GET', `${API}/v1/generations/${entry.generationId}`)).generations_by_pk;
      entry.status = g?.status ?? 'UNKNOWN';
      if (entry.status === 'COMPLETE' || entry.status === 'FAILED') break;
      if (Date.now() - t0 > POLL_LIMIT_MS) { entry.status = 'TIMEOUT'; break; }
    }
    reported ??= reportedCost(g);
    const url = (g?.generated_images ?? [])[0]?.url;
    if (entry.status === 'COMPLETE' && url) {
      const ext = (path.extname(new URL(url).pathname) || '.png').toLowerCase();
      const buf = Buffer.from(await (await fetch(url)).arrayBuffer());
      const dest = path.join(DIR, outName + ext);
      fs.mkdirSync(DIR, { recursive: true });
      fs.writeFileSync(dest, buf);
      entry.file = path.relative(ROOT, dest).replaceAll('\\', '/');
      entry.bytes = buf.length;
    }
  } catch (e) {
    entry.error = redact(e.message ?? e, key).slice(0, 500);
  }
  const after = await balance(key);
  if (reported != null) { entry.credits = reported; entry.creditsSource = 'api'; }
  else if (before != null && after != null && before - after > 0) { entry.credits = before - after; entry.creditsSource = 'balance'; }
  else if (entry.generationId) { entry.credits = COST[quality]; entry.creditsSource = 'fallback'; }
  else { entry.credits = 0; entry.creditsSource = 'no-generation'; }
  writeLog([...log, entry]);
  console.log(`${entry.status} ${viewId} s${seed} ${quality} credits=${entry.credits} (${entry.creditsSource}) total=${runTotal()} file=${entry.file ?? '-'}${entry.error ? ` error=${entry.error}` : ''}`);
  if (entry.status !== 'COMPLETE') process.exit(1);
}

if (cmd === 'gen') await gen();
else if (cmd === 'total') console.log(JSON.stringify({ calls: readLog().length, totalCredits: runTotal(), cap: RUN_CAP }));
else if (cmd === 'prompt') console.log(VIEWS[opt('view', 'front')]?.text ?? 'unknown view');
else { console.error('usage: gen | total | prompt (see the header)'); process.exit(1); }
