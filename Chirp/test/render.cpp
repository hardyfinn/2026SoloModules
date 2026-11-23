// Offline demo: renders chirp-demo.wav (44.1 kHz stereo) using the module's own engine.
//   make test   (or: g++ -O2 -std=c++17 test/render.cpp -o test/render && ./test/render .)
#include "../src/ChirpPresets.hpp"
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <algorithm>

using namespace chirp;

struct Event {
	double at;        // seconds
	int preset;
	float pitch;      // V/OCT offset
	float level;
	float pan;        // -1..1
	double loopFor;   // for looping presets: seconds before LOOP switches off
	float sweepMul = 1.f, decayMul = 1.f, toneAdd = 0.f;
};

static int P(const char* n) {
	for (int i = 0; i < NUM_PRESETS; i++) if (!strcmp(PRESETS[i].name, n)) return i;
	fprintf(stderr, "no preset %s\n", n); return 0;
}

int main(int argc, char** argv) {
	std::string dir = argc > 1 ? argv[1] : ".";
	const float SR = 44100.f;
	const double BPM = 130.0, BEAT = 60.0 / BPM, STEP = BEAT / 4.0, BAR = BEAT * 4.0;
	std::vector<Event> ev;
	auto add = [&](double at, const char* n, float pitch, float level, float pan, double loopFor = 0.0) {
		Event e; e.at = at; e.preset = P(n); e.pitch = pitch; e.level = level; e.pan = pan; e.loopFor = loopFor;
		ev.push_back(e); return &ev.back();
	};

	// ---- Part 1: a techno groove where every sound is one Chirp (8 bars)
	double t0 = 0.2;
	for (int bar = 0; bar < 8; bar++) {
		double b = t0 + bar * BAR;
		for (int q = 0; q < 4; q++) add(b + q * BEAT, "Kick", 0.f, 0.95f, 0.f);
		if (bar >= 1) for (int s = 0; s < 16; s++) {
			float acc = (s % 4 == 2) ? 0.55f : 0.22f;
			add(b + s * STEP + (s % 2 ? 0.012 : 0.0), "Tick", (s % 4 == 2) ? 0.f : 0.3f, acc, 0.35f);
		}
		if (bar >= 2) { int rs[] = {3, 6, 11, 14}; for (int s : rs) add(b + s * STEP, "Rim", s == 14 ? 0.2f : 0.f, 0.35f, -0.4f); }
		if (bar >= 2) { int ts[] = {7, 10}; float tp[] = {0.f, -0.25f}; for (int i = 0; i < 2; i++) add(b + ts[i] * STEP, "Tom", tp[i], 0.45f, i ? 0.3f : -0.2f); }
		// bassline: drops on the off-beats, a minor riff
		if (bar >= 4) { float riff[] = {0.f, 0.f, 0.25f, -0.0833f}; for (int q = 0; q < 4; q++) {
			Event* e = add(b + q * BEAT + BEAT / 2, "Bass Drop", riff[q] + 0.4f, 0.55f, 0.f); e->decayMul = 0.22f; e->sweepMul = 0.6f; } }
		if (bar == 3 || bar == 7) add(b + 3.5 * BEAT, "Zap", 0.f, 0.5f, 0.2f);
		if (bar == 5) add(b + 2 * BEAT, "Laser", 0.f, 0.3f, -0.3f);
	}

	// ---- Part 2: the animals (each preset on its own, then a little dusk scene)
	double t = t0 + 8 * BAR + 0.8;
	add(t, "Bat", 0.f, 0.6f, -0.5f); add(t + 0.55, "Bat", 0.1f, 0.5f, -0.1f); add(t + 1.1, "Bat", -0.05f, 0.55f, 0.4f);
	t += 2.1;
	add(t, "Frog", 0.f, 0.6f, -0.2f); add(t + 0.75, "Frog", -0.15f, 0.55f, 0.3f); add(t + 1.4, "Frog", 0.f, 0.6f, -0.2f);
	t += 2.4;
	for (int i = 0; i < 5; i++) add(t + i * 0.42, "Cricket", 0.f, 0.45f, 0.5f);
	t += 2.4;
	add(t, "Cicada", 0.f, 0.35f, -0.3f, 2.2);
	t += 2.8;
	add(t, "Bird Up", 0.f, 0.5f, 0.2f); add(t + 0.55, "Bird Down", 0.f, 0.45f, -0.2f); add(t + 1.1, "Bird Up", 0.25f, 0.5f, 0.3f);
	add(t + 1.5, "Droplet", 0.f, 0.5f, 0.f); add(t + 1.8, "Droplet", 0.3f, 0.45f, 0.2f);
	t += 2.8;
	// dusk scene: slow kick under cicadas, crickets, frogs and birds
	double scene = t;
	add(scene, "Cicada", 0.f, 0.22f, -0.5f, 7.4);
	add(scene + 2.5, "Cicada", 0.08f, 0.16f, 0.5f, 4.5);
	for (int i = 0; i < 16; i++) add(scene + i * BEAT, "Kick", 0.f, 0.8f, 0.f);
	for (int i = 0; i < 8; i++) add(scene + 0.3 + i * 0.9, "Cricket", (i % 3) * 0.04f, 0.3f, 0.6f);
	add(scene + 1.0, "Frog", 0.f, 0.45f, -0.3f); add(scene + 3.2, "Frog", -0.15f, 0.4f, 0.35f); add(scene + 5.4, "Frog", 0.f, 0.45f, -0.3f);
	add(scene + 1.9, "Bird Up", 0.f, 0.35f, 0.4f); add(scene + 4.4, "Bird Down", 0.1f, 0.35f, -0.4f);
	add(scene + 6.0, "Bat", 0.f, 0.4f, 0.f);
	t = scene + 16 * BEAT + 0.6;
	// ---- Part 3: sweeps you would use as effects and modulation
	add(t, "Siren", 0.f, 0.3f, 0.f, 3.0);
	add(t + 3.6, "Slow Wobble", 0.f, 0.5f, 0.f, 3.3);
	add(t + 7.4, "Kick", 0.f, 0.95f, 0.f);
	double end = t + 9.0;

	std::sort(ev.begin(), ev.end(), [](const Event& a, const Event& b) { return a.at < b.at; });
	size_t N = (size_t)(end * SR);
	std::vector<float> L(N, 0.f), R(N, 0.f);
	for (const Event& e : ev) {
		Settings s = settingsFor(PRESETS[e.preset]);
		s.pitch += e.pitch; s.level = e.level; s.sweep *= e.sweepMul; s.decay *= e.decayMul; s.tone += e.toneAdd;
		Voice v;
		size_t i0 = (size_t)(e.at * SR);
		bool looping = s.loop;
		if (!looping) v.trigger(s);
		float gl = std::sqrt(0.5f * (1.f - e.pan)), gr = std::sqrt(0.5f * (1.f + e.pan));
		for (size_t i = i0; i < N; i++) {
			double el = (i - i0) / SR;
			if (looping && el > e.loopFor) s.loop = false;
			float o, en, pv;
			v.process(s, SR, o, en, pv);
			L[i] += o * gl; R[i] += o * gr;
			if (!v.active && en < 1e-5f && el > 0.01) break;
		}
	}
	// gentle bus limiter so the demo never clips
	float peak = 0.f;
	for (size_t i = 0; i < N; i++) peak = std::max(peak, std::max(std::fabs(L[i]), std::fabs(R[i])));
	float g = peak > 0.f ? 0.89f / peak : 1.f;
	if (g > 1.f) g = 1.f;
	std::string path = dir + "/chirp-demo.wav";
	FILE* f = fopen(path.c_str(), "wb");
	if (!f) { perror(path.c_str()); return 1; }
	auto w32 = [&](uint32_t x) { fwrite(&x, 4, 1, f); };
	auto w16 = [&](uint16_t x) { fwrite(&x, 2, 1, f); };
	fwrite("RIFF", 1, 4, f); w32(36 + N * 4); fwrite("WAVEfmt ", 1, 8, f);
	w32(16); w16(1); w16(2); w32(44100); w32(44100 * 4); w16(4); w16(16);
	fwrite("data", 1, 4, f); w32(N * 4);
	for (size_t i = 0; i < N; i++) {
		int16_t a = (int16_t)std::lrint(std::tanh(L[i] * g * 1.1f) / 1.1f * 32767.f);
		int16_t b = (int16_t)std::lrint(std::tanh(R[i] * g * 1.1f) / 1.1f * 32767.f);
		fwrite(&a, 2, 1, f); fwrite(&b, 2, 1, f);
	}
	fclose(f);
	printf("wrote %s (%.1f s, %zu events, peak %.2f)\n", path.c_str(), end, ev.size(), peak);
	return 0;
}
