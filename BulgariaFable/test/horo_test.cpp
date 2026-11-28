// Offline checks for the Horo engine (docs/HORO-BUILD-INSTRUCTIONS.md section 8).  make test
#include "../src/dsp/HoroEngine.hpp"
#include <cstdio>
#include <cmath>
#include <vector>
#include <string>
#include <cstdlib>

using namespace horo;

static int fails = 0, passes = 0;
static void check(bool ok, const std::string& what, double got = NAN, double want = NAN) {
	if (ok) { passes++; return; }
	fails++;
	if (std::isnan(got)) std::printf("FAIL %s\n", what.c_str());
	else std::printf("FAIL %s (got %.4f, want %.4f)\n", what.c_str(), got, want);
}
static bool near(double a, double b, double tol) { return std::fabs(a - b) <= tol; }

static Params base(int slot = 2) {
	Params p; p.slot = slot; p.lilt = 0; p.accent = 0.8; p.bars = 4; p.phraseMode = LAST_BEAT;
	return p;
}

// A clock at `rate` pulses per second (1 ms high). Records rises of every output.
struct Sim {
	Engine e; Params p; Inputs in; double sr; long n = 0;
	double period;
	long nextEdge = 0;
	std::vector<long> clk, pulse, beat, lng, bar, phrase;
	std::vector<double> accentAtPulse, beatLen;
	bool pP = false, pB = false, pL = false, pBar = false, pPh = false;
	long beatRise = 0;
	bool clockOn = true;
	Sim(Params q, double rate = 8, double s = 48000) : p(q), sr(s), period(1.0 / rate) { e.init(sr); }
	void step() {
		long since = n - nextEdge;
		if (clockOn && n >= nextEdge) { clk.push_back(n); nextEdge = n + std::lround(period * sr); since = 0; }
		in.clock = (clockOn && since >= 0 && since < (long)(0.001 * sr)) ? 10 : 0;
		e.process(p, in);
		if (e.pulse && !pP) { pulse.push_back(n); accentAtPulse.push_back(e.accentV); }
		if (e.beat && !pB) { beat.push_back(n); beatRise = n; }
		if (!e.beat && pB) beatLen.push_back((n - beatRise) / sr);
		if (e.lng && !pL) lng.push_back(n);
		if (e.barOut() && !pBar) bar.push_back(n);
		if (e.phraseOut() && !pPh) phrase.push_back(n);
		pP = e.pulse; pB = e.beat; pL = e.lng; pBar = e.barOut(); pPh = e.phraseOut();
		n++;
	}
	void clocks(int k) { size_t target = clk.size() + k; while (clk.size() < target) step(); finishPulse(); }
	// run up to just before the next clock edge
	void finishPulse() { while (n < nextEdge) step(); }
	void run(double secs) { long m = std::lround(secs * sr); for (long i = 0; i < m; i++) step(); }
	// pulse index (counted from the first clock) of a sample
	double pulseAt(long s) const { return (s - clk[0]) / (period * sr); }
};

static std::vector<int> idx(const Sim& s, const std::vector<long>& v) {
	std::vector<int> o;
	for (long x : v) o.push_back((int)std::lround(s.pulseAt(x)));
	return o;
}

static void testRuchenitsa() {
	Sim s(base());
	s.clocks(70);
	check(s.pulse.size() == 70, "70 clocks give 70 pulses", s.pulse.size(), 70);
	bool same = s.pulse.size() == 70;
	for (size_t i = 0; same && i < 70; i++) if (s.pulse[i] != s.clk[i]) same = false;
	check(same, "LILT 0: every PULSE rise on the sample of its clock edge");
	std::vector<int> b = idx(s, s.beat), l = idx(s, s.lng), br = idx(s, s.bar);
	bool okb = b.size() == 30, okl = l.size() == 10, okbar = br.size() == 10;
	for (size_t i = 0; okb && i < b.size(); i++) { int r = b[i] % 7; if (r != 0 && r != 2 && r != 4) okb = false; }
	for (int x : l) if (x % 7 != 4) okl = false;
	for (int x : br) if (x % 7 != 0) okbar = false;
	check(okb, "BEAT on pulses 0, 2, 4 of each bar");
	check(okl, "LONG on pulse 4 only");
	check(okbar, "BAR on pulse 0");
	double tol = 2.0 / 48000, P = 1.0 / 8;
	bool okLen = s.beatLen.size() >= 27;
	for (size_t i = 3; okLen && i < 27; i++) {   // bar 0 runs before the period is known (1 ms gates)
		double want = (i % 3 == 2) ? 1.5 * P : 1.0 * P;
		if (!near(s.beatLen[i], want, tol)) { okLen = false; check(false, "BEAT gate length", s.beatLen[i] / P, want / P); }
	}
	check(okLen, "BEAT gates 1 pulse on the 2s, 1.5 on the 3");
}

static void testFactory() {
	const int counts[NUM_FACTORY] = {6, 5, 7, 7, 7, 9, 9, 11, 12, 13, 15, 18, 22, 25};
	for (int i = 0; i < NUM_FACTORY; i++) {
		Metre m = factoryMetre(i);
		check(m.count() == counts[i] && m.valid(), std::string("count of ") + SLOT_NAMES[i], m.count(), counts[i]);
		Sim s(base(i));
		s.clocks(m.count() * 2);
		check((int)s.beat.size() == 2 * m.ng, std::string("beats per bar = groups, ") + SLOT_NAMES[i], s.beat.size(), 2 * m.ng);
	}
}

static void testPhrase() {
	{
		Sim s(base()); s.clocks(56);
		std::vector<int> ph = idx(s, s.phrase);
		check(ph.size() == 2 && ph[0] == 25 && ph[1] == 53, "PHRASE once per 28 pulses on pulse 4 of bar 4", ph.empty() ? -1 : ph[0], 25);
	}
	{
		Params p = base(); p.phraseMode = LAST_PULSE;
		Sim s(p); s.clocks(28);
		std::vector<int> ph = idx(s, s.phrase);
		check(ph.size() == 1 && ph[0] == 27, "PHRASE last pulse: pulse 6 of bar 4", ph.empty() ? -1 : ph[0], 27);
	}
	{
		Params p = base(7); p.phraseMode = LAST_LONG;
		Sim s(p); s.clocks(44);
		std::vector<int> ph = idx(s, s.phrase);
		check(ph.size() == 1 && ph[0] == 33 + 4, "KOPANITSA last long beat: pulse 4 of the last bar", ph.empty() ? -1 : ph[0], 37);
	}
	{
		Params p = base(); p.bars = 1;
		Sim s(p); s.clocks(21);
		check(s.phrase.size() == 3, "BARS 1: a phrase trigger every bar", s.phrase.size(), 3);
	}
}

static void testAccent() {
	Params p = base(); p.accent = 1;
	Sim s(p); s.clocks(14);
	const double want[14] = {10, 2, 5.5, 2, 7.5, 2, 3.5, 9, 2, 5.5, 2, 7.5, 2, 3.5};
	bool ok = s.accentAtPulse.size() == 14;
	for (int i = 0; ok && i < 14; i++) if (!near(s.accentAtPulse[i], want[i], 1e-9)) { ok = false; check(false, "accent value", s.accentAtPulse[i], want[i]); }
	check(ok, "ACCENT 1: 10, 9, 7.5, 5.5, 3.5, 2 V on the right pulses");
	p.accent = 0;
	Sim t(p); t.clocks(14);
	bool all10 = true;
	for (double a : t.accentAtPulse) if (a != 10) all10 = false;
	check(all10, "ACCENT 0: always 10 V");
}

static void testLilt() {
	Params p = base(); p.lilt = 1;
	Sim s(p); s.clocks(70);
	check(s.pulse.size() == 70, "LILT 1: 70 clocks in, 70 pulses out", s.pulse.size(), 70);
	const double want[7] = {0, 0.8125, 1.625, 2.4375, 3.25, 4.5, 5.75};
	bool ok = true;
	for (int b = 1; b < 10; b++)   // bar 0 runs before the period is known
		for (int k = 0; k < 7; k++) {
			double got = s.pulseAt(s.pulse[7 * b + k]) - 7 * b;
			if (!near(got, want[k], 2.0 / 48000 * 8 + 1e-9)) { ok = false; check(false, "LILT 1 pulse time", got, want[k]); }
		}
	check(ok, "LILT 1 ruchenitsa pulse times 0, .8125, 1.625, ... 5.75");
	bool onEdge = true;
	for (int b = 0; b < 10; b++) if (s.pulse[7 * b] != s.clk[7 * b]) onEdge = false;
	check(onEdge, "LILT 1: pulse 0 of every bar on its clock edge");

	double u[MAXP + 1]; bool lg[MAXG];
	liltTable(factoryMetre(2), -1, u, lg);
	check(near(u[5] - u[4], 0.75, 1e-12) && near(u[1] - u[0], 1.1875, 1e-12), "LILT -1: long 0.75, short 1.1875", u[5] - u[4], 0.75);
	Metre m; parseGrouping("3 3 2", m);
	liltTable(m, 1, u, lg);
	check(near(u[7] - u[6], 0.6, 1e-12), "LILT limit: 3 3 2 short pulse 0.6", u[7] - u[6], 0.6);
	liltTable(factoryMetre(0), 1, u, lg);
	check(near(u[1], 1, 1e-12), "equal groups: no lilt");
}

static void testMetreChange() {
	{
		Sim s(base()); s.clocks(10);    // into bar 2
		s.p.slot = 7;                   // kopanitsa
		s.clocks(4);                    // up to pulse 13, the last of bar 2
		check(s.e.N == 7, "metre change waits for the bar line");
		s.clocks(1);
		check(s.e.N == 11 && s.e.slot == 7, "metre change takes effect at the bar line", s.e.N, 11);
	}
	{
		Params p = base(); p.changeAtPhrase = true;
		Sim s(p); s.clocks(10);
		s.p.slot = 7;
		s.clocks(18);   // 28 pulses: bars 1..4 still ruchenitsa
		check(s.e.N == 7, "next phrase: still 7 inside the phrase");
		s.clocks(1);
		check(s.e.N == 11 && s.e.bar == 0, "next phrase: change at bar 0", s.e.N, 11);
	}
	{
		Params p = base(); Sim s(p); s.clocks(3);
		s.p.lilt = 1; s.finishPulse();
		check(s.e.u[1] == 1.0, "LILT change waits for the bar line");
	}
}

static void testParse() {
	Metre a, b, c, d = factoryMetre(2), e = factoryMetre(2);
	check(parseGrouping("2+2+3", a) && parseGrouping("2 2 3", b) && parseGrouping("223", c) && a == b && b == c && a == factoryMetre(2),
		"user parsing: 2+2+3 = 2 2 3 = 223");
	check(parseGrouping("2, 2,3", a) && a == factoryMetre(2), "commas accepted");
	check(!parseGrouping("", d) && d == factoryMetre(2), "empty string rejected, previous kept");
	check(!parseGrouping("9999 99", e) && e == factoryMetre(2), "a 54-pulse string rejected, previous kept");
	check(!parseGrouping("22222222222222222", e), "17 groups rejected");
	Metre f; check(parseGrouping("x3y3z2!", f) && f.str() == "3 3 2", "other characters ignored");
}

static void testStop() {
	Sim s(base());
	s.clocks(10);
	s.run(0.01);   // a little into pulse 10
	s.clockOn = false;
	bool lowAll = false;
	long t0 = s.n;
	while (s.n < t0 + 3 * 48000) { s.step(); if (s.n == t0 + 48000) lowAll = !s.e.pulse && !s.e.beat && !s.e.lng; }
	check(lowAll, "clock stopped: gates low");
	size_t before = s.pulse.size();
	s.clockOn = true; s.nextEdge = s.n;
	s.clocks(11);
	(void)before;
	check(s.pulse.size() == s.clk.size(), "clock stop and resume: pulses out = clocks in", s.pulse.size(), s.clk.size());
	int c = (int)s.clk.size() - 1;
	check(s.e.kIn == c % 7 && s.e.bar == (c / 7) % 4, "bar position continues after a stop");
}

static void testTempoJumps() {
	Params p = base(); p.lilt = 1;
	Sim s(p);
	s.clocks(10);
	s.period = 1.0 / 16; s.nextEdge = s.clk.back() + std::lround(s.period * s.sr);  // doubling mid-bar
	s.clocks(15);
	s.period = 1.0 / 4; s.nextEdge = s.clk.back() + std::lround(s.period * s.sr);    // halving
	s.clocks(10);
	s.period = 1.0 / 30; s.nextEdge = s.clk.back() + std::lround(s.period * s.sr);
	s.clocks(14);
	s.finishPulse();
	check(s.pulse.size() == s.clk.size(), "tempo jumps at LILT 1: pulses out = clocks in", s.pulse.size(), s.clk.size());
	long minGap = 1 << 30;
	for (size_t i = 1; i < s.pulse.size(); i++) minGap = std::min(minGap, s.pulse[i] - s.pulse[i - 1]);
	check(minGap >= 24, "no two PULSE rises closer than 0.5 ms", minGap / 48.0, 0.5);
}

static void testReset() {
	Params p = base(); p.accent = 1;
	Sim s(p); s.clocks(10);
	s.run(0.05);
	s.in.reset = 10; s.step(); s.in.reset = 0;
	size_t bars = s.bar.size();
	s.finishPulse(); s.step();
	check(s.bar.size() == bars + 1 && near(s.e.accentV, 10, 1e-9), "RESET mid-bar: next clock gives BAR and 10 V accent");
	check(s.e.bar == 0 && s.e.kIn == 0, "RESET: bar 0 pulse 0");
	// reset arriving a sample after the clock edge: that edge becomes pulse 0
	Sim t(p); t.clocks(10);
	t.step();   // the edge of pulse 10 is fired on this sample
	t.in.reset = 10; t.step(); t.in.reset = 0;
	check(t.e.bar == 0 && t.e.kIn == 0 && t.e.kOut == 1, "RESET just after a clock edge restarts on that edge");
	t.clocks(7);
	check(t.e.bar == 1 && t.e.kIn == 0, "...and the bar runs from there");
}

static void testMetreCV() {
	// slot = knob + round(2 * volts), clamped 0..17 (done by the module; checked here as arithmetic)
	auto slotOf = [](int knob, double v) { return std::max(0, std::min(NUM_SLOTS - 1, knob + (int)std::lround(2 * v))); };
	check(slotOf(2, 0.5) == 3 && slotOf(2, 1.0) == 4 && slotOf(2, -0.5) == 1, "METRE CV: 0.5 V steps one slot");
	check(slotOf(2, 10) == 17 && slotOf(2, -10) == 0, "METRE CV clamps");
}

static void testFuzz() {
	const double rates[3] = {44100, 48000, 96000};
	for (double sr : rates) {
		Engine e; e.init(sr);
		Params p; Inputs in;
		uint64_t r = 0x12345678ull ^ (uint64_t)sr;
		auto rnd = [&]() { r ^= r << 13; r ^= r >> 7; r ^= r << 17; return (r >> 11) * (1.0 / 9007199254740992.0); };
		long n = (long)(60 * sr), next = 0, high = 0, nextParam = 0, quietUntil = 0;
		double period = 0.1;
		bool ok = true, conserved = true;
		long clocks = 0, pulsesAtReset = 0, clocksAtReset = 0;
		uint32_t pc0 = 0;
		for (long i = 0; i < n; i++) {
			if (i >= nextParam) {
				nextParam = i + (long)(0.05 * sr * 20);
				p.slot = (int)(rnd() * NUM_SLOTS); p.lilt = 2 * rnd() - 1; p.accent = rnd();
				p.bars = BARS_VALUES[(int)(rnd() * NUM_BARS)]; p.phraseMode = (int)(rnd() * 3); p.changeAtPhrase = rnd() < 0.5;
				period = 0.03 + 0.2 * rnd();
				if (rnd() < 0.1) { quietUntil = i + (long)(sr * (0.5 + rnd())); }
				if (rnd() < 0.1) { in.reset = 10; }
			} else in.reset = 0;
			if (in.reset > 0) { pulsesAtReset = e.pulseCount; clocksAtReset = clocks; (void)pulsesAtReset; (void)clocksAtReset; }
			if (i >= next && i >= quietUntil) {
				next = i + (long)(period * sr * (0.7 + 0.6 * rnd())); high = i + (long)(0.001 * sr); clocks++;
			}
			in.clock = i < high ? 10 : 0;
			e.process(p, in);
			if (!std::isfinite(e.accentV) || e.accentV < 0 || e.accentV > 10 || !std::isfinite(e.T)) ok = false;
			if (e.kOut > e.N || e.kIn > e.N) ok = false;
		}
		(void)pc0;
		// conservation is checked in a jitter run without resets (fuzz resets restart the bar)
		Engine f; f.init(sr); Params q; q.lilt = 1; Inputs fi;
		long nx = 0, hi = 0, cl = 0;
		for (long i = 0; i < (long)(30 * sr); i++) {
			if (i % (long)(sr * 3) == 0) { q.slot = (int)(rnd() * NUM_SLOTS); q.lilt = 2 * rnd() - 1; }
			bool quiet = (i % (long)(sr * 5)) > (long)(sr * 4);
			if (i >= nx && !quiet) { nx = i + (long)(0.1 * sr * (0.7 + 0.6 * rnd())); hi = i + (long)(0.001 * sr); cl++; }
			fi.clock = i < hi ? 10 : 0;
			f.process(q, fi);
			if (quiet && (i % (long)(sr * 5)) == (long)(sr * 4.99)) {
				// after a quiet second pulses out match clocks in, give or take the lilt shift inside one bar
				long owed = (long)f.pulseCount - cl;
				if (owed > 3 || owed < -3) conserved = false;
			}
		}
		char b[80];
		std::snprintf(b, sizeof b, "fuzz %.0f Hz: finite, accent 0..10, state in range", sr); check(ok, b);
		std::snprintf(b, sizeof b, "fuzz %.0f Hz: pulse count conserved under jitter", sr); check(conserved, b);
	}
}

int main() {
	testRuchenitsa();
	testFactory();
	testPhrase();
	testAccent();
	testLilt();
	testMetreChange();
	testParse();
	testStop();
	testTempoJumps();
	testReset();
	testMetreCV();
	testFuzz();
	std::printf("Horo: %d passed, %d failed\n", passes, fails);
	return fails ? 1 : 0;
}
