// Validate hand clips against docs/CLIP-SCHEMA.md. Node built-ins only.
// Usage: node tools/clipgen/validate.mjs [dir]
import fs from 'node:fs';
import path from 'node:path';

const SCHEMA_V1 = 'mage-arena/hand-clip@1';
const SCHEMA_V2 = 'mage-arena/hand-clip@2';
const SPACE = 'seated-origin, metres, +X forward, +Y right, +Z up (Unreal axes, cm converted to m)';
const KEYPOINTS = [
  'Palm', 'Wrist',
  'ThumbMetacarpal', 'ThumbProximal', 'ThumbDistal', 'ThumbTip',
  'IndexMetacarpal', 'IndexProximal', 'IndexIntermediate', 'IndexDistal', 'IndexTip',
  'MiddleMetacarpal', 'MiddleProximal', 'MiddleIntermediate', 'MiddleDistal', 'MiddleTip',
  'RingMetacarpal', 'RingProximal', 'RingIntermediate', 'RingDistal', 'RingTip',
  'LittleMetacarpal', 'LittleProximal', 'LittleIntermediate', 'LittleDistal', 'LittleTip',
];
const HEADER_KEYS = ['schema', 'name', 'action', 'variant', 'hands', 'hz', 'space', 'keypoints', 'source', 'seed'];
const FRAME_KEYS = ['t', 'hand', 'conf', 'pinch', 'joints'];
// @2 adds one optional boolean, `sys`, written after `pinch`. @1 stays closed.
const FRAME_KEYS_SYS = ['t', 'hand', 'conf', 'pinch', 'sys', 'joints'];
const VARIANTS = new Set(['normal', 'slow', 'sloppy']);
const SOURCES = new Set(['synthetic', 'recorded', 'mouse']);

function sameKeys(obj, expected) {
  const keys = Object.keys(obj);
  if (keys.length !== expected.length) {
    return false;
  }
  return expected.every((key) => Object.prototype.hasOwnProperty.call(obj, key));
}

function validateFile(file) {
  const errors = [];
  const buf = fs.readFileSync(file);
  if (buf.length >= 3 && buf[0] === 0xef && buf[1] === 0xbb && buf[2] === 0xbf) {
    errors.push('UTF-8 BOM');
  }
  if (buf.includes(0x0d)) {
    errors.push('CR byte; line endings must be LF');
  }
  if (buf.length === 0 || buf[buf.length - 1] !== 0x0a) {
    errors.push('file must end with LF');
  }
  let text;
  try {
    text = new TextDecoder('utf-8', { fatal: true }).decode(buf);
  } catch (err) {
    errors.push(`not UTF-8 (${err.message})`);
    return errors;
  }
  const lines = text.split('\n');
  if (lines[lines.length - 1] === '') {
    lines.pop();
  }
  if (lines.length < 2) {
    errors.push('need a header and at least one frame');
    return errors;
  }
  if (lines.some((line) => line.length === 0)) {
    errors.push('blank line');
  }
  let header;
  try {
    header = JSON.parse(lines[0]);
  } catch (err) {
    errors.push(`header JSON: ${err.message}`);
    return errors;
  }
  if (!header || typeof header !== 'object' || Array.isArray(header)) {
    errors.push('header is not an object');
    return errors;
  }
  if (!sameKeys(header, HEADER_KEYS)) {
    errors.push(`header fields must be ${HEADER_KEYS.join(',')}`);
  }
  if (header.schema !== SCHEMA_V1 && header.schema !== SCHEMA_V2) {
    errors.push(`schema ${JSON.stringify(header.schema)}`);
  }
  if (typeof header.name !== 'string' || header.name.length === 0) {
    errors.push('name must be a non-empty string');
  }
  if (typeof header.action !== 'string' || header.action.length === 0) {
    errors.push('action must be a non-empty string');
  }
  if (!VARIANTS.has(header.variant)) {
    errors.push(`variant ${JSON.stringify(header.variant)}`);
  }
  if (!Array.isArray(header.hands) || header.hands.length === 0 || header.hands.length > 2) {
    errors.push('hands must be a non-empty subset of L,R');
  } else {
    const allowed = header.hands.every((hand, i) => hand === 'L' || hand === 'R');
    const unique = new Set(header.hands).size === header.hands.length;
    const order = header.hands.length < 2 || (header.hands[0] === 'L' && header.hands[1] === 'R');
    if (!allowed || !unique || !order) {
      errors.push(`hands ${JSON.stringify(header.hands)} (L before R, no duplicates)`);
    }
  }
  if (header.hz !== 72) {
    errors.push(`hz ${JSON.stringify(header.hz)}`);
  }
  if (header.space !== SPACE) {
    errors.push('space string mismatch');
  }
  if (!Array.isArray(header.keypoints) || header.keypoints.length !== 26
    || header.keypoints.some((name, i) => name !== KEYPOINTS[i])) {
    errors.push('keypoints are not the 26 EHandKeypoint names in enum order');
  }
  if (!SOURCES.has(header.source)) {
    errors.push(`source ${JSON.stringify(header.source)}`);
  }
  if (!Number.isInteger(header.seed) || header.seed < 0 || header.seed > 4294967295) {
    errors.push(`seed ${JSON.stringify(header.seed)}`);
  }
  const base = path.basename(file, '.jsonl');
  if (typeof header.action === 'string' && VARIANTS.has(header.variant)) {
    const parts = base.split('.');
    const idOk = parts.length === 2
      || (parts.length === 3 && /^[A-Za-z0-9_-]+$/.test(parts[2]));
    if (!idOk || parts[0] !== header.action || parts[1] !== header.variant) {
      errors.push(`file name ${path.basename(file)} does not match ${header.action}.${header.variant}[.id].jsonl`);
    }
  }

  const bSchemaV2 = header.schema === SCHEMA_V2;
  const declared = new Set(Array.isArray(header.hands) ? header.hands : []);
  const perHand = new Map();
  let prevT = -Infinity;
  const expectedDt = 1 / 72;
  for (let i = 1; i < lines.length; i++) {
    if (lines[i].length === 0) {
      continue;
    }
    let frame;
    try {
      frame = JSON.parse(lines[i]);
    } catch (err) {
      errors.push(`line ${i + 1}: ${err.message}`);
      continue;
    }
    const where = `line ${i + 1}`;
    if (!frame || typeof frame !== 'object' || Array.isArray(frame)) {
      errors.push(`${where}: frame is not an object`);
      continue;
    }
    const hasSys = Object.prototype.hasOwnProperty.call(frame, 'sys');
    if (!sameKeys(frame, hasSys && bSchemaV2 ? FRAME_KEYS_SYS : FRAME_KEYS)) {
      errors.push(`${where}: frame fields must be ${FRAME_KEYS.join(',')}${bSchemaV2 ? ' (sys optional after pinch)' : ''}`);
      continue;
    }
    if (hasSys && typeof frame.sys !== 'boolean') {
      errors.push(`${where}: sys must be a boolean`);
    }
    if (typeof frame.t !== 'number' || !Number.isFinite(frame.t) || frame.t < 0) {
      errors.push(`${where}: t`);
    } else if (frame.t < prevT) {
      errors.push(`${where}: t went backwards`);
    }
    prevT = typeof frame.t === 'number' ? frame.t : prevT;
    if (frame.hand !== 'L' && frame.hand !== 'R') {
      errors.push(`${where}: hand`);
    } else if (!declared.has(frame.hand)) {
      errors.push(`${where}: hand ${frame.hand} is not in the header`);
    }
    if (typeof frame.conf !== 'number' || !Number.isFinite(frame.conf) || frame.conf < 0 || frame.conf > 1) {
      errors.push(`${where}: conf`);
    }
    if (typeof frame.pinch !== 'number' || !Number.isFinite(frame.pinch) || frame.pinch < 0 || frame.pinch > 1) {
      errors.push(`${where}: pinch`);
    }
    if (!Array.isArray(frame.joints) || frame.joints.length !== 26) {
      errors.push(`${where}: joints must have length 26`);
    } else {
      frame.joints.forEach((joint, j) => {
        if (!Array.isArray(joint) || joint.length !== 7 || joint.some((n) => typeof n !== 'number' || !Number.isFinite(n))) {
          errors.push(`${where}: joint ${j} is not 7 finite numbers`);
          return;
        }
        const qn = Math.hypot(joint[3], joint[4], joint[5], joint[6]);
        if (Math.abs(qn - 1) > 1e-3) {
          errors.push(`${where}: joint ${j} quaternion norm ${qn}`);
        }
      });
    }
    if (typeof frame.t === 'number' && (frame.hand === 'L' || frame.hand === 'R')) {
      const list = perHand.get(frame.hand) || [];
      list.push(frame.t);
      perHand.set(frame.hand, list);
    }
  }

  for (const hand of declared) {
    const times = perHand.get(hand) || [];
    if (times.length < 2) {
      errors.push(`hand ${hand} needs at least 2 samples`);
      continue;
    }
    if (times[0] !== 0) {
      errors.push(`hand ${hand} must start at t=0`);
    }
    for (let i = 1; i < times.length; i++) {
      if (!(times[i] > times[i - 1])) {
        errors.push(`hand ${hand} t is not strictly increasing at sample ${i}`);
      }
      const dt = times[i] - times[i - 1];
      if (dt < expectedDt * 0.9 || dt > expectedDt * 1.1) {
        errors.push(`hand ${hand} spacing ${dt} is outside 10% of 1/hz`);
        break;
      }
    }
  }
  if (declared.has('L') && declared.has('R')) {
    const lt = perHand.get('L') || [];
    const rt = perHand.get('R') || [];
    if (lt.length !== rt.length || lt.some((t, i) => t !== rt[i])) {
      errors.push('L and R must share timestamps');
    }
  }
  return errors;
}

function summarise(file, errors, bufText) {
  const name = path.basename(file);
  if (errors.length > 0) {
    return `FAIL ${name} ${errors.join('; ')}`;
  }
  const lines = bufText.split('\n').filter((line) => line.length > 0);
  const header = JSON.parse(lines[0]);
  let tMax = 0;
  for (let i = 1; i < lines.length; i++) {
    tMax = Math.max(tMax, JSON.parse(lines[i]).t);
  }
  const frames = lines.length - 1;
  return `ok ${name} frames=${frames} hands=${header.hands.join(',')} duration=${tMax.toFixed(6)}s`;
}

function collectJsonl(dir, out) {
  for (const entry of fs.readdirSync(dir, { withFileTypes: true })) {
    const full = path.join(dir, entry.name);
    if (entry.isDirectory()) {
      collectJsonl(full, out);
    } else if (entry.name.endsWith('.jsonl')) {
      out.push(full);
    }
  }
}

function main() {
  const dir = path.resolve(process.cwd(), process.argv[2] || path.join('Game', 'Clips'));
  if (!fs.existsSync(dir) || !fs.statSync(dir).isDirectory()) {
    process.stdout.write(`FAIL ${dir} is not a directory\n`);
    process.stdout.write('0 clips, 1 violations\n');
    process.exit(1);
  }
  const files = [];
  collectJsonl(dir, files);
  files.sort((a, b) => a.localeCompare(b));
  let violations = 0;
  for (const file of files) {
    const errors = validateFile(file);
    const text = fs.readFileSync(file, 'utf8');
    if (errors.length > 0) {
      violations += errors.length;
    }
    const shown = summarise(file, errors, text).replace(path.basename(file), path.relative(dir, file).split(path.sep).join('/'));
    process.stdout.write(`${shown}\n`);
  }
  process.stdout.write(`${files.length} clips, ${violations} violations\n`);
  process.exit(violations === 0 ? 0 : 1);
}

main();
