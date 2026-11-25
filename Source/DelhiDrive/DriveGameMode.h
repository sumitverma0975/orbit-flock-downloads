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
	void ChangeSession();
	void RecoverVehicle();
	void StartNextLevel();
	void StartMission();
	void OpenMenu();
	void RestartMission();

	// ---- world callbacks ----
	void RegisterCheckpoint(int32 Index, const FVector& Loc = FVector::ZeroVector, const FRotator& Rot = FRotator::ZeroRotator);
	void EvaluateCheckpoint(int32 Index, float DeltaSeconds);
	void SetRoutePath(const TArray<FVector>& Points) { NavigationRoute = Points; }
	void CheckpointReached(int32 Index);
	void ReportCollision();
	void SetPlayerStart(const FVector& Loc, const FRotator& Rot);
	void TeleportPlayerToStart();
	void ApplyDifficulty();

	// ---- queries ----
	const FDifficultySpec& Diff() const { return DriveCatalog::Difficulty(Difficulty); }
	const FDriveLevelSpec& Level() const { return IsSchool() ? DriveLevels::Lesson(SchoolIndex) : DriveLevels::Level(LevelIndex); }
	bool IsSchool() const { return Session == EDriveSession::School; }
	bool IsTour() const { return Session == EDriveSession::Tour; }
	int32 SelectedChallenge() const { return IsSchool() ? SchoolIndex : LevelIndex; }
	int32 ChallengeCount() const { return IsSchool() ? DriveLevels::NumLessons() : DriveLevels::NumLevels(); }
	const TCHAR* SessionName() const { return IsTour() ? TEXT("OPEN WORLD TOUR") : (IsSchool() ? TEXT("DRIVING SCHOOL") : TEXT("CAREER")); }
	int32 ChallengeStars() const;
	bool IsLevelUnlocked() const { return IsTour() || (IsSchool() ? SchoolIndex <= HighestUnlockedLesson : LevelIndex <= HighestUnlockedLevel); }
	bool CanStartNextLevel() const { return !IsTour() && bMissionOver && bLastMissionSuccess && SelectedChallenge() + 1 < ChallengeCount(); }
	const TCHAR* CityName() const { return CityIndex == 0 ? TEXT("DELHI") : TEXT("CHANDIGARH"); }
	bool IsCarUnlocked(int32 Index) const { return TotalScore >= DriveCatalog::Car(Index).UnlockScore; }
	FString GetMissionStatus() const;
	FString GetObjectiveText() const;
	FString GetNavigationStatus() const;
	bool GetNavigationTarget(FVector& OutPosition, FRotator& OutRotation) const;
	bool GetRouteGuidanceTarget(FVector& OutPosition) const;
	bool IsCheckpointActive(int32 Index) const;

	int32 CarIndex = 0;
	EDriveDifficulty Difficulty = EDriveDifficulty::Medium;
	int32 Quality = 1;               // 0 low, 1 medium, 2 high
	int32 LevelIndex = 0;
	EDriveSession Session = EDriveSession::School;
	int32 SchoolIndex = 0;
	int32 HighestUnlockedLesson = 0;
	TArray<int32> SchoolStars;
	TArray<FVector> NavigationRoute;
	FString CoachMessage;
	FString ResultFeedback;
	float SpeedingSeconds = 0.f;
	int32 HarshEvents = 0;
	float DistanceMeters = 0.f;
	float StopHold = 0.f;
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
	TArray<FVector> CheckpointLocations;
	TArray<FRotator> CheckpointRotations;
	FVector PreviousLocation = FVector::ZeroVector;
	float PreviousYaw = 0.f;
	float CoachTime = 0.f;
	float HarshCooldown = 0.f;
	float ElapsedTime = 0.f;
	void Coach(const FString& Text, float Seconds = 3.f);
	bool bHasStart = false;
	FVector StartLoc = FVector::ZeroVector;
	FRotator StartRot = FRotator::ZeroRotator;
};
