#!/usr/bin/env node
// Brennic's multi-view master reference (docs/concepts/VISUAL-PLAN.md section (d), steps 0-2): the step-0 consistency
// control arm (B and C) and the front, right side and back views, one Leonardo generation per call, logged.
// Dependency-free (Node's fetch and fs only). The call recipe is ../assets/leonardo-client.mjs, imported unchanged.
//
//   node apps/vr/tools/refs/leonardo-ref.mjs budget                 today's images and credits against every cap, no call
//   node apps/vr/tools/refs/leonardo-ref.mjs list                   the call ids, their references and size
//   node apps/vr/tools/refs/leonardo-ref.mjs prompt --id front      print the compiled prompt and its sha256, no call
//   node apps/vr/tools/refs/leonardo-ref.mjs gen --id front --seed 6101 [--quality MEDIUM]
//   node apps/vr/tools/refs/leonardo-ref.mjs verify                 every logged file's sha256 against the staged file
//
// Every prompt compiles the approved STYLE BLOCK of VISUAL-PLAN (c) in full, with the light slot set to its named
// reference-sheet override, then the action block. The block is checked against VISUAL-PLAN before every call, so a
// drifted copy refuses to run. A reference image is sent only if its sha256 matches the approved file listed in REFS;
// the prompt says first that the references are references, not content to copy.
//
// The images are written OUTSIDE every repo, to STAGING, with a .sha256 beside each; no image is committed. Every
// call, failed ones included, is logged to apps/vr/art/characters/brennic/mv-master/leonardo-log.json, as a PENDING
// row at the expected cost before the request is sent and replaced when the call ends; reference uploads (no
// generation, no credits) are logged in the same file under `uploads`. Before every call these are refused: the
// lane's LANE_CAP images, the owner's DAY_CAP images for the day, the client's NIGHT_CAP credits, and any call after
// a quota or HTTP 402 error (the log latches it). The day is counted over every leonardo-log.json of this tree, the
// client's night roots and the VR checkout and its sibling worktrees (read only), each generation once.
//
// The key is LEONARDO_API_KEY, read at run time by the client (the environment, else personas' untracked .env, else
// bp's); it is never printed, logged or written. Every call is "public": false. The presigned upload POST carries
// no credentials.
import crypto from 'node:crypto';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { API, COST, MODEL, NIGHT_CAP, balance, call, findLogs, nightArts, nightTotal, pollGeneration, readKey, redact, reportedCost, saveImage, settleCredits, sumCredits, writeLog as writeLogTo } from '../assets/leonardo-client.mjs';

const ROOT = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../../../..');
const DIR = path.join(ROOT, 'apps/vr/art/characters/brennic/mv-master');
const LOG = path.join(DIR, 'leonardo-log.json');
const PLAN = path.join(ROOT, 'docs/concepts/VISUAL-PLAN.md');
const STAGING = 'C:/Users/kazda/mage-art-staging/brennic-mv-2026-10-24';
const LANE_CAP = 12; // this lane's images (brief of run d444ae02, 2026-10-24)
const DAY_CAP = 30; // the owner's Leonardo images for 2026-10-24 (say-mage-visual-approvals.md, answer 3)
// This repo's main checkout and its sibling worktrees (read only, never written). The client's night roots hold them
// too since 2026-10-24; dayArts still names them, and dayCount reads each log file and each generation once.
const VR_MAIN = 'C:/Users/kazda/kiro/mage-arena-vr';
const VR_TREES = 'C:/Users/kazda/.personas/headless-masters/worktrees/mage-arena-vr';
const sha256 = (buf) => crypto.createHash('sha256').update(buf).digest('hex');
const sha256File = (f) => sha256(fs.readFileSync(f));

// The approved references: approved 2026-10-24 by Athena under owner delegation (style sheet S1-S5 and the Brennic
// identity master, C:/Users/kazda/.personas/standup/2026-10-24/say-mage-visual-approvals.md, answers 1+2). The Brennic
// master is also style sheet S5. A file whose bytes differ from the hash here is never sent.
const REFS = {
  master: {
    file: 'apps/vr/art/characters/brennic/concept-front-s3801-medium.jpg',
    sha256: '34b43291deda6db6739413fc38febe4b5b0ee0a1701d1c3392cff78c8d3263d7',
    approval: 'Brennic identity master and style sheet S5, approved 2026-10-24 by Athena under owner delegation',
  },
  S1: {
    file: 'apps/vr/art/key-art/key-art-s5101-medium.jpg',
    sha256: '81a487c9be00501d5c211bfcff2c0c95b9cd09d3504d88eb4b01fccf2bcb1de4',
    approval: 'style sheet S1 (the north-star frame), approved 2026-10-24 by Athena under owner delegation',
  },
};

// The STYLE BLOCK of VISUAL-PLAN (c), slot by slot. The palette slot is the section's table written as one sentence.
const BLOCK = {
  technique:
    'Technique: stylised game concept art in the look of a finished low-poly 3D game model, rendered: large readable ' +
    'shapes, chunky heroic proportions, big hands, clean silhouettes that read at six metres.',
  subject:
    'Subject treatment: Roman-era arena world; people and creatures are solid and grounded, with readable faces (clear ' +
    'eyes, brows and mouth at rest), costume built from a few large material blocks, straps and wraps shown as a few ' +
    'broad bands, never fine pattern.',
  palette:
    'Palette with roles: ground: warm ochre sand, warm mid-grey stone; structure accents: deep crimson cloth with gold ' +
    'trim, bronze, iron grey; characters: muted earth reds, charcoal browns, mid-brown leather, warm tan skin; absorb ' +
    '(element colour, reserved for magic): water turquoise #19E0E0, fire vermilion #FF3B1F; dodge: plain steel grey. ' +
    'Black core with a red rim appears only on the one unstoppable spell.',
  finish:
    'Finish: matte surfaces, one flat colour per material with at most soft broad tonal variation, no fine texture ' +
    'detail, no outlines, no halftone, no chalk grain.',
  // the named, scoped override of the light slot for reference sheets (VISUAL-PLAN (c), "Light")
  light: 'Light (reference-sheet override): soft even studio light, no cast shadow.',
  exclusions:
    'Exclusions: no text, letters, numbers, runes that look like letters, logos, watermark, frame border or user ' +
    'interface; no extra or disembodied hands, no extra figures; no sun disc or sun-ray burst; no floating smudges or ' +
    'construction lines; no off-palette purple or green; no photorealism; no VR headset or controllers.',
};
// The phrases each slot must find in VISUAL-PLAN (c), so the block cannot drift from the approved text unseen.
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

/** The phrases of BLOCK_SOURCE that VISUAL-PLAN no longer contains (quote markers and line breaks folded away). */
function blockDrift() {
  const plan = fs.readFileSync(PLAN, 'utf8').split('\n').map((l) => l.replace(/^>\s?/, '').trim()).join(' ').replace(/\s+/g, ' ');
  return BLOCK_SOURCE.filter((p) => !plan.includes(p));
}

const STYLE_TEXT = [BLOCK.technique, BLOCK.subject, BLOCK.palette, BLOCK.finish, BLOCK.light, BLOCK.exclusions].join(' ');

// The action block: canonical A-pose, single subject, plain flat light-grey background, one view per image, one
// size and framing for every view, the exclusions as text.
const FRAME =
  'The whole figure from the top of the head to the soles of the sandals is visible, standing upright, centred, ' +
  'filling about 85 percent of the image height with the same empty margin above the head and below the feet, ' +
  'orthographic camera at chest height, on a plain flat light grey background with no gradient. A single figure. ';
const ACTION_EXCLUSIONS =
  'Avoid: scenery, floor, ground plane, contact shadow or any cast shadow, sand, crowd, arena, sky, fire, flames, ' +
  'glow, sparks, smoke, magic effects, weapon, staff, held objects, a second figure, a second view of the figure, ' +
  'labels, captions, grid lines, colour swatches.';
const A_POSE =
  'He stands in a neutral A-pose: arms straight, held out and angled about 35 degrees down away from the torso so ' +
  'they do not touch the body or the cloak, open empty hands with fingers together, legs slightly apart, feet flat, ' +
  'head level and facing the same way as the body.';
// Brennic as the approved master shows him; the master image carries identity, this text only names what it shows.
const BRENNIC =
  'Brennic, the broad, stocky, middle-aged fire mage shown in the identity reference: bare-headed with short dark ' +
  'curly hair and a full dark beard, a strong brow; a heavy plain iron collar with a small square plate at the front; ' +
  'a knee-length muted brick red tunic with short sleeves; a wide mid-brown leather belt with a square buckle; a ' +
  'dark charcoal brown wool cloak over both shoulders, fastened at the chest with one round bronze brooch and ' +
  'hanging behind him to the back of the knees; broad mid-brown leather bands wrapped on both forearms and on both ' +
  'shins below the knee; simple strapped leather sandals.';
// The step-0 subject: not on the reference sheet, a humanoid like the asset that follows it. Colours are left to the
// block (and, in arm B, the references), so the palette roles are what the probe reads.
const HERALD =
  'an arena herald of the Roman era, a tall, lean, older man with a clean-shaven long face and short grey hair: an ' +
  'ankle-length tunic, a wide cloth sash at the waist, a cloak with a broad trimmed edge over his left shoulder, ' +
  'metal bands on both upper arms, a thin metal circlet on his head, simple leather sandals.';

const REF_LEAD = {
  // the step-0 conditioned arm: both images as style only
  style:
    'The attached images are style references, not content to copy: take only their rendering idiom, palette roles, ' +
    'shape simplicity and finish. Do not draw the people, the arena, the crowd, the fire, the water or the pose from ' +
    'them; draw only the subject described below. ',
  // the Brennic views: the master as identity (and S5 style), S1 as style only
  identity:
    'The attached images are references, not content to copy. The first is the identity reference: the same man, ' +
    'his face, build, proportions, hair, beard and costume must match it exactly; it is the front view and it wins ' +
    'any disagreement. The second is a style reference only: take its rendering idiom, palette roles and finish, ' +
    'never its scene, lighting, people, crowd, fire, water or camera. ',
};

const W = 768;
const H = 1376;
// id: what it is, the references it sends (in order), the reference lead, the action text
const CALLS = {
  'arm-b': {
    what: 'step 0, consistency control arm B: a new subject conditioned on the sheet (S5 = Brennic master, S1)',
    refs: ['master', 'S1'],
    lead: 'style',
    action: `Full-body orthographic front view of ${HERALD} He stands in a neutral A-pose: arms straight, held out ` +
      'and angled about 35 degrees down away from the torso so they do not touch the body or the cloak, open empty ' +
      `hands with fingers together, legs slightly apart, feet flat, facing the camera directly. ${FRAME}`,
  },
  'arm-c': {
    what: 'step 0, consistency control arm C: the same subject as arm B, unconditioned (no reference)',
    refs: [],
    lead: null,
    action: null, // arm B's action exactly: the only difference between B and C is the references
  },
  front: {
    what: 'Brennic multi-view master reference: front view',
    refs: ['master', 'S1'],
    lead: 'identity',
    action: `Full-body orthographic front view of ${BRENNIC} ${A_POSE} He faces the camera directly, symmetrical. ${FRAME}`,
  },
  side: {
    what: 'Brennic multi-view master reference: right side view',
    refs: ['master', 'S1'],
    lead: 'identity',
    action: `Full-body orthographic right side view, in exact profile, of ${BRENNIC} ${A_POSE} His right side faces ` +
      'the camera and he looks to the right of the image, the whole head and body turned 90 degrees from the front ' +
      `view, the arms in the same A-pose as the front view. ${FRAME}`,
  },
  back: {
    what: 'Brennic multi-view master reference: back view',
    refs: ['master', 'S1'],
    lead: 'identity',
    action: `Full-body orthographic back view of ${BRENNIC} ${A_POSE} He faces directly away from the camera, ` +
      `symmetrical; the back of the head, the cloak hanging down his back and the backs of the legs are seen. ${FRAME}`,
  },
};
CALLS['arm-c'].action = CALLS['arm-b'].action;

/** The compiled prompt of call `id`: reference lead (when references ride along), the style block, the action block. */
function compile(id) {
  const c = CALLS[id];
  if (!c) throw new Error(`unknown id ${id}; known: ${Object.keys(CALLS).join(', ')}`);
  return `${c.lead ? REF_LEAD[c.lead] : ''}${STYLE_TEXT} ${c.action}${ACTION_EXCLUSIONS}`;
}

const args = process.argv.slice(2);
const cmd = args[0];
const opt = (name, dflt) => {
  const i = args.indexOf(`--${name}`);
  return i > 0 && i + 1 < args.length ? args[i + 1] : dflt;
};

const emptyLog = () => ({ calls: [], uploads: [], latched: null });
const readLog = () => (fs.existsSync(LOG) ? { ...emptyLog(), ...JSON.parse(fs.readFileSync(LOG, 'utf8')) } : emptyLog());
function writeLog(doc) {
  writeLogTo(LOG, {
    card: 'VISUAL-PLAN (d) steps 0-2',
    asset: 'Brennic multi-view master reference',
    model: MODEL,
    laneCap: { images: LANE_CAP },
    dayCap: { images: DAY_CAP, day: '2026-10-24' },
    nightCap: { credits: NIGHT_CAP },
    staging: STAGING,
    totalCredits: sumCredits(doc.calls),
    note: 'One entry per Leonardo generation call, failed calls included; reference uploads (v1 init-image, no ' +
      'generation, no credits) are under uploads. A PENDING entry (creditsSource reserved) is a call whose run ended ' +
      'before the call did; it counts at the expected cost. credits come from the API response (creditsSource api), ' +
      'else the account balance difference, else the expected cost (fallback). file is outside every repo; sha256 ' +
      'binds it. latched is set by the first quota or HTTP 402 error, after which every call is refused.',
    calls: doc.calls,
    uploads: doc.uploads,
    latched: doc.latched,
  });
}

/** Every apps/vr/art the day is counted over: the client's night roots plus the VR checkout and its sibling worktrees. */
function dayArts() {
  const arts = [...nightArts(ROOT), path.join(VR_MAIN, 'apps/vr/art')];
  try {
    for (const e of fs.readdirSync(VR_TREES, { withFileTypes: true })) {
      if (e.isDirectory()) arts.push(path.join(VR_TREES, e.name, 'apps/vr/art'));
    }
  } catch { /* no sibling worktrees */ }
  return arts;
}

/** Today's (UTC) generation rows over every counted log, each generation once: { images, credits }. */
function dayCount(day = new Date().toISOString().slice(0, 10)) {
  const seen = new Map();
  for (const f of new Set(dayArts().flatMap((d) => findLogs(d)).map((f) => path.resolve(f).toLowerCase()))) {
    let calls = [];
    try { calls = JSON.parse(fs.readFileSync(f, 'utf8')).calls ?? []; } catch { continue; }
    for (const c of calls) {
      if (!String(c.date).startsWith(day)) continue;
      // a call refused before any generation existed made no image and spent nothing
      if (!c.generationId && c.status !== 'PENDING') continue;
      seen.set(c.generationId ?? `${f}|${c.date}|${c.seed}|${c.id ?? c.view}`, Number(c.credits) || 0);
    }
  }
  return { images: seen.size, credits: nightTotal(dayArts(), day) };
}

/** A failure that means the account is out of quota or credit: the lane stops at the first one. */
const isQuotaError = (msg) => /HTTP 402|quota|insufficient|not enough (api )?(credits|tokens)|payment required/i.test(String(msg));

/** Uploads one approved reference through v1 init-image (or reuses its earlier upload); returns the init image id. */
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
    // The presigned slot gets no authorization header; its URL may be signed, so it is never logged.
    const put = await fetch(slot.url, { method: 'POST', body: form });
    if (![200, 201, 204].includes(put.status)) throw new Error(`upload slot answered HTTP ${put.status}`);
    up.initImageId = slot.id;
    up.status = 'OK';
  } catch (e) {
    up.error = redact(e.message ?? e, key).slice(0, 500);
  }
  doc.uploads.push(up);
  writeLog(doc);
  if (up.status !== 'OK') throw new Error(`reference upload ${name} failed: ${up.error}`);
  return up.initImageId;
}

/** The generation object's fields that say whether the references were taken (routing is inside the test boundary). */
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

function refuseIfOver(doc, quality) {
  if (doc.latched) return `the lane is latched by a quota or 402 error at ${doc.latched.date}: ${doc.latched.error}`;
  const staging = path.resolve(STAGING).toLowerCase();
  if ([ROOT, VR_MAIN, 'C:/Users/kazda/kiro', 'C:/Users/kazda/ai-tools'].some((r) => staging.startsWith(path.resolve(r).toLowerCase()))) {
    return `the staging folder ${STAGING} is inside a repo or a reserved folder`;
  }
  const lane = doc.calls.length;
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
  const doc = readLog();
  const why = refuseIfOver(doc, quality);
  if (why) {
    console.error(`REFUSED: ${why}`);
    process.exit(2);
  }
  const name = `${id}-s${seed}-${quality.toLowerCase()}`;
  if (doc.calls.some((x) => x.out === name && x.status === 'COMPLETE')) {
    console.error(`REFUSED: ${name} already exists in the log; pick another seed`);
    process.exit(2);
  }

  let key;
  try {
    key = readKey();
  } catch (e) {
    // No request was sent, so there is no call to log; the message names the file, never a key.
    console.error(`BLOCKED: no Leonardo key (${e.code ?? 'error'}: ${String(e.message).split('\n')[0].slice(0, 200)})`);
    process.exit(3);
  }
  const prompt = compile(id);
  const entry = {
    date: new Date().toISOString(), id, what: c.what, model: MODEL, public: false, seed, quality, width: W, height: H,
    lightSlot: 'reference-sheet override', refs: c.refs.map((r) => ({ ref: r, file: REFS[r].file, sha256: REFS[r].sha256 })),
    prompt, promptSha256: sha256(Buffer.from(prompt, 'utf8')), out: name,
    generationId: null, status: 'ERROR', credits: 0, creditsSource: null, file: null, sha256: null,
  };
  let reported = null;
  let before = null;
  let reserved = false;
  try {
    const ids = [];
    for (const r of c.refs) ids.push(await uploadRef(key, doc, r));
    entry.refs.forEach((r, i) => { r.initImageId = ids[i]; });
    const body = {
      model: MODEL,
      public: false,
      parameters: {
        width: W, height: H, prompt_enhance: 'OFF', quality, quantity: 1, seed, prompt,
        ...(ids.length ? { guidances: { image_reference: ids.map((x) => ({ image: { id: x, type: 'UPLOADED' } })) } } : {}),
      },
    };
    // Logged before the first billed request: a run killed mid-call, or one started meanwhile, still counts it.
    writeLog({ ...doc, calls: [...doc.calls, { ...entry, status: 'PENDING', credits: COST[quality], creditsSource: 'reserved' }] });
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
  writeLog(doc);
  console.log(`${entry.status} ${id} s${seed} ${quality} credits=${entry.credits} (${entry.creditsSource}) lane=${doc.calls.length}/${LANE_CAP} ` +
    `prompt=${entry.promptSha256.slice(0, 12)} file=${entry.file ?? '-'} sha256=${entry.sha256 ?? '-'}${entry.error ? ` error=${entry.error}` : ''}`);
  if (entry.status !== 'COMPLETE' || !entry.file) process.exit(1);
}

function verify() {
  const doc = readLog();
  let bad = 0;
  for (const c of doc.calls.filter((x) => x.file)) {
    const ok = fs.existsSync(c.file) && sha256File(c.file) === c.sha256;
    if (!ok) bad++;
    console.log(`${ok ? 'OK  ' : 'BAD '} ${c.id} s${c.seed} ${c.sha256} ${c.file}`);
  }
  if (bad) process.exit(1);
}

{
  if (cmd === 'gen') await gen();
  else if (cmd === 'budget') {
    const doc = readLog();
    const day = dayCount();
    console.log(JSON.stringify({
      day: new Date().toISOString().slice(0, 10), dayImages: day.images, dayCap: DAY_CAP, dayCredits: day.credits,
      nightCap: NIGHT_CAP, clientNightTotal: nightTotal(nightArts(ROOT)), laneImages: doc.calls.length, laneCap: LANE_CAP,
      latched: doc.latched, blockDrift: blockDrift(),
    }));
  } else if (cmd === 'list') {
    for (const [id, c] of Object.entries(CALLS)) console.log(`${id}\t${W}x${H}\trefs=${c.refs.join('+') || '-'}\t${c.what}`);
  } else if (cmd === 'prompt') {
    const p = compile(opt('id', 'front'));
    console.log(p);
    console.log(`\nsha256 ${sha256(Buffer.from(p, 'utf8'))} (${p.length} chars)`);
  } else if (cmd === 'verify') verify();
  else {
    console.error('usage: budget | list | prompt --id <id> | gen --id <id> --seed <n> | verify (see the header)');
    process.exit(1);
  }
}
