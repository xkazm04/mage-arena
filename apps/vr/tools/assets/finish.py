# T38 Brennic: TRELLIS hi mesh -> budget LOD0, either with one flat colour per material (mode=flat, the default) or with
# the TRELLIS albedo baked onto it as a texture (mode=bake).
# Adapted from lynx-bench pof3d/round2/characters/work/finish_B.py (the round-2 best recipe):
# import with merge_vertices -> drop floaters -> scale to HEIGHT, feet at z=0, facing -Y -> voxel remesh (~1 cm)
# -> collapse to TARGET tris -> smart UV -> Cycles CPU high-to-low bake of base colour, then
#   flat: (the bake kept as a separate PNG, record only) every face takes the nearest palette colour of its baked texels,
#         smoothed over neighbours -> one material per palette entry with a flat colour, no image nodes.
#   bake: a white-emission bake of the source finds the texels the rays missed; they are filled from their neighbours;
#         an optional style clamp pulls the texture toward the palette (never flat) -> one material M_<name> whose base
#         colour is T_<name>_BaseColor.png. Seen 2026-10-24: flat finishing lost Brennic's face, brooch and straps, which
#         the raw TRELLIS texture keeps (docs/research/BRENNIC-MESH-2026-10-24.md).
#   blender -b -P finish.py -- <hi.glb> <outdir> <name> <rot_z_deg> <bake_res> <voxel_m> <target_tris> [<palette set, default Brennic>]
#        [mode=flat|bake] [clamp=none|posterize|hue|both] [strength=0.35] [levels=8] [cage=0.02] [ray=0.08] [margin=16]
# The options and the Blender-free maths live in ../mesh/finish_lib.py (tested by test_finish.py beside this file).
# Palette sets (PALETTES): PALETTE are the display colours of the concept's palette blocks, REFERENCE the same parts as
# TRELLIS bakes them, RULES re-label parts that share a colour: by height (from, to, 'below' | 'above', fraction of height)
# or by place (from, to, 'box', (xmin, xmax, zmin, zmax[, ymin, ymax]) in metres, character frame: left is +X, front is -Y). On a tie in REFERENCE the part listed first wins.
# or by an ellipse on the front (from, to, 'inside' | 'outside', (x centre, z centre, x radius, z radius)).
# Optional per set: HEIGHT (metres, default 1.8), SLAB (drop a fused ground slab of that thickness, in import units) and
# FILL (fill a star-shaped body whose skin remeshes hollow, from a centre at that fraction of the height).
# The k-means centres of the face colours go to <name>_finish.json ('face_kmeans') to tune a new set's REFERENCE.
import bpy, sys, os, json, time, bmesh, math, numpy as np, mathutils
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'mesh'))
import finish_lib as FL

a = sys.argv[sys.argv.index('--') + 1:]
(hi_glb, out, name, ROT, RES, VOX, TARGET), SET, OPT = FL.split_args(a)
ROT = float(ROT); RES = int(RES); VOX = float(VOX); TARGET = int(TARGET); MODE = OPT['mode']
HEIGHT = 1.8
PALETTES = {
    'Brennic': {
        # Display colours: the palette blocks of the Leonardo concept (concept-front-s3801-medium.jpg), sRGB.
        'PALETTE': {
            'Tunic': (0.56, 0.24, 0.20),
            'Cloak': (0.25, 0.21, 0.18),
            'Leather': (0.42, 0.28, 0.19),
            'Iron': (0.40, 0.41, 0.42),
            'Skin': (0.76, 0.54, 0.40),
            'Hair': (0.20, 0.14, 0.11),
        },
        # Classification colours: the same parts as TRELLIS bakes them (darker than the concept), sRGB, measured by k-means
        # on the seed-42 bake of 2026-10-21. A face takes the part whose reference is nearest in CIE Lab.
        'REFERENCE': {
            'Tunic': (0.34, 0.11, 0.11),
            'Cloak': (0.16, 0.125, 0.10),
            'Leather': (0.22, 0.13, 0.09),
            'Iron': (0.26, 0.22, 0.22),
            'Skin': (0.50, 0.33, 0.25),
            'Hair': (0.11, 0.075, 0.055),
        },
        # hair and cloak are both dark: above the collar it is hair, below it is cloak
        'RULES': [('Hair', 'Cloak', 'below', 0.80), ('Cloak', 'Hair', 'above', 0.86)],
    },
    # concept-front-s4101-medium.jpg (sheet 03); REFERENCE from the k-means of the seed-42 bake of 2026-10-21. The shield is the
    # same brown as the leather: it is told apart by its place (left of the body, hip to chest height).
    'Conscript': {
        'PALETTE': {
            'Mail': (0.28, 0.29, 0.33),
            'Iron': (0.55, 0.56, 0.58),
            'Linen': (0.72, 0.65, 0.52),
            'Leather': (0.38, 0.24, 0.15),
            'Wood': (0.45, 0.27, 0.17),
            'Skin': (0.76, 0.54, 0.40),
        },
        'REFERENCE': {
            'Mail': (0.25, 0.24, 0.235),
            'Iron': (0.50, 0.48, 0.46),
            'Linen': (0.58, 0.53, 0.48),
            'Leather': (0.25, 0.14, 0.085),
            'Wood': (0.25, 0.14, 0.085),
            'Skin': (0.64, 0.43, 0.36),
        },
        'RULES': [('Iron', 'Mail', 'below', 0.86), ('Leather', 'Wood', 'box', (0.17, 0.45, 0.50, 1.22)),
                  ('Mail', 'Wood', 'box', (0.25, 0.50, 0.45, 1.15)), ('Linen', 'Wood', 'box', (0.25, 0.50, 0.45, 1.15))],
    },
    # concept-front-s4201-medium.jpg (sheet 04)
    'Slinger': {
        'PALETTE': {
            'Linen': (0.78, 0.70, 0.56),
            'Jerkin': (0.27, 0.17, 0.12),
            'Iron': (0.45, 0.45, 0.46),
            'Leather': (0.45, 0.27, 0.16),
            'Bag': (0.68, 0.46, 0.25),
            'Skin': (0.76, 0.54, 0.40),
            'Hair': (0.30, 0.20, 0.14),
        },
        'REFERENCE': {
            'Linen': (0.56, 0.50, 0.40),
            'Jerkin': (0.18, 0.11, 0.08),
            'Iron': (0.32, 0.32, 0.33),
            'Leather': (0.30, 0.17, 0.10),
            'Bag': (0.48, 0.32, 0.17),
            'Skin': (0.50, 0.33, 0.25),
            'Hair': (0.20, 0.13, 0.09),
        },
        'RULES': [('Iron', 'Bag', 'below', 0.86)],  # the net pouch of stones bakes grey
    },
    # The Bout 2 beasts. A set's HEIGHT replaces the humanoid 1.8 m. The A04 sheets carry no unit: the scale is the pixel
    # scale of sheet 03, where the conscript is 1.80 m over 626 px (2.875 mm/px, the 42.6 px grid that sheets 03 and 06
    # share; sheet 05's grid is that grid halved). Cinder hound: side view of sheet 05, ear tips to paws 473 px -> 1.36 m.
    # concept-turnaround-s4302-medium.jpg (side panel) was the TRELLIS input.
    'CinderHound': {
        'HEIGHT': 1.36,
        'PALETTE': {
            'Cinder': (0.20, 0.20, 0.21),
            'Ember': (0.96, 0.45, 0.10),
            'Paw': (0.06, 0.06, 0.07),
        },
        'REFERENCE': {
            'Cinder': (0.15, 0.15, 0.15),
            'Ember': (0.733, 0.221, 0.086),
            'Paw': (0.05, 0.052, 0.054),
        },
        'RULES': [],
    },
    # Mire maw: sheet 06, top of the mound to the feet 435 px -> 1.25 m. concept-front-s4401-medium.jpg was the TRELLIS input.
    'MireMaw': {
        'HEIGHT': 1.25,
        'SLAB': 0.006,  # TRELLIS units: the seed-42 mesh stands on a 1 m square slab about 5 mm thick
        'FILL': 0.48,  # and its seed-42 skin remeshes hollow: fill it from the cavity centre at 0.48 x the height
        'PALETTE': {
            'Skin': (0.42, 0.40, 0.22),
            'Belly': (0.62, 0.55, 0.38),
            'Moss': (0.25, 0.33, 0.12),
        },
        # k-means of the seed-42 bake of 2026-10-21; the pebbles bake as moss speckle, so they take no part of their own
        'REFERENCE': {
            'Skin': (0.25, 0.23, 0.13),
            'Belly': (0.43, 0.36, 0.27),
            'Moss': (0.09, 0.15, 0.04),
        },
        # concept front: moss caps the top and covers the back, the belly is the front, the skin is the rest; the moss
        # bakes speckled, so its place decides where colour alone does not
        'RULES': [('Belly', 'Moss', 'box', (-2, 2, 0.98, 2)), ('Skin', 'Moss', 'box', (-2, 2, 1.06, 2)),
                  ('Belly', 'Moss', 'box', (-2, 2, 0.62, 2, 0.05, 2)), ('Belly', 'Skin', 'box', (-2, 2, -1, 0.62, 0.05, 2)),
                  ('Moss', 'Skin', 'below', 0.42), ('Moss', 'Skin', 'box', (-0.45, 0.45, -1, 0.98, -2, -0.35)),
                  ('Belly', 'Skin', 'below', 0.12), ('Skin', 'Moss', 'box', (-2, 2, 0.62, 2, 0.05, 2)),
                  ('Belly', 'Skin', 'outside', (0.0, 0.47, 0.40, 0.33)), ('Skin', 'Belly', 'inside', (0.0, 0.47, 0.36, 0.30))],
    },
}
# Mire maw from the open-gape master maw-open-gape-s7202 (seed 7, docs/research/CREATURE-MESH-2026-10-24.md): the MireMaw
# set without SLAB, because this mesh stands on its own feet (SLAB cut 47,679 sole faces from it). FILL stays: its back half
# is a torn shell the voxel remesh turns hollow. Palette and rules only matter to the flat mode and the clamp anchors.
PALETTES['MireMawOpenGape'] = {k: v for k, v in PALETTES['MireMaw'].items() if k != 'SLAB'}
PALETTE, REFERENCE, RULES = (PALETTES[SET][k] for k in ('PALETTE', 'REFERENCE', 'RULES'))
HEIGHT = PALETTES[SET].get('HEIGHT', HEIGHT)
R = {'mode': MODE}; T = {}; t0 = time.time()


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
if 'SLAB' in PALETTES[SET]:  # a ground slab TRELLIS fused under the feet: drop the faces that lie wholly inside the bottom band
    bm = bmesh.new(); bm.from_mesh(hm); zlo = min(v.co.z for v in bm.verts) + PALETTES[SET]['SLAB']
    kill = [f for f in bm.faces if all(v.co.z < zlo for v in f.verts)]; R['slab_faces_removed'] = len(kill)
    bmesh.ops.delete(bm, geom=kill, context='FACES'); bmesh.ops.delete(bm, geom=[v for v in bm.verts if not v.link_faces], context='VERTS')
    # the cut opens the soles; close the holes it leaves
    R['slab_holes_filled'] = len(bmesh.ops.holes_fill(bm, edges=bm.edges[:], sides=0)['faces'])
    bm.to_mesh(hm); bm.free(); hm.update()
R['hi_floaters_removed'], R['hi_components'] = drop_small_components(hm, 0.005)
co = np.empty(len(hm.vertices) * 3); hm.vertices.foreach_get('co', co); co = co.reshape(-1, 3); mn, mx = co.min(0), co.max(0)
s = HEIGHT / (mx[2] - mn[2])
hm.transform(mathutils.Matrix.Scale(s, 4) @ mathutils.Matrix.Translation((-(mn[0] + mx[0]) / 2, -(mn[1] + mx[1]) / 2, -mn[2]))); hm.update()
lap('import')

lo = bpy.data.objects.new('LOW', bpy.data.meshes.new('LOW')); bpy.context.scene.collection.objects.link(lo)
lo.data.from_pydata([v.co[:] for v in hm.vertices], [], [tuple(p.vertices) for p in hm.polygons]); lo.data.update()
bpy.ops.object.select_all(action='DESELECT'); lo.select_set(True); bpy.context.view_layer.objects.active = lo
lo.data.remesh_voxel_size = VOX; lo.data.remesh_voxel_adaptivity = 0.0; bpy.ops.object.voxel_remesh()
if PALETTES[SET].get('FILL'):
    # TRELLIS can return a skin the voxel remesh turns into a hollow shell (here 1.5-5 cm thick). For a star-shaped body,
    # cast rays from the cavity's centre (FILL x the height), put a sphere vertex midway between the inner and the outer
    # wall in every direction, join that star to the shell and remesh again: the cavity is filled and the outer
    # surface is untouched. Pockets the star misses (inside the legs) come out as small inner components, dropped below.
    from mathutils.bvhtree import BVHTree
    bvh = BVHTree.FromObject(lo, bpy.context.evaluated_depsgraph_get()); ctr = mathutils.Vector((0, 0, PALETTES[SET]['FILL'] * HEIGHT))
    st = bpy.data.objects.new('STAR', bpy.data.meshes.new('STAR')); bpy.context.scene.collection.objects.link(st)
    bm = bmesh.new(); bmesh.ops.create_uvsphere(bm, u_segments=96, v_segments=48, radius=1.0); misses = 0
    for v in bm.verts:
        d = v.co.normalized(); hits = []; o_ = ctr.copy()
        while len(hits) < 2:
            h = bvh.ray_cast(o_, d, 5.0)
            if h[0] is None: break
            hits.append((h[0] - ctr).length); o_ = h[0] + d * 1e-4
        if not hits: misses += 1; hits = [0.05]
        v.co = ctr + d * ((hits[0] + hits[1]) / 2 if len(hits) > 1 else hits[0])
    bm.to_mesh(st.data); bm.free(); R['fill_star_misses'] = misses
    # join, not a boolean (the shell touches itself): the voxel remesh unions the two volumes
    bpy.ops.object.select_all(action='DESELECT'); st.select_set(True); lo.select_set(True); bpy.context.view_layer.objects.active = lo
    bpy.ops.object.join(); lo.data.remesh_voxel_size = VOX; lo.data.remesh_voxel_adaptivity = 0.0; bpy.ops.object.voxel_remesh()
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
bpy.ops.uv.smart_project(angle_limit=math.radians(66), island_margin=0.003)
if MODE == 'bake':  # the texture ships: repack the islands tighter (smart project alone used 30 % of the square on Brennic seed 7)
    bpy.ops.uv.pack_islands(rotate=True, margin_method='FRACTION', margin=0.003, shape_method='CONCAVE')
bpy.ops.object.mode_set(mode='OBJECT'); lap('uv')
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
b = sc.render.bake; b.use_selected_to_active = True; b.cage_extrusion = OPT['cage']; b.max_ray_distance = OPT['ray']; b.margin = OPT['margin'] if MODE == 'bake' else 10; b.margin_type = 'EXTEND'; b.use_clear = True
b.use_pass_direct = False; b.use_pass_indirect = False; b.use_pass_color = True
bpy.ops.object.select_all(action='DESELECT'); hi.select_set(True); lo.select_set(True); bpy.context.view_layer.objects.active = lo
if MODE == 'bake':
    # hit mask: the same selected-to-active bake of the source in white emission, no margin. A texel a ray found the
    # source from reads 1; a texel inside an island that no ray found reads 0 (a hole the albedo bake would leave black).
    hit_img = bpy.data.images.new('hit', RES, RES, alpha=False, float_buffer=True)
    wm_ = bpy.data.materials.new('HIT'); wm_.use_nodes = True; nt_ = wm_.node_tree; nt_.nodes.clear()
    em_ = nt_.nodes.new('ShaderNodeEmission'); em_.inputs['Color'].default_value = (1, 1, 1, 1); em_.inputs['Strength'].default_value = 1.0
    nt_.links.new(em_.outputs['Emission'], nt_.nodes.new('ShaderNodeOutputMaterial').inputs['Surface'])
    keep_ = [s_.material for s_ in hi.material_slots]
    for s_ in hi.material_slots: s_.material = wm_
    hn_ = bmat.node_tree.nodes.new('ShaderNodeTexImage'); hn_.image = hit_img; bmat.node_tree.nodes.active = hn_
    b.margin = 0; t1 = time.time(); bpy.ops.object.bake(type='EMIT'); R['hit_bake_s'] = round(time.time() - t1, 1)
    for s_, m_ in zip(hi.material_slots, keep_): s_.material = m_
    bmat.node_tree.nodes.active = tn; b.margin = OPT['margin']
t1 = time.time(); bpy.ops.object.bake(type='DIFFUSE'); R['bake_s'] = round(time.time() - t1, 1)
img.filepath_raw = f'{out}/{name}_bake_basecolor.png'; img.file_format = 'PNG'; img.save()
R['bake_png'] = f'{name}_bake_basecolor.png'
lap('bake')


def finish_bake():
    """mode=bake: account for missed texels, fill them, clamp if asked, and make the texture the LOD0's one material."""
    me = lo.data
    px = np.empty(RES * RES * 4, dtype=np.float32); img.pixels.foreach_get(px); rgb = px.reshape(RES, RES, 4)[:, :, :3].astype(np.float64)
    hp = np.empty(RES * RES * 4, dtype=np.float32); hit_img.pixels.foreach_get(hp); hit = hp.reshape(RES, RES, 4)[:, :, 0]
    uv = np.empty(len(me.loops) * 2); me.uv_layers.active.data.foreach_get('uv', uv); uv = uv.reshape(-1, 2)
    ls = np.empty(len(me.polygons), dtype=np.int64); me.polygons.foreach_get('loop_start', ls)
    lv = np.empty(len(me.loops), dtype=np.int64); me.loops.foreach_get('vertex_index', lv)
    co = np.empty(len(me.vertices) * 3); me.vertices.foreach_get('co', co); co = co.reshape(-1, 3)
    corner = ls[:, None] + np.arange(3)[None, :]  # every face is a triangle here (quads_convert_to_tris above)
    cov = FL.uv_coverage(uv[corner], RES)
    miss, counts = FL.bake_misses(hit, cov)
    q = {**counts, 'dark_in_islands_before_fill': FL.dark_texels(rgb, cov), 'dark_where_hit': FL.dark_texels(rgb, cov & ~miss)}
    unfilled = rgb
    rgb, q['fill_rings'] = FL.fill_misses(rgb, miss) if counts['missed'] else (rgb, 0)
    # that fill takes colour from Blender's margin too; where the margin was unhit black it leaves black texels and darkens
    # the rest. Then every miss is filled again from the unfilled bake, from hit island texels alone, and the misses no
    # island path reaches (islands no ray hit at all) take the median hit colour
    q['black_after_ring_fill'] = FL.dark_texels(rgb, cov)
    if q['black_after_ring_fill']:
        rgb, q['refill_rings'] = FL.fill_misses(unfilled, miss, max_iter=1024, known=cov & ~miss)
        orphan = miss & FL.dark_mask(rgb); q['orphan_black_filled'] = int(orphan.sum())
        if orphan.any():
            rgb, oc = FL.fill_orphans(rgb, orphan, cov & ~miss); q['orphan_fill_colour'] = [round(float(c), 4) for c in oc]
    q['dark_in_islands_after_fill'] = FL.dark_texels(rgb, cov)
    q.update(FL.texel_density(uv[corner], co[lv[corner]], RES))
    q.update({'res': RES, 'cage_extrusion_m': OPT['cage'], 'max_ray_distance_m': OPT['ray'], 'margin_px': OPT['margin']})
    R['bake'] = q
    hit_img.filepath_raw = f'{out}/{name}_bake_hitmask.png'; hit_img.file_format = 'PNG'; hit_img.save(); R['hitmask_png'] = f'{name}_bake_hitmask.png'
    if OPT['clamp'] != 'none':
        out_rgb = FL.style_clamp(rgb, PALETTE, OPT['clamp'], OPT['strength'], OPT['levels'])
        R['clamp'] = {'clamp': OPT['clamp'], 'strength': OPT['strength'], 'levels': OPT['levels'],
                      'detail_kept': round(FL.detail_kept(rgb, out_rgb, cov), 3),
                      'mean_shift_srgb': round(float(np.abs(out_rgb - rgb)[cov].mean()), 4)}
        rgb = out_rgb
    else:
        R['clamp'] = {'clamp': 'none'}
    tex = bpy.data.images.new(f'T_{name}_BaseColor', RES, RES, alpha=False)
    tex.pixels.foreach_set(np.concatenate([rgb, np.ones((RES, RES, 1))], -1).astype(np.float32).ravel())
    tex.filepath_raw = f'{out}/T_{name}_BaseColor.png'; tex.file_format = 'PNG'; tex.save()
    me.materials.clear()
    m = bpy.data.materials.new(f'M_{name}'); m.use_nodes = True; nt = m.node_tree; pb = nt.nodes['Principled BSDF']
    tx = nt.nodes.new('ShaderNodeTexImage'); tx.image = tex; nt.links.new(tx.outputs['Color'], pb.inputs['Base Color'])
    pb.inputs['Roughness'].default_value = 0.8; pb.inputs['Metallic'].default_value = 0.0
    mean = FL.srgb_to_linear(rgb[cov].mean(0)); m.diffuse_color = (*mean, 1)
    me.materials.append(m); me.polygons.foreach_set('material_index', np.zeros(len(me.polygons), dtype=np.int32)); me.update()
    R['texture_png'] = f'T_{name}_BaseColor.png'
    R['materials'] = {f'M_{name}': {'texture': R['texture_png'], 'faces': len(me.polygons)}}
    lap('texture')


if MODE == 'bake':
    finish_bake(); me = lo.data
else:
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
    # parts that share a colour are told apart by height (RULES)
    cz = np.empty(len(me.polygons) * 3); me.polygons.foreach_get('center', cz); cz = cz.reshape(-1, 3)[:, 2]
    cx = np.empty(len(me.polygons) * 3); me.polygons.foreach_get('center', cx); cy = cx.reshape(-1, 3)[:, 1]; cx = cx.reshape(-1, 3)[:, 0]
    for p_from, p_to, where, frac in RULES:
        if where == 'box':
            hit = (cx >= frac[0]) & (cx <= frac[1]) & (cz >= frac[2]) & (cz <= frac[3])
            if len(frac) > 4: hit &= (cy >= frac[4]) & (cy <= frac[5])
        elif where in ('inside', 'outside'):  # an ellipse on the front half, seen from the front: (x centre, z centre, x radius, z radius)
            e = ((cx - frac[0]) / frac[2]) ** 2 + ((cz - frac[1]) / frac[3]) ** 2
            hit = (e <= 1) & (cy < 0) if where == 'inside' else (e > 1) | (cy >= 0)
        else: hit = (cz < frac * HEIGHT) if where == 'below' else (cz > frac * HEIGHT)
        sel = (lab == names.index(p_from)) & hit
        lab[sel] = names.index(p_to)
    # k-means centres of the face colours (tunes REFERENCE for a new palette set)
    rng = np.random.default_rng(1); kc = srgb[rng.choice(len(srgb), len(names) + 2, replace=False)]
    for _ in range(25):
        asg = ((srgb[:, None, :] - kc[None]) ** 2).sum(2).argmin(1)
        kc = np.array([srgb[asg == k].mean(0) if (asg == k).any() else kc[k] for k in range(len(kc))])
    R['face_kmeans'] = [{'srgb': [round(float(x), 3) for x in kc[k]], 'faces': int((asg == k).sum())} for k in np.argsort(-np.bincount(asg, minlength=len(kc)))]
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
