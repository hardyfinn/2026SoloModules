// Molten offline checks (no Rack needed): make test
// Every check prints PASS or FAIL. ONLY=1,5,9 runs just those tests.
//   ./test/render OUT REFDIR            the tests (test 15 writes one WAV per preset to OUT)
//   ./test/render OUT demos             demo WAVs of the four reference scenes from the C++ engine
#include "../src/dsp/MoltenEngine.hpp"
#include "../src/dsp/Mapping.hpp"
#include "scenes.h"
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <string>
#include <complex>
#include <chrono>
#include <map>
#include <set>
#include <thread>
#include <atomic>
#include <mutex>
#include <cstdarg>
#include <functional>
#include <algorithm>
using namespace molten;

static int nFail = 0, nPass = 0;
static std::mutex outMu;
static void report(bool ok, const std::string& name, const std::string& detail) {
	std::lock_guard<std::mutex> l(outMu);
	std::printf("%s  %-30s %s\n", ok ? "PASS" : "FAIL", name.c_str(), detail.c_str());
	std::fflush(stdout);
	if (ok) nPass++; else nFail++;
}
static std::string fmt(const char* f, ...) {
	char b[1024]; va_list a; va_start(a, f); vsnprintf(b, sizeof b, f, a); va_end(a); return b;
}
static std::set<int> only;
static bool want(int t) { return only.empty() || only.count(t); }

// run jobs on all cores
static void parallelFor(int n, std::function<void(int)> f) {
	std::atomic<int> next(0);
	int nt = std::max(1u, std::thread::hardware_concurrency());
	std::vector<std::thread> th;
	for (int i = 0; i < nt; i++) th.emplace_back([&]() { for (;;) { int j = next++; if (j >= n) break; f(j); } });
	for (auto& t : th) t.join();
}

// ------------------------------------------------------------------ WAV
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

// ------------------------------------------------------------------ FFT + band levels
static void fft(std::vector<std::complex<double>>& a) {
	size_t n = a.size();
	for (size_t i = 1, j = 0; i < n; i++) { size_t bit = n >> 1; for (; j & bit; bit >>= 1) j ^= bit; j ^= bit; if (i < j) std::swap(a[i], a[j]); }
	for (size_t len = 2; len <= n; len <<= 1) {
		double ang = -2 * PI / len; std::complex<double> wl(std::cos(ang), std::sin(ang));
		for (size_t i = 0; i < n; i += len) { std::complex<double> w(1); for (size_t j = 0; j < len / 2; j++) { auto u = a[i + j], v = a[i + j + len / 2] * w; a[i + j] = u + v; a[i + j + len / 2] = u - v; w *= wl; } }
	}
}
static std::vector<double> spectrum(const std::vector<double>& x, long from, long len, int nfft = 8192) {
	std::vector<double> P(nfft / 2 + 1, 0.0); int frames = 0;
	for (long s = from; s + nfft <= from + len && s + nfft <= (long)x.size(); s += nfft / 2) {
		std::vector<std::complex<double>> a(nfft);
		for (int i = 0; i < nfft; i++) a[i] = x[s + i] * (0.5 - 0.5 * std::cos(2 * PI * i / nfft));
		fft(a); for (int i = 0; i <= nfft / 2; i++) P[i] += std::norm(a[i]); frames++;
	}
	if (frames) for (double& p : P) p /= frames;
	return P;
}
static std::vector<double> thirdOctaves(const std::vector<double>& P, double sr, int nfft = 8192) {
	std::vector<double> b;
	for (double fc = 50; fc <= 16000; fc *= std::pow(2.0, 1.0 / 3)) {
		double lo = fc * std::pow(2.0, -1.0 / 6), hi = fc * std::pow(2.0, 1.0 / 6), s = 0;
		for (int i = 0; i < (int)P.size(); i++) { double f = i * sr / nfft; if (f >= lo && f < hi) s += P[i]; }
		b.push_back(10 * std::log10(s + 1e-30));
	}
	return b;
}
static double peakFreq(const std::vector<double>& x, long from, long len, double sr, double lo = 0, double hi = 1e9) {
	int nfft = 1; while (nfft * 2 <= len && nfft < (1 << 18)) nfft *= 2;
	std::vector<std::complex<double>> a(nfft);
	for (int i = 0; i < nfft; i++) a[i] = x[from + i] * (0.5 - 0.5 * std::cos(2 * PI * i / nfft));
	fft(a); int bi = 1; double bm = 0;
	int i0 = std::max(1, (int)(lo * nfft / sr)), i1 = std::min(nfft / 2 - 1, (int)(hi * nfft / sr));
	for (int i = i0; i <= i1; i++) if (std::norm(a[i]) > bm) { bm = std::norm(a[i]); bi = i; }
	double l = std::log(std::norm(a[bi - 1]) + 1e-30), c = std::log(std::norm(a[bi]) + 1e-30), r = std::log(std::norm(a[bi + 1]) + 1e-30);
	double d = 0.5 * (l - r) / (l - 2 * c + r);
	return (bi + d) * sr / nfft;
}
static double bandDiff(const std::vector<double>& a, const std::vector<double>& b) {
	long n = std::min(a.size(), b.size()), win = 2 * 96000; double sum = 0; int nw = 0;
	for (long s = 0; s + win <= n; s += win) {
		auto ba = thirdOctaves(spectrum(a, s, win), 96000), bb = thirdOctaves(spectrum(b, s, win), 96000);
		double pk = *std::max_element(bb.begin(), bb.end()); double d = 0; int k = 0;
		for (size_t i = 0; i < ba.size(); i++) if (bb[i] > pk - 40) { d += std::fabs(ba[i] - bb[i]); k++; }
		if (k) { sum += d / k; nw++; }
	}
	return nw ? sum / nw : 99;
}
static double rmsOf(const std::vector<double>& x, long a, long b) { double s = 0; b = std::min(b, (long)x.size()); for (long i = a; i < b; i++) s += x[i] * x[i]; return std::sqrt(s / std::max(1L, b - a)); }
static double dB(double x) { return 20 * std::log10(x + 1e-30); }

// ------------------------------------------------------------------ engine-rate render of a reference scene
struct Run { std::vector<float> st; std::vector<double> M, Ts; long guards = 0; double secs = 0; long casts = 0; };
static Run renderScene(const Scene& sc, double fe = 96000) {
	Engine* e = new Engine(); e->setRate(fe); e->build(0); e->reset(); e->f1 = sc.f1; e->allowIdle = false;
	sc.ctl(0, *e); e->setThermal(e->c.heat);
	Run r; long n = (long)(sc.dur * fe); r.st.resize(2 * n);
	long half = (long)(0.5 * fe), dn = 0;
	auto t0 = std::chrono::steady_clock::now();
	for (long i = 0; i < n; i++) {
		if (i % BLK == 0) sc.ctl(i / fe, *e);
		e->process(0.0);
		r.st[2 * i] = (float)e->outL; r.st[2 * i + 1] = (float)e->outR;
		if (++dn == half) { r.M.push_back(e->M); r.Ts.push_back(e->Tsol); dn = 0; }
	}
	r.secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
	r.guards = e->guards + e->clampN; r.casts = e->casts; delete e; return r;
}
static std::vector<double> monoOf(const std::vector<float>& st) { std::vector<double> m(st.size() / 2); for (size_t i = 0; i < m.size(); i++) m[i] = 0.5 * ((double)st[2 * i] + st[2 * i + 1]); return m; }
// the reference's log: M and Ts every 0.5 s (two decimals)
static void readLog(const std::string& path, std::vector<double>& M, std::vector<double>& Ts) {
	FILE* f = std::fopen(path.c_str(), "r"); if (!f) return;
	char line[512];
	while (std::fgets(line, sizeof line, f)) {
		const char* a = std::strstr(line, " M "); const char* b = std::strstr(line, " Ts ");
		if (a && b) { M.push_back(std::atof(a + 3)); Ts.push_back(std::atof(b + 4)); }
	}
	std::fclose(f);
}

// ------------------------------------------------------------------ engine driven by knobs (no AGE, no leveller)
struct KRun {
	Engine* e; double fe;
	KRun(double fe_, const Knobs& k) : fe(fe_) { e = new Engine(); e->setRate(fe); e->build(k.body); e->reset(); e->meterLen = (int)(0.1 * fe); mapKnobs(k, *e, nullptr); e->setThermal(0); }
	~KRun() { delete e; }
	// hold the body at a steady flame temperature from the start
	void settle(const Knobs& k) { mapKnobs(k, *e, nullptr); e->setThermal(e->c.heat); }
	template <class F> void run(double secs, F perSample, std::function<void(double, Knobs&)> mod, Knobs k, double t0 = 0) {
		long n = (long)(secs * fe);
		for (long i = 0; i < n; i++) {
			double t = t0 + i / fe;
			if (e->blk == 0) { Knobs kk = k; if (mod) mod(t, kk); mapKnobs(kk, *e, nullptr); }
			e->process(0);
			perSample(t, 0.5 * (e->outL + e->outR));
		}
	}
};

// ------------------------------------------------------------------ full module chain (knobs -> host-rate volts)
struct Chain {
	Engine* e; Age age; Decimator dec; Leveller lev; double host; bool os; int ctr = 0;
	double level = 0.7; bool ageOn = true;
	Chain(double host_, uint64_t seed = 1, int body = 0) : host(host_) {
		e = new Engine(); os = host < 88200; e->setRate(os ? 2 * host : host); e->build(body); e->reset();
		age.init(seed); lev.setRate(host); dec.reset(); lev.reset();
		Knobs k; k.body = body; mapKnobs(k, *e, &age); e->setThermal(0);
	}
	~Chain() { delete e; }
	void step(const Knobs& k, double inV, double strikeVel, double& L, double& R) {
		if (ctr == 0) {
			int ob = e->body;
			if (ageOn) age.update(BLK / e->fe, e->emean);
			mapKnobs(k, *e, ageOn ? &age : nullptr);
			if (e->body != ob) e->reset();
		}
		if (++ctr >= BLK / (os ? 2 : 1)) ctr = 0;
		if (strikeVel > 0) e->strike(420 * strikeVel);
		double inS = inV * e->c.inLevel;
		double l2[2], r2[2];
		for (int s = 0; s < (os ? 2 : 1); s++) { e->process(inS); l2[s] = e->outL; r2[s] = e->outR; }
		double l, r;
		if (os) dec.process(l2, r2, l, r); else { l = l2[0]; r = r2[0]; }
		double g = lev.gain(l, r);
		double s = level * OUT_SCALE;
		L = 11 * std::tanh(l * g * s / 11); R = 11 * std::tanh(r * g * s / 11);
	}
	static double OUT_SCALE;
};
double Chain::OUT_SCALE = OUT_VOLTS;

// ------------------------------------------------------------------ presets (test/presets.txt from tools/make_presets.py)
static const char* PRESET_KEYS[] = {"heat", "water", "mass", "flame", "width", "latent", "supercool", "self", "pings",
	"drive", "gain", "focus", "pitch", "track", "pos", "grab", "release", "tension", "loose",
	"sag", "cast", "body", "size", "thick", "curve", "decay", "pick", "amp", "out", "mallet", "inlevel", "bright", "age", "level"};
static const int NPK = sizeof(PRESET_KEYS) / sizeof(PRESET_KEYS[0]);
static Knobs knobsFrom(const double* v, double& level) {
	Knobs k;
	k.heat = v[0]; k.quenchAmt = v[1]; k.mass = v[2]; k.flame = v[3]; k.width = v[4]; k.latent = v[5]; k.supercool = v[6]; k.self = v[7]; k.pings = v[8];
	k.drive = v[9]; k.gain = 20.0 * std::pow(30.0, v[10]); k.focus = v[11]; k.pitch = v[12]; k.trackPlate = v[13] > 0.5; k.pos = v[14]; k.grab = v[15]; k.release = v[16];
	k.tension = v[17]; k.loose = v[18]; k.sag = v[19]; k.cast = v[20]; k.body = (int)std::lround(v[21]); k.size = 20.0 * std::pow(10.0, v[22]); k.thick = v[23];
	k.curve = v[24]; k.decay = v[25]; k.pick = v[26]; k.amp = v[27]; k.out = v[28]; k.mallet = v[29]; k.inLevel = v[30]; k.bright = v[31]; k.age = v[32];
	level = v[33];
	return k;
}
// what each preset's notes say to patch, so its render shows what it is for
static void presetPatch(const std::string& nm, double t, Knobs& k, double& inV, double& sv, Rng& g) {
	if (nm == "Foundry") k.quenching = std::fmod(t, 6.0) > 4.0;
	if (nm == "Flash_Freeze") { k.heat = t < 4 ? 1.0 : k.heat; if (std::fabs(t - 8.0) < 1e-9) sv = 1.0; }
	if (nm == "Walking_Flame") k.flame = 0.5 - 0.45 * std::cos(2 * PI * t / 10.0);
	if (nm == "Cold_Bell" && std::fmod(t, 2.5) < 1e-9) sv = 0.8;
	if (nm == "Annealed" && std::fmod(t, 1.25) < 1e-9) sv = 0.7;
	if (nm == "Molten_Lead") k.pitch = (int)(t / 1.25) % 4 == 0 ? 0 : (int)(t / 1.25) % 4 == 1 ? 3.0 / 12 : (int)(t / 1.25) % 4 == 2 ? -5.0 / 12 : 7.0 / 12;
	if (nm == "Meltdown") k.heat = 0.5 + 0.4 * std::sin(2 * PI * 0.7 * t);
	if (nm == "Resonator") { double ph = std::fmod(t, 0.5); inV = ph < 0.03 ? 8 * g.bi() * std::exp(-ph / 0.008) : 0; }
}

static void presetTest(const std::string& out, double secs = 12) {
	FILE* f = std::fopen("test/presets.txt", "r");
	if (!f) { report(false, "15 presets", "no test/presets.txt (run tools/make_presets.py)"); return; }
	std::vector<std::string> names; std::vector<std::vector<double>> vals; char nm[128];
	while (std::fscanf(f, "%127s", nm) == 1) {
		std::vector<double> v(NPK, 0.0);
		for (int i = 0; i < NPK; i++) if (std::fscanf(f, "%lf", &v[i]) != 1) v[i] = 0;
		names.push_back(nm); vals.push_back(v);
	}
	std::fclose(f);
	std::vector<std::string> det(names.size()); std::atomic<int> bad(0);
	parallelFor((int)names.size(), [&](int idx) {
		double level; Knobs k = knobsFrom(vals[idx].data(), level);
		Chain ch(48000, 0x51A7E0000ULL + 7919 * (idx + 1), k.body); ch.level = level;
		std::string name = names[idx];
		long n = (long)(secs * 48000); std::vector<float> st(2 * n); double ss = 0, pk = 0, prs = 0, Ms = 0; int prn = 0; Rng g; g.seed(5);
		for (long i = 0; i < n; i++) {
			double t = i / 48000.0, inV = 0, sv = 0;
			Knobs kk = k; presetPatch(name, t, kk, inV, sv, g);
			double L, R; ch.step(kk, inV, sv, L, R);
			st[2 * i] = (float)(L / 10); st[2 * i + 1] = (float)(R / 10);
			if (t >= 2) { ss += L * L + R * R; pk = std::max({pk, std::fabs(L), std::fabs(R)}); if (i % 2400 == 0) { prs += ch.e->pr; Ms += ch.e->M; prn++; } }
		}
		double rms = std::sqrt(ss / (2.0 * (secs - 2) * 48000));
		writeWav(out + fmt("/preset-%02d-%s.wav", idx + 1, name.c_str()), st, 48000);
		bool good = rms > 0.3 && ch.e->guards == 0 && pk <= 11.0;
		if (!good) bad++;
		for (char& c : name) if (c == '_') c = ' ';
		det[idx] = fmt("%s%s %.1f V, PR %.1f, liquid %.0f%%; ", good ? "" : "BAD ", name.c_str(), rms, prs / std::max(prn, 1), 100 * Ms / std::max(prn, 1));
	});
	std::string all; for (auto& d : det) all += d;
	report(bad == 0, "15 presets", all);
}

// ------------------------------------------------------------------ the tests
int main(int argc, char** argv) {
	std::string out = argc > 1 ? argv[1] : "test/out";
	std::string refDir = argc > 2 ? argv[2] : "test/ref";
	if (const char* o = getenv("ONLY")) { std::string s = o; size_t p = 0; while (p < s.size()) { only.insert(std::atoi(s.c_str() + p)); p = s.find(',', p); if (p == std::string::npos) break; p++; } }
	auto scenes = makeScenes();
	std::map<std::string, Scene*> S; for (auto& s : scenes) S[s.name] = &s;

	if (refDir == "demos") {
		// The four reference scenes from the C++ engine, through the module's decimator, leveller and output stage.
		parallelFor((int)scenes.size(), [&](int j) {
			const Scene& x = scenes[j];
			Run r = renderScene(x, 96000);
			Decimator dec; Leveller lev; lev.setRate(48000);
			long n = r.st.size() / 4; std::vector<float> o(2 * n); double pk = 0;
			for (long i = 0; i < n; i++) {
				double l2[2] = {r.st[4 * i], r.st[4 * i + 2]}, r2[2] = {r.st[4 * i + 1], r.st[4 * i + 3]}, L, R;
				dec.process(l2, r2, L, R); double g = lev.gain(L, R) * 0.7 * OUT_VOLTS;
				L = 11 * std::tanh(L * g / 11); R = 11 * std::tanh(R * g / 11);
				o[2 * i] = (float)(L / 11); o[2 * i + 1] = (float)(R / 11); pk = std::max({pk, std::fabs(L), std::fabs(R)});
			}
			writeWav(out + "/scene-" + x.name + ".wav", o, 48000);
			std::lock_guard<std::mutex> l(outMu);
			std::printf("%-8s %5.1f s  peak %.1f V  casts %ld  guards %ld\n", x.name, x.dur, pk, r.casts, r.guards);
		});
		presetTest(out, 30);
		return 0;
	}
	if (getenv("ONLY_PRESETS")) { presetTest(out); return nFail ? 1 : 0; }

	// ---------------------------------------------------------- 1. reference match
	if (want(1)) {
		const char* names[4] = {"heat", "cast", "flame", "pushed"};
		std::string det[4]; bool ok[4];
		parallelFor(4, [&](int si) {
			const char* nm = names[si];
			auto ref = readRaw(refDir + "/raw-" + nm + ".f32");
			if (ref.empty()) { det[si] = std::string(nm) + ": no reference; "; ok[si] = false; return; }
			Run r = renderScene(*S[nm]);
			auto a = monoOf(r.st); std::vector<double> b(ref.size() / 2); for (size_t i = 0; i < b.size(); i++) b[i] = 0.5 * ((double)ref[2 * i] + ref[2 * i + 1]);
			long n = std::min(a.size(), b.size());
			// (a) sample for sample until the chaos (or a random ping) parts them: first 10 ms window off by more than 1e-3 of the running peak
			double pkr = 1e-9; long split = n;
			for (long s = 0; s + 960 <= n; s += 960) {
				double d = 0; for (long i = s; i < s + 960; i++) { pkr = std::max(pkr, std::fabs(b[i])); d = std::max(d, std::fabs(a[i] - b[i])); }
				if (d > 1e-3 * pkr) { split = s; break; }
			}
			// (b) thermal curves (logged to two decimals by the reference)
			std::vector<double> rM, rT; readLog(refDir + "/log-" + nm + ".txt", rM, rT);
			double tw = 0; for (size_t i = 0; i < std::min(rM.size(), r.M.size()); i++) tw = std::max({tw, std::fabs(rM[i] - r.M[i]), std::fabs(rT[i] - r.Ts[i])});
			if (rM.empty()) tw = 99;
			// (c) whole scene: third-octave bands within 40 dB of the peak, 2 s windows, mean difference; and overall level.
			// The limit is the reference's own spread: the same reference compiled without -ffast-math (test/ref2)
			// differs from itself this much once its chaos parts, so the port is held to 1.5 times that, and at least 3 dB.
			double meanD = bandDiff(a, b);
			double spread = 0; auto ref2 = readRaw(refDir + "2/raw-" + nm + ".f32");
			if (!ref2.empty()) { std::vector<double> c(ref2.size() / 2); for (size_t i = 0; i < c.size(); i++) c[i] = 0.5 * ((double)ref2[2 * i] + ref2[2 * i + 1]); spread = bandDiff(c, b); }
			double lv = dB(rmsOf(a, 0, n)) - dB(rmsOf(b, 0, n));
			double lim = std::max(3.0, 1.5 * spread);
			ok[si] = split >= 3 * 96000 && tw <= 0.025 && meanD <= lim && std::fabs(lv) <= 4.0 && r.guards == 0;
			det[si] = fmt("%s: same to %.1f s, thermal within %.3f, bands %.1f dB (reference against itself %.1f), level %+.1f dB; ", nm, split / 96000.0, tw, meanD, spread, lv);
			if (std::string(nm) == "heat") {
				double md = 0;
				for (long s = 0; s + 9600 <= 5 * 96000; s += 9600) md = std::max(md, std::fabs(dB(rmsOf(a, s, s + 9600)) - dB(rmsOf(b, s, s + 9600))));
				det[si] += fmt("first 5 s within %.4f dB; ", md);
				if (md > 0.1) ok[si] = false;
			}
		});
		report(ok[0] && ok[1] && ok[2] && ok[3], "1 reference match", det[0] + det[1] + det[2] + det[3]);
	}

	Knobs k0; k0.age = 0; k0.pings = 0; k0.pick = 0; k0.amp = 0;

	// ---------------------------------------------------------- 2. cold = Plate Driver (three regimes, tuning)
	auto regimes = [&](double fe, const std::string& tag) {
		// Plate Driver's swell: drive 250 -> 60000 over 20 s, held 4 s; HEAT 0, CAST 0, PINGS 0.
		Engine* e = new Engine(); e->setRate(fe); e->build(0); e->reset(); e->f1 = 65.4; e->meterLen = (int)(0.5 * fe);
		std::vector<std::pair<double, double>> up;
		for (long i = 0; i < (long)(24 * fe); i++) {
			double t = i / fe;
			if (i % BLK == 0) { baseS(e->c); e->c.cast = 0; e->c.ping = 0; e->c.focus = 0.5; e->c.fc = 262; e->c.drive = t < 20 ? xlerpS(250, 60000, t / 20) : 60000; }
			e->process(0);
			if (e->meterN == 0) up.push_back({e->c.drive, e->pr});
		}
		long g = e->guards; delete e;
		auto stat = [&](double lo, double hi, int what) {
			std::vector<double> x; for (auto& p : up) if (p.first >= lo && p.first <= hi) x.push_back(p.second);
			if (x.empty()) return -1.0; std::sort(x.begin(), x.end());
			return what == 0 ? x.back() : x[x.size() / 2]; };
		double pure = stat(250, 600, 0), split = stat(4000, 9000, 1), roar = stat(35000, 60000, 0);
		report(pure < 1.2 && split > 1.8 && split < 8 && roar > 10 && g == 0, "2 cold: three regimes " + tag,
			fmt("max PR %.2f at drive 250-600, median %.2f at 4k-9k, max %.1f at 35k-60k, guards %ld", pure, split, roar, g));
	};
	auto tuning = [&](double fe, const std::string& tag) {
		std::vector<double> cents(5);
		parallelFor(5, [&](int i) {
			int oct = i - 2;
			Knobs k = k0; k.drive = 0.2; k.focus = 0.8; k.pitch = oct; k.cast = 0;
			KRun r(fe, k); std::vector<double> L, R; long n = (long)(4 * fe); L.reserve(n); R.reserve(n);
			r.run(4.0, [&](double, double) { L.push_back(r.e->outL); R.push_back(r.e->outR); }, nullptr, k);
			double fs = C4 * std::pow(2.0, oct);
			double fl = peakFreq(L, (long)(2.0 * fe), (long)(2.0 * fe), fe, fs * 0.84, fs * 1.19);
			double fr = peakFreq(R, (long)(2.0 * fe), (long)(2.0 * fe), fe, fs * 0.84, fs * 1.19);
			double f = rmsOf(L, (long)(2 * fe), n) > rmsOf(R, (long)(2 * fe), n) ? fl : fr;
			cents[i] = 1200 * std::log2(f / fs);
		});
		double worst = 0; std::string det;
		for (int i = 0; i < 5; i++) { det += fmt("C%d %+.1f ", 2 + i, cents[i]); worst = std::max(worst, std::fabs(cents[i])); }
		report(worst <= 25, "2 cold: tuning " + tag, det + "cents (Plate Driver's limit at its default stretch: 25)");
	};
	if (want(2)) { regimes(96000, "(96k)"); tuning(96000, "(96k)"); }

	// ---------------------------------------------------------- 3. heat loosens
	auto loosens = [&](double fe, const std::string& tag) {
		const double heats[7] = {0.0, 0.2, 0.25, 0.3, 0.35, 0.4, 0.45};
		std::vector<double> pr(7);
		parallelFor(7, [&](int i) {
			// the heat sketch's loop: focus at G3 on the untracked body, gain 40, force 2500
			Knobs k = k0; k.drive = 0.485; k.gain = 40; k.trackPlate = false; k.pitch = std::log2(196.0 / C4); k.heat = heats[i];
			KRun r(fe, k); r.settle(k);
			double s = 0; int c = 0; long n = 0;
			r.run(10.0, [&](double t, double) { if (t >= 8 && r.e->meterN == 0) { s += r.e->pr; c++; } n++; }, nullptr, k);
			pr[i] = s / std::max(c, 1);
		});
		double best = 0; std::string det = fmt("DRIVE 0.485: cold PR %.2f; ", pr[0]);
		for (int i = 1; i < 7; i++) { best = std::max(best, pr[i]); det += fmt("HEAT %.2f PR %.1f; ", heats[i], pr[i]); }
		report(pr[0] < 1.2 && best > 2.5, "3 heat loosens " + tag, det);
	};
	if (want(3)) loosens(96000, "(96k)");

	// ---------------------------------------------------------- 4. sag
	if (want(4)) {
		// Light drive, FOCUS 0: the loop sings whichever partial it likes, and heat can hand the song to another
		// one. So the sung pitch is compared with where that same partial sits on the cold body.
		const double sags[2] = {0.67, 0.0};
		double semis[2], fs[2]; int tops[2];
		parallelFor(2, [&](int si) {
			Knobs k = k0; k.drive = 0.08; k.focus = 0; k.sag = sags[si]; k.heat = 0.45;
			KRun r(96000, k); r.settle(k);
			std::vector<double> m; m.reserve(8 * 96000);
			std::vector<int> cnt(N, 0);
			r.run(8.0, [&](double t, double y) { m.push_back(y); if (t > 4 && r.e->meterN == 0) cnt[r.e->top]++; }, nullptr, k);
			int j = (int)(std::max_element(cnt.begin(), cnt.end()) - cnt.begin());
			double fw = r.e->w[j] / (2 * PI), cold = r.e->f0[j] * r.e->c.tune;
			fs[si] = peakFreq(m, 4 * 96000, 4 * 96000, 96000, fw * 0.9, fw * 1.1);
			semis[si] = 12 * std::log2(cold / fs[si]); tops[si] = j;
		});
		report(semis[0] >= 5 && semis[0] <= 9 && std::fabs(100 * semis[1]) <= 30, "4 sag",
			fmt("HEAT 0.45: at SAG 0.67 mode %d sings %.1f Hz, %.1f semitones under its cold pitch; at SAG 0 mode %d is %+.0f cents off", tops[0], fs[0], semis[0], tops[1], -100 * semis[1]));
	}

	// ---------------------------------------------------------- 5. melting and latent heat
	auto latent = [&](double fe, const std::string& tag) {
		double tl[2]; double top[2];
		for (int li = 0; li < 2; li++) {
			Knobs k = k0; k.drive = 0; k.heat = 1.0; k.latent = li;
			KRun r(fe, k);
			double t5 = -1, t95 = -1; top[li] = 0;
			r.run(60.0, [&](double t, double) {
				double liq = 10 * clampd(r.e->M, 0, 1); top[li] = std::max(top[li], liq);
				if (t5 < 0 && liq >= 0.5) t5 = t; if (t95 < 0 && liq >= 9.5) t95 = t; }, nullptr, k);
			tl[li] = t95 > 0 && t5 >= 0 ? t95 - t5 : 1e9;
		}
		report(top[0] >= 9.99 && top[1] >= 9.99 && tl[1] >= 3 * tl[0], "5 melting, latent heat " + tag,
			fmt("LIQUID reaches %.2f / %.2f V; 5%% to 95%% liquid in %.2f s at LATENT 0, %.2f s at LATENT 1 (x%.1f)", top[0], top[1], tl[0], tl[1], tl[1] / tl[0]));
	};
	if (want(5)) latent(96000, "(96k)");

	// ---------------------------------------------------------- 6. narrow flame
	if (want(6)) {
		double mMin = 1, mMax = 0, mTop = 0; std::string zones[2]; int differ = 0;
		const double flames[2] = {0.1, 0.9};
		for (int fi = 0; fi < 2; fi++) {
			Knobs k = k0; k.drive = 0; k.width = 0.2; k.heat = 1.0; k.flame = flames[fi];
			KRun r(96000, k);
			r.run(20.0, [&](double t, double) { if (fi == 0) { mTop = std::max(mTop, r.e->M); if (t >= 15) { mMin = std::min(mMin, r.e->M); mMax = std::max(mMax, r.e->M); } } }, nullptr, k);
			for (int z = 0; z < K; z++) zones[fi] += r.e->phi[z] > 0.5 ? '#' : '.';
		}
		for (int z = 0; z < K; z++) if (zones[0][z] != zones[1][z]) differ++;
		report(mMin >= 0.2 && mMax <= 0.7 && mTop < 0.999 && differ >= 4, "6 narrow flame",
			fmt("WIDTH 0.2: liquid %.0f%% to %.0f%% over 15-20 s (max %.0f%%); liquid zones at FLAME 0.1 [%s], at 0.9 [%s]", 100 * mMin, 100 * mMax, 100 * mTop, zones[0].c_str(), zones[1].c_str()));
	}

	// ---------------------------------------------------------- 7. casting
	if (want(7)) {
		// Ten cycles: melt with the loop roaring through the liquid, quench, then let the cold body sing.
		// Loudest mode and sung pitch after each.
		const double castK[2] = {0.5, 0.0};
		std::vector<int> tops[2]; std::vector<double> pitches[2]; long casts[2]; double forgot = 0;
		parallelFor(2, [&](int ci) {
			Knobs k = k0; k.cast = castK[ci]; k.focus = 0.3; k.trackPlate = false; k.mass = 0.1; k.width = 1;
			KRun r(96000, k);
			for (int cyc = 0; cyc < 10; cyc++) {
				double t0 = cyc * 9.0;
				std::vector<int> cnt(N, 0); std::vector<double> m; m.reserve(2 * 96000);
				r.run(9.0, [&](double t, double y) {
					double x = t - t0;
					if (x >= 7) { if (r.e->meterN == 0) cnt[r.e->top]++; m.push_back(y); }
				}, [&](double t, Knobs& kk) {
					double x = t - t0;
					kk.heat = x < 3 ? 1.0 : 0.0;           // melt
					kk.quenching = x >= 3 && x < 5;        // quench
					kk.drive = x < 3.4 ? 0.55 : x >= 5.5 ? 0.3 : 0.0;
				}, k, t0);
				tops[ci].push_back((int)(std::max_element(cnt.begin(), cnt.end()) - cnt.begin()));
				pitches[ci].push_back(peakFreq(m, 0, (long)m.size(), 96000, 30, 4000));
			}
			casts[ci] = r.e->casts;
			if (ci == 0) {
				r.e->forgetCasting();
				Knobs kk = k; kk.drive = 0;
				r.run(2.0, [](double, double) {}, nullptr, kk, 90);
				for (int j = 0; j < N; j++) forgot = std::max(forgot, std::fabs(r.e->cast[j] - 1.0));
			}
		});
		// The loop keeps singing the partial it couples to best (mode 5 at this POS), so the loudest mode rarely
		// changes; what a casting changes is where that partial and its neighbours sit. Count distinct sung pitches
		// (more than 30 cents apart) after the ten castings.
		auto distinct = [](const std::vector<double>& p) { std::vector<double> u; for (double f : p) { bool nw = true; for (double g : u) if (std::fabs(1200 * std::log2(f / g)) < 30) nw = false; if (nw) u.push_back(f); } return (int)u.size(); };
		std::set<int> d0(tops[0].begin(), tops[0].end());
		int p0 = distinct(pitches[0]), p1 = distinct(pitches[1]);
		std::string det = fmt("CAST 0.5: %ld castings, sung Hz", casts[0]);
		for (double p : pitches[0]) det += fmt(" %.0f", p);
		det += fmt(" (%d different pitches; loudest mode took %d values); CAST 0: sung Hz", p0, (int)d0.size());
		for (double p : pitches[1]) det += fmt(" %.0f", p);
		det += fmt(" (%d); after Forget casting the tuning is off by at most %.2g", p1, forgot);
		report(p0 >= 5 && p1 == 1 && forgot < 1e-6 && casts[0] >= 10, "7 casting", det);
	}

	// ---------------------------------------------------------- 8. supercooling
	if (want(8)) {
		double mAfter5 = 0, tFreeze = -1, tSelf = -1;
		{
			Knobs k = k0; k.drive = 0; k.supercool = 1; k.heat = 1.0;
			KRun r(96000, k); r.settle(k);
			Knobs kk = k; kk.heat = 0.4;
			double minM = 1;
			r.run(5.0, [&](double, double) { minM = std::min(minM, r.e->M); }, nullptr, kk);
			mAfter5 = minM;
			r.e->strike(420 * 1.0);
			r.run(2.0, [&](double t, double) { if (tFreeze < 0 && r.e->M < 0.5) tFreeze = t - 5; }, nullptr, kk, 5);
		}
		{
			Knobs k = k0; k.drive = 0; k.supercool = 0; k.heat = 1.0;
			KRun r(96000, k); r.settle(k);
			Knobs kk = k; kk.heat = 0.4;
			r.run(10.0, [&](double t, double) { if (tSelf < 0 && r.e->M < 0.5) tSelf = t; }, nullptr, kk);
		}
		report(mAfter5 > 0.999 && tFreeze >= 0 && tFreeze <= 0.5 && tSelf > 0, "8 supercooling",
			fmt("SUPERCOOL 1: %.0f%% liquid at the least over 5 s at HEAT 0.4; set %.3f s after a strike; SUPERCOOL 0: sets by itself after %.2f s", 100 * mAfter5, tFreeze, tSelf));
	}

	// ---------------------------------------------------------- 9. energy
	if (want(9)) {
		const double heats[4] = {0.0, 0.45, 0.6, 1.0};
		std::string det[4]; bool ok[4];
		parallelFor(4, [&](int i) {
			// (a) after a strike, drive 0, steady heat: energy (kinetic + linear + stretch) never rises (10 ms means, 0.5%)
			Knobs k = k0; k.drive = 0; k.heat = heats[i] * 0.5 / 0.5; k.heat = heats[i]; k.thick = 0; k.curve = 1;
			k.heat = heats[i];
			KRun r(96000, k); r.settle(k); r.e->strike(420);
			double last = 1e300, acc = 0, worst = 1; int rises = 0, c = 0; long idx = 0;
			r.run(4.0, [&](double, double) {
				acc += r.e->energy + r.e->epot; idx++;
				if (++c == 960) { double m = acc / c; if (idx > 2000 && m > last * 1.005) { rises++; worst = std::max(worst, m / last); } last = m; acc = 0; c = 0; }
			}, nullptr, k);
			// (b) hard drive, then cut: below -80 dB of the driven level within 30 s
			Knobs kd = k0; kd.drive = 0.9; kd.heat = heats[i]; kd.amp = 0.6;
			KRun d(96000, kd); d.settle(kd);
			std::vector<double> m; m.reserve(34 * 96000);
			d.run(4.0, [&](double, double y) { m.push_back(y); }, nullptr, kd);
			Knobs kc = kd; kc.drive = 0;
			d.run(30.0, [&](double, double y) { m.push_back(y); }, nullptr, kc, 4);
			double lvl = rmsOf(m, 3 * 96000, 4 * 96000), tail = rmsOf(m, 33 * 96000, 34 * 96000);
			double drop = dB(tail) - dB(lvl);
			ok[i] = rises == 0 && drop <= -80;
			det[i] = fmt("HEAT %.2f: rises %d (worst x%.4f), %.0f dB after 30 s; ", heats[i], rises, worst, std::max(drop, -300.0));
		});
		report(ok[0] && ok[1] && ok[2] && ok[3], "9 energy", det[0] + det[1] + det[2] + det[3]);
	}

	// ---------------------------------------------------------- 10. quench bloom
	if (want(10)) {
		// Default knobs (PINGS 0.3): the molten body is driven, let ring, then quenched with the drive off. The
		// freezing zones crack (stress pings) and the body flares. Six bodies, since the cracks fall at random.
		std::vector<double> rise(6); std::vector<double> mEnd(6);
		parallelFor(6, [&](int bi) {
			Knobs k = k0; k.body = bi; k.pings = 0.3; k.heat = 1.0; k.drive = 0.5; k.quenchAmt = 0.4;
			KRun r(96000, k); r.settle(k);
			std::vector<double> m; m.reserve(8 * 96000);
			r.run(3.0, [&](double, double y) { m.push_back(y); }, nullptr, k);
			Knobs kr = k; kr.drive = 0;
			r.run(0.3, [&](double, double y) { m.push_back(y); }, nullptr, kr, 3);
			Knobs kq = kr; kq.quenching = true;
			r.run(3.0, [&](double, double y) { m.push_back(y); mEnd[bi] = r.e->M; }, nullptr, kq, 3.3);
			long a = (long)(3.2 * 96000), b = (long)(3.3 * 96000);
			double before = rmsOf(m, a, b), most = 0;
			for (long s = b; s + 9600 <= (long)m.size(); s += 4800) most = std::max(most, rmsOf(m, s, s + 9600));
			rise[bi] = dB(most) - dB(before);
		});
		std::vector<double> sr = rise; std::sort(sr.begin(), sr.end());
		std::string det = "loudest 100 ms during the freeze against the 100 ms before, BODY 0-5:";
		for (double x : rise) det += fmt(" %+.1f", x);
		det += fmt(" dB (median %+.1f); all solid after 3 s: %s", 0.5 * (sr[2] + sr[3]), *std::max_element(mEnd.begin(), mEnd.end()) < 0.01 ? "yes" : "no");
		report(0.5 * (sr[2] + sr[3]) >= 3 && *std::max_element(mEnd.begin(), mEnd.end()) < 0.01, "10 quench bloom", det);
	}

	// ---------------------------------------------------------- 16. self-heating (spec 3.6, untested there)
	if (want(16)) {
		// SELF 1, HEAT 0, a held roar: the body reaches the melting point by itself within 5 to 20 s (first 5%
		// liquid); at SELF 0 it never does; with the drive cut it cools and sets again. Melting deadens the roar,
		// which cools the body, so at most drives it breathes in and out of the melt instead of sticking.
		double tMelt[2] = {-1, -1}, mMax[2] = {0, 0}, mEnd = 1; int breaths = 0;
		parallelFor(2, [&](int i) {
			Knobs k = k0; k.self = i == 0 ? 1.0 : 0.0; k.drive = 0.9; k.heat = 0;
			KRun r(96000, k); bool liq = false;
			r.run(30.0, [&](double t, double) {
				double m = r.e->M; mMax[i] = std::max(mMax[i], m);
				if (tMelt[i] < 0 && m > 0.05) tMelt[i] = t;
				if (i == 0) { if (!liq && m > 0.05) { liq = true; breaths++; } else if (liq && m < 0.01) liq = false; }
			}, nullptr, k);
			if (i == 0) { Knobs kc = k; kc.drive = 0; r.run(15.0, [&](double, double) { mEnd = r.e->M; }, nullptr, kc, 30); }
		});
		report(tMelt[0] >= 5 && tMelt[0] <= 20 && tMelt[1] < 0 && mEnd < 0.01, "16 self-heating",
			fmt("SELF 1, DRIVE 0.9, HEAT 0: first melts after %.1f s, up to %.0f%% liquid, %d melts in 30 s; SELF 0: %s; drive cut: %.0f%% liquid 15 s later",
				tMelt[0], 100 * mMax[0], breaths, tMelt[1] < 0 ? "never melts" : "melts", 100 * mEnd));
	}

	// ---------------------------------------------------------- 11. fuzz (and 12: other sample rates)
	auto randomKnobs = [](Rng& g) {
		Knobs k; k.heat = g.uni(); k.quenchAmt = g.uni(); k.mass = g.uni(); k.flame = g.uni(); k.width = g.uni(); k.latent = g.uni();
		k.supercool = g.uni(); k.self = g.uni(); k.pings = g.uni(); k.tension = -2 + 3 * g.uni(); k.sag = g.uni(); k.loose = g.uni();
		k.cast = g.uni(); k.body = (int)(g.uni() * 16) % 16; k.size = 20 * std::pow(10.0, g.uni()); k.thick = g.uni(); k.curve = g.uni();
		k.decay = 0.15 + 3.85 * g.uni(); k.drive = g.uni(); k.gain = 20 * std::pow(30.0, g.uni()); k.focus = g.uni(); k.pitch = -5 + 10 * g.uni();
		k.trackPlate = g.uni() < 0.5; k.pos = 0.05 + 0.9 * g.uni(); k.grab = g.uni() < 0.3 ? g.uni() : 0; k.release = g.uni();
		k.gate = g.uni() < 0.8 ? 1 : 0; k.pick = g.uni(); k.amp = g.uni(); k.out = g.uni(); k.mallet = g.uni(); k.inLevel = g.uni();
		k.bright = g.uni(); k.age = g.uni(); k.quenching = g.uni() < 0.15;
		return k;
	};
	auto fuzz = [&](double host, int patches, double secs, const std::string& name) {
		std::atomic<int> bad(0); std::atomic<long> guards(0), clamps(0); std::atomic<int> clampedPatches(0); std::mutex mu; std::string det;
		parallelFor(patches, [&](int p) {
			Rng g; g.seed(1000 + p * 7919ULL);
			Knobs k = randomKnobs(g);
			Chain ch(host, 77 + p, k.body);
			long n = (long)(secs * host), stepN = (long)(0.5 * host); double peak = 0; bool nanSeen = false; double inBurst = 0;
			for (long i = 0; i < n; i++) {
				if (i % stepN == 0 && i) {
					Knobs k2 = randomKnobs(g); k2.body = k.body;
					if (g.uni() < 0.5) k = k2; else { k.drive = k2.drive; k.pitch = k2.pitch; k.focus = k2.focus; k.heat = k2.heat; k.quenching = k2.quenching; k.flame = k2.flame; k.loose = k2.loose; }
				}
				double sv = (i % (long)(0.37 * host) == 0) ? g.uni() : 0;
				if (i % (long)(1.3 * host) == 0) inBurst = 0.2 * host;
				double inV = 0; if (inBurst > 0) { inBurst--; inV = 10 * g.bi(); }
				double L, R; ch.step(k, inV, sv, L, R);
				if (!std::isfinite(L) || !std::isfinite(R)) nanSeen = true;
				peak = std::max({peak, std::fabs(L), std::fabs(R)});
			}
			guards += ch.e->guards; clamps += ch.e->clampN; if (ch.e->clampN) clampedPatches++;
			if (nanSeen || peak > 11.0 || ch.e->guards) { bad++; std::lock_guard<std::mutex> l(mu); if (det.size() < 300) det += fmt("patch %d: %s peak %.1f guards %ld; ", p, nanSeen ? "NaN" : "", peak, ch.e->guards); }
		});
		double frac = 100.0 * clamps / (patches * secs * host * (host < 88200 ? 2 : 1));
		report(bad == 0 && frac <= 0.1, name, fmt("%d patches x %.0f s at %.1f kHz, %d bad, resets %ld, energy clamps on %.4f%% of samples in %d patches. %s",
			patches, secs, host / 1000, (int)bad, (long)guards, frac, (int)clampedPatches, det.c_str()));
	};
	if (want(11)) fuzz(48000, getenv("FUZZN") ? atoi(getenv("FUZZN")) : 400, 20, "11 fuzz (48k)");

	if (want(12)) {
		regimes(88200, "(44.1k host)"); tuning(88200, "(44.1k host)"); loosens(88200, "(44.1k host)"); latent(88200, "(44.1k host)");
		fuzz(44100, 100, 10, "12 fuzz (44.1k)");
		fuzz(96000, 100, 10, "12 fuzz (96k)");
	}

	// ---------------------------------------------------------- 13. silence
	if (want(13)) {
		bool ok = true; std::string det;
		for (double h : {0.0, 0.45, 1.0}) {
			Chain ch(48000, 5); Knobs k; k.age = 0; k.heat = h; double worst = 0;
			for (long i = 0; i < 62 * 48000L; i++) { k.drive = i < 2 * 48000 ? 0.6 : 0.0; double L, R; ch.step(k, 0, 0, L, R); if (i > 60 * 48000L) worst = std::max({worst, std::fabs(L), std::fabs(R)}); }
			det += fmt("HEAT %.2f: max %.3g V after 60 s; ", h, worst); if (worst != 0) ok = false;
		}
		report(ok, "13 silence", det + "(PINGS at default: a cooling body may still tick, and must fall silent between ticks)");
	}

	// ---------------------------------------------------------- 14. speed
	if (want(14)) {
		std::string det;
		for (double h : {0.0, 0.8}) {
			Chain ch(48000, 3); Knobs k; k.drive = 0.6; k.heat = h; long n = 10 * 48000;
			double L, R; for (long i = 0; i < 2 * 48000; i++) ch.step(k, 0, 0, L, R);
			auto t0 = std::chrono::steady_clock::now();
			for (long i = 0; i < n; i++) ch.step(k, 0, 0, L, R);
			double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
			det += fmt("%s: %.1f%% of one core; ", h > 0 ? "molten" : "cold", 100 * secs / 10.0);
		}
		report(true, "14 speed (48 kHz host)", det);
	}

	if (want(15)) presetTest(out);
	std::printf("\n%d passed, %d failed\n", nPass, nFail);
	return nFail ? 1 : 0;
}
