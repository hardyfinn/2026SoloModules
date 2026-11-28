// Offline checks for the Tiers engine (no Rack needed): make test
#include <cstdio>
#include <cstring>
#include "../src/TiersEngine.hpp"
using namespace tiers;

static int fails = 0;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL: "); printf(__VA_ARGS__); printf("\n"); } } while (0)

int main() {
	int count;
	const Preset* P = presets(count);
	printf("%d presets\n", count);
	// 1. every preset is 16 or 32 steps and reproduces exactly
	for (int p = 0; p < count; p++) {
		int len = (int) strlen(P[p].steps);
		CHECK(len == 16 || len == 32, "%s has %d steps", P[p].name, len);
		Engine e;
		Settings s;
		e.loadPreset(P[p], s);
		Result r;
		e.evaluate(s, r);
		for (int i = 0; i < len; i++) {
			char c = P[p].steps[i];
			bool h = c != '-', a = c == 'X' || c == 'R', q = c == 'r' || c == 'R';
			CHECK(r.hit[i] == h && r.acc[i] == a && r.rat[i] == q, "%s step %d", P[p].name, i + 1);
		}
	}
	// 2. raising a weight only ever adds hits (chance mode)
	Engine e;
	Settings s;
	for (int l = 0; l < 4; l++) {
		for (int k = 0; k < 4; k++) s.w[k] = 0.3f;
		Result prev;
		s.w[l] = 0.f;
		e.evaluate(s, prev);
		for (int step = 1; step <= 20; step++) {
			s.w[l] = step / 20.f;
			Result r;
			e.evaluate(s, r);
			for (int i = 0; i < s.length; i++) CHECK(!prev.hit[i] || r.hit[i], "level %d lost a hit at %d", l, i);
			prev = r;
		}
		CHECK(prev.hits >= 2, "level %d at 100%% too few hits", l);
	}
	// 3. count mode gives exactly HITS hits, and is nested as HITS grows
	s.count = true;
	for (int k = 0; k < 4; k++) s.w[k] = 0.5f;
	Result prev;
	s.hits = 0;
	e.evaluate(s, prev);
	for (int h = 1; h <= 16; h++) {
		s.hits = h;
		Result r;
		e.evaluate(s, r);
		CHECK(r.hits == h, "count %d gave %d", h, r.hits);
		for (int i = 0; i < 16; i++) CHECK(!prev.hit[i] || r.hit[i], "count %d dropped step %d", h, i);
		prev = r;
	}
	// 4. heavy 1/4 weighting in count mode lands on beats first
	s.w[0] = 0.f; s.w[1] = 1.f; s.w[2] = 0.05f; s.w[3] = 0.05f;
	s.hits = 2;
	long onBeats = 0, total = 0;
	for (int t = 0; t < 2000; t++) {
		e.generate();
		Result r;
		e.evaluate(s, r);
		for (int i = 0; i < 16; i++)
			if (r.hit[i]) { total++; if (i == 4 || i == 12) onBeats++; }
	}
	CHECK(onBeats > total * 0.65, "weighting ignored: %ld of %ld on 1/4 steps", onBeats, total);
	// 5. mutate changes things, lock-equivalent (amount 0) changes nothing, back restores
	s.count = false;
	e.generate();
	Result a, b;
	e.evaluate(s, a);
	CHECK(e.mutate(0.f, s) == 0, "mutate 0 changed steps");
	int changed = 0;
	for (int t = 0; t < 8; t++) changed += e.mutate(1.f, s);
	CHECK(changed > 0, "mutate 1 changed nothing");
	e.back();
	e.evaluate(s, b);
	for (int i = 0; i < 16; i++) CHECK(a.hit[i] == b.hit[i], "back did not restore step %d", i);
	// 6. rotate shifts hits
	s.rotate = 3;
	Result c;
	e.evaluate(s, c);
	for (int i = 0; i < 16; i++) CHECK(c.hit[(i + 3) % 16] == a.hit[i], "rotate step %d", i);
	// 7. hand edits survive weights at 0 and mutation
	s.rotate = 0;
	for (int k = 0; k < 5; k++) s.w[k] = 0.f;
	e.setHit(5, s, true);
	for (int t = 0; t < 10; t++) e.mutate(1.f, s);
	Result d;
	e.evaluate(s, d);
	CHECK(d.hit[5] && d.hits == 1, "hand edit lost");
	printf(fails ? "%d FAILED\n" : "all checks passed\n", fails);
	return fails ? 1 : 0;
}
