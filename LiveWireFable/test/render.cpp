// Live Wire offline checks (no Rack needed): make test
// Every check prints PASS or FAIL. See docs/BUILD-INSTRUCTIONS.md section 7.
// Usage: render test <outdir> <refdir>    all tests (refdir holds the C reference renders, see the Makefile)
//        render demos <outdir>            scene and preset renders (raw stereo f32 at 48 kHz, plus a list)
//        render tune                      measures the singing presets' pitch at 0 V
#include "../src/dsp/Voice.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstdarg>
#include <cstring>
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <functional>
#include <algorithm>
#include <chrono>

namespace refscene {
using namespace livewire;
#include "../tools/proto/scenes.h"
}
using namespace livewire;

static int nFail = 0, nPass = 0;
static void report(bool ok, const char* name, const std::string& detail) {
	std::printf("%s  %-26s %s\n", ok ? "PASS" : "FAIL", name, detail.c_str()); std::fflush(stdout);
	if (ok) nPass++; else nFail++;
}
static std::string fmt(const char* f, ...) { char b[4096]; va_list a; va_start(a, f); vsnprintf(b, sizeof b, f, a); va_end(a); return b; }
static void parallelFor(int n, const std::function<void(int)>& fn) {
	int nt = std::max(1u, std::thread::hardware_concurrency());
	std::atomic<int> next(0); std::vector<std::thread> th;
	for (int t = 0; t < nt; t++) th.emplace_back([&]() { for (;;) { int i = next++; if (i >= n) break; fn(i); } });
	for (auto& t : th) t.join();
}
static void writeRaw(const std::string& p, const std::vector<float>& v) { FILE* f = fopen(p.c_str(), "wb"); if (!f) return; fwrite(v.data(), 4, v.size(), f); fclose(f); }
static std::vector<float> readRaw(const std::string& p) {
	std::vector<float> v; FILE* f = fopen(p.c_str(), "rb"); if (!f) return v;
	fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET); v.resize(n / 4); size_t g = fread(v.data(), 4, v.size(), f); v.resize(g); fclose(f); return v;
}

// ------------------------------------------------------------ the reference main(), run on the C++ engine
struct SceneOut { std::vector<float> st; long strikes[3] = {0, 0, 0}; long guard = 0; bool finite = true; };
static SceneOut runScene(int sc, int fuzzSeed, double sr) {
	SceneOut o; bool fuzz = sc < 0;
	double dur = fuzz ? 30 : refscene::scene_dur(sc); long N = (long)(dur * sr);
	o.st.assign(2 * N, 0.f);
	std::vector<Engine> e(3); for (int i = 0; i < 3; i++) e[i].init(sr, fuzz ? fuzzSeed * 7 + i : 1 + i);
	K k[3]; int ninst = 1; unsigned long long fr = fuzz ? fuzzSeed * 977 + 5 : 1;
	K fz; std::memset(&fz, 0, sizeof fz);
	for (long n = 0; n < N; n++) {
		double t = n / sr, L = 0, R = 0;
		if (fuzz) { ninst = 1; fz.trig = fz.surge = 0;
			if (n % (long)(0.5 * sr) == 0) { for (int i = 0; i < NKNOB; i++) { fr ^= fr << 13; fr ^= fr >> 7; fr ^= fr << 17; fz.*KNOBS[i] = (fr >> 11) * (1.0 / 9007199254740992.0); }
				fr ^= fr << 13; fr ^= fr >> 7; fr ^= fr << 17; fz.track = fr & 1; fz.keyPatched = (fr >> 1) & 1; fz.key = (fr >> 2) & 1; fz.touch = ((fr >> 3) & 7) == 0; fz.voct = ((fr >> 8) & 255) / 255.0 * 8 - 4; fz.voctMains = ((fr >> 16) & 255) / 255.0 * 6 - 2; }
			if (n % (long)(0.11 * sr) == 0) fz.trig = 1;
			if (n % (long)(1.7 * sr) == 0) fz.surge = 1;
			k[0] = fz; }
		else refscene::scene(sc, t, n, sr, k, &ninst);
		double in = fuzz ? ((n / (long)(0.25 * sr)) % 3 == 0 ? std::sin(n * 0.05) * 2.0 : 0) : 0;
		for (int i = 0; i < ninst; i++) { double l, r, a, f; e[i].process(k[i], in, &l, &r, &a, &f); L += l; R += r; }
		if (!std::isfinite(L) || !std::isfinite(R)) { o.finite = false; L = R = 0; }
		o.st[2 * n] = (float)L; o.st[2 * n + 1] = (float)R;
	}
	for (int i = 0; i < 3; i++) { o.strikes[i] = e[i].strikes; o.guard += e[i].guard; }
	return o;
}

// ------------------------------------------------------------ presets through the module chain
struct Preset { std::string name, drv; std::vector<float> p; };
static std::vector<Preset> loadPresets() {
	std::vector<Preset> v; FILE* f = fopen("test/presets.txt", "r"); if (!f) return v;
	char line[8192];
	while (fgets(line, sizeof line, f)) {
		if (line[0] == '#') continue;
		Preset p; char nm[256], dr[256]; int off = 0;
		if (sscanf(line, "%255s %255s%n", nm, dr, &off) < 2) continue;
		p.name = nm; p.drv = dr; std::replace(p.name.begin(), p.name.end(), '_', ' ');
		char* s = line + off; p.p.assign(NUM_P, 0.f);
		for (int i = 0; i < P_FIRE; i++) p.p[i] = (float)strtod(s, &s);
		v.push_back(p);
	}
	fclose(f); return v;
}
static const int NOTES[8] = {0, 0, 12, 0, 3, 0, 7, -2};
struct Run { std::vector<float> st; long guard = 0; bool finite = true; double peak = 0; double clipFrac = 0; };
// Plays a preset with the driver its line names. seconds at 48 kHz.
static Run playPreset(const Preset& pr, double secs, double sr = 48000, double voctFixed = 1e9) {
	Voice v; v.seed = 0x1E000 + std::hash<std::string>()(pr.name) % 1000; v.init(sr);
	Run r; long N = (long)(secs * sr); r.st.assign(2 * N, 0.f);
	double st = 60.0 / 124 / 4; long clip = 0;
	for (long n = 0; n < N; n++) {
		double t = n / sr; Jacks j; std::vector<float> p = pr.p;
		int step = (int)(t / st); double u = t - step * st; int s = step % 16;
		const std::string& d = pr.drv;
		if (d == "touch") j.touch = (t > 0.5 && t < 0.7) ? 10 : 0;
		if (d == "notes" || d == "notesMains") {
			int ns = (int)(t / (2 * st)); double un = t - ns * 2 * st;
			j.voct = voctFixed < 1e8 ? voctFixed : NOTES[ns % 8] / 12.0;
			if (d == "notes") { j.keyPatched = true; j.key = (un < 2 * st * 0.7 && un > 0.0005) ? 10 : 0; }
		}
		if (d == "kick") j.trig = (s % 4 == 0 && u < 0.002) ? 10 : 0;
		if (d == "hat") j.trig = (s % 2 == 1 && u < 0.002) ? 10 : 0;
		if (d == "snare") { double g = ((s == 4 || s == 12) && u < 0.1) ? 5.0 : 0.0; j.cv[CV_VOLTS] = g; j.cv[CV_CURRENT] = g; }
		if (d == "surge") j.surge = (t > 4.0 && t < 4.005) ? 10 : 0;
		if (d == "audio") { // a little drum loop: decaying sine kick and noise snare, +-5 V
			double kt = std::fmod(t, 4 * st), sn = std::fmod(t + 8 * st, 16 * st);
			static unsigned int ns = 1; ns = ns * 1664525u + 1013904223u;
			j.audio = 5.0 * (std::exp(-kt / 0.08) * std::sin(2 * 3.14159265 * 55 * kt * (1 + 2 * std::exp(-kt / 0.02))) + 0.6 * std::exp(-sn / 0.06) * ((ns >> 9) / 4194304.0 - 1.0));
		}
		v.process(p.data(), j);
		if (!std::isfinite(v.L) || !std::isfinite(v.R)) r.finite = false;
		r.st[2 * n] = (float)(v.L / 10.0); r.st[2 * n + 1] = (float)(v.R / 10.0);
		double a = std::max(std::fabs(v.L), std::fabs(v.R)); r.peak = std::max(r.peak, a); if (a > 7.9) clip++;
	}
	r.guard = v.e.guard; r.clipFrac = (double)clip / N; return r;
}

// autocorrelation pitch of the FIELD output, last half second of 'secs'
static double fieldPitch(const Preset& pr, double voct, double f0) {
	Voice v; v.seed = 7; v.init(48000); double sr = 48000; long N = (long)(1.5 * sr); std::vector<double> y(N);
	for (long n = 0; n < N; n++) { Jacks j; j.voct = voct; j.keyPatched = true; j.key = n > 24 ? 10 : 0; v.process(pr.p.data(), j); y[n] = v.field; }
	long s0 = N - 24000, L = 20000; int lo = (int)(sr / (f0 * 1.7)), hi = std::min((int)(sr / (f0 * 0.55)), (int)(s0 - 1)); double best = -1e300; int bl = lo;
	for (int lag = lo; lag <= hi; lag++) { double c = 0; for (long i = 0; i < L; i++) c += y[s0 + i] * y[s0 + i - lag]; if (c > best) { best = c; bl = lag; } }
	// parabolic refinement
	auto ac = [&](int lag) { double c = 0; for (long i = 0; i < L; i++) c += y[s0 + i] * y[s0 + i - lag]; return c; };
	double a = ac(bl - 1), b = best, c = ac(bl + 1), d = 0.5 * (a - c) / (a - 2 * b + c);
	return sr / (bl + d);
}

int main(int argc, char** argv) {
	std::string mode = argc > 1 ? argv[1] : "test";
	auto presets = loadPresets();
	if (presets.empty()) { std::fprintf(stderr, "test/presets.txt missing (python3 tools/make_presets.py)\n"); return 1; }
	if (mode == "tune") {
		for (auto& pr : presets) if (pr.drv == "notes") {
			std::printf("%-14s", pr.name.c_str());
			for (int o = -2; o <= 2; o++) { double f0 = 261.6256 * std::pow(2.0, o); double f = fieldPitch(pr, o, f0); std::printf("  %+d oct: %+6.0f c", o, 1200 * std::log2(f / f0)); }
			std::printf("\n");
		}
		return 0;
	}
	if (mode == "demos") {
		std::string out = argc > 2 ? argv[2] : "test/demos";
		const char* names[5] = {"arc", "sing", "kit", "kit2", "sub"};
		parallelFor(5 + (int)presets.size(), [&](int i) {
			if (i < 5) { auto o = runScene(refscene::scene_id(names[i]), 0, 48000); writeRaw(out + "/scene-" + names[i] + ".f32", o.st); }
			else { auto& pr = presets[i - 5]; auto r = playPreset(pr, pr.drv == "touch" ? 10 : 8); std::string nm = pr.name; std::replace(nm.begin(), nm.end(), ' ', '_');
				writeRaw(out + fmt("/preset-%02d-%s.f32", i - 4, nm.c_str()), r.st); }
		});
		return 0;
	}
	std::string out = argc > 2 ? argv[2] : "test/out", ref = argc > 3 ? argv[3] : "test/ref";

	// 1. mappings: Mapping.hpp is mapping.h; check forward and inverse agree over the range
	{ double worst = 0; for (int i = 0; i <= 100; i++) { double k = i / 100.0;
			double pairs[][2] = {{I_FREQ(M_FREQ(k)), k}, {I_CUR(M_CUR(k)), k}, {I_GAP(M_GAP(k)), k}, {I_COOL(M_COOL(k)), k}, {I_TUNE(M_TUNE(k)), k}, {I_LOAD(M_LOAD(k)), k}, {I_CLIMB(M_CLIMB(k)), k}, {I_ARC(M_ARC(k)), k}};
			for (auto& p : pairs) worst = std::max(worst, std::fabs(p[0] - p[1])); }
		report(worst < 1e-9, "mapping round trip", fmt("worst %.2g", worst)); }

	// 2. port fidelity against the C reference renders
	{ const char* names[2] = {"arc", "kit2"};
		for (auto nm : names) {
			auto c = readRaw(ref + "/" + nm + ".f32");
			if (c.empty()) { report(false, fmt("fidelity %s", nm).c_str(), "no reference render (make test builds it)"); continue; }
			auto o = runScene(refscene::scene_id(nm), 0, 48000);
			long n1 = std::min((long)c.size(), (long)(2 * 48000)); double d1 = 0; for (long i = 0; i < n1; i++) d1 = std::max(d1, (double)std::fabs(c[i] - o.st[i]));
			double dAll = 0; long firstDiff = -1; for (size_t i = 0; i < std::min(c.size(), o.st.size()); i++) { double d = std::fabs(c[i] - o.st[i]); dAll = std::max(dAll, d); if (d > 1e-6 && firstDiff < 0) firstDiff = (long)i / 2; }
			report(d1 < 1e-6 && c.size() == o.st.size(), fmt("fidelity %s", nm).c_str(),
				fmt("max diff first 1 s %.2g, whole scene %.2g%s", d1, dAll, firstDiff >= 0 ? fmt(" (first >1e-6 at %.3f s)", firstDiff / 48000.0).c_str() : ""));
		}
	}

	// 3. fuzz: 8 seeds at 48 k, one each at 44.1, 96 and 192 k
	{ struct F { int seed; double sr; }; std::vector<F> fz = {{1, 48000}, {2, 48000}, {3, 48000}, {4, 48000}, {5, 48000}, {6, 48000}, {7, 48000}, {8, 48000}, {9, 44100}, {10, 96000}, {11, 192000}};
		std::vector<std::string> res(fz.size()); std::atomic<int> bad(0);
		auto t0 = std::chrono::steady_clock::now();
		parallelFor((int)fz.size(), [&](int i) {
			auto o = runScene(-1, fz[i].seed, fz[i].sr); double pk = 0; for (float x : o.st) pk = std::max(pk, (double)std::fabs(x));
			bool ok = o.finite && o.guard == 0 && pk < 3.3;   // three... one instance: 1.6 per side before the x5, plus DC-blocker overshoot
			if (!ok) bad++;
			res[i] = fmt("seed %d @%gk: %s guard %ld peak %.2f", fz[i].seed, fz[i].sr / 1000, o.finite ? "finite" : "NON-FINITE", o.guard, pk);
		});
		double s = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
		for (auto& r : res) std::printf("      %s\n", r.c_str());
		report(bad == 0, "fuzz", fmt("%d runs x 30 s, %d bad (%.0f s wall)", (int)fz.size(), (int)bad, s)); }

	// 4. CPU: one instance of Init at 48 kHz, single thread
	{ Voice v; v.init(48000); Jacks j; auto& p = presets[0].p; auto t0 = std::chrono::steady_clock::now();
		for (long n = 0; n < 48000 * 5; n++) v.process(p.data(), j);
		double s = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
		report(true, "cpu", fmt("Init: %.1f%% of one core on this machine (5 s audio in %.2f s)", 100 * s / 5, s)); }

	// 5. every preset with its driver: not silent, not clipped flat, no guards
	{ std::vector<std::string> res(presets.size()); std::atomic<int> bad(0);
		parallelFor((int)presets.size(), [&](int i) {
			auto& pr = presets[i]; auto r = playPreset(pr, 8);
			double ss = 0; for (float x : r.st) ss += (double)x * x; double rmsV = 10 * std::sqrt(ss / r.st.size());
			bool ok = r.finite && r.guard == 0 && rmsV > 0.05 && r.clipFrac < 0.02;
			if (!ok) bad++;
			res[i] = fmt("%s%-16s %-10s peak %5.2f V  rms %5.2f V  near-clip %.2f%%  guard %ld", ok ? "" : "BAD ", pr.name.c_str(), pr.drv.c_str(), r.peak, rmsV, 100 * r.clipFrac, r.guard);
		});
		for (auto& r : res) std::printf("      %s\n", r.c_str());
		report(bad == 0, "presets", fmt("%d presets, %d bad", (int)presets.size(), (int)bad)); }

	// 6. singing presets track V/OCT: C4 at 0 V within 15 cents, octaves within 40 cents from -2 to +2 (the starved and howling arcs wander)
	{ std::atomic<int> bad(0); std::vector<std::string> res(presets.size());
		parallelFor((int)presets.size(), [&](int i) {
			auto& pr = presets[i]; if (pr.drv != "notes") return;
			std::string s = pr.name; double worst = 0, c0 = 0;
			for (int o = -2; o <= 2; o++) { double f0 = 261.6256 * std::pow(2.0, o); double c = 1200 * std::log2(fieldPitch(pr, o, f0) / f0); if (o == 0) c0 = c; worst = std::max(worst, std::fabs(c)); s += fmt(" %+.0f", c); }
			if (std::fabs(c0) > 15 || worst > 40) bad++;
			res[i] = s;
		});
		std::string all; for (auto& r : res) if (!r.empty()) all += "\n      " + r;
		report(bad == 0, "singing pitch (cents)", all); }

	std::printf("\n%d passed, %d failed\n", nPass, nFail);
	return nFail ? 1 : 0;
}
