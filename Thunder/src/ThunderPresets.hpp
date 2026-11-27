// Thunder factory memories (what slots 1-8 hold on a fresh module).
// Values are knob positions, in this order (MEM_* in Thunder.cpp):
//   algoA algoB morph mode tune bend sweep click decay p1 p2 noise
//   cutoff res fenv vcf drive tone send time fdbk accent wear mutate
// tune: 0..1 = 20 Hz .. 2560 Hz (20 * 128^x). decay: 0..1 = 12 ms .. 4.8 s, top 3 % = drone.
// cutoff: 0..1 = 20 Hz .. 20 kHz. vcf: 0 HP, 1 BP, 2 LP.
#pragma once
#include "ThunderEngine.hpp"
#include <cmath>

namespace thunder {

static const int NUM_MEM_VALUES = 24;
static const int NUM_MEMORIES = 8;

struct Preset {
	const char* name;
	float v[NUM_MEM_VALUES];
};

inline const Preset& factoryPreset(int i) {
	static const Preset P[NUM_MEMORIES] = {
		//                algA algB morph mode tune   bend  sweep click decay p1    p2    noise cut   res   fenv  vcf drive tone  send  time  fdbk  acc   wear  mut
		{"Thunder Kick",  {0,   2,   0.15f, 0, 0.189f, 0.45f, 0.30f, 0.35f, 0.56f, 0.35f, 0.50f, 0.00f, 1.00f, 0.10f, 0.00f, 2, 0.35f, 0.70f, 0.00f, 0.45f, 0.30f, 0.50f, 0.10f, 0.00f}},
		{"Surf Kick",     {1,   0,   0.00f, 0, 0.190f, 0.50f, 0.35f, 0.20f, 0.60f, 0.20f, 0.60f, 0.00f, 0.85f, 0.20f, 0.10f, 2, 0.30f, 0.60f, 0.00f, 0.45f, 0.30f, 0.50f, 0.10f, 0.00f}},
		{"Boom",          {2,   0,   0.00f, 0, 0.170f, 0.30f, 0.25f, 0.25f, 0.70f, 0.40f, 0.25f, 0.00f, 1.00f, 0.10f, 0.00f, 2, 0.15f, 0.70f, 0.00f, 0.45f, 0.30f, 0.50f, 0.05f, 0.00f}},
		{"FM Perc",       {3,   9,   0.20f, 0, 0.494f, 0.20f, 0.20f, 0.20f, 0.40f, 0.55f, 0.45f, 0.00f, 0.90f, 0.20f, 0.00f, 2, 0.20f, 0.80f, 0.30f, 0.40f, 0.40f, 0.50f, 0.30f, 0.80f}},
		{"Res Snare",     {4,   5,   0.10f, 1, 0.453f, 0.15f, 0.20f, 0.40f, 0.47f, 0.55f, 0.55f, 0.00f, 0.12f, 0.10f, 0.00f, 0, 0.30f, 0.75f, 0.10f, 0.35f, 0.25f, 0.50f, 0.15f, 0.00f}},
		{"Slap",          {5,   4,   0.00f, 0, 0.474f, 0.00f, 0.20f, 0.10f, 0.50f, 0.45f, 0.35f, 0.00f, 0.25f, 0.10f, 0.00f, 0, 0.20f, 0.80f, 0.25f, 0.30f, 0.25f, 0.50f, 0.15f, 0.00f}},
		{"Dust Hat",      {7,   8,   0.25f, 0, 0.617f, 0.00f, 0.20f, 0.10f, 0.27f, 0.60f, 0.30f, 0.00f, 0.45f, 0.10f, 0.00f, 0, 0.10f, 0.90f, 0.00f, 0.30f, 0.25f, 0.50f, 0.25f, 0.20f}},
		{"Tom to Bell",   {6,   11,  0.30f, 0, 0.351f, 0.25f, 0.35f, 0.20f, 0.58f, 0.40f, 0.40f, 0.00f, 0.90f, 0.15f, 0.00f, 2, 0.20f, 0.75f, 0.20f, 0.50f, 0.35f, 0.50f, 0.10f, 0.10f}},
	};
	return P[i < 0 ? 0 : i >= NUM_MEMORIES ? NUM_MEMORIES - 1 : i];
}

// knob positions (memory order) -> engine controls. LEVEL, LOCK and SEND-patched are set by the caller.
inline void valuesToControls(const float* v, Controls& c) {
	c.algoA = std::max(0, std::min(NUM_ALGOS - 1, (int) std::round(v[0])));
	c.algoB = std::max(0, std::min(NUM_ALGOS - 1, (int) std::round(v[1])));
	c.morph = v[2];
	c.mode = std::max(0, std::min(2, (int) std::round(v[3])));
	c.tuneHz = 20.f * std::pow(128.f, v[4]);
	c.bend = v[5];
	c.sweep = v[6];
	c.click = v[7];
	c.decay = v[8];
	c.p1 = v[9];
	c.p2 = v[10];
	c.noise = v[11];
	c.cutoff = v[12];
	c.res = v[13];
	c.fenv = v[14];
	c.vcf = std::max(0, std::min(2, (int) std::round(v[15])));
	c.drive = v[16];
	c.tone = v[17];
	c.send = v[18];
	c.time = v[19];
	c.fdbk = v[20];
	c.accentAmt = v[21];
	c.wear = v[22];
	c.mutate = v[23];
}

} // namespace thunder
