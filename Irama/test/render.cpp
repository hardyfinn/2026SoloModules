// Offline checks for the Irama / Gatra engines + an event list for the demo.
//   make test  ->  runs checks, writes test/events.csv (time,voice,volts,vel)
//   python3 test/render_demo.py test/events.csv irama-demo.wav
#include "../src/GamelanCore.hpp"
#include <cstdio>
#include <cassert>
#include <vector>
#include <string>
using namespace irama;

static const float SR = 48000.f;
static FILE* out;
static double T0 = 0.0;
static const char* VN[NUM_VOICES] = {"saron", "peking", "bonang", "polos", "sangsih"};
static const char* TN[NUM_TRIGS] = {"gong", "kenong", "kempul", "kethuk", "kempyang", "gatra", "kendang", "end"};
static const char* DN[NUM_DRUM] = {"kick", "dgong", "clap", "ohat", "ohat2", "rim", "hat", "shaker", "accent"};
static int fails = 0;
#define CHECK(c, msg) do { if (!(c)) { printf("FAIL: %s\n", msg); fails++; } } while (0)

struct Scene {
	IramaCore* ir; GatraCore* ga; float ombak; float root; double bpm16; double secs; bool selehBass;
	float swing;
};

// clock is 16ths at bpm16 (16ths per minute / 4 = bpm)
static double run(Scene sc, std::vector<std::pair<double, std::string> > cues = std::vector<std::pair<double, std::string> >()) {
	double per = SR * 60.0 / sc.bpm16;
	long n = (long)(sc.secs * SR);
	double nextClk = 0.0;
	size_t ci = 0;
	int gongs = 0, notes = 0, nanc = 0;
	for (long i = 0; i < n; i++) {
		double t = i / (double)SR;
		while (ci < cues.size() && cues[ci].first <= t) {
			const std::string& c = cues[ci].second;
			if (sc.ir) {
				if (c == "lvl0") sc.ir->levelTarget = 0;
				if (c == "lvl1") sc.ir->levelTarget = 1;
				if (c == "lvl2") sc.ir->levelTarget = 2;
				if (c == "lvl3") sc.ir->levelTarget = 3;
				if (c == "kebyar_on") sc.ir->kebyar = true;
				if (c == "kebyar_off") sc.ir->kebyar = false;
				if (c == "angsel") sc.ir->triggerAngsel();
				if (c == "suwuk") sc.ir->triggerSuwuk();
				if (c == "telu") sc.ir->kotekan = K_TELU;
				if (c == "nyogcag") sc.ir->kotekan = K_NYOGCAG;
				if (c == "empat") sc.ir->kotekan = K_EMPAT;
			}
			if (sc.ga) {
				if (c == "land0") sc.ga->land = 0.f;
				if (c == "land05") sc.ga->land = 0.5f;
				if (c == "land1") sc.ga->land = 1.f;
				if (c == "drop_on") sc.ga->drop = true;
				if (c == "drop_off") sc.ga->drop = false;
				if (c == "gtelu") sc.ga->kotekan = DK_TELU;
				if (c == "cyc20") sc.ga->cycle = 20;
			}
			ci++;
		}
		bool rise = false;
		if (i >= (long)nextClk) { rise = true; nextClk += per; }
		if (sc.ir) {
			IramaFrame f;
			sc.ir->tick(rise, f);
			for (int v = 0; v < NUM_VOICES; v++) if (f.fire[v]) {
				float vv = f.volts[v] + sc.root;
				if (!(vv > -5.f && vv < 5.f)) nanc++;
				fprintf(out, "%.5f,%s,%.5f,%.3f\n", T0 + t, VN[v], vv, sc.ombak);
				notes++;
			}
			for (int k = 0; k < NUM_TRIGS; k++) if (f.trig[k]) {
				if (k == T_GONG) gongs++;
				if (k == T_GATRA) continue;
				fprintf(out, "%.5f,%s,%.5f,%.3f\n", T0 + t, TN[k], sc.ir->poolVolts(sc.ir->balP[sc.ir->beat]) + sc.root, sc.ombak);
			}
			if (f.seleh && sc.selehBass) fprintf(out, "%.5f,bass,%.5f,0\n", T0 + t, f.selehVolts + sc.root);
		}
		if (sc.ga && rise) {
			GatraFrame g;
			sc.ga->clock(g);
			double dt = (g.odd && sc.swing > 0.5f) ? (sc.swing - 0.5f) * 2.0 * per / SR : 0.0;
			for (int k = 0; k < NUM_DRUM; k++) if (g.trig[k]) fprintf(out, "%.5f,%s,0,0\n", T0 + t + dt, DN[k]);
		}
	}
	CHECK(nanc == 0, "pitch out of range");
	printf("  %.1f s: %d notes, %d gongs\n", sc.secs, notes, gongs);
	T0 += sc.secs;
	return sc.secs;
}

int main(int argc, char** argv) {
	std::string dir = argc > 1 ? argv[1] : ".";
	out = fopen((dir + "/test/events.csv").c_str(), "w");

	// ---- checks: forms
	for (int st = 0; st < 2; st++) for (int fm = 0; fm < 4; fm++) {
		int N = formBeats(st, fm), g = 0;
		for (int b = 1; b <= N; b++) if (formMask(st, fm, b) & C_GONG) g++;
		CHECK(g == 1 && (formMask(st, fm, N) & C_GONG), "one gong per cycle, on the last beat");
	}
	// ---- checks: seleh landing tone of the gong gatra is the stressed tone; walk stays in range
	for (int m = 0; m < 8; m++) {
		IramaCore c; c.style = m < 6 ? JAVA : BALI; c.mode = m; c.form = 2; c.rng.seed(100 + m); c.buildAll();
		CHECK(modp(c.balP[c.N - 1], 5) == MODES[m].stressed, "gong tone = stressed tone");
		for (int i = 0; i < c.N; i++) CHECK(c.balP[i] >= 3 && c.balP[i] <= 11, "balungan range");
	}
	// ---- checks: gatra LAND moves the gong onto the downbeat
	{
		GatraCore g; g.cycle = 16; g.land = 1.f; GatraFrame f; int firstGong = -1;
		for (int i = 0; i < 64 * 3; i++) { g.clock(f); if (f.trig[D_GONG] && firstGong < 0) firstGong = i; if (f.trig[D_GONG]) CHECK(i % 64 == 0, "LAND 1: gong on the bar-1 downbeat"); }
		CHECK(firstGong == 0, "LAND 1 gong at start");
		g.reset(); g.land = 0.f;
		for (int i = 0; i < 64 * 2; i++) { g.clock(f); if (f.trig[D_GONG]) CHECK(i % 64 == 60, "LAND 0: gong on beat 16"); }
		g.reset(); g.land = 0.5f;
		for (int i = 0; i < 64 * 2; i++) { g.clock(f); if (f.trig[D_GONG]) CHECK(i % 64 == 62, "LAND 0.5: gong on the and of 4"); }
	}
	// ---- checks: irama levels give the right number of kotekan notes per beat (fixed tempo)
	for (int L = 0; L < 5; L++) {
		IramaCore c; c.levelTarget = L; c.tempoFixed = true; c.kotekan = K_NYOGCAG; c.rng.seed(5);
		IramaFrame f; int notes = 0; double per = 6000; long n = (long)(per * 4 * 16 * 2);
		for (long i = 0; i < n; i++) { c.tick(i % (long)per == 0, f); if (f.fire[V_POLOS]) notes++; if (f.fire[V_SANGSIH]) notes++; }
		int expect = (L == 0 ? 1 : LEVEL_D[L]) * 32;
		char msg[64]; snprintf(msg, sizeof msg, "irama %d kotekan density (%d vs %d)", L, notes, expect);
		CHECK(std::abs(notes - expect) <= LEVEL_D[L] + 1, msg);
	}

	printf("demo scenes:\n");
	// 1. Java lancaran, slendro manyura, traditional tempo, irama 1 -> 2 -> 3, then suwuk
	{
		IramaCore c; c.style = JAVA; c.form = 0; c.mode = 2; c.levelTarget = 0; c.kotekan = K_IMBAL; c.mutate = 0.6f; c.rng.seed(11);
		Scene s = {&c, 0, 0.f, 0.f, 400.0, 46.0, false, 0.5f};
		std::vector<std::pair<double, std::string> > cu;
		cu.push_back(std::make_pair(9.0, "lvl1")); cu.push_back(std::make_pair(20.0, "lvl2")); cu.push_back(std::make_pair(34.0, "suwuk"));
		run(s, cu);
	}
	// 2. Bali kebyar: selisir, gilak, kotekan telu -> nyog cag -> empat, angsels, ombak 6.5 Hz
	{
		IramaCore c; c.style = BALI; c.form = 0; c.mode = 6; c.levelTarget = 2; c.tempoFixed = true; c.kotekan = K_TELU; c.mutate = 0.5f; c.rng.seed(23);
		c.kebyar = true; c.prevKebyar = false;
		Scene s = {&c, 0, 6.5f, 0.25f, 520.0, 44.0, false, 0.5f};
		std::vector<std::pair<double, std::string> > cu;
		cu.push_back(std::make_pair(4.2, "kebyar_off"));
		cu.push_back(std::make_pair(15.0, "nyogcag")); cu.push_back(std::make_pair(22.0, "angsel"));
		cu.push_back(std::make_pair(25.0, "empat")); cu.push_back(std::make_pair(33.0, "angsel"));
		cu.push_back(std::make_pair(38.0, "kebyar_on")); cu.push_back(std::make_pair(41.0, "kebyar_off"));
		run(s, cu);
	}
	// 3. Fusion: Irama fixed tempo at 124 BPM in pelog nem + Gatra drums, seleh as bass
	{
		IramaCore c; c.style = JAVA; c.form = 0; c.mode = 4; c.levelTarget = 1; c.tempoFixed = true; c.kotekan = K_NYOGCAG; c.mutate = 0.5f; c.rng.seed(31);
		GatraCore g; g.cycle = 16; g.land = 0.f; g.kotekan = DK_NYOGCAG; g.density = 0.8f; g.reseed(7);
		Scene s = {&c, &g, 3.0f, -0.25f, 124.0 * 4, 62.0, true, 0.58f};
		std::vector<std::pair<double, std::string> > cu;
		double bar = 60.0 / 124.0 * 4;
		cu.push_back(std::make_pair(bar * 8, "land05"));
		cu.push_back(std::make_pair(bar * 12, "lvl2")); cu.push_back(std::make_pair(bar * 12, "gtelu"));
		cu.push_back(std::make_pair(bar * 16 - 0.01, "land1"));
		cu.push_back(std::make_pair(bar * 22, "drop_on")); cu.push_back(std::make_pair(bar * 23, "drop_off"));
		cu.push_back(std::make_pair(bar * 23, "lvl3"));
		cu.push_back(std::make_pair(bar * 27, "cyc20"));
		run(s, cu);
	}
	fclose(out);
	printf(fails ? "%d checks FAILED\n" : "all checks passed\n", fails);
	return fails ? 1 : 0;
}
