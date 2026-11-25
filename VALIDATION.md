# Unreal source milestone verification

The Android build command exited with code 1 on this host. Unreal Engine 5.5 and an accessible Android-capable Unreal build worker were unavailable. The source archive is downloadable. A signed Unreal APK, an Unreal gameplay capture, and measured on-device performance are pending.

| Check | Result |
| --- | --- |
| Vehicle inputs | All twelve prepared GLBs matched their catalog IDs, source checksums, dimensions and expected axes. All three externally sourced models had provenance records. |
| City geometry | 862 Delhi and 677 Chandigarh road parts passed source checks. The 1,299 Delhi and 1,088 Chandigarh building outlines have indexed roofs. Each roof triangle has the expected winding, and their combined area matches its footprint. |
| Route logic | The portable C++ router selected connected directed road segments on both districts, across four seeds per district. Synthetic topology tests also passed. |
| Mission logic | All twelve career specifications and six lessons passed a standalone catalog check. The C++ goal evaluator passed 27 boundary cases, including stopping, parking, score and fuel checks. |
| Vehicle and audio math | Standalone C++ suspension, braking and traction checks passed at 30/60/120 Hz. Synthesized audio checks passed for engine, rolling, braking, impact and silence. These exercise the shared equations and DSP without the Unreal runtime. |
| Packaging logic | Nine regression tests covered artifact type, payload structure, signature failure, size limit and editor setup completion markers. Source preflight and syntax checks passed. |
| Visuals | All twelve car models rendered offline in Blender. The [garage image](Docs/vehicle-garage-preview.png) shows the shipped art. Final lighting, wheel contact, materials, touch controls and frame timing require testing in Unreal. |
| Build attempt | [Android build log](Docs/UE5-build-attempt.log). No editor target build, asset import, map generation, cook, signing or installation ran after the engine prerequisite failed. |

The full-game goal remains open. Current coverage is two districts on flat terrain, a shared itinerary per city, twelve original career missions, six driving lessons and free touring. Potholes and traffic are simulations. GTA-level image quality, full city coverage, every Dr. Driving mission, and a game download under 3 GB have not been demonstrated.

Run the source checks with:

```sh
python3 -m unittest discover -s Tools/tests -v
python3 Tools/tests/test_mission_catalog.py
c++ -std=c++17 -Wall -Wextra -Werror Tests/native_vehicle_dynamics_test.cpp -o /tmp/dd-vehicle && /tmp/dd-vehicle
c++ -std=c++17 -Wall -Wextra -Werror Tools/verify_driving_audio.cpp -o /tmp/dd-audio && /tmp/dd-audio
python3 Tools/validate_vehicle_sources.py
python3 Tools/preflight.py --source-only
```
