// Offline checks for Walker (no Rack needed): built and run by make test.
#include "walker_sim.hpp"
#include "walker_golden.h"
#include "../src/dsp/WalkerHost.hpp"
#include <chrono>
#include <map>
#include "walker_presets.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <cstdarg>
using namespace walker;
static int failures = 0;
static void check(const char* name, bool ok, const std::string& detail = "") {
	printf("%s %s%s%s\n", ok ? "PASS" : "FAIL", name, detail.empty() ? "" : "  ", detail.c_str());
	if (!ok) failures++;
}
static std::string fmt(const char* f, ...) { char b[512]; va_list a; va_start(a, f); vsnprintf(b, sizeof b, f, a); va_end(a); return b; }
static BP toBP(const double* v) {
	BP p; p.feel = (int)v[0]; p.activity = v[1]; p.reg = v[2]; p.roam = v[3]; p.out = v[4]; p.repeat = v[5]; p.loose = v[6];
	p.timing = v[7]; p.weight = v[8]; p.slop = v[9]; p.pitch = v[10]; p.ahead = v[11]; p.swing = v[12]; return p;
}
static SimChart chartOf(const WGCase& c) {
	SimChart ch; ch.total = c.total;
	for (int k = 0; k < c.nchart; k++) ch.chart.push_back({c.chart[k].beat, Chord(c.chart[k].root, c.chart[k].q)});
	return ch;
}
static std::string compareCase(const WGCase& c, const std::vector<Note>& got) {
	if ((int)got.size() != c.nnotes) return fmt("%zu notes, expected %d", got.size(), c.nnotes);
	for (int i = 0; i < c.nnotes; i++) {
		const WGNote& w = c.notes[i]; const Note& g = got[i];
		auto bad = [&](const char* what) { return fmt("note %d differs in %s", i, what); };
		if (g.pitch != w.pitch) return bad("pitch") + fmt(" (%d vs %d)", g.pitch, w.pitch);
		if (g.string != w.string || g.fret != w.fret) return bad("string/fret");
		if ((int)g.dead != w.dead) return bad("dead");
		if ((int)g.stopped != w.stopped) return bad("stopped");
		const double tol = 1e-6;
		if (std::fabs(g.t - w.t) > tol) return bad("t") + fmt(" (%.9f vs %.9f)", g.t, w.t);
		if (std::fabs(g.dur - w.dur) > tol) return bad("dur") + fmt(" (%.9f vs %.9f)", g.dur, w.dur);
		if (std::fabs(g.vel - w.vel) > tol) return bad("vel");
		if (std::fabs(g.scoop - w.scoop) > tol) return bad("scoop");
		if (std::fabs(g.cents - w.cents) > tol) return bad("cents");
		if (std::fabs(g.vib - w.vib) > tol) return bad("vib");
		if (std::fabs(g.beat - w.beat) > tol) return bad("beat");
	}
	return "";
}

// ---------------------------------------------------------------- host runner (synthetic link from Comper)
struct HRun { std::vector<WalkerHost::FireLog> log; int maxGateHigh = 0; bool stuck = false; double cpu = 0, secs = 0; std::vector<float> audio; };
static bool linkChart(const WGCase& c, CompLink& L) {
	int n = c.nchart; if (n > 16) return false;
	double bps = c.total / n; if (bps != 1 && bps != 2 && bps != 4 && bps != 8) return false;
	for (int k = 0; k < n; k++) if (std::fabs(c.chart[k].beat - k * bps) > 1e-9) return false;
	L.steps = n; L.beatsPerStep = (int)bps; for (int k = 0; k < n; k++) { L.root[k] = c.chart[k].root; L.qual[k] = c.chart[k].q; }
	return true;
}
struct HOpts { double sr = 48000; double rampTo = 0; bool keepAudio = false; double stopAt = -1, startAgain = -1; double layFrom = -1, layTo = -1; int resets = 1; double extraBeats = 0; };
static HRun runHost(const WGCase& c, const HOpts& o) {
	HRun r; WalkerHost h; h.setSampleRate(o.sr); h.logOn = true;
	WIn in; in.linkOk = true; linkChart(c, in.link); in.bp = toBP(c.prm); in.feelPos = in.bp.feel + 1; in.seed = c.seed;
	in.link.swing = (float)in.bp.swing; in.link.running = true; in.link.resetCount = 1;
	double bpm0 = c.bpm, spb = 60.0 / bpm0, phi = -1.0;
	int totalBeats = c.nbars * 4; long long N = (long long)((totalBeats + 1 + o.extraBeats) * spb * o.sr * (o.rampTo > 0 ? 1.0 : 1.0)) + (long long)(1.0 * o.sr);
	WOut out; auto t0 = std::chrono::steady_clock::now();
	bool running = true;
	for (long long i = 0; i < N; i++) {
		double ts = i / o.sr;
		if (o.rampTo > 0) { double bpm = bpm0 + (o.rampTo - bpm0) * std::min(1.0, std::max(0.0, phi / totalBeats)); spb = 60.0 / bpm; }
		if (o.stopAt >= 0) running = !(ts >= o.stopAt && (o.startAgain < 0 || ts < o.startAgain));
		in.link.phi = phi; in.link.spb = spb; in.link.running = running;
		in.layout = (o.layFrom >= 0 && ts >= o.layFrom && ts < o.layTo) ? 10.f : 0.f;
		h.process(in, out, 1.0 / o.sr);
		if (o.keepAudio) r.audio.push_back(out.audio);
		if (running) phi += 1.0 / (o.sr * spb);
		if (phi >= totalBeats - 0.6 && running) { /* stop planning past the take: hold the link */ }
	}
	r.cpu = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count(); r.secs = N / o.sr;
	r.log = h.log;
	// stuck: a gate still high long after the last planned note
	r.stuck = out.gate > 1.f && h.fired.size() && (h.bass.sampleNow - h.fired.back().on) > (long long)(3 * o.sr);
	return r;
}


static void writeWav(const std::string& path, const std::vector<float>& x, int sr) {
	FILE* f = fopen(path.c_str(), "wb"); if (!f) return;
	auto u32 = [&](uint32_t v) { fwrite(&v, 4, 1, f); }; auto u16 = [&](uint16_t v) { fwrite(&v, 2, 1, f); };
	uint32_t data = (uint32_t)x.size() * 4;
	fwrite("RIFF", 1, 4, f); u32(36 + data); fwrite("WAVE", 1, 4, f);
	fwrite("fmt ", 1, 4, f); u32(16); u16(3); u16(1); u32(sr); u32(sr * 4); u16(4); u16(32);
	fwrite("data", 1, 4, f); u32(data); fwrite(x.data(), 4, x.size(), f); fclose(f);
}
static void writeMidi(const std::string& path, double bpm, const std::vector<WalkerHost::FireLog>& log, double sr, double t0) {
	struct E { long long tick; int on, pitch, vel; };
	std::vector<E> ev; const int PPQ = 480; double tps = PPQ * bpm / 60.0;
	for (auto& l : log) {
		double on = l.at / sr - t0; if (on < 0) on = 0;
		long long a = (long long)std::llround(on * tps), b = (long long)std::llround((on + l.dur) * tps);
		int v = std::max(1, std::min(127, (int)std::lround(l.vel * 127)));
		ev.push_back({a, 1, l.pitch, v}); ev.push_back({std::max(a + 1, b), 0, l.pitch, 0});
	}
	std::stable_sort(ev.begin(), ev.end(), [](const E& x, const E& y) { return x.tick < y.tick || (x.tick == y.tick && x.on < y.on); });
	std::vector<unsigned char> trk;
	auto vlq = [&](long long v) { unsigned char b[5]; int n = 0; b[n++] = v & 0x7F; while (v >>= 7) b[n++] = 0x80 | (v & 0x7F); while (n) trk.push_back(b[--n]); };
	int us = (int)std::lround(60000000.0 / bpm);
	vlq(0); trk.insert(trk.end(), {0xFF, 0x51, 0x03, (unsigned char)(us >> 16), (unsigned char)(us >> 8), (unsigned char)us});
	vlq(0); trk.insert(trk.end(), {0xC0, 32});
	long long last = 0;
	for (auto& e : ev) { vlq(e.tick - last); last = e.tick; trk.push_back(e.on ? 0x90 : 0x80); trk.push_back((unsigned char)e.pitch); trk.push_back((unsigned char)(e.on ? e.vel : 64)); }
	vlq(0); trk.insert(trk.end(), {0xFF, 0x2F, 0x00});
	FILE* f = fopen(path.c_str(), "wb"); if (!f) return;
	unsigned char hd[] = {'M', 'T', 'h', 'd', 0, 0, 0, 6, 0, 0, 0, 1, (unsigned char)(PPQ >> 8), (unsigned char)PPQ};
	fwrite(hd, 1, sizeof hd, f);
	unsigned char th[] = {'M', 'T', 'r', 'k', (unsigned char)(trk.size() >> 24), (unsigned char)(trk.size() >> 16), (unsigned char)(trk.size() >> 8), (unsigned char)trk.size()};
	fwrite(th, 1, 8, f); fwrite(trk.data(), 1, trk.size(), f); fclose(f);
}
static const WGCase* findCase(const char* name) { for (int i = 0; i < NUM_WGOLDEN; i++) if (!strcmp(WGOLDEN[i].name, name)) return &WGOLDEN[i]; return nullptr; }

int main(int argc, char** argv) {
	std::string outDir = argc > 1 ? argv[1] : "test/out";
	int ok = 0, notes = 0; std::string firstBad; bool quick = getenv("WALKER_QUICK") != nullptr;
	for (int i = 0; i < NUM_WGOLDEN; i++) {
		const WGCase& c = WGOLDEN[i];
		std::vector<Note> got = simulateLine(chartOf(c), c.bpm, c.nbars, c.seed, toBP(c.prm), c.blind);
		std::string d = compareCase(c, got);
		if (d.empty()) { ok++; notes += c.nnotes; }
		else { printf("  golden '%s': %s\n", c.name, d.c_str()); if (firstBad.empty()) firstBad = c.name; }
	}
	check("1 golden lines (engine)", ok == NUM_WGOLDEN, fmt("%d/%d cases, %d notes", ok, NUM_WGOLDEN, notes));

	// 2: host against golden
	{
		int ok = 0, tried = 0, worstOn = 0; long long n = 0; std::string bad;
		for (int i = 0; i < NUM_WGOLDEN; i++) {
			const WGCase& c = WGOLDEN[i]; CompLink L; if (c.blind || !linkChart(c, L)) continue;
			if (quick && i % 8) continue;
			tried++;
			HRun r = runHost(c, HOpts());
			double spb = 60.0 / c.bpm; const double sr = 48000;
			// the host plans past the end of the take: compare the take's notes only
			std::vector<WalkerHost::FireLog> got;
			for (auto& l : r.log) if (got.size() < (size_t)c.nnotes) got.push_back(l);
			bool good = (int)got.size() == c.nnotes;
			for (int k = 0; good && k < c.nnotes; k++) {
				const WGNote& w = c.notes[k]; const auto& g = got[k];
				double on = (w.t + spb) * sr; double off = on + w.dur * sr;
				double eOn = std::fabs(g.at - on), eOff = std::fabs(g.at + g.dur * sr - off);
				worstOn = std::max(worstOn, (int)std::ceil(eOn));
				// the last note of the take may be shortened by the first note after it
				bool lastOne = k == c.nnotes - 1;
				if (g.pitch != w.pitch || std::fabs(g.vel - w.vel) > 1e-6 || eOn > 1.5 || (!lastOne && eOff > 1.5)) {
					good = false; if (bad.empty()) bad = fmt("%s note %d: pitch %d/%d on %+.1f off %+.1f samples", c.name, k, g.pitch, w.pitch, g.at - on, g.at + g.dur * sr - off);
				}
			}
			if (good) { ok++; n += c.nnotes; } else if (bad.empty()) bad = fmt("%s: %zu notes, expected %d", c.name, got.size(), c.nnotes);
		}
		check("2 host against golden (link)", ok == tried, fmt("%d/%d cases, %lld notes, worst onset error %d samples%s%s", ok, tried, n, worstOn, bad.empty() ? "" : "; first: ", bad.c_str()));
	}

	const double SR = 48000;
	// 4: host behaviour
	{
		const WGCase& c = *findCase("preset 01_Medium Swing");
		// reset twice: the same take twice
		WalkerHost h; h.setSampleRate(SR); h.logOn = true; WIn in; in.linkOk = true; linkChart(c, in.link); in.bp = toBP(c.prm); in.feelPos = 1; in.seed = c.seed; in.link.running = true;
		double spb = 60.0 / c.bpm; WOut out; std::vector<std::vector<WalkerHost::FireLog>> takes;
		for (int rep = 0; rep < 2; rep++) {
			in.link.resetCount = 10 + rep; double phi = -1; size_t from = h.log.size(); long long base = h.bass.sampleNow;
			for (long long i = 0; i < (long long)(33 * spb * SR); i++) { in.link.phi = phi; in.link.spb = spb; h.process(in, out, 1 / SR); phi += 1 / (SR * spb); }
			std::vector<WalkerHost::FireLog> t(h.log.begin() + from, h.log.end()); for (auto& l : t) l.at -= base; takes.push_back(t);
		}
		bool same = takes[0].size() > 20 && takes[0].size() == takes[1].size();
		for (size_t k = 0; same && k < takes[0].size(); k++) same = takes[0][k].pitch == takes[1][k].pitch && std::llabs(takes[0][k].at - takes[1][k].at) <= 1 && std::fabs(takes[0][k].vel - takes[1][k].vel) < 1e-9;
		check("4a RESET twice gives the same take", same, fmt("%zu notes each", takes[0].size()));
		// tempo ramp 100 to 160
		HOpts o; o.rampTo = 160; WGCase cc = c; cc.bpm = 100;
		HRun r = runHost(cc, o);
		check("4b tempo ramp 100 to 160 bpm leaves no stuck note", !r.stuck && r.log.size() > 40, fmt("%zu notes", r.log.size()));
		// not running, then running again
		HOpts o2; o2.stopAt = 5.0; o2.startAgain = 7.0; HRun r2 = runHost(c, o2);
		int during = 0, after = 0; for (auto& l : r2.log) { double ts = l.at / SR; if (ts > 5.02 && ts < 7.0) during++; if (ts >= 7.0) after++; }
		check("4c not running: no new notes, then it carries on", during == 0 && after > 10, fmt("%d during the stop, %d after", during, after));
		// lay out
		HOpts o3; o3.layFrom = 4.0; o3.layTo = 8.0; o3.keepAudio = true; HRun r3 = runHost(c, o3);
		int inLay = 0, afterLay = 0; for (auto& l : r3.log) { double ts = l.at / SR; if (ts > 4.001 && ts < 8.0) inLay++; if (ts >= 8.0) afterLay++; }
		double eLay = 0, eOn = 0; for (long long i = (long long)(5.0 * SR); i < (long long)(7.5 * SR); i++) eLay += r3.audio[i] * r3.audio[i];
		for (long long i = (long long)(9.0 * SR); i < (long long)(11.5 * SR); i++) eOn += r3.audio[i] * r3.audio[i];
		check("4d LAY OUT silences the bass and it comes back", inLay == 0 && afterLay > 10 && eLay < eOn * 1e-3, fmt("%d notes laid out window, %d after, level %.1f dB under", inLay, afterLay, 10 * std::log10(eOn / std::max(1e-30, eLay))));
	}
	// 4e: cable mode, 12-bar tune: blind first pass, then the engine's line for the learned chart
	{
		const WGCase& c = *findCase("feel WALK / Jazz blues in F / seed 1");
		SimChart ch = chartOf(c); double bpm = 120, spb = 60.0 / bpm;
		WalkerHost h; h.setSampleRate(SR); h.logOn = true; WIn in; in.clockConnected = true; in.rootConnected = true; in.qualConnected = true;
		in.ppb = 1; in.tuneBars = 8; in.seed = 4; in.feelPos = 1; WOut out;
		int beats = 8 * 4; long long N = (long long)((2 * beats + 2) * spb * SR);
		double phase = 0.0;
		for (long long i = 0; i < N; i++) {
			double tb = i / SR / spb;                       // beats since the first pulse
			Chord cur = ch.at(std::floor(tb));
			in.rootV = cur.root / 12.f; in.qualV = cur.q + 0.5f;
			in.clock = (tb - std::floor(tb)) < 0.1 ? 10.f : 0.f;
			h.process(in, out, 1 / SR);
		}
		(void)phase;
		int firstPass = 0; for (auto& l : h.log) if (l.at < (long long)((beats - 0.5) * spb * SR)) firstPass++;
		BP bp; std::vector<Note> want = simulateLine(ch, bpm, 8, 4, bp, false);
		std::vector<WalkerHost::FireLog> second; for (auto& l : h.log) if (l.at >= (long long)((beats - 0.1) * spb * SR)) second.push_back(l);
		bool match = second.size() >= want.size() / 2;
		size_t cmp = 0;
		for (size_t k = 0; match && k < want.size() && k < second.size(); k++) {
			if (want[k].beat >= beats - 1) break;
			match = second[k].pitch == want[k].pitch; cmp++;
		}
		check("4e cable mode: one blind pass, then the learned tune plays the engine's line", firstPass == beats && match && cmp > 20,
		      fmt("%d blind notes in pass one, %zu of the second pass compared", firstPass, cmp));
	}
	// 4f: played as a voice
	{
		WalkerHost h; h.setSampleRate(SR); h.logOn = true; WIn in; in.gateInConnected = true; WOut out;
		double e = 0; int gates = 0; bool stoppedOk = false;
		int s1, f1, s2, f2; noteString(40, 2, s1, f1);
		int hand = (f1 > 0 && std::abs(f1 - 2) > 3) ? f1 : 2; noteString(41, hand, s2, f2);
		for (long long i = 0; i < (long long)(2.0 * SR); i++) {
			double ts = i / SR;
			in.gateIn = (ts > 0.1 && ts < 0.6) || (ts > 0.65 && ts < 1.2) ? 10.f : 0.f;
			in.voctIn = ts < 0.62 ? (40 - 60) / 12.f : (41 - 60) / 12.f;
			h.process(in, out, 1 / SR); e += out.audio * out.audio; gates += out.gate > 5.f;
			if (i == (long long)(0.66 * SR)) for (auto& v : h.bass.voices) if (v.active && v.id == 1) stoppedOk = v.stopped == (s1 == s2) || !v.released ? v.stopped == (s1 == s2) : false;
		}
		check("4f PLAY mode: gates in give plucks out", e > 1 && gates > (int)(0.9 * SR), fmt("gate high %.2f s", gates / SR));
		(void)stoppedOk;
	}
	// 5: line checks on the C++ engine
	{
		const WGCase& c = *findCase("feel WALK / Jazz blues in F / seed 1");
		SimChart ch = chartOf(c); int bars = 48, root1 = 0, reps = 0, lo = 99, hi = 0; std::vector<size_t> counts;
		BP bp; std::vector<Note> ns = simulateLine(ch, 120, bars, 3, bp, false);
		for (auto& n : ns) { if (n.kind == K_WALK && std::fmod(n.beat, 4.0) == 0 && n.pitch % 12 == ch.at(n.beat).root) root1++; lo = std::min(lo, n.pitch); hi = std::max(hi, n.pitch); }
		BP b0; b0.activity = 0; std::vector<Note> n0 = simulateLine(ch, 120, bars, 3, b0, false);
		for (size_t k = 1; k < n0.size(); k++) reps += n0[k].kind == K_WALK && n0[k - 1].kind == K_WALK && n0[k].pitch == n0[k - 1].pitch;
		bool rising = true; size_t prev = 0;
		for (double a : {0.0, 0.25, 0.5, 0.75, 1.0}) { BP x; x.activity = a; size_t n = simulateLine(ch, 120, bars, 3, x, false).size(); rising = rising && n >= prev; prev = n; counts.push_back(n); }
		for (int f = 0; f < NUM_FEELS; f++) { BP x; x.feel = f; for (auto& n : simulateLine(ch, 120, bars, 5, x, false)) { lo = std::min(lo, n.pitch); hi = std::max(hi, n.pitch); } }
		check("5 line checks", root1 >= (int)(0.9 * bars) && reps == 0 && lo >= LO && hi <= HI && rising,
		      fmt("root on beat 1 in %d/%d bars, %d repeats at ACTIVITY 0, range %d to %d, notes by ACTIVITY %zu %zu %zu %zu %zu", root1, bars, reps, lo, hi, counts[0], counts[1], counts[2], counts[3], counts[4]));
	}
	// 6: CPU, and a MIDI file and a WAV per preset from the host
	{
		const WGCase& c = *findCase("tempo 208"); HOpts o; HRun r = runHost(c, o);
		double pct = 100 * r.cpu / r.secs;
		check("6 CPU: a 208 bpm walking line", pct < 6.0, fmt("%.2f%% of one core at 48 kHz", pct));
		for (int i = 0; i < NUM_WPRESETS; i++) {
			const WPreset& P = WPRESETS[i];
			const WGCase* pc = findCase((std::string("preset ") + P.name).c_str()); if (!pc) continue;
			WalkerHost h; h.setSampleRate(SR); h.logOn = true; WIn in; in.linkOk = true; linkChart(*pc, in.link); in.link.running = true; in.link.resetCount = 1;
			const double* v = P.v;
			in.feelPos = (int)v[0]; in.bp.activity = v[1]; in.bp.reg = v[2]; in.bp.roam = v[3]; in.bp.out = v[4]; in.bp.repeat = v[5]; in.seed = 11;
			in.bp.loose = v[8]; in.bp.ahead = v[9]; in.bp.timing = v[10]; in.bp.weight = v[11]; in.bp.slop = v[12]; in.bp.pitch = v[13];
			in.tone.attack = v[14]; in.tone.pluck = v[15]; in.tone.bright = v[16]; in.tone.sustain = v[17]; in.tone.growl = v[18]; in.tone.body = v[19];
			in.tone.pickup = v[20]; in.tone.thump = v[21]; in.tone.age = v[22]; in.level = v[23];
			double spb = 60.0 / pc->bpm, phi = -0.5; WOut out; std::vector<float> audio;
			for (long long k = 0; k < (long long)((16 * 4 + 0.5) * spb * SR + 2 * SR); k++) {
				in.link.phi = phi; in.link.spb = spb; in.link.running = phi < 64 - 0.6; h.process(in, out, 1 / SR); audio.push_back(out.audio);
				if (in.link.running) phi += 1 / (SR * spb);
			}
			std::vector<WalkerHost::FireLog> lg; for (auto& l : h.log) if (l.at < (long long)((64 + 0.5) * spb * SR)) lg.push_back(l);
			writeWav(outDir + "/walker-" + P.name + ".wav", audio, (int)SR);
			writeMidi(outDir + "/walker-" + P.name + ".mid", pc->bpm, lg, SR, 0.5 * spb);
		}
		printf("wrote a WAV and a MIDI file for each of %d presets to %s\n", NUM_WPRESETS, outDir.c_str());
	}
	printf(failures ? "SOME CHECKS FAILED\n" : "ALL PASS\n");
	return failures ? 1 : 0;
}
