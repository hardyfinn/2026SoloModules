// Renders every factory preset (test/presets.txt, written by tools/make_presets.py) through the
// engine: each must build, stay finite and in range, and make a sound of a sensible level.
//   ./test/preset_check [out-dir]     writes presets-cymbolism.wav, presets-extended.wav and presets-lite.wav there
#include "../src/CyEngine.hpp"
#include <cstdio>
#include <fstream>
#include <map>
#include <sstream>
#include <thread>
using namespace cy;

struct Preset {
	std::string name, gesture, model;
	std::map<std::string, double> v;
	std::vector<float> L, R;
	std::string report;
	bool ok = false;
};

static void writeWav(const std::string& path, const std::vector<float>& L, const std::vector<float>& R) {
	FILE* fp = fopen(path.c_str(), "wb");
	if (!fp) return;
	uint32_t n = (uint32_t)L.size(), bytes = n * 4;
	auto w32 = [&](uint32_t x) { fwrite(&x, 4, 1, fp); };
	auto w16 = [&](uint16_t x) { fwrite(&x, 2, 1, fp); };
	fwrite("RIFF", 1, 4, fp); w32(36 + bytes); fwrite("WAVE", 1, 4, fp);
	fwrite("fmt ", 1, 4, fp); w32(16); w16(1); w16(2); w32(48000); w32(48000 * 4); w16(4); w16(16);
	fwrite("data", 1, 4, fp); w32(bytes);
	for (uint32_t i = 0; i < n; i++)
		for (float x : {L[i], R[i]}) { int16_t q = (int16_t)std::lround(std::max(-1.f, std::min(1.f, x / 10.f)) * 32767.f); fwrite(&q, 2, 1, fp); }
	fclose(fp);
}

static void render(Preset& p) {
	auto& v = p.v;
	bool ext = p.model == "CymbolismExtended";
	GeomParams gp;
	gp.type = (int)v["type"];
	gp.weight = v["weight"]; gp.taper = v["taper"]; gp.rise = v["rise"]; gp.bell = v["bell"]; gp.flangeDeg = v["flange"];
	gp.alloy = (int)v["alloy"];
	gp.tension = ext ? 0.0 : v["tension"];
	gp.hole = ext ? v["hole"] : -1.0;
	if (ext && v["size"] > 1.5) gp.band = std::round(v["size"] * 4) / 4;
	gp.sampleRate = 48000;
	if (p.model == "CymbolismLite") gp.quality = 3;   // Lite's default engine (MID)
	auto M = buildModel(gp);
	Controls c;
	c.size = v["size"]; c.decay = v["decay"]; c.bloom = v["bloom"]; c.damp = v["damp"]; c.mount = v["mount"]; c.hammer = v["hammer"];
	c.mic = v["mic"]; c.sizzle = v["sizzle"]; c.position = v["position"]; c.hardness = v["hardness"]; c.force = v["force"];
	c.rate = v["rate"]; c.play = (int)v["play"]; c.seed = (uint64_t)v["seed"];
	if (ext) {
		Unreal& u = c.u;
		u.morph = v["morph"]; u.warp = v["warp"]; u.pivot = v["pivot"]; u.tune = v["tune"]; u.scale = (int)v["scale"]; u.root = v["root"];
		u.tilt = v["tilt"]; u.swell = v["swell"]; u.colour = v["colour"]; u.flow = v["flow"]; u.load = v["load"]; u.loadPos = v["loadpos"];
		u.pickup = v["pickup"]; u.width = v["width"]; u.bounce = v["bounce"]; u.tension = v["tension"];
	}
	Engine E;
	E.ctl = c;
	E.init(M, 48000);
	if (ext && v["morph"] > 0) {
		GeomParams gb;
		gb.fromType((int)v["typeb"]);
		gb.alloy = gp.alloy;
		gb.sampleRate = 48000;
		E.setMorphModel(*buildModel(gb));
	}
	const double dur = 5.0;
	int n = (int)(dur * 48000);
	p.L.assign(n, 0.f);
	p.R.assign(n, 0.f);
	for (int s = 0; s < n; s += 32) {
		double t = (double)s / 48000;
		if (p.gesture == "strike" && s == 0) E.strike(c.force);
		if (p.gesture == "roll") E.ctl.roll = t < 3.0;
		if (p.gesture == "hold") { E.ctl.u.sustain = t < 3.0 ? v["sustain"] : 0.0; if (s == 0) E.strike(c.force); }
		Engine::Extra ex;
		ex.R = p.R.data() + s;
		E.process(nullptr, p.L.data() + s, 32, ext ? &ex : nullptr);
	}
	if (!ext) p.R = p.L;
	double pk = 0, e = 0;
	bool finite = true;
	for (int i = 0; i < n; i++) { pk = std::max(pk, (double)std::max(std::fabs(p.L[i]), std::fabs(p.R[i]))); e += (double)p.L[i] * p.L[i]; finite &= std::isfinite(p.L[i]) && std::isfinite(p.R[i]); }
	double e1 = 0;
	for (int i = 0; i < 48000; i++) e1 += (double)p.L[i] * p.L[i];
	double eLast = 0;
	for (int i = n - 24000; i < n; i++) eLast += (double)p.L[i] * p.L[i];
	double rms = std::sqrt(e / n);
	p.ok = finite && pk > 0.3 && pk <= 10.01 && E.guardEvents <= 2;
	char b[300];
	snprintf(b, sizeof b, "%-18s %-22s %-11s %5d modes  build %5.0f ms  aScale %.2f  peak %5.2f V  rms %5.2f V  last half second %4.0f dB  guard %d",
	         p.model.c_str(), p.name.c_str(), CY_TYPES[gp.type].name, E.N, M->buildMs, M->aScale, pk, rms,
	         10 * std::log10(eLast / 24000 / std::max(1e-30, e1 / 48000) + 1e-30), E.guardEvents);
	p.report = b;
}

int main(int argc, char** argv) {
	std::string dir = argc > 1 ? argv[1] : "";
	std::ifstream in("test/presets.txt");
	if (!in) { fprintf(stderr, "run from the plugin folder (test/presets.txt)\n"); return 2; }
	std::vector<Preset> ps;
	std::string line;
	while (std::getline(in, line)) {
		if (line.empty()) continue;
		Preset p;
		size_t a = line.find('|'), b = line.find('|', a + 1);
		p.name = line.substr(0, a);
		p.gesture = line.substr(a + 1, b - a - 1);
		std::istringstream ss(line.substr(b + 1));
		std::string kv;
		while (ss >> kv) {
			size_t eq = kv.find('=');
			std::string k = kv.substr(0, eq), val = kv.substr(eq + 1);
			if (k == "model") p.model = val;
			else p.v[k] = atof(val.c_str());
		}
		ps.push_back(p);
	}
	// four at a time
	for (size_t i = 0; i < ps.size(); i += 4) {
		std::vector<std::thread> th;
		for (size_t j = i; j < std::min(ps.size(), i + 4); j++) th.emplace_back([&ps, j] { render(ps[j]); });
		for (auto& t : th) t.join();
	}
	int fails = 0;
	for (auto& p : ps) { printf("  %s  %s\n", p.ok ? "ok  " : "FAIL", p.report.c_str()); if (!p.ok) fails++; }
	if (!dir.empty()) {
		for (const char* model : {"Cymbolism", "CymbolismExtended", "CymbolismLite"}) {
			std::vector<float> L, R;
			for (auto& p : ps) {
				if (p.model != model) continue;
				L.insert(L.end(), p.L.begin(), p.L.end());
				R.insert(R.end(), p.R.begin(), p.R.end());
				L.insert(L.end(), 9600, 0.f);
				R.insert(R.end(), 9600, 0.f);
			}
			std::string m = model;
			writeWav(dir + (m == "Cymbolism" ? "/presets-cymbolism.wav" : m == "CymbolismLite" ? "/presets-lite.wav" : "/presets-extended.wav"), L, R);
		}
	}
	if (fails) printf("preset_check: %d FAILED\n", fails);
	else printf("preset_check: all %zu presets ok\n", ps.size());
	return fails ? 1 : 0;
}
