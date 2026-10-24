# Offline proof of the beast fit (beast_fit.py) that beast_parts.py and rig_anim.py read: the defaults are the 2026-10-21
# tables both scripts carried, a fit file must be complete, and the committed fit files validate. No Blender.
#
#   python -m unittest apps/vr/tools/mesh/test_beast_fit.py
import copy
import glob
import json
import os
import sys
import tempfile
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import beast_fit as bf  # noqa: E402


def names(fit):
    return [b[0] for b in fit['bones']]


class Defaults(unittest.TestCase):
    """The tables that were hard-coded in rig_anim.py and beast_parts.py until 2026-10-24, spot-checked verbatim."""

    def test_hound_skeleton(self):
        d = bf.load('', 'Hound')
        self.assertEqual(d['refHeightM'], 1.36)
        self.assertEqual(len(d['bones']), 26)  # the 26-bone quadruped of the 2026-10-21 HANDOFF
        bones = {b[0]: b for b in d['bones']}
        self.assertEqual(bones['jaw'], ('jaw', (0.0, -0.76, 1.10), (0.0, -0.95, 1.105), 'head'))
        self.assertEqual(bones['back_paw_r'], ('back_paw_r', None, (-0.08, 0.37, 0.01), 'back_hock_r'))
        self.assertEqual(d['rules'], {'muzzleY': -0.74, 'minZ': 1.0, 'lipZ': 1.115, 'lipRise': 0.12, 'noseY': -0.96, 'ramp': 0.05})
        self.assertEqual([(p['centre'], p['radii'], p['op'], p['part']) for p in d['parts']],
                         [((0.0, -0.97, 1.115), (0.12, 0.17, 0.007), 'DIFFERENCE', 'Ember')])

    def test_maw_skeleton(self):
        d = bf.load('', 'Maw')
        self.assertEqual(d['refHeightM'], 1.25)
        self.assertEqual(names(d), ['root', 'foot_l', 'foot_r', 'body', 'body_upper', 'head', 'jaw',
                                    'tongue_01', 'tongue_02', 'tongue_03', 'tongue_04'])
        self.assertEqual([p['label'] for p in d['parts']], ['mouth slot', 'tongue', 'eye', 'pupil', 'eye', 'pupil'])
        self.assertEqual(d['parts'][2]['centre'], (0.22, -0.36, 1.05))
        self.assertEqual(d['parts'][4]['centre'], (-0.22, -0.36, 1.05))
        self.assertEqual(d['rules']['backWallY'], -0.12)

    def test_kinds_map_to_styles(self):
        self.assertEqual(bf.KIND_STYLE, {'CinderHound': 'Hound', 'MireMaw': 'Maw'})


class FitFiles(unittest.TestCase):
    def roundtrip(self, fit, style):
        with tempfile.TemporaryDirectory() as t:
            p = os.path.join(t, 'fit.json')
            with open(p, 'w', encoding='utf8') as f: json.dump(fit, f)
            return bf.load(p, style)

    def test_a_file_of_the_defaults_loads_to_the_defaults(self):
        for style in bf.DEFAULTS:
            self.assertEqual(self.roundtrip(bf.DEFAULTS[style], style), bf.load('', style))

    def test_incomplete_or_wrong_files_are_refused(self):
        base = json.loads(json.dumps(bf.DEFAULTS['Hound']))
        cases = {
            'fit misses parts': lambda f: f.pop('parts'),
            'fit is for': lambda f: f.update(style='Maw'),
            'unknown keys': lambda f: f.update(scale=2),
            'exactly the Hound skeleton': lambda f: f['bones'].pop(),
            'must come before it': lambda f: f['bones'].insert(1, f['bones'].pop(2)),
            'rules must have exactly': lambda f: f['rules'].pop('ramp'),
            'shape one of': lambda f: f['parts'][0].update(shape='cube'),
            'part one of': lambda f: f['parts'][0].update(part='Horn'),
            'radii must be positive': lambda f: f['parts'][0].update(radii=[0.1, 0, 0.1]),
            'a 3-vector': lambda f: f['bones'][1].__setitem__(2, [0, 1]),
            'positive number': lambda f: f.update(refHeightM=0),
        }
        for msg, spoil in cases.items():
            f = copy.deepcopy(base); spoil(f)
            with self.subTest(msg), self.assertRaisesRegex(ValueError, msg): bf.validate(f, 'Hound')

    def test_committed_fits_validate(self):
        files = sorted(glob.glob(os.path.join(HERE, 'beast-fits', '*.json')))
        self.assertTrue(files)
        for p in files:
            with open(p, encoding='utf8') as f: style = json.load(f)['style']
            fit = bf.load(p, style)
            self.assertEqual(sorted(names(fit)), sorted(names(bf.DEFAULTS[style])), p)

    def test_hound_s7101_fit_sits_on_its_mesh(self):
        # the 2026-10-24 hound: nose tip at y -1.30 and the front paws at y -0.96 (CinderHound_finish.glb, 1.357 m); the
        # 2026-10-21 tables put them at -0.96 and -0.60, which is why this mesh needs its own fit
        fit = bf.load(os.path.join(HERE, 'beast-fits', 'cinder-hound-s7101-s42.json'), 'Hound')
        bones = {b[0]: b for b in fit['bones']}
        self.assertAlmostEqual(bones['head'][2][1], -1.30)
        self.assertAlmostEqual(bones['front_paw_l'][2][1], -0.96)
        slit = fit['parts'][0]
        self.assertLess(slit['centre'][1] + slit['radii'][1], fit['rules']['muzzleY'])  # the slit ends ahead of the jaw hinge rule
        self.assertAlmostEqual(slit['centre'][2], fit['rules']['lipZ'])  # and is cut on the lip line the jaw rule splits at

    def test_maw_s7202_fit_sits_on_its_mesh(self):
        # the 2026-10-24 maw: the gape's throat wall is at y -0.55 and the lower lip at y -0.97 (a section through the
        # finished mesh); the jaw hinges at the wall and reaches the lip, the tongue is rooted inside the wall and runs
        # straight forward on the floor, and the jaw rule stops short of the front legs (x +-0.55 or more)
        fit = bf.load(os.path.join(HERE, 'beast-fits', 'mire-maw-s7202-s7.json'), 'Maw')
        bones, r = {b[0]: b for b in fit['bones']}, fit['rules']
        self.assertAlmostEqual(bones['jaw'][1][1], -0.50); self.assertAlmostEqual(bones['jaw'][2][1], -0.97)
        tongue = fit['parts'][0]
        self.assertEqual((tongue['shape'], tongue['part']), ('capsule', 'Tongue'))
        self.assertGreater(tongue['p0'][1], r['backWallY'])
        self.assertEqual(tongue['p0'][2], tongue['p1'][2])
        self.assertLessEqual(r['halfWidth'], 0.55)
        self.assertEqual(bf.DEFAULTS['Maw']['rules']['halfWidth'], 99.0)  # the default bounds nothing


if __name__ == '__main__':
    unittest.main()
