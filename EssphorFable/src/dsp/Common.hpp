// Shared pieces for the three Essphor engines. No Rack dependency.
#pragma once
#include <cmath>
#include <cstdint>
#include <algorithm>

#ifndef ESSPHOR_AGE
#define ESSPHOR_AGE 1   // 0 compiles the AGE code out (test 9 and 16 only)
#endif

namespace essphor {

static const double PI = 3.14159265358979323846;

// xorshift64* with a Box-Muller normal. Deterministic from its seed.
struct Rng {
	uint64_t s = 0x9E3779B97F4A7C15ull;
	bool haveSpare = false;
	double spare = 0.0;
	void seed(uint64_t v) { s = v ? v : 0x9E3779B97F4A7C15ull; haveSpare = false; for (int i = 0; i < 4; i++) next(); }
	uint64_t next() { s ^= s >> 12; s ^= s << 25; s ^= s >> 27; return s * 0x2545F4914F6CDD1Dull; }
	double uniform() { return (next() >> 11) * (1.0 / 9007199254740992.0); }   // [0, 1)
	double bipolar() { return 2.0 * uniform() - 1.0; }                        // [-1, 1)
	double normal() {
		if (haveSpare) { haveSpare = false; return spare; }
		double u1 = std::max(1e-300, uniform()), u2 = uniform();
		double r = std::sqrt(-2.0 * std::log(u1));
		spare = r * std::sin(2 * PI * u2); haveSpare = true;
		return r * std::cos(2 * PI * u2);
	}
};

// The 20 scales, pitch classes above the root. Index 19 is FREE (no quantising).
static const int NUM_SCALES = 20;
static const int SCALE_FREE = 19;
static const char* const SCALE_NAMES[NUM_SCALES] = {
	"CHROMATIC", "MAJOR", "MINOR", "DORIAN", "LYDIAN", "SUPER LOCRIAN", "MIXOLYDIAN", "HEX AEOLIAN", "HEX DORIAN", "BLUES",
	"PENTATONIC", "HIRAJOSHI", "KUMOI", "IWATO", "WHOLE TONE", "PELOG", "TETRATONIC", "FIFTHS", "OCTAVES", "FREE"};
struct ScaleDef { int n; int deg[12]; };
static const ScaleDef SCALES[NUM_SCALES] = {
	{12, {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11}},
	{7, {0, 2, 4, 5, 7, 9, 11}},
	{7, {0, 2, 3, 5, 7, 8, 10}},
	{7, {0, 2, 3, 5, 7, 9, 10}},
	{7, {0, 2, 4, 6, 7, 9, 11}},
	{7, {0, 1, 3, 4, 6, 8, 10}},
	{7, {0, 2, 4, 5, 7, 9, 10}},
	{6, {0, 3, 5, 7, 8, 10}},
	{6, {0, 2, 3, 5, 7, 10}},
	{6, {0, 3, 5, 6, 7, 10}},
	{5, {0, 3, 5, 7, 10}},
	{5, {0, 2, 3, 7, 8}},
	{5, {0, 2, 3, 7, 9}},
	{5, {0, 1, 5, 6, 10}},
	{6, {0, 2, 4, 6, 8, 10}},
	{5, {0, 1, 3, 7, 8}},
	{4, {0, 4, 7, 11}},
	{2, {0, 7}},
	{1, {0}},
	{0, {0}},
};

// Nearest scale interval to a pitch shift in semitones; ties go down (quantise() in mosaic_proto.py).
inline double quantise(double semi, int scale) {
	if (scale < 0 || scale >= SCALE_FREE) return semi;
	const ScaleDef& sd = SCALES[scale];
	double o = std::floor(semi / 12.0);
	double best = 0.0; bool have = false;
	for (int k = -1; k <= 1; k++)
		for (int i = 0; i < sd.n; i++) {
			double c = (o + k) * 12.0 + sd.deg[i];
			if (!have || std::fabs(c - semi) < std::fabs(best - semi)) { best = c; have = true; }
		}
	return best;
}

// Output limiter: linear to 1.6, smooth knee to a ceiling of 2.4 (internal units, 1 = 5 V).
inline double softlimit(double x) {
	double a = std::fabs(x);
	if (a <= 1.6) return x;
	double y = 1.6 + 0.8 * std::tanh((a - 1.6) / 0.8);
	return x < 0 ? -y : y;
}

// Sound-on-sound clip: linear to 1, then tanh-shaped to 1.5.
inline double sosclip(double x) {
	double a = std::fabs(x);
	if (a <= 1.0) return x;
	double y = 1.0 + 0.5 * std::tanh((a - 1.0) / 0.5);
	return x < 0 ? -y : y;
}

// Two-pole TPT state-variable filter (end caps, follower filter).
struct Svf {
	double s1 = 0, s2 = 0, G = 0, R = 0.5, d = 1;
	void set(double f, double q, double fs) { G = std::tan(PI * std::min(f, 0.45 * fs) / fs); R = 1 / (2 * q); d = 1 / (1 + 2 * R * G + G * G); }
	void run(double x, double& lp, double& bp, double& hp) {
		hp = (x - (2 * R + G) * s1 - s2) * d;
		bp = G * hp + s1; s1 = G * hp + bp;
		lp = G * bp + s2; s2 = G * bp + lp;
	}
};

// White noise through two one-pole low-passes, as in the references (warp, drift).
struct SlowRandom {
	double t = 0.0, y = 0.0, a = 1.0, k = 0.0;
	void init(double tau, double fs) { a = std::exp(-1.0 / (tau * fs)); k = std::sqrt(1.0 - a * a); }
	double step(Rng& r) { t = t * a + r.normal() * k; y = y * a + t * (1.0 - a); return y; }
};

// Tempo and beat phase from an internal tempo or a clock input.
struct Clock {
	double phase = 0.0;          // beats since reset
	double bps = 2.0;            // beats per second
	int ppqn = 1;                // clock pulses per beat
	bool stopped = false;        // clock patched but silent for more than 2 s
	bool resetNow = false;       // a reset happened this sample
	double sinceLast = 1e9, iv0 = -1, iv1 = -1, lastTarget = -1.0;
	double slewLeft = 0.0, slewRate = 0.0;
	bool wasPatched = false;

	void reset() { phase = 0.0; lastTarget = -1.0 / ppqn; slewLeft = 0.0; resetNow = true; }

	void tick(double dt, bool patched, bool edge, bool resetEdge, double internalBpm) {
		resetNow = false;
		if (resetEdge) reset();
		if (!patched) {
			wasPatched = false; stopped = false;
			bps = internalBpm / 60.0;
			phase += bps * dt;
			return;
		}
		if (!wasPatched) { wasPatched = true; sinceLast = 1e9; iv0 = iv1 = -1; lastTarget = std::round(phase * ppqn) / ppqn - 1.0 / ppqn; }
		sinceLast += dt;
		if (edge) {
			if (sinceLast < 2.0) {
				iv0 = iv1; iv1 = sinceLast;
				double avg = iv0 > 0 ? 0.5 * (iv0 + iv1) : iv1;
				if (avg > 1e-4) bps = 1.0 / (avg * ppqn);
			} else {
				// first pulse after silence: line up with the nearest pulse position
				lastTarget = std::round(phase * ppqn) / ppqn - 1.0 / ppqn;
			}
			sinceLast = 0.0;
			stopped = false;
			double target = lastTarget + 1.0 / ppqn;
			lastTarget = target;
			double err = target - phase;
			if (std::fabs(err) > 0.1 / ppqn) { phase = target; slewLeft = 0.0; }
			else { slewLeft = 0.02; slewRate = err / 0.02; }
		}
		if (sinceLast > 2.0) stopped = true;
		double adv = bps * dt;
		if (slewLeft > 0.0) { double s = std::min(dt, slewLeft); adv += slewRate * s; slewLeft -= s; }
		phase += adv;
		// while the clock runs, never get ahead of where the next pulse is due
		if (!stopped) phase = std::min(phase, lastTarget + 1.0 / ppqn);
	}
};

}  // namespace essphor
