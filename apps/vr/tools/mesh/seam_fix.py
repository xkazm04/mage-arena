# Colour-match and feather the flank seam of a split-and-merge finish (merge_halves.py, then finish.mjs --mode bake) on
# its baked texture, in UV space. The geometry is not touched: the GLB is only read. numpy + scipy + Pillow, no Blender.
# The logic is seam_lib.py (tested in test_seam_lib.py). First used on the mire maw (docs/research/CREATURE-CAST-UE-2026-10-24.md).
#
#   python apps/vr/tools/mesh/seam_fix.py <finish.glb> <in.png> <out.png> [--source <id.png> | --cut -0.30] [--band 0.05]
#        [--radius-match 0.08] [--falloff 0.15] [--max-gain 1.0] [--feather 0.03] [--radius 0.015] [--record <out.json>]
#   python apps/vr/tools/mesh/seam_fix.py <finish.glb> <in.png> --measure [--source <id.png> | --cut -0.30] [--record <out.json>]
#
# The seam is the source-id bake's front/back switch (--source, from bake_source_id.py), else the plane y = --cut. Every
# back texel moves from the back's local mean colour onto the front's, both measured within --band of the seam at
# anchors along it, within --radius-match (seam_lib.anchor_match); away from the seam the local shift fades into the
# seam-wide one (--falloff m, seam_lib.fade_to_global), so a hind leg does not take a far anchor's shift; its spread
# can be scaled by one gain for the whole seam (at most --max-gain; 1, the default, keeps it). Then texels within --feather of the seam are blended toward their 3D neighbourhood mean
# (--radius), and the bake margin takes its nearest island texel's colour. --measure writes nothing but the record: the
# seam report of <in.png> (seam_report below). Otherwise the record holds the match, the report before and after.
import argparse
import json
import os
import sys
import time

import numpy as np
from PIL import Image

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import seam_lib as sl  # noqa: E402

PATCH_Z = (0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9)
PATCH_GAPS = (0.0, 0.04)  # adjacent to the seam (inside the feather), and 4-6 cm out (outside it: the match alone)


def seam_report(rgb, P, M, back, dist):
    """Per flank: the 2 cm patches either side of the seam at each height of PATCH_Z, at each gap of PATCH_GAPS (their
    mean back - front per channel, the systematic part, and its mean |back - front|, with each height's numbers); the noise floor (the mean |difference| of two 2 cm
    patches on the front half, 0-2 and 4-6 cm from the seam: one texture, so what a patch pair differs by anyway); and
    5 cm bands either side of the seam on the flank (|x| > 0.4 m, z 0.2-0.9 m)."""
    rep = {}
    for side, sgn in (('left', -1), ('right', 1)):
        r = {'patches': {}, 'mean_delta': {}, 'mean_abs_delta': {}}
        floor = []
        for gap in PATCH_GAPS:
            rows = []
            for z in PATCH_Z:
                q = sl.patch_means(rgb, sl.flank_patches(P, M, back, dist, z, gap=gap))[side]
                if min(q['n']) >= 5:
                    rows.append({'z': z, **q})
            r['patches'][f'{gap:.2f}'] = rows
            r['mean_delta'][f'{gap:.2f}'] = np.round(np.mean([x['delta'] for x in rows], 0), 4).tolist() if rows else None
            r['mean_abs_delta'][f'{gap:.2f}'] = np.round(np.mean([np.abs(x['delta']) for x in rows], 0), 4).tolist() if rows else None
        for z in PATCH_Z:
            a = sl.flank_patches(P, M, back, dist, z, gap=0.0)[side]['front']; b = sl.flank_patches(P, M, back, dist, z, gap=0.04)[side]['front']
            if a.sum() >= 5 and b.sum() >= 5:
                floor.append(np.abs(rgb[a].mean(0) - rgb[b].mean(0)))
        r['noise_floor'] = np.round(np.mean(floor, 0), 4).tolist() if floor else None
        fl = M & (sgn * P[..., 0] > 0.4) & (P[..., 2] > 0.2) & (P[..., 2] < 0.9) & (dist < 0.05)
        fm, _, nf = sl.mask_stats(rgb, fl & ~back); bm, _, nb = sl.mask_stats(rgb, fl & back)
        r['bands'] = {'front': fm.round(4).tolist(), 'back': bm.round(4).tolist(), 'delta': (bm - fm).round(4).tolist(), 'n': [nf, nb]}
        rep[side] = r
    return rep


def summary(rep):
    return {s: {'patch_mean_delta': rep[s]['mean_delta'], 'patch_mean_abs_delta': rep[s]['mean_abs_delta'], 'noise_floor': rep[s]['noise_floor'], 'band_delta': rep[s]['bands']['delta']}
            for s in rep}


def main(argv):
    ap = argparse.ArgumentParser()
    ap.add_argument('glb'); ap.add_argument('png'); ap.add_argument('out', nargs='?')
    ap.add_argument('--source'); ap.add_argument('--cut', type=float, default=-0.30)
    ap.add_argument('--band', type=float, default=0.05); ap.add_argument('--radius-match', type=float, default=0.08)
    ap.add_argument('--max-gain', type=float, default=1.0); ap.add_argument('--falloff', type=float, default=0.15)
    ap.add_argument('--feather', type=float, default=0.03); ap.add_argument('--radius', type=float, default=0.015)
    ap.add_argument('--record'); ap.add_argument('--measure', action='store_true')
    a = ap.parse_args(argv)
    if not a.measure and not a.out:
        ap.error('give <out.png>, or --measure')
    if min(a.band, a.feather, a.radius, a.radius_match) <= 0 or a.max_gain < 1:
        ap.error('band, feather and both radii are positive, max-gain >= 1')
    t0 = time.time()
    pos, uv, tri = sl.read_glb(a.glb)
    rgb = np.asarray(Image.open(a.png).convert('RGB'), dtype=np.float64) / 255
    if rgb.shape[0] != rgb.shape[1]:
        raise SystemExit(f'{a.png}: expected a square texture, got {rgb.shape[1]}x{rgb.shape[0]}')
    P, M = sl.raster_positions(uv, pos, tri, rgb.shape[0])
    if a.source:
        q = np.asarray(Image.open(a.source).convert('L'))
        if q.shape != M.shape:
            raise SystemExit(f'{a.source}: {q.shape} does not match the texture {M.shape}')
        back = sl.back_from_id(q, M, P)
    else:
        back = sl.back_from_cut(P, M, a.cut)
    dist = sl.seam_distance(P, M, back)
    seam_y = P[..., 1][M & (dist < 0.005)]
    R = {'glb': a.glb, 'png': a.png, 'seam': a.source or f'plane y = {a.cut}', 'island_share': round(float(M.mean()), 4),
         'back_share_of_islands': round(float(back.sum() / M.sum()), 4),
         'seam_y_m': {'p5': round(float(np.percentile(seam_y, 5)), 4), 'median': round(float(np.median(seam_y)), 4),
                      'p95': round(float(np.percentile(seam_y, 95)), 4)},
         'before': seam_report(rgb, P, M, back, dist)}
    if not a.measure:
        near = dist < a.band
        fm, fs, _ = sl.mask_stats(rgb, M & ~back & near); bm, bs, _ = sl.mask_stats(rgb, back & near)
        gain, _ = sl.gain_offset(bm, bs, fm, fs, a.max_gain)  # the spread: one gain for the whole seam
        lf, lb, local = sl.anchor_match(rgb, P, M, back, dist, a.band, a.radius_match)
        lf = sl.fade_to_global(lf, fm, dist[back], a.falloff); lb = sl.fade_to_global(lb, bm, dist[back], a.falloff)
        out = sl.match_back(rgb, back, lf, lb, gain)
        out = sl.blur_band(out, P, M, dist, a.feather, a.radius)
        out[~M] = sl.spread_to_margin(out, M)[~M]
        q8 = np.round(out * 255).astype(np.uint8)
        os.makedirs(os.path.dirname(os.path.abspath(a.out)), exist_ok=True)
        Image.fromarray(q8, 'RGB').save(a.out)
        R.update({'out': a.out, 'band': a.band, 'radius_match': a.radius_match, 'max_gain': a.max_gain, 'feather': a.feather,
                  'radius': a.radius, 'falloff': a.falloff, 'seam_front_mean': fm.round(4).tolist(), 'seam_front_std': fs.round(4).tolist(),
                  'seam_back_mean': bm.round(4).tolist(), 'seam_back_std': bs.round(4).tolist(), 'gain': gain.round(4).tolist(),
                  'local': local, 'after': seam_report(q8 / 255.0, P, M, back, dist)})
    R['secs'] = round(time.time() - t0, 1)
    if a.record:
        with open(a.record, 'w') as fh:
            json.dump(R, fh, indent=1)
    print('SEAM_FIX', json.dumps({k: summary(R[k]) for k in ('before', 'after') if k in R}))


if __name__ == '__main__':
    main(sys.argv[1:])
