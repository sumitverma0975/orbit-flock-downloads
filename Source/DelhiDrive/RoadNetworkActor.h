#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RoadNetworkActor.generated.h"

class UProceduralMeshComponent;
class UBoxComponent;
class UHierarchicalInstancedStaticMeshComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class ACityBlocks;

/**
 * Builds the whole drivable world from a JSON file (Content/Data/<DataFile>) made by
 * Tools/fetch_osm_roads.py from OpenStreetMap:
 *  roads + lane markings, actual building footprints with a synthetic fallback, potholes, level crossings at real OSM
 *  crossing nodes, AI traffic (cars, autos, buses), demo checkpoints, and puts the player in the left lane.
 * Place ONE of these at world origin (0,0,0) in your level.
 */
UCLASS()
class DELHIDRIVE_API ARoadNetworkActor : public AActor
{
	GENERATED_BODY()

public:
	ARoadNetworkActor();
	virtual void BeginPlay() override;

	/** 0 low, 1 medium, 2 high: building shadows and draw distance. */
	void ApplyQuality(int32 Level);
	FString StreetNameAt(const FVector& WorldPosition) const;

	/** File inside Content/Data. Falls back to sample_grid.json if missing. */
	UPROPERTY(EditAnywhere, Category = "Roads") FString DataFile = TEXT("delhi.json");
	UPROPERTY(EditAnywhere, Category = "Roads") float MaxRadiusCm = 300000.f;   // 3 km. Lower this for phones.
	UPROPERTY(EditAnywhere, Category = "Roads") float RoadZ = 3.f;
	UPROPERTY(EditAnywhere, Category = "Roads") bool bLaneMarkings = true;

	UPROPERTY(EditAnywhere, Category = "Potholes") float PotholesPerKm = 12.f;
	UPROPERTY(EditAnywhere, Category = "Potholes") int32 MaxPotholes = 1500;
	UPROPERTY(EditAnywhere, Category = "Potholes") int32 PotholeSeed = 42;

	UPROPERTY(EditAnywhere, Category = "Scenery") int32 MaxBuildings = 800;
	UPROPERTY(EditAnywhere, Category = "Scenery") int32 MaxTrees = 240;
	UPROPERTY(EditAnywhere, Category = "Scenery") int32 MaxStreetLamps = 120;
	/** Connected centre-line route. All points are WORLD-space centimetres. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Navigation") TArray<FVector> NavigationPath;
	UPROPERTY(EditAnywhere, Category = "Traffic") int32 TrafficCount = 30;

	UPROPERTY(EditAnywhere, Category = "Mission") int32 DemoCheckpoints = 8;
	UPROPERTY(EditAnywhere, Category = "Mission") bool bPlacePlayerOnRoad = true;

private:
	bool Build();
	struct FStreetSegment { FVector2D A, B; float HalfWidth; FName Name; };
	TArray<FStreetSegment> Streets;

	UPROPERTY(VisibleAnywhere) TObjectPtr<UProceduralMeshComponent> Mesh;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UBoxComponent> GroundCollision;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UHierarchicalInstancedStaticMeshComponent> Buildings;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UHierarchicalInstancedStaticMeshComponent> BarrierMarkers;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UHierarchicalInstancedStaticMeshComponent> TreeTrunks;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UHierarchicalInstancedStaticMeshComponent> TreeCanopies;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UHierarchicalInstancedStaticMeshComponent> LampPoles;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UHierarchicalInstancedStaticMeshComponent> LampHeads;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UHierarchicalInstancedStaticMeshComponent> FacadeWindows;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UHierarchicalInstancedStaticMeshComponent> RoofDetails;
	UPROPERTY() TObjectPtr<ACityBlocks> CityBlocks;
	UPROPERTY() TObjectPtr<UMaterialInterface> BaseMaterial;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> RoadMat;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> GroundMat;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> MarkMat;
	FTimerHandle PlaceHandle;
};
