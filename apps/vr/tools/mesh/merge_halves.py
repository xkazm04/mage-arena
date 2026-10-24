# Split-and-merge for a creature whose front TRELLIS mesh has a torn back: keep the front half of the mesh reconstructed
# from the front image and the back half of the mesh reconstructed from a back image, and join them into one textured
# source GLB for finish.mjs. First used on the mire maw (docs/research/MAW-MV-2026-10-24.md).
#
#   blender -b --factory-startup --python-exit-code 1 -P merge_halves.py -- <front.glb> <back.glb> <out.glb> <rot_front_deg>
#        <rot_back_deg> <cut_y_m> <overlap_m> <height_m> [<record.json>]
#
# Both meshes are posed in finish.py's frame (facing -Y, feet at z 0, centred, the class height) by halves_lib.fit_halves:
# rot_front turns the front mesh so its face looks down -Y; rot_back turns the back mesh so its back-image side looks
# down +Y (a turn, never a reflection), and the back is scaled to the front's height and percentile footprint. Each half
# is cut at y = cut +- overlap (bmesh bisect) and its cut is capped, so each is as closed as its source; the two overlap
# across the seam. They are joined, not booleaned: finish.py's voxel remesh (and its FILL star) is the merge, and its
# finished card is the watertight check. The joined source keeps both TRELLIS textures, so the bake takes the front's
# colours on the front and the back's on the back. The record also carries a voxel trial of the join at 1 cm.
# Run finish.mjs on the result with --rot 0: the posing is done here.
import json
import os
import sys
import time

import bmesh
import bpy
import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import components as cc  # noqa: E402
import halves_lib as hl  # noqa: E402

A = hl.parse_args(sys.argv[sys.argv.index('--') + 1:])
t0 = time.time()
bpy.ops.wm.read_factory_settings(use_empty=True)


def load(path, name):
    before = set(bpy.data.objects)
    bpy.ops.import_scene.gltf(filepath=path, merge_vertices=True)
    new = [o for o in bpy.data.objects if o not in before]
    ms = [o for o in new if o.type == 'MESH']
    if len(ms) != 1:
        raise SystemExit(f'{path}: expected one mesh, found {[o.name for o in ms]}')
    o = ms[0]; o.name = name
    bpy.ops.object.select_all(action='DESELECT'); o.select_set(True); bpy.context.view_layer.objects.active = o
    bpy.ops.object.parent_clear(type='CLEAR_KEEP_TRANSFORM'); bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    for x in new:
        if x is not o: bpy.data.objects.remove(x, do_unlink=True)
    return o


def coords(o):
    co = np.empty(len(o.data.vertices) * 3); o.data.vertices.foreach_get('co', co)
    return co.reshape(-1, 3)


def set_coords(o, co):
    o.data.vertices.foreach_set('co', np.ascontiguousarray(co, dtype=np.float64).ravel()); o.data.update()


def cut_half(o, side):
    """Bisect at the half's own plane (cut + overlap for the front, cut - overlap for the back), drop the far side, cap
    the cut's open loops. Returns (tris kept, cap faces)."""
    y = A['cut'] + A['overlap'] if side == 'front' else A['cut'] - A['overlap']
    bm = bmesh.new(); bm.from_mesh(o.data)
    r = bmesh.ops.bisect_plane(bm, geom=bm.verts[:] + bm.edges[:] + bm.faces[:], plane_co=(0, y, 0),
                               plane_no=(0, 1, 0), clear_outer=(side == 'front'), clear_inner=(side == 'back'))
    cut_edges = [e for e in r['geom_cut'] if isinstance(e, bmesh.types.BMEdge) and e.is_valid and e.is_boundary]
    caps = bmesh.ops.holes_fill(bm, edges=cut_edges, sides=0)['faces']
    n_caps = len(bmesh.ops.triangulate(bm, faces=caps)['faces']) if caps else 0
    bmesh.ops.delete(bm, geom=[v for v in bm.verts if not v.link_faces], context='VERTS')
    bm.to_mesh(o.data); bm.free(); o.data.update()
    return sum(len(p.vertices) - 2 for p in o.data.polygons), n_caps


def mesh_stats(me):
    """Triangles, components (by shared vertex) and edges not shared by exactly two faces."""
    n = len(me.vertices); ev = np.empty(len(me.edges) * 2, dtype=np.int64); me.edges.foreach_get('vertices', ev)
    lab = cc.labels(n, ev[0::2], ev[1::2]) if n else np.zeros(0, dtype=np.int64)
    used = np.zeros(n, dtype=bool); lv = np.empty(len(me.loops), dtype=np.int64); me.loops.foreach_get('vertex_index', lv); used[lv] = True
    bm = bmesh.new(); bm.from_mesh(me); bad = sum(1 for e in bm.edges if len(e.link_faces) != 2); bm.free()
    return {'tris': sum(len(p.vertices) - 2 for p in me.polygons), 'components': int(len(np.unique(lab[used]))),
            'non_manifold_or_boundary_edges': bad}


front = load(A['front'], 'FRONT'); back = load(A['back'], 'BACK')
R = {'front_glb': A['front'], 'back_glb': A['back'], 'cut_y_m': A['cut'], 'overlap_m': A['overlap'],
     'front_tris_in': sum(len(p.vertices) - 2 for p in front.data.polygons), 'back_tris_in': sum(len(p.vertices) - 2 for p in back.data.polygons)}
mf, mb, R['fit'] = hl.fit_halves(coords(front), coords(back), A['rot_front'], A['rot_back'], A['height'])
set_coords(front, hl.apply(mf, coords(front))); set_coords(back, hl.apply(mb, coords(back)))
if np.linalg.det(mb[:3, :3]) < 0 or np.linalg.det(mf[:3, :3]) < 0:
    raise SystemExit('a pose matrix reflects; refusing (halves_lib turns, never mirrors)')
R['front_tris_kept'], R['front_cap_faces'] = cut_half(front, 'front')
R['back_tris_kept'], R['back_cap_faces'] = cut_half(back, 'back')
for o in (front, back):
    if not o.data.polygons: raise SystemExit(f'{o.name}: the cut left nothing; check cut_y_m and the rotations')
bpy.ops.object.select_all(action='DESELECT'); front.select_set(True); back.select_set(True); bpy.context.view_layer.objects.active = front
bpy.ops.object.join(); merged = front; merged.name = 'MERGED'
R['merged'] = mesh_stats(merged.data); R['materials'] = [m.name for m in merged.data.materials]
co = coords(merged); R['merged_extent_m'] = [round(float(x), 4) for x in co.max(0) - co.min(0)]

# voxel trial: what the finish's first remesh will see (a copy, not exported)
trial = merged.copy(); trial.data = merged.data.copy(); bpy.context.scene.collection.objects.link(trial)
bpy.ops.object.select_all(action='DESELECT'); trial.select_set(True); bpy.context.view_layer.objects.active = trial
trial.data.remesh_voxel_size = 0.01; trial.data.remesh_voxel_adaptivity = 0.0; bpy.ops.object.voxel_remesh()
R['voxel_trial_1cm'] = mesh_stats(trial.data); bpy.data.objects.remove(trial, do_unlink=True)

bpy.ops.object.select_all(action='DESELECT'); merged.select_set(True); bpy.context.view_layer.objects.active = merged
os.makedirs(os.path.dirname(os.path.abspath(A['out'])), exist_ok=True)
bpy.ops.export_scene.gltf(filepath=A['out'], export_format='GLB', use_selection=True, export_apply=True, export_image_format='AUTO')
R['out_glb'] = A['out']; R['out_bytes'] = os.path.getsize(A['out']); R['secs'] = round(time.time() - t0, 1)
with open(A['record'], 'w') as fh: json.dump(R, fh, indent=1)
print('MERGE_HALVES', json.dumps(R))
