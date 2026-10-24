#!/usr/bin/env node
// Key-art-look identity masters for the cinder hound and the mire maw (DECISIONS 2026-10-24, "Visual approvals", answer 4).
// Same procedure as leonardo-ref.mjs (docs/research/BRENNIC-MV-MASTER-2026-10-24.md): the STYLE BLOCK of VISUAL-PLAN (c)
// with the reference-sheet light override, an image_reference lead, one subject on a plain background, every call logged.
//
//   node apps/vr/tools/refs/creature-masters.mjs budget
//   node apps/vr/tools/refs/creature-masters.mjs prompt --id hound
//   node apps/vr/tools/refs/creature-masters.mjs gen --id hound --seed 7101
//   node apps/vr/tools/refs/creature-masters.mjs gen --id maw --seed 7202 --variant open-gape
//   node apps/vr/tools/refs/creature-masters.mjs verify
//
// Images go OUTSIDE every repo (STAGING), one log per creature in apps/vr/art/characters/<creature>/master/. The lane is
// refused above LANE_CAP images, above the owner's DAY_CAP for the day, and after a quota or 402 error (latched).
import crypto from 'node:crypto';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { API, COST, MODEL, NIGHT_CAP, balance, call, findLogs, nightArts, nightTotal, pollGeneration, readKey, redact, reportedCost, saveImage, settleCredits, sumCredits, writeLog as writeLogTo } from '../assets/leonardo-client.mjs';

const ROOT = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../../../..');
const PLAN = path.join(ROOT, 'docs/concepts/VISUAL-PLAN.md');
const STAGING = 'C:/Users/kazda/mage-art-staging/creature-masters-2026-10-24';
const LANE_CAP = 6;
const DAY_CAP = 30;
const VR_MAIN = 'C:/Users/kazda/kiro/mage-arena-vr';
const VR_TREES = 'C:/Users/kazda/.personas/headless-masters/worktrees/mage-arena-vr';
const sha256 = (buf) => crypto.createHash('sha256').update(buf).digest('hex');
const sha256File = (f) => sha256(fs.readFileSync(f));
const W = 1376;
const H = 768;

const REFS = {
  still08: {
    file: 'apps/vr/art/key-art/still-08-creatures-s5103-medium.jpg',
    sha256: '73741f260556c4d1e1b8928ec3303d6fc68488dd636887cabdc429f011edbdfc',
    approval: 'the creature look of the key art (DECISIONS 2026-10-24, answer 4); gallery still 08',
  },
  S1: {
    file: 'apps/vr/art/key-art/key-art-s5101-medium.jpg',
    sha256: '81a487c9be00501d5c211bfcff2c0c95b9cd09d3504d88eb4b01fccf2bcb1de4',
    approval: 'style sheet S1 (the north-star frame), approved 2026-10-24 by Athena under owner delegation',
  },
};

// The STYLE BLOCK of VISUAL-PLAN (c), as in leonardo-ref.mjs (a drifted copy refuses to run).
const STYLE_TEXT = [
  'Technique: stylised game concept art in the look of a finished low-poly 3D game model, rendered: large readable ' +
    'shapes, chunky heroic proportions, big hands, clean silhouettes that read at six metres.',
  'Subject treatment: Roman-era arena world; people and creatures are solid and grounded, with readable faces (clear ' +
    'eyes, brows and mouth at rest), costume built from a few large material blocks, straps and wraps shown as a few ' +
    'broad bands, never fine pattern.',
  'Palette with roles: ground: warm ochre sand, warm mid-grey stone; structure accents: deep crimson cloth with gold ' +
    'trim, bronze, iron grey; characters: muted earth reds, charcoal browns, mid-brown leather, warm tan skin; absorb ' +
    '(element colour, reserved for magic): water turquoise #19E0E0, fire vermilion #FF3B1F; dodge: plain steel grey. ' +
    'Black core with a red rim appears only on the one unstoppable spell.',
  'Finish: matte surfaces, one flat colour per material with at most soft broad tonal variation, no fine texture ' +
    'detail, no outlines, no halftone, no chalk grain.',
  'Light (reference-sheet override): soft even studio light, no cast shadow.',
  'Exclusions: no text, letters, numbers, runes that look like letters, logos, watermark, frame border or user ' +
    'interface; no extra or disembodied hands, no extra figures; no sun disc or sun-ray burst; no floating smudges or ' +
    'construction lines; no off-palette purple or green; no photorealism; no VR headset or controllers.',
].join(' ');
const BLOCK_SOURCE = [
  'stylised game concept art in the look of a finished low-poly 3D game model, rendered: large readable shapes, chunky heroic proportions, big hands, clean silhouettes that read at six metres.',
  'Roman-era arena world; people and creatures are solid and grounded, with readable faces (clear eyes, brows and mouth at rest), costume built from a few large material blocks, straps and wraps shown as a few broad bands, never fine pattern.',
  '| Ground | warm ochre sand, warm mid-grey stone |',
  '| Structure accents | deep crimson cloth with gold trim, bronze, iron grey |',
  '| Characters | muted earth reds, charcoal browns, mid-brown leather, warm tan skin |',
  '| Absorb (element colour, reserved for magic) | water turquoise #19E0E0, fire vermilion #FF3B1F |',
  '| Dodge | plain steel grey |',
  'Black core with a red rim appears only on the one unstoppable spell.',
  'matte surfaces, one flat colour per material with at most soft broad tonal variation, no fine texture detail, no outlines, no halftone, no chalk grain.',
  'Override for reference sheets: soft even studio light, no cast shadow.',
  'no text, letters, numbers, runes that look like letters, logos, watermark, frame border or user interface; no extra or disembodied hands, no extra figures; no sun disc or sun-ray burst; no floating smudges or construction lines; no off-palette purple or green; no photorealism; no VR headset or controllers.',
];
function blockDrift() {
  const plan = fs.readFileSync(PLAN, 'utf8').split('\n').map((l) => l.replace(/^>\s?/, '').trim()).join(' ').replace(/\s+/g, ' ');
  return BLOCK_SOURCE.filter((p) => !plan.includes(p));
}

const LEAD =
  'The attached images are references, not content to copy. The first shows the creature look to match: the rendering ' +
  'idiom, the colours and the shapes of its creatures. The second is a style reference only: its rendering idiom, ' +
  'palette roles and finish. Never draw the arena, the stands, the sand, the hands, the water, the sparks, the flames, ' +
  'the sky, the crowd, the camera or the other creatures from them; draw only the single creature described below. ';
const FRAME =
  'The whole creature from nose to tail (or to the back) and from the top of the body to the soles of the feet is ' +
  'fully visible with an empty margin on every side, centred and filling about 80 percent of the image width, on a ' +
  'plain flat light grey background with no gradient. A single creature. ';
const AVOID =
  'Avoid: scenery, floor, ground plane, sand, contact shadow or any cast shadow, arena, sky, flames, fire effects, ' +
  'glow haze, sparks, embers in the air, smoke, steam, water, magic effects, a second creature, a second view, ' +
  'props, labels, captions, grid lines, colour swatches.';

const CALLS = {
  hound: {
    creature: 'cinder-hound',
    what: 'cinder hound identity master, key-art look, side three-quarter view',
    action:
      'Full-body view of a cinder hound, a lean, long-bodied hunting hound seen from a side three-quarter angle, its ' +
      'left side toward the camera and its head turned a little toward the camera: dark red, cracked, rock-like hide ' +
      'in a few large faceted plates, with wide glowing orange-vermilion lava cracks across the shoulders, the ribs, ' +
      'the hips and the legs, and glowing lava paws and eyes; a narrow pointed skull with a closed mouth, short ' +
      'pointed ears, a deep narrow chest, long strong legs with compact paws, and a thin tail held low behind. The ' +
      'lava is solid glowing colour inside the cracks, not flames. It stands in a neutral four-legged stance: the ' +
      'back level, all four legs straight and clear of each other and of the body, all four paws flat, the head ' +
      'level, the mouth closed, the tail held straight out behind. ' + FRAME,
  },
  maw: {
    creature: 'mire-maw',
    what: 'mire maw identity master, key-art look, front three-quarter view',
    action:
      'Full-body view of a mire maw, a huge squat toad-like bog creature seen from a front three-quarter angle: a ' +
      'broad, low, heavy mud-brown body with a warty, bumpy hide in a few large lumpy blocks, a very wide head merged ' +
      'into the body with a huge wide mouth that is closed, showing a row of large blunt cream-coloured conical teeth ' +
      'along the lip line, two small yellow eyes on low bulges on top of the head, short thick bowed front legs with ' +
      'wide flat clawed feet planted on both sides, and a lighter tan belly. It squats low and symmetrical in a ' +
      'neutral pose: the legs apart and clear of the body, the feet flat, the head facing nearly the camera. No ' +
      'moss, no reeds, no spikes, no water. ' + FRAME,
  },
};

// Prompt variants: a named replacement of one phrase of an action (the maw's closed mouth is the one so far).
const VARIANTS = {
  'open-gape': {
    id: 'maw',
    from: 'a huge wide mouth that is closed, showing a row of large blunt cream-coloured conical teeth along the lip line',
    to: 'a huge wide mouth gaping wide open, showing the dark throat, a pink-tan tongue and rows of large blunt ' +
      'cream-coloured conical teeth on the upper and lower jaw',
  },
};
function compile(id, variant) {
  const c = CALLS[id];
  if (!c) throw new Error(`unknown id ${id}; known: ${Object.keys(CALLS).join(', ')}`);
  let action = c.action;
  if (variant) {
    const v = VARIANTS[variant];
    if (!v || v.id !== id || !action.includes(v.from)) throw new Error(`unknown variant ${variant} for ${id}; known: ${Object.keys(VARIANTS).join(', ')}`);
    action = action.replace(v.from, v.to);
  }
  return `${LEAD}${STYLE_TEXT} ${action}${AVOID}`;
}
const logPath = (id) => path.join(ROOT, 'apps/vr/art/characters', CALLS[id].creature, 'master/leonardo-log.json');

const args = process.argv.slice(2);
const cmd = args[0];
const opt = (name, dflt) => {
  const i = args.indexOf(`--${name}`);
  return i > 0 && i + 1 < args.length ? args[i + 1] : dflt;
};

const emptyLog = () => ({ calls: [], uploads: [], latched: null });
const readLogAt = (f) => (fs.existsSync(f) ? { ...emptyLog(), ...JSON.parse(fs.readFileSync(f, 'utf8')) } : emptyLog());
// The lane is both creatures' logs; the lane cap and the latch span them.
const laneDocs = () => Object.keys(CALLS).map((id) => ({ id, doc: readLogAt(logPath(id)) }));
function writeLog(id, doc) {
  writeLogTo(logPath(id), {
    card: 'creature masters, key-art look (DECISIONS 2026-10-24 answer 4)',
    asset: `${CALLS[id].creature} identity master`,
    model: MODEL,
    laneCap: { images: LANE_CAP, note: 'both creatures together' },
    dayCap: { images: DAY_CAP, day: '2026-10-24' },
    nightCap: { credits: NIGHT_CAP },
    staging: STAGING,
    totalCredits: sumCredits(doc.calls),
    note: 'One entry per Leonardo generation call, failed calls included; reference uploads (no generation, no credits) ' +
      'are under uploads. A PENDING entry (creditsSource reserved) is a call whose run ended before the call did. ' +
      'credits come from the API response (creditsSource api), else the balance difference, else the expected cost. ' +
      'file is outside every repo; sha256 binds it. latched is set by the first quota or HTTP 402 error.',
    calls: doc.calls,
    uploads: doc.uploads,
    latched: doc.latched,
  });
}

function dayArts() {
  const arts = [...nightArts(ROOT), path.join(VR_MAIN, 'apps/vr/art')];
  try {
    for (const e of fs.readdirSync(VR_TREES, { withFileTypes: true })) {
      if (e.isDirectory()) arts.push(path.join(VR_TREES, e.name, 'apps/vr/art'));
    }
  } catch { /* no sibling worktrees */ }
  return arts;
}
function dayCount(day = new Date().toISOString().slice(0, 10)) {
  const seen = new Map();
  for (const f of new Set(dayArts().flatMap((d) => findLogs(d)).map((f) => path.resolve(f).toLowerCase()))) {
    let calls = [];
    try { calls = JSON.parse(fs.readFileSync(f, 'utf8')).calls ?? []; } catch { continue; }
    for (const c of calls) {
      if (!String(c.date).startsWith(day)) continue;
      if (!c.generationId && c.status !== 'PENDING') continue;
      seen.set(c.generationId ?? `${f}|${c.date}|${c.seed}|${c.id ?? c.view}`, Number(c.credits) || 0);
    }
  }
  return { images: seen.size, credits: nightTotal(dayArts(), day) };
}
const isQuotaError = (msg) => /HTTP 402|quota|insufficient|not enough (api )?(credits|tokens)|payment required/i.test(String(msg));

async function uploadRef(key, doc, name) {
  const ref = REFS[name];
  const abs = path.join(ROOT, ref.file);
  const hash = sha256File(abs);
  if (hash !== ref.sha256) throw new Error(`REFUSED: ${ref.file} is not the approved image (sha256 ${hash})`);
  const done = doc.uploads.find((u) => u.sha256 === hash && u.status === 'OK');
  if (done) return done.initImageId;
  const up = { date: new Date().toISOString(), ref: name, file: ref.file, sha256: hash, approval: ref.approval, initImageId: null, status: 'ERROR' };
  try {
    const res = await call(key, 'POST', `${API}/v1/init-image`, { extension: path.extname(abs).slice(1) });
    const slot = res.uploadInitImage;
    if (!slot?.id || !slot.url) throw new Error(`no upload slot in the init-image reply: ${JSON.stringify(res).slice(0, 300)}`);
    const fields = typeof slot.fields === 'string' ? JSON.parse(slot.fields) : slot.fields ?? {};
    const form = new FormData();
    for (const [k, v] of Object.entries(fields)) form.append(k, String(v));
    form.append('file', new Blob([fs.readFileSync(abs)], { type: 'image/jpeg' }), path.basename(abs));
    const put = await fetch(slot.url, { method: 'POST', body: form });
    if (![200, 201, 204].includes(put.status)) throw new Error(`upload slot answered HTTP ${put.status}`);
    up.initImageId = slot.id;
    up.status = 'OK';
  } catch (e) {
    up.error = redact(e.message ?? e, key).slice(0, 500);
  }
  doc.uploads.push(up);
  if (up.status !== 'OK') throw new Error(`reference upload ${name} failed: ${up.error}`);
  return up.initImageId;
}

function routingEvidence(g) {
  const out = {};
  const walk = (o, p, depth) => {
    if (!o || typeof o !== 'object' || depth > 3) return;
    for (const [k, v] of Object.entries(o)) {
      if (/guidance|reference|init_?image|controlnet|element/i.test(k)) out[p + k] = JSON.stringify(v ?? null).slice(0, 300);
      else if (v && typeof v === 'object' && k !== 'generated_images') walk(v, `${p}${k}.`, depth + 1);
    }
  };
  walk(g, '', 0);
  return out;
}

function refuseIfOver(quality) {
  const docs = laneDocs();
  const latched = docs.find((d) => d.doc.latched);
  if (latched) return `latched by a quota or 402 error at ${latched.doc.latched.date}: ${latched.doc.latched.error}`;
  const staging = path.resolve(STAGING).toLowerCase();
  if ([ROOT, VR_MAIN, 'C:/Users/kazda/kiro', 'C:/Users/kazda/ai-tools'].some((r) => staging.startsWith(path.resolve(r).toLowerCase()))) {
    return `the staging folder ${STAGING} is inside a repo or a reserved folder`;
  }
  const lane = docs.reduce((n, d) => n + d.doc.calls.length, 0);
  if (lane + 1 > LANE_CAP) return `the lane's ${lane} images + 1 would pass ${LANE_CAP}`;
  const day = dayCount();
  if (day.images + 1 > DAY_CAP) return `today's ${day.images} images + 1 would pass the owner's ${DAY_CAP}`;
  if (day.credits + COST[quality] > NIGHT_CAP) return `today's ${day.credits} credits + ${COST[quality]} would pass ${NIGHT_CAP}`;
  const drift = blockDrift();
  if (drift.length) return `the style block drifted from VISUAL-PLAN (c): ${drift.map((d) => d.slice(0, 60)).join(' | ')}`;
  return null;
}

async function gen() {
  const id = opt('id');
  const c = CALLS[id];
  if (!c) throw new Error(`unknown id ${id}; known: ${Object.keys(CALLS).join(', ')}`);
  const seed = Number(opt('seed'));
  if (!Number.isInteger(seed)) throw new Error('--seed is required and must be an integer');
  const quality = opt('quality', 'MEDIUM').toUpperCase();
  if (!(quality in COST)) throw new Error('quality must be LOW, MEDIUM or HIGH');
  const why = refuseIfOver(quality);
  if (why) {
    console.error(`REFUSED: ${why}`);
    process.exit(2);
  }
  const doc = readLogAt(logPath(id));
  const variant = opt('variant');
  const name = `${id}${variant ? `-${variant}` : ''}-s${seed}-${quality.toLowerCase()}`;
  if (doc.calls.some((x) => x.out === name && x.status === 'COMPLETE')) {
    console.error(`REFUSED: ${name} already exists in the log; pick another seed`);
    process.exit(2);
  }
  let key;
  try {
    key = readKey();
  } catch (e) {
    console.error(`BLOCKED: no Leonardo key (${e.code ?? 'error'}: ${String(e.message).split('\n')[0].slice(0, 200)})`);
    process.exit(3);
  }
  const refs = ['still08', 'S1'];
  const prompt = compile(id, variant);
  const entry = {
    date: new Date().toISOString(), id, ...(variant ? { variant } : {}), what: c.what, model: MODEL, public: false, seed, quality, width: W, height: H,
    lightSlot: 'reference-sheet override', refs: refs.map((r) => ({ ref: r, file: REFS[r].file, sha256: REFS[r].sha256 })),
    prompt, promptSha256: sha256(Buffer.from(prompt, 'utf8')), out: name,
    generationId: null, status: 'ERROR', credits: 0, creditsSource: null, file: null, sha256: null,
  };
  let reported = null;
  let before = null;
  let reserved = false;
  try {
    const ids = [];
    for (const r of refs) ids.push(await uploadRef(key, doc, r));
    entry.refs.forEach((r, i) => { r.initImageId = ids[i]; });
    const body = {
      model: MODEL,
      public: false,
      parameters: {
        width: W, height: H, prompt_enhance: 'OFF', quality, quantity: 1, seed, prompt,
        guidances: { image_reference: ids.map((x) => ({ image: { id: x, type: 'UPLOADED' } })) },
      },
    };
    writeLog(id, { ...doc, calls: [...doc.calls, { ...entry, status: 'PENDING', credits: COST[quality], creditsSource: 'reserved' }] });
    reserved = true;
    before = await balance(key);
    const created = await call(key, 'POST', `${API}/v2/generations`, body);
    entry.generationId = created.generate?.generationId ?? created.generationId ?? null;
    reported = reportedCost(created);
    if (!entry.generationId) throw new Error(`no generationId in response: ${JSON.stringify(created).slice(0, 300)}`);
    const { status, generation: g } = await pollGeneration(
      async (gid) => (await call(key, 'GET', `${API}/v1/generations/${gid}`)).generations_by_pk, entry.generationId);
    entry.status = status;
    entry.routing = routingEvidence(g);
    reported ??= reportedCost(g);
    const url = (g?.generated_images ?? [])[0]?.url;
    if (entry.status === 'COMPLETE' && url) {
      const { dest, bytes } = await saveImage(url, STAGING, name);
      entry.file = dest.replaceAll('\\', '/');
      entry.bytes = bytes;
      entry.sha256 = sha256File(dest);
      fs.writeFileSync(`${dest}.sha256`, `${entry.sha256} *${path.basename(dest)}\n`);
    }
  } catch (e) {
    entry.error = redact(e.message ?? e, key).slice(0, 500);
  }
  if (reserved) {
    const after = await balance(key);
    Object.assign(entry, settleCredits({ reported, before, after, generationId: entry.generationId, expected: COST[quality] }));
  } else {
    entry.creditsSource = 'no-generation';
  }
  if (entry.error && isQuotaError(entry.error)) doc.latched = { date: entry.date, id, error: entry.error };
  doc.calls.push(entry);
  writeLog(id, doc);
  console.log(`${entry.status} ${id} s${seed} ${quality} credits=${entry.credits} (${entry.creditsSource}) ` +
    `prompt=${entry.promptSha256.slice(0, 12)} file=${entry.file ?? '-'} sha256=${entry.sha256 ?? '-'}${entry.error ? ` error=${entry.error}` : ''}`);
  if (entry.status !== 'COMPLETE' || !entry.file) process.exit(1);
}

function verify() {
  let bad = 0;
  for (const { doc } of laneDocs()) {
    for (const c of doc.calls.filter((x) => x.file)) {
      const ok = fs.existsSync(c.file) && sha256File(c.file) === c.sha256;
      if (!ok) bad++;
      console.log(`${ok ? 'OK  ' : 'BAD '} ${c.id} s${c.seed} ${c.sha256} ${c.file}`);
    }
  }
  if (bad) process.exit(1);
}

if (cmd === 'gen') await gen();
else if (cmd === 'budget') {
  const day = dayCount();
  console.log(JSON.stringify({
    day: new Date().toISOString().slice(0, 10), dayImages: day.images, dayCap: DAY_CAP, dayCredits: day.credits,
    nightCap: NIGHT_CAP, laneImages: laneDocs().reduce((n, d) => n + d.doc.calls.length, 0), laneCap: LANE_CAP,
    latched: laneDocs().map((d) => d.doc.latched).find(Boolean) ?? null, blockDrift: blockDrift(),
  }));
} else if (cmd === 'prompt') {
  const p = compile(opt('id', 'hound'), opt('variant'));
  console.log(p);
  console.log(`\nsha256 ${sha256(Buffer.from(p, 'utf8'))} (${p.length} chars)`);
} else if (cmd === 'verify') verify();
else {
  console.error('usage: budget | prompt --id <hound|maw> | gen --id <hound|maw> --seed <n> [--variant open-gape] | verify (see the header)');
  process.exit(1);
}
