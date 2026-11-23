// Offline checks for the Gongoozler engine (no Rack needed):  make test
//  1. stability: hundreds of random settings and hits, output must stay finite and in range
//  2. bloom: a hard tam-tam hit must get brighter over the first second, a soft one must not
//  3. glide: an opera gong with GLIDE must bend and come back
//  4. speed: time per sample
//  5. writes the demo WAV (gongoozler-demo.wav) and one WAV per starting point
#include "../src/GongEngine.hpp"
#if defined(__SSE__)
#include <xmmintrin.h>
#include <pmmintrin.h>
#endif
#include <cstdio>
#include <vector>
#include <complex>
#include <chrono>
#include <string>

using namespace gong;
static const float FS = 48000.f;

static void writeWav(const char* path, const std::vector<float>& L, const std::vector<float>& R) {
	FILE* fp = fopen(path, "wb");
	if (!fp) return;
	uint32_t n = L.size(), bytes = n * 4;
	auto u32 = [&](uint32_t v) { fwrite(&v, 4, 1, fp); };
	auto u16 = [&](uint16_t v) { fwrite(&v, 2, 1, fp); };
	fwrite("RIFF", 1, 4, fp); u32(36 + bytes); fwrite("WAVEfmt ", 1, 8, fp);
	u32(16); u16(1); u16(2); u32((uint32_t) FS); u32((uint32_t) FS * 4); u16(4); u16(16);
	fwrite("data", 1, 4, fp); u32(bytes);
	for (uint32_t i = 0; i < n; i++) {
		int16_t a = (int16_t) std::lrint(std::max(-1.f, std::min(1.f, L[i] / 10.f)) * 32767.f);
		int16_t b = (int16_t) std::lrint(std::max(-1.f, std::min(1.f, R[i] / 10.f)) * 32767.f);
		fwrite(&a, 2, 1, fp); fwrite(&b, 2, 1, fp);
	}
	fclose(fp);
}

static void fft(std::vector<std::complex<float>>& a) {
	int n = a.size();
	for (int i = 1, j = 0; i < n; i++) {
		int bit = n >> 1;
		for (; j & bit; bit >>= 1) j ^= bit;
		j ^= bit;
		if (i < j) std::swap(a[i], a[j]);
	}
	for (int len = 2; len <= n; len <<= 1) {
		float ang = -2.f * PI / len;
		std::complex<float> wl(std::cos(ang), std::sin(ang));
		for (int i = 0; i < n; i += len) {
			std::complex<float> w(1.f);
			for (int j = 0; j < len / 2; j++) {
				auto u = a[i + j], t = a[i + j + len / 2] * w;
				a[i + j] = u + t; a[i + j + len / 2] = u - t;
				w *= wl;
			}
		}
	}
}

// spectral centroid (Hz) and level (dB) of a 4096-sample window at time t
static void analyse(const std::vector<float>& s, float t, float& centroid, float& db, float& hiShare) {
	const int W = 4096;
	int i0 = (int) (t * FS);
	std::vector<std::complex<float>> a(W);
	double ss = 0;
	for (int i = 0; i < W; i++) {
		float x = (i0 + i < (int) s.size()) ? s[i0 + i] : 0.f;
		ss += x * x;
		a[i] = x * (0.5f - 0.5f * std::cos(2 * PI * i / W));
	}
	fft(a);
	double num = 0, den = 0, hi = 0;
	for (int k = 1; k < W / 2; k++) {
		double p = std::norm(a[k]);
		double fk = k * FS / W;
		num += fk * p; den += p;
		if (fk > 2000) hi += p;
	}
	centroid = den > 0 ? num / den : 0;
	hiShare = den > 0 ? hi / den : 0;
	db = 10 * std::log10(ss / W + 1e-12);
}

struct Render {
	Engine eng;
	Controls c;
	std::vector<float> L, R;
	Render() { eng.setSampleRate(FS); }
	void run(float sec) {
		int n = (int) (sec * FS);
		for (int i = 0; i < n; i++) { eng.process(c); L.push_back(eng.outL); R.push_back(eng.outR); }
	}
};

static int fails = 0;
#define CHECK(cond, ...) do { if (!(cond)) { fails++; printf("FAIL: "); printf(__VA_ARGS__); printf("\n"); } } while (0)

static Controls preset(const std::string& name) {
	Controls c;
	if (name == "tamtam") { c.type = TAMTAM; c.size = 90; c.thick = 0.45f; c.beat = 0.4f; c.position = 0.4f; c.mallet = 0.75f; c.decay = 0.7f; c.damp = 0.35f; c.bloom = 0.7f; c.build = 0.5f; c.roar = 0.35f; }
	if (name == "wind") { c.type = WIND; c.size = 60; c.thick = 0.3f; c.beat = 0.4f; c.position = 0.55f; c.mallet = 0.55f; c.decay = 0.55f; c.damp = 0.3f; c.bloom = 0.85f; c.build = 0.3f; c.roar = 0.45f; }
	if (name == "operabig") { c.type = OPERA; c.size = 32; c.thick = 0.5f; c.glide = 2.5f; c.position = 0.1f; c.mallet = 0.4f; c.decay = 0.4f; c.damp = 0.45f; c.bloom = 0.3f; c.build = 0.3f; c.roar = 0.2f; c.tune = -7.f / 12.f; }
	if (name == "operasmall") { c.type = OPERA; c.size = 22; c.thick = 0.5f; c.glide = -2.5f; c.position = 0.1f; c.mallet = 0.2f; c.decay = 0.3f; c.damp = 0.4f; c.bloom = 0.2f; c.build = 0.3f; c.roar = 0.15f; c.tune = 7.f / 12.f; }
	if (name == "ageng") { c.type = BOSSED; c.size = 90; c.thick = 0.7f; c.beat = 0.45f; c.glide = 0.3f; c.position = 0.0f; c.mallet = 0.85f; c.decay = 0.8f; c.damp = 0.55f; c.bloom = 0.2f; c.build = 0.6f; c.roar = 0.15f; c.tune = -2.f; }
	if (name == "kempul") { c.type = BOSSED; c.size = 50; c.thick = 0.6f; c.beat = 0.3f; c.position = 0.0f; c.mallet = 0.7f; c.decay = 0.6f; c.damp = 0.5f; c.bloom = 0.15f; c.build = 0.5f; c.roar = 0.1f; c.tune = -1.f; }
	return c;
}

int main(int argc, char** argv) {
#if defined(__SSE__)
	// Rack flushes tiny numbers to zero; do the same here so long tails don't crawl
	_MM_SET_FLUSH_ZERO_MODE(_MM_FLUSH_ZERO_ON);
	_MM_SET_DENORMALS_ZERO_MODE(_MM_DENORMALS_ZERO_ON);
#endif
	std::string dir = argc > 1 ? argv[1] : ".";

	// ---- 1. stability
	{
		Rng g; g.seed(42);
		float worst = 0.f;
		for (int t = 0; t < 120; t++) {
			Render r;
			Controls& c = r.c;
			c.type = g.next() % NTYPES; c.size = 15 + 145 * g.uni(); c.thick = g.uni(); c.beat = g.uni();
			c.glide = 14 * g.uni() - 7; c.cast = g.next() % 16; c.position = g.uni(); c.mallet = g.uni();
			c.decay = g.uni(); c.damp = g.uni(); c.bloom = g.uni(); c.build = g.uni(); c.roar = g.uni();
			c.tune = 4 * g.uni() - 2; c.followPitch = g.next() & 1; c.mute = g.uni() * g.uni(); c.width = g.uni();
			for (int h = 0; h < 6; h++) {
				r.eng.trigger(1.5f * g.uni(), c);
				r.run(0.05f + 0.3f * g.uni());
			}
			r.run(1.0f);
			for (size_t i = 0; i < r.L.size(); i++) {
				float m = std::max(std::fabs(r.L[i]), std::fabs(r.R[i]));
				if (!std::isfinite(m)) { m = 1e9f; }
				worst = std::max(worst, m);
			}
			bool ok = true;
			for (int k = 0; k < N; k++) if (!std::isfinite(r.eng.x[k]) || std::fabs(r.eng.x[k]) > 100.f) ok = false;
			CHECK(ok, "state blew up in random patch %d (type %d)", t, c.type);
		}
		printf("stability: 120 random patches, loudest sample %.2f V\n", worst);
		CHECK(worst <= 10.01f, "output above 10 V");
	}

	// ---- 2. bloom
	{
		for (float vel : {1.0f, 0.15f}) {
			Render r;
			r.c = preset("tamtam");
			r.eng.trigger(vel, r.c);
			r.run(4.f);
			float c0, c1, d0, d1, h0, h1, cP = 0, tP = 0;
			analyse(r.L, 0.02f, c0, d0, h0);
			for (float t = 0.1f; t < 2.5f; t += 0.1f) {
				float cc, dd, hh;
				analyse(r.L, t, cc, dd, hh);
				if (cc > cP) { cP = cc; tP = t; }
			}
			analyse(r.L, 1.0f, c1, d1, h1);
			printf("tam-tam vel %.2f: centroid %5.0f Hz at 20 ms -> %5.0f Hz at 1 s (peak %5.0f Hz at %.1f s); >2k share %.2f -> %.2f; level %.1f -> %.1f dB\n",
			       vel, c0, c1, cP, tP, h0, h1, d0, d1);
			if (vel > 0.5f) CHECK(cP > 3.f * c0 && tP > 0.3f && h1 > 0.08f, "hard hit did not bloom");
			else CHECK(h1 < 0.05f, "soft hit bloomed too much");
		}
	}

	// ---- 3. glide
	{
		Render r;
		r.c = preset("operabig");
		r.eng.trigger(1.f, r.c);
		float p0 = 0, p1 = 0;
		r.run(0.03f); p0 = r.eng.pitchMul;
		r.run(1.5f); p1 = r.eng.pitchMul;
		float st0 = 12 * std::log2(p0), st1 = 12 * std::log2(p1);
		printf("opera gong GLIDE +2.5: pitch +%.2f st at 30 ms, +%.2f st at 1.5 s\n", st0, st1);
		CHECK(st0 > 1.5f && st1 < 0.3f * st0, "glide did not bend and return");
	}

	// ---- 4. speed
	{
		Render r;
		r.c = preset("tamtam");
		r.eng.trigger(1.f, r.c);
		auto t0 = std::chrono::steady_clock::now();
		int n = (int) (FS * 5);
		for (int i = 0; i < n; i++) r.eng.process(r.c);
		double us = std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - t0).count();
		printf("speed: %.3f us per sample (%.1f %% of one core at 48 kHz)\n", us / n, us / n * 48000 / 1e4);
	}

	// ---- 5. one WAV per starting point: soft, medium, hard hit
	if (argc > 2) {
		for (const char* p : {"tamtam", "wind", "operabig", "operasmall", "ageng", "kempul"}) {
			Render r;
			r.c = preset(p);
			for (float vel : {0.25f, 0.6f, 1.f}) {
				r.eng.trigger(vel, r.c);
				r.run(std::string(p).find("opera") == 0 ? 2.5f : 6.f);
			}
			writeWav((dir + "/hit-" + p + ".wav").c_str(), r.L, r.R);
		}
	}

	printf(fails ? "%d check(s) FAILED\n" : "all checks passed\n", fails);
	return fails ? 1 : 0;
}
