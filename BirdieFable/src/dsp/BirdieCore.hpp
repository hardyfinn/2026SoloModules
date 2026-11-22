// Birdie module logic without Rack: knob mappings, CV, smoothing, triggers, seeds,
// clock lock, AUTO, HOLD and the outputs. src/Birdie.cpp only plumbs Rack into this,
// so test/render.cpp can exercise exactly what runs in Rack.
#pragma once
#include <cmath>
#include <cstdint>
#include <cstring>
#include <algorithm>
#include "birdie_ref.h"

namespace birdie {

enum ParamId {
	SIZE_P, TENSION_P, PRESSURE_P, BREATH_P, ROUGH_P, V2MIX_P, V2INT_P, V2SLIDE_P,
	LENGTH_P, SLIDE_P, SHAPE_P, ATTACK_P, RELEASE_P, BUZZRATE_P, BUZZ_P, CHOP_P,
	TRACHEA_P, TUBE_P, THROAT_P, TUNE_P, FOLLOW_P, BEAK_P, TIME_P, HOLD_P,
	NOTES_P, RATE_P, PATTERN_P, VARY_P, DRIFT_P, ACCEL_P, SWELL_P, GAP_P,
	CLOCK_P, AUTO_P, NEW_P, NOTE_P, PHRASE_P,
	NUM_P
};
enum InputId {
	SIZE_I, TENSION_I, PRESSURE_I, ROUGH_I, V2MIX_I, LENGTH_I, SLIDE_I, BUZZRATE_I, BEAK_I, TIME_I, RATE_I, VARY_I,
	NOTE_I, PHRASE_I, NEW_I, CLOCK_I, HOLD_I,
	NUM_I
};
enum OutputId { END_O, GATE_O, PRESS_O, PITCH_O, OUT_O, NUM_O };

static const char* const SHAPE_NAMES[SH_COUNT] = {"LINE", "FALL", "SWOOP", "ARCH", "DOUBLE"};
static const char* const PATTERN_NAMES[PT_COUNT] = {"REPEAT", "PAIR", "MOTIF", "TAIL", "RUN", "CALL", "SHUFFLE"};
static const double LOCK_RATIOS[9] = {0.25, 1.0 / 3.0, 0.5, 1.0, 2.0, 3.0, 4.0, 6.0, 8.0};
static const char* const LOCK_NAMES[9] = {"1/4", "1/3", "1/2", "1", "2", "3", "4", "6", "8"};

// ---- knob position k (0..1) <-> engine units. Integer params (SHAPE, NOTES, PATTERN) and switches are stored as is.
inline double lin(double k, double lo, double hi) { return lo + (hi - lo) * k; }
inline double unlin(double v, double lo, double hi) { return (v - lo) / (hi - lo); }
inline double ex(double k, double lo, double ratio) { return lo * std::pow(ratio, k); }
inline double unex(double v, double lo, double ratio) { return std::log(v / lo) / std::log(ratio); }

inline double mapSize(double k)     { return ex(k, 0.5, 128.0); }
inline double mapTension(double k)  { return lin(k, -36.0, 7.0); }
inline double mapPressure(double k) { return lin(k, 0.05, 0.6); }
inline double mapV2Int(double k)    { return lin(k, -12.0, 12.0); }
inline double mapV2Slide(double k)  { return lin(k, -1.0, 1.0); }
inline double mapLength(double k)   { return ex(k, 0.01, 400.0); }     // 10 ms .. 4 s
inline double mapSlide(double k)    { return lin(k, -24.0, 24.0); }
inline double mapAR(double k)       { return lin(k, 0.02, 0.6); }
inline double mapBuzzRate(double k) { return ex(k, 5.0, 80.0); }
inline double mapBuzz(double k)     { return 0.5 * k; }
inline double mapTrachea(double k)  { return ex(k, 0.005, 20.0); }
inline double mapTube(double k)     { return 0.9 * k; }
inline double mapBeak(double k)     { return ex(k, 500.0, 32.0); }
inline double mapTime(double k)     { return ex(k, 1.0, 16.0); }
inline double mapRate(double k)     { return ex(k, 0.25, 160.0); }     // 0.25 .. 40 notes per second
inline double mapDrift(double k)    { return lin(k, -12.0, 12.0); }
inline double mapAccel(double k)    { return lin(k, -1.5, 1.5); }
inline double mapSwell(double k)    { return lin(k, -1.0, 1.0); }
inline double mapGap(double k)      { return ex(k, 0.1, 80.0); }
inline int lockZone(double k)       { return std::min(8, std::max(0, (int)std::floor(k * 9.0))); }

// Engine units -> Rack param values (what presets store).
inline void paramsToKnobs(const BdParams& p, float* kv) {
	auto c = [](double v) { return (float)std::min(1.0, std::max(0.0, v)); };
	kv[SIZE_P] = c(unex(p.size, 0.5, 128.0));
	kv[TENSION_P] = c(unlin(p.tension, -36.0, 7.0));
	kv[PRESSURE_P] = c(unlin(p.pressure, 0.05, 0.6));
	kv[BREATH_P] = c(p.breath);
	kv[ROUGH_P] = c(p.rough);
	kv[V2MIX_P] = c(p.v2mix);
	kv[V2INT_P] = c(unlin(p.v2int, -12.0, 12.0));
	kv[V2SLIDE_P] = c(unlin(p.v2slide, -1.0, 1.0));
	kv[LENGTH_P] = c(unex(p.length, 0.01, 400.0));
	kv[SLIDE_P] = c(unlin(p.slide, -24.0, 24.0));
	kv[SHAPE_P] = (float)p.shape;
	kv[ATTACK_P] = c(unlin(p.attack, 0.02, 0.6));
	kv[RELEASE_P] = c(unlin(p.release, 0.02, 0.6));
	kv[BUZZRATE_P] = c(unex(p.buzzRate, 5.0, 80.0));
	kv[BUZZ_P] = c(p.buzz / 0.5);
	kv[CHOP_P] = c(p.chop);
	kv[TRACHEA_P] = c(unex(p.trachea, 0.005, 20.0));
	kv[TUBE_P] = c(p.tube / 0.9);
	kv[THROAT_P] = c(p.throat);
	kv[TUNE_P] = c(p.tune);
	kv[FOLLOW_P] = p.follow ? 1.f : 0.f;
	kv[BEAK_P] = c(unex(p.beak, 500.0, 32.0));
	kv[TIME_P] = c(unex(p.time, 1.0, 16.0));
	kv[HOLD_P] = p.hold ? 1.f : 0.f;
	kv[NOTES_P] = (float)p.notes;
	kv[RATE_P] = c(unex(p.rate, 0.25, 160.0));
	kv[PATTERN_P] = (float)p.pattern;
	kv[VARY_P] = c(p.vary);
	kv[DRIFT_P] = c(unlin(p.drift, -12.0, 12.0));
	kv[ACCEL_P] = c(unlin(p.accel, -1.5, 1.5));
	kv[SWELL_P] = c(unlin(p.swell, -1.0, 1.0));
	kv[GAP_P] = c(unex(1.0, 0.1, 80.0));
	kv[CLOCK_P] = 0.f;
	kv[AUTO_P] = 0.f;
	kv[NEW_P] = kv[NOTE_P] = kv[PHRASE_P] = 0.f;
	if (p.clockPeriod > 0) {
		// Locked presets: RATE sits where FREE gives the same note rate, and LOCK gives
		// the nearest notes-per-clock for a quarter-note clock at that tempo.
		kv[RATE_P] = c(unex(1.0 / p.clockPeriod, 0.25, 160.0));
		kv[CLOCK_P] = 1.f;
	}
}

struct Trig {   // Schmitt trigger, 0.1 V / 1 V like Rack's
	bool high = false;
	bool process(float v) {
		if (high) { if (v <= 0.1f) high = false; return false; }
		if (v >= 1.0f) { high = true; return true; }
		return false;
	}
};

struct Core {
	Birdie eng;
	BdParams P;
	double sr = 48000.0, T = 1.0 / 48000.0;
	uint32_t seed = 1;
	uint32_t seedRng = 0x2545F491u;
	bool newEachPhrase = false;

	Trig noteT, phraseT, newT, clockT, noteB, phraseB, newB;
	double lastClockT = -1.0, clockPeriod = 0.0, now = 0.0;
	bool autoWas = false;
	double autoWait = 0.0;
	int endSamples = 0;
	double pitchHeld = 0.0;
	bool smoothInit = false;
	double sLogSize = 0, sTension = 0, sPressure = 0, sLogBeak = 0, sTune = 0;
	int idleCount = 0;
	float out[NUM_O] = {};
	// display
	int dispPattern = PT_MOTIF, dispIdx = 0, dispN = 0;
	bool dispPlaying = false;

	Core() { setSampleRate(48000.0); }

	void setSampleRate(double rate) {
		sr = rate; T = 1.0 / rate;
		bd_init(&eng, rate);
		smoothInit = false;
		lastClockT = -1.0; clockPeriod = 0.0;
	}

	uint32_t freshSeed() {
		uint32_t x = seedRng; x ^= x << 13; x ^= x >> 17; x ^= x << 5; seedRng = x ? x : 1u;
		return seedRng;
	}

	static double cv(const float* in, const bool* conn, int i) { return conn[i] ? (double)in[i] : 0.0; }
	static double clampd(double v, double lo, double hi) { return std::min(hi, std::max(lo, v)); }

	// Knob + CV -> engine params. Returns the slot length in LOCK (0 when free).
	void buildParams(const float* kv, const float* in, const bool* conn) {
		BdParams& p = P;
		double a = 1.0 - std::exp(-T / 0.005);
		double tSize = std::log(clampd(mapSize(kv[SIZE_P]) * std::pow(2.0, -cv(in, conn, SIZE_I)), 0.5, 64.0));
		double tTension = clampd(mapTension(kv[TENSION_P]) + 12.0 * cv(in, conn, TENSION_I), -36.0, 7.0);
		double tPressure = clampd(mapPressure(kv[PRESSURE_P]) + 0.055 * cv(in, conn, PRESSURE_I), 0.05, 0.6);
		double tBeak = std::log(clampd(mapBeak(kv[BEAK_P]) * std::pow(2.0, cv(in, conn, BEAK_I)), 500.0, 16000.0));
		double tTune = kv[TUNE_P];
		if (!smoothInit) { sLogSize = tSize; sTension = tTension; sPressure = tPressure; sLogBeak = tBeak; sTune = tTune; smoothInit = true; }
		sLogSize += a * (tSize - sLogSize); sTension += a * (tTension - sTension); sPressure += a * (tPressure - sPressure);
		sLogBeak += a * (tBeak - sLogBeak); sTune += a * (tTune - sTune);
		p.size = std::exp(sLogSize); p.tension = sTension; p.pressure = sPressure; p.beak = std::exp(sLogBeak); p.tune = sTune;
		p.breath = kv[BREATH_P];
		p.rough = clampd(kv[ROUGH_P] + 0.1 * cv(in, conn, ROUGH_I), 0.0, 1.0);
		p.v2mix = clampd(kv[V2MIX_P] + 0.1 * cv(in, conn, V2MIX_I), 0.0, 1.0);
		p.v2int = mapV2Int(kv[V2INT_P]);
		p.v2slide = mapV2Slide(kv[V2SLIDE_P]);
		p.length = clampd(mapLength(kv[LENGTH_P]) * std::pow(2.0, cv(in, conn, LENGTH_I)), 0.01, 4.0);
		p.slide = clampd(mapSlide(kv[SLIDE_P]) + 4.8 * cv(in, conn, SLIDE_I), -24.0, 24.0);
		p.shape = std::min(SH_COUNT - 1, std::max(0, (int)std::lround(kv[SHAPE_P])));
		p.attack = mapAR(kv[ATTACK_P]);
		p.release = mapAR(kv[RELEASE_P]);
		p.buzzRate = clampd(mapBuzzRate(kv[BUZZRATE_P]) * std::pow(2.0, cv(in, conn, BUZZRATE_I)), 5.0, 400.0);
		p.buzz = mapBuzz(kv[BUZZ_P]);
		p.chop = kv[CHOP_P];
		p.trachea = mapTrachea(kv[TRACHEA_P]);
		p.tube = mapTube(kv[TUBE_P]);
		p.throat = kv[THROAT_P];
		p.follow = kv[FOLLOW_P] > 0.5f;
		p.time = clampd(mapTime(kv[TIME_P]) * std::pow(2.0, cv(in, conn, TIME_I)), 1.0, 16.0);
		p.hold = (kv[HOLD_P] > 0.5f) || (conn[HOLD_I] && in[HOLD_I] >= 1.0f);
		p.notes = std::min(32, std::max(1, (int)std::lround(kv[NOTES_P])));
		p.pattern = std::min(PT_COUNT - 1, std::max(0, (int)std::lround(kv[PATTERN_P])));
		p.vary = clampd(kv[VARY_P] + 0.1 * cv(in, conn, VARY_I), 0.0, 1.0);
		p.drift = mapDrift(kv[DRIFT_P]);
		p.accel = mapAccel(kv[ACCEL_P]);
		p.swell = mapSwell(kv[SWELL_P]);
		bool lock = kv[CLOCK_P] > 0.5f;
		if (lock) {
			p.rate = mapRate(kv[RATE_P]);
			double kz = kv[RATE_P] + cv(in, conn, RATE_I) / 9.0;     // one zone per volt
			p.clockPeriod = clockPeriod > 0.0 ? clockPeriod / LOCK_RATIOS[lockZone(kz)] : 0.0;
		}
		else {
			p.rate = clampd(mapRate(kv[RATE_P]) * std::pow(2.0, cv(in, conn, RATE_I)), 0.25, 40.0);
			p.clockPeriod = 0.0;
		}
	}

	void startPhrase() {
		if (newEachPhrase) seed = freshSeed();
		bd_phrase(&eng, &P, seed);
	}

	// One sample. kv: Rack param values. in/conn: input voltages and whether patched.
	float process(const float* kv, const float* in, const bool* conn) {
		now += T;
		// clock period (measured even in FREE, so switching to LOCK is instant)
		if (conn[CLOCK_I] && clockT.process(in[CLOCK_I])) {
			if (lastClockT >= 0.0) clockPeriod = now - lastClockT;
			lastClockT = now;
		}
		if (!conn[CLOCK_I]) { lastClockT = -1.0; clockPeriod = 0.0; }
		buildParams(kv, in, conn);

		bool newHit = newB.process(kv[NEW_P] * 10.f) | (conn[NEW_I] && newT.process(in[NEW_I]));
		if (newHit) seed = freshSeed();
		bool noteHit = noteB.process(kv[NOTE_P] * 10.f) | (conn[NOTE_I] && noteT.process(in[NOTE_I]));
		bool phraseHit = phraseB.process(kv[PHRASE_P] * 10.f) | (conn[PHRASE_I] && phraseT.process(in[PHRASE_I]));
		double gap = mapGap(kv[GAP_P]) * P.time;
		bool autoOn = kv[AUTO_P] > 0.5f;
		if (autoOn && !autoWas) autoWait = 0.0;
		autoWas = autoOn;

		if (phraseHit) { startPhrase(); autoWait = gap; }
		else if (noteHit) { bd_note(&eng, &P); autoWait = gap; }
		else if (autoOn && !eng.phOn && !eng.noteOn) {
			autoWait -= T;
			if (autoWait <= 0.0) { startPhrase(); autoWait = gap; }
		}

		double y = bd_process(&eng, &P);

		if (eng.outEnd) endSamples = (int)std::ceil(0.001 * sr);
		out[END_O] = endSamples > 0 ? 10.f : 0.f;
		if (endSamples > 0) endSamples--;
		out[GATE_O] = eng.outGate ? 10.f : 0.f;
		out[PRESS_O] = (float)eng.outPress;
		if (eng.outPress > 0.1) pitchHeld = eng.outPitch;
		out[PITCH_O] = (float)pitchHeld;
		out[OUT_O] = (float)y;

		// keep long silences free of denormals (the engine file stays identical to the reference)
		if (eng.envSm < 1e-9 && !eng.noteOn && !P.hold) {
			if (++idleCount > (int)sr) { flushTiny(); idleCount = 0; }
		}
		else idleCount = 0;

		dispPlaying = eng.phOn != 0;
		dispPattern = eng.phOn ? eng.phPattern : P.pattern;
		dispIdx = eng.phIdx; dispN = eng.phN;
		return out[OUT_O];
	}

	void flushTiny() {
		auto f = [](double& v) { if (std::fabs(v) < 1e-20) v = 0.0; };
		f(eng.ic1); f(eng.ic2); f(eng.lpA); f(eng.lpB); f(eng.dcX); f(eng.dcY); f(eng.envSm); f(eng.holdSm);
		for (int i = 0; i < BD_DELAY; i++) f(eng.dl[i]);
		for (int i = 0; i < eng.ntaps; i++) f(eng.hist[i]);
	}
};

}  // namespace birdie
