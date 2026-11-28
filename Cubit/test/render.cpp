// Offline renders for the spec section 12 checks. Not part of the plugin.
//   g++ -O2 -std=c++17 -I../src render.cpp -o render && ./render <outdir> && python3 check.py <outdir>
// Each check writes float32 raw files (48 kHz unless named otherwise); check.py analyses them.
#include "CubitDSP.hpp"
#include "CubitRom.hpp"
#include <chrono>
#include <cstdio>
#include <functional>
#include <string>
using namespace cubit;

static std::string DIR;
static const DecimFilter HB;

struct Outs { std::vector<float> out, uni, sub, t[4], clk; };

// One voice driven like the module drives it: controls -> frame -> voice -> volts.
struct Sim {
	double fs;
	Voice v;
	Controls k;
	VoiceCv cv;
	const uint8_t* rom = FACTORY_ROM;
	EdgeDetect syncEd, clkEd;
	bool ext = false;
	Sim(double fs = 48000, uint64_t seed = 1) : fs(fs) { v.setSeed(seed); v.setSampleRate(fs, &HB); k.drift = 0; }
	// per-sample hook gets the sample index and may set sync / clk voltages
	Outs run(size_t n, std::function<void(size_t, float& sync, float& clk)> hook = nullptr) {
		Outs o;
		for (auto* x : {&o.out, &o.uni, &o.sub, &o.t[0], &o.t[1], &o.t[2], &o.t[3], &o.clk}) x->resize(n);
		for (size_t i = 0; i < n; i++) {
			float sy = 0, cl = 0;
			if (hook) hook(i, sy, cl);
			Frame f;
			f.path = makePath(k, cv);
			f.rate = clockRate(k, cv);
			f.drift = k.drift;
			f.syncT = syncEd.process(sy);
			f.ext = ext;
			if (ext) { f.extT = clkEd.process(cl); f.extHigh = clkEd.high; }
			Result r = v.process(rom, f);
			o.out[i] = (float)outVolts(r.v[CH_DAC], f.path);
			o.uni[i] = (float)uniVolts(r.v[CH_DAC]);
			o.sub[i] = (float)r.v[CH_SUB];
			for (int j = 0; j < 4; j++) o.t[j][i] = (float)r.v[CH_T1 + j];
			o.clk[i] = (float)r.v[CH_CLK];
		}
		return o;
	}
};

static void put(const std::string& name, const std::vector<float>& x) {
	FILE* fp = std::fopen((DIR + "/" + name + ".f32").c_str(), "wb");
	if (!fp) { std::fprintf(stderr, "cannot write %s\n", name.c_str()); return; }
	std::fwrite(x.data(), 4, x.size(), fp);
	std::fclose(fp);
}

static float voctFor(double hz) { return (float)std::log2(hz / C4_HZ); }

int main(int argc, char** argv) {
	DIR = argc > 1 ? argv[1] : ".";
	const size_t S = 48000;

	// 1. pitch
	{ Sim s; auto o = s.run(2 * S); put("c1_out_0v", o.out); put("c1_sub_0v", o.sub); }
	{ Sim s; s.cv.voct = 1; put("c1_out_1v", s.run(2 * S).out); }
	{ Sim s; s.cv.voct = -2; put("c1_clk_m2v", s.run(2 * S).clk); }

	// 2. levels: wave 3-0-0 at 20 Hz
	{ Sim s; s.k.A = 3; s.cv.voct = voctFor(20); auto o = s.run(S); put("c2_out", o.out); put("c2_uni", o.uni); }
	{ Sim s(96000); s.k.A = 3; s.cv.voct = voctFor(20); auto o = s.run(2 * S); put("c2_out_96k", o.out); put("c2_uni_96k", o.uni); }

	// 3 / 4. spectra of 0-0-0
	for (int hz : {110, 1000, 5000, 13000}) {
		Sim s; s.cv.voct = voctFor(hz);
		put("c34_out_" + std::to_string(hz), s.run(2 * S).out);
	}

	// 5. sync: 100 Hz square into SYNC, note 150 Hz, wave 3-0-0
	{
		Sim s; s.k.A = 3; s.cv.voct = voctFor(150);
		auto o = s.run(2 * S, [&](size_t i, float& sy, float&) {
			// band-limited-style square: each edge is a 3-sample linear ramp placed at its exact time,
			// as a Rack VCO's square would carry it (a naive square has no sub-sample timing to recover)
			double t = i * 100.0 / 48000.0 + 0.123;
			double ph = t - std::floor(t);
			double r = ph * 480.0;
			if (r > 240) r -= 480;                       // samples from the nearest rising edge
			double fd = (ph - 0.5) * 480.0;              // samples from the falling edge
			double v = std::fabs(r) < 1.5 ? r / 1.5 : std::fabs(fd) < 1.5 ? -fd / 1.5 : (ph < 0.5 ? 1.0 : -1.0);
			sy = (float)(5 * v);
		});
		put("c5_out", o.out);
	}

	// 6. external clock: single pulses, then a 6 kHz train
	{
		Sim s; s.ext = true;
		size_t per = 100;   // one pulse every 100 samples, 64 + 10 pulses
		auto o = s.run(per * 76, [&](size_t i, float&, float& cl) { cl = (i % per) < 10 && i >= per / 2 ? 5.f : 0.f; });
		put("c6_uni_pulses", o.uni);
	}
	{
		Sim s; s.ext = true;
		auto o = s.run(2 * S, [&](size_t i, float&, float& cl) { cl = (i % 8) < 4 ? 5.f : 0.f; });   // 6 kHz
		put("c6_out_6k", o.out);
	}

	// 7. taps on C7 B0 A0, one bar at 32 steps/s through the internal clock (RANGE LO, 0.5 Hz cycle)
	{
		Sim s; s.k.C = 7; s.k.rangeLo = true; s.cv.voct = voctFor(0.5 * 256);
		auto o = s.run(2 * S);
		for (int j = 0; j < 4; j++) put("c7_tap" + std::to_string(j + 1), o.t[j]);
	}

	// 8. tuning: C7 B7 A2, UNI steps
	{
		Sim s; s.k.C = 7; s.k.B = 7; s.k.A = 2; s.k.rangeLo = true; s.cv.voct = voctFor(0.5 * 256);
		put("c8_uni", s.run(4 * S).uni);
	}

	// 9. splice / res / depth at 20 Hz
	{
		Sim s; s.cv.voct = voctFor(20); s.k.splice = 1; s.k.with = 0;
		put("c9_splice_uni", s.run(S).uni);
		Sim r; r.cv.voct = voctFor(20); r.k.res = 3;
		put("c9_res8_uni", r.run(S).uni);
		Sim d; d.cv.voct = voctFor(20); d.k.depth = 7;
		put("c9_depth1_out", d.run(S).out);
		// expected byte sequences for check.py
		std::vector<float> w0(64), w1(64);
		for (int i = 0; i < 64; i++) { w0[i] = FACTORY_ROM[i]; w1[i] = FACTORY_ROM[64 + i]; }
		put("c9_wave0", w0); put("c9_wave1", w1);
	}

	// 10. ROM round trip and WAV load
	{
		std::string p = DIR + "/c10_export.bin";
		bool ok = saveRom(p, FACTORY_ROM);
		std::vector<uint8_t> back(32768, 0);
		ok = ok && loadRom(p, 64, NORM_FILE, back.data());
		bool same = ok && !std::memcmp(back.data(), FACTORY_ROM, 32768);
		// 16-bit WAV of 32768 samples: a ramp across the whole file, normalisation off
		std::string wp = DIR + "/c10_in.wav";
		FILE* fp = std::fopen(wp.c_str(), "wb");
		auto w32 = [&](uint32_t v) { uint8_t b[4] = {(uint8_t)v, (uint8_t)(v >> 8), (uint8_t)(v >> 16), (uint8_t)(v >> 24)}; std::fwrite(b, 1, 4, fp); };
		auto w16 = [&](uint16_t v) { uint8_t b[2] = {(uint8_t)v, (uint8_t)(v >> 8)}; std::fwrite(b, 1, 2, fp); };
		std::fwrite("RIFF", 1, 4, fp); w32(36 + 65536); std::fwrite("WAVE", 1, 4, fp);
		std::fwrite("fmt ", 1, 4, fp); w32(16); w16(1); w16(1); w32(44100); w32(88200); w16(2); w16(16);
		std::fwrite("data", 1, 4, fp); w32(65536);
		std::vector<int16_t> pcm(32768);
		for (int i = 0; i < 32768; i++) pcm[i] = (int16_t)std::lround(std::sin(i * 0.001) * 30000);
		std::fwrite(pcm.data(), 2, 32768, fp);
		std::fclose(fp);
		std::vector<uint8_t> wr(32768, 0);
		bool wok = loadRom(wp, 64, NORM_OFF, wr.data());
		int bad = 0;
		for (int i = 0; i < 32768; i++) {
			double x = pcm[i] / 32768.0;
			int e = (int)std::max(0L, std::min(255L, std::lround((x * 0.5 + 0.5) * 255)));
			if (wr[i] != e) bad++;
		}
		// md5 is checked by check.py on the embedded array
		FILE* rf = std::fopen((DIR + "/c10_factory.bin").c_str(), "wb");
		std::fwrite(FACTORY_ROM, 1, 32768, rf); std::fclose(rf);
		std::printf("c10 export/load round trip identical: %d; wav load ok %d, mismatched bytes %d\n", (int)same, (int)wok, bad);
	}

	// 11. 16 voices at 2 kHz, all outputs; through-zero with +-10 V at 1 kHz into LIN FM
	{
		const int NV = 16;
		std::vector<Sim> v(NV);
		for (int i = 0; i < NV; i++) { v[i].v.setSeed(i + 1); v[i].k.drift = 0.1f; v[i].cv.voct = voctFor(2000); v[i].k.A = i % 8; }
		auto t0 = std::chrono::steady_clock::now();
		bool nan = false;
		const size_t n = 10 * S;
		for (size_t i = 0; i < n; i++)
			for (int c = 0; c < NV; c++) {
				Frame f; f.path = makePath(v[c].k, v[c].cv); f.rate = clockRate(v[c].k, v[c].cv); f.drift = v[c].k.drift;
				Result r = v[c].v.process(FACTORY_ROM, f);
				for (double x : r.v) if (!std::isfinite(x)) nan = true;
			}
		double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
		std::printf("c11 16 voices at 2 kHz, all 7 channels: %.1f %% of one core at 48 kHz, NaN %d\n", 100 * secs / 10.0, (int)nan);
		Sim s; s.k.throughZero = true; s.k.lin = 1; s.cv.voct = voctFor(1000);
		double mx = 0; bool nan2 = false;
		Outs o;
		for (int rep = 0; rep < 1; rep++) {
			o = s.run(2 * S, [&](size_t i, float&, float&) { s.cv.linFm = (float)(10 * std::sin(2 * M_PI * 1000 * i / 48000.0)); });
		}
		for (float x : o.out) { if (!std::isfinite(x)) nan2 = true; mx = std::max(mx, (double)std::fabs(x)); }
		std::printf("c11 through-zero +-10 V lin FM: peak %.2f V, NaN %d\n", mx, (int)nan2);
		put("c11_tz_out", o.out);
	}

	// 12. drift
	{
		Sim s(48000, 7); s.k.drift = 1; s.cv.voct = voctFor(100); s.k.A = 3;
		put("c12_drift1", s.run(60 * S).out);
		Sim a(48000, 1), b(48000, 99); a.cv.voct = b.cv.voct = voctFor(330); a.k.A = b.k.A = 3;
		auto oa = a.run(10 * S), ob = b.run(10 * S);
		double diff = 0;
		for (size_t i = 0; i < oa.out.size(); i++) diff = std::max(diff, (double)std::fabs(oa.out[i] - ob.out[i]));
		std::printf("c12 DRIFT 0 two instances, max difference %g V (want 0)\n", diff);
	}
	// mutation (spec 9): audio and patterns never cross; DEPTH never drops below 2 bits
	{
		Rng g; g.seed(5);
		int cross = 0, depthLow = 0, n = 0;
		for (int c0 = 0; c0 < 8; c0++)
			for (int i = 0; i < 2000; i++, n++) {
				Controls k; k.C = c0; k.depth = 3;
				k.mut.draw(1.f, c0, [&]() { return g.uni(); });
				VoiceCv cv;
				Path p = makePath(k, cv);
				if ((c0 == 7) != (p.wave / 64 == 7)) cross++;
				if (p.depthBits < 2) depthLow++;
			}
		std::printf("c13 mutation over %d draws at depth 1: audio/pattern crossings %d, depth below 2 bits %d (want 0, 0)\n", n, cross, depthLow);
	}
	return 0;
}
