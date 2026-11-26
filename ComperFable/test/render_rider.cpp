// Rider offline checks (no Rack): the player against test/rider_golden.json, then the host: linked to Comper's host,
// on a clock cable, behaviour, other sample rates and cost.
//   c++ -O3 -funsafe-math-optimizations -std=c++17 test/render_rider.cpp -o test/render_rider && ./test/render_rider [OUTDIR]
#include "../src/dsp/Host.hpp"
#include "../src/dsp/RiderHost.hpp"
#include "golden.h"
#include "rider_golden.h"
#include "rider_presets.h"
#include <cstdio>
#include <cstring>
#include <string>
#include <chrono>
#include <algorithm>
#if defined(__x86_64__) || defined(__i386__)
#include <xmmintrin.h>
#endif

static const RGCase* findCase(const std::string& name) {
	for (int i = 0; i < NUM_RGOLDEN; i++) if (name == RGOLDEN[i].name) return &RGOLDEN[i];
	return nullptr;
}

// ---------------------------------------------------------------- 1. the player
static int checkPlayer() {
	int fails = 0;
	for (int ci = 0; ci < NUM_RGOLDEN; ci++) {
		const RGCase& c = RGOLDEN[ci];
		rider::Rider d(c.seed);
		rider::RP p; p.feel = (int)c.prm[0]; p.energy = c.prm[1]; p.comp = c.prm[2]; p.listen = c.prm[3]; p.fills = c.prm[4];
		p.feather = c.prm[5]; p.repeat = c.prm[6]; p.loose = c.prm[7]; p.ahead = c.prm[8]; p.swing = c.prm[9];
		double spb = 60.0 / c.bpm;
		std::vector<RGNote> got;
		for (int bar = 0; bar < c.nbars; bar++) {
			const RGBar& gb = c.bars[bar];
			rider::BarIn bi; bi.hasPiano = gb.hasPiano != 0; bi.slots = gb.slots;
			for (int s = 0; s < 16; s++) { bi.kind[s] = gb.kind[s]; bi.slotBeat[s] = gb.slotBeat[s]; bi.push[s] = gb.push[s] != 0; }
			bi.nbass = gb.nbass; for (int i = 0; i < gb.nbass; i++) bi.bassOff[i] = gb.bassOff[i];
			bi.forceFill = gb.forceFill;
			if (gb.energy >= 0) p.energy = gb.energy;
			for (auto& s : d.planBar(bar, c.bpm, c.chorusBars, p, bi))
				got.push_back(RGNote{(bar * 4 + s.beat) * spb + s.offs, s.voice, s.vel, s.dur * spb, bar * 4 + s.beat});
		}
		bool ok = (int)got.size() == c.nnotes; int at = -1;
		for (int i = 0; ok && i < c.nnotes; i++) {
			const RGNote &a = got[i], &b = c.notes[i];
			if (a.voice != b.voice || a.vel != b.vel || std::fabs(a.t - b.t) > 1e-6 || std::fabs(a.dur - b.dur) > 1e-6 || std::fabs(a.beat - b.beat) > 1e-6) { ok = false; at = i; }
		}
		if (!ok) {
			fails++;
			printf("FAIL %s: %d strokes, want %d", c.name, (int)got.size(), c.nnotes);
			if (at >= 0 && at < (int)got.size()) printf("; first difference at %d: got (%.6f v%d %d) want (%.6f v%d %d)", at, got[at].t, got[at].voice, got[at].vel, c.notes[at].t, c.notes[at].voice, c.notes[at].vel);
			printf("\n");
		}
	}
	printf("player: %d of %d drum parts match the reference\n", NUM_RGOLDEN - fails, NUM_RGOLDEN);
	return fails;
}

// ---------------------------------------------------------------- helpers
static rider::RIn presetIn(const RPreset& P, int seed) {
	const double* v = P.v; rider::RIn in;
	in.feelPos = (int)v[0];
	in.rp.energy = v[1]; in.rp.comp = v[2]; in.rp.listen = v[3]; in.rp.fills = v[4]; in.rp.feather = v[5]; in.rp.repeat = v[6];
	in.seed = seed; in.rp.loose = v[10]; in.rp.ahead = v[11];
	for (int k = 0; k < 5; k++) in.kit.mix[k] = v[12 + k];
	in.kit.tune = v[17]; in.kit.decay = v[18]; in.kit.bright = v[19]; in.kit.width = v[20]; in.kit.age = v[21]; in.kit.level = v[22];
	return in;
}
static comper::Params toParams(const double* v) {
	comper::Params p; p.style = (int)v[0]; p.spread = v[1]; p.reg = v[2]; p.voices = (int)v[3]; p.lead = v[4]; p.out = v[5]; p.roots = (int)v[6];
	p.groove = (int)v[7]; p.density = v[8]; p.swing = v[9]; p.repeat = v[10]; p.push = v[11]; p.loose = v[12]; p.timing = v[13];
	p.roll = v[14]; p.weight = v[15]; p.slop = v[16]; p.pedal = v[17]; p.listen = v[18]; return p;
}
// Comper's host, set up as tools/render_trio.cpp does, filling the link as Comper's sendLink() does
struct ComperSide {
	comper::HostIn in; comper::HostOut o; comper::Host h;
	bool setup(const char* name) {
		std::string cname = std::string("preset ") + name;
		const GCase* c = nullptr;
		for (int i = 0; i < NUM_GOLDEN; i++) if (cname == GOLDEN[i].name) c = &GOLDEN[i];
		if (!c) return false;
		in.steps = c->nchart; in.beatsPerStep = (int)(c->total / c->nchart);
		for (int k = 0; k < c->nchart; k++) { in.stepRoot[k] = c->chart[k].root; in.stepQual[k] = c->chart[k].q; }
		in.p = toParams(c->prm); in.seed = c->seed; in.tempo = c->bpm;
		return true;
	}
	void step(double dt, CompLink& m) {
		h.process(in, o, dt, -0.5);
		m = CompLink();
		m.hasComp = true; m.resetCount = h.resetCount; m.running = h.running && !h.waitingFirstPulse;
		m.phi = h.phi; m.spb = h.spb; m.steps = in.steps; m.beatsPerStep = in.beatsPerStep;
		for (int k = 0; k < 16; k++) { m.root[k] = comper::pmod(in.stepRoot[k] + h.keyOffset(in), 12); m.qual[k] = in.stepQual[k]; }
		m.external = h.source != comper::SRC_CHART; m.curRoot = h.srcChord.root; m.curQual = h.srcChord.q;
		m.groove = in.p.groove; m.swing = in.p.swing;
		m.planBar = h.planBar; m.planSlots = h.eng.slots;
		for (int s = 0; s < 16; s++) { m.planKind[s] = h.eng.cellmap[s]; m.planBeat[s] = h.eng.slotBeats[s]; m.planPush[s] = h.eng.pushOk[s]; }
	}
};
// the host's log, in plan order, against a golden case (bar, voice, velocity, beat)
static bool compareLog(const char* what, std::vector<rider::RiderHost::Log> lg, const RGCase* c, int nbars, double tol = 1e-6) {
	std::sort(lg.begin(), lg.end(), [](const rider::RiderHost::Log& a, const rider::RiderHost::Log& b) { return a.idx < b.idx; });
	std::vector<rider::RiderHost::Log> got;
	for (auto& l : lg) if (l.bar < nbars) got.push_back(l);
	bool ok = (int)got.size() == c->nnotes; int at = -1;
	int n = std::min((int)got.size(), c->nnotes);
	for (int i = 0; i < n; i++) {
		const auto& a = got[i]; const RGNote& b = c->notes[i];
		if (a.voice != b.voice || a.vel != b.vel || std::fabs(a.bar * 4 + a.beat - b.beat) > tol) { ok = false; at = i; break; }
	}
	if (!ok) {
		printf("FAIL %s: %d strokes, want %d", what, (int)got.size(), c->nnotes);
		if (at >= 0) printf("; first difference at %d: got (bar %d beat %.7f v%d %d) want (%.7f v%d %d)", at, got[at].bar, got[at].beat, got[at].voice, got[at].vel,
		                    c->notes[at].beat, c->notes[at].voice, c->notes[at].vel);
		printf("\n");
	}
	return ok;
}

// ---------------------------------------------------------------- 2. linked to Comper's host
static int checkLinked() {
	int fails = 0;
	const double SR = 48000, dt = 1.0 / SR;
	for (int pi = 0; pi < NUM_RPRESETS; pi++) {
		const RPreset& P = RPRESETS[pi];
		const RGCase* c = findCase(std::string("linked ") + P.name);
		ComperSide cs; if (!c || !cs.setup(P.name)) { printf("FAIL linked %s: no case\n", P.name); fails++; continue; }
		rider::RiderHost rh; rh.setSampleRate(SR); rh.logOn = true;
		rider::RIn in = presetIn(P, 11);
		for (long long s = 0; s < (long long)(20 * 4 * 4 * SR); s++) {
			cs.step(dt, in.link);
			rider::ROut o; rh.process(in, o, dt);
			if (rh.phi > c->nbars * 4 + 0.6) break;
		}
		if (!compareLog((std::string("linked ") + P.name).c_str(), rh.log, c, c->nbars)) fails++;
	}
	printf("linked: %d of %d presets play the reference strokes next to Comper's host\n", NUM_RPRESETS - fails, NUM_RPRESETS);
	return fails;
}

// ---------------------------------------------------------------- a clocked take
struct Take {
	std::vector<rider::RiderHost::Log> log;
	std::vector<std::pair<long long, int>> rises;   // sample, trigger group
	double energy = 0; long long n = 0;
	std::vector<float> L, R;
	bool anyTrig = false;
};
struct TakeOpts {
	double sr = 48000; int feelPos = 1; int seed = 1; int bars = 12; double bpm = 120;
	double fills = 0.5; double resetAtPhi = -1; double seedAtPhi = -1; int newSeed = 2; double fillAtPhi = -1; bool layout = false;
	bool keepAudio = false;
};
static Take clocked(const TakeOpts& o) {
	Take tk;
	rider::RiderHost rh; rh.setSampleRate(o.sr); rh.logOn = true;
	rider::RIn in; in.clockConnected = true; in.ppb = 1; in.feelPos = o.feelPos; in.seed = o.seed; in.rp.fills = o.fills; in.chorusBars = 12;
	if (o.layout) in.layout = 10.f;
	double dt = 1.0 / o.sr; double per = 60.0 / o.bpm;
	long long perS = (long long)std::llround(per * o.sr);
	long long N = (long long)((o.bars * 4 + 0.6) * per * o.sr);
	bool prevT[rider::NUM_TG] = {};
	bool didReset = false, didSeed = false, didFill = false; long long resetPulseAt = -1;
	long long pulseShift = 0;
	for (long long s = 0; s < N + (o.resetAtPhi >= 0 ? (long long)(8 * per * o.sr) : 0); s++) {
		long long k = s - pulseShift;
		in.clock = (k >= 0 && k % perS < (long long)(0.002 * o.sr)) ? 10.f : 0.f;
		in.reset = 0.f;
		if (o.resetAtPhi >= 0 && !didReset && rh.phi >= o.resetAtPhi && k % perS > perS / 2) {
			// a reset pulse half a beat before the next clock pulse
			resetPulseAt = s; didReset = true;
		}
		if (resetPulseAt >= 0 && s - resetPulseAt < (long long)(0.002 * o.sr)) in.reset = 10.f;
		if (o.seedAtPhi >= 0 && !didSeed && rh.phi >= o.seedAtPhi) { in.seed = o.newSeed; didSeed = true; }
		in.fill = 0.f;
		if (o.fillAtPhi >= 0 && rh.phi >= o.fillAtPhi && rh.phi < o.fillAtPhi + 0.01) { in.fill = 10.f; didFill = true; }
		rider::ROut out; rh.process(in, out, dt);
		for (int g = 0; g < rider::NUM_TG; g++) {
			bool hi = out.trig[g] > 5.f;
			if (hi) tk.anyTrig = true;
			if (hi && !prevT[g]) tk.rises.push_back({rh.sampleNow - 1, g});
			prevT[g] = hi;
		}
		tk.energy += (double)out.L * out.L + (double)out.R * out.R; tk.n++;
		if (o.keepAudio) { tk.L.push_back(out.L); tk.R.push_back(out.R); }
	}
	(void)didFill;
	tk.log = rh.log;
	return tk;
}

// ---------------------------------------------------------------- 3. clock
static int checkClock() {
	int fails = 0;
	for (int f = 0; f < 5; f++) {
		std::string name = std::string("alone ") + rider::FEEL_NAMES[f] + " / 120 bpm / seed 1";
		const RGCase* c = findCase(name);
		if (!c) { printf("FAIL no case %s\n", name.c_str()); fails++; continue; }
		TakeOpts o; o.feelPos = f + 1; o.bars = c->nbars;
		Take tk = clocked(o);
		if (!compareLog(("clock " + name).c_str(), tk.log, c, c->nbars)) fails++;
	}
	printf("clock: %d of 5 feels play the reference strokes from a 120 bpm clock\n", 5 - fails);
	return fails;
}

// ---------------------------------------------------------------- 4. behaviour
static std::vector<rider::RiderHost::Log> barsOf(const std::vector<rider::RiderHost::Log>& lg, int a, int b) {
	std::vector<rider::RiderHost::Log> o;
	for (auto& l : lg) if (l.bar >= a && l.bar < b) o.push_back(l);
	std::sort(o.begin(), o.end(), [](const rider::RiderHost::Log& x, const rider::RiderHost::Log& y) { return x.idx < y.idx; });
	return o;
}
static bool sameStrokes(const std::vector<rider::RiderHost::Log>& a, const std::vector<rider::RiderHost::Log>& b) {
	if (a.size() != b.size()) return false;
	for (size_t i = 0; i < a.size(); i++)
		if (a[i].bar != b[i].bar || a[i].voice != b[i].voice || a[i].vel != b[i].vel || std::fabs(a[i].beat - b[i].beat) > 1e-9) return false;
	return true;
}
static int checkBehaviour() {
	int fails = 0;
	auto check = [&](bool ok, const char* what) { printf("  %s %s\n", ok ? "ok  " : "FAIL", what); if (!ok) fails++; };
	TakeOpts base; base.bars = 12;
	Take ref = clocked(base);
	// a reset mid-take replays the same strokes
	{
		TakeOpts o = base; o.resetAtPhi = 22.0;
		Take tk = clocked(o);
		// strokes after the reset carry a second run of bar numbers: split the log where the bar number goes back
		std::vector<rider::RiderHost::Log> after; int maxBar = -1; bool split = false;
		for (auto& l : tk.log) { if (!split && l.bar < maxBar - 1) split = true; if (split) after.push_back(l); maxBar = std::max(maxBar, l.bar); }
		int lo = 1 << 30; for (auto& l : after) lo = std::min(lo, l.idx);
		for (auto& l : after) l.idx -= lo;
		check(split && sameStrokes(barsOf(after, 0, 6), barsOf(ref.log, 0, 6)), "a reset mid-take replays the same strokes");
	}
	// a new SEED takes effect only at a bar line
	{
		TakeOpts o = base; o.seedAtPhi = 13.5;
		Take tk = clocked(o);
		bool before = sameStrokes(barsOf(tk.log, 0, 4), barsOf(ref.log, 0, 4));
		bool after = !sameStrokes(barsOf(tk.log, 4, 12), barsOf(ref.log, 4, 12));
		check(before && after, "a new SEED takes effect at the next bar line (bars 0-3 as before, 4 on new)");
	}
	// a FILL trigger with FILLS at 0 puts snare or tom strokes in beats 3 and 4 of the next bar
	{
		TakeOpts o = base; o.fills = 0.0; o.fillAtPhi = 9.5;
		Take tk = clocked(o);
		TakeOpts q = base; q.fills = 0.0; Take none = clocked(q);
		auto count = [](const std::vector<rider::RiderHost::Log>& v) { int n = 0; for (auto& l : v) if (l.beat >= 2.0 && (l.voice == rider::SNARE || l.voice == rider::TOM_HI || l.voice == rider::TOM_LO)) n++; return n; };
		int nf = count(barsOf(tk.log, 3, 4)), n0 = count(barsOf(none.log, 3, 4));
		bool anyOther = false; for (int b = 0; b < 12; b++) if (b != 3) anyOther = anyOther || !sameStrokes(barsOf(tk.log, b, b + 1), barsOf(none.log, b, b + 1)) ;
		(void)anyOther;
		check(nf > 0 && nf > n0 && sameStrokes(barsOf(tk.log, 0, 3), barsOf(none.log, 0, 3)), "a FILL trigger with FILLS at 0 sets up the next bar (snare or toms in beats 3-4)");
	}
	// LAY OUT
	{
		TakeOpts o = base; o.layout = true;
		Take tk = clocked(o);
		check(tk.log.empty() && !tk.anyTrig && tk.energy == 0.0, "LAY OUT high: no strokes, no triggers, no sound");
	}
	// the six triggers fire for the right voices
	{
		bool ok = !ref.rises.empty();
		for (auto& r : ref.rises) {
			bool found = false;
			for (auto& l : ref.log) if (l.at == r.first && rider::trigGroup(l.voice) == r.second) found = true;
			ok = ok && found;
		}
		int need = 0; for (auto& l : ref.log) if (rider::trigGroup(l.voice) >= 0) need++;
		check(ok && (int)ref.rises.size() <= need && (int)ref.rises.size() > need * 0.8, "the trigger outputs fire for the right voices");
		// every feel exercises its groups; bossa (cross-stick, hats, kick) and house (open hat, snare)
		int groupsSeen[rider::NUM_TG] = {};
		for (int f = 1; f <= 5; f++) { TakeOpts o = base; o.feelPos = f; o.fills = 1.0; Take t = clocked(o); for (auto& r : t.rises) groupsSeen[r.second]++; }
		bool all = true; for (int g = 0; g < rider::NUM_TG; g++) all = all && groupsSeen[g] > 0;
		check(all, "all six trigger outputs fire across the five feels");
	}
	printf("behaviour: %s\n", fails ? "FAILED" : "all pass");
	return fails;
}

// ---------------------------------------------------------------- 5. other rates
static int checkRates() {
	int fails = 0;
	TakeOpts o; o.bars = 8; o.feelPos = 1;
	Take r48 = clocked(o);
	for (double sr : {44100.0, 96000.0}) {
		TakeOpts q = o; q.sr = sr; Take t = clocked(q);
		bool same = sameStrokes(barsOf(t.log, 0, 8), barsOf(r48.log, 0, 8));
		double e48 = r48.energy / r48.n, e = t.energy / t.n;
		double db = 10 * std::log10(e / e48);
		bool ok = same && std::fabs(db) <= 1.0;
		printf("  %s %.1f kHz: %s strokes, energy %+.2f dB against 48 kHz\n", ok ? "ok  " : "FAIL", sr / 1000, same ? "same" : "DIFFERENT", db);
		if (!ok) fails++;
	}
	printf("rates: %s\n", fails ? "FAILED" : "all pass");
	return fails;
}

// ---------------------------------------------------------------- 6. cost
static int checkCost(const std::string& outDir) {
	const double SR = 48000, dt = 1.0 / SR;
	const RPreset& P = RPRESETS[1];
	long long N = (long long)(30 * SR);
	auto run = [&](bool withRider, std::vector<float>* L, std::vector<float>* R) {
		ComperSide cs; cs.setup(P.name);
		rider::RiderHost rh; rh.setSampleRate(SR);
		rider::RIn in = presetIn(P, 11);
		CompLink m; double sink = 0;
		auto t0 = std::chrono::steady_clock::now();
		for (long long s = 0; s < N; s++) {
			cs.step(dt, withRider ? in.link : m);
			if (withRider) { rider::ROut o; rh.process(in, o, dt); sink += o.L; if (L) { L->push_back(o.L); R->push_back(o.R); } }
		}
		auto t1 = std::chrono::steady_clock::now();
		if (sink == 12345.678) printf(" ");
		return std::chrono::duration<double>(t1 - t0).count();
	};
	double best = 1e9;
	for (int k = 0; k < 3; k++) { double a = run(false, nullptr, nullptr), b = run(true, nullptr, nullptr); best = std::min(best, b - a); }
	double frac = best / 30.0;
	printf("cost: Rider takes %.1f%% of one core at 48 kHz on preset 02 (target 6%%, fail above 10%%)\n", frac * 100);
	if (!outDir.empty()) {
		std::vector<float> L, R; run(true, &L, &R);
		std::string path = outDir + "/rider-preset02-linked.wav";
		FILE* f = fopen(path.c_str(), "wb");
		if (f) {
			int n = (int)L.size(), bytes = n * 8;
			auto w32 = [&](uint32_t v) { fwrite(&v, 4, 1, f); }; auto w16 = [&](uint16_t v) { fwrite(&v, 2, 1, f); };
			fwrite("RIFF", 1, 4, f); w32(36 + bytes); fwrite("WAVEfmt ", 1, 8, f); w32(16); w16(3); w16(2); w32(48000); w32(48000 * 8); w16(8); w16(32);
			fwrite("data", 1, 4, f); w32(bytes);
			for (int i = 0; i < n; i++) { float l = L[i] * 0.1f, r = R[i] * 0.1f; fwrite(&l, 4, 1, f); fwrite(&r, 4, 1, f); }
			fclose(f);
		}
	}
	return frac > 0.10 ? 1 : 0;
}

int main(int argc, char** argv) {
#if defined(__x86_64__) || defined(__i386__)
	_mm_setcsr(_mm_getcsr() | 0x8040);
#endif
	std::string out = argc > 1 ? argv[1] : "";
	const char* only = argc > 2 ? argv[2] : nullptr;
	int f = 0;
	auto want = [&](const char* k) { return !only || strstr(k, only); };
	if (want("player")) f += checkPlayer();
	if (want("linked")) f += checkLinked();
	if (want("clock")) f += checkClock();
	if (want("behaviour")) f += checkBehaviour();
	if (want("rates")) f += checkRates();
	if (want("cost")) f += checkCost(out);
	printf(f ? "render_rider: FAILED\n" : "render_rider: all pass\n");
	return f ? 1 : 0;
}
