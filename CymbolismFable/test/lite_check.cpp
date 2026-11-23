// Cymbolism Lite: offline checks of the Lite ranges (no Rack).  make test
//   L1 ranges     random knobs plus CV at both extremes stay inside the Lite limits
//   L2 stability  random Lite settings at MID and ECO, hard strikes: finite, bounded
//   L3 ceiling    the largest active mode count over those settings (and the corners)
//   L4 CPU        random draws, full ranges at HIGH against Lite ranges at MID
#include "../src/CyEngine.hpp"
#include "../src/CyLimits.hpp"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <thread>
#include <vector>
using namespace cy;

static double now() { return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count(); }
static double clampd(double v, double lo, double hi) { return std::max(lo, std::min(hi, v)); }

// The module's own mapping (src/Cymbolism.cpp readGeometry / readControls / playNow) for one knob
// position u in [0,1] across the knob's range, plus a CV in volts.
struct Mapped { double size, weight; int rivets, play; };
static Mapped mapKnobs(const CyLimits& L, double uSize, double uWeight, double uSizzle, double uPlay, double cvV) {
	Mapped m;
	double lam = 0.6 + (L.sizeMax - 0.6) * uSize;
	lam *= std::pow(2.0, -cvV);   // SIZE CV is 1 V/oct (+1 V halves)
	m.size = clampd(lam, 0.35, L.sizeCvMax);
	double wr = 2.5 - L.weightMin;
	m.weight = clampd(L.weightMin + wr * uWeight + cvV * 0.1 * wr, L.weightMin, 2.5);
	double sz = std::round(L.sizzleMax * uSizzle);
	m.rivets = (int)std::round(clampd(sz + cvV * 0.1 * L.sizzleMax, 0, L.sizzleMax));
	double p = std::round((L.numPlays - 1) * uPlay) + clampd(cvV, 0, 10) * 0.1 * (L.numPlays - 1);
	m.play = (int)clampd(std::round(p), 0, L.numPlays - 1);
	return m;
}

struct Set { GeomParams g; Controls c; };
static Set draw(CyRng& r, const CyLimits& L, int quality) {
	double u[20];
	for (double& x : u) x = r.uni();
	int t = (int)(u[0] * NUM_TYPES) % NUM_TYPES;
	const CymbalType& ct = CY_TYPES[t];
	Mapped m = mapKnobs(L, u[1], u[2], u[3], u[4], 0.0);
	Set s;
	s.g.type = t;
	s.g.weight = std::round(m.weight * 50) / 50;
	s.g.taper = std::round((1 + 1.5 * u[5]) * 40) / 40;
	s.g.rise = std::round(0.14 * u[6] * 500) / 500;
	s.g.bell = std::round(2 * u[7] * 40) / 40;
	s.g.flangeDeg = std::round(-20 + 45 * u[8]);
	s.g.alloy = (int)(u[9] * NUM_ALLOYS) % NUM_ALLOYS;
	s.g.tension = std::round((-1 + 2 * u[10]) * 20) / 20;
	s.g.quality = quality;
	s.g.sampleRate = 48000;
	(void)ct;
	s.c.size = m.size;
	s.c.sizzle = m.rivets / 12.0;
	s.c.play = m.play;
	s.c.decay = 0.25 * std::pow(16.0, u[11]);
	s.c.bloom = 2 * u[12];
	s.c.damp = u[13];
	s.c.mount = u[14];
	s.c.hammer = u[15];
	s.c.mic = 4 * u[16];
	s.c.hardness = u[17];
	s.c.position = u[18];
	s.c.force = 0.2 * std::pow(60.0, u[19]);
	s.c.rate = 8;
	return s;
}

struct Res { bool ok; double peak, cpu; int na; };
static Res run(const Set& s, bool hard, double dur) {
	auto M = buildModel(s.g);
	Engine E;
	E.ctl = s.c;
	E.init(M, 48000);
	int n = (int)(dur * 48000);
	float o[32];
	Res r{true, 0, 0, 0};
	double t0 = now();
	for (int k = 0; k < n; k += 32) {
		if (k % 24000 == 0) E.strike(hard ? 12.0 : s.c.force);
		E.process(nullptr, o, 32);
		for (float x : o) { r.ok &= std::isfinite(x); r.peak = std::max(r.peak, (double)std::fabs(x)); }
		r.na = std::max(r.na, E.NA);
	}
	r.cpu = (now() - t0) / dur * 100;
	r.ok &= r.peak <= 10.01;
	return r;
}

template <class F> static void parallel(int n, int threads, F f) {
	std::atomic<int> next{0};
	std::vector<std::thread> th;
	for (int w = 0; w < threads; w++) th.emplace_back([&] { for (int i; (i = next++) < n;) f(i); });
	for (auto& t : th) t.join();
}

int main() {
	int fails = 0;
	const CyLimits& L = CY_LITE;

	// L1
	{
		CyRng r; r.seed(7);
		int bad = 0;
		for (int i = 0; i < 300; i++) {
			double u[4]; for (double& x : u) x = r.uni();
			for (double cvV : {-10.0, 0.0, 10.0}) {
				Mapped m = mapKnobs(L, u[0], u[1], u[2], u[3], cvV);
				if (m.size > 1.2 + 1e-6 || m.weight < 0.8 - 1e-6 || m.rivets > 6 || m.play >= BOW) bad++;
			}
		}
		for (int t = 0; t < NUM_TYPES; t++)
			if (CY_TYPES[t].rivets > L.sizzleMax || CY_TYPES[t].play >= L.numPlays) bad++;
		printf("L1 ranges: %s (900 knob + CV draws, %d type defaults)\n", bad ? "FAIL" : "ok", NUM_TYPES);
		fails += bad != 0;
	}

	// L2 + L3
	int maxNA = 0, cornerNA = 0;
	double cornerCpu = 0;
	const char* cornerType = "";
	{
		const int N = 40;
		for (int q : {3, 0}) {
			CyRng r; r.seed(11 + q);
			std::vector<Set> sets;
			for (int i = 0; i < N; i++) sets.push_back(draw(r, L, q));
			// the corners: biggest, thinnest, flattest of every type
			for (int t = 0; t < NUM_TYPES; t++) {
				Set s = draw(r, L, q);
				s.g.type = t; s.g.weight = L.weightMin; s.g.taper = 1.0; s.g.rise = 0.0; s.g.bell = 0.0; s.g.flangeDeg = 25;
				s.c.size = L.sizeCvMax; s.c.sizzle = L.sizzleMax / 12.0;
				sets.push_back(s);
			}
			std::vector<Res> res(sets.size());
			parallel((int)sets.size(), 4, [&](int i) { res[i] = run(sets[i], true, 2.0); });
			int bad = 0, worst = 0;
			for (size_t i = 0; i < res.size(); i++) {
				if (i >= (size_t)N) {   // corners: reported, not held to the ceiling
					if (res[i].na > cornerNA) { cornerNA = res[i].na; cornerType = CY_TYPES[sets[i].g.type].name; }
					if (!res[i].ok) bad++;
					continue;
				}
				if (!res[i].ok) { bad++; printf("   unbounded: type %s size %.2f weight %.2f peak %.2f\n", CY_TYPES[sets[i].g.type].name, sets[i].c.size, sets[i].g.weight, res[i].peak); }
				if (res[i].na > maxNA) maxNA = res[i].na;
				worst = std::max(worst, res[i].na);
			}
			printf("L2 stability %s: %s (%zu settings, hard strikes; most active modes %d)\n", q == 3 ? "MID" : "ECO", bad ? "FAIL" : "ok", res.size(), worst);
			fails += bad != 0;
		}
		bool ok = maxNA <= 2600;
		printf("L3 mode ceiling: %s (random settings: most active modes %d, limit 2600; extreme corner %s: %d)\n", ok ? "ok" : "FAIL", maxNA, cornerType, cornerNA);
		fails += !ok;
	}

	// L4
	{
		const int N = 40;
		CyRng r1; r1.seed(23);
		CyRng r2; r2.seed(23);
		std::vector<Set> full, lite;
		for (int i = 0; i < N; i++) { full.push_back(draw(r1, CY_FULL, 1)); lite.push_back(draw(r2, L, 3)); }
		std::vector<Res> rf(N), rl(N);
		parallel(N, 2, [&](int i) { rf[i] = run(full[i], false, 3.0); });
		parallel(N, 2, [&](int i) { rl[i] = run(lite[i], false, 3.0); });
		double mf = 0, ml = 0, wf = 0, wl = 0;
		for (int i = 0; i < N; i++) { mf += rf[i].cpu; ml += rl[i].cpu; wf = std::max(wf, rf[i].cpu); wl = std::max(wl, rl[i].cpu); }
		mf /= N; ml /= N;
		bool ok = ml <= 0.85 * mf;
		printf("L4 CPU: %s (full ranges at HIGH: mean %.1f%%, worst %.1f%%; Lite at MID: mean %.1f%%, worst %.1f%%; mean ratio %.2f)\n",
		       ok ? "ok" : "FAIL", mf, wf, ml, wl, ml / mf);
		fails += !ok;
	}

	if (fails) printf("lite_check: %d FAILED\n", fails);
	else printf("lite_check: all ok\n");
	return fails ? 1 : 0;
}
