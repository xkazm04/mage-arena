# T38 evidence, rendered from the exported FBX with EEVEE (adapted from lynx-bench round-2 render.py):
#  <name>_turnaround.png      8 views every 45 degrees, Idle frame 0, orthographic
#  <name>_game_640x480.png    seated player's view: eye 1.2 m, rival 6 m away, 90 degree horizontal FOV, 640x480
#  <name>_clip_<Clip>.png     a still strip per clip: its key frames (at most 6), front three-quarter view
#   blender -b -P render_evidence.py -- <in.fbx> <outdir> <name> [<name>_rig.json from rig_anim.py] [<attack clip name, default Cast>]
#       [<framing: Humanoid (default) | CinderHound | MireMaw>]
# A beast's framing: landscape tiles, the camera aimed at its middle; the maw's strips leave room for the 2.4 m tongue.
import bpy, sys, os, json, math, numpy as np
from mathutils import Vector

a = sys.argv[sys.argv.index('--') + 1:]; fbx, out, name = a[:3]
RIG = json.load(open(a[3])) if len(a) > 3 else None
CLIPS = ['Idle', a[4] if len(a) > 4 else 'Cast', 'Hit', 'Death']
# turnaround (tile w, h, ortho scale, centre z), game-view target z, strips (tile w, h, ortho scale, target x, target z)
FRAMES = {'Humanoid': ((360, 640, 2.1, 0.95), 0.9, (400, 600, 2.3, 0.0, 0.9)),
          'CinderHound': ((480, 400, 2.3, 0.75), 0.6, (480, 400, 2.6, 0.0, 0.6)),
          'MireMaw': ((480, 400, 1.9, 0.65), 0.6, (560, 400, 4.0, 1.0, 0.65))}
TURN, GAME_Z, STRIP = FRAMES[a[5] if len(a) > 5 else 'Humanoid']
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=fbx)
s = bpy.context.scene; s.render.engine = 'BLENDER_EEVEE'; s.view_settings.view_transform = 'Standard'
try: s.eevee.taa_render_samples = 24
except Exception: pass
arm = next(o for o in s.objects if o.type == 'ARMATURE')
acts = {act.name.split('|')[-1]: act for act in bpy.data.actions}
w_ = bpy.data.worlds.new('w'); s.world = w_; w_.use_nodes = True; w_.node_tree.nodes['Background'].inputs[0].default_value = (0.30, 0.30, 0.32, 1)
for loc, e in [((3, -2, 3), 3.0), ((-1, 3, 1.5), 1.5), ((-3, 0, 2), 1.0)]:
    L = bpy.data.objects.new('l', bpy.data.lights.new('l', 'SUN')); L.data.energy = e; s.collection.objects.link(L)
    L.rotation_euler = (-Vector(loc)).to_track_quat('-Z', 'Y').to_euler()
floor = bpy.data.objects.new('floor', bpy.data.meshes.new('floor')); s.collection.objects.link(floor)
floor.data.from_pydata([(-20, -20, 0), (20, -20, 0), (20, 20, 0), (-20, 20, 0)], [], [(0, 1, 2, 3)])
fm = bpy.data.materials.new('floor'); fm.use_nodes = True; fm.node_tree.nodes['Principled BSDF'].inputs['Base Color'].default_value = (0.18, 0.18, 0.19, 1); floor.data.materials.append(fm)
cam = bpy.data.objects.new('cam', bpy.data.cameras.new('cam')); s.collection.objects.link(cam); s.camera = cam
s.render.image_settings.file_format = 'PNG'; s.render.resolution_percentage = 100


def use(clip, frame):
    ad = arm.animation_data or arm.animation_data_create(); act = acts[clip]; ad.action = act
    if hasattr(ad, 'action_slot') and ad.action_slot is None and len(act.slots): ad.action_slot = act.slots[0]
    s.frame_set(int(frame))


def aim(loc, target):
    cam.location = loc; cam.rotation_euler = (Vector(target) - Vector(loc)).to_track_quat('-Z', 'Y').to_euler()


def shot(path, rx, ry):
    s.render.resolution_x = rx; s.render.resolution_y = ry; s.render.filepath = path; bpy.ops.render.render(write_still=True)
    im = bpy.data.images.load(path); px = np.empty(rx * ry * 4, dtype=np.float32); im.pixels.foreach_get(px); bpy.data.images.remove(im)
    return px.reshape(ry, rx, 4)


def stitch(tiles, path):
    img = np.concatenate(tiles, axis=1); im = bpy.data.images.new('st', img.shape[1], img.shape[0], alpha=True)
    im.pixels.foreach_set(img.ravel()); im.filepath_raw = path; im.file_format = 'PNG'; im.save(); bpy.data.images.remove(im)


tmp = os.path.join(out, '_tile.png')
# turnaround: front (+X) first, then every 45 degrees counter-clockwise
cam.data.type = 'ORTHO'; cam.data.ortho_scale = TURN[2]; use('Idle', 0); tiles = []
for i in range(8):
    r = math.radians(45 * i); d = Vector((math.cos(r), math.sin(r), 0)) * 6
    aim(Vector((0, 0, TURN[3])) + d, (0, 0, TURN[3])); tiles.append(shot(tmp, TURN[0], TURN[1]))
stitch(tiles, os.path.join(out, f'{name}_turnaround.png'))
# game distance: seated eye height, rival 6 m in front, facing the player
cam.data.type = 'PERSP'; cam.data.sensor_fit = 'HORIZONTAL'; cam.data.angle = math.radians(90); use('Idle', 0)
aim(Vector((6, 0, 1.2)), (0, 0, GAME_Z)); shot(os.path.join(out, f'{name}_game_640x480.png'), 640, 480)
# clip strips
cam.data.type = 'ORTHO'; cam.data.ortho_scale = STRIP[2]
for clip in CLIPS:
    use(clip, 0); tiles = []
    keys = RIG['clips'][clip]['keys'] if RIG else []  # the FBX is baked every frame, so the authored keys come from rig.json
    if not keys:
        f0, f1 = acts[clip].frame_range; keys = [round(f0 + (f1 - f0) * k / 5) for k in range(6)]
    for fr in keys:
        use(clip, fr)
        r = math.radians(-35); aim(Vector((math.cos(r) * 6 + STRIP[3], math.sin(r) * 6, 1.6 + (STRIP[4] - 0.9))), (STRIP[3], 0, STRIP[4]))
        tiles.append(shot(tmp, STRIP[0], STRIP[1]))
    stitch(tiles, os.path.join(out, f'{name}_clip_{clip}.png'))
os.remove(tmp)
print('EVIDENCE', sorted(f for f in os.listdir(out) if f.startswith(name) and f.endswith('.png')))
