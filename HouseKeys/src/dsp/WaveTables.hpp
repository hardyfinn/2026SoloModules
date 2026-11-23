// Wave memory for the WAVE engine, rebuilt by synthesis at start-up (spec 7.3).
#pragma once
#include <vector>
#include <cstdint>

namespace hk {

enum WaveKind { WK_SINGLE = 0, WK_LOOP, WK_ONESHOT };

struct WaveBand {
	std::vector<float> f16;   // 16-bit quantised, for GRIT 0 (linear interpolation)
	std::vector<int8_t> q8;   // 8-bit, the machine
	std::vector<int32_t> c8;  // prefix sums of q8 (len + 1)
	std::vector<int8_t> q6;   // 6-bit (stored as multiples of 4)
	std::vector<int32_t> c6;
};

struct Wave {
	int kind = WK_SINGLE;
	int len = 256;           // samples; 256 per nominal cycle in every kind
	WaveBand band[8];        // per octave of played pitch, from C1
};

// built once, shared by every instance (thread-safe)
const std::vector<Wave>& waveTables();

// band for a played frequency
inline int waveBandFor(float hz) {
	int b = 0;
	float f = 32.7032f * 2.f;
	while (b < 7 && hz >= f) { b++; f *= 2.f; }
	return b;
}

} // namespace hk
