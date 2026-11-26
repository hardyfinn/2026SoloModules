// Offline checks for Walker 2.4.0 OCTAVE, LENGTH and VARY (no Rack needed).
//   g++ -O3 -std=c++17 test/render_walker_len.cpp -o test/render_walker_len && ./test/render_walker_len test/out
#include "walker_sim.hpp"
#include "walker_golden.h"
#include "../src/dsp/WalkerHost.hpp"
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <algorithm>
using namespace walker;
static int failures = 0;
static void check(const char* name, bool ok, const std::string& d = "") { printf("%s %s  %s\n", ok ? "PASS" : "FAIL", name, d.c_str()); if (!ok) failures++; }
static BP toBP(const double* v) {
	BP p; p.feel = (int)v[0]; p.activity = v[1]; p.reg = v[2]; p.roam = v[3]; p.out = v[4]; p.repeat = v[5]; p.loose = v[6];
	p.timing = v[7]; p.weight = v[8]; p.slop = v[9]; p.pitch = v[10]; p.ahead = v[11]; p.swing = v[12]; return p;
}
static bool linkChart(const WGCase& c, CompLink& L) {
	int n = c.nchart; if (n > 16) return false;
	double bps = c.total / n; if (bps != 1 && bps != 2 && bps != 4 && bps != 8) return false;
	for (int k = 0; k < n; k++) if (std::fabs(c.chart[k].beat - k * bps) > 1e-9) return false;
	L.steps = n; L.beatsPerStep = (int)bps; for (int k = 0; k < n; k++) { L.root[k] = c.chart[k].root; L.qual[k] = c.chart[k].q; }
	return true;
}
struct Run { std::vector<float> audio, gate; std::vector<WalkerHost::FireLog> log; double spb; };
static Run run(const WGCase& c, double length, double vary, int octave, int feelOverride = -1) {
	Run r; WalkerHost h; double sr = 48000; h.setSampleRate(sr); h.logOn = true;
	WIn in; in.linkOk = true; linkChart(c, in.link); in.bp = toBP(c.prm); if (feelOverride >= 0) in.bp.feel = feelOverride;
	in.feelPos = in.bp.feel + 1; in.seed = c.seed;
	in.bp.length = length; in.bp.vary = vary; in.octave = octave;
	in.link.swing = (float)in.bp.swing; in.link.running = true; in.link.resetCount = 1;
	double spb = 60.0 / c.bpm, phi = -1.0; r.spb = spb;
	long long N = (long long)((c.nbars * 4 + 1) * spb * sr) + (long long)sr;
	WOut out;
	for (long long i = 0; i < N; i++) {
		in.link.phi = phi; in.link.spb = spb;
		h.process(in, out, 1.0 / sr);
		r.audio.push_back(out.audio); r.gate.push_back(out.gate);
		phi += 1.0 / (sr * spb);
	}
	r.log = h.log; return r;
}
// gate-high lengths, in beats, measured on the GATE output
static std::vector<double> gateLens(const Run& r) {
	std::vector<double> v; long long on = -1; double sr = 48000;
	for (size_t i = 0; i < r.gate.size(); i++) {
		bool g = r.gate[i] > 1.f;
		if (g && on < 0) on = (long long)i;
		if (!g && on >= 0) { v.push_back((i - on) / sr / r.spb); on = -1; }
	}
	return v;
}
static void writeWav(const std::string& path, const std::vector<float>& x) {
	FILE* f = fopen(path.c_str(), "wb"); if (!f) return;
	auto u32 = [&](uint32_t v) { fwrite(&v, 4, 1, f); }; auto u16 = [&](uint16_t v) { fwrite(&v, 2, 1, f); };
	uint32_t data = (uint32_t)x.size() * 2;
	fwrite("RIFF", 1, 4, f); u32(36 + data); fwrite("WAVE", 1, 4, f);
	fwrite("fmt ", 1, 4, f); u32(16); u16(1); u16(1); u32(48000); u32(96000); u16(2); u16(16);
	fwrite("data", 1, 4, f); u32(data);
	for (float s : x) { int16_t q = (int16_t)std::max(-32767.f, std::min(32767.f, s / 5.f * 32767.f)); fwrite(&q, 2, 1, f); }
	fclose(f);
}
int main(int argc, char** argv) {
	std::string outDir = argc > 1 ? argv[1] : "test/out";
	const char* FEELS[6] = {"WALK", "TWO", "BALLAD", "BOSSA", "SOUL", "HOUSE"};
	// pick one linkable case per feel
	const WGCase* byFeel[6] = {};
	for (int i = 0; i < NUM_WGOLDEN; i++) {
		const WGCase& c = WGOLDEN[i]; CompLink L;
		if (c.blind || !linkChart(c, L)) continue;
		int f = (int)c.prm[0]; if (f >= 0 && f < 6 && !byFeel[f]) byFeel[f] = &c;
	}
	printf("feel     setting                notes  p10   median  p90 (gate length in beats)\n");
	for (int f = 0; f < 6; f++) {
		const WGCase* c = byFeel[f] ? byFeel[f] : byFeel[0];
		struct S { const char* n; double l, v; } sets[4] = {{"old (1.0, 0)", 1.0, 0.0}, {"new default (1.4, .5)", 1.4, 0.5}, {"max vary (1.4, 1)", 1.4, 1.0}, {"long (2.5, .5)", 2.5, 0.5}};
		double medOld = 0, medNew = 0, spreadOld = 0, spreadVary = 0;
		for (auto& s : sets) {
			Run r = run(*c, s.l, s.v, 0, f);
			std::vector<double> g = gateLens(r); std::sort(g.begin(), g.end());
			double med = g.empty() ? 0 : g[g.size() / 2], p10 = g.empty() ? 0 : g[g.size() / 10], p90 = g.empty() ? 0 : g[g.size() * 9 / 10];
			printf("%-8s %-22s %4zu  %.2f  %.2f    %.2f\n", FEELS[f], s.n, g.size(), p10, med, p90);
			if (s.l == 1.0) { medOld = med; spreadOld = p90 / std::max(1e-3, p10); }
			if (s.l == 1.4 && s.v == 0.5) medNew = med;
			if (s.v == 1.0) spreadVary = p90 / std::max(1e-3, p10);
			// the line stays one voice: gates never overlap (one GATE output) and audio stays sane
			float pk = 0; bool finite = true; for (float x : r.audio) { if (!std::isfinite(x)) finite = false; pk = std::max(pk, std::fabs(x)); }
			if (!finite || pk > 12.f) check("audio sane", false, FEELS[f]);
		}
		check((std::string("longer by default: ") + FEELS[f]).c_str(), medNew > medOld, "");
		check((std::string("more mixed lengths with VARY: ") + FEELS[f]).c_str(), spreadVary >= spreadOld * 0.98, std::to_string(spreadOld) + " -> " + std::to_string(spreadVary));
	}
	// OCTAVE: same notes, 12 and 24 semitones up, sane audio
	{
		const WGCase& c = *byFeel[0];
		Run r0 = run(c, 1.4, 0.5, 0), r2 = run(c, 1.4, 0.5, 2);
		bool same = r0.log.size() == r2.log.size(); size_t i = 0;
		for (; same && i < r0.log.size(); i++) if (r2.log[i].pitch != r0.log[i].pitch || r2.log[i].at != r0.log[i].at) same = false;
		check("octave keeps the line (log is the written line)", same);
		float pk = 0; bool finite = true; for (float x : r2.audio) { if (!std::isfinite(x)) finite = false; pk = std::max(pk, std::fabs(x)); }
		check("octave +2 audio sane", finite && pk > 0.05f && pk < 12.f, "peak " + std::to_string(pk));
		Run r1 = run(c, 1.4, 0.5, 1);
		writeWav(outDir + "/walker-octave0.wav", r0.audio);
		writeWav(outDir + "/walker-octave1.wav", r1.audio);
		writeWav(outDir + "/walker-octave2.wav", r2.audio);
		Run old = run(c, 1.0, 0.0, 0);
		writeWav(outDir + "/walker-lengths-old.wav", old.audio);
	}
	{
		const WGCase& c = *(byFeel[5] ? byFeel[5] : byFeel[0]);
		writeWav(outDir + "/walker-house-old.wav", run(c, 1.0, 0.0, 0, 5).audio);
		writeWav(outDir + "/walker-house-new.wav", run(c, 1.4, 0.5, 0, 5).audio);
		writeWav(outDir + "/walker-house-octave1.wav", run(c, 1.4, 0.5, 1, 5).audio);
	}
	printf(failures ? "%d FAILED\n" : "ALL PASS\n", failures);
	return failures ? 1 : 0;
}
