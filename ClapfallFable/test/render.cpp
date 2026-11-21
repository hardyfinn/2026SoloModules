// Offline checks and demo renders for Clapfall (no Rack needed):  make test
//   ./test/render <outdir>
#include "../src/dsp/Engine.hpp"
#include <cstdio>
#include <cstring>
#include <chrono>
#include <complex>
#include <functional>
#include <map>

using namespace clapfall;

static int failures = 0;
#define CHECK(cond, ...) do { if (!(cond)) { failures++; printf("  FAIL: "); printf(__VA_ARGS__); printf("\n"); } else { printf("  ok:   "); printf(__VA_ARGS__); printf("\n"); } } while (0)

static void writeWav(const std::string& path, const std::vector<float>& L, const std::vector<float>& R, int sr) {
	FILE* f = fopen(path.c_str(), "wb");
	if (!f) { printf("cannot write %s\n", path.c_str()); return; }
	int n = (int)L.size(), ch = 2, bytes = n * ch * 2;
	auto w32 = [&](uint32_t v) { fwrite(&v, 4, 1, f); };
	auto w16 = [&](uint16_t v) { fwrite(&v, 2, 1, f); };
	fwrite("RIFF", 1, 4, f); w32(36 + bytes); fwrite("WAVEfmt ", 1, 8, f);
	w32(16); w16(1); w16(ch); w32(sr); w32(sr * ch * 2); w16(ch * 2); w16(16);
	fwrite("data", 1, 4, f); w32(bytes);
	for (int i = 0; i < n; i++) {
		for (int c = 0; c < 2; c++) {
			float v = (c ? R[i] : L[i]) / 10.f;   // +-10 V -> full scale
			v = std::max(-1.f, std::min(1.f, v));
			int16_t s = (int16_t)std::lrint(v * 32767.f);
			fwrite(&s, 2, 1, f);
		}
	}
	fclose(f);
}

// ---- spectrum helpers
static void fft(std::vector<std::complex<double>>& a) {
	int n = (int)a.size();
	for (int i = 1, j = 0; i < n; i++) {
		int bit = n >> 1;
		for (; j & bit; bit >>= 1) j ^= bit;
		j ^= bit;
		if (i < j) std::swap(a[i], a[j]);
	}
	for (int len = 2; len <= n; len <<= 1) {
		double ang = -2 * M_PI / len;
		std::complex<double> wl(std::cos(ang), std::sin(ang));
		for (int i = 0; i < n; i += len) {
			std::complex<double> w(1);
			for (int j = 0; j < len / 2; j++) {
				auto u = a[i + j], v = a[i + j + len / 2] * w;
				a[i + j] = u + v; a[i + j + len / 2] = u - v; w *= wl;
			}
		}
	}
}
static const int FB[] = {200, 315, 500, 630, 800, 1000, 1250, 1600, 2000, 2500, 3150, 4000, 5000, 6300, 8000, 10000, 12500};
static const int NFB = 17;
static std::vector<double> bands(const std::vector<float>& x, double sr) {
	float mx = 0; for (float v : x) mx = std::max(mx, std::fabs(v));
	int i0 = 0; while (i0 < (int)x.size() && std::fabs(x[i0]) < 0.1f * mx) i0++;
	int a = std::max(0, i0 - 32), b = std::min((int)x.size(), i0 + (int)(0.04 * sr));
	std::vector<std::complex<double>> s(1 << 15, 0.0);
	for (int i = a; i < b; i++) s[i - a] = x[i];
	fft(s);
	std::vector<double> e(NFB, 0.0);
	for (int k = 0; k < (1 << 14); k++) {
		double f = k * sr / (1 << 15);
		for (int j = 0; j < NFB; j++) if (f > FB[j] / 1.122 && f < FB[j] * 1.122) e[j] += std::norm(s[k]);
	}
	double m = 1e-30; for (double v : e) m = std::max(m, v);
	for (double& v : e) v = 10 * std::log10(v / m + 1e-12);
	return e;
}

// ---- knob helpers (from Hz / seconds)
static float kDensity(float hz) { return std::log(hz / 1.5f) / std::log(400.f); }
static float kDecay(float s) { return std::log(s / 0.012f) / std::log(500.f); }
static float kTone(float hz) { return std::log(hz / 150.f) / std::log(10000.f / 150.f); }

static std::vector<Surface> SURF;
static int surfIdx(const char* n) { for (int i = 0; i < NUM_BUILTIN; i++) if (SURF[i].name == n) return i; return 0; }

struct Setting {
	const char* name; const char* surf;
	float density, decay, loose, tone, spread; int env, filter; float reverb, size;
};
static Controls ctl(const Setting& s) {
	Controls c;
	c.surf = &SURF[surfIdx(s.surf)];
	c.density = s.density; c.decay = s.decay; c.loose = s.loose; c.tone = s.tone; c.spread = s.spread;
	c.env = s.env; c.filter = s.filter; c.reverb = s.reverb; c.size = s.size;
	return c;
}

struct Out { std::vector<float> L, R; };
// render with trigger times (s); optional per-sample control hook
static Out render(Controls c, double dur, std::vector<double> trigs, float sr = 48000.f,
                  std::vector<double> ones = {}, std::function<void(Controls&, double)> hook = nullptr, Engine* eng = nullptr) {
	Engine local; Engine& e = eng ? *eng : local;
	e.setSampleRate(sr);
	int n = (int)(dur * sr);
	Out o; o.L.resize(n); o.R.resize(n);
	size_t ti = 0, oi = 0;
	std::sort(trigs.begin(), trigs.end());
	for (int i = 0; i < n; i++) {
		double t = i / (double)sr;
		bool tr = false, on = false;
		while (ti < trigs.size() && trigs[ti] <= t) { tr = true; ti++; }
		while (oi < ones.size() && ones[oi] <= t) { on = true; oi++; }
		if (hook) hook(c, t);
		e.process(c, tr, on, false, o.L[i], o.R[i]);
	}
	return o;
}

static std::vector<float> addKick(Out& o, std::vector<double> times, float sr = 48000.f, float level = 6.f) {
	for (double t0 : times) {
		int i0 = (int)(t0 * sr);
		double ph = 0;
		for (int i = 0; i < (int)(0.5 * sr) && i0 + i < (int)o.L.size(); i++) {
			double t = i / sr;
			double f = 48 + 110 * std::exp(-t / 0.03);
			ph += 2 * M_PI * f / sr;
			float v = level * (float)(std::sin(ph) * std::exp(-t / 0.22) * (1 - std::exp(-t / 0.0015)));
			o.L[i0 + i] += v; o.R[i0 + i] += v;
		}
	}
	return {};
}

int main(int argc, char** argv) {
	std::string outdir = argc > 1 ? argv[1] : ".";
	printf("Clapfall offline checks\n");

	// 1. tables
	auto t0 = std::chrono::steady_clock::now();
	SURF = buildSurfaces();
	double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
	size_t bytes = 0; bool finite = true, peak = true, dc = true;
	for (auto& s : SURF)
		for (auto& v : s.vars) {
			bytes += v.size() * 4;
			float m = 0; double sum = 0;
			for (float x : v) { if (!std::isfinite(x)) finite = false; m = std::max(m, std::fabs(x)); sum += x; }
			if (std::fabs(m - 1.f) > 1e-3f) peak = false;
			if (std::fabs(sum / v.size()) > 0.02) dc = false;
		}
	printf("[1] tables\n");
	CHECK(ms < 300, "build time %.0f ms (%.1f MB)", ms, bytes / 1e6);
	CHECK(finite && peak && dc, "all tables finite, peak 1.0, DC-free");

	// 2. hand spectra
	printf("[2] hand spectra\n");
	auto avgBands = [&](const char* n) {
		std::vector<double> acc(NFB, 0.0);
		for (auto& v : SURF[surfIdx(n)].vars) { auto b = bands(v, 48000); for (int j = 0; j < NFB; j++) acc[j] += b[j] / NVAR; }
		return acc;
	};
	auto printB = [&](const char* n, const std::vector<double>& b) { printf("        %-8s", n); for (double v : b) printf("%4.0f", v); printf("\n"); };
	printf("        band    "); for (int j = 0; j < NFB; j++) printf("%4d", FB[j] / 100); printf("  (x100 Hz)\n");
	auto bc = avgBands("CLAP"), bcu = avgBands("CUPPED");
	printB("CLAP", bc); printB("CUPPED", bcu); printB("PALMS", avgBands("PALMS")); printB("FINGERS", avgBands("FINGERS"));
	int pk = (int)(std::max_element(bc.begin(), bc.end()) - bc.begin());
	CHECK(FB[pk] >= 1250 && FB[pk] <= 2000, "CLAP peaks at %d Hz band", FB[pk]);
	CHECK(bc[13] <= -8, "CLAP is %.0f dB down at 6.3 kHz", bc[13]);
	int pkc = (int)(std::max_element(bcu.begin(), bcu.end()) - bcu.begin());
	CHECK(FB[pkc] <= 800, "CUPPED peaks at %d Hz band", FB[pkc]);

	// 3. 808 shape and repeatability
	printf("[3] 808 clap\n");
	Setting s808 = {"808", "808", kDensity(95), kDecay(0.09f), 0, kTone(1100), 0, ENV_CHANCE, FILT_PUNCH, 0, 0.2f};
	{
		Controls c = ctl(s808);
		Out o = render(c, 2.0, {0.1, 1.1});
		int h0 = (int)(0.1 * 48000), h1 = (int)(1.1 * 48000);
		std::vector<float> fr;
		for (int k = 0; k < 10; k++) {
			double e = 0; for (int i = 0; i < 960; i++) { float v = o.L[h0 + k * 960 + i]; e += v * v; }
			fr.push_back((float)std::sqrt(e / 960));
		}
		float mx = *std::max_element(fr.begin(), fr.end());
		printf("        20 ms frames (%% of max):"); for (float v : fr) printf(" %3.0f", 100 * v / mx); printf("\n");
		int am = (int)(std::max_element(fr.begin(), fr.end()) - fr.begin());
		CHECK(am <= 2 && fr[1] >= 0.6f * fr[0] && fr[5] < 0.25f * mx, "rises or holds for 40 ms, below a quarter by 100 ms");
		float diff = 0, pk2 = 0;
		for (int i = 0; i < 48000 / 2; i++) { diff = std::max(diff, std::fabs(o.L[h0 + i] - o.L[h1 + i])); pk2 = std::max(pk2, std::fabs(o.L[h0 + i])); }
		CHECK(diff < 1e-3f * pk2, "two hits identical at LOOSE 0 (max diff %.2g of peak %.2f V)", diff, pk2);
	}

	// 4. particle count at LOOSE 0
	printf("[4] particle count\n");
	{
		Controls c; c.surf = &SURF[0]; c.loose = 0; c.env = ENV_CHANCE; c.density = kDensity(100);
		bool all = true;
		for (float dec : {0.1f, 0.3f, 1.0f}) {
			c.decay = kDecay(dec);
			Engine e; render(c, dec + 0.5, {0.0}, 48000, {}, nullptr, &e);
			double tau = dec / 3.0;
			int expect = (int)std::floor(100 * tau * std::log(2.0)) + 1;
			printf("        decay %.1f s: %d particles, expected %d\n", dec, e.particleCount, expect);
			if (std::abs(e.particleCount - expect) > 1) all = false;
		}
		CHECK(all, "count matches DECAY within one");
	}

	// 5. interval statistics
	printf("[5] timing\n");
	for (float loose : {1.f, 0.f}) {
		Controls c; c.surf = &SURF[0]; c.loose = loose; c.env = ENV_AMP; c.decay = 1.f; c.free = true; c.density = kDensity(50);
		Engine e; e.setSampleRate(48000);
		std::vector<double> times; int last = 0; float l, r;
		for (int i = 0; i < 48000 * 60; i++) {
			e.process(c, false, false, false, l, r);
			if (e.particleCount != last) { last = e.particleCount; times.push_back(i / 48000.0); }
		}
		double m = 0, v = 0; int n = (int)times.size() - 1;
		for (int i = 0; i < n; i++) m += times[i + 1] - times[i];
		m /= n;
		for (int i = 0; i < n; i++) { double d = times[i + 1] - times[i] - m; v += d * d; }
		double cv = std::sqrt(v / n) / m;
		if (loose > 0) CHECK(cv > 0.85 && cv < 1.15, "LOOSE 1 interval CV %.3f (%d particles, mean %.1f ms)", cv, n, m * 1000);
		else CHECK(cv < 0.01, "LOOSE 0 interval CV %.4f", cv);
	}

	// 6. free silence, fuzz
	printf("[6] robustness\n");
	{
		Controls c; c.surf = &SURF[0]; c.free = true; c.decay = 0.f; c.env = ENV_BOTH;
		Out o = render(c, 2.0, {0.5, 1.0});
		float m = 0; for (float v : o.L) m = std::max(m, std::fabs(v));
		CHECK(m < 1e-6f, "FREE with DECAY 0 is silent (peak %.2g V)", m);
	}
	for (float sr : {44100.f, 48000.f, 96000.f}) {
		Engine e; e.setSampleRate(sr);
		Rng r(99);
		Controls c;
		float peak = 0; bool fin = true;
		auto st = std::chrono::steady_clock::now();
		int n = (int)(60 * sr);
		for (int i = 0; i < n; i++) {
			if (i % (int)(sr * 0.25f) == 0) {
				c.surf = &SURF[r.below(NUM_BUILTIN)];
				c.density = (float)r.uni(); c.decay = (float)r.uni(); c.loose = (float)r.uni(); c.tone = (float)r.uni();
				c.spread = (float)r.uni(); c.reverb = (float)r.uni(); c.size = (float)r.uni();
				c.env = r.below(3); c.filter = r.below(3); c.free = r.uni() < 0.25; c.vOctOn = r.uni() < 0.3; c.vOct = (float)r.uni(-5, 5);
			}
			bool tr = r.uni() < 8.0 / sr, on = r.uni() < 4.0 / sr, ac = r.uni() < 0.3;
			float l, rr;
			e.process(c, tr, on, ac, l, rr);
			if (!std::isfinite(l) || !std::isfinite(rr)) fin = false;
			peak = std::max(peak, std::max(std::fabs(l), std::fabs(rr)));
		}
		double sec = std::chrono::duration<double>(std::chrono::steady_clock::now() - st).count();
		CHECK(fin && peak <= 10.f, "fuzz %.0f Hz: finite, peak %.2f V, %.1f%% of real time", sr, peak, 100 * sec / 60);
	}
	{   // CPU in dense applause
		Controls c; c.surf = &SURF[4]; c.free = true; c.decay = 0.95f; c.density = 0.95f; c.loose = 1; c.env = ENV_CHANCE; c.reverb = 0.4f; c.size = 0.7f; c.spread = 0.4f;
		Engine e; e.setSampleRate(48000);
		auto st = std::chrono::steady_clock::now();
		float l, r;
		for (int i = 0; i < 48000 * 20; i++) e.process(c, false, false, false, l, r);
		double sec = std::chrono::duration<double>(std::chrono::steady_clock::now() - st).count();
		CHECK(sec / 20 < 0.02, "dense applause uses %.2f%% of a core", 100 * sec / 20);
	}

	// 7. reverb RT60
	printf("[7] reverb\n");
	for (float sz : {0.f, 0.5f, 1.f}) {
		Reverb rv; rv.setSampleRate(48000);
		int n = (int)(48000 * (sizeRt60(sz) * 2.0 + 0.5));
		std::vector<double> en(n), ir(n);
		for (int i = 0; i < 48000; i++) { float a, b; rv.process(0, 0, sz, a, b); }   // settle size smoothing
		rv.reset();
		for (int i = 0; i < n; i++) {
			float a, b; rv.process(i == 0 ? 1.f : 0.f, i == 0 ? 1.f : 0.f, sz, a, b);
			ir[i] = a + b;
		}
		// mid-band RT60 (octave around 1 kHz), the usual way a room's reverb time is quoted
		std::vector<double> mid(n, 0.0);
		bpAdd(ir, mid, 1000, 1.41, 1.0, 48000);
		for (int i = 0; i < n; i++) en[i] = mid[i] * mid[i];
		std::vector<double> sch(n); double acc = 0;
		for (int i = n - 1; i >= 0; i--) { acc += en[i]; sch[i] = acc; }
		auto tAt = [&](double db) { for (int i = 0; i < n; i++) if (10 * std::log10(sch[i] / sch[0]) <= db) return i / 48000.0; return n / 48000.0; };
		double t30 = 2.0 * (tAt(-35) - tAt(-5));
		double target = sizeRt60(sz);
		CHECK(std::fabs(t30 / target - 1) < 0.2, "SIZE %.1f: RT60 (1 kHz) %.2f s, target %.2f s", sz, t30, target);
	}
	{   // wet level at REVERB 1
		Controls c; c.surf = &SURF[0]; c.reverb = 1.f; c.size = 0.5f; c.decay = 0; c.density = 0; c.loose = 0;
		Out wet = render(c, 1.0, {0.1});
		c.reverb = 0.f;
		Out dry = render(c, 1.0, {0.1});
		float dpk = 0; for (float v : dry.L) dpk = std::max(dpk, std::fabs(v));
		double e = 0; int a = (int)(0.2 * 48000), b = (int)(0.25 * 48000);
		for (int i = a; i < b; i++) { float d = wet.L[i] - dry.L[i]; e += d * d; }
		float tail = (float)std::sqrt(e / (b - a)) * 1.414f;
		float db = 20 * std::log10(tail / dpk);
		CHECK(db > -14 && db < -5, "single clap: tail starts %.1f dB below the dry peak (%.2f V)", db, dpk);
	}

	// level
	{
		Controls c; c.surf = &SURF[0];
		c.decay = 0; c.density = 0;   // a single particle
		Out o = render(c, 0.5, {0.05});
		float m = 0; for (float v : o.L) m = std::max(m, std::fabs(v));
		for (float v : o.R) m = std::max(m, std::fabs(v));
		printf("[level] single CLAP particle at default settings peaks at %.2f V\n", m);
	}

	// 8. demos
	printf("[8] demos -> %s\n", outdir.c_str());
	double beat = 60.0 / 118.0;
	std::vector<double> back, quarters;
	for (int i = 0; i < 8; i++) back.push_back(beat * (1 + 2 * i) + 0.05);
	for (int i = 0; i < 16; i++) quarters.push_back(beat * i + 0.05);
	double D = beat * 16 + 1.5;
	std::vector<Setting> recipes = {
		{"01-808-clap", "808", kDensity(95), kDecay(0.09f), 0, kTone(1100), 0, ENV_CHANCE, FILT_PUNCH, 0.12f, 0.2f},
		{"02-909-clap", "909", kDensity(85), kDecay(0.13f), 0, kTone(1400), 0, ENV_CHANCE, FILT_PUNCH, 0.2f, 0.3f},
		{"03-one-person", "CLAP", kDensity(20), kDecay(0.012f), 0.5f, kTone(1600), 0.05f, ENV_BOTH, FILT_PUNCH, 0.25f, 0.2f},
		{"04-five-people", "GROUP", kDensity(170), kDecay(0.06f), 1, kTone(1500), 0.22f, ENV_CHANCE, FILT_PUNCH, 0.3f, 0.3f},
		{"05-gospel-room", "GROUP", kDensity(420), kDecay(0.11f), 1, kTone(1400), 0.3f, ENV_CHANCE, FILT_PUNCH, 0.5f, 0.55f},
		{"07-snaps", "SNAP", kDensity(60), kDecay(0.03f), 0.9f, kTone(3000), 0.12f, ENV_BOTH, FILT_PUNCH, 0.3f, 0.3f},
	};
	for (auto& s : recipes) {
		Out o = render(ctl(s), D, back);
		addKick(o, quarters);
		writeWav(outdir + "/" + s.name + ".wav", o.L, o.R, 48000);
	}
	{   // applause swell, free running, DECAY moved by hand
		Setting s = {"06-applause", "GROUP", 0.3f, 0, 1, kTone(1700), 0.4f, ENV_CHANCE, FILT_PUNCH, 0.35f, 0.7f};
		Controls c = ctl(s); c.free = true;
		Out o = render(c, 15.0, {}, 48000, {}, [](Controls& c, double t) {
			double x = t < 6 ? t / 6 : (t < 9 ? 1 : std::max(0.0, 1 - (t - 9) / 5));
			c.decay = (float)(0.05 + 0.95 * x);
			c.density = (float)(0.35 + 0.6 * x);
		});
		writeWav(outdir + "/06-applause.wav", o.L, o.R, 48000);
	}
	{
		Setting s = {"08-stick-roll", "STICKS", kDensity(28), kDecay(0.5f), 0.15f, kTone(2500), 0.15f, ENV_AMP, FILT_PUNCH, 0.2f, 0.3f};
		std::vector<double> t; for (int i = 0; i < 8; i++) t.push_back(beat * 2 * i + 0.05);
		Out o = render(ctl(s), D, t); addKick(o, quarters);
		writeWav(outdir + "/08-stick-roll.wav", o.L, o.R, 48000);
	}
	{
		Setting s = {"09-rain", "DROP", kDensity(40), kDecay(3.f), 1, 0.5f, 0.6f, ENV_CHANCE, FILT_PITCH, 0.4f, 0.6f};
		Out o = render(ctl(s), 8.0, {0.05, 4.0});
		writeWav(outdir + "/09-rain.wav", o.L, o.R, 48000);
	}
	{
		Setting s = {"10-coins", "COINS", kDensity(70), kDecay(0.5f), 1, 0.55f, 0.35f, ENV_CHANCE, FILT_PUNCH, 0.25f, 0.4f};
		Out o = render(ctl(s), 6.0, {0.05, 2.0, 4.0});
		writeWav(outdir + "/10-coins.wav", o.L, o.R, 48000);
	}
	{
		Setting s = {"11-ping-pong", "PING", kDensity(9), kDecay(2.5f), 0.3f, 0.5f, 0.1f, ENV_AMP, FILT_PUNCH, 0.3f, 0.3f};
		Out o = render(ctl(s), 6.0, {0.05, 3.0});
		writeWav(outdir + "/11-ping-pong.wav", o.L, o.R, 48000);
	}
	{   // late reverb: REVERB opened after the hit (patch an envelope into REVERB CV)
		Setting s = {"12-late-reverb", "909", kDensity(85), kDecay(0.13f), 0, kTone(1400), 0, ENV_CHANCE, FILT_PUNCH, 0, 0.85f};
		Controls c = ctl(s);
		Out o = render(c, D, back, 48000, {}, [&](Controls& c, double t) {
			double ph = std::fmod(t - 0.05 + 4 * beat, 2 * beat);   // since last backbeat
			c.reverb = (ph > 0.06 && ph < 0.25) ? 1.f : 0.f;
		});
		addKick(o, quarters);
		writeWav(outdir + "/12-late-reverb.wav", o.L, o.R, 48000);
	}
	{   // rim shots with ONE on a 16th pattern, plus a clap burst on 2 and 4
		Setting s = {"13-rim-one", "RIM", 0.5f, kDecay(0.05f), 0.3f, 0.5f, 0.05f, ENV_BOTH, FILT_PUNCH, 0.15f, 0.25f};
		std::vector<double> ones; const int pat[16] = {0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 0, 1, 0, 1, 0};
		for (int b = 0; b < 4; b++) for (int i = 0; i < 16; i++) if (pat[i]) ones.push_back(b * 4 * beat + i * beat / 4 + 0.05);
		Out o = render(ctl(s), D, {}, 48000, ones); addKick(o, quarters);
		writeWav(outdir + "/13-rim-one.wav", o.L, o.R, 48000);
	}
	{   // V/OCT melody from GLASS
		Setting s = {"14-glass-melody", "GLASS", kDensity(6), kDecay(0.6f), 0.2f, 0.5f, 0.02f, ENV_AMP, FILT_PUNCH, 0.35f, 0.6f};
		Controls c = ctl(s); c.vOctOn = true;
		const float notes[8] = {0, 3 / 12.f, 7 / 12.f, 10 / 12.f, 7 / 12.f, 3 / 12.f, 5 / 12.f, -2 / 12.f};
		std::vector<double> t; for (int i = 0; i < 16; i++) t.push_back(i * beat / 2 + 0.05);
		Out o = render(c, 16 * beat / 2 + 2, t, 48000, {}, [&](Controls& c, double tt) {
			int i = std::max(0, std::min(15, (int)((tt - 0.05) / (beat / 2)))); c.vOct = notes[i % 8] - 0.5f;
		});
		writeWav(outdir + "/14-glass-melody.wav", o.L, o.R, 48000);
	}
	{   // vinyl crackle bed
		Setting s = {"15-crackle", "CRACKLE", kDensity(25), 0.5f, 1, 0.75f, 0.2f, ENV_CHANCE, FILT_BAND, 0, 0.2f};
		Controls c = ctl(s); c.free = true;
		Out o = render(c, 6.0, {});
		for (auto& v : o.L) v *= 0.6f;
		for (auto& v : o.R) v *= 0.6f;
		writeWav(outdir + "/15-crackle.wav", o.L, o.R, 48000);
	}
	{   // gallery: every surface, four hits each at a medium setting
		Out all;
		for (int si = 0; si < NUM_BUILTIN; si++) {
			Controls c; c.surf = &SURF[si]; c.density = kDensity(60); c.decay = kDecay(0.15f); c.loose = 0.6f; c.spread = 0.15f;
			c.env = ENV_BOTH; c.filter = FILT_PUNCH; c.tone = 0.5f; c.reverb = 0.2f; c.size = 0.35f;
			if (SURF[si].choke) { c.loose = 0; c.env = ENV_CHANCE; c.density = kDensity(95); c.decay = kDecay(0.1f); c.tone = kTone(1200); }
			Out o = render(c, 2.4, {0.05, 0.45, 0.85, 1.25});
			all.L.insert(all.L.end(), o.L.begin(), o.L.end());
			all.R.insert(all.R.end(), o.R.begin(), o.R.end());
		}
		writeWav(outdir + "/00-gallery.wav", all.L, all.R, 48000);
	}

	printf("%s (%d failure%s)\n", failures ? "FAILED" : "all passed", failures, failures == 1 ? "" : "s");
	return failures ? 1 : 0;
}
