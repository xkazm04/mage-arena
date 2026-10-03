// Plane fit and pinch segmentation, matching Gestures/SigilStrokeBuilder.cpp.
// Positions in the clip are metres. The builder works in centimetres, so tips are scaled by 100.
import fs from 'node:fs';

export const INDEX_TIP = 10;
const PINCH_DOWN = 0.7;
const PINCH_UP = 0.5;

export function loadClip(file) {
  const text = fs.readFileSync(file, 'utf8').replace(/^\uFEFF/, '');
  const lines = text.split('\n').filter((line) => line.length > 0);
  if (lines.length < 2) {
    throw new Error(`clip has no frames: ${file}`);
  }
  return {
    header: JSON.parse(lines[0]),
    frames: lines.slice(1).map((line) => JSON.parse(line)),
  };
}

function jacobiRotate(A, V, p, q) {
  const apq = A[p][q];
  if (Math.abs(apq) < 1.0e-18) {
    return;
  }
  const app = A[p][p];
  const aqq = A[q][q];
  const tau = (aqq - app) / (2.0 * apq);
  const t = (tau >= 0.0 ? 1.0 : -1.0) / (Math.abs(tau) + Math.sqrt(1.0 + tau * tau));
  const c = 1.0 / Math.sqrt(1.0 + t * t);
  const s = t * c;
  A[p][p] = app - t * apq;
  A[q][q] = aqq + t * apq;
  A[p][q] = 0.0;
  A[q][p] = 0.0;
  for (let r = 0; r < 3; r++) {
    if (r === p || r === q) {
      continue;
    }
    const arp = A[r][p];
    const arq = A[r][q];
    A[r][p] = c * arp - s * arq;
    A[p][r] = A[r][p];
    A[r][q] = s * arp + c * arq;
    A[q][r] = A[r][q];
  }
  for (let r = 0; r < 3; r++) {
    const vrp = V[r][p];
    const vrq = V[r][q];
    V[r][p] = c * vrp - s * vrq;
    V[r][q] = s * vrp + c * vrq;
  }
}

/** Smallest-eigenvector plane. Normal is flipped toward seated +X. AxisX = AxisY × Normal. */
export function fitPlane(points) {
  if (points.length < 2) {
    return null;
  }
  let cx = 0;
  let cy = 0;
  let cz = 0;
  for (const p of points) {
    cx += p.x;
    cy += p.y;
    cz += p.z;
  }
  const n = points.length;
  cx /= n;
  cy /= n;
  cz /= n;

  const A = [
    [0, 0, 0],
    [0, 0, 0],
    [0, 0, 0],
  ];
  for (const p of points) {
    const dx = p.x - cx;
    const dy = p.y - cy;
    const dz = p.z - cz;
    A[0][0] += dx * dx;
    A[0][1] += dx * dy;
    A[0][2] += dx * dz;
    A[1][1] += dy * dy;
    A[1][2] += dy * dz;
    A[2][2] += dz * dz;
  }
  A[1][0] = A[0][1];
  A[2][0] = A[0][2];
  A[2][1] = A[1][2];

  const V = [
    [1, 0, 0],
    [0, 1, 0],
    [0, 0, 1],
  ];
  for (let iteration = 0; iteration < 16; iteration++) {
    jacobiRotate(A, V, 0, 1);
    jacobiRotate(A, V, 0, 2);
    jacobiRotate(A, V, 1, 2);
  }

  let smallest = 0;
  if (A[1][1] < A[smallest][smallest]) {
    smallest = 1;
  }
  if (A[2][2] < A[smallest][smallest]) {
    smallest = 2;
  }
  let nx = V[0][smallest];
  let ny = V[1][smallest];
  let nz = V[2][smallest];
  const nn = Math.hypot(nx, ny, nz);
  if (nn < 1.0e-12) {
    return null;
  }
  nx /= nn;
  ny /= nn;
  nz /= nn;
  if (nx * 1.0 + ny * 0.0 + nz * 0.0 < 0.0) {
    nx = -nx;
    ny = -ny;
    nz = -nz;
  }

  let yx = 0.0 - nx * nz;
  let yy = 0.0 - ny * nz;
  let yz = 1.0 - nz * nz;
  if (yx * yx + yy * yy + yz * yz < 1.0e-16) {
    yx = 0.0 - nx * ny;
    yy = 1.0 - ny * ny;
    yz = 0.0 - nz * ny;
  }
  const yl = Math.hypot(yx, yy, yz);
  if (yl < 1.0e-16) {
    return null;
  }
  yx /= yl;
  yy /= yl;
  yz /= yl;

  let xx = yy * nz - yz * ny;
  let xy = yz * nx - yx * nz;
  let xz = yx * ny - yy * nx;
  const xl = Math.hypot(xx, xy, xz);
  if (xl < 1.0e-16) {
    return null;
  }
  xx /= xl;
  xy /= xl;
  xz /= xl;

  return { cx, cy, cz, xx, xy, xz, yx, yy, yz, nx, ny, nz };
}

export function projectPoint(point, plane) {
  const dx = point.x - plane.cx;
  const dy = point.y - plane.cy;
  const dz = point.z - plane.cz;
  return {
    x: dx * plane.xx + dy * plane.xy + dz * plane.xz,
    y: dx * plane.yx + dy * plane.yy + dz * plane.yz,
  };
}

function tipCm(frame) {
  const joint = frame.joints[INDEX_TIP];
  return { x: joint[0] * 100, y: joint[1] * 100, z: joint[2] * 100 };
}

/**
 * Style A: pen down at pinch >= 0.7, up below 0.5. Right index tip only.
 * Returns one array of centimetre points per stroke. This is the point set
 * WritePoints would hand to the recognizer, before the circle check.
 */
export function extractRightIndexStrokes(frames) {
  let pen = false;
  const strokes = [];
  for (const frame of frames) {
    if (frame.hand !== 'R') {
      continue;
    }
    const was = pen;
    if (!pen) {
      if (frame.pinch >= PINCH_DOWN) {
        pen = true;
      }
    } else if (frame.pinch < PINCH_UP) {
      pen = false;
    }
    if (pen) {
      if (!was) {
        strokes.push([]);
      }
      strokes[strokes.length - 1].push(tipCm(frame));
    }
  }
  return strokes.filter((stroke) => stroke.length >= 2);
}

/** Project extracted strokes onto their best-fit plane. Null when the plane is degenerate. */
export function projectRightIndex(frames) {
  const strokes = extractRightIndexStrokes(frames);
  if (strokes.length === 0) {
    return null;
  }
  const plane = fitPlane(strokes.flat());
  if (!plane) {
    return null;
  }
  return strokes.map((stroke) => stroke.map((point) => projectPoint(point, plane)));
}

function trimHold(points) {
  let last = points.length - 1;
  const end = points[last];
  while (last > 1) {
    const prev = points[last - 1];
    if (prev.x !== end.x || prev.y !== end.y || prev.z !== end.z) {
      break;
    }
    last -= 1;
  }
  return points.slice(0, last + 1);
}

/**
 * Mudra is a two-hand pose, not a right-index pen stroke. Both index tips are
 * projected with the same plane and axes. The held tail (identical samples) is
 * one endpoint, so the path is the approach into the seal.
 */
export function projectMudra(frames) {
  const hands = { L: [], R: [] };
  for (const frame of frames) {
    if (frame.hand !== 'L' && frame.hand !== 'R') {
      continue;
    }
    hands[frame.hand].push(tipCm(frame));
  }
  if (hands.L.length < 2 || hands.R.length < 2) {
    return null;
  }
  const left = trimHold(hands.L);
  const right = trimHold(hands.R);
  const plane = fitPlane([...left, ...right]);
  if (!plane) {
    return null;
  }
  return {
    left: left.map((point) => projectPoint(point, plane)),
    right: right.map((point) => projectPoint(point, plane)),
    plane,
  };
}

function solve3(a00, a01, a02, a11, a12, a22, b0, b1, b2) {
  const m = [
    [a00, a01, a02],
    [a01, a11, a12],
    [a02, a12, a22],
  ];
  const b = [b0, b1, b2];
  for (let col = 0; col < 3; col++) {
    let pivot = col;
    for (let row = col + 1; row < 3; row++) {
      if (Math.abs(m[row][col]) > Math.abs(m[pivot][col])) {
        pivot = row;
      }
    }
    if (Math.abs(m[pivot][col]) < 1.0e-18) {
      return null;
    }
    if (pivot !== col) {
      const swap = m[col];
      m[col] = m[pivot];
      m[pivot] = swap;
      const sb = b[col];
      b[col] = b[pivot];
      b[pivot] = sb;
    }
    const div = m[col][col];
    for (let row = col + 1; row < 3; row++) {
      const f = m[row][col] / div;
      for (let k = col; k < 3; k++) {
        m[row][k] -= f * m[col][k];
      }
      b[row] -= f * b[col];
    }
  }
  const x = [0, 0, 0];
  for (let row = 2; row >= 0; row--) {
    let sum = b[row];
    for (let k = row + 1; k < 3; k++) {
      sum -= m[row][k] * x[k];
    }
    x[row] = sum / m[row][row];
  }
  return x;
}

function circleError(points, cx, cy, r) {
  let sum = 0;
  let max = 0;
  for (const point of points) {
    const error = Math.abs(Math.hypot(point.x - cx, point.y - cy) - r);
    sum += error * error;
    if (error > max) {
      max = error;
    }
  }
  return { rms: Math.sqrt(sum / points.length), max };
}

/** Geometric circle. Kåsa start, then a few Gauss-Newton steps. Null if it cannot fit. */
export function fitCircle(points) {
  if (points.length < 3) {
    return null;
  }
  let sx = 0;
  let sy = 0;
  let sxx = 0;
  let syy = 0;
  let sxy = 0;
  let sxz = 0;
  let syz = 0;
  let sz = 0;
  for (const point of points) {
    const z = point.x * point.x + point.y * point.y;
    sx += point.x;
    sy += point.y;
    sxx += point.x * point.x;
    syy += point.y * point.y;
    sxy += point.x * point.y;
    sxz += point.x * z;
    syz += point.y * z;
    sz += z;
  }
  const n = points.length;
  const algebraic = solve3(sxx, sxy, sx, syy, sy, n, sxz, syz, sz);
  if (!algebraic) {
    return null;
  }
  let cx = algebraic[0] / 2;
  let cy = algebraic[1] / 2;
  let r2 = algebraic[2] + cx * cx + cy * cy;
  if (!(r2 > 0)) {
    return null;
  }
  let r = Math.sqrt(r2);
  const seed = { cx, cy, r };
  for (let iteration = 0; iteration < 6; iteration++) {
    let a00 = 0;
    let a01 = 0;
    let a02 = 0;
    let a11 = 0;
    let a12 = 0;
    let a22 = 0;
    let b0 = 0;
    let b1 = 0;
    let b2 = 0;
    for (const point of points) {
      const dx = point.x - cx;
      const dy = point.y - cy;
      const dist = Math.hypot(dx, dy);
      if (dist < 1.0e-12) {
        continue;
      }
      const j0 = -dx / dist;
      const j1 = -dy / dist;
      const j2 = -1;
      const residual = dist - r;
      a00 += j0 * j0;
      a01 += j0 * j1;
      a02 += j0 * j2;
      a11 += j1 * j1;
      a12 += j1 * j2;
      a22 += j2 * j2;
      b0 += j0 * residual;
      b1 += j1 * residual;
      b2 += j2 * residual;
    }
    const step = solve3(a00, a01, a02, a11, a12, a22, b0, b1, b2);
    if (!step) {
      break;
    }
    cx -= step[0];
    cy -= step[1];
    r -= step[2];
    if (!(r > 0)) {
      cx = seed.cx;
      cy = seed.cy;
      r = seed.r;
      break;
    }
  }
  const error = circleError(points, cx, cy, r);
  return { cx, cy, r, rms: error.rms, max: error.max };
}
