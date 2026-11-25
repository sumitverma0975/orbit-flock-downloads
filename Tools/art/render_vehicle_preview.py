"""Blender-only offline model renders; explicitly not an Unreal gameplay capture."""
import bpy
from mathutils import Vector
from pathlib import Path
import json
import math
ROOT=Path(__file__).resolve().parents[2]
BASE=ROOT/'SourceAssets/Vehicles'
OUT=ROOT/'Docs/VehiclePreviews'
OUT.mkdir(parents=True,exist_ok=True)
entries=json.loads((BASE/'prepared_manifest.json').read_text())['assets']
for entry in entries:
    bpy.ops.object.select_all(action='SELECT');bpy.ops.object.delete(use_global=False)
    bpy.ops.import_scene.gltf(filepath=str(BASE/entry['file']))
    scene=bpy.context.scene
    scene.render.engine='CYCLES';scene.cycles.device='CPU';scene.cycles.samples=24
    scene.cycles.use_denoising=False
    scene.render.resolution_x=480;scene.render.resolution_y=310;scene.render.resolution_percentage=100
    scene.render.image_settings.file_format='PNG';scene.render.film_transparent=False
    scene.world.use_nodes=True
    scene.world.node_tree.nodes['Background'].inputs['Color'].default_value=(.16,.21,.28,1)
    scene.world.node_tree.nodes['Background'].inputs['Strength'].default_value=.65
    bpy.ops.mesh.primitive_plane_add(size=200,location=(0,0,-.015))
    ground=bpy.context.object;ground.name='Preview ground (not vehicle asset)'
    material=bpy.data.materials.new('Preview background');material.diffuse_color=(.075,.105,.145,1)
    ground.data.materials.append(material)
    for location,energy,size in [((4,-4,6),900,5),((-3,2,5),1100,4),((0,5,2),650,4)]:
        light=bpy.data.lights.new('Studio softbox','AREA');light.energy=energy;light.shape='DISK';light.size=size
        node=bpy.data.objects.new('Studio softbox',light);scene.collection.objects.link(node);node.location=location
        node.rotation_euler=(Vector((0,0,.6))-node.location).to_track_quat('-Z','Y').to_euler()
    camera=bpy.data.objects.new('Preview camera',bpy.data.cameras.new('Preview camera'));scene.collection.objects.link(camera)
    camera.location=(7.0,-8.8,5.1);camera.rotation_euler=(Vector((0,0,.6))-camera.location).to_track_quat('-Z','Y').to_euler()
    camera.data.type='ORTHO';camera.data.ortho_scale=7.0
    scene.camera=camera
    scene.render.filepath=str(OUT/(entry['id']+'.png'))
    bpy.ops.render.render(write_still=True)
    print('PREVIEW',entry['id'],flush=True)
