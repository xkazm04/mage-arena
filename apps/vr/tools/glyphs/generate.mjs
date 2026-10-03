// Canonical sigils from the normal templates, tier runes, and palette textures.
// Node built-ins only. Two runs write the same bytes: fixed geometry, fixed raster,
// PNG chunks IHDR/IDAT/IEND with zlib level 9 and no timestamps.
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { loadClip, projectRightIndex, projectMudra, fitCircle } from './stroke.mjs';
import { rasterMask, widen, colorize, maskRgba, encodePng } from './paint.mjs';

const HERE = path.dirname(fileURLToPath(import.meta.url));
const ROOT = path.resolve(HERE, '../..');
const TEMPLATE_DIR = path.join(ROOT, 'Game', 'Clips', 'templates');
const MUDRA_FILE = path.join(ROOT, 'Game', 'Clips', 'mudra.normal.jsonl');
const PROMPTS = path.join(ROOT, 'art', 'style-studies', 'prompts.json');
const OUT = path.join(ROOT, 'art', 'glyphs');

const CX = 128;
const CY = 128;
const RADIUS = 78;
const STROKE = 14;
const DOT_R = 11;
const SIZES = [256, 512, 1024];
const CLASSES = ['sigil-line1', 'sigil-line2', 'sigil-line3'];
const SNAP_LIMIT = (12 * Math.PI) / 180;
const SPELL_DIRECTIONS = [0, Math.PI / 4, Math.PI / 2, Math.PI, -Math.PI / 4, -Math.PI / 2, (-3 * Math.PI) / 4];

const COPY = {
  'sigil-line1': 'Circle, counterclockwise from the bottom, then a vertical bar drawn upward.',
  'sigil-line2': 'The same circle, then a horizontal bar drawn from left to right.',
  'sigil-line3': 'The same circle, then a diagonal bar drawn from lower left to upper right.',
  'sigil-mudra': 'Both index tips move from rest into the seal. Left hand first, then the right. The pose is not a $Q class.',
  'rune-tier1': 'One dot inside a ring.',
  'rune-tier2': 'Two dots inside a ring, left and right.',
  'rune-tier3': 'Three dots inside a ring, one above and two below.',
  'rune-tier4': 'A closed ring and a four-point star: four inward arcs, a dot on each point, and a centre dot.',
};

function fmt(n) {
  if (!Number.isFinite(n)) {
    throw new Error(`non-finite coordinate ${n}`);
  }
  let text = n.toFixed(4);
  text = text.replace(/(\.\d*?)0+$/, '$1').replace(/\.$/, '');
  if (text === '-0') {
    return '0';
  }
  return text;
}

function esc(text) {
  return text.replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;');
}

function unitToSvg(point) {
  return { x: CX + point.x * RADIUS, y: CY - point.y * RADIUS };
}

function parsePalette(text) {
  const out = {};
  for (const part of text.split(',')) {
    const match = part.trim().match(/^(.*?)\s+(#[0-9A-Fa-f]{6})$/);
    if (!match) {
      throw new Error(`bad palette entry: ${part}`);
    }
    out[match[1].trim()] = match[2].toUpperCase();
  }
  return out;
}

function loadStyles() {
  const prompts = JSON.parse(fs.readFileSync(PROMPTS, 'utf8'));
  const wanted = [
    ['13-screen-print', ['teal', 'sunset orange', 'deep plum', 'mustard']],
    ['25-moonlit-silver', ['moon silver', 'slate', 'night blue', 'water']],
    ['30-chalk-slate', ['slate', 'chalk white', 'chalk blue', 'chalk red', 'chalk yellow']],
  ];
  return wanted.map(([id, keys]) => {
    const style = prompts.styles.find((item) => item.id === id);
    if (!style) {
      throw new Error(`prompts.json has no style ${id}`);
    }
    const palette = parsePalette(style.palette);
    for (const key of keys) {
      if (!palette[key]) {
        throw new Error(`${id} palette has no ${key}`);
      }
    }
    return { id, name: style.name, palette };
  });
}

function circularMean(angles) {
  let sin = 0;
  let cos = 0;
  for (const angle of angles) {
    sin += Math.sin(angle);
    cos += Math.cos(angle);
  }
  return Math.atan2(sin, cos);
}

function snapDirection(mean) {
  let best = mean;
  let bestDelta = Infinity;
  for (const candidate of SPELL_DIRECTIONS) {
    const delta = Math.abs(Math.atan2(Math.sin(mean - candidate), Math.cos(mean - candidate)));
    if (delta < bestDelta) {
      bestDelta = delta;
      best = candidate;
    }
  }
  if (bestDelta <= SNAP_LIMIT) {
    return { angle: best, snapped: true, delta: bestDelta };
  }
  return { angle: mean, snapped: false, delta: bestDelta };
}

function circlePath() {
  const r = fmt(RADIUS);
  const x = fmt(CX);
  const top = fmt(CY - RADIUS);
  const bottom = fmt(CY + RADIUS);
  return `M ${x} ${bottom} A ${r} ${r} 0 0 0 ${x} ${top} A ${r} ${r} 0 0 0 ${x} ${bottom}`;
}

function linePath(a, b) {
  return `M ${fmt(a.x)} ${fmt(a.y)} L ${fmt(b.x)} ${fmt(b.y)}`;
}

function polyPath(points) {
  return points.map((point, index) => `${index === 0 ? 'M' : 'L'} ${fmt(point.x)} ${fmt(point.y)}`).join(' ');
}

function arrowPolygon(tip, tangent, size) {
  const length = Math.hypot(tangent.x, tangent.y) || 1;
  const tx = tangent.x / length;
  const ty = tangent.y / length;
  const px = -ty;
  const py = tx;
  const backX = tip.x - tx * size;
  const backY = tip.y - ty * size;
  const wing = size * 0.55;
  return [
    { x: backX + px * wing, y: backY + py * wing },
    tip,
    { x: backX - px * wing, y: backY - py * wing },
  ];
}

function polygonAttr(points) {
  return points.map((point) => `${fmt(point.x)},${fmt(point.y)}`).join(' ');
}

function svgDocument(body) {
  return `<?xml version="1.0" encoding="UTF-8"?>\n<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 256 256">\n${body}</svg>\n`;
}

function strokeGroup(paths) {
  const tags = paths.map((pathD, index) => `    <path id="stroke-${index}" data-stroke="${index}" d="${pathD}"/>`).join('\n');
  return `  <g id="strokes" fill="none" stroke="#ffffff" stroke-width="${STROKE}" stroke-linecap="round" stroke-linejoin="round">\n${tags}\n  </g>`;
}

function marksGroup(dots, arrows) {
  const circles = dots.map((dot, index) => `    <circle id="start-${index}" cx="${fmt(dot.x)}" cy="${fmt(dot.y)}" r="${fmt(dot.r)}"/>`).join('\n');
  const polygons = arrows.map((points, index) => `    <polygon id="arrow-${index}" points="${polygonAttr(points)}"/>`).join('\n');
  return `  <g id="marks" fill="#ffffff" stroke="none">\n${circles}${polygons ? `\n${polygons}` : ''}\n  </g>`;
}

function loadNormalTemplates() {
  const names = fs.readdirSync(TEMPLATE_DIR).filter((name) => name.includes('.normal.') && name.endsWith('.jsonl')).sort();
  const grouped = Object.fromEntries(CLASSES.map((label) => [label, { two: [], one: [] }]));
  for (const name of names) {
    const label = name.split('.')[0];
    if (!grouped[label]) {
      continue;
    }
    const clip = loadClip(path.join(TEMPLATE_DIR, name));
    const strokes = projectRightIndex(clip.frames);
    if (!strokes || strokes.length === 0) {
      throw new Error(`no pen-down strokes in ${name}`);
    }
    const bucket = strokes.length >= 2 ? 'two' : 'one';
    grouped[label][bucket].push({ name, strokes });
  }
  return grouped;
}

function buildLineSigil(label, grouped) {
  const two = grouped.two;
  if (two.length === 0) {
    throw new Error(`${label} has no two-stroke normal template`);
  }
  const lengths = [];
  const angles = [];
  const mids = [];
  let rmsSum = 0;
  let radiusSum = 0;
  for (const item of two) {
    const circle = fitCircle(item.strokes[0]);
    if (!circle) {
      throw new Error(`circle fit failed for ${item.name}`);
    }
    rmsSum += circle.rms;
    radiusSum += circle.r;
    const start = item.strokes[1][0];
    const end = item.strokes[1][item.strokes[1].length - 1];
    const ax = (start.x - circle.cx) / circle.r;
    const ay = (start.y - circle.cy) / circle.r;
    const bx = (end.x - circle.cx) / circle.r;
    const by = (end.y - circle.cy) / circle.r;
    lengths.push(Math.hypot(bx - ax, by - ay));
    angles.push(Math.atan2(by - ay, bx - ax));
    mids.push({ x: (ax + bx) / 2, y: (ay + by) / 2 });
  }
  const meanRadius = radiusSum / two.length;
  const meanRms = rmsSum / two.length;
  if (meanRms / meanRadius > 0.05) {
    throw new Error(`${label} circle rms ${meanRms} is too large to replace with a circle`);
  }
  const length = lengths.reduce((sum, value) => sum + value, 0) / lengths.length;
  const mid = {
    x: mids.reduce((sum, point) => sum + point.x, 0) / mids.length,
    y: mids.reduce((sum, point) => sum + point.y, 0) / mids.length,
  };
  const centered = Math.hypot(mid.x, mid.y) <= 0.04;
  const origin = centered ? { x: 0, y: 0 } : mid;
  const meanAngle = circularMean(angles);
  const snapped = snapDirection(meanAngle);
  const half = length / 2;
  const dirX = Math.cos(snapped.angle);
  const dirY = Math.sin(snapped.angle);
  const barStart = unitToSvg({ x: origin.x - dirX * half, y: origin.y - dirY * half });
  const barEnd = unitToSvg({ x: origin.x + dirX * half, y: origin.y + dirY * half });
  const start = unitToSvg({ x: 0, y: -1 });
  const anchor = two[0];
  const anchorCircle = fitCircle(anchor.strokes[0]);
  const overlay = anchor.strokes.map((stroke) => stroke.map((point) => unitToSvg({
    x: (point.x - anchorCircle.cx) / anchorCircle.r,
    y: (point.y - anchorCircle.cy) / anchorCircle.r,
  })));
  const degrees = (radians) => (radians * 180) / Math.PI;
  return {
    id: label,
    title: label,
    blurb: COPY[label],
    stats: `${two.length} two-stroke normals averaged, ${grouped.one.length} continuous normals not mixed in. Circle rms ${meanRms.toFixed(3)} cm on a ${meanRadius.toFixed(2)} cm radius. Bar length ${length.toFixed(3)} radii, mean angle ${degrees(meanAngle).toFixed(2)}°, drawn at ${degrees(snapped.angle).toFixed(2)}°${snapped.snapped ? ' (snapped to the spell axis)' : ''}.`,
    paths: [circlePath(), linePath(barStart, barEnd)],
    dots: [{ x: start.x, y: start.y, r: DOT_R }],
    arrows: [
      arrowPolygon(unitToSvg({ x: 1.36, y: 0 }), { x: 0, y: -1 }, 18),
      arrowPolygon(
        unitToSvg({ x: origin.x + dirX * (half + 0.14), y: origin.y + dirY * (half + 0.14) }),
        { x: dirX, y: -dirY },
        16,
      ),
    ],
    overlay,
    drawables: [
      { type: 'ring', cx: CX, cy: CY, r: RADIUS, width: STROKE },
      { type: 'segment', x1: barStart.x, y1: barStart.y, x2: barEnd.x, y2: barEnd.y, width: STROKE },
      { type: 'disk', cx: start.x, cy: start.y, r: DOT_R },
    ],
  };
}

function resample(points, count) {
  const cumulative = [0];
  for (let index = 1; index < points.length; index++) {
    cumulative.push(cumulative[index - 1] + Math.hypot(points[index].x - points[index - 1].x, points[index].y - points[index - 1].y));
  }
  const total = cumulative[cumulative.length - 1];
  if (total < 1.0e-8) {
    return Array.from({ length: count }, () => ({ x: points[0].x, y: points[0].y }));
  }
  const out = [];
  let seg = 1;
  for (let k = 0; k < count; k++) {
    const distance = (total * k) / (count - 1);
    while (seg < cumulative.length - 1 && cumulative[seg] < distance) {
      seg += 1;
    }
    const span = cumulative[seg] - cumulative[seg - 1];
    const t = span > 1.0e-12 ? (distance - cumulative[seg - 1]) / span : 0;
    out.push({
      x: points[seg - 1].x + t * (points[seg].x - points[seg - 1].x),
      y: points[seg - 1].y + t * (points[seg].y - points[seg - 1].y),
    });
  }
  return out;
}

function frameOf(strokes, margin) {
  let minX = Infinity;
  let minY = Infinity;
  let maxX = -Infinity;
  let maxY = -Infinity;
  for (const stroke of strokes) {
    for (const point of stroke) {
      if (point.x < minX) minX = point.x;
      if (point.y < minY) minY = point.y;
      if (point.x > maxX) maxX = point.x;
      if (point.y > maxY) maxY = point.y;
    }
  }
  const span = Math.max(maxX - minX, maxY - minY, 1.0e-6);
  const scale = (256 - 2 * margin) / span;
  const mx = (minX + maxX) / 2;
  const my = (minY + maxY) / 2;
  return (point) => ({ x: CX + (point.x - mx) * scale, y: CY - (point.y - my) * scale });
}

function buildMudra() {
  const clip = loadClip(MUDRA_FILE);
  if (clip.header.variant !== 'normal' || clip.header.action !== 'mudra') {
    throw new Error('mudra.normal.jsonl is not the normal mudra clip');
  }
  const projected = projectMudra(clip.frames);
  if (!projected) {
    throw new Error('mudra projection failed');
  }
  const map = frameOf([projected.left, projected.right], 46);
  const left = resample(projected.left, 64).map(map);
  const right = resample(projected.right, 64).map(map);
  const overlay = [projected.left.map(map), projected.right.map(map)];
  const tangent = (stroke) => {
    const a = stroke[stroke.length - 2];
    const b = stroke[stroke.length - 1];
    return { x: b.x - a.x, y: b.y - a.y };
  };
  const endTip = (stroke) => {
    const a = stroke[stroke.length - 2];
    const b = stroke[stroke.length - 1];
    const len = Math.hypot(b.x - a.x, b.y - a.y) || 1;
    return { x: b.x + ((b.x - a.x) / len) * 8, y: b.y + ((b.y - a.y) / len) * 8 };
  };
  return {
    id: 'sigil-mudra',
    title: 'sigil-mudra',
    blurb: COPY['sigil-mudra'],
    stats: `Projected from ${path.basename(MUDRA_FILE)} with the stroke builder's plane. ${projected.left.length} left samples and ${projected.right.length} right samples after the hold is trimmed to one endpoint, then resampled to 64 points.`,
    paths: [polyPath(left), polyPath(right)],
    dots: [
      { x: left[0].x, y: left[0].y, r: DOT_R },
      { x: right[0].x, y: right[0].y, r: DOT_R },
    ],
    arrows: [arrowPolygon(endTip(left), tangent(left), 16), arrowPolygon(endTip(right), tangent(right), 16)],
    overlay,
    drawables: [
      { type: 'polyline', points: left, width: STROKE },
      { type: 'polyline', points: right, width: STROKE },
      { type: 'disk', cx: left[0].x, cy: left[0].y, r: DOT_R },
      { type: 'disk', cx: right[0].x, cy: right[0].y, r: DOT_R },
    ],
  };
}

/** Arc from a to b bowed toward the glyph centre by sagitta pixels. */
function bowArc(a, b, sagitta, steps) {
  const mx = (a.x + b.x) / 2;
  const my = (a.y + b.y) / 2;
  const dx = b.x - a.x;
  const dy = b.y - a.y;
  const chord = Math.hypot(dx, dy);
  let px = -dy / chord;
  let py = dx / chord;
  if (px * (CX - mx) + py * (CY - my) < 0) {
    px = -px;
    py = -py;
  }
  const radius = (sagitta * sagitta + (chord / 2) * (chord / 2)) / (2 * sagitta);
  const centerDist = radius - sagitta;
  const ccx = mx - px * centerDist;
  const ccy = my - py * centerDist;
  const a0 = Math.atan2(a.y - ccy, a.x - ccx);
  let delta = Math.atan2(b.y - ccy, b.x - ccx) - a0;
  while (delta > Math.PI) delta -= Math.PI * 2;
  while (delta < -Math.PI) delta += Math.PI * 2;
  const mid = a0 + delta / 2;
  const inward = (ccx + Math.cos(mid) * radius - mx) * px + (ccy + Math.sin(mid) * radius - my) * py;
  if (inward <= 0) {
    delta += delta > 0 ? -Math.PI * 2 : Math.PI * 2;
  }
  const points = [];
  for (let step = 0; step <= steps; step++) {
    const theta = a0 + delta * (step / steps);
    points.push({ x: ccx + radius * Math.cos(theta), y: ccy + radius * Math.sin(theta) });
  }
  return points;
}

function ring() {
  return `<circle cx="${CX}" cy="${CY}" r="${RADIUS}" fill="none" stroke="#ffffff" stroke-width="${STROKE}"/>`;
}

function disk(x, y, r) {
  return `<circle cx="${fmt(x)}" cy="${fmt(y)}" r="${fmt(r)}" fill="#ffffff"/>`;
}

function buildRunes() {
  const dot = 10;
  const tier1 = {
    id: 'rune-tier1',
    parts: [ring(), disk(CX, CY, dot)],
    drawables: [
      { type: 'ring', cx: CX, cy: CY, r: RADIUS, width: STROKE },
      { type: 'disk', cx: CX, cy: CY, r: dot },
    ],
  };
  const pair = 32;
  const tier2 = {
    id: 'rune-tier2',
    parts: [ring(), disk(CX - pair, CY, dot), disk(CX + pair, CY, dot)],
    drawables: [
      { type: 'ring', cx: CX, cy: CY, r: RADIUS, width: STROKE },
      { type: 'disk', cx: CX - pair, cy: CY, r: dot },
      { type: 'disk', cx: CX + pair, cy: CY, r: dot },
    ],
  };
  const orbit = 34;
  const tri = [90, 210, 330].map((deg) => {
    const rad = (deg * Math.PI) / 180;
    return { x: CX + Math.cos(rad) * orbit, y: CY - Math.sin(rad) * orbit };
  });
  const tier3 = {
    id: 'rune-tier3',
    parts: [ring(), ...tri.map((point) => disk(point.x, point.y, dot))],
    drawables: [
      { type: 'ring', cx: CX, cy: CY, r: RADIUS, width: STROKE },
      ...tri.map((point) => ({ type: 'disk', cx: point.x, cy: point.y, r: dot })),
    ],
  };
  const tipAt = 58;
  const tips = [
    { x: CX, y: CY - tipAt },
    { x: CX + tipAt, y: CY },
    { x: CX, y: CY + tipAt },
    { x: CX - tipAt, y: CY },
  ];
  const bows = [0, 1, 2, 3].map((index) => bowArc(tips[index], tips[(index + 1) % 4], 18, 12));
  const tier4 = {
    id: 'rune-tier4',
    parts: [
      ring(),
      ...bows.map((points) => `<path d="${polyPath(points)}" fill="none" stroke="#ffffff" stroke-width="10" stroke-linecap="round" stroke-linejoin="round"/>`),
      disk(CX, CY, 6),
      ...tips.map((point) => disk(point.x, point.y, 8)),
    ],
    drawables: [
      { type: 'ring', cx: CX, cy: CY, r: RADIUS, width: STROKE },
      ...bows.map((points) => ({ type: 'polyline', points, width: 10 })),
      { type: 'disk', cx: CX, cy: CY, r: 6 },
      ...tips.map((point) => ({ type: 'disk', cx: point.x, cy: point.y, r: 8 })),
    ],
  };
  return [tier1, tier2, tier3, tier4].map((rune) => ({
    id: rune.id,
    title: rune.id,
    blurb: COPY[rune.id],
    stats: 'Geometric mark. Circles, arcs, and dots only, so the mark is not a letter or a digit.',
    svgBody: `  <g id="rune">\n    ${rune.parts.join('\n    ')}\n  </g>`,
    drawables: rune.drawables,
    paths: null,
    overlay: null,
  }));
}

function writeSvg(glyph, guide) {
  if (glyph.svgBody && !guide) {
    fs.writeFileSync(path.join(OUT, `${glyph.id}.svg`), svgDocument(glyph.svgBody), 'utf8');
    return;
  }
  if (!glyph.paths) {
    return;
  }
  const body = `${strokeGroup(glyph.paths)}\n${marksGroup(glyph.dots, guide ? glyph.arrows : [])}`;
  const name = guide ? `${glyph.id}-guide.svg` : `${glyph.id}.svg`;
  fs.writeFileSync(path.join(OUT, name), svgDocument(body), 'utf8');
}

function writeTextures(glyph, styles) {
  const folders = ['mask', ...styles.map((style) => style.id)];
  for (const folder of folders) {
    fs.mkdirSync(path.join(OUT, folder), { recursive: true });
  }
  let maxBytes = 0;
  let maxName = '';
  for (const size of SIZES) {
    const core = rasterMask(glyph.drawables, size);
    const wide = rasterMask(widen(glyph.drawables, 1.72), size);
    const maskPath = path.join(OUT, 'mask', `${glyph.id}-${size}.png`);
    const maskPng = encodePng(size, size, maskRgba(core, size));
    fs.writeFileSync(maskPath, maskPng);
    if (maskPng.length > maxBytes) {
      maxBytes = maskPng.length;
      maxName = path.relative(OUT, maskPath);
    }
    for (const style of styles) {
      const png = encodePng(size, size, colorize(core, wide, size, style.id, style.palette));
      const file = path.join(OUT, style.id, `${glyph.id}-${size}.png`);
      fs.writeFileSync(file, png);
      if (png.length > maxBytes) {
        maxBytes = png.length;
        maxName = path.relative(OUT, file);
      }
    }
  }
  return { maxBytes, maxName };
}

function comparisonSvg(glyph) {
  if (!glyph.paths) {
    return '';
  }
  const paths = glyph.paths.map((d) => `<path class="ink" d="${d}"/>`).join('');
  const dots = glyph.dots.map((dot) => `<circle class="ink" cx="${fmt(dot.x)}" cy="${fmt(dot.y)}" r="${fmt(dot.r)}"/>`).join('');
  const overlay = (glyph.overlay || []).map((points) => {
    const attr = points.map((point) => `${fmt(point.x)},${fmt(point.y)}`).join(' ');
    return `<polyline class="overlay" points="${attr}"/>`;
  }).join('');
  return `<svg viewBox="0 0 256 256" role="img" aria-label="${esc(glyph.title)} with template overlay">${paths}${dots}${overlay}</svg>`;
}

function textureBlock(glyph, styles) {
  const columns = [
    { id: 'mask', name: 'White mask' },
    ...styles.map((style) => ({ id: style.id, name: style.name })),
  ];
  const cells = columns.map((column) => {
    const src = `${column.id}/${glyph.id}-256.png`;
    const lightImg = column.id === 'mask'
      ? `<div class="chip"><img src="${src}" alt="${esc(glyph.title)} mask on a dark chip"></div>`
      : `<img src="${src}" alt="${esc(glyph.title)} in ${esc(column.name)}">`;
    return `<div class="cell">
      <h3>${esc(column.name)}</h3>
      <div class="swatch dark"><img src="${src}" alt=""></div>
      <div class="swatch light">${lightImg}</div>
      <p class="links"><a href="${column.id}/${glyph.id}-512.png">512</a> <a href="${column.id}/${glyph.id}-1024.png">1024</a></p>
    </div>`;
  }).join('\n');
  return `<div class="grid">${cells}</div>`;
}

function page(sigils, runes, styles) {
  const chips = styles.map((style) => {
    const swatches = Object.entries(style.palette).map(([name, hex]) => (
      `<span class="palette"><i style="background:${hex}"></i>${esc(name)} <span class="hex">${hex}</span></span>`
    )).join('');
    return `<div class="style-col"><div class="style-name">${esc(style.name)}</div><div class="style-id">${esc(style.id)}</div><div class="swatches">${swatches}</div></div>`;
  }).join('');
  const sigilBlocks = sigils.map((glyph) => `<section>
    <h2>${esc(glyph.title)}</h2>
    <p>${esc(glyph.blurb)}</p>
    <p class="stats">${esc(glyph.stats)}</p>
    <div class="plates">
      <figure class="plate dark">${comparisonSvg(glyph)}<figcaption>Dark ground</figcaption></figure>
      <figure class="plate light">${comparisonSvg(glyph)}<figcaption>Light ground</figcaption></figure>
      <figure class="plate dark"><img src="${glyph.id}.svg" alt="${esc(glyph.title)} file"><figcaption>SVG file</figcaption></figure>
      <figure class="plate dark"><img src="${glyph.id}-guide.svg" alt="${esc(glyph.title)} guide"><figcaption>Guide, dark ground</figcaption></figure>
      <figure class="plate light"><div class="chip"><img src="${glyph.id}-guide.svg" alt=""></div><figcaption>Guide, light ground</figcaption></figure>
    </div>
    <p class="legend">The thin red line is the projected anchor template (or, for the mudra, the unsmoothed clip). Ink is the canonical stroke.</p>
    ${textureBlock(glyph, styles)}
  </section>`).join('\n');
  const runeBlocks = runes.map((glyph) => `<section>
    <h2>${esc(glyph.title)}</h2>
    <p>${esc(glyph.blurb)} ${esc(glyph.stats)}</p>
    <div class="plates">
      <figure class="plate dark"><img src="${glyph.id}.svg" alt="${esc(glyph.title)}"><figcaption>Dark ground</figcaption></figure>
      <figure class="plate light"><div class="chip"><img src="${glyph.id}.svg" alt=""></div><figcaption>Light ground, dark chip (the file is white ink)</figcaption></figure>
    </div>
    ${textureBlock(glyph, styles)}
  </section>`).join('\n');
  return `<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Sigil glyphs and tier runes</title>
  <style>
    :root { color-scheme: dark; }
    * { box-sizing: border-box; }
    body { margin: 0; background: #141414; color: #eceae4; font: 18px/1.45 "Segoe UI", system-ui, sans-serif; }
    header, main { padding: 28px 32px; max-width: 78rem; }
    h1 { font-size: 32px; font-weight: 650; margin: 0 0 12px; letter-spacing: -0.02em; }
    h2 { font-size: 24px; font-weight: 650; margin: 36px 0 8px; }
    h3 { font-size: 16px; font-weight: 650; margin: 0 0 8px; }
    p { margin: 0 0 10px; }
    a { color: #9fd0c8; }
    .lede, .stats, .legend { max-width: 70ch; color: #d4d0c8; }
    .styles { display: flex; flex-wrap: wrap; gap: 28px; margin-top: 22px; }
    .style-name { font-size: 20px; font-weight: 650; }
    .style-id { color: #b7b3aa; font-size: 16px; }
    .swatches { display: flex; flex-wrap: wrap; gap: 8px 12px; margin-top: 8px; max-width: 24rem; }
    .palette { display: inline-flex; align-items: center; gap: 6px; font-size: 16px; }
    .palette i { width: 22px; height: 22px; border-radius: 4px; border: 1px solid rgba(255,255,255,0.35); display: inline-block; }
    .hex { color: #b7b3aa; }
    .plates { display: flex; flex-wrap: wrap; gap: 16px; margin: 14px 0; }
    .plate { margin: 0; padding: 12px; border-radius: 10px; }
    .plate.dark { background: #121212; }
    .plate.light { background: #f4f0e6; color: #1c1c1c; }
    .plate svg, .plate img { width: 220px; height: 220px; display: block; }
    .plate figcaption { margin-top: 8px; font-size: 16px; }
    .plate.dark svg path.ink { fill: none; stroke: #ffffff; stroke-width: 14; stroke-linecap: round; stroke-linejoin: round; }
    .plate.light svg path.ink { fill: none; stroke: #1c1c1c; stroke-width: 14; stroke-linecap: round; stroke-linejoin: round; }
    .plate.dark svg circle.ink { fill: #ffffff; stroke: none; }
    .plate.light svg circle.ink { fill: #1c1c1c; stroke: none; }
    .overlay { fill: none; stroke: #e23b4a; stroke-width: 1.5; stroke-linejoin: round; stroke-linecap: round; }
    .grid { display: grid; grid-template-columns: repeat(4, minmax(150px, 1fr)); gap: 16px; margin-top: 8px; }
    .swatch { border-radius: 8px; padding: 8px; margin-top: 8px; }
    .swatch.dark { background: #121212; }
    .swatch.light { background: #f4f0e6; }
    .swatch img, .chip img { width: 100%; height: auto; display: block; }
    .chip { background: #121212; border-radius: 8px; padding: 8px; }
    .links { font-size: 16px; margin-top: 6px; }
    @media (max-width: 800px) {
      header, main { padding: 18px 16px; }
      .grid { grid-template-columns: repeat(2, minmax(120px, 1fr)); }
      .plate svg, .plate img { width: 160px; height: 160px; }
    }
  </style>
</head>
<body>
  <header>
    <h1>Sigil glyphs and tier runes</h1>
    <p class="lede">Canonical strokes from the normal templates, and four geometric cuff runes. Masks are white on transparency for tinting. Each shortlisted palette has a coloured version. The 512 and 1024 files are the same drawing. This sheet opens from disk.</p>
    <div class="styles">${chips}</div>
  </header>
  <main>
    <h2>Sigils</h2>
    ${sigilBlocks}
    <h2>Tier runes</h2>
    ${runeBlocks}
  </main>
</body>
</html>
`;
}

function main() {
  const styles = loadStyles();
  const grouped = loadNormalTemplates();
  const sigils = CLASSES.map((label) => buildLineSigil(label, grouped[label]));
  sigils.push(buildMudra());
  const runes = buildRunes();
  fs.rmSync(OUT, { recursive: true, force: true });
  fs.mkdirSync(OUT, { recursive: true });
  for (const glyph of sigils) {
    writeSvg(glyph, false);
    writeSvg(glyph, true);
  }
  for (const rune of runes) {
    writeSvg(rune, false);
  }
  let maxBytes = 0;
  let maxName = '';
  for (const glyph of [...sigils, ...runes]) {
    const sized = writeTextures(glyph, styles);
    if (sized.maxBytes > maxBytes) {
      maxBytes = sized.maxBytes;
      maxName = sized.maxName;
    }
  }
  fs.writeFileSync(path.join(OUT, 'index.html'), page(sigils, runes, styles), 'utf8');
  for (const glyph of sigils) {
    if (glyph.stats) {
      process.stdout.write(`${glyph.id}: ${glyph.stats}\n`);
    }
  }
  process.stdout.write(`largest png ${maxName} ${maxBytes} bytes\n`);
  process.stdout.write(`wrote glyphs to ${OUT}\n`);
}

main();
