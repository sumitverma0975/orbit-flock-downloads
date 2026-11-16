"""Run inside Unreal Editor Python; create missing city worlds independently."""
import unreal


CITY_MAPS = (
    ("/Game/Maps/Lvl_Delhi", "delhi.json", "Delhi"),
    ("/Game/Maps/Lvl_Chandigarh", "chandigarh.json", "Chandigarh"),
)


def create_city_map(map_path, data_file, city_name):
    assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
    if assets.does_asset_exist(map_path):
        unreal.log("DelhiDrive: existing map preserved: " + map_path)
        return

    road_class = unreal.load_class(None, "/Script/DelhiDrive.RoadNetworkActor")
    game_mode = unreal.load_class(None, "/Script/DelhiDrive.DriveGameMode")
    if not road_class or not game_mode:
        raise RuntimeError("DelhiDrive native classes are unavailable. Build DelhiDriveEditor first.")
    if not unreal.EditorLevelLibrary.new_level(map_path):
        raise RuntimeError("Could not create level " + map_path)

    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    world = unreal.EditorLevelLibrary.get_editor_world()
    world.get_world_settings().set_editor_property("default_game_mode", game_mode)

    roads = actors.spawn_actor_from_class(road_class, unreal.Vector(0, 0, 0))
    if not roads:
        raise RuntimeError("Could not spawn RoadNetworkActor")
    roads.set_actor_label(city_name + " Road Network")
    roads.set_editor_property("data_file", data_file)

    start = actors.spawn_actor_from_class(unreal.PlayerStart, unreal.Vector(0, -200, 150))
    start.set_actor_label("DelhiDrive Player Start")
    sun = actors.spawn_actor_from_class(
        unreal.DirectionalLight, unreal.Vector(0, 0, 1200), unreal.Rotator(-35, -40, 0))
    sun.set_actor_label("DelhiDrive Sun")
    sun_component = sun.get_component_by_class(unreal.DirectionalLightComponent)
    sun_component.set_editor_property("mobility", unreal.ComponentMobility.MOVABLE)
    sun_component.set_editor_property("intensity", 5.0)
    sun_component.set_editor_property("cast_shadows", False)

    sky = actors.spawn_actor_from_class(unreal.SkyLight, unreal.Vector(0, 0, 1200))
    sky.set_actor_label("DelhiDrive Ambient Light")
    sky_component = sky.get_component_by_class(unreal.SkyLightComponent)
    sky_component.set_editor_property("mobility", unreal.ComponentMobility.MOVABLE)
    sky_component.set_editor_property("intensity", 1.0)
    sky_component.set_editor_property("cast_shadows", False)

    # This engine Blueprint contains a sky mesh suitable for the mobile renderer.
    sky_class = unreal.load_class(None, "/Engine/EngineSky/BP_Sky_Sphere.BP_Sky_Sphere_C")
    if sky_class:
        sphere = actors.spawn_actor_from_class(sky_class, unreal.Vector(0, 0, 0))
        sphere.set_actor_label("DelhiDrive Sky")
    else:
        unreal.log_warning("Engine sky sphere unavailable; add a mobile sky mesh manually.")

    if not unreal.EditorLevelLibrary.save_current_level():
        raise RuntimeError("Could not save " + map_path)
    unreal.log("DelhiDrive: created and saved " + map_path + " using " + data_file)


def main():
    for map_path, data_file, city_name in CITY_MAPS:
        create_city_map(map_path, data_file, city_name)


main()
