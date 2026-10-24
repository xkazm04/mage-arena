#!/usr/bin/env node
// Input gate: before any image-to-3D job, check mechanically that the source image (or view set) can be
// reconstructed, and write a verdict JSON with defect codes. Blender measures (image_stats.py), gate-lib.mjs decides.
//
//   node apps/vr/tools/mesh/input-gate.mjs <image> --out <verdict.json>
//        one image: the master (front) view; the set needs only the front
//   node apps/vr/tools/mesh/input-gate.mjs --front <a> [--side <b>] [--back <c>] [--require front,side,back] --out <v.json>
//        a view set, judged as a set against the master (front)
//
// Exit: 0 proceed (pass, warn); 1 refuse (fail, prepare: fix the image and gate it again from scratch);
// 3 incomplete (a criterion is UNMEASURED: generating anyway records the input as ungated on it); 2 usage or error.
// A pass is not a quality claim; the JSON says so.
import { createHash } from 'node:crypto';
import { mkdirSync, readFileSync, writeFileSync } from 'node:fs';
import { dirname, resolve } from 'node:path';
import { extract } from './blender.mjs';
import { exitCodeFor, gateInput } from './gate-lib.mjs';

const VIEWS = ['front', 'side', 'back'];

function parse(argv) {
  const o = { views: {}, out: null, require: null };
  for (let i = 0; i < argv.length; i++) {
    const a = argv[i];
    const next = () => {
      if (i + 1 >= argv.length) throw new Error(`${a} needs a value`);
      return argv[++i];
    };
    if (a === '--out') o.out = next();
    else if (a === '--require') o.require = next().split(',').filter(Boolean);
    else if (VIEWS.includes(a.slice(2)) && a.startsWith('--')) o.views[a.slice(2)] = next();
    else if (!a.startsWith('--') && !o.views.front) o.views.front = a;
    else throw new Error(`unknown argument ${a}`);
  }
  if (!o.views.front && !Object.keys(o.views).length) throw new Error('no image given');
  if (!o.out) throw new Error('--out <verdict.json> is required');
  const set = Object.keys(o.views).length > 1 || o.views.side || o.views.back;
  o.require ??= set ? VIEWS : ['front'];
  for (const v of o.require) if (!VIEWS.includes(v)) throw new Error(`unknown view ${v}`);
  return o;
}

let o;
try {
  o = parse(process.argv.slice(2));
} catch (e) {
  console.error(`${e.message}\nusage: input-gate.mjs <image> --out <v.json> | --front <a> [--side <b>] [--back <c>] [--require front,side,back] --out <v.json>`);
  process.exit(2);
}

try {
  const t0 = Date.now();
  const stats = {};
  const files = {};
  for (const [view, file] of Object.entries(o.views)) {
    const { data } = extract('image_stats.py', resolve(file));
    stats[view] = data;
    files[view] = { path: resolve(file), sha256: createHash('sha256').update(readFileSync(file)).digest('hex') };
  }
  const result = gateInput(stats, o.require);
  const record = { ...result, at: new Date(t0).toISOString(), secs: Number(((Date.now() - t0) / 1000).toFixed(1)), files };
  mkdirSync(dirname(resolve(o.out)), { recursive: true });
  writeFileSync(o.out, JSON.stringify(record, null, 1) + '\n');
  console.log(`input-gate ${result.verdict}: ${result.findings.map((f) => `${f.code}${f.view ? `(${f.view})` : ''}`).join(', ') || 'no findings'}${result.unmeasured.length ? `; unmeasured: ${result.unmeasured.join(', ')}` : ''}`);
  console.log(result.notice);
  process.exit(exitCodeFor(result.verdict));
} catch (e) {
  console.error(e.message);
  process.exit(2);
}
