// Sawstack voice engine. No Rack dependency, so it can be tested offline (make test).
// A port of tools/proto/sawstack_ref.py: where the two differ, the Python is the definition.
#pragma once
#include <cmath>
#include <cstdint>
#include <algorithm>

namespace sawstack {

static const double F0 = 261.6256;
static const double RICH_MAX = 8.0;
static const double PI = 3.14159265358979323846;
static const int NOSC = 5;

enum Voicing { OCTAVES = 0, FIFTHS = 1, HARMONICS = 2, NUM_VOICINGS = 3 };

inline const double* voicingRatios(int v) {
	static const double R[3][5] = {
		{-2.0, -1.0, 0.0, 1.0, 2.0},
		{-1.0, 0.5849625007211562 - 1.0, 0.0, 0.5849625007211562, 1.0},
		{-1.5849625007211562, -0.5849625007211562, 0.0, 0.41503749927884376, 0.7369655941662062},
	};
	return R[std::min(std::max(v, 0), 2)];
}
static const double PANS[NOSC] = {-0.45, 0.9, 0.0, -0.9, 0.45};

// ---------------------------------------------------------------- random numbers
inline uint64_t splitmixNext(uint64_t& state, double& u) {
	state += 0x9E3779B97F4A7C15ULL;
	uint64_t z = state;
	z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
	z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
	z = z ^ (z >> 31);
	u = (double)(z >> 11) / 9007199254740992.0;
	return z;
}

struct Rng {  // xoshiro256+ for the noise sources
	uint64_t s[4];
	void seed(uint64_t x) { double u; for (int i = 0; i < 4; i++) s[i] = splitmixNext(x, u); }
	static uint64_t rotl(uint64_t x, int k) { return (x << k) | (x >> (64 - k)); }
	uint64_t next() {
		uint64_t r = s[0] + s[3], t = s[1] << 17;
		s[2] ^= s[0]; s[3] ^= s[1]; s[1] ^= s[2]; s[0] ^= s[3]; s[2] ^= t; s[3] = rotl(s[3], 45);
		return r;
	}
	double uniform() { return (double)(next() >> 11) / 9007199254740992.0; }
	double gauss() {  // Box-Muller
		double u1 = uniform(); if (u1 < 1e-300) u1 = 1e-300;
		return std::sqrt(-2.0 * std::log(u1)) * std::cos(2.0 * PI * uniform());
	}
	double uniformCentred() {  // uniform with unit variance (the jitter filter makes it gaussian enough)
		return ((double)(next() >> 11) / 9007199254740992.0 - 0.5) * 3.4641016151377544;
	}
	double gaussFast() {  // sum of four uniforms, unit variance; for per-sample jitter and hiss
		uint64_t a = next(), b = next();
		double x = (double)(a & 0xFFFFFFFF) + (double)(a >> 32) + (double)(b & 0xFFFFFFFF) + (double)(b >> 32);
		return (x / 4294967296.0 - 2.0) * 1.7320508075688772;
	}
};

// ---------------------------------------------------------------- the unit: fixed component errors at AGE 1
struct Unit {
	double tune[NOSC], vtrack[NOSC], strack[NOSC], shp[NOSC], dc[NOSC], lvl_db[NOSC], lp_hz[NOSC], droop[NOSC], ph0[NOSC];
	static Unit make(uint64_t seed) {
		Unit q;
		double U[NOSC][9];
		uint64_t st = seed;
		for (int i = 0; i < NOSC; i++)
			for (int j = 0; j < 9; j++) splitmixNext(st, U[i][j]);
		for (int i = 0; i < NOSC; i++) {
			auto b = [&](int j, double a) { return (2.0 * U[i][j] - 1.0) * a; };
			q.tune[i] = b(0, 5.0) / 1200.0;
			q.vtrack[i] = b(1, 0.004);
			q.strack[i] = b(2, 0.05);
			q.shp[i] = b(3, 0.10);
			q.dc[i] = b(4, 0.02);
			q.lvl_db[i] = b(5, 1.2);
			q.lp_hz[i] = 6000.0 + 5000.0 * U[i][6];
			q.droop[i] = 5.0 + 15.0 * U[i][7];
			q.ph0[i] = U[i][8];
		}
		return q;
	}
};

// sine for |x| <= 2 (the shaper's whole range): Taylor series to x^13, error < 1e-7
inline double sinShaper(double x) {
	double x2 = x * x;
	return x * (1.0 + x2 * (-1.0 / 6 + x2 * (1.0 / 120 + x2 * (-1.0 / 5040 + x2 * (1.0 / 362880 + x2 * (-1.0 / 39916800 + x2 * (1.0 / 6227020800.0)))))));
}

// 2^x, relative error < 2e-10 (0.0000003 cents)
inline double fastExp2(double x) {
	double fl = std::floor(x), f = x - fl;
	// minimax-quality Taylor in f*ln2 to degree 9 on [0, 1)
	double t = f * 0.6931471805599453;
	double p = 1.0 + t * (1.0 + t * (0.5 + t * (1.0 / 6 + t * (1.0 / 24 + t * (1.0 / 120 + t * (1.0 / 720 + t * (1.0 / 5040 + t * (1.0 / 40320 + t * (1.0 / 362880)))))))));
	int e = (int)fl;
	if (e < -1000) return 0.0;
	if (e > 1000) e = 1000;
	union { uint64_t u; double d; } b; b.u = (uint64_t)(e + 1023) << 52;
	return p * b.d;
}

inline double wrap1(double x) { return x - 2.0 * std::floor((x + 1.0) * 0.5); }  // into [-1, 1)

// fast tanh: Pade approximant on a clamped range, error < 2e-4 over all inputs
inline double fastTanh(double x) {
	if (x > 4.97) return 1.0;
	if (x < -4.97) return -1.0;
	double x2 = x * x;
	double r = x * (135135.0 + x2 * (17325.0 + x2 * (378.0 + x2))) / (135135.0 + x2 * (62370.0 + x2 * (3150.0 + x2 * 28.0)));
	return std::max(-1.0, std::min(1.0, r));
}

// ---------------------------------------------------------------- halfband decimator (2:1)
struct Decimator2 {
	static const int N = 47;           // odd length, centre tap at 23
	double h[N];
	double buf[2 * N] = {};
	int pos = 0;
	Decimator2() {
		const int M = N - 1;
		double sum = 0;
		for (int n = 0; n < N; n++) {
			double t = n - M / 2.0;
			double s = t == 0 ? 0.5 : std::sin(PI * 0.5 * t) / (PI * t);
			double w = 0.42 - 0.5 * std::cos(2 * PI * n / M) + 0.08 * std::cos(4 * PI * n / M);  // Blackman
			h[n] = s * w; sum += h[n];
		}
		for (int n = 0; n < N; n++) h[n] /= sum;
		for (int n = 0; n < N; n++) if ((n - M / 2) % 2 == 0 && n != M / 2) h[n] = 0.0;   // exact halfband zeros
	}
	void reset() { std::fill(buf, buf + 2 * N, 0.0); pos = 0; }
	double process(double a, double b) {  // two input samples (a first), one output sample
		push(a); push(b);
		double y = 0;
		const double* x = buf + pos;   // x[0] oldest .. x[N-1] newest
		const int c = N / 2;                       // symmetric filter: pair taps, skip the halfband zeros
		y = h[c] * x[c];
		for (int d = 1; d <= c; d += 2) y += h[c - d] * (x[c - d] + x[c + d]);
		return y;
	}
	void push(double v) {
		buf[pos] = v; buf[pos + N] = v;
		pos = (pos + 1) % N;
	}
};

// ---------------------------------------------------------------- one voice: five oscillators, VCA and output stage
struct Voice {
	// settings
	double engineRate = 48000.0, fs = 96000.0;
	double age = -1.0, drive = -1.0;
	uint64_t seed = 1;
	double kdrift = 1.0, kjit = 1.0;     // test hooks: 0 removes drift / jitter
	int soloMask = 0x1F;                 // test hook: which oscillators reach the mix

	// unit, scaled by age
	Unit unit;
	double tune[NOSC], vtrack[NOSC], strack[NOSC], shp[NOSC], dc[NOSC], lvl[NOSC], lpc[NOSC], droop[NOSC], rsnorm[NOSC];
	bool overrideUnit = false;           // test hook: keep the arrays above as set by hand

	// derived
	double invFs = 1.0 / 96000.0, jscale = 0, a1, a2, a3, ac, ja, jgain, sdecay, couple, od, ob, onorm, rnorm, tob, quiet, og, obias, otb, orn;

	// state
	double ph[NOSC], pend[NOSC], prev[NOSC], z[NOSC], spike[NOSC], jz[NOSC], d1[NOSC], d2[NOSC], d3[NOSC], drift[NOSC];
	double alpha[NOSC];
	double dcm = 0, rail = 0;
	int counter = 0;
	Rng rng, hissRng;
	double oscOut[NOSC] = {};            // last oscillator outputs at fs (for tests)

	void init(double sampleRate, uint64_t unitSeed, double age_, double drive_) {
		engineRate = sampleRate; fs = 2.0 * sampleRate; seed = unitSeed;
		unit = Unit::make(seed);
		rng.seed(seed ^ 0xD1B54A32D192ED03ULL);
		hissRng.seed(seed ^ 0x8CB92BA72F3D8DD7ULL);
		age = -1; drive = -1;
		setAgeDrive(age_, drive_);
		reset();
	}
	void reset() {
		for (int i = 0; i < NOSC; i++) {
			ph[i] = unit.ph0[i]; pend[i] = prev[i] = z[i] = spike[i] = jz[i] = 0; drift[i] = 0; alpha[i] = -1;
			d1[i] = rng.gauss(); d2[i] = rng.gauss(); d3[i] = rng.gauss();
		}
		dcm = rng.gauss(); rail = 0; counter = 0;
		resetFilter();
	}
	void setAgeDrive(double age_, double drive_) {
		if (age_ == age && drive_ == drive) return;
		age = age_; drive = drive_;
		if (!overrideUnit) {
			for (int i = 0; i < NOSC; i++) {
				tune[i] = age * unit.tune[i];
				vtrack[i] = 1.0 + age * unit.vtrack[i];
				strack[i] = 1.0 + age * unit.strack[i];
				shp[i] = 1.0 + age * unit.shp[i];
				dc[i] = age * unit.dc[i];
				lvl[i] = std::pow(10.0, age * unit.lvl_db[i] / 20.0);
				lpc[i] = std::exp(-2 * PI * (20000.0 - age * unit.lp_hz[i]) / fs);
				droop[i] = age * unit.droop[i];
			}
		}
		for (int i = 0; i < NOSC; i++) rsnorm[i] = 1.0 / std::sin(0.5 * PI * std::min(shp[i], 1.0));
		const double dt = 32.0 / fs;
		a1 = std::exp(-dt / 0.3); a2 = std::exp(-dt / 2.5); a3 = std::exp(-dt / 25.0); ac = std::exp(-dt / 20.0);
		ja = std::exp(-2 * PI * 400.0 / engineRate);
		jgain = age * 0.001156 / std::sqrt((1 - ja) / (1 + ja));
		jscale = (1 - ja) * kjit * jgain;
		invFs = 1.0 / fs;
		sdecay = std::exp(-1.0 / (160e-6 * fs));
		couple = age * 0.001;
		od = 0.35 + 1.4 * drive; ob = 0.12 * age + 0.1 * drive; onorm = std::tanh(od); rnorm = 1.0 / onorm; tob = fastTanh(od * ob);
		og = 0.30 * (1.0 + 2.5 * drive); obias = 0.10 * age + 0.12 * drive; otb = fastTanh(obias); orn = 1.0 / (0.75 + 1.35 * drive);
		quiet = std::max(0.0, age - 0.6) / 0.4;
	}

	inline double sine(double p, int i) const {
		double tri = 4.0 * std::fabs(p - 0.5) - 1.0;
		return -sinShaper(0.5 * PI * tri * shp[i]) * rsnorm[i];
	}
	inline double shape(double sn, double saw, double t, int i) const {
		double y = (1.0 - t) * sn + t * 0.8 * saw + dc[i];
		return lvl[i] * (fastTanh(od * (y + ob)) - tob) * rnorm;
	}

	double fbase[NOSC];
	// once per engine sample (= two oscillator samples): slow drift and the base frequencies
	void prepare(double pitch, double spread, const double* ratios) {
		if (counter % 32 == 0) {
			dcm = ac * dcm + std::sqrt(1 - ac * ac) * rng.gauss();
			for (int i = 0; i < NOSC; i++) {
				d1[i] = a1 * d1[i] + std::sqrt(1 - a1 * a1) * rng.gauss();
				d2[i] = a2 * d2[i] + std::sqrt(1 - a2 * a2) * rng.gauss();
				d3[i] = a3 * d3[i] + std::sqrt(1 - a3 * a3) * rng.gauss();
				drift[i] = kdrift * age * (1.7 * d1[i] + 5.0 * d2[i] + 5.0 * d3[i] + 3.0 * dcm) / 1200.0;
			}
		}
		const double sa = spread * spread * spread;
		for (int i = 0; i < NOSC; i++) {
			double octv = pitch * vtrack[i] + ratios[i] * sa * strack[i] + tune[i] + drift[i];
			double f = F0 * fastExp2(octv);
			f = droop[i] > 0 ? f * fastExp2(-droop[i] * f * (1.0 / 4800000.0)) : f;
			jz[i] = ja * jz[i] + jscale * rng.uniformCentred();      // jitter, run at the engine rate
			fbase[i] = f * (1.0 + jz[i]);
		}
	}
	// one sample at fs. Writes the five oscillator outputs to o[].
	void oscSub(double timbre, double rich, double* o) {
		counter++;
		const double t = timbre;
		const double k0 = 1.0 + (RICH_MAX - 1.0) * rich * rich;
		const double w0 = 40e-6 * fs;
		for (int i = 0; i < NOSC; i++) {
			double f = fbase[i];
			if (f > 0.225 * fs) f = 0.225 * fs;
			if (f < 0.01) f = 0.01;
			const double dp = f * invFs;
			double k = k0;
			if (k * dp > 0.45) k = std::max(1.0, 0.45 / dp);
			const double p0 = ph[i];
			double p1 = p0 + dp;
			double ycorr = 0.0;
			const double thr = 1.0 - couple * (rail - spike[i]);
			// in-cycle wraps of the multiplied saw
			{
				double x0 = k * (2 * p0 - 1), x1 = k * (2 * std::min(p1, thr) - 1);
				double xc = 2.0 * std::ceil((x0 - 1.0) * 0.5) + 1.0;
				while (xc < x1 && xc < k - 1e-9) {
					if (xc > x0) {
						double pc = (xc / k + 1.0) * 0.5;
						double a = (p1 - pc) / dp;
						double sn = sine(pc, i);
						double h = shape(sn, -1.0, t, i) - shape(sn, 1.0, t, i);
						ycorr += h * (-0.5 * (1 - a) * (1 - a)); pend[i] += h * 0.5 * a * a;
					}
					xc += 2.0;
				}
			}
			alpha[i] = -1.0;
			if (p1 >= thr) {
				double a = (p1 - thr) / dp;
				if (a > 1.0) a = 1.0;
				alpha[i] = a;
				double pb = std::min(thr, 1.0 - 1e-12);
				double h = shape(sine(0.0, i), wrap1(-k), t, i) - shape(sine(pb, i), wrap1(k * (2 * pb - 1)), t, i);
				ycorr += h * (-0.5 * (1 - a) * (1 - a)); pend[i] += h * 0.5 * a * a;
				p1 = a * dp;
				double x0 = -k, x1 = k * (2 * p1 - 1);
				double xc = 2.0 * std::ceil((x0 - 1.0) * 0.5) + 1.0;
				while (xc < x1) {
					if (xc > x0) {
						double pc = (xc / k + 1.0) * 0.5;
						double a2_ = (p1 - pc) / dp;
						double sn = sine(pc, i);
						double hh = shape(sn, -1.0, t, i) - shape(sn, 1.0, t, i);
						ycorr += hh * (-0.5 * (1 - a2_) * (1 - a2_)); pend[i] += hh * 0.5 * a2_ * a2_;
					}
					xc += 2.0;
				}
			}
			ph[i] = p1;
			double sn = sine(p1, i);
			double saw = wrap1(k * (2 * p1 - 1));
			double w = std::min(0.1, dp * w0);
			if (std::fabs(p1 - 0.5) < 4.0 * w) {
				double g = (p1 - 0.5) / w;
				double gl = age * 0.06 * std::exp(-g * g);
				saw -= gl; sn -= 0.5 * gl;
			}
			double y = shape(sn, saw, t, i) + ycorr;
			double done = prev[i] + pend[i];
			prev[i] = y; pend[i] = 0.0;
			z[i] = lpc[i] * z[i] + (1 - lpc[i]) * done;
			o[i] = z[i];
		}
		rail = 0.0;
		for (int i = 0; i < NOSC; i++) {
			spike[i] *= sdecay;
			if (alpha[i] >= 0.0) spike[i] += std::pow(sdecay, alpha[i]);
			rail += spike[i];
		}
	}

	inline double outStage(double mix, double a) {
		double x = (fastTanh(mix * a * og + obias) - otb) * orn;
		if (quiet > 0) x += quiet * 2.2e-4 * 1.4142135623730951 * hissRng.gaussFast();
		return x;
	}

	// One engine-rate sample, returned as two samples at fs (before decimation). Controls are 0..1 except
	// pitch (octaves from C4). In mono, l2 carries the mono mix and r2 is left alone.
	void processOS(double pitch, double spread, double timbre, double rich, double amp, int voicing, bool mono,
	               double* l2, double* r2) {
		const double* ratios = voicingRatios(voicing);
		spread = std::min(std::max(spread, 0.0), 1.0);
		timbre = std::min(std::max(timbre, 0.0), 1.0);
		rich = std::min(std::max(rich, 0.0), 1.0);
		double a = std::min(std::max(amp, 0.0), 1.0) + quiet * 0.0008;
		prepare(pitch, spread, ratios);
		static const double GL[NOSC] = {0.9081431738250813, 0.078459095727845, 0.7071067811865476, 0.996917333733128, 0.41865973753742813};
		static const double GR[NOSC] = {0.41865973753742813, 0.996917333733128, 0.7071067811865475, 0.07845909572784493, 0.9081431738250813};
		for (int s = 0; s < 2; s++) {
			oscSub(timbre, rich, oscOut);
			if (mono) {
				double m = 0;
				for (int i = 0; i < NOSC; i++) if (soloMask & (1 << i)) m += oscOut[i];
				l2[s] += outStage(m * 0.7071067811865476, a);
			} else {
				double ml = 0, mr = 0;
				for (int i = 0; i < NOSC; i++) {
					if (!(soloMask & (1 << i))) continue;
					ml += GL[i] * oscOut[i]; mr += GR[i] * oscOut[i];
				}
				l2[s] += outStage(ml, a); r2[s] += outStage(mr, a);
			}
		}
	}

	// Convenience for one voice on its own: process + decimate + DC block. 1.0 = 5 V.
	struct OutFilter* filt = nullptr;
	void resetFilter();
	void process(double pitch, double spread, double timbre, double rich, double amp, int voicing, bool mono,
	             double& outL, double& outR);
};

// Decimates the summed 2x signal of any number of voices and removes DC. Linear, so it runs once per module.
struct OutFilter {
	Decimator2 decL, decR;
	double xL = 0, yL = 0, xR = 0, yR = 0, c = 0;
	void init(double engineRate) { decL.reset(); decR.reset(); xL = yL = xR = yR = 0; c = std::exp(-2 * PI * 8.0 / engineRate); }
	void process(const double* l2, const double* r2, bool mono, double& outL, double& outR) {
		double a = decL.process(l2[0], l2[1]);
		double o = a - xL + c * yL; xL = a; yL = o;
		if (mono) { outL = outR = o; return; }
		double b = decR.process(r2[0], r2[1]);
		double p = b - xR + c * yR; xR = b; yR = p;
		outL = o; outR = p;
	}
};

inline void Voice::resetFilter() { if (filt) filt->init(engineRate); }

inline void Voice::process(double pitch, double spread, double timbre, double rich, double amp, int voicing, bool mono,
                           double& outL, double& outR) {
	if (!filt) { filt = new OutFilter(); filt->init(engineRate); }
	double l2[2] = {0, 0}, r2[2] = {0, 0};
	processOS(pitch, spread, timbre, rich, amp, voicing, mono, l2, r2);
	filt->process(l2, r2, mono, outL, outR);
}

// ---------------------------------------------------------------- rise/fall envelope
struct Envelope {
	double v = 0, sc = 1;
	int stage = 0;            // 0 idle, 1 rise, 2 hold, 3 fall
	bool high = false;
	int endHold = 0;
	Rng rng;
	void init(uint64_t seed) { rng.seed(seed ^ 0x2545F4914F6CDD1DULL); v = 0; stage = 0; high = false; endHold = 0; }
	// gate in volts, times in seconds. Returns level 0..1; endPulse true while the 1 ms end pulse is high.
	double process(double gate, double rise, double fall, double age, double sr, bool& endPulse) {
		if (!high && gate >= 1.0) { high = true; stage = 1; sc = 1.0 + age * 0.06 * (2 * rng.uniform() - 1); }
		else if (high && gate < 0.5) high = false;
		double c = 0.6 * age, nr = 1.0, nf = 1.0;
		if (c > 1e-4) {
			nr = std::log((1 + 0.8 * c) / (1 - 0.5 * c)) / (1.3 * c);
			nf = std::log((1 + 1.2 * c) / (1 - c)) / (2.2 * c);
		}
		if (stage == 1) {
			v += (1.0 + c * (1.3 * (1 - v) - 0.5)) * nr / (rise * sc * sr);
			if (v >= 1.0) { v = 1.0; stage = 2; }
		}
		else if (stage == 2) {
			if (!high) stage = 3;
		}
		else if (stage == 3) {
			v -= (1.0 + c * (2.2 * v - 1.0)) * nf / (fall * sc * sr);
			if (v <= 0.0) { v = 0.0; stage = 0; endHold = (int)(0.001 * sr); }
		}
		endPulse = endHold > 0;
		if (endHold > 0) endHold--;
		return v;
	}
};

}  // namespace sawstack
