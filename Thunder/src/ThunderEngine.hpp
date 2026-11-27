// Thunder engine: one thunder drum voice, no Rack dependency (so test/render.cpp can use it).
//
// Signal path (after the Perkons idea, all models our own):
//   per hit:  algorithm A --+
//                           +-- MORPH (equal power) -> + CLICK + NOISE -> multimode filter (HP/BP/LP)
//             algorithm B --+
//   voice:    sum of up to 4 overlapping hits -> DC block -> overdrive (DRIVE = dry/wet) -> TONE
//             -> + bucket-brigade style delay (SEND, TIME, FDBK) -> LEVEL
//
// DECAY fully up turns any algorithm into a drone. WEAR varies every hit a little; MUTATE lets
// P1, P2, MORPH and MODE wander from hit to hit (LOCK freezes where they got to).
#pragma once
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>
#include <algorithm>

namespace thunder {

static const int NUM_ALGOS = 15;
static const int MAX_HITS = 4; // sounding at once
static const int KS_SIZE = 8192;
static const float PI = 3.14159265f;
static const float TWO_PI = 6.28318531f;

struct AlgoInfo {
	const char* name;
	const char* p1;
	const char* p2;
	const char* mode[3];
};

inline const AlgoInfo& algoInfo(int i) {
	static const AlgoInfo T[NUM_ALGOS] = {
		{"FOLD BD", "fold", "symmetry", {"sine", "triangle", "fold env"}},
		{"WAVETABLE BD", "position", "surf", {"classic", "vocal", "digital"}},
		{"RESONANT BD", "knock", "fm", {"pure", "punch", "overdriven"}},
		{"COMPLEX", "ratio", "index", {"fm", "ring", "feedback"}},
		{"RES SNARE", "noise tone", "snap", {"tight", "open", "brush"}},
		{"SLAP", "tone", "spread", {"retrig 1", "retrig 2", "retrig 3"}},
		{"TOM", "overtone", "skin", {"tom", "conga", "talking"}},
		{"NOISE HAT", "tone", "metal", {"white", "crushed", "dust"}},
		{"PULSE HAT", "spread", "tone", {"808", "bell", "random"}},
		{"KARPLUS", "bright", "damp", {"string", "drum", "metal"}},
		{"SAMPLE", "start", "attack", {"slot 1", "slot 2", "slot 3"}},
		{"METAL", "inharm", "strike", {"cowbell", "bell", "plate"}},
		{"PARTICLES", "beads", "size", {"maracas", "tambourine", "scrape"}},
		{"CIRCUIT 909", "shape", "attack", {"kick", "snare", "rim"}},
		{"CHAOS", "chaos", "speed", {"logistic", "feedback", "lorenz"}},
	};
	return T[std::max(0, std::min(NUM_ALGOS - 1, i))];
}

struct Rng {
	uint32_t s;
	Rng() : s(0x9E3779B9u) {}
	uint32_t next() {
		s ^= s << 13;
		s ^= s >> 17;
		s ^= s << 5;
		return s;
	}
	float uni() { return (next() >> 8) * (1.f / 16777216.f); }
	float bi() { return uni() * 2.f - 1.f; }
};

struct Sample {
	std::vector<float> data;
	float sr;
	Sample() : sr(48000.f) {}
};

struct Controls {
	int algoA, algoB, mode, vcf;
	float morph, tuneHz, bend, sweep, click, decay, p1, p2, noise;
	float cutoff, res, fenv, drive, tone, send, time, fdbk, accentAmt, level, wear, mutate;
	bool lock, sendPatched;
	Controls()
		: algoA(0), algoB(2), mode(0), vcf(2), morph(0.f), tuneHz(50.f), bend(0.45f), sweep(0.3f), click(0.3f),
		  decay(0.56f), p1(0.35f), p2(0.5f), noise(0.f), cutoff(1.f), res(0.1f), fenv(0.f), drive(0.3f), tone(0.7f),
		  send(0.f), time(0.45f), fdbk(0.3f), accentAmt(0.5f), level(0.8f), wear(0.f), mutate(0.f), lock(false),
		  sendPatched(false) {}
};

// DECAY knob (0..1) -> seconds; the top 3 % is the drone zone
inline float decaySeconds(float d) { return 0.012f * std::pow(400.f, std::min(d, 1.f)); }
inline bool isDrone(float d) { return d >= 0.97f; }
inline float sweepSeconds(float s) { return 0.004f * std::pow(100.f, s); }
inline float cutoffHz(float c) { return 20.f * std::pow(1000.f, c); }
inline float delaySeconds(float t) { return 0.02f * std::pow(50.f, t); }

inline float wrap1(float p) { return p - std::floor(p); }
inline float sinp(float ph) { return std::sin(TWO_PI * ph); }
inline float softclip(float x) { return x / (1.f + std::fabs(x)); }

// Trapezoidal state-variable filter (lossless when k = 0)
struct SVF {
	float ic1, ic2, lp, bp, hp;
	SVF() { reset(); }
	void reset() { ic1 = ic2 = lp = bp = hp = 0.f; }
	void process(float x, float g, float k) {
		float a1 = 1.f / (1.f + g * (g + k));
		float v3 = x - ic2;
		float v1 = a1 * ic1 + g * a1 * v3;
		float v2 = ic2 + g * v1;
		ic1 = 2.f * v1 - ic1;
		ic2 = 2.f * v2 - ic2;
		lp = v2;
		bp = v1;
		hp = x - k * v1 - v2;
	}
};
inline float svfG(float hz, float sr) { return std::tan(PI * std::min(std::max(hz, 5.f), 0.45f * sr) / sr); }


// ---- small DSP helpers ----
// polyBLEP residual for a discontinuity at phase 0 (phase t in 0..1, increment dt)
inline float polyblep(float t, float dt) {
	if (dt <= 0.f) return 0.f;
	if (t < dt) {
		t /= dt;
		return t + t - t * t - 1.f;
	}
	if (t > 1.f - dt) {
		t = (t - 1.f) / dt;
		return t * t + t + t + 1.f;
	}
	return 0.f;
}
// band-limited square (+1 for the first half) and rising saw (0 at phase 0, jump at phase 0.5)
inline float blepSquare(float p, float dp) { return (p < 0.5f ? 1.f : -1.f) + polyblep(p, dp) - polyblep(wrap1(p + 0.5f), dp); }
inline float blepSaw(float p, float dp) {
	float q = wrap1(p + 0.5f);
	return 2.f * q - 1.f - polyblep(q, dp);
}
// a one-pole coefficient designed at 48 kHz, made to behave the same at any sample rate
inline float rateCoef(float a48, float sr) { return 1.f - std::pow(1.f - std::min(a48, 0.9999f), 48000.f / sr); }
inline float logcosh(float x) {
	float a = std::fabs(x);
	return a + std::log1p(std::exp(-2.f * a)) - 0.69314718f;
}

struct AlgoState {
	float ph[8];
	float z[8];
	float ratio[8];
	float prev, sPos, ksLp, apZ, holdV, holdPh;
	SVF f1, f2, f3;
	int ksW;
	float ks[KS_SIZE];
	void reset(bool clearKs) {
		std::memset(ph, 0, sizeof(ph));
		std::memset(z, 0, sizeof(z));
		std::memset(ratio, 0, sizeof(ratio));
		prev = sPos = ksLp = apZ = holdV = holdPh = 0.f;
		f1.reset();
		f2.reset();
		f3.reset();
		ksW = 0;
		if (clearKs) std::memset(ks, 0, sizeof(ks));
	}
};

// Everything an algorithm needs for one sample
struct Ctx {
	float f, e, t, p1, p2, sr, dt, decayT;
	float nsc; // white-noise scale so noisy sounds keep their level at any sample rate
	int mode;
	bool drone, first;
	float accent;
	Rng* rng;
	const Sample* samples;
};

// ---- the 12 algorithms. Each returns roughly +-1, already shaped by its envelope. ----

inline float algoFold(AlgoState& s, const Ctx& c) {
	s.ph[0] = wrap1(s.ph[0] + c.f * c.dt);
	float osc;
	if (c.mode == 1) {
		float p = s.ph[0];
		osc = p < 0.25f ? 4.f * p : p < 0.75f ? 2.f - 4.f * p : 4.f * p - 4.f;
	}
	else
		osc = sinp(s.ph[0]);
	float amt = 1.f + c.p1 * 7.f * (c.mode == 2 ? c.e * c.e : 1.f) + c.accent * 1.5f;
	float y = std::sin((osc * amt + (c.p2 - 0.5f) * 2.f) * PI * 0.5f);
	return y * c.e;
}

inline float algoWavetable(AlgoState& s, const Ctx& c) {
	float dp = c.f * c.dt;
	s.ph[0] = wrap1(s.ph[0] + dp);
	float p = s.ph[0];
	float pos = std::min(1.f, std::max(0.f, c.p1 + c.p2 * c.e * (1.f - c.p1)));
	float y;
	if (c.mode == 0) {
		float sn = sinp(p);
		float tr = p < 0.25f ? 4.f * p : p < 0.75f ? 2.f - 4.f * p : 4.f * p - 4.f;
		float x = pos * 3.f;
		if (x < 1.f) y = sn + (tr - sn) * x;
		else if (x < 2.f) y = tr + (blepSaw(p, dp) - tr) * (x - 1.f);
		else {
			float sw = blepSaw(p, dp);
			y = sw + (blepSquare(p, dp) - sw) * (x - 2.f);
		}
		y *= 0.9f;
	}
	else if (c.mode == 1) {
		// vocal: the formant also opens with the envelope, so it talks even with SURF at zero
		float cen = 1.f + std::min(1.f, pos + 0.25f * c.e) * 9.f, sum = 0.f, wsum = 0.f;
		for (int k = 1; k <= 10; k++) {
			if (c.f * k > 0.45f * c.sr) break;
			float d = k - cen;
			float w = d * d < 16.f ? 1.f / (1.f + 0.6f * d * d) : 0.f;
			sum += sinp(wrap1(p * k)) * w;
			wsum += w;
		}
		y = 1.3f * sum / std::max(wsum, 0.3f);
	}
	else {
		float d = 0.5f - 0.45f * pos;
		float q = p < d ? p * 0.5f / d : 0.5f + (p - d) * 0.5f / (1.f - d);
		y = sinp(q);
		float levels = std::pow(2.f, 7.f - 5.f * pos);
		y = std::round(y * levels) / levels;
	}
	return y * c.e;
}

inline float algoResonant(AlgoState& s, const Ctx& c) {
	if (c.first) {
		s.f1.reset();
		s.f1.ic2 = 1.f; // ring from rest as a sine (bandpass output starts at 0)
	}
	float fEff = c.f * (1.f + c.p2 * 0.8f * s.prev);
	float g = svfG(fEff, c.sr);
	float k = c.drone ? 0.f : std::min(1.5f, 1.f / (PI * std::max(c.f, 10.f) * c.decayT));
	// knock: a short pulse into the resonator and straight out
	float knock = c.p1 * std::exp(-c.t / 0.0012f);
	s.f1.process(knock * 0.6f, g, k);
	float y = -s.f1.bp;
	s.prev = y;
	if (c.mode == 1) y = std::tanh(y * (1.f + 3.f * c.e)) * 0.8f; // punch: drive follows the envelope
	else if (c.mode == 2) y = std::tanh(y * (2.f + 4.f * c.e)) * 0.7f;
	return y + knock * 0.5f;
}

inline float algoComplex(AlgoState& s, const Ctx& c) {
	static const float R[8] = {0.5f, 1.f, 1.4142f, 2.f, 2.7183f, 3.5f, 5.19f, 7.11f};
	float r = R[std::min(7, (int) (c.p1 * 7.999f))];
	s.ph[0] = wrap1(s.ph[0] + c.f * c.dt);
	s.ph[1] = wrap1(s.ph[1] + c.f * r * c.dt);
	float y;
	if (c.mode == 0) {
		float I = c.p2 * 6.f * (0.25f + 0.75f * c.e) * (1.f + 0.5f * c.accent);
		y = std::sin(TWO_PI * s.ph[0] + I * sinp(s.ph[1]));
	}
	else if (c.mode == 1) {
		float car = sinp(s.ph[0]);
		y = car * (1.f - c.p2) + car * sinp(s.ph[1]) * c.p2;
	}
	else {
		float m = std::sin(TWO_PI * s.ph[1] + c.p2 * 1.8f * s.prev);
		s.prev = m;
		y = std::sin(TWO_PI * s.ph[0] + 2.5f * c.e * m);
	}
	return y * c.e;
}

inline float algoSnare(AlgoState& s, const Ctx& c) {
	static const float NM[3] = {0.55f, 1.f, 1.8f};
	if (c.first) {
		s.z[4] = 1.f;
		s.z[5] = std::exp(-c.dt / (c.decayT * NM[c.mode]));
		s.z[6] = c.decayT;
	}
	if (s.z[6] != c.decayT) { // DECAY moved: recompute the noise envelope coefficient
		s.z[5] = std::exp(-c.dt / (c.decayT * NM[c.mode]));
		s.z[6] = c.decayT;
	}
	s.ph[0] = wrap1(s.ph[0] + c.f * c.dt);
	s.ph[1] = wrap1(s.ph[1] + c.f * 1.62f * c.dt);
	float body = sinp(s.ph[0]) + 0.5f * sinp(s.ph[1]);
	if (!c.drone) s.z[4] *= s.z[5];
	float en = s.z[4];
	if (c.mode == 2) en *= std::min(1.f, c.t / 0.004f);
	float n = c.rng->bi() * c.nsc;
	float cen = 800.f * std::pow(2.f, c.p1 * 3.3f);
	s.f1.process(n, svfG(cen, c.sr), c.mode == 2 ? 1.6f : 1.1f);
	s.f2.process(n, svfG(5500.f, c.sr), 1.4f); // bright wire layer in every mode
	float noise = s.f1.bp * 1.3f + s.f2.hp * (c.mode == 0 ? 0.45f : 0.3f);
	return body * (1.f - 0.75f * c.p2) * c.e * c.e * 0.8f + noise * (0.3f + 0.9f * c.p2) * en;
}

inline float algoSlap(AlgoState& s, const Ctx& c) {
	static const int NB[3] = {3, 4, 5};
	int nb = NB[c.mode];
	float gap = 0.004f + c.p2 * 0.02f;
	if (c.first) {
		float tb = 0.f;
		for (int i = 0; i < nb; i++) {
			s.ratio[i] = tb;
			float gg = c.mode == 2 ? gap * (0.4f + 1.2f * c.rng->uni()) : gap * (c.mode == 1 ? (i == 2 ? 1.6f : 1.f) : 1.f);
			tb += gg;
		}
	}
	float env = 0.f;
	for (int i = 0; i < nb; i++) {
		if (c.t >= s.ratio[i]) {
			float dt = c.t - s.ratio[i];
			bool last = i == nb - 1;
			float be = std::exp(-dt / 0.0035f) * (last ? 1.f : 0.85f);
			if (last) be = std::max(be, 0.7f * c.e);
			env = std::max(env, be);
		}
	}
	// two hand-cavity resonances in parallel
	float cen = std::min(0.18f * c.sr, (500.f + c.p1 * 2000.f) * std::sqrt(std::max(c.f, 20.f) / 200.f));
	float n = c.rng->bi() * c.nsc;
	s.f1.process(n, svfG(cen, c.sr), 0.5f);
	s.f2.process(n, svfG(cen * 2.4f, c.sr), 0.5f);
	return (s.f1.bp + 0.7f * s.f2.bp) * env * 0.9f;
}

inline float algoTom(AlgoState& s, const Ctx& c) {
	static const float OV[3] = {1.5f, 2.3f, 2.0f};
	float f = c.f;
	if (c.mode == 2) f *= 1.f + 0.45f * (1.f - c.e); // talking drum: squeeze raises the pitch
	s.ph[0] = wrap1(s.ph[0] + f * c.dt);
	s.ph[1] = wrap1(s.ph[1] + f * OV[c.mode] * c.dt);
	float y = sinp(s.ph[0]) + c.p1 * 0.6f * sinp(s.ph[1]) * c.e;
	float skinT = c.mode == 1 ? 0.008f : 0.015f;
	s.z[0] += (c.rng->bi() * c.nsc - s.z[0]) * rateCoef(0.35f, c.sr);
	float skin = s.z[0] * std::exp(-c.t / skinT) * c.p2 * 1.2f;
	return y * c.e * 0.85f + skin;
}

inline float algoNoiseHat(AlgoState& s, const Ctx& c) {
	float n;
	if (c.mode == 0) n = c.rng->bi() * c.nsc;
	else if (c.mode == 1) {
		s.holdPh += c.f * 16.f * c.dt;
		if (s.holdPh >= 1.f) {
			s.holdPh -= std::floor(s.holdPh);
			s.holdV = c.rng->bi();
		}
		n = s.holdV;
	}
	else {
		// dust: sparse clicks, each scaled so the density sounds the same at any sample rate
		float dens = (200.f + c.f * 20.f) * c.dt;
		n = c.rng->uni() < dens ? c.rng->bi() * 1.65f * (c.sr / 48000.f) : 0.f;
	}
	float cen = 2000.f * std::pow(2.f, c.p1 * 2.6f);
	s.f1.process(n, svfG(cen, c.sr), 1.6f - 1.4f * c.p2);
	float y = s.f1.hp * (1.f - 0.5f * c.p2) + s.f1.bp * c.p2 * 1.5f;
	return y * c.e * c.e;
}

inline float algoPulseHat(AlgoState& s, const Ctx& c) {
	static const float R808[6] = {1.f, 1.483f, 1.800f, 2.546f, 2.630f, 3.897f};
	static const float RBELL[6] = {1.f, 1.59f, 2.14f, 2.30f, 2.65f, 3.33f};
	if (c.first && c.mode == 2)
		for (int i = 0; i < 6; i++) s.ratio[i] = 1.f + 3.f * c.rng->uni();
	float sum = 0.f;
	for (int i = 0; i < 6; i++) {
		float r = c.mode == 0 ? R808[i] : c.mode == 1 ? RBELL[i] : s.ratio[i];
		r = 1.f + (r - 1.f) * (0.3f + 1.4f * c.p1);
		float dp = std::min(c.f * r * c.dt, 0.45f);
		s.ph[i] = wrap1(s.ph[i] + dp);
		sum += blepSquare(s.ph[i], dp);
	}
	float cen = 3000.f * std::pow(2.f, c.p2 * 2.f);
	s.f1.process(sum * 0.25f, svfG(cen, c.sr), 0.8f);
	s.f2.process(s.f1.bp, svfG(cen * 0.7f, c.sr), 1.4f);
	return s.f2.hp * c.e * c.e * 2.4f;
}

inline float algoKarplus(AlgoState& s, const Ctx& c) {
	float f = std::max(c.f, c.sr / (KS_SIZE - 8));
	float aDamp = rateCoef(1.f - 0.85f * c.p2, c.sr);
	// loop length minus the delay the damping filter (and the metal allpass) adds, so it plays in tune
	float D = c.sr / f - (1.f - aDamp) / aDamp - (c.mode == 2 ? 4.f : 0.f);
	D = std::max(D, 2.f);
	float in = 0.f;
	if (c.t < 1.f / f) {
		float a = rateCoef(0.08f + 0.9f * c.p1, c.sr);
		s.z[0] += (c.rng->bi() * c.nsc - s.z[0]) * a;
		in = s.z[0] * 1.3f;
	}
	float rp = s.ksW - D;
	while (rp < 0.f) rp += KS_SIZE;
	int i0 = (int) rp;
	float fr = rp - i0;
	float x = s.ks[i0 % KS_SIZE] * (1.f - fr) + s.ks[(i0 + 1) % KS_SIZE] * fr;
	s.ksLp += (x - s.ksLp) * aDamp;
	float loop = s.ksLp;
	if (c.mode == 1 && c.rng->uni() < 0.5f) loop = -loop;
	if (c.mode == 2) {
		float a = 0.6f;
		float y = -a * loop + s.apZ;
		s.apZ = loop + a * y;
		loop = y;
	}
	float g = c.drone ? 0.9995f : std::exp(-1.f / (f * c.decayT));
	float w = in + loop * g;
	s.ks[s.ksW] = w;
	s.ksW = (s.ksW + 1) % KS_SIZE;
	return w;
}

inline float algoSample(AlgoState& s, const Ctx& c) {
	const Sample& smp = c.samples[c.mode];
	int len = (int) smp.data.size();
	if (len < 4) return 0.f;
	if (c.first) s.sPos = c.p1 * (len - 1);
	float y = 0.f;
	int i0 = (int) s.sPos;
	if (i0 < len - 1) {
		// 4-point Hermite interpolation
		float fr = s.sPos - i0;
		float xm1 = smp.data[std::max(0, i0 - 1)], x0 = smp.data[i0], x1 = smp.data[i0 + 1], x2 = smp.data[std::min(len - 1, i0 + 2)];
		float c1 = 0.5f * (x1 - xm1), c2 = xm1 - 2.5f * x0 + 2.f * x1 - 0.5f * x2, c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
		y = ((c3 * fr + c2) * fr + c1) * fr + x0;
		s.sPos += (c.f / 261.63f) * (smp.sr / c.sr);
	}
	else if (c.drone)
		s.sPos = c.p1 * (len - 1);
	// edge fades so a start point mid-waveform or the drone loop never clicks
	float edge = std::min(1.f, (len - 1 - s.sPos) / (0.002f * smp.sr));
	float att = c.p2 * c.p2 * c.p2 * 0.1f;
	float a = std::min(1.f, c.t / std::max(att, c.p1 > 0.f ? 0.001f : 0.0002f));
	return y * a * std::max(0.f, edge) * c.e;
}

inline float algoMetal(AlgoState& s, const Ctx& c) {
	static const float BELL[6] = {0.5f, 1.f, 1.2f, 1.5f, 2.f, 2.74f};
	static const float PLATE[6] = {1.f, 1.58f, 2.24f, 2.92f, 3.61f, 4.47f};
	if (c.first)
		for (int i = 0; i < 6; i++) s.ratio[i] = c.rng->bi();
	float strike = c.rng->bi() * c.nsc * std::exp(-c.t / 0.002f) * c.p2 * 0.5f;
	if (c.mode == 0) {
		float r = 1.482f + c.p1 * 0.4f;
		float d0 = c.f * c.dt, d1 = std::min(c.f * r * c.dt, 0.45f);
		s.ph[0] = wrap1(s.ph[0] + d0);
		s.ph[1] = wrap1(s.ph[1] + d1);
		float sq = blepSquare(s.ph[0], d0) + blepSquare(s.ph[1], d1);
		float cen = c.f * (1.8f + c.p2 * 1.2f);
		s.f1.process(sq * 0.5f, svfG(cen, c.sr), 0.6f);
		s.f2.process(s.f1.bp, svfG(cen * 1.9f, c.sr), 1.2f);
		float env = (0.6f * std::exp(-c.t / 0.012f) + 0.4f) * c.e;
		return (s.f1.bp * 0.75f + s.f2.bp * 0.35f) * env + strike;
	}
	float le = std::log(std::max(c.e, 1e-9f));
	float sum = 0.f, norm = 0.f;
	for (int i = 0; i < 6; i++) {
		float r = c.mode == 1 ? std::pow(BELL[i], 1.f + c.p1 * 0.3f) : PLATE[i] * (1.f + c.p1 * 0.1f * s.ratio[i]);
		if (c.f * r > 0.45f * c.sr) continue;
		s.ph[i] = wrap1(s.ph[i] + c.f * r * c.dt);
		float amp = 1.f / (1.f + i * (1.f - c.p2));
		sum += sinp(s.ph[i]) * amp * std::exp(le * (0.5f + 0.4f * r));
		norm += amp;
	}
	return sum / std::max(norm, 1.f) * 1.6f + strike;
}

// 13 PARTICLES: many little collisions (shaker physics after the stochastic "PhISEM" idea, our own tuning)
inline float algoParticles(AlgoState& s, const Ctx& c) {
	float cen = std::min(0.4f * c.sr, c.f * 12.f * (1.4f - c.p2)); // bigger beads, lower ring
	if (c.mode == 2) {
		// scrape: a stick across teeth; the stroke speeds up then slows (guiro / sandpaper)
		float gesture = std::sin(PI * std::min(1.f, c.t / std::max(c.decayT * 2.f, 0.02f)));
		float rate = c.f * 0.12f * (0.3f + 1.7f * c.p1) * (0.3f + 0.7f * gesture);
		s.holdPh += rate * c.dt * (0.8f + 0.4f * c.rng->uni());
		if (s.holdPh >= 1.f) {
			s.holdPh -= 1.f;
			s.z[0] += 0.6f + 0.4f * c.rng->uni();
		}
		s.z[0] *= 1.f - rateCoef(0.004f, c.sr);
		float n = c.rng->bi() * c.nsc;
		s.f1.process(n * s.z[0], svfG(cen * 0.6f, c.sr), 0.7f);
		return s.f1.bp * std::min(1.f, gesture * 3.f + c.e) * c.e * 2.2f;
	}
	// beads: collision chance rises with the shake energy (the envelope)
	float rate = (40.f + c.p1 * c.p1 * 2400.f) * c.e;
	if (c.first) s.z[0] = 1.f; // the shake itself
	if (c.rng->uni() < rate * c.dt) s.z[0] += (0.3f + 0.7f * c.rng->uni()) * c.e;
	s.z[0] *= 1.f - rateCoef(c.mode == 1 ? 0.0015f : 0.004f, c.sr);
	float n = c.rng->bi() * c.nsc * s.z[0];
	if (c.mode == 0) {
		s.f1.process(n, svfG(cen, c.sr), 0.5f);
		return s.f1.bp * 0.7f;
	}
	// tambourine: metal jingles ring on after each collision
	s.f1.process(n, svfG(cen * 0.55f, c.sr), 0.12f);
	s.f2.process(n, svfG(cen * 1.27f, c.sr), 0.1f);
	s.f3.process(n, svfG(cen * 1.83f, c.sr), 0.08f);
	return (s.f1.bp * 0.12f + s.f2.bp * 0.1f + s.f3.bp * 0.08f) * 0.95f + n * 0.06f;
}

// 14 CIRCUIT 909: our own take on the analogue house/techno kit voices
inline float algoCircuit909(AlgoState& s, const Ctx& c) {
	if (c.mode == 0) {
		// kick: shaped sine with its own fast pitch drop (on top of BEND), attack click
		float f = c.f * (1.f + 2.2f * std::exp(-c.t / 0.010f) + 0.35f * std::exp(-c.t / 0.06f));
		s.ph[0] = wrap1(s.ph[0] + f * c.dt);
		float sh = 1.f + 3.f * c.p1;
		float y = std::tanh(sinp(s.ph[0]) * sh) / std::tanh(sh);
		s.z[0] += (c.rng->bi() * c.nsc - s.z[0]) * rateCoef(0.25f, c.sr);
		float click = (s.z[0] * 1.5f + std::exp(-c.t / 0.0004f)) * std::exp(-c.t / 0.003f) * c.p2;
		return y * c.e + click;
	}
	if (c.mode == 1) {
		// snare: two detuned body oscillators (ratio set by P1) + "snappy" noise
		if (c.first) s.z[1] = 1.f;
		float bend = 1.f + 0.6f * std::exp(-c.t / 0.008f);
		float d0 = c.f * bend * c.dt, d1 = c.f * (1.5f + 0.3f * c.p1) * bend * c.dt;
		s.ph[0] = wrap1(s.ph[0] + d0);
		s.ph[1] = wrap1(s.ph[1] + d1);
		float tri0 = 1.f - 4.f * std::fabs(s.ph[0] - 0.5f), tri1 = 1.f - 4.f * std::fabs(s.ph[1] - 0.5f);
		float body = (0.6f * std::tanh(1.6f * tri0) + 0.45f * std::tanh(1.6f * tri1)) * c.e * c.e;
		if (!c.drone) s.z[1] *= std::exp(-c.dt / (c.decayT * 0.9f));
		float n = c.rng->bi() * c.nsc;
		s.f1.process(n, svfG(1100.f, c.sr), 1.f);
		s.f2.process(s.f1.hp, svfG(7500.f, c.sr), 1.2f);
		float snappy = s.f2.lp * s.z[1] * (0.2f + 1.3f * c.p2);
		return body * (1.1f - 0.4f * c.p2) + snappy;
	}
	// rim: two ringing bandpasses pinged by a pulse, short and woody
	float pulse = c.t < 0.0003f ? 1.f : 0.f;
	pulse *= std::sqrt(48000.f / c.sr) * c.nsc; // same energy at any rate
	float e3 = c.e * c.e * c.e;
	s.f1.process(pulse * 6.f, svfG(c.f, c.sr), 0.06f);
	s.f2.process(pulse * 6.f, svfG(c.f * (2.6f + 1.2f * c.p1), c.sr), 0.05f);
	float y = (s.f1.bp * 0.08f + s.f2.bp * 0.06f) * e3;
	float clk = c.rng->bi() * c.nsc * std::exp(-c.t / 0.0008f) * c.p2 * 0.8f;
	return std::tanh(y * 3.f) * 0.8f + clk;
}

// 15 CHAOS: chaotic maths as a drum; every hit starts from a new place
inline float algoChaos(AlgoState& s, const Ctx& c) {
	if (c.mode == 0) {
		// logistic map, iterated at a rate set by TUNE and P2, smoothed between steps
		if (c.first) {
			s.z[0] = 0.1f + 0.8f * c.rng->uni();
			s.z[1] = s.z[2] = s.z[0];
		}
		float r = 3.55f + 0.449f * c.p1;
		s.holdPh += c.f * (0.5f + 6.f * c.p2) * c.dt;
		while (s.holdPh >= 1.f) {
			s.holdPh -= 1.f;
			s.z[1] = s.z[0];
			s.z[0] = r * s.z[0] * (1.f - s.z[0]);
		}
		float x = s.z[1] + (s.z[0] - s.z[1]) * s.holdPh;
		s.z[2] += (x - s.z[2]) * rateCoef(0.5f, c.sr);
		return (2.f * s.z[2] - 1.f) * c.e;
	}
	if (c.mode == 1) {
		// feedback FM: one sine modulating itself; past about 1.2 it tears into noise
		s.ph[0] = wrap1(s.ph[0] + c.f * (0.5f + 2.f * c.p2) * c.dt);
		float fb = (0.6f + 2.6f * c.p1) * (0.4f + 0.6f * c.e);
		float y = std::sin(TWO_PI * s.ph[0] + fb * 0.5f * (s.prev + s.z[0]));
		s.z[0] = s.prev;
		s.prev = y;
		return y * c.e;
	}
	// Lorenz attractor, running at a speed set by TUNE and P2
	if (c.first) {
		s.z[0] = c.rng->bi() * 10.f;
		s.z[1] = c.rng->bi() * 10.f;
		s.z[2] = 20.f + c.rng->bi() * 5.f;
	}
	float rho = 20.f + 16.f * c.p1;
	float step = c.f * (0.2f + 2.f * c.p2) * c.dt * 0.8f;
	int n = 1 + (int) (step / 0.004f);
	n = std::min(n, 16);
	float h = step / n;
	for (int i = 0; i < n; i++) {
		float x = s.z[0], y = s.z[1], z = s.z[2];
		s.z[0] += h * 10.f * (y - x);
		s.z[1] += h * (x * (rho - z) - y);
		s.z[2] += h * (x * y - 2.6667f * z);
	}
	if (!std::isfinite(s.z[0])) s.z[0] = s.z[1] = 1.f, s.z[2] = 20.f;
	return std::tanh(s.z[0] * 0.07f) * 1.1f * c.e;
}

inline float runAlgo(int algo, AlgoState& s, const Ctx& c) {
	switch (algo) {
		case 0: return algoFold(s, c);
		case 1: return algoWavetable(s, c);
		case 2: return algoResonant(s, c);
		case 3: return algoComplex(s, c);
		case 4: return algoSnare(s, c);
		case 5: return algoSlap(s, c) * 1.6f;
		case 6: return algoTom(s, c);
		case 7: return algoNoiseHat(s, c) * (c.mode == 2 ? 0.55f : 1.f);
		case 8: return algoPulseHat(s, c);
		case 9: return algoKarplus(s, c);
		case 10: return algoSample(s, c) * 1.3f;
		case 11: return algoMetal(s, c) * (c.mode == 0 ? 0.65f : 1.f);
		case 12: return algoParticles(s, c);
		case 13: return algoCircuit909(s, c);
		default: return algoChaos(s, c);
	}
}

// Built-in sounds for the three SAMPLE slots until you load your own WAVs
inline void makeDefaultSample(int slot, Sample& out) {
	Rng r;
	const float S = 48000.f;
	out.sr = S;
	if (slot == 0) { // rim click: two damped modes + click
		out.data.assign((int) (0.25f * S), 0.f);
		for (size_t i = 0; i < out.data.size(); i++) {
			float t = i / S;
			out.data[i] = 0.6f * std::sin(TWO_PI * 1650.f * t) * std::exp(-t / 0.018f) + 0.4f * std::sin(TWO_PI * 520.f * t) * std::exp(-t / 0.035f) + r.bi() * std::exp(-t / 0.0015f) * 0.5f;
		}
	}
	else if (slot == 1) { // wood block
		out.data.assign((int) (0.3f * S), 0.f);
		for (size_t i = 0; i < out.data.size(); i++) {
			float t = i / S;
			out.data[i] = 0.8f * std::sin(TWO_PI * 760.f * t) * std::exp(-t / 0.04f) + 0.3f * std::sin(TWO_PI * 2140.f * t) * std::exp(-t / 0.012f);
		}
	}
	else { // shaker
		out.data.assign((int) (0.35f * S), 0.f);
		SVF f;
		float g = svfG(6500.f, S);
		for (size_t i = 0; i < out.data.size(); i++) {
			float t = i / S;
			f.process(r.bi(), g, 1.2f);
			out.data[i] = f.bp * std::min(1.f, t / 0.03f) * std::exp(-t / 0.08f) * 2.f;
		}
	}
}


static const int NUM_SLOTS = 6; // 4 sounding hits + room for the oldest ones to fade out

struct Hit {
	bool active, choking, firstA, firstB;
	int algoA, algoB, mode; // latched at the hit, so knob moves never garble a ringing tail
	float t, e, pe, fade, accent;
	float dTune, dDecay, dCut, dP1, dP2, gain;
	float eCoefT, eCoef; // cached decay coefficient (for decayT == eCoefT)
	SVF filter;
	AlgoState a, b;
	Rng rng;
	Hit() : active(false), choking(false), firstA(false), firstB(false), algoA(0), algoB(0), mode(0), t(0), e(0), pe(0), fade(1), accent(0),
	        dTune(0), dDecay(1), dCut(0), dP1(0), dP2(0), gain(1), eCoefT(-1), eCoef(0) {}
};

struct Engine {
	float sr, dt;
	Hit hits[NUM_SLOTS];
	int lastHit;
	Rng rng;
	Sample samples[3];
	// MUTATE offsets (shown on the screen)
	float mutP1, mutP2, mutMorph;
	int mutMode;
	// smoothing (knob and CV steps never zipper)
	float smCoef, smTimeCoef, sLevel, sCut, sDrive, sSend, sTime;
	bool smInit;
	// voice stage
	float dcX, dcY, dc2X, dc2Y, toneLp, uPrev;
	std::vector<float> dline;
	int dW;
	float dLp, dHp, wowPh;
	// cached control maths
	float cDecayIn, cDecay, cSweepIn, cSweepCoef, cBendIn, cBend, cToneIn, cToneCoef;
	// outputs
	float out, env, sendOut, meter;

	Engine() : sr(0.f), dt(1.f / 48000.f), lastHit(0), mutP1(0), mutP2(0), mutMorph(0), mutMode(0),
	           smCoef(0), smTimeCoef(0), sLevel(0), sCut(0), sDrive(0), sSend(0), sTime(0), smInit(false),
	           dcX(0), dcY(0), dc2X(0), dc2Y(0), toneLp(0), uPrev(0), dW(0), dLp(0), dHp(0), wowPh(0),
	           cDecayIn(-1), cDecay(0), cSweepIn(-1), cSweepCoef(0), cBendIn(-1), cBend(0), cToneIn(-1), cToneCoef(0),
	           out(0), env(0), sendOut(0), meter(0) {
		setSampleRate(48000.f);
		makeDefaultSamples();
	}

	void setSampleRate(float s) {
		if (s == sr && !dline.empty()) return;
		sr = s;
		dt = 1.f / s;
		dline.assign((size_t) (s * 2.2f), 0.f);
		dW = 0;
		smCoef = 1.f - std::exp(-dt / 0.008f);
		smTimeCoef = 1.f - std::exp(-dt / 0.08f);
		cSweepIn = cToneIn = -1.f; // recompute rate-dependent caches
		for (int i = 0; i < NUM_SLOTS; i++) hits[i].eCoefT = -1.f;
	}

	void reset() {
		for (int i = 0; i < NUM_SLOTS; i++) hits[i].active = false;
		std::fill(dline.begin(), dline.end(), 0.f);
		dcX = dcY = dc2X = dc2Y = toneLp = dLp = dHp = uPrev = 0.f;
		smInit = false;
		clearMutation();
	}

	void clearMutation() {
		mutP1 = mutP2 = mutMorph = 0.f;
		mutMode = 0;
	}

	void makeDefaultSamples() {
		for (int k = 0; k < 3; k++) makeDefaultSample(k, samples[k]);
	}

	int effMode(const Controls& c) const { return (c.mode + mutMode) % 3; }
	float effP1(const Controls& c) const { return std::min(1.f, std::max(0.f, c.p1 + mutP1)); }
	float effP2(const Controls& c) const { return std::min(1.f, std::max(0.f, c.p2 + mutP2)); }
	float effMorph(const Controls& c) const { return std::min(1.f, std::max(0.f, c.morph + mutMorph)); }

	void choke() {
		for (int i = 0; i < NUM_SLOTS; i++)
			if (hits[i].active) hits[i].choking = true;
	}

	// accent 0..1
	void hit(float accent, const Controls& c) {
		// MUTATE: offsets take a step at some hits; LOCK holds them
		if (!c.lock && c.mutate > 0.f) {
			float m = c.mutate;
			if (rng.uni() < m) {
				mutP1 = std::max(-0.5f, std::min(0.5f, mutP1 * 0.97f + rng.bi() * 0.15f * m));
				mutP2 = std::max(-0.5f, std::min(0.5f, mutP2 * 0.97f + rng.bi() * 0.15f * m));
				mutMorph = std::max(-0.6f, std::min(0.6f, mutMorph * 0.97f + rng.bi() * 0.2f * m));
			}
			if (rng.uni() < m * 0.15f) mutMode = (mutMode + 1) % 3;
		}
		if (isDrone(c.decay)) choke();
		// four sounding hits at most: a fifth fades the oldest out (3 ms) instead of cutting it
		int sounding = 0, oldest = -1;
		for (int i = 0; i < NUM_SLOTS; i++)
			if (hits[i].active && !hits[i].choking) {
				sounding++;
				if (oldest < 0 || hits[i].t > hits[oldest].t) oldest = i;
			}
		if (sounding >= MAX_HITS && oldest >= 0) hits[oldest].choking = true;
		int slot = -1;
		for (int i = 0; i < NUM_SLOTS; i++)
			if (!hits[i].active) {
				slot = i;
				break;
			}
		if (slot < 0) { // all busy: take the quietest fading one
			for (int i = 0; i < NUM_SLOTS; i++)
				if (hits[i].choking && (slot < 0 || hits[i].fade < hits[slot].fade)) slot = i;
			if (slot < 0) slot = oldest >= 0 ? oldest : 0;
		}
		Hit& h = hits[slot];
		h.active = true;
		h.choking = false;
		h.firstA = h.firstB = true;
		h.algoA = c.algoA;
		h.algoB = c.algoB;
		h.mode = effMode(c);
		h.e = 1.f;
		h.pe = 1.f;
		h.fade = 1.f;
		h.accent = std::min(1.f, std::max(0.f, accent));
		float w = c.wear;
		h.dTune = rng.bi() * w * 0.6f;
		h.dDecay = std::exp(rng.bi() * w * 0.35f);
		h.dCut = rng.bi() * w * 0.6f;
		h.dP1 = rng.bi() * w * 0.12f;
		h.dP2 = rng.bi() * w * 0.12f;
		h.gain = std::exp(rng.bi() * w * 0.25f);
		h.t = -rng.uni() * w * 0.004f; // a hand is never quite on the grid
		h.rng.s = rng.next() | 1u;
		h.eCoefT = -1.f;
		h.filter.reset();
		h.a.reset(c.algoA == 9);
		h.b.reset(c.algoB == 9);
		lastHit = slot;
	}

	void process(const Controls& c) {
		if (!smInit) {
			sLevel = c.level;
			sCut = c.cutoff;
			sDrive = c.drive;
			sSend = c.send;
			sTime = delaySeconds(c.time);
			smInit = true;
		}
		sLevel += (c.level - sLevel) * smCoef;
		sCut += (c.cutoff - sCut) * smCoef;
		sDrive += (c.drive - sDrive) * smCoef;
		sSend += (c.send - sSend) * smCoef;
		sTime += (delaySeconds(c.time) - sTime) * smTimeCoef;

		float morph = effMorph(c);
		float p1 = effP1(c), p2 = effP2(c);
		bool drone = isDrone(c.decay);
		if (c.decay != cDecayIn) {
			cDecayIn = c.decay;
			cDecay = decaySeconds(c.decay);
		}
		if (c.sweep != cSweepIn) {
			cSweepIn = c.sweep;
			cSweepCoef = std::exp(-dt / sweepSeconds(c.sweep));
		}
		if (c.bend != cBendIn) {
			cBendIn = c.bend;
			cBend = 48.f * std::pow(c.bend, 1.5f);
		}
		float k = 2.f - 1.9f * c.res;
		float resComp = 1.f / (1.f + 1.5f * c.res);
		float cutBase = cutoffHz(sCut);
		bool needA = morph < 0.999f, needB = morph > 0.001f;
		// between linear and equal power: no hole in the middle, no +3 dB bump on similar sounds
		float ga = 0.5f * ((1.f - morph) + std::cos(morph * PI * 0.5f));
		float gb = 0.5f * (morph + std::sin(morph * PI * 0.5f));
		float nsc = std::sqrt(sr / 48000.f);

		float sum = 0.f;
		env = 0.f;
		for (int i = 0; i < NUM_SLOTS; i++) {
			Hit& h = hits[i];
			if (!h.active) continue;
			h.t += dt;
			if (h.t < 0.f) continue;
			float decayT = cDecay * h.dDecay;
			if (decayT != h.eCoefT) {
				h.eCoefT = decayT;
				h.eCoef = std::exp(-dt / decayT);
			}
			bool first = h.firstA && h.firstB;
			if (!first) {
				if (!drone) h.e *= h.eCoef;
				h.pe *= cSweepCoef;
			}
			float f = c.tuneHz * std::exp2((h.dTune + cBend * h.pe) * (1.f / 12.f));
			f = std::min(f, 0.45f * sr);
			Ctx x;
			x.f = f;
			x.e = h.e;
			x.t = h.t;
			x.p1 = std::min(1.f, std::max(0.f, p1 + h.dP1));
			x.p2 = std::min(1.f, std::max(0.f, p2 + h.dP2));
			x.sr = sr;
			x.dt = dt;
			x.decayT = decayT;
			x.nsc = nsc;
			x.mode = h.mode;
			x.drone = drone;
			x.accent = h.accent * c.accentAmt;
			x.rng = &h.rng;
			x.samples = samples;
			float y = 0.f;
			if (needA) {
				x.first = h.firstA;
				y += ga * runAlgo(h.algoA, h.a, x);
				h.firstA = false;
			}
			if (needB) {
				x.first = h.firstB;
				y += gb * runAlgo(h.algoB, h.b, x);
				h.firstB = false;
			}
			// click (a short pulse plus a noise tick) + noise
			float clickEnv = std::exp(-h.t / 0.0015f);
			float clk = (h.rng.bi() * 0.6f * nsc + std::exp(-h.t / 0.0003f)) * clickEnv * c.click * 1.5f;
			float nz = h.rng.bi() * nsc * h.e * h.e * c.noise * 0.8f;
			y += clk + nz;
			// filter
			float fc = cutBase * std::exp2(c.fenv * 6.f * h.e + h.dCut + x.accent);
			h.filter.process(y, svfG(fc, sr), k);
			float fy = c.vcf == 0 ? h.filter.hp : c.vcf == 1 ? h.filter.bp * k : h.filter.lp;
			fy *= resComp;
			if (h.choking) {
				h.fade *= std::exp(-dt / 0.003f);
				if (h.fade < 1e-3f) h.active = false;
			}
			sum += fy * h.fade * h.gain * (1.f + x.accent);
			if (!drone && h.e < 1e-4f) h.active = false;
			if (i == lastHit) env = h.e * h.fade;
		}

		// DC block
		float R = 1.f - TWO_PI * 10.f / sr;
		dcY = sum - dcX + R * dcY;
		dcX = sum;
		float v = dcY;
		// overdrive: DRIVE is the dry/wet of an asymmetric overdrive (antiderivative anti-aliased tanh)
		float d = sDrive;
		float gain = 1.5f + d * d * 14.f;
		float bias = std::min(0.25f * gain / 1.5f, 0.9f);
		float u = gain * v + bias;
		float du = u - uPrev;
		float sat = std::fabs(du) > 1e-4f ? (logcosh(u) - logcosh(uPrev)) / du : std::tanh(0.5f * (u + uPrev));
		uPrev = u;
		float wet = sat - std::tanh(bias);
		dc2Y = wet - dc2X + R * dc2Y;
		dc2X = wet;
		wet = dc2Y;
		v = v * (1.f - d) + wet * d;
		// TONE: one-pole low-pass 600 Hz .. 19 kHz
		if (c.tone != cToneIn) {
			cToneIn = c.tone;
			float tc = std::min(600.f * std::pow(2.f, c.tone * 5.f), 0.45f * sr);
			cToneCoef = 1.f - std::exp(-TWO_PI * tc / sr);
		}
		toneLp += (v - toneLp) * cToneCoef;
		v = toneLp;

		// FX: bucket-brigade style delay (internal unless SEND is patched)
		float send = v * sSend;
		sendOut = send * 5.f;
		float ret = 0.f;
		int N = (int) dline.size();
		if (!c.sendPatched) {
			wowPh = wrap1(wowPh + 0.43f * dt);
			float dsec = sTime * (1.f + 0.003f * sinp(wowPh));
			float rp = dW - dsec * sr;
			while (rp < 0.f) rp += N;
			int i0 = (int) rp;
			float fr = rp - i0;
			float y = dline[i0 % N] * (1.f - fr) + dline[(i0 + 1) % N] * fr;
			float lpHz = 5500.f * (1.f - 0.6f * c.time);
			dLp += (y - dLp) * (1.f - std::exp(-TWO_PI * lpHz / sr));
			dHp += (dLp - dHp) * (1.f - std::exp(-TWO_PI * 60.f / sr));
			ret = dLp - dHp;
			float fb = c.fdbk * 1.05f;
			dline[dW] = std::tanh(send + ret * fb + rng.bi() * 1e-5f);
		}
		else
			dline[dW] = 0.f; // let the internal delay empty while it's bypassed
		dW = (dW + 1) % N;
		float o = (v + ret * 0.8f) * sLevel * 5.f;
		out = 10.f * std::tanh(o * 0.1f);
		meter = std::max(meter * 0.9995f, std::fabs(out) * 0.2f);
	}
};

} // namespace thunder
