// Keel engine: everything except Rack. Kept Rack-free so test/render.cpp can drive it.
//
// Three jobs in one:
//   1. KEY FOLLOW  - pick the note the kick should sit on (key root, or the root of the
//                    chord coming in), placed in the octave nearest the kick's own pitch,
//                    and turn it into a V/OCT offset for any kick with a V/OCT input.
//   2. PATTERN     - up to 16 steps of intervals (root, 5th, 4th, octave, 3rd, 7th) that
//                    advance on every kick trigger; MUTATE re-rolls unlocked steps at the
//                    end of each loop, LOCK freezes them.
//   3. METER + UNDERTONE - listen to the kick's audio, measure the body pitch after the
//                    click (Hz, note, cents off target), optionally correct the V/OCT
//                    automatically (AUTO) or learn the kick's natural pitch (LEARN), and
//                    replace the kick's low end with a sub tone tuned to the target.
#pragma once
#include <cmath>
#include <cstdint>
#include <cstring>
#include <algorithm>

namespace keel {

static const float PI = 3.14159265358979f;
static const int MAXSTEPS = 16;

// ---------------------------------------------------------------- scales
enum Scale { MINOR, MAJOR, DORIAN, PHRYGIAN, MIXOLYDIAN, MINOR_PENT, CHROMATIC, NSCALES };
inline const char* scaleName(int s) {
	static const char* n[NSCALES] = {"MINOR", "MAJOR", "DORIAN", "PHRYGIAN", "MIXOLYDIAN", "MIN PENT", "CHROMATIC"};
	return n[s < 0 ? 0 : s >= NSCALES ? NSCALES - 1 : s];
}
// bit i set = semitone i above the root is in the scale
inline int scaleMask(int s) {
	static const int m[NSCALES] = {
		(1 << 0) | (1 << 2) | (1 << 3) | (1 << 5) | (1 << 7) | (1 << 8) | (1 << 10),   // natural minor
		(1 << 0) | (1 << 2) | (1 << 4) | (1 << 5) | (1 << 7) | (1 << 9) | (1 << 11),   // major
		(1 << 0) | (1 << 2) | (1 << 3) | (1 << 5) | (1 << 7) | (1 << 9) | (1 << 10),   // dorian
		(1 << 0) | (1 << 1) | (1 << 3) | (1 << 5) | (1 << 7) | (1 << 8) | (1 << 10),   // phrygian
		(1 << 0) | (1 << 2) | (1 << 4) | (1 << 5) | (1 << 7) | (1 << 9) | (1 << 10),   // mixolydian
		(1 << 0) | (1 << 3) | (1 << 5) | (1 << 7) | (1 << 10),                         // minor pentatonic
		0xFFF};
	return m[s < 0 ? 0 : s >= NSCALES ? NSCALES - 1 : s];
}
inline int pcMod(int n) { return ((n % 12) + 12) % 12; }
inline const char* pcName(int pc) {
	static const char* n[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
	return n[pcMod(pc)];
}
// nearest note to a frequency: midi number and cents off it
inline void noteOf(float hz, int& midi, float& cents) {
	float m = 69.f + 12.f * std::log2(std::max(hz, 1e-3f) / 440.f);
	midi = (int) std::floor(m + 0.5f);
	cents = 100.f * (m - midi);
}
inline float midiHz(float m) { return 440.f * std::exp2((m - 69.f) / 12.f); }
// pitch class of a V/OCT voltage (0 V = C4)
inline int pcOfVolts(float v) { return pcMod((int) std::floor(v * 12.f + 0.5f)); }

// ---------------------------------------------------------------- pattern intervals
enum Iv { IV_ROOT, IV_FIFTH, IV_FOURTH, IV_OCTAVE, IV_THIRD, IV_SEVENTH, NIV };
inline const char* ivLabel(int i) {
	static const char* n[NIV] = {"1", "5", "4", "8", "3", "7"};
	return n[i < 0 || i >= NIV ? 0 : i];
}
// semitones above the anchor, picked to stay in the key where the interval has two
// flavours (3rd: minor or major, 7th: flat or major). Octave adds 12 after placement.
inline int ivSemis(int iv, int anchorPc, int keyRoot, int scale) {
	int mask = scaleMask(scale);
	auto inKey = [&](int semis) { return (mask >> pcMod(anchorPc + semis - keyRoot)) & 1; };
	switch (iv) {
		case IV_FIFTH: return inKey(7) || !inKey(6) ? 7 : 6;
		case IV_FOURTH: return inKey(5) || !inKey(6) ? 5 : 6;
		case IV_THIRD: return inKey(3) ? 3 : inKey(4) ? 4 : 3;
		case IV_SEVENTH: return inKey(10) ? 10 : inKey(11) ? 11 : 10;
		default: return 0;
	}
}

// small, seedable RNG (xorshift64*)
struct Rng {
	uint64_t s = 0x9E3779B97F4A7C15ull;
	void seed(uint64_t v) { s = v ? v : 0x9E3779B97F4A7C15ull; }
	uint64_t next() { s ^= s >> 12; s ^= s << 25; s ^= s >> 27; return s * 2685821657736338717ull; }
	float uni() { return (float) ((next() >> 40) * (1.0 / 16777216.0)); }
};

struct Pattern {
	int iv[MAXSTEPS];
	int pos = 0;      // step that is playing now
	Pattern() { clear(); }
	void clear() { for (int i = 0; i < MAXSTEPS; i++) iv[i] = IV_ROOT; pos = 0; }
	// one random interval; SPREAD 0 = mostly root with a few fifths, 1 = all six
	static int roll(Rng& r, float spread) {
		float w[NIV] = {1.f, 0.25f + 0.75f * spread, 0.7f * spread, 0.6f * spread, 0.5f * spread, 0.4f * spread};
		float sum = 0.f;
		for (int i = 0; i < NIV; i++) sum += w[i];
		float u = r.uni() * sum;
		for (int i = 0; i < NIV; i++) {
			if (u < w[i]) return i;
			u -= w[i];
		}
		return IV_ROOT;
	}
	// re-roll each step with chance `amount` (steps beyond `len` too, so a longer
	// pattern later is not all roots); returns true if anything changed
	bool mutate(Rng& r, float amount, float spread) {
		bool changed = false;
		for (int i = 0; i < MAXSTEPS; i++) {
			if (r.uni() < amount) {
				int v = roll(r, spread);
				if (v != iv[i]) { iv[i] = v; changed = true; }
			}
		}
		return changed;
	}
};

// ---------------------------------------------------------------- filters
struct Biquad {
	float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
	void lowpass(float fc, float fs, float q = 0.70710678f) {
		float w = 2.f * PI * std::min(fc, 0.45f * fs) / fs, c = std::cos(w), al = std::sin(w) / (2.f * q);
		float a0 = 1.f + al;
		b0 = (1.f - c) / 2.f / a0; b1 = (1.f - c) / a0; b2 = b0; a1 = -2.f * c / a0; a2 = (1.f - al) / a0;
	}
	void highpass(float fc, float fs, float q = 0.70710678f) {
		float w = 2.f * PI * std::min(fc, 0.45f * fs) / fs, c = std::cos(w), al = std::sin(w) / (2.f * q);
		float a0 = 1.f + al;
		b0 = (1.f + c) / 2.f / a0; b1 = -(1.f + c) / a0; b2 = b0; a1 = -2.f * c / a0; a2 = (1.f - al) / a0;
	}
	float process(float x) {
		float y = b0 * x + z1;
		z1 = b1 * x - a1 * y + z2;
		z2 = b2 * x - a2 * y;
		return y;
	}
	void reset() { z1 = z2 = 0.f; }
};

// ---------------------------------------------------------------- meter
// Measures the body pitch of each hit: low-pass at 200 Hz, then the periods between
// upward zero crossings from 15 ms after the onset until the body fades (3% of its
// peak) or 300 ms pass. The pitch is the median period of the later half of the hit,
// so a slow 909 sweep (TUNE up) or a 707's glide has settled before it counts.
struct Meter {
	static const int MAXP = 96;
	float fs = 48000.f;
	Biquad lp1, lp2;
	float skipS = 0.015f, windowS = 0.3f;
	bool measuring = false;
	double t = 0.0;
	float prev = 0.f, peak = 0.f;
	double lastCross = -1.0;
	float periods[MAXP];
	int nPeriods = 0;
	// result of the last finished hit
	bool have = false;
	float hz = 0.f;
	int hits = 0;     // count of finished measurements (screen/tests watch it change)

	void setSampleRate(float f) {
		fs = f;
		lp1.lowpass(200.f, fs);
		lp2.lowpass(200.f, fs);
	}
	void onset() {
		finish();
		measuring = true;
		t = 0.0;
		peak = 0.f;
		lastCross = -1.0;
		nPeriods = 0;
	}
	void finish() {
		if (!measuring) return;
		measuring = false;
		if (nPeriods < 3) return;
		int from = nPeriods >= 6 ? nPeriods / 2 : 0;
		float tmp[MAXP];
		int n = 0;
		for (int i = from; i < nPeriods; i++) tmp[n++] = periods[i];
		std::sort(tmp, tmp + n);
		float med = n % 2 ? tmp[n / 2] : 0.5f * (tmp[n / 2 - 1] + tmp[n / 2]);
		hz = 1.f / med;
		have = true;
		hits++;
	}
	// returns true on the sample a measurement completes
	bool process(float x) {
		float y = lp2.process(lp1.process(x));
		if (!measuring) { prev = y; return false; }
		double dt = 1.0 / fs;
		t += dt;
		if (t >= skipS) {
			peak = std::max(peak, std::fabs(y));
			if (prev < 0.f && y >= 0.f) {
				double frac = prev / (double) (prev - y);
				double tc = t - dt + frac * dt;
				if (lastCross >= 0.0) {
					double p = tc - lastCross;
					if (p > 1.0 / 400.0 && p < 1.0 / 15.0 && nPeriods < MAXP) periods[nPeriods++] = (float) p;
				}
				lastCross = tc;
			}
			bool faded = peak > 0.f && std::fabs(y) < 0.03f * peak && nPeriods >= 3 && t > skipS + 0.03 &&
			             t - lastCross > 1.0 / 15.0;
			if (t >= skipS + windowS || faded) {
				prev = y;
				int before = hits;
				finish();
				return hits != before;
			}
		}
		prev = y;
		return false;
	}
};

// onset detector for audio with no TRIG patched: fast envelope jumps above the slow one
struct Onset {
	float fs = 48000.f, fast = 0.f, slow = 0.f, aF = 0.f, rF = 0.f, aS = 0.f;
	double since = 1.0;
	void setSampleRate(float f) {
		fs = f;
		aF = 1.f - std::exp(-1.f / (0.0005f * fs));
		rF = 1.f - std::exp(-1.f / (0.03f * fs));
		aS = 1.f - std::exp(-1.f / (0.08f * fs));
	}
	bool process(float x) {
		float r = std::fabs(x);
		fast += (r > fast ? aF : rF) * (r - fast);
		slow += aS * (r - slow);
		since += 1.0 / fs;
		if (since > 0.08 && fast > 0.4f && fast > 2.5f * slow + 0.2f) { since = 0.0; return true; }
		return false;
	}
};

// ---------------------------------------------------------------- undertone (sub layer)
// Linkwitz-Riley split at XOVER; the low band's envelope drives a sine (DRIVE adds
// soft harmonics) at the target pitch, starting DROP semitones sharp and falling in
// about 20 ms, like an 808's sigh. MIX = highs + (1-REPLACE) lows + REPLACE sub.
struct Undertone {
	float fs = 48000.f, xover = 120.f;
	Biquad lpA, lpB, hpA, hpB;
	float env = 0.f, rel = 0.f;
	double phase = 0.0;
	float dropEnv = 0.f, dropK = 0.f;
	float sub = 0.f, mix = 0.f;
	void setSampleRate(float f) {
		fs = f;
		rel = std::exp(-1.f / (0.02f * fs));
		dropK = std::exp(-1.f / (0.018f * fs));
		setXover(xover, true);
	}
	void setXover(float hz, bool force = false) {
		if (!force && std::fabs(hz - xover) < 0.5f) return;
		xover = hz;
		// Butterworth squared = LR4 (low + high sums flat in magnitude)
		lpA.lowpass(hz, fs); lpB.lowpass(hz, fs);
		hpA.highpass(hz, fs); hpB.highpass(hz, fs);
	}
	void onset() { phase = 0.0; dropEnv = 1.f; }
	void process(float x, float hz, float replace, float drop, float drive, float level) {
		float lo = lpB.process(lpA.process(x));
		float hi = hpB.process(hpA.process(x));
		float r = std::fabs(lo);
		env = r > env ? r : env * rel + r * (1.f - rel);
		float f = hz * std::exp2(drop * dropEnv / 12.f);
		dropEnv *= dropK;
		phase += f / fs;
		if (phase >= 1.0) phase -= std::floor(phase);
		float s = std::sin(2.f * PI * (float) phase);
		if (drive > 0.001f) {
			float g = 1.f + 6.f * drive;
			s = std::tanh(g * s) / std::tanh(g);
		}
		sub = s * env * level;
		// LR4: low + high sum to an all-pass, so REPLACE 0 leaves the kick as it was
		mix = hi + (1.f - replace) * lo + replace * sub;
	}
};

// ---------------------------------------------------------------- the whole module
struct Settings {
	int root = 0;          // 0 = C
	int scale = MINOR;
	bool followChord = false;
	int chordPc = -1;      // pitch class of the chord's lowest note, -1 = none
	int octave = 0;        // octaves added after placing near the kick
	float fineCents = 0.f;
	float kickHz = 49.5f;  // the kick's own pitch at 0 V (808 and 909 Circuits: 49.5 Hz)
	int steps = 1;
	bool autoTune = false;
};

struct Engine {
	Rng rng;
	Pattern pat;
	Meter meter;
	Onset onsetDet;
	Undertone under;
	float fs = 48000.f;

	// note held for the current hit (changes only on a trigger, so a ringing kick
	// is never bent by a key or chord change)
	float targetHz = 65.41f;
	int targetMidi = 36;
	float voctNow = 0.f;      // the V/OCT the kick is being sent right now (after glide)
	float voctHit = 0.f;      // the V/OCT that was set at the last trigger
	float corr = 0.f;         // AUTO correction, octaves
	float lastErrCents = 0.f; // heard pitch vs target, for the last measured hit
	bool lastValid = false;

	Engine() { setSampleRate(48000.f); }
	void setSampleRate(float f) {
		fs = f;
		meter.setSampleRate(f);
		onsetDet.setSampleRate(f);
		under.setSampleRate(f);
	}

	// where a step's note sits: the octave of anchor+interval nearest the kick's own
	// pitch (so the kick moves at most a tritone), then OCTAVE, then the step's octave
	float noteFor(const Settings& s, int iv) const {
		int anchor = (s.followChord && s.chordPc >= 0) ? s.chordPc : s.root;
		int pc = pcMod(anchor + ivSemis(iv, anchor, s.root, s.scale));
		float kickMidi = 69.f + 12.f * std::log2(std::max(s.kickHz, 1.f) / 440.f);
		// candidate midi numbers with this pitch class; pick the nearest to the kick
		float best = 0.f, bestD = 1e9f;
		for (int m = pc; m < 128; m += 12) {
			float d = std::fabs(m - kickMidi);
			if (d < bestD || (d == bestD && m < best)) { bestD = d; best = (float) m; }
		}
		best += 12.f * s.octave;
		if (iv == IV_OCTAVE) best += 12.f;
		return best;
	}

	// a kick trigger: advance the pattern, take the new note
	// returns true when a loop ended (where MUTATE acts)
	bool trigger(const Settings& s, bool first) {
		int len = std::max(1, std::min(MAXSTEPS, s.steps));
		bool wrapped = false;
		if (!first) {
			pat.pos++;
			if (pat.pos >= len) { pat.pos = 0; wrapped = true; }
		}
		else pat.pos = std::min(pat.pos, len - 1);
		retarget(s);
		return wrapped;
	}
	void retarget(const Settings& s) {
		float m = noteFor(s, pat.iv[pat.pos]);
		targetMidi = (int) m;
		targetHz = midiHz(m + s.fineCents / 100.f);
	}
	float voctTarget(const Settings& s) const {
		return std::log2(targetHz / std::max(s.kickHz, 1.f)) + (s.autoTune ? corr : 0.f);
	}
	// V/OCT for an absolute pitch output (0 V = C4)
	float pitchVolts() const { return std::log2(targetHz / 261.6255653f); }

	// after the meter finished a hit: work out the error, apply AUTO correction
	void measured(const Settings& s) {
		float heard = meter.hz;
		lastErrCents = 1200.f * std::log2(heard / targetHz);
		lastValid = std::fabs(lastErrCents) < 2400.f;
		if (!lastValid) return;
		if (s.autoTune) {
			corr -= 0.8f * lastErrCents / 1200.f;
			corr = std::max(-2.f, std::min(2.f, corr));
		}
	}
	// LEARN: the kick's own pitch at 0 V, from the last measurement and the V/OCT it was sent
	float learnedKickHz() const { return meter.hz / std::exp2(voctHit); }
};

}  // namespace keel
