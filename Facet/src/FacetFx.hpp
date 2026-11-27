// Facet patch effects, all written from scratch:
//   group A (in one of 4 orders): distortion (7 types), phaser, 6-band spectrum EQ, enhancer
//   group B: chorus -> 3-tap delay -> reverb (10 types)
//   then a 3-band EQ.
// Parameters are read in JD-style units from the common block of the patch (see FacetParams.hpp).
#pragma once
#include "FacetParams.hpp"
#include <vector>

namespace facet {

struct Biquad {
	float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
	float z1 = 0, z2 = 0;
	inline float process(float x) {
		float y = b0 * x + z1;
		z1 = b1 * x - a1 * y + z2;
		z2 = b2 * x - a2 * y;
		return y;
	}
	void reset() { z1 = z2 = 0; }
	void set(double B0, double B1, double B2, double A0, double A1, double A2) {
		b0 = (float) (B0 / A0);
		b1 = (float) (B1 / A0);
		b2 = (float) (B2 / A0);
		a1 = (float) (A1 / A0);
		a2 = (float) (A2 / A0);
	}
	void peak(double f, double q, double db, double sr) {
		double A = std::pow(10.0, db / 40.0), w = 2 * M_PI * f / sr, al = std::sin(w) / (2 * q), c = std::cos(w);
		set(1 + al * A, -2 * c, 1 - al * A, 1 + al / A, -2 * c, 1 - al / A);
	}
	void lowShelf(double f, double db, double sr) {
		double A = std::pow(10.0, db / 40.0), w = 2 * M_PI * f / sr, c = std::cos(w), al = std::sin(w) / 2 * std::sqrt(2.0);
		double sa = 2 * std::sqrt(A) * al;
		set(A * ((A + 1) - (A - 1) * c + sa), 2 * A * ((A - 1) - (A + 1) * c), A * ((A + 1) - (A - 1) * c - sa),
		    (A + 1) + (A - 1) * c + sa, -2 * ((A - 1) + (A + 1) * c), (A + 1) + (A - 1) * c - sa);
	}
	void highShelf(double f, double db, double sr) {
		double A = std::pow(10.0, db / 40.0), w = 2 * M_PI * f / sr, c = std::cos(w), al = std::sin(w) / 2 * std::sqrt(2.0);
		double sa = 2 * std::sqrt(A) * al;
		set(A * ((A + 1) + (A - 1) * c + sa), -2 * A * ((A - 1) + (A + 1) * c), A * ((A + 1) + (A - 1) * c - sa),
		    (A + 1) - (A - 1) * c + sa, 2 * ((A - 1) - (A + 1) * c), (A + 1) - (A - 1) * c - sa);
	}
	void lowpass(double f, double q, double sr) {
		f = std::min(f, sr * 0.45);
		double w = 2 * M_PI * f / sr, al = std::sin(w) / (2 * q), c = std::cos(w);
		set((1 - c) / 2, 1 - c, (1 - c) / 2, 1 + al, -2 * c, 1 - al);
	}
	void highpass(double f, double q, double sr) {
		double w = 2 * M_PI * f / sr, al = std::sin(w) / (2 * q), c = std::cos(w);
		set((1 + c) / 2, -(1 + c), (1 + c) / 2, 1 + al, -2 * c, 1 - al);
	}
	void bandpass(double f, double q, double sr) {
		double w = 2 * M_PI * f / sr, al = std::sin(w) / (2 * q), c = std::cos(w);
		set(al, 0, -al, 1 + al, -2 * c, 1 - al);
	}
};

struct DelayLine {
	std::vector<float> buf;
	int w = 0, mask = 0;
	void init(int minLen) {
		int n = 1;
		while (n < minLen + 4) n <<= 1;
		buf.assign(n, 0.f);
		mask = n - 1;
		w = 0;
	}
	inline void push(float x) {
		buf[w] = x;
		w = (w + 1) & mask;
	}
	inline float tap(int d) const { return buf[(w - 1 - d) & mask]; }
	inline float tapf(float d) const {
		d = std::max(0.f, std::min((float) (mask - 2), d));
		int i = (int) d;
		float f = d - i;
		float a = tap(i), b = tap(i + 1);
		return a + (b - a) * f;
	}
};

struct Fx {
	float sr = 0.f;
	int ctr = 0;
	float c[NC];
	bool first = true;

	// distortion
	Biquad dsPre[2], dsPost[2];
	float dsGain = 1.f, dsOut = 0.6f;
	int dsType = -1;
	// phaser
	float phAp[2][6] = {}, phFb[2] = {0, 0}, phLfo = 0.f;
	// spectrum
	Biquad sp[2][6];
	float spCache[7] = {99, 99, 99, 99, 99, 99, 99};
	// enhancer
	Biquad ehHp[2];
	float ehLp[2] = {0, 0};
	// chorus
	DelayLine chL, chR;
	float chLfo = 0.f;
	// delay
	DelayLine dl;
	// reverb
	DelayLine rvPre, rvLong, rvLine[8], rvAp[4];
	float rvLp[8] = {};
	float rvGateEnv = 0.f, rvGate = 0.f, rvHold = 0.f;
	int rvLen[8] = {}, rvApLen[4] = {};
	float rvG[8] = {};
	int rvTypeCache = -1;
	float rvSizeCache = -1.f;
	// eq
	Biquad eq[2][3];
	float eqCache[7] = {99, 99, 99, 99, 99, 99, 99};
	// dc
	float dcx[2] = {0, 0}, dcy[2] = {0, 0};

	void setSampleRate(float s) {
		sr = s;
		chL.init((int) (0.07f * s));
		chR.init((int) (0.07f * s));
		dl.init((int) (1.25f * s));
		rvPre.init((int) (0.2f * s));
		rvLong.init((int) (1.6f * s));
		for (int i = 0; i < 8; i++) rvLine[i].init((int) (0.12f * 1.5f * s));
		for (int i = 0; i < 4; i++) rvAp[i].init((int) (0.02f * 1.5f * s));
		first = true;
		dsType = -1;
		rvTypeCache = -1;
		for (float& x : spCache) x = 99;
		for (float& x : eqCache) x = 99;
	}

	// ---------------------------------------------------- coefficients (control rate)
	void update() {
		// distortion
		int type = (int) c[C_DS_TYPE];
		static const float PRE_F[7] = {700, 900, 1100, 600, 800, 250, 900};
		static const float PRE_DB[7] = {0, 3, 9, 0, 2, 6, 4};
		static const float POST_F[7] = {5000, 6500, 4000, 3500, 7000, 3000, 5000};
		if (type != dsType) {
			for (int ch = 0; ch < 2; ch++) {
				dsPre[ch].peak(PRE_F[type], type == 2 ? 2.5 : 0.8, PRE_DB[type], sr);
				dsPost[ch].lowpass(POST_F[type], 0.7, sr);
			}
			dsType = type;
		}
		dsGain = std::pow(10.f, c[C_DS_DRIVE] * 0.02f) * (type >= 3 ? 2.f : 1.f);
		dsOut = c[C_DS_LEVEL] * 0.01f * 0.7f;
		// spectrum
		bool spChanged = c[C_SP_WIDTH] != spCache[6];
		for (int b = 0; b < 6; b++) spChanged |= c[C_SP_B1 + b] != spCache[b];
		if (spChanged) {
			static const float F[6] = {250, 500, 1000, 2000, 4000, 8000};
			static const float Q[5] = {3.0f, 2.0f, 1.4f, 1.0f, 0.7f};
			float q = Q[std::max(0, std::min(4, (int) c[C_SP_WIDTH] - 1))];
			for (int b = 0; b < 6; b++) {
				for (int ch = 0; ch < 2; ch++) sp[ch][b].peak(std::min(F[b], sr * 0.4f), q, c[C_SP_B1 + b], sr);
				spCache[b] = c[C_SP_B1 + b];
			}
			spCache[6] = c[C_SP_WIDTH];
		}
		if (first) {
			for (int ch = 0; ch < 2; ch++) ehHp[ch].highpass(std::min(3000.f, sr * 0.4f), 0.7, sr);
		}
		// eq
		float e[7] = {c[C_EQ_LOF], c[C_EQ_LOG], c[C_EQ_MIDF], c[C_EQ_MIDQ], c[C_EQ_MIDG], c[C_EQ_HIF], c[C_EQ_HIG]};
		bool eqChanged = false;
		for (int i = 0; i < 7; i++) eqChanged |= e[i] != eqCache[i];
		if (eqChanged) {
			for (int ch = 0; ch < 2; ch++) {
				eq[ch][0].lowShelf(e[0] > 0.5f ? 400 : 200, e[1], sr);
				eq[ch][1].peak(std::min(200.0 * std::pow(40.0, e[2] * 0.01), sr * 0.4), MIDQ_VAL[(int) e[3]], e[4], sr);
				eq[ch][2].highShelf(std::min(e[5] > 0.5f ? 8000.f : 4000.f, sr * 0.4f), e[6], sr);
			}
			for (int i = 0; i < 7; i++) eqCache[i] = e[i];
		}
		// reverb
		int rt = (int) c[C_RV_TYPE];
		static const float SIZE[10] = {0.45f, 0.6f, 0.8f, 1.0f, 1.2f, 1.45f, 0.8f, 0.9f, 1.f, 1.f};
		float size = SIZE[rt];
		if (rt != rvTypeCache || size != rvSizeCache) {
			static const float BASE[8] = {29.7f, 37.1f, 41.1f, 43.7f, 53.3f, 59.9f, 67.3f, 73.1f};
			static const float AP[4] = {4.77f, 3.59f, 12.73f, 9.31f};
			for (int i = 0; i < 8; i++) rvLen[i] = std::max(8, (int) (BASE[i] * size * 0.001f * sr));
			for (int i = 0; i < 4; i++) rvApLen[i] = std::max(4, (int) (AP[i] * std::sqrt(size) * 0.001f * sr));
			rvTypeCache = rt;
			rvSizeCache = size;
		}
		float t60;
		if (rt <= 5) t60 = 0.1f * std::pow(200.f, c[C_RV_TIME] * 0.01f);
		else if (rt == 6) t60 = 1.2f;
		else t60 = 0.6f;
		for (int i = 0; i < 8; i++) rvG[i] = std::pow(10.f, -3.f * rvLen[i] / (t60 * sr));
		first = false;
	}

	// ---------------------------------------------------- blocks
	inline float shape(int type, float x) {
		switch (type) {
			case 0: return std::tanh(x);
			case 1: return std::tanh(x + 0.2f) - 0.197f;
			case 2: return std::tanh(1.3f * x);
			case 3: return x / (1.f + std::fabs(x));
			case 4: return std::max(-1.f, std::min(1.f, x)) * 0.9f + 0.1f * std::tanh(x);
			case 5: return std::tanh(1.5f * std::tanh(x) + 0.3f * x);
			default: {
				float y = x > 0.f ? 1.f - std::exp(-x) : -0.6f * (1.f - std::exp(0.8f * x));
				return std::max(-0.8f, std::min(1.f, 1.5f * y));
			}
		}
	}
	void dist(float& L, float& R) {
		float* io[2] = {&L, &R};
		for (int ch = 0; ch < 2; ch++) {
			float x = dsPre[ch].process(*io[ch]) * dsGain;
			*io[ch] = dsPost[ch].process(shape(dsType, x)) * dsOut;
		}
	}
	void phaser(float& L, float& R) {
		float rate = 0.1f * std::pow(100.f, c[C_PH_RATE] * 0.01f);
		phLfo += rate / sr;
		if (phLfo >= 1.f) phLfo -= 1.f;
		float man = 50.f * std::pow(300.f, c[C_PH_MAN] * 0.01f);
		float depth = c[C_PH_DEPTH] * 0.01f * 2.5f;
		float fb = c[C_PH_RESO] * 0.01f * 0.85f;
		float mix = c[C_PH_MIX] * 0.01f;
		float* io[2] = {&L, &R};
		for (int ch = 0; ch < 2; ch++) {
			float lfo = std::sin(2.f * PI_F * (phLfo + ch * 0.25f));
			float f = std::max(20.f, std::min(sr * 0.45f, man * std::exp2(depth * lfo)));
			float t = std::tan(PI_F * f / sr);
			float a = (t - 1.f) / (t + 1.f);
			float dry = *io[ch];
			float x = dry + fb * phFb[ch];
			for (int s = 0; s < 6; s++) {
				float y = a * x + phAp[ch][s];
				phAp[ch][s] = x - a * y;
				x = y;
			}
			phFb[ch] = x;
			*io[ch] = dry * (1.f - mix) + x * mix;
		}
	}
	void spectrum(float& L, float& R) {
		for (int b = 0; b < 6; b++) {
			L = sp[0][b].process(L);
			R = sp[1][b].process(R);
		}
	}
	void enhancer(float& L, float& R) {
		float drive = 1.f + c[C_EH_SENS] * 0.08f;
		float mix = c[C_EH_MIX] * 0.01f * 0.6f;
		float* io[2] = {&L, &R};
		float a = 1.f - std::exp(-2.f * PI_F * 9000.f / sr);
		for (int ch = 0; ch < 2; ch++) {
			float h = ehHp[ch].process(*io[ch]);
			float e = std::tanh(h * drive) / std::sqrt(drive);
			ehLp[ch] += a * (e - ehLp[ch]);
			*io[ch] += mix * ehLp[ch];
		}
	}
	void chorus(float& L, float& R) {
		float rate = 0.1f * std::pow(100.f, c[C_CH_RATE] * 0.01f);
		chLfo += rate / sr;
		if (chLfo >= 1.f) chLfo -= 1.f;
		float baseMs = 0.1f * std::pow(500.f, c[C_CH_DELAY] * 0.01f);
		float modMs = std::min(6.f, c[C_CH_DEPTH] * 0.01f * (0.3f + 0.45f * baseMs));
		float fb = c[C_CH_FB] * 0.01f;
		float lev = c[C_CH_LEVEL] * 0.01f;
		float ms = sr * 0.001f;
		float dL = (baseMs + modMs * (0.5f + 0.5f * std::sin(2.f * PI_F * chLfo))) * ms;
		float dR = (baseMs + modMs * (0.5f + 0.5f * std::cos(2.f * PI_F * chLfo))) * ms;
		float wl = chL.tapf(dL), wr = chR.tapf(dR);
		chL.push(L + fb * wl);
		chR.push(R + fb * wr);
		L += wl * lev;
		R += wr * lev;
	}
	void delay(float& L, float& R) {
		float ms = sr * 0.001f;
		float ct = dl.tapf(c[C_DL_CTIME] * ms - 1.f);
		float lt = dl.tapf(c[C_DL_LTIME] * ms - 1.f);
		float rt = dl.tapf(c[C_DL_RTIME] * ms - 1.f);
		float fb = c[C_DL_FB] * 0.01f;
		float in = 0.5f * (L + R);
		dl.push(in + fb * ct);
		float cl = c[C_DL_CLEV] * 0.01f;
		L += ct * cl + lt * c[C_DL_LLEV] * 0.01f;
		R += ct * cl + rt * c[C_DL_RLEV] * 0.01f;
	}
	void reverb(float& L, float& R) {
		int rt = rvTypeCache;
		float ms = sr * 0.001f;
		float in = 0.5f * (L + R);
		rvPre.push(in);
		float pre = rvPre.tap((int) (c[C_RV_PRE] * ms));
		// input diffusion
		float x = pre;
		for (int i = 0; i < 4; i++) {
			float d = rvAp[i].tap(rvApLen[i]);
			float y = -0.65f * x + d;
			rvAp[i].push(x + 0.65f * y);
			x = y;
		}
		float wl = 0.f, wr = 0.f;
		float damp = HFDAMP_HZ[(int) c[C_RV_HFD]];
		float da = 1.f - std::exp(-2.f * PI_F * std::min(damp, sr * 0.45f) / sr);
		if (rt <= 6) {
			float y[8], s = 0.f;
			for (int i = 0; i < 8; i++) {
				y[i] = rvLine[i].tap(rvLen[i]);
				rvLp[i] += da * (y[i] - rvLp[i]);
				s += rvLp[i];
			}
			s *= 0.25f;
			for (int i = 0; i < 8; i++) rvLine[i].push(x * (i & 1 ? 0.35f : -0.35f) + rvG[i] * (rvLp[i] - s));
			wl = 0.35f * (y[0] - y[2] + y[4] - y[6]);
			wr = 0.35f * (y[1] - y[3] + y[5] - y[7]);
			// early reflections
			static const float ER[6] = {7.1f, 11.3f, 16.9f, 23.7f, 31.1f, 38.3f};
			static const float EG[6] = {0.9f, 0.8f, 0.7f, 0.6f, 0.5f, 0.4f};
			float er = c[C_RV_EARLY] * 0.01f * 0.5f;
			float size = rvSizeCache;
			for (int i = 0; i < 6; i++) {
				float tp = rvPre.tap((int) ((c[C_RV_PRE] + ER[i] * size) * ms));
				if (i & 1) wr += er * EG[i] * tp;
				else wl += er * EG[i] * tp;
			}
			if (rt == 6) {
				// gated: open while the input is playing, hold for TIME, then close fast
				float lenS = (5.f + 495.f * std::pow(c[C_RV_TIME] * 0.01f, 1.3f)) * 0.001f;
				float a = std::fabs(in);
				rvGateEnv = std::max(a, rvGateEnv * (1.f - 1.f / (0.02f * sr)));
				if (rvGateEnv > 0.02f) rvHold = lenS;
				else rvHold -= 1.f / sr;
				float tgt = rvHold > 0.f ? 1.f : 0.f;
				rvGate += (tgt - rvGate) * (tgt > rvGate ? 0.02f : 0.004f);
				wl *= rvGate;
				wr *= rvGate;
			}
		}
		else {
			rvLong.push(x);
			float lenMs = 5.f + 495.f * std::pow(c[C_RV_TIME] * 0.01f, 1.3f);
			if (rt == 7) {
				// reverse: taps that get louder the further back they reach, then stop
				const int N = 24;
				for (int k = 0; k < N; k++) {
					float frac = (k + 1) / (float) N;
					float jit = 0.9f + 0.2f * (float) ((k * 7919) % 97) / 97.f;
					float g = std::pow(frac, 1.5f) * 0.18f;
					float tp = rvLong.tap((int) (lenMs * frac * jit * ms));
					if (k & 1) wr += g * tp;
					else wl += g * tp;
				}
			}
			else {
				// flying: echoes that travel across the stereo field
				const int N = 12;
				for (int k = 0; k < N; k++) {
					float d = lenMs * 3.f * (k + 1) / N;
					float g = 0.5f * std::pow(0.82f, (float) k);
					float pan = rt == 8 ? k / (float) (N - 1) : 0.5f + 0.5f * std::sin(k * 1.3f);
					float tp = rvLong.tap((int) (d * ms));
					wl += g * tp * std::cos(pan * 0.5f * PI_F);
					wr += g * tp * std::sin(pan * 0.5f * PI_F);
				}
			}
			for (int ch = 0; ch < 2; ch++) {
				float& w = ch ? wr : wl;
				rvLp[ch] += da * (w - rvLp[ch]);
				w = rvLp[ch];
			}
		}
		float lev = c[C_RV_LEVEL] * 0.01f;
		L += wl * lev;
		R += wr * lev;
	}

	void process(const float* common, float& L, float& R) {
		if (++ctr >= 32 || first) {
			ctr = 0;
			std::memcpy(c, common, sizeof(c));
			update();
		}
		static const int ORDER[4][4] = {{0, 1, 2, 3}, {0, 2, 1, 3}, {3, 0, 1, 2}, {2, 3, 0, 1}};
		int o = std::max(0, std::min(3, (int) c[C_ORDER]));
		for (int s = 0; s < 4; s++) {
			switch (ORDER[o][s]) {
				case 0: if (c[C_DS_ON] > 0.5f) dist(L, R); break;
				case 1: if (c[C_PH_ON] > 0.5f) phaser(L, R); break;
				case 2: if (c[C_SP_ON] > 0.5f) spectrum(L, R); break;
				case 3: if (c[C_EH_ON] > 0.5f) enhancer(L, R); break;
			}
		}
		if (c[C_CH_ON] > 0.5f) chorus(L, R);
		if (c[C_DL_ON] > 0.5f) delay(L, R);
		else dl.push(0.f);
		if (c[C_RV_ON] > 0.5f) reverb(L, R);
		for (int b = 0; b < 3; b++) {
			L = eq[0][b].process(L);
			R = eq[1][b].process(R);
		}
		// DC blocker
		float* io[2] = {&L, &R};
		for (int ch = 0; ch < 2; ch++) {
			float y = *io[ch] - dcx[ch] + 0.9995f * dcy[ch];
			dcx[ch] = *io[ch];
			dcy[ch] = y;
			*io[ch] = y;
		}
	}
};

} // namespace facet
