#include "TrafficVehicle.h"
#include "DriveShapes.h"
#include "DriveGameMode.h"
#include "Engine/World.h"
#include "CollisionQueryParams.h"
#include "CollisionShape.h"

ATrafficVehicle::ATrafficVehicle()
{
	PrimaryActorTick.bCanEverTick = true;
#if PLATFORM_ANDROID || PLATFORM_IOS
	PrimaryActorTick.TickInterval = 1.f / 30.f;
#endif

	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	RootComponent = Body;
	Body->SetStaticMesh(DriveShapes::Cube());
	Body->SetMobility(EComponentMobility::Movable);
	Body->SetCollisionProfileName(TEXT("BlockAllDynamic"));
	Body->SetCastShadow(false);
}

void ATrafficVehicle::InitPath(const TArray<FVector2D>& InPts, float InRoadWidth, float InGroundZ, int32 InKind)
{
	Pts = InPts;
	RoadW = InRoadWidth;
	GroundZ = InGroundZ;
	Kind = InKind;
	SegI = FMath::RandRange(0, FMath::Max(0, Pts.Num() - 2));
	Dir = FMath::RandBool() ? 1 : -1;
}

void ATrafficVehicle::BeginPlay()
{
	Super::BeginPlay();

	FLinearColor Colour;
	if (Kind == 1)      // auto-rickshaw: yellow roof, green body
	{
		Half = FVector(135.f, 70.f, 85.f);
		Cruise = FMath::RandRange(600.f, 900.f);
		Colour = FLinearColor(0.10f, 0.45f, 0.12f);
	}
	else if (Kind == 2) // bus
	{
		Half = FVector(525.f, 125.f, 160.f);
		Cruise = FMath::RandRange(700.f, 1000.f);
		Colour = FLinearColor(0.70f, 0.08f, 0.06f);
	}
	else                // car
	{
		Half = FVector(210.f, 90.f, 70.f);
		Cruise = FMath::RandRange(900.f, 1500.f);
		static const FLinearColor Palette[] = {
			FLinearColor(0.85f, 0.85f, 0.85f), FLinearColor(0.05f, 0.05f, 0.06f), FLinearColor(0.55f, 0.55f, 0.58f),
			FLinearColor(0.60f, 0.08f, 0.08f), FLinearColor(0.08f, 0.15f, 0.45f)
		};
		Colour = Palette[FMath::RandRange(0, 4)];
	}
	SetActorScale3D(Half * 2.f / 100.f);
	DriveShapes::Tint(Body, Colour);

	Speed = Cruise;
	if (Pts.Num() >= 2)
	{
		Along = FMath::FRand() * SegLen();
		UpdateTransform(0.f, true);
	}
}

float ATrafficVehicle::SegLen() const
{
	FVector2D A, B;
	GetEnds(A, B);
	return FMath::Max(1.f, static_cast<float>(FVector2D::Distance(A, B)));
}

void ATrafficVehicle::GetEnds(FVector2D& A, FVector2D& B) const
{
	const int32 I = FMath::Clamp(SegI, 0, Pts.Num() - 2);
	A = (Dir > 0) ? Pts[I] : Pts[I + 1];
	B = (Dir > 0) ? Pts[I + 1] : Pts[I];
}

void ATrafficVehicle::NextSeg()
{
	if (Dir > 0)
	{
		if (++SegI >= Pts.Num() - 1) { Dir = -1; SegI = Pts.Num() - 2; }
	}
	else
	{
		if (--SegI < 0) { Dir = 1; SegI = 0; }
	}
}

void ATrafficVehicle::UpdateTransform(float DT, bool bSnap)
{
	FVector2D A, B;
	GetEnds(A, B);
	const float Len = SegLen();
	const FVector2D D = (B - A) / Len;
	const FVector2D L(D.Y, -D.X); // left of travel (India drives on the left)
	const FVector2D Q = A + D * Along + L * (RoadW * 0.25f);

	const FRotator Target(0.f, FMath::RadiansToDegrees(FMath::Atan2(D.Y, D.X)), 0.f);
	const FRotator Rot = bSnap ? Target : FMath::RInterpTo(GetActorRotation(), Target, DT, 6.f);
	SetActorLocationAndRotation(FVector(Q.X, Q.Y, GroundZ + Half.Z + 15.f), Rot);
}

void ATrafficVehicle::Tick(float DT)
{
	Super::Tick(DT);
	if (Pts.Num() < 2) return;
	const ADriveGameMode* GM = GetWorld()->GetAuthGameMode<ADriveGameMode>();
	if (GM && (GM->bInMenu || GM->bMissionOver)) return;

	// Look ahead for the player, gates, trains or other traffic (10 times a second, not every frame).
	TraceTimer -= DT;
	if (TraceTimer <= 0.f)
	{
		TraceTimer = 0.1f + FMath::FRand() * 0.05f;
		const FVector Fwd = GetActorForwardVector();
		const FVector Start = GetActorLocation();
		FCollisionQueryParams QP(SCENE_QUERY_STAT(TrafficBrake), false, this);
		FHitResult Hit;
		const FCollisionShape Probe = FCollisionShape::MakeBox(FVector(Half.X, Half.Y * 0.9f, Half.Z * 0.9f));
		bBlockedCached = GetWorld()->SweepSingleByChannel(Hit, Start, Start + Fwd * (800.f + Speed * 0.8f),
			GetActorQuat(), ECC_Visibility, Probe, QP);
	}
	const bool bBlocked = bBlockedCached;

	const bool bStop = bBlocked; // never drive through a closed crossing to break a queue
	Speed = FMath::FInterpConstantTo(Speed, bStop ? 0.f : Cruise, DT, bStop ? 1500.f : 350.f);

	Along += Speed * DT;
	float Len = SegLen();
	int32 Guard = 0;
	while (Along >= Len && Guard++ < 8)
	{
		Along -= Len;
		NextSeg();
		Len = SegLen();
	}
	UpdateTransform(DT, false);
}

void ATrafficVehicle::SetDifficultyActive(bool bActive)
{
	SetActorHiddenInGame(!bActive);
	SetActorEnableCollision(bActive);
	SetActorTickEnabled(bActive);
}
