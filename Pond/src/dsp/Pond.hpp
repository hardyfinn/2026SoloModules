// Pond: a chorus of frogs and toads that call, answer, fall silent when a predator
// comes, and then tentatively build up again.
// No Rack includes, so test/render_pond.cpp can run it offline.
//
// One frog:
//   call scheduler -> notes -> pulses -> source (whistle oscillator + glottal buzz)
//   -> buzz rings three resonators (carrier, second band, vocal sac) -> position (pan, distance)
// The pond:
//   every frog hears every other frog (closer is louder). What it hears drives
//   TALK (synchronise <-> call and answer), BOUTS (excitement spreads, then tiredness),
//   and the HUSH recovery (safety in numbers: hearing others calms a frightened frog).
#pragma once
#include <cmath>
#include <cstdint>
#include <algorithm>

namespace pond {

static const double PI = 3.14159265358979323846;
static const int MAX_FROGS = 16;
static const int CTL = 32;                      // control rate: every 32 samples

// ------------------------------------------------------------------ species
// Values at 20 °C. Sources: docs/POND-RESEARCH.md.
struct Species {
	const char* name;      // display name
	double carrier;        // dominant frequency, Hz
	double f2ratio, f2gain;// second band (x carrier) and its level
	double tonal;          // 1 = pure whistle, 0 = glottal buzz through the resonators
	double f0;             // buzz fundamental, Hz (0 = no buzz)
	double q;              // resonator Q for the buzz
	double sac;            // vocal-sac resonance (low, at 2 x f0) level
	double pulseRate;      // pulses per second inside a note (0 = smooth note)
	double duty;           // fraction of each pulse period that sounds
	double noteLen;        // seconds
	int notesMin, notesMax;// notes per call
	double noteGap;        // seconds between notes
	double callGap;        // seconds of silence between calls (eager frog)
	double sweep;          // semitones over each note (+ rises)
	double accel;          // pulse rate at the end of a note / at the start
	double noteStep;       // semitones added per note (coqui: "co" -> "qui")
	double attack, release;// note envelope, seconds
	double rasp;           // roughness: jitter and breath noise
	double gain;           // loudness trim so all species sit at a similar level
};

static const int NUM_SPECIES = 16;
static const Species SPECIES[NUM_SPECIES] = {
	// name             carr   f2r  f2g  ton  f0    q    sac  pulse duty note  nmin nmax ngap  cgap swp  acc  step att    rel   rasp gain
	{"SPRING PEEPER",   2900, 2.00, 0.06, 1.0,  0,   10, 0.0,  0,  0.0, 0.13, 1, 1, 0.00, 0.85, 2.6, 1.0, 0.0, 0.020, 0.040, 0.05, 0.70},
	{"AMERICAN TOAD",   1800, 2.00, 0.10, 0.85, 900, 12, 0.0, 45, 0.55, 8.00, 1, 1, 0.00, 9.00, -0.3,1.0, 0.0, 0.400, 0.500, 0.10, 0.80},
	{"GRAY TREEFROG",   2200, 0.50, 0.70, 0.45, 1100, 9, 0.0, 22, 0.50, 0.70, 1, 1, 0.00, 4.50, 0.0, 1.0, 0.0, 0.050, 0.080, 0.15, 1.00},
	{"BULLFROG",         250, 5.60, 0.45, 0.05, 105,  5, 0.8,  0, 0.00, 0.70, 2, 5, 0.38, 7.00, -1.2,1.0, -0.5,0.060, 0.180, 0.25, 1.10},
	{"GREEN FROG",       560, 4.50, 0.35, 0.15, 135,  7, 0.5,  0, 0.00, 0.13, 1, 3, 0.40, 3.50, -2.5,1.0, -0.8,0.004, 0.120, 0.15, 1.30},
	{"WOOD FROG",        900, 1.80, 0.50, 0.10, 210,  5, 0.3, 60, 0.65, 0.09, 2, 4, 0.13, 3.00, 1.2, 1.0, 0.3, 0.008, 0.040, 0.50, 1.00},
	{"CHORUS FROG",     3000, 0.55, 0.25, 0.65, 1500, 9, 0.0, 14, 0.40, 0.60, 1, 1, 0.00, 2.40, 1.6, 2.3, 0.0, 0.020, 0.030, 0.15, 0.90},
	{"COQUI",           1150, 2.00, 0.05, 1.0,  0,   10, 0.0,  0, 0.00, 0.11, 2, 2, 0.06, 1.70, 1.6, 1.0, 8.5, 0.008, 0.035, 0.03, 0.70},
	{"NATTERJACK",      1700, 0.50, 0.35, 0.45, 850,  8, 0.0, 32, 0.50, 1.60, 1, 1, 0.00, 3.50, 0.0, 1.0, 0.0, 0.080, 0.120, 0.30, 0.95},
	{"TREE FROG",       2200, 0.45, 0.50, 0.35, 650,  8, 0.0,170, 0.55, 0.06, 4,10, 0.11, 3.00, 0.0, 1.0, 0.0, 0.005, 0.020, 0.25, 1.00},
	{"FIRE-BELLY",       480, 2.00, 0.15, 0.90, 160, 10, 0.2,  0, 0.00, 0.20, 1, 1, 0.00, 1.60, 0.6, 1.0, 0.0, 0.050, 0.120, 0.05, 0.85},
	{"MIDWIFE TOAD",    1400, 2.00, 0.04, 1.0,  0,   10, 0.0,  0, 0.00, 0.15, 1, 1, 0.00, 2.20, 0.0, 1.0, 0.0, 0.006, 0.130, 0.02, 0.75},
	{"LEOPARD FROG",    1300, 0.40, 0.60, 0.15, 300,  6, 0.2, 18, 0.45, 2.20, 1, 1, 0.00, 6.00, -1.0,0.75,0.0, 0.050, 0.200, 0.50, 1.00},
	{"COMMON TOAD",      650, 2.20, 0.40, 0.25, 220,  6, 0.3, 70, 0.60, 0.15, 3, 6, 0.24, 4.00, 0.0, 1.0, 0.0, 0.010, 0.040, 0.40, 1.00},
	{"COMMON FROG",      450, 2.80, 0.30, 0.15, 160,  5, 0.4, 25, 0.60, 0.80, 1, 2, 0.40, 5.00, 0.0, 1.0, 0.0, 0.080, 0.150, 0.35, 1.15},
	{"MARSH FROG",      1300, 0.55, 0.50, 0.30, 420,  7, 0.1, 45, 0.50, 0.25, 3, 6, 0.14, 4.00, 0.0, 1.3, 0.0, 0.010, 0.040, 0.35, 0.95},
};

// ------------------------------------------------------------------ knob mappings
inline double mapTempC(double k) { return 5.0 + 25.0 * k; }                      // 5..30 °C
inline double tempSpeed(double c) { return std::pow(2.0, (c - 20.0) / 11.0); }    // x2 every 11 °C
inline double tempActivity(double c) { return std::min(1.0, std::max(0.0, (c - 6.0) / 6.0)); }
inline double mapPulse(double k) { return std::pow(2.0, (k - 0.5) * 2.0); }      // x0.5..x2
inline double mapLength(double k) { return std::pow(2.0, (k - 0.5) * 4.0); }     // x0.25..x4
inline double mapReturn(double k) { return 2.0 * std::pow(30.0, k); }            // 2..60 s
inline int mapFrogs(double k) { return std::max(1, std::min(MAX_FROGS, (int)std::round(k))); }

// xorshift32
struct Rng {
	uint32_t s = 1;
	void seed(uint32_t seed) {
		uint64_t v = ((uint64_t)seed * 2654435761ull) % 4294967296ull;
		s = v ? (uint32_t)v : 1u;
	}
	double uni() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s / 4294967296.0; }
	double gauss() { return (uni() + uni() + uni() + uni() - 2.0) * 1.7320508; }
};

struct Params {
	int spA = 0, spB = 1;
	double mix = 0.0;          // fraction of frogs that are species B
	int frogs = 8;
	double pitch = 0.0;        // semitones
	double pulse = 1.0;        // pulse-rate multiplier
	double length = 1.0;       // note-length multiplier
	double rough = 0.5;        // 0.5 = the species' own roughness
	double variety = 0.5;      // how different individuals are
	double space = 0.4;        // pond width and distance, plus night air
	double tempC = 20.0;
	double busy = 0.7;         // how keen the pond is to sing
	double bouts = 0.4;        // excitement spreads, then tiredness
	double talk = 0.5;         // -1 synchronise, 0 independent, +1 call and answer
	double returnS = 12.0;     // seconds for frightened frogs to recover after HUSH
};

// Complex one-pole resonator (rings at f, decays with bandwidth bw).
struct Res {
	double zr = 0, zi = 0, cr = 1, ci = 0, mag = 0, ang = 0;
	void set(double f, double bw, double dt) {
		ang = 2.0 * PI * f * dt; mag = std::exp(-PI * bw * dt);
		cr = mag * std::cos(ang); ci = mag * std::sin(ang);
	}
	// add an impulse 'late' samples ago
	void inject(double a, double late) {
		double m = a * std::pow(mag, late), t = ang * late;
		zr += m * std::cos(t); zi += m * std::sin(t);
	}
	double tick() {
		double r = zr * cr - zi * ci, i = zr * ci + zi * cr;
		zr = std::fabs(r) < 1e-30 ? 0.0 : r; zi = std::fabs(i) < 1e-30 ? 0.0 : i;
		return zi;
	}
	void clear() { zr = zi = 0.0; }
};

struct OnePole {
	double b = 1, a1 = 0, x1 = 0, y1 = 0;
	void set(double fc, double sr) {
		double k = std::tan(PI * std::min(fc, 0.45 * sr) / sr);
		b = k / (1 + k); a1 = (k - 1) / (k + 1);
	}
	double process(double x) {
		double y = b * x + b * x1 - a1 * y1;
		x1 = x; y1 = (std::fabs(y) < 1e-30) ? 0.0 : y;
		return y;
	}
};

// ------------------------------------------------------------------ one frog
struct Frog {
	// who it is (fixed per pond seed)
	double x = 0, y = 0.5;      // position: x -1..1 left/right, y 0 near .. 1 far
	double size = 0;            // gaussian: bigger frogs sing lower and slower
	double eager = 1.0;         // how readily it calls
	double brave = 1.0;         // how quickly it recovers from a HUSH
	double mixRank = 0.5;       // turns into species B when MIX passes this
	double rateJit = 1.0, gapJit = 1.0;
	Rng rng;

	// chorus state
	int species = 0;
	double phase = 0.0;         // 0..1, calls at 1
	double arousal = 0.3, fatigue = 0.0, fear = 0.0;
	double heard = 0.0;         // what it hears of the others, smoothed
	double answerT = -1.0;      // >= 0: answer this many seconds from now
	bool armed = false;         // ready, waiting for a clock edge or a quiet moment
	bool calling = false;
	double callGain = 1.0;      // tentative calls are quieter

	// call scheduler
	int note = 0, notes = 1;
	bool inNote = false;
	double t = 0.0, noteLen = 0.1, gapLen = 0.0;
	double killEnv = 1.0;       // fast fade when hushed
	bool killing = false;

	// sound state
	double oscPh = 0, osc2Ph = 0, buzzPh = 0, pulsePh = 0, pulseAmp = 1, pulseRateNow = 0;
	double fNow = 1000, f0Now = 100, prNow = 0, raspNow = 0;
	Res r1, r2, r3;
	double buzzNorm = 1.0, bw1 = 100;
	OnePole air;
	double gL = 0.7, gR = 0.7, level = 0.0, levelOut = 0.0;
	double lastNoteEnv = 0.0;

	// outputs
	bool trigOut = false;
	double pitchV = 0.0;        // carrier as V/OCT (0 V = C4)
};

// Freeverb-style night air (public-domain design): 4 combs + 2 allpasses per side.
struct NightAir {
	static const int NC = 4, NA = 2;
	float* buf = nullptr;
	int combLen[2][NC], apLen[2][NA], combPos[2][NC], apPos[2][NA], combOff[2][NC], apOff[2][NA];
	double combLp[2][NC];
	int total = 0;
	void setSampleRate(double sr) {
		static const int CB[NC] = {1116, 1277, 1422, 1557};
		static const int AP[NA] = {556, 341};
		double s = sr / 44100.0;
		int off = 0;
		for (int c = 0; c < 2; c++) {
			for (int i = 0; i < NC; i++) { combLen[c][i] = (int)((CB[i] + 23 * c) * s); combOff[c][i] = off; off += combLen[c][i]; combPos[c][i] = 0; combLp[c][i] = 0; }
			for (int i = 0; i < NA; i++) { apLen[c][i] = (int)((AP[i] + 23 * c) * s); apOff[c][i] = off; off += apLen[c][i]; apPos[c][i] = 0; }
		}
		delete[] buf; buf = new float[off]; total = off;
		for (int i = 0; i < off; i++) buf[i] = 0.f;
	}
	~NightAir() { delete[] buf; }
	void process(double inL, double inR, double& outL, double& outR) {
		const double fb = 0.78, damp = 0.45;
		double in[2] = {inL, inR}, out[2] = {0, 0};
		for (int c = 0; c < 2; c++) {
			double acc = 0;
			for (int i = 0; i < NC; i++) {
				float* b = buf + combOff[c][i]; int& p = combPos[c][i];
				double y = b[p];
				combLp[c][i] = y * (1 - damp) + combLp[c][i] * damp;
				if (std::fabs(combLp[c][i]) < 1e-30) combLp[c][i] = 0;
				b[p] = (float)(in[c] + combLp[c][i] * fb);
				if (++p >= combLen[c][i]) p = 0;
				acc += y;
			}
			for (int i = 0; i < NA; i++) {
				float* b = buf + apOff[c][i]; int& p = apPos[c][i];
				double bo = b[p];
				b[p] = (float)(acc + bo * 0.5);
				acc = bo - acc;
				if (++p >= apLen[c][i]) p = 0;
			}
			out[c] = acc;
		}
		outL = out[0]; outR = out[1];
	}
};

// ------------------------------------------------------------------ the pond
struct Pond {
	double sr = 48000.0, dt = 1.0 / 48000.0;
	Frog frogs[MAX_FROGS];
	int n = 0;                  // frogs in use
	uint32_t seedV = 1;
	long counter = 0;
	NightAir* airVerb = nullptr;
	OnePole verbLp[2];
	double env = 0.0, envAtt = 0, envRel = 0;
	bool tick = false;          // set for one sample by clockTick()
	bool clocked = false;
	bool held = false;           // HUSH held: nobody starts a call
	int callingCount = 0;
	double meanFear = 0.0;
	Rng rng;

	Pond() { airVerb = new NightAir; setSampleRate(48000.0); reseed(1); }
	~Pond() { delete airVerb; }
	Pond(const Pond&) = delete;
	Pond& operator=(const Pond&) = delete;

	void setSampleRate(double s) {
		sr = s; dt = 1.0 / s;
		airVerb->setSampleRate(s);
		verbLp[0].set(4500.0, s); verbLp[1].set(4500.0, s);
		envAtt = 1.0 - std::exp(-1.0 / (0.005 * s));
		envRel = 1.0 - std::exp(-1.0 / (0.250 * s));
	}

	// New pond: new positions and individuals.
	void reseed(uint32_t s) {
		seedV = s; rng.seed(s * 7u + 3u);
		for (int i = 0; i < MAX_FROGS; i++) {
			Frog& f = frogs[i];
			Frog fresh;
			f = fresh;
			f.rng.seed(s + 101u * (uint32_t)(i + 1));
			Rng& r = f.rng;
			// spread across the pond: golden-ratio stepping so few frogs still spread out
			f.x = std::fmod(0.5 + i * 0.6180339887, 1.0) * 2.0 - 1.0;
			f.x = std::max(-1.0, std::min(1.0, f.x + 0.15 * r.gauss()));
			f.y = 0.08 + 0.92 * r.uni();
			f.size = std::max(-2.0, std::min(2.0, r.gauss()));
			f.eager = 0.65 + 0.7 * r.uni();
			f.brave = 0.45 + 1.1 * r.uni();
			f.mixRank = (i + 0.5) / MAX_FROGS;
			f.rateJit = r.gauss(); f.gapJit = r.gauss();
			f.phase = r.uni();
			f.arousal = 0.2 + 0.4 * r.uni();
		}
		// someone starts singing soon after the module loads
		frogs[0].phase = 0.97; frogs[0].arousal = 0.8;
		// shuffle mixRank so species B frogs are spread over the pond
		for (int i = MAX_FROGS - 1; i > 0; i--) {
			int j = (int)(rng.uni() * (i + 1)) % (i + 1);
			std::swap(frogs[i].mixRank, frogs[j].mixRank);
		}
	}

	// ---- events
	void hush() {
		for (int i = 0; i < MAX_FROGS; i++) {
			Frog& f = frogs[i];
			f.fear = 1.0;
			f.arousal *= 0.2;
			f.answerT = -1.0; f.armed = false;
			if (f.calling) f.killing = true;
			f.phase = 0.0;
		}
	}
	// Make a frog call now. frog < 0 picks the keenest one that is quiet.
	void callNow(int which, const Params& p) {
		if (which < 0) {
			double best = -1;
			for (int i = 0; i < n; i++) {
				Frog& f = frogs[i];
				if (f.calling) continue;
				double s = f.eager * (1.0 - 0.7 * f.fear) + 0.3 * rng.uni();
				if (s > best) { best = s; which = i; }
			}
			if (which < 0) return;
		}
		if (which >= n) return;
		Frog& f = frogs[which];
		if (f.calling || held) return;
		f.fear *= 0.5;
		f.arousal = std::max(f.arousal, 0.75);
		startCall(which, p);
	}
	void clockTick() { tick = true; }

	const Species& sp(const Frog& f) const { return SPECIES[std::max(0, std::min(NUM_SPECIES - 1, f.species))]; }

	void startCall(int i, const Params& p) {
		Frog& f = frogs[i];
		const Species& s = sp(f);
		double speed = tempSpeed(p.tempC) * (1.0 + 0.04 * f.rateJit * p.variety);
		int span = s.notesMax - s.notesMin;
		int nn = s.notesMin + (span > 0 ? (int)(f.rng.uni() * (span + 1)) % (span + 1) : 0);
		// a frightened frog calls tentatively: fewer notes, quieter
		double bold = 1.0 - f.fear;
		nn = std::max(1, (int)std::round(nn * (0.35 + 0.65 * bold)));
		f.callGain = (0.35 + 0.65 * std::sqrt(std::max(0.0, bold))) * (1.0 + 0.15 * f.rng.gauss() * p.variety);
		f.callGain = std::max(0.2, f.callGain);
		f.notes = nn; f.note = 0; f.t = 0.0; f.inNote = true;
		f.noteLen = s.noteLen * p.length * std::pow(2.0, 0.12 * f.size * p.variety) / std::pow(speed, 0.6);
		if (f.fear > 0.05) f.noteLen *= 0.6 + 0.4 * bold;
		f.noteLen = std::max(0.02, f.noteLen * (1.0 + 0.08 * f.rng.gauss() * p.variety));
		f.gapLen = s.noteGap / std::pow(speed, 0.8);
		f.calling = true; f.killing = false; f.killEnv = 1.0;
		f.armed = false; f.answerT = -1.0;
		f.trigOut = true;
		f.pulsePh = 0.999; f.buzzPh = 0.999 * f.rng.uni();
		f.oscPh = 0.0; f.osc2Ph = 0.0;
		f.r1.clear(); f.r2.clear(); f.r3.clear();
		// everyone else reacts to the start of a call (synchronising frogs)
		if (p.talk < 0.0) {
			double k = -p.talk;
			for (int j = 0; j < n; j++) {
				if (j == i) continue;
				Frog& g = frogs[j];
				if (g.calling || g.fear > 0.6) continue;
				double pr = prox(f, g);
				// Mirollo-Strogatz style phase advance: nearly-ready frogs jump to join in
				g.phase += k * 0.9 * pr * (0.25 + g.phase);
				if (g.phase >= 1.0 && g.answerT < 0.0) {
					g.answerT = 0.015 + 0.05 * g.rng.uni() * (1.2 - k);    // near-synchrony lag
					g.phase = 1.0;
				}
			}
		}
	}

	void endCall(int i, const Params& p) {
		Frog& f = frogs[i];
		f.calling = false; f.inNote = false; f.killing = false;
		f.phase = 0.0;
		// call and answer: neighbours that heard it may reply
		if (p.talk > 0.0) {
			double k = p.talk;
			for (int j = 0; j < n; j++) {
				if (j == i) continue;
				Frog& g = frogs[j];
				if (g.calling || g.answerT >= 0.0) continue;
				double pr = prox(f, g);
				double chance = k * pr * (0.25 + 0.75 * g.arousal) * (1.0 - g.fear) * 1.6;
				if (g.rng.uni() < chance) {
					// reply latency: quick when keen, slower and more hesitant when frightened
					g.answerT = (0.12 + 0.35 * g.rng.uni()) * (1.0 + 2.0 * g.fear) / (0.6 + 0.6 * g.arousal);
				}
			}
		}
	}

	static double prox(const Frog& a, const Frog& b) {
		double dx = (a.x - b.x) * 0.5, dy = a.y - b.y;
		return 1.0 / (1.0 + 7.0 * (dx * dx + dy * dy));
	}

	// ---- control rate: the chorus
	void control(const Params& p, const double* voct) {
		double dtc = CTL * dt;
		double tAct = tempActivity(p.tempC);
		double speed = tempSpeed(p.tempC);
		int calling = 0; double fearSum = 0.0;
		for (int i = 0; i < n; i++) {
			Frog& f = frogs[i];
			// species can change between calls
			if (!f.calling) f.species = (f.mixRank < p.mix) ? p.spB : p.spA;
			// what it hears
			double h = 0.0;
			for (int j = 0; j < n; j++) {
				if (j == i) continue;
				// a call that has just started counts as heard straight away
				double lv = frogs[j].calling && !frogs[j].killing ? std::max(frogs[j].level, 0.6) : frogs[j].level;
				h += lv * prox(frogs[j], f);
			}
			f.heard += (h - f.heard) * std::min(1.0, dtc / 0.15);
			bool hearing = h > 0.12;
			// fear: drains with time (brave frogs first); hearing others calls helps
			if (f.fear > 0.0) {
				f.fear -= dtc / (p.returnS * f.brave) + dtc * 0.35 * std::min(1.0, f.heard);
				if (f.fear < 0.0) f.fear = 0.0;
			}
			// arousal
			double drive = p.busy * tAct * f.eager * (1.0 - 0.55 * p.bouts);
			double target = drive + p.bouts * 1.6 * std::min(1.0, f.heard) - p.bouts * f.fatigue;
			target = std::max(0.0, std::min(1.0, target)) * (1.0 - f.fear) * (1.0 - f.fear);
			double tau = target > f.arousal ? 1.2 : 3.5;
			f.arousal += (target - f.arousal) * std::min(1.0, dtc / tau);
			// fatigue: singing tires, rest restores (only matters with BOUTS)
			if (f.calling) f.fatigue += dtc * 0.07 * (0.3 + p.bouts);
			else f.fatigue -= dtc * 0.03;
			f.fatigue = std::max(0.0, std::min(1.2, f.fatigue));

			if (f.calling) { calling++; }
			else {
				const Species& s = sp(f);
				double gap = s.callGap * std::pow(2.0, 0.15 * f.size * p.variety) * (1.0 + 0.2 * f.gapJit * p.variety) / speed;
				gap = std::max(0.15, gap);
				// keener frogs call more often
				f.phase += dtc / gap * (0.15 + 0.85 * f.arousal) * (p.busy > 0.0 ? 1.0 : 0.0);
				// pending answer
				if (f.answerT >= 0.0) {
					f.answerT -= dtc;
					if (f.answerT < 0.0) { f.answerT = -1.0; f.armed = true; f.phase = 1.0; }
				}
				if (f.phase >= 1.0 && !f.armed) {
					f.phase = 1.0;
					// does it actually call, or let this one pass?
					double willing = (0.1 + 0.9 * f.arousal) * tAct * (1.0 - f.fear * 0.85);
					if (p.busy <= 0.001 || f.rng.uni() > willing) f.phase = f.rng.uni() * 0.3;
					else f.armed = true;
				}
				// call and answer: do not start over a loud neighbour, wait for the gap
				if (f.armed && p.talk > 0.0 && hearing && f.rng.uni() < p.talk) {
					// inhibited: back off and try again after a short random wait,
					// so the frogs that held back do not all jump in together
					f.armed = false;
					f.answerT = 0.1 + 0.6 * f.rng.uni();
				}
				else if (f.armed && !clocked && !held) {
					startCall(i, p);
					calling++;
				}
			}
			fearSum += f.fear;
			// pitch for the PITCH output
			const Species& s = sp(f);
			double semis = p.pitch + 12.0 * voct[i] - 1.6 * f.size * p.variety;
			double fc = s.carrier * std::pow(2.0, semis / 12.0) * (1.0 + 0.004 * (p.tempC - 20.0));
			f.pitchV = std::log2(fc / 261.6256);
			setupSound(f, p, fc, speed);
		}
		callingCount = calling;
		meanFear = n > 0 ? fearSum / n : 0.0;
	}

	void setupSound(Frog& f, const Params& p, double fc, double speed) {
		const Species& s = sp(f);
		double u = f.inNote ? std::min(1.0, f.t / std::max(f.noteLen, 1e-3)) : 0.0;
		double semis = s.sweep * u + s.noteStep * f.note;
		double fnow = fc * std::pow(2.0, semis / 12.0);
		f.fNow = std::min(fnow, 0.45 * sr);
		double f0 = s.f0 * std::pow(speed, 0.25) * std::pow(2.0, (semis + p.pitch - 1.6 * f.size * p.variety) / 24.0);
		f.f0Now = f0;
		f.prNow = s.pulseRate * p.pulse * speed * (1.0 + 0.05 * f.rateJit * p.variety) * (1.0 + (s.accel - 1.0) * u);
		double r = s.rasp * 2.0 * p.rough + std::max(0.0, p.rough - 0.5) * 1.0;
		f.raspNow = std::max(0.0, std::min(1.0, r));
		double bw = f.fNow / s.q;
		f.bw1 = bw;
		f.r1.set(f.fNow, bw, dt);
		f.r2.set(std::min(f.fNow * s.f2ratio, 0.45 * sr), std::max(f.fNow * s.f2ratio / s.q, 30.0), dt);
		double sacF = std::max(60.0, 2.0 * f0);
		f.r3.set(std::min(sacF, 0.45 * sr), sacF / 4.0, dt);
		f.buzzNorm = f0 > 0.0 ? (1.0 - std::exp(-PI * bw / f0)) : 1.0;
		// position: SPACE widens the pond and pushes it away
		double w = 0.25 + 0.75 * p.space;
		double pan = std::max(-1.0, std::min(1.0, f.x * w));
		double a = (pan + 1.0) * PI / 4.0;
		double dist = f.y * p.space;
		double g = 1.0 / (1.0 + 2.2 * dist);
		f.gL = std::cos(a) * g; f.gR = std::sin(a) * g;
		f.air.set(16000.0 * (1.0 - 0.85 * dist) + 1500.0, sr);
	}

	// ---- audio rate
	// voct: per-frog V/OCT (n values). Returns stereo.
	void process(const Params& p, const double* voct, bool hushHeld, double& outL, double& outR) {
		int want = std::max(1, std::min(MAX_FROGS, p.frogs));
		if (want != n) {
			for (int i = n; i < want; i++) { frogs[i].calling = false; frogs[i].level = 0; }
			n = want;
		}
		held = hushHeld;
		if (hushHeld) {
			for (int i = 0; i < n; i++) {
				frogs[i].fear = 1.0; frogs[i].answerT = -1.0; frogs[i].armed = false;
				if (frogs[i].calling) frogs[i].killing = true;
			}
		}
		if (counter % CTL == 0) control(p, voct);
		counter++;
		// clocked starts
		if (tick && held) tick = false;
		if (tick) {
			for (int i = 0; i < n; i++) {
				Frog& f = frogs[i];
				if (!f.armed || f.calling) continue;
				double h = 0.0;
				for (int j = 0; j < n; j++) if (j != i && frogs[j].calling) h += std::max(frogs[j].level, 0.6) * prox(frogs[j], f);
				if (p.talk > 0.0 && h > 0.12 && f.rng.uni() < p.talk) { f.armed = false; f.answerT = 0.1 + 0.6 * f.rng.uni(); continue; }
				startCall(i, p);
			}
			tick = false;
		}

		double L = 0.0, R = 0.0;
		for (int i = 0; i < n; i++) {
			Frog& f = frogs[i];
			if (!f.calling) { f.level *= 0.999; f.levelOut = 0; continue; }
			double y = voice(f, p);
			f.level = f.lastNoteEnv * f.callGain;
			double a = f.air.process(y);
			L += a * f.gL; R += a * f.gR;
		}
		double norm = 1.0 / std::sqrt(std::max(1.0, n * 0.5));
		L *= norm; R *= norm;
		// night air
		double wet = p.space * p.space * 0.55;
		double vl, vr;
		airVerb->process(L * 0.12, R * 0.12, vl, vr);
		vl = verbLp[0].process(vl); vr = verbLp[1].process(vr);
		L += wet * vl; R += wet * vr;
		double m = std::fabs(L + R) * 0.5;
		env += (m - env) * (m > env ? envAtt : envRel);
		outL = L; outR = R;
	}

	// one frog, one sample
	double voice(Frog& f, const Params& p) {
		const Species& s = sp(f);
		f.t += dt;
		double out = 0.0;
		if (f.inNote) {
			// note envelope
			double att = std::max(0.002, s.attack), rel = std::max(0.004, s.release);
			if (rel > 0.8 * f.noteLen) rel = 0.8 * f.noteLen;
			if (att > 0.5 * f.noteLen) att = 0.5 * f.noteLen;
			double e = std::min(1.0, f.t / att) * std::min(1.0, std::max(0.0, (f.noteLen - f.t) / rel));
			e = e * e * (3.0 - 2.0 * e);
			// pulses
			double pe = 1.0;
			if (f.prNow > 0.0) {
				f.pulsePh += f.prNow * dt;
				if (f.pulsePh >= 1.0) {
					f.pulsePh -= std::floor(f.pulsePh);
					f.pulseAmp = std::max(0.2, 1.0 + 0.35 * f.raspNow * f.rng.gauss());
				}
				double xx = f.pulsePh / std::max(0.05, s.duty);
				if (xx < 1.0) { double yy = std::sin(PI * std::pow(xx, 0.65)); pe = yy * yy * f.pulseAmp; }
				else pe = 0.0;
			}
			double amp = e * pe;
			f.lastNoteEnv = e;
			// whistle
			double wob = 1.0 + 0.012 * f.raspNow * f.rng.gauss();
			f.oscPh += f.fNow * dt * wob; f.oscPh -= std::floor(f.oscPh);
			f.osc2Ph += f.fNow * s.f2ratio * dt * wob; f.osc2Ph -= std::floor(f.osc2Ph);
			double tone = std::sin(2 * PI * f.oscPh);
			if (f.fNow * s.f2ratio < 0.45 * sr) tone += s.f2gain * std::sin(2 * PI * f.osc2Ph);
			out += s.tonal * tone * amp;
			// glottal buzz rings the resonators
			if (s.f0 > 0.0 && s.tonal < 1.0) {
				f.buzzPh += f.f0Now * dt;
				if (f.buzzPh >= 1.0) {
					f.buzzPh -= 1.0;
					double late = f.buzzPh / (f.f0Now * dt);
					// period jitter for roughness
					f.buzzPh -= 0.25 * f.raspNow * f.rng.gauss() * 0.3;
					double a = amp * std::max(0.0, 1.0 + 0.3 * f.raspNow * f.rng.gauss());
					f.r1.inject(a, late); f.r2.inject(a, late); f.r3.inject(a, late);
				}
				// breath
				if (f.raspNow > 0.0) f.r1.inject(amp * f.raspNow * 0.10 * f.rng.gauss() * std::sqrt(f.bw1 * dt * 40.0), 0.0);
			}
		}
		else f.lastNoteEnv = 0.0;
		double b1 = f.r1.tick(), b2 = f.r2.tick(), b3 = f.r3.tick();
		if (s.f0 > 0.0 && s.tonal < 1.0)
			out += (1.0 - s.tonal) * f.buzzNorm * 2.2 * (b1 + s.f2gain * b2 + s.sac * 0.6 * b3);

		// scheduler: note -> gap -> next note -> end
		if (f.inNote && f.t >= f.noteLen) {
			f.inNote = false; f.t = 0.0;
			if (f.note + 1 >= f.notes) f.t = -0.08;     // let the resonators ring out
		}
		else if (!f.inNote) {
			if (f.note + 1 >= f.notes) {
				if (f.t >= 0.0) endCall((int)(&f - frogs), p);
			}
			else if (f.t >= f.gapLen) {
				f.note++; f.t = 0.0; f.inNote = true; f.pulsePh = 0.999;
			}
		}
		if (f.killing) {
			f.killEnv -= dt / 0.04;
			if (f.killEnv <= 0.0) { f.killEnv = 0.0; f.killing = false; f.r1.clear(); f.r2.clear(); f.r3.clear(); endCallSilently(f); }
		}
		return out * s.gain * f.callGain * f.killEnv;
	}

	void endCallSilently(Frog& f) {
		f.calling = false; f.inNote = false; f.phase = 0.0; f.level = 0.0; f.lastNoteEnv = 0.0;
	}
};

static const double OUT_GAIN = 5.0;
inline double softClip(double x) { return 10.0 * std::tanh(x / 10.0); }

} // namespace pond
