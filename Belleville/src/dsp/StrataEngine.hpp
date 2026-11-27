// Strata: the chord player and the trio's harmony leader. Builds the chart from a style's progression,
// lets DRIFT wander away from it (and back) once a loop, voices the chords and plays them in a rhythm.
#pragma once
#include "Link.hpp"
#include <vector>

namespace bv {

enum Rhythm { RH_PAD, RH_STAB1, RH_OFFBEAT, RH_CHICAGO, RH_PUSH, RH_SKANK, RH_DUB, RH_BROKEN, RH_COUNT };
static inline const char* rhythmName(int r) {
	static const char* N[RH_COUNT] = {"pad", "stab on one", "offbeat", "chicago 3-3-2", "push", "skank", "dub", "broken"};
	return N[std::max(0, std::min(RH_COUNT - 1, r))];
}
enum Voicing { VO_CLOSE, VO_DROP2, VO_OPEN, VO_ROOTLESS, VO_MEMORY, VO_SHELL, VO_COUNT };
static inline const char* voicingName(int v) {
	static const char* N[VO_COUNT] = {"close", "drop 2", "open", "rootless", "chord memory", "shell"};
	return N[std::max(0, std::min(VO_COUNT - 1, v))];
}

// a rhythm: for each sixteenth, the gate length in sixteenths (0 = no hit), and how optional it is (0 = always)
struct RhythmDef { float len[16]; float opt[16]; };
static inline const RhythmDef& rhythmDef(int r) {
	static const RhythmDef R[RH_COUNT] = {
		// PAD (held; handled specially, one hit per chord)
		{{0}, {0}},
		// STAB on one
		{{1.5f, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, .7f, 0, 0, 0, 0, 0, 0, 0}},
		// OFFBEAT
		{{0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0}, {0, 0, 0, 0, 0, 0, .3f, 0, 0, 0, 0, 0, 0, 0, .3f, 0}},
		// CHICAGO 3-3-2 piano
		{{1.5f, 0, 0, 1.5f, 0, 0, 1.5f, 0, 0, 0, 1, 0, 1.5f, 0, 0, 0}, {0, 0, 0, 0, 0, 0, .2f, 0, 0, 0, .6f, 0, .3f, 0, 0, 0}},
		// PUSH (the last hit anticipates the next chord)
		{{3, 0, 0, 0, 0, 0, 2, 0, 0, 0, 2, 0, 0, 0, 2.5f, 0}, {0, 0, 0, 0, 0, 0, .4f, 0, 0, 0, .5f, 0, 0, 0, 0, 0}},
		// SKANK (2 and 4)
		{{0, 0, 0, 0, .8f, 0, 0, 0, 0, 0, 0, 0, .8f, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
		// DUB (one stab, late, and an answer)
		{{0, 0, 0, 1.2f, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, .6f, 0, 0, 0, 0, 0}},
		// BROKEN
		{{1, 0, 0, 1, 0, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0}, {0, 0, 0, .3f, 0, 0, 0, .2f, 0, 0, .4f, 0, 0, .5f, 0, 0}},
	};
	return R[std::max(0, std::min(RH_COUNT - 1, r))];
}

struct StrataEngine {
	// knobs (set by the host every step)
	int style = ST_DETROIT, progSel = 0, key = 5, mode = M_AUTO, rhythm = RH_OFFBEAT, voicing = VO_CLOSE;
	float colour = 0.5f, spread = 0.4f, drift = 0.25f, energy = 0.5f;
	int lengthSel = 1;             // harmonic rhythm: 0 = x0.5, 1 = as written, 2 = x2, 3 = x4
	int registerOct = 0;           // right-click: -2..+2
	bool hold = false;

	// state
	Rng rng;
	int progId = -1;               // the progression loaded (index into progDef)
	int baseN = 0, baseDeg[16] = {}, baseQ[16] = {}, baseBeats[16] = {};
	int curDeg[16] = {}, curQ[16] = {};
	int loaded[5] = {-1, -1, -1, -1, -1};   // style, progSel, lengthSel, key, mode the chart was built for
	bool nextArmed = false;
	int driftedCount = 0;
	int prevVoicing[6] = {}; int prevN = 0;
	int memShape[6] = {}; int memN = 0;     // the chord memory shape (intervals from the root)
	bool memSet = false;
	// external chords (CHORDS input)
	bool external = false; int extN = 0; int extNotes[16] = {};
	Chart chart;
	int lastSlot = -1;
	uint16_t barMask = 0;

	StrataEngine() { rng.seed(12345); }

	int progSelWrapped() const { int k = progsInStyle(style); return k ? ((progSel % k) + k) % k : 0; }

	void loadProg(int id) {
		const ProgDef& p = progDef(id);
		progId = id;
		baseN = p.n;
		for (int i = 0; i < p.n; i++) { baseDeg[i] = p.deg[i]; baseQ[i] = p.q[i]; baseBeats[i] = p.beats[i]; curDeg[i] = p.deg[i]; curQ[i] = p.q[i]; }
		driftedCount = 0;
		memSet = false;
	}
	int effectiveMode() const { return mode == M_AUTO ? progDef(std::max(0, progId)).mode : mode; }

	// rebuild the chart from the progression (and its drifted chords), the key and LENGTH
	void buildChart() {
		static const float mult[4] = {0.5f, 1.f, 2.f, 4.f};
		float m = mult[std::max(0, std::min(3, lengthSel))];
		Chart c;
		c.key = key;
		c.scale = modeMask(key, effectiveMode());
		if (external && extN > 0) {
			c.n = 1; c.loopSteps = 16; c.start[0] = 0; c.len[0] = 16;
			c.root[0] = ((extNotes[0] % 12) + 12) % 12; c.qual[0] = Q_MAJ;   // the qualities are replaced by the notes below
		} else {
			int pos = 0;
			c.n = std::min(baseN, 16);
			for (int i = 0; i < c.n; i++) {
				int steps = std::max(2, (int)std::lround(baseBeats[i] * 4 * m));
				c.start[i] = pos; c.len[i] = steps; pos += steps;
				c.root[i] = (key + curDeg[i] + 120) % 12;
				c.qual[i] = colourQual(curQ[i], colour);
			}
			c.loopSteps = std::max(4, pos);
		}
		chart = c;
	}

	void checkReload() {
		int want[5] = {style, progSelWrapped(), lengthSel, key, mode};
		bool progChange = want[0] != loaded[0] || want[1] != loaded[1];
		if (progChange) loadProg(progIndex(style, want[1]));
		for (int i = 0; i < 5; i++) loaded[i] = want[i];
		buildChart();
	}

	// once a loop: DRIFT swaps a chord for a relative (and sometimes wanders back home)
	void driftLoop() {
		if (hold || drift <= 0.001f || baseN <= 0) return;
		int changes = (rng.uni() < drift) ? 1 + (rng.uni() < drift * drift ? 1 : 0) : 0;
		for (int k = 0; k < changes; k++) {
			int i = rng.range(baseN);
			bool moved = curDeg[i] != baseDeg[i] || curQ[i] != baseQ[i];
			if (moved && rng.uni() < 0.45f - 0.3f * drift) { curDeg[i] = baseDeg[i]; curQ[i] = baseQ[i]; continue; }
			int d = curDeg[i], q = curQ[i];
			bool minor = qdef(q).minor;
			switch (rng.range(6)) {
				case 0: q = rng.uni() < 0.5f ? qdef(q).richer : qdef(q).simpler; break;                 // colour
				case 1: if (minor) { d += 3; q = rng.uni() < 0.5f ? Q_MAJ7 : Q_69; } else { d += 9; q = Q_MIN7; } break; // relative
				case 2: d += 5; break;                                                                    // up a fourth, same shape
				case 3: q = minor ? Q_MIN11 : Q_9SUS4; break;                                             // suspend / open up
				case 4: if (style == ST_WAREHOUSE || style == ST_DUB || style == ST_MINIMAL) d += (rng.uni() < 0.5f ? 2 : 10); else { d += 10; q = Q_69; } break; // a tone away
				case 5: d = baseDeg[(i + 1) % baseN]; q = baseQ[(i + 1) % baseN]; break;                // borrow the next chord early
			}
			curDeg[i] = ((d % 12) + 12) % 12; curQ[i] = q;
		}
		driftedCount = 0;
		for (int i = 0; i < baseN; i++) if (curDeg[i] != baseDeg[i] || curQ[i] != baseQ[i]) driftedCount++;
	}

	void newSeed(uint32_t s) { rng.seed(s); }
	void setExternal(int n, const int* notes) {
		bool was = external;
		external = n >= 2;
		extN = std::min(n, 16);
		for (int i = 0; i < extN; i++) extNotes[i] = notes[i];
		std::sort(extNotes, extNotes + extN);
		if (was != external) buildChart();
	}

	// pick notes for a chord with voice leading from the last one
	int voice(int root, int q, int* out) {
		int pcs[8], n = 0;
		int centre = 57 + 12 * registerOct;   // A3
		if (external && extN > 0) {
			for (int i = 0; i < extN && n < 6; i++) pcs[n++] = extNotes[i] - extNotes[0];
			root = ((extNotes[0] % 12) + 12) % 12;
		} else {
			const QualDef& d = qdef(q);
			for (int i = 0; i < d.n; i++) pcs[n++] = d.iv[i];
		}
		// close order: fold the extensions into one octave so inversions stack in pitch order
		if (voicing != VO_MEMORY) {
			for (int i = 0; i < n; i++) pcs[i] = ((pcs[i] % 12) + 12) % 12;
			std::sort(pcs, pcs + n);
			n = (int)(std::unique(pcs, pcs + n) - pcs);
		}
		if (voicing == VO_ROOTLESS && n >= 4) { for (int i = 1; i < n; i++) pcs[i - 1] = pcs[i]; n--; }
		if (voicing == VO_SHELL && n >= 4) {
			int third = -1, sev = -1;
			for (int i = 0; i < n; i++) { if (pcs[i] == 3 || pcs[i] == 4 || pcs[i] == 5) third = third < 0 ? pcs[i] : third; if (pcs[i] == 10 || pcs[i] == 11 || pcs[i] == 9) sev = pcs[i]; }
			int s3[3] = {0, third < 0 ? 7 : third, sev < 0 ? 7 : sev}; n = 3; for (int i = 0; i < 3; i++) pcs[i] = s3[i];
		}
		if (voicing == VO_MEMORY) {
			if (!memSet) { memN = n; for (int i = 0; i < n; i++) memShape[i] = pcs[i]; memSet = true; }
			n = memN; for (int i = 0; i < n; i++) pcs[i] = memShape[i];
			int base = centre - 5 + ((root - (centre - 5)) % 12 + 12) % 12;
			for (int i = 0; i < n; i++) out[i] = base + pcs[i];
			n = applySpread(out, n);
			remember(out, n);
			return n;
		}
		// candidates: each inversion, two octaves; score by movement from the last chord and distance from the centre
		int best[6] = {}, bestN = 0; float bestScore = 1e9f;
		for (int inv = 0; inv < n; inv++) {
			for (int oct = -1; oct <= 1; oct++) {
				int c[6], m = n;
				int lowPc = (root + pcs[inv]) % 12;
				int low = centre - 6 + ((lowPc - (centre - 6)) % 12 + 12) % 12 + 12 * oct;
				c[0] = low;
				for (int k = 1; k < n; k++) {
					int pc = (root + pcs[(inv + k) % n]) % 12;
					int x = c[k - 1] + 1;
					while (((x % 12) + 12) % 12 != pc) x++;
					c[k] = x;
				}
				if (voicing == VO_DROP2 && m >= 3) { int t = c[m - 2] - 12; for (int k = m - 2; k > 0; k--) c[k] = c[k - 1]; c[0] = t; }
				if (voicing == VO_OPEN) {
					// wide: every other note up an octave
					std::sort(c, c + m);
					for (int k = 1; k < m; k += 2) c[k] += 12;
				}
				std::sort(c, c + m);
				float score = 0.f;
				if (prevN > 0) {
					for (int k = 0; k < m; k++) {
						int dmin = 99;
						for (int j = 0; j < prevN; j++) dmin = std::min(dmin, std::abs(c[k] - prevVoicing[j]));
						score += dmin;
					}
				}
				float mid = 0.5f * (c[0] + c[m - 1]);
				score += 0.6f * std::fabs(mid - (centre + 5));
				if (c[0] < centre - 12 || c[m - 1] > centre + 24) score += 40.f;
				if (score < bestScore) { bestScore = score; bestN = m; for (int k = 0; k < m; k++) best[k] = c[k]; }
			}
		}
		for (int k = 0; k < bestN; k++) out[k] = best[k];
		bestN = applySpread(out, bestN);
		remember(out, bestN);
		return bestN;
	}
	int applySpread(int* c, int n) {
		// SPREAD lifts every other note (from the second up) an octave, more of them as it turns up
		int lifts = (int)std::floor(spread * 3.f + 0.001f);
		int done = 0;
		for (int k = n - 2; k >= 1 && done < lifts; k -= 2) { c[k] += 12; done++; }
		std::sort(c, c + n);
		// and over 90 %, doubles the root an octave down
		if (spread > 0.9f && n < 6) { int r = c[0] - 12; for (int k = n; k > 0; k--) c[k] = c[k - 1]; c[0] = r; n++; }
		return n;
	}
	void remember(int* c, int n) { prevN = std::min(n, 6); for (int k = 0; k < prevN; k++) prevVoicing[k] = c[k]; }

	// one sixteenth. step counts from 0 at reset.
	NoteEvent tick(int64_t step) {
		NoteEvent e;
		int loopPos = (int)(step % chart.loopSteps);
		if (loopPos == 0 && step > 0) {
			if (nextArmed) { nextArmed = false; progSel++; }
			driftLoop();
			checkReload();
			loopPos = 0;
		}
		int slot = chart.slotAt(loopPos);
		bool change = (slot != lastSlot) || (loopPos == 0 && chart.n == 1 && step == 0);
		if (external) change = change || extChanged;
		extChanged = false;
		lastSlot = slot;
		e.change = change && (chart.n > 1 || step == 0 || external);
		int bar16 = (int)(step % 16);
		if (bar16 == 0) barMask = 0;
		float len = 0.f;
		int useSlot = slot;
		if (rhythm == RH_PAD) {
			if (change || bar16 == 0) {
				int steps = std::min(chart.stepsToChange(step), 16 - bar16);
				if (chart.n == 1) steps = 16 - bar16;
				len = std::max(1.f, steps - 0.25f);
			}
		} else {
			const RhythmDef& r = rhythmDef(rhythm);
			len = r.len[bar16];
			if (len > 0.f && r.opt[bar16] > 0.f) {
				// ENERGY decides how many optional hits play; the choice is steady within a loop
				float p = 1.f - r.opt[bar16] * (1.2f - 1.4f * energy);
				uint32_t h = (uint32_t)(bar16 * 2654435761u) ^ (uint32_t)((step / 16) * 40503u);
				h ^= h >> 13; h *= 0x5bd1e995; h ^= h >> 15;
				if ((h & 0xFFFF) / 65536.f > p) len = 0.f;
			}
			// push: a hit that ends within two sixteenths of a change plays the next chord
			if (rhythm == RH_PUSH || rhythm == RH_BROKEN || rhythm == RH_CHICAGO) {
				int toCh = chart.stepsToChange(step);
				if (len > 0.f && toCh <= 2 && chart.n > 1 && bar16 >= 12) useSlot = chart.nextSlot(step);
			}
			// a chord change between hits: play it anyway on the change (so the band hears it) when ENERGY is up
			if (change && len <= 0.f && energy > 0.6f) len = 1.f;
		}
		if (len > 0.f) {
			e.hit = true;
			e.gateSteps = len;
			int notes[8];
			int n = voice(chart.root[useSlot], chart.qual[useSlot], notes);
			e.n = std::min(n, 6);
			for (int k = 0; k < e.n; k++) {
				e.note[k] = notes[k];
				float v = 0.75f + ((bar16 % 4) == 0 ? 0.1f : 0.f) + (e.change ? 0.1f : 0.f);
				e.vel[k] = std::min(1.f, v - 0.04f * k);
			}
			barMask |= 1 << bar16;
		}
		return e;
	}
	bool extChanged = false;
	void markExternalChange() { extChanged = true; }

	// the hits this bar, as far as they are known (the whole rhythm, for the others to plan around)
	uint16_t plannedMask() const {
		if (rhythm == RH_PAD) return 1;
		uint16_t m = 0;
		const RhythmDef& r = rhythmDef(rhythm);
		for (int i = 0; i < 16; i++) if (r.len[i] > 0.f) m |= 1 << i;
		return m;
	}
};

} // namespace bv
