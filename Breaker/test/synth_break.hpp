// A made-up funk break for testing (no records were harmed): kick, snare, ghost
// snares, hats, an open hat and a tom, 2 bars at 100 BPM with a little swing.
#pragma once
#include "../src/Analysis.hpp"
#include <random>

namespace testbreak {
using namespace breaker;
struct Truth { double t; int type; bool ghost; };

inline std::shared_ptr<Audio> make(float rate, std::vector<Truth>& truth, int variant = 0) {
	auto a = std::make_shared<Audio>();
	a->rate = rate;
	const double bpm = variant ? 92.0 : 100.0, six = 15.0 / bpm;
	const int bars = 2;
	size_t N = (size_t) (rate * six * 16 * bars);
	a->L.assign(N, 0.f); a->R.assign(N, 0.f);
	std::mt19937 rng(11 + variant);
	std::uniform_real_distribution<float> U(-1, 1);
	auto add = [&](double t, int type, float g, bool ghost) {
		truth.push_back({t, type, ghost});
		size_t s0 = (size_t) (t * rate);
		Biquad hp1, hp2, bp1, bp2;
		hp1.set(true, 7000, rate); hp2.set(true, 7000, rate);
		bp1.set(true, 900, rate); bp2.set(false, 6000, rate);
		double ph = 0;
		for (size_t i = s0; i < N && i < s0 + (size_t) (rate * 0.6); i++) {
			double tt = (i - s0) / (double) rate;
			float v = 0;
			if (type == KICK) {
				double f = 48 + 110 * std::exp(-tt * 30);
				ph += 2 * M_PI * f / rate;
				v = (float) (std::sin(ph) * std::exp(-tt * 9) + 0.25 * U(rng) * std::exp(-tt * 200));
			} else if (type == SNARE) {
				ph += 2 * M_PI * 190 / rate;
				float n = bp2.step(bp1.step(U(rng)));
				v = (float) (0.5 * std::sin(ph) * std::exp(-tt * 30) + 0.9 * n * std::exp(-tt * 16));
			} else if (type == HAT) {
				float n = hp2.step(hp1.step(U(rng)));
				v = (float) (1.2 * n * std::exp(-tt * (ghost ? 60 : 45)));
			} else if (type == 4) {   // open hat / cymbal: rings on
				float n = hp2.step(hp1.step(U(rng)));
				v = (float) (1.0 * n * std::exp(-tt * 4));
			} else {                 // tom
				double f = 120 + 40 * std::exp(-tt * 20);
				ph += 2 * M_PI * f / rate;
				v = (float) (std::sin(ph) * std::exp(-tt * 10));
			}
			a->L[i] += g * v * 0.6f; a->R[i] += g * v * 0.55f;
		}
	};
	// 2 bars, 16ths; swing 56%
	const char* K = variant ? "1000001000100000" "1000000100100100" : "1000000010100000" "1000000110000100";
	const char* S = variant ? "0000100000001000" "0000100000001001" : "0000100100001001" "0000100001001000";
	const char* G = variant ? "0001000101000100" "0100000100000100" : "0000000001000100" "0001000100000000";   // ghost snares
	const char* Hh = "1010101010101010" "1010101010100010";
	for (int s = 0; s < 32; s++) {
		double t = s * six + ((s & 1) ? 0.12 * six : 0.0);
		if (K[s] == '1') add(t, KICK, 1.0f, false);
		if (S[s] == '1') add(t, SNARE, 0.9f, false);
		else if (G[s] == '1') add(t, SNARE, 0.25f, true);
		if (Hh[s] == '1' && K[s] != '1' && S[s] != '1') add(t, HAT, (s % 4 == 0) ? 0.45f : 0.3f, false);
	}
	add(14 * six + 16 * six * 1, 4, 0.35f, false);     // open hat near the end of bar 2
	add(15 * six + 0.12 * six, OTHER, 0.6f, false);    // tom at the end of bar 1
	std::sort(truth.begin(), truth.end(), [](const Truth& x, const Truth& y) { return x.t < y.t; });
	// soft clip like a record
	for (size_t i = 0; i < N; i++) { a->L[i] = std::tanh(a->L[i]); a->R[i] = std::tanh(a->R[i]); }
	return a;
}
} // namespace testbreak
