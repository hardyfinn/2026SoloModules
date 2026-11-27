// Offline checks for the 808 Circuit Bench kit (no Rack needed): make test
#include "../src/BenchVoices.hpp"
#include <vector>
#include <string>
using namespace bench;
static const float FS = 48000.f;
static int fails = 0;
static void check(bool ok, const std::string& w) { printf("  [%s] %s\n", ok ? "ok" : "FAIL", w.c_str()); if (!ok) fails++; }
static float peakOf(Kit& kit, int v, float acc, float sec) {
	float p = 0.f; bool fin = true;
	kit.hit(v, acc);
	for (int i = 0; i < sec * FS; i++) { kit.v[v]->process(); float o = kit.v[v]->out; if (!std::isfinite(o)) fin = false; p = std::max(p, std::fabs(o)); }
	return fin ? p : -1.f;
}
int main() {
	printf("808 Circuit Bench\n");
	Kit kit; kit.setSampleRate(FS); kit.setSerial(808);
	bool lv = true;
	for (int v = 0; v < NV; v++) {
		float u = peakOf(kit, v, 0.f, 1.f), a = peakOf(kit, v, 1.f, 1.f);
		printf("  %s %-11s peak %.2f V, accented %.2f V\n", code(v), longName(v), u, a);
		if (!(u > 1.f && u < 11.f && a > u && a < 12.f)) lv = false;
	}
	check(lv, "every voice: stock level 1-11 V, accent louder, finite");
	// toms keep their range whatever is loaded
	kit.v[LT]->toStock(); kit.v[HT]->set(c808::tom::RANGE, 0.f);
	check(kit.v[LT]->k[c808::tom::RANGE] == 0.f && kit.v[HT]->k[c808::tom::RANGE] == 2.f, "LT/MT/HT hold their range");
	// closed hat chokes the open hat
	{
		Kit k2; k2.setSampleRate(FS);
		k2.hit(OH, 0.f);
		for (int i = 0; i < 0.05f * FS; i++) k2.v[OH]->process();
		k2.hit(CH, 0.f);
		float p = 0.f; for (int i = 0; i < 0.2f * FS; i++) { k2.v[OH]->process(); if (i > 0.1f * FS) p = std::max(p, std::fabs(k2.v[OH]->out)); }
		printf("  OH 100-200 ms after a CH: %.3f V\n", p);
		check(p < 0.05f, "CH chokes OH");
	}
	// random settings stay finite and bounded
	{
		c808::Rng r; r.seed(5); bool ok = true;
		for (int t = 0; t < 20; t++) for (int v = 0; v < NV; v++) {
			Kit k3; k3.setSampleRate(t % 2 ? 96000.f : 44100.f);
			for (int i = 0; i < k3.v[v]->nK(); i++) k3.v[v]->set(i, t == 0 ? 0.f : t == 1 ? 1.f : r.uni());
			k3.hit(v, t % 3 ? 1.f : 0.f);
			for (int i = 0; i < 0.3f * 48000; i++) { k3.v[v]->process(); float o = k3.v[v]->out; if (!std::isfinite(o) || std::fabs(o) > (v >= CY ? 25.f : 13.f)) { if (ok) printf("  %s trial %d: %g\n", code(v), t, o); ok = false; } }
		}
		check(ok, "20 random/extreme settings per voice at 44.1/96 kHz: finite, bounded (808 Metal as on its own module)");
	}
	// silent voices sleep
	{
		Kit k4; k4.setSampleRate(FS); bool all = true;
		for (int v = 0; v < NV; v++) {
			k4.hit(v, 0.f); int n = 0;
			while (!k4.v[v]->sleeping() && n < 12 * FS) { k4.v[v]->process(); n++; }
			if (!k4.v[v]->sleeping()) { all = false; printf("  %s never sleeps\n", code(v)); }
		}
		check(all, "every voice sleeps after its sound ends");
	}
	// every block parameter is a valid index and every voice's params appear once
	{
		bool ok = true;
		for (int v = 0; v < NV; v++) {
			const Layout& L = layout(kindOf(v));
			std::vector<int> seen(MAXK, 0);
			for (int b = 0; b < L.nBlocks; b++) for (int j = 0; j < L.blocks[b].n; j++) {
				int p = L.blocks[b].p[j]; if (p == PAN) continue;
				if (p < 0 || p >= kit.v[v]->nK()) ok = false; else seen[p]++;
			}
			for (int i = 0; i < kit.v[v]->nK(); i++) {
				bool hidden = kindOf(v) == K_TOM && i == c808::tom::RANGE;
				if (seen[i] != (hidden ? 0 : 1)) { ok = false; printf("  %s param %s appears %d times\n", code(v), kit.v[v]->def(i).id, seen[i]); }
			}
		}
		check(ok, "screen blocks show every parameter exactly once");
	}
	// demo: four bars of a kit pattern through the stereo mix
	{
		Kit kd; kd.setSampleRate(FS); kd.setSerial(808);
		kd.v[CY]->pan = 0.7f; kd.v[OH]->pan = 0.35f; kd.v[CH]->pan = 0.35f; kd.v[CB]->pan = 0.65f; kd.v[CL]->pan = 0.7f;
		kd.v[LT]->pan = 0.3f; kd.v[HT]->pan = 0.7f; kd.v[MA]->pan = 0.6f;
		const char* pat[NV] = {
			"x.....x...x.....|x.....x...x..x..",   // BD
			"....x.......x...|....x.......x.x.",   // SD
			"................|.............x..",   // LT
			"................|..............x.",   // MT
			"................|...............x",   // HT
			"...x......x.....|...x............",   // RS
			"..x..x..........|..x..x..........",   // CL
			"............x...|............x...",   // CP
			"x.xxx.xxx.xxx.xx|x.xxx.xxx.xxx.xx",   // MA
			".......x...x....|.......x........",   // CB
			"x...............|................",   // CY
			"..............x.|..............x.",   // OH
			"x.x.x.x.x.x.x.x.|x.x.x.x.x.x.x.x.",   // CH
		};
		int step = (int)(60.f / 120.f / 4.f * FS);
		std::vector<float> L, R;
		for (int bar = 0; bar < 4; bar++) for (int st = 0; st < 16; st++) {
			for (int v = 0; v < NV; v++) {
				char c = pat[v][(bar % 2) * 17 + st];
				if (c == 'x') kd.hit(v, (st % 4 == 0) ? 1.f : 0.f);
			}
			for (int i = 0; i < step; i++) {
				float l = 0, r = 0;
				for (int v = 0; v < NV; v++) {
					kd.v[v]->process(); float o = kd.v[v]->out;
					if (v == MA) o *= 0.5f;
					l += o * std::min(1.f, 2.f * (1.f - kd.v[v]->pan)); r += o * std::min(1.f, 2.f * kd.v[v]->pan);
				}
				L.push_back(l); R.push_back(r);
			}
		}
		FILE* f = fopen("test/out/bench-01-kit.wav", "wb");
		if (f) {
			uint32_t n = L.size(), bytes = n * 4;
			auto w32 = [&](uint32_t x) { fwrite(&x, 4, 1, f); };
			auto w16 = [&](uint16_t x) { fwrite(&x, 2, 1, f); };
			fwrite("RIFF", 1, 4, f); w32(36 + bytes); fwrite("WAVEfmt ", 1, 8, f);
			w32(16); w16(1); w16(2); w32((uint32_t)FS); w32((uint32_t)FS * 4); w16(4); w16(16);
			fwrite("data", 1, 4, f); w32(bytes);
			for (uint32_t i = 0; i < n; i++) for (float x : {L[i], R[i]}) { float c = std::max(-1.f, std::min(1.f, x * 0.5f / 10.f)); w16((uint16_t)(int16_t)(c * 32767)); }
			fclose(f);
		}
	}

	printf(fails ? "%d FAILED\n" : "all bench checks passed\n", fails);
	return fails ? 1 : 0;
}
