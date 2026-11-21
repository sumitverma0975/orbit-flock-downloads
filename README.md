# Delhi Drive Mobile

Delhi Drive is an offline 3D Android driving game built in Godot 4.6.3. It has a fictional city, 12 career challenges, four difficulty settings and a garage of 12 cars. Three of the cars use licensed BMW and Mercedes models. The game has original missions, interface, audio and city scenery.

Complete parking, checkpoint, precision and fuel challenges to earn stars and credits. Progress saves on the device. Drive with the touch pedals and steering pads, switch between automatic and manual gears, and choose chase, bonnet or overhead cameras.

- Install the signed ARM APK from the release. The APK supports Android 7 or later on ARM64 and ARMv7 devices that support OpenGL ES 3.0.
- To edit the game, open `project.godot` in Godot 4.6.3 and run the main scene. The built-in help lists keyboard controls for desktop testing.
- To export another Android build, follow [the build instructions](tools/BUILD_ANDROID.md).
- To check the catalog, driving and audio locally, run `godot --headless --path . --script res://tests/test_catalog.gd`, `godot --headless --path . --fixed-fps 60 --script res://tests/world_smoke.gd` and `godot --headless --path . --script res://tests/test_audio.gd`.

The renderer uses shared, instanced city geometry, bounded traffic and three graphics settings. Phone frame rate depends on the device. Vehicle art credits and original source downloads are in [the attribution file](SourceAssets/Vehicles/ATTRIBUTION.md). The underlying font licence is in [assets/fonts](assets/fonts/LICENSE.txt).

The Godot project draws a stylized fictional city. The separate Unreal source preview has the Delhi and Chandigarh OpenStreetMap extracts.
