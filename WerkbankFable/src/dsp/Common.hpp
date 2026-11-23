// Werkbank: shared DSP pieces. Ported from tools/proto/wb_common.py. No Rack dependency.
#pragma once
#include <cmath>
#include <cstdint>
#include <algorithm>
#include <vector>

namespace wb {

static const double PI = 3.14159265358979323846;
static const double C4 = 261.6256;

inline double clampd(double x, double lo, double hi) { return x < lo ? lo : (x > hi ? hi : x); }

// splitmix64, as in Sawstack. Returns a uniform 0..1.
inline double splitmix(uint64_t& st) {
	st += 0x9E3779B97F4A7C15ULL;
	uint64_t z = st;
	z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
	z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
	z = z ^ (z >> 31);
	return (double)(z >> 11) / (double)(1ULL << 53);
}

// n bipolar uniforms that define one unit's personality at AGE = 1 (unit() in wb_common.py)
inline void unitValues(uint64_t seed, double* out, int n) {
	uint64_t st = seed;
	for (int i = 0; i < n; i++) out[i] = 2.0 * splitmix(st) - 1.0;
}

// xorshift32, uniform -1..1 (xs32 in wb_common.py)
struct XS32 {
	uint32_t s = 1;
	void seed(uint32_t v) { s = v ? v : 1; }
	double next() {
		s ^= s << 13;
		s ^= s >> 17;
		s ^= s << 5;
		return (double)s / 2147483648.0 - 1.0;
	}
};

// polyBLEP residual for a unit downward step at phase 0
inline double blep(double t, double dt) {
	if (t < dt) { double x = t / dt; return x + x - x * x - 1.0; }
	if (t > 1.0 - dt) { double x = (t - 1.0) / dt; return x * x + x + x + 1.0; }
	return 0.0;
}

// slow drift: two one-pole filtered noises (0.3 s and 8 s)
struct Drift {
	double d0 = 0, d1 = 0, a1 = 0, a2 = 0, g1 = 0, g2 = 0;
	void init(double fs) {
		a1 = 1.0 / (0.3 * fs); a2 = 1.0 / (8.0 * fs);
		g1 = std::sqrt(3.0 / a1) * 0.33; g2 = std::sqrt(3.0 / a2) * 0.33;
		d0 = d1 = 0;
	}
	double tick(double r) {
		d0 += a1 * (r * g1 - d0);
		d1 += a2 * (r * g2 - d1);
		return 0.4 * d0 + 0.6 * d1;
	}
};

// Sawstack's 2:1 half-band decimator (47-tap Blackman, exact zeros)
struct Decimator2 {
	static const int N = 47;
	double h[N];
	double buf[2 * N] = {};
	int pos = 0;
	Decimator2() {
		const int M = N - 1;
		double sum = 0;
		for (int n = 0; n < N; n++) {
			double t = n - M / 2.0;
			double s = t == 0 ? 0.5 : std::sin(PI * 0.5 * t) / (PI * t);
			double w = 0.42 - 0.5 * std::cos(2 * PI * n / M) + 0.08 * std::cos(4 * PI * n / M);
			h[n] = s * w; sum += h[n];
		}
		for (int n = 0; n < N; n++) h[n] /= sum;
		for (int n = 0; n < N; n++) if ((n - M / 2) % 2 == 0 && n != M / 2) h[n] = 0.0;
	}
	void reset() { std::fill(buf, buf + 2 * N, 0.0); pos = 0; }
	void push(double v) { buf[pos] = v; buf[pos + N] = v; pos = (pos + 1) % N; }
	double process(double a, double b) {
		push(a); push(b);
		const double* x = buf + pos;     // x[0] oldest .. x[N-1] newest
		const int c = N / 2;
		double y = h[c] * x[c];
		for (int d = 1; d <= c; d += 2) y += h[c - d] * (x[c - d] + x[c + d]);
		return y;
	}
};

// Zavalishin/Cytomic state-variable filter, trapezoidal
struct SVF {
	double ic1 = 0, ic2 = 0;
	double a1 = 0, a2 = 0, a3 = 0, k = 1;
	void set(double g, double kk) { k = kk; a1 = 1.0 / (1.0 + g * (g + k)); a2 = g * a1; a3 = g * a2; }
	// returns band; low and high through refs
	inline void tick(double x, double& band, double& low) {
		double v3 = x - ic2, v1 = a1 * ic1 + a2 * v3, v2 = ic2 + a2 * ic1 + a3 * v3;
		ic1 = 2.0 * v1 - ic1; ic2 = 2.0 * v2 - ic2;
		band = v1; low = v2;
	}
	void reset() { ic1 = ic2 = 0; }
};

// Schmitt trigger at 0.1 V / 1 V
struct Schmitt {
	bool high = false;
	bool process(float v) {
		if (high) { if (v <= 0.1f) high = false; return false; }
		if (v >= 1.f) { high = true; return true; }
		return false;
	}
};

}  // namespace wb
