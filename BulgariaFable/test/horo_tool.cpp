// Demo tool: runs the Horo engine over a list of segments and prints every output rise.
//   test/horo_tool segments.txt [accent bars phraseMode]
// segments.txt: lines "seconds pulses_per_second slot lilt". Output lines: "t kind value", kind one of
// PULSE (value = accent volts), BEAT, LONG, BAR, PHRASE.
#include "../src/dsp/HoroEngine.hpp"
#include <cstdio>
#include <cstdlib>
#include <vector>

using namespace horo;

int main(int argc, char** argv) {
	if (argc < 2) { std::fprintf(stderr, "usage: horo_tool segments [accent bars phraseMode]\n"); return 1; }
	struct Seg { double secs, rate; int slot; double lilt; };
	std::vector<Seg> segs;
	FILE* f = std::fopen(argv[1], "r");
	if (!f) return 1;
	Seg s;
	while (std::fscanf(f, "%lf %lf %d %lf", &s.secs, &s.rate, &s.slot, &s.lilt) == 4) segs.push_back(s);
	std::fclose(f);
	const double sr = 48000;
	Engine e; e.init(sr);
	Params p;
	p.accent = argc > 2 ? std::atof(argv[2]) : 0.8;
	p.bars = argc > 3 ? std::atoi(argv[3]) : 4;
	p.phraseMode = argc > 4 ? std::atoi(argv[4]) : 0;
	Inputs in;
	long n = 0; double nextEdge = 0;
	bool pP = false, pB = false, pL = false, pBar = false, pPh = false;
	for (const Seg& g : segs) {
		long end = n + (long)(g.secs * sr);
		p.slot = g.slot; p.lilt = g.lilt;
		for (; n < end; n++) {
			double t = n / sr;
			if (t >= nextEdge) nextEdge += 1.0 / g.rate;
			in.clock = (t - (nextEdge - 1.0 / g.rate) < 0.001) ? 10 : 0;
			e.process(p, in);
			if (e.pulse && !pP) std::printf("%.6f PULSE %.3f\n", t, e.accentV);
			if (e.beat && !pB) std::printf("%.6f BEAT 0\n", t);
			if (e.lng && !pL) std::printf("%.6f LONG 0\n", t);
			if (e.barOut() && !pBar) std::printf("%.6f BAR 0\n", t);
			if (e.phraseOut() && !pPh) std::printf("%.6f PHRASE 0\n", t);
			pP = e.pulse; pB = e.beat; pL = e.lng; pBar = e.barOut(); pPh = e.phraseOut();
		}
	}
	return 0;
}
