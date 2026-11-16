#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "CarCatalog.h"
#include "LevelCatalog.h"
#include "DriveGameMode.generated.h"

/**
 * Game flow: menu (car / difficulty / graphics) -> mission (checkpoints, timer, score) -> result.
 * Saves career progress; cars unlock with total score. Applies mobile quality settings.
 */
UCLASS(Config = Game)
class DELHIDRIVE_API ADriveGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ADriveGameMode();

	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual UClass* GetDefaultPawnClassForController_Implementation(AController* InController) override;

	/** Used when /Game/Cars/BP_<Id> does not exist, so one car Blueprint can stand in for all 12. */
	UPROPERTY(Config, EditAnywhere, Category = "DelhiDrive")
	FString FallbackCarPath = TEXT("/Game/Cars/BP_Car_01.BP_Car_01_C");
	UPROPERTY(Config, EditAnywhere, Category = "DelhiDrive")
	float SecondsPerCheckpoint = 75.f;

	// ---- menu actions ----
	void ChangeCar(APlayerController* PC, int32 Delta);
	void ChangeDifficulty(int32 Delta);
	void ChangeQuality(int32 Delta);
	void ChangeLevel(int32 Delta);
	void ChangeCity();
	void StartNextLevel();
	void StartMission();
	void OpenMenu();
	void RestartMission();

	// ---- world callbacks ----
	void RegisterCheckpoint(int32 Index);
	void CheckpointReached(int32 Index);
	void ReportCollision();
	void SetPlayerStart(const FVector& Loc, const FRotator& Rot);
	void TeleportPlayerToStart();
	void ApplyDifficulty();

	// ---- queries ----
	const FDifficultySpec& Diff() const { return DriveCatalog::Difficulty(Difficulty); }
	const FDriveLevelSpec& Level() const { return DriveLevels::Level(LevelIndex); }
	bool IsLevelUnlocked() const { return LevelIndex <= HighestUnlockedLevel; }
	bool CanStartNextLevel() const { return bMissionOver && bLastMissionSuccess && LevelIndex + 1 < DriveLevels::NumLevels(); }
	const TCHAR* CityName() const { return CityIndex == 0 ? TEXT("DELHI") : TEXT("CHANDIGARH"); }
	bool IsCarUnlocked(int32 Index) const { return TotalScore >= DriveCatalog::Car(Index).UnlockScore; }
	FString GetMissionStatus() const;

	int32 CarIndex = 0;
	EDriveDifficulty Difficulty = EDriveDifficulty::Medium;
	int32 Quality = 1;               // 0 low, 1 medium, 2 high
	int32 LevelIndex = 0;
	int32 CityIndex = 0;
	int32 HighestUnlockedLevel = 0;
	TArray<int32> LevelStars;
	int32 EarnedStars = 0;
	bool bLastMissionSuccess = false;
	bool bInMenu = true;
	bool bMissionOver = false;

	int32 TotalScore = 0;
	TArray<int32> BestScores;

	int32 NextCheckpoint = 0;
	int32 TotalCheckpoints = 0;
	int32 Score = 0;
	int32 Crashes = 0;
	float TimeLeft = 0.f;
	float StartingTime = 0.f;
	FString EndMessage;

private:
	UClass* LoadCarClass(int32 Index) const;
	void SwapCar(APlayerController* PC, int32 NewIndex);
	void RecomputeTotals();
	void EndMission(bool bSuccess, const TCHAR* FailureReason = TEXT("TIME UP"));
	void ApplyQuality();
	void LoadProgress();
	void SaveProgress();

	int32 RegisteredCheckpoints = 0;
	bool bHasStart = false;
	FVector StartLoc = FVector::ZeroVector;
	FRotator StartRot = FRotator::ZeroRotator;
};
