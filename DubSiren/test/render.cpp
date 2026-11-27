// Offline check + demo for Dub Siren (no Rack needed): make test
// Writes test/dubsiren-demo.wav and prints the pitch of each mode.
#include <cstdio>
#include <cmath>
#include <cstdint>
#include <vector>
#include <string>
#include "../src/DubSirenDSP.hpp"

using namespace dubsiren;
static const float SR = 48000.f;

struct Wav {
	std::vector<int16_t> d;
	void push(float v) { d.push_back((int16_t) std::lround(clampf(v / 6.f, -1.f, 1.f) * 32000.f)); }
	void save(const char* p) {
		FILE* f = fopen(p, "wb"); if (!f) return;
		uint32_t n = d.size() * 2, r = (uint32_t) SR, br = r * 2; uint16_t ch = 1, bps = 16, ba = 2, fmt = 1; uint32_t sz = 36 + n, f16 = 16;
		fwrite("RIFF", 1, 4, f); fwrite(&sz, 4, 1, f); fwrite("WAVEfmt ", 1, 8, f); fwrite(&f16, 4, 1, f);
		fwrite(&fmt, 2, 1, f); fwrite(&ch, 2, 1, f); fwrite(&r, 4, 1, f); fwrite(&br, 4, 1, f); fwrite(&ba, 2, 1, f); fwrite(&bps, 2, 1, f);
		fwrite("data", 1, 4, f); fwrite(&n, 4, 1, f); fwrite(d.data(), 2, d.size(), f); fclose(f);
	}
};

int main() {
	Engine e; e.setRate(SR);
	Wav w;
	const float dt = 1.f / SR;
	// 1. pitch check: each PITCH, mode 2 (+V | square) with SIREN held = steady tone
	for (int p = 0; p < 4; p++) {
		Settings s; s.pitch = p; s.mode = 1; s.beat = 3; s.power = true; s.siren = true;
		int cross = 0; float prev = 0; double rms = 0; int N = (int) (SR * 0.6f);
		for (int i = 0; i < N; i++) {
			float v = e.process(s, 0, dt);
			if (i > N / 3) { if (prev < 0 && v >= 0) cross++; rms += v * v; }
			prev = v; w.push(v);
		}
		printf("PITCH %d: %.0f Hz, rms %.2f V\n", p + 1, cross / (0.6 * 2 / 3), std::sqrt(rms / (N * 2 / 3)));
	}
	// 2. every MODE x BEAT, 2.4 s each, TRIG on, SIREN tapped at the start
	for (int m = 0; m < 4; m++)
		for (int b = 0; b < 4; b++) {
			Settings s; s.pitch = 2; s.mode = m; s.beat = b; s.power = true;
			float peak = 0; double rms = 0; int N = (int) (SR * 2.4f);
			for (int i = 0; i < N; i++) {
				s.siren = i < (int) (SR * 0.25f);
				s.power = i < (int) (SR * 2.0f);
				float v = e.process(s, 0, dt);
				if (!std::isfinite(v)) { printf("NaN!\n"); return 1; }
				peak = std::max(peak, std::fabs(v)); rms += v * v; w.push(v);
			}
			printf("MODE %d BEAT %s: peak %.2f V rms %.2f V\n", m + 1, b < 3 ? std::to_string(b + 1).c_str() : "OFF", peak, std::sqrt(rms / N));
		}
	// 3. TONE button whoop with BEAT OFF, mode 4, then release
	{
		Settings s; s.pitch = 2; s.mode = 3; s.beat = 3; s.power = true;
		for (int i = 0; i < (int) (SR * 4); i++) {
			s.tone = i < (int) (SR * 2.0f);
			s.power = i < (int) (SR * 3.2f);
			w.push(e.process(s, 0, dt));
		}
	}
	// 4. a dub run: mode 1, beat 2, echo 80s then 90s with feedback pushed
	for (int era = 0; era < 2; era++) {
		Settings s; s.pitch = 2; s.mode = 0; s.beat = 1; s.feedback = 0.8f; s.time = 0.375f; s.era80 = era == 0;
		for (int i = 0; i < (int) (SR * 7); i++) {
			float t = i / SR;
			s.power = (t < 1.5f) || (t > 3.f && t < 3.6f);
			s.siren = (t > 3.0f && t < 3.15f);
			if (t > 4.5f) s.feedback = 1.08f;
			float v = e.process(s, 0, dt);
			if (!std::isfinite(v)) { printf("NaN!\n"); return 1; }
			w.push(v);
		}
	}
	w.save("test/dubsiren-demo.wav");
	printf("wrote test/dubsiren-demo.wav (%.1f s)\n", w.d.size() / SR);
	return 0;
}
