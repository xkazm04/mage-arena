// The closed fist held over a fallen rival (T25, the missio choice): the off hand rises from the lap to chest height,
// closes into a fist with the thumb over the fingers, holds, then opens and returns. Three takes (normal, slow, sloppy)
// plus the impostor `fist-loose`, a loose half-curl that must never count as a fist (FistDetector.h).
// Opt in with `node apps/vr/tools/clipgen/fist.mjs --canonical` (without the flag it refuses; importing it is inert). It
// writes Game/Clips/fist/{fist.normal,fist.slow,fist.sloppy,fist-loose.normal}.jsonl and nothing else, so it can never
// rewrite a tracked action clip. Same bytes every run.
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath, pathToFileURL } from 'node:url';
import {
  CURLS, HZ, v3, lerp, ease, lerpCurls, slerpAxes, restPose, ballistic, distortAround,
  variantParams, renderClip, isDetectorRaised,
} from './generate.mjs';

// The ward hand (FMageSettings::WardHand, left by default): the hand that warded all bout opens to spare or closes to
// finish. The fist is held thumb up, palm toward the body's centre line, so the palm never faces the arena: a fist is
// never a ward (the ward needs the palm within 35 degrees of forward, WardDetector.h).
const HAND = 'L';
const REACH_S = 0.35;
const HOLD_S = 0.80;
const BACK_S = 0.35;
const END_POS = v3(0.34, -0.12, 0.30);
const END_NORMAL = v3(0, 1, 0);
const END_FINGER = v3(1, 0, 0);
// pinch 0.9 brings the thumb tip onto the curled index, over the fingers (the mudra's grip uses the same value).
const FIST_PINCH = 0.9;
// The impostor stops halfway between the open hand and the fist, with the thumb resting at the side.
const LOOSE_CURL = 0.5;
const LOOSE_PINCH = 0.3;

const TAKES = [
  { action: 'fist', variant: 'normal', curls: CURLS.fist, pinch: FIST_PINCH },
  { action: 'fist', variant: 'slow', curls: CURLS.fist, pinch: FIST_PINCH },
  { action: 'fist', variant: 'sloppy', curls: CURLS.fist, pinch: FIST_PINCH },
  { action: 'fist-loose', variant: 'normal', curls: lerpCurls(CURLS.open, CURLS.fist, LOOSE_CURL), pinch: LOOSE_PINCH },
];

function durationOf(params) {
  return (REACH_S + HOLD_S + BACK_S) * params.slow;
}

function sampleAt(take, params, t) {
  const reach = REACH_S * params.slow;
  const hold = HOLD_S * params.slow;
  const back = BACK_S * params.slow;
  const rest = restPose(HAND);
  let u;
  if (t < reach) {
    u = ballistic(t / reach, params.overshoot);
  } else if (t < reach + hold) {
    u = 1;
  } else {
    u = 1 - ease((t - reach - hold) / back);
  }
  const close = Math.min(1, Math.max(0, u));
  const turned = slerpAxes(rest.normal, rest.finger, END_NORMAL, END_FINGER, close);
  return [{
    side: HAND,
    palm: distortAround(lerp(rest.pos, END_POS, u), rest.pos, params),
    normal: turned.normal,
    finger: turned.finger,
    curls: lerpCurls(CURLS.relaxed, take.curls, close),
    pinch: take.pinch * close,
  }];
}

function build(take) {
  const params = variantParams(take.action, take.variant);
  const duration = durationOf(params);
  const rendered = renderClip({
    action: take.action, variant: take.variant, hands: [HAND], seed: params.seed, source: 'synthetic', params,
    duration, sampleAt: (t) => sampleAt(take, params, Math.min(t, duration)),
  });
  if (rendered.frames.some((frame) => isDetectorRaised(frame))) {
    throw new Error(`${take.action} ${take.variant}: a frame reads as a raised ward`);
  }
  if (take.variant === 'sloppy' && !rendered.frames.some((frame) => frame.conf === 0.4)) {
    throw new Error(`${take.action} sloppy has no confidence dip`);
  }
  return rendered.text;
}

const REPO_GAME = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..', '..', 'Game');

// Writes the fist clips under <gameRoot>/Clips/fist. Pure of side effects at import time; the repo's Game tree is only
// touched when the caller passes it (or nothing) on purpose.
export function writeFist(gameRoot = REPO_GAME, log = (text) => process.stdout.write(text)) {
  const dir = path.join(gameRoot, 'Clips', 'fist');
  fs.mkdirSync(dir, { recursive: true });
  for (const take of TAKES) {
    fs.writeFileSync(path.join(dir, `${take.action}.${take.variant}.jsonl`), build(take), { encoding: 'utf8' });
  }
  log(`wrote ${TAKES.length} clips to Game/Clips/fist (${HZ} Hz)\n`);
}

function isDirectInvocation() {
  return !!process.argv[1] && import.meta.url === pathToFileURL(process.argv[1]).href;
}

if (isDirectInvocation()) {
  if (process.argv.slice(2).join(' ') === '--canonical') {
    writeFist();
  } else {
    process.stderr.write('fist.mjs rewrites tracked clips; pass --canonical to do that on purpose.\n'
      + 'To verify without writing: node apps/vr/tools/clipgen/generate.mjs --check\n');
    process.exitCode = 2;
  }
}
