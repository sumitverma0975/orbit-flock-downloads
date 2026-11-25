#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "DriveVehicle.generated.h"

class UBoxComponent;
class UPrimitiveComponent;
class USceneComponent;
class UStaticMeshComponent;
class UCameraComponent;
class USpringArmComponent;
class UDriveVehicleAudioComponent;
struct FCarSpec;

UENUM(BlueprintType)
enum class EDriveCamera : uint8 { Chase, Hood, Interior, Top };

/** Source-only raycast vehicle: a Chaos rigid chassis and four spring/damper tyres.
 * No skeletal mesh, wheel Blueprint, animation Blueprint or ChaosVehicles setup is required.
 * Imported complete static car models are optional visuals; physics always uses the same native chassis.
 */
UCLASS()
class DELHIDRIVE_API ADriveVehicle : public APawn
{
	GENERATED_BODY()
public:
	ADriveVehicle(const FObjectInitializer& ObjectInitializer);
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual FVector GetVelocity() const override;

	void SetDriveInput(float InThrottle, float InBrake, float InSteer, bool bInHandbrake);
	void CycleCamera();
	void SetCamera(EDriveCamera NewCam);
	void ToggleAutomatic();
	void ToggleReverse();
	void ShiftUp();
	void ShiftDown();
	void ApplySpec(const FCarSpec& InSpec);
	void ResetForStart(const FVector& Loc, const FRotator& Rot);

	/** Compatibility accessor for pothole/collision gameplay. This is a primitive body, not a skeleton. */
	UPrimitiveComponent* GetMesh() const;
	float GetSpeedKmh() const;
	float GetSignedSpeedKmh() const;
	float GetRPMNormalized() const;
	float GetThrottleInput() const { return Throttle; }
	float GetBrakeInput() const { return Brake; }
	float GetSteerInput() const { return Steer; }
	float GetLongitudinalAccelerationMps2() const { return LongitudinalAcceleration; }
	FString GetGearLabel() const;
	FString GetVisualDescription() const;
	bool IsAutomatic() const { return bAutomatic; }
	bool IsReverseSelected() const { return Gear < 0; }
	bool HasImportedModel() const { return bHasImportedModel; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DelhiDrive|Fuel") float FuelPercent = 100.f;
	UPROPERTY(EditAnywhere, Category="DelhiDrive|Fuel") float FuelBurnIdle = 0.012f;
	UPROPERTY(EditAnywhere, Category="DelhiDrive|Fuel") float FuelBurnFull = 0.09f;
	UPROPERTY(EditAnywhere, Category="DelhiDrive|Camera") FVector HoodCamOffset = FVector(150.f, 0.f, 70.f);
	UPROPERTY(EditAnywhere, Category="DelhiDrive|Camera") FVector InteriorCamOffset = FVector(15.f, 35.f, 75.f);
	UPROPERTY(EditAnywhere, Category="DelhiDrive|Camera") float ChaseDistance = 650.f;

protected:
	UPROPERTY(VisibleAnywhere, Category="DelhiDrive|Physics") TObjectPtr<UBoxComponent> Chassis;
	UPROPERTY(VisibleAnywhere, Category="DelhiDrive|Visual") TObjectPtr<USceneComponent> VisualRoot;
	UPROPERTY(VisibleAnywhere, Category="DelhiDrive|Visual") TObjectPtr<UStaticMeshComponent> ImportedModel;
	UPROPERTY(VisibleAnywhere, Category="DelhiDrive|Visual") TArray<TObjectPtr<UStaticMeshComponent>> BodyParts;
	UPROPERTY(VisibleAnywhere, Category="DelhiDrive|Visual") TArray<TObjectPtr<UStaticMeshComponent>> WheelVisuals;
	UPROPERTY(VisibleAnywhere, Category="DelhiDrive|Audio") TObjectPtr<UDriveVehicleAudioComponent> DriveAudio;
	UPROPERTY(VisibleAnywhere, Category="DelhiDrive|Camera") TObjectPtr<USpringArmComponent> ChaseArm;
	UPROPERTY(VisibleAnywhere, Category="DelhiDrive|Camera") TObjectPtr<UCameraComponent> ChaseCam;
	UPROPERTY(VisibleAnywhere, Category="DelhiDrive|Camera") TObjectPtr<UCameraComponent> HoodCam;
	UPROPERTY(VisibleAnywhere, Category="DelhiDrive|Camera") TObjectPtr<UCameraComponent> InteriorCam;
	UPROPERTY(VisibleAnywhere, Category="DelhiDrive|Camera") TObjectPtr<USpringArmComponent> TopArm;
	UPROPERTY(VisibleAnywhere, Category="DelhiDrive|Camera") TObjectPtr<UCameraComponent> TopCam;

private:
	UFUNCTION() void OnMeshHit(UPrimitiveComponent* HitComp, AActor* Other, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);
	void SimulateWheels(float DT, bool bParked);
	void UpdateTransmission(float DT);
	void BuildVisuals();
	bool IsDrivenWheel(int32 Wheel) const;

	const FCarSpec* Spec = nullptr;
	FVector WheelAnchors[4]; // front left, front right, rear left, rear right
	float WheelSpin[4] = {0.f, 0.f, 0.f, 0.f};
	float SuspensionRest = 45.f;
	float SuspensionTravel = 18.f;
	float SuspensionSag = 15.f;
	float RideHeight = 54.f;
	float RPM = 900.f;
	float ShiftCooldown = 0.f;
	float PreviousForwardSpeed = 0.f;
	float LongitudinalAcceleration = 0.f;
	int32 Gear = 1; // reverse=-1, neutral=0, forward=1..6
	EDriveCamera CurrentCamera = EDriveCamera::Chase;
	bool bAutomatic = true;
	bool bHandbrake = false;
	bool bHasImportedModel = false;
	float TargetThrottle = 0.f, TargetBrake = 0.f, TargetSteer = 0.f;
	float Throttle = 0.f, Brake = 0.f, Steer = 0.f;
	double LastCrashTime = -10.0;
};
