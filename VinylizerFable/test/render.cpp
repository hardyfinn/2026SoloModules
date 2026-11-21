// Offline checks for the Vinylizer Fable engine, plus a demo WAV. No Rack needed.
//   make test   (writes vinylizer-demo.wav into the folder given as argv[1])
#include "../src/VinylDSP.hpp"
#include <cstdio>
#include <string>

using namespace vinyl;
static const float FS = 48000;

static Settings allOff() {
	Settings s;
	s.ellOn = s.hflOn = s.cutOn = s.medOn = s.trcOn = s.trkOn = s.cartOn = s.armOn = s.pltOn = s.phoOn = false;
	return s;
}

struct Tone { double fund = 0, h2 = 0, h3 = 0, side = 0; };

// amplitude of harmonics of a sine through the engine (left channel), stereo input given by phase of R
static Tone measure(Settings s, float f, float amp, bool antiphase = false) {
	Engine e;
	e.s = s;
	e.init(FS);
	int N = (int)(FS * 1.5f), start = (int)(FS * 0.7f);
	std::complex<double> a1 = 0, a2 = 0, a3 = 0;
	double sideE = 0, midE = 0;
	for (int n = 0; n < N; n++) {
		float x = amp * std::sin(2 * kPi * f * n / FS);
		float wl, wr, dl, dr;
		e.process(x, antiphase ? -x : x, wl, wr, dl, dr);
		if (n >= start) {
			double t = 2 * kPi * f * n / FS;
			a1 += (double) wl * std::polar(1.0, -t);
			a2 += (double) wl * std::polar(1.0, -2 * t);
			a3 += (double) wl * std::polar(1.0, -3 * t);
			sideE += 0.25 * (wl - wr) * (wl - wr);
			midE += 0.25 * (wl + wr) * (wl + wr);
		}
	}
	double M = N - start;
	Tone r;
	r.fund = 2 * std::abs(a1) / M; r.h2 = 2 * std::abs(a2) / M; r.h3 = 2 * std::abs(a3) / M;
	r.side = midE > 0 ? 10 * std::log10(sideE / (midE + 1e-20)) : 0;
	return r;
}

static double db(double x) { return 20 * std::log10(std::max(x, 1e-12)); }
static int fails = 0;
static void check(bool ok, const char* what) { std::printf("  [%s] %s\n", ok ? " ok " : "FAIL", what); if (!ok) fails++; }

static void writeWav(const std::string& path, const std::vector<float>& L, const std::vector<float>& R) {
	FILE* f = std::fopen(path.c_str(), "wb");
	if (!f) return;
	uint32_t n = (uint32_t) L.size(), bytes = n * 4;
	auto w32 = [&](uint32_t v) { std::fwrite(&v, 4, 1, f); };
	auto w16 = [&](uint16_t v) { std::fwrite(&v, 2, 1, f); };
	std::fwrite("RIFF", 1, 4, f); w32(36 + bytes); std::fwrite("WAVEfmt ", 1, 8, f);
	w32(16); w16(1); w16(2); w32((uint32_t) FS); w32((uint32_t) FS * 4); w16(4); w16(16);
	std::fwrite("data", 1, 4, f); w32(bytes);
	for (uint32_t i = 0; i < n; i++) {
		int16_t a = (int16_t) clampf(L[i] / 6.f * 32767.f, -32767, 32767), b = (int16_t) clampf(R[i] / 6.f * 32767.f, -32767, 32767);
		w16((uint16_t) a); w16((uint16_t) b);
	}
	std::fclose(f);
}

// A little 90s-ish loop: kick, offbeat hats, stereo pad, bass. +-5 V peaks.
static void music(int n, float& l, float& r) {
	static uint32_t rs = 1;
	double t = n / FS, beat = 60.0 / 128.0;
	double bt = std::fmod(t, beat), ht = std::fmod(t + beat / 2, beat);
	double kick = std::exp(-bt * 18) * std::sin(2 * kPi * (45 * bt + 90 * (1 - std::exp(-bt * 30)) / 30));
	rs ^= rs << 13; rs ^= rs >> 17; rs ^= rs << 5;
	double nz = rs / 4294967295.0 * 2 - 1;
	static double hp = 0, prev = 0;
	hp = 0.6 * (hp + nz - prev); prev = nz;
	double hat = std::exp(-ht * 60) * hp;
	int bar = (int)(t / (beat * 4)) % 4;
	const double roots[4] = {55.0, 55.0 * 1.189, 55.0 * 0.891, 55.0 * 1.335};
	double f0 = roots[bar];
	double bassEnv = std::exp(-std::fmod(t, beat / 2) * 6);
	double bass = 0;
	for (int k = 1; k < 8; k++) bass += std::sin(2 * kPi * f0 * k * t) / k * std::pow(0.7, k);
	bass *= bassEnv;
	double padL = 0, padR = 0;
	const double iv[3] = {4, 7.02, 11};
	for (int k = 0; k < 3; k++) {
		double f = f0 * 4 * std::pow(2.0, iv[k] / 12.0);
		padL += std::sin(2 * kPi * f * t * 1.002) + 0.3 * std::sin(4 * kPi * f * t);
		padR += std::sin(2 * kPi * f * t * 0.998 + 1.0) + 0.3 * std::sin(4 * kPi * f * t + 0.5);
	}
	l = (float)(1.9 * kick + 0.9 * bass + 0.9 * hat + 0.28 * padL);
	r = (float)(1.9 * kick + 0.9 * bass + 0.9 * hat + 0.28 * padR);
}

int main(int argc, char** argv) {
	std::string dir = argc > 1 ? argv[1] : ".";
	std::printf("Vinylizer Fable engine checks\n");

	// 1. every process off: the matched record / playback pair is flat
	{
		Settings s = allOff();
		double g100 = db(measure(s, 100, 1).fund), g1k = db(measure(s, 1000, 1).fund), g10k = db(measure(s, 10000, 1).fund);
		std::printf("  all off: 100 Hz %.2f dB, 1 kHz %.2f dB, 10 kHz %.2f dB\n", g100, g1k, g10k);
		check(std::fabs(g100) < 0.3 && std::fabs(g1k) < 0.3 && std::fabs(g10k) < 0.5, "matched pair is flat with every process off");
	}
	// 2. defaults: response and distortion, outer vs inner groove of an LP
	{
		Settings s;
		s.disc = 2; s.speed = 0; s.sideMin = 20; s.wow = 0; s.flutter = 0; s.eccMm = 0;
		for (float nd : {0.f, 1.f}) {
			s.needle = nd;
			Tone a = measure(s, 1000, 2), b = measure(s, 5000, 2), c = measure(s, 100, 2);
			std::printf("  LP needle %.0f: 100 Hz %.2f dB, 1 kHz %.2f dB (H2 %.2f%% H3 %.2f%%), 5 kHz %.2f dB (H2 %.2f%%)\n",
				nd, db(c.fund / 2), db(a.fund / 2), 100 * a.h2 / a.fund, 100 * a.h3 / a.fund, db(b.fund / 2), 100 * b.h2 / b.fund);
		}
		s.needle = 0; Tone o = measure(s, 5000, 2); s.needle = 1; Tone i = measure(s, 5000, 2);
		check(i.h2 / i.fund > 2 * o.h2 / o.fund, "inner groove has much more 2nd harmonic than the outer groove");
		check(i.fund < o.fund, "inner groove loses treble");
	}
	// 3. elliptic: out-of-phase bass becomes (nearly) nothing, in-phase bass passes
	{
		Settings s = allOff(); s.ellOn = true;
		double anti = db(measure(s, 50, 1, true).fund), inph = db(measure(s, 50, 1).fund);
		std::printf("  elliptic 50 Hz: in phase %.2f dB, out of phase %.2f dB\n", inph, anti);
		check(anti < -15 && std::fabs(inph) < 0.5, "elliptic folds bass to mono");
	}
	// 4. tracing distortion lands in the side channel (pinch effect) for mono input
	{
		Settings s = allOff(); s.trcOn = true; s.needle = 1; s.sideMin = 20;
		Tone t = measure(s, 4000, 3);
		std::printf("  tracing only, inner LP groove, 4 kHz mono: side/mid %.1f dB\n", t.side);
		check(t.side > -60, "tracing puts distortion into the vertical (side) signal");
	}
	// 5. mistracking with low force and a hot cut
	{
		Settings s; s.cutLevelDb = 9; s.force = 0.75f; s.cartModel = 1; s.hflOn = false; s.wow = s.flutter = s.eccMm = 0;
		s.needle = 0; s.stylus = 1;
		Engine e; e.s = s; e.init(FS);
		float mx = 0;
		for (int n = 0; n < (int) FS; n++) {
			float x = 4.5f * std::sin(2 * kPi * 8000 * n / FS), a, b, c, d;
			e.process(x, x, a, b, c, d);
			mx = std::max(mx, e.m.mistrack);
		}
		std::printf("  hot 8 kHz at 0.75 g: mistrack meter %.2f\n", mx);
		check(mx > 0.05f, "mistracking appears with a hot cut and light tracking force");
	}
	// 6. wow: eccentric hole changes the phase of a steady tone at the turn rate
	{
		Settings s = allOff(); s.pltOn = true; s.eccMm = 0.5f; s.wow = s.flutter = 0;
		Engine e; e.s = s; e.init(FS);
		float mn = 1e9f, mxv = -1e9f, prev = 0; int last = -1; std::vector<int> zc;
		for (int n = 0; n < (int)(FS * 4); n++) {
			float x = std::sin(2 * kPi * 1000 * n / FS), a, b, c, d;
			e.process(x, x, a, b, c, d);
			if (n > FS && prev < 0 && a >= 0) { if (last >= 0) zc.push_back(n - last); last = n; }
			prev = a;
		}
		for (int p : zc) { mn = std::min(mn, (float) p); mxv = std::max(mxv, (float) p); }
		(void) mn;
		std::printf("  eccentric 0.5 mm: period spread %.0f..%.0f samples\n", mn, mxv);
		check(mxv - mn >= 1, "eccentric record wobbles the pitch");
	}

	// demo: dry, 12" 45 outer, LP 33 at the label, worn dub plate (8 s each)
	{
		std::vector<float> L, R;
		float mt = 0, hg = 0, eg = 0;
		auto run = [&](Settings s, float secs) {
			Engine e; e.s = s; e.init(FS);
			int n0 = (int) L.size();
			for (int n = 0; n < (int)(secs * FS); n++) {
				float l, r, a, b, c, d;
				music(n0 + n, l, r);
				e.process(l, r, a, b, c, d);
				L.push_back(a); R.push_back(b);
				mt = std::max(mt, e.m.mistrack); hg = std::min(hg, e.m.hflGR); eg = std::min(eg, e.m.excGR);
			}
			double pk = 0, rms = 0;
			for (size_t i = n0; i < L.size(); i++) { pk = std::max(pk, (double) std::fabs(L[i])); rms += L[i] * L[i]; }
			std::printf("  demo part: peak %.2f V, rms %.2f V, mistrack %.2f, HF GR %.1f dB, excursion GR %.1f dB\n",
				pk, std::sqrt(rms / (L.size() - n0)), mt, hg, eg);
			mt = 0; hg = 0; eg = 0;
		};
		{
			int n0 = (int) L.size();
			for (int n = 0; n < (int)(8 * FS); n++) { float l, r; music(n0 + n, l, r); L.push_back(l); R.push_back(r); }
		}
		Settings a; a.disc = 2; a.speed = 1; a.sideMin = 7; a.cutLevelDb = 6; a.needle = 0;
		run(a, 8);
		Settings b; b.disc = 2; b.speed = 0; b.sideMin = 22; b.needle = 1; b.phoType = 0; b.drive = 1; b.wow = 0.4f;
		run(b, 8);
		Settings c; c.disc = 1; c.speed = 1; c.sideMin = 6; c.cutLevelDb = 6; c.medType = 2; c.wear = 40; c.needle = 0.6f;
		c.phoType = 2; c.force = 3; c.eccMm = 0.3f;
		run(c, 8);
		writeWav(dir + "/vinylizer-demo.wav", L, R);
		std::printf("  wrote %s/vinylizer-demo.wav (dry, 12in 45, LP 33 at label, worn dub plate)\n", dir.c_str());
	}
	std::printf(fails ? "%d check(s) FAILED\n" : "all checks passed\n", fails);
	return fails ? 1 : 0;
}
