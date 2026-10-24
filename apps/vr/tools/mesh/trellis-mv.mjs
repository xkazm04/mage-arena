#!/usr/bin/env node
// Two-view TRELLIS.2 on the ComfyUI already running at 127.0.0.1:8188: front + back image in, one GLB out.
// The single-image job stays apps/vr/tools/assets/trellis.mjs, unchanged; this adds the second view to the same workflow
// (trellis-api.json beside that file) and is what `comfy.mjs run ... --back <image>` calls.
//
//   node apps/vr/tools/mesh/trellis-mv.mjs run <front> <label> <seed> <outdir> --back <image> [--faces N] [--dry-run]
//
// The conditioning (read from the installed pack, ComfyUI/comfy_extras/nodes_trellis2.py, 2026-10-24): the pack's only
// multi-image node is Pixal3DMultiViewConditioning (views front, left, back, right at 0, 90, 180, 270 degrees; fov 20 as
// framed). The installed checkpoint trellis_2_int8_convrot has no `proj_linear` weights, so the model runs in its "global"
// attention mode and the node's contribution is the mean of the views' DINOv3 class and register tokens: the back view
// steers the whole-object conditioning, there is no per-voxel projection. Each view goes through the same
// RemoveBackground + ImageCropToMask (pad 1.1, 1024 square, black background) the single-image graph uses, which is the
// framing that node asks for. --dry-run prints the workflow it would send and touches nothing (no ComfyUI, no upload).
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const HERE = path.dirname(fileURLToPath(import.meta.url));
export const BASE_WORKFLOW = path.join(HERE, '..', 'assets', 'trellis-api.json');
const URL_ = 'http://127.0.0.1:8188';
const SEED_NODES = ['3', '12', '18', '23'];
const QUEUE_POLL_MS = 120000;
const JOB_POLL_MS = 15000;
const JOB_LIMIT_MS = 60 * 60 * 1000;
// the single-image view chain (LoadImage 122 -> RemoveBackground 192 -> switch 248 -> MaskPreview 303 -> ImageCropToMask
// 312), cloned for the back view under these ids
const BACK_CHAIN = { 122: '422', 192: '492', 248: '448', 303: '403', 312: '412' };
export const MV_CONDITIONING = '399';
export const MV_FOV = 20;

/** The `run` arguments: `<front> <label> <seed> <outdir>` plus --back <image>, --faces N, --dry-run. */
export function parseRunArgs(argv) {
  const pos = [];
  const o = { back: null, faces: 700000, dryRun: false };
  for (let i = 0; i < argv.length; i++) {
    const a = argv[i];
    if (a === '--dry-run') o.dryRun = true;
    else if (a === '--back' || a === '--faces') {
      if (i + 1 >= argv.length) throw new Error(`${a} needs a value`);
      const v = argv[++i];
      if (a === '--back') o.back = v;
      else o.faces = Number(v);
    } else if (a.startsWith('--')) throw new Error(`unknown argument ${a}`);
    else pos.push(a);
  }
  const [front, label, seedArg, outdir] = pos;
  const seed = Number(seedArg);
  if (pos.length !== 4 || !Number.isInteger(seed) || !Number.isInteger(o.faces) || o.faces < 1) {
    throw new Error('usage: run <front image> <label> <seed> <outdir> [--back <image>] [--faces N] [--dry-run]');
  }
  return { ...o, front, label, seed, outdir };
}

/** The name an image is uploaded to ComfyUI under: one per view, so the two never overwrite each other. */
export const uploadName = (label, view, file) => `t38_${label}_${view}${path.extname(file)}`;

/**
 * `base` (the parsed single-image workflow) with the image, face count, seeds and output prefix set. With `names.back`, the
 * view chain is cloned for the back image and node 299 (Trellis2Conditioning, front only) is replaced by node 399
 * (Pixal3DMultiViewConditioning, front + back) everywhere it was wired. `base` is not modified.
 */
export function buildWorkflow(base, { names, seed, faces, label }) {
  const p = structuredClone(base);
  p['122'].inputs.image = names.front;
  p['186'].inputs.target_face_count = faces;
  for (const id of SEED_NODES) p[id].inputs.seed = seed;
  p['322'].inputs.filename_prefix = `t38/${label}`;
  if (!names.back) return p;

  const remap = (v) => (Array.isArray(v) && BACK_CHAIN[v[0]] ? [BACK_CHAIN[v[0]], v[1]] : v);
  for (const [from, to] of Object.entries(BACK_CHAIN)) {
    const node = structuredClone(p[from]);
    node.inputs = Object.fromEntries(Object.entries(node.inputs).map(([k, v]) => [k, remap(v)]));
    if (node._meta) node._meta = { ...node._meta, title: `${node._meta.title} (back)` };
    p[to] = node;
  }
  p['422'].inputs.image = names.back;
  p[MV_CONDITIONING] = {
    inputs: { fov: MV_FOV, clip_vision_model: ['15', 0], front: ['312', 0], back: ['412', 0] },
    class_type: 'Pixal3DMultiViewConditioning',
    _meta: { title: 'Pixal3D Multi-View Conditioning (front + back)' },
  };
  for (const node of Object.values(p)) {
    for (const [k, v] of Object.entries(node.inputs)) {
      if (Array.isArray(v) && v[0] === '299') node.inputs[k] = [MV_CONDITIONING, v[1]];
    }
  }
  delete p['299'];
  return p;
}

const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
const stamp = () => new Date().toISOString();
const getJson = async (p) => (await fetch(URL_ + p)).json();

async function upload(file, name) {
  const form = new FormData();
  form.append('image', new Blob([fs.readFileSync(file)]), name);
  form.append('overwrite', 'true');
  const up = await (await fetch(URL_ + '/upload/image', { method: 'POST', body: form })).json();
  return up.subfolder ? `${up.subfolder}/${up.name}` : up.name;
}

async function waitIdle() {
  for (;;) {
    const q = await getJson('/queue');
    const busy = q.queue_running.length + q.queue_pending.length;
    if (!busy) return;
    console.log(`${stamp()} queue busy (running ${q.queue_running.length}, pending ${q.queue_pending.length}); next look in 2 min`);
    await sleep(QUEUE_POLL_MS);
  }
}

/** The dry run: the workflow `run` would send, with the upload names the images would get. Reads no ComfyUI. */
export function dryRun(a, base = JSON.parse(fs.readFileSync(BASE_WORKFLOW, 'utf8'))) {
  for (const f of [a.front, a.back].filter(Boolean)) if (!fs.existsSync(f)) throw new Error(`no such image: ${f}`);
  const names = { front: uploadName(a.label, 'front', a.front), back: a.back ? uploadName(a.label, 'back', a.back) : null };
  return buildWorkflow(base, { names, seed: a.seed, faces: a.faces, label: a.label });
}

async function run(argv) {
  const a = parseRunArgs(argv);
  if (a.dryRun) {
    console.log(JSON.stringify(dryRun(a), null, 1));
    return;
  }
  if (!a.back) throw new Error('trellis-mv.mjs needs --back; the single image job is apps/vr/tools/assets/trellis.mjs');
  fs.mkdirSync(a.outdir, { recursive: true });
  const base = JSON.parse(fs.readFileSync(BASE_WORKFLOW, 'utf8'));
  const names = {
    front: await upload(a.front, uploadName(a.label, 'front', a.front)),
    back: await upload(a.back, uploadName(a.label, 'back', a.back)),
  };
  const prompt = buildWorkflow(base, { names, seed: a.seed, faces: a.faces, label: a.label });

  await waitIdle();
  const t0 = Date.now();
  const res = await fetch(URL_ + '/prompt', { method: 'POST', headers: { 'content-type': 'application/json' }, body: JSON.stringify({ prompt }) });
  const body = await res.json();
  if (!res.ok || body.error) throw new Error(`submit failed: ${JSON.stringify(body).slice(0, 2000)}`);
  const pid = body.prompt_id;
  console.log(`${stamp()} submitted ${a.label} seed ${a.seed} faces ${a.faces} (front + back) as ${pid}`);
  let h;
  for (;;) {
    await sleep(JOB_POLL_MS);
    h = (await getJson(`/history/${pid}`))[pid];
    if (h?.status?.completed || h?.status?.status_str === 'error') break;
    if (Date.now() - t0 > JOB_LIMIT_MS) throw new Error(`job ${pid} still running after 60 min`);
  }
  const record = {
    label: a.label, seed: a.seed, faces: a.faces, views: ['front', 'back'], front: a.front, back: a.back, promptId: pid,
    secs: Math.round((Date.now() - t0) / 1000), status: h.status?.status_str, at: new Date(t0).toISOString(), files: [],
  };
  if (h.status?.status_str === 'error') {
    record.error = JSON.stringify(h.status.messages?.find((m) => m[0] === 'execution_error')?.[1] ?? {}).slice(0, 1500);
  }
  for (const o of Object.values(h.outputs ?? {})) {
    for (const x of Object.values(o).flat()) {
      if (!x?.filename || !/\.(glb|gltf|obj|fbx)$/i.test(x.filename) || x.type !== 'output') continue;
      const q = new URLSearchParams({ filename: x.filename, subfolder: x.subfolder ?? '', type: x.type });
      const buf = Buffer.from(await (await fetch(`${URL_}/view?${q}`)).arrayBuffer());
      const dest = path.join(a.outdir, `${a.label}${path.extname(x.filename)}`);
      fs.writeFileSync(dest, buf);
      record.files.push({ comfy: `${x.subfolder}/${x.filename}`, local: dest, bytes: buf.length });
    }
  }
  fs.appendFileSync(path.join(a.outdir, 'trellis-runs.jsonl'), JSON.stringify(record) + '\n');
  console.log(JSON.stringify(record));
  if (record.status !== 'success' || !record.files.length) process.exitCode = 1;
}

if (process.argv[1] && path.resolve(process.argv[1]) === fileURLToPath(import.meta.url)) {
  try {
    if (process.argv[2] === 'run') await run(process.argv.slice(3));
    else {
      console.error('usage: trellis-mv.mjs run <front> <label> <seed> <outdir> --back <image> [--faces N] [--dry-run]');
      process.exitCode = 2;
    }
  } catch (e) {
    console.error(e.message);
    process.exitCode = 2;
  }
}
