#!/usr/bin/env node
// Competition key art and gallery stills: one Leonardo generation per call, logged. Dependency-free (Node's fetch and fs only).
// The call recipe is apps/vr/tools/assets/leonardo.mjs (its call recipe lives in ../assets/leonardo-client.mjs, shared with it).
//
//   node apps/vr/tools/keyart/leonardo-keyart.mjs list                       the image ids, their purpose and size
//   node apps/vr/tools/keyart/leonardo-keyart.mjs prompt --id key-art        print the composed prompt, no call
//   node apps/vr/tools/keyart/leonardo-keyart.mjs gen --id key-art --seed 5101 [--quality MEDIUM]
//   node apps/vr/tools/keyart/leonardo-keyart.mjs total                      this run's credits and tonight's project total
//   node apps/vr/tools/keyart/leonardo-keyart.mjs manifest [--keep a.jpg,b.jpg]   write manifest.json and index.html from the log
//
// The key is LEONARDO_API_KEY, read at run time from bp's untracked .env; it is never printed, logged or written.
// Every call is "public": false. Every call, failed ones included, is appended to apps/vr/art/key-art/leonardo-log.json.
// The row is written as PENDING at the expected cost before the request is sent and replaced when the call ends.
// Before every call the credits of every leonardo-log.json dated today under apps/vr/art/ of this tree, the main checkout
// and the sibling worktrees are summed (each generation once): a call that would
// take this run past RUN_CAP or the project's night past NIGHT_CAP is refused.
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { API, COST, MODEL, NIGHT_CAP, POLL_LIMIT_MS, POLL_MS, balance, call, nightArts, nightTotal as nightTotalOf, readKey, redact, reportedCost, sleep, sumCredits as sum, writeLog as writeLogTo } from '../assets/leonardo-client.mjs';

const ROOT = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../../../..');
const ART = path.join(ROOT, 'apps/vr/art');
const DIR = path.join(ART, 'key-art');
const LOG = path.join(DIR, 'leonardo-log.json');
const RUN_CAP = 140;
const LABEL = 'concept key art, not a gameplay capture';

const LOOK =
  'Stylised game concept art in the look of a simple 3D model: large readable shapes, chunky proportions, ' +
  'matte surfaces, one flat colour per material with no pattern and no fine texture detail, soft warm dusk light. Threat ' +
  'colour language: spell colour means absorb it, grey steel means dodge it, a black core with a red rim means leave. ' +
  'No text, no letters, no numbers, no logos, no watermark, no user interface, no VR headset, no controllers.';
const ARENA =
  "First-person view from a seated player on a raised stone dais at one end of an oval Roman-era sand arena, the player's " +
  'own bare hands with five fingers each in the lower foreground, stone stands and torches behind. ';
const BRENNIC =
  'Brennic, a broad, stocky, middle-aged fire mage: bare-headed with short dark curly hair, a full dark beard, a strong brow, ' +
  'a knee-length muted brick red tunic, a dark charcoal brown cloak over both shoulders, leather forearm wraps and a plain ' +
  'iron collar';
const CONSCRIPT =
  'Roman conscripts in a brimmed iron helmet, dark steel grey mail shirt, linen trousers and a large curved rectangular ' +
  'wood brown shield with a bronze boss';
const SLINGER =
  'lean slingers in cap helmets, undyed linen tunics and dark brown leather jerkins, a leather stone bag at the hip';

// id: file name stem, size, purpose (what the image is for), beat (storyboard frame it draws from, if any), text
const IMAGES = {
  'key-art': {
    width: 1376, height: 768, purpose: 'hero key art for the competition entry (submission items 2 and 7)', beat: null,
    text: `${ARENA}The seated mage's two raised open palms fill the lower third, a translucent turquoise water ward arc ` +
      `glowing between them. Across the sand, ${BRENNIC}, hurls a blazing vermilion fireball straight toward the player; ` +
      `flames curl around his raised hands. The ward meets the fireball in a bright ring of droplets and sparks. Crowd ` +
      `on its feet in the stands, golden dusk sky. Cinematic wide hero composition.`,
  },
  'still-06-soldiers': {
    width: 1376, height: 768, purpose: 'gallery still: Bout 1, the soldiers', beat: '06-bout-soldiers',
    text: `${ARENA}${CONSCRIPT}, and ${SLINGER}, advance across the sand; a steel-grey spear is in flight toward the player. ` +
      `The player's raised hand holds a small turquoise water ward.`,
  },
  'still-08-creatures': {
    width: 1376, height: 768, purpose: 'gallery still: Bout 2, the creatures', beat: '08-bout-creatures',
    text: `${ARENA}Three lean hounds made of glowing embers charge across the sand, one bursting into sparks against the ` +
      `player's raised turquoise water shield; a squat mud beast with a huge mouth lurks behind them.`,
  },
  'still-09-entrance': {
    width: 1376, height: 768, purpose: 'gallery still: the Fire mage enters', beat: '09-fire-mage-entrance',
    text: `${ARENA}Through a gate under the stands, ${BRENNIC}, walks into the arena, flames curling around his hands, the ` +
      `crowd on its feet.`,
  },
  'still-10-sunfall': {
    width: 1376, height: 768, purpose: 'gallery still: the duel, the unstoppable spell', beat: '10-duel-sunfall',
    text: `${ARENA}${BRENNIC}, hurls a huge black sphere with a jagged red rim high into the sky above the player; a red ring ` +
      `marks the player's pad on the sand; the player's hand flicks to blink to a side pad.`,
  },
  'still-11-finisher': {
    width: 1376, height: 768, purpose: 'gallery still: the finisher', beat: '11-finisher',
    text: `${ARENA}Both hands press together in a seal; a vast orb of turquoise water surges forward toward ${BRENNIC}, in a ` +
      `frozen slow-motion moment, the crowd blurred and roaring.`,
  },
  'still-12-victory': {
    width: 1376, height: 768, purpose: 'gallery still: victory', beat: '12-victory',
    text: `${ARENA}${BRENNIC}, kneels defeated on the steaming sand, the bronze cuff on the player's wrist glowing with four ` +
      `runes, the arena quiet and golden in the last light.`,
  },
};
const promptOf = (id) => `${IMAGES[id].text} ${LOOK}`;

const args = process.argv.slice(2);
const cmd = args[0];
const opt = (name, dflt) => {
  const i = args.indexOf(`--${name}`);
  return i > 0 && i + 1 < args.length ? args[i + 1] : dflt;
};

const readLog = () => (fs.existsSync(LOG) ? JSON.parse(fs.readFileSync(LOG, 'utf8')).calls : []);
const today = () => new Date().toISOString().slice(0, 10);

/** Credits logged today by every checkout's apps/vr/art logs (this run's included), each generation once. */
const nightTotal = () => nightTotalOf(nightArts(ROOT), today());

function writeLog(calls) {
  writeLogTo(LOG, {
    asset: 'Competition key art and gallery stills',
    model: MODEL,
    runCap: RUN_CAP,
    nightCap: NIGHT_CAP,
    totalCredits: sum(calls),
    note: 'One entry per Leonardo call, failed calls included. A PENDING entry (creditsSource reserved) is a call whose run ended before the call did; it counts at the expected cost. credits come from the API response (creditsSource api), else the account balance difference, else the expected cost (fallback).',
    calls,
  });
}

async function gen() {
  const id = opt('id', '');
  const img = IMAGES[id];
  if (!img) throw new Error(`unknown --id ${id}; known: ${Object.keys(IMAGES).join(', ')}`);
  const seed = Number(opt('seed', '1'));
  const quality = opt('quality', 'MEDIUM').toUpperCase();
  if (!(quality in COST)) throw new Error('quality must be LOW, MEDIUM or HIGH');
  if (!Number.isInteger(seed)) throw new Error('seed must be an integer');
  const outName = opt('out', `${id}-s${seed}-${quality.toLowerCase()}`);
  if (path.basename(outName) !== outName) throw new Error('--out is a file name inside the key-art folder');

  const log = readLog();
  const run = sum(log);
  const night = nightTotal();
  if (run + COST[quality] > RUN_CAP) {
    console.error(`REFUSED: run total ${run} + expected ${COST[quality]} at ${quality} would pass the run cap ${RUN_CAP}`);
    process.exit(2);
  }
  if (night + COST[quality] > NIGHT_CAP) {
    console.error(`REFUSED: tonight's project total ${night} + expected ${COST[quality]} at ${quality} would pass the night cap ${NIGHT_CAP}`);
    process.exit(2);
  }

  const prompt = promptOf(id);
  const key = readKey();
  const body = {
    model: MODEL,
    public: false,
    parameters: { width: img.width, height: img.height, prompt_enhance: 'OFF', quality, quantity: 1, seed, prompt },
  };
  const entry = {
    date: new Date().toISOString(), id, model: MODEL, public: false, prompt, seed, quality,
    width: img.width, height: img.height, generationId: null, status: 'ERROR', credits: 0, creditsSource: null, file: null,
  };
  // Logged before the first billed request: a run killed mid-call, or one started meanwhile, still counts these credits.
  writeLog([...log, { ...entry, status: 'PENDING', credits: COST[quality], creditsSource: 'reserved' }]);
  const before = await balance(key);
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
  console.log(`${entry.status} ${id} s${seed} ${quality} credits=${entry.credits} (${entry.creditsSource}) run=${sum(readLog())} night=${nightTotal()} file=${entry.file ?? '-'}${entry.error ? ` error=${entry.error}` : ''}`);
  if (entry.status !== 'COMPLETE') process.exit(1);
}

const esc = (s) => String(s).replace(/[&<>"]/g, (c) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;' })[c]);

/** manifest.json and index.html from the log: the last COMPLETE call per id, or only the files named by --keep. */
function manifest() {
  const keep = args.includes('--keep') ? args[args.indexOf('--keep') + 1].split(',') : null;
  const calls = readLog();
  const done = calls.filter((c) => c.status === 'COMPLETE' && c.file && fs.existsSync(path.join(ROOT, c.file)));
  const pick = {};
  for (const c of done) if (!keep || keep.includes(path.basename(c.file))) pick[c.id] = c;
  const images = Object.keys(IMAGES).filter((id) => pick[id]).map((id) => {
    const c = pick[id];
    return {
      path: c.file, prompt: c.prompt, seed: c.seed, quality: c.quality, credits: c.credits,
      creditsForId: sum(calls.filter((x) => x.id === id)),
      purpose: IMAGES[id].purpose, storyboardBeat: IMAGES[id].beat, label: LABEL, bytes: fs.statSync(path.join(ROOT, c.file)).size,
    };
  });
  const doc = {
    title: 'Mage Arena VR key art and gallery stills',
    label: LABEL,
    model: MODEL,
    note: 'Leonardo concept images for the competition entry. None is an in-engine screenshot or a gameplay capture; gameplay captures are still owed from Wolf. Licence status: docs/submission/ASSET-LEDGER.md.',
    log: 'apps/vr/art/key-art/leonardo-log.json',
    images,
  };
  fs.writeFileSync(path.join(DIR, 'manifest.json'), JSON.stringify(doc, null, 2) + '\n');
  const cards = images.map((i) => `    <figure>
      <img src="${esc(path.basename(i.path))}" alt="${esc(i.purpose)}" loading="lazy">
      <figcaption><strong>${esc(i.purpose)}</strong><br>${LABEL}<br><small>seed ${i.seed}, ${esc(i.quality)}, ${esc(path.basename(i.path))}</small></figcaption>
    </figure>`).join('\n');
  fs.writeFileSync(path.join(DIR, 'index.html'), `<!doctype html>
<html lang="en"><head><meta charset="utf-8"><title>Mage Arena VR: key art and gallery stills</title>
<style>
body{margin:0;padding:24px;background:#16120f;color:#e8dfd0;font:15px/1.4 system-ui,sans-serif}
h1{font-size:22px;margin:0 0 4px}p{margin:0 0 20px;color:#b9ad9a}
main{display:grid;grid-template-columns:repeat(auto-fill,minmax(420px,1fr));gap:20px}
figure{margin:0;background:#241d18;padding:10px;border-radius:6px}img{width:100%;height:auto;display:block}
figcaption{padding-top:8px}small{color:#9a8f7e}
</style></head><body>
<h1>Mage Arena VR: key art and gallery stills</h1>
<p>Every image here is ${LABEL}. Made with Leonardo from text prompts; none is an in-engine screenshot.</p>
<main>
${cards}
</main></body></html>
`);
  console.log(`manifest.json and index.html: ${images.length} images`);
}

if (cmd === 'gen') await gen();
else if (cmd === 'total') console.log(JSON.stringify({ runCredits: sum(readLog()), runCap: RUN_CAP, nightCredits: nightTotal(), nightCap: NIGHT_CAP, calls: readLog().length }));
else if (cmd === 'prompt') console.log(IMAGES[opt('id', '')] ? promptOf(opt('id', '')) : 'unknown id');
else if (cmd === 'list') for (const [id, i] of Object.entries(IMAGES)) console.log(`${id}\t${i.width}x${i.height}\t${i.purpose}`);
else if (cmd === 'manifest') manifest();
else { console.error('usage: gen | total | prompt | list | manifest (see the header)'); process.exit(1); }
