#pragma once
#include <algorithm>
#include <cmath>

/** Engine-independent goal boundaries, also exercised by the standalone rule tests. */
namespace DriveMissionRules
{
	inline bool ActiveCheckpoint(int Index, int Next, int Total, bool InMenu, bool Over, bool Tour)
	{
		return !InMenu && !Over && !Tour && Index >= 0 && Index == Next && Index < Total;
	}
	inline bool ValidStop(float SpeedKmh, float HeadingError, bool Parking)
	{
		return std::isfinite(SpeedKmh) && std::abs(SpeedKmh) <= 2.f && (!Parking || (std::isfinite(HeadingError) && std::abs(HeadingError) <= 25.f));
	}
	inline bool InParkingBay(float LongitudinalCm, float LateralCm)
	{
		return std::abs(LongitudinalCm) <= 250.f && std::abs(LateralCm) <= 75.f;
	}
	inline float AccumulateHold(float Previous, float DeltaSeconds, bool Valid)
	{
		return Valid ? std::max(0.f, Previous) + std::clamp(DeltaSeconds, 0.f, .25f) : 0.f;
	}
	inline bool HoldComplete(float Hold, float Required)
	{
		return Hold + .001f >= std::max(0.f, Required);
	}
	inline int Stars(bool Success, int Crashes, float SpeedingSeconds, int HarshEvents, float TimeLeft, float StartingTime)
	{
		if (!Success) return 0;
		int Result = 1;
		if (Crashes <= 1 && SpeedingSeconds <= 3.f) ++Result;
		if (Crashes == 0 && HarshEvents <= 1 && SpeedingSeconds < 1.f && TimeLeft >= StartingTime * .2f) ++Result;
		return Result;
	}
}
