// Lindenmayer: three L-system sketches. No Rack code here, so
// test/render.cpp can use the same engines to make the demo WAVs.
//   Sprout - a rhythm that grows: the generation's string is spread over a
//            bar (or read as an endless stream), one more generation every
//            few bars.
//   Graft  - a drum groove whose steps are rewritten by chance rules each
//            generation (ratchet, drop, sprout, home).
//   Bloom  - chord progressions grown by rules on scale degrees, voice-led.
#pragma once
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <algorithm>

namespace lsys {

inline float clampf(float x, float a, float b) { return std::min(std::max(x, a), b); }

// Small fast random numbers (xorshift), seedable so a demo is repeatable.
struct Rng {
	uint32_t s = 0x9E3779B9u;
	explicit Rng(uint32_t seed = 1) { s = seed * 2654435761u + 1u; }
	uint32_t next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
	float uni() { return (next() >> 8) * (1.f / 16777216.f); }
	int below(int n) { return (int) (uni() * n) % std::max(n, 1); }
};

// Grows generations of a context-free L-system. Symbols are small numbers;
// rules[s] is the string that replaces symbol s.
inline std::vector<std::vector<uint8_t>> grow(const std::vector<std::vector<uint8_t>>& rules, uint8_t axiom,
                                              size_t maxLen, int maxGens = 24) {
	std::vector<std::vector<uint8_t>> gens;
	gens.push_back(std::vector<uint8_t>(1, axiom));
	for (int g = 1; g < maxGens; g++) {
		std::vector<uint8_t> next;
		for (uint8_t s : gens.back()) {
			const std::vector<uint8_t>& r = rules[s];
			next.insert(next.end(), r.begin(), r.end());
			if (next.size() > maxLen) break;
		}
		if (next.size() > maxLen) break;
		gens.push_back(next);
	}
	return gens;
}

// Measures the clock so things can happen between clock pulses.
struct ClockTracker {
	float period = 0.125f, since = 0.f;
	bool have = false;
	void tick(float dt) { since += dt; }
	void edge() {
		if (have && since > 0.001f && since < 4.f) period = since;
		have = true;
		since = 0.f;
	}
	float frac() const { return clampf(since / std::max(period, 1e-4f), 0.f, 0.999f); }
};

// ============================================================ Sprout
static const int SPROUT_RULES = 6;
struct SproutRule { const char* name; const char* text; int nsym; const char* r[3]; };
static const SproutRule SPROUT_RULESETS[SPROUT_RULES] = {
	{"FIBONACCI", "A>AB B>A", 2, {"AB", "A", ""}},
	{"THUE-MORSE", "A>AB B>BA", 2, {"AB", "BA", ""}},
	{"DOUBLING", "A>AB B>AA", 2, {"AB", "AA", ""}},
	{"CANTOR", "A>ABA B>BBB", 2, {"ABA", "BBB", ""}},
	{"TRIBONACCI", "A>AB B>AC C>A", 3, {"AB", "AC", "A"}},
	{"PADOVAN", "A>B B>C C>AB", 3, {"B", "C", "AB"}},
};
static const int SPROUT_BAR_MAX = 48;      // densest string spread over one cycle
static const int SPROUT_STREAM_MAX = 4096;

struct SproutTables {
	std::vector<std::vector<uint8_t>> bar[SPROUT_RULES];  // generations up to SPROUT_BAR_MAX long
	std::vector<uint8_t> stream[SPROUT_RULES];            // one long generation for STREAM mode
	SproutTables() {
		for (int i = 0; i < SPROUT_RULES; i++) {
			std::vector<std::vector<uint8_t>> rules(3);
			for (int s = 0; s < 3; s++)
				for (const char* c = SPROUT_RULESETS[i].r[s]; *c; c++) rules[s].push_back((uint8_t) (*c - 'A'));
			bar[i] = grow(rules, 0, SPROUT_BAR_MAX);
			std::vector<std::vector<uint8_t>> all = grow(rules, 0, SPROUT_STREAM_MAX, 40);
			stream[i] = all.back();
		}
	}
};
inline const SproutTables& sproutTables() { static SproutTables t; return t; }

struct Sprout {
	// settings
	int rule = 0;
	int startGen = 2;
	int grow = 2;          // cycles per generation, 0 = hold
	int length = 16;       // clock pulses per cycle
	bool stream = false;   // read one symbol per clock from the endless word
	bool quant = false;    // BAR mode: snap hits to the clock
	// state
	int gen = 2, step = -1, cycles = 0, streamIdx = -1, lastFired = -1;
	bool fired[3] = {false, false, false};
	bool cycleStart = false;
	int lastSym = -1;       // last symbol fired (for the screen)
	int lastIndex = -1;

	int maxGen() const { return (int) sproutTables().bar[rule].size() - 1; }
	const std::vector<uint8_t>& current() const {
		const SproutTables& t = sproutTables();
		return stream ? t.stream[rule] : t.bar[rule][std::min(gen, maxGen())];
	}
	void reset() {
		step = -1; cycles = 0; streamIdx = -1; lastFired = -1;
		gen = std::min(startGen, maxGen());
	}

	// Call every sample. Sets fired[sym] for hits this sample.
	void process(bool clockEdge, const ClockTracker& clk) {
		fired[0] = fired[1] = fired[2] = false;
		cycleStart = false;
		if (gen > maxGen()) gen = maxGen();
		if (stream) {
			if (clockEdge) {
				const std::vector<uint8_t>& w = sproutTables().stream[rule];
				streamIdx = (streamIdx + 1) % (int) w.size();
				fired[w[streamIdx]] = true;
				lastSym = w[streamIdx];
				lastIndex = streamIdx;
				if (streamIdx % std::max(length, 1) == 0) cycleStart = true;
			}
			return;
		}
		if (clockEdge) {
			step++;
			if (step >= length) {
				step = 0;
				cycles++;
				if (grow > 0 && cycles % grow == 0) {
					gen++;
					if (gen > maxGen()) gen = std::min(startGen, maxGen());
				}
				lastFired = -1;
			}
			if (step == 0) { cycleStart = true; lastFired = -1; }
		}
		if (step < 0) return;
		const std::vector<uint8_t>& w = sproutTables().bar[rule][gen];
		int L = (int) w.size();
		float pos = (step + (quant ? 0.f : clk.frac())) / (float) length;
		// fire every symbol whose start time has been reached
		while (lastFired + 1 < L && (float) (lastFired + 1) / L <= pos + 1e-6f) {
			lastFired++;
			fired[w[lastFired]] = true;
			lastSym = w[lastFired];
			lastIndex = lastFired;
		}
	}
};

// ============================================================ Graft
static const int GRAFT_LANES = 4, GRAFT_STEPS = 16, GRAFT_GROOVES = 4;
static const char* const GRAFT_GROOVE_NAMES[GRAFT_GROOVES] = {"DETROIT", "ELECTRO", "BREAK", "BROKEN"};
static const char* const GRAFT_SEEDS[GRAFT_GROOVES][GRAFT_LANES] = {
	// kick, snare/clap, hat, perc
	{"x...x...x...x...", "....x.......x...", "..x...x...x...x.", "...x..x....x..x."},
	{"x.....x...x.....", "....x.......x...", "x.x.x.x.x.x.x.x.", "......x.......x."},
	{"x.x.......xx....", "....x..x.x..x...", "x.x.x.x.x.x.x.x.", "..........x....."},
	{"x..x..x...x..x..", "....x.......x.x.", ".xx.xx.x.xx.xx.x", "x.....x.....x..."},
};

// One step: divided into div equal parts (1, 2 or 4); mask says which parts hit.
struct Cell {
	uint8_t div = 1, mask = 0;
	bool operator==(const Cell& o) const { return div == o.div && mask == o.mask; }
};

struct Graft {
	// settings
	int groove = 0;
	int every = 1;            // bars per generation, 0 = frozen
	float ratchet = 0.25f, drop = 0.15f, sprout = 0.1f, home = 0.2f;
	// state
	Cell seed[GRAFT_LANES][GRAFT_STEPS];
	Cell cell[GRAFT_LANES][GRAFT_STEPS];
	int step = -1, bars = 0, generation = 0;
	uint8_t firedMask[GRAFT_LANES] = {};
	bool fired[GRAFT_LANES] = {};
	int loadedGroove = -1;
	Rng rng{7};

	void loadSeed() {
		for (int l = 0; l < GRAFT_LANES; l++)
			for (int s = 0; s < GRAFT_STEPS; s++) {
				Cell c;
				c.div = 1;
				c.mask = GRAFT_SEEDS[groove][l][s] == 'x' ? 1 : 0;
				seed[l][s] = c;
				cell[l][s] = c;
			}
		loadedGroove = groove;
		generation = 0;
	}
	void reset() { step = -1; bars = 0; }
	void goHome() { loadSeed(); }

	// Kick hits on the beat in the seed are never dropped: the floor stays.
	bool anchored(int l, int s) const { return l == 0 && s % 4 == 0 && seed[l][s].mask; }

	// One generation: every step is rewritten by a chance rule.
	void mutate() {
		for (int l = 0; l < GRAFT_LANES; l++)
			for (int s = 0; s < GRAFT_STEPS; s++) {
				Cell& c = cell[l][s];
				if (rng.uni() < home) { c = seed[l][s]; continue; }
				float r = rng.uni();
				if (c.mask) {
					if (r < ratchet) {
						if (c.div < 4) {
							// every hit splits into two at half length: x -> xx
							uint8_t m = 0;
							for (int b = 0; b < c.div; b++)
								if (c.mask & (1 << b)) m |= (uint8_t) (3 << (2 * b));
							c.div *= 2;
							c.mask = m;
						}
						else c.mask = (uint8_t) (((c.mask << 1) | (c.mask >> 3)) & 15);  // roll the ratchet round
					}
					else if (r < ratchet + drop && !anchored(l, s)) {
						// lose one hit
						int bits[4], n = 0;
						for (int b = 0; b < c.div; b++) if (c.mask & (1 << b)) bits[n++] = b;
						c.mask &= (uint8_t) ~(1 << bits[rng.below(n)]);
						if (!c.mask) c.div = 1;
					}
				}
				else if (r < sprout) {
					// a rest grows a hit, sometimes on the off half of the step
					if (rng.uni() < 0.35f) { c.div = 2; c.mask = 2; }
					else { c.div = 1; c.mask = 1; }
				}
			}
		generation++;
	}

	void process(bool clockEdge, const ClockTracker& clk) {
		if (loadedGroove != groove) loadSeed();
		for (int l = 0; l < GRAFT_LANES; l++) fired[l] = false;
		if (clockEdge) {
			step++;
			if (step >= GRAFT_STEPS) {
				step = 0;
				bars++;
				if (every > 0 && bars % every == 0) mutate();
			}
			for (int l = 0; l < GRAFT_LANES; l++) firedMask[l] = 0;
		}
		if (step < 0) return;
		float f = clk.frac();
		for (int l = 0; l < GRAFT_LANES; l++) {
			const Cell& c = cell[l][step];
			for (int b = 0; b < c.div; b++) {
				if (!(c.mask & (1 << b)) || (firedMask[l] & (1 << b))) continue;
				if (f + 1e-6f >= (float) b / c.div) {
					firedMask[l] |= (uint8_t) (1 << b);
					fired[l] = true;
				}
			}
		}
	}
};

// ============================================================ Bloom
static const int BLOOM_RULES = 6, BLOOM_MODES = 6;
struct BloomRule { const char* name; const char* text; const char* r[7]; };
// Rules on scale degrees 1-7 (as characters). A degree with no rule stays itself.
static const BloomRule BLOOM_RULESETS[BLOOM_RULES] = {
	{"HOMECOMING", "1>14 4>25 5>51", {"14", "2", "3", "25", "51", "6", "7"}},
	{"FALLING", "1>16 6>45 5>1", {"16", "2", "3", "4", "1", "45", "7"}},
	{"CIRCLE", "1>14 4>73 3>62 2>51", {"14", "51", "62", "73", "5", "6", "7"}},
	{"FIBONACCI", "1>15 5>1", {"15", "2", "3", "4", "1", "6", "7"}},
	{"DRIFT", "1>13 3>64 4>41", {"13", "2", "64", "41", "5", "6", "7"}},
	{"TWO STEP", "1>17 7>61 6>6", {"17", "2", "3", "4", "5", "6", "61"}},
};
static const char* const BLOOM_MODE_NAMES[BLOOM_MODES] = {"MAJOR", "DORIAN", "MINOR", "MIXOLYDIAN", "PHRYGIAN", "LYDIAN"};
static const int BLOOM_SCALES[BLOOM_MODES][7] = {
	{0, 2, 4, 5, 7, 9, 11}, {0, 2, 3, 5, 7, 9, 10}, {0, 2, 3, 5, 7, 8, 10},
	{0, 2, 4, 5, 7, 9, 10}, {0, 1, 3, 5, 7, 8, 10}, {0, 2, 4, 6, 7, 9, 11},
};
static const char* const NOTE_NAMES[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
static const int BLOOM_MAX = 32;   // longest progression before it starts again

struct BloomTables {
	std::vector<std::vector<uint8_t>> gens[BLOOM_RULES];
	BloomTables() {
		for (int i = 0; i < BLOOM_RULES; i++) {
			std::vector<std::vector<uint8_t>> rules(7);
			for (int s = 0; s < 7; s++)
				for (const char* c = BLOOM_RULESETS[i].r[s]; *c; c++) rules[s].push_back((uint8_t) (*c - '1'));
			gens[i] = grow(rules, 0, BLOOM_MAX);
		}
	}
};
inline const BloomTables& bloomTables() { static BloomTables t; return t; }

// Roman numeral for scale degree d (0-6) in a mode: case from the triad.
inline std::string roman(int mode, int d) {
	static const char* const U[7] = {"I", "II", "III", "IV", "V", "VI", "VII"};
	static const char* const Lw[7] = {"i", "ii", "iii", "iv", "v", "vi", "vii"};
	const int* sc = BLOOM_SCALES[mode];
	int r = sc[d], t = sc[(d + 2) % 7] + ((d + 2) >= 7 ? 12 : 0), f = sc[(d + 4) % 7] + ((d + 4) >= 7 ? 12 : 0);
	int third = t - r, fifth = f - r;
	if (third == 4) return U[d];
	if (fifth == 6) return std::string(Lw[d]) + "o";
	return Lw[d];
}

struct Bloom {
	// settings
	int rule = 0, mode = 1, key = 5;   // F Dorian
	int length = 16;                   // clock pulses per chord
	int startGen = 1;
	bool growOn = true;
	bool sevenths = false;
	// state
	int gen = 1, idx = 0, pulse = -1;
	bool changed = true;      // a new chord this sample
	bool gate = false;
	float voices[4] = {0, 0, 0, 0};  // volts, 0 V = C4
	int nVoices = 3;
	float bass = 0.f;
	int degree = 0;
	bool started = false;

	int maxGen() const { return (int) bloomTables().gens[rule].size() - 1; }
	const std::vector<uint8_t>& current() const { return bloomTables().gens[rule][std::min(gen, maxGen())]; }
	void reset() { gen = std::min(startGen, maxGen()); idx = 0; pulse = -1; started = false; }

	void voice() {
		const int* sc = BLOOM_SCALES[mode];
		int n = sevenths ? 4 : 3;
		int pcs[4];
		for (int i = 0; i < n; i++) {
			int d = degree + 2 * i;
			pcs[i] = sc[d % 7] + 12 * (d / 7) + key;
		}
		// Try each inversion at a couple of octaves; keep the one that moves
		// the voices least and stays around middle C.
		float best[4];
		float bestCost = 1e9f;
		for (int inv = 0; inv < n; inv++)
			for (int oct = -2; oct <= 1; oct++) {
				int notes[4];
				for (int i = 0; i < n; i++) notes[i] = pcs[(i + inv) % n] + ((i + inv) >= n ? 12 : 0) + 12 * oct;
				float cost = 0.f, mean = 0.f;
				for (int i = 0; i < n; i++) mean += notes[i];
				mean /= n;
				if (started && nVoices == n)
					for (int i = 0; i < n; i++) cost += std::fabs(notes[i] - voices[i] * 12.f);
				cost += 0.35f * std::fabs(mean - 2.f);   // centre around D4
				if (cost < bestCost) {
					bestCost = cost;
					for (int i = 0; i < n; i++) best[i] = notes[i] / 12.f;
				}
			}
		for (int i = 0; i < n; i++) voices[i] = best[i];
		nVoices = n;
		bass = (sc[degree] + key) / 12.f - 2.f;
		if (bass < -2.5f) bass += 1.f;
	}

	void process(bool clockEdge, const ClockTracker& clk) {
		changed = false;
		if (gen > maxGen()) gen = maxGen();
		if (!started) {
			gen = std::min(gen, maxGen());
			degree = current()[std::min(idx, (int) current().size() - 1)];
			voice();
			started = true;
			changed = true;
		}
		if (clockEdge) {
			pulse++;
			if (pulse >= length) {
				pulse = 0;
				idx++;
				if (idx >= (int) current().size()) {
					idx = 0;
					if (growOn) {
						gen++;
						if (gen > maxGen()) gen = std::min(startGen, maxGen());
					}
				}
			}
			if (pulse == 0) {
				degree = current()[idx];
				voice();
				changed = true;
			}
		}
		// gate opens on each chord, closes for the second half of its last pulse
		gate = pulse >= 0 && !(pulse == length - 1 && clk.frac() > 0.5f);
	}
};

} // namespace lsys
