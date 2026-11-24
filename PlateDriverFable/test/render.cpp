// Plate Driver offline checks (no Rack needed): make test
// Every check prints PASS or FAIL. Demo WAVs go to the folder given as the first argument.
#include "../src/dsp/PlateEngine.hpp"
#include "../src/dsp/Mapping.hpp"
#include "scenes.h"
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <string>
#include <complex>
#include <chrono>
#include <map>
#include <thread>
#include <atomic>
#include <mutex>
#include <cstdarg>
#include <functional>
using namespace plate;

static int nFail = 0, nPass = 0;
static void report(bool ok, const char* name, const std::string& detail) {
	std::printf("%s  %-34s %s\n", ok ? "PASS" : "FAIL", name, detail.c_str());
	if (ok) nPass++; else nFail++;
}
static std::string fmt(const char* f, ...) {
	char b[512]; va_list a; va_start(a, f); vsnprintf(b, sizeof b, f, a); va_end(a); return b;
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
// power spectrum of mono x[from .. from+len) (averaged over 8192-point frames)
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
// strongest component (optionally only between lo and hi Hz), parabolic interpolation
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

// ------------------------------------------------------------------ engine-rate render of a scene
static std::vector<double> lastL, lastR;
struct Run { std::vector<float> st; std::vector<double> prT; long guards = 0; double secs = 0; };
static long KCLIP = 0, SOFT = 0;
static Run renderScene(const Scene& sc, double fe = 96000, int type = SHEET, int cast = 0) {
	Engine* e = new Engine(); e->setRate(fe); if (getenv("KMUL")) e->kmul = atof(getenv("KMUL")); e->build(type, cast); e->reset(); e->f1 = sc.f1;
	Run r; long n = (long)(sc.dur * fe); r.st.resize(2 * n);
	auto t0 = std::chrono::steady_clock::now();
	for (long i = 0; i < n; i++) {
		if (i % BLK == 0) sc.ctl(i / fe, *e);
		e->process(0.0);
		r.st[2 * i] = (float)e->outL; r.st[2 * i + 1] = (float)e->outR;
		if (e->meterN == 0) r.prT.push_back(e->pr);
	}
	r.secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
	r.guards = e->guards; KCLIP = e->kclipN; SOFT = e->softN; delete e; return r;
}
static std::vector<double> monoOf(const std::vector<float>& st) { std::vector<double> m(st.size() / 2); for (size_t i = 0; i < m.size(); i++) m[i] = 0.5 * ((double)st[2 * i] + st[2 * i + 1]); return m; }

// ------------------------------------------------------------------ full module chain (knobs -> host-rate volts)
struct Chain {
	Engine* e; Age age; Decimator dec; Leveller lev; double host; bool os; int ctr = 0; double gateS = 1;
	double level = 0.7; bool ageOn = true;
	Chain(double host_, uint64_t seed = 1) : host(host_) {
		e = new Engine(); if (getenv("KMUL")) e->kmul = atof(getenv("KMUL")); os = host < 88200; e->setRate(os ? 2 * host : host); e->build(SHEET, 0); e->reset();
		age.init(seed); lev.setRate(host); dec.reset(); lev.reset();
	}
	~Chain() { delete e; }
	// call once per host sample; in = volts at IN; strike velocity > 0 triggers
	void step(const Knobs& k0, double inV, double strikeVel, double& L, double& R) {
		Knobs k = k0;
		if (ctr == 0) {
			if (ageOn) age.update(BLK / e->fe, e->emean);
			mapKnobs(k, *e, ageOn ? &age : nullptr);
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

static void presetTest(const std::string& out) {
	// ---------------------------------------------------------- 14. presets (from test/presets.txt, written by tools/make_presets.py)
	if (!getenv("SKIP_PRESETS")) {
		FILE* f = std::fopen("test/presets.txt", "r");
		if (!f) report(false, "14 presets", "no test/presets.txt (run tools/make_presets.py)");
		else {
			bool ok = true; std::string det; char nm[128]; int idx = 0;
			double v[30];
			while (std::fscanf(f, "%127s", nm) == 1) {
				for (int i = 0; i < 30; i++) if (std::fscanf(f, "%lf", &v[i]) != 1) v[i] = 0;
				idx++;
				Knobs k; k.type = (int)v[0]; k.cast = (int)v[1]; k.size = v[2]; k.thick = v[3]; k.curve = v[4]; k.stretch = v[5]; k.decay = v[6];
				k.bright = v[7]; k.age = v[8]; k.drive = v[10]; k.gain = v[11]; k.focus = v[12]; k.pitch = v[13]; k.phase = v[14]; k.trackPlate = v[15] > 0.5;
				k.driverPos = v[16]; k.spread = v[17]; k.grab = v[18]; k.pick = v[19]; k.amp = v[20]; k.drv = v[21]; k.rat = v[22]; k.bleed = v[23]; k.out = v[24];
				k.pump = v[25]; k.rate = v[26]; k.mallet = v[27]; k.inLevel = v[28]; k.release = v[29];
				Chain ch(48000, 0x51A7E0000ULL + 7919 * idx); ch.level = v[9];
				std::string name = nm;
				bool needsIn = name == "Resonator";
				long n = 10 * 48000; std::vector<float> st(2 * n); double ss = 0, pk = 0, prs = 0; int prn = 0; Rng g; g.seed(5);
				for (long i = 0; i < n; i++) {
					double t = i / 48000.0, inV = 0, sv = 0;
					if (needsIn) { double ph = std::fmod(t, 0.5); inV = ph < 0.03 ? 8 * g.bi() * std::exp(-ph / 0.008) : 0; }   // noise hits, like a drum
					if (name == "Infinite_Gong" && i == 0) sv = 1.0;
					Knobs kk = k;
					if (name == "Metal_Stabs") kk.gate = std::fmod(t, 0.75) < 0.25 ? 1 : 0;
					double L, R; ch.step(kk, inV, sv, L, R);
					st[2 * i] = (float)(L / 10); st[2 * i + 1] = (float)(R / 10);
					if (t >= 4) { ss += L * L + R * R; pk = std::max({pk, std::fabs(L), std::fabs(R)}); if (i % 2400 == 0) { prs += ch.e->pr; prn++; } }
				}
				double rms = std::sqrt(ss / (2.0 * 6 * 48000));
				for (char& c : name) if (c == '_') c = ' ';
				writeWav(out + fmt("/preset-%02d-%s.wav", idx, nm), st, 48000);
				bool good = rms > 0.3 && ch.e->guards == 0;
				if (!good) ok = false;
				det += fmt("%s%s %.1f V rms, PR %.1f; ", good ? "" : "BAD ", name.c_str(), rms, prs / std::max(prn, 1));
			}
			std::fclose(f);
			report(ok, "14 presets", det);
		}
	}
}

int main(int argc, char** argv) {
	if (argc > 3 && std::string(argv[3]) == "dump") { auto sc = makeScenes(); for (auto& x : sc) if (x.name == std::string(argv[4])) { Run r = renderScene(x); FILE* f = std::fopen((std::string(argv[1]) + "/raw-" + argv[4] + ".f32").c_str(), "wb"); std::fwrite(r.st.data(), 4, r.st.size(), f); std::fclose(f); std::printf("guards %ld secs %.2f kclip %ld soft %ld\n", r.guards, r.secs, KCLIP, SOFT); } return 0; }
	std::string out = argc > 1 ? argv[1] : "test/out";
	std::string refDir = argc > 2 ? argv[2] : "test/ref";
	if (getenv("ONLY_PRESETS")) { presetTest(out); return nFail ? 1 : 0; }
	auto scenes = makeScenes();
	std::map<std::string, Scene*> S; for (auto& s : scenes) S[s.name] = &s;

	// ---------------------------------------------------------- 1. reference match
	{
		bool allOk = true; std::string det;
		for (const char* nm : {"swell", "h-stages", "h-pump", "h-pulse"}) {
			auto ref = readRaw(refDir + "/raw-" + nm + ".f32");
			if (ref.empty()) { det += std::string(nm) + ": no reference (run tools/proto first); "; allOk = false; continue; }
			Run r = renderScene(*S[nm]);
			auto a = monoOf(r.st); std::vector<double> b(ref.size() / 2); for (size_t i = 0; i < b.size(); i++) b[i] = 0.5 * ((double)ref[2 * i] + ref[2 * i + 1]);
			// Chaotic passages cannot match sample for sample: even the reference compiled with different
			// rounding differs from itself by 2.7 dB on average in swell. So: mean third-octave difference
			// over 6 s windows, bands within 40 dB of the window's peak, at most 3 dB.
			long win = 6 * 96000; double sum = 0; int nw = 0;
			for (long s = 0; s + win <= (long)std::min(a.size(), b.size()); s += win) {
				auto ba = thirdOctaves(spectrum(a, s, win), 96000), bb = thirdOctaves(spectrum(b, s, win), 96000);
				double pk = *std::max_element(bb.begin(), bb.end()); double d = 0; int k = 0;
				for (size_t i = 0; i < ba.size(); i++) if (bb[i] > pk - 40) { d += std::fabs(ba[i] - bb[i]); k++; }
				if (k) { sum += d / k; nw++; }
			}
			double meanD = nw ? sum / nw : 99;
			det += fmt("%s mean %.1f dB; ", nm, meanD);
			if (meanD > 3.0) allOk = false;
			if (std::string(nm) == "swell") {
				long n15 = (long)(1.5 * 96000); double ea = 0, eb = 0, md = 0;
				for (long s = 9600; s + 9600 <= n15; s += 9600) {
					ea = eb = 0; for (long i = s; i < s + 9600; i++) { ea += a[i] * a[i]; eb += b[i] * b[i]; }
					md = std::max(md, std::fabs(10 * std::log10((ea + 1e-30) / (eb + 1e-30))));
				}
				det += fmt("first 1.5 s within %.3f dB; ", md);
				if (md > 0.1) allOk = false;
			}
			if (r.guards) { allOk = false; det += fmt("%s guards %ld; ", nm, r.guards); }
		}
		report(allOk, "1 reference match", det);
	}

	// ---------------------------------------------------------- CAST 0 = the reference's random sequence
	{
		Engine* e = new Engine(); e->setRate(96000); e->build(SHEET, 0);
		uint64_t rs = 0x2545F4914F6CDD1DULL;
		auto rnd = [&]() { rs ^= rs << 13; rs ^= rs >> 7; rs ^= rs << 17; return (double)(rs >> 11) * (1.0 / 9007199254740992.0); };
		double worst = 0; double r[N], ph[N];
		for (int j = 0; j < N; j++) { r[j] = 1 + 0.6 * (j + (j ? 0.45 * (2 * rnd() - 1) : 0)); ph[j] = 2 * PI * rnd(); }
		for (int j = 0; j < N; j++) worst = std::max({worst, std::fabs(r[j] - e->r[j]), std::fabs(ph[j] - e->ph[j])});
		for (int c = 0; c < C; c++) for (int j = 0; j < N; j++) {
			double A = (rnd() < 0.5 ? -1 : 1) / std::sqrt((double)C * N), L = (0.3 + rnd()) / std::sqrt((double)C) * 0.8, G = (rnd() < 0.5 ? -1 : 1) / std::sqrt((double)C);
			worst = std::max({worst, std::fabs(A - e->A[c][j]), std::fabs(L - e->Lm[c][j]), std::fabs(G - e->Sg[c][j])});
		}
		report(worst == 0, "1b CAST 0 = reference draws", fmt("worst difference %.3g", worst));
		delete e;
	}

	// sweep used by tests 2, 3 (reference sw2: focus 0.5, curve 0.6, nu 0, drive 5 -> 60000 over 30 s, back over 15 s)
	auto sweepRun = [&](double fe, std::vector<std::pair<double, double>>& upPR, std::vector<std::pair<double, double>>& downPR) {
		Engine* e = new Engine(); e->setRate(fe); e->build(SHEET, 0); e->reset(); e->f1 = 65.4; e->meterLen = (int)(0.5 * fe);
		long n = (long)(45 * fe); double drive = 0;
		for (long i = 0; i < n; i++) {
			double t = i / fe;
			if (i % BLK == 0) { baseS(e->c); e->c.focus = 0.5; drive = t < 30 ? xlerpS(5, 60000, t / 30) : xlerpS(60000, 5, (t - 30) / 15); e->c.drive = drive; }
			e->process(0);
			if (e->meterN == 0) (t < 30 ? upPR : downPR).push_back({drive, e->pr});
		}
		long g = e->guards; delete e; return g;
	};
	auto prAt = [](const std::vector<std::pair<double, double>>& v, double d) { double best = 1e30, pr = 0; for (auto& x : v) if (std::fabs(std::log(x.first / d)) < best) { best = std::fabs(std::log(x.first / d)); pr = x.second; } return pr; };
	// Tests 2 and 3 use the approved swell sketch (drive 250 -> 60000 over 20 s, held 4 s, down to 150 over 14 s).
	auto regimes = [&](double fe, const char* tag) {
		Engine* e = new Engine(); e->setRate(fe); e->build(SHEET, 0); e->reset(); e->f1 = 65.4; e->meterLen = (int)(0.5 * fe);
		std::vector<std::pair<double, double>> up, down;
		for (long i = 0; i < (long)(40 * fe); i++) { double t = i / fe; if (i % BLK == 0) S["swell"]->ctl(t, *e); e->process(0);
			if (e->meterN == 0) (t <= 24 ? up : down).push_back({e->c.drive, e->pr}); }
		long g = e->guards; delete e;
		auto stat = [](const std::vector<std::pair<double, double>>& v, double lo, double hi, int what) {
			std::vector<double> x; for (auto& p : v) if (p.first >= lo && p.first <= hi) x.push_back(p.second);
			if (x.empty()) return -1.0; std::sort(x.begin(), x.end());
			if (what == 0) return x.back(); if (what == 1) return x[x.size() / 2]; double s = 0; for (double y : x) s += y; return s / x.size(); };
		double pure = stat(up, 250, 600, 0), split = stat(up, 4000, 9000, 1), roar = stat(up, 35000, 60000, 0);
		report(pure < 1.2 && split > 1.8 && split < 8 && roar > 10 && g == 0, fmt("2 three regimes %s", tag).c_str(),
			fmt("max PR %.2f at 250-600, median %.2f at 4k-9k, max %.1f at 35k-60k, guards %ld", pure, split, roar, g));
		double hu = stat(up, 2500, 5000, 2), hd = stat(down, 2500, 5000, 2);
		report(hd > hu + 0.5, fmt("3 hysteresis %s", tag).c_str(), fmt("mean PR at drive 2.5k-5k: up %.2f, down %.2f", hu, hd));
	};
	regimes(96000, "(96k)");

	// ---------------------------------------------------------- 4. energy never rises without drive
	{
		bool ok = true; std::string det;
		for (int ty = 0; ty < NTYPES; ty++) {
			Engine* e = new Engine(); e->setRate(96000); e->build(ty, 0); e->reset(); e->f1 = 65.4;
			baseS(e->c); e->c.bloom = 4; e->c.curve = 1; e->c.nu = 4e-5; e->c.drive = 0; e->strike(420);
			// kinetic + linear potential + stretch potential, averaged over 10 ms (the instantaneous
			// split between them swings within a cycle); must never rise by more than 0.5 %.
			double last = 1e300, acc = 0; int rises = 0, k = 0; double worst = 1;
			for (long i = 0; i < 4 * 96000; i++) {
				e->process(0); acc += e->energy + e->epot;
				if (++k == 960) { double m = acc / k; if (i > 2000 && m > last * 1.005) { rises++; worst = std::max(worst, m / last); } last = m; acc = 0; k = 0; }
			}
			det += fmt("type %d rises %d (worst x%.4f); ", ty, rises, worst);
			if (rises) ok = false;
			delete e;
		}
		report(ok, "4 energy after a strike", det);
	}

	// helper: engine at fe driven by knobs, returns mono output, PR trace, pitch etc.
	auto knobRun = [&](double fe, Knobs k, double secs, std::vector<double>* prTrace, std::function<void(double, Knobs&)> mod) {
		Engine* e = new Engine(); e->setRate(fe); e->build(k.type, k.cast); e->reset(); e->meterLen = (int)(0.1 * fe);
		long n = (long)(secs * fe); std::vector<double> m(n); lastL.assign(n, 0); lastR.assign(n, 0);
		for (long i = 0; i < n; i++) {
			if (i % BLK == 0) { Knobs kk = k; if (mod) mod(i / fe, kk); mapKnobs(kk, *e, nullptr); }
			e->process(0); m[i] = 0.5 * (e->outL + e->outR); lastL[i] = e->outL; lastR[i] = e->outR;
			if (prTrace && e->meterN == 0) prTrace->push_back(e->pr);
		}
		delete e; return m;
	};
	Knobs k0; k0.age = 0; k0.pick = k0.amp = k0.drv = k0.bleed = 0;

	// ---------------------------------------------------------- 5. tuning
	// The sung tone rises with amplitude as the plate tightens (that is STRETCH). So: within 10 cents
	// at STRETCH 0, within 25 cents at the default STRETCH 1, both at DRIVE 0.2.
	auto tuning = [&](double fe, const char* tag) {
		bool ok = true; std::string det;
		for (double st : {0.0, 1.0}) {
			double worst = 0; det += fmt("STRETCH %.0f:", st);
			for (int oct = -2; oct <= 2; oct++) {
				Knobs k = k0; k.drive = 0.2; k.focus = 0.8; k.pitch = oct; k.stretch = st;
				knobRun(fe, k, 4.0, nullptr, nullptr);
				double fs = C4 * std::pow(2.0, oct);
				// louder channel (a mode can cancel in the mono sum); the sung tone, not its octave partner
				double fl = peakFreq(lastL, (long)(2.0 * fe), (long)(2.0 * fe), fe, fs * 0.84, fs * 1.19);
				double fr = peakFreq(lastR, (long)(2.0 * fe), (long)(2.0 * fe), fe, fs * 0.84, fs * 1.19);
				auto P = [&](std::vector<double>& x, double f) { double s = 0; for (long i = (long)(2 * fe); i < (long)(4 * fe); i++) s += x[i] * x[i]; return s; };
				double f = P(lastL, fl) > P(lastR, fr) ? fl : fr;
				double cents = 1200 * std::log2(f / fs);
				det += fmt(" C%d %+.1f", 4 + oct, cents);
				worst = std::max(worst, std::fabs(cents));
			}
			det += "; ";
			if (worst > (st == 0 ? 10 : 25)) ok = false;
		}
		report(ok, fmt("5 tuning %s", tag).c_str(), det);
	};
	tuning(96000, "(96k)");

	// ---------------------------------------------------------- 6. drive scaling with pitch
	{
		std::string det; double at[5];
		for (int oct = -2; oct <= 2; oct++) {
			Knobs k = k0; k.focus = 0.5; k.pitch = oct;
			std::vector<double> pr;
			knobRun(96000, k, 24.0, &pr, [](double t, Knobs& kk) { kk.drive = 0.15 + 0.85 * t / 24.0; });
			double kv = -1; bool settled = false;
			for (size_t i = 0; i < pr.size(); i++) { if (pr[i] < 1.15 && i > 4) settled = true; if (settled && pr[i] > 2.0) { kv = 0.15 + 0.85 * (i + 1) * 0.1 / 24.0; break; } }
			at[oct + 2] = kv; det += fmt("C%d %.2f; ", 4 + oct, kv);
		}
		bool ok = true; for (int i = 0; i < 5; i++) if (at[i] < 0 || std::fabs(at[i] - at[2]) > 0.15) ok = false;
		report(ok, "6 drive scaling", det);
	}

	// ---------------------------------------------------------- 7. STRETCH
	{
		std::string det; bool ok = true;
		const double lo[3] = {10, 100, 180}, hi[3] = {40, 150, 250}; const double st[3] = {0, 1, 2};
		for (int i = 0; i < 3; i++) {
			Engine* e = new Engine(); e->setRate(96000); e->build(SHEET, 0); e->reset(); e->f1 = 65.4;
			long n = 30 * 96000; std::vector<double> m(n);
			for (long s = 0; s < n; s++) { double t = s / 96000.0; if (s % BLK == 0) { baseS(e->c); e->c.stretch = st[i]; e->c.focus = 1; e->c.drive = xlerpS(5, 2500, t / 30); } e->process(0); m[s] = e->outL + e->outR; }
			// the sung tone's own frequency (at STRETCH 0 an octave partner can grow louder, so look within +-3 semitones)
			auto fnear = [&](double a, double f0) {
				long s0 = (long)(a * 96000); int nfft = 65536; std::vector<std::complex<double>> X(nfft);
				for (int i = 0; i < nfft; i++) X[i] = m[s0 + i] * (0.5 - 0.5 * std::cos(2 * PI * i / nfft));
				fft(X); int lo = (int)(f0 * 0.84 * nfft / 96000), hi = (int)(f0 * 1.19 * nfft / 96000), bi = lo; double bm = 0;
				for (int i = lo; i <= hi; i++) if (std::norm(X[i]) > bm) { bm = std::norm(X[i]); bi = i; }
				double l = std::log(std::norm(X[bi - 1]) + 1e-30), cc = std::log(std::norm(X[bi]) + 1e-30), r = std::log(std::norm(X[bi + 1]) + 1e-30);
				return (bi + 0.5 * (l - r) / (l - 2 * cc + r)) * 96000.0 / nfft; };
			double f8 = peakFreq(m, 8 * 96000, 65536, 96000);
			auto zc = [&](double a) { return fnear(a, f8); };
			double c = 1200 * std::log2(zc(29) / zc(8));
			det += fmt("STRETCH %.0f: %+.0f cents; ", st[i], c);
			if (c < lo[i] || c > hi[i]) ok = false;
			delete e;
		}
		report(ok, "7 STRETCH pitch rise", det);
	}

	// ---------------------------------------------------------- 8. pump
	{
		auto pumpRun = [&](double rate, double& swing, int& bursts) {
			Engine* e = new Engine(); e->setRate(96000); e->build(SHEET, 0); e->reset(); e->f1 = 36.7;
			long n = 14 * 96000; std::vector<double> lv;
			double acc = 0; int k = 0;
			for (long s = 0; s < n; s++) {
				if (s % BLK == 0) { baseS(e->c); e->c.nu = 4e-5; e->c.focus = 0.3; e->c.fc = 147; e->c.drive = 16000; e->c.bright = 0.4; e->c.amp = 0.3; e->c.pump = std::min(1.0, s / 96000.0 / 6); e->c.rate = rate; }
				e->process(0); double o = e->outL + e->outR; acc += o * o;
				if (++k == 480) { lv.push_back(10 * std::log10(acc / k + 1e-20)); acc = 0; k = 0; }   // 5 ms
			}
			std::vector<double> tail(lv.end() - 800, lv.end());   // last 4 s
			double mx = *std::max_element(tail.begin(), tail.end()), mn = *std::min_element(tail.begin(), tail.end());
			swing = mx - mn; bursts = 0; bool up = false;
			for (double x : tail) { if (!up && x > mx - 6) { up = true; bursts++; } else if (up && x < mx - 12) up = false; }
			delete e;
		};
		double s1, s9; int b1, b9; pumpRun(0.1, s1, b1); pumpRun(0.9, s9, b9);
		report(s9 > 12 && s1 > 12 && b9 >= 4 * std::max(b1, 1), "8 pump", fmt("swing %.0f / %.0f dB, bursts in 4 s: %d at RATE 0.1, %d at RATE 0.9", s1, s9, b1, b9));
	}

	// ---------------------------------------------------------- 9. rattle keeps singing
	{
		double eRat[2], hf[2];
		for (int i = 0; i < 2; i++) {
			Engine* e = new Engine(); e->setRate(96000); e->build(SHEET, 0); e->reset(); e->f1 = 49;
			long n = 8 * 96000; std::vector<double> m(n); double es = 0; long en = 0;
			for (long s = 0; s < n; s++) { if (s % BLK == 0) { baseS(e->c); e->c.nu = 4e-5; e->c.focus = 0.6; e->c.fc = 196; e->c.drive = 2600; e->c.bright = 1; e->c.bleed = 0.6; e->c.ratDamp = 2; e->c.rat = i ? 0.78 * 0.95 * segS(s / 96000.0, 2, 3) : 0; /* RATTLE knob 0.95 */ } e->process(0); m[s] = e->outL + e->outR; if (s > 4 * 96000) { es += e->energy; en++; } }
			auto P = spectrum(m, 4 * 96000, 4 * 96000); double h = 0; for (size_t b = 0; b < P.size(); b++) { double f = b * 96000.0 / 8192; if (f >= 3000 && f <= 10000) h += P[b]; }
			eRat[i] = es / en; hf[i] = 10 * std::log10(h + 1e-30); delete e;
		}
		report(eRat[1] > 0.2 * eRat[0] && hf[1] - hf[0] >= 10, "9 rattle keeps singing", fmt("energy %.0f%% of no rattle, 3-10 kHz %+.1f dB", 100 * eRat[1] / eRat[0], hf[1] - hf[0]));
	}

	// ---------------------------------------------------------- 10. fuzz (and 11: other sample rates)
	auto randomKnobs = [](Rng& g) {
		Knobs k; k.type = (int)(g.uni() * 4) % 4; k.cast = (int)(g.uni() * 16) % 16; k.size = 20 * std::pow(10.0, g.uni());
		k.thick = g.uni(); k.curve = g.uni(); k.stretch = 2 * g.uni(); k.decay = 0.15 + 3.85 * g.uni(); k.drive = g.uni();
		k.gain = 20 * std::pow(30.0, g.uni()); k.focus = g.uni(); k.pitch = -5 + 10 * g.uni(); k.trackPlate = g.uni() < 0.5;
		k.phase = g.uni(); k.driverPos = 0.05 + 0.9 * g.uni(); k.spread = 0.5 * g.uni(); k.grab = g.uni() < 0.3 ? g.uni() : 0; k.release = g.uni();
		k.gate = g.uni() < 0.8 ? 1 : 0; k.pick = g.uni(); k.amp = g.uni(); k.drv = g.uni(); k.rat = g.uni(); k.bleed = g.uni(); k.out = g.uni();
		k.pump = g.uni(); k.rate = g.uni(); k.mallet = g.uni(); k.inLevel = g.uni(); k.bright = g.uni(); k.age = g.uni();
		return k;
	};
	auto fuzz = [&](double host, int patches, double secs, const char* name) {
		std::atomic<int> next(0), bad(0); std::atomic<long> guards(0); std::mutex mu; std::string det;
		auto worker = [&]() {
			for (;;) {
				int p = next++; if (p >= patches) break;
				Rng g; g.seed(1000 + p * 7919ULL);
				Chain ch(host, 77 + p); Knobs k = randomKnobs(g); k.type = p % 4;
				long n = (long)(secs * host), stepN = (long)(0.5 * host); double peak = 0; bool nanSeen = false; double inBurst = 0;
				for (long i = 0; i < n; i++) {
					if (i % stepN == 0 && i) { Knobs k2 = randomKnobs(g); k2.type = k.type; k2.cast = k.cast; if (g.uni() < 0.5) k = k2; else { k.drive = k2.drive; k.pitch = k2.pitch; k.focus = k2.focus; k.rat = k2.rat; } }
					double sv = (i % (long)(0.37 * host) == 0) ? g.uni() : 0;
					if (i % (long)(1.3 * host) == 0) inBurst = 0.2 * host;
					double inV = 0; if (inBurst > 0) { inBurst--; inV = 10 * g.bi(); }
					double L, R; ch.step(k, inV, sv, L, R);
					if (!std::isfinite(L) || !std::isfinite(R)) nanSeen = true;
					peak = std::max({peak, std::fabs(L), std::fabs(R)});
				}
				guards += ch.e->guards;
				if (nanSeen || peak > 11.0 || ch.e->guards) { bad++; std::lock_guard<std::mutex> l(mu); if (det.size() < 300) det += fmt("patch %d type %d: %s peak %.1f guards %ld; ", p, k.type, nanSeen ? "NaN" : "", peak, ch.e->guards); }
			}
		};
		std::vector<std::thread> th; int nt = std::max(1u, std::thread::hardware_concurrency());
		for (int i = 0; i < nt; i++) th.emplace_back(worker); for (auto& t : th) t.join();
		report(bad == 0, name, fmt("%d patches x %.0f s at %.1f kHz, %d bad, guard events %ld. %s", patches, secs, host / 1000, (int)bad, (long)guards, det.c_str()));
	};
	fuzz(48000, 400, 20, "10 fuzz (48k)");

	regimes(88200, "(88.2k engine = 44.1k host)");
	tuning(88200, "(88.2k engine = 44.1k host)");
	fuzz(44100, 100, 10, "11 fuzz (44.1k)");
	fuzz(96000, 100, 10, "11 fuzz (96k)");

	// ---------------------------------------------------------- 12. silence
	{
		bool ok = true; std::string det;
		for (double a : {0.0, 1.0}) {
			Chain ch(48000, 5); Knobs k; k.age = a; double worst = 0;
			for (long i = 0; i < 62 * 48000L; i++) { k.drive = i < 2 * 48000 ? 0.6 : 0.0; double L, R; ch.step(k, 0, 0, L, R); if (i > 60 * 48000L) worst = std::max({worst, std::fabs(L), std::fabs(R)}); }
			det += fmt("AGE %.0f: max %.3g V after 60 s; ", a, worst); if (worst != 0) ok = false;
		}
		report(ok, "12 silence", det);
	}

	// ---------------------------------------------------------- 13. speed
	{
		Chain ch(48000, 3); Knobs k; k.drive = 0.9; long n = 20 * 48000;
		auto t0 = std::chrono::steady_clock::now();
		double L, R; for (long i = 0; i < n; i++) ch.step(k, 0, 0, L, R);
		double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
		report(true, "13 speed", fmt("roar at 48 kHz host: %.1f%% of one core on this machine", 100 * secs / 20.0));
	}
	presetTest(out);
	std::printf("\n%d passed, %d failed\n", nPass, nFail);
	return nFail ? 1 : 0;
}
