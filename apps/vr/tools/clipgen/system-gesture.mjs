// The one @2 clip: the system gesture (an open palm toward the head, then a thumb-index pinch).
// Opt in with `node apps/vr/tools/clipgen/system-gesture.mjs`. It writes Game/Clips/system/system-gesture.normal.jsonl
// and nothing else, so it can never rewrite a tracked @1 clip. Same bytes every run. See docs/CLIP-SCHEMA.md, Schema @2.
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import {
  CURLS, HZ, v3, lerp, ease, clamp01, normalize, restPose, slerpAxes, lerpCurls,
  seedFor, renderClip, headerLine, frameLine, num,
} from './generate.mjs';

const ACTION = 'system-gesture';
const VARIANT = 'normal';
const RAISE_S = 0.40;
const OPEN_HOLD_S = 0.40;
const PINCH_IN_S = 0.15;
const PINCH_HOLD_S = 0.35;
const PINCH_OUT_S = 0.15;
const TAIL_S = 0.25;
const DURATION_S = RAISE_S + OPEN_HOLD_S + PINCH_IN_S + PINCH_HOLD_S + PINCH_OUT_S + TAIL_S;
// The runtime raises the bit once the palm faces the head: from the end of the turn to the end of the clip.
const SYS_FROM_S = RAISE_S * 0.75;

const HAND = 'R';
// In front of the face, a little to the right. The palm normal points back at the head (-X), fingers up.
const END_POS = v3(0.30, 0.10, 0.62);
const END_NORMAL = normalize(v3(-1, -0.15, 0.10));
const END_FINGER = v3(0, 0, 1);

function sampleAt(t) {
  const rest = restPose(HAND);
  const u = ease(t / RAISE_S);
  const turned = slerpAxes(rest.normal, rest.finger, END_NORMAL, END_FINGER, u);
  const pinchT = t - (RAISE_S + OPEN_HOLD_S);
  const pinch = clamp01(pinchT / PINCH_IN_S) * (1 - clamp01((pinchT - PINCH_IN_S - PINCH_HOLD_S) / PINCH_OUT_S));
  const curls = lerpCurls(lerpCurls(CURLS.relaxed, CURLS.open, u), CURLS.seal, 0.6 * pinch);
  return [{
    side: HAND,
    palm: lerp(rest.pos, END_POS, u),
    normal: turned.normal,
    finger: turned.finger,
    curls,
    pinch: 0.85 * pinch,
  }];
}

function build() {
  const seed = seedFor(ACTION, VARIANT);
  const params = { seed, slow: 1, jitter: 0, distort: 0, overshoot: 0, dips: false, phases: [0, 0, 0, 0, 0] };
  const rendered = renderClip({
    action: ACTION, variant: VARIANT, hands: [HAND], seed, source: 'synthetic', params,
    duration: DURATION_S, sampleAt: (t) => sampleAt(Math.min(t, DURATION_S)),
  });
  const lines = [headerLine(ACTION, VARIANT, [HAND], seed).replace('hand-clip@1', 'hand-clip@2')];
  let raised = 0;
  for (const frame of rendered.frames) {
    const sys = frame.t >= SYS_FROM_S - 1e-9;
    raised += sys ? 1 : 0;
    // sys goes after pinch, before joints.
    lines.push(frameLine(frame).replace(`"pinch":${num(frame.pinch)},`, `"pinch":${num(frame.pinch)},"sys":${sys},`));
  }
  if (raised === 0 || raised === rendered.frames.length) {
    throw new Error('the system gesture clip needs frames with and without the bit');
  }
  return `${lines.join('\n')}\n`;
}

function main() {
  const here = path.dirname(fileURLToPath(import.meta.url));
  const dir = path.resolve(here, '..', '..', 'Game', 'Clips', 'system');
  fs.mkdirSync(dir, { recursive: true });
  const file = path.join(dir, `${ACTION}.${VARIANT}.jsonl`);
  fs.writeFileSync(file, build(), { encoding: 'utf8' });
  process.stdout.write(`wrote 1 clip to Game/Clips/system (${HZ} Hz, ${DURATION_S.toFixed(2)} s)\n`);
}

main();
