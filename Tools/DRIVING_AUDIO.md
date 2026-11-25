# Generated driving audio

`UDriveVehicleAudioComponent` generates original 22,050 Hz mono PCM at runtime.
It combines an RPM/load-sensitive harmonic engine, speed-sensitive road noise,
braking/handbrake/cornering tire noise, and a short impact transient. These are
synthetic sounds, not recordings of a particular manufacturer's engine.
Tire intensity approximates slip from vehicle inputs and speed.

The implementation and generated audio are original project material. There
are no third-party recordings, external downloads, `.uasset` imports, or editor
sound setup steps. The compiled component creates its procedural sound and
audio output at runtime, including in Android packages. It uses only the
existing Engine module and keeps at most about 100 ms of queued PCM; one
player vehicle uses a single non-spatialized audio voice. Queued audio can
take up to 100 ms to reflect new input. Longer game-thread stalls may cause
audible dropouts. Native-device listening/performance still requires an
Unreal build and phone testing.

Create `UDriveVehicleAudioComponent` as a default subobject on the player car.
Call `UpdateDrivingState` every vehicle tick, including the menu/mission-over
branch with `bGameplayActive=false`. Pass the engine's running/fuel state
separately so rolling friction remains audible after fuel runs out. Call
`PlayImpact` with normalized collision severity and `ResetAudioState` when
restarting. `SetEngineCharacter` varies pitch per car; `MasterVolume` is editable.

Portable DSP verification and an optional listening sample:

```sh
c++ -std=c++17 -O2 Tools/verify_driving_audio.cpp -o /tmp/verify_driving_audio
/tmp/verify_driving_audio /tmp/delhi-driving-preview.wav
```

This checks generated sample behavior and writes an eight-second idle,
acceleration and braking/impact preview. It does not replace Unreal compilation
or an audio-device integration test.
