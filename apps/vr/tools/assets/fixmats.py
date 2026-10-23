# T15: clean up speckle in the face materials of a rigged soldier (the Slinger's pale blotches on the satchel and the boots).
# finish.py labels each face from its baked texels; a few faces of a boot or of the satchel land on the wrong palette entry. Here every
# connected patch of one material of at most MAXF faces, inside the REGIONS below, takes the material it shares most edges with.
# Material indices only: no vertex, weight or bone changes. Run on <name>_rigged.blend before weapon.py.
#   blender -b -P fixmats.py -- <in.blend> <out.blend> <name> [<max patch faces, default 60>]
import bpy, sys, json, bmesh, numpy as np

a = sys.argv[sys.argv.index('--') + 1:]; src, dst, name = a[:3]; MAXF = int(a[3]) if len(a) > 3 else 60
bpy.ops.wm.open_mainfile(filepath=src)
me = bpy.data.objects['SM_' + name].data
mats = [m.name.split('_')[-1] for m in me.materials]
n = len(me.polygons); c = np.empty(n * 3); me.polygons.foreach_get('center', c); c = c.reshape(-1, 3)
mi = np.empty(n, dtype=np.int64); me.polygons.foreach_get('material_index', mi)
bag = c[mi == mats.index('Bag')]; bag = bag[bag[:, 2] > 0.6]
lo, hi = bag.min(0) - 0.03, bag.max(0) + 0.03
region = (c[:, 2] < 0.40) | ((c >= lo) & (c <= hi)).all(1)
KEEP = {mats.index(m) for m in ('Iron', 'Jerkin', 'Hair') if m in mats}  # real small details: rivets, buckle, hair
bm = bmesh.new(); bm.from_mesh(me); bm.faces.ensure_lookup_table()
comp = -np.ones(n, dtype=np.int64); patches = []
for f in bm.faces:
    if comp[f.index] >= 0: continue
    st = [f]; comp[f.index] = len(patches); mem = []
    while st:
        x = st.pop(); mem.append(x.index)
        for e in x.edges:
            for y in e.link_faces:
                if comp[y.index] < 0 and mi[y.index] == mi[f.index]: comp[y.index] = len(patches); st.append(y)
    patches.append(mem)
changed = 0
for mem in patches:
    m0 = mi[mem[0]]
    if len(mem) > MAXF or m0 in KEEP or not region[mem].all(): continue
    cnt = {}
    for i in mem:
        for e in bm.faces[i].edges:
            for y in e.link_faces:
                if mi[y.index] != m0: cnt[mi[y.index]] = cnt.get(mi[y.index], 0) + 1
    if cnt: mi[mem] = max(cnt, key=cnt.get); changed += len(mem)
me.polygons.foreach_set('material_index', mi.astype(np.int32)); me.update()
bpy.ops.wm.save_as_mainfile(filepath=dst); print('FIXMATS', json.dumps({'faces_relabelled': changed, 'patches': len(patches), 'max_patch_faces': MAXF}))
