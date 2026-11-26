// Renders the horns of src/dsp/Horn.hpp. ./test/render_horn grid DIR  (sustained notes for tools/horn_check.py)
//                                         ./test/render_horn demo DIR  (played phrases)
#include "../src/dsp/Horn.hpp"
#include <cstdio>
#include <string>
#include <chrono>
#include <functional>
static void writeWav(const std::string& path, const std::vector<float>& x, int sr, int ch = 1) {
	FILE* f = fopen(path.c_str(), "wb"); if (!f) return;
	auto u32 = [&](uint32_t v) { fwrite(&v, 4, 1, f); }; auto u16 = [&](uint16_t v) { fwrite(&v, 2, 1, f); };
	uint32_t data = (uint32_t)x.size() * 4;
	fwrite("RIFF", 1, 4, f); u32(36 + data); fwrite("WAVE", 1, 4, f);
	fwrite("fmt ", 1, 4, f); u32(16); u16(3); u16(ch); u32(sr); u32(sr * 4 * ch); u16(4 * ch); u16(32);
	fwrite("data", 1, 4, f); u32(data); fwrite(x.data(), 4, x.size(), f); fclose(f);
}

// ---- a tiny player for the demos: notes in beats, swung eighths, the articulation a horn player would use
struct Ev { double beat, len; double pitch; double vel; bool slur; double tongue, scoop, fall, vib, growl, air; bool gliss; };
struct Phrase { std::string name; int kind; double bpm; double swing; double room; std::vector<Ev> ev; double (*breath)(double) = nullptr; };
static Ev N(double beat, double len, double pitch, double vel = 0.6, bool slur = false, double tongue = 0.5) { return {beat, len, pitch, vel, slur, tongue, 0, 0, 0, 0, 0, false}; }
static void renderPhrase(const Phrase& ph, const std::string& dir, int SR, FILE* log) {
	horn::Horn h; h.setSampleRate(SR); h.setKind((horn::Kind)ph.kind); h.rng.seed(4242);
	horn::Room room; room.setSampleRate(SR);
	double spb = 60.0 / ph.bpm, end = 0;
	auto T = [&](double beat) { double b = std::floor(beat * 2 + 1e-9) / 2, fr = beat - b; double eighth = std::fmod(b, 1.0) >= 0.5 ? 1 : 0;   // swing the off-beat eighths
		double base = std::floor(beat); double pos = beat - base; double sw = pos < 0.5 ? pos * (ph.swing / 0.5) : ph.swing + (pos - 0.5) * ((1 - ph.swing) / 0.5); (void)fr; (void)eighth; return (base + sw) * spb + 0.3; };
	for (auto& e : ph.ev) end = std::max(end, T(e.beat + e.len));
	int NS = (int)((end + 1.6) * SR); std::vector<float> out(NS * 2);
	size_t ei = 0; int offAt = -1; double offFall = 0; double vibT = 0, growlT = 0, airT = 0, vib = 0;
	for (int i = 0; i < NS; i++) {
		double t = (double)i / SR;
		while (ei < ph.ev.size() && T(ph.ev[ei].beat) <= t) {
			const Ev& e = ph.ev[ei]; horn::Note n; n.pitch = e.pitch; n.vel = e.vel; n.slur = e.slur; n.tongue = e.tongue; n.scoop = e.scoop; n.gliss = e.gliss;
			h.noteOn(n); vibT = e.vib; growlT = e.growl; airT = e.air;
			double offT = T(e.beat + e.len); bool nextSlur = ei + 1 < ph.ev.size() && ph.ev[ei + 1].slur && T(ph.ev[ei + 1].beat) <= offT + 0.02;
			offAt = nextSlur ? -1 : (int)(offT * SR); offFall = e.fall;
			if (log) fprintf(log, "%s %d %.4f %.4f %.2f\n", ph.name.c_str(), ph.kind, t, offT, e.pitch);
			ei++;
		}
		if (i == offAt) { h.noteOff(offFall); offAt = -1; }
		if (ph.breath) h.breath = ph.breath(t);
		vib += (vibT - vib) * 0.0005; h.vibrato = vib; h.growl = growlT; h.air = airT;
		double y = h.process() * 0.5, L, R; room.process(y, ph.room, L, R);
		out[2 * i] = (float)L; out[2 * i + 1] = (float)R;
	}
	writeWav(dir + "/" + ph.name + ".wav", out, SR, 2);
}
static double centsOff(const std::vector<float>& x, int a, int b, double fexp, int SR) {       // autocorrelation pitch against fexp
	int n = b - a; double best = -1e9, bl = 0; int lo = (int)(SR / (fexp * 2.3)), hi = (int)(SR / (fexp / 2.3)); std::vector<double> ac(hi + 2);
	for (int l = lo; l <= hi + 1; l++) { double s = 0; for (int i = a; i < b - l; i++) s += x[i] * x[i + l]; ac[l] = s / (n - l); if (ac[l] > best) best = ac[l]; }
	for (int l = lo + 1; l <= hi; l++) if (ac[l] > ac[l - 1] && ac[l] >= ac[l + 1] && ac[l] > 0.8 * best) { double d = 0.5 * (ac[l - 1] - ac[l + 1]) / (ac[l - 1] - 2 * ac[l] + ac[l + 1]); bl = l + d; break; }
	return bl > 0 ? 1200 * std::log2((SR / bl) / fexp) : 9999.0;
}
static void demos(const std::string& dir, int SR);
#include "horn_demos.h"
int main(int argc, char** argv) {
	std::string mode = argc > 1 ? argv[1] : "grid", dir = argc > 2 ? argv[2] : "test/out/horn";
	const int SR = 48000;
	if (mode == "grid") {
		double secs = 0, cpu = 0;
		const double BR[3] = {0.2, 0.5, 0.85};
		for (int k = 0; k < horn::NUM_KINDS; k++)
			for (int p = horn::RANGE_LO[k]; p <= horn::RANGE_TOP[k]; p += 4)
				for (int b = 0; b < 3; b++) {
					horn::Horn h; h.setSampleRate(SR); h.setKind((horn::Kind)k); h.rng.seed(1234 + p * 7 + b);
					horn::Note n; n.pitch = p; n.vel = BR[b]; n.tongue = 0.5;
					int N = (int)(1.5 * SR); std::vector<float> x(N);
					auto t0 = std::chrono::steady_clock::now();
					h.noteOn(n);
					for (int i = 0; i < N; i++) { if (i == (int)(1.2 * SR)) h.noteOff(); x[i] = (float)h.process(); }
					cpu += std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count(); secs += 1.5;
					char name[64]; snprintf(name, sizeof name, "/g_%d_%d_%d.wav", k, p, b); writeWav(dir + name, x, SR);
				}
		printf("grid: %.0f s of audio, CPU %.2f%% of one core\n", secs, 100 * cpu / secs);
	}
	if (mode == "sweep") {      // every semitone, five dynamics, three tongues: for the pitch/regime check
		const double BR[5] = {0.1, 0.3, 0.5, 0.7, 0.95};
		int kk = argc > 3 ? atoi(argv[3]) : 0;
		for (int p = horn::RANGE_LO[kk]; p <= horn::RANGE_TOP[kk]; p++)
			for (int b = 0; b < 5; b++) {
				horn::Horn h; h.setSampleRate(SR); h.setKind((horn::Kind)kk); h.rng.seed(99 + p * 13 + b);
				horn::Note n; n.pitch = p; n.vel = BR[b]; n.tongue = (b * 7 + p) % 3 * 0.5;
				int N = (int)(1.0 * SR); std::vector<float> x(N);
				h.noteOn(n);
				for (int i = 0; i < N; i++) x[i] = (float)h.process();
				char name[64]; snprintf(name, sizeof name, "/g_%d_%d_%d.wav", kk, p, b); writeWav(dir + name, x, SR);
			}
	}
	if (mode == "demo") demos(dir, SR);
	if (mode == "long") {       // held notes: does a 5 s note stay in its register, plain, with vibrato, growled and breathy?
		int kk = argc > 3 ? atoi(argv[3]) : 0; int bad = 0, total = 0; double worst = 0, peak = 0;
		for (int p = horn::RANGE_LO[kk]; p <= horn::RANGE_TOP[kk]; p += 2)
			for (int v = 0; v < 3; v++) for (int var = 0; var < 4; var++) {
				horn::Horn h; h.setSampleRate(SR); h.setKind((horn::Kind)kk); h.rng.seed(1000 + p * 37 + v * 11 + var);
				horn::Note n; n.pitch = p; n.vel = 0.4 + 0.3 * v; n.tongue = 0.6; h.noteOn(n);
				h.vibrato = var == 1 ? 1.0 : 0.0; h.growl = var == 2 ? 0.8 : 0.0; h.air = var == 3 ? 0.8 : 0.0;
				int N = 5 * SR; std::vector<float> x(N);
				for (int i = 0; i < N; i++) { x[i] = (float)h.process(); peak = std::max(peak, (double)std::fabs(x[i])); }
				double fexp = horn::hz(p); bool ok = true; double w = 0;
				for (double t = 0.3; t < 4.8; t += 0.45) { double c = centsOff(x, (int)(t * SR), (int)((t + 0.12) * SR), fexp, SR); double lim = var == 2 ? 500 : (var == 1 ? 70 : 30); if (std::fabs(c) > lim) ok = false; else w = std::max(w, std::fabs(c)); }
				total++; if (!ok) { bad++; if (bad <= 12) printf("  bad held %d vel %.1f var %d\n", p, n.vel, var); } else if (var != 1 && var != 2) worst = std::max(worst, w);
			}
		printf("long %s: %d bad of %d, worst error (plain/breathy) %.1f cents, peak %.2f\n", horn::KIND_NAMES[kk], bad, total, worst, peak);
	}
	if (mode == "trans") {      // note-to-note transitions: does the second note land in the right register, early and late?
		int kk = argc > 3 ? atoi(argv[3]) : 0; int bad = 0, total = 0; double worst = 0;
		for (int p1 = horn::RANGE_LO[kk]; p1 <= horn::RANGE_TOP[kk]; p1 += 3)
			for (int p2 = horn::RANGE_LO[kk]; p2 <= horn::RANGE_TOP[kk]; p2 += 2)
				for (int v = 0; v < 3; v++) for (int art = 0; art < 4; art++) {
					horn::Horn h; h.setSampleRate(SR); h.setKind((horn::Kind)kk); h.rng.seed(7 + p1 * 31 + p2 * 17 + v * 5 + art);
					horn::Note n; n.pitch = p1; n.vel = 0.3 + 0.3 * v; n.tongue = 0.5; h.noteOn(n);
					int N1 = (int)(0.25 * SR), N = (int)(0.85 * SR); std::vector<float> x(N);
					for (int i = 0; i < N; i++) {
						if (art == 3 && i == N1 - (int)(0.018 * SR)) h.noteOff();      // released just before, as a line of separate notes is
						if (i == N1) { n.pitch = p2; n.slur = art == 0; n.tongue = art == 2 ? 1.0 : 0.3; h.noteOn(n); }
						x[i] = (float)h.process();
					}
					double fexp = horn::hz(p2);
					double c1 = centsOff(x, N1 + (int)(0.07 * SR), N1 + (int)(0.17 * SR), fexp, SR), c2 = centsOff(x, N1 + (int)(0.45 * SR), N1 + (int)(0.58 * SR), fexp, SR);
					total++; if (std::fabs(c1) > 60 || std::fabs(c2) > 30) { bad++; if (bad <= 25) printf("  bad %d -> %d vel %.1f art %d: early %+.0f late %+.0f\n", p1, p2, n.vel, art, c1, c2); }
					else worst = std::max(worst, std::fabs(c2));
				}
		printf("trans %s: %d bad of %d, worst late error among good %.1f cents\n", horn::KIND_NAMES[kk], bad, total, worst);
	}
	return 0;
}
