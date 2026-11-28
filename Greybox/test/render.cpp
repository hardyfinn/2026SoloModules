// Offline checks and demo renders for GreyboxDSP.hpp (spec section 12). Not part of the plugin.
//   g++ -O2 -std=c++17 -I../src render.cpp -o render
//   ./render checks <outdir>              spec 12 test signals (analyse with check.py)
//   ./render demo <src.wav> <out.wav> ... see main() for the demo recipe syntax
#include "GreyboxDSP.hpp"
#include <cstdio>
#include <cstdlib>
#include <string>
using namespace greybox;

static std::vector<float> run(const CaptureResult& c, double fs, Frame f, double secs) {
	Voice v; v.setSampleRate(fs); v.setMemory(c.codes.data(), c.codes.size(), c.fsRec, c.companded);
	std::vector<float> out((size_t)(secs * fs));
	for (size_t i = 0; i < out.size(); i++) {
		f.trig = i == 0;
		out[i] = v.process(f).out / 5.f;
	}
	return out;
}

static CaptureResult cap(const std::vector<float>& src, double fs, int model, float rate = 1.f, float gain = 1.f,
                         double stretch = 1.0, int dtime = 1000) {
	CaptureSettings s;
	s.model = model; s.fsRec = recRate(model, rate); s.gain = gain; s.stretch = stretch; s.dtime = dtime;
	s.nMax = 1440000;
	return capture(src, fs, s);
}

static void put(const std::string& p, const std::vector<float>& x, double fs) {
	if (!writeWavFloat(p.c_str(), x, fs)) std::fprintf(stderr, "could not write %s\n", p.c_str());
}

static int checks(const std::string& dir) {
	const double fsS = 96000;
	auto sine = [&](double amp, double hz, double secs) {
		std::vector<float> s((size_t)(secs * fsS));
		for (size_t i = 0; i < s.size(); i++) s[i] = (float)(amp * std::sin(2 * M_PI * hz * i / fsS));
		return s;
	};
	auto s1k = sine(0.5, 1000, 2.0);
	Frame f; f.model = M950; f.level = 1.f; f.decay = 1.f; f.filter = 99.f; f.noNoise = true;
	f.fsRecLive = recRate(M950, 1.f);
	auto c950 = cap(s1k, fsS, M950);
	std::printf("950 capture: %zu words at %.0f Hz\n", c950.codes.size(), c950.fsRec);
	// 12.1 / 12.2 / 12.3
	for (int t : {0, -12, 12}) {
		f.semis = (float)t;
		put(dir + "/c1_950_tune" + std::to_string(t) + ".wav", run(c950, 96000, f, 1.0), 96000);
	}
	f.semis = -12; f.follow = false;
	put(dir + "/c3_950_tune-12_nofollow.wav", run(c950, 96000, f, 1.0), 96000);
	f.follow = true;
	// 12.4: bright source (saw-ish: sum of harmonics of 1.1 kHz up to 40 kHz) at RATE 0.5
	{
		std::vector<float> br((size_t)(1.0 * fsS));
		for (size_t i = 0; i < br.size(); i++) {
			double v = 0;
			for (int k = 1; 1100.0 * k < 45000; k++) v += std::sin(2 * M_PI * 1100 * k * i / fsS) / k;
			br[i] = (float)(0.3 * v);
		}
		for (int m : {M900, M950}) {
			auto c = cap(br, fsS, m, 0.5f);
			Frame g = f; g.model = m; g.semis = 0; g.fsRecLive = c.fsRec;
			put(dir + "/c4_bright_rate05_" + std::string(m == M900 ? "900" : "950") + ".wav", run(c, 96000, g, 0.8), 96000);
			std::printf("rate 0.5 model %d: fs_rec %.0f\n", m, c.fsRec);
		}
	}
	// 12.5: stretch
	{
		auto c40 = cap(s1k, fsS, M950, (float)(std::log(40000.0 / 7500.0) / std::log(48000.0 / 7500.0)));
		auto cs = cap(s1k, fsS, M950, (float)(std::log(40000.0 / 7500.0) / std::log(48000.0 / 7500.0)), 1.f, 2.0, 1000);
		auto c1 = cap(s1k, fsS, M950, (float)(std::log(40000.0 / 7500.0) / std::log(48000.0 / 7500.0)), 1.f, 1.0, 1000);
		std::printf("stretch: plain %zu (fs %.1f), x2 %zu, x1 identical %d\n", c40.codes.size(), c40.fsRec, cs.codes.size(),
			(int)(c1.codes == c40.codes));
		Frame g = f; g.semis = 0; g.fsRecLive = cs.fsRec;
		put(dir + "/c5_stretch200.wav", run(cs, 96000, g, 2.0), 96000);
	}
	// 12.6: FILTER 10 whistle
	{
		Frame g = f; g.semis = 0; g.filter = 10.f; g.decay = 0.4f;
		put(dir + "/c6_filter10.wav", run(c950, 96000, g, 1.5), 96000);
	}
	// 12.7: ALT loop on a ramp
	{
		CaptureResult r; r.fsRec = 48000;
		for (int i = 0; i < 480; i++) r.codes.push_back((int16_t)(-1600 + i * 3200 / 480));
		Frame g = f; g.semis = -12; g.loop = LOOP_ALT; g.filter = 99.f;
		put(dir + "/c7_alt_ramp.wav", run(r, 96000, g, 0.2), 96000);
	}
	// 12.8: 60 at -40 and 0 dBFS, and at TUNE -12
	{
		Frame g = f; g.model = M60; g.fsRecLive = 40000;
		for (double a : {0.99, 0.0099}) {
			auto c = cap(sine(a, 1000, 2.0), fsS, M60);
			g.semis = 0;
			put(dir + "/c8_60_amp" + std::to_string((int)std::round(-20 * std::log10(a))) + ".wav", run(c, 96000, g, 1.0), 96000);
		}
		for (double a : {0.99, 0.0099}) {
			auto c = cap(sine(a, 1000, 2.0), fsS, M950);
			g.model = M950; g.fsRecLive = 48000; g.semis = 0;
			put(dir + "/c8_950_amp" + std::to_string((int)std::round(-20 * std::log10(a))) + ".wav", run(c, 96000, g, 1.0), 96000);
		}
		auto c = cap(s1k, fsS, M60);
		g.model = M60; g.fsRecLive = 40000; g.semis = -12; g.follow = true;
		put(dir + "/c8_60_tune-12.wav", run(c, 96000, g, 1.0), 96000);
	}
	// AGE 0 neutral across seeds (noise off: the base floor is the one non-neutral item)
	{
		Voice a, b; a.setSeed(1); b.setSeed(999);
		a.setSampleRate(48000); b.setSampleRate(48000);
		a.setMemory(c950.codes.data(), c950.codes.size(), c950.fsRec, false);
		b.setMemory(c950.codes.data(), c950.codes.size(), c950.fsRec, false);
		Frame g = f; g.decay = 0.5f; g.semis = -5;
		double diff = 0;
		for (int i = 0; i < 48000; i++) { g.trig = i == 0; diff += std::fabs(a.process(g).out - b.process(g).out); }
		std::printf("AGE 0 seed difference: %g (want 0)\n", diff);
	}
	return 0;
}

// demo <src.wav> <out.wav> <fsOut> then steps "model rate tune filter stretch dtime gain age follow loop secs"
// each step is one trigger of the pad with those settings; a 0.35 s gap follows each step.
static int demo(int argc, char** argv) {
	std::vector<float> src; double fs;
	if (!readWav(argv[2], src, fs, 64.0)) { std::fprintf(stderr, "read failed\n"); return 1; }
	std::string out = argv[3];
	double fsOut = std::atof(argv[4]);
	std::vector<float> all;
	for (int a = 5; a < argc; a++) {
		int model, follow, loop; float rate, tune, filter, gain, age, secs, env, decay; double stretch; int dtime;
		if (std::sscanf(argv[a], "%d %f %f %f %lf %d %f %f %d %d %f %f %f", &model, &rate, &tune, &filter, &stretch, &dtime,
		                &gain, &age, &follow, &loop, &secs, &env, &decay) != 13) {
			std::fprintf(stderr, "bad step: %s\n", argv[a]); return 1;
		}
		auto c = cap(src, fs, model, rate, gain, stretch, dtime);
		Frame f; f.model = model; f.semis = tune; f.filter = filter; f.age = age; f.follow = follow != 0;
		f.loop = loop; f.level = 0.8f; f.fsRecLive = c.fsRec; f.envAmt = env; f.decay = decay;
		Voice v; v.setSeed(7); v.setSampleRate(fsOut); v.setMemory(c.codes.data(), c.codes.size(), c.fsRec, c.companded);
		size_t n = (size_t)(secs * fsOut);
		for (size_t i = 0; i < n; i++) { f.trig = i == 0; all.push_back(v.process(f).out / 5.f); }
		all.insert(all.end(), (size_t)(0.35 * fsOut), 0.f);
		std::printf("step %d: model %d fs_rec %.0f words %zu clipped %d\n", a - 4, model, c.fsRec, c.codes.size(), (int)c.clipped);
	}
	put(out, all, fsOut);
	return 0;
}

int main(int argc, char** argv) {
	std::string mode = argc > 1 ? argv[1] : "checks";
	if (mode == "demo" && argc > 5) return demo(argc, argv);
	return checks(argc > 2 ? argv[2] : ".");
}
