#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MissionCheckpoint.generated.h"

class USphereComponent;
class UStaticMeshComponent;

/** Ordered checkpoint. Only the next one in the sequence is visible/active. */
UCLASS()
class DELHIDRIVE_API AMissionCheckpoint : public AActor
{
	GENERATED_BODY()

public:
	AMissionCheckpoint();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mission", meta = (ExposeOnSpawn = true))
	int32 Index = 0;

private:
	UFUNCTION()
	void OnOverlap(UPrimitiveComponent* OverlappedComp, AActor* Other, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UPROPERTY(VisibleAnywhere) TObjectPtr<USphereComponent> Trigger;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Beam;
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> BayPieces;
	bool bStateKnown = false;
	bool bWasActive = false;
};
