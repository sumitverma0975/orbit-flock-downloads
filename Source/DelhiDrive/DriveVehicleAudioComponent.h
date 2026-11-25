#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DriveAudioSynth.h"
#include "DriveVehicleAudioComponent.generated.h"

class UAudioComponent;
class USoundWaveProcedural;

/** One lightweight generated mono stream for the player's engine, tires, road and impacts. */
UCLASS(ClassGroup=(DelhiDrive), meta=(BlueprintSpawnableComponent))
class DELHIDRIVE_API UDriveVehicleAudioComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDriveVehicleAudioComponent();
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	void UpdateDrivingState(float RPM01, float Throttle01, float SpeedKmh,
		float Brake01, float Steer01, bool bHandbrake, bool bEngineRunning, bool bGameplayActive);
	void PlayImpact(float Severity01);
	void SetEngineCharacter(float PitchScale);
	void ResetAudioState();

	UPROPERTY(EditAnywhere, Category="DelhiDrive|Audio", meta=(ClampMin="0.0", ClampMax="1.0"))
	float MasterVolume = 0.7f;

private:
	void FillBuffer();
	UPROPERTY(Transient) TObjectPtr<UAudioComponent> Output;
	UPROPERTY(Transient) TObjectPtr<USoundWaveProcedural> Wave;
	DelhiDriveAudio::FParameters Parameters;
	DelhiDriveAudio::FSynthesizer Synth;
	TArray<int16> ScratchPCM;
};
