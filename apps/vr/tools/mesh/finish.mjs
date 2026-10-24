#!/usr/bin/env node
// Finishing wrapper: one raw image-to-3D mesh -> the class's world scale and LOD0 triangle budget, gated on both sides.
//   1. mesh card on the raw file (stage raw): refuses when a finding is one finishing does not resolve
//   2. apps/vr/tools/assets/finish.py (Blender 4.2): floaters dropped, scaled to the palette set's HEIGHT with the feet
//      at z = 0, voxel remesh, collapse to the class triangle target, UV, bake; then --mode bake (default) the baked
//      TRELLIS albedo as the texture T_<name>_BaseColor.png, or --mode flat one flat palette colour per material
//   3. export_glb.py: the finished SM_ mesh as <name>_finish.glb (bake mode: the texture embedded)
//   4. mesh card on that GLB (stage finished, the class budget)
// Writes <outdir>/<name>_card_raw.json, <name>_finish.{blend,json,glb}, <name>_card_finished.json and
// <name>_finish_record.json (seconds per step, before and after numbers).
//
//   node apps/vr/tools/mesh/finish.mjs <raw.glb> <outdir> --class Humanoid --name Brennic
//        [--palette <finish.py set, default the class's>] [--rot <deg>] [--bake <px>] [--voxel <m>] [--budgets <json>]
//        [--mode bake|flat] [--clamp none|posterize|hue|both] [--strength <0-0.6>] [--levels <n>] [--cage <m>]
//        [--ray <m>] [--margin <px>]      (the bake-mode flags: finish-args.mjs)
//
// The class (budgets.json) gives the triangle target and the height band; finish.py takes its height from the palette
// set, so a set whose HEIGHT disagrees with the class shows up as scale-off on the finished card.
// Exit: the finished card's (0 pass or warn, 1 fail, 3 incomplete); 1 when the raw card refuses; 2 usage or error.
import { mkdirSync, readFileSync, writeFileSync } from 'node:fs';
import { join, resolve } from 'node:path';
import { HERE, runBlender } from './blender.mjs';
import { finishParams, finishPyArgs } from './finish-args.mjs';
import { classBudget, exitCodeFor, MESH_CODES } from './gate-lib.mjs';
import { meshCard } from './mesh-card.mjs';

const FINISH = resolve(HERE, '..', 'assets', 'finish.py');

const argv = process.argv.slice(2);
const opt = (name, dflt) => {
  const i = argv.indexOf(`--${name}`);
  return i >= 0 && i + 1 < argv.length ? argv[i + 1] : dflt;
};
const [raw, outdir] = argv.filter((a, i) => !a.startsWith('--') && !argv[i - 1]?.startsWith('--'));
const className = opt('class');
const name = opt('name');
if (!raw || !outdir || !className || !name) {
  console.error('usage: finish.mjs <raw.glb> <outdir> --class <class> --name <Name> [--palette <set>] [--rot <deg>] [--bake <px>] [--voxel <m>] [--budgets <json>] [--mode bake|flat] [--clamp none|posterize|hue|both] [--strength <s>] [--levels <n>] [--cage <m>] [--ray <m>] [--margin <px>]');
  process.exit(2);
}

try {
  const budgetsPath = opt('budgets', join(HERE, 'budgets.json'));
  const budgets = JSON.parse(readFileSync(budgetsPath, 'utf8'));
  const cls = classBudget(budgets, className);
  const params = finishParams(opt, budgets, cls);
  mkdirSync(outdir, { recursive: true });
  const rec = { at: new Date().toISOString(), raw: resolve(raw), class: className, name, params, secs: {} };
  const save = () => writeFileSync(join(outdir, `${name}_finish_record.json`), JSON.stringify(rec, null, 1) + '\n');

  // 1. the raw card
  const before = meshCard(raw, { stage: 'raw', className, budgetsPath });
  writeFileSync(join(outdir, `${name}_card_raw.json`), JSON.stringify(before, null, 1) + '\n');
  rec.secs.cardRaw = before.secs;
  rec.before = { verdict: before.verdict, tris: before.metrics.tris, components: before.metrics.components, heightM: before.metrics.heightM, codes: before.findings.map((x) => x.code) };
  console.log(`raw card ${before.verdict}: ${before.metrics.tris} tris, ${before.metrics.components} components, height ${before.metrics.heightM}; ${rec.before.codes.join(', ') || 'no findings'}`);
  const blocking = before.findings.filter((x) => MESH_CODES[x.code].remedy !== 'finish');
  if (blocking.length) {
    rec.refused = blocking.map((x) => `${x.code}: ${x.line}`);
    save();
    console.error(`refused: finishing does not resolve ${blocking.map((x) => x.code).join(', ')}`);
    process.exit(1);
  }

  // 2. finish.py, 3. export
  const fin = runBlender(FINISH, finishPyArgs(resolve(raw), resolve(outdir), name, params));
  rec.secs.finish = fin.secs;
  rec.finishPy = JSON.parse(readFileSync(join(outdir, `${name}_finish.json`), 'utf8'));
  const glb = join(resolve(outdir), `${name}_finish.glb`);
  rec.secs.export = runBlender(join(HERE, 'export_glb.py'), [glb], { blend: join(resolve(outdir), `${name}_finish.blend`) }).secs;

  // 4. the finished card
  const after = meshCard(glb, { stage: 'finished', className, budgetsPath });
  writeFileSync(join(outdir, `${name}_card_finished.json`), JSON.stringify(after, null, 1) + '\n');
  rec.secs.cardFinished = after.secs;
  rec.after = { verdict: after.verdict, tris: after.metrics.tris, components: after.metrics.components, heightM: after.metrics.heightM, minZ: after.metrics.minZ, watertight: after.metrics.watertight, codes: after.findings.map((x) => x.code) };
  save();
  console.log(`finished card ${after.verdict}: ${after.metrics.tris} tris (budget ${cls.tris.min}-${cls.tris.max}), height ${after.metrics.heightM} m (${className} ${cls.heightM} m +-${100 * cls.heightTolerance} %); ${rec.after.codes.join(', ') || 'no findings'}`);
  console.log(`seconds: ${JSON.stringify(rec.secs)}`);
  process.exit(exitCodeFor(after.verdict));
} catch (e) {
  console.error(e.message);
  process.exit(2);
}
