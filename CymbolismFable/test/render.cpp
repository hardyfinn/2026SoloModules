// Cymbolism Fable offline renders and checks (no Rack).  make test
#include "../src/CyEngine.hpp"
#include <chrono>
#include <cstdio>
#include <cstring>
#include <string>
#include <functional>
#include <memory>
using namespace cy;

static void writeWav(const std::string& path, const std::vector<float>& x, int sr) {
	FILE* fp = fopen(path.c_str(), "wb");
	if (!fp) return;
	int n = (int)x.size();
	int bytes = n * 3;
	auto w32 = [&](uint32_t v) { fwrite(&v, 4, 1, fp); };
	auto w16 = [&](uint16_t v) { fwrite(&v, 2, 1, fp); };
	fwrite("RIFF", 1, 4, fp); w32(36 + bytes); fwrite("WAVE", 1, 4, fp);
	fwrite("fmt ", 1, 4, fp); w32(16); w16(1); w16(1); w32(sr); w32(sr * 3); w16(3); w16(24);
	fwrite("data", 1, 4, fp); w32(bytes);
	for (float v : x) {
		double s = std::max(-1.0, std::min(1.0, (double)v / 10.0));
		int32_t iv = (int32_t)std::lround(s * 8388607.0);
		unsigned char b[3] = {(unsigned char)(iv & 255), (unsigned char)((iv >> 8) & 255), (unsigned char)((iv >> 16) & 255)};
		fwrite(b, 1, 3, fp);
	}
	fclose(fp);
}

struct Hit { double t, v; };

static std::vector<float> renderHits(Engine& E, const std::vector<Hit>& hits, double dur, double chokeAt = -1) {
	int sr = (int)E.fsHost;
	int n = (int)(dur * sr);
	std::vector<float> out(n);
	size_t h = 0;
	const int B = 32;
	for (int s = 0; s < n; s += B) {
		double t = (double)s / sr;
		while (h < hits.size() && hits[h].t <= t) { E.strike(hits[h].v); h++; }
		E.ctl.choke = chokeAt >= 0 && t >= chokeAt;
		int m = std::min(B, n - s);
		E.process(nullptr, out.data() + s, m);
	}
	return out;
}

// a scripted performance: called once per 32-sample block before processing
typedef std::function<void(Engine&, double t)> Script;
static std::vector<float> perform(int type, double dur, Script sc, Controls base, int quality = 1) {
	GeomParams gp;
	gp.fromType(type);
	gp.quality = quality;
	gp.sampleRate = 48000;
	auto M = buildModel(gp);
	Engine E;
	E.ctl = base;
	E.init(M, 48000);
	int n = (int)(dur * 48000);
	std::vector<float> out(n);
	auto t0 = std::chrono::steady_clock::now();
	for (int s = 0; s < n; s += 32) {
		sc(E, (double)s / 48000);
		E.process(nullptr, out.data() + s, std::min(32, n - s));
	}
	double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
	float pk = 0;
	for (float v : out) pk = std::max(pk, std::fabs(v));
	printf("  %-11s %5.1f s  %.1fx real time  peak %.2f V  guard %d\n", CY_TYPES[type].name, dur, dur / secs, pk, E.guardEvents);
	return out;
}

static Controls defaults(int type) {
	Controls c;
	c.play = CY_TYPES[type].play;
	c.position = CY_TYPES[type].position;
	c.mic = 1.0;
	return c;
}

// one-shot strikes at given times
static Script hitsAt(std::vector<Hit> hits) {
	auto idx = std::make_shared<size_t>(0);
	return [hits, idx](Engine& E, double t) {
		while (*idx < hits.size() && hits[*idx].t <= t) { E.strike(hits[*idx].v); (*idx)++; }
	};
}

static void append(std::vector<float>& a, const std::vector<float>& b) { a.insert(a.end(), b.begin(), b.end()); }

int main(int argc, char** argv) {
	std::string dir = argc > 1 ? argv[1] : ".";
	std::string outDir = dir + "/test/out";
	std::string mk = "mkdir -p " + outDir;
	if (system(mk.c_str()) != 0) return 1;
	const int SR = 48000;
	bool quick = argc > 2 && !strcmp(argv[2], "quick");
	double L = quick ? 0.4 : 1.0;   // length scale

	// gallery: every type, a soft hit then a hard hit
	printf("gallery\n");
	std::vector<float> gal;
	for (int t = 0; t < NUM_TYPES; t++) {
		double d = (t == 4 || t == 5 || t == 9 ? 9.0 : 6.5) * L;
		append(gal, perform(t, d, hitsAt({{0.02, 1.0}, {d * 0.4, 7.0}}), defaults(t)));
	}
	writeWav(outDir + "/gallery.wav", gal, SR);

	printf("ride pattern with bell accents\n");
	{
		Controls c = defaults(4);
		double bpm = 150, beat = 60 / bpm;
		auto st = std::make_shared<int>(0);
		auto w = perform(4, 12 * L, [=](Engine& E, double t) {
			// swing ride: 1, 2 &a, 3, 4 &a ... with the bell on every fourth bar's 1
			int bar = (int)(t / (4 * beat));
			double tb = std::fmod(t, 4 * beat) / beat;
			const double pat[6] = {0, 1, 1.67, 2, 3, 3.67};
			for (int k = 0; k < 6; k++) {
				int id = bar * 6 + k;
				if (id >= *st && tb >= pat[k] && tb < pat[k] + 0.05) {
					*st = id + 1;
					bool bell = (k == 0 && bar % 4 == 3);
					E.ctl.position = bell ? 0.06 : 0.55;
					E.strike(bell ? 4.0 : (k == 0 || k == 3 ? 2.6 : 1.6));
				}
			}
		}, c);
		writeWav(outDir + "/ride-pattern.wav", w, SR);
	}

	printf("crash then choke\n");
	{
		auto w = perform(1, 6 * L, [=](Engine& E, double t) {
			static bool hit = false;
			if (t < 0.01) hit = false;
			if (!hit) { E.strike(6.0); hit = true; }
			E.ctl.choke = t > 1.6 * L && t < 2.3 * L;
		}, defaults(1));
		writeWav(outDir + "/crash-choke.wav", w, SR);
	}

	printf("mallet swell\n");
	{
		Controls c = defaults(5);
		c.play = MALLET; c.hardness = 0.35; c.rate = 16; c.position = 0.9; c.mic = 3.0;
		double d = 10 * L;
		auto w = perform(5, d, [=](Engine& E, double t) { E.ctl.roll = t < d - 3; E.ctl.force = 0.3 + 5.0 * t / d; }, c);
		writeWav(outDir + "/mallet-swell.wav", w, SR);
	}

	printf("brush sweep and taps\n");
	{
		Controls c = defaults(3);
		c.play = BRUSH; c.rate = 6; c.force = 2.5; c.position = 0.7;
		double d = 8 * L;
		auto st = std::make_shared<int>(0);
		auto w = perform(3, d, [=](Engine& E, double t) {
			E.ctl.roll = std::fmod(t, 2.0) < 1.2;
			int k = (int)(t / 0.5);
			if (k >= *st && std::fmod(t, 2.0) > 1.3) { *st = k + 1; E.strike(2.0); }
		}, c);
		writeWav(outDir + "/brush.wav", w, SR);
	}

	printf("bowed china and bowed bell\n");
	{
		Controls c = defaults(7);
		c.play = BOW; c.force = 3; c.rate = 6;
		double d = 8 * L;
		auto w = perform(7, d, [=](Engine& E, double t) { E.ctl.roll = t < d - 2.5; E.ctl.rate = 5 + 3 * std::sin(t); }, c);
		Controls b = defaults(9);
		b.play = BOW; b.force = 3; b.rate = 6;
		append(w, perform(9, d, [=](Engine& E, double t) { E.ctl.roll = t < d - 2.5; }, b));
		writeWav(outDir + "/bowed.wav", w, SR);
	}

	printf("rods on a splash\n");
	{
		Controls c = defaults(0);
		c.play = RODS;
		auto w = perform(0, 5 * L, hitsAt({{0.02, 2.0}, {0.6, 3.0}, {1.1, 2.0}, {1.35, 2.0}, {1.6, 5.0}}), c);
		writeWav(outDir + "/rods-splash.wav", w, SR);
	}

	printf("sizzle ride\n");
	{
		Controls c = defaults(4);
		c.sizzle = 0.5;
		auto w = perform(4, 8 * L, hitsAt({{0.02, 2.5}, {2.0, 2.0}, {3.0, 4.0}}), c);
		writeWav(outDir + "/sizzle-ride.wav", w, SR);
	}

	printf("bloom 0, 1, 2 on the same crash hit\n");
	{
		std::vector<float> w;
		for (double b : {0.0, 1.0, 2.0}) {
			Controls c = defaults(1);
			c.bloom = b;
			append(w, perform(1, 4 * L, hitsAt({{0.02, 6.0}}), c));
		}
		writeWav(outDir + "/bloom-sweep.wav", w, SR);
	}

	printf("drum loop through EXCITE\n");
	{
		// synthesised kick and snare, pushed into a china
		GeomParams gp;
		gp.fromType(7);
		gp.sampleRate = SR;
		auto M = buildModel(gp);
		Engine E;
		E.ctl = defaults(7);
		E.ctl.excite = 0.8;
		E.ctl.force = 3.0;
		E.init(M, SR);
		int n = (int)(8 * L * SR);
		std::vector<float> exc(n), out(n);
		CyRng r;
		double beat = 60.0 / 110.0;
		for (int i = 0; i < n; i++) {
			double t = (double)i / SR, tb = std::fmod(t, 2 * beat);
			double kick = tb < 0.3 ? std::sin(2 * CY_PI * (50 * tb + 300 * (1 - std::exp(-tb * 30)) / 30)) * std::exp(-tb * 12) : 0;
			double ts = std::fmod(t + beat, 2 * beat);
			double snare = ts < 0.25 ? (0.6 * r.bi() + 0.4 * std::sin(2 * CY_PI * 190 * ts)) * std::exp(-ts * 20) : 0;
			exc[i] = (float)(4.0 * (kick + snare));
		}
		for (int s = 0; s < n; s += 32) E.process(exc.data() + s, out.data() + s, std::min(32, n - s));
		std::vector<float> mix(n);
		for (int i = 0; i < n; i++) mix[i] = 0.5f * exc[i] + out[i];
		writeWav(outDir + "/excite-drums.wav", mix, SR);
		float pk = 0;
		for (float v : out) pk = std::max(pk, std::fabs(v));
		printf("  CHINA excite peak %.2f V\n", pk);
	}
	return 0;
}
