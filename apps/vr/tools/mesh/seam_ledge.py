# The geometric ledge of a split-and-merge finish at the cut: measure it on orthographic side depth maps, or smooth it
# away on the finished LOD0 (positions only: triangles, UVs and the baked texture unchanged). ledge_lib.py has the logic
# and its tests; README "The seam ledge" has the run of record.
#
#   blender -b <finish>/<Name>_finish.blend --python-exit-code 1 -P seam_ledge.py -- measure <out.json> [<centre_m> <half_m>]
#   blender -b <finish>/<Name>_finish.blend --python-exit-code 1 -P seam_ledge.py -- smooth <out.blend> <record.json>
#        [<centre_m> <core_m> <edge_m> <iters>]
#
# measure: rays along -X and +X every 2 mm (500 px/m, the critic renders' scale) over y and z hit SM_<Name>; per flank
# the depth map (the outermost |x|) and normals, then ledge_lib.flank_report on the band |y - centre| <= half (default
# the 2026-10-24 maw: cut -0.30, half 0.15, the overlap planes at -0.35 and -0.25 inside it). The depth maps go to
# <out>.npz beside the JSON.
# smooth: ledge_lib.taubin on SM_<Name>'s vertices with ledge_lib.band_weight(y, centre, core, edge) (default -0.32,
# 0.12, 0.24, 120 pairs: the 2026-10-26 maw's run of record, after 40 and 80 pairs on narrower bands left the left
# flank's ledge), the top 3 cm pinned, then saves the .blend under a new name (the texture stays an external file: copy
# T_<Name>_BaseColor.png beside it, as the rig step expects). The record counts the moved vertices, the largest move and
# the folded faces (normals turned past 90 degrees).
import json
import os
import sys
import time

import bpy
import numpy as np
from mathutils import Vector
from mathutils.bvhtree import BVHTree

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ledge_lib as ll  # noqa: E402

STEP = 0.002  # 500 px/m
PIN_TOP = 0.03  # vertices within 3 cm of the top are not moved, so the height (1.2509 m on the maw) holds
a = sys.argv[sys.argv.index('--') + 1:]
if not a or a[0] not in ('measure', 'smooth'):
    raise SystemExit('usage: seam_ledge.py -- measure <out.json> [<centre> <half>] | smooth <out.blend> <record.json> [<centre> <core> <edge> <iters>]')
t0 = time.time()
sm = [o for o in bpy.context.scene.objects if o.type == 'MESH' and o.name.startswith('SM_')]
if len(sm) != 1:
    raise SystemExit(f'expected one SM_ mesh, found {[o.name for o in sm]}')
ob = sm[0]; me = ob.data
co = np.empty(len(me.vertices) * 3); me.vertices.foreach_get('co', co); co = co.reshape(-1, 3)
mw = np.array(ob.matrix_world)
if not np.allclose(mw, np.eye(4)):
    raise SystemExit(f'{ob.name} carries a transform; the finish applies it, so this is not a finish .blend')
me.calc_loop_triangles(); tris = np.empty(len(me.loop_triangles) * 3, dtype=np.int64); me.loop_triangles.foreach_get('vertices', tris)
tris = tris.reshape(-1, 3)

if a[0] == 'measure':
    out = a[1]; centre = float(a[2]) if len(a) > 2 else -0.30; half = float(a[3]) if len(a) > 3 else 0.15
    bvh = BVHTree.FromPolygons([tuple(v) for v in co], [tuple(t) for t in tris])
    lo, hi = co.min(0), co.max(0)
    ys = np.arange(np.floor(lo[1] / STEP) * STEP, hi[1] + STEP, STEP)
    zs = np.arange(STEP / 2, hi[2] + STEP, STEP)
    R = {'mesh': ob.name, 'tris': len(tris), 'step_m': STEP, 'centre_m': centre, 'half_m': half, 'flanks': {}}
    maps = {}
    for side, sx in (('right', 1.0), ('left', -1.0)):
        dep = np.full((len(zs), len(ys)), np.nan); nor = np.zeros((len(zs), len(ys), 3))
        x0 = sx * (abs(hi[0]) + abs(lo[0]) + 1.0); d = Vector((-sx, 0, 0))
        for i, z in enumerate(zs):
            for j, y in enumerate(ys):
                p, n, _, _ = bvh.ray_cast(Vector((x0, y, z)), d)
                if p is not None:
                    dep[i, j] = sx * p.x; nor[i, j] = n
        R['flanks'][side] = ll.flank_report(dep, nor, ys, zs, centre, half)
        R['flanks'][side]['hit_px'] = int(np.isfinite(dep).sum())
        maps[f'{side}_depth'] = dep; maps[f'{side}_normal'] = nor.astype(np.float32)
    np.savez_compressed(os.path.splitext(out)[0] + '.npz', y=ys, z=zs, **maps)
else:
    out_blend, rec = a[1], a[2]
    centre, core, edge, iters = (float(a[3]), float(a[4]), float(a[5]), int(a[6])) if len(a) > 6 else (-0.32, 0.12, 0.24, 120)
    ev = np.empty(len(me.edges) * 2, dtype=np.int64); me.edges.foreach_get('vertices', ev)
    w = ll.pin_top(co[:, 2], ll.band_weight(co[:, 1], centre, core, edge), PIN_TOP)
    co1 = ll.taubin(co, ev[0::2], ev[1::2], w, iters)
    mv = np.linalg.norm(co1 - co, axis=1)
    me.vertices.foreach_set('co', co1.ravel()); me.update()
    os.makedirs(os.path.dirname(os.path.abspath(out_blend)), exist_ok=True)
    bpy.ops.wm.save_as_mainfile(filepath=os.path.abspath(out_blend), copy=True)
    R = {'mesh': ob.name, 'out_blend': out_blend, 'centre_m': centre, 'core_m': core, 'edge_m': edge, 'iters': iters, 'pin_top_m': PIN_TOP,
         'tris': len(tris), 'vertices': len(co), 'vertices_weighted': int((w > 0).sum()),
         'move_max_m': round(float(mv.max()), 5), 'move_p95_weighted_m': round(float(np.percentile(mv[w > 0], 95)), 5),
         'folded_faces': ll.flipped_faces(co, co1, tris),
         'min_z_m': round(float(co1[:, 2].min()), 5), 'height_m': round(float(co1[:, 2].max() - co1[:, 2].min()), 5)}
    out = rec
R['secs'] = round(time.time() - t0, 1)
with open(out, 'w') as fh:
    json.dump(R, fh, indent=1)
print('SEAM_LEDGE', json.dumps(R))
