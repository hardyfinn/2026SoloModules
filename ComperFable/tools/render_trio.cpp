// Renders Comper + Walker + Rider demos: Comper's host drives Joanna (this repository's piano) and the
// three hosts pass the link every sample, as the modules do side by side in Rack (Comper's ROOTS off,
// as when a Walker is in the row; Rider hears Comper's figure and Walker's off-beat notes; BAND reaches
// Comper and Walker). Writes NAME.piano.wav (stereo), NAME.bass.wav (mono) and NAME.drums.wav (stereo)
// for tools/make_walker_demos.py and tools/make_rider_demos_cpp.py to mix.
//   c++ -O3 -funsafe-math-optimizations -std=c++17 tools/render_trio.cpp -o /tmp/render_trio && /tmp/render_trio OUTDIR
#include "../src/dsp/Host.hpp"
#include "../src/dsp/WalkerHost.hpp"
#include "../src/dsp/RiderHost.hpp"
#include "../test/rider_presets.h"
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
	std::vector<std::pair<int, double>> rtweak = {};   // Rider parameter index, value
	double e0 = -1, e1 = -1;                     // an ENERGY ramp over the take (Rider), when e0 >= 0
	int nbars = 0;                               // 0: the Comper case's length
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
	// Rider's tours: Medium Swing unless named
	enum { R_ENERGY = 1, R_LISTEN = 3, R_FILLS = 4, R_BAND = 7, R_TUNE = 17, R_DECAY = 18, R_BRIGHT = 19, R_WIDTH = 20, R_AGE = 21 };
	auto rj = [&](const char* f, int wp, std::vector<std::pair<int, double>> rt, double e0 = -1, double e1 = -1, int nb = 0) {
		Job j{f, nullptr, wp}; j.rtweak = rt; j.e0 = e0; j.e1 = e1; j.nbars = nb; jobs.push_back(j); };
	rj("rider-energy-0.1", 0, {{R_ENERGY, 0.1}});
	rj("rider-energy-0.9", 0, {{R_ENERGY, 0.9}});
	rj("rider-listen-0", 0, {{R_LISTEN, 0.0}});
	rj("rider-building-blues", 6, {{R_FILLS, 0.7}}, 0.15, 0.95, 24);
	rj("rider-band-0-energy-ramp", 0, {{R_BAND, 0.0}}, 0.05, 0.95, 24);
	rj("rider-band-1-energy-ramp", 0, {{R_BAND, 1.0}}, 0.05, 0.95, 24);
	rj("rider-kit-default", 0, {}, -1, -1, 8);
	rj("rider-kit-tune-0", 0, {{R_TUNE, 0.0}}, -1, -1, 8);
	rj("rider-kit-tune-1", 0, {{R_TUNE, 1.0}}, -1, -1, 8);
	rj("rider-kit-decay-0", 0, {{R_DECAY, 0.0}}, -1, -1, 8);
	rj("rider-kit-decay-1", 0, {{R_DECAY, 1.0}}, -1, -1, 8);
	rj("rider-kit-bright-0", 0, {{R_BRIGHT, 0.0}}, -1, -1, 8);
	rj("rider-kit-bright-1", 0, {{R_BRIGHT, 1.0}}, -1, -1, 8);
	rj("rider-kit-width-0", 0, {{R_WIDTH, 0.0}}, -1, -1, 8);
	rj("rider-kit-age-1", 0, {{R_AGE, 1.0}}, -1, -1, 8);
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
		if (j.nbars > 0) nbars = j.nbars;
		const double baseDensity = in.p.density;
		// Rider: the preset paired with Walker's by number and name
		const RPreset& RPr = RPRESETS[std::min(j.walkerPreset, NUM_RPRESETS - 1)];
		double rv[23]; for (int k = 0; k < 23; k++) rv[k] = RPr.v[k];
		for (auto& t : j.rtweak) rv[t.first] = t.second;
		rider::RiderHost rh; rh.setSampleRate(SR); rider::RIn rin; rider::ROut ro;
		rin.feelPos = (int)rv[0]; rin.rp.energy = rv[1]; rin.rp.comp = rv[2]; rin.rp.listen = rv[3]; rin.rp.fills = rv[4]; rin.rp.feather = rv[5]; rin.rp.repeat = rv[6];
		rin.seed = 11; rin.rp.loose = rv[10]; rin.rp.ahead = rv[11];
		for (int k = 0; k < 5; k++) rin.kit.mix[k] = rv[12 + k];
		rin.kit.tune = rv[17]; rin.kit.decay = rv[18]; rin.kit.bright = rv[19]; rin.kit.width = rv[20]; rin.kit.age = rv[21]; rin.kit.level = rv[22];
		const double band = rv[7];
		float bandX = 0.f;
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
		std::vector<float> L(N), R(N), B(N), DL(N), DR(N);
		const double walkerActivity = win.bp.activity;
		bool gateHigh[16] = {}; int keyOf[16]; for (int k = 0; k < 16; k++) keyOf[k] = -1;
		uint32_t stamp = 0;
		for (int s = 0; s < N; s++) {
			in.runButton = s < stopAt;
			in.p.density = std::max(0.0, std::min(1.0, baseDensity + 0.35 * bandX));
			win.bp.activity = std::max(0.0, std::min(1.0, walkerActivity + 0.35 * bandX));
			h.process(in, o, dt, -0.5);
			// the link, as Comper's sendLink() writes it
			CompLink& m = win.link; win.linkOk = true;
			m.resetCount = h.resetCount; m.stamp = ++stamp; m.running = h.running && !h.waitingFirstPulse;
			m.phi = h.phi; m.spb = h.spb; m.steps = in.steps; m.beatsPerStep = in.beatsPerStep;
			for (int k = 0; k < 16; k++) { m.root[k] = comper::pmod(in.stepRoot[k] + h.keyOffset(in), 12); m.qual[k] = in.stepQual[k]; }
			m.external = h.source != comper::SRC_CHART; m.curRoot = h.srcChord.root; m.curQual = h.srcChord.q;
			m.groove = in.p.groove; m.swing = in.p.swing;
			m.hasComp = true; m.planBar = h.planBar; m.planSlots = h.eng.slots;
			for (int k = 0; k < 16; k++) { m.planKind[k] = h.eng.cellmap[k]; m.planBeat[k] = h.eng.slotBeats[k]; m.planPush[k] = h.eng.pushOk[k]; }
			wh.process(win, wo, dt);
			B[s] = wo.audio * 0.1f * std::pow(10.f, 3.f * bandX / 20.f);
			// Rider hears Comper's section and Walker's off-beat notes
			rin.link = m; rin.link.hasBass = true; rin.link.bassN = wh.offBeats(rin.link.bassBeat, 16);
			if (j.e0 >= 0) rin.rp.energy = j.e0 + (j.e1 - j.e0) * std::max(0.0, std::min(1.0, h.phi / (nbars * 4.0)));
			rh.process(rin, ro, dt);
			DL[s] = ro.L * 0.1f; DR[s] = ro.R * 0.1f;
			bandX = (float)std::max(-1.0, std::min(1.0, band * 2.0 * (rin.rp.energy - 0.5)));
			if (j.piano) {
				for (int k = 0; k < 16; k++) {
					bool high = k < o.channels && o.gate[k] > 1.f;
					if (high) {
						int key = std::max(0, std::min(joanna::NKEYS - 1, (int)std::round(o.voct[k] * 12.f) + joanna::KEY_C4));
						if (!gateHigh[k] || key != keyOf[k]) {
							if (gateHigh[k] && keyOf[k] >= 0) e->noteOff(keyOf[k]);
							e->noteOn(key, std::max(0.f, std::min(1.f, o.vel[k] * 0.1f * (1.f + 0.25f * bandX))), 0.f);
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
		writeWav(out + "/" + j.file + ".drums.wav", DL, &DR, SR);
		printf("%s  %.1f s  groove %d  bpm %.0f  voices stolen %d\n", j.file.c_str(), N / (double)SR, in.p.groove, bpm, (int)wh.bass.steals);
		fflush(stdout);
		delete e;
	}
}
