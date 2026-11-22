// Krka engine: one envelope / function channel, the shape maths and the
// chain algorithms. No Rack code here, so test/render.cpp can use it.
#pragma once
#include <cmath>
#include <algorithm>
#include "KrkaShapes.hpp"

namespace krka {

static const int NUM_CH = 4;

inline float clampf(float x, float a, float b) { return std::min(std::max(x, a), b); }

// Segment bend. k in -1..1: power curve (0 = straight), k = 2: smooth (cosine).
inline float bend(float u, float k) {
	if (k > 1.5f) return 0.5f - 0.5f * std::cos(3.14159265f * u);
	if (std::fabs(k) < 1e-4f) return u;
	return std::pow(u, std::exp2(3.f * k));
}

// Value of shape s (0..259) at position x (0..1).
inline float shapeValue(int s, float x) {
	const ShapeDef& d = SHAPES[s];
	x = clampf(x, 0.f, 1.f);
	// binary search for the first point with t >= x
	int lo = 1, hi = d.n - 1;
	while (lo < hi) {
		int mid = (lo + hi) / 2;
		if (d.p[mid].t < x) lo = mid + 1;
		else hi = mid;
	}
	// with vertical jumps (equal t) take the last of the run
	while (lo + 1 < d.n && d.p[lo + 1].t <= x && d.p[lo + 1].t == d.p[lo].t) lo++;
	const ShapePoint& a = d.p[lo - 1];
	const ShapePoint& b = d.p[lo];
	float span = b.t - a.t;
	if (span <= 1e-7f) return b.v;
	float u = clampf((x - a.t) / span, 0.f, 1.f);
	return a.v + (b.v - a.v) * bend(u, b.k);
}

// Position of the shape's first highest point (the GATE hold point).
inline float shapePeak(int s) {
	const ShapeDef& d = SHAPES[s];
	int best = 0;
	for (int i = 1; i < d.n; i++)
		if (d.p[i].v > d.p[best].v + 1e-4f) best = i;
	return d.p[best].t;
}

// RESPONSE bends the value axis, WARP the time axis. -1..1, 0 = as drawn.
// Beyond +-1 (CV) they keep going and get extreme.
inline float respond(float v, float r) {
	if (std::fabs(r) < 1e-4f) return v;
	return std::pow(clampf(v, 0.f, 1.f), std::exp2(-3.f * r));
}
inline float warpTime(float x, float w) {
	if (std::fabs(w) < 1e-4f) return x;
	return std::pow(clampf(x, 0.f, 1.f), std::exp2(3.f * w));
}
inline float unwarpTime(float y, float w) {
	if (std::fabs(w) < 1e-4f) return y;
	return std::pow(clampf(y, 0.f, 1.f), std::exp2(-3.f * w));
}

// Everything a channel needs this sample (after knobs + CV).
struct ChSettings {
	int shape = 0;
	bool reverse = false;
	float time = 1.f;      // seconds per cycle
	float resp = 0.f;
	float warp = 0.f;
	float level = 1.f;     // 0..1
	bool gateMode = false;
	bool analog = false;
	float phase = 1.f;     // where in the previous channel's last cycle this one starts (chains)
	int repeats = 0;       // extra cycles after the first; -1 = forever
};

// Shape position -> output (0..1) with reverse, warp and response.
inline float shapeOut(const ChSettings& s, float ph) {
	float x = warpTime(ph, s.warp);
	if (s.reverse) x = 1.f - x;
	return respond(shapeValue(s.shape, x), s.resp);
}

// What one sample of a channel did, so the chain can see whether a
// follower's PHASE point was passed. Positions are 0..1 of the cycle.
struct Step {
	bool moved = false;
	float from = 0.f, to = 0.f;
	bool lastA = false;    // the segment (from, to] or (from, 1] was in the last cycle
	bool wrapped = false;  // a new cycle began: second segment [0, to]
	bool lastB = false;    // ...and that new cycle is the last one
	bool end = false;      // the run finished (with REPEAT forever: a cycle finished)

	// Did this sample pass position p (0..1)?
	bool passed(float p) const {
		if (!moved) return false;
		if (!wrapped) return lastA && p > from && p <= to;
		return (lastA && p > from) || (lastB && p <= to);
	}
};

struct Channel {
	bool running = false;
	bool holding = false;
	float ph = 0.f;          // position in the current cycle 0..1
	int cycle = 0;           // which cycle of the run (0 = first)
	float value = 0.f;       // last output, 0..1 before LEVEL
	float offset = 0.f;      // analog retrigger glide
	float offsetLen = 0.f;
	bool started = false;    // ever triggered

	void stop() { running = false; holding = false; }

	bool lastCycle(const ChSettings& s) const { return s.repeats < 0 || cycle >= s.repeats; }

	float peakPhase(const ChSettings& s) const {
		float p = shapePeak(s.shape);
		if (s.reverse) p = 1.f - p;
		return unwarpTime(p, s.warp);
	}

	// Start (or restart) a run. Returns true when this first cycle is also
	// the last (then followers with PHASE 0 start right now).
	bool trigger(const ChSettings& s) {
		float cur = value;
		running = true;
		holding = false;
		cycle = 0;
		ph = 0.f;
		offset = 0.f;
		started = true;
		if (s.analog) {
			// Analog: carry on from the level we are at. If the shape rises
			// to its peak, start at the point of the rise that matches it;
			// otherwise glide from here into the shape.
			float start = shapeOut(s, 0.f);
			if (cur > start + 1e-3f) {
				float peak = peakPhase(s);
				const int N = 256;
				bool found = false;
				for (int i = 1; i <= N; i++) {
					float p = peak * i / N;
					if (shapeOut(s, p) >= cur) { ph = p; found = true; break; }
				}
				if (!found) { offset = cur - start; offsetLen = 0.05f; }
			}
			else if (cur < start - 1e-3f) {
				offset = cur - start;
				offsetLen = 0.05f;
			}
		}
		return lastCycle(s);
	}

	// Advance by dt seconds. gate = this channel's own gate (for GATE mode).
	Step process(const ChSettings& s, float dt, bool gate) {
		Step st;
		if (running) {
			float d = dt / std::max(s.time, 1e-5f);
			float next = ph + d;
			holding = false;
			if (s.gateMode && gate && cycle == 0) {
				float hold = peakPhase(s);
				if (ph <= hold && next >= hold) { next = hold; holding = true; }
			}
			st.moved = next > ph;
			st.from = ph;
			st.lastA = lastCycle(s);
			if (offset != 0.f) {
				offsetLen -= d;
				if (offsetLen <= 0.f) offset = 0.f;
			}
			if (next >= 1.f) {
				st.end = st.lastA;
				if (s.repeats < 0 || cycle < s.repeats) {
					if (s.repeats >= 0) cycle++;
					next -= 1.f;
					if (next >= 1.f) next = 0.f;
					st.wrapped = true;
					st.lastB = lastCycle(s);
					st.to = next;
				}
				else {
					next = 1.f;
					st.to = 1.f;
					running = false;
				}
			}
			else st.to = next;
			ph = next;
		}
		float v = started ? shapeOut(s, ph) : 0.f;
		if (offset != 0.f && offsetLen > 0.f) v += offset * (offsetLen / 0.05f);
		value = clampf(v, 0.f, 1.f);
		return st;
	}
};

// ---- chain algorithms: who hands over to whom (bit i = channel i)
struct Algo { const char* name; unsigned char next[NUM_CH]; };
static const int NUM_ALGOS = 16;
static const Algo ALGOS[NUM_ALGOS] = {
	{"FREE",             {0, 0, 0, 0}},
	{"A>B",              {2, 0, 0, 0}},
	{"A>B  C>D",         {2, 0, 8, 0}},
	{"A>B>C",            {2, 4, 0, 0}},
	{"A>B>C>D",          {2, 4, 8, 0}},
	{"A>B+C",            {6, 0, 0, 0}},
	{"A>B+C+D",          {14, 0, 0, 0}},
	{"A>B>C+D",          {2, 12, 0, 0}},
	{"LOOP A>B",         {2, 1, 0, 0}},
	{"LOOP A>B  C>D",    {2, 1, 8, 4}},
	{"LOOP A>B>C",       {2, 4, 1, 0}},
	{"LOOP A>B>C>D",     {2, 4, 8, 1}},
	{"A then LOOP B>C>D", {2, 4, 8, 2}},
	{"A>B then LOOP C>D", {2, 4, 8, 4}},
	{"LOOP A>C  B>D",    {4, 8, 1, 2}},
	{"A>B+C+D, D>A",     {14, 0, 0, 1}},
};

// A channel listens to its own TRIG input unless an earlier (lower-letter)
// channel hands over to it. So chain heads and loop heads still take triggers.
inline bool takesOwnTrigger(int algo, int ch) {
	for (int j = 0; j < ch; j++)
		if (ALGOS[algo].next[j] & (1 << ch)) return false;
	return true;
}

// Clock-synced TIME values, in beats.
static const int NUM_DIVS = 19;
static const float DIV_BEATS[NUM_DIVS] = {0.125f, 1.f / 6.f, 0.25f, 1.f / 3.f, 0.5f, 2.f / 3.f, 1.f, 1.5f, 2.f, 3.f, 4.f,
                                          6.f, 8.f, 12.f, 16.f, 24.f, 32.f, 48.f, 64.f};
static const char* const DIV_NAMES[NUM_DIVS] = {"1/32", "1/16T", "1/16", "1/8T", "1/8", "1/4T", "1/4", "1/4.", "1/2",
                                                "1/2.", "1 BAR", "1.5 BAR", "2 BARS", "3 BARS", "4 BARS", "6 BARS",
                                                "8 BARS", "12 BARS", "16 BARS"};

// Free TIME knob 0..1 -> seconds, 0.85 ms .. 30 min.
inline float knobSeconds(float k) { return 0.00085f * std::exp2(k * 21.0107f); }

} // namespace krka
