// Offline checks + demo WAVs for Paddock engines (no Rack needed):  make test
#include "../src/HorseDSP.hpp"
#include <cstdio>
#include <vector>
#include <string>
using namespace paddock;

static void wav(const std::string& path, const std::vector<float>& L, const std::vector<float>& R, int sr) {
	FILE* f = fopen(path.c_str(), "wb"); if (!f) return;
	int n = (int) L.size(), bytes = n * 4;
	auto u32 = [&](uint32_t v) { fwrite(&v, 4, 1, f); };
	auto u16 = [&](uint16_t v) { fwrite(&v, 2, 1, f); };
	fwrite("RIFF", 1, 4, f); u32(36 + bytes); fwrite("WAVEfmt ", 1, 8, f);
	u32(16); u16(1); u16(2); u32(sr); u32(sr * 4); u16(4); u16(16);
	fwrite("data", 1, 4, f); u32(bytes);
	for (int i = 0; i < n; i++) {
		int16_t a = (int16_t) std::max(-32767.f, std::min(32767.f, L[i] / 5.f * 26000.f));
		int16_t b = (int16_t) std::max(-32767.f, std::min(32767.f, R[i] / 5.f * 26000.f));
		fwrite(&a, 2, 1, f); fwrite(&b, 2, 1, f);
	}
	fclose(f);
}
static float avg(const float* x) { float s = 0; for (int k = 0; k < OS; k++) s += x[k]; return s / OS; }
static int fails = 0;
static void check(bool ok, const char* what) { printf("%s %s\n", ok ? "ok  " : "FAIL", what); if (!ok) fails++; }

int main(int argc, char** argv) {
	std::string dir = argc > 1 ? argv[1] : ".";
	const int sr = 48000; const float st = 1.f / sr;

	// ---- Swerve: pitch at +-5 V bounds, then narrowing bounds raises pitch, paradox stays finite
	{
		SwerveEngine e; e.rng.seed(1);
		int tops = 0;
		for (int i = 0; i < sr; i++) { e.process(220.f, 220.f, -5.f, 5.f, false, 0.f, st); if (e.events & 1) tops++; }
		check(std::abs(tops - 220) <= 2, "Swerve 220 Hz at +-5 V bounds");
		tops = 0;
		for (int i = 0; i < sr; i++) { e.process(220.f, 220.f, -2.5f, 2.5f, false, 0.f, st); if (e.events & 1) tops++; }
		check(std::abs(tops - 440) <= 3, "Swerve doubles when bounds halve");
		bool finite = true; float mx = 0;
		for (int i = 0; i < sr; i++) { e.process(220.f, 220.f, 1.f, 0.f, false, 1.f, st); for (int k = 0; k < OS; k++) { finite &= std::isfinite(e.tri[k]); mx = std::max(mx, std::fabs(e.tri[k])); } }
		check(finite && mx <= 1.1f, "Swerve paradox stays inside the overlap");
		// one-shot
		SwerveEngine o; o.rng.seed(2);
		for (int i = 0; i < 2000; i++) o.process(50.f, 50.f, -5.f, 5.f, true, 0.f, st);
		bool parked = o.h.dir == 0; o.trigUp();
		int ev = 0; for (int i = 0; i < sr; i++) { o.process(50.f, 50.f, -5.f, 5.f, true, 0.f, st); ev |= o.events; }
		check(parked && o.h.dir == 0 && ev == 3, "Swerve ONCE parks, then one swoop per UP trigger");

		// demo: bounds swept by a slow LFO, tilt changing
		std::vector<float> L, R;
		SwerveEngine d; d.rng.seed(3);
		for (int i = 0; i < sr * 6; i++) {
			float t = i * st;
			float hi = 1.f + 4.f * (0.5f + 0.5f * std::sin(t * 1.3f));
			float lo = -5.f + 3.f * (0.5f + 0.5f * std::sin(t * 0.7f));
			d.process(110.f, 110.f * (1.f + 2.f * (t / 6.f)), lo, hi, false, 1.f, st);
			float y = avg(d.tri) * 0.6f; L.push_back(y); R.push_back(y);
		}
		wav(dir + "/swerve-demo.wav", L, R, sr);
	}

	// ---- Gallop: four horses stay finite, inside the rails, and keep moving
	{
		GallopEngine g; g.rng.seed(4);
		float hz[4] = {55.f, 82.f, 61.f, 97.f}, tilt[4] = {0.f, 0.3f, -0.4f, 0.1f};
		float nanv = NAN, hiIn[4] = {nanv, nanv, nanv, nanv}, loIn[4] = {nanv, nanv, nanv, nanv};
		bool finite = true, inside = true; float mn[4] = {9, 9, 9, 9}, mx[4] = {-9, -9, -9, -9};
		std::vector<float> L, R;
		for (int i = 0; i < sr * 8; i++) {
			float t = i * st;
			hz[0] = 55.f * std::pow(2.f, 1.5f * std::sin(t * 0.4f));
			g.process(hz, tilt, hiIn, loIn, 5.f, 1.f, st);
			float a[4];
			for (int j = 0; j < 4; j++) {
				a[j] = avg(g.tri[j]);
				finite &= std::isfinite(a[j]);
				inside &= std::fabs(a[j]) <= 5.2f;
				if (i > sr) { mn[j] = std::min(mn[j], a[j]); mx[j] = std::max(mx[j], a[j]); }
			}
			L.push_back(0.5f * (a[0] + a[1]) * 0.8f); R.push_back(0.5f * (a[2] + a[3]) * 0.8f);
		}
		check(finite, "Gallop finite");
		check(inside, "Gallop inside the +-5 V rails");
		bool moving = true; for (int j = 0; j < 4; j++) moving &= (mx[j] - mn[j]) > 0.5f;
		check(moving, "Gallop: all four horses keep moving");
		wav(dir + "/gallop-demo.wav", L, R, sr);
	}

	// ---- Tumble: rhythm rates, coupling changes the pattern, level sane
	{
		float hz[5] = {2.f, 3.f, 1.5f, 2.5f, 4.f}, touch0[5] = {0, 0, 0, 0, 0}, touch1[5] = {0.7f, -0.5f, 0.6f, 0.4f, -0.6f};
		TumbleEngine a; a.rng.seed(5); a.roll();
		int hits0 = 0; for (int i = 0; i < sr * 4; i++) { a.process(hz, touch0, 110.f, 3.f, 0.4f, 0.6f, 0.f, sr); hits0 += a.fired[0]; }
		check(std::abs(hits0 - 8) <= 1, "Tumble rolz 1 at 2 Hz fires about 8 times in 4 s untouched");
		TumbleEngine b; b.rng.seed(6); b.roll();
		std::vector<float> L, R; float peak = 0;
		int hits[5] = {};
		for (int i = 0; i < sr * 10; i++) {
			const float* tc = i < sr * 4 ? touch0 : touch1;
			if (i % (sr * 2) == 0 && i >= sr * 7) b.sync();
			b.process(hz, tc, 98.f, 5.f, 0.5f, 0.5f, 1.f, sr);
			for (int j = 0; j < 5; j++) hits[j] += b.fired[j];
			L.push_back(b.outL); R.push_back(b.outR);
			peak = std::max(peak, std::max(std::fabs(b.outL), std::fabs(b.outR)));
		}
		printf("     Tumble hits in 10 s: %d %d %d %d %d, peak %.2f V\n", hits[0], hits[1], hits[2], hits[3], hits[4], peak);
		check(peak > 1.f && peak <= 5.f, "Tumble gong level between 1 and 5 V");
		wav(dir + "/tumble-demo.wav", L, R, sr);
	}
	printf(fails ? "%d FAILED\n" : "all passed\n", fails);
	return fails ? 1 : 0;
}
