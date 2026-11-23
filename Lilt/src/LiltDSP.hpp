// Lilt engine: a relabi generator (John Berndt's idea).
//
// Four LFOs run at ratios of a felt PULSE (beats per minute). Their weighted
// sum wanders above and below four thresholds; each crossing is a trigger.
// Because the LFOs are near (but not exactly at) simple ratios of the pulse,
// the triggers lean towards the beat, fall away from it and come back.
//
//   DRIFT   pulls each LFO off its ratio by its own small amount (0-15 %)
//   MUTATE  slowly random-walks those amounts, nudges phases and levels
//   GRID    pulls triggers onto the nearest 1/4-pulse tick (0 = free)
//
// Plain C++ with no Rack code, so test/render.cpp can run it offline.
#pragma once
#include <cmath>
#include <cstdint>
#include <algorithm>

namespace lilt {

static const int NUM_LFO = 4;
static const int NUM_CH = 4;
static const int GRID_DIV = 4;  // GRID ticks per pulse
static const float TWO_PI = 6.28318530718f;

static const int NUM_SNAP = 18;
static const float SNAP_RATIOS[NUM_SNAP] = {1.f / 8, 1.f / 6, 1.f / 4, 1.f / 3, 3.f / 8, 1.f / 2, 2.f / 3, 3.f / 4, 1.f,
                                            5.f / 4, 4.f / 3, 3.f / 2, 2.f, 5.f / 2, 3.f, 4.f, 6.f, 8.f};
static const char* const SNAP_NAMES[NUM_SNAP] = {"1/8", "1/6", "1/4", "1/3", "3/8", "1/2", "2/3", "3/4", "1",
                                                 "5/4", "4/3", "3/2", "2",   "5/2", "3",   "4",   "6",   "8"};

// Nearest musical ratio to 2^log2r.
inline int snapIndex(float log2r) {
	int best = 0;
	float bestD = 1e9f;
	for (int i = 0; i < NUM_SNAP; i++) {
		float d = std::fabs(std::log2(SNAP_RATIOS[i]) - log2r);
		if (d < bestD) { bestD = d; best = i; }
	}
	return best;
}

struct Rng {
	uint32_t s = 0x9E3779B9u;
	uint32_t next() {
		s ^= s << 13; s ^= s >> 17; s ^= s << 5;
		return s;
	}
	float uni() { return (next() >> 8) * (1.f / 16777216.f); }  // 0..1
	float bi() { return uni() * 2.f - 1.f; }                      // -1..1
};

// Everything the panel sets, already combined with CV.
struct RelabiControls {
	float bpm = 120.f;           // the felt pulse
	float speed = 1.f;           // extra factor on the LFOs only (RATE CV when clocked)
	float ratioLog2[NUM_LFO] = {0.f, -0.415f, 0.585f, -1.585f};
	bool snap = true;
	float level[NUM_LFO] = {1.f, 0.8f, 0.6f, 0.5f};  // -1..1, negative flips the LFO
	float drift = 0.2f;          // 0..1
	float mutate = 0.f;          // 0..1
	bool freeze = false;         // holds MUTATE where it is
	float shape = 0.f;           // 0 sine, 0.5 triangle, 1 soft square
	float thresh = 0.f;          // -1..1 (centre of the four thresholds)
	float spread = 0.5f;         // 0..1 (how far apart they are)
	int dir = 0;                 // 0 rising, 1 both (gate toggles), 2 falling
	float grid = 0.f;            // 0..1
};

struct RelabiFrame {
	float lfo[NUM_LFO] = {};     // -1..1, before LEVEL
	float sum = 0.f;             // -1..1
	float thr[NUM_CH] = {};
	bool trig[NUM_CH] = {};      // fires this sample
	bool gate[NUM_CH] = {};
	bool pulse = false;          // pulse beat this sample
	float acc = 0.f;             // 0..1, how steep the sum was at the last trigger
	int lastCh = -1;
};

inline float waveShape(float ph, float shape) {
	float s = std::sin(TWO_PI * ph);
	float p = ph + 0.25f;
	p -= std::floor(p);
	float tri = 1.f - 4.f * std::fabs(p - 0.5f);
	if (shape <= 0.5f) {
		float t = shape * 2.f;
		return s + (tri - s) * t;
	}
	float t = (shape - 0.5f) * 2.f;
	float sq = std::tanh(5.f * s) / std::tanh(5.f);
	return tri + (sq - tri) * t;
}

struct RelabiEngine {
	float phase[NUM_LFO] = {};
	float detune[NUM_LFO] = {0.31f, -0.57f, 0.79f, -0.23f};  // each LFO's own pull, -1..1 (DRIFT scales it)
	float wobble[NUM_LFO] = {};                                // level wobble from MUTATE
	float beat = 0.f;                                          // pulse phase 0..1
	bool above[NUM_CH] = {};
	bool pending[NUM_CH] = {};
	float pendingAcc[NUM_CH] = {};
	bool gateOn[NUM_CH] = {};
	float gateHold[NUM_CH] = {};
	bool toggle[NUM_CH] = {};
	float prevSum = 0.f;
	float acc = 0.f;
	int lastCh = -1;
	int mutCount = 0;
	bool primed = false;
	Rng rng;

	void seed(uint32_t s) {
		rng.s = s ? s : 1u;
		for (int i = 0; i < NUM_LFO; i++) detune[i] = rng.bi();
	}

	// RESET: everything back to the one.
	void reset() {
		for (int i = 0; i < NUM_LFO; i++) phase[i] = 0.f;
		beat = 0.f;
		for (int k = 0; k < NUM_CH; k++) pending[k] = false;
	}

	// Clocked mode: put the pulse phase where the clock says it is.
	// Returns true if this lands on a beat that the free-running phase hadn't reached yet.
	bool syncBeat(float frac) {
		bool fire = false;
		if (frac == 0.f && beat > 0.5f) fire = true;
		beat = frac;
		return fire;
	}

	float ratioOf(const RelabiControls& c, int i) const {
		float r = c.snap ? SNAP_RATIOS[snapIndex(c.ratioLog2[i])] : std::pow(2.f, c.ratioLog2[i]);
		return r * (1.f + 0.15f * c.drift * detune[i]);
	}

	void process(float dt, const RelabiControls& c, bool beatFromClock, RelabiFrame& out) {
		const float bps = std::max(c.bpm, 1.f) / 60.f;

		// ---- MUTATE (control rate, every 32 samples)
		if (++mutCount >= 32) {
			mutCount = 0;
			if (!c.freeze && c.mutate > 0.f) {
				float cdt = dt * 32.f;
				float sq = std::sqrt(cdt);
				float m2 = c.mutate * c.mutate;
				for (int i = 0; i < NUM_LFO; i++) {
					detune[i] += rng.bi() * sq * m2 * 1.4f;
					if (detune[i] > 1.f) detune[i] = 2.f - detune[i];
					if (detune[i] < -1.f) detune[i] = -2.f - detune[i];
					phase[i] += rng.bi() * sq * m2 * 0.06f;
					wobble[i] += -wobble[i] * cdt * 0.4f + rng.bi() * sq * c.mutate * 0.9f;
					wobble[i] = std::max(-1.f, std::min(1.f, wobble[i]));
				}
			}
		}

		// ---- pulse and grid ticks
		float oldTick = std::floor(beat * GRID_DIV);
		out.pulse = false;
		beat += bps * dt;
		if (beat >= 1.f) {
			beat -= std::floor(beat);
			if (!beatFromClock) out.pulse = true;
			oldTick = -1.f;
		}
		bool tick = std::floor(beat * GRID_DIV) != oldTick;

		// ---- LFOs and their sum
		float num = 0.f, den = 0.f;
		for (int i = 0; i < NUM_LFO; i++) {
			phase[i] += ratioOf(c, i) * bps * c.speed * dt;
			phase[i] -= std::floor(phase[i]);
			float v = waveShape(phase[i], c.shape);
			out.lfo[i] = v;
			float l = c.level[i] * (1.f + 0.35f * c.mutate * wobble[i]);
			num += l * v;
			den += std::fabs(l);
		}
		float sum = den > 1e-3f ? num / den : 0.f;
		out.sum = sum;
		if (!primed) { prevSum = sum; primed = true; }
		float slopeBeats = (sum - prevSum) / (dt * bps);
		prevSum = sum;
		float steep = std::min(1.f, std::fabs(slopeBeats) / 6.f);

		// ---- thresholds
		float frac = beat * GRID_DIV;
		frac -= std::floor(frac);
		const float hyst = 0.004f;
		for (int k = 0; k < NUM_CH; k++) {
			float t = c.thresh + c.spread * (k - 1.5f) * 0.4f;
			t = std::max(-1.2f, std::min(1.2f, t));
			out.thr[k] = t;
			bool up = false, down = false;
			if (!above[k] && sum > t + hyst) { above[k] = true; up = true; }
			else if (above[k] && sum < t - hyst) { above[k] = false; down = true; }
			bool event = (c.dir == 0 && up) || (c.dir == 1 && (up || down)) || (c.dir == 2 && down);
			bool fire = false;
			float a = steep;
			if (event) {
				if (c.grid > 0.f && (1.f - frac) <= c.grid) { pending[k] = true; pendingAcc[k] = steep; }
				else fire = true;
			}
			if (tick && pending[k] && !fire) { fire = true; a = pendingAcc[k]; pending[k] = false; }
			out.trig[k] = fire;
			if (fire) {
				acc = a;
				lastCh = k;
				if (c.dir == 1) toggle[k] = !toggle[k];
				gateOn[k] = true;
				gateHold[k] = 0.005f;
			}
			// gate: open from the trigger until the sum leaves the zone (or toggles in BOTH)
			if (gateHold[k] > 0.f) gateHold[k] -= dt;
			bool inZone = (c.dir == 2) ? !above[k] : above[k];
			if (c.dir == 1) gateOn[k] = toggle[k];
			else if (gateOn[k] && !inZone && gateHold[k] <= 0.f && !pending[k]) gateOn[k] = false;
			out.gate[k] = gateOn[k];
		}
		out.acc = acc;
		out.lastCh = lastCh;
	}
};

}  // namespace lilt
