// Prints every voice's 16 knob slots (stage, name, value, knob position, stock position) as
// JSON for tools/preview_bench.py. Optional argument: a factory kit number to load first.
//   g++ -std=c++11 tools/bench_dump.cpp -o /tmp/bench_dump && /tmp/bench_dump [kit] > /tmp/bench.json
#include "../src/BenchVoices.hpp"
using namespace bench;
int main(int argc, char** argv) {
	Kit kit;
	if (argc > 1) applyKit(kit, factoryKits()[atoi(argv[1])].text);
	printf("[");
	for (int v = 0; v < NV; v++) {
		const Layout& L = layout(kindOf(v));
		const Slots& S = slots(kindOf(v));
		printf("%s{\"code\":\"%s\",\"name\":\"%s\",\"colour\":\"#%06X\",\"slots\":[", v ? "," : "", code(v), longName(v), colour(v));
		for (int s = 0; s < S.n; s++) {
			int p = S.p[s];
			char buf[32];
			if (p == PAN) { float k = kit.v[v]->pan; if (std::fabs(k - 0.5f) < 0.01f) snprintf(buf, 32, "C"); else snprintf(buf, 32, "%s%.0f", k < 0.5f ? "L" : "R", std::fabs(k - 0.5f) * 200.f); }
			else format(buf, 32, kindOf(v), kit.v[v]->def(p), kit.v[v]->get(p));
			printf("%s[\"%s\",\"%s\",\"%s\",%g,%g]", s ? "," : "", L.blocks[S.blk[s]].name, p == PAN ? "PAN" : kit.v[v]->def(p).label, buf,
				kit.v[v]->get(p), kit.v[v]->stockOf(p));
		}
		printf("]}");
	}
	printf("]\n");
}
