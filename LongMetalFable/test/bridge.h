// Runs the reference scene scripts (tools/proto/scenes.h) against the C++ engine, one engine step per sample
// at the nominal 48 kHz, exactly as the reference's main() does. The scene file is included unchanged; its
// globals are mapped onto the engine with macros.
#pragma once
#include "../src/dsp/LineEngine.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <vector>

namespace refscene {
static longmetal::Engine* E = nullptr;
#define SR 48000.0
#define NMAX longmetal::NMAX
#define p (E->p)
#define trainPos (E->trainPos)
#define trainSpeed (E->trainSpeed)
#define trainSide (E->trainSide)
#define roughLevel (E->roughLevel)
#define flatLevel (E->flatLevel)
#define jointLevel (E->jointLevel)
#define roughBright (E->roughBright)
#define wheelHz (E->wheelHz)
#define roughDens (E->roughDens)
#define joints (E->joints)
#define njoint (E->njoint)
#define nax (E->nax)
static double urand() { return E->urand(); }
static double nrand() { return E->nrand(); }
static void train_init(int n) { E->train_init(n); }
static void train_step() { E->train_step(); }
static void strike(double pos, double amp, double ms) { E->strike(pos, amp, ms); }
static void strike_step() { E->strike_step(); }
static void inject(double pos, double v) { E->inject(pos, v); }
static void grip_reset() { E->grip_reset(); }
static double lerp(double a, double b, double x) { if (x < 0) x = 0; if (x > 1) x = 1; return a + (b - a) * x; }
static double sm(double x) { if (x < 0) x = 0; if (x > 1) x = 1; return x * x * (3 - 2 * x); }
#include "../tools/proto/scenes.h"
#undef SR
#undef NMAX
#undef p
#undef trainPos
#undef trainSpeed
#undef trainSide
#undef roughLevel
#undef flatLevel
#undef jointLevel
#undef roughBright
#undef wheelHz
#undef roughDens
#undef joints
#undef njoint
#undef nax

struct Result { std::vector<float> st; long guard = 0; long snaps = 0; double E0 = 0, Emin = 1e300, Emax = 0; double secs = 0; };
// name: rail, zaps, hard, live, live2, lossless, fuzz (seed used for fuzz)
static Result run(const char* name, long seed = 0, double maxDur = 1e9, std::vector<long>* snapsEvery100ms = nullptr) {
	Result r;
	int sc = scene_id(name);
	E = new longmetal::Engine();
	if (sc == SC_FUZZ) E->rs ^= (unsigned long long)seed * 0x9E3779B97F4A7C15ULL;
	fz_next = 0;
	E->line_init();
	double dur = std::min(maxDur, scene_setup(sc));
	long n = (long)(dur * 48000.0);
	r.st.resize(2 * n);
	for (long t = 0; t < n; t++) {
		if ((t & 15) == 0) scene_ctl(sc, t / 48000.0);
		scene_audio(sc, t);
		double l, rr; E->line_step(&l, &rr);
		if (sc == SC_LOSSLESS && t >= 4800 && (t % 480) == 0) { double En = E->total_energy(); if (t == 4800) r.E0 = En; r.Emin = std::min(r.Emin, En); r.Emax = std::max(r.Emax, En); }
		r.st[2 * t] = (float)l; r.st[2 * t + 1] = (float)rr;
		if (snapsEvery100ms && t % 4800 == 0) snapsEvery100ms->push_back(E->nsnap);
	}
	r.guard = E->guard; r.snaps = E->nsnap; r.secs = dur;
	delete E; E = nullptr;
	return r;
}
}  // namespace refscene
