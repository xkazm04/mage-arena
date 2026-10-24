# merge_halves.py's Blender-free logic: pose two TRELLIS meshes of one creature (one reconstructed from the front image,
# one from the back image) in one frame, and say which faces each keeps. numpy only, tested in test_merge_halves.py.
#
# The frame is finish.py's: the creature faces -Y, feet at z 0, the bbox centre on the z axis, scaled to the class height.
# The back mesh comes out of TRELLIS facing -Y with the back image's side forward, so it is turned (its rot_z), never
# reflected: a reflection would swap its left and right flanks and invert its winding. It is then fitted to the front
# mesh: the same height, and its x and y extents scaled to the front's footprint. The footprint uses percentile extents,
# so a torn flap or a loose lump at one edge does not set the scale.
import numpy as np


def rot_z(deg):
    """3x3 rotation about +Z by deg degrees (counter-clockwise seen from above, as finish.py's ROT)."""
    t = np.radians(deg); c, s = np.cos(t), np.sin(t)
    return np.array([[c, -s, 0.0], [s, c, 0.0], [0.0, 0.0, 1.0]])


def robust_box(co, pct=1.0):
    """(lo, hi) per axis: the pct and 100-pct percentiles in x and y, the true min and max in z (finish.py scales by the
    full z extent, so the height must be the full one)."""
    co = np.asarray(co, dtype=np.float64)
    lo = np.percentile(co[:, :2], pct, axis=0); hi = np.percentile(co[:, :2], 100 - pct, axis=0)
    return np.array([lo[0], lo[1], co[:, 2].min()]), np.array([hi[0], hi[1], co[:, 2].max()])


def affine(lin, t):
    m = np.eye(4); m[:3, :3] = lin; m[:3, 3] = t
    return m


def apply(m, co):
    co = np.asarray(co, dtype=np.float64)
    return co @ m[:3, :3].T + m[:3, 3]


def fit_halves(front_co, back_co, rot_front, rot_back, height, pct=1.0):
    """4x4 matrices (front, back) that pose both meshes in the finish frame, and a record of what was measured.

    front: turned by rot_front, scaled uniformly to height, the percentile footprint centred on the z axis, feet at z 0.
    back: turned by rot_back, then x, y and z scaled apart so its percentile footprint and its full height equal the
    posed front's, centred the same way."""
    if height <= 0:
        raise ValueError('height must be positive')
    rf = rot_z(rot_front); f = apply(affine(rf, (0, 0, 0)), front_co); flo, fhi = robust_box(f, pct)
    s = height / (fhi[2] - flo[2])
    c = np.array([(flo[0] + fhi[0]) / 2, (flo[1] + fhi[1]) / 2, flo[2]])
    mf = affine(s * rf, -s * c)
    fext = np.array([(fhi[0] - flo[0]) * s, (fhi[1] - flo[1]) * s, height])
    rb = rot_z(rot_back); b = apply(affine(rb, (0, 0, 0)), back_co); blo, bhi = robust_box(b, pct)
    bext = bhi - blo
    if (bext <= 0).any():
        raise ValueError('the back mesh is flat on an axis')
    k = fext / bext
    cb = np.array([(blo[0] + bhi[0]) / 2, (blo[1] + bhi[1]) / 2, blo[2]])
    mb = affine(np.diag(k) @ rb, -k * cb)
    rec = {'pct': pct, 'height_m': height, 'rot_front_deg': rot_front, 'rot_back_deg': rot_back,
           'front_scale': round(float(s), 6), 'back_scale_xyz': [round(float(x), 6) for x in k],
           'footprint_m': [round(float(x), 4) for x in fext[:2]],
           'back_anisotropy': round(float(k.max() / k.min()), 4)}
    return mf, mb, rec


def keep_mask(face_y, cut, overlap, side):
    """Faces a half keeps, by their centre's y. The front half keeps y < cut + overlap, the back half y > cut - overlap,
    so the two overlap by 2 x overlap and the voxel remesh unions them across the seam."""
    if overlap < 0:
        raise ValueError('overlap must be >= 0')
    y = np.asarray(face_y, dtype=np.float64)
    if side == 'front':
        return y < cut + overlap
    if side == 'back':
        return y > cut - overlap
    raise ValueError(f'side must be front or back, not {side!r}')


def parse_args(argv):
    """merge_halves.py positionals: front.glb back.glb out.glb rot_front rot_back cut overlap height [record.json]."""
    if len(argv) not in (8, 9):
        raise SystemExit('usage: merge_halves.py -- <front.glb> <back.glb> <out.glb> <rot_front_deg> <rot_back_deg> '
                         '<cut_y_m> <overlap_m> <height_m> [<record.json>]')
    front, back, out = argv[:3]
    rot_front, rot_back, cut, overlap, height = (float(x) for x in argv[3:8])
    if overlap < 0 or height <= 0:
        raise SystemExit('overlap must be >= 0 and height > 0')
    if abs(cut) >= height:
        raise SystemExit('cut must lie inside the body (|cut| < height)')
    record = argv[8] if len(argv) == 9 else out[:-4] + '_merge.json'
    return {'front': front, 'back': back, 'out': out, 'rot_front': rot_front, 'rot_back': rot_back, 'cut': cut,
            'overlap': overlap, 'height': height, 'record': record}
