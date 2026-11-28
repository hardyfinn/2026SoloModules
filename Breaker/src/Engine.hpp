// Breaker - pattern generator + player (no Rack dependencies; the module and the
// test renderer both drive this).
//
// The pattern is 4 lanes (kick, snare, hat, other) x up to 4 bars x 16 steps.
// Every cell holds three random numbers rolled when you press NEW:
//   rMain  - the cell plays when rMain < the style's chance for that step
//            (DENSITY bends the chances, so turning it adds/removes hits smoothly
//            and turning it back brings the same pattern back)
//   rGhost - a quiet ghost note plays when rGhost < the style's ghost chance x GHOSTS
//   rHit   - which of the break's hits of that type to play (VARIETY)
// MUTATE re-rolls a few cells' numbers at each bar line; LOCK stops that. NEW
// re-rolls everything that isn't in a locked lane. STYLE only changes the chances.
#pragma once
#include "Analysis.hpp"
#include <atomic>
#include <cstring>
#include <random>

namespace breaker {

enum Style { ST_BREAK = 0, ST_BREAKS, ST_JUNGLE, ST_HIPHOP, ST_TECHNO, NUM_STYLES };
static const char* const STYLE_NAMES[NUM_STYLES] = {"BREAK", "BREAKS", "JUNGLE", "HIP HOP", "BROKEN TECHNO"};
static const char* const LANE_NAMES[NUM_LANES] = {"Kick", "Snare", "Hat", "Other"};

// Chances per 16th as digits: 0 = never, 1-8 = n/9, 9 = always.
struct StyleTable { const char* main[NUM_LANES]; const char* ghost[NUM_LANES]; };
inline const StyleTable& styleTable(int s) {
	static const StyleTable T[NUM_STYLES] = {
		// BREAK: filled in from the break itself; these are the faint extras DENSITY can add
		{{"1001001010010010", "0000101000101001", "2121212121212121", "0000000100000010"},
		 {"0000000000000000", "0000000000000000", "0000000000000000", "0000000000000000"}},
		// BREAKS: funk breakbeat, kick on 1 and the "and of 3", backbeat snare, 8th hats
		{{"9002003040900030", "0000900000009000", "9090909090909090", "0000000000000020"},
		 {"0000000302040306", "0102000403020003", "0303030303030303", "0000000000000000"}},
		// JUNGLE: double kick, snare on 2 and 4 plus the classic syncopated hits, busy ghosts
		{{"9060000000830000", "0000900506009007", "6060606060606060", "0000000000000030"},
		 {"0000000000000000", "0302030103020300", "0202020202020202", "0000000200000002"}},
		// HIP HOP: boom bap - heavy kick, fat backbeat, straight 8th hats, few ghosts
		{{"9000000460700100", "0000900000009000", "9090909090909090", "0000000000000000"},
		 {"0000000000000000", "0000000100020103", "0101010101010101", "0000000000000100"}},
		// BROKEN TECHNO: no four-on-the-floor; kicks dodge the beat, off-8th hats lead
		{{"9005004000600300", "0000700000008020", "4494449444944494", "0000040003000204"},
		 {"0000000000000000", "0000000300030003", "0000000000000000", "0000000000000000"}},
	};
	return T[s < 0 ? 0 : s >= NUM_STYLES ? NUM_STYLES - 1 : s];
}
inline float digit(char c) { return c == '9' ? 1.f : (c - '0') / 9.f; }

enum CellState { C_OFF = 0, C_GHOST, C_HIT, C_ACCENT };
enum Force : int8_t { F_NONE = -1, F_OFF = 0, F_GHOST = 1, F_HIT = 2 };
static const int CELLS = MAX_BARS * STEPS;

struct Pattern {
	float rMain[NUM_LANES][CELLS], rGhost[NUM_LANES][CELLS], rHit[NUM_LANES][CELLS];
	int8_t force[NUM_LANES][CELLS];
};

struct Knobs {
	int style = ST_BREAK;
	float density = 0.5f, mutate = 0.f, ghosts = 0.5f, fills = 0.25f, swing = 0.5f;
	float variety = 0.f, decay = 1.f, level = 1.f, pitch = 0.f;   // pitch in octaves (knob + V/OCT)
	int bars = 1;
	bool lock = false, run = true;
	bool clockConnected = false;
	float clock = 0.f, reset = 0.f, newTrig = 0.f;
	bool laneOutConnected[NUM_LANES] = {};
};

struct Outs {
	float lane[NUM_LANES] = {};
	float L = 0, R = 0;
	float gate[NUM_LANES] = {};
	float accent = 0, clk = 0;
};

struct BTrigger {
	bool high = false;
	bool process(float v) {
		if (high) { if (v <= 0.1f) high = false; return false; }
		if (v >= 1.f) { high = true; return true; }
		return false;
	}
};

struct BVoice {
	int hit = -1;
	double pos = 0, end = 0;
	float gain = 0, fadeIn = 0, fadeOut = 1;
	bool releasing = false;
};

// Small lock-free queue for edits from the GUI thread.
struct EditQueue {
	static const int SIZE = 64;
	std::atomic<int> head{0}, tail{0};
	int buf[SIZE];
	void push(int v) {
		int h = head.load(), n = (h + 1) % SIZE;
		if (n == tail.load()) return;
		buf[h] = v;
		head.store(n);
	}
	bool pop(int& v) {
		int t = tail.load();
		if (t == head.load()) return false;
		v = buf[t];
		tail.store((t + 1) % SIZE);
		return true;
	}
};

struct BreakEngine {
	const BreakData* brk = nullptr;
	Pattern pat, base;               // base = as rolled by the last NEW ("Clear mutation")
	bool laneLock[NUM_LANES] = {};
	std::mt19937 rng{12345};
	EditQueue edits;                 // GUI -> audio: cell toggles, lane locks, NEW, clear

	// clock
	BTrigger clockT, resetT, newT;
	double sinceClock = 1e9, period = 0.125, phase = 0;
	int step = -1;                   // absolute step counter inside the pattern (0..bars*16-1)
	long barCount = 0;               // bars played since reset (for fills/phrases)
	bool resetPending = false, wasRunning = false;
	double pendingDelay = -1;        // swung step waiting to fire
	int pendingStep = -1;

	// fills: overlay for the current bar
	bool fillActive = false;
	int fillFrom = 16;
	int8_t fillCell[NUM_LANES][STEPS];
	float fillGain[NUM_LANES][STEPS];

	// voices: two per lane so a new hit can fade the last one out without a click
	BVoice voice[NUM_LANES][2];
	int cur[NUM_LANES] = {};
	float gateT[NUM_LANES] = {}, accentT = 0, clkT = 0;

	// display info
	std::atomic<int> dispStep{-1};
	uint32_t mutatedBar[NUM_LANES][CELLS];   // barCount+1 when the cell last mutated
	std::atomic<int> lastHit[NUM_LANES];

	BreakEngine() {
		for (int l = 0; l < NUM_LANES; l++) lastHit[l] = -1;
		std::memset(mutatedBar, 0, sizeof(mutatedBar));
		roll(true);
	}

	float rnd() { return std::uniform_real_distribution<float>(0.f, 1.f)(rng); }

	void rollCell(int l, int c) {
		pat.rMain[l][c] = rnd(); pat.rGhost[l][c] = rnd(); pat.rHit[l][c] = rnd();
		pat.force[l][c] = F_NONE;
	}
	// NEW: re-roll every lane that isn't locked (all = ignore lane locks)
	void roll(bool all) {
		for (int l = 0; l < NUM_LANES; l++) {
			if (laneLock[l] && !all) continue;
			for (int c = 0; c < CELLS; c++) { rollCell(l, c); mutatedBar[l][c] = 0; }
		}
		base = pat;
	}
	void clearMutation() { pat = base; std::memset(mutatedBar, 0, sizeof(mutatedBar)); }

	void setBreak(const BreakData* b) {
		brk = b;
		for (auto& lv : voice) for (auto& v : lv) v.hit = -1;
	}

	// ---- what a cell does, given the knobs
	float chanceMain(const Knobs& k, int l, int c) const {
		int s = c % STEPS;
		float p;
		if (k.style == ST_BREAK && brk) {
			int lb = (c / STEPS) % std::max(1, std::min(brk->loopBars, MAX_LOOP_BARS));
			int h = brk->orig[lb][s][l];
			if (h >= 0 && !brk->hits[h].ghost) p = brk->hits[h].rel > 0.7f ? 1.f : 0.9f;
			else p = h >= 0 ? 0.f : 0.08f * digit(styleTable(ST_BREAK).main[l][s]) * 9.f / 2.f;
		}
		else p = digit(styleTable(k.style).main[l][s]);
		if (p <= 0.f || p >= 1.f) return p;
		// DENSITY: 0.5 = as written; down thins, up fills (zero chances stay zero)
		float e = std::pow(2.f, (0.5f - k.density) * 6.f);
		return std::pow(p, e);
	}
	float chanceGhost(const Knobs& k, int l, int c) const {
		int s = c % STEPS;
		float p;
		if (k.style == ST_BREAK && brk) {
			int lb = (c / STEPS) % std::max(1, std::min(brk->loopBars, MAX_LOOP_BARS));
			int h = brk->orig[lb][s][l];
			p = (h >= 0 && brk->hits[h].ghost) ? 1.f : 0.f;
		}
		else p = digit(styleTable(k.style).ghost[l][s]);
		return std::min(1.f, p * k.ghosts * 2.f);
	}
	int cellState(const Knobs& k, int l, int c) const {
		int8_t f = pat.force[l][c];
		int st;
		if (f == F_OFF) return C_OFF;
		if (f == F_GHOST) return C_GHOST;
		if (f == F_HIT || pat.rMain[l][c] < chanceMain(k, l, c)) st = C_HIT;
		else if (pat.rGhost[l][c] < chanceGhost(k, l, c)) return C_GHOST;
		else return C_OFF;
		int s = c % STEPS;
		if ((l == KICK && s == 0) || (l == SNARE && (s == 4 || s == 12))) st = C_ACCENT;
		return st;
	}

	// which of the break's hits a lane plays
	const std::vector<int>* laneHits(int l) const {
		if (!brk) return nullptr;
		static const int fb[NUM_LANES][3] = {{KICK, OTHER, SNARE}, {SNARE, OTHER, HAT}, {HAT, OTHER, SNARE}, {OTHER, SNARE, HAT}};
		for (int t : fb[l]) if (!brk->byType[t].empty()) return &brk->byType[t];
		return nullptr;
	}
	// returns hit index (or -1) and the gain to play it at
	int chooseHit(const Knobs& k, int l, int c, int state, float r, float& gain) const {
		const std::vector<int>* v = laneHits(l);
		if (!v) return -1;
		int n = (int) v->size();
		float var = k.variety;
		// BREAK style: the break's own hit on its own step, unless VARIETY swaps it
		if (k.style == ST_BREAK && brk->byType[l].size()) {
			int lb = (c / STEPS) % std::max(1, std::min(brk->loopBars, MAX_LOOP_BARS));
			int h = brk->orig[lb][c % STEPS][l];
			float r2 = std::fmod(r * 7.31f, 1.f);
			if (h >= 0 && r2 >= var) { gain = 1.f; return h; }
		}
		if (state == C_GHOST) {
			int range = std::max(1, (int) std::ceil(std::max(var, 0.34f) * n));
			int idx = n - 1 - std::min(range - 1, (int) (r * range));
			const Hit& h = brk->hits[(*v)[idx]];
			gain = h.ghost ? 1.f : 0.45f;
			return (*v)[idx];
		}
		int range = std::max(1, (int) std::ceil(var * n));
		int idx = std::min(range - 1, (int) (r * range));
		gain = state == C_ACCENT ? 1.f : 0.88f;
		return (*v)[idx];
	}

	void trigger(const Knobs& k, int l, int hit, float gain) {
		gateT[l] = 0.005f;
		lastHit[l] = hit;
		if (!brk || hit < 0) return;
		const Hit& h = brk->hits[hit];
		BVoice& old = voice[l][cur[l]];
		if (old.hit >= 0) old.releasing = true;
		cur[l] ^= 1;
		BVoice& v = voice[l][cur[l]];
		v.hit = hit;
		v.pos = (double) h.start;
		double len = (double) (h.end - h.start);
		double frac = 0.06 + 0.94 * std::pow(k.decay, 1.5);
		v.end = h.start + std::max(len * frac, std::min(len, brk->audio->rate * 0.015));
		v.gain = gain;
		v.fadeIn = 0.f; v.fadeOut = 1.f; v.releasing = false;
	}

	void makeFill(const Knobs& k) {
		fillActive = false;
		if (rnd() >= k.fills) return;
		fillActive = true;
		fillFrom = (k.fills > 0.5f && rnd() < 0.5f) ? 8 : 12;
		float acc = 0;
		for (int s = 0; s < STEPS; s++)
			for (int l = 0; l < NUM_LANES; l++) { fillCell[l][s] = -1; fillGain[l][s] = 0; }
		for (int s = fillFrom; s < STEPS; s++) {
			float ramp = 0.45f + 0.55f * (s - fillFrom) / (float) (STEPS - 1 - fillFrom);
			bool sn = rnd() < 0.78f;
			fillCell[SNARE][s] = sn ? C_HIT : C_OFF; fillGain[SNARE][s] = ramp;
			fillCell[OTHER][s] = (!sn && rnd() < 0.6f) ? C_HIT : C_OFF; fillGain[OTHER][s] = ramp;
			fillCell[KICK][s] = (s == fillFrom || rnd() < 0.15f) ? C_HIT : C_OFF; fillGain[KICK][s] = 0.9f;
			fillCell[HAT][s] = C_OFF;
			acc += ramp;
		}
		(void) acc;
	}

	void mutateBar(const Knobs& k) {
		if (k.lock || k.mutate <= 0.f) return;
		int cells = k.bars * STEPS;
		int tries = (int) std::floor(k.mutate * 7.f + rnd());
		for (int i = 0; i < tries; i++) {
			int l = (int) (rnd() * NUM_LANES) % NUM_LANES;
			int c = (int) (rnd() * cells) % cells;
			if (laneLock[l] || pat.force[l][c] != F_NONE) continue;
			pat.rMain[l][c] = rnd(); pat.rGhost[l][c] = rnd(); pat.rHit[l][c] = rnd();
			mutatedBar[l][c] = (uint32_t) barCount + 1;
		}
	}

	// play one 16th
	void playStep(const Knobs& k, int s) {
		int bar = s / STEPS, sb = s % STEPS;
		if (sb == 0) {
			if (step >= 0 || barCount > 0) mutateBar(k);
			int phrase = std::max(4, k.bars);
			bool last = (barCount % phrase) == phrase - 1;
			if (last) makeFill(k); else fillActive = false;
		}
		clkT = 0.005f;
		dispStep = s;
		bool accent = false;
		for (int l = 0; l < NUM_LANES; l++) {
			int st = cellState(k, l, s);
			float extra = 1.f;
			if (fillActive && sb >= fillFrom) { st = fillCell[l][sb]; extra = fillGain[l][sb]; }
			if (st == C_OFF) continue;
			float g = 1.f;
			int h = chooseHit(k, l, s, st, pat.rHit[l][s] * 0.999f, g);
			if (fillActive && sb >= fillFrom) {
				// fills use a different snare/tom each step
				const std::vector<int>* v = laneHits(l);
				if (v) h = (*v)[(int) (rnd() * v->size()) % v->size()];
			}
			trigger(k, l, h, g * extra);
			if (st == C_ACCENT) accent = true;
		}
		if (accent) accentT = 0.005f;
		if (sb == STEPS - 1) barCount++;
		(void) bar;
	}

	void doEdits(Knobs& k) {
		int e;
		while (edits.pop(e)) {
			int op = e >> 16, arg = e & 0xFFFF;
			if (op == 1 || op == 2) {          // toggle hit / ghost on cell
				int l = arg / CELLS, c = arg % CELLS;
				if (l < 0 || l >= NUM_LANES) continue;
				int st = cellState(k, l, c);
				if (op == 1) pat.force[l][c] = (st == C_HIT || st == C_ACCENT) ? F_OFF : F_HIT;
				else pat.force[l][c] = st == C_GHOST ? F_OFF : F_GHOST;
			}
			else if (op == 3 && arg < NUM_LANES) laneLock[arg] = !laneLock[arg];
			else if (op == 4) roll(false);
			else if (op == 5) clearMutation();
			else if (op == 6) { roll(true); }
			else if (op == 7) { for (auto& lf : pat.force) for (auto& f : lf) f = F_NONE; }
		}
	}

	void advance(const Knobs& k) {
		int len = k.bars * STEPS;
		if (pendingStep >= 0) { playStep(k, pendingStep); pendingStep = -1; pendingDelay = -1; }
		step = (step + 1) % len;
		bool odd = step & 1;
		double delay = odd ? (2.0 * k.swing - 1.0) * period : 0.0;
		if (delay > 1e-5) { pendingStep = step; pendingDelay = delay; }
		else playStep(k, step);
	}

	void doReset() {
		step = -1; barCount = 0; pendingStep = -1; pendingDelay = -1; phase = 0;
		fillActive = false;
	}

	void process(Knobs& k, float dt, Outs& o) {
		doEdits(k);
		if (newT.process(k.newTrig)) roll(false);
		bool reset = resetT.process(k.reset);
		k.bars = std::max(1, std::min(MAX_BARS, k.bars));
		if (step >= k.bars * STEPS) step = step % (k.bars * STEPS);

		// ---- clock
		sinceClock += dt;
		if (k.clockConnected) {
			if (reset) doReset();
			if (clockT.process(k.clock)) {
				if (sinceClock < 2.0) period = sinceClock;
				sinceClock = 0;
				advance(k);
			}
		}
		else {
			const float ratio = std::pow(2.f, k.pitch);
			const double bpm = (brk ? brk->bpm : 120.0) * ratio;
			period = 15.0 / std::max(20.0, bpm);
			if (k.run && !wasRunning) { doReset(); }
			wasRunning = k.run;
			if (reset) doReset();
			if (k.run) {
				if (step < 0) advance(k);
				else {
					phase += dt / period;
					if (phase >= 1.0) { phase -= 1.0; advance(k); }
				}
			}
		}
		if (pendingStep >= 0) {
			pendingDelay -= dt;
			if (pendingDelay <= 0) { int s = pendingStep; pendingStep = -1; playStep(k, s); }
		}

		// ---- voices
		const float ratio = std::pow(2.f, k.pitch);
		float L = 0, R = 0;
		for (int l = 0; l < NUM_LANES; l++) {
			float lL = 0, lR = 0;
			for (BVoice& v : voice[l]) {
				if (v.hit < 0 || !brk) continue;
				const Audio& A = *brk->audio;
				double inc = (A.rate * dt) * ratio;
				size_t i = (size_t) v.pos;
				if (i + 1 >= A.size() || v.pos >= v.end) { v.hit = -1; continue; }
				float f = (float) (v.pos - i);
				float sl = A.L[i] + (A.L[i + 1] - A.L[i]) * f;
				float sr = A.R[i] + (A.R[i + 1] - A.R[i]) * f;
				// 1 ms fade in, fade out over the last 4 ms or when choked
				v.fadeIn = std::min(1.f, v.fadeIn + dt / 0.001f);
				float remain = (float) ((v.end - v.pos) / (A.rate * ratio));
				float tail = std::min(1.f, remain / 0.004f);
				if (v.releasing) { v.fadeOut -= dt / 0.003f; if (v.fadeOut <= 0) { v.hit = -1; continue; } }
				float g = v.gain * v.fadeIn * tail * v.fadeOut;
				lL += sl * g; lR += sr * g;
				v.pos += inc;
			}
			o.lane[l] = 0.5f * (lL + lR) * 5.f * k.level;
			if (!k.laneOutConnected[l]) { L += lL; R += lR; }
			gateT[l] -= dt;
			o.gate[l] = gateT[l] > 0 ? 10.f : 0.f;
		}
		o.L = L * 5.f * k.level;
		o.R = R * 5.f * k.level;
		accentT -= dt; clkT -= dt;
		o.accent = accentT > 0 ? 10.f : 0.f;
		o.clk = clkT > 0 ? 10.f : 0.f;
	}
};

} // namespace breaker
