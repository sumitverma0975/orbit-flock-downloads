#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TrafficVehicle.generated.h"

class UStaticMeshComponent;
class UBoxComponent;

/** Traffic follows OSM way direction and keeps left on two-way roads. */
UCLASS()
class DELHIDRIVE_API ATrafficVehicle : public AActor
{
	GENERATED_BODY()

public:
	ATrafficVehicle();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	/** Points are WORLD-space X/Y (cm). Kind: 0 car, 1 auto-rickshaw, 2 bus. Call before FinishSpawning. */
	void InitPath(const TArray<FVector2D>& InPts, float InRoadWidth, float InGroundZ, int32 InKind, int32 OneWay = 0);

	/** 0..1, random per vehicle. Active when Rank < the difficulty's traffic fraction. */
	float Rank = 0.f;
	void SetDifficultyActive(bool bActive);

private:
	float SegLen() const;
	void GetEnds(FVector2D& A, FVector2D& B) const;
	bool AdvanceSegment();
	void ConfigureAppearance(const FLinearColor& Paint);
	void UpdateTransform(float DT, bool bSnap);

	UPROPERTY(VisibleAnywhere) TObjectPtr<UBoxComponent> Bounds;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Body;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Cabin;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Canopy;
    UPROPERTY(VisibleAnywhere) TArray<TObjectPtr<UStaticMeshComponent>> Wheels;

	TArray<FVector2D> Pts;
	float RoadW = 700.f;
	float GroundZ = 0.f;
	int32 Kind = 0;
	int32 SegI = 0;
	int32 Dir = 1;
	bool bOneWay = false;
	bool bParked = false;
	float Along = 0.f;
	float Speed = 0.f;
	float Cruise = 1000.f;
	float TraceTimer = 0.f;
	bool bBlockedCached = false;
	FVector Half = FVector(210.f, 90.f, 70.f);
};
