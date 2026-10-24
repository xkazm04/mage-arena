# T38 Brennic: rig the finished LOD0 to UE5 mannequin bone chains, author Idle / Cast / Hit / Death, export one FBX.
# Rig adapted from lynx-bench pof3d/round2/characters/work/rig.py: the armature is fitted to the mesh, heat weights are
# computed on a watertight voxel proxy and transferred to the real mesh by nearest proxy vertex, then smoothed, limited
# to 4 influences and normalised; nearest-bone fallback for anything the heat left unweighted.
# The mesh comes from finish.py facing -Y. Everything here is authored in that character frame and mapped through C
# (+90 degrees about Z) so the export faces +X with Z up, origin at the feet, metres (FBX_SCALE_ALL -> UE centimetres).
# Clips sit on NLA strips named exactly Idle, Cast, Hit, Death; the FBX exporter names each take after its strip.
#   blender -b -P rig_anim.py -- <finish.blend> <outdir> <name> <fbx path> [<attack style>] [<beast fit.json>]
# Attack style: Cast (default, Brennic: clips Idle, Cast, Hit, Death) | Thrust | Sling (the Bout 2 soldiers: clips Idle, Attack, Hit, Death)
# | Hound | Maw (the Bout 2 beasts, run on beast_parts.py's <name>_parts.blend: their own skeletons, a quadruped chain for the
# hound and a body, jaw and tongue chain for the maw, rule weights for the jaw and tongue, clips Idle, Attack, Hit, Death).
# Beast fit (../mesh/beast_fit.py): the joints and the weight rules; without a fit file, the 2026-10-21 tables.
import bpy, sys, os, json, time, math, numpy as np
from mathutils import Vector, Quaternion, Matrix, kdtree

a = sys.argv[sys.argv.index('--') + 1:]; blend, out, name, fbx = a[:4]
STYLE = a[4] if len(a) > 4 else 'Cast'; ATTACK = 'Cast' if STYLE == 'Cast' else 'Attack'
FIT_PATH = a[5] if len(a) > 5 else ''
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

BEAST = STYLE in ('Hound', 'Maw')  # the Bout 2 beasts: their own skeletons, weights rules and clips
if FIT_PATH and not BEAST: raise SystemExit(f'a beast fit needs STYLE Hound or Maw, not {STYLE}')
if BEAST:
    sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'mesh'))
    import beast_fit
    FIT = beast_fit.load(FIT_PATH, STYLE); FR = FIT['rules']
    R['fit'] = {'file': os.path.abspath(FIT_PATH) if FIT_PATH else None, 'ref_height_m': FIT['refHeightM']}

arm = bpy.data.objects.new('Armature', bpy.data.armatures.new('Armature')); bpy.context.scene.collection.objects.link(arm)
bpy.context.view_layer.objects.active = arm; bpy.ops.object.mode_set(mode='EDIT'); E = arm.data.edit_bones
V = lambda x, y, zz: C3 @ Vector((x, y, zz))


def bone(n, h, t, p=None):
    b = E.new(n); b.head = h; b.tail = t; b.roll = 0.0
    if p: b.parent = E[p]; b.use_connect = False
    return b


MOVER = 'pelvis'; PROXY_VOX = 0.016
if BEAST:
    # ---- the Bout 2 beasts. Joints are metres in the character frame (front -Y, left +X, feet at z = 0), from the fit
    # (by default the profiles of the finished meshes of 2026-10-21, hound seed 7 at 1.36 m, maw seed 42 at 1.25 m),
    # scaled with H against the fit's reference height.
    # Rotations in the clips: X+ turns a forward-pointing bone's tip down (opens a jaw, lowers a head) and a down-pointing
    # bone's tip back; Y+ rolls the body onto its left side; Z+ turns a forward-pointing bone's tip to the right.
    # ('SY', k) stretches a bone k times along its length (the maw's tongue).
    S = H / FIT['refHeightM']
    P = lambda x, y, zz: V(x * S, y * S, zz * S)
    MOVER = 'pelvis' if STYLE == 'Hound' else 'body'
    PROXY_VOX = 0.010 if STYLE == 'Hound' else 0.016  # the hound's legs and tail are 2-5 cm thick

    def beast_bones():
        for n, h, t, par in FIT['bones']:  # root first; a head of None starts at the parent's tail
            if par is None: bone(n, P(*h), P(*t))
            else: bone(n, E[par].tail.copy() if h is None else P(*h), P(*t), par)

    def beast_weights():
        """Rules over the heat weights where a beast needs a clean split: the jaw below the mouth, the maw's tongue."""
        x, y, zz = (co[:, i] / S for i in range(3))  # character frame at the reference height
        vg = {g.name: g for g in ob.vertex_groups}; n_rule = 0
        tongue = set()
        if STYLE == 'Maw':
            mt = next((i for i, m in enumerate(me.materials) if m and m.name.endswith('_Tongue')), None)
            for p in me.polygons:
                if p.material_index == mt: tongue.update(p.vertices)
        for v in me.vertices:
            i = v.index
            if STYLE == 'Hound':
                if y[i] > FR['muzzleY'] or zz[i] < FR['minZ']: continue  # only the muzzle
                below = zz[i] < FR['lipZ'] + FR['lipRise'] * max(0.0, y[i] - FR['noseY'])  # the lip line, rising slightly to the cheek
                wj = (1.0 if below else 0.0) * min(1.0, max(0.0, (FR['muzzleY'] - y[i]) / FR['ramp']))
                own = {'jaw': wj, 'head': 1.0 - wj}
            elif i in tongue:  # each tongue vertex to its tongue bone, by its place along the tongue
                k = min(4, max(1, int((FR['tongueRootY'] - y[i]) / FR['tongueStep']) + 1))
                own = {f'tongue_{k:02d}': 1.0}
            else:  # the maw's lower lip (in front of the slot's back wall, under the slot, above the belly); no other
                # vertex keeps a jaw or tongue weight, or the roof would swing up with the jaw and the lash stretch the head
                wj = 0.0
                if zz[i] <= FR['lipZ'] and y[i] <= FR['backWallY']:
                    wj = min(1.0, max(0.0, (FR['backWallY'] - y[i]) / FR['backRamp'])) * min(1.0, max(0.0, (zz[i] - FR['bellyZ']) / FR['bellyRamp']))
                    wj *= min(1.0, max(0.0, (FR['halfWidth'] - abs(x[i] - FR['jawX'])) / FR['xRamp']))  # not the front legs
                own = {'jaw': wj, **{f'tongue_{k:02d}': 0.0 for k in range(1, 5)}}  # set outright: no stray heat jaw weight
                if wj <= 0 and not any(ob.vertex_groups[g.group].name in own for g in v.groups): continue
            n_rule += 1
            rest = 1.0 - sum(own.values())
            others = sum(g.weight for g in v.groups if ob.vertex_groups[g.group].name not in own)
            for g in v.groups:  # scale the other weights into what the rule leaves
                if ob.vertex_groups[g.group].name not in own: g.weight = g.weight * rest / others if others > 1e-6 else 0.0
            for nm, w in own.items():
                if w > 0: vg[nm].add([i], w, 'REPLACE')
                else: vg[nm].remove([i])
            if rest > 0 and others <= 1e-6: vg['body_upper' if STYLE == 'Maw' else 'head'].add([i], rest, 'REPLACE')
        R['rule_weighted_verts'] = n_rule

    def beast_clips():
        """Idle, Attack, Hit, Death as (frame, {bone: [(axis, degrees) | ('SY', k)]}, mover delta in metres)."""
        Z = Vector()
        clips = {}
        if STYLE == 'Hound':
            # Idle (sheet 07 pose 1, low and watchful): head carried at back level, slow breath, a look left and back, tail swaying
            def idle(look=0.0, breath=0.0, sway=0.0):
                return {'neck_01': [('X', 18 + breath)], 'neck_02': [('X', -4), ('Z', look)], 'head': [('X', -12)],
                        'spine_02': [('X', 1.5 * breath)], 'spine_03': [('X', 3)],
                        'front_upper_l': [('X', 6)], 'front_upper_r': [('X', 6)], 'front_lower_l': [('X', -8)], 'front_lower_r': [('X', -8)],
                        'back_thigh_l': [('X', -6)], 'back_thigh_r': [('X', -6)], 'back_shin_l': [('X', 8)], 'back_shin_r': [('X', 8)],
                        'tail_01': [('X', -18), ('Z', sway)], 'tail_02': [('X', -6), ('Z', sway)], 'jaw': [('X', 2 * breath)]}
            I, ID = idle(), Vector((0, 0, -0.03))
            clips['Idle'] = [(0, I, ID), (30, idle(10, 1, 8), Vector((0, 0, -0.035))), (60, I, ID)]
            # Attack, the ember spit (VR overlay: windup 0.6 s, so the release is frame 18): crouch and draw the head
            # back, then thrust it forward with the jaw wide open, hold, recover
            crouch = {'front_upper_l': [('X', 18)], 'front_upper_r': [('X', 18)], 'front_lower_l': [('X', -26)], 'front_lower_r': [('X', -26)],
                      'back_thigh_l': [('X', -14)], 'back_thigh_r': [('X', -14)], 'back_shin_l': [('X', 18)], 'back_shin_r': [('X', 18)]}
            clips['Attack'] = [
                (0, I, ID),
                (9, merged(crouch, {'spine_03': [('X', 8)], 'neck_01': [('X', 30)], 'neck_02': [('X', -2)], 'head': [('X', 4)],
                                    'tail_01': [('X', 6)], 'jaw': [('X', 4)]}), Vector((0, 0.05, -0.08))),
                (18, merged(crouch, {'spine_03': [('X', 2)], 'neck_01': [('X', 26)], 'neck_02': [('X', 12)], 'head': [('X', -32)],
                                     'tail_01': [('X', 14)], 'jaw': [('X', 40)]}), Vector((0, -0.10, -0.06))),
                (23, merged(crouch, {'spine_03': [('X', 2)], 'neck_01': [('X', 22)], 'neck_02': [('X', 8)], 'head': [('X', -26)],
                                     'tail_01': [('X', 10)], 'jaw': [('X', 30)]}), Vector((0, -0.08, -0.06))),
                (35, I, ID),
            ]
            # Hit (sheet 07 pose 3): recoil sideways, head jerked up and away, tail tucked, recover
            clips['Hit'] = [
                (0, I, ID),
                (4, {'pelvis': [('Y', -8)], 'spine_02': [('Z', -18)], 'spine_03': [('Z', -14)], 'neck_01': [('X', -16), ('Z', -24)],
                     'head': [('X', -14)], 'jaw': [('X', 18)], 'tail_01': [('X', -40)],
                     'front_upper_l': [('Y', -14)], 'front_upper_r': [('Y', -14)], 'back_thigh_l': [('Y', -10)], 'back_thigh_r': [('Y', -10)]},
                 Vector((-0.16, 0.08, 0.02))),
                (10, {'pelvis': [('Y', -5)], 'spine_02': [('Z', -12)], 'spine_03': [('Z', -9)], 'neck_01': [('X', -6), ('Z', -14)],
                      'head': [('X', -8)], 'jaw': [('X', 10)], 'tail_01': [('X', -32)],
                      'front_upper_l': [('Y', -8)], 'front_upper_r': [('Y', -8)]}, Vector((-0.12, 0.06, 0.0))),
                (24, I, ID),
            ]
            # Death (sheet 07 pose 4): rear up, legs buckle, roll onto the left side; the last key is held
            fold = {'front_upper_l': [('X', 40)], 'front_upper_r': [('X', 40)], 'front_lower_l': [('X', -70)], 'front_lower_r': [('X', -70)],
                    'back_thigh_l': [('X', -35)], 'back_thigh_r': [('X', -35)], 'back_shin_l': [('X', 55)], 'back_shin_r': [('X', 55)],
                    'back_hock_l': [('X', -30)], 'back_hock_r': [('X', -30)]}
            limp = {'front_upper_l': [('X', 20)], 'front_upper_r': [('X', 10)], 'front_lower_l': [('X', -25)], 'front_lower_r': [('X', -15)],
                    'back_thigh_l': [('X', -20)], 'back_thigh_r': [('X', -10)], 'back_shin_l': [('X', 25)], 'back_shin_r': [('X', 15)],
                    'tail_01': [('X', -10), ('Z', 15)], 'tail_02': [('Z', 10)]}
            clips['Death'] = [
                (0, I, ID),
                (6, {'neck_01': [('X', -14)], 'neck_02': [('X', -10)], 'head': [('X', -16)], 'jaw': [('X', 28)], 'tail_01': [('X', -30)]},
                 Vector((0, 0.04, 0.02))),
                (18, merged(fold, {'neck_01': [('X', 26)], 'head': [('X', 4)], 'jaw': [('X', 20)], 'tail_01': [('X', -30)]}),
                 Vector((0, 0.02, -0.30 * S))),
                (32, merged(limp, {'pelvis': [('Y', 55)], 'neck_01': [('X', 20), ('Z', 10)], 'head': [('X', 6)], 'jaw': [('X', 24)]}),
                 Vector((0.10 * S, 0, -0.50 * S))),
                (46, merged(limp, {'pelvis': [('Y', 84)], 'neck_01': [('X', 12), ('Z', 18)], 'neck_02': [('Z', 8)], 'head': [('X', 4)], 'jaw': [('X', 26)]}),
                 Vector((0.04 * S, 0, -0.68 * S))),
                (60, merged(limp, {'pelvis': [('Y', 86)], 'neck_01': [('X', 12), ('Z', 20)], 'neck_02': [('Z', 10)], 'head': [('X', 4)], 'jaw': [('X', 28)]}),
                 Vector((0.04 * S, 0, -0.69 * S))),
            ]
        else:
            # Idle: slow breath swelling the throat, a small chew
            def idle(breath=0.0):
                return {'body_upper': [('X', -1.5 * breath)], 'head': [('X', -2 * breath)], 'jaw': [('X', 3 * breath)]}
            clips['Idle'] = [(0, idle(0), Z), (30, idle(1), Vector((0, 0, 0.012))), (60, idle(0), Z)]
            # Attack, the tongue pull (pinned windupS 1.0, so the lash is frame 30-36): lean back and gather, open, lash the
            # tongue out about six times its length (2.4 m), hold while leaning back to pull, reel it in, close
            clips['Attack'] = [
                (0, idle(0), Z),
                (14, {'body': [('X', -6)], 'body_upper': [('X', -6)], 'head': [('X', -4)], 'jaw': [('X', 6)]}, Vector((0, 0.03, -0.04))),
                (30, {'body': [('X', 4)], 'body_upper': [('X', 6)], 'head': [('X', -6)], 'jaw': [('X', 30)], 'tongue_01': [('X', -26)]},
                 Vector((0, -0.03, 0.0))),
                (36, {'body': [('X', 6)], 'body_upper': [('X', 6)], 'head': [('X', -6)], 'jaw': [('X', 30)],
                      'tongue_01': [('X', -40), ('SY', 6.0)]}, Vector((0, -0.05, 0.0))),
                (42, {'body': [('X', -5)], 'body_upper': [('X', -4)], 'head': [('X', -6)], 'jaw': [('X', 28)],
                      'tongue_01': [('X', -24), ('SY', 5.6)]}, Vector((0, 0.03, -0.02))),
                (56, {'body': [('X', -6)], 'body_upper': [('X', -4)], 'head': [('X', -4)], 'jaw': [('X', 18)],
                      'tongue_01': [('X', -14), ('SY', 1.3)]}, Vector((0, 0.03, -0.03))),
                (66, idle(0), Z),
            ]
            # Hit: knocked back, the head jerks up, the mouth gapes, recover
            clips['Hit'] = [
                (0, idle(0), Z),
                (4, {'body': [('X', -9)], 'body_upper': [('X', -8)], 'head': [('X', -10)], 'jaw': [('X', 14)]}, Vector((0, 0.07, -0.03))),
                (10, {'body': [('X', -5)], 'body_upper': [('X', -5)], 'head': [('X', -6)], 'jaw': [('X', 8)]}, Vector((0, 0.04, -0.02))),
                (24, idle(0), Z),
            ]
            # Death: a last gasp, then the mound slumps forward and sinks, the jaw slack and the tongue lolling; held
            clips['Death'] = [
                (0, idle(0), Z),
                (8, {'body_upper': [('X', -10)], 'head': [('X', -12)], 'jaw': [('X', 22)]}, Vector((0, 0, 0.03))),
                (24, {'body': [('X', 12)], 'body_upper': [('X', 12)], 'head': [('X', 6)], 'jaw': [('X', 30)],
                      'tongue_01': [('X', 10), ('SY', 2.0)]}, Vector((0, -0.06, -0.14 * S))),
                (40, {'body': [('X', 20)], 'body_upper': [('X', 16)], 'head': [('X', 8)], 'jaw': [('X', 34)],
                      'tongue_01': [('X', 26), ('SY', 2.4)]}, Vector((0, -0.10, -0.26 * S))),
                (60, {'body': [('X', 22)], 'body_upper': [('X', 17)], 'head': [('X', 8)], 'jaw': [('X', 35)],
                      'tongue_01': [('X', 30), ('SY', 2.4)]}, Vector((0, -0.11, -0.28 * S))),
            ]
        return clips


if not BEAST:
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
else:
    beast_bones()
bpy.ops.object.mode_set(mode='OBJECT')
if not BEAST: R['arm_down_deg'] = {k: round(v, 1) for k, v in (('l', arm_down[1]), ('r', arm_down[-1]))}
R['bones'] = len(arm.data.bones)
me.transform(C); me.update()  # mesh now faces +X like the armature

# ---- weights: heat on a watertight voxel proxy, transferred by nearest proxy vertex
bpy.ops.object.select_all(action='DESELECT')
px = ob.copy(); px.data = ob.data.copy(); px.name = 'PROXY'; bpy.context.scene.collection.objects.link(px); px.data.materials.clear()
bpy.ops.object.select_all(action='DESELECT'); px.select_set(True); bpy.context.view_layer.objects.active = px
px.data.remesh_voxel_size = PROXY_VOX; px.data.remesh_voxel_adaptivity = 0.0; bpy.ops.object.voxel_remesh()
R['proxy_tris'] = sum(len(p.vertices) - 2 for p in px.data.polygons)
bpy.ops.object.select_all(action='DESELECT'); px.select_set(True); arm.select_set(True); bpy.context.view_layer.objects.active = arm
t1 = time.time(); method = f'proxy heat (ARMATURE_AUTO on a watertight {PROXY_VOX * 1000:.0f} mm voxel proxy) transferred by nearest proxy vertex'
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
if BEAST:  # the rules go over the smoothed weights, so the smoothing cannot bleed them back
    bpy.ops.object.mode_set(mode='OBJECT'); beast_weights(); bpy.ops.object.mode_set(mode='WEIGHT_PAINT')
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
    for ax, deg in rots:
        if ax in AX: Ra = (C3 @ Matrix.Rotation(math.radians(deg), 3, AX[ax]) @ Ci3) @ Ra  # 'SY' (beasts) is a scale, not a turn
    return (Rr.inverted() @ Ra @ Rr).to_quaternion()


def key(frame, spec, pelvis_delta=Vector()):
    for b in pb:
        b.rotation_quaternion = q_for(b.name, spec.get(b.name, [])); b.location = Vector()
        if BEAST: b.scale = Vector((1, next((k for ax, k in spec.get(b.name, []) if ax == 'SY'), 1.0), 1))
    b = pb[MOVER]; b.location = arm.data.bones[MOVER].matrix_local.to_3x3().inverted() @ (C3 @ pelvis_delta)
    for b in pb:
        b.keyframe_insert('rotation_quaternion', frame=frame); b.keyframe_insert('location', frame=frame)
        if BEAST: b.keyframe_insert('scale', frame=frame)


def merged(*specs):
    out_ = {}
    for s_ in specs:
        for k, v in s_.items(): out_[k] = out_.get(k, []) + v
    return out_


if not BEAST:
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

    IDLE, IDLE_D = idle_pose()
    CLIPS = {}
    # Idle: 2 s breathing loop (first and last key equal)
    CLIPS['Idle'] = [(0, *idle_pose(0)), (30, *idle_pose(1)), (60, *idle_pose(0))]
    # Cast: sheet 02 - stand, crouch and draw back, peak wind-up, release with the right arm extended, follow-through, recover
    crouch_legs = {'thigh_l': [('X', -16)], 'thigh_r': [('X', -16)], 'calf_l': [('X', 30)], 'calf_r': [('X', 30)], 'foot_l': [('X', -14)], 'foot_r': [('X', -14)]}
    CLIPS[ATTACK] = [
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
    if STYLE == 'Thrust':  # conscript: shield arm forward, draw the sword hand back, lunge and thrust, hold, recover
        guard = {'upperarm_l': [('Y', L_DOWN - 28), ('Z', -45)], 'lowerarm_l': [('Z', -50)]}
        CLIPS[ATTACK] = [
            (0, IDLE, IDLE_D),
            (7, merged(guard, {'thigh_l': [('X', -10)], 'thigh_r': [('X', 8)], 'calf_l': [('X', 14)], 'calf_r': [('X', 8)],
                               'spine_01': [('X', 4)], 'spine_02': [('Z', -18)], 'spine_03': [('Z', -12)], 'neck_01': [('Z', 14)],
                               'upperarm_r': [('Y', -R_DOWN + 22), ('Z', -40)], 'lowerarm_r': [('Z', 40)]}), Vector((0, 0.03, -0.03))),
            (12, merged(guard, {'thigh_l': [('X', -30)], 'calf_l': [('X', 26)], 'thigh_r': [('X', 16)], 'calf_r': [('X', 10)], 'foot_r': [('X', -8)],
                                'spine_01': [('X', 12)], 'spine_02': [('Z', 24)], 'spine_03': [('Z', 14)], 'neck_01': [('Z', -14)],
                                'upperarm_r': [('Y', arm_down[-1] - 12), ('Z', 88)], 'lowerarm_r': [('Z', 14)]}), Vector((0, -0.14, -0.08))),
            (18, merged(guard, {'thigh_l': [('X', -30)], 'calf_l': [('X', 26)], 'thigh_r': [('X', 16)], 'calf_r': [('X', 10)],
                                'spine_01': [('X', 12)], 'spine_02': [('Z', 22)], 'spine_03': [('Z', 12)], 'neck_01': [('Z', -12)],
                                'upperarm_r': [('Y', arm_down[-1] - 12), ('Z', 84)], 'lowerarm_r': [('Z', 10)]}), Vector((0, -0.14, -0.08))),
            (35, IDLE, IDLE_D),
        ]
    elif STYLE == 'Sling':  # slinger: right arm winds up high behind the head, whirls over, releases forward and down
        CLIPS[ATTACK] = [
            (0, IDLE, IDLE_D),
            (9, {'thigh_l': [('X', -10)], 'thigh_r': [('X', 6)], 'calf_l': [('X', 12)], 'calf_r': [('X', 6)],
                 'spine_01': [('X', -4)], 'spine_02': [('Z', -22), ('X', -4)], 'spine_03': [('Z', -14)], 'neck_01': [('Z', 12)],
                 'upperarm_r': [('Y', arm_down[-1] + 80), ('Z', -30)], 'lowerarm_r': [('Z', 50)],
                 'upperarm_l': [('Y', L_DOWN - 35), ('Z', -50)], 'lowerarm_l': [('Z', -25)]}, Vector((0, 0.03, -0.02))),
            (17, {'thigh_l': [('X', -14)], 'thigh_r': [('X', 8)], 'calf_l': [('X', 12)], 'calf_r': [('X', 6)],
                  'spine_01': [('X', -2)], 'spine_02': [('Z', -6)], 'spine_03': [('Z', -4)], 'neck_01': [('Z', 4)],
                  'upperarm_r': [('Y', arm_down[-1] + 92), ('Z', 25)], 'lowerarm_r': [('Z', 40)],
                  'upperarm_l': [('Y', L_DOWN - 35), ('Z', -50)], 'lowerarm_l': [('Z', -25)]}, Vector((0, 0.0, -0.02))),
            (24, {'thigh_l': [('X', -26)], 'calf_l': [('X', 20)], 'thigh_r': [('X', 14)], 'calf_r': [('X', 8)],
                  'spine_01': [('X', 14)], 'spine_02': [('Z', 24)], 'spine_03': [('Z', 14)], 'neck_01': [('Z', -12)],
                  'upperarm_r': [('Y', arm_down[-1] + 8), ('Z', 88)], 'lowerarm_r': [('Z', 6)],
                  'upperarm_l': [('Y', L_DOWN - 10), ('Z', 20)], 'lowerarm_l': [('Z', -20)]}, Vector((0, -0.1, -0.05))),
            (32, {'thigh_l': [('X', -26)], 'calf_l': [('X', 20)], 'thigh_r': [('X', 14)], 'calf_r': [('X', 8)],
                  'spine_01': [('X', 18)], 'spine_02': [('Z', 28)], 'spine_03': [('Z', 16)], 'neck_01': [('Z', -14)],
                  'upperarm_r': [('Y', arm_down[-1] - 28), ('Z', 70)], 'lowerarm_r': [('Z', 8)],
                  'upperarm_l': [('Y', L_DOWN - 5), ('Z', 30)], 'lowerarm_l': [('Z', -15)]}, Vector((0, -0.1, -0.06))),
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
else:
    CLIPS = beast_clips()

ad = arm.animation_data_create()
R['clips'] = {}
for clip, keys in CLIPS.items():
    act = bpy.data.actions.new(clip); act.use_fake_user = True; ad.action = act
    for fr, spec, dl in keys: key(fr, spec, dl)
    tr = ad.nla_tracks.new(); tr.name = clip
    st = tr.strips.new(clip, 0, act); st.name = clip
    ad.action = None
    R['clips'][clip] = {'frames': int(keys[-1][0]) + 1, 'keys': [k[0] for k in keys]}
for b in pb: b.rotation_quaternion = Quaternion(); b.location = Vector(); b.scale = Vector((1, 1, 1))
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
