// Cubit DSP: clock-is-pitch 8-bit wavetable core, rendered exactly with a quadratic
// B-spline kernel (spec section 5). No Rack dependency, so test/render.cpp can use it.
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace cubit {

// ---- (ear) constants: revisit these first (spec 1, 4.1, 4.2) --------------------
static const double LIN_FM_FULL_VOLTS = 5.0;     // LIN at 1 with 5 V = +-100 % of the frequency
static const double DRIFT_CENTS = 25.0;          // wander at DRIFT 1
static const double DRIFT_JITTER = 0.04;         // edge jitter std, in steps, at DRIFT 1
static const double DRIFT_WANDER_SLOW_HZ = 0.2;
static const double DRIFT_WANDER_FAST_HZ = 3.0;
static const bool SUB_TOGGLES_ON_SYNC = true;    // sub flip-flop clocked by the counter's top bit falling
static const double MAX_CLOCK_HZ = 20000.0;      // note frequency clamp
static const double C4_HZ = 261.6256;

enum Channel { CH_DAC, CH_SUB, CH_T1, CH_T2, CH_T3, CH_T4, CH_CLK, NCH };
static const int TAP_BITS[4] = {0, 3, 5, 7};
static const int SPLICE_LEN[7] = {0, 32, 16, 8, 4, 2, 1};   // OFF, 1/2 .. 1/64
static const int RES_STEPS[5] = {64, 32, 16, 8, 4};

// What the data path does to a step, fixed for one host sample (spec 6, 7).
struct Path {
	int wave = 0;          // 0..511
	int spliceLen = 0;     // 0 = off, else block length in steps
	int withBit = 0;       // 0..8
	int stuck = 0;         // 0 off, 1 LO, 2 HI
	int stuckBit = 7;
	int resMask = 63;      // step & resMask
	int depthBits = 8;

	inline int byteAt(const uint8_t* rom, int step) const {
		int w = wave;
		if (spliceLen && ((step / spliceLen) & 1)) w ^= (1 << withBit);
		int b = rom[(w & 511) * 64 + (step & resMask)];
		if (stuck == 1) b &= ~(1 << stuckBit);
		else if (stuck == 2) b |= (1 << stuckBit);
		return b & 0xFF;
	}
	inline int dacByte(int byte) const {
		int drop = 8 - depthBits;
		return byte & (0xFF << drop) & 0xFF;
	}
	// OUT re-centring offset when bits are dropped (spec 7.3)
	inline double dacOffset() const { return ((1 << (8 - depthBits)) - 1) * 0.5; }
};

// ---- 2x decimator: Kaiser-windowed lowpass, computed at the output rate -------------
// Not a half-band: a half-band is -6 dB at the output Nyquist, and its sharp edge there rings
// hard at every edge and lets the top octave alias back. This one is flat to 15 kHz, -2 dB at
// 18 kHz, -59 dB at 24 kHz. Any decimator this steep rings about 6 % at a staircase edge (the
// band-limited truth at 48 kHz); the kernel itself never overshoots (os = 1 at 88 kHz and up).
#ifndef CUBIT_DEC_K
#define CUBIT_DEC_K 24
#endif
#ifndef CUBIT_DEC_FC
#define CUBIT_DEC_FC 19500.0
#endif
#ifndef CUBIT_DEC_BETA
#define CUBIT_DEC_BETA 8.0
#endif
struct DecimFilter {
	static const int K = CUBIT_DEC_K;        // taps -K..K at the internal rate
	static const int N = 2 * K + 1;
	float h[N];
	DecimFilter() {
		auto bessel0 = [](double x) {
			double s = 1, t = 1;
			for (int k = 1; k < 30; k++) { t *= (x / (2 * k)) * (x / (2 * k)); s += t; }
			return s;
		};
		const double beta = CUBIT_DEC_BETA;
		const double fc = CUBIT_DEC_FC / 96000.0;   // cycles per internal sample (os = 2 at 48 kHz)
		double norm = bessel0(beta), sum = 0, hd[N];
		for (int i = 0; i < N; i++) {
			int n = i - K;
			double sinc = n == 0 ? 2 * fc : std::sin(2 * M_PI * fc * n) / (M_PI * n);
			double r = (double)n / (K + 1);
			hd[i] = sinc * bessel0(beta * std::sqrt(std::max(0.0, 1 - r * r))) / norm;
			sum += hd[i];
		}
		for (int i = 0; i < N; i++) h[i] = (float)(hd[i] / sum);
	}
};

struct Decim {
	float buf[2 * DecimFilter::N] = {};
	int idx = 0;
	void reset() { std::memset(buf, 0, sizeof(buf)); idx = 0; }
	// push two inputs (older first), return one output
	inline double process(const DecimFilter& hb, double a, double b) {
		push(a); push(b);
		const int N = DecimFilter::N;
		const float* x = buf + idx;             // x[0] oldest .. x[N-1] newest (mirrored buffer)
		float y = 0.f;
		for (int i = 0; i < N; i++) y += hb.h[i] * x[i];
		return y;
	}
	inline void push(double xd) {
		const int N = DecimFilter::N;
		float x = (float)xd;
		buf[idx] = x; buf[idx + N] = x;
		if (++idx >= N) idx = 0;
	}
};

// ---- small RNG ------------------------------------------------------------------
struct Rng {
	uint64_t s = 0x9E3779B97F4A7C15ull;
	void seed(uint64_t x) { s = x * 0x9E3779B97F4A7C15ull + 0x632BE59BD9B4E019ull; if (!s) s = 1; next(); next(); }
	inline uint64_t next() { s ^= s << 13; s ^= s >> 7; s ^= s << 17; return s; }
	inline double uni() { return (next() >> 11) * (1.0 / 9007199254740992.0); }
	inline double gauss() {
		double u1 = std::max(uni(), 1e-300), u2 = uni();
		return std::sqrt(-2 * std::log(u1)) * std::cos(2 * M_PI * u2);
	}
};

// Per host sample input to a voice.
struct Frame {
	double rate = 0;        // steps per second (internal clock), may be negative (through-zero)
	bool ext = false;       // CLK IN patched: rate ignored, edges from extT
	double extT = -1;       // rising edge of CLK IN inside this host sample (0..1), -1 none
	bool extHigh = false;   // squared-up CLK IN for CLK OUT in ext mode
	double syncT = -1;      // SYNC rising edge (0..1), -1 none
	double drift = 0;       // 0..1
	Path path;
	unsigned mask = (1u << NCH) - 1;   // which channels to render
};

struct Result {
	double v[NCH] = {};     // rendered channel values: DAC in byte units, SUB +-5, taps 0/10, CLK 0/5
};

struct Voice {
	// counter
	double pos = 0;         // clock phase in steps
	int64_t count = 0;      // running step count
	double jit = 0;         // jitter of the next edge
	int sub = 1;            // +-1
	bool clkHigh = true;
	// renderer
	int os = 2;
	double fsI = 96000;
	double P[NCH] = {}, Q[NCH] = {}, Q2[NCH] = {};   // carried kernel terms
	Decim dec[NCH];
	double half[NCH] = {};  // first sub-sample output waiting for the decimator
	// drift
	Rng rng;
	double wSlow = 0, wFast = 0, aSlow = 0, aFast = 0, gSlow = 0, gFast = 0;
	double fsHost = 48000;
	const DecimFilter* hb = nullptr;

	void setSeed(uint64_t s) { rng.seed(s); }

	void setSampleRate(double fs, const DecimFilter* h) {
		hb = h;
		fsHost = fs;
		int nos = fs < 88000 ? 2 : 1;
		if (nos != os) resetRenderer();
		os = nos;
		fsI = os * fs;
		aSlow = 1 - std::exp(-2 * M_PI * DRIFT_WANDER_SLOW_HZ / fs);
		aFast = 1 - std::exp(-2 * M_PI * DRIFT_WANDER_FAST_HZ / fs);
		// white noise of std g into a one-pole of coefficient a has std g * sqrt(a / (2 - a)); aim for 0.3 each
		gSlow = 0.3 / std::sqrt(aSlow / (2 - aSlow));
		gFast = 0.3 / std::sqrt(aFast / (2 - aFast));
	}

	void resetRenderer() {
		for (int c = 0; c < NCH; c++) { P[c] = Q[c] = Q2[c] = half[c] = 0; dec[c].reset(); }
	}

	void resetVoice() {
		pos = 0; count = 0; jit = 0; sub = 1; clkHigh = true;
		wSlow = wFast = 0;
		resetRenderer();
	}

	inline int step() const { return (int)(count & 63); }

	inline void values(const uint8_t* rom, const Path& p, bool ext, bool extHigh, double* v) const {
		int b = p.byteAt(rom, step());
		v[CH_DAC] = p.dacByte(b);
		v[CH_SUB] = 5.0 * sub;
		for (int i = 0; i < 4; i++) v[CH_T1 + i] = ((b >> TAP_BITS[i]) & 1) ? 10.0 : 0.0;
		v[CH_CLK] = ext ? (extHigh ? 5.0 : 0.0) : (clkHigh ? 5.0 : 0.0);
	}

	inline void forwardEdge(double drift) {
		count++;
		if ((count & 63) == 0) sub = -sub;
		clkHigh = true;
		jit = drift > 0 ? std::max(-0.4, std::min(0.4, rng.gauss() * DRIFT_JITTER * drift)) : 0.0;
	}
	inline void backwardEdge(double drift) {
		if ((count & 63) == 0) sub = -sub;
		count--;
		clkHigh = true;
		jit = drift > 0 ? std::max(-0.4, std::min(0.4, rng.gauss() * DRIFT_JITTER * drift)) : 0.0;
	}
	inline void doSync(double drift) {
		if (SUB_TOGGLES_ON_SYNC && step() >= 32) sub = -sub;
		count -= step();
		pos = (double)count;
		clkHigh = true;
		jit = drift > 0 ? std::max(-0.4, std::min(0.4, rng.gauss() * DRIFT_JITTER * drift)) : 0.0;
	}

	// One host sample. Returns the decimated channel values.
	Result process(const uint8_t* rom, const Frame& f) {
		Result r;
		// drift wander (host rate)
		double driftFactor = 1;
		if (f.drift > 0 && !f.ext) {
			wSlow += aSlow * (rng.gauss() * gSlow - wSlow);
			wFast += aFast * (rng.gauss() * gFast - wFast);
			double w = wSlow + wFast;
			w = 1.2 * std::tanh(w / 1.2);
			driftFactor = std::pow(2.0, w * DRIFT_CENTS * f.drift / 1200.0);
		}
		double inc = f.ext ? 0.0 : f.rate * driftFactor / fsI;
		double drift = f.ext ? 0.0 : f.drift;
		if (!std::isfinite(inc)) inc = 0;

		// events of this host sample, mapped to sub-samples
		int extK = -1, syncK = -1;
		double extU = 2, syncU = 2;
		if (f.ext && f.extT >= 0) { double t = f.extT * os; extK = std::min(os - 1, (int)t); extU = t - extK; }
		if (f.syncT >= 0) { double t = f.syncT * os; syncK = std::min(os - 1, (int)t); syncU = t - syncK; }
		// internal clock edges are suppressed with an external clock, and also CLK OUT halves
		const bool wantHalf = ((f.mask >> CH_CLK) & 1) && !f.ext;

		double y[2][NCH];
		for (int k = 0; k < os; k++) {
			double M0[NCH] = {}, M1[NCH] = {}, M2[NCH] = {};
			double v[NCH];
			values(rom, f.path, f.ext, f.extHigh, v);
			double u = 0;
			bool extPending = k == extK, syncPending = k == syncK;
			int events = 0;
			for (;;) {
				// candidate event times
				double tEdge = 2, tHalf = 2;
				if (inc > 0) {
					double thr = (double)count + 1 + jit;
					tEdge = u + (thr - pos) / inc;
					if (wantHalf && clkHigh) {
						double hp = (double)count + 0.5;
						tHalf = pos < hp ? u + (hp - pos) / inc : u;
					}
				}
				else if (inc < 0) {
					double thr = (double)count + std::min(0.0, jit);
					tEdge = u + (thr - pos) / inc;
					if (wantHalf && clkHigh) {
						double hp = (double)count + 0.5;
						tHalf = pos > hp ? u + (hp - pos) / inc : u;
					}
				}
				tEdge = std::max(tEdge, u);
				tHalf = std::max(tHalf, u);
				double tExt = extPending ? std::max(extU, u) : 2;
				double tSync = syncPending ? std::max(syncU, u) : 2;
				double uNext = std::min(std::min(tEdge, tHalf), std::min(tExt, tSync));
				if (uNext > 1) uNext = 1;
				if (events >= 32) uNext = 1;
				// integrate segment [u, uNext)
				if (uNext > u) {
					double d0 = uNext - u;
					double d1 = (uNext * uNext - u * u) * 0.5;
					double d2 = (uNext * uNext * uNext - u * u * u) * (1.0 / 3.0);
					for (int c = 0; c < NCH; c++) {
						if (!((f.mask >> c) & 1)) continue;
						M0[c] += v[c] * d0; M1[c] += v[c] * d1; M2[c] += v[c] * d2;
					}
				}
				if (uNext >= 1) {
					pos += inc * (1 - u);
					if (events >= 32) {
						// event cap: jump the counter to its end-of-sub-sample value
						if (inc > 0) while (pos >= (double)count + 1 + jit) forwardEdge(0);
						else if (inc < 0) while (pos < (double)count) backwardEdge(0);
					}
					break;
				}
				pos += inc * (uNext - u);
				u = uNext;
				events++;
				if (syncPending && tSync <= uNext) { doSync(drift); syncPending = false; }
				else if (extPending && tExt <= uNext) { forwardEdge(0); pos = (double)count; extPending = false; }
				else if (tEdge <= uNext) {
					if (inc > 0) forwardEdge(drift);
					else backwardEdge(drift);
				}
				else if (tHalf <= uNext) clkHigh = false;
				values(rom, f.path, f.ext, f.extHigh, v);
			}
			// kernel output for this sub-sample
			for (int c = 0; c < NCH; c++) {
				if (!((f.mask >> c) & 1)) { y[k][c] = 0; continue; }
				y[k][c] = M2[c] * 0.5 + P[c] + Q2[c];
				Q2[c] = Q[c];
				P[c] = (M0[c] + 2 * M1[c] - 2 * M2[c]) * 0.5;
				Q[c] = (M0[c] - 2 * M1[c] + M2[c]) * 0.5;
			}
		}
		for (int c = 0; c < NCH; c++) {
			if (!((f.mask >> c) & 1)) { r.v[c] = 0; continue; }
			r.v[c] = os == 2 ? dec[c].process(*hb, y[0][c], y[1][c]) : y[0][c];
		}
		// with an external clock, CLK OUT is the squared input with no kernel
		if (f.ext) r.v[CH_CLK] = f.extHigh ? 5.0 : 0.0;

		// guards (spec 11)
		if (!std::isfinite(pos) || !std::isfinite(r.v[CH_DAC])) {
			resetVoice();
			for (int c = 0; c < NCH; c++) r.v[c] = 0;
		}
		if (count > (1ll << 40) || count < -(1ll << 40)) {
			int64_t sh = (count / 128) * 128;
			count -= sh; pos -= (double)sh;
		}
		return r;
	}
};

// ---- crossing-time Schmitt (spec 4.3) -------------------------------------------
struct EdgeDetect {
	bool high = false;
	float prev = 0;
	// returns crossing time 0..1 inside this host sample, or -1
	inline double process(float v) {
		double t = -1;
		if (!high) {
			if (v >= 1.f) {
				high = true;
				float d = v - prev;
				t = d > 0 ? (1.0 - prev) / d : 1.0;
				t = std::max(0.0, std::min(1.0, t));
				// the host sample spans [previous input, this input], so t is the edge's place inside it
			}
		}
		else if (v <= 0.1f) high = false;
		prev = v;
		return t;
	}
};

// ---- ROM files (spec 8.2) -------------------------------------------------------
inline bool readFile(const char* path, std::vector<uint8_t>& d) {
	FILE* fp = std::fopen(path, "rb");
	if (!fp) return false;
	std::fseek(fp, 0, SEEK_END);
	long len = std::ftell(fp);
	std::fseek(fp, 0, SEEK_SET);
	if (len < 0 || len > 512L * 1024 * 1024) { std::fclose(fp); return false; }
	d.resize(len);
	size_t got = len ? std::fread(d.data(), 1, len, fp) : 0;
	std::fclose(fp);
	return (long)got == len;
}

// Mono-summed float samples from a PCM (8 / 16 / 24 / 32-bit) or float WAV.
inline bool parseWav(const std::vector<uint8_t>& d, std::vector<float>& out) {
	if (d.size() < 44 || std::memcmp(d.data(), "RIFF", 4) || std::memcmp(d.data() + 8, "WAVE", 4)) return false;
	auto u16 = [&](size_t o) { return (uint32_t)d[o] | ((uint32_t)d[o + 1] << 8); };
	auto u32 = [&](size_t o) { return u16(o) | (u16(o + 2) << 16); };
	int fmt = 0, ch = 0, bits = 0;
	size_t dataOff = 0, dataLen = 0, o = 12;
	while (o + 8 <= d.size()) {
		uint32_t sz = u32(o + 4);
		if (!std::memcmp(&d[o], "fmt ", 4) && o + 8 + 16 <= d.size()) {
			fmt = (int)u16(o + 8); ch = (int)u16(o + 10); bits = (int)u16(o + 22);
			if (fmt == 0xFFFE && sz >= 40 && o + 8 + 26 <= d.size()) fmt = (int)u16(o + 8 + 24);
		}
		else if (!std::memcmp(&d[o], "data", 4)) {
			dataOff = o + 8;
			dataLen = std::min((size_t)sz, d.size() - dataOff);
			break;
		}
		o += 8 + (size_t)sz + (sz & 1);
	}
	if (!dataOff || ch < 1 || !(fmt == 1 || fmt == 3)) return false;
	int bps = bits / 8;
	if (bps < 1 || bps > 8 || (fmt == 3 && bps != 4 && bps != 8)) return false;
	size_t frames = std::min(dataLen / (bps * ch), (size_t)(512 * 2048));
	out.assign(frames, 0.f);
	const uint8_t* p = d.data() + dataOff;
	for (size_t i = 0; i < frames; i++) {
		double sum = 0;
		for (int c = 0; c < ch; c++) {
			const uint8_t* s = p + (i * ch + c) * bps;
			double v = 0;
			if (fmt == 1) {
				if (bps == 1) v = (s[0] - 128) / 128.0;
				else {
					int32_t x = 0;
					for (int b = 0; b < std::min(bps, 4); b++) x |= (int32_t)((uint32_t)s[bps - 1 - b] << (24 - 8 * b));
					v = x / 2147483648.0;
				}
			}
			else if (bps == 4) { float x; std::memcpy(&x, s, 4); v = x; }
			else { double x; std::memcpy(&x, s, 8); v = x; }
			sum += std::isfinite(v) ? v : 0.0;
		}
		out[i] = (float)(sum / ch);
	}
	return frames > 0;
}

enum Normalise { NORM_OFF, NORM_FILE, NORM_WAVE };

// Samples to ROM: frames of frameSize reduced to 64 by averaging, normalised, repeated to fill.
inline bool samplesToRom(const std::vector<float>& x, int frameSize, int norm, uint8_t* rom) {
	if (frameSize != 256 && frameSize != 2048) frameSize = 64;
	size_t nFrames = std::min(x.size() / frameSize, (size_t)512);
	if (nFrames == 0) return false;
	int group = frameSize / 64;
	std::vector<double> w(nFrames * 64);
	for (size_t f = 0; f < nFrames; f++)
		for (int s = 0; s < 64; s++) {
			double sum = 0;
			for (int g = 0; g < group; g++) sum += x[f * frameSize + s * group + g];
			w[f * 64 + s] = sum / group;
		}
	if (norm == NORM_FILE) {
		double pk = 0;
		for (double v : w) pk = std::max(pk, std::fabs(v));
		if (pk > 1e-9) for (double& v : w) v /= pk;
	}
	else if (norm == NORM_WAVE) {
		for (size_t f = 0; f < nFrames; f++) {
			double pk = 0;
			for (int s = 0; s < 64; s++) pk = std::max(pk, std::fabs(w[f * 64 + s]));
			if (pk > 1e-9) for (int s = 0; s < 64; s++) w[f * 64 + s] /= pk;
		}
	}
	for (size_t i = 0; i < 32768; i++) {
		double v = w[i % w.size()];
		long b = std::lround((v * 0.5 + 0.5) * 255.0);
		rom[i] = (uint8_t)std::max(0L, std::min(255L, b));
	}
	return true;
}

// Load .bin / .rom (exactly 32768 bytes) or .wav into rom. False keeps rom untouched.
inline bool loadRom(const std::string& path, int frameSize, int norm, uint8_t* rom) {
	std::vector<uint8_t> d;
	if (!readFile(path.c_str(), d)) return false;
	if (d.size() >= 12 && !std::memcmp(d.data(), "RIFF", 4)) {
		std::vector<float> x;
		if (!parseWav(d, x)) return false;
		std::vector<uint8_t> tmp(32768);
		if (!samplesToRom(x, frameSize, norm, tmp.data())) return false;
		std::memcpy(rom, tmp.data(), 32768);
		return true;
	}
	if (d.size() != 32768) return false;
	std::memcpy(rom, d.data(), 32768);
	return true;
}

inline bool saveRom(const std::string& path, const uint8_t* rom) {
	FILE* fp = std::fopen(path.c_str(), "wb");
	if (!fp) return false;
	bool ok = std::fwrite(rom, 1, 32768, fp) == 32768;
	ok = std::fclose(fp) == 0 && ok;
	return ok;
}

// ---- controls to a voice frame (spec 4.1, 6, 7, 9) --------------------------------

struct Mutation {
	int a = 0, b = 0, c = 0;
	bool splice = false; int splicePos = 0, withPos = 0;
	bool stuck = false; int stuckMode = 0, stuckBit = 7;
	int res = 0;       // positions toward 4
	int depth = 0;     // bits dropped
	bool any() const { return a || b || c || splice || stuck || res || depth; }

	// Fresh offsets at depth m (not cumulative). uni() returns 0..1. cBefore = C before mutation (voice 1).
	template <typename U>
	void draw(float m, int cBefore, U uni) {
		auto irand = [&](int n) { return std::min(n - 1, (int)(uni() * n)); };
		int ra = (int)std::lround(4.0 * m);
		a = ra > 0 ? irand(2 * ra + 1) - ra : 0;
		b = uni() < m ? irand(8) : 0;
		c = 0;
		if (cBefore != 7 && uni() < 0.4 * m) {
			for (int tries = 0; tries < 64; tries++) {
				c = irand(8);
				if (((cBefore + c) & 7) != 7) break;
			}
			if (((cBefore + c) & 7) == 7) c = 0;
		}
		splice = uni() < 0.5 * m;
		splicePos = 1 + irand(6);
		withPos = irand(9);
		stuck = uni() < 0.25 * m;
		stuckMode = uni() < 0.5 ? 1 : 2;
		stuckBit = 4 + irand(4);
		res = 0;
		if (uni() < 0.3 * m) { int hi = (int)std::lround(1 + 3.0 * m); res = 1 + irand(hi); }
		depth = 0;
		if (uni() < 0.3 * m) { int hi = (int)std::lround(1 + 4.0 * m); depth = 1 + irand(hi); }
	}
};

struct Controls {
	// knobs
	float freq = 0, fine = 0; bool rangeLo = false;
	float expo = 0, lin = 0, drift = 0.1f;
	bool throughZero = false;
	int A = 0, B = 0, C = 0;
	int splice = 0, with = 0, bit = 7, stuck = 1;   // stuck: 0 LO, 1 OFF, 2 HI (switch order)
	int res = 0, depth = 0;                          // position indices
	// channel-1 bend CVs
	float spliceCv = 0, resCv = 0, depthCv = 0;
	Mutation mut;
};

struct VoiceCv {
	float voct = 0, expoFm = 0, linFm = 0, cvA = 0, cvB = 0, cvC = 0;
};

inline int cvPos(int knob, float cv, int n) {
	int p = knob + (int)std::lround(cv / 10.0 * (n - 1));
	return std::max(0, std::min(n - 1, p));
}
inline int cubeIndex(float cv) { return (int)std::floor(cv / 1.25 + 0.5); }

// Effective digit (0..7) of axis a/b/c for one voice.
inline void effectiveDigits(const Controls& k, const VoiceCv& v, int& a, int& b, int& c) {
	a = ((k.A + cubeIndex(v.cvA) + k.mut.a) % 8 + 8) % 8;
	b = ((k.B + cubeIndex(v.cvB) + k.mut.b) % 8 + 8) % 8;
	int c0 = ((k.C + cubeIndex(v.cvC)) % 8 + 8) % 8;
	c = c0;
	if (c0 != 7 && k.mut.c) {
		c = (c0 + k.mut.c) & 7;
		if (c == 7) c = (c + 1) & 7;     // audio and patterns never cross
	}
}

inline Path makePath(const Controls& k, const VoiceCv& v) {
	Path p;
	int a, b, c;
	effectiveDigits(k, v, a, b, c);
	p.wave = c * 64 + b * 8 + a;
	int sp = k.mut.splice ? k.mut.splicePos : k.splice;
	int wi = k.mut.splice ? k.mut.withPos : k.with;
	sp = cvPos(sp, k.spliceCv, 7);
	p.spliceLen = SPLICE_LEN[sp];
	p.withBit = std::max(0, std::min(8, wi));
	int st = k.mut.stuck ? (k.mut.stuckMode == 1 ? 0 : 2) : k.stuck;
	p.stuck = st == 0 ? 1 : st == 2 ? 2 : 0;
	p.stuckBit = k.mut.stuck ? k.mut.stuckBit : std::max(0, std::min(7, k.bit));
	int r = cvPos(k.res, k.resCv, 5);
	r = std::min(4, r + k.mut.res);
	p.resMask = 63 & ~(64 / RES_STEPS[r] - 1);
	int d = cvPos(k.depth, k.depthCv, 8);      // index 0 = 8 bits .. 7 = 1 bit
	if (k.mut.depth) d = std::min(d + k.mut.depth, std::max(d, 6));
	p.depthBits = 8 - d;
	return p;
}

// Steps per second of the internal clock (before drift).
inline double clockRate(const Controls& k, const VoiceCv& v) {
	double oct = k.freq + k.fine / 12.0 + v.voct + k.expo * v.expoFm;
	oct = std::max(-20.0, std::min(20.0, oct));
	double f = C4_HZ * std::pow(2.0, oct) * (k.rangeLo ? 1.0 / 256.0 : 1.0);
	f *= 1.0 + k.lin * v.linFm / LIN_FM_FULL_VOLTS;
	f = k.throughZero ? std::max(-MAX_CLOCK_HZ, std::min(MAX_CLOCK_HZ, f)) : std::max(0.0, std::min(MAX_CLOCK_HZ, f));
	if (!std::isfinite(f)) f = 0;
	return 64.0 * f;
}

inline double outVolts(double dac, const Path& p) { return (dac + p.dacOffset() - 127.5) / 127.5 * 7.0; }
inline double uniVolts(double dac) { return dac / 48.0; }

} // namespace cubit
