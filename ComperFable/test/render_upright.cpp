// Renders every case of test/upright_cases.json through src/dsp/Upright.hpp to DIR/NNN.wav (mono 48 kHz float),
// for tools/check_upright.py. Also reports the CPU cost.  ./test/render_upright DIR
#include "../src/dsp/Upright.hpp"
#include "upright_cases.h"
#include <cstdio>
#include <string>
#include <chrono>
static void writeWav(const char* path, const std::vector<float>& x, int sr) {
	FILE* f = fopen(path, "wb"); if (!f) return;
	auto u32 = [&](uint32_t v) { fwrite(&v, 4, 1, f); }; auto u16 = [&](uint16_t v) { fwrite(&v, 2, 1, f); };
	uint32_t data = (uint32_t)x.size() * 4;
	fwrite("RIFF", 1, 4, f); u32(36 + data); fwrite("WAVE", 1, 4, f);
	fwrite("fmt ", 1, 4, f); u32(16); u16(3); u16(1); u32(sr); u32(sr * 4); u16(4); u16(32);
	fwrite("data", 1, 4, f); u32(data); fwrite(x.data(), 4, x.size(), f); fclose(f);
}
int main(int argc, char** argv) {
	std::string dir = argc > 1 ? argv[1] : "test/out/upright";
	const int SR = 48000; double secs = 0, cpu = 0;
	for (int ci = 0; ci < NUM_UCASES; ci++) {
		const UCase& c = UCASES[ci];
		upright::Bass b; b.setSampleRate(SR);
		const double* t = c.tone; b.tone.pluck = t[0]; b.tone.bright = t[1]; b.tone.sustain = t[2]; b.tone.growl = t[3]; b.tone.body = t[4];
		b.tone.pickup = t[5]; b.tone.age = t[6]; b.tone.thump = t[7]; b.tone.attack = t[8];
		for (int i = 0; i < c.nnotes; i++) {
			const UNote& n = c.notes[i]; upright::NoteSpec ns; ns.pitch = n.pitch; ns.vel = n.vel; ns.string = n.string; ns.fret = n.fret;
			ns.dead = n.dead; ns.scoop = n.scoop; ns.cents = n.cents; ns.vib = n.vib;
			long long at = (long long)std::nearbyint(n.t * SR);
			b.noteOn(ns, (int64_t)c.seed * 7919 + 31 * i + 5, i, at); b.release(i, n.dur, n.stopped);
		}
		int N = (int)(c.total * SR); std::vector<float> x(N);
		auto t0 = std::chrono::steady_clock::now();
		for (int i = 0; i < N; i++) x[i] = (float)b.process();
		cpu += std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count(); secs += c.total;
		char path[512]; snprintf(path, sizeof path, "%s/%03d.wav", dir.c_str(), ci); writeWav(path, x, SR);
	}
	printf("rendered %d sound cases, %.1f s of audio, CPU %.2f%% of one core\n", NUM_UCASES, secs, 100 * cpu / secs);
	return 0;
}
