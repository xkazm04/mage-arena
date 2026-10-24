# The Bout 2 beasts' fit: where ../assets/beast_parts.py cuts and adds parts and where ../assets/rig_anim.py (STYLE Hound
# or Maw) puts the joints and the jaw / tongue weight rules. Blender-free, so test_beast_fit.py can check it.
#
# DEFAULTS are the tables both scripts carried until 2026-10-24, measured on the finished meshes of 2026-10-21 (hound
# seed 7 at 1.36 m, maw seed 42 at 1.25 m). They scale with the mesh height only, so a mesh with other proportions (the
# 2026-10-24 key-art masters: a hound 2.6 m long with splayed legs, docs/research/CREATURE-MESH-2026-10-24.md) needs its
# own fit file. Without one, both scripts behave exactly as before.
#
# A fit file is JSON with every key of DEFAULTS[style]: style, refHeightM (the height its metres were measured at),
# bones (the whole chain, the same bone names as the default, since the clips and rules key on them), rules (the same
# keys as the default) and parts (a list, may be empty). Positions are metres in the finish.py character frame (front -Y,
# left +X, feet at z = 0). A key left out is an error, never a silent default: the defaults belong to other meshes.
import json

COLOURS = {'Ember': (0.96, 0.45, 0.10), 'Mouth': (0.24, 0.16, 0.12), 'Tongue': (0.55, 0.24, 0.24),
           'Eye': (0.86, 0.80, 0.42), 'Pupil': (0.05, 0.05, 0.05)}
SHAPES = {'ellipsoid': ('centre', 'radii'), 'capsule': ('p0', 'p1', 'rx', 'rz')}
OPS = ('DIFFERENCE', 'UNION')


def _hound_bones():
    chain = [('pelvis', (0, 0.38, 0.84), (0, 0.18, 0.88), 'root'), ('spine_01', None, (0, -0.06, 0.89), 'pelvis'),
             ('spine_02', None, (0, -0.28, 0.89), 'spine_01'), ('spine_03', None, (0, -0.48, 0.90), 'spine_02'),
             ('neck_01', None, (0, -0.60, 1.02), 'spine_03'), ('neck_02', None, (0, -0.70, 1.13), 'neck_01'),
             ('head', None, (0, -0.96, 1.13), 'neck_02'), ('jaw', (0, -0.76, 1.10), (0, -0.95, 1.105), 'head'),
             ('tail_01', (0, 0.46, 0.88), (0, 0.62, 0.875), 'pelvis'), ('tail_02', None, (0, 0.78, 0.87), 'tail_01'),
             ('tail_03', None, (0, 0.96, 0.87), 'tail_02')]
    for sfx, sg in (('_l', 1), ('_r', -1)):
        chain += [('front_upper' + sfx, (sg * 0.10, -0.50, 0.80), (sg * 0.09, -0.46, 0.56), 'spine_03'),
                  ('front_lower' + sfx, None, (sg * 0.085, -0.485, 0.14), 'front_upper' + sfx),
                  ('front_paw' + sfx, None, (sg * 0.095, -0.60, 0.01), 'front_lower' + sfx),
                  ('back_thigh' + sfx, (sg * 0.09, 0.36, 0.80), (sg * 0.06, 0.22, 0.60), 'pelvis'),
                  ('back_shin' + sfx, None, (sg * 0.058, 0.49, 0.18), 'back_thigh' + sfx),
                  ('back_hock' + sfx, None, (sg * 0.07, 0.46, 0.04), 'back_shin' + sfx),
                  ('back_paw' + sfx, None, (sg * 0.08, 0.37, 0.01), 'back_hock' + sfx)]
    return chain


def _maw_bones():
    return [('foot_l', (0.30, -0.05, 0.25), (0.30, -0.05, 0.02), 'root'), ('foot_r', (-0.29, 0.05, 0.25), (-0.29, 0.05, 0.02), 'root'),
            ('body', (0, 0, 0.25), (0, 0, 0.58), 'root'), ('body_upper', None, (0, 0, 0.84), 'body'),
            ('head', (0, 0, 0.88), (0, 0, 1.22), 'body_upper'), ('jaw', (0, -0.10, 0.84), (0, -0.56, 0.80), 'body_upper'),
            ('tongue_01', (0, -0.10, 0.858), (0, -0.20, 0.858), 'jaw'), ('tongue_02', None, (0, -0.30, 0.858), 'tongue_01'),
            ('tongue_03', None, (0, -0.40, 0.858), 'tongue_02'), ('tongue_04', None, (0, -0.50, 0.858), 'tongue_03')]


def _root():
    return ('root', (0, 0, 0), (0, 0, 0.15), None)


DEFAULTS = {
    'Hound': {
        'style': 'Hound', 'refHeightM': 1.36,
        'bones': [_root()] + _hound_bones(),
        # the jaw takes the muzzle below the lip line: vertices ahead of muzzleY and above minZ; the lip line is lipZ at
        # noseY, rising lipRise per metre towards the cheek; the jaw weight ramps in over `ramp` metres ahead of muzzleY
        'rules': {'muzzleY': -0.74, 'minZ': 1.0, 'lipZ': 1.115, 'lipRise': 0.12, 'noseY': -0.96, 'ramp': 0.05},
        # mouth slit: the muzzle runs from the nose tip (y -0.964, z 1.09-1.14) back to the cheek (y -0.80); the slit is
        # 1.4 cm tall at the lip line z 1.115 and closes towards the cheek, where the jaw stays joined to the skull
        'parts': [{'label': 'mouth slit (Ember lining)', 'shape': 'ellipsoid', 'op': 'DIFFERENCE', 'part': 'Ember',
                   'centre': (0, -0.97, 1.115), 'radii': (0.12, 0.17, 0.007)}],
    },
    'Maw': {
        'style': 'Maw', 'refHeightM': 1.25,
        'bones': [_root()] + _maw_bones(),
        # tongue faces (the _Tongue material) go to tongue_01-04 by their place: tongueRootY, then one bone per tongueStep;
        # the lower lip: ahead of backWallY (ramp backRamp), at or under lipZ, above bellyZ (ramp bellyRamp), and within
        # halfWidth of x = jawX (ramp xRamp; the defaults 0, 99, 1 bound nothing: the 2026-10-21 mound had no legs there)
        'rules': {'tongueRootY': -0.10, 'tongueStep': 0.10, 'backWallY': -0.12, 'backRamp': 0.18, 'lipZ': 0.86,
                  'bellyZ': 0.58, 'bellyRamp': 0.14, 'jawX': 0.0, 'halfWidth': 99.0, 'xRamp': 1.0},
        'parts': [
            # mouth slot: a lens across the front at z 0.86 (the sphere's front is y -0.56 there), 9 cm tall at the
            # centre, 46 cm deep, opening over about +-0.42 m of the front
            {'label': 'mouth slot', 'shape': 'ellipsoid', 'op': 'DIFFERENCE', 'part': 'Mouth',
             'centre': (0, -0.45, 0.86), 'radii': (0.45, 0.35, 0.045), 'segs': 48, 'rings': 16},
            # tongue: rooted 20 cm inside the slot's back wall (y -0.10) so a thick section crosses it, lying in the slot,
            # tip short of the lips (y -0.50)
            {'label': 'tongue', 'shape': 'capsule', 'op': 'UNION', 'part': 'Tongue',
             'p0': (0, 0.10, 0.858), 'p1': (0, -0.50, 0.858), 'rx': 0.085, 'rz': 0.017},
        ] + [p for sg in (1, -1) for p in (  # eyes on the brow, half sunk into the skin, each with a pupil in front
            {'label': 'eye', 'shape': 'ellipsoid', 'op': 'UNION', 'part': 'Eye',
             'centre': (sg * 0.22, -0.36, 1.05), 'radii': (0.066, 0.066, 0.066), 'segs': 20, 'rings': 10},
            {'label': 'pupil', 'shape': 'ellipsoid', 'op': 'UNION', 'part': 'Pupil',
             'centre': (sg * 0.226, -0.418, 1.056), 'radii': (0.028, 0.016, 0.03), 'segs': 16, 'rings': 8})],
    },
}
KIND_STYLE = {'CinderHound': 'Hound', 'MireMaw': 'Maw'}  # beast_parts.py's kind -> the rig style


def _vec(v, where):
    if not (isinstance(v, (list, tuple)) and len(v) == 3 and all(isinstance(x, (int, float)) for x in v)):
        raise ValueError(f'{where}: a 3-vector of numbers, got {v!r}')
    return tuple(float(x) for x in v)


def _num(v, where, positive=False):
    if not isinstance(v, (int, float)) or isinstance(v, bool) or (positive and v <= 0):
        raise ValueError(f'{where}: a {"positive " if positive else ""}number, got {v!r}')
    return float(v)


def validate(fit, style):
    """Checks a fit against DEFAULTS[style]'s shape; returns it with tuples. Raises ValueError naming the first fault."""
    if style not in DEFAULTS: raise ValueError(f'style {style!r}: one of {", ".join(DEFAULTS)}')
    d = DEFAULTS[style]
    missing = [k for k in d if k not in fit]
    if missing: raise ValueError(f'fit misses {", ".join(missing)} (no key falls back to the 2026-10-21 tables)')
    extra = [k for k in fit if k not in d and not k.startswith('_')]
    if extra: raise ValueError(f'fit has unknown keys {", ".join(extra)}')
    if fit['style'] != style: raise ValueError(f'fit is for {fit["style"]!r}, the rig style is {style!r}')
    out = {'style': style, 'refHeightM': _num(fit['refHeightM'], 'refHeightM', True)}
    names = [b[0] for b in d['bones']]; seen = set(); bones = []
    for i, b in enumerate(fit['bones']):
        if not (isinstance(b, (list, tuple)) and len(b) == 4): raise ValueError(f'bones[{i}]: [name, head | null, tail, parent | null]')
        n, h, t, p = b
        if n in seen: raise ValueError(f'bone {n} twice')
        if p is None and n != 'root' or p is not None and p not in seen: raise ValueError(f'bone {n}: parent {p!r} must come before it')
        if h is None and p is None: raise ValueError(f'bone {n}: a head of null needs a parent (it starts at the parent tail)')
        bones.append((n, None if h is None else _vec(h, f'bone {n} head'), _vec(t, f'bone {n} tail'), p)); seen.add(n)
    if sorted(seen) != sorted(names):
        raise ValueError(f'bones must be exactly the {style} skeleton; missing {sorted(set(names) - seen)}, unknown {sorted(seen - set(names))}')
    out['bones'] = bones
    if sorted(fit['rules']) != sorted(d['rules']): raise ValueError(f'rules must have exactly {", ".join(sorted(d["rules"]))}')
    out['rules'] = {k: _num(v, f'rules.{k}') for k, v in fit['rules'].items()}
    parts = []
    for i, p in enumerate(fit['parts']):
        if p.get('shape') not in SHAPES: raise ValueError(f'parts[{i}]: shape one of {", ".join(SHAPES)}')
        if p.get('op') not in OPS: raise ValueError(f'parts[{i}]: op one of {", ".join(OPS)}')
        if p.get('part') not in COLOURS: raise ValueError(f'parts[{i}]: part one of {", ".join(COLOURS)}')
        q = dict(p)
        for k in SHAPES[p['shape']]:
            if k not in p: raise ValueError(f'parts[{i}] ({p["shape"]}) needs {k}')
            q[k] = _vec(p[k], f'parts[{i}].{k}') if k in ('centre', 'radii', 'p0', 'p1') else _num(p[k], f'parts[{i}].{k}', True)
        if p['shape'] == 'ellipsoid' and min(q['radii']) <= 0: raise ValueError(f'parts[{i}].radii must be positive')
        q.setdefault('label', p['part'].lower())
        parts.append(q)
    out['parts'] = parts
    return out


def load(path, style):
    """The fit for a rig style: DEFAULTS[style] when path is empty, else the validated file."""
    if not path: return validate(DEFAULTS[style], style)
    with open(path, encoding='utf8') as f: return validate(json.load(f), style)
