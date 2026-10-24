#!/usr/bin/env node
// Mesh card: the structural acceptance card of one generated or finished mesh. Blender headless measures
// (mesh_metrics.py: triangles, components and their face shares, watertightness, winding, bbox, height, origin),
// gate-lib.mjs scores the numbers into defect codes. Structural only: a pass is not a quality claim.
//
//   node apps/vr/tools/mesh/mesh-card.mjs <mesh.glb|mesh.fbx|mesh.blend> --out <card.json>
//        [--stage raw|finished] [--class Humanoid] [--budgets <budgets.json>]
//
// --stage: raw grades what the finishing pass resolves (density, scale, origin, floaters) as warn; finished, or no
// stage, grades them fail. --class: a budgets.json class; without one, world scale and the triangle budget are
// UNMEASURED and the card cannot pass.
// Exit: 0 pass or warn; 1 fail; 3 incomplete (something unmeasured); 2 usage or error.
import { mkdirSync, readFileSync, writeFileSync } from 'node:fs';
import { dirname, join, resolve } from 'node:path';
import { extract, HERE } from './blender.mjs';
import { classBudget, exitCodeFor, scoreMesh } from './gate-lib.mjs';

export function meshCard(file, { stage, className, budgetsPath = join(HERE, 'budgets.json') } = {}) {
  const budgets = JSON.parse(readFileSync(budgetsPath, 'utf8'));
  const cls = className ? classBudget(budgets, className) : null;
  const { data, secs } = extract('mesh_metrics.py', resolve(file));
  const comps = Object.fromEntries(Object.entries(budgets.components ?? {}).filter(([k]) => !k.startsWith('_')));
  const card = scoreMesh(data, { stage, cls, className }, comps);
  return { ...card, at: new Date().toISOString(), secs, metrics: data };
}

const isMain = process.argv[1] && resolve(process.argv[1]) === resolve(HERE, 'mesh-card.mjs');
if (isMain) {
  const argv = process.argv.slice(2);
  const opt = (name) => {
    const i = argv.indexOf(`--${name}`);
    return i >= 0 && i + 1 < argv.length ? argv[i + 1] : undefined;
  };
  const file = argv.find((a, i) => !a.startsWith('--') && !argv[i - 1]?.startsWith('--'));
  const out = opt('out');
  if (!file || !out) {
    console.error('usage: mesh-card.mjs <mesh.glb|fbx|blend> --out <card.json> [--stage raw|finished] [--class <name>] [--budgets <json>]');
    process.exit(2);
  }
  try {
    const card = meshCard(file, { stage: opt('stage'), className: opt('class'), budgetsPath: opt('budgets') });
    mkdirSync(dirname(resolve(out)), { recursive: true });
    writeFileSync(out, JSON.stringify(card, null, 1) + '\n');
    const m = card.metrics;
    console.log(`mesh-card ${card.verdict} (stage ${card.stage}, score ${card.score}): ${m.tris} tris, ${m.components} components, height ${m.heightM} m; ${card.findings.map((f) => `${f.code}[${f.severity}]`).join(', ') || 'no findings'}${card.unmeasured.length ? `; unmeasured: ${card.unmeasured.join(', ')}` : ''}`);
    process.exit(exitCodeFor(card.verdict));
  } catch (e) {
    console.error(e.message);
    process.exit(2);
  }
}
