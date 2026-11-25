// Standalone DSP smoke test: c++ -std=c++17 -O2 Tools/verify_driving_audio.cpp -o /tmp/verify_driving_audio
// Run with an optional output .wav path to preview the original synthesized sound.
#include "../Source/DelhiDrive/DriveAudioSynth.h"
#include <cassert>
#include <fstream>
#include <iostream>
#include <limits>
#include <vector>

static double RMS(const std::vector<int16_t>& Samples)
{
	double Sum = 0;
	for (int16_t Sample : Samples) Sum += double(Sample) * double(Sample);
	return std::sqrt(Sum / Samples.size());
}

static void WriteLE(std::ofstream& Out, uint32_t Value, int Bytes)
{
	for (int I = 0; I < Bytes; ++I) Out.put(char((Value >> (8 * I)) & 255));
}

int main(int argc, char** argv)
{
	using namespace DelhiDriveAudio;
	FSynthesizer Synth;
	FParameters P;
	std::vector<int16_t> Second(SampleRate), Preview;
	Synth.Render(Second.data(), SampleRate);
	assert(RMS(Second) == 0.0);
	P.bGameplayActive = P.bEngineRunning = true;
	P.RPM = 0.14f;
	Synth.SetParameters(P);
	Synth.Render(Second.data(), SampleRate);
	const double Idle = RMS(Second);
	assert(Idle > 1000.0);
	Preview.insert(Preview.end(), Second.begin(), Second.end());
	for (int Step = 0; Step < 60; ++Step)
	{
		P.RPM = 0.14f + Step / 70.f;
		P.Throttle = 0.9f;
		P.SpeedKmh = Step * 2.1f;
		Synth.SetParameters(P);
		const int Count = SampleRate / 10;
		Synth.Render(Second.data(), Count);
		Preview.insert(Preview.end(), Second.begin(), Second.begin() + Count);
	}
	P.Brake = 1.f; P.bHandbrake = true;
	Synth.SetParameters(P);
	Synth.TriggerImpact(1.f);
	Synth.Render(Second.data(), SampleRate);
	const double Braking = RMS(Second);
	assert(Braking > Idle);
	Preview.insert(Preview.end(), Second.begin(), Second.end());
	P.bGameplayActive = false;
	Synth.SetParameters(P);
	Synth.Render(Second.data(), SampleRate);
	Synth.Render(Second.data(), SampleRate);
	assert(RMS(Second) == 0.0);
	// A fuel-empty car still makes rolling noise, and stationary handbraking is silent.
	Synth.Reset();
	P = FParameters();
	P.bGameplayActive = true;
	P.SpeedKmh = 90.f;
	Synth.SetParameters(P);
	Synth.Render(Second.data(), SampleRate);
	assert(RMS(Second) > 500.0);
	Synth.Reset();
	P.SpeedKmh = 0.f;
	P.Brake = 1.f;
	P.bHandbrake = true;
	Synth.SetParameters(P);
	Synth.Render(Second.data(), SampleRate);
	assert(RMS(Second) == 0.0);
	Synth.TriggerImpact(0.8f);
	Synth.Render(Second.data(), SampleRate);
	assert(RMS(Second) > 300.0);
	Synth.Render(Second.data(), SampleRate);
	assert(RMS(Second) < 1.0); // A collision transient must finish rather than drone.
	Synth.Reset();
	P.bGameplayActive = false;
	P.RPM = std::numeric_limits<float>::infinity();
	P.PitchScale = std::numeric_limits<float>::quiet_NaN();
	P.SpeedKmh = -100.f;
	Synth.SetParameters(P);
	Synth.Render(Second.data(), SampleRate);
	assert(RMS(Second) == 0.0);
	int Peak = 0;
	for (int16_t Sample : Preview) Peak = std::max(Peak, std::abs(int(Sample)));
	assert(Peak < 32767);
	if (argc > 1)
	{
		std::ofstream Out(argv[1], std::ios::binary);
		const uint32_t Bytes = static_cast<uint32_t>(Preview.size() * 2);
		Out.write("RIFF",4); WriteLE(Out,36+Bytes,4); Out.write("WAVEfmt ",8);
		WriteLE(Out,16,4); WriteLE(Out,1,2); WriteLE(Out,1,2);
		WriteLE(Out,SampleRate,4); WriteLE(Out,SampleRate*2,4);
		WriteLE(Out,2,2); WriteLE(Out,16,2); Out.write("data",4); WriteLE(Out,Bytes,4);
		for (int16_t Sample : Preview) WriteLE(Out,static_cast<uint16_t>(Sample),2);
		if (!Out) return 1;
	}
	std::cout << "PASS: silence, idle, acceleration, skid/impact, menu fade, engine-off rolling, stationary brakes, invalid inputs; peak="
		<< Peak << " idle_rms=" << Idle << " braking_rms=" << Braking << '\n';
}
