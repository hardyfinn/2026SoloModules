// Seal Legs DSP: stereo character delay with TAPE / BBD / DIGI models.
// Spec: vcv-modules/seal-legs.md. No Rack dependencies, so test/render.cpp can drive it offline.
// Signal unit throughout: u = volts / 5.
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

namespace seallegs {

// ---- tunable constants; (ear) values from the spec are design guesses ----
static const double TAPE_INERTIA_S = 0.120;     // (ear) two cascaded one-poles
static const double FAST_INERTIA_S = 0.005;     // BBD / DIGI speed inertia
static const double TAPE_SAG_DEPTH = 0.04;      // (ear) pitch dip at each chunk boundary
static const double TAPE_SAG_TAU = 0.080;
static const double TAPE_FREEZE_GAIN = 1.01;    // (ear) frozen tape creeps into saturation (spec said 1.03; that grows +5 dB in 30 s)
static const double BBD_FREEZE_GAIN = 1.0;
static const double BBD_FREEZE_LEAK = 0.25;     // (ear) dB lost per second when frozen, per second of delay (charge leakage)
static const double TAPE_WRITE_NOISE = 0.0015;
static const double TAPE_DRAG_RATE = 0.6;       // events per second per COLOR
static const double TAPE_DRAG_TAU = 0.150;
static const double TAPE_DROP_RATE = 3.0;       // events per second per COLOR^2
static const double BBD_JITTER = 0.16;          // (ear) clock instability per COLOR
static const double BBD_NOISE = 0.002;
static const double DRIVE_MAX_DB = 24.0;        // (ear)
static const double HISS_RMS = 0.02;            // (ear) at NOISE = 1
static const double CRACKLE_BOOST = 8.0;        // (ear) a one-sample click through the 2.5 kHz bandpass peaks at about 1/6 of its height
static const double FOG_BANDWIDTH_HZ = 7000.0;  // (ear)
static const double FOG_DAMPING = 0.35;         // (ear)
static const double FOG_EXCURSION = 24.0;       // (ear) samples at 29761 Hz
static const double FOG_MOD_HZ = 1.0;           // (ear)
static const double TAPESTOP_BRAKE_S = 0.9;     // (ear)
static const double TAPESTOP_EXP = 1.5;         // (ear)
static const double TAPESTOP_SPINUP_S = 0.5;
// Output limiter: linear to OUT_KNEE, then tanh into the 11 V ceiling. (The spec's plain
// 2.2*tanh(x/2.2) costs 0.5 dB at 5 V, which fails its own unity check, so a knee is used.)
static const double OUT_KNEE = 1.2;
static const double OUT_CEIL = 2.2;

enum Model { TAPE = 0, BBD = 1, DIGI = 2 };
enum Stereo { PONG = 0, TW = 1, LR = 2 };
enum Range { SHORT = 0, MED = 1, LONG = 2 };
enum Reverse { REV_OFF = 0, REV_ON = 1, REV_MIX = 2 };
enum Pos { POS_IN = 0, POS_DLY = 1, POS_MIX = 2, POS_LOOP = 3 };
enum AuxDest { AUX_DRIVE = 0, AUX_NOISE = 1, AUX_MOD = 2, AUX_FOG = 3, AUX_MIX = 4 };

static const double D_MIN[3] = {0.003125, 0.05, 0.2};
static const int BBD_STAGES[3] = {512, 8192, 32768};
static const double SYNC_DIV[3][7] = {
	{0.25, 1.0 / 3.0, 0.375, 0.5, 2.0 / 3.0, 0.75, 1.0},
	{1.0, 4.0 / 3.0, 1.5, 2.0, 8.0 / 3.0, 3.0, 4.0},
	{4.0, 16.0 / 3.0, 6.0, 8.0, 32.0 / 3.0, 12.0, 16.0},
};
static const double MOD_SYNC_16THS[13] = {64, 32, 24, 16, 12, 8, 6, 4, 3, 2, 1.5, 1, 0.5};
static const char* const MOD_SHAPE_NAMES[10] = {"Wow & flutter", "Envelope -", "Envelope +", "Random (LFV)",
	"Sample & hold", "Square", "Sine", "Saw (falling)", "Triangle", "Ramp (rising)"};

inline double clamp01(double x) { return x < 0 ? 0 : (x > 1 ? 1 : x); }
inline double clampd(double x, double a, double b) { return x < a ? a : (x > b ? b : x); }
inline double opc(double fc, double fs) { return 1.0 - std::exp(-2.0 * M_PI * fc / fs); }
inline double opcTau(double tau, double fs) { return 1.0 - std::exp(-1.0 / (tau * fs)); }

struct Rng {
	uint32_t s;
	explicit Rng(uint32_t seed = 1) : s(seed ? seed : 1) {}
	inline uint32_t u32() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
	inline double uni() { return (u32() >> 8) * (1.0 / 16777216.0); }
	inline double bi() { return uni() * 2.0 - 1.0; }
};

struct Biquad {
	double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
	inline double process(double x) {
		double y = b0 * x + z1;
		z1 = b1 * x - a1 * y + z2;
		z2 = b2 * x - a2 * y;
		return y;
	}
	void reset() { z1 = z2 = 0; }
	void lowpass(double fs, double fc, double Q) {
		fc = std::min(fc, 0.49 * fs);
		double w = 2 * M_PI * fc / fs, c = std::cos(w), al = std::sin(w) / (2 * Q), a0 = 1 + al;
		b0 = (1 - c) / 2 / a0; b1 = (1 - c) / a0; b2 = b0; a1 = -2 * c / a0; a2 = (1 - al) / a0;
	}
	void bandpass(double fs, double fc, double Q) {   // 0 dB peak
		double w = 2 * M_PI * fc / fs, c = std::cos(w), al = std::sin(w) / (2 * Q), a0 = 1 + al;
		b0 = al / a0; b1 = 0; b2 = -al / a0; a1 = -2 * c / a0; a2 = (1 - al) / a0;
	}
	void highShelf(double fs, double fc, double dB) {
		double A = std::pow(10, dB / 40), w = 2 * M_PI * fc / fs, c = std::cos(w), s = std::sin(w);
		double al = s / 2 * std::sqrt(2.0), sq = 2 * std::sqrt(A) * al;
		double a0 = (A + 1) - (A - 1) * c + sq;
		b0 = A * ((A + 1) + (A - 1) * c + sq) / a0; b1 = -2 * A * ((A - 1) + (A + 1) * c) / a0;
		b2 = A * ((A + 1) + (A - 1) * c - sq) / a0; a1 = 2 * ((A - 1) - (A + 1) * c) / a0;
		a2 = ((A + 1) - (A - 1) * c - sq) / a0;
	}
	void peaking(double fs, double fc, double Q, double dB) {
		double A = std::pow(10, dB / 40), w = 2 * M_PI * fc / fs, c = std::cos(w), al = std::sin(w) / (2 * Q);
		double a0 = 1 + al / A;
		b0 = (1 + al * A) / a0; b1 = -2 * c / a0; b2 = (1 - al * A) / a0; a1 = -2 * c / a0; a2 = (1 - al / A) / a0;
	}
};

// Linear ramp towards a target (crossfades of a fixed length).
struct Ramp {
	double v = 0;
	inline double step(double target, double inc) {
		if (v < target) v = std::min(target, v + inc);
		else if (v > target) v = std::max(target, v - inc);
		return v;
	}
};

// Paul Kellet's pinking filter (about -3 dB/oct), white in, pink out.
struct Pink {
	double b0 = 0, b1 = 0, b2 = 0, b3 = 0, b4 = 0, b5 = 0, b6 = 0;
	inline double process(double w) {
		b0 = 0.99886 * b0 + w * 0.0555179; b1 = 0.99332 * b1 + w * 0.0750759;
		b2 = 0.96900 * b2 + w * 0.1538520; b3 = 0.86650 * b3 + w * 0.3104856;
		b4 = 0.55000 * b4 + w * 0.5329522; b5 = -0.7616 * b5 - w * 0.0168980;
		double y = b0 + b1 + b2 + b3 + b4 + b5 + b6 + w * 0.5362;
		b6 = w * 0.115926;
		return y;
	}
};

// ============================================================================
// FOG: plate reverb after Dattorro, "Effect Design Part 1" (JAES 1997).
// Written from the paper's figure and tables; lengths scaled from 29761 Hz.
// ============================================================================
struct DelayLine {
	std::vector<float> b;
	int mask = 0, w = 0;
	void init(int maxLen) {
		int n = 1;
		while (n < maxLen + 4) n <<= 1;
		b.assign(n, 0.f); mask = n - 1; w = 0;
	}
	void clear() { std::fill(b.begin(), b.end(), 0.f); }
	inline void write(double x) { b[w] = (float)x; w = (w + 1) & mask; }
	inline double tap(int d) const { return b[(w - 1 - d) & mask]; }   // d = 0 is the newest sample
	inline double tapFrac(double d) const {
		int i = (int)d; double f = d - i;
		return tap(i) * (1 - f) + tap(i + 1) * f;
	}
};

struct Fog {
	double fs = 48000, k = 1;
	DelayLine in1, in2, in3, in4, apL, dL1, apL2, dL2, apR, dR1, apR2, dR2;
	int n_in1, n_in2, n_in3, n_in4, n_apL, n_dL1, n_apL2, n_dL2, n_apR, n_dR1, n_apR2, n_dR2;
	double bw = 0, dampL = 0, dampR = 0, lfo = 0, bwA = 0.5, exc = 16;
	int tL[7], tR[7];
	double fbL = 0, fbR = 0;

	int S(double n) const { return std::max(1, (int)std::round(n * k)); }
	void setSampleRate(double f) {
		fs = f; k = fs / 29761.0;
		n_in1 = S(142); n_in2 = S(107); n_in3 = S(379); n_in4 = S(277);
		n_apL = S(672); n_dL1 = S(4453); n_apL2 = S(1800); n_dL2 = S(3720);
		n_apR = S(908); n_dR1 = S(4217); n_apR2 = S(2656); n_dR2 = S(3163);
		exc = FOG_EXCURSION * k;
		in1.init(n_in1); in2.init(n_in2); in3.init(n_in3); in4.init(n_in4);
		apL.init(n_apL + (int)exc + 2); dL1.init(n_dL1); apL2.init(n_apL2); dL2.init(n_dL2);
		apR.init(n_apR + (int)exc + 2); dR1.init(n_dR1); apR2.init(n_apR2); dR2.init(n_dR2);
		const double l[7] = {266, 2974, 1913, 1996, 1990, 187, 1066};
		const double r[7] = {353, 3627, 1228, 2673, 2111, 335, 121};
		for (int i = 0; i < 7; i++) { tL[i] = S(l[i]); tR[i] = S(r[i]); }
		bwA = opc(FOG_BANDWIDTH_HZ, fs);
		clear();
	}
	void clear() {
		for (DelayLine* d : {&in1, &in2, &in3, &in4, &apL, &dL1, &apL2, &dL2, &apR, &dR1, &apR2, &dR2}) d->clear();
		bw = dampL = dampR = fbL = fbR = 0;
	}
	static inline double ap(DelayLine& d, int n, double g, double x) {
		double z = d.tap(n - 1);
		double v = x - g * z;
		d.write(v);
		return z + g * v;
	}
	static inline double apMod(DelayLine& d, double n, double g, double x) {
		double z = d.tapFrac(n - 1);
		double v = x - g * z;
		d.write(v);
		return z + g * v;
	}
	// mono in, stereo out
	inline void process(double x, double decay, double& outL, double& outR) {
		bw += bwA * (x - bw);
		double y = bw;
		y = ap(in1, n_in1, 0.75, y);
		y = ap(in2, n_in2, 0.75, y);
		y = ap(in3, n_in3, 0.625, y);
		y = ap(in4, n_in4, 0.625, y);
		lfo += 2 * M_PI * FOG_MOD_HZ / fs;
		if (lfo > 2 * M_PI) lfo -= 2 * M_PI;
		double m = std::sin(lfo) * exc;
		double dd2 = clampd(decay + 0.15, 0.25, 0.5);
		// left half of the tank, fed from the right half's end
		double l = y + decay * fbR;
		l = apMod(apL, n_apL + m, -0.7, l);
		dL1.write(l);
		double t = dL1.tap(n_dL1 - 1);
		dampL = t * (1 - FOG_DAMPING) + dampL * FOG_DAMPING;
		l = ap(apL2, n_apL2, dd2, dampL * decay);
		dL2.write(l);
		// right half
		double r = y + decay * fbL;
		r = apMod(apR, n_apR - m, -0.7, r);
		dR1.write(r);
		t = dR1.tap(n_dR1 - 1);
		dampR = t * (1 - FOG_DAMPING) + dampR * FOG_DAMPING;
		r = ap(apR2, n_apR2, dd2, dampR * decay);
		dR2.write(r);
		fbL = dL2.tap(n_dL2 - 1);
		fbR = dR2.tap(n_dR2 - 1);
		outL = 0.6 * (dR1.tap(tL[0]) + dR1.tap(tL[1]) - apR2.tap(tL[2]) + dR2.tap(tL[3])
			- dL1.tap(tL[4]) - apL2.tap(tL[5]) - dL2.tap(tL[6]));
		outR = 0.6 * (dL1.tap(tR[0]) + dL1.tap(tR[1]) - apL2.tap(tR[2]) + dL2.tap(tR[3])
			- dR1.tap(tR[4]) - apR2.tap(tR[5]) - dR2.tap(tR[6]));
	}
};

// ============================================================================
// Delay line: position buffer (spec 4) with readers, chunk reader, hard loop.
// ============================================================================
struct Reader {
	double Lr = 0.3, x0 = 0, r = 1, base = 0;
	bool chunk = false;
	int64_t ri = -1;
};

struct Line {
	double fs = 48000;
	std::vector<float> buf;
	std::vector<double> xb;
	int64_t N = 0, mask = 0, w = 0;
	double xw = 0, v = 1, L = 0.3;
	Reader cur, old;
	int fadeN = 0, fadeLen = 1;
	double rate = 1, segx0 = 0;
	bool syncRestart = false;
	double vTarget = 1, vs1 = 1, vs2 = 1, sag = 0, drag = 0, jit = 0;
	double fClkS = -1;   // smoothed BBD clock for the filters, so a JUMP does not step them
	// write chain state
	double dc = 0, hf1 = 0, hf2 = 0, tapeK = 0.5;
	Biquad pre, de, aa[2], rc[2], bump;
	double cenv = 0, eenv = 0;
	int dropN = 0;
	double dropTarget = 1, dropGain = 1;
	// feedback-path and output filters
	double fbHp = 0, fbLp = 0, oHp = 0, oLp = 0;
	// hard loop (DIGI freeze, tape stop)
	bool hard = false, writing = true;
	Ramp hardAmt;
	double fpos = 0;
	int64_t fstart = 0, flen = 1;
	Rng rng;
	// precomputed coefficients
	double aDC = 0, aTape = 0, aFast = 0, sagK = 0, dragK = 0, a40 = 0, aCompA = 0, aCompR = 0;

	inline double X(int64_t i) const { return xb[i & mask]; }
	inline float B(int64_t i) const { return buf[i & mask]; }

	void init(double fs_, double L0, uint32_t seed) {
		fs = fs_;
		N = 1;
		while (N < (int64_t)(18.0 * fs)) N <<= 1;
		mask = N - 1;
		buf.assign(N, 0.f);
		xb.resize(N);
		for (int64_t i = 0; i < N; i++) xb[i] = (double)(i - N) / fs;   // a history of silence at speed 1
		w = N; xw = 0; v = 1; L = L0;
		vTarget = vs1 = vs2 = 1;
		rate = 1; segx0 = 0; fadeN = 0;
		cur = Reader(); cur.Lr = L0; old = cur;
		hard = false; writing = true; hardAmt.v = 0;
		rng = Rng(seed);
		aDC = opc(30, fs); aTape = opcTau(TAPE_INERTIA_S, fs); aFast = opcTau(FAST_INERTIA_S, fs);
		sagK = std::exp(-1.0 / (TAPE_SAG_TAU * fs)); dragK = std::exp(-1.0 / (TAPE_DRAG_TAU * fs));
		a40 = opc(40, fs); aCompA = opc(400, fs); aCompR = opc(12, fs);
		pre.highShelf(fs, 3000, 5); de.highShelf(fs, 3000, -5); bump.peaking(fs, 90, 0.9, 2.5);
		clearState();
	}
	void clearState() {
		dc = hf1 = hf2 = 0;
		pre.reset(); de.reset(); bump.reset();
		for (int i = 0; i < 2; i++) { aa[i].reset(); rc[i].reset(); }
		cenv = eenv = 0;
		dropN = 0; dropTarget = dropGain = 1;
		fbHp = fbLp = oHp = oLp = 0;
		sag = drag = jit = 0;
	}
	void clearAudio() {
		std::fill(buf.begin(), buf.end(), 0.f);
		clearState();
	}

	// Fractional index of a medium position (spec 4.2).
	void locate(double target, int64_t& ri, int64_t& i, double& f) const {
		int64_t lo = w - N + 8, hi = w - 3;
		if (target <= X(lo)) { i = ri = lo; f = 0; return; }
		if (target >= X(hi)) { i = ri = hi; f = 0; return; }
		int64_t j = ri;
		bool ok = false;
		if (j >= lo && j < hi) {
			int steps = 0;
			while (j > lo && X(j) > target && steps < 64) { j--; steps++; }
			while (j < hi - 1 && X(j + 1) <= target && steps < 64) { j++; steps++; }
			ok = X(j) <= target && X(j + 1) > target;
		}
		if (!ok) {
			int64_t a = lo, b = hi;
			while (b - a > 1) {
				int64_t m = a + (b - a) / 2;
				if (X(m) <= target) a = m; else b = m;
			}
			j = a;
		}
		ri = i = j;
		double dx = X(j + 1) - X(j);
		f = dx > 1e-18 ? (target - X(j)) / dx : 0;
		f = clamp01(f);
	}
	inline double herm(int64_t i, double f) const {
		double a = B(i - 1), b = B(i), c = B(i + 1), d = B(i + 2);
		double c1 = 0.5 * (c - a), c2 = a - 2.5 * b + 2 * c - 0.5 * d, c3 = 0.5 * (d - a) + 1.5 * (b - c);
		return ((c3 * f + c2) * f + c1) * f + b;
	}
	inline double targetOf(const Reader& r) const { return r.chunk ? r.base + r.r * (xw - r.x0) : xw - r.Lr; }
	double readReader(Reader& r, double grid) const {
		double t = targetOf(r);
		double lim = xw - 3 * v / fs;
		if (t > lim) t = lim;
		if (grid > 0) t = std::floor(t / grid) * grid;
		int64_t i; double f;
		locate(t, r.ri, i, f);
		double y = herm(i, f);
		if (r.chunk && std::fabs(r.r) > 1.5) y = 0.5 * (y + herm(i - 1, f));   // crude half-band at x2
		return y;
	}
	void startReader(Reader& r, double rt, double Lr) {
		r.Lr = Lr; r.r = rt; r.x0 = xw; r.chunk = rt != 1.0; r.ri = -1;
		if (r.chunk) r.base = rt < 0 ? xw - 3 * v / fs : xw - std::max(1.0, rt) * Lr;
	}
	// Normal (non-hard) read with the reader crossfade.
	double readNormal(double grid) {
		Reader& c = cur;
		double y = readReader(c, grid);
		if (fadeN > 0) {
			double wt = 0.5 - 0.5 * std::cos(M_PI * (double)fadeN / fadeLen);
			y = (1 - wt) * y + wt * readReader(old, grid);
		}
		return y;
	}

	// Speed (spec 4.3 + model wobble).
	void updateSpeed(int model, double modFactor, double color) {
		if (model == TAPE) { vs1 += aTape * (vTarget - vs1); vs2 += aTape * (vs1 - vs2); }
		else { vs1 += aFast * (vTarget - vs1); vs2 = vs1; }
		sag *= sagK;
		double vv = vs2 * modFactor * (1 - sag);
		if (model == BBD) {
			jit += a40 * (rng.bi() - jit);
			vv *= 1 + BBD_JITTER * color * jit;
		}
		else if (model == TAPE) {
			if (color > 0 && rng.uni() < TAPE_DRAG_RATE * color / fs) drag = (0.02 + 0.04 * rng.uni()) * 2 * color;
			drag *= dragK;
			vv *= 1 - drag;
		}
		v = std::max(vv, 0.02);
	}

	// Reader management: chunk segments, rate changes, GLIDE / JUMP (spec 4.3, 4.4).
	void manage(int model, double rateWanted, double Dstar, bool jumpMode, bool& forceJump) {
		int fadeShort = (int)std::max(1.0, (model == DIGI ? 0.003 : 0.010) * fs);
		if (fadeN == 0) {
			bool boundary = rate != 1.0 && xw - segx0 >= cur.Lr;
			bool restart = syncRestart && rateWanted != 1.0;
			if (rateWanted != rate || boundary || restart) {
				old = cur;
				rate = rateWanted;
				startReader(cur, rate, L);
				segx0 = xw;
				fadeN = fadeLen = fadeShort;
				syncRestart = false;
				if (model == TAPE && rate != 1.0) sag = TAPE_SAG_DEPTH;
			}
		}
		if (!jumpMode && !forceJump) {
			vTarget = L / Dstar;
		}
		else {
			double Lnew = Dstar * vs2;
			if (std::fabs(Lnew - L) > 0.005 * L) {
				if (fadeN == 0) {
					old = cur;
					L = Lnew;
					startReader(cur, rate, L);
					segx0 = xw;
					fadeN = fadeLen = (int)std::max(1.0, (model == DIGI ? 0.005 : 0.030) * fs);
					forceJump = false;
				}
			}
			else forceJump = false;
		}
	}
	inline void advanceFade() { if (fadeN > 0) fadeN--; }

	// Hard loop (spec 7.2).
	void hardStart() {
		int64_t ri = w - 10, i; double f;
		locate(xw - L, ri, i, f);
		fstart = i;
		flen = std::max<int64_t>(16, (w - 1) - fstart);
		fpos = 0;
		hard = true;
	}
	double hardRead(double step, double gridStep) {
		double pos = fpos;
		if (gridStep > 1) pos = std::floor(fpos / gridStep) * gridStep;
		int64_t ii = (int64_t)std::floor(pos);
		double f = pos - ii;
		double y = herm(fstart + ii, f);
		if (std::fabs(step) > 1.5) y = 0.5 * (y + herm(fstart + ii - 1, f));
		double seam = std::min(0.010 * fs, flen * 0.5);
		double rem = flen - pos;
		if (rem < seam) {
			double wt = 1 - rem / seam;
			double z = herm(fstart + ii - flen, f);
			if (std::fabs(step) > 1.5) z = 0.5 * (z + herm(fstart + ii - flen - 1, f));
			y = (1 - wt) * y + wt * z;
		}
		fpos += step;
		while (fpos >= flen) fpos -= flen;
		while (fpos < 0) fpos += flen;
		return y;
	}

	// Write chains (spec 5).
	void write(double x, int model, double color, double tapeFc, int div) {
		if (!writing) return;
		dc += aDC * (x - dc);
		x -= dc;
		if (model == TAPE) {
			if ((div & 31) == 0) tapeK = opc(tapeFc, fs);
			x += TAPE_WRITE_NOISE * rng.bi();
			x = pre.process(x);
			x = std::tanh(x + 0.05) - std::tanh(0.05);
			x = de.process(x);
			hf1 += tapeK * (x - hf1);
			hf2 += tapeK * (hf1 - hf2);
			x = hf2;
			if (dropN > 0) { if (--dropN == 0) dropTarget = 1; }
			else if (color > 0 && rng.uni() < TAPE_DROP_RATE * color * color / fs) {
				dropN = (int)((0.02 + 0.1 * rng.uni()) * fs);
				dropTarget = 0.15 + 0.5 * rng.uni();
			}
			dropGain += a40 * (dropTarget - dropGain);
			x *= dropGain;
		}
		else if (model == BBD) {
			// Compressor with its detector on its own output (feedback, as in the NE570-style
			// companders), so it and the expander both detect the compressed signal and track.
			// Same 2:1 law as the spec's feedforward form; that form mistracks by about
			// +0.5 dB per pass and made frozen BBD loops grow instead of fade.
			x = x / std::max(cenv, 0.1);
			double e = std::fabs(x) / 0.637;
			cenv += (e > cenv ? aCompA : aCompR) * (e - cenv);
			x = aa[1].process(aa[0].process(x));
			x += BBD_NOISE * (1 + 8 * color) * rng.bi();
			const double c = 1.4;
			x = c * std::tanh((x + 0.03) / c) - c * std::tanh(0.03 / c);
		}
		else {
			x = clampd(x, -2.4, 2.4);
		}
		buf[w & mask] = (float)x;
		xb[w & mask] = xw;
		w++;
		xw += v / fs;
	}
	void setBbdFilters(double fc) {
		aa[0].lowpass(fs, fc, 0.54); aa[1].lowpass(fs, fc, 1.31);
		rc[0].lowpass(fs, fc, 0.54); rc[1].lowpass(fs, fc, 1.31);
	}
	// BBD read side: reconstruction filter and expander.
	inline double bbdRead(double y) {
		y = rc[1].process(rc[0].process(y));
		double e = std::fabs(y) / 0.637;
		eenv += (e > eenv ? aCompA : aCompR) * (e - eenv);
		return y * std::max(eenv, 0.01);
	}
};

// ============================================================================
// Engine
// ============================================================================
struct Frame {
	double inL = 0, inR = 0;          // volts
	double trim = 1;                  // input trim, linear
	// effective controls (after CV, AUX and mutation), 0..1 unless noted
	double drive = 0.5, noise = 0, noiseMode = 0, fog = 0, fogDecay = 0.5, mix = 0.5, duck = 0;
	double time = 0.5, width = 0, fb = 0.4, modRate = 0.3, modDepth = 0, modShape = 0 /* 0..9 */;
	double color = 0, hp = 0, lp = 1;
	int range = MED, stereo = TW, model = TAPE, fogPos = POS_DLY, drivePos = POS_IN;
	int reverse = REV_OFF;            // after REV gate and mutation
	double speed = 1;                 // 0.5, 1 or 2
	bool jump = false, tapeStop = false;
	bool freeze = false, delaySync = false, modSync = false, fbCross = false, modPhase = false;
	bool tap = false, clear = false;  // events this sample
	bool clkEdge = false, clkConnected = false, clkQuarter = false;
	bool sendReturn = false, modOut10 = false;
	// test hook: when > 0, overrides the TIME mapping (seconds for line A; line B follows WIDTH as usual)
	double timeOverride = 0;
	double fbGainOverride = -1;       // test hook: feedback gain directly
};

struct Output {
	double l = 0, r = 0, mod = 0;
	double driveGreen = 0, driveRed = 0, modPos = 0, modNeg = 0;
	bool beat = false;
};

inline double feedbackGain(double s) { return s <= 0.75 ? s / 0.75 : 1 + 0.6 * (s - 0.75) / 0.25; }
inline double driveGain(double k) { return k <= 0.5 ? (2 * k) * (2 * k) : std::pow(10.0, DRIVE_MAX_DB * (2 * k - 1) / 20.0); }
inline double timeFree(int range, double s) { return D_MIN[range] * std::pow(30.0, s); }
inline double sliderForTime(int range, double secs) { return std::log(secs / D_MIN[range]) / std::log(30.0); }

struct Engine {
	double fs = 48000;
	Line A, B;
	Fog fog;
	Rng rng{12345};
	int div = 0;

	// tempo
	double Pc = 0.125;
	int64_t sinceClk = 0, sinceTap = 1 << 30;
	bool clkSeen = false, tapResetPending = false, segRestartPending = false, prevDelaySync = false;
	int clkCount = 0;
	double beatPh = 0;

	// mod
	double modPh = 0, sh = 0, lfvC = 0, lfvT = 0, lfv1 = 0, lfv2 = 0, ef = 0;
	double tph[4] = {0, 1, 2, 3}, wn1 = 0, wn2 = 0, fn1 = 0, fn2 = 0;

	// smoothers
	double sColor = 0, sHp = 0, sLp = 1, sG = 0.5, sMix = 0.5, sDrive = 1, sFog = 0, sDuck = 0, sNoise = 0, sDecay = 0.5;
	double aSmooth = 0.01, aClk = 0.01;
	double hpOn = 0, lpOn = 0;
	double aHp = 0, aLp = 1;

	// crossfades
	int modelCur = TAPE, modelOld = TAPE, modelFadeN = 0, modelFadeLen = 480;
	int routeStereo = TW, routeCross = 0, oldStereo = TW, oldCross = 0, routeFadeN = 0, routeFadeLen = 480;
	Ramp wIn, wDly, wMix;
	int fogPosCur = POS_DLY;
	Ramp fogPosGain;
	Ramp freezeAmt;
	bool frozenPrev = false;
	int rangeCur = MED;
	bool forceJumpA = false, forceJumpB = false;
	bool tapeStopLatched = false;
	// tape stop
	double rho = 1;
	bool spinUp = false;
	double spinFrom = 1, spinT = 0, brakeT = 0;
	// clear
	int clearState = 0;   // 0 idle, 1 fading out, 2 fading in
	Ramp clearGain;
	// noise
	Pink pinkL, pinkR;
	double pinkScale = 1;
	Biquad crkL, crkR;
	// duck, drive light
	double duckEnv = 0, aDuckA = 0, aDuckR = 0;
	double clipHold = 0;
	double prevYA = 0, prevYB = 0;

	Engine() { setSampleRate(48000); }

	void setSampleRate(double f) {
		fs = f;
		A.init(fs, timeFree(MED, 0.5), 0x1234567u);
		B.init(fs, timeFree(MED, 0.5), 0x7654321u);
		fog.setSampleRate(fs);
		aSmooth = opcTau(0.005 / 3.0, fs);   // settles in about 5 ms
		aClk = opcTau(0.010, fs);
		modelFadeLen = routeFadeLen = (int)(0.010 * fs);
		crkL.bandpass(fs, 2500, 2); crkR.bandpass(fs, 2500, 2);
		aDuckA = opcTau(0.005, fs); aDuckR = opcTau(0.250, fs);
		// calibrate the pink filter to unit rms
		Pink p; Rng r(99); double acc = 0; const int n = 200000;
		for (int i = 0; i < n; i++) { double y = p.process(r.bi() * std::sqrt(3.0)); if (i > 20000) acc += y * y; }
		pinkScale = 1.0 / std::sqrt(acc / (n - 20001));
		wIn.v = 1; wDly.v = 0; wMix.v = 0; fogPosGain.v = 1; freezeAmt.v = 0; clearGain.v = 1;
		hpOn = 0; lpOn = 0;
		initialised = false;
	}
	bool initialised = false;

	void resetDSP() {
		A.clearAudio(); B.clearAudio(); fog.clear();
		A.hard = B.hard = false; A.writing = B.writing = true; A.hardAmt.v = B.hardAmt.v = 0;
		rho = 1; spinUp = false;
		lfv1 = lfv2 = ef = 0; duckEnv = 0; prevYA = prevYB = 0;
		crkL.reset(); crkR.reset(); pinkL = Pink(); pinkR = Pink();
	}

	// Delay time for each line in seconds (spec 8).
	void times(const Frame& f, double& DA, double& DB) {
		int rg = f.range;
		if (f.timeOverride > 0) DA = f.timeOverride;
		else if (f.delaySync) {
			DA = Pc * SYNC_DIV[rg][(int)std::round(clamp01(f.time) * 6)];
			while (DA > 8) DA *= 0.5;
		}
		else DA = timeFree(rg, f.time);
		if (f.stereo == LR) {
			if (f.delaySync) {
				DB = Pc * SYNC_DIV[rg][(int)std::round(clamp01(f.width) * 6)];
				while (DB > 8) DB *= 0.5;
			}
			else DB = timeFree(rg, f.width);
		}
		else DB = DA * (1 + 0.33 * f.width);
	}

	static inline double driveShape(double x, double gain, bool& clip) {
		double xo = x * gain + 0.03;
		double a = std::fabs(xo);
		double y = xo;
		if (a > 0.8) { clip = true; y = (xo < 0 ? -1 : 1) * (0.8 + 0.5 * std::tanh((a - 0.8) / 0.5)); }
		return y - 0.03;
	}
	static inline double limiter(double x) {
		double a = std::fabs(x);
		if (a <= OUT_KNEE) return x;
		double k = OUT_CEIL - OUT_KNEE;
		return (x < 0 ? -1 : 1) * (OUT_KNEE + k * std::tanh((a - OUT_KNEE) / k));
	}

	// Modulator m in -1..+1 (spec 9).
	double modulator(const Frame& f, double mono) {
		double rate = clamp01(f.modRate);
		// envelope follower always runs
		double tau = 0.5 * std::pow(0.004, rate);
		double ca = opcTau(tau / 4, fs), cr = opcTau(tau, fs);
		ef += (mono * 1.6 > ef ? ca : cr) * (mono * 1.6 - ef);
		double env = clamp01(ef);
		double k = clampd(f.modShape, 0, 9);
		if (k < 0.5) {
			double wr = 0.7 + 0.6 * rate;
			const double hz[4] = {0.47, 1.13, 6.3, 11.7};
			for (int i = 0; i < 4; i++) { tph[i] += 2 * M_PI * hz[i] * wr / fs; if (tph[i] > 2 * M_PI) tph[i] -= 2 * M_PI; }
			double a1 = opc(1.0, fs), a25 = opc(25, fs), a5 = opc(5, fs);
			wn1 += a1 * (rng.bi() - wn1); wn2 += a1 * (wn1 - wn2);
			fn1 += a25 * (rng.bi() - fn1); fn2 += a5 * (fn1 - fn2);
			double wow = 0.6 * std::sin(tph[0]) + 0.3 * std::sin(tph[1]) + 0.5 * wn2 * 150;
			double fl = 0.5 * std::sin(tph[2]) + 0.3 * std::sin(tph[3]) + 0.4 * (fn1 - fn2) * 25;
			return wow * std::cos(rate * M_PI / 2) + 0.25 * fl * std::sin(rate * M_PI / 2);
		}
		if (k < 1.5) return -env;
		if (k < 2.5) return env;
		// oscillating shapes
		double fr = 0.1 * std::pow(400.0, rate);
		if (f.modSync) fr = 1.0 / (MOD_SYNC_16THS[(int)std::round(rate * 12)] * Pc);
		modPh += fr / fs;
		if (modPh >= 1) { modPh -= std::floor(modPh); sh = rng.bi(); }
		lfvC += 2 * fr / fs;
		if (lfvC >= 1) { lfvC -= std::floor(lfvC); lfvT = rng.bi(); }
		double af = opc(fr, fs);
		lfv1 += af * (lfvT * 1.6 - lfv1);
		lfv2 += af * (lfv1 - lfv2);
		auto shape = [&](int s) -> double {
			switch (s) {
				case 3: return clampd(lfv2, -1, 1);
				case 4: return sh;
				case 5: return modPh < 0.5 ? 1 : -1;
				case 6: return std::sin(2 * M_PI * modPh);
				case 7: return 1 - 2 * modPh;
				case 8: return modPh < 0.5 ? 4 * modPh - 1 : 3 - 4 * modPh;
				default: return 2 * modPh - 1;
			}
		};
		if (k <= 3) return shape(3);
		int n = std::min(8, (int)std::floor(k));
		double fr2 = k - n;
		double t = fr2 < 0.1 ? 0 : (fr2 > 0.9 ? 1 : (fr2 - 0.1) / 0.8);
		if (n >= 9) return shape(9);
		if (t <= 0) return shape(n);
		if (t >= 1) return shape(n + 1);
		return (1 - t) * shape(n) + t * shape(n + 1);
	}

	Output process(const Frame& f) {
		Output o;
		if (!initialised) {
			double DA, DB;
			times(f, DA, DB);
			A.L = A.cur.Lr = DA; B.L = B.cur.Lr = DB;
			modelCur = modelOld = f.model; rangeCur = f.range;
			routeStereo = oldStereo = f.stereo; routeCross = oldCross = f.fbCross;
			fogPosCur = f.fogPos;
			wIn.v = f.drivePos == POS_IN; wDly.v = f.drivePos == POS_DLY; wMix.v = f.drivePos == POS_MIX;
			sColor = f.color; sHp = f.hp; sLp = f.lp; sMix = f.mix; sFog = f.fog; sDecay = f.fogDecay;
			sG = f.fbGainOverride >= 0 ? f.fbGainOverride : feedbackGain(f.fb);
			sDrive = driveGain(f.drive); sNoise = f.noise; sDuck = f.duck;
			hpOn = f.hp >= 0.01; lpOn = f.lp <= 0.99;
			initialised = true;
		}
		div++;
		const double inc10 = 1.0 / (0.010 * fs), inc5 = 1.0 / (0.005 * fs);

		// ---- tempo (spec 8) ----
		sinceClk++; sinceTap++;
		bool clkEdge = f.clkConnected && f.clkEdge;
		if (f.clkConnected) {
			if (clkEdge) {
				if (clkSeen) {
					double iv = sinceClk / fs;
					if (iv > 0.001 && iv < 16) Pc = f.clkQuarter ? iv / 4 : iv;
				}
				clkSeen = true; sinceClk = 0;
				clkCount++;
				if (f.clkQuarter || clkCount % 4 == 0) { beatPh = 0; o.beat = true; }
			}
		}
		else { clkSeen = false; clkCount = 0; }
		if (f.tap) {
			if (!f.clkConnected) {
				double iv = sinceTap / fs;
				if (iv >= 0.1 && iv <= 3) Pc = iv / 4;
			}
			sinceTap = 0;
			tapResetPending = true;
		}
		if (tapResetPending && (!f.clkConnected || clkEdge)) {
			modPh = 0; lfvC = 0; tapResetPending = false;
			if (!f.clkConnected) { beatPh = 0; o.beat = true; clkCount = 0; }
			segRestartPending = true;
		}
		if (!f.clkConnected) {
			beatPh += 1.0 / (4 * Pc * fs);
			if (beatPh >= 1) { beatPh -= 1; o.beat = true; }
		}
		if (f.delaySync && !prevDelaySync) segRestartPending = true;
		prevDelaySync = f.delaySync;
		if (segRestartPending && f.delaySync && (clkEdge || !f.clkConnected)) {
			A.syncRestart = B.syncRestart = true;
			segRestartPending = false;
		}
		if (!f.delaySync) segRestartPending = false;

		// ---- input ----
		double xl = f.inL / 5.0 * f.trim, xr = f.inR / 5.0 * f.trim;
		if (!std::isfinite(xl)) xl = 0;
		if (!std::isfinite(xr)) xr = 0;
		double mono = 0.5 * (std::fabs(xl) + std::fabs(xr));

		// ---- smoothed controls ----
		sColor += aSmooth * (f.color - sColor);
		sHp += aSmooth * (f.hp - sHp);
		sLp += aSmooth * (f.lp - sLp);
		sMix += aSmooth * (f.mix - sMix);
		sFog += aSmooth * (f.fog - sFog);
		sDecay += aSmooth * (f.fogDecay - sDecay);
		sNoise += aSmooth * (f.noise - sNoise);
		sDuck += aSmooth * (f.duck - sDuck);
		sDrive += aSmooth * (driveGain(f.drive) - sDrive);
		double gT = f.fbGainOverride >= 0 ? f.fbGainOverride : feedbackGain(f.fb);
		sG += aSmooth * (gT - sG);
		hpOn += aSmooth * ((f.hp >= 0.01 ? 1.0 : 0.0) - hpOn);
		lpOn += aSmooth * ((f.lp <= 0.99 ? 1.0 : 0.0) - lpOn);
		if ((div & 31) == 1) {
			aHp = opc(10.0 * std::pow(800.0, sHp), fs);
			aLp = opc(std::min(80.0 * std::pow(250.0, sLp), 0.45 * fs), fs);
		}
		double color = clamp01(sColor);

		// ---- MOD ----
		double m = modulator(f, mono);
		double depth = clamp01(f.modDepth);
		double k = clampd(f.modShape, 0, 9);
		double modA, modB;
		if (k < 0.5) { modA = 1 + 0.05 * depth * m; modB = 1 + 0.05 * depth * (f.modPhase ? -m : m); }
		else { modA = std::pow(2.0, -m * depth * depth); modB = f.modPhase ? 1.0 / modA : modA; }
		double mOut = clampd(m, -1, 1);
		o.mod = mOut * (f.modOut10 ? 10 : 5);
		o.modPos = std::max(0.0, mOut); o.modNeg = std::max(0.0, -mOut);

		// ---- DRIVE + dry tap + send (spec 3, 10.1) ----
		wIn.step(f.drivePos == POS_IN ? 1 : 0, inc10);
		wDly.step(f.drivePos == POS_DLY ? 1 : 0, inc10);
		wMix.step(f.drivePos == POS_MIX ? 1 : 0, inc10);
		bool clip = false;
		double dL = xl, dR = xr;
		if (wIn.v > 0) { dL += wIn.v * (driveShape(xl, sDrive, clip) - xl); dR += wIn.v * (driveShape(xr, sDrive, clip) - xr); }
		double sL = dL, sR = dR;
		if (wDly.v > 0) { sL += wDly.v * (driveShape(dL, sDrive, clip) - dL); sR += wDly.v * (driveShape(dR, sDrive, clip) - dR); }

		// ---- FOG position switching: fade the fog out, switch, fade back in ----
		if (f.fogPos != fogPosCur) { if (fogPosGain.step(0, inc5) <= 0) fogPosCur = f.fogPos; }
		else fogPosGain.step(1, inc5);
		double fogF = clamp01(sFog) * fogPosGain.v;
		double fogC = std::cos(fogF * M_PI / 2), fogS = std::sin(fogF * M_PI / 2);
		double decay = 0.2 + 0.795 * clamp01(sDecay);
		bool fogRan = false;
		double revL = 0, revR = 0;
		if (fogPosCur == POS_IN) {
			fog.process(0.5 * (sL + sR), decay, revL, revR); fogRan = true;
			sL = sL * fogC + 1.3 * revL * fogS; sR = sR * fogC + 1.3 * revR * fogS;
		}

		// ---- NOISE (spec 10.2) ----
		double na = clamp01(sNoise), nm = clamp01(f.noiseMode);
		double preG = nm < 0.5 ? std::cos(M_PI * nm) : 0, crkG = std::sin(M_PI * nm), postG = nm > 0.5 ? -std::cos(M_PI * nm) : 0;
		double hissL = 0, hissR = 0;
		if (na > 0.0005) {
			double lvl = HISS_RMS * na * na * pinkScale;
			hissL = lvl * pinkL.process(rng.bi() * std::sqrt(3.0));
			hissR = lvl * pinkR.process(rng.bi() * std::sqrt(3.0));
			double rate = (3 + 25 * na) / fs;
			double iL = 0, iR = 0;
			if (rng.uni() < rate) { double g = rng.bi(); iL = 0.5 * na * na * std::fabs(g * g * g) * (rng.uni() < 0.5 ? -1 : 1); }
			if (rng.uni() < rate) { double g = rng.bi(); iR = 0.5 * na * na * std::fabs(g * g * g) * (rng.uni() < 0.5 ? -1 : 1); }
			double cL = crkL.process(iL * CRACKLE_BOOST), cR = crkR.process(iR * CRACKLE_BOOST);
			sL += preG * hissL + crkG * cL;
			sR += preG * hissR + crkG * cR;
		}

		// ---- FREEZE, model, routing, range ----
		bool frz = f.freeze;
		if (frz && !frozenPrev) tapeStopLatched = f.tapeStop;
		frozenPrev = frz;
		freezeAmt.step(frz ? 1 : 0, inc10);
		if (f.model != modelCur && modelFadeN == 0) {
			modelOld = modelCur; modelCur = f.model; modelFadeN = modelFadeLen;
		}
		if ((f.stereo != routeStereo || (int)f.fbCross != routeCross) && routeFadeN == 0) {
			oldStereo = routeStereo; oldCross = routeCross;
			routeStereo = f.stereo; routeCross = f.fbCross; routeFadeN = routeFadeLen;
		}
		if (f.range != rangeCur) { rangeCur = f.range; forceJumpA = forceJumpB = true; }

		// ---- CLEAR (spec 7.4) ----
		if (f.clear && clearState == 0) clearState = 1;
		if (clearState == 1) {
			if (clearGain.step(0, inc5) <= 0) {
				A.clearAudio(); B.clearAudio(); fog.clear();
				prevYA = prevYB = 0;
				clearState = 2;
				if (f.delaySync) segRestartPending = true;
			}
		}
		else if (clearState == 2) { if (clearGain.step(1, inc5) >= 1) clearState = 0; }

		double fa = freezeAmt.v;
		double send = (1 - fa);
		sL *= send; sR *= send;

		// ---- hard loop (DIGI freeze / tape stop) ----
		bool hardWanted = frz && (modelCur == DIGI || (modelCur == TAPE && tapeStopLatched));
		bool tapeStopping = modelCur == TAPE && tapeStopLatched;
		Line* L2[2] = {&A, &B};
		for (Line* ln : L2) {
			if (hardWanted && !ln->hard && ln->hardAmt.v <= 0) { ln->hardStart(); }
		}
		if (hardWanted && A.hard && !spinUp && tapeStopping) {
			brakeT += 1.0 / fs;
			double u = 1 - brakeT / TAPESTOP_BRAKE_S;
			rho = u > 0 ? std::pow(u, TAPESTOP_EXP) : 0;
		}
		if (!hardWanted && A.hard) {
			if (tapeStopping && rho < 1) {
				if (!spinUp) { spinUp = true; spinFrom = rho; spinT = 0; }
				spinT += 1.0 / fs;
				double u = std::min(1.0, spinT / TAPESTOP_SPINUP_S);
				rho = spinFrom + (1 - spinFrom) * (1 - (1 - u) * (1 - u));
				if (u >= 1) rho = 1;
			}
			if (!tapeStopping || rho >= 1) {
				for (Line* ln : L2) if (ln->hard && ln->hardAmt.v >= 1) { ln->hard = false; ln->writing = true; }
				spinUp = false;
			}
		}
		if (hardWanted && spinUp) {   // re-frozen during the spin-up: brake again from the current speed
			spinUp = false;
			brakeT = TAPESTOP_BRAKE_S * (1 - std::pow(std::max(rho, 1e-9), 1.0 / TAPESTOP_EXP));
		}
		if (!A.hard) { rho = 1; brakeT = 0; }
		for (Line* ln : L2) {
			ln->hardAmt.step(ln->hard ? 1 : 0, inc10);
			if (ln->hard && ln->hardAmt.v >= 1) ln->writing = false;
		}
		double tsGain = tapeStopping ? std::min(1.0, 8 * rho) : 1.0;

		// ---- delay times and reader rates ----
		double DA, DB;
		times(f, DA, DB);
		double rA = f.speed, rB = f.speed;
		if (f.reverse == REV_ON) { rA = -rA; rB = -rB; }
		else if (f.reverse == REV_MIX) rB = -rB;

		// ---- lines: speed, readers, read chains ----
		double y[2];
		double fsD = 48000.0 * std::pow(1500.0 / 48000.0, color);
		double bits = 16 - 12 * color, q = std::pow(2.0, bits - 1);
		int N = BBD_STAGES[rangeCur];
		for (int c = 0; c < 2; c++) {
			Line& ln = *L2[c];
			ln.updateSpeed(modelCur, c == 0 ? modA : modB, color);
			bool& fj = c == 0 ? forceJumpA : forceJumpB;
			if (!ln.hard || ln.hardAmt.v < 1) ln.manage(modelCur, c == 0 ? rA : rB, c == 0 ? DA : DB, f.jump, fj);
			double fClk = N * ln.v / ln.L;
			if (ln.fClkS < 0) ln.fClkS = fClk;
			ln.fClkS += aClk * (fClk - ln.fClkS);
			if ((div & 31) == 0) ln.setBbdFilters(std::min({16000.0, 0.45 * fs, ln.fClkS * (0.42 + 0.8 * color * color)}));
			double hardRaw = 0;
			if (ln.hard || ln.hardAmt.v > 0) {
				double gstep = (modelCur == DIGI && color > 0.001 && fsD < fs) ? fs / fsD : 0;
				hardRaw = ln.hardRead((c == 0 ? rA : rB) * rho, gstep) * tsGain;
			}
			auto chain = [&](int md) -> double {
				double grid = 0;
				if (md == BBD && fClk < fs) grid = ln.L / N;
				if (md == DIGI && color > 0.001 && fsD < fs) grid = ln.v / fsD;
				double raw = ln.hardAmt.v < 1 ? ln.readNormal(grid) : 0;
				if (ln.hardAmt.v > 0) raw = raw * (1 - ln.hardAmt.v) + hardRaw * ln.hardAmt.v;
				if (md == BBD) raw = ln.bbdRead(raw);
				else if (md == DIGI && color > 0.001) raw = std::floor(raw / 2.4 * q + 0.5) / q * 2.4;
				return raw;
			};
			double yc = chain(modelCur);
			if (modelFadeN > 0) {
				double wt = 0.5 - 0.5 * std::cos(M_PI * (double)modelFadeN / modelFadeLen);
				yc = (1 - wt) * yc + wt * chain(modelOld);
			}
			y[c] = yc;
			ln.advanceFade();
		}

		// ---- feedback path (spec 6) ----
		double fbs[2], wet[2];
		for (int c = 0; c < 2; c++) {
			Line& ln = *L2[c];
			double t = y[c];
			ln.fbHp += aHp * (t - ln.fbHp);
			t -= hpOn * ln.fbHp;
			ln.fbLp += aLp * (t - ln.fbLp);
			t += lpOn * (ln.fbLp - t);
			fbs[c] = t * (1 - fa) + y[c] * fa;       // filters leave the loop when frozen
			double u = y[c];
			ln.oHp += aHp * (u - ln.oHp);
			u -= hpOn * ln.oHp;
			ln.oLp += aLp * (u - ln.oLp);
			u += lpOn * (ln.oLp - u);
			wet[c] = y[c] * (1 - fa) + u * fa;        // ... and act on the output instead
		}
		if (fogPosCur == POS_LOOP) {
			fog.process(0.5 * (fbs[0] + fbs[1]), decay, revL, revR); fogRan = true;
			fbs[0] = fbs[0] * fogC + 0.7 * revL * fogS;
			fbs[1] = fbs[1] * fogC + 0.7 * revR * fogS;
		}
		double softG = modelCur == TAPE ? TAPE_FREEZE_GAIN : BBD_FREEZE_GAIN;
		double gB = softG;
		if (modelCur == BBD) {   // leakage: longer clock periods droop more, so long frozen loops fade fastest
			double D = A.L / std::max(A.v, 1e-6);
			softG = BBD_FREEZE_GAIN * std::pow(10.0, -BBD_FREEZE_LEAK * D * D / 20.0);
			D = B.L / std::max(B.v, 1e-6);
			gB = BBD_FREEZE_GAIN * std::pow(10.0, -BBD_FREEZE_LEAK * D * D / 20.0);
		}
		else gB = softG;
		double g = sG * (1 - fa) + softG * fa;
		double g2 = sG * (1 - fa) + gB * fa;

		// ---- routing (spec 3) ----
		auto route = [&](int st, int cr, double& inA, double& inB) {
			double mo = 0.5 * (sL + sR);
			if (st == PONG) {
				if (!cr) { inA = mo + g * fbs[1]; inB = fbs[0]; }
				else { inB = mo + g2 * fbs[0]; inA = fbs[1]; }
			}
			else {
				if (!cr) { inA = sL + g * fbs[0]; inB = sR + g2 * fbs[1]; }
				else { inA = sL + g * fbs[1]; inB = sR + g2 * fbs[0]; }
			}
		};
		double inA, inB;
		route(routeStereo, routeCross, inA, inB);
		if (routeFadeN > 0) {
			double oa, ob;
			route(oldStereo, oldCross, oa, ob);
			double wt = 0.5 - 0.5 * std::cos(M_PI * (double)routeFadeN / routeFadeLen);
			inA = (1 - wt) * inA + wt * oa; inB = (1 - wt) * inB + wt * ob;
			routeFadeN--;
		}
		if (modelFadeN > 0) modelFadeN--;

		// ---- write chains ----
		for (int c = 0; c < 2; c++) {
			Line& ln = *L2[c];
			double D = ln.L / std::max(ln.vs2, 1e-6);
			double fc = clampd(8000 * std::pow(0.42 / D, 0.3) * (1 - 0.6 * color), 1500, 12000);
			if (fa > 0.5) fc = std::max(fc, 12000.0);
			ln.write(c == 0 ? inA : inB, modelCur, color, std::min(fc, 0.45 * fs), div);
		}

		// ---- wet side ----
		double tapeW = modelCur == TAPE ? 1.0 : 0.0;
		if (modelFadeN > 0) {
			double wt = 0.5 - 0.5 * std::cos(M_PI * (double)modelFadeN / modelFadeLen);
			tapeW = (1 - wt) * tapeW + wt * (modelOld == TAPE ? 1.0 : 0.0);
		}
		for (int c = 0; c < 2; c++) {
			double bp = L2[c]->bump.process(wet[c]);
			wet[c] += tapeW * (bp - wet[c]);
		}
		if (fogPosCur == POS_DLY) {
			fog.process(0.5 * (wet[0] + wet[1]), decay, revL, revR); fogRan = true;
			wet[0] = wet[0] * fogC + 1.3 * revL * fogS;
			wet[1] = wet[1] * fogC + 1.3 * revR * fogS;
		}
		wet[0] += postG * hissL;
		wet[1] += postG * hissR;
		double dm = 0.5 * (std::fabs(dL) + std::fabs(dR));
		duckEnv += (dm > duckEnv ? aDuckA : aDuckR) * (dm - duckEnv);
		double wg = (1 - clamp01(sDuck) * clamp01(2 * duckEnv)) * clearGain.v;
		wet[0] *= wg; wet[1] *= wg;

		// ---- mix and output ----
		double mx = clamp01(sMix);
		double oL, oR;
		if (f.sendReturn) { oL = wet[0] * mx; oR = wet[1] * mx; }
		else {
			double cm = std::cos(mx * M_PI / 2), sm = std::sin(mx * M_PI / 2);
			oL = dL * cm + wet[0] * sm; oR = dR * cm + wet[1] * sm;
		}
		if (fogPosCur == POS_MIX) {
			fog.process(0.5 * (oL + oR), decay, revL, revR); fogRan = true;
			oL = oL * fogC + 1.3 * revL * fogS;
			oR = oR * fogC + 1.3 * revR * fogS;
		}
		if (!fogRan) fog.process(0, decay, revL, revR);   // keep the tank's clock running
		if (wMix.v > 0) {
			double gm = std::max(sDrive, 0.25);
			oL += wMix.v * (driveShape(oL, gm, clip) - oL);
			oR += wMix.v * (driveShape(oR, gm, clip) - oR);
		}
		oL = limiter(oL); oR = limiter(oR);

		// ---- NaN guard (spec 13) ----
		if (!std::isfinite(oL) || !std::isfinite(oR) || !std::isfinite(y[0]) || !std::isfinite(y[1])
			|| !std::isfinite(revL) || !std::isfinite(A.v) || !std::isfinite(B.v)) {
			resetDSP();
			oL = oR = 0;
		}
		o.l = oL * 5; o.r = oR * 5;

		// drive light
		if (clip) clipHold = 0.05;
		else clipHold = std::max(0.0, clipHold - 1.0 / fs);
		double effG = f.drivePos == POS_MIX ? std::max(sDrive, 0.25) : sDrive;
		bool on = effG >= 0.25;
		o.driveRed = on && clipHold > 0 ? 1 : 0;
		o.driveGreen = on && clipHold <= 0 ? 1 : 0;
		return o;
	}
};

}  // namespace seallegs
