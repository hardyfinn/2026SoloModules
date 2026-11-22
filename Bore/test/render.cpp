// Offline checks for the Bore engines + demo WAVs. No Rack needed.
//   make test   (writes test/scanner-demo.wav, test/boreroom-demo.wav)
#include "../src/BoreDSP.hpp"
#include <cstdio>
#include <vector>
#include <string>
#include <cstring>
#include <xmmintrin.h>

using namespace bore;
static const float SR = 48000.f;
static int fails = 0;
#define CHECK(c, msg) do { if (!(c)) { std::printf("FAIL: %s\n", msg); fails++; } else std::printf("ok:   %s\n", msg); } while (0)

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
		int16_t a = (int16_t) std::max(-32767.f, std::min(32767.f, L[i] * 0.9f * 32767.f));
		int16_t b = (int16_t) std::max(-32767.f, std::min(32767.f, R[i] * 0.9f * 32767.f));
		std::fwrite(&a, 2, 1, f); std::fwrite(&b, 2, 1, f);
	}
	std::fclose(f);
}

struct Stats { float rms, peak; };
static Stats stats(const std::vector<float>& v, size_t from = 0) {
	double a = 0; float p = 0; size_t n = 0;
	for (size_t i = from; i < v.size(); i++) { a += v[i] * v[i]; p = std::max(p, std::fabs(v[i])); n++; }
	Stats s; s.rms = n ? (float) std::sqrt(a / n) : 0.f; s.peak = p; return s;
}

// dominant frequency of the X coil (zero-crossing count of the gradient) during scanning
static void renderMode(ScannerEngine& e, ScanControls& c, int mode, float secs, std::vector<float>& L, std::vector<float>& R, Stats* st = nullptr) {
	c.mode = mode;
	std::vector<float> l, r;
	int n = (int) (secs * SR);
	for (int i = 0; i < n; i++) {
		e.process(c, 0.f, false, i == 0);
		l.push_back(e.outL); r.push_back(e.outR);
	}
	if (st) *st = stats(l, n / 4);
	L.insert(L.end(), l.begin(), l.end());
	R.insert(R.end(), r.begin(), r.end());
}

int main(int argc, char** argv) {
	// Rack runs its engine with denormals flushed to zero; do the same here
	_mm_setcsr(_mm_getcsr() | 0x8040);
	std::string dir = argc > 1 ? argv[1] : ".";
	// ---------- bore geometry
	{
		BoreRoom b; b.init(SR);
		float f[BORE_MODES];
		b.modeFreqs(0.60f, 1.60f, f);
		std::printf("60 cm bore modes:");
		for (int i = 0; i < BORE_MODES; i++) std::printf(" %.0f", f[i]);
		std::printf(" Hz\n");
		CHECK(f[0] > 320 && f[0] < 350, "first cross mode of a 60 cm tube ~335 Hz");
		CHECK(f[6] > 85 && f[6] < 110, "first length mode ~95 Hz");
	}
	// ---------- bore impulse response: decays, stays finite
	{
		BoreRoom b; b.init(SR);
		BoreParams p;
		std::vector<float> l;
		for (int i = 0; i < (int) SR * 2; i++) { float o1, o2; b.process(i == 0 ? 1.f : 0.f, i == 0 ? 1.f : 0.f, p, o1, o2); l.push_back(o1); }
		Stats early = stats(std::vector<float>(l.begin() + 1, l.begin() + 4800));
		Stats late = stats(std::vector<float>(l.begin() + 72000, l.end()));
		std::printf("bore IR: early rms %.5f, after 1.5 s rms %.7f\n", early.rms, late.rms);
		CHECK(std::isfinite(early.rms) && late.rms < early.rms * 0.01f, "bore tail decays by 40 dB within 1.5 s");
	}
	// ---------- EPI pitch follows 1 / (2 ES)
	{
		ScannerEngine e; e.init(SR);
		ScanControls c; c.mode = EPI; c.plant = 0.f;
		int cross = 0; float prev = 0.f; int samples = 0;
		for (int i = 0; i < (int) SR; i++) {
			e.process(c, 0.f, false, i == 0);
			if (e.scanning && e.pulses.size() > 0) {
				// count sign changes of the X readout train only while it runs
				float gx = e.g[0];
				if ((gx > 0.3f && prev < -0.3f) || (gx < -0.3f && prev > 0.3f)) cross++;
				if (std::fabs(gx) > 0.3f) prev = gx;
			}
			if (e.scanning) samples++;
		}
		std::printf("EPI: %d readout flips in %d scanning samples\n", cross, samples);
		CHECK(cross > 100, "EPI readout alternates");
	}
	// ---------- every mode makes sound, nothing blows up
	ScannerEngine e; e.init(SR);
	ScanControls c;
	c.plant = 0.35f; c.slices = 10;
	std::vector<float> L, R;
	for (int m = 0; m < NUM_MODES; m++) {
		Stats st;
		float secs = (m == T2_TSE || m == FLAIR || m == DIFFUSION) ? 9.f : 5.f;
		renderMode(e, c, m, secs, L, R, &st);
		std::printf("%-14s rms %.3f peak %.3f\n", MODE_NAMES[m], st.rms, st.peak);
		char msg[96]; std::snprintf(msg, sizeof msg, "%s is audible and finite", MODE_NAMES[m]);
		CHECK(std::isfinite(st.rms) && st.rms > 0.005f && st.peak <= 1.f, msg);
	}
	// ---------- 3 T vs 1.5 T loudness
	{
		ScannerEngine a; a.init(SR); ScanControls ca; ca.plant = 0.f; ca.field = 0.f;
		ScannerEngine b; b.init(SR); ScanControls cb; cb.plant = 0.f; cb.field = 0.5f;
		std::vector<float> l1, r1, l2, r2; Stats s1, s2;
		renderMode(a, ca, T1_SE, 3.f, l1, r1, &s1);
		renderMode(b, cb, T1_SE, 3.f, l2, r2, &s2);
		std::printf("T1 rms 1.5 T %.3f, 3 T %.3f\n", s1.rms, s2.rms);
		CHECK(s2.rms > s1.rms * 1.3f, "3 T is louder than 1.5 T");
	}
	// ---------- silent mode is quieter than gradient echo
	{
		ScannerEngine a; a.init(SR); ScanControls ca; ca.plant = 0.f;
		std::vector<float> l1, r1, l2, r2; Stats s1, s2;
		renderMode(a, ca, GRE, 3.f, l1, r1, &s1);
		ScannerEngine b; b.init(SR);
		renderMode(b, ca, SILENT, 3.f, l2, r2, &s2);
		std::printf("GRE rms %.3f, SILENT rms %.3f\n", s1.rms, s2.rms);
		CHECK(s2.rms < s1.rms * 0.5f, "silent mode is much quieter");
	}
	// ---------- earplugs darken
	{
		ScannerEngine a; a.init(SR); ScanControls ca; ca.plant = 0.f;
		ScannerEngine b; b.init(SR); ScanControls cb = ca; cb.bore.ears = 1; cb.bore.body = 0.f;
		std::vector<float> l1, r1, l2, r2;
		renderMode(a, ca, T1_SE, 3.f, l1, r1);
		renderMode(b, cb, T1_SE, 3.f, l2, r2);
		// high band energy via first difference
		double h1 = 0, h2 = 0, e1 = 0, e2 = 0;
		for (size_t i = 1; i < l1.size(); i++) { float d1 = l1[i] - l1[i - 1], d2 = l2[i] - l2[i - 1]; h1 += d1 * d1; h2 += d2 * d2; e1 += l1[i] * l1[i]; e2 += l2[i] * l2[i]; }
		h1 /= e1; h2 /= e2;
		std::printf("high-band share open %.4f, earplugs %.4f\n", h1, h2);
		CHECK(h2 < h1 * 0.5f, "earplugs cut the highs");
	}
	// ---------- session runs and advances
	{
		ScannerEngine a; a.init(SR); ScanControls ca; ca.session = 5.f;
		int eos = 0; int tr = 0;
		for (int i = 0; i < (int) (SR * 60); i++) { a.process(ca, 0.f, false, false); if (a.trigEOS) eos++; if (a.trigTR) tr++; }
		std::printf("session: %d series ends, %d TRs in the first minute, now at step %d (%s)\n", eos, tr, a.sessStep, a.stepName());
		CHECK(eos >= 3, "session moves through table, prescan and localiser in a minute");
	}
	// ---------- clocked TR
	{
		ScannerEngine a; a.init(SR); ScanControls ca; ca.clocked = true; ca.mode = T1_SE; ca.slices = 4;
		int tr = 0; int per = (int) (SR * 0.25f);
		for (int i = 0; i < (int) SR * 4; i++) { a.process(ca, 0.f, i % per == 0, false); if (a.trigTR) tr++; }
		std::printf("clocked: %d TRs from 16 clocks\n", tr);
		CHECK(tr >= 15 && tr <= 17, "one TR per clock");
	}
	writeWav((dir + "/test/scanner-demo.wav").c_str(), L, R);

	// ---------- a session excerpt "symphony": localiser, T2, EPI, diffusion with pump and earplugs
	{
		ScannerEngine a; a.init(SR); ScanControls ca; ca.plant = 0.5f; ca.slices = 12; ca.drift = 0.3f; ca.human = 0.f;
		ca.bore.ears = 0; ca.bore.body = 0.4f;
		std::vector<float> l, r;
		const int seq[6] = {PRESCAN, LOCALISER, T1_SE, T2_TSE, EPI, DIFFUSION};
		const float len[6] = {4.f, 5.f, 8.f, 10.f, 10.f, 12.f};
		for (int k = 0; k < 6; k++) {
			renderMode(a, ca, seq[k], len[k], l, r);
			// 2 s of gap with only the pump
			ca.mode = seq[k];
			for (int i = 0; i < (int) (SR * 2); i++) {
				ScanControls cp = ca; cp.lx = cp.ly = cp.lz = 0.f;
				a.process(cp, 0.f, false, false);
				l.push_back(a.outL); r.push_back(a.outR);
			}
		}
		writeWav((dir + "/test/scanner-symphony.wav").c_str(), l, r);
	}
	// ---------- Bore Room on a drum-ish loop
	{
		BoreRoom b; b.init(SR);
		BoreParams p; p.size = 0.5f; p.decay = 0.45f;
		std::vector<float> l, r; Rng rng;
		float env = 0, ph = 0, nenv = 0;
		for (int i = 0; i < (int) (SR * 12); i++) {
			int step = (int) (i / (SR * 0.125f));
			if (i % (int) (SR * 0.125f) == 0) {
				if (step % 4 == 0) env = 1.f;
				if (step % 2 == 1) nenv = 0.6f;
			}
			ph += (45.f + 120.f * env) / SR; if (ph > 1.f) ph -= 1.f;
			float x = std::sin(2 * PI_F * ph) * env * 0.8f + rng.bi() * nenv * 0.3f;
			env *= 0.9997f; nenv *= 0.9993f;
			p.size = i < SR * 4 ? 0.f : (i < SR * 8 ? 0.5f : 1.f);
			float o1, o2; b.process(x, x, p, o1, o2);
			float dryAmt = 0.f;
			l.push_back(std::tanh(o1 + x * dryAmt)); r.push_back(std::tanh(o2 + x * dryAmt));
		}
		writeWav((dir + "/test/boreroom-demo.wav").c_str(), l, r);
	}
	std::printf(fails ? "\n%d check(s) FAILED\n" : "\nall checks passed\n", fails);
	return fails ? 1 : 0;
}
