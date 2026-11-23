// TAKT: two-row 16/32-step pitch sequencer logic. No Rack dependency; tested in test/render.cpp.
#pragma once
#include <cmath>
#include <cstdint>
#include <algorithm>

namespace wb {

enum TaktMode { T_PLAY = 0, T_TIE = 1, T_REST = 2 };

struct TaktPattern {
	float slider[2][16];
	int mode[2][16];
	bool accent[2][16];
	int last[2];
	void clear() {
		for (int r = 0; r < 2; r++) {
			for (int i = 0; i < 16; i++) { slider[r][i] = 0.f; mode[r][i] = T_PLAY; accent[r][i] = false; }
			last[r] = 16;
		}
	}
};

struct TaktSettings {
	int range = 2;            // octaves of slider travel: 1, 2, 4
	bool series = false;      // CHAIN: false parallel, true series
	uint16_t scale = 0xFFF;   // allowed notes, bit n = semitone n above C
	int keyTranspose = 0;     // 0..11 from the keys
	int cvTranspose = 0;      // semitones from the TRANSPOSE input
	bool atBar = false;       // WHEN: false NOW, true BAR
	double gate = 0.5;        // 0.05..1 of the step
};

// nearest allowed semitone; ties go down. Mask repeats every octave.
inline int taktQuantise(int semis, uint16_t mask) {
	if (!(mask & 0xFFF)) return semis;
	for (int d = 0; d < 12; d++) {
		int lo = semis - d, hi = semis + d;
		if (mask & (1 << (((lo % 12) + 12) % 12))) return lo;
		if (mask & (1 << (((hi % 12) + 12) % 12))) return hi;
	}
	return semis;
}

inline int taktSemis(float slider, int range, uint16_t mask) {
	return taktQuantise((int)std::lround(slider * 12.0 * range), mask);
}

struct TaktRowOut { float voct = 0.f; bool gate = false; bool accent = false; };

struct TaktCore {
	TaktPattern pat[8];
	int cur = 0, requested = 0;
	bool switched = false;            // set when cur changed during the last process(); the module reloads its panel
	// per-row state
	int pos[2] = {-1, -1};            // current step 0..15, -1 = before step 1
	int spos = -1;                    // series position 0..31
	int stepSemis[2] = {0, 0};
	bool accentNow[2] = {false, false};
	bool gateOpen[2] = {false, false};
	bool tieHold[2] = {false, false};
	double gateLeft[2] = {0, 0};      // seconds until the gate closes (when not tied)
	double gapLeft[2] = {0, 0};       // 1 ms retrigger gap
	int applied = 0;                  // transposition in effect
	double period = 0.125, sinceClock = 1e9, endLeft = 0;
	bool haveClock = false;

	TaktCore() {
		for (int p = 0; p < 8; p++) pat[p].clear();
		// default pattern 1: semitones 0 12 0 7 0 12 3 10 0 12 0 7 5 12 3 15 at RANGE 2
		const int semi[16] = {0, 12, 0, 7, 0, 12, 3, 10, 0, 12, 0, 7, 5, 12, 3, 15};
		const char* modes = "PRPPPRPPPRPPPPRP";
		for (int i = 0; i < 16; i++) {
			pat[0].slider[0][i] = semi[i] / 24.f;
			pat[0].mode[0][i] = modes[i] == 'P' ? T_PLAY : T_REST;
		}
		pat[0].accent[0][0] = pat[0].accent[0][8] = true;
	}
	void reset() {
		if (sinceClock < 0.001 && haveClock) {           // reset just after a clock: treat as if it came first
			pos[0] = pos[1] = -1; spos = -1;
			step(TaktSettings(), true);
			return;
		}
		pos[0] = pos[1] = -1; spos = -1;
	}
	TaktSettings lastSettings;
	void advanceRow(int r, int stepIndex, const TaktPattern& P, int prow, const TaktSettings& s, int nextMode) {
		int m = P.mode[prow][stepIndex];
		bool wasOpen = gateOpen[r];
		if (m == T_REST) { gateOpen[r] = false; tieHold[r] = false; accentNow[r] = false; return; }
		stepSemis[r] = taktSemis(P.slider[prow][stepIndex], s.range, s.scale);
		accentNow[r] = P.accent[prow][stepIndex];
		if (m == T_TIE) {
			tieHold[r] = true; gateOpen[r] = true; gapLeft[r] = 0;   // through the whole step, no gap (or full-length PLAY after a rest)
			return;
		}
		tieHold[r] = nextMode == T_TIE;     // the next step ties to this one: hold the gate to the clock
		gateOpen[r] = true;
		gateLeft[r] = s.gate >= 0.999 ? 1e9 : s.gate * period;
		gapLeft[r] = wasOpen ? 0.001 : 0.0;
	}
	void step(const TaktSettings& sIn, bool replay = false) {
		const TaktSettings& s = replay ? lastSettings : sIn;
		// where does row A go next?
		int nextA;
		if (s.series) {
			int lastA = pat[cur].last[0], lastB = pat[cur].last[1];
			int len = lastA < 16 ? lastA : 16 + lastB;
			nextA = (spos + 1) % len;
			if (spos < 0) nextA = 0;
		} else {
			nextA = pos[0] < 0 ? 0 : (pos[0] + 1) % pat[cur].last[0];
		}
		bool wrap = nextA == 0;
		if (wrap) {
			if (requested != cur) { cur = requested; switched = true; }
			if (pos[0] >= 0 || spos >= 0) endLeft = 0.001;
			if (s.atBar) applied = s.keyTranspose + s.cvTranspose;
		}
		const TaktPattern& P = pat[cur];
		if (s.series) {
			spos = nextA;
			int row = spos < 16 ? 0 : 1, idx = spos % 16;
			int lastA = P.last[0], len = lastA < 16 ? lastA : 16 + P.last[1];
			int nx = (spos + 1) % len;
			advanceRow(0, idx, P, row, s, P.mode[nx < 16 ? 0 : 1][nx % 16]);
			stepSemis[1] = stepSemis[0]; gateOpen[1] = gateOpen[0]; tieHold[1] = tieHold[0];
			gateLeft[1] = gateLeft[0]; gapLeft[1] = gapLeft[0]; accentNow[1] = accentNow[0];
			pos[0] = pos[1] = idx;
		} else {
			pos[0] = nextA;
			pos[1] = pos[1] < 0 ? 0 : (pos[1] + 1) % P.last[1];
			advanceRow(0, pos[0], P, 0, s, P.mode[0][(pos[0] + 1) % P.last[0]]);
			advanceRow(1, pos[1], P, 1, s, P.mode[1][(pos[1] + 1) % P.last[1]]);
		}
	}
	// one engine sample. Returns outputs for both rows; end = END trigger high.
	void process(double dt, bool clock, bool resetEdge, bool running, const TaktSettings& s, TaktRowOut* out, bool& end) {
		switched = false;
		lastSettings = s;
		if (!running && requested != cur) { cur = requested; switched = true; }
		if (!s.atBar) applied = s.keyTranspose + s.cvTranspose;
		if (resetEdge) reset();
		sinceClock += dt;
		if (clock && running) {
			if (haveClock) period = sinceClock;
			haveClock = true;
			sinceClock = 0;
			step(s);
		}
		for (int r = 0; r < 2; r++) {
			if (!running) gateOpen[r] = false;
			bool g = gateOpen[r];
			if (g && gapLeft[r] > 0) { gapLeft[r] -= dt; g = false; }
			else if (g && !tieHold[r]) {
				gateLeft[r] -= dt;
				if (gateLeft[r] <= 0) { gateOpen[r] = false; g = false; }
			}
			out[r].gate = g;
			out[r].accent = g && accentNow[r];
			out[r].voct = (stepSemis[r] + applied) / 12.f;
		}
		end = endLeft > 0;
		if (endLeft > 0) endLeft -= dt;
	}
};

}  // namespace wb
