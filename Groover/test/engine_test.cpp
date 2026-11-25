// Offline checks for the Groover engine (no Rack needed):
//   g++ -O2 -std=c++17 test/engine_test.cpp -o test/engine_test && ./test/engine_test [groove files...]
#include "../src/GrooverDSP.hpp"
#include <cstdio>

using namespace groover;

static int fails = 0;
#define CHECK(c, ...) do { if (!(c)) { fails++; std::printf("FAIL: " __VA_ARGS__); std::printf("\n"); } } while (0)

// Run a 120 BPM, 4 PPQN clock for `beats` beats; return fire times (ms) of the grooved clock.
static std::vector<double> runClock(const Groove& g, Settings st, double beats, double startDelay = 0.0) {
	const double sr = 48000.0, dt = 1.0 / sr;
	Clock clk; clk.ppqn = 4; clk.reset();
	GroovedClock gc; gc.reset();
	std::vector<double> fired;
	double pulseEvery = 0.125, nextPulse = startDelay;
	long samples = (long) ((beats * 0.5 + startDelay) * sr);
	for (long i = 0; i < samples; i++) {
		double now = i * dt;
		clk.tick(dt);
		if (now >= nextPulse - 1e-12) { clk.pulse(); nextPulse += pulseEvery; }
		if (gc.process(g, st, clk.pos()) >= 0) fired.push_back((now - startDelay) * 1000.0);
	}
	return fired;
}

int main(int argc, char** argv) {
	std::vector<Groove> b = builtinGrooves();
	std::printf("%zu built-in grooves\n", b.size());
	for (const Groove& g : b) {
		std::printf("  %-24s len %4.1f base %-5s steps %d\n", g.name.c_str(), g.length, BASE_NAMES[g.base], g.steps(g.base));
		CHECK(g.steps(g.base) > 0, "%s has no steps", g.name.c_str());
	}

	// slots 1-2: 75% swing, off-16ths half a step late (62.5 ms at 120 BPM); accents
	{
		Settings st; st.base = 3;
		std::vector<double> f = runClock(b[0], st, 4.0, 0.0);
		CHECK(f.size() >= 8 && std::fabs(f[1] - 187.5) < 0.2 && std::fabs(f[2] - 250.0) < 0.2, "swing75 %.2f %.2f", f[1], f[2]);
		st.velocity = 1.f;
		for (int k = 0; k < 16; k++) {
			bool a1 = k % 2 == 0, a2 = k % 4 == 0;
			CHECK((stepAccent(b[0], st, k) > 0.99f) == a1, "slot1 accent step %d", k + 1);
			CHECK((stepAccent(b[1], st, k) > 0.99f) == a2, "slot2 accent step %d", k + 1);
		}
	}
	// Swing 16 58%: off-16ths 0.04 beat late = 20 ms at 120 BPM, on-16ths on time
	{
		Settings st; st.base = 3;
		std::vector<double> f = runClock(b[4], st, 8.0, 0.0);
		CHECK(f.size() >= 30, "swing58 fired %zu", f.size());
		for (size_t k = 1; k < 9 && k < f.size(); k++) {
			double grid = k * 125.0;
			double want = (k & 1) ? grid + 20.0 : grid;
			CHECK(std::fabs(f[k] - want) < 0.2, "swing58 step %zu at %.2f ms, want %.2f", k, f[k], want);
		}
		std::printf("swing58 first 6 (ms): %.1f %.1f %.1f %.1f %.1f %.1f\n", f[0], f[1], f[2], f[3], f[4], f[5]);
	}
	// Pushing: off-16ths 0.07 step EARLY = 8.75 ms before the grid (prediction)
	{
		Settings st; st.base = 3;
		std::vector<double> f = runClock(b[10], st, 8.0, 0.0);
		std::printf("pushing first 6 (ms): %.2f %.2f %.2f %.2f %.2f %.2f\n", f[0], f[1], f[2], f[3], f[4], f[5]);
		CHECK(std::fabs(f[5] - (625.0 - 8.75)) < 0.2, "pushing step 5 at %.2f", f[5]);
		CHECK(std::fabs(f[4] - 500.0) < 0.2, "pushing step 4 at %.2f", f[4]);
	}
	// AMOUNT 0 = straight
	{
		Settings st; st.base = 3; st.amount = 0.f;
		std::vector<double> f = runClock(b[6], st, 4.0, 0.0);
		for (size_t k = 0; k < f.size(); k++) CHECK(std::fabs(f[k] - k * 125.0) < 0.1, "amount0 step %zu %.2f", k, f[k]);
	}
	// clock stops: nothing runs on past the last real pulse (on-grid steps)
	{
		Settings st; st.base = 3;
		const double sr = 48000.0, dt = 1.0 / sr;
		Clock clk; clk.reset(); GroovedClock gc; gc.reset();
		int fired = 0;
		for (long i = 0; i < (long) (3.0 * sr); i++) {
			double now = i * dt;
			clk.tick(dt);
			if (now < 1.0 && std::fmod(now + 1e-9, 0.125) < dt) clk.pulse();
			if (gc.process(b[2], st, clk.pos()) >= 0) fired++;
		}
		CHECK(fired == 8, "straight with 8 pulses fired %d", fired);
	}
	// Lane: trigger on an off-16th gets 20 ms (swing 58); look-ahead allows early
	{
		Settings st; st.base = 3;
		long k;
		double d = laneDelay(b[4], st, 0.25, 0.5, 0.f, 0.0, k);
		CHECK(k == 1 && std::fabs(d - 0.020) < 1e-6, "lane delay %.4f k %ld", d, k);
		d = laneDelay(b[10], st, 0.25, 0.5, 0.f, 0.0, k);
		CHECK(d == 0.0, "pushing lane without look-ahead should be 0, got %f", d);
		d = laneDelay(b[10], st, 0.25, 0.5, 0.f, 0.020, k);
		CHECK(std::fabs(d - (0.020 - 0.00875)) < 1e-6, "pushing lane look-ahead %f", d);
		Lane ln;
		ln.push(0.010, true); ln.push(0.015, false);
		CHECK(!ln.process(0.005) && ln.process(0.011) && ln.out && !ln.process(0.016) && !ln.out, "lane queue");
	}
	// Accent: velocity 100% follows the groove, -100% flips it
	{
		Settings st; st.base = 3; st.velocity = 1.f;
		float a0 = stepAccent(b[4], st, 0), a1 = stepAccent(b[4], st, 1);
		CHECK(a0 > 0.99f && std::fabs(a1 - 85.f / 110.f) < 1e-3, "accent %f %f", a0, a1);
		st.velocity = -1.f;
		CHECK(stepAccent(b[4], st, 1) < stepAccent(b[4], st, 0) + 1.f, "flip");
	}
	// inflate on a known buffer
	{
		// gzip of "hello hello hello hello\n"
		const unsigned char gz[] = {0x1f,0x8b,0x08,0x00,0x00,0x00,0x00,0x00,0x02,0xff,0xcb,0x48,0xcd,0xc9,0xc9,0x57,0xc8,0x40,0x27,0xb9,0x00,0x00,0x88,0x59,0x0b,0x18,0x00,0x00,0x00};
		std::string out;
		bool ok = gunzip(std::string((const char*) gz, sizeof(gz)), out);
		CHECK(ok && out == "hello hello hello hello\n", "gunzip: ok=%d '%s'", ok, out.c_str());
	}
	// write every built-in as an .agr (into grooves/ when run as "make test"), load it back, compare tables
	{
		std::string dir = std::getenv("GROOVER_EXPORT") ? std::getenv("GROOVER_EXPORT") : "/tmp";
		for (size_t i = 0; i < b.size(); i++) {
			Groove g = b[i];
			g.velocity = 100.f;
			char fn[512];
			std::snprintf(fn, sizeof(fn), "%s/%02zu %s.agr", dir.c_str(), i + 1, g.name.c_str());
			{ std::ofstream of(fn); of << writeAgr(g); }
			Groove r; std::string why;
			bool ok = loadGrooveFile(fn, r, why);
			CHECK(ok, "reload %s: %s", fn, why.c_str());
			if (!ok) continue;
			CHECK(r.base == g.base && std::fabs(r.length - g.length) < 1e-9 && r.steps(r.base) == g.steps(g.base), "reload %s shape", fn);
			for (int k = 0; k < g.steps(g.base); k++)
				CHECK(std::fabs(r.off[r.base][k] - g.off[g.base][k]) < 1e-5 && std::fabs(r.vel[r.base][k] - g.vel[g.base][k]) < 0.01f,
				      "reload %s step %d", fn, k);
		}
	}
	// files given on the command line
	for (int i = 1; i < argc; i++) {
		Groove g; std::string why;
		bool ok = loadGrooveFile(argv[i], g, why);
		CHECK(ok, "%s: %s", argv[i], why.c_str());
		if (!ok) continue;
		std::printf("%s: '%s' %zu notes, %.2f beats, base %s, T%.0f Q%.0f R%.0f V%.0f\n   offsets:", argv[i], g.name.c_str(),
		            g.notes.size(), g.length, BASE_NAMES[g.base], g.timing, g.quantize, g.random, g.velocity);
		for (int k = 0; k < std::min(16, g.steps(g.base)); k++) std::printf(" %+.2f", g.off[g.base][k]);
		std::printf("\n   velocity:");
		for (int k = 0; k < std::min(16, g.steps(g.base)); k++) std::printf(" %.2f", g.vel[g.base][k]);
		std::printf("\n");
	}
	std::printf(fails ? "%d FAILED\n" : "all ok\n", fails);
	return fails ? 1 : 0;
}
