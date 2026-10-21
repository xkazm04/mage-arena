# A07 / T42: the arena environment kit. Hand-authored parametric geometry (bmesh) in Blender, sized from
# apps/vr/data/vr/arena-layout.json (read only) and shaped after the A07 sheets environment-kit/01..03-*.jpg (reference only).
# No generation step: no Leonardo, no TRELLIS, no ComfyUI, 0 credits. Patterns copied from weapon.py, metrics.py, render_evidence.py.
#   blender -b -P env_kit.py -- build   <arena-layout.json> <kit3d dir> <work dir>   pieces -> <kit3d>/export/SM_Arena_<Piece>.fbx,
#                                                                                     <kit3d>/manifest.json, <work>/env_kit.blend
#   blender -b -P env_kit.py -- metrics <arena-layout.json> <kit3d dir>               re-imports every FBX -> <kit3d>/metrics.json
#   blender -b -P env_kit.py -- render  <arena-layout.json> <kit3d dir>               from the FBX files -> kit_lineup.png, arena_seated_640x480.png
# Conventions: one mesh per FBX, origin at the base (z = 0), +X forward (toward the arena centre once placed), metres, unit scale,
# flat colours, no textures, triangulated. Walls, the gate and the tiers have their origin on the arena-facing line (x = 0), body at
# -X, length along Y. Every piece draws from the one shared set of six materials M_Arena_<Name>, so instances merge (T37).
# The manifest's assembly is in Unreal axes (layout axes: +Y is the player's right). Blender's +Y is Unreal's -Y after the FBX
# import, so the render mirrors y and yaw.
import bpy, bmesh, sys, json, math, os, hashlib, numpy as np
from mathutils import Vector, Matrix
from mathutils.bvhtree import BVHTree

a = sys.argv[sys.argv.index('--') + 1:]; MODE, LAYOUT_PATH, KIT = a[:3]
WORK = a[3] if len(a) > 3 else None
L = json.load(open(LAYOUT_PATH)); C = L['colours']
EXPORT = os.path.join(KIT, 'export')
MATS = {'Sand': C['sand'], 'Stone': C['stone'], 'StoneDark': C['dais'], 'Wood': C['bannerPole'], 'Iron': C['brazier'], 'Cloth': C['bannerCloth']}
KIT_MATS = ['M_Arena_' + m for m in MATS]
TRI_BUDGET = 60000

# ---- dimensions, all from the layout
WALL_H, WALL_T = L['wall']['heightM'], L['wall']['thicknessM']
ST = L['stands']; TIER_D, RISE, GAP = ST['depthM'], ST['stepRiseM'], ST['gapOutsideWallM']
TOPS = [WALL_H + ST['firstTopAboveWallM'] + i * RISE for i in range(ST['tiers'])]       # 3.5, 4.35, 5.2
DAIS = L['dais']['sizeM']; DAIS_TOP = L['dais']['heightM']
PADS = L['pads']; SEAT = L['seat']['sizeM']
BRZ = L['braziers'][0]; BAN = L['banners'][0]
A_, B_ = L['floor']['radiusXM'], L['floor']['radiusYM']; SEGS = L['wall']['segments']
# kit choices (not in the layout): nominal lengths, the curved pieces' radius, the gate opening
STRAIGHT_LEN = 3.0                       # straight wall and tier, front length; the assembly scales Y to each chord
SPLAY = math.pi / SEGS                   # half the corner angle of a 32-gon: the straight pieces' ends splay so the ring closes
CURVE_R = 8.0                            # curved wall, inner face radius; curved tier front = CURVE_R + WALL_T + GAP
CURVE_SPAN = 2 * math.asin(1.1 / CURVE_R)  # 2.2 m inner chord
CURVE_BELOW = 12.0                       # a chord whose ellipse radius of curvature is under this takes the curved pieces
GATE_W, GATE_H, GATE_X = 4.0, WALL_H + 0.6, (-0.7, 0.1)
OPEN_HALF, SPRING = 1.1, 1.6             # opening half width, arch springing height (top of the opening 2.7 m)
STAIR_W = 1.2
PAD_R, PAD_H, GROOVE = PADS['ringRadiusM'] + PADS['ringWidthM'] / 2 + 0.04, 0.04, (PADS['ringRadiusM'] - PADS['ringWidthM'] / 2, PADS['ringRadiusM'] + PADS['ringWidthM'] / 2, 0.015)
TIER_PROFILE = [(0, 0), (0, TOPS[0])] + [p for i in range(1, len(TOPS)) for p in ((-i * TIER_D, TOPS[i - 1]), (-i * TIER_D, TOPS[i]))] \
    + [(-len(TOPS) * TIER_D, TOPS[-1]), (-len(TOPS) * TIER_D, 0)]
WALL_RECTS = [(-WALL_T, 0, 0, WALL_H, 'Stone'),                    # body
              (-WALL_T - 0.05, 0.05, 0, 0.25, 'StoneDark'),        # plinth
              (-WALL_T - 0.03, 0.03, 2.10, 2.18, 'Stone'),         # string course under the parapet
              (-WALL_T - 0.04, 0.04, WALL_H - 0.12, WALL_H, 'StoneDark')]  # coping: a plain parapet, no merlons


def rect(x0, x1, z0, z1): return [(x0, z0), (x1, z0), (x1, z1), (x0, z1)]


class Piece:
    def __init__(s): s.bm = bmesh.new(); s.mats = []

    def mi(s, m):
        if m not in s.mats: s.mats.append(m)
        return s.mats.index(m)

    def sweep(s, prof, mat, pos, stations, caps=True):
        # prof: closed polygon of (u, v); pos(u, v, station) -> 3D; side quads between stations, end caps as n-gons
        rings = [[s.bm.verts.new(pos(u, v, k)) for u, v in prof] for k in range(stations + 1)]; n = len(prof); fs = []
        for r0, r1 in zip(rings, rings[1:]):
            for i in range(n): fs.append(s.bm.faces.new((r0[i], r0[(i + 1) % n], r1[(i + 1) % n], r1[i])))
        if caps: fs += [s.bm.faces.new(rings[0]), s.bm.faces.new(rings[-1])]
        for f in fs: f.material_index = s.mi(mat)

    def straight(s, prof, mat, length, splay=0.0):
        # profile in XZ swept along Y; the end at x has half length length/2 - x tan(splay) (planar, longer at -X)
        t = math.tan(splay); s.sweep(prof, mat, lambda x, z, k: Vector((x, (2 * k - 1) * (length / 2 - x * t), z)), 1)

    def arc(s, prof, mat, r, span, n):
        # profile in XZ swept around a centre at (r, 0) on the +X side: x = 0 lies on the arc of radius r
        def pos(x, z, k):
            f = -span / 2 + span * k / n; return Vector((r - (r - x) * math.cos(f), (r - x) * math.sin(f), z))
        s.sweep(prof, mat, pos, n)

    def yz(s, outline, x0, x1, mat):
        # outline in YZ extruded along X
        s.sweep(outline, mat, lambda y, z, k: Vector((x0 if k == 0 else x1, y, z)), 1)

    def box(s, lo, hi, mat):
        s.sweep(rect(lo[0], hi[0], lo[2], hi[2]), mat, lambda x, z, k: Vector((x, lo[1] if k == 0 else hi[1], z)), 1)

    def lathe(s, prof, mats, n, c=(0, 0), phase=0.0):
        # prof: open polyline of (r, z) whose ends lie on the axis (r = 0); mats: one name per profile edge
        ring = []
        for r, z in prof:
            if r == 0: ring.append([s.bm.verts.new((c[0], c[1], z))] * n)
            else: ring.append([s.bm.verts.new((c[0] + r * math.cos(phase + 2 * math.pi * i / n), c[1] + r * math.sin(phase + 2 * math.pi * i / n), z)) for i in range(n)])
        for e, (r0, r1) in enumerate(zip(ring, ring[1:])):
            for i in range(n):
                q = [r0[i], r0[(i + 1) % n], r1[(i + 1) % n], r1[i]]; q = [v for j, v in enumerate(q) if v not in q[:j]]
                s.bm.faces.new(q).material_index = s.mi(mats[e])

    def tube(s, p0, p1, r, mat, n=4):
        ax = (Vector(p1) - Vector(p0)).normalized(); u = ax.cross(Vector((0, 0, 1)) if abs(ax.z) < 0.9 else Vector((1, 0, 0))).normalized(); v = ax.cross(u)
        prof = [(r * math.cos(2 * math.pi * i / n + math.pi / 4), r * math.sin(2 * math.pi * i / n + math.pi / 4)) for i in range(n)]
        s.sweep(prof, mat, lambda x, y, k: (Vector(p0) if k == 0 else Vector(p1)) + u * x + v * y, 1)

    def finish(s, name):
        bm = s.bm; bm.normal_update(); bmesh.ops.recalc_face_normals(bm, faces=bm.faces); bmesh.ops.triangulate(bm, faces=bm.faces)
        me = bpy.data.meshes.new(name); bm.to_mesh(me); bm.free()
        for m in s.mats: me.materials.append(bpy.data.materials['M_Arena_' + m])
        for p in me.polygons: p.use_smooth = False
        ob = bpy.data.objects.new(name, me); bpy.context.scene.collection.objects.link(ob); return ob


# ---- the pieces
def wall_straight(p):
    for x0, x1, z0, z1, m in WALL_RECTS: p.straight(rect(x0, x1, z0, z1), m, STRAIGHT_LEN, SPLAY)


def wall_curved(p):
    for x0, x1, z0, z1, m in WALL_RECTS: p.arc(rect(x0, x1, z0, z1), m, CURVE_R, CURVE_SPAN, 8)


def gate_arch(p):
    w, h = GATE_W / 2, GATE_H; n = 14
    arch = [(OPEN_HALF * math.cos(math.pi * i / n), SPRING + OPEN_HALF * math.sin(math.pi * i / n)) for i in range(n + 1)]
    outline = [(-w, 0), (-OPEN_HALF, 0)] + arch[::-1] + [(OPEN_HALF, 0), (w, 0), (w, h), (-w, h)]
    p.yz(outline, GATE_X[0], GATE_X[1], 'Stone')
    p.box((GATE_X[0] - 0.05, -w - 0.05, h - 0.14), (GATE_X[1] + 0.05, w + 0.05, h), 'StoneDark')       # coping
    p.box((GATE_X[0] - 0.06, -w - 0.06, 0), (GATE_X[1] + 0.06, -OPEN_HALF - 0.06, 0.25), 'StoneDark')   # pier plinths
    p.box((GATE_X[0] - 0.06, OPEN_HALF + 0.06, 0), (GATE_X[1] + 0.06, w + 0.06, 0.25), 'StoneDark')
    ro = OPEN_HALF + 0.24                                                                               # arch ring, both faces
    ring = [(ro * math.cos(math.pi * i / n), SPRING + ro * math.sin(math.pi * i / n)) for i in range(n + 1)] + arch[::-1]
    p.yz(ring, GATE_X[1] - 0.01, GATE_X[1] + 0.05, 'StoneDark'); p.yz(ring, GATE_X[0] - 0.05, GATE_X[0] + 0.01, 'StoneDark')
    # wooden portcullis, closed, in a slot inside the opening: verticals behind, horizontals in front, iron spikes at the foot
    xv, xh, b = -0.33, -0.27, 0.04
    top = lambda y: SPRING + math.sqrt(OPEN_HALF ** 2 - y * y)
    for i in range(7):
        y = -0.93 + i * 0.31
        p.box((xv - b, y - b, 0.15), (xv + b, y + b, top(abs(y) + b) + 0.06), 'Wood')
        p.lathe([(0, 0.0), (0.045, 0.15), (0, 0.15)], ['Iron', 'Iron'], 4, c=(xv, y), phase=math.pi / 4)
    for z in (0.55, 1.15, 1.75, 2.3):
        half = OPEN_HALF if z <= SPRING else math.sqrt(OPEN_HALF ** 2 - (z + b - SPRING) ** 2)
        p.box((xh - b, -half - 0.06, z - b), (xh + b, half + 0.06, z + b), 'Wood')


def tier_straight(p): p.straight(TIER_PROFILE, 'Stone', STRAIGHT_LEN, SPLAY)


def tier_curved(p): p.arc(TIER_PROFILE, 'Stone', CURVE_R + WALL_T + GAP, CURVE_SPAN, 8)


def tier_stair(p):
    # the cavea section with an aisle: two half rises per tier, same footprint and top as the tier, no splay
    prof = [(0, 0)]; d, r = TIER_D / 2, RISE / 2; z = TOPS[0] - r
    for i in range(2 * len(TOPS)):
        prof += [(-i * d, z), (-(i + 1) * d, z)]; z += r
    prof = [prof[0]] + prof[1:] + [(-len(TOPS) * TIER_D, 0)]
    p.straight(prof, 'Stone', STAIR_W)


def dais(p):
    x, y, h = DAIS[0] / 2, DAIS[1] / 2, DAIS_TOP
    p.box((-x, -y, 0), (x, y, 0.08), 'StoneDark'); p.box((-x + 0.06, -y + 0.06, 0.08), (x - 0.06, y - 0.06, h - 0.08), 'StoneDark')
    p.box((-x, -y, h - 0.08), (x, y, h), 'StoneDark')


def rune_pad(p):
    g0, g1, gd = GROOVE
    p.lathe([(0, 0), (PAD_R, 0), (PAD_R, PAD_H), (g1, PAD_H), (g1, PAD_H - gd), (g0, PAD_H - gd), (g0, PAD_H), (0, PAD_H)],
            ['Stone', 'Stone', 'Stone', 'Sand', 'Sand', 'Sand', 'Stone'], 48)


def seat(p):
    x, y, h = SEAT[0] / 2, SEAT[1] / 2, SEAT[2]   # backless low block (BACKLOG A07 review): plinth, waist, top slab
    p.box((-x, -y, 0), (x, y, 0.06), 'Stone'); p.box((-x + 0.03, -y + 0.03, 0.06), (x - 0.03, y - 0.03, h - 0.06), 'Stone')
    p.box((-x, -y, h - 0.06), (x, y, h), 'Stone')


def brazier(p):
    s, br, bh = BRZ['standHeightM'], BRZ['bowlRadiusM'], BRZ['bowlHeightM']; t = s + bh
    p.lathe([(0, s), (br * 0.66, s), (br * 0.9, s + bh * 0.3), (br, s + bh * 0.75), (br, t), (br * 0.9, t), (br * 0.83, s + bh * 0.68),
             (br * 0.6, s + bh * 0.46), (0, s + bh * 0.46)], ['Iron'] * 8, 24)
    for k in range(3):   # three legs, the first on +X
        f = 2 * math.pi * k / 3; c, sn = math.cos(f), math.sin(f)
        p.tube((0.30 * c, 0.30 * sn, s + 0.04), (0.40 * c, 0.40 * sn, 0.03), 0.02, 'Iron')
        p.box((0.40 * c - 0.04, 0.40 * sn - 0.04, 0), (0.40 * c + 0.04, 0.40 * sn + 0.04, 0.03), 'Iron')


def banner(p):
    h, r = BAN['poleHeightM'], BAN['poleRadiusM']; th, w, ch = BAN['clothSizeM']
    p.lathe([(0, 0), (r, 0), (r, h - 0.06), (0, h - 0.06)], ['Wood'] * 3, 8)
    p.lathe([(0, h - 0.08), (r + 0.01, h - 0.08), (r + 0.01, h - 0.04), (0, h)], ['Iron'] * 3, 8)          # iron cap
    p.box((-0.04, -w / 2, h - 0.08), (0.04, w / 2, h - 0.02), 'Wood')                                     # crossbar
    top = h - 0.08; bot = top - ch                                                                       # cloth faces +X, swallowtail foot
    p.yz([(-w / 2, top), (-w / 2, bot), (0, bot + 0.3), (w / 2, bot), (w / 2, top)][::-1], 0.04, 0.04 + th, 'Cloth')


PIECES = {  # name: (builder, what it replaces in the greybox, pivot)
    'WallStraight': (wall_straight, 'wall: one ring-wall chord box (side chords)', 'base, middle of the arena-facing face'),
    'WallCurved': (wall_curved, 'wall: one ring-wall chord box (end chords, radius of curvature under 12 m)', 'base, middle of the arena-facing face'),
    'GateArch': (gate_arch, 'wall: the two chord boxes at the far (+X) end, theta = -11.25..11.25 degrees', 'base, middle of the arena-facing face'),
    'TierStraight': (tier_straight, 'stands: the three tier boxes behind one side chord', 'base, middle of the front (arena-facing) edge'),
    'TierCurved': (tier_curved, 'stands: the three tier boxes behind one end chord', 'base, middle of the front edge'),
    'TierStair': (tier_stair, 'stands: an aisle in four side chords (with two shortened TierStraight)', 'base, middle of the front edge'),
    'Dais': (dais, 'dais box', 'base centre'),
    'RunePad': (rune_pad, 'pads: disc cylinder and the 16 ring boxes, per pad', 'base centre (sits on the dais top)'),
    'Seat': (seat, 'seat box, per pad', 'base centre (sits on the pad)'),
    'Brazier': (brazier, 'braziers: stand and bowl (the flame cone stays a greybox or VFX part)', 'base centre'),
    'Banner': (banner, 'banners: pole, crossbar and cloth', 'base of the pole'),
}
# expected dimensions (m) from the layout: bbox x, y, z where fixed, with a tolerance
EXPECT = {
    'WallStraight': {'z': WALL_H, 'body_x': WALL_T}, 'WallCurved': {'z': WALL_H, 'body_x': WALL_T},
    'GateArch': {'z': GATE_H, 'y': GATE_W + 0.12},
    'TierStraight': {'z': TOPS[-1], 'x': len(TOPS) * TIER_D, 'front_top': TOPS[0]},
    'TierCurved': {'z': TOPS[-1], 'front_top': TOPS[0]},
    'TierStair': {'z': TOPS[-1], 'x': len(TOPS) * TIER_D, 'y': STAIR_W},
    'Dais': {'x': DAIS[0], 'y': DAIS[1], 'z': DAIS_TOP},
    'RunePad': {'x': 2 * PAD_R, 'y': 2 * PAD_R, 'z': PAD_H},
    'Seat': {'x': SEAT[0], 'y': SEAT[1], 'z': SEAT[2]},
    'Brazier': {'z': BRZ['standHeightM'] + BRZ['bowlHeightM'], 'bowl_r': BRZ['bowlRadiusM']},
    'Banner': {'z': BAN['poleHeightM'], 'y': BAN['clothSizeM'][1]},
}
OPEN = {}  # piece: reason, for a piece that is intentionally open. Every piece is closed today.


def nominal_front(piece):
    if piece in ('WallStraight', 'TierStraight'): return STRAIGHT_LEN
    if piece == 'WallCurved': return 2 * CURVE_R * math.sin(CURVE_SPAN / 2)
    if piece == 'TierCurved': return 2 * (CURVE_R + WALL_T + GAP) * math.sin(CURVE_SPAN / 2)
    return None


def assembly():
    # Unreal (layout) axes, metres, degrees. The ring follows ArenaGreyboxActor::BuildShell: 32 chords of the ellipse, each wall
    # on the chord (inner face on it), x1.04 overlap, tiers behind it at wall + gap, scaled by radius like the greybox.
    P = lambda t: Vector((A_ * math.cos(t), B_ * math.sin(t), 0))
    rho = lambda t: (A_ ** 2 * math.sin(t) ** 2 + B_ ** 2 * math.cos(t) ** 2) ** 1.5 / (A_ * B_)
    gate = {SEGS - 1, 0}; stairs = {6, 9, 22, 25}; out = []
    put = lambda piece, loc, yaw, sy=1.0: out.append({'piece': piece, 'locationM': [round(loc.x, 4), round(loc.y, 4), round(loc.z, 4)],
                                                     'yawDeg': round(yaw, 3), 'scale': [1.0, round(sy, 4), 1.0]})
    for i in range(SEGS):
        t0, t1 = 2 * math.pi * i / SEGS, 2 * math.pi * (i + 1) / SEGS; m = (t0 + t1) / 2
        p0, p1 = P(t0), P(t1); chord = (p1 - p0).length; mid = (p0 + p1) / 2; u = (p1 - p0).normalized()
        nrm = Vector((B_ * math.cos(m), A_ * math.sin(m), 0)).normalized(); yaw = math.degrees(math.atan2(-nrm.y, -nrm.x))
        if i in gate: continue
        curved = rho(m) < CURVE_BELOW
        w = 'WallCurved' if curved else 'WallStraight'; put(w, mid, yaw, chord * 1.04 / nominal_front(w))
        tm = mid + nrm * (WALL_T + GAP); need = chord * 1.04 * tm.length / mid.length
        if i in stairs:
            seg = (need - STAIR_W) / 2; put('TierStair', tm, yaw)
            for sgn in (-1, 1): put('TierStraight', tm + u * sgn * (STAIR_W + seg) / 2, yaw, seg / STRAIGHT_LEN)
        else:
            tp = 'TierCurved' if curved else 'TierStraight'; put(tp, tm, yaw, need / nominal_front(tp))
    gm = (P(-2 * math.pi / SEGS) + P(2 * math.pi / SEGS)) / 2; put('GateArch', gm, 180.0)
    put('Dais', Vector((L['dais']['centreM'][0], L['dais']['centreM'][1], 0)), 0.0)
    for pad in PADS['items']:
        q = Vector(pad['positionM']); yaw = math.degrees(math.atan2(-q.y, -q.x))
        put('RunePad', q, yaw); put('Seat', q + Vector((0, 0, PAD_H)), yaw)
    for b in L['braziers']: put('Brazier', Vector((b['positionM'][0], b['positionM'][1], 0)), math.degrees(math.atan2(-b['positionM'][1], -b['positionM'][0])))
    for b in L['banners']: put('Banner', Vector((b['positionM'][0], b['positionM'][1], 0)), math.degrees(math.atan2(-b['positionM'][1], -b['positionM'][0])))
    return out


FLOOR_SEGS = 64
FLOOR_TRIS = 4 * FLOOR_SEGS - 4   # the greybox sand ellipse stand-in (not a kit piece): two caps of 62 tris and 128 side tris


def make_materials():
    for m, c in MATS.items():
        mt = bpy.data.materials.new('M_Arena_' + m); mt.use_nodes = True; bsdf = mt.node_tree.nodes['Principled BSDF']
        bsdf.inputs['Base Color'].default_value = (*c, 1); bsdf.inputs['Roughness'].default_value = 0.9; mt.diffuse_color = (*c, 1)


def sha(path): return hashlib.sha256(open(path, 'rb').read()).hexdigest()


def export(ob, path):
    bpy.ops.object.select_all(action='DESELECT'); ob.select_set(True); bpy.context.view_layer.objects.active = ob
    bpy.ops.export_scene.fbx(filepath=path, use_selection=True, object_types={'MESH'}, apply_scale_options='FBX_SCALE_ALL',
                             mesh_smooth_type='FACE', bake_anim=False, path_mode='STRIP', embed_textures=False, use_mesh_modifiers=False)


# ================================================================ mesh checks (topology for build and metrics)
def topo(co, tr):
    # index topology as written in the FBX: closed (every edge in exactly two triangles), winding, components
    nF = len(tr); e = np.vstack([tr[:, [0, 1]], tr[:, [1, 2]], tr[:, [2, 0]]]); es = np.sort(e, axis=1)
    key = es[:, 0].astype(np.int64) * (len(co) + 1) + es[:, 1]
    uk, inv, cnt = np.unique(key, return_inverse=True, return_counts=True); inv = inv.ravel()
    dirsign = (e[:, 0] < e[:, 1]).astype(int); two = np.where(cnt == 2)[0]; wind = True
    if len(two):
        order = np.argsort(inv, kind='stable'); starts = np.searchsorted(inv[order], two)
        wind = bool((dirsign[order[starts]] != dirsign[order[starts + 1]]).all())
    parent = list(range(len(co)))

    def find(x):
        while parent[x] != x: parent[x] = parent[parent[x]]; x = parent[x]
        return x
    for f in tr:
        r0 = find(f[0])
        for v in f[1:]:
            r = find(v)
            if r != r0: parent[r] = r0
    roots = np.array([find(f[0]) for f in tr]); comps = [np.where(roots == r)[0] for r in np.unique(roots)]
    v0, v1, v2 = co[tr[:, 0]], co[tr[:, 1]], co[tr[:, 2]]; area = np.linalg.norm(np.cross(v1 - v0, v2 - v0), axis=1) / 2
    return {'closed': bool((cnt == 2).all()), 'open_edges': int((cnt != 2).sum()), 'winding': wind, 'degenerate': int((area < 1e-10).sum())}, comps


def support(co, tr, comps, eps=0.002):
    # a component is supported when it rests on z = 0 or touches (within eps, or inside) a supported component; the rest are floaters
    trees, verts = [], []
    for c in comps:
        f = tr[c]; vid = np.unique(f); remap = {v: i for i, v in enumerate(vid)}
        trees.append(BVHTree.FromPolygons([tuple(co[v]) for v in vid], [tuple(remap[v] for v in t) for t in f])); verts.append(co[vid])
    n = len(comps); touch = [[False] * n for _ in range(n)]
    for i in range(n):
        for j in range(n):
            if i == j: continue
            for v in verts[i]:
                hit = trees[j].find_nearest(Vector(v))
                if hit[0] is not None and (hit[3] <= eps or (Vector(v) - hit[0]).dot(hit[1]) < 0):
                    touch[i][j] = touch[j][i] = True; break
    ok = {i for i in range(n) if verts[i][:, 2].min() <= eps}; todo = list(ok)
    while todo:
        i = todo.pop()
        for j in range(n):
            if touch[i][j] and j not in ok: ok.add(j); todo.append(j)
    return n - len(ok)


def forward_check(name, co, tr, mats_of_face, mats):
    mn, mx = co.min(0), co.max(0)
    if name in ('WallStraight', 'WallCurved', 'GateArch'):
        return mx[0] <= 0.16 and (mn[0] + mx[0]) / 2 < 0 and (mx[1] - mn[1]) > (mx[0] - mn[0]), \
            f'arena face at x max {mx[0]:.3f} m, body to x min {mn[0]:.3f} m, length along Y {mx[1] - mn[1]:.3f} m'
    if name.startswith('Tier'):
        front = co[co[:, 0] > -0.3][:, 2].max(); back = co[co[:, 0] < mn[0] + 0.3][:, 2].max()
        return mx[0] <= 0.15 and abs(co[co[:, 0] > -0.3][:, 0].min()) < 0.001 and back > front, f'front edge at x 0 at its middle (x max {mx[0]:.3f} m at the curved ends), tread top {front:.3f} m at the front rising to {back:.3f} m at x {mn[0]:.3f} m (away from the arena)'
    if name == 'Banner':
        ci = mats.index('M_Arena_Cloth'); cx = co[tr[mats_of_face == ci]].reshape(-1, 3)[:, 0]
        return cx.min() > 0, f'cloth spans x {cx.min():.3f}..{cx.max():.3f} m, in front of the pole (+X)'
    if name == 'Brazier':
        ft = co[co[:, 2] < 0.035]   # the three foot pads
        return ft[:, 0].max() > -ft[:, 0].min() + 0.1, f'feet reach x {ft[:, 0].max():.3f} m on +X (one leg forward) and x {ft[:, 0].min():.3f} m behind'
    if name in ('Dais', 'Seat'):
        e = EXPECT[name]; return abs(mx[0] - mn[0] - e['x']) < 0.005 and abs(mx[1] - mn[1] - e['y']) < 0.005, \
            f'X extent {mx[0] - mn[0]:.3f} m (layout {e["x"]}), Y extent {mx[1] - mn[1]:.3f} m (layout {e["y"]})'
    return all(abs(mn[i] + mx[i]) < 0.002 for i in (0, 1)), 'radially symmetric, no facing: bbox centred on the origin'


def scale_check(name, co, tr, obscale):
    e = EXPECT[name]; mn, mx = co.min(0), co.max(0); d = mx - mn; got, ok = [], all(abs(s - 1) < 1e-4 for s in obscale)
    for k, i in (('x', 0), ('y', 1), ('z', 2)):
        if k in e: ok &= abs(d[i] - e[k]) <= max(0.005, 0.01 * e[k]); got.append(f'{k} {d[i]:.3f} (layout {e[k]:.3f})')
    if 'body_x' in e:   # the body, measured through the mesh at y = 0, z = 1.5 m (between the plinth and the string course)
        tree = BVHTree.FromPolygons([tuple(v) for v in co], [tuple(t) for t in tr])
        hi_ = tree.ray_cast(Vector((5, 0, 1.5)), Vector((-1, 0, 0)))[0]; lo_ = tree.ray_cast(Vector((-5, 0, 1.5)), Vector((1, 0, 0)))[0]
        t = hi_.x - lo_.x; ok &= abs(t - e['body_x']) < 0.002; got.append(f'body thickness {t:.3f} (layout {e["body_x"]})')
    if 'front_top' in e:
        ft = co[co[:, 0] > -0.3][:, 2].max(); ok &= abs(ft - e['front_top']) < 0.002; got.append(f'first tread {ft:.3f} (layout {e["front_top"]:.3f})')
    if 'bowl_r' in e:
        br = np.hypot(co[co[:, 2] > 0.6][:, 0], co[co[:, 2] > 0.6][:, 1]).max(); ok &= abs(br - e['bowl_r']) < 0.002; got.append(f'bowl radius {br:.3f} (layout {e["bowl_r"]})')
    return ok, ', '.join(got) + f', object scale {[round(s, 4) for s in obscale]}'


# ================================================================ build
if MODE == 'build':
    bpy.ops.wm.read_factory_settings(use_empty=True); make_materials(); os.makedirs(EXPORT, exist_ok=True)
    pieces = {}
    for name, (fn, replaces, pivot) in PIECES.items():
        p = Piece(); fn(p); ob = p.finish('SM_Arena_' + name); me = ob.data
        path = os.path.join(EXPORT, f'SM_Arena_{name}.fbx'); export(ob, path)
        co = np.array([v.co for v in me.vertices]); tr = np.array([tuple(f.vertices) for f in me.polygons], dtype=np.int64)
        pieces[name] = {'file': f'export/SM_Arena_{name}.fbx', 'tris': len(me.polygons), 'verts': len(me.vertices), 'components': len(topo(co, tr)[1]),
                        'boundsM': {'min': [round(float(x), 4) for x in co.min(0)], 'max': [round(float(x), 4) for x in co.max(0)]},
                        'materials': ['M_Arena_' + m for m in p.mats], 'pivot': pivot, 'replaces': replaces,
                        'sha256': sha(path), 'bytes': os.path.getsize(path)}
    asm = assembly(); count = {}
    for it in asm: count[it['piece']] = count.get(it['piece'], 0) + 1
    tris = sum(pieces[k]['tris'] * n for k, n in count.items())
    man = {'card': 'A07 / T42', 'generated': '2026-10-21',
           'tool': 'Blender 5.2.2 LTS (Windows ARM64), apps/vr/tools/assets/env_kit.py; hand-authored parametric geometry, no AI step, 0 credits',
           'layout': 'apps/vr/data/vr/arena-layout.json (read only)', 'units': 'metres, unit scale; origin at the base; +X forward; Z up',
           'materials': {('M_Arena_' + m): {'base_color_linear': c} for m, c in MATS.items()},
           'pieces': pieces,
           'assembly': {'axes': 'Unreal axes as in arena-layout.json (+X dais to centre, +Y the seated player\'s right, Z up); '
                                'yawDeg about +Z, Unreal sense (+X toward +Y); scale is the instance scale, Y = length along the chord',
                        'instanceCount': count, 'floorStandInTris': FLOOR_TRIS,
                        'trisKitInstances': tris, 'trisTotalWithFloor': tris + FLOOR_TRIS, 'triBudget': TRI_BUDGET,
                        'note': 'every wall, gate, tier, stair, dais, pad, seat, brazier and banner instance is counted; the floor is the '
                                'greybox sand ellipse (not a kit piece), counted as a 64-segment stand-in; flames, markers, threats and hands '
                                'are not part of the kit',
                        'instances': asm}}
    json.dump(man, open(os.path.join(KIT, 'manifest.json'), 'w'), indent=1)
    if WORK:
        os.makedirs(WORK, exist_ok=True); bpy.ops.wm.save_as_mainfile(filepath=os.path.join(WORK, 'env_kit.blend'))
    print('BUILD', json.dumps({k: v['tris'] for k, v in pieces.items()}), 'assembled', tris + FLOOR_TRIS)


# ================================================================ metrics (static profile, on the FBX files re-imported fresh)
if MODE == 'metrics':
    from io_scene_fbx import parse_fbx
    man = json.load(open(os.path.join(KIT, 'manifest.json'))); out = {'pieces': {}, 'kit': {}}; all_mats = set(); fbx_tris = {}
    for name in PIECES:
        path = os.path.join(EXPORT, f'SM_Arena_{name}.fbx')
        root, version = parse_fbx.parse(path); objs = next(e for e in root.elems if e.id == b'Objects'); kinds = {}
        for e in objs.elems:
            nm = e.props[1].split(b'\x00\x01')[0].decode() if len(e.props) > 1 and isinstance(e.props[1], bytes) else ''
            sub = e.props[2].decode() if len(e.props) > 2 and isinstance(e.props[2], bytes) else ''
            kinds.setdefault(f'{e.id.decode()}:{sub}', []).append(nm)
        stacks = sum(1 for e in objs.elems if e.id == b'AnimationStack')
        f_mats = kinds.get('Material:', []); textures = len(kinds.get('Texture:', [])) + len(kinds.get('Video:Clip', []))
        bpy.ops.wm.read_factory_settings(use_empty=True); bpy.ops.import_scene.fbx(filepath=path)
        sc = bpy.context.scene; meshes = [o for o in sc.objects if o.type == 'MESH']; arms = [o for o in sc.objects if o.type == 'ARMATURE']
        ob = meshes[0]; me = ob.data; me.calc_loop_triangles(); mw = np.array(ob.matrix_world)
        co = np.empty(len(me.vertices) * 3); me.vertices.foreach_get('co', co); co = co.reshape(-1, 3) @ mw[:3, :3].T + mw[:3, 3]
        tr = np.empty(len(me.loop_triangles) * 3, dtype=np.int64); me.loop_triangles.foreach_get('vertices', tr); tr = tr.reshape(-1, 3)
        mf = np.empty(len(me.loop_triangles), dtype=np.int64); me.loop_triangles.foreach_get('material_index', mf)
        mats = [m.name for m in me.materials]; all_mats |= set(mats)
        flat = []
        for m in me.materials:
            imgs = [n for n in m.node_tree.nodes if n.type == 'TEX_IMAGE'] if m.node_tree else []
            pbn = next((n for n in m.node_tree.nodes if n.type == 'BSDF_PRINCIPLED'), None) if m.node_tree else None
            flat.append({'name': m.name, 'image_nodes': len(imgs), 'base_color_linear': [round(x, 4) for x in pbn.inputs['Base Color'].default_value[:3]] if pbn else None})
        t, comps = topo(co, tr); floaters = support(co, tr, comps); mn, mx = co.min(0), co.max(0)
        fwd = forward_check(name, co, tr, mf, mats); scl = scale_check(name, co, tr, ob.matrix_world.to_scale())
        fbx_tris[name] = len(tr); m_p = man['pieces'][name]
        checks = [
            ('S1 one static mesh', len(meshes) == 1 and not arms and stacks == 0 and not kinds.get('Deformer:Skin'),
             f'{len(meshes)} mesh, {len(arms)} armature, {stacks} animation stacks'),
            ('S2 kit materials only, flat colour, no textures', set(mats) <= set(KIT_MATS) and textures == 0 and all(f['image_nodes'] == 0 for f in flat),
             f'{mats}; {textures} textures in the FBX'),
            ('S3 closed or intentionally open', (t['closed'] and t['winding']) or name in OPEN,
             f'{"closed" if t["closed"] else str(t["open_edges"]) + " open edges"}, winding {"consistent" if t["winding"] else "inconsistent"}' + (f'; open by intent: {OPEN[name]}' if name in OPEN else '')),
            ('S4 no floaters', floaters == 0, f'{len(comps)} closed part(s), {floaters} not resting on z = 0 or on a part that does'),
            ('S5 origin at the base', abs(mn[2]) <= 0.001 and mn[0] <= 0.001 and mx[0] >= -0.001 and mn[1] <= 0 <= mx[1],
             f'min z {mn[2]:.4f} m; origin inside the footprint x {mn[0]:.3f}..{mx[0]:.3f}, y {mn[1]:.3f}..{mx[1]:.3f}'),
            ('S6 +X forward', fwd[0], fwd[1]),
            ('S7 unit scale, dimensions against the layout', scl[0], scl[1]),
            ('S8 no degenerate triangles', t['degenerate'] == 0, f'{t["degenerate"]} degenerate'),
            ('S9 manifest matches the file', m_p['tris'] == len(tr) and m_p['verts'] == len(me.vertices) and sha(path) == m_p['sha256'],
             f'{len(tr)} tris, {len(me.vertices)} verts, sha256 {"matches" if sha(path) == m_p["sha256"] else "differs"}'),
        ]
        out['pieces'][name] = {'fbx': m_p['file'], 'fbx_version': version, 'bytes': os.path.getsize(path), 'tris': len(tr), 'verts': len(me.vertices),
                               'components': len(comps), 'floaters': floaters, 'closed': t['closed'], 'winding_consistent': t['winding'],
                               'boundsM': {'min': [round(float(x), 4) for x in mn], 'max': [round(float(x), 4) for x in mx]},
                               'fbx_materials': f_mats, 'materials': flat,
                               'checks': [{'check': c, 'pass': bool(p), 'value': v} for c, p, v in checks]}
    asm = man['assembly']; count = asm['instanceCount']
    total = sum(fbx_tris[k] * n for k, n in count.items()) + FLOOR_TRIS
    walls = count.get('WallStraight', 0) + count.get('WallCurved', 0) + 2 * count.get('GateArch', 0)
    tiers = count.get('TierStraight', 0) - count.get('TierStair', 0) + count.get('TierCurved', 0)
    kit = [
        ('K1 at most 6 materials in the whole kit, all from the shared set', len(all_mats) <= 6 and all_mats <= set(KIT_MATS), f'{len(all_mats)}: {sorted(all_mats)}'),
        ('K2 assembled arena within the triangle budget', total <= TRI_BUDGET and total == asm['trisTotalWithFloor'],
         f'{total} tris with every instance and the floor stand-in ({total - FLOOR_TRIS} kit + {FLOOR_TRIS} floor), budget {TRI_BUDGET}; manifest says {asm["trisTotalWithFloor"]}'),
        ('K3 every piece re-opened from its FBX', len(fbx_tris) == len(PIECES), f'{len(fbx_tris)} of {len(PIECES)}'),
        ('K4 the assembly covers the layout', walls == SEGS and tiers == SEGS - 2 and count.get('RunePad') == len(PADS['items']) == count.get('Seat')
         and count.get('Dais') == 1 and count.get('Brazier') == len(L['braziers']) and count.get('Banner') == len(L['banners']),
         f'{walls} of {SEGS} wall chords (gate = 2), stands behind {tiers} chords (not behind the gate), {count.get("RunePad")} pads, {count.get("Seat")} seats, '
         f'{count.get("Dais")} dais, {count.get("Brazier")} braziers, {count.get("Banner")} banners'),
    ]
    out['kit'] = {'assembledTris': total, 'triBudget': TRI_BUDGET, 'materials': sorted(all_mats), 'instanceCount': count,
                  'checks': [{'check': c, 'pass': bool(p), 'value': v} for c, p, v in kit]}
    allc = [c for p in out['pieces'].values() for c in p['checks']] + out['kit']['checks']
    out['profile'] = 'static environment kit (env_kit.py): S1-S9 per piece, K1-K4 for the kit'
    out['pass'] = all(c['pass'] for c in allc)
    json.dump(out, open(os.path.join(KIT, 'metrics.json'), 'w'), indent=1)
    for n, p in out['pieces'].items():
        for c in p['checks']: print('CHECK', 'PASS' if c['pass'] else 'FAIL', n, c['check'], '|', c['value'])
    for c in out['kit']['checks']: print('CHECK', 'PASS' if c['pass'] else 'FAIL', 'kit', c['check'], '|', c['value'])
    print('METRICS', 'PASS' if out['pass'] else 'FAIL', total)


# ================================================================ render (from the FBX files, not the build scene)
if MODE == 'render':
    man = json.load(open(os.path.join(KIT, 'manifest.json')))
    bpy.ops.wm.read_factory_settings(use_empty=True); s = bpy.context.scene
    s.render.engine = 'BLENDER_EEVEE'; s.view_settings.view_transform = 'Standard'
    try: s.eevee.taa_render_samples = 24
    except Exception: pass
    src = {}
    for name in PIECES:
        before = set(bpy.data.objects); bpy.ops.import_scene.fbx(filepath=os.path.join(EXPORT, f'SM_Arena_{name}.fbx'))
        ob = next(o for o in set(bpy.data.objects) - before if o.type == 'MESH'); src[name] = ob
    # one material per name: the importer makes M_Arena_Stone.001 for a repeat, so point every slot at the first
    for ob in src.values():
        for sl in ob.material_slots:
            base = sl.material.name.split('.')[0]; sl.material = bpy.data.materials[base]
    for m in bpy.data.materials:
        if m.node_tree:
            pbn = next((n for n in m.node_tree.nodes if n.type == 'BSDF_PRINCIPLED'), None)
            if pbn: pbn.inputs['Roughness'].default_value = 0.9; pbn.inputs['Specular IOR Level'].default_value = 0.2
    cam = bpy.data.objects.new('cam', bpy.data.cameras.new('cam')); s.collection.objects.link(cam); s.camera = cam
    s.render.image_settings.file_format = 'PNG'; s.render.resolution_percentage = 100
    w_ = bpy.data.worlds.new('w'); s.world = w_; w_.use_nodes = True; bg = w_.node_tree.nodes['Background']
    suns = []

    def sun(d, e, col=(1, 1, 1)):
        Ls = bpy.data.objects.new('l', bpy.data.lights.new('l', 'SUN')); Ls.data.energy = e; Ls.data.color = col; s.collection.objects.link(Ls)
        Ls.rotation_euler = (-Vector(d)).to_track_quat('-Z', 'Y').to_euler(); suns.append(Ls)

    def aim(loc, target): cam.location = loc; cam.rotation_euler = (Vector(target) - Vector(loc)).to_track_quat('-Z', 'Y').to_euler()

    # -- lineup: the big pieces in the back row, the props in front, every piece once, origins on z = 0
    bg.inputs[0].default_value = (0.30, 0.30, 0.32, 1)
    for d, e in [((3, -2, 3), 3.0), ((-1, 3, 1.5), 1.5), ((-3, 0, 2), 1.0)]: sun(d, e)
    back = ['WallStraight', 'WallCurved', 'GateArch', 'TierStraight', 'TierCurved', 'TierStair', 'Dais']; front = ['RunePad', 'Seat', 'Brazier', 'Banner']
    y = 0.0; lin = []
    for row, x0, gap in ((back, 0.0, 1.2), (front, 6.5, 1.0)):
        y = 0.0
        for name in row:
            ob = src[name]; bb = [Vector(c) for c in ob.bound_box]; w = max(c.y for c in bb) - min(c.y for c in bb)
            ob.location = (x0, -(y + w / 2) - (max(c.y for c in bb) + min(c.y for c in bb)) / 2, 0); y += w + gap; lin.append(ob)
    bpy.context.view_layer.update()
    pts = [ob.matrix_world @ Vector(c) for ob in lin for c in ob.bound_box]; cx = sum(p.x for p in pts) / len(pts); cy = sum(p.y for p in pts) / len(pts)
    cam.data.type = 'ORTHO'; cam.data.ortho_scale = 50
    aim(Vector((cx + 30, cy - 14, 18)), (cx, cy, 1.6))
    s.render.resolution_x, s.render.resolution_y = 1600, 900; s.render.filepath = os.path.join(KIT, 'kit_lineup.png'); bpy.ops.render.render(write_still=True)
    for ob in lin: ob.hide_render = True

    # -- assembled arena, from the seated eye: 1.2 m above the centre pad top, facing the arena centre, 90 degree horizontal FOV
    for l_ in suns: bpy.data.objects.remove(l_)
    lt = L['lighting']; bg.inputs[0].default_value = tuple(0.5 * h + 0.5 * k for h, k in zip(lt['skyHorizon'], lt['skyLightColour'])) + (1,)
    # evidence lighting, not the UE look: the layout's warm colour, but from behind the dais and 25 degrees up so the faces read
    sun((-0.9, 0.2, 0.45), 2.2, tuple(lt['directionalColour']))
    sun((0.2, 0.3, 1.0), 1.4, tuple(lt['skyLightColour']))
    fl = bpy.data.meshes.new('floor'); fo = bpy.data.objects.new('SandFloorStandIn', fl); s.collection.objects.link(fo)
    bmf = bmesh.new(); ring_t = [bmf.verts.new((A_ * math.cos(2 * math.pi * i / FLOOR_SEGS), B_ * math.sin(2 * math.pi * i / FLOOR_SEGS), 0)) for i in range(FLOOR_SEGS)]
    ring_b = [bmf.verts.new((v.co.x, v.co.y, -L['floor']['thicknessM'])) for v in ring_t]
    bmf.faces.new(ring_t); bmf.faces.new(ring_b[::-1])
    for i in range(FLOOR_SEGS): bmf.faces.new((ring_t[i], ring_b[i], ring_b[(i + 1) % FLOOR_SEGS], ring_t[(i + 1) % FLOOR_SEGS]))
    bmesh.ops.triangulate(bmf, faces=bmf.faces); bmf.to_mesh(fl); bmf.free(); fl.materials.append(bpy.data.materials['M_Arena_Sand'])
    assert len(fl.polygons) == FLOOR_TRIS
    for it in man['assembly']['instances']:
        o = bpy.data.objects.new(it['piece'], src[it['piece']].data); s.collection.objects.link(o)
        x, y_, z = it['locationM']; o.location = (x, -y_, z); o.rotation_euler = (0, 0, -math.radians(it['yawDeg'])); o.scale = it['scale']
    centre = next(p for p in PADS['items'] if p['id'] == 'centre')['positionM']
    eye = Vector((centre[0], -centre[1], centre[2] + L['seatedEyeHeightAbovePadM'])); tgt = L['lookTargetM']
    cam.data.type = 'PERSP'; cam.data.sensor_fit = 'HORIZONTAL'; cam.data.angle = math.radians(L['cameraFovDeg']); cam.data.clip_end = 200
    aim(eye, (tgt[0], -tgt[1], tgt[2]))
    s.render.resolution_x, s.render.resolution_y = 640, 480; s.render.filepath = os.path.join(KIT, 'arena_seated_640x480.png'); bpy.ops.render.render(write_still=True)
    # an overview from above the far stands, for the reader (not a gate view)
    cam.data.angle = math.radians(70); aim(Vector((24, -20, 16)), (-2, 0, 0))
    s.render.resolution_x, s.render.resolution_y = 1280, 720; s.render.filepath = os.path.join(KIT, 'arena_overview.png'); bpy.ops.render.render(write_still=True)
    print('RENDER', sorted(f for f in os.listdir(KIT) if f.endswith('.png')), 'eye', [round(v, 3) for v in eye])
