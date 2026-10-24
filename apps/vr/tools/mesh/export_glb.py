# Export the finished mesh of a finish.py .blend as one GLB, so the mesh card grades a file an importer reads, not the
# scene that wrote it. Only the SM_ object is exported with its materials: flat colours (mode=flat), or the baked base
# colour texture embedded as a PNG (mode=bake), so the GLB carries what an importer needs.
#   blender -b <name>_finish.blend -P export_glb.py -- <out.glb>
import bpy, sys

out = sys.argv[sys.argv.index('--') + 1:][0]
sm = [o for o in bpy.context.scene.objects if o.type == 'MESH' and o.name.startswith('SM_')]
if len(sm) != 1:
    raise SystemExit(f'expected one SM_ mesh, found {[o.name for o in sm]}')
bpy.ops.object.select_all(action='DESELECT')
sm[0].select_set(True)
bpy.context.view_layer.objects.active = sm[0]
images = any(n.type == 'TEX_IMAGE' and n.image for m in sm[0].data.materials if m and m.use_nodes for n in m.node_tree.nodes)
bpy.ops.export_scene.gltf(filepath=out, export_format='GLB', use_selection=True, export_apply=True, export_image_format='AUTO' if images else 'NONE')
print('EXPORT_GLB', out, 'images' if images else 'no images')
