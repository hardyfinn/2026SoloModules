// Offline checks of the Krka engine (no Rack needed): make test
// Prints PASS/FAIL lines and writes test/chain.csv (four outputs of a looped
// chain) for the guide's picture.
#include <cstdio>
#include <cstdlib>
#include "../src/KrkaDSP.hpp"

using namespace krka;

static int fails = 0;
static void check(bool ok, const char* what) {
	printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
	if (!ok) fails++;
}

struct Rig {
	Channel ch[NUM_CH];
	ChSettings s[NUM_CH];
	int algo = 0;
	unsigned started = 0;
	int ends[NUM_CH] = {};
	void start(int c) {
		if (started & (1u << c)) return;
		started |= 1u << c;
		if (!ch[c].trigger(s[c])) return;
		for (int f = 0; f < NUM_CH; f++)
			if ((ALGOS[algo].next[c] & (1u << f)) && s[f].phase <= 0.f) start(f);
	}
	void step(float dt, const bool* trig, const bool* gate) {
		started = 0;
		for (int c = 0; c < NUM_CH; c++)
			if (trig && trig[c] && takesOwnTrigger(algo, c)) start(c);
		for (int c = 0; c < NUM_CH; c++) {
			Step st = ch[c].process(s[c], dt, gate ? gate[c] : false);
			for (int f = 0; f < NUM_CH; f++)
				if ((ALGOS[algo].next[c] & (1u << f)) && st.passed(s[f].phase)) start(f);
			if (st.end) ends[c]++;
		}
	}
};

int main() {
	const float sr = 48000.f, dt = 1.f / sr;
	bool t1[NUM_CH] = {true, false, false, false};

	// 1. one-shot AD: rises, falls back to 0, stops
	{
		Rig r; r.s[0].shape = 2; r.s[0].time = 0.1f;
		r.step(dt, t1, nullptr);
		float peak = 0.f;
		for (int i = 0; i < (int) (0.2f * sr); i++) { r.step(dt, nullptr, nullptr); peak = std::max(peak, r.ch[0].value); }
		check(peak > 0.99f && r.ch[0].value < 0.01f && !r.ch[0].running && r.ends[0] == 1, "one-shot AD peaks and returns to zero");
	}
	// 2. repeats
	{
		Rig r; r.s[0].shape = 10; r.s[0].time = 0.05f; r.s[0].repeats = 2;
		r.step(dt, t1, nullptr);
		int peaks = 0; float prev = 0.f;
		for (int i = 0; i < (int) (0.3f * sr); i++) {
			r.step(dt, nullptr, nullptr);
			if (r.ch[0].value > 0.5f && prev <= 0.5f) peaks++;
			prev = r.ch[0].value;
		}
		check(peaks == 3 && r.ends[0] == 1, "REPEAT 2 plays three times, one END");
	}
	// 3. chain A>B at 50 %
	{
		Rig r; r.algo = 1;
		r.s[0].shape = 20; r.s[0].time = 0.1f; r.s[1].shape = 10; r.s[1].time = 0.1f;
		r.s[1].phase = 0.5f;  // B starts half way through A
		r.step(dt, t1, nullptr);
		int startB = -1;
		for (int i = 0; i < (int) (0.3f * sr); i++) {
			r.step(dt, nullptr, nullptr);
			if (startB < 0 && r.ch[1].running) startB = i;
		}
		check(std::abs(startB - (int) (0.05f * sr)) < 3, "A>B: B starts when A is half way (PHASE 50 %)");
	}
	// 4. B ignores its own trigger when chained
	{
		Rig r; r.algo = 1; bool tb[NUM_CH] = {false, true, false, false};
		r.s[1].time = 0.1f;
		r.step(dt, tb, nullptr);
		check(!r.ch[1].running && takesOwnTrigger(8, 0) && !takesOwnTrigger(8, 1), "chained channel ignores its own TRIG, loop head still listens");
	}
	// 5. loop A>B>C>D keeps going; write CSV
	{
		Rig r; r.algo = 11;
		int shapes[NUM_CH] = {10, 130, 52, 220};
		for (int c = 0; c < NUM_CH; c++) { r.s[c].shape = shapes[c]; r.s[c].time = 0.25f; r.s[c].phase = 0.75f; }
		FILE* f = fopen("test/chain.csv", "w");
		r.step(dt, t1, nullptr);
		for (int i = 0; i < (int) (3.f * sr); i++) {
			r.step(dt, nullptr, nullptr);
			if (f && i % 48 == 0) fprintf(f, "%.3f,%.4f,%.4f,%.4f,%.4f\n", i / sr, r.ch[0].value, r.ch[1].value, r.ch[2].value, r.ch[3].value);
		}
		if (f) fclose(f);
		check(r.ends[0] >= 3 && r.ends[3] >= 3, "LOOP A>B>C>D cycles on its own");
	}
	// 6. loop with every PHASE at 0 does not hang
	{
		Rig r; r.algo = 8;
		r.s[0].phase = 0.f; r.s[1].phase = 0.f; r.s[0].time = r.s[1].time = 0.01f;
		r.step(dt, t1, nullptr);
		for (int i = 0; i < 4800; i++) r.step(dt, nullptr, nullptr);
		check(true, "LOOP with PHASE 0 everywhere stays safe");
	}
	// 7. gate holds at the peak
	{
		Rig r; r.s[0].shape = 20; r.s[0].time = 0.1f; r.s[0].gateMode = true;  // ADSR 0
		bool g[NUM_CH] = {true, false, false, false};
		r.step(dt, t1, g);
		for (int i = 0; i < (int) (0.5f * sr); i++) r.step(dt, nullptr, g);
		bool held = r.ch[0].running && r.ch[0].value > 0.99f;
		for (int i = 0; i < (int) (0.2f * sr); i++) r.step(dt, nullptr, nullptr);
		check(held && !r.ch[0].running, "GATE holds at the peak, finishes after release");
	}
	// 8. analog retrigger has no jump
	{
		Rig r; r.s[0].shape = 5; r.s[0].time = 0.2f; r.s[0].analog = true;
		r.step(dt, t1, nullptr);
		for (int i = 0; i < (int) (0.08f * sr); i++) r.step(dt, nullptr, nullptr);
		float before = r.ch[0].value;
		r.step(dt, t1, nullptr);
		float after = r.ch[0].value;
		check(before > 0.2f && std::fabs(after - before) < 0.05f, "ANALOG retrigger carries on from the current level");
		Rig d; d.s[0] = r.s[0]; d.s[0].analog = false;
		d.step(dt, t1, nullptr);
		for (int i = 0; i < (int) (0.08f * sr); i++) d.step(dt, nullptr, nullptr);
		d.step(dt, t1, nullptr);
		check(d.ch[0].value < 0.05f, "DIGITAL retrigger jumps back to the start");
	}
	// 9. every shape evaluates inside 0..1
	{
		bool ok = true;
		for (int s = 0; s < NUM_SHAPES; s++)
			for (int i = 0; i <= 1000; i++) {
				float v = shapeValue(s, i / 1000.f);
				if (!(v >= 0.f && v <= 1.f)) ok = false;
			}
		check(ok, "all 260 shapes stay inside 0..1");
	}
	printf("%s\n", fails ? "SOME CHECKS FAILED" : "all checks passed");
	return fails ? 1 : 0;
}
