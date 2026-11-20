// Heathrow sound engine: a phaser and a flanger running side by side,
// MORPH crossfades between them. No Rack code in here, so the offline test
// (test/render.cpp) can use it on its own.
//
//   phaser  = a chain of 2nd-order all-pass filters (TPT state-variable form,
//             safe under fast and audio-rate sweeps). Each section adds one
//             notch. RES sets their Q (0.5 = classic 1st-order phaser sound).
//   flanger = a short modulated delay (cubic interpolation).
//   through-zero (TZ) = the dry signal is also delayed by a fixed amount, so
//             the moving delay can pass "through" it: the jet sound of two
//             tape machines.
//   feedback = bipolar, soft-clipped so it can't blow up.
#pragma once
#include <cmath>
#include <cstring>
#include <cstdint>
#include <algorithm>
#include <complex>

namespace heathrow {

static const int MAX_SECTIONS = 12;      // 24 "stages" on the panel
static const int DELAY_SIZE = 16384;     // power of two, > 40 ms at 192 kHz
static const float TZ_REF_MS = 3.f;      // fixed dry delay in through-zero mode
static const float PI_F = 3.14159265358979f;

// feedback saturation for Rack levels: nearly clean up to about +-6 V,
// flattens out at +-15 V so heavy feedback can't run away
inline float softClip(float v) {
	float x = v * 0.1f;
	if (x > 3.f) x = 1.5f;
	else if (x < -3.f) x = -1.5f;
	else x = x - x * x * x / 18.f;
	return x * 10.f;
}

// sweep position 0..1 -> phaser centre frequency (Hz): 40 Hz .. 16 kHz
inline float phaserHz(float s) { return 40.f * std::pow(400.f, s); }
// sweep position 0..1 -> flanger delay (ms): 15 ms .. 0.15 ms (higher = shorter)
inline float flangerMs(float s) { return 15.f * std::pow(0.01f, s); }
// through-zero: 0..1 -> 2*ref .. 0 ms; 0.5 is the zero point (wet = dry)
inline float flangerTzMs(float s) { return 2.f * TZ_REF_MS * (1.f - s); }
// RES 0..1 -> all-pass Q 0.5 .. 6
inline float resToQ(float r) { return 0.5f * std::pow(12.f, r); }

struct Settings {
	float morph = 0.f;      // 0 phaser .. 1 flanger
	int sections = 4;       // 1..12 (stages = 2x)
	float q = 0.5f;
	float feedback = 0.f;   // -0.97 .. 0.97
	float mix = 0.5f;       // -1 .. 1 ; 0 dry, +-0.5 classic 50/50, +-1 wet only
	bool tz = false;
};

// MIX knob -> dry and wet gains. Centre = dry, half-way = 50/50 (deepest
// notches), ends = wet only. Negative side flips the wet signal.
inline void mixGains(float mix, float& dg, float& wg) {
	float a = std::min(std::fabs(mix), 1.f);
	float d = a <= 0.5f ? 1.f : 2.f * (1.f - a);
	float w = std::min(2.f * a, 1.f);
	float n = 1.f / (d + w);
	dg = d * n;
	wg = (mix < 0.f ? -w : w) * n;
}

struct Channel {
	// phaser state
	float ic1[MAX_SECTIONS] = {}, ic2[MAX_SECTIONS] = {};
	float phLast = 0.f;
	// flanger state
	float buf[DELAY_SIZE] = {};
	float dryBuf[DELAY_SIZE] = {};
	int w = 0;
	float flLast = 0.f;

	void reset() {
		std::memset(ic1, 0, sizeof(ic1));
		std::memset(ic2, 0, sizeof(ic2));
		std::memset(buf, 0, sizeof(buf));
		std::memset(dryBuf, 0, sizeof(dryBuf));
		phLast = flLast = 0.f;
	}

	static float readFrac(const float* b, int w, float d) {
		// d samples back from the newest sample (index w)
		if (d < 0.f) d = 0.f;
		int i = (int) d;
		float f = d - i;
		auto at = [&](int k) { return b[(w - k) & (DELAY_SIZE - 1)]; };
		if (i < 1) {
			// too close to "now" for cubic: linear
			return at(0) + (at(1) - at(0)) * d;
		}
		float y0 = at(i - 1), y1 = at(i), y2 = at(i + 1), y3 = at(i + 2);
		// Hermite
		float c1 = 0.5f * (y2 - y0);
		float c2 = y0 - 2.5f * y1 + 2.f * y2 - 0.5f * y3;
		float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
		return ((c3 * f + c2) * f + c1) * f + y1;
	}

	// x = input, s = sweep position 0..1. Returns the mixed output.
	float process(float x, float s, const Settings& st, float sr) {
		s = std::min(std::max(s, 0.f), 1.f);
		w = (w + 1) & (DELAY_SIZE - 1);
		dryBuf[w] = x;
		float dry = st.tz ? readFrac(dryBuf, w, TZ_REF_MS * 0.001f * sr) : x;

		float wet = 0.f;
		float m = std::min(std::max(st.morph, 0.f), 1.f);

		// ---- phaser (skipped when fully flanger)
		if (m < 0.999f) {
			float fc = std::min(phaserHz(s), 0.45f * sr);
			float g = std::tan(PI_F * fc / sr);
			float k = 1.f / st.q;
			float a1 = 1.f / (1.f + g * (g + k));
			float v = dry + st.feedback * softClip(phLast);
			int n = std::min(std::max(st.sections, 1), MAX_SECTIONS);
			for (int i = 0; i < n; i++) {
				// TPT state-variable filter (Simper); all-pass = in - 2k*band
				float v3 = v - ic2[i];
				float bp = a1 * (ic1[i] + g * v3);
				float lp = ic2[i] + g * bp;
				ic1[i] = 2.f * bp - ic1[i];
				ic2[i] = 2.f * lp - ic2[i];
				v = v - 2.f * k * bp;
			}
			phLast = v;
			wet += (1.f - m) * v;
		}

		// ---- flanger (always runs so its delay line stays filled)
		{
			float ms = st.tz ? flangerTzMs(s) : flangerMs(s);
			float d = ms * 0.001f * sr;
			// the delay line always takes the raw input; in TZ mode the dry
			// reference is itself delayed by TZ_REF_MS, so at s = 0.5 they line up
			buf[w] = x + st.feedback * softClip(flLast);
			float y = readFrac(buf, w, d);
			flLast = y;
			if (m > 0.001f) wet += m * y;
		}

		float dg, wg;
		mixGains(st.mix, dg, wg);
		return dg * dry + wg * wet;
	}
};

// ---------------------------------------------------------------------------
// Modulation: LFO with six blended shapes, stepped sweeps, clock sync.

struct Rng {
	uint32_t s = 0x1234567u;
	float next() {  // -1..1
		s ^= s << 13; s ^= s >> 17; s ^= s << 5;
		return (s >> 8) * (2.f / 16777216.f) - 1.f;
	}
};

static const int NUM_SHAPES = 6;
static const char* const SHAPE_NAMES[NUM_SHAPES] = {"sine", "triangle", "ramp", "square", "drift", "S&H"};

// cycles of the LFO per clock pulse, chosen by RATE in clock mode
static const int NUM_RATIOS = 13;
static const float RATIO_CYC[NUM_RATIOS] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 3, 4};
static const int RATIO_CLK[NUM_RATIOS]   = {64, 32, 16, 12, 8, 6, 4, 3, 2, 1, 1, 1, 1};

struct Lfo {
	double phase = 0.0;      // 0..1
	float v0 = 0.f, v1 = 0.f, v2 = 0.f;  // random values: this cycle, next, after
	Rng rng;

	void reset() { phase = 0.0; }

	void advance(double inc) {
		phase += inc;
		while (phase >= 1.0) {
			phase -= 1.0;
			v0 = v1; v1 = v2; v2 = rng.next();
		}
		while (phase < 0.0) phase += 1.0;
	}

	static float basic(int shape, float p, float a, float b) {
		switch (shape) {
			case 0: return std::sin(2.f * PI_F * p);
			case 1: return p < 0.25f ? 4.f * p : (p < 0.75f ? 2.f - 4.f * p : 4.f * p - 4.f);
			case 2: return 2.f * p - 1.f;
			case 3: return p < 0.5f ? 1.f : -1.f;
			case 4: { float t = 0.5f - 0.5f * std::cos(PI_F * p); return a + (b - a) * t; }
			default: return a;
		}
	}

	// value -1..1 at phase offset `off` (0..1) with SHAPE 0..5 (blended),
	// and STEPS (0 = smooth, else number of steps per cycle)
	float value(float off, float shape, int steps) const {
		float p = (float) phase + off;
		float a = v0, b = v1;
		if (p >= 1.f) { p -= 1.f; a = v1; b = v2; }
		if (steps > 0) p = std::floor(p * steps) / steps;
		shape = std::min(std::max(shape, 0.f), (float) (NUM_SHAPES - 1));
		int i = (int) shape;
		float f = shape - i;
		float y = basic(i, p, a, b);
		if (f > 0.f && i + 1 < NUM_SHAPES) y += (basic(i + 1, p, a, b) - y) * f;
		return y;
	}
};

// Envelope follower for ENV (0..~1 for a 5 V signal)
struct Follower {
	float env = 0.f;
	float process(float x, float sr) {
		float a = std::fabs(x) * 0.2f;
		float t = a > env ? 0.005f : 0.15f;
		env += (a - env) * (1.f - std::exp(-1.f / (t * sr)));
		return env;
	}
};

// ---------------------------------------------------------------------------
// Response for the screen: complex gain of the whole effect at frequency hz.
inline float responseDb(float hz, float s, const Settings& st, float sr) {
	using C = std::complex<float>;
	float wn = 2.f * PI_F * hz / sr;
	C z1 = std::polar(1.f, -wn);  // z^-1
	C dry = st.tz ? std::polar(1.f, -wn * TZ_REF_MS * 0.001f * sr) : C(1.f, 0.f);
	float m = std::min(std::max(st.morph, 0.f), 1.f);
	C wet(0.f, 0.f);
	if (m < 0.999f) {
		// analog all-pass (s^2 - k s + 1)/(s^2 + k s + 1), bilinear with prewarp
		float fc = std::min(phaserHz(s), 0.45f * sr);
		float g = std::tan(PI_F * fc / sr);
		float k = 1.f / st.q;
		C sp = (C(1.f) - z1) / (C(1.f) + z1) / g;
		C ap = (sp * sp - k * sp + C(1.f)) / (sp * sp + k * sp + C(1.f));
		int n = std::min(std::max(st.sections, 1), MAX_SECTIONS);
		C A = std::pow(ap, (float) n);
		C H = dry * A / (C(1.f) - st.feedback * A * z1);
		wet += (1.f - m) * H;
	}
	if (m > 0.001f) {
		float ms = st.tz ? flangerTzMs(s) : flangerMs(s);
		float d = ms * 0.001f * sr;
		C D = std::polar(1.f, -wn * d);
		C H = D / (C(1.f) - st.feedback * D * z1);
		wet += m * H;
	}
	float dg, wg;
	mixGains(st.mix, dg, wg);
	C out = dg * dry + wg * wet;
	return 20.f * std::log10(std::abs(out) + 1e-5f);
}

}  // namespace heathrow
