// Deidre engine: 8 clocked modulation outputs, no Rack code here so it can be tested offline.
// Everything is a pure function of (beat position, output settings, random seed), so a
// pattern with LOOP set repeats exactly, and random choices only change when reseeded.
#pragma once
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>

namespace deidre {

enum { NUM_OUT = 8, NUM_CV = 6, SCOPE_N = 160, NUM_BANKS = 64, NUM_USER = 3 };

// ---- per-output parameters (the 23 controls that follow the selected output)
enum P {
	P_RATE, P_PHASE, P_PROB, P_FLEX, P_AMOUNT,
	P_SHAPE, P_WIDTH, P_SLEW, P_LEVEL, P_OFFSET,
	P_STEPS, P_TRIGS, P_PAD, P_RAND, P_ESHIFT,
	P_LOOP, P_NAP, P_WAKE, P_LSHIFT,
	P_CROSS, P_SOURCE, P_SCALE, P_INVERT,
	NUM_P
};

struct PSpec {
	const char* name;   // short, for the screen
	const char* longName;
	float min, max, def;
	bool stepped;
	int unit;           // 0 plain number, 1 percent, 2 degrees, 3 list (rate/shape/...), 4 beats
};

// ---- lists
inline int rateCount() { return 31; }
inline const char* rateName(int i) {
	static const char* n[31] = {"OFF", "GATE", "START", "STOP",
		"/64", "/48", "/32", "/24", "/16", "/12", "/8", "/7", "/6", "/5", "/4", "/3", "/2", "/1.5",
		"x1", "x1.5", "x2", "x3", "x4", "x5", "x6", "x7", "x8", "x12", "x16", "x24", "x32"};
	return n[i < 0 ? 0 : i > 30 ? 30 : i];
}
// steps per beat; negative = special (OFF -1, GATE -2, START -3, STOP -4)
inline double rateValue(int i) {
	static const double v[31] = {-1, -2, -3, -4,
		1.0 / 64, 1.0 / 48, 1.0 / 32, 1.0 / 24, 1.0 / 16, 1.0 / 12, 1.0 / 8, 1.0 / 7, 1.0 / 6, 1.0 / 5, 1.0 / 4, 1.0 / 3, 1.0 / 2, 2.0 / 3,
		1, 1.5, 2, 3, 4, 5, 6, 7, 8, 12, 16, 24, 32};
	return v[i < 0 ? 0 : i > 30 ? 30 : i];
}
enum { RATE_X1 = 18 };

enum Shape { SH_PULSE, SH_RATCHET2, SH_RATCHET4, SH_SMOOTH_PULSE, SH_TRIANGLE, SH_TRAPEZOID, SH_SINE, SH_HUMP,
	SH_EXP, SH_SMOOTH_EXP, SH_LOG, SH_SMOOTH_LOG, SH_RANDOM, SH_SMOOTH_RANDOM, NUM_SHAPES };
inline const char* shapeName(int i) {
	static const char* n[NUM_SHAPES] = {"PULSE", "RATCHET x2", "RATCHET x4", "SMOOTH PULSE", "TRIANGLE", "TRAPEZOID",
		"SINE", "HUMP", "EXP ENV", "SMOOTH EXP", "LOG ENV", "SMOOTH LOG", "RANDOM", "SMOOTH RANDOM"};
	return n[i < 0 ? 0 : i >= NUM_SHAPES ? NUM_SHAPES - 1 : i];
}
inline bool isRandomShape(int s) { return s == SH_RANDOM || s == SH_SMOOTH_RANDOM; }

enum Flex { FX_OFF, FX_HUMAN, FX_SWING, FX_RAMP_UP, FX_RAMP_DOWN, FX_HUMP, FX_DELAY, FX_PWR2, NUM_FLEX };
inline const char* flexName(int i) {
	static const char* n[NUM_FLEX] = {"OFF", "HUMAN", "SWING", "RAMP UP", "RAMP DOWN", "HUMP", "DELAY", "PWR2"};
	return n[i < 0 ? 0 : i >= NUM_FLEX ? NUM_FLEX - 1 : i];
}

enum Cross { CR_NONE, CR_MIX, CR_MULT, CR_ADD, CR_SUB, CR_MIN, CR_MAX, CR_HOLD, CR_SH, CR_MASK, CR_NOT,
	CR_OR, CR_XOR, CR_AND, CR_BITOR, CR_BITXOR, CR_BITAND, CR_SEED, CR_LATCH, NUM_CROSS };
inline const char* crossName(int i) {
	static const char* n[NUM_CROSS] = {"NONE", "MIX", "MULT", "ADD", "SUB", "MIN", "MAX", "HOLD", "S&H", "MASK", "NOT",
		"OR", "XOR", "AND", "BitOR", "BitXOR", "BitAND", "SEED", "LATCH"};
	return n[i < 0 ? 0 : i >= NUM_CROSS ? NUM_CROSS - 1 : i];
}
enum { NUM_SOURCES = 12 };  // OUT 1-8, CV 1-4
inline std::string sourceName(int i) {
	if (i < 8) return "OUT " + std::to_string(i + 1);
	return "CV " + std::to_string(i - 7);
}

enum { NUM_SCALES = 22, SC_USER1 = 19 };
inline const char* scaleName(int i) {
	static const char* n[NUM_SCALES] = {"OFF", "CHROMATIC", "MAJOR", "MINOR", "DORIAN", "PHRYGIAN", "LYDIAN", "MIXOLYDIAN",
		"LOCRIAN", "HARM MINOR", "MELOD MINOR", "MAJ PENTA", "MIN PENTA", "BLUES", "WHOLE TONE", "OCTAVES", "FIFTHS",
		"MAJ TRIAD", "MIN TRIAD", "USER 1", "USER 2", "USER 3"};
	return n[i < 0 ? 0 : i >= NUM_SCALES ? NUM_SCALES - 1 : i];
}
// bit k = semitone k above C
inline uint16_t scaleMask(int i) {
	static const uint16_t m[SC_USER1] = {0, 0xFFF,
		0xAB5, 0x5AD, 0x6AD, 0x5AB, 0xAD5, 0x6B5, 0x56B, 0x9AD, 0xAAD, 0x295, 0x4A9, 0x4E9, 0x555, 0x001, 0x081, 0x091, 0x089};
	return (i >= 0 && i < SC_USER1) ? m[i] : 0;
}

inline const PSpec& pspec(int i) {
	static PSpec s[NUM_P];
	static bool init = false;
	if (!init) {
		init = true;
		auto set = [](PSpec& p, const char* n, const char* ln, float mn, float mx, float df, bool st, int u) {
			p.name = n; p.longName = ln; p.min = mn; p.max = mx; p.def = df; p.stepped = st; p.unit = u;
		};
		set(s[P_RATE], "RATE", "Rate (multiply / divide the beat)", 0, rateCount() - 1, RATE_X1, true, 3);
		set(s[P_PHASE], "PHASE", "Phase", 0, 1, 0, false, 2);
		set(s[P_PROB], "PROB", "Probability", 0, 1, 1, false, 1);
		set(s[P_FLEX], "FLEX", "Flex (timing)", 0, NUM_FLEX - 1, 0, true, 3);
		set(s[P_AMOUNT], "AMOUNT", "Flex amount", 0, 1, 0.3f, false, 1);
		set(s[P_SHAPE], "SHAPE", "Shape", 0, NUM_SHAPES - 1, 0, true, 3);
		set(s[P_WIDTH], "WIDTH", "Width", 0, 1, 0.5f, false, 1);
		set(s[P_SLEW], "SLEW", "Slew", 0, 1, 0, false, 1);
		set(s[P_LEVEL], "LEVEL", "Level", 0, 1, 1, false, 1);
		set(s[P_OFFSET], "OFFSET", "Offset", 0, 1, 0, false, 1);
		set(s[P_STEPS], "STEPS", "Euclid steps (0 = off)", 0, 64, 0, true, 0);
		set(s[P_TRIGS], "TRIGS", "Euclid triggers", 0, 64, 0, true, 0);
		set(s[P_PAD], "PAD", "Euclid pad (empty steps at the end)", 0, 32, 0, true, 0);
		set(s[P_RAND], "RAND", "Euclid extra random triggers", 0, 1, 0, false, 1);
		set(s[P_ESHIFT], "E.SHIFT", "Euclid shift (rotate)", 0, 63, 0, true, 0);
		set(s[P_LOOP], "LOOP", "Loop length in beats (0 = off)", 0, 64, 0, true, 4);
		set(s[P_NAP], "NAP", "Loops to sleep (0 = never)", 0, 16, 0, true, 0);
		set(s[P_WAKE], "WAKE", "Loops to play before napping", 1, 16, 1, true, 0);
		set(s[P_LSHIFT], "L.SHIFT", "Loop shift (offsets the nap/wake cycle)", 0, 15, 0, true, 0);
		set(s[P_CROSS], "CROSS", "Cross operation", 0, NUM_CROSS - 1, 0, true, 3);
		set(s[P_SOURCE], "SOURCE", "Cross source", 0, NUM_SOURCES - 1, 0, true, 3);
		set(s[P_SCALE], "SCALE", "Quantiser scale", 0, NUM_SCALES - 1, 0, true, 3);
		set(s[P_INVERT], "INVERT", "Invert", 0, 1, 0, true, 3);
	}
	return s[i < 0 ? 0 : i >= NUM_P ? NUM_P - 1 : i];
}

// value as text for the screen
inline std::string valueText(int p, float v) {
	int k = (int) std::lround(v);
	switch (p) {
		case P_RATE: return rateName(k);
		case P_FLEX: return flexName(k);
		case P_SHAPE: return shapeName(k);
		case P_CROSS: return crossName(k);
		case P_SOURCE: return sourceName(k);
		case P_SCALE: return scaleName(k);
		case P_INVERT: return k ? "ON" : "OFF";
		case P_LOOP: return k ? std::to_string(k) + " b" : "OFF";
		case P_STEPS: return k ? std::to_string(k) : "OFF";
		case P_NAP: return k ? std::to_string(k) : "OFF";
		case P_PHASE: return std::to_string((int) std::lround(v * 360)) + "\xC2\xB0";
		default: break;
	}
	if (pspec(p).unit == 1) return std::to_string((int) std::lround(v * 100)) + "%";
	return std::to_string(k);
}

// ---- small helpers
inline uint32_t hash32(uint32_t a, uint32_t b, uint32_t c, uint32_t d) {
	uint32_t h = 0x9E3779B9u ^ a;
	h = (h ^ (b + 0x85EBCA6Bu + (h << 6) + (h >> 2))) * 0x27D4EB2Fu;
	h = (h ^ (c + 0xC2B2AE35u + (h << 6) + (h >> 2))) * 0x165667B1u;
	h = (h ^ (d + 0x27D4EB2Fu + (h << 6) + (h >> 2))) * 0x85EBCA77u;
	h ^= h >> 15; h *= 0x2C1B3C6Du; h ^= h >> 12; h *= 0x297A2D39u; h ^= h >> 15;
	return h;
}
inline float hashf(uint32_t a, uint32_t b, uint32_t c, uint32_t d) { return (hash32(a, b, c, d) >> 8) * (1.0f / 16777216.0f); }
inline float clamp01(float x) { return x < 0.f ? 0.f : x > 1.f ? 1.f : x; }
inline bool euclidHit(int i, int k, int n) {  // Bresenham-style Euclidean pattern, hit on step 0
	if (n <= 0 || k <= 0) return false;
	if (k >= n) return true;
	return ((i * k) % n) < k;
}

// quantise 1 V/oct volts to the nearest note in mask
inline float quantize(float volts, uint16_t mask) {
	if (!mask) return volts;
	float semis = volts * 12.f;
	int base = (int) std::floor(semis);
	float best = 1e9f; int bestN = base;
	for (int d = -12; d <= 12; d++) {
		int n = base + d;
		int pc = ((n % 12) + 12) % 12;
		if (!(mask & (1 << pc))) continue;
		float dist = std::fabs(semis - n);
		if (dist < best) { best = dist; bestN = n; }
	}
	return bestN / 12.f;
}

// ---- one output's stored settings
struct OutConfig {
	float v[NUM_P];
	int cvSrc[NUM_P];     // 0 = none, 1..6 = CV1-4, CLK, RUN (when those are set to CV)
	float cvAmt[NUM_P];   // -1..1 of the full range per 5 V
	bool mute;
	uint32_t seed;
	void reset() {
		for (int i = 0; i < NUM_P; i++) { v[i] = pspec(i).def; cvSrc[i] = 0; cvAmt[i] = 0.5f; }
		mute = false;
		seed = 1;
	}
	OutConfig() { reset(); }
};

struct OutState {
	float eff[NUM_P];
	float randHold = 0.f;
	float slewed = 0.f;
	float final01 = 0.f;    // after cross + invert, before quantise (what other outputs read)
	float volts = 0.f;
	float held = 0.f;       // for HOLD / S&H cross ops
	bool srcHigh = false;
	bool latchOff = false;
	bool active = false;    // this step plays (for lights / screen)
	bool sleeping = false;
	int64_t lastN = -1;
	int curStep = 0;        // step index inside the euclid pattern (for the ring)
	float scope[SCOPE_N];
	OutState() { for (int i = 0; i < SCOPE_N; i++) scope[i] = 0.f; for (int i = 0; i < NUM_P; i++) eff[i] = 0.f; }
};

enum ClkMode { CLK_CLOCK, CLK_CV, CLK_NEXT_BANK, NUM_CLKMODES };
enum RunMode { RUN_GATE, RUN_RESET, RUN_CV, RUN_PREV_BANK, NUM_RUNMODES };
inline const char* clkModeName(int i) { static const char* n[3] = {"CLOCK", "CV 5", "NEXT BANK"}; return n[i < 0 ? 0 : i > 2 ? 2 : i]; }
inline const char* runModeName(int i) { static const char* n[4] = {"RUN GATE", "RESET", "CV 6", "PREV BANK"}; return n[i < 0 ? 0 : i > 3 ? 3 : i]; }
inline int ppqnValue(int i) { static const int v[8] = {1, 2, 4, 8, 12, 24, 48, 96}; return v[i < 0 ? 0 : i > 7 ? 7 : i]; }
enum { NUM_PPQN = 8, PPQN_24 = 5 };

struct Inputs {
	float cv[NUM_CV];         // volts: CV1-4, CLK (as CV), RUN (as CV)
	bool clockEdge = false;   // rising edge on CLK (CLOCK mode)
	bool clockConnected = false;
	bool runConnected = false;
	bool runGate = false;     // RUN input high (RUN GATE mode)
	bool resetEdge = false;   // RESET button or RUN input in RESET mode
	bool runToggle = false;   // RUN button
	Inputs() { for (int i = 0; i < NUM_CV; i++) cv[i] = 0.f; }
};

struct Engine {
	OutConfig cfg[NUM_OUT];
	OutState st[NUM_OUT];
	uint16_t user[NUM_USER] = {0x091, 0x4A9, 0xAB5};
	float bpm = 120.f;          // internal tempo
	int clkMode = CLK_CLOCK, runMode = RUN_GATE, ppqnIdx = PPQN_24;
	bool runLatch = true;       // RUN button state (on, so a fresh module plays)
	bool running = false;
	double beat = 0.0;
	double extBpm = 0.0;        // measured external tempo
	double sinceEdge = 1e9;
	long long edgeCount = 0;
	bool extActive = false;
	float startTimer = 0.f, stopTimer = 0.f;

	Engine() { for (int i = 0; i < NUM_OUT; i++) cfg[i].seed = 1 + i * 7919; }

	double tempo() const { return extActive ? extBpm : bpm; }

	void resetClock() {
		beat = 0.0;
		edgeCount = 0;
		for (int i = 0; i < NUM_OUT; i++) { st[i].lastN = -1; st[i].latchOff = false; }
	}
	void reseed(int o) { cfg[o].seed = hash32(cfg[o].seed, 77, (uint32_t) o, 1234567u) | 1u; }

	float effective(int o, int p, const Inputs& in) const {
		const PSpec& s = pspec(p);
		float v = cfg[o].v[p];
		int src = cfg[o].cvSrc[p];
		if (src > 0 && src <= NUM_CV) {
			float cvn = in.cv[src - 1] / 5.f;
			if (cvn < -2.f) cvn = -2.f;
			if (cvn > 2.f) cvn = 2.f;
			v += cvn * cfg[o].cvAmt[p] * (s.max - s.min);
		}
		if (v < s.min) v = s.min;
		if (v > s.max) v = s.max;
		if (s.stepped) v = std::floor(v + 0.5f);
		return v;
	}

	// the raw wave (0..1) of one step at phase ph
	float shapeValue(int shape, float ph, float w, float rA, float rB) const {
		const float sm = 0.04f;  // edge time of the smooth shapes (fraction of a step)
		switch (shape) {
			case SH_PULSE: return ph < w ? 1.f : 0.f;
			case SH_RATCHET2:
			case SH_RATCHET4: {
				int base = shape == SH_RATCHET2 ? 2 : 4;
				int slots = base * 2;
				int slot = (int) (ph * slots);
				int count = (int) std::lround(w * 2.f * base);
				if (count < 1) count = 1;
				if (count > base) count = base;
				return (slot % 2 == 0 && slot / 2 < count) ? 1.f : 0.f;
			}
			case SH_SMOOTH_PULSE: {
				float ww = w < 2 * sm ? 2 * sm : w;
				if (ph < sm) return ph / sm;
				if (ph < ww - sm) return 1.f;
				if (ph < ww) return (ww - ph) / sm;
				return 0.f;
			}
			case SH_TRIANGLE: {
				if (w <= 0.001f) return 1.f - ph;
				if (w >= 0.999f) return ph;
				return ph < w ? ph / w : (1.f - ph) / (1.f - w);
			}
			case SH_TRAPEZOID: {
				float r = 0.5f * w + 0.001f;
				if (ph < r) return ph / r;
				if (ph < 0.5f) return 1.f;
				if (ph < 0.5f + r) return 1.f - (ph - 0.5f) / r;
				return 0.f;
			}
			case SH_SINE:
			case SH_HUMP: {
				float c = 0.05f + 0.9f * w;  // lean: where the half-way point sits
				float p2 = ph < c ? 0.5f * ph / c : 0.5f + 0.5f * (ph - c) / (1.f - c);
				if (shape == SH_SINE) return 0.5f - 0.5f * std::cos(2.f * (float) M_PI * p2);
				return std::sin((float) M_PI * p2);
			}
			case SH_EXP:
			case SH_SMOOTH_EXP: {
				float T = 0.02f + 0.5f * w;
				float v = std::exp(-ph / T);
				if (shape == SH_SMOOTH_EXP && ph < sm) v *= ph / sm;
				return v;
			}
			case SH_LOG:
			case SH_SMOOTH_LOG: {
				float T = 0.04f + 0.96f * w;
				float x = ph / T;
				float v = x >= 1.f ? 0.f : 1.f - x * x * x;
				if (shape == SH_SMOOTH_LOG && ph < sm) v *= ph / sm;
				return v;
			}
			case SH_RANDOM: {
				float spread = 2.f * w;  // below 50% narrower, above 50% pushed to the extremes
				return clamp01(0.5f + (rA - 0.5f) * spread);
			}
			case SH_SMOOTH_RANDOM: {
				float lin = ph, cosv = 0.5f - 0.5f * std::cos((float) M_PI * ph);
				float k = w * lin + (1.f - w) * cosv;
				return rA + (rB - rA) * k;
			}
		}
		return 0.f;
	}

	static float warpRamp(int mode, float u, float amt) {
		float k = 1.f + 1.6f * amt;
		switch (mode) {
			case FX_RAMP_UP: return 1.f - std::pow(1.f - u, k);
			case FX_RAMP_DOWN: return std::pow(u, k);
			case FX_HUMP:
				if (u < 0.5f) return 0.5f * (1.f - std::pow(1.f - 2.f * u, k));
				return 0.5f + 0.5f * std::pow(2.f * u - 1.f, k);
		}
		return u;
	}

	float sourceValue(int src, int self, const Inputs& in) const {
		if (src < 8) return src == self ? st[self].final01 : st[src].final01;
		int c = src - 8;
		return clamp01(in.cv[c] / 5.f);
	}

	// one output, one sample
	void processOut(int o, float dt, const Inputs& in) {
		OutState& s = st[o];
		const OutConfig& c = cfg[o];
		for (int p = 0; p < NUM_P; p++) s.eff[p] = effective(o, p, in);
		const float* e = s.eff;
		int rateIdx = (int) e[P_RATE];
		double r = rateValue(rateIdx);
		int shape = (int) e[P_SHAPE];
		float w = e[P_WIDTH];
		float v = 0.f;
		bool play = false;
		s.sleeping = false;

		if (r < 0) {
			if (r == -2) v = running ? 1.f : 0.f;
			else if (r == -3) v = startTimer > 0.f ? 1.f : 0.f;
			else if (r == -4) v = stopTimer > 0.f ? 1.f : 0.f;
			play = v > 0.f;
			s.curStep = 0;
		}
		else {
			double B = beat;
			int loop = (int) e[P_LOOP];
			long long li = 0;
			double bl = B;
			if (loop > 0) { li = (long long) std::floor(B / loop); bl = B - (double) li * loop; }
			int nap = (int) e[P_NAP], wake = (int) e[P_WAKE];
			if (loop > 0 && nap > 0) {
				long long cyc = wake + nap;
				long long pos = ((li + (long long) e[P_LSHIFT]) % cyc + cyc) % cyc;
				s.sleeping = pos >= wake;
			}
			double t = bl * r;
			int flex = (int) e[P_FLEX];
			float amt = e[P_AMOUNT];
			if (flex == FX_RAMP_UP || flex == FX_RAMP_DOWN || flex == FX_HUMP) {
				double N = loop > 0 ? loop * r : 16.0;
				if (N < 1.0) N = 1.0;
				double wi = std::floor(t / N);
				float u = (float) ((t - wi * N) / N);
				t = wi * N + warpRamp(flex, u, amt) * N;
			}
			else if (flex == FX_SWING) {
				double sw = 0.6 * amt;
				double pairStart = 2.0 * std::floor(t / 2.0);
				double p = t - pairStart;
				t = p < 1.0 + sw ? pairStart + p / (1.0 + sw) : pairStart + 1.0 + (p - 1.0 - sw) / (1.0 - sw);
			}
			t += e[P_PHASE];
			double nf = std::floor(t);
			long long n = (long long) nf;
			float ph = (float) (t - nf);
			uint32_t seed = c.seed;
			uint32_t nu = (uint32_t) (n & 0xFFFFFFFF);
			bool rest = false;
			if (flex == FX_HUMAN || flex == FX_DELAY) {
				float d = flex == FX_DELAY ? 0.95f * amt : 0.4f * amt * hashf(seed, nu, o, 11);
				if (ph < d) rest = true;
				else ph -= d;
			}
			else if (flex == FX_PWR2) {
				int maxk = (int) (amt * 3.99f);
				int k = (int) (hashf(seed, nu, o, 12) * (maxk + 1));
				if (k > 0) { float sub = (float) (1 << k); ph = ph * sub - std::floor(ph * sub); }
			}
			// euclid
			bool hit = true;
			int S = (int) e[P_STEPS];
			if (S > 0) {
				int L = S + (int) e[P_PAD];
				int idx = (int) ((((n - (long long) e[P_ESHIFT]) % L) + L) % L);
				s.curStep = idx;
				hit = idx < S && euclidHit(idx, (int) e[P_TRIGS], S);
				if (!hit && idx < S && e[P_RAND] > 0.f)
					hit = hashf(seed, (uint32_t) idx, o, 13) < e[P_RAND];
			}
			else s.curStep = (int) (((n % 16) + 16) % 16);
			if (hit && e[P_PROB] < 1.f) hit = hashf(seed, nu, o, 14) < e[P_PROB];
			play = hit && !rest && !s.sleeping;
			float rA = hashf(seed, nu, o, 15), rB = hashf(seed, nu + 1, o, 15);
			if (isRandomShape(shape)) {
				if (play) { v = shapeValue(shape, ph, w, rA, rB); s.randHold = v; }
				else v = s.sleeping ? 0.f : s.randHold;
			}
			else if (play) v = shapeValue(shape, ph, w, rA, rB);
			if (n != s.lastN) s.lastN = n;
		}
		if (rateIdx == 0) { v = 0.f; play = false; }

		float out = rateIdx == 0 ? 0.f : clamp01(e[P_OFFSET] + v * e[P_LEVEL]);
		// slew (up to ~1 s)
		float sl = e[P_SLEW];
		if (sl > 0.f) {
			float tau = 0.0005f + sl * sl * 1.f;
			s.slewed += (out - s.slewed) * (1.f - std::exp(-dt / tau));
			out = s.slewed;
		}
		else s.slewed = out;

		// cross operation
		int op = (int) e[P_CROSS];
		if (op != CR_NONE) {
			float sv = sourceValue((int) e[P_SOURCE], o, in);
			bool sh = sv > 0.02f;
			bool rise = sh && !s.srcHigh;
			s.srcHigh = sh;
			bool ah = out > 0.02f;
			int ai = (int) (out * 4095.f), bi = (int) (sv * 4095.f);
			switch (op) {
				case CR_MIX: out = 0.5f * (out + sv); break;
				case CR_MULT: out = out * sv; break;
				case CR_ADD: out = clamp01(out + sv); break;
				case CR_SUB: out = clamp01(out - sv); break;
				case CR_MIN: out = out < sv ? out : sv; break;
				case CR_MAX: out = out > sv ? out : sv; break;
				case CR_HOLD: if (sh) out = s.held; else s.held = out; break;
				case CR_SH: if (rise) s.held = out; out = s.held; break;
				case CR_MASK: if (!sh) out = 0.f; break;
				case CR_NOT: if (sh) out = 0.f; break;
				case CR_OR: out = (ah || sh) ? 1.f : 0.f; break;
				case CR_XOR: out = (ah != sh) ? 1.f : 0.f; break;
				case CR_AND: out = (ah && sh) ? 1.f : 0.f; break;
				case CR_BITOR: out = (ai | bi) / 4095.f; break;
				case CR_BITXOR: out = (ai ^ bi) / 4095.f; break;
				case CR_BITAND: out = (ai & bi) / 4095.f; break;
				case CR_SEED: if (rise) reseed(o); break;
				case CR_LATCH: if (rise) s.latchOff = !s.latchOff; if (s.latchOff) out = 0.f; break;
			}
		}
		if (e[P_INVERT] > 0.5f) out = 1.f - out;
		s.final01 = out;
		s.active = play;
		float volts = out * 5.f;
		int sc = (int) e[P_SCALE];
		if (sc > 0) volts = quantize(volts, sc >= SC_USER1 ? user[sc - SC_USER1] : scaleMask(sc));
		if (c.mute) volts = 0.f;
		s.volts = volts;
		// phase-locked scope: 4 beats wide
		double sp = std::fmod(beat, 4.0) / 4.0;
		int si = (int) (sp * SCOPE_N);
		if (si >= 0 && si < SCOPE_N) s.scope[si] = c.mute ? 0.f : volts / 5.f;
	}

	// returns +1 / -1 when an input asked for the next / previous bank
	int process(float dt, const Inputs& in) {
		int bankStep = 0;
		if (in.runToggle) runLatch = !runLatch;
		// clock input
		bool ext = clkMode == CLK_CLOCK && in.clockConnected;
		sinceEdge += dt;
		if (ext && in.clockEdge) {
			if (sinceEdge < 4.0) {
				double b = 60.0 / (sinceEdge * ppqnValue(ppqnIdx));
				extBpm = extBpm > 0 ? extBpm + (b - extBpm) * 0.5 : b;
			}
			sinceEdge = 0.0;
			extActive = true;
		}
		if (!ext || sinceEdge > 2.0) extActive = false;

		bool want;
		if (runMode == RUN_GATE && in.runConnected) want = in.runGate;  // DIN-sync style run gate
		else if (ext) want = extActive && runLatch;                     // follow the external clock
		else want = runLatch;
		if (want && !running) { running = true; startTimer = 0.01f; resetClock(); }
		if (!want && running) { running = false; stopTimer = 0.01f; }
		if (!running) { beat = 0.0; edgeCount = 0; }
		if (in.resetEdge) resetClock();
		if (clkMode == CLK_NEXT_BANK && in.clockEdge) bankStep = 1;

		if (running) {
			if (extActive) {
				if (in.clockEdge) {
					edgeCount++;
					beat = (double) (edgeCount - 1) / ppqnValue(ppqnIdx);
				}
				else {
					double next = (double) edgeCount / ppqnValue(ppqnIdx);
					beat += dt * extBpm / 60.0;
					if (beat > next - 1e-6) beat = next - 1e-6;
				}
			}
			else beat += dt * bpm / 60.0;
		}
		for (int o = 0; o < NUM_OUT; o++) processOut(o, dt, in);
		if (startTimer > 0.f) startTimer -= dt;
		if (stopTimer > 0.f) stopTimer -= dt;
		return bankStep;
	}
};

}  // namespace deidre
