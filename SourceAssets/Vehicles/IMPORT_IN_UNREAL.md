# Importing the vehicle source scenes into Unreal Engine 5.5

The three included vehicles are static GLB scenes. They contain real geometry and materials, but no skeleton, wheel rig, physics asset or drivable Chaos Blueprint. Their scene hierarchies matter: BMW has 49 mesh definitions, G-Class 22, and AMG GT 33. Importing isolated mesh parts and placing all of them at the origin can break the assembled car.

The import script and its Unreal Python API calls have been checked against UE5.5 documentation, but have **not run inside Unreal** in this workspace. Source file structure, checksums and attribution have been validated separately.

## Validate source files

From the project directory, run:

```sh
python Tools/validate_vehicle_sources.py
python Tools/import_vehicle_assets.py --validate-only
```

`asset_manifest.json` selects the source for each import. Use `mercedes-benz-amg-gt-import.glb` for the AMG GT: it is the decoded derivative without the original meshopt compression. The original compressed GLB is retained for provenance. Keep `ATTRIBUTION.md`, the license and provenance records when sharing these assets.

## Import complete static scenes

1. Open the project in Unreal Engine 5.5 after compiling its editor module. Enable the Python Editor Script and Interchange import support if they are unavailable.
2. Save all work in the current level. The importer opens separate preview worlds and refuses to proceed if an open map has unsaved changes.
3. Choose **Tools → Execute Python Script** and select `Tools/import_vehicle_assets.py`.
4. Inspect `Saved/VehicleAssetImport.json`. It records actual imported object paths, errors and the preview map created for each car. Geometry/material assets go to `/Game/Cars/Imported/<vehicle_id>`. Preview maps go to `/Game/VehicleImportPreviews/Preview_<vehicle_id>` and are not listed as packaged game maps.
5. Open each preview map. Check the whole vehicle, every wheel, glass/materials, dimensions and orientation. Use the viewport's Unlit mode when inspecting the geometry in these unlit preview worlds.

The script uses synchronous **Interchange scene import**, which recreates the source hierarchy and transforms along with the static mesh assets. It does not join all mesh parts or invent wheel pivots. Existing destination assets are preserved by default. For an intentional reimport, use Unreal's Python command runner with script arguments `--replace-existing`; it overwrites imported assets but creates a new preview map instead of overwriting an earlier preview. `--asset-id bmw_320i`, `mercedes_gclass` or `mercedes_amg_gt` limits an import to one car. Inspect any partial failure before retrying.

If automated Interchange import fails in your engine version, use Unreal's **File → Import Into Level** with the manifest's GLB file and the Interchange scene workflow. Keep the imported actor hierarchy and transforms, then save the preview level. A Content Browser mesh-only import is not an equivalent assembled-scene import. Alternatively, import the complete GLB scene in Blender, preserve world transforms while joining only the intended body parts, then export your assembled static mesh or properly rigged vehicle as FBX.

## Make a car drivable

Build a Chaos-compatible vehicle rig in a DCC tool, with a root/body bone, separate wheel bones at the correct pivots and valid skin weights. Verify centimetre scale and Unreal's vehicle axis convention. Import the skeletal mesh and skeleton; create/tune a physics asset, wheel classes, suspension, wheel bone assignments and a vehicle animation Blueprint. A migrated UE Vehicle-template rig can serve as a starting point only after its geometry and wheel layout are adapted correctly.

Create a Blueprint derived from this project's `DriveVehicle` class and configure its Chaos movement component, skeletal mesh, animation Blueprint and collision. Save a working fallback at `/Game/Cars/BP_Car_01`, or use the `/Game/Cars/BP_<catalog_id>` paths listed in `Source/DelhiDrive/CarCatalog.cpp` for individual profiles. Static import destinations do not automatically replace those Blueprint paths. Verify acceleration, steering, suspension, reverse, braking and collisions before packaging.

## Mobile asset work still required

The supplied source estimates are approximately 13,441 triangles for BMW, 12,516 for G-Class and **166,321 for AMG GT**, before any Unreal processing. The AMG GT's 30 material slots and separate parts need reduction and consolidation for a phone. Build and inspect LODs, reduce distant detail, cap textures appropriately (start with 1K for most surfaces), keep glass/materials simple, and disable unnecessary shadows and collision on decorative parts. Keep Nanite disabled for the mobile renderer. Confirm memory, frame time and visual quality on the target Android phone; source inclusion and import automation do not establish device performance.

## City maps and packaging

`python Tools/setup_map.py --ue-root /path/to/UE_5.5` builds the editor target and creates missing `/Game/Maps/Lvl_Delhi` and `/Game/Maps/Lvl_Chandigarh` maps using `delhi.json` and `chandigarh.json`. Existing maps are preserved individually. Both maps and `/Game/Cars` are configured for cooking. Run `python Tools/preflight.py --ue-root /path/to/UE_5.5` after supplying the drivable Blueprint; the build scripts also run Unreal Turnkey to check Android SDK/NDK compatibility.
