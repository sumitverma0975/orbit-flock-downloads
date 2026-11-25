#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CityBlocks.generated.h"

/** Extrudes the mapped district's building outlines using baked roof triangles. */
UCLASS()
class DELHIDRIVE_API ACityBlocks : public AActor
{
    GENERATED_BODY()
public:
    ACityBlocks();
    bool InitCity(const FString& RoadFile);
};
