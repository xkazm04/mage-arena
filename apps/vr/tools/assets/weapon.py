# T15: hand-authored weapon for a rigged Bout 2 soldier. Parametric Blender geometry, no generation, no external service.
#   spear (conscript): short spear (1.45 m: the idle tip stays under 1.8 m so metrics check 8 holds), wooden shaft, iron socket, leaf head and butt cap, in the Thrust attack
#   sling (slinger):   two cords and a leather pouch, in the Sling attack
# The weapon is built in armature space around the grip point of hand_r, joined into the one skinned mesh (so the FBX keeps one
# mesh and one skin), every weapon vertex weighted 100% to hand_r, and made of the creature's own materials (flat colours, no textures).
# A rigid weapon on one bone cannot follow the arm by itself: at every Attack key the hand_r key is re-solved so that the weapon axis
# points where the strike needs it (WRIST, desired world direction per key frame; +X forward, left +Y, Z up). Idle, Hit and Death
# are untouched, so the weapon rests along its Idle direction there.
#   blender -b -P weapon.py -- <name_rigged.blend> <outdir> <name> <fbx path> <spear|sling>
# Writes <outdir>/<name>_armed.blend and <outdir>/<name>_weapon.json, and exports the FBX with the same options as rig_anim.py.
import bpy, sys, json, math, bmesh
from mathutils import Vector, Quaternion

a = sys.argv[sys.argv.index('--') + 1:]; blend, out, name, fbx, kind = a[:5]
bpy.ops.wm.open_mainfile(filepath=blend)
arm = bpy.data.objects['Armature']; ob = bpy.data.objects['SM_' + name]
hb = arm.data.bones['hand_r']
G = hb.head_local + (hb.tail_local - hb.head_local) * 0.5  # the fist
N = lambda *v: Vector(v).normalized()
Y = Vector((0, 1, 0))

# weapon kind: idle axis (from the fist toward the business end), materials, wrist targets per Attack key frame
KINDS = {
    'spear': {'axis': N(0.34, 0, 1), 'mats': ('Wood', 'Iron'), 'wrist': {7: N(0.95, 0.15, 0.25), 12: N(0.98, 0.05, 0.12), 18: N(0.98, 0.05, 0.12)}},
    'sling': {'axis': N(0.12, 0, -1), 'mats': ('Linen', 'Leather'),
              'wrist': {9: N(-0.7, -0.3, -0.6), 17: N(0.0, -0.25, 0.95), 24: N(0.85, -0.1, 0.5), 32: N(0.5, -0.2, -0.85)}},
}
K = KINDS[kind]; AX = K['axis']
M = {m: bpy.data.materials[f'M_{name}_{m}'] for m in K['mats']}
bm = bmesh.new(); mat_slots = [M[m] for m in K['mats']]


def frame(ax):
    u = ax.cross(Y if abs(ax.dot(Y)) < 0.9 else Vector((1, 0, 0))).normalized(); return u, ax.cross(u).normalized()


def loft(specs, mat, n=8, apex0=None, apex1=None):
    # specs: (centre, rx, ry) rings along AX; an end is an apex point (pointed) or a flat cap
    u, v = frame(AX); rings = []
    for c, rx, ry in specs:
        rings.append([bm.verts.new(c + u * rx * math.cos(2 * math.pi * i / n) + v * ry * math.sin(2 * math.pi * i / n)) for i in range(n)])
    mi = mat_slots.index(M[mat]); fs = []
    for r0, r1 in zip(rings, rings[1:]):
        for i in range(n): fs.append(bm.faces.new((r0[i], r0[(i + 1) % n], r1[(i + 1) % n], r1[i])))
    for ring, ap in ((rings[0], apex0), (rings[-1], apex1)):
        if ap is None: fs.append(bm.faces.new(ring))
        else:
            p = bm.verts.new(ap)
            for i in range(n): fs.append(bm.faces.new((ring[i], ring[(i + 1) % n], p)))
    for f in fs: f.material_index = mi
    return fs


def tube(p0, p1, r, mat, n=6):
    global AX
    keep = AX; AX = (p1 - p0).normalized(); loft([(p0, r, r), (p1, r, r)], mat, n); AX = keep


def at(t): return G + AX * t


if kind == 'spear':
    loft([(at(-0.40), 0.016, 0.016), (at(0.62), 0.016, 0.016)], 'Wood')                  # shaft
    loft([(at(-0.44), 0.020, 0.020), (at(-0.40), 0.020, 0.020)], 'Iron', apex0=at(-0.47))  # butt cap
    loft([(at(0.58), 0.024, 0.024), (at(0.70), 0.020, 0.020)], 'Iron')                   # socket
    loft([(at(0.68), 0.020, 0.007), (at(0.76), 0.036, 0.010), (at(0.82), 0.030, 0.008)], 'Iron', n=4, apex1=at(0.98))  # leaf head
else:
    L = 0.54; c = at(L); u, v = frame(AX)  # pouch centre; u is the width axis (cords attach at its two ends), v the thickness
    w = Y if abs(AX.dot(Y)) < 0.9 else u
    for s_ in (-1, 1):
        tube(G + w * s_ * 0.014, c - AX * 0.035 + w * s_ * 0.06, 0.012, 'Linen')
    loft([(G - AX * 0.02, 0.026, 0.026), (G + AX * 0.02, 0.026, 0.026)], 'Linen', n=6)  # the knot in the fist
    rings = [(-0.060, 0.0), (-0.040, 0.7), (0.0, 1.0), (0.035, 0.8)]
    loft([(c + AX * t, 0.07 * k, 0.03 * k) for t, k in rings[1:]], 'Leather', n=8, apex0=c + AX * rings[0][0], apex1=c + AX * 0.065)

bm.normal_update(); bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
bmesh.ops.triangulate(bm, faces=bm.faces)
wm = bpy.data.meshes.new('W_' + kind); bm.to_mesh(wm); bm.free()
w_tris = len(wm.polygons)
for m_ in mat_slots: wm.materials.append(m_)
wo = bpy.data.objects.new('W_' + kind, wm); bpy.context.scene.collection.objects.link(wo)
wo.vertex_groups.new(name='hand_r').add(list(range(len(wm.vertices))), 1.0, 'REPLACE')
for p in wm.polygons: p.use_smooth = False
bpy.ops.object.select_all(action='DESELECT'); wo.select_set(True); ob.select_set(True); bpy.context.view_layer.objects.active = ob
bpy.ops.object.join()

# ---- wrist: re-solve hand_r at each Attack key so the weapon axis points at its target
sc = bpy.context.scene; ad = arm.animation_data; pb = arm.pose.bones['hand_r']
for t in ad.nla_tracks: t.mute = True
act = bpy.data.actions['Attack']; ad.action = act
if hasattr(ad, 'action_slot') and ad.action_slot is None and len(act.slots): ad.action_slot = act.slots[0]
wl = hb.matrix_local.to_3x3().inverted() @ AX; info = {}
pb.rotation_mode = 'QUATERNION'; prev = Quaternion()
for fr, D in sorted(K['wrist'].items()):
    sc.frame_set(fr); bpy.context.view_layer.update()
    R0 = pb.matrix.to_3x3().normalized(); qb = pb.rotation_quaternion.copy()
    P = R0 @ qb.to_matrix().inverted(); cur = R0 @ wl
    Rc = cur.rotation_difference(D).to_matrix()
    q = (P.inverted() @ Rc @ R0).to_quaternion(); q.make_compatible(prev); prev = q
    pb.rotation_quaternion = q; pb.keyframe_insert('rotation_quaternion', frame=fr)
    info[fr] = {'wrist_deg': round(math.degrees(2 * math.acos(min(1, abs(q.w)))), 1), 'was_deg_off_target': round(math.degrees(cur.angle(D)), 1)}
for t in ad.nla_tracks: t.mute = False
ad.action = None
pb.rotation_quaternion = Quaternion()

bpy.ops.wm.save_as_mainfile(filepath=f'{out}/{name}_armed.blend')
bpy.ops.object.select_all(action='DESELECT'); arm.select_set(True); ob.select_set(True); bpy.context.view_layer.objects.active = arm
bpy.ops.export_scene.fbx(filepath=fbx, use_selection=True, object_types={'ARMATURE', 'MESH'}, apply_scale_options='FBX_SCALE_ALL',
                         mesh_smooth_type='FACE', add_leaf_bones=False, armature_nodetype='NULL', bake_anim=True,
                         bake_anim_use_all_bones=True, bake_anim_use_nla_strips=True, bake_anim_use_all_actions=False,
                         bake_anim_force_startend_keying=True, bake_anim_step=1.0, bake_anim_simplify_factor=0.0,
                         path_mode='STRIP', embed_textures=False)
R = {'weapon': kind, 'weapon_tris': w_tris, 'grip': [round(x, 3) for x in G], 'idle_axis': [round(x, 3) for x in AX], 'wrist': info, 'fbx': fbx,
     'authoring': 'hand-authored parametric geometry (bmesh) in Blender; no generation, no Leonardo, no TRELLIS, no ComfyUI'}
json.dump(R, open(f'{out}/{name}_weapon.json', 'w'), indent=1); print('WEAPON', json.dumps(R))
