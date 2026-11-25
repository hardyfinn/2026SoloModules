// Long Metal offline checks (no Rack needed): make test
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
#include <algorithm>
#include <fstream>
#include <sstream>
#include <map>
#include <memory>
#if defined(__x86_64__)
#include <xmmintrin.h>
#include <pmmintrin.h>
#endif
using namespace longmetal;

static int nFail = 0, nPass = 0;
static void report(bool ok, const char* name, const std::string& detail) {
	std::printf("%s  %-26s %s\n", ok ? "PASS" : "FAIL", name, detail.c_str()); std::fflush(stdout);
	if (ok) nPass++; else nFail++;
}
static void info(const char* name, const std::string& detail) { std::printf("INFO  %-26s %s\n", name, detail.c_str()); std::fflush(stdout); }
static std::string fmt(const char* f, ...) { char b[4096]; va_list a; va_start(a, f); vsnprintf(b, sizeof b, f, a); va_end(a); return b; }
static void ftz() {
#if defined(__x86_64__)
	_MM_SET_FLUSH_ZERO_MODE(_MM_FLUSH_ZERO_ON); _MM_SET_DENORMALS_ZERO_MODE(_MM_DENORMALS_ZERO_ON);
#endif
}
static void parallelFor(int n, const std::function<void(int)>& fn) {
	int nt = std::max(1u, std::thread::hardware_concurrency());
	std::atomic<int> next(0); std::vector<std::thread> th;
	for (int t = 0; t < nt; t++) th.emplace_back([&]() { ftz(); for (;;) { int i = next++; if (i >= n) break; fn(i); } });
	for (auto& t : th) t.join();
}

// ------------------------------------------------------------------ WAV / raw
static void writeWav(const std::string& path, const std::vector<float>& st, int sr) {
	FILE* f = std::fopen(path.c_str(), "wb"); if (!f) return;
	uint32_t n = st.size() / 2, data = n * 2 * 2;
	auto w32 = [&](uint32_t v) { std::fwrite(&v, 4, 1, f); }; auto w16 = [&](uint16_t v) { std::fwrite(&v, 2, 1, f); };
	std::fwrite("RIFF", 1, 4, f); w32(36 + data); std::fwrite("WAVEfmt ", 1, 8, f); w32(16); w16(1); w16(2); w32(sr); w32(sr * 4); w16(4); w16(16);
	std::fwrite("data", 1, 4, f); w32(data);
	for (float x : st) { int16_t v = (int16_t)std::lround(std::max(-1.0f, std::min(1.0f, x)) * 32767.0); w16((uint16_t)v); }
	std::fclose(f);
}
static std::vector<float> readRaw(const std::string& path) {
	std::vector<float> v; FILE* f = std::fopen(path.c_str(), "rb"); if (!f) return v;
	std::fseek(f, 0, SEEK_END); long n = std::ftell(f); std::fseek(f, 0, SEEK_SET); v.resize(n / 4); size_t got = std::fread(v.data(), 4, v.size(), f); v.resize(got); std::fclose(f); return v;
}
static std::vector<double> mono(const std::vector<float>& st) { std::vector<double> m(st.size() / 2); for (size_t i = 0; i < m.size(); i++) m[i] = 0.5 * ((double)st[2 * i] + st[2 * i + 1]); return m; }
static std::vector<double> chan(const std::vector<float>& st, int c) { std::vector<double> m(st.size() / 2); for (size_t i = 0; i < m.size(); i++) m[i] = st[2 * i + c]; return m; }

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
	for (int i = 0; i < nfft; i++) a[i] = (from + i < (long)x.size() && from + i >= 0 ? x[from + i] : 0.0) * (0.5 - 0.5 * std::cos(2 * PI * i / nfft));
	fft(a); std::vector<double> P(nfft / 2); for (int i = 0; i < nfft / 2; i++) P[i] = std::norm(a[i]);
	nfftOut = nfft; return P;
}
struct Peak { double f, p; };
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
// strongest peak inside [lo, hi]
static double peakIn(const std::vector<double>& x, long from, long len, double sr, double lo, double hi) {
	double tot; auto p = peaks(x, from, len, sr, tot, lo, hi); return p.empty() ? 0 : p[0].f;
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
	double s = 0; long a = std::max(0L, (long)(t0 * sr)), b = std::min((long)(t1 * sr), (long)x.size());
	for (long i = a; i < b; i++) s += x[i] * x[i];
	return 10 * std::log10(s / std::max(1L, b - a) + 1e-300);
}
static double peakAbs(const std::vector<double>& x, double t0, double t1, double sr) {
	double m = 0; long a = std::max(0L, (long)(t0 * sr)), b = std::min((long)(t1 * sr), (long)x.size());
	for (long i = a; i < b; i++) m = std::max(m, std::fabs(x[i])); return m;
}
static double cents(double f, double ref) { return 1200 * std::log2(f / ref); }

// ------------------------------------------------------------------ the full module chain
struct Drive {   // what is patched, per host sample
	std::function<void(double t, Knobs& k, double& strikeVel, bool& pass, double& in)> fn;
};
struct ChainResult { std::vector<float> st; long guard = 0; bool finite = true; double maxAbs = 0; double secs = 0; std::vector<float> env; std::vector<char> clack, gate; };
static ChainResult runChain(const Knobs& k0, double host, double dur, const Drive* d = nullptr, bool keepAux = false) {
	Voice v; v.init(host);
	ChainResult r; long n = (long)(dur * host); r.st.resize(2 * n);
	if (keepAux) { r.env.resize(n); r.clack.resize(n); r.gate.resize(n); }
	auto t0 = std::chrono::steady_clock::now();
	for (long i = 0; i < n; i++) {
		Knobs k = k0; double sv = -1, in = 0; bool pass = false;
		if (d) d->fn(i / host, k, sv, pass, in);
		v.process(k, sv, pass, in);
		r.st[2 * i] = (float)v.outL; r.st[2 * i + 1] = (float)v.outR;
		if (!std::isfinite(v.outL) || !std::isfinite(v.outR)) r.finite = false;
		r.maxAbs = std::max(r.maxAbs, std::max(std::fabs(v.outL), std::fabs(v.outR)));
		if (keepAux) { r.env[i] = (float)v.env; r.clack[i] = v.clack; r.gate[i] = v.passGate; }
	}
	r.secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
	r.guard = v.e->guard;
	return r;
}

// ------------------------------------------------------------------ presets
struct Preset { std::string name, drive; std::map<std::string, double> v; };
static std::vector<Preset> loadPresets(const std::string& path) {
	std::vector<Preset> out; std::ifstream f(path); std::string line; std::vector<std::string> ids;
	while (std::getline(f, line)) {
		std::istringstream ss(line); std::string a, b; ss >> a >> b;
		if (a == "#") { ids.clear(); std::string s; ss >> s; /* "drive" */ while (ss >> s) ids.push_back(s); continue; }
		if (a.empty()) continue;
		Preset p; p.name = a; p.drive = b; double x;
		for (auto& id : ids) { ss >> x; p.v[id] = x; }
		out.push_back(p);
	}
	return out;
}
static Knobs knobsFrom(const Preset& p) {
	Knobs k; auto g = [&](const char* n) { return p.v.at(n); };
	k.pitch = g("PITCH"); k.length = g("LENGTH"); k.spacing = g("SPACING"); k.heavy = g("GAUGE") > 0.5;
	k.stiff = g("STIFF"); k.supports = g("SUPPORTS"); k.endA = g("ENDA"); k.endB = g("ENDB"); k.ground = g("GROUND"); k.bright = g("BRIGHT");
	k.age = g("AGE"); k.live = g("LIVE"); k.distance = g("DISTANCE"); k.listen = g("LISTEN"); k.passTime = g("PASSTIME");
	k.warp = g("WARP"); k.swell = g("SWELL"); k.loose = g("LOOSE"); k.grip = g("GRIP"); k.train = g("TRAIN"); k.speed = g("SPEED");
	k.flats = g("FLATS"); k.hard = g("HARD"); k.inLevel = g("INLEVEL"); k.even = g("EVEN"); k.width = g("WIDTH"); k.level = g("LEVEL");
	return k;
}
// how the tests and demos patch each preset
static Drive driveFor(const std::string& drive, double host) {
	Drive d;
	bool pass = drive.find("pass") != std::string::npos, strike = drive.find("strike") != std::string::npos, softHard = drive.find("SoftHard") != std::string::npos;
	bool lfoStiff = drive.find("lfoStiff") != std::string::npos, lfoSup = drive.find("lfoSupports") != std::string::npos;
	bool gate = drive.find("gate") != std::string::npos, audio = drive.find("audio") != std::string::npos, seq = drive.find("seqVoct") != std::string::npos;
	auto rs = std::make_shared<uint64_t>(12345);
	auto pink = std::make_shared<std::vector<double>>(7, 0.0);
	d.fn = [=](double t, Knobs& k, double& sv, bool& ps, double& in) {
		long i = (long)std::llround(t * host);
		if (pass && i == (long)(0.5 * host)) ps = true;
		if (strike) {
			// hits every 1.25 s, velocity cycling; the soft/hard variant alternates a tap and a hard hit
			const double per = 1.25; long ip = (long)std::llround(per * host);
			if (i % ip == (long)(0.1 * host)) {
				int n = (int)(i / ip);
				if (softHard) sv = (n % 2 == 0) ? 0.15 : 0.95;
				else { const double vs[4] = {0.55, 0.8, 0.65, 0.95}; sv = vs[n % 4]; }
			}
		}
		if (seq && i % (long)(0.5 * host) == (long)(0.05 * host)) { const double notes[8] = {0, 3, 7, 10, 12, 7, 5, -2}; sv = 0.7; (void)notes; }
		if (seq) { const double notes[8] = {0, 3, 7, 10, 12, 7, 5, -2}; k.pitch += notes[((long)(t / 0.5)) % 8] / 12.0; }
		if (lfoStiff) k.stiff = clampd(k.stiff + 0.12 * std::sin(2 * PI * t / 9.0), 0, 1);
		if (lfoSup) k.supports = clampd(k.supports + 0.12 * std::sin(2 * PI * t / 5.5), 0, 1);
		if (gate) k.live = clampd(k.live + ((std::fmod(t * 3.0, 1.0) < 0.5) ? 0.5 : 0.0), 0, 1);
		if (audio) {
			// pink-ish noise in 0.4 s bursts, 5 V
			uint64_t& s = *rs; s ^= s << 13; s ^= s >> 7; s ^= s << 17; double w = (s >> 11) * (2.0 / 9007199254740992.0) - 1;
			auto& b = *pink; b[0] = 0.99886 * b[0] + w * 0.0555179; b[1] = 0.99332 * b[1] + w * 0.0750759; b[2] = 0.96900 * b[2] + w * 0.1538520;
			b[3] = 0.86650 * b[3] + w * 0.3104856; b[4] = 0.55000 * b[4] + w * 0.5329522; b[5] = -0.7616 * b[5] - w * 0.0168980;
			double pk = b[0] + b[1] + b[2] + b[3] + b[4] + b[5] + b[6] + w * 0.5362; b[6] = w * 0.115926;
			in = (std::fmod(t, 2.0) < 0.4) ? 5.0 * pk / 3.0 : 0.0;
		}
	};
	return d;
}

// ------------------------------------------------------------------ engine-level runs (test rail, cable)
struct EngSetup { int N = 256, M = 4, d = 2; double a0 = -0.9, ties = 0, rho = -0.95, g = 0.9995, hidamp = 0.002, hif = 5000, pick = 20; };
static std::vector<double> strikeRun(const EngSetup& s, double dur, double pos = 230, double amp = 1, double ms = 0.1) {
	Engine* e = new Engine(); e->line_init();
	Params& p = e->p; p.N = s.N; p.M = s.M; p.d = s.d; p.a0 = s.a0; p.ties = s.ties; p.rhoL = p.rhoR = s.rho; p.g = s.g; p.hidamp = s.hidamp; p.hif = s.hif; p.pick = s.pick;
	long n = (long)(dur * 48000); std::vector<double> x(n);
	for (long t = 0; t < n; t++) {
		if (t == (long)(0.05 * 48000)) e->strike(pos, amp, ms);
		e->strike_step(); double l, r; e->line_step(&l, &r); x[t] = l + r;
	}
	delete e; return x;
}
// first-arrival time of frequency f: STFT (512 window, hop 64, frames centred on k*hop), argmax over the first second
static double arrivalMs(const std::vector<double>& x, double f, double strikeT = 0.05) {
	const int W = 512, hop = 64; double best = -1; int bestK = 0;
	for (int k = 0; k * hop < 48000; k++) {
		std::complex<double> acc(0);
		for (int i = 0; i < W; i++) { long j = (long)k * hop - W / 2 + i; double v = (j >= 0 && j < (long)x.size()) ? x[j] : 0.0;
			acc += v * (0.5 - 0.5 * std::cos(2 * PI * i / W)) * std::polar(1.0, -2 * PI * f * i / 48000.0); }
		double m = std::norm(acc); if (m > best) { best = m; bestK = k; }
	}
	return bestK * hop / 48.0 - strikeT * 1000;
}

// ------------------------------------------------------------------ tests
static void test1(const std::string& refdir) {
	struct S { const char* name; double chaosFrom, chaosTo; };
	const S scenes[5] = {{"rail", 1e9, 1e9}, {"zaps", 20, 30}, {"hard", 8, 36}, {"live", 14, 32}, {"live2", 14, 44}};
	for (const S& sc : scenes) {
		auto ref = readRaw(refdir + "/" + sc.name + ".f32");
		if (ref.empty()) { report(false, "1 reference match", fmt("%s: no reference render", sc.name)); continue; }
		auto r = refscene::run(sc.name);
		auto a = mono(ref), b = mono(r.st);
		long n = std::min(a.size(), b.size()); const double sr = 48000;
		double worst = 0, worstChaos = 0; int windows = (int)(n / (2 * sr));
		double pk = -1e9; std::vector<std::vector<double>> A(windows), B(windows);
		for (int w = 0; w < windows; w++) { A[w] = thirdOctaves(a, (long)(w * 2 * sr), (long)(2 * sr), sr); B[w] = thirdOctaves(b, (long)(w * 2 * sr), (long)(2 * sr), sr); for (double v : A[w]) pk = std::max(pk, v); }
		std::vector<double> chA(A.empty() ? 0 : A[0].size(), 0.0), chB = chA; int nch = 0;
		for (int w = 0; w < windows; w++) {
			bool chaos = w * 2 >= sc.chaosFrom && w * 2 < sc.chaosTo;
			if (chaos) { for (size_t j = 0; j < chA.size(); j++) { chA[j] += std::pow(10, A[w][j] / 10); chB[j] += std::pow(10, B[w][j] / 10); } nch++; continue; }
			for (size_t j = 0; j < A[w].size(); j++) if (A[w][j] > pk - 40) worst = std::max(worst, std::fabs(A[w][j] - B[w][j]));
		}
		if (nch) { double cpk = -1e9; for (double v : chA) cpk = std::max(cpk, 10 * std::log10(v / nch));
			for (size_t j = 0; j < chA.size(); j++) { double x = 10 * std::log10(chA[j] / nch), y = 10 * std::log10(chB[j] / nch); if (x > cpk - 40) worstChaos = std::max(worstChaos, std::fabs(x - y)); } }
		double err5 = 0, mx5 = 1e-30; for (long i = 0; i < (long)(5 * sr) && i < n; i++) { err5 = std::max(err5, std::fabs(a[i] - b[i])); mx5 = std::max(mx5, std::fabs(a[i])); }
		bool ok = worst <= 2.0 && worstChaos <= 3.0 && r.guard == 0 && (std::string(sc.name) != "zaps" || err5 / mx5 < 0.06);
		report(ok, "1 reference match", fmt("%-5s steady windows within %.2f dB, chaotic part (averaged) within %.2f dB, first 5 s sample error %.1e, guard %ld",
			sc.name, worst, worstChaos, err5 / mx5, r.guard));
	}
}
static void test2() {
	auto r = refscene::run("lossless");
	double dev = (r.Emax - r.Emin) / r.E0;
	report(dev < 1e-3 && r.guard == 0, "2 lossless", fmt("WARP, SWELL, LOOSE on, STIFF and SUPPORTS moving: energy spread %.2e over 10 s", dev));
}
static void test3() {
	EngSetup s; auto x = strikeRun(s, 1.2);
	double t8 = arrivalMs(x, 8000), t1 = arrivalMs(x, 1000), t2 = arrivalMs(x, 2000), t05 = arrivalMs(x, 500);
	report(std::fabs(t8 - 21) <= 5 && std::fabs(t1 - 177) <= 20, "3 zing", fmt("strike 210 cells away: 8 kHz at %.0f ms, 2 kHz %.0f, 1 kHz %.0f, 500 Hz %.0f", t8, t2, t1, t05));
}
static std::vector<double> testRailTones(const std::vector<double>& x, double t0, double t1, double sr, double scale = 1) {
	const double want[4] = {850, 1800, 3000, 4400}; std::vector<double> got;
	for (double w : want) { w *= scale; got.push_back(peakIn(x, (long)(t0 * sr), (long)((t1 - t0) * sr), sr, w * 0.94, w * 1.06)); }
	return got;
}
static void test4() {
	EngSetup s; s.ties = 0.12; s.d = 10; s.M = 3; s.a0 = -0.7; s.rho = 0;
	auto x = strikeRun(s, 3.2);
	auto g = testRailTones(x, 1.0, 3.0, 48000);
	const double want[4] = {850, 1800, 3000, 4400}; bool ok = true; std::string d;
	for (int i = 0; i < 4; i++) { double e = std::fabs(g[i] / want[i] - 1); if (e > 0.03) ok = false; d += fmt("%.0f (%.1f%%) ", g[i], 100 * e); }
	report(ok, "4 support tones", "test rail tones near 850/1800/3000/4400 Hz: " + d);
}
static void test5() {
	auto r = refscene::run("rail");
	auto m = mono(r.st), L = chan(r.st, 0), R = chan(r.st, 1);
	double rise = rmsDb(m, 26, 28, 48000) - rmsDb(m, 2, 4, 48000);
	double sxy = 0, sxx = 0, syy = 0; for (long i = 2 * 48000; i < 26 * 48000; i++) { sxy += L[i] * R[i]; sxx += L[i] * L[i]; syy += R[i] * R[i]; }
	double corr = sxy / std::sqrt(sxx * syy);
	report(rise >= 25 && rise <= 45 && std::fabs(corr) < 0.2, "5 approach", fmt("level 26-28 s is %.1f dB above 2-4 s; L/R correlation %.3f", rise, corr));
}
static Knobs liveKnobs() { Knobs k; k.train = 0; k.live = 0.74; return k; }
static void test6() {
	// Init with LIVE 0.74, then LIVE 0.3. Init's strong supports make the singing line nearly two pure tones, so the
	// "at least three peaks" check runs on the Live Line preset's line instead (README, deviations).
	Knobs k = liveKnobs(); k.even = 0;
	Drive d; d.fn = [](double t, Knobs& kk, double&, bool&, double&) { if (t >= 10) kk.live = 0.3; };
	auto r = runChain(k, 48000, 20, &d);
	auto m = mono(r.st);
	double on = rmsDb(m, 4, 5, 48000), off = rmsDb(m, 19, 20, 48000);
	double tot; auto pk = peaks(m, 6 * 48000, 3 * 48000, 48000, tot, 50, 18000);
	int initPeaks = 0; for (auto& p : pk) if (p.p > pk[0].p * 0.01) initPeaks++;
	Knobs l; l.length = 200; l.spacing = 7; l.stiff = 0.54; l.supports = 0.35; l.endA = -0.92; l.endB = 0.92; l.live = 0.76; l.warp = 0.3;
	l.train = 0; l.bright = 0.09; l.ground = 0.14; l.listen = 0.3; l.distance = 0.2; l.age = 0.4; l.even = 0;
	auto r2 = runChain(l, 48000, 9);
	auto m2 = mono(r2.st);
	auto pk2 = peaks(m2, 6 * 48000, 3 * 48000, 48000, tot, 50, 18000);
	int strong = 0; for (auto& p : pk2) if (p.p > pk2[0].p * 0.01) strong++;
	report(on > -40 && r.guard == 0 && r2.guard == 0 && r.maxAbs <= 11 && r2.maxAbs <= 11 && strong >= 3 && off < -100, "6 live",
		fmt("Init + LIVE 0.74 from the seed alone: %.0f dBV at 4-5 s (%d peaks within 20 dB); LIVE 0.3: %.0f dBV 9 s later; Live Line preset line: %d peaks within 20 dB", on, initPeaks, off, strong));
}
static void test7() {
	Knobs k = liveKnobs(); k.even = 0;
	Drive d; d.fn = [](double t, Knobs& kk, double&, bool&, double&) { kk.live = (t < 1.0) ? 0.9 : ((std::fmod(t * 3.0, 1.0) < 0.5) ? 1.0 : 0.45); };
	auto r = runChain(k, 48000, 11, &d);
	auto m = mono(r.st);
	double onS = 0, offS = 0; int nOn = 0, nOff = 0;
	for (int c = 3; c < 30; c++) { double t = c / 3.0; onS += rmsDb(m, t + 0.10, t + 0.16, 48000); nOn++; offS += rmsDb(m, t + 0.27, t + 0.33, 48000); nOff++; }
	double diff = onS / nOn - offS / nOff;
	report(diff > 6 && r.guard == 0 && r.finite, "7 live gate", fmt("LIVE gated 0.45/1.0 at 3 Hz: on halves %.1f dB above off halves; guard %ld", diff, r.guard));
}
static void test8() {
	std::vector<long> sn;
	auto r = refscene::run("hard", 0, 1e9, &sn);
	auto m = mono(r.st);
	double soft = rmsDb(m, 25.2, 26.3, 48000), hard = rmsDb(m, 26.4, 27.5, 48000);
	long released = sn[275] - sn[264];
	// same line with GRIP 0: the soft hit is heard
	Engine* e = new Engine(); e->line_init();
	Params& p = e->p; p.N = 256; p.M = 4; p.d = 18; p.a0 = -0.9; p.pick = 12; p.g = 0.9988; p.hidamp = 0.0015; p.ties = 0.12; p.loose = 0.2; p.warp = 0.3; p.swell = 0.3; p.age = 0.35;
	std::vector<double> x(48000);
	for (long t = 0; t < 48000; t++) { if (t == 0) e->strike(240, 0.3, 0.12); e->strike_step(); double l, rr; e->line_step(&l, &rr); x[t] = 0.5 * (l + rr); }
	delete e;
	double soft0 = rmsDb(x, 0, 1, 48000);
	report(soft < hard - 60 && released >= 200 && soft0 > hard - 60, "8 grip",
		fmt("soft hit %.0f dB below the hard one; the hard hit released %ld cells; with GRIP 0 the soft hit is %.0f dB below it", hard - soft, released, hard - soft0));
}
static void test9() {
	Knobs k;   // Init
	auto r = runChain(k, 48000, 4);
	auto m = mono(r.st);
	long on = 0; while (on < (long)m.size() && std::fabs(m[on]) < 0.01) on++;
	double t0 = on / 48000.0 + 0.05;
	double pk = peakAbs(m, t0, t0 + 1, 48000), rms = std::pow(10, rmsDb(m, t0, t0 + 1, 48000) / 20);
	double jump = 0, prev = rmsDb(m, t0, t0 + 0.05, 48000);
	for (int w = 1; w < 20; w++) { double v = rmsDb(m, t0 + w * 0.05, t0 + (w + 1) * 0.05, 48000); jump = std::max(jump, std::fabs(v - prev)); prev = v; }
	report(pk / rms < 8 && jump < 12, "9 no burst on the train", fmt("Init from silence: sound starts at %.2f s; first second crest %.1f, largest step between 50 ms windows %.1f dB", on / 48000.0, pk / rms, jump));
}
// the test rail of test 4 played through the module chain
static Knobs testRailKnobs() {
	Knobs k; k.length = 256; k.spacing = 10; k.heavy = false; k.stiff = 1 - std::sqrt(1 - 0.7 / 0.95); k.supports = std::sqrt(0.12 / 1.3);
	k.endA = k.endB = 0; k.live = 0.5 - 0.5 * std::pow(0.0005 / 0.02, 0.4); k.bright = 1 - std::sqrt(0.002 / 0.012); k.ground = 0; k.age = 0;
	k.listen = 18.0 / 251; k.distance = 210.0 / 255; k.train = 0; k.hard = 1 - std::sqrt((0.1 - 0.06) / 0.6); k.even = 0; k.level = 0.7;
	return k;
}
static bool pitchCheck(double host, std::string& d) {
	const double shifts[4] = {-1, 0, 1, 0.5};
	std::vector<double> base; bool ok = true;
	for (int si = 0; si < 4; si++) {
		double sh = shifts[si];
		Knobs k = testRailKnobs(); k.pitch = sh;
		double ts = std::pow(2.0, -sh);
		Drive dr; dr.fn = [=](double t, Knobs&, double& sv, bool&, double&) { if ((long)std::llround(t * host) == (long)(0.05 * host)) sv = std::pow(0.01, 0.4); };
		auto r = runChain(k, host, 0.1 + 3.2 * ts, &dr);
		auto m = mono(r.st);
		double sc = std::pow(2.0, sh);
		auto g = testRailTones(m, 0.05 + 1.0 * ts, 0.05 + 3.0 * ts, host, sc);
		if (si == 0) { base = g; for (auto& b : base) b *= 2; }   // measured at -1, scaled to PITCH 0
		std::string s;
		for (int i = 0; i < 4; i++) { double expect = base[i] * sc; double c = cents(g[i], expect); if (std::fabs(c) > 5 || r.guard) ok = false; s += fmt("%+.1f ", c); }
		d += fmt("[%+.1f oct: %s] ", sh, s.c_str());
	}
	return ok;
}
static void test10() {
	std::string d; bool ok = pitchCheck(48000, d);
	report(ok, "10 PITCH", "support-tone error in cents against the scaled -1 oct tones: " + d);
}
static void test11() {
	Knobs k; k.passTime = 10; k.even = 0; k.listen = 0.5; k.distance = 0.4;
	Drive dr; dr.fn = [](double t, Knobs&, double&, bool& ps, double&) { if (std::fabs(t - 1.0) < 0.5 / 48000) ps = true; };
	auto r = runChain(k, 48000, 17, &dr, true);
	auto L = chan(r.st, 0), R = chan(r.st, 1), m = mono(r.st);
	// the lead axle passes the listener at: start + T * (src - pick) / (src - end), end = -60
	double pick = 2 + 0.5 * 315, src = pick + 0.4 * 319;
	double tPass = 1.0 + 10.0 * (src - pick) / (src + 60.0);
	double best = -1e9, tBest = 0; for (double t = 1.5; t < 11; t += 0.05) { double v = rmsDb(m, t, t + 0.25, 48000); if (v > best) { best = v; tBest = t + 0.125; } }
	double balBefore = rmsDb(R, 2, 4, 48000) - rmsDb(L, 2, 4, 48000), tRear = tPass + 44.0 * 10.0 / (src + 60.0);
	double balAfter = rmsDb(L, tRear + 0.15, tRear + 0.6, 48000) - rmsDb(R, tRear + 0.15, tRear + 0.6, 48000);
	long g0 = 0, g1 = 0; for (size_t i = 0; i < r.gate.size(); i++) if (r.gate[i]) { if (!g0) g0 = i; g1 = i; }
	double gateOn = g0 / 48000.0, gateOff = g1 / 48000.0;
	double steady = peakAbs(m, 14.5, 16.5, 48000), around = peakAbs(m, 11.9, 13.2, 48000), idle = peakAbs(m, 0.2, 0.9, 48000);
	bool ok = std::fabs(tBest - tPass) < 1.0 && balBefore > 0 && balAfter > 0 && std::fabs(gateOn - 1.0) < 0.01 && std::fabs(gateOff - 11.0) < 0.05 && around <= 1.5 * std::max(steady, idle);
	report(ok, "11 PASS", fmt("loudest at %.2f s, lead axle passes at %.2f s; R louder by %.1f dB before, L louder by %.1f dB after; gate %.2f-%.2f s; peak around the return %.3g vs steady %.3g",
		tBest, tPass, balBefore, balAfter, gateOn, gateOff, around, steady));
}
struct FuzzStats { std::atomic<long> guard{0}, nonfinite{0}, over{0}; double maxAbs = 0; std::mutex mu; };
static void fuzzOne(int seed, double host, double secs, FuzzStats& st) {
	uint64_t s = 0x9E3779B97F4A7C15ULL * (seed + 1); auto u = [&]() { s ^= s << 13; s ^= s >> 7; s ^= s << 17; return (s >> 11) * (1.0 / 9007199254740992.0); };
	Knobs k;
	auto draw = [&]() {
		k.pitch = -2 + 3 * u() + (u() < 0.2 ? (u() - 0.5) * 4 : 0); k.length = 16 + 304 * u(); k.spacing = 1 + 39 * u(); k.heavy = u() < 0.5;
		k.stiff = u(); k.supports = u(); k.endA = 2 * u() - 1; k.endB = 2 * u() - 1; k.live = u(); k.ground = u(); k.bright = u();
		k.listen = u(); k.distance = 2 * u() - 1; k.passTime = 4 + 116 * u(); k.train = u(); k.speed = u(); k.flats = u(); k.hard = u(); k.inLevel = u();
		k.warp = u(); k.swell = u(); k.loose = u(); k.grip = u() < 0.5 ? 0 : u(); k.age = u(); k.even = u(); k.width = u(); k.level = 0.7 + 0.3 * u();
	};
	draw();
	Voice v; v.init(host);
	long n = (long)(secs * host), next = (long)(0.5 * host);
	double mx = 0;
	for (long i = 0; i < n; i++) {
		if (i == next) { draw(); next += (long)(0.5 * host); }
		double sv = (u() < 2.0 / host) ? u() : -1; bool ps = u() < 0.3 / host;
		double in = (std::fmod(i / host, 3.0) < 0.3) ? 10 * (u() - 0.5) : 0;
		v.process(k, sv, ps, in);
		if (!std::isfinite(v.outL) || !std::isfinite(v.outR)) { st.nonfinite++; break; }
		double a = std::max(std::fabs(v.outL), std::fabs(v.outR)); mx = std::max(mx, a); if (a > 11.0001) st.over++;
	}
	st.guard += v.e->guard;
	std::lock_guard<std::mutex> lk(st.mu); st.maxAbs = std::max(st.maxAbs, mx);
}
static void test12(int n = 400, double secs = 20, double host = 48000, const char* name = "12 fuzz") {
	FuzzStats st;
	parallelFor(n, [&](int i) { fuzzOne(i, host, secs, st); });
	report(st.guard == 0 && st.nonfinite == 0 && st.over == 0, name, fmt("%d random patches x %.0f s at %.1f kHz: guard %ld, non-finite %ld, max |out| %.2f V", n, secs, host / 1000, (long)st.guard, (long)st.nonfinite, st.maxAbs));
}
static void test13() {
	const double rates[3] = {44100, 48000, 96000};
	for (double h : rates) {
		std::string d; bool ok = pitchCheck(h, d);
		report(ok, "13 sample rate (pitch)", fmt("%.1f kHz host: ", h / 1000) + d);
		test12(100, 10, h, "13 sample rate (fuzz)");
	}
}
static void test14() {
	Knobs k; k.train = 0; k.live = 0.3;
	auto r = runChain(k, 48000, 61);
	long nz = 0; for (long i = 60 * 48000; i < 61 * 48000; i++) if (r.st[2 * i] != 0.f || r.st[2 * i + 1] != 0.f) nz++;
	report(nz == 0, "14 silence", fmt("TRAIN 0, LIVE 0.3: %ld non-zero samples in the 61st second", nz));
}
static void test15() {
	Knobs k = liveKnobs(); k.train = 0.5;
	Drive d; d.fn = [](double t, Knobs& kk, double&, bool&, double&) {
		kk.distance = clampd(kk.distance + 0.2 * 2.0 * std::sin(2 * PI * 200 * t), -1, 1);
		kk.live = clampd(kk.live + 0.1 * 2.0 * std::sin(2 * PI * 200 * t + 1), 0, 1);
	};
	auto r = runChain(k, 48000, 10, &d);
	report(r.guard == 0 && r.finite && r.maxAbs <= 11, "15 audio-rate CV", fmt("DISTANCE and LIVE driven by 200 Hz +-2 V sines: guard %ld, max %.2f V", r.guard, r.maxAbs));
}
static void test16() {
	struct C { const char* n; double pitch; bool pushed; };
	const C cs[4] = {{"PITCH 0", 0, false}, {"PITCH +1", 1, false}, {"PITCH 0, pushed", 0, true}, {"PITCH +1, pushed", 1, true}};
	std::string d;
	for (auto& c : cs) {
		Knobs k; k.pitch = c.pitch; if (c.pushed) { k.warp = 0.5; k.swell = 0.5; k.loose = 0.5; }
		auto r = runChain(k, 48000, 8);
		d += fmt("%s %.0f%%  ", c.n, 100 * r.secs / 8);
	}
	info("16 speed", "share of one core at 48 kHz, 320 cells: " + d);
}
static void test17() {
	Knobs k = liveKnobs(); k.live = 1.0;
	auto r = runChain(k, 48000, 8, nullptr, true);
	double s = 0; for (long i = 5 * 48000; i < 8 * 48000; i++) s += r.env[i]; s /= 3 * 48000;
	report(s > 6 && s < 10, "17 ENV calibration", fmt("LIVE 1 at its ceiling: ENV %.1f V", s));
}
static void testPresets(const std::vector<Preset>& ps) {
	std::vector<std::string> lines(ps.size()); std::vector<char> ok(ps.size(), 1);
	parallelFor((int)ps.size(), [&](int i) {
		Knobs k = knobsFrom(ps[i]); Drive d = driveFor(ps[i].drive, 48000);
		bool hasDrive = ps[i].drive != "-";
		auto r = runChain(k, 48000, 10, hasDrive ? &d : nullptr);
		auto m = mono(r.st);
		double rms = std::pow(10, rmsDb(m, 1, 10, 48000) / 20);
		double tot; auto pk = peaks(m, 2 * 48000, 6 * 48000, 48000, tot, 30, 18000);
		double top = pk.empty() ? 1 : pk[0].p / tot;
		bool good = r.finite && r.guard == 0 && r.maxAbs <= 11 && rms > 0.05;
		if (i >= 3 && i <= 7 && top > 0.5) good = false;   // the live presets must not be a polite tone
		ok[i] = good;
		lines[i] = fmt("%-18s %-20s rms %.2f V, peak %.2f V, strongest partial %.0f%% of power", ps[i].name.c_str(), ps[i].drive.c_str(), rms, r.maxAbs, 100 * top);
	});
	for (size_t i = 0; i < ps.size(); i++) report(ok[i], "18 preset", lines[i]);
	// Init level
	Knobs k = knobsFrom(ps[0]); auto r = runChain(k, 48000, 10); auto m = mono(r.st);
	double pk = std::max(peakAbs(chan(r.st, 0), 3, 10, 48000), peakAbs(chan(r.st, 1), 3, 10, 48000));
	report(pk > 3 && pk < 7, "18 Init level", fmt("Init peaks at %.1f V", pk));
}

// ------------------------------------------------------------------ demos
static std::vector<float> levelForListening(const std::vector<float>& st, double sr, double ratio) {
	// the post.py treatment of the sketches: slow level riding, then a soft ceiling
	long n = st.size() / 2; std::vector<double> m(n); for (long i = 0; i < n; i++) m[i] = st[2 * i] * st[2 * i] + st[2 * i + 1] * st[2 * i + 1];
	long k = (long)(0.25 * sr); std::vector<double> env(n); double acc = 0; std::vector<double> c(n + 1, 0.0);
	for (long i = 0; i < n; i++) c[i + 1] = c[i] + m[i];
	double ref = 0; std::vector<double> e2(n);
	for (long i = 0; i < n; i++) { long a = std::max(0L, i - k / 2), b = std::min(n, i + k / 2); e2[i] = std::sqrt((c[b] - c[a]) / std::max(1L, b - a)) + 1e-12; }
	std::vector<double> sorted = e2; std::nth_element(sorted.begin(), sorted.begin() + (long)(0.97 * n), sorted.end()); ref = sorted[(long)(0.97 * n)];
	(void)acc; (void)env;
	std::vector<float> y(st.size()); double p999 = 0; std::vector<double> tmp(st.size());
	for (long i = 0; i < n; i++) { double g = std::pow(std::max(e2[i], ref * 0.003) / ref, -(1 - 1 / ratio)); tmp[2 * i] = st[2 * i] * g; tmp[2 * i + 1] = st[2 * i + 1] * g; }
	std::vector<double> a(tmp.size()); for (size_t i = 0; i < tmp.size(); i++) a[i] = std::fabs(tmp[i]);
	std::nth_element(a.begin(), a.begin() + (long)(0.9995 * a.size()), a.end()); p999 = a[(long)(0.9995 * a.size())] + 1e-12;
	for (size_t i = 0; i < tmp.size(); i++) y[i] = (float)(0.9 * std::tanh(tmp[i] / p999));
	return y;
}
static void demos(const std::string& out, const std::vector<Preset>& ps) {
	const char* scenes[4] = {"rail", "hard", "live", "live2"}; const double ratio[4] = {3, 2, 1.5, 1.5};
	for (int i = 0; i < 4; i++) { auto r = refscene::run(scenes[i]); writeWav(out + "/scene-" + std::to_string(i + 1) + "-" + scenes[i] + ".wav", levelForListening(r.st, 48000, ratio[i]), 48000); }
	parallelFor((int)ps.size(), [&](int i) {
		Knobs k = knobsFrom(ps[i]); Drive d = driveFor(ps[i].drive, 48000); bool hasDrive = ps[i].drive != "-";
		double dur = ps[i].drive == "pass" ? 34 : 12;
		if (ps[i].drive == "pass") k.passTime = 30;
		auto r = runChain(k, 48000, dur, hasDrive ? &d : nullptr);
		std::vector<float> y(r.st.size()); for (size_t j = 0; j < y.size(); j++) y[j] = r.st[j] / 11.0f;
		long fade = 48000 / 50; long n = y.size() / 2;
		for (long j = 0; j < fade; j++) { float g = (float)j / fade; y[2 * j] *= g; y[2 * j + 1] *= g; y[2 * (n - 1 - j)] *= g; y[2 * (n - 1 - j) + 1] *= g; }
		std::string nm = ps[i].name; std::replace(nm.begin(), nm.end(), ' ', '_');
		std::string dn = hasDrive ? "_" + ps[i].drive : ""; std::replace(dn.begin(), dn.end(), '+', '-');
		writeWav(out + "/" + fmt("preset-%02d-", i + 1) + nm + dn + ".wav", y, 48000);
	});
}

int main(int argc, char** argv) {
	ftz();
	if (argc < 3) { std::fprintf(stderr, "usage: render <outdir> <refdir|demos|fuzz n secs>\n"); return 1; }
	std::string out = argv[1], mode = argv[2];
	auto ps = loadPresets("test/presets.txt");
	if (mode == "demos") { demos(out, ps); return 0; }
	if (mode == "fuzz") { test12(argc > 3 ? std::atoi(argv[3]) : 400, argc > 4 ? std::atof(argv[4]) : 20); return nFail ? 1 : 0; }
	test1(mode == "quick" ? std::string("test/ref") : mode); test2(); test3(); test4(); test5(); test6(); test7(); test8(); test9();
	test14(); test10(); test11(); test15(); test17(); testPresets(ps); test16();
	if (mode != "quick") { test12(); test13(); }
	std::printf("\n%d passed, %d failed\n", nPass, nFail);
	return nFail ? 1 : 0;
}
