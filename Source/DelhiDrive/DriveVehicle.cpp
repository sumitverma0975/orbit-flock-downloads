#include "DriveVehicle.h"
#include "DriveGameMode.h"
#include "CarCatalog.h"
#include "DriveShapes.h"
#include "DriveVehicleAudioComponent.h"
#include "DriveVehicleDynamics.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "CollisionQueryParams.h"

namespace
{
	constexpr float GravityCm = 980.f;
	constexpr float GearRatios[] = {3.45f, 2.20f, 1.55f, 1.16f, .92f, .76f};
	constexpr float FinalDrive = 3.4f;
	constexpr float IdleRPM = 900.f;
	constexpr float RedlineRPM = 7200.f;
}

ADriveVehicle::ADriveVehicle(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PrePhysics;
	Chassis = CreateDefaultSubobject<UBoxComponent>(TEXT("Chassis"));
	RootComponent = Chassis;
	Chassis->SetBoxExtent(FVector(210.f, 85.f, 23.f));
	Chassis->SetCollisionProfileName(TEXT("Pawn"));
	Chassis->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	Chassis->SetSimulatePhysics(true);
	Chassis->SetNotifyRigidBodyCollision(true);
	Chassis->SetGenerateOverlapEvents(true);
	Chassis->SetLinearDamping(.08f);
	Chassis->SetAngularDamping(1.2f);
	Chassis->BodyInstance.bUseCCD = true;
	VisualRoot = CreateDefaultSubobject<USceneComponent>(TEXT("Visuals"));
	VisualRoot->SetupAttachment(Chassis);
	ImportedModel = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ImportedCarModel"));
	ImportedModel->SetupAttachment(VisualRoot);
	ImportedModel->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ImportedModel->SetCastShadow(true);
	for (int32 i = 0; i < 12; ++i)
	{
		const FName Name(*FString::Printf(TEXT("BodyPart%d"), i));
		UStaticMeshComponent* Part = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		Part->SetupAttachment(VisualRoot);
		Part->SetStaticMesh(DriveShapes::Cube());
		Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Part->SetCastShadow(false);
		BodyParts.Add(Part);
	}
	for (int32 i = 0; i < 4; ++i)
	{
		const FName Name(*FString::Printf(TEXT("Wheel%d"), i));
		UStaticMeshComponent* Wheel = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		Wheel->SetupAttachment(VisualRoot);
		Wheel->SetStaticMesh(DriveShapes::Cylinder());
		Wheel->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Wheel->SetCastShadow(false);
		WheelVisuals.Add(Wheel);
	}
	DriveAudio = CreateDefaultSubobject<UDriveVehicleAudioComponent>(TEXT("VehicleAudio"));
	ChaseArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("ChaseArm"));
	ChaseArm->SetupAttachment(Chassis);
	ChaseArm->TargetArmLength = ChaseDistance;
	ChaseArm->SocketOffset = FVector(0.f, 0.f, 160.f);
	ChaseArm->SetRelativeRotation(FRotator(-12.f, 0.f, 0.f));
	ChaseArm->bDoCollisionTest = true;
	ChaseArm->ProbeChannel = ECC_Camera;
	ChaseArm->bInheritPitch = false;
	ChaseArm->bInheritRoll = false;
	ChaseArm->bEnableCameraLag = true;
	ChaseArm->CameraLagSpeed = 9.f;
	ChaseArm->bEnableCameraRotationLag = true;
	ChaseArm->CameraRotationLagSpeed = 7.f;
	ChaseCam = CreateDefaultSubobject<UCameraComponent>(TEXT("ChaseCam"));
	ChaseCam->SetupAttachment(ChaseArm, USpringArmComponent::SocketName);
	HoodCam = CreateDefaultSubobject<UCameraComponent>(TEXT("HoodCam"));
	HoodCam->SetupAttachment(Chassis);
	HoodCam->bAutoActivate = false;
	InteriorCam = CreateDefaultSubobject<UCameraComponent>(TEXT("InteriorCam"));
	InteriorCam->SetupAttachment(Chassis);
	InteriorCam->bAutoActivate = false;
	InteriorCam->FieldOfView = 95.f;
	TopArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("TopArm"));
	TopArm->SetupAttachment(Chassis);
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
	Chassis->OnComponentHit.AddDynamic(this, &ADriveVehicle::OnMeshHit);
	const ADriveGameMode* GM = GetWorld()->GetAuthGameMode<ADriveGameMode>();
	ApplySpec(DriveCatalog::Car(GM ? GM->CarIndex : 0));
	ChaseArm->TargetArmLength = ChaseDistance;
	SetCamera(EDriveCamera::Chase);
}

void ADriveVehicle::ApplySpec(const FCarSpec& InSpec)
{
	Spec = &InSpec;
	Chassis->SetBoxExtent(FVector(Spec->SizeCm.X * .44f, Spec->SizeCm.Y * .43f, 23.f));
	Chassis->SetMassOverrideInKg(NAME_None, Spec->MassKg, true);
	Chassis->SetCenterOfMass(FVector(0.f, 0.f, -18.f));
	RideHeight = Spec->WheelRadiusCm + SuspensionRest - SuspensionSag - 10.f;
	for (int32 i = 0; i < 4; ++i)
	{
		WheelAnchors[i] = FVector((i < 2 ? 1.f : -1.f) * Spec->WheelbaseCm * .5f,
			(i % 2 == 0 ? -1.f : 1.f) * Spec->TrackCm * .5f, 10.f);
	}
	HoodCam->SetRelativeLocation(FVector(Spec->SizeCm.X * .36f, 0.f, 65.f));
	InteriorCam->SetRelativeLocation(FVector(12.f, Spec->SizeCm.Y * .20f, Spec->SizeCm.Z - RideHeight - 18.f));
	DriveAudio->SetEngineCharacter(Spec->EnginePitch);
	BuildVisuals();
}

void ADriveVehicle::BuildVisuals()
{
	int32 Index = 0;
	for (int32 i = 0; i < DriveCatalog::NumCars(); ++i)
	{
		if (&DriveCatalog::Car(i) == Spec) { Index = i; break; }
	}
	UStaticMesh* Model = LoadObject<UStaticMesh>(nullptr, *DriveCatalog::CarStaticMeshPath(Index), nullptr, LOAD_NoWarn);
	bHasImportedModel = Model != nullptr;
	ImportedModel->SetStaticMesh(Model);
	ImportedModel->SetVisibility(bHasImportedModel);
	if (Model)
	{
		const FBox Bounds = Model->GetBoundingBox();
		const float Scale = Spec->SizeCm.X / FMath::Max(1.f, static_cast<float>(Bounds.GetSize().X));
		ImportedModel->SetRelativeScale3D(FVector(Scale));
		ImportedModel->SetRelativeLocation(FVector(-Bounds.GetCenter().X * Scale, -Bounds.GetCenter().Y * Scale,
			-Bounds.Min.Z * Scale - RideHeight));
	}
	for (UStaticMeshComponent* Part : BodyParts) Part->SetVisibility(!bHasImportedModel);
	for (UStaticMeshComponent* Wheel : WheelVisuals)
	{
		Wheel->SetVisibility(!bHasImportedModel);
		Wheel->SetRelativeScale3D(FVector(Spec->WheelRadiusCm / 50.f, Spec->WheelRadiusCm / 50.f, .23f));
		DriveShapes::Tint(Wheel, FLinearColor(.018f, .020f, .025f));
	}
	// The asset-free fallback is deliberately identified as an original proxy.
	// Its dimensions, body silhouette, wheelbase and handling still vary by profile.
	const float L = Spec->SizeCm.X, W = Spec->SizeCm.Y;
	const float Roof = Spec->SizeCm.Z - RideHeight;
	const bool bSUV = Spec->BodyStyle == ECarBodyStyle::SUV;
	const bool bEstate = Spec->BodyStyle == ECarBodyStyle::Estate;
	const bool bOpen = Spec->BodyStyle == ECarBodyStyle::Roadster;
	const bool bSport = Spec->Class == ECarClass::Supercar;
	const float CabinL = L * (bSUV || bEstate ? .64f : (bSport ? .38f : .52f));
	const float CabinZ = 23.f + (Roof - 23.f) * .5f;
	auto Part = [this](int32 I, const FVector& Size, const FVector& Location, const FLinearColor& Color)
	{
		BodyParts[I]->SetRelativeLocation(Location);
		BodyParts[I]->SetRelativeScale3D(Size / 100.f);
		DriveShapes::Tint(BodyParts[I], Color);
	};
	Part(0, FVector(L, W, 45.f), FVector(0, 0, 0), Spec->Paint);
	Part(1, FVector(CabinL, W * .78f, FMath::Max(15.f, Roof - 23.f)), FVector(-L * .05f, 0, CabinZ), FLinearColor(.035f,.07f,.10f));
	Part(2, FVector(CabinL * .80f, W * .72f, 5.f), FVector(-L * .05f, 0, Roof), Spec->Paint);
	Part(3, FVector(L * .28f, W * .91f, 12.f), FVector(L * .32f, 0, 22.f), Spec->Paint);
	Part(4, FVector(4.f, W * .55f, 15.f), FVector(L * .502f, 0, 1.f), FLinearColor(.025f,.027f,.032f));
	Part(5, FVector(5.f, W * .21f, 9.f), FVector(L * .503f, -W * .34f, 15.f), FLinearColor(.95f,.9f,.72f));
	Part(6, FVector(5.f, W * .21f, 9.f), FVector(L * .503f, W * .34f, 15.f), FLinearColor(.95f,.9f,.72f));
	Part(7, FVector(4.f, W * .25f, 9.f), FVector(-L * .503f, -W * .33f, 15.f), FLinearColor(.7f,.025f,.035f));
	Part(8, FVector(4.f, W * .25f, 9.f), FVector(-L * .503f, W * .33f, 15.f), FLinearColor(.7f,.025f,.035f));
	Part(9, FVector(24.f, W * .97f, 5.f), FVector(-L * .4f, 0, 44.f), Spec->Paint * .55f);
	Part(10, FVector(24.f, 14.f, 9.f), FVector(L * .12f, -W * .55f, 42.f), Spec->Paint);
	Part(11, FVector(24.f, 14.f, 9.f), FVector(L * .12f, W * .55f, 42.f), Spec->Paint);
	BodyParts[2]->SetVisibility(!bHasImportedModel && !bOpen);
	BodyParts[9]->SetVisibility(!bHasImportedModel && (Spec->BodyStyle == ECarBodyStyle::Track || Spec->BodyStyle == ECarBodyStyle::Hypercar));
}

void ADriveVehicle::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!Spec || DeltaSeconds <= 0.f) return;
	const float DT = FMath::Clamp(DeltaSeconds, .001f, .05f);
	const ADriveGameMode* GM = GetWorld()->GetAuthGameMode<ADriveGameMode>();
	const bool bActive = !GM || (!GM->bInMenu && !GM->bMissionOver);
	const float ForwardSpeed = FVector::DotProduct(GetVelocity(), GetActorForwardVector());
	LongitudinalAcceleration = FMath::FInterpTo(LongitudinalAcceleration,
		(ForwardSpeed - PreviousForwardSpeed) / FMath::Max(DeltaSeconds, .001f) / 100.f, DT, 8.f);
	PreviousForwardSpeed = ForwardSpeed;
	if (!bActive)
	{
		Throttle = Steer = TargetThrottle = TargetSteer = 0.f;
		Brake = 1.f;
	}
	else
	{
		Throttle = FMath::FInterpConstantTo(Throttle, FuelPercent > 0.f ? TargetThrottle : 0.f, DT, 3.f);
		Brake = FMath::FInterpConstantTo(Brake, TargetBrake, DT, 6.f);
		Steer = FMath::FInterpTo(Steer, TargetSteer, DT, 9.f);
		if (GM && GM->IsTour()) FuelPercent = 100.f;
		else FuelPercent = FMath::Max(0.f, FuelPercent - (FuelBurnIdle + Throttle * FuelBurnFull * (.4f + GetRPMNormalized())) * (GM ? GM->Diff().FuelBurnMult : 1.f) * DeltaSeconds);
	}
	UpdateTransmission(DT);
	SimulateWheels(DT, !bActive);
	DriveAudio->UpdateDrivingState(GetRPMNormalized(), Throttle, GetSpeedKmh(), Brake, Steer, bHandbrake,
		FuelPercent > 0.f, bActive);
}

bool ADriveVehicle::IsDrivenWheel(int32 Wheel) const
{
	return Spec->DriveType == EDriveTrain::All || (Spec->DriveType == EDriveTrain::Front ? Wheel < 2 : Wheel >= 2);
}

void ADriveVehicle::SimulateWheels(float DT, bool bParked)
{
	const FVector Up = GetActorUpVector();
	const FVector Forward = GetActorForwardVector();
	const FTransform Transform = GetActorTransform();
	const float Mass = FMath::Max(100.f, Chassis->GetMass());
	const float QuarterMass = Mass * .25f;
	const float QuarterWeight = QuarterMass * GravityCm;
	const float SpeedCm = FMath::Abs(FVector::DotProduct(GetVelocity(), Forward));
	const float MaxSpeed = Gear < 0 ? 35.f : Spec->TopSpeedKmh;
	const float Limiter = FMath::Clamp((MaxSpeed - SpeedCm * .036f) / 12.f, 0.f, 1.f);
	const float LaunchForce = Mass * Spec->LaunchAcceleration;
	const float GearTorqueScale = Gear > 0 ? FMath::Clamp(GearRatios[Gear - 1] / GearRatios[0], .28f, 1.f) : .7f;
	const float EngineForce = (Gear == 0 || bParked ? 0.f : Throttle * Limiter)
		* DriveVehicleDynamics::PowerLimitedForce(Spec->EnginePowerKw, SpeedCm, LaunchForce * GearTorqueScale) * (Gear < 0 ? -1.f : 1.f);
	const float DrivenWheels = Spec->DriveType == EDriveTrain::All ? 4.f : 2.f;
	const float SteerAngle = FMath::DegreesToRadians(Steer * 32.f * Spec->SteerScale * FMath::Lerp(1.f, .28f, FMath::Clamp(SpeedCm / 6000.f, 0.f, 1.f)));
	FCollisionQueryParams Query(SCENE_QUERY_STAT(DriveSuspension), false, this);
	int32 Grounded = 0;
	for (int32 i = 0; i < 4; ++i)
	{
		const FVector Anchor = Transform.TransformPosition(WheelAnchors[i]);
		const float Radius = Spec->WheelRadiusCm;
		FHitResult Hit;
		const bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, Anchor,
			Anchor - Up * (SuspensionRest + SuspensionTravel + Radius), ECC_Visibility, Query);
		float Length = SuspensionRest + SuspensionTravel;
		float WheelSpeed = 0.f;
		if (bHit && FVector::DotProduct(Hit.ImpactNormal, Up) > .30f)
		{
			++Grounded;
			Length = FMath::Clamp(Hit.Distance - Radius, SuspensionRest - SuspensionTravel, SuspensionRest + SuspensionTravel);
			const FVector PointVelocity = Chassis->GetPhysicsLinearVelocityAtPoint(Anchor);
			const float Compression = FMath::Max(0.f, SuspensionRest - Length);
			const float VerticalSpeed = static_cast<float>(FVector::DotProduct(PointVelocity, Up));
			const float Load = DriveVehicleDynamics::SuspensionLoad(Mass, SuspensionSag, Compression, VerticalSpeed);
			Chassis->AddForceAtLocation(Up * Load, Anchor);
			FVector TyreForward = i < 2 ? FQuat(Up, SteerAngle).RotateVector(Forward) : Forward;
			TyreForward = FVector::VectorPlaneProject(TyreForward, Hit.ImpactNormal).GetSafeNormal();
			const FVector TyreRight = FVector::CrossProduct(Hit.ImpactNormal, TyreForward).GetSafeNormal();
			WheelSpeed = FVector::DotProduct(PointVelocity, TyreForward);
			const float SideSpeed = FVector::DotProduct(PointVelocity, TyreRight);
			const bool bRearHand = (bHandbrake || bParked) && i >= 2;
			const float Grip = Spec->TireGrip * (bRearHand && !bParked ? .65f : 1.f);
			const float MaxTraction = Load * Grip;
			const float RequestedBrake = QuarterMass * (Brake * 1150.f * Spec->BrakeScale + (bRearHand ? 1500.f : 0.f));
			// Clamp brake + rolling resistance together so their sum cannot reverse a nearly stopped wheel.
			const float Resistance = DriveVehicleDynamics::OpposingBrakeForce(WheelSpeed, QuarterMass, RequestedBrake + QuarterWeight * .015f, DT);
			float LongForce = (IsDrivenWheel(i) ? EngineForce / DrivenWheels : 0.f) + Resistance;
			float SideForce = -SideSpeed * QuarterMass * 9.f;
			const DriveVehicleDynamics::FTireForces Limited = DriveVehicleDynamics::LimitTraction(LongForce, SideForce, MaxTraction);
			LongForce = Limited.Longitudinal;
			SideForce = Limited.Lateral;
			// Apply tyres near the contact patch. Lowered centre of mass provides roll stability.
			Chassis->AddForceAtLocation(TyreForward * LongForce + TyreRight * SideForce, Hit.ImpactPoint);
		}
		WheelSpin[i] = FMath::Fmod(WheelSpin[i] + WheelSpeed / FMath::Max(1.f, Radius) * DT, 2.f * PI);
		WheelVisuals[i]->SetRelativeLocation(WheelAnchors[i] - FVector::UpVector * Length);
		const FQuat Steering(FVector::UpVector, i < 2 ? SteerAngle : 0.f);
		const FQuat Spin(FVector::RightVector, WheelSpin[i]);
		const FQuat Axle(FVector::ForwardVector, PI * .5f);
		WheelVisuals[i]->SetRelativeRotation(Steering * Spin * Axle);
	}
	// Aerodynamic drag in kg*cm/s^2; road rolling resistance is applied only at contacts.
	const FVector Velocity = GetVelocity();
	Chassis->AddForce(-Velocity * Velocity.Size() * .0038f);
	if (Grounded >= 2)
	{
		const float Downforce = Mass * 80.f * FMath::Square(FMath::Clamp(SpeedCm / 8000.f, 0.f, 1.5f));
		Chassis->AddForce(-Up * Downforce);
	}
}

void ADriveVehicle::UpdateTransmission(float DT)
{
	ShiftCooldown = FMath::Max(0.f, ShiftCooldown - DT);
	const float WheelRPM = FMath::Abs(GetSignedSpeedKmh()) / .036f / (2.f * PI * Spec->WheelRadiusCm) * 60.f;
	if (bAutomatic && Gear > 0 && ShiftCooldown <= 0.f)
	{
		const float CoupledRPM = WheelRPM * GearRatios[Gear - 1] * FinalDrive;
		if (CoupledRPM > 6200.f && Gear < 6) { ++Gear; ShiftCooldown = .35f; }
		else if (CoupledRPM < 2200.f && Gear > 1) { --Gear; ShiftCooldown = .35f; }
	}
	const float Ratio = Gear > 0 ? GearRatios[Gear - 1] : 3.f;
	const float TargetRPM = Gear == 0 ? IdleRPM + Throttle * 5500.f : FMath::Max(IdleRPM + Throttle * 400.f, WheelRPM * Ratio * FinalDrive);
	RPM = FMath::FInterpTo(RPM, FMath::Clamp(TargetRPM, IdleRPM, RedlineRPM), DT, 10.f);
}

void ADriveVehicle::SetDriveInput(float InThrottle, float InBrake, float InSteer, bool bInHandbrake)
{
	TargetThrottle = FMath::IsFinite(InThrottle) ? FMath::Clamp(InThrottle, 0.f, 1.f) : 0.f;
	TargetBrake = FMath::IsFinite(InBrake) ? FMath::Clamp(InBrake, 0.f, 1.f) : 0.f;
	TargetSteer = FMath::IsFinite(InSteer) ? FMath::Clamp(InSteer, -1.f, 1.f) : 0.f;
	bHandbrake = bInHandbrake;
}
void ADriveVehicle::ToggleAutomatic()
{
	bAutomatic = !bAutomatic;
	if (Gear == 0) Gear = 1;
	ShiftCooldown = .3f;
}
void ADriveVehicle::ToggleReverse()
{
	if (bAutomatic && GetSpeedKmh() < 5.f) { Gear = Gear < 0 ? 1 : -1; ShiftCooldown = .3f; }
}
void ADriveVehicle::ShiftUp()
{
	if (!bAutomatic && ShiftCooldown <= 0.f)
	{
		if (Gear < 1 && GetSpeedKmh() >= 5.f) return;
		Gear = FMath::Min(6, Gear + 1); ShiftCooldown = .25f;
	}
}
void ADriveVehicle::ShiftDown()
{
	if (!bAutomatic && ShiftCooldown <= 0.f)
	{
		if (Gear <= 1 && GetSpeedKmh() >= 5.f) return;
		Gear = FMath::Max(-1, Gear - 1); ShiftCooldown = .25f;
	}
}
void ADriveVehicle::ResetForStart(const FVector& Loc, const FRotator& Rot)
{
	SetActorLocationAndRotation(Loc, Rot, false, nullptr, ETeleportType::TeleportPhysics);
	Chassis->SetPhysicsLinearVelocity(FVector::ZeroVector);
	Chassis->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
	Chassis->WakeAllRigidBodies();
	FuelPercent = 100.f;
	Throttle = Brake = Steer = TargetThrottle = TargetBrake = TargetSteer = 0.f;
	PreviousForwardSpeed = LongitudinalAcceleration = 0.f;
	bHandbrake = false; Gear = 1; RPM = IdleRPM; ShiftCooldown = 0.f;
	LastCrashTime = GetWorld()->GetTimeSeconds();
	DriveAudio->ResetAudioState();
}
UPrimitiveComponent* ADriveVehicle::GetMesh() const { return Chassis; }
FVector ADriveVehicle::GetVelocity() const { return Chassis ? Chassis->GetPhysicsLinearVelocity() : FVector::ZeroVector; }
float ADriveVehicle::GetSpeedKmh() const { return GetVelocity().Size() * .036f; }
float ADriveVehicle::GetSignedSpeedKmh() const { return FVector::DotProduct(GetVelocity(), GetActorForwardVector()) * .036f; }
float ADriveVehicle::GetRPMNormalized() const { return FMath::Clamp(RPM / RedlineRPM, 0.f, 1.f); }
FString ADriveVehicle::GetGearLabel() const
{
	if (Gear < 0) return TEXT("R");
	if (Gear == 0) return TEXT("N");
	return bAutomatic ? FString::Printf(TEXT("D%d"), Gear) : FString::FromInt(Gear);
}
FString ADriveVehicle::GetVisualDescription() const
{
	if (!Spec) return TEXT("Native vehicle");
	return bHasImportedModel ? FString(Spec->DesignLabel) : TEXT("Original source-built proxy; game-tuned handling");
}
void ADriveVehicle::CycleCamera() { SetCamera(static_cast<EDriveCamera>((static_cast<uint8>(CurrentCamera) + 1) % 4)); }
void ADriveVehicle::SetCamera(EDriveCamera NewCam)
{
	CurrentCamera = NewCam;
	ChaseCam->SetActive(NewCam == EDriveCamera::Chase); HoodCam->SetActive(NewCam == EDriveCamera::Hood);
	InteriorCam->SetActive(NewCam == EDriveCamera::Interior); TopCam->SetActive(NewCam == EDriveCamera::Top);
	// Static showroom models have opaque cabins, so hide the visual shell in cockpit view.
	VisualRoot->SetVisibility(NewCam != EDriveCamera::Interior, true);
	if (NewCam != EDriveCamera::Interior && Spec) // preserve imported/proxy visibility without recreating materials
	{
		ImportedModel->SetVisibility(bHasImportedModel);
		for (UStaticMeshComponent* Part : BodyParts) Part->SetVisibility(!bHasImportedModel);
		for (UStaticMeshComponent* Wheel : WheelVisuals) Wheel->SetVisibility(!bHasImportedModel);
		BodyParts[2]->SetVisibility(!bHasImportedModel && Spec->BodyStyle != ECarBodyStyle::Roadster);
		BodyParts[9]->SetVisibility(!bHasImportedModel && (Spec->BodyStyle == ECarBodyStyle::Track || Spec->BodyStyle == ECarBodyStyle::Hypercar));
	}
}
void ADriveVehicle::OnMeshHit(UPrimitiveComponent* HitComp, AActor* Other, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	const float Mass = Chassis->GetMass();
	const double Now = GetWorld()->GetTimeSeconds();
	const float Severity = Mass > 0.f ? NormalImpulse.Size() / Mass : 0.f;
	if (Severity > 150.f && Now - LastCrashTime > 1.0)
	{
		LastCrashTime = Now;
		DriveAudio->PlayImpact(FMath::Clamp(Severity / 1500.f, .1f, 1.f));
		if (ADriveGameMode* GM = GetWorld()->GetAuthGameMode<ADriveGameMode>()) GM->ReportCollision();
	}
}
