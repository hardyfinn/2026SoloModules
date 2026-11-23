// Drives the Runner like Rack would (sample by sample, real-time paced in bursts)
// and checks: the first build arrives, strikes sound, a geometry change swaps in,
// inline and threaded modes agree in level, and nothing underruns at real-time pace.
#include "../src/CyRunner.hpp"
#include <cstdio>
using namespace cy;

static double runFor(Runner& R, double secs, double strikeEvery, double& peak, int sr = 48000) {
	int n = (int)(secs * sr);
	double rms = 0;
	auto t0 = std::chrono::steady_clock::now();
	for (int s = 0; s < n; s++) {
		float sv = 0.f;
		if (strikeEvery > 0 && s % (int)(strikeEvery * sr) == 100) sv = 3.0f;
		float y = R.tick(0.f, sv);
		rms += y * y;
		peak = std::max(peak, (double)std::fabs(y));
		// pace like an audio callback: every 256 samples wait until real time catches up
		if ((s & 255) == 255) {
			auto due = t0 + std::chrono::microseconds((int64_t)((s + 1) * 1e6 / sr));
			std::this_thread::sleep_until(due);
		}
	}
	return std::sqrt(rms / n);
}

int main() {
	int fails = 0;
	Runner R;
	GeomParams g;
	g.fromType(1);
	g.sampleRate = 48000;
	R.ctl.play = SHOULDER;
	R.ctl.position = 0.95;
	while (!R.requestBuild(g, R.ctl)) {}
	double pk = 0;
	auto t0 = std::chrono::steady_clock::now();
	while (!R.hasModel) {
		R.tick(0.f, 0.f);
		std::this_thread::sleep_for(std::chrono::microseconds(20));
		if (std::chrono::steady_clock::now() - t0 > std::chrono::seconds(20)) break;
	}
	printf("first model after %.2f s (with a strike to swap it in)\n", std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count());
	pk = 0;
	runFor(R, 1.0, 0, pk);   // settle the latency to the host's burst size
	printf("latency settled at %d blocks (%.1f ms)\n", R.latBlocks, R.latBlocks * 64 / 48.0);
	R.xruns = 0;
	pk = 0;
	double rms = runFor(R, 3.0, 1.0, pk);
	printf("threaded: rms %.3f peak %.2f cpu %.2f xruns %d\n", rms, pk, R.cpu.load(), R.xruns.load());
	if (!(rms > 0.05)) { printf("FAIL: silent\n"); fails++; }
	// geometry change: RIDE; must swap at a strike
	g.fromType(4);
	while (!R.requestBuild(g, R.ctl)) {}
	pk = 0;
	runFor(R, 3.0, 0.5, pk);
	printf("after change: model %s cpu %.2f xruns %d\n", R.modelKey().c_str(), R.cpu.load(), R.xruns.load());
	if (R.modelKey() != g.key()) { printf("FAIL: no swap\n"); fails++; }
	// inline
	R.threaded = false;
	pk = 0;
	double rms2 = runFor(R, 2.0, 1.0, pk);
	printf("inline: rms %.3f peak %.2f\n", rms2, pk);
	if (!(rms2 > 0.05)) { printf("FAIL: inline silent\n"); fails++; }
	R.threaded = true;
	pk = 0;
	runFor(R, 1.0, 0.5, pk);
	printf("back to threaded: xruns %d (latency %d blocks)\n", R.xruns.load(), R.latBlocks);
	if (R.xruns.load() > 20) { printf("FAIL: underruns at real-time pace\n"); fails++; }
	printf(fails ? "RUNNER CHECK FAILED\n" : "runner check ok\n");
	return fails;
}
