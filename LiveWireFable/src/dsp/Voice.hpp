// The whole module chain without Rack: panel values + jacks -> engine controls -> engine -> output volts.
// The Rack module and the offline tests both use this, so the tests check what the panel does.
#pragma once
#include "LiveWireEngine.hpp"

namespace livewire {

// ParamIds order (presets and tools/make_presets.py use the same order)
enum ParamId {
	P_VOLTS, P_MAINS, P_FREQ, P_CURRENT, P_CHOKE, P_GAP, P_HEAT, P_COOL, P_QUENCH, P_MEMORY, P_RESTRIKE, P_JITTER, P_DRAW,
	P_WIND, P_CLIMB, P_TUNE, P_RING, P_LOAD, P_IRON, P_CORONA, P_HUM, P_FB, P_DIST, P_AGE, P_IN, P_ARC, P_FIELD, P_OUT,
	P_TRACK, P_VTARGET, P_FIRE, P_TOUCH, P_SURGE, NUM_P
};
// CV inputs, in jack order: 10 V sweeps the whole knob
enum Cv { CV_VOLTS, CV_CURRENT, CV_GAP, CV_HEAT, CV_COOL, CV_WIND, CV_CLIMB, CV_RING, CV_LOAD, CV_IRON, CV_CORONA, CV_FB, CV_MAINS, CV_AGE, NUM_CV };
static const int CV_PARAM[NUM_CV] = {P_VOLTS, P_CURRENT, P_GAP, P_HEAT, P_COOL, P_WIND, P_CLIMB, P_RING, P_LOAD, P_IRON, P_CORONA, P_FB, P_MAINS, P_AGE};

struct Jacks {
	double cv[NUM_CV] = {};
	double voct = 0;
	double trig = 0, key = 0, touch = 0, surge = 0, audio = 0;   // volts
	bool keyPatched = false;
};

struct Edge {   // Schmitt trigger, 1 V on, 0.1 V off
	bool high = false;
	bool step(double v) { if (high) { if (v <= 0.1) high = false; return false; } if (v >= 1.0) { high = true; return true; } return false; }
};

static inline double clamp01(double x) { return x < 0 ? 0 : (x > 1 ? 1 : x); }

static const double OUT_VOLTS = 5.0;

struct Voice {
	Engine e;
	K k;
	Edge trigE, surgeE, fireE, surgeBtnE;
	unsigned long long seed = 1;
	double L = 0, R = 0, arc = 0, field = 0;   // volts
	void init(double rate) { e.init(rate, seed); trigE = surgeE = fireE = surgeBtnE = Edge(); }

	void process(const float* p, const Jacks& j) {
		static const int KNOB_PARAM[NKNOB] = {P_VOLTS, P_MAINS, P_FREQ, P_CURRENT, P_CHOKE, P_GAP, P_HEAT, P_COOL, P_QUENCH, P_MEMORY,
			P_RESTRIKE, P_JITTER, P_WIND, P_CLIMB, P_TUNE, P_RING, P_LOAD, P_IRON, P_CORONA, P_HUM, P_FB, P_DIST, P_AGE, P_ARC,
			P_FIELD, P_OUT, P_DRAW, P_IN};   // same order as KNOBS[] in the engine
		double v[NUM_P];
		for (int i = 0; i < NUM_P; i++) v[i] = p[i];
		for (int c = 0; c < NUM_CV; c++) v[CV_PARAM[c]] += 0.1 * j.cv[c];
		for (int i = 0; i < NKNOB; i++) k.*KNOBS[i] = clamp01(v[KNOB_PARAM[i]]);
		k.track = p[P_TRACK] > 0.5f;
		int target = (int)std::lround(p[P_VTARGET]);   // 0 TANK, 1 BOTH, 2 MAINS
		k.voct = target <= 1 ? j.voct : 0.0;
		k.voctMains = target >= 1 ? j.voct : 0.0;
		bool t1 = trigE.step(j.trig), t2 = fireE.step(p[P_FIRE] > 0.5f ? 10.0 : 0.0);
		k.trig = t1 || t2;
		bool s1 = surgeE.step(j.surge), s2 = surgeBtnE.step(p[P_SURGE] > 0.5f ? 10.0 : 0.0);
		k.surge = s1 || s2;
		k.keyPatched = j.keyPatched;
		k.key = j.key >= 1.0;
		k.touch = j.touch >= 1.0 || p[P_TOUCH] > 0.5f;
		double l, r, a, f;
		e.process(k, j.audio / 5.0, &l, &r, &a, &f);
		L = OUT_VOLTS * l; R = OUT_VOLTS * r; arc = OUT_VOLTS * a; field = OUT_VOLTS * f;
	}
};

}  // namespace livewire
