#pragma once

#include "CoreMinimal.h"

/** Career challenges apply to the road network in the currently loaded city map. */
struct FDriveLevelSpec
{
	const TCHAR* Name;
	int32 Checkpoints;
	float TimeMult;
	float TrafficMult;
	float PotholeMult;
	int32 MaxCrashes;
	float ScoreMult;
};

namespace DriveLevels
{
	int32 NumLevels();
	const FDriveLevelSpec& Level(int32 Index);
}
