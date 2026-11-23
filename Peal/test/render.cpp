// Offline checks for the ringing engine, plus a demo WAV (simple bell tones).
//   make test   (no Rack needed)
#include "../src/PealEngine.hpp"
#include "../src/FoundryDSP.hpp"
#include <cstdio>
#include <cmath>
#include <set>
#include <string>
using namespace peal;

static std::string rowStr(const uint8_t* r, int n) {
	std::string s;
	for (int i = 0; i < n; i++) s += BELL_CHARS[r[i]];
	return s;
}

// Ring the plain course (no calls) from rounds; report changes until rounds
// come back and whether any row repeated on the way (a "false" course).
static int plainCourse(int m, int stage, bool& isTrue, int calls = 0) {
	Ringer e;
	RingParams p; p.stage = stage; p.method = m; p.gap = 0; p.going = true;
	e.stage = stage; e.reset();
	std::set<std::string> seen;
	if (calls) e.call(calls);
	// first row (rounds) is rung, then the method starts at the next handstroke
	int changes = 0; isTrue = true;
	for (int t = 0; t < 200000; t++) {
		if (calls && e.inMethod && e.changeIdx == 0 && e.pos == 0) e.call(calls);
		Strike s = e.tick(p);
		if (s.rowStart && e.inMethod) {
			std::string r = rowStr(e.row, stage);
			if (changes > 0 && r == rowStr((const uint8_t*) "\0\1\2\3\4\5\6\7\10\11\12\13", stage)) return changes + 1;
			if (!seen.insert(r).second) isTrue = false;
			changes++;
		}
	}
	return -1;
}

int main(int argc, char** argv) {
	int fails = 0;
	struct C { int m, stage, expect; } cases[] = {
		{HUNT, 6, 12}, {HUNT, 5, 10}, {PLAIN_BOB, 6, 60}, {PLAIN_BOB, 8, 112}, {PLAIN_BOB, 5, 40},
		{PLAIN_BOB, 4, 24}, {PLAIN_BOB, 12, 264}, {GRANDSIRE, 5, 30}, {GRANDSIRE, 7, 70},
		{STEDMAN, 5, 60}, {STEDMAN, 7, 84}, {CAMBRIDGE, 6, 120}, {CAMBRIDGE, 8, 224},
		{CAMBRIDGE, 10, 360}, {CAMBRIDGE, 12, 528}, {LITTLE_BOB, 6, -1}, {LITTLE_BOB, 8, -1},
		{GRANDSIRE, 8, 70}, {CAMBRIDGE, 7, 120}};
	for (auto& c : cases) {
		bool tr;
		int n = plainCourse(c.m, c.stage, tr);
		bool ok = n > 0 && tr && (c.expect < 0 || n == c.expect);
		printf("%-12s %-8s plain course %4d changes (expect %4d) %s %s\n", METHOD_NAMES[c.m], STAGE_NAMES[c.stage], n,
		       c.expect, tr ? "true " : "FALSE", ok ? "ok" : "<-- FAIL");
		if (!ok) fails++;
	}
	// calls must give valid permutations and still come round eventually
	for (int m : {PLAIN_BOB, GRANDSIRE, STEDMAN, CAMBRIDGE}) {
		for (int call : {1, 2}) {
			bool tr;
			int n = plainCourse(m, m == GRANDSIRE || m == STEDMAN ? 7 : 8, tr, call);
			printf("%-12s every lead %-6s -> rounds after %d changes\n", METHOD_NAMES[m], call == 1 ? "BOB" : "SINGLE", n);
		}
	}
	// print the first lead of Plain Bob Minor so it can be eyeballed
	{
		Ringer e; RingParams p; p.stage = 6; p.gap = 0; e.stage = 6; e.reset();
		printf("Plain Bob Minor, first rows:\n");
		int rows = 0;
		for (int t = 0; rows < 15; t++) { Strike s = e.tick(p); if (s.rowStart) { printf("  %s\n", rowStr(e.row, 6).c_str()); rows++; } }
	}
	printf(fails ? "%d FAILED\n" : "all ok\n", fails);

	// ---- demo WAV: Peal into Bell Foundry.
	//   0-24 s  Plain Bob Major, church bells, true-harmonic, from the churchyard, calls at 35 %
	//   24-48 s Wander on 7 bells, gamelan profile with ombak beating, steel, ringing chamber
	const char* dir = argc > 1 ? argv[1] : ".";
	const int SR = 48000;
	const double beat = 0.17;
	const int secs = 52;
	std::vector<float> L(SR * secs), R(SR * secs);
	using namespace foundry;
	static Bell bells[NB];
	Rng cr; for (int i = 0; i < NB; i++) bells[i].recast(cr);
	Reverb verb; verb.init(SR);
	Ringer e; RingParams p; p.stage = 8; p.method = PLAIN_BOB; p.chance = 0.35f; p.gap = 1;
	e.stage = 8; e.reset();
	const int maj[7] = {0, 2, 4, 5, 7, 9, 11}, pel[5] = {0, 1, 3, 7, 8};
	Settings st; st.tuning = TRUE_HARMONIC;
	int tower = 1;
	double nextBeat = 0.0;
	float lpL = 0, lpR = 0;
	for (int i = 0; i < SR * secs; i++) {
		double t = (double) i / SR;
		if (t >= 24.0 && p.method == PLAIN_BOB) {
			p.method = WANDER; p.stage = 7; p.chance = 0.6f;
			st.profile = 2.f; st.tuning = OMBAK; st.metal = 0.8f; st.clapper = 0.8f; tower = 0;
		}
		if (t >= nextBeat && t < secs - 5) {
			nextBeat += beat;
			Strike s = e.tick(p);
			if (s.struck) {
				int d = e.stage - 1 - s.bell;
				double semis = p.method == WANDER ? pel[d % 5] + 12 * (d / 5) : maj[d % 7] + 12 * (d / 7);
				bells[s.bell].strike((float) (130.81 * pow(2.0, semis / 12.0)), 0.7f + 0.3f * (s.place == 0), st, SR);
			}
		}
		float l = 0, r = 0;
		for (int b = 0; b < NB; b++) {
			float v = bells[b].process();
			float pan = 0.5f + 0.35f * sinf(TWO_PI * (b + 0.5f) / e.stage);
			l += v * sqrtf(1 - pan); r += v * sqrtf(pan);
		}
		const float LP[3] = {16000.f, 6000.f, 2200.f}, WET[3] = {0.12f, 0.3f, 0.55f}, DRY[3] = {1.f, 0.8f, 0.45f}, FB[3] = {0.72f, 0.8f, 0.86f};
		float a = 1.f - expf(-TWO_PI * LP[tower] / SR);
		lpL += a * (l - lpL); lpR += a * (r - lpR);
		float wl, wr;
		verb.process(lpL, lpR, wl, wr, tower == 0 ? 200 : 1000, FB[tower], 0.35f);
		L[i] = tanhf((lpL * DRY[tower] + wl * WET[tower]) * 0.8f) * 0.9f;
		R[i] = tanhf((lpR * DRY[tower] + wr * WET[tower]) * 0.8f) * 0.9f;
	}
	std::string path = std::string(dir) + "/peal-demo.wav";
	FILE* fo = fopen(path.c_str(), "wb");
	if (fo) {
		int n = (int) L.size(), bytes = n * 4;
		auto w32 = [&](uint32_t v) { fwrite(&v, 4, 1, fo); };
		auto w16 = [&](uint16_t v) { fwrite(&v, 2, 1, fo); };
		fwrite("RIFF", 1, 4, fo); w32(36 + bytes); fwrite("WAVEfmt ", 1, 8, fo);
		w32(16); w16(1); w16(2); w32(SR); w32(SR * 4); w16(4); w16(16);
		fwrite("data", 1, 4, fo); w32(bytes);
		for (int i = 0; i < n; i++) {
			int16_t a = (int16_t) (fmax(-1.0, fmin(1.0, L[i])) * 30000), b = (int16_t) (fmax(-1.0, fmin(1.0, R[i])) * 30000);
			fwrite(&a, 2, 1, fo); fwrite(&b, 2, 1, fo);
		}
		fclose(fo);
		printf("wrote %s\n", path.c_str());
	}
	return fails ? 1 : 0;
}
