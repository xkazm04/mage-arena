// $Q normalisation and cloud distance, matching Gestures/QPointCloudRecognizer.cpp.
// The LUT lower bound is only an early-abandon aid. The distance CloudMatch returns
// is the weighted greedy sum below, over starts 0, step, 2·step, … and both directions.
// Step is floor(sqrt(n)). Weights run n, n-1, …, 1. An equal nearest-point keeps the
// earlier unmatched slot (strict <), as the C++ scan does.

export const POINT_COUNT = 32;
export const LUT_SIZE = 64;
export const REJECT_DISTANCE = 26000;

function gridCoord(value) {
  const rounded = Math.floor(value + 0.5);
  if (rounded < 0) {
    return 0;
  }
  if (rounded > LUT_SIZE - 1) {
    return LUT_SIZE - 1;
  }
  return rounded;
}

/** Integer grid of POINT_COUNT points, or null when the path has no extent. */
export function normalize(points) {
  if (!points || points.length === 0) {
    return null;
  }
  let path = 0;
  for (let index = 1; index < points.length; index++) {
    if (points[index].strokeId !== points[index - 1].strokeId) {
      continue;
    }
    path += Math.hypot(points[index].x - points[index - 1].x, points[index].y - points[index - 1].y);
  }

  const working = points.map((point) => ({ x: point.x, y: point.y, strokeId: point.strokeId }));
  const resampled = [];
  if (path < 1.0e-8) {
    for (let index = 0; index < POINT_COUNT; index++) {
      resampled.push({ x: points[0].x, y: points[0].y, strokeId: points[0].strokeId });
    }
  } else {
    const interval = path / (POINT_COUNT - 1);
    resampled.push({ x: working[0].x, y: working[0].y, strokeId: working[0].strokeId });
    let accumulated = 0;
    for (let index = 1; index < working.length && resampled.length < POINT_COUNT; index++) {
      if (working[index].strokeId !== working[index - 1].strokeId) {
        continue;
      }
      const dx = working[index].x - working[index - 1].x;
      const dy = working[index].y - working[index - 1].y;
      const dist = Math.hypot(dx, dy);
      if (accumulated + dist >= interval && dist > 1.0e-12) {
        const t = (interval - accumulated) / dist;
        const inserted = {
          x: working[index - 1].x + t * dx,
          y: working[index - 1].y + t * dy,
          strokeId: working[index].strokeId,
        };
        resampled.push(inserted);
        working.splice(index, 0, inserted);
        accumulated = 0;
      } else {
        accumulated += dist;
      }
    }
    if (resampled.length === POINT_COUNT - 1) {
      const last = working[working.length - 1];
      resampled.push({ x: last.x, y: last.y, strokeId: last.strokeId });
    }
    while (resampled.length < POINT_COUNT) {
      const last = resampled[resampled.length - 1];
      resampled.push({ x: last.x, y: last.y, strokeId: last.strokeId });
    }
    if (resampled.length > POINT_COUNT) {
      resampled.length = POINT_COUNT;
    }
  }

  let centroidX = 0;
  let centroidY = 0;
  for (const point of resampled) {
    centroidX += point.x;
    centroidY += point.y;
  }
  centroidX /= POINT_COUNT;
  centroidY /= POINT_COUNT;

  let minX = Infinity;
  let minY = Infinity;
  let maxX = -Infinity;
  let maxY = -Infinity;
  for (const point of resampled) {
    point.x -= centroidX;
    point.y -= centroidY;
    if (point.x < minX) minX = point.x;
    if (point.y < minY) minY = point.y;
    if (point.x > maxX) maxX = point.x;
    if (point.y > maxY) maxY = point.y;
  }
  const span = Math.max(maxX - minX, maxY - minY);
  if (span < 1.0e-8) {
    return null;
  }
  const scale = span / (LUT_SIZE - 1);
  const grid = new Array(POINT_COUNT);
  for (let index = 0; index < POINT_COUNT; index++) {
    grid[index] = {
      x: gridCoord((resampled[index].x - minX) / scale),
      y: gridCoord((resampled[index].y - minY) / scale),
    };
  }
  return grid;
}

function squared(a, b) {
  const dx = a.x - b.x;
  const dy = a.y - b.y;
  return dx * dx + dy * dy;
}

function cloudOneWay(from, to, start) {
  const unmatched = new Array(POINT_COUNT);
  for (let index = 0; index < POINT_COUNT; index++) {
    unmatched[index] = index;
  }
  let sum = 0;
  let cursor = start;
  let weight = POINT_COUNT;
  let matched = 0;
  do {
    let bestSlot = matched;
    let bestDistance = Infinity;
    const fromPoint = from[cursor];
    for (let slot = matched; slot < POINT_COUNT; slot++) {
      const distance = squared(fromPoint, to[unmatched[slot]]);
      if (distance < bestDistance) {
        bestDistance = distance;
        bestSlot = slot;
      }
    }
    unmatched[bestSlot] = unmatched[matched];
    sum += weight * bestDistance;
    weight -= 1;
    cursor = (cursor + 1) % POINT_COUNT;
    matched += 1;
  } while (cursor !== start);
  return sum;
}

/** Completed $Q cloud distance. No early abandon, so every pair yields its real minimum. */
export function cloudDistance(a, b) {
  const step = Math.floor(Math.sqrt(POINT_COUNT));
  let best = Infinity;
  for (let index = 0; index < POINT_COUNT; index += step) {
    const forward = cloudOneWay(a, b, index);
    const reverse = cloudOneWay(b, a, index);
    if (forward < best) best = forward;
    if (reverse < best) best = reverse;
  }
  return best;
}

export function strokesToPoints(strokes) {
  const points = [];
  strokes.forEach((stroke, strokeId) => {
    for (const point of stroke) {
      points.push({ x: point.x, y: point.y, strokeId });
    }
  });
  return points;
}
