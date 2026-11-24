// Runs the reference scene scripts (tools/proto/scenes.h) against the C++ engine.
// The scenes set the reference's global control variables once per block; render() copies them into
// the engine. The scene file is included unchanged.
#pragma once
#include "../src/dsp/SheetEngine.hpp"
#include <vector>
#include <string>
#include <functional>

namespace refscene {
static double FSR = 96000.0;
#define FS (refscene::FSR)
#define T (1.0 / refscene::FSR)
#define BLK 32
static uint64_t rs = 0;
static double rnd() { rs ^= rs << 13; rs ^= rs >> 7; rs ^= rs << 17; return (rs >> 11) * (1.0 / 9007199254740992.0); }
static double bi() { return 2 * rnd() - 1; }
static double cTune = 1, cAspect = 5, cBendHand = 0, cShape = 0, cGrip = 0.5, cSnap = 0, cHold = 0.5, cDip = 0, cTaper = 0, cAge = 0, cNL = 1;
static double cBowP = 0, cBowV = 1, cBowX = 0.5, cStrikeX = 0.37, cHard = 0.5, cWB = 1.0, cBack = 1, cKick = 1, cShake = 0, cDepth = 1, cSpot = 0, cCrack = 1, cPick = 0, cOut = 0, cIn = 0;
static double strikeAt = -1, strikeV = 0;
static double lerp(double a, double b, double x) { x = fmax(0, fmin(1, x)); return a + (b - a) * x; }
static double xlerp(double a, double b, double x) { x = fmax(0, fmin(1, x)); return a * pow(b / a, x); }
static double seg(double t, double a, double b) { return fmax(0, fmin(1, (t - a) / (b - a))); }
static void base() { cTune = 1; cAspect = 5; cBendHand = 0; cShape = 0; cGrip = 0.5; cSnap = 0; cHold = 0.5; cDip = 0; cTaper = 0; cAge = 0; cNL = 1; cBowP = 0; cBowV = 1; cBowX = 0.5; cStrikeX = 0.37; cHard = 0.5; cWB = 1; cBack = 1; cKick = 1; cShake = 0; cDepth = 1; cSpot = 0; cCrack = 1; cPick = 0; cOut = 0; cIn = 0; }
typedef void (*Ctl)(double t);

// result of the last render (engine-rate stereo, raw, like the reference's raw-*.f32)
static std::vector<float> lastOut;
static long lastGuards = 0, lastLimits = 0, lastClamps = 0;
static double lastSeconds = 0;
static bent::Engine* eng = nullptr;

static void copyControls(bent::Controls& c, double F1) {
	c.f1 = F1 * cTune; c.weMul = sqrt(cTune); c.aspect = cAspect; c.hand = cBendHand; c.shape = cShape; c.spot = cSpot;
	c.grip = cGrip; c.snap = cSnap; c.hold = cHold; c.crack = cCrack; c.dip = cDip; c.depth = cDepth; c.taper = cTaper; c.age = cAge;
	c.nl = cNL; c.bowP = cBowP; c.bowV = cBowV; c.bowX = cBowX; c.hard = cHard; c.wb = cWB; c.back = cBack; c.kick = cKick;
	c.shake = cShake; c.pick = cPick; c.out = cOut; c.decay = 1;
}
static void render(double dur, Ctl ctl, const char* name, double f1) {
	(void)name;
	bent::Engine* e = eng ? eng : new bent::Engine();
	e->setRate(FSR); e->build(); rs = e->rngAfterBuild;
	long n = (long)(dur * FSR);
	lastOut.assign(2 * n, 0.f);
	for (long i = 0; i < n; i++) {
		double t = i * T;
		if (i % BLK == 0) { ctl(t); copyControls(e->c, f1); e->block(); }
		if (strikeAt >= 0 && t >= strikeAt) { e->strike(strikeV, cStrikeX); strikeAt = -1; }
		e->inForce = cIn; e->inPos = cStrikeX;
		e->process();
		lastOut[2 * i] = (float)e->outL; lastOut[2 * i + 1] = (float)e->outR;
	}
	lastGuards = e->guards; lastLimits = e->limits; lastClamps = e->clamps; lastSeconds = dur;
	if (!eng) delete e;
}
#define main scenes_main
#include "../tools/proto/scenes.h"
#undef main
#undef FS
#undef T
#undef BLK
}
