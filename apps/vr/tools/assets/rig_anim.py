# T38 Brennic: rig the finished LOD0 to UE5 mannequin bone chains, author Idle / Cast / Hit / Death, export one FBX.
# Rig adapted from lynx-bench pof3d/round2/characters/work/rig.py: the armature is fitted to the mesh, heat weights are
# computed on a watertight voxel proxy and transferred to the real mesh by nearest proxy vertex, then smoothed, limited
# to 4 influences and normalised; nearest-bone fallback for anything the heat left unweighted.
# The mesh comes from finish.py facing -Y. Everything here is authored in that character frame and mapped through C
# (+90 degrees about Z) so the export faces +X with Z up, origin at the feet, metres (FBX_SCALE_ALL -> UE centimetres).
# Clips sit on NLA strips named exactly Idle, Cast, Hit, Death; the FBX exporter names each take after its strip.
#   blender -b -P rig_anim.py -- <finish.blend> <outdir> <name> <fbx path>
import bpy, sys, json, time, math, numpy as np
from mathutils import Vector, Quaternion, Matrix, kdtree

a = sys.argv[sys.argv.index('--') + 1:]; blend, out, name, fbx = a[:4]
t0 = time.time(); R = {}
FPS = 30
C = Matrix.Rotation(math.radians(90), 4, 'Z'); C3 = C.to_3x3(); Ci3 = C3.inverted()
bpy.ops.wm.open_mainfile(filepath=blend)
ob = bpy.data.objects['SM_' + name]
for o in list(bpy.data.objects):
    if o is not ob: bpy.data.objects.remove(o, do_unlink=True)
bpy.context.view_layer.objects.active = ob
me = ob.data
co = np.empty(len(me.vertices) * 3); me.vertices.foreach_get('co', co); co = co.reshape(-1, 3)
H = float(co[:, 2].max() - co[:, 2].min()); R['height_m'] = round(H, 4)

# ---- fit, in the character frame (front -Y, left +X); proportions from the concept, depths and limbs measured
z = co[:, 2]
arms = co[(z > 0.40 * H) & (z < 0.80 * H) & (np.abs(co[:, 0]) > 0.36)]
ys = float(np.median(arms[:, 1])) if len(arms) > 50 else float(np.median(co[:, 1]))
yh = float(np.median(co[z > 0.90 * H][:, 1]))
shin = co[(z > 0.10 * H) & (z < 0.25 * H)]
yl = float(np.median(shin[:, 1]))
xf = {1: float(shin[shin[:, 0] > 0][:, 0].mean()), -1: float(shin[shin[:, 0] < 0][:, 0].mean())}
zone = co[(z > 0.40 * H) & (z < 0.80 * H)]
tip = {}
for sg in (1, -1):
    side = zone[zone[:, 0] * sg > 0]; i = int(np.argmax(side[:, 0] * sg)); tip[sg] = Vector(side[i])
toe_y = float(co[z < 0.04 * H][:, 1].min())
f = lambda k: k * H
SH_Z, SH_X, HIP_Z, HIP_X, KNEE_Z, ANKLE_Z = f(0.73), 0.21, f(0.46), 0.11, f(0.29), f(0.075)
R['fit'] = {'spine_y': round(ys, 3), 'head_y': round(yh, 3), 'leg_y': round(yl, 3), 'shin_x': [round(xf[1], 3), round(xf[-1], 3)],
            'hand_tip_l': [round(x, 3) for x in tip[1]], 'hand_tip_r': [round(x, 3) for x in tip[-1]], 'toe_y': round(toe_y, 3)}

arm = bpy.data.objects.new('Armature', bpy.data.armatures.new('Armature')); bpy.context.scene.collection.objects.link(arm)
bpy.context.view_layer.objects.active = arm; bpy.ops.object.mode_set(mode='EDIT'); E = arm.data.edit_bones
V = lambda x, y, zz: C3 @ Vector((x, y, zz))


def bone(n, h, t, p=None):
    b = E.new(n); b.head = h; b.tail = t; b.roll = 0.0
    if p: b.parent = E[p]; b.use_connect = False
    return b


bone('root', V(0, 0, 0), V(0, 0, 0.15))
bone('pelvis', V(0, ys, f(0.50)), V(0, ys, f(0.55)), 'root')
bone('spine_01', V(0, ys, f(0.55)), V(0, ys, f(0.62)), 'pelvis'); bone('spine_02', V(0, ys, f(0.62)), V(0, ys, f(0.69)), 'spine_01')
bone('spine_03', V(0, ys, f(0.69)), V(0, ys, f(0.79)), 'spine_02')
bone('neck_01', V(0, (ys + yh) / 2, f(0.79)), V(0, yh, f(0.85)), 'spine_03'); bone('head', V(0, yh, f(0.85)), V(0, yh, f(1.0)), 'neck_01')
arm_down = {}
for sfx, sg in (('_l', 1), ('_r', -1)):
    sh = Vector((sg * SH_X, ys, SH_Z)); d = tip[sg] - sh; L = d.length; d = Vector((d.x, 0, d.z)).normalized()
    arm_down[sg] = math.degrees(math.atan2(-d.z, abs(d.x)))
    el = sh + d * 0.45 * L; wr = sh + d * 0.84 * L; hd = sh + d * L
    bone('clavicle' + sfx, V(sg * 0.04, ys, f(0.76)), C3 @ sh, 'spine_03'); bone('upperarm' + sfx, C3 @ sh, C3 @ el, 'clavicle' + sfx)
    bone('lowerarm' + sfx, C3 @ el, C3 @ wr, 'upperarm' + sfx); bone('hand' + sfx, C3 @ wr, C3 @ hd, 'lowerarm' + sfx)
    x = xf[sg]; hx = sg * HIP_X; kx = hx + (x - hx) * (HIP_Z - KNEE_Z) / (HIP_Z - ANKLE_Z)
    bone('thigh' + sfx, V(hx, yl, HIP_Z), V(kx, yl, KNEE_Z), 'pelvis'); bone('calf' + sfx, V(kx, yl, KNEE_Z), V(x, yl, ANKLE_Z), 'thigh' + sfx)
    by = (yl + toe_y) / 2
    bone('foot' + sfx, V(x, yl, ANKLE_Z), V(x, by, 0.03), 'calf' + sfx); bone('ball' + sfx, V(x, by, 0.03), V(x, toe_y, 0.02), 'foot' + sfx)
bpy.ops.object.mode_set(mode='OBJECT')
R['arm_down_deg'] = {k: round(v, 1) for k, v in (('l', arm_down[1]), ('r', arm_down[-1]))}
R['bones'] = len(arm.data.bones)
me.transform(C); me.update()  # mesh now faces +X like the armature

# ---- weights: heat on a watertight voxel proxy, transferred by nearest proxy vertex
bpy.ops.object.select_all(action='DESELECT')
px = ob.copy(); px.data = ob.data.copy(); px.name = 'PROXY'; bpy.context.scene.collection.objects.link(px); px.data.materials.clear()
bpy.ops.object.select_all(action='DESELECT'); px.select_set(True); bpy.context.view_layer.objects.active = px
px.data.remesh_voxel_size = 0.016; px.data.remesh_voxel_adaptivity = 0.0; bpy.ops.object.voxel_remesh()
R['proxy_tris'] = sum(len(p.vertices) - 2 for p in px.data.polygons)
bpy.ops.object.select_all(action='DESELECT'); px.select_set(True); arm.select_set(True); bpy.context.view_layer.objects.active = arm
t1 = time.time(); method = 'proxy heat (ARMATURE_AUTO on a watertight 16 mm voxel proxy) transferred by nearest proxy vertex'
try: bpy.ops.object.parent_set(type='ARMATURE_AUTO')
except Exception as e: method = 'ARMATURE_AUTO raised ' + str(e)[:100]
R['heat_secs'] = round(time.time() - t1, 1)
pm = px.data; names = [b.name for b in arm.data.bones]; gi = {g.index: g.name for g in px.vertex_groups}
W = np.zeros((len(pm.vertices), len(names)), dtype=np.float32); ni = {n: i for i, n in enumerate(names)}
for v in pm.vertices:
    for g in v.groups:
        if gi[g.group] in ni: W[v.index, ni[gi[g.group]]] = g.weight
R['proxy_zero_weight_verts'] = int((W.sum(1) < 1e-6).sum()); R['proxy_verts'] = len(pm.vertices)
if R['proxy_zero_weight_verts'] > 0.05 * len(pm.vertices): method = 'heat failed on proxy -> nearest-bone-segment weights'
kd = kdtree.KDTree(len(pm.vertices)); pco = np.empty(len(pm.vertices) * 3); pm.vertices.foreach_get('co', pco); pco = pco.reshape(-1, 3)
for i, c in enumerate(pco): kd.insert(c, i)
kd.balance()
for n in names: ob.vertex_groups.new(name=n)
segs = [(n, arm.data.bones[n].head_local, arm.data.bones[n].tail_local) for n in names if n != 'root']


def nearest(p):
    best = None; bd = 1e9
    for n, h, t in segs:
        d = t - h; L = d.length_squared; u = 0 if L == 0 else max(0, min(1, (p - h).dot(d) / L)); dist = (p - (h + d * u)).length
        if dist < bd: bd = dist; best = n
    return best


fb = 0; use_heat = method.startswith('proxy')
for v in me.vertices:
    done = False
    if use_heat:
        _, pi, _ = kd.find(v.co); w = W[pi]
        if w.sum() > 1e-6:
            for j in np.nonzero(w)[0]: ob.vertex_groups[names[j]].add([v.index], float(w[j]), 'REPLACE')
            done = True
    if not done: fb += 1; ob.vertex_groups[nearest(v.co)].add([v.index], 1.0, 'REPLACE')
R['nearest_bone_fallback_verts'] = fb
bpy.data.objects.remove(px, do_unlink=True)
for o in bpy.context.scene.objects: o.select_set(False)
bpy.context.view_layer.objects.active = ob; ob.select_set(True); bpy.ops.object.mode_set(mode='WEIGHT_PAINT')
bpy.ops.object.vertex_group_smooth(group_select_mode='ALL', factor=0.5, repeat=4, expand=0.0)
bpy.ops.object.vertex_group_limit_total(group_select_mode='ALL', limit=4); bpy.ops.object.vertex_group_normalize_all(group_select_mode='ALL', lock_active=False)
bpy.ops.object.mode_set(mode='OBJECT')
ob.modifiers.clear(); mo = ob.modifiers.new('Armature', 'ARMATURE'); mo.object = arm; ob.parent = arm; ob.matrix_parent_inverse = Matrix()
R['weights'] = method + '; then vertex_group_smooth x4, limit 4, normalize_all'

# ---- clips, authored as armature-space rotations in the character frame (front -Y, left +X)
#  X+: bend forward (spine), thigh X-: leg forward, calf X+: knee flex; arm Y: s*deg lowers (s=+1 left, -1 right);
#  arm Z: -s*deg swings forward; spine Z+: right shoulder forward.
sc = bpy.context.scene; sc.render.fps = FPS
bpy.context.view_layer.objects.active = arm; arm.select_set(True); bpy.ops.object.mode_set(mode='POSE'); pb = arm.pose.bones
for b in pb: b.rotation_mode = 'QUATERNION'
AX = {'X': Vector((1, 0, 0)), 'Y': Vector((0, 1, 0)), 'Z': Vector((0, 0, 1))}


def q_for(n, rots):
    Rr = arm.data.bones[n].matrix_local.to_3x3(); Ra = Matrix.Identity(3)
    for ax, deg in rots: Ra = (C3 @ Matrix.Rotation(math.radians(deg), 3, AX[ax]) @ Ci3) @ Ra
    return (Rr.inverted() @ Ra @ Rr).to_quaternion()


def side(sg, base):  # mirror a left-side spec to a side: Y and Z flip with the side, X does not
    return [(ax, deg if ax == 'X' else deg * sg) for ax, deg in base]


L_DOWN, R_DOWN = 72 - arm_down[1], 72 - arm_down[-1]  # idle arms about 72 degrees below horizontal


def idle_pose(breath=0.0):
    p = {'upperarm_l': [('Y', L_DOWN), ('Z', -6)], 'upperarm_r': [('Y', -R_DOWN), ('Z', 6)],
         'lowerarm_l': [('Z', -14)], 'lowerarm_r': [('Z', 14)],
         'spine_02': [('X', 2 * breath)], 'spine_03': [('X', -1.5 * breath)], 'neck_01': [('X', -0.8 * breath)],
         'clavicle_l': [('Y', -1.5 * breath)], 'clavicle_r': [('Y', 1.5 * breath)],
         'thigh_l': [('Y', -2)], 'thigh_r': [('Y', 2)], 'calf_l': [('X', 3)], 'calf_r': [('X', 3)]}
    return p, Vector((0, 0, -0.005 * breath))


def key(frame, spec, pelvis_delta=Vector()):
    for b in pb:
        b.rotation_quaternion = q_for(b.name, spec.get(b.name, [])); b.location = Vector()
    b = pb['pelvis']; b.location = arm.data.bones['pelvis'].matrix_local.to_3x3().inverted() @ (C3 @ pelvis_delta)
    for b in pb:
        b.keyframe_insert('rotation_quaternion', frame=frame); b.keyframe_insert('location', frame=frame)


def merged(*specs):
    out_ = {}
    for s_ in specs:
        for k, v in s_.items(): out_[k] = out_.get(k, []) + v
    return out_


IDLE, IDLE_D = idle_pose()
CLIPS = {}
# Idle: 2 s breathing loop (first and last key equal)
CLIPS['Idle'] = [(0, *idle_pose(0)), (30, *idle_pose(1)), (60, *idle_pose(0))]
# Cast: sheet 02 - stand, crouch and draw back, peak wind-up, release with the right arm extended, follow-through, recover
crouch_legs = {'thigh_l': [('X', -16)], 'thigh_r': [('X', -16)], 'calf_l': [('X', 30)], 'calf_r': [('X', 30)], 'foot_l': [('X', -14)], 'foot_r': [('X', -14)]}
CLIPS['Cast'] = [
    (0, IDLE, IDLE_D),
    (9, merged(crouch_legs, {'spine_01': [('X', 10)], 'spine_02': [('Z', -14)], 'spine_03': [('Z', -10)],
                             'upperarm_r': [('Y', -R_DOWN + 20), ('Z', -45)], 'lowerarm_r': [('Z', 35)],
                             'upperarm_l': [('Y', L_DOWN - 25), ('Z', -40)], 'lowerarm_l': [('Z', -35)]}), Vector((0, 0, -0.07))),
    (17, merged({'thigh_l': [('X', -8)], 'thigh_r': [('X', -6)], 'calf_l': [('X', 12)], 'calf_r': [('X', 12)]},
                {'spine_01': [('X', -4)], 'spine_02': [('Z', -20)], 'spine_03': [('Z', -12)], 'neck_01': [('Z', 12)],
                 'upperarm_r': [('Y', arm_down[-1] + 35), ('Z', -25)], 'lowerarm_r': [('Y', 75)], 'hand_r': [('Y', 20)],
                 'upperarm_l': [('Y', -arm_down[1] + 5), ('Z', -55)], 'lowerarm_l': [('Z', -30)]}), Vector((0, 0, -0.03))),
    (23, merged({'thigh_l': [('X', -24)], 'calf_l': [('X', 18)], 'thigh_r': [('X', 14)], 'calf_r': [('X', 8)], 'foot_r': [('X', -8)]},
                {'spine_01': [('X', 10)], 'spine_02': [('Z', 16)], 'spine_03': [('Z', 12)], 'neck_01': [('Z', -10)],
                 'upperarm_r': [('Y', arm_down[-1]), ('Z', 78)], 'lowerarm_r': [('Z', 4)], 'hand_r': [('Y', 25)],
                 'upperarm_l': [('Y', L_DOWN - 10), ('Z', 28)], 'lowerarm_l': [('Z', -20)]}), Vector((0, -0.06, -0.04))),
    (31, merged({'thigh_l': [('X', -24)], 'calf_l': [('X', 22)], 'thigh_r': [('X', 16)], 'calf_r': [('X', 10)]},
                {'spine_01': [('X', 16)], 'spine_02': [('Z', 26)], 'spine_03': [('Z', 16)], 'neck_01': [('Z', -14)],
                 'upperarm_r': [('Y', arm_down[-1] - 25), ('Z', 92)], 'lowerarm_r': [('Z', 22)],
                 'upperarm_l': [('Y', L_DOWN - 5), ('Z', 35)], 'lowerarm_l': [('Z', -15)]}), Vector((0, -0.08, -0.05))),
    (46, IDLE, IDLE_D),
]
# Hit: torso twisted back, the left arm raised to shield the face, recover
CLIPS['Hit'] = [
    (0, IDLE, IDLE_D),
    (4, {'spine_01': [('X', -10)], 'spine_02': [('X', -10), ('Z', 18)], 'spine_03': [('X', -6)], 'neck_01': [('X', -14)], 'head': [('X', -10), ('Z', 10)],
         'upperarm_l': [('Y', -arm_down[1] - 10), ('Z', -70)], 'lowerarm_l': [('Y', -95)], 'hand_l': [('Y', -10)],
         'upperarm_r': [('Y', -R_DOWN + 25), ('Z', -25)], 'lowerarm_r': [('Z', 25)],
         'thigh_l': [('X', 8)], 'thigh_r': [('X', -10)], 'calf_r': [('X', 14)]}, Vector((0, 0.05, -0.02))),
    (10, {'spine_01': [('X', -7)], 'spine_02': [('X', -7), ('Z', 14)], 'spine_03': [('X', -4)], 'neck_01': [('X', -8)], 'head': [('X', -6), ('Z', 8)],
          'upperarm_l': [('Y', -arm_down[1] - 5), ('Z', -65)], 'lowerarm_l': [('Y', -85)],
          'upperarm_r': [('Y', -R_DOWN + 20), ('Z', -18)], 'lowerarm_r': [('Z', 20)],
          'thigh_l': [('X', 6)], 'thigh_r': [('X', -8)], 'calf_r': [('X', 10)]}, Vector((0, 0.04, -0.02))),
    (24, IDLE, IDLE_D),
]
# Death: recoil, knees buckle, kneel, collapse forward; the last key is held
DROP = KNEE_Z - 0.08
limp = {'upperarm_l': [('Y', L_DOWN + 4), ('Z', -12)], 'upperarm_r': [('Y', -R_DOWN - 4), ('Z', 12)], 'lowerarm_l': [('Z', -10)], 'lowerarm_r': [('Z', 10)]}
CLIPS['Death'] = [
    (0, IDLE, IDLE_D),
    (6, {'spine_01': [('X', -12)], 'spine_02': [('X', -10)], 'neck_01': [('X', -16)], 'head': [('X', -10)],
         'upperarm_l': [('Y', L_DOWN - 30), ('Z', 10)], 'upperarm_r': [('Y', -R_DOWN + 30), ('Z', -10)]}, Vector((0, 0.05, 0))),
    (20, merged(limp, {'thigh_l': [('X', -35)], 'thigh_r': [('X', -30)], 'calf_l': [('X', 75)], 'calf_r': [('X', 70)], 'foot_l': [('X', -20)], 'foot_r': [('X', -20)],
                       'spine_01': [('X', 14)], 'spine_02': [('X', 8)], 'neck_01': [('X', 10)]}), Vector((0, 0.02, -0.5 * DROP))),
    (34, merged(limp, {'thigh_l': [('X', -6)], 'thigh_r': [('X', -2)], 'calf_l': [('X', 92)], 'calf_r': [('X', 90)], 'foot_l': [('X', 40)], 'foot_r': [('X', 40)],
                       'spine_01': [('X', 22)], 'spine_02': [('X', 12)], 'neck_01': [('X', 18)], 'head': [('X', 10)]}), Vector((0, 0.0, -DROP))),
    (48, {'pelvis': [('X', 22)], 'thigh_l': [('X', -26)], 'thigh_r': [('X', -22)], 'calf_l': [('X', 92)], 'calf_r': [('X', 90)], 'foot_l': [('X', 40)], 'foot_r': [('X', 40)],
          'spine_01': [('X', 26)], 'spine_02': [('X', 18)], 'spine_03': [('X', 10)], 'neck_01': [('X', 20)], 'head': [('X', 12)],
          'upperarm_l': [('Y', L_DOWN + 2), ('Z', -55)], 'upperarm_r': [('Y', -R_DOWN - 2), ('Z', 55)], 'lowerarm_l': [('Z', -25)], 'lowerarm_r': [('Z', 25)]},
     Vector((0, -0.06, -DROP - 0.02))),
    (60, {'pelvis': [('X', 25)], 'thigh_l': [('X', -29)], 'thigh_r': [('X', -25)], 'calf_l': [('X', 92)], 'calf_r': [('X', 90)], 'foot_l': [('X', 40)], 'foot_r': [('X', 40)],
          'spine_01': [('X', 28)], 'spine_02': [('X', 20)], 'spine_03': [('X', 12)], 'neck_01': [('X', 24)], 'head': [('X', 14)],
          'upperarm_l': [('Y', L_DOWN + 4), ('Z', -50)], 'upperarm_r': [('Y', -R_DOWN - 4), ('Z', 50)], 'lowerarm_l': [('Z', -28)], 'lowerarm_r': [('Z', 28)]},
     Vector((0, -0.07, -DROP - 0.03))),
]
ad = arm.animation_data_create()
R['clips'] = {}
for clip, keys in CLIPS.items():
    act = bpy.data.actions.new(clip); act.use_fake_user = True; ad.action = act
    for fr, spec, dl in keys: key(fr, spec, dl)
    tr = ad.nla_tracks.new(); tr.name = clip
    st = tr.strips.new(clip, 0, act); st.name = clip
    ad.action = None
    R['clips'][clip] = {'frames': int(keys[-1][0]) + 1, 'keys': [k[0] for k in keys]}
for b in pb: b.rotation_quaternion = Quaternion(); b.location = Vector()
bpy.ops.object.mode_set(mode='OBJECT')
sc.frame_start = 0; sc.frame_end = 60
bpy.ops.wm.save_as_mainfile(filepath=f'{out}/{name}_rigged.blend')
bpy.ops.object.select_all(action='DESELECT'); arm.select_set(True); ob.select_set(True)
bpy.ops.export_scene.fbx(filepath=fbx, use_selection=True, object_types={'ARMATURE', 'MESH'}, apply_scale_options='FBX_SCALE_ALL',
                         mesh_smooth_type='FACE', add_leaf_bones=False, armature_nodetype='NULL', bake_anim=True,
                         bake_anim_use_all_bones=True, bake_anim_use_nla_strips=True, bake_anim_use_all_actions=False,
                         bake_anim_force_startend_keying=True, bake_anim_step=1.0, bake_anim_simplify_factor=0.0,
                         path_mode='STRIP', embed_textures=False)
R['fbx'] = fbx; R['secs'] = round(time.time() - t0, 1)
json.dump(R, open(f'{out}/{name}_rig.json', 'w'), indent=1); print('RIG', json.dumps(R))
