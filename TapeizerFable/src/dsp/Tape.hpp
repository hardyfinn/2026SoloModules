// Tapeizer tape engine: one deck pass on up to MAXT tracks that share one tape and one transport.
// A port of tools/proto/tape_ref.py (the definition of the sound). No Rack dependency.
//
// Units: 1.0 = 0 VU = reference flux. Lengths in micrometres, tape speed in cm/s.
// Per track: REC gain -> NR encode -> record EQ (FIR) -> x2 up -> tape curve, modulation noise,
// transport delay -> x2 down -> playback loss (FIR) -> drop-outs -> level wobble -> head bump, low cut ->
// crosstalk -> hiss and hum -> CAL -> NR decode -> / REC gain
#pragma once
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>
#include <algorithm>

namespace tapeizer {

static const double PI = 3.14159265358979323846;
static const int MAXT = 10;   // tracks on one tape (8 TRACKS + IN L + IN R)

inline double db(double d) { return std::pow(10.0, d / 20.0); }
inline double clampd(double x, double a, double b) { return x < a ? a : (x > b ? b : x); }
inline double kn(double k) { return (2.0 * k) * (2.0 * k); }   // knob law: 0.5 -> x1, 1 -> x4, 0 -> off
inline double sincn(double x) { if (std::fabs(x) < 1e-12) return 1.0; double a = PI * x; return std::sin(a) / a; }

// ------------------------------------------------------------------ random numbers
struct SplitMix {   // identical to splitmix() in the reference
	uint64_t s;
	explicit SplitMix(uint64_t seed) : s(seed) {}
	double next() {
		s += 0x9E3779B97F4A7C15ULL;
		uint64_t z = s;
		z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
		z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
		z ^= z >> 31;
		return (double)(z >> 11) / 9007199254740992.0;
	}
};

struct Rng {   // fast noise: xorshift64*, near-gaussian from four uniforms
	uint64_t s = 0x2545F4914F6CDD1DULL;
	void seed(uint64_t v) { s = v * 0x9E3779B97F4A7C15ULL + 0x632BE59BD9B4E019ULL; if (!s) s = 1; for (int i = 0; i < 4; i++) next64(); }
	uint64_t next64() { s ^= s >> 12; s ^= s << 25; s ^= s >> 27; return s * 0x2545F4914F6CDD1DULL; }
	double uni() { return (double)(next64() >> 11) / 9007199254740992.0; }
	double gauss() { return (uni() + uni() + uni() + uni() - 2.0) * 1.7320508075688772; }
	double expo(double mean) { return -mean * std::log(1.0 - uni()); }
};

// ------------------------------------------------------------------ the unit: fixed errors of one machine
struct Unit { double sens[MAXT], cal[MAXT], az, bias; };
inline Unit makeUnit(uint64_t seed) {   // draw order as make_unit(seed, C=10) in the reference
	SplitMix g(seed);
	Unit u;
	for (int c = 0; c < MAXT; c++) { u.sens[c] = 2 * g.next() - 1; u.cal[c] = 2 * g.next() - 1; }
	u.az = (2 * g.next() - 1) >= 0 ? 1.0 : -1.0;
	u.bias = 2 * g.next() - 1;
	return u;
}

// ------------------------------------------------------------------ filters
struct Biquad {
	double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
	inline double process(double x) { double y = b0 * x + z1; z1 = b1 * x - a1 * y + z2; z2 = b2 * x - a2 * y; return y; }
	void reset() { z1 = z2 = 0; }
	void set(double B0, double B1, double B2, double A0, double A1, double A2) { b0 = B0 / A0; b1 = B1 / A0; b2 = B2 / A0; a1 = A1 / A0; a2 = A2 / A0; }
	// RBJ / bilinear Butterworth (prewarped, same as scipy.signal.butter)
	void lp2(double fc, double fs, double Q = 0.7071067811865476) {
		double w = 2 * PI * fc / fs, cs = std::cos(w), al = std::sin(w) / (2 * Q);
		set((1 - cs) / 2, 1 - cs, (1 - cs) / 2, 1 + al, -2 * cs, 1 - al);
	}
	void hp2(double fc, double fs, double Q = 0.7071067811865476) {
		double w = 2 * PI * fc / fs, cs = std::cos(w), al = std::sin(w) / (2 * Q);
		set((1 + cs) / 2, -(1 + cs), (1 + cs) / 2, 1 + al, -2 * cs, 1 - al);
	}
	void lp1(double fc, double fs) {   // butter(1)
		double wc = 2 * fs * std::tan(PI * fc / fs);
		set(wc, wc, 0, 2 * fs + wc, wc - 2 * fs, 0);
	}
	void shelf1(double fz, double fp, double fs) {   // first-order shelf: zero at fz, pole at fp (shelf1 in the reference)
		double wz = 2 * fs * std::tan(PI * fz / fs), wp = 2 * fs * std::tan(PI * fp / fs);
		set((2 * fs + wz) / wz, (wz - 2 * fs) / wz, 0, (2 * fs + wp) / wp, (wp - 2 * fs) / wp, 0);
	}
	void peak(double f0, double gainDb, double Q, double fs) {
		double A = std::pow(10.0, gainDb / 40), w = 2 * PI * f0 / fs, al = std::sin(w) / (2 * Q);
		set(1 + al * A, -2 * std::cos(w), 1 - al * A, 1 + al / A, -2 * std::cos(w), 1 - al / A);
	}
	void shelf(double f0, double gainDb, double fs, bool high) {   // RBJ shelf, slope 1
		double A = std::pow(10.0, gainDb / 40), w = 2 * PI * f0 / fs, cs = std::cos(w), al = std::sin(w) / 2 * std::sqrt(2.0), s = 2 * std::sqrt(A) * al;
		if (high) set(A * ((A + 1) + (A - 1) * cs + s), -2 * A * ((A - 1) + (A + 1) * cs), A * ((A + 1) + (A - 1) * cs - s),
		              (A + 1) - (A - 1) * cs + s, 2 * ((A - 1) - (A + 1) * cs), (A + 1) - (A - 1) * cs - s);
		else set(A * ((A + 1) - (A - 1) * cs + s), 2 * A * ((A - 1) - (A + 1) * cs), A * ((A + 1) - (A - 1) * cs - s),
		         (A + 1) + (A - 1) * cs + s, -2 * ((A - 1) + (A + 1) * cs), (A + 1) + (A - 1) * cs - s);
	}
	// sum of the squared impulse response (noise gain), closed form for one biquad
	double noiseGain() const {
		double r0 = (1 + a2) / ((1 - a2) * ((1 + a2) * (1 + a2) - a1 * a1));
		double r1 = -a1 * r0 / (1 + a2), r2 = -a1 * r1 - a2 * r0;
		return (b0 * b0 + b1 * b1 + b2 * b2) * r0 + 2 * (b0 * b1 + b1 * b2) * r1 + 2 * b0 * b2 * r2;
	}
};

// Unit-RMS filtered noise. Kinds: low-pass (one 2-pole Butterworth) or band-pass (2-pole high-pass then 2-pole low-pass).
struct NoiseSrc {
	Biquad f1, f2; bool band = false; double norm = 1;
	void lowpass(double fc, double fs) { band = false; f1.lp2(std::min(fc, 0.45 * fs), fs); f1.reset(); norm = 1.0 / std::sqrt(f1.noiseGain()); }
	void bandpass(double lo, double hi, double fs) {
		band = true; f1.hp2(lo, fs); f2.lp2(std::min(hi, 0.45 * fs), fs);
		// numeric noise gain of the cascade
		Biquad a = f1, b = f2; a.reset(); b.reset();
		double s = 0, x = 1; long n = 0, lim = (long)(40.0 * fs / lo) + 1000;
		for (; n < lim; n++) { double y = b.process(a.process(x)); x = 0; s += y * y; }
		norm = 1.0 / std::sqrt(s);
		f1.reset(); f2.reset();
	}
	inline double next(Rng& r) { double y = f1.process(r.gauss()); if (band) y = f2.process(y); return y * norm; }
	void warm(Rng& r, int n) { for (int i = 0; i < n; i++) next(r); }
};

// ------------------------------------------------------------------ linear-phase FIR from a magnitude curve
// fir_design() in the reference: 513 points from 0 to fs/2, inverse real DFT of length 1024, Hann window.
struct FirDesigner {
	int N = 63, half = 31; double fs = 48000;
	std::vector<double> cosT;   // [n][k] = cos(2 pi k n / 1024), n = 0..half, k = 1..511
	std::vector<double> Hk; std::vector<double> win;
	void init(double sampleRate) {
		fs = sampleRate; N = fs <= 50000 ? 63 : 127; half = N / 2;
		cosT.assign((half + 1) * 512, 0.0); Hk.assign(513, 0.0); win.assign(half + 1, 0.0);
		for (int n = 0; n <= half; n++) {
			for (int k = 0; k < 512; k++) cosT[n * 512 + k] = std::cos(2 * PI * k * n / 1024.0);
			win[n] = 0.5 + 0.5 * std::cos(PI * n / (half + 1));
		}
	}
	template <typename F> void design(F H, double* h) {
		for (int k = 0; k <= 512; k++) Hk[k] = H(k * fs / 1024.0);
		for (int n = 0; n <= half; n++) {
			double s = Hk[0] + Hk[512] * ((n & 1) ? -1.0 : 1.0);
			const double* c = &cosT[n * 512];
			for (int k = 1; k < 512; k++) s += 2.0 * Hk[k] * c[k];
			double v = s / 1024.0 * win[n];
			h[half + n] = v; h[half - n] = v;
		}
	}
};

struct FirLine {   // one track's delay line; double-length buffer so the convolution reads straight
	double buf[2 * 128] = {}; int pos = 0;
	void reset() { std::memset(buf, 0, sizeof buf); pos = 0; }
	inline void push(double x, int N) { pos = (pos == 0 ? N : pos) - 1; buf[pos] = x; buf[pos + N] = x; }
	inline double conv(const double* h, int N) const { const double* b = buf + pos; double s = 0; for (int i = 0; i < N; i++) s += h[i] * b[i]; return s; }
};

// ------------------------------------------------------------------ x2 oversampling (63-tap Kaiser half-band)
struct HalfBand {
	static const int NH = 63, M = 31;
	double h[NH];
	HalfBand() {
		auto I0 = [](double x) { double s = 1, t = 1; for (int k = 1; k < 40; k++) { t *= (x / (2 * k)) * (x / (2 * k)); s += t; } return s; };
		const double beta = 7.0; double sum = 0;
		for (int n = 0; n < NH; n++) {
			double r = (n - M) / (double)M;
			h[n] = 0.5 * sincn(0.5 * (n - M)) * I0(beta * std::sqrt(std::max(0.0, 1 - r * r))) / I0(beta);
			sum += h[n];
		}
		for (int n = 0; n < NH; n++) h[n] /= sum;
	}
	static const HalfBand& get() { static HalfBand hb; return hb; }
};
struct Up2 {   // one input -> two outputs at 2 fs. Latency M samples at 2 fs.
	double x[64] = {}; int pos = 0;   // last 32 inputs, double buffer
	void reset() { std::memset(x, 0, sizeof x); pos = 0; }
	inline void process(double in, double& y0, double& y1) {
		const HalfBand& hb = HalfBand::get();
		pos = (pos == 0 ? 32 : pos) - 1; x[pos] = in; x[pos + 32] = in;
		const double* b = x + pos;   // b[i] = input i steps ago
		double s = 0;
		for (int i = 0; i < 32; i++) s += hb.h[2 * i] * b[i];   // even taps on the stuffed sequence
		y0 = 2 * s;
		y1 = 2 * hb.h[HalfBand::M] * b[15];                       // centre tap: input (M - 1) / 2 = 15 steps ago
	}
};
struct Down2 {   // two inputs at 2 fs -> one output. Latency M samples at 2 fs.
	double v[128] = {}; int pos = 0;
	void reset() { std::memset(v, 0, sizeof v); pos = 0; }
	inline void push(double a) { pos = (pos == 0 ? 64 : pos) - 1; v[pos] = a; v[pos + 64] = a; }
	inline double process(double a0, double a1) {
		push(a0); push(a1);
		const HalfBand& hb = HalfBand::get(); const double* b = v + pos;   // b[j] = sample j steps ago at 2 fs
		double s = hb.h[HalfBand::M] * b[HalfBand::M];
		for (int j = 0; j < HalfBand::NH; j += 2) s += hb.h[j] * b[j];
		return s;
	}
};

// ------------------------------------------------------------------ noise reduction
enum NrType { NR_OFF = 0, NR_B, NR_C, NR_X2, NUM_NR };

struct X2Core {   // _x2 in the reference: 2:1 compander, detector always reads the tape-side signal
	double s = 1e-3, hp = 0, xp = 0, lp = 0, sh = 0, shp = 0;
	double ca = 0, cr = 0, ah = 0, al = 0, asf = 0;
	void init(double fs) {
		ca = 1 - std::exp(-1 / (0.005 * fs)); cr = 1 - std::exp(-1 / (0.030 * fs));
		ah = 1 / (1 + 2 * PI * 40 / fs); al = 1 - std::exp(-2 * PI * 11000 / fs); asf = 1 / (1 + 2 * PI * 1600 / fs);
		s = 1e-3; hp = xp = lp = sh = shp = 0;
	}
	inline double process(double x, bool encode) {
		double r = std::sqrt(s); if (r < 3e-4) r = 3e-4;
		double o, d;
		if (encode) { double g = 1.0 / r; if (g > 30.0) g = 30.0; o = x * g; d = o; }
		else { d = x; o = x * r; }
		hp = ah * (hp + d - xp); xp = d;
		lp += al * (hp - lp);
		sh = asf * (sh + lp - shp); shp = lp;
		double w = lp + 6.0 * sh, w2 = w * w;
		s += (w2 > s ? ca : cr) * (w2 - s);
		return o;
	}
};

struct Slide {   // _slide in the reference: one sliding-band stage
	double G = 2.16, f0 = 1800, thr = 0.03, fs = 48000, hp = 0, inp = 0, env = 0, att = 0, rel = 0;
	void init(double sampleRate, double g, double f, double t) {
		fs = sampleRate; G = g; f0 = f; thr = t; hp = inp = env = 0;
		att = 1 - std::exp(-1 / (0.002 * fs)); rel = 1 - std::exp(-1 / (0.06 * fs));
	}
	inline double process(double x, bool encode) {
		double q = env / thr, fc = f0 * (1 + q * q); if (fc > 60000.0) fc = 60000.0;
		double a = 1 / (1 + 2 * PI * fc / fs), c = a * (hp - inp), o, u, h;
		if (encode) { u = x; h = a * u + c; o = u + G * h; }
		else { o = (x - G * c) / (1 + G * a); u = o; h = a * u + c; }
		hp = h; inp = u;
		double m = std::fabs(G * h);
		env += (m > env ? att : rel) * (m - env);
		return o;
	}
};

struct NoiseReduction {   // one direction (encode or decode) of one type, for one track
	int type = NR_OFF; bool encode = true;
	Biquad shelf; X2Core x2; Slide s1, s2;
	void init(int t, bool enc, double fs) {
		type = t; encode = enc; shelf.reset();
		if (t == NR_X2) { if (enc) shelf.shelf1(400, 1600, fs); else shelf.shelf1(1600, 400, fs); x2.init(fs); }
		else if (t == NR_B) s1.init(fs, 2.16, 1800.0, 0.03);
		else if (t == NR_C) {
			if (enc) { shelf.shelf1(6300, 4000, fs); s1.init(fs, 2.16, 450.0, 0.03); s2.init(fs, 2.16, 450.0, 0.003); }
			else { shelf.shelf1(4000, 6300, fs); s1.init(fs, 2.16, 450.0, 0.003); s2.init(fs, 2.16, 450.0, 0.03); }
		}
	}
	inline double process(double x) {
		switch (type) {
			case NR_X2: return encode ? x2.process(shelf.process(x), true) : shelf.process(x2.process(x, false));
			case NR_B: return s1.process(x, encode);
			case NR_C: return encode ? s2.process(s1.process(shelf.process(x), true), true) : shelf.process(s2.process(s1.process(x, false), false));
			default: return x;
		}
	}
};

// ------------------------------------------------------------------ machines and tapes
struct MachineDef { const char* name; double v, d0, gap, w, pitch, cap, hiss, wow, dc, dp, bump, hp, xt; bool reel; };
enum Machine { M_4TR = 0, M_4TR_HI, M_4TR_C, M_8TR_CASS, M_8TR_75, M_8TR_15, M_DECK, M_BOOMBOX, M_QUARTER, NUM_MACHINES };
static const MachineDef MACHINES[NUM_MACHINES] = {
	{"4TR",      4.76,  0.20, 1.2, 0.60, 0.90, 12, -50, 0.07, 2.5, 11, 80,  35, -45, false},
	{"4TR HI",   9.53,  0.20, 1.2, 0.60, 0.90, 12, -52, 0.05, 2.5, 11, 110, 35, -50, false},
	{"4TR C",    9.53,  0.20, 1.0, 0.60, 0.90, 16, -52, 0.05, 2.5, 11, 110, 35, -50, false},
	{"8TR CASS", 9.53,  0.20, 1.0, 0.28, 0.45, 13, -49, 0.04, 2.5, 11, 110, 35, -44, false},
	{"8TR 7.5",  19.05, 0.50, 3.0, 0.45, 0.80, 12, -55, 0.05, 6.0, 40, 50,  28, -48, true},
	{"8TR 15",   38.1,  0.50, 3.0, 0.45, 0.80, 12, -57, 0.06, 6.0, 40, 65,  28, -45, true},
	{"DECK",     4.76,  0.20, 1.2, 0.60, 0.90, 17, -51, 0.08, 2.5, 11, 70,  25, -40, false},
	{"BOOMBOX",  4.76,  0.45, 2.0, 0.60, 0.90, 9,  -43, 0.22, 2.0, 10, 110, 70, -30, false},
	{"1/4 INCH", 19.05, 0.50, 3.0, 2.00, 4.00, 12, -61, 0.05, 6.0, 45, 45,  20, -55, true},
};
struct TapeDef { double mol, delta, hiss; };
static const TapeDef TAPES_CASS[3] = {{2.5, 2.0, 4.0}, {3.0, 1.5, 0.0}, {7.5, 1.1, 0.0}};
static const TapeDef TAPES_REEL[3] = {{6.0, 6.0, 1.0}, {8.0, 6.0, 0.0}, {10.0, 6.0, -1.0}};

// Playback wavelength loss, linear gain (loss() in the reference). v cm/s; delta, d, gap, wtan in um.
inline double tapeLoss(double f, double v, double delta, double d, double gap, double wtan) {
	double lam = v * 1e4 / std::max(f, 1.0), k = 2 * PI / lam;
	double thick = (1 - std::exp(-k * delta)) / (k * delta);
	return thick * std::min(std::exp(-k * d), db(4)) * std::fabs(sincn(gap / lam)) * std::fabs(sincn(wtan / lam));
}

// ------------------------------------------------------------------ one deck pass
struct DeckSettings {
	// structural: a change needs reset()
	int machine = M_4TR, tape = 1, nrEnc = NR_OFF, nrDec = NR_OFF;
	bool quiet = false;
	// continuous
	double recDb = 0, calDb = 0, bias = 0, age = 0.25, wear = 0, wow = 0.5, flutter = 0.5, azimuth = 0, dropout = 0.5, hiss = 0.5, strength = 1, humHz = 50;
};

struct Deck {
	double fs = 48000, fs2 = 96000; int C = 1;
	uint64_t unitSeed = 1, noiseSeed = 1;
	Unit U;
	DeckSettings st;
	const MachineDef* m = &MACHINES[0]; const TapeDef* tp = &TAPES_CASS[1];

	// derived (update())
	double S = 1, g = 1, MsEff = 1, dcOff = 0, hMem = 0, rPlay = 0, t0 = 0, mn = 0, a = 0, sig = 0, fcap = 0, fpin = 0;
	double baseTarget = 0, base = 0, baseCoef = 0, skew0 = 0, az = 0, levAmp = 0, xt = 0, hissLev = 0, humLev = 0;
	double ageC = 0, kw = 0, kf = 0, rate = 0, meanDepth = 0, contact = 0, aMax = 1;
	double sensG[MAXT], calG[MAXT], pos[MAXT];
	double peakThr = 1;

	// FIRs (record EQ and playback loss), with crossfade on redesign
	FirDesigner fd;
	double hPre[128], hPreOld[128], hLos[128], hLosOld[128];
	int xfPre = 0, xfLos = 0; static const int XF = 256;
	double kPre[3] = {-1, -1, -1}, kLos[6] = {-1, -1, -1, -1, -1, -1};
	int updCount = 0;

	// per track state
	NoiseReduction enc[MAXT], dec[MAXT];
	FirLine linePre[MAXT], lineLos[MAXT];
	Up2 up[MAXT]; Down2 down[MAXT];
	double play[MAXT];
	std::vector<float> ring[MAXT]; int ringMask = 0, wpos = 0;
	NoiseSrc modN[MAXT], cnN[MAXT], levN[MAXT];
	Biquad hissF1[MAXT], hissF2[MAXT]; double hissNorm = 1;
	int doLeft[MAXT], doLen[MAXT]; double doDepth[MAXT], doS1[MAXT], doS2[MAXT];
	Biquad bump[MAXT], hpf[MAXT], xlp[MAXT];
	double vuAvg[MAXT]; double vuCoef = 0;
	double peakHold = 0;   // largest |level on tape| / threshold since last read

	// transport
	Rng rng;
	NoiseSrc nPhase, nWow, nFl, nScrape, nWeave;
	double phc = 0, php = 0, pw = 0, lw = 0, lf = 0, leakP = 0, humPh = 0;

	void init(double sampleRate, int tracks, uint64_t uSeed, uint64_t nSeed, const DeckSettings& s) {
		fs = sampleRate; fs2 = 2 * fs; C = std::max(1, std::min(MAXT, tracks)); unitSeed = uSeed; noiseSeed = nSeed; st = s;
		U = makeUnit(unitSeed);
		m = &MACHINES[clampi(st.machine, 0, NUM_MACHINES - 1)];
		tp = &(m->reel ? TAPES_REEL : TAPES_CASS)[clampi(st.tape, 0, 2)];
		fd.init(fs);
		rng.seed(noiseSeed * 7919 + 17);
		int need = (int)(0.05 * fs2) + 16, sz = 1; while (sz < need) sz <<= 1;
		ringMask = sz - 1; wpos = 0;
		for (int c = 0; c < MAXT; c++) {
			enc[c].init(st.nrEnc, true, fs); dec[c].init(st.nrDec, false, fs);
			linePre[c].reset(); lineLos[c].reset(); up[c].reset(); down[c].reset();
			play[c] = 0;
			if (c < C) ring[c].assign(sz, 0.f); else ring[c].clear();
			modN[c].lowpass(2500, fs2); cnN[c].lowpass(30, fs); levN[c].lowpass(4, fs);
			hissF1[c].shelf1(1500, 7000, fs); hissF2[c].lp2(std::min(17000.0, 0.45 * fs), fs); hissF1[c].reset(); hissF2[c].reset();
			doLeft[c] = 0; doLen[c] = 1; doDepth[c] = 0; doS1[c] = doS2[c] = 0;
			bump[c].reset(); hpf[c].reset(); xlp[c].lp1(200, fs); xlp[c].reset();
			vuAvg[c] = 0;
			cnN[c].warm(rng, (int)(0.2 * fs)); levN[c].warm(rng, (int)(fs));
		}
		{   // hiss chain noise gain
			Biquad a1 = hissF1[0], a2 = hissF2[0]; double s2 = 0, x = 1;
			for (int n = 0; n < (int)fs; n++) { double y = a2.process(a1.process(x)); x = 0; s2 += y * y; }
			hissNorm = 1.0 / std::sqrt(s2);
		}
		nPhase.lowpass(0.5, fs2); nWow.lowpass(1.2, fs2); nFl.bandpass(8, 45, fs2); nScrape.bandpass(2500, 6000, fs2); nWeave.lowpass(0.7, fs2);
		nPhase.warm(rng, (int)(2 * fs2)); nWow.warm(rng, (int)(2 * fs2)); nFl.warm(rng, (int)(0.2 * fs2)); nWeave.warm(rng, (int)(2 * fs2));
		fcap = m->v * 10 / (PI * m->dc); fpin = m->v * 10 / (PI * m->dp);
		phc = rng.uni() * 6.28; php = rng.uni() * 6.28; pw = 0; lw = 0; lf = 0; humPh = 0;
		leakP = std::exp(-1 / (2.0 * fs2));
		baseCoef = 1 - std::exp(-32.0 / (0.5 * fs2));
		vuCoef = 1 - std::exp(-1 / (0.3 * fs));
		aMax = 1 - std::exp(-2 * PI * 1.8);
		kPre[0] = kLos[0] = -1;   // force a design
		xfPre = xfLos = 0; updCount = 0; peakHold = 0;
		update(true);
		base = baseTarget;
		// level wobble and transport were warmed above; leaky integrators start at rest
	}
	static int clampi(int x, int a, int b) { return x < a ? a : (x > b ? b : x); }

	// Recompute everything that depends on the continuous settings. Call every 32 samples.
	void update(bool force = false) {
		const double age = st.age, wear = st.wear;
		S = clampd(st.strength, 0, 1); const double Sc = std::max(S, 0.02);
		g = db(st.recDb);
		double bias_e = clampd(st.bias + 0.3 * age * U.bias, -1, 1);
		az = (U.az * (2 + 16 * age) + 20 * st.azimuth) / 60 * PI / 180;
		double dHead = m->d0 + 0.35 * age + 0.3 * wear;
		double gap = m->gap * (1 + 0.6 * age);
		double Ms = db(tp->mol - 2.5 * wear + 1.5 * bias_e) / 0.55;
		double ub = std::max(0.0, -bias_e) + 0.25 * age;
		double dBias = bias_e > 0 ? 0.35 * bias_e : 0.2 * bias_e;
		MsEff = Ms / Sc; dcOff = 0.04 * age * Ms; hMem = S * std::min(0.35, 0.35 * ub); rPlay = 0.03 * Ms; t0 = std::tanh(dcOff / MsEff);
		peakThr = 0.55 * MsEff;
		a = m->wow / 100 * (1 + 5 * std::pow(age, 1.5)) * std::sqrt(2.0) / 1.3;
		kw = kn(st.wow); kf = kn(st.flutter);
		sig = kw * a * (0.5 / (2 * PI * fcap) + (0.6 + 1.5 * age) * 0.71 / (2 * PI * fpin) + (0.5 + 2 * age) * 0.2) + kf * a * 0.004;
		skew0 = 0.5 * (C - 1) * m->pitch * 1e3 * std::tan(40.0 / 60 * PI / 180) / (m->v * 1e4);
		baseTarget = st.quiet ? 0.0005 + skew0 : 0.0005 + skew0 + S * 4 * sig;
		mn = st.quiet ? 0.0 : S * db(-34 + 6 * age + 6 * wear);
		ageC = age;
		rate = (0.15 + 3.0 * age * age + 5.0 * wear) * kn(st.dropout);
		meanDepth = S * (0.5 + 1.5 * wear + 0.8 * age) * std::min(1.0, 2 * st.dropout);
		contact = S * 0.08 * (1 + 4 * wear);
		levAmp = S * db(-32 + 10 * age);
		for (int c = 0; c < C; c++) {
			sensG[c] = db(S * (U.sens[c] * 2.0 * age - 1.0 * wear));
			calG[c] = db(st.calDb + S * 1.5 * age * U.cal[c]);
			pos[c] = c - 0.5 * (C - 1);
		}
		xt = S * db(m->xt + 8 * age);
		hissLev = S * db(m->hiss + tp->hiss + 4 * age + 3 * wear) * kn(st.hiss);
		humLev = S * db(-78 + 22 * age) * kn(st.hiss);
		// filters that move with STRENGTH: head bump and low cut (keep their state)
		for (int c = 0; c < C; c++) {
			double z1 = bump[c].z1, z2 = bump[c].z2; bump[c].peak(m->bump, 1.7 * S, 1.1, fs); bump[c].z1 = z1; bump[c].z2 = z2;
			z1 = hpf[c].z1; z2 = hpf[c].z2; hpf[c].hp2(std::max(8.0, m->hp * S), fs); hpf[c].z1 = z1; hpf[c].z2 = z2;
		}
		// FIR redesign at most every 256 samples
		if (force || ++updCount >= 8) {
			updCount = 0;
			const double v = m->v, delta = tp->delta, cap = db(m->cap), d0 = m->d0, gap0 = m->gap;
			double wtan = m->w * 1e3 * std::tan(std::fabs(az));
			double kp[3] = {S, (double)st.machine, (double)st.tape};
			if (force || std::fabs(kp[0] - kPre[0]) > 1e-4 || kp[1] != kPre[1] || kp[2] != kPre[2]) {
				if (!force) { std::memcpy(hPreOld, hPre, sizeof hPre); xfPre = XF; }
				fd.design([&](double f) {
					double nom = tapeLoss(f, v, delta, d0, gap0, 0.0);
					double x = std::min(1.0 / nom, cap) / std::sqrt(1 + std::pow(f / 21000, 8));
					return std::pow(x, S);
				}, hPre);
				std::memcpy(kPre, kp, sizeof kp);
			}
			double dTot = dHead + dBias;
			double kl[6] = {S, (double)st.machine, (double)st.tape, dTot, gap, wtan};
			bool ch = force || kl[1] != kLos[1] || kl[2] != kLos[2] || std::fabs(kl[0] - kLos[0]) > 1e-4
			          || std::fabs(kl[3] - kLos[3]) > 1e-4 || std::fabs(kl[4] - kLos[4]) > 1e-4 || std::fabs(kl[5] - kLos[5]) > 1e-3;
			if (ch) {
				if (!force) { std::memcpy(hLosOld, hLos, sizeof hLos); xfLos = XF; }
				fd.design([&](double f) {
					double x = tapeLoss(f, v, delta, dTot, gap, wtan) / std::sqrt(1 + std::pow(f / 19000, 4));
					return std::pow(x, S);
				}, hLos);
				std::memcpy(kLos, kl, sizeof kl);
			}
		}
	}

	int firN() const { return fd.N; }
	// Nominal delay through one pass, in samples at fs: two FIRs, the x2 pair (M at 2 fs each, read at odd
	// 2 fs times by the decimator, so M - 0.5 at fs), and the tape delay.
	double latency() const { return (fd.N - 1) + HalfBand::M - 0.5 + base * fs; }

	inline double firOut(FirLine& L, const double* h, const double* hOld, int& xf) {
		const int N = fd.N;
		double y = L.conv(h, N);
		if (xf > 0) { double t = (double)xf / XF; y = (1 - t) * y + t * L.conv(hOld, N); }
		return y;
	}

	// One sample at fs. x, y: C tracks.
	void process(const double* x, double* y) {
		const int N = fd.N;
		double u0[MAXT], u1[MAXT];
		// ---- record
		for (int c = 0; c < C; c++) {
			double v = enc[c].process(x[c] * g);
			vuAvg[c] += vuCoef * (std::fabs(v) - vuAvg[c]);
			linePre[c].push(v, N);
			double r = firOut(linePre[c], hPre, hPreOld, xfPre);
			double pk = std::fabs(r) / peakThr; if (pk > peakHold) peakHold = pk;
			up[c].process(r, u0[c], u1[c]);
		}
		if (xfPre > 0) xfPre--;
		// ---- tape at 2 fs
		double o0[MAXT], o1[MAXT];
		for (int k = 0; k < 2; k++) {
			double delay, weave = 0;
			base += baseCoef / 32 * (baseTarget - base);
			if (st.quiet) delay = base;
			else {
				pw += 0.6 * nPhase.next(rng) / fs2;
				phc += 2 * PI * fcap / fs2; if (phc > 2 * PI) phc -= 2 * PI;
				php += 2 * PI * fpin / fs2; if (php > 2 * PI) php -= 2 * PI;
				lw = lw * leakP + nWow.next(rng) / fs2;
				lf = lf * leakP + nFl.next(rng) / fs2;
				double dly = kw * a * (0.7 / (2 * PI * fcap) * std::sin(phc + pw) + (0.6 + 1.5 * ageC) / (2 * PI * fpin) * std::sin(php) + (0.5 + 2.0 * ageC) * lw);
				dly += kf * (a * 0.7 * lf + 1.5e-7 * (1 + 3 * ageC) * nScrape.next(rng));
				delay = clampd(base + S * dly, 2 / fs2, 0.045);
				weave = S * nWeave.next(rng) * (1 + 7 * ageC) / 60 * PI / 180;
			}
			const double tz = std::tan(S * az + weave) / (m->v * 1e4) * m->pitch * 1e3;
			wpos = (wpos + 1) & ringMask;
			for (int c = 0; c < C; c++) {
				double uu = k == 0 ? u0[c] : u1[c];
				double& p = play[c];
				if (p < uu - rPlay) p = uu - rPlay; else if (p > uu + rPlay) p = uu + rPlay;
				uu = (1 - hMem) * uu + hMem * p;
				double val = MsEff * (std::tanh((uu + dcOff) / MsEff) - t0);
				if (mn > 0) val *= 1 + mn * modN[c].next(rng);
				float* rb = ring[c].data();
				rb[wpos] = (float)val;
				double d = (delay + pos[c] * tz) * fs2; if (d < 1.0) d = 1.0;
				int ki = (int)d; double f = d - ki;
				int j = wpos - ki;
				double x0 = rb[(j + 1) & ringMask], x1 = rb[j & ringMask], x2 = rb[(j - 1) & ringMask], x3 = rb[(j - 2) & ringMask];
				double out = -f * (f - 1) * (f - 2) / 6 * x0 + (f + 1) * (f - 1) * (f - 2) / 2 * x1 - (f + 1) * f * (f - 2) / 2 * x2 + (f + 1) * f * (f - 1) / 6 * x3;
				(k == 0 ? o0 : o1)[c] = out;
			}
		}
		// ---- playback
		double z[MAXT];
		const double vum = m->v * 1e4;
		for (int c = 0; c < C; c++) {
			double v = down[c].process(o0[c], o1[c]);
			lineLos[c].push(v, N);
			v = firOut(lineLos[c], hLos, hLosOld, xfLos);
			if (!st.quiet) {
				if (rate > 0) {
					double edge = (c == 0 || c == C - 1) ? 1.6 : 1.0;
					if (doLeft[c] == 0 && rng.uni() < rate * edge / fs) {
						doLen[c] = std::max(1, (int)(fs * (0.004 + 0.086 * rng.uni()))); doLeft[c] = doLen[c]; doDepth[c] = rng.expo(meanDepth);
					}
					double d = contact * std::fabs(cnN[c].next(rng));
					if (doLeft[c] > 0) { double ph = (double)(doLen[c] - doLeft[c]) / doLen[c]; d += doDepth[c] * (0.5 - 0.5 * std::cos(2 * PI * ph)); doLeft[c]--; }
					double fc = 0.086 * vum / std::max(d, 1e-3), aa;
					if (fc >= 1.8 * fs) aa = aMax; else aa = 1 - std::exp(-2 * PI * fc / fs);
					doS1[c] += aa * (v - doS1[c]); doS2[c] += aa * (doS1[c] - doS2[c]); v = doS2[c];
				}
				v *= 1 + levAmp * levN[c].next(rng);
			}
			v *= sensG[c];
			v = bump[c].process(v);
			v = hpf[c].process(v);
			z[c] = v;
		}
		if (xfLos > 0) xfLos--;
		double hum = 0;
		if (!st.quiet && humLev > 0) {
			humPh += 2 * PI * st.humHz / fs; if (humPh > 2 * PI) humPh -= 2 * PI;
			hum = humLev * (std::sin(humPh) + 0.5 * std::sin(2 * humPh + 1) + 0.3 * std::sin(3 * humPh + 2));
		}
		double lk[MAXT];
		if (C > 1) for (int c = 0; c < C; c++) lk[c] = z[c] + 4 * xlp[c].process(z[c]);
		for (int c = 0; c < C; c++) {
			double v = z[c];
			if (C > 1) { if (c > 0) v += xt * lk[c - 1]; if (c < C - 1) v += xt * lk[c + 1]; }
			if (!st.quiet && hissLev > 0) v += hissF2[c].process(hissF1[c].process(rng.gauss())) * hissNorm * hissLev + hum;
			v *= calG[c];
			v = dec[c].process(v);
			y[c] = v / g;
		}
	}

	double vu() const { double mx = 0; for (int c = 0; c < C; c++) mx = std::max(mx, vuAvg[c]); return mx * PI / 2; }   // 1 = 0 VU
	double takePeak() { double p = peakHold; peakHold = 0; return p; }
};

// ------------------------------------------------------------------ the other sections
struct Desk {   // multitracker channel: TRIM into an op-amp that clips (x2), then two swept shelves. Latency M at 2 fs.
	Up2 up; Down2 down; Biquad lo, hi, dcb; double fs = 48000;
	void init(double sr) { fs = sr; up.reset(); down.reset(); lo.reset(); hi.reset(); dcb.hp2(5, fs); dcb.reset(); set(0, 100, 0, 10000); }
	void set(double loDb, double loF, double hiDb, double hiF) {
		double a1 = lo.z1, a2 = lo.z2, b1 = hi.z1, b2 = hi.z2;
		lo.shelf(loF, loDb, fs, false); hi.shelf(hiF, hiDb, fs, true);
		lo.z1 = a1; lo.z2 = a2; hi.z1 = b1; hi.z2 = b2;
	}
	static inline double clip(double u) { double v = 1.5 * std::tanh(u / 1.5) + 0.03 * u * u; return v < -1.6 ? -1.6 : (v > 1.45 ? 1.45 : v); }
	inline double process(double x, double trimGain) {
		double a, b; up.process(x * trimGain, a, b);
		double y = down.process(clip(a), clip(b)) / trimGain;
		return hi.process(lo.process(dcb.process(y)));
	}
};

struct BusComp {   // budget VCA compressor: stereo-linked peak detector, hard knee, 3 ms / 120 ms
	double e = 0, ca, cr, gr = 0;
	void init(double fs) { e = 0; ca = 1 - std::exp(-1 / (0.003 * fs)); cr = 1 - std::exp(-1 / (0.12 * fs)); }
	inline void process(double& l, double& r, double thrDb, double ratio) {
		double mm = std::max(std::fabs(l), std::fabs(r));
		e += (mm > e ? ca : cr) * (mm - e);
		double over = 20 * std::log10(std::max(e, 1e-6)) - thrDb, gdb = 0;
		if (over > 0) gdb = -(over - over / ratio);
		gr = -gdb;
		double gg = db(gdb - 0.5 * thrDb * (1 - 1 / ratio));
		l *= gg; r *= gg;
	}
};

struct Dat {   // 16-bit with TPDF dither, full scale +15 dB over 0 VU, 8-pole 20 kHz low-pass
	Biquad lp[2][4]; Rng rng; double fsc = 1, q = 1;
	void init(double fs, uint64_t seed) {
		double fc = std::min(20000.0, 0.45 * fs);
		for (int c = 0; c < 2; c++) for (int k = 0; k < 4; k++) { lp[c][k].lp2(fc, fs, 1.0 / (2 * std::cos((2 * k + 1) * PI / 16))); lp[c][k].reset(); }
		rng.seed(seed); fsc = db(15.0); q = fsc / 32768;
	}
	inline double process(int c, double x) {
		for (int k = 0; k < 4; k++) x = lp[c][k].process(x);
		x = clampd(x, -fsc, fsc - q);
		return std::round(x / q + (rng.uni() - 0.5) + (rng.uni() - 0.5)) * q;
	}
};

}  // namespace tapeizer
