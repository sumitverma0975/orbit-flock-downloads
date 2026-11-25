#include "TrafficVehicle.h"
#include "DriveShapes.h"
#include "Components/BoxComponent.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/Pawn.h"
#include "CarCatalog.h"
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

    Bounds = CreateDefaultSubobject<UBoxComponent>(TEXT("Chassis bounds"));
    SetRootComponent(Bounds);
    Bounds->SetCollisionProfileName(TEXT("BlockAllDynamic"));
    auto Part = [this](FName Name, UStaticMesh* Mesh)
    {
        UStaticMeshComponent* C = CreateDefaultSubobject<UStaticMeshComponent>(Name);
        C->SetupAttachment(Bounds);
        C->SetMobility(EComponentMobility::Movable);
        C->SetStaticMesh(Mesh);
        C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        C->SetCullDistance(45000.f);
        C->SetCastShadow(false);
        return C;
    };
    Body = Part(TEXT("Panels"), DriveShapes::Cube());
    Cabin = Part(TEXT("Windows"), DriveShapes::Cube());
    Canopy = Part(TEXT("Canopy"), DriveShapes::Cube());
    for (int32 I=0; I<4; ++I)
        Wheels.Add(Part(FName(*FString::Printf(TEXT("Wheel%d"), I)), DriveShapes::Cylinder()));
}

void ATrafficVehicle::InitPath(const TArray<FVector2D>& InPts, float InRoadWidth, float InGroundZ, int32 InKind, int32 OneWay)
{
	Pts = InPts;
	RoadW = InRoadWidth;
	GroundZ = InGroundZ;
	Kind = InKind;
	SegI = FMath::RandRange(0, FMath::Max(0, Pts.Num() - 2));
	bOneWay = OneWay != 0;
	Dir = bOneWay ? (OneWay < 0 ? -1 : 1) : (FMath::RandBool() ? 1 : -1);
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
	Bounds->SetBoxExtent(Half);
	ConfigureAppearance(Colour);

	Speed = Cruise;
	if (Pts.Num() >= 2)
	{
		Along = FMath::FRand() * SegLen();
		UpdateTransform(0.f, true);
	}
}

void ATrafficVehicle::ConfigureAppearance(const FLinearColor& Paint)
{
    if (Kind == 0)
    {
        // Use a small original touring mesh that is already shared with the garage.
        const FString Path = DriveCatalog::CarStaticMeshPath(8);
        if (UStaticMesh* Car = LoadObject<UStaticMesh>(nullptr, *Path))
        {
            const FBox Box = Car->GetBoundingBox();
            const double Fit = Half.X * 2. / FMath::Max(Box.GetSize().X, 1.);
            Body->SetStaticMesh(Car);
            Body->SetRelativeScale3D(FVector(Fit));
            Body->SetRelativeLocation(FVector(-Box.GetCenter().X*Fit, -Box.GetCenter().Y*Fit, -Half.Z-Box.Min.Z*Fit));
            Cabin->SetVisibility(false);
            Canopy->SetVisibility(false);
            for (auto& Wheel : Wheels) Wheel->SetVisibility(false);
            return;
        }
    }
    auto SetPanel = [](UStaticMeshComponent* Panel,const FVector& Position,const FVector& Size,const FLinearColor& Tint)
    {
        Panel->SetRelativeLocation(Position);
        Panel->SetRelativeScale3D(Size/100.);
        DriveShapes::Tint(Panel,Tint);
    };
    SetPanel(Body,{0,0,-0.25*Half.Z},{Half.X*1.98,Half.Y*1.94,Half.Z*0.90},Paint);
    const double CabinLength = Kind == 2 ? 1.94 : 1.48;
    SetPanel(Cabin,{-Half.X*(2.-CabinLength)/2.,0,Half.Z*0.45},{Half.X*CabinLength,Half.Y*1.79,Half.Z*0.78},
             FLinearColor(0.075f,0.14f,0.16f));
    SetPanel(Canopy,{-Half.X*(2.-CabinLength)/2.,0,Half.Z*0.87},{Half.X*CabinLength,Half.Y*1.91,Half.Z*0.14},
             Kind==1 ? FLinearColor(0.9f,0.63f,0.055f) : Paint);
    const double Radius = Half.Z*(Kind==2 ? 0.31 : 0.38);
    for (int32 I=0; I<Wheels.Num(); ++I)
    {
        const bool Front = I<2;
        const bool TrikeFront = Kind==1 && Front;
        Wheels[I]->SetVisibility(!(TrikeFront && I==1));
        Wheels[I]->SetRelativeLocation({Half.X*(Front?0.67:-0.63),TrikeFront ? 0. : Half.Y*(I%2 ? 0.89 : -0.89),-Half.Z+Radius});
        Wheels[I]->SetRelativeRotation(FRotator(0,0,90));
        Wheels[I]->SetRelativeScale3D({Radius/50.,Radius/50.,Half.Y*0.32/100.});
        DriveShapes::Tint(Wheels[I],FLinearColor(.022f,.023f,.025f));
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

bool ATrafficVehicle::AdvanceSegment()
{
    const int32 Next = SegI + Dir;
    if (Next >= 0 && Next < Pts.Num()-1)
    {
        SegI = Next;
        return true;
    }
    if (bOneWay && FVector2D::Distance(Pts[0],Pts.Last()) > 100.)
    {
        bParked = true;
        Along = SegLen();
        Speed = 0.f;
        return false;
    }
    if (bOneWay) SegI = Dir>0 ? 0 : Pts.Num()-2;
    else Dir = -Dir;
    return true;
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
	SetActorLocationAndRotation(FVector(Q.X, Q.Y, GroundZ + Half.Z), Rot);
}

void ATrafficVehicle::Tick(float DT)
{
	Super::Tick(DT);
	if (Pts.Num() < 2) return;
	const ADriveGameMode* GM = GetWorld()->GetAuthGameMode<ADriveGameMode>();
	if (GM && (GM->bInMenu || GM->bMissionOver)) return;

    if (bParked)
    {
        const APawn* Driver = UGameplayStatics::GetPlayerPawn(this,0);
        const FVector2D Origin = Dir>0 ? Pts[0] : Pts.Last();
        if (!Driver) return;
        const FVector2D Player(Driver->GetActorLocation().X,Driver->GetActorLocation().Y);
        if (FVector2D::DistSquared(Player, Origin) < FMath::Square(45000.) ||
            FVector::DistSquared2D(Driver->GetActorLocation(),GetActorLocation()) < FMath::Square(45000.)) return;
        SegI = Dir>0 ? 0 : Pts.Num()-2;
        Along = 0.f;
        bParked = false;
        UpdateTransform(0.f,true);
    }
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
		if (!AdvanceSegment()) break;
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
