# Vehicle visuals for native Unreal physics

The prepared pack contains **twelve complete static car visuals**. Three use attributed BMW/Mercedes artwork; nine are original fictional designs. Every mesh includes bodywork, glazing, wheels, lamps and trim. The native `DriveVehicle` implementation supplies suspension, movement and chassis collision independently of the artwork. Imported visual wheels are static; these are not skeletal Chaos rigs.

## Included runtime paths

Each catalog ID maps to `/Game/Cars/Models/SM_<Id>.SM_<Id>`:

| Catalog ID | Visual | Source triangles |
|---|---|---:|
| lux_sedan | BMW 320i E90 | 13,441 |
| lux_suv | Mercedes-Benz G-Class | 12,516 |
| lux_coupe | Rhein Sport Coupe, original | 9,852 |
| lux_limo | Berlin Executive L, original | 9,836 |
| sup_gt | Mercedes-AMG GT | 54,908 |
| sup_mid | Stuttgart Mid-Engine, original | 9,552 |
| sup_hyper | Autobahn Hyper GT, original | 10,048 |
| sup_track | Nuerburg Track RS, original | 10,288 |
| lux_estate | Rhein Touring RS, original | 10,084 |
| lux_grand | Black Forest GT, original | 9,776 |
| sup_spider | Tejas Spider, original | 9,744 |
| sup_apex | Vajra Apex, original | 10,276 |

The originals are fictional models; they are not representations of additional manufacturers' cars. The named BMW/Mercedes artwork credits and licence remain in [ATTRIBUTION.md](ATTRIBUTION.md).

## Unattended import

Prepared GLBs and their hashes are already included. **Blender is not needed to import the shipped pack.** With the project's editor module compiled in Unreal Engine 5.5, run:

```text
UnrealEditor-Cmd <absolute-path>/DelhiDrive.uproject -run=pythonscript -script=<absolute-path>/Tools/import_vehicle_assets.py -unattended -nop4 -NullRHI
```

The packaging setup script invokes this step automatically. The importer uses automated asset tasks and per-car material directories, then gives the single imported body its stable runtime path. It never relies on an imported scene hierarchy or a manually configured car Blueprint.

`Saved/VehicleAssetImport.json` uses schema version 2. `success` becomes `true` only after every catalog body exists as an actual Unreal StaticMesh and its dimensions, axes and ground origin pass checks. Each entry includes the source SHA-256, asset path, status and bounds. A commandlet exit code alone is not evidence of success. Existing generated assets are preserved only when their previously recorded source hash and current bounds match; use `--replace-existing` intentionally to replace them.

Expected Unreal geometry: centimetres, car forward along **+X**, right along **+Y**, up along **+Z**, centred horizontally, tyres touching Z=0. The native visual attachment fits the body to its driving profile and sets its chassis offset. Prepared GLBs use metres and glTF Y-up, with the car deliberately facing +X so the importer preserves the longitudinal axis when converting up and handedness.

The importer requests two reduced LODs, removes artwork collision (native chassis handles collision), and disables Nanite. Source triangle counts are already bounded if an engine-specific LOD call is unavailable; such calls are recorded as notes. LOD appearance, materials and sustained phone performance must still be checked in Unreal and on a device.

## Regenerate art

The source artwork and Blender generator are provided for reproducibility:

```sh
blender -b --python Tools/art/build_vehicle_models.py
python Tools/validate_vehicle_sources.py
python Tools/import_vehicle_assets.py --validate-only
```

The Blender script bakes source hierarchies, removes the AMG presentation floor, recentres/grounds the car, fits its length, merges material slots, reduces the AMG's geometry budget and writes one identity-node GLB per car. It also creates the nine distinct original models, including a coupe, limousine, estate, grand tourer, mid-engine car, track car, roadster and two hypercar designs. An optional trailing catalog ID after `--` rebuilds only that car.

`prepared_manifest.json` records all generated hashes, dimensions and counts. `asset_manifest.json` separately preserves the original third-party downloads and their provenance. Keep both sets of credits when distributing derivatives.

## Verification status

All twelve prepared files passed host validation for GLB structure, embedded resources, identity transforms, catalog coverage, hashes, axes, dimensions, grounded/centred origins and triangle budgets. The three third-party originals also passed their existing provenance checks. **Unreal import has not been run in this workspace, so no successful engine import, APK, gameplay capture or phone benchmark is asserted by this document.**

Any `Docs/vehicle-garage-preview.png` image is labelled as an offline Blender asset preview. It demonstrates the actual model geometry and materials, not an Unreal frame or finished game.
