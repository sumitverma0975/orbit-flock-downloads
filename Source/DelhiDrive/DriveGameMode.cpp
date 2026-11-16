#include "DriveGameMode.h"
#include "DrivePlayerController.h"
#include "DriveHUD.h"
#include "DriveVehicle.h"
#include "PotholeActor.h"
#include "TrafficVehicle.h"
#include "RoadNetworkActor.h"
#include "DriveSaveGame.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "HAL/IConsoleManager.h"

static const TCHAR* const GSaveSlot = TEXT("DelhiDrive");

static void ResetPlayerControls(UObject* WorldContext)
{
	if (ADrivePlayerController* PC = Cast<ADrivePlayerController>(UGameplayStatics::GetPlayerController(WorldContext, 0)))
	{
		PC->ResetControls();
	}
}

ADriveGameMode::ADriveGameMode()
{
	PlayerControllerClass = ADrivePlayerController::StaticClass();
	HUDClass = ADriveHUD::StaticClass();
	PrimaryActorTick.bCanEverTick = true;
}

void ADriveGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);
	LoadProgress(); // before the player spawns, so the saved car is the one that appears
	CityIndex = MapName.Contains(TEXT("Chandigarh")) ? 1 : 0;
}

void ADriveGameMode::BeginPlay()
{
	Super::BeginPlay();
	ApplyQuality();
	ApplyDifficulty();
}

UClass* ADriveGameMode::LoadCarClass(int32 Index) const
{
	if (UClass* C = StaticLoadClass(ADriveVehicle::StaticClass(), nullptr, *DriveCatalog::CarBlueprintPath(Index), nullptr, LOAD_NoWarn))
	{
		return C;
	}
	return StaticLoadClass(ADriveVehicle::StaticClass(), nullptr, *FallbackCarPath, nullptr, LOAD_NoWarn);
}

UClass* ADriveGameMode::GetDefaultPawnClassForController_Implementation(AController* InController)
{
	if (UClass* C = LoadCarClass(CarIndex))
	{
		return C;
	}
	UE_LOG(LogTemp, Warning, TEXT("DelhiDrive: no car Blueprint found (%s or %s)"), *DriveCatalog::CarBlueprintPath(CarIndex), *FallbackCarPath);
	return Super::GetDefaultPawnClassForController_Implementation(InController);
}

// ------------------------------------------------------------------ progress / quality

void ADriveGameMode::LoadProgress()
{
	if (UDriveSaveGame* S = Cast<UDriveSaveGame>(UGameplayStatics::LoadGameFromSlot(GSaveSlot, 0)))
	{
		TotalScore = FMath::Max(0, S->TotalScore);
		BestScores = S->BestScores;
		CarIndex = FMath::Clamp(S->CarIndex, 0, DriveCatalog::NumCars() - 1);
		Difficulty = static_cast<EDriveDifficulty>(FMath::Clamp(S->Difficulty, 0, 3));
		Quality = S->Quality;
		LevelIndex = FMath::Clamp(S->LevelIndex, 0, DriveLevels::NumLevels() - 1);
		LevelStars = S->LevelStars;
	}
	else
	{
		Quality = -1;
	}

	BestScores.SetNumZeroed(4);
	for (int32& Best : BestScores) Best = FMath::Max(0, Best);
	LevelStars.SetNumZeroed(DriveLevels::NumLevels());
	for (int32& Stars : LevelStars) Stars = FMath::Clamp(Stars, 0, 3);
	HighestUnlockedLevel = 0;
	while (HighestUnlockedLevel + 1 < DriveLevels::NumLevels() && LevelStars[HighestUnlockedLevel] > 0)
	{
		++HighestUnlockedLevel;
	}
	LevelIndex = FMath::Min(LevelIndex, HighestUnlockedLevel);
	if (!IsCarUnlocked(CarIndex)) CarIndex = 0;
	if (Quality < 0)
	{
#if PLATFORM_ANDROID || PLATFORM_IOS
		Quality = 1;
#else
		Quality = 2;
#endif
	}
	Quality = FMath::Clamp(Quality, 0, 2);
}

void ADriveGameMode::SaveProgress()
{
	UDriveSaveGame* S = Cast<UDriveSaveGame>(UGameplayStatics::CreateSaveGameObject(UDriveSaveGame::StaticClass()));
	if (!S) return;
	S->TotalScore = TotalScore;
	S->BestScores = BestScores;
	S->CarIndex = CarIndex;
	S->Difficulty = static_cast<int32>(Difficulty);
	S->Quality = Quality;
	S->LevelIndex = LevelIndex;
	S->LevelStars = LevelStars;
	UGameplayStatics::SaveGameToSlot(S, GSaveSlot, 0);
}

void ADriveGameMode::ApplyQuality()
{
	const int32 L = FMath::Clamp(Quality, 0, 2);
	const int32 Group = (L == 0) ? 0 : (L == 1 ? 1 : 3);
	const float Res = (L == 0) ? 60.f : (L == 1 ? 80.f : 100.f);

	auto SetCVar = [](const TCHAR* Name, float Value)
	{
		if (IConsoleVariable* CV = IConsoleManager::Get().FindConsoleVariable(Name))
		{
			CV->Set(Value, ECVF_SetByCode);
		}
	};
	static const TCHAR* const Groups[] = {
		TEXT("sg.ViewDistanceQuality"), TEXT("sg.ShadowQuality"), TEXT("sg.PostProcessQuality"),
		TEXT("sg.TextureQuality"), TEXT("sg.EffectsQuality"), TEXT("sg.FoliageQuality")
	};
	for (const TCHAR* G : Groups) SetCVar(G, static_cast<float>(Group));
	SetCVar(TEXT("r.ScreenPercentage"), Res);
	SetCVar(TEXT("t.MaxFPS"), L == 0 ? 30.f : 60.f);
#if PLATFORM_ANDROID || PLATFORM_IOS
	// Mobile render scale; r.ScreenPercentage is not honored by every mobile path.
	SetCVar(TEXT("r.MobileContentScaleFactor"), L == 0 ? 0.75f : (L == 1 ? 1.f : 1.25f));
#endif

	for (TActorIterator<ARoadNetworkActor> It(GetWorld()); It; ++It)
	{
		It->ApplyQuality(L);
	}
}

// ------------------------------------------------------------------ menu actions

void ADriveGameMode::ChangeCar(APlayerController* PC, int32 Delta)
{
	const int32 N = DriveCatalog::NumCars();
	SwapCar(PC, ((CarIndex + Delta) % N + N) % N);
}

void ADriveGameMode::SwapCar(APlayerController* PC, int32 NewIndex)
{
	if (!PC) return;
	UClass* C = LoadCarClass(NewIndex);
	if (!C) return;

	APawn* Old = PC->GetPawn();
	const FRotator Rot = bHasStart ? StartRot : (Old ? FRotator(0.f, Old->GetActorRotation().Yaw, 0.f) : FRotator::ZeroRotator);
	const FVector Loc = bHasStart ? StartLoc : (Old ? Old->GetActorLocation() : FVector(0.f, 0.f, 90.f));

	const int32 Prev = CarIndex;
	CarIndex = NewIndex; // set first: the new pawn reads it in BeginPlay

	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	APawn* NewPawn = GetWorld()->SpawnActor<APawn>(C, Loc, Rot, P);
	if (!NewPawn)
	{
		CarIndex = Prev;
		return;
	}
	if (Old)
	{
		PC->UnPossess();
		Old->Destroy();
	}
	PC->Possess(NewPawn);
	SaveProgress();
}

void ADriveGameMode::ChangeDifficulty(int32 Delta)
{
	Difficulty = static_cast<EDriveDifficulty>(((static_cast<int32>(Difficulty) + Delta) % 4 + 4) % 4);
	ApplyDifficulty();
	SaveProgress();
}

void ADriveGameMode::ChangeQuality(int32 Delta)
{
	Quality = ((Quality + Delta) % 3 + 3) % 3;
	ApplyQuality();
	SaveProgress();
}

void ADriveGameMode::ChangeLevel(int32 Delta)
{
	const int32 N = DriveLevels::NumLevels();
	LevelIndex = ((LevelIndex + Delta) % N + N) % N;
	ApplyDifficulty();
	SaveProgress();
}

void ADriveGameMode::ChangeCity()
{
	if (!bInMenu) return;
	SaveProgress();
	const FName Map(CityIndex == 0 ? TEXT("/Game/Maps/Lvl_Chandigarh") : TEXT("/Game/Maps/Lvl_Delhi"));
	UGameplayStatics::OpenLevel(this, Map);
}

void ADriveGameMode::StartNextLevel()
{
	if (!CanStartNextLevel()) return;
	++LevelIndex;
	StartMission();
}

void ADriveGameMode::StartMission()
{
	if (!IsCarUnlocked(CarIndex) || !IsLevelUnlocked()) return;
	if (!Cast<ADriveVehicle>(UGameplayStatics::GetPlayerPawn(this, 0))) return;
	bInMenu = false;
	SaveProgress();
	RestartMission();
}

void ADriveGameMode::OpenMenu()
{
	bInMenu = true;
	bMissionOver = false;
	ResetPlayerControls(this);
}

void ADriveGameMode::RestartMission()
{
	NextCheckpoint = 0;
	Score = 0;
	Crashes = 0;
	bMissionOver = false;
	bLastMissionSuccess = false;
	EarnedStars = 0;
	EndMessage.Reset();
	ApplyDifficulty();
	TeleportPlayerToStart();
	ResetPlayerControls(this);
}

// ------------------------------------------------------------------ difficulty

void ADriveGameMode::RecomputeTotals()
{
	const int32 ExtraCheckpoints = FMath::Max(0, (Diff().Checkpoints - 3) / 2);
	TotalCheckpoints = FMath::Min(RegisteredCheckpoints, FMath::Min(8, Level().Checkpoints + ExtraCheckpoints));
}

void ADriveGameMode::ApplyDifficulty()
{
	const FDifficultySpec& D = Diff();
	RecomputeTotals();
	StartingTime = SecondsPerCheckpoint * FMath::Max(1, TotalCheckpoints) * D.TimeMult * Level().TimeMult;
	TimeLeft = StartingTime;

	// The world is built once at maximum density; difficulty just switches things on and off.
	for (TActorIterator<APotholeActor> It(GetWorld()); It; ++It)
	{
		It->SetDifficultyActive(It->Rank < FMath::Clamp(D.PotholeFraction * Level().PotholeMult, 0.f, 1.f));
	}
	for (TActorIterator<ATrafficVehicle> It(GetWorld()); It; ++It)
	{
		It->SetDifficultyActive(It->Rank < FMath::Clamp(D.TrafficFraction * Level().TrafficMult, 0.f, 1.f));
	}
}

// ------------------------------------------------------------------ mission

void ADriveGameMode::Tick(float DT)
{
	Super::Tick(DT);
	if (bInMenu || bMissionOver || TotalCheckpoints <= 0) return;

	TimeLeft -= DT;
	if (TimeLeft <= 0.f)
	{
		TimeLeft = 0.f;
		EndMission(false);
	}
}

void ADriveGameMode::RegisterCheckpoint(int32 Index)
{
	RegisteredCheckpoints = FMath::Max(RegisteredCheckpoints, Index + 1);
	RecomputeTotals();
	StartingTime = SecondsPerCheckpoint * FMath::Max(1, TotalCheckpoints) * Diff().TimeMult * Level().TimeMult;
	TimeLeft = StartingTime;
}

void ADriveGameMode::CheckpointReached(int32 Index)
{
	if (bInMenu || bMissionOver || Index != NextCheckpoint || Index >= TotalCheckpoints) return;

	Score += 100;
	++NextCheckpoint;
	if (NextCheckpoint >= TotalCheckpoints)
	{
		Score += FMath::RoundToInt(TimeLeft);
		EndMission(true);
	}
}

void ADriveGameMode::ReportCollision()
{
	if (bInMenu || bMissionOver) return;
	++Crashes;
	Score = FMath::Max(0, Score - Diff().CrashPenalty);
	if (Crashes >= Level().MaxCrashes) EndMission(false, TEXT("DAMAGE LIMIT"));
}

void ADriveGameMode::EndMission(bool bSuccess, const TCHAR* FailureReason)
{
	if (bMissionOver) return;
	bMissionOver = true;
	bLastMissionSuccess = bSuccess;
	ResetPlayerControls(this);
	EarnedStars = bSuccess ? 1 : 0;
	if (bSuccess && Crashes <= 1) ++EarnedStars;
	if (bSuccess && Crashes == 0 && TimeLeft >= StartingTime * 0.2f) ++EarnedStars;
	if (bSuccess)
	{
		LevelStars[LevelIndex] = FMath::Max(LevelStars[LevelIndex], EarnedStars);
		HighestUnlockedLevel = FMath::Max(HighestUnlockedLevel, FMath::Min(LevelIndex + 1, DriveLevels::NumLevels() - 1));
	}

	const FDifficultySpec& D = Diff();
	const int32 Final = FMath::RoundToInt(Score * D.ScoreMult * Level().ScoreMult);
	const int32 Before = TotalScore;
	TotalScore = static_cast<int32>(FMath::Min(static_cast<int64>(MAX_int32), static_cast<int64>(TotalScore) + Final));

	int32& Best = BestScores[static_cast<int32>(Difficulty)];
	const bool bNewBest = Final > Best;
	if (bNewBest) Best = Final;

	int32 UnlockedCars = 0;
	for (int32 i = 0; i < DriveCatalog::NumCars(); ++i)
	{
		const int32 U = DriveCatalog::Car(i).UnlockScore;
		if (Before < U && TotalScore >= U)
		{
			++UnlockedCars;
		}
	}

	const FString Unlocked = UnlockedCars > 0 ? FString::Printf(TEXT("   %d CARS UNLOCKED"), UnlockedCars) : FString();
	EndMessage = FString::Printf(TEXT("%s   %d pts   %d/3 STARS%s%s"),
		bSuccess ? TEXT("LEVEL COMPLETE") : FailureReason, Final, EarnedStars,
		bNewBest ? TEXT("   NEW BEST") : TEXT(""), *Unlocked);
	SaveProgress();
}

void ADriveGameMode::SetPlayerStart(const FVector& Loc, const FRotator& Rot)
{
	bHasStart = true;
	StartLoc = Loc;
	StartRot = Rot;
}

void ADriveGameMode::TeleportPlayerToStart()
{
	if (!bHasStart) return;
	if (ADriveVehicle* V = Cast<ADriveVehicle>(UGameplayStatics::GetPlayerPawn(this, 0)))
	{
		V->ResetForStart(StartLoc, StartRot);
	}
}

FString ADriveGameMode::GetMissionStatus() const
{
	if (TotalCheckpoints <= 0) return TEXT("Free drive");
	const int32 T = FMath::CeilToInt(TimeLeft);
	return FString::Printf(TEXT("LEVEL %d   %s   CP %d/%d   %d:%02d   SCORE %d   CRASH %d/%d"),
		LevelIndex + 1, Diff().Name, FMath::Min(NextCheckpoint + 1, TotalCheckpoints), TotalCheckpoints, T / 60, T % 60, Score, Crashes, Level().MaxCrashes);
}
