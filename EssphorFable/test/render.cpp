// Offline checks and demo renders for Essphor, Essphor Res and Essphor Mod. No Rack needed:  make test
//   ./test/render OUTDIR            run the checks (exit code = number of failures)
//   ./test/render OUTDIR --demos    also render the demo WAVs into OUTDIR
// Check numbers follow section 9 of docs/BUILD-INSTRUCTIONS.md. Expected values come from the
// Python references in tools/proto (check_proto.txt, check_res.txt, check_feedback.txt, check_age.txt).
#include "../src/dsp/Grains.hpp"
#include "../src/dsp/Res.hpp"
#include "../src/dsp/Mod.hpp"
#include "../src/dsp/Wav.hpp"
#include <cstdio>
#include <string>
#include <vector>
#include <complex>
#include <functional>
#include <chrono>

#include "noage.hpp"
using namespace essphor;
typedef std::complex<double> Cx;

static int failures = 0;
static void check(bool ok, const char* what, const std::string& detail) {
	printf("%s  %-62s %s\n", ok ? "PASS" : "FAIL", what, detail.c_str());
	fflush(stdout);
	if (!ok) failures++;
}
template <typename... A> static std::string fmt(const char* f, A... a) { char b[2048]; snprintf(b, sizeof b, f, a...); return b; }

// ---------------------------------------------------------------- helpers
static void fft(std::vector<Cx>& a) {
	const size_t n = a.size();
	for (size_t i = 1, j = 0; i < n; i++) { size_t bit = n >> 1; for (; j & bit; bit >>= 1) j ^= bit; j ^= bit; if (i < j) std::swap(a[i], a[j]); }
	for (size_t len = 2; len <= n; len <<= 1) {
		Cx wl = std::polar(1.0, -2 * PI / len);
		for (size_t i = 0; i < n; i += len) { Cx w(1); for (size_t j = 0; j < len / 2; j++) { Cx u = a[i + j], v = a[i + j + len / 2] * w; a[i + j] = u + v; a[i + j + len / 2] = u - v; w *= wl; } }
	}
}
static size_t pow2(size_t n) { size_t m = 1; while (m < n) m <<= 1; return m; }
// magnitude spectrum of x (zero-padded to a power of two), optionally Hann-windowed; returns |X| and bin width
static Vec spectrum(const Vec& x, bool hann, double fs, double& df, size_t nfft = 0) {
	size_t n = nfft ? nfft : pow2(x.size());
	std::vector<Cx> a(n, 0.0);
	for (size_t i = 0; i < x.size() && i < n; i++) a[i] = x[i] * (hann ? 0.5 - 0.5 * std::cos(2 * PI * i / (x.size() - 1)) : 1.0);
	fft(a);
	Vec m(n / 2 + 1); for (size_t i = 0; i <= n / 2; i++) m[i] = std::abs(a[i]);
	df = fs / n; return m;
}
static double peakFreq(const Vec& x, double fs, double lo, double hi) {
	double df; Vec m = spectrum(x, true, fs, df, pow2(x.size()) * 4);
	size_t best = 0; double bv = -1;
	for (size_t i = (size_t)(lo / df); i < m.size() && i * df < hi; i++) if (m[i] > bv) { bv = m[i]; best = i; }
	if (best > 0 && best + 1 < m.size()) {   // parabolic interpolation on log magnitude
		double a = std::log(m[best - 1] + 1e-30), b = std::log(m[best] + 1e-30), c = std::log(m[best + 1] + 1e-30);
		double d = 0.5 * (a - c) / (a - 2 * b + c);
		return (best + d) * df;
	}
	return best * df;
}
static double rmsDb(const Vec& x, size_t a, size_t b) { double s = 0; b = std::min(b, x.size()); for (size_t i = a; i < b; i++) s += x[i] * x[i]; return 10 * std::log10(s / std::max<size_t>(1, b - a) + 1e-24); }
static double peakDb(const Vec& x) { double m = 0; for (double v : x) m = std::max(m, std::fabs(v)); return 20 * std::log10(m + 1e-12); }

double rmsLin(const Vec& x) { double s = 0; for (double v : x) s += v * v; return s / std::max<size_t>(1, x.size()); }

// Run the grain engine over a stereo input with a callback per sample for automation.
static Stereo runGrains(GrainEngine& e, const Stereo& in, double fs, double bpm, std::function<void(GrainEngine&, size_t)> each = nullptr,
                        std::vector<size_t>* fires = nullptr, int* maxActive = nullptr) {
	Clock clk; clk.reset();
	Stereo o; o.l.resize(in.l.size()); o.r.resize(in.l.size());
	for (size_t i = 0; i < in.l.size(); i++) {
		if (each) each(e, i);
		clk.tick(1.0 / fs, false, false, i == 0, bpm);
		double a, b; e.process(in.l[i], in.r[i], clk, a, b);
		o.l[i] = a; o.r[i] = b;
		if (fires && e.fired) fires->push_back(i);
		if (maxActive) *maxActive = std::max(*maxActive, e.active);
	}
	return o;
}
static Stereo mono2(const Vec& x) { return Stereo{x, x}; }
static double rateKnob(double hz) { return std::log(hz / 0.25) / std::log(1000.0); }
static double sizeKnob(double sec, double bpm) { return std::log(sec * bpm / 60.0 * 64.0) / std::log(512.0); }
static double delayKnob(double sec, double bufSec = 4.0) { return std::sqrt(sec / (bufSec - 0.05)); }
static GrainParams freeParams(double rateHz, double sizeSec, double bpm = 120) {
	GrainParams p; p.sync = false; p.rateKnob = rateKnob(rateHz); p.sizeKnob = sizeKnob(sizeSec, bpm); p.wet = 1.0; p.scale = SCALE_FREE;
	return p;
}

// ---------------------------------------------------------------- grain checks
static void grainChecks(double fs) {
	const double bpm = 120;
	// 1. pitch
	{
		size_t n = (size_t)(6 * fs); Vec x(n); for (size_t i = 0; i < n; i++) x[i] = 0.25 * std::sin(2 * PI * 440 * i / fs);
		const double sts[6] = {-12, -7, 0, 7, 12, 24};
		double worst = 0; std::string det;
		for (double st : sts) {
			GrainEngine e; e.p = freeParams(16, 0.25); e.p.scanMode = SCAN_DELAY_FREE; e.p.scan = delayKnob(0.25); e.p.pitch = st;
			e.seed(1); e.setPatternSeed(1); e.init(fs, 4);
			Stereo y = runGrains(e, mono2(x), fs, bpm);
			Vec seg(y.l.begin() + (size_t)(2 * fs), y.l.begin() + (size_t)(5 * fs));
			double want = 440 * std::pow(2.0, st / 12), got = peakFreq(seg, fs, want * 0.8, want * 1.2);
			double err = std::fabs(got / want - 1);
			if (err > worst) worst = err;
			det += fmt("%+.0f:%.1f ", st, got);
		}
		check(worst < 0.02, fmt("1. pitch, six shifts, %.0f Hz", fs).c_str(), fmt("worst %.2f%%  %s", worst * 100, det.c_str()));
	}
	// 2. delay
	{
		size_t n = (size_t)(6 * fs); Vec x(n); Rng r; r.seed(3);
		for (size_t i = 0; i < n; i++) { double t = i / fs; x[i] = std::fmod(t, 0.5) < 0.05 ? r.normal() * 0.1 : 0.0; }
		const double ds[4] = {0.125, 0.25, 0.5, 1.0};
		double worst = 0; std::string det;
		for (double d : ds) {
			GrainEngine e; e.p = freeParams(32, 0.1); e.p.scanMode = SCAN_DELAY_FREE; e.p.scan = delayKnob(d);
			e.seed(1); e.setPatternSeed(1); e.init(fs, 4);
			Stereo y = runGrains(e, mono2(x), fs, bpm);
			size_t a = (size_t)fs, b = (size_t)(5 * fs), N = pow2(2 * (b - a));
			std::vector<Cx> A(N, 0.0), B(N, 0.0);
			for (size_t i = a; i < b; i++) { A[i - a] = x[i]; B[i - a] = y.l[i]; }
			fft(A); fft(B);
			for (size_t i = 0; i < N; i++) B[i] *= std::conj(A[i]);
			for (auto& v : B) v = std::conj(v);
			fft(B);   // inverse via conjugation (scale irrelevant)
			size_t best = 0; double bv = -1e300;
			for (size_t i = 0; i < (size_t)(1.2 * fs); i++) if (B[i].real() > bv) { bv = B[i].real(); best = i; }
			double lag = best / fs; worst = std::max(worst, std::fabs(lag - d));
			det += fmt("%.4f ", lag);
		}
		check(worst <= 0.001, fmt("2. delay time, 0.125 to 1 s, %.0f Hz", fs).c_str(), fmt("worst %.2f ms  measured %s", worst * 1000, det.c_str()));
	}
	// 3. stretch
	{
		size_t n = (size_t)(6 * fs); Vec x(n);
		for (size_t i = 0; i < n; i++) { double t = i / fs; x[i] = t > 4 ? 0 : 0.25 * std::sin(2 * PI * (200 * t + 0.5 * 450 * t * t)); }
		GrainEngine e; e.p = freeParams(40, 0.08); e.p.scanMode = SCAN_STRETCH; e.p.scan = (0.5 + 2) / 4;
		e.seed(1); e.setPatternSeed(1); e.init(fs, 4);
		Stereo y = runGrains(e, mono2(x), fs, bpm);
		double worst = 0; std::string det;
		for (double t : {2.0, 3.0, 4.0, 5.0}) {
			Vec seg(y.l.begin() + (size_t)((t - 0.1) * fs), y.l.begin() + (size_t)((t + 0.1) * fs));
			double want = 200 + 450 * t / 2, got = peakFreq(seg, fs, want * 0.7, want * 1.3);
			worst = std::max(worst, std::fabs(got / want - 1)); det += fmt("%.0f/%.0f ", got, want);
		}
		check(worst < 0.03, fmt("3. stretch at half speed, %.0f Hz", fs).c_str(), fmt("worst %.2f%%  %s", worst * 100, det.c_str()));
	}
	// 4. level
	{
		size_t n = (size_t)(6 * fs); Rng r; r.seed(3);
		Vec a(n), b(n); for (size_t i = 0; i < n; i++) { a[i] = r.normal(); b[i] = r.normal(); }
		Stereo x; x.l.resize(n); x.r.resize(n);
		for (size_t i = 0; i < n; i++) { x.l[i] = a[i] + (i ? a[i - 1] : 0) + (i > 1 ? a[i - 2] : 0); x.r[i] = b[i] + (i ? b[i - 1] : 0) + (i > 1 ? b[i - 2] : 0); }
		double s = std::sqrt(0.5 * (rmsLin(x.l) + rmsLin(x.r)));
		for (size_t i = 0; i < n; i++) { x.l[i] *= 0.1 / s; x.r[i] *= 0.1 / s; }
		double lo = 1e9, hi = -1e9; std::string det;
		struct { double rate, size; } cs[4] = {{4, 0.25}, {16, 0.25}, {64, 0.25}, {64, 1.0}};
		for (auto c : cs) for (double sp : {0.0, 0.5}) {
			GrainEngine e; e.p = freeParams(c.rate, c.size); e.p.scanMode = SCAN_DELAY_FREE; e.p.scan = delayKnob(0.5); e.p.spray = sp; e.p.sprayMode = SPRAY_RANDOM_MONO;
			e.seed(1); e.setPatternSeed(1); e.init(fs, 4);
			Stereo y = runGrains(e, x, fs, bpm);
			double v = rmsDb(y.l, (size_t)(2 * fs), n); lo = std::min(lo, v); hi = std::max(hi, v); det += fmt("%.1f ", v);
		}
		check(hi - lo <= 3.0, fmt("4. level across overlap 1 to 64, %.0f Hz", fs).c_str(), fmt("spread %.1f dB  %s", hi - lo, det.c_str()));
	}
	// 5. feedback. The reference feeds back one 32-sample block late, which moves the comb phase of coherent grains,
	// so absolute levels are compared for information only. Judged: FEEDBACK 1 holds (11-12 s no more than 6 dB under 5-6 s and
	// above -70 dB), FEEDBACK 0.5 dies out (below -60 dB) wherever the reference dies out, and the peak never passes 0 dB.
	{
		const double ref[15] = {-239.9, -107.2, -27.2, -199.8, -46.3, -14.1, -30.0, -11.4, -9.1, -114.4, -42.3, -13.7, -240.0, -48.1, -13.2};
		struct C { double rate, size, scan, spray, pitch; } cs[5] = {{16, 0.25, 0.25, 0, 0}, {16, 0.25, 0.25, 0.2, 0}, {4, 0.25, 0.5, 0, 0}, {40, 0.4, 0.5, 0.3, 0}, {16, 0.25, 0.25, 0, 12}};
		size_t n = (size_t)(12 * fs); Vec x(n, 0.0);
		for (size_t i = 0; i < (size_t)fs; i++) x[i] = 0.25 * std::sin(2 * PI * 330 * i / fs) * (0.5 - 0.5 * std::cos(2 * PI * i / (fs - 1)));
		bool ok = true; double pk = -300; std::string det; int k = 0;
		for (auto c : cs) for (double F : {0.5, 0.8, 1.0}) {
			GrainEngine e; e.p = freeParams(c.rate, c.size); e.p.scanMode = SCAN_DELAY_FREE; e.p.scan = delayKnob(c.scan); e.p.spray = c.spray; e.p.pitch = c.pitch; e.p.feedback = F;
			e.seed(1); e.setPatternSeed(1); e.init(fs, 4);
			Stereo y = runGrains(e, mono2(x), fs, bpm);
			double v = rmsDb(y.l, (size_t)(11 * fs), n), mid = rmsDb(y.l, (size_t)(5 * fs), (size_t)(6 * fs)); pk = std::max(pk, peakDb(y.l));
			double R = ref[k++];
			bool good = true;
			if (F == 1.0) good = v >= mid - 6.0 && v > -70;   // holds or swells (the limiter caps it)
			if (F == 0.5 && R < -60) good = v < -60;
			if (!good) ok = false;
			det += fmt("%.0f/%.0f%s ", std::max(v, -199.0), R, good ? "" : "!");
		}
		check(ok && pk <= 0.0, fmt("5. grain feedback, 15 cases, %.0f Hz", fs).c_str(), fmt("peak %.1f dB  11-12 s got/ref: %s", pk, det.c_str()));
	}
	// 6. guard: +36 semitones and reversed grains next to the write head
	{
		size_t n = (size_t)(20 * fs); Vec x(n); for (size_t i = 0; i < n; i++) x[i] = 0.25 * std::sin(2 * PI * 50 * i / fs);
		double worst = 0;
		for (int mode = 0; mode < 3; mode++) {
			GrainEngine e; e.p = freeParams(8, 0.5); e.p.pitch = 36;
			if (mode == 0) { e.p.scanMode = SCAN_DELAY_FREE; e.p.scan = delayKnob(0.01); }
			if (mode == 1) { e.p.scanMode = SCAN_POSITION; e.p.scan = 0.3; }
			if (mode == 2) { e.p.scanMode = SCAN_DELAY_FREE; e.p.scan = delayKnob(0.01); e.p.randRev = 1.0; e.p.pitch = 12; }
			e.seed(1); e.setPatternSeed(1); e.init(fs, 4);
			Stereo y = runGrains(e, mono2(x), fs, bpm);
			double amp = 0; for (size_t i = (size_t)(5 * fs); i < n; i++) amp = std::max(amp, std::fabs(y.l[i]));
			double d2 = 0; for (size_t i = (size_t)(5 * fs); i + 1 < n; i++) d2 = std::max(d2, std::fabs(y.l[i + 1] - 2 * y.l[i] + y.l[i - 1]));
			worst = std::max(worst, d2 / std::max(1e-9, amp));
		}
		check(worst < 0.05, fmt("6. no clicks at the write head (+36, position, reverse), %.0f Hz", fs).c_str(), fmt("worst second difference %.4f of peak", worst));
	}
	// 7. lock: scan across the frozen join
	{
		size_t n = (size_t)(14 * fs); Vec x(n); for (size_t i = 0; i < n; i++) x[i] = 0.25 * std::sin(2 * PI * 50 * i / fs);
		GrainEngine e; e.p = freeParams(20, 0.1); e.p.scanMode = SCAN_POSITION;
		e.seed(1); e.setPatternSeed(1); e.init(fs, 4);
		Stereo y = runGrains(e, mono2(x), fs, bpm, [&](GrainEngine& g, size_t i) {
			double t = i / fs; g.p.lock = t > 4.37; g.p.scan = std::max(0.0, std::min(1.0, (t - 4.5) / 9.0));
		});
		double amp = 0, d2 = 0;
		for (size_t i = (size_t)(4.6 * fs); i + 1 < n; i++) { amp = std::max(amp, std::fabs(y.l[i])); d2 = std::max(d2, std::fabs(y.l[i + 1] - 2 * y.l[i] + y.l[i - 1])); }
		check(d2 / amp < 0.05, fmt("7. lock, position scan across the join, %.0f Hz", fs).c_str(), fmt("worst second difference %.4f of peak", d2 / amp));
	}
	// 8. grid
	{
		size_t n = (size_t)(60 * fs);
		auto run = [&](double rr, std::vector<size_t>& fires) {
			GrainEngine e; GrainParams p; p.sync = true; p.rateKnob = 0.75; p.sizeKnob = 0.3; p.randRate = rr; p.scanMode = SCAN_DELAY_FREE; p.scan = 0.3; e.p = p;
			e.seed(1); e.setPatternSeed(1); e.init(fs, 4);
			Clock clk; clk.ppqn = 1;
			const double per = fs * 0.5;   // 120 BPM, one pulse a beat
			long long nextPulse = 0; int k = 0;
			for (size_t i = 0; i < n; i++) {
				bool edge = (long long)i == nextPulse;
				if (edge) { k++; nextPulse = (long long)std::llround(k * per); }
				clk.tick(1.0 / fs, true, edge, i == 0, 120);
				double a, b; e.process(0, 0, clk, a, b);
				if (e.fired) fires.push_back(i);
			}
		};
		std::vector<size_t> f1, f0; run(1.0, f1); run(0.0, f0);
		double worst = 0; for (size_t k = 0; k < f0.size(); k++) worst = std::max(worst, std::fabs(f0[k] - k * fs * 0.125));
		check(std::abs((int)f1.size() - 480) <= 1 && std::abs((int)f0.size() - 480) <= 1 && worst <= 1.0,
		      fmt("8. clock grid, 1/16 at 120 BPM for 60 s, %.0f Hz", fs).c_str(),
		      fmt("%d grains with RAND RATE 1, %d with 0, worst offset %.2f samples", (int)f1.size(), (int)f0.size(), worst));
	}
}


// AGE exactness and scale (9, 9b) and voices (10)
static void ageChecks(double fs) {
	size_t n = (size_t)(5 * fs); Rng r; r.seed(9);
	Stereo x; x.l.resize(n); x.r.resize(n); for (size_t i = 0; i < n; i++) { x.l[i] = 0.2 * r.normal(); x.r[i] = 0.2 * r.normal(); }
	{
		GrainEngine e; e.p = freeParams(16, 0.25); e.p.scanMode = SCAN_DELAY_FREE; e.p.scan = delayKnob(0.3); e.p.spray = 0.2; e.p.feedback = 0.5; e.p.randRev = 0.3; e.p.randPitch = 0.4;
		e.seed(5); e.setPatternSeed(5); e.init(fs, 4);
		Stereo y = runGrains(e, x, fs, 120);
		Stereo z = noage::runGrains(x, fs, &e.p, sizeof e.p, 5);
		double d = 0; for (size_t i = 0; i < n; i++) d = std::max(d, std::fabs(y.l[i] - z.l[i]) + std::fabs(y.r[i] - z.r[i]));
		ResEngine re; re.p.scale = 10; re.p.decay = 0.6; re.p.cutoff = 3000; re.init(fs);
		Vec ry(n); for (size_t i = 0; i < n; i++) { double a, b; re.process(x.l[i], x.r[i], a, b); ry[i] = a + b; }
		Vec rz = noage::runRes(x, fs, &re.p, sizeof re.p);
		double dr = 0; for (size_t i = 0; i < n; i++) dr = std::max(dr, std::fabs(ry[i] - rz[i]));
		check(d == 0.0, fmt("9. AGE 0 is exact on Essphor, %.0f Hz", fs).c_str(), fmt("largest difference %.3g", d));
		check(dr == 0.0, fmt("16a. AGE 0 is exact on Essphor Res, %.0f Hz", fs).c_str(), fmt("largest difference %.3g", dr));
	}
	{
		const double ref[4] = {-86.8, -77.7, -68.6, -60.0};
		size_t m = (size_t)(4 * fs); Stereo sil{Vec(m, 0.0), Vec(m, 0.0)};
		Rng q; q.seed(2); Stereo nz{Vec(m), Vec(m)}; for (size_t i = 0; i < m; i++) { nz.l[i] = 0.1 * q.normal(); nz.r[i] = 0.1 * q.normal(); }
		bool ok = true; std::string det; double prevHf = 1e9; bool falls = true;
		for (int k = 0; k <= 4; k++) {
			double age = 0.25 * k;
			GrainEngine e; e.p = freeParams(16, 0.25); e.p.scanMode = SCAN_DELAY_FREE; e.p.scan = delayKnob(0.25); e.p.age = age;
			e.seed(1); e.setPatternSeed(1); e.init(fs, 4);
			Stereo h = runGrains(e, sil, fs, 120);
			double lv = rmsDb(h.l, (size_t)fs, m);
			GrainEngine e2; e2.p = e.p; e2.seed(1); e2.setPatternSeed(1); e2.init(fs, 4);
			Stereo y = runGrains(e2, nz, fs, 120);
			Vec seg(y.l.begin() + (size_t)fs, y.l.end()); double df; Vec sp = spectrum(seg, false, fs, df);
			double hf = 0, lf = 0; int nh = 0, nl = 0;
			for (size_t i = 0; i < sp.size(); i++) { double f = i * df; if (f > 8000 && f < 12000) { hf += sp[i] * sp[i]; nh++; } if (f > 200 && f < 2000) { lf += sp[i] * sp[i]; nl++; } }
			double rel = 10 * std::log10((hf / nh) / (lf / nl));
			if (k > 0) { bool g = std::fabs(lv - ref[k - 1]) <= 3.0; ok = ok && g; det += fmt("%.1f/%.1f ", lv, ref[k - 1]); }
			if (k > 0 && rel > prevHf + 0.5) falls = false;
			prevHf = rel;
		}
		check(ok && falls, fmt("9b. AGE: hiss level and treble loss, %.0f Hz", fs).c_str(), fmt("hiss got/ref %s, treble falls %s", det.c_str(), falls ? "yes" : "NO"));
	}
	{
		size_t m = (size_t)(8 * fs); Rng q; q.seed(4); Stereo nz{Vec(m), Vec(m)}; for (size_t i = 0; i < m; i++) { nz.l[i] = 0.5 * q.normal(); nz.r[i] = 0.5 * q.normal(); }
		GrainEngine e; e.p = freeParams(250, 2.0); e.p.scanMode = SCAN_DELAY_FREE; e.p.scan = delayKnob(0.5); e.p.feedback = 1.0; e.p.spray = 1.0;
		e.seed(1); e.setPatternSeed(1); e.init(fs, 4);
		int maxA = 0; Stereo y = runGrains(e, nz, fs, 120, nullptr, nullptr, &maxA);
		bool fin = true; double pk = 0; for (size_t i = 0; i < m; i++) { if (!std::isfinite(y.l[i]) || !std::isfinite(y.r[i])) fin = false; pk = std::max(pk, std::max(std::fabs(y.l[i]), std::fabs(y.r[i]))); }
		check(maxA <= MAX_GRAINS && fin && pk < 2.4, fmt("10. voices: 250 grains/s of 2 s, %.0f Hz", fs).c_str(), fmt("max active %d, finite %s, peak %.2f (ceiling 2.4)", maxA, fin ? "yes" : "NO", pk));
	}
}

// ---------------------------------------------------------------- resonator checks
static Vec resImpulse(double fs, size_t n, std::function<void(ResParams&)> set, int chans = 1) {
	ResEngine e; set(e.p); e.init(fs);
	Vec y(n); for (size_t i = 0; i < n; i++) { double a, b; e.process(i == 0 ? 1.0 : 0.0, i == 0 ? 1.0 : 0.0, a, b); y[i] = chans == 1 ? a : b; }
	return y;
}
static void resChecks(double fs) {
	// 11. flatness
	{
		Vec y = resImpulse(fs, 1 << 15, [](ResParams& p) {});
		double df; Vec m = spectrum(y, false, fs, df, 1 << 15);
		struct { double a, b, mean, mn, mx; } R[3] = {{20, 60, -1.0, -4.4, 2.5}, {60, 6000, -1.4, -4.0, 0.9}, {6000, 18000, -2.5, -4.0, 2.4}};
		bool ok = true; std::string det;
		for (auto r : R) {
			double s = 0, mn = 1e9, mx = -1e9; int k = 0;
			for (size_t i = 0; i < m.size(); i++) { double f = i * df; if (f > r.a && f < r.b) { double v = 20 * std::log10(m[i] + 1e-12); s += v; k++; mn = std::min(mn, v); mx = std::max(mx, v); } }
			double mean = s / k;
			bool g = std::fabs(mean - r.mean) <= 1 && std::fabs(mn - r.mn) <= 1 && std::fabs(mx - r.mx) <= 1;
			ok = ok && g;
			det += fmt("[%.0f-%.0f: %.1f %.1f %+.1f]%s ", r.a, r.b, mean, mn, mx, g ? "" : "!");
		}
		check(ok || fs != 44100, fmt("11. bank flatness, filter open, %.0f Hz%s", fs, fs != 44100 ? " (info)" : "").c_str(), det);
	}
	// 12. band counts
	{
		struct { int sc, n; double lo, hi; } R[7] = {{0, 48, 32.7, 7902}, {1, 48, 32.7, 7902}, {10, 41, 32.7, 8372}, {14, 45, 32.7, 8372}, {16, 32, 32.7, 7902}, {17, 17, 32.7, 8372}, {18, 9, 32.7, 8372}};
		bool ok = true; std::string det;
		for (auto r : R) {
			double nt[NB]; int c = bandNotes(r.sc, nt);
			double lo = C1 * std::pow(2.0, nt[0] / 12), hi = C1 * std::pow(2.0, nt[c - 1] / 12);
			bool g = c == r.n && std::fabs(lo - r.lo) < 0.5 && std::fabs(hi - r.hi) < 1.0;
			ok = ok && g; det += fmt("%s:%d ", SCALE_NAMES[r.sc], c);
		}
		check(ok, "12. band counts and range for seven scales", det);
	}
	// 13. filter curve
	{
		const double F[7] = {125, 250, 500, 1000, 2000, 4000, 8000};
		struct { double slope, res; double v[7]; } R[4] = {
			{-1, 0, {-2.8, -1.4, -3.2, -4.5, -15.2, -25.7, -38.4}}, {0, 0, {-17.9, -10.4, -6.2, -1.4, -6.2, -10.5, -16.6}},
			{1, 0, {-38.7, -25.5, -15.1, -4.4, -3.1, -1.4, -0.7}}, {-1, 0.7, {-2.6, -1.0, -0.2, 12.0, -12.9, -23.8, -37.4}}};
		double worst = 0; std::string det;
		for (auto r : R) {
			Vec y = resImpulse(fs, 1 << 15, [&](ResParams& p) { p.cutoff = 1000; p.slope = r.slope; p.res = r.res; });
			double df; Vec m = spectrum(y, false, fs, df, 1 << 15);
			for (int i = 0; i < 7; i++) {
				double s = 0; int k = 0;
				for (size_t b = 0; b < m.size(); b++) { double f = b * df; if (f > F[i] / 1.12 && f < F[i] * 1.12) { s += 20 * std::log10(m[b] + 1e-12); k++; } }
				double v = s / k; double e = std::fabs(v - r.v[i]);
				if (e > worst) { worst = e; det = fmt("worst at slope %+.0f res %.1f %.0f Hz: %.1f vs %.1f", r.slope, r.res, F[i], v, r.v[i]); }
			}
		}
		check(worst <= 1.5, fmt("13. filter curve LP/BP/HP/resonant, %.0f Hz", fs).c_str(), fmt("worst %.2f dB (%s)", worst, det.c_str()));
	}
	// 14. ring time and tuning
	{
		bool ok = true; std::string det;
		for (double dec : {0.25, 0.5, 0.75, 1.0}) {
			double design = 0.05 * std::pow(400.0, dec);
			size_t n = (size_t)(fs * std::min(30.0, design * 1.6 + 0.5));
			Vec y = resImpulse(fs, n, [&](ResParams& p) { p.scale = 18; p.decay = dec; p.cutoff = 262; p.slope = 0; p.res = 0.6; });
			double pk = peakFreq(y, fs, 200, 330);
			Vec env(n); int W = (int)(0.01 * fs) | 1; double acc = 0;
			std::vector<double> sq(n); for (size_t i = 0; i < n; i++) sq[i] = y[i] * y[i];
			for (size_t i = 0; i < n; i++) { acc += sq[i]; if (i >= (size_t)W) acc -= sq[i - W]; env[i >= (size_t)(W / 2) ? i - W / 2 : 0] = std::sqrt(acc / W); }
			size_t i0 = (size_t)(0.02 * fs); double e0 = env[i0], t60 = -1;
			for (size_t i = i0; i < n - W; i++) if (env[i] < e0 * std::pow(10.0, -30.0 / 20)) { t60 = 2.0 * (i - i0) / fs; break; }
			bool g = t60 > 0 && std::fabs(t60 / design - 1) <= 0.10 && std::fabs(pk / 261.63 - 1) <= 0.005;
			ok = ok && g; det += fmt("%.2f:%.2fs@%.1f%s ", dec, t60, pk, g ? "" : "!");
		}
		check(ok, fmt("14. ring time and tuning, DECAY 0.25 to 1, %.0f Hz", fs).c_str(), det);
	}
	// 15. sustain and choke: ringing above 200 Hz from 0.6 to 1.0 s, with CUTOFF left open or closed to 100 Hz at 0.5 s.
	// (Bands below 100 Hz are inside the closed filter and keep ringing in both modes, so they are left out.)
	{
		bool ok = true; std::string det;
		for (int choke = 0; choke < 2; choke++) {
			double lv[2];
			for (int sweep = 0; sweep < 2; sweep++) {
				ResEngine e; e.p.scale = 10; e.p.decay = 0.75; e.p.choke = choke; e.init(fs);
				Rng r; r.seed(1); size_t n = (size_t)(1.5 * fs); Vec y(n);
				for (size_t i = 0; i < n; i++) {
					if (sweep && i == (size_t)(0.5 * fs)) e.p.cutoff = 100;
					double x = i < (size_t)(0.05 * fs) ? 0.3 * r.normal() : 0.0, a, b;
					e.process(x, x, a, b); y[i] = a;
				}
				Vec seg(y.begin() + (size_t)(0.6 * fs), y.begin() + (size_t)(1.0 * fs)); double df; Vec m = spectrum(seg, true, fs, df);
				double s2 = 0; for (size_t i = 0; i < m.size(); i++) if (i * df > 200) s2 += m[i] * m[i];
				lv[sweep] = 10 * std::log10(s2 + 1e-30);
			}
			bool g = choke ? lv[1] <= lv[0] - 20 : std::fabs(lv[1] - lv[0]) <= 3;
			ok = ok && g; det += fmt("%s: swept minus open %+.1f dB%s  ", choke ? "CHOKE" : "SUSTAIN", lv[1] - lv[0], g ? "" : "!");
		}
		check(ok, fmt("15. SUSTAIN keeps ringing, CHOKE cuts it (above 200 Hz), %.0f Hz", fs).c_str(), det);
	}
	// 16b. AGE 1 detunes left and right differently
	{
		ResEngine e; e.p.age = 1.0; e.seeds[0] = 11; e.seeds[1] = 22; e.init(fs);
		double a, b; for (int i = 0; i < 64; i++) e.process(0, 0, a, b);
		int diff = 0; for (int k = 0; k < NB; k++) if (std::fabs(e.ch[0].fCur[k] / e.ch[1].fCur[k] - 1) > 1e-4) diff++;
		check(diff > NB / 2, fmt("16b. AGE 1, two seeds: left and right bands differ, %.0f Hz", fs).c_str(), fmt("%d of %d bands differ", diff, NB));
	}
}

// ---------------------------------------------------------------- modulator checks
static void modChecks(double fs) {
	const double dt = 1.0 / fs;
	// 17. WAVE
	{
		Wave w; w.p.rateMode = RM_FREE; w.p.rate = std::log(1.0 / 0.01) / std::log(10000.0);
		Clock clk; clk.reset();
		size_t n = (size_t)(10 * fs); int up = 0; double mx = -99, mn = 99; size_t first = 0, last = 0; double prev = 0;
		for (size_t i = 0; i < n; i++) {
			clk.tick(dt, false, false, i == 0, 120); w.process(clk, dt);
			if (i && prev < 0 && w.outA >= 0) { if (!up) first = i; last = i; up++; }
			prev = w.outA; mx = std::max(mx, w.outA); mn = std::min(mn, w.outA);
		}
		double f1 = (up - 1) / ((last - first) / fs);
		Wave s; s.p.rateMode = RM_STRAIGHT; s.p.rate = 6.0 / 9.0;   // index 6 of 8/1..1/64 = 1/8 = half a beat
		Clock c2; c2.reset(); up = 0; prev = 0; first = last = 0;
		for (size_t i = 0; i < n; i++) {
			c2.tick(dt, false, false, i == 0, 120); s.process(c2, dt);
			if (i && prev < 0 && s.outA >= 0) { if (!up) first = i; last = i; up++; }
			prev = s.outA;
		}
		double f2 = (up - 1) / ((last - first) / fs);
		Wave q; q.p.rateMode = RM_FREE; q.p.rate = w.p.rate; q.p.spread = 0.5;
		Clock c3; c3.reset(); double phErr = 0;
		for (size_t i = 0; i < (size_t)fs; i++) { c3.tick(dt, false, false, i == 0, 120); q.process(c3, dt); double t = (i + 1) / fs; phErr = std::max(phErr, std::fabs(q.outB - 5 * std::sin(2 * PI * (t + 0.25)))); }
		bool ok = std::fabs(f1 - 1) < 0.001 && std::fabs(f2 - 4) < 0.004 && mx <= 5.0001 && mn >= -5.0001 && phErr < 0.01;
		check(ok, fmt("17. WAVE: rate, sync, range, spread, %.0f Hz", fs).c_str(), fmt("free %.4f Hz, 1/8 at 120 BPM %.4f Hz, range %.2f..%.2f V, spread error %.4f V", f1, f2, mn, mx, phErr));
	}
	// 18. RANDOM
	{
		auto run = [&](double var) {
			RandomSeq r; r.seed(7, 8); r.p.rateMode = RM_FREE; r.p.rate = std::log(10.0 / 0.01) / std::log(10000.0); r.p.length = 8; r.p.variation = var;
			Clock clk; clk.reset(); std::vector<double> v;
			for (size_t i = 0; i < (size_t)(80 * fs); i++) { clk.tick(dt, false, false, i == 0, 120); r.process(clk, dt); if (i % (size_t)(fs / 10) == (size_t)(fs / 20)) v.push_back(r.outA); }
			return v;
		};
		std::vector<double> a = run(0.0), b = run(1.0);
		bool rep = true, norep = false;
		for (size_t i = 8; i < a.size(); i++) if (std::fabs(a[i] - a[i - 8]) > 1e-9) rep = false;
		for (size_t i = 8; i < b.size(); i++) if (std::fabs(b[i] - b[i - 8]) > 1e-6) norep = true;
		check(rep && norep, fmt("18. RANDOM: VARIATION 0 repeats, 1 does not, %.0f Hz", fs).c_str(), fmt("%d steps compared", (int)a.size()));
	}
	// 19. FOLLOW
	{
		Follow f; f.init(fs); f.p.rise = 0; f.p.fall = 0.15;
		Clock clk; clk.reset(); double mx = 0;
		size_t n = (size_t)(1.0 * fs), tStop = (size_t)(0.5 * fs); double atStop = 0, after = 0;
		for (size_t i = 0; i < n; i++) {
			double x = i < tStop ? 5.0 * std::sin(2 * PI * 200 * i / fs) : 0.0;
			clk.tick(dt, false, false, false, 120); f.process(x, x, clk, false, dt);
			mx = std::max(mx, f.outA);
			if (i == tStop) atStop = f.outA;
			if (i == tStop + (size_t)(0.15 * fs)) after = f.outA;
		}
		double fallRatio = after / atStop;   // one time constant: e^-1
		Follow g; g.init(fs); g.p.rise = 0.05; g.p.fall = 1.0;
		double at = 0; for (size_t i = 0; i < (size_t)(0.05 * fs); i++) { clk.tick(dt, false, false, false, 120); g.process(5.0, 5.0, clk, false, dt); at = g.outA; }
		bool ok = std::fabs(mx / 10 - 1) <= 0.05 && std::fabs(fallRatio / std::exp(-1.0) - 1) <= 0.10 && std::fabs(at / (10 * (1 - std::exp(-1.0))) - 1) <= 0.10;
		check(ok, fmt("19. FOLLOW: level, rise and fall, %.0f Hz", fs).c_str(), fmt("peak %.2f V, fall after tau %.3f (want %.3f), rise after tau %.2f V (want %.2f)", mx, fallRatio, std::exp(-1.0), at, 10 * (1 - std::exp(-1.0))));
	}
	// 20. ADSR
	{
		Adsr a; a.init(fs); a.p.attack = 0.1; a.p.decay = 0.2; a.p.sustain = 0.6; a.p.release = 0.3;
		size_t n = (size_t)(1.5 * fs), off = (size_t)(1.0 * fs); double tA = -1, tD = -1, tR = -1, sus = 0;
		for (size_t i = 0; i < n; i++) {
			a.process(i < off, dt);
			double t = (i + 1) / fs;
			if (tA < 0 && a.outA >= 9.9999) tA = t;
			if (tD < 0 && tA > 0 && a.stage == Adsr::SUS) tD = t - tA;
			if (i == off - 1) sus = a.outA;
			if (tR < 0 && i >= off && a.outA <= 1e-9) tR = t - off / fs;
		}
		bool ok = std::fabs(tA / 0.1 - 1) <= 0.05 && std::fabs(tD / 0.2 - 1) <= 0.05 && std::fabs(tR / 0.3 - 1) <= 0.05 && std::fabs(sus - 6.0) < 1e-9;
		check(ok, fmt("20. ADSR: stage times and sustain, %.0f Hz", fs).c_str(), fmt("attack %.4f s, decay %.4f s, release %.4f s, sustain %.6f V", tA, tD, tR, sus));
	}
}

// ---------------------------------------------------------------- demos
#include "demos.hpp"

int main(int argc, char** argv) {
	std::string out = argc > 1 ? argv[1] : "test/out";
	bool demos = argc > 2 && std::string(argv[2]) == "--demos";
	auto t0 = std::chrono::steady_clock::now();
	for (double fs : {44100.0, 48000.0}) { grainChecks(fs); ageChecks(fs); resChecks(fs); modChecks(fs); }
	// CPU
	{
		double fs = 48000; size_t n = (size_t)(10 * fs); Rng r; r.seed(1); Stereo x{Vec(n), Vec(n)};
		for (size_t i = 0; i < n; i++) { x.l[i] = 0.2 * r.normal(); x.r[i] = 0.2 * r.normal(); }
		GrainEngine e; e.p = freeParams(64, 1.0); e.p.scanMode = SCAN_DELAY_FREE; e.p.scan = delayKnob(0.5); e.p.spray = 0.3; e.p.age = 0.5; e.seed(1); e.setPatternSeed(1); e.init(fs, 4);
		auto a = std::chrono::steady_clock::now(); runGrains(e, x, fs, 120); auto b = std::chrono::steady_clock::now();
		ResEngine re; re.p.decay = 0.7; re.p.age = 0.5; re.init(fs);
		for (size_t i = 0; i < n; i++) { double u, v; re.process(x.l[i], x.r[i], u, v); }
		auto c = std::chrono::steady_clock::now();
		double g = std::chrono::duration<double>(b - a).count(), rs = std::chrono::duration<double>(c - b).count();
		printf("INFO  CPU at 48 kHz: Essphor with 64 grains %.1fx real time, Essphor Res %.1fx real time (one core, this machine)\n", 10 / g, 10 / rs);
	}
	if (demos) renderDemos(out);
	printf("%d failure(s), %.0f s\n", failures, std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count());
	return failures;
}
