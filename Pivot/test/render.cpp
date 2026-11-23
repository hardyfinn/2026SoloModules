// Offline checks for the Pivot engine + a demo WAV. No Rack needed.
//   make test      (prints checks, writes test/pivot-demo.wav)
// A fake "Live" sends a 24 PPQN clock at 120 BPM, and Pivot drives a tiny drum kit.
#include "../src/PivotEngine.hpp"
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <string>
#include <algorithm>

using namespace pivot;
static const double SR = 48000.0;
static int fails = 0;
#define CHECK(c, msg) do { if (!(c)) { std::printf("FAIL: %s\n", msg); fails++; } else std::printf("ok:   %s\n", msg); } while (0)

// Default chain, same as the module's knob defaults.
static const int DEF_OLD[NUM_STEPS] = {8, 9, 3, 9, 6, 5, 2, 7};
static const int DEF_NEW[NUM_STEPS] = {9, 8, 6, 7, 5, 6, 0, 9};

static void fillCum(Settings& s) {
	s.cum[0] = 1.0;
	for (int i = 0; i < NUM_STEPS; i++) {
		int n, d;
		stepRatio(DEF_OLD[i], DEF_NEW[i], n, d);
		s.cum[i + 1] = s.cum[i] * n / d;
	}
}

// Runs the engine against a steady 120 BPM, 24 PPQN clock.
struct Rig {
	Engine e;
	Settings s;
	long n = 0;
	double clockPhase = 0.0;
	Rig() { fillCum(s); s.clockConnected = true; s.inPpqn = 24; }
	Out tick(Events ev = Events()) {
		double t = n / SR;
		double pulseLen = 60.0 / 120.0 / 24.0;
		long idx = (long) std::floor(t / pulseLen + 1e-12);
		long prev = n == 0 ? -1 : (long) std::floor((n - 1) / SR / pulseLen + 1e-12);
		ev.clock = idx != prev;
		n++;
		return e.process(1.0 / SR, s, ev);
	}
};

static void writeWav(const char* path, const std::vector<float>& L, const std::vector<float>& R) {
	FILE* f = std::fopen(path, "wb");
	if (!f) return;
	uint32_t n = L.size(), bytes = n * 4;
	auto u32 = [&](uint32_t v) { std::fwrite(&v, 4, 1, f); };
	auto u16 = [&](uint16_t v) { std::fwrite(&v, 2, 1, f); };
	std::fwrite("RIFF", 1, 4, f); u32(36 + bytes); std::fwrite("WAVEfmt ", 1, 8, f);
	u32(16); u16(1); u16(2); u32((uint32_t) SR); u32((uint32_t) SR * 4); u16(4); u16(16);
	std::fwrite("data", 1, 4, f); u32(bytes);
	for (uint32_t i = 0; i < n; i++) {
		int16_t a = (int16_t) std::max(-32767.f, std::min(32767.f, L[i] * 32767.f));
		int16_t b = (int16_t) std::max(-32767.f, std::min(32767.f, R[i] * 32767.f));
		std::fwrite(&a, 2, 1, f); std::fwrite(&b, 2, 1, f);
	}
	std::fclose(f);
}

int main(int argc, char** argv) {
	std::string dir = argc > 1 ? argv[1] : ".";

	// 0. ratios
	{
		int n, d;
		stepRatio(8, 9, n, d);  // dotted 8th = quarter
		CHECK(n == 4 && d == 3, "dotted 8th = quarter is x4/3");
		stepRatio(3, 6, n, d);  // 8th triplet = 8th
		CHECK(n == 3 && d == 2, "8th triplet = 8th is x3/2");
		stepRatio(9, 7, n, d);  // quarter = quarter triplet
		CHECK(n == 2 && d == 3, "quarter = quarter triplet is x2/3");
	}

	// 1. at home, output beats sit on the clock's beats
	{
		Rig g;
		std::vector<double> beats;
		for (int i = 0; i < (int) (SR * 4); i++) { Out o = g.tick(); if (o.beat) beats.push_back((g.n - 1) / SR); }
		bool ok = beats.size() == 8;
		for (size_t i = 0; i < beats.size(); i++) ok = ok && std::fabs(beats[i] - 0.5 * i) < 1.0 / SR + 1e-9;
		std::printf("      %zu beats in 4 s at home\n", beats.size());
		CHECK(ok, "home: beats line up with the incoming clock");
	}

	// 2. NEXT -> x4/3 at the next bar, beats every 0.375 s from the pivot on
	{
		Rig g;
		std::vector<double> beats;
		double pivotAt = -1;
		for (int i = 0; i < (int) (SR * 8); i++) {
			Events ev;
			if (i == (int) (SR * 1.0)) ev.next = true;  // pressed mid bar 1
			Out o = g.tick(ev);
			if (o.pivot) pivotAt = (g.n - 1) / SR;
			if (o.beat && pivotAt >= 0) beats.push_back((g.n - 1) / SR);
		}
		std::printf("      pivot at %.4f s, tempo now %.2f BPM\n", pivotAt, g.e.tempo());
		CHECK(std::fabs(pivotAt - 2.0) < 2.0 / SR, "pivot waits for the next bar line");
		bool ok = beats.size() > 10;
		for (size_t i = 0; i < beats.size(); i++) ok = ok && std::fabs(beats[i] - (pivotAt + 0.375 * i)) < 2.0 / SR;
		CHECK(ok, "after x4/3 the new beat is the old dotted 8th (0.375 s)");
		CHECK(std::fabs(g.e.tempo() - 160.0) < 0.05, "tempo reads 160 BPM");
	}

	// 3. go to step 3, then HOME with LAND: output bars land back on the clock's bars
	for (int glide = 0; glide < 2; glide++) {
		Rig g;
		g.s.glideIdx = glide ? 4 : 0;  // snap, or 2-bar glides
		std::vector<double> bars;
		double landedAt = -1;
		bool wasAway = false;
		for (int i = 0; i < (int) (SR * 60); i++) {
			Events ev;
			if (i == (int) (SR * 1.0)) ev.request = 3;  // x4/3 * x3/4 * x3/2 = x1.5
			if (i == (int) (SR * 13.3)) ev.home = true;
			Out o = g.tick(ev);
			if (o.away) wasAway = true;
			if (wasAway && !o.away && landedAt < 0) landedAt = (g.n - 1) / SR;
			if (o.bar && landedAt >= 0) bars.push_back((g.n - 1) / SR);
		}
		bool ok = landedAt > 0 && bars.size() > 5;
		for (size_t i = 0; i < bars.size(); i++) {
			double k = bars[i] / 2.0;
			ok = ok && std::fabs(k - std::floor(k + 0.5)) * 2.0 < 2.0 / SR;
		}
		std::printf("      %s: landed at %.4f s (a bar line is every 2 s), %zu bars checked after\n",
		            glide ? "2-bar glide" : "snap", landedAt, bars.size());
		CHECK(ok, glide ? "HOME + LAND with glide comes back on Live's bar lines" : "HOME + LAND (snap) comes back on Live's bar lines");
	}

	// 4. glides are smooth: tempo moves one way, no jumps
	{
		Rig g;
		g.s.glideIdx = 5;  // 4 bars
		double last = g.e.tempo(), maxJump = 0;
		bool mono = true;
		for (int i = 0; i < (int) (SR * 14); i++) {
			Events ev;
			if (i == 100) ev.request = 1;
			g.tick(ev);
			if (i % 480 == 0 && g.e.mode == MODE_GLIDE) {
				double t = g.e.tempo();
				if (t < last - 1e-6) mono = false;
				maxJump = std::max(maxJump, t - last);
				last = t;
			}
		}
		std::printf("      biggest tempo change per 10 ms during the glide: %.3f BPM, ends at %.2f\n", maxJump, g.e.tempo());
		CHECK(mono && maxJump < 1.0 && std::fabs(g.e.tempo() - 160.0) < 0.05, "4-bar glide 120 -> 160 is smooth");
	}

	// 5. AUTO walks the chain and comes home on its own
	{
		Rig g;
		g.s.autoBars = 2; g.s.length = 3;
		std::vector<int> seen;
		for (int i = 0; i < (int) (SR * 40); i++) {
			Out o = g.tick();
			if (o.pivot) seen.push_back(g.e.step);
		}
		std::printf("      AUTO 2 bars, LENGTH 3:");
		for (size_t i = 0; i < seen.size(); i++) std::printf(" %d", seen[i]);
		std::printf("\n");
		CHECK(seen.size() >= 5 && seen[0] == 1 && seen[1] == 2 && seen[2] == 3 && seen[3] == 0 && seen[4] == 1, "AUTO: 1, 2, 3, home, 1...");
	}

	// ---- demo: AUTO every 4 bars through 4 steps, snap first, then with 1-bar glides
	{
		Rig g;
		g.s.autoBars = 4; g.s.length = 4; g.s.outPpqn = 4;
		const int N = (int) (SR * 96);
		std::vector<float> L(N), R(N);
		double kEnv = 0, kPh = 0, hEnv = 0, sEnv = 0, gEnv = 0, gPh = 0, bEnv = 0, bPh = 0;
		uint32_t rnd = 1;
		bool lastClk = false;
		long sixteenth = 0;
		for (int i = 0; i < N; i++) {
			if (i == N / 2) g.s.glideIdx = 3;  // second half: 1-bar glides
			Out o = g.tick();
			if (o.clock && !lastClk) {  // a 16th of the pivoted grid
				hEnv = (sixteenth % 4 == 2) ? 1.0 : 0.55;
				sixteenth++;
			}
			lastClk = o.clock;
			if (o.beat) {
				kEnv = 1.0; kPh = 0;
				if (sixteenth % 8 == 5) sEnv = 1.0;  // beats 2 and 4
			}
			if (o.bar) sixteenth = 1;
			// ghost (the tempo we just left) on a rim
			static bool lastGhost = false;
			if (o.ghost && !lastGhost && g.e.step != 0) gEnv = 0.6;
			lastGhost = o.ghost;
			// Live's own bar line: a low bell, so you can hear Pivot leave and land
			double t = i / SR;
			if (std::fmod(t, 2.0) < 1.0 / SR) { bEnv = 0.5; bPh = 0; }

			rnd = rnd * 1664525u + 1013904223u;
			double noise = (rnd >> 8) / 8388608.0 - 1.0;
			double kf = 45.0 + 120.0 * kEnv * kEnv;
			kPh += kf / SR;
			double kick = std::sin(2 * M_PI * kPh) * kEnv;
			kEnv *= 0.99985;
			static double hp = 0;  // crude high-pass for hats
			double hat = (noise - hp) * hEnv * 0.25; hp += (noise - hp) * 0.3;
			hEnv *= 0.9992;
			static double bp = 0;
			double snare = (noise * 0.6 + 0.4 * std::sin(2 * M_PI * 190 * t)) * sEnv * 0.35; bp = snare;
			sEnv *= 0.9996;
			gPh += 1800.0 / SR;
			double rim = std::sin(2 * M_PI * gPh) * gEnv * 0.18; gEnv *= 0.9975;
			bPh += 98.0 / SR;
			double bell = (std::sin(2 * M_PI * bPh) + 0.3 * std::sin(2 * M_PI * bPh * 2.76)) * bEnv * 0.12; bEnv *= 0.99993;
			L[i] = (float) std::tanh(0.6 * kick + hat * 0.8 + snare + rim * 1.2 + bell);
			R[i] = (float) std::tanh(0.6 * kick + hat * 1.2 + snare + rim * 0.6 + bell);
		}
		std::string path = dir + "/test/pivot-demo.wav";
		writeWav(path.c_str(), L, R);
		std::printf("      wrote %s\n", path.c_str());
	}

	std::printf(fails ? "\n%d check(s) FAILED\n" : "\nall checks passed\n", fails);
	return fails ? 1 : 0;
}
