#pragma once

#include <algorithm>
#include <cmath>

/** Engine-independent force math in Unreal units (kg, cm, seconds).
 * Keeping these equations independent permits numerical regression tests without
 * pretending that such tests validate Chaos integration or a packaged game.
 */
namespace DriveVehicleDynamics
{
	constexpr float GravityCm = 980.f;

	inline float SuspensionLoad(float VehicleMass, float SagCm, float CompressionCm, float VerticalSpeedCm)
	{
		const float QuarterMass = std::max(25.f, VehicleMass * .25f);
		const float QuarterWeight = QuarterMass * GravityCm;
		const float Spring = QuarterWeight / std::max(1.f, SagCm);
		const float Damper = 2.f * std::sqrt(Spring * QuarterMass) * .72f;
		return std::clamp(Spring * std::max(0.f, CompressionCm) - Damper * VerticalSpeedCm, 0.f, QuarterWeight * 4.f);
	}

	inline float OpposingBrakeForce(float SpeedCm, float SupportedMass, float RequestedForce, float DT)
	{
		const float Sign = SpeedCm > 0.f ? 1.f : (SpeedCm < 0.f ? -1.f : 0.f);
		return -Sign * std::min(std::max(0.f, RequestedForce), std::abs(SpeedCm) * SupportedMass / std::max(.001f, DT));
	}

	struct FTireForces { float Longitudinal; float Lateral; };
	inline FTireForces LimitTraction(float Longitudinal, float Lateral, float MaximumForce)
	{
		const float Length = std::sqrt(Longitudinal * Longitudinal + Lateral * Lateral);
		const float Cap = std::max(0.f, MaximumForce);
		const float Scale = Length > Cap && Length > 0.f ? Cap / Length : 1.f;
		return { Longitudinal * Scale, Lateral * Scale };
	}

	inline float PowerLimitedForce(float PowerKw, float SpeedCm, float MaximumForce)
	{
		// kW -> W, and W / (cm/s) -> kg*cm/s^2 requires another factor of 10,000.
		return std::min(MaximumForce, PowerKw * 1000.f * 10000.f / std::max(600.f, std::abs(SpeedCm)));
	}
}
