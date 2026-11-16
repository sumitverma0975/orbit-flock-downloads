#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "DrivePlayerController.generated.h"

/** One on-screen touch button (screen pixels). Shared by controller (hit test) and HUD (drawing). */
struct FDriveButton
{
	FName Id;
	FVector2D Center;
	FVector2D Half;
	FString Label;
	bool bHold;
};

/**
 * Touch controls: draggable steering wheel, gas/brake pedals, handbrake, gear buttons, camera/transmission/menu
 * buttons, plus the menu and result screens. Keyboard works too for testing on PC.
 */
UCLASS()
class DELHIDRIVE_API ADrivePlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;
	virtual void PlayerTick(float DeltaTime) override;

	static void BuildLayout(const FVector2D& ViewSize, bool bAuto, TArray<FDriveButton>& Out, FVector2D& WheelCenter, float& WheelRadius);
	static void BuildMenuLayout(const FVector2D& ViewSize, TArray<FDriveButton>& Out);
	static void BuildOverLayout(const FVector2D& ViewSize, TArray<FDriveButton>& Out);
	bool IsHeld(FName Id) const;
	/** Release every touch/steering input when changing screens or restarting. */
	void ResetControls();

	float WheelAngleDeg = 0.f;
	static constexpr float MaxWheelDeg = 270.f;

private:
	void OnTouchDown(ETouchIndex::Type Finger, FVector Location);
	void OnTouchUp(ETouchIndex::Type Finger, FVector Location);
	void OnTouchMove(ETouchIndex::Type Finger, FVector Location);
	void HandleButton(FName Id);
	void HandleMenuButton(FName Id);
	bool HitAny(const TArray<FDriveButton>& Buttons, const FVector2D& P, FName& OutId) const;

	TMap<int32, FName> Held;
	int32 WheelFinger = -1;
	float LastFingerAngle = 0.f;
};
