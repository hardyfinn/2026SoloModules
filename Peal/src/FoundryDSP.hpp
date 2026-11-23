// Bell Foundry sound engine: 12 modal bells (decaying sine partials, each with
// a slightly detuned twin), a clapper click, crack rattle, and a small stereo
// reverb for hearing the tower from further away. No Rack code, so
// test/render.cpp can use it.
#pragma once
#include <cmath>
#include <cstdint>
#include <algorithm>

namespace foundry {

static const int NB = 12;    // bells
static const int NP = 8;     // partials per bell
static const float TWO_PI = 6.28318530718f;

// Partial tables for the three profiles: ratio to the strike note, level, ring time (s).
//   church   hum, prime, tierce (minor 3rd), quint, nominal, deciem, superquint, octave nominal
//   handbell fundamental with a tuned twelfth, thin upper partials
//   gamelan  bonang / gong: strong fundamental, inharmonic upper partials
static const float RATIO[3][NP] = {
	{0.5f, 1.0f, 1.2f, 1.5f, 2.0f, 2.5f, 3.0f, 4.0f},
	{0.5f, 1.0f, 2.4f, 3.0f, 4.5f, 5.3f, 6.6f, 8.0f},
	{0.5f, 1.0f, 1.47f, 2.0f, 2.76f, 3.52f, 4.4f, 5.6f}};
static const float LEVEL[3][NP] = {
	{0.45f, 0.40f, 0.45f, 0.20f, 0.90f, 0.35f, 0.30f, 0.22f},
	{0.00f, 1.00f, 0.12f, 0.60f, 0.10f, 0.25f, 0.08f, 0.06f},
	{0.20f, 1.00f, 0.25f, 0.35f, 0.20f, 0.15f, 0.10f, 0.06f}};
static const float RING[3][NP] = {
	{12.f, 6.f, 5.f, 3.f, 3.5f, 2.f, 1.5f, 1.f},
	{0.1f, 4.f, 1.f, 1.5f, 0.8f, 0.8f, 0.5f, 0.4f},
	{8.f, 7.f, 2.5f, 3.f, 1.5f, 1.f, 0.8f, 0.6f}};

enum Tuning { TRUE_HARMONIC, OLD_BELLS, OMBAK };
enum Tower { CHAMBER, CHURCHYARD, TOWN };

struct Settings {
	float profile = 0.f;    // 0 church .. 1 handbell .. 2 gamelan
	float metal = 0.5f;     // 0 brass, 0.5 bell bronze, 1 steel
	float thickness = 0.5f;
	float size = 0.f;       // octaves
	float crack = 0.f;
	float clapper = 0.5f;   // 0 leather .. 1 iron
	float decay = 1.f;      // ring time multiplier
	int tuning = TRUE_HARMONIC;
};

struct Rng {
	uint32_t s = 22222u;
	float uni() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return (s >> 8) * (1.f / 16777216.f); }
	float bi() { return uni() * 2.f - 1.f; }
};

struct Partial {
	float re = 0.f, im = 0.f;   // oscillator state
	float c = 1.f, s = 0.f;     // rotation per sample (with decay folded in)
	float re2 = 0.f, im2 = 0.f; // the twin
	float c2 = 1.f, s2 = 0.f;
};

// What makes each cast bell a little different (re-rolled by RECAST).
struct Casting {
	float ratioErr[NP];   // small tuning errors per partial
	float oldErr[NP];     // larger errors for OLD_BELLS
	float twin[NP];       // doublet split (fraction of frequency)
	float level[NP];
	float ombakHz;
};

struct Bell {
	Partial p[NP];
	Casting cast;
	float twinLevel = 0.3f;
	float click = 0.f, clickDecay = 0.99f, clickLp = 0.f, clickCoef = 0.5f;
	float env = 0.f;         // follower for the crack rattle and the screen
	float crack = 0.f;
	float pan = 0.5f;
	Rng rng;

	void recast(Rng& r) {
		for (int k = 0; k < NP; k++) {
			cast.ratioErr[k] = r.bi() * 0.004f;
			cast.oldErr[k] = r.bi() * (k <= 2 ? 0.045f : 0.015f);   // hum, prime, tierce wander most
			cast.twin[k] = (0.0004f + 0.0016f * r.uni()) * (r.uni() < 0.5f ? -1.f : 1.f);
			cast.level[k] = 0.8f + 0.4f * r.uni();
		}
		cast.ombakHz = 5.f + 3.f * r.uni();
		rng.s = (uint32_t) (r.uni() * 4e9f) | 1u;
	}

	void strike(float freq, float vel, const Settings& st, float sr) {
		int a = (int) std::floor(std::min(st.profile, 1.999f));
		float f = st.profile - a;
		float brass = std::max(0.f, (0.5f - st.metal) * 2.f);
		float steel = std::max(0.f, (st.metal - 0.5f) * 2.f);
		float th = st.thickness - 0.5f;
		float sizeMul = std::pow(2.f, st.size);
		float baseHz = freq * sizeMul;
		// big, low bells ring longer
		float lowRing = std::sqrt(clampf(262.f / baseHz, 0.25f, 6.f));
		float ringMul = st.decay * lowRing * (1.f - 0.4f * brass) * (1.f - 0.2f * steel) * (1.f + 0.6f * th);
		crack = st.crack;
		for (int k = 0; k < NP; k++) {
			float ratio = RATIO[a][k] + (RATIO[a + 1 < 3 ? a + 1 : 2][k] - RATIO[a][k]) * f;
			float level = LEVEL[a][k] + (LEVEL[a + 1 < 3 ? a + 1 : 2][k] - LEVEL[a][k]) * f;
			float ring = RING[a][k] + (RING[a + 1 < 3 ? a + 1 : 2][k] - RING[a][k]) * f;
			// thickness pushes upper partials up (thick) or down (thin); steel stretches them
			if (ratio > 1.f) ratio = std::pow(ratio, 1.f + 0.08f * th + 0.035f * steel);
			ratio *= 1.f + cast.ratioErr[k];
			if (st.tuning == OLD_BELLS) ratio *= 1.f + cast.oldErr[k];
			level *= cast.level[k] * (1.f - 0.6f * brass * std::min(1.f, k / 5.f)) * (1.f + 0.25f * steel * k);
			// clapper: leather favours low partials, iron the high ones
			level *= std::pow(std::max(ratio, 0.25f), (st.clapper - 0.6f) * 1.4f);
			// a crack kills the high partials early
			ring *= ringMul * (1.f - 0.85f * crack * std::min(1.f, ratio / 4.f));
			float hz = baseHz * ratio;
			float nyq = sr * 0.45f;
			Partial& P = p[k];
			if (hz >= nyq || hz < 10.f) { P.c = 1.f; P.s = 0.f; P.c2 = 1.f; P.s2 = 0.f; continue; }
			float g = std::exp(-6.9078f / (std::max(ring, 0.02f) * sr));   // T60
			float w = TWO_PI * hz / sr;
			P.c = g * std::cos(w); P.s = g * std::sin(w);
			float split = cast.twin[k] + crack * 0.02f * cast.twin[k] / std::fabs(cast.twin[k]) * (0.5f + 0.5f * cast.level[k]);
			float hz2 = hz * (1.f + split);
			if (st.tuning == OMBAK) hz2 = hz + cast.ombakHz * (k < 4 ? 1.f : 0.6f);
			hz2 = std::min(hz2, nyq);
			float w2 = TWO_PI * hz2 / sr;
			P.c2 = g * std::cos(w2); P.s2 = g * std::sin(w2);
			twinLevel = (st.tuning == OMBAK) ? 1.f : 0.3f + 0.7f * crack;
			float amp = level * vel;
			P.re += amp;
			P.re2 += amp * twinLevel;
		}
		// clapper click: soft and long for leather, short and bright for iron
		click += vel * (0.3f + 0.7f * st.clapper);
		clickDecay = std::exp(-1.f / ((0.008f - 0.0065f * st.clapper) * sr));
		float fc = 800.f + 9000.f * st.clapper * st.clapper;
		clickCoef = 1.f - std::exp(-TWO_PI * fc / sr);
	}

	static float clampf(float x, float a, float b) { return x < a ? a : (x > b ? b : x); }

	float process() {
		float out = 0.f;
		for (int k = 0; k < NP; k++) {
			Partial& P = p[k];
			float r = P.re * P.c - P.im * P.s;
			float i = P.re * P.s + P.im * P.c;
			P.re = r; P.im = i;
			float r2 = P.re2 * P.c2 - P.im2 * P.s2;
			float i2 = P.re2 * P.s2 + P.im2 * P.c2;
			P.re2 = r2; P.im2 = i2;
			out += i + i2;
		}
		// click
		if (click > 1e-5f) {
			clickLp += clickCoef * (rng.bi() * click - clickLp);
			out += clickLp * 0.6f;
			click *= clickDecay;
		}
		float a = std::fabs(out);
		env += (a > env ? 0.01f : 0.0005f) * (a - env);
		// crack rattle: noise riding on the bell's own level, gated by its sign
		if (crack > 0.f) out += crack * crack * env * rng.bi() * (out > 0.f ? 1.2f : 0.2f);
		return out * 0.12f;
	}

	void silence() {
		for (int k = 0; k < NP; k++) { p[k].re = p[k].im = p[k].re2 = p[k].im2 = 0.f; }
		click = 0.f; env = 0.f;
	}
};

// Small stereo reverb (Freeverb-style combs + allpasses), sized for the sample rate.
struct Reverb {
	static const int NC = 4, NA = 2;
	float* comb[2][NC] = {};
	int clen[2][NC], cidx[2][NC] = {};
	float cfilt[2][NC] = {};
	float* ap[2][NA] = {};
	int alen[2][NA], aidx[2][NA] = {};
	float* pre[2] = {};
	int plen = 1, pidx = 0;
	bool ready = false;

	void free_() {
		for (int s = 0; s < 2; s++) {
			for (int i = 0; i < NC; i++) { delete[] comb[s][i]; comb[s][i] = nullptr; }
			for (int i = 0; i < NA; i++) { delete[] ap[s][i]; ap[s][i] = nullptr; }
			delete[] pre[s]; pre[s] = nullptr;
		}
		ready = false;
	}
	~Reverb() { free_(); }

	void init(float sr) {
		free_();
		const int cl[NC] = {1116, 1188, 1277, 1356}, al[NA] = {556, 441};
		float k = sr / 44100.f;
		for (int s = 0; s < 2; s++) {
			for (int i = 0; i < NC; i++) {
				clen[s][i] = std::max(8, (int) ((cl[i] + s * 23) * k * 1.6f));
				comb[s][i] = new float[clen[s][i]]();
				cidx[s][i] = 0; cfilt[s][i] = 0.f;
			}
			for (int i = 0; i < NA; i++) {
				alen[s][i] = std::max(8, (int) ((al[i] + s * 23) * k));
				ap[s][i] = new float[alen[s][i]]();
				aidx[s][i] = 0;
			}
			plen = std::max(2, (int) (0.12f * sr));
			pre[s] = new float[plen]();
		}
		pidx = 0;
		ready = true;
	}

	// predelay in samples (< plen), feedback 0..1, damp 0..1
	void process(float inL, float inR, float& outL, float& outR, int preS, float fb, float damp) {
		if (!ready) { outL = outR = 0.f; return; }
		float in[2] = {inL, inR};
		float o[2];
		for (int s = 0; s < 2; s++) {
			pre[s][pidx] = in[s];
			int rd = pidx - std::min(preS, plen - 1);
			if (rd < 0) rd += plen;
			float x = pre[s][rd] * 0.25f;
			float acc = 0.f;
			for (int i = 0; i < NC; i++) {
				float y = comb[s][i][cidx[s][i]];
				cfilt[s][i] = y * (1.f - damp) + cfilt[s][i] * damp;
				comb[s][i][cidx[s][i]] = x + cfilt[s][i] * fb;
				if (++cidx[s][i] >= clen[s][i]) cidx[s][i] = 0;
				acc += y;
			}
			for (int i = 0; i < NA; i++) {
				float b = ap[s][i][aidx[s][i]];
				ap[s][i][aidx[s][i]] = acc + b * 0.5f;
				acc = b - acc;
				if (++aidx[s][i] >= alen[s][i]) aidx[s][i] = 0;
			}
			o[s] = acc;
		}
		if (++pidx >= plen) pidx = 0;
		outL = o[0]; outR = o[1];
	}
};

}  // namespace foundry
