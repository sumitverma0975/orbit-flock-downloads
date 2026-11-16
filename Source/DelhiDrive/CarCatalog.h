#pragma once

#include "CoreMinimal.h"

enum class ECarClass : uint8 { Luxury, Supercar };

/**
 * Handling profile for one car. Names are fictional on purpose (real brands need licences).
 * PowerScale/MassKg/TopSpeed/Brake/Steer are applied on top of whatever engine the car Blueprint has,
 * so ONE Blueprint can already drive like all 12 cars. Give each car its own Blueprint when you have the models.
 */
struct FCarSpec
{
	const TCHAR* Id;          // Blueprint is /Game/Cars/BP_<Id>
	const TCHAR* Name;
	ECarClass Class;
	float MassKg;
	float PowerScale;         // 0..1 of the Blueprint's engine output
	float TopSpeedKmh;        // soft speed limiter
	float BrakeScale;
	float SteerScale;
	int32 UnlockScore;        // total career score needed
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
}
