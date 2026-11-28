// Offline checks for CrateDSP.hpp (spec section 12). Not part of the plugin build.
//   g++ -O2 -std=c++17 -I../src render.cpp -o render && ./render outdir
#include "CrateDSP.hpp"
#include <cstdio>
#include <string>
using namespace crate;

static std::vector<float> run(const std::vector<int16_t>& mem, double fs, Frame f, double secs, bool trigAtStart = true) {
	Voice v; v.setSampleRate(fs); v.setMemory(mem.data(), mem.size());
	std::vector<float> out((size_t)(secs * fs));
	for (size_t i = 0; i < out.size(); i++) {
		f.trig = trigAtStart && i == 0;
		out[i] = v.process(f).out / 5.f;
	}
	return out;
}

int main(int argc, char** argv) {
	std::string dir = argc > 1 ? argv[1] : ".";
	const double fsS = 48000;
	// 1 kHz sine source, 2 s
	std::vector<float> sine(96000);
	for (size_t i = 0; i < sine.size(); i++) sine[i] = 0.5f * std::sin(2 * M_PI * 1000 * i / fsS);
	auto cap = capture(sine, fsS, 1.f, 1.0, 10.0);
	std::printf("sine capture: %zu codes (%.3f s)\n", cap.codes.size(), cap.codes.size() / SP_RATE);
	Frame f; f.channel = 3; f.decay = 1.f; f.level = 1.f;
	for (int t : {0, -7, 7}) {
		f.ratioSemis = (float)t;
		auto o = run(cap.codes, 96000, f, 1.5);
		writeWavFloat((dir + "/sine_tune" + std::to_string(t) + ".wav").c_str(), o, 96000);
	}
	// AGE 0 must be neutral: run twice with different seeds
	{
		Voice a, b; a.setSeed(1); b.setSeed(999);
		a.setSampleRate(48000); b.setSampleRate(48000);
		a.setMemory(cap.codes.data(), cap.codes.size()); b.setMemory(cap.codes.data(), cap.codes.size());
		Frame g = f; g.channel = 0; g.decay = 0.5f;
		double diff = 0;
		for (int i = 0; i < 48000; i++) { g.trig = i == 0; diff += std::fabs(a.process(g).out - b.process(g).out); }
		std::printf("AGE 0 seed difference: %g (want 0)\n", diff);
	}
	// synthetic break: kick, snare, hats at 95 bpm, 2 bars
	double bpm = 95, sp16 = 60.0 / bpm / 4;
	std::vector<float> brk((size_t)(32 * sp16 * fsS));
	const char* pat = "K.h.S.h.K.hKS.h.K.h.S.hKK.h.S.hS";
	uint32_t r = 7;
	for (int s = 0; s < 32; s++) {
		size_t o = (size_t)(s * sp16 * fsS);
		for (size_t i = 0; i < (size_t)(0.4 * fsS) && o + i < brk.size(); i++) {
			double t = i / fsS; r = r * 1664525u + 1013904223u; double n = ((r >> 8) / 8388608.0) - 1.0;
			double v = 0;
			if (pat[s] == 'K') v = 0.9 * std::sin(2 * M_PI * (50 * t + 120 * (1 - std::exp(-t * 30)) / 30)) * std::exp(-t * 7);
			if (pat[s] == 'S') v = (0.5 * n + 0.4 * std::sin(2 * M_PI * 190 * t)) * std::exp(-t * 14);
			v += 0.12 * n * std::exp(-t * 60) * (s % 2 == 0 ? 1 : 0.6);
			brk[o + i] += (float)v;
		}
	}
	writeWavFloat((dir + "/break_source.wav").c_str(), brk, fsS);
	// break across the tune range, 33 vs 45, through channel 7·8 and 1·2
	for (double k : {1.0, 1.35}) {
		auto c = capture(brk, fsS, 1.5f, k, 10.0);
		std::vector<float> all;
		Frame g; g.k = k; g.level = 0.9f; g.channel = 3;
		for (int t : {0, -5, -10, 5}) {
			g.ratioSemis = (float)t;
			double len = c.codes.size() / SP_RATE * k / std::pow(2.0, t / 12.0);
			auto o = run(c.codes, 48000, g, len + 0.2);
			all.insert(all.end(), o.begin(), o.end());
		}
		writeWavFloat((dir + "/break_k" + std::to_string((int)(k * 100)) + ".wav").c_str(), all, 48000);
		std::printf("break k=%.2f: %zu codes clipped=%d\n", k, c.codes.size(), (int)c.clipped);
	}
	// channel 1·2 with a short decay on a held sine: tail should darken
	{
		Frame g; g.channel = 0; g.decay = 0.35f; g.level = 1.f;
		std::vector<float> nz(96000); uint32_t q = 3;
		for (float& v : nz) { q = q * 1664525u + 1013904223u; v = 0.5f * (((q >> 8) / 8388608.f) - 1.f); }
		auto c = capture(nz, fsS, 1.f, 1.0, 10.0);
		auto o = run(c.codes, 48000, g, 1.5);
		writeWavFloat((dir + "/ch12_decay.wav").c_str(), o, 48000);
	}
	return 0;
}
