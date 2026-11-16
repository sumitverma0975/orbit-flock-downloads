#include "LevelCatalog.h"

namespace
{
	const FDriveLevelSpec GLevels[] = {
		// Name                         checkpoints time traffic potholes crashes score
		{ TEXT("First Drive"),             3, 1.40f, 0.45f, 0.35f, 12, 1.00f },
		{ TEXT("City Cruise"),             3, 1.30f, 0.60f, 0.45f, 11, 1.10f },
		{ TEXT("Market Run"),              4, 1.20f, 0.70f, 0.60f, 10, 1.20f },
		{ TEXT("Crossing Patrol"),         4, 1.10f, 0.80f, 0.70f,  9, 1.30f },
		{ TEXT("Pothole Precision"),       5, 1.05f, 0.85f, 1.10f,  8, 1.40f },
		{ TEXT("Evening Commute"),         5, 1.00f, 1.00f, 1.00f,  7, 1.50f },
		{ TEXT("Executive Express"),       6, 0.95f, 1.10f, 1.10f,  6, 1.60f },
		{ TEXT("Rush Hour"),               6, 0.90f, 1.25f, 1.15f,  6, 1.75f },
		{ TEXT("Rough Road Rally"),        7, 0.85f, 1.30f, 1.45f,  5, 1.90f },
		{ TEXT("Precision Sprint"),        7, 0.80f, 1.40f, 1.50f,  4, 2.10f },
		{ TEXT("Capital Challenge"),       8, 0.75f, 1.50f, 1.60f,  3, 2.30f },
		{ TEXT("Roadmaster Finale"),       8, 0.70f, 1.65f, 1.75f,  3, 2.50f },
	};
}

int32 DriveLevels::NumLevels() { return UE_ARRAY_COUNT(GLevels); }

const FDriveLevelSpec& DriveLevels::Level(int32 Index)
{
	return GLevels[FMath::Clamp(Index, 0, static_cast<int32>(UE_ARRAY_COUNT(GLevels)) - 1)];
}
