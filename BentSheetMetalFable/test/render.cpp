// Bent Sheet Metal offline checks (no Rack needed): make test
// Every check prints PASS or FAIL. See docs/BUILD-INSTRUCTIONS.md section 7.
// Usage: render <outdir> <refdir>            all tests
//        render <outdir> demos               demo WAVs (scenes + preset clips)
//        render <outdir> fuzz <n> <seconds>  only the fuzz test
#include "bridge.h"
#include "../src/dsp/Mapping.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstdarg>
#include <vector>
#include <string>
#include <complex>
#include <chrono>
#include <thread>
#include <atomic>
#include <mutex>
#include <functional>
#include <array>
#include <algorithm>
using namespace bent;

static int nFail = 0, nPass = 0;
static void report(bool ok, const char* name, const std::string& detail) {
	std::printf("%s  %-28s %s\n", ok ? "PASS" : "FAIL", name, detail.c_str()); std::fflush(stdout);
	if (ok) nPass++; else nFail++;
}
static std::string fmt(const char* f, ...) { char b[2048]; va_list a; va_start(a, f); vsnprintf(b, sizeof b, f, a); va_end(a); return b; }
static void parallelFor(int n, const std::function<void(int)>& fn) {
	int nt = std::max(1u, std::thread::hardware_concurrency());
	std::atomic<int> next(0); std::vector<std::thread> th;
	for (int t = 0; t < nt; t++) th.emplace_back([&]() { for (;;) { int i = next++; if (i >= n) break; fn(i); } });
	for (auto& t : th) t.join();
}

// ------------------------------------------------------------------ WAV / raw
static void writeWav(const std::string& path, const std::vector<float>& st, int sr) {
	FILE* f = std::fopen(path.c_str(), "wb"); if (!f) return;
	uint32_t n = st.size() / 2, data = n * 2 * 3;
	auto w32 = [&](uint32_t v) { std::fwrite(&v, 4, 1, f); }; auto w16 = [&](uint16_t v) { std::fwrite(&v, 2, 1, f); };
	std::fwrite("RIFF", 1, 4, f); w32(36 + data); std::fwrite("WAVEfmt ", 1, 8, f); w32(16); w16(1); w16(2); w32(sr); w32(sr * 6); w16(6); w16(24);
	std::fwrite("data", 1, 4, f); w32(data);
	for (float x : st) { int32_t v = (int32_t)std::lround(std::max(-1.0f, std::min(1.0f, x)) * 8388607.0); uint8_t b[3] = {(uint8_t)v, (uint8_t)(v >> 8), (uint8_t)(v >> 16)}; std::fwrite(b, 1, 3, f); }
	std::fclose(f);
}
static std::vector<float> readRaw(const std::string& path) {
	std::vector<float> v; FILE* f = std::fopen(path.c_str(), "rb"); if (!f) return v;
	std::fseek(f, 0, SEEK_END); long n = std::ftell(f); std::fseek(f, 0, SEEK_SET); v.resize(n / 4); size_t got = std::fread(v.data(), 4, v.size(), f); v.resize(got); std::fclose(f); return v;
}
static std::vector<double> mono(const std::vector<float>& st) { std::vector<double> m(st.size() / 2); for (size_t i = 0; i < m.size(); i++) m[i] = 0.5 * ((double)st[2 * i] + st[2 * i + 1]); return m; }

// ------------------------------------------------------------------ spectra
static void fft(std::vector<std::complex<double>>& a) {
	size_t n = a.size();
	for (size_t i = 1, j = 0; i < n; i++) { size_t bit = n >> 1; for (; j & bit; bit >>= 1) j ^= bit; j ^= bit; if (i < j) std::swap(a[i], a[j]); }
	for (size_t len = 2; len <= n; len <<= 1) {
		double ang = -2 * PI / len; std::complex<double> wl(std::cos(ang), std::sin(ang));
		for (size_t i = 0; i < n; i += len) { std::complex<double> w(1); for (size_t j = 0; j < len / 2; j++) { auto u = a[i + j], v = a[i + j + len / 2] * w; a[i + j] = u + v; a[i + j + len / 2] = u - v; w *= wl; } }
	}
}
static std::vector<double> powerSpec(const std::vector<double>& x, long from, long len, int& nfftOut) {
	int nfft = 1; while (nfft * 2 <= len && nfft < (1 << 18)) nfft *= 2;
	std::vector<std::complex<double>> a(nfft);
	for (int i = 0; i < nfft; i++) a[i] = (from + i < (long)x.size() ? x[from + i] : 0.0) * (0.5 - 0.5 * std::cos(2 * PI * i / nfft));
	fft(a); std::vector<double> P(nfft / 2); for (int i = 0; i < nfft / 2; i++) P[i] = std::norm(a[i]);
	nfftOut = nfft; return P;
}
struct Peak { double f, p; };
// spectral peaks (5-bin power sums), strongest first; total = all power
static std::vector<Peak> peaks(const std::vector<double>& x, long from, long len, double sr, double& total, double lo = 20, double hi = 20000) {
	int nfft; auto P = powerSpec(x, from, len, nfft); total = 0; for (double p : P) total += p;
	std::vector<Peak> pk;
	for (int i = 2; i < nfft / 2 - 2; i++) if (P[i] > P[i - 1] && P[i] >= P[i + 1]) {
		double f = i * sr / nfft; if (f < lo || f > hi) continue;
		double l = std::log(P[i - 1] + 1e-300), c = std::log(P[i] + 1e-300), r = std::log(P[i + 1] + 1e-300);
		double d = 0.5 * (l - r) / (l - 2 * c + r);
		pk.push_back({(i + d) * sr / nfft, P[i - 2] + P[i - 1] + P[i] + P[i + 1] + P[i + 2]});
	}
	std::sort(pk.begin(), pk.end(), [](const Peak& a, const Peak& b) { return a.p > b.p; });
	return pk;
}
static double strongest(const std::vector<double>& x, double t0, double t1, double sr, double lo = 20, double hi = 20000) {
	double tot; auto p = peaks(x, (long)(t0 * sr), (long)((t1 - t0) * sr), sr, tot, lo, hi); return p.empty() ? 0 : p[0].f;
}
static std::vector<double> thirdOctaves(const std::vector<double>& x, long from, long len, double sr) {
	std::vector<double> P(4097, 0.0); int frames = 0; const int nfft = 8192;
	for (long s = from; s + nfft <= from + len && s + nfft <= (long)x.size(); s += nfft / 2) {
		std::vector<std::complex<double>> a(nfft);
		for (int i = 0; i < nfft; i++) a[i] = x[s + i] * (0.5 - 0.5 * std::cos(2 * PI * i / nfft));
		fft(a); for (int i = 0; i <= nfft / 2; i++) P[i] += std::norm(a[i]); frames++;
	}
	std::vector<double> b;
	for (double fc = 50; fc <= 16000; fc *= std::pow(2.0, 1.0 / 3)) {
		double lo = fc * std::pow(2.0, -1.0 / 6), hi = fc * std::pow(2.0, 1.0 / 6), s = 0;
		for (int i = 0; i <= nfft / 2; i++) { double f = i * sr / nfft; if (f >= lo && f < hi) s += P[i]; }
		b.push_back(10 * std::log10(s / std::max(frames, 1) + 1e-30));
	}
	return b;
}
static double rmsDb(const std::vector<double>& x, double t0, double t1, double sr) {
	double s = 0; long a = (long)(t0 * sr), b = std::min((long)(t1 * sr), (long)x.size());
	for (long i = a; i < b; i++) s += x[i] * x[i];
	return 10 * std::log10(s / std::max(1L, b - a) + 1e-300);
}
static double cents(double f, double ref) { return 1200 * std::log2(f / ref); }

// ------------------------------------------------------------------ engine-rate runs with fixed controls
struct EngRun { std::vector<double> x; std::vector<double> bend; std::vector<int> snaps; long guards = 0, limits = 0, clamps = 0; bool finite = true; double maxEnergyRise = 0; };
// ctl(t, c) may change controls each block; strikes: list of (time, v, pos)
static EngRun runEngine(double fe, double dur, const std::function<void(double, Controls&)>& ctl,
		std::vector<std::array<double, 3>> strikes = {}, bool trackEnergy = false) {
	Engine* e = new Engine(); e->setRate(fe); e->build();
	EngRun r; long n = (long)(dur * fe); r.x.resize(n); r.bend.resize(n);
	size_t si = 0; double lastE = -1; long lastCheck = 0;
	for (long i = 0; i < n; i++) {
		double t = i / fe;
		if (i % BLK == 0) { ctl(t, e->c); e->block(); }
		while (si < strikes.size() && t >= strikes[si][0]) { e->strike(strikes[si][1], strikes[si][2]); si++; }
		e->process();
		r.x[i] = 0.5 * (e->outL + e->outR); r.bend[i] = e->bend;
		if (e->snapEvent) r.snaps.push_back((int)i);
		if (!std::isfinite(e->outL) || !std::isfinite(e->outR)) r.finite = false;
		if (trackEnergy && i - lastCheck >= (long)(0.01 * fe)) {
			double fe_ = e->fullEnergy();
			if (lastE > 0 && t > strikes.back()[0] + 0.001) r.maxEnergyRise = std::max(r.maxEnergyRise, fe_ / lastE - 1);
			lastE = fe_; lastCheck = i;
		}
	}
	r.guards = e->guards; r.limits = e->limits; r.clamps = e->clamps; delete e; return r;
}

// ------------------------------------------------------------------ reference scenes through the C++ engine
static std::mutex sceneMu;   // the scene scripts use globals
static std::vector<float> runScene(const char* name, double fe, long* guards = nullptr) {
	std::lock_guard<std::mutex> lk(sceneMu);
	refscene::FSR = fe;
	const char* args[2] = {"x", name};
	refscene::scenes_main(2, (char**)args);
	if (guards) *guards = refscene::lastGuards;
	return refscene::lastOut;
}

// ------------------------------------------------------------------ the module chain with random knobs (fuzz)
struct FuzzResult { bool finite = true; long guards = 0, limits = 0, clamps = 0; double peak = 0; };
static FuzzResult fuzzPatch(uint64_t seed, double host, double secs) {
	Rng g; g.s = seed * 2654435761ULL + 12345; for (int i = 0; i < 4; i++) g.u();
	Voice v; v.seed = seed + 1; v.init(host);
	Knobs k; FuzzResult r; long n = (long)(secs * host); long step = (long)(0.5 * host);
	double noiseUntil = -1; Rng ng; ng.s = seed + 99;
	auto randomize = [&]() {
		k.pitch = 6 * g.u() - 3 + (g.u() < 0.3 ? 4 * g.b() : 0);
		k.taper = g.u(); k.grip = g.u(); k.decay = 0.25 + 3.75 * g.u();
		k.bend = g.b(); k.bendVoct = g.u() < 0.3 ? 4 * g.b() : 0; k.shape = g.u(); k.spot = g.b(); k.hold = g.u(); k.snap = g.u(); k.crack = g.u();
		k.dip = g.u() < 0.4 ? 0 : g.u(); k.depth = g.b(); k.bow = g.u() < 0.2 ? 0 : g.u(); k.speed = g.u(); k.bowPos = g.b();
		k.bowGate = g.u() < 0.8 ? 1 : 0; k.hard = g.u(); k.roll = g.u() < 0.6 ? 0 : g.u(); k.inLevel = g.u();
		k.stretch = 3 * g.u(); k.pick = g.u(); k.out = g.u(); k.age = g.u();
	};
	randomize();
	for (long i = 0; i < n; i++) {
		double t = i / host;
		double sv = -1;
		if (i % step == 0) {
			if (g.u() < 0.7) randomize();
			if (g.u() < 0.4) sv = g.u();
			if (g.u() < 0.3) noiseUntil = t + 0.3 * g.u();
		}
		// audio-rate wobble on BEND sometimes
		Knobs kk = k;
		if ((seed % 5) == 0) kk.bend = clampd(k.bend + 0.4 * std::sin(2 * PI * 150 * t), -1, 1);
		double inV = t < noiseUntil ? 10 * ng.b() : 0;
		v.process(kk, sv, inV, 1.0);
		if (!std::isfinite(v.outL) || !std::isfinite(v.outR)) { r.finite = false; break; }
		r.peak = std::max({r.peak, std::fabs(v.outL), std::fabs(v.outR)});
	}
	r.guards = v.e->guards; r.limits = v.e->limits; r.clamps = v.e->clamps;
	return r;
}
static void fuzzTest(const char* name, int patches, double secs, std::vector<double> rates) {
	std::atomic<int> bad(0); std::atomic<long> guards(0), lim(0), clamps(0), withLim(0), withClamp(0); std::mutex mu; std::string first;
	double peak = 0;
	int total = patches * (int)rates.size();
	auto t0 = std::chrono::steady_clock::now();
	parallelFor(total, [&](int i) {
		double host = rates[i % rates.size()];
		FuzzResult r = fuzzPatch(1000 + i, host, secs);
		guards += r.guards; lim += r.limits; clamps += r.clamps; if (r.limits) withLim++; if (r.clamps) withClamp++;
		std::lock_guard<std::mutex> lk(mu); peak = std::max(peak, r.peak);
		if (!r.finite || r.guards || r.peak > 11.0) { bad++; if (first.empty()) first = fmt(" first bad: patch %d at %.1f kHz (finite %d guards %ld peak %.2f)", i, host / 1000, r.finite, r.guards, r.peak); }
	});
	double s = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
	report(bad == 0, name, fmt("%d patches x %.0f s: %d bad, guard events %ld, peak %.2f V; stiffness limiter fired in %ld patches, energy clamp in %ld patches; %.0f s%s",
		total, secs, (int)bad, (long)guards, peak, (long)withLim, (long)withClamp, s, first.c_str()));
}

// ------------------------------------------------------------------ presets (test/presets.txt, written by tools/make_presets.py)
struct PresetDef { std::string name; Knobs k; double level = 0.7; std::string drive; };
static std::vector<PresetDef> loadPresets() {
	std::vector<PresetDef> v; FILE* f = std::fopen("test/presets.txt", "r"); if (!f) return v;
	char line[1024];
	while (std::fgets(line, sizeof line, f)) {
		if (line[0] == '#' || line[0] == '\n') continue;
		char nm[128], drv[128]; double a[26];
		int got = std::sscanf(line, "%127s %127s %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf",
			nm, drv, &a[0], &a[1], &a[2], &a[3], &a[4], &a[5], &a[6], &a[7], &a[8], &a[9], &a[10], &a[11], &a[12], &a[13], &a[14], &a[15], &a[16], &a[17], &a[18], &a[19], &a[20], &a[21], &a[22]);
		if (got < 25) continue;
		PresetDef p; p.name = nm; p.drive = drv; Knobs& k = p.k;
		k.pitch = a[0]; k.taper = a[1]; k.grip = a[2]; k.decay = a[3]; k.bend = a[4]; k.shape = a[5]; k.spot = a[6]; k.hold = a[7]; k.snap = a[8]; k.crack = a[9];
		k.dip = a[10]; k.depth = a[11]; k.bow = a[12]; k.speed = a[13]; k.bowPos = a[14]; k.hard = a[15]; k.roll = a[16]; k.inLevel = a[17];
		k.stretch = a[18]; k.pick = a[19]; k.out = a[20]; k.age = a[21]; p.level = a[22];
		v.push_back(p);
	}
	std::fclose(f); return v;
}
// render a preset through the module chain at 48 kHz; `drive` names the test patching (LFO, trigger, audio)
static std::vector<float> renderPreset(const PresetDef& p, double secs, uint64_t seed, long* guards = nullptr) {
	const double host = 48000; Voice v; v.seed = seed; v.init(host);
	long n = (long)(secs * host); std::vector<float> st(2 * n); Rng g; g.s = seed + 7;
	for (long i = 0; i < n; i++) {
		double t = i / host; Knobs k = p.k; double sv = -1, inV = 0;
		const std::string& d = p.drive;
		if (d.find("lfoDip") != std::string::npos) k.dip = clampd(p.k.dip + 0.3 * std::sin(2 * PI * t / 5.0), 0, 1);
		if (d.find("lfoBend") != std::string::npos) k.bend = clampd(p.k.bend + 0.15 * std::sin(2 * PI * t / 4.0), -1, 1);
		if (d.find("lfoBow") != std::string::npos) k.bow = clampd(p.k.bow + 0.15 * std::sin(2 * PI * t / 2.7), 0, 1);
		if (d.find("wobble") != std::string::npos) k.bend = clampd(p.k.bend + 0.42 * std::sin(2 * PI * 2.5 * t), -1, 1);
		if (d.find("thunder") != std::string::npos) k.bend = clampd(p.k.bend + 0.5 * std::sin(2 * PI * (3.5 + 1.5 * std::sin(2 * PI * t / 3.1)) * t), -1, 1);
		if (d.find("flip") != std::string::npos) k.bend = clampd(p.k.bend + 0.5 * std::sin(2 * PI * 0.4 * t), -1, 1);
		if (d.find("seqSpot") != std::string::npos) { static const double sp[4] = {-0.3, 0.1, 0.35, -0.05}; k.spot = sp[(int)(t / 1.25) % 4]; }
		if (d.find("strike") != std::string::npos) { long per = (long)(2.5 * host); if (i % per == (long)(0.1 * host)) sv = 0.9; }
		if (d.find("audio") != std::string::npos) { double ph = std::fmod(t, 0.5); inV = ph < 0.03 ? 8 * g.b() * std::exp(-ph / 0.008) : 0; }
		v.process(k, sv, inV, p.level);
		st[2 * i] = (float)v.outL; st[2 * i + 1] = (float)v.outR;
	}
	if (guards) *guards = v.e->guards;
	return st;
}

// ------------------------------------------------------------------ demos
static void demos(const std::string& out) {
	const char* scenes[5] = {"alive", "dip", "break", "saw", "flex"};
	for (int s = 0; s < 5; s++) {
		auto raw = runScene(scenes[s], 96000);
		Decimator dec; Leveller lev; lev.setRate(48000);
		long n = raw.size() / 4; std::vector<float> o(2 * n);
		for (long i = 0; i < n; i++) {
			double l2[2] = {raw[4 * i], raw[4 * i + 2]}, r2[2] = {raw[4 * i + 1], raw[4 * i + 3]}, L, R;
			dec.process(l2, r2, L, R); double g = lev.gain(L, R) * 0.7 * OUT_VOLTS;
			o[2 * i] = (float)(11 * std::tanh(L * g / 11) / 11); o[2 * i + 1] = (float)(11 * std::tanh(R * g / 11) / 11);
		}
		writeWav(out + fmt("/scene-%d-%s.wav", s + 1, scenes[s]), o, 48000);
		std::printf("scene %s\n", scenes[s]); std::fflush(stdout);
	}
	auto ps = loadPresets();
	parallelFor((int)ps.size(), [&](int i) {
		auto st = renderPreset(ps[i], 10, 0xB5000 + i);
		for (float& x : st) x /= 11.f;
		std::string nm = ps[i].name; std::string tag = ps[i].drive == "-" ? "" : "_" + ps[i].drive;
		writeWav(out + fmt("/preset-%02d-%s%s.wav", i + 1, nm.c_str(), tag.c_str()), st, 48000);
	});
}

// ONLY=4,15 runs just those tests
static bool want(int n) {
	const char* o = getenv("ONLY"); if (!o) return true;
	std::string s = std::string(",") + o + ","; return s.find("," + std::to_string(n) + ",") != std::string::npos;
}

int main(int argc, char** argv) {
	std::string out = argc > 1 ? argv[1] : "test/out";
	std::string refDir = argc > 2 ? argv[2] : "test/ref";
	if (refDir == "demos") { demos(out); return 0; }
	if (refDir == "fuzz") { fuzzTest("10 fuzz", argc > 3 ? atoi(argv[3]) : 400, argc > 4 ? atof(argv[4]) : 20, {48000}); return nFail ? 1 : 0; }
	const double FE = 96000;

	// ---------------------------------------------------------- 1. reference match
	if (want(1)) {
		const char* names[5] = {"saw", "flex", "dip", "break", "alive"};
		std::vector<std::string> det(5); std::vector<int> ok(5, 1);
		std::vector<std::vector<float>> cpp(5);
		for (int s = 0; s < 5; s++) { long gd = 0; cpp[s] = runScene(names[s], FE, &gd); if (gd) ok[s] = 0; }
		for (int s = 0; s < 5; s++) {
			auto ref = readRaw(refDir + "/raw-" + names[s] + ".f32");
			if (ref.empty()) { ok[s] = 0; det[s] = fmt("%s: no reference", names[s]); continue; }
			auto a = mono(ref), b = mono(cpp[s]);
			long n = std::min(a.size(), b.size());
			int cells = 0, within = 0; double worst = 0;
			for (long w = 0; w + (long)(2 * FE) <= n; w += (long)(2 * FE)) {
				auto ba = thirdOctaves(a, w, (long)(2 * FE), FE), bb = thirdOctaves(b, w, (long)(2 * FE), FE);
				double mx = *std::max_element(ba.begin(), ba.end());
				for (size_t i = 0; i < ba.size(); i++) if (ba[i] > mx - 40) { cells++; double d = std::fabs(ba[i] - bb[i]); if (d <= 2) within++; worst = std::max(worst, d); }
			}
			double frac = cells ? (double)within / cells : 0;
			// Chaotic passages (hard strikes, snaps, the roar) diverge sample by sample, so band levels in a 2 s
			// window wander. The yardstick is the reference against itself built with other compiler flags
			// (test/ref2): the C++ engine must agree with the reference at least that well, less 5 points.
			double base = 1.0;
			auto ref2 = readRaw(refDir + "2/raw-" + names[s] + ".f32");
			if (!ref2.empty()) {
				auto c2 = mono(ref2); int cl = 0, wi = 0;
				for (long w = 0; w + (long)(2 * FE) <= n; w += (long)(2 * FE)) {
					auto ba = thirdOctaves(a, w, (long)(2 * FE), FE), bb = thirdOctaves(c2, w, (long)(2 * FE), FE);
					double mx = *std::max_element(ba.begin(), ba.end());
					for (size_t i = 0; i < ba.size(); i++) if (ba[i] > mx - 40) { cl++; if (std::fabs(ba[i] - bb[i]) <= 2) wi++; }
				}
				base = cl ? (double)wi / cl : 1.0;
			}
			if (frac < std::min(0.9, base - 0.05)) ok[s] = 0;
			det[s] = fmt("%s %.0f%% of bands within 2 dB (reference vs itself %.0f%%)", names[s], 100 * frac, 100 * base);
			if (s == 0) {
				// the pure opening: level and pitch
				double la = rmsDb(a, 1.0, 2.5, FE), lb = rmsDb(b, 1.0, 2.5, FE);
				double fa = strongest(a, 1.0, 2.5, FE), fb = strongest(b, 1.0, 2.5, FE);
				if (std::fabs(la - lb) > 0.2 || std::fabs(cents(fb, fa)) > 2) ok[s] = 0;
				det[s] += fmt(", opening %.3f dB / %.2f cents", lb - la, cents(fb, fa));
			}
		}
		bool all = true; std::string d; for (int s = 0; s < 5; s++) { all = all && ok[s]; d += det[s] + (s < 4 ? "; " : ""); }
		report(all, "1 reference match", d);
	}

	// ---------------------------------------------------------- 2. bend raises pitch (STRETCH 0 so loudness does not)
	if (want(2)) {
		const double hs[5] = {0, 1, 2, 4, 8}; std::vector<double> got(5);
		parallelFor(5, [&](int i) {
			auto r = runEngine(FE, 2.5, [&](double, Controls& c) { c.f1 = 110; c.weMul = std::sqrt(110 / C3); c.shape = 0; c.taper = 0; c.grip = 0; c.hold = 1; c.nl = 0; c.hand = hs[i]; },
				{{{0.2, 5.0, 0.37}}});
			double ex = 110 * std::sqrt(1 + hs[i] * hs[i]);
			got[i] = strongest(r.x, 1.0, 2.4, FE, 0.85 * ex, 1.15 * ex);
		});
		bool ok = true; std::string d;
		for (int i = 0; i < 5; i++) { double ex = 110 * std::sqrt(1 + hs[i] * hs[i]); double e = 100 * (got[i] / ex - 1); if (std::fabs(e) > 2) ok = false; d += fmt("bend %g: %.1f Hz (%+.1f%%) ", hs[i], got[i], e); }
		report(ok, "2 bend raises pitch", d);
	}

	// ---------------------------------------------------------- 3. trapping
	if (want(3)) {
		double rate[2];
		parallelFor(2, [&](int i) {
			double h = i == 0 ? 4 : 0;
			auto r = runEngine(FE, 4.5, [&](double, Controls& c) { c.f1 = 110; c.weMul = std::sqrt(110 / C3); c.shape = 1; c.taper = 0; c.grip = 1; c.hold = 1; c.hand = h; },
				{{{0.2, 30.0, 0.37}}});
			rate[i] = (rmsDb(r.x, 3.0, 3.25, FE) - rmsDb(r.x, 4.0, 4.25, FE)) / 1.0;
		});
		report(rate[0] < 8 && rate[1] > 25, "3 trapping", fmt("S-bend decays %.1f dB/s (need < 8), flat decays %.1f dB/s (need > 25)", rate[0], rate[1]));
	}

	// ---------------------------------------------------------- 4. saw tuning
	if (want(4)) {
		auto st = runScene("saw", FE); auto x = mono(st);
		const double tg[6] = {220, 329.63, 293.66, 261.63, 220, 196}, du[6] = {2.0, 2.0, 1.5, 1.5, 2.0, 1.5};
		double a = 0.8, worst = 0; std::string d;
		for (int i = 0; i < 6; i++) { double f = strongest(x, a + du[i] * 0.5, a + du[i] * 0.5 + 0.4, FE); double c = cents(f, tg[i]); worst = std::max(worst, std::fabs(c)); d += fmt("%+.0f ", c); a += du[i]; }
		report(worst <= 15, "4 saw tuning", "cents: " + d);
	}

	// ---------------------------------------------------------- 5. bow regimes
	if (want(5)) {
		double share = 0, f02 = 0, f1 = 0; int near = 0;
		parallelFor(2, [&](int i) {
			auto r = runEngine(FE, 5, [&](double, Controls& c) { c.f1 = 110; c.weMul = std::sqrt(110 / C3); c.shape = 1; c.taper = 0; c.grip = 1; c.hold = 0.9; c.hand = 4;
				c.bowP = i == 0 ? 0.2 : 1.0; c.bowV = i == 0 ? 0.2 : 1.0; });
			double tot; auto p = peaks(r.x, (long)(3 * FE), (long)(2 * FE), FE, tot);
			if (i == 0) { share = p[0].p / tot; f02 = p[0].f; }
			else { f1 = p[0].f; for (auto& q : p) if (q.p > p[0].p * std::pow(10, -1.2)) near++; }
		});
		report(share > 0.97 && near >= 3 && cents(f1, f02) > 100, "5 bow regimes",
			fmt("light bow: %.1f%% in one partial at %.1f Hz; heavy bow: %d partials within 12 dB, strongest %.1f Hz (%+.0f cents)", 100 * share, f02, near, f1, cents(f1, f02)));
	}

	// ---------------------------------------------------------- 6. energy never rises after a strike (no bow, bend held, dry)
	if (want(6)) {
		const double nls[3] = {0, 1, 3}; double rise[3]; long gd[3];
		parallelFor(3, [&](int i) {
			auto r = runEngine(FE, 3, [&](double, Controls& c) { c.f1 = 110; c.weMul = std::sqrt(110 / C3); c.shape = 1; c.grip = 0.3; c.hold = 1; c.snap = 0; c.hand = 4; c.nl = nls[i]; c.kick = 0; c.crack = 0; c.back = 0; },
				{{{0.2, 60.0, 0.37}}}, true);
			rise[i] = r.maxEnergyRise; gd[i] = r.guards;
		});
		// The bend is held still (no back-reaction), so the sheet is a fixed shape: the stretching may move
		// energy between modes but never add it. (With the bend free, the sound and the bend trade energy,
		// which is the point of the flex, so that case is not checked here.)
		bool ok = true; std::string d;
		for (int i = 0; i < 3; i++) { if (rise[i] > 1e-3 || gd[i]) ok = false; d += fmt("STRETCH %g: max rise %.3f%% ", nls[i], 100 * rise[i]); }
		report(ok, "6 energy", d);
	}

	// ---------------------------------------------------------- 7. snap
	if (want(7)) {
		double crossRate = 0, trigs = 0, lvl[2] = {0, 0};
		parallelFor(2, [&](int i) {
			auto r = runEngine(FE, 6, [&](double t, Controls& c) { c.f1 = 130.81; c.shape = 0; c.hold = 0.2; c.snap = 0.55; c.grip = 0.2; c.hand = 3 * std::sin(2 * PI * 2.5 * t); c.crack = i == 0 ? 3 * 0.35 * 0.35 : 0; });
			if (i == 0) {
				int cross = 0; for (size_t k = (size_t)(2 * FE); k < r.bend.size(); k++) if ((r.bend[k - 1] < 0) != (r.bend[k] < 0)) cross++;
				crossRate = cross / 4.0; int tr = 0; for (int s : r.snaps) if (s > 2 * FE) tr++; trigs = tr / 4.0;
			}
			// broadband level above 2 kHz
			std::vector<double> hp(r.x.size()); double lp = 0; for (size_t k = 0; k < r.x.size(); k++) { lp += (r.x[k] - lp) * 0.13; hp[k] = r.x[k] - lp; }
			lvl[i] = rmsDb(hp, 2, 6, FE);
		});
		report(crossRate >= 4 && trigs >= 0.9 * crossRate && lvl[0] - lvl[1] > 1, "7 snap",
			fmt("%.1f crossings/s, %.1f SNAP triggers/s, CRACK adds %.1f dB above 2 kHz", crossRate, trigs, lvl[0] - lvl[1]));
	}

	// ---------------------------------------------------------- 8. water
	if (want(8)) {
		// bowed saw note (as the dip scene's second half), DIP swept 0 -> 0.6 -> 0
		double f[3][2];
		parallelFor(2, [&](int i) {
			double sg = i == 0 ? 1 : -1;
			auto r = runEngine(FE, 9, [&](double t, Controls& c) { c.f1 = 98; c.weMul = std::sqrt(98 / C3); c.shape = 1; c.taper = 1; c.grip = 1; c.hold = 0.9; c.spot = 0.075; c.hand = 6.591; c.bowX = 0.5375;
				c.bowP = 0.24; c.bowV = 0.2; c.depth = sg * 0.8; c.dip = 0.6 * refscene::seg(t, 3, 4) - 0.6 * refscene::seg(t, 6, 7); });
			f[0][i] = strongest(r.x, 2.0, 2.9, FE, 150, 500); f[1][i] = strongest(r.x, 4.6, 5.6, FE, 100, 700); f[2][i] = strongest(r.x, 8.0, 8.9, FE, 150, 500);
		});
		double drop = -cents(f[1][0], f[0][0]) / 100, back = cents(f[2][0], f[0][0]), rise = cents(f[1][1], f[0][1]) / 100;
		report(drop >= 2 && drop <= 6 && std::fabs(back) < 20 && rise > 0.3, "8 water",
			fmt("dipped: %.1f semitones down, back within %.0f cents; lighter than air: %+.1f semitones", drop, back, rise));
	}

	// ---------------------------------------------------------- 9. STRETCH (pitch glide of a hard-struck trapped note)
	if (want(9)) {
		const double nls[3] = {0, 1, 3}; double rise[3];
		parallelFor(3, [&](int i) {
			auto r = runEngine(FE, 5, [&](double, Controls& c) { c.f1 = 110; c.weMul = std::sqrt(110 / C3); c.shape = 1; c.taper = 0; c.grip = 1; c.hold = 1; c.hand = 4; c.nl = nls[i]; },
				{{{0.1, 60.0, 0.37}}});
			double late = strongest(r.x, 3.5, 4.8, FE, 120, 200);
			double early = strongest(r.x, 0.25, 0.6, FE, late * 0.97, late * 1.5);
			rise[i] = cents(early, late);
		});
		report(std::fabs(rise[0]) < 20 && rise[1] > 100 && rise[2] > rise[1], "9 STRETCH",
			fmt("loud-to-quiet glide: %.0f cents at 0, %.0f at 1, %.0f at 3", rise[0], rise[1], rise[2]));
	}

	// ---------------------------------------------------------- 10, 11. fuzz and sample rates
	int fuzzN = getenv("FUZZ_N") ? atoi(getenv("FUZZ_N")) : 400;
	double fuzzS = getenv("FUZZ_S") ? atof(getenv("FUZZ_S")) : 20;
	if (want(10)) fuzzTest("10 fuzz (48 kHz)", fuzzN, fuzzS, {48000});
	if (want(11)) fuzzTest("11 fuzz at 44.1/48/96 kHz", std::max(1, fuzzN / 4), fuzzS / 2, {44100, 48000, 96000});
	if (want(11)) {
		// tests 2 and 4 at engine rate 88.2 kHz (44.1 host) as well
		auto r = runEngine(88200, 2.5, [&](double, Controls& c) { c.f1 = 110; c.weMul = std::sqrt(110 / C3); c.shape = 0; c.taper = 0; c.grip = 0; c.hold = 1; c.nl = 0; c.hand = 4; }, {{{0.2, 5.0, 0.37}}});
		double f = strongest(r.x, 1.0, 2.4, 88200, 0.85 * 453.5, 1.15 * 453.5);
		auto st = runScene("saw", 88200); auto x = mono(st);
		const double tg[6] = {220, 329.63, 293.66, 261.63, 220, 196}, du[6] = {2.0, 2.0, 1.5, 1.5, 2.0, 1.5};
		double a = 0.8, worst = 0; for (int i = 0; i < 6; i++) { double ff = strongest(x, a + du[i] * 0.5, a + du[i] * 0.5 + 0.4, 88200); worst = std::max(worst, std::fabs(cents(ff, tg[i]))); a += du[i]; }
		report(std::fabs(f / 453.5 - 1) < 0.02 && worst <= 15, "11 tests 2 and 4 at 88.2 kHz", fmt("bend 4: %.1f Hz; saw worst %.0f cents", f, worst));
	}

	// ---------------------------------------------------------- 12. silence
	if (want(12)) {
		double pk[2] = {0, 0};
		parallelFor(2, [&](int i) {
			Voice v; v.init(48000); Knobs k; k.bow = 0; k.age = i == 0 ? 0 : 0.3;
			for (long n = 0; n < 61 * 48000L; n++) { v.process(k, -1, 0, 0.7); if (n > 60 * 48000L) pk[i] = std::max({pk[i], std::fabs(v.outL), std::fabs(v.outR)}); }
		});
		report(pk[0] == 0 && pk[1] < 1e-3, "12 silence", fmt("after 60 s: peak %.3g V at AGE 0, %.3g V at AGE 0.3 (tired hand)", pk[0], pk[1]));
	}

	// ---------------------------------------------------------- 13. audio-rate bend
	if (want(13)) {
		Voice v; v.init(48000); Knobs k; k.bow = 0.5; k.speed = 0.6; bool fin = true; double pk = 0;
		for (long n = 0; n < 10 * 48000L; n++) { Knobs kk = k; kk.bend = clampd(k.bend + 0.4 * std::sin(2 * PI * 200 * n / 48000.0), -1, 1); v.process(kk, -1, 0, 0.7);
			if (!std::isfinite(v.outL)) fin = false; pk = std::max(pk, std::fabs(v.outL)); }
		report(fin && v.e->guards == 0, "13 audio-rate bend", fmt("200 Hz +-2 V on BEND under a bowed tone: guards %ld, peak %.1f V", v.e->guards, pk));
	}

	// ---------------------------------------------------------- 14. speed
	if (want(14)) {
		auto cost = [&](double dip) {
			Voice v; v.init(48000); Knobs k; k.bow = 0.8; k.speed = 0.7; k.dip = dip; long n = 4 * 48000;
			auto t0 = std::chrono::steady_clock::now();
			for (long i = 0; i < n; i++) v.process(k, -1, 0, 0.7);
			return std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count() / 4.0;
		};
		double a = cost(0), b = cost(0.5);
		report(true, "14 speed", fmt("%.0f%% of one core at 48 kHz host (bowed roar, dry), %.0f%% with DIP 0.5 (this machine)", 100 * a, 100 * b));
	}

	// ---------------------------------------------------------- 15. presets through the module chain
	if (want(15)) {
		auto ps = loadPresets();
		if (ps.empty()) report(false, "15 presets", "no test/presets.txt (run tools/make_presets.py)");
		else {
			std::vector<std::string> det(ps.size()); std::vector<int> ok(ps.size(), 1); double initPeak = 0;
			parallelFor((int)ps.size(), [&](int i) {
				long gd = 0; auto st = renderPreset(ps[i], 10, 0xB5000 + i, &gd); auto x = mono(st);
				double l = rmsDb(x, 3, 10, 48000) / 2; double rms = std::pow(10.0, rmsDb(x, 3, 10, 48000) / 20);
				double tot; auto p = peaks(x, 3 * 48000, 7 * 48000, 48000, tot); double share = p.empty() ? 1 : p[0].p / tot;
				double pk = 0; for (size_t q = 6 * 48000; q < st.size(); q++) pk = std::max(pk, (double)std::fabs(st[q]));
				if (i == 0) initPeak = pk;
				bool good = rms > 0.3 && gd == 0;
				const std::string& n = ps[i].name;
				if ((n == "Alive" || n == "Dragged_Under" || n == "Break-up" || n == "Overbowed" || n == "Scream") && share > 0.9) good = false;
				ok[i] = good; (void)l;
				det[i] = fmt("%s%s %.1f V rms, top partial %.0f%%", good ? "" : "BAD ", n.c_str(), rms, 100 * share);
			});
			bool all = true; std::string d; for (size_t i = 0; i < ps.size(); i++) { all = all && ok[i]; d += det[i] + "; "; }
			report(all, "15 presets", d);
			report(initPeak > 3.5 && initPeak < 7, "16 Init level", fmt("Init peaks at %.2f V (aim 5 V)", initPeak));
		}
	}

	std::printf("\n%d passed, %d failed\n", nPass, nFail);
	return nFail ? 1 : 0;
}
