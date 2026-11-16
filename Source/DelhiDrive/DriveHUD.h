#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "DriveHUD.generated.h"

/** Canvas HUD: steering wheel, pedals, buttons, speed/RPM/gear, fuel, mission text. No texture assets needed. */
UCLASS()
class DELHIDRIVE_API ADriveHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;
};
