// Offline checks for SealLegsDSP.hpp (spec section 14). Not part of the plugin.
//   g++ -O2 -std=c++17 -I../src render.cpp -o render
//   ./render checks [loop.wav]      runs every check and prints PASS / FAIL lines
//   ./render file <in.wav> <out.wav> key=value ...   renders a file through one setting (see main)
#include "SealLegsDSP.hpp"
#include <chrono>
#include <cstdarg>
#include <complex>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <string>
using namespace seallegs;

static const double FS = 48000;
static int fails = 0;
static void verdict(bool ok, const char* name, const std::string& detail) {
	std::printf("%s  %-34s %s\n", ok ? "PASS" : "FAIL", name, detail.c_str());
	if (!ok) fails++;
}
static std::string fmt(const char* f, ...) {
	char b[512];
	va_list a; va_start(a, f); std::vsnprintf(b, sizeof b, f, a); va_end(a);
	return b;
}

// ---- tiny WAV I/O ----
static bool readWav(const char* p, std::vector<float>& L, std::vector<float>& R, double& fs) {
	FILE* fp = std::fopen(p, "rb");
	if (!fp) return false;
	std::vector<uint8_t> d;
	uint8_t tmp[65536];
	size_t n;
	while ((n = std::fread(tmp, 1, sizeof tmp, fp)) > 0) d.insert(d.end(), tmp, tmp + n);
	std::fclose(fp);
	if (d.size() < 44 || std::memcmp(d.data(), "RIFF", 4) || std::memcmp(d.data() + 8, "WAVE", 4)) return false;
	int ch = 0, bits = 0, fmtTag = 0;
	size_t pos = 12;
	while (pos + 8 <= d.size()) {
		uint32_t sz; std::memcpy(&sz, &d[pos + 4], 4);
		if (!std::memcmp(&d[pos], "fmt ", 4)) {
			fmtTag = d[pos + 8] | d[pos + 9] << 8; ch = d[pos + 10] | d[pos + 11] << 8;
			uint32_t r; std::memcpy(&r, &d[pos + 12], 4); fs = r; bits = d[pos + 22] | d[pos + 23] << 8;
		}
		else if (!std::memcmp(&d[pos], "data", 4)) {
			size_t bps = bits / 8, frames = std::min<size_t>(sz, d.size() - pos - 8) / (bps * ch);
			L.resize(frames); R.resize(frames);
			for (size_t i = 0; i < frames; i++)
				for (int c = 0; c < std::min(ch, 2); c++) {
					const uint8_t* s = &d[pos + 8 + (i * ch + c) * bps];
					float v = 0;
					if (fmtTag == 3 && bits == 32) std::memcpy(&v, s, 4);
					else if (bits == 16) v = (int16_t)(s[0] | s[1] << 8) / 32768.f;
					else if (bits == 24) v = ((int32_t)(s[0] << 8 | s[1] << 16 | s[2] << 24) >> 8) / 8388608.f;
					(c == 0 ? L : R)[i] = v;
				}
			if (ch == 1) R = L;
			return true;
		}
		pos += 8 + sz + (sz & 1);
	}
	return false;
}
static void writeWav(const char* p, const std::vector<float>& L, const std::vector<float>& R, double fs) {
	FILE* fp = std::fopen(p, "wb");
	if (!fp) return;
	uint32_t n = L.size(), data = n * 4, rate = fs, br = rate * 4;
	auto u32 = [&](uint32_t v) { std::fwrite(&v, 4, 1, fp); };
	auto u16 = [&](uint16_t v) { std::fwrite(&v, 2, 1, fp); };
	std::fwrite("RIFF", 1, 4, fp); u32(36 + data); std::fwrite("WAVEfmt ", 1, 8, fp);
	u32(16); u16(1); u16(2); u32(rate); u32(br); u16(4); u16(16);
	std::fwrite("data", 1, 4, fp); u32(data);
	for (uint32_t i = 0; i < n; i++)
		for (float v : {L[i], R[i]}) { int16_t s = (int16_t)std::lround(clampd(v / 10.0, -1, 1) * 32767); u16((uint16_t)s); }
	std::fclose(fp);
}

// ---- analysis helpers ----
static void fft(std::vector<std::complex<double>>& a) {
	size_t n = a.size();
	for (size_t i = 1, j = 0; i < n; i++) {
		size_t bit = n >> 1;
		for (; j & bit; bit >>= 1) j ^= bit;
		j ^= bit;
		if (i < j) std::swap(a[i], a[j]);
	}
	for (size_t len = 2; len <= n; len <<= 1) {
		double ang = -2 * M_PI / len;
		std::complex<double> wl(std::cos(ang), std::sin(ang));
		for (size_t i = 0; i < n; i += len) {
			std::complex<double> w(1);
			for (size_t j = 0; j < len / 2; j++) {
				auto u = a[i + j], v = a[i + j + len / 2] * w;
				a[i + j] = u + v; a[i + j + len / 2] = u - v; w *= wl;
			}
		}
	}
}
// Dominant frequency of x[a..a+n) (Hann window, parabolic peak).
static double peakHz(const std::vector<float>& x, size_t a, size_t n) {
	size_t N = 1; while (N < n * 4) N <<= 1;
	std::vector<std::complex<double>> b(N);
	for (size_t i = 0; i < n && a + i < x.size(); i++) b[i] = x[a + i] * (0.5 - 0.5 * std::cos(2 * M_PI * i / n));
	fft(b);
	size_t best = 1; double bm = 0;
	for (size_t i = 2; i < N / 2 - 1; i++) if (std::abs(b[i]) > bm) { bm = std::abs(b[i]); best = i; }
	double l = std::abs(b[best - 1]), c = std::abs(b[best]), r = std::abs(b[best + 1]);
	double d = 0.5 * (l - r) / (l - 2 * c + r);
	return (best + d) * FS / N;
}
// Welch power spectrum band levels (dB).
static double bandDb(const std::vector<float>& x, size_t a, size_t len, double f0, double f1) {
	const size_t N = 8192;
	std::vector<double> P(N / 2, 0);
	int segs = 0;
	for (size_t s = a; s + N <= a + len && s + N <= x.size(); s += N / 2, segs++) {
		std::vector<std::complex<double>> b(N);
		for (size_t i = 0; i < N; i++) b[i] = x[s + i] * (0.5 - 0.5 * std::cos(2 * M_PI * i / N));
		fft(b);
		for (size_t i = 0; i < N / 2; i++) P[i] += std::norm(b[i]);
	}
	double acc = 0; int cnt = 0;
	for (size_t i = 0; i < N / 2; i++) { double f = i * FS / N; if (f >= f0 && f < f1) { acc += P[i]; cnt++; } }
	return 10 * std::log10(acc / std::max(cnt, 1) / std::max(segs, 1) + 1e-30);
}
static double rms(const std::vector<float>& x, size_t a, size_t n) {
	double s = 0; size_t c = 0;
	for (size_t i = a; i < a + n && i < x.size(); i++, c++) s += (double)x[i] * x[i];
	return std::sqrt(s / std::max<size_t>(c, 1));
}
static double db(double x) { return 20 * std::log10(x + 1e-30); }

struct Run { std::vector<float> L, R; };
// Render n samples; `step(i, frame)` sets the frame for each sample, input in volts.
static Run render(Engine& e, size_t n, const std::function<void(size_t, Frame&)>& step) {
	Run r; r.L.resize(n); r.R.resize(n);
	Frame f;
	for (size_t i = 0; i < n; i++) {
		step(i, f);
		Output o = e.process(f);
		f.tap = f.clear = f.clkEdge = false;
		r.L[i] = (float)o.l; r.R[i] = (float)o.r;
	}
	return r;
}
static Frame wetFrame(int model) {
	Frame f;
	f.model = model; f.mix = 1; f.sendReturn = true; f.drive = 0.5; f.drivePos = POS_IN;
	f.modDepth = 0; f.modShape = 6; f.color = 0; f.hp = 0; f.lp = 1; f.fog = 0; f.stereo = TW;
	return f;
}
static const char* MN[3] = {"TAPE", "BBD", "DIGI"};

static void check1() {
	for (int md = 0; md < 3; md++) {
		Engine e; e.setSampleRate(FS);
		Frame base = wetFrame(md); base.timeOverride = 0.3; base.fbGainOverride = 0.5;
		Run r = render(e, (size_t)(1.2 * FS), [&](size_t i, Frame& f) {
			f = base;
			double t = i / FS;
			f.inL = f.inR = t < 0.05 ? 2.5 * std::sin(2 * M_PI * 1000 * t) : 0;
		});
		std::string d; bool ok = true; double prev = 0;
		for (int k = 1; k <= 3; k++) {
			size_t a = (size_t)((k * 0.3 - 0.1) * FS), b = (size_t)((k * 0.3 + 0.15) * FS);
			double e2 = 0, et = 0, pk = 0;
			size_t onset = 0;
			for (size_t i = a; i < b; i++) pk = std::max(pk, (double)std::fabs(r.L[i]));
			for (size_t i = a; i < b; i++) if (std::fabs(r.L[i]) > 0.5 * pk) { onset = i; break; }
			for (size_t i = a; i < b; i++) { double v = r.L[i] * r.L[i]; e2 += v; et += v * i; }
			double ms = onset / FS * 1000;   // 50 % rise of the echo's peak
			ok &= std::fabs(ms - k * 300) <= 1.0;
			d += fmt("%.1f ms", ms);
			(void)et;
			if (k > 1) { d += fmt(" x%.2f", pk / prev); ok &= pk / prev > 0.4 && pk / prev < 0.66; }
			d += k < 3 ? ", " : "";
			prev = pk;
		}
		verdict(ok, fmt("1 timing %s", MN[md]).c_str(), d);
	}
}

static Run glideRun(int md, bool jump) {
	Engine e; e.setSampleRate(FS);
	Frame base = wetFrame(md); base.fbGainOverride = 0; base.jump = jump; base.modShape = 6;
	return render(e, (size_t)(4.0 * FS), [&](size_t i, Frame& f) {
		f = base;
		double t = i / FS;
		f.timeOverride = t < 2.0 ? 0.3 : 0.2;
		f.inL = f.inR = 2.5 * std::sin(2 * M_PI * 440 * t);
	});
}

static void check2() {
	for (int md = 0; md < 3; md++) {
		Run r = glideRun(md, false);
		std::string d; bool ok = true;
		double mx = 0, mxT = 0;
		std::vector<double> fr;
		for (int w = 0; w < 30; w++) {   // 20 ms windows from 2.0 s
			double hz = peakHz(r.L, (size_t)((2.0 + w * 0.02) * FS), (size_t)(0.02 * FS));
			fr.push_back(hz);
			if (hz > mx) { mx = hz; mxT = w * 20; }
		}
		double end = peakHz(r.L, (size_t)(3.8 * FS), (size_t)(0.15 * FS));
		if (md == TAPE) {
			ok = mx > 540 && mx < 600 && mxT <= 300 && std::fabs(end - 440) < 3;
			d = fmt("peak %.0f Hz at +%.0f ms, settles at %.1f Hz", mx, mxT, end);
		}
		else {
			int hi = 0;
			for (int w = 1; w < 10; w++) hi += std::fabs(fr[w] - 660) < 12;
			bool after = std::fabs(fr[12] - 440) < 12 && std::fabs(fr[15] - 440) < 12;
			ok = hi >= 8 && after && std::fabs(end - 440) < 2;
			d = fmt("+20..200 ms: %.0f %.0f %.0f %.0f Hz; +240 ms %.0f Hz; end %.1f Hz", fr[1], fr[4], fr[7], fr[9], fr[12], end);
		}
		verdict(ok, fmt("2 glide %s", MN[md]).c_str(), d);
	}
}

static void check3() {
	for (int md = 0; md < 3; md++) {
		Run r = glideRun(md, true);
		double lo = 1e9, hi = 0;
		for (int w = 0; w < 30; w++) {
			double hz = peakHz(r.L, (size_t)((2.0 + w * 0.02) * FS), (size_t)(0.02 * FS));
			lo = std::min(lo, hz); hi = std::max(hi, hz);
		}
		// click: energy above 2 kHz (8th-order Butterworth HP), peak during the jump vs steady state
		Biquad h[4];
		const double Q[4] = {0.51, 0.60, 0.90, 2.56};
		for (int i = 0; i < 4; i++) {
			double w = 2 * M_PI * 2000 / FS, c = std::cos(w), al = std::sin(w) / (2 * Q[i]), a0 = 1 + al;
			h[i].b0 = (1 + c) / 2 / a0; h[i].b1 = -(1 + c) / a0; h[i].b2 = h[i].b0; h[i].a1 = -2 * c / a0; h[i].a2 = (1 - al) / a0;
		}
		double pkJ = 0, pkS = 0;
		for (size_t i = (size_t)(1.5 * FS); i < (size_t)(2.6 * FS); i++) {
			double y = r.L[i];
			for (auto& b : h) y = b.process(y);
			if (i > (size_t)(1.7 * FS) && i < (size_t)(1.95 * FS)) pkS = std::max(pkS, std::fabs(y));
			if (i >= (size_t)(2.0 * FS)) pkJ = std::max(pkJ, std::fabs(y));
		}
		double rel = db(pkJ / 2.5), relS = db(pkS / 2.5);
		bool ok = lo > 436 && hi < 444 && (rel < -40 || rel < relS + 3);   // no click above the model's own images / hiss
		verdict(ok, fmt("3 jump %s", MN[md]).c_str(),
			fmt("pitch %.1f..%.1f Hz; HF peak %.1f dB re signal (steady %.1f dB)", lo, hi, rel, relS));
	}
}

static std::vector<double> speedSeq(double speed, int rev, int nWin) {
	Engine e; e.setSampleRate(FS);
	Frame base = wetFrame(DIGI); base.timeOverride = 0.3; base.fbGainOverride = 0.7; base.speed = speed; base.reverse = rev;
	Run r = render(e, (size_t)((0.3 * (nWin + 1) + 0.1) * FS), [&](size_t i, Frame& f) {
		f = base;
		double t = i / FS;
		f.inL = f.inR = t < 0.3 ? 2.5 * std::sin(2 * M_PI * 300 * t) : 0;
	});
	std::vector<double> s;
	for (int w = 1; w <= nWin; w++) s.push_back(peakHz(r.L, (size_t)(w * 0.3 * FS + 0.02 * FS), (size_t)(0.26 * FS)));
	return s;
}
static void check4() {
	auto show = [](const std::vector<double>& s) { std::string d; for (double v : s) d += fmt("%.0f ", v); return d; };
	auto close = [](const std::vector<double>& s, std::vector<double> want) {
		for (size_t i = 0; i < want.size(); i++) if (std::fabs(s[i] / want[i] - 1) > 0.06) return false;
		return true;
	};
	auto s2 = speedSeq(2, REV_OFF, 6), sh = speedSeq(0.5, REV_OFF, 3), sr = speedSeq(1, REV_ON, 4);
	verdict(close(s2, {600, 600, 1200, 2400, 2400, 4800}), "4 speed x2", show(s2) + "Hz (want 600 600 1200 2400 2400 4800)");
	verdict(close(sh, {150, 75, 37.5}), "4 speed x0.5", show(sh) + "Hz (want 150 75 37)");
	verdict(close(sr, {300, 300, 300, 300}), "4 reverse", show(sr) + "Hz (want 300 throughout)");
}

static void check5() {
	struct Want { double D, f0, f1, dB; };
	const Want W[4] = {{0.3, 8000, 16000, -7}, {1.0, 2000, 4000, -4}, {1.0, 4000, 8000, -23}, {1.5, 2000, 4000, -12}};
	for (const Want& w : W) {
		Engine e; e.setSampleRate(FS);
		Frame base = wetFrame(BBD); base.timeOverride = w.D; base.fbGainOverride = 0;
		Rng rn(7);
		Run r = render(e, (size_t)((w.D + 4.0) * FS), [&](size_t, Frame& f) {
			f = base;
			f.inL = f.inR = 1.5 * rn.bi();
		});
		size_t a = (size_t)((w.D + 0.5) * FS), n = (size_t)(3.0 * FS);
		double ref = bandDb(r.L, a, n, 100, 500), b = bandDb(r.L, a, n, w.f0, w.f1);
		double rel = b - ref;
		verdict(std::fabs(rel - w.dB) <= 3, fmt("5 BBD %.1f s %g-%gk", w.D, w.f0 / 1000, w.f1 / 1000).c_str(),
			fmt("%.1f dB (want %.0f)", rel, w.dB));
	}
}

static void check6() {
	{
		Engine e; e.setSampleRate(FS);
		Frame base = wetFrame(DIGI); base.time = 0.5; base.fbGainOverride = 0.6;
		Rng rn(3);
		const double T0 = 2.0, T1 = 62.0;
		Run r = render(e, (size_t)(T1 * FS), [&](size_t i, Frame& f) {
			f = base;
			double t = i / FS;
			f.inL = 2.0 * rn.bi() * (0.5 + 0.5 * std::sin(2 * M_PI * 3 * t));
			f.inR = f.inL;
			f.freeze = t >= T0;
			if (t > T0 + 1) { f.time = 0.5 + 0.5 * std::sin(2 * M_PI * 0.05 * t); f.modDepth = 1; f.modRate = 0.5; }
		});
		int64_t len = e.A.flen;
		double worst = 0;
		for (size_t i = (size_t)((T0 + 0.5) * FS) + len; i < r.L.size(); i++) worst = std::max(worst, (double)std::fabs(r.L[i] - r.L[i - len]));
		verdict(worst < 1e-6, "6 DIGI freeze repeats", fmt("loop %lld samples, worst sample difference %.2g V over 60 s", (long long)len, worst));
	}
	{
		Engine e; e.setSampleRate(FS);
		Frame base = wetFrame(DIGI); base.timeOverride = 0.25; base.fbGainOverride = 0;
		Rng rn(5);
		std::vector<float> in;
		Run r = render(e, (size_t)(3 * FS), [&](size_t i, Frame& f) {
			f = base;
			double t = i / FS;
			f.inL = f.inR = 2.0 * std::sin(2 * M_PI * 997 * t) + 1.0 * rn.bi();
			in.push_back((float)f.inL);
		});
		// reference: the input through the spec's 30 Hz write DC blocker, delayed 0.25 s
		double dc = 0, a = opc(30, FS), err = 0, sig = 0;
		size_t d = (size_t)(0.25 * FS);
		std::vector<float> ref(in.size());
		for (size_t i = 0; i < in.size(); i++) { dc += a * (in[i] - dc); ref[i] = in[i] - dc; }
		for (size_t i = FS; i < r.L.size(); i++) { double x = ref[i - d]; err += (r.L[i] - x) * (r.L[i] - x); sig += x * x; }
		double rel = 10 * std::log10(err / sig);
		verdict(rel < -80, "6 DIGI clean = delayed input", fmt("residual %.1f dB (against the input after the 30 Hz write DC blocker)", rel));
	}
}

static void check7() {
	for (int md : {TAPE, BBD}) {
		Engine e; e.setSampleRate(FS);
		Frame base = wetFrame(md); base.timeOverride = md == TAPE ? 0.5 : 1.0; base.fbGainOverride = 0.3;
		Rng rn(11);
		Biquad bp; bp.bandpass(FS, 600, 0.7);
		Run r = render(e, (size_t)(34 * FS), [&](size_t i, Frame& f) {
			f = base;
			double t = i / FS;
			f.inL = f.inR = t < 2.0 ? 2.5 * bp.process(rn.bi()) : 0;
			f.freeze = t >= 2.0;
		});
		double first = rms(r.L, (size_t)(3 * FS), FS), lo = 1e9, hi = 0, last = rms(r.L, (size_t)(32 * FS), FS);
		for (int s = 3; s < 33; s++) { double v = rms(r.L, s * FS, FS); lo = std::min(lo, v); hi = std::max(hi, v); }
		if (md == TAPE)
			verdict(db(hi / first) < 3 && db(lo / first) > -3, "7 soft freeze TAPE holds",
				fmt("1 s rms over 30 s: %.1f..%+.1f dB re the first second", db(lo / first), db(hi / first)));
		else
			verdict(db(last / first) < -12, "7 soft freeze BBD 1 s decays", fmt("%.1f dB after 30 s", db(last / first)));
	}
}

static void check8(const std::vector<float>& LL, const std::vector<float>& RR) {
	for (int md = 0; md < 3; md++)
		for (int st = 0; st < 3; st++) {
			Engine e; e.setSampleRate(FS);
			Frame base = wetFrame(md); base.fb = 1.0; base.stereo = st; base.fog = 1; base.fogPos = POS_LOOP; base.fogDecay = 1;
			base.time = 0.4; base.width = 0.5; base.sendReturn = false; base.mix = 0.6; base.color = 0.5; base.modDepth = 0.4;
			base.modShape = 0; base.noise = 0.3;
			double mx = 0; bool nan = false;
			Frame f = base;
			for (size_t i = 0; i < (size_t)(60 * FS); i++) {
				size_t j = (size_t)(i * 44100.0 / FS) % LL.size();
				f.inL = 5 * LL[j]; f.inR = 5 * RR[j];
				Output o = e.process(f);
				if (!std::isfinite(o.l) || !std::isfinite(o.r)) nan = true;
				mx = std::max({mx, std::fabs(o.l), std::fabs(o.r)});
			}
			verdict(!nan && mx <= 11.0, fmt("8 stability %s %s", MN[md], st == PONG ? "PONG" : st == TW ? "T:W" : "L/R").c_str(),
				fmt("peak %.2f V%s", mx, nan ? ", NaN" : ""));
		}
}

static void check9() {
	Engine e; e.setSampleRate(FS);
	Frame base = wetFrame(TAPE); base.mix = 0; base.sendReturn = false;
	Run r = render(e, FS, [&](size_t i, Frame& f) { f = base; f.inL = f.inR = 5 * std::sin(2 * M_PI * 1000 * i / FS); });
	double g = db(rms(r.L, FS / 2, FS / 2) / (5 / std::sqrt(2.0)));
	verdict(std::fabs(g) < 0.5, "9 unity", fmt("%.2f dB", g));
}

static void check10(const std::vector<float>& LL, const std::vector<float>& RR) {
	for (int worst = 0; worst < 2; worst++) {
		Engine e; e.setSampleRate(FS);
		Frame f = wetFrame(BBD);
		f.sendReturn = false; f.mix = 0.5; f.fb = 0.6; f.time = 0.5;
		if (worst) { f.fog = 0.6; f.noise = 0.5; f.modDepth = 0.5; f.modShape = 6.5; f.reverse = REV_MIX; f.speed = 2; f.color = 0.5; f.width = 0.3; }
		size_t n = (size_t)(20 * FS);
		auto t0 = std::chrono::steady_clock::now();
		double sink = 0;
		for (size_t i = 0; i < n; i++) {
			size_t j = i % LL.size();
			f.inL = 5 * LL[j]; f.inR = 5 * RR[j];
			if (worst && i % (size_t)(0.5 * FS) == 0) f.model = (f.model + 1) % 3;
			sink += e.process(f).l;
		}
		double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
		double pct = 100 * secs / 20.0;
		verdict(pct < 5, worst ? "10 CPU, busy patch" : "10 CPU, plain patch", fmt("%.2f %% of one core (this machine, x86 cloud)%s", pct, sink == 1234.5 ? " " : ""));
	}
}

int main(int argc, char** argv) {
	std::string mode = argc > 1 ? argv[1] : "checks";
	std::vector<float> LL, RR;
	double lfs = 44100;
	if (argc > 2 && mode == "checks") readWav(argv[2], LL, RR, lfs);
	if (LL.empty()) {   // fallback test loop: plucked noise bursts
		Rng rn(1);
		LL.resize(4 * 44100); RR.resize(LL.size());
		for (size_t i = 0; i < LL.size(); i++) {
			double t = std::fmod(i / 44100.0, 0.5);
			LL[i] = RR[i] = (float)(0.6 * rn.bi() * std::exp(-t * 12));
		}
	}
	if (mode == "checks") {
		check1(); check2(); check3(); check4(); check5(); check6(); check7(); check9(); check8(LL, RR); check10(LL, RR);
		std::printf("%d failed\n", fails);
		return fails ? 1 : 0;
	}
	if (mode == "file" && argc >= 4) {
		std::vector<float> L, R; double fs;
		if (!readWav(argv[2], L, R, fs)) { std::fprintf(stderr, "cannot read %s\n", argv[2]); return 1; }
		Engine e; e.setSampleRate(fs);
		Frame f;
		double tail = 8;
		for (int a = 4; a < argc; a++) {
			std::string kv = argv[a]; size_t eq = kv.find('=');
			if (eq == std::string::npos) continue;
			std::string k = kv.substr(0, eq); double v = std::atof(kv.c_str() + eq + 1);
			if (k == "model") f.model = (int)v; else if (k == "stereo") f.stereo = (int)v; else if (k == "range") f.range = (int)v;
			else if (k == "time") f.time = v; else if (k == "width") f.width = v; else if (k == "fb") f.fb = v;
			else if (k == "mix") f.mix = v; else if (k == "color") f.color = v; else if (k == "hp") f.hp = v; else if (k == "lp") f.lp = v;
			else if (k == "rate") f.modRate = v; else if (k == "depth") f.modDepth = v; else if (k == "shape") f.modShape = v;
			else if (k == "fog") f.fog = v; else if (k == "decay") f.fogDecay = v; else if (k == "fogpos") f.fogPos = (int)v;
			else if (k == "drive") f.drive = v; else if (k == "drivepos") f.drivePos = (int)v; else if (k == "noise") f.noise = v;
			else if (k == "noisemode") f.noiseMode = v; else if (k == "duck") f.duck = v; else if (k == "rev") f.reverse = (int)v;
			else if (k == "speed") f.speed = v; else if (k == "jump") f.jump = v > 0; else if (k == "tail") tail = v;
		}
		std::vector<float> oL, oR;
		for (size_t i = 0; i < L.size() + (size_t)(tail * fs); i++) {
			f.inL = i < L.size() ? 5 * L[i] : 0; f.inR = i < L.size() ? 5 * R[i] : 0;
			Output o = e.process(f);
			oL.push_back((float)o.l); oR.push_back((float)o.r);
		}
		writeWav(argv[3], oL, oR, fs);
		return 0;
	}
	std::fprintf(stderr, "usage: render checks [loop.wav] | render file in.wav out.wav key=value...\n");
	return 1;
}
