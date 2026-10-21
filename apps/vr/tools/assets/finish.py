# T38 Brennic: TRELLIS hi mesh -> budget LOD0 with one flat colour per material.
# Adapted from lynx-bench pof3d/round2/characters/work/finish_B.py (the round-2 best recipe):
# import with merge_vertices -> drop floaters -> scale to HEIGHT, feet at z=0, facing -Y -> voxel remesh (~1 cm)
# -> collapse to TARGET tris -> smart UV -> Cycles CPU high-to-low bake of base colour (kept as a separate PNG, record only)
# -> every face takes the nearest palette colour of its baked texels, smoothed over neighbours -> one material per palette
# entry with a flat colour, no image nodes.
#   blender -b -P finish.py -- <hi.glb> <outdir> <name> <rot_z_deg> <bake_res> <voxel_m> <target_tris>
import bpy, sys, json, time, bmesh, math, numpy as np, mathutils

a = sys.argv[sys.argv.index('--') + 1:]
hi_glb, out, name = a[:3]; ROT = float(a[3]); RES = int(a[4]); VOX = float(a[5]); TARGET = int(a[6])
HEIGHT = 1.8
# Display colours: the palette blocks of the Leonardo concept (concept-front-s3801-medium.jpg), sRGB.
PALETTE = {
    'Tunic': (0.56, 0.24, 0.20),
    'Cloak': (0.25, 0.21, 0.18),
    'Leather': (0.42, 0.28, 0.19),
    'Iron': (0.40, 0.41, 0.42),
    'Skin': (0.76, 0.54, 0.40),
    'Hair': (0.20, 0.14, 0.11),
}
# Classification colours: the same parts as TRELLIS bakes them (darker than the concept), sRGB, measured by k-means
# on the seed-42 bake of 2026-10-21. A face takes the part whose reference is nearest in CIE Lab.
REFERENCE = {
    'Tunic': (0.34, 0.11, 0.11),
    'Cloak': (0.16, 0.125, 0.10),
    'Leather': (0.22, 0.13, 0.09),
    'Iron': (0.26, 0.22, 0.22),
    'Skin': (0.50, 0.33, 0.25),
    'Hair': (0.11, 0.075, 0.055),
}
R = {}; T = {}; t0 = time.time()


def lap(k):
    global t0
    T[k] = round(time.time() - t0, 1); t0 = time.time(); print('LAP', k, T[k], flush=True)


def tris(o):
    return sum(len(p.vertices) - 2 for p in o.data.polygons)


def drop_small_components(me, frac):
    bm = bmesh.new(); bm.from_mesh(me); bm.faces.ensure_lookup_table(); comp = {}; sizes = []
    for f in bm.faces:
        if f.index in comp: continue
        cid = len(sizes); st = [f]; comp[f.index] = cid; n = 0
        while st:
            x = st.pop(); n += 1
            for e in x.edges:
                for y in e.link_faces:
                    if y.index not in comp: comp[y.index] = cid; st.append(y)
        sizes.append(n)
    tot = len(bm.faces); kill = [f for f in bm.faces if sizes[comp[f.index]] < frac * tot]
    removed = sum(1 for s in sizes if s < frac * tot)
    bmesh.ops.delete(bm, geom=kill, context='FACES'); bmesh.ops.delete(bm, geom=[v for v in bm.verts if not v.link_faces], context='VERTS')
    bm.to_mesh(me); bm.free(); me.update()
    return removed, len(sizes)


bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.gltf(filepath=hi_glb, merge_vertices=True)
hi = [o for o in bpy.context.scene.objects if o.type == 'MESH'][0]; hi.name = 'HI'
for o in list(bpy.context.scene.objects):
    if o is not hi and o.type != 'MESH': bpy.data.objects.remove(o, do_unlink=True)
bpy.ops.object.select_all(action='DESELECT'); hi.select_set(True); bpy.context.view_layer.objects.active = hi
bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
hm = hi.data
if ROT: hm.transform(mathutils.Matrix.Rotation(math.radians(ROT), 4, 'Z'))
R['hi_tris'] = tris(hi)
R['hi_floaters_removed'], R['hi_components'] = drop_small_components(hm, 0.005)
co = np.empty(len(hm.vertices) * 3); hm.vertices.foreach_get('co', co); co = co.reshape(-1, 3); mn, mx = co.min(0), co.max(0)
s = HEIGHT / (mx[2] - mn[2])
hm.transform(mathutils.Matrix.Scale(s, 4) @ mathutils.Matrix.Translation((-(mn[0] + mx[0]) / 2, -(mn[1] + mx[1]) / 2, -mn[2]))); hm.update()
lap('import')

lo = bpy.data.objects.new('LOW', bpy.data.meshes.new('LOW')); bpy.context.scene.collection.objects.link(lo)
lo.data.from_pydata([v.co[:] for v in hm.vertices], [], [tuple(p.vertices) for p in hm.polygons]); lo.data.update()
bpy.ops.object.select_all(action='DESELECT'); lo.select_set(True); bpy.context.view_layer.objects.active = lo
lo.data.remesh_voxel_size = VOX; lo.data.remesh_voxel_adaptivity = 0.0; bpy.ops.object.voxel_remesh()
R['voxel_m'] = VOX; R['voxel_tris'] = tris(lo)
if tris(lo) > TARGET:
    md = lo.modifiers.new('d', 'DECIMATE'); md.decimate_type = 'COLLAPSE'; md.ratio = min(1.0, TARGET / tris(lo)); md.use_collapse_triangulate = True
    bpy.ops.object.modifier_apply(modifier='d')
bpy.ops.object.mode_set(mode='EDIT'); bpy.ops.mesh.select_all(action='SELECT'); bpy.ops.mesh.quads_convert_to_tris()
bpy.ops.mesh.remove_doubles(threshold=1e-6); bpy.ops.mesh.delete_loose(); bpy.ops.mesh.normals_make_consistent(inside=False); bpy.ops.object.mode_set(mode='OBJECT')
R['low_floater_components_removed'], _ = drop_small_components(lo.data, 0.01)
bm = bmesh.new(); bm.from_mesh(lo.data); R['low_nonmanifold_edges'] = sum(1 for e in bm.edges if not e.is_manifold); bm.free()
R['low_tris'] = tris(lo); bpy.ops.object.shade_smooth()
lap('lowpoly')

bpy.ops.object.mode_set(mode='EDIT'); bpy.ops.mesh.select_all(action='SELECT')
bpy.ops.uv.smart_project(angle_limit=math.radians(66), island_margin=0.003); bpy.ops.object.mode_set(mode='OBJECT'); lap('uv')
for m_ in hi.data.materials:
    for n_ in m_.node_tree.nodes:
        if n_.type == 'BSDF_PRINCIPLED':
            for k_ in ('Metallic', 'Transmission Weight', 'Emission Strength'):
                if k_ in n_.inputs:
                    for l_ in list(n_.inputs[k_].links): m_.node_tree.links.remove(l_)
                    n_.inputs[k_].default_value = 0.0
img = bpy.data.images.new('bc', RES, RES, alpha=False); img.generated_color = (0.1, 0.1, 0.1, 1)
bmat = bpy.data.materials.new('BAKE'); bmat.use_nodes = True; lo.data.materials.append(bmat)
tn = bmat.node_tree.nodes.new('ShaderNodeTexImage'); tn.image = img; bmat.node_tree.nodes.active = tn
sc = bpy.context.scene; sc.render.engine = 'CYCLES'; sc.cycles.device = 'CPU'; sc.cycles.samples = 1; sc.cycles.use_denoising = False
b = sc.render.bake; b.use_selected_to_active = True; b.cage_extrusion = 0.02; b.max_ray_distance = 0.08; b.margin = 10; b.margin_type = 'EXTEND'; b.use_clear = True
b.use_pass_direct = False; b.use_pass_indirect = False; b.use_pass_color = True
bpy.ops.object.select_all(action='DESELECT'); hi.select_set(True); lo.select_set(True); bpy.context.view_layer.objects.active = lo
t1 = time.time(); bpy.ops.object.bake(type='DIFFUSE'); R['bake_s'] = round(time.time() - t1, 1)
img.filepath_raw = f'{out}/{name}_bake_basecolor.png'; img.file_format = 'PNG'; img.save()
R['bake_png'] = f'{name}_bake_basecolor.png'
lap('bake')

# ---- palette segmentation: per face, the median baked colour of its 3 corner UVs + centroid, nearest palette entry
px = np.empty(RES * RES * 4, dtype=np.float32); img.pixels.foreach_get(px); px = px.reshape(RES, RES, 4)[:, :, :3]  # byte sRGB image: values are sRGB
me = lo.data; uvl = me.uv_layers.active.data
uv = np.empty(len(me.loops) * 2); uvl.foreach_get('uv', uv); uv = uv.reshape(-1, 2)
ls = np.empty(len(me.polygons), dtype=np.int64); me.polygons.foreach_get('loop_start', ls)
samples = []
for k in range(3):
    samples.append(uv[ls + k])
samples.append(sum(samples) / 3)
cols = []
for u in samples:
    ix = np.clip((u[:, 0] * RES).astype(int), 0, RES - 1); iy = np.clip((u[:, 1] * RES).astype(int), 0, RES - 1)
    cols.append(px[iy, ix])
srgb = np.median(np.stack(cols), axis=0)


def to_lab(c):
    c = np.asarray(c, dtype=np.float64); l_ = np.where(c <= 0.04045, c / 12.92, ((c + 0.055) / 1.055) ** 2.4)
    M = np.array([[0.4124, 0.3576, 0.1805], [0.2126, 0.7152, 0.0722], [0.0193, 0.1192, 0.9505]])
    xyz = l_ @ M.T / np.array([0.9505, 1.0, 1.089])
    f_ = np.where(xyz > 0.008856, np.cbrt(xyz), 7.787 * xyz + 16 / 116)
    return np.stack([116 * f_[..., 1] - 16, 500 * (f_[..., 0] - f_[..., 1]), 200 * (f_[..., 1] - f_[..., 2])], -1)


names = list(PALETTE); P = to_lab([REFERENCE[n] for n in names])
d = ((to_lab(srgb)[:, None, :] - P[None, :, :]) ** 2).sum(2); lab = d.argmin(1)
# hair and cloak are both dark: above the collar it is hair, below it is cloak
cz = np.empty(len(me.polygons) * 3); me.polygons.foreach_get('center', cz); cz = cz.reshape(-1, 3)[:, 2]
hair, cloak = names.index('Hair'), names.index('Cloak')
lab[(lab == hair) & (cz < 0.80 * HEIGHT)] = cloak
lab[(lab == cloak) & (cz > 0.86 * HEIGHT)] = hair
# majority smoothing over edge neighbours removes speckle
bm = bmesh.new(); bm.from_mesh(me); bm.faces.ensure_lookup_table()
nbr = [[y.index for e in f.edges for y in e.link_faces if y.index != f.index] for f in bm.faces]; bm.free()
for _ in range(6):
    new = lab.copy()
    for i, nb in enumerate(nbr):
        if not nb: continue
        c = np.bincount(lab[nb], minlength=len(names))
        if c.max() >= 2 and c.argmax() != lab[i]: new[i] = c.argmax()
    lab = new
me.materials.clear()
used = [i for i in range(len(names)) if (lab == i).any()]
slot = {}
for i in used:
    m = bpy.data.materials.new(f'M_{name}_{names[i]}'); m.use_nodes = True
    pb = m.node_tree.nodes['Principled BSDF']; c = PALETTE[names[i]]
    lc = [((x + 0.055) / 1.055) ** 2.4 if x > 0.04045 else x / 12.92 for x in c]
    pb.inputs['Base Color'].default_value = (*lc, 1); pb.inputs['Roughness'].default_value = 0.8; pb.inputs['Metallic'].default_value = 0.0
    m.diffuse_color = (*lc, 1)
    slot[i] = len(me.materials); me.materials.append(m)
me.polygons.foreach_set('material_index', np.array([slot[i] for i in lab], dtype=np.int32)); me.update()
R['materials'] = {f'M_{name}_{names[i]}': {'srgb': PALETTE[names[i]], 'faces': int((lab == i).sum())} for i in used}
bpy.data.objects.remove(hi, do_unlink=True)
co = np.empty(len(me.vertices) * 3); me.vertices.foreach_get('co', co); co = co.reshape(-1, 3)
R['height_m'] = round(float(co[:, 2].max() - co[:, 2].min()), 4); R['extent'] = [round(float(x), 3) for x in co.max(0) - co.min(0)]
lo.name = 'SM_' + name; R['tris_final'] = tris(lo); R['verts_final'] = len(me.vertices); R['secs'] = T; R['bake_res'] = RES
bpy.ops.wm.save_as_mainfile(filepath=f'{out}/{name}_finish.blend')
json.dump(R, open(f'{out}/{name}_finish.json', 'w'), indent=1); print('FINISH', json.dumps(R))
