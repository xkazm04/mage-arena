// T63: authors the spell flipbook atlas (apps/vr/art/vfx/spell-flipbook.png) procedurally and deterministically.
// Node built-ins only; no image service, no randomness outside the seeded hash below.
//
// The atlas is 512x512: a 4x4 grid of 128 px cells, one row a school (docs/research/SPELL-FLIPBOOK-T63-2026-10-25.md).
// It is a MASK, not a picture: R = G = B = shade, A = opacity. M_SpellGlow (Tools/CreateBudgetFxCommandlet.cpp) draws
// Emissive = Color x shade and Opacity = A, so a pixel is the threat colour or a darker tone of the same chroma, never a
// brighter or another hue: the colour language stays SpellVfx::GlowColour's. The cell layout is SpellVfx::ECell.
//
//   node apps/vr/tools/vfx/spell-flipbook.mjs --write             writes the PNG and manifest.json
//   node apps/vr/tools/vfx/spell-flipbook.mjs --check             regenerates in memory and compares the pixels with the PNG
//   node apps/vr/tools/vfx/spell-flipbook.mjs --preview <dir>     also writes each cell over the arena sand, per school colour
import { createHash } from 'node:crypto';
import { existsSync, mkdirSync, readFileSync, writeFileSync } from 'node:fs';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';
import { deflateSync, inflateSync } from 'node:zlib';

const here = dirname(fileURLToPath(import.meta.url));
const outDir = join(here, '..', '..', 'art', 'vfx');
const pngPath = join(outDir, 'spell-flipbook.png');
const manifestPath = join(outDir, 'manifest.json');

const SIZE = 512;
const GRID = 4;
const CELL = SIZE / GRID;
// Supersamples per pixel side: the edges are antialiased in the source, so the mips start clean.
const SS = 4;
// Content stays inside this fraction of a half-cell: the cell border is transparent, so a card never shows its square
// and the first mips do not bleed a neighbour in.
const MARGIN = 0.9;

// The cells, row-major. Keep in step with SpellVfx::ECell (Session/SpellVfx.h).
const CELLS = [
  ['water-shell-a', 'water-shell-b', 'water-spray', 'water-crown'],
  ['fire-flame-a', 'fire-flame-b', 'fire-embers', 'fire-crown'],
  ['air-swirl-a', 'air-swirl-b', 'air-wisp', 'air-crown'],
  ['soft-glow', 'droplet-streak', 'spark-streak', 'grit'],
];

// ---- deterministic helpers -------------------------------------------------------------------------------------
function hash01(seed, index) {
  let x = (Math.imul(seed >>> 0, 0x9e3779b1) + Math.imul(index >>> 0, 0x85ebca77)) >>> 0;
  x ^= x >>> 15;
  x = Math.imul(x, 0x2c1b3c6d) >>> 0;
  x ^= x >>> 12;
  x = Math.imul(x, 0x297a2d39) >>> 0;
  x ^= x >>> 15;
  return (x >>> 0) / 4294967295;
}
const clamp01 = (v) => (v < 0 ? 0 : v > 1 ? 1 : v);
const lerp = (a, b, t) => a + (b - a) * t;
function smoothstep(e0, e1, x) {
  const t = clamp01((x - e0) / (e1 - e0));
  return t * t * (3 - 2 * t);
}
// Value noise on an integer lattice, smooth, seeded.
function noise2(seed, x, y) {
  const xi = Math.floor(x);
  const yi = Math.floor(y);
  const fx = x - xi;
  const fy = y - yi;
  const h = (i, j) => hash01(seed, ((i & 1023) << 10) | (j & 1023));
  const u = fx * fx * (3 - 2 * fx);
  const v = fy * fy * (3 - 2 * fy);
  return lerp(lerp(h(xi, yi), h(xi + 1, yi), u), lerp(h(xi, yi + 1), h(xi + 1, yi + 1), u), v);
}
function fbm(seed, x, y) {
  return 0.55 * noise2(seed, x, y) + 0.3 * noise2(seed + 1, x * 2.1, y * 2.1) + 0.15 * noise2(seed + 2, x * 4.3, y * 4.3);
}

// ---- layers -----------------------------------------------------------------------------------------------------
// A layer answers (x, y) in [-1, 1]^2 (y up) with [alpha, shade] or null. Layers stack with "over".
function over(layers) {
  return (x, y) => {
    let a = 0;
    let s = 0;
    for (const layer of layers) {
      const hit = layer(x, y);
      if (!hit || hit[0] <= 0) continue;
      const [la, ls] = hit;
      const na = la + a * (1 - la);
      s = na > 0 ? (ls * la + s * a * (1 - la)) / na : 0;
      a = na;
    }
    return [a, s];
  };
}

// A soft glow: the far hemisphere of a sphere, as T58's M_SpellGlow drew it (opacity (1 - r^2)^0.75).
function glow(radius, gain, shade) {
  return (x, y) => {
    const r = Math.hypot(x, y) / radius;
    if (r >= 1) return null;
    return [gain * Math.pow(1 - r * r, 0.75), shade];
  };
}

// A droplet: a dark rim, a mid body and a bright crest toward the top left, sharp edged (antialiased by supersampling).
function droplet(cx, cy, r, stretch = 1, angle = 0) {
  const c = Math.cos(angle);
  const s = Math.sin(angle);
  return (x, y) => {
    const dx = x - cx;
    const dy = y - cy;
    const u = (dx * c + dy * s) / stretch;
    const v = -dx * s + dy * c;
    const d = Math.hypot(u, v) / r;
    if (d >= 1) return null;
    const crest = Math.hypot(u / r + 0.35, v / r - 0.35);
    const shade = d > 0.72 ? 0.5 : crest < 0.38 ? 1.0 : 0.72;
    return [1, shade];
  };
}

// A tapered stroke along a parametric path p(t), t in [0, 1]: width w0 at t = 0 tapering to w1 at t = 1.
function stroke(path, w0, w1, shadeAt, steps = 48) {
  const pts = [];
  for (let i = 0; i <= steps; i++) pts.push(path(i / steps));
  return (x, y) => {
    let best = Infinity;
    let bestT = 0;
    for (let i = 0; i < steps; i++) {
      const [ax, ay] = pts[i];
      const [bx, by] = pts[i + 1];
      const ex = bx - ax;
      const ey = by - ay;
      const len2 = ex * ex + ey * ey || 1e-9;
      const t = clamp01(((x - ax) * ex + (y - ay) * ey) / len2);
      const d = Math.hypot(x - (ax + ex * t), y - (ay + ey * t));
      if (d < best) {
        best = d;
        bestT = (i + t) / steps;
      }
    }
    const w = lerp(w0, w1, bestT);
    if (w <= 0 || best >= w) return null;
    const edge = best / w;
    return [smoothstep(1, 0.6, edge), shadeAt(bestT, edge)];
  };
}

// A spark: a bright point with a dark ember rim and a faint halo.
function spark(cx, cy, r) {
  return (x, y) => {
    const d = Math.hypot(x - cx, y - cy) / r;
    if (d >= 2.4) return null;
    if (d >= 1) return [0.3 * (1 - (d - 1) / 1.4), 0.55];
    return [1, lerp(1.0, 0.5, smoothstep(0.4, 1.0, d))];
  };
}

// A filled signed-distance shape (negative inside) with a darker rim: shade rises from 0.45 at the edge to 1 deep in.
function sdfLayer(sd, alphaIn = 1, seed = 0, shades = null) {
  return (x, y) => {
    const d = sd(x, y);
    if (d >= 0.012) return null;
    if (shades) {
      // T67: a layer with its own shades [rim, lo, hi], for the nested flame star.
      const depth = -d;
      if (depth < 0.04) return [alphaIn * smoothstep(-0.012, 0.012, depth), lerp(shades[0], shades[1], smoothstep(0.012, 0.04, depth))];
      return [alphaIn * smoothstep(-0.012, 0.012, depth), lerp(shades[1], shades[2], smoothstep(0.35, 0.75, fbm(seed + 11, x * 5.0, y * 3.2 - 0.7)))];
    }
    return [alphaIn * smoothstep(-0.012, 0.012, -d), layeredShade(-d, seed, x, y)];
  };
}

// The shade inside a filled shape: a darker rim band (0.5) along the edge (darker still read as brown on fire), a mid tone, and inside it hotter or brighter
// streaks from noise, so a flame or a sheet of water is layered, not one flat tone. Never above 1.
function layeredShade(depth, seed, x, y) {
  if (depth < 0.045) return lerp(0.5, 0.62, smoothstep(0.015, 0.045, depth));
  const inner = smoothstep(0.045, 0.16, depth);
  const streak = smoothstep(0.35, 0.75, fbm(seed + 11, x * 5.0, y * 3.2 - 0.7));
  return lerp(0.6, lerp(0.72, 1.0, streak), inner);
}

// A splash or flame star: a body of radius r0 with n fingers reaching between rMin and rMax further out.
function burst(seed, r0, n, rMin, rMax, width, alphaIn, shades) {
  const fingers = [];
  for (let i = 0; i < n; i++) {
    const a = ((i + 0.5 * hash01(seed, i + 500)) / n) * 2 * Math.PI;
    const len = lerp(rMin, rMax, hash01(seed, i + 520));
    const bend = (hash01(seed, i + 540) - 0.5) * 0.5;
    const pts = [];
    for (let k = 0; k <= 12; k++) {
      const t = k / 12;
      const r = r0 * 0.8 + len * t;
      pts.push([r * Math.cos(a + bend * t), r * Math.sin(a + bend * t)]);
    }
    fingers.push({ pts, width: width * (0.8 + 0.4 * hash01(seed, i + 560)) });
  }
  return sdfLayer((x, y) => {
    const nx = x + 0.05 * (fbm(seed, x * 4, y * 4) - 0.5) * 2;
    const ny = y + 0.05 * (fbm(seed + 3, x * 4, y * 4) - 0.5) * 2;
    let d = Math.hypot(nx, ny) - r0;
    for (const finger of fingers) {
      const [dist, t] = polylineDistance(finger.pts, nx, ny);
      d = smin(d, dist - finger.width * Math.pow(1 - t, 0.8), 0.06);
    }
    return d;
  }, alphaIn, seed, shades);
}

// ---- cells ------------------------------------------------------------------------------------------------------
// The shells: the bolt's opaque core covers r < 0.42 of the card (SpellVfx::ShellScale 2.4), so their detail lives in
// the ring outside it.
function waterShell(seed) {
  // No thin film: a part-transparent teal over the orange sand mixes to grey. Instead a crisp, noisy sheet of water
  // hugging the core, three curling crests flung off it, and a fine spray of drops in the ring.
  const layers = [
    sdfLayer((x, y) => {
      const r = Math.hypot(x, y);
      const a = Math.atan2(y, x);
      const edge = 0.55 + 0.07 * (fbm(seed, Math.cos(a) * 2 + 3, Math.sin(a) * 2 + 3) - 0.5) * 2 + 0.05 * Math.sin(a * 7 + seed);
      return r - edge;
    }, 1, seed),
  ];
  for (let k = 0; k < 3; k++) {
    const a0 = (k / 3) * 2 * Math.PI + hash01(seed, k) * 0.9;
    const r0 = 0.5 + 0.06 * hash01(seed, k + 10);
    layers.push(stroke((t) => {
      const a = a0 + t * 1.6;
      const r = r0 + 0.24 * t;
      return [r * Math.cos(a), r * Math.sin(a)];
    }, 0.1, 0.0, (t, e) => (e > 0.7 ? 0.55 : t < 0.4 ? 1.0 : 0.8)));
  }
  for (let i = 0; i < 26; i++) {
    const a = hash01(seed, 100 + i) * 2 * Math.PI;
    const r = 0.58 + 0.3 * Math.sqrt(hash01(seed, 200 + i));
    const size = 0.022 + 0.04 * hash01(seed, 300 + i) * (1.1 - r);
    layers.push(droplet(r * Math.cos(a), r * Math.sin(a), size, 1.25, a));
  }
  return over(layers);
}

function waterSpray(seed) {
  // A droplet cluster for the trail: a dense run of fine drops, larger at the head (+x, the flight side).
  const layers = [];
  for (let i = 0; i < 14; i++) {
    const t = hash01(seed, i);
    const x = lerp(-0.75, 0.6, t);
    const y = (hash01(seed, i + 40) - 0.5) * 0.9 * (0.4 + 0.6 * (1 - t));
    const size = lerp(0.035, 0.13, t * t) * (0.7 + 0.5 * hash01(seed, i + 80));
    layers.push(droplet(x, y, size, 1.3, 0));
  }
  return over(layers);
}

function waterCrown(seed) {
  // The absorb splash: a solid crown of water with fingers thrown outward, and drops beyond them.
  const layers = [burst(seed, 0.3, 12, 0.2, 0.42, 0.075, 0.95), ];
  for (let i = 0; i < 22; i++) {
    const a = hash01(seed, 100 + i) * 2 * Math.PI;
    const r = 0.62 + 0.24 * hash01(seed, 200 + i);
    layers.push(droplet(r * Math.cos(a), r * Math.sin(a), 0.025 + 0.03 * hash01(seed, 300 + i), 1.4, a));
  }
  return over(layers);
}

// Distance from (x, y) to a polyline, and the path parameter of the nearest point.
function polylineDistance(pts, x, y) {
  let best = Infinity;
  let bestT = 0;
  const steps = pts.length - 1;
  for (let i = 0; i < steps; i++) {
    const [ax, ay] = pts[i];
    const [bx, by] = pts[i + 1];
    const ex = bx - ax;
    const ey = by - ay;
    const len2 = ex * ex + ey * ey || 1e-9;
    const t = clamp01(((x - ax) * ex + (y - ay) * ey) / len2);
    const d = Math.hypot(x - (ax + ex * t), y - (ay + ey * t));
    if (d < best) {
      best = d;
      bestT = (i + t) / steps;
    }
  }
  return [best, bestT];
}
// Polynomial smooth minimum: the tongues grow out of the body without a crease.
function smin(a, b, k) {
  const h = clamp01(0.5 + (0.5 * (b - a)) / k);
  return lerp(b, a, h) - k * h * (1 - h);
}

// T67: a flame layer, a body round the core with tongues licking up, as one signed distance with a noisy edge. Shade runs
// from rim at the edge, through lo, to hi deep in and low in the layer (heat rises from the base), never above 1.
function flameLayer(seed, o) {
  const tongues = [];
  for (let i = 0; i < o.count; i++) {
    const theta = lerp(-0.12, 1.12, (i + 0.25 + 0.5 * hash01(seed, i)) / o.count) * Math.PI;
    const side = Math.cos(theta);
    const length = (o.length + 0.3 * hash01(seed, i + 10)) * (1 - 0.4 * Math.abs(side)) * o.scale;
    const sway = (hash01(seed, i + 20) - 0.5) * 0.24 * o.scale;
    const bx = o.body * side * o.scale;
    const by = (o.body * Math.sin(theta) - 0.06) * o.scale + o.lift;
    const pts = [];
    for (let k = 0; k <= 20; k++) {
      const t = k / 20;
      // Up and out first, then curling back over the top.
      pts.push([bx + side * 0.12 * t * o.scale + sway * Math.sin(t * Math.PI * 1.3), by + length * t]);
    }
    tongues.push({ pts, width: (o.width + 0.04 * hash01(seed, i + 30)) * o.scale });
  }
  return (x, y) => {
    const nx = x + 0.09 * (fbm(seed, x * 3.1, y * 3.1 - 2.3) - 0.5) * 2;
    const ny = y + 0.06 * (fbm(seed + 7, x * 3.1, y * 3.1) - 0.5) * 2;
    let d = Math.hypot(nx, ny + 0.04 - o.lift) - o.body * 1.15 * o.scale;
    for (const tongue of tongues) {
      const [dist, t] = polylineDistance(tongue.pts, nx, ny);
      d = smin(d, dist - tongue.width * Math.pow(1 - t, 0.9), 0.09 * o.scale);
    }
    if (d >= 0.012 || ny > 0.92) return null;
    const depth = -d;
    const alpha = smoothstep(-0.012, 0.012, depth);
    if (depth < 0.04) return [alpha, lerp(o.rim, o.lo, smoothstep(0.012, 0.04, depth))];
    const inner = smoothstep(0.04, 0.16, depth);
    const streak = smoothstep(0.35, 0.75, fbm(seed + 11, nx * 5.0, ny * 3.2 - 0.7));
    const heat = clamp01((0.5 - ny) * 0.8) * 0.5 + 0.5 * streak;
    return [alpha, lerp(o.lo, lerp(o.lo, o.hi, heat), inner)];
  };
}

function fireFlame(seed) {
  // T67: three nested flames, so the fire has volume. The outer flame is the darker body with a dark rim (outline on the
  // orange sand), a mid flame sits in it and a small hot core at the base is the full element colour (shade 1, never
  // brighter than the threat). The rim of each inner flame is darker than the body round it, so the layers read apart.
  const layers = [
    // T67 round 2: a darker backing flame a hair larger than the body, the outline that separates the fire from the sand.
    flameLayer(seed, { scale: 1.08, count: 10, body: 0.46, length: 0.42, width: 0.1, rim: 0.4, lo: 0.42, hi: 0.46, lift: 0.01 }),
    flameLayer(seed, { scale: 1.0, count: 10, body: 0.46, length: 0.42, width: 0.1, rim: 0.46, lo: 0.52, hi: 0.68, lift: 0 }),
    flameLayer(seed + 100, { scale: 0.72, count: 7, body: 0.46, length: 0.34, width: 0.115, rim: 0.6, lo: 0.74, hi: 0.9, lift: -0.02 }),
    flameLayer(seed + 200, { scale: 0.4, count: 5, body: 0.46, length: 0.3, width: 0.13, rim: 0.86, lo: 0.94, hi: 1.0, lift: -0.08 }),
  ];
  for (let i = 0; i < 9; i++) {
    const a = lerp(0.3, Math.PI - 0.3, hash01(seed, 50 + i));
    const r = 0.72 + 0.14 * hash01(seed, 60 + i);
    layers.push(spark(r * Math.cos(a), Math.min(0.86, r * Math.sin(a) + 0.05), 0.022 + 0.018 * hash01(seed, 70 + i)));
  }
  return over(layers);
}

function fireEmbers(seed) {
  // An ember cluster for the trail: many sparks (T67: eleven) with dark rims and faint halos, bigger ones toward the head.
  const layers = [];
  for (let i = 0; i < 11; i++) {
    const t = hash01(seed, i + 100);
    const x = lerp(-0.7, 0.6, t);
    const y = (hash01(seed, i + 20) - 0.5) * 1.2 * (0.5 + 0.5 * (1 - t));
    layers.push(spark(x, y, 0.05 + 0.09 * hash01(seed, i + 40) * (0.5 + t)));
  }
  return over(layers);
}

function fireCrown(seed) {
  // The absorb burst: a solid flame star (T67: twelve licks, a mid star and a hot heart) with licks thrown outward, and sparks beyond.
  const layers = [
    burst(seed, 0.3, 12, 0.26, 0.52, 0.09, 1.0, [0.46, 0.6, 0.82]),
    burst(seed + 5, 0.2, 9, 0.12, 0.3, 0.075, 1.0, [0.62, 0.8, 0.96]),
    burst(seed + 9, 0.1, 6, 0.02, 0.1, 0.06, 1.0, [0.86, 0.95, 1.0]),
  ];
  for (let i = 0; i < 16; i++) {
    const a = hash01(seed, 100 + i) * 2 * Math.PI;
    const r = 0.66 + 0.2 * hash01(seed, 200 + i);
    layers.push(spark(r * Math.cos(a), r * Math.sin(a), 0.025 + 0.02 * hash01(seed, 300 + i)));
  }
  return over(layers);
}

function airSwirl(seed) {
  // Wisps spiralling round the core, crisp, with air between them (no film: green over the sand mixes to khaki).
  const layers = [];
  for (let k = 0; k < 5; k++) {
    const a0 = (k / 5) * 2 * Math.PI + hash01(seed, k) * 0.6;
    const r0 = 0.46 + 0.1 * hash01(seed, k + 10);
    layers.push(stroke((t) => {
      const a = a0 + t * 2.5;
      const r = r0 + 0.34 * t;
      return [r * Math.cos(a), r * Math.sin(a)];
    }, 0.115, 0.012, (t, e) => (e > 0.65 ? 0.6 : t < 0.35 ? 1.0 : 0.82)));
  }
  return over(layers);
}

function airWisp(seed) {
  // A trail wisp, drawn for a card stretched about 4:1 along x: a wavy stroke, head at +x, tail fading at -x.
  return over([
    stroke((t) => [lerp(0.66, -0.88, t), 0.4 * Math.sin(t * Math.PI * 1.6 + seed)], 0.17, 0.015, (t, e) => (e > 0.6 ? 0.6 : t < 0.4 ? 1.0 : 0.8)),
  ]);
}

function airCrown(seed) {
  // The absorb gust: a tight swirl at the centre unwinding into wisps thrown outward.
  const layers = [];
  for (let k = 0; k < 4; k++) {
    const a0 = (k / 4) * 2 * Math.PI + hash01(seed, k + 40) * 0.5;
    layers.push(stroke((t) => {
      const a = a0 + t * 2.6;
      const r = lerp(0.12, 0.42, t);
      return [r * Math.cos(a), r * Math.sin(a)];
    }, 0.06, 0.1, (t, e) => (e > 0.65 ? 0.6 : 0.95)));
  }
  for (let k = 0; k < 8; k++) {
    const a0 = (k / 8) * 2 * Math.PI + hash01(seed, k) * 0.4;
    layers.push(stroke((t) => {
      const a = a0 + t * 0.9;
      const r = lerp(0.45, 0.86, t);
      return [r * Math.cos(a), r * Math.sin(a)];
    }, 0.08, 0.01, (t, e) => (e > 0.65 ? 0.6 : 0.95)));
  }
  return over(layers);
}

function softGlow() {
  // The unblockable's shell and the fallback: T58's soft falloff, unchanged, so the leave cue reads as it did.
  return glow(MARGIN, 1.0, 1.0);
}

function dropletStreak() {
  // A flung drop (head at +x) with a tapering tail, for the splash and the perfect ring of Water and Earth.
  return over([
    stroke((t) => [lerp(0.4, -0.85, t), 0], 0.3, 0.0, (t, e) => (e > 0.7 ? 0.5 : 0.75)),
    droplet(0.45, 0, 0.36, 1.15, 0),
  ]);
}

function sparkStreak() {
  // A thrown spark (head at +x) with an ember tail, for the splash and the perfect ring of Fire. T67: a wider tail with a
  // darker outer band and a hot lit core, and two trailing sparks, so each bead is a small flame, not a thin line.
  return over([
    stroke((t) => [lerp(0.4, -0.85, t), 0.05 * t], 0.3, 0.0, () => 0.46),
    stroke((t) => [lerp(0.4, -0.8, t), 0.05 * t], 0.2, 0.0, (t, e) => (e > 0.6 ? 0.62 : t < 0.4 ? 1.0 : 0.78)),
    spark(-0.2, 0.08, 0.07),
    spark(-0.5, -0.04, 0.05),
    spark(0.45, 0, 0.3),
  ]);
}

function grit(seed) {
  const layers = [];
  for (let i = 0; i < 9; i++) {
    const cx = (hash01(seed, i) - 0.5) * 1.4;
    const cy = (hash01(seed, i + 20) - 0.5) * 1.4;
    const r = 0.06 + 0.08 * hash01(seed, i + 40);
    const rot = hash01(seed, i + 60) * Math.PI;
    layers.push((x, y) => {
      const dx = x - cx;
      const dy = y - cy;
      const u = Math.abs(dx * Math.cos(rot) + dy * Math.sin(rot));
      const v = Math.abs(-dx * Math.sin(rot) + dy * Math.cos(rot));
      const d = (u + v) / r;
      if (d >= 1) return null;
      return [1, d > 0.7 ? 0.45 : 0.8];
    });
  }
  return over(layers);
}

const BUILDERS = {
  'water-shell-a': () => waterShell(631),
  'water-shell-b': () => waterShell(632),
  'water-spray': () => waterSpray(633),
  'water-crown': () => waterCrown(634),
  'fire-flame-a': () => fireFlame(641),
  'fire-flame-b': () => fireFlame(642),
  'fire-embers': () => fireEmbers(643),
  'fire-crown': () => fireCrown(644),
  'air-swirl-a': () => airSwirl(651),
  'air-swirl-b': () => airSwirl(652),
  'air-wisp': () => airWisp(653),
  'air-crown': () => airCrown(654),
  'soft-glow': () => softGlow(),
  'droplet-streak': () => dropletStreak(),
  'spark-streak': () => sparkStreak(),
  grit: () => grit(661),
};

// ---- rasterise --------------------------------------------------------------------------------------------------
function render() {
  const rgba = Buffer.alloc(SIZE * SIZE * 4);
  for (let row = 0; row < GRID; row++) {
    for (let col = 0; col < GRID; col++) {
      const field = BUILDERS[CELLS[row][col]]();
      for (let py = 0; py < CELL; py++) {
        for (let px = 0; px < CELL; px++) {
          let a = 0;
          let sa = 0;
          for (let sy = 0; sy < SS; sy++) {
            for (let sx = 0; sx < SS; sx++) {
              // Cell coordinates in [-1, 1], y up; content is scaled into the margin.
              const x = ((px + (sx + 0.5) / SS) / CELL * 2 - 1) / MARGIN;
              const y = -((py + (sy + 0.5) / SS) / CELL * 2 - 1) / MARGIN;
              if (Math.abs(x) >= 1 || Math.abs(y) >= 1) continue;
              const hit = field(x, y);
              if (!hit) continue;
              const [ha, hs] = hit;
              a += ha;
              sa += ha * hs;
            }
          }
          a /= SS * SS;
          const shade = a > 0 ? sa / (a * SS * SS) : 0;
          const o = ((row * CELL + py) * SIZE + col * CELL + px) * 4;
          const s8 = Math.round(clamp01(shade) * 255);
          rgba[o] = s8;
          rgba[o + 1] = s8;
          rgba[o + 2] = s8;
          rgba[o + 3] = Math.round(clamp01(a) * 255);
        }
      }
    }
  }
  return rgba;
}

// ---- PNG --------------------------------------------------------------------------------------------------------
const CRC = (() => {
  const t = new Uint32Array(256);
  for (let n = 0; n < 256; n++) {
    let c = n;
    for (let k = 0; k < 8; k++) c = c & 1 ? 0xedb88320 ^ (c >>> 1) : c >>> 1;
    t[n] = c >>> 0;
  }
  return t;
})();
function crc32(buf) {
  let c = 0xffffffff;
  for (const b of buf) c = CRC[(c ^ b) & 0xff] ^ (c >>> 8);
  return (c ^ 0xffffffff) >>> 0;
}
function chunk(type, data) {
  const len = Buffer.alloc(4);
  len.writeUInt32BE(data.length);
  const body = Buffer.concat([Buffer.from(type, 'ascii'), data]);
  const crc = Buffer.alloc(4);
  crc.writeUInt32BE(crc32(body));
  return Buffer.concat([len, body, crc]);
}
function encodePng(rgba, w, h) {
  const ihdr = Buffer.alloc(13);
  ihdr.writeUInt32BE(w, 0);
  ihdr.writeUInt32BE(h, 4);
  ihdr[8] = 8;
  ihdr[9] = 6;
  const raw = Buffer.alloc((w * 4 + 1) * h);
  for (let y = 0; y < h; y++) rgba.copy(raw, y * (w * 4 + 1) + 1, y * w * 4, (y + 1) * w * 4);
  return Buffer.concat([
    Buffer.from('89504e470d0a1a0a', 'hex'),
    chunk('IHDR', ihdr),
    chunk('IDAT', deflateSync(raw, { level: 9 })),
    chunk('IEND', Buffer.alloc(0)),
  ]);
}
// Decodes an 8-bit RGBA, non-interlaced PNG (what encodePng writes, or a re-save of it).
function decodePng(png) {
  if (png.subarray(0, 8).toString('hex') !== '89504e470d0a1a0a') throw new Error('not a PNG (an LFS pointer? run git lfs pull)');
  let off = 8;
  let w = 0;
  let h = 0;
  const idat = [];
  while (off < png.length) {
    const len = png.readUInt32BE(off);
    const type = png.toString('ascii', off + 4, off + 8);
    const data = png.subarray(off + 8, off + 8 + len);
    if (type === 'IHDR') {
      w = data.readUInt32BE(0);
      h = data.readUInt32BE(4);
      if (data[8] !== 8 || data[9] !== 6 || data[12] !== 0) throw new Error('expected 8-bit RGBA, not interlaced');
    } else if (type === 'IDAT') idat.push(data);
    off += 12 + len;
  }
  const raw = inflateSync(Buffer.concat(idat));
  const out = Buffer.alloc(w * h * 4);
  const stride = w * 4;
  for (let y = 0; y < h; y++) {
    const f = raw[y * (stride + 1)];
    for (let x = 0; x < stride; x++) {
      const v = raw[y * (stride + 1) + 1 + x];
      const a = x >= 4 ? out[y * stride + x - 4] : 0;
      const b = y > 0 ? out[(y - 1) * stride + x] : 0;
      const c = x >= 4 && y > 0 ? out[(y - 1) * stride + x - 4] : 0;
      let p = 0;
      if (f === 1) p = a;
      else if (f === 2) p = b;
      else if (f === 3) p = (a + b) >> 1;
      else if (f === 4) {
        const pa = Math.abs(b - c);
        const pb = Math.abs(a - c);
        const pc = Math.abs(a + b - 2 * c);
        p = pa <= pb && pa <= pc ? a : pb <= pc ? b : c;
      }
      out[y * stride + x] = (v + p) & 0xff;
    }
  }
  return { w, h, rgba: out };
}

const sha = (buf) => createHash('sha256').update(buf).digest('hex');

// The border of every cell must be transparent, or a card shows its square.
function borderProblems(rgba) {
  const problems = [];
  const band = Math.floor(CELL * (1 - MARGIN) / 2) - 1;
  for (let row = 0; row < GRID; row++) {
    for (let col = 0; col < GRID; col++) {
      let worst = 0;
      for (let py = 0; py < CELL; py++) {
        for (let px = 0; px < CELL; px++) {
          if (px >= band && px < CELL - band && py >= band && py < CELL - band) continue;
          worst = Math.max(worst, rgba[((row * CELL + py) * SIZE + col * CELL + px) * 4 + 3]);
        }
      }
      if (worst > 0) problems.push(`${CELLS[row][col]}: border alpha ${worst}`);
    }
  }
  return problems;
}

function manifest(rgba) {
  return {
    card: 'T63',
    generator: 'apps/vr/tools/vfx/spell-flipbook.mjs',
    file: 'spell-flipbook.png',
    sizePx: [SIZE, SIZE],
    grid: [GRID, GRID],
    channels: 'R=G=B shade (Emissive = Color x shade), A opacity; linear, not sRGB',
    cells: CELLS.flat().map((name, index) => ({ index, name, col: index % GRID, row: Math.floor(index / GRID) })),
    pixelsSha256: sha(rgba),
  };
}

// Each cell over the arena sand, in each school colour, as the material blends it: for a look before Unreal.
function writePreview(rgba, dir) {
  mkdirSync(dir, { recursive: true });
  const toSrgb = (l) => (l <= 0.0031308 ? 12.92 * l : 1.055 * Math.pow(l, 1 / 2.4) - 0.055);
  const sand = [0.62, 0.38, 0.2];
  const colours = { water: [0.0, 0.62, 0.62], fire: [0.9, 0.26, 0.02], air: [0.2, 0.85, 0.22], rim: [0.8, 0.02, 0.02] };
  for (const [name, colour] of Object.entries(colours)) {
    const out = Buffer.alloc(SIZE * SIZE * 4);
    for (let i = 0; i < SIZE * SIZE; i++) {
      const s = rgba[i * 4] / 255;
      const a = rgba[i * 4 + 3] / 255;
      for (let c = 0; c < 3; c++) out[i * 4 + c] = Math.round(255 * toSrgb(lerp(sand[c], colour[c] * s, a)));
      out[i * 4 + 3] = 255;
    }
    writeFileSync(join(dir, `preview-${name}.png`), encodePng(out, SIZE, SIZE));
  }
}

function main(argv) {
  const mode = argv[0];
  if (mode !== '--write' && mode !== '--check' && mode !== '--preview') {
    console.error('usage: spell-flipbook.mjs --write | --check | --preview <dir>');
    process.exit(2);
  }
  const rgba = render();
  const problems = borderProblems(rgba);
  if (problems.length) {
    console.error(`spell-flipbook: FAIL\n  ${problems.join('\n  ')}`);
    process.exit(1);
  }
  const man = manifest(rgba);
  if (mode === '--preview') {
    writePreview(rgba, argv[1] || 'spell-flipbook-preview');
    writeFileSync(join(argv[1] || 'spell-flipbook-preview', 'spell-flipbook.png'), encodePng(rgba, SIZE, SIZE));
    console.log(`spell-flipbook: preview written to ${argv[1] || 'spell-flipbook-preview'}`);
    return;
  }
  if (mode === '--write') {
    mkdirSync(outDir, { recursive: true });
    writeFileSync(pngPath, encodePng(rgba, SIZE, SIZE));
    writeFileSync(manifestPath, `${JSON.stringify(man, null, 2)}\n`);
    console.log(`spell-flipbook: wrote ${SIZE}x${SIZE}, pixels ${man.pixelsSha256}`);
    return;
  }
  if (!existsSync(pngPath)) {
    console.error(`spell-flipbook: FAIL ${pngPath} missing`);
    process.exit(1);
  }
  const committed = decodePng(readFileSync(pngPath));
  const saved = JSON.parse(readFileSync(manifestPath, 'utf8'));
  const fails = [];
  if (committed.w !== SIZE || committed.h !== SIZE) fails.push(`size ${committed.w}x${committed.h}`);
  if (sha(committed.rgba) !== man.pixelsSha256) fails.push('the PNG pixels differ from a fresh render');
  if (saved.pixelsSha256 !== man.pixelsSha256) fails.push('manifest.json pixelsSha256 differs from a fresh render');
  if (fails.length) {
    console.error(`spell-flipbook: FAIL\n  ${fails.join('\n  ')}`);
    process.exit(1);
  }
  console.log(`spell-flipbook: OK ${SIZE}x${SIZE}, pixels ${man.pixelsSha256}`);
}

main(process.argv.slice(2));
