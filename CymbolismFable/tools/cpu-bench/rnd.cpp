// CPU of Cymbolism under randomized knobs (as Rack's Randomize) and per-control sweeps
#include "CyEngine.hpp"
#include <chrono>
#include <cstdio>
#include <map>
#include <mutex>
#include <thread>
using namespace cy;
static double now() { return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count(); }
struct Set { int type; double size, weight, taper, rise, bell, flange; int alloy; double hammer, tension, decay, bloom, damp, mount; int sizzle; double mic; int play; double hard, force, pos; int quality = 1; };
static Set def(int t) { const auto& ct = CY_TYPES[t]; return {t, 1, 1, ct.taper, std::min(0.14, ct.rise), 1, ct.flangeDeg, 0, 0.3, 0, 1, 1, 0, 0.5, ct.rivets, 1, ct.play, 0.5, 3, ct.position}; }
struct Res { double cpu; int N, NA; size_t trips; double build; };
static std::mutex mx; static std::map<std::string, std::shared_ptr<Model>> cache;
static Res run(const Set& s, bool roll = false) {
	GeomParams gp; gp.type = s.type; gp.weight = std::round(s.weight * 50) / 50; gp.taper = std::round(s.taper * 40) / 40; gp.rise = std::round(s.rise * 500) / 500; gp.bell = std::round(s.bell * 40) / 40;
	gp.flangeDeg = std::round(s.flange); gp.alloy = s.alloy; gp.tension = std::round(s.tension * 20) / 20; gp.quality = s.quality; gp.sampleRate = 48000;
	std::shared_ptr<Model> M;
	{ std::lock_guard<std::mutex> l(mx); auto it = cache.find(gp.key()); if (it != cache.end()) M = it->second; }
	if (!M) { M = buildModel(gp); std::lock_guard<std::mutex> l(mx); if (cache.size() > 40) cache.clear(); cache[gp.key()] = M; }
	Controls c; c.size = s.size; c.decay = s.decay; c.bloom = s.bloom; c.damp = s.damp; c.mount = s.mount; c.hammer = s.hammer; c.mic = s.mic; c.sizzle = s.sizzle / 12.0;
	c.position = s.pos; c.hardness = s.hard; c.force = s.force; c.play = s.play; c.rate = 8;
	Engine E; E.ctl = c; E.init(M, 48000);
	const double dur = 3.0; int n = (int)(dur * 48000); float o[32];
	double t0 = now();
	for (int k = 0; k < n; k += 32) { E.ctl.roll = roll; if (!roll && k % 24000 == 0) E.strike(s.force); E.process(nullptr, o, 32); }
	return {(now() - t0) / dur * 100, E.N, E.NA, E.trips.size(), M->buildMs};
}
int main(int argc, char** argv) {
	std::string mode = argv[1];
	if (mode == "sweep") {
		auto show = [&](const char* what, Set s) { Res r = run(s); printf("%-34s cpu %5.1f%%  modes %4d  coupling terms %5zu  build %4.0f ms\n", what, r.cpu, r.NA, r.trips, r.build); fflush(stdout); };
		for (int t = 0; t < NUM_TYPES; t++) show((std::string("TYPE ") + CY_TYPES[t].name).c_str(), def(t));
		Set c = def(1);
		for (double v : {0.6, 0.8, 1.0, 1.25, 1.5}) { Set s = c; s.size = v; char b[64]; snprintf(b, 64, "crash SIZE %.2f", v); show(b, s); }
		for (double v : {0.5, 0.7, 1.0, 1.5, 2.5}) { Set s = c; s.weight = v; char b[64]; snprintf(b, 64, "crash WEIGHT %.2f", v); show(b, s); }
		for (double v : {1.0, 1.7, 2.5}) { Set s = c; s.taper = v; char b[64]; snprintf(b, 64, "crash TAPER %.2f", v); show(b, s); }
		for (double v : {0.0, 0.07, 0.14}) { Set s = c; s.rise = v; char b[64]; snprintf(b, 64, "crash PROFILE %.2f", v); show(b, s); }
		for (double v : {0.0, 1.0, 2.0}) { Set s = c; s.bell = v; char b[64]; snprintf(b, 64, "crash BELL %.1f", v); show(b, s); }
		for (double v : {-20.0, 0.0, 25.0}) { Set s = c; s.flange = v; char b[64]; snprintf(b, 64, "crash FLANGE %.0f", v); show(b, s); }
		for (int v = 0; v < 6; v++) { Set s = c; s.alloy = v; char b[64]; snprintf(b, 64, "crash ALLOY %s", CY_MATERIALS[v].name); show(b, s); }
		for (double v : {-1.0, 0.0, 1.0}) { Set s = c; s.tension = v; char b[64]; snprintf(b, 64, "crash TENSION %.0f", v); show(b, s); }
		for (int v : {0, 2, 4, 6, 8, 12}) { Set s = c; s.sizzle = v; char b[64]; snprintf(b, 64, "crash SIZZLE %d", v); show(b, s); }
		for (double v : {0.0, 0.5, 1.0, 2.0}) { Set s = c; s.bloom = v; char b[64]; snprintf(b, 64, "crash BLOOM %.1f", v); show(b, s); }
		for (int v = 0; v < 6; v++) { Set s = c; s.play = v; char b[64]; snprintf(b, 64, "crash PLAY %s (strikes)", CY_PLAY_NAMES_EXT[v]); show(b, s); }
		for (int v = 0; v < 6; v++) { Set s = c; s.play = v; Res r = run(s, true); printf("crash PLAY %-9s (ROLL held)    cpu %5.1f%%\n", CY_PLAY_NAMES_EXT[v], r.cpu); }
		for (double v : {0.2, 3.0, 12.0}) { Set s = c; s.force = v; char b[64]; snprintf(b, 64, "crash FORCE %.1f m/s", v); show(b, s); }
		for (int q : {0, 3, 1, 2}) { Set s = c; s.quality = q; char b[64]; snprintf(b, 64, "crash quality code %d", q); show(b, s); }
		return 0;
	}
	// random: n keep key=value...
	int n = atoi(argv[2]); bool keep = atoi(argv[3]);
	std::map<std::string,double> L = {{"sizeMax",1.5},{"weightMin",0.5},{"sizzleMax",12},{"quality",1},{"flangeMin",-20},{"flangeMax",25},{"tensionMin",-1},{"tensionMax",1},{"taperMin",1},{"riseMin",0},{"noChina",0},{"noSwish",0},{"noFlat",0},{"typeFlangeCap",99},{"csv",0},{"playMax",5}};
	for (int a = 4; a < argc; a++) { std::string k = argv[a]; auto p = k.find('='); L[k.substr(0, p)] = atof(k.substr(p + 1).c_str()); }
	std::vector<Set> sets(n); std::vector<Res> res(n);
	CyRng r; r.seed(99);
	for (int i = 0; i < n; i++) {
		int t = (int)(r.uni() * NUM_TYPES) % NUM_TYPES;
		double u[20]; for (double& x : u) x = r.uni();
		std::string tn = CY_TYPES[t].name;
		if ((L["noChina"] && tn == "CHINA") || (L["noSwish"] && tn == "SWISH") || (L["noFlat"] && tn == "FLAT RIDE")) t = 1 + (int)(u[19] * 5) % 5;
		Set s = def(t);
		if (keep) { s.weight = L["weightMin"] + (2.5 - L["weightMin"]) * u[1]; s.taper = L["taperMin"] + (2.5 - L["taperMin"]) * u[2]; s.rise = L["riseMin"] + (0.14 - L["riseMin"]) * u[3]; s.bell = 2 * u[4]; s.flange = L["flangeMin"] + (L["flangeMax"] - L["flangeMin"]) * u[5]; }
		else { s.flange = std::max(-L["typeFlangeCap"], std::min(L["typeFlangeCap"], s.flange)); }
		s.size = 0.6 + (L["sizeMax"] - 0.6) * u[0];
		s.alloy = (int)(u[6] * 6) % 6; s.hammer = u[7]; s.tension = L["tensionMin"] + (L["tensionMax"] - L["tensionMin"]) * u[8]; s.decay = 0.25 * std::pow(16.0, u[9]); s.bloom = 2 * u[10]; s.damp = u[11]; s.mount = u[12];
		s.sizzle = (int)std::lround(L["sizzleMax"] * u[13]); s.mic = 4 * u[14]; s.play = (int)std::lround(L["playMax"] * u[15]); s.hard = u[16]; s.force = 0.2 * std::pow(60.0, u[17]); s.pos = u[18];
		s.quality = (int)L["quality"];
		sets[i] = s;
	}
	std::vector<std::thread> th; std::atomic<int> next{0};
	for (int w = 0; w < 2; w++) th.emplace_back([&] { for (;;) { int i = next++; if (i >= n) break; res[i] = run(sets[i]); } });
	for (auto& t : th) t.join();
	std::vector<double> c; double sum = 0; std::vector<int> idx(n);
	for (int i = 0; i < n; i++) { c.push_back(res[i].cpu); sum += res[i].cpu; idx[i] = i; }
	std::sort(c.begin(), c.end());
	std::sort(idx.begin(), idx.end(), [&](int a, int b) { return res[a].cpu > res[b].cpu; });
	printf("n %d keep %d", n, (int)keep); for (int a = 4; a < argc; a++) printf(" %s", argv[a]);
	printf(": mean %.1f  median %.1f  90th %.1f  max %.1f\n", sum / n, c[n / 2], c[(int)(0.9 * n)], c[n - 1]);
	for (int k = 0; k < 5; k++) { const Set& w = sets[idx[k]]; printf("   worst %d: %5.1f%% %s size %.2f wt %.2f taper %.2f rise %.3f flange %.0f tension %.2f sizzle %d bloom %.2f play %s, %d modes %zu terms\n", k + 1, res[idx[k]].cpu, CY_TYPES[w.type].name, w.size, w.weight, w.taper, w.rise, w.flange, w.tension, w.sizzle, w.bloom, CY_PLAY_NAMES_EXT[w.play], res[idx[k]].NA, res[idx[k]].trips); }
	if (L["csv"]) for (int i = 0; i < n; i++) { const Set& w = sets[i]; printf("CSV,%s,%.2f,%.2f,%.2f,%.3f,%.0f,%.2f,%d,%.2f,%d,%d,%zu,%.1f\n", CY_TYPES[w.type].name, w.size, w.weight, w.taper, w.rise, w.flange, w.tension, w.sizzle, w.bloom, w.play, res[i].NA, res[i].trips, res[i].cpu); }
}
