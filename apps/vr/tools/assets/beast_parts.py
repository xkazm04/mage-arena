# T15 Bout 2 beasts: hand-authored Blender parts on the finished LOD0, before the rig. Parametric bmesh geometry written for
# this project (no Leonardo, TRELLIS, ComfyUI or third-party source). Every part is joined to the body with an EXACT boolean,
# so the mesh stays one piece with no floaters; the new faces take the part's own flat-colour material.
#   CinderHound: a thin mouth slit along the muzzle, lined with the Ember material, so the jaw can open and glow (ember spit).
#   MireMaw:     a wide lens-shaped mouth slot across the front (Mouth), a flat tongue lying in it, rooted in the slot's back
#                wall (Tongue), and two eyes with pupils on the brow (Eye, Pupil). TRELLIS lost the face on every seed, so
#                the face is made here.
# Positions are metres in the finish.py character frame (front -Y, left +X, feet at z = 0), measured on the meshes of
# 2026-10-21 (hound seed 7 at 1.36 m, maw seed 42 at 1.25 m); they scale with the mesh height.
#   blender -b -P beast_parts.py -- <finish.blend> <outdir> <name> <CinderHound | MireMaw>
import bpy, bmesh, sys, json, math
from mathutils import Matrix, Vector

a = sys.argv[sys.argv.index('--') + 1:]; blend, out, name, KIND = a[:4]
REF_H = {'CinderHound': 1.36, 'MireMaw': 1.25}[KIND]
COLOURS = {'Ember': (0.96, 0.45, 0.10), 'Mouth': (0.24, 0.16, 0.12), 'Tongue': (0.55, 0.24, 0.24),
           'Eye': (0.86, 0.80, 0.42), 'Pupil': (0.05, 0.05, 0.05)}
bpy.ops.wm.open_mainfile(filepath=blend)
ob = bpy.data.objects['SM_' + name]; me = ob.data
co = [v.co.z for v in me.vertices]; S = (max(co) - min(co)) / REF_H
R = {'scale_from_reference': round(S, 4), 'tris_before': sum(len(p.vertices) - 2 for p in me.polygons), 'parts': []}


def material(part):
    nm = f'M_{name}_{part}'
    m = bpy.data.materials.get(nm)
    if m is None:
        m = bpy.data.materials.new(nm); m.use_nodes = True; c = COLOURS[part]
        lc = [((x + 0.055) / 1.055) ** 2.4 if x > 0.04045 else x / 12.92 for x in c]
        pb = m.node_tree.nodes['Principled BSDF']; pb.inputs['Base Color'].default_value = (*lc, 1)
        pb.inputs['Roughness'].default_value = 0.8; pb.inputs['Metallic'].default_value = 0.0; m.diffuse_color = (*lc, 1)
    return m


def ellipsoid(centre, radii, part, segs=24, rings=12):
    """A closed UV ellipsoid; centre and radii in metres at the reference height."""
    o = bpy.data.objects.new('part', bpy.data.meshes.new('part')); bpy.context.scene.collection.objects.link(o)
    bm = bmesh.new(); bmesh.ops.create_uvsphere(bm, u_segments=segs, v_segments=rings, radius=1.0)
    bmesh.ops.transform(bm, matrix=Matrix.Translation(Vector(centre) * S) @ Matrix.Diagonal((*(Vector(radii) * S), 1)), verts=bm.verts)
    bm.to_mesh(o.data); bm.free(); o.data.materials.append(material(part))
    return o


def capsule(p0, p1, rx, rz, part, segs=16, rings=10):
    """A flat tapered tongue pointing forward (-Y) from p0 (root, half-width rx, half-thickness rz) to p1 straight ahead
    of it (rounded tip, 60 % as wide)."""
    o = bpy.data.objects.new('part', bpy.data.meshes.new('part')); bpy.context.scene.collection.objects.link(o)
    bm = bmesh.new(); bmesh.ops.create_uvsphere(bm, u_segments=segs, v_segments=rings, radius=1.0)
    p0 = Vector(p0) * S; p1 = Vector(p1) * S; L = p0.y - p1.y
    assert L > 0 and abs(p0.x - p1.x) < 1e-6 and abs(p0.z - p1.z) < 1e-6, 'the tongue runs straight forward'
    for v in bm.verts:  # sphere -> lens along -Y: y in [1, -1] maps root to tip, width tapers to the tip
        t = (1 - v.co.y) / 2; w = 1.0 - 0.4 * t
        v.co = p0 + Vector((v.co.x * rx * S * w, -t * L, v.co.z * rz * S * w))
    bm.to_mesh(o.data); bm.free(); o.data.materials.append(material(part))
    return o


def boolean(cutter, op, part):
    md = ob.modifiers.new('b', 'BOOLEAN'); md.operation = op; md.object = cutter; md.solver = 'EXACT'
    md.material_mode = 'TRANSFER'
    bpy.context.view_layer.objects.active = ob; bpy.ops.object.modifier_apply(modifier=md.name)
    bpy.data.objects.remove(cutter, do_unlink=True)
    R['parts'].append({'part': part, 'op': op, 'tris_after': sum(len(p.vertices) - 2 for p in me.polygons)})


if KIND == 'CinderHound':
    # mouth slit: the muzzle runs from the nose tip (y -0.964, z 1.09-1.14) back to the cheek (y -0.80); the slit is 1.4 cm
    # tall at the lip line z 1.115 and closes towards the cheek, where the jaw stays joined to the skull
    boolean(ellipsoid((0, -0.97, 1.115), (0.12, 0.17, 0.007), 'Ember'), 'DIFFERENCE', 'mouth slit (Ember lining)')
else:
    # mouth slot: a lens across the front at z 0.86 (the sphere's front is y -0.56 there), 9 cm tall at the centre, 46 cm deep,
    # opening over about +-0.42 m of the front
    boolean(ellipsoid((0, -0.45, 0.86), (0.45, 0.35, 0.045), 'Mouth', 48, 16), 'DIFFERENCE', 'mouth slot')
    # tongue: rooted 20 cm inside the slot's back wall (y -0.10) so a thick section crosses it, lying in the slot, tip
    # short of the lips (y -0.50)
    boolean(capsule((0, 0.10, 0.858), (0, -0.50, 0.858), 0.085, 0.017, 'Tongue'), 'UNION', 'tongue')
    for sg in (1, -1):  # eyes on the brow, half sunk into the skin, each with a pupil in front
        boolean(ellipsoid((sg * 0.22, -0.36, 1.05), (0.066, 0.066, 0.066), 'Eye', 20, 10), 'UNION', 'eye')
        boolean(ellipsoid((sg * 0.226, -0.418, 1.056), (0.028, 0.016, 0.03), 'Pupil', 16, 8), 'UNION', 'pupil')
bpy.context.view_layer.objects.active = ob; ob.select_set(True)
bpy.ops.object.mode_set(mode='EDIT'); bpy.ops.mesh.select_all(action='SELECT'); bpy.ops.mesh.quads_convert_to_tris()
bpy.ops.mesh.remove_doubles(threshold=1e-6); bpy.ops.mesh.normals_make_consistent(inside=False); bpy.ops.object.mode_set(mode='OBJECT')
bm = bmesh.new(); bm.from_mesh(me); R['nonmanifold_edges'] = sum(1 for e in bm.edges if not e.is_manifold); bm.free()
R['tris_final'] = sum(len(p.vertices) - 2 for p in me.polygons); R['materials'] = [m.name for m in me.materials]
bpy.ops.wm.save_as_mainfile(filepath=f'{out}/{name}_parts.blend')
json.dump(R, open(f'{out}/{name}_parts.json', 'w'), indent=1); print('PARTS', json.dumps(R))
