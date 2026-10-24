# The parts of apps/vr/tools/assets/finish.py that need no Blender, so they can be tested on any Python with numpy
# (python -m unittest apps/vr/tools/assets/test_finish.py): the finish options (mode and style clamp), sRGB <-> CIE Lab,
# the style clamp, UV coverage of a texture, the accounting and filling of bake misses, and the tangent-space normal map
# (miss fill toward flat, its deviation from flat, a 16-bit PNG writer).
#
# Modes. 'flat' (the default, the T38 recipe): every face takes the nearest palette colour of its baked texels, one flat
# colour per material. 'bake': the LOD0 keeps the high-to-low base-colour bake as its texture, one material.
# Style clamp (bake mode only, off by default): pulls the baked texture toward the VISUAL-PLAN (c) palette without ever
# replacing it. 'posterize' snaps CIE L* part of the way to a few levels; 'hue' turns each texel's hue part of the way to
# the nearest palette hue; 'both' does both. Strength is the fraction of the way (at most MAX_STRENGTH), never all of it,
# and neither step touches chroma, so the texture's detail survives (detail_kept measures it).
# Normal (bake mode only, off by default): normal=on also bakes the source's surface normal in the LOD0's tangent space
# (T_<name>_Normal.png, OpenGL Y+, linear 16-bit) and feeds it to the material through a Normal Map node.
import math
import struct
import zlib

import numpy as np

MODES = ('flat', 'bake')
CLAMPS = ('none', 'posterize', 'hue', 'both')
NORMALS = ('off', 'on')
MAX_STRENGTH = 0.6  # above this a posterize starts to read as flat bands
MIN_LEVELS = 4
DEFAULTS = {'mode': 'flat', 'clamp': 'none', 'strength': 0.35, 'levels': 8, 'cage': 0.02, 'ray': 0.08, 'margin': 16, 'normal': 'off'}

# The VISUAL-PLAN (c) structure accents a character can carry (bronze brooch and buckle, gold trim, crimson cloth, iron),
# sRGB. The element colours (water turquoise, fire vermilion) are reserved for magic and are never clamp targets.
STYLE_ACCENTS = {
    'Bronze': (0.60, 0.42, 0.22),
    'Gold': (0.80, 0.62, 0.25),
    'Crimson': (0.55, 0.10, 0.12),
    'Iron': (0.40, 0.41, 0.42),
}
HUE_MIN_CHROMA = 8.0  # an anchor (or a texel) with less Lab chroma than this has no hue worth snapping


def split_args(a):
    """finish.py's arguments after '--': 7 positionals, an optional palette set, then key=value options."""
    if len(a) < 7:
        raise ValueError('usage: <hi.glb> <outdir> <name> <rot_z_deg> <bake_res> <voxel_m> <target_tris> [<palette set>] [key=value ...]')
    pos, rest = list(a[:7]), list(a[7:])
    palette = rest.pop(0) if rest and '=' not in rest[0] else 'Brennic'
    return pos, palette, parse_options(rest)


def parse_options(tokens):
    o = dict(DEFAULTS)
    for t in tokens:
        if '=' not in t:
            raise ValueError(f'option {t!r} is not key=value')
        k, v = t.split('=', 1)
        if k not in DEFAULTS:
            raise ValueError(f'unknown option {k!r}; known: {", ".join(DEFAULTS)}')
        o[k] = type(DEFAULTS[k])(v) if k not in ('mode', 'clamp', 'normal') else v
    if o['mode'] not in MODES:
        raise ValueError(f'mode {o["mode"]!r}: one of {MODES}')
    if o['clamp'] not in CLAMPS:
        raise ValueError(f'clamp {o["clamp"]!r}: one of {CLAMPS}')
    if o['normal'] not in NORMALS:
        raise ValueError(f'normal {o["normal"]!r}: one of {NORMALS}')
    if o['mode'] == 'flat' and o['clamp'] != 'none':
        raise ValueError('the style clamp works on a baked texture: it needs mode=bake')
    if o['mode'] == 'flat' and o['normal'] != 'off':
        raise ValueError('the normal bake shares the bake mode UV layout and hit mask: it needs mode=bake')
    if not 0 < o['strength'] <= MAX_STRENGTH:
        raise ValueError(f'strength {o["strength"]}: in (0, {MAX_STRENGTH}]')
    if o['levels'] < MIN_LEVELS:
        raise ValueError(f'levels {o["levels"]}: at least {MIN_LEVELS}')
    if o['cage'] < 0 or o['ray'] < 0 or o['margin'] < 0:
        raise ValueError('cage, ray and margin are not negative')
    return o


# ---- colour: sRGB (0-1, display) <-> linear <-> XYZ (D65) <-> CIE Lab
_M = np.array([[0.4124564, 0.3575761, 0.1804375], [0.2126729, 0.7151522, 0.0721750], [0.0193339, 0.1191920, 0.9503041]])
_MI = np.linalg.inv(_M)
_WHITE = _M.sum(1)
_E = 216 / 24389
_K = 24389 / 27


def srgb_to_linear(c):
    c = np.asarray(c, dtype=np.float64)
    return np.where(c <= 0.04045, c / 12.92, ((c + 0.055) / 1.055) ** 2.4)


def linear_to_srgb(c):
    c = np.clip(np.asarray(c, dtype=np.float64), 0, 1)
    return np.where(c <= 0.0031308, c * 12.92, 1.055 * c ** (1 / 2.4) - 0.055)


def srgb_to_lab(c):
    xyz = srgb_to_linear(c) @ _M.T / _WHITE
    f = np.where(xyz > _E, np.cbrt(xyz), (_K * xyz + 16) / 116)
    return np.stack([116 * f[..., 1] - 16, 500 * (f[..., 0] - f[..., 1]), 200 * (f[..., 1] - f[..., 2])], -1)


def lab_to_srgb(lab):
    lab = np.asarray(lab, dtype=np.float64)
    fy = (lab[..., 0] + 16) / 116
    f = np.stack([fy + lab[..., 1] / 500, fy, fy - lab[..., 2] / 200], -1)
    xyz = np.where(f ** 3 > _E, f ** 3, (116 * f - 16) / _K)
    xyz[..., 1] = np.where(lab[..., 0] > _K * _E, fy ** 3, lab[..., 0] / _K)
    return linear_to_srgb((xyz * _WHITE) @ _MI.T)


def anchor_hues(palette):
    """Lab hue angles (radians) of the palette entries that have a hue, plus the shared style accents."""
    cols = list(palette.values()) + list(STYLE_ACCENTS.values())
    lab = srgb_to_lab(np.array(cols))
    keep = np.hypot(lab[:, 1], lab[:, 2]) >= HUE_MIN_CHROMA
    return np.arctan2(lab[keep, 2], lab[keep, 1])


def style_clamp(rgb, palette, clamp, strength=DEFAULTS['strength'], levels=DEFAULTS['levels']):
    """rgb: (..., 3) sRGB in 0-1. Returns the clamped copy; 'none' returns it unchanged. Never more than `strength` of
    the way to a level or a palette hue, so the output keeps the input's variation."""
    if clamp not in CLAMPS:
        raise ValueError(f'clamp {clamp!r}: one of {CLAMPS}')
    if not 0 < strength <= MAX_STRENGTH:
        raise ValueError(f'strength {strength}: in (0, {MAX_STRENGTH}]')
    if levels < MIN_LEVELS:
        raise ValueError(f'levels {levels}: at least {MIN_LEVELS}')
    rgb = np.asarray(rgb, dtype=np.float64)
    if clamp == 'none':
        return rgb.copy()
    lab = srgb_to_lab(rgb)
    if clamp in ('posterize', 'both'):
        step = 100.0 / levels
        lq = np.round(lab[..., 0] / step) * step
        lab[..., 0] += strength * (lq - lab[..., 0])
    if clamp in ('hue', 'both'):
        hues = anchor_hues(palette)
        c = np.hypot(lab[..., 1], lab[..., 2])
        h = np.arctan2(lab[..., 2], lab[..., 1])
        d = (hues[None, :] - h.reshape(-1, 1) + math.pi) % (2 * math.pi) - math.pi  # signed shortest turn to each anchor
        turn = d[np.arange(d.shape[0]), np.abs(d).argmin(1)].reshape(h.shape)
        w = strength * np.clip(c / HUE_MIN_CHROMA, 0, 1)  # greys have no hue to snap
        h2 = h + w * turn
        lab[..., 1], lab[..., 2] = c * np.cos(h2), c * np.sin(h2)
    return lab_to_srgb(lab)


def detail_kept(before, after, mask=None):
    """Mean absolute L* step between 4-neighbours, after over before (1.0 = all detail kept, 0 = flat)."""
    def grad(img):
        L = srgb_to_lab(img)[..., 0]
        gx, gy = np.abs(np.diff(L, axis=1)), np.abs(np.diff(L, axis=0))
        if mask is None:
            return gx.mean() + gy.mean()
        mx, my = mask[:, 1:] & mask[:, :-1], mask[1:, :] & mask[:-1, :]
        return gx[mx].mean() + gy[my].mean()
    g0 = grad(before)
    return float(grad(after) / g0) if g0 > 0 else 1.0


# ---- the bake: which texels the UV islands cover, which of those the bake missed, and filling them
def uv_coverage(uv_tris, res):
    """uv_tris: (n, 3, 2) UVs in 0-1. A (res, res) bool mask, row 0 at v = 0 (Blender's pixel order), True where a
    texel centre lies inside (or on the edge of) a triangle."""
    cov = np.zeros((res, res), dtype=bool)
    t = np.asarray(uv_tris, dtype=np.float64) * res - 0.5  # texel-centre coordinates: texel i has its centre at i
    for (ax, ay), (bx, by), (cx, cy) in t:
        x0, x1 = max(int(math.ceil(min(ax, bx, cx))), 0), min(int(math.floor(max(ax, bx, cx))), res - 1)
        y0, y1 = max(int(math.ceil(min(ay, by, cy))), 0), min(int(math.floor(max(ay, by, cy))), res - 1)
        if x0 > x1 or y0 > y1:
            continue
        den = (by - cy) * (ax - cx) + (cx - bx) * (ay - cy)
        if abs(den) < 1e-12:
            continue
        X, Y = np.meshgrid(np.arange(x0, x1 + 1), np.arange(y0, y1 + 1))
        l1 = ((by - cy) * (X - cx) + (cx - bx) * (Y - cy)) / den
        l2 = ((cy - ay) * (X - cx) + (ax - cx) * (Y - cy)) / den
        inside = (l1 >= -1e-9) & (l2 >= -1e-9) & (1 - l1 - l2 >= -1e-9)
        cov[y0:y1 + 1, x0:x1 + 1] |= inside
    return cov


def erode(mask, n=1):
    """Shrink a bool mask by n texels (4-neighbourhood); the border of the image counts as outside."""
    m = mask.copy()
    for _ in range(n):
        p = np.pad(m, 1, constant_values=False)
        m = m & p[:-2, 1:-1] & p[2:, 1:-1] & p[1:-1, :-2] & p[1:-1, 2:]
    return m


def bake_misses(hit, coverage):
    """hit: (res, res) values of a white-emission bake of the source (1 where a ray found the source, 0 where it did
    not). Returns (misses mask, counts). Island edges (one texel) are counted apart: there Blender's rasteriser and
    uv_coverage can disagree on a texel, so only the interior count is a fault."""
    miss = coverage & (np.asarray(hit) < 0.5)
    inner = erode(coverage, 1)
    return miss, {'covered': int(coverage.sum()), 'missed': int(miss.sum()), 'missed_interior': int((miss & inner).sum())}


def dark_mask(rgb, thresh=1.5 / 255):
    """Texels with every channel at or below `thresh` (byte 0 or 1): what an unhit bake texel looks like."""
    return np.asarray(rgb)[..., :3].max(-1) <= thresh


def dark_texels(rgb, mask, thresh=1.5 / 255):
    """How many texels inside `mask` are dark (dark_mask)."""
    return int((mask & dark_mask(rgb, thresh)).sum())


def fill_misses(rgb, miss, max_iter=256, known=None):
    """Fill missed texels from their known 8-neighbours, ring by ring (a dilation), growing only into `miss`. Returns
    (filled copy, rings used). `known` (default: every texel not missed, the margin Blender paints around the islands
    included) is what a ring may take colour from; finish.py passes the island texels alone for its second pass, so a
    texel the margin left black is refilled from the island it belongs to."""
    img = np.asarray(rgb, dtype=np.float64).copy()
    known = ~miss.copy() if known is None else known.copy()
    for it in range(max_iter):
        if known.all():
            return img, it
        acc = np.zeros_like(img)
        cnt = np.zeros(known.shape)
        pi = np.pad(img * known[..., None], ((1, 1), (1, 1), (0, 0)))
        pk = np.pad(known.astype(np.float64), 1)
        h, w = known.shape
        for dy in (-1, 0, 1):
            for dx in (-1, 0, 1):
                if dy or dx:
                    acc += pi[1 + dy:1 + dy + h, 1 + dx:1 + dx + w]
                    cnt += pk[1 + dy:1 + dy + h, 1 + dx:1 + dx + w]
        new = miss & ~known & (cnt > 0)
        if not new.any():
            return img, it
        img[new] = acc[new] / cnt[new][:, None]
        known |= new
    return img, max_iter


def fill_orphans(rgb, orphan, source):
    """Give orphaned texels (those no island path from a hit texel reaches: a UV island no ray hit at all) the per-channel
    median of the `source` texels (the hit ones). Returns (copy, colour)."""
    img = np.asarray(rgb, dtype=np.float64).copy()
    colour = np.median(img[source], axis=0)
    img[orphan] = colour
    return img, colour


def texel_density(uv_tris, tris_3d, res):
    """Texels per metre of surface: res x sqrt(UV area / 3D area). Also the UV area share of the square."""
    uv = np.asarray(uv_tris, dtype=np.float64)
    p = np.asarray(tris_3d, dtype=np.float64)
    e1, e2 = uv[:, 1] - uv[:, 0], uv[:, 2] - uv[:, 0]
    a_uv = 0.5 * np.abs(e1[:, 0] * e2[:, 1] - e1[:, 1] * e2[:, 0]).sum()
    a_3d = 0.5 * np.linalg.norm(np.cross(p[:, 1] - p[:, 0], p[:, 2] - p[:, 0]), axis=1).sum()
    return {'uv_share': round(float(a_uv), 4), 'surface_m2': round(float(a_3d), 4),
            'texels_per_m': round(float(res * math.sqrt(a_uv / a_3d)), 1) if a_3d > 0 else 0.0}


# ---- the tangent-space normal map: texels hold n * 0.5 + 0.5, (0.5, 0.5, 1.0) is the LOD0's own shading normal (flat)
FLAT_NORMAL = (0.5, 0.5, 1.0)


def fill_normal_misses(nrm, miss, coverage, margin):
    """nrm: (res, res, 3) baked tangent-space normal texels. Every missed island texel (no ray found the source: there is
    no relief to carry) becomes flat, never black. Then the margin is rebuilt from the island texels alone, `margin` rings
    out, and every texel outside that is flat, so mip levels near an island edge average island normals, not Blender's
    margin of an unhit texel. Returns (copy, counts)."""
    img = np.asarray(nrm, dtype=np.float64).copy()
    flat = np.array(FLAT_NORMAL)
    q = {'missed_not_flat_before_fill': int((miss & (np.abs(img - flat).max(-1) > 1.5 / 255)).sum()),
         'dark_where_hit': dark_texels(img, coverage & ~miss)}
    img[miss] = flat
    out = ~coverage
    img, q['margin_rings'] = fill_misses(img, out, max_iter=margin, known=coverage)
    grown = coverage
    for _ in range(q['margin_rings']):
        grown = _dilate(grown)
    img[~grown] = flat
    q['dark_after_fill'] = dark_texels(img, np.ones(coverage.shape, dtype=bool))
    return img, q


def _dilate(mask):
    """Grow a bool mask by one texel (8-neighbourhood)."""
    p = np.pad(mask, 1, constant_values=False)
    h, w = mask.shape
    out = mask.copy()
    for dy in (-1, 0, 1):
        for dx in (-1, 0, 1):
            out |= p[1 + dy:1 + dy + h, 1 + dx:1 + dx + w]
    return out


def normal_deviation(nrm, mask, thresholds=(10, 20)):
    """Angle (degrees) between each texel's decoded tangent-space normal and flat (0, 0, 1), over `mask`: the share of
    texels past each threshold and the mean. The measure of how much relief the map carries; a map of flat reads 0."""
    v = np.asarray(nrm, dtype=np.float64)[mask] * 2 - 1
    n = np.linalg.norm(v, axis=-1)
    z = np.where(n > 0, v[:, 2] / np.where(n > 0, n, 1), 1.0)
    ang = np.degrees(np.arccos(np.clip(z, -1, 1)))
    r = {'texels': int(ang.size), 'mean_deg': round(float(ang.mean()), 3) if ang.size else 0.0}
    for t in thresholds:
        r[f'share_over_{t}deg'] = round(float((ang > t).mean()), 4) if ang.size else 0.0
    return r


def back_region_faces(face_normals, centroids_z, zlo=0.5, zhi=1.5, min_back=0.5):
    """The back-cloak face rule: a face whose unit normal points back (+Y in the character frame, the front faces -Y) by
    more than `min_back` (within 60 degrees of straight back) with its centroid between zlo and zhi metres."""
    fn = np.asarray(face_normals, dtype=np.float64)
    cz = np.asarray(centroids_z, dtype=np.float64)
    return (fn[:, 1] > min_back) & (cz >= zlo) & (cz <= zhi)


def png16(path, rgb):
    """Write a (h, w, 3) float image in 0-1 as a 16-bit RGB PNG, linear: no gAMA, no sRGB chunk. Row 0 of `rgb` is the
    bottom (Blender's pixel order) and becomes the last PNG row."""
    a = np.round(np.clip(np.asarray(rgb, dtype=np.float64)[::-1], 0, 1) * 65535).astype('>u2')
    h, w = a.shape[:2]
    raw = b''.join(b'\x00' + a[y].tobytes() for y in range(h))

    def chunk(t, d):
        return struct.pack('>I', len(d)) + t + d + struct.pack('>I', zlib.crc32(t + d) & 0xffffffff)
    with open(path, 'wb') as f:
        f.write(b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 16, 2, 0, 0, 0))
                + chunk(b'IDAT', zlib.compress(raw, 6)) + chunk(b'IEND', b''))


def read_png16(path):
    """Read back a png16 file (no filters, as png16 writes it): (h, w, 3) float, row 0 at the bottom."""
    b = open(path, 'rb').read()
    i, idat, w, h = 8, b'', 0, 0
    while i < len(b):
        n = struct.unpack('>I', b[i:i + 4])[0]
        t, d = b[i + 4:i + 8], b[i + 8:i + 8 + n]
        if t == b'IHDR':
            w, h = struct.unpack('>II', d[:8])
        elif t == b'IDAT':
            idat += d
        i += 12 + n
    raw = zlib.decompress(idat)
    rows = [np.frombuffer(raw[y * (1 + 6 * w) + 1:(y + 1) * (1 + 6 * w)], dtype='>u2').reshape(w, 3) for y in range(h)]
    return np.stack(rows)[::-1].astype(np.float64) / 65535
