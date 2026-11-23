// Pivot: metric modulation clock engine. No Rack dependency, so test/render.cpp can drive it.
//
// Two time-lines:
//   H = "home" position in beats, taken from the incoming clock (or the BPM knob).
//   P = "pivot" position in beats, what the outputs play.
// Every sample P moves by ratio * (change in H), so tempo changes never break the phase:
// a metric modulation just changes how fast P runs against H.
//
// A step's ratio comes from "OLD note = NEW note": the old note value, at the old tempo,
// lasts as long as the new note value at the new tempo, so new tempo = old * NEW / OLD.
// Steps multiply along the chain (step k = step1 * ... * stepk), and HOME = 1.
#pragma once
#include <cmath>
#include <cstdint>

namespace pivot {

static const int NUM_STEPS = 8;
static const int NUM_NOTES = 13;
// Note values in quarter-note beats, as fractions n/d.
static const char* const NOTE_NAMES[NUM_NOTES] = {
	"1/16T", "1/16Q", "1/16", "1/8T", "1/16.", "1/8Q", "1/8", "1/4T", "1/8.", "1/4", "1/2T", "1/4.", "1/2"};
static const char* const NOTE_LONG[NUM_NOTES] = {
	"16th triplet", "16th quintuplet", "16th", "8th triplet", "dotted 16th", "8th quintuplet", "8th",
	"quarter triplet", "dotted 8th", "quarter", "half triplet", "dotted quarter", "half"};
static const int NOTE_N[NUM_NOTES] = {1, 1, 1, 1, 3, 2, 1, 2, 3, 1, 4, 3, 2};
static const int NOTE_D[NUM_NOTES] = {6, 5, 4, 3, 8, 5, 2, 3, 4, 1, 3, 2, 1};

inline int gcd(int a, int b) { while (b) { int t = a % b; a = b; b = t; } return a < 0 ? -a : a; }

// Ratio of one step as a reduced fraction num/den (new tempo / old tempo).
inline void stepRatio(int oldIdx, int newIdx, int& num, int& den) {
	num = NOTE_N[newIdx] * NOTE_D[oldIdx];
	den = NOTE_D[newIdx] * NOTE_N[oldIdx];
	int g = gcd(num, den);
	num /= g; den /= g;
}

static const int NUM_PPQN = 7;
static const int PPQN_VALUES[NUM_PPQN] = {1, 2, 4, 8, 12, 24, 48};
static const int NUM_GLIDES = 8;
static const char* const GLIDE_NAMES[NUM_GLIDES] = {"Snap", "1 beat", "2 beats", "1 bar", "2 bars", "4 bars", "8 bars", "16 bars"};
static const int NUM_AUTO = 7;
static const int AUTO_BARS[NUM_AUTO] = {0, 1, 2, 4, 8, 16, 32};

enum Quant { QUANT_NOW = 0, QUANT_BEAT, QUANT_BAR };
enum AtEnd { END_HOME = 0, END_LOOP, END_HOLD };
enum Mode { MODE_STEADY = 0, MODE_GLIDE, MODE_LAND };

struct Settings {
	double cum[NUM_STEPS + 1];  // tempo ratio of HOME (=1) and of each step, already multiplied up
	int length;                 // steps in the chain, 1-8
	int autoBars;               // 0 = off
	int quant;
	int atEnd;
	bool land;
	int glideIdx;
	int beatsPerBar;
	int inPpqn, outPpqn;
	double bpm;                 // used when no clock is patched
	bool clockConnected;
	Settings() : length(4), autoBars(0), quant(QUANT_BAR), atEnd(END_HOME), land(true), glideIdx(0),
	             beatsPerBar(4), inPpqn(24), outPpqn(4), bpm(120.0), clockConnected(false) {
		for (int i = 0; i <= NUM_STEPS; i++) cum[i] = 1.0;
	}
	double glideBeats() const {
		static const double g[NUM_GLIDES] = {0, 1, 2, -1, -2, -4, -8, -16};  // negative = bars
		double v = g[glideIdx < 0 ? 0 : glideIdx >= NUM_GLIDES ? NUM_GLIDES - 1 : glideIdx];
		return v < 0 ? -v * beatsPerBar : v;
	}
};

// One sample's events in.
struct Events {
	bool clock, reset, next, home;
	int request;  // -1 none, 0 = HOME, 1-8 = step (buttons, SELECT CV)
	Events() : clock(false), reset(false), next(false), home(false), request(-1) {}
};

// One sample's results out. Triggers are true for one sample; the Rack side stretches them.
struct Out {
	bool clock, ghost;        // 50% gates at OUT PPQN
	bool beat, bar, reset, pivot;
	bool away;
	double ratio;             // current tempo / home tempo
	Out() : clock(false), ghost(false), beat(false), bar(false), reset(false), pivot(false), away(false), ratio(1.0) {}
};

struct Engine {
	// time lines
	double H = 0.0, P = 0.0, r = 1.0;
	double ghostP = 0.0, ghostR = 1.0;
	// incoming clock
	long pulses = -1;
	double time = 0.0, lastPulseTime = -1.0, period = 0.0;
	bool started = false;
	double homeBpm = 120.0;
	// chain
	int step = 0;
	int pending = -1;
	int barsAtStep = 0;
	int lastSelect = -1;
	// tempo move in progress
	int mode = MODE_STEADY;
	double mH0 = 0, mL = 1, mA = 1, mB = 1, mC = 0, mPt = 0;
	// edge tracking
	long lastBeat = -1, lastBar = -1, lastGhostBeat = -1;

	// history for the screen: H positions of recent output beats / ghost beats
	static const int HIST = 256;
	double histH[HIST];
	uint8_t histKind[HIST];  // 1 beat, 2 bar, 3 ghost beat
	int histPos = 0;

	Engine() { clearHistory(); }

	void clearHistory() {
		for (int i = 0; i < HIST; i++) { histH[i] = -1e9; histKind[i] = 0; }
	}
	void remember(double h, uint8_t k) {
		histH[histPos] = h; histKind[histPos] = k;
		histPos = (histPos + 1) % HIST;
	}

	void doReset(const Settings& s, Out& o) {
		H = P = ghostP = 0.0;
		r = ghostR = 1.0;
		pulses = -1;
		started = !s.clockConnected;
		step = 0; pending = -1; barsAtStep = 0;
		mode = MODE_STEADY;
		lastBeat = lastBar = lastGhostBeat = -1;
		clearHistory();
		o.reset = true;
	}

	bool atHomeAndLanded() const { return step == 0 && mode == MODE_STEADY; }

	// What NEXT means from the current step (or the pending one, so repeated NEXTs queue up).
	int nextTarget(const Settings& s) const {
		int from = pending >= 0 ? pending : step;
		if (from < s.length) return from + 1;
		if (s.atEnd == END_HOME) return 0;
		if (s.atEnd == END_LOOP) return 1;
		return from;
	}

	double landRatio(double t) const { return mA + (1.0 - mA) * t + mC * t * (1.0 - t); }

	void startLanding(const Settings& s) {
		double B = s.beatsPerBar;
		double gb = s.glideBeats();
		double Lmin = gb > 2 * B ? gb : 2 * B;
		double Ht = std::ceil((H + Lmin) / B - 1e-9) * B;
		mH0 = H; mL = Ht - H;
		mA = gb > 0 ? r : 1.0;
		double avg = (mA + 1.0) * 0.5;
		double Pt = std::floor((P + mL * avg) / B + 0.5) * B;
		for (int tries = 0; tries < 16; tries++) {
			if (Pt <= P) { Pt += B; continue; }
			mC = 6.0 * ((Pt - P) / mL - avg);
			double lo = 1e9;
			for (int i = 0; i <= 32; i++) { double v = landRatio(i / 32.0); if (v < lo) lo = v; }
			if (lo >= 0.2 * (mA < 1.0 ? mA : 1.0)) break;
			Pt += B;
		}
		mPt = Pt;
		mode = MODE_LAND;
	}

	void apply(int target, const Settings& s, Out& o) {
		ghostR = r; ghostP = P;
		lastGhostBeat = (long) std::floor(ghostP + 1e-9);
		double rt = s.cum[target];
		if (target == 0 && s.land) {
			startLanding(s);
		}
		else if (s.glideBeats() > 0) {
			mode = MODE_GLIDE; mH0 = H; mL = s.glideBeats(); mA = r; mB = rt;
		}
		else {
			mode = MODE_STEADY; r = rt;
		}
		step = target;
		barsAtStep = 0;
		o.pivot = true;
	}

	Out process(double dt, const Settings& s, const Events& e) {
		Out o;
		time += dt;
		if (e.reset) doReset(s, o);
		if (!s.clockConnected) started = true;

		// ---- requests
		if (e.request >= 0 && e.request <= NUM_STEPS) pending = e.request;
		if (e.home) pending = 0;
		if (e.next) pending = nextTarget(s);

		// ---- home time line
		double newH = H;
		if (s.clockConnected) {
			double ppq = s.inPpqn;
			if (e.clock) {
				pulses++;
				if (lastPulseTime >= 0) {
					double p = time - lastPulseTime;
					if (p > 1e-4) {
						period = p;
						double bpm = 60.0 / (p * ppq);
						homeBpm += (bpm - homeBpm) * (bpm > homeBpm * 1.5 || bpm < homeBpm / 1.5 ? 1.0 : 0.1);
					}
				}
				lastPulseTime = time;
				if (!started) { started = true; lastBeat = lastBar = lastGhostBeat = -1; }
				double hp = pulses / ppq;
				if (hp > newH) newH = hp;
			}
			else if (period > 0 && pulses >= 0 && time - lastPulseTime < period * 4.0) {
				double ext = newH + dt / (period * ppq);
				double cap = (pulses + 1) / ppq - 1e-7;  // never run past the next pulse
				newH = ext < cap ? ext : (newH > cap ? newH : cap);
			}
		}
		else {
			newH = H + dt * s.bpm / 60.0;
			homeBpm = s.bpm;
		}
		double dH = newH - H;
		if (dH < 0) dH = 0;

		// ---- ratio for this sample, and move P
		double prevP = P;
		bool landed = false;
		if (mode == MODE_STEADY) {
			r = s.cum[step];
			P += r * dH;
		}
		else {
			double t = (newH - mH0) / mL;
			if (t >= 1.0) {
				if (mode == MODE_LAND) {
					// finish exactly on the target downbeat, then carry on at home tempo
					P = mPt + (newH - (mH0 + mL));
					r = 1.0;
					landed = true;
				}
				else {
					r = mB;
					P += r * dH;
				}
				mode = MODE_STEADY;
			}
			else {
				r = (mode == MODE_GLIDE) ? mA * std::pow(mB / mA, t) : landRatio(t);
				P += r * dH;
			}
		}
		if (P < prevP) P = prevP;
		ghostP += ghostR * dH;
		H = newH;
		(void) landed;

		// ---- beats and bars of the output
		const double B = s.beatsPerBar;
		bool beatEdge = false, barEdge = false;
		if (started) {
			long b = (long) std::floor(P + 1e-9);
			long br = (long) std::floor(P / B + 1e-9);
			if (b != lastBeat) { beatEdge = true; lastBeat = b; remember(H, 1); }
			if (br != lastBar) {
				barEdge = true; lastBar = br;
				histKind[(histPos + HIST - 1) % HIST] = 2;
				barsAtStep++;
			}
			long gb = (long) std::floor(ghostP + 1e-9);
			if (gb != lastGhostBeat) { lastGhostBeat = gb; remember(H, 3); }
		}
		o.beat = beatEdge;
		o.bar = barEdge;

		// ---- AUTO: every N bars at a step, ask for the next one
		if (barEdge && s.autoBars > 0 && pending < 0 && barsAtStep > s.autoBars) {
			pending = nextTarget(s);
		}

		// ---- a request for the step we are already steady on does nothing
		if (pending == step && mode == MODE_STEADY) pending = -1;

		// ---- apply a pending pivot on the chosen grid
		if (pending >= 0 && started) {
			bool go = s.quant == QUANT_NOW || (s.quant == QUANT_BEAT && beatEdge) || (s.quant == QUANT_BAR && barEdge);
			if (go) {
				int t = pending;
				pending = -1;
				apply(t, s, o);
				if (barEdge) barsAtStep = 1;
			}
		}

		// ---- outputs
		double opp = s.outPpqn;
		if (started) {
			double f = P * opp - std::floor(P * opp);
			o.clock = f < 0.5;
			double g = ghostP * opp - std::floor(ghostP * opp);
			o.ghost = g < 0.5;
		}
		o.ratio = r;
		o.away = !(step == 0 && mode == MODE_STEADY);
		return o;
	}

	double tempo() const { return homeBpm * r; }
};

}  // namespace pivot
