// Seeded sigil drawings, the template set, and the false-cast noise clips.
// Imported by generate.mjs (--templates / --corpus). Templates and corpus never share a seed.
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import {
  IDX,
  DRAW_X,
  CHEST_Z,
  CIRCLE_R,
  CURLS,
  ACTIONS,
  VARIANTS,
  v3,
  add,
  lerp,
  clamp01,
  ease,
  mulberry32,
  seedFor,
  palmForTip,
  restPose,
  drawAxes,
  lerpCurls,
  slerpAxes,
  quatFromZX,
  quatRotate,
  buildLocal,
  ballistic,
  sampleFlick,
  renderClip,
} from './generate.mjs';

export const SIGIL_CLASSES = [
  ['sigil-line1', 'vertical'],
  ['sigil-line2', 'horizontal'],
  ['sigil-line3', 'diagonal'],
];

const SIGIL_PHASE = { approach: 0.28, circle: 1.00, lift: 0.16, stroke: 0.45, release: 0.22 };

// Reserved bands. Corpus seeds are the caller's base plus a small index and must stay out of these.
export const TEMPLATE_BASE = 800000000;
export const MOUSE_BASE = 700000000;
export const IMPOSTOR_BASE = 500000000;
const NOISE_BASE = 900000000;

export const IMPOSTOR_KINDS = ['bare', 'dot', 'zigzag', 'hook', 'arc', 'swipe'];
const IMPOSTOR_PER_KIND = 30;

export const TEMPLATE_PLAN = [
  { profile: 'anchor', continuous: false, variant: 'normal' },
  { profile: 'normal', continuous: false, variant: 'normal' },
  { profile: 'normal', continuous: false, variant: 'normal' },
  { profile: 'normal', continuous: false, variant: 'normal' },
  { profile: 'anchor', continuous: true, variant: 'normal' },
  { profile: 'normal', continuous: true, variant: 'normal' },
  { profile: 'normal', continuous: true, variant: 'normal' },
  { profile: 'normal', continuous: true, variant: 'normal' },
  { profile: 'sloppy', continuous: false, variant: 'sloppy' },
  { profile: 'sloppy', continuous: false, variant: 'sloppy' },
  { profile: 'sloppy', continuous: true, variant: 'sloppy' },
  { profile: 'sloppy', continuous: true, variant: 'sloppy' },
];

export function sigilDuration(params) {
  return (SIGIL_PHASE.approach + SIGIL_PHASE.circle + SIGIL_PHASE.lift + SIGIL_PHASE.stroke + SIGIL_PHASE.release) * params.slow;
}

function bandOf(seed) {
  if (seed >= MOUSE_BASE && seed < MOUSE_BASE + 10000000) return 'mouse';
  if (seed >= TEMPLATE_BASE && seed < TEMPLATE_BASE + 10000000) return 'template';
  if (seed >= IMPOSTOR_BASE && seed < IMPOSTOR_BASE + 10000000) return 'impostor';
  if (seed >= NOISE_BASE && seed < NOISE_BASE + 10000000) return 'noise';
  return null;
}

export function variedSigilParams(seed, profile, continuous) {
  const rng = mulberry32(seed >>> 0);
  const r = () => rng() * 2 - 1;
  const u = () => rng();
  const anchor = profile === 'anchor';
  const sloppy = profile === 'sloppy';
  const mouse = profile === 'mouse';
  let jitter = 0;
  let wobble = 0;
  let radiusJ = 0.04;
  let ellipse = 0.05;
  let center = 0.006;
  let startJ = 0.10;
  let closureDrop = 0.015;
  let angleJ = 0.05;
  let halfJ = 0.005;
  let shift = 0.004;
  let overshoot = 0;
  let xWobble = 0.0004;
  if (anchor) {
    radiusJ = 0.005;
    ellipse = 0.008;
    center = 0.001;
    startJ = 0.02;
    closureDrop = 0.004;
    angleJ = 0.012;
    halfJ = 0.001;
    shift = 0.001;
    jitter = 0;
    wobble = 0;
    xWobble = 0;
    overshoot = 0;
  } else if (sloppy) {
    radiusJ = 0.10;
    ellipse = 0.14;
    center = 0.010;
    startJ = 0.25;
    closureDrop = 0.045;
    angleJ = 0.10;
    halfJ = 0.008;
    shift = 0.008;
    jitter = 0.004 + u() * 0.004;
    wobble = 0.002 + u() * 0.002;
    xWobble = 0.001 + u() * 0.001;
    overshoot = 0.04 + u() * 0.04;
  } else if (mouse) {
    radiusJ = 0.06;
    ellipse = 0.09;
    center = 0.008;
    startJ = 0.18;
    closureDrop = 0.04;
    angleJ = 0.07;
    halfJ = 0.006;
    shift = 0.006;
    jitter = 0.0015 + u() * 0.002;
    wobble = 0.002 + u() * 0.0025;
    xWobble = 0.0006;
    overshoot = 0.02 * u();
  } else {
    jitter = u() * 0.0015;
    wobble = u() * 0.001;
  }
  return {
    seed: seed >>> 0,
    slow: 1,
    continuous: !!continuous,
    radius: 1 + r() * radiusJ,
    ellipseY: 1 + r() * ellipse,
    ellipseZ: 1 + r() * ellipse,
    centerY: r() * center,
    centerZ: r() * center,
    startAngle: r() * startJ,
    closure: 1 - u() * closureDrop,
    strokeAngleJitter: r() * angleJ,
    strokeHalf: 0.065 + r() * halfJ,
    strokeShift: r() * shift,
    jitter,
    wobble,
    wobblePhase: u() * Math.PI * 2,
    xWobble,
    overshoot,
    dips: sloppy,
    phases: [0, 1, 2, 3, 4].map(() => u() * Math.PI * 2),
    dipA: u(),
    dipB: u(),
    speedWarp: mouse,
    warpAmp: mouse ? 0.22 + u() * 0.15 : 0,
    warpCycles: mouse ? 2 + Math.floor(u() * 2) : 1,
  };
}

function warpU(u, params) {
  if (!params.speedWarp || params.warpAmp <= 0) {
    return u;
  }
  const k = params.warpCycles || 2;
  const amp = Math.min(0.45, params.warpAmp);
  return clamp01(u + (amp * Math.sin(2 * Math.PI * k * u)) / (2 * Math.PI * k));
}

function variedCirclePoint(angle, params) {
  const y = Math.cos(angle) * CIRCLE_R * params.radius * params.ellipseY;
  const z = Math.sin(angle) * CIRCLE_R * params.radius * params.ellipseZ;
  const w = params.wobble * Math.sin(3 * angle + params.wobblePhase);
  return v3(
    DRAW_X + params.xWobble * Math.sin(angle * 2 + params.wobblePhase),
    params.centerY + y + Math.cos(angle) * w,
    CHEST_Z + params.centerZ + z + Math.sin(angle) * w,
  );
}

function variedStrokeEnds(stroke, params) {
  const base = stroke === 'vertical' ? Math.PI / 2 : stroke === 'horizontal' ? 0 : Math.PI / 4;
  const ang = base + params.strokeAngleJitter;
  const dirY = Math.cos(ang);
  const dirZ = Math.sin(ang);
  const offY = -dirZ * params.strokeShift;
  const offZ = dirY * params.strokeShift;
  const cy = params.centerY + offY;
  const cz = CHEST_Z + params.centerZ + offZ;
  return [
    v3(DRAW_X, cy - dirY * params.strokeHalf, cz - dirZ * params.strokeHalf),
    v3(DRAW_X, cy + dirY * params.strokeHalf, cz + dirZ * params.strokeHalf),
  ];
}

export function sampleVariedSigil(stroke, params, t) {
  const s = params.slow;
  const approach = SIGIL_PHASE.approach * s;
  const circle = SIGIL_PHASE.circle * s;
  const lift = SIGIL_PHASE.lift * s;
  const strokeT = SIGIL_PHASE.stroke * s;
  const release = SIGIL_PHASE.release * s;
  const axes = drawAxes();
  const startAngle = params.startAngle - Math.PI / 2;
  const circleStart = variedCirclePoint(startAngle, params);
  const circleEnd = variedCirclePoint(startAngle + Math.PI * 2 * params.closure, params);
  const [strokeStart, strokeEnd] = variedStrokeEnds(stroke, params);
  const rest = restPose('R');
  const restLocal = buildLocal('R', rest.curls, 0);
  const restTip = add(rest.pos, quatRotate(quatFromZX(rest.finger, rest.normal), restLocal[IDX.IndexTip]));
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
    const u = warpU((t - approach) / circle, params);
    const angle = startAngle + ease(u) * Math.PI * 2 * params.closure;
    tip = variedCirclePoint(angle, params);
    curls = CURLS.pen;
    pinch = 1;
  } else if (t <= approach + circle + lift) {
    const u = (t - approach - circle) / lift;
    tip = lerp(circleEnd, strokeStart, ease(u));
    curls = CURLS.pen;
    if (params.continuous) {
      pinch = 1;
    } else {
      const valley = clamp01(u);
      pinch = valley < 0.5 ? 1 - ease(valley / 0.5) : ease((valley - 0.5) / 0.5);
    }
  } else if (t <= approach + circle + lift + strokeT) {
    const u = warpU((t - approach - circle - lift) / strokeT, params);
    const p = ballistic(u, params.overshoot);
    tip = lerp(strokeStart, strokeEnd, p);
    curls = CURLS.pen;
    pinch = 1;
  } else {
    const u = clamp01((t - approach - circle - lift - strokeT) / release);
    tip = strokeEnd;
    curls = lerpCurls(CURLS.pen, CURLS.relaxed, ease(u));
    pinch = 1 - ease(u);
  }
  const palm = palmForTip('R', tip, normal, finger, curls, pinch);
  return [{ side: 'R', palm, normal, finger, curls, pinch }];
}

function clipsRoot() {
  const here = path.dirname(fileURLToPath(import.meta.url));
  return path.resolve(here, '..', '..', 'Game', 'Clips');
}

// The generated test clips (corpus, impostors, noise, mouse). Kept out of Game/Clips, which the package stages whole.
export function testClipsRoot() {
  const here = path.dirname(fileURLToPath(import.meta.url));
  return path.resolve(here, '..', '..', 'Game', 'TestClips');
}

// Before the generators moved to Game/TestClips they wrote these folders under Game/Clips, which the package stages whole.
// Remove an old copy only when it holds nothing but .jsonl files; anything else is left whole with a warning.
export function removeLegacyClips(name) {
  if (!['corpus', 'impostors', 'noise', 'mouse'].includes(name)) {
    throw new Error(`not a legacy clip folder: ${name}`);
  }
  const dir = path.join(clipsRoot(), name);
  if (!fs.existsSync(dir)) {
    return;
  }
  const entries = fs.readdirSync(dir, { withFileTypes: true });
  if (!entries.every((e) => e.isFile() && e.name.endsWith('.jsonl'))) {
    process.stdout.write(`warning: Game/Clips/${name} holds more than .jsonl files; left in place, delete it by hand\n`);
    return;
  }
  let bytes = 0;
  for (const e of entries) {
    bytes += fs.statSync(path.join(dir, e.name)).size;
  }
  fs.rmSync(dir, { recursive: true });
  process.stdout.write(`removed legacy Game/Clips/${name}: ${entries.length} files, ${bytes} bytes\n`);
}

function wipeJsonl(dir) {
  fs.mkdirSync(dir, { recursive: true });
  for (const name of fs.readdirSync(dir)) {
    if (name.endsWith('.jsonl')) {
      fs.unlinkSync(path.join(dir, name));
    }
  }
}

function canonicalSeedSet() {
  const seeds = new Set();
  for (const variant of VARIANTS) {
    for (const [action] of ACTIONS) {
      seeds.add(seedFor(action, variant));
    }
  }
  return seeds;
}

function seedsOnDisk(dir) {
  const seeds = new Set();
  if (!fs.existsSync(dir)) {
    return seeds;
  }
  const stack = [dir];
  while (stack.length) {
    const current = stack.pop();
    for (const entry of fs.readdirSync(current, { withFileTypes: true })) {
      const full = path.join(current, entry.name);
      if (entry.isDirectory()) {
        stack.push(full);
      } else if (entry.name.endsWith('.jsonl')) {
        const line = fs.readFileSync(full, 'utf8').split('\n', 1)[0];
        const header = JSON.parse(line);
        seeds.add(header.seed);
      }
    }
  }
  return seeds;
}

function claimSeed(seed, used) {
  if (used.has(seed)) {
    throw new Error(`seed ${seed} is already used`);
  }
  const band = bandOf(seed);
  used.add(seed);
  return band;
}

export function buildVariedClip(action, stroke, variant, seed, profile, continuous, source = 'synthetic') {
  const params = variedSigilParams(seed, profile, continuous);
  params.slow = variant === 'slow' ? 1.6 : 1;
  const built = renderClip({
    action,
    variant,
    hands: ['R'],
    seed,
    source,
    params,
    duration: sigilDuration(params),
    sampleAt: (t) => sampleVariedSigil(stroke, params, t),
  });
  return built;
}

export function writeTemplates() {
  const root = clipsRoot();
  const dir = path.join(root, 'templates');
  wipeJsonl(dir);
  const used = canonicalSeedSet();
  let count = 0;
  for (let c = 0; c < SIGIL_CLASSES.length; c++) {
    const [action, stroke] = SIGIL_CLASSES[c];
    for (let i = 0; i < TEMPLATE_PLAN.length; i++) {
      const spec = TEMPLATE_PLAN[i];
      const seed = TEMPLATE_BASE + c * 1000 + i;
      claimSeed(seed, used);
      if (bandOf(seed) !== 'template') {
        throw new Error(`template seed ${seed} left its band`);
      }
      const built = buildVariedClip(action, stroke, spec.variant, seed, spec.profile, spec.continuous);
      const name = `${action}.${spec.variant}.t${seed}.jsonl`;
      fs.writeFileSync(path.join(dir, name), built.text, { encoding: 'utf8' });
      count++;
    }
  }
  process.stdout.write(`wrote ${count} template clips to Game/Clips/templates\n`);
}

function restIndexTip() {
  const rest = restPose('R');
  const local = buildLocal('R', rest.curls, 0);
  return add(rest.pos, quatRotate(quatFromZX(rest.finger, rest.normal), local[IDX.IndexTip]));
}

function penHand(tip, pinch) {
  const axes = drawAxes();
  const curls = CURLS.pen;
  const palm = palmForTip('R', tip, axes.normal, axes.finger, curls, pinch);
  return { side: 'R', palm, normal: axes.normal, finger: axes.finger, curls, pinch };
}

function arcPoint(delta) {
  const angle = -Math.PI / 2 + delta;
  return v3(DRAW_X, Math.cos(angle) * 0.10, CHEST_Z + Math.sin(angle) * 0.10);
}

function chainFlick(spec, period, t) {
  const flickDur = spec.windup + spec.flick + spec.recover;
  const params = { distort: 0, overshoot: 0, jitter: 0 };
  const lt = t % period;
  if (lt <= flickDur) {
    return sampleFlick(spec, lt, 1, params);
  }
  const end = sampleFlick(spec, flickDur, 1, params)[0];
  const rest = restPose('R');
  const u = ease((lt - flickDur) / (period - flickDur));
  const turned = slerpAxes(end.normal, end.finger, rest.normal, rest.finger, u);
  return [{
    side: 'R',
    palm: lerp(end.palm, rest.pos, u),
    normal: turned.normal,
    finger: turned.finger,
    curls: lerpCurls(end.curls, rest.curls, u),
    pinch: 0,
  }];
}

function chainWard(t) {
  const period = 1.5;
  const lt = t % period;
  const rest = restPose('L');
  const endPos = v3(0.36, -0.12, 0.36);
  const endNormal = v3(1, 0, 0);
  const endFinger = v3(0, 0, 1);
  let u;
  if (lt < 0.30) {
    u = ease(lt / 0.30);
  } else if (lt < 1.10) {
    u = 1;
  } else {
    u = 1 - ease((lt - 1.10) / 0.40);
  }
  const turned = slerpAxes(rest.normal, rest.finger, endNormal, endFinger, u);
  // The ward is the off hand. The casting hand stays in its idle drift for the same span,
  // which is what the sigil recognizer actually sees.
  const right = chainIdle(t)[0];
  return [{
    side: 'L',
    palm: lerp(rest.pos, endPos, u),
    normal: turned.normal,
    finger: turned.finger,
    curls: CURLS.open,
    pinch: 0,
  }, right];
}

function chainIdle(t) {
  const rest = restPose('R');
  const drift = v3(0.008 * Math.sin(t * 0.7), 0.012 * Math.sin(t * 0.5 + 1.1), 0.010 * Math.sin(t * 0.4 + 2.0));
  return [{
    side: 'R',
    palm: add(rest.pos, drift),
    normal: rest.normal,
    finger: rest.finger,
    curls: rest.curls,
    pinch: 0,
  }];
}

function chainPartial(t) {
  const period = 1.2;
  const rep = Math.floor(Math.max(0, t - 1e-9) / period);
  const lt = t - rep * period;
  const sweep = (200 + (rep % 5) * 12) * Math.PI / 180;
  const arcStart = 0.12;
  const arcEnd = 0.78;
  const releaseEnd = 0.92;
  if (lt < arcStart) {
    const u = ease(lt / arcStart);
    return [penHand(lerp(restIndexTip(), arcPoint(0), u), 0)];
  }
  if (lt < arcEnd) {
    const u = (lt - arcStart) / (arcEnd - arcStart);
    const pinch = u < 0.08 ? ease(u / 0.08) : 1;
    return [penHand(arcPoint(ease(u) * sweep), pinch)];
  }
  if (lt < releaseEnd) {
    const u = (lt - arcEnd) / (releaseEnd - arcEnd);
    return [penHand(arcPoint(sweep), 1 - ease(u))];
  }
  const u = ease((lt - releaseEnd) / (period - releaseEnd));
  const endHand = penHand(arcPoint(sweep), 0);
  const rest = restPose('R');
  const turned = slerpAxes(endHand.normal, endHand.finger, rest.normal, rest.finger, u);
  return [{
    side: 'R',
    palm: lerp(endHand.palm, rest.pos, u),
    normal: turned.normal,
    finger: turned.finger,
    curls: lerpCurls(CURLS.pen, rest.curls, u),
    pinch: 0,
  }];
}

const NOISE_PLAN = [
  { action: 'noise-ward', hands: ['L', 'R'], seed: NOISE_BASE + 1, sampleAt: chainWard },
  { action: 'noise-blink', hands: ['R'], seed: NOISE_BASE + 2, sampleAt: (t) => chainFlick({ yaw: [ -60, 60, 180 ][Math.floor(t / 0.75) % 3], windup: 0.08, flick: 0.12, recover: 0.18, distance: 0.16 }, 0.75, t) },
  { action: 'noise-bolt', hands: ['R'], seed: NOISE_BASE + 3, sampleAt: (t) => chainFlick({ yaw: 0, windup: 0.10, flick: 0.12, recover: 0.20, distance: 0.18 }, 0.80, t) },
  { action: 'noise-idle', hands: ['R'], seed: NOISE_BASE + 4, sampleAt: chainIdle },
  { action: 'noise-partial', hands: ['R'], seed: NOISE_BASE + 5, sampleAt: chainPartial },
];

export function buildNoiseClips() {
  const seconds = 36;
  const quiet = { jitter: 0, dips: false, phases: [0, 0, 0, 0, 0] };
  return NOISE_PLAN.map((spec) => {
    const built = renderClip({
      action: spec.action,
      variant: 'normal',
      hands: spec.hands,
      seed: spec.seed,
      source: 'synthetic',
      params: quiet,
      duration: seconds,
      sampleAt: spec.sampleAt,
    });
    return { action: spec.action, frames: built.frames, duration: built.duration };
  });
}

function writeNoise(used) {
  const dir = path.join(testClipsRoot(), 'noise');
  wipeJsonl(dir);
  const seconds = 36;
  let total = 0;
  const quiet = { jitter: 0, dips: false, phases: [0, 0, 0, 0, 0] };
  for (const spec of NOISE_PLAN) {
    claimSeed(spec.seed, used);
    const built = renderClip({
      action: spec.action,
      variant: 'normal',
      hands: spec.hands,
      seed: spec.seed,
      source: 'synthetic',
      params: quiet,
      duration: seconds,
      sampleAt: spec.sampleAt,
    });
    if (Math.abs(built.duration - seconds) > 1e-9) {
      throw new Error(`${spec.action} duration ${built.duration} is not ${seconds}`);
    }
    total += built.duration;
    fs.writeFileSync(path.join(dir, `${spec.action}.normal.jsonl`), built.text, { encoding: 'utf8' });
  }
  if (Math.abs(total - 180) > 1e-6) {
    throw new Error(`noise total ${total}s is not 180s`);
  }
  process.stdout.write(`wrote ${NOISE_PLAN.length} noise clips (${total.toFixed(3)}s) to Game/TestClips/noise\n`);
}

function impostorPhase(kind) {
  if (kind === 'bare') return { approach: 0.28, draw: 1.00, lift: 0, mark: 0, release: 0.90 };
  if (kind === 'arc') return { approach: 0.28, draw: 0.85, lift: 0, mark: 0, release: 0.40 };
  if (kind === 'swipe') return { approach: 0.28, draw: 0.45, lift: 0, mark: 0, release: 0.35 };
  return { approach: 0.28, draw: 1.00, lift: 0.16, mark: 0.40, release: 0.30 };
}

function impostorDuration(kind) {
  const phase = impostorPhase(kind);
  return phase.approach + phase.draw + phase.lift + phase.mark + phase.release;
}

function impostorParams(seed) {
  const rng = mulberry32(seed >>> 0);
  const r = () => rng() * 2 - 1;
  const u = () => rng();
  return {
    seed: seed >>> 0,
    slow: 1,
    radius: 0.92 + u() * 0.16,
    centerY: r() * 0.012,
    centerZ: r() * 0.012,
    startAngle: r() * 0.25,
    closure: 0.97 + u() * 0.03,
    // Geometric sweep stays under 300 degrees. The measured peak turn on the
    // fingertip path is lower than the sweep because the approach is nearly straight.
    arcSweep: (230 + u() * 40) * Math.PI / 180,
    swipeAngle: r() * 0.5,
    jitter: 0,
    wobble: u() * 0.001,
    wobblePhase: u() * Math.PI * 2,
    xWobble: 0,
    overshoot: 0,
    dips: false,
    phases: [0, 0, 0, 0, 0],
    dipA: 0,
    dipB: 0,
  };
}

function impostorCircle(angle, params) {
  const wobble = params.wobble * Math.sin(3 * angle + params.wobblePhase);
  return v3(
    DRAW_X,
    params.centerY + Math.cos(angle) * CIRCLE_R * params.radius + Math.cos(angle) * wobble,
    CHEST_Z + params.centerZ + Math.sin(angle) * CIRCLE_R * params.radius + Math.sin(angle) * wobble,
  );
}

function impostorMark(kind, u, params) {
  if (kind === 'dot') {
    const angle = u * Math.PI * 2;
    return v3(DRAW_X, params.centerY + Math.cos(angle) * 0.008, CHEST_Z + params.centerZ + Math.sin(angle) * 0.006);
  }
  if (kind === 'zigzag') {
    const legs = 3;
    const x = Math.min(legs - 1e-9, u * legs);
    const leg = Math.floor(x);
    const f = x - leg;
    const y = params.centerY - 0.02 + (leg + f) * (0.04 / legs);
    const sign = leg % 2 === 0 ? f : 1 - f;
    const z = CHEST_Z + params.centerZ + 0.02 + (sign - 0.5) * 0.018;
    return v3(DRAW_X, y, z);
  }
  if (kind === 'hook') {
    const angle = -Math.PI / 2 + u * (Math.PI / 2);
    return v3(
      DRAW_X,
      params.centerY + 0.01 + Math.cos(angle) * 0.035,
      CHEST_Z + params.centerZ - 0.01 + Math.sin(angle) * 0.035,
    );
  }
  return v3(DRAW_X, params.centerY, CHEST_Z + params.centerZ);
}

function impostorSwipe(u, params) {
  const dirY = Math.cos(params.swipeAngle);
  const dirZ = Math.sin(params.swipeAngle);
  const along = u * 2 - 1;
  const bow = Math.sin(u * Math.PI) * 0.015;
  return v3(
    DRAW_X,
    params.centerY + along * 0.11 * dirY - dirZ * bow,
    CHEST_Z + params.centerZ + along * 0.11 * dirZ + dirY * bow,
  );
}

export function sampleImpostor(kind, params, t) {
  const phase = impostorPhase(kind);
  const axes = drawAxes();
  const startAngle = params.startAngle - Math.PI / 2;
  const sweep = kind === 'arc' ? params.arcSweep : Math.PI * 2 * params.closure;
  const drawStart = kind === 'swipe' ? impostorSwipe(0, params) : impostorCircle(startAngle, params);
  const drawEnd = kind === 'swipe' ? impostorSwipe(1, params) : impostorCircle(startAngle + sweep, params);
  const markStart = impostorMark(kind, 0, params);
  const rest = restPose('R');
  const restLocal = buildLocal('R', rest.curls, 0);
  const restTip = add(rest.pos, quatRotate(quatFromZX(rest.finger, rest.normal), restLocal[IDX.IndexTip]));
  let tip;
  let curls;
  let pinch;
  let normal = axes.normal;
  let finger = axes.finger;
  const drawEndTime = phase.approach + phase.draw;
  const liftEnd = drawEndTime + phase.lift;
  const markEnd = liftEnd + phase.mark;
  if (t <= phase.approach) {
    const u = ease(t / phase.approach);
    tip = lerp(restTip, drawStart, u);
    curls = lerpCurls(CURLS.relaxed, CURLS.pen, u);
    const turned = slerpAxes(rest.normal, rest.finger, axes.normal, axes.finger, u);
    normal = turned.normal;
    finger = turned.finger;
    const pinchU = clamp01((t - phase.approach * 0.65) / (phase.approach * 0.35));
    pinch = ease(pinchU);
  } else if (t <= drawEndTime) {
    const u = (t - phase.approach) / phase.draw;
    if (kind === 'swipe') {
      tip = impostorSwipe(ease(u), params);
    } else {
      tip = impostorCircle(startAngle + ease(u) * sweep, params);
    }
    curls = CURLS.pen;
    pinch = 1;
  } else if (t <= liftEnd) {
    const u = (t - drawEndTime) / phase.lift;
    tip = lerp(drawEnd, markStart, ease(u));
    curls = CURLS.pen;
    const valley = clamp01(u);
    pinch = valley < 0.5 ? 1 - ease(valley / 0.5) : ease((valley - 0.5) / 0.5);
  } else if (t <= markEnd) {
    const u = (t - liftEnd) / phase.mark;
    tip = impostorMark(kind, ease(u), params);
    curls = CURLS.pen;
    pinch = 1;
  } else {
    const u = clamp01((t - markEnd) / phase.release);
    tip = phase.mark > 0 ? impostorMark(kind, 1, params) : drawEnd;
    curls = lerpCurls(CURLS.pen, CURLS.relaxed, ease(u));
    pinch = 1 - ease(u);
  }
  const palm = palmForTip('R', tip, normal, finger, curls, pinch);
  return [{ side: 'R', palm, normal, finger, curls, pinch }];
}

function writeImpostors(used) {
  const dir = path.join(testClipsRoot(), 'impostors');
  wipeJsonl(dir);
  let count = 0;
  for (let k = 0; k < IMPOSTOR_KINDS.length; k++) {
    const kind = IMPOSTOR_KINDS[k];
    for (let i = 0; i < IMPOSTOR_PER_KIND; i++) {
      const seed = IMPOSTOR_BASE + k * 1000 + i;
      claimSeed(seed, used);
      if (bandOf(seed) !== 'impostor') {
        throw new Error(`impostor seed ${seed} left its band`);
      }
      const params = impostorParams(seed);
      const action = `impostor-${kind}`;
      const built = renderClip({
        action,
        variant: 'normal',
        hands: ['R'],
        seed,
        source: 'synthetic',
        params,
        duration: impostorDuration(kind),
        sampleAt: (t) => sampleImpostor(kind, params, t),
      });
      fs.writeFileSync(path.join(dir, `${action}.normal.i${seed}.jsonl`), built.text, { encoding: 'utf8' });
      count++;
    }
  }
  process.stdout.write(`wrote ${count} impostor clips to Game/TestClips/impostors\n`);
}

export function writeCorpus(n, baseSeed) {
  const root = clipsRoot();
  for (const name of ['corpus', 'impostors', 'noise']) {
    removeLegacyClips(name);
  }
  const dir = path.join(testClipsRoot(), 'corpus');
  wipeJsonl(dir);
  const used = canonicalSeedSet();
  for (const seed of seedsOnDisk(path.join(root, 'templates'))) {
    used.add(seed);
  }
  for (const seed of seedsOnDisk(path.join(testClipsRoot(), 'mouse'))) {
    used.add(seed);
  }
  let count = 0;
  const variants = ['normal', 'slow', 'sloppy'];
  for (let c = 0; c < SIGIL_CLASSES.length; c++) {
    const [action, stroke] = SIGIL_CLASSES[c];
    for (let v = 0; v < variants.length; v++) {
      const variant = variants[v];
      for (let i = 0; i < n; i++) {
        const seed = baseSeed + count;
        if (seed > 4294967295) {
          throw new Error('corpus seed overflow');
        }
        if (bandOf(seed)) {
          throw new Error(`corpus seed ${seed} collides with the ${bandOf(seed)} band; pick another --seed`);
        }
        claimSeed(seed, used);
        const profile = variant === 'sloppy' ? 'sloppy' : 'normal';
        const built = buildVariedClip(action, stroke, variant, seed, profile, false);
        fs.writeFileSync(path.join(dir, `${action}.${variant}.${seed}.jsonl`), built.text, { encoding: 'utf8' });
        count++;
      }
    }
  }
  process.stdout.write(`wrote ${count} corpus clips to Game/TestClips/corpus\n`);
  writeNoise(used);
  writeImpostors(used);
}

