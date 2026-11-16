#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PotholeActor.generated.h"

class USphereComponent;
class UStaticMeshComponent;

/** A pothole: dark patch + trigger. Hitting it at speed drops the car, then kicks it up and slows it. */
UCLASS()
class DELHIDRIVE_API APotholeActor : public AActor
{
	GENERATED_BODY()

public:
	APotholeActor();
	virtual void BeginPlay() override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pothole", meta = (ExposeOnSpawn = true))
	float RadiusCm = 50.f;
	/** 0..1 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pothole", meta = (ExposeOnSpawn = true))
	float Severity = 0.6f;

	/** Velocity change (cm/s) at full severity and ~43 km/h. Tune to taste. */
	UPROPERTY(EditAnywhere, Category = "Pothole") float DropVelocity = 120.f;
	UPROPERTY(EditAnywhere, Category = "Pothole") float KickVelocity = 170.f;
	UPROPERTY(EditAnywhere, Category = "Pothole") float SlowVelocity = 70.f;

	/** 0..1, random per pothole. Active when Rank < the difficulty's pothole fraction. */
	float Rank = 0.f;
	void SetDifficultyActive(bool bActive);

private:
	UFUNCTION()
	void OnOverlap(UPrimitiveComponent* OverlappedComp, AActor* Other, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UPROPERTY(VisibleAnywhere) TObjectPtr<USphereComponent> Trigger;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Visual;
	double LastHitTime = -100.0;
};
