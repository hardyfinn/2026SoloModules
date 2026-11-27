// Prints every voice's screen (blocks, parameter names, stock values) as JSON for
// tools/preview_bench.py.  g++ -std=c++11 tools/bench_dump.cpp -o /tmp/bench_dump
#include "../src/BenchVoices.hpp"
using namespace bench;
int main() {
	Kit kit;
	printf("[");
	for (int v = 0; v < NV; v++) {
		const Layout& L = layout(kindOf(v));
		printf("%s{\"code\":\"%s\",\"name\":\"%s\",\"colour\":\"#%06X\",\"blocks\":[", v ? "," : "", code(v), longName(v), colour(v));
		for (int b = 0; b < L.nBlocks; b++) {
			const Block& B = L.blocks[b];
			printf("%s{\"name\":\"%s\",\"col\":%g,\"row\":%g,\"p\":[", b ? "," : "", B.name, B.col, B.row);
			for (int j = 0; j < B.n; j++) {
				char buf[32]; int p = B.p[j];
				if (p == PAN) snprintf(buf, 32, "C"); else format(buf, 32, kindOf(v), kit.v[v]->def(p), kit.v[v]->get(p));
				printf("%s[\"%s\",\"%s\"]", j ? "," : "", p == PAN ? "PAN" : kit.v[v]->def(p).label, buf);
			}
			printf("]}");
		}
		printf("],\"arrows\":[");
		for (int a = 0; a < L.nArrows; a++) printf("%s[%d,%d]", a ? "," : "", L.arrows[a].from, L.arrows[a].to);
		printf("]}");
	}
	printf("]\n");
}
