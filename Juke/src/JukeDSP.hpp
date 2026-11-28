// Juke engine: footwork presets, the pattern generator (generate, mutate, fill,
// phrase evolve) and nine synthesised voices. No Rack code in here, so
// test/render.cpp can run it offline.
//
// The generator is a straight port of the browser prototype (footwork-sequencer.html):
// same PRNG (mulberry32, in doubles) and the same order of random draws, so a seed
// with the same density and feel gives the same bar in both.
#pragma once
#include <cmath>
#include <cstdint>
#include <cstring>
#include <algorithm>

namespace juke {

static const float PI_F = 3.14159265358979f;
enum Lane { KICK, CLAP, SNARE, CHAT, OHAT, TOMH, TOML, BELL, CHOP, NUM_LANES };
static const int STEPS = 16;
static const char* const LANE_NAMES[NUM_LANES] = {"Kick", "Clap", "Snare", "Closed hat", "Open hat", "Tom hi", "Tom lo", "Cowbell", "Vocal chop"};

// Step values: 0 off, 1 hit, 2 ghost.
struct Pattern {
	uint8_t s[NUM_LANES][STEPS];
	Pattern() { clear(); }
	void clear() { std::memset(s, 0, sizeof(s)); }
	int count(int lane, int v) const { int n = 0; for (int i = 0; i < STEPS; i++) n += s[lane][i] == v; return n; }
};

// ---- presets ----
// x hit, o ghost, . off. nullptr = empty lane. Lane order as enum Lane.
struct PresetDef { const char* name; const char* lanes[NUM_LANES]; };
static const PresetDef PRESETS[] = {
	{"Basic",           {"x..x..x.x..x..x.", "....x.......x...", nullptr, "x.x.x.x.x.x.x.x.", nullptr, nullptr, nullptr, "...x.....x.....o", nullptr}},
	{"Intermediate",    {"x..x..x..oxx..x.", "....x..o....x..o", nullptr, "xoxxxox.xoxxxox.", ".......x.......x", nullptr, ".............x..", "...x.....x......", "..x.......x....."}},
	{"Roll / fill",     {"x..x..x.........", nullptr, "........xoxoxxxx", "xxxxxxxx........", nullptr, ".........x.x....", nullptr, nullptr, "............xxxx"}},
	{"3-3-2 vs 4",      {"x..x..x.x..x..x.", nullptr, nullptr, "x...x...x...x...", nullptr, nullptr, nullptr, nullptr, nullptr}},
	{"Half-time heavy", {"x..x......x..x..", "........x.......", nullptr, "x.x.x.x.x.x.x.x.", nullptr, nullptr, nullptr, "...x.....x....x.", "......x.......x."}},
	{"Juke drive",      {"x..x..x.x..x..x.", "....x.......x...", ".......o......oo", "xoxoxoxoxoxoxoxo", "......x.......x.", nullptr, nullptr, nullptr, nullptr}},
	{"Tom runner",      {"x.......x.......", "........x.......", nullptr, "x.x.x.x.x.x.x.x.", nullptr, "...x.....x..x...", "......x......x.x", nullptr, nullptr}},
	{"Chop stutter",    {"x..x..x.x..x..x.", "........x.......", nullptr, "..x...x...x...x.", nullptr, nullptr, nullptr, nullptr, "x.xx..x...x.xxxx"}},
	{"Cowbell hook",    {"x..x..x...x..x..", "....x.......x...", nullptr, "x.x.x.x.x.x.x.x.", nullptr, nullptr, nullptr, "..x..x.x..x..x.x", nullptr}},
	{"Ghost shuffle",   {"x..x..x.x..x..x.", "....x.......x...", "..o..o.o..o..o.o", "xoxoxoxoxoxoxoxo", "...............x", nullptr, nullptr, nullptr, nullptr}},
	{"Broken 3-3-2",    {"x..x..x..x.x..x.", "........x.......", ".......o.......o", "x.xx.xx.x.xx.xx.", nullptr, nullptr, "..............x.", nullptr, nullptr}},
	{"Clap roll build", {"x..x..x.........", "....x...xoxoxxxx", "............xxxx", "x.x.x.x.x.x.x.x.", nullptr, nullptr, nullptr, nullptr, nullptr}},
	{"Sparse intro",    {nullptr, nullptr, nullptr, "x.x.x.x.x.x.x.x.", nullptr, nullptr, nullptr, "...x.....x......", "..x.......x....."}},
	{"Blank",           {nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr}},
};
static const int NUM_PRESETS = (int) (sizeof(PRESETS) / sizeof(PRESETS[0]));

static inline void loadPreset(int i, Pattern& p) {
	p.clear();
	const PresetDef& d = PRESETS[std::min(std::max(i, 0), NUM_PRESETS - 1)];
	for (int l = 0; l < NUM_LANES; l++) {
		if (!d.lanes[l]) continue;
		for (int s = 0; s < STEPS && d.lanes[l][s]; s++)
			p.s[l][s] = d.lanes[l][s] == 'x' ? 1 : (d.lanes[l][s] == 'o' ? 2 : 0);
	}
}
static inline void toText(const Pattern& p, int lane, char* out17) {
	for (int s = 0; s < STEPS; s++) out17[s] = p.s[lane][s] == 1 ? 'x' : (p.s[lane][s] == 2 ? 'o' : '.');
	out17[STEPS] = 0;
}
static inline void fromText(const char* t, Pattern& p, int lane) {
	for (int s = 0; s < STEPS; s++) {
		char c = t && t[s] ? t[s] : '.';
		if (!t || !t[s]) t = nullptr;
		p.s[lane][s] = c == 'x' ? 1 : (c == 'o' ? 2 : 0);
	}
}

// ---- PRNG (mulberry32, same as the prototype) ----
struct Rng {
	uint32_t a = 1u;
	void seed(uint32_t s) { a = s; }
	double next() {
		a += 0x6D2B79F5u;
		uint32_t t = a;
		t = (t ^ (t >> 15)) * (1u | t);
		t = (t + ((t ^ (t >> 7)) * (61u | t))) ^ t;
		return (double) (t ^ (t >> 14)) / 4294967296.0;
	}
	bool chance(double p) { return next() < p; }
	int below(int n) { return (int) std::floor(next() * n); }          // pick index
	int range(int lo, int hi) { return lo + (int) std::floor(next() * (hi - lo + 1)); }
};

// Small fixed list (no allocation on the audio thread).
struct List {
	int v[STEPS + 4]; int n = 0;
	void push(int x) { if (n < STEPS + 4) v[n++] = x; }
	bool has(int x) const { for (int i = 0; i < n; i++) if (v[i] == x) return true; return false; }
	int pick(Rng& r) const { return v[r.below(n)]; }
	int take(Rng& r) { int i = r.below(n), x = v[i]; for (int j = i; j < n - 1; j++) v[j] = v[j + 1]; n--; return x; }
};

enum Feel { FEEL_AUTO, FEEL_DOUBLE, FEEL_HALF };

// Split 16 steps into groups of 3, 2 and sometimes 4. Fills `starts` with group start steps.
static inline void compose(Rng& r, List& starts) {
	for (int t = 0; t < 60; t++) {
		starts.n = 0;
		int s = 0;
		while (s < 16) { starts.push(s); s += r.chance(0.6) ? 3 : (r.chance(0.65) ? 2 : 4); }
		if (s == 16) return;
	}
	starts.n = 0;
	const int fb[6] = {0, 3, 6, 8, 11, 14};
	for (int x : fb) starts.push(x);
}

static inline List hitsOf(const Pattern& p, int lane, int v /*0 = any*/) {
	List l;
	for (int i = 0; i < STEPS; i++) if (v ? p.s[lane][i] == v : p.s[lane][i] != 0) l.push(i);
	return l;
}

// Style rules after generate and mutate.
static inline void guard(Pattern& p, Rng& r) {
	if (p.count(KICK, 1) == 0) return;    // kick-less bars (intros) stay kick-less
	p.s[KICK][0] = 1;
	for (int i = 1; i < STEPS; i++) if (p.s[CLAP][i] == 1) p.s[KICK][i] = 0;
	for (;;) {
		List h; for (int i = 1; i < STEPS; i++) if (p.s[KICK][i] == 1) h.push(i);
		if (h.n <= 6) break;
		p.s[KICK][h.pick(r)] = 0;
	}
}

static inline void generate(Pattern& p, Rng& r, double dens, int feel) {
	p.clear();
	if (feel == FEEL_AUTO) feel = r.chance(0.6) ? FEEL_DOUBLE : FEEL_HALF;
	List claps;
	if (feel == FEEL_DOUBLE) { claps.push(4); claps.push(12); } else claps.push(8);
	for (int i = 0; i < claps.n; i++) p.s[CLAP][claps.v[i]] = 1;

	// Kick: one hit per group start, thinned by density, nudged off the clap.
	List starts, dropped;
	compose(r, starts);
	double keepP = (feel == FEEL_HALF ? 0.4 : 0.55) + 0.4 * dens;
	for (int k = 0; k < starts.n; k++) {
		int i = starts.v[k];
		if (i > 0 && !r.chance(keepP)) { dropped.push(i); continue; }
		int j = i;
		if (claps.has(j)) j += r.chance(0.5) ? -1 : 1;
		if (j > 0 && p.s[KICK][j - 1] && r.chance(0.7)) { dropped.push(i); continue; }
		p.s[KICK][j] = 1;
	}
	{
		List fr; for (int i = 0; i < STEPS; i++) if (!p.s[KICK][i] && !claps.has(i) && (i % 2)) fr.push(i);
		if (fr.n && r.chance(0.3 * dens)) p.s[KICK][fr.pick(r)] = 2;
	}
	// Snare ghosts hug the kick and clap.
	{
		List near;
		for (int i = 0; i < STEPS; i++)
			if (!p.s[KICK][i] && !claps.has(i) && (claps.has(i - 1) || claps.has(i + 1) || (i + 1 < STEPS && p.s[KICK][i + 1]))) near.push(i);
		for (int n = (int) std::round(dens * 4); n > 0 && near.n; n--) p.s[SNARE][near.take(r)] = 2;
		if (r.chance(0.35)) p.s[SNARE][15] = r.chance(0.5) ? 1 : 2;
	}
	// Hats: 8ths, 16ths with ghosts, or accents on the groupings.
	double hr = r.next();
	for (int i = 0; i < STEPS; i++) {
		if (hr < 0.4) p.s[CHAT][i] = (i % 2) ? 0 : 1;
		else if (hr < 0.75) p.s[CHAT][i] = (i % 2) ? (r.chance(0.4 + 0.5 * dens) ? 2 : 0) : 1;
		else p.s[CHAT][i] = starts.has(i) ? 1 : (r.chance(0.3 + 0.5 * dens) ? 2 : 0);
	}
	if (r.chance(0.25)) { int w = r.range(2, 13); p.s[CHAT][w] = p.s[CHAT][w + 1] = 0; }
	if (r.chance(0.6)) {
		List c; c.push(7); c.push(15);
		int last = starts.v[starts.n - 1] - 1;
		if (last > 0) c.push(last);
		int o = c.pick(r);
		p.s[OHAT][o] = 1; p.s[CHAT][o] = 0;
	}
	// Toms answer the kick on the group starts it skipped.
	if (r.chance(0.45)) {
		bool hi = true;
		for (int k = 0; k < std::min(dropped.n, 3); k++) { p.s[hi ? TOMH : TOML][dropped.v[k]] = 1; hi = !hi; }
		if (r.chance(0.35)) { p.s[TOMH][13] = 1; p.s[TOML][14] = 1; }
	}
	// Cowbell runs its own grouping, offset from the kick.
	if (r.chance(0.6)) {
		int off = r.range(1, 2);
		List bs; compose(r, bs);
		for (int k = 0; k < bs.n; k++) bs.v[k] = (bs.v[k] + off) % 16;
		for (int n = r.range(2, 3 + (int) std::round(dens * 2)); n > 0 && bs.n; n--) p.s[BELL][bs.take(r)] = 1;
		if (r.chance(0.3) && !p.s[BELL][15]) p.s[BELL][15] = 2;
	}
	// Vocal chop: off-beat motif in the first half, answered in the second; sometimes a stutter.
	if (r.chance(0.7)) {
		static const int opts[6] = {1, 2, 3, 5, 6, 7};
		for (int n = r.range(1, 2); n > 0; n--) {
			int i = opts[r.below(6)];
			p.s[CHOP][i] = 1;
			if (r.chance(0.8)) p.s[CHOP][i + 8] = 1;
		}
		if (r.chance(0.3)) for (int i = 12; i < 16; i++) p.s[CHOP][i] = 1;
	}
	guard(p, r);
}

static inline void mutate(Pattern& p, Rng& r, double amt) {
	List claps = hitsOf(p, CLAP, 1);
	for (int n = 1 + (int) std::round(amt * 5); n > 0; n--) {
		static const int OPS[7] = {0, 0, 1, 2, 3, 4, 5};   // shift x2, ghost, drop, add, rotate, stutter
		switch (OPS[r.below(7)]) {
		case 0: {
			List ks; for (int i = 1; i < STEPS; i++) if (p.s[KICK][i] == 1) ks.push(i);
			if (!ks.n) break;
			int i = ks.pick(r), j = i + (r.chance(0.5) ? 1 : -1);
			if (j > 0 && j < STEPS && !p.s[KICK][j] && !claps.has(j)) { p.s[KICK][j] = 1; p.s[KICK][i] = 0; }
		} break;
		case 1: {
			static const int L[6] = {SNARE, CHAT, BELL, CHOP, TOMH, TOML};
			int k = L[r.below(6)];
			List h = hitsOf(p, k, 0);
			if (h.n) { int i = h.pick(r); p.s[k][i] = p.s[k][i] == 1 ? 2 : 1; }
		} break;
		case 2: {
			static const int L[7] = {CHAT, BELL, CHOP, TOMH, TOML, SNARE, OHAT};
			int k = L[r.below(7)];
			List h = hitsOf(p, k, 0);
			if (h.n) p.s[k][h.pick(r)] = 0;
		} break;
		case 3: {
			static const int L[3] = {SNARE, CHAT, SNARE};
			int k = L[r.below(3)], i = r.range(0, 15);
			if (!p.s[k][i] && !claps.has(i)) p.s[k][i] = 2;
		} break;
		case 4: {
			static const int L[4] = {BELL, CHOP, TOMH, TOML};
			int k = L[r.below(4)];
			int rot = r.range(1, 3) * (r.chance(0.5) ? 1 : -1);
			uint8_t row[STEPS]; std::memcpy(row, p.s[k], STEPS);
			for (int i = 0; i < STEPS; i++) p.s[k][i] = row[(i - rot + 16) % 16];
		} break;
		case 5: {
			static const int L[2] = {CHOP, SNARE};
			int k = L[r.below(2)], s = r.range(12, 14);
			for (int i = s; i < STEPS; i++) p.s[k][i] = i == s ? 1 : (r.chance(0.5) ? 1 : 2);
		} break;
		}
	}
	guard(p, r);
}

// Phrase-ending fill: kick drops out of the second half, hats out of the last beat.
static inline void fillBar(Pattern& p, Rng& r) {
	for (int i = 8; i < STEPS; i++) p.s[KICK][i] = 0;
	for (int i = 12; i < STEPS; i++) { p.s[CHAT][i] = 0; p.s[OHAT][i] = 0; }
	static const int T[5] = {0, 0, 1, 2, 3};   // snare roll x2, clap roll, tom descent, chop stutter
	int t = T[r.below(5)];
	if (t == 0 || t == 1) {
		int k = t == 0 ? SNARE : CLAP;
		for (int i = 8; i < STEPS; i++) p.s[k][i] = i < 12 ? ((i % 2 == 0 || i == 11) ? 2 : 0) : 1;
	}
	else if (t == 2) { p.s[TOMH][12] = p.s[TOMH][13] = 1; p.s[TOML][14] = p.s[TOML][15] = 1; p.s[SNARE][15] = 2; }
	else { for (int i = 12; i < STEPS; i++) p.s[CHOP][i] = 1; p.s[SNARE][14] = p.s[SNARE][15] = 2; }
}

// ---- voices ----
struct Biquad {
	float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
	void set(int type, float f, float q, float sr) {   // 0 low-pass, 1 high-pass, 2 band-pass (0 dB peak)
		f = std::min(f, sr * 0.45f);
		float w = 2.f * PI_F * f / sr, c = std::cos(w), al = std::sin(w) / (2.f * q), a0 = 1.f + al;
		if (type == 0) { b0 = (1 - c) / 2; b1 = 1 - c; b2 = (1 - c) / 2; }
		else if (type == 1) { b0 = (1 + c) / 2; b1 = -(1 + c); b2 = (1 + c) / 2; }
		else { b0 = al; b1 = 0; b2 = -al; }
		a1 = -2 * c; a2 = 1 - al;
		b0 /= a0; b1 /= a0; b2 /= a0; a1 /= a0; a2 /= a0;
	}
	float process(float x) {
		float y = b0 * x + z1;
		z1 = b1 * x - a1 * y + z2;
		z2 = b2 * x - a2 * y;
		return y;
	}
	void reset() { z1 = z2 = 0; }
};

// Attack 1 ms, then down to -80 dB over `dec` seconds (Web Audio exponentialRamp to 0.0001).
static inline float envAD(float t, float dec) {
	if (t < 0.001f) return t / 0.001f;
	return std::exp(-9.21f * (t - 0.001f) / dec);
}
// Pitch sweep f0 -> f1 over `dur` (exponential), then holds f1.
static inline float sweep(float t, float f0, float f1, float dur) {
	return t >= dur ? f1 : f0 * std::pow(f1 / f0, t / dur);
}

struct Voice {
	float t = 10.f, vel = 0.f, ph = 0.f, ph2 = 0.f, choke = 1.f;
	bool on = false;
	Biquad f1, f2, f3;
	void trig(float v) { t = 0.f; vel = v; ph = ph2 = 0.f; choke = 1.f; on = true; }
};

struct DrumEngine {
	float sr = 48000.f;
	Voice v[NUM_LANES];
	uint32_t ns = 22222u;
	float tune = 1.f, decay = 1.f;   // reserved for later per-kit controls

	DrumEngine() { setSampleRate(48000.f); }
	void setSampleRate(float s) {
		sr = s;
		v[SNARE].f1.set(1, 1200.f, 0.707f, sr);
		v[CLAP].f1.set(2, 1300.f, 1.2f, sr);
		v[CHAT].f1.set(1, 7500.f, 0.707f, sr);
		v[OHAT].f1.set(1, 7000.f, 0.707f, sr);
		v[BELL].f1.set(2, 800.f, 2.5f, sr);
		v[CHOP].f1.set(2, 750.f, 8.f, sr);
		v[CHOP].f2.set(2, 1150.f, 10.f, sr);
		v[CHOP].f3.set(2, 2600.f, 12.f, sr);
	}
	float noise() { ns ^= ns << 13; ns ^= ns >> 17; ns ^= ns << 5; return (float) (int32_t) ns / 2147483648.f; }

	void hit(int lane, int level) {
		v[lane].trig(level == 1 ? 1.f : 0.4f);
		if (lane == CHAT && v[OHAT].on) v[OHAT].choke = 0.999f;   // start the choke fade
	}

	float sample(int i) {
		Voice& x = v[i];
		const float dt = 1.f / sr, t = x.t;
		float out = 0.f;
		switch (i) {
		case KICK: {
			x.ph += sweep(t, 160.f, 42.f, 0.09f) * dt; x.ph -= std::floor(x.ph);
			x.ph2 += sweep(t, 900.f, 200.f, 0.01f) * dt; x.ph2 -= std::floor(x.ph2);
			float tri = 4.f * std::fabs(x.ph2 - 0.5f) - 1.f;
			out = 1.1f * std::sin(2.f * PI_F * x.ph) * envAD(t, 0.55f) + 0.25f * tri * envAD(t, 0.02f);
			if (t > 0.7f) x.on = false;
		} break;
		case SNARE: {
			x.ph += sweep(t, 200.f, 160.f, 0.06f) * dt; x.ph -= std::floor(x.ph);
			float tri = 4.f * std::fabs(x.ph - 0.5f) - 1.f;
			out = 0.6f * x.f1.process(noise()) * envAD(t, 0.15f) + 0.35f * tri * envAD(t, 0.08f);
			if (t > 0.25f) x.on = false;
		} break;
		case CLAP: {
			float g;
			if (t < 0.033f) { float tb = std::fmod(t, 0.011f); g = tb < 0.009f ? 0.9f * std::pow(0.05f / 0.9f, tb / 0.009f) : 0.05f; }
			else g = 0.8f * std::pow(0.0001f / 0.8f, (t - 0.033f) / 0.217f);
			out = 3.3f * x.f1.process(noise()) * g;
			if (t > 0.3f) x.on = false;
		} break;
		case CHAT: {
			out = 0.9f * x.f1.process(noise()) * envAD(t, 0.04f);
			if (t > 0.06f) x.on = false;
		} break;
		case OHAT: {
			if (x.choke < 1.f) { x.choke *= std::exp(-dt / 0.005f); if (x.choke < 1e-4f) x.on = false; }
			out = 0.8f * x.f1.process(noise()) * envAD(t, 0.35f) * x.choke;
			if (t > 0.45f) x.on = false;
		} break;
		case TOMH: case TOML: {
			bool hi = i == TOMH;
			x.ph += (hi ? sweep(t, 210.f, 140.f, 0.12f) : sweep(t, 130.f, 85.f, 0.14f)) * dt; x.ph -= std::floor(x.ph);
			out = (hi ? 0.8f : 0.9f) * std::sin(2.f * PI_F * x.ph) * envAD(t, hi ? 0.3f : 0.38f);
			if (t > 0.5f) x.on = false;
		} break;
		case BELL: {
			x.ph += 540.f * dt; x.ph -= std::floor(x.ph);
			x.ph2 += 800.f * dt; x.ph2 -= std::floor(x.ph2);
			float sq = (x.ph < 0.5f ? 1.f : -1.f) + (x.ph2 < 0.5f ? 1.f : -1.f);
			out = 0.6f * x.f1.process(sq) * envAD(t, 0.28f);
			if (t > 0.35f) x.on = false;
		} break;
		case CHOP: {
			float f = t < 0.12f ? 230.f - 25.f * t / 0.12f : 205.f;
			x.ph += f * dt; x.ph -= std::floor(x.ph);
			float saw = 2.f * x.ph - 1.f;
			float s = x.f1.process(saw) + 0.6f * x.f2.process(saw) + 0.25f * x.f3.process(saw);
			float g = t < 0.008f ? t / 0.008f : (t < 0.07f ? 1.f : std::pow(0.0001f, (t - 0.07f) / 0.06f));
			out = 2.4f * s * g;
			if (t > 0.15f) x.on = false;
		} break;
		}
		x.t += dt;
		return out * x.vel;
	}

	// out[NUM_LANES], each about +-1.
	void process(float* out) {
		for (int i = 0; i < NUM_LANES; i++) out[i] = v[i].on ? sample(i) : 0.f;
	}
};

// ---- the stepper: base pattern, the bar being played, phrase evolve and fills ----
struct Seq {
	Pattern base, play;
	int step = 0, bar = 0;
	int phrase = 4;           // bars per phrase when evolving
	bool evolve = false;
	bool fillArmed = false, fillDone = false;
	double vary = 0.35;
	Rng free;                 // mutations, fills, evolve
	Rng gen;                  // Generate (seeded, recallable)
	uint32_t seed = 1;
	int barKind = 0;          // 0 base, 1 variation, 2 fill (for the display)
	int barInPhrase = 0;
	bool holdBar = false;     // while shifted: wrapping to step 1 replays this bar instead of starting the next
	bool wrapHeld = false;

	Seq() { free.seed(0xC0FFEEu); loadPreset(0, base); play = base; }
	void reset() { step = 0; bar = 0; fillArmed = false; fillDone = false; play = base; barKind = 0; barInPhrase = 0; holdBar = wrapHeld = false; }

	void startBar() {
		fillDone = false;
		if (!evolve) { play = base; barKind = 0; barInPhrase = 0; bar++; return; }
		int len = std::max(1, phrase), b = bar % len;
		bar++;
		barInPhrase = b;
		if (b == 0) {
			if (bar > 1) mutate(base, free, vary * 0.35);
			play = base; barKind = 0;
		}
		else if (b == len - 1) { play = base; fillBar(play, free); barKind = 2; }
		else { play = base; mutate(play, free, vary * 0.5); barKind = 1; }
	}

	// Level of each lane for the coming step into lv[] (0 none, 1 hit, 2 ghost), then advance.
	void next(uint8_t* lv) {
		if (step == 0) { if (wrapHeld) wrapHeld = false; else startBar(); }
		else wrapHeld = false;
		if (step == 8 && fillArmed && barKind != 2) { fillBar(play, free); fillArmed = false; fillDone = true; barKind = 2; }
		for (int l = 0; l < NUM_LANES; l++) lv[l] = play.s[l][step];
		step = (step + 1) % STEPS;
		if (step == 0 && holdBar) wrapHeld = true;
	}

	void doGenerate(uint32_t s, double dens, int feel) {
		seed = s; gen.seed(s);
		generate(base, gen, dens, feel);
		play = base;
	}
	void doMutate() { mutate(base, free, vary); play = base; }
	void edit(int lane, int st) {
		uint8_t v = (uint8_t) ((play.s[lane][st] + 1) % 3);
		base.s[lane][st] = v; play.s[lane][st] = v;
	}
};


// ---- metric shift: play the pattern's 16ths at a ratio of the clock's 16ths ----
// Tempo ratios new:old. 4:3 makes the dotted 8th the new beat (160 -> 213 BPM feel),
// 3:2 plays 16th triplets (240), 1:2 is half-time (80) and so on. The pattern stays
// locked to the clock: every clock 16th re-anchors where the shifted steps fall.
struct Ratio { const char* name; int a, b; };
static const Ratio RATIOS[] = {
	{"1:2", 1, 2}, {"2:3", 2, 3}, {"3:4", 3, 4}, {"1:1", 1, 1}, {"4:3", 4, 3}, {"3:2", 3, 2}, {"2:1", 2, 1},
};
static const int NUM_RATIOS = 7, UNITY_RATIO = 3;

struct MetricShift {
	long master = -1;          // clock 16ths so far (index of the last one)
	float since = 0.f;         // samples since the last clock 16th
	bool active = false;
	int a = 1, b = 1;          // ratio in force
	long c0 = 0, fired = 0;

	void reset() { master = -1; since = 0.f; active = false; fired = 0; }
	int masterStep() const { return master < 0 ? -1 : (int) (master % STEPS); }
	float stepLen(float period) const { return active ? period * b / a : period; }

	// Call every sample. clockTick: a clock 16th arrived this sample. period: clock 16th in samples.
	// want: shift requested. ri: index into RATIOS. returnOnBar: come back on the clock's bar line
	// (and ask for the pattern to realign to step 1), otherwise on the next beat.
	// Returns how many pattern steps to play this sample.
	int process(bool clockTick, float period, bool want, int ri, bool returnOnBar, bool& realign) {
		realign = false;
		int steps = 0;
		if (clockTick) {
			master++;
			since = 0.f;
			int ms = masterStep();
			bool beat = (ms % 4) == 0;
			const Ratio& r = RATIOS[std::min(std::max(ri, 0), NUM_RATIOS - 1)];
			bool unity = r.a == r.b;
			if (!active) {
				if (want && !unity && beat) { active = true; a = r.a; b = r.b; c0 = master; fired = 0; }
			}
			else {
				bool stop = !want || unity;
				if (stop && (returnOnBar ? ms == 0 : beat)) { active = false; realign = returnOnBar; }
				else if (!stop && (r.a != a || r.b != b) && beat) { a = r.a; b = r.b; c0 = master; fired = 0; }
			}
			if (!active) steps = 1;
		}
		else since += 1.f;
		if (active) {
			double frac = period > 0.f ? std::min((double) since / period, 0.999) : 0.0;
			double pos = ((double) (master - c0) + frac) * a / b;
			long due = (long) std::floor(pos + 1e-9);
			while (fired <= due) { steps++; fired++; }
		}
		return steps;
	}
};

} // namespace juke
