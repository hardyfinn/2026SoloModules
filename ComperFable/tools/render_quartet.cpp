// Renders quartet demos: Comper (on Joanna), Walker, Rider and one or two Soloists, passing the link every sample
// as the modules do side by side in Rack, with the band's 2.5.0 Soloist logic copied from Comper.cpp and Walker.cpp.
// Plays the whole tune (head, solos, trading, head out, end) and writes one stereo WAV per demo.
//   c++ -O3 -funsafe-math-optimizations -std=c++17 tools/render_quartet.cpp -o /tmp/render_quartet && /tmp/render_quartet OUTDIR [name]
#include "../src/dsp/Host.hpp"
#include "../src/dsp/WalkerHost.hpp"
#include "../src/dsp/RiderHost.hpp"
#include "../src/dsp/SoloHost.hpp"
#include "../src/Progressions.hpp"
#include "../test/rider_presets.h"
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

static void writeWav16(const std::string& path, const std::vector<float>& l, const std::vector<float>& r, int sr) {
	FILE* f = fopen(path.c_str(), "wb"); if (!f) return;
	int n = (int)l.size(); int bytes = n * 4;
	auto w32 = [&](uint32_t v) { fwrite(&v, 4, 1, f); }; auto w16 = [&](uint16_t v) { fwrite(&v, 2, 1, f); };
	fwrite("RIFF", 1, 4, f); w32(36 + bytes); fwrite("WAVEfmt ", 1, 8, f); w32(16); w16(1); w16(2); w32(sr); w32(sr * 4); w16(4); w16(16);
	fwrite("data", 1, 4, f); w32(bytes);
	for (int i = 0; i < n; i++) for (float x : {l[i], r[i]}) { int v = (int)std::lround(std::max(-1.f, std::min(1.f, x)) * 32767.f); w16((uint16_t)(int16_t)v); }
	fclose(f);
}

struct Job {
	std::string file; const char* prog; int groove; double bpm;
	int horn = horn::TENOR; int horn2 = -1; int section = 0;
	int trade = 2, with = 2, choruses = 2;
	double outside = 0.1, energy = 0.6, density = 0.5, space = 0.4;
	int seed = 3; int walkerPreset = 0, riderPreset = 0;
	int form = solo::F_TUNE; double maxBeats = 0;
};

int main(int argc, char** argv) {
#if defined(__x86_64__) || defined(__i386__)
	_mm_setcsr(_mm_getcsr() | 0x8040);
#endif
	std::string outDir = argc > 1 ? argv[1] : "/tmp";
	const char* only = argc > 2 ? argv[2] : nullptr;
	const int SR = 48000;
	using namespace solo;
	std::vector<Job> jobs;
	auto J = [&](Job j) { jobs.push_back(j); };
	{ Job j; j.file = "01-tenor-jazz-blues-fours-with-drums"; j.prog = "Jazz blues in F"; j.groove = G_SWING; j.bpm = 168; j.with = 0; J(j); }
	{ Job j; j.file = "02-trumpet-major-minor-fours-drums-and-piano"; j.prog = "Major then minor"; j.groove = G_SWING; j.bpm = 184; j.horn = horn::TRUMPET; j.seed = 5; J(j); }
	{ Job j; j.file = "03-trombone-bossa"; j.prog = "Bossa"; j.groove = G_BOSSA; j.bpm = 132; j.horn = horn::TROMBONE; j.trade = 0; j.energy = 0.45; j.walkerPreset = 4; j.seed = 8; J(j); }
	{ Job j; j.file = "04-tenor-ballad"; j.prog = "Ballad"; j.groove = G_BALLAD; j.bpm = 66; j.trade = 0; j.choruses = 1; j.energy = 0.4; j.space = 0.5; j.walkerPreset = 2; j.riderPreset = 2; j.seed = 2; J(j); }
	{ Job j; j.file = "05-trumpet-house-eights-with-piano"; j.prog = "House: piano lift"; j.groove = G_HOUSE; j.bpm = 122; j.horn = horn::TRUMPET; j.trade = 3; j.with = 1; j.seed = 4; J(j); }
	{ Job j; j.file = "06-tenor-soul-vamp"; j.prog = "Soul vamp"; j.groove = G_SOUL; j.bpm = 96; j.trade = 1; j.with = 0; j.seed = 6; J(j); }
	{ Job j; j.file = "07-tenor-outside-full-free"; j.prog = "Modal two-chord"; j.groove = G_SWING; j.bpm = 176; j.outside = 1.0; j.energy = 0.85; j.density = 0.65; j.trade = 0; j.seed = 7; J(j); }
	{ Job j; j.file = "08-two-horns-tenor-trumpet-section"; j.prog = "Falling fifths"; j.groove = G_SWING; j.bpm = 172; j.horn2 = horn::TRUMPET; j.section = 0; j.trade = 2; j.with = 0; j.seed = 9; J(j); }
	{ Job j; j.file = "09-two-horns-tenor-trombone-turns"; j.prog = "Blues: shuffle in G"; j.groove = G_SWING; j.bpm = 150; j.horn2 = horn::TROMBONE; j.section = 1; j.trade = 2; j.with = 0; j.choruses = 2; j.seed = 10; J(j); }
	{ Job j; j.file = "10-trombone-reggae-lovers-rock"; j.prog = "Reggae: lovers rock"; j.groove = G_REGGAE; j.bpm = 78; j.horn = horn::TROMBONE; j.trade = 0; j.energy = 0.45; j.seed = 12; J(j); }

	for (auto& j : jobs) {
		if (only && j.file.find(only) == std::string::npos) continue;
		int pi = -1; for (int i = 0; i < comper::NUM_PROGRESSIONS; i++) if (!strcmp(comper::PROGRESSIONS[i].name, j.prog)) pi = i;
		if (pi < 0) { printf("no progression %s\n", j.prog); continue; }
		std::vector<comper::Chord> ch; std::string bad; comper::parseChart(comper::PROGRESSIONS[pi].text, ch, bad);
		const int genre = genreOf(comper::PROGRESSIONS[pi].name);
		// Comper
		comper::HostIn in; comper::HostOut o; comper::Host h;
		in.steps = std::min(16, (int)ch.size()); in.beatsPerStep = comper::PROGRESSIONS[pi].beats;
		for (int k = 0; k < in.steps; k++) { in.stepRoot[k] = ch[k].root; in.stepQual[k] = ch[k].q; }
		in.p.groove = j.groove; in.p.roots = 0; in.p.human = true; in.seed = j.seed; in.tempo = j.bpm;
		if (j.groove == G_BALLAD) in.p.density = 0.4;
		const comper::Params base = in.p;
		// Walker, Rider: presets' tone and kit, FEEL follow
		const WPreset& WP = WPRESETS[j.walkerPreset]; double v[24]; for (int k = 0; k < 24; k++) v[k] = WP.v[k];
		walker::WalkerHost wh; wh.setSampleRate(SR); walker::WIn win; walker::WOut wo;
		win.feelPos = 0; win.bp.activity = v[1]; win.bp.reg = v[2]; win.bp.roam = v[3]; win.bp.out = v[4]; win.bp.repeat = v[5]; win.seed = j.seed;
		win.bp.loose = v[8]; win.bp.ahead = v[9]; win.bp.timing = v[10]; win.bp.weight = v[11]; win.bp.slop = v[12]; win.bp.pitch = v[13];
		win.tone.attack = v[14]; win.tone.pluck = v[15]; win.tone.bright = v[16]; win.tone.sustain = v[17]; win.tone.growl = v[18]; win.tone.body = v[19];
		win.tone.pickup = v[20]; win.tone.thump = v[21]; win.tone.age = v[22]; win.level = v[23];
		const walker::BP wbase = win.bp;
		const RPreset& RP = RPRESETS[j.riderPreset]; double rv[23]; for (int k = 0; k < 23; k++) rv[k] = RP.v[k];
		rider::RiderHost rh; rh.setSampleRate(SR); rider::RIn rin; rider::ROut ro;
		rin.feelPos = 0; rin.rp.energy = rv[1]; rin.rp.comp = rv[2]; rin.rp.listen = rv[3]; rin.rp.fills = rv[4]; rin.rp.feather = rv[5]; rin.rp.repeat = rv[6];
		rin.seed = j.seed; rin.rp.loose = rv[10]; rin.rp.ahead = rv[11];
		for (int k = 0; k < 5; k++) rin.kit.mix[k] = rv[12 + k];
		rin.kit.tune = rv[17]; rin.kit.decay = rv[18]; rin.kit.bright = rv[19]; rin.kit.width = rv[20]; rin.kit.age = rv[21]; rin.kit.level = rv[22];
		// Soloists
		int nSol = j.horn2 >= 0 ? 2 : 1;
		SoloHost sh[2]; SIn sin[2]; SOut so[2]; CompLink solMsg[2];
		for (int k = 0; k < nSol; k++) {
			sh[k].setSampleRate(SR);
			SIn& s = sin[k]; s.kind = k == 0 ? j.horn : j.horn2; s.section = j.section; s.trade = j.trade; s.tradeWith = j.with; s.choruses = j.choruses;
			s.outside = j.outside; s.energy = j.energy; s.density = j.density; s.space = j.space; s.seed = j.seed + 100 * k; s.form = j.form;
			s.reg = 0.5; s.room = 0.3;
		}
		// piano
		joanna::Engine* e = new joanna::Engine(); e->setSampleRate((float)SR);
		joanna::Controls ctl; joanna::presetToControls(joanna::PRESETS[4], ctl);
		for (int i = 0; i < 64; i++) e->process(ctl, 0.f);
		const double spb = 60.0 / j.bpm, dt = 1.0 / SR;
		std::vector<float> PL, PR, B, DL, DR, HL, HR;
		bool gateHigh[16] = {}; int keyOf[16]; for (int k = 0; k < 16; k++) keyOf[k] = -1;
		const long long maxN = (long long)(15 * 60 * SR);
		double stopBeat = j.maxBeats > 0 ? j.maxBeats : 1e9; long long stopAt = -1;
		for (long long s = 0; s < maxN; s++) {
			double endB = sh[0].plan.endBeat;
			if (j.form == F_TUNE && endB > 0) stopBeat = endB + 8.0;
			if (stopAt < 0 && h.phi >= stopBeat) stopAt = s;
			in.runButton = stopAt < 0;
			if (stopAt >= 0 && s > stopAt + 3 * SR) break;
			// the band hears the Soloists (last sample's messages, as in Rack)
			CompLink heard; for (int k = 0; k < nSol; k++) linkCopySolo(heard, solMsg[k], k);
			// Comper, as Comper.cpp
			in.p = base; in.layout = false; in.soloConnected = false; in.soloPitchConnected = false;
			bool pianoTurn = false;
			if (heard.hasSolo()) {
				int k = (heard.sol[0].on && (heard.sol[0].sounding || !heard.sol[1].sounding)) ? 0 : 1;
				in.soloConnected = true; in.solo = soloSounding(heard) ? 10.f : 0.f;
				in.soloPitchConnected = true; in.soloPitch = (heard.sol[k].pitch - 60) / 12.f;
				in.p.listen = std::max(in.p.listen, 0.6);
				in.p.out = std::min(1.0, in.p.out + 0.5 * soloOutside(heard));
				int turn = turnAt(heard, h.phi + 0.15);
				in.layout = turn == TURN_DRUMS || turn == TURN_END;
				if (turn == TURN_PIANO) { in.p.density = std::min(1.0, in.p.density + 0.4); in.p.reg = std::min(79.0, in.p.reg + 5.0); in.p.lead = 1.0; in.p.listen = 0.0; pianoTurn = true; }
			}
			h.process(in, o, dt, -0.5);
			CompLink m;
			m.resetCount = h.resetCount; m.running = h.running && !h.waitingFirstPulse;
			m.phi = h.phi; m.spb = h.spb; m.steps = in.steps; m.beatsPerStep = in.beatsPerStep;
			for (int k = 0; k < 16; k++) { m.root[k] = comper::pmod(in.stepRoot[k] + h.keyOffset(in), 12); m.qual[k] = in.stepQual[k]; }
			m.external = h.source != comper::SRC_CHART; m.curRoot = h.srcChord.root; m.curQual = h.srcChord.q;
			m.groove = in.p.groove; m.swing = in.p.swing; m.genre = genre;
			m.hasComp = true; m.planBar = h.planBar; m.planSlots = h.eng.slots;
			for (int k = 0; k < 16; k++) { m.planKind[k] = h.eng.cellmap[k]; m.planBeat[k] = h.eng.slotBeats[k]; m.planPush[k] = h.eng.pushOk[k]; }
			// Walker, as Walker.cpp
			win.link = m; for (int k = 0; k < nSol; k++) linkCopySolo(win.link, solMsg[k], k); win.linkOk = true;
			win.bp = wbase; win.layout = 0.f;
			if (heard.hasSolo()) {
				int turn = turnAt(heard, wh.phi + 0.15);
				if (turn == TURN_DRUMS || turn == TURN_END) win.layout = 10.f;
				win.bp.activity = std::max(0.0, std::min(1.0, win.bp.activity - 0.5 * std::max(0.f, soloEnergy(heard) - 0.7f)));
				win.bp.out = std::min(1.0, win.bp.out + 0.4 * soloOutside(heard));
			}
			wh.process(win, wo, dt);
			// Rider
			rin.link = win.link; rin.link.hasBass = true; rin.link.bassN = wh.offBeats(rin.link.bassBeat, 16);
			rh.process(rin, ro, dt);
			// Soloists: the leader hears the band (and the second); the second hears the leader
			float hl = 0, hr = 0;
			CompLink nextMsg[2];
			for (int k = 0; k < nSol; k++) {
				SIn& si = sin[k];
				si.link = m; si.link.hasBass = true; si.link.hasDrum = true;
				if (nSol > 1) linkCopySolo(si.link, solMsg[1 - k], 1 - k);
				si.linkOk = true; si.follower = k == 1;
				sh[k].process(si, so[k], dt);
				nextMsg[k] = CompLink(); sh[k].fillLink(si, nextMsg[k]);
				float pan = nSol > 1 ? (k == 0 ? -0.35f : 0.35f) : 0.f;
				hl += so[k].L * (1 - pan); hr += so[k].R * (1 + pan);
			}
			for (int k = 0; k < nSol; k++) solMsg[k] = nextMsg[k];
			// piano
			for (int k = 0; k < 16; k++) {
				bool high = k < o.channels && o.gate[k] > 1.f;
				if (high) {
					int key = std::max(0, std::min(joanna::NKEYS - 1, (int)std::round(o.voct[k] * 12.f) + joanna::KEY_C4));
					if (!gateHigh[k] || key != keyOf[k]) {
						if (gateHigh[k] && keyOf[k] >= 0) e->noteOff(keyOf[k]);
						e->noteOn(key, std::max(0.f, std::min(1.f, o.vel[k] * 0.1f * (pianoTurn ? 1.25f : 1.f))), 0.f);
						keyOf[k] = key;
					}
				} else if (gateHigh[k]) { if (keyOf[k] >= 0) e->noteOff(keyOf[k]); keyOf[k] = -1; }
				gateHigh[k] = high;
			}
			ctl.pedal = o.pedal * 0.1f;
			e->process(ctl, 0.f);
			PL.push_back(e->outL * 0.1f); PR.push_back(e->outR * 0.1f);
			B.push_back(wo.audio * 0.1f); DL.push_back(ro.L * 0.1f); DR.push_back(ro.R * 0.1f);
			HL.push_back(hl * 0.1f); HR.push_back(hr * 0.1f);
		}
		// the mix: levels by RMS so every demo sits the same way, then a peak trim
		auto rms = [](const std::vector<float>& a, const std::vector<float>* b) { double s = 0; size_t n = a.size(); for (size_t i = 0; i < n; i++) { s += a[i] * a[i]; if (b) s += (*b)[i] * (*b)[i]; } return std::sqrt(s / std::max<size_t>(1, n * (b ? 2 : 1))) + 1e-9; };
		double gp = 0.10 / rms(PL, &PR), gb = 0.10 / rms(B, nullptr), gd = 0.09 / rms(DL, &DR), gh = 0.14 / rms(HL, &HR);
		size_t N = PL.size(); std::vector<float> L(N), R(N); double peak = 1e-9;
		for (size_t i = 0; i < N; i++) {
			L[i] = (float)(gp * PL[i] + gb * B[i] * 0.95 + gd * DL[i] + gh * HL[i]);
			R[i] = (float)(gp * PR[i] + gb * B[i] * 0.95 + gd * DR[i] + gh * HR[i]);
			peak = std::max(peak, (double)std::max(std::fabs(L[i]), std::fabs(R[i])));
		}
		double trim = std::min(1.0, 0.89 / peak);
		for (size_t i = 0; i < N; i++) { L[i] *= (float)trim; R[i] *= (float)trim; }
		if (argc > 3) {   // per bar: piano, bass, drums, horn levels (dB)
			size_t bar = (size_t)(4 * spb * SR);
			for (size_t b0 = 0, bi = 0; b0 + bar <= N; b0 += bar, bi++) {
				auto lv = [&](const std::vector<float>& a, double g) { double s = 0; for (size_t i = b0; i < b0 + bar; i++) s += a[i] * a[i] * g * g; return 10 * std::log10(s / bar + 1e-12); };
				printf("bar %3d  P %6.1f  B %6.1f  D %6.1f  H %6.1f  turn %d\n", (int)bi, lv(PL, gp), lv(B, gb), lv(DL, gd), lv(HL, gh), turnAt(sh[0].plan.asLink(), bi * 4.0 + 0.1));
			}
		}
		writeWav16(outDir + "/" + j.file + ".wav", L, R, SR);
		printf("%s  %.1f s  %s  bpm %.0f  end beat %.0f  trade %.0f-%.0f  horn notes %d\n", j.file.c_str(), N / (double)SR, j.prog, j.bpm,
		       sh[0].plan.endBeat, sh[0].plan.tradeFrom, sh[0].plan.tradeTo, (int)sh[0].noteSerial);
		fflush(stdout);
		delete e;
	}
}
