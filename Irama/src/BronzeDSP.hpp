// BronzeDSP: modal synthesis for gamelan bronze (gongs, bars, kettles).
// No Rack dependency (test/ renders it offline). Design: docs/instruments-design.md
//
//   Res     complex one-pole resonator: one partial
//   Voice   one struck object (a bar, a kettle, a gong): up to 24 partials,
//           bloom (slow swell of flagged partials), pitch glide, choke (hand damping)
//   Click   short filtered noise burst (mallet contact, rim click)
#pragma once
#include <cmath>
#include <cstdint>
#include <cstring>
#include <algorithm>

namespace bronze {

static const float TWO_PI = 6.28318531f;

struct Rng {
	uint32_t s;
	Rng() : s(0x2545F491u) {}
	uint32_t next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
	float uni() { return (next() >> 8) * (1.f / 16777216.f); }
	float bi() { return uni() * 2.f - 1.f; }
};

// deterministic per-bar tuning error in [-1, 1] for a pitch (quarter-tone buckets)
static inline float setError(uint32_t seed, float hz, int salt) {
	int key = (int)std::floor(std::log2(std::max(hz, 1.f) / 16.35f) * 48.f + 0.5f);
	uint32_t h = seed ^ (uint32_t)(key * 2654435761u) ^ (uint32_t)(salt * 40503u);
	h ^= h >> 15; h *= 0x2C1B3C6Du; h ^= h >> 12; h *= 0x297A2D39u; h ^= h >> 15;
	return (h >> 8) * (2.f / 16777216.f) - 1.f;
}

// mallet low-pass: contact time (s) -> gain at frequency f
static inline float mallet(float f, float contact) {
	float x = f * contact;
	return std::exp(-1.2f * x * x);
}

// contact time between two values, log interpolated by hardness 0..1
static inline float contactTime(float softS, float hardS, float hard) {
	return softS * std::pow(hardS / softS, std::min(std::max(hard, 0.f), 1.f));
}

struct Res {
	float re, im, cr, ci;
	float f, T;
	bool bloom, sub;
};

// a partial description handed to Voice::strike
struct Partial {
	float f, T, a;
	bool bloom, sub;
};

struct Voice {
	static const int MAXR = 24;
	Res r[MAXR];
	int n;
	float freq;          // nominal pitch, for allocation
	float env, peak, envCoef;
	float bloomEnv, bloomCoef;
	float glide, glideNow;
	int glideCount;
	int chokeDelay;
	float pendingChoke;
	float sr;
	float pan;
	int tag;             // owner info (part / slot)

	Voice() { reset(48000.f); }
	void reset(float sampleRate) {
		std::memset(r, 0, sizeof(r));
		n = 0; freq = 0.f; env = 0.f; peak = 1e-6f; sr = sampleRate;
		envCoef = std::exp(-1.f / (0.05f * sr));
		bloomEnv = 1.f; bloomCoef = 0.f; glide = 0.f; glideNow = 1.f; glideCount = 0;
		chokeDelay = -1; pendingChoke = 0.f; pan = 0.f; tag = 0;
	}
	bool active() const { return env > 2e-5f; }

	void coef(Res& x, float mul) {
		float f = x.f * mul;
		if (f <= 0.f || f >= sr * 0.45f) { x.cr = 0.f; x.ci = 0.f; return; }
		float g = std::exp(-1.f / (std::max(x.T, 1e-4f) * sr));
		float w = TWO_PI * f / sr;
		x.cr = g * std::cos(w); x.ci = g * std::sin(w);
	}

	// strike with a list of partials. If the voice is still ringing with the same
	// partial layout it is re-struck (energy adds), otherwise it starts fresh.
	// bloomTime > 0 makes bloom-flagged partials swell in over that time.
	void strike(const Partial* p, int count, float nominal, float bloomTime, float glideAmt) {
		count = std::min(count, MAXR);
		bool same = active() && count == n;
		if (!same) { std::memset(r, 0, sizeof(r)); bloomEnv = 0.f; }
		else bloomEnv *= 0.5f;
		n = count; freq = nominal;
		glide = glideAmt; glideNow = 1.f + glide; glideCount = 0;
		bloomCoef = bloomTime > 0.f ? std::exp(-1.f / (bloomTime * sr)) : 0.f;
		if (bloomTime <= 0.f) bloomEnv = 1.f;
		chokeDelay = -1;
		float pk = 0.f;
		for (int i = 0; i < n; i++) {
			Res& x = r[i];
			x.f = p[i].f; x.T = p[i].T; x.bloom = p[i].bloom; x.sub = p[i].sub;
			coef(x, glideNow);
			if (x.cr == 0.f && x.ci == 0.f) continue;
			x.re += p[i].a;
			pk += std::fabs(p[i].a);
		}
		peak = std::max(pk, 1e-4f);
		env = std::max(env, peak);
	}

	// hand damping: every partial's decay shortened to at most t seconds
	void choke(float t) {
		for (int i = 0; i < n; i++) {
			if (r[i].T > t) { r[i].T = t; coef(r[i], glideNow); }
		}
	}
	void chokeLater(float t, int samples) { pendingChoke = t; chokeDelay = samples; }

	// true level of the resonators (independent of the output follower)
	float level() const {
		float e = 0.f;
		for (int i = 0; i < n; i++) e += r[i].re * r[i].re + r[i].im * r[i].im;
		return std::sqrt(e);
	}

	// small extra excitation (sympathetic resonance)
	void nudge(float a) {
		for (int i = 0; i < n; i++) r[i].re += a / (float)(i + 1);
	}

	inline float process(float& sub) {
		sub = 0.f;
		if (!active()) return 0.f;
		if (chokeDelay > 0 && --chokeDelay == 0) choke(pendingChoke);
		if (glide > 0.f && ++glideCount >= 32) {
			glideCount = 0;
			float e = std::min(env / peak, 1.f);
			glideNow = 1.f + glide * e * e;
			for (int i = 0; i < n; i++) coef(r[i], glideNow);
		}
		bloomEnv = 1.f - (1.f - bloomEnv) * bloomCoef;
		float a = 0.f, b = 0.f, s = 0.f;
		for (int i = 0; i < n; i++) {
			Res& x = r[i];
			float nr = x.cr * x.re - x.ci * x.im;
			float ni = x.ci * x.re + x.cr * x.im;
			x.re = nr; x.im = ni;
			if (x.bloom) b += ni; else a += ni;
			if (x.sub) s += x.bloom ? ni * bloomEnv : ni;
		}
		float out = a + b * bloomEnv;
		float m = std::fabs(out) + std::fabs(b) * 0.5f;
		env = std::max(m, env * envCoef);
		if (!active()) { for (int i = 0; i < n; i++) { r[i].re = 0.f; r[i].im = 0.f; } env = 0.f; }
		sub = s;
		return out;
	}
};

// find a voice for pitch hz: the same bar if it is ringing (within 8 cents),
// else a silent one, else the quietest
static inline int allocVoice(Voice* v, int count, float hz) {
	for (int i = 0; i < count; i++)
		if (v[i].active() && v[i].freq > 0.f && std::fabs(std::log2(v[i].freq / hz)) * 1200.f < 8.f) return i;
	for (int i = 0; i < count; i++) if (!v[i].active()) return i;
	int q = 0;
	for (int i = 1; i < count; i++) if (v[i].env < v[q].env) q = i;
	return q;
}

struct Click {
	float amp, coef, hp, lp, hpC, lpC, prev;
	Rng rng;
	Click() : amp(0), coef(0), hp(0), lp(0), hpC(0), lpC(0), prev(0) {}
	void trigger(float a, float tau, float hpHz, float lpHz, float sr) {
		amp = a; coef = std::exp(-1.f / (tau * sr));
		hpC = std::exp(-TWO_PI * hpHz / sr);
		lpC = std::exp(-TWO_PI * lpHz / sr);
	}
	inline float process() {
		if (amp < 1e-5f) return 0.f;
		float x = rng.bi() * amp;
		amp *= coef;
		// one-pole high-pass then low-pass
		hp = hpC * (hp + x - prev); prev = x;
		lp = lp * lpC + hp * (1.f - lpC);
		return lp;
	}
};

static inline void panGains(float pan, float& gl, float& gr) {
	float a = (std::min(std::max(pan, -1.f), 1.f) + 1.f) * 0.78539816f;
	gl = std::cos(a) * 1.41421356f; gr = std::sin(a) * 1.41421356f;
}

static inline float softLimit(float x) { return 8.f * std::tanh(x * 0.125f); }

} // namespace bronze
