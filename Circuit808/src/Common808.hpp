// Shared circuit parts for the 808 Circuits voices (Rack-free).
// Each voice is a small circuit model built from these: the 808's trigger pulse and
// pulse shaper, bridged-T resonators, filters, noise, and the AGE part tolerances.
#pragma once
#include <cmath>
#include <cstdint>
#include <algorithm>

namespace c808 {

static const float PI = 3.14159265f;
static inline float clampf(float x, float a, float b) { return std::min(std::max(x, a), b); }
static inline float expMap(float k, float lo, float hi) { return lo * std::pow(hi / lo, clampf(k, 0.f, 1.f)); }

struct Rng {
	uint32_t s = 1u;
	void seed(uint32_t v) { s = v ? v : 1u; }
	uint32_t next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
	float bi() { return (next() & 0xffffff) / float(0x7fffff) - 1.f; }   // -1..1
	float uni() { return (next() & 0xffffff) / float(0xffffff); }        // 0..1
};

// AGE: the 808 used +-20 % capacitors and +-5 % resistors, so every unit differed.
// A serial number fixes where this unit's parts sit in those tolerances; AGE scales how
// far off they are. Each hit also drifts a little; past 0.7 the capacitors are worn
// (shorter ring and a little hiss). AGE 0 = exactly the stock unit.
struct Age {
	float u[8] = {0, 0, 0, 0, 0, 0, 0, 0};
	Rng rng;
	void setSerial(uint32_t serial) {
		rng.seed(serial * 2654435761u + 12345u);
		for (int i = 0; i < 8; i++) u[i] = rng.bi();
	}
	// unit offset i scaled by AGE and a spread (e.g. 0.12 = +-12 % at full AGE)
	float part(int i, float age, float spread) const { return 1.f + spread * age * u[i & 7]; }
	float drift(float age, float spread) { return 1.f + spread * age * rng.bi(); }
	static float worn(float age) { return clampf((age - 0.7f) / 0.3f, 0.f, 1.f); }
	float hiss(float age) { float w = worn(age); return w > 0.f ? w * 0.006f * rng.bi() : 0.f; }
};

// The 808 trigger: a pulse of A volts (4 V, up to 14 V accented) and width w seconds,
// through the low-shelf pulse shaper (R/R/C) with a diode clamping the negative edge.
struct PulseShaper {
	float lp = 0.f, t = 1e9f, A = 0.f, w = 0.001f;
	void fire(float amp, float width) { A = amp; w = width; t = 0.f; }
	float process(float dt) {
		float v = (t < w) ? A : 0.f;
		t += dt;
		lp += (v - lp) * (1.f - std::exp(-dt / 0.25e-3f));
		float x = v - 0.7f * lp;
		if (x < 0.f) x = -0.71f * std::tanh(-x / 0.71f);
		return x;
	}
};

// Bridged-T resonator as a trapezoidal state-variable filter. Excite with the shaped
// pulse; ring() returns the band-pass ring. T60 sets the ring time; T60 <= 0 gives a
// slightly negative damping that a soft limiter holds (sustain).
struct Resonator {
	float ic1 = 0.f, ic2 = 0.f, bp = 0.f, lp = 0.f;
	void reset() { ic1 = ic2 = bp = lp = 0.f; }
	float amp() const { return std::sqrt(bp * bp + lp * lp); }
	float process(float u, float hz, float t60, float fs, float wRef = 0.f) {
		float w = 2.f * PI * clampf(hz, 5.f, 0.45f * fs);
		float k;
		if (t60 > 0.f) k = 13.8155f / (w * t60);
		else k = t60 + 0.08f * (bp * bp + lp * lp) / 1.7f;
		k = clampf(k, -0.2f, 2.f);
		float g = std::tan(0.5f * w / fs);
		// constant excitation level across tunings (relative to wRef when given)
		if (wRef > 0.f) u *= wRef / w;
		float a1 = 1.f / (1.f + g * (g + k)), a2 = g * a1, a3 = g * a2;
		float v3 = u - ic2;
		float v1 = a1 * ic1 + a2 * v3;
		float v2 = ic2 + a2 * ic1 + a3 * v3;
		ic1 = 2.f * v1 - ic1; ic2 = 2.f * v2 - ic2;
		bp = v1; lp = v2;
		if (!std::isfinite(bp) || !std::isfinite(lp)) reset();
		return bp;
	}
};

// Plain state-variable filter (low / band / high), TPT.
struct Svf {
	float ic1 = 0.f, ic2 = 0.f, lp = 0.f, bp = 0.f, hp = 0.f;
	void process(float x, float hz, float q, float fs) {
		float g = std::tan(PI * clampf(hz, 10.f, 0.45f * fs) / fs);
		float k = 1.f / std::max(q, 0.3f);
		float a1 = 1.f / (1.f + g * (g + k)), a2 = g * a1, a3 = g * a2;
		float v3 = x - ic2;
		float v1 = a1 * ic1 + a2 * v3;
		float v2 = ic2 + a2 * ic1 + a3 * v3;
		ic1 = 2.f * v1 - ic1; ic2 = 2.f * v2 - ic2;
		lp = v2; bp = v1; hp = x - k * v1 - v2;
		if (!std::isfinite(lp)) { ic1 = ic2 = lp = bp = hp = 0.f; }
	}
};

// One-pole TPT low-pass; hp(x) = x - lp.
struct OnePole {
	float z = 0.f;
	float lp(float x, float hz, float fs) {
		float g = std::tan(PI * clampf(hz, 1.f, 0.45f * fs) / fs);
		float v = (x - z) * g / (1.f + g);
		float y = v + z; z = y + v; return y;
	}
	float hp(float x, float hz, float fs) { return x - lp(x, hz, fs); }
};

// Paul Kellet's economy pink filter on white noise
struct Pink {
	float b0 = 0.f, b1 = 0.f, b2 = 0.f;
	float process(float w) {
		b0 = 0.99765f * b0 + w * 0.0990460f;
		b1 = 0.96300f * b1 + w * 0.2965164f;
		b2 = 0.57000f * b2 + w * 1.0526913f;
		return (b0 + b1 + b2 + w * 0.1848f) * 0.25f;
	}
};

// The output op-amp: DRIVE pushes into the rails (about +-10.5 V)
static inline float opamp(float x, float drive) {
	float pre = 1.f + 11.f * drive * drive;
	return 10.5f * std::tanh(x * pre / 10.5f) / (1.f + 0.6f * drive);
}

// Accent: trigger height 4 V, up to 14 V when accented (VR3)
static inline float triggerVolts(float accentKnob, float accentIn) {
	return 4.f + 10.f * clampf(accentKnob, 0.f, 1.f) * clampf(accentIn, 0.f, 1.f);
}

// exponential decay with a given T60
static inline float decayMul(float t60, float dt) { return t60 > 0.f ? std::exp(-6.9078f * dt / t60) : 1.f; }

}  // namespace c808
