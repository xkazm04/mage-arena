# Offline proof of the seam ledge's Blender-free logic (ledge_lib.py): the band weight, the top pin, the Taubin smoothing
# (a step closes, pinned vertices stay, the mesh keeps its size), the fold count and the side-view measures (a ledge line,
# a wart, the outline and the ground notch). numpy only, no Blender.
#
#   python -m unittest apps/vr/tools/mesh/test_ledge_lib.py
import os
import sys
import unittest

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ledge_lib as ll  # noqa: E402


def grid(n, m, height):
    """An n x m vertex grid in the x = height(y) plane: rows z, columns y, 1 cm apart, with its edges and triangles."""
    y, z = np.meshgrid(np.arange(m) * 0.01, np.arange(n) * 0.01)
    co = np.stack([height(y), y, z], -1).reshape(-1, 3)
    idx = np.arange(n * m).reshape(n, m)
    ea = np.concatenate([idx[:, :-1].ravel(), idx[:-1, :].ravel()]); eb = np.concatenate([idx[:, 1:].ravel(), idx[1:, :].ravel()])
    a, b, c, d = idx[:-1, :-1].ravel(), idx[:-1, 1:].ravel(), idx[1:, :-1].ravel(), idx[1:, 1:].ravel()
    tris = np.concatenate([np.stack([a, b, d], 1), np.stack([a, d, c], 1)])
    return co, ea, eb, tris


class BandWeight(unittest.TestCase):
    def test_core_edge_and_between(self):
        w = ll.band_weight([-0.30, -0.25, -0.20, -0.10, 0.0], -0.30, 0.05, 0.15)
        self.assertEqual(w[0], 1.0); self.assertEqual(w[1], 1.0)
        self.assertTrue(0 < w[2] < 1)
        self.assertEqual(w[3], 0.0); self.assertEqual(w[4], 0.0)

    def test_symmetric(self):
        self.assertAlmostEqual(float(ll.band_weight(-0.2, -0.3, 0.05, 0.15)), float(ll.band_weight(-0.4, -0.3, 0.05, 0.15)))

    def test_refuses_an_empty_ramp(self):
        with self.assertRaises(ValueError):
            ll.band_weight(0, 0, 0.1, 0.1)

    def test_pin_top(self):
        w = ll.pin_top([0.0, 1.0, 1.24, 1.25], [1, 1, 1, 1], 0.03)
        self.assertEqual(w.tolist(), [1, 1, 0, 0])


class Smoothing(unittest.TestCase):
    def setUp(self):
        # a 10 cm step in x halfway along y: the ledge
        self.co, self.ea, self.eb, self.tris = grid(20, 40, lambda y: np.where(y < 0.195, 0.10, 0.0))

    def step(self, co):
        x = co[:, 0].reshape(20, 40)
        return float(np.abs(np.diff(x[10])).max())

    def test_closes_a_step(self):
        w = ll.band_weight(self.co[:, 1], 0.195, 0.08, 0.15)
        out = ll.taubin(self.co, self.ea, self.eb, w, 120)
        self.assertGreater(self.step(self.co), 0.09)
        self.assertLess(self.step(out), 0.1 / 3)  # the 10 cm step to a third or less between neighbours 1 cm apart

    def test_zero_weight_pins(self):
        out = ll.taubin(self.co, self.ea, self.eb, np.zeros(len(self.co)), 30)
        self.assertTrue(np.array_equal(out, self.co))

    def test_far_vertices_stay(self):
        w = ll.band_weight(self.co[:, 1], 0.195, 0.03, 0.08)
        out = ll.taubin(self.co, self.ea, self.eb, w, 30)
        far = np.abs(self.co[:, 1] - 0.195) > 0.08
        self.assertTrue(np.array_equal(out[far], self.co[far]))

    def test_taubin_keeps_a_sphere_size(self):
        # the inflate step keeps a closed shape from shrinking, which plain umbrella steps do
        rng = np.random.default_rng(1); p = rng.normal(size=(400, 3)); p /= np.linalg.norm(p, axis=1, keepdims=True)
        from scipy.spatial import ConvexHull
        h = ConvexHull(p); e = np.concatenate([h.simplices[:, [0, 1]], h.simplices[:, [1, 2]], h.simplices[:, [2, 0]]])
        t = ll.taubin(p, e[:, 0], e[:, 1], np.ones(len(p)), 20)
        plain = p.copy()
        for _ in range(40):
            plain = plain + 0.5 * (ll.neighbour_mean(plain, e[:, 0], e[:, 1]) - plain)
        self.assertGreater(np.linalg.norm(t, axis=1).mean(), np.linalg.norm(plain, axis=1).mean())
        self.assertGreater(np.linalg.norm(t, axis=1).mean(), 0.9)

    def test_fold_count(self):
        co2 = self.co.copy(); co2[:, 2] *= -1  # mirrored in z: every face turns over
        self.assertEqual(ll.flipped_faces(self.co, self.co, self.tris), 0)
        self.assertEqual(ll.flipped_faces(self.co, co2, self.tris), len(self.tris))


class Measures(unittest.TestCase):
    def maps(self, ledge=True, wart=True):
        y = np.arange(-0.6, 0.0, 0.002); z = np.arange(0.001, 1.0, 0.002)
        d = np.full((len(z), len(y)), 0.8)
        if ledge:
            rows = (z > 0.2) & (z < 0.8); d[np.ix_(rows, y < -0.30)] = 0.9  # a 10 cm ledge, 60 cm tall, at y -0.30
        if wart:
            rows = (z > 0.5) & (z < 0.54); d[np.ix_(rows, (y > -0.1) & (y < -0.06))] = 0.85
        d[np.ix_(z < 0.25, (y > -0.3) & (y < -0.2))] = np.nan  # a 25 cm notch at the ground behind the ledge
        n = np.zeros(d.shape + (3,)); n[..., 0] = 1
        return d, n, y, z

    def test_ledge_line_in_band(self):
        d, n, y, z = self.maps()
        r = ll.flank_report(d, n, y, z, -0.30, 0.10)
        self.assertGreater(r['depth_ledge']['band_height_m'], 0.5)
        self.assertAlmostEqual(r['depth_ledge']['band_at_y'], -0.30, delta=0.004)
        self.assertAlmostEqual(r['depth_ledge']['band_median'], 0.10, places=6)
        # the wart is 4 cm tall: a short line outside the band
        self.assertLess(r['depth_ledge']['outside_height_m'], 0.05)

    def test_no_ledge(self):
        d, n, y, z = self.maps(ledge=False)
        r = ll.flank_report(d, n, y, z, -0.30, 0.10)
        self.assertLess(r['depth_ledge']['band_height_m'], 0.05)

    def test_ground_notch(self):
        # the band reaches past the notch's far edge by more than the reach (5 columns, 1 cm)
        d, n, y, z = self.maps()
        r = ll.flank_report(d, n, y, z, -0.30, 0.12)
        self.assertAlmostEqual(r['ground_notch_m']['band_max'], 0.25, delta=0.003)
        self.assertEqual(r['ground_notch_m']['outside_max'], 0.0)

    def test_outlines(self):
        d = np.array([[np.nan, 1.0], [1.0, 1.0], [np.nan, np.nan]])
        top, bot = ll.outlines(d, [0.0, 0.1, 0.2])
        self.assertEqual(top.tolist(), [0.1, 0.1]); self.assertEqual(bot.tolist(), [0.1, 0.0])


if __name__ == '__main__':
    unittest.main()
