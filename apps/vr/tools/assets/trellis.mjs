#!/usr/bin/env node
// T38: one TRELLIS.2 image-to-3D job on the ComfyUI already running at 127.0.0.1:8188 (never start a second one).
// Workflow: trellis-api.json beside this file, a copy of lynx-bench media/3d-trellis-api.json. Dependency-free.
//
//   node apps/vr/tools/assets/trellis.mjs run <concept image> <label> <seed> <outdir> [--faces 700000]
//   node apps/vr/tools/assets/trellis.mjs free      unload models and free memory (before the Blender bakes)
//
// One GPU job at a time on the machine: before submitting, GET /queue; while anything runs or waits, poll every
// 2 minutes and do not submit beside it. DecimateMesh (186) runs before UnwrapMesh (196) and the texture bake (147),
// so the baked texture matches the decimated mesh. The seed goes to the four samplers (3, 12, 18, 23).
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const HERE = path.dirname(fileURLToPath(import.meta.url));
const URL_ = 'http://127.0.0.1:8188';
const SEED_NODES = ['3', '12', '18', '23'];
const QUEUE_POLL_MS = 120000;
const JOB_POLL_MS = 15000;
const JOB_LIMIT_MS = 60 * 60 * 1000;

const args = process.argv.slice(2);
const opt = (name, dflt) => {
  const i = args.indexOf(`--${name}`);
  return i > 0 && i + 1 < args.length ? args[i + 1] : dflt;
};
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
const getJson = async (p) => (await fetch(URL_ + p)).json();
const stamp = () => new Date().toISOString();

async function waitIdle() {
  for (;;) {
    const q = await getJson('/queue');
    const busy = q.queue_running.length + q.queue_pending.length;
    if (!busy) return;
    console.log(`${stamp()} queue busy (running ${q.queue_running.length}, pending ${q.queue_pending.length}); next look in 2 min`);
    await sleep(QUEUE_POLL_MS);
  }
}

async function run() {
  const [, image, label, seedArg, outdir] = args;
  const seed = Number(seedArg);
  const faces = Number(opt('faces', '700000'));
  if (!image || !label || !Number.isInteger(seed) || !outdir) throw new Error('usage: run <image> <label> <seed> <outdir> [--faces N]');
  fs.mkdirSync(outdir, { recursive: true });

  const form = new FormData();
  form.append('image', new Blob([fs.readFileSync(image)]), `t38_${label}${path.extname(image)}`);
  form.append('overwrite', 'true');
  const up = await (await fetch(URL_ + '/upload/image', { method: 'POST', body: form })).json();

  const prompt = JSON.parse(fs.readFileSync(path.join(HERE, 'trellis-api.json'), 'utf8'));
  prompt['122'].inputs.image = up.subfolder ? `${up.subfolder}/${up.name}` : up.name;
  prompt['186'].inputs.target_face_count = faces;
  for (const id of SEED_NODES) prompt[id].inputs.seed = seed;
  prompt['322'].inputs.filename_prefix = `t38/${label}`;

  await waitIdle();
  const t0 = Date.now();
  const res = await fetch(URL_ + '/prompt', { method: 'POST', headers: { 'content-type': 'application/json' }, body: JSON.stringify({ prompt }) });
  const body = await res.json();
  if (!res.ok || body.error) throw new Error(`submit failed: ${JSON.stringify(body).slice(0, 2000)}`);
  const pid = body.prompt_id;
  console.log(`${stamp()} submitted ${label} seed ${seed} faces ${faces} as ${pid}`);
  let h;
  for (;;) {
    await sleep(JOB_POLL_MS);
    h = (await getJson(`/history/${pid}`))[pid];
    if (h?.status?.completed || h?.status?.status_str === 'error') break;
    if (Date.now() - t0 > JOB_LIMIT_MS) throw new Error(`job ${pid} still running after 60 min`);
  }
  const secs = Math.round((Date.now() - t0) / 1000);
  const record = { label, seed, faces, promptId: pid, secs, status: h.status?.status_str, at: new Date(t0).toISOString(), files: [] };
  if (h.status?.status_str === 'error') {
    record.error = JSON.stringify(h.status.messages?.find((m) => m[0] === 'execution_error')?.[1] ?? {}).slice(0, 1500);
  }
  for (const o of Object.values(h.outputs ?? {})) {
    for (const x of Object.values(o).flat()) {
      if (!x?.filename || !/\.(glb|gltf|obj|fbx)$/i.test(x.filename) || x.type !== 'output') continue;
      const q = new URLSearchParams({ filename: x.filename, subfolder: x.subfolder ?? '', type: x.type });
      const buf = Buffer.from(await (await fetch(`${URL_}/view?${q}`)).arrayBuffer());
      const dest = path.join(outdir, `${label}${path.extname(x.filename)}`);
      fs.writeFileSync(dest, buf);
      record.files.push({ comfy: `${x.subfolder}/${x.filename}`, local: dest, bytes: buf.length });
    }
  }
  fs.appendFileSync(path.join(outdir, 'trellis-runs.jsonl'), JSON.stringify(record) + '\n');
  console.log(JSON.stringify(record));
  if (record.status !== 'success' || !record.files.length) process.exit(1);
}

async function free() {
  const res = await fetch(URL_ + '/free', { method: 'POST', headers: { 'content-type': 'application/json' }, body: JSON.stringify({ unload_models: true, free_memory: true }) });
  console.log(`${stamp()} /free -> HTTP ${res.status}`);
}

if (args[0] === 'run') await run();
else if (args[0] === 'free') await free();
else { console.error('usage: run <image> <label> <seed> <outdir> [--faces N] | free'); process.exit(1); }
