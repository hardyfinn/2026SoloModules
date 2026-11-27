// Offline checks for the Perun engine + demo WAVs. No Rack needed.
//   make test   (prints the checks, writes test/out/*.wav)
#include "../src/PerunEngine.hpp"
#include "../src/PerunPresets.hpp"
#include <cstdio>
#include <string>
#include <vector>
#include <chrono>
#include <cstring>

using namespace perun;
static float SR = 48000.f;
static int fails = 0;
#define CHECK(c, ...) do { if (!(c)) { std::printf("FAIL: "); fails++; } else std::printf("ok:   "); std::printf(__VA_ARGS__); std::printf("\n"); } while (0)

static void writeWav(const std::string& path, const std::vector<float>& x, float gain = 0.1f) {
	FILE* f = std::fopen(path.c_str(), "wb");
	if (!f) return;
	int n = x.size(), sr = (int) SR, bytes = n * 2;
	auto w32 = [&](int v) { std::fwrite(&v, 4, 1, f); };
	auto w16 = [&](short v) { std::fwrite(&v, 2, 1, f); };
	std::fwrite("RIFF", 1, 4, f); w32(36 + bytes); std::fwrite("WAVEfmt ", 1, 8, f);
	w32(16); w16(1); w16(1); w32(sr); w32(sr * 2); w16(2); w16(16);
	std::fwrite("data", 1, 4, f); w32(bytes);
	for (float v : x) { float s = std::max(-1.f, std::min(1.f, v * gain)); w16((short) (s * 32767)); }
	std::fclose(f);
}

static Controls fromPreset(int i) {
	Controls c;
	valuesToControls(factoryPreset(i).v, c);
	c.level = 0.8f;
	return c;
}

static float peak(const std::vector<float>& x, int a = 0, int b = -1) {
	if (b < 0) b = x.size();
	float p = 0.f;
	for (int i = a; i < b; i++) p = std::max(p, std::fabs(x[i]));
	return p;
}

// 16-step pattern at bpm; pattern chars: x = hit, X = accent, . = rest
static std::vector<float> pattern(Engine& e, Controls c, const char* pat, float bpm, int bars, Controls* (*mod)(Controls&, int) = nullptr) {
	std::vector<float> out;
	int stepLen = (int) (SR * 60.f / bpm / 4.f);
	int n = (int) std::strlen(pat);
	for (int s = 0; s < 16 * bars; s++) {
		Controls cc = c;
		if (mod) mod(cc, s);
		char ch = pat[s % n];
		if (ch == 'x' || ch == 'X') e.hit(ch == 'X' ? 1.f : 0.f, cc);
		for (int i = 0; i < stepLen; i++) { e.process(cc); out.push_back(e.out); }
	}
	return out;
}

int main(int argc, char** argv) {
	std::string dir = argc > 1 ? argv[1] : "test/out";
	// 1. every algorithm makes sound, stays bounded and ends
	for (int a = 0; a < NUM_ALGOS; a++) {
		for (int m = 0; m < 3; m++) {
			Engine e; e.setSampleRate(SR);
			Controls c = fromPreset(0);
			c.algoA = a; c.morph = 0.f; c.mode = m; c.drive = 0.f; c.send = 0.f; c.click = 0.f;
			c.tuneHz = (a >= 7 && a != 9 && a != 10) ? 400.f : (a == 10 ? 261.63f : 120.f);
			c.decay = 0.45f;
			if (a == 10) c.p1 = 0.f;
			e.hit(0.f, c);
			std::vector<float> o;
			for (int i = 0; i < (int) (2.f * SR); i++) { e.process(c); o.push_back(e.out); }
			bool finite = true;
			for (float v : o) if (!std::isfinite(v)) finite = false;
			float p = peak(o, 0, (int) (0.3f * SR)), tail = peak(o, (int) (1.6f * SR));
			CHECK(finite && p > 0.3f && p < 10.01f && tail < 0.05f, "%-12s %-10s peak %.2f V tail %.4f", algoInfo(a).name, algoInfo(a).mode[m], p, tail);
		}
	}
	// 2. drone: DECAY at max keeps ringing
	{
		Engine e; Controls c = fromPreset(0); c.decay = 1.f; c.send = 0;
		e.hit(0, c);
		std::vector<float> o;
		for (int i = 0; i < (int) (3.f * SR); i++) { e.process(c); o.push_back(e.out); }
		CHECK(peak(o, (int) (2.5f * SR)) > 0.5f, "drone still sounding after 2.5 s (%.2f V)", peak(o, (int) (2.5f * SR)));
	}
	// 3. mutate moves, lock holds
	{
		Engine e; Controls c = fromPreset(3); c.mutate = 1.f;
		for (int i = 0; i < 40; i++) e.hit(0, c);
		float a = e.mutP1;
		CHECK(std::fabs(e.mutP1) + std::fabs(e.mutP2) + std::fabs(e.mutMorph) > 0.01f, "MUTATE wandered (P1 %+.2f P2 %+.2f morph %+.2f)", e.mutP1, e.mutP2, e.mutMorph);
		c.lock = true;
		for (int i = 0; i < 40; i++) e.hit(0, c);
		CHECK(e.mutP1 == a, "LOCK holds the offsets");
	}
	// 4. CPU
	{
		Engine e; Controls c = fromPreset(7); c.morph = 0.5f; c.decay = 0.9f;
		auto t0 = std::chrono::steady_clock::now();
		for (int k = 0; k < 4; k++) e.hit(0, c);
		for (int i = 0; i < (int) SR * 2; i++) e.process(c);
		double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
		CHECK(true, "worst case (4 hits x 2 algorithms, morph) %.1f%% of one core", ms / 2000.0 * 100.0);
	}

	// demos
	const char* pats[8] = {
		"X...x...X...x.x.", "X..x..x.X...x...", "X.......x..x....", "x.xX.x.xx.x.X.x.",
		"....X.......X..x", "....X.......X...", "xxXxxxXxxxXxxxXx", "X..x..x...x.X..."};
	std::vector<float> all;
	for (int p = 0; p < 8; p++) {
		Engine e; Controls c = fromPreset(p);
		std::vector<float> o = pattern(e, c, pats[p], 124.f, 4);
		char name[64];
		std::snprintf(name, sizeof(name), "/%d-%s.wav", p + 1, factoryPreset(p).name);
		std::string fn = dir + name;
		for (auto& ch : fn) if (ch == ' ') ch = '-';
		writeWav(fn, o);
		all.insert(all.end(), o.begin(), o.end());
	}
	// morph sweep: Fold kick -> Tom over 8 bars
	{
		Engine e; Controls c = fromPreset(0); c.algoB = 6;
		std::vector<float> o = pattern(e, c, "X...x...x...x...", 124.f, 8, [](Controls& cc, int s) -> Controls* { cc.morph = s / 127.f; return nullptr; });
		writeWav(dir + "/morph-kick-to-tom.wav", o);
	}
	// mutate: FM perc with MUTATE at 0.8 over 8 bars
	{
		Engine e; Controls c = fromPreset(3); c.mutate = 0.8f; c.wear = 0.3f;
		std::vector<float> o = pattern(e, c, "x.xx.x.xx.x.x.xx", 124.f, 8);
		writeWav(dir + "/mutate-fm-perc.wav", o);
	}
	// groove: three Peruns summed (kick, snare, hat)
	{
		Engine k, s, h; Controls ck = fromPreset(0), cs = fromPreset(4), ch = fromPreset(6);
		ch.level = 0.5f; cs.level = 0.7f;
		std::vector<float> o;
		int stepLen = (int) (SR * 60.f / 126.f / 4.f);
		const char* pk = "X...x...X...x..x"; const char* ps = "....X.......X..."; const char* ph = "x.X.x.Xxx.X.x.Xx";
		for (int st = 0; st < 16 * 8; st++) {
			int i16 = st % 16;
			if (pk[i16] != '.') k.hit(pk[i16] == 'X', ck);
			if (ps[i16] != '.') s.hit(ps[i16] == 'X', cs);
			if (ph[i16] != '.') h.hit(ph[i16] == 'X', ch);
			for (int i = 0; i < stepLen; i++) { k.process(ck); s.process(cs); h.process(ch); o.push_back(k.out + s.out + h.out); }
		}
		writeWav(dir + "/groove-three-peruns.wav", o, 0.07f);
	}
	std::printf("%s (%d failed)\n", fails ? "SOME CHECKS FAILED" : "all checks passed", fails);
	return fails ? 1 : 0;
}
