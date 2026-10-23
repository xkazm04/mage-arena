# T38: score the exported FBX itself (not the scene that wrote it) and gate it against the T38 Wolf pre-import checks.
# Mesh metrics adapted from lynx-bench pof3d/round2/characters/work/pofmetrics2.py + met.py (vertices welded by exact
# position, components by shared edges, watertight / winding). The gate checks of gate.cjs come from pof's compiled
# libraries, which are not copied here; the checks below are the T38 card's, written out.
#   blender -b -P metrics.py -- <in.fbx> <metrics.json> [<attack clip name, default Cast>] [<profile, default Humanoid>]
# Profiles: Humanoid (the T38 checks, 1.8 m mannequin) | CinderHound | MireMaw (the Bout 2 beasts: their own scale target
# from the A04 sheets, +-10 %, their tri budget, +X forward read from their own bones, and no floaters).
import bpy, sys, json, os, numpy as np

a = sys.argv[sys.argv.index('--') + 1:]; fbx, outp = a[:2]
CLIPS = ['Idle', a[2] if len(a) > 2 else 'Cast', 'Hit', 'Death']
TRI_BUDGET_LOD0 = (18000, 30000)
PROFILE = a[3] if len(a) > 3 else 'Humanoid'
# height target (m, sheet 03 pixel scale, see finish.py), LOD0 tri budget, a left and a right bone, a bone that points forward
BEASTS = {'CinderHound': {'height': 1.36, 'tris': 12000, 'side': ('front_paw_l', 'front_paw_r'), 'forward': 'head',
                          'budget_note': 'three hounds on screen at once'},
          'MireMaw': {'height': 1.25, 'tris': 20000, 'side': ('foot_l', 'foot_r'), 'forward': 'jaw', 'budget_note': 'one maw'}}
BEAST = BEASTS.get(PROFILE)


def compute(V, F):
    nF = len(F)
    e = np.vstack([F[:, [0, 1]], F[:, [1, 2]], F[:, [2, 0]]]); es = np.sort(e, axis=1)
    key = es[:, 0].astype(np.int64) * (len(V) + 1) + es[:, 1]
    uk, inv, cnt = np.unique(key, return_inverse=True, return_counts=True)
    watertight = bool((cnt == 2).all())
    fi = np.tile(np.arange(nF), 3); dirsign = (e[:, 0] < e[:, 1]).astype(int); wind = True
    two = np.where(cnt == 2)[0]
    if len(two):
        order = np.argsort(inv, kind='stable'); inv_s = inv[order]; starts = np.searchsorted(inv_s, two)
        wind = bool((dirsign[order[starts]] != dirsign[order[starts + 1]]).all())
    parent = np.arange(nF)

    def find(x):
        while parent[x] != x:
            parent[x] = parent[parent[x]]; x = parent[x]
        return x
    order = np.argsort(inv, kind='stable'); inv_s = inv[order]
    for g in np.split(order, np.flatnonzero(np.diff(inv_s)) + 1):
        if len(g) >= 2:
            f0 = fi[g[0]]
            for x in g[1:]:
                r1 = find(f0); r2 = find(fi[x])
                if r1 != r2: parent[r2] = r1
    roots = np.array([find(i) for i in range(nF)]); _, counts = np.unique(roots, return_counts=True)
    comp = sorted(counts.tolist(), reverse=True)
    v0, v1, v2 = V[F[:, 0]], V[F[:, 1]], V[F[:, 2]]; ar = np.linalg.norm(np.cross(v1 - v0, v2 - v0), axis=1) / 2
    return dict(verts=int(len(np.unique(F))), tris=int(nF), watertight=watertight, windingConsistent=wind, components=len(comp),
                floaters=sum(1 for c in comp if c < 0.01 * nF), componentFaces=comp[:16], degenerateFaces=int((ar < 1e-12).sum()),
                euler=int(len(np.unique(F)) - len(uk) + nF))


# ---- the FBX file as written: objects, deformers, materials, textures, animation stacks
from io_scene_fbx import parse_fbx
root, version = parse_fbx.parse(fbx)
objs = next(e for e in root.elems if e.id == b'Objects')
kinds = {}
stacks = []
KTIME = 46186158000
for e in objs.elems:
    nm = e.props[1].split(b'\x00\x01')[0].decode() if len(e.props) > 1 and isinstance(e.props[1], bytes) else ''
    sub = e.props[2].decode() if len(e.props) > 2 and isinstance(e.props[2], bytes) else ''
    kinds.setdefault(f'{e.id.decode()}:{sub}', []).append(nm)
    if e.id == b'AnimationStack':
        p70 = next((x for x in e.elems if x.id == b'Properties70'), None); t = {}
        for p in (p70.elems if p70 else []):
            if p.props[0] in (b'LocalStart', b'LocalStop'): t[p.props[0].decode()] = p.props[4]
        stacks.append({'name': nm, 'start_s': t.get('LocalStart', 0) / KTIME, 'stop_s': t.get('LocalStop', 0) / KTIME})
fbx_file = {'version': version, 'bytes': os.path.getsize(fbx),
            'limb_nodes': len(kinds.get('Model:LimbNode', [])), 'mesh_models': kinds.get('Model:Mesh', []),
            'skin_deformers': len(kinds.get('Deformer:Skin', [])), 'materials': kinds.get('Material:', []),
            'textures': len(kinds.get('Texture:', [])) + len(kinds.get('Video:Clip', [])), 'anim_stacks': stacks}

# ---- the FBX imported fresh into Blender
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=fbx)
sc = bpy.context.scene
arms = [o for o in sc.objects if o.type == 'ARMATURE']; meshes = [o for o in sc.objects if o.type == 'MESH']
skinned = [o for o in meshes if any(m.type == 'ARMATURE' and m.object for m in o.modifiers) and o.vertex_groups]
ob = skinned[0] if skinned else meshes[0]; arm = arms[0] if arms else None
me = ob.data; me.calc_loop_triangles()
mw = np.array(ob.matrix_world)
co = np.empty(len(me.vertices) * 3); me.vertices.foreach_get('co', co); co = co.reshape(-1, 3) @ mw[:3, :3].T + mw[:3, 3]
tr = np.empty(len(me.loop_triangles) * 3, dtype=np.int64); me.loop_triangles.foreach_get('vertices', tr); tr = tr.reshape(-1, 3)
_, inv = np.unique(co.astype(np.float32), axis=0, return_inverse=True); inv = inv.ravel()
Vm = np.zeros((inv.max() + 1, 3)); Vm[inv] = co
mesh = compute(Vm, inv[tr])
mn, mx = co.min(0), co.max(0)
mesh.update(height_m=round(float(mx[2] - mn[2]), 4), bbox_m=[round(float(x), 4) for x in mx - mn],
            min_z_m=round(float(mn[2]), 4), centre_xy_m=[round(float((mn[i] + mx[i]) / 2), 4) for i in (0, 1)])
# weights
gn = {g.index: g.name for g in ob.vertex_groups}; bones = set(arm.data.bones.keys()) if arm else set()
sums = np.zeros(len(me.vertices)); infl = np.zeros(len(me.vertices), dtype=int)
for v in me.vertices:
    for g in v.groups:
        if gn[g.group] in bones and g.weight > 0: sums[v.index] += g.weight; infl[v.index] += 1
weights = {'unweighted_verts': int((sums < 1e-6).sum()), 'max_abs_sum_error': round(float(np.abs(sums - 1).max()), 6),
           'max_influences': int(infl.max()), 'normalised': bool(np.abs(sums - 1).max() < 1e-3)}
# skeleton and facing: +X forward means the left side is +Y and the toes point +X
skel = {}
if arm and BEAST:
    B = arm.data.bones; Mw = arm.matrix_world
    pos = lambda n, end='head': Mw @ (B[n].head_local if end == 'head' else B[n].tail_local)
    l_, r_ = BEAST['side']; fw = BEAST['forward']
    skel = {'bones': len(B), 'names': list(B.keys()), 'left_y': round(pos(l_).y, 3), 'right_y': round(pos(r_).y, 3),
            'forward_dx': round((pos(fw, 'tail') - pos(fw)).x, 3), 'root_head': [round(x, 3) for x in pos('root')]}
elif arm:
    B = arm.data.bones; Mw = arm.matrix_world
    pos = lambda n, end='head': Mw @ (B[n].head_local if end == 'head' else B[n].tail_local)
    skel = {'bones': len(B), 'names': list(B.keys()),
            'hand_l_y': round(pos('hand_l').y, 3), 'hand_r_y': round(pos('hand_r').y, 3),
            'toe_dx_l': round((pos('ball_l', 'tail') - pos('foot_l')).x, 3), 'root_head': [round(x, 3) for x in pos('root')]}
# clips: actions of the import, keyed motion per clip
clips = {}
for act in bpy.data.actions:
    nm = act.name.split('|')[-1]
    fcs = [fc for l in act.layers for s in l.strips for cb in s.channelbags for fc in cb.fcurves] if hasattr(act, 'layers') else list(act.fcurves)
    motion = 0.0
    for fc in fcs:
        vals = [k.co[1] for k in fc.keyframe_points]
        if vals: motion = max(motion, max(vals) - min(vals))
    f0, f1 = act.frame_range
    clips[nm] = {'action': act.name, 'frames': int(round(f1 - f0)) + 1, 'fcurves': len(fcs), 'max_channel_range': round(motion, 4)}
mats = []
for m in me.materials:
    imgs = [n for n in (m.node_tree.nodes if m and m.node_tree else []) if n.type == 'TEX_IMAGE']
    bc = None
    if m and m.node_tree:
        pbn = next((n for n in m.node_tree.nodes if n.type == 'BSDF_PRINCIPLED'), None)
        if pbn: bc = [round(x, 4) for x in pbn.inputs['Base Color'].default_value[:3]]
    mats.append({'name': m.name if m else None, 'base_color_linear': bc, 'image_nodes': len(imgs)})

stack_names = [s['name'] for s in stacks]
fps = sc.render.fps / sc.render.fps_base
checks = [
    ('1 one skeleton', len(arms) == 1 and fbx_file['skin_deformers'] == 1, f'{len(arms)} armature, {fbx_file["skin_deformers"]} skin deformer'),
    ('2 one skinned mesh', len(meshes) == 1 and len(skinned) == 1, f'{len(meshes)} mesh, {len(skinned)} skinned'),
    ('3 exactly the four clips, none empty', sorted(stack_names) == sorted(CLIPS) and all(c in clips and clips[c]['max_channel_range'] > 1e-3 for c in CLIPS),
     f'stacks {stack_names}; frames ' + ', '.join(f'{c} {clips.get(c, {}).get("frames")}' for c in CLIPS)),
    ('4 LOD0 triangle count recorded', TRI_BUDGET_LOD0[0] <= mesh['tris'] <= TRI_BUDGET_LOD0[1], f'{mesh["tris"]} tris (target {TRI_BUDGET_LOD0[0]}-{TRI_BUDGET_LOD0[1]}, scene budget 350000)'),
    ('5 one flat colour per material, no embedded textures', fbx_file['textures'] == 0 and all(m['image_nodes'] == 0 for m in mats), f'{len(mats)} materials, {fbx_file["textures"]} textures'),
    ('6 origin at the feet', abs(mesh['min_z_m']) < 0.01 and all(abs(c) < 0.15 for c in mesh['centre_xy_m']), f'min z {mesh["min_z_m"]} m, bbox centre xy {mesh["centre_xy_m"]} m'),
    ('7 +X forward', bool(skel) and skel.get('hand_l_y', 0) > 0 > skel.get('hand_r_y', 0) and skel.get('toe_dx_l', 0) > 0, f'hand_l y {skel.get("hand_l_y")}, hand_r y {skel.get("hand_r_y")}, toe dx {skel.get("toe_dx_l")}'),
    ('8 unit scale, height 1.62-1.98 m', 1.62 <= mesh['height_m'] <= 1.98 and all(abs(s - 1) < 1e-4 for o in (ob, arm) if o for s in o.matrix_world.to_scale()), f'height {mesh["height_m"]} m, object scale {[round(s, 4) for s in ob.matrix_world.to_scale()]}'),
    ('9 weights normalised', weights['normalised'] and weights['unweighted_verts'] == 0, f'max |sum-1| {weights["max_abs_sum_error"]}, unweighted {weights["unweighted_verts"]}, max influences {weights["max_influences"]}'),
]
if BEAST:
    lo_, hi_ = 0.9 * BEAST['height'], 1.1 * BEAST['height']; l_, r_ = BEAST['side']
    checks[3] = ('4 LOD0 triangle count within budget', 0 < mesh['tris'] <= BEAST['tris'],
                 f'{mesh["tris"]} tris (budget {BEAST["tris"]}, {BEAST["budget_note"]})')
    checks[6] = ('7 +X forward', bool(skel) and skel['left_y'] > 0 > skel['right_y'] and skel['forward_dx'] > 0,
                 f'{l_} y {skel.get("left_y")}, {r_} y {skel.get("right_y")}, {BEAST["forward"]} dx {skel.get("forward_dx")}')
    checks[7] = (f'8 unit scale, height {BEAST["height"]} m +-10 % ({lo_:.3f}-{hi_:.3f} m)',
                 lo_ <= mesh['height_m'] <= hi_ and all(abs(s - 1) < 1e-4 for o in (ob, arm) if o for s in o.matrix_world.to_scale()),
                 f'height {mesh["height_m"]} m ({100 * (mesh["height_m"] / BEAST["height"] - 1):+.1f} %), object scale {[round(s, 4) for s in ob.matrix_world.to_scale()]}')
    checks.append(('10 no floaters', mesh['components'] == 1 and mesh['floaters'] == 0,
                   f'{mesh["components"]} component(s), {mesh["floaters"]} floaters'))
M = {'fbx': fbx, 'fbx_file': fbx_file, 'mesh': mesh, 'skeleton': skel, 'weights': weights, 'clips': clips, 'fps': fps, 'materials': mats,
     'checks': [{'check': c, 'pass': bool(p), 'value': v} for c, p, v in checks]}
json.dump(M, open(outp, 'w'), indent=1)
for c in M['checks']: print('CHECK', 'PASS' if c['pass'] else 'FAIL', c['check'], '|', c['value'])
print('METRICS', json.dumps({k: M['mesh'][k] for k in ('verts', 'tris', 'components', 'floaters', 'height_m', 'watertight')}))
