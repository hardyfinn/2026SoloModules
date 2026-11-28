// Greybox DSP: everything that makes the sound, with no Rack dependency so it can
// be rendered and tested offline (test/render.cpp). Spec: vcv-modules/greybox.md
// (with shared sections in vcv-modules/crate.md).
//
// Three models behind one switch:
//   900 / 950  pitch = speed of the DAC clock. Every stored word is played once, in
//              order (no fractional position anywhere), so the staircase images move
//              with pitch and a 36 dB tracking filter decides how much you hear.
//   60         fixed clock, companded 12-bit words, linear interpolation through memory.
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

namespace greybox {

enum Model { M900 = 0, M950 = 1, M60 = 2 };
enum LoopMode { LOOP_OFF = 0, LOOP_FWD = 1, LOOP_ALT = 2 };

// ---- named constants (values marked (ear) in the spec are tunable) -------------
static const double FS_MIN = 7500.0;
static const double FS_MAX[3] = {40000.0, 48000.0, 40000.0};
static const int IN_ORDER[3] = {4, 10, 8};                 // input filter order (ear)
static const double IN_DIV[3] = {2.5, 2.5, 2.22};          // fc_in = fs_rec / div
static const double F_TOP[3] = {16000.0, 20000.0, 18000.0};// FILTER = 99 cutoff (ear)
static const float NOISE_BASE_DB[3] = {-72.f, -78.f, -84.f}; // noise floor at AGE 0 (ear)
static const float AGE_NOISE_DB = 22.f;
static const double MU = 24.0;                             // 60 compander (ear)
static const double FILTER_SPAN_OCT = 7.2;                 // F 99 -> 0 (ear)
static const float ENV_DEPTH = 60.f;                       // FILTER units the ENV knob closes
static const float VCA_DRIVE = 0.7f;                       // OTA VCA (ear)
static const float SC_FEED_DB = -62.f;                     // switched-cap clock whistle (ear)
static const double SC_RATIO = 50.0;                       // switched-cap clock = 50 x cutoff
static const double DROOP_MS = 4.0;                        // hold droop time at AGE 1 (ear)
static const double RATIO_MIN = 1.0 / 16.0, RATIO_MAX = 4.0;

inline double recRate(int model, float rate) {
	rate = std::min(std::max(rate, 0.f), 1.f);
	return FS_MIN * std::pow(FS_MAX[model] / FS_MIN, (double)rate);
}

// ---- small helpers -------------------------------------------------------------

inline float softClip(float x) {
	float a = std::fabs(x);
	if (a < 0.8f) return x;
	float y = 0.8f + 0.2f * std::tanh((a - 0.8f) / 0.2f);
	return x < 0.f ? -y : y;
}

inline uint64_t mix64(uint64_t z) {
	z += 0x9E3779B97F4A7C15ull;
	z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
	z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
	return z ^ (z >> 31);
}

struct Rng {
	uint64_t s = 1;
	void seed(uint64_t v) { s = mix64(v) | 1; }
	uint32_t next() { s ^= s << 13; s ^= s >> 7; s ^= s << 17; return (uint32_t)(s >> 16); }
	float uni() { return (next() & 0xFFFFFF) / 16777216.f; }
	float bi() { return uni() * 2.f - 1.f; }
};

// ---- 12-bit words: linear (900 / 950) or companded (60) ---------------------------

inline int16_t encodeLinear(double y) {
	double c = std::round(y * 2047.0);
	if (c > 2047.0) c = 2047.0;
	if (c < -2048.0) c = -2048.0;
	return (int16_t)c;
}

inline int16_t encodeCompand(double y) {
	y = std::min(std::max(y, -1.0), 1.0);
	y = std::round(y * 32767.0) / 32767.0;                 // the 16-bit ADC
	double c = std::round(2047.0 * std::log1p(MU * std::fabs(y)) / std::log1p(MU));
	if (c > 2047.0) c = 2047.0;
	return (int16_t)(y < 0 ? -c : c);
}

// 4096-entry expander, index code + 2048. Decoded values are rounded to 16 bits.
inline const float* compandTable() {
	static float t[4096];
	static bool init = [] {
		for (int u = 0; u < 4096; u++) {
			int c = std::min(std::max(u - 2048, -2047), 2047);
			double v = (std::pow(1.0 + MU, std::abs(c) / 2047.0) - 1.0) / MU;
			v = std::round(v * 32767.0) / 32767.0;
			t[u] = (float)(c < 0 ? -v : v);
		}
		return true;
	}();
	(void)init;
	return t;
}

inline int16_t encode(double y, bool companded) { return companded ? encodeCompand(y) : encodeLinear(y); }
inline double decode(int16_t c, bool companded) { return companded ? compandTable()[c + 2048] : c / 2048.0; }

// ---- filters ---------------------------------------------------------------------

struct Biquad {
	double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
	void lowpass(double fc, double fs, double q) {
		double w = 2.0 * M_PI * fc / fs;
		double cw = std::cos(w), alpha = std::sin(w) / (2.0 * q);
		double a0 = 1.0 + alpha;
		b0 = (1.0 - cw) / 2.0 / a0; b1 = (1.0 - cw) / a0; b2 = b0;
		a1 = -2.0 * cw / a0; a2 = (1.0 - alpha) / a0;
	}
	double process(double x) {
		double y = b0 * x + z1;
		z1 = b1 * x - a1 * y + z2;
		z2 = b2 * x - a2 * y;
		return y;
	}
	void reset() { z1 = z2 = 0; }
};

// Even-order Butterworth lowpass as cascaded biquads (order 2..10).
struct ButterN {
	Biquad s[5];
	int n = 0;
	void set(int order, double fc, double fs) {
		n = order / 2;
		fc = std::min(fc, 0.45 * fs);
		for (int k = 0; k < n; k++) {
			double q = 1.0 / (2.0 * std::cos(M_PI * (2.0 * k + 1.0) / (2.0 * order)));
			s[k].lowpass(fc, fs, q);
		}
	}
	double process(double x) {
		for (int k = 0; k < n; k++) x = s[k].process(x);
		return x;
	}
	void reset() { for (Biquad& b : s) b.reset(); }
};

// TPT state-variable lowpass section (stable under audio-rate cutoff changes).
struct SvfLP {
	double ic1 = 0, ic2 = 0;
	double process(double x, double g, double k) {
		double a1 = 1.0 / (1.0 + g * (g + k)), a2 = g * a1, a3 = g * a2;
		double v3 = x - ic2;
		double v1 = a1 * ic1 + a2 * v3;
		double v2 = ic2 + a2 * ic1 + a3 * v3;
		ic1 = 2.0 * v1 - ic1;
		ic2 = 2.0 * v2 - ic2;
		return v2;
	}
	void reset() { ic1 = ic2 = 0; }
};

// ---- capture: source audio -> 12-bit memory (spec 3.3, 3.4) ------------------------

struct CaptureSettings {
	int model = M950;
	double fsRec = 48000.0;
	float gain = 1.f;              // linear
	double stretch = 1.0;          // 0.25..4, exactly 1 = bypass
	int dtime = 1000;              // samples
	size_t nMax = 480000;
};

struct CaptureResult {
	std::vector<int16_t> codes;
	double fsRec = 48000.0;
	bool companded = false;
	bool clipped = false;
	bool full = false;             // hit the memory cap
};

// Cyclic "harmoniser" time-stretch on decoded words (spec 3.4). Not a phase vocoder.
inline std::vector<int16_t> stretchCodes(const std::vector<int16_t>& codes, bool comp, double s, int D, size_t nMax, bool& full) {
	size_t N = codes.size();
	size_t nOut = (size_t)std::llround((double)N * s);
	if (nOut > nMax) { nOut = nMax; full = true; }
	D = std::max(D, 2);
	int X = std::max(8, D / 8);
	auto in = [&](long a) -> double { return (a >= 0 && (size_t)a < N) ? decode(codes[a], comp) : 0.0; };
	std::vector<int16_t> out(nOut);
	for (size_t n = 0; n < nOut; n++) {
		long g = (long)(n / D);
		long r = (long)n - g * D;
		long a = (long)std::llround(g * (double)D / s) + r;
		double y;
		if (g > 0 && r < X) {
			long b = (long)std::llround((g - 1) * (double)D / s) + D + r;
			double w = (double)r / X;
			y = (1.0 - w) * in(b) + w * in(a);
			out[n] = encode(y, comp);
		}
		else {
			out[n] = (a >= 0 && (size_t)a < N) ? codes[a] : encode(0.0, comp);   // exact word, no re-rounding
		}
	}
	return out;
}

inline CaptureResult capture(const std::vector<float>& src, double fsSrc, const CaptureSettings& cs) {
	CaptureResult r;
	r.fsRec = cs.fsRec;
	r.companded = cs.model == M60;
	size_t L = src.size();
	if (L < 4 || fsSrc <= 0) return r;

	// x4 polyphase upsampler, 64-tap Blackman-windowed sinc (as Crate)
	const int PH = 4, TAPS = 16, NT = PH * TAPS;
	double h[NT];
	const double D = (NT - 1) / 2.0;
	for (int t = 0; t < NT; t++) {
		double x = (t - D) / PH * 0.9;
		double s = std::fabs(x) < 1e-12 ? 1.0 : std::sin(M_PI * x) / (M_PI * x);
		double w = 0.42 - 0.5 * std::cos(2 * M_PI * t / (NT - 1)) + 0.08 * std::cos(4 * M_PI * t / (NT - 1));
		h[t] = s * w;
	}
	for (int p = 0; p < PH; p++) {
		double sum = 0;
		for (int j = 0; j < TAPS; j++) sum += h[p + PH * j];
		for (int j = 0; j < TAPS; j++) h[p + PH * j] /= sum;
	}

	const double fs4 = fsSrc * PH;
	ButterN lp;
	double fcIn = std::min(cs.fsRec / IN_DIV[cs.model], 0.45 * fsSrc);
	lp.set(IN_ORDER[cs.model], fcIn, fs4);

	const double step = fs4 / cs.fsRec;
	const size_t L4 = L * PH;
	double avail = (double)L4 - D - 3.0;
	if (avail <= 0) return r;
	// Without stretch the cap applies now; with stretch, keep everything and cap after.
	size_t N = (size_t)std::floor(avail / step);
	bool stretching = cs.stretch != 1.0;
	if (!stretching && N > cs.nMax) { N = cs.nMax; r.full = true; }
	r.codes.reserve(N);

	double ring[4] = {0, 0, 0, 0};
	double p = D;
	for (size_t n = 0; n < L4 && r.codes.size() < N; n++) {
		int ph = (int)(n % PH);
		long base = (long)(n / PH);
		double u = 0;
		for (int j = 0; j < TAPS; j++) {
			long idx = base - j;
			if (idx >= 0 && idx < (long)L) u += h[ph + PH * j] * src[idx];
		}
		double g = cs.gain * u;
		if (std::fabs(g) > 0.8) r.clipped = true;
		ring[n & 3] = lp.process(softClip((float)g));

		while (r.codes.size() < N) {
			size_t i = (size_t)std::floor(p);
			if (n < i + 2) break;
			double t = p - (double)i;
			double xm1 = ring[(i - 1) & 3], x0 = ring[i & 3], x1 = ring[(i + 1) & 3], x2 = ring[(i + 2) & 3];
			double c1 = 0.5 * (x1 - xm1);
			double c2 = xm1 - 2.5 * x0 + 2.0 * x1 - 0.5 * x2;
			double c3 = 0.5 * (x2 - xm1) + 1.5 * (x0 - x1);
			double v = ((c3 * t + c2) * t + c1) * t + x0;
			r.codes.push_back(encode(v, r.companded));
			p += step;
		}
	}
	if (stretching) r.codes = stretchCodes(r.codes, r.companded, cs.stretch, cs.dtime, cs.nMax, r.full);
	return r;
}

// ---- half-band decimator (2 -> 1), Kaiser windowed ----------------------------------

template <int NT>
struct HalfBand {
	float h[NT];
	float buf[NT * 2] = {};
	int w = 0;
	HalfBand() {
		auto i0 = [](double x) {
			double s = 1, t = 1;
			for (int k = 1; k < 30; k++) { t *= (x / (2 * k)) * (x / (2 * k)); s += t; }
			return s;
		};
		const double beta = 8.0, c = (NT - 1) / 2.0;
		double sum = 0;
		for (int n = 0; n < NT; n++) {
			double x = (n - c) / 2.0;
			double s = std::fabs(x) < 1e-12 ? 1.0 : std::sin(M_PI * x) / (M_PI * x);
			double r = (n - c) / c;
			h[n] = (float)(0.5 * s * i0(beta * std::sqrt(std::max(0.0, 1 - r * r))) / i0(beta));
			sum += h[n];
		}
		for (int n = 0; n < NT; n++) h[n] = (float)(h[n] / sum);
	}
	void push(float x) { buf[w] = x; buf[w + NT] = x; w = (w + 1) % NT; }
	float out() const {
		float y = 0;
		for (int n = 0; n < NT; n++) y += h[n] * buf[w + n];
		return y;
	}
	void reset() { std::fill(buf, buf + NT * 2, 0.f); }
};

// ---- the voice (spec 5-8, 10) ---------------------------------------------------------

struct Frame {
	bool trig = false;
	int model = M950;
	float semis = 0.f;          // TUNE + mutation + V/OCT*12
	double fsRecLive = 48000;   // fs_rec from RATE + MODEL now (THRU clock)
	float vel = 1.f;
	float decay = 1.f;
	float start = 0.f, end = 1.f;
	int loop = LOOP_OFF;
	bool reverse = false;       // mutation: this trigger plays backwards
	float filter = 99.f;        // FILTER + filtMut + 10*CV, before ENV
	float envAmt = 0.f;
	bool follow = true;
	float level = 0.8f;
	float age = 0.f;
	bool thru = false;
	bool trigConnected = false;
	float in = 0.f;             // volts
	float gain = 1.f;           // linear
	bool noNoise = false;       // tests only
};

struct Result {
	float out = 0.f;
	bool end = false;
	bool inClip = false;
};

struct Voice {
	const int16_t* mem = nullptr;
	size_t N = 0;
	double fsRecMem = 48000;
	bool memComp = false;

	double fsE = 48000, fsI = 192000;
	int os = 4;
	uint32_t seed = 1;

	// playback
	double clk = 0, pos = 0;
	long idx = 0;
	int dir = 1;
	bool playing = false;
	long startIdx = 0, endIdx = 0;
	float vHeld = 0.f, prevOut = 0.f;
	int16_t lastCode = 0;
	float lastV = 0.f;

	// envelope
	enum Stage { OFF, ATTACK, DECAY, RELEASE };
	Stage stage = OFF;
	float env = 0.f, vel = 1.f, eSmooth = 0.f, thruGate = 0.f, playSmooth = 0.f;

	// thru
	float lastIn = 0.f;
	double thruPrev = 0, thruCur = 0;
	ButterN thruLP;
	int thruModel = -1;
	double thruFc = -1;

	// switched-capacitor stage
	double scClk = 0, scPh = 0;
	float scHeld = 0.f, scPrevOut = 0.f, scInPrev = 0.f, sigDelay = 0.f;

	// voice filter
	SvfLP svf[3];
	double fcSm = 20000;

	float dcX = 0.f, dcY = 0.f, dcR = 0.999f;
	HalfBand<31> dec1;
	HalfBand<63> dec2;
	float dec1Out = 0.f;

	// AGE
	float dacTable[4096];
	float tableAge = -1.f;
	float ek[12];
	float w1 = 0, w1t = 0, w2 = 0, w2t = 0;
	int walkCount = 0;
	Rng rng;
	int block = 0;
	float drift = 0.f, ageScale = 1.f;

	Voice() {
		setSeed(1);
		setSampleRate(48000);
	}

	void setSeed(uint32_t s) {
		seed = s;
		Rng r; r.seed(s);
		for (int b = 0; b < 12; b++) ek[b] = r.bi() * (b <= 7 ? 0.02f : 0.006f);
		rng.seed(s ^ 0xA5A5A5A5u);
		tableAge = -1.f;
	}

	void setSampleRate(double fs) {
		fsE = fs;
		os = fs < 50000 ? 4 : (fs < 100000 ? 2 : 1);
		fsI = fs * os;
		thruModel = -1;
		thruLP.reset();
		for (SvfLP& s : svf) s.reset();
		dcR = 1.f - (float)(2 * M_PI * 5.0 / fsI);
		dcX = dcY = 0;
		dec1.reset(); dec2.reset();
		vHeld = prevOut = 0;
		scHeld = scPrevOut = scInPrev = sigDelay = 0;
		clk = scClk = scPh = 0;
	}

	void setMemory(const int16_t* m, size_t n, double fsRec, bool companded) {
		mem = m; N = n; fsRecMem = fsRec; memComp = companded;
		playing = false; stage = OFF; env = 0;
		idx = 0; pos = 0; dir = 1;
	}

	bool active() const { return playing || stage != OFF; }

	void fadeOut() {
		if (playing) playing = false;
		if (stage != OFF) stage = RELEASE;
	}

	void buildTable(float a) {
		auto S = [&](int u) {
			double s = 0;
			for (int b = 0; b < 12; b++) if (u & (1 << b)) s += (double)(1 << b) * (1.0 + a * ek[b]);
			return s;
		};
		double off = S(2048) / 2048.0 - 1.0;
		for (int u = 0; u < 4096; u++) dacTable[u] = (float)(S(u) / 2048.0 - 1.0 - off);
		tableAge = a;
	}

	// 16-bit DAC for the 60: the top 12 bits go through the (possibly worn) weights.
	float dac16(float v) const {
		long q = std::lround((double)v * 32768.0);
		q = std::min(std::max(q, -32768L), 32767L);
		long top = q >> 4;                        // arithmetic: floor
		long low = q - top * 16;
		return dacTable[top + 2048] + (float)low / 32768.f;
	}

	int16_t readMem(long i, float a) const {
		int16_t c = mem[i];
		if (a > 0.6f) {
			uint64_t hsh = mix64(((uint64_t)seed << 32) ^ (uint64_t)i);
			float prob = 0.004f * (a - 0.6f) / 0.4f;
			if ((hsh & 0xFFFF) / 65536.f < prob) {
				int bit = (int)((hsh >> 16) % 6);
				int u = (c + 2048) ^ (1 << bit);
				c = (int16_t)(u - 2048);
			}
		}
		return c;
	}

	long idxOf(float frac) const {
		frac = std::min(std::max(frac, 0.f), 1.f);
		return std::min((long)(frac * (double)N), (long)N);
	}

	void computeRange(const Frame& f, bool newStart) {
		if (newStart) {
			long s = idxOf(f.start);
			if (s + 16 > (long)N) s = (long)N - 16;
			startIdx = std::max(0L, s);
		}
		endIdx = idxOf(f.end);
		if (endIdx < startIdx + 16) endIdx = std::min((long)N, startIdx + 16);
	}

	void controlBlock(const Frame& f) {
		float a = f.age;
		if (std::fabs(a - tableAge) > 0.002f || (a == 0.f && tableAge != 0.f)) buildTable(a);
		double blockDt = 32.0 / fsI;
		if (--walkCount <= 0) {
			walkCount = (int)(fsI / 32.0 / 0.3);
			w1t = rng.bi(); w2t = rng.bi();
		}
		float c = (float)(1.0 - std::exp(-2 * M_PI * 0.15 * blockDt));
		w1 += (w1t - w1) * c;
		w2 += (w2t - w2) * c;
		drift = a * (-0.006f + 0.005f * w1);
		ageScale = std::pow(2.f, -0.5f * a) * (1.f + 0.06f * a * w2);
		if (a == 0.f) { drift = 0.f; ageScale = 1.f; }
	}

	void stop(Result& res) {
		playing = false;
		stage = RELEASE;
		res.end = true;
	}

	// 900 / 950: whole words only, idx is an integer (spec 5.2, 5.3).
	void advanceInt(const Frame& f, Result& res) {
		idx += dir;
		if (dir > 0 && idx >= endIdx) {
			if (f.loop == LOOP_OFF) stop(res);
			else if (f.loop == LOOP_FWD) { computeRange(f, true); idx = startIdx; }
			else { dir = -1; idx = std::max(endIdx - 2, startIdx); }
		}
		else if (dir < 0 && idx < startIdx) {
			if (f.loop == LOOP_OFF) stop(res);
			else if (f.loop == LOOP_FWD) idx = endIdx - 1;
			else { computeRange(f, true); dir = 1; idx = std::min(startIdx + 1, endIdx - 1); }
		}
		else if (dir < 0 && idx >= endIdx) idx = endIdx - 1;   // END moved below us
	}

	// 60: fractional step through memory, keeping the fraction on wraps (spec 5.4).
	void advanceFrac(const Frame& f, double ratio, Result& res) {
		pos += dir * ratio;
		double span = (double)(endIdx - startIdx);
		if (dir > 0 && pos >= (double)endIdx) {
			if (f.loop == LOOP_OFF) stop(res);
			else if (f.loop == LOOP_FWD) { computeRange(f, true); pos -= span; }
			else { dir = -1; pos = 2.0 * (endIdx - 1) - pos; }
		}
		else if (dir < 0 && pos < (double)startIdx) {
			if (f.loop == LOOP_OFF) stop(res);
			else if (f.loop == LOOP_FWD) pos += span;
			else { computeRange(f, true); dir = 1; pos = 2.0 * startIdx - pos; }
		}
		if (playing) pos = std::min(std::max(pos, (double)startIdx), (double)endIdx - 1e-6);
	}

	Result process(const Frame& f) {
		Result res;
		const int model = std::min(std::max(f.model, 0), 2);
		const bool clockPitch = model != M60;
		double ratio = std::pow(2.0, f.semis / 12.0);
		ratio = std::min(std::max(ratio, RATIO_MIN), RATIO_MAX);

		if (f.trig) {
			if (N >= 16) {
				computeRange(f, true);
				if (f.reverse) { dir = -1; idx = endIdx - 1; }
				else { dir = 1; idx = startIdx; }
				pos = (double)idx;
				playing = true;
				stage = ATTACK; env = 0.f; vel = f.vel;
			}
			else if (f.thru) {
				stage = ATTACK; env = 0.f; vel = f.vel;
			}
		}
		if (N >= 16) computeRange(f, false);

		// THRU input filter follows MODEL and RATE
		double fcThru = std::min(f.fsRecLive / IN_DIV[model], 0.45 * fsE);
		if (model != thruModel || std::fabs(fcThru - thruFc) > 0.002 * thruFc) {
			thruLP.set(IN_ORDER[model], fcThru, fsI);
			if (model != thruModel) thruLP.reset();
			thruModel = model; thruFc = fcThru;
		}

		const float atkInc = (float)(1.0 / (0.0003 * fsI));
		const float relInc = (float)(1.0 / (0.001 * fsI));
		float decCoef = 1.f;
		if (f.decay < 0.98f) {
			double tau = 0.02 * std::pow(500.0, f.decay / 0.98);
			decCoef = (float)std::exp(-1.0 / (tau * fsI));
		}
		const float eCoef = (float)(1.0 - std::exp(-1.0 / (0.002 * fsI)));
		const float gCoef = (float)(1.0 - std::exp(-1.0 / (0.001 * fsI)));
		const double fcCoef = 1.0 - std::exp(-1.0 / (0.001 * fsI));
		float noiseAmp = f.noNoise ? 0.f
			: std::pow(10.f, (NOISE_BASE_DB[model] + AGE_NOISE_DB * f.age) / 20.f) * (float)std::sqrt(fsI / 40000.0) * 1.7320508f;
		const float droop = (clockPitch && f.age >= 0.05f) ? (float)std::exp(-1.0 / (DROOP_MS * 0.001 / f.age * fsI)) : 1.f;
		const float scFeed = std::pow(10.f, SC_FEED_DB / 20.f);
		const double qk[3] = {1.0 / 0.5176, 1.0 / 0.7071, 1.0 / 1.9319};

		float inNow = std::isfinite(f.in) ? f.in : 0.f;
		float outE = 0.f;

		for (int i = 0; i < os; i++) {
			if (block-- <= 0) { controlBlock(f); block = 31; }

			// THRU path: input brought up to the internal rate, clipped, input filter
			float frac = (float)(i + 1) / os;
			float inI = lastIn + (inNow - lastIn) * frac;
			float xg = inI / 5.f * f.gain;
			if (std::fabs(xg) > 0.8f) res.inClip = true;
			thruPrev = thruCur;
			thruCur = thruLP.process(softClip(xg));

			// envelope
			switch (stage) {
				case ATTACK: env += atkInc; if (env >= 1.f) { env = 1.f; stage = DECAY; } break;
				case DECAY:
					env *= decCoef;
					if (env < 3.2e-5f) { env = 0.f; stage = OFF; playing = false; }
					break;
				case RELEASE: env -= relInc; if (env <= 0.f) { env = 0.f; stage = OFF; } break;
				default: break;
			}
			float gateT = (f.thru && !f.trigConnected && !playing) ? 1.f : 0.f;
			thruGate += (gateT - thruGate) * gCoef;
			float e = std::max(env, thruGate);
			float velNow = stage != OFF ? vel : 1.f;
			playSmooth += ((active() || thruGate > 1e-4f ? 1.f : 0.f) - playSmooth) * gCoef;

			// DAC clock. 900/950: the clock IS the pitch. 60: fixed clock.
			double fsBase = playing || (stage == RELEASE && N > 0) ? fsRecMem : f.fsRecLive;
			double fv = fsBase * (1.0 + drift);
			if (clockPitch && playing) fv *= ratio;
			fv = std::min(fv, fsI);
			double inc = fv / fsI;

			if (droop < 1.f) vHeld *= droop;           // AGE: hold capacitor leaks between ticks

			clk += inc;
			float cNow = 0.f, cPrev = 0.f;
			if (clk >= 1.0) {
				clk -= 1.0;
				if (clk >= 1.0) clk = 0.0;
				double d = clk / inc;
				float vNew = 0.f;
				if (clockPitch) {
					int16_t code = 0;
					if (playing) {
						code = readMem(idx, f.age);     // whole words only
						lastCode = code;
						advanceInt(f, res);
					}
					else if (f.thru) code = encodeLinear(thruCur * (1.0 - d) + thruPrev * d);
					else if (stage == RELEASE) code = lastCode;
					vNew = dacTable[code + 2048];
				}
				else {
					float v = 0.f;
					float amp = e * velNow;
					if (playing) {
						long i0 = (long)std::floor(pos);
						long i1 = std::min(i0 + 1, (long)N - 1);
						float fr = (float)(pos - (double)i0);
						float a0 = (float)decode(readMem(i0, f.age), memComp);
						float a1 = (float)decode(readMem(i1, f.age), memComp);
						v = a0 + (a1 - a0) * fr;            // linear interpolation on decoded words
						lastV = v;
						advanceFrac(f, ratio, res);
						v *= amp;
					}
					else if (f.thru) v = (float)decode(encodeCompand(thruCur * (1.0 - d) + thruPrev * d), true) * amp;
					else if (stage == RELEASE) v = lastV * amp;
					vNew = dac16(v);
				}
				float h = vNew - vHeld;
				float dd = (float)d;
				cNow = -h * (1.f - dd) * (1.f - dd) * 0.5f;
				cPrev = h * dd * dd * 0.5f;
				vHeld = vNew;
			}
			float stair = vHeld + cNow;
			prevOut += cPrev;
			float sig = prevOut;                       // one-sample delay for the BLEP residual
			prevOut = stair;

			if (noiseAmp > 0.f) sig += noiseAmp * rng.bi() * (clockPitch ? 1.f : playSmooth);

			// voice filter cutoff (spec 7.1)
			eSmooth += (e - eSmooth) * eCoef;
			float F = f.filter - ENV_DEPTH * f.envAmt * (1.f - eSmooth);
			F = std::min(std::max(F, 0.f), 99.f);
			double fc = F_TOP[model] * std::exp2(-(99.0 - F) * FILTER_SPAN_OCT / 99.0);
			if (f.follow && clockPitch && playing) fc *= ratio;
			fc *= ageScale;
			fc = std::min(std::max(fc, 40.0), 0.45 * fsI);
			fcSm += (fc - fcSm) * fcCoef;

			// switched-capacitor input sampling (900 / 950)
			double fsc = SC_RATIO * fcSm;
			float x = sig;
			if (clockPitch) {
				float wSC = (float)std::min(std::max((0.5 * fsI - fsc) / (0.1 * fsI), 0.0), 1.0);
				float dly = sigDelay; sigDelay = sig;
				if (wSC > 0.f) {
					double sinc = fsc / fsI;
					scClk += sinc;
					float sNow = 0.f, sPrev = 0.f;
					if (scClk >= 1.0) {
						scClk -= 1.0;
						if (scClk >= 1.0) scClk = 0.0;
						float d = (float)(scClk / sinc);
						float vN = sig * (1.f - d) + scInPrev * d;
						float h = vN - scHeld;
						sNow = -h * (1.f - d) * (1.f - d) * 0.5f;
						sPrev = h * d * d * 0.5f;
						scHeld = vN;
					}
					float st = scHeld + sNow;
					scPrevOut += sPrev;
					float held = scPrevOut;
					scPrevOut = st;
					x = dly + (held - dly) * wSC;
				}
				else {
					scHeld = scPrevOut = sig;
					scClk = 0;
					x = dly;
				}
				scInPrev = sig;
			}

			// 6th-order Butterworth: three TPT sections
			double g = std::tan(M_PI * fcSm / fsI);
			double y = x;
			for (int s = 0; s < 3; s++) y = svf[s].process(y, g, qk[s]);

			// switched-capacitor clock feedthrough (whistle at 50 x cutoff)
			if (clockPitch && fsc < 20000.0) {
				float lvl = scFeed * (float)std::min((20000.0 - fsc) / 4000.0, 1.0);
				double dt = fsc / fsI;
				scPh += dt;
				if (scPh >= 1.0) scPh -= 1.0;
				auto blep = [&](double t) {
					if (t < dt) { t /= dt; return t + t - t * t - 1.0; }
					if (t > 1.0 - dt) { t = (t - 1.0) / dt; return t * t + t + t + 1.0; }
					return 0.0;
				};
				double sq = scPh < 0.5 ? 1.0 : -1.0;
				sq += blep(scPh);
				double t2 = scPh + 0.5; if (t2 >= 1.0) t2 -= 1.0;
				sq -= blep(t2);
				y += lvl * sq;
			}

			// VCA
			float yo;
			if (clockPitch) {
				float amp = e * f.level * velNow;
				yo = std::tanh(VCA_DRIVE * (float)y) / VCA_DRIVE * amp;
			}
			else yo = (float)y * f.level;

			float dcOut = yo - dcX + dcR * dcY;
			dcX = yo; dcY = dcOut;

			if (os == 4) {
				dec1.push(dcOut);
				if (i & 1) { dec2.push(dec1.out()); if (i == 3) outE = dec2.out(); }
			}
			else if (os == 2) {
				dec2.push(dcOut);
				if (i == 1) outE = dec2.out();
			}
			else outE = dcOut;
		}
		lastIn = inNow;
		if (!std::isfinite(outE)) {
			for (SvfLP& s : svf) s.reset();
			dec1.reset(); dec2.reset();
			dcX = dcY = 0; outE = 0.f;
		}
		res.out = outE * 5.f;
		return res;
	}
};

// ---- WAV I/O (mono-summed float) ------------------------------------------------

inline bool readWav(const char* path, std::vector<float>& out, double& fs, double maxSec) {
	FILE* fp = std::fopen(path, "rb");
	if (!fp) return false;
	std::vector<uint8_t> d;
	std::fseek(fp, 0, SEEK_END);
	long len = std::ftell(fp);
	std::fseek(fp, 0, SEEK_SET);
	if (len < 44 || len > 1024L * 1024 * 1024) { std::fclose(fp); return false; }
	d.resize(len);
	size_t got = std::fread(d.data(), 1, len, fp);
	std::fclose(fp);
	if ((long)got != len || std::memcmp(d.data(), "RIFF", 4) || std::memcmp(d.data() + 8, "WAVE", 4)) return false;
	auto u16 = [&](size_t o) { return (uint32_t)d[o] | ((uint32_t)d[o + 1] << 8); };
	auto u32 = [&](size_t o) { return u16(o) | (u16(o + 2) << 16); };
	int fmt = 0, ch = 0, bits = 0;
	uint32_t rate = 0;
	size_t dataOff = 0, dataLen = 0;
	size_t o = 12;
	while (o + 8 <= d.size()) {
		uint32_t sz = u32(o + 4);
		if (!std::memcmp(&d[o], "fmt ", 4) && o + 8 + 16 <= d.size()) {
			fmt = (int)u16(o + 8); ch = (int)u16(o + 10); rate = u32(o + 12); bits = (int)u16(o + 22);
			if (fmt == 0xFFFE && sz >= 40 && o + 8 + 26 <= d.size()) fmt = (int)u16(o + 8 + 24);
		}
		else if (!std::memcmp(&d[o], "data", 4)) {
			dataOff = o + 8;
			dataLen = std::min((size_t)sz, d.size() - dataOff);
			break;
		}
		o += 8 + sz + (sz & 1);
	}
	if (!dataOff || ch < 1 || rate < 1000 || !(fmt == 1 || fmt == 3)) return false;
	int bps = bits / 8;
	if (bps < 1 || bps > 8) return false;
	size_t frames = dataLen / (bps * ch);
	frames = std::min(frames, (size_t)(maxSec * rate));
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
					for (int b = 0; b < std::min(bps, 4); b++) x |= (int32_t)s[bps - 1 - b] << (24 - 8 * b);
					v = x / 2147483648.0;
				}
			}
			else if (bps == 4) { float x; std::memcpy(&x, s, 4); v = x; }
			else if (bps == 8) { double x; std::memcpy(&x, s, 8); v = x; }
			sum += std::isfinite(v) ? v : 0.0;
		}
		out[i] = (float)(sum / ch);
	}
	fs = rate;
	return true;
}

inline bool writeWavFloat(const char* path, const std::vector<float>& x, double fs) {
	FILE* fp = std::fopen(path, "wb");
	if (!fp) return false;
	auto w32 = [&](uint32_t v) { uint8_t b[4] = {(uint8_t)v, (uint8_t)(v >> 8), (uint8_t)(v >> 16), (uint8_t)(v >> 24)}; std::fwrite(b, 1, 4, fp); };
	auto w16 = [&](uint16_t v) { uint8_t b[2] = {(uint8_t)v, (uint8_t)(v >> 8)}; std::fwrite(b, 1, 2, fp); };
	uint32_t dataLen = (uint32_t)(x.size() * 4);
	std::fwrite("RIFF", 1, 4, fp); w32(36 + dataLen); std::fwrite("WAVE", 1, 4, fp);
	std::fwrite("fmt ", 1, 4, fp); w32(16); w16(3); w16(1); w32((uint32_t)fs); w32((uint32_t)fs * 4); w16(4); w16(32);
	std::fwrite("data", 1, 4, fp); w32(dataLen);
	std::fwrite(x.data(), 4, x.size(), fp);
	bool ok = !std::ferror(fp);
	std::fclose(fp);
	return ok;
}

} // namespace greybox
