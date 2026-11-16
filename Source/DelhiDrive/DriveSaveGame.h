#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "DriveSaveGame.generated.h"

/** Career progress: total score (unlocks cars), best score per difficulty, last choices. */
UCLASS()
class DELHIDRIVE_API UDriveSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY() int32 TotalScore = 0;
	UPROPERTY() TArray<int32> BestScores;
	UPROPERTY() int32 CarIndex = 0;
	UPROPERTY() int32 Difficulty = 1;
	UPROPERTY() int32 Quality = -1; // -1 = pick automatically for the platform
	UPROPERTY() int32 LevelIndex = 0;
	UPROPERTY() TArray<int32> LevelStars;
};
