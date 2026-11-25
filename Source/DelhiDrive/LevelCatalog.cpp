#include "LevelCatalog.h"

namespace
{
	const FDriveLevelSpec GLevels[] = {
		{ TEXT("Neighbourhood Welcome"), 3, 1.50f,.45f,.25f,6,1.f, EDriveObjective::Checkpoints, TEXT("Follow the amber route. Brake before turns; look ahead for traffic."), 50.f,20.f,20,100.f,0.f,0.f },
		{ TEXT("Parcel Stops"), 3,1.45f,.60f,.35f,5,1.15f, EDriveObjective::ControlledStop, TEXT("Stop inside EVERY marker below 2 km/h; hold the brake for two seconds."), 45.f,18.f,20,100.f,0.f,2.f },
		{ TEXT("Embassy Arrival"), 3,1.40f,.65f,.40f,4,1.3f, EDriveObjective::Parking, TEXT("At the final bay align with its arrow, stop below 2 km/h and hold."), 45.f,15.f,15,100.f,0.f,2.f },
		{ TEXT("School Zone Patrol"), 4,1.45f,.70f,.45f,4,1.4f, EDriveObjective::SpeedControl, TEXT("Stay below 35 km/h. More than eight seconds speeding fails the route."), 35.f,8.f,15,100.f,0.f,0.f },
		{ TEXT("Fragile Cargo"), 4,1.35f,.75f,.60f,3,1.5f, EDriveObjective::SmoothDrive, TEXT("Ease on the gas and brake. Avoid hard acceleration, braking and abrupt steering."), 45.f,12.f,6,100.f,0.f,0.f },
		{ TEXT("Last Quarter Tank"), 5,1.25f,.85f,.65f,4,1.6f, EDriveObjective::FuelSave, TEXT("Start with 25% fuel. Finish with at least 5%; coast early and avoid detours."), 55.f,18.f,15,25.f,5.f,0.f },
		{ TEXT("Market Deliveries"), 5,1.25f,1.f,.85f,3,1.75f, EDriveObjective::ControlledStop, TEXT("Busy streets: stop fully at each delivery marker without blocking the crossing."), 45.f,10.f,12,100.f,0.f,2.f },
		{ TEXT("Executive Chauffeur"), 6,1.20f,1.05f,.90f,3,1.9f, EDriveObjective::SmoothDrive, TEXT("Keep your passenger comfortable: at most four harsh manoeuvres."), 50.f,10.f,4,100.f,0.f,0.f },
		{ TEXT("Rush Hour Arrival"), 6,1.15f,1.20f,1.f,3,2.05f, EDriveObjective::Parking, TEXT("Navigate rush hour; park facing the arrow at the final marker."), 55.f,10.f,10,100.f,0.f,3.f },
		{ TEXT("Economy Challenge"), 7,1.15f,1.25f,1.10f,3,2.2f, EDriveObjective::FuelSave, TEXT("A 22% tank must last the route. Keep 5% in reserve at the finish."), 60.f,8.f,10,22.f,5.f,0.f },
		{ TEXT("Capital Precision"), 7,1.10f,1.35f,1.15f,2,2.35f, EDriveObjective::SpeedControl, TEXT("Precise city driving: stay below 45 km/h with only five seconds' tolerance."), 45.f,5.f,8,100.f,0.f,0.f },
		{ TEXT("Roadmaster Assessment"), 8,1.10f,1.45f,1.25f,2,2.5f, EDriveObjective::Parking, TEXT("Combine smooth control, a 50 km/h limit and a three-second final park."), 50.f,5.f,5,100.f,0.f,3.f },
	};
	const FDriveLevelSpec GLessons[] = {
		{ TEXT("1. Start and Stop"),1,2.f,0.f,0.f,4,.55f, EDriveObjective::ControlledStop, TEXT("Slide up GAS for more power, down to ease off. Use BRAKE to stop inside the marker (keyboard W / S)."),30.f,30.f,20,100.f,0.f,2.f },
		{ TEXT("2. Steering and Corners"),2,1.8f,.15f,0.f,4,.65f, EDriveObjective::Checkpoints, TEXT("Turn the touch wheel or A/D. Slow before a bend and unwind the wheel after it."),40.f,25.f,20,100.f,0.f,0.f },
		{ TEXT("3. Respect the Limit"),3,1.8f,.25f,.10f,3,.75f, EDriveObjective::SpeedControl, TEXT("Use short gas presses and coast to hold 40 km/h. Watch the speeding allowance."),40.f,8.f,20,100.f,0.f,0.f },
		{ TEXT("4. Park with Control"),2,2.f,.20f,0.f,3,.8f, EDriveObjective::Parking, TEXT("Approach slowly; align with the bay arrow. D/R or R selects reverse for corrections."),35.f,20.f,20,100.f,0.f,2.f },
		{ TEXT("5. Smooth Passenger Ride"),3,1.8f,.30f,.15f,3,.9f, EDriveObjective::SmoothDrive, TEXT("Build speed gradually and brake early. Six harsh manoeuvres is the maximum."),45.f,15.f,6,100.f,0.f,0.f },
		{ TEXT("6. Driving Test"),4,1.7f,.45f,.25f,2,1.f, EDriveObjective::Parking, TEXT("Apply every skill: 45 km/h limit, smooth corners, then align and park."),45.f,8.f,5,100.f,0.f,3.f },
	};
}
int32 DriveLevels::NumLevels() { return UE_ARRAY_COUNT(GLevels); }
int32 DriveLevels::NumLessons() { return UE_ARRAY_COUNT(GLessons); }
const FDriveLevelSpec& DriveLevels::Level(int32 Index) { return GLevels[FMath::Clamp(Index,0,NumLevels()-1)]; }
const FDriveLevelSpec& DriveLevels::Lesson(int32 Index) { return GLessons[FMath::Clamp(Index,0,NumLessons()-1)]; }
const TCHAR* DriveLevels::ObjectiveName(EDriveObjective Objective)
{
	switch (Objective)
	{
	case EDriveObjective::ControlledStop: return TEXT("STOP & DELIVER");
	case EDriveObjective::Parking: return TEXT("PARKING");
	case EDriveObjective::SpeedControl: return TEXT("SPEED CONTROL");
	case EDriveObjective::SmoothDrive: return TEXT("SMOOTH DRIVING");
	case EDriveObjective::FuelSave: return TEXT("FUEL ECONOMY");
	default: return TEXT("CITY ROUTE");
	}
}
