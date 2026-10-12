// The three stone reaches (T20): one hand moves from rest to hover over a stone beside the seat, holds, and returns.
// The session reads a palm or index tip within 6 cm of a stone centre (ArenaSession.cpp StonesTouched). Comfort settings
// toggle on the touch in cold; the stone pick in an intermission needs the hold (teach.json offerHoldS, 0.40 s).
// Opt in with `node apps/vr/tools/clipgen/stones.mjs`. It writes Game/Clips/stones/stone-{left,centre,right}.normal.jsonl
// and nothing else, so it can never rewrite a tracked action clip. Same bytes every run.
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import {
  CURLS, HZ, v3, lerp, ease, normalize, restPose, slerpAxes, lerpCurls,
  seedFor, renderClip, headerLine, frameLine,
} from './generate.mjs';

const VARIANT = 'normal';
const REACH_S = 0.55;
const HOLD_S = 0.90;
const BACK_S = 0.45;
const DURATION_S = REACH_S + HOLD_S + BACK_S;

// Stone centres in metres, seated origin, +X forward, +Y right (ArenaSession.cpp, the same cm values / 100).
// The palm hovers 4 cm above the centre, inside the 6 cm radius. The palm faces down, so it is never a ward
// (a ward needs the palm toward the arena), and the fingers stay open, so it is never a pointing flick.
const STONES = [
  { action: 'stone-left', hand: 'L', centre: v3(0.55, -0.28, 0.18) },
  { action: 'stone-centre', hand: 'R', centre: v3(0.55, 0.00, 0.18) },
  { action: 'stone-right', hand: 'R', centre: v3(0.55, 0.28, 0.18) },
];
const HOVER_Z = 0.04;
// The reach goes over the stone first and then down onto it, so the index tip (about 10 cm ahead of the palm) never
// grazes the stone radius on the way in. A straight reach let the tip enter, leave, and the palm enter again, which
// restarted the hold.
const LIFT_Z = 0.24;
const LIFT_PART = 0.65;
const DOWN_NORMAL = normalize(v3(0, 0, -1));
const FORWARD = v3(1, 0, 0);

function sampleAt(stone, t) {
  const rest = restPose(stone.hand);
  const over = v3(stone.centre.x, stone.centre.y, stone.centre.z + HOVER_Z);
  let u;
  if (t < REACH_S) {
    u = ease(t / REACH_S);
  } else if (t < REACH_S + HOLD_S) {
    u = 1;
  } else {
    u = 1 - ease((t - REACH_S - HOLD_S) / BACK_S);
  }
  const turned = slerpAxes(rest.normal, rest.finger, DOWN_NORMAL, FORWARD, u);
  const above = v3(over.x, over.y, over.z + LIFT_Z);
  const palm = u < LIFT_PART
    ? lerp(rest.pos, above, u / LIFT_PART)
    : lerp(above, over, (u - LIFT_PART) / (1 - LIFT_PART));
  return [{
    side: stone.hand,
    palm,
    normal: turned.normal,
    finger: turned.finger,
    curls: lerpCurls(CURLS.relaxed, CURLS.open, u),
    pinch: 0,
  }];
}

function build(stone) {
  const seed = seedFor(stone.action, VARIANT);
  const params = { seed, slow: 1, jitter: 0, distort: 0, overshoot: 0, dips: false, phases: [0, 0, 0, 0, 0] };
  const rendered = renderClip({
    action: stone.action, variant: VARIANT, hands: [stone.hand], seed, source: 'synthetic', params,
    duration: DURATION_S, sampleAt: (t) => sampleAt(stone, Math.min(t, DURATION_S)),
  });
  const lines = [headerLine(stone.action, VARIANT, [stone.hand], seed)];
  for (const frame of rendered.frames) {
    lines.push(frameLine(frame));
  }
  return `${lines.join('\n')}\n`;
}

function main() {
  const here = path.dirname(fileURLToPath(import.meta.url));
  const dir = path.resolve(here, '..', '..', 'Game', 'Clips', 'stones');
  fs.mkdirSync(dir, { recursive: true });
  for (const stone of STONES) {
    fs.writeFileSync(path.join(dir, `${stone.action}.${VARIANT}.jsonl`), build(stone), { encoding: 'utf8' });
  }
  process.stdout.write(`wrote ${STONES.length} clips to Game/Clips/stones (${HZ} Hz, ${DURATION_S.toFixed(2)} s)\n`);
}

main();
