// Demo tool: runs the C++ engine over an event list and writes per-voice pitch (cents from C4), gate and push
// at 1 kHz as float32, [frames][8 voices][3]. Used by tools/make_demos.py.
//   test/tool "<15 param values, ParamIds order>" events.txt out.f32 seconds [seed]
// events.txt: lines "t voct gate phrase slide shake whoop" (values held until the next line).
#include "../src/dsp/BulgariaEngine.hpp"
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <sstream>
#include <string>

using namespace bulgaria;

int main(int argc, char** argv) {
	if (argc < 5) { std::fprintf(stderr, "usage: tool params events out secs [seed]\n"); return 1; }
	std::vector<double> pv; { std::stringstream ss(argv[1]); double x; while (ss >> x) pv.push_back(x); }
	if (pv.size() != 15) { std::fprintf(stderr, "need 15 params\n"); return 1; }
	Params p;
	p.root = (int)pv[0]; p.oct = (int)pv[1]; p.mode = (int)pv[2]; p.tune = pv[3]; p.voices = (int)pv[4]; p.spread = pv[5];
	p.rub = pv[6]; p.hz = pv[7] > 0.5; p.chanceMode = pv[8] > 0.5; p.chance = pv[9]; p.resolve = pv[10] > 0.5;
	p.slide = pv[11]; p.shake = pv[12]; p.whoop = pv[13]; p.loose = pv[14];
	struct Ev { double t, voct, gate, phrase, sl, sh, wh; };
	std::vector<Ev> ev;
	FILE* f = std::fopen(argv[2], "r");
	if (!f) return 1;
	Ev e;
	while (std::fscanf(f, "%lf %lf %lf %lf %lf %lf %lf", &e.t, &e.voct, &e.gate, &e.phrase, &e.sl, &e.sh, &e.wh) == 7) ev.push_back(e);
	std::fclose(f);
	double secs = std::atof(argv[4]);
	uint64_t seed = argc > 5 ? std::strtoull(argv[5], nullptr, 10) : 1;
	const double sr = 48000;
	Engine en; en.init(sr, seed);
	Inputs in; in.gatePatched = true;
	FILE* o = std::fopen(argv[3], "wb");
	size_t k = 0;
	int n = (int)(secs * sr);
	for (int i = 0; i < n; i++) {
		double t = i / sr;
		while (k < ev.size() && ev[k].t <= t) {
			in.voct = ev[k].voct; in.gate = ev[k].gate; in.phrase = ev[k].phrase;
			in.slideTrig = ev[k].sl; in.shakeTrig = ev[k].sh; in.whoopTrig = ev[k].wh; k++;
		}
		en.process(p, in);
		if (i % 48 == 0) {
			float fr[NV * 3];
			for (int v = 0; v < NV; v++) {
				fr[3 * v] = (float)en.v[v].outCents; fr[3 * v + 1] = (float)(en.v[v].gate ? 1 : 0); fr[3 * v + 2] = (float)en.pushV(v);
			}
			std::fwrite(fr, sizeof(float), NV * 3, o);
		}
	}
	std::fclose(o);
	return 0;
}
