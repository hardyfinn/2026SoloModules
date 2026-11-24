// Renders Comper + Walker demos: Comper's host drives Joanna (this repository's piano) and sends
// its link to Walker's host every sample, exactly as the two modules do side by side in Rack
// (Comper's ROOTS off, as when a Walker is attached). Writes NAME.piano.wav (stereo) and
// NAME.bass.wav (mono) for tools/make_walker_demos.py to mix with drums.
//   c++ -O3 -funsafe-math-optimizations -std=c++17 tools/render_trio.cpp -o /tmp/render_trio && /tmp/render_trio OUTDIR
#include "../src/dsp/Host.hpp"
#include "../src/dsp/WalkerHost.hpp"
#include "../test/golden.h"
#include "../test/walker_golden.h"
#include "../test/walker_presets.h"
#include "../../Joanna/src/JoannaEngine.hpp"
#include "../../Joanna/src/JoannaPresets.hpp"
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#if defined(__x86_64__) || defined(__i386__)
#include <xmmintrin.h>
#endif

static comper::Params toParams(const double* v) {
	comper::Params p; p.style = (int)v[0]; p.spread = v[1]; p.reg = v[2]; p.voices = (int)v[3]; p.lead = v[4]; p.out = v[5]; p.roots = (int)v[6];
	p.groove = (int)v[7]; p.density = v[8]; p.swing = v[9]; p.repeat = v[10]; p.push = v[11]; p.loose = v[12]; p.timing = v[13];
	p.roll = v[14]; p.weight = v[15]; p.slop = v[16]; p.pedal = v[17]; p.listen = v[18]; return p;
}
static void writeWav(const std::string& path, const std::vector<float>& l, const std::vector<float>* r, int sr) {
	FILE* f = fopen(path.c_str(), "wb"); if (!f) return;
	int ch = r ? 2 : 1; int n = (int)l.size(); int bytes = n * 4 * ch;
	auto w32 = [&](uint32_t v) { fwrite(&v, 4, 1, f); }; auto w16 = [&](uint16_t v) { fwrite(&v, 2, 1, f); };
	fwrite("RIFF", 1, 4, f); w32(36 + bytes); fwrite("WAVEfmt ", 1, 8, f); w32(16); w16(3); w16(ch); w32(sr); w32(sr * 4 * ch); w16(4 * ch); w16(32);
	fwrite("data", 1, 4, f); w32(bytes);
	for (int i = 0; i < n; i++) { fwrite(&l[i], 4, 1, f); if (r) fwrite(&(*r)[i], 4, 1, f); }
	fclose(f);
}

struct Job {
	std::string file; const char* comperCase; int walkerPreset;
	std::vector<std::pair<int, double>> tweak;   // Walker parameter index, value
	bool piano = true;
	const char* chartFrom = nullptr;             // a Walker golden case for the chart (bass alone)
};

int main(int argc, char** argv) {
#if defined(__x86_64__) || defined(__i386__)
	_mm_setcsr(_mm_getcsr() | 0x8040);
#endif
	std::string out = argc > 1 ? argv[1] : "/tmp";
	const char* only = argc > 2 ? argv[2] : nullptr;
	const int SR = 48000;
	enum { ACT = 1, LOOSE = 8, ATTACK = 14, BRIGHT = 16, SUSTAIN = 17, GROWL = 18, BODY = 19, PICKUP = 20, THUMP = 21, AGE = 22 };
	std::vector<Job> jobs;
	for (int i = 0; i < NUM_WPRESETS; i++) jobs.push_back({std::string("trio-") + WPRESETS[i].name, nullptr, i});
	jobs.push_back({"bass-alone-blues", nullptr, 0, {}, false, "feel WALK / Jazz blues in F / seed 1"});
	jobs.push_back({"activity-0.2", nullptr, 0, {{ACT, 0.2}}});
	jobs.push_back({"activity-0.5", nullptr, 0, {{ACT, 0.5}}});
	jobs.push_back({"activity-0.95", nullptr, 0, {{ACT, 0.95}}});
	jobs.push_back({"loose-0-machine", nullptr, 0, {{LOOSE, 0.0}}});
	jobs.push_back({"loose-1-player", nullptr, 0, {{LOOSE, 1.0}}});
	jobs.push_back({"loose-2-very-loose", nullptr, 0, {{LOOSE, 2.0}}});
	jobs.push_back({"attack-0", nullptr, 0, {{ATTACK, 0.0}}});
	jobs.push_back({"attack-1-as-measured", nullptr, 0, {{ATTACK, 1.0}}});
	jobs.push_back({"attack-2.2", nullptr, 0, {{ATTACK, 2.2}}});
	jobs.push_back({"tone-gut-and-soft", nullptr, 0, {{BRIGHT, 0.1}, {SUSTAIN, 0.3}, {PICKUP, 0.0}, {THUMP, 0.65}}});
	jobs.push_back({"tone-steel-and-ringing", nullptr, 0, {{BRIGHT, 0.85}, {SUSTAIN, 0.9}, {PICKUP, 0.3}}});
	jobs.push_back({"tone-growl-full", nullptr, 0, {{GROWL, 1.0}}});
	jobs.push_back({"tone-pickup-only", nullptr, 0, {{PICKUP, 1.0}}});
	jobs.push_back({"tone-age-full", nullptr, 0, {{AGE, 1.0}}});
	for (auto& j : jobs) {
		if (only && j.file.find(only) == std::string::npos) continue;
		const WPreset& P = WPRESETS[j.walkerPreset];
		std::string cname = std::string("preset ") + P.name;
		const GCase* c = nullptr;
		for (int i = 0; i < NUM_GOLDEN; i++) if (cname == GOLDEN[i].name) c = &GOLDEN[i];
		if (!c) { printf("no Comper case %s\n", cname.c_str()); continue; }
		// chart, tempo, length, seed
		int nchart = c->nchart; double total = c->total, bpm = c->bpm; int nbars = c->nbars, seed = c->seed;
		std::vector<int> roots, quals;
		for (int k = 0; k < nchart; k++) { roots.push_back(c->chart[k].root); quals.push_back(c->chart[k].q); }
		if (j.chartFrom) {
			const WGCase* w = nullptr;
			for (int i = 0; i < NUM_WGOLDEN; i++) if (!strcmp(WGOLDEN[i].name, j.chartFrom)) w = &WGOLDEN[i];
			if (!w) { printf("no Walker case %s\n", j.chartFrom); continue; }
			nchart = w->nchart; total = w->total; bpm = w->bpm; nbars = w->nbars; seed = w->seed; roots.clear(); quals.clear();
			for (int k = 0; k < nchart; k++) { roots.push_back(w->chart[k].root); quals.push_back(w->chart[k].q); }
		}
		comper::HostIn in; comper::HostOut o; comper::Host h;
		in.steps = nchart; in.beatsPerStep = (int)(total / nchart);
		for (int k = 0; k < nchart; k++) { in.stepRoot[k] = roots[k]; in.stepQual[k] = quals[k]; }
		in.p = toParams(c->prm); in.p.roots = 0;
		in.seed = seed; in.tempo = bpm;
		// Walker
		double v[24]; for (int k = 0; k < 24; k++) v[k] = P.v[k];
		for (auto& t : j.tweak) v[t.first] = t.second;
		walker::WalkerHost wh; wh.setSampleRate(SR); walker::WIn win; walker::WOut wo;
		win.feelPos = (int)v[0]; win.bp.activity = v[1]; win.bp.reg = v[2]; win.bp.roam = v[3]; win.bp.out = v[4]; win.bp.repeat = v[5]; win.seed = 11;
		win.bp.loose = v[8]; win.bp.ahead = v[9]; win.bp.timing = v[10]; win.bp.weight = v[11]; win.bp.slop = v[12]; win.bp.pitch = v[13];
		win.tone.attack = v[14]; win.tone.pluck = v[15]; win.tone.bright = v[16]; win.tone.sustain = v[17]; win.tone.growl = v[18]; win.tone.body = v[19];
		win.tone.pickup = v[20]; win.tone.thump = v[21]; win.tone.age = v[22]; win.level = v[23];
		joanna::Engine* e = new joanna::Engine(); e->setSampleRate((float)SR);
		joanna::Controls ctl; joanna::presetToControls(joanna::PRESETS[4], ctl);
		for (int i = 0; i < 64; i++) e->process(ctl, 0.f);
		double spb = 60.0 / bpm, pre = 0.5 * spb, dt = 1.0 / SR;
		int N = (int)((nbars * 4 * spb + pre + 3.0) * SR);
		int stopAt = (int)((nbars * 4 * spb + pre) * SR);
		std::vector<float> L(N), R(N), B(N);
		bool gateHigh[16] = {}; int keyOf[16]; for (int k = 0; k < 16; k++) keyOf[k] = -1;
		uint32_t stamp = 0;
		for (int s = 0; s < N; s++) {
			in.runButton = s < stopAt;
			h.process(in, o, dt, -0.5);
			// the link, as Comper's sendLink() writes it
			CompLink& m = win.link; win.linkOk = true;
			m.resetCount = h.resetCount; m.stamp = ++stamp; m.running = h.running && !h.waitingFirstPulse;
			m.phi = h.phi; m.spb = h.spb; m.steps = in.steps; m.beatsPerStep = in.beatsPerStep;
			for (int k = 0; k < 16; k++) { m.root[k] = comper::pmod(in.stepRoot[k] + h.keyOffset(in), 12); m.qual[k] = in.stepQual[k]; }
			m.external = h.source != comper::SRC_CHART; m.curRoot = h.srcChord.root; m.curQual = h.srcChord.q;
			m.groove = in.p.groove; m.swing = in.p.swing;
			wh.process(win, wo, dt);
			B[s] = wo.audio * 0.1f;
			if (j.piano) {
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
		}
		if (j.piano) writeWav(out + "/" + j.file + ".piano.wav", L, &R, SR);
		writeWav(out + "/" + j.file + ".bass.wav", B, nullptr, SR);
		printf("%s  %.1f s  groove %d  bpm %.0f  voices stolen %d\n", j.file.c_str(), N / (double)SR, in.p.groove, bpm, (int)wh.bass.steals);
		fflush(stdout);
		delete e;
	}
}
