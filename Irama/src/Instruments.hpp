// Instrument engines for Ageng (gong pair + kempul), Gangsa (polos + sangsih,
// umbang + isep) and Reyong (two kettle players + kenong and kethuk).
// No Rack dependency. Design: docs/instruments-design.md
#pragma once
#include "BronzeDSP.hpp"

namespace bronze {

static const float VOLTS = 5.f;   // a normal strike peaks around 5 V

// =====================================================================  AGENG
struct AgengParams {
	float tuneHz, interval, size, bloom, ombak, mallet, bright, kempulRing, gongLevel, kempulLevel;
	AgengParams() : tuneHz(49.f), interval(3.f), size(0.5f), bloom(0.4f), ombak(1.6f), mallet(0.35f),
		bright(0.5f), kempulRing(2.5f), gongLevel(1.f), kempulLevel(0.8f) {}
};

struct AgengEngine {
	static const int NK = 4;
	Voice gong[2], kemp[NK];
	Click thud;
	float sr;
	AgengEngine() : sr(48000.f) {}
	void setSampleRate(float s) {
		sr = s;
		for (int i = 0; i < 2; i++) gong[i].reset(s);
		for (int i = 0; i < NK; i++) kemp[i].reset(s);
	}

	int build(Partial* p, float f0, float T0, float vel, const AgengParams& q, bool isKempul) {
		const float hard = q.mallet;
		const float contact = contactTime(0.006f, 0.0008f, hard);
		const float A = VOLTS * vel;
		int k = 0;
		const float omb = q.ombak * (isKempul ? 1.4f : 1.f);
		const float oct = 2.0f + 0.04f * q.size;
		// ratio, gain, decay multiplier, twin Hz, bloom, sub
		struct M { float r, g, d, tw; bool bl, sub; };
		M m[8] = {
			{1.00f, 1.00f, 1.00f, omb, true, true},
			{oct, 0.55f, 0.60f, omb * 1.6f, true, false},
			{2.96f, 0.22f, 0.35f, 0.f, false, false},
			{4.02f, 0.14f, 0.25f, 0.f, false, false},
			{5.40f, 0.08f * q.bright * 2.f * (0.5f + q.size), 0.12f, 0.f, false, false},
			{6.80f, 0.05f * q.bright * 2.f * (0.5f + q.size), 0.12f, 0.f, false, false},
			{8.30f, 0.03f * q.bright * 2.f * (0.5f + q.size), 0.12f, 0.f, false, false},
			{1.48f, isKempul ? 0.f : 0.10f * hard * hard * 2.f, 0.20f, 0.7f, false, false},
		};
		for (int i = 0; i < 8; i++) {
			if (m[i].g <= 0.f) continue;
			float f = f0 * m[i].r;
			float a = A * m[i].g * mallet(f, contact);
			if (m[i].tw > 0.f) {
				Partial x = {f, T0 * m[i].d, a * 0.5f, m[i].bl, m[i].sub};
				Partial y = {f + m[i].tw, T0 * m[i].d * 0.97f, a * 0.5f, m[i].bl, m[i].sub};
				p[k++] = x; p[k++] = y;
			}
			else { Partial x = {f, T0 * m[i].d, a, m[i].bl, m[i].sub}; p[k++] = x; }
		}
		return k;
	}

	void strikeGong(int which, float vel, const AgengParams& q) {
		Partial p[Voice::MAXR];
		float f0 = q.tuneHz * (which ? std::pow(2.f, q.interval / 12.f) : 1.f);
		float T0 = (6.f + 24.f * q.size) * (which ? 0.8f : 1.f);
		int n = build(p, f0, T0, vel * q.gongLevel, q, false);
		gong[which].pan = which ? 0.3f : -0.3f;
		float tb = 0.02f + 1.18f * q.bloom;
		gong[which].strike(p, n, f0, tb, 0.012f * vel * q.mallet);
		thud.trigger(0.25f * vel * q.gongLevel * (0.3f + q.mallet), 0.004f, 30.f, 600.f + 3000.f * q.mallet, sr);
	}

	void strikeKempul(float hz, float vel, const AgengParams& q) {
		Partial p[Voice::MAXR];
		int n = build(p, hz, q.kempulRing, vel * q.kempulLevel, q, true);
		int i = allocVoice(kemp, NK, hz);
		kemp[i].pan = 0.f;
		kemp[i].strike(p, n, hz, (0.02f + 1.18f * q.bloom) * 0.25f, 0.008f * vel * q.mallet);
	}

	void damp() {
		for (int i = 0; i < 2; i++) gong[i].choke(0.3f);
		for (int i = 0; i < NK; i++) kemp[i].choke(0.15f);
	}

	void process(float& L, float& R, float& sub) {
		L = R = sub = 0.f;
		float s;
		for (int i = 0; i < 2; i++) {
			float o = gong[i].process(s);
			float gl, gr; panGains(gong[i].pan, gl, gr);
			L += o * gl; R += o * gr; sub += s;
		}
		for (int i = 0; i < NK; i++) {
			float o = kemp[i].process(s);
			L += o; R += o;
		}
		float t = thud.process();
		L += t; R += t;
		L = softLimit(L); R = softLimit(R); sub = softLimit(sub);
	}
};

// =====================================================================  GANGSA
struct GangsaParams {
	float ombak, kantilan, mallet, damp, ring, bright, tube, set, width;
	bool dampOnGate;
	uint32_t seed;
	GangsaParams() : ombak(6.5f), kantilan(0.4f), mallet(0.7f), damp(0.75f), ring(1.8f), bright(0.6f),
		tube(0.5f), set(0.3f), width(0.6f), dampOnGate(false), seed(12345u) {}
};

struct GangsaEngine {
	static const int NV = 8;
	Voice v[2][NV];
	Voice thud[2];
	Click click[2];
	int last[2];
	float sr;
	GangsaEngine() : sr(48000.f) { last[0] = last[1] = -1; }
	void setSampleRate(float s) {
		sr = s;
		for (int p = 0; p < 2; p++) { for (int i = 0; i < NV; i++) v[p][i].reset(s); thud[p].reset(s); }
	}

	static float chokeTime(float d) {
		if (d < 0.05f) return -1.f;
		return 0.4f * std::pow(0.04f, (d - 0.05f) / 0.95f);   // 400 ms -> 16 ms
	}

	int bar(Partial* p, int k, float f0, float T0, float A, float contact, const GangsaParams& q, float err) {
		float f = f0 * std::pow(2.f, err * q.set * 12.f / 1200.f);
		struct M { float r, g, d; };
		M m[5] = {
			{1.00f, 1.00f, 1.f + 0.5f * q.tube},
			{1.004f, 0.25f * q.tube, 0.25f},
			{2.71f, 0.30f * q.bright * 1.6f, 0.28f},
			{5.15f, 0.14f * q.bright * 1.6f, 0.12f},
			{8.30f, 0.06f * q.bright * 1.6f, 0.06f},
		};
		for (int i = 0; i < 5; i++) {
			if (m[i].g <= 0.f) continue;
			float ff = f * m[i].r;
			Partial x = {ff, T0 * m[i].d, A * m[i].g * mallet(ff, contact) * (i == 1 ? -1.f : 1.f), false, false};
			p[k++] = x;
		}
		return k;
	}

	// isepHz <= 0: isep = umbang + ombak Hz
	void strike(int part, float hz, float isepHz, float vel, const GangsaParams& q) {
		hz = std::max(hz, 20.f);
		if (isepHz <= 0.f) isepHz = hz + q.ombak;
		const float contact = contactTime(0.0025f, 0.00025f, q.mallet);
		const float A = VOLTS * vel * 0.55f;
		Partial p[Voice::MAXR];
		int k = 0;
		k = bar(p, k, hz, q.ring, A, contact, q, setError(q.seed, hz, 1));
		if (std::fabs(isepHz - hz) > 0.01f) k = bar(p, k, isepHz, q.ring * 0.98f, A, contact, q, setError(q.seed, hz, 2));
		if (q.kantilan > 0.01f) {
			float kh = hz * 2.f, ki = kh + (isepHz - hz);
			k = bar(p, k, kh, q.ring * 0.6f, A * q.kantilan, contact, q, setError(q.seed, kh, 3));
			if (std::fabs(isepHz - hz) > 0.01f) k = bar(p, k, ki, q.ring * 0.58f, A * q.kantilan, contact, q, setError(q.seed, kh, 4));
		}
		int i = allocVoice(v[part], NV, hz);
		float ct = chokeTime(q.damp);
		if (!q.dampOnGate && last[part] >= 0 && last[part] != i && ct > 0.f) {
			v[part][last[part]].choke(ct);
			if (q.damp > 0.5f && v[part][last[part]].active()) {
				Partial t = {180.f, 0.04f, 0.15f * q.damp * vel, false, false};
				thud[part].strike(&t, 1, 180.f, 0.f, 0.f);
			}
		}
		v[part][i].pan = (part ? 1.f : -1.f) * q.width * 0.7f;
		v[part][i].strike(p, k, hz, 0.f, 0.004f * q.mallet * vel);
		last[part] = i;
		click[part].trigger(0.5f * q.mallet * q.mallet * vel, 0.0015f, 1500.f, 9000.f, sr);
	}

	void release(int part, const GangsaParams& q) {
		float ct = chokeTime(q.damp);
		if (q.dampOnGate && last[part] >= 0 && ct > 0.f) v[part][last[part]].choke(ct);
	}

	void process(float& L, float& R, float& polos, float& sangsih) {
		L = R = 0.f;
		float part[2] = {0.f, 0.f};
		float s;
		for (int p = 0; p < 2; p++) {
			for (int i = 0; i < NV; i++) {
				float o = v[p][i].process(s);
				if (o == 0.f) continue;
				float gl, gr; panGains(v[p][i].pan, gl, gr);
				L += o * gl; R += o * gr; part[p] += o;
			}
			float c = click[p].process() + thud[p].process(s);
			float gl, gr; panGains((p ? 1.f : -1.f) * 0.4f, gl, gr);
			L += c * gl; R += c * gr; part[p] += c;
		}
		L = softLimit(L); R = softLimit(R);
		polos = softLimit(part[0]); sangsih = softLimit(part[1]);
	}
};

// =====================================================================  REYONG
struct ReyongParams {
	bool bali;
	float strike, mallet, ring, damp, byot, cek, sympathy, set, width, kenongRing, kethukHz;
	bool dampOnGate;
	uint32_t seed;
	ReyongParams() : bali(false), strike(0.2f), mallet(0.5f), ring(1.5f), damp(0.5f), byot(0.f), cek(0.5f),
		sympathy(0.3f), set(0.3f), width(0.6f), kenongRing(3.f), kethukHz(220.f), dampOnGate(false), seed(777u) {}
};

struct ReyongEngine {
	static const int NV = 8;
	Voice v[2][NV];
	Voice kenong[2], kethuk, cekModes;
	Click cek;
	int last[2];
	float lastHz;
	float sr;
	Rng rng;
	ReyongEngine() : lastHz(220.f), sr(48000.f) { last[0] = last[1] = -1; }
	void setSampleRate(float s) {
		sr = s;
		for (int p = 0; p < 2; p++) for (int i = 0; i < NV; i++) v[p][i].reset(s);
		kenong[0].reset(s); kenong[1].reset(s); kethuk.reset(s); cekModes.reset(s);
	}

	int kettle(Partial* p, float f, float T0, float A, float contact, float strikePos) {
		struct M { float r, gb, gs, d, tw; };
		M m[6] = {
			{1.00f, 1.00f, 0.45f, 1.00f, 0.f},
			{1.52f, 0.16f, 0.42f, 0.35f, 0.f},
			{2.02f, 0.42f, 0.30f, 0.55f, 1.2f},
			{3.01f, 0.14f, 0.26f, 0.25f, 0.f},
			{4.23f, 0.07f, 0.20f, 0.15f, 0.f},
			{5.90f, 0.03f, 0.14f, 0.08f, 0.f},
		};
		int k = 0;
		for (int i = 0; i < 6; i++) {
			float g = m[i].gb + (m[i].gs - m[i].gb) * strikePos;
			float ff = f * m[i].r;
			float a = A * g * mallet(ff, contact);
			if (m[i].tw > 0.f) {
				Partial x = {ff, T0 * m[i].d, a * 0.5f, false, false};
				Partial y = {ff + m[i].tw, T0 * m[i].d, a * 0.5f, false, false};
				p[k++] = x; p[k++] = y;
			}
			else { Partial x = {ff, T0 * m[i].d, a, false, false}; p[k++] = x; }
		}
		return k;
	}

	float contactFor(const ReyongParams& q) const {
		float base = q.bali ? 0.0006f : 0.0009f;   // padded bonang sticks still let the fundamental through
		return base * (1.6f - 1.2f * q.mallet);
	}
	static float chokeTime(float d) { return GangsaEngine::chokeTime(d); }

	void strike(int part, float hz, float vel, const ReyongParams& q, bool forceByot) {
		hz = std::max(hz, 20.f);
		float f = hz * std::pow(2.f, setError(q.seed, hz, 7 + part) * q.set * 12.f / 1200.f);
		float T0 = q.ring * (q.bali ? 1.f : 1.6f);
		Partial p[Voice::MAXR];
		// a soft stick dulls the kettle but should not make it vanish: give back half the
		// fundamental's mallet loss (in dB) so high kettles stay audible
		float ct0 = contactFor(q);
		float A = VOLTS * vel * 0.6f / std::sqrt(std::max(mallet(f, ct0), 0.05f));
		int n = kettle(p, f, T0, A, ct0, q.strike);
		int i = allocVoice(v[part], NV, hz);
		float ct = chokeTime(q.damp);
		if (!q.dampOnGate && last[part] >= 0 && last[part] != i && ct > 0.f) v[part][last[part]].choke(ct);
		// sympathy: the shared frame passes a little energy to everything ringing
		if (q.sympathy > 0.f) {
			for (int pp = 0; pp < 2; pp++) for (int j = 0; j < NV; j++) {
				if ((pp == part && j == i) || !v[pp][j].active()) continue;
				v[pp][j].nudge(VOLTS * vel * q.sympathy * 0.04f);
			}
		}
		v[part][i].pan = (part ? 1.f : -1.f) * q.width * 0.7f;
		v[part][i].strike(p, n, hz, 0.f, 0.002f * vel * q.mallet);
		if (forceByot || (q.byot > 0.f && rng.uni() < q.byot)) v[part][i].chokeLater(0.03f, (int)(0.025f * sr));
		last[part] = i;
		if (part == 0) lastHz = hz;
	}

	void release(int part, const ReyongParams& q) {
		float ct = chokeTime(q.damp);
		if (q.dampOnGate && last[part] >= 0 && ct > 0.f) v[part][last[part]].choke(ct);
	}

	void cekHit(float vel, const ReyongParams& q) {
		cek.trigger(VOLTS * vel * q.cek * 0.6f, 0.003f, 1800.f, 5000.f, sr);
		Partial p[2] = {{lastHz * 2.3f, 0.015f, VOLTS * vel * q.cek * 0.3f, false, false},
		                {lastHz * 3.9f, 0.012f, VOLTS * vel * q.cek * 0.2f, false, false}};
		cekModes.strike(p, 2, lastHz, 0.f, 0.f);
	}

	void kenongHit(float hz, float vel, const ReyongParams& q) {
		Partial p[Voice::MAXR];
		int n = kettle(p, hz, q.kenongRing, VOLTS * vel * 0.9f, 0.0016f, 0.05f);
		int i = allocVoice(kenong, 2, hz);
		kenong[i].strike(p, n, hz, 0.03f, 0.004f * vel);
	}

	void kethukHit(float vel, const ReyongParams& q) {
		float f = q.kethukHz;
		Partial p[3] = {{f, 0.09f, VOLTS * vel * 0.6f, false, false},
		                {f * 1.5f, 0.05f, VOLTS * vel * 0.18f, false, false},
		                {f * 2.2f, 0.03f, VOLTS * vel * 0.09f, false, false}};
		kethuk.strike(p, 3, f, 0.f, 0.f);
	}

	void process(float& L, float& R, float& colo) {
		L = R = 0.f;
		float s;
		for (int p = 0; p < 2; p++) for (int i = 0; i < NV; i++) {
			float o = v[p][i].process(s);
			if (o == 0.f) continue;
			float gl, gr; panGains(v[p][i].pan, gl, gr);
			L += o * gl; R += o * gr;
		}
		float c = cek.process() + cekModes.process(s);
		L += c; R += c;
		float k = kenong[0].process(s) + kenong[1].process(s);
		float t = kethuk.process(s);
		float gl, gr; panGains(0.2f, gl, gr);
		L += k + t * gl; R += k + t * gr;
		colo = softLimit(k + t);
		L = softLimit(L); R = softLimit(R);
	}
};

} // namespace bronze
