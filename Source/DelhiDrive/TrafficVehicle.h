#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TrafficVehicle.generated.h"

class UStaticMeshComponent;

/**
 * Simple AI traffic: a box-shaped car / auto-rickshaw / bus that drives back and forth along one
 * road polyline in the LEFT lane and brakes for anything ahead (player, gates, other traffic).
 * Replace the box with a real mesh in a Blueprint child when you have art.
 */
UCLASS()
class DELHIDRIVE_API ATrafficVehicle : public AActor
{
	GENERATED_BODY()

public:
	ATrafficVehicle();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	/** Points are WORLD-space X/Y (cm). Kind: 0 car, 1 auto-rickshaw, 2 bus. Call before FinishSpawning. */
	void InitPath(const TArray<FVector2D>& InPts, float InRoadWidth, float InGroundZ, int32 InKind);

	/** 0..1, random per vehicle. Active when Rank < the difficulty's traffic fraction. */
	float Rank = 0.f;
	void SetDifficultyActive(bool bActive);

private:
	float SegLen() const;
	void GetEnds(FVector2D& A, FVector2D& B) const;
	void NextSeg();
	void UpdateTransform(float DT, bool bSnap);

	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Body;

	TArray<FVector2D> Pts;
	float RoadW = 700.f;
	float GroundZ = 0.f;
	int32 Kind = 0;
	int32 SegI = 0;
	int32 Dir = 1;
	float Along = 0.f;
	float Speed = 0.f;
	float Cruise = 1000.f;
	float TraceTimer = 0.f;
	bool bBlockedCached = false;
	FVector Half = FVector(210.f, 90.f, 70.f);
};
