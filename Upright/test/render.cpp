// Offline checks for the Upright sample player + a demo WAV. No Rack needed.
//   make test      (prints the checks, writes test/out/*.wav)
#include "../src/UprightEngine.hpp"
#include <cstdio>
#include <chrono>
#include <memory>

using namespace upright;
static float SR = 48000.f;
static int fails = 0;
#define CHECK(c, ...) do { if (!(c)) { std::printf("FAIL: "); fails++; } else std::printf("ok:   "); std::printf(__VA_ARGS__); std::printf("\n"); } while (0)

struct Ev { float t; float key; float vel; float dur; };
struct Ped { float t; bool on; };

static std::vector<float> play(const Bank& b, const std::vector<Ev>& evs, const std::vector<Ped>& ped, float secs, Controls c = Controls()) {
	std::unique_ptr<Engine> e(new Engine());
	e->bank = &b;
	e->setSampleRate(SR);
	std::vector<float> out;
	int n = (int)(secs * SR);
	for (int s = 0; s < n; s++) {
		for (auto& p : ped) if ((int)(p.t * SR) == s) { c.pedal = p.on; e->setPedal(p.on, c); }
		for (size_t i = 0; i < evs.size(); i++) {
			if ((int)((evs[i].t + evs[i].dur) * SR) == s) e->noteOff((int)i, c);
			if ((int)(evs[i].t * SR) == s) e->noteOn((int)i, evs[i].key, evs[i].vel, c);
		}
		e->process(c);
		out.push_back(e->outL); out.push_back(e->outR);
	}
	return out;
}
static float rmsDb(const std::vector<float>& x, float a, float b) {
	int ia = 2 * (int)(a * SR), ib = std::min(2 * (int)(b * SR), (int)x.size());
	double s = 0; for (int i = ia; i < ib; i++) s += x[i] * x[i];
	return 10.f * std::log10(s / std::max(1, ib - ia) + 1e-20);
}
static float peak(const std::vector<float>& x) { float p = 0; for (float v : x) p = std::max(p, std::fabs(v)); return p; }
static void writeWav(const std::string& path, const std::vector<float>& x, float gain) {
	FILE* f = std::fopen(path.c_str(), "wb");
	if (!f) return;
	uint32_t bytes = x.size() * 2;
	auto w32 = [&](uint32_t v) { std::fwrite(&v, 4, 1, f); };
	auto w16 = [&](uint16_t v) { std::fwrite(&v, 2, 1, f); };
	std::fwrite("RIFF", 1, 4, f); w32(36 + bytes); std::fwrite("WAVEfmt ", 1, 8, f);
	w32(16); w16(1); w16(2); w32((uint32_t)SR); w32((uint32_t)SR * 4); w16(4); w16(16);
	std::fwrite("data", 1, 4, f); w32(bytes);
	for (float v : x) { int16_t s = (int16_t)std::max(-32767.f, std::min(32767.f, v * gain * 32767.f)); std::fwrite(&s, 2, 1, f); }
	std::fclose(f);
}

int main(int argc, char** argv) {
	std::string out = argc > 1 ? argv[1] : "test/out";
	Bank bank;
	auto t0 = std::chrono::high_resolution_clock::now();
	bool ok = bank.loadAll("res/samples");
	double loadS = std::chrono::duration<double>(std::chrono::high_resolution_clock::now() - t0).count();
	bank.state = ok ? 2 : 3;
	size_t bytes = 0;
	for (auto& a : bank.s) for (auto& b : a) for (auto& c : b) bytes += c.data.size() * 2;
	CHECK(bank.loaded == 79, "all 79 samples decoded (%d) in %.1f s, %.0f MB in memory", bank.loaded, loadS, bytes / 1e6);
	std::printf("      layer loudness pp %.1f, mf %.1f, f %.1f dB\n", bank.layerDb[0], bank.layerDb[1], bank.layerDb[2]);

	// every key A0..C8 sounds
	{
		int silent = 0;
		for (int k = 21; k <= 108; k++) {
			auto x = play(bank, {{0.01f, (float)k, 0.6f, 0.3f}}, {}, 0.35f);
			if (rmsDb(x, 0.f, 0.3f) < -60.f) { silent++; std::printf("      silent key %d\n", k); }
		}
		CHECK(silent == 0, "all 88 keys sound (%d silent)", silent);
	}
	// loudness rises smoothly with velocity, through both layer crossfades
	{
		float prev = -200.f; int bad = 0; float worstJump = 0.f;
		std::printf("      C4 velocity -> level (dB):");
		for (int i = 1; i <= 40; i++) {
			float v = i / 40.f;
			auto x = play(bank, {{0.01f, 60.f, v, 0.5f}}, {}, 0.4f);
			float d = rmsDb(x, 0.f, 0.3f);
			if (i % 4 == 0) std::printf(" %.2f:%.0f", v, d);
			if (d < prev - 0.5f) bad++;
			if (i > 1) worstJump = std::max(worstJump, d - prev);
			prev = d;
		}
		std::printf("\n");
		CHECK(bad == 0, "C4 gets louder with every velocity step (%d steps went down)", bad);
		CHECK(worstJump < 4.f, "no sudden jumps between steps (largest %.1f dB per 1/40)", worstJump);
	}
	// sustain pedal keeps notes ringing after the gate ends; without it they stop
	{
		auto a = play(bank, {{0.01f, 48.f, 0.6f, 0.3f}}, {}, 2.f);
		auto b = play(bank, {{0.01f, 48.f, 0.6f, 0.3f}}, {{0.f, true}}, 2.f);
		CHECK(rmsDb(a, 1.5f, 2.f) < rmsDb(b, 1.5f, 2.f) - 30.f, "damper stops C3 (%.0f dB) but the pedal keeps it (%.0f dB)", rmsDb(a, 1.5f, 2.f), rmsDb(b, 1.5f, 2.f));
	}
	// the demo phrase
	std::vector<Ev> ev; std::vector<Ped> ped;
	{
		float b = 60.f / 84.f;
		auto chord = [&](float beat, std::vector<int> ns, float vel, float len) {
			for (size_t i = 0; i < ns.size(); i++) ev.push_back({beat * b + 0.008f * i, (float)ns[i], vel * (0.92f + 0.05f * (i % 3)), len * b});
		};
		auto note = [&](float beat, int n, float vel, float len) { ev.push_back({beat * b, (float)n, vel, len * b}); };
		chord(0, {38, 45}, 0.55f, 3.8f); chord(0.02f, {65, 69, 72, 76}, 0.45f, 1.8f);
		note(2, 77, 0.6f, 0.5f); note(2.5f, 76, 0.5f, 0.5f); note(3, 74, 0.55f, 1.f);
		chord(4, {31, 41}, 0.6f, 3.8f); chord(4.02f, {59, 64, 65, 69}, 0.48f, 1.8f);
		note(6, 71, 0.62f, 0.5f); note(6.5f, 74, 0.55f, 0.5f); note(7, 79, 0.7f, 1.f);
		chord(8, {36, 43, 52}, 0.6f, 7.5f); chord(8.03f, {62, 64, 67, 71}, 0.5f, 3.5f);
		int run[] = {84, 83, 79, 76, 74, 72, 71, 67};
		for (int i = 0; i < 8; i++) note(12 + 0.25f * i, run[i], 0.42f + 0.03f * (i % 2), 0.3f);
		chord(14, {60, 64, 67, 71, 74}, 0.38f, 3.f);
		chord(18, {33, 40}, 0.85f, 1.8f); chord(18.02f, {60, 64, 67, 69}, 0.8f, 1.8f);
		chord(20, {38, 45}, 0.9f, 3.8f); chord(20.02f, {62, 65, 69, 72}, 0.82f, 3.8f);
		ped = {{0.f, true}, {4 * b - 0.05f, false}, {4 * b + 0.05f, true}, {8 * b - 0.05f, false}, {8 * b + 0.05f, true}, {17 * b, false},
		       {18 * b + 0.05f, true}, {20 * b - 0.05f, false}, {20 * b + 0.05f, true}, {24 * b, false}};
		auto x = play(bank, ev, ped, 24 * b + 2.5f);
		float p = peak(x);
		std::printf("      phrase peak %.2f (x5 = %.1f V in Rack)\n", p, p * 5.f);
		CHECK(p * 5.f < 10.f, "loud chords stay under 10 V (%.1f V)", p * 5.f);
		writeWav(out + "/upright-phrase.wav", x, 0.89f / p);
	}
	// CPU: a pedalled 16-note cluster
	{
		std::unique_ptr<Engine> e(new Engine());
		e->bank = &bank; e->setSampleRate(SR);
		Controls c; c.pedal = true; e->setPedal(true, c);
		for (int i = 0; i < 16; i++) e->noteOn(i, 40.f + 2 * i, 0.78f, c);   // on a crossfade: 2 voices each
		int n = (int)(2 * SR); float acc = 0;
		auto t1 = std::chrono::high_resolution_clock::now();
		for (int i = 0; i < n; i++) { e->process(c); acc += e->outL; }
		double s = std::chrono::duration<double>(std::chrono::high_resolution_clock::now() - t1).count();
		std::printf("      %d voices: 2 s of audio in %.3f s (acc %g)\n", e->activeVoices(), s, acc);
		CHECK(s / 2 < 0.05, "16 pedalled notes under 5%% of a core (%.1f%%)", 100 * s / 2);
	}
	std::printf("\n%s (%d failed)\n", fails ? "SOME CHECKS FAILED" : "all checks passed", fails);
	return fails ? 1 : 0;
}
