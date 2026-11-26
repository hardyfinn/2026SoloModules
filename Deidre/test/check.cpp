// Offline checks of the Deidre engine (no Rack needed): make test
#include "../src/DeidreEngine.hpp"
#include <cstdio>
using namespace deidre;

static int fails = 0;
static void expect(bool ok, const char* what) { printf("%s  %s\n", ok ? "ok  " : "FAIL", what); if (!ok) fails++; }

// count rising edges above 2.5 V on output o over `beats` at 120 BPM
static int rises(Engine& e, int o, double beats) {
	Inputs in;
	float sr = 48000.f, dt = 1.f / sr;
	int n = (int) (beats * 0.5 * sr), c = 0;
	bool hi = false;
	for (int i = 0; i < n; i++) {
		e.process(dt, in);
		bool h = e.st[o].volts > 2.5f;
		if (h && !hi) c++;
		hi = h;
	}
	return c;
}

int main() {
	{ Engine e; expect(rises(e, 0, 16) == 16, "x1 pulse: 16 beats -> 16 pulses"); }
	{ Engine e; e.cfg[0].v[P_RATE] = 22; expect(rises(e, 0, 4) == 16, "x4: 4 beats -> 16 pulses"); }
	{ Engine e; e.cfg[0].v[P_RATE] = 14; expect(rises(e, 0, 16) == 4, "/4: 16 beats -> 4 pulses"); }
	{ Engine e; e.cfg[0].v[P_STEPS] = 8; e.cfg[0].v[P_TRIGS] = 3; expect(rises(e, 0, 16) == 6, "euclid 3/8: 16 beats -> 6"); }
	{ Engine e; e.cfg[0].v[P_STEPS] = 8; e.cfg[0].v[P_TRIGS] = 3; e.cfg[0].v[P_PAD] = 8; expect(rises(e, 0, 32) == 6, "euclid 3/8 + pad 8: 32 beats -> 6"); }
	{ Engine e; e.cfg[0].v[P_LOOP] = 4; e.cfg[0].v[P_NAP] = 1; e.cfg[0].v[P_WAKE] = 1; expect(rises(e, 0, 16) == 8, "loop 4, wake 1 nap 1: half the pulses"); }
	{ Engine e; e.cfg[0].v[P_PROB] = 0.5f; int c = rises(e, 0, 400); expect(c > 150 && c < 250, "prob 50%: about half"); }
	{ Engine e; e.cfg[0].v[P_SHAPE] = SH_RATCHET4; expect(rises(e, 0, 8) == 32, "ratchet x4 at 50% width: 4 per step"); }
	{ Engine e; e.cfg[0].v[P_RATE] = 2; expect(rises(e, 0, 8) == 1, "START: one pulse"); }
	{ Engine e; e.cfg[1].v[P_RATE] = 20; e.cfg[1].v[P_CROSS] = CR_AND; e.cfg[1].v[P_SOURCE] = 0;
	  expect(rises(e, 1, 8) == 8, "x2 AND x1: 8 per 8 beats"); }
	{ Engine e; e.cfg[0].v[P_SHAPE] = SH_RANDOM; e.cfg[0].v[P_LOOP] = 2;
	  Inputs in; float dt = 1.f / 48000.f; float a[4], b[4];
	  for (int k = 0; k < 4; k++) { for (int i = 0; i < 24000; i++) e.process(dt, in); a[k] = e.st[0].volts; }
	  for (int k = 0; k < 4; k++) { for (int i = 0; i < 24000; i++) e.process(dt, in); b[k] = e.st[0].volts; }
	  bool same = true; for (int k = 0; k < 4; k++) same &= std::fabs(a[k] - b[k]) < 1e-4f;
	  expect(same, "random with LOOP 2 repeats every 2 beats"); }
	{ expect(std::fabs(quantize(0.30f, scaleMask(2)) - 4 / 12.f) < 1e-5f, "quantise 0.30 V to C major -> E (0.333 V)"); }
	{ Engine e; Inputs in; in.clockConnected = true; float dt = 1.f / 48000.f;
	  // external 24 ppqn at 150 BPM: one edge every 1/60 s
	  int per = 48000 / 60; int c = 0; bool hi = false;
	  for (int i = 0; i < 48000 * 4; i++) { in.clockEdge = (i % per) == 0; e.process(dt, in);
	    bool h = e.st[0].volts > 2.5f; if (h && !hi) c++; hi = h; }
	  expect(std::fabs(e.tempo() - 150.0) < 0.5, "external 24 ppqn clock measured as 150 BPM");
	  expect(c >= 9 && c <= 10, "x1 follows external clock: ~10 pulses in 4 s"); }
	{ Engine e; e.cfg[0].v[P_FLEX] = FX_SWING; e.cfg[0].v[P_AMOUNT] = 1.f; expect(rises(e, 0, 16) == 16, "swing keeps the pulse count"); }
	{ Engine e; e.cfg[0].v[P_FLEX] = FX_RAMP_UP; e.cfg[0].v[P_AMOUNT] = 1.f; int c = rises(e, 0, 32); expect(c >= 31 && c <= 33, "ramp up keeps the count over its window"); }
	printf(fails ? "%d FAILED\n" : "all passed\n", fails);
	return fails ? 1 : 0;
}
