// Shared DSP building blocks for House Keys. No Rack dependency.
#pragma once
#include <cmath>
#include <cstdint>
#include <cstring>
#include <algorithm>
#include <vector>

namespace hk {

static constexpr float PI_F = 3.14159265358979f;
static constexpr double PI_D = 3.14159265358979323846;

inline float clampf(float x, float a, float b) { return x < a ? a : (x > b ? b : x); }
inline float dbToGain(float db) { return std::pow(10.f, db / 20.f); }
inline float semis(float s) { return std::exp2(s / 12.f); }

// rational tanh, exact at 0, saturates at +-3
inline float ftanh(float x) {
	if (x > 3.f) return 1.f;
	if (x < -3.f) return -1.f;
	float x2 = x * x;
	return x * (27.f + x2) / (27.f + 9.f * x2);
}

inline uint32_t hash32(uint32_t x) {
	x ^= x >> 16; x *= 0x7feb352dU; x ^= x >> 15; x *= 0x846ca68bU; x ^= x >> 16;
	return x;
}
inline uint32_t hash3(uint32_t a, uint32_t b, uint32_t c) { return hash32(a * 0x9E3779B1U ^ hash32(b + 0x632BE5ABU) ^ hash32(c * 0x85EBCA77U + 1)); }
// 0..1
inline float hash01(uint32_t a, uint32_t b = 0, uint32_t c = 0) { return (hash3(a, b, c) >> 8) * (1.f / 16777216.f); }
// -1..1
inline float hashBi(uint32_t a, uint32_t b = 0, uint32_t c = 0) { return 2.f * hash01(a, b, c) - 1.f; }

// deterministic white noise, -1..1
struct Noise {
	uint32_t s = 0x12345678;
	void seed(uint32_t v) { s = hash32(v) | 1; }
	float next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return (int32_t) s * (1.f / 2147483648.f); }
};

// one-pole low-pass / high-pass (TPT)
struct OnePole {
	float g = 0.f, s = 0.f;
	void setLP(float fc, float fs) { float w = std::tan(PI_F * clampf(fc / fs, 1e-5f, 0.49f)); g = w / (1.f + w); }
	float lp(float x) { float v = (x - s) * g; float y = v + s; s = y + v; return y; }
	float hp(float x) { return x - lp(x); }
	void reset() { s = 0.f; }
};

// RBJ biquad (direct form 1 transposed)
struct Biquad {
	float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
	void reset() { z1 = z2 = 0; }
	float process(float x) { float y = b0 * x + z1; z1 = b1 * x - a1 * y + z2; z2 = b2 * x - a2 * y; return y; }
	void set(double B0, double B1, double B2, double A0, double A1, double A2) {
		b0 = B0 / A0; b1 = B1 / A0; b2 = B2 / A0; a1 = A1 / A0; a2 = A2 / A0;
	}
	void lowpass(float fc, float q, float fs) {
		double w = 2 * PI_D * std::min(fc, 0.49f * fs) / fs, c = std::cos(w), al = std::sin(w) / (2 * q);
		set((1 - c) / 2, 1 - c, (1 - c) / 2, 1 + al, -2 * c, 1 - al);
	}
	void highpass(float fc, float q, float fs) {
		double w = 2 * PI_D * std::min(fc, 0.49f * fs) / fs, c = std::cos(w), al = std::sin(w) / (2 * q);
		set((1 + c) / 2, -(1 + c), (1 + c) / 2, 1 + al, -2 * c, 1 - al);
	}
	void bandpass(float fc, float q, float fs) {
		double w = 2 * PI_D * std::min(fc, 0.49f * fs) / fs, c = std::cos(w), al = std::sin(w) / (2 * q);
		set(al, 0, -al, 1 + al, -2 * c, 1 - al);
	}
	void peak(float fc, float q, float db, float fs) {
		double A = std::pow(10.0, db / 40.0), w = 2 * PI_D * std::min(fc, 0.49f * fs) / fs, c = std::cos(w), al = std::sin(w) / (2 * q);
		set(1 + al * A, -2 * c, 1 - al * A, 1 + al / A, -2 * c, 1 - al / A);
	}
	void highShelf(float fc, float db, float fs) {
		double A = std::pow(10.0, db / 40.0), w = 2 * PI_D * std::min(fc, 0.49f * fs) / fs, c = std::cos(w), s = std::sin(w);
		double al = s / 2 * std::sqrt(2.0), sa = 2 * std::sqrt(A) * al;
		set(A * ((A + 1) + (A - 1) * c + sa), -2 * A * ((A - 1) + (A + 1) * c), A * ((A + 1) + (A - 1) * c - sa),
			(A + 1) - (A - 1) * c + sa, 2 * ((A - 1) - (A + 1) * c), (A + 1) - (A - 1) * c - sa);
	}
	void lowShelf(float fc, float db, float fs) {
		double A = std::pow(10.0, db / 40.0), w = 2 * PI_D * std::min(fc, 0.49f * fs) / fs, c = std::cos(w), s = std::sin(w);
		double al = s / 2 * std::sqrt(2.0), sa = 2 * std::sqrt(A) * al;
		set(A * ((A + 1) - (A - 1) * c + sa), 2 * A * ((A - 1) - (A + 1) * c), A * ((A + 1) - (A - 1) * c - sa),
			(A + 1) + (A - 1) * c + sa, -2 * ((A - 1) + (A + 1) * c), (A + 1) + (A - 1) * c - sa);
	}
};

// 4-stage OTA ladder low-pass, zero-delay feedback solved linearly, tanh at every
// stage input (spec 5.3). k = feedback (4 = edge of oscillation).
struct Ladder {
	float s[4] = {0, 0, 0, 0};
	float G = 0.f, g = 0.f;
	void reset() { for (float& v : s) v = 0.f; }
	void setCutoff(float fc, float fs) {
		g = std::tan(PI_F * clampf(fc / fs, 1e-5f, 0.45f));
		G = g / (1.f + g);
	}
	// returns the 4-pole output; out2 gets the 2-pole tap
	float process(float x, float k, float* out2 = nullptr) {
		float b = 1.f / (1.f + g);
		float G2 = G * G, G3 = G2 * G, G4 = G2 * G2;
		float S = G3 * b * s[0] + G2 * b * s[1] + G * b * s[2] + b * s[3];
		float u = (x - k * S) / (1.f + k * G4);
		float in = ftanh(u);
		float y = 0.f;
		for (int i = 0; i < 4; i++) {
			float v = (in - s[i]) * G;
			y = v + s[i];
			s[i] = y + v;
			if (i == 1 && out2) *out2 = y;
			in = (i < 3) ? ftanh(y) : y;
		}
		return y;
	}
};

// ADSR with exponential decay/release (time constant = time / 4), slightly convex attack.
struct Adsr {
	enum { IDLE, ATT, DEC, SUS, REL };
	int stage = IDLE;
	float v = 0.f, a = 0.002f, d = 0.1f, sus = 1.f, r = 0.01f;
	float ca = 0, cd = 0, cr = 0;
	bool gateMode = false;
	void set(const float* e, bool gate, float fs, float atkScale, float decScale) {
		gateMode = gate;
		if (gate) { a = 0.002f; d = 0.f; sus = 1.f; r = 0.006f; }
		else { a = e[0]; d = e[1]; sus = e[2]; r = e[3]; }
		a = std::max(a * atkScale, 0.0005f);
		d = std::max(d * decScale, 0.0005f);
		r = std::max(r * decScale, 0.001f);
		ca = 1.f - std::exp(-1.f / (a * fs / 1.6f)); // reaches 1 from 0 towards 1.3 in ~a
		cd = 1.f - std::exp(-4.f / (d * fs));
		cr = 1.f - std::exp(-4.f / (r * fs));
	}
	void gateOn() { stage = ATT; }
	void gateOff() { if (stage != IDLE) stage = REL; }
	float next() {
		switch (stage) {
			case ATT: v += (1.3f - v) * ca; if (v >= 1.f) { v = 1.f; stage = (d > 0.f) ? DEC : SUS; } break;
			case DEC: v += (sus - v) * cd; if (std::fabs(v - sus) < 1e-4f) { v = sus; stage = SUS; } break;
			case SUS: v = sus; break;
			case REL: v -= v * cr; if (v < 1e-5f) { v = 0.f; stage = IDLE; } break;
			default: break;
		}
		return v;
	}
	bool active() const { return stage != IDLE; }
};

// Half-band 2:1 decimator, 47-tap Kaiser windowed sinc (stopband > 90 dB above 0.27 fs_in).
struct Decimator2 {
	static constexpr int N = 47;
	float h[N];
	float buf[2 * N] = {};
	int pos = 0;
	Decimator2() {
		double beta = 8.0, sum = 0;
		auto i0 = [](double x) { double s = 1, t = 1; for (int k = 1; k < 30; k++) { t *= (x / (2 * k)) * (x / (2 * k)); s += t; } return s; };
		for (int i = 0; i < N; i++) {
			double n = i - (N - 1) / 2.0;
			double sinc = (n == 0) ? 0.5 : std::sin(PI_D * 0.5 * n) / (PI_D * n);
			double w = i0(beta * std::sqrt(1 - (2 * i / double(N - 1) - 1) * (2 * i / double(N - 1) - 1))) / i0(beta);
			h[i] = sinc * w; sum += h[i];
		}
		for (int i = 0; i < N; i++) h[i] /= sum;
	}
	void reset() { std::memset(buf, 0, sizeof(buf)); pos = 0; }
	// feed two samples, get one
	float process(float a, float b) {
		buf[pos] = buf[pos + N] = a; pos = (pos + 1) % N;
		buf[pos] = buf[pos + N] = b; pos = (pos + 1) % N;
		const float* p = buf + pos;
		float acc = 0.f;
		for (int i = 0; i < N; i++) acc += h[i] * p[i];
		return acc;
	}
};

// Arbitrary-ratio windowed-sinc resampler (32 taps, Kaiser, 256 phases with linear
// interpolation between phases). Used to bring the FM chip rate to the host rate.
struct Resampler {
	static constexpr int TAPS = 32, PHASES = 256;
	std::vector<float> table; // (PHASES + 1) * TAPS
	std::vector<float> hist;  // input history
	double ratio = 1.0;       // input samples per output sample
	double frac = 0.0;
	int filled = 0;
	void init(double inRate, double outRate) {
		ratio = inRate / outRate;
		double cutoff = 0.5 * std::min(1.0, outRate / inRate) * 0.9; // in cycles per input sample
		table.assign((PHASES + 1) * TAPS, 0.f);
		auto i0 = [](double x) { double s = 1, t = 1; for (int k = 1; k < 30; k++) { t *= (x / (2 * k)) * (x / (2 * k)); s += t; } return s; };
		double beta = 9.0;
		for (int p = 0; p <= PHASES; p++) {
			double sum = 0;
			for (int t = 0; t < TAPS; t++) {
				double x = (t - (TAPS / 2 - 1)) - p / double(PHASES);
				double s = (x == 0) ? 2 * cutoff : std::sin(2 * PI_D * cutoff * x) / (PI_D * x);
				double r = x / (TAPS / 2.0);
				double w = (std::fabs(r) >= 1) ? 0 : i0(beta * std::sqrt(1 - r * r)) / i0(beta);
				table[p * TAPS + t] = s * w; sum += s * w;
			}
			for (int t = 0; t < TAPS; t++) table[p * TAPS + t] /= sum;
		}
		hist.assign(TAPS, 0.f);
		frac = 0.0;
	}
	// usage per output sample: while (needInput()) push(nextInput); y = pull();
	// frac is the output position between hist[TAPS/2-1] and hist[TAPS/2].
	bool needInput() const { return frac >= 1.0; }
	void push(float x) {
		std::memmove(hist.data(), hist.data() + 1, (TAPS - 1) * sizeof(float));
		hist[TAPS - 1] = x;
		frac -= 1.0;
	}
	float pull() {
		double fp = std::min(std::max(frac, 0.0), 1.0 - 1e-9) * PHASES;
		int p = (int) fp; float t = (float) (fp - p);
		const float* a = &table[p * TAPS];
		const float* b = &table[(p + 1) * TAPS];
		float acc = 0.f;
		for (int i = 0; i < TAPS; i++) acc += hist[i] * (a[i] + t * (b[i] - a[i]));
		frac += ratio;
		return acc;
	}
};

// Integral of a piecewise-constant (staircase) table, for box-integrated playback.
// cum[i] = sum of tab[0..i-1]. Integral from 0 to x (x in samples, 0..len) is
// cum[floor x] + frac * tab[floor x].
struct Stair {
	const float* tab = nullptr;
	const double* cum = nullptr;
	int len = 0;
	double total = 0;
	inline double integ(double x) const {
		// x may be any value >= 0; wraps
		double w = std::floor(x / len);
		double r = x - w * len;
		int i = (int) r; if (i >= len) i = len - 1;
		return w * total + cum[i] + (r - i) * tab[i];
	}
	// average of the staircase over [x0, x0 + dx)
	inline float avg(double x0, double dx) const {
		if (dx < 1e-9) { int i = (int) x0 % len; return tab[i]; }
		return (float) ((integ(x0 + dx) - integ(x0)) / dx);
	}
};

} // namespace hk
