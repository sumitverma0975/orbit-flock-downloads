#include "DriveVehicle.h"
#include "DriveGameMode.h"
#include "CarCatalog.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"

ADriveVehicle::ADriveVehicle(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true;

	GetMesh()->SetNotifyRigidBodyCollision(true);
	GetMesh()->SetGenerateOverlapEvents(true);

	// Chase camera
	ChaseArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("ChaseArm"));
	ChaseArm->SetupAttachment(GetMesh());
	ChaseArm->TargetArmLength = ChaseDistance;
	ChaseArm->SocketOffset = FVector(0.f, 0.f, 160.f);
	ChaseArm->SetRelativeRotation(FRotator(-10.f, 0.f, 0.f));
	ChaseArm->bDoCollisionTest = false;
	ChaseArm->bInheritPitch = false;
	ChaseArm->bInheritRoll = false;
	ChaseArm->bEnableCameraLag = true;
	ChaseArm->CameraLagSpeed = 9.f;
	ChaseArm->bEnableCameraRotationLag = true;
	ChaseArm->CameraRotationLagSpeed = 6.f;

	ChaseCam = CreateDefaultSubobject<UCameraComponent>(TEXT("ChaseCam"));
	ChaseCam->SetupAttachment(ChaseArm, USpringArmComponent::SocketName);
	ChaseCam->bUsePawnControlRotation = false;

	// Bonnet / hood camera
	HoodCam = CreateDefaultSubobject<UCameraComponent>(TEXT("HoodCam"));
	HoodCam->SetupAttachment(GetMesh());
	HoodCam->bAutoActivate = false;

	// Interior (driver's eye, right-hand drive)
	InteriorCam = CreateDefaultSubobject<UCameraComponent>(TEXT("InteriorCam"));
	InteriorCam->SetupAttachment(GetMesh());
	InteriorCam->bAutoActivate = false;
	InteriorCam->FieldOfView = 95.f;

	// Top-down (heading up)
	TopArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("TopArm"));
	TopArm->SetupAttachment(GetMesh());
	TopArm->TargetArmLength = 2200.f;
	TopArm->SetRelativeRotation(FRotator(-90.f, 0.f, 0.f));
	TopArm->bDoCollisionTest = false;
	TopArm->bInheritPitch = false;
	TopArm->bInheritRoll = false;

	TopCam = CreateDefaultSubobject<UCameraComponent>(TEXT("TopCam"));
	TopCam->SetupAttachment(TopArm, USpringArmComponent::SocketName);
	TopCam->bAutoActivate = false;
}

void ADriveVehicle::BeginPlay()
{
	Super::BeginPlay();

	ChaseArm->TargetArmLength = ChaseDistance;
	HoodCam->SetRelativeLocation(HoodCamOffset);
	InteriorCam->SetRelativeLocation(InteriorCamOffset);

	GetMesh()->OnComponentHit.AddDynamic(this, &ADriveVehicle::OnMeshHit);

	ApplyTransmission();
	SetCamera(EDriveCamera::Chase);

	if (const ADriveGameMode* GM = GetWorld()->GetAuthGameMode<ADriveGameMode>())
	{
		ApplySpec(DriveCatalog::Car(GM->CarIndex));
	}
}

void ADriveVehicle::Tick(float DT)
{
	Super::Tick(DT);

	UChaosWheeledVehicleMovementComponent* VM = Wheeled();
	if (!VM) return;
	const ADriveGameMode* GM = GetWorld()->GetAuthGameMode<ADriveGameMode>();
	if (GM && (GM->bInMenu || GM->bMissionOver))
	{
		Throttle = Brake = Steer = 0.f;
		TargetThrottle = TargetBrake = TargetSteer = 0.f;
		GearUpPulse = GearDownPulse = 0.f;
		VM->SetThrottleInput(0.f);
		VM->SetBrakeInput(1.f);
		VM->SetSteeringInput(0.f);
		VM->SetHandbrakeInput(true);
		VM->SetChangeUpInput(false);
		VM->SetChangeDownInput(false);
		return; // fuel only burns during play
	}

	// Touch pedals are on/off, so ease the inputs like real pedals.
	const float TT = (FuelPercent > 0.f) ? TargetThrottle : 0.f;
	Throttle = FMath::FInterpConstantTo(Throttle, TT, DT, (TT > Throttle) ? 2.2f : 6.f);
	Brake    = FMath::FInterpConstantTo(Brake, TargetBrake, DT, (TargetBrake > Brake) ? 4.f : 8.f);
	Steer    = FMath::FInterpTo(Steer, TargetSteer, DT, 10.f);

	// Car profile: scale the Blueprint's engine, brakes and steering, and soft-limit top speed.
	const float SpeedKmh = GetSpeedKmh();
	const float PowerScale = Spec ? Spec->PowerScale : 1.f;
	const float TopLimiter = Spec ? FMath::Clamp((Spec->TopSpeedKmh - SpeedKmh) / 15.f, 0.f, 1.f) : 1.f;
	const float BrakeScale = Spec ? Spec->BrakeScale : 1.f;
	const float SteerScale = Spec ? Spec->SteerScale : 1.f;

	VM->SetThrottleInput(Throttle * PowerScale * TopLimiter);
	VM->SetBrakeInput(FMath::Min(1.f, Brake * BrakeScale));
	VM->SetSteeringInput(FMath::Clamp(Steer * SteerScale, -1.f, 1.f));
	VM->SetHandbrakeInput(bHandbrake);

	GearUpPulse -= DT;
	GearDownPulse -= DT;
	VM->SetChangeUpInput(GearUpPulse > 0.f);
	VM->SetChangeDownInput(GearDownPulse > 0.f);

	const float FuelMult = GM ? GM->Diff().FuelBurnMult : 1.f;
	const float Burn = (FuelBurnIdle + Throttle * FuelBurnFull * (0.4f + GetRPMNormalized())) * FuelMult;
	FuelPercent = FMath::Max(0.f, FuelPercent - Burn * DT);
}

void ADriveVehicle::SetDriveInput(float InThrottle, float InBrake, float InSteer, bool bInHandbrake)
{
	TargetThrottle = FMath::Clamp(InThrottle, 0.f, 1.f);
	TargetBrake    = FMath::Clamp(InBrake, 0.f, 1.f);
	TargetSteer    = FMath::Clamp(InSteer, -1.f, 1.f);
	bHandbrake     = bInHandbrake;
}

void ADriveVehicle::CycleCamera()
{
	SetCamera(static_cast<EDriveCamera>((static_cast<uint8>(CurrentCamera) + 1) % 4));
}

void ADriveVehicle::SetCamera(EDriveCamera NewCam)
{
	CurrentCamera = NewCam;
	ChaseCam->SetActive(NewCam == EDriveCamera::Chase);
	HoodCam->SetActive(NewCam == EDriveCamera::Hood);
	InteriorCam->SetActive(NewCam == EDriveCamera::Interior);
	TopCam->SetActive(NewCam == EDriveCamera::Top);
}

void ADriveVehicle::ToggleAutomatic()
{
	bAutomatic = !bAutomatic;
	bReverse = false;
	ApplyTransmission();
}

void ADriveVehicle::ToggleReverse()
{
	if (!bAutomatic) return;
	bReverse = !bReverse;
	ApplyTransmission();
}

void ADriveVehicle::ShiftUp()
{
	if (!bAutomatic) GearUpPulse = 0.12f;
}

void ADriveVehicle::ShiftDown()
{
	if (!bAutomatic) GearDownPulse = 0.12f;
}

UChaosWheeledVehicleMovementComponent* ADriveVehicle::Wheeled() const
{
	return Cast<UChaosWheeledVehicleMovementComponent>(GetVehicleMovementComponent());
}

void ADriveVehicle::ApplySpec(const FCarSpec& InSpec)
{
	Spec = &InSpec;
	GetMesh()->SetMassOverrideInKg(NAME_None, InSpec.MassKg, true);
}

void ADriveVehicle::ResetForStart(const FVector& Loc, const FRotator& Rot)
{
	SetActorLocationAndRotation(Loc, Rot, false, nullptr, ETeleportType::TeleportPhysics);
	GetMesh()->SetAllPhysicsLinearVelocity(FVector::ZeroVector);
	GetMesh()->SetAllPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
	FuelPercent = 100.f;
	Throttle = Brake = Steer = 0.f;
	TargetThrottle = TargetBrake = TargetSteer = 0.f;
	GearUpPulse = GearDownPulse = 0.f;
	bHandbrake = false;
	bReverse = false;
	ApplyTransmission();
}

void ADriveVehicle::ApplyTransmission()
{
	if (UChaosWheeledVehicleMovementComponent* VM = Wheeled())
	{
		// Automatic: Chaos shifts for us in D; R locks the box in reverse.
		// Manual: the player shifts with UP / DOWN.
		VM->SetUseAutomaticGears(bAutomatic && !bReverse);
		VM->SetTargetGear(bReverse ? -1 : 1, true);
	}
}

float ADriveVehicle::GetSpeedKmh() const
{
	const UChaosWheeledVehicleMovementComponent* VM = Wheeled();
	return VM ? FMath::Abs(VM->GetForwardSpeed()) * 0.036f : 0.f; // cm/s -> km/h
}

float ADriveVehicle::GetRPMNormalized() const
{
	const UChaosWheeledVehicleMovementComponent* VM = Wheeled();
	if (!VM) return 0.f;
	const float MaxRPM = VM->GetEngineMaxRotationSpeed();
	return MaxRPM > 1.f ? FMath::Clamp(VM->GetEngineRotationSpeed() / MaxRPM, 0.f, 1.f) : 0.f;
}

FString ADriveVehicle::GetGearLabel() const
{
	const UChaosWheeledVehicleMovementComponent* VM = Wheeled();
	if (!VM) return TEXT("-");
	const int32 G = VM->GetCurrentGear();
	if (G < 0) return TEXT("R");
	if (G == 0) return TEXT("N");
	return bAutomatic ? FString::Printf(TEXT("D%d"), G) : FString::FromInt(G);
}

void ADriveVehicle::OnMeshHit(UPrimitiveComponent* HitComp, AActor* Other, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	const float Mass = GetMesh()->GetMass();
	const double Now = GetWorld()->GetTimeSeconds();
	if (Mass > 0.f && (NormalImpulse.Size() / Mass) > 150.f && (Now - LastCrashTime) > 1.0)
	{
		LastCrashTime = Now;
		if (ADriveGameMode* GM = GetWorld()->GetAuthGameMode<ADriveGameMode>())
		{
			GM->ReportCollision();
		}
	}
}
