#!/usr/bin/env node
// T38 concept art: one Leonardo generation per call, logged. Dependency-free (Node's fetch and fs only).
// Adapted from kiro/bp scripts/lookdev/leonardo.mjs (its call recipe lives in leonardo-client.mjs).
//
//   node apps/vr/tools/assets/leonardo.mjs gen [--asset conscript] --view front --seed 101 --quality MEDIUM [--out name.png]
//   node apps/vr/tools/assets/leonardo.mjs gen --asset mire-maw --view back-ref --seed 7301 --ref <image> --ref-sha256 <hex> --staging <dir>
//       --ref uploads <image> (sha256-checked when --ref-sha256 is given) and sends it as the call's image_reference;
//       --staging saves the image and a .sha256 file in <dir> (outside every repo) instead of the asset folder
//   node apps/vr/tools/assets/leonardo.mjs total [--asset conscript]    the credit total of the asset's cap group from the logs
//   node apps/vr/tools/assets/leonardo.mjs prompt [--asset conscript] --view front    print the composed prompt, no call
//   node apps/vr/tools/assets/leonardo.mjs night    the project's credits for today (UTC) from every log, against NIGHT_CAP
//
// --asset picks the subject, palette, output folder and log: brennic (the default, whose prompt text stays byte-identical),
// conscript or slinger (the two humanoid Bout 2 soldiers), cinder-hound or mire-maw (the two Bout 2 beasts, with their
// own view texts). Assets of one cap group share one credit cap.
//
// The key is LEONARDO_API_KEY, read at run time from bp's untracked .env. It is never printed, logged or written
// anywhere; neither is the authorization header. Every call is "public": false. Every call, failed ones included,
// is appended to apps/vr/art/characters/<asset>/leonardo-log.json, as a PENDING row at the expected cost before the
// request is sent and replaced when the call ends. The logged total of a cap group may never pass its cap,
// and the project's total for the UTC day (every leonardo-log.json under apps/vr/art/ of this tree, the
// main checkout and the sibling worktrees, read only, each generation counted once) may never pass NIGHT_CAP.
import crypto from 'node:crypto';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { API, COST, MODEL, NIGHT_CAP, balance, call, imageReferenceGuidance, pollGeneration, nightArts, nightTotal as nightTotalOf, readKey, redact, reportedCost, routingEvidence, saveImage, settleCredits, sumCredits, uploadInitImage, writeLog as writeLogTo } from './leonardo-client.mjs';

const ROOT = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../../../..');
const CHARS = path.join(ROOT, 'apps/vr/art/characters');

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
// The Bout 2 beasts as model sheets 05-07 draw them (silhouette only), with the T15 card's colours: the hound low, long and
// fast with ember-orange accents, the maw a big squat mound with a mouth slot in bog green-brown. A beast's view texts
// replace the humanoid ones (no "he", no sandals); the hound's turnaround puts its side view first, where its length reads.
const CINDER_HOUND_SUBJECT =
  'a cinder hound, a lean long-bodied hunting hound made of cracked dark cinder and ash: a long low body with a deep ' +
  'narrow chest and a tucked belly, a narrow pointed skull on a long neck, a long closed muzzle, short upright pointed ' +
  'ears, long thin legs with compact paws, and a long thin tail. Broad glowing ember-orange bands run across the cinder ' +
  'like wide cracks: a collar band round the neck, a band over each shoulder and each hip, a band round each ankle, ' +
  'and the eyes glow ember orange.';
const CINDER_HOUND_POSE =
  'It stands in a neutral four-legged stance: the back level, all four legs straight and slightly apart, clear of each ' +
  'other and of the body, paws flat, the head raised on the neck and looking straight ahead, the mouth closed, the tail ' +
  'held straight out behind. No fire, no flames, no smoke, no sparks, no effects.';
const MIRE_MAW_SUBJECT =
  'a mire maw, a big squat toad-like bog creature shaped like a mound: a huge round body as wide as it is tall with ' +
  'the head merged into it, a very wide closed mouth slot running across the whole front from side to side with thick ' +
  'muddy lips, two small eyes on low bulges on top, two nostril holes, short thick bowed legs with wide flat webbed ' +
  'feet, and two small stubby arms resting on the round belly. A thick ragged mat of moss covers its back and the top ' +
  'of its head, with a few small pebbles in it.';
const MIRE_MAW_POSE =
  'It squats on its short legs in a neutral pose: the mouth closed, the small arms resting on the belly, the feet flat ' +
  'and apart. No reeds, no thin spikes, no water, no splashes, no effects.';
// The maw's back (2026-10-24, docs/research/MAW-BACK-2026-10-24.md): drawn from the accepted open-gape master sent as an
// image_reference, so the back is the same creature in the same look. The reference is not content to copy: the back must
// be a closed shell.
const MIRE_MAW_BACK_REF = {
  width: 1376,
  height: 768,
  text:
    'The attached image is the approved identity master of a mire maw seen from the front. Draw the same single creature ' +
    'seen from directly behind, the camera turned 180 degrees around it to look at its back, with exactly the same ' +
    'rendering style, mud-brown warty hide with darker brown lumps and blunt pebble-like bumps, lighter tan underside, ' +
    'the same proportions, the same size, the same camera height, the same framing and the same plain flat light grey ' +
    'background as the attached image. Seen from behind: the broad, low, heavy rounded back covered with the same warty ' +
    'lumps, the wide flat back of the head merged into the body with the two small yellow eyes just visible on low bulges ' +
    'at the top edge, the two thick bowed rear legs with wide flat clawed feet planted on both sides, and the front feet ' +
    'just visible beyond the sides of the body. The back is one closed, whole, solid shell of continuous hide: no tears, ' +
    'no holes, no gaps, no open mouth, no teeth and no throat visible, no hollow, no cut-away. The whole creature from ' +
    'the top of the back to the soles of the feet is fully visible with an empty margin on every side, centred and ' +
    'filling about 80 percent of the image width. Matte surfaces, one flat colour per material, no fine texture detail, ' +
    'no outlines, soft even studio light, no cast shadow, no contact shadow. Avoid: scenery, floor, ground plane, sand, ' +
    'sky, flames, glow, sparks, smoke, steam, water, magic effects, a second creature, a second view, props, labels, ' +
    'captions, text, grid lines, colour swatches.',
};
const BEAST_LOOK_HEAD =
  'Game-ready 3D creature model reference for a modeller: simple large readable shapes, chunky forms, matte surfaces, ' +
  'one flat colour per material with no pattern and no texture detail. Palette blocks: ';
/** A beast's views: front and turnaround in creature wording; `frontSize` is [width, height] of the front call. */
const beastViews = (frontSize, sideFirst) => (A) => ({
  front: {
    width: frontSize[0],
    height: frontSize[1],
    text: `Full-body orthographic front view of ${A.subject} ${A.pose} It faces the camera directly, symmetrical, the ` +
      `whole creature from the top of the head to the feet visible with empty margin around it, centred on a plain flat ` +
      `light grey background, no scenery, no ground, a single creature. ${A.look}`,
  },
  turnaround: {
    width: 1376,
    height: 768,
    text: `Creature turnaround sheet of ${A.subject} Three full-body orthographic views of the same creature side by side ` +
      `at the same scale and height: ${sideFirst ? 'on the left a wide side view in profile with the head to the right, ' +
      'then a front view, then a back view' : 'front view, right side view, back view'}. ${A.pose} Plain flat light grey ` +
      `background, no scenery, no ground. ${A.look}`,
  },
});
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
  // the T15 beasts run (2026-10-21): at most 2 calls plus 1 retry per beast, 112 credits for the two
  'cinder-hound': {
    card: 'T15', title: 'Cinder hound concept', subject: CINDER_HOUND_SUBJECT, pose: CINDER_HOUND_POSE,
    group: ['cinder-hound', 'mire-maw'], cap: 112, views: beastViews([768, 1376], true),
    look: `${BEAST_LOOK_HEAD}cinder body dark charcoal grey, glowing bands and eyes bright ember orange, paws and claws ` +
      `black.${LOOK_TAIL}`,
  },
  'mire-maw': {
    card: 'T15', title: 'Mire maw concept', subject: MIRE_MAW_SUBJECT, pose: MIRE_MAW_POSE,
    group: ['cinder-hound', 'mire-maw'], cap: 112, views: (A) => ({ ...beastViews([1376, 768], false)(A), 'back-ref': MIRE_MAW_BACK_REF }),
    look: `${BEAST_LOOK_HEAD}skin bog olive green-brown, belly lighter muddy tan, moss mat dark mossy green, lips and ` +
      `mouth slot dark mud brown, eyes pale yellow, pebbles grey.${LOOK_TAIL}`,
  },
};
const viewsFor = (A) => A.views ? A.views(A) : ({
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
/** The credits logged so far by every asset of this asset's cap group; `log` stands in for this asset's own log. */
const runTotal = (log = readLog()) => sumCredits(log) + sumCredits(ASSET.group.filter((id) => id !== assetId).flatMap(readLogOf));
/** The project's credits for the UTC day `day` (YYYY-MM-DD): every checkout's apps/vr/art logs, each generation once. */
const nightTotal = (day) => nightTotalOf(nightArts(ROOT), day);
function writeLog(calls) {
  writeLogTo(LOG, {
    card: ASSET.card,
    asset: ASSET.title,
    model: MODEL,
    runCap: RUN_CAP,
    totalCredits: runTotal(calls),
    note: 'One entry per Leonardo call, failed calls included. A PENDING entry (creditsSource reserved) is a call whose run ended before the call did; it counts at the expected cost. credits come from the API response (creditsSource api), else the account balance difference, else the expected cost (fallback).',
    calls,
  });
}

async function gen() {
  const viewId = opt('view', 'front');
  const view = VIEWS[viewId];
  if (!view) throw new Error(`unknown view ${viewId}; known: ${Object.keys(VIEWS).join(', ')}`);
  const seed = Number(opt('seed', '1'));
  const quality = opt('quality', 'MEDIUM').toUpperCase();
  if (!(quality in COST)) throw new Error('quality must be LOW, MEDIUM or HIGH');
  if (!Number.isInteger(seed)) throw new Error('seed must be an integer');
  const refFile = opt('ref');
  const refSha = opt('ref-sha256');
  const staging = opt('staging');
  if (refSha && !refFile) throw new Error('--ref-sha256 needs --ref');
  const outName = opt('out', `concept-${viewId}-s${seed}-${quality.toLowerCase()}`);
  if (path.basename(outName) !== outName) throw new Error('--out is a file name inside the asset folder');

  const log = readLog();
  if (runTotal(log) + COST[quality] > RUN_CAP) {
    console.error(`REFUSED: run total ${runTotal(log)} + expected ${COST[quality]} at ${quality} would pass ${RUN_CAP}`);
    process.exit(2);
  }
  if (nightTotal() + COST[quality] > NIGHT_CAP) {
    console.error(`REFUSED: the project's total tonight ${nightTotal()} + expected ${COST[quality]} would pass ${NIGHT_CAP}`);
    process.exit(2);
  }

  const key = readKey();
  const parameters = { width: view.width, height: view.height, prompt_enhance: 'OFF', quality, quantity: 1, seed, prompt: view.text };
  const entry = {
    date: new Date().toISOString(), view: viewId, model: MODEL, public: false, prompt: view.text, seed, quality,
    width: view.width, height: view.height, generationId: null, status: 'ERROR', credits: 0, creditsSource: null, file: null,
  };
  if (refFile) {
    // the hash is checked before anything is logged or sent
    const got = crypto.createHash('sha256').update(fs.readFileSync(refFile)).digest('hex');
    if (refSha && got !== refSha) {
      console.error(`REFUSED: ${refFile} is not the expected image (sha256 ${got})`);
      process.exit(2);
    }
    entry.refs = [{ file: path.resolve(refFile).split('\\').join('/'), sha256: got, initImageId: null }];
  }
  // Logged before the first billed request: a run killed mid-call, or one started meanwhile, still counts these credits.
  writeLog([...log, { ...entry, status: 'PENDING', credits: COST[quality], creditsSource: 'reserved' }]);
  const before = await balance(key);
  let reported = null;
  try {
    if (refFile) {
      entry.refs[0].initImageId = await uploadInitImage(key, refFile, { sha256: refSha });
      parameters.guidances = imageReferenceGuidance([entry.refs[0].initImageId]);
    }
    const body = { model: MODEL, public: false, parameters };
    const created = await call(key, 'POST', `${API}/v2/generations`, body);
    entry.generationId = created.generate?.generationId ?? created.generationId ?? null;
    reported = reportedCost(created);
    if (!entry.generationId) throw new Error(`no generationId in response: ${JSON.stringify(created).slice(0, 300)}`);
    const { status, generation: g } = await pollGeneration(
      async (gid) => (await call(key, 'GET', `${API}/v1/generations/${gid}`)).generations_by_pk, entry.generationId);
    entry.status = status;
    if (refFile) entry.routing = routingEvidence(g);
    reported ??= reportedCost(g);
    const url = (g?.generated_images ?? [])[0]?.url;
    if (entry.status === 'COMPLETE' && url) {
      const { dest, bytes } = await saveImage(url, staging ? path.resolve(staging) : DIR, outName);
      entry.bytes = bytes;
      if (staging) {
        entry.file = dest.replaceAll('\\', '/');
        entry.sha256 = crypto.createHash('sha256').update(fs.readFileSync(dest)).digest('hex');
        fs.writeFileSync(`${dest}.sha256`, `${entry.sha256} *${path.basename(dest)}\n`);
      } else {
        entry.file = path.relative(ROOT, dest).replaceAll('\\', '/');
      }
    }
  } catch (e) {
    entry.error = redact(e.message ?? e, key).slice(0, 500);
  }
  const after = await balance(key);
  Object.assign(entry, settleCredits({ reported, before, after, generationId: entry.generationId, expected: COST[quality] }));
  writeLog([...log, entry]);
  console.log(`${entry.status} ${viewId} s${seed} ${quality} credits=${entry.credits} (${entry.creditsSource}) total=${runTotal()} file=${entry.file ?? '-'}${entry.error ? ` error=${entry.error}` : ''}`);
  if (entry.status !== 'COMPLETE' || !entry.file) process.exit(1);
}

if (cmd === 'gen') await gen();
else if (cmd === 'total') console.log(JSON.stringify({ asset: assetId, calls: readLog().length, totalCredits: runTotal(), cap: RUN_CAP }));
else if (cmd === 'night') console.log(JSON.stringify({ day: new Date().toISOString().slice(0, 10), totalCredits: nightTotal(), cap: NIGHT_CAP }));
else if (cmd === 'prompt') console.log(VIEWS[opt('view', 'front')]?.text ?? 'unknown view');
else { console.error('usage: gen | total | prompt (see the header)'); process.exit(1); }
