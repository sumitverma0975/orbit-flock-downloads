#include "RoadNetworkActor.h"
#include "PotholeActor.h"
#include "RailwayCrossing.h"
#include "MissionCheckpoint.h"
#include "TrafficVehicle.h"
#include "DriveVehicle.h"
#include "DriveGameMode.h"
#include "DriveShapes.h"
#include "ProceduralMeshComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/Pawn.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"

namespace
{
	struct FCellMesh
	{
		TArray<FVector> V;
		TArray<int32> T;
		TArray<FVector> N;
		TArray<FVector2D> UV;
	};

	struct FSeg
	{
		FVector2D A, B;
		float W;
		float PotholeFactor;
	};

	struct FWay
	{
		TArray<FVector2D> P;
		float W;
	};

	float PotholeFactorFor(const FString& H)
	{
		if (H == TEXT("motorway") || H == TEXT("trunk")) return 0.1f;
		if (H == TEXT("primary"))      return 0.3f;
		if (H == TEXT("secondary"))    return 0.6f;
		if (H == TEXT("tertiary"))     return 1.0f;
		if (H == TEXT("residential"))  return 1.4f;
		if (H == TEXT("service"))      return 1.5f;
		return 1.2f;
	}

	FVector2D ClosestOnSeg(const FVector2D& P, const FVector2D& A, const FVector2D& B)
	{
		const FVector2D AB = B - A;
		const double L2 = AB.SizeSquared();
		const double T = L2 > 0.0 ? FMath::Clamp(FVector2D::DotProduct(P - A, AB) / L2, 0.0, 1.0) : 0.0;
		return A + AB * T;
	}

	// Left of travel direction in Unreal's X-forward / Y-right frame.
	FVector2D LeftOf(const FVector2D& Dir) { return FVector2D(Dir.Y, -Dir.X); }

	// Flat quad strip between A and B, L = half-width vector (left of travel). Clockwise from above = faces up.
	void AddQuad(FCellMesh& C, const FVector2D& A, const FVector2D& B, const FVector2D& L, double Z)
	{
		const int32 b = C.V.Num();
		C.V.Add(FVector(A + L, Z));   // 0 start-left
		C.V.Add(FVector(A - L, Z));   // 1 start-right
		C.V.Add(FVector(B + L, Z));   // 2 end-left
		C.V.Add(FVector(B - L, Z));   // 3 end-right
		for (int32 k = 0; k < 4; ++k)
		{
			C.N.Add(FVector::UpVector);
			C.UV.Add(FVector2D(C.V[b + k].X / 400.0, C.V[b + k].Y / 400.0));
		}
		C.T.Append({ b + 0, b + 2, b + 1,   b + 1, b + 2, b + 3 });
	}
}

ARoadNetworkActor::ARoadNetworkActor()
{
	PrimaryActorTick.bCanEverTick = false;

	Mesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("RoadMesh"));
	RootComponent = Mesh;
	Mesh->bUseAsyncCooking = true;
	Mesh->SetCollisionProfileName(TEXT("BlockAll"));
	Mesh->SetCastShadow(false); // roads never need to cast shadows

	Buildings = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("Buildings"));
	Buildings->SetupAttachment(Mesh);
	Buildings->SetStaticMesh(DriveShapes::Cube());
	Buildings->SetCollisionProfileName(TEXT("BlockAll"));
	BarrierMarkers = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("ObservedBarrierMarkers"));
	BarrierMarkers->SetupAttachment(Mesh);
	BarrierMarkers->SetStaticMesh(DriveShapes::Cylinder());
	BarrierMarkers->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BarrierMarkers->SetCastShadow(false);

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Mat(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	BaseMaterial = Mat.Object;
}

void ARoadNetworkActor::BeginPlay()
{
	Super::BeginPlay();

#if PLATFORM_ANDROID || PLATFORM_IOS
	// Phone budget: smaller world, fewer actors.
	MaxRadiusCm = FMath::Min(MaxRadiusCm, 150000.f);
	MaxPotholes = FMath::Min(MaxPotholes, 180);
	MaxBuildings = FMath::Min(MaxBuildings, 900);
	TrafficCount = FMath::Min(TrafficCount, 16);
#endif

	if (!Build())
	{
		UE_LOG(LogTemp, Error, TEXT("DelhiDrive: road network could not be built (see earlier log lines)."));
	}
}

bool ARoadNetworkActor::Build()
{
	// ---------- load JSON ----------
	FString Path = FPaths::ProjectContentDir() / TEXT("Data") / DataFile;
	FString Text;
	if (!FFileHelper::LoadFileToString(Text, *Path))
	{
		UE_LOG(LogTemp, Warning, TEXT("DelhiDrive: '%s' not found, trying sample_grid.json"), *Path);
		Path = FPaths::ProjectContentDir() / TEXT("Data") / TEXT("sample_grid.json");
		if (!FFileHelper::LoadFileToString(Text, *Path))
		{
			UE_LOG(LogTemp, Error, TEXT("DelhiDrive: cannot read %s"), *Path);
			return false;
		}
	}

	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("DelhiDrive: JSON parse error in %s"), *Path);
		return false;
	}

	const FVector O = GetActorLocation();
	const double R2 = static_cast<double>(MaxRadiusCm) * MaxRadiusCm;
	FRandomStream Rand(PotholeSeed);

	TArray<FSeg> Segs;
	TArray<FWay> Ways;
	// Sections of one procedural component do not have independent culling bounds.
	// Batch by material rather than paying a draw call for every grid cell.
	FCellMesh RoadGeometry;
	FCellMesh MarkGeometry;

	// ---------- parse roads, build road + lane-marking quads ----------
	const TArray<TSharedPtr<FJsonValue>>* Roads = nullptr;
	if (Root->TryGetArrayField(TEXT("roads"), Roads))
	{
		for (const TSharedPtr<FJsonValue>& RV : *Roads)
		{
			const TSharedPtr<FJsonObject> Road = RV->AsObject();
			if (!Road.IsValid()) continue;

			const float W = static_cast<float>(Road->GetNumberField(TEXT("w")) * 100.0);
			const float PF = PotholeFactorFor(Road->GetStringField(TEXT("h")));
			const TArray<TSharedPtr<FJsonValue>>* Pts = nullptr;
			if (!Road->TryGetArrayField(TEXT("p"), Pts) || Pts->Num() < 2) continue;

			FWay Way;
			Way.W = W;
			Way.P.Reserve(Pts->Num());
			for (const TSharedPtr<FJsonValue>& PV : *Pts)
			{
				const TArray<TSharedPtr<FJsonValue>>& XY = PV->AsArray();
				if (XY.Num() >= 2) Way.P.Add(FVector2D(XY[0]->AsNumber(), XY[1]->AsNumber()));
			}
			const TArray<FVector2D>& P = Way.P;

			bool bAny = false;
			for (int32 i = 0; i + 1 < P.Num(); ++i)
			{
				const FVector2D A = P[i], B = P[i + 1];
				const FVector2D Mid = (A + B) * 0.5;
				if (Mid.SizeSquared() > R2) continue;

				const FVector2D D = (B - A);
				const double Len = D.Size();
				if (Len < 1.0) continue;
				const FVector2D Dir = D / Len;
				bAny = true;

				Segs.Add({ A, B, W, PF });

				// One quad per segment, extended a little at both ends to hide gaps at bends and junctions.
				const double Ext = W * 0.35;
				const double Z = RoadZ + W * 0.002;
				AddQuad(RoadGeometry, A - Dir * Ext, B + Dir * Ext, LeftOf(Dir) * (W * 0.5), Z);

				// Dashed centre / lane line on wide roads.
				if (bLaneMarkings && W >= 700.f)
				{
					for (double t = 0.0; t < Len; t += 700.0)
					{
						const FVector2D DA = A + Dir * t;
						const FVector2D DB = A + Dir * FMath::Min(t + 300.0, Len);
						AddQuad(MarkGeometry, DA, DB, LeftOf(Dir) * 8.0, Z + 0.8);
					}
				}
			}
			if (bAny) Ways.Add(MoveTemp(Way));
		}
	}

	if (Segs.Num() == 0)
	{
		UE_LOG(LogTemp, Error, TEXT("DelhiDrive: no road segments inside radius"));
		return false;
	}

	// ---------- materials ----------
	if (BaseMaterial)
	{
		RoadMat = UMaterialInstanceDynamic::Create(BaseMaterial, this);
		RoadMat->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.045f, 0.045f, 0.05f));
		GroundMat = UMaterialInstanceDynamic::Create(BaseMaterial, this);
		GroundMat->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.28f, 0.24f, 0.16f));
		MarkMat = UMaterialInstanceDynamic::Create(BaseMaterial, this);
		MarkMat->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.85f, 0.85f, 0.8f));
	}

	// ---------- ground (section 0), roads (1), markings (2) ----------
	{
		const double G = static_cast<double>(MaxRadiusCm) * 1.3;
		const TArray<FVector> V = { FVector(-G, -G, 0.0), FVector(G, -G, 0.0), FVector(-G, G, 0.0), FVector(G, G, 0.0) };
		const TArray<int32> T = { 0, 1, 2,   2, 1, 3 };
		const TArray<FVector> N = { FVector::UpVector, FVector::UpVector, FVector::UpVector, FVector::UpVector };
		const TArray<FVector2D> UV = { FVector2D(-G / 1000.0, -G / 1000.0), FVector2D(G / 1000.0, -G / 1000.0), FVector2D(-G / 1000.0, G / 1000.0), FVector2D(G / 1000.0, G / 1000.0) };
		Mesh->CreateMeshSection(0, V, T, N, UV, TArray<FColor>(), TArray<FProcMeshTangent>(), true);
		if (GroundMat) Mesh->SetMaterial(0, GroundMat);
	}
	Mesh->CreateMeshSection(1, RoadGeometry.V, RoadGeometry.T, RoadGeometry.N, RoadGeometry.UV,
		TArray<FColor>(), TArray<FProcMeshTangent>(), true);
	if (RoadMat) Mesh->SetMaterial(1, RoadMat);
	if (MarkGeometry.V.Num() > 0)
	{
		Mesh->CreateMeshSection(2, MarkGeometry.V, MarkGeometry.T, MarkGeometry.N, MarkGeometry.UV,
			TArray<FColor>(), TArray<FProcMeshTangent>(), false);
		if (MarkMat) Mesh->SetMaterial(2, MarkMat);
	}
	UE_LOG(LogTemp, Log, TEXT("DelhiDrive: built %d road segments in at most three mesh sections"), Segs.Num());

	// ---------- potholes ----------
	int32 Spawned = 0;
	for (const FSeg& S : Segs)
	{
		if (Spawned >= MaxPotholes) break;
		const FVector2D D = S.B - S.A;
		const double Len = D.Size();
		const FVector2D L = LeftOf(D / Len);

		const float Expected = static_cast<float>(Len / 100.0 / 1000.0) * PotholesPerKm * S.PotholeFactor;
		int32 N = FMath::FloorToInt(Expected);
		if (Rand.FRand() < Expected - N) ++N;

		for (int32 i = 0; i < N && Spawned < MaxPotholes; ++i)
		{
			const FVector2D P = S.A + D * Rand.FRand() + L * (Rand.FRandRange(-0.42f, 0.42f) * S.W);
			const FTransform T(FRotator(0.f, Rand.FRand() * 360.f, 0.f), O + FVector(P.X, P.Y, RoadZ + S.W * 0.002f));
			if (APotholeActor* PH = GetWorld()->SpawnActorDeferred<APotholeActor>(APotholeActor::StaticClass(), T, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
			{
				PH->RadiusCm = Rand.FRandRange(35.f, 90.f);
				PH->Severity = Rand.FRandRange(0.3f, 1.f);
				PH->Rank = Rand.FRand();
				PH->FinishSpawning(T);
				++Spawned;
			}
		}
	}

	// ---------- level crossings (real OSM railway=level_crossing nodes) ----------
	TArray<FVector2D> CrossPts;
	const TArray<TSharedPtr<FJsonValue>>* Crossings = nullptr;
	if (Root->TryGetArrayField(TEXT("crossings"), Crossings))
	{
		for (const TSharedPtr<FJsonValue>& CV : *Crossings)
		{
			const TArray<TSharedPtr<FJsonValue>>& XY = CV->AsArray();
			if (XY.Num() < 2) continue;
			const FVector2D P(XY[0]->AsNumber(), XY[1]->AsNumber());
			if (P.SizeSquared() > R2) continue;

			double Best = TNumericLimits<double>::Max();
			const FSeg* BestSeg = nullptr;
			for (const FSeg& S : Segs)
			{
				const double D2 = FVector2D::DistSquared(P, ClosestOnSeg(P, S.A, S.B));
				if (D2 < Best) { Best = D2; BestSeg = &S; }
			}
			if (!BestSeg) continue;

			CrossPts.Add(P);
			const FVector2D Dir = (BestSeg->B - BestSeg->A).GetSafeNormal();
			const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(Dir.Y, Dir.X));
			const FTransform T(FRotator(0.f, Yaw, 0.f), O + FVector(P.X, P.Y, RoadZ + BestSeg->W * 0.002f + 2.f));
			if (ARailwayCrossing* RC = GetWorld()->SpawnActorDeferred<ARailwayCrossing>(ARailwayCrossing::StaticClass(), T, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
			{
				RC->RoadWidthCm = BestSeg->W + 100.f;
				RC->FinishSpawning(T);
			}
		}
	}

	// ---------- explicitly mapped barrier nodes ----------
	// OSM does not establish current opening/access state or dimensions. These
	// nonblocking markers locate mapped features without inventing a closed road.
	const TArray<TSharedPtr<FJsonValue>>* Features = nullptr;
	int32 MarkerCount = 0;
	if (Root->TryGetArrayField(TEXT("features"), Features))
	{
		for (const TSharedPtr<FJsonValue>& Value : *Features)
		{
			if (MarkerCount >= 150) break;
			const TSharedPtr<FJsonObject> Feature = Value->AsObject();
			FString Kind;
			const TArray<TSharedPtr<FJsonValue>>* Point = nullptr;
			if (!Feature.IsValid() || !Feature->TryGetStringField(TEXT("kind"), Kind) || Kind != TEXT("barrier")
				|| !Feature->TryGetArrayField(TEXT("p"), Point) || Point->Num() < 2) continue;
			const FVector2D Position((*Point)[0]->AsNumber(), (*Point)[1]->AsNumber());
			if (Position.SizeSquared() > R2) continue;
			BarrierMarkers->AddInstance(FTransform(FRotator::ZeroRotator, FVector(Position.X, Position.Y, RoadZ + 55.f), FVector(0.25f, 0.25f, 1.1f)));
			++MarkerCount;
		}
	}
	DriveShapes::Tint(BarrierMarkers, FLinearColor(0.9f, 0.4f, 0.04f));

	// ---------- roadside buildings (boxes) kept clear of every road and crossing ----------
	if (MaxBuildings > 0)
	{
		const double GC = 5000.0;
		auto CellOf = [GC](const FVector2D& P) { return FIntPoint(FMath::FloorToInt(P.X / GC), FMath::FloorToInt(P.Y / GC)); };

		TMap<FIntPoint, TArray<int32>> SegGrid;
		for (int32 si = 0; si < Segs.Num(); ++si)
		{
			const FSeg& S = Segs[si];
			const FIntPoint c0 = CellOf(FVector2D(FMath::Min(S.A.X, S.B.X), FMath::Min(S.A.Y, S.B.Y)));
			const FIntPoint c1 = CellOf(FVector2D(FMath::Max(S.A.X, S.B.X), FMath::Max(S.A.Y, S.B.Y)));
			for (int32 x = c0.X; x <= c1.X; ++x)
			{
				for (int32 y = c0.Y; y <= c1.Y; ++y)
				{
					SegGrid.FindOrAdd(FIntPoint(x, y)).Add(si);
				}
			}
		}

		auto TooCloseToRoad = [&](const FVector2D& P, double Radius)
		{
			const FIntPoint c = CellOf(P);
			for (int32 dx = -1; dx <= 1; ++dx)
			{
				for (int32 dy = -1; dy <= 1; ++dy)
				{
					const TArray<int32>* List = SegGrid.Find(FIntPoint(c.X + dx, c.Y + dy));
					if (!List) continue;
					for (int32 si : *List)
					{
						const FSeg& S = Segs[si];
						if (FVector2D::DistSquared(P, ClosestOnSeg(P, S.A, S.B)) < FMath::Square(S.W * 0.5 + Radius)) return true;
					}
				}
			}
			return false;
		};

		TMap<FIntPoint, TArray<FVector>> BGrid; // x, y, half-size of placed buildings
		int32 Placed = 0;
		for (const FSeg& S : Segs)
		{
			if (Placed >= MaxBuildings) break;
			const FVector2D D = S.B - S.A;
			const double Len = D.Size();
			const FVector2D Dir = D / Len;
			const FVector2D L = LeftOf(Dir);
			const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(Dir.Y, Dir.X));

			int32 N = FMath::FloorToInt(Len / 2200.0);
			if (Rand.FRand() < Len / 2200.0 - N) ++N;

			for (int32 i = 0; i < N && Placed < MaxBuildings; ++i)
			{
				const float Half = Rand.FRandRange(400.f, 900.f);
				const float Height = Rand.FRandRange(600.f, 3200.f);
				const float Side = Rand.FRand() < 0.5f ? -1.f : 1.f;
				const FVector2D P = S.A + D * Rand.FRand() + L * (Side * (S.W * 0.5f + Rand.FRandRange(500.f, 1200.f) + Half));

				if (P.SizeSquared() > R2) continue;
				if (TooCloseToRoad(P, Half * 1.5 + 150.0)) continue;

				bool bBlocked = false;
				for (const FVector2D& C : CrossPts)
				{
					if (FVector2D::DistSquared(P, C) < FMath::Square(17000.0)) { bBlocked = true; break; }
				}
				if (bBlocked) continue;

				const FIntPoint bc = CellOf(P);
				for (int32 dx = -1; dx <= 1 && !bBlocked; ++dx)
				{
					for (int32 dy = -1; dy <= 1 && !bBlocked; ++dy)
					{
						const TArray<FVector>* List = BGrid.Find(FIntPoint(bc.X + dx, bc.Y + dy));
						if (!List) continue;
						for (const FVector& B : *List)
						{
							if (FVector2D::Distance(P, FVector2D(B.X, B.Y)) < (Half + B.Z) * 1.5 + 100.0) { bBlocked = true; break; }
						}
					}
				}
				if (bBlocked) continue;

				BGrid.FindOrAdd(bc).Add(FVector(P.X, P.Y, Half));
				Buildings->AddInstance(FTransform(FRotator(0.f, Yaw, 0.f), FVector(P.X, P.Y, Height * 0.5f),
					FVector(Half * 2.f / 100.f, Half * 2.f / 100.f, Height / 100.f)));
				++Placed;
			}
		}
		DriveShapes::Tint(Buildings, FLinearColor(0.55f, 0.48f, 0.38f));
		UE_LOG(LogTemp, Log, TEXT("DelhiDrive: placed %d buildings"), Placed);
	}

	// ---------- AI traffic: cars, auto-rickshaws, buses ----------
	{
		TArray<int32> Eligible;
		for (int32 i = 0; i < Ways.Num(); ++i)
		{
			const FWay& Wy = Ways[i];
			if (Wy.W < 600.f || Wy.P.Num() < 2) continue;
			if (Wy.P[Wy.P.Num() / 2].SizeSquared() < FMath::Square(6000.0)) continue; // keep clear of the player start
			if (FVector2D::Distance(Wy.P[0], Wy.P.Last()) < 4000.0) continue;
			Eligible.Add(i);
		}
		int32 NumTraffic = 0;
		for (int32 Try = 0; Try < TrafficCount * 4 && NumTraffic < TrafficCount && Eligible.Num() > 0; ++Try)
		{
			const FWay& Wy = Ways[Eligible[Rand.RandRange(0, Eligible.Num() - 1)]];
			TArray<FVector2D> WP;
			WP.Reserve(Wy.P.Num());
			for (const FVector2D& p : Wy.P) WP.Add(p + FVector2D(O.X, O.Y));

			const float r = Rand.FRand();
			const int32 Kind = r < 0.5f ? 0 : (r < 0.85f ? 1 : 2);
			const FTransform T(FRotator::ZeroRotator, FVector(WP[0].X, WP[0].Y, O.Z + RoadZ + 100.f));
			if (ATrafficVehicle* TV = GetWorld()->SpawnActorDeferred<ATrafficVehicle>(ATrafficVehicle::StaticClass(), T, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
			{
				TV->InitPath(WP, Wy.W, O.Z + RoadZ + Wy.W * 0.002f, Kind);
				TV->Rank = Rand.FRand();
				TV->FinishSpawning(T);
				++NumTraffic;
			}
		}
	}

	// ---------- demo mission: chained checkpoints 400 m - 1 km apart ----------
	{
		FVector2D Prev(0.0, 0.0);
		int32 PlacedCheckpoints = 0;
		for (int32 i = 0; i < DemoCheckpoints; ++i)
		{
			for (int32 Try = 0; Try < 800; ++Try)
			{
				const FSeg& S = Segs[Rand.RandRange(0, Segs.Num() - 1)];
				if (S.W < 600.f) continue;
				const FVector2D M = (S.A + S.B) * 0.5;
				const double Dist = FVector2D::Distance(Prev, M);
				if (Dist < 40000.0 || Dist > 100000.0 || M.SizeSquared() > R2 * 0.5) continue;

				const FTransform T(FRotator::ZeroRotator, O + FVector(M.X, M.Y, RoadZ + 5.f));
				if (AMissionCheckpoint* CP = GetWorld()->SpawnActorDeferred<AMissionCheckpoint>(AMissionCheckpoint::StaticClass(), T, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
				{
					CP->Index = PlacedCheckpoints;
					CP->FinishSpawning(T);
					++PlacedCheckpoints;
					Prev = M;
				}
				break;
			}
		}
	}

	// ---------- put the player on the road nearest the origin, in the left lane ----------
	if (bPlacePlayerOnRoad)
	{
		double Best = TNumericLimits<double>::Max();
		FVector2D BestPt(0.0, 0.0), BestDir(1.0, 0.0);
		float BestW = 600.f;
		for (const FSeg& S : Segs)
		{
			if (S.W < 600.f) continue;
			const FVector2D C = ClosestOnSeg(FVector2D::ZeroVector, S.A, S.B);
			const double D2 = C.SizeSquared();
			if (D2 < Best) { Best = D2; BestPt = C; BestDir = (S.B - S.A).GetSafeNormal(); BestW = S.W; }
		}
		const FVector2D Spot = BestPt + LeftOf(BestDir) * (BestW * 0.25f);
		const FVector Loc = O + FVector(Spot.X, Spot.Y, RoadZ + 90.f);
		const FRotator Rot(0.f, FMath::RadiansToDegrees(FMath::Atan2(BestDir.Y, BestDir.X)), 0.f);

		GetWorldTimerManager().SetTimer(PlaceHandle, FTimerDelegate::CreateWeakLambda(this, [this, Loc, Rot]()
		{
			if (ADriveGameMode* GM = GetWorld()->GetAuthGameMode<ADriveGameMode>())
			{
				GM->SetPlayerStart(Loc, Rot);
				GM->TeleportPlayerToStart();
			}
		}), 0.25f, false);
	}

	// Everything is spawned at full density; the difficulty switches parts on/off.
	int32 Quality = 1;
	if (ADriveGameMode* GM = GetWorld()->GetAuthGameMode<ADriveGameMode>())
	{
		GM->ApplyDifficulty();
		Quality = GM->Quality;
	}
	ApplyQuality(Quality);
	return true;
}

void ARoadNetworkActor::ApplyQuality(int32 Level)
{
	Buildings->SetCastShadow(Level >= 2);
	Buildings->SetCullDistances(0, Level <= 0 ? 40000 : (Level == 1 ? 60000 : 90000));
	BarrierMarkers->SetCullDistances(0, Level <= 0 ? 20000 : 40000);
}
