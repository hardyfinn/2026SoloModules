// House Keys host test program (spec section 12), loudness trims (spec 2) and demos (spec 10).
//   hktest test            run the tests
//   hktest trims FILE      measure every sound and write the trims header
//   hktest demos DIR       render the demo phrases and gallery.wav (44.1 kHz)
//   hktest one N DIR       render sound N (1..100) phrase to DIR
#include "../src/dsp/Core.hpp"
#include "../src/generated/Sounds.hpp"
#include <array>
#if defined(__x86_64__)
#include <xmmintrin.h>
#include <pmmintrin.h>
#endif
#include <chrono>
#include <cstdarg>
#include <complex>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <string>
#include <vector>

using namespace hk;

// ------------------------------------------------------------------ helpers
static void writeWav(const std::string& path, const std::vector<float>& L, const std::vector<float>& R, int sr) {
	FILE* f = std::fopen(path.c_str(), "wb");
	if (!f) { std::printf("cannot write %s\n", path.c_str()); return; }
	uint32_t n = (uint32_t) L.size();
	uint32_t dataBytes = n * 4;
	auto w32 = [&](uint32_t v) { std::fwrite(&v, 4, 1, f); };
	auto w16 = [&](uint16_t v) { std::fwrite(&v, 2, 1, f); };
	std::fwrite("RIFF", 1, 4, f); w32(36 + dataBytes); std::fwrite("WAVE", 1, 4, f);
	std::fwrite("fmt ", 1, 4, f); w32(16); w16(1); w16(2); w32(sr); w32(sr * 4); w16(4); w16(16);
	std::fwrite("data", 1, 4, f); w32(dataBytes);
	for (uint32_t i = 0; i < n; i++) {
		// 10 V = full scale
		int16_t a = (int16_t) std::lround(clampf(L[i] / 10.f, -1.f, 1.f) * 32767.f);
		int16_t b = (int16_t) std::lround(clampf(R[i] / 10.f, -1.f, 1.f) * 32767.f);
		w16((uint16_t) a); w16((uint16_t) b);
	}
	std::fclose(f);
}

static float dbfs(float volts) { return 20.f * std::log10(std::max(volts / 10.f, 1e-12f)); }

// one note or chord event in a phrase: start and length in seconds
struct Ev { double t, len; float note; float vel; int ch; };

// Drives a Core with V/OCT, GATE and VEL per channel, block by block.
struct Rig {
	Core core;
	CoreParams p;
	float fs;
	Rig(float sr, int sound) : fs(sr) {
		core.init(sr, true);
		p.sound = sound;
		p.level = 0.35355339f; // output gain x1: 0 dBFS = 10 V
	}
	void render(const std::vector<Ev>& evs, double seconds, std::vector<float>& L, std::vector<float>& R, bool velPatched = true) {
		int n = (int) (seconds * fs);
		L.assign(n, 0.f); R.assign(n, 0.f);
		float voct[MAX_CH], gate[MAX_CH], vel[MAX_CH];
		int nch = 1;
		for (auto& e : evs) nch = std::max(nch, e.ch + 1);
		for (int i = 0; i < MAX_CH; i++) { voct[i] = 0.f; gate[i] = 0.f; vel[i] = 0.f; }
		for (int i = 0; i < n; i += BLOCK) {
			double t = i / (double) fs;
			for (int c = 0; c < nch; c++) gate[c] = 0.f;
			for (auto& e : evs) {
				if (t >= e.t && t < e.t + e.len) {
					gate[e.ch] = 10.f; voct[e.ch] = (e.note - 60.f) / 12.f; vel[e.ch] = e.vel / 127.f * 10.f;
				}
			}
			core.setInputs(nch, voct, gate, velPatched ? vel : nullptr);
			int m = std::min(BLOCK, n - i);
			core.process(&L[i], &R[i], m, p);
		}
	}
};

static float peakOf(const std::vector<float>& x, size_t a = 0, size_t b = (size_t) -1) {
	float p = 0.f; b = std::min(b, x.size());
	for (size_t i = a; i < b; i++) p = std::max(p, std::fabs(x[i]));
	return p;
}
static double rmsOf(const std::vector<float>& x, size_t a, size_t b) {
	double s = 0; b = std::min(b, x.size()); if (b <= a) return 0;
	for (size_t i = a; i < b; i++) s += (double) x[i] * x[i];
	return std::sqrt(s / (b - a));
}

// McLeod normalised square difference pitch estimate (Hz); 0 if none
static double pitchNsdf(const std::vector<float>& x, size_t a, size_t n, double fs, double fmin = 25, double fmax = 4000) {
	if (a + 2 * n > x.size()) n = (x.size() - a) / 2;
	int maxLag = std::min<int>((int) (fs / fmin), (int) n - 1), minLag = (int) (fs / fmax);
	std::vector<double> r(maxLag + 2, 0.0);
	for (int tau = 0; tau <= maxLag + 1; tau++) {
		double ac = 0, m = 0;
		for (size_t i = 0; i < n; i++) { double u = x[a + i], v = x[a + i + tau]; ac += u * v; m += u * u + v * v; }
		r[tau] = m > 0 ? 2 * ac / m : 0;
	}
	// key maxima after the first negative zero crossing
	double best = -1; int bl = 0;
	std::vector<std::pair<int, double>> peaks;
	int tau = minLag;
	while (tau < maxLag && r[tau] > 0) tau++;
	for (; tau < maxLag; tau++) {
		if (r[tau] > 0) {
			int pk = tau; double pv = r[tau];
			while (tau < maxLag && r[tau] > 0) { if (r[tau] > pv) { pv = r[tau]; pk = tau; } tau++; }
			peaks.push_back({pk, pv});
			best = std::max(best, pv);
		}
	}
	if (peaks.empty()) return 0;
	for (auto& pk : peaks) if (pk.second >= 0.9 * best) { bl = pk.first; break; }
	double y0 = r[bl - 1], y1 = r[bl], y2 = r[bl + 1];
	double d = (y0 - 2 * y1 + y2) != 0 ? 0.5 * (y0 - y2) / (y0 - 2 * y1 + y2) : 0;
	return fs / (bl + d);
}

// magnitude spectrum peak search with a Hann window (direct DFT at chosen bins)
static double dftMag(const std::vector<float>& x, size_t a, size_t n, double f, double fs) {
	std::complex<double> acc = 0;
	for (size_t i = 0; i < n; i++) {
		double w = 0.5 - 0.5 * std::cos(2 * PI_D * i / (n - 1));
		acc += std::polar(w * x[a + i], -2 * PI_D * f * i / fs);
	}
	return std::abs(acc) / (n * 0.25);
}

// in-place radix-2 FFT
static void fft(std::vector<std::complex<double>>& a) {
	size_t n = a.size();
	for (size_t i = 1, j = 0; i < n; i++) { size_t bit = n >> 1; for (; j & bit; bit >>= 1) j ^= bit; j ^= bit; if (i < j) std::swap(a[i], a[j]); }
	for (size_t len = 2; len <= n; len <<= 1) {
		std::complex<double> wl = std::polar(1.0, -2 * PI_D / len);
		for (size_t i = 0; i < n; i += len) { std::complex<double> w = 1; for (size_t j = 0; j < len / 2; j++) { auto u = a[i + j], v = a[i + j + len / 2] * w; a[i + j] = u + v; a[i + j + len / 2] = u - v; w *= wl; } }
	}
}
// Hann-windowed magnitude spectrum of n = 2^k samples
static std::vector<double> spectrum(const std::vector<float>& x, size_t a, size_t n) {
	std::vector<std::complex<double>> c(n);
	for (size_t i = 0; i < n; i++) {
		double t = 2 * PI_D * i / n; // 4-term Blackman-Harris, sidelobes below -92 dB
		c[i] = (0.35875 - 0.48829 * std::cos(t) + 0.14128 * std::cos(2 * t) - 0.01168 * std::cos(3 * t)) * x[a + i];
	}
	fft(c);
	std::vector<double> m(n / 2);
	for (size_t i = 0; i < n / 2; i++) m[i] = std::abs(c[i]);
	return m;
}

static int failures = 0, checks = 0;
static void check(bool ok, const char* fmt, ...) {
	checks++;
	if (!ok) failures++;
	va_list ap; va_start(ap, fmt);
	std::printf(ok ? "  ok   " : "  FAIL ");
	std::vprintf(fmt, ap);
	std::printf("\n");
	va_end(ap);
}

static bool isInharmonic(int sound) {
	std::string n = soundName(sound);
	for (const char* k : {"Bell", "Chime", "AM Bells", "Orchestra", "Chord Stab", "Orch", "Metal"})
		if (n.find(k) != std::string::npos) return true;
	return false;
}

// ------------------------------------------------------------------ tests
static void testAllSounds(float fs, bool quick) {
	std::printf("All sounds: NaN, silence, DC, determinism, release\n");
	for (int s = 0; s < NUM_SOUNDS; s++) {
		int oct = soundOctave(s);
		for (int note : {36, 60, 84}) {
			if (quick && note != 60) continue;
			Rig r(fs, s);
			std::vector<float> L, R;
			r.render({{0.0, 1.5, (float) note, 100, 0}}, 2.0, L, R);
			bool nan = false;
			for (size_t i = 0; i < L.size(); i++) if (!std::isfinite(L[i]) || !std::isfinite(R[i])) nan = true;
			double rms = rmsOf(L, (size_t) (0.02 * fs), (size_t) (1.5 * fs));
			double mean = 0; for (float v : L) mean += v; mean /= L.size();
			bool ok = !nan && dbfs(rms) > -50.f && dbfs(std::fabs(mean)) < -50.f;
			if (!ok || note == 60)
				check(ok, "%3d %-20s note %d  rms %6.1f dBFS  dc %6.1f dBFS%s", s + 1, soundName(s), note, dbfs(rms), dbfs(std::fabs(mean)), nan ? " NaN" : "");
			(void) oct;
		}
	}
}

static void testDeterminism(float fs) {
	std::printf("Determinism\n");
	int bad = 0;
	for (int s = 0; s < NUM_SOUNDS; s++) {
		std::vector<float> L1, R1, L2, R2;
		{ Rig r(fs, s); r.p.twin = 0.4f; r.p.tape = 0.3f; r.render({{0.0, 0.5, 60, 100, 0}, {0.1, 0.4, 67, 90, 1}}, 0.8, L1, R1); }
		{ Rig r(fs, s); r.p.twin = 0.4f; r.p.tape = 0.3f; r.render({{0.0, 0.5, 60, 100, 0}, {0.1, 0.4, 67, 90, 1}}, 0.8, L2, R2); }
		if (std::memcmp(L1.data(), L2.data(), L1.size() * 4) || std::memcmp(R1.data(), R2.data(), R1.size() * 4)) {
			bad++; std::printf("  not bit-identical: %d %s\n", s + 1, soundName(s));
		}
	}
	check(bad == 0, "all 100 sounds bit-identical across two renders (%d differ)", bad);
}

static void testRelease(float fs) {
	std::printf("Note off: time to -60 dBFS after the gate falls\n");
	int bad = 0;
	for (int s = 0; s < NUM_SOUNDS; s++) {
		Rig r(fs, s);
		std::vector<float> L, R;
		double gateLen = 1.0, total = 12.0;
		r.render({{0.0, gateLen, (float) (soundOctave(s) <= -1 ? 36 : 60), 100, 0}}, total, L, R);
		size_t start = (size_t) (gateLen * fs), win = (size_t) (0.01 * fs);
		double tEnd = -1;
		for (size_t i = start; i + win < L.size(); i += win) {
			if (dbfs((float) std::max(rmsOf(L, i, i + win), rmsOf(R, i, i + win))) < -60.f) { tEnd = (i - start) / (double) fs; break; }
		}
		if (tEnd < 0 || tEnd > 8.0) { bad++; std::printf("  slow release %3d %-20s %s\n", s + 1, soundName(s), tEnd < 0 ? "> 11 s" : std::to_string(tEnd).c_str()); }
	}
	check(bad == 0, "every sound falls below -60 dBFS within 8 s of note off (%d slow)", bad);
}

// tuning spread a sound is programmed with: half of it is allowed on top of 5 cents
static double detuneAllowance(int sound) {
	int e, i; soundLocate(sound, e, i);
	switch (e) {
		case ENG_POLY: {
			const PolySound& p = POLY_SOUNDS[i];
			double a = p.lvl2 > 0 ? std::fabs(p.fine2) / 2 : 0;
			// short stabs measured inside a resonant filter sweep: the moving phase bends pitch a little
			if (!p.aenvGate && p.aenv[2] == 0.f && p.aenv[1] < 0.3f && p.env > 1.f) a += 5;
			return a;
		}
		case ENG_PD: { const PdSound& p = PD_SOUNDS[i]; return p.mode != 0 ? std::fabs(p.det[2]) / 2 : 0; }
		case ENG_WAVE: {
			const WaveSound& w = WAVE_SOUNDS[i]; double lo = 0, hi = 0;
			for (int k = 0; k < 3; k++) if (w.o[k].on) { double c = (w.o[k].semi == -1 ? -100.0 : 0.0) + w.o[k].fine * 3.125; if (std::fabs(c) < 100) { lo = std::min(lo, c); hi = std::max(hi, c); } }
			return (hi - lo) / 2;
		}
		case ENG_SMP: {
			const SmpSound& p = SMP_SOUNDS[i];
			double g = p.gen == GEN_STRINGS ? 5 : p.gen == GEN_CHOIR ? 6 : p.gen == GEN_PIANO ? 8 : p.gen == GEN_BRASS ? 3 : 0;
			return g + p.det / 2;
		}
		case ENG_FM: {
			FmVoiceData d = decodeVmem(FM_SOUNDS[i].vmem, FM_SOUNDS[i].opz);
			for (auto& o : d.op) if (o.det != 3 && o.out > 0) return 3; // DT1 detune on an operator
			return 0;
		}
		default: return 0;
	}
}

// WAVE sounds with an LFO on oscillator pitch: their pitch is meant to move
static bool programmedVibrato(int sound) {
	int e, i; soundLocate(sound, e, i);
	if (e != ENG_WAVE) return false;
	const WaveSound& w = WAVE_SOUNDS[i];
	for (int k = 0; k < 3; k++) {
		int src = w.o[k].pmSrc;
		if (w.o[k].on && src >= SRC_LFO1 && src <= SRC_LFO3 && w.o[k].pmDepth > 0) {
			const int* l = w.lfo[src - SRC_LFO1];
			if (l[2] > 0 || l[4] > 0) return true;
		}
	}
	return false;
}

static void testPitch(float fs) {
	std::printf("Pitch at A3 and A4 (octave-agnostic, 5 cents plus half the programmed detune)\n");
	int bad = 0, tested = 0;
	for (int s = 0; s < NUM_SOUNDS; s++) {
		if (isInharmonic(s)) continue;
		if (programmedVibrato(s)) { std::printf("  skip  %3d %-20s (LFO vibrato in the program)\n", s + 1, soundName(s)); continue; }
		for (int note : {57, 69}) {
			Rig r(fs, s);
			r.p.mod = 0.f; r.p.chorus = 1; // chorus off: its delay sweep bends pitch
			std::vector<float> L, R;
			r.render({{0.0, 1.2, (float) note, 100, 0}}, 1.2, L, R);
			std::vector<float> M(L.size());
			for (size_t i = 0; i < L.size(); i++) M[i] = L[i] + R[i];
			double ref = 440.0 * std::exp2((note - 69) / 12.0);
			double tol = 5.0 + detuneAllowance(s);
			bool ok = false; double c = 0, f = 0, bestErr = 1e9;
			// windows of 0.2 and 0.4 s: the loudest one, and the latest within 10 dB of it
			for (double wl : {0.2, 0.4}) {
				size_t win = (size_t) (wl * fs);
				std::vector<std::pair<size_t, double>> wr; double br = -1;
				for (size_t a = (size_t) (0.03 * fs); a + 2 * win < M.size(); a += win / 4) { double r2 = rmsOf(M, a, a + 2 * win); wr.push_back({a, r2}); br = std::max(br, r2); }
				size_t loud = 0, late = 0;
				for (auto& w : wr) { if (w.second == br) loud = w.first; if (w.second >= 0.316 * br) late = w.first; }
				for (size_t a : {loud, late}) {
					double fx = pitchNsdf(M, a, win, fs);
					double cents = fx > 0 ? 1200 * std::log2(fx / ref) : 9999;
					for (double iv : {0.0, 702.0, 498.0}) {
						double d = std::fmod(cents - iv + 120000.0, 1200.0); if (d > 600) d -= 1200;
						if (std::fabs(d) < bestErr) { bestErr = std::fabs(d); c = d; f = fx; }
						if (std::fabs(d) <= tol) ok = true;
					}
				}
			}
			tested++;
			if (!ok) { bad++; std::printf("  pitch %3d %-20s note %d: %.2f Hz (%.1f cents from an octave of %.2f, allowed %.1f)\n", s + 1, soundName(s), note, f, c, ref, tol); }
		}
	}
	check(bad == 0, "pitch within tolerance on %d renders (%d off)", tested, bad);
}

// peak of a sound at velocity 100 over the first 2 s (C2 for low sounds, else C4), in volts at gain x1
static float measurePeak(int s, float fs, bool trims, float trimOverride = 1e9f) {
	Rig r(fs, s);
	r.core.applyTrims = trims;
	if (trimOverride < 1e8f) r.core.trimDb[s] = trimOverride;
	std::vector<float> L, R;
	int note = soundOctave(s) <= -1 ? 36 : 60;
	r.render({{0.0, 1.8, (float) note, 100, 0}}, 2.0, L, R);
	return std::max(peakOf(L), peakOf(R));
}

static void testTrims(float fs) {
	std::printf("Trimmed peaks (target -12 dBFS +-1.5 dB)\n");
	int bad = 0;
	for (int s = 0; s < NUM_SOUNDS; s++) {
		float pk = dbfs(measurePeak(s, fs, true));
		if (std::fabs(pk + 12.f) > 1.5f) { bad++; std::printf("  %3d %-20s %.2f dBFS\n", s + 1, soundName(s), pk); }
	}
	check(bad == 0, "trimmed peaks within 1.5 dB (%d outside)", bad);
}

static void writeTrims(const char* path, float fs) {
	std::vector<float> t(NUM_SOUNDS);
	for (int s = 0; s < NUM_SOUNDS; s++) {
		float pk = measurePeak(s, fs, false);
		t[s] = -12.f - dbfs(pk);
		// the chorus input and the soft limit are nonlinear: iterate
		for (int it = 0; it < 3; it++) t[s] += -12.f - dbfs(measurePeak(s, fs, true, t[s]));
		std::printf("%3d %-20s peak %6.2f dBFS  trim %+6.2f dB\n", s + 1, soundName(s), dbfs(pk), t[s]);
	}
	FILE* f = std::fopen(path, "w");
	std::fprintf(f, "// Loudness trims in dB per sound (spec 2). Generated by `make trims`.\n#pragma once\nnamespace hk {\nstatic const float SOUND_TRIM_DB[100] = {\n");
	for (int s = 0; s < NUM_SOUNDS; s++) std::fprintf(f, "\t%.3ff,%s // %d %s\n", t[s], "", s + 1, soundName(s));
	std::fprintf(f, "};\n}\n");
	std::fclose(f);
}

// a pure sine FM voice: ALG 7, only OP1 sounding at ratio 1.00
static void sineVoice(uint8_t* v) {
	std::memcpy(v, FM_SOUNDS[0].vmem, 128);
	for (int k = 0; k < 4; k++) {
		uint8_t* o = v + 10 * k;
		o[0] = 31; o[1] = 0; o[2] = 0; o[3] = 15; o[4] = 15; o[5] = 0; o[6] = 0;
		o[7] = (k == 3) ? 99 : 0; o[8] = 4; o[9] = 3;
	}
	v[40] = 7; v[41] = 0; v[43] = 0; v[44] = 0; v[45] = 0; v[46] = 24;
	for (int i = 73; i < 81; i++) v[i] = 0;
}

static void testFm(float fs) {
	std::printf("FM\n");
	{   // 440 Hz
		FmEngine e; e.init(fs); uint8_t v[128]; sineVoice(v); e.setVoice(v, false);
		Macros m; m.grit = 0.f;
		std::vector<float> L((size_t) fs), R((size_t) fs);
		e.noteOn(0, 69, 100, 0); e.process(L.data(), R.data(), (int) fs, m);
		double f = pitchNsdf(L, (size_t) (0.2 * fs), (size_t) (0.5 * fs), fs);
		check(std::fabs(1200 * std::log2(f / 440.0)) < 2.0, "note 69 sounds %.3f Hz (%.2f cents)", f, 1200 * std::log2(f / 440.0));
	}
	{   // CRS table
		const float want[9] = {0.50f, 0.71f, 0.78f, 0.87f, 1.00f, 1.41f, 1.57f, 1.73f, 2.00f};
		bool ok = true;
		for (int i = 0; i < 9; i++) { int mul, dt2; float r; crsToMulDt2(i, mul, dt2, &r); if (std::fabs(r - want[i]) > 0.01f) ok = false; }
		check(ok, "CRS 0..8 give 0.50 0.71 0.78 0.87 1.00 1.41 1.57 1.73 2.00");
	}
	{   // Solid Bass decode
		FmVoiceData d = decodeVmem(FM_SOUNDS[0].vmem, FM_SOUNDS[0].opz);
		bool crsOk = true;
		for (int k = 0; k < 4; k++) { int mul, dt2; float r; crsToMulDt2(d.op[k].crs, mul, dt2, &r); if (std::fabs(r - 1.f) > 0.01f && std::fabs(r - 0.5f) > 0.01f) crsOk = false; }
		check(d.alg == 1 && d.fb == 7 && d.trps == 12 && d.op[0].out == 70 && d.op[3].out == 99 && crsOk,
			"Solid Bass: ALG %d (stored), FBL %d, TRPS %d, OP4 OUT %d, OP1 OUT %d, CRS 1.00/0.50 %s",
			d.alg, d.fb, d.trps - 24, d.op[0].out, d.op[3].out, crsOk ? "yes" : "no");
	}
	{   // GRIT 0 vs 0.5 noise floor of a decaying note
		auto tail = [&](float grit) {
			FmEngine e; e.init(fs); e.setSound(20); // E Piano 1, held: a long decay
			Macros m; m.grit = grit;
			std::vector<float> L((size_t) (4 * fs)), R(L.size());
			e.noteOn(0, 60, 100, 0);
			e.process(L.data(), R.data(), (int) L.size(), m);
			return L;
		};
		auto a = tail(0.f), b = tail(0.5f);
		// error between the two renders late in the decay, relative to the signal there
		size_t i0 = (size_t) (3.0 * fs), i1 = (size_t) (4.0 * fs);
		std::vector<float> d(a.size());
		for (size_t i = 0; i < a.size(); i++) d[i] = b[i] - a[i];
		double sig = rmsOf(a, i0, i1), err = rmsOf(d, i0, i1);
		check(err > 1e-6 && sig > 0, "GRIT 0.5 adds a converter error floor late in a decay (signal %.1f dB, error %.1f dB re 1.0)", 20 * std::log10(sig + 1e-12), 20 * std::log10(err + 1e-12));
		double rel = 20 * std::log10(err / sig);
		check(rel > -80 && rel < -40, "the error floor sits %.1f dB under the signal (a 10-bit floating-point converter gives about -60)", rel);
	}
	{   // loaded bank path
		uint8_t bank[32][128];
		for (int i = 0; i < 32; i++) std::memcpy(bank[i], FM_SOUNDS[(i % 30)].vmem, 128);
		std::vector<uint8_t> sx = {0xF0, 0x43, 0x00, 0x04, 0x20, 0x00};
		int sum = 0;
		for (int i = 0; i < 32; i++) for (int j = 0; j < 128; j++) { sx.push_back(bank[i][j]); sum += bank[i][j]; }
		sx.push_back((uint8_t) ((-sum) & 0x7F)); sx.push_back(0xF7);
		std::string path = "/tmp/hk_bank_test.syx";
		FILE* f = std::fopen(path.c_str(), "wb"); std::fwrite(sx.data(), 1, sx.size(), f); std::fclose(f);
		f = std::fopen(path.c_str(), "rb");
		std::vector<uint8_t> rd(8192); size_t got = std::fread(rd.data(), 1, rd.size(), f); rd.resize(got); std::fclose(f);
		uint8_t out[32][128];
		int n = parseVmemBank(rd, out);
		auto render = [&](bool user) {
			FmEngine e; e.init(fs);
			if (user) e.setVoice(out[3], FM_SOUNDS[3].opz); else e.setSound(3);
			Macros m;
			std::vector<float> L((size_t) fs), R(L.size());
			e.noteOn(0, 60, 100, 0); e.process(L.data(), R.data(), (int) fs, m);
			return L;
		};
		auto a = render(true), b = render(false);
		check(n == 32 && a == b, "loaded .syx voice renders bit-exactly like the built-in voice (%d voices parsed)", n);
	}
}

static void testPoly(float fs) {
	std::printf("POLY\n");
	{   // self-oscillation at res 1
		for (float cut : {500.f, 1000.f, 2000.f}) {
			PolySound ps = POLY_SOUNDS[0];
			ps.saw = ps.pulse = ps.tri = ps.sub = ps.noise = ps.lvl2 = 0.f;
			ps.res = 1.f; ps.cut = cut; ps.env = 0.f; ps.kbd = 0.f; ps.velcut = 0.f; ps.hpf = 1; ps.poles = 4;
			ps.aenvGate = true; ps.mono = false; ps.lfoPitch = 0.f;
			ps.noise = 0.0005f; // a whisper to start it
			PolyEngine e; e.init(fs); e.setSound(0); e.s = &ps;
			Macros m;
			std::vector<float> L((size_t) (2 * fs)), R(L.size());
			e.noteOn(0, 60, 100, 0); e.process(L.data(), R.data(), (int) L.size(), m);
			double f = pitchNsdf(L, (size_t) (1.2 * fs), (size_t) (0.3 * fs), fs, 50, 8000);
			check(std::fabs(f / cut - 1.0) < 0.03, "res 1, cut %.0f Hz: oscillates at %.1f Hz (%.1f%%)", cut, f, 100 * (f / cut - 1));
		}
	}
	{   // DCO models: no drift
		for (int s : {0, 15}) { // Juno-type and JX-type
			PolySound ps = POLY_SOUNDS[s];
			PolyEngine e; e.init(fs); e.setSound(s);
			Macros m;
			std::vector<float> L((size_t) (6 * fs)), R(L.size());
			e.noteOn(0, 57, 100, 0); e.process(L.data(), R.data(), (int) L.size(), m);
			double f1 = pitchNsdf(L, (size_t) (0.5 * fs), (size_t) (0.2 * fs), fs);
			double f2 = pitchNsdf(L, (size_t) (5.0 * fs), (size_t) (0.2 * fs), fs);
			check(ps.model == PM_JUP || std::fabs(1200 * std::log2(f2 / f1)) < 0.5, "%s (%s): pitch at 0.5 s %.3f Hz, at 5 s %.3f Hz",
				ps.name, ps.model == PM_JUNO ? "DCO" : ps.model == PM_JX ? "DCO" : "VCO", f1, f2);
		}
	}
	{   // chorus rates
		for (int mode : {1, 2}) {
			Chorus c; c.init(fs);
			double f0 = 100.0; int n = (int) (24 * fs);
			std::vector<float> L(n), R(n), X(n);
			for (int i = 0; i < n; i++) X[i] = L[i] = R[i] = 0.25f * (float) std::sin(2 * PI_D * f0 * i / fs);
			for (int i = 0; i < n; i += BLOCK) c.process(&L[i], &R[i], std::min(BLOCK, n - i), mode, 0.f);
			// wet only, I/Q demodulated phase -> delay
			int per = (int) (fs / f0);
			std::vector<double> d;
			for (int i = (int) (2 * fs); i + per < n; i += per) {
				double re = 0, im = 0;
				for (int k = 0; k < per; k++) { double w = L[i + k] / 0.7 - X[i + k]; re += w * std::cos(2 * PI_D * f0 * (i + k) / fs); im += w * std::sin(2 * PI_D * f0 * (i + k) / fs); }
				d.push_back(std::atan2(im, re));
			}
			double mean = 0; for (double v : d) mean += v; mean /= d.size();
			std::vector<double> cross;
			for (size_t i = 1; i < d.size(); i++) if (d[i - 1] - mean < 0 && d[i] - mean >= 0) cross.push_back(i - 1 + (mean - d[i - 1]) / (d[i] - d[i - 1]));
			double rate = cross.size() >= 2 ? (cross.size() - 1) / ((cross.back() - cross.front()) / f0) : 0;
			double want = mode == 1 ? 0.513 : 0.863;
			check(std::fabs(rate / want - 1) < 0.02, "chorus %s rate %.3f Hz (want %.3f)", mode == 1 ? "I" : "II", rate, want);
		}
	}
}

static void testPd(float fs) {
	std::printf("PD\n");
	(void) fs;
	for (int w : {PDW_SAW, PDW_SQUARE, PDW_PULSE}) {
		int N = 4096; std::vector<float> x(N);
		for (int i = 0; i < N; i++) x[i] = pdWave(w, (i * 8.0 / N) - std::floor(i * 8.0 / N), 0.f);
		double fund = dftMag(x, 0, N, 8, N), worst = 0;
		for (int h = 2; h <= 40; h++) worst = std::max(worst, dftMag(x, 0, N, 8.0 * h, N));
		check(20 * std::log10(worst / fund + 1e-12) < -60, "wave %d with a = 0: worst harmonic %.1f dB", w, 20 * std::log10(worst / fund + 1e-12));
	}
}

static void testWaveSmp(float fs) {
	std::printf("WAVE and SAMPLER\n");
	double imgGain = 0;
	for (double HZ : {1013.0, 1033.0, 1061.0}) {   // box integration vs nearest sample: 8-bit staircase near 1 kHz at the 2x engine rate
		const auto& tabs = waveTables();
		const Wave& W = tabs[0]; // SINE
		double fs2 = 2.0 * fs, hz = HZ, inc = hz * 256.0 / fs2;
		int band = waveBandFor((float) hz), N = 1 << 16;
		std::vector<float> box(N), nn(N);
		double ph = 0; bool done = false;
		for (int i = 0; i < N; i++) {
			box[i] = readOsc(W, band, ph, inc, 0.5f, done);
			nn[i] = W.band[band].q8[(int) ph % W.len] / 128.f;
			ph += inc; if (ph >= W.len) ph -= W.len;
		}
		// largest non-harmonic component (the folded images) relative to the fundamental
		auto worstImage = [&](const std::vector<float>& x) {
			auto m = spectrum(x, 0, N);
			double binHz = fs2 / N, fund = 0, w = 0;
			for (size_t k = 0; k < m.size(); k++) {
				double f = k * binHz, h = f / hz, dh = std::fabs(h - std::round(h)) * hz;
				if (std::round(h) == 1 && dh < 6 * binHz) fund = std::max(fund, m[k]);
				else if (dh >= 6 * binHz && f > 20 && f < 20000) w = std::max(w, m[k]);
			}
			return 20 * std::log10(w / fund + 1e-12);
		};
		double a = worstImage(box), b = worstImage(nn);
		imgGain += (b - a) / 3;
		std::printf("       images of a %.0f Hz staircase folded into 20 Hz..20 kHz: box %.1f dB, nearest sample %.1f dB\n", HZ, a, b);
	}
	check(imgGain >= 20, "box integration lowers folded images by %.1f dB on average (spec: 20)", imgGain);
	{
		int bad = 0; double worstT = 0; int worstMem = 0;
		for (int i = 0; i < 12; i++) {
			auto t0 = std::chrono::steady_clock::now();
			auto m = renderSmpSound(i);
			double t = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
			worstT = std::max(worstT, t); worstMem = std::max(worstMem, m->totalSamples);
			if (m->totalSamples > 131072 || t > 0.5) bad++;
		}
		check(bad == 0, "SAMPLER memory max %d samples (limit 131072), slowest render %.3f s", worstMem, worstT);
	}
}

static void testPerformance(float fs) {
	std::printf("Performance: 8 voices of the heaviest sound per engine, 48 kHz\n");
	const int heavy[5] = {2, 30, 54, 66, 88};
	for (int e = 0; e < 5; e++) {
		double worst = 0; int worstS = 0;
		int a = engineFirstSound(e), b = e < 4 ? engineFirstSound(e + 1) : 100;
		for (int s = a; s < b; s++) {
			Rig r(fs, s);
			std::vector<Ev> evs;
			for (int c = 0; c < 8; c++) evs.push_back({0.0, 2.0, (float) (48 + 3 * c), 100, c});
			std::vector<float> L, R;
			r.render({{0.0, 0.01, 60, 100, 0}}, 0.05, L, R); // warm up
			auto t0 = std::chrono::steady_clock::now();
			r.render(evs, 2.0, L, R);
			double t = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
			if (t > worst) { worst = t; worstS = s; }
			if (getenv("HK_PERF_ALL")) std::printf("       %-20s %.1f%%\n", soundName(s), 100 * t / 2.0);
		}
		(void) heavy;
		check(worst / 2.0 < 0.12, "%-4s heaviest %-20s %.1f%% of one core", engineName(e), soundName(worstS), 100 * worst / 2.0);
	}
}

// ------------------------------------------------------------------ demos
static void renderTo(const std::string& path, int sound, const std::vector<Ev>& evs, double seconds, std::function<void(CoreParams&)> tweak = nullptr) {
	Rig r(44100.f, sound);
	r.p.level = 0.4f; // a little under the default so chords stay clear of the limiter
	if (tweak) tweak(r.p);
	std::vector<float> L, R;
	r.render(evs, seconds, L, R);
	writeWav(path, L, R, 44100);
	std::printf("  wrote %s (peak %.2f V)\n", path.c_str(), std::max(peakOf(L), peakOf(R)));
}

// 16th-note bass riff at bpm; pattern of (step, note, len in steps)
static std::vector<Ev> riff(double bpm, int bars, const std::vector<std::array<int, 3>>& pat, int root, float vel = 110) {
	std::vector<Ev> e; double st = 60.0 / bpm / 4.0;
	for (int b = 0; b < bars; b++) for (auto& p : pat) e.push_back({(b * 16 + p[0]) * st, p[2] * st * 0.9, (float) (root + p[1]), vel, 0});
	return e;
}

static std::vector<Ev> stabs(double bpm, int bars, const std::vector<std::array<int, 2>>& steps, std::vector<int> chord, float vel = 100) {
	std::vector<Ev> e; double st = 60.0 / bpm / 4.0;
	for (int b = 0; b < bars; b++) for (auto& s : steps) {
		int ch = 0;
		for (int n : chord) e.push_back({(b * 16 + s[0]) * st, s[1] * st * 0.85, (float) n, vel, ch++});
	}
	return e;
}

static int findSound(const char* name) {
	for (int s = 0; s < NUM_SOUNDS; s++) if (std::string(soundName(s)) == name) return s;
	std::printf("no sound named %s\n", name);
	return 0;
}

static void demos(const std::string& dir, bool galleryToo) {
	double bpm = 123;
	std::vector<std::array<int, 3>> bassPat = {{0, 0, 2}, {3, 0, 1}, {6, 12, 1}, {8, 0, 2}, {10, 7, 1}, {11, 10, 2}, {14, 12, 1}};
	std::vector<std::array<int, 2>> stabPat = {{2, 1}, {6, 2}, {10, 1}, {13, 2}};
	auto spb = 60.0 / bpm * 4; // seconds per bar
	// FM: Solid Bass with TWIN, Jazz Organ with STAB MIN7, LatelyBass
	renderTo(dir + "/fm-solid-bass.wav", findSound("Solid Bass"), riff(bpm, 4, bassPat, 33), 4 * spb + 0.5, [](CoreParams& p) { p.twin = 0.3f; });
	renderTo(dir + "/fm-jazz-organ.wav", findSound("Jazz Organ"), stabs(bpm, 4, stabPat, {57}), 4 * spb + 0.5, [](CoreParams& p) { p.stab = 5; });
	renderTo(dir + "/fm-lately-bass.wav", findSound("LatelyBass"), riff(bpm, 4, bassPat, 33), 4 * spb + 0.5);
	// POLY: Feel Bass plus Feel Chords
	renderTo(dir + "/poly-feel-bass.wav", findSound("Feel Bass"), riff(bpm, 4, {{0, 0, 3}, {4, 0, 1}, {7, 3, 1}, {8, 0, 3}, {12, 5, 1}, {14, 7, 2}}, 33), 4 * spb + 0.5);
	renderTo(dir + "/poly-feel-chords.wav", findSound("Feel Chords"), stabs(bpm, 4, {{0, 6}, {8, 6}}, {57, 60, 64, 67}), 4 * spb + 1.5);
	if (!galleryToo) return;
	renderTo(dir + "/pd-demo.wav", 54, stabs(bpm, 4, stabPat, {57, 60, 64}), 4 * spb + 1.0);
	renderTo(dir + "/wave-demo.wav", 66, stabs(bpm, 4, {{0, 7}, {8, 7}}, {55, 62, 65, 70}), 4 * spb + 1.5);
	renderTo(dir + "/sampler-demo.wav", 88, stabs(bpm, 4, {{0, 15}}, {57, 64, 69, 72}), 4 * spb + 2.0);
	// gallery: every sound two bars
	std::vector<float> GL, GR;
	for (int s = 0; s < NUM_SOUNDS; s++) {
		Rig r(44100.f, s);
		r.p.level = 0.4f;
		std::vector<Ev> e;
		if (soundOctave(s) <= -1) e = riff(bpm, 2, bassPat, 36);
		else e = stabs(bpm, 2, {{0, 3}, {6, 2}, {10, 1}, {12, 4}}, {60, 63, 67, 70});
		std::vector<float> L, R;
		r.render(e, 2 * spb, L, R);
		GL.insert(GL.end(), L.begin(), L.end()); GR.insert(GR.end(), R.begin(), R.end());
	}
	writeWav(dir + "/gallery.wav", GL, GR, 44100);
	std::printf("  wrote %s/gallery.wav\n", dir.c_str());
}

int main(int argc, char** argv) {
#if defined(__x86_64__)
	// Rack runs its engine threads with flush-to-zero and denormals-are-zero
	_mm_setcsr(_mm_getcsr() | 0x8040);
#endif
	std::string cmd = argc > 1 ? argv[1] : "test";
	float fs = 48000.f;
	if (cmd == "trims") { writeTrims(argc > 2 ? argv[2] : "src/generated/Trims.hpp", fs); return 0; }
	if (cmd == "demos") { demos(argc > 2 ? argv[2] : "demos", true); return 0; }
	if (cmd == "early") { demos(argc > 2 ? argv[2] : "demos", false); return 0; }
	if (cmd == "one") {
		int s = std::atoi(argv[2]) - 1;
		std::string dir = argc > 3 ? argv[3] : ".";
		renderTo(dir + "/sound-" + std::to_string(s + 1) + ".wav", s, stabs(123, 2, {{0, 3}, {6, 2}, {10, 1}, {12, 4}}, {60, 63, 67, 70}), 4.5);
		return 0;
	}
	if (cmd == "perf") { testPerformance(fs); return failures ? 1 : 0; }
	bool quick = cmd == "quick";
	testFm(fs);
	testPoly(fs);
	testPd(fs);
	testWaveSmp(fs);
	testAllSounds(fs, quick);
	testPitch(fs);
	testTrims(fs);
	if (!quick) { testDeterminism(fs); testRelease(fs); testPerformance(fs); }
	std::printf("\n%d checks, %d failed\n", checks, failures);
	return failures ? 1 : 0;
}
