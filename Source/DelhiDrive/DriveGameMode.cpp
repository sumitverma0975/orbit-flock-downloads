#include "DriveGameMode.h"
#include "DriveMissionRules.h"
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
	if (UClass* Fallback = StaticLoadClass(ADriveVehicle::StaticClass(), nullptr, *FallbackCarPath, nullptr, LOAD_NoWarn)) return Fallback;
	return ADriveVehicle::StaticClass();
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
		Session = static_cast<EDriveSession>(FMath::Clamp(S->Session, 0, 2));
		SchoolIndex = FMath::Clamp(S->SchoolIndex, 0, DriveLevels::NumLessons() - 1);
		SchoolStars = S->SchoolStars;
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
	SchoolStars.SetNumZeroed(DriveLevels::NumLessons());
	for (int32& Stars : SchoolStars) Stars = FMath::Clamp(Stars, 0, 3);
	HighestUnlockedLesson = 0;
	while (HighestUnlockedLesson + 1 < DriveLevels::NumLessons() && SchoolStars[HighestUnlockedLesson] > 0) ++HighestUnlockedLesson;
	SchoolIndex = FMath::Min(SchoolIndex, HighestUnlockedLesson);
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
	S->Session = static_cast<int32>(Session);
	S->SchoolIndex = SchoolIndex;
	S->SchoolStars = SchoolStars;
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
	if (IsTour()) return;
	const int32 N = ChallengeCount();
	int32& Index = IsSchool() ? SchoolIndex : LevelIndex;
	Index = ((Index + Delta) % N + N) % N;
	ApplyDifficulty();
	SaveProgress();
}

void ADriveGameMode::ChangeSession()
{
	if (!bInMenu) return;
	Session = static_cast<EDriveSession>((static_cast<int32>(Session) + 1) % 3);
	ApplyDifficulty();
	SaveProgress();
}

void ADriveGameMode::RecoverVehicle()
{
	if (bInMenu || bMissionOver) return;
	ADriveVehicle* V = Cast<ADriveVehicle>(UGameplayStatics::GetPlayerPawn(this, 0));
	if (!V) return;
	const float Fuel = V->FuelPercent;
	const int32 Previous = NextCheckpoint - 1;
	const FVector Loc = CheckpointLocations.IsValidIndex(Previous) ? CheckpointLocations[Previous] + FVector(0.f,0.f,110.f) : StartLoc;
	const FRotator Rot = CheckpointRotations.IsValidIndex(Previous) ? CheckpointRotations[Previous] : StartRot;
	V->ResetForStart(Loc, Rot);
	V->FuelPercent = IsTour() ? 100.f : Fuel;
	PreviousLocation = Loc;
	PreviousYaw = Rot.Yaw;
	StopHold = 0.f;
	if (!IsTour() && !IsSchool())
	{
		TimeLeft = FMath::Max(0.f, TimeLeft - 10.f);
		Score = FMath::Max(0, Score - 100);
	}
	ResetPlayerControls(this);
	Coach(IsTour() || IsSchool() ? TEXT("Car recovered. Check the route and set off gently.") : TEXT("Recovered: 10 seconds and 100 route points deducted."), 5.f);
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
	if (IsSchool()) ++SchoolIndex; else ++LevelIndex;
	StartMission();
}

void ADriveGameMode::StartMission()
{
	if (!IsCarUnlocked(CarIndex) || !IsLevelUnlocked()) return;
	if (!Cast<ADriveVehicle>(UGameplayStatics::GetPlayerPawn(this, 0))) return;
	if (!IsTour() && RegisteredCheckpoints <= 0) { Coach(TEXT("The road route is still loading.")); return; }
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
	ResultFeedback.Reset();
	SpeedingSeconds = 0.f;
	HarshEvents = 0;
	DistanceMeters = 0.f;
	StopHold = 0.f;
	ElapsedTime = 0.f;
	HarshCooldown = 2.f;
	ApplyDifficulty();
	TeleportPlayerToStart();
	if (ADriveVehicle* V = Cast<ADriveVehicle>(UGameplayStatics::GetPlayerPawn(this, 0)))
	{
		V->FuelPercent = IsTour() ? 100.f : Level().StartingFuel;
		PreviousLocation = V->GetActorLocation();
		PreviousYaw = V->GetActorRotation().Yaw;
	}
	Coach(IsTour() ? TEXT("Explore freely: no timer, no fuel limit. RECOVER returns you to the route start.") : Level().Instruction, 10.f);
	ResetPlayerControls(this);
}

// ------------------------------------------------------------------ difficulty

void ADriveGameMode::RecomputeTotals()
{
	if (IsTour()) { TotalCheckpoints = 0; return; }
	const int32 ExtraCheckpoints = IsSchool() ? 0 : FMath::Max(0, (Diff().Checkpoints - 3) / 2);
	TotalCheckpoints = FMath::Min(RegisteredCheckpoints, FMath::Min(8, Level().Checkpoints + ExtraCheckpoints));
}

void ADriveGameMode::ApplyDifficulty()
{
	const FDifficultySpec& D = Diff();
	RecomputeTotals();
	StartingTime = IsTour() ? 0.f : SecondsPerCheckpoint * FMath::Max(1, TotalCheckpoints) * D.TimeMult * Level().TimeMult;
	TimeLeft = StartingTime;

	// The world is built once at maximum density; difficulty just switches things on and off.
	for (TActorIterator<APotholeActor> It(GetWorld()); It; ++It)
	{
		It->SetDifficultyActive(It->Rank < FMath::Clamp(D.PotholeFraction * (IsTour() ? 0.6f : Level().PotholeMult), 0.f, 1.f));
	}
	for (TActorIterator<ATrafficVehicle> It(GetWorld()); It; ++It)
	{
		It->SetDifficultyActive(It->Rank < FMath::Clamp(D.TrafficFraction * (IsTour() ? 1.f : Level().TrafficMult), 0.f, 1.f));
	}
}

// ------------------------------------------------------------------ mission

void ADriveGameMode::Coach(const FString& Text, float Seconds)
{
	CoachMessage = Text;
	CoachTime = Seconds;
}

void ADriveGameMode::Tick(float DT)
{
	Super::Tick(DT);
	if (bInMenu || bMissionOver) return;
	ADriveVehicle* V = Cast<ADriveVehicle>(UGameplayStatics::GetPlayerPawn(this, 0));
	if (!V) return;
	const float Step = FMath::Clamp(DT, 0.f, 0.25f);
	ElapsedTime += Step;
	CoachTime = FMath::Max(0.f, CoachTime - Step);
	HarshCooldown = FMath::Max(0.f, HarshCooldown - Step);
	const float Travel = FVector::Dist2D(V->GetActorLocation(), PreviousLocation);
	if (Travel < 10000.f) DistanceMeters += Travel * .01f;
	PreviousLocation = V->GetActorLocation();
	if (IsTour()) { V->FuelPercent = 100.f; return; }
	if (TotalCheckpoints <= 0) return;
	const float Speed = V->GetSpeedKmh();
	const float Accel = V->GetLongitudinalAccelerationMps2();
	const float YawRate = Step > .001f ? FMath::Abs(FMath::FindDeltaAngleDegrees(PreviousYaw, static_cast<float>(V->GetActorRotation().Yaw))) / Step : 0.f;
	PreviousYaw = V->GetActorRotation().Yaw;
	if (ElapsedTime > 2.f && HarshCooldown <= 0.f && (FMath::Abs(Accel) > 8.5f || (Speed > 30.f && YawRate > 75.f)))
	{
		++HarshEvents;
		HarshCooldown = 2.f;
		Coach(TEXT("Ease the pedals and steering. Brake before the corner, then turn."));
		if (HarshEvents > Level().MaxHarshEvents) { EndMission(false,TEXT("RIDE TOO ROUGH")); return; }
	}
	if (Speed > Level().SpeedLimitKmh + 3.f)
	{
		SpeedingSeconds += Step;
		if (CoachTime <= 0.f) Coach(FString::Printf(TEXT("Ease off: limit %.0f km/h. Watch your speeding allowance."),Level().SpeedLimitKmh));
		if (SpeedingSeconds > Level().SpeedingAllowanceSeconds) { EndMission(false,TEXT("SPEED LIMIT EXCEEDED")); return; }
	}
	FVector Target; FRotator TargetRot;
	if (GetNavigationTarget(Target,TargetRot) && FVector::Dist2D(V->GetActorLocation(),Target) > 450.f) StopHold = 0.f;
	TimeLeft = FMath::Max(0.f,TimeLeft - Step);
	if (V->FuelPercent <= 0.f) EndMission(false,TEXT("FUEL EMPTY"));
	else if (TimeLeft <= 0.f) EndMission(false);
}

void ADriveGameMode::RegisterCheckpoint(int32 Index, const FVector& Loc, const FRotator& Rot)
{
	if (Index < 0 || Index >= 64) return;
	RegisteredCheckpoints = FMath::Max(RegisteredCheckpoints, Index + 1);
	CheckpointLocations.SetNum(RegisteredCheckpoints);
	CheckpointRotations.SetNum(RegisteredCheckpoints);
	CheckpointLocations[Index] = Loc;
	CheckpointRotations[Index] = Rot;
	RecomputeTotals();
	if (bInMenu)
	{
		StartingTime = IsTour() ? 0.f : SecondsPerCheckpoint * FMath::Max(1, TotalCheckpoints) * Diff().TimeMult * Level().TimeMult;
		TimeLeft = StartingTime;
	}
}

bool ADriveGameMode::IsCheckpointActive(int32 Index) const
{
	return DriveMissionRules::ActiveCheckpoint(Index,NextCheckpoint,TotalCheckpoints,bInMenu,bMissionOver,IsTour());
}

void ADriveGameMode::EvaluateCheckpoint(int32 Index, float DT)
{
	if (!IsCheckpointActive(Index) || !CheckpointLocations.IsValidIndex(Index)) return;
	const ADriveVehicle* V = Cast<ADriveVehicle>(UGameplayStatics::GetPlayerPawn(this,0));
	if (!V) return;
	const bool bParking = Level().Objective == EDriveObjective::Parking && Index == TotalCheckpoints - 1;
	const bool bStop = bParking || Level().Objective == EDriveObjective::ControlledStop;
	const float Distance = FVector::Dist2D(V->GetActorLocation(),CheckpointLocations[Index]);
	const FVector BayPosition = CheckpointRotations[Index].UnrotateVector(V->GetActorLocation()-CheckpointLocations[Index]);
	if ((bParking && !DriveMissionRules::InParkingBay(BayPosition.X,BayPosition.Y)) || (!bParking && Distance > 450.f)) { StopHold = 0.f; return; }
	if (bStop)
	{
		const float HeadingError = FMath::Abs(FMath::FindDeltaAngleDegrees(V->GetActorRotation().Yaw,CheckpointRotations[Index].Yaw));
		if (!DriveMissionRules::ValidStop(V->GetSpeedKmh(),HeadingError,bParking))
		{
			StopHold = 0.f;
			if (CoachTime <= 0.f) Coach(bParking && HeadingError > 25.f ? TEXT("Align with the bay arrow. Use reverse to straighten the car.") : TEXT("Brake gently to a full stop inside the marker."));
			return;
		}
		StopHold = DriveMissionRules::AccumulateHold(StopHold,DT,true);
		if (!DriveMissionRules::HoldComplete(StopHold,Level().StopHoldSeconds)) return;
	}
	CheckpointReached(Index);
}

void ADriveGameMode::CheckpointReached(int32 Index)
{
	if (bInMenu || bMissionOver || Index != NextCheckpoint || Index >= TotalCheckpoints) return;

	Score += 100;
	StopHold = 0.f;
	++NextCheckpoint;
	Coach(Level().Objective == EDriveObjective::ControlledStop ? TEXT("Delivery complete. Check traffic before moving off.") : TEXT("Checkpoint reached. Follow the next amber marker."));
	if (NextCheckpoint >= TotalCheckpoints)
	{
		const ADriveVehicle* V = Cast<ADriveVehicle>(UGameplayStatics::GetPlayerPawn(this,0));
		if (V && V->FuelPercent < Level().RequiredFuel) { EndMission(false,TEXT("FUEL RESERVE MISSED")); return; }
		Score += FMath::RoundToInt(TimeLeft);
		EndMission(true);
	}
}

void ADriveGameMode::ReportCollision()
{
	if (bInMenu || bMissionOver) return;
	++Crashes;
	Coach(TEXT("Collision: slow down, leave space and look through the next corner."));
	if (IsTour()) return;
	Score = FMath::Max(0, Score - Diff().CrashPenalty);
	if (Crashes >= Level().MaxCrashes) EndMission(false, TEXT("DAMAGE LIMIT"));
}

void ADriveGameMode::EndMission(bool bSuccess, const TCHAR* FailureReason)
{
	if (bMissionOver) return;
	bMissionOver = true;
	bLastMissionSuccess = bSuccess;
	ResetPlayerControls(this);
	EarnedStars = DriveMissionRules::Stars(bSuccess,Crashes,SpeedingSeconds,HarshEvents,TimeLeft,StartingTime);
	if (bSuccess)
	{
		if (IsSchool())
		{
			SchoolStars[SchoolIndex] = FMath::Max(SchoolStars[SchoolIndex],EarnedStars);
			HighestUnlockedLesson = FMath::Max(HighestUnlockedLesson,FMath::Min(SchoolIndex+1,DriveLevels::NumLessons()-1));
		}
		else
		{
			LevelStars[LevelIndex] = FMath::Max(LevelStars[LevelIndex], EarnedStars);
			HighestUnlockedLevel = FMath::Max(HighestUnlockedLevel, FMath::Min(LevelIndex + 1, DriveLevels::NumLevels() - 1));
		}
	}

	const FDifficultySpec& D = Diff();
	const int32 Final = bSuccess ? FMath::RoundToInt(Score * D.ScoreMult * Level().ScoreMult) : 0;
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
		bSuccess ? (IsSchool() ? TEXT("LESSON PASSED") : TEXT("CHALLENGE COMPLETE")) : FailureReason, Final, EarnedStars,
		bNewBest ? TEXT("   NEW BEST") : TEXT(""), *Unlocked);
	ResultFeedback = FString::Printf(TEXT("%.0f m driven | %d collisions | %.1f s speeding | %d harsh manoeuvres"),DistanceMeters,Crashes,SpeedingSeconds,HarshEvents);
	CoachMessage = bSuccess ? TEXT("Three stars: no collisions, almost no speeding, smooth inputs and time to spare.") : Level().Instruction;
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

int32 ADriveGameMode::ChallengeStars() const
{
	const TArray<int32>& Stars = IsSchool() ? SchoolStars : LevelStars;
	return Stars.IsValidIndex(SelectedChallenge()) ? Stars[SelectedChallenge()] : 0;
}

bool ADriveGameMode::GetNavigationTarget(FVector& OutPosition, FRotator& OutRotation) const
{
	if (IsTour() || !CheckpointLocations.IsValidIndex(NextCheckpoint) || NextCheckpoint >= TotalCheckpoints) return false;
	OutPosition = CheckpointLocations[NextCheckpoint];
	OutRotation = CheckpointRotations[NextCheckpoint];
	return true;
}

bool ADriveGameMode::GetRouteGuidanceTarget(FVector& OutPosition) const
{
	FRotator Rotation;
	if (!GetNavigationTarget(OutPosition,Rotation)) return false;
	const APawn* Car = UGameplayStatics::GetPlayerPawn(this,0);
	if (!Car || NavigationRoute.Num()<2) return true;
	// Project onto the connected route, then aim a short distance ahead on it.
	// Restrict the search to this checkpoint leg so nearby parallel roads do not
	// send the driver across blocks or skip forward to a later part of the route.
	auto Project = [&](const FVector& Point, float From, float To)
	{
		float Along=0.f, BestAlong=From, BestDistance=MAX_flt;
		for (int32 i=1;i<NavigationRoute.Num();++i)
		{
			const FVector A=NavigationRoute[i-1], B=NavigationRoute[i];
			const float Length=FVector::Dist2D(A,B);
			if (Length>.1f && Along+Length>=From && Along<=To)
			{
				const FVector FlatDelta(B.X-A.X,B.Y-A.Y,0.f);
				const FVector FlatPoint(Point.X-A.X,Point.Y-A.Y,0.f);
				const float Low=FMath::Clamp((From-Along)/Length,0.f,1.f), High=FMath::Clamp((To-Along)/Length,0.f,1.f);
				const float T=FMath::Clamp(static_cast<float>(FVector::DotProduct(FlatPoint,FlatDelta)/(Length*Length)),Low,High);
				const float Distance=FVector::DistSquared2D(Point,FMath::Lerp(A,B,T));
				if (Distance<BestDistance) { BestDistance=Distance; BestAlong=Along+T*Length; }
			}
			Along+=Length;
		}
		return BestAlong;
	};
	const float End=Project(OutPosition,0.f,MAX_flt);
	const float Begin=NextCheckpoint>0 ? Project(CheckpointLocations[NextCheckpoint-1],0.f,End) : 0.f;
	const float Current=Project(Car->GetActorLocation(),FMath::Max(0.f,Begin-1000.f),End);
	const float Guidance=FMath::Min(End,Current+1000.f);
	float Along=0.f;
	for (int32 i=1;i<NavigationRoute.Num();++i)
	{
		const float Length=FVector::Dist2D(NavigationRoute[i-1],NavigationRoute[i]);
		if (Length>.1f && Along+Length>=Guidance)
		{
			if (End-Current>1200.f) OutPosition=FMath::Lerp(NavigationRoute[i-1],NavigationRoute[i],FMath::Clamp((Guidance-Along)/Length,0.f,1.f));
			return true;
		}
		Along+=Length;
	}
	return true;
}

FString ADriveGameMode::GetNavigationStatus() const
{
	FVector Target; FRotator Rotation;
	const APawn* V = UGameplayStatics::GetPlayerPawn(this,0);
	if (!V || !GetNavigationTarget(Target,Rotation)) return IsTour() ? TEXT("Explore any connected road. RECOVER resets your car.") : FString();
	const float MarkerDistance = FVector::Dist2D(Target,V->GetActorLocation());
	FVector Guidance=Target;
	GetRouteGuidanceTarget(Guidance);
	const FVector Delta = Guidance - V->GetActorLocation();
	const float Bearing = FMath::FindDeltaAngleDegrees(V->GetActorRotation().Yaw,Delta.Rotation().Yaw);
	const TCHAR* Direction = FMath::Abs(Bearing) < 25.f ? TEXT("AHEAD") : (FMath::Abs(Bearing) > 145.f ? TEXT("BEHIND") : (Bearing > 0.f ? TEXT("RIGHT") : TEXT("LEFT")));
	return FString::Printf(TEXT("FOLLOW ROAD %s  |  %.0f m to marker  %d / %d"),Direction,MarkerDistance*.01f,NextCheckpoint+1,TotalCheckpoints);
}

FString ADriveGameMode::GetObjectiveText() const
{
	if (IsTour()) return TEXT("UNTIMED TOUR | Unlimited fuel | Enjoy the city | TAB / MENU to change car or mode");
	if (StopHold > 0.f) return FString::Printf(TEXT("Hold still... %.1f / %.1f seconds"),StopHold,Level().StopHoldSeconds);
	if (CoachTime > 0.f && !CoachMessage.IsEmpty()) return CoachMessage;
	return Level().Instruction;
}

FString ADriveGameMode::GetMissionStatus() const
{
	if (IsTour()) return FString::Printf(TEXT("%s TOUR | %.2f km explored | %s"),CityName(),DistanceMeters*.001f,Diff().Name);
	const int32 T = FMath::CeilToInt(TimeLeft);
	return FString::Printf(TEXT("%s %02d | %d:%02d | LIMIT %.0f | SPEEDING %.1f/%.0f s | CRASH %d/%d | HARSH %d/%d"),
		IsSchool()?TEXT("LESSON"):TEXT("CAREER"),SelectedChallenge()+1,T/60,T%60,Level().SpeedLimitKmh,SpeedingSeconds,Level().SpeedingAllowanceSeconds,Crashes,Level().MaxCrashes,HarshEvents,Level().MaxHarshEvents);
}
