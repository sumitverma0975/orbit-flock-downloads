#include "RailwayCrossing.h"
#include "DriveShapes.h"
#include "DriveGameMode.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"

ARailwayCrossing::ARailwayCrossing()
{
	PrimaryActorTick.bCanEverTick = true;
#if PLATFORM_ANDROID || PLATFORM_IOS
	PrimaryActorTick.TickInterval = 1.f / 30.f;
#endif

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	auto MakeBox = [this](const TCHAR* Name, USceneComponent* Parent, const FVector& Loc, const FVector& Scale) -> UStaticMeshComponent*
	{
		UStaticMeshComponent* C = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		C->SetupAttachment(Parent);
		C->SetStaticMesh(DriveShapes::Cube());
		C->SetRelativeLocation(Loc);
		C->SetRelativeScale3D(Scale);
		C->SetCollisionProfileName(TEXT("BlockAll"));
		C->SetCastShadow(false);
		return C;
	};

	// Broad-gauge rails (about 1.68 m apart), 150 m each side, 6 cm high (a real bump).
	RailL = MakeBox(TEXT("RailL"), Root, FVector(-84.f, 0.f, 3.f), FVector(0.08f, 300.f, 0.06f));
	RailR = MakeBox(TEXT("RailR"), Root, FVector( 84.f, 0.f, 3.f), FVector(0.08f, 300.f, 0.06f));

	// Two full-width gates, one on each approach side. Pivot sits at the -Y road edge.
	PivotA = CreateDefaultSubobject<USceneComponent>(TEXT("PivotA"));
	PivotA->SetupAttachment(Root);
	PivotB = CreateDefaultSubobject<USceneComponent>(TEXT("PivotB"));
	PivotB->SetupAttachment(Root);
	ArmA = MakeBox(TEXT("ArmA"), PivotA, FVector::ZeroVector, FVector(0.15f, 8.f, 0.15f));
	ArmB = MakeBox(TEXT("ArmB"), PivotB, FVector::ZeroVector, FVector(0.15f, 8.f, 0.15f));

	LampA = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LampA"));
	LampA->SetupAttachment(Root);
	LampA->SetStaticMesh(DriveShapes::Sphere());
	LampA->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	LampB = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LampB"));
	LampB->SetupAttachment(Root);
	LampB->SetStaticMesh(DriveShapes::Sphere());
	LampB->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// Train: engine + coaches, front at TrainRoot, extending toward -Y, moving +Y.
	TrainRoot = CreateDefaultSubobject<USceneComponent>(TEXT("TrainRoot"));
	TrainRoot->SetupAttachment(Root);
	for (int32 i = 0; i < NumCoaches; ++i)
	{
		const FName N(*FString::Printf(TEXT("Coach%d"), i));
		UStaticMeshComponent* C = MakeBox(*N.ToString(), TrainRoot, FVector(0.f, -(i + 0.5f) * CoachSpacing, 210.f), FVector(3.2f, 21.f, 4.f));
		Coaches.Add(C);
	}
}

void ARailwayCrossing::BeginPlay()
{
	Super::BeginPlay();

	const float Half = RoadWidthCm * 0.5f + 30.f;
	const float Len = Half * 2.f;

	PivotA->SetRelativeLocation(FVector(-450.f, -Half, 95.f));
	PivotB->SetRelativeLocation(FVector( 450.f, -Half, 95.f));
	ArmA->SetRelativeLocation(FVector(0.f, Len * 0.5f, 0.f));
	ArmB->SetRelativeLocation(FVector(0.f, Len * 0.5f, 0.f));
	ArmA->SetRelativeScale3D(FVector(0.15f, Len / 100.f, 0.15f));
	ArmB->SetRelativeScale3D(FVector(0.15f, Len / 100.f, 0.15f));
	DriveShapes::Tint(ArmA, FLinearColor(0.85f, 0.05f, 0.05f));
	DriveShapes::Tint(ArmB, FLinearColor(0.85f, 0.05f, 0.05f));

	LampA->SetRelativeLocation(FVector(-450.f, -Half - 40.f, 220.f));
	LampB->SetRelativeLocation(FVector( 450.f, -Half - 40.f, 220.f));
	LampA->SetRelativeScale3D(FVector(0.4f));
	LampB->SetRelativeScale3D(FVector(0.4f));
	LampMatA = LampA->CreateAndSetMaterialInstanceDynamic(0);
	LampMatB = LampB->CreateAndSetMaterialInstanceDynamic(0);

	DriveShapes::Tint(RailL, FLinearColor(0.12f, 0.12f, 0.13f));
	DriveShapes::Tint(RailR, FLinearColor(0.12f, 0.12f, 0.13f));
	for (int32 i = 0; i < Coaches.Num(); ++i)
	{
		DriveShapes::Tint(Coaches[i], i == 0 ? FLinearColor(0.02f, 0.05f, 0.30f) : FLinearColor(0.04f, 0.10f, 0.42f));
	}

	WaitTime = FMath::RandRange(6.f, 14.f);
	TrainRoot->SetRelativeLocation(FVector(0.f, -1000000.f, 0.f)); // parked far away
}

void ARailwayCrossing::Tick(float DT)
{
	Super::Tick(DT);
	const ADriveGameMode* GM = GetWorld()->GetAuthGameMode<ADriveGameMode>();
	if (GM && (GM->bInMenu || GM->bMissionOver)) return;
	PhaseTime += DT;
	const float TrainLength = NumCoaches * CoachSpacing;
	const float WaitMult = GM ? GM->Diff().TrainWaitMult : 1.f;

	switch (Phase)
	{
	case EPhase::Idle:
		if (PhaseTime >= WaitTime) { Phase = EPhase::Closing; PhaseTime = 0.f; }
		break;
	case EPhase::Closing:
		GateAngle = FMath::Max(0.f, 90.f - (PhaseTime / 3.f) * 90.f);
		if (PhaseTime >= 4.f) { Phase = EPhase::Passing; PhaseTime = 0.f; TrainY = -TrainStart; }
		break;
	case EPhase::Passing:
		TrainY += TrainSpeedCmPerSec * DT;
		if (TrainY - TrainLength > 2500.f) { Phase = EPhase::Opening; PhaseTime = 0.f; }
		break;
	case EPhase::Opening:
		GateAngle = FMath::Min(90.f, (PhaseTime / 3.f) * 90.f);
		if (PhaseTime >= 3.f)
		{
			Phase = EPhase::Idle;
			PhaseTime = 0.f;
			WaitTime = FMath::RandRange(MinWaitSec, MaxWaitSec) * WaitMult;
		}
		break;
	}

	// Positive roll swings +Y toward -Z, so "up" is a negative roll.
	const FRotator R(0.f, 0.f, -GateAngle);
	PivotA->SetRelativeRotation(R);
	PivotB->SetRelativeRotation(R);

	TrainRoot->SetRelativeLocation(FVector(0.f, Phase == EPhase::Passing ? TrainY : -1000000.f, 0.f));

	// Alternating red lamps whenever the crossing is active.
	const bool bActive = Phase != EPhase::Idle;
	const bool bFlip = FMath::Fmod(GetGameTimeSinceCreation() * 2.f, 2.f) < 1.f;
	const FLinearColor On(1.f, 0.f, 0.f), Off(0.12f, 0.02f, 0.02f);
	if (LampMatA) LampMatA->SetVectorParameterValue(TEXT("Color"), (bActive && bFlip) ? On : Off);
	if (LampMatB) LampMatB->SetVectorParameterValue(TEXT("Color"), (bActive && !bFlip) ? On : Off);
}
