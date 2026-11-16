# Delhi Drive — UE5 project source and assets

**Source preview, not a finished game or APK.** This project targets Unreal Engine 5.5. C++ compilation, Unreal asset import, cooking and Android gameplay have not been run. No APK is included or advertised in this release.

The original upload contained C++ and a sample grid, with no Unreal map or vehicle assets. This revision adds career progression, mobile-oriented changes, real road extracts and three attributed vehicle source models. An Unreal build machine and completed vehicle rigging are still needed for a playable build.

## Included

- **12 career stages** in C++ with sequential unlocks, saved 1–3-star results, crash limits, escalating time/traffic/pothole pressure and a Next Level action.
- **12 handling profiles:** six luxury and six supercar profiles with distinct mass, power, speed limit, braking, steering and score unlocks. These are not twelve separate 3D models.
- **Four difficulty modes:** Easy, Medium, Hard and Extreme, independent of career stage.
- **Three actual vehicle source models:** BMW 320i E90, Mercedes-Benz G-Class W263 and Mercedes-Benz AMG GT, with textures, original-author attribution, CC BY 4.0 licence text, provenance snapshots and checksums.
- **Delhi and Chandigarh road extracts:** actual OpenStreetMap geometry within 1.2 km of each centre, downloaded 15 November 2026 and included for offline use.
- City selection, touch steering/pedals, four cameras, automatic/manual gearing, fuel, traffic, railway crossings and checkpoint gameplay in source.
- Android packaging preflight, Unreal editor scripts for both city maps and static-model import, source validation and reproducible source packaging.

## Vehicle models and integration status

| Source model | Artist | Approximate triangles |
|---|---|---:|
| BMW 320i E90 | rifdanzz | 13,441 |
| Mercedes-Benz G-Class W263 | Lexyc16 | 12,516 |
| Mercedes-Benz AMG GT | Yan Carvalho | 166,321 |

Files and attribution are in [SourceAssets/Vehicles](SourceAssets/Vehicles/ATTRIBUTION.md). The manifest selects an uncompressed AMG derivative; original downloaded files are preserved.

These are static GLB scenes, **not rigged Chaos vehicles**. They do not yet supply skeletal meshes, wheel bones, animation Blueprints, physics assets or configured `DriveVehicle` Blueprints. Static import alone does not make a drivable car. The AMG also needs lower-detail meshes and fewer material sections for mobile use. Preserve artist credits when redistributing models or including them in a game.

Runtime profiles remain fictional names in `Source/DelhiDrive/CarCatalog.cpp`. They load `/Game/Cars/BP_<Id>` and fall back to `/Game/Cars/BP_Car_01`. No branded model is claimed to be integrated into that driving system yet.

## Maps and road conditions

| City extract | Centre | Radius |
|---|---|---:|
| Delhi / Connaught Place | 28.6315 N, 77.2167 E | 1.2 km |
| Chandigarh | 30.7333 N, 76.7794 E | 1.2 km |

See [road-data provenance](Content/Data/README.md). These are bounded flat road extracts, **not complete or photorealistic city maps**. Bridges and tunnels are excluded to avoid inventing at-grade junctions. Widths use mapped widths where available, otherwise documented estimates.

Mapped surface, smoothness, access and feature tags are retained. At most 150 explicitly mapped barrier nodes receive simple nonblocking marker posts; their shapes and open/closed states are unknown. Railway-crossing nodes seed the crossing simulation. Other retained feature tags are not all rendered. Missing pothole tags do not establish the absence of real potholes.

Buildings, traffic, random potholes, checkpoints and train schedules are generated game content, not current street conditions. There is no Google Maps or Apple Maps imagery/data in this package. OSM attribution is included in data files and the game menu.

## Career and controls

The stages run from First Drive through City Cruise, Market Run, Crossing Patrol, Pothole Precision, Evening Commute, Executive Express, Rush Hour, Rough Road Rally, Precision Sprint, Capital Challenge and Roadmaster Finale. Completion unlocks the next stage. Difficulty changes the clock, checkpoint count, hazards, crash penalty, fuel use and score multiplier.

| Screen | Keyboard controls |
|---|---|
| Menu | Left/Right car; Up/Down difficulty; Q/E stage; C city; G graphics; Enter start |
| Driving | W/S throttle/brake; A/D steer; Space handbrake; C camera; M gearbox; R reverse; E/Q shift; Tab menu |
| Result | Enter retry; N next level after success; Backspace menu |

Touch controls are implemented in `DrivePlayerController`. Career stars and car unlocks are saved locally and shared across both city maps. City selection lasts for the session; a new launch starts in Delhi. Checkpoint reachability and difficulty timing still require gameplay testing on connected road paths.

## Complete the Unreal build

1. Install **UE 5.5 with Android support**, its supported C++ compiler, and Epic's supported SDK/NDK/JDK versions. Generate project files and compile `DelhiDriveEditor`.
2. Import the supplied GLBs through Unreal Interchange. `Tools/import_vehicle_assets.py` validates sources and imports static geometry/materials. Preserve multipart scene transforms when assembling each vehicle; see the importer instructions.
3. Rig the vehicles and configure Chaos wheels, physics, animation and gearing. Create a child of `DriveVehicle` at `/Game/Cars/BP_Car_01`, then optional per-profile Blueprints. Migrating a complete Unreal Vehicle Template vehicle can establish an initial driving asset.
4. Set `UE_ROOT` and run `python Tools/setup_map.py`. Unreal creates `/Game/Maps/Lvl_Delhi` and `/Game/Maps/Lvl_Chandigarh` from the supplied data with lights and sky. Existing maps are preserved. No fake `.umap` files are generated outside Unreal.
5. Validate the editor game, run the build helper, then install and profile the APK on a physical phone.

Windows:

```bat
set "UE_ROOT=C:\Program Files\Epic Games\UE_5.5"
Tools\build_apk.bat
```

Linux/macOS with an Android-capable UE installation:

```sh
UE_ROOT="/path/to/UE_5.5" bash Tools/build_apk.sh
```

The helpers verify prerequisites, run Unreal Turnkey SDK checks and package an ARM64 Shipping ASTC build with data inside the APK. They stop on missing assets/toolchain and do not report success without an APK. Output goes to `Artifacts/Android-*`; `DELHIDRIVE_BUILD_OUTPUT` can override it. Configure release signing on the build machine; no signing keys are included.

## Performance changes and validation

Mobile budgets cap worlds at a 1.5 km radius, 180 generated potholes, 900 instanced buildings and 16 traffic vehicles. Buildings and mapped barrier markers use hierarchical instancing. Roads use at most three mesh sections. Low quality uses a 30 FPS cap; Medium/High use a 60 FPS cap. Traffic queries and checkpoint updates run less often than rendering. Mobile HDR, Lumen, Nanite, virtual shadow maps, motion blur and expensive postprocessing are disabled in the Android-oriented configuration.

These are budget/configuration changes, **not measured frame rates or a claim of stunning graphics**. Detailed city art, vehicle LODs, audio, final visual polish and physical-device profiling remain unfinished.

Repairs stop fuel consumption and controls in menus/results, keep checkpoint indices contiguous, place pothole visuals above the road, prevent car selection from raising the spawn point, and use a volume sweep so traffic detects gates and other vehicles.

Run checks without Unreal using Python 3.11 or newer:

```sh
python -m unittest discover -s Tools/tests -v
python Tools/validate_vehicle_sources.py
python Tools/import_vehicle_assets.py --validate-only
python Tools/preflight.py --source-only
```

The GitHub workflow runs source/data checks only. See [VALIDATION.md](VALIDATION.md) for delivered checks and remaining blockers. No Unreal compilation, APK or phone test is implied by a green source-validation workflow.

Map data © [OpenStreetMap contributors](https://www.openstreetmap.org/copyright), ODbL 1.0. Vehicle artwork: [individual CC BY 4.0 credits](SourceAssets/Vehicles/ATTRIBUTION.md).
