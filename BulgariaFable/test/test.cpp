// Offline checks for the Bulgaria engine (docs/BUILD-INSTRUCTIONS.md section 8).  make test
#include "../src/dsp/BulgariaEngine.hpp"
#include <cstdio>
#include <cmath>
#include <vector>
#include <string>
#include <chrono>

using namespace bulgaria;

static int fails = 0, passes = 0;
static void check(bool ok, const char* what, double got = NAN, double want = NAN) {
	if (ok) { passes++; return; }
	fails++;
	if (std::isnan(got)) std::printf("FAIL %s\n", what);
	else std::printf("FAIL %s (got %.3f, want %.3f)\n", what, got, want);
}
static bool near(double a, double b, double tol) { return std::fabs(a - b) <= tol; }

// Base settings of the tests: LOOSE 0, no ornaments, root C4, SHOPE, TUNE 1, 3 voices, CENTS 170, RULE.
static Params base() {
	Params p;
	p.loose = 0; p.slide = 0; p.shake = 0; p.whoop = 0;
	p.root = 0; p.oct = 0; p.mode = 0; p.tune = 1; p.voices = 3;
	p.rub = rubCentsInv(170); p.hz = false; p.chanceMode = false; p.resolve = false;
	return p;
}

struct Sim {
	Engine e; Params p; Inputs in; double sr;
	Sim(Params q, double rate = 48000, uint64_t seed = 12345) : p(q), sr(rate) {
		e.init(rate, seed);
		in.gatePatched = true;
	}
	void run(double secs) { int n = (int)std::lround(secs * sr); for (int i = 0; i < n; i++) e.process(p, in); }
	void step() { e.process(p, in); }
	double c(int i) const { return e.v[i].outCents; }
	void hold(int semis) { in.voct = semis / 12.0; in.gate = 10; }
};

static void testBasic() {
	Sim s(base());
	s.hold(3);
	s.run(0.0001);
	double v2start = s.c(1);
	s.run(1.0);
	check(near(s.c(0), 310, 1), "lead at 310 after 1 s", s.c(0), 310);
	check(near(s.c(1), 140, 1), "voice 2 at 140 after 1 s", s.c(1), 140);
	check(near(s.c(2), 0, 1), "drone at 0 after 1 s", s.c(2), 0);
	check(near(v2start, 170, 5), "voice 2 starts on the scale step (170)", v2start, 170);
	Sim t(base()); t.hold(3); t.run(0.6);
	check(near(t.c(1), 140, 3), "voice 2 within 3 cents of 140 by 0.6 s", t.c(1), 140);
}

static void testHz() {
	Params p = base(); p.hz = true; p.rub = rubHzInv(32);
	const int notes[4] = {-9, 0, 7, 15};
	for (int n : notes) {
		Sim s(p); s.hold(n); s.run(1.5);
		double d = std::fabs(centsToHz(s.e.v[0].y) - centsToHz(s.e.v[1].y));
		char b[96]; std::snprintf(b, sizeof b, "HZ rub 32 Hz at input %+d", n);
		check(near(d, 32.0, 0.1), b, d, 32);
	}
}

static void testRootAndCadence() {
	{ Sim s(base()); s.hold(0); s.run(1.0); check(near(s.c(2), -190, 1), "drone dodges to -190 on the root", s.c(2), -190); }
	{ Sim s(base()); s.in.phrase = 10; s.hold(0); s.run(0.5); check(near(s.c(2), 0, 1), "drone stays on 0 at a cadence", s.c(2), 0); }
	{
		Sim s(base()); s.hold(2); s.run(1.0);
		check(near(s.c(0), 170, 1), "lead one step up at 170", s.c(0), 170);
		check(near(s.c(1), 0, 1), "voice 2 at 0 cents", s.c(1), 0);
		// drone push target 7.5 (smoothed: settles there)
		check(near(s.e.v[2].outPush, 7.5, 0.05), "drone push 7.5", s.e.v[2].outPush, 7.5);
	}
	{
		Params p = base(); p.resolve = true;
		Sim s(p); s.hold(3); s.run(0.3);
		s.in.phrase = 10; s.run(0.002 + 1.0 / 48000 + 0.75);
		check(near(s.c(1), s.c(0), 1), "RESOLVE: voice 2 at unison 0.75 s after PHRASE", s.c(1) - s.c(0), 0);
	}
	{
		Sim s(base()); s.hold(3); s.run(0.3);
		s.in.phrase = 10; s.run(1.0);
		check(near(s.c(0) - s.c(1), 102, 1), "TIGHT: voice 2 102 cents under the lead", s.c(0) - s.c(1), 102);
	}
	{
		// PHRASE one sample before a legato note change marks the new note
		Sim s(base()); s.hold(3); s.run(0.4);
		s.in.phrase = 10; s.step(); s.hold(5); s.run(0.01);
		check(s.e.cadence && s.e.cur.L == 3, "PHRASE just before a legato change marks the new note");
		s.in.phrase = 0; s.run(0.2);
		s.hold(7); s.run(0.01);
		check(!s.e.cadence, "the note after is not a cadence");
	}
	{
		// PHRASE with the gate low arms the next note
		Sim s(base()); s.in.gate = 0; s.in.phrase = 10; s.run(0.1);
		s.in.phrase = 0; s.hold(3); s.run(0.05);
		check(s.e.cadence, "PHRASE while the gate is low marks the next note");
	}
}

static void testTuneZero() {
	Params p = base(); p.tune = 0;
	bool ok = true; double worst = 0;
	for (int n = -12; n <= 12; n++) {
		Sim s(p); s.hold(n); s.run(0.2);
		double e = std::fabs(s.c(0) - 100.0 * n);
		worst = std::max(worst, e);
		if (e > 0.01) ok = false;
	}
	check(ok, "TUNE 0 passes every semitone -12..12 to the lead", worst, 0);
}

static void testWhoop() {
	Params p = base(); p.whoop = 1;
	Sim s(p); s.in.phrase = 10; s.hold(3); s.run(0.5);
	double before = s.c(0);
	s.in.gate = 0; s.step();
	check(!s.e.v[2].gate, "drone gate falls at once at the whoop");
	check(s.e.v[0].gate, "lead gate held during the whoop");
	s.run(0.169);
	check(s.e.v[0].gate, "lead gate still high at 169 ms");
	double top = s.c(0) - before;
	check(near(top, 1000, 20), "whoop climbs ~1000 cents", top, 1000);
	s.run(0.003);
	check(!s.e.v[0].gate, "lead gate falls after 170 ms");
	// voice 5 joins at VOICES 5
	Params q = p; q.voices = 5;
	Sim t(q); t.in.phrase = 10; t.hold(3); t.run(0.5); t.in.gate = 0; t.run(0.1);
	check(t.e.v[4].gate && t.e.v[4].whooping, "voice 5 whoops when active");
	// no cadence, no whoop
	Sim u(p); u.hold(3); u.run(0.5); u.in.gate = 0; u.step();
	check(!u.e.v[0].gate, "no whoop without a cadence");
	// a triggered whoop with the gate high mutes the whooping voices until the next note
	Sim w(p); w.hold(3); w.run(0.3); w.in.whoopTrig = 10; w.run(0.2);
	check(!w.e.v[0].gate && w.e.v[0].muted, "triggered whoop mutes the lead until the next note");
	w.hold(5); w.run(0.01);
	check(w.e.v[0].gate, "next note unmutes");
}

static void testVoicesAndSpread() {
	for (int n = 1; n <= 8; n++) {
		Params p = base(); p.voices = n;
		Sim s(p); s.hold(3); s.run(0.3);
		// the drone (voice 3) is always computed
		check(near(s.c(2), 0, 1) && s.e.v[2].gate, "drone values valid at every VOICES count");
	}
	Params p = base(); p.voices = 8;
	{ Sim s(p); s.hold(3); s.run(1.0);
	  check(near(s.c(7), 1200, 1), "voice 8 high drone at SPREAD 0", s.c(7), 1200);
	  check(near(s.c(5), 700, 1), "voice 6 fifth at SPREAD 0", s.c(5), 700);
	  check(near(s.c(3), -1200, 1), "voice 4 low drone at SPREAD 0", s.c(3), -1200); }
	p.spread = 0.3;
	{ Sim s(p); s.hold(3); s.run(1.0); check(near(s.c(7), 310 + 340, 1), "SPREAD > 0.25: voice 8 rubs 2 steps above", s.c(7), 650); check(near(s.c(5), 700, 1), "voice 6 still fifth at 0.3", s.c(5), 700); }
	p.spread = 0.6;
	{ Sim s(p); s.hold(3); s.run(1.0); check(near(s.c(5), 310 - 510, 1), "SPREAD > 0.5: voice 6 rubs 3 steps below", s.c(5), -200); check(near(s.c(3), -1200, 1), "voice 4 still low drone at 0.6", s.c(3), -1200); }
	p.spread = 0.8;
	{ Sim s(p); s.hold(3); s.run(1.0); check(near(s.c(3), 310 + 510, 1), "SPREAD > 0.75: voice 4 rubs 3 steps above", s.c(3), 820); }
}

static void testLoose() {
	Params p = base(); p.loose = 1;
	double maxd = 0; bool ok = true;
	for (int k = 0; k < 40; k++) {
		Sim s(p, 48000, 1000 + k); s.hold(3); s.run(0.0001);
		for (int i = 1; i < NV; i++) {
			double d = -s.e.v[i].tv;   // remaining stagger right after the start
			if (s.e.v[i].pending) { maxd = std::max(maxd, d); if (d < 0 || d > 0.035) ok = false; }
		}
	}
	check(ok && maxd > 0.02, "LOOSE 1 stagger within 0..35 ms", maxd * 1000, 35);
	// drift RMS: hold a note for a long time and measure the drone (voice 3) against its target, lead too
	Params q = base(); q.loose = 1; q.voices = 3;
	Sim s(q, 48000, 77); s.hold(3); s.run(1.0);
	double sd = 0, sl = 0; int n = 0;
	for (int i = 0; i < 48000 * 120; i++) {
		s.step();
		if (i % 64 == 0) { double a = s.c(2) - 0, b = s.c(0) - 310; sd += a * a; sl += b * b; n++; }
	}
	sd = std::sqrt(sd / n); sl = std::sqrt(sl / n);
	check(sd > 6.5 && sd < 11.5, "drift RMS about 9 cents", sd, 9);
	check(sl > 3.0 && sl < 6.0, "lead drift RMS about 4.5 cents", sl, 4.5);
}

static void testChance() {
	Params p = base(); p.chanceMode = true; p.chance = 0.0;
	Sim s(p); s.hold(3); s.run(1.0);
	check(near(s.c(1), 0, 1), "CHANCE 0: voice 2 on a plain third below (degree 1 = 0 from L 3 - 2)", s.c(1), 0);
	p.chance = 1.0;
	Sim t(p); t.hold(3); t.run(1.0);
	check(near(t.c(1), 140, 1), "CHANCE 1: voice 2 rubs", t.c(1), 140);
	// half the notes rub at CHANCE 0.5
	p.chance = 0.5; Sim u(p); int rub = 0;
	for (int k = 0; k < 400; k++) { u.hold(3 + (k % 2) * 2); u.run(0.01); if (u.e.v[1].rubbing) rub++; }
	check(rub > 150 && rub < 250, "CHANCE 0.5 rubs about half the notes", rub, 200);
}

static void testOrnaments() {
	Params p = base(); p.slide = 1;
	Sim s(p); s.hold(3); s.run(0.001);
	check(s.c(0) < 310 - 100, "SLIDE: lead starts well below the note", s.c(0), 150);
	s.run(0.5);
	check(near(s.c(0), 310, 1), "SLIDE settles", s.c(0), 310);
	Params q = base(); q.shake = 1;
	Sim t(q); t.hold(3); t.run(0.30 + 0.014);
	check(t.c(0) > 310 + 150, "SHAKE flick reaches up at 0.31 s", t.c(0), 500);
	t.run(0.3);
	check(near(t.c(0), 310, 1), "SHAKE over", t.c(0), 310);
	// trigger inputs
	Sim u(base()); u.hold(3); u.run(0.5); u.in.shakeTrig = 10; u.run(0.014);
	check(u.c(0) > 310 + 150, "SHAKE trigger", u.c(0), 500);
	Sim w(base()); w.hold(3); w.run(0.5); w.in.slideTrig = 10; w.run(0.001);
	check(w.c(0) < 310 - 100, "SLIDE trigger", w.c(0), 150);
}

static void testGateUnpatched() {
	Sim s(base()); s.in.gatePatched = false; s.in.voct = 3 / 12.0; s.run(0.5);
	check(s.e.v[0].gate && near(s.c(0), 310, 1), "gate unpatched: always sounding");
	s.in.voct = 5 / 12.0; s.run(0.5);
	check(near(s.c(0), 500, 1), "gate unpatched: pitch change is a new note", s.c(0), 500);
}

static void testDroneInput() {
	Sim s(base()); s.in.dronePatched = true; s.in.drone = 2 / 12.0; s.hold(5); s.run(0.5);
	check(near(s.c(2), 200, 1), "DRONE input sets the root", s.c(2), 200);
	check(near(s.c(0), 200 + 310, 1), "lead relative to the DRONE root", s.c(0), 510);
}

static void fuzz(double rate, uint64_t seed, double secs) {
	Rng r; r.seed(seed);
	Params p; Sim s(p, rate, seed);
	int n = (int)(secs * rate), every = (int)(0.05 * rate);
	bool bad = false; double maxV = 0;
	for (int i = 0; i < n; i++) {
		if (i % every == 0) {
			p.root = (int)(r.uni() * 12); p.oct = (int)(r.uni() * 5) - 2; p.mode = (int)(r.uni() * 7);
			p.tune = r.uni(); p.voices = 1 + (int)(r.uni() * 8); p.spread = r.uni(); p.rub = r.uni();
			p.hz = r.uni() < 0.5; p.chanceMode = r.uni() < 0.5; p.chance = r.uni(); p.resolve = r.uni() < 0.5;
			p.slide = r.uni(); p.shake = r.uni(); p.whoop = r.uni(); p.loose = r.uni();
			s.p = p;
			s.in.voct = r.bi() * 10; s.in.gate = r.uni() < 0.6 ? 10 : 0; s.in.gatePatched = r.uni() < 0.9;
			s.in.drone = r.bi() * 10; s.in.dronePatched = r.uni() < 0.3;
			s.in.phrase = r.uni() < 0.3 ? 10 : 0;
			s.in.slideTrig = r.uni() < 0.2 ? 10 : 0; s.in.shakeTrig = r.uni() < 0.2 ? 10 : 0; s.in.whoopTrig = r.uni() < 0.2 ? 10 : 0;
		}
		s.step();
		for (int v = 0; v < NV; v++) {
			double c = s.e.v[v].outCents / 1200.0, pu = s.e.pushV(v);
			if (!std::isfinite(c) || !std::isfinite(pu) || std::fabs(c) > 6.0 || pu < 0 || pu > 10) bad = true;
			maxV = std::max(maxV, std::fabs(c));
		}
	}
	char b[96]; std::snprintf(b, sizeof b, "fuzz %.0f Hz seed %llu: finite, pitch within 6 V, push 0..10", rate, (unsigned long long)seed);
	check(!bad, b, maxV, 6);
}

static void cpu() {
	Params p; p.voices = 8; p.loose = 1; p.hz = true;
	Sim s(p); s.hold(3);
	auto t0 = std::chrono::steady_clock::now();
	int n = 48000 * 10;
	for (int i = 0; i < n; i++) { if (i % 6000 == 0) s.hold((i / 6000) % 12); s.step(); }
	double el = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
	std::printf("CPU: %.2f%% of one core at 48 kHz (8 voices, HZ, LOOSE 1)\n", 100.0 * el / 10.0);
}

int main() {
	testBasic(); testHz(); testRootAndCadence(); testTuneZero(); testWhoop(); testVoicesAndSpread();
	testLoose(); testChance(); testOrnaments(); testGateUnpatched(); testDroneInput();
	const double rates[3] = {44100, 48000, 96000};
	for (double r : rates) fuzz(r, 7 + (uint64_t)r, 60.0);
	cpu();
	std::printf("%d passed, %d failed\n", passes, fails);
	return fails ? 1 : 0;
}
