# The flank seam of a split-and-merge creature (merge_halves.py), measured and fixed in UV space on the finished bake.
# numpy and scipy, tested in test_seam_lib.py. First used on the mire maw
# (docs/research/CREATURE-CAST-UE-2026-10-24.md).
#
# The finish bakes the front half's TRELLIS texture onto the front of the LOD0 and the back half's onto the back, so the
# two meet in a hard line at the cut. Every texel of the baked texture is given its point on the finished surface (the
# UV triangles rasterized with their positions), in finish.py's frame: facing -Y, feet at z 0. The bake margin (texels
# outside every island) takes its nearest island texel's colour after a fix, so mip filtering at an island edge does not
# pull the old colour back in. Colours are sRGB-encoded floats 0-1, as the PNG stores them.
#
# The seam is where the bake switched from one half's texture to the other's. The cut plane only approximates it (the
# remesh surface lies between the two halves' skins in the overlap band, so the switch wanders a few cm), so it is read
# from a source-id bake (bake_source_id.py; back_from_id) when there is one, else taken as the plane (back_from_cut).
# seam_distance gives every texel its 3D distance to the other half. The fix (seam_fix.py): the colour on each side is
# measured locally at anchors along the seam (anchor_match: the left flank, the top and each leg differ), every back
# texel is moved from the back's local mean onto the front's, taken from its nearest anchors (match_back: the whole back
# half moves), and the seam itself is feathered: each texel within a few cm of it is blended toward the 3D
# neighbourhood mean of its colour (blur_band), so the line where the two TRELLIS textures meet does not stay a hard edge.
import json
import struct

import numpy as np

_CTYPE = {5120: np.int8, 5121: np.uint8, 5122: np.int16, 5123: np.uint16, 5125: np.uint32, 5126: np.float32}
_NCOMP = {'SCALAR': 1, 'VEC2': 2, 'VEC3': 3, 'VEC4': 4}


def read_glb(path):
    """(positions in the finish frame, UVs with v down the image, triangle indices) of a GLB's first mesh, all its
    primitives. glTF is Y-up (Blender (x, y, z) is glTF (x, z, -y)) and its v runs down the image, as the PNG's rows."""
    with open(path, 'rb') as fh:
        data = fh.read()
    if data[:4] != b'glTF':
        raise ValueError(f'{path}: not a GLB')
    n = struct.unpack_from('<I', data, 12)[0]
    doc = json.loads(data[20:20 + n])
    off = 20 + n
    blob = data[off + 8:off + 8 + struct.unpack_from('<I', data, off)[0]]

    def acc(i):
        a = doc['accessors'][i]; bv = doc['bufferViews'][a['bufferView']]
        dt = np.dtype(_CTYPE[a['componentType']]); k = _NCOMP[a['type']]
        start = bv.get('byteOffset', 0) + a.get('byteOffset', 0)
        stride = bv.get('byteStride', 0) or dt.itemsize * k
        raw = np.frombuffer(blob, dtype=np.uint8, count=stride * (a['count'] - 1) + dt.itemsize * k, offset=start)
        out = np.lib.stride_tricks.as_strided(raw, shape=(a['count'], dt.itemsize * k), strides=(stride, 1))
        return np.ascontiguousarray(out).view(dt).reshape(a['count'], k)

    pos, uv, tri, base = [], [], [], 0
    for p in doc['meshes'][0]['primitives']:
        g = acc(p['attributes']['POSITION']).astype(np.float64)
        pos.append(np.stack([g[:, 0], -g[:, 2], g[:, 1]], 1))
        uv.append(acc(p['attributes']['TEXCOORD_0']).astype(np.float64))
        tri.append(acc(p['indices']).reshape(-1, 3).astype(np.int64) + base)
        base += len(g)
    return np.vstack(pos), np.vstack(uv), np.vstack(tri)


def raster_positions(uv, pos, tris, res):
    """(res x res x 3 surface point of each texel centre, res x res island mask). A texel covered by no triangle is
    outside every island (the bake margin) and stays unmasked."""
    P = np.zeros((res, res, 3)); M = np.zeros((res, res), dtype=bool)
    px = uv * res - 0.5  # texel centres sit at integer + 0.5 of the UV square
    for t in tris:
        a, b, c = px[t]
        x0, x1 = int(np.floor(min(a[0], b[0], c[0]))), int(np.ceil(max(a[0], b[0], c[0])))
        y0, y1 = int(np.floor(min(a[1], b[1], c[1]))), int(np.ceil(max(a[1], b[1], c[1])))
        x0, y0 = max(x0, 0), max(y0, 0); x1, y1 = min(x1, res - 1), min(y1, res - 1)
        if x1 < x0 or y1 < y0:
            continue
        d = (b[1] - c[1]) * (a[0] - c[0]) + (c[0] - b[0]) * (a[1] - c[1])
        if abs(d) < 1e-14:
            continue
        gx, gy = np.meshgrid(np.arange(x0, x1 + 1), np.arange(y0, y1 + 1))
        l0 = ((b[1] - c[1]) * (gx - c[0]) + (c[0] - b[0]) * (gy - c[1])) / d
        l1 = ((c[1] - a[1]) * (gx - c[0]) + (a[0] - c[0]) * (gy - c[1])) / d
        l2 = 1 - l0 - l1
        inside = (l0 >= -1e-9) & (l1 >= -1e-9) & (l2 >= -1e-9)
        if not inside.any():
            continue
        p = l0[..., None] * pos[t[0]] + l1[..., None] * pos[t[1]] + l2[..., None] * pos[t[2]]
        yy, xx = gy[inside], gx[inside]
        P[yy, xx] = p[inside]; M[yy, xx] = True
    return P, M



def mask_stats(rgb, mask):
    """(mean, std, count) per channel of the masked texels."""
    c = rgb[mask]
    if len(c) == 0:
        raise ValueError('no texels in the mask')
    return c.mean(0), c.std(0), int(len(c))


def weighted_stats(c, w):
    """(mean, std) per channel of the colours c (n x k), each weighted by w (n): face areas for samples taken per face."""
    c = np.asarray(c, float); w = np.asarray(w, float)
    if len(c) == 0 or w.sum() <= 0:
        raise ValueError('no weighted samples')
    m = (c * w[:, None]).sum(0) / w.sum()
    return m, np.sqrt((((c - m) ** 2) * w[:, None]).sum(0) / w.sum())


def gain_offset(src_mean, src_std, ref_mean, ref_std, max_gain=2.0):
    """Per-channel gain and offset that move the src statistics onto ref: g * src + o has ref's mean and (gain clamped
    to [1/max_gain, max_gain]) ref's spread."""
    g = np.clip(np.asarray(ref_std, float) / np.maximum(np.asarray(src_std, float), 1e-6), 1 / max_gain, max_gain)
    return g, np.asarray(ref_mean, float) - g * np.asarray(src_mean, float)


def spread_to_margin(values, mask):
    """values (res x res [x k]) with every unmasked texel given its nearest masked texel's value."""
    from scipy.ndimage import distance_transform_edt
    _, (iy, ix) = distance_transform_edt(~mask, return_indices=True)
    return values[iy, ix]


def back_from_cut(P, M, cut):
    """The back half by the cut plane alone: island texels with y >= cut."""
    return M & (P[..., 1] >= cut)


def back_from_id(q, M, P):
    """The back half by a source-id map (bake_source_id.py: 128 front, 255 back, 0 missed). A missed island texel (the
    finish filled it from its neighbours) takes the side of the nearest hit island texel on the surface (3D)."""
    from scipy.spatial import cKDTree
    q = np.asarray(q)
    hit = M & (q >= 64)
    if not hit.any():
        raise ValueError('the source-id map has no hit island texel')
    back = M & (q >= 192)
    miss = M & ~hit
    if miss.any():
        back[miss] = back[hit][cKDTree(P[hit]).query(P[miss])[1]]
    return back


def seam_distance(P, M, back):
    """3D distance of each island texel to the nearest island texel of the other half (inf outside the islands): 0 on
    the seam, growing away from it on both sides."""
    from scipy.spatial import cKDTree
    front = M & ~back
    d = np.full(M.shape, np.inf)
    if back.any() and front.any():
        d[back] = cKDTree(P[front]).query(P[back])[0]
        d[front] = cKDTree(P[back]).query(P[front])[0]
    return d


def anchor_match(rgb, P, M, back, dist, band, radius=0.08, step=8, k=12, min_texels=30):
    """The local colour on each side of the seam, at anchors along it, carried to every texel of the back half.

    Anchors are every step-th back texel within band of the seam. At each, the front mean is the mean colour of the
    front texels within band of the seam and within radius (3D) of the anchor, the back mean that of the back ones; an
    anchor with fewer than min_texels on a side is dropped. The radius spans the small step the two halves' surfaces
    make at the cut, so a flank is compared with the same flank, a leg with the same leg. Every back texel takes the
    inverse-distance mean of its k nearest anchors' two means. Returns (front means, back means) for the texels of back
    in mask order, and a record."""
    from scipy.spatial import cKDTree
    fsel = M & ~back & (dist < band); bsel = back & (dist < band)
    if not fsel.any() or not bsel.any():
        raise ValueError(f'no texels within {band} m of the seam on one side')
    anchors = P[bsel][::step]
    fc, bc = rgb[fsel], rgb[bsel]
    fi, bi = cKDTree(P[fsel]).query_ball_point(anchors, radius), cKDTree(P[bsel]).query_ball_point(anchors, radius)
    ok = np.array([len(f) >= min_texels and len(b) >= min_texels for f, b in zip(fi, bi)])
    if not ok.any():
        raise ValueError('no anchor has texels on both sides of the seam')
    am = anchors[ok]
    af = np.array([fc[f].mean(0) for f, o in zip(fi, ok) if o]); ab = np.array([bc[b].mean(0) for b, o in zip(bi, ok) if o])
    d, j = cKDTree(am).query(P[back], k=min(k, len(am)))
    if d.ndim == 1:
        d, j = d[:, None], j[:, None]
    w = 1 / np.maximum(d, 1e-4); w /= w.sum(1, keepdims=True)
    diff = ab - af
    rec = {'anchors': int(ok.sum()), 'anchors_dropped': int((~ok).sum()), 'radius_m': radius,
           'back_minus_front_mean': diff.mean(0).round(4).tolist(),
           'back_minus_front_p95_abs': np.percentile(np.abs(diff), 95, 0).round(4).tolist()}
    return (af[j] * w[..., None]).sum(1), (ab[j] * w[..., None]).sum(1), rec


def match_back(rgb, back, front_mean, back_mean, gain):
    """Every back texel moved from its back mean onto its front mean (anchor_match), with its spread around that mean
    scaled by gain (per channel): g * (c - back) + front, clipped to 0-1. Front texels are kept."""
    out = rgb.copy()
    out[back] = np.clip(np.asarray(gain, float) * (rgb[back] - back_mean) + front_mean, 0, 1)
    return out


def blur_band(rgb, P, M, dist, feather, radius):
    """rgb with every island texel within feather of the seam blended toward the mean colour of the island texels within
    radius of it (3D), by a tent weight: all of the way on the seam, none at feather. Island texels only; the margin is
    left for spread_to_margin."""
    from scipy.spatial import cKDTree
    near = M & (dist < feather + radius); tgt = M & (dist < feather)
    out = rgb.copy()
    if not tgt.any():
        return out
    src_col = rgb[near]
    nb = cKDTree(P[near]).query_ball_point(P[tgt], radius)
    mean = np.array([src_col[ix].mean(0) for ix in nb])
    t = (1 - dist[tgt] / feather)[:, None]
    out[tgt] = rgb[tgt] * (1 - t) + mean * t
    return out


def flank_patches(P, M, back, dist, z, size=0.02, gap=0.0, depth=0.02):
    """Masks of the 2 cm patches on each side of the seam on both flanks at height z: the texels within size/2 of z, on
    that half, between gap and gap + size from the seam, and within depth of the outermost |x| among them (the outer
    skin, not a leg's inner face; each half has its own, because the two halves' surfaces can step at the cut). Keys
    'left' (-X) and 'right' (+X), each {'front': mask, 'back': mask}."""
    x, zz = P[..., 0], P[..., 2]
    zwin = M & (np.abs(zz - z) <= size / 2) & (dist >= gap) & (dist < gap + size)
    out = {}
    for side, sgn in (('left', -1), ('right', 1)):
        out[side] = {}
        for half, hm in (('front', ~back), ('back', back)):
            win = zwin & hm & (sgn * x > 0)
            edge = (sgn * x[win]).max() if win.any() else 0.0
            out[side][half] = win & (sgn * x >= edge - depth)
    return out


def patch_means(rgb, patches):
    """{'left': {'front': [r, g, b], 'back': [...], 'delta': [...], 'n': [nf, nb]}, 'right': ...}; delta is back - front."""
    res = {}
    for side, halves in patches.items():
        f, b = halves['front'], halves['back']
        mf = rgb[f].mean(0) if f.any() else np.full(3, np.nan)
        mb = rgb[b].mean(0) if b.any() else np.full(3, np.nan)
        res[side] = {'front': [round(float(v), 4) for v in mf], 'back': [round(float(v), 4) for v in mb],
                     'delta': [round(float(v), 4) for v in mb - mf], 'n': [int(f.sum()), int(b.sum())]}
    return res
