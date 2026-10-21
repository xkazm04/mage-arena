#!/usr/bin/env node
// T38 concept art: one Leonardo generation per call, logged. Dependency-free (Node's fetch and fs only).
// Adapted from kiro/bp scripts/lookdev/leonardo.mjs (read, not imported).
//
//   node apps/vr/tools/assets/leonardo.mjs gen [--asset conscript] --view front --seed 101 --quality MEDIUM [--out name.png]
//   node apps/vr/tools/assets/leonardo.mjs total [--asset conscript]    the credit total of the asset's cap group from the logs
//   node apps/vr/tools/assets/leonardo.mjs prompt [--asset conscript] --view front    print the composed prompt, no call
//
// --asset picks the subject, palette, output folder and log: brennic (the default, whose prompt text stays byte-identical),
// conscript or slinger (the two humanoid Bout 2 soldiers). Assets of one cap group share one credit cap.
//
// The key is LEONARDO_API_KEY, read at run time from bp's untracked .env. It is never printed, logged or written
// anywhere; neither is the authorization header. Every call is "public": false. Every call, failed ones included,
// is appended to apps/vr/art/characters/<asset>/leonardo-log.json. The logged total of a cap group may never pass its cap.
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const ROOT = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../../../..');
const CHARS = path.join(ROOT, 'apps/vr/art/characters');
const ENV_FILE = 'C:/Users/kazda/kiro/bp/.env';
const API = 'https://cloud.leonardo.ai/api/rest';
const MODEL = 'openai/gpt-image-2.5-flare';
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
// The Bout 2 soldiers as model sheets 03 and 04 draw them; the look text keeps Brennic's wording except the palette.
const LOOK_HEAD =
  'Game-ready 3D character model reference for a modeller: simple large readable shapes, chunky proportions, big hands, ' +
  'matte surfaces, one flat colour per material with no pattern and no texture detail. Palette blocks: ';
const LOOK_TAIL = ' Soft even studio light, no cast shadows, no text, no labels, no grid, no watermark.';
const CONSCRIPT_SUBJECT =
  'a Roman conscript foot soldier, a stocky young man with a plain unshaven face: a helmet with a brim, a ridge and ' +
  'hinged cheek guards; a short-sleeved mail shirt reaching mid-thigh with a plain trimmed neck and sleeve hems; a ' +
  'leather belt with a short sword in a plain scabbard hanging at his right hip; loose knee-length linen trousers; ' +
  'wrapped shins and simple leather sandals. A large curved rectangular shield with a round central boss hangs by a ' +
  'strap at his left side, its lower edge at knee height, held clear of his body and of his left arm.';
const CONSCRIPT_POSE =
  'He stands in a neutral A-pose: arms straight, held out and angled about 35 degrees down away from the torso so they ' +
  'do not touch the body, the shield or the sword, open empty hands with fingers together, legs slightly apart, feet ' +
  'flat. The sword stays sheathed. No fire, no effects.';
const SLINGER_SUBJECT =
  'a Roman auxiliary slinger, a lean young man with a plain clean-shaven face: a close-fitting cap helmet with short ' +
  'cheek flaps; a short-sleeved tunic ending above the knee with a studded leather jerkin over the chest; a leather ' +
  'sling coiled and tucked into the wide belt at his right hip; a large leather shoulder bag of stones on a strap ' +
  'across the chest, hanging at his left hip, with a small net pouch of stones at his right hip; laced leather ' +
  'wraps from the ankles to below the knees and simple leather sandals.';
const SLINGER_POSE =
  'He stands in a neutral A-pose: arms straight, held out and angled about 35 degrees down away from the torso so they ' +
  'do not touch the body or the bags, open empty hands with fingers together, legs slightly apart, feet flat. The ' +
  'sling stays coiled at the belt. No fire, no effects.';
// cap group: the assets whose logs add up against one credit cap (Brennic keeps the 120 it had)
const ASSETS = {
  brennic: { card: 'T38', title: 'Brennic concept turnaround', subject: SUBJECT, pose: POSE, look: LOOK, group: ['brennic'], cap: 120 },
  conscript: {
    card: 'T15', title: 'Conscript concept', subject: CONSCRIPT_SUBJECT, pose: CONSCRIPT_POSE, group: ['conscript', 'slinger'], cap: 100,
    look: `${LOOK_HEAD}mail shirt dark steel grey, helmet lighter iron grey, shield face mid wood brown with the boss muted bronze, ` +
      `trousers and shin wraps warm linen beige, belt, sword scabbard and sandals mid brown leather, skin warm tan, hair dark ` +
      `brown.${LOOK_TAIL}`,
  },
  slinger: {
    card: 'T15', title: 'Slinger concept', subject: SLINGER_SUBJECT, pose: SLINGER_POSE, group: ['conscript', 'slinger'], cap: 100,
    look: `${LOOK_HEAD}tunic undyed linen beige, jerkin dark brown leather, cap helmet iron grey, belt, sling, shin wraps and ` +
      `sandals mid brown leather, stone bags warm tan leather with muted red-brown straps, skin warm tan, hair mid ` +
      `brown.${LOOK_TAIL}`,
  },
};
const viewsFor = (A) => ({
  front: {
    width: 768,
    height: 1376,
    text: `Full-body orthographic front view of ${A.subject} ${A.pose} He faces the camera directly, symmetrical, the whole ` +
      `figure from head to sandals visible with empty margin around it, centred on a plain flat light grey background, ` +
      `no scenery, no ground, a single character. ${A.look}`,
  },
  turnaround: {
    width: 1376,
    height: 768,
    text: `Character turnaround sheet of ${A.subject} Three full-body orthographic views of the same character side by side ` +
      `at the same scale and height: front view, right side view, back view. ${A.pose} Plain flat light grey background, ` +
      `no scenery, no ground. ${A.look}`,
  },
});

const args = process.argv.slice(2);
const cmd = args[0];
const opt = (name, dflt) => {
  const i = args.indexOf(`--${name}`);
  return i > 0 && i + 1 < args.length ? args[i + 1] : dflt;
};

const assetId = opt('asset', 'brennic');
const ASSET = ASSETS[assetId];
if (!ASSET) throw new Error(`unknown asset ${assetId}; known: ${Object.keys(ASSETS).join(', ')}`);
const VIEWS = viewsFor(ASSET);
const RUN_CAP = ASSET.cap;
const dirOf = (id) => path.join(CHARS, id);
const DIR = dirOf(assetId);
const LOG = path.join(DIR, 'leonardo-log.json');
const readLogOf = (id) => {
  const f = path.join(dirOf(id), 'leonardo-log.json');
  return fs.existsSync(f) ? JSON.parse(fs.readFileSync(f, 'utf8')).calls : [];
};
const readLog = () => readLogOf(assetId);
const sumCredits = (log) => log.reduce((n, e) => n + (Number(e.credits) || 0), 0);
/** The credits logged so far by every asset of this asset's cap group; `log` stands in for this asset's own log. */
const runTotal = (log = readLog()) => sumCredits(log) + sumCredits(ASSET.group.filter((id) => id !== assetId).flatMap(readLogOf));
function writeLog(calls) {
  fs.mkdirSync(DIR, { recursive: true });
  const doc = {
    card: ASSET.card,
    asset: ASSET.title,
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
  if (path.basename(outName) !== outName) throw new Error('--out is a file name inside the asset folder');

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
else if (cmd === 'total') console.log(JSON.stringify({ asset: assetId, calls: readLog().length, totalCredits: runTotal(), cap: RUN_CAP }));
else if (cmd === 'prompt') console.log(VIEWS[opt('view', 'front')]?.text ?? 'unknown view');
else { console.error('usage: gen | total | prompt (see the header)'); process.exit(1); }
