// Riddim engine: ten synthesised drum voices, eight "kit flavours", and the
// pattern stepper (variations, mutate, fills). No Rack code in here, so
// test/render.cpp can run it offline.
#pragma once
#include <cmath>
#include <cstdint>
#include <algorithm>
#include "RiddimPatterns.hpp"

namespace riddim {

static const float PI_F = 3.14159265358979f;
enum Track { KICK, SNARE, RIM, CLAP, TOM, CHAT, OHAT, COW, PERC, SHAKE };

static const char* const KIT_NAMES[9] = {"AUTO", "CASIO", "ROLAND 808", "ROLAND PCM", "LINN", "SIMMONS", "SAMPLER", "WORKSTATION", "LIVE"};

struct Rng {
	uint32_t s = 12345u;
	float bi() { s = s * 1664525u + 1013904223u; return (float) (int32_t)(s) / 2147483648.f; }
	float uni() { return bi() * 0.5f + 0.5f; }
};

// How each machine flavour shapes the voices. Kit index 0..7 (CASIO .. LIVE).
struct KitParams {
	float kickF, kickSweep, kickDec, kickClick;
	float snareF, snareNoise, snareDec;
	float tomF, tomSweep, tomDec;
	float hatDecC, hatDecO, hatBright;
	float cowF;
	float clapDec;
	int bits, srDiv;
	float drive;
};
static const KitParams KITS[8] = {
	// kickF sweep dec click | snareF noise dec | tomF sweep dec | hatC hatO bright | cow | clap | bits srDiv drive
	{ 90.f, 140.f, 0.14f, 0.15f,  190.f, 0.80f, 0.11f,  150.f,  60.f, 0.16f, 0.025f, 0.16f, 0.75f, 520.f, 0.08f,  8, 3, 0.35f}, // CASIO
	{ 48.f,  90.f, 0.42f, 0.05f,  175.f, 0.55f, 0.16f,  120.f,  40.f, 0.30f, 0.040f, 0.30f, 0.70f, 540.f, 0.14f, 16, 1, 0.20f}, // ROLAND 808
	{ 60.f, 120.f, 0.22f, 0.12f,  200.f, 0.65f, 0.14f,  140.f,  80.f, 0.22f, 0.035f, 0.22f, 0.85f, 560.f, 0.12f, 12, 1, 0.25f}, // ROLAND PCM
	{ 62.f, 130.f, 0.20f, 0.18f,  210.f, 0.60f, 0.15f,  160.f, 110.f, 0.20f, 0.030f, 0.20f, 0.80f, 600.f, 0.11f,  8, 2, 0.30f}, // LINN / OBERHEIM
	{ 55.f, 200.f, 0.34f, 0.10f,  230.f, 0.85f, 0.20f,  170.f, 260.f, 0.30f, 0.032f, 0.22f, 0.70f, 500.f, 0.16f, 12, 1, 0.35f}, // SIMMONS
	{ 58.f, 110.f, 0.26f, 0.12f,  195.f, 0.65f, 0.15f,  135.f,  70.f, 0.22f, 0.032f, 0.20f, 0.65f, 540.f, 0.13f, 12, 2, 0.30f}, // SAMPLER (SP-1200 grit)
	{ 52.f, 100.f, 0.38f, 0.08f,  185.f, 0.60f, 0.17f,  125.f,  50.f, 0.28f, 0.036f, 0.26f, 0.90f, 560.f, 0.15f, 16, 1, 0.15f}, // WORKSTATION
	{ 70.f,  60.f, 0.12f, 0.20f,  170.f, 0.70f, 0.18f,  115.f,  30.f, 0.28f, 0.040f, 0.30f, 0.55f, 480.f, 0.10f, 16, 1, 0.10f}, // LIVE
};

struct Ctl {
	float tune = 1.f;     // frequency ratio
	float decay = 1.f;    // decay scale
	float tone = 0.5f;    // 0 dark .. 1 bright
	float crush = 0.f;    // 0..1
	float drive = 0.f;    // 0..1
	int kit = 1;          // 0..7 resolved flavour
};

static inline float expDecay(float t, float tau) { return std::exp(-t / std::max(tau, 1e-4f)); }

struct Crusher {
	float held = 0.f; int count = 0;
	float process(float x, int bits, int div) {
		if (++count >= div) { count = 0; held = x; }
		float q = std::pow(2.f, (float) bits - 1.f);
		return std::round(held * q) / q;
	}
};

struct Voice {
	float t = 1e6f, vel = 0.f, phase = 0.f, lvl = 1.f;
	float p2 = 0.f, p3 = 0.f;
	float lp = 0.f, hp = 0.f, hp2 = 0.f, prev = 0.f;
	float bp1 = 0.f, bp2 = 0.f;
	float hph[6] = {0, 0, 0, 0, 0, 0};
	float pitchIdx = 1.f;
	bool active() const { return t < 2.f; }
	void trig(float v, float pitch = 1.f) { t = 0.f; vel = v; phase = 0.f; p2 = 0.f; p3 = 0.f; pitchIdx = pitch; }
};

struct DrumEngine {
	float sr = 48000.f;
	Voice v[10];
	Crusher crush[10];
	Rng rng;
	void setSampleRate(float s) { sr = s; }

	static float vel(int level) { return level <= 1 ? 0.42f : (level == 2 ? 0.8f : 1.f); }

	void hit(int track, int level, const Ctl&) {
		float vv = vel(level);
		if (track == TOM) {
			static const float pi[4] = {1.f, 0.72f, 1.f, 1.45f};
			v[TOM].trig(0.9f, pi[std::min(level, 3)]);
		}
		else if (track == PERC) v[PERC].trig(vv, level == 1 ? 0.85f : (level == 3 ? 1.25f : 1.f));
		else v[track].trig(vv);
		if (track == CHAT) v[OHAT].t = 2.f;  // closed hat chokes the open one
	}

	float noise() { return rng.bi(); }

	float voiceSample(int i, const Ctl& c, const KitParams& k) {
		Voice& x = v[i];
		float dt = 1.f / sr, t = x.t;
		float d = c.decay, tn = c.tune, out = 0.f;
		switch (i) {
		case KICK: {
			float f = (k.kickF + k.kickSweep * expDecay(t, 0.028f)) * tn;
			x.phase += f * dt; x.phase -= std::floor(x.phase);
			float s = std::sin(2.f * PI_F * x.phase);
			out = s * expDecay(t, k.kickDec * d) + noise() * k.kickClick * expDecay(t, 0.003f);
			out = std::tanh(out * 1.6f) * 0.9f;
		} break;
		case SNARE: {
			float f1 = k.snareF * tn, f2 = k.snareF * 1.75f * tn;
			x.p2 += f1 * dt; x.p2 -= std::floor(x.p2);
			x.p3 += f2 * dt; x.p3 -= std::floor(x.p3);
			float tone = (std::sin(2.f * PI_F * x.p2) + 0.6f * std::sin(2.f * PI_F * x.p3)) * expDecay(t, 0.07f * d);
			float n = noise();
			float a = 0.08f + 0.30f * c.tone;              // noise high-pass amount
			x.hp += a * (n - x.hp);
			float nh = n - x.hp;
			float noi = nh * expDecay(t, k.snareDec * 0.5f * d);
			out = tone * (1.f - k.snareNoise) * 0.6f + noi * (0.35f + k.snareNoise * 0.65f);
		} break;
		case RIM: {
			x.p2 += 480.f * tn * dt; x.p2 -= std::floor(x.p2);
			x.p3 += 1750.f * tn * dt; x.p3 -= std::floor(x.p3);
			out = (std::sin(2.f * PI_F * x.p2) + 0.7f * std::sin(2.f * PI_F * x.p3)) * expDecay(t, 0.013f * d) * 0.8f
			    + noise() * 0.4f * expDecay(t, 0.002f);
		} break;
		case CLAP: {
			float n = noise();
			float f = 0.18f + 0.1f * c.tone;                 // band-pass about 1.2 kHz
			x.bp1 += f * (n - x.bp1); x.bp2 += f * (x.bp1 - x.bp2);
			float band = (x.bp1 - x.bp2) * 3.5f;
			float env = 0.f;
			for (int b = 0; b < 3; b++) {
				float tb = t - 0.010f * b;
				if (tb >= 0.f) env = std::max(env, expDecay(tb, 0.0035f));
			}
			env = std::max(env, t > 0.027f ? 0.9f * expDecay(t - 0.027f, k.clapDec * 0.6f * d) : 0.f);
			out = band * env;
		} break;
		case TOM: {
			float base = k.tomF * x.pitchIdx * tn;
			float f = base + k.tomSweep * x.pitchIdx * expDecay(t, 0.05f);
			x.phase += f * dt; x.phase -= std::floor(x.phase);
			out = std::sin(2.f * PI_F * x.phase) * expDecay(t, k.tomDec * 0.7f * d) * 0.95f
			    + noise() * 0.25f * expDecay(t, 0.004f);
		} break;
		case CHAT: case OHAT: {
			static const float fr[6] = {205.3f, 369.6f, 304.4f, 522.7f, 540.f, 800.f};
			float s = 0.f;
			float* ph = &x.hph[0];
			for (int n = 0; n < 6; n++) {
				ph[n] += fr[n] * 2.f * tn * dt; ph[n] -= std::floor(ph[n]);
				s += ph[n] < 0.5f ? 1.f : -1.f;
			}
			s = s * 0.18f;
			float a = 0.45f + 0.5f * k.hatBright * (0.4f + 0.6f * c.tone);
			x.hp += a * (s - x.hp); float h1 = s - x.hp;
			x.hp2 += a * (h1 - x.hp2); float h2 = h1 - x.hp2;
			float dec = (i == CHAT ? k.hatDecC : k.hatDecO) * d;
			out = (h2 + noise() * 0.12f) * expDecay(t, dec);
		} break;
		case COW: {
			x.p2 += k.cowF * tn * dt; x.p2 -= std::floor(x.p2);
			x.p3 += k.cowF * 1.48f * tn * dt; x.p3 -= std::floor(x.p3);
			float s = (x.p2 < 0.5f ? 1.f : -1.f) + (x.p3 < 0.5f ? 1.f : -1.f);
			x.bp1 += 0.12f * (s - x.bp1);
			x.hp += 0.03f * (x.bp1 - x.hp);
			float b = (x.bp1 - x.hp) * 1.6f;
			out = b * (expDecay(t, 0.015f) * 0.6f + expDecay(t, 0.10f * d) * 0.5f);
		} break;
		case PERC: {
			float f = (190.f + 110.f * expDecay(t, 0.02f)) * x.pitchIdx * tn;
			x.phase += f * dt; x.phase -= std::floor(x.phase);
			out = std::sin(2.f * PI_F * x.phase) * expDecay(t, 0.11f * d)
			    + noise() * 0.3f * expDecay(t, 0.006f);
		} break;
		case SHAKE: {
			float n = noise();
			x.hp += 0.35f * (n - x.hp);
			float h = n - x.hp;
			float env = std::min(1.f, t / 0.006f) * expDecay(t, 0.05f * d);
			out = h * env * 0.7f;
		} break;
		}
		x.t += dt;
		return out * x.vel;
	}

	// out[10] get each voice, post crush/drive. Hat voices etc. all at about +-1.
	void process(const Ctl& c, float* out) {
		const KitParams& k = KITS[std::min(std::max(c.kit, 0), 7)];
		int bits = (int) std::round(k.bits + (4.f - k.bits) * c.crush);
		bits = std::max(4, bits);
		int div = std::max(1, (int) std::round(k.srDiv * (1.f + c.crush * 5.f)));
		float dr = std::min(1.f, k.drive + c.drive);
		for (int i = 0; i < 10; i++) {
			float s = v[i].active() ? voiceSample(i, c, k) : 0.f;
			if (bits < 16 || div > 1) s = crush[i].process(s, bits, div);
			s = std::max(-1.f, std::min(1.f, s));
			s = std::tanh(s * (1.f + dr * 3.f)) / std::tanh(1.f + dr * 3.f);
			out[i] = s;
		}
	}
};

// ---- the stepper ----
struct Seq {
	int step = 0, bar = 0;
	bool fillArmed = false;
	int autoFill = 0;    // 0 off, else fill every N bars
	Rng rng;

	void reset() { step = 0; bar = 0; fillArmed = false; }

	static void fillTemplate(int kind, int stepIn, uint8_t* lv) {
		// stepIn 12..15 : override the last beat with a fill
		int j = stepIn - 12;
		for (int t = 0; t < 10; t++) if (t != CHAT && t != OHAT) lv[t] = 0;
		switch (kind & 3) {
		case 0: { static const uint8_t s[4] = {1, 2, 2, 3}; lv[SNARE] = s[j]; if (j == 0) lv[KICK] = 3; } break;
		case 1: { static const uint8_t t[4] = {3, 2, 1, 1}; lv[TOM] = t[j]; if (j == 0) lv[KICK] = 3; } break;
		case 2: { lv[RIM] = j == 3 ? 3 : 2; if (j == 3) lv[CLAP] = 3; if (j == 0) lv[KICK] = 2; } break;
		default: { static const uint8_t k[4] = {3, 0, 2, 0}; static const uint8_t s[4] = {0, 1, 0, 3}; lv[KICK] = k[j]; lv[SNARE] = s[j]; } break;
		}
	}

	// Compute this step's hit levels into lv[10] (0 = no hit), then advance.
	void next(int riddim, int var, bool varGate, float mutate, bool fillNow, uint8_t* lv) {
		riddim = std::min(std::max(riddim, 0), NUM_RIDDIMS - 1);
		const RiddimDef& r = RIDDIMS[riddim];
		bool useB = var == 2 || (var == 1 && (bar % 4) == 3) || varGate;
		for (int t = 0; t < 10; t++) lv[t] = useB ? r.b[t][step] : r.a[t][step];

		if (mutate > 0.f) {
			// extra ghost notes and dropped soft hits; fresh every step, so the groove wobbles
			static const float addP[10] = {0.30f, 0.15f, 0.30f, 0.05f, 0.10f, 0.45f, 0.08f, 0.20f, 0.35f, 0.35f};
			static const float dropP[10] = {0.f, 0.f, 0.3f, 0.f, 0.3f, 0.30f, 0.f, 0.3f, 0.3f, 0.3f};
			for (int t = 0; t < 10; t++) {
				float u = rng.uni();
				if (lv[t] == 0) { if (u < mutate * addP[t] * 0.6f) lv[t] = (t == TOM) ? (uint8_t)(1 + (int)(rng.uni() * 2.99f)) : 1; }
				else if (lv[t] < 3 && u < mutate * dropP[t] * 0.6f) lv[t] = 0;
			}
		}
		bool doFill = fillNow || fillArmed || (autoFill > 0 && ((bar + 1) % autoFill) == 0);
		if (doFill && step >= 12) fillTemplate(bar / 2 + (int) (r.name[0]), step, lv);

		step++;
		if (step >= NUM_STEPS) { step = 0; bar++; fillArmed = false; }
	}
};

} // namespace riddim
