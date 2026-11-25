#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "DriveHUD.generated.h"

class UTexture2D;
class ARoadNetworkActor;

/** Driving controls, instrument cluster and north-up district navigation. */
UCLASS()
class DELHIDRIVE_API ADriveHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;
private:
	UPROPERTY(Transient) TObjectPtr<UTexture2D> DistrictMap;
	int32 CachedCity = INDEX_NONE;
    TWeakObjectPtr<ARoadNetworkActor> DistrictRoads;
    double NextStreetUpdate = 0.;
    FString CurrentStreet;
};
