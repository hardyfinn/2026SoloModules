// Offline checks for the Copy That engine + demo WAVs. No Rack needed.
//   make test      (writes test/copythat-demo.wav)
#include "../src/CopyThatDSP.hpp"
#include <cstdio>
#include <vector>
#include <string>
#include <chrono>

using namespace copythat;
static const float SR = 48000.f;
static int fails = 0;
#define CHECK(c, msg) do { if (!(c)) { std::printf("FAIL: %s\n", msg); fails++; } else std::printf("ok:   %s\n", msg); } while (0)

static void writeWav(const char* path, const std::vector<float>& L) {
	FILE* f = std::fopen(path, "wb");
	if (!f) return;
	uint32_t n = L.size(), bytes = n * 2;
	auto u32 = [&](uint32_t v) { std::fwrite(&v, 4, 1, f); };
	auto u16 = [&](uint16_t v) { std::fwrite(&v, 2, 1, f); };
	std::fwrite("RIFF", 1, 4, f); u32(36 + bytes); std::fwrite("WAVEfmt ", 1, 8, f);
	u32(16); u16(1); u16(1); u32((uint32_t) SR); u32((uint32_t) SR * 2); u16(2); u16(16);
	std::fwrite("data", 1, 4, f); u32(bytes);
	for (uint32_t i = 0; i < n; i++) {
		int16_t a = (int16_t) std::max(-32767.f, std::min(32767.f, L[i] / 10.f * 32767.f));
		std::fwrite(&a, 2, 1, f);
	}
	std::fclose(f);
}

// A little test programme: kick, hat, and a chord stab, 125 BPM
struct Source {
	Rng r; double t = 0; float kph = 0, env = 0, hat = 0, padPh[3] = {0, 0, 0};
	float next() {
		float beat = 60.f / 125.f;
		double ph = std::fmod(t, beat);
		double ph8 = std::fmod(t + beat / 2, beat);
		float kf = 45.f + 120.f * std::exp(-ph * 30.f);
		kph += kf / SR; if (kph > 1) kph -= 1;
		float kick = std::sin(TWO_PI * kph) * std::exp(-ph * 7.f);
		float h = r.bi() * std::exp(-ph8 * 60.f) * 0.3f;
		static const float fr[3] = {174.61f, 207.65f, 261.63f}; // F minor
		float pad = 0;
		double bar = std::fmod(t, beat * 4);
		float stab = (bar < beat * 0.5 || (bar > beat * 2.5 && bar < beat * 3)) ? 1.f : 0.f;
		for (int i = 0; i < 3; i++) { padPh[i] += fr[i] / SR; padPh[i] -= std::floor(padPh[i]); pad += (2.f * padPh[i] - 1.f) * 0.15f; }
		env += (stab - env) * 0.002f;
		t += 1.0 / SR;
		return 5.f * (0.6f * kick + h + pad * env);
	}
};

static double rms(const std::vector<float>& v, size_t a, size_t b) {
	double s = 0; for (size_t i = a; i < b; i++) s += v[i] * v[i]; return std::sqrt(s / std::max<size_t>(1, b - a));
}

int main(int argc, char** argv) {
	std::string dir = argc > 1 ? argv[1] : ".";
	// 1. clean link: wet should track dry closely so the two cancel
	{
		Channel ch; ch.setRate(SR, 1);
		Settings st; st.tune = 0.f; st.shift = 0.f; st.drive = 0.1f;
		Source src;
		std::vector<float> w, d, diff;
		for (int i = 0; i < (int) SR * 3; i++) {
			Frame f = ch.process(src.next(), st, false);
			w.push_back(f.wet); d.push_back(f.dry); diff.push_back(f.wet - f.dry);
		}
		size_t a = SR, b = SR * 3;
		double rw = rms(w, a, b), rd = rms(d, a, b), rx = rms(diff, a, b);
		double null = 20 * std::log10(rx / rd);
		std::printf("     clean: wet %.3f V rms, dry %.3f V rms, null %.1f dB\n", rw, rd, null);
		CHECK(std::fabs(20 * std::log10(rw / rd)) < 1.5, "clean wet level matches dry within 1.5 dB");
		CHECK(null < -15, "clean wet cancels dry by at least 15 dB");
		CHECK(ch.locked, "receiver locked on station");
	}
	// 2. sweep each control: no NaN, bounded output; measure lost-signal level
	{
		const float tunes[] = {0.f, 0.3f, 0.5f, 0.7f, 0.9f, 1.f};
		const float shifts[] = {0.f, 0.5f, 1.f};
		bool finite = true; float peak = 0.f;
		for (int ext = 0; ext < 2; ext++)
		for (float t : tunes) for (float s : shifts) {
			Channel ch; ch.setRate(SR, 7);
			Settings st; st.tune = t; st.shift = s; st.drive = 0.3f; st.extended = ext;
			Source src; std::vector<float> w, df; double sigAcc = 0; int lockN = 0;
			for (int i = 0; i < (int) SR; i++) {
				Frame f = ch.process(src.next(), st, false);
				if (!std::isfinite(f.wet)) finite = false;
				peak = std::max(peak, std::fabs(f.wet)); w.push_back(f.wet); df.push_back(f.wet - f.dry);
				sigAcc += f.signal; lockN += f.lock;
			}
			std::printf("     R%d tune %.1f shift %.1f: wet %.2f V rms, damage %5.1f dB, signal %.2f, locked %3.0f%%\n",
				ext + 1, t, s, rms(w, SR / 2, SR), 20 * std::log10(rms(df, SR / 2, SR) / rms(w, SR / 2, SR)), sigAcc / SR, 100.0 * lockN / SR);
		}
		CHECK(finite, "all settings finite");
		std::printf("     peak wet %.1f V\n", peak);
	}
	// 3. fade to nothing: signal meter falls, lock drops
	{
		Channel ch; ch.setRate(SR, 3);
		Settings st; st.fade = 0.01f; Source src; Frame f;
		for (int i = 0; i < (int) SR; i++) f = ch.process(src.next(), st, false);
		std::printf("     faded: signal %.2f\n", f.signal);
		CHECK(f.signal < 0.3f && !f.lock, "no signal = low meter and no lock");
	}
	// 4. CPU: 16 channels for 1 s
	{
		std::vector<Channel> ch(16); for (int c = 0; c < 16; c++) ch[c].setRate(SR, c + 1);
		Settings st; st.tune = 0.6f; st.shift = 0.6f; st.extended = true; Source src;
		auto t0 = std::chrono::steady_clock::now();
		float sink = 0;
		for (int i = 0; i < (int) SR; i++) { float x = src.next(); for (auto& c : ch) sink += c.process(x, st, true).wet; }
		double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
		std::printf("     16 channels x 1 s: %.0f ms (%.1f%% of one core per channel) %g\n", ms, ms / 16 / 10, sink * 0);
	}
	// 5. demo: a 40 s tour
	{
		Channel ch; ch.setRate(SR, 11); Source src; std::vector<float> out;
		int N = SR * 40;
		for (int i = 0; i < N; i++) {
			float t = i / SR; Settings st; bool voice = false; float wet = 1, dry = 0;
			if (t < 4) { st.tune = 0.f; }                                        // clean link
			else if (t < 12) { st.tune = (t - 4) / 8.f * 0.75f; st.shift = 0.2f; } // drift off the station
			else if (t < 18) { st.tune = 0.35f; st.shift = 0.2f + (t - 12) / 6.f * 0.8f; } // SHIFT up
			else if (t < 24) { st.tune = 0.25f; st.shift = 0.3f; st.fade = 0.5f + 0.5f * std::cos(TWO_PI * 0.5f * t); st.fade = st.fade * st.fade; } // FADE LFO
			else if (t < 32) { st.extended = true; st.tune = 0.36f + (t - 24) / 8.f * 0.55f; st.shift = 0.35f; } // RANGE 2 to the neighbour
			else { st.tune = 0.45f; st.shift = 0.5f; st.drive = 0.7f; voice = true; dry = 1.f; } // artifacts only, mid-scoop
			Frame f = ch.process(src.next(), st, voice);
			float y = -wet * f.wet + dry * f.dry;
			out.push_back(y);
		}
		writeWav((dir + "/test/copythat-demo.wav").c_str(), out);
		std::printf("     wrote test/copythat-demo.wav\n");
	}
	std::printf(fails ? "%d FAILED\n" : "all passed\n", fails);
	return fails ? 1 : 0;
}
