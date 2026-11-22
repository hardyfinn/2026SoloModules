// GamelanCore: the sequencing engines behind Irama and Gatra.
// No Rack dependency, so test/render.cpp can run them offline.
//
//   IramaCore  - gong cycle (Javanese and Balinese forms), balungan built
//                backwards from its landing tones (seleh) with pathet weights,
//                elaborating parts at 5 irama levels, kotekan / imbal
//                interlocking pairs, angsel breaks, kebyar bursts, suwuk.
//   GatraCore  - the same structure turned into a 4/4 drum sequencer:
//                straight kick, gong cycle on drum roles, LAND moves the gong
//                from the end of the cycle to the downbeat, kotekan hats,
//                polymeter cycles, drop / fill.
//
// Weights for slendro / pelog / selisir are designed from the pathet literature
// (see research/gamelan-weights-and-seleh-quantizer.pdf), not measured counts.
#pragma once
#include <cstdint>
#include <cmath>
#include <cstring>
#include <algorithm>
#include <cstdlib>

namespace irama {

struct Rng {
	uint32_t s;
	Rng() : s(0x9E3779B9u) {}
	void seed(uint32_t v) { s = v ? v : 0x1234567u; }
	uint32_t next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
	float uni() { return (next() >> 8) * (1.f / 16777216.f); }
	int below(int n) { int r = (int)(uni() * n); return r < n ? r : n - 1; }
};

static inline int floorDiv(int a, int b) { return (a >= 0) ? a / b : -((-a + b - 1) / b); }
static inline int modp(int a, int b) { int m = a % b; return m < 0 ? m + b : m; }

// ---------------------------------------------------------------- tunings
enum Laras { SLENDRO = 0, PELOG = 1, SELISIR = 2 };
static const float TUNE_SLENDRO[5] = {0.f, 240.f, 475.f, 720.f, 955.f};             // 1 2 3 5 6
static const float TUNE_PELOG[7] = {0.f, 120.f, 258.f, 539.f, 675.f, 785.f, 943.f}; // 1 2 3 4 5 6 7
static const float TUNE_SELISIR[5] = {0.f, 125.f, 270.f, 680.f, 795.f};             // ding dong deng dung dang

static inline float tuneCents(int laras, int idx) {
	if (laras == PELOG) return TUNE_PELOG[modp(idx, 7)];
	if (laras == SELISIR) return TUNE_SELISIR[modp(idx, 5)];
	return TUNE_SLENDRO[modp(idx, 5)];
}

struct Mode {
	const char* name;
	int laras;
	int pool[5];      // tuning index of each of the 5 melody tones, low to high
	float w[5];       // seleh weights (designed)
	int stressed;     // pool index of the gong tone
	int guest[2];     // rare tuning tones used as passing notes (-1 none)
	float guestW[2];
};

// Java: 6 pathet. Bali: selisir and slendro (gender wayang).
static const int NUM_JAVA_MODES = 6;
static const int NUM_BALI_MODES = 2;
static const Mode MODES[8] = {
	{"SLENDRO NEM",     SLENDRO, {0, 1, 2, 3, 4}, {.10f, .30f, .18f, .24f, .18f}, 1, {-1, -1}, {0.f, 0.f}},
	{"SLENDRO SANGA",   SLENDRO, {0, 1, 2, 3, 4}, {.24f, .18f, .10f, .30f, .18f}, 3, {-1, -1}, {0.f, 0.f}},
	{"SLENDRO MANYURA", SLENDRO, {0, 1, 2, 3, 4}, {.18f, .24f, .18f, .10f, .30f}, 4, {-1, -1}, {0.f, 0.f}},
	{"PELOG LIMA",      PELOG,   {0, 1, 2, 4, 5}, {.28f, .14f, .16f, .20f, .12f}, 0, {3, 6}, {.05f, .03f}},
	{"PELOG NEM",       PELOG,   {0, 1, 2, 4, 5}, {.20f, .14f, .12f, .28f, .16f}, 3, {3, 6}, {.05f, .03f}},
	{"PELOG BARANG",    PELOG,   {1, 2, 4, 5, 6}, {.20f, .14f, .16f, .28f, .12f}, 3, {3, 0}, {.05f, .03f}},
	{"SELISIR",         SELISIR, {0, 1, 2, 3, 4}, {.28f, .18f, .20f, .14f, .20f}, 0, {-1, -1}, {0.f, 0.f}},
	{"SLENDRO (BALI)",  SLENDRO, {0, 1, 2, 3, 4}, {.18f, .24f, .18f, .10f, .30f}, 4, {-1, -1}, {0.f, 0.f}},
};

// Cipher (kepatihan) names per tuning index; Bali uses the vowel names.
static inline const char* toneName(int laras, int idx) {
	static const char* SL[5] = {"1", "2", "3", "5", "6"};
	static const char* PL[7] = {"1", "2", "3", "4", "5", "6", "7"};
	static const char* BA[5] = {"i", "o", "e", "u", "a"};   // ding dong deng dung dang
	if (laras == PELOG) return PL[modp(idx, 7)];
	if (laras == SELISIR) return BA[modp(idx, 5)];
	return SL[modp(idx, 5)];
}

// ---------------------------------------------------------------- forms
enum Colo { C_GONG = 1, C_KENONG = 2, C_KEMPUL = 4, C_KETHUK = 8, C_KEMPYANG = 16 };
enum Style { JAVA = 0, BALI = 1 };
static const char* const JAVA_FORMS[4] = {"LANCARAN", "KETAWANG", "LADRANG", "GENDHING"};
static const char* const BALI_FORMS[4] = {"GILAK", "BAPANG", "TABUH PISAN", "TABUH TELU"};
static const int JAVA_BEATS[4] = {16, 16, 32, 64};
static const int BALI_BEATS[4] = {8, 16, 32, 64};

static inline int formBeats(int style, int form) {
	return style == BALI ? BALI_BEATS[form & 3] : JAVA_BEATS[form & 3];
}

// Colotomic strokes on beat `b` (1-based, gong on the last beat).
// Bali uses the same outputs: KENONG = kempur, KEMPUL = kemong,
// KETHUK = kempli (every beat), KEMPYANG = kajar (odd beats).
static inline int formMask(int style, int form, int b) {
	int N = formBeats(style, form);
	int m = 0;
	if (b == N) m |= C_GONG;
	if (style == JAVA) {
		switch (form & 3) {
		case 0: // lancaran
			if (b % 2 == 1) m |= C_KETHUK;
			if (b == 6 || b == 10 || b == 14) m |= C_KEMPUL;
			if (b % 4 == 0) m |= C_KENONG;
			break;
		case 1: // ketawang
			if (b % 2 == 1) m |= C_KEMPYANG;
			if (b % 4 == 2) m |= C_KETHUK;
			if (b == 12) m |= C_KEMPUL;
			if (b % 8 == 0) m |= C_KENONG;
			break;
		case 2: // ladrang
			if (b % 2 == 1) m |= C_KEMPYANG;
			if (b % 4 == 2) m |= C_KETHUK;
			if (b == 12 || b == 20 || b == 28) m |= C_KEMPUL;
			if (b % 8 == 0) m |= C_KENONG;
			break;
		default: // gendhing ketuk 2 kerep
			if (b % 16 == 4 || b % 16 == 12) m |= C_KETHUK;
			if (b % 16 == 0) m |= C_KENONG;
			if (b % 16 == 8 && b > 16) m |= C_KEMPUL;
			break;
		}
	}
	else {
		m |= C_KETHUK;                 // kempli on every beat
		if (b % 2 == 1) m |= C_KEMPYANG;
		int half = N / 2, q = N / 4;
		if (b % half == 0 && b != N) m |= C_KENONG;             // kempur
		if (b % q == 0 && b % half != 0) m |= C_KEMPUL;         // kemong
		if (N == 8) { m &= ~C_KEMPUL; if (b == 2 || b == 6) m |= C_KEMPUL; }
	}
	return m;
}

// ---------------------------------------------------------------- irama
static const char* const LEVEL_NAMES[5] = {"LANCAR", "TANGGUNG", "DADI", "WILET", "RANGKEP"};
static const int LEVEL_D[5] = {1, 2, 4, 8, 16};    // fastest line (kotekan) per balungan beat
static const int LEVEL_PEK[5] = {1, 2, 2, 4, 4};   // peking / kantilan
static const int LEVEL_BON[5] = {1, 1, 2, 4, 8};   // bonang / reyong

enum Kotekan { K_IMBAL = 0, K_NOROT, K_NYOGCAG, K_TELU, K_EMPAT, NUM_KOTEKAN };
static const char* const KOTEKAN_NAMES[NUM_KOTEKAN] = {"IMBAL", "NOROT", "NYOG CAG", "TELU", "EMPAT"};

enum Voice { V_SARON = 0, V_PEKING, V_BONANG, V_POLOS, V_SANGSIH, NUM_VOICES };
enum Trig { T_GONG = 0, T_KENONG, T_KEMPUL, T_KETHUK, T_KEMPYANG, T_GATRA, T_KENDANG, T_END, NUM_TRIGS };

static const int MAX_BEATS = 64;

struct IramaFrame {
	bool trig[NUM_TRIGS];
	bool fire[NUM_VOICES];
	float volts[NUM_VOICES];   // octave offsets relative to the root (0 V = root, middle octave)
	float interval[NUM_VOICES];// seconds until this line's next note (for gate length)
	bool seleh;                // seleh pitch changed
	float selehVolts;
	bool angsel;               // break in progress
	void clear() { std::memset(this, 0, sizeof(IramaFrame)); }
};

struct IramaCore {
	// ---- parameters (set by the module)
	int style = JAVA, form = 0, mode = 2;
	int levelTarget = 1;
	int kotekan = K_IMBAL;
	bool tempoFixed = false;
	float mutate = 0.3f, focus = 1.f, angselChance = 0.f;
	bool lock = false;
	bool kebyar = false;            // gate (held)

	// ---- balungan
	int balP[MAX_BEATS];            // pool position: octave * 5 + pool index (middle octave = 5..9)
	int balG[MAX_BEATS];            // guest tuning tone replacing it, -1 none
	int N = 16;
	int builtStyle = -1, builtForm = -1, builtMode = -1;
	Rng rng;

	// ---- transport
	bool started = false, stopped = false;
	int beat = 0, level = 1, B = 2, D = 2;
	int pulseInBeat = 0, nextStep = 0;
	double sinceClock = 0.0, period = 0.0;    // samples
	float sampleRate = 48000.f;
	bool angselGatra = false, angselPending = false, suwukPending = false;
	bool prevKebyar = false;
	int kebyarRun = 0;
	int angselCount = 0;

	IramaCore() {
		for (int i = 0; i < MAX_BEATS; i++) { balP[i] = 7; balG[i] = -1; }
	}

	const Mode& M() const { return MODES[mode]; }
	int numModes() const { return style == BALI ? NUM_BALI_MODES : NUM_JAVA_MODES; }
	int modeIndex(int knob) const {
		if (style == BALI) return NUM_JAVA_MODES + (knob % NUM_BALI_MODES);
		return knob % NUM_JAVA_MODES;
	}

	// pitch of a pool position (+ guest), volts relative to root, middle octave at 0 V
	float poolVolts(int p, int guest = -1) const {
		const Mode& m = M();
		int oct = floorDiv(p, 5), idx = modp(p, 5);
		int t = guest >= 0 ? guest : m.pool[idx];
		return (float)(oct - 1) + tuneCents(m.laras, t) / 1200.f;
	}

	// ---------------------------------------------------- balungan builder
	int pickWeighted(const float* w, int n, float f) {
		float ww[8]; float sum = 0.f;
		for (int i = 0; i < n; i++) { ww[i] = std::pow(w[i] + 1e-4f, f); sum += ww[i]; }
		float r = rng.uni() * sum;
		for (int i = 0; i < n; i++) { r -= ww[i]; if (r <= 0.f) return i; }
		return n - 1;
	}
	bool isKenongGatra(int gi) const { return (formMask(style, form, gi * 4 + 4) & (C_KENONG | C_GONG)) != 0; }

	void buildGatra(int gi) {
		const Mode& m = M();
		int G = N / 4;
		int sel;
		if (gi == G - 1 && focus > 0.4f) sel = m.stressed;
		else sel = pickWeighted(m.w, 5, isKenongGatra(gi) ? focus + 1.f : focus);
		int p = sel + 5;
		int base = gi * 4;
		balP[base + 3] = p; balG[base + 3] = -1;
		static const int STEPS[5] = {1, -1, 2, -2, 0};
		static const float SW[5] = {.42f, .26f, .14f, .10f, .08f};
		for (int k = 2; k >= 0; k--) {
			int st = STEPS[pickWeighted(SW, 5, 1.f)];
			int np = p + st;
			if (np < 3 || np > 11) np = p - st;
			p = np;
			balP[base + k] = p; balG[base + k] = -1;
			// rare guest tone (pelog 4 / 7 / 1) replaces a neighbouring pool tone
			for (int j = 0; j < 2; j++) {
				int g = m.guest[j];
				if (g < 0) continue;
				int t = m.pool[modp(p, 5)];
				if (std::abs(t - g) == 1 && rng.uni() < m.guestW[j] * 2.5f) { balG[base + k] = g; break; }
			}
		}
	}

	void buildAll() {
		N = formBeats(style, form);
		for (int g = 0; g < N / 4; g++) buildGatra(g);
		builtStyle = style; builtForm = form; builtMode = mode;
	}

	void checkBuild() {
		if (builtStyle != style || builtForm != form || builtMode != mode) {
			int oldN = N;
			buildAll();
			if (oldN != N) beat = beat % N;
		}
	}

	void mutateCycle() {
		if (lock) return;
		int G = N / 4;
		if (rng.uni() < mutate) buildGatra(rng.below(G > 1 ? G - 1 : 1));
		if (rng.uni() < mutate * mutate) buildGatra(rng.below(G > 1 ? G - 1 : 1));
	}

	// ---------------------------------------------------- transport
	void reset() { started = false; stopped = false; beat = 0; pulseInBeat = 0; nextStep = 0; suwukPending = false; angselGatra = false; }
	void triggerAngsel() { angselPending = true; }
	void triggerSuwuk() {
		if (stopped) { reset(); return; }
		suwukPending = true;
	}

	void startBeat() {
		pulseInBeat = 0; nextStep = 0;
		D = LEVEL_D[level];
		B = tempoFixed ? 4 : (D < 2 ? 2 : D);
	}
	void onGatraStart() {
		level = suwukPending ? 0 : levelTarget;
		angselGatra = angselPending || (angselChance > 0.f && rng.uni() < angselChance * angselChance);
		angselPending = false;
		if (angselGatra) angselCount++;
	}
	void advanceBeat(IramaFrame& f) {
		beat++;
		if (beat >= N) {
			beat = 0;
			if (suwukPending) { stopped = true; suwukPending = false; f.trig[T_END] = true; return; }
			mutateCycle();
		}
		checkBuild();
		if (beat % 4 == 0) onGatraStart();
		startBeat();
	}

	float stepSeconds() const {
		double per = period > 0.0 ? period : sampleRate * 0.125;
		return (float)(per * B / D / sampleRate);
	}

	// call once per sample
	void tick(bool clockRise, IramaFrame& f) {
		f.clear();
		if (clockRise) {
			if (started && !stopped) {
				fireUpTo(f, (float)pulseInBeat + 0.9999f);
				pulseInBeat++;
				if (pulseInBeat >= B) advanceBeat(f);
			}
			else if (!started) {
				started = true; stopped = false;
				checkBuild();
				beat = 0;
				onGatraStart();
				startBeat();
			}
			if (sinceClock > 0.0 && sinceClock < sampleRate * 4.0) period = sinceClock;
			sinceClock = 0.0;
		}
		else sinceClock += 1.0;
		if (started && !stopped) {
			float ph = period > 0.0 ? (float)std::min(sinceClock / period, 0.9999) : 0.f;
			fireUpTo(f, (float)pulseInBeat + ph);
		}
	}

	void fireUpTo(IramaFrame& f, float x) {
		while (nextStep < D && (float)nextStep * B / D <= x + 1e-6f) {
			doStep(f, nextStep);
			nextStep++;
		}
	}

	// ---------------------------------------------------- one elaboration step
	void note(IramaFrame& f, int v, float volts, int perBeat) {
		f.fire[v] = true; f.volts[v] = volts;
		f.interval[v] = stepSeconds() * (float)D / (float)perBeat;
	}

	void tutti(IramaFrame& f, int x, int a) {
		note(f, V_SARON, poolVolts(modp(a, 5) + 5), D);
		note(f, V_PEKING, poolVolts(modp(a, 5) + 5) + 1.f, D);
		note(f, V_BONANG, poolVolts(modp(x, 5) + 7), D);
		note(f, V_POLOS, poolVolts(modp(x, 5) + 5) + 1.f, D);
		note(f, V_SANGSIH, poolVolts(modp(x, 5) + 8) + 1.f, D);   // kempyung (4 tones up)
		f.trig[T_KENDANG] = true;
	}

	void doStep(IramaFrame& f, int s) {
		const int b = beat;
		const int a = balP[b];
		const int n = balP[(b + 1) % N];
		const int posInGatra = b % 4;

		// ---- kebyar: unison bursts while the gate is high
		if (kebyar != prevKebyar) {
			prevKebyar = kebyar;
			f.trig[T_GONG] = true;
			tutti(f, n, a);
			kebyarRun = 1;
			return;
		}
		if (kebyar) {
			float p = kebyarRun >= 2 ? 0.12f : 0.38f;
			if (s == 0 && posInGatra == 3) p = 0.9f;
			if (rng.uni() < p) { tutti(f, n, a); kebyarRun++; }
			else kebyarRun = 0;
			if (s == 0 && b == N - 1) f.trig[T_GONG] = true;
			return;
		}

		// ---- colotomy and seleh on the beat
		const bool inAngsel = angselGatra && posInGatra >= 2;
		f.angsel = inAngsel;
		if (s == 0) {
			int m = formMask(style, form, b + 1);
			if (inAngsel) m &= (C_GONG | C_KENONG);
			for (int i = 0; i < 5; i++) if (m & (1 << i)) f.trig[T_GONG + i] = true;
			if (posInGatra == 3) {
				f.trig[T_GATRA] = true;
				f.seleh = true;
				f.selehVolts = poolVolts(modp(a, 5) + 5, balG[b]) - 1.f;
			}
		}
		// kendang cue half a beat before each seleh, and before an angsel
		if (!inAngsel && s == D / 2 && D >= 2 && (posInGatra == 2 || (angselGatra && posInGatra == 1))) f.trig[T_KENDANG] = true;
		if (!inAngsel && D == 1 && posInGatra == 2 && s == 0) f.trig[T_KENDANG] = true;

		if (inAngsel) {
			// 3 + 3 + 2 accents over the last two beats, then a held breath
			float fr0 = ((posInGatra - 2) * D + s) / (2.f * D);
			float fr1 = ((posInGatra - 2) * D + s + 1) / (2.f * D);
			static const float ACC[3] = {0.f, 0.375f, 0.75f};
			for (int i = 0; i < 3; i++)
				if (ACC[i] >= fr0 - 1e-5f && ACC[i] < fr1 - 1e-5f) tutti(f, n, a);
			return;
		}

		// ---- saron: the balungan on the beat
		if (s == 0) note(f, V_SARON, poolVolts(a, balG[b]), 1);

		const int p0 = b - (b % 2);
		const int px = balP[p0], py = balP[(p0 + 1) % N];

		// ---- peking: "x x y y" over each pair of beats
		{
			int pd = LEVEL_PEK[level], every = D / pd;
			if (s % every == 0) {
				int k = (b % 2) * pd + s / every;
				int t = pd == 1 ? a : ((k % 4) < 2 ? px : py);
				note(f, V_PEKING, poolVolts(t) + 1.f, pd);
			}
		}
		// ---- bonang (Java: mipil, alternating the pair) / reyong (Bali: norot on the next tone)
		{
			int bd = LEVEL_BON[level], every = D / bd;
			if (s % every == 0) {
				int k = (b % 2) * bd + s / every;
				int t;
				if (style == BALI) t = (bd == 1) ? n : ((k % 2 == 0) ? modp(n, 5) + 6 : modp(n, 5) + 5);
				else t = (k % 2 == 0) ? px : py;
				note(f, V_BONANG, poolVolts(t), bd);
			}
		}
		// ---- kotekan / imbal: two interlocking parts, one octave up
		{
			const int x = modp(n, 5) + 5;   // target (next balungan tone), gangsa register
			const int aT = modp(a, 5) + 5;
			if (D == 1) {
				if (b % 2 == 0) note(f, V_POLOS, poolVolts(aT) + 1.f, 1);
				else note(f, V_SANGSIH, poolVolts(aT + 1) + 1.f, 1);
				return;
			}
			if (s == 0) {
				note(f, V_POLOS, poolVolts(aT) + 1.f, D);
				if (kotekan == K_TELU || kotekan == K_NOROT) note(f, V_SANGSIH, poolVolts(aT) + 1.f, D);
				if (kotekan == K_EMPAT) note(f, V_SANGSIH, poolVolts(aT + 3) + 1.f, D);
				return;
			}
			const int k = D - s;   // steps left until the next beat (1 = last)
			switch (kotekan) {
			case K_NOROT: {
				if (k % 2 == 0) note(f, V_POLOS, poolVolts(x) + 1.f, D);
				else note(f, V_SANGSIH, poolVolts(x + 1) + 1.f, D);
			} break;
			case K_NYOGCAG: {
				static const int NC[8] = {1, 2, 3, 2, 1, 2, 3, 4};
				int t = x + NC[(k - 1) % 8];
				if (s % 2 == 0) note(f, V_POLOS, poolVolts(t) + 1.f, D);
				else note(f, V_SANGSIH, poolVolts(t) + 1.f, D);
			} break;
			case K_TELU: {
				static const int T3[3] = {1, 2, 0};
				int o = T3[(k - 1) % 3];
				if (o <= 1) note(f, V_POLOS, poolVolts(x + o) + 1.f, D);
				if (o >= 1) note(f, V_SANGSIH, poolVolts(x + o) + 1.f, D);
			} break;
			case K_EMPAT: {
				static const int T4[4] = {1, 2, 3, 0};
				int o = T4[(k - 1) % 4];
				if (o <= 1) note(f, V_POLOS, poolVolts(x + o) + 1.f, D);
				if (o >= 2) note(f, V_SANGSIH, poolVolts(x + o) + 1.f, D);
				if (o == 0) note(f, V_SANGSIH, poolVolts(x + 3) + 1.f, D);   // kempyung: both strike
			} break;
			default: { // IMBAL (Java bonang imbal): fixed alternating figure
				if (k % 2 == 1) note(f, V_SANGSIH, poolVolts(x + ((k - 1) / 2 % 2 == 0 ? 1 : 0)), D);
				else note(f, V_POLOS, poolVolts(x + ((k - 2) / 2 % 2 == 0 ? 3 : 2)), D);
			} break;
			}
		}
	}
};

// ================================================================ Gatra (drums)
enum DrumOut { D_KICK = 0, D_GONG, D_KENONG, D_KEMPUL, D_KETHUK, D_KEMPYANG, D_POLOS, D_SANGSIH, D_ACCENT, NUM_DRUM };
enum DrumKot { DK_NYOGCAG = 0, DK_TELU, DK_EMPAT };
static const int CYCLE_CHOICES[7] = {8, 12, 16, 20, 24, 28, 32};

struct GatraFrame {
	bool trig[NUM_DRUM];
	bool angsel;     // break in progress (gate)
	bool odd;        // an odd 16th (swing applies)
	void clear() { std::memset(this, 0, sizeof(GatraFrame)); }
};

struct GatraCore {
	// parameters
	int cycle = 16;            // beats per gong cycle
	float land = 0.f;          // 0 = gong at the end (gamelan), 1 = on the downbeat (techno)
	int beatLen = 4;           // 16ths per gamelan beat (4 = quarter, 2 = eighth)
	int kotekan = DK_NYOGCAG;
	float density = 0.85f, mutate = 0.2f, angselChance = 0.f;
	bool kickOn = true;
	bool drop = false, fill = false;

	// state
	int64_t pos = -1;         // 16th index since reset
	float dens[64];
	bool angselCycle = false, prevDrop = false, dropRelease = false;
	Rng rng;

	GatraCore() { for (int i = 0; i < 64; i++) dens[i] = rng.uni(); }
	void reseed(uint32_t s) { rng.seed(s); for (int i = 0; i < 64; i++) dens[i] = rng.uni(); }
	void reset() { pos = -1; }

	int shift() const { return (int)std::floor(land * beatLen + 0.5f); }
	int cycleLen() const { return cycle * beatLen; }
	// position inside the gong cycle for 16th p (0 = first 16th of beat 1)
	int cyclePos(int64_t p) const { return (int)(((p - shift()) % cycleLen() + cycleLen()) % cycleLen()); }

	int coloMask(int b) const {   // b 1-based beat of the cycle
		int m = 0, Nb = cycle, K = Nb / 4;
		if (b == Nb) m |= C_GONG;
		if (b % K == 0) m |= C_KENONG;
		for (int k = 1; k <= 3; k++) if (b == k * K + (K + 1) / 2 && b % K != 0) m |= C_KEMPUL;
		if (b % 4 == 2) m |= C_KETHUK;
		if (b % 2 == 1) m |= C_KEMPYANG;
		return m;
	}

	void clock(GatraFrame& f) {
		f.clear();
		pos++;
		const int q = cyclePos(pos);
		const int L = cycleLen();
		if (q == 0) {   // a new gong cycle begins (just after the gong)
			for (int i = 0; i < 64; i++) if (rng.uni() < mutate * 0.25f) dens[i] = rng.uni();
			angselCycle = angselChance > 0.f && rng.uni() < angselChance;
		}
		const bool brk = (angselCycle && q >= L - 4 * beatLen && q < L - beatLen) || drop;
		f.angsel = brk;
		f.odd = (pos % 2) == 1;

		if (prevDrop && !drop) dropRelease = true;
		prevDrop = drop;

		// ---- fill (kebyar): random tutti bursts
		if (fill) {
			if (rng.uni() < 0.45f) {
				int pick = 1 + rng.below(31);
				if (pick & 1) f.trig[D_KICK] = true;
				if (pick & 2) f.trig[D_KENONG] = true;
				if (pick & 4) f.trig[D_KEMPUL] = true;
				if (pick & 8) f.trig[D_POLOS] = true;
				if (pick & 16) f.trig[D_SANGSIH] = true;
				f.trig[D_ACCENT] = true;
			}
			if (q % beatLen == 0 && (coloMask(q / beatLen + 1) & C_GONG)) f.trig[D_GONG] = true;
			return;
		}

		// ---- gong cycle on drum roles
		if (q % beatLen == 0) {
			int m = coloMask(q / beatLen + 1);
			if (brk) m &= (C_GONG | C_KENONG);
			if (m & C_GONG) { f.trig[D_GONG] = true; f.trig[D_ACCENT] = true; }
			if (m & C_KENONG) f.trig[D_KENONG] = true;
			if (m & C_KEMPUL) f.trig[D_KEMPUL] = true;
			if (m & C_KETHUK) f.trig[D_KETHUK] = true;
			if (m & C_KEMPYANG) f.trig[D_KEMPYANG] = true;
		}
		// ---- straight kick
		if (pos % 4 == 0) {
			if (dropRelease) { f.trig[D_GONG] = true; f.trig[D_ACCENT] = true; dropRelease = false; if (kickOn) f.trig[D_KICK] = true; }
			else if (kickOn && !brk) f.trig[D_KICK] = true;
		}
		if (brk) return;
		// ---- kotekan hats
		if (dens[pos % 64] < density || pos % 4 == 0) {
			bool pol = false, san = false;
			switch (kotekan) {
			case DK_TELU: { int t = (int)(pos % 3); pol = t <= 1; san = t >= 1; } break;
			case DK_EMPAT: {
				static const int E[8] = {3, 2, 0, 1, 3, 2, 0, 1};
				int o = E[pos % 8];
				pol = o <= 1; san = o >= 2 || o == 0;
			} break;
			default: pol = (pos % 2) == 0; san = !pol; break;
			}
			f.trig[D_POLOS] = pol;
			f.trig[D_SANGSIH] = san;
			if (pol && san) f.trig[D_ACCENT] = true;
		}
	}
};

} // namespace irama
