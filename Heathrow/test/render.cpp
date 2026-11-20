// Offline checks for the Heathrow engine + a demo WAV. No Rack needed.
//   make test      (writes test/heathrow-demo.wav)
#include "../src/HeathrowDSP.hpp"
#include <cstdio>
#include <vector>
#include <string>

using namespace heathrow;
static const float SR = 48000.f;
static int fails = 0;
#define CHECK(c, msg) do { if (!(c)) { std::printf("FAIL: %s\n", msg); fails++; } else std::printf("ok:   %s\n", msg); } while (0)

// RMS of the output for a sine at hz, with a fixed sweep position
static float sineRms(float hz, float s, Settings st) {
	static Channel ch;
	ch.reset();
	double acc = 0; int n = 0;
	for (int i = 0; i < (int) SR; i++) {
		float x = 5.f * std::sin(2.f * PI_F * hz * i / SR);
		float y = ch.process(x, s, st, SR);
		if (i > SR / 2) { acc += y * y; n++; }
	}
	return std::sqrt(acc / n);
}

static void writeWav(const char* path, const std::vector<float>& L, const std::vector<float>& R) {
	FILE* f = std::fopen(path, "wb");
	if (!f) return;
	uint32_t n = L.size(), bytes = n * 4;
	auto u32 = [&](uint32_t v) { std::fwrite(&v, 4, 1, f); };
	auto u16 = [&](uint16_t v) { std::fwrite(&v, 2, 1, f); };
	std::fwrite("RIFF", 1, 4, f); u32(36 + bytes); std::fwrite("WAVEfmt ", 1, 8, f);
	u32(16); u16(1); u16(2); u32((uint32_t) SR); u32((uint32_t) SR * 4); u16(4); u16(16);
	std::fwrite("data", 1, 4, f); u32(bytes);
	for (uint32_t i = 0; i < n; i++) {
		int16_t a = (int16_t) std::max(-32767.f, std::min(32767.f, L[i] / 10.f * 32767.f));
		int16_t b = (int16_t) std::max(-32767.f, std::min(32767.f, R[i] / 10.f * 32767.f));
		std::fwrite(&a, 2, 1, f); std::fwrite(&b, 2, 1, f);
	}
	std::fclose(f);
}

int main(int argc, char** argv) {
	std::string dir = argc > 1 ? argv[1] : ".";

	// 1. phaser, one section, 50/50: a sine at the centre frequency cancels
	{
		Settings st; st.sections = 1; st.mix = 0.5f;
		float s = 0.5f, fc = phaserHz(s);
		float notch = sineRms(fc, s, st), off = sineRms(fc * 4.f, s, st);
		std::printf("      phaser notch %.4f V rms, two octaves up %.3f V rms\n", notch, off);
		CHECK(notch < 0.05f && off > 1.5f, "phaser notch sits at CENTER");
	}
	// 2. through-zero: at the zero point, wet == dry, so -WET 50/50 is silence
	{
		Settings st; st.morph = 1.f; st.tz = true; st.mix = -0.5f;
		float r = sineRms(440.f, 0.5f, st);
		std::printf("      TZ zero point, inverted mix: %.5f V rms\n", r);
		CHECK(r < 0.01f, "through-zero cancels at the zero point");
	}
	// 3. flanger first notch at 1/(2d)
	{
		Settings st; st.morph = 1.f; st.mix = 0.5f;
		float s = 0.5f, d = flangerMs(s) * 0.001f;
		float r = sineRms(1.f / (2.f * d), s, st);
		CHECK(r < 0.05f, "flanger notch at 1/(2*delay)");
	}
	// 4. engine response formula agrees with the sound (screen is honest)
	{
		Settings st; st.sections = 4; st.feedback = 0.6f; st.mix = 0.5f; st.q = 1.2f;
		float worst = 0.f;
		for (float hz : {200.f, 700.f, 1500.f, 5000.f}) {
			float meas = 20.f * std::log10(sineRms(hz, 0.45f, st) / (5.f / std::sqrt(2.f)) + 1e-6f);
			float calc = responseDb(hz, 0.45f, st, SR);
			worst = std::max(worst, std::fabs(meas - calc));
		}
		std::printf("      response screen vs measured: worst %.2f dB\n", worst);
		CHECK(worst < 1.0f, "screen response matches the audio");
	}
	// 5. stress: max feedback both signs, audio-rate sweep, every setting -> finite, bounded
	{
		bool ok = true; float peak = 0.f;
		for (int tz = 0; tz < 2; tz++)
		for (float fb : {-0.97f, 0.97f})
		for (float m : {0.f, 0.5f, 1.f})
		for (int n : {1, 12}) {
			Channel ch; Settings st; st.tz = tz; st.feedback = fb; st.morph = m; st.sections = n; st.q = 6.f; st.mix = 1.f;
			Rng r;
			for (int i = 0; i < 48000; i++) {
				float s = 0.5f + 0.5f * std::sin(2.f * PI_F * 300.f * i / SR);
				float y = ch.process(5.f * r.next(), s, st, SR);
				if (!std::isfinite(y)) ok = false;
				peak = std::max(peak, std::fabs(y));
			}
		}
		std::printf("      stress peak %.2f V\n", peak);
		CHECK(ok && peak < 60.f, "no blow-ups at max feedback / audio-rate sweep");
	}

	// ---- demo: a saw chord + noisy hats through a tour of settings
	std::vector<float> L, R;
	Channel cl, cr; Lfo lfo; Follower fol;
	auto saw = [](double ph) { return (float) (2.0 * (ph - std::floor(ph)) - 1.0); };
	const float notes[3] = {110.f, 164.8f, 207.6f};
	struct Part { float secs, morph, fb, rate, depth, center, spread, shape, mix; int sections, steps; bool tz; };
	Part parts[] = {
		{6, 0.f, 0.f,  0.25f, 0.8f, 0.5f, 0.25f, 0.f, 0.5f, 3, 0, false},   // gentle 6-stage phaser
		{6, 0.f, -0.8f,0.4f,  0.7f, 0.5f, 0.5f,  1.f, 0.5f, 8, 0, false},   // 16 stages, negative feedback
		{6, 1.f, 0.7f, 0.15f, 0.9f, 0.45f,0.25f, 0.f, 0.5f, 4, 0, false},   // flanger, positive feedback
		{6, 1.f, 0.f,  0.12f, 1.f,  0.5f, 0.0f,  0.f, -0.5f,4, 0, true},    // through-zero jet
		{6, 0.5f,0.6f, 2.f,   0.8f, 0.5f, 0.5f,  5.f, 0.5f, 6, 8, false},   // morph, stepped S&H
	};
	double t = 0;
	for (const Part& p : parts) {
		Settings st; st.morph = p.morph; st.feedback = p.fb; st.sections = p.sections; st.mix = p.mix; st.tz = p.tz;
		int n = (int) (p.secs * SR);
		for (int i = 0; i < n; i++, t += 1.0 / SR) {
			float x = 0.f;
			for (float f : notes) x += saw(t * f);
			x *= 1.3f;
			lfo.advance(p.rate / SR);
			float l = p.center + 0.5f * p.depth * lfo.value(0.f, p.shape, p.steps);
			float r = p.center + 0.5f * p.depth * lfo.value(p.spread * 0.5f, p.shape, p.steps);
			fol.process(x, SR);
			float fade = std::min(1.f, std::min(i / 2400.f, (n - i) / 2400.f));
			L.push_back(fade * cl.process(x, l, st, SR));
			R.push_back(fade * cr.process(x, r, st, SR));
		}
	}
	writeWav((dir + "/heathrow-demo.wav").c_str(), L, R);
	std::printf("%s (%d checks failed)\n", fails ? "FAILED" : "all passed", fails);
	return fails ? 1 : 0;
}
