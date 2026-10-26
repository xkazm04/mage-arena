# The geometric ledge of a split-and-merge creature (merge_halves.py) at the cut, measured on orthographic side depth
# maps and smoothed away on the finished LOD0. numpy only, tested in test_ledge_lib.py; seam_ledge.py is the Blender
# side. First used on the mire maw (docs/research/MAW-SEAM-LEDGE-T70-2026-10-26.md).
#
# The finish's voxel remesh is the union of the two capped halves, so where the front half's skin stands proud of the
# back half's, the front's cap plane shows as a vertical ledge down the flank and a square notch where a leg is cut at the
# ground. The smoothing moves vertex positions only, in a band around the cut: the triangles and the UVs stay the same, so
# the mesh stays one watertight component by construction and the bake's UV layout still fits (seam_fix.py is re-run on
# the moved surface, because its seam report and its anchors read texel positions).
#
# Smoothing: Taubin's lambda/mu pairs (a shrink step then an inflate step) of the uniform umbrella operator, each vertex's
# step scaled by its band weight, a smoothstep of its distance from the cut that is 1 within the core and 0 past the edge;
# the top few cm are pinned so the finished height holds.
#
# Measures, on a flank's depth map (rows z, columns y, the outermost |x| the side ray hit, NaN where it hit nothing):
# - depth ledge: per adjacent column pair, the height of the rows whose depth jumps by 1 cm or more. A vertical ledge is
#   one tall line of jumps at one y, a wart a short one. Reported as the band's tallest line (its y and median jump)
#   against the tallest outside the band (the body's own relief: a leg's edge in front of the belly).
# - normal ledge: the same with the normals turning by 45 degrees or more.
# - silhouette step: the largest jump of the top and the bottom outline (the highest and lowest row hit per column)
#   between columns within a short reach.
# - ground notch: the largest rise of the bottom outline within a short reach, from a column that touches the ground.
import numpy as np


def band_weight(y, centre, core, edge):
    """1 within `core` of `centre`, 0 beyond `edge`, a smoothstep between."""
    if edge <= core:
        raise ValueError('edge must exceed core')
    d = np.abs(np.asarray(y, float) - centre)
    t = np.clip((edge - d) / (edge - core), 0.0, 1.0)
    return t * t * (3 - 2 * t)


def pin_top(z, w, margin):
    """w with every vertex within `margin` of the highest one pinned (0), so smoothing keeps the mesh's height (the import
    spec and the rig hold the finished height)."""
    z = np.asarray(z, float)
    return np.where(z >= z.max() - margin, 0.0, np.asarray(w, float))


def neighbour_mean(co, ea, eb):
    """The mean position of each vertex's edge neighbours (a vertex with none keeps its own)."""
    n = len(co)
    acc = np.zeros_like(co); cnt = np.zeros(n)
    np.add.at(acc, ea, co[eb]); np.add.at(acc, eb, co[ea])
    np.add.at(cnt, ea, 1); np.add.at(cnt, eb, 1)
    out = co.copy(); has = cnt > 0
    out[has] = acc[has] / cnt[has, None]
    return out


def taubin(co, ea, eb, w, iters, lam=0.5, mu=-0.53):
    """`iters` lambda/mu pairs of umbrella smoothing, each vertex's step scaled by w (0 pins it)."""
    co = np.array(co, float); w = np.asarray(w, float)[:, None]
    for _ in range(iters):
        for f in (lam, mu):
            co = co + f * w * (neighbour_mean(co, ea, eb) - co)
    return co


def face_normals(co, tris):
    n = np.cross(co[tris[:, 1]] - co[tris[:, 0]], co[tris[:, 2]] - co[tris[:, 0]])
    ln = np.linalg.norm(n, axis=1, keepdims=True)
    return n / np.where(ln > 0, ln, 1)


def flipped_faces(co0, co1, tris):
    """Faces whose normal turned more than 90 degrees (a fold)."""
    return int((np.einsum('ij,ij->i', face_normals(co0, tris), face_normals(co1, tris)) < 0).sum())


def ledge_lines(depth, normals, step, min_depth=0.01, min_deg=45.0):
    """Per adjacent column pair: (the height in m of the rows whose depth jumps by min_depth or more between the two
    columns, the median jump on those rows, the height of the rows whose normals turn by min_deg or more, their median
    angle). A ledge is one tall line of jumps at one y; a wart's jumps are short. Rows where either column missed are
    skipped; a pair with no qualifying row reads 0."""
    a, b = depth[:, :-1], depth[:, 1:]
    ok = np.isfinite(a) & np.isfinite(b)
    d = np.where(ok, np.abs(a - b), 0.0)
    hit = ok & (d >= min_depth)
    h = hit.sum(0) * step
    med = np.array([np.median(d[hit[:, c], c]) if hit[:, c].any() else 0.0 for c in range(d.shape[1])])
    if normals is None:
        return h, med, None, None
    dot = np.clip(np.einsum('rck,rck->rc', normals[:, :-1], normals[:, 1:]), -1, 1)
    g = np.where(ok, np.degrees(np.arccos(dot)), 0.0)
    nh = ok & (g >= min_deg)
    gmed = np.array([np.median(g[nh[:, c], c]) if nh[:, c].any() else 0.0 for c in range(g.shape[1])])
    return h, med, nh.sum(0) * step, gmed


def outlines(depth, z):
    """(top z, bottom z) per column: the highest and lowest row with a hit, NaN for an empty column."""
    hit = np.isfinite(depth)
    any_ = hit.any(0)
    zz = np.where(hit, np.asarray(z, float)[:, None], np.nan)
    top = np.full(depth.shape[1], np.nan); bot = np.full(depth.shape[1], np.nan)
    top[any_] = np.nanmax(zz[:, any_], axis=0); bot[any_] = np.nanmin(zz[:, any_], axis=0)
    return top, bot


def reach_jump(v, reach):
    """Per column, the largest |v[c + k] - v[c]| for 1 <= k <= reach (pairs with a NaN are skipped)."""
    out = np.zeros(len(v))
    for k in range(1, reach + 1):
        d = np.abs(v[k:] - v[:-k])
        d = np.where(np.isfinite(d), d, 0)
        out[:-k] = np.maximum(out[:-k], d)
    return out


def ground_notch(bot, reach, ground):
    """Per column, the largest rise of the bottom outline within `reach` columns either way, from this column if it
    touches the ground (bottom <= `ground`); 0 elsewhere."""
    out = np.zeros(len(bot))
    on = np.isfinite(bot) & (bot <= ground)
    for k in range(1, reach + 1):
        for s in (k, -k):
            nb = np.roll(bot, -s)
            valid = np.ones(len(bot), bool)
            if s > 0: valid[-s:] = False
            else: valid[:-s] = False
            rise = np.where(on & valid & np.isfinite(nb), nb - bot, 0)
            out = np.maximum(out, rise)
    return out


def flank_report(depth, normals, y, z, centre, half, reach=5, ground=0.02, min_depth=0.01, min_deg=45.0):
    """The ledge numbers of one flank. depth/normals: rows z, columns y, both at the spacing of y. The band is
    |y - centre| <= half; the reference is every other column of the body. Lengths in metres, angles in degrees.
    depth_ledge: the column pair with the tallest line of depth jumps in the band (its y, that height, the median jump on
    it), and the tallest such line outside; normal_ledge the same for the normal turn. silhouette_step and ground_notch
    are of the outline, so equal for both flanks of one orthographic side view."""
    y = np.asarray(y, float); z = np.asarray(z, float); step = float(y[1] - y[0])
    h, med, nh, gmed = ledge_lines(depth, normals, step, min_depth, min_deg)
    ym = (y[:-1] + y[1:]) / 2
    band = np.abs(ym - centre) <= half
    top, bot = outlines(depth, z)
    sil = np.maximum(reach_jump(top, reach), reach_jump(bot, reach))
    notch = ground_notch(bot, reach, ground)
    bandc = np.abs(y - centre) <= half
    body = np.isfinite(top)

    def line(hh, mm):
        i = int(np.argmax(np.where(band, hh, -1))); o = int(np.argmax(np.where(~band, hh, -1)))
        return {'band_height_m': round(float(hh[i]), 4), 'band_at_y': round(float(ym[i]), 4), 'band_median': round(float(mm[i]), 4),
                'outside_height_m': round(float(hh[o]), 4), 'outside_at_y': round(float(ym[o]), 4), 'outside_median': round(float(mm[o]), 4)}

    def mx(v, m):
        v = v[m & np.isfinite(v)]
        return round(float(v.max()), 4) if len(v) else float('nan')
    out = {'depth_ledge': line(h, med),
           'silhouette_step_m': {'band_max': mx(sil, bandc), 'outside_max': mx(sil, ~bandc & body)},
           'ground_notch_m': {'band_max': mx(notch, bandc), 'outside_max': mx(notch, ~bandc & body)}}
    if nh is not None:
        out['normal_ledge'] = line(nh, gmed)
    return out
