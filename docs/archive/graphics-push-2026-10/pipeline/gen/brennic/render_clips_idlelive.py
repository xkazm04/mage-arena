# 2.5D spike S2: render Brennic's game clips (Idle, Cast = CastLunge under chalk, Hit, Death) from the seat camera, locked
# off, as the driving video for the repaint. The motion and timing are the game's own; only the paint is generated.
# blender -b -P render_clips.py -- <SK_Brennic.fbx> <A_Brennic_CastLunge.fbx> <T_BrennicChalk_BaseColor.png> <outdir> [res]
# Writes <outdir>/<Clip>/f_####.png (RGBA, transparent background, 30 fps, the FBX's own frame rate) and clips.json.
import bpy, sys, os, math, json, mathutils

argv = sys.argv[sys.argv.index("--") + 1:]
fbx, lunge, albedo, out = argv[:4]
res = int(argv[4]) if len(argv) > 4 else 768
os.makedirs(out, exist_ok=True)
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=fbx)
arm = [o for o in bpy.data.objects if o.type == 'ARMATURE'][0]
meshes = [o for o in bpy.data.objects if o.type == 'MESH']
base_actions = {a.name.split('|')[-1]: a for a in bpy.data.actions}
print("ACTIONS", list(base_actions))
before = set(bpy.data.actions)
bpy.ops.import_scene.fbx(filepath=lunge)
lunge_actions = [a for a in bpy.data.actions if a not in before]
# The lunge FBX brings its own armature and mesh: keep only its action.
for o in list(bpy.data.objects):
    if o not in meshes and o != arm:
        bpy.data.objects.remove(o, do_unlink=True)
print("LUNGE", [a.name for a in lunge_actions])

img = bpy.data.images.load(albedo)
mat = bpy.data.materials.new("M_Chalk")
mat.use_nodes = True
nt = mat.node_tree
bsdf = nt.nodes["Principled BSDF"]
tex = nt.nodes.new("ShaderNodeTexImage"); tex.image = img
nt.links.new(tex.outputs["Color"], bsdf.inputs["Base Color"])
bsdf.inputs["Roughness"].default_value = 0.85
# The chalk albedo is laid out for the engine's UVs; the repaint wants clean value blocks, so by default the FBX's own
# six flat materials stay (BR_ALBEDO=1 swaps in the albedo).
if os.environ.get("BR_ALBEDO") == "1":
    for o in meshes:
        o.data.materials.clear(); o.data.materials.append(mat)

scn = bpy.context.scene
engines = [e.identifier for e in bpy.types.RenderSettings.bl_rna.properties['engine'].enum_items]
scn.render.engine = 'BLENDER_EEVEE' if 'BLENDER_EEVEE' in engines else 'BLENDER_EEVEE_NEXT'
scn.render.resolution_x = res; scn.render.resolution_y = res
scn.render.film_transparent = True
scn.render.image_settings.file_format = 'PNG'; scn.render.image_settings.color_mode = 'RGBA'
scn.view_settings.view_transform = 'Standard'
scn.world = bpy.data.worlds.new("W"); scn.world.use_nodes = True
scn.world.node_tree.nodes["Background"].inputs[0].default_value = (0.5, 0.48, 0.46, 1)
scn.world.node_tree.nodes["Background"].inputs[1].default_value = 0.7
# Warm key from the front, high left (S30-KEY's firelit front), a cool rim from behind.
key = bpy.data.objects.new("Key", bpy.data.lights.new("Key", 'SUN')); scn.collection.objects.link(key)
key.data.energy = 3.2; key.data.color = (1.0, 0.86, 0.7)
key.rotation_euler = mathutils.Vector((14.0, 6.0, 9.0)).to_track_quat('Z', 'Y').to_euler()
rim = bpy.data.objects.new("Rim", bpy.data.lights.new("Rim", 'SUN')); scn.collection.objects.link(rim)
rim.data.energy = 1.6; rim.data.color = (0.75, 0.82, 1.0)
rim.rotation_euler = mathutils.Vector((-10.0, -4.0, 6.0)).to_track_quat('Z', 'Y').to_euler()

# The seat eye: 16 m out along his facing (+X), 1.9 m up, aimed at his middle. Frame 2.4 m tall (the cell): feet 0.2 m above its bottom.
cam = bpy.data.objects.new("Cam", bpy.data.cameras.new("Cam")); scn.collection.objects.link(cam); scn.camera = cam
dist, eye_h, aim_h, frame_m = 16.0, 1.9, 1.0, 2.4
cam.location = (dist, 0.0, eye_h)
cam.rotation_euler = (mathutils.Vector((0.0, 0.0, aim_h)) - cam.location).to_track_quat('-Z', 'Y').to_euler()
cam.data.sensor_fit = 'VERTICAL'
cam.data.angle = 2.0 * math.atan(frame_m * 0.5 / dist)

# BR_IDLE_LIVE=1: only IdleLive, a 4 s loop built on Idle's first pose: two breaths, one weight shift from foot to foot,
# (a lean over one hip, then the other, feet planted), a slow look round and a little arm and hand sway (the game's Idle barely moves; the sprite's idle needs life).
if os.environ.get("BR_IDLE_LIVE") == "1":
    if not arm.animation_data:
        arm.animation_data_create()
    act = base_actions.get("Idle")
    arm.animation_data.action = act
    if hasattr(arm.animation_data, "action_slot") and act.slots:
        arm.animation_data.action_slot = act.slots[0]
    scn.frame_set(int(act.frame_range[0]))
    pbs = arm.pose.bones
    for pb in pbs:
        pb.rotation_mode = 'QUATERNION'
    base = {pb.name: (pb.rotation_quaternion.copy(), pb.location.copy()) for pb in pbs}
    arm.animation_data.action = None
    n = int(os.environ.get("BR_IDLE_FRAMES", "120"))
    d = os.path.join(out, "IdleLive"); os.makedirs(d, exist_ok=True)

    def rot(name, axis, deg):
        if name in pbs:
            pb = pbs[name]
            pb.rotation_quaternion = pb.rotation_quaternion @ mathutils.Quaternion(axis, math.radians(deg))

    for i in range(n):
        ph = 2.0 * math.pi * i / n
        for pb in pbs:
            pb.rotation_quaternion, pb.location = base[pb.name][0].copy(), base[pb.name][1].copy()
        breath = math.sin(2.0 * ph)
        shift = math.sin(ph)
        # (UE mannequin bones in Blender: local X bends sideways, Z forward and back, Y twists.)
        for b in ("spine_02", "spine_03"):
            rot(b, (0, 0, 1), 1.4 * breath)
        rot("clavicle_l", (1, 0, 0), 1.5 * breath)
        rot("clavicle_r", (1, 0, 0), 1.5 * breath)
        # The weight shift leans the body over one hip then the other; the feet stay planted (no pelvis translation).
        rot("spine_01", (1, 0, 0), 3.0 * shift)
        rot("spine_03", (1, 0, 0), -1.0 * shift)
        rot("head", (0, 1, 0), 7.0 * math.sin(ph + 1.0))
        rot("head", (1, 0, 0), 2.0 * math.sin(2.0 * ph + 0.5))
        for side, sg in (("l", 1.0), ("r", -1.0)):
            rot(f"upperarm_{side}", (1, 0, 0), 3.0 * math.sin(ph + 0.6 * sg))
            rot(f"lowerarm_{side}", (1, 0, 0), 4.0 * math.sin(ph + 1.2 + 0.6 * sg))
            rot(f"hand_{side}", (0, 0, 1), 6.0 * math.sin(2.0 * ph + sg))
        bpy.context.view_layer.update()
        scn.render.filepath = os.path.join(d, "f_%04d.png" % i)
        bpy.ops.render.render(write_still=True)
    print("CLIP IdleLive", n)
    sys.exit(0)

clips = {"Idle": base_actions.get("Idle"), "Cast": lunge_actions[0] if lunge_actions else base_actions.get("Cast"), "Hit": base_actions.get("Hit"), "Death": base_actions.get("Death")}
if not arm.animation_data:
    arm.animation_data_create()
for t in arm.animation_data.nla_tracks:
    t.mute = True
scn.render.fps = 30
meta = {"fps": 30, "res": res, "camera": {"dist_m": dist, "eye_h_m": eye_h, "aim_h_m": aim_h, "frame_m": frame_m}, "clips": {}}
for name, action in clips.items():
    arm.animation_data.action = action
    if hasattr(arm.animation_data, "action_slot") and action.slots:
        arm.animation_data.action_slot = action.slots[0]
    f0, f1 = (int(round(x)) for x in action.frame_range)
    d = os.path.join(out, name); os.makedirs(d, exist_ok=True)
    for i, f in enumerate(range(f0, f1 + 1)):
        scn.frame_set(f)
        scn.render.filepath = os.path.join(d, "f_%04d.png" % i)
        bpy.ops.render.render(write_still=True)
    meta["clips"][name] = {"action": action.name, "frames": f1 - f0 + 1}
    print("CLIP", name, action.name, f1 - f0 + 1)
json.dump(meta, open(os.path.join(out, "clips.json"), "w"), indent=1)
