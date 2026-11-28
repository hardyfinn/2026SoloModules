// Horo: clock and accent sequencer for Bulgarian uneven dance metres. No Rack includes, so the offline tests can use it.
// Spec: docs/HORO-BUILD-INSTRUCTIONS.md (section numbers in the comments refer to it).
#pragma once
#include <cmath>
#include <cstdint>
#include <string>
#include <algorithm>

namespace horo {

static const int MAXP = 32;     // pulses per bar
static const int MAXG = 16;     // groups per bar
static const int NUM_FACTORY = 14;
static const int NUM_USER = 4;
static const int NUM_SLOTS = NUM_FACTORY + NUM_USER;
static const int NUM_BARS = 8;
static const int BARS_VALUES[NUM_BARS] = {1, 2, 3, 4, 6, 8, 12, 16};

enum PhraseMode { LAST_BEAT = 0, LAST_LONG = 1, LAST_PULSE = 2 };

struct Metre {
	int ng = 0;
	int g[MAXG] = {};
	int count() const { int n = 0; for (int i = 0; i < ng; i++) n += g[i]; return n; }
	bool valid() const {
		if (ng < 1 || ng > MAXG) return false;
		for (int i = 0; i < ng; i++) if (g[i] < 1 || g[i] > 9) return false;
		return count() <= MAXP;
	}
	bool operator==(const Metre& o) const {
		if (ng != o.ng) return false;
		for (int i = 0; i < ng; i++) if (g[i] != o.g[i]) return false;
		return true;
	}
	std::string str(const char* sep = " ") const {
		std::string s;
		for (int i = 0; i < ng; i++) { if (i) s += sep; s += char('0' + g[i]); }
		return s;
	}
};

inline Metre makeMetre(std::initializer_list<int> l) {
	Metre m;
	for (int x : l) if (m.ng < MAXG) m.g[m.ng++] = x;
	return m;
}

// Section 2: factory metres (slots 0..13), and the defaults of USER 1..4 (slots 14..17).
static const char* const SLOT_NAMES[NUM_SLOTS] = {
	"PRAVO", "PAIDUSHKO", "RUCHENITSA", "LESNOTO", "ELENINO", "DAICHOVO", "GRANCHARSKO", "KOPANITSA", "PETRUNINO",
	"KRIVO", "BUCHIMISH", "YOVE", "SANDANSKO", "SEDI DONKA", "USER 1", "USER 2", "USER 3", "USER 4"};

inline Metre factoryMetre(int slot) {
	switch (slot) {
		case 0: return makeMetre({3, 3});
		case 1: return makeMetre({2, 3});
		case 2: return makeMetre({2, 2, 3});
		case 3: return makeMetre({3, 2, 2});
		case 4: return makeMetre({2, 2, 1, 2});
		case 5: return makeMetre({2, 2, 2, 3});
		case 6: return makeMetre({2, 3, 2, 2});
		case 7: return makeMetre({2, 2, 3, 2, 2});
		case 8: return makeMetre({3, 2, 2, 3, 2});
		case 9: return makeMetre({2, 2, 2, 3, 2, 2});
		case 10: return makeMetre({2, 2, 2, 2, 3, 2, 2});
		case 11: return makeMetre({3, 2, 2, 2, 2, 3, 2, 2});
		case 12: return makeMetre({2, 2, 2, 3, 2, 2, 2, 3, 2, 2});
		case 13: return makeMetre({3, 2, 2, 3, 2, 2, 2, 2, 3, 2, 2});
		case 14: return makeMetre({3, 3, 2});
		case 15: return makeMetre({2, 3, 3});
		case 16: return makeMetre({3, 2, 3});
		default: return makeMetre({2, 2, 2, 2});
	}
}

// User grouping text: digits, separated by '+', spaces, commas or nothing; other characters ignored.
// Returns false (and leaves out untouched) if the result is empty or breaks the limits.
inline bool parseGrouping(const std::string& s, Metre& out) {
	Metre m;
	for (char c : s) {
		if (c >= '1' && c <= '9') {
			if (m.ng >= MAXG) return false;
			m.g[m.ng++] = c - '0';
		}
		else if (c == '0') return false;
	}
	if (!m.valid()) return false;
	out = m;
	return true;
}

struct Params {
	int slot = 2;              // 0..17, after METRE CV
	double lilt = 0;           // -1..1, after CV
	double accent = 0.8;       // 0..1
	int bars = 4;              // phrase length in bars (a value from BARS_VALUES)
	int phraseMode = LAST_BEAT;
	bool changeAtPhrase = false;
	Metre user[NUM_USER] = {factoryMetre(14), factoryMetre(15), factoryMetre(16), factoryMetre(17)};
	Metre metreFor(int s) const { return s >= NUM_FACTORY ? user[std::min(s - NUM_FACTORY, NUM_USER - 1)] : factoryMetre(s); }
};

struct Inputs {
	double clock = 0;
	double reset = 0;   // the input and the panel button, combined by the caller (max)
};

// Section 3, lilt table: warped pulse starts u[0..N] in units of pulses.
inline void liltTable(const Metre& m, double lilt, double* u, bool* longG) {
	int shortest = 99;
	for (int i = 0; i < m.ng; i++) shortest = std::min(shortest, m.g[i]);
	int PL = 0, PS = 0;
	for (int i = 0; i < m.ng; i++) {
		longG[i] = m.g[i] > shortest;
		if (longG[i]) PL += m.g[i]; else PS += m.g[i];
	}
	double a = 0;
	if (PL > 0 && PS > 0) a = std::max(-0.4, std::min(0.25 * lilt, 0.4 * PS / PL));
	double dl = 1 + a, ds = (PL > 0 && PS > 0) ? 1 - a * PL / PS : 1;
	int k = 0;
	u[0] = 0;
	for (int i = 0; i < m.ng; i++)
		for (int j = 0; j < m.g[i]; j++) { u[k + 1] = u[k] + (longG[i] ? dl : ds); k++; }
	u[k] = k;   // exact bar length
}

struct Engine {
	double sr = 48000, dt = 1.0 / 48000;

	// timing (section 3)
	double T = 0, elapsed = 0;
	bool haveEdge = false, started = false;
	int kIn = 0, kOut = 0, bar = 0;
	bool clkHigh = false, rstHigh = false;

	// latched bar
	Metre m;
	int slot = 2;                 // latched slot
	int N = 7;
	double u[MAXP + 1] = {};
	bool longG[MAXG] = {};
	int grp[MAXP] = {}, pos[MAXP] = {}, gStart[MAXG] = {};
	int phraseK = 0;

	// per-pulse event, fired now or queued
	struct Ev { bool beat, lng, barT, phraseT; double w, pulseFall, beatFall; };
	Ev q[2 * MAXP + 4];
	int qn = 0;
	double sinceFire = 1e9;

	// outputs
	bool pulse = false, beat = false, lng = false;
	double pulseFall = 0, beatFall = 0, lngFall = 0;
	double pulseHigh = 0, beatHigh = 0, lngHigh = 0;   // seconds each gate has been high
	double minHighP = 0, minHighB = 0, minHighL = 0;
	double pulseLow = 1;
	double barT = 0, phraseT = 0;                       // seconds left of the 1 ms triggers
	double accentV = 10;
	int lastK = -1;                                     // index of the last pulse fired (for the display)
	uint32_t barCount = 0, phraseCount = 0, beatCount = 0, pulseCount = 0;   // for tests and lights

	void init(double rate) {
		sr = rate; dt = 1.0 / rate;
		T = 0; elapsed = 0; haveEdge = false; started = false;
		kIn = kOut = bar = 0; clkHigh = rstHigh = false;
		qn = 0; sinceFire = 1e9;
		pulse = beat = lng = false; barT = phraseT = 0; accentV = 10; lastK = -1;
		Params p; latch(p, true);
	}

	void latch(const Params& p, bool metreToo) {
		if (metreToo) {
			Metre nm = p.metreFor(p.slot);
			if (nm.valid()) { m = nm; slot = p.slot; }
		}
		N = m.count();
		liltTable(m, std::max(-1.0, std::min(1.0, p.lilt)), u, longG);
		int k = 0;
		for (int i = 0; i < m.ng; i++) {
			gStart[i] = k;
			for (int j = 0; j < m.g[i]; j++) { grp[k] = i; pos[k] = j; k++; }
		}
		int lastG = m.ng - 1;
		if (p.phraseMode == LAST_PULSE) phraseK = N - 1;
		else {
			int g = lastG;
			if (p.phraseMode == LAST_LONG) { for (int i = lastG; i >= 0; i--) if (longG[i]) { g = i; break; } }
			phraseK = gStart[g];
		}
	}

	// The event for pulse k of the latched bar. late: an old-bar pulse emitted after the bar line (fall at once).
	Ev makeEv(int k, const Params& p, bool late) const {
		Ev e;
		int g = grp[k], j = pos[k], n = m.g[g];
		e.beat = (j == 0);
		e.lng = e.beat && longG[g];
		e.barT = (k == 0);
		e.phraseT = (bar == p.bars - 1 || p.bars <= 1) && k == phraseK;
		double w;
		if (k == 0) w = (bar == 0) ? 1.0 : 0.9;
		else if (j == 0) w = longG[g] ? 0.75 : 0.55;
		else if (j == n - 1 && n >= 3) w = 0.35;
		else w = 0.2;
		e.w = w;
		if (late) { e.pulseFall = e.beatFall = -1e9; }
		else {
			e.pulseFall = u[k] + 0.5 * (u[k + 1] - u[k]);
			int gs = gStart[g], ge = gs + n;
			e.beatFall = u[gs] + 0.5 * (u[ge] - u[gs]);
		}
		return e;
	}

	void push(const Ev& e) { if (qn < (int)(sizeof(q) / sizeof(q[0]))) q[qn++] = e; }

	void fire(const Ev& e, const Params& p) {
		bool tZero = (T <= 0);
		double mh = tZero ? 0.001 : 0.0005;
		pulse = true; pulseHigh = 0; pulseFall = tZero ? -1e9 : e.pulseFall; minHighP = mh;
		pulseCount++;
		if (e.beat) { beat = true; beatHigh = 0; beatFall = tZero ? -1e9 : e.beatFall; minHighB = mh; beatCount++; }
		if (e.lng) { lng = true; lngHigh = 0; lngFall = tZero ? -1e9 : e.beatFall; minHighL = mh; }
		if (e.barT) { barT = 0.001; barCount++; }
		if (e.phraseT) { phraseT = 0.001; phraseCount++; }
		accentV = 10.0 * (1.0 - std::max(0.0, std::min(1.0, p.accent)) * (1.0 - e.w));
		sinceFire = 0;
	}

	void barLine(const Params& p) {
		// queue what the old bar still owes (only after a sudden tempo jump)
		while (kOut < N) { push(makeEv(kOut, p, true)); kOut++; }
		pulseFall -= N; beatFall -= N; lngFall -= N;   // old-bar gates into new-bar coordinates
		kIn = 0; kOut = 0;
		bar = (bar + 1) % std::max(1, p.bars);
		latch(p, !p.changeAtPhrase || bar == 0);
	}

	void startBar0(const Params& p) {
		started = true; kIn = 0; kOut = 0; bar = 0;
		latch(p, true);
	}

	void doReset(const Params& p) {
		pulse = beat = lng = false; barT = phraseT = 0; qn = 0;
		pulseFall = beatFall = lngFall = 0;
		// A clock edge that came within 1 ms before the reset is taken as pulse 0 (clock and reset from one source).
		if (haveEdge && elapsed < 0.001) startBar0(p);
		else started = false;
	}

	void process(const Params& p, const Inputs& in) {
		// RESET
		if (!rstHigh && in.reset >= 1.0) { rstHigh = true; doReset(p); }
		else if (rstHigh && in.reset <= 0.1) rstHigh = false;

		// CLOCK edge
		bool edge = false;
		if (!clkHigh && in.clock >= 1.0) { clkHigh = true; edge = true; }
		else if (clkHigh && in.clock <= 0.1) clkHigh = false;
		if (edge) {
			if (haveEdge && elapsed < 2.0) {
				if (T <= 0 || std::fabs(elapsed - T) > 0.2 * T) T = elapsed;
				else T = 0.5 * T + 0.5 * elapsed;
			}
			elapsed = 0; haveEdge = true;
			if (!started) startBar0(p);
			else { kIn++; if (kIn >= N) barLine(p); }
		}

		// pulses due
		bool wasEmpty = (qn == 0);
		double phi = 0;
		if (started) {
			phi = kIn + (T > 0 ? std::min(elapsed / T, 0.999) : 0.0);
			while (kOut < N && phi >= u[kOut]) { push(makeEv(kOut, p, false)); lastK = kOut; kOut++; }
		}
		if (qn > 0) {
			// A rise needs the gate low first; a backlog goes out one per millisecond with the gate low 0.5 ms between.
			bool ok = !pulse && (wasEmpty ? (sinceFire >= 0.0005) : (sinceFire >= 0.001 && pulseLow >= 0.0005));
			if (ok) {
				fire(q[0], p);
				for (int i = 1; i < qn; i++) q[i - 1] = q[i];
				qn--;
			}
		}

		// gate falls
		if (pulse && phi >= pulseFall && pulseHigh >= minHighP) pulse = false;
		if (beat && phi >= beatFall && beatHigh >= minHighB) beat = false;
		if (lng && phi >= lngFall && lngHigh >= minHighL) lng = false;
		if (qn > 0 && pulse && pulseHigh >= 0.0005) pulse = false;   // make room for a queued pulse
		// clock stopped
		if (T > 0 && elapsed > std::max(4 * T, 0.25)) pulse = beat = lng = false;

		pulseHigh += dt; beatHigh += dt; lngHigh += dt;
		pulseLow = pulse ? 0 : pulseLow + dt;
		barT = std::max(0.0, barT - dt);
		phraseT = std::max(0.0, phraseT - dt);
		sinceFire += dt;
		elapsed += dt;
	}

	double gateV(bool b) const { return b ? 10.0 : 0.0; }
	bool barOut() const { return barT > 0; }
	bool phraseOut() const { return phraseT > 0; }
	int pendingSlot(const Params& p) const { return p.slot; }
};

}  // namespace horo
