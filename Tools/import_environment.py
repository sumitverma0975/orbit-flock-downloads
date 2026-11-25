"""Build the four mobile PBR materials in the UE 5.5 editor from the shipped CC0 maps."""
import hashlib
import json
from pathlib import Path
import unreal

SOURCE = Path(__file__).resolve().parents[1] / "SourceAssets/Environment"
DEST = "/Game/Environment"
assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
tools = unreal.AssetToolsHelpers.get_asset_tools()
library = unreal.MaterialEditingLibrary
assets.make_directory(DEST)

def load_texture(role, entry):
    path = SOURCE / entry["file"]
    if hashlib.sha256(path.read_bytes()).hexdigest() != entry["sha256"]:
        raise RuntimeError(f"Invalid source texture: {path}")
    asset_path = DEST + "/" + path.stem
    texture = assets.load_asset(asset_path)
    if not texture:
        task = unreal.AssetImportTask()
        task.filename = str(path)
        task.destination_path = DEST
        task.destination_name = path.stem
        task.automated = True
        task.save = True
        tools.import_asset_tasks([task])
        texture = assets.load_asset(asset_path)
    if not isinstance(texture, unreal.Texture2D):
        raise RuntimeError("Texture import failed: " + str(path))
    compression = {"Normal":unreal.TextureCompressionSettings.TC_NORMALMAP,
                   "Rough":unreal.TextureCompressionSettings.TC_MASKS,
                   "Color":unreal.TextureCompressionSettings.TC_DEFAULT}
    texture.set_editor_property("compression_settings", compression[role])
    texture.set_editor_property("srgb", role == "Color")
    if not assets.save_loaded_asset(texture, only_if_is_dirty=False):
        raise RuntimeError("Could not save " + asset_path)
    return texture

properties = {"Color": unreal.MaterialProperty.MP_BASE_COLOR,
              "Normal": unreal.MaterialProperty.MP_NORMAL,
              "Rough": unreal.MaterialProperty.MP_ROUGHNESS}
samplers = {"Color": unreal.MaterialSamplerType.SAMPLERTYPE_COLOR,
            "Normal": unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL,
            "Rough": unreal.MaterialSamplerType.SAMPLERTYPE_MASKS}
for surface in json.loads((SOURCE / "surfaces.json").read_text()):
    name = "M_" + surface["material"]
    mat = assets.load_asset(DEST + "/" + name) or tools.create_asset(name, DEST, unreal.Material, unreal.MaterialFactoryNew())
    if not mat:
        raise RuntimeError("Could not create " + name)
    library.delete_all_material_expressions(mat)
    textures = json.loads((SOURCE / "facade_bays.json").read_text()) if name == "M_Facade" else surface["textures"]
    for index,(role,entry) in enumerate(textures.items()):
        tex = load_texture(role, entry)
        node = library.create_material_expression(mat, unreal.MaterialExpressionTextureSample, -500, index*230)
        node.texture = tex
        node.set_editor_property("sampler_type", samplers[role])
        if not library.connect_material_property(node, "R" if role=="Rough" else "RGB", properties[role]):
            raise RuntimeError("Material connection failed: " + name + "/" + role)
    library.recompile_material(mat)
    if not assets.save_loaded_asset(mat, only_if_is_dirty=False):
        raise RuntimeError("Could not save " + name)

for city in ("delhi", "chandigarh"):
    path = SOURCE.parent / "Cartography" / ("T_Map_" + city + ".png")
    task = unreal.AssetImportTask()
    task.filename = str(path)
    task.destination_path = DEST
    task.destination_name = path.stem
    task.automated = True
    task.replace_existing = True
    task.save = True
    tools.import_asset_tasks([task])
    texture = assets.load_asset(DEST + "/" + path.stem)
    if not isinstance(texture, unreal.Texture2D):
        raise RuntimeError("Could not import district map " + str(path))
    texture.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
    if not assets.save_loaded_asset(texture, only_if_is_dirty=False):
        raise RuntimeError("Could not save district map " + path.stem)

daylight = json.loads((SOURCE / "daylight.json").read_text())
sky_file = SOURCE / daylight["file"]
if hashlib.sha256(sky_file.read_bytes()).hexdigest() != daylight["sha256"]:
    raise RuntimeError("Invalid daylight cubemap input")
task = unreal.AssetImportTask()
task.filename = str(sky_file)
task.destination_path = DEST
task.destination_name = "T_Daylight"
task.automated = True
task.replace_existing = True
task.save = True
tools.import_asset_tasks([task])
if not isinstance(assets.load_asset(DEST + "/T_Daylight"), unreal.TextureCube):
    raise RuntimeError("Expected a Radiance HDR texture cube")
