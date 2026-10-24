# Offline proof of finish.py's Blender-free logic (apps/vr/tools/mesh/finish_lib.py): mode selection, the style clamp
# math, UV coverage, bake-miss accounting and filling, and the normal map (miss fill toward flat, deviation, 16-bit PNG).
# numpy only, no Blender.
#
#   python -m unittest apps/vr/tools/assets/test_finish.py
import os
import sys
import tempfile
import unittest

import numpy as np

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'mesh'))
import finish_lib as fl  # noqa: E402

BRENNIC = {  # finish.py PALETTES['Brennic']['PALETTE']
    'Tunic': (0.56, 0.24, 0.20), 'Cloak': (0.25, 0.21, 0.18), 'Leather': (0.42, 0.28, 0.19),
    'Iron': (0.40, 0.41, 0.42), 'Skin': (0.76, 0.54, 0.40), 'Hair': (0.20, 0.14, 0.11),
}
POS = ['hi.glb', 'out', 'Brennic', '0', '4096', '0.01', '24000']


def textured(n=64, seed=3):
    """A texture-like test image: smooth palette-ish gradients plus fine noise (what a bake looks like)."""
    rng = np.random.default_rng(seed)
    y, x = np.mgrid[0:n, 0:n] / n
    base = np.stack([0.3 + 0.3 * x, 0.15 + 0.2 * y, 0.1 + 0.1 * x * y], -1)
    return np.clip(base + rng.normal(0, 0.04, base.shape), 0, 1)


class ModeSelection(unittest.TestCase):
    def test_legacy_call_is_flat(self):
        # finish.mjs before the bake mode, and the HANDOFF rebuild line: 7 positionals and a palette, nothing else
        pos, palette, o = fl.split_args(POS + ['Brennic'])
        self.assertEqual((pos, palette, o['mode'], o['clamp']), (POS, 'Brennic', 'flat', 'none'))

    def test_palette_is_optional(self):
        _, palette, o = fl.split_args(POS)
        self.assertEqual((palette, o['mode']), ('Brennic', 'flat'))
        _, palette, o = fl.split_args(POS + ['mode=bake'])
        self.assertEqual((palette, o['mode']), ('Brennic', 'bake'))

    def test_bake_with_options(self):
        _, palette, o = fl.split_args(POS + ['Conscript', 'mode=bake', 'clamp=both', 'strength=0.5', 'levels=6', 'cage=0.015', 'ray=0.1', 'margin=8'])
        self.assertEqual(palette, 'Conscript')
        self.assertEqual(o, {'mode': 'bake', 'clamp': 'both', 'strength': 0.5, 'levels': 6, 'cage': 0.015, 'ray': 0.1, 'margin': 8,
                             'normal': 'off'})
        self.assertIsInstance(o['levels'], int)

    def test_rejections(self):
        bad = [['mode=paint'], ['clamp=flat', 'mode=bake'], ['clamp=hue'],  # a clamp without a bake
               ['mode=bake', 'clamp=hue', 'strength=1'], ['mode=bake', 'strength=0'], ['mode=bake', 'levels=2'],
               ['mode=bake', 'cage=-0.01'], ['colour=red'], ['Brennic', 'bake']]
        for extra in bad:
            with self.assertRaises(ValueError, msg=extra):
                fl.split_args(POS + extra)
        with self.assertRaises(ValueError):
            fl.split_args(POS[:6])

    def test_normal_is_off_unless_asked(self):
        # the flat call and the current bake call parse to the options they parsed to before the normal bake existed
        self.assertEqual(fl.split_args(POS + ['Brennic'])[2]['normal'], 'off')
        self.assertEqual(fl.split_args(POS + ['Brennic', 'mode=bake'])[2]['normal'], 'off')
        o = fl.split_args(POS + ['Brennic', 'mode=bake', 'normal=on'])[2]
        self.assertEqual((o['mode'], o['normal'], o['cage'], o['ray'], o['margin']), ('bake', 'on', 0.02, 0.08, 16))
        for extra in (['normal=on'], ['mode=flat', 'normal=on'], ['mode=bake', 'normal=yes'], ['mode=bake', 'normal=1']):
            with self.assertRaises(ValueError, msg=extra):
                fl.split_args(POS + extra)


class Colour(unittest.TestCase):
    def test_lab_round_trip(self):
        c = np.random.default_rng(1).random((500, 3))
        np.testing.assert_allclose(fl.lab_to_srgb(fl.srgb_to_lab(c)), c, atol=1e-6)

    def test_lab_known_values(self):
        np.testing.assert_allclose(fl.srgb_to_lab([1, 1, 1]), [100, 0, 0], atol=0.01)
        np.testing.assert_allclose(fl.srgb_to_lab([0, 0, 0]), [0, 0, 0], atol=0.01)
        np.testing.assert_allclose(fl.srgb_to_lab([1, 0, 0]), [53.24, 80.09, 67.20], atol=0.05)


class StyleClamp(unittest.TestCase):
    def test_none_is_identity(self):
        img = textured()
        np.testing.assert_array_equal(fl.style_clamp(img, BRENNIC, 'none'), img)

    def test_posterize_moves_lightness_partway_only(self):
        img = textured()
        out = fl.style_clamp(img, BRENNIC, 'posterize', strength=0.5, levels=8)
        L0, L1 = fl.srgb_to_lab(img)[..., 0], fl.srgb_to_lab(out)[..., 0]
        q = np.round(L0 / 12.5) * 12.5
        # half way to the nearest level, no further; 0.1 L* of slack for the few texels the sRGB gamut clip moves back
        np.testing.assert_allclose(L1, L0 + 0.5 * (q - L0), atol=0.1)
        self.assertLessEqual(np.abs(L1 - L0).max(), 0.5 * 12.5 / 2 + 1e-3)

    def test_hue_snap_turns_toward_nearest_anchor_and_keeps_l_and_chroma(self):
        green = np.array([[0.25, 0.45, 0.20]])  # off-palette green (VISUAL-PLAN (c) exclusion)
        out = fl.style_clamp(green, BRENNIC, 'hue', strength=0.5)
        a, b = fl.srgb_to_lab(green)[0], fl.srgb_to_lab(out)[0]
        self.assertAlmostEqual(a[0], b[0], places=3)
        self.assertAlmostEqual(np.hypot(a[1], a[2]), np.hypot(b[1], b[2]), places=3)
        h = lambda v: np.arctan2(v[2], v[1])  # noqa: E731
        hues = fl.anchor_hues(BRENNIC)
        dist = lambda x: np.abs((hues - x + np.pi) % (2 * np.pi) - np.pi).min()  # noqa: E731
        self.assertLess(dist(h(b)), dist(h(a)))
        self.assertAlmostEqual(dist(h(b)), 0.5 * dist(h(a)), places=6)

    def test_hue_snap_leaves_greys(self):
        grey = np.array([[0.4, 0.4, 0.4], [0.7, 0.7, 0.7]])
        np.testing.assert_allclose(fl.style_clamp(grey, BRENNIC, 'hue', strength=0.6), grey, atol=1e-6)

    def test_never_flat(self):
        # the brief's bound: the clamp may never replace the texture with flat colours. At the strongest allowed
        # setting the output keeps most of the input's detail and far more colours than the palette has.
        img = textured(128)
        out = fl.style_clamp(img, BRENNIC, 'both', strength=fl.MAX_STRENGTH, levels=fl.MIN_LEVELS)
        self.assertGreater(fl.detail_kept(img, out), 0.6)
        distinct = len(np.unique(np.round(out.reshape(-1, 3) * 255).astype(int), axis=0))
        self.assertGreater(distinct, 50 * len(BRENNIC))

    def test_clamp_rejects_out_of_bounds(self):
        for kw in ({'strength': 0.9}, {'strength': 0}, {'levels': 3}):
            with self.assertRaises(ValueError):
                fl.style_clamp(textured(8), BRENNIC, 'both', **kw)
        with self.assertRaises(ValueError):
            fl.style_clamp(textured(8), BRENNIC, 'flat')

    def test_element_colours_are_not_anchors(self):
        # water turquoise #19E0E0 is reserved for magic: a turquoise texel is pulled away from it, never toward it
        turq = np.array([[0x19 / 255, 0xE0 / 255, 0xE0 / 255]])
        hues = fl.anchor_hues(BRENNIC)
        th = np.arctan2(*fl.srgb_to_lab(turq)[0, [2, 1]])
        self.assertGreater(np.abs((hues - th + np.pi) % (2 * np.pi) - np.pi).min(), 1.0)


class Bake(unittest.TestCase):
    def test_uv_coverage_square(self):
        # two triangles covering the lower-left quarter of the square: exactly a quarter of the texels
        tris = np.array([[[0, 0], [0.5, 0], [0.5, 0.5]], [[0, 0], [0.5, 0.5], [0, 0.5]]])
        cov = fl.uv_coverage(tris, 16)
        self.assertEqual(cov.sum(), 64)
        self.assertTrue(cov[:8, :8].all())  # row 0 is v = 0
        self.assertFalse(cov[8:, :].any() or cov[:, 8:].any())

    def test_uv_coverage_area(self):
        cov = fl.uv_coverage(np.array([[[0.1, 0.1], [0.9, 0.1], [0.1, 0.9]]]), 256)
        self.assertAlmostEqual(cov.sum() / 256 ** 2, 0.32, delta=0.01)

    def test_misses_interior_and_edge(self):
        cov = np.zeros((10, 10), bool); cov[2:8, 2:8] = True
        hit = cov.astype(float); hit[4, 4] = 0; hit[2, 5] = 0  # one interior miss, one on the island edge
        miss, n = fl.bake_misses(hit, cov)
        self.assertEqual(n, {'covered': 36, 'missed': 2, 'missed_interior': 1})
        self.assertTrue(miss[4, 4] and miss[2, 5])

    def test_dark_texels(self):
        img = np.full((4, 4, 3), 0.3); img[1, 1] = 0; img[2, 2] = (0.004, 0.0, 0.0); img[3, 3] = 0
        mask = np.ones((4, 4), bool); mask[3, 3] = False
        self.assertEqual(fl.dark_texels(img, mask), 2)

    def test_fill_misses(self):
        img = np.zeros((9, 9, 3)); img[..., 0] = np.arange(9)[None, :] / 8  # a red ramp
        miss = np.zeros((9, 9), bool); miss[3:6, 3:6] = True
        holed = img.copy(); holed[miss] = 0
        out, rings = fl.fill_misses(holed, miss)
        self.assertEqual(rings, 2)
        np.testing.assert_array_equal(out[~miss], img[~miss])  # known texels untouched
        self.assertGreater(out[miss][:, 0].min(), 0.2)  # no black left
        np.testing.assert_allclose(out[4, 4, 0], 0.5, atol=0.05)

    def test_fill_misses_from_island_texels_only(self):
        # an island (brown) whose edge texels were missed, next to background Blender left black: the default fill takes
        # the black margin in, the island-only pass (known = the island's hit texels) does not
        img = np.zeros((10, 10, 3)); cov = np.zeros((10, 10), bool); cov[2:8, 2:8] = True; img[cov] = (0.4, 0.3, 0.2)
        miss = np.zeros_like(cov); miss[2:8, 2] = True; img[miss] = 0
        default, _ = fl.fill_misses(img, miss)
        self.assertLess(default[miss][:, 0].min(), 0.4)  # pulled toward the black background
        island, rings = fl.fill_misses(img, miss, known=cov & ~miss)
        self.assertEqual(rings, 1)
        np.testing.assert_allclose(island[miss], np.tile((0.4, 0.3, 0.2), (6, 1)))
        np.testing.assert_array_equal(island[~cov], img[~cov])  # it never grows into the background
        # a missed island with no hit texel is left for fill_orphans
        cov2 = cov.copy(); cov2[0, 9] = True; miss2 = miss.copy(); miss2[0, 9] = True
        out, _ = fl.fill_misses(img, miss2, known=cov2 & ~miss2)
        self.assertEqual(fl.dark_texels(out, cov2), 1)

    def test_fill_orphans_takes_the_hit_median(self):
        img = np.zeros((6, 6, 3)); img[:3] = (0.4, 0.3, 0.2); img[3:] = 0  # hit half brown, a missed patch black
        orphan = np.zeros((6, 6), bool); orphan[3:] = True
        out, colour = fl.fill_orphans(img, orphan, ~orphan)
        np.testing.assert_allclose(colour, (0.4, 0.3, 0.2))
        np.testing.assert_allclose(out[orphan], np.tile((0.4, 0.3, 0.2), (18, 1)))
        np.testing.assert_array_equal(out[~orphan], img[~orphan])
        self.assertEqual(fl.dark_texels(out, np.ones((6, 6), bool)), 0)

    def test_texel_density(self):
        # one 1 m x 1 m square (two triangles) on the lower-left quarter of the UV square, at 1024: 512 texels per metre
        uv = [[[0, 0], [0.5, 0], [0.5, 0.5]], [[0, 0], [0.5, 0.5], [0, 0.5]]]
        p = [[[0, 0, 0], [1, 0, 0], [1, 0, 1]], [[0, 0, 0], [1, 0, 1], [0, 0, 1]]]
        d = fl.texel_density(uv, p, 1024)
        self.assertEqual((d['uv_share'], d['surface_m2'], d['texels_per_m']), (0.25, 1.0, 512.0))


class NormalMap(unittest.TestCase):
    FLAT = np.array(fl.FLAT_NORMAL)

    @staticmethod
    def encode(v):
        v = np.asarray(v, dtype=np.float64)
        return v / np.linalg.norm(v, axis=-1, keepdims=True) * 0.5 + 0.5

    def test_misses_fill_toward_flat_never_black(self):
        n = 20; cov = np.zeros((n, n), bool); cov[4:12, 4:12] = True
        nrm = np.zeros((n, n, 3)); nrm[cov] = self.encode([0.3, 0.0, 1.0])  # a tilted island on black background
        miss = np.zeros_like(cov); miss[6:8, 6:8] = True; miss[4, 4:12] = True; nrm[miss] = 0  # holes the rays left black
        out, q = fl.fill_normal_misses(nrm, miss, cov, margin=3)
        np.testing.assert_allclose(out[miss], np.tile(self.FLAT, (miss.sum(), 1)))
        np.testing.assert_array_equal(out[cov & ~miss], nrm[cov & ~miss])  # hit texels untouched
        self.assertEqual((q['missed_not_flat_before_fill'], q['dark_where_hit'], q['dark_after_fill']), (12, 0, 0))
        self.assertEqual(q['margin_rings'], 3)
        # the margin is grown from island texels only: the ring next to the hit right edge carries the island's tilt
        np.testing.assert_allclose(out[8, 12], nrm[8, 11])
        np.testing.assert_allclose(out[8, 14], nrm[8, 11])
        np.testing.assert_allclose(out[8, 15], self.FLAT)  # past the margin: flat, not black
        np.testing.assert_allclose(out[0, 0], self.FLAT)
        self.assertEqual(fl.dark_texels(out, np.ones((n, n), bool)), 0)

    def test_deviation_from_flat(self):
        mask = np.ones((10, 10), bool)
        flat = np.tile(self.FLAT, (10, 10, 1))
        self.assertEqual(fl.normal_deviation(flat, mask), {'texels': 100, 'mean_deg': 0.0, 'share_over_10deg': 0.0, 'share_over_20deg': 0.0})
        img = flat.copy()
        t = np.radians
        img[:2] = self.encode([np.sin(t(15)), 0, np.cos(t(15))])  # 20 texels at 15 degrees
        img[2:3] = self.encode([0, -np.sin(t(30)), np.cos(t(30))])  # 10 texels at 30 degrees
        d = fl.normal_deviation(img, mask)
        self.assertEqual((d['share_over_10deg'], d['share_over_20deg']), (0.3, 0.1))
        self.assertAlmostEqual(d['mean_deg'], (20 * 15 + 10 * 30) / 100, delta=0.05)
        sub = np.zeros_like(mask); sub[2:4] = True  # the statistic follows the mask
        self.assertEqual(fl.normal_deviation(img, sub)['share_over_20deg'], 0.5)
        self.assertEqual(fl.normal_deviation(img, np.zeros_like(mask))['texels'], 0)

    def test_back_region_rule(self):
        fn = np.array([[0, 1, 0], [0, -1, 0], [0, 0.6, 0.8], [0, 0.4, 0.917], [0, 1, 0], [0, 1, 0]])
        cz = np.array([1.0, 1.0, 1.0, 1.0, 0.3, 1.6])
        np.testing.assert_array_equal(fl.back_region_faces(fn, cz), [True, False, True, False, False, False])

    def test_png16_round_trip_and_row_order(self):
        rng = np.random.default_rng(5); img = rng.random((7, 5, 3)); img[0, 0] = (1.0, 0.0, 0.5)
        with tempfile.TemporaryDirectory() as d:
            p = os.path.join(d, 'n.png'); fl.png16(p, img)
            raw = open(p, 'rb').read()
            self.assertEqual(raw[:8], bytes([0x89]) + b'PNG' + bytes([13, 10, 26, 10]))
            self.assertEqual(raw[24:26], bytes([16, 2]))  # bit depth 16, colour type RGB
            self.assertNotIn(b'gAMA', raw); self.assertNotIn(b'sRGB', raw)
            back = fl.read_png16(p)
        np.testing.assert_allclose(back, img, atol=0.5 / 65535)
        self.assertEqual(back.shape, (7, 5, 3))


if __name__ == '__main__':
    unittest.main()
