// Walker line engine: a line-by-line port of tools/proto/walker_ref.py (the definition).
// Same order of operations and random draws as the Python, doubles everywhere, so the same seed,
// chart and settings give the same bass line (checked by make test against test/walker_golden.json).
// Each Python feel loop is split into a start-up and a planning step so the host can plan just
// ahead of time; calling the steps in order gives exactly what the offline loop gives.
// No Rack includes.
#pragma once
#include "Engine.hpp"
#include <functional>

namespace walker {
using comper::Rng;
using comper::Chord;
using comper::SCALES;
using comper::SCALE_LEN;
using comper::pmod;

static const int CHORD_T[comper::NUM_Q][4] = {
	{0, 4, 7, 11}, {0, 4, 7, 9}, {0, 3, 7, 10}, {0, 3, 7, 9}, {0, 3, 7, 11}, {0, 4, 7, 10},
	{0, 4, 8, 10}, {0, 3, 6, 10}, {0, 3, 6, 9}, {0, 5, 7, 10}};
static const int LO = 28, HI = 55;
enum Feel { WALK, TWO, BALLAD, BOSSA, SOUL, HOUSE, NUM_FEELS };
static const char* const FEEL_NAMES[NUM_FEELS] = {"WALK", "TWO", "BALLAD", "BOSSA", "SOUL", "HOUSE"};
static const int OPEN[4] = {28, 33, 38, 43};
static const double TOUCH_S = 0.055;

static inline double stepCost(int d) {
	static const double c[13] = {6.0, 0.6, 0.0, 0.45, 0.55, 1.2, 2.4, 1.5, 2.6, 2.6, 2.8, 3.5, 2.0};
	return d >= 0 && d <= 12 ? c[d] : 4.5;
}

struct CellHit { int slot; char role; double len; double w; };
static const std::vector<std::vector<CellHit>>& soulCells() {
	static const std::vector<std::vector<CellHit>> c = {
		{{0, 'R', 2.5, 1.0}, {3, 'R', 1, 0.6}, {6, '5', 1.5, 0.75}, {8, 'R', 2, 0.9}, {11, 'G', 1, 0.4}, {12, '7', 1.5, 0.7}, {14, 'A', 1.5, 0.8}},
		{{0, 'R', 3, 1.0}, {4, 'G', 1, 0.4}, {6, 'O', 1, 0.8}, {7, 'R', 2, 0.7}, {10, 'R', 1.5, 0.85}, {13, '5', 1, 0.6}, {15, 'A', 1, 0.75}},
		{{0, 'R', 1.5, 1.0}, {2, 'O', 1, 0.7}, {4, 'G', 1, 0.4}, {6, 'R', 2, 0.85}, {10, '7', 1.5, 0.7}, {12, '5', 2, 0.75}, {15, 'A', 1, 0.7}}};
	return c;
}
static const std::vector<std::vector<CellHit>>& houseCells() {
	static const std::vector<std::vector<CellHit>> c = {
		{{2, 'R', 1.6, 0.9}, {6, 'R', 1.6, 0.85}, {10, 'R', 1.6, 0.9}, {14, 'O', 1.6, 0.8}},
		{{0, 'R', 1.5, 1.0}, {3, 'R', 1, 0.7}, {6, 'O', 1.5, 0.8}, {10, 'R', 1.5, 0.9}, {14, '7', 1.5, 0.75}},
		{{0, 'R', 1, 1.0}, {2, 'R', 1, 0.7}, {4, 'R', 1, 0.8}, {6, 'O', 1, 0.75}, {8, 'R', 1, 0.95}, {10, 'R', 1, 0.7}, {12, '5', 1, 0.8}, {14, 'A', 1, 0.8}}};
	return c;
}

struct BP {
	int feel = WALK;
	double activity = 0.5, reg = 40.0, roam = 0.5, out = 0.2, repeat = 0.7;
	double loose = 1.0, timing = 1.0, weight = 1.0, slop = 1.0, pitch = 1.0, ahead = 0.5, swing = 0.0;
	// 2.4.0: note lengths. length scales every note (1 = as the reference); vary mixes in short and long notes.
	// At length 1, vary 0 nothing changes and no random numbers are drawn (the golden test still holds).
	double length = 1.0, vary = 0.0;
};

enum Kind { K_WALK, K_SKIP, K_RUN, K_TWO, K_PICKUP, K_BOSSA, K_CELL };
struct Event { double beat; double len; int pitch; double w; int kind; bool dead; bool vib; };   // aggregate (C++11): missing fields are false
struct Note {
	double t = 0, vel = 0, dur = 0, scoop = 0, cents = 0, vib = 0, beat = 0;
	int pitch = 0, string = 0, fret = 0, kind = 0;
	bool dead = false, stopped = false;
	bool held = false;     // lengthened by LENGTH/VARY: the next note ends it, whatever the string
};

static inline int nearN(int pc, double ref, int lo = LO, int hi = HI) {
	int best = -1; double bd = 0;
	for (int n = lo; n <= hi; n++) {
		if (pmod(n, 12) != pmod(pc, 12)) continue;
		double d = std::fabs(n - ref);
		if (best < 0 || d < bd) { best = n; bd = d; }
	}
	return best;
}

static inline void noteString(int pitch, int hand, int& string, int& fret) {
	bool have = false; double bc = 0; int bs = 0, bf = 0;
	for (int s = 0; s < 4; s++) {
		int f = pitch - OPEN[s];
		if (f < 0 || f > 15) continue;
		double cost = f > 0 ? (double)std::abs(f - hand) : 1.5;
		cost += 0.4 * f / 3.0 + (s == 3 ? 0.3 : 0.0);
		if (!have || cost < bc) { have = true; bc = cost; bs = s; bf = f; }
	}
	if (!have) { string = 0; fret = std::max(0, pitch - OPEN[0]); return; }
	string = bs; fret = bf;
}

static inline bool inScale(int pc, int root, int q) {
	for (int i = 0; i < SCALE_LEN[q]; i++) if (pmod(root + SCALES[q][i], 12) == pc) return true;
	return false;
}

struct Player {
	// chart: chord at a beat (any real number, may be negative); the host supplies it
	std::function<Chord(double)> chordAt;
	double bpm = 120.0;
	BP p;
	Rng r, h, lr;          // lr: note lengths only (2.4.0), so the line and the hands keep their random streams
	double cperiod = 5, cphase = 0;
	// hands state
	bool haveLast = false; Note* last = nullptr;
	double drift = 0, phrase = 0; int hand = 2;
	// feel state
	int feelStarted = -1;
	int t1 = 0;            // WALK: the note chosen for the next segment start
	int walkB0 = 0;        // WALK: start of the next segment
	int lastP = 0;         // TWO / BALLAD / BOSSA
	int nextBar = 0;       // bar-based feels: next bar to plan
	const std::vector<CellHit>* cell = nullptr; std::vector<CellHit> kept; bool haveKept = false;

	void reset(int seed) {
		r = Rng(seed); h = Rng((int64_t)seed * 7919 + 17); lr = Rng((int64_t)seed * 104729 + 3);
		haveLast = false; last = nullptr; drift = 0; phrase = 0; hand = 2;
		feelStarted = -1; t1 = 0; walkB0 = 0; lastP = 0; nextBar = 0; cell = nullptr; kept.clear(); haveKept = false;
		cperiod = 5 + r.n(5); cphase = r.u();
	}
	Chord ch(double b) const { return chordAt(b); }
	double centre(double beat) const {
		return p.reg + 6.5 * p.roam * std::sin(2 * M_PI * (beat / 4.0 / cperiod + cphase));
	}
	int target(double beat, double ref, bool same) {
		Chord c = ch(beat); int pc = c.root;
		if (same && r.u() < 0.25 + 0.45 * p.roam) {
			if (r.u() < 0.6) {
				int pcs[3] = {c.root + CHORD_T[c.q][1], c.root + CHORD_T[c.q][2], c.root};
				pc = pcs[r.n(3)];
			} else { r.n(1); pc = c.root; }
		}
		double cen = centre(beat); bool have = false; double bc = 0; int bn = 0;
		for (int n = LO; n <= HI; n++) {
			if (pmod(n, 12) != pmod(pc, 12)) continue;
			double d = std::fabs(n - ref);
			double cost = 0.5 * std::fabs(n - cen) + (d == 0 ? 2.5 : 0.0) + (d > 7 ? 0.25 * (d - 7) : 0.0) + r.u() * (0.6 + 2.0 * p.roam);
			if (!have || cost < bc) { have = true; bc = cost; bn = n; }
		}
		return bn;
	}
	int approach(int t2, int prev, double beat) {
		Chord c = ch(beat), nc = ch(beat + 1);
		bool sc[12] = {};
		for (int i = 0; i < SCALE_LEN[c.q]; i++) sc[pmod(c.root + SCALES[c.q][i], 12)] = true;
		for (int i = 0; i < SCALE_LEN[nc.q]; i++) sc[pmod(nc.root + SCALES[nc.q][i], 12)] = true;
		double o = p.out;
		const int D[6] = {-1, 1, -2, 2, 7, -5};
		const double W[6] = {0.9 + o, 0.75 + o, 1.0, 0.9, 0.45, 0.6};
		double ow[6]; int on[6]; int k = 0;
		for (int i = 0; i < 6; i++) {
			int n = t2 + D[i];
			if (n < LO || n > HI || n == prev) continue;
			if (std::abs(D[i]) == 2 && !sc[pmod(n, 12)]) continue;
			double w = W[i];
			w *= (n - prev) * (t2 - n) > 0 ? 1.35 : 1.0;
			w *= std::abs(n - prev) > 7 ? 0.5 : 1.0;
			ow[k] = w; on[k] = n; k++;
		}
		if (k == 0) return t2 + (t2 < HI ? 1 : -1);
		double sum = 0; for (int i = 0; i < k; i++) sum += ow[i];
		double x = r.u() * sum;
		for (int i = 0; i < k; i++) { x -= ow[i]; if (x <= 0) return on[i]; }
		return on[k - 1];
	}
	std::vector<int> passing(int a, int b, int beat0, int count) {
		std::vector<int> res;
		if (count <= 0) return res;
		double cen = centre(beat0), o = p.out;
		std::vector<std::vector<std::pair<int, double>>> cands;
		for (int i = 0; i < count; i++) {
			Chord c = ch(beat0 + i); bool chs[12] = {}, scs[12] = {};
			for (int j = 0; j < 4; j++) chs[pmod(c.root + CHORD_T[c.q][j], 12)] = true;
			for (int j = 0; j < SCALE_LEN[c.q]; j++) scs[pmod(c.root + SCALES[c.q][j], 12)] = true;
			bool strong = pmod(beat0 + i, 2) == 0;
			std::vector<std::pair<int, double>> row;
			int lo = std::max(LO, std::min(a, b) - 9), hi = std::min(HI, std::max(a, b) + 9);
			for (int n = lo; n <= hi; n++) {
				int pc = pmod(n, 12);
				double cost = chs[pc] ? 0.0 : (scs[pc] ? (strong ? 2.2 : 0.9) : (strong ? 5.0 : 3.4 - 2.6 * o));
				cost += 0.035 * (n - cen) * (n - cen) / 4.0 + r.u() * (0.5 + 1.4 * p.roam);
				row.push_back({n, cost});
			}
			cands.push_back(row);
		}
		auto st = [](int x, int y) { return stepCost(std::abs(x - y)); };
		bool have = false; double bt = 0;
		if (count == 1) {
			int bn = 0;
			for (auto& nc : cands[0]) {
				int n = nc.first;
				double t = nc.second + st(a, n) + st(n, b) + ((n - a) * (b - n) < 0 ? 0.4 : 0.0);
				if (!have || t < bt) { have = true; bt = t; bn = n; }
			}
			res.push_back(bn);
		} else {
			int b1 = 0, b2 = 0;
			for (auto& c1 : cands[0]) {
				int n1 = c1.first; double s1 = c1.second + st(a, n1);
				for (auto& c2 : cands[1]) {
					int n2 = c2.first;
					double t = s1 + c2.second + st(n1, n2) + st(n2, b) + ((n1 - a) * (n2 - n1) < 0 ? 0.4 : 0.0) + ((n2 - n1) * (b - n2) < 0 ? 0.4 : 0.0);
					if (n2 == a) t += 1.5;
					if (!have || t < bt) { have = true; bt = t; b1 = n1; b2 = n2; }
				}
			}
			res.push_back(b1); res.push_back(b2);
		}
		return res;
	}
	double swingF() const {
		double sw = std::max(1.0, comper::autoSwing(bpm) + p.swing);
		return sw / (1 + sw);
	}
	// ---------------------------------------------------------------- start-up of a feel (draws as the Python does before its loop)
	void startFeel(int feel) { startFeelAt(feel, 0, false); }
	// start a feel at a bar line. fromLast: carry on from the last note played (a change of feel mid-tune)
	// instead of drawing a new first note; not part of the reference, so not covered by the golden test.
	void startFeelAt(int feel, int bar, bool fromLast) {
		feelStarted = feel; int B = bar * 4;
		fromLast = fromLast && haveLast;
		if (feel == WALK) { t1 = fromLast ? nearN(ch(B).root, last->pitch) : target(B, centre(B), false); walkB0 = B; }
		else if (feel == TWO || feel == BALLAD || feel == BOSSA) { lastP = fromLast ? last->pitch : target(B, centre(B), false); nextBar = bar; }
		else { const auto& bank = feel == SOUL ? soulCells() : houseCells(); cell = &bank[r.n((int)bank.size())]; haveKept = false; nextBar = bar; }
	}
	// the beat the next planning step starts at
	double nextStepBeat() const {
		if (feelStarted == WALK) return walkB0;
		return nextBar * 4.0;
	}
	// ---------------------------------------------------------------- one planning step; events sorted by beat (stable)
	std::vector<Event> step() {
		std::vector<Event> ev;
		int f = feelStarted;
		if (f == WALK) stepWalk(ev);
		else if (f == TWO) stepTwo(ev, false);
		else if (f == BALLAD) stepTwo(ev, true);
		else if (f == BOSSA) stepBossa(ev);
		else stepCells(ev, f == SOUL ? soulCells() : houseCells());
		std::stable_sort(ev.begin(), ev.end(), [](const Event& a, const Event& b) { return a.beat < b.beat; });
		return ev;
	}
	void stepWalk(std::vector<Event>& ev) {
		double fsw = swingF();
		int b0 = walkB0, b1 = b0 + 1;
		while (!(pmod(b1, 4) == 0 || ch(b1) != ch(b1 - 1))) b1++;
		int n = b1 - b0;
		bool same = ch(b1) == ch(b0);
		int t2 = target(b1, t1, same);
		std::vector<int> line = {t1};
		if (n >= 2) {
			int ap = approach(t2, t1, b1 - 1);
			std::vector<int> mid = passing(t1, ap, b0 + 1, n - 2);
			line.insert(line.end(), mid.begin(), mid.end());
			line.push_back(ap);
		}
		for (size_t i = 0; i < line.size(); i++) {
			int pitch = line[i]; int b = b0 + (int)i;
			Event e{(double)b, 0.94, pitch, 1.0 + (pmod(b, 2) ? 0.07 : 0.0) + (pmod(b, 4) == 0 ? 0.04 : 0.0), K_WALK};
			size_t ei = ev.size(); ev.push_back(e);
			int nxt = i + 1 < line.size() ? line[i + 1] : t2;
			double u = r.u(), act = p.activity;
			bool strongNext = pmod(b + 1, 2) == 0;
			if (u < (0.02 + 0.42 * std::max(0.0, act - 0.3)) * (strongNext ? 1.3 : 0.8)) {
				double v = r.u();
				if (v < 0.45) { Event s{b + fsw, (1 - fsw) * 0.8, pitch, 0.5, K_SKIP}; s.dead = true; ev.push_back(s); ev[ei].len = fsw * 0.95; }
				else if (v < 0.75) { ev.push_back(Event{b + fsw, (1 - fsw) * 0.9, pitch, 0.62, K_SKIP}); ev[ei].len = fsw * 0.9; }
				else {
					int lean = nxt > pitch ? nxt - 1 : nxt + 1;
					if (LO <= lean && lean <= HI && lean != pitch) { ev.push_back(Event{b + fsw, (1 - fsw) * 0.95, lean, 0.66, K_SKIP}); ev[ei].len = fsw * 0.95; }
				}
			} else if (act > 0.62 && u > 1 - 0.3 * (act - 0.62) && (std::abs(nxt - pitch) == 3 || std::abs(nxt - pitch) == 4)) {
				int d = nxt > pitch ? 1 : -1; ev[ei].len = 0.32;
				for (int j = 1; j <= 2; j++) ev.push_back(Event{b + j / 3.0, 0.32, pitch + d * j, 0.7, K_RUN});
			}
		}
		t1 = t2; walkB0 = b1;
	}
	void stepTwo(std::vector<Event>& ev, bool ballad) {
		double fsw = swingF();
		int bar = nextBar++; int b = bar * 4;
		Chord c1 = ch(b), c3 = ch(b + 2), cn = ch(b + 4);
		int n1 = nearN(c1.root, bar ? (double)lastP : centre(b), LO, 47);
		if (std::fabs(n1 - centre(b)) > 9) n1 = nearN(c1.root, centre(b), LO, 47);
		int n3;
		if (c3 != c1) n3 = nearN(c3.root, n1, LO, 47);
		else {
			int fifth = c1.root + CHORD_T[c1.q][2], third = c1.root + CHORD_T[c1.q][1];
			int pc = r.u() > 0.22 * (0.5 + p.roam) ? fifth : third;
			n3 = nearN(pc, r.u() < 0.6 ? n1 - 3 : n1 + 4, LO, 50);
			if (n3 == n1) n3 = nearN(pc, n1 + 6, LO, 50);
		}
		bool whole = ballad && c3 == c1 && r.u() < 0.55 - 0.6 * p.activity;
		{ Event e{(double)b, whole ? 3.8 : 1.85, n1, 1.0, K_TWO}; e.vib = ballad; ev.push_back(e); }
		if (!whole) { Event e{b + 2.0, 1.85, n3, 0.9, K_TWO}; e.vib = ballad; ev.push_back(e); }
		int last = whole ? n1 : n3;
		int nn = nearN(cn.root, last, LO, 47);
		if (r.u() < (ballad ? 0.12 : 0.22) + 0.6 * p.activity) {
			int ap = approach(nn, last, b + 3);
			bool late = r.u() < (ballad ? 0.3 : 0.5);
			if (!whole) ev.back().len = late ? 1.45 : 0.95;
			else ev.back().len = late ? 3.4 : 2.9;
			ev.push_back(Event{b + (late ? 3 + fsw : 3.0), late ? (1 - fsw) : 0.95, ap, 0.7, K_PICKUP}); last = ap;
		}
		if (!ballad && p.activity > 0.5 && r.u() < 0.5 * (p.activity - 0.5)) {
			Event e{b + 1 + fsw, (1 - fsw) * 0.8, n1, 0.45, K_SKIP}; e.dead = true; ev.push_back(e);
		}
		lastP = last;
	}
	void stepBossa(std::vector<Event>& ev) {
		int bar = nextBar++; int b = bar * 4;
		Chord c1 = ch(b), c3 = ch(b + 2), cn = ch(b + 4);
		int n1 = nearN(c1.root, centre(b) - 2);
		int fpc = c3 != c1 ? c3.root : c1.root + CHORD_T[c1.q][2];
		int n3 = nearN(fpc, (c3 == c1 && r.u() < 0.65) ? n1 - 4 : n1 + 3);
		int nn = nearN(cn.root, centre(b + 4) - 2);
		ev.push_back(Event{b + 0.0, 1.4, n1, 1.0, K_BOSSA});
		if (r.u() < 0.35 + 0.6 * p.activity) ev.push_back(Event{b + 1.5, 0.45, r.u() < 0.6 ? n3 : n1, 0.62, K_PICKUP});
		ev.push_back(Event{b + 2.0, 1.4, n3, 0.88, K_BOSSA});
		if (r.u() < 0.5 + 0.5 * p.activity) { ev.push_back(Event{b + 3.5, 0.5, nn, 0.72, K_PICKUP}); ev[ev.size() - 2].len = 1.4; }
		lastP = nn;
	}
	void stepCells(std::vector<Event>& ev, const std::vector<std::vector<CellHit>>& bank) {
		int bar = nextBar++;
		if (bar && bar % 2 == 0 && r.u() > p.repeat) { cell = &bank[r.n((int)bank.size())]; haveKept = false; }
		if (!haveKept || r.u() > 0.5 + 0.5 * p.repeat) {
			kept.clear();
			for (const auto& hh : *cell) if (hh.slot == (*cell)[0].slot || r.u() < std::min(1.0, 0.45 + p.activity)) kept.push_back(hh);
			haveKept = true;
		}
		for (const auto& hh : kept) {
			double beat = bar * 4 + hh.slot / 4.0; Chord c = ch(beat); int base = nearN(c.root, centre(bar * 4) - 3, LO, 40);
			int n;
			if (hh.role == 'R' || hh.role == 'G') n = base;
			else if (hh.role == 'O') n = base + 12;
			else if (hh.role == '5') n = base + CHORD_T[c.q][2] - ((base + CHORD_T[c.q][2] > base + 6 && r.u() < 0.5 && base + CHORD_T[c.q][2] - 12 >= LO) ? 12 : 0);
			else if (hh.role == '7') n = base + CHORD_T[c.q][3] - ((base + CHORD_T[c.q][3] - 12 >= LO && r.u() < 0.4) ? 12 : 0);
			else {
				Chord nc = ch(bar * 4 + 4); int tgt = nearN(nc.root, base, LO, 40); n = tgt + (r.u() < 0.6 ? -1 : 2);
				if (n == tgt + 2 && !inScale(pmod(n, 12), nc.root, nc.q)) n = tgt - 2;
				n = std::max(LO, n);
			}
			Event e{beat, hh.len / 4.0 * 0.9, n, hh.w, K_CELL}; e.dead = hh.role == 'G'; ev.push_back(e);
		}
	}
	// ---------------------------------------------------------------- the hands
	// 2.4.0 LENGTH / VARY: the length multiplier for one event. Long notes favour the beat, short ones the offbeats.
	double lengthMul(const Event& e) {
		double m = p.length;
		if (p.vary <= 0.0) return m;
		double v = std::min(1.0, p.vary), u = lr.u(), x = lr.u();
		double frac = e.beat - std::floor(e.beat);
		bool onBeat = frac < 0.02 || frac > 0.98;
		double pLong = v * (onBeat ? 0.42 : 0.16), pShort = v * (onBeat ? 0.14 : 0.40);
		if (u < pLong) m *= 1.8 + 1.2 * x;
		else if (u < pLong + pShort) m *= 0.35 + 0.25 * x;
		else m *= 1.0 + 0.3 * v * (2.0 * x - 1.0);
		return m;
	}
	// Appends to `out` (a std::deque, or a std::vector that is only ever appended to by this function).
	// May change dur/stopped of the note it returned last, through `last`; onPrevChanged is told when it does.
	template <class C>
	void perform(const std::vector<Event>& ev, C& out, const std::function<void(Note*)>& onPrevChanged = nullptr) {
		double spb = 60.0 / bpm, L = p.loose;
		for (const Event& e : ev) {
			drift = 0.9 * drift + 0.0016 * h.g() * L * p.timing;
			if ((int)e.beat % 8 == 0 && e.beat == (double)(int)e.beat) phrase = 0.05 * h.g() * L * p.weight;
			double t = e.beat * spb - 0.016 * p.ahead * std::min(1.5, L + 0.25) + drift + 0.0045 * h.g() * L * p.timing;
			double vel = 0.74 * e.w + phrase + 0.045 * h.g() * L * p.weight;
			double dur = e.len * spb, dur0 = dur;   // dur0: vibrato is decided on the written length, so LENGTH never moves the hands' random draws
			bool dead = e.dead; Note* a = haveLast ? last : nullptr;
			int leap = a ? std::abs(e.pitch - a->pitch) : 0;
			bool changed = false;
			if (leap >= 5 && !dead && a) { a->dur = std::max(0.06, a->dur - std::min(0.05, 0.12 * spb)); changed = true; }
			if (!dead && e.kind == K_WALK && h.u() < 0.018 * L * p.slop) dead = true;
			double lm = dead ? 1.0 : lengthMul(e);
			bool held = lm > 1.0;
			if (lm != 1.0) dur = std::min(4.0 * spb, std::max(0.04, dur * lm));
			double scoop = 0;
			if (!dead && (leap >= 5 || h.u() < 0.1 * L)) scoop = (18 + 40 * h.u()) * std::min(1.5, L) * p.pitch;
			double cents = 4.5 * h.g() * L * p.pitch + (e.pitch > 47 ? 3.0 * L * p.pitch : 0.0);
			double vib = ((e.vib && dur0 > 0.8) || dur0 > 1.3) ? (5 + 5 * h.u()) * std::min(1.5, L) : 0.0;
			Note n; n.t = t; n.pitch = e.pitch; n.vel = std::max(0.12, std::min(1.0, vel)); n.dur = dur; n.dead = dead; n.scoop = scoop;
			n.cents = cents; n.vib = vib; n.kind = e.kind; n.beat = e.beat; n.stopped = false; n.held = held;
			noteString(n.pitch, hand, n.string, n.fret);
			if (n.fret > 0 && std::abs(n.fret - hand) > 3) hand = n.fret;
			if (a) {
				double gap = n.t - a->t, touch = std::min(TOUCH_S, 0.22 * gap);
				if (a->string == n.string && a->t + a->dur > n.t - touch - 0.02) { a->dur = std::max(0.04, gap - touch); a->stopped = true; changed = true; }
				// a held (lengthened) note on another string: the player lets go as the next note speaks, so the line stays one voice
				else if (a->held && a->t + a->dur > n.t) { a->dur = std::max(0.04, gap - 0.3 * touch); changed = true; }
			}
			if (changed && onPrevChanged) onPrevChanged(a);
			out.push_back(n);
			last = &out.back(); haveLast = true;
		}
	}
	// ---------------------------------------------------------------- cable mode before the tune is known
	Event blindBeat(int b, int root, int q, bool changed) {
		int lastPitch = haveLast ? last->pitch : (int)std::nearbyint(centre(b));
		int k = pmod(b, 4); int n;
		if (changed || k == 0) n = nearN(root, std::fabs(lastPitch - centre(b)) <= 9 ? (double)lastPitch : centre(b));
		else if (k == 2) n = nearN(root + CHORD_T[q][2], r.u() < 0.5 ? lastPitch - 2 : lastPitch + 2);
		else if (k == 3) {
			int tgt = nearN(root, lastPitch); n = r.u() < 0.6 ? tgt - 1 : tgt + 1;
			if (n == lastPitch) n = tgt + (n < tgt ? 2 : -2);
		} else {
			std::vector<int> c;
			for (int m = std::max(LO, lastPitch - 4); m <= std::min(HI, lastPitch + 4); m++)
				if (m != lastPitch && inScale(pmod(m, 12), root, q)) c.push_back(m);
			n = c.empty() ? nearN(root + CHORD_T[q][1], lastPitch) : c[r.n((int)c.size())];
		}
		n = std::max(LO, std::min(HI, n));
		return Event{(double)b, 0.94, n, 1.0 + (pmod(b, 2) ? 0.07 : 0.0) + (k == 0 ? 0.04 : 0.0), K_WALK};
	}
};

} // namespace walker
