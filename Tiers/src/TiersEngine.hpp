#pragma once
// Tiers pattern engine. No Rack code here, so test/render.cpp can use it too.
//
// The idea: every step owns three random numbers ("thresholds") that are rolled
// once by GENERATE and then kept:
//   r = does the step play?   q = does it double into 32nds?   a = is it accented?
// A step plays when its r is below the weight of its metric level (1/2, 1/4,
// 1/8, 1/16, 1/32). So turning a weight up adds hits on that level one by one,
// in a fixed order, and turning it back down removes the same hits again.
// MUTATE re-rolls a few thresholds at the end of each loop; LOCK stops that.
// A threshold below 0 means "always" and above 1.5 means "never" (hand edits).
#include <cmath>
#include <cstdint>
#include <cstring>
#include "TiersPresets.hpp"

namespace tiers {

static const int MAXS = 32;
static const int LEVELS = 5;  // 0 = 1/2, 1 = 1/4, 2 = 1/8, 3 = 1/16, 4 = 1/32
static const float ON = -1.f, OFF = 2.f;

// Metric level of a step. STEP 1/16: a bar is 16 steps and the 1/32 weight is the
// chance of a 32nd-note double. STEP 1/32: a bar is 32 steps and 1/32 is a level.
inline int levelOf(int i, bool res32) {
	if (res32) {
		int p = i % 32;
		if (p % 16 == 0) return 0;
		if (p % 8 == 0) return 1;
		if (p % 4 == 0) return 2;
		if (p % 2 == 0) return 3;
		return 4;
	}
	int p = i % 16;
	if (p % 8 == 0) return 0;
	if (p % 4 == 0) return 1;
	if (p % 2 == 0) return 2;
	return 3;
}

struct Settings {
	float w[LEVELS] = {0.9f, 0.5f, 0.3f, 0.15f, 0.f};
	bool count = false;   // COUNT mode: exactly `hits` steps, chosen by weight
	int hits = 6;
	float accent = 0.5f;  // 0..1
	int rotate = 0;
	int length = 16;      // 16 or 32
	bool res32 = false;
};

struct Thresholds {
	float r[MAXS], q[MAXS], a[MAXS];
};

struct Result {
	bool hit[MAXS], acc[MAXS], rat[MAXS];
	int level[MAXS];  // metric level the hit came from (for the LEVEL output)
	int hits;
};

struct Rng {
	uint64_t s = 0x9E3779B97F4A7C15ull;
	void seed(uint64_t v) { s = v ? v : 0x9E3779B97F4A7C15ull; }
	float uni() {  // [0, 1)
		s ^= s << 13; s ^= s >> 7; s ^= s << 17;
		return (float) ((s >> 40) & 0xFFFFFF) / 16777216.f;
	}
};

struct Engine {
	Thresholds cur, seed;  // seed = pattern at the last GENERATE / LOAD (BACK returns to it)
	Rng rng;

	Engine() { generate(); }

	float accentThreshold(int level) {
		// Strong positions get low thresholds so they take accents first.
		static const float base[LEVELS] = {0.15f, 0.30f, 0.52f, 0.70f, 0.82f};
		float v = base[level] + (rng.uni() - 0.5f) * 0.5f;
		return v < 0.f ? 0.f : v > 1.f ? 1.f : v;
	}

	void rollStep(Thresholds& t, int i, bool res32) {
		t.r[i] = rng.uni();
		t.q[i] = rng.uni();
		t.a[i] = accentThreshold(levelOf(i, res32));
	}

	void generate(bool res32 = false) {
		for (int i = 0; i < MAXS; i++) rollStep(cur, i, res32);
		seed = cur;
	}

	void back() { cur = seed; }

	// At the end of a loop: each step re-rolls with chance amount * 0.25.
	// Hand-placed steps (ON / OFF) stay put. Returns how many changed.
	int mutate(float amount, const Settings& s) {
		int n = 0;
		float p = amount * 0.25f;
		for (int i = 0; i < s.length; i++) {
			if (rng.uni() >= p) continue;
			if (cur.r[i] >= 0.f && cur.r[i] <= 1.f) cur.r[i] = rng.uni();
			if (cur.q[i] >= 0.f && cur.q[i] <= 1.f) cur.q[i] = rng.uni();
			if (cur.a[i] >= 0.f && cur.a[i] <= 1.f) cur.a[i] = accentThreshold(levelOf(i, s.res32));
			n++;
		}
		return n;
	}

	static bool below(float t, float w) { return t < w; }  // ON (-1) always, OFF (2) never

	void evaluate(const Settings& s, Result& out) const {
		bool hit[MAXS] = {};
		int len = s.length;
		if (!s.count) {
			for (int i = 0; i < len; i++) hit[i] = below(cur.r[i], s.w[levelOf(i, s.res32)]);
		}
		else {
			// Weighted pick without replacement: key = (1 - r)^(1 / w), take the largest.
			// The same r keeps the choice stable while HITS or the weights move.
			float key[MAXS];
			for (int i = 0; i < len; i++) {
				float w = s.w[levelOf(i, s.res32)];
				float r = cur.r[i];
				if (r < 0.f) key[i] = 2.f;
				else if (r > 1.5f || w <= 0.f) key[i] = -1.f;
				else key[i] = std::pow(1.f - r, 1.f / w);
			}
			int want = s.hits < 0 ? 0 : s.hits > len ? len : s.hits;
			for (int k = 0; k < want; k++) {
				int best = -1;
				for (int i = 0; i < len; i++)
					if (!hit[i] && key[i] >= 0.f && (best < 0 || key[i] > key[best])) best = i;
				if (best < 0) break;
				hit[best] = true;
			}
			for (int i = 0; i < len; i++)
				if (key[i] == 2.f) hit[i] = true;  // hand-placed hits always play
		}
		out.hits = 0;
		for (int k = 0; k < MAXS; k++) {
			out.hit[k] = out.acc[k] = out.rat[k] = false;
			out.level[k] = 0;
		}
		for (int k = 0; k < len; k++) {
			int i = ((k - s.rotate) % len + len) % len;  // ROTATE slides the pattern along the grid
			if (!hit[i]) continue;
			out.hit[k] = true;
			out.hits++;
			out.level[k] = levelOf(i, s.res32);
			out.acc[k] = below(cur.a[i], s.accent);
			out.rat[k] = !s.res32 && below(cur.q[i], s.w[4]);
		}
	}

	int baseIndex(int k, const Settings& s) const {
		return ((k - s.rotate) % s.length + s.length) % s.length;
	}

	// Hand edits on the display, by displayed step k. They go into the seed too.
	void setHit(int k, const Settings& s, bool on) {
		int i = baseIndex(k, s);
		cur.r[i] = seed.r[i] = on ? ON : OFF;
	}
	void setAccent(int k, const Settings& s, bool on) {
		int i = baseIndex(k, s);
		cur.a[i] = seed.a[i] = on ? ON : OFF;
	}
	void setRatchet(int k, const Settings& s, bool on) {
		int i = baseIndex(k, s);
		cur.q[i] = seed.q[i] = on ? ON : OFF;
	}
	void clearHandEdits(const Settings& s) {
		for (int i = 0; i < MAXS; i++) {
			float* v[3] = {&cur.r[i], &cur.q[i], &cur.a[i]};
			for (int j = 0; j < 3; j++)
				if (*v[j] < 0.f || *v[j] > 1.5f) {
					if (j == 2) *v[j] = accentThreshold(levelOf(i, s.res32));
					else *v[j] = rng.uni();
				}
		}
		seed = cur;
	}

	// Load a preset (16th grid). Fills `s` with the weights, length and accent that
	// reproduce it exactly in CHANCE mode, and thresholds so that moving a weight
	// adds or removes hits from there.
	void loadPreset(const Preset& p, Settings& s) {
		int len = (int) std::strlen(p.steps);
		if (len > MAXS) len = MAXS;
		s.length = len > 16 ? 32 : 16;
		s.res32 = false;
		s.count = false;
		s.rotate = 0;
		s.accent = 0.5f;
		bool hit[MAXS], acc[MAXS], rat[MAXS];
		for (int i = 0; i < MAXS; i++) {
			char c = p.steps[i % len];
			hit[i] = c != '-';
			acc[i] = c == 'X' || c == 'R';
			rat[i] = c == 'r' || c == 'R';
		}
		int n[LEVELS] = {}, h[LEVELS] = {}, hits = 0, rats = 0;
		for (int i = 0; i < s.length; i++) {
			int l = levelOf(i, false);
			n[l]++;
			if (hit[i]) { h[l]++; hits++; if (rat[i]) rats++; }
		}
		for (int l = 0; l < 4; l++) s.w[l] = n[l] ? (float) h[l] / n[l] : 0.f;
		s.w[4] = hits ? (float) rats / hits : 0.f;
		s.hits = hits;
		for (int i = 0; i < MAXS; i++) {
			float w = s.w[levelOf(i, false)];
			float u = rng.uni();
			cur.r[i] = hit[i] ? u * w : w + u * (1.f - w);
			if (!hit[i] && w >= 1.f) cur.r[i] = OFF;
			if (hit[i] && w <= 0.f) cur.r[i] = ON;
			float v = rng.uni(), w4 = s.w[4];
			cur.q[i] = rat[i] ? v * w4 : w4 + v * (1.f - w4);
			if (!rat[i] && w4 >= 1.f) cur.q[i] = OFF;
			float a = rng.uni();
			cur.a[i] = acc[i] ? a * 0.5f : 0.5f + 0.001f + a * 0.499f;
		}
		seed = cur;
	}
};

} // namespace tiers
