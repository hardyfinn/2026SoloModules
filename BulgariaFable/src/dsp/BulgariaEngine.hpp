// Bulgaria harmoniser engine. No Rack includes, so the offline tests can use it.
// Spec: docs/BUILD-INSTRUCTIONS.md (section numbers in the comments refer to it).
// Pitch inside the engine is in cents relative to C4 (0 V).
#pragma once
#include <cmath>
#include <cstdint>
#include <algorithm>

namespace bulgaria {

static const int NV = 8;
static const int NUM_MODES = 7;
static const char* const MODE_NAMES[NUM_MODES] = {"Shope", "Aeolian", "Dorian", "Phrygian", "Hijaz", "Mixolydian", "Major"};
static const char* const NOTE_NAMES[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};

// Section 3: tempered and village tunings, cents from the root.
static const double TEMP[NUM_MODES][7] = {
	{0, 200, 300, 500, 700, 900, 1000},
	{0, 200, 300, 500, 700, 800, 1000},
	{0, 200, 300, 500, 700, 900, 1000},
	{0, 100, 300, 500, 700, 800, 1000},
	{0, 100, 400, 500, 700, 800, 1000},
	{0, 200, 400, 500, 700, 900, 1000},
	{0, 200, 400, 500, 700, 900, 1100},
};
static const double VILL[NUM_MODES][7] = {
	{0, 170, 310, 500, 700, 870, 1010},
	{0, 190, 305, 500, 700, 810, 1000},
	{0, 195, 300, 500, 700, 890, 1000},
	{0, 125, 305, 500, 700, 815, 1000},
	{0, 130, 385, 500, 700, 810, 1000},
	{0, 195, 385, 500, 700, 885, 1000},
	{0, 200, 390, 500, 700, 890, 1090},
};

// RUB knob maps (section 5).
inline double rubCents(double x) { return 25.0 + 225.0 * x; }
inline double rubHz(double x) { return 1.0 + 79.0 * std::pow(std::max(0.0, x), 2.2); }
inline double rubCentsInv(double c) { return (c - 25.0) / 225.0; }
inline double rubHzInv(double hz) { return std::pow((hz - 1.0) / 79.0, 1.0 / 2.2); }

inline int floorDiv(int a, int b) { int q = a / b; if ((a % b != 0) && ((a < 0) != (b < 0))) q--; return q; }
inline int floorMod(int a, int b) { return a - b * floorDiv(a, b); }
inline double clamp01(double x) { return x < 0 ? 0 : (x > 1 ? 1 : x); }
inline double centsToHz(double c) { return 261.6256 * std::exp2(c / 1200.0); }

struct Rng {
	uint64_t s = 0x9E3779B97F4A7C15ull;
	void seed(uint64_t v) { s = v ? v : 0x9E3779B97F4A7C15ull; for (int i = 0; i < 4; i++) next(); }
	uint64_t next() { s ^= s >> 12; s ^= s << 25; s ^= s >> 27; return s * 2685821657736338717ull; }
	double uni() { return (next() >> 11) * (1.0 / 9007199254740992.0); }   // 0..1
	double bi() { return 2.0 * uni() - 1.0; }                               // -1..1
};

// Slow noise (section 7): white through two one-poles at 0.5 Hz, unit RMS. Stepped at control rate.
struct SlowNoise {
	double a = 0, b = 0, y1 = 0, y2 = 0, norm = 1;
	void init(double controlRate) {
		a = std::exp(-2.0 * M_PI * 0.5 / controlRate);
		b = 1.0 - a;
		// variance of a cascade of two identical one-poles driven by unit-variance white noise
		double var = b * b * b * b * (1 + a * a) / std::pow(1 - a * a, 3);
		norm = 1.0 / std::sqrt(var);
		y1 = y2 = 0;
	}
	void step(Rng& r) {
		double w = r.bi() * 1.7320508;   // uniform with unit variance
		y1 = a * y1 + b * w;
		y2 = a * y2 + b * y1;
	}
	double value() const { return y2 * norm; }
	void settle(Rng& r, int n) { for (int i = 0; i < n; i++) step(r); }
};

struct Params {
	int root = 0;        // 0..11
	int oct = 0;         // -2..2
	int mode = 0;        // 0..6
	double tune = 1.0;   // 0..1 (after CV)
	int voices = 3;      // 1..8 (after CV)
	double spread = 0.0;
	double rub = 0.644;  // knob position 0..1 (after CV)
	bool hz = false;     // RUB means Hz
	bool chanceMode = false;
	double chance = 0.5;
	bool resolve = false;   // CADENCE: false TIGHT, true RESOLVE
	double slide = 0.3, shake = 0.3, whoop = 0.5;
	double loose = 0.3;
};

struct Inputs {
	double voct = 0;
	double gate = 0;
	bool gatePatched = false;
	double drone = 0;
	bool dronePatched = false;
	double phrase = 0;
	double slideTrig = 0, shakeTrig = 0, whoopTrig = 0;
};

enum Kind { K_LEAD, K_DEG, K_RUB, K_THIRD };

struct NoteInfo {
	int L = 0;          // lead degree
	int r = 0;          // played semitone within the octave (0..11)
	double snapped = 0; // tempered value it snapped to (0..1200)
	double spread = 0;  // latched at note start
	bool rub2 = true;   // voice 2 rubs (false = plain third, CHANCE mode)
	int rootSemi = 0;   // root in semitones from C4
};

struct Schmitt {
	bool high = false;
	// returns +1 on rise, -1 on fall, 0 otherwise
	int process(double v, double lo = 0.1, double hi = 1.0) {
		if (!high && v >= hi) { high = true; return 1; }
		if (high && v <= lo) { high = false; return -1; }
		return 0;
	}
};

struct VoiceState {
	NoteInfo info;          // adopted note
	NoteInfo pendingInfo;
	bool pending = false;
	double tv = 1e9;        // seconds since this voice's own note start
	bool gate = false;
	bool muted = false;     // after a triggered whoop while the input gate is still high
	double y = 0;           // smoothed pitch (cents, before ornaments and drift)
	bool yInit = false;
	double lowTime = 1e9;   // seconds the gate has been low
	double push = 0;        // smoothed push (V)
	// slide
	double slideAmp = 0, slideTau = 0.07, slideT = -1;
	bool slidePending = false;
	double slidePendAmp = 0;
	// whoop
	bool whooping = false;
	double whoopHeight = 1000;
	// noises
	SlowNoise drift, wander, pushWander;
	// outputs
	double outCents = 0;
	double outPush = 0;
	bool rubbing = false;
};

struct Engine {
	double sr = 48000, dt = 1.0 / 48000;
	Rng rng;
	uint64_t seedValue = 1;
	VoiceState v[NV];
	Schmitt gateT, phraseT, slideTrigT, shakeTrigT, whoopTrigT;

	// note state
	bool noteOn = false;        // input gate high (or unpatched) and a note has started
	bool started = false;       // a note has ever started
	int curA = 0;               // current absolute input semitone (from C4)
	bool haveA = false;
	double noteTime = 0;        // seconds since the note start (lead)
	double sinceRise = 1e9;     // seconds since the last gate rise
	NoteInfo cur;
	bool cadence = false;
	double tc = -1;             // cadence clock
	bool armNext = false;
	double phrasePend = -1;     // seconds until a PHRASE edge is acted on
	int droneSemi = 0;
	bool haveDrone = false;
	// shake
	double shakeT0 = -1, shakeDepth = 190;
	double nextShakeTry = 0.30;
	// whoop
	bool whoopOn = false;
	double whoopT = 0;
	bool whoopTriggered = false;
	// control-rate noise
	int noiseDiv = 16, noiseCount = 0;
	Params p;

	void init(double sampleRate, uint64_t seed) {
		sr = sampleRate; dt = 1.0 / sr;
		seedValue = seed;
		rng.seed(seed);
		double cr = sr / noiseDiv;
		for (int i = 0; i < NV; i++) {
			VoiceState& s = v[i];
			s = VoiceState();
			s.drift.init(cr); s.wander.init(cr); s.pushWander.init(cr);
			int settle = (int)(cr * 4.0);
			s.drift.settle(rng, settle); s.wander.settle(rng, settle); s.pushWander.settle(rng, settle);
		}
		gateT = phraseT = slideTrigT = shakeTrigT = whoopTrigT = Schmitt();
		noteOn = started = haveA = false;
		noteTime = 0; sinceRise = 1e9; cadence = false; tc = -1; armNext = false; phrasePend = -1;
		haveDrone = false; shakeT0 = -1; whoopOn = false; whoopTriggered = false;
	}

	// Section 3
	double degCents(int L, const Params& q) const {
		int o = floorDiv(L, 7), d = floorMod(L, 7);
		return 1200.0 * o + TEMP[q.mode][d] + q.tune * (VILL[q.mode][d] - TEMP[q.mode][d]);
	}
	int rootSemiNow(const Params& q, const Inputs& in) {
		if (in.dronePatched) {
			double s = in.drone * 12.0;
			if (!haveDrone || std::fabs(s - droneSemi) > 0.7) { droneSemi = (int)std::lround(s); haveDrone = true; }
			return std::max(-48, std::min(48, droneSemi));
		}
		haveDrone = false;
		return q.root + 12 * q.oct;
	}
	NoteInfo snap(int A, int rootSemi, const Params& q) const {
		NoteInfo n;
		n.rootSemi = rootSemi;
		int s = A - rootSemi;
		int o = floorDiv(s, 12), r = floorMod(s, 12);
		int bestD = 0; double best = 1e9, bestV = 0;
		for (int d = 0; d <= 7; d++) {
			double c = d < 7 ? TEMP[q.mode][d] : 1200.0;
			double e = std::fabs(c - 100.0 * r);
			if (e < best - 1e-9) { best = e; bestD = d; bestV = c; }
		}
		n.r = r; n.snapped = bestV;
		n.L = bestD < 7 ? 7 * o + bestD : 7 * (o + 1);
		return n;
	}
	double leadCents(const NoteInfo& n, const Params& q) const {
		return 100.0 * n.rootSemi + degCents(n.L, q) + (1.0 - q.tune) * (100.0 * n.r - n.snapped);
	}

	// Section 5: what each voice does for a given note.
	struct Plan { Kind kind; int deg; int k; int sgn; };
	Plan plan(int i, const NoteInfo& n) const {
		int nearS = n.L >= 0 ? -1 : 1, farS = -nearS;
		switch (i) {
			case 0: return {K_LEAD, 0, 0, 0};
			case 1: if (n.rub2) return {K_RUB, 0, 1, nearS}; return {K_THIRD, n.L + 2 * nearS, 0, 0};
			case 2: return {K_DEG, (n.L == 0 && !cadence) ? -1 : 0, 0, 0};
			case 3: if (n.spread > 0.75) return {K_RUB, 0, 3, farS}; return {K_DEG, -7, 0, 0};
			case 4: return {K_RUB, 0, 1, farS};
			case 5: if (n.spread > 0.5) return {K_RUB, 0, 3, nearS}; return {K_DEG, 4, 0, 0};
			case 6: return {K_RUB, 0, 2, nearS};
			default: if (n.spread > 0.25) return {K_RUB, 0, 2, farS}; return {K_DEG, 7, 0, 0};
		}
	}

	double lean(const VoiceState& s) const { return s.tv >= 0 ? 1.0 - std::exp(-s.tv / 0.12) : 0.0; }

	void noteStart(int A, const Params& q, const Inputs& in) {
		int rs = rootSemiNow(q, in);
		cur = snap(A, rs, q);
		cur.spread = q.spread;
		cur.rub2 = q.chanceMode ? (rng.uni() < q.chance) : true;
		started = true;
		noteTime = 0;
		cadence = armNext; armNext = false;
		tc = cadence ? 0 : -1;
		// whoop is cancelled by a new note
		if (whoopOn) endWhoop();
		for (int i = 0; i < NV; i++) v[i].muted = false;
		// shake clock
		shakeT0 = -1; nextShakeTry = 0.30;
		// slide by rule
		bool sl = rng.uni() < q.slide;
		for (int i = 0; i < NV; i++) {
			VoiceState& s = v[i];
			double d = i == 0 ? 0.0 : 0.035 * q.loose * rng.uni();
			s.pendingInfo = cur;
			s.pending = true;
			s.tv = -d;
			s.slidePending = sl;
			s.slidePendAmp = (i == 0 ? -160.0 : -90.0) * depthVar(q);
		}
		adoptDue(q);
	}

	double depthVar(const Params& q) { return 1.0 + 0.4 * q.loose * rng.bi(); }

	void adoptDue(const Params& q) {
		for (int i = 0; i < NV; i++) {
			VoiceState& s = v[i];
			if (s.pending && s.tv >= 0) {
				s.info = s.pendingInfo;
				s.pending = false;
				if (noteOn && !s.muted) s.gate = true;
				if (s.slidePending) {
					Plan pl = plan(i, s.info);
					if (i == 0 || pl.kind == K_RUB) { s.slideAmp = s.slidePendAmp; s.slideT = s.tv; s.slideTau = i == 0 ? 0.045 : 0.07; }
					s.slidePending = false;
				}
			}
		}
		(void)q;
	}

	void endWhoop() {
		whoopOn = false;
		for (int i = 0; i < NV; i++) v[i].whooping = false;
	}

	void startWhoop(const Params& q, bool triggered) {
		whoopOn = true; whoopT = 0; whoopTriggered = triggered;
		v[0].whooping = true; v[0].whoopHeight = 1000;
		if (q.voices >= 5) { v[4].whooping = true; v[4].whoopHeight = 1150; }
	}

	void fireSlideNow(const Params& q) {
		for (int i = 0; i < NV; i++) {
			VoiceState& s = v[i];
			Plan pl = plan(i, s.info);
			if (i == 0 || pl.kind == K_RUB) {
				s.slideAmp = (i == 0 ? -160.0 : -90.0) * depthVar(q);
				s.slideT = 0; s.slideTau = i == 0 ? 0.045 : 0.07;
			}
		}
	}

	static double shakeOffset(double t) {
		double o = 0;
		for (int k = 0; k < 3; k++) {
			double u = t - 0.09 * k;
			if (u >= 0 && u < 0.028) { double s = std::sin(M_PI * u / 0.028); o += s * s; }
		}
		return o;
	}

	void process(const Params& q, const Inputs& in) {
		p = q;
		// ---- input pitch ----
		double semis = std::max(-54.0, std::min(54.0, in.voct * 12.0));
		bool aChanged = false;
		if (!haveA || std::fabs(semis - curA) > 0.6) {
			int na = (int)std::lround(semis);
			if (!haveA || na != curA) aChanged = true;
			curA = na; haveA = true;
		}
		// ---- gate ----
		int ge = 0;
		bool gateHigh;
		if (in.gatePatched) { ge = gateT.process(in.gate); gateHigh = gateT.high; }
		else { gateHigh = true; if (!noteOn) ge = 1; gateT.high = true; }

		if (ge == 1) {
			noteOn = true; sinceRise = 0;
			for (int i = 0; i < NV; i++) v[i].gate = false;
			noteStart(curA, q, in);
		} else if (ge == -1) {
			noteOn = false;
			bool doWhoop = cadence && rng.uni() < q.whoop;
			for (int i = 0; i < NV; i++) { v[i].gate = false; v[i].pending = false; v[i].slidePending = false; }
			if (whoopOn && !whoopTriggered) endWhoop();
			if (doWhoop) startWhoop(q, false);
			if (whoopOn) for (int i = 0; i < NV; i++) if (v[i].whooping) v[i].gate = true;
		} else if (noteOn && aChanged) {
			if (sinceRise < 0.002) {
				// adopted silently as part of the note just started
				NoteInfo n = snap(curA, cur.rootSemi, q);
				n.spread = cur.spread; n.rub2 = cur.rub2;
				cur = n;
				for (int i = 0; i < NV; i++) { v[i].pendingInfo = cur; if (!v[i].pending) v[i].info = cur; }
			} else {
				noteStart(curA, q, in);
			}
		}
		(void)gateHigh;

		// ---- PHRASE (acted on 2 ms after the edge) ----
		if (phraseT.process(in.phrase) == 1) phrasePend = 0.002;
		if (phrasePend >= 0) {
			phrasePend -= dt;
			if (phrasePend < 0) {
				if (noteOn) { cadence = true; tc = 0; }
				else armNext = true;
				phrasePend = -1;
			}
		}

		// ---- ornament triggers ----
		if (slideTrigT.process(in.slideTrig) == 1 && started) fireSlideNow(q);
		if (shakeTrigT.process(in.shakeTrig) == 1 && started) { shakeT0 = 0; shakeDepth = 190.0 * depthVar(q); }
		if (whoopTrigT.process(in.whoopTrig) == 1 && started) {
			startWhoop(q, true);
			for (int i = 0; i < NV; i++) if (v[i].whooping) v[i].gate = true;
		}

		// ---- clocks ----
		sinceRise += dt;
		if (noteOn) noteTime += dt;
		if (tc >= 0) tc += dt;
		for (int i = 0; i < NV; i++) {
			v[i].tv += dt;
			if (v[i].slideT >= 0) v[i].slideT += dt;
		}
		adoptDue(q);

		// shake by rule
		if (noteOn && noteTime >= nextShakeTry) {
			if (rng.uni() < q.shake) { shakeT0 = 0; shakeDepth = 190.0 * depthVar(q); }
			nextShakeTry = q.shake > 0.75 ? nextShakeTry + 0.6 : 1e9;
		}
		if (shakeT0 >= 0) { shakeT0 += dt; if (shakeT0 > 0.3) shakeT0 = -1; }
		double shakeBump = shakeT0 >= 0 ? shakeOffset(shakeT0) : 0.0;

		// whoop clock
		double wx = 0;
		if (whoopOn) {
			whoopT += dt;
			wx = std::min(1.0, whoopT / 0.170);
			if (whoopT >= 0.170) {
				for (int i = 0; i < NV; i++) if (v[i].whooping) {
					v[i].gate = false;
					if (whoopTriggered && noteOn) v[i].muted = true;
				}
				endWhoop();
				wx = 0;
			}
		}

		// ---- slow noises ----
		if (++noiseCount >= noiseDiv) {
			noiseCount = 0;
			for (int i = 0; i < NV; i++) { v[i].drift.step(rng); v[i].wander.step(rng); v[i].pushWander.step(rng); }
		}

		// ---- pitches ----
		double W = rubCents(q.rub), B = rubHz(q.rub);
		if (cadence && !q.resolve && tc >= 0) {
			double f = 1.0 - 0.4 * clamp01(tc / 0.15);
			W *= f; B *= f;
		}
		double leadY = v[0].yInit ? v[0].y : leadCents(v[0].info, q);
		double fLead = centsToHz(leadY);
		int firstRub = -1;
		for (int i = 0; i < NV; i++) {
			VoiceState& s = v[i];
			Plan pl = plan(i, s.info);
			double lc = leadCents(s.info, q);
			double target;
			s.rubbing = false;
			switch (pl.kind) {
				case K_LEAD: target = lc; break;
				case K_DEG: case K_THIRD: target = 100.0 * s.info.rootSemi + degCents(pl.deg, q); break;
				default: {
					s.rubbing = i < q.voices;
					if (firstRub < 0 && i > 0 && i < q.voices) firstRub = i;
					double sc = degCents(s.info.L + pl.sgn * pl.k, q) - degCents(s.info.L, q);
					double tgt;
					if (!q.hz) tgt = pl.sgn * pl.k * W;
					else tgt = 1200.0 * std::log2(std::max(fLead + pl.sgn * pl.k * B, 0.5 * fLead) / fLead);
					tgt *= 1.0 + 0.12 * q.loose * s.wander.value();
					double iv = sc + lean(s) * (tgt - sc);
					if (cadence && q.resolve && tc >= 0) iv *= clamp01(1.0 - (tc - 0.35) / 0.30);
					target = lc + iv;
				}
			}
			// glide
			if (!s.yInit || s.lowTime >= 0.050) { s.y = target; s.yInit = true; }
			else {
				double tau = i == 0 ? 0.012 : 0.022;
				double a = std::exp(-dt / tau);
				s.y = target + a * (s.y - target);
			}
			// ornaments
			double orn = 0;
			if (s.slideT >= 0) {
				double e = s.slideAmp * std::exp(-s.slideT / s.slideTau);
				orn += e;
				if (std::fabs(e) < 0.01) s.slideT = -1;
			}
			if (i == 0 && shakeBump > 0) orn += shakeDepth * shakeBump;
			if (s.whooping) orn += s.whoopHeight * std::pow(wx, 1.6);
			double dr = 9.0 * q.loose * s.drift.value() * (i == 0 ? 0.5 : 1.0);
			s.outCents = s.y + orn + dr;
		}

		// ---- push (section 7) ----
		double leadLean = firstRub >= 0 ? lean(v[firstRub]) : 0.0;
		if (v[1].rubbing) leadLean = lean(v[1]);
		bool tight = cadence && !q.resolve && tc >= 0;
		for (int i = 0; i < NV; i++) {
			VoiceState& s = v[i];
			double t;
			if (i == 0) t = firstRub >= 0 ? 7.0 + 3.0 * leadLean : 7.0;
			else if (s.rubbing) t = 5.5 + 4.5 * lean(s);
			else if (i == 2) {
				Plan pl = plan(2, s.info);
				t = std::abs(v[0].info.L - pl.deg) == 1 ? 7.5 : 6.0;
			} else t = 6.0;
			if (tight && (i == 0 || s.rubbing)) t += 1.0;
			if (i == 0 && shakeBump > 0) t *= 0.55;
			t += 0.5 * q.loose * s.pushWander.value();
			bool direct = false;
			if (s.whooping) { t = 10.0 * std::pow(1.0 - wx, 0.7); direct = true; }
			if (!s.gate) { s.push = 0; s.lowTime += dt; }
			else {
				s.lowTime = 0;
				if (direct) s.push = t;
				else {
					double tau = t > s.push ? 0.030 : 0.060;
					s.push = t + std::exp(-dt / tau) * (s.push - t);
				}
			}
			s.outPush = std::max(0.0, std::min(10.0, s.push));
			if (!s.gate) s.outPush = 0;
		}
	}

	// convenience accessors
	double volts(int i) const { return std::max(-6.0, std::min(6.0, v[i].outCents / 1200.0)); }
	double gateV(int i) const { return v[i].gate ? 10.0 : 0.0; }
	double pushV(int i) const { return v[i].outPush; }
};

}  // namespace bulgaria
