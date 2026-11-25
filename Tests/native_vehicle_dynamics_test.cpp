#include "../Source/DelhiDrive/DriveVehicleDynamics.h"
#include <cassert>
#include <iostream>

using namespace DriveVehicleDynamics;

int main()
{
    // All vehicle masses must support their own weight at the intended 15 cm sag.
    for (float mass : {1250.f, 1480.f, 1720.f, 1950.f, 2480.f})
    {
        const float static_load = SuspensionLoad(mass, 15.f, 15.f, 0.f);
        assert(std::abs(static_load * 4.f - mass * GravityCm) < 1.f);
        // Drop the car from its 90 cm spawn height. Semi-implicit integration
        // approximates a rigid body frame; actual contact queries remain UE-only.
        for (float dt : {1.f / 30.f, 1.f / 60.f, 1.f / 120.f})
        {
            float height = 90.f;
            float velocity = 0.f;
            for (int step = 0; step < static_cast<int>(8.f / dt); ++step)
            {
                const float ray_distance = height + 10.f;
                float load = 0.f;
                if (ray_distance <= 45.f + 18.f + 34.f)
                {
                    const float length = std::clamp(ray_distance - 34.f, 27.f, 63.f);
                    load = SuspensionLoad(mass, 15.f, std::max(0.f, 45.f - length), velocity);
                }
                velocity += (load * 4.f / mass - GravityCm) * dt;
                height += velocity * dt;
                assert(std::isfinite(height) && height > 20.f && height < 110.f);
            }
            assert(std::abs(height - 54.f) < .15f);
            assert(std::abs(velocity) < .15f);
        }
    }
    // Brakes dissipate motion in both directions and never add reverse velocity.
    for (float speed : {-3000.f, -1.f, -.001f, 0.f, .001f, 1.f, 3000.f})
    {
        for (float dt : {1.f / 30.f, 1.f / 60.f, 1.f / 120.f})
        {
            const float force = OpposingBrakeForce(speed, 450.f, 1000000.f, dt);
            const float after = speed + force / 450.f * dt;
            assert(force * speed <= 0.f);
            assert(after * speed >= -.001f);
            assert(std::abs(after) <= std::abs(speed) + .001f);
        }
    }
    // The friction circle bounds combined cornering + acceleration, including no-contact cases.
    for (float cap : {0.f, 50000.f, 500000.f})
    {
        const auto force = LimitTraction(700000.f, -900000.f, cap);
        assert(std::hypot(force.Longitudinal, force.Lateral) <= cap + .1f);
        assert(force.Longitudinal >= 0.f && force.Lateral <= 0.f);
    }
    // 100 kW at 10 m/s means 10,000 N = 1,000,000 Unreal force units.
    assert(std::abs(PowerLimitedForce(100.f, 1000.f, 2000000.f) - 1000000.f) < .1f);
    assert(PowerLimitedForce(100.f, 2000.f, 2000000.f) < PowerLimitedForce(100.f, 1000.f, 2000000.f));
    std::cout << "PASS: static support, suspension drop stability at 30/60/120 Hz, forward/reverse braking, friction circle, and power units.\n";
    std::cout << "Force-math validation only; Unreal/Chaos integration and device performance remain untested.\n";
}
