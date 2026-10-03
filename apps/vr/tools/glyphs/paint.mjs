// Deterministic rasteriser. Coverage is analytic (distance to a ring, a disk, or a
// segment) and composited in float, then quantised once. Glow is three box blurs
// whose widths come from the fixed sigma. PNG has IHDR, one IDAT, IEND: no tIME,
// no text, filter None, zlib level 9 with explicit parameters.
import zlib from 'node:zlib';

const CRC_TABLE = (() => {
  const table = new Uint32Array(256);
  for (let n = 0; n < 256; n++) {
    let c = n;
    for (let k = 0; k < 8; k++) {
      c = (c & 1) ? (0xedb88320 ^ (c >>> 1)) : (c >>> 1);
    }
    table[n] = c >>> 0;
  }
  return table;
})();

function crc32(buf) {
  let c = 0xffffffff;
  for (let i = 0; i < buf.length; i++) {
    c = CRC_TABLE[(c ^ buf[i]) & 0xff] ^ (c >>> 8);
  }
  return (c ^ 0xffffffff) >>> 0;
}

function chunk(type, data) {
  const length = Buffer.alloc(4);
  length.writeUInt32BE(data.length, 0);
  const name = Buffer.from(type, 'ascii');
  const crc = Buffer.alloc(4);
  crc.writeUInt32BE(crc32(Buffer.concat([name, data])), 0);
  return Buffer.concat([length, name, data, crc]);
}

export function encodePng(width, height, rgba) {
  if (rgba.length !== width * height * 4) {
    throw new Error(`rgba length ${rgba.length} does not match ${width}x${height}`);
  }
  const stride = width * 4 + 1;
  const raw = Buffer.alloc(stride * height);
  for (let y = 0; y < height; y++) {
    raw[y * stride] = 0;
    rgba.copy(raw, y * stride + 1, y * width * 4, (y + 1) * width * 4);
  }
  const idat = zlib.deflateSync(raw, {
    level: 9,
    windowBits: 15,
    memLevel: 8,
    strategy: zlib.constants.Z_DEFAULT_STRATEGY,
  });
  const ihdr = Buffer.alloc(13);
  ihdr.writeUInt32BE(width, 0);
  ihdr.writeUInt32BE(height, 4);
  ihdr[8] = 8;
  ihdr[9] = 6;
  ihdr[10] = 0;
  ihdr[11] = 0;
  ihdr[12] = 0;
  const signature = Buffer.from([137, 80, 78, 71, 13, 10, 26, 10]);
  return Buffer.concat([signature, chunk('IHDR', ihdr), chunk('IDAT', idat), chunk('IEND', Buffer.alloc(0))]);
}

function clamp01(value) {
  if (value < 0) return 0;
  if (value > 1) return 1;
  return value;
}

function cover(half, distance) {
  return clamp01(half + 0.5 - distance);
}

function scaleDrawables(drawables, size) {
  const k = size / 256;
  return drawables.map((drawable) => {
    if (drawable.type === 'ring') {
      return {
        type: 'ring',
        cx: drawable.cx * k,
        cy: drawable.cy * k,
        r: drawable.r * k,
        width: drawable.width * k,
      };
    }
    if (drawable.type === 'disk') {
      return { type: 'disk', cx: drawable.cx * k, cy: drawable.cy * k, r: drawable.r * k };
    }
    if (drawable.type === 'segment') {
      return {
        type: 'segment',
        x1: drawable.x1 * k,
        y1: drawable.y1 * k,
        x2: drawable.x2 * k,
        y2: drawable.y2 * k,
        width: drawable.width * k,
      };
    }
    if (drawable.type === 'polyline') {
      return {
        type: 'polyline',
        width: drawable.width * k,
        points: drawable.points.map((point) => ({ x: point.x * k, y: point.y * k })),
      };
    }
    throw new Error(`unknown drawable ${drawable.type}`);
  });
}

export function widen(drawables, factor) {
  return drawables.map((drawable) => {
    if (drawable.type === 'disk') {
      return { ...drawable, r: drawable.r * factor };
    }
    return { ...drawable, width: drawable.width * factor };
  });
}

function paintSegment(coverage, size, x1, y1, x2, y2, width) {
  const half = width / 2;
  const pad = half + 1;
  const minX = Math.max(0, Math.floor(Math.min(x1, x2) - pad));
  const maxX = Math.min(size - 1, Math.ceil(Math.max(x1, x2) + pad));
  const minY = Math.max(0, Math.floor(Math.min(y1, y2) - pad));
  const maxY = Math.min(size - 1, Math.ceil(Math.max(y1, y2) + pad));
  const dx = x2 - x1;
  const dy = y2 - y1;
  const len2 = dx * dx + dy * dy;
  for (let y = minY; y <= maxY; y++) {
    const py = y + 0.5;
    const row = y * size;
    for (let x = minX; x <= maxX; x++) {
      const px = x + 0.5;
      let dist;
      if (len2 < 1.0e-12) {
        dist = Math.hypot(px - x1, py - y1);
      } else {
        let t = ((px - x1) * dx + (py - y1) * dy) / len2;
        if (t < 0) t = 0;
        else if (t > 1) t = 1;
        dist = Math.hypot(px - (x1 + t * dx), py - (y1 + t * dy));
      }
      const value = cover(half, dist);
      if (value > coverage[row + x]) {
        coverage[row + x] = value;
      }
    }
  }
}

export function rasterMask(drawables, size) {
  const scaled = scaleDrawables(drawables, size);
  const coverage = new Float64Array(size * size);
  for (const drawable of scaled) {
    if (drawable.type === 'ring') {
      const half = drawable.width / 2;
      const outer = drawable.r + half + 1;
      const minX = Math.max(0, Math.floor(drawable.cx - outer));
      const maxX = Math.min(size - 1, Math.ceil(drawable.cx + outer));
      const minY = Math.max(0, Math.floor(drawable.cy - outer));
      const maxY = Math.min(size - 1, Math.ceil(drawable.cy + outer));
      for (let y = minY; y <= maxY; y++) {
        const py = y + 0.5 - drawable.cy;
        const row = y * size;
        for (let x = minX; x <= maxX; x++) {
          const px = x + 0.5 - drawable.cx;
          const dist = Math.abs(Math.hypot(px, py) - drawable.r);
          const value = cover(half, dist);
          if (value > coverage[row + x]) {
            coverage[row + x] = value;
          }
        }
      }
    } else if (drawable.type === 'disk') {
      const outer = drawable.r + 1;
      const minX = Math.max(0, Math.floor(drawable.cx - outer));
      const maxX = Math.min(size - 1, Math.ceil(drawable.cx + outer));
      const minY = Math.max(0, Math.floor(drawable.cy - outer));
      const maxY = Math.min(size - 1, Math.ceil(drawable.cy + outer));
      for (let y = minY; y <= maxY; y++) {
        const py = y + 0.5 - drawable.cy;
        const row = y * size;
        for (let x = minX; x <= maxX; x++) {
          const px = x + 0.5 - drawable.cx;
          const value = cover(drawable.r, Math.hypot(px, py));
          if (value > coverage[row + x]) {
            coverage[row + x] = value;
          }
        }
      }
    } else if (drawable.type === 'segment') {
      paintSegment(coverage, size, drawable.x1, drawable.y1, drawable.x2, drawable.y2, drawable.width);
    } else if (drawable.type === 'polyline') {
      const points = drawable.points;
      for (let i = 1; i < points.length; i++) {
        paintSegment(coverage, size, points[i - 1].x, points[i - 1].y, points[i].x, points[i].y, drawable.width);
      }
    }
  }
  return coverage;
}

function boxBlur(src, size, radius) {
  if (radius <= 0) {
    return src;
  }
  const tmp = new Float64Array(size * size);
  const out = new Float64Array(size * size);
  const win = radius * 2 + 1;
  const prefix = new Float64Array(size + 1);
  for (let y = 0; y < size; y++) {
    const row = y * size;
    prefix[0] = 0;
    for (let x = 0; x < size; x++) {
      prefix[x + 1] = prefix[x] + src[row + x];
    }
    for (let x = 0; x < size; x++) {
      const left = Math.max(0, x - radius);
      const right = Math.min(size - 1, x + radius);
      tmp[row + x] = (prefix[right + 1] - prefix[left]) / win;
    }
  }
  for (let x = 0; x < size; x++) {
    prefix[0] = 0;
    for (let y = 0; y < size; y++) {
      prefix[y + 1] = prefix[y] + tmp[y * size + x];
    }
    for (let y = 0; y < size; y++) {
      const top = Math.max(0, y - radius);
      const bottom = Math.min(size - 1, y + radius);
      out[y * size + x] = (prefix[bottom + 1] - prefix[top]) / win;
    }
  }
  return out;
}

/** Three box widths approximating a gaussian of the given sigma. */
export function boxRadii(sigma) {
  const n = 3;
  const wIdeal = Math.sqrt((12 * sigma * sigma) / n + 1);
  let wl = Math.floor(wIdeal);
  if (wl % 2 === 0) {
    wl -= 1;
  }
  if (wl < 1) {
    wl = 1;
  }
  const wu = wl + 2;
  const mIdeal = (12 * sigma * sigma - n * wl * wl - 4 * n * wl - 3 * n) / (-4 * wl - 4);
  let m = Math.round(mIdeal);
  if (m < 0) m = 0;
  if (m > n) m = n;
  const radii = [];
  for (let i = 0; i < n; i++) {
    const width = i < m ? wl : wu;
    radii.push((width - 1) >> 1);
  }
  return radii;
}

export function glowBlur(src, size, sigma) {
  const radii = boxRadii(sigma);
  let image = src;
  for (const radius of radii) {
    image = boxBlur(image, size, radius);
  }
  return image;
}

function grain(x, y) {
  let n = Math.imul(x + 1, 374761393) ^ Math.imul(y + 1, 668265263);
  n = Math.imul(n ^ (n >>> 13), 1274126177);
  return (n >>> 0) % 10000;
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

function sample(coverage, size, x, y) {
  if (x < 0 || y < 0 || x >= size || y >= size) {
    return 0;
  }
  return coverage[y * size + x];
}

function writePixel(rgba, index, color, alpha) {
  const a = clamp01(alpha);
  if (a <= 0) {
    rgba[index] = 0;
    rgba[index + 1] = 0;
    rgba[index + 2] = 0;
    rgba[index + 3] = 0;
    return;
  }
  rgba[index] = Math.floor(clamp01(color.r / 255) * 255 + 0.5);
  rgba[index + 1] = Math.floor(clamp01(color.g / 255) * 255 + 0.5);
  rgba[index + 2] = Math.floor(clamp01(color.b / 255) * 255 + 0.5);
  rgba[index + 3] = Math.floor(a * 255 + 0.5);
}

function over(dst, srcColor, srcAlpha) {
  const a = clamp01(srcAlpha);
  if (a <= 0) {
    return dst;
  }
  const outA = a + dst.a * (1 - a);
  if (outA <= 0) {
    return { r: 0, g: 0, b: 0, a: 0 };
  }
  return {
    r: (srcColor.r * a + dst.r * dst.a * (1 - a)) / outA,
    g: (srcColor.g * a + dst.g * dst.a * (1 - a)) / outA,
    b: (srcColor.b * a + dst.b * dst.a * (1 - a)) / outA,
    a: outA,
  };
}

function screenPrint(core, wide, size, palette) {
  const teal = hex(palette.teal);
  const orange = hex(palette['sunset orange']);
  const plum = hex(palette['deep plum']);
  const mustard = hex(palette.mustard);
  const dx = Math.round(size * 0.014);
  const dy = Math.round(size * 0.01);
  const cell = Math.max(4, Math.round(size / 28));
  const dotR = cell * 0.22;
  const rgba = Buffer.alloc(size * size * 4);
  for (let y = 0; y < size; y++) {
    const cy = y % cell - (cell >> 1);
    for (let x = 0; x < size; x++) {
      const cx = x % cell - (cell >> 1);
      const body = core[y * size + x];
      const plate = wide[y * size + x];
      const shifted = sample(core, size, x - dx, y - dy);
      let pixel = { r: 0, g: 0, b: 0, a: 0 };
      pixel = over(pixel, plum, plate);
      if (plate > 0.45 && body < 0.12 && cx * cx + cy * cy <= dotR * dotR) {
        pixel = over(pixel, mustard, 0.9);
      }
      pixel = over(pixel, orange, shifted);
      pixel = over(pixel, teal, body);
      writePixel(rgba, (y * size + x) * 4, pixel, pixel.a);
    }
  }
  return rgba;
}

function moonlit(core, size, palette) {
  const sigma = size * 0.028;
  const glow = glowBlur(core, size, sigma);
  const water = hex(palette.water);
  const silver = hex(palette['moon silver']);
  const slate = hex(palette.slate);
  const night = hex(palette['night blue']);
  const rgba = Buffer.alloc(size * size * 4);
  const bodyColor = mix(slate, water, 0.62);
  for (let i = 0; i < size * size; i++) {
    const body = core[i];
    const halo = Math.min(1, glow[i] * 1.35);
    let pixel = { r: 0, g: 0, b: 0, a: 0 };
    pixel = over(pixel, night, Math.min(1, halo * 0.28));
    pixel = over(pixel, water, halo * 0.72);
    const bright = mix(bodyColor, silver, body * body);
    pixel = over(pixel, bright, body);
    writePixel(rgba, i * 4, pixel, pixel.a);
  }
  return rgba;
}

function chalk(core, size, palette) {
  const blue = hex(palette['chalk blue']);
  const white = hex(palette['chalk white']);
  const yellow = hex(palette['chalk yellow']);
  const red = hex(palette['chalk red']);
  const slate = hex(palette.slate);
  const dx = Math.max(1, Math.round(size * 0.008));
  const dy = dx;
  const rgba = Buffer.alloc(size * size * 4);
  for (let y = 0; y < size; y++) {
    for (let x = 0; x < size; x++) {
      const body = core[y * size + x];
      const shadow = sample(core, size, x - dx, y - dy);
      const g = grain(x, y);
      let alpha = body;
      if (alpha > 0.05 && alpha < 0.55 && g > 6200) {
        alpha = 0;
      }
      let color = blue;
      if (body > 0.86) {
        color = mix(blue, white, (body - 0.86) / 0.14);
      }
      if (alpha > 0.25 && g < 280) {
        color = yellow;
      } else if (alpha > 0.4 && g > 9850) {
        color = red;
      }
      let pixel = { r: 0, g: 0, b: 0, a: 0 };
      pixel = over(pixel, slate, shadow * 0.55);
      pixel = over(pixel, color, alpha);
      writePixel(rgba, (y * size + x) * 4, pixel, pixel.a);
    }
  }
  return rgba;
}

export function colorize(core, wide, size, styleId, palette) {
  if (styleId === '13-screen-print') {
    return screenPrint(core, wide, size, palette);
  }
  if (styleId === '25-moonlit-silver') {
    return moonlit(core, size, palette);
  }
  if (styleId === '30-chalk-slate') {
    return chalk(core, size, palette);
  }
  throw new Error(`unknown style ${styleId}`);
}

export function maskRgba(core, size) {
  const rgba = Buffer.alloc(size * size * 4);
  for (let i = 0; i < size * size; i++) {
    const alpha = Math.floor(clamp01(core[i]) * 255 + 0.5);
    const index = i * 4;
    if (alpha === 0) {
      continue;
    }
    rgba[index] = 255;
    rgba[index + 1] = 255;
    rgba[index + 2] = 255;
    rgba[index + 3] = alpha;
  }
  return rgba;
}
