# Offline proof of the seam fix's Blender-free logic (seam_lib.py) and merge_halves.py's colour-match band
# (halves_lib.band_mask, the --match-back flag): the GLB reader's frame, the UV rasterizer, the statistics, the gain
# and offset, the seam (by plane and by source id), the local match, the feather, the margin spread and the flank
# patches. numpy and scipy, no Blender.
#
#   python -m unittest apps/vr/tools/mesh/test_seam_lib.py
import json
import os
import struct
import sys
import tempfile
import unittest

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import halves_lib as hl  # noqa: E402
import seam_lib as sl  # noqa: E402


def write_glb(path, pos_gltf, uv, idx):
    """A minimal one-primitive GLB (positions in glTF's Y-up frame)."""
    p = np.asarray(pos_gltf, np.float32).tobytes(); t = np.asarray(uv, np.float32).tobytes(); i = np.asarray(idx, np.uint32).tobytes()
    blob = p + t + i
    doc = {'asset': {'version': '2.0'}, 'buffers': [{'byteLength': len(blob)}],
           'bufferViews': [{'buffer': 0, 'byteOffset': 0, 'byteLength': len(p)}, {'buffer': 0, 'byteOffset': len(p), 'byteLength': len(t)},
                           {'buffer': 0, 'byteOffset': len(p) + len(t), 'byteLength': len(i)}],
           'accessors': [{'bufferView': 0, 'componentType': 5126, 'count': len(pos_gltf), 'type': 'VEC3'},
                         {'bufferView': 1, 'componentType': 5126, 'count': len(uv), 'type': 'VEC2'},
                         {'bufferView': 2, 'componentType': 5125, 'count': int(np.size(idx)), 'type': 'SCALAR'}],
           'meshes': [{'primitives': [{'attributes': {'POSITION': 0, 'TEXCOORD_0': 1}, 'indices': 2}]}]}
    js = json.dumps(doc).encode(); js += b' ' * (-len(js) % 4); blob += b'\0' * (-len(blob) % 4)
    with open(path, 'wb') as fh:
        fh.write(b'glTF' + struct.pack('<II', 2, 12 + 8 + len(js) + 8 + len(blob)))
        fh.write(struct.pack('<I', len(js)) + b'JSON' + js + struct.pack('<I', len(blob)) + b'BIN\0' + blob)


# a unit quad in the finish frame's x-y plane (z 0.5), y from -1 to 1, its UVs filling the square: u = x, v = (y + 1) / 2
QUAD_POS = np.array([[0, -1, 0.5], [1, -1, 0.5], [1, 1, 0.5], [0, 1, 0.5]], float)
QUAD_UV = np.array([[0, 0], [1, 0], [1, 1], [0, 1]], float)
QUAD_TRI = np.array([[0, 1, 2], [0, 2, 3]])


class ReadGlb(unittest.TestCase):
    def test_turns_y_up_into_the_finish_frame(self):
        with tempfile.TemporaryDirectory() as d:
            f = os.path.join(d, 'q.glb')
            gltf = np.stack([QUAD_POS[:, 0], QUAD_POS[:, 2], -QUAD_POS[:, 1]], 1)  # Blender (x, y, z) -> glTF (x, z, -y)
            write_glb(f, gltf, QUAD_UV, QUAD_TRI.ravel())
            pos, uv, tri = sl.read_glb(f)
        np.testing.assert_allclose(pos, QUAD_POS, atol=1e-6)
        np.testing.assert_allclose(uv, QUAD_UV, atol=1e-7)
        self.assertEqual(tri.tolist(), QUAD_TRI.tolist())

    def test_refuses_a_non_glb(self):
        with tempfile.TemporaryDirectory() as d:
            f = os.path.join(d, 'x.glb')
            with open(f, 'wb') as fh:
                fh.write(b'nope' * 8)
            with self.assertRaises(ValueError):
                sl.read_glb(f)


class Raster(unittest.TestCase):
    def test_each_texel_gets_its_surface_point(self):
        P, M = sl.raster_positions(QUAD_UV, QUAD_POS, QUAD_TRI, 16)
        self.assertTrue(M.all())
        # texel (row 3, column 5) has its centre at u = 5.5 / 16, v = 3.5 / 16
        np.testing.assert_allclose(P[3, 5], [5.5 / 16, 3.5 / 16 * 2 - 1, 0.5], atol=1e-9)

    def test_uncovered_texels_stay_outside_the_islands(self):
        half = np.array([[0, 1, 2]])  # the lower-right triangle only
        _, M = sl.raster_positions(QUAD_UV, QUAD_POS, half, 16)
        self.assertGreater(M.sum(), 100); self.assertLess(M.sum(), 16 * 16 - 100)
        self.assertFalse(M[15, 0])  # the far corner of the other triangle


def slab(cut=-0.3):
    """Two flanks (x = -0.5 and +0.5) of a box sampled every 4 mm, y -0.6 to 0.0, z 0 to 1: the front half (y < cut) a
    textured 0.6 grey-green, the back half a 0.1 darker, browner copy of the same texture."""
    ys, zs = np.meshgrid(np.arange(-0.6, 0.0, 0.004), np.arange(0, 1, 0.004))
    P = np.zeros(ys.shape + (3,)); P[..., 1] = ys; P[..., 2] = zs
    P = np.concatenate([P.copy(), P.copy()], 0); P[:ys.shape[0], :, 0] = -0.5; P[ys.shape[0]:, :, 0] = 0.5
    M = np.ones(P.shape[:2], bool)
    tex = 0.05 * np.sin(P[..., 1:2] * 90) * np.sin(P[..., 2:3] * 70)
    rgb = np.where(P[..., 1:2] < cut, np.array([0.60, 0.55, 0.40]), np.array([0.55, 0.45, 0.28])) + tex
    return P, M, rgb


class Stats(unittest.TestCase):
    def test_mask_stats(self):
        rgb = np.zeros((10, 10, 3)); rgb[:5] = 0.6; rgb[5:] = 0.2; m = np.zeros((10, 10), bool); m[:5] = True
        mean, std, n = sl.mask_stats(rgb, m)
        np.testing.assert_allclose(mean, 0.6); np.testing.assert_allclose(std, 0, atol=1e-12); self.assertEqual(n, 50)
        with self.assertRaises(ValueError):
            sl.mask_stats(rgb, np.zeros((10, 10), bool))

    def test_weighted_stats(self):
        m, s = sl.weighted_stats([[0, 0, 0], [1, 1, 1]], [3, 1])
        np.testing.assert_allclose(m, 0.25); np.testing.assert_allclose(s, np.sqrt(0.1875))
        with self.assertRaises(ValueError):
            sl.weighted_stats(np.zeros((0, 3)), [])

    def test_gain_offset_moves_the_statistics(self):
        rng = np.random.default_rng(5)
        src = rng.normal([0.29, 0.19, 0.11], [0.04, 0.03, 0.03], (5000, 3))
        ref_m, ref_s = np.array([0.31, 0.22, 0.14]), np.array([0.05, 0.04, 0.04])
        g, o = sl.gain_offset(src.mean(0), src.std(0), ref_m, ref_s)
        out = src * g + o
        np.testing.assert_allclose(out.mean(0), ref_m, atol=1e-9); np.testing.assert_allclose(out.std(0), ref_s, atol=1e-9)

    def test_gain_is_clamped(self):
        g, o = sl.gain_offset([0.5] * 3, [0.01] * 3, [0.5] * 3, [0.08] * 3, max_gain=1.5)
        np.testing.assert_allclose(g, 1.5); np.testing.assert_allclose(o, 0.5 - 1.5 * 0.5)  # the mean still lands

    def test_margin_takes_the_nearest_island_value(self):
        mask = np.zeros((5, 5), bool); mask[2, 2] = True
        v = np.zeros((5, 5)); v[2, 2] = 7
        self.assertTrue((sl.spread_to_margin(v, mask) == 7).all())


class Seam(unittest.TestCase):
    def test_back_from_cut(self):
        P, M, _ = slab()
        b = sl.back_from_cut(P, M, -0.3)
        self.assertTrue((P[b][:, 1] >= -0.3).all()); self.assertTrue((P[M & ~b][:, 1] < -0.3).all())

    def test_back_from_id_gives_a_miss_its_nearest_side_on_the_surface(self):
        P, M, _ = slab()
        q = np.where(P[..., 1] >= -0.28, 255, 128).astype(np.uint8)  # the bake switched 2 cm behind the plane
        q[:, :20] = 0  # a strip of misses on both flanks, at the front end
        q[P[..., 1] > -0.1] = 0  # and the back end
        b = sl.back_from_id(q, M, P)
        self.assertTrue(b[P[..., 1] > -0.1].all()); self.assertFalse(b[:, :20].any())
        self.assertFalse(b[(P[..., 1] < -0.285) & (P[..., 1] > -0.5)].any())
        with self.assertRaises(ValueError):
            sl.back_from_id(np.zeros_like(q), M, P)

    def test_seam_distance(self):
        P, M, _ = slab()
        d = sl.seam_distance(P, M, sl.back_from_cut(P, M, -0.3))
        y = P[..., 1]
        np.testing.assert_allclose(d[M & (np.abs(y + 0.3) > 0.01)], np.abs(y + 0.3)[M & (np.abs(y + 0.3) > 0.01)], atol=0.005)
        self.assertLess(d[M & (np.abs(y + 0.3) < 0.003)].max(), 0.006)

    def test_match_lands_the_back_on_the_front_and_keeps_the_front(self):
        P, M, rgb = slab()
        back = sl.back_from_cut(P, M, -0.3); dist = sl.seam_distance(P, M, back)
        lf, lb, rec = sl.anchor_match(rgb, P, M, back, dist, 0.05, radius=0.08, step=16)
        self.assertGreater(rec['anchors'], 10); np.testing.assert_allclose(rec['back_minus_front_mean'], [-0.05, -0.10, -0.12], atol=0.01)
        out = sl.match_back(rgb, back, lf, lb, np.ones(3))
        np.testing.assert_array_equal(out[~back], rgb[~back])
        near = dist < 0.05
        np.testing.assert_allclose(out[back & near].mean(0), rgb[~back & near].mean(0), atol=0.005)

    def test_far_texels_take_the_seam_wide_shift(self):
        local = np.array([[0.1, 0.2, 0.3], [0.1, 0.2, 0.3]]); g = np.array([0.5, 0.5, 0.5])
        out = sl.fade_to_global(local, g, np.array([0.0, 10.0]), 0.15)
        np.testing.assert_allclose(out[0], local[0]); np.testing.assert_allclose(out[1], g, atol=1e-9)
        np.testing.assert_array_equal(sl.fade_to_global(local, g, np.array([5.0, 5.0]), 0), local)

    def test_blur_band_softens_the_seam_only(self):
        P, M, rgb = slab()
        back = sl.back_from_cut(P, M, -0.3); dist = sl.seam_distance(P, M, back)
        out = sl.blur_band(rgb, P, M, dist, 0.03, 0.015)
        far = dist >= 0.03
        np.testing.assert_array_equal(out[far], rgb[far])
        y = P[..., 1]; z = P[..., 2]
        row = M & (P[..., 0] > 0) & (np.abs(z - 0.5) < 0.003)
        step_before = np.abs(np.diff(rgb[row][np.argsort(y[row])][:, 0])).max()
        step_after = np.abs(np.diff(out[row][np.argsort(y[row])][:, 0])).max()
        self.assertLess(step_after, step_before / 2)

    def test_patches_sit_either_side_of_the_seam_on_the_outer_skin(self):
        P, M, rgb = slab()
        back = sl.back_from_cut(P, M, -0.3); dist = sl.seam_distance(P, M, back)
        flat = np.where(back[..., None], 0.2, 0.6) * np.ones(3)
        pt = sl.flank_patches(P, M, back, dist, 0.5)
        r = sl.patch_means(flat, pt)
        for side in ('left', 'right'):
            self.assertEqual(r[side]['front'], [0.6] * 3); self.assertEqual(r[side]['back'], [0.2] * 3)
            self.assertEqual(r[side]['delta'], [-0.4] * 3)
            self.assertGreater(min(r[side]['n']), 5)
            self.assertTrue((P[pt[side]['front']][:, 1] >= -0.3 - 0.021).all())
        self.assertTrue((P[pt['left']['front']][:, 0] < 0).all()); self.assertTrue((P[pt['right']['back']][:, 0] > 0).all())


class MatchBand(unittest.TestCase):
    def test_band_leaves_out_the_cap_planes(self):
        y = np.array([-0.35, -0.349, -0.3, -0.251, -0.25])
        self.assertEqual(hl.band_mask(y, -0.3, 0.05).tolist(), [False, False, True, False, False])
        self.assertEqual(hl.band_mask(y, -0.3, 0.05, eps=0).tolist(), [False, True, True, True, False])

    def test_match_back_flag(self):
        pos = ['f.glb', 'b.glb', 'out/m.glb', '-50', '315', '-0.3', '0.05', '1.25']
        self.assertFalse(hl.parse_args(pos)['match_back'])
        a = hl.parse_args(pos + ['--match-back'])
        self.assertTrue(a['match_back']); self.assertEqual(a['record'], 'out/m_merge.json')
        self.assertEqual(hl.parse_args(pos + ['r.json', '--match-back'])['record'], 'r.json')
        with self.assertRaises(SystemExit):
            hl.parse_args(pos + ['--mirror'])


if __name__ == '__main__':
    unittest.main()
