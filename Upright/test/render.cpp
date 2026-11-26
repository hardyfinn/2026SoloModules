// Offline checks for the Upright engine + demo WAVs. No Rack needed.
//   make test      (prints the checks, writes test/out/*.wav)
#include "../src/UprightEngine.hpp"
#include <cstdio>
#include <vector>
#include <string>
#include <complex>
#include <chrono>
#include <memory>
#if defined(__x86_64__) || defined(__i386__)
#include <xmmintrin.h>
#include <pmmintrin.h>
#endif

using namespace upright;
static float SR = 48000.f;
static int fails = 0;
#define CHECK(c, ...) do { if (!(c)) { std::printf("FAIL: "); fails++; } else std::printf("ok:   "); std::printf(__VA_ARGS__); std::printf("\n"); } while (0)

struct Ev { float t; float m; float vel; float dur; };
struct PedalEv { float t; bool on; };
struct Take { std::vector<float> l, r; };

static Take play(Controls c, const std::vector<Ev>& evs, float secs, const std::vector<PedalEv>& ped = {}) {
	std::unique_ptr<Engine> e(new Engine());
	e->setSampleRate(SR);
	Take t;
	int n = (int)(secs * SR);
	// each event gets its own channel (round robin over 16)
	std::vector<int> on(evs.size()), off(evs.size());
	for (size_t i = 0; i < evs.size(); i++) { on[i] = (int)(evs[i].t * SR); off[i] = (int)((evs[i].t + evs[i].dur) * SR); }
	for (int s = 0; s < n; s++) {
		for (auto& p : ped) if ((int)(p.t * SR) == s) e->setSustain(p.on);
		for (size_t i = 0; i < evs.size(); i++) {
			if (off[i] == s) e->noteOff((int)(i % 16));
			if (on[i] == s) e->noteOn((int)(i % 16), evs[i].m, evs[i].vel, c);
		}
		e->process(c);
		t.l.push_back(e->outL); t.r.push_back(e->outR);
	}
	return t;
}

static float peakAbs(const Take& t);
static void writeWav(const std::string& path, const Take& t, float gain) {
	gain = 0.89f / std::max(1e-6f, peakAbs(t));   // demos: peak at -1 dBFS
	FILE* f = std::fopen(path.c_str(), "wb");
	if (!f) return;
	int n = t.l.size();
	int bytes = n * 4;
	auto w32 = [&](uint32_t v) { std::fwrite(&v, 4, 1, f); };
	auto w16 = [&](uint16_t v) { std::fwrite(&v, 2, 1, f); };
	std::fwrite("RIFF", 1, 4, f); w32(36 + bytes); std::fwrite("WAVEfmt ", 1, 8, f);
	w32(16); w16(1); w16(2); w32((uint32_t)SR); w32((uint32_t)SR * 4); w16(4); w16(16);
	std::fwrite("data", 1, 4, f); w32(bytes);
	for (int i = 0; i < n; i++) {
		int16_t a = (int16_t)clampf(t.l[i] * gain * 32767.f, -32767.f, 32767.f);
		int16_t b = (int16_t)clampf(t.r[i] * gain * 32767.f, -32767.f, 32767.f);
		std::fwrite(&a, 2, 1, f); std::fwrite(&b, 2, 1, f);
	}
	std::fclose(f);
}

static float peakAbs(const Take& t) { float p = 0; for (size_t i = 0; i < t.l.size(); i++) p = std::max(p, std::max(std::fabs(t.l[i]), std::fabs(t.r[i]))); return p; }
static float rmsDb(const Take& t, float a, float b) {
	int ia = (int)(a * SR), ib = std::min((int)(b * SR), (int)t.l.size());
	double s = 0; for (int i = ia; i < ib; i++) s += 0.5 * (t.l[i] * t.l[i] + t.r[i] * t.r[i]);
	return 10.f * std::log10(s / std::max(1, ib - ia) + 1e-20);
}

static std::vector<double> spectrum(const std::vector<float>& x, int a, int N) {
	std::vector<std::complex<double>> v(N);
	for (int i = 0; i < N; i++) {
		double w = 0.5 - 0.5 * std::cos(2 * M_PI * i / (N - 1));
		v[i] = (a + i < (int)x.size() ? x[a + i] : 0.f) * w;
	}
	for (int i = 1, j = 0; i < N; i++) { int bit = N >> 1; for (; j & bit; bit >>= 1) j ^= bit; j ^= bit; if (i < j) std::swap(v[i], v[j]); }
	for (int len = 2; len <= N; len <<= 1) {
		std::complex<double> wl = std::polar(1.0, -2 * M_PI / len);
		for (int i = 0; i < N; i += len) { std::complex<double> w = 1; for (int j = 0; j < len / 2; j++) { auto u = v[i + j], q = v[i + j + len / 2] * w; v[i + j] = u + q; v[i + j + len / 2] = u - q; w *= wl; } }
	}
	std::vector<double> p(N / 2);
	for (int i = 0; i < N / 2; i++) p[i] = std::norm(v[i]);
	return p;
}
static double centroid(const Take& t, float a, float len) {
	int N = 1; while (N < len * SR) N <<= 1;
	auto p = spectrum(t.l, (int)(a * SR), N);
	double s = 0, w = 0; for (int i = 1; i < N / 2; i++) { double f = i * SR / N; s += f * p[i]; w += p[i]; }
	return s / w;
}
// share of energy above f (dB relative to all), first part of a note
static double highShare(const Take& t, float a, float len, double f) {
	int N = 1; while (N < len * SR) N <<= 1;
	auto p = spectrum(t.l, (int)(a * SR), N);
	double hi = 0, all = 0; for (int i = 1; i < N / 2; i++) { all += p[i]; if (i * SR / N > f) hi += p[i]; }
	return 10 * std::log10(hi / all + 1e-30);
}
// level (dB) of the strongest bin near frequency f over a window
static double partialDb(const Take& t, double f, float a, int N) {
	auto p = spectrum(t.l, (int)(a * SR), N);
	int k = (int)std::round(f * N / SR);
	double best = 0; for (int i = std::max(1, k - 3); i <= std::min(N / 2 - 1, k + 3); i++) best = std::max(best, p[i]);
	return 10 * std::log10(best + 1e-30);
}
static double peakNear(const Take& t, double f, float a, int N) {
	auto p = spectrum(t.l, (int)(a * SR), N);
	int k = (int)std::round(f * N / SR), bi = k; double best = 0;
	for (int i = std::max(1, k - 12); i <= std::min(N / 2 - 2, k + 12); i++) if (p[i] > best) { best = p[i]; bi = i; }
	// parabolic interpolation on dB
	double a1 = std::log(p[bi - 1] + 1e-30), b = std::log(p[bi] + 1e-30), c = std::log(p[bi + 1] + 1e-30);
	double d = 0.5 * (a1 - c) / (a1 - 2 * b + c);
	return (bi + d) * SR / N;
}

static const char* NAMES[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
static std::string noteName(int m) { return std::string(NAMES[m % 12]) + std::to_string(m / 12 - 1); }

int main(int argc, char** argv) {
#if defined(__x86_64__) || defined(__i386__)
	_MM_SET_FLUSH_ZERO_MODE(_MM_FLUSH_ZERO_ON);
	_MM_SET_DENORMALS_ZERO_MODE(_MM_DENORMALS_ZERO_ON);
#endif
	std::string out = argc > 1 ? argv[1] : "test/out";
	Controls c;
	const float G = 1.0f;   // WAV gain

	// 1. single notes across the range, mezzo-forte, held 3 s
	{
		int keys[] = {21, 28, 36, 43, 48, 55, 60, 67, 72, 79, 84, 91, 96, 103, 108};
		std::vector<Ev> evs; float t = 0.2f;
		for (int k : keys) { evs.push_back({t, (float)k, 0.55f, 2.6f}); t += 3.f; }
		Take tk = play(c, evs, t + 0.5f);
		writeWav(out + "/01-single-notes-A0-to-C8.wav", tk, G);
		std::printf("\nSingle notes, mf: level 0-0.3 s, 0.5-1 s, 2-2.5 s (dB), centroid 0-0.25 s\n");
		for (size_t i = 0; i < evs.size(); i++) {
			float s = evs[i].t;
			std::printf("  %-4s %6.1f %6.1f %6.1f   %6.0f Hz\n", noteName(keys[i]).c_str(), rmsDb(tk, s, s + 0.3f), rmsDb(tk, s + 0.5f, s + 1.f), rmsDb(tk, s + 2.f, s + 2.5f), centroid(tk, s, 0.25f));
		}
		std::printf("  peak %.2f\n", peakAbs(tk));
	}
	// 2. velocity ladder on C4 and on C2
	for (int key : {60, 36}) {
		float vels[] = {0.12f, 0.3f, 0.5f, 0.72f, 0.95f};
		std::vector<Ev> evs; float t = 0.2f;
		for (float v : vels) { evs.push_back({t, (float)key, v, 2.2f}); t += 2.5f; }
		Take tk = play(c, evs, t + 0.3f);
		writeWav(out + "/02-velocity-" + noteName(key) + "-pp-to-ff.wav", tk, G);
		std::printf("\nVelocity ladder %s: level 0-0.3 s (dB), energy above 1.5 kHz in first 0.2 s (dB re all)\n", noteName(key).c_str());
		double h0 = 0, h4 = 0, l0 = 0, l4 = 0;
		for (int i = 0; i < 5; i++) {
			double hs = highShare(tk, evs[i].t, 0.2f, 1500.0), lv = rmsDb(tk, evs[i].t, evs[i].t + 0.3f);
			if (i == 0) { h0 = hs; l0 = lv; } if (i == 4) { h4 = hs; l4 = lv; }
			std::printf("  vel %.2f  %6.1f dB  %6.1f dB\n", vels[i], lv, hs);
		}
		CHECK(h4 - h0 > 12.0, "%s ff is clearly brighter than pp (+%.1f dB above 1.5 kHz)", noteName(key).c_str(), h4 - h0);
		CHECK(l4 - l0 > 25.0, "%s pp to ff spans a piano's dynamic range (%.1f dB)", noteName(key).c_str(), l4 - l0);
	}
	// 3. partial frequencies of A1 (stretched by inharmonicity) and decay of C4
	{
		Take tk = play(c, {{0.05f, 33.f, 0.6f, 6.f}}, 6.f);
		float f0 = 55.f * std::pow(2.f, stretchCents(33) / 1200.f);
		float B = inharmB(33);
		std::printf("\nA1 partials (Hz): measured vs ideal harmonic vs stiff-string (B=%.2e)\n", B);
		double worst = 0;
		for (int k : {1, 2, 4, 8, 12, 16, 20}) {
			double fm = peakNear(tk, k * f0 * std::sqrt(1 + B * k * k) / std::sqrt(1 + B), 0.3f, 1 << 17);
			double fs = k * f0 * std::sqrt(1 + B * k * k) / std::sqrt(1 + B);
			std::printf("  k=%2d  %8.2f  %8.2f  %8.2f\n", k, fm, k * f0, fs);
			if (k > 1) worst = std::max(worst, std::fabs(1200 * std::log2(fm / fs)));
		}
		CHECK(worst < 10.0, "A1 partials follow the stiff-string law (worst %.1f cents)", worst);
	}
	{
		Take tk = play(c, {{0.05f, 60.f, 0.55f, 9.f}}, 9.f);
		double f1 = 261.63 * std::pow(2.0, stretchCents(60) / 1200.0);
		std::printf("\nC4 held, level of partials 1, 2, 4, 8 over time (dB)\n  t(s)    p1     p2     p4     p8\n");
		double first[4] = {}, last[4] = {};
		float ts[] = {0.1f, 0.5f, 1.f, 2.f, 3.f, 5.f, 7.f};
		for (int i = 0; i < 7; i++) {
			std::printf("  %4.1f", ts[i]);
			int ks[] = {1, 2, 4, 8};
			for (int j = 0; j < 4; j++) {
				double B = inharmB(60), fk = ks[j] * f1 * std::sqrt(1 + B * ks[j] * ks[j]);
				double d = partialDb(tk, fk, ts[i], 8192);
				if (i == 0) first[j] = d; if (i == 6) last[j] = d;
				std::printf(" %6.1f", d);
			}
			std::printf("\n");
		}
		double early = (partialDb(tk, f1, 0.1f, 8192) - partialDb(tk, f1, 1.1f, 8192));
		double late = (partialDb(tk, f1, 5.f, 8192) - partialDb(tk, f1, 7.f, 8192)) / 2.0;
		CHECK(early > 2.0 * late, "C4 fundamental has a two-stage decay (first second %.1f dB/s, later %.1f dB/s)", early, late);
		CHECK(first[3] - last[3] > first[0] - last[0], "upper partials die faster than the fundamental");
	}
	// 4. a chord
	{
		std::vector<Ev> evs = {{0.1f, 48, 0.55f, 5.f}, {0.1f, 55, 0.5f, 5.f}, {0.1f, 64, 0.52f, 5.f}, {0.1f, 71, 0.55f, 5.f}, {0.1f, 74, 0.5f, 5.f}};
		Take tk = play(c, evs, 6.5f);
		writeWav(out + "/03-chord-Cmaj9.wav", tk, G);
		std::printf("\nChord peak %.2f\n", peakAbs(tk));
	}
	// 5. a short phrase (gospel-ish ii-V-I with pedal), dry and with the room
	for (int rv = 0; rv < 2; rv++) {
		std::vector<Ev> evs;
		std::vector<PedalEv> ped;
		float bpm = 84.f, b = 60.f / bpm;
		auto chord = [&](float beat, std::vector<int> ns, float vel, float len) {
			for (size_t i = 0; i < ns.size(); i++) evs.push_back({beat * b + 0.008f * i, (float)ns[i], vel * (0.92f + 0.05f * (i % 3)), len * b});
		};
		auto note = [&](float beat, int n, float vel, float len) { evs.push_back({beat * b, (float)n, vel, len * b}); };
		// bar 1: Dm9 (left hand D2 A2, right hand F4 A4 C5 E5)
		chord(0, {38, 45}, 0.55f, 3.8f); chord(0.02f, {65, 69, 72, 76}, 0.45f, 1.8f);
		note(2, 77, 0.6f, 0.5f); note(2.5f, 76, 0.5f, 0.5f); note(3, 74, 0.55f, 1.f);
		// bar 2: G13 (G1 F2, B3 E4 F4 A4)
		chord(4, {31, 41}, 0.6f, 3.8f); chord(4.02f, {59, 64, 65, 69}, 0.48f, 1.8f);
		note(6, 71, 0.62f, 0.5f); note(6.5f, 74, 0.55f, 0.5f); note(7, 79, 0.7f, 1.f);
		// bar 3-4: Cmaj9 spread, then a high run
		chord(8, {36, 43, 52}, 0.6f, 7.5f); chord(8.03f, {62, 64, 67, 71}, 0.5f, 3.5f);
		int run[] = {84, 83, 79, 76, 74, 72, 71, 67};
		for (int i = 0; i < 8; i++) note(12 + 0.25f * i, run[i], 0.42f + 0.03f * (i % 2), 0.3f);
		chord(14, {60, 64, 67, 71, 74}, 0.38f, 3.f);
		ped = {{0.f, true}, {4.f * b - 0.05f, false}, {4.f * b + 0.05f, true}, {8.f * b - 0.05f, false}, {8.f * b + 0.05f, true}, {17.f * b, false}};
		Controls cc = c; cc.reverb = rv ? 0.5f : 0.f;
		Take tk = play(cc, evs, 17.f * b + 2.f, ped);
		writeWav(out + (rv ? "/05-phrase-with-room.wav" : "/04-phrase-dry.wav"), tk, G);
		std::printf("\nPhrase %s peak %.2f\n", rv ? "with room" : "dry", peakAbs(tk));
		CHECK(peakAbs(tk) < 1.0f, "phrase does not clip (%.2f)", peakAbs(tk));
	}
	// 6. CPU: 16 notes held (a big pedalled chord) for 3 s
	{
		std::unique_ptr<Engine> e(new Engine());
		e->setSampleRate(SR);
		e->setSustain(true);
		for (int i = 0; i < 16; i++) e->noteOn(i, 36.f + 3 * i, 0.7f, c);
		int n = (int)(3 * SR);
		auto t0 = std::chrono::high_resolution_clock::now();
		float acc = 0;
		for (int i = 0; i < n; i++) { e->process(c); acc += e->outL; }
		double secs = std::chrono::duration<double>(std::chrono::high_resolution_clock::now() - t0).count();
		int oscs = 0; for (auto& v : e->voices) if (v.active) oscs += v.n;
		std::printf("\nCPU: 16 pedalled notes, 3 s audio in %.3f s (%.1f%% of one core), %d modes left at end (acc %g)\n", secs, 100 * secs / 3, oscs, acc);
		CHECK(secs / 3 < 0.25, "16 notes under 25%% of a core (%.1f%%)", 100 * secs / 3);
	}
	std::printf("\n%s (%d failed)\n", fails ? "SOME CHECKS FAILED" : "all checks passed", fails);
	return fails ? 1 : 0;
}
