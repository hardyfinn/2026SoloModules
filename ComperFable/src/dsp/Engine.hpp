// Comper engine: a line-by-line port of tools/proto/comper_ref.py (the definition).
// Same tables, same order of operations and random draws, doubles everywhere, so the
// same seed and inputs give the same notes (checked by make test against test/golden.json).
// No Rack includes.
#pragma once
#include <cstdint>
#include <cmath>
#include <vector>
#include <algorithm>
#include <functional>
#include <utility>
#include <initializer_list>

namespace comper {

// ------------------------------------------------------------------ random numbers
struct Rng {
	uint64_t s = 0;
	Rng() {}
	explicit Rng(int64_t seed) {
		s = ((uint64_t)seed * 2654435761ULL + 12345ULL) & 0xFFFFFFFFULL;
		for (int i = 0; i < 4; i++) u();
	}
	double u() {
		s = (s * 1664525ULL + 1013904223ULL) & 0xFFFFFFFFULL;
		return (double)(s >> 8) / 16777216.0;
	}
	double uni(double a, double b) { return a + (b - a) * u(); }
	int n(int k) { return (int)(u() * k); }
	double g() {
		double a = u(); a += u(); a += u(); a += u();
		return (a - 2.0) * 1.7320508075688772;
	}
};

// ------------------------------------------------------------------ chords
// TRI and MIN (plain triads) were added in 2.3.0 for reggae and pop-shaped progressions; they come last
// so saved patches keep their qualities.
enum Quality { MAJ, MAJ6, M7, M6, MMAJ, DOM, ALT, HDIM, DIM, SUS, TRI, MIN, NUM_Q };
static const int NUM_Q_JAZZ = 10;   // the qualities the reference engine and POLY IN recognition know
static const char* const Q_NAMES[NUM_Q] = {"maj7", "6", "m7", "m6", "mMaj7", "7", "7alt", "m7b5", "dim7", "7sus", "", "m"};
static const char* const NOTE_NAMES[12] = {"C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B"};
static inline bool minorFamily(int q) { return q == M7 || q == M6 || q == MMAJ || q == HDIM || q == MIN; }

struct Chord {
	int root = 0, q = MAJ;
	Chord() {}
	Chord(int r, int qq) : root(r), q(qq) {}
	bool operator==(const Chord& o) const { return root == o.root && q == o.q; }
	bool operator!=(const Chord& o) const { return !(*this == o); }
};

enum Style { SHELL, ROOTLESS, QUARTAL, SPREADV, MIXED, COLOR, NUM_STYLE_SLOTS };
static const char* const STYLE_NAMES[5] = {"SHELL", "ROOTLESS", "QUARTAL", "SPREAD", "MIXED"};

typedef std::vector<int> Notes;
typedef std::vector<Notes> RecipeList;

struct Tables {
	RecipeList rec[NUM_Q][NUM_STYLE_SLOTS];
	Tables() {
		auto S = [&](int q, int st, RecipeList l) { rec[q][st] = l; };
		S(MAJ, SHELL, {{4,11},{11,4},{4,11,2},{11,4,7}});
		S(MAJ, ROOTLESS, {{4,7,11,2},{11,2,4,7},{4,9,11,2}});
		S(MAJ, QUARTAL, {{4,9,2},{9,2,7},{11,4,9}});
		S(MAJ, SPREADV, {{11,4,9,2,7},{4,9,2,7,11},{4,11,2,7}});
		S(MAJ, COLOR, {{4,11,2,6},{11,2,4,6,9}});
		S(MAJ6, SHELL, {{4,9},{9,4},{4,9,2}});
		S(MAJ6, ROOTLESS, {{4,7,9,2},{9,2,4,7}});
		S(MAJ6, QUARTAL, {{4,9,2},{9,2,7}});
		S(MAJ6, SPREADV, {{4,9,2,7},{9,4,7,2}});
		S(M7, SHELL, {{3,10},{10,3},{3,10,2},{10,3,7}});
		S(M7, ROOTLESS, {{3,7,10,2},{10,2,3,7}});
		S(M7, QUARTAL, {{5,10,3},{7,0,5},{2,7,0}});
		S(M7, SPREADV, {{0,5,10,3,7},{3,10,2,5,7},{10,3,7,2,5}});
		S(M6, SHELL, {{3,9},{9,3},{3,9,2}});
		S(M6, ROOTLESS, {{3,7,9,2},{9,2,3,7}});
		S(M6, QUARTAL, {{3,9,2},{9,2,7}});
		S(M6, SPREADV, {{3,9,2,7},{9,3,7,2}});
		S(MMAJ, SHELL, {{3,11},{11,3}});
		S(MMAJ, ROOTLESS, {{3,7,11,2},{11,2,3,7}});
		S(MMAJ, QUARTAL, {{3,7,11,2}});
		S(MMAJ, SPREADV, {{3,11,2,7}});
		S(DOM, SHELL, {{4,10},{10,4},{4,10,2},{10,4,9}});
		S(DOM, ROOTLESS, {{4,9,10,2},{10,2,4,9}});
		S(DOM, QUARTAL, {{10,4,9},{4,9,2}});
		S(DOM, SPREADV, {{10,4,9,2,7},{4,10,2,7,9}});
		S(DOM, COLOR, {{4,10,2,6,9},{10,4,9,2,6}});
		S(ALT, SHELL, {{4,10,3},{10,4,8}});
		S(ALT, ROOTLESS, {{4,8,10,3},{10,1,4,8}});
		S(ALT, QUARTAL, {{10,3,8},{4,10,3}});
		S(ALT, SPREADV, {{4,10,8,0,3},{10,4,8,1,3}});
		S(HDIM, SHELL, {{3,10},{10,3},{6,10,3}});
		S(HDIM, ROOTLESS, {{3,6,10,0},{10,0,3,6},{6,10,3,5}});
		S(HDIM, QUARTAL, {{5,10,3},{6,10,3,5}});
		S(HDIM, SPREADV, {{6,10,3,5},{3,10,5,6,0}});
		S(DIM, SHELL, {{3,9},{9,3},{6,0}});
		S(DIM, ROOTLESS, {{3,6,9,0},{9,0,3,6},{6,9,0,3}});
		S(DIM, QUARTAL, {{3,6,9,0}});
		S(DIM, SPREADV, {{3,9,0,6},{9,3,6,0}});
		S(SUS, SHELL, {{5,10},{10,5,2}});
		S(SUS, ROOTLESS, {{5,9,10,2},{10,2,5,9}});
		S(SUS, QUARTAL, {{10,2,5},{2,5,10}});
		S(SUS, SPREADV, {{10,5,9,2,7},{5,10,2,7,9}});
		// plain triads: the three inversions, doubled at the top for fuller styles
		S(TRI, SHELL, {{0,4,7},{4,7,0},{7,0,4}});
		S(TRI, ROOTLESS, {{4,7,0,4},{7,0,4,7},{0,4,7,0}});
		S(TRI, QUARTAL, {{7,0,4},{4,7,0}});
		S(TRI, SPREADV, {{0,7,4,7},{4,0,7,0},{7,4,0,4}});
		S(MIN, SHELL, {{0,3,7},{3,7,0},{7,0,3}});
		S(MIN, ROOTLESS, {{3,7,0,3},{7,0,3,7},{0,3,7,0}});
		S(MIN, QUARTAL, {{7,0,3},{3,7,0}});
		S(MIN, SPREADV, {{0,7,3,7},{3,0,7,0},{7,3,0,3}});
	}
};
static inline const Tables& tables() { static Tables t; return t; }

static const int WINDOW_LO[NUM_STYLE_SLOTS] = {48, 50, 52, 44, 0, 46};
static const int WINDOW_HI[NUM_STYLE_SLOTS] = {64, 67, 68, 58, 0, 60};
static const int GUIDES[NUM_Q][2] = {{4,11},{4,9},{3,10},{3,9},{3,11},{4,10},{4,10},{3,10},{3,9},{5,10},{4,7},{3,7}};
static const int SCALE_LEN[NUM_Q] = {7, 5, 7, 7, 7, 6, 7, 7, 8, 6, 7, 7};
static const int SCALES[NUM_Q][8] = {
	{0,2,4,6,7,9,11}, {0,2,4,7,9}, {0,2,3,5,7,9,10}, {0,2,3,5,7,9,11}, {0,2,3,5,7,9,11},
	{0,2,4,7,9,10}, {0,1,3,4,6,8,10}, {0,1,3,5,6,8,10}, {0,2,3,5,6,8,9,11}, {0,2,5,7,9,10},
	{0,2,4,5,7,9,11}, {0,2,3,5,7,9,10}};

static inline int pmod(int a, int m) { int r = a % m; return r < 0 ? r + m : r; }

static inline void realize(int root, const Notes& recipe, int lo, int hi, std::vector<Notes>& out) {
	out.clear();
	int pc0 = pmod(root + recipe[0], 12);
	for (int b = lo; b <= hi; b++) {
		if (pmod(b, 12) != pc0) continue;
		Notes v; v.push_back(b);
		for (size_t i = 1; i < recipe.size(); i++) {
			int n = v.back() + 1;
			while (pmod(n, 12) != pmod(root + recipe[i], 12)) n++;
			v.push_back(n);
		}
		out.push_back(v);
	}
}

static inline double mud(const Notes& v, double topSoft) {
	double p = 0.0;
	for (size_t i = 0; i + 1 < v.size(); i++) {
		int a = v[i]; int d = v[i + 1] - a;
		if (d <= 2 && a < 55) p += 6;
		if (d <= 4 && a < 47) p += 6;
		if (d == 1) p += 2;
	}
	double top = v.back();
	if (top > topSoft + 10) p += 3 * (top - topSoft - 10);
	if (top < topSoft - 10) p += 2 * (topSoft - 10 - top);
	return p;
}

static inline double moveCost(const Notes& v, const Notes* p) {
	if (!p) return 0.0;
	if (v.size() == p->size()) {
		int s = 0;
		for (size_t i = 0; i < v.size(); i++) s += std::abs(v[i] - (*p)[i]);
		return (double)s;
	}
	int s = 0;
	for (int a : v) {
		int m = 999;
		for (int b : *p) if (std::abs(a - b) < m) m = std::abs(a - b);
		s += m;
	}
	return (double)s * (double)std::max(v.size(), p->size()) / (double)v.size();
}

static inline Notes limitVoices(Notes v, int root, int q, int nmax) {
	int g0 = pmod(root + GUIDES[q][0], 12), g1 = pmod(root + GUIDES[q][1], 12);
	auto isG = [&](int x) { int pc = pmod(x, 12); return pc == g0 || pc == g1; };
	while ((int)v.size() > nmax) {
		int idx = -1;
		for (int i = 1; i < (int)v.size() - 1; i++) if (!isG(v[i])) { idx = i; break; }
		if (idx < 0) {
			int cand[2] = {(int)v.size() - 1, 0};
			for (int i : cand) if (!isG(v[i])) { idx = i; break; }
		}
		if (idx < 0) idx = (int)v.size() - 1;
		v.erase(v.begin() + idx);
	}
	return v;
}

// ------------------------------------------------------------------ rhythm cells
enum { S_ = 0, L_ = 1 };
// REGGAE (2.3.0): straight eighths, chords chopped on the offbeat (the skank) and on 2 and 4.
enum Groove { SWING, BOSSA, BALLAD, SOUL, HOUSE, REGGAE, NUM_GROOVES };
static const char* const GROOVE_NAMES[NUM_GROOVES] = {"SWING", "BOSSA", "BALLAD", "SOUL", "HOUSE", "REGGAE"};
static const int GROOVE_SLOTS[NUM_GROOVES] = {8, 8, 8, 16, 16, 8};

typedef std::pair<int, int> Hit;           // (slot, long?)
typedef std::vector<Hit> Cell;

struct CellTables {
	std::vector<Cell> pool[NUM_GROOVES][3];
	CellTables() {
		auto P = [&](int g, int d, std::vector<Cell> c) { pool[g][d] = c; };
		const int S = S_, L = L_;
		P(SWING, 0, {{}, {{0,L}}, {{7,L}}, {{3,S}}, {{3,L}}});
		P(SWING, 1, {{{0,S},{3,L}}, {{1,S},{4,L}}, {{3,L},{7,L}}, {{2,S},{5,L}}, {{1,L},{5,S}}, {{3,S},{7,L}}, {{0,L},{7,L}}});
		P(SWING, 2, {{{0,S},{3,S},{6,S}}, {{3,S},{5,S},{7,L}}, {{1,S},{3,L},{7,L}}, {{0,S},{3,L},{5,S},{7,L}}, {{1,S},{2,S},{5,L}}});
		P(BOSSA, 0, {{{0,L}}, {{0,L},{5,S}}, {{3,L}}});
		P(BOSSA, 1, {{{0,S},{3,L},{6,S}}, {{1,L},{4,S},{7,L}}, {{0,L},{3,S},{5,L}}, {{2,S},{5,L}}});
		P(BOSSA, 2, {{{0,S},{3,S},{5,S},{6,S}}, {{1,S},{3,S},{4,S},{7,L}}});
		P(BALLAD, 0, {{{0,L}}, {{0,L}}, {{0,L},{4,L}}});
		P(BALLAD, 1, {{{0,L},{5,S}}, {{0,L},{4,L}}, {{0,L},{7,L}}, {{0,L},{3,S},{4,L}}});
		P(BALLAD, 2, {{{0,L},{3,S},{5,S},{7,L}}, {{0,L},{2,S},{4,L},{7,L}}});
		P(SOUL, 0, {{{0,L}}, {{3,L}}, {{6,S},{10,L}}});
		P(SOUL, 1, {{{0,S},{3,S},{10,L}}, {{3,S},{6,L},{14,S}}, {{0,L},{7,S},{10,S}}, {{2,S},{6,S},{11,L}}});
		P(SOUL, 2, {{{0,S},{3,S},{6,S},{10,S},{13,S}}, {{2,S},{3,S},{7,S},{10,L},{15,S}}});
		P(HOUSE, 0, {{{2,S},{10,S}}, {{0,L}}, {{6,S},{14,S}}});
		P(HOUSE, 1, {{{2,S},{6,S},{10,S},{14,S}}, {{0,S},{3,S},{6,S},{10,S},{13,S}}, {{3,S},{6,S},{11,S},{14,S}}, {{0,S},{3,S},{6,L}}});
		P(HOUSE, 2, {{{0,S},{3,S},{6,S},{8,S},{11,S},{14,S}}, {{2,S},{5,S},{6,S},{10,S},{13,S},{14,S}}});
		P(REGGAE, 0, {{{2,S},{6,S}}, {{1,S},{3,S},{5,S},{7,S}}, {{2,S},{6,S}}});
		P(REGGAE, 1, {{{1,S},{3,S},{5,S},{7,S}}, {{1,S},{2,S},{5,S},{6,S}}, {{2,S},{3,S},{6,S},{7,S}}, {{1,S},{3,S},{5,S},{7,S}}});
		P(REGGAE, 2, {{{1,S},{2,S},{3,S},{5,S},{6,S},{7,S}}, {{1,S},{3,S},{4,S},{5,S},{7,S}}});
	}
};
static inline const CellTables& cells() { static CellTables t; return t; }

static inline const std::vector<int>& upbeats(int slots) {
	static const std::vector<int> a = {1, 3, 5, 7}, b = {2, 6, 10, 14};
	return slots == 8 ? a : b;
}
static inline const std::vector<int>& addSlots(int slots) {
	static const std::vector<int> a = {1, 3, 5, 7}, b = {3, 6, 10, 14};
	return slots == 8 ? a : b;
}

static inline double autoSwing(double bpm) {
	return std::max(1.15, std::min(2.2, 2.05 - (bpm - 120.0) / 170.0));
}

// ------------------------------------------------------------------ parameters
struct Params {
	int style = ROOTLESS;
	double spread = 0.25;
	double reg = 69.0;
	int voices = 5;
	double lead = 1.0;
	double out = 0.0;
	int roots = 0;
	int groove = SWING;
	double density = 0.5;
	double swing = 0.0;
	double repeat = 0.6;
	double push = 0.85;
	double loose = 1.0;
	double timing = 1.0, roll = 1.0, weight = 1.0, slop = 1.0;
	double pedal = 0.0;
	double listen = 0.0;
	// 2.3.0 (host side, not in the reference engine): human note lengths. Off = the reference lengths.
	bool human = false;
	double length = 0.6;           // 0 short, 1 legato
};

struct Ctx {
	double spb = 0.5;
	double leadS = 0.06;
	std::function<Chord(double)> chordAt;
	std::function<bool(double, Chord&)> nextChange;   // empty = "None"
	bool soloConnected = false;
	bool soloSounding = false;
	double soloGap = 99.0;
	bool soloRecent = false;
	bool hasSoloLow = false;
	double soloLow = 0.0;
};

struct Ev {
	double t; int pitch; int vel; bool lng; double dur; double extra;
};
struct Result {
	std::vector<Ev> notes;
	std::vector<std::pair<double, double>> pedal;
	bool hit = false;
	int kind = -1;
	Chord chord;
};

// ------------------------------------------------------------------ the player
struct Engine {
	Params p;
	Rng rr, rv, rf;
	bool hasPrev = false;
	Notes prev;
	bool hasPrevChord = false;
	Chord prevChord;
	bool hasTarget = false;
	double target = 0.0;
	bool hasMotif = false;
	Cell motif[2];
	int motifGroove = 0;
	int curStyle = ROOTLESS;
	int cellmap[16];                 // -1 none, else S_/L_
	bool pushOk[16];
	double slotBeats[16];
	int slots = 8;
	double dyn = 0.0;
	double lastHitBeat = -99.0;
	bool hasPending = false;
	int pendBar = 0, pendSlot = 0;
	Notes pendV;
	Chord pendChord;
	bool hasRootChord = false;
	Chord rootChord;
	bool firstBar = true;
	bool begun = false;

	Engine(int64_t seed = 1) { reset(seed); }

	void reset(int64_t seed) {
		rr = Rng(seed * 3 + 0); rv = Rng(seed * 3 + 1); rf = Rng(seed * 3 + 2);
		hasPrev = false; prev.clear(); hasPrevChord = false; hasTarget = false;
		hasMotif = false; curStyle = ROOTLESS;
		for (int i = 0; i < 16; i++) { cellmap[i] = -1; pushOk[i] = false; slotBeats[i] = 0.0; }
		slots = 8; dyn = 0.0; lastHitBeat = -99.0; hasPending = false; hasRootChord = false;
		firstBar = true; begun = false;
	}

	// ---- rhythm
	Cell pickCell() {
		const auto& C = cells().pool[p.groove]; double d = p.density;
		double w0 = std::max(0.0, 1 - 2 * d) + 0.08;
		double w1 = 1 - std::fabs(2 * d - 1) + 0.08;
		double w2 = std::max(0.0, 2 * d - 1) + 0.08;
		double r = rr.u() * (w0 + w1 + w2);
		const std::vector<Cell>& pool = r < w0 ? C[0] : (r < w0 + w1 ? C[1] : C[2]);
		return pool[rr.n((int)pool.size())];
	}
	static bool hasSlot(const Cell& c, int s) {
		for (auto& h : c) if (h.first == s) return true;
		return false;
	}
	Cell mutate(Cell cell) {
		double r = rr.u();
		if (!cell.empty() && r < 0.35) {
			cell.erase(cell.begin() + rr.n((int)cell.size()));
		} else if (!cell.empty() && r < 0.7) {
			int i = rr.n((int)cell.size()); int s = cell[i].first, l = cell[i].second;
			int s2 = pmod(s + (rr.u() < 0.5 ? -1 : 1), slots);
			if (!hasSlot(cell, s2)) cell[i] = Hit(s2, l);
		} else {
			const auto& a = addSlots(slots); int s2 = a[rr.n((int)a.size())];
			if (!hasSlot(cell, s2)) cell.push_back(Hit(s2, S_));
		}
		std::sort(cell.begin(), cell.end());
		return cell;
	}
	int pickStyle() {
		if (p.style != MIXED) return p.style;
		double r = rr.u();
		return r < 0.4 ? ROOTLESS : r < 0.55 ? SHELL : r < 0.75 ? QUARTAL : SPREADV;
	}

	void beginBar(int bar, double bpm) {
		int sl = GROOVE_SLOTS[p.groove];
		if (firstBar || sl != slots || !hasMotif || p.groove != motifGroove) {
			slots = sl;
			Cell a = pickCell(); Cell b = pickCell();
			motif[0] = a; motif[1] = b; hasMotif = true;
			motifGroove = p.groove;
			curStyle = pickStyle();
			firstBar = false;
		} else if (bar % 2 == 0) {
			if (rr.u() > p.repeat) {
				Cell a = pickCell(); Cell b = pickCell();
				motif[0] = a; motif[1] = b;
			} else if (rr.u() < 0.6) {
				int k = rr.n(2); motif[k] = mutate(motif[k]);
			}
			curStyle = pickStyle();
		}
		if (p.style != MIXED) curStyle = p.style;
		for (int i = 0; i < 16; i++) cellmap[i] = -1;
		for (auto& h : motif[bar % 2]) cellmap[h.first] = h.second;
		for (int i = 0; i < slots; i++) pushOk[i] = rr.u() < p.push;
		if (slots == 8) {
			double base = (p.groove == SWING || p.groove == BALLAD) ? autoSwing(bpm) : 1.0;
			double ratio = std::max(1.0, std::min(3.0, base * std::pow(2.0, p.swing * 0.6)));
			double f = ratio / (1.0 + ratio);
			for (int s = 0; s < 8; s++) slotBeats[s] = (double)(s / 2) + ((s % 2) ? f : 0.0);
		} else {
			double sw = std::max(0.5, std::min(0.67, (p.groove == SOUL ? 0.54 : 0.5) + 0.12 * p.swing));
			const double sub[4] = {0.0, sw / 2, 0.5, 0.5 + sw / 2};
			for (int s = 0; s < 16; s++) slotBeats[s] = (double)(s / 4) + sub[s % 4];
		}
		begun = true;
	}

	bool onUp(int slot) const { return slots == 8 ? (slot % 2 == 1) : (slot % 4 != 0); }

	// ---- harmony
	Chord chordFor(int bar, int slot, Ctx& ctx, bool& antic) {
		double beat = bar * 4 + slotBeats[slot];
		bool up = onUp(slot);
		double look = (up && pushOk[slot]) ? std::floor(beat) + 1 : beat;
		Chord chord = ctx.chordAt(look);
		antic = up && chord != ctx.chordAt(beat);
		double u1 = rv.u(); double u2 = rv.u(); double out = p.out;
		if (chord.q == DOM || chord.q == ALT) {
			Chord nxt; bool hasN = ctx.nextChange && ctx.nextChange(look, nxt);
			bool resolves = hasN && pmod(nxt.root - chord.root, 12) == 5;
			double pTri = resolves ? 0.3 * out : (!hasN ? 0.15 * out : 0.0);
			if (u1 < pTri) {
				chord = Chord(pmod(chord.root + 6, 12), DOM);
			} else if (chord.q == DOM) {
				double pAlt;
				if (resolves && minorFamily(nxt.q)) pAlt = std::min(1.0, 1.5 * out);
				else if (resolves || !hasN) pAlt = 0.8 * out;
				else pAlt = 0.3 * out;
				if (u2 < pAlt) chord = Chord(chord.root, ALT);
			}
		}
		return chord;
	}

	Notes pickVoicing(Chord chord, Ctx& ctx) {
		int root = chord.root, q = chord.q;
		double centre = p.reg;
		if (!hasTarget) { target = centre; hasTarget = true; }
		target += rv.g() * 0.8;
		target = std::min(centre + 7, std::max(centre - 5, target));
		if (ctx.hasSoloLow && p.listen > 0)
			target = std::min(target, std::max(58.0, ctx.soloLow - 4.0));
		int shift = (int)std::floor(centre - 69.0 + 0.5);
		const Tables& T = tables();
		int style = curStyle;
		struct Rec { const Notes* r; int st; };
		std::vector<Rec> recs;
		for (auto& r : T.rec[q][style]) recs.push_back({&r, style});
		double uc = rv.u();
		if (!T.rec[q][COLOR].empty() && uc < p.out)
			for (auto& r : T.rec[q][COLOR]) recs.push_back({&r, COLOR});
		struct Cand { Notes v; bool opened; };
		std::vector<Cand> cands;
		std::vector<Notes> real;
		for (auto& rc : recs) {
			realize(root, *rc.r, WINDOW_LO[rc.st] + shift, WINDOW_HI[rc.st] + shift, real);
			for (auto& v : real) {
				cands.push_back({limitVoices(v, root, q, p.voices), false});
				if (p.spread > 0.05 && v.size() >= 4) {
					Notes d = v; int x = d[d.size() - 2] - 12; d.erase(d.begin() + (d.size() - 2));
					if (x >= 43 + shift) {
						d.push_back(x); std::sort(d.begin(), d.end());
						cands.push_back({limitVoices(d, root, q, p.voices), true});
					}
				}
			}
		}
		bool same = hasPrevChord && chord == prevChord;
		bool stay = rv.u() < 0.7;
		double bias = 8.0 * (p.spread - 0.5);
		const Cand* best = nullptr; double bestk = 1e18;
		for (auto& c : cands) {
			const Notes& v = c.v;
			double k = p.lead * (moveCost(v, hasPrev ? &prev : nullptr) + (hasPrev ? 1.5 * std::abs(v.back() - prev.back()) : 0.0));
			k += 1.1 * std::fabs(v.back() - target) + mud(v, centre);
			if (same && hasPrev && v == prev) k += stay ? -4.0 : 6.0;
			k += (c.opened ? std::max(0.0, -bias) : std::max(0.0, bias));
			k += rv.u() * (1.8 + 10.0 * (1.0 - p.lead));
			if (k < bestk) { bestk = k; best = &c; }
		}
		prev = best->v; hasPrev = true; prevChord = chord; hasPrevChord = true;
		return prev;
	}

	// ---- one chord, as a hand would play it
	void emit(std::vector<Ev>& ev, const Notes& v, int kind, double acc, double boost, bool up, Ctx& ctx,
	          bool low, bool hasDur, double durBeats) {
		double T = p.loose * p.timing, R = p.loose * p.roll, W = p.loose * p.weight, S = p.loose * p.slop;
		double Wc = std::min(W, 1.0);
		double lay = T * (up ? 0.006 : 0.012) + rf.g() * 0.006 * T;
		double roll = rf.uni(0.004, 0.018) * R * (p.groove == BALLAD ? 3.5 : 1.0);
		double mode = rf.u();
		int n = (int)v.size();
		for (int k = 0; k < n; k++) {
			double off;
			if (n == 1) off = 0.0;
			else if (mode < 0.7) off = roll * k / (n - 1);
			else if (mode < 0.85) off = roll * (n - 1 - k) / (n - 1);
			else off = rf.uni(0.0, roll);
			off += rf.g() * 0.002 * R;
			double shape = n == 1 ? 0.0 : (k == n - 1 ? 7.0 : k == 0 ? 2.0 : -5.0);
			double vel = (76.0 - 12.0 * Wc) + Wc * (acc + shape) + boost + rf.g() * 4.0 * std::min(W, 1.5);
			if (low) off -= 0.004 * T;
			int iv = (int)std::max(18.0, std::min(120.0, vel));
			double extra = rf.uni(0.0, 0.02) * S;
			double t = std::max(-ctx.leadS + 0.001, lay + off);
			Ev e; e.t = t; e.pitch = v[k]; e.vel = iv; e.lng = (kind == L_ && !hasDur);
			e.dur = hasDur ? durBeats * ctx.spb : 0.0; e.extra = extra;
			ev.push_back(e);
		}
	}

	Result strike(int bar, int slot, int kind, double boost, Ctx& ctx, bool follow = false) {
		double beat = bar * 4 + slotBeats[slot];
		bool up = follow ? false : onUp(slot);
		Notes v; Chord chord; bool antic = false;
		if (hasPending && pendBar == bar && pendSlot == slot) {
			v = pendV; chord = pendChord; antic = false;
		} else if (follow) {
			chord = ctx.chordAt(beat); antic = false; v = pickVoicing(chord, ctx);
		} else {
			chord = chordFor(bar, slot, ctx, antic);
			v = pickVoicing(chord, ctx);
		}
		hasPending = false;
		double S = p.loose * p.slop;
		double dur = rf.uni(0.28, 0.45);
		dyn = std::max(-10.0, std::min(10.0, dyn + rf.g() * 2.5));
		double acc = ((kind == L_ && up) ? 12.0 : antic ? 4.0 : 0.0) + dyn;
		if (rf.u() < 0.06 * S) { acc -= 25.0; kind = S_; }
		double u = rf.u(); double u2 = rf.u();
		if (kind == S_ && v.size() >= 4 && u < 0.25 * std::min(1.0, S))
			v.erase(v.begin() + (1 + (int)(u2 * (double)(v.size() - 2))));
		Result res;
		emit(res.notes, v, kind, acc, boost, up, ctx, false, kind == S_, dur);
		double ur = rf.u();
		bool wantRoot = p.roots == 2 || (p.roots == 1 && ur < 0.5);
		if (wantRoot && (slot == 0 || !hasRootChord || chord != rootChord)) {
			int r = 36 + chord.root;
			if (r > 43) r -= 12;
			Notes one = {r};
			emit(res.notes, one, kind, acc - 4.0, boost, up, ctx, true, kind == S_, dur);
			rootChord = chord; hasRootChord = true;
		}
		if (p.pedal > 0) {
			double t0 = 1e18;
			for (auto& e : res.notes) t0 = std::min(t0, e.t);
			res.pedal.push_back({t0, 0.0});
			if (kind == L_) res.pedal.push_back({t0 + 0.07, p.pedal});
		}
		lastHitBeat = beat;
		res.hit = true; res.kind = kind; res.chord = chord;
		return res;
	}

	Result decide(int bar, int slot, Ctx& ctx) {
		int kind = cellmap[slot];
		bool promised = hasPending && pendBar == bar && pendSlot == slot;
		bool extra = false;
		if (kind < 0 && p.listen > 0 && ctx.soloConnected) {
			for (int s : upbeats(slots)) if (s == slot) extra = true;
		}
		bool hit = false; double boost = 0.0;
		if (promised) {
			hit = true;
			if (kind < 0) kind = S_;
		} else if (kind >= 0 || extra) {
			if (p.listen > 0 && ctx.soloConnected) {
				if (ctx.soloSounding) {
					if (!extra) {
						hit = !(rr.u() < 0.8 * p.listen);
						if (hit) { kind = S_; boost = -12.0; }
					}
				} else if (ctx.soloRecent && ctx.soloGap < 2.5) {
					if (extra) {
						hit = !(rr.u() > 0.55 * p.listen);
						if (hit) kind = S_;
					} else hit = true;
					if (hit) boost = 8.0;
				} else hit = !extra;
			} else hit = !extra;
		}
		if (hit) return strike(bar, slot, kind, boost, ctx);
		int nxt = slot + 1;
		if (p.out > 0 && nxt < slots && cellmap[nxt] >= 0 && !hasPending
		    && bar * 4 + slotBeats[nxt] - lastHitBeat > 0.9
		    && !(p.listen > 0 && ctx.soloConnected && ctx.soloSounding)) {
			if (rv.u() < 0.35 * p.out) {
				bool antic;
				Chord chord = chordFor(bar, nxt, ctx, antic);
				Notes v = pickVoicing(chord, ctx);
				int sh = rv.u() < 0.5 ? 1 : -1;
				hasPending = true; pendBar = bar; pendSlot = nxt; pendV = v; pendChord = chord;
				Notes slipped = v;
				for (auto& x : slipped) x += sh;
				Result res;
				emit(res.notes, slipped, S_, -10.0 + dyn, 0.0, onUp(slot), ctx, false, true, 0.3);
				res.hit = false; res.kind = S_; res.chord = chord;
				return res;
			}
		}
		return Result();
	}

	double capBeats() const { return p.groove == BALLAD ? 3.8 : 2.6; }
};

// ------------------------------------------------------------------ chord recognition (POLY IN)
static inline int maskOf(std::initializer_list<int> l) { int m = 0; for (int x : l) m |= 1 << x; return m; }
static inline int popc(int m) { int c = 0; while (m) { c += m & 1; m >>= 1; } return c; }

// notes: MIDI numbers sounding. Returns false for "None".
static inline bool detectChord(const std::vector<int>& notes, int lastQuality, Chord& out) {
	if (notes.empty()) return false;
	int lo = notes[0];
	for (int n : notes) lo = std::min(lo, n);
	int low = pmod(lo, 12);
	int pcm = 0;
	for (int n : notes) pcm |= 1 << pmod(n, 12);
	std::vector<int> pcs;
	for (int i = 0; i < 12; i++) if (pcm & (1 << i)) pcs.push_back(i);
	if (pcs.size() == 1) { out = Chord(low, lastQuality); return true; }
	if (pcs.size() == 2) {
		int iv = 0;
		for (int x : pcs) if (x != low) { iv = pmod(x - low, 12); break; }
		int q;
		switch (iv) {
			case 3: q = M7; break; case 4: q = MAJ6; break; case 10: q = DOM; break; case 11: q = MAJ; break;
			case 9: q = MAJ6; break; case 6: q = DIM; break; case 5: q = SUS; break; case 8: q = ALT; break;
			default: q = lastQuality;
		}
		out = Chord(low, q); return true;
	}
	struct Templ { int q, guides, other, allowed; };
	static const Templ TEMPL[10] = {
		{MAJ6, maskOf({4, 9}), maskOf({0, 7}), maskOf({2})},
		{MAJ, maskOf({4, 11}), maskOf({0, 7}), maskOf({2, 6, 9})},
		{M7, maskOf({3, 10}), maskOf({0, 7}), maskOf({2, 5})},
		{M6, maskOf({3, 9}), maskOf({0, 7}), maskOf({2, 5})},
		{MMAJ, maskOf({3, 11}), maskOf({0, 7}), maskOf({2})},
		{DOM, maskOf({4, 10}), maskOf({0, 7}), maskOf({2, 9, 6})},
		{ALT, maskOf({4, 10}), maskOf({0}), maskOf({1, 3, 8, 6})},
		{HDIM, maskOf({3, 10}), maskOf({0, 6}), maskOf({5, 8, 2})},
		{DIM, maskOf({3, 9}), maskOf({0, 6}), maskOf({11, 2})},
		{SUS, maskOf({5, 10}), maskOf({0, 7}), maskOf({2, 9})}};
	bool found = false; double bests = -1e9; Chord best;
	for (int r : pcs) {
		int rel = 0;
		for (int x : pcs) rel |= 1 << pmod(x - r, 12);
		for (int i = 0; i < 10; i++) {
			const Templ& t = TEMPL[i];
			double s = 3.0 * popc(rel & t.guides) + 1.0 * popc(rel & t.other);
			s -= 3.0 * popc(rel & ~t.guides & ~t.other & ~t.allowed);
			if (t.q == ALT) s += (rel & maskOf({1, 3, 8})) ? 2.0 : -5.0;
			if ((t.q == HDIM || t.q == DIM) && !(rel & (1 << 6))) s -= 4.0;
			if (r == low) s += 1.5;
			s -= 0.01 * i;
			if (s > bests) { bests = s; best = Chord(r, t.q); found = true; }
		}
	}
	out = best;
	return found;
}

} // namespace comper
