// Checks the analyser on made-up breaks with known hits. Build: make -C .. check
#include "synth_break.hpp"
#include <cstdio>
using namespace breaker;
int main() {
	const char* nm = "KSHO";
	int totalOk = 0, totalTruth = 0, totalFound = 0;
	for (int variant = 0; variant < 2; variant++) {
		std::vector<testbreak::Truth> truth;
		auto a = testbreak::make(44100.f, truth, variant);
		for (float sens : {0.25f, 0.5f, 0.75f}) {
			BreakData* d = analyse(a, "test", sens, 0, {});
			int ok = 0, matched = 0;
			for (auto& t : truth) {
				int expect = t.type == 4 ? OTHER : t.type;
				for (auto& h : d->hits)
					if (std::fabs(h.start / 44100.0 - t.t) < 0.012) { matched++; if (h.type == expect) ok++; break; }
			}
			std::printf("variant %d sens %.2f: %zu found / %zu real, %d matched, %d right type, %d bars %.1f BPM\n",
				variant, sens, d->hits.size(), truth.size(), matched, ok, d->loopBars, d->bpm);
			if (sens == 0.5f) {
				totalOk += ok; totalTruth += (int) truth.size(); totalFound += (int) d->hits.size();
				for (auto& h : d->hits) {
					const testbreak::Truth* tr = nullptr;
					for (auto& t : truth) if (std::fabs(h.start / 44100.0 - t.t) < 0.012) tr = &t;
					std::printf("  %6.3fs step %2d %c%s  sub %.2f body %.2f mid %.2f air %.2f ring %.2f loud %.2f  truth %c\n",
						h.start / 44100.0, h.step, nm[h.type], h.ghost ? "g" : " ", h.sub, h.body, h.mid, h.air, h.ring, h.loud,
						tr ? (tr->type == 4 ? 'o' : nm[tr->type]) : '-');
				}
			}
			delete d;
		}
	}
	std::printf("default SENS: %d of %d right (%d found)\n", totalOk, totalTruth, totalFound);
	return 0;
}
