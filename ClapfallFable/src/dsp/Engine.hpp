// Clapfall engine: particle clock, 32-voice particle player, band-pass /
// punch filter and a stereo FDN reverb. Port of burst() in
// tools/proto/clapfall_proto.py, made per-sample. No Rack includes.
#pragma once
#include "Particles.hpp"
#include <array>
#include <complex>

namespace clapfall {

static const int MAX_VOICES = 32;
static const int VOICE_SLOTS = 40;          // extra slots so stolen / choked voices can fade out
static const float OUT_GAIN = 6.5f;         // volts per unit of the prototype's output
enum EnvMode { ENV_AMP = 0, ENV_BOTH = 1, ENV_CHANCE = 2 };
enum FiltMode { FILT_BAND = 0, FILT_PITCH = 1, FILT_PUNCH = 2 };

inline float densityHz(float k) { return 1.5f * std::pow(400.f, k); }        // 1.5 .. 600 particles/s
inline float decaySec(float k) { return 0.012f * std::pow(500.f, k); }       // 12 ms .. 6 s
inline float toneHz(float k) { return 150.f * std::pow(10000.f / 150.f, k); } // 150 Hz .. 10 kHz
inline float spreadOct(float k) { return 1.25f * k * k + 0.25f * k; }
inline float sizeRt60(float k) { return 0.25f * std::pow(12.8f, k); }        // 0.25 .. 3.2 s

struct Controls {
	const Surface* surf = nullptr;   // null or empty -> no particles
	float density = 0.6f, decay = 0.35f, loose = 0.5f, tone = 0.5f, spread = 0.3f;
	float reverb = 0.f, size = 0.3f;
	int env = ENV_BOTH, filter = FILT_PUNCH;
	bool vOctOn = false; float vOct = 0.f;
	bool free = false;
	bool sameEveryHit = false;
};

// ---------------------------------------------------------------- voice
struct Voice {
	const float* d = nullptr;
	int len = 0;
	double pos = 0, step = 1;
	float gl = 0, gr = 0;
	bool active = false, fading = false;
	float fadeGain = 1.f, fadeStep = 0.f;

	float progress() const { return len > 0 ? (float)(pos / len) : 1.f; }
	void startFade(float samples) {
		if (!active) return;
		if (fading && fadeStep >= fadeGain / std::max(samples, 1.f)) return;   // already fading faster
		fading = true;
		fadeStep = fadeGain / std::max(samples, 1.f);
	}
	inline float at(int i) const { return (i >= 0 && i < len) ? d[i] : 0.f; }
	inline void process(float& L, float& R) {
		if (!active) return;
		int i = (int)pos;
		float f = (float)(pos - i);
		float xm = at(i - 1), x0 = at(i), x1 = at(i + 1), x2 = at(i + 2);
		// 4-point Hermite
		float c1 = 0.5f * (x1 - xm);
		float c2 = xm - 2.5f * x0 + 2.f * x1 - 0.5f * x2;
		float c3 = 0.5f * (x2 - xm) + 1.5f * (x0 - x1);
		float s = ((c3 * f + c2) * f + c1) * f + x0;
		if (fading) {
			s *= fadeGain;
			fadeGain -= fadeStep;
			if (fadeGain <= 0.f) { active = false; return; }
		}
		L += s * gl;
		R += s * gr;
		pos += step;
		if (pos >= len) active = false;
	}
};

// ---------------------------------------------------------------- filter (TPT state-variable band-pass)
struct Svf {
	float ic1 = 0, ic2 = 0;
	inline float bp(float x, float a1, float a2, float a3) {
		float v3 = x - ic2;
		float v1 = a1 * ic1 + a2 * v3;
		float v2 = ic2 + a2 * ic1 + a3 * v3;
		ic1 = 2.f * v1 - ic1;
		ic2 = 2.f * v2 - ic2;
		if (std::fabs(ic1) < 1e-20f) ic1 = 0.f;
		if (std::fabs(ic2) < 1e-20f) ic2 = 0.f;
		return v1;
	}
	void reset() { ic1 = ic2 = 0; }
};

// ---------------------------------------------------------------- reverb (8-line FDN + early reflections)
struct Reverb {
	static constexpr int N = 8;
	static constexpr float WET = 1.05f;
	float sr = 48000.f;
	std::vector<float> line[N];
	int w[N] = {};
	std::vector<float> pre[2];
	int preW = 0;
	float lp[N] = {}, hpS[N] = {};
	float sizeSm = -1.f;
	float dl[N] = {}, g[N] = {}, hs[N] = {};
	float aLp = 0.f, aHp = 0.f, preSamp = 0.f;
	double lfoPh = 0.0;
	int ctr = 0;
	float erT[2][6] = {}, erG[2][6] = {}, erU[2][6] = {};
	float erSamp[2][6] = {};

	void setSampleRate(float s) {
		sr = s;
		int maxLen = (int)std::ceil(0.0467f * 1.2f * sr) + 32;
		for (int i = 0; i < N; i++) { line[i].assign(maxLen, 0.f); w[i] = 0; }
		int preLen = (int)std::ceil(0.2f * sr) + 8;
		for (int c = 0; c < 2; c++) pre[c].assign(preLen, 0.f);
		preW = 0;
		Rng r(4242);
		for (int c = 0; c < 2; c++)
			for (int k = 0; k < 6; k++) {
				erU[c][k] = (float)r.uni();
				erG[c][k] = (r.uni() < 0.5 ? -1.f : 1.f) * std::pow(0.75f, (float)k);
			}
		sizeSm = -1.f;
		reset();
	}
	void reset() {
		for (int i = 0; i < N; i++) { std::fill(line[i].begin(), line[i].end(), 0.f); lp[i] = hpS[i] = 0.f; }
		for (int c = 0; c < 2; c++) std::fill(pre[c].begin(), pre[c].end(), 0.f);
	}
	void coeffs(float s) {
		static const float BASE[N] = {11.3f, 13.7f, 17.9f, 21.1f, 26.3f, 31.7f, 38.9f, 46.7f};
		float rt = sizeRt60(s);
		float scale = 0.45f + 0.75f * s;
		for (int i = 0; i < N; i++) {
			dl[i] = BASE[i] * scale * sr / 1000.f;
			g[i] = std::pow(10.f, -3.f * (dl[i] / sr) / rt);
			// high shelf in the loop: highs die in about 40 % of the time
			float gh = std::pow(10.f, -3.f * (dl[i] / sr) / (0.4f * rt));
			hs[i] = gh / g[i];
		}
		float fc = 9000.f - 5000.f * s;
		aLp = std::exp(-2.f * (float)M_PI * fc / sr);
		aHp = std::exp(-2.f * (float)M_PI * 180.f / sr);
		// the loop filters also cost a little at 1 kHz on every pass; give that back so
		// the quoted reverb time holds in the middle of the spectrum
		{
			typedef std::complex<double> C;
			double wv = 2 * M_PI * 1000.0 / sr;
			C z1 = std::polar(1.0, -wv);
			C hl = (1.0 - aLp) / (1.0 - (double)aLp * z1);
			C hh = 1.0 - (1.0 - aHp) / (1.0 - (double)aHp * z1);
			for (int i = 0; i < N; i++) {
				double h = hs[i];
				double mag = std::abs((C(h) + (1.0 - h) * hl) * hh);
				g[i] = (float)std::min(0.9995, g[i] / std::max(mag, 0.5));
			}
		}
		float preMs = 4.f + 20.f * s;
		preSamp = preMs * sr / 1000.f;
		for (int c = 0; c < 2; c++)
			for (int k = 0; k < 6; k++) {
				float tk = preMs * 0.4f / 1000.f + (0.003f + erU[c][k] * (0.009f + 0.05f * s)) * (k + 1) / 3.f;
				erSamp[c][k] = tk * sr;
			}
	}
	static inline float readFrac(const std::vector<float>& b, int wpos, float delay) {
		int L = (int)b.size();
		float rp = (float)wpos - delay;
		while (rp < 0) rp += L;
		int i0 = (int)rp;
		float f = rp - i0;
		int i1 = i0 + 1; if (i1 >= L) i1 -= L;
		return b[i0] + f * (b[i1] - b[i0]);
	}
	inline void process(float inL, float inR, float size, float& oL, float& oR) {
		if (sizeSm < 0.f) { sizeSm = size; coeffs(size); }
		sizeSm += (size - sizeSm) * (1.f - std::exp(-1.f / (0.3f * sr)));
		if (++ctr >= 32) { ctr = 0; coeffs(sizeSm); }
		// pre-delay and early reflections
		pre[0][preW] = inL; pre[1][preW] = inR;
		float pdL = readFrac(pre[0], preW, preSamp), pdR = readFrac(pre[1], preW, preSamp);
		float erL = 0.f, erR = 0.f;
		for (int k = 0; k < 6; k++) {
			erL += erG[0][k] * readFrac(pre[0], preW, erSamp[0][k]);
			erR += erG[1][k] * readFrac(pre[1], preW, erSamp[1][k]);
		}
		if (++preW >= (int)pre[0].size()) preW = 0;
		// late field
		lfoPh += 0.37 / sr; if (lfoPh >= 1.0) lfoPh -= 1.0;
		float m0 = 3.f * (float)std::sin(2 * M_PI * lfoPh) * sr / 48000.f;
		float m1 = 2.5f * (float)std::sin(2 * M_PI * (lfoPh * 2.3 + 0.25)) * sr / 48000.f;
		float o[N];
		for (int i = 0; i < N; i++) {
			float d = dl[i] + (i == 1 ? m0 : 0.f) + (i == 6 ? m1 : 0.f);
			float x = readFrac(line[i], w[i], d);
			lp[i] = (1.f - aLp) * x + aLp * lp[i];
			float damped = lp[i] + hs[i] * (x - lp[i]);
			hpS[i] = (1.f - aHp) * damped + aHp * hpS[i];
			o[i] = (damped - hpS[i]) * g[i];
			if (std::fabs(lp[i]) < 1e-20f) lp[i] = 0.f;
			if (std::fabs(hpS[i]) < 1e-20f) hpS[i] = 0.f;
		}
		// Hadamard mix
		float y[N];
		for (int i = 0; i < N; i++) y[i] = o[i];
		for (int h = 1; h < N; h <<= 1)
			for (int i = 0; i < N; i += 2 * h)
				for (int j = i; j < i + h; j++) { float a = y[j], b = y[j + h]; y[j] = a + b; y[j + h] = a - b; }
		const float norm = 0.35355339f;
		for (int i = 0; i < N; i++) {
			float in = (i & 1) ? pdR : pdL;
			line[i][w[i]] = y[i] * norm + in;
			if (++w[i] >= (int)line[i].size()) w[i] = 0;
		}
		oL = WET * (o[0] - o[2] + o[4] - o[6] + o[1] + o[5]) + 0.3f * WET * erL;
		oR = WET * (o[1] - o[3] + o[5] - o[7] + o[2] + o[6]) + 0.3f * WET * erR;
	}
};

// ---------------------------------------------------------------- engine
struct Engine {
	float sr = 48000.f, dt = 1.f / 48000.f;
	Voice voices[VOICE_SLOTS];
	Rng rng{777};
	int lastVar = -1;
	const Surface* lastSurf = nullptr;
	// clock / burst
	bool bursting = false, wasFree = false;
	double age = 0.0, tickIn = 0.0;
	float accLen = 1.f, accAmp = 1.f;
	float boost = 0.f;
	// filter / output
	Svf svf[2];
	float fcSm = 1000.f, a1 = 0, a2 = 0, a3 = 0, kQ = 1.f / 1.8f;
	int fctr = 0;
	float sendSm = 0.f;
	Reverb rev;
	// for lights / tests
	int particleCount = 0;
	float lastParticleAmp = 0.f;

	void setSampleRate(float s) {
		sr = s; dt = 1.f / s;
		rev.setSampleRate(s);
		reset();
	}
	void reset() {
		for (auto& v : voices) v.active = false;
		for (auto& f : svf) f.reset();
		rev.reset();
		bursting = false; boost = 0.f; tickIn = 0.0; lastVar = -1;
	}

	static float le(float loose) { return std::min(1.f, 4.f * loose); }

	void allocate(const float* d, int len, double step, float gl, float gr) {
		int live = 0, oldest = -1; float op = -1.f;
		for (int i = 0; i < VOICE_SLOTS; i++)
			if (voices[i].active && !voices[i].fading) {
				live++;
				float p = voices[i].progress();
				if (p > op) { op = p; oldest = i; }
			}
		if (live >= MAX_VOICES && oldest >= 0) voices[oldest].startFade(0.002f * sr);
		int slot = -1; float minG = 2.f;
		for (int i = 0; i < VOICE_SLOTS; i++)
			if (!voices[i].active) { slot = i; break; }
		if (slot < 0)
			for (int i = 0; i < VOICE_SLOTS; i++)
				if (voices[i].fading && voices[i].fadeGain < minG) { minG = voices[i].fadeGain; slot = i; }
		if (slot < 0) slot = oldest >= 0 ? oldest : 0;
		Voice& v = voices[slot];
		v.d = d; v.len = len; v.pos = 0.0; v.step = step; v.gl = gl; v.gr = gr;
		v.active = true; v.fading = false; v.fadeGain = 1.f; v.fadeStep = 0.f;
	}

	void fire(const Controls& c, float amp) {
		const Surface* s = c.surf;
		if (!s || s->vars.empty()) return;
		if (s != lastSurf) { lastSurf = s; lastVar = -1; }
		int n = (int)s->vars.size();
		int v;
		if (lastVar < 0 || n == 1) v = rng.below(n);
		else { v = rng.below(n - 1); if (v >= lastVar) v++; }
		lastVar = v;
		float centre = 1.f;
		if (c.vOctOn) centre = std::pow(2.f, c.vOct);
		else if (c.filter >= FILT_PITCH) centre = std::pow(2.f, (c.tone - 0.5f) * 4.f);
		float sp = spreadOct(c.spread);
		float ratio = centre * std::pow(2.f, (float)rng.uni(-1, 1) * sp);
		ratio = std::min(8.f, std::max(1.f / 16.f, ratio));
		float panW = std::min(1.f, 1.6f * c.spread);
		float pan = (float)rng.uni(-1, 1) * panW;
		float th = (pan + 1.f) * (float)M_PI / 4.f;
		float L = le(c.loose);
		float comp = s->choke ? 1.f : 1.f / std::sqrt(1.f + densityHz(c.density) / 80.f);
		float gg = amp * comp * std::pow(10.f, L * (float)rng.uni(-2.5, 2.5) / 20.f);
		if (s->choke)
			for (auto& vo : voices) vo.startFade(0.0015f * sr);
		const std::vector<float>& tab = s->vars[v];
		double step = ratio * s->rates[v] / sr;
		allocate(tab.data(), (int)tab.size(), step, gg * std::cos(th), gg * std::sin(th));
		particleCount++;
		lastParticleAmp = std::max(lastParticleAmp, gg);
	}

	// amp / probability for an envelope value e (0..1)
	void envShape(int mode, float e, float L, float& amp, float& prob) {
		if (mode == ENV_AMP) { amp = e; prob = 1.f; }
		else if (mode == ENV_BOTH) { amp = std::sqrt(e); prob = std::sqrt(e); }
		else { amp = 1.f - L * (float)rng.uni(0, 0.65); prob = e; }
	}

	double nextInterval(const Controls& c, float rateMul) {
		double T = 1.0 / (densityHz(c.density) * rateMul);
		return T * ((1.0 - c.loose) + c.loose * rng.expo());
	}

	// One particle, full level, independent of the burst (ONE jack)
	void single(const Controls& c) { fire(c, 1.f); }

	void trigger(const Controls& c, bool accent) {
		if (c.free) {
			boost = std::min(7.f, boost + 3.f);
			return;
		}
		if (c.sameEveryHit || c.loose <= 0.f) { rng.reseed(12345); lastVar = -1; }
		age = 0.0;
		bursting = true;
		accLen = accent ? 1.6f : 1.f;
		accAmp = accent ? 1.5f : 1.f;
		float amp, prob;
		envShape(c.env, 1.f, le(c.loose), amp, prob);
		fire(c, amp * accAmp);   // the first particle always fires, on the trigger sample
		tickIn = nextInterval(c, 1.f);
	}

	void tick(const Controls& c) {
		float L = le(c.loose), e, amp, prob, mul = 1.f;
		if (c.free) {
			e = c.decay;
			if (e <= 0.f) return;
			envShape(c.env, e, L, amp, prob);
		}
		else {
			if (!bursting) return;
			float tau = decaySec(c.decay) / 3.f * accLen;
			e = (float)std::exp(-age / tau);
			if (e < 1e-3f) { bursting = false; return; }
			envShape(c.env, e, L, amp, prob);
			amp *= accAmp;
		}
		(void)mul;
		float u = 0.5f + ((float)rng.uni() - 0.5f) * L;
		if (u < prob) fire(c, amp);
	}

	void process(const Controls& c, bool trig, bool one, bool accentHigh, float& outL, float& outR) {
		if (c.free != wasFree) {
			wasFree = c.free;
			bursting = false;
			tickIn = c.free ? 0.0 : tickIn;
		}
		if (one) single(c);
		if (trig) trigger(c, accentHigh);
		// clock
		if (c.free || bursting) {
			tickIn -= dt;
			int guard = 0;
			while (tickIn <= 0.0 && guard++ < 16) {
				if (!c.free && !bursting) break;
				tick(c);
				tickIn += nextInterval(c, c.free ? (1.f + boost) : 1.f);
			}
			if (tickIn <= 0.0) tickIn = 0.0;
		}
		age += dt;
		boost *= std::exp(-dt / 0.12f);
		// voices
		float L = 0.f, R = 0.f;
		for (auto& v : voices) v.process(L, R);
		// filter
		float target = std::min(toneHz(c.tone), 0.45f * sr);
		fcSm += (target - fcSm) * (1.f - std::exp(-dt / 0.005f));
		if (fctr-- <= 0) {
			fctr = 7;
			float gq = std::tan((float)M_PI * fcSm / sr);
			a1 = 1.f / (1.f + gq * (gq + kQ));
			a2 = gq * a1;
			a3 = gq * a2;
		}
		float bl = 2.f * kQ * svf[0].bp(L, a1, a2, a3);
		float br = 2.f * kQ * svf[1].bp(R, a1, a2, a3);
		float yl, yr;
		if (c.filter == FILT_PUNCH) { yl = L + 1.5f * bl; yr = R + 1.5f * br; }
		else { yl = bl; yr = br; }
		yl *= 0.5f; yr *= 0.5f;
		// reverb send
		sendSm += (c.reverb - sendSm) * (1.f - std::exp(-dt / 0.005f));
		float wl, wr;
		rev.process(sendSm * yl, sendSm * yr, c.size, wl, wr);
		yl += wl; yr += wr;
		outL = 10.f * std::tanh(yl * OUT_GAIN / 10.f);
		outR = 10.f * std::tanh(yr * OUT_GAIN / 10.f);
	}
};

}  // namespace clapfall
