// Resample each canonical sigil SVG and classify it with the $Q port.
// A line sigil must be nearer its own template class than either other class.
// The mudra is a pose, so it has no class; its nearest line and distance are printed.
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { loadClip, projectRightIndex } from './stroke.mjs';
import { normalize, cloudDistance, REJECT_DISTANCE } from './qdollar.mjs';
import { samplePath, selfTest } from './svgpath.mjs';

const HERE = path.dirname(fileURLToPath(import.meta.url));
const ROOT = path.resolve(HERE, '../..');
const TEMPLATE_DIR = path.join(ROOT, 'Game', 'Clips', 'templates');
const GLYPH_DIR = path.join(ROOT, 'art', 'glyphs');
const CLASSES = ['sigil-line1', 'sigil-line2', 'sigil-line3'];

function svgStrokes(file) {
  const text = fs.readFileSync(file, 'utf8');
  const found = [];
  const re = /<path\b([^>]*)\/>/g;
  let match = re.exec(text);
  while (match) {
    const tag = match[1];
    const stroke = tag.match(/data-stroke="(\d+)"/);
    if (stroke) {
      const d = tag.match(/\bd="([^"]*)"/);
      if (!d) {
        throw new Error(`stroke path has no d in ${file}`);
      }
      // SVG Y grows downward. The stroke builder's plane Y is world up, so negate Y.
      // Translation is irrelevant: $Q subtracts the centroid before scaling.
      found.push({
        id: Number(stroke[1]),
        points: samplePath(d[1]).map((point) => ({ x: point.x, y: -point.y })),
      });
    }
    match = re.exec(text);
  }
  found.sort((a, b) => a.id - b.id);
  if (found.length === 0) {
    throw new Error(`no data-stroke paths in ${file}`);
  }
  const points = [];
  for (const stroke of found) {
    for (const point of stroke.points) {
      points.push({ x: point.x, y: point.y, strokeId: stroke.id });
    }
  }
  return points;
}

function loadTemplates() {
  const names = fs.readdirSync(TEMPLATE_DIR).filter((name) => name.endsWith('.jsonl')).sort();
  const templates = [];
  const failed = [];
  for (const name of names) {
    const clip = loadClip(path.join(TEMPLATE_DIR, name));
    const strokes = projectRightIndex(clip.frames);
    if (!strokes || strokes.length === 0) {
      failed.push(`${name} (no strokes)`);
      continue;
    }
    const points = [];
    strokes.forEach((stroke, strokeId) => {
      for (const point of stroke) {
        points.push({ x: point.x, y: point.y, strokeId });
      }
    });
    const grid = normalize(points);
    if (!grid) {
      failed.push(`${name} (normalize failed)`);
      continue;
    }
    templates.push({ name, label: name.split('.')[0], grid });
  }
  return { templates, failed };
}

function classify(grid, templates) {
  const byClass = {};
  let bestLabel = null;
  let bestDistance = Infinity;
  let bestIndex = Infinity;
  templates.forEach((template, index) => {
    const distance = cloudDistance(grid, template.grid);
    if (byClass[template.label] === undefined || distance < byClass[template.label]) {
      byClass[template.label] = distance;
    }
    if (distance < bestDistance || (distance === bestDistance && index < bestIndex)) {
      bestDistance = distance;
      bestIndex = index;
      bestLabel = template.label;
    }
  });
  return { byClass, bestLabel, bestDistance };
}

function main() {
  selfTest();
  const { templates, failed } = loadTemplates();
  let maxSelf = 0;
  for (const template of templates) {
    const distance = cloudDistance(template.grid, template.grid);
    if (distance > maxSelf) {
      maxSelf = distance;
    }
  }
  process.stdout.write(`templates ${templates.length}  extract-failed ${failed.length}  self-distance-max ${maxSelf}\n`);
  for (const name of failed) {
    process.stdout.write(`  failed ${name}\n`);
  }
  let ok = failed.length === 0 && maxSelf === 0 && templates.length > 0;
  for (const label of CLASSES) {
    const grid = normalize(svgStrokes(path.join(GLYPH_DIR, `${label}.svg`)));
    if (!grid) {
      process.stdout.write(`${label}  normalize-failed  FAIL\n`);
      ok = false;
      continue;
    }
    const { byClass, bestLabel } = classify(grid, templates);
    const own = byClass[label];
    const second = Math.min(...CLASSES.filter((name) => name !== label).map((name) => byClass[name]));
    const pass = bestLabel === label && own < second;
    if (!pass) {
      ok = false;
    }
    const columns = CLASSES.map((name) => `${name}=${byClass[name]}`).join('  ');
    const reject = own > REJECT_DISTANCE ? 'yes' : 'no';
    process.stdout.write(`${label}  nearest=${bestLabel}  ${columns}  margin=${second - own}  over-reject=${reject}  ${pass ? 'OK' : 'FAIL'}\n`);
  }
  const mudra = normalize(svgStrokes(path.join(GLYPH_DIR, 'sigil-mudra.svg')));
  if (!mudra) {
    process.stdout.write('sigil-mudra  normalize-failed\n');
    ok = false;
  } else {
    const { byClass, bestLabel, bestDistance } = classify(mudra, templates);
    const columns = CLASSES.map((name) => `${name}=${byClass[name]}`).join('  ');
    const reject = bestDistance > REJECT_DISTANCE ? 'yes' : 'no';
    process.stdout.write(`sigil-mudra  no-template-class  nearest-line=${bestLabel}  ${columns}  over-reject=${reject}\n`);
  }
  process.exit(ok ? 0 : 1);
}

main();
