#include "CarCatalog.h"

namespace
{
	const FCarSpec GCars[] =
	{
		// Existing IDs/order preserve saved car selections. Dimensions and power are gameplay tuning.
		{ TEXT("lux_sedan"), TEXT("BMW 320i E90"), ECarClass::Luxury, 1480.f, .40f, 220.f, 1.00f, 1.00f, 0,
		  TEXT("Supplied BMW model; game-tuned physics"), EDriveTrain::Rear, ECarBodyStyle::Sedan,
		  FVector(453,182,142), 276,154,33, 110,390,1.05f, FLinearColor(.12f,.23f,.42f), 1.05f },
		{ TEXT("lux_suv"), TEXT("Mercedes-Benz G-Class"), ECarClass::Luxury, 2480.f, .67f, 225.f, .95f, .82f, 0,
		  TEXT("Supplied Mercedes model; game-tuned physics"), EDriveTrain::All, ECarBodyStyle::SUV,
		  FVector(482,198,194), 289,166,41, 310,510,1.08f, FLinearColor(.07f,.08f,.10f), .78f },
		{ TEXT("lux_coupe"), TEXT("Rhein Sport Coupe"), ECarClass::Luxury, 1720.f, .73f, 280.f, 1.10f, 1.06f, 1500,
		  TEXT("Original Germany-inspired performance design"), EDriveTrain::Rear, ECarBodyStyle::Coupe,
		  FVector(468,188,134), 278,160,34, 310,650,1.16f, FLinearColor(.10f,.37f,.22f), 1.0f },
		{ TEXT("lux_limo"), TEXT("Berlin Executive L"), ECarClass::Luxury, 2350.f, .68f, 240.f, 1.00f, .78f, 3000,
		  TEXT("Original Germany-inspired luxury design"), EDriveTrain::All, ECarBodyStyle::Limousine,
		  FVector(570,194,150), 358,163,35, 300,490,1.10f, FLinearColor(.09f,.09f,.11f), .80f },
		{ TEXT("sup_gt"), TEXT("Mercedes-AMG GT"), ECarClass::Supercar, 1640.f, .85f, 304.f, 1.20f, 1.12f, 5000,
		  TEXT("Supplied Mercedes-AMG model; game-tuned physics"), EDriveTrain::Rear, ECarBodyStyle::GT,
		  FVector(456,194,129), 263,166,35, 390,760,1.25f, FLinearColor(.68f,.12f,.05f), .90f },
		{ TEXT("sup_mid"), TEXT("Stuttgart Mid-Engine"), ECarClass::Supercar, 1450.f, .90f, 315.f, 1.20f, 1.18f, 8000,
		  TEXT("Original Germany-inspired mid-engine design"), EDriveTrain::All, ECarBodyStyle::MidEngine,
		  FVector(449,194,122), 263,166,34, 440,850,1.30f, FLinearColor(.57f,.58f,.61f), 1.15f },
		{ TEXT("sup_hyper"), TEXT("Autobahn Hyper GT"), ECarClass::Supercar, 1500.f, 1.f, 355.f, 1.28f, 1.20f, 12000,
		  TEXT("Original Germany-inspired hypercar design"), EDriveTrain::All, ECarBodyStyle::Hypercar,
		  FVector(476,204,117), 280,171,35, 680,1050,1.38f, FLinearColor(.08f,.28f,.43f), 1.20f },
		{ TEXT("sup_track"), TEXT("Nuerburg Track RS"), ECarClass::Supercar, 1370.f, .97f, 325.f, 1.30f, 1.26f, 16000,
		  TEXT("Original Germany-inspired circuit design"), EDriveTrain::Rear, ECarBodyStyle::Track,
		  FVector(450,196,120), 266,166,34, 480,900,1.42f, FLinearColor(.68f,.58f,.06f), 1.28f },
		{ TEXT("lux_estate"), TEXT("Rhein Touring RS"), ECarClass::Luxury, 1950.f, .80f, 285.f, 1.12f, 1.02f, 1800,
		  TEXT("Original Germany-inspired touring design"), EDriveTrain::All, ECarBodyStyle::Estate,
		  FVector(491,193,148), 291,164,35, 370,670,1.16f, FLinearColor(.17f,.24f,.34f), .95f },
		{ TEXT("lux_grand"), TEXT("Black Forest GT"), ECarClass::Luxury, 1880.f, .85f, 300.f, 1.15f, 1.08f, 6500,
		  TEXT("Original Germany-inspired grand tourer"), EDriveTrain::Rear, ECarBodyStyle::GT,
		  FVector(493,192,133), 284,161,35, 400,720,1.20f, FLinearColor(.47f,.35f,.16f), .86f },
		{ TEXT("sup_spider"), TEXT("Tejas Spider"), ECarClass::Supercar, 1380.f, .98f, 335.f, 1.25f, 1.24f, 21000,
		  TEXT("Original open-top supercar design"), EDriveTrain::Rear, ECarBodyStyle::Roadster,
		  FVector(448,196,118), 267,167,34, 510,880,1.32f, FLinearColor(.62f,.09f,.13f), 1.22f },
		{ TEXT("sup_apex"), TEXT("Vajra Apex"), ECarClass::Supercar, 1250.f, 1.f, 380.f, 1.30f, 1.30f, 28000,
		  TEXT("Original extreme hypercar design"), EDriveTrain::All, ECarBodyStyle::Hypercar,
		  FVector(480,208,112), 281,175,35, 820,1150,1.45f, FLinearColor(.45f,.46f,.49f), 1.32f },
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

FString DriveCatalog::CarStaticMeshPath(int32 Index)
{
	const FString Id = Car(Index).Id;
	return FString::Printf(TEXT("/Game/Cars/Models/SM_%s.SM_%s"), *Id, *Id);
}
