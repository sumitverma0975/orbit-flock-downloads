#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RailwayCrossing.generated.h"

class UStaticMeshComponent;
class UMaterialInstanceDynamic;

/**
 * Level crossing. Local +X = road direction, +Y = rail direction.
 * Cycle: idle -> gates + lamps close -> train passes -> gates open.
 * Gates and train are solid, so driving through them is a crash.
 */
UCLASS()
class DELHIDRIVE_API ARailwayCrossing : public AActor
{
	GENERATED_BODY()

public:
	ARailwayCrossing();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crossing", meta = (ExposeOnSpawn = true))
	float RoadWidthCm = 800.f;
	UPROPERTY(EditAnywhere, Category = "Crossing") float TrainSpeedCmPerSec = 2500.f; // 90 km/h
	UPROPERTY(EditAnywhere, Category = "Crossing") float MinWaitSec = 20.f;
	UPROPERTY(EditAnywhere, Category = "Crossing") float MaxWaitSec = 45.f;

private:
	static constexpr int32 NumCoaches = 8;
	static constexpr float CoachSpacing = 2200.f;
	static constexpr float TrainStart = 15000.f;

	enum class EPhase : uint8 { Idle, Closing, Passing, Opening };
	EPhase Phase = EPhase::Idle;
	float PhaseTime = 0.f;
	float WaitTime = 10.f;
	float GateAngle = 90.f; // 90 = open (vertical), 0 = closed (across the road)
	float TrainY = 0.f;

	UPROPERTY() TObjectPtr<USceneComponent> Root;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> RailL;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> RailR;
	UPROPERTY() TObjectPtr<USceneComponent> PivotA;
	UPROPERTY() TObjectPtr<USceneComponent> PivotB;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> ArmA;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> ArmB;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> LampA;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> LampB;
	UPROPERTY() TObjectPtr<USceneComponent> TrainRoot;
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Coaches;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> LampMatA;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> LampMatB;
};
