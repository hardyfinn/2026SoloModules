// Offline checks for the Juke engine, no Rack needed:  make test
//  1. Thousands of generated / mutated / filled bars stay valid and keep the style rules.
//  2. Prints seed 42 (density 0.55, auto feel) to compare with the browser prototype.
//  3. Renders 16 evolving bars at 160 BPM to test/juke-demo.wav and checks the audio.
#include "../src/JukeDSP.hpp"
#include <cstdio>
#include <vector>
#include <string>
using namespace juke;

static void print(const Pattern& p) {
	for (int l = 0; l < NUM_LANES; l++) { char t[17]; toText(p, l, t); std::printf("%-11s%s\n", LANE_NAMES[l], t); }
}
static void wavWrite(const char* path, const std::vector<float>& s, int sr) {
	FILE* f = std::fopen(path, "wb"); if (!f) return;
	int n = (int) s.size(), bytes = n * 2;
	auto w32 = [&](uint32_t v) { std::fwrite(&v, 4, 1, f); };
	auto w16 = [&](uint16_t v) { std::fwrite(&v, 2, 1, f); };
	std::fwrite("RIFF", 1, 4, f); w32(36 + bytes); std::fwrite("WAVEfmt ", 1, 8, f);
	w32(16); w16(1); w16(1); w32(sr); w32(sr * 2); w16(2); w16(16);
	std::fwrite("data", 1, 4, f); w32(bytes);
	for (float x : s) { int v = (int) std::lround(std::max(-1.f, std::min(1.f, x)) * 32767.f); w16((uint16_t) (int16_t) v); }
	std::fclose(f);
}

int main(int argc, char** argv) {
	int bad = 0;
	Rng r;
	for (uint32_t s = 1; s < 5000; s++) {
		Pattern p; r.seed(s);
		generate(p, r, (s % 101) / 100.0, s % 3);
		for (int k = 0; k < 3; k++) {
			if (k == 1) mutate(p, r, 1.0);
			if (k == 2) fillBar(p, r);
			for (int l = 0; l < NUM_LANES; l++) for (int i = 0; i < STEPS; i++) if (p.s[l][i] > 2) bad++;
			if (k < 2) {
				if (p.count(KICK, 1) && p.s[KICK][0] != 1) { bad++; std::printf("seed %u: no downbeat kick\n", s); }
				for (int i = 1; i < STEPS; i++) if (p.s[CLAP][i] == 1 && p.s[KICK][i] == 1) { bad++; std::printf("seed %u: kick on clap\n", s); }
				int kicks = 0; for (int i = 1; i < STEPS; i++) kicks += p.s[KICK][i] == 1;
				if (kicks > 6) { bad++; std::printf("seed %u: %d kicks\n", s, kicks); }
			}
		}
	}
	// mutating a kick-less preset must not invent kicks
	for (int s = 0; s < 500; s++) { Pattern p; loadPreset(12, p); r.seed(s + 7); mutate(p, r, 1.0); if (p.count(KICK, 1) + p.count(KICK, 2)) bad++; }

	// metric shift timing: clock 16th = 1000 samples; request 4:3 from clock step 2 to step 22
	{
		for (int ri = 0; ri < NUM_RATIOS; ri++) {
			MetricShift ms; long lastFire = -1; int stepsAt = 0; bool engagedOnBeat = true, gapsOk = true, landsOk = true;
			const Ratio& rr = RATIOS[ri];
			double want = 1000.0 * rr.b / rr.a;
			bool prevActive = false;
			for (long t = 0; t < 48 * 1000; t++) {
				bool tickNow = (t % 1000) == 0;
				long ck = t / 1000;
				bool w = ck >= 2 && ck < 22, re;
				int n = ms.process(tickNow, 1000.f, w, ri, true, re);
				if (ms.active && !prevActive && (ms.masterStep() % 4)) engagedOnBeat = false;
				if (!ms.active && prevActive && ms.masterStep() != 0) landsOk = false;
				for (int k = 0; k < n; k++) {
					if (ms.active && prevActive && lastFire >= 0 && std::fabs((t - lastFire) - want) > 1.5) gapsOk = false;
					lastFire = t; stepsAt++;
				}
				prevActive = ms.active;
			}
			bool ok = engagedOnBeat && gapsOk && landsOk;
			if (!ok) bad++;
			std::printf("shift %s: %d steps in 48 clock 16ths, on beat %s, even gaps %s, returns on bar %s\n", rr.name, stepsAt,
				engagedOnBeat ? "yes" : "NO", gapsOk ? "yes" : "NO", landsOk ? "yes" : "NO");
		}
	}
	std::printf("pattern and shift checks: %d problems\n\n", bad);

	Pattern p; r.seed(42); generate(p, r, 0.55, FEEL_AUTO);
	std::printf("seed 42, density 0.55, auto:\n"); print(p);

	// 16 evolving bars of "Intermediate" at 160 BPM
	const int sr = 48000;
	DrumEngine d; d.setSampleRate((float) sr);
	Seq q; loadPreset(1, q.base); q.play = q.base; q.evolve = true; q.vary = 0.5;
	int stepLen = (int) std::lround(sr * 60.0 / 160.0 / 4.0);
	std::vector<float> out;
	float peak = 0.f; bool nanSeen = false;
	for (int st = 0; st < 16 * 16; st++) {
		uint8_t lv[NUM_LANES]; q.next(lv);
		for (int l = 0; l < NUM_LANES; l++) if (lv[l]) d.hit(l, lv[l]);
		for (int n = 0; n < stepLen; n++) {
			float v[NUM_LANES]; d.process(v);
			float m = 0.f; for (int l = 0; l < NUM_LANES; l++) { m += v[l]; if (!std::isfinite(v[l])) nanSeen = true; }
			m = std::tanh(m * 0.5f);
			peak = std::max(peak, std::fabs(m)); out.push_back(m * 0.9f);
		}
	}
	// each voice alone: peak level
	std::printf("\nvoice peaks:");
	for (int l = 0; l < NUM_LANES; l++) {
		DrumEngine e; e.setSampleRate((float) sr); e.hit(l, 1); float pk = 0.f;
		for (int n = 0; n < sr; n++) { float v[NUM_LANES]; e.process(v); pk = std::max(pk, std::fabs(v[l])); if (!std::isfinite(v[l])) nanSeen = true; }
		std::printf(" %s %.2f", LANE_NAMES[l], pk);
	}
	std::printf("\nmix peak %.2f, NaN %s\n", peak, nanSeen ? "YES" : "no");

	// metric shift demo: "Juke drive", bars 1-2 straight, 4:3 shift for bars 3-4, back on bar 5, repeated
	{
		DrumEngine e; e.setSampleRate((float) sr);
		Seq q2; loadPreset(5, q2.base); q2.play = q2.base;
		MetricShift ms;
		const float per = sr * 60.f / 160.f / 4.f;
		std::vector<float> o2;
		float phase2 = 1.f;
		for (long t = 0; t < (long) (per * 16 * 12); t++) {
			phase2 += 1.f / per;
			bool tk = false;
			if (phase2 >= 1.f) { phase2 -= std::floor(phase2); tk = true; }
			long bar2 = (ms.master + (tk ? 1 : 0)) / 16;
			bool re;
			int n = ms.process(tk, per, (bar2 % 6) >= 2 && (bar2 % 6) < 4, 4, true, re);
			if (re) { q2.step = 0; q2.wrapHeld = false; }
			q2.holdBar = ms.active;
			for (int k = 0; k < n; k++) { uint8_t lv[NUM_LANES]; q2.next(lv); for (int l = 0; l < NUM_LANES; l++) if (lv[l]) e.hit(l, lv[l]); }
			float v[NUM_LANES]; e.process(v);
			float m = 0.f; for (int l = 0; l < NUM_LANES; l++) m += v[l];
			o2.push_back(std::tanh(m * 0.5f) * 0.9f);
		}
		std::string d2 = argc > 1 ? argv[1] : ".";
		wavWrite((d2 + "/test/juke-shift-demo.wav").c_str(), o2, sr);
	}
	std::string dir = argc > 1 ? argv[1] : ".";
	wavWrite((dir + "/test/juke-demo.wav").c_str(), out, sr);
	return (bad || nanSeen) ? 1 : 0;
}
