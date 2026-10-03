// Synthetic hand clips for the desktop gesture path. Node built-ins only.
// Same seed, same bytes. See docs/CLIP-SCHEMA.md.
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath, pathToFileURL } from 'node:url';

const HZ = 72;
const SPACE = 'seated-origin, metres, +X forward, +Y right, +Z up (Unreal axes, cm converted to m)';
const KEYPOINTS = [
  'Palm', 'Wrist',
  'ThumbMetacarpal', 'ThumbProximal', 'ThumbDistal', 'ThumbTip',
  'IndexMetacarpal', 'IndexProximal', 'IndexIntermediate', 'IndexDistal', 'IndexTip',
  'MiddleMetacarpal', 'MiddleProximal', 'MiddleIntermediate', 'MiddleDistal', 'MiddleTip',
  'RingMetacarpal', 'RingProximal', 'RingIntermediate', 'RingDistal', 'RingTip',
  'LittleMetacarpal', 'LittleProximal', 'LittleIntermediate', 'LittleDistal', 'LittleTip',
];
const IDX = Object.fromEntries(KEYPOINTS.map((name, i) => [name, i]));
const TIP_NAMES = ['ThumbTip', 'IndexTip', 'MiddleTip', 'RingTip', 'LittleTip'];
const CHILD = {
  ThumbMetacarpal: 'ThumbProximal',
  ThumbProximal: 'ThumbDistal',
  ThumbDistal: 'ThumbTip',
  IndexMetacarpal: 'IndexProximal',
  IndexProximal: 'IndexIntermediate',
  IndexIntermediate: 'IndexDistal',
  IndexDistal: 'IndexTip',
  MiddleMetacarpal: 'MiddleProximal',
  MiddleProximal: 'MiddleIntermediate',
  MiddleIntermediate: 'MiddleDistal',
  MiddleDistal: 'MiddleTip',
  RingMetacarpal: 'RingProximal',
  RingProximal: 'RingIntermediate',
  RingIntermediate: 'RingDistal',
  RingDistal: 'RingTip',
  LittleMetacarpal: 'LittleProximal',
  LittleProximal: 'LittleIntermediate',
  LittleIntermediate: 'LittleDistal',
  LittleDistal: 'LittleTip',
};

const CHEST_Z = 0.34;
const DRAW_X = 0.12 + 0.40;
const CIRCLE_R = 0.10;

const CURLS = {
  relaxed: { index: [0.25, 0.20, 0.10], middle: [0.30, 0.25, 0.12], ring: [0.35, 0.28, 0.12], little: [0.40, 0.30, 0.14] },
  open: { index: [0.08, 0.05, 0.02], middle: [0.08, 0.05, 0.02], ring: [0.10, 0.06, 0.03], little: [0.14, 0.08, 0.04] },
  pen: { index: [0.55, 0.85, 0.40], middle: [1.15, 1.45, 0.90], ring: [1.25, 1.55, 0.95], little: [1.30, 1.50, 0.90] },
  point: { index: [0.04, 0.02, 0.00], middle: [1.30, 1.55, 0.95], ring: [1.35, 1.60, 1.00], little: [1.40, 1.55, 0.95] },
  seal: { index: [0.75, 0.95, 0.50], middle: [0.25, 0.18, 0.08], ring: [0.30, 0.20, 0.08], little: [0.40, 0.25, 0.10] },
  fist: { index: [1.20, 1.45, 0.90], middle: [1.25, 1.50, 0.95], ring: [1.30, 1.55, 0.95], little: [1.35, 1.50, 0.90] },
};

const FINGERS = [
  { name: 'index', meta: [0, -0.020, -0.012], mcp: [0, -0.022, 0.030], lens: [0.039, 0.024, 0.017], spread: -0.06 },
  { name: 'middle', meta: [0, -0.002, -0.010], mcp: [0, -0.002, 0.034], lens: [0.044, 0.027, 0.018], spread: 0.00 },
  { name: 'ring', meta: [0, 0.015, -0.012], mcp: [0, 0.016, 0.028], lens: [0.041, 0.025, 0.017], spread: 0.08 },
  { name: 'little', meta: [0, 0.030, -0.016], mcp: [0, 0.032, 0.016], lens: [0.031, 0.018, 0.015], spread: 0.16 },
];

const ACTIONS = [
  // `line` is the inner stroke. It must not be named `stroke`: that key is the stroke duration below.
  ['sigil-line1', { kind: 'sigil', line: 'vertical', hands: ['R'], approach: 0.28, circle: 1.00, lift: 0.16, stroke: 0.45, release: 0.22 }],
  ['sigil-line2', { kind: 'sigil', line: 'horizontal', hands: ['R'], approach: 0.28, circle: 1.00, lift: 0.16, stroke: 0.45, release: 0.22 }],
  ['sigil-line3', { kind: 'sigil', line: 'diagonal', hands: ['R'], approach: 0.28, circle: 1.00, lift: 0.16, stroke: 0.45, release: 0.22 }],
  ['mudra', { kind: 'mudra', hands: ['L', 'R'], approach: 0.30, hold: 0.60 }],
  ['bolt', { kind: 'flick', hands: ['R'], yaw: 0, windup: 0.10, flick: 0.12, recover: 0.20, distance: 0.18 }],
  ['ward-raise', { kind: 'ward', hands: ['L'], rise: 0.25, hold: 1.00 }],
  ['both-palms', { kind: 'palms', hands: ['L', 'R'], rise: 0.25, hold: 0.80 }],
  ['blink-left', { kind: 'flick', hands: ['R'], yaw: -60, windup: 0.08, flick: 0.12, recover: 0.18, distance: 0.16 }],
  ['blink-right', { kind: 'flick', hands: ['R'], yaw: 60, windup: 0.08, flick: 0.12, recover: 0.18, distance: 0.16 }],
  ['blink-back', { kind: 'flick', hands: ['R'], yaw: 180, windup: 0.08, flick: 0.12, recover: 0.18, distance: 0.16 }],
  ['staff-plant', { kind: 'staff', dir: -1, hands: ['L', 'R'], thrust: 0.40, settle: 0.25 }],
  ['staff-lift', { kind: 'staff', dir: 1, hands: ['L', 'R'], thrust: 0.40, settle: 0.25 }],
];
const VARIANTS = ['normal', 'slow', 'sloppy'];

const v3 = (x, y, z) => ({ x, y, z });
const add = (a, b, c) => ({
  x: a.x + b.x + (c ? c.x : 0),
  y: a.y + b.y + (c ? c.y : 0),
  z: a.z + b.z + (c ? c.z : 0),
});
const sub = (a, b) => ({ x: a.x - b.x, y: a.y - b.y, z: a.z - b.z });
const scale = (a, s) => ({ x: a.x * s, y: a.y * s, z: a.z * s });
const dot = (a, b) => a.x * b.x + a.y * b.y + a.z * b.z;
const cross = (a, b) => ({
  x: a.y * b.z - a.z * b.y,
  y: a.z * b.x - a.x * b.z,
  z: a.x * b.y - a.y * b.x,
});
const len = (a) => Math.hypot(a.x, a.y, a.z);
const normalize = (a) => {
  const n = len(a);
  if (n < 1e-12) {
    throw new Error('zero vector');
  }
  return scale(a, 1 / n);
};
const lerp = (a, b, t) => add(scale(a, 1 - t), scale(b, t));
const clamp01 = (u) => (u < 0 ? 0 : u > 1 ? 1 : u);
const ease = (u) => {
  const x = clamp01(u);
  return x * x * (3 - 2 * x);
};

function quatFromBasis(x, y, z) {
  const m00 = x.x;
  const m01 = y.x;
  const m02 = z.x;
  const m10 = x.y;
  const m11 = y.y;
  const m12 = z.y;
  const m20 = x.z;
  const m21 = y.z;
  const m22 = z.z;
  const tr = m00 + m11 + m22;
  let qx;
  let qy;
  let qz;
  let qw;
  if (tr > 0) {
    const s = Math.sqrt(tr + 1) * 2;
    qw = 0.25 * s;
    qx = (m21 - m12) / s;
    qy = (m02 - m20) / s;
    qz = (m10 - m01) / s;
  } else if (m00 > m11 && m00 > m22) {
    const s = Math.sqrt(1 + m00 - m11 - m22) * 2;
    qw = (m21 - m12) / s;
    qx = 0.25 * s;
    qy = (m01 + m10) / s;
    qz = (m02 + m20) / s;
  } else if (m11 > m22) {
    const s = Math.sqrt(1 + m11 - m00 - m22) * 2;
    qw = (m02 - m20) / s;
    qx = (m01 + m10) / s;
    qy = 0.25 * s;
    qz = (m12 + m21) / s;
  } else {
    const s = Math.sqrt(1 + m22 - m00 - m11) * 2;
    qw = (m10 - m01) / s;
    qx = (m02 + m20) / s;
    qy = (m12 + m21) / s;
    qz = 0.25 * s;
  }
  const n = Math.hypot(qx, qy, qz, qw);
  return { x: qx / n, y: qy / n, z: qz / n, w: qw / n };
}

function quatFromZX(zAxis, xHint) {
  const z = normalize(zAxis);
  let x = sub(xHint, scale(z, dot(xHint, z)));
  if (len(x) < 1e-6) {
    const tmp = Math.abs(z.y) < 0.9 ? v3(0, 1, 0) : v3(1, 0, 0);
    x = sub(tmp, scale(z, dot(tmp, z)));
  }
  x = normalize(x);
  const y = cross(z, x);
  return quatFromBasis(x, y, z);
}

function quatRotate(q, v) {
  const { x, y, z, w } = q;
  const uvx = y * v.z - z * v.y;
  const uvy = z * v.x - x * v.z;
  const uvz = x * v.y - y * v.x;
  const uuvx = y * uvz - z * uvy;
  const uuvy = z * uvx - x * uvz;
  const uuvz = x * uvy - y * uvx;
  return {
    x: v.x + 2 * (w * uvx + uuvx),
    y: v.y + 2 * (w * uvy + uuvy),
    z: v.z + 2 * (w * uvz + uuvz),
  };
}

function quatDot(a, b) {
  return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
}

function slerp(a, b, t) {
  let bx = b.x;
  let by = b.y;
  let bz = b.z;
  let bw = b.w;
  let d = quatDot(a, b);
  if (d < 0) {
    bx = -bx;
    by = -by;
    bz = -bz;
    bw = -bw;
    d = -d;
  }
  if (d > 0.9995) {
    return normalizeQuat({
      x: a.x + (bx - a.x) * t,
      y: a.y + (by - a.y) * t,
      z: a.z + (bz - a.z) * t,
      w: a.w + (bw - a.w) * t,
    });
  }
  const theta = Math.acos(Math.min(1, d));
  const s = Math.sin(theta);
  const w1 = Math.sin((1 - t) * theta) / s;
  const w2 = Math.sin(t * theta) / s;
  return normalizeQuat({ x: a.x * w1 + bx * w2, y: a.y * w1 + by * w2, z: a.z * w1 + bz * w2, w: a.w * w1 + bw * w2 });
}

function normalizeQuat(q) {
  const n = Math.hypot(q.x, q.y, q.z, q.w);
  return { x: q.x / n, y: q.y / n, z: q.z / n, w: q.w / n };
}

function rotateAround(v, axis, angle) {
  const k = normalize(axis);
  const c = Math.cos(angle);
  const s = Math.sin(angle);
  const cr = cross(k, v);
  const d = dot(k, v);
  return add(scale(v, c), scale(cr, s), scale(k, d * (1 - c)));
}

function flexDir(dir, angle) {
  const palm = v3(1, 0, 0);
  const axis = cross(palm, dir);
  if (len(axis) < 1e-8) {
    return dir;
  }
  return normalize(rotateAround(dir, axis, -angle));
}

function seedFor(action, variant) {
  let h = 2166136261;
  const s = `${action}\0${variant}`;
  for (let i = 0; i < s.length; i++) {
    h ^= s.charCodeAt(i);
    h = Math.imul(h, 16777619);
  }
  return h >>> 0;
}

function mulberry32(seed) {
  let a = seed >>> 0;
  return () => {
    a |= 0;
    a = (a + 0x6D2B79F5) | 0;
    let t = Math.imul(a ^ (a >>> 15), 1 | a);
    t = (t + Math.imul(t ^ (t >>> 7), 61 | t)) ^ t;
    return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
  };
}

function variantParams(action, variant) {
  const seed = seedFor(action, variant);
  if (variant !== 'sloppy') {
    return { seed, slow: variant === 'slow' ? 1.6 : 1, jitter: 0, distort: 0, overshoot: 0, dips: false, phases: [0, 0, 0, 0, 0] };
  }
  const rng = mulberry32(seed);
  return {
    seed,
    slow: 1,
    jitter: 0.004 + rng() * 0.004,
    distort: 0.10 + rng() * 0.10,
    overshoot: 0.08 + rng() * 0.07,
    dips: true,
    rng,
    phases: [rng() * Math.PI * 2, rng() * Math.PI * 2, rng() * Math.PI * 2, rng() * Math.PI * 2, rng() * Math.PI * 2],
    dipA: rng(),
    dipB: rng(),
  };
}

function durationOf(spec, scale) {
  if (spec.kind === 'sigil') {
    return (spec.approach + spec.circle + spec.lift + spec.stroke + spec.release) * scale;
  }
  if (spec.kind === 'mudra') {
    return (spec.approach + spec.hold) * scale;
  }
  if (spec.kind === 'ward' || spec.kind === 'palms') {
    return (spec.rise + spec.hold) * scale;
  }
  if (spec.kind === 'staff') {
    return (spec.thrust + spec.settle) * scale;
  }
  return (spec.windup + spec.flick + spec.recover) * scale;
}

function sampleCount(duration) {
  return Math.max(2, Math.round(duration * HZ) + 1);
}

function lerpCurls(a, b, t) {
  const out = {};
  for (const name of ['index', 'middle', 'ring', 'little']) {
    out[name] = a[name].map((v, i) => v + (b[name][i] - v) * t);
  }
  return out;
}

function yOf(side, y) {
  return side === 'R' ? y : -y;
}

// Two-bone IK with a fixed pole. Reach is kept short of full extension so the bend cannot flip.
function ik2(start, target, len1, len2, pole) {
  const max = (len1 + len2) * 0.96;
  const min = Math.abs(len1 - len2) + 0.004;
  let aim = sub(target, start);
  let dist = len(aim);
  if (dist < 1e-8) {
    aim = v3(0, 0, 1);
    dist = 1;
  }
  const dir = normalize(aim);
  const clamped = Math.min(max, Math.max(min, dist));
  const cosA = (len1 * len1 + clamped * clamped - len2 * len2) / (2 * len1 * clamped);
  const ang = Math.acos(Math.max(-1, Math.min(1, cosA)));
  let bend = sub(pole, start);
  bend = sub(bend, scale(dir, dot(bend, dir)));
  if (len(bend) < 1e-6) {
    const tmp = Math.abs(dir.x) < 0.9 ? v3(1, 0, 0) : v3(0, 1, 0);
    bend = cross(dir, tmp);
  }
  bend = normalize(bend);
  const midDir = normalize(add(scale(dir, Math.cos(ang)), scale(bend, Math.sin(ang))));
  return [start, add(start, scale(midDir, len1)), add(start, scale(dir, clamped))];
}

function placeThumb(meta, side, pinch, indexTip) {
  const ySign = side === 'R' ? 1 : -1;
  const amount = clamp01(pinch);
  const restDir = normalize(v3(0.25, ySign * -0.85, 0.46));
  const pinchDir = normalize(v3(0.05, ySign * -0.35, 0.93));
  const baseDir = normalize(lerp(restDir, pinchDir, amount));
  const proximal = add(meta, scale(baseDir, 0.046));
  const thumbSide = v3(0, ySign * -1, 0);
  const restTarget = add(meta, scale(thumbSide, 0.062), v3(0.02, 0, 0.02));
  const pinchTarget = add(indexTip, scale(thumbSide, 0.009), v3(0.004, 0, 0.002));
  const target = lerp(restTarget, pinchTarget, amount);
  const pole = add(proximal, v3(1, ySign * -0.25, 0.1));
  const chain = ik2(proximal, target, 0.032, 0.026, pole);
  return [meta, chain[0], chain[1], chain[2]];
}

function slerpAxes(normalA, fingerA, normalB, fingerB, t) {
  const q = slerp(quatFromZX(fingerA, normalA), quatFromZX(fingerB, normalB), clamp01(t));
  return {
    normal: quatRotate(q, v3(1, 0, 0)),
    finger: quatRotate(q, v3(0, 0, 1)),
  };
}

function buildLocal(side, curls, pinch) {
  const joints = Array.from({ length: 26 }, () => v3(0, 0, 0));
  joints[IDX.Palm] = v3(0, 0, 0);
  joints[IDX.Wrist] = v3(0, yOf(side, 0.006), -0.058);
  for (const finger of FINGERS) {
    const meta = v3(finger.meta[0], yOf(side, finger.meta[1]), finger.meta[2]);
    const mcp = v3(finger.mcp[0], yOf(side, finger.mcp[1]), finger.mcp[2]);
    const cap = finger.name[0].toUpperCase() + finger.name.slice(1);
    joints[IDX[`${cap}Metacarpal`]] = meta;
    joints[IDX[`${cap}Proximal`]] = mcp;
    let dir = normalize(v3(0, yOf(side, Math.sin(finger.spread)), Math.cos(finger.spread)));
    let pos = mcp;
    const names = ['Intermediate', 'Distal', 'Tip'];
    // Index/middle/ring/little: proximal is the MCP (already placed). Three bones follow.
    const flex = curls[finger.name];
    for (let i = 0; i < finger.lens.length; i++) {
      dir = flexDir(dir, flex[i]);
      pos = add(pos, scale(dir, finger.lens[i]));
      joints[IDX[`${cap}${names[i]}`]] = pos;
    }
  }
  const thumbMeta = v3(0.010, yOf(side, -0.018), 0.000);
  const thumb = placeThumb(thumbMeta, side, pinch, joints[IDX.IndexTip]);
  joints[IDX.ThumbMetacarpal] = thumb[0];
  joints[IDX.ThumbProximal] = thumb[1];
  joints[IDX.ThumbDistal] = thumb[2];
  joints[IDX.ThumbTip] = thumb[3];
  return joints;
}

function poseHand(side, palmPos, palmNormal, fingerDir, curls, pinch) {
  const q = quatFromZX(fingerDir, palmNormal);
  const local = buildLocal(side, curls, pinch);
  const world = local.map((p) => add(palmPos, quatRotate(q, p)));
  return { world, palmNormal: quatRotate(q, v3(1, 0, 0)) };
}

function jointQuats(world, palmNormal) {
  const quats = Array.from({ length: 26 }, () => ({ x: 0, y: 0, z: 0, w: 1 }));
  const zPalm = sub(world[IDX.MiddleProximal], world[IDX.Palm]);
  quats[IDX.Palm] = quatFromZX(zPalm, palmNormal);
  quats[IDX.Wrist] = quatFromZX(sub(world[IDX.Palm], world[IDX.Wrist]), palmNormal);
  for (const [name, child] of Object.entries(CHILD)) {
    const i = IDX[name];
    const z = sub(world[IDX[child]], world[i]);
    quats[i] = quatFromZX(len(z) < 1e-8 ? v3(0, 0, 1) : z, palmNormal);
  }
  for (const tip of TIP_NAMES) {
    const parentName = Object.keys(CHILD).find((key) => CHILD[key] === tip);
    const z = sub(world[IDX[tip]], world[IDX[parentName]]);
    quats[IDX[tip]] = quatFromZX(len(z) < 1e-8 ? v3(0, 0, 1) : z, palmNormal);
  }
  return quats;
}

function applyJitter(world, t, params) {
  if (params.jitter <= 0) {
    return;
  }
  TIP_NAMES.forEach((name, i) => {
    const phase = params.phases[i];
    const wiggle = v3(
      Math.sin(t * 17 + phase),
      Math.sin(t * 23 + phase * 1.3),
      Math.sin(t * 29 + phase * 0.7),
    );
    const n = len(wiggle) || 1;
    const delta = scale(wiggle, params.jitter / n);
    const p = world[IDX[name]];
    world[IDX[name]] = add(p, delta);
  });
}

function distortAround(point, anchor, params) {
  if (params.distort <= 0) {
    return point;
  }
  const d = sub(point, anchor);
  d.x *= 1 + params.distort * 0.25;
  d.y *= 1 + params.distort;
  d.z *= 1 - params.distort * 0.55;
  d.y += d.z * params.distort * 0.35;
  return add(anchor, d);
}

// Progress that reaches 1+overshoot near the end of the phase, then settles to 1.
function ballistic(u, overshoot) {
  const x = clamp01(u);
  if (overshoot <= 0) {
    return ease(x);
  }
  const peakAt = 0.78;
  if (x <= peakAt) {
    return ease(x / peakAt) * (1 + overshoot);
  }
  const w = (x - peakAt) / (1 - peakAt);
  return (1 + overshoot) + (1 - (1 + overshoot)) * ease(w);
}

function restPose(side) {
  return {
    pos: v3(0.22, side === 'R' ? 0.20 : -0.20, 0.10),
    normal: v3(0, 0, 1),
    finger: v3(1, 0, 0),
    curls: CURLS.relaxed,
    pinch: 0,
  };
}

function palmForTip(side, tipWorld, normal, finger, curls, pinch) {
  const q = quatFromZX(finger, normal);
  const local = buildLocal(side, curls, pinch);
  const offset = quatRotate(q, local[IDX.IndexTip]);
  return sub(tipWorld, offset);
}

function yawDir(deg) {
  const r = (deg * Math.PI) / 180;
  return v3(Math.cos(r), Math.sin(r), 0);
}

function circlePoint(angle, params) {
  const centre = v3(DRAW_X, 0, CHEST_Z);
  const raw = v3(DRAW_X, Math.cos(angle) * CIRCLE_R, CHEST_Z + Math.sin(angle) * CIRCLE_R);
  return distortAround(raw, centre, params);
}

function strokeEnds(stroke, params) {
  const centre = v3(DRAW_X, 0, CHEST_Z);
  let a;
  let b;
  if (stroke === 'vertical') {
    a = v3(DRAW_X, 0, CHEST_Z - 0.07);
    b = v3(DRAW_X, 0, CHEST_Z + 0.07);
  } else if (stroke === 'horizontal') {
    a = v3(DRAW_X, -0.07, CHEST_Z);
    b = v3(DRAW_X, 0.07, CHEST_Z);
  } else {
    a = v3(DRAW_X, -0.06, CHEST_Z - 0.06);
    b = v3(DRAW_X, 0.06, CHEST_Z + 0.06);
  }
  return [distortAround(a, centre, params), distortAround(b, centre, params)];
}

function drawAxes() {
  return {
    normal: normalize(v3(-0.15, -0.95, 0.20)),
    finger: normalize(v3(0.25, -0.10, 0.96)),
  };
}

function flickNormal(dir) {
  let normal = cross(dir, v3(0, 0, 1));
  if (len(normal) < 1e-6) {
    normal = v3(0, -1, 0);
  }
  normal = normalize(normal);
  if (normal.y > 0) {
    normal = scale(normal, -1);
  }
  return normal;
}

function sampleSigil(spec, t, timeScale, params) {
  const approach = spec.approach * timeScale;
  const circle = spec.circle * timeScale;
  const lift = spec.lift * timeScale;
  const strokeT = spec.stroke * timeScale;
  const release = spec.release * timeScale;
  const axes = drawAxes();
  const circleStart = circlePoint(-Math.PI / 2, params);
  const circleEnd = circlePoint(-Math.PI / 2 + Math.PI * 2, params);
  const [strokeStart, strokeEnd] = strokeEnds(spec.line, params);
  const rest = restPose('R');
  const restTip = add(
    rest.pos,
    quatRotate(quatFromZX(rest.finger, rest.normal), buildLocal('R', rest.curls, 0)[IDX.IndexTip]),
  );
  let tip;
  let curls;
  let pinch;
  let normal = axes.normal;
  let finger = axes.finger;
  if (t <= approach) {
    const u = ease(t / approach);
    tip = lerp(restTip, circleStart, u);
    curls = lerpCurls(CURLS.relaxed, CURLS.pen, u);
    const turned = slerpAxes(rest.normal, rest.finger, axes.normal, axes.finger, u);
    normal = turned.normal;
    finger = turned.finger;
    const pinchU = clamp01((t - approach * 0.65) / (approach * 0.35));
    pinch = ease(pinchU);
  } else if (t <= approach + circle) {
    const u = (t - approach) / circle;
    const angle = -Math.PI / 2 + ease(u) * Math.PI * 2;
    tip = circlePoint(angle, params);
    curls = CURLS.pen;
    pinch = 1;
  } else if (t <= approach + circle + lift) {
    const u = (t - approach - circle) / lift;
    tip = lerp(circleEnd, strokeStart, ease(u));
    curls = CURLS.pen;
    const valley = clamp01(u);
    pinch = valley < 0.5 ? 1 - ease(valley / 0.5) : ease((valley - 0.5) / 0.5);
  } else if (t <= approach + circle + lift + strokeT) {
    const u = (t - approach - circle - lift) / strokeT;
    const p = ballistic(u, params.overshoot);
    tip = lerp(strokeStart, strokeEnd, p);
    curls = CURLS.pen;
    pinch = 1;
  } else {
    const u = clamp01((t - approach - circle - lift - strokeT) / release);
    const back = add(strokeEnd, v3(-0.05, 0.02, -0.03));
    tip = lerp(strokeEnd, back, ease(u));
    curls = lerpCurls(CURLS.pen, CURLS.relaxed, ease(u));
    pinch = 1 - ease(u);
  }
  const palm = palmForTip('R', tip, normal, finger, curls, pinch);
  return [{ side: 'R', palm, normal, finger, curls, pinch }];
}

function sampleFlick(spec, t, timeScale, params) {
  const windup = spec.windup * timeScale;
  const flick = spec.flick * timeScale;
  const recover = spec.recover * timeScale;
  const dir = yawDir(spec.yaw);
  const start = distortAround(v3(0.34, 0.14, 0.32), v3(0.34, 0.14, 0.32), { distort: 0 });
  const anchor = v3(0.34, 0.14, 0.32);
  const flicked = distortAround(add(anchor, scale(dir, spec.distance)), anchor, params);
  const flickDelta = sub(flicked, anchor);
  let tip;
  if (t <= windup) {
    const u = ease(t / Math.max(windup, 1e-6));
    tip = add(anchor, scale(flickDelta, -0.18 * u));
  } else if (t <= windup + flick) {
    const u = (t - windup) / flick;
    const p = ballistic(u, params.overshoot);
    tip = lerp(add(anchor, scale(flickDelta, -0.18)), add(anchor, flickDelta), p);
  } else {
    const u = clamp01((t - windup - flick) / recover);
    tip = lerp(add(anchor, flickDelta), add(anchor, scale(flickDelta, 0.35)), ease(u));
  }
  const normal = flickNormal(dir);
  const finger = dir;
  const curls = CURLS.point;
  const pinch = 0;
  const palm = palmForTip('R', tip, normal, finger, curls, pinch);
  return [{ side: 'R', palm, normal, finger, curls, pinch }];
}

function samplePalms(spec, t, timeScale, params) {
  const rise = spec.rise * timeScale;
  const u = t <= rise ? ballistic(t / rise, params.overshoot) : 1;
  const endNormal = v3(1, 0, 0);
  const endFinger = v3(0, 0, 1);
  return ['L', 'R'].map((side) => {
    const start = restPose(side);
    const endPos = v3(0.36, side === 'L' ? -0.14 : 0.14, 0.36);
    const turned = slerpAxes(start.normal, start.finger, endNormal, endFinger, u);
    return {
      side,
      // The right hand's mirrored metacarpals flip the joint-cross normal. Build it
      // with the left layout so both palms face +X the way the ward detector measures.
      build: 'L',
      palm: distortAround(lerp(start.pos, endPos, u), start.pos, params),
      normal: turned.normal,
      finger: turned.finger,
      curls: CURLS.open,
      pinch: 0,
    };
  });
}

function sampleWard(spec, t, timeScale, params) {
  const rise = spec.rise * timeScale;
  const start = restPose('L');
  const endPos = v3(0.36, -0.12, 0.36);
  const endNormal = v3(1, 0, 0);
  const endFinger = v3(0, 0, 1);
  const u = t <= rise ? ballistic(t / rise, params.overshoot) : 1;
  const palmPos = distortAround(lerp(start.pos, endPos, u), start.pos, params);
  const turned = slerpAxes(start.normal, start.finger, endNormal, endFinger, u);
  const normal = turned.normal;
  const finger = turned.finger;
  return [{
    side: 'L',
    palm: palmPos,
    normal,
    finger,
    curls: CURLS.open,
    pinch: 0,
  }];
}

// Rise, then sit nearly still short of the raised pose, then rotate into it.
// The still span is the top of the 0.15–0.30 s range the onset tail has to cross.
const SETTLE_RISE_S = 0.22;
const SETTLE_STILL_S = 0.30;
const SETTLE_ORIENT_S = 0.12;
const SETTLE_HOLD_DEG = 55;
const SETTLE_DURATION_S = SETTLE_RISE_S + SETTLE_STILL_S + SETTLE_ORIENT_S;

function settleAxes(deg) {
  const rad = (deg * Math.PI) / 180;
  return {
    normal: v3(Math.cos(rad), 0, Math.sin(rad)),
    finger: v3(-Math.sin(rad), 0, Math.cos(rad)),
  };
}

function sampleWardSettle(t) {
  const start = restPose('L');
  const endPos = v3(0.36, -0.12, 0.36);
  const held = settleAxes(SETTLE_HOLD_DEG);
  const endNormal = v3(1, 0, 0);
  const endFinger = v3(0, 0, 1);
  const stillStart = SETTLE_RISE_S;
  const orientStart = SETTLE_RISE_S + SETTLE_STILL_S;
  let palm;
  let normal;
  let finger;
  if (t <= stillStart) {
    const u = ease(t / stillStart);
    palm = lerp(start.pos, endPos, u);
    const turned = slerpAxes(start.normal, start.finger, held.normal, held.finger, u);
    normal = turned.normal;
    finger = turned.finger;
  } else if (t <= orientStart) {
    palm = endPos;
    normal = held.normal;
    finger = held.finger;
  } else {
    const u = ease(clamp01((t - orientStart) / SETTLE_ORIENT_S));
    palm = endPos;
    const turned = slerpAxes(held.normal, held.finger, endNormal, endFinger, u);
    normal = turned.normal;
    finger = turned.finger;
  }
  return [{
    side: 'L',
    palm,
    normal,
    finger,
    curls: CURLS.open,
    pinch: 0,
  }];
}

function sampleMudra(spec, t, timeScale, params) {
  const approach = spec.approach * timeScale;
  const u = t >= approach ? 1 : ballistic(t / approach, params.overshoot);
  const orient = clamp01(u);
  const leftRest = restPose('L');
  const rightRest = restPose('R');
  const leftEnd = v3(0.30, -0.02, 0.33);
  const rightEnd = v3(0.30, 0.02, 0.33);
  const leftN = normalize(v3(0.05, 1, 0.08));
  const rightN = normalize(v3(0.05, -1, 0.08));
  const fingers = v3(0, 0, 1);
  const pinch = 0.85 * orient;
  const curls = lerpCurls(CURLS.relaxed, CURLS.seal, orient);
  const leftTurn = slerpAxes(leftRest.normal, leftRest.finger, leftN, fingers, orient);
  const rightTurn = slerpAxes(rightRest.normal, rightRest.finger, rightN, fingers, orient);
  return [
    {
      side: 'L',
      palm: distortAround(lerp(leftRest.pos, leftEnd, u), leftRest.pos, params),
      normal: leftTurn.normal,
      finger: leftTurn.finger,
      curls,
      pinch,
    },
    {
      side: 'R',
      palm: distortAround(lerp(rightRest.pos, rightEnd, u), rightRest.pos, params),
      normal: rightTurn.normal,
      finger: rightTurn.finger,
      curls,
      pinch,
    },
  ];
}

function sampleStaff(spec, t, timeScale, params) {
  const thrust = spec.thrust * timeScale;
  const u = t >= thrust ? 1 : ease(t / Math.max(thrust, 1e-6));
  const zTopL = 0.48;
  const zBotL = 0.23;
  const zTopR = 0.45;
  const zBotR = 0.20;
  const dropping = spec.dir < 0;
  const leftStart = v3(0.34, -0.04, dropping ? zTopL : zBotL);
  const leftEnd = v3(0.34, -0.04, dropping ? zBotL : zTopL);
  const rightStart = v3(0.34, 0.04, dropping ? zTopR : zBotR);
  const rightEnd = v3(0.34, 0.04, dropping ? zBotR : zTopR);
  const finger = v3(1, 0, 0);
  return [
    {
      side: 'L',
      palm: distortAround(lerp(leftStart, leftEnd, u), leftStart, params),
      normal: v3(0, 1, 0),
      finger,
      curls: CURLS.fist,
      pinch: 0.90,
    },
    {
      side: 'R',
      palm: distortAround(lerp(rightStart, rightEnd, u), rightStart, params),
      normal: v3(0, -1, 0),
      finger,
      curls: CURLS.fist,
      pinch: 0.90,
    },
  ];
}

function sampleAction(spec, t, timeScale, params) {
  if (spec.kind === 'sigil') {
    return sampleSigil(spec, t, timeScale, params);
  }
  if (spec.kind === 'flick') {
    return sampleFlick(spec, t, timeScale, params);
  }
  if (spec.kind === 'ward') {
    return sampleWard(spec, t, timeScale, params);
  }
  if (spec.kind === 'palms') {
    return samplePalms(spec, t, timeScale, params);
  }
  if (spec.kind === 'staff') {
    return sampleStaff(spec, t, timeScale, params);
  }
  return sampleMudra(spec, t, timeScale, params);
}

function buildClip(action, spec, variant) {
  const params = variantParams(action, variant);
  const duration = durationOf(spec, params.slow);
  const n = sampleCount(duration);
  const frames = [];
  for (let i = 0; i < n; i++) {
    const t = i / HZ;
    const hands = sampleAction(spec, t, params.slow, params);
    for (const hand of hands) {
      const posed = poseHand(hand.build || hand.side, hand.palm, hand.normal, hand.finger, hand.curls, hand.pinch);
      applyJitter(posed.world, t, params);
      const quats = jointQuats(posed.world, posed.palmNormal);
      const joints = posed.world.map((p, j) => [p.x, p.y, p.z, quats[j].x, quats[j].y, quats[j].z, quats[j].w]);
      frames.push({ t, hand: hand.side, conf: params.dips ? 0.96 : 1, pinch: hand.pinch, joints });
    }
  }
  if (params.dips) {
    const hands = spec.hands.length;
    const per = n;
    const pick = (r) => 4 + Math.floor(r * Math.max(1, per - 10));
    let a = pick(params.dipA);
    let b = pick(params.dipB);
    if (Math.abs(a - b) < 4) {
      b = Math.min(per - 3, a + 5);
    }
    for (const centre of [a, b]) {
      for (let k = -1; k <= 1; k++) {
        const frameIndex = centre + k;
        if (frameIndex < 0 || frameIndex >= per) {
          continue;
        }
        for (let h = 0; h < hands; h++) {
          frames[frameIndex * hands + h].conf = 0.4;
        }
      }
    }
  }
  stabilize(frames, spec.hands);
  return { frames, params, duration: (n - 1) / HZ };
}

function stabilize(frames, hands) {
  const prev = {};
  for (const frame of frames) {
    for (let j = 0; j < 26; j++) {
      const q = {
        x: frame.joints[j][3],
        y: frame.joints[j][4],
        z: frame.joints[j][5],
        w: frame.joints[j][6],
      };
      const key = `${frame.hand}:${j}`;
      if (prev[key] && quatDot(prev[key], q) < 0) {
        q.x = -q.x;
        q.y = -q.y;
        q.z = -q.z;
        q.w = -q.w;
      }
      prev[key] = q;
      frame.joints[j][3] = q.x;
      frame.joints[j][4] = q.y;
      frame.joints[j][5] = q.z;
      frame.joints[j][6] = q.w;
    }
  }
}

function num(n) {
  if (!Number.isFinite(n)) {
    throw new Error(`non-finite number ${n}`);
  }
  let s = n.toFixed(6);
  if (s === '-0.000000') {
    s = '0.000000';
  }
  return s;
}

function frameLine(frame) {
  const joints = frame.joints.map((joint) => `[${joint.map(num).join(',')}]`).join(',');
  return `{"t":${num(frame.t)},"hand":"${frame.hand}","conf":${num(frame.conf)},"pinch":${num(frame.pinch)},"joints":[${joints}]}`;
}

function headerLine(action, variant, hands, seed, source = 'synthetic') {
  const kp = KEYPOINTS.map((name) => `"${name}"`).join(',');
  const hs = hands.map((hand) => `"${hand}"`).join(',');
  return `{"schema":"mage-arena/hand-clip@1","name":${JSON.stringify(action)},"action":${JSON.stringify(action)},"variant":"${variant}","hands":[${hs}],"hz":72,"space":${JSON.stringify(SPACE)},"keypoints":[${kp}],"source":${JSON.stringify(source)},"seed":${seed}}`;
}

// Frames for a clip whose sample function returns the same hand poses as the quick-action sampler.
function renderClip({ action, variant, hands, seed, source, params, duration, sampleAt }) {
  const n = sampleCount(duration);
  const frames = [];
  for (let i = 0; i < n; i++) {
    const t = i / HZ;
    const posedHands = sampleAt(Math.min(t, duration));
    for (const hand of posedHands) {
      const posed = poseHand(hand.build || hand.side, hand.palm, hand.normal, hand.finger, hand.curls, hand.pinch);
      applyJitter(posed.world, t, params);
      const quats = jointQuats(posed.world, posed.palmNormal);
      const joints = posed.world.map((p, j) => [p.x, p.y, p.z, quats[j].x, quats[j].y, quats[j].z, quats[j].w]);
      frames.push({ t, hand: hand.side, conf: params.dips ? 0.96 : 1, pinch: hand.pinch, joints });
    }
  }
  if (params.dips) {
    const handCount = hands.length;
    const per = n;
    const pick = (r) => 4 + Math.floor(r * Math.max(1, per - 10));
    let a = pick(params.dipA ?? 0);
    let b = pick(params.dipB ?? 0);
    if (Math.abs(a - b) < 4) {
      b = Math.min(per - 3, a + 5);
    }
    for (const centre of [a, b]) {
      for (let k = -1; k <= 1; k++) {
        const frameIndex = centre + k;
        if (frameIndex < 0 || frameIndex >= per) {
          continue;
        }
        for (let h = 0; h < handCount; h++) {
          frames[frameIndex * handCount + h].conf = 0.4;
        }
      }
    }
  }
  stabilize(frames, hands);
  for (const frame of frames) {
    for (let j = 0; j < 26; j++) {
      const q = frame.joints[j].slice(3).map((c) => Number(c.toFixed(6)));
      const qn = Math.hypot(q[0], q[1], q[2], q[3]);
      if (Math.abs(qn - 1) > 1e-3) {
        throw new Error(`${action} ${variant}: rounded quat norm ${qn} at ${KEYPOINTS[j]}`);
      }
    }
    if (frame.pinch < -1e-6 || frame.pinch > 1 + 1e-6) {
      throw new Error(`${action} pinch ${frame.pinch}`);
    }
  }
  const lines = [headerLine(action, variant, hands, seed, source)];
  for (const frame of frames) {
    lines.push(frameLine(frame));
  }
  return { text: `${lines.join('\n')}\n`, frames, duration: (n - 1) / HZ };
}

function tip(frame) {
  const j = frame.joints[IDX.IndexTip];
  return v3(j[0], j[1], j[2]);
}

function thumbTip(frame) {
  const j = frame.joints[IDX.ThumbTip];
  return v3(j[0], j[1], j[2]);
}

function palmNormalOf(frame) {
  const q = { x: frame.joints[0][3], y: frame.joints[0][4], z: frame.joints[0][5], w: frame.joints[0][6] };
  return quatRotate(q, v3(1, 0, 0));
}

function assertClip(action, spec, variant, clip) {
  const { frames } = clip;
  const hands = spec.hands;
  if (frames.length % hands.length !== 0) {
    throw new Error(`${action} ${variant}: frame count not divisible by hands`);
  }
  let worst = 0;
  let worstWhere = '';
  const prev = {};
  const prevPalm = {};
  for (const frame of frames) {
    const palmNow = v3(frame.joints[0][0], frame.joints[0][1], frame.joints[0][2]);
    const palmStep = prevPalm[frame.hand] ? len(sub(palmNow, prevPalm[frame.hand])) : 0;
    prevPalm[frame.hand] = palmNow;
    for (let j = 0; j < 26; j++) {
      const q = frame.joints[j].slice(3);
      const n = Math.hypot(q[0], q[1], q[2], q[3]);
      if (Math.abs(n - 1) > 1e-6) {
        throw new Error(`${action} ${variant}: quat norm ${n} at ${KEYPOINTS[j]}`);
      }
      const rounded = q.map((c) => Number(c.toFixed(6)));
      const rn = Math.hypot(...rounded);
      if (Math.abs(rn - 1) > 1e-3) {
        throw new Error(`${action} ${variant}: rounded quat norm ${rn}`);
      }
      const p = v3(frame.joints[j][0], frame.joints[j][1], frame.joints[j][2]);
      const key = `${frame.hand}:${j}`;
      if (prev[key]) {
        const step = len(sub(p, prev[key]));
        if (step > 0.12) {
          throw new Error(`${action} ${variant}: ${KEYPOINTS[j]} teleported ${step.toFixed(4)} m at t=${frame.t.toFixed(3)}`);
        }
        const relative = step - palmStep;
        if (relative > worst) {
          worst = relative;
          worstWhere = `${KEYPOINTS[j]} t=${frame.t.toFixed(3)} palm=${palmStep.toFixed(4)}`;
        }
      }
      prev[key] = p;
    }
    if (frame.pinch < -1e-6 || frame.pinch > 1 + 1e-6) {
      throw new Error(`${action} pinch ${frame.pinch}`);
    }
  }
  if (worst > 0.025) {
    throw new Error(`${action} ${variant}: joint moved ${worst.toFixed(4)} m past the palm at ${worstWhere}`);
  }
  if (variant !== 'normal') {
    return;
  }
  const right = frames.filter((frame) => frame.hand === 'R');
  const left = frames.filter((frame) => frame.hand === 'L');
  if (spec.kind === 'sigil') {
    checkSigil(action, spec, right);
  } else if (spec.kind === 'flick') {
    checkFlick(action, spec, right);
  } else if (spec.kind === 'ward') {
    checkWard(left);
  } else if (spec.kind === 'palms') {
    checkPalms(left, right);
  } else if (spec.kind === 'staff') {
    checkStaff(action, spec, left, right);
  } else {
    checkMudra(left, right);
  }
}

function checkPinchDistance(action, frames) {
  for (const frame of frames) {
    const d = len(sub(thumbTip(frame), tip(frame)));
    if (frame.pinch > 0.95 && d > 0.02) {
      throw new Error(`${action}: pinch ${frame.pinch} but thumb-index is ${d.toFixed(4)} m`);
    }
    if (frame.pinch < 0.05 && d < 0.025) {
      throw new Error(`${action}: pinch ${frame.pinch} but thumb-index is only ${d.toFixed(4)} m`);
    }
  }
}

function checkSigil(action, spec, frames) {
  const approach = spec.approach;
  const circleEnd = approach + spec.circle;
  const liftEnd = circleEnd + spec.lift;
  const strokeEnd = liftEnd + spec.stroke;
  const circle = frames.filter((frame) => frame.t >= approach + 0.02 && frame.t <= circleEnd - 0.02);
  let minR = Infinity;
  let maxR = 0;
  for (const frame of circle) {
    const p = tip(frame);
    const r = Math.hypot(p.y, p.z - CHEST_Z);
    minR = Math.min(minR, r);
    maxR = Math.max(maxR, r);
  }
  if (Math.abs(maxR - CIRCLE_R) > 0.01 || Math.abs(minR - CIRCLE_R) > 0.01) {
    throw new Error(`${action}: circle radius ${minR.toFixed(3)}..${maxR.toFixed(3)}`);
  }
  const xs = circle.map((frame) => tip(frame).x);
  const xSpread = Math.max(...xs) - Math.min(...xs);
  if (xSpread > 0.01) {
    throw new Error(`${action}: circle is not on a frontal plane, x spread ${xSpread}`);
  }
  if (circle.some((frame) => frame.pinch < 0.99)) {
    throw new Error(`${action}: pen lifted during the circle`);
  }
  const midLift = frames.reduce((best, frame) => {
    const target = (circleEnd + liftEnd) / 2;
    return Math.abs(frame.t - target) < Math.abs(best.t - target) ? frame : best;
  }, frames[0]);
  if (midLift.pinch > 0.15) {
    throw new Error(`${action}: lift pinch ${midLift.pinch}`);
  }
  const stroke = frames.filter((frame) => frame.t >= liftEnd + 0.02 && frame.t <= strokeEnd - 0.02);
  const a = tip(stroke[0]);
  const b = tip(stroke[stroke.length - 1]);
  const dy = b.y - a.y;
  const dz = b.z - a.z;
  if (spec.line === 'vertical' && !(dz > 0.10 && Math.abs(dy) < 0.03)) {
    throw new Error(`${action}: vertical stroke dy ${dy} dz ${dz}`);
  }
  if (spec.line === 'horizontal' && !(dy > 0.10 && Math.abs(dz) < 0.03)) {
    throw new Error(`${action}: horizontal stroke dy ${dy} dz ${dz}`);
  }
  if (spec.line === 'diagonal' && !(dy > 0.08 && dz > 0.08)) {
    throw new Error(`${action}: diagonal stroke dy ${dy} dz ${dz}`);
  }
  if (frames[frames.length - 1].pinch > 0.05) {
    throw new Error(`${action}: pinch not released`);
  }
  checkPinchDistance(action, frames);
}

function checkFlick(action, spec, frames) {
  const start = spec.windup;
  const end = spec.windup + spec.flick;
  const a = tip(frames.find((frame) => frame.t >= start - 1e-9));
  const b = tip(frames.find((frame) => frame.t >= end - 1e-9) || frames[frames.length - 1]);
  const delta = sub(b, a);
  const dir = yawDir(spec.yaw);
  const along = dot(normalize(delta), dir);
  if (along < 0.95 || len(delta) < spec.distance * 0.75) {
    throw new Error(`${action}: flick delta ${delta.x.toFixed(3)},${delta.y.toFixed(3)},${delta.z.toFixed(3)} along ${along.toFixed(3)}`);
  }
}

function checkWard(frames) {
  const start = frames[0];
  const risen = frames.find((frame) => frame.t >= 0.25 - 1e-9);
  const end = frames[frames.length - 1];
  if (tip(risen).z - tip(start).z < 0.18 && risen.joints[0][2] - start.joints[0][2] < 0.18) {
    throw new Error(`ward rise too small palm dz ${risen.joints[0][2] - start.joints[0][2]}`);
  }
  const n = palmNormalOf(end);
  if (n.x < 0.8) {
    throw new Error(`ward palm normal ${n.x.toFixed(3)},${n.y.toFixed(3)},${n.z.toFixed(3)}`);
  }
  const hold = frames.filter((frame) => frame.t >= 0.30);
  let drift = 0;
  for (let i = 1; i < hold.length; i++) {
    drift = Math.max(drift, Math.abs(hold[i].joints[0][2] - hold[0].joints[0][2]));
  }
  if (drift > 0.005) {
    throw new Error(`ward hold drift ${drift}`);
  }
}

function checkPalms(left, right) {
  if (left.length === 0 || right.length === 0) {
    throw new Error('both-palms needs both hands');
  }
  checkWard(left);
  checkWard(right);
  if (!isDetectorRaised(left[left.length - 1]) || !isDetectorRaised(right[right.length - 1])) {
    throw new Error('both-palms end pose is not a raised palm');
  }
  const gap = Math.abs(framePalm(left[left.length - 1]).y - framePalm(right[right.length - 1]).y);
  if (gap < 0.20) {
    throw new Error(`both-palms hands only ${gap.toFixed(3)} m apart`);
  }
}

function checkStaff(action, spec, left, right) {
  if (left.length === 0 || right.length === 0) {
    throw new Error(`${action}: staff needs both hands`);
  }
  const dz = (framePalm(left[left.length - 1]).z - framePalm(left[0]).z
    + framePalm(right[right.length - 1]).z - framePalm(right[0]).z) / 2;
  const expected = spec.dir * 0.25;
  if (Math.abs(dz - expected) > 0.04) {
    throw new Error(`${action}: staff dz ${dz.toFixed(3)} expected ${expected.toFixed(3)}`);
  }
  const count = Math.min(left.length, right.length);
  for (let i = 0; i < count; i++) {
    const gap = Math.abs(framePalm(left[i]).y - framePalm(right[i]).y);
    if (gap > 0.12) {
      throw new Error(`${action}: staff hands ${gap.toFixed(3)} m apart`);
    }
    if (left[i].pinch < 0.85 || right[i].pinch < 0.85) {
      throw new Error(`${action}: staff pinch ${left[i].pinch}`);
    }
  }
}

function checkMudra(left, right) {
  const holdL = left.filter((frame) => frame.t >= 0.30);
  const holdR = right.filter((frame) => frame.t >= 0.30);
  let drift = 0;
  for (let i = 1; i < holdL.length; i++) {
    drift = Math.max(drift, len(sub(v3(holdL[i].joints[0][0], holdL[i].joints[0][1], holdL[i].joints[0][2]), v3(holdL[0].joints[0][0], holdL[0].joints[0][1], holdL[0].joints[0][2]))));
  }
  if (drift > 0.004) {
    throw new Error(`mudra hold drift ${drift}`);
  }
  const nL = palmNormalOf(holdL[holdL.length - 1]);
  const nR = palmNormalOf(holdR[holdR.length - 1]);
  if (nL.y < 0.8 || nR.y > -0.8) {
    throw new Error(`mudra palms ${nL.y.toFixed(2)} ${nR.y.toFixed(2)}`);
  }
  checkPinchDistance('mudra', holdL.concat(holdR));
}

function framePalm(frame) {
  const j = frame.joints[0];
  return v3(j[0], j[1], j[2]);
}

function frameJoint(frame, index) {
  const j = frame.joints[index];
  return v3(j[0], j[1], j[2]);
}

// Same construction as FWardDetector::PalmNormal: index/little metacarpals off the wrist.
function detectorPalmNormal(frame) {
  const wrist = frameJoint(frame, IDX.Wrist);
  const index = frameJoint(frame, IDX.IndexMetacarpal);
  const little = frameJoint(frame, IDX.LittleMetacarpal);
  const crossed = cross(sub(index, wrist), sub(little, wrist));
  if (len(crossed) < 1e-8) {
    return palmNormalOf(frame);
  }
  return normalize(crossed);
}

function angleToForwardDeg(normal) {
  const d = Math.max(-1, Math.min(1, normal.x));
  return (Math.acos(d) * 180) / Math.PI;
}

function isDetectorRaised(frame) {
  return angleToForwardDeg(detectorPalmNormal(frame)) <= 35 && framePalm(frame).z >= 0.20;
}

function palmSpeeds(frames) {
  const speeds = [0];
  for (let i = 1; i < frames.length; i++) {
    const dt = frames[i].t - frames[i - 1].t;
    speeds.push(len(sub(framePalm(frames[i]), framePalm(frames[i - 1]))) / dt);
  }
  return speeds;
}

function lerpFramePalm(frames, sourceT) {
  if (sourceT <= frames[0].t) {
    return framePalm(frames[0]);
  }
  const last = frames[frames.length - 1];
  if (sourceT >= last.t) {
    return framePalm(last);
  }
  let hi = 1;
  while (hi < frames.length && frames[hi].t < sourceT) {
    hi += 1;
  }
  const before = frames[hi - 1];
  const after = frames[hi];
  const span = after.t - before.t;
  const alpha = span > 0 ? (sourceT - before.t) / span : 0;
  return lerp(framePalm(before), framePalm(after), alpha);
}

function lerpRaised(frames, sourceT) {
  if (sourceT <= frames[0].t) {
    return isDetectorRaised(frames[0]);
  }
  const last = frames[frames.length - 1];
  if (sourceT >= last.t) {
    return isDetectorRaised(last);
  }
  let hi = 1;
  while (hi < frames.length && frames[hi].t < sourceT) {
    hi += 1;
  }
  const before = frames[hi - 1];
  const after = frames[hi];
  const span = after.t - before.t;
  const alpha = span > 0 ? (sourceT - before.t) / span : 0;
  const wrist = lerp(frameJoint(before, IDX.Wrist), frameJoint(after, IDX.Wrist), alpha);
  const index = lerp(frameJoint(before, IDX.IndexMetacarpal), frameJoint(after, IDX.IndexMetacarpal), alpha);
  const little = lerp(frameJoint(before, IDX.LittleMetacarpal), frameJoint(after, IDX.LittleMetacarpal), alpha);
  const palm = lerp(framePalm(before), framePalm(after), alpha);
  const crossed = cross(sub(index, wrist), sub(little, wrist));
  const normal = len(crossed) < 1e-8 ? detectorPalmNormal(after) : normalize(crossed);
  return angleToForwardDeg(normal) <= 35 && palm.z >= 0.20;
}

// Mirrors FWardDetector::FindOnset, including the lower-time bound and the
// elevated-only bridge. The fast walk stays height-free: the rise starts in the lap.
function findOnset(history, tail, bound) {
  let index = history.length - 1;
  const confirm = history[index].time;
  const lowerH = 0.16;
  while (index > 0
    && history[index].speed < 0.30
    && history[index].height >= lowerH
    && history[index - 1].height >= lowerH
    && (confirm - history[index].time) <= tail
    && history[index - 1].time >= bound - 1e-9) {
    index -= 1;
  }
  while (index > 0
    && history[index - 1].speed >= 0.30
    && history[index - 1].time >= bound - 1e-9) {
    index -= 1;
  }
  return history[index].time;
}

function geometricOnset(frames, speeds) {
  for (let i = 1; i < frames.length; i++) {
    if (speeds[i] >= 0.30) {
      return frames[i].t;
    }
  }
  return -1;
}

function warpedTrial(frames, latency, tail, geoOnset, confirmSource) {
  const frameDt = 1 / HZ;
  const anchor = geoOnset + 2 * frameDt;
  const span = confirmSource - anchor;
  const scale = span / (span + latency);
  const history = [];
  const push = (source, output) => {
    const palm = lerpFramePalm(frames, source);
    let speed = 0;
    if (history.length > 0) {
      const dt = output - history[history.length - 1].time;
      if (dt >= 1e-6) {
        speed = len(sub(palm, history[history.length - 1].palm)) / dt;
      }
    }
    history.push({ time: output, palm, height: palm.z, speed, raised: lerpRaised(frames, source) });
  };
  push(0, 0);
  let output = 0;
  let guard = 0;
  while (!history[history.length - 1].raised && output < confirmSource + latency + 0.5 && guard < 500) {
    output += frameDt;
    let source = output;
    if (output > anchor) {
      source = anchor + (output - anchor) * scale;
    }
    source = Math.max(0, Math.min(frames[frames.length - 1].t, source));
    push(source, output);
    guard += 1;
  }
  const raised = history[history.length - 1].raised;
  const onset = raised ? findOnset(history, tail, -1e9) : -1;
  return { raised, onset, confirm: raised ? output : -1, history };
}

function assertSettle(frames) {
  if (!(SETTLE_STILL_S >= 0.15 && SETTLE_STILL_S <= 0.30)) {
    throw new Error(`settle still ${SETTLE_STILL_S} is outside 0.15-0.30 s`);
  }
  const speeds = palmSpeeds(frames);
  const orientStart = SETTLE_RISE_S + SETTLE_STILL_S;
  let sawFast = false;
  let maxStillSpeed = 0;
  let firstRaised = -1;
  for (let i = 0; i < frames.length; i++) {
    const authored = palmNormalOf(frames[i]);
    const detected = detectorPalmNormal(frames[i]);
    if (dot(normalize(authored), detected) < 0.99) {
      throw new Error(`settle palm normal diverges at t=${frames[i].t.toFixed(3)}`);
    }
    if (speeds[i] >= 0.30) {
      sawFast = true;
    }
    if (frames[i].t >= SETTLE_RISE_S - 1e-9 && frames[i].t <= orientStart + 1e-9) {
      maxStillSpeed = Math.max(maxStillSpeed, speeds[i]);
    }
    if (firstRaised < 0 && isDetectorRaised(frames[i])) {
      firstRaised = frames[i].t;
    }
  }
  if (!sawFast) {
    throw new Error('settle rise never reaches onset speed');
  }
  if (maxStillSpeed >= 0.30) {
    throw new Error(`settle is not nearly still (${maxStillSpeed.toFixed(3)} m/s)`);
  }
  if (firstRaised < orientStart - 1e-9) {
    throw new Error(`settle confirms at ${firstRaised.toFixed(3)} before the orient`);
  }
  const geo = geometricOnset(frames, speeds);
  if (!(firstRaised > geo + SETTLE_STILL_S - 1 / HZ)) {
    throw new Error(`settle confirm ${firstRaised} is not after a ${SETTLE_STILL_S}s still from onset ${geo}`);
  }
  let fastRun = false;
  let broke = false;
  for (let i = 1; i < speeds.length; i++) {
    if (speeds[i] >= 0.30) {
      if (broke) {
        throw new Error(`settle rise speed dips below onset at t=${frames[i].t.toFixed(3)}`);
      }
      fastRun = true;
    } else if (fastRun) {
      broke = true;
    }
  }
  return { speeds, geo, firstRaised };
}

function chooseOnsetTail(settleFrames, normalFrames) {
  const settle = assertSettle(settleFrames);
  const normalSpeeds = palmSpeeds(normalFrames);
  const normalOnset = geometricOnset(normalFrames, normalSpeeds);
  let normalConfirm = -1;
  for (const frame of normalFrames) {
    if (isDetectorRaised(frame)) {
      normalConfirm = frame.t;
      break;
    }
  }
  const frameDt = 1 / HZ;
  const latencies = [0, 0.02, 0.04, 0.06];
  const probe = (tail) => {
    for (const latency of latencies) {
      const trial = warpedTrial(settleFrames, latency, tail, settle.geo, settle.firstRaised);
      if (!trial.raised || Math.abs(trial.onset - settle.geo) > frameDt + 1e-6) {
        return false;
      }
      const normal = warpedTrial(normalFrames, latency, tail, normalOnset, normalConfirm);
      if (!normal.raised || Math.abs(normal.onset - normalOnset) > frameDt + 1e-6) {
        return false;
      }
    }
    return true;
  };
  let lo = 0;
  let hi = 0.9;
  if (!probe(hi)) {
    throw new Error('settle onset is not recovered even with a 0.90 s tail');
  }
  for (let i = 0; i < 24; i++) {
    const mid = (lo + hi) / 2;
    if (probe(mid)) {
      hi = mid;
    } else {
      lo = mid;
    }
  }
  const needed = hi;
  const chosen = Math.ceil(needed * HZ - 1e-9) / HZ;
  if (!probe(chosen)) {
    throw new Error(`rounded tail ${chosen} does not recover settle onset`);
  }
  const at60 = warpedTrial(settleFrames, 0.06, chosen, settle.geo, settle.firstRaised);
  let lastFast = settle.geo;
  for (let i = 0; i < at60.history.length; i++) {
    if (at60.history[i].speed >= 0.30 && at60.history[i].time <= at60.confirm) {
      lastFast = at60.history[i].time;
    }
  }
  return {
    needed,
    chosen,
    gapAt60: at60.confirm - lastFast,
    onsetAt60: at60.onset,
    geo: settle.geo,
    confirm: settle.firstRaised,
    stillSpeed: null,
  };
}

function writeSettleClip(outDir) {
  const params = variantParams('ward-raise', 'normal');
  const rendered = renderClip({
    action: 'ward-raise',
    variant: 'normal',
    hands: ['L'],
    seed: params.seed,
    source: 'synthetic',
    params,
    duration: SETTLE_DURATION_S,
    sampleAt: (t) => sampleWardSettle(Math.min(t, SETTLE_DURATION_S)),
  });
  const normalText = fs.readFileSync(path.join(outDir, 'ward-raise.normal.jsonl'), 'utf8');
  const left = normalText.trim().split('\n').slice(1).map((line) => JSON.parse(line));
  const dir = path.join(outDir, 'ward');
  fs.mkdirSync(dir, { recursive: true });
  const filePath = path.join(dir, 'ward-raise.normal.settle.jsonl');
  fs.writeFileSync(filePath, rendered.text, { encoding: 'utf8' });
  // Measure the tail on the rounded bytes the detector will load, not the in-memory poses.
  const rounded = fs.readFileSync(filePath, 'utf8').trim().split('\n').slice(1).map((line) => JSON.parse(line));
  const tail = chooseOnsetTail(rounded, left);
  const chosenFrames = Math.round(tail.chosen * HZ);
  if (chosenFrames !== 29) {
    throw new Error(`settle tail moved to ${tail.chosen.toFixed(6)} s (${chosenFrames}/72); update WardDetector.h OnsetTailS`);
  }
  process.stdout.write(
    `settle clip still=${SETTLE_STILL_S.toFixed(2)}s confirm=${tail.confirm.toFixed(4)} onset=${tail.geo.toFixed(4)} neededTail=${tail.needed.toFixed(4)} chosenTail=${tail.chosen.toFixed(4)} gapAt60=${tail.gapAt60.toFixed(4)}\n`,
  );
  return tail;
}

function selfTestMath() {
  const q = quatFromZX(v3(0, 0, 1), v3(1, 0, 0));
  const rx = quatRotate(q, v3(1, 0, 0));
  const rz = quatRotate(q, v3(0, 0, 1));
  if (len(sub(rx, v3(1, 0, 0))) > 1e-6 || len(sub(rz, v3(0, 0, 1))) > 1e-6) {
    throw new Error(`quat roundtrip failed ${JSON.stringify(rx)} ${JSON.stringify(rz)}`);
  }
}

function writeCanonical() {
  const here = path.dirname(fileURLToPath(import.meta.url));
  const outDir = path.resolve(here, '..', '..', 'Game', 'Clips');
  fs.mkdirSync(outDir, { recursive: true });
  const normals = new Map();
  for (const variant of VARIANTS) {
    for (const [action, spec] of ACTIONS) {
      const clip = buildClip(action, spec, variant);
      assertClip(action, spec, variant, clip);
      if (variant === 'normal') {
        normals.set(action, clip);
      }
      if (variant === 'slow') {
        const base = durationOf(spec, 1);
        const ratio = clip.duration / durationOf(spec, 1);
        if (Math.abs(ratio - 1.6) > 0.02 && Math.abs(clip.duration - base * 1.6) > 2 / HZ) {
          throw new Error(`${action} slow duration ratio ${ratio}`);
        }
      }
      if (variant === 'sloppy') {
        if (!clip.frames.some((frame) => frame.conf === 0.4)) {
          throw new Error(`${action} sloppy has no confidence dip`);
        }
        const normal = normals.get(action);
        const a = tip(normal.frames.find((frame) => frame.hand === spec.hands[0] && frame.t > 0.2) || normal.frames[0]);
        const b = tip(clip.frames.find((frame) => frame.hand === spec.hands[0] && frame.t > 0.2) || clip.frames[0]);
        if (spec.kind === 'sigil' && len(sub(a, b)) < 0.004) {
          throw new Error(`${action} sloppy path matches normal`);
        }
      }
      const lines = [headerLine(action, variant, spec.hands, clip.params.seed)];
      for (const frame of clip.frames) {
        lines.push(frameLine(frame));
      }
      const text = `${lines.join('\n')}\n`;
      fs.writeFileSync(path.join(outDir, `${action}.${variant}.jsonl`), text, { encoding: 'utf8' });
    }
  }
  writeSettleClip(outDir);
  process.stdout.write(`wrote ${ACTIONS.length * VARIANTS.length} clips to Game/Clips\n`);
}

function parseArgs(argv) {
  let templates = false;
  let corpus = null;
  let seed = null;
  for (let i = 2; i < argv.length; i++) {
    const arg = argv[i];
    if (arg === '--templates') {
      templates = true;
    } else if (arg === '--corpus') {
      corpus = Number(argv[++i]);
      if (!Number.isInteger(corpus) || corpus < 1) {
        throw new Error('--corpus needs a positive integer');
      }
    } else if (arg === '--seed') {
      seed = Number(argv[++i]);
      if (!Number.isInteger(seed) || seed < 0 || seed > 4294967295) {
        throw new Error('--seed must be an integer in [0, 4294967295]');
      }
    } else {
      throw new Error(`unknown argument ${arg}`);
    }
  }
  if (templates && corpus != null) {
    throw new Error('pass --templates or --corpus, not both');
  }
  if (corpus != null && seed == null) {
    throw new Error('--corpus requires --seed');
  }
  return { templates, corpus, seed };
}

function isDirectInvocation() {
  return !!process.argv[1] && import.meta.url === pathToFileURL(process.argv[1]).href;
}

async function main() {
  selfTestMath();
  const args = parseArgs(process.argv);
  if (args.templates || args.corpus != null) {
    const corpus = await import('./corpus.mjs');
    if (args.templates) {
      corpus.writeTemplates();
    } else {
      corpus.writeCorpus(args.corpus, args.seed);
    }
    return;
  }
  writeCanonical();
}

if (isDirectInvocation()) {
  // Not a top-level await: corpus.mjs imports this module, and --templates imports corpus
  // while this evaluation would still be suspended.
  main().catch((error) => {
    console.error(error);
    process.exitCode = 1;
  });
}

export {
  HZ,
  SPACE,
  KEYPOINTS,
  IDX,
  DRAW_X,
  CHEST_Z,
  CIRCLE_R,
  CURLS,
  ACTIONS,
  VARIANTS,
  v3,
  add,
  sub,
  scale,
  len,
  normalize,
  lerp,
  clamp01,
  ease,
  mulberry32,
  seedFor,
  poseHand,
  palmForTip,
  applyJitter,
  jointQuats,
  stabilize,
  restPose,
  drawAxes,
  lerpCurls,
  slerpAxes,
  quatFromZX,
  quatRotate,
  buildLocal,
  ballistic,
  distortAround,
  sampleFlick,
  sampleWard,
  yawDir,
  num,
  frameLine,
  headerLine,
  renderClip,
  sampleCount,
};
