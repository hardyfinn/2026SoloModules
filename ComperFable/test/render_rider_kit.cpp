// Rider's kit against test/rider_kit_cases.json: the same strokes through the C++ kit, measured as the reference was
// (26 band filters per channel, energy per time window), must land on the same numbers.
//   c++ -O3 -std=c++17 test/render_rider_kit.cpp -o test/render_rider_kit && ./test/render_rider_kit [OUTDIR]
#include "../src/dsp/RiderKit.hpp"
#include "rider_kit_cases.h"
#include <cstdio>
#include <cstring>
#include <string>
#include <cstdint>
using namespace riderkit;

static void writeWav(const std::string& path, const std::vector<float>& l, const std::vector<float>& r) {
	FILE* f = fopen(path.c_str(), "wb"); if (!f) return;
	int n = (int)l.size(); int bytes = n * 8;
	auto w32 = [&](uint32_t v) { fwrite(&v, 4, 1, f); }; auto w16 = [&](uint16_t v) { fwrite(&v, 2, 1, f); };
	fwrite("RIFF", 1, 4, f); w32(36 + bytes); fwrite("WAVEfmt ", 1, 8, f); w32(16); w16(3); w16(2); w32(SR); w32(SR * 8); w16(8); w16(32);
	fwrite("data", 1, 4, f); w32(bytes);
	for (int i = 0; i < n; i++) { fwrite(&l[i], 4, 1, f); fwrite(&r[i], 4, 1, f); }
	fclose(f);
}

int main(int argc, char** argv) {
	std::string out = argc > 1 ? argv[1] : "";
	const char* only = argc > 2 ? argv[2] : nullptr;
	int fails = 0, ran = 0; double worstMean = 0, worstCell = 0;
	for (int ci = 0; ci < NUM_RKCASES; ci++) {
		const RKCase& c = RKCASES[ci];
		if (only && !strstr(c.name, only)) continue;
		ran++;
		Kit* k = new Kit(); k->init(c.seed);
		k->kp.level = c.kit[0]; k->kp.tune = c.kit[1]; k->kp.decay = c.kit[2]; k->kp.bright = c.kit[3]; k->kp.width = c.kit[4]; k->kp.age = c.kit[5];
		for (int i = 0; i < 5; i++) k->kp.mix[i] = c.mix[i];
		// the reference sorts strokes by tick, stably
		for (int i = 0; i < c.nstrokes; i++) k->strokeAtTick((long long)std::ceil(c.strokes[i].t * SR / CTRL), c.strokes[i].voice, c.strokes[i].vel);
		for (int i = 0; i < c.nsweeps; i++) k->sweepAt((long long)std::nearbyint(c.sweeps[i].t * SR), std::max(1LL, (long long)std::nearbyint(c.sweeps[i].dur * 1.08 * SR)), c.sweeps[i].vel);
		int n = (int)(c.dur * SR) / CTRL * CTRL;
		std::vector<float> L(n), R(n); std::vector<double> y[2] = {std::vector<double>(n), std::vector<double>(n)};
		double peak = 0;
		for (int s = 0; s < n; s++) { double l, r; k->process(l, r); y[0][s] = l; y[1][s] = r; L[s] = (float)l; R[s] = (float)r; peak = std::max(peak, std::max(std::fabs(l), std::fabs(r))); }
		delete k;
		// measure
		std::vector<float> cells(2 * NB * c.nwins);
		for (int ch = 0; ch < 2; ch++)
			for (int i = 0; i < NB; i++) {
				double z[2][2] = {{0, 0}, {0, 0}}; std::vector<double> b(n);
				for (int s = 0; s < n; s++) {
					double v = y[ch][s];
					for (int q = 0; q < 2; q++) { const double* S = SOS[i][q]; double o = S[0] * v + z[q][0]; z[q][0] = S[1] * v - S[3] * o + z[q][1]; z[q][1] = S[2] * v - S[4] * o; v = o; }
					b[s] = v * v;
				}
				for (int w = 0; w < c.nwins; w++) {
					int a = (int)(c.wins[w].a * SR), e = std::min(n, (int)(c.wins[w].b * SR)); double sum = 0;
					for (int s = a; s < e; s++) sum += b[s];
					cells[(ch * NB + i) * c.nwins + w] = (float)(10 * std::log10(sum + 1e-16));
				}
			}
		float top = -1e9; for (int i = 0; i < 2 * NB * c.nwins; i++) top = std::max(top, c.cells[i]);
		double sum = 0, worst = 0; int cnt = 0, wi = 0;
		for (int i = 0; i < 2 * NB * c.nwins; i++) {
			if (c.cells[i] < top - 60) continue;
			double d = std::fabs(cells[i] - c.cells[i]); sum += d; cnt++;
			if (d > worst) { worst = d; wi = i; }
		}
		double mean = cnt ? sum / cnt : 0;
		bool ok = mean <= 0.3 && worst <= 2.0 && std::fabs(peak - c.peak) <= 0.05 * c.peak;
		worstMean = std::max(worstMean, mean); worstCell = std::max(worstCell, worst);
		if (!ok) {
			fails++;
			int ch = wi / (NB * c.nwins), band = (wi / c.nwins) % NB, w = wi % c.nwins;
			printf("FAIL %-46s mean %.3f dB, worst %.2f dB (ch %d band %d window %d: %.2f vs %.2f), peak %.4f vs %.4f\n", c.name, mean, worst, ch, band, w, cells[wi], c.cells[wi], peak, c.peak);
		}
		if (!out.empty()) {
			std::string nm = c.name; for (auto& ch : nm) if (!isalnum((unsigned char)ch) && ch != '-' && ch != '.') ch = '_';
			writeWav(out + "/" + nm + ".wav", L, R);
		}
	}
	printf("kit: %d of %d renders match the reference (worst mean %.3f dB, worst cell %.2f dB)\n", ran - fails, ran, worstMean, worstCell);
	return fails ? 1 : 0;
}
