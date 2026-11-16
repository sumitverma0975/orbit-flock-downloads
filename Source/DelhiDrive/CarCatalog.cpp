#include "CarCatalog.h"

namespace
{
	const FCarSpec GCars[] =
	{
		// Id             Name                 Class               kg      pwr   top   brk   steer unlock
		{ TEXT("lux_sedan"),  TEXT("Maharaja Sedan"),  ECarClass::Luxury,  1950.f, 0.60f, 190.f, 0.90f, 0.90f, 0     },
		{ TEXT("lux_suv"),    TEXT("Nawab SUV"),       ECarClass::Luxury,  2400.f, 0.65f, 200.f, 0.85f, 0.85f, 0     },
		{ TEXT("lux_coupe"),  TEXT("Sultan Coupe"),    ECarClass::Luxury,  1800.f, 0.75f, 240.f, 1.00f, 1.00f, 1500  },
		{ TEXT("lux_limo"),   TEXT("Shahi Limousine"), ECarClass::Luxury,  2800.f, 0.55f, 170.f, 0.80f, 0.70f, 3000  },
		{ TEXT("sup_gt"),     TEXT("Rani GT"),         ECarClass::Supercar,1550.f, 0.85f, 290.f, 1.10f, 1.10f, 5000  },
		{ TEXT("sup_mid"),    TEXT("Garuda Mid-Engine"),ECarClass::Supercar,1400.f,0.95f, 320.f, 1.15f, 1.20f, 8000  },
		{ TEXT("sup_hyper"),  TEXT("Vayu Hypercar"),   ECarClass::Supercar,1300.f, 1.00f, 360.f, 1.25f, 1.25f, 12000 },
		{ TEXT("sup_track"),  TEXT("Agni Track Special"),ECarClass::Supercar,1200.f,1.00f, 380.f, 1.30f, 1.30f, 16000 },
		// Append new entries so existing saves retain their selected car index.
		{ TEXT("lux_estate"), TEXT("Dilli Grand Estate"),ECarClass::Luxury, 2100.f,0.70f, 220.f, 1.00f, 0.95f, 1800 },
		{ TEXT("lux_grand"),  TEXT("Raj Grand Tourer"), ECarClass::Luxury, 1850.f,0.82f, 265.f, 1.10f, 1.05f, 6500 },
		{ TEXT("sup_spider"), TEXT("Tejas Spider"),     ECarClass::Supercar,1380.f,0.98f, 335.f, 1.25f, 1.28f, 21000 },
		{ TEXT("sup_apex"),   TEXT("Vajra Apex"),       ECarClass::Supercar,1250.f,1.00f, 390.f, 1.30f, 1.30f, 28000 },
	};

	const FDifficultySpec GDiff[] =
	{
		// Name            Blurb                                                   time  traf  pothole sev  train cps pen  fuel  score
		{ TEXT("EASY"),    TEXT("Light traffic, few potholes, generous time"),     1.5f, 0.30f, 0.30f, 0.6f, 1.6f, 3, 10, 0.6f, 1.0f },
		{ TEXT("MEDIUM"),  TEXT("Busy roads, regular potholes and trains"),        1.0f, 0.60f, 0.60f, 1.0f, 1.0f, 5, 20, 1.0f, 1.5f },
		{ TEXT("HARD"),    TEXT("Heavy traffic, rough roads, tight clock"),        0.75f,0.85f, 0.85f, 1.3f, 0.7f, 7, 35, 1.3f, 2.0f },
		{ TEXT("EXTREME"), TEXT("Delhi rush hour: everything, all the time"),      0.55f,1.00f, 1.00f, 1.6f, 0.5f, 8, 50, 1.6f, 3.0f },
	};
}

int32 DriveCatalog::NumCars() { return UE_ARRAY_COUNT(GCars); }

const FCarSpec& DriveCatalog::Car(int32 Index)
{
	return GCars[FMath::Clamp(Index, 0, static_cast<int32>(UE_ARRAY_COUNT(GCars)) - 1)];
}

const FDifficultySpec& DriveCatalog::Difficulty(EDriveDifficulty D)
{
	return GDiff[FMath::Clamp(static_cast<int32>(D), 0, static_cast<int32>(UE_ARRAY_COUNT(GDiff)) - 1)];
}

FString DriveCatalog::CarBlueprintPath(int32 Index)
{
	const FString Id = Car(Index).Id;
	return FString::Printf(TEXT("/Game/Cars/BP_%s.BP_%s_C"), *Id, *Id);
}
