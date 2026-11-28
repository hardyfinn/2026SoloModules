// Renders demo WAVs from the real engine on a made-up break (no Rack needed).
//   make test   ->  test/out/*.wav
#include "../src/Engine.hpp"
#include "synth_break.hpp"
#include <cstdio>
#include <string>
using namespace breaker;

static void writeWav(const std::string& path, const std::vector<float>& L, const std::vector<float>& R, int rate) {
	FILE* f = std::fopen(path.c_str(), "wb");
	if (!f) { std::printf("cannot write %s\n", path.c_str()); return; }
	uint32_t n = (uint32_t) L.size(), bytes = n * 4;
	auto w32 = [&](uint32_t v) { std::fwrite(&v, 4, 1, f); };
	auto w16 = [&](uint16_t v) { std::fwrite(&v, 2, 1, f); };
	std::fwrite("RIFF", 1, 4, f); w32(36 + bytes); std::fwrite("WAVEfmt ", 1, 8, f);
	w32(16); w16(1); w16(2); w32(rate); w32(rate * 4); w16(4); w16(16);
	std::fwrite("data", 1, 4, f); w32(bytes);
	for (uint32_t i = 0; i < n; i++) {
		int16_t a = (int16_t) std::max(-32767.f, std::min(32767.f, L[i] * 32767.f));
		int16_t b = (int16_t) std::max(-32767.f, std::min(32767.f, R[i] * 32767.f));
		std::fwrite(&a, 2, 1, f); std::fwrite(&b, 2, 1, f);
	}
	std::fclose(f);
}

struct Run { const char* file; int style; float density, mutate, ghosts, fills, swing, variety, bpm; int bars; float secs; bool lockHalf; };

int main() {
	const int RATE = 44100;
	std::vector<testbreak::Truth> truth;
	auto audio = testbreak::make((float) RATE, truth, 0);
	writeWav("test/out/00-made-up-break.wav", audio->L, audio->R, RATE);
	BreakData* brk = analyse(audio, "made-up-break.wav", 0.5f, 0, {});
	std::printf("break: %zu hits, %d bars, %.1f BPM  (K %zu S %zu H %zu O %zu)\n", brk->hits.size(), brk->loopBars, brk->bpm,
		brk->byType[0].size(), brk->byType[1].size(), brk->byType[2].size(), brk->byType[3].size());

	const Run runs[] = {
		{"01-break-own-rhythm", ST_BREAK, 0.5f, 0.f, 0.5f, 0.f, 0.56f, 0.f, 0, 2, 9.6f, false},
		{"02-break-variety-mutate", ST_BREAK, 0.6f, 0.5f, 0.5f, 0.3f, 0.54f, 0.6f, 0, 2, 19.2f, false},
		{"03-breaks", ST_BREAKS, 0.5f, 0.2f, 0.6f, 0.3f, 0.56f, 0.5f, 0, 2, 19.2f, false},
		{"04-jungle-170", ST_JUNGLE, 0.55f, 0.35f, 0.6f, 0.5f, 0.5f, 0.7f, 170, 2, 17.0f, false},
		{"05-hip-hop-90", ST_HIPHOP, 0.5f, 0.15f, 0.5f, 0.2f, 0.62f, 0.3f, 90, 1, 16.0f, false},
		{"06-broken-techno-128", ST_TECHNO, 0.55f, 0.4f, 0.4f, 0.25f, 0.53f, 0.6f, 128, 2, 15.0f, false},
		{"07-mutate-then-lock", ST_JUNGLE, 0.5f, 0.9f, 0.5f, 0.2f, 0.5f, 0.6f, 165, 1, 17.4f, true},
	};
	for (const Run& r : runs) {
		BreakEngine e;
		e.rng.seed(77);
		e.roll(true);
		e.setBreak(brk);
		Knobs k;
		k.style = r.style; k.density = r.density; k.mutate = r.mutate; k.ghosts = r.ghosts; k.fills = r.fills;
		k.swing = r.swing; k.variety = r.variety; k.bars = r.bars;
		k.clockConnected = r.bpm > 0;
		double clkPeriod = r.bpm > 0 ? 15.0 / r.bpm : 0, clkPhase = 0;
		size_t n = (size_t) (r.secs * RATE);
		std::vector<float> L(n), R(n);
		const float dt = 1.f / RATE;
		int hits = 0;
		float peak = 0;
		for (size_t i = 0; i < n; i++) {
			if (k.clockConnected) {
				clkPhase += dt;
				if (clkPhase >= clkPeriod) clkPhase -= clkPeriod;
				k.clock = clkPhase < 0.002 ? 10.f : 0.f;
			}
			k.lock = r.lockHalf && i > n / 2;
			Outs o;
			e.process(k, dt, o);
			if (o.gate[0] > 0 || o.gate[1] > 0) hits++;
			L[i] = o.L / 5.f * 0.7f; R[i] = o.R / 5.f * 0.7f;
			peak = std::max(peak, std::fabs(L[i]));
		}
		writeWav(std::string("test/out/") + r.file + ".wav", L, R, RATE);
		std::printf("%-26s peak %.2f  bars played %ld\n", r.file, peak, e.barCount);
	}
	delete brk;
	return 0;
}
