// Essphor Mod: WAVE, RANDOM, FOLLOW and ADSR. Part C of docs/BUILD-INSTRUCTIONS.md.
// No Rack dependency. Outputs are in volts.
#pragma once
#include "Common.hpp"
#include <vector>

namespace essphor {

enum RateMode { RM_FREE, RM_STRAIGHT, RM_TRIPLET, RM_DOTTED };
static const int NUM_MOD_DIVS = 10;
static const double MOD_DIVS[NUM_MOD_DIVS] = {32, 16, 8, 4, 2, 1, 0.5, 0.25, 0.125, 0.0625};   // beats: 8/1 .. 1/64
static const char* const MOD_DIV_NAMES[NUM_MOD_DIVS] = {"8/1", "4/1", "2/1", "1/1", "1/2", "1/4", "1/8", "1/16", "1/32", "1/64"};

inline int modDivIndex(double k) { return std::max(0, std::min(NUM_MOD_DIVS - 1, (int)std::lround(k * (NUM_MOD_DIVS - 1)))); }
inline double modFreeHz(double k) { return 0.01 * std::pow(10000.0, std::max(0.0, std::min(1.0, k))); }
inline double modPeriodBeats(int mode, double k) {
	double b = MOD_DIVS[modDivIndex(k)];
	if (mode == RM_TRIPLET) b *= 2.0 / 3.0;
	if (mode == RM_DOTTED) b *= 1.5;
	return b;
}

// Phase source shared by WAVE and RANDOM: free running, or locked to the clock's beat phase.
struct ModPhase {
	double free = 0.0;
	// returns position in cycles (unbounded)
	double step(int mode, double rateKnob, const Clock& clk, double dt) {
		if (clk.resetNow) free = 0.0;
		if (mode == RM_FREE) { free += modFreeHz(rateKnob) * dt; return free; }
		free = clk.phase / modPeriodBeats(mode, rateKnob);
		return free;
	}
};

inline double frac(double x) { return x - std::floor(x); }

// ---------------------------------------------------------------- WAVE
enum WaveShape { SH_SIN, SH_TRI, SH_SQUARE };
struct WaveParams { int rateMode = RM_FREE; double rate = 0.5, amount = 1, phase = 0, offset = 0, skew = 0, fold = 0, curve = 0, spread = 0; int shape = SH_SIN; };

inline double waveAt(const WaveParams& p, double pos) {
	double ph = frac(pos + p.phase / 360.0);
	const double s = 0.5 + 0.49 * p.skew;
	ph = ph < s ? 0.5 * ph / s : 0.5 + 0.5 * (ph - s) / (1 - s);
	double y;
	if (p.shape == SH_SIN) y = std::sin(2 * PI * ph);
	else if (p.shape == SH_TRI) y = ph < 0.25 ? 4 * ph : ph < 0.75 ? 2 - 4 * ph : 4 * ph - 4;
	else y = ph < 0.5 ? 1.0 : -1.0;
	const double e = std::pow(2.0, -2.0 * p.curve);
	y = (y < 0 ? -1 : 1) * std::pow(std::fabs(y), e);
	if (p.fold > 0) {
		double z = y * (1 + 4 * p.fold);
		double m = std::fmod(z + 1, 4.0); if (m < 0) m += 4;
		y = 1 - std::fabs(m - 2);
	}
	return y;
}

struct Wave {
	WaveParams p; ModPhase ph;
	double outA = 0, outB = 0;
	void process(const Clock& clk, double dt) {
		const double pos = ph.step(p.rateMode, p.rate, clk, dt);
		outA = std::max(-10.0, std::min(10.0, 5.0 * (p.offset + p.amount * waveAt(p, pos))));
		outB = std::max(-10.0, std::min(10.0, 5.0 * (p.offset + p.amount * waveAt(p, pos + 0.5 * p.spread))));
	}
};

// ---------------------------------------------------------------- RANDOM
struct RandomParams { int rateMode = RM_STRAIGHT; double rate = 0.6667, amount = 1, phase = 0, offset = 0, variation = 0, smooth = 0, spread = 0; int length = 8; };

struct RandomSeq {
	RandomParams p; ModPhase ph; Rng rng;
	uint64_t seedA = 1, seedB = 2;
	double valA[32], valB[32];
	long long lastStep = -1;
	double yA = 0, yB = 0, outA = 0, outB = 0;
	void seed(uint64_t a, uint64_t b) {
		seedA = a; seedB = b;
		Rng r; r.seed(a); for (int i = 0; i < 32; i++) valA[i] = r.bipolar();
		r.seed(b); for (int i = 0; i < 32; i++) valB[i] = r.bipolar();
		rng.seed(a ^ 0xABCDEF1234567ull);
	}
	void process(const Clock& clk, double dt) {
		const double pos = ph.step(p.rateMode, p.rate, clk, dt) + p.phase / 360.0;
		const long long st = (long long)std::floor(pos);
		const int L = std::max(1, std::min(32, p.length));
		int idx = (int)(((st % L) + L) % L);
		if (st != lastStep) {
			if (lastStep >= 0 && p.variation > 0) {
				if (rng.uniform() < p.variation) valA[idx] = rng.bipolar();
				if (rng.uniform() < p.variation) valB[idx] = rng.bipolar();
			}
			lastStep = st;
		}
		// one step lasts this many seconds
		double stepSec = p.rateMode == RM_FREE ? 1.0 / modFreeHz(p.rate) : modPeriodBeats(p.rateMode, p.rate) / std::max(1e-6, clk.bps);
		const double tau = p.smooth * stepSec;
		const double a = tau > 1e-6 ? 1.0 - std::exp(-dt / tau) : 1.0;
		yA += (valA[idx] - yA) * a; yB += (valB[idx] - yB) * a;
		const double b = (1 - p.spread) * yA + p.spread * yB;
		outA = std::max(-10.0, std::min(10.0, 5.0 * (p.offset + p.amount * yA)));
		outB = std::max(-10.0, std::min(10.0, 5.0 * (p.offset + p.amount * b)));
	}
};

// ---------------------------------------------------------------- FOLLOW
struct FollowParams { double delay = 0, amount = 1, rise = 0.005, fall = 0.15, input = 1, filter = 0, hold = 0, spread = 0; };

struct Follow {
	FollowParams p;
	double fs = 48000;
	Svf fl[2];
	double envA = 0, envB = 0, heldA = 0, heldB = 0, holdPhase = 0;
	std::vector<float> dA, dB; int dw = 0;
	double outA = 0, outB = 0;
	double lastFilter = 1e9;
	void init(double sampleRate) {
		fs = sampleRate;
		int n = (int)(0.5 * fs) + 2;
		dA.assign(n, 0.f); dB.assign(n, 0.f); dw = 0;
		envA = envB = heldA = heldB = 0; lastFilter = 1e9;
	}
	void process(double inL, double inR, const Clock& clk, bool clockPatched, double dt) {
		// DJ-style filter
		if (p.filter != lastFilter) {
			lastFilter = p.filter;
			double f = p.filter < 0 ? 20000.0 * std::pow(80.0 / 20000.0, -p.filter) : 20.0 * std::pow(250.0, p.filter);
			for (auto& s : fl) s.set(f, 0.707, fs);
		}
		double x[2] = {inL, inR};
		for (int c = 0; c < 2; c++) {
			double lp, bp, hp; fl[c].run(x[c], lp, bp, hp);
			x[c] = std::fabs(p.filter > 0.001 ? hp : p.filter < -0.001 ? lp : x[c]) / 5.0 * p.input;
		}
		const double mid = 0.5 * (x[0] + x[1]);
		const double inA = (1 - p.spread) * mid + p.spread * x[0];
		const double inB = (1 - p.spread) * mid + p.spread * x[1];
		auto follow = [&](double& env, double in) {
			const double tau = in > env ? p.rise : p.fall;
			const double a = tau > 1e-6 ? 1.0 - std::exp(-dt / tau) : 1.0;
			env += (in - env) * a;
			env = std::min(1.0, env);
		};
		follow(envA, inA); follow(envB, inB);
		// delay
		const int n = (int)dA.size();
		dA[dw] = (float)envA; dB[dw] = (float)envB;
		int dly = (int)std::lround(p.delay * fs); dly = std::max(0, std::min(n - 1, dly));
		int r = dw - dly; if (r < 0) r += n;
		double a = dA[r], b = dB[r];
		if (++dw >= n) dw = 0;
		// sample and hold
		if (p.hold > 0.02) {
			bool tick = false;
			if (clockPatched) {
				static const double HB[6] = {4, 2, 1, 0.5, 0.25, 0.125};
				int i = std::max(0, std::min(5, (int)std::lround((p.hold - 0.02) / 0.98 * 5)));
				double pos = clk.phase / HB[i];
				tick = std::floor(pos) != std::floor(holdPhase) || clk.resetNow;
				holdPhase = pos;
			} else {
				double hz = 0.5 * std::pow(100.0, (p.hold - 0.02) / 0.98);
				holdPhase += hz * dt;
				if (holdPhase >= 1) { holdPhase -= std::floor(holdPhase); tick = true; }
			}
			if (tick) { heldA = a; heldB = b; }
			a = heldA; b = heldB;
		}
		outA = 10.0 * p.amount * a; outB = 10.0 * p.amount * b;
	}
};

// ---------------------------------------------------------------- ADSR
struct AdsrParams { double attack = 0.005, decay = 0.2, sustain = 0.6, release = 0.3, attC = 0, decC = 0, relC = 0, spread = 0; };

struct Adsr {
	AdsrParams p;
	double fs = 48000;
	enum Stage { IDLE, ATT, DEC, SUS, REL } stage = IDLE;
	double t = 0, from = 0, env = 0;
	bool gate = false;
	std::vector<float> dl; int dw = 0;
	double outA = 0, outB = 0;
	void init(double sampleRate) { fs = sampleRate; dl.assign((int)(0.25 * fs) + 2, 0.f); dw = 0; stage = IDLE; env = 0; }
	static double shape(double x, double c) { return std::pow(std::max(0.0, std::min(1.0, x)), std::pow(2.0, 2.0 * c)); }
	void process(bool g, double dt) {
		if (g && !gate) { stage = ATT; t = 0; from = env; }
		if (!g && gate && stage != IDLE) { stage = REL; t = 0; from = env; }
		gate = g;
		switch (stage) {
			case ATT: t += dt; env = from + (1 - from) * shape(t / p.attack, p.attC); if (t >= p.attack) { stage = DEC; t = 0; env = 1; } break;
			case DEC: t += dt; env = 1 - (1 - p.sustain) * shape(t / p.decay, p.decC); if (t >= p.decay) { stage = SUS; env = p.sustain; } break;
			case SUS: env = p.sustain; break;
			case REL: t += dt; env = from * (1 - shape(t / p.release, p.relC)); if (t >= p.release) { stage = IDLE; env = 0; } break;
			default: env = 0; break;
		}
		const int n = (int)dl.size();
		dl[dw] = (float)env;
		int d = (int)std::lround(p.spread * 0.25 * fs); d = std::max(0, std::min(n - 1, d));
		int r = dw - d; if (r < 0) r += n;
		outA = 10.0 * env; outB = 10.0 * dl[r];
		if (++dw >= n) dw = 0;
	}
};

}  // namespace essphor
