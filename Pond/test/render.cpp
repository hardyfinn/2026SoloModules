// Offline checks and demo renders for Pond.  No Rack needed:  make test
//   ./test/render OUTDIR
// Every check prints PASS or FAIL; the program exits non-zero if any fails.
#include "../src/dsp/Pond.hpp"
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <functional>
#include <cmath>
#include <algorithm>

using namespace pond;

static int failures = 0;
static void check(bool ok, const char* what, const std::string& detail) {
	printf("%s  %-60s %s\n", ok ? "PASS" : "FAIL", what, detail.c_str());
	if (!ok) failures++;
}
template <typename... A> static std::string fmt(const char* f, A... a) { char b[512]; snprintf(b, sizeof b, f, a...); return b; }

static void writeWav(const std::string& path, const std::vector<float>& L, const std::vector<float>& R, int sr) {
	FILE* fp = fopen(path.c_str(), "wb");
	if (!fp) { perror(path.c_str()); return; }
	int n = (int)L.size();
	uint32_t dataBytes = n * 4, riff = 36 + dataBytes, fmtLen = 16, srate = sr, brate = sr * 4;
	uint16_t pcm = 1, ch = 2, align = 4, bits = 16;
	fwrite("RIFF", 1, 4, fp); fwrite(&riff, 4, 1, fp); fwrite("WAVEfmt ", 1, 8, fp);
	fwrite(&fmtLen, 4, 1, fp); fwrite(&pcm, 2, 1, fp); fwrite(&ch, 2, 1, fp); fwrite(&srate, 4, 1, fp);
	fwrite(&brate, 4, 1, fp); fwrite(&align, 2, 1, fp); fwrite(&bits, 2, 1, fp);
	fwrite("data", 1, 4, fp); fwrite(&dataBytes, 4, 1, fp);
	for (int i = 0; i < n; i++) {
		int16_t a = (int16_t)std::max(-32767.f, std::min(32767.f, L[i] / 10.f * 32767.f));
		int16_t b = (int16_t)std::max(-32767.f, std::min(32767.f, R[i] / 10.f * 32767.f));
		fwrite(&a, 2, 1, fp); fwrite(&b, 2, 1, fp);
	}
	fclose(fp);
}

struct Events {
	bool hushHeld = false, hush = false, call = false, tick = false, clocked = false;
	double kick = 0.0;           // add a kick drum (demo only)
};
typedef std::function<void(double t, Params& p, Events& e)> Auto;

struct Run {
	std::vector<float> L, R;
	std::vector<std::vector<double>> onsets;   // call starts per frog, seconds
	std::vector<double> calling;               // frogs calling, per 10 ms
	std::vector<double> rms;                   // per 10 ms
	double peak = 0.0;
};

static Run render(Params base, double seconds, Auto aut, uint32_t seed = 1, double sr = 48000.0) {
	Run r;
	Pond* pd = new Pond;
	pd->setSampleRate(sr); pd->reseed(seed);
	int n = (int)(seconds * sr);
	r.L.resize(n); r.R.resize(n); r.onsets.resize(MAX_FROGS);
	double voct[MAX_FROGS] = {};
	int blk = (int)(0.01 * sr); double acc = 0; int callAcc = 0;
	double kickPh = 0, kickT = 10;
	for (int s = 0; s < n; s++) {
		double t = s / sr;
		Params p = base; Events e;
		if (aut) aut(t, p, e);
		if (e.hush) pd->hush();
		if (e.call) pd->callNow(-1, p);
		pd->clocked = e.clocked;
		if (e.tick) pd->clockTick();
		double L, R;
		pd->process(p, voct, e.hushHeld, L, R);
		double l = softClip(L * OUT_GAIN), rr = softClip(R * OUT_GAIN);
		{
			// simple kick for the clocked demo
			if (e.tick && e.kick > 0) { kickT = 0; kickPh = 0; }
			if (kickT < 0.5) {
				double f = 45.0 + 110.0 * std::exp(-kickT / 0.03);
				kickPh += f / sr;
				double k = std::sin(2 * PI * kickPh) * std::exp(-kickT / 0.28);
				l += k * 6.0; rr += k * 6.0;
			}
			kickT += 1.0 / sr;
		}
		r.L[s] = (float)l; r.R[s] = (float)rr;
		r.peak = std::max(r.peak, std::max(std::fabs(l), std::fabs(rr)));
		for (int i = 0; i < pd->n; i++) if (pd->frogs[i].trigOut) { r.onsets[i].push_back(t); pd->frogs[i].trigOut = false; }
		acc += l * l + rr * rr;
		if (s % CTL == 0) callAcc += pd->callingCount;
		if ((s + 1) % blk == 0) {
			r.rms.push_back(std::sqrt(acc / (2.0 * blk)));
			r.calling.push_back(callAcc / (double)(blk / CTL));
			acc = 0; callAcc = 0;
		}
	}
	delete pd;
	return r;
}

static double meanRms(const Run& r, double t0, double t1) {
	int a = (int)(t0 * 100), b = std::min((int)r.rms.size(), (int)(t1 * 100));
	double s = 0; int c = 0;
	for (int i = a; i < b; i++) { s += r.rms[i] * r.rms[i]; c++; }
	return c ? std::sqrt(s / c) : 0.0;
}
static int onsetsIn(const Run& r, double t0, double t1) {
	int c = 0;
	for (auto& v : r.onsets) for (double t : v) if (t >= t0 && t < t1) c++;
	return c;
}
static std::vector<double> allOnsets(const Run& r) {
	std::vector<double> v;
	for (auto& o : r.onsets) v.insert(v.end(), o.begin(), o.end());
	std::sort(v.begin(), v.end());
	return v;
}

int main(int argc, char** argv) {
	std::string out = argc > 1 ? argv[1] : "test/out";
	const double SR = 48000.0;

	// ---- 1. every species sounds, at a sane level, and calls
	{
		std::vector<float> tL, tR;
		for (int s = 0; s < NUM_SPECIES; s++) {
			Params p; p.spA = s; p.frogs = 6; p.talk = 0.6; p.busy = 0.85; p.bouts = 0.2; p.space = 0.35;
			Run r = render(p, 6.0, [](double t, Params& q, Events& e) { e.call = (t < 1.0 / 48000.0) || (t >= 0.7 && t < 0.7 + 1.0 / 48000.0); }, 11 + s);
			double lv = meanRms(r, 0.5, 6.0);
			int calls = onsetsIn(r, 0.0, 6.0);
			check(lv > 0.08 && lv < 3.0 && r.peak < 9.9, fmt("%-14s level", SPECIES[s].name).c_str(),
			      fmt("rms %.2f V, peak %.2f V, %d calls", lv, r.peak, calls));
			// species tour demo: 4 s each with a short fade
			int len = (int)(4.0 * SR), st = (int)(1.0 * SR);
			for (int i = 0; i < len; i++) {
				double g = std::min(1.0, std::min(i / (0.05 * SR), (len - i) / (0.3 * SR)));
				tL.push_back((float)(r.L[st + i] * g)); tR.push_back((float)(r.R[st + i] * g));
			}
		}
		writeWav(out + "/pond-species-tour.wav", tL, tR, (int)SR);
	}

	// ---- 2. HUSH silences the pond within 60 ms; recovery is gradual
	{
		Params p; p.spA = 0; p.spB = 1; p.mix = 0.25; p.frogs = 10; p.talk = 0.8; p.busy = 0.8; p.returnS = 12.0;
		Run r = render(p, 30.0, [](double t, Params&, Events& e) { if (t >= 8.0 && t < 8.0 + 1.0 / 48000.0) e.hush = true; });
		double before = meanRms(r, 4.0, 8.0), after = meanRms(r, 8.07, 9.0);
		check(after < 0.05 * before, "HUSH: silent within 70 ms", fmt("%.3f V -> %.4f V", before, after));
		auto on = allOnsets(r);
		double first = -1; for (double t : on) if (t > 8.0) { first = t - 8.0; break; }
		check(first > 1.0, "HUSH: first brave call comes after a pause", fmt("%.1f s after the hush", first));
		int c1 = onsetsIn(r, 9.0, 14.0), c2 = onsetsIn(r, 20.0, 25.0), c0 = onsetsIn(r, 3.0, 8.0);
		check(c1 < c2, "HUSH: chorus builds up again", fmt("calls before %d, 1-6 s after %d, 12-17 s after %d", c0, c1, c2));
		// a held HUSH keeps them silent, even when CALL is pressed
		Run h = render(p, 16.0, [](double t, Params&, Events& e) {
			e.hushHeld = t >= 4.0 && t < 10.0;
			e.call = t >= 6.0 && t < 6.0 + 1.0 / 48000.0;
		});
		check(onsetsIn(h, 4.0, 10.0) == 0 && meanRms(h, 4.1, 10.0) < 0.01, "HUSH held: nobody calls, even on CALL",
		      fmt("%d calls, %.4f V", onsetsIn(h, 4.0, 10.0), meanRms(h, 4.1, 10.0)));
	}

	// ---- 3. TALK: answering avoids overlaps, synchronising stacks calls together
	{
		auto overlapAndCluster = [&](double talk, double& overlapFrac, double& cluster) {
			Params p; p.spA = 2; p.frogs = 8; p.talk = talk; p.busy = 0.8; p.bouts = 0.0;
			Run r = render(p, 40.0, nullptr, 5);
			int both = 0, any = 0;
			for (size_t i = 200; i < r.calling.size(); i++) { if (r.calling[i] >= 0.5) any++; if (r.calling[i] >= 1.5) both++; }
			overlapFrac = any ? both / (double)any : 0;
			auto on = allOnsets(r);
			int close = 0;
			for (size_t i = 1; i < on.size(); i++) if (on[i] - on[i - 1] < 0.1) close++;
			cluster = on.size() > 1 ? close / (double)(on.size() - 1) : 0;
		};
		double oFree, cFree, oAns, cAns, oSync, cSync;
		overlapAndCluster(0.0, oFree, cFree);
		overlapAndCluster(1.0, oAns, cAns);
		overlapAndCluster(-1.0, oSync, cSync);
		check(oAns < 0.8 * oFree, "TALK ANSWER: fewer overlapping calls than FREE", fmt("overlap %.2f vs free %.2f", oAns, oFree));
		check(cSync > 1.5 * cFree, "TALK SYNC: calls start together", fmt("onsets within 100 ms: %.2f vs free %.2f", cSync, cFree));
	}

	// ---- 4. temperature: warmer = faster pulses and more calls
	{
		Params p; p.spA = 1; p.frogs = 6; p.busy = 0.8;
		Params pc = p; pc.tempC = 12.0;
		Params pw = p; pw.tempC = 28.0;
		Run rc = render(pc, 30.0, nullptr, 3), rw = render(pw, 30.0, nullptr, 3);
		int cc = onsetsIn(rc, 0, 30), cw = onsetsIn(rw, 0, 30);
		check(cw > cc, "TEMP: warm pond calls more often", fmt("12 C: %d calls, 28 C: %d calls", cc, cw));
		Params pf = p; pf.tempC = 5.0;
		Run rf = render(pf, 15.0, nullptr, 3);
		check(onsetsIn(rf, 2, 15) == 0, "TEMP: too cold to sing at 5 C", fmt("%d calls", onsetsIn(rf, 2, 15)));
	}

	// ---- 5. CPU-ish: 16 frogs, 10 s
	{
		Params p; p.spA = 3; p.spB = 9; p.mix = 0.5; p.frogs = 16; p.busy = 1.0;
		Run r = render(p, 10.0, nullptr, 9);
		check(r.peak < 10.0 && meanRms(r, 1, 10) > 0.05, "16 frogs: no blow-up", fmt("peak %.2f V", r.peak));
	}

	// ---- demo: a night at the pond (call and answer, a heron, a tentative return)
	{
		Params p; p.spA = 0; p.spB = 1; p.mix = 0.0; p.frogs = 4; p.talk = 0.9; p.busy = 0.75; p.bouts = 0.35;
		p.space = 0.45; p.tempC = 17.0; p.returnS = 10.0;
		Run r = render(p, 96.0, [](double t, Params& q, Events& e) {
			// 0-14: four peepers calling and answering; the pond fills up to 12 by 24 s
			q.frogs = t < 14 ? 4 : std::min(12, 4 + (int)((t - 14) * 0.8));
			// toads join from 18 s
			q.mix = t < 18 ? 0.0 : std::min(0.3, (t - 18) * 0.03);
			q.tempC = 17.0 + std::min(4.0, t * 0.05);
			// 36 s: a heron lands: HUSH held for 3 s, then the tentative return
			if (t >= 36.0 && t < 39.0) e.hushHeld = true;
			// 64 s: a bullfrog / green frog pond, a frog starts it off
			if (t >= 64.0) { q.spA = 3; q.spB = 4; q.mix = 0.5; q.talk = 0.9; q.frogs = 8; q.space = 0.55; }
			if (t >= 64.0 && t < 64.0 + 1.0 / 48000.0) e.call = true;
			// 82 s: another scare
			if (t >= 82.0 && t < 82.0 + 1.0 / 48000.0) e.hush = true;
		}, 21);
		writeWav(out + "/pond-night.wav", r.L, r.R, (int)SR);
		printf("night: rms 5-14 %.2f, 37-41 %.3f, 45-64 %.2f, peak %.2f\n",
		       meanRms(r, 5, 14), meanRms(r, 37, 41), meanRms(r, 45, 64), r.peak);
	}

	// ---- demo: clocked at 128 BPM with a kick (frogs on the grid, answering)
	{
		Params p; p.spA = 9; p.spB = 2; p.mix = 0.35; p.frogs = 8; p.talk = 0.7; p.busy = 0.9; p.bouts = 0.2;
		p.space = 0.35; p.tempC = 22.0;
		double step = 60.0 / 128.0 / 4.0;   // 16ths
		Run r = render(p, 30.0, [step](double t, Params&, Events& e) {
			double ph = std::fmod(t, step);
			e.tick = ph < 1.0 / 48000.0; e.clocked = true;
			// kick on quarters only: we only use tick to reset, so model quarters by a gate
			int sixteenth = (int)std::floor(t / step + 1e-9);
			e.kick = (sixteenth % 4 == 0) ? 1.0 : 0.0;
			if (t >= 18.0 && t < 18.0 + 1.0 / 48000.0) e.hush = true;
		}, 4);
		writeWav(out + "/pond-clocked-128.wav", r.L, r.R, (int)SR);
	}

	printf("%s\n", failures ? "SOME CHECKS FAILED" : "ALL CHECKS PASSED");
	return failures ? 1 : 0;
}
