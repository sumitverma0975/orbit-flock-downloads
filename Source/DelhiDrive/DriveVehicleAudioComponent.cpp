#include "DriveVehicleAudioComponent.h"
#include "Components/AudioComponent.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"
#include "Sound/SoundWaveProcedural.h"

UDriveVehicleAudioComponent::UDriveVehicleAudioComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
}

void UDriveVehicleAudioComponent::BeginPlay()
{
	Super::BeginPlay();
	if (!GetWorld() || GetWorld()->GetNetMode() == NM_DedicatedServer || !GetOwner())
	{
		SetComponentTickEnabled(false);
		return;
	}

	// Runtime generation means this works in packaged games without editor sound imports.
	Wave = NewObject<USoundWaveProcedural>(this);
	Wave->SetSampleRate(DelhiDriveAudio::SampleRate);
	Wave->NumChannels = 1;
	Wave->Duration = INDEFINITELY_LOOPING_DURATION;
	Wave->bLooping = false; // The procedural queue supplies a continuous stream itself.
	Wave->VirtualizationMode = EVirtualizationMode::PlayWhenSilent;

	Output = NewObject<UAudioComponent>(GetOwner(), TEXT("DrivingAudioOutput"));
	Output->bAutoActivate = false;
	Output->bAutoDestroy = false;
	Output->bAllowSpatialization = false; // Player-local mix is consistent across all cameras.
	Output->SetupAttachment(GetOwner()->GetRootComponent());
	Output->RegisterComponent();
	Output->SetSound(Wave);
	Output->SetVolumeMultiplier(FMath::Clamp(MasterVolume, 0.f, 1.f));
	FillBuffer();
	Output->Play();
}

void UDriveVehicleAudioComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	if (Output)
	{
		Output->Stop();
		Output->DestroyComponent();
		Output = nullptr;
	}
	if (Wave) Wave->ResetAudio();
	Wave = nullptr;
	Super::EndPlay(Reason);
}

void UDriveVehicleAudioComponent::TickComponent(float DT, ELevelTick TickType, FActorComponentTickFunction* TickFunction)
{
	Super::TickComponent(DT, TickType, TickFunction);
	if (!Wave || !Output) return;
	Output->SetVolumeMultiplier(FMath::Clamp(MasterVolume, 0.f, 1.f));
	FillBuffer();
}

void UDriveVehicleAudioComponent::UpdateDrivingState(float RPM01, float Throttle01,
	float SpeedKmh, float Brake01, float Steer01, bool bHandbrake, bool bEngineRunning, bool bGameplayActive)
{
	Parameters.RPM = RPM01;
	Parameters.Throttle = Throttle01;
	Parameters.SpeedKmh = SpeedKmh;
	Parameters.Brake = Brake01;
	Parameters.Steer = Steer01;
	Parameters.bHandbrake = bHandbrake;
	Parameters.bEngineRunning = bEngineRunning;
	Parameters.bGameplayActive = bGameplayActive;
	Synth.SetParameters(Parameters);
}

void UDriveVehicleAudioComponent::PlayImpact(float Severity01)
{
	Synth.TriggerImpact(Severity01);
}

void UDriveVehicleAudioComponent::SetEngineCharacter(float PitchScale)
{
	Parameters.PitchScale = PitchScale;
	Synth.SetParameters(Parameters);
}

void UDriveVehicleAudioComponent::ResetAudioState()
{
	const float Character = Parameters.PitchScale;
	Parameters = DelhiDriveAudio::FParameters();
	Parameters.PitchScale = Character;
	Synth.SetParameters(Parameters);
	// Preserve oscillator phases and fade the old state; abruptly clearing PCM can pop.
}

void UDriveVehicleAudioComponent::FillBuffer()
{
	if (!Wave) return;
	// 100 ms covers ordinary mobile frame jitter; the queue cannot grow unbounded.
	constexpr int32 TargetSamples = DelhiDriveAudio::SampleRate / 10;
	const int32 QueuedSamples = Wave->GetAvailableAudioByteCount() / sizeof(int16);
	const int32 Needed = FMath::Clamp(TargetSamples - QueuedSamples, 0, TargetSamples);
	if (Needed <= 0) return;
	ScratchPCM.SetNumUninitialized(Needed, EAllowShrinking::No);
	Synth.Render(ScratchPCM.GetData(), Needed);
	Wave->QueueAudio(reinterpret_cast<const uint8*>(ScratchPCM.GetData()), Needed * sizeof(int16));
}
