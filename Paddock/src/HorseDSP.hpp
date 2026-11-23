// Shared engines for the Paddock plugin. No Rack headers, so test/render.cpp can use it.
//
//   Unit   - one "physical unit": a seeded random number generator that gives every
//            module instance its own small part tolerances, plus slow thermal drift.
//   Horse  - a bounce oscillator. It rises at one slope until it reaches its upper
//            bound, turns, falls at another slope to its lower bound, turns again.
//            The bounds can move (CV, or other horses), so the pitch follows the gap.
//            If the lower bound rises above the upper one the comparator fights itself
//            and the horse "hyper-oscillates" between them (the paradox wave).
//   Rolz   - a rhythm multivibrator: charges, flips its output, charges again.
//            Neighbours push its charging current through "touch" coupling.
//   Gong   - a struck three-mode resonator for Tumble's voices.
#pragma once
#include <cmath>
#include <cstdint>
#include <algorithm>

namespace paddock {

struct Rng {
	uint64_t s = 0x9E3779B97F4A7C15ull;
	void seed(uint64_t v) { s = v ? v : 0x9E3779B97F4A7C15ull; for (int i = 0; i < 4; i++) next(); }
	uint64_t next() {   // xorshift64*
		s ^= s >> 12; s ^= s << 25; s ^= s >> 27;
		return s * 2685821657736338717ull;
	}
	float uni() { return (float) ((next() >> 40) * (1.0 / 16777216.0)); }   // 0..1
	float bi() { return uni() * 2.f - 1.f; }                                  // -1..1
};

// Slow wander: low-passed noise, roughly +-1 over a few seconds.
struct Drift {
	float y = 0.f, z = 0.f;
	float step(Rng& r, float dt, float speed = 0.35f) {
		float a = std::min(1.f, dt * speed * 6.2832f);
		z += a * (r.bi() * 1.7f - z);
		y += a * (z - y);
		return y;
	}
};

// Frequency of a knob (0..1) in the three ranges, before CV. Matches the panel marks.
enum Range { RANGE_CV = 0, RANGE_LOW = 1, RANGE_AUDIO = 2 };
inline float rangeHz(int range, float knob) {
	switch (range) {
		case RANGE_CV: return 0.005f * std::pow(4096.f, knob);   // 0.005 .. 20 Hz
		case RANGE_LOW: return 0.5f * std::pow(256.f, knob);     // 0.5 .. 128 Hz
		default: return 16.f * std::pow(512.f, knob);            // 16 Hz .. 8 kHz
	}
}

struct Horse {
	float v = 0.f;        // position (volts)
	int dir = 1;          // +1 rising, -1 falling, 0 parked (Swerve one-shot)
	bool paradox = false; // lower bound above upper bound right now

	// per-unit tolerances (set by Paddock modules from their Unit seed)
	float tolUp = 0.f, tolDown = 0.f;   // slope error, fraction
	float tolHi = 0.f, tolLo = 0.f;     // comparator offset, volts
	Drift drift;

	void roll(Rng& r) {
		tolUp = r.bi() * 0.025f;  tolDown = r.bi() * 0.025f;
		tolHi = r.bi() * 0.018f;  tolLo = r.bi() * 0.018f;
	}

	// One step. up/down are slopes in V/s (> 0). age scales tolerances (0 = ideal).
	// Returns +1 when it turned at the top, -1 at the bottom, 0 otherwise.
	int step(float up, float down, float lo, float hi, float dt, float age, float wander) {
		up *= 1.f + age * (tolUp + 0.004f * wander);
		down *= 1.f + age * (tolDown - 0.004f * wander);
		hi += age * tolHi;
		lo += age * tolLo;
		paradox = lo > hi;
		if (paradox) {
			// The two comparators disagree: the horse is thrown back and forth across the
			// overlap, faster the deeper the overlap. Bounds swap so it stays inside them.
			float over = lo - hi;
			float hyper = std::min(64.f, 1.f + 24.f * over);
			std::swap(lo, hi);
			up *= hyper; down *= hyper;
		}
		up = std::max(up, 1e-4f); down = std::max(down, 1e-4f);
		int ev = 0;
		if (dir > 0) {
			v += up * dt;
			if (v >= hi) {
				float ex = v - hi;
				v = std::max(lo, hi - ex * down / up);
				dir = -1; ev = 1;
			}
		} else if (dir < 0) {
			v -= down * dt;
			if (v <= lo) {
				float ex = lo - v;
				v = std::min(hi, lo + ex * up / down);
				dir = 1; ev = -1;
			}
		}
		if (v > 12.f) v = 12.f;
		if (v < -12.f) v = -12.f;
		return ev;
	}
};

// Rhythm core for Tumble.
struct Rolz {
	float p = 0.f;     // charge 0..thr
	int s = -1;        // output state
	float tolThr = 0.f, tolRate = 0.f;
	Drift drift;
	void roll(Rng& r) { tolThr = r.bi() * 0.04f; tolRate = r.bi() * 0.03f; }
};

// Three-mode struck resonator ("gong").
struct Gong {
	float y1[3] = {}, y2[3] = {};
	float a1[3] = {}, a2[3] = {}, g[3] = {};
	float exc = 0.f, excDecay = 0.f, vel = 0.f;
	float gain = 0.6f;  // level (checked in test/render.cpp)
	void strike(float velocity, float sr) {
		vel = velocity; exc = 1.f;
		excDecay = std::exp(-1.f / (0.0015f * sr));   // 1.5 ms noise click
	}
	void tune(float f0, float tone, float t60, float sr) {
		const float ratio[3] = {1.f, 2.f + 0.76f * tone, 3.f + 2.4f * tone};
		const float amp[3] = {1.f, 0.55f, 0.12f + 0.3f * tone};
		for (int m = 0; m < 3; m++) {
			float f = std::min(f0 * ratio[m], sr * 0.45f);
			float w = 2.f * 3.14159265f * f / sr;
			float t = t60 / std::sqrt(ratio[m]);
			float r = std::exp(-6.9078f / (t * sr));
			a1[m] = 2.f * r * std::cos(w);
			a2[m] = -r * r;
			g[m] = amp[m] * std::sin(w) * gain;
		}
	}
	float process(Rng& rng) {
		float x = 0.f;
		if (exc > 1e-4f) { x = rng.bi() * exc * vel; exc *= excDecay; }
		float out = 0.f;
		for (int m = 0; m < 3; m++) {
			float y = g[m] * x + a1[m] * y1[m] + a2[m] * y2[m];
			y2[m] = y1[m]; y1[m] = y;
			out += y;
		}
		return out;
	}
};

inline float softClip(float x) { return 5.f * std::tanh(x / 5.f); }


static const int OS = 8;   // oversampling for the bounce oscillators

// ---------------------------------------------------------------- Swerve
struct SwerveEngine {
	Horse h;
	Rng rng;
	float wander = 0.f;
	// out: OS samples of position and direction (+-5 V square)
	float tri[OS], sqr[OS];
	int events = 0;   // bit 1 = turned at top, bit 2 = turned at bottom (this block)

	void process(float upHz, float downHz, float lo, float hi, bool once, float age, float sampleTime) {
		const float span = 10.f;   // reference span: +-5 V bounds give the marked pitch
		float dt = sampleTime / OS;
		wander = h.drift.step(rng, sampleTime);
		events = 0;
		for (int k = 0; k < OS; k++) {
			int ev = h.step(2.f * span * upHz, 2.f * span * downHz, lo, hi, dt, age, wander);
			if (ev > 0) events |= 1;
			if (ev < 0) {
				events |= 2;
				if (once) { h.dir = 0; h.v = std::min(lo, hi); }
			}
			if (h.dir == 0) h.v = std::min(lo, hi);   // parked: follow the lower bound
			tri[k] = h.v;
			sqr[k] = h.dir > 0 ? 5.f : -5.f;
		}
	}
	void trigUp() { h.dir = 1; }
	void trigDown() { h.dir = -1; }
};

// ---------------------------------------------------------------- Gallop
struct GallopEngine {
	static const int N = 4;
	Horse h[N];
	Rng rng;
	float wander[N] = {};
	float tri[N][OS], sqr[N][OS];
	bool paradox[N] = {};

	GallopEngine() {
		for (int i = 0; i < N; i++) { h[i].v = 3.f - 2.f * i; h[i].dir = (i % 2) ? -1 : 1; }
	}
	// hz/tilt per horse; hiIn/loIn: NAN = use neighbour; rail = top/bottom bound (V)
	void process(const float* hz, const float* tilt, const float* hiIn, const float* loIn,
	             float rail, float age, float sampleTime) {
		const float span = 2.5f;   // a horse usually has about a quarter of the stack
		float dt = sampleTime / OS;
		float up[N], down[N];
		for (int i = 0; i < N; i++) {
			wander[i] = h[i].drift.step(rng, sampleTime, 0.25f + 0.07f * i);
			float p = std::min(0.98f, std::max(0.02f, 0.5f * (1.f + tilt[i])));
			float base = 2.f * span * hz[i];
			up[i] = base / (2.f * p);
			down[i] = base / (2.f * (1.f - p));
		}
		for (int k = 0; k < OS; k++) {
			float pos[N];
			for (int i = 0; i < N; i++) pos[i] = h[i].v;   // everyone reads the same moment
			for (int i = 0; i < N; i++) {
				float hi = std::isnan(hiIn[i]) ? (i == 0 ? rail : pos[i - 1]) : hiIn[i];
				float lo = std::isnan(loIn[i]) ? (i == N - 1 ? -rail : pos[i + 1]) : loIn[i];
				h[i].step(up[i], down[i], lo, hi, dt, age, wander[i]);
				tri[i][k] = h[i].v;
				sqr[i][k] = h[i].dir > 0 ? 5.f : -5.f;
			}
		}
		for (int i = 0; i < N; i++) paradox[i] = h[i].paradox;
	}
};

// ---------------------------------------------------------------- Tumble
struct TumbleEngine {
	static const int N = 5;
	Rolz r[N];
	Gong gong[N];
	Rng rng;
	float skin[N] = {};      // jitter on each touch (a finger is never still)
	bool fired[N] = {};
	float outL = 0.f, outR = 0.f;
	int tuneDiv = 0;

	TumbleEngine() { for (int i = 0; i < N; i++) r[i].p = 0.2f * i; }

	void sync() {
		for (int i = 0; i < N; i++) { r[i].s = -1; r[i].p = 10.f; }   // flips (and strikes) on the next step
	}
	void roll() { for (int i = 0; i < N; i++) r[i].roll(rng); }

	// hz: rolz rate; touch[i] couples rolz i and i+1 (ring), -1..1;
	// pitchHz, spread (semitones per rolz), tone 0..1, decay (s), age
	void process(const float* hz, const float* touch, float pitchHz, float spread, float tone,
	             float decay, float age, float sr) {
		float dt = 1.f / sr;
		if (++tuneDiv >= 32) {
			tuneDiv = 0;
			for (int i = 0; i < N; i++)
				gong[i].tune(pitchHz * std::pow(2.f, spread * i / 12.f), tone, decay, sr);
		}
		float sNow[N];
		for (int i = 0; i < N; i++) sNow[i] = (float) r[i].s;
		for (int i = 0; i < N; i++) {
			float w = r[i].drift.step(rng, dt, 0.4f);
			skin[i] += std::min(1.f, dt * 30.f) * (rng.bi() - skin[i]);
			// touches on both sides of this rolz: i-1..i and i..i+1
			float tl = touch[(i + N - 1) % N], tr = touch[i];
			int jl = (i + N - 1) % N, jr = (i + 1) % N;
			float c = tl * sNow[jl] + tr * sNow[jr];
			c *= 0.6f;
			c += (std::fabs(tl) + std::fabs(tr)) * 0.25f * skin[i];
			float rate = 2.f * hz[i] * (1.f + age * (r[i].tolRate + 0.01f * w)) * std::max(0.05f, 1.f + c);
			r[i].p += rate * dt;
			fired[i] = false;
			float thr = 1.f + age * r[i].tolThr;
			if (r[i].p >= thr) {
				r[i].p -= thr;
				if (r[i].p > thr) r[i].p = 0.f;
				r[i].s = -r[i].s;
				if (r[i].s > 0) {
					fired[i] = true;
					gong[i].strike(0.75f + 0.25f * rng.uni(), sr);
				}
				// kick the neighbours: positive touch pulls them towards flipping with us,
				// negative pushes them away (they lurch, then drift back)
				r[jl].p = std::max(0.f, r[jl].p + 0.12f * tl);
				r[jr].p = std::max(0.f, r[jr].p + 0.12f * tr);
			}
		}
		float L = 0.f, R = 0.f;
		for (int i = 0; i < N; i++) {
			float y = gong[i].process(rng);
			float pan = i / (float) (N - 1);
			L += y * std::sqrt(1.f - pan);
			R += y * std::sqrt(pan);
		}
		outL = softClip(L);
		outR = softClip(R);
	}
};

} // namespace paddock
