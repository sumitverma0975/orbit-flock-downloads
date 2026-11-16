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
}

void AMissionCheckpoint::BeginPlay()
{
	Super::BeginPlay();
	DriveShapes::Tint(Beam, FLinearColor(1.f, 0.75f, 0.05f));
	Trigger->OnComponentBeginOverlap.AddDynamic(this, &AMissionCheckpoint::OnOverlap);
	if (ADriveGameMode* GM = GetWorld()->GetAuthGameMode<ADriveGameMode>())
	{
		GM->RegisterCheckpoint(Index);
	}
}

void AMissionCheckpoint::Tick(float DT)
{
	Super::Tick(DT);
	const ADriveGameMode* GM = GetWorld()->GetAuthGameMode<ADriveGameMode>();
	const bool bActive = GM && !GM->bInMenu && !GM->bMissionOver && GM->NextCheckpoint == Index && Index < GM->TotalCheckpoints;
	if (!bStateKnown || bActive != bWasActive)
	{
		bStateKnown = true;
		bWasActive = bActive;
		SetActorHiddenInGame(!bActive);
		Trigger->SetGenerateOverlapEvents(bActive);
	}
}

void AMissionCheckpoint::OnOverlap(UPrimitiveComponent* OverlappedComp, AActor* Other, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!Cast<ADriveVehicle>(Other)) return;
	if (ADriveGameMode* GM = GetWorld()->GetAuthGameMode<ADriveGameMode>())
	{
		GM->CheckpointReached(Index);
	}
}
