#pragma once

#include "CoreMinimal.h"
#include "WheeledVehiclePawn.h"
#include "DriveVehicle.generated.h"

class UCameraComponent;
class USpringArmComponent;
class UChaosWheeledVehicleMovementComponent;
struct FCarSpec;

UENUM(BlueprintType)
enum class EDriveCamera : uint8
{
	Chase,
	Hood,
	Interior,
	Top
};

/**
 * Chaos-physics car with 4 cameras, automatic/manual gearbox, fuel and crash counting.
 * Make a Blueprint child per car: assign skeletal mesh, anim BP, wheel classes and
 * engine/gear setup in the Blueprint (see README).
 */
UCLASS()
class DELHIDRIVE_API ADriveVehicle : public AWheeledVehiclePawn
{
	GENERATED_BODY()

public:
	ADriveVehicle(const FObjectInitializer& ObjectInitializer);

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	// ---- called by the player controller ----
	void SetDriveInput(float InThrottle, float InBrake, float InSteer, bool bInHandbrake);
	void CycleCamera();
	void SetCamera(EDriveCamera NewCam);
	void ToggleAutomatic();
	void ToggleReverse();   // automatic mode: D <-> R
	void ShiftUp();         // manual mode
	void ShiftDown();       // manual mode (1 -> N -> R)

	/** Handling profile from the catalog (mass, power, top speed, brakes, steering). */
	void ApplySpec(const FCarSpec& InSpec);
	/** Teleport to the start line with a full tank and stopped. */
	void ResetForStart(const FVector& Loc, const FRotator& Rot);

	// ---- HUD queries ----
	float GetSpeedKmh() const;
	float GetRPMNormalized() const;
	FString GetGearLabel() const;
	bool IsAutomatic() const { return bAutomatic; }
	bool IsReverseSelected() const { return bReverse; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DelhiDrive|Fuel")
	float FuelPercent = 100.f;
	UPROPERTY(EditAnywhere, Category = "DelhiDrive|Fuel")
	float FuelBurnIdle = 0.012f;   // % per second
	UPROPERTY(EditAnywhere, Category = "DelhiDrive|Fuel")
	float FuelBurnFull = 0.09f;    // % per second at full throttle

	// Camera positions in cm, relative to the mesh. Indian cars are right-hand drive.
	UPROPERTY(EditAnywhere, Category = "DelhiDrive|Camera")
	FVector HoodCamOffset = FVector(150.f, 0.f, 135.f);
	UPROPERTY(EditAnywhere, Category = "DelhiDrive|Camera")
	FVector InteriorCamOffset = FVector(15.f, 35.f, 125.f);
	UPROPERTY(EditAnywhere, Category = "DelhiDrive|Camera")
	float ChaseDistance = 650.f;

protected:
	UPROPERTY(VisibleAnywhere, Category = "DelhiDrive|Camera") TObjectPtr<USpringArmComponent> ChaseArm;
	UPROPERTY(VisibleAnywhere, Category = "DelhiDrive|Camera") TObjectPtr<UCameraComponent> ChaseCam;
	UPROPERTY(VisibleAnywhere, Category = "DelhiDrive|Camera") TObjectPtr<UCameraComponent> HoodCam;
	UPROPERTY(VisibleAnywhere, Category = "DelhiDrive|Camera") TObjectPtr<UCameraComponent> InteriorCam;
	UPROPERTY(VisibleAnywhere, Category = "DelhiDrive|Camera") TObjectPtr<USpringArmComponent> TopArm;
	UPROPERTY(VisibleAnywhere, Category = "DelhiDrive|Camera") TObjectPtr<UCameraComponent> TopCam;

private:
	UFUNCTION()
	void OnMeshHit(UPrimitiveComponent* HitComp, AActor* Other, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

	void ApplyTransmission();
	/** The wheeled movement component (AWheeledVehiclePawn only returns the base type). */
	UChaosWheeledVehicleMovementComponent* Wheeled() const;

	const FCarSpec* Spec = nullptr;
	EDriveCamera CurrentCamera = EDriveCamera::Chase;
	bool bAutomatic = true;
	bool bReverse = false;

	float TargetThrottle = 0.f, TargetBrake = 0.f, TargetSteer = 0.f;
	float Throttle = 0.f, Brake = 0.f, Steer = 0.f;
	bool bHandbrake = false;

	float GearUpPulse = 0.f, GearDownPulse = 0.f;
	double LastCrashTime = -10.0;
};
