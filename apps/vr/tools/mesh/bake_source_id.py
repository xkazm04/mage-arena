# Source-id bake for a split-and-merge finish: which half of the merged source each texel of the finished bake took its
# colour from. seam_fix.py --source uses it to find the real seam (the bake's front/back switch wanders a few cm around
# the cut plane, because the remesh surface lies between the two halves' skins in the overlap band).
#
#   blender -b <finish>/<Name>_finish.blend --python-exit-code 1 -P bake_source_id.py -- <merged.glb> <out.png> <height_m>
#        [<cage_m> <ray_m>]
#
# The finished LOD0 (SM_<Name>, with its UVs) is in the .blend; the merged source is imported and posed as finish.py
# poses its HI (transforms applied, components under 0.5 % of the faces dropped, scaled to the height with the bbox
# centred and the feet at z 0; --rot 0 for a merge). Its first material (the front half's) emits grey 0.5, every other
# (the back half's) white, and it is baked selected-to-active in EMIT with finish.py's cage and ray and no margin, at the
# size of T_<Name>_BaseColor. So a texel reads 0.5 front, 1 back, 0 missed (the finish filled those from neighbours).
# The record (<out>.json) counts each class and the island texels a ray missed.
import json
import os
import sys
import time

import bpy
import mathutils
import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import components as cc  # noqa: E402

a = sys.argv[sys.argv.index('--') + 1:]
if len(a) not in (3, 5):
    raise SystemExit('usage: bake_source_id.py -- <merged.glb> <out.png> <height_m> [<cage_m> <ray_m>]')
SRC, OUT, HEIGHT = a[0], a[1], float(a[2])
CAGE, RAY = (float(a[3]), float(a[4])) if len(a) == 5 else (0.02, 0.08)
t0 = time.time()
lows = [o for o in bpy.data.objects if o.type == 'MESH' and o.name.startswith('SM_')]
if len(lows) != 1:
    raise SystemExit(f'expected one SM_ mesh in the .blend, found {[o.name for o in lows]}')
lo = lows[0]
tex = [i for i in bpy.data.images if i.name.startswith('T_')]
RES = tex[0].size[0] if tex else 2048

before = set(bpy.data.objects)
bpy.ops.import_scene.gltf(filepath=SRC, merge_vertices=True)
new = [o for o in bpy.data.objects if o not in before]
hi = [o for o in new if o.type == 'MESH']
if len(hi) != 1:
    raise SystemExit(f'{SRC}: expected one mesh, found {[o.name for o in hi]}')
hi = hi[0]
for o in new:
    if o is not hi: bpy.data.objects.remove(o, do_unlink=True)
bpy.ops.object.select_all(action='DESELECT'); hi.select_set(True); bpy.context.view_layer.objects.active = hi
bpy.ops.object.parent_clear(type='CLEAR_KEEP_TRANSFORM'); bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
hm = hi.data

# finish.py drops components under 0.5 % of the faces before it measures the bbox; do the same so the pose matches
n = len(hm.vertices); ev = np.empty(len(hm.edges) * 2, dtype=np.int64); hm.edges.foreach_get('vertices', ev)
lab = cc.labels(n, ev[0::2], ev[1::2])
fv = np.empty(len(hm.polygons), dtype=np.int64); hm.polygons.foreach_get('loop_start', fv)
lv = np.empty(len(hm.loops), dtype=np.int64); hm.loops.foreach_get('vertex_index', lv)
flab = lab[lv[fv]]; u, inv, cnt = np.unique(flab, return_inverse=True, return_counts=True)
small = cnt[inv] < 0.005 * len(hm.polygons)
import bmesh  # noqa: E402
bm = bmesh.new(); bm.from_mesh(hm); bm.faces.ensure_lookup_table()
bmesh.ops.delete(bm, geom=[bm.faces[i] for i in np.nonzero(small)[0]], context='FACES')
bmesh.ops.delete(bm, geom=[v for v in bm.verts if not v.link_faces], context='VERTS'); bm.to_mesh(hm); bm.free(); hm.update()
co = np.empty(len(hm.vertices) * 3); hm.vertices.foreach_get('co', co); co = co.reshape(-1, 3); mn, mx = co.min(0), co.max(0)
s = HEIGHT / (mx[2] - mn[2])
hm.transform(mathutils.Matrix.Scale(s, 4) @ mathutils.Matrix.Translation((-(mn[0] + mx[0]) / 2, -(mn[1] + mx[1]) / 2, -mn[2]))); hm.update()


def emit(name, v):
    m = bpy.data.materials.new(name); m.use_nodes = True; nt = m.node_tree; nt.nodes.clear()
    e = nt.nodes.new('ShaderNodeEmission'); e.inputs['Color'].default_value = (v, v, v, 1); e.inputs['Strength'].default_value = 1.0
    nt.links.new(e.outputs['Emission'], nt.nodes.new('ShaderNodeOutputMaterial').inputs['Surface'])
    return m


names = [sl.material.name if sl.material else None for sl in hi.material_slots]
front_m, back_m = emit('ID_FRONT', 0.5), emit('ID_BACK', 1.0)
for i, sl in enumerate(hi.material_slots):
    sl.material = front_m if i == 0 else back_m

img = bpy.data.images.new('srcid', RES, RES, alpha=False, float_buffer=True); img.generated_color = (0, 0, 0, 1)
bmat = bpy.data.materials.new('SRCID'); bmat.use_nodes = True
tn = bmat.node_tree.nodes.new('ShaderNodeTexImage'); tn.image = img; bmat.node_tree.nodes.active = tn
lo.data.materials.clear(); lo.data.materials.append(bmat)
sc = bpy.context.scene; sc.render.engine = 'CYCLES'; sc.cycles.device = 'CPU'; sc.cycles.samples = 1; sc.cycles.use_denoising = False
b = sc.render.bake; b.use_selected_to_active = True; b.cage_extrusion = CAGE; b.max_ray_distance = RAY; b.margin = 0; b.use_clear = True
bpy.ops.object.select_all(action='DESELECT'); hi.select_set(True); lo.select_set(True); bpy.context.view_layer.objects.active = lo
bpy.ops.object.bake(type='EMIT')

px = np.empty(RES * RES * 4, dtype=np.float32); img.pixels.foreach_get(px); v = px.reshape(RES, RES, 4)[::-1, :, 0]  # rows top-down
q = np.where(v > 0.75, 255, np.where(v > 0.25, 128, 0)).astype(np.uint8)
out_img = bpy.data.images.new('srcid8', RES, RES, alpha=False)
rgba = np.repeat(q[::-1, :, None].astype(np.float32) / 255, 4, axis=2); rgba[..., 3] = 1
out_img.pixels.foreach_set(rgba.ravel()); out_img.filepath_raw = OUT; out_img.file_format = 'PNG'
os.makedirs(os.path.dirname(os.path.abspath(OUT)), exist_ok=True); out_img.save()
R = {'source': SRC, 'out': OUT, 'res': RES, 'height_m': HEIGHT, 'cage_m': CAGE, 'ray_m': RAY, 'materials': names,
     'components_dropped': int(len(np.unique(flab[small]))), 'scale': round(float(s), 6),
     'texels_front': int((q == 128).sum()), 'texels_back': int((q == 255).sum()), 'texels_zero': int((q == 0).sum()),
     'secs': round(time.time() - t0, 1)}
with open(os.path.splitext(OUT)[0] + '.json', 'w') as fh:
    json.dump(R, fh, indent=1)
print('SOURCE_ID', json.dumps(R))
