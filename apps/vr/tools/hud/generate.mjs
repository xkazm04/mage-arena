// Wrist-cuff HUD. One flat band, 1024x256 (4:1), every readable state.
// Node built-ins plus the glyph PNG writer. Two runs write the same bytes:
// fixed geometry, analytic coverage, IHDR/IDAT/IEND, zlib level 9, no timestamps.
import crypto from 'node:crypto';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { encodePng, boxRadii } from '../glyphs/paint.mjs';

const HERE = path.dirname(fileURLToPath(import.meta.url));
const ROOT = path.resolve(HERE, '../..');
const COMBAT = path.join(ROOT, 'data/pinned/docs/design/baseline-fourteen-nights/design/data/combat.json');
const PROMPTS = path.join(ROOT, 'art/style-studies/prompts.json');
const OUT = path.join(ROOT, 'art/hud');

const W = 1024;
const H = 256;
const BAND = { x: 12, y: 12, w: 1000, h: 232, rad: 36 };
const SOCKET_Y = 160.5;
const SOCKET_X = [152.5, 392.5, 632.5, 872.5];
const WELL_R = 47;
const BEZEL_R = 64;
const BEZEL_W = 8;
const ARC_R = 55;
const ARC_W = 5.5;
const RUNE_SCALE = 0.44;
const OUTLINE_STROKE = 1.6;
const OUTLINE_DOT = 1.75;
const PIP_Y = 48.5;
const PIP_R = 12;
const PIP_X = [136.5, 324.5, 512.5, 700.5, 888.5];
// Channel behind the beads. Top clears the groove stroke; bottom clears the crest seal.
const RAIL = {
  x: PIP_X[0] - PIP_R - 6,
  y: PIP_Y - PIP_R - 3,
  w: PIP_X[4] - PIP_X[0] + 2 * (PIP_R + 6),
  h: 2 * (PIP_R + 3),
  rad: PIP_R + 3,
};
const SEAL = { x: 512.5, y: 84, r: 10 };
const LAMP_DX = 12;
const LAMP_DY = 12;
const DIM_T = 0.6;
const DIM_TOWARD = { r: 0x1a, g: 0x12, b: 0x0e };

const RUNE_EXPECT = {
  'rune-tier1': { ring: 1, disk: 1, polyline: 0 },
  'rune-tier2': { ring: 1, disk: 2, polyline: 0 },
  'rune-tier3': { ring: 1, disk: 3, polyline: 0 },
  'rune-tier4': { ring: 1, disk: 5, polyline: 4 },
};

function fmt(n) {
  if (!Number.isFinite(n)) throw new Error(`non-finite coordinate ${n}`);
  let text = n.toFixed(4);
  text = text.replace(/(\.\d*?)0+$/, '$1').replace(/\.$/, '');
  if (text === '-0') return '0';
  return text;
}

function esc(text) {
  return text.replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;');
}

function clamp01(value) {
  if (value < 0) return 0;
  if (value > 1) return 1;
  return value;
}

function cover(half, distance) {
  const value = half + 0.5 - distance;
  if (value <= 0) return 0;
  if (value >= 1) return 1;
  return value;
}

function hex(value) {
  const n = Number.parseInt(value.slice(1), 16);
  return { r: (n >> 16) & 255, g: (n >> 8) & 255, b: n & 255 };
}

function mix(a, b, t) {
  return {
    r: a.r + (b.r - a.r) * t,
    g: a.g + (b.g - a.g) * t,
    b: a.b + (b.b - a.b) * t,
  };
}

function rgbHex(color) {
  const byte = (channel) => Math.round(clamp01(channel / 255) * 255).toString(16).padStart(2, '0').toUpperCase();
  return `#${byte(color.r)}${byte(color.g)}${byte(color.b)}`;
}

function channelLinear(channel) {
  const s = channel / 255;
  return s <= 0.04045 ? s / 12.92 : ((s + 0.055) / 1.055) ** 2.4;
}

function luminance(r, g, b) {
  return 0.2126 * channelLinear(r) + 0.7152 * channelLinear(g) + 0.0722 * channelLinear(b);
}

function contrastRatio(lighter, darker) {
  return (lighter + 0.05) / (darker + 0.05);
}

function parsePalette(text) {
  const out = {};
  for (const part of text.split(',')) {
    const match = part.trim().match(/^(.*?)\s+(#[0-9A-Fa-f]{6})$/);
    if (!match) throw new Error(`bad palette entry: ${part}`);
    out[match[1].trim()] = match[2].toUpperCase();
  }
  return out;
}

function loadStyles() {
  const prompts = JSON.parse(fs.readFileSync(PROMPTS, 'utf8'));
  const wanted = [
    ['13-screen-print', ['paper', 'teal', 'sunset orange', 'deep plum', 'mustard']],
    ['25-moonlit-silver', ['moon silver', 'slate', 'night blue', 'water']],
    ['30-chalk-slate', ['slate', 'chalk white', 'chalk blue', 'chalk yellow', 'chalk red']],
  ];
  return wanted.map(([id, keys]) => {
    const style = prompts.styles.find((item) => item.id === id);
    if (!style) throw new Error(`prompts.json has no style ${id}`);
    const palette = parsePalette(style.palette);
    for (const key of keys) {
      if (!palette[key]) throw new Error(`${id} palette has no ${key}`);
    }
    return { id, name: style.name, palette };
  });
}

function loadClock() {
  const data = JSON.parse(fs.readFileSync(COMBAT, 'utf8'));
  const table = data.tierClock.unlockAtSeconds;
  const unlocks = [table['1'], table['2'], table['3'], table['4']];
  const advance = data.tierClock.perfectAbsorbAdvanceS;
  const minGap = data.tierClock.minimumSecondsBetweenUnlocks;
  const flowMax = data.flow.max;
  if (unlocks.join(',') !== '0,15,30,45') throw new Error(`tier unlocks are ${unlocks.join(',')}`);
  if (advance !== 2) throw new Error(`perfectAbsorbAdvanceS is ${advance}`);
  if (minGap !== 6) throw new Error(`minimumSecondsBetweenUnlocks is ${minGap}`);
  if (flowMax !== 5) throw new Error(`flow.max is ${flowMax}`);
  const window = unlocks[1] - unlocks[0];
  if (unlocks[2] - unlocks[1] !== window || unlocks[3] - unlocks[2] !== window) {
    throw new Error('tier windows are not equal');
  }
  return { unlocks, advance, minGap, flowMax, window };
}

function loadRunes() {
  return [1, 2, 3, 4].map((tier) => {
    const id = `rune-tier${tier}`;
    const svg = fs.readFileSync(path.join(ROOT, 'art/glyphs', `${id}.svg`), 'utf8');
    const match = svg.match(/<g id="rune">([\s\S]*?)<\/g>\s*<\/svg>/);
    if (!match) throw new Error(`${id} has no rune group`);
    const inner = match[1];
    const tags = [...inner.matchAll(/<([a-z]+)\b/g)].map((item) => item[1]);
    for (const tag of tags) {
      if (tag !== 'circle' && tag !== 'path') throw new Error(`${id} has a <${tag}>`);
    }
    const drawables = [];
    const tagRe = /<(circle|path)\b([^>]*?)\/>/g;
    let tag;
    while ((tag = tagRe.exec(inner))) {
      const kind = tag[1];
      const attrs = tag[2];
      const num = (name) => {
        const found = new RegExp(`${name}="([0-9.]+)"`).exec(attrs);
        if (!found) throw new Error(`${id} ${kind} missing ${name}`);
        return Number(found[1]);
      };
      if (kind === 'circle') {
        const fill = /fill="([^"]*)"/.exec(attrs);
        if (!fill) throw new Error(`${id} circle has no fill`);
        if (fill[1] === 'none') {
          drawables.push({ type: 'ring', cx: num('cx'), cy: num('cy'), r: num('r'), width: num('stroke-width') });
        } else {
          drawables.push({ type: 'disk', cx: num('cx'), cy: num('cy'), r: num('r') });
        }
      } else {
        const d = /d="([^"]*)"/.exec(attrs);
        if (!d) throw new Error(`${id} path has no d`);
        const leftover = d[1].replace(/[ML]\s*[0-9.-]+\s+[0-9.-]+/g, '').trim();
        if (leftover) throw new Error(`${id} path has unsupported commands: ${leftover}`);
        const points = [];
        const pointRe = /[ML]\s*([0-9.-]+)\s+([0-9.-]+)/g;
        let point;
        while ((point = pointRe.exec(d[1]))) points.push({ x: Number(point[1]), y: Number(point[2]) });
        if (points.length < 2) throw new Error(`${id} path is too short`);
        drawables.push({ type: 'polyline', points, width: num('stroke-width') });
      }
    }
    const expect = RUNE_EXPECT[id];
    for (const type of ['ring', 'disk', 'polyline']) {
      const got = drawables.filter((item) => item.type === type).length;
      if (got !== expect[type]) throw new Error(`${id} has ${got} ${type}, expected ${expect[type]}`);
    }
    return { id, tier, inner, drawables };
  });
}

function litCount(t, unlocks) {
  let count = 0;
  for (const unlock of unlocks) if (t + 1e-9 >= unlock) count += 1;
  return count;
}

function progressAt(t, index, unlocks) {
  if (t + 1e-9 >= unlocks[index]) return 1;
  if (index === 0) return 1;
  const prev = unlocks[index - 1];
  if (t <= prev) return 0;
  return (t - prev) / (unlocks[index] - prev);
}

function flowBlurb(n) {
  return `Flow ${n} of 5. The clock is held at 22.5 s so only the pips change: tiers I and II are lit, the tier III arc is half full, and tier IV is dark.`;
}

function makeStates(clock) {
  const blurbs = {
    idle: 'Wave start. The clock is at 0 s, so only tier I is lit. Its arc is full. Tiers II, III and IV are dark, with empty tracks. Flow is 0.',
    tier2: '15 s. Tiers I and II are lit and their arcs are full. Tier III is next, so its track is empty. Flow is 0.',
    tier3: '30 s. Tiers I to III are lit. Tier IV is next. Flow is 0.',
    tier4: '45 s. All four tiers are lit and every arc is full. Flow is 0.',
    'unlock-flash': 'The moment tier II lights, at 15 s. Socket II is hotter than a settled lamp and throws a burst. Tier I stays steady. Flow is 0.',
    'perfect-advance': 'One perfect absorb. The clock jumps 2 s, from 6 s to 8 s, on the way to tier II at 15 s. The tick marks 6 s. The second colour is the 2 s jump. Tier I stays lit. Flow is 0.',
    'crest-ready': 'Flow is 5, so the next spell is a free Crest. The five pips join, a seal sits under the row, and the rim wakes. The clock matches the flow row: 22.5 s, tiers I and II lit, tier III arc half full.',
    pause: 'Pause. The flow-2 cuff, dimmed: 22.5 s, tiers I and II lit, tier III arc half full, two pips. Lit lamps stay lighter than dark sockets.',
  };
  const rows = [
    ['idle', 0, {}],
    ['tier2', 15, {}],
    ['tier3', 30, {}],
    ['tier4', 45, {}],
    ['unlock-flash', 15, { flash: 1 }],
    ['perfect-advance', 8, { advance: clock.advance }],
  ];
  for (let n = 0; n <= clock.flowMax; n += 1) rows.push([`flow-${n}`, 22.5, { flow: n }]);
  rows.push(['crest-ready', 22.5, { flow: clock.flowMax, crest: true }]);
  rows.push(['pause', 22.5, { flow: 2, dim: true }]);
  return rows.map(([id, t, extra]) => {
    const lit = litCount(t, clock.unlocks);
    const advance = extra.advance ?? 0;
    if (advance) {
      if (t - advance < 0) throw new Error('perfect-advance crosses the previous unlock');
      if (t >= clock.unlocks[1]) throw new Error('perfect-advance example should stay before tier II');
      if (t < clock.minGap) throw new Error('perfect-advance example is inside the minimum gap');
    }
    return {
      id,
      t,
      lit,
      flow: extra.flow ?? 0,
      flash: extra.flash ?? -1,
      advance,
      crest: Boolean(extra.crest),
      dim: Boolean(extra.dim),
      blurb: blurbs[id] || flowBlurb(extra.flow ?? 0),
    };
  });
}

function placeDrawable(drawable, cx, cy, scale) {
  if (drawable.type === 'disk') {
    return {
      type: 'disk',
      cx: cx + (drawable.cx - 128) * scale,
      cy: cy + (drawable.cy - 128) * scale,
      r: drawable.r * scale,
    };
  }
  if (drawable.type === 'ring') {
    return {
      type: 'ring',
      cx: cx + (drawable.cx - 128) * scale,
      cy: cy + (drawable.cy - 128) * scale,
      r: drawable.r * scale,
      width: drawable.width * scale,
    };
  }
  return {
    type: 'polyline',
    width: drawable.width * scale,
    points: drawable.points.map((point) => ({
      x: cx + (point.x - 128) * scale,
      y: cy + (point.y - 128) * scale,
    })),
  };
}

function outlineCopy(shape) {
  if (shape.type === 'disk') return { ...shape, r: shape.r * OUTLINE_DOT };
  return { ...shape, width: shape.width * OUTLINE_STROKE };
}

function arcAngle(fraction) {
  // y grows down, so atan2 increases clockwise. Fraction 0 is 12 o'clock.
  return -Math.PI / 2 + fraction * Math.PI * 2;
}

function buildShapes(state, clock) {
  const shapes = [];
  const push = (shape) => shapes.push({ glow: false, lit: false, ...shape });
  push({
    type: 'roundrect', x: BAND.x, y: BAND.y, w: BAND.w, h: BAND.h, rad: BAND.rad,
    role: 'band', group: 'band',
  });
  push({
    type: 'roundrect-stroke',
    x: BAND.x + 14, y: BAND.y + 14, w: BAND.w - 28, h: BAND.h - 28, rad: BAND.rad - 14, width: 4,
    role: 'groove', group: 'band',
  });
  push({
    type: 'roundrect-stroke',
    x: BAND.x, y: BAND.y, w: BAND.w, h: BAND.h, rad: BAND.rad, width: 8,
    role: 'rim', group: 'band',
  });

  for (let index = 0; index < 4; index += 1) {
    const cx = SOCKET_X[index];
    const cy = SOCKET_Y;
    const group = `socket-${index + 1}`;
    const lit = index < state.lit;
    const hot = state.flash === index;
    const progress = progressAt(state.t, index, clock.unlocks);
    const groupAttrs = ` data-tier="${index + 1}" data-lit="${lit ? 1 : 0}" data-progress="${fmt(progress)}"`;
    push({ type: 'ring', cx, cy, r: BEZEL_R, width: BEZEL_W, role: 'bezel', group, groupAttrs });
    push({
      type: 'disk', cx, cy, r: WELL_R,
      role: hot ? 'well-hot' : (lit ? 'well-lit' : 'well-dark'),
      lit: lit, glow: lit, group,
    });
    push({ type: 'ring', cx, cy, r: ARC_R, width: ARC_W, role: 'arc-track', group });
    if (progress >= 1) {
      push({ type: 'ring', cx, cy, r: ARC_R, width: ARC_W, role: 'arc-fill', lit: true, glow: true, group });
    } else if (progress > 0) {
      push({
        type: 'arc', cx, cy, r: ARC_R, width: ARC_W, a0: arcAngle(0), sweep: progress * Math.PI * 2,
        role: 'arc-fill', lit: true, glow: true, group,
      });
      if (state.advance > 0) {
        const from = progress - state.advance / clock.window;
        if (from < 0 || from >= progress) throw new Error(`bad advance span ${from} to ${progress}`);
        push({
          type: 'arc', cx, cy, r: ARC_R, width: ARC_W, a0: arcAngle(from), sweep: (progress - from) * Math.PI * 2,
          role: 'arc-advance', lit: true, glow: true, group,
        });
        const tick = arcAngle(from);
        push({
          type: 'segment',
          x1: cx + Math.cos(tick) * (ARC_R - 11),
          y1: cy + Math.sin(tick) * (ARC_R - 11),
          x2: cx + Math.cos(tick) * (ARC_R + 11),
          y2: cy + Math.sin(tick) * (ARC_R + 11),
          width: 3,
          role: 'arc-tick', lit: true, group,
        });
      }
    }
    for (const drawable of state.runes[index].drawables) {
      const placed = placeDrawable(drawable, cx, cy, RUNE_SCALE);
      if (lit) {
        push({
          ...outlineCopy(placed),
          role: 'rune-lit-outline', lit: true, glow: true, group, rune: true, runeIndex: index,
          anchor: { x: cx, y: cy },
        });
      }
      push({
        ...placed,
        role: lit ? 'rune-lit' : 'rune-dark', lit, glow: lit, group, rune: true, runeIndex: index,
        anchor: { x: cx, y: cy },
      });
    }
    if (hot) {
      push({ type: 'ring', cx, cy, r: 74, width: 6, role: 'flash', lit: true, glow: true, group: 'flash' });
      for (let ray = 0; ray < 8; ray += 1) {
        const angle = arcAngle(ray / 8);
        push({
          type: 'segment',
          x1: cx + Math.cos(angle) * 70,
          y1: cy + Math.sin(angle) * 70,
          x2: cx + Math.cos(angle) * 80,
          y2: cy + Math.sin(angle) * 80,
          width: 5,
          role: 'flash', lit: true, glow: true, group: 'flash',
        });
      }
    }
  }

  push({
    type: 'roundrect',
    x: RAIL.x, y: RAIL.y, w: RAIL.w, h: RAIL.h, rad: RAIL.rad,
    role: 'pip-rail', group: 'flow-rail',
  });

  if (state.crest) {
    push({
      type: 'roundrect-stroke',
      x: BAND.x + 3, y: BAND.y + 3, w: BAND.w - 6, h: BAND.h - 6, rad: BAND.rad - 3, width: 7,
      role: 'crest', lit: true, glow: true, group: 'crest',
    });
    push({
      type: 'roundrect',
      x: PIP_X[0], y: PIP_Y - 3.5, w: PIP_X[4] - PIP_X[0], h: 7, rad: 3.5,
      role: 'crest', lit: true, glow: true, group: 'crest',
    });
    push({ type: 'disk', cx: SEAL.x, cy: SEAL.y, r: SEAL.r, role: 'crest', lit: true, glow: true, group: 'crest' });
    push({ type: 'ring', cx: SEAL.x, cy: SEAL.y, r: SEAL.r + 5, width: 3, role: 'crest', lit: true, glow: true, group: 'crest' });
  }

  for (let index = 0; index < 5; index += 1) {
    const cx = PIP_X[index];
    const filled = index < state.flow;
    push({ type: 'disk', cx, cy: PIP_Y, r: PIP_R, role: 'pip-empty', group: 'flow' });
    if (filled) push({ type: 'disk', cx, cy: PIP_Y, r: PIP_R, role: 'pip-lit', lit: true, glow: true, group: 'flow' });
    push({ type: 'ring', cx, cy: PIP_Y, r: PIP_R, width: 3.5, role: 'pip-ring', lit: true, group: 'flow' });
  }
  return shapes;
}

function shiftShape(shape, dx, dy, role) {
  const next = { ...shape, role, lit: false, glow: false, rune: false };
  if (next.cx !== undefined) next.cx += dx;
  if (next.cy !== undefined) next.cy += dy;
  if (next.type === 'roundrect' || next.type === 'roundrect-stroke') {
    next.x += dx;
    next.y += dy;
  }
  if (next.type === 'segment') {
    next.x1 += dx;
    next.y1 += dy;
    next.x2 += dx;
    next.y2 += dy;
  }
  if (next.points) next.points = next.points.map((point) => ({ x: point.x + dx, y: point.y + dy }));
  return next;
}

function decorate(shapes, styleId) {
  // 10, 8 stays inside 1024x256: band right 1012+10, band bottom 244+8.
  // The pip channel is not a plate, so it is not shifted.
  if (styleId === '13-screen-print') {
    const ghost = shapes
      .filter((shape) => shape.role === 'band' || shape.role === 'bezel' || shape.role === 'rim')
      .map((shape) => shiftShape(shape, 10, 8, 'misreg'));
    const dots = [];
    for (let x = 48.5; x <= 976; x += 20) {
      dots.push({ type: 'disk', cx: x, cy: 234.5, r: 2.1, role: 'halftone', lit: false, glow: false, group: 'band' });
    }
    return [...ghost, ...shapes, ...dots];
  }
  if (styleId === '30-chalk-slate') {
    const ghost = shapes
      .filter((shape) => shape.role === 'band' || shape.role === 'bezel' || shape.role === 'rim')
      .map((shape) => shiftShape(shape, 10, 8, 'shadow'));
    return [...ghost, ...shapes];
  }
  return shapes;
}

function eachPixel(w, h, minX, minY, maxX, maxY, fn) {
  const x0 = Math.max(0, minX);
  const y0 = Math.max(0, minY);
  const x1 = Math.min(w - 1, maxX);
  const y1 = Math.min(h - 1, maxY);
  for (let y = y0; y <= y1; y++) {
    for (let x = x0; x <= x1; x++) fn(x, y);
  }
}

function rasterShape(w, h, shape, paint) {
  if (shape.type === 'disk') {
    const reach = shape.r + 1;
    eachPixel(w, h, Math.floor(shape.cx - reach), Math.floor(shape.cy - reach), Math.ceil(shape.cx + reach), Math.ceil(shape.cy + reach), (x, y) => {
      const cov = cover(shape.r, Math.hypot(x + 0.5 - shape.cx, y + 0.5 - shape.cy));
      if (cov > 0) paint(x, y, cov);
    });
    return;
  }
  if (shape.type === 'ring') {
    const reach = shape.r + shape.width / 2 + 1;
    const half = shape.width / 2;
    eachPixel(w, h, Math.floor(shape.cx - reach), Math.floor(shape.cy - reach), Math.ceil(shape.cx + reach), Math.ceil(shape.cy + reach), (x, y) => {
      const cov = cover(half, Math.abs(Math.hypot(x + 0.5 - shape.cx, y + 0.5 - shape.cy) - shape.r));
      if (cov > 0) paint(x, y, cov);
    });
    return;
  }
  if (shape.type === 'segment') {
    paintSegment(w, h, shape.x1, shape.y1, shape.x2, shape.y2, shape.width, paint);
    return;
  }
  if (shape.type === 'polyline') {
    const points = shape.points;
    for (let i = 1; i < points.length; i++) {
      paintSegment(w, h, points[i - 1].x, points[i - 1].y, points[i].x, points[i].y, shape.width, paint);
    }
    return;
  }
  if (shape.type === 'arc') {
    const reach = shape.r + shape.width / 2 + 2;
    const half = shape.width / 2;
    const tau = Math.PI * 2;
    eachPixel(w, h, Math.floor(shape.cx - reach), Math.floor(shape.cy - reach), Math.ceil(shape.cx + reach), Math.ceil(shape.cy + reach), (x, y) => {
      const dx = x + 0.5 - shape.cx;
      const dy = y + 0.5 - shape.cy;
      let rel = Math.atan2(dy, dx) - shape.a0;
      rel %= tau;
      if (rel < 0) rel += tau;
      let distance;
      if (rel <= shape.sweep) {
        distance = Math.abs(Math.hypot(dx, dy) - shape.r);
      } else {
        const a1 = shape.a0 + shape.sweep;
        const d0 = Math.hypot(dx - shape.r * Math.cos(shape.a0), dy - shape.r * Math.sin(shape.a0));
        const d1 = Math.hypot(dx - shape.r * Math.cos(a1), dy - shape.r * Math.sin(a1));
        distance = Math.min(d0, d1);
      }
      const cov = cover(half, distance);
      if (cov > 0) paint(x, y, cov);
    });
    return;
  }
  if (shape.type === 'roundrect' || shape.type === 'roundrect-stroke') {
    const pad = shape.type === 'roundrect' ? 1 : shape.width / 2 + 1;
    eachPixel(w, h, Math.floor(shape.x - pad), Math.floor(shape.y - pad), Math.ceil(shape.x + shape.w + pad), Math.ceil(shape.y + shape.h + pad), (x, y) => {
      const sd = sdRoundRect(x + 0.5, y + 0.5, shape.x, shape.y, shape.w, shape.h, shape.rad);
      const cov = shape.type === 'roundrect' ? cover(0, sd) : cover(shape.width / 2, Math.abs(sd));
      if (cov > 0) paint(x, y, cov);
    });
    return;
  }
  throw new Error(`unknown shape ${shape.type}`);
}

function paintSegment(w, h, x1, y1, x2, y2, width, paint) {
  const half = width / 2;
  const pad = half + 1;
  const dx = x2 - x1;
  const dy = y2 - y1;
  const len2 = dx * dx + dy * dy;
  eachPixel(w, h, Math.floor(Math.min(x1, x2) - pad), Math.floor(Math.min(y1, y2) - pad), Math.ceil(Math.max(x1, x2) + pad), Math.ceil(Math.max(y1, y2) + pad), (x, y) => {
    const px = x + 0.5;
    const py = y + 0.5;
    let distance;
    if (len2 < 1e-12) distance = Math.hypot(px - x1, py - y1);
    else {
      let t = ((px - x1) * dx + (py - y1) * dy) / len2;
      if (t < 0) t = 0;
      else if (t > 1) t = 1;
      distance = Math.hypot(px - (x1 + t * dx), py - (y1 + t * dy));
    }
    const cov = cover(half, distance);
    if (cov > 0) paint(x, y, cov);
  });
}

function sdRoundRect(px, py, x, y, w, h, rad) {
  const cx = x + w / 2;
  const cy = y + h / 2;
  const hx = w / 2 - rad;
  const hy = h / 2 - rad;
  const dx = Math.abs(px - cx) - hx;
  const dy = Math.abs(py - cy) - hy;
  const ax = Math.max(dx, 0);
  const ay = Math.max(dy, 0);
  return Math.min(Math.max(dx, dy), 0) + Math.hypot(ax, ay) - rad;
}

function overInto(buf, index, color, cov) {
  const srcA = color.a * cov;
  if (srcA <= 0) return;
  const dstA = buf[index + 3];
  const outA = srcA + dstA * (1 - srcA);
  if (outA <= 0) return;
  const keep = dstA * (1 - srcA);
  buf[index] = (color.r * srcA + buf[index] * keep) / outA;
  buf[index + 1] = (color.g * srcA + buf[index + 1] * keep) / outA;
  buf[index + 2] = (color.b * srcA + buf[index + 2] * keep) / outA;
  buf[index + 3] = outA;
}

function boxBlurRect(src, w, h, radius) {
  if (radius <= 0) return src;
  const tmp = new Float64Array(w * h);
  const out = new Float64Array(w * h);
  const win = radius * 2 + 1;
  const prefix = new Float64Array(Math.max(w, h) + 1);
  for (let y = 0; y < h; y++) {
    const row = y * w;
    prefix[0] = 0;
    for (let x = 0; x < w; x++) prefix[x + 1] = prefix[x] + src[row + x];
    for (let x = 0; x < w; x++) {
      const left = Math.max(0, x - radius);
      const right = Math.min(w - 1, x + radius);
      tmp[row + x] = (prefix[right + 1] - prefix[left]) / win;
    }
  }
  for (let x = 0; x < w; x++) {
    prefix[0] = 0;
    for (let y = 0; y < h; y++) prefix[y + 1] = prefix[y] + tmp[y * w + x];
    for (let y = 0; y < h; y++) {
      const top = Math.max(0, y - radius);
      const bottom = Math.min(h - 1, y + radius);
      out[y * w + x] = (prefix[bottom + 1] - prefix[top]) / win;
    }
  }
  return out;
}

function quantize(buf) {
  const rgba = Buffer.alloc(buf.length);
  for (let i = 0; i < buf.length; i += 4) {
    const a = buf[i + 3];
    if (a <= 0) continue;
    rgba[i] = Math.floor(clamp01(buf[i] / 255) * 255 + 0.5);
    rgba[i + 1] = Math.floor(clamp01(buf[i + 1] / 255) * 255 + 0.5);
    rgba[i + 2] = Math.floor(clamp01(buf[i + 2] / 255) * 255 + 0.5);
    rgba[i + 3] = Math.floor(clamp01(a) * 255 + 0.5);
  }
  return rgba;
}

function paintShapes(shapes, colorOf, options) {
  const buf = new Float64Array(W * H * 4);
  const stamp = (shape, color) => {
    rasterShape(W, H, shape, (x, y, cov) => overInto(buf, (y * W + x) * 4, color, cov));
  };
  const back = shapes.filter((shape) => !shape.lit);
  const lit = shapes.filter((shape) => shape.lit);
  for (const shape of back) stamp(shape, colorOf(shape.role));
  if (options.glow) {
    const cov = new Float64Array(W * H);
    for (const shape of lit) {
      if (!shape.glow) continue;
      rasterShape(W, H, shape, (x, y, value) => {
        const index = y * W + x;
        if (value > cov[index]) cov[index] = value;
      });
    }
    let halo = cov;
    for (const radius of boxRadii(6)) halo = boxBlurRect(halo, W, H, radius);
    const glow = options.glow;
    for (let i = 0; i < halo.length; i++) {
      if (halo[i] < 0.04) continue;
      overInto(buf, i * 4, glow, Math.min(1, halo[i] * 1.25));
    }
  }
  for (const shape of lit) stamp(shape, colorOf(shape.role));
  if (options.grain) options.grain(buf);
  if (options.dim) {
    const toward = options.dim;
    for (let i = 0; i < buf.length; i += 4) {
      if (buf[i + 3] <= 0) continue;
      buf[i] = buf[i] * (1 - DIM_T) + toward.r * DIM_T;
      buf[i + 1] = buf[i + 1] * (1 - DIM_T) + toward.g * DIM_T;
      buf[i + 2] = buf[i + 2] * (1 - DIM_T) + toward.b * DIM_T;
    }
  }
  return quantize(buf);
}

function grain(x, y) {
  let n = Math.imul(x + 1, 374761393) ^ Math.imul(y + 1, 668265263);
  n = Math.imul(n ^ (n >>> 13), 1274126177);
  return (n >>> 0) % 10000;
}

function chalkGrain(palette) {
  const yellow = hex(palette['chalk yellow']);
  const red = hex(palette['chalk red']);
  const white = hex(palette['chalk white']);
  return (buf) => {
    for (let y = 0; y < H; y++) {
      for (let x = 0; x < W; x++) {
        const i = (y * W + x) * 4;
        if (buf[i + 3] < 0.4) continue;
        const g = grain(x, y);
        if (g < 120) {
          buf[i] = yellow.r;
          buf[i + 1] = yellow.g;
          buf[i + 2] = yellow.b;
        } else if (g > 9930) {
          buf[i] = red.r;
          buf[i + 1] = red.g;
          buf[i + 2] = red.b;
        } else if (g > 6700 && g < 6920) {
          buf[i] = buf[i] * 0.75 + white.r * 0.25;
          buf[i + 1] = buf[i + 1] * 0.75 + white.g * 0.25;
          buf[i + 2] = buf[i + 2] * 0.75 + white.b * 0.25;
        }
      }
    }
  };
}

function paintMap(entries) {
  const map = {};
  for (const [role, color] of Object.entries(entries)) map[role] = { r: color.r, g: color.g, b: color.b, a: 1 };
  return (role) => {
    const color = map[role];
    if (!color) throw new Error(`no colour for ${role}`);
    return color;
  };
}

function screenColors(palette) {
  const paper = hex(palette.paper);
  const teal = hex(palette.teal);
  const orange = hex(palette['sunset orange']);
  const plum = hex(palette['deep plum']);
  const mustard = hex(palette.mustard);
  return paintMap({
    band: mustard,
    groove: mix(mustard, plum, 0.28),
    rim: plum,
    bezel: plum,
    'well-lit': mix(paper, teal, 0.16),
    'well-hot': paper,
    'well-dark': mix(plum, mustard, 0.14),
    'rune-lit': paper,
    'rune-lit-outline': plum,
    'rune-dark': mix(plum, mustard, 0.38),
    'arc-track': plum,
    'arc-fill': paper,
    'arc-advance': orange,
    'arc-tick': plum,
    'pip-rail': mustard,
    'pip-empty': mix(plum, mustard, 0.18),
    'pip-lit': paper,
    'pip-ring': plum,
    flash: paper,
    crest: teal,
    misreg: orange,
    halftone: plum,
  });
}

function moonlitColors(palette) {
  const silver = hex(palette['moon silver']);
  const slate = hex(palette.slate);
  const night = hex(palette['night blue']);
  const water = hex(palette.water);
  return paintMap({
    band: mix(slate, night, 0.38),
    groove: mix(silver, night, 0.72),
    rim: silver,
    bezel: night,
    'well-lit': mix(water, silver, 0.22),
    'well-hot': silver,
    'well-dark': night,
    'rune-lit': silver,
    'rune-lit-outline': night,
    'rune-dark': slate,
    'arc-track': night,
    'arc-fill': water,
    'arc-advance': silver,
    'arc-tick': night,
    'pip-rail': mix(silver, slate, 0.5),
    'pip-empty': night,
    'pip-lit': water,
    'pip-ring': night,
    flash: silver,
    crest: silver,
  });
}

function chalkColors(palette) {
  const slate = hex(palette.slate);
  const white = hex(palette['chalk white']);
  const blue = hex(palette['chalk blue']);
  const yellow = hex(palette['chalk yellow']);
  const band = mix(yellow, slate, 0.22);
  return paintMap({
    band,
    groove: mix(yellow, slate, 0.45),
    rim: slate,
    bezel: slate,
    'well-lit': white,
    'well-hot': white,
    'well-dark': slate,
    'rune-lit': white,
    'rune-lit-outline': slate,
    'rune-dark': mix(slate, white, 0.22),
    'arc-track': slate,
    'arc-fill': blue,
    'arc-advance': yellow,
    'arc-tick': slate,
    'pip-rail': band,
    'pip-empty': slate,
    'pip-lit': white,
    'pip-ring': slate,
    flash: white,
    crest: blue,
    shadow: slate,
  });
}

function stylePaint(style) {
  if (style.id === '13-screen-print') {
    return { colorOf: screenColors(style.palette), glow: null, grain: null, dimToward: hex(style.palette['deep plum']) };
  }
  if (style.id === '25-moonlit-silver') {
    const water = hex(style.palette.water);
    return {
      colorOf: moonlitColors(style.palette),
      glow: { r: water.r, g: water.g, b: water.b, a: 0.9 },
      grain: null,
      dimToward: hex(style.palette['night blue']),
    };
  }
  if (style.id === '30-chalk-slate') {
    return { colorOf: chalkColors(style.palette), glow: null, grain: chalkGrain(style.palette), dimToward: hex(style.palette.slate) };
  }
  throw new Error(`unknown style ${style.id}`);
}

const MASK_ALPHA = {
  band: 0.14,
  groove: 0.24,
  rim: 0.62,
  bezel: 0.5,
  'well-lit': 0.78,
  'well-hot': 1,
  'well-dark': 0.2,
  'rune-lit': 1,
  'rune-lit-outline': 0.42,
  'rune-dark': 0.32,
  'arc-track': 0.28,
  'arc-fill': 1,
  'arc-advance': 1,
  'arc-tick': 0.9,
  'pip-rail': 0.42,
  'pip-empty': 0.24,
  'pip-lit': 1,
  'pip-ring': 0.55,
  flash: 1,
  crest: 1,
};

function maskColor(role, dim) {
  const alpha = MASK_ALPHA[role];
  if (alpha === undefined) throw new Error(`mask has no ${role}`);
  return { r: 255, g: 255, b: 255, a: dim ? alpha * 0.4 : alpha };
}

const NEUTRAL = {
  band: '#8C6239',
  groove: '#C4A36A',
  rim: '#2A1C14',
  bezel: '#3A271C',
  'well-lit': '#F4E4C4',
  'well-hot': '#FFFFFF',
  'well-dark': '#2A2118',
  'rune-lit': '#FFF8EC',
  'rune-lit-outline': '#1A120E',
  'rune-dark': '#5A4638',
  'arc-track': '#1A120E',
  'arc-fill': '#F4E4C4',
  'arc-advance': '#E2B15A',
  'arc-tick': '#1A120E',
  'pip-rail': '#CDB88A',
  'pip-empty': '#2A2118',
  'pip-lit': '#FFF8EC',
  'pip-ring': '#1A120E',
  flash: '#FFFFFF',
  crest: '#F4E4C4',
};

function neutralColor(role, dim) {
  const base = NEUTRAL[role];
  if (!base) throw new Error(`neutral has no ${role}`);
  const color = dim ? mix(hex(base), DIM_TOWARD, 0.55) : hex(base);
  return { ...color, a: 1 };
}

function shapeSvg(shape, color) {
  const ink = rgbHex(color);
  if (shape.type === 'disk') return `<circle cx="${fmt(shape.cx)}" cy="${fmt(shape.cy)}" r="${fmt(shape.r)}" fill="${ink}"/>`;
  if (shape.type === 'ring') {
    return `<circle cx="${fmt(shape.cx)}" cy="${fmt(shape.cy)}" r="${fmt(shape.r)}" fill="none" stroke="${ink}" stroke-width="${fmt(shape.width)}"/>`;
  }
  if (shape.type === 'segment') {
    return `<line x1="${fmt(shape.x1)}" y1="${fmt(shape.y1)}" x2="${fmt(shape.x2)}" y2="${fmt(shape.y2)}" stroke="${ink}" stroke-width="${fmt(shape.width)}" stroke-linecap="round"/>`;
  }
  if (shape.type === 'arc') {
    const x0 = shape.cx + shape.r * Math.cos(shape.a0);
    const y0 = shape.cy + shape.r * Math.sin(shape.a0);
    const a1 = shape.a0 + shape.sweep;
    const x1 = shape.cx + shape.r * Math.cos(a1);
    const y1 = shape.cy + shape.r * Math.sin(a1);
    const large = shape.sweep >= Math.PI ? 1 : 0;
    const d = `M ${fmt(x0)} ${fmt(y0)} A ${fmt(shape.r)} ${fmt(shape.r)} 0 ${large} 1 ${fmt(x1)} ${fmt(y1)}`;
    return `<path d="${d}" fill="none" stroke="${ink}" stroke-width="${fmt(shape.width)}" stroke-linecap="round"/>`;
  }
  if (shape.type === 'roundrect') {
    return `<rect x="${fmt(shape.x)}" y="${fmt(shape.y)}" width="${fmt(shape.w)}" height="${fmt(shape.h)}" rx="${fmt(shape.rad)}" fill="${ink}"/>`;
  }
  if (shape.type === 'roundrect-stroke') {
    return `<rect x="${fmt(shape.x)}" y="${fmt(shape.y)}" width="${fmt(shape.w)}" height="${fmt(shape.h)}" rx="${fmt(shape.rad)}" fill="none" stroke="${ink}" stroke-width="${fmt(shape.width)}"/>`;
  }
  throw new Error(`no svg for ${shape.type}`);
}

function transformRune(inner, stroke, fill, strokeFactor, dotFactor) {
  return inner.replace(/<(circle|path)\b([^>]*?)\/>/g, (full, kind, attrs) => {
    let next = attrs;
    if (kind === 'circle') {
      const fillAttr = /fill="([^"]*)"/.exec(attrs);
      const disk = fillAttr && fillAttr[1] !== 'none';
      if (disk && dotFactor !== 1) {
        next = next.replace(/r="([0-9.]+)"/, (_, r) => `r="${fmt(Number(r) * dotFactor)}"`);
        next = next.replace(/fill="[^"]*"/, `fill="${fill}"`);
      } else if (disk) {
        next = next.replace(/fill="[^"]*"/, `fill="${fill}"`);
      }
      next = next.replace(/stroke="#ffffff"/, `stroke="${stroke}"`);
      if (!disk) {
        next = next.replace(/stroke-width="([0-9.]+)"/, (_, w) => `stroke-width="${fmt(Number(w) * strokeFactor)}"`);
      }
    } else {
      next = next.replace(/stroke="#ffffff"/, `stroke="${stroke}"`);
      next = next.replace(/stroke-width="([0-9.]+)"/, (_, w) => `stroke-width="${fmt(Number(w) * strokeFactor)}"`);
    }
    return `<${kind}${next}/>`;
  });
}

function embedRune(rune, anchor, lit, colorOf) {
  const ink = rgbHex(colorOf(lit ? 'rune-lit' : 'rune-dark'));
  const core = transformRune(rune.inner, ink, ink, 1, 1).trim();
  const body = [];
  if (lit) {
    const outline = rgbHex(colorOf('rune-lit-outline'));
    const under = transformRune(rune.inner, outline, outline, OUTLINE_STROKE, OUTLINE_DOT).trim();
    body.push(`      <g class="inlay">${under}</g>`);
  }
  body.push(`      <g class="rune">${core}</g>`);
  const transform = `translate(${fmt(anchor.x)} ${fmt(anchor.y)}) scale(${fmt(RUNE_SCALE)}) translate(-128 -128)`;
  return `    <g id="rune-${rune.tier}" data-source="${rune.id}.svg" transform="${transform}">\n${body.join('\n')}\n    </g>`;
}

function renderSvg(state, colorOf) {
  const lines = [];
  let group = null;
  const emitted = new Set();
  const close = () => {
    if (group) lines.push('  </g>');
    group = null;
  };
  for (const shape of state.shapes) {
    if (shape.group !== group) {
      close();
      group = shape.group;
      lines.push(`  <g id="${group}"${shape.groupAttrs || ''}>`);
    }
    if (shape.rune) {
      if (!emitted.has(shape.runeIndex)) {
        emitted.add(shape.runeIndex);
        const lit = shape.runeIndex < state.lit;
        lines.push(embedRune(state.runes[shape.runeIndex], shape.anchor, lit, colorOf));
      }
      continue;
    }
    lines.push(`    ${shapeSvg(shape, colorOf(shape.role))}`);
  }
  close();
  const desc = `${state.blurb} Unlocks at 0, 15, 30 and 45 s. Perfect absorb advances 2 s. Unlocks stay at least 6 s apart. Flow max is 5. Left to right is tier I to IV. Arcs fill clockwise from the top.`;
  return `<?xml version="1.0" encoding="UTF-8"?>\n<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 ${W} ${H}" width="${W}" height="${H}" data-state="${state.id}" data-clock-s="${fmt(state.t)}" data-flow="${state.flow}" data-lit="${state.lit}">\n  <title>${esc(state.id)}</title>\n  <desc>${esc(desc)}</desc>\n  <!-- Neutral bronze master. Palette PNGs carry the style texture. Runes are rune-tier1..4.svg. Progress uses y-down angles: 0 is 12 o'clock and sweep is clockwise. -->\n${lines.join('\n')}\n</svg>\n`;
}

function pixel(rgba, x, y) {
  const px = Math.round(x - 0.5);
  const py = Math.round(y - 0.5);
  const index = (py * W + px) * 4;
  return { r: rgba[index], g: rgba[index + 1], b: rgba[index + 2], a: rgba[index + 3], x: px, y: py };
}

function near(got, want, slack) {
  return Math.abs(got.r - want.r) <= slack && Math.abs(got.g - want.g) <= slack && Math.abs(got.b - want.b) <= slack && got.a > 250;
}

function findInk(rgba, want, offsets) {
  for (const [dx, dy] of offsets) {
    const sample = pixel(rgba, want.x + dx, want.y + dy);
    if (near(sample, want.color, want.slack)) return sample;
  }
  return null;
}

function diskMeanL(rgba, cx, cy, radius) {
  let sum = 0;
  let n = 0;
  const r2 = radius * radius;
  const x0 = Math.max(0, Math.floor(cx - radius));
  const x1 = Math.min(W - 1, Math.ceil(cx + radius));
  const y0 = Math.max(0, Math.floor(cy - radius));
  const y1 = Math.min(H - 1, Math.ceil(cy + radius));
  for (let y = y0; y <= y1; y++) {
    for (let x = x0; x <= x1; x++) {
      const dx = x + 0.5 - cx;
      const dy = y + 0.5 - cy;
      if (dx * dx + dy * dy > r2) continue;
      const index = (y * W + x) * 4;
      if (rgba[index + 3] < 250) continue;
      sum += luminance(rgba[index], rgba[index + 1], rgba[index + 2]);
      n += 1;
    }
  }
  if (!n) throw new Error('empty disk sample');
  return sum / n;
}

function meanL(rgba) {
  let sum = 0;
  let n = 0;
  for (let i = 0; i < rgba.length; i += 4) {
    if (rgba[i + 3] < 250) continue;
    sum += luminance(rgba[i], rgba[i + 1], rgba[i + 2]);
    n += 1;
  }
  return sum / n;
}

function pairReport(lit, dark) {
  const litL = luminance(lit.r, lit.g, lit.b);
  const darkL = luminance(dark.r, dark.g, dark.b);
  return {
    lit, dark, litL, darkL,
    ratio: contrastRatio(Math.max(litL, darkL), Math.min(litL, darkL)),
    litBrighter: litL > darkL,
  };
}

const INK_OFFSETS = [[0, 0], [1, 0], [-1, 0], [0, 1], [0, -1]];
const LAMP_OFFSETS = [[0, 0], [4, -2], [-4, 2], [2, 6], [-6, 2], [8, 4], [-2, 8]];

function measurePalette(name, idle, flow0, flow5, crest, pause, flow2, colorOf) {
  const lampLit = findInk(idle, {
    x: SOCKET_X[0] + LAMP_DX, y: SOCKET_Y + LAMP_DY, color: colorOf('well-lit'), slack: 28,
  }, LAMP_OFFSETS);
  const lampDark = findInk(idle, {
    x: SOCKET_X[3] + LAMP_DX, y: SOCKET_Y + LAMP_DY, color: colorOf('well-dark'), slack: 28,
  }, LAMP_OFFSETS);
  const inkLit = findInk(idle, { x: SOCKET_X[0], y: SOCKET_Y, color: colorOf('rune-lit'), slack: 28 }, INK_OFFSETS);
  const inkDark = findInk(idle, { x: SOCKET_X[3], y: SOCKET_Y, color: colorOf('rune-dark'), slack: 28 }, INK_OFFSETS);
  if (!lampLit || !lampDark) throw new Error(`${name} lamp sample missed the well`);
  if (!inkLit || !inkDark) throw new Error(`${name} ink sample missed the rune dot`);
  const lamp = pairReport(lampLit, lampDark);
  const ink = pairReport(inkLit, inkDark);
  const faceLit = diskMeanL(idle, SOCKET_X[0], SOCKET_Y, WELL_R);
  const faceDark = diskMeanL(idle, SOCKET_X[3], SOCKET_Y, WELL_R);
  const faceRatio = contrastRatio(Math.max(faceLit, faceDark), Math.min(faceLit, faceDark));
  const pipLit = pixel(flow5, PIP_X[0], PIP_Y);
  const pipEmpty = pixel(flow0, PIP_X[0], PIP_Y);
  const pip = pairReport(pipLit, pipEmpty);
  const seal = pixel(crest, SEAL.x, SEAL.y);
  const sealOff = pixel(flow5, SEAL.x, SEAL.y);
  const playL = meanL(flow2);
  const pauseL = meanL(pause);
  const problems = [];
  if (!lamp.litBrighter || lamp.ratio < 4.5) problems.push(`lamp contrast ${lamp.ratio.toFixed(2)}`);
  if (!ink.litBrighter || ink.ratio < 3) problems.push(`ink contrast ${ink.ratio.toFixed(2)}`);
  if (!(faceLit > faceDark) || faceRatio < 3) problems.push(`socket face contrast ${faceRatio.toFixed(2)}`);
  if (!pip.litBrighter || pip.ratio < 3) problems.push(`pip contrast ${pip.ratio.toFixed(2)}`);
  if (seal.r === sealOff.r && seal.g === sealOff.g && seal.b === sealOff.b) problems.push('crest seal matches flow-5');
  if (!(pauseL < playL * 0.8)) problems.push(`pause mean L ${pauseL.toFixed(3)} vs play ${playL.toFixed(3)}`);
  if (problems.length) throw new Error(`${name}: ${problems.join('; ')}`);
  return {
    name,
    lamp, ink, pip,
    faceLit, faceDark, faceRatio,
    playL, pauseL,
  };
}

function sha256(buf) {
  return crypto.createHash('sha256').update(buf).digest('hex');
}

function writePng(file, rgba) {
  const png = encodePng(W, H, rgba);
  if (png.length >= 300 * 1024) throw new Error(`${file} is ${png.length} bytes`);
  if (png.readUInt32BE(16) !== W || png.readUInt32BE(20) !== H) throw new Error(`${file} IHDR size`);
  if (png[24] !== 8 || png[25] !== 6) throw new Error(`${file} is not 8-bit RGBA`);
  fs.writeFileSync(file, png);
  return png;
}

function armSymbol(id, ground, skin, edge) {
  return `<symbol id="${id}">
      <rect width="640" height="400" fill="${ground}"/>
      <g fill="${skin}" stroke="${edge}" stroke-width="5" stroke-linejoin="round">
        <rect x="278" y="250" width="84" height="150" rx="36"/>
        <ellipse cx="320" cy="238" rx="34" ry="20"/>
        <ellipse cx="320" cy="176" rx="52" ry="58"/>
        <rect x="276" y="62" width="18" height="92" rx="9"/>
        <rect x="300" y="46" width="18" height="108" rx="9"/>
        <rect x="324" y="54" width="18" height="100" rx="9"/>
        <rect x="348" y="70" width="18" height="86" rx="9"/>
        <rect x="368" y="154" width="74" height="20" rx="10" transform="rotate(32 368 164)"/>
      </g>
    </symbol>`;
}

function wristFigure(href, symbol, label) {
  return `<figure class="wrist"><svg viewBox="0 0 640 400" role="img" aria-label="${esc(label)}"><use href="#${symbol}"/><image href="${href}" x="222" y="214" width="196" height="49"/></svg><figcaption>${esc(label)}</figcaption></figure>`;
}

function page(states, styles, metrics) {
  const swatches = styles.map((style) => {
    const chips = Object.entries(style.palette).map(([name, value]) => (
      `<span class="palette"><i style="background:${value}"></i>${esc(name)} <span class="hex">${value}</span></span>`
    )).join('');
    return `<div class="style-col"><div class="style-name">${esc(style.name)}</div><div class="style-id">${esc(style.id)}</div><div class="swatches">${chips}</div></div>`;
  }).join('');
  const rows = metrics.map((row) => (
    `<tr><td>${esc(row.name)}</td><td>${rgbHex(row.lamp.lit)}</td><td>${rgbHex(row.lamp.dark)}</td><td>${row.lamp.ratio.toFixed(2)} : 1</td><td>${rgbHex(row.ink.lit)}</td><td>${rgbHex(row.ink.dark)}</td><td>${row.ink.ratio.toFixed(2)} : 1</td><td>${row.faceRatio.toFixed(2)} : 1</td><td>${row.pauseL.toFixed(3)} / ${row.playL.toFixed(3)}</td></tr>`
  )).join('');
  const cards = states.map((state) => {
    const bands = [
      ['Neutral SVG', `${state.id}.svg`, false],
      ['White mask', `mask/${state.id}.png`, true],
      ...styles.map((style) => [style.name, `${style.id}/${state.id}.png`, false]),
    ].map(([label, href, mask]) => (
      `<figure class="band"><div class="${mask ? 'chip' : 'plate'}"><img src="${href}" width="1024" height="256" alt="${esc(state.id)} ${esc(label)}"></div><figcaption>${esc(label)}</figcaption></figure>`
    )).join('');
    const wrists = [
      ['arm-13', `13-screen-print/${state.id}.png`, 'Screen-Print Poster'],
      ['arm-25', `25-moonlit-silver/${state.id}.png`, 'Moonlit Silver Nocturne'],
      ['arm-30', `30-chalk-slate/${state.id}.png`, 'Chalk & Slate'],
    ].map(([symbol, href, label]) => wristFigure(href, symbol, `${label}, ${state.id}`)).join('');
    return `<section id="${state.id}"><h2>${esc(state.id)}</h2><p>${esc(state.blurb)}</p><div class="bands">${bands}</div><div class="wrists">${wrists}</div></section>`;
  }).join('\n');
  const glance = (folder, label) => {
    const items = states.map((state) => (
      `<div class="glance-item"><img src="${folder}${state.id}.png" width="64" height="16" alt="${esc(state.id)} at 64 px"><span>${esc(state.id)}</span></div>`
    )).join('');
    return `<h3>${esc(label)}</h3><div class="glance-row">${items}</div>`;
  };
  const glanceBlock = [
    glance('mask/', 'White mask'),
    ...styles.map((style) => glance(`${style.id}/`, style.name)),
  ].join('\n');
  return `<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Wrist-cuff HUD</title>
  <style>
    :root { color-scheme: dark; }
    * { box-sizing: border-box; }
    body { margin: 0; background: #14120f; color: #eceae4; font: 18px/1.45 "Segoe UI", system-ui, sans-serif; }
    header, main { padding: 28px 32px; max-width: 86rem; }
    h1 { font-size: 32px; font-weight: 650; margin: 0 0 12px; letter-spacing: -0.02em; }
    h2 { font-size: 24px; font-weight: 650; margin: 36px 0 8px; }
    h3 { font-size: 18px; font-weight: 650; margin: 18px 0 8px; }
    p { margin: 0 0 10px; max-width: 78ch; }
    a { color: #9fd0c8; }
    .styles { display: flex; flex-wrap: wrap; gap: 28px; margin-top: 18px; }
    .style-name { font-size: 20px; font-weight: 650; }
    .style-id { color: #b7b3aa; font-size: 16px; }
    .swatches { display: flex; flex-wrap: wrap; gap: 8px 12px; margin-top: 8px; max-width: 28rem; }
    .palette { display: inline-flex; align-items: center; gap: 6px; font-size: 16px; }
    .palette i { width: 22px; height: 22px; border-radius: 4px; border: 1px solid rgba(255,255,255,0.35); display: inline-block; }
    .hex { color: #b7b3aa; }
    table { border-collapse: collapse; margin: 12px 0 20px; font-size: 16px; }
    th, td { border: 1px solid #3a342c; padding: 6px 8px; text-align: left; }
    th { background: #221e1a; font-weight: 650; }
    .bands { display: grid; gap: 12px; }
    .band, .wrist { margin: 0; }
    .plate, .chip { border-radius: 10px; padding: 8px; }
    .plate { background: #5c574e; }
    .chip { background: #121212; }
    .band img { width: 100%; height: auto; display: block; }
    .wrists { display: grid; grid-template-columns: repeat(3, minmax(180px, 1fr)); gap: 12px; margin-top: 12px; }
    .arm, .wrist svg { width: 100%; height: auto; display: block; border-radius: 8px; }
    figcaption { margin-top: 6px; font-size: 16px; color: #d4d0c8; }
    .glance-row { display: flex; flex-wrap: wrap; gap: 14px 12px; align-items: end; }
    .glance-item { width: 96px; }
    .glance-item img { width: 64px; height: 16px; display: block; background: #5c574e; border-radius: 2px; }
    .glance-item span { display: block; margin-top: 4px; font-size: 14px; line-height: 1.25; }
    @media (max-width: 800px) {
      header, main { padding: 18px 16px; }
      .wrists { grid-template-columns: 1fr; }
      table { display: block; overflow-x: auto; }
    }
  </style>
</head>
<body>
  <header>
    <h1>Wrist-cuff HUD</h1>
    <p>The off-hand bronze cuff, unrolled as a 1024 by 256 texture. Four sockets, left to right, are tiers I to IV. A light socket is unlocked. The thin arc on each socket fills clockwise from the top across the 15 s before that tier. Five pips in a channel along the top edge are Flow. At 5, the joining bar, the seal under the channel and the waking rim mean the next cast is a free Crest. Unlocks are at 0, 15, 30 and 45 s. A perfect absorb jumps the active arc forward 2 s. Unlocks stay at least 6 s apart. Pause dims the same cuff.</p>
    <p>The SVG is the neutral trace. Masks are white ink on transparency, with lit marks more opaque than dark marks. Coloured PNGs use each shortlisted palette. Screen-print shifts an orange plate and dots the lower lip with plum. Moonlit blooms a water halo, and its Flow beads sit in a light channel. Chalk keeps a slate shadow and a fixed grain. No red rim: a red rim is the leave-it threat, not a friendly cue. The moonlit fire swatch is the enemy element and is not used on the cuff.</p>
    <div class="styles">${swatches}</div>
    <h2>Lit against dark</h2>
    <p>Relative luminance is IEC 61966-2-1. Contrast is (L lighter + 0.05) / (L darker + 0.05). Lamp samples are the socket face, 12 px right and 12 px down from the centre, clear of the rune. Ink samples are the centre dots of tier I and tier IV on idle. The face ratio is the mean luminance of each well. Pause is the mean of the dimmed flow-2 cuff over the undimmed one.</p>
    <table>
      <thead><tr><th>Palette</th><th>Lit lamp</th><th>Dark lamp</th><th>Lamp</th><th>Lit ink</th><th>Dark ink</th><th>Ink</th><th>Well mean</th><th>Pause / play</th></tr></thead>
      <tbody>${rows}</tbody>
    </table>
  </header>
  <main>
    <svg xmlns="http://www.w3.org/2000/svg" width="0" height="0" style="position:absolute">
      ${armSymbol('arm-13', '#F1E7CF', '#E2B33C', '#3B2340')}
      ${armSymbol('arm-25', '#1A2235', '#4C5A6E', '#C9D3DE')}
      ${armSymbol('arm-30', '#2A2F33', '#F2D06B', '#EDEDE8')}
    </svg>
    ${cards}
    <h2>Glance test</h2>
    <p>Each state at 64 px wide, about the cuff at arm's length. The lamp and the pips are the signals that have to survive that size. The progress arc is a thin ring on the full texture.</p>
    ${glanceBlock}
  </main>
</body>
</html>
`;
}

function selfTest() {
  const buf = new Float64Array(32 * 32 * 4);
  rasterShape(32, 32, { type: 'disk', cx: 16, cy: 16, r: 6 }, (x, y, cov) => overInto(buf, (y * 32 + x) * 4, { r: 255, g: 0, b: 0, a: 1 }, cov));
  if (buf[(16 * 32 + 16) * 4] < 250) throw new Error('disk centre did not fill');
  if (buf[0] !== 0) throw new Error('disk leaked');
  const arc = new Float64Array(80 * 80 * 4);
  const shape = { type: 'arc', cx: 40, cy: 40, r: 20, width: 4, a0: arcAngle(0), sweep: Math.PI / 2 };
  rasterShape(80, 80, shape, (x, y, cov) => overInto(arc, (y * 80 + x) * 4, { r: 0, g: 255, b: 0, a: 1 }, cov));
  const top = arc[(20 * 80 + 40) * 4 + 1];
  const bottom = arc[(60 * 80 + 40) * 4 + 1];
  if (top < 200) throw new Error(`arc missed 12 o'clock (${top})`);
  if (bottom !== 0) throw new Error('arc painted 6 o\'clock');
  const sd = sdRoundRect(512, 128, BAND.x, BAND.y, BAND.w, BAND.h, BAND.rad);
  if (sd >= 0) throw new Error('band centre is outside the band');
}

function main() {
  selfTest();
  const clock = loadClock();
  const runes = loadRunes();
  const styles = loadStyles();
  const states = makeStates(clock).map((state) => ({ ...state, runes, shapes: null }));
  for (const state of states) state.shapes = buildShapes(state, clock);
  if (states.length !== 14) throw new Error(`expected 14 states, got ${states.length}`);

  fs.rmSync(OUT, { recursive: true, force: true });
  fs.mkdirSync(path.join(OUT, 'mask'), { recursive: true });
  for (const style of styles) fs.mkdirSync(path.join(OUT, style.id), { recursive: true });

  const pngs = new Map();
  const hashes = new Map();
  const claim = (folder, id, buf) => {
    const hash = sha256(buf);
    const key = `${folder}:${hash}`;
    if (hashes.has(key)) throw new Error(`${folder}/${id} duplicates ${hashes.get(key)}`);
    hashes.set(key, `${folder}/${id}`);
    return hash;
  };

  let maxBytes = 0;
  let maxName = '';
  for (const state of states) {
    const svg = renderSvg(state, (role) => neutralColor(role, state.dim));
    if (!svg.includes('M 128 70 L 128.2085')) throw new Error(`${state.id} dropped the tier IV path`);
    if (!svg.includes('data-source="rune-tier1.svg"')) throw new Error(`${state.id} dropped a rune`);
    const svgBuf = Buffer.from(svg, 'utf8');
    fs.writeFileSync(path.join(OUT, `${state.id}.svg`), svgBuf);
    claim('svg', state.id, svgBuf);

    const mask = paintShapes(state.shapes, (role) => maskColor(role, state.dim), {});
    const maskPng = writePng(path.join(OUT, 'mask', `${state.id}.png`), mask);
    claim('mask', state.id, maskPng);
    pngs.set(`mask/${state.id}`, mask);
    if (maskPng.length > maxBytes) {
      maxBytes = maskPng.length;
      maxName = `mask/${state.id}.png`;
    }

    for (const style of styles) {
      const paint = stylePaint(style);
      const rgba = paintShapes(decorate(state.shapes, style.id), paint.colorOf, {
        glow: paint.glow,
        grain: paint.grain,
        dim: state.dim ? paint.dimToward : null,
      });
      const file = path.join(OUT, style.id, `${state.id}.png`);
      const png = writePng(file, rgba);
      claim(style.id, state.id, png);
      pngs.set(`${style.id}/${state.id}`, rgba);
      if (png.length > maxBytes) {
        maxBytes = png.length;
        maxName = `${style.id}/${state.id}.png`;
      }
    }
  }

  const metrics = styles.map((style) => measurePalette(
    style.name,
    pngs.get(`${style.id}/idle`),
    pngs.get(`${style.id}/flow-0`),
    pngs.get(`${style.id}/flow-5`),
    pngs.get(`${style.id}/crest-ready`),
    pngs.get(`${style.id}/pause`),
    pngs.get(`${style.id}/flow-2`),
    stylePaint(style).colorOf,
  ));
  fs.writeFileSync(path.join(OUT, 'index.html'), page(states, styles, metrics), 'utf8');

  const files = [];
  const walk = (dir) => {
    for (const name of fs.readdirSync(dir).sort()) {
      const full = path.join(dir, name);
      if (fs.statSync(full).isDirectory()) walk(full);
      else files.push(full);
    }
  };
  walk(OUT);
  const manifest = files.map((file) => {
    const buf = fs.readFileSync(file);
    const rel = path.relative(OUT, file).split(path.sep).join('/');
    return `${sha256(buf)}  ${buf.length}  ${rel}`;
  }).join('\n');
  console.log(manifest);
  console.log(`files ${files.length}`);
  console.log(`max-png ${maxName} ${maxBytes}`);
  for (const row of metrics) {
    console.log(`${row.name} lamp ${row.lamp.ratio.toFixed(2)} ink ${row.ink.ratio.toFixed(2)} face ${row.faceRatio.toFixed(2)} pause ${row.pauseL.toFixed(3)}/${row.playL.toFixed(3)}`);
  }
}

main();
