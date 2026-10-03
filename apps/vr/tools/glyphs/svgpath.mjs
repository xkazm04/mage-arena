// Samples the SVG path commands this generator writes (M, L, A, C).
// Arc conversion follows the SVG implementation notes so a semicircle lands on the
// side the sweep flag selects. The round-trip check resamples these points with $Q.

function tokenize(d) {
  const tokens = d.match(/[a-zA-Z]|[-+]?(?:\d+\.?\d*|\.\d+)(?:[eE][-+]?\d+)?/g);
  if (!tokens) {
    throw new Error('empty path');
  }
  return tokens;
}

function angleBetween(ux, uy, vx, vy) {
  return Math.atan2(ux * vy - uy * vx, ux * vx + uy * vy);
}

function sampleArc(x1, y1, rx, ry, phiDeg, large, sweep, x2, y2, out) {
  if (Math.hypot(x2 - x1, y2 - y1) < 1.0e-9) {
    return;
  }
  let rxAbs = Math.abs(rx);
  let ryAbs = Math.abs(ry);
  const phi = (phiDeg * Math.PI) / 180;
  const cos = Math.cos(phi);
  const sin = Math.sin(phi);
  const dx2 = (x1 - x2) / 2;
  const dy2 = (y1 - y2) / 2;
  const x1p = cos * dx2 + sin * dy2;
  const y1p = -sin * dx2 + cos * dy2;
  let lam = (x1p * x1p) / (rxAbs * rxAbs) + (y1p * y1p) / (ryAbs * ryAbs);
  if (lam > 1) {
    const root = Math.sqrt(lam);
    rxAbs *= root;
    ryAbs *= root;
    lam = 1;
  }
  const sign = large === sweep ? -1 : 1;
  const num = rxAbs * rxAbs * ryAbs * ryAbs - rxAbs * rxAbs * y1p * y1p - ryAbs * ryAbs * x1p * x1p;
  const den = rxAbs * rxAbs * y1p * y1p + ryAbs * ryAbs * x1p * x1p;
  const coef = sign * Math.sqrt(Math.max(0, num / den));
  const cxp = coef * ((rxAbs * y1p) / ryAbs);
  const cyp = coef * ((-ryAbs * x1p) / rxAbs);
  const cx = cos * cxp - sin * cyp + (x1 + x2) / 2;
  const cy = sin * cxp + cos * cyp + (y1 + y2) / 2;
  const theta1 = angleBetween(1, 0, (x1p - cxp) / rxAbs, (y1p - cyp) / ryAbs);
  let delta = angleBetween(
    (x1p - cxp) / rxAbs,
    (y1p - cyp) / ryAbs,
    (-x1p - cxp) / rxAbs,
    (-y1p - cyp) / ryAbs,
  );
  if (!sweep && delta > 0) {
    delta -= Math.PI * 2;
  } else if (sweep && delta < 0) {
    delta += Math.PI * 2;
  }
  const length = Math.hypot(rxAbs, ryAbs) * Math.abs(delta);
  const steps = Math.max(2, Math.ceil(length / 2));
  for (let step = 1; step <= steps; step++) {
    const theta = theta1 + delta * (step / steps);
    const cosT = Math.cos(theta);
    const sinT = Math.sin(theta);
    out.push({
      x: cos * rxAbs * cosT - sin * ryAbs * sinT + cx,
      y: sin * rxAbs * cosT + cos * ryAbs * sinT + cy,
    });
  }
}

function sampleCubic(x0, y0, x1, y1, x2, y2, x3, y3, out) {
  const chord = Math.hypot(x3 - x0, y3 - y0);
  const cont = Math.hypot(x1 - x0, y1 - y0) + Math.hypot(x2 - x1, y2 - y1) + Math.hypot(x3 - x2, y3 - y2);
  const steps = Math.max(2, Math.ceil(Math.max(chord, cont) / 2));
  for (let step = 1; step <= steps; step++) {
    const t = step / steps;
    const u = 1 - t;
    out.push({
      x: u * u * u * x0 + 3 * u * u * t * x1 + 3 * u * t * t * x2 + t * t * t * x3,
      y: u * u * u * y0 + 3 * u * u * t * y1 + 3 * u * t * t * y2 + t * t * t * y3,
    });
  }
}

export function samplePath(d) {
  const tokens = tokenize(d);
  let index = 0;
  let command = '';
  let cx = 0;
  let cy = 0;
  let sx = 0;
  let sy = 0;
  const points = [];
  const read = () => {
    if (index >= tokens.length) {
      throw new Error(`path ended early after ${command}`);
    }
    const value = Number(tokens[index]);
    index += 1;
    if (!Number.isFinite(value)) {
      throw new Error(`expected a number in path, got ${tokens[index - 1]}`);
    }
    return value;
  };
  while (index < tokens.length) {
    if (/[a-zA-Z]/.test(tokens[index])) {
      command = tokens[index];
      index += 1;
    }
    const relative = command === command.toLowerCase();
    const op = command.toUpperCase();
    if (op === 'M') {
      const x = read() + (relative ? cx : 0);
      const y = read() + (relative ? cy : 0);
      cx = x;
      cy = y;
      sx = x;
      sy = y;
      points.push({ x, y });
      command = relative ? 'l' : 'L';
    } else if (op === 'L') {
      const x = read() + (relative ? cx : 0);
      const y = read() + (relative ? cy : 0);
      cx = x;
      cy = y;
      points.push({ x, y });
    } else if (op === 'A') {
      const rx = read();
      const ry = read();
      const phi = read();
      const large = read() !== 0;
      const sweep = read() !== 0;
      const x = read() + (relative ? cx : 0);
      const y = read() + (relative ? cy : 0);
      sampleArc(cx, cy, rx, ry, phi, large, sweep, x, y, points);
      cx = x;
      cy = y;
    } else if (op === 'C') {
      const x1 = read() + (relative ? cx : 0);
      const y1 = read() + (relative ? cy : 0);
      const x2 = read() + (relative ? cx : 0);
      const y2 = read() + (relative ? cy : 0);
      const x = read() + (relative ? cx : 0);
      const y = read() + (relative ? cy : 0);
      sampleCubic(cx, cy, x1, y1, x2, y2, x, y, points);
      cx = x;
      cy = y;
    } else if (op === 'Z') {
      cx = sx;
      cy = sy;
      points.push({ x: cx, y: cy });
    } else {
      throw new Error(`unsupported path command ${command}`);
    }
  }
  return points;
}

export function selfTest() {
  const right = samplePath('M 128 206 A 78 78 0 0 0 128 50');
  let maxX = -Infinity;
  for (const point of right) {
    if (point.x > maxX) maxX = point.x;
  }
  if (maxX < 200) {
    throw new Error(`arc sweep 0 from bottom to top went left (max x ${maxX})`);
  }
  const left = samplePath('M 128 50 A 78 78 0 0 0 128 206');
  let minX = Infinity;
  for (const point of left) {
    if (point.x < minX) minX = point.x;
  }
  if (minX > 56) {
    throw new Error(`arc sweep 0 from top to bottom did not pass the left side (min x ${minX})`);
  }
}
