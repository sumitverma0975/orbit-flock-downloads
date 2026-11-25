#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

/** Original, deterministic vehicle sound synthesis. No recordings or external assets. */
namespace DelhiDriveAudio
{
constexpr int SampleRate = 22050;
constexpr float TwoPi = 6.28318530718f;

struct FParameters
{
	float RPM = 0.f;
	float Throttle = 0.f;
	float SpeedKmh = 0.f;
	float Brake = 0.f;
	float Steer = 0.f;
	float PitchScale = 1.f;
	bool bHandbrake = false;
	bool bEngineRunning = false;
	bool bGameplayActive = false;
};

inline float FiniteClamp(float Value, float Low, float High)
{
	return std::isfinite(Value) ? std::clamp(Value, Low, High) : Low;
}

/** Game-thread-only DSP; QueueAudio transfers the generated samples to Unreal's audio thread. */
class FSynthesizer
{
public:
	void SetParameters(const FParameters& In)
	{
		Target = In;
		Target.RPM = FiniteClamp(In.RPM, 0.f, 1.f);
		Target.Throttle = FiniteClamp(In.Throttle, 0.f, 1.f);
		Target.SpeedKmh = FiniteClamp(In.SpeedKmh, 0.f, 500.f);
		Target.Brake = FiniteClamp(In.Brake, 0.f, 1.f);
		Target.Steer = FiniteClamp(In.Steer, -1.f, 1.f);
		Target.PitchScale = FiniteClamp(In.PitchScale, 0.65f, 1.5f);
	}

	void TriggerImpact(float Severity)
	{
		if (Target.bGameplayActive)
			Impact = std::max(Impact, FiniteClamp(Severity, 0.f, 1.f));
	}

	void Reset()
	{
		*this = FSynthesizer();
	}

	void Render(int16_t* OutPCM, int Count)
	{
		if (!Target.bGameplayActive && ActiveGain < 0.00001f)
		{
			// Menus keep the audio voice alive without evaluating the oscillators.
			std::fill_n(OutPCM, Count, int16_t(0));
			ActiveGain = 0.f;
			Impact = 0.f;
			return;
		}
		const float WantedEngine = Target.bEngineRunning ? 1.f : 0.f;
		const float WantedActive = Target.bGameplayActive ? 1.f : 0.f;
		const float WantedRoad = FiniteClamp(Target.SpeedKmh / 130.f, 0.f, 1.f);
		const float Moving = FiniteClamp((Target.SpeedKmh - 8.f) / 35.f, 0.f, 1.f);
		// A steering/braking approximation, not a claim to measure wheel slip.
		const float Corner = FiniteClamp((std::abs(Target.Steer) * Target.SpeedKmh - 45.f) / 75.f, 0.f, 1.f);
		const float WantedSkid = Moving * std::max({Target.Brake * 0.72f,
			Target.bHandbrake ? 1.f : 0.f, Corner * 0.7f});

		for (int I = 0; I < Count; ++I)
		{
			RPM += (Target.RPM - RPM) * 0.00075f;
			Throttle += (Target.Throttle - Throttle) * 0.001f;
			EngineGain += (WantedEngine - EngineGain) * 0.001f;
			ActiveGain += (WantedActive - ActiveGain) * 0.0018f;
			RoadGain += (WantedRoad - RoadGain) * 0.0008f;
			SkidGain += (WantedSkid - SkidGain) * 0.0012f;
			Pitch += (Target.PitchScale - Pitch) * 0.0005f;

			const float Noise = WhiteNoise();
			RoadLow += 0.035f * (Noise - RoadLow);
			RoadHigh += 0.30f * (Noise - RoadHigh);
			ImpactLow += 0.08f * (Noise - ImpactLow);

			const float Frequency = (34.f + RPM * 118.f) * Pitch;
			Advance(EnginePhase, Frequency);
			Advance(TirePhase, 790.f + Target.SpeedKmh * 2.1f);
			Advance(ImpactPhase, 87.f);
			const float EngineWave = std::sin(EnginePhase)
				+ 0.43f * std::sin(2.f * EnginePhase + 0.25f)
				+ (0.16f + Throttle * 0.13f) * std::sin(3.f * EnginePhase)
				+ Throttle * 0.11f * std::sin(6.f * EnginePhase);
			const float Engine = EngineWave * (0.115f + 0.085f * Throttle + 0.025f * RPM) * EngineGain;
			const float Road = (RoadHigh - RoadLow + RoadLow * 0.9f) * RoadGain * 0.31f;
			const float Tire = (0.075f * std::sin(TirePhase) + (Noise - RoadHigh) * 0.16f) * SkidGain;
			const float Hit = Impact * (0.64f * ImpactLow + 0.30f * Noise
				+ 0.22f * std::sin(ImpactPhase));
			Impact *= 0.99955f;
			if (Impact < 0.00001f) Impact = 0.f;

			const float Mixed = (Engine + Road + Tire + Hit) * ActiveGain;
			// Soft saturation prevents stacked engine/skid/impact transients from clipping.
			const float Limited = Mixed / (1.f + std::abs(Mixed) * 0.45f);
			OutPCM[I] = static_cast<int16_t>(FiniteClamp(Limited, -0.96f, 0.96f) * 32767.f);
		}
	}

private:
	float WhiteNoise()
	{
		NoiseState ^= NoiseState << 13;
		NoiseState ^= NoiseState >> 17;
		NoiseState ^= NoiseState << 5;
		return static_cast<float>(NoiseState >> 8) * (2.f / 16777215.f) - 1.f;
	}
	static void Advance(float& Phase, float Frequency)
	{
		Phase += TwoPi * Frequency / static_cast<float>(SampleRate);
		if (Phase >= TwoPi) Phase -= TwoPi;
	}

	FParameters Target;
	uint32_t NoiseState = 0x74A12B39u;
	float RPM = 0.f, Throttle = 0.f, Pitch = 1.f;
	float EngineGain = 0.f, ActiveGain = 0.f, RoadGain = 0.f, SkidGain = 0.f;
	float EnginePhase = 0.f, TirePhase = 0.f, ImpactPhase = 0.f;
	float RoadLow = 0.f, RoadHigh = 0.f, ImpactLow = 0.f, Impact = 0.f;
};
}
