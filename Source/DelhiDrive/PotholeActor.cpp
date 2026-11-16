#include "PotholeActor.h"
#include "DriveVehicle.h"
#include "DriveGameMode.h"
#include "DriveShapes.h"
#include "Components/SphereComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"

APotholeActor::APotholeActor()
{
	PrimaryActorTick.bCanEverTick = false;

	Trigger = CreateDefaultSubobject<USphereComponent>(TEXT("Trigger"));
	RootComponent = Trigger;
	Trigger->SetSphereRadius(50.f);
	Trigger->SetCollisionProfileName(TEXT("OverlapAllDynamic"));

	Visual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Visual"));
	Visual->SetupAttachment(Trigger);
	Visual->SetStaticMesh(DriveShapes::Cylinder());
	Visual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Visual->SetCastShadow(false);
}

void APotholeActor::BeginPlay()
{
	Super::BeginPlay();

	Trigger->SetSphereRadius(RadiusCm);
	const float D = RadiusCm * 2.f / 100.f;
	Visual->SetRelativeScale3D(FVector(D, D, 0.02f));
	Visual->SetRelativeLocation(FVector(0.f, 0.f, 1.5f));
	DriveShapes::Tint(Visual, FLinearColor(0.015f, 0.015f, 0.015f));

	Trigger->OnComponentBeginOverlap.AddDynamic(this, &APotholeActor::OnOverlap);
}

void APotholeActor::OnOverlap(UPrimitiveComponent* OverlappedComp, AActor* Other, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	ADriveVehicle* V = Cast<ADriveVehicle>(Other);
	if (!V || OtherComp != V->GetMesh()) return;
	const ADriveGameMode* GM = GetWorld()->GetAuthGameMode<ADriveGameMode>();
	if (GM && (GM->bInMenu || GM->bMissionOver)) return;

	const double Now = GetWorld()->GetTimeSeconds();
	if (Now - LastHitTime < 0.4) return;
	LastHitTime = Now;

	USkeletalMeshComponent* Body = V->GetMesh();
	const float M = Body->GetMass();
	const float SpeedMs = V->GetVelocity().Size() / 100.f;
	const float SevMult = GM ? GM->Diff().PotholeSeverityMult : 1.f;
	const float S = FMath::Clamp(SpeedMs / 12.f, 0.15f, 1.6f) * Severity * SevMult;
	const FVector Loc = GetActorLocation();

	// 1) wheel drops into the hole
	Body->AddImpulseAtLocation(FVector(0.f, 0.f, -1.f) * M * DropVelocity * S, Loc);

	// 2) a moment later the far edge kicks the car up, slows it and nudges it sideways
	const float Kick = KickVelocity, Slow = SlowVelocity;
	FTimerHandle Handle;
	GetWorldTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(V, [V, Loc, M, S, Kick, Slow]()
	{
		USkeletalMeshComponent* B = V->GetMesh();
		const FVector Impulse =
			FVector(0.f, 0.f, 1.f) * M * Kick * S
			- V->GetActorForwardVector() * M * Slow * S
			+ V->GetActorRightVector() * M * FMath::RandRange(-40.f, 40.f) * S;
		B->AddImpulseAtLocation(Impulse, Loc);
	}), 0.07f, false);
}

void APotholeActor::SetDifficultyActive(bool bActive)
{
	SetActorHiddenInGame(!bActive);
	SetActorEnableCollision(bActive);
}
