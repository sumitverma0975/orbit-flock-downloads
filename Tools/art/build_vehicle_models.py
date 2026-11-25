"""Blender 4.3+ source-art build. No game engine is used by this script.

blender -b --python Tools/art/build_vehicle_models.py
Writes single-mesh GLBs in metres; +X is car forward and glTF +Y is up.
Unreal Interchange converts this standard coordinate system to +X/+Z and cm.
"""
import bpy
from mathutils import Matrix, Vector
from pathlib import Path
import math
import json
import hashlib
import sys

ROOT = Path(__file__).resolve().parents[2]
BASE = ROOT / 'SourceAssets/Vehicles'
OUTPUT = BASE / 'Prepared'
SPECS = [
    dict(id='lux_sedan', name='BMW 320i E90', source='bmw-320i.glb', yaw=90, length=4.53, original='bmw_320i'),
    dict(id='lux_suv', name='Mercedes-Benz G-Class', source='mercedes-gclass.glb', yaw=90, length=4.82, original='mercedes_gclass'),
    dict(id='lux_coupe', name='Rhein Sport Coupe', style='coupe', length=4.48, width=1.88, height=.65, roof=.54, cabin=2.05, color='7d8994'),
    dict(id='lux_limo', name='Berlin Executive L', style='limo', length=5.85, width=1.94, height=.72, roof=.62, cabin=3.45, color='20242a'),
    dict(id='sup_gt', name='Mercedes-AMG GT', source='mercedes-benz-amg-gt-import.glb', yaw=167, length=4.55, original='mercedes_amg_gt', remove='Object_4', triangle_budget=55000),
    dict(id='sup_mid', name='Stuttgart Mid-Engine', style='mid', length=4.56, width=2.02, height=.50, roof=.47, cabin=1.8, color='9bb831'),
    dict(id='sup_hyper', name='Autobahn Hyper GT', style='hyper', length=4.84, width=2.08, height=.45, roof=.45, cabin=1.65, color='2879bc'),
    dict(id='sup_track', name='Nuerburg Track RS', style='track', length=4.50, width=2.02, height=.45, roof=.44, cabin=1.7, color='c0472b'),
    dict(id='lux_estate', name='Rhein Touring RS', style='estate', length=4.92, width=1.94, height=.73, roof=.66, cabin=3.0, color='2d6358'),
    dict(id='lux_grand', name='Black Forest GT', style='grand', length=4.90, width=1.98, height=.60, roof=.50, cabin=2.05, color='73303d'),
    dict(id='sup_spider', name='Tejas Spider', style='spider', length=4.30, width=1.96, height=.49, roof=.30, cabin=1.55, color='a74e7f'),
    dict(id='sup_apex', name='Vajra Apex', style='apex', length=4.96, width=2.12, height=.40, roof=.46, cabin=1.5, color='d1d7d6'),
]


def rgba(value):
    return tuple(int(value[i:i+2], 16) / 255 for i in (0, 2, 4)) + (1,)


def mat(name, color, metallic=0, roughness=.45, emission=0):
    result = bpy.data.materials.new(name)
    result.diffuse_color = rgba(color)
    result.use_nodes = True
    p = result.node_tree.nodes.get('Principled BSDF')
    p.inputs['Base Color'].default_value = rgba(color)
    p.inputs['Metallic'].default_value = metallic
    p.inputs['Roughness'].default_value = roughness
    p.inputs['Coat Weight'].default_value = .35 if metallic else .05
    if emission:
        p.inputs['Emission Color'].default_value = rgba(color)
        p.inputs['Emission Strength'].default_value = emission
    return result


def attach(obj, material):
    obj.data.materials.append(material)
    return obj


def box(name, location, dimensions, material, bevel=0):
    bpy.ops.mesh.primitive_cube_add(size=1, location=location)
    obj = bpy.context.object
    obj.name = name
    obj.dimensions = dimensions
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    attach(obj, material)
    if bevel:
        mod = obj.modifiers.new('Crafted edges', 'BEVEL')
        mod.width = bevel
        mod.segments = 2
        bpy.ops.object.modifier_apply(modifier=mod.name)
        obj.modifiers.new('Weighted normals', 'WEIGHTED_NORMAL')
    return obj


def loft(name, sections, material):
    # X / half-width / lower Z / upper Z. Eight-point rounded cross-section.
    vertices, faces = [], []
    for x, w, lo, hi in sections:
        h = hi - lo
        vertices += [(x, -.82*w, lo), (x, .82*w, lo), (x, w, lo+h*.22), (x, w*.98, hi-h*.16),
                     (x, w*.77, hi), (x, -w*.77, hi), (x, -w*.98, hi-h*.16), (x, -w, lo+h*.22)]
    faces.append(tuple(reversed(range(8))))
    for i in range(len(sections)-1):
        for j in range(8):
            faces.append((i*8+j, i*8+(j+1)%8, (i+1)*8+(j+1)%8, (i+1)*8+j))
    faces.append(tuple((len(sections)-1)*8+j for j in range(8)))
    mesh = bpy.data.meshes.new(name)
    mesh.from_pydata(vertices, [], faces)
    mesh.update()
    obj = bpy.data.objects.new(name, mesh)
    bpy.context.collection.objects.link(obj)
    attach(obj, material)
    bpy.context.view_layer.objects.active = obj
    obj.select_set(True)
    bpy.ops.object.mode_set(mode='EDIT')
    bpy.ops.mesh.select_all(action='SELECT')
    bpy.ops.mesh.normals_make_consistent(inside=False)
    bpy.ops.object.mode_set(mode='OBJECT')
    obj.select_set(False)
    bevel = obj.modifiers.new('Soft panel edges', 'BEVEL')
    bevel.width = .025
    bevel.segments = 2
    obj.modifiers.new('Panel normals', 'WEIGHTED_NORMAL')
    return obj


def cylinder(name, location, radius, depth, material, vertices=24):
    bpy.ops.mesh.primitive_cylinder_add(vertices=vertices, radius=radius, depth=depth, location=location, rotation=(math.pi/2,0,0))
    obj = bpy.context.object
    obj.name = name
    attach(obj, material)
    for face in obj.data.polygons:
        face.use_smooth = len(face.vertices) == 4
    return obj


def make_original(spec):
    l,w,h,r,c = (spec[k] for k in ['length','width','height','roof','cabin'])
    sport = spec['id'].startswith('sup_')
    style = spec['style']
    paint = mat('Paint_'+spec['id'], spec['color'], .72, .28)
    trim = mat('Satin carbon', '11171d', .28, .4)
    glass = mat('Dark solar glass', '172b35', .38, .18)
    rubber = mat('Performance rubber', '111214', 0, .85)
    alloy = mat('Forged alloy', 'a8b0b8', .9, .22)
    red = mat('Brake caliper', 'b22b24', .4, .35)
    head = mat('Daytime running lights', 'e1f3ff', .1, .2, 1.2)
    tail = mat('Rear light signature', 'dd293b', .1, .2, .8)
    base = .21
    belt = base+h
    cabin_x = -.12*l if style not in ('mid','hyper','track','apex','spider') else .04*l
    if style=='estate': cabin_x = -.05*l
    # Distinct long-hood GT, cabin-forward mid-engine, limousine and shooting brake silhouettes.
    loft('Coachwork', [(-.5*l,.78*w/2,base+.08,belt-.12),(-.43*l,w/2,base,belt),(-.28*l,w/2,base,belt+.04),
        (.12*l,.96*w/2,base,belt),(.36*l,.98*w/2,base+.02,belt-.10),(.48*l,.85*w/2,base+.10,belt-.19),(.5*l,.72*w/2,base+.12,belt-.23)],paint)
    cabin_w = w*(.72 if sport else .80)/2
    roof_hi = belt+r
    loft('Glazing', [(cabin_x-c*.5,cabin_w*.9,belt-.02,belt+.04), (cabin_x-c*.24,cabin_w*.88,belt-.02,roof_hi),
        (cabin_x+c*.14,cabin_w*.84,belt-.02,roof_hi-.015), (cabin_x+c*.50,cabin_w*.95,belt-.02,belt+.035)],glass)
    if style!='spider':
        box('Roof panel',(cabin_x-c*.045,0,roof_hi+.014),(c*.40,w*.57,.045),paint,.025)
    else:
        box('Open cockpit',(cabin_x-.18,0,belt+.08),(c*.51,w*.61,.20),trim,.04)
        for side in (-1,1):
            box('Seat roll hoop',(cabin_x-.42,side*w*.21,belt+.27),(.08,.20,.23),alloy,.035)
    for side in (-1,1):
        if not sport:
            pillar = box('Pillar',(cabin_x-c*.08,side*cabin_w*.93,belt+r*.44),(.075,.045,r*.83),trim,.009)
            box('Door brightwork',(cabin_x+.05,side*w*.495,belt-.15),(.21,.025,.032),alloy,.009)
        box('Mirror',(cabin_x+c*.36,side*(w/2+.055),belt+.11),(.24,.17,.105),paint,.045)
        box('Sill',(0,side*w*.488,base+.025),(l*.62,.08,.10),trim,.02)
        # Wheel geometry with physically visible tires, rim dishes, six spokes and brake discs.
        radius = .355 if not sport else .34
        for x in (-.315*l,.315*l):
            y = side*w*.46
            bpy.ops.mesh.primitive_torus_add(major_radius=radius-.075,minor_radius=.075,major_segments=24,minor_segments=10,
                location=(x,y,radius),rotation=(math.pi/2,0,0))
            tire=bpy.context.object; tire.name='Tire'; attach(tire,rubber)
            for face in tire.data.polygons: face.use_smooth=True
            cylinder('Rim barrel',(x,y,radius),radius*.76,.22,trim)
            cylinder('Brake rotor',(x,y+side*.08,radius),radius*.59,.025,alloy)
            box('Caliper',(x+.13,y+side*.102,radius),(.09,.035,.20),red,.018)
            for spoke in range(6):
                a=spoke*math.tau/6
                arm=box('Alloy spoke',(x+math.cos(a)*radius*.30,y+side*.125,radius+math.sin(a)*radius*.30),(.22,.03,.045),alloy,.012)
                arm.rotation_euler.y=-a
            cylinder('Hub',(x,y+side*.137,radius),radius*.16,.035,alloy,16)
        # Grille, segmented lamps, diffuser and rear marker assemblies.
        box('Front lamp',(.485*l,side*w*.31,belt-.16),(.035,w*.25,.07 if sport else .105),head,.018)
        box('Rear lamp',(-.487*l,side*w*.32,belt-.13),(.035,w*.26,.067),tail,.018)
        cylinder('Exhaust',(-.497*l,side*w*.32,base+.12),.055,.12,alloy,16).rotation_euler=(0,math.pi/2,0)
        if sport:
            vent=box('Side cooling intake',(cabin_x-c*.40,side*w*.495,belt-.18),(.40,.025,.18),trim,.045)
    box('Front grille',(.494*l,0,base+.23),(.04,w*.50,h*.35),trim,.035)
    for j in range(-5,6):
        box('Grille vane',(.518*l,j*w*.038,base+.23),(.02,.013,h*.26),alloy,.003)
    box('Front splitter',(.465*l,0,base-.01),(.37,w*.96,.07),trim,.025)
    box('Rear diffuser',(-.475*l,0,base+.015),(.22,w*.80,.12),trim,.018)
    box('Plate',(-.501*l,0,base+.28),(.015,.40,.11),alloy,.009)
    if style in ('hyper','track','apex'):
        for side in (-1,1):
            box('Wing mount',(-.37*l,side*w*.29,belt+.17),(.12,.06,.36),trim,.015)
        wing=box('Rear aerofoil',(-.37*l,0,belt+.37),(.34,w*1.04,.075),paint,.025)
        wing.rotation_euler.y=-.12
        for side in (-1,1):
            box('Aero endplate',(-.37*l,side*w*.52,belt+.37),(.38,.045,.19),trim,.015)
    if style in ('track','apex'):
        for side in (-1,1):
            stripe=box('Hood racing stripe',(.32*l,side*.10,belt-.075),(.65,.085,.018),trim,.006)
            stripe.rotation_euler.y=.10
    if style=='estate':
        for side in (-1,1):
            box('Roof rail',(cabin_x,side*w*.29,roof_hi+.07),(c*.65,.055,.055),alloy,.02)
    return None


def bounds(objects):
    points=[obj.matrix_world @ Vector(vertex) for obj in objects for vertex in obj.bound_box]
    return Vector(tuple(min(p[i] for p in points) for i in range(3))),Vector(tuple(max(p[i] for p in points) for i in range(3)))


def build(spec):
    bpy.ops.object.select_all(action='SELECT'); bpy.ops.object.delete(use_global=False)
    for datablock in list(bpy.data.materials): bpy.data.materials.remove(datablock)
    scene=bpy.context.scene
    scene.unit_settings.system='METRIC'; scene.unit_settings.scale_length=1.0
    if 'source' in spec:
        bpy.ops.import_scene.gltf(filepath=str(BASE/spec['source']))
        for obj in list(bpy.data.objects):
            if obj.type!='MESH' or obj.name==spec.get('remove'):
                if obj.type=='MESH' or not obj.children:
                    bpy.data.objects.remove(obj,do_unlink=True)
        objects=[o for o in scene.objects if o.type=='MESH']
        rotate=Matrix.Rotation(math.radians(spec['yaw']),4,'Z')
        # Freeze complete source scene hierarchy into world-space positions.
        matrices={o:rotate@o.matrix_world for o in objects}
        for obj in objects:
            obj.parent=None;obj.matrix_world=matrices[obj]
        lo,hi=bounds(objects)
        factor=spec['length']/(hi.x-lo.x)
        offset=Vector(((lo.x+hi.x)/2,(lo.y+hi.y)/2,lo.z))
        for obj in objects:
            obj.matrix_world=Matrix.Scale(factor,4)@Matrix.Translation(-offset)@obj.matrix_world
    else:
        make_original(spec)
    bpy.ops.object.select_all(action='DESELECT')
    objects=[o for o in scene.objects if o.type=='MESH']
    for obj in objects:obj.select_set(True)
    bpy.context.view_layer.objects.active=objects[0]
    bpy.ops.object.convert(target='MESH')
    bpy.ops.object.join()
    merged=bpy.context.object
    merged.name='SM_'+spec['id'];merged.data.name=merged.name
    bpy.ops.object.transform_apply(location=True,rotation=True,scale=True)
    # Collapse duplicate slots after joining, while preserving authored material appearance.
    materials=[];remap={}
    for slot_index,material in enumerate(merged.data.materials):
        if material not in materials:materials.append(material)
        remap[slot_index]=materials.index(material)
    indices=[remap[p.material_index] for p in merged.data.polygons]
    merged.data.materials.clear()
    for material in materials:merged.data.materials.append(material)
    for polygon,index in zip(merged.data.polygons,indices):polygon.material_index=index
    initial_triangles=sum(len(p.vertices)-2 for p in merged.data.polygons)
    budget=spec.get('triangle_budget',26000)
    if initial_triangles>budget:
        modifier=merged.modifiers.new('Mobile geometry budget','DECIMATE')
        modifier.ratio=budget/initial_triangles
        modifier.use_collapse_triangulate=True
        bpy.ops.object.modifier_apply(modifier=modifier.name)
    bpy.ops.object.transform_apply(location=True,rotation=True,scale=True)
    # Refit final visible geometry after decimation, bumper additions and joining.
    verts = [v.co.copy() for v in merged.data.vertices]
    lo = Vector(tuple(min(v[i] for v in verts) for i in range(3)))
    hi = Vector(tuple(max(v[i] for v in verts) for i in range(3)))
    center = Vector(((lo.x+hi.x)/2, (lo.y+hi.y)/2, lo.z))
    fit = spec['length'] / (hi.x-lo.x)
    for vertex in merged.data.vertices:
        vertex.co = (vertex.co-center)*fit
    # Keep vehicle +X forward through the standard Blender Z-up -> glTF Y-up export.
    # Static glTF geometry may use any forward direction; preserving X removes ambiguity
    # when Unreal performs its up-axis and handedness conversion.
    merged.rotation_mode='XYZ'
    merged.rotation_euler=(0,0,0)
    bpy.ops.object.transform_apply(location=True,rotation=True,scale=True)
    path=OUTPUT/(merged.name+'.glb')
    bpy.ops.export_scene.gltf(filepath=str(path),export_format='GLB',use_selection=True,export_yup=True,
        export_apply=True,export_texcoords=True,export_normals=True,export_materials='EXPORT',export_cameras=False,export_lights=False)
    data=path.read_bytes()
    # Export payload is intentionally self-contained; no source path dependency.
    return dict(id=spec['id'],display_name=spec['name'],file='Prepared/'+path.name,
        asset_path='/Game/Cars/Models/'+merged.name+'.'+merged.name,mesh_name=merged.name,
        sha256=hashlib.sha256(data).hexdigest(),bytes=len(data),length_m=spec['length'],
        triangles=sum(len(p.vertices)-2 for p in merged.data.polygons),materials=len(merged.data.materials),
        source_asset_id=spec.get('original'),original_geometry='source' not in spec,
        processing='Blender: hierarchy baked, centered and grounded, normalized in metres and facing +X, joined by materials; mobile decimation when over budget. AMG presentation floor removed.')


def main():
    OUTPUT.mkdir(parents=True,exist_ok=True)
    manifest=[]
    requested=sys.argv[sys.argv.index('--')+1:] if '--' in sys.argv else []
    for spec in SPECS:
        if requested and spec['id'] not in requested:continue
        result=build(spec);manifest.append(result);print('PREPARED',result,flush=True)
    target=BASE/'prepared_manifest.json'
    if requested and target.exists():
        previous=json.loads(target.read_text())['assets']
        changed={a['id'] for a in manifest};manifest=[a for a in previous if a['id'] not in changed]+manifest
    target.write_text(json.dumps(dict(schema_version=1,coordinates='glTF metres: +X vehicle forward, +Y up; origin centered horizontally with tyre bottoms at Y=0',ue_expected='Unreal Interchange converts metres to centimetres and +Y up to +Z. Vehicle +X forward is preserved. Importer asserts real imported bounds.',engine_import_tested=False,assets=manifest),indent=2)+'\n')

if __name__=='__main__':main()
