# T38: quick orthographic look at a GLB from four sides (-Y, +X, +Y, -X), stitched into one PNG, plus its bbox.
#   blender -b -P preview.py -- <in.glb> <out.png>
import bpy, sys, math, numpy as np, mathutils

a = sys.argv[sys.argv.index('--') + 1:]; src, dst = a[:2]
bpy.ops.wm.read_factory_settings(use_empty=True)
if src.endswith(".blend"):
    bpy.ops.wm.open_mainfile(filepath=src)
else:
    bpy.ops.import_scene.gltf(filepath=src)
objs = [o for o in bpy.context.scene.objects if o.type == 'MESH']
mn = mathutils.Vector((1e9,) * 3); mx = -mn
for o in objs:
    for c in o.bound_box:
        w = o.matrix_world @ mathutils.Vector(c); mn = mathutils.Vector(map(min, mn, w)); mx = mathutils.Vector(map(max, mx, w))
ctr = (mn + mx) / 2; h = max(mx - mn)
print('BBOX', [round(x, 3) for x in mn], [round(x, 3) for x in mx])
s = bpy.context.scene; s.render.engine = 'BLENDER_EEVEE'; s.view_settings.view_transform = 'Standard'
w_ = bpy.data.worlds.new('w'); s.world = w_; w_.use_nodes = True; w_.node_tree.nodes['Background'].inputs[0].default_value = (0.8, 0.8, 0.8, 1)
cam = bpy.data.objects.new('cam', bpy.data.cameras.new('cam')); s.collection.objects.link(cam); s.camera = cam
cam.data.type = 'ORTHO'; cam.data.ortho_scale = h * 1.1
L = bpy.data.objects.new('l', bpy.data.lights.new('l', 'SUN')); L.data.energy = 3; s.collection.objects.link(L)
s.render.resolution_x = 400; s.render.resolution_y = 600; s.render.image_settings.file_format = 'PNG'
tiles = []
for i, az in enumerate((0, 90, 180, 270)):
    r = math.radians(az); d = mathutils.Vector((math.sin(r), -math.cos(r), 0)) * h * 3
    cam.location = ctr + d; cam.rotation_euler = (-d).to_track_quat('-Z', 'Y').to_euler()
    L.rotation_euler = (-(d + mathutils.Vector((0, 0, h * 2)))).to_track_quat('-Z', 'Y').to_euler()
    p = f'{dst}.{i}.png'; s.render.filepath = p; bpy.ops.render.render(write_still=True)
    im = bpy.data.images.load(p); px = np.empty(400 * 600 * 4, dtype=np.float32); im.pixels.foreach_get(px); tiles.append(px.reshape(600, 400, 4))
out = np.concatenate(tiles, axis=1)
im = bpy.data.images.new('out', out.shape[1], out.shape[0], alpha=True); im.pixels.foreach_set(out.ravel())
im.filepath_raw = dst; im.file_format = 'PNG'; im.save()
import os
for i in range(4): os.remove(f'{dst}.{i}.png')
