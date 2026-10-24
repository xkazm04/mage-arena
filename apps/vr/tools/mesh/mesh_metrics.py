# Mesh card, extraction half: import one GLB, FBX or .blend headless and write its structural numbers as JSON. It
# decides nothing; mesh-card.mjs scores the numbers (gate-lib.mjs). Vertices are welded by exact position, so a UV or
# normal seam does not split a component (the weld of apps/vr/tools/assets/metrics.py), and the import itself merges
# nothing. Every mesh object is measured together in world space, at rest: modifiers (an armature's pose) are not
# applied, as in metrics.py.
#   blender -b --factory-startup -P mesh_metrics.py -- <in.glb|in.fbx|in.blend> <out.json>
# Units: Blender metres after the importer's axis conversion (Z up). The histogram is per connected component, face
# counts largest first, capped at KEEP with the omitted count beside it.
import bpy, sys, os, json, numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import components  # noqa: E402

KEEP = 64
a = sys.argv[sys.argv.index('--') + 1:]
src, outp = a[:2]
ext = os.path.splitext(src)[1].lower()
if ext == '.blend':
    bpy.ops.wm.open_mainfile(filepath=os.path.abspath(src))
else:
    bpy.ops.wm.read_factory_settings(use_empty=True)
    if ext in ('.glb', '.gltf'):
        bpy.ops.import_scene.gltf(filepath=os.path.abspath(src))
    elif ext == '.fbx':
        bpy.ops.import_scene.fbx(filepath=os.path.abspath(src))
    else:
        raise SystemExit(f'unsupported mesh type {ext}')

objs = [o for o in bpy.context.scene.objects if o.type == 'MESH']
cos, tris, off = [], [], 0
for o in objs:
    me = o.data
    me.calc_loop_triangles()
    co = np.empty(len(me.vertices) * 3); me.vertices.foreach_get('co', co); co = co.reshape(-1, 3)
    mw = np.array(o.matrix_world); co = co @ mw[:3, :3].T + mw[:3, 3]
    tr = np.empty(len(me.loop_triangles) * 3, dtype=np.int64); me.loop_triangles.foreach_get('vertices', tr)
    cos.append(co); tris.append(tr.reshape(-1, 3) + off); off += len(co)
co = np.concatenate(cos) if cos else np.zeros((0, 3))
F = np.concatenate(tris) if tris else np.zeros((0, 3), dtype=np.int64)

R = {'file': {'path': os.path.abspath(src), 'type': ext.lstrip('.'), 'bytes': os.path.getsize(src)},
     'blender': bpy.app.version_string, 'objects': len(objs), 'units': 'm, Z up', 'tris': int(len(F))}
if len(F):
    _, inv = np.unique(co.astype(np.float32), axis=0, return_inverse=True); inv = inv.ravel()
    V = np.zeros((inv.max() + 1, 3)); V[inv] = co; F = inv[F]
    used = np.unique(F)
    e = np.vstack([F[:, [0, 1]], F[:, [1, 2]], F[:, [2, 0]]])
    es = np.sort(e, axis=1); key = es[:, 0] * (len(V) + 1) + es[:, 1]
    uk, einv, cnt = np.unique(key, return_inverse=True, return_counts=True)
    # winding: an edge shared by two faces is consistent when the faces run it in opposite directions
    two = np.flatnonzero(cnt == 2); fwd = e[:, 0] < e[:, 1]; wrong = 0
    if len(two):
        order = np.argsort(einv, kind='stable'); starts = np.searchsorted(einv[order], two)
        wrong = int((fwd[order[starts]] == fwd[order[starts + 1]]).sum())
    lab = components.labels(len(V), es[:, 0], es[:, 1])[F[:, 0]]
    hist = [s for _, s in components.sizes(lab)]
    v0, v1, v2 = V[F[:, 0]], V[F[:, 1]], V[F[:, 2]]
    area = np.linalg.norm(np.cross(v1 - v0, v2 - v0), axis=1) / 2
    mn, mx = V[used].min(0), V[used].max(0); diag = float(np.linalg.norm(mx - mn))
    R.update(verts=int(len(used)), edges=int(len(uk)), boundaryEdges=int((cnt == 1).sum()), nonManifoldEdges=int((cnt > 2).sum()),
             watertight=bool((cnt == 2).all()), windingInconsistentEdges=wrong,
             degenerateFaces=int((area <= (diag * 1e-6) ** 2).sum()), areaM2=round(float(area.sum()), 5),
             components=len(hist), componentFaces=hist[:KEEP], componentFacesOmitted=max(0, len(hist) - KEEP),
             bboxMin=[round(float(x), 5) for x in mn], bboxMax=[round(float(x), 5) for x in mx],
             extentM=[round(float(x), 5) for x in mx - mn], heightM=round(float(mx[2] - mn[2]), 5),
             minZ=round(float(mn[2]), 5), centreXY=[round(float((mn[i] + mx[i]) / 2), 5) for i in (0, 1)])
json.dump(R, open(outp, 'w'), indent=1)
print('MESH_METRICS', json.dumps({k: R.get(k) for k in ('tris', 'components', 'watertight', 'heightM', 'minZ')}))
