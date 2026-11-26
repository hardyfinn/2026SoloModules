// Rider engine: a line-by-line port of tools/proto/rider_ref.py (the definition).
// Plans one bar of drumming at a time from what it is told at the bar line. Same order of operations and
// random draws as the Python, doubles everywhere, so the same seed and inputs give the same strokes
// (checked by make test against test/rider_golden.json). No Rack includes.
#pragma once
#include "Engine.hpp"
#include <vector>
#include <algorithm>
#include <cmath>

namespace rider {

using comper::Rng;

enum Voice { RIDE, BELL, CRASH, HATF, HATC, HATO, SNARE, STICK, KICK, TOM_HI, TOM_LO, BR_TAP, BR_SWEEP, BR_HATF, NUM_PLAYER_VOICES };
enum Feel { SWING, BOSSA, BALLAD, SOUL, HOUSE, NUM_FEELS };
static const char* const FEEL_NAMES[NUM_FEELS] = {"SWING", "BOSSA", "BALLAD", "SOUL", "HOUSE"};
enum { S_ = 0, L_ = 1 };
enum { SUB0 = 0, SUBM = 1, SUBA = 2 };

struct RP {
	int feel;
	double energy, comp, listen, fills, feather, repeat, loose, ahead, swing;
	RP() : feel(SWING), energy(0.5), comp(0.5), listen(1.0), fills(0.5), feather(0.5), repeat(0.6), loose(1.0), ahead(0.5), swing(0.0) {}
};

// what the drummer is told at the bar line
struct BarIn {
	bool hasPiano;
	int slots;
	int kind[16];          // -1 no hit, S_, L_
	double slotBeat[16];   // beats from the bar line
	bool push[16];
	int nbass;
	double bassOff[16];    // beats from the bar line of bass notes off the beat
	int forceFill;         // 0 no, 1 small, 2 big
	BarIn() : hasPiano(false), slots(8), nbass(0), forceFill(0) {
		for (int i = 0; i < 16; i++) { kind[i] = -1; slotBeat[i] = 0.0; push[i] = false; bassOff[i] = 0.0; }
	}
};

struct Stroke {
	double beat;   // beats from the bar line
	double offs;   // seconds
	int voice, vel;
	double dur;    // beats (sweeps only)
};

struct Stats {
	int pushes, caught, comps, fills, bombs;   // bombs: for the display only
	Stats() : pushes(0), caught(0), comps(0), fills(0), bombs(0) {}
};

struct Pos2 { int b, sub; };
struct Fig3 { int par, b, sub; };
struct KFig { int par, s; };

struct Rider {
	Rng r, h;
	bool hasFigure; std::vector<Fig3> figure;
	bool hasKfig; std::vector<KFig> kfig;
	int landing;
	double drift;
	std::vector<double> prevHits;
	std::vector<Stroke> ev;
	Stats stats;
	// per-call
	RP p; BarIn bi; double bpm;

	Rider(int64_t seed = 1) { reset(seed); }

	void reset(int64_t seed) {
		r = Rng(seed * 5 + 1); h = Rng(seed * 5 + 2);
		hasFigure = false; figure.clear(); hasKfig = false; kfig.clear();
		landing = 0; drift = 0.0; prevHits.clear(); ev.clear(); stats = Stats();
	}

	// ---- helpers
	double swingF(double bpm_, const RP& pp) const {
		double ratio = std::max(1.0, std::min(3.0, comper::autoSwing(bpm_) * std::pow(2.0, pp.swing * 0.6)));
		return ratio / (1.0 + ratio);
	}

	void hit(double beat, int voice, double vel, double late = 0.0, double jit = 1.0, double dur = 0.0) {
		double L = p.loose;
		double offs = (late + drift) * L + h.g() * 0.004 * jit * L - (p.ahead - 0.5) * 0.024;
		int v = (int)std::max(1.0, std::min(127.0, vel + h.g() * 4.0 * L));
		Stroke s; s.beat = beat; s.offs = offs; s.voice = voice; s.vel = v; s.dur = dur;
		ev.push_back(s);
	}

	int pianoAt(double beat, double win) const {
		int best = -1; double bd = 1e9;
		if (!bi.hasPiano) return -1;
		for (int s = 0; s < bi.slots; s++) {
			if (bi.kind[s] < 0) continue;
			double d = std::fabs(bi.slotBeat[s] - beat);
			if (d <= win && d < bd - 1e-12) { best = s; bd = d; }
		}
		return best;
	}

	bool strong(int s) const { return bi.kind[s] == L_ || bi.push[s]; }

	bool quiet(double beat) const {
		if (!bi.hasPiano) return true;
		for (int s = 0; s < bi.slots; s++)
			if (bi.kind[s] >= 0 && beat - 1.6 <= bi.slotBeat[s] && bi.slotBeat[s] <= beat + 0.4) return false;
		for (double b : prevHits)
			if (beat - 1.6 <= b && b <= beat + 0.4) return false;
		return true;
	}

	int lastFill = 0;   // the size of the last set-up picked (for the display; no draws)
	int pickFill(int bar, int chorusBars, double e) { lastFill = pickFill_(bar, chorusBars, e); return lastFill; }
	int pickFill_(int bar, int chorusBars, double e) {
		if (bi.forceFill) return bi.forceFill;
		if ((bar + 1) % chorusBars == 0)
			return r.u() < std::min(1.0, p.fills * 1.5) ? 2 : 0;
		if ((bar + 1) % 8 == 0) {
			if (r.u() < p.fills * 0.9) return r.u() < 0.3 + 0.5 * e ? 2 : 1;
			return 0;
		}
		if ((bar + 1) % 4 == 0)
			return r.u() < p.fills * 0.45 ? 1 : 0;
		return 0;
	}

	void land(double e) {
		if (landing) {
			hit(0.0, KICK, 76 + 34 * e);
			if (landing == 2 && e > 0.4) hit(0.0, CRASH, 76 + 34 * e, 0.002);
			landing = 0;
		}
	}

	// ---- one bar
	const std::vector<Stroke>& planBar(int bar, double bpm_, int chorusBars, const RP& pp, const BarIn& b_) {
		p = pp; bi = b_; ev.clear(); bpm = bpm_; lastFill = 0;
		if (chorusBars < 1) chorusBars = 1;
		double e = std::max(0.0, std::min(1.0, p.energy));
		if (p.feel == SWING) swingBar(bar, chorusBars, e);
		else if (p.feel == BALLAD) balladBar(bar, chorusBars, e);
		else if (p.feel == BOSSA) bossaBar(bar, chorusBars, e);
		else if (p.feel == SOUL) soulBar(bar, chorusBars, e);
		else houseBar(bar, chorusBars, e);
		prevHits.clear();
		for (int s = 0; s < bi.slots; s++) if (bi.hasPiano && bi.kind[s] >= 0) prevHits.push_back(bi.slotBeat[s] - 4.0);
		return ev;
	}

	// ------------------------------------------------------------ swing, sticks
	static bool inPos(const std::vector<Pos2>& v, int b, int sub) {
		for (auto& q : v) if (q.b == b && q.sub == sub) return true;
		return false;
	}
	static bool inInts(const std::vector<int>& v, int b) {
		for (int x : v) if (x == b) return true;
		return false;
	}

	void swingBar(int bar, int chorusBars, double e) {
		static const Pos2 FS[4][3] = {{{3, SUB0}, {3, SUBM}, {3, SUBA}}, {{2, SUBA}, {3, SUB0}, {3, SUBA}}, {{3, SUBA}, {0, 0}, {0, 0}}, {{2, SUBA}, {3, SUBM}, {3, SUBA}}};
		static const int FSN[4] = {3, 3, 1, 3};
		static const Pos2 FB[3][6] = {{{2, SUB0}, {2, SUBM}, {2, SUBA}, {3, SUB0}, {3, SUBM}, {3, SUBA}},
		                              {{2, SUB0}, {2, SUBA}, {3, SUB0}, {3, SUBM}, {3, SUBA}, {0, 0}},
		                              {{1, SUBA}, {2, SUB0}, {2, SUBA}, {3, SUB0}, {3, SUBM}, {3, SUBA}}};
		static const int FBN[3] = {6, 5, 6};
		static const Pos2 RP_[5][7] = {{{0, SUB0}, {1, SUB0}, {1, SUBA}, {2, SUB0}, {3, SUB0}, {3, SUBA}, {0, 0}},
		                               {{0, SUB0}, {1, SUB0}, {1, SUBA}, {2, SUB0}, {3, SUB0}, {0, 0}, {0, 0}},
		                               {{0, SUB0}, {1, SUB0}, {2, SUB0}, {2, SUBA}, {3, SUB0}, {3, SUBA}, {0, 0}},
		                               {{0, SUB0}, {1, SUB0}, {2, SUB0}, {3, SUB0}, {0, 0}, {0, 0}, {0, 0}},
		                               {{0, SUB0}, {0, SUBA}, {1, SUB0}, {1, SUBA}, {2, SUB0}, {3, SUB0}, {3, SUBA}}};
		static const int RPN[5] = {6, 5, 6, 4, 7};
		double f = swingF(bpm, p);
		auto pos = [f](int b, int sub) { return b + (sub == SUB0 ? 0.0 : sub == SUBM ? f / 2.0 : f); };
		drift = 0.6 * drift + h.g() * 0.0015;
		// --- where is the phrase? fills and landings
		int fill = pickFill(bar, chorusBars, e);
		std::vector<Pos2> fillPos; bool landOnPush = false;
		if (fill) {
			stats.fills++;
			if (fill == 1) { int k = r.n(4); for (int i = 0; i < FSN[k]; i++) fillPos.push_back(FS[k][i]); }
			else { int k = r.n(3); for (int i = 0; i < FBN[k]; i++) fillPos.push_back(FB[k][i]); }
			int s = pianoAt(pos(3, SUBA), 0.18);
			if (s >= 0 && strong(s)) {
				if (r.u() < p.listen) {
					landOnPush = true;
					std::vector<Pos2> keep;
					for (auto& q : fillPos) if (!(q.b == 3 && q.sub == SUBA)) keep.push_back(q);
					fillPos = keep;
				}
			}
		}
		std::vector<int> fillBeats;
		if (fill == 2 || fillPos.size() > 1) {
			for (auto& q : fillPos) if (!inInts(fillBeats, q.b)) fillBeats.push_back(q.b);
			std::sort(fillBeats.begin(), fillBeats.end());
		}
		// --- ride cymbal: the time
		bool fast = bpm > 190;
		double w[5] = {5.0, 1.2, 0.6 + e, 0.5 + (fast ? 2.5 : 0.0), 0.3 + 1.2 * e};
		double x = r.u() * (w[0] + w[1] + w[2] + w[3] + w[4]); int k = 0;
		while (k < 4 && x >= w[k]) { x -= w[k]; k++; }
		double base = 50 + 34 * e;
		for (int i = 0; i < RPN[k]; i++) {
			int b = RP_[k][i].b, sub = RP_[k][i].sub;
			if (inInts(fillBeats, b)) continue;
			double v = base + ((b == 1 || b == 3) && sub == SUB0 ? 7 : 0) - (sub == SUBA ? 13 : 0);
			hit(pos(b, sub), RIDE, v, 0.0, 0.8);
		}
		for (int b : {1, 3}) hit((double)b, HATF, 58 + 16 * e, -0.003, 0.7);
		if (p.feather > 0.05 && e < 0.85)
			for (int b = 0; b < 4; b++) hit((double)b, KICK, 18 + 14 * p.feather, -0.002, 0.8);
		// --- comping: snare and bass drum, a kept two-bar figure
		int par = bar % 2;
		if (par == 0 || !hasFigure) {
			if (!hasFigure || r.u() > p.repeat) {
				std::vector<Fig3> fig;
				for (int bb = 0; bb < 2; bb++)
					for (int b = 0; b < 4; b++) {
						const int subs[3] = {SUBA, SUB0, SUBM};
						const double wts[3] = {(b == 1 || b == 3) ? 1.0 : 0.7, b == 3 ? 0.4 : 0.22, 0.18 * e};
						for (int q = 0; q < 3; q++)
							if (r.u() < p.comp * (0.22 + 0.5 * e) * wts[q]) fig.push_back(Fig3{bb, b, subs[q]});
					}
				figure = fig; hasFigure = true;
			}
		}
		int run = 0;
		for (int b = 0; b < 4; b++) {
			for (int sub : {SUB0, SUBM, SUBA}) {
				if (inPos(fillPos, b, sub) || inInts(fillBeats, b)) continue;
				double beat = pos(b, sub);
				bool want = false;
				for (auto& q : figure) if (q.par == par && q.b == b && q.sub == sub) { want = true; break; }
				int s = sub != SUBM ? pianoAt(beat, 0.17) : -1;
				bool caught = false;
				if (s >= 0) {
					if (sub == SUBA && strong(s)) {
						stats.pushes++;
						if (r.u() < p.listen * (0.55 + 0.4 * e)) { want = true; caught = true; }
					} else if (want) {
						if (r.u() < 0.65 * p.listen) want = false;
					}
				} else if (sub == SUBA && !want && bi.hasPiano) {
					if (quiet(beat)) {
						if (r.u() < 0.28 * p.listen * p.comp * (0.5 + e)) want = true;
					}
				}
				if (want && !caught) {
					bool near = false;
					for (int i = 0; i < bi.nbass; i++) if (std::fabs(bi.bassOff[i] - beat) < 0.12) near = true;
					if (near) {
						if (r.u() < 0.5 * p.listen) want = false;
					}
				}
				if (want && run >= (e < 0.7 ? 2 : 3)) want = false;
				run = want ? run + 1 : 0;
				if (!want) continue;
				stats.comps++;
				if (caught) stats.caught++;
				bool bomb = r.u() < (0.16 + 0.26 * e) * ((sub == SUBA && (b == 1 || b == 3)) ? 1.5 : 1.0);
				if (caught && e > 0.55 && bi.kind[s] == L_) {
					bool withSnare = r.u() < 0.5;
					hit(beat, KICK, 78 + 30 * e, 0.0);
					hit(beat, withSnare ? SNARE : RIDE, 80 + 25 * e, 0.003);
				} else if (bomb) {
					hit(beat, KICK, 62 + 40 * e, 0.001); stats.bombs++;
				} else {
					bool ghost = (r.u() < 0.5 - 0.3 * e) && !caught;
					hit(beat, SNARE, ghost ? (36 + 14 * e) : (62 + 34 * e), 0.005);
				}
			}
		}
		// --- the fill, and where it lands
		int n = (int)fillPos.size();
		bool toms = e > 0.45 && fill == 2;
		for (int i = 0; i < n; i++) {
			double xx = (i + 1) / (double)n;
			int voice = SNARE;
			if (toms && i >= n - 3) {
				static const int TV[3] = {TOM_HI, TOM_HI, TOM_LO};
				if (r.u() < 0.75) voice = TV[i - (n - 3)];
			}
			hit(pos(fillPos[i].b, fillPos[i].sub), voice, 48 + 30 * e + 26 * xx, -0.004 * xx, 0.9);
		}
		land(e);
		if (fill) {
			if (landOnPush) {
				hit(pos(3, SUBA), KICK, 84 + 30 * e);
				hit(pos(3, SUBA), (fill == 2 && e > 0.4) ? CRASH : RIDE, 82 + 30 * e, 0.002);
				stats.caught++;
			} else landing = fill == 2 ? 2 : 1;
		}
	}

	// ------------------------------------------------------------ ballad, brushes
	void balladBar(int bar, int chorusBars, double e) {
		double f = swingF(bpm, p);
		drift = 0.6 * drift + h.g() * 0.002;
		landing = 0;
		for (int b = 0; b < 4; b++) {
			hit((double)b, BR_SWEEP, 40 + 22 * e + ((b == 1 || b == 3) ? 5 : 0), 0.0, 1.2, 0.95);
			if (b == 1 || b == 3) hit((double)b, BR_TAP, 44 + 24 * e, 0.006);
			if (r.u() < 0.35 + 0.4 * p.comp) hit(b + f, BR_TAP, 30 + 18 * e, 0.008);
			if (b == 1 || b == 3) hit((double)b, BR_HATF, 40 + 14 * e, -0.003);
		}
		if (p.feather > 0.05) hit(0.0, KICK, 26 + 12 * p.feather);
		if (bi.hasPiano) {
			for (int s = 0; s < bi.slots; s++) {
				if (bi.kind[s] < 0 || !strong(s)) continue;
				stats.pushes++;
				if (r.u() < 0.5 * p.listen) { hit(bi.slotBeat[s], BR_TAP, 68 + 26 * e, 0.004); stats.caught++; }
			}
		}
		int fill = pickFill(bar, chorusBars, e);
		if (fill) {
			const double subs[3] = {0.0, f / 2.0, f};
			for (int i = 0; i < 3; i++) hit(3 + subs[i], BR_TAP, 40 + 10 * i + 20 * e, 0.004);
			stats.fills++;
		}
	}

	// ------------------------------------------------------------ bossa
	void bossaBar(int bar, int chorusBars, double e) {
		static const int CL[2][3][2] = {{{0, 0}, {1, 1}, {3, 0}}, {{1, 0}, {2, 1}, {0, 0}}};
		static const int CLN[2] = {3, 2};
		drift = 0.6 * drift + h.g() * 0.0012;
		int top = e > 0.55 ? RIDE : HATC;
		for (int b = 0; b < 4; b++) {
			hit((double)b, top, 50 + 22 * e, 0.0, 0.7);
			hit(b + 0.5, top, 40 + 20 * e, 0.0, 0.7);
			if (b == 1 || b == 3) hit((double)b, HATF, 54 + 12 * e, -0.003);
		}
		const int KB[4][2] = {{0, 0}, {1, 1}, {2, 0}, {3, 1}};
		for (int i = 0; i < 4; i++) hit(KB[i][0] + 0.5 * KB[i][1], KICK, (KB[i][1] == 0 ? 70 : 54) + 16 * e, -0.001);
		bool extra = r.u() < 0.25 * (1 - p.repeat) + 0.1;
		int par = bar % 2;
		for (int i = 0; i < CLN[par]; i++) hit(CL[par][i][0] + 0.5 * CL[par][i][1], STICK, 60 + 18 * e, 0.004);
		if (extra) hit(3.5, STICK, 60 + 18 * e, 0.004);
		land(e);
		int fill = pickFill(bar, chorusBars, e);
		if (fill) {
			for (int half = 0; half < 2; half++) hit(3 + 0.5 * half, SNARE, 60 + 24 * e);
			stats.fills++;
			if (fill == 2) landing = 2;
		}
	}

	// ------------------------------------------------------------ soul: sixteenths with a backbeat
	void grid16(bool soul, double* g) const {
		double sw = std::max(0.5, std::min(0.67, (soul ? 0.54 : 0.5) + 0.12 * p.swing));
		const double sub[4] = {0.0, sw / 2, 0.5, 0.5 + sw / 2};
		for (int s = 0; s < 16; s++) g[s] = (double)(s / 4) + sub[s % 4];
	}

	void soulBar(int bar, int chorusBars, double e) {
		static const int KS[7] = {3, 6, 7, 8, 10, 11, 14};
		static const double KW[7] = {0.30, 0.45, 0.25, 0.35, 0.5, 0.3, 0.3};
		double g[16]; grid16(true, g);
		drift = 0.6 * drift + h.g() * 0.0012;
		int fill = pickFill(bar, chorusBars, e);
		int fillFrom = 16;
		if (fill) { fillFrom = fill == 1 ? 12 : 8; stats.fills++; }
		int par = bar % 2;
		if (par == 0 || !hasKfig) {
			if (!hasKfig || r.u() > p.repeat) {
				std::vector<KFig> kf;
				for (int bb = 0; bb < 2; bb++)
					for (int i = 0; i < 7; i++)
						if (r.u() < KW[i] * (0.35 + 0.9 * p.comp) * (0.6 + 0.6 * e)) kf.push_back(KFig{bb, KS[i]});
				kfig = kf; hasKfig = true;
			}
		}
		bool bell = e > 0.72;
		int openAt = -1;
		if (par == 1) { if (r.u() < 0.15 + 0.3 * e) openAt = 14; }
		for (int s = 0; s < 16; s++) {
			if (s >= fillFrom) break;
			if (s % 2 == 1) {
				if (r.u() < std::max(0.0, e - 0.45) * 1.4 && !bell) hit(g[s], HATC, 34 + 18 * e, 0.0, 0.6);
				continue;
			}
			if (bell) hit(g[s], s % 4 == 0 ? BELL : RIDE, (s % 4 == 0 ? 58 : 46) + 26 * e, 0.0, 0.6);
			else if (s == openAt) hit(g[s], HATO, 60 + 24 * e, 0.0, 0.6);
			else hit(g[s], HATC, (s % 4 == 0 ? 56 : 44) + 24 * e, 0.0, 0.6);
		}
		if (bell) for (int b : {1, 3}) hit((double)b, HATF, 58 + 14 * e, -0.003);
		for (int s : {4, 12}) {
			if (s >= fillFrom) continue;
			hit(g[s], e < 0.3 ? STICK : SNARE, (e < 0.3 ? 64 : 80) + 30 * e, 0.004);
		}
		bool landed = landing != 0;
		land(e);
		std::vector<int> kicks; kicks.push_back(0);
		for (auto& q : kfig) if (q.par == par) kicks.push_back(q.s);
		if (bi.hasPiano) {
			for (int s = 0; s < 16; s++) {
				double beat = g[s];
				if (s == 4 || s == 12 || inInts(kicks, s)) continue;
				int ps = pianoAt(beat, 0.07);
				if (ps >= 0 && strong(ps)) {
					stats.pushes++;
					if (r.u() < p.listen * (0.4 + 0.4 * e)) { kicks.push_back(s); stats.caught++; }
				}
			}
		}
		std::vector<int> ks = kicks; std::sort(ks.begin(), ks.end()); ks.erase(std::unique(ks.begin(), ks.end()), ks.end());
		for (int s : ks) {
			if (s >= fillFrom) continue;
			if (s == 0 && landed) continue;
			hit(g[s], KICK, (s == 0 ? 88 : 70) + 24 * e, -0.001);
		}
		for (int s : {7, 9, 15, 3}) {
			if (s >= fillFrom) continue;
			if (r.u() < p.comp * (0.2 + 0.35 * e)) { hit(g[s], SNARE, 28 + 10 * e, 0.005); stats.comps++; }
		}
		int n = 16 - fillFrom;
		for (int i = 0; i < n; i++) {
			int s = fillFrom + i; double xx = (i + 1) / (double)n;
			if (fill == 1 && i % 2 == 1) { if (r.u() < 0.5) continue; }
			int voice = SNARE;
			if (fill == 2 && e > 0.45 && i >= n - 4) voice = i < n - 2 ? TOM_HI : TOM_LO;
			hit(g[s], voice, 50 + 30 * e + 24 * xx, -0.003 * xx, 0.8);
		}
		if (fill) landing = fill == 2 ? 2 : 1;
	}

	// ------------------------------------------------------------ house: four on the floor
	void houseBar(int bar, int chorusBars, double e) {
		double g[16]; grid16(false, g);
		drift = 0.6 * drift + h.g() * 0.0006;
		int fill = pickFill(bar, chorusBars, e);
		int fillFrom = 16;
		if (fill) { fillFrom = fill == 1 ? 12 : 8; stats.fills++; }
		bool crash = landing == 2 && e > 0.4;
		landing = 0;
		for (int s = 0; s < 16; s++) {
			double beat = g[s];
			if (s % 4 == 0) {
				if (s < fillFrom || fill == 1) hit(beat, KICK, 98 + 18 * e, -0.001, 0.4);
				if ((s == 4 || s == 12) && e > 0.25 && s < fillFrom) hit(beat, SNARE, 70 + 26 * e, 0.003, 0.5);
			} else if (s % 4 == 2) {
				if (s < fillFrom) hit(beat, e > 0.35 ? HATO : HATC, 58 + 30 * e, 0.0, 0.5);
			} else {
				if (s < fillFrom) {
					if (r.u() < std::max(0.0, e - 0.4) * 1.5) hit(beat, HATC, 32 + 20 * e, 0.0, 0.5);
					int ps = pianoAt(beat, 0.07);
					if (ps >= 0 && strong(ps)) {
						stats.pushes++;
						if (r.u() < p.listen * p.comp) { hit(beat, SNARE, 34 + 16 * e, 0.004); stats.caught++; }
					}
				}
			}
		}
		if (crash) hit(0.0, CRASH, 80 + 30 * e, 0.002);
		int n = 16 - fillFrom;
		for (int i = 0; i < n; i++) {
			int s = fillFrom + i; double xx = (i + 1) / (double)n;
			hit(g[s], SNARE, 44 + 30 * e + 36 * xx, -0.002 * xx, 0.6);
		}
		if (fill) landing = fill == 2 ? 2 : 1;
	}
};

} // namespace rider
