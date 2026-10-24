# Offline proof of merge_halves.py's Blender-free logic (halves_lib.py): the rotation, the percentile footprint, the fit
# of the back mesh to the front's height and footprint, the seam masks and the argument check. numpy only, no Blender.
#
#   python -m unittest apps/vr/tools/mesh/test_merge_halves.py
import os
import sys
import unittest

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import halves_lib as hl  # noqa: E402


def box(nx, ny, nz, ext, centre=(0, 0, 0), seed=0):
    """A point cloud filling an axis-aligned box of extents ext around centre."""
    rng = np.random.default_rng(seed)
    return rng.uniform(-0.5, 0.5, (nx * ny * nz, 3)) * np.array(ext) + np.array(centre)


class Rotation(unittest.TestCase):
    def test_quarter_turn_is_counter_clockwise(self):
        np.testing.assert_allclose(hl.rot_z(90) @ [1, 0, 0], [0, 1, 0], atol=1e-12)

    def test_turn_is_not_a_reflection(self):
        for d in (-50, 135, 180, 315):
            self.assertAlmostEqual(np.linalg.det(hl.rot_z(d)), 1.0)


class RobustBox(unittest.TestCase):
    def test_a_flap_does_not_set_the_footprint(self):
        co = box(20, 20, 20, (2, 1, 1), seed=1)
        flap = np.array([[5.0, 0, 0]] * 10)  # 10 of 8,010 points far out on +X
        lo, hi = hl.robust_box(np.vstack([co, flap]), 1.0)
        self.assertLess(hi[0], 1.1)

    def test_height_is_the_full_extent(self):
        co = np.vstack([box(10, 10, 10, (1, 1, 1), (0, 0, 0.5)), [[0, 0, 3.0]]])
        lo, hi = hl.robust_box(co, 1.0)
        self.assertEqual(hi[2], 3.0)


class Fit(unittest.TestCase):
    def setUp(self):
        self.front = box(20, 20, 20, (2.4, 2.2, 0.5), centre=(0.3, -0.2, 0.1), seed=2)
        self.back = box(20, 20, 20, (1.0, 1.5, 0.4), centre=(-1, 2, 5), seed=3)

    def test_front_lands_in_the_finish_frame(self):
        mf, _, rec = hl.fit_halves(self.front, self.back, 0, 0, 1.25)
        p = hl.apply(mf, self.front); lo, hi = hl.robust_box(p)
        self.assertAlmostEqual(lo[2], 0.0, places=9); self.assertAlmostEqual(hi[2], 1.25, places=9)
        self.assertAlmostEqual((lo[0] + hi[0]) / 2, 0.0, places=9); self.assertAlmostEqual((lo[1] + hi[1]) / 2, 0.0, places=9)
        self.assertAlmostEqual(rec['front_scale'], 1.25 / (self.front[:, 2].max() - self.front[:, 2].min()), places=5)

    def test_back_takes_the_front_height_and_footprint(self):
        mf, mb, rec = hl.fit_halves(self.front, self.back, -50, 135, 1.25)
        flo, fhi = hl.robust_box(hl.apply(mf, self.front)); blo, bhi = hl.robust_box(hl.apply(mb, self.back))
        np.testing.assert_allclose(blo, flo, atol=1e-9); np.testing.assert_allclose(bhi, fhi, atol=1e-9)
        self.assertEqual(rec['footprint_m'], [round(float(fhi[0] - flo[0]), 4), round(float(fhi[1] - flo[1]), 4)])

    def test_back_is_turned_not_reflected(self):
        _, mb, _ = hl.fit_halves(self.front, self.back, 0, 180, 1.25)
        self.assertGreater(np.linalg.det(mb[:3, :3]), 0)
        # a point on the back mesh's -Y face (its forward face, the back image's side) ends on +Y: the creature's back
        lo, hi = hl.robust_box(self.back)
        fwd = np.array([[(lo[0] + hi[0]) / 2, lo[1], (lo[2] + hi[2]) / 2]])
        self.assertGreater(hl.apply(mb, fwd)[0, 1], 0)

    def test_rotation_is_applied_before_the_fit(self):
        long_x = box(20, 20, 20, (2.0, 1.0, 1.0), seed=4)
        mf, _, rec = hl.fit_halves(long_x, self.back, 90, 0, 1.0)
        lo, hi = hl.robust_box(hl.apply(mf, long_x))
        self.assertGreater(hi[1] - lo[1], hi[0] - lo[0])  # the long side now runs along y
        self.assertGreater(rec['footprint_m'][1], rec['footprint_m'][0])

    def test_bad_inputs(self):
        with self.assertRaises(ValueError):
            hl.fit_halves(self.front, self.back, 0, 0, 0)
        flat = self.back.copy(); flat[:, 0] = 1.0
        with self.assertRaises(ValueError):
            hl.fit_halves(self.front, flat, 0, 0, 1.25)


class Masks(unittest.TestCase):
    def test_halves_overlap_by_twice_the_band(self):
        y = np.linspace(-1, 1, 201)
        f = hl.keep_mask(y, -0.2, 0.05, 'front'); b = hl.keep_mask(y, -0.2, 0.05, 'back')
        self.assertTrue((f | b).all())
        both = y[f & b]
        self.assertAlmostEqual(both.min(), -0.24, places=9); self.assertAlmostEqual(both.max(), -0.16, places=9)

    def test_zero_overlap_splits_cleanly(self):
        y = np.array([-0.5, -0.1, 0.1, 0.5])
        self.assertEqual(hl.keep_mask(y, 0, 0, 'front').tolist(), [True, True, False, False])
        self.assertEqual(hl.keep_mask(y, 0, 0, 'back').tolist(), [False, False, True, True])

    def test_bad_side_or_overlap(self):
        with self.assertRaises(ValueError):
            hl.keep_mask([0], 0, 0.1, 'left')
        with self.assertRaises(ValueError):
            hl.keep_mask([0], 0, -0.1, 'front')


class Args(unittest.TestCase):
    POS = ['f.glb', 'b.glb', 'out/m.glb', '-50', '135', '-0.2', '0.05', '1.25']

    def test_defaults_the_record_beside_the_glb(self):
        a = hl.parse_args(self.POS)
        self.assertEqual(a['record'], 'out/m_merge.json')
        self.assertEqual((a['rot_front'], a['rot_back'], a['cut'], a['overlap'], a['height']), (-50, 135, -0.2, 0.05, 1.25))

    def test_explicit_record(self):
        self.assertEqual(hl.parse_args(self.POS + ['r.json'])['record'], 'r.json')

    def test_refusals(self):
        for bad in (self.POS[:7], self.POS[:5] + ['-0.2', '-0.01', '1.25'], self.POS[:7] + ['0'], self.POS[:5] + ['2', '0', '1.25']):
            with self.assertRaises(SystemExit):
                hl.parse_args(bad)


if __name__ == '__main__':
    unittest.main()
