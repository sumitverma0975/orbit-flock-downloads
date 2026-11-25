#pragma once

#include "CoreMinimal.h"

enum class ECarClass : uint8 { Luxury, Supercar };
enum class EDriveTrain : uint8 { Front, Rear, All };
enum class ECarBodyStyle : uint8 { Sedan, SUV, Coupe, Limousine, GT, MidEngine, Hypercar, Track, Estate, Roadster };

/**
 * Native rigid-body vehicle profiles. Licensed model identities are distinguished
 * from original designs; physics tuning is game-oriented, not manufacturer data.
 */
struct FCarSpec
{
	const TCHAR* Id;
	const TCHAR* Name;
	ECarClass Class;
	float MassKg;
	float PowerScale;         // normalized garage stat
	float TopSpeedKmh;        // soft speed limiter
	float BrakeScale;
	float SteerScale;
	int32 UnlockScore;        // total career score needed
	const TCHAR* DesignLabel;
	EDriveTrain DriveType;
	ECarBodyStyle BodyStyle;
	FVector SizeCm;           // length X, width Y, roof height above road Z
	float WheelbaseCm;
	float TrackCm;
	float WheelRadiusCm;
	float EnginePowerKw;
	float LaunchAcceleration; // cm/s^2 before traction limiting
	float TireGrip;
	FLinearColor Paint;
	float EnginePitch;
};

enum class EDriveDifficulty : uint8 { Easy, Medium, Hard, Extreme };

struct FDifficultySpec
{
	const TCHAR* Name;
	const TCHAR* Blurb;
	float TimeMult;
	float TrafficFraction;        // share of spawned traffic that is active
	float PotholeFraction;        // share of spawned potholes that is active
	float PotholeSeverityMult;
	float TrainWaitMult;          // bigger = fewer trains
	int32 Checkpoints;
	int32 CrashPenalty;
	float FuelBurnMult;
	float ScoreMult;
};

namespace DriveCatalog
{
	int32 NumCars();
	const FCarSpec& Car(int32 Index);
	const FDifficultySpec& Difficulty(EDriveDifficulty D);
	FString CarBlueprintPath(int32 Index);   // /Game/Cars/BP_<Id>.BP_<Id>_C
	FString CarStaticMeshPath(int32 Index);  // optional complete imported visual
}
