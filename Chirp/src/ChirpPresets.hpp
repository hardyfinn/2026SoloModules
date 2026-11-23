// Factory presets, written in real units and turned into knob positions.
// Used by tools/export_presets.cpp (writes presets/Chirp/*.vcvm) and test/render.cpp.
#pragma once
#include "ChirpEngine.hpp"

namespace chirp {

struct Preset {
	const char* name;
	float pitch, sweep, time, curve, decay, tone;
	int repeat;
	float rate;
	bool loop;
};

// pitch: V/OCT (0 = C4)  sweep: octaves  time/decay: seconds  rate: chirps per second
static const Preset PRESETS[] = {
	{"Kick",        -2.4f,  3.2f,  0.060f, -0.70f, 0.45f, 0.05f,  1, 10.f, false},
	{"Tom",         -1.0f,  1.0f,  0.120f, -0.40f, 0.35f, 0.10f,  1, 10.f, false},
	{"Rim",          1.0f,  1.0f,  0.004f, -0.50f, 0.025f, 0.70f, 1, 10.f, false},
	{"Tick",         3.0f,  1.5f,  0.002f,  0.00f, 0.012f, 1.00f, 1, 10.f, false},
	{"Zap",         -1.5f,  5.0f,  0.080f, -0.30f, 0.12f, 0.40f,  1, 10.f, false},
	{"Bass Drop",   -2.0f,  1.0f,  0.250f,  0.20f, 1.20f, 0.55f,  1, 10.f, false},
	{"Laser",        0.5f,  3.5f,  0.300f, -0.60f, 0.40f, 0.20f,  1, 10.f, false},
	{"Droplet",      1.2f, -1.5f,  0.030f,  0.50f, 0.04f, 0.00f,  1, 10.f, false},
	{"Bat",          2.6f,  1.3f,  0.004f, -0.20f, 0.006f, 0.00f, 8, 18.f, false},
	{"Frog",         0.0f, -1.2f,  0.090f,  0.30f, 0.12f, 0.85f,  3,  7.f, false},
	{"Cricket",      2.0f, -0.15f, 0.010f,  0.00f, 0.015f, 0.30f, 4, 30.f, false},
	{"Cicada",       2.2f,  0.4f,  0.004f,  0.00f, 0.005f, 0.90f, 1, 120.f, true},
	{"Bird Up",      2.0f, -1.0f,  0.070f,  0.40f, 0.09f, 0.05f,  3,  9.f, false},
	{"Bird Down",    1.6f,  1.2f,  0.050f, -0.20f, 0.07f, 0.00f,  5, 14.f, false},
	{"Siren",        1.0f, -2.0f,  0.500f,  0.00f, 4.00f, 0.30f,  1, 1.6f, true},
	{"Slow Wobble", -3.0f,  4.0f,  1.500f,  0.00f, 4.00f, 0.00f,  1, 0.6f, true},
};
static const int NUM_PRESETS = sizeof(PRESETS) / sizeof(PRESETS[0]);

inline Settings settingsFor(const Preset& p) {
	Settings s;
	s.pitch = p.pitch; s.sweep = p.sweep; s.time = p.time; s.curve = p.curve;
	s.decay = p.decay; s.tone = p.tone; s.repeat = p.repeat; s.rate = p.rate; s.loop = p.loop;
	return s;
}

} // namespace chirp
