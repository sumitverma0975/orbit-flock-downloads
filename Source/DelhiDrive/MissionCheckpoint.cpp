#include "MissionCheckpoint.h"
#include "DriveGameMode.h"
#include "DriveVehicle.h"
#include "DriveShapes.h"
#include "Components/SphereComponent.h"
#include "Engine/World.h"

AMissionCheckpoint::AMissionCheckpoint()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.1f;

	Trigger = CreateDefaultSubobject<USphereComponent>(TEXT("Trigger"));
	RootComponent = Trigger;
	Trigger->SetSphereRadius(450.f);
	Trigger->SetCollisionProfileName(TEXT("OverlapAllDynamic"));

	Beam = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Beam"));
	Beam->SetupAttachment(Trigger);
	Beam->SetStaticMesh(DriveShapes::Cylinder());
	Beam->SetRelativeLocation(FVector(0.f, 0.f, 1000.f));
	Beam->SetRelativeScale3D(FVector(4.f, 4.f, 20.f));
	Beam->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Beam->SetCastShadow(false);
	const FVector Positions[] = { FVector(0.f,-150.f,6.f), FVector(0.f,150.f,6.f), FVector(-350.f,0.f,6.f), FVector(350.f,0.f,6.f), FVector(170.f,0.f,9.f), FVector(230.f,-35.f,9.f), FVector(230.f,35.f,9.f) };
	const FVector Scales[] = { FVector(7.f,.10f,.05f), FVector(7.f,.10f,.05f), FVector(.10f,3.f,.05f), FVector(.10f,3.f,.05f), FVector(1.8f,.12f,.05f), FVector(.8f,.12f,.05f), FVector(.8f,.12f,.05f) };
	for (int32 i=0;i<7;++i)
	{
		UStaticMeshComponent* Part = CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("ParkingMark%d"),i));
		Part->SetupAttachment(Trigger);
		Part->SetStaticMesh(DriveShapes::Cube());
		Part->SetRelativeLocation(Positions[i]);
		Part->SetRelativeScale3D(Scales[i]);
		if (i>=5) Part->SetRelativeRotation(FRotator(0.f,i==5?45.f:-45.f,0.f));
		Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Part->SetCastShadow(false);
		BayPieces.Add(Part);
	}
}

void AMissionCheckpoint::BeginPlay()
{
	Super::BeginPlay();
	DriveShapes::Tint(Beam, FLinearColor(1.f, 0.75f, 0.05f));
	for (UStaticMeshComponent* Part : BayPieces) DriveShapes::Tint(Part,FLinearColor(.1f,1.f,.65f));
	Trigger->OnComponentBeginOverlap.AddDynamic(this, &AMissionCheckpoint::OnOverlap);
	if (ADriveGameMode* GM = GetWorld()->GetAuthGameMode<ADriveGameMode>())
	{
		GM->RegisterCheckpoint(Index, GetActorLocation(), GetActorRotation());
	}
}

void AMissionCheckpoint::Tick(float DT)
{
	Super::Tick(DT);
	ADriveGameMode* GM = GetWorld()->GetAuthGameMode<ADriveGameMode>();
	const bool bActive = GM && GM->IsCheckpointActive(Index);
	if (!bStateKnown || bActive != bWasActive)
	{
		bStateKnown = true;
		bWasActive = bActive;
		SetActorHiddenInGame(!bActive);
		Trigger->SetGenerateOverlapEvents(bActive);
	}
	if (bActive)
	{
		const bool bBay = GM->Level().Objective == EDriveObjective::ControlledStop || (GM->Level().Objective == EDriveObjective::Parking && Index == GM->TotalCheckpoints-1);
		for (UStaticMeshComponent* Part : BayPieces) Part->SetVisibility(bBay);
		GM->EvaluateCheckpoint(Index,DT);
	}
}

void AMissionCheckpoint::OnOverlap(UPrimitiveComponent* OverlappedComp, AActor* Other, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!Cast<ADriveVehicle>(Other)) return;
	if (ADriveGameMode* GM = GetWorld()->GetAuthGameMode<ADriveGameMode>())
	{
		GM->EvaluateCheckpoint(Index,0.f);
	}
}
