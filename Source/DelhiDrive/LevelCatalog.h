#pragma once

#include "CoreMinimal.h"

enum class EDriveSession : uint8 { Career, School, Tour };
enum class EDriveObjective : uint8 { Checkpoints, ControlledStop, Parking, SpeedControl, SmoothDrive, FuelSave };

/** Shared rules for city career routes and short driving-school lessons. */
struct FDriveLevelSpec
{
	const TCHAR* Name;
	int32 Checkpoints;
	float TimeMult;
	float TrafficMult;
	float PotholeMult;
	int32 MaxCrashes;
	float ScoreMult;
	EDriveObjective Objective;
	const TCHAR* Instruction;
	float SpeedLimitKmh;
	float SpeedingAllowanceSeconds;
	int32 MaxHarshEvents;
	float StartingFuel;
	float RequiredFuel;
	float StopHoldSeconds;
};

namespace DriveLevels
{
	int32 NumLevels();
	const FDriveLevelSpec& Level(int32 Index);
	int32 NumLessons();
	const FDriveLevelSpec& Lesson(int32 Index);
	const TCHAR* ObjectiveName(EDriveObjective Objective);
}
