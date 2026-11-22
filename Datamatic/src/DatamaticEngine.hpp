// Datamatic engine: numbers -> bits -> four interlocking gate lanes.
// Plain C++ (no Rack), so test/render.cpp can run it offline.
//
// The idea (after Ryoji Ikeda's "test pattern" / "datamatics"): a short loop
// of up to 64 bits is taken from a data source (counting, primes, Fibonacci,
// the digits of pi or e, text, seeded random, or a live CV). Four lanes read
// that same loop in different orders and at different speeds, so they always
// interlock:
//   SUB   reads forwards, once per clock (legato: runs of 1s hold the gate)
//   TONE  reads backwards at half RATE and only where SUB is silent (legato)
//   CLICK reads every 3rd bit at full RATE (short triggers)
//   NOISE needs two bits (5th and 7th strides) at full RATE (short triggers)
// RATE subdivides the clock (1x..16x) for Ikeda-style micro-rhythm, ROLL turns
// a step into an accelerating ratchet, CUT drops half or all of a 16-clock bar
// into digital silence and MUTATE flips bits each time the loop comes round.
#pragma once
#include <cstdint>
#include <cmath>
#include <string>
#include <algorithm>

namespace datamatic {

enum Source { SRC_COUNT, SRC_PRIMES, SRC_FIB, SRC_PI, SRC_E, SRC_TEXT, SRC_RANDOM, SRC_INPUT, NUM_SOURCES };
static const char* const SOURCE_NAMES[NUM_SOURCES] = {"COUNT", "PRIMES", "FIB", "PI", "E", "TEXT", "RANDOM", "INPUT"};
static const int RATES[5] = {1, 2, 4, 8, 16};

enum Lane { SUB, TONE, CLICK, NOISE, NUM_LANES };

static const char* const PI_DIGITS =
	"31415926535897932384626433832795028841971693993751058209749445923078164062862089986280348253421170679"
	"82148086513282306647093844609550582231725359408128481117450284102701938521105559644622948954930381964"
	"42881097566593344612847564823378678316527120190914564856692346034861045432664821339360726024914127372"
	"45870066063155881";
static const char* const E_DIGITS =
	"27182818284590452353602874713526624977572470936999595749669676277240766303535475945713821785251664274"
	"27466391932003059921817413596629043572900334295260595630738132328627943490763233829880753195251019011"
	"57383418793070215408914993488416750924476146066808226480016847741185374234544243710753907774499206955"
	"17027618386062613";

struct Settings {
	int source = SRC_PI;
	int word = 0;        // 0..255, where in the data the loop starts
	int length = 16;     // 4..64 bits
	int rateIndex = 1;   // index into RATES
	float mutate = 0.f;  // 0..1
	float roll = 0.f;    // 0..1
	float cut = 0.f;     // 0..1
	float bpm = 130.f;   // internal clock (16ths) when nothing is patched to CLOCK
};

struct Outputs {
	bool gate[NUM_LANES] = {false, false, false, false};
	bool bit = false;
	bool cut = false;
	float data = 0.f;    // 0..10 V, the byte under the playhead
	bool clockTick = false;
};

struct Engine {
	// ---- the loop
	uint8_t bytes[8] = {0, 0, 0, 0, 0, 0, 0, 0};
	uint64_t mask = 0;               // MUTATE flips live here
	std::string text = "datamatics";
	int builtSource = -1, builtWord = -1;
	std::string builtText;

	// ---- timing
	double now = 0.0;
	double lastClock = -1.0;
	double period = 60.0 / 130.0 / 4.0;
	double internalPhase = 0.0;
	int subIndex = 0;      // which subdivision of the current clock has fired
	int rate = 2;
	int64_t clockCount = -1;
	int subPos = -1, tonePos = -1, fullPos = -1;
	int length = 16;

	// ---- gates: high while from <= now < until
	double gFrom[NUM_LANES] = {0, 0, 0, 0}, gUntil[NUM_LANES] = {0, 0, 0, 0};
	double bitUntil = 0.0;
	bool laneBit[NUM_LANES] = {false, false, false, false};
	float dataOut = 0.f;

	// ---- roll
	bool rolling = false;
	double rollStart = 0.0, rollDur = 0.0;
	int rollIdx = 0, rollN = 0;
	bool rollLane[NUM_LANES] = {false, false, false, false};

	// ---- cut
	bool cutHalf = false, cutFull = false, cutActive = false;

	uint32_t rng = 0x9E3779B9u;
	float rand01() {
		rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
		return (rng >> 8) * (1.f / 16777216.f);
	}

	// ---------------------------------------------------------------- data
	static int primeAt(int n) {
		static int table[400];
		static bool init = false;
		if (!init) {
			int c = 0;
			for (int k = 2; c < 400; k++) {
				bool p = true;
				for (int d = 2; d * d <= k; d++) if (k % d == 0) { p = false; break; }
				if (p) table[c++] = k;
			}
			init = true;
		}
		return table[std::max(0, std::min(399, n))];
	}

	void rebuild(int source, int word) {
		switch (source) {
			case SRC_COUNT:
				for (int i = 0; i < 8; i++) bytes[i] = (uint8_t) ((word + i) & 255);
				break;
			case SRC_PRIMES:
				for (int i = 0; i < 8; i++) bytes[i] = (uint8_t) (primeAt(word + i) & 255);
				break;
			case SRC_FIB: {
				int a = 0, b = 1;
				for (int i = 0; i < word; i++) { int t = (a + b) & 255; a = b; b = t; }
				for (int i = 0; i < 8; i++) { bytes[i] = (uint8_t) a; int t = (a + b) & 255; a = b; b = t; }
			} break;
			case SRC_PI:
			case SRC_E: {
				// binary-coded decimal: two digits per byte, starting at digit WORD
				const char* d = source == SRC_PI ? PI_DIGITS : E_DIGITS;
				for (int i = 0; i < 8; i++)
					bytes[i] = (uint8_t) (((d[word + 2 * i] - '0') << 4) | (d[word + 2 * i + 1] - '0'));
			} break;
			case SRC_TEXT: {
				int n = (int) text.size();
				for (int i = 0; i < 8; i++) bytes[i] = n ? (uint8_t) text[(word + i) % n] : 0;
			} break;
			case SRC_RANDOM: {
				uint32_t s = 2166136261u ^ (uint32_t) (word * 16777619u + 1u);
				for (int i = 0; i < 8; i++) {
					s ^= s << 13; s ^= s >> 17; s ^= s << 5;
					bytes[i] = (uint8_t) (s >> 11);
				}
			} break;
			case SRC_INPUT:
				break;   // written live from the DATA input
		}
		mask = 0;
		builtSource = source;
		builtWord = word;
		builtText = text;
	}

	bool rawBit(int i) const {
		i &= 63;
		bool b = (bytes[i >> 3] >> (7 - (i & 7))) & 1;
		return b ^ (bool) ((mask >> i) & 1);
	}
	bool loopBit(int i) const {
		int L = std::max(1, length);
		return rawBit(((i % L) + L) % L);
	}
	// What each lane reads at its own position j (same rule the display uses)
	bool subBit(int j) const { return loopBit(j); }
	bool toneBit(int j) const { return loopBit(length - 1 - j); }
	bool clickBit(int j) const { return loopBit(j * 3 + 1); }
	bool noiseBit(int j) const { return loopBit(j * 5 + 2) && loopBit(j * 7 + 3); }

	// ---------------------------------------------------------------- gates
	bool gateHigh(int v) const { return now >= gFrom[v] && now < gUntil[v]; }
	void trigger(int v, double len) {
		// a retrigger while high leaves a short gap so the next module sees a new edge
		gFrom[v] = gateHigh(v) ? now + 0.0007 : now;
		gUntil[v] = gFrom[v] + len;
	}
	void hold(int v, double len) {
		if (gateHigh(v)) gUntil[v] = now + len;   // legato: run of 1s keeps it high
		else { gFrom[v] = now; gUntil[v] = now + len; }
	}
	void release(int v) { gUntil[v] = std::min(gUntil[v], now); }

	void reset() {
		subPos = tonePos = fullPos = -1;
		clockCount = -1;
		subIndex = 0;
		rolling = false;
		cutHalf = cutFull = cutActive = false;
		internalPhase = 0.0;
	}

	// One subdivision of the clock: s = 0..rate-1
	void step(int s, const Settings& st) {
		double dur = period / rate;
		int L = length;

		// full-rate lanes
		fullPos = (fullPos + 1) % L;
		if (fullPos == 0) {
			if (st.mutate > 0.f && rand01() < st.mutate) mask ^= (uint64_t) 1 << (int) (rand01() * L);
			if (st.mutate > 0.5f && rand01() < (st.mutate - 0.5f) * 2.f) mask ^= (uint64_t) 1 << (int) (rand01() * L);
		}
		if (st.source == SRC_INPUT && fullPos % 8 == 0) {
			// INPUT: the loop is rewritten byte by byte from the DATA jack
			bytes[(fullPos / 8) & 7] = pendingInputByte;
		}
		laneBit[CLICK] = clickBit(fullPos);
		laneBit[NOISE] = noiseBit(fullPos);
		bool b = loopBit(fullPos);
		if (b) bitUntil = now + dur * 0.5;
		int byte = 0;
		for (int k = 0; k < 8; k++) byte = (byte << 1) | (int) loopBit(fullPos + k);
		dataOut = byte * (10.f / 255.f);

		double trig = std::min(dur * 0.5, 0.01);
		if (laneBit[CLICK]) trigger(CLICK, trig);
		if (laneBit[NOISE]) trigger(NOISE, trig);

		// SUB: once per clock
		if (s == 0) {
			subPos = (subPos + 1) % L;
			laneBit[SUB] = subBit(subPos);
			if (laneBit[SUB]) hold(SUB, period + 0.004);
			else release(SUB);
		}
		// TONE: half RATE, fills the gaps SUB leaves
		int tdiv = std::max(1, rate / 2);
		if (s % tdiv == 0) {
			tonePos = (tonePos + 1) % L;
			laneBit[TONE] = toneBit(tonePos) && !laneBit[SUB];
			if (laneBit[TONE]) hold(TONE, dur * tdiv + 0.004);
			else release(TONE);
		}

		// ROLL: this step becomes an accelerating ratchet
		bool any = laneBit[CLICK] || laneBit[NOISE] || (s == 0 && laneBit[SUB]);
		if (!rolling && any && st.roll > 0.f && rand01() < st.roll * 0.35f) {
			rolling = true;
			rollStart = now;
			rollDur = (s == 0 && laneBit[SUB] && rate == 1) ? period : dur;
			if (rate > 1 && rand01() < st.roll) rollDur = period;   // sometimes the whole clock
			rollN = 4 + (int) (rand01() * (2.f + st.roll * 12.f));
			rollIdx = 1;
			for (int v = 0; v < NUM_LANES; v++) rollLane[v] = laneBit[v];
			if (!rollLane[CLICK] && !rollLane[NOISE] && !rollLane[SUB]) rollLane[CLICK] = true;
		}
	}

	uint8_t pendingInputByte = 0;

	// ---------------------------------------------------------------- per sample
	Outputs process(double dt, bool clockEdge, bool resetEdge, bool external, float dataIn, const Settings& st) {
		Outputs o;
		now += dt;
		if (st.source != builtSource || st.word != builtWord || (st.source == SRC_TEXT && text != builtText))
			rebuild(st.source, st.word);
		length = std::max(4, std::min(64, st.length));
		rate = RATES[std::max(0, std::min(4, st.rateIndex))];
		pendingInputByte = (uint8_t) std::max(0.f, std::min(255.f, dataIn / 10.f * 255.f + 0.5f));

		if (resetEdge) reset();

		bool tick = false;
		if (external) {
			if (clockEdge) {
				if (lastClock >= 0.0) period = std::max(0.001, std::min(4.0, now - lastClock));
				lastClock = now;
				tick = true;
			}
		}
		else {
			period = 60.0 / std::max(20.f, st.bpm) / 4.0;
			internalPhase += dt / period;
			if (internalPhase >= 1.0 || clockCount < 0) {
				internalPhase -= std::floor(internalPhase);
				lastClock = now;
				tick = true;
			}
		}

		if (tick) {
			o.clockTick = true;
			clockCount++;
			int inBar = (int) (clockCount % 16);
			if (inBar == 0) {
				cutHalf = st.cut > 0.f && rand01() < st.cut;
				cutFull = cutHalf && rand01() < st.cut * 0.4f;
			}
			cutActive = cutFull || (cutHalf && inBar >= 8);
			subIndex = 0;
			step(0, st);
		}
		// later subdivisions of this clock
		while (lastClock >= 0.0 && subIndex + 1 < rate && now >= lastClock + (subIndex + 1) * period / rate) {
			subIndex++;
			step(subIndex, st);
		}

		// ratchet
		if (rolling) {
			while (rollIdx < rollN) {
				double f = (double) rollIdx / rollN;
				double t = rollStart + rollDur * (1.0 - (1.0 - f) * (1.0 - f));
				if (now < t) break;
				double gap = rollDur * (2.0 * (1.0 - f) / rollN);
				for (int v = 0; v < NUM_LANES; v++)
					if (rollLane[v]) trigger(v, std::max(0.0008, gap * 0.45));
				rollIdx++;
			}
			if (rollIdx >= rollN) rolling = false;
		}

		for (int v = 0; v < NUM_LANES; v++) o.gate[v] = !cutActive && gateHigh(v);
		o.bit = !cutActive && now < bitUntil;
		o.cut = cutActive;
		o.data = dataOut;
		return o;
	}
};

} // namespace datamatic
