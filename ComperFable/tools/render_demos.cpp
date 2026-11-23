// Renders Comper demos through Joanna (this repository's piano), as they would sound in Rack:
// the Comper host's poly V/OCT, GATE, VEL and PEDAL drive Joanna's engine exactly as Joanna's
// module reads its jacks.
//   c++ -O2 -std=c++17 tools/render_demos.cpp -o /tmp/render_demos && /tmp/render_demos OUTDIR
#include "../src/dsp/Host.hpp"
#include "../test/golden.h"
#include "../../Joanna/src/JoannaEngine.hpp"
#include "../../Joanna/src/JoannaPresets.hpp"
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#if defined(__x86_64__) || defined(__i386__)
#include <xmmintrin.h>
#endif
using namespace comper;

static Params toParams(const double* v) {
	Params p; p.style = (int)v[0]; p.spread = v[1]; p.reg = v[2]; p.voices = (int)v[3]; p.lead = v[4]; p.out = v[5]; p.roots = (int)v[6];
	p.groove = (int)v[7]; p.density = v[8]; p.swing = v[9]; p.repeat = v[10]; p.push = v[11]; p.loose = v[12]; p.timing = v[13];
	p.roll = v[14]; p.weight = v[15]; p.slop = v[16]; p.pedal = v[17]; p.listen = v[18]; return p;
}
static void writeWav(const std::string& path, const std::vector<float>& l, const std::vector<float>& r, int sr) {
	FILE* f = fopen(path.c_str(), "wb"); if (!f) return;
	int n = (int)l.size(); int bytes = n * 4;
	auto w32 = [&](uint32_t v) { fwrite(&v, 4, 1, f); }; auto w16 = [&](uint16_t v) { fwrite(&v, 2, 1, f); };
	fwrite("RIFF", 1, 4, f); w32(36 + bytes); fwrite("WAVEfmt ", 1, 8, f); w32(16); w16(1); w16(2); w32(sr); w32(sr * 4); w16(4); w16(16);
	fwrite("data", 1, 4, f); w32(bytes);
	for (int i = 0; i < n; i++) {
		int16_t a = (int16_t)std::max(-32767.f, std::min(32767.f, l[i] * 32767.f));
		int16_t b = (int16_t)std::max(-32767.f, std::min(32767.f, r[i] * 32767.f));
		fwrite(&a, 2, 1, f); fwrite(&b, 2, 1, f);
	}
	fclose(f);
}

struct Job { const char* caseName; std::string file; double listen; bool solo; int joanna; };

int main(int argc, char** argv) {
#if defined(__x86_64__) || defined(__i386__)
	_mm_setcsr(_mm_getcsr() | 0x8040);
#endif
	std::string out = argc > 1 ? argv[1] : "/tmp";
	const int SR = 48000;
	std::vector<Job> jobs;
	for (int i = 0; i < NUM_GOLDEN; i++)
		if (!strncmp(GOLDEN[i].name, "preset ", 7)) jobs.push_back({GOLDEN[i].name, std::string(GOLDEN[i].name + 7), -1, GOLDEN[i].nsolo > 0, 4});
	jobs.push_back({"sweep loose=0.0", "demo-loose-0-machine", -1, false, 4});
	jobs.push_back({"sweep density=0.75", "demo-loose-1-player", -1, false, 4});
	jobs.push_back({"sweep loose=2.0", "demo-loose-2-too-loose", -1, false, 4});
	jobs.push_back({"sweep lead=0.0", "demo-lead-0-no-voice-leading", -1, false, 4});
	jobs.push_back({"sweep lead=0.5", "demo-lead-half", -1, false, 4});
	jobs.push_back({"sweep out=1.0", "demo-out-1", -1, false, 4});
	jobs.push_back({"listen 1.0", "demo-listen-0", 0.0, true, 4});
	jobs.push_back({"listen 1.0", "demo-listen-1", 1.0, true, 4});
	const char* only = argc > 2 ? argv[2] : nullptr;
	for (auto& j : jobs) {
		if (only && j.file.find(only) == std::string::npos) continue;
		const GCase* c = nullptr;
		for (int i = 0; i < NUM_GOLDEN; i++) if (!strcmp(GOLDEN[i].name, j.caseName)) c = &GOLDEN[i];
		if (!c) { printf("no case %s\n", j.caseName); continue; }
		HostIn in; HostOut o; Host h;
		int n = c->nchart; double bps = c->total / n;
		in.steps = n; in.beatsPerStep = (int)bps;
		for (int k = 0; k < n; k++) { in.stepRoot[k] = c->chart[k].root; in.stepQual[k] = c->chart[k].q; }
		in.p = toParams(c->prm); if (j.listen >= 0) in.p.listen = j.listen;
		in.seed = c->seed; in.tempo = c->bpm;
		in.soloConnected = j.solo; in.soloPitchConnected = j.solo;
		joanna::Engine* e = new joanna::Engine(); e->setSampleRate((float)SR);
		joanna::Controls ctl; joanna::presetToControls(joanna::PRESETS[j.joanna], ctl);
		for (int i = 0; i < 64; i++) e->process(ctl, 0.f);
		double spb = 60.0 / c->bpm, pre = 0.5 * spb, dt = 1.0 / SR;
		int N = (int)((c->nbars * 4 * spb + pre + 3.0) * SR);
		int stopAt = (int)((c->nbars * 4 * spb + pre) * SR);
		std::vector<float> L(N), R(N);
		bool gateHigh[16] = {}; int keyOf[16]; for (int k = 0; k < 16; k++) keyOf[k] = -1;
		for (int s = 0; s < N; s++) {
			double gt = s * dt - pre;
			if (j.solo) {
				bool on = false; double pv = 0;
				for (int k = 0; k < c->nsolo; k++) if (c->solo[k].on <= gt && gt < c->solo[k].off) { on = true; pv = (c->solo[k].pitch - 60) / 12.0; }
				in.solo = on ? 10.f : 0.f; in.soloPitch = (float)pv;
			}
			in.runButton = s < stopAt;
			h.process(in, o, dt, -0.5);
			for (int k = 0; k < 16; k++) {
				bool high = k < o.channels && o.gate[k] > 1.f;
				if (high) {
					int key = std::max(0, std::min(joanna::NKEYS - 1, (int)std::round(o.voct[k] * 12.f) + joanna::KEY_C4));
					if (!gateHigh[k] || key != keyOf[k]) {
						if (gateHigh[k] && keyOf[k] >= 0) e->noteOff(keyOf[k]);
						e->noteOn(key, std::max(0.f, std::min(1.f, o.vel[k] * 0.1f)), 0.f);
						keyOf[k] = key;
					}
				} else if (gateHigh[k]) { if (keyOf[k] >= 0) e->noteOff(keyOf[k]); keyOf[k] = -1; }
				gateHigh[k] = high;
			}
			ctl.pedal = o.pedal * 0.1f;
			e->process(ctl, 0.f);
			L[s] = e->outL * 0.1f; R[s] = e->outR * 0.1f;
		}
		float pk = 1e-9f; for (int s = 0; s < N; s++) pk = std::max(pk, std::max(std::fabs(L[s]), std::fabs(R[s])));
		float g = 0.7f / pk;
		for (int s = 0; s < N; s++) { L[s] *= g; R[s] *= g; }
		writeWav(out + "/" + j.file + ".wav", L, R, SR);
		printf("%s  %.1f s  peak was %.2f V\n", j.file.c_str(), N / (double)SR, pk * 10);
		fflush(stdout);
		delete e;
	}
}
